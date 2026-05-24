#include "AngelscriptTestEngineHelper.h"
#include "AngelscriptTestMacros.h"

#include "CQTest.h"
#include "ClassGenerator/ASClass.h"
#include "Misc/ScopeExit.h"
#include "UObject/GarbageCollection.h"

#if WITH_DEV_AUTOMATION_TESTS


namespace ASClassMetadataTests
{
	struct FDeveloperOnlyModuleCase
	{
		FName ModuleName;
		FString Filename;
		FName GeneratedClassName;
		bool bExpectedDeveloperOnly = false;
	};

	UASClass* CompileMetadataCase(
		FAutomationTestBase& Test,
		FAngelscriptEngine& Engine,
		const FDeveloperOnlyModuleCase& TestCase)
	{
		const FString ScriptSource = FString::Printf(
			TEXT(R"AS(
UCLASS()
class %s : AActor
{
}
)AS"),
			*TestCase.GeneratedClassName.ToString());

		if (!Test.TestTrue(
			*FString::Printf(TEXT("ASClass metadata case '%s' should compile"), *TestCase.ModuleName.ToString()),
			CompileAnnotatedModuleFromMemory(&Engine, TestCase.ModuleName, TestCase.Filename, ScriptSource)))
		{
			return nullptr;
		}

		UASClass* GeneratedClass = Cast<UASClass>(FindGeneratedClass(&Engine, TestCase.GeneratedClassName));
		Test.TestNotNull(
			*FString::Printf(TEXT("ASClass metadata case '%s' should generate '%s'"), *TestCase.ModuleName.ToString(), *TestCase.GeneratedClassName.ToString()),
			GeneratedClass);
		return GeneratedClass;
	}
}

TEST_CLASS_WITH_FLAGS(FAngelscriptASClassMetadataTests,
	"Angelscript.TestModule.ClassGenerator.ASClass",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(IsDeveloperOnlyRecognizesNestedEditorModuleNames)
	{
		using namespace ASClassMetadataTests;
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine);

		const TArray<ASClassMetadataTests::FDeveloperOnlyModuleCase> Cases =
		{
			{
				FName(TEXT("Game.Tools.Editor.Visualizers")),
				TEXT("Game/Tools/Editor/Visualizers.as"),
				FName(TEXT("ANestedEditorVisualizerActor")),
				true
			},
			{
				FName(TEXT("Editor.Visualizers")),
				TEXT("Editor/Visualizers.as"),
				FName(TEXT("APrefixedEditorVisualizerActor")),
				true
			},
			{
				FName(TEXT("Game.Tools.Runtime.Visualizers")),
				TEXT("Game/Tools/Runtime/Visualizers.as"),
				FName(TEXT("ARuntimeVisualizerActor")),
				false
			}
		};

		ON_SCOPE_EXIT
		{
			for (const ASClassMetadataTests::FDeveloperOnlyModuleCase& TestCase : Cases)
			{
				Engine.DiscardModule(*TestCase.ModuleName.ToString());
			}
			ASTEST_RESET_ENGINE(Engine);
		};

		for (const ASClassMetadataTests::FDeveloperOnlyModuleCase& TestCase : Cases)
		{
			UASClass* GeneratedClass = ASClassMetadataTests::CompileMetadataCase(*TestRunner, Engine, TestCase);
			if (GeneratedClass == nullptr)
			{
				return;
			}

			TestRunner->TestEqual(
				*FString::Printf(
					TEXT("ASClass metadata case '%s' should report the expected developer-only state"),
					*TestCase.ModuleName.ToString()),
				GeneratedClass->IsDeveloperOnly(),
				TestCase.bExpectedDeveloperOnly);
		}

		}
	}

	TEST_METHOD(IsFunctionImplementedInScriptTurnsFalseAfterDiscard)
	{
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine);

		static const FName DiscardModuleName(TEXT("Game.Tools.Runtime.DiscardMetadata"));
		static const FName DiscardGeneratedClassName(TEXT("UMetadataDiscardCarrier"));
		bool bModuleDiscarded = false;
		ON_SCOPE_EXIT
		{
			if (!bModuleDiscarded)
			{
				Engine.DiscardModule(*DiscardModuleName.ToString());
			}
			ASTEST_RESET_ENGINE(Engine);
		};

		const FString ScriptSource = TEXT(R"AS(
UCLASS()
class UMetadataDiscardCarrier : UObject
{
	UFUNCTION()
	int ComputeValue()
	{
		return 7;
	}
}
)AS");

		if (!TestRunner->TestTrue(
				TEXT("ASClass discard metadata case should compile"),
				CompileAnnotatedModuleFromMemory(
					&Engine,
					DiscardModuleName,
					TEXT("Game/Tools/Runtime/DiscardMetadata.as"),
					ScriptSource)))
		{
			return;
		}

		UASClass* GeneratedClass = Cast<UASClass>(FindGeneratedClass(&Engine, DiscardGeneratedClassName));
		if (!TestRunner->TestNotNull(TEXT("ASClass discard metadata case should generate the script class"), GeneratedClass))
		{
			return;
		}

		UASFunction* GeneratedFunction = Cast<UASFunction>(FindGeneratedFunction(GeneratedClass, TEXT("ComputeValue")));
		if (!TestRunner->TestNotNull(TEXT("ASClass discard metadata case should resolve the generated script function"), GeneratedFunction))
		{
			return;
		}

		const bool bImplementedBeforeDiscard = GeneratedClass->IsFunctionImplementedInScript(TEXT("ComputeValue"));
		const FString SourcePathBeforeDiscard = GeneratedFunction->GetSourceFilePath();
		const int32 SourceLineBeforeDiscard = GeneratedFunction->GetSourceLineNumber();

		Engine.DiscardModule(*DiscardModuleName.ToString());
		bModuleDiscarded = true;

		// UE 5.7: post-Discard cleanup of UASClass' IsFunctionImplementedInScript
		// book-keeping + source-file metadata is driven by GC finalization rather
		// than by the DiscardModule call itself. Force one pass so the asserts
		// below observe the fully-cleaned state.
		CollectGarbage(RF_NoFlags, true);

		const bool bImplementedAfterDiscard = GeneratedClass->IsFunctionImplementedInScript(TEXT("ComputeValue"));
		const FString SourcePathAfterDiscard = GeneratedFunction->GetSourceFilePath();
		const int32 SourceLineAfterDiscard = GeneratedFunction->GetSourceLineNumber();

		TestRunner->TestTrue(TEXT("ASClass should report the function implemented before discard"), bImplementedBeforeDiscard);
		TestRunner->TestFalse(TEXT("ASClass should stop reporting the function implemented after discard"), bImplementedAfterDiscard);
		TestRunner->TestFalse(TEXT("Generated script function should expose a non-empty source path before discard"), SourcePathBeforeDiscard.IsEmpty());
		TestRunner->TestTrue(TEXT("Generated script function should expose a positive source line before discard"), SourceLineBeforeDiscard > 0);
		TestRunner->TestTrue(
			TEXT("Generated script function should clear its source metadata after discard"),
			SourcePathAfterDiscard.IsEmpty() || SourceLineAfterDiscard == -1);

		}
	}
};

#endif
