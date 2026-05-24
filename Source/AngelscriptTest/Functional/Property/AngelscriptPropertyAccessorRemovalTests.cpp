#include "Shared/AngelscriptFunctionalTestUtils.h"
#include "Shared/AngelscriptReflectiveAccess.h"
#include "Shared/AngelscriptTestMacros.h"
#include "Shared/AngelscriptTestWorld.h"
#include "Testing/AngelscriptPropertyAccessorRemovalTestTypes.h"

#include "CQTest.h"
#include "Misc/ScopeExit.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptFunctionalTestUtils;

namespace PropertyAccessorRemovalTest
{
	static FAngelscriptEngineConfig CreateEditorScanFreeConfig()
	{
		FAngelscriptEngineConfig Config = FAngelscriptEngineConfig::FromCurrentProcess();
		Config.bIsEditor = true;
		Config.bForcePreprocessEditorCode = true;
		return Config;
	}

	static TUniquePtr<FAngelscriptEngine> CreateEditorScanFreeFullEngine()
	{
		AAngelscriptPropertyAccessorCarrier::StaticClass();
		return CreateScriptScanFreeFullEngineForTesting(
			CreateEditorScanFreeConfig(),
			FAngelscriptEngineDependencies::CreateDefault());
	}

	static bool CreateEditorEngineForTest(
		FAutomationTestBase& Test,
		TUniquePtr<FAngelscriptEngine>& OutEngine,
		FAngelscriptEngine*& OutEnginePtr)
	{
		OutEngine = CreateEditorScanFreeFullEngine();
		OutEnginePtr = OutEngine.Get();
		return Test.TestNotNull(TEXT("Accessor removal test should create an editor-configured scan-free engine"), OutEnginePtr);
	}

	static const FAngelscriptCompileTraceDiagnosticSummary* FindDiagnosticContaining(
		const TArray<FAngelscriptCompileTraceDiagnosticSummary>& Diagnostics,
		const TCHAR* Fragment)
	{
		for (const FAngelscriptCompileTraceDiagnosticSummary& Diagnostic : Diagnostics)
		{
			if (Diagnostic.bIsError && Diagnostic.Message.Contains(Fragment))
			{
				return &Diagnostic;
			}
		}

		return nullptr;
	}

	static bool CompileScript(
		FAutomationTestBase& Test,
		FAngelscriptEngine& Engine,
		FName ModuleName,
		const TCHAR* Filename,
		const TCHAR* Script,
		FAngelscriptCompileTraceSummary& OutSummary)
	{
		const bool bCompiled = CompileModuleWithSummary(
			&Engine,
			ECompileType::FullReload,
			ModuleName,
			FString(Filename),
			FString(Script),
			true,
			OutSummary,
			true);

		const bool bPassed = Test.TestTrue(
			*FString::Printf(TEXT("Module '%s' should compile"), *ModuleName.ToString()),
			bCompiled);

		if (!bPassed)
		{
			for (const FAngelscriptCompileTraceDiagnosticSummary& Diagnostic : OutSummary.Diagnostics)
			{
				Test.AddInfo(FString::Printf(
					TEXT("Compile diagnostic: %s @%d:%d %s"),
					Diagnostic.bIsError ? TEXT("ERROR") : TEXT("WARN"),
					Diagnostic.Row,
					Diagnostic.Column,
					*Diagnostic.Message));
			}
		}

		return bCompiled;
	}
}

TEST_CLASS_WITH_FLAGS(FAngelscriptPropertyAccessorRemovalTests,
	"Angelscript.TestModule.Functional.Property.AccessorRemoval",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(RawFieldAccessCompilesAndRuns)
	{
		TUniquePtr<FAngelscriptEngine> OwnedEngine;
		FAngelscriptEngine* EnginePtr = nullptr;
		if (!PropertyAccessorRemovalTest::CreateEditorEngineForTest(*TestRunner, OwnedEngine, EnginePtr))
		{
			return;
		}
		FAngelscriptEngine& Engine = *EnginePtr;
		FAngelscriptEngineScope Scope(Engine);
		static const FName ModuleName(TEXT("PropertyAccessorRemovalRawField"));
		ON_SCOPE_EXIT { Engine.DiscardModule(*ModuleName.ToString()); };

		const FString Source = TEXT(R"AS(
#if EDITOR
UCLASS()
class AAutoAccessorRawFieldScriptActor : AAngelscriptPropertyAccessorCarrier
{
	UFUNCTION()
	int32 CheckRawFieldAccess()
	{
		if (Field != 17)
			return 10;
		if (!bEnabled)
			return 20;

		Field = 23;
		if (Field != 23)
			return 30;

		return 1;
	}
}
#endif
)AS");

		FAngelscriptCompileTraceSummary Summary;
		if (!PropertyAccessorRemovalTest::CompileScript(*TestRunner, Engine, ModuleName, TEXT("PropertyAccessorRemovalRawField.as"), *Source, Summary))
		{
			return;
		}

		UClass* ScriptClass = FindGeneratedClass(&Engine, TEXT("AAutoAccessorRawFieldScriptActor"));
		if (!TestRunner->TestNotNull(TEXT("Raw-field test should materialize the generated class"), ScriptClass))
		{
			return;
		}

		FAngelscriptTestWorld W(*TestRunner, Engine);
		if (!W.IsValid())
		{
			return;
		}

		AActor* Actor = W.SpawnActorOfClass(ScriptClass);
		if (!TestRunner->TestNotNull(TEXT("Raw-field test should spawn the generated actor"), Actor))
		{
			return;
		}

		FFunctionInvoker Invoker(*TestRunner, Actor, FName(TEXT("CheckRawFieldAccess")));
		if (!Invoker.IsValid())
		{
			return;
		}

		const int32 Result = Invoker.CallAndReturn<int32>(INDEX_NONE);
		TestRunner->TestEqual(TEXT("Raw-field access should execute successfully"), Result, 1);
	}

	TEST_METHOD(RawFieldSyntheticGetterDoesNotCompile)
	{
		TUniquePtr<FAngelscriptEngine> OwnedEngine;
		FAngelscriptEngine* EnginePtr = nullptr;
		if (!PropertyAccessorRemovalTest::CreateEditorEngineForTest(*TestRunner, OwnedEngine, EnginePtr))
		{
			return;
		}
		FAngelscriptEngine& Engine = *EnginePtr;
		FAngelscriptEngineScope Scope(Engine);
		static const FName ModuleName(TEXT("PropertyAccessorRemovalRawFieldFailure"));
		ON_SCOPE_EXIT { Engine.DiscardModule(*ModuleName.ToString()); };

		const FString Source = TEXT(R"AS(
#if EDITOR
UCLASS()
class AAutoAccessorRawFieldScriptActorFailure : AAngelscriptPropertyAccessorCarrier
{
	UFUNCTION()
	int32 CheckSyntheticGetter()
	{
		return GetField();
	}
}
#endif
)AS");

		FAngelscriptCompileTraceSummary Summary;
		const bool bCompiled = CompileModuleWithSummary(
			&Engine,
			ECompileType::FullReload,
			ModuleName,
			TEXT("PropertyAccessorRemovalRawFieldFailure.as"),
			Source,
			true,
			Summary,
			true);

		TestRunner->TestFalse(TEXT("Synthetic getter access should stop compiling for raw fields"), bCompiled);
		TestRunner->TestFalse(TEXT("Synthetic getter access should not report success in the compile summary"), Summary.bCompileSucceeded);
		TestRunner->TestEqual(TEXT("Synthetic getter access should surface a compile error"), Summary.CompileResult, ECompileResult::Error);
		TestRunner->TestNotNull(
			TEXT("Synthetic getter failure should mention GetField in the diagnostic stream"),
			PropertyAccessorRemovalTest::FindDiagnosticContaining(Summary.Diagnostics, TEXT("GetField")));
	}

	TEST_METHOD(BlueprintGetterRemainsCallableWithoutSyntheticAlias)
	{
		TUniquePtr<FAngelscriptEngine> OwnedEngine;
		FAngelscriptEngine* EnginePtr = nullptr;
		if (!PropertyAccessorRemovalTest::CreateEditorEngineForTest(*TestRunner, OwnedEngine, EnginePtr))
		{
			return;
		}
		FAngelscriptEngine& Engine = *EnginePtr;
		FAngelscriptEngineScope Scope(Engine);
		static const FName ModuleName(TEXT("PropertyAccessorRemovalBlueprintGetter"));
		ON_SCOPE_EXIT { Engine.DiscardModule(*ModuleName.ToString()); };

		const FString Source = TEXT(R"AS(
#if EDITOR
UCLASS()
class AAutoAccessorGetterScriptActor : AAngelscriptPropertyAccessorCarrier
{
	UFUNCTION()
	int32 CheckGetterAccess()
	{
		if (Score != 7)
			return 10;

		return FetchScore();
	}
}
#endif
)AS");

		FAngelscriptCompileTraceSummary Summary;
		if (!PropertyAccessorRemovalTest::CompileScript(*TestRunner, Engine, ModuleName, TEXT("PropertyAccessorRemovalBlueprintGetter.as"), *Source, Summary))
		{
			return;
		}

		UClass* ScriptClass = FindGeneratedClass(&Engine, TEXT("AAutoAccessorGetterScriptActor"));
		if (!TestRunner->TestNotNull(TEXT("BlueprintGetter test should materialize the generated class"), ScriptClass))
		{
			return;
		}

		FAngelscriptTestWorld W(*TestRunner, Engine);
		if (!W.IsValid())
		{
			return;
		}

		AActor* Actor = W.SpawnActorOfClass(ScriptClass);
		if (!TestRunner->TestNotNull(TEXT("BlueprintGetter test should spawn the generated actor"), Actor))
		{
			return;
		}

		FFunctionInvoker Invoker(*TestRunner, Actor, FName(TEXT("CheckGetterAccess")));
		if (!Invoker.IsValid())
		{
			return;
		}

		const int32 Result = Invoker.CallAndReturn<int32>(INDEX_NONE);
		TestRunner->TestEqual(TEXT("Underlying BlueprintGetter UFUNCTION should remain callable directly"), Result, 7);
	}

	TEST_METHOD(BlueprintGetterSyntheticAliasDoesNotCompile)
	{
		TUniquePtr<FAngelscriptEngine> OwnedEngine;
		FAngelscriptEngine* EnginePtr = nullptr;
		if (!PropertyAccessorRemovalTest::CreateEditorEngineForTest(*TestRunner, OwnedEngine, EnginePtr))
		{
			return;
		}
		FAngelscriptEngine& Engine = *EnginePtr;
		FAngelscriptEngineScope Scope(Engine);
		static const FName ModuleName(TEXT("PropertyAccessorRemovalBlueprintGetterFailure"));
		ON_SCOPE_EXIT { Engine.DiscardModule(*ModuleName.ToString()); };

		const FString Source = TEXT(R"AS(
#if EDITOR
UCLASS()
class AAutoAccessorGetterScriptActorFailure : AAngelscriptPropertyAccessorCarrier
{
	UFUNCTION()
	int32 CheckSyntheticGetterAlias()
	{
		return GetScore();
	}
}
#endif
)AS");

		FAngelscriptCompileTraceSummary Summary;
		const bool bCompiled = CompileModuleWithSummary(
			&Engine,
			ECompileType::FullReload,
			ModuleName,
			TEXT("PropertyAccessorRemovalBlueprintGetterFailure.as"),
			Source,
			true,
			Summary,
			true);

		TestRunner->TestFalse(TEXT("BlueprintGetter synthetic alias should stop compiling"), bCompiled);
		TestRunner->TestFalse(TEXT("BlueprintGetter synthetic alias should not report success in the compile summary"), Summary.bCompileSucceeded);
		TestRunner->TestEqual(TEXT("BlueprintGetter synthetic alias should surface a compile error"), Summary.CompileResult, ECompileResult::Error);
		TestRunner->TestNotNull(
			TEXT("BlueprintGetter alias failure should mention GetScore in the diagnostic stream"),
			PropertyAccessorRemovalTest::FindDiagnosticContaining(Summary.Diagnostics, TEXT("GetScore")));
	}

	TEST_METHOD(PropertyDecoratorDoesNotCompile)
	{
		TUniquePtr<FAngelscriptEngine> OwnedEngine;
		FAngelscriptEngine* EnginePtr = nullptr;
		if (!PropertyAccessorRemovalTest::CreateEditorEngineForTest(*TestRunner, OwnedEngine, EnginePtr))
		{
			return;
		}
		FAngelscriptEngine& Engine = *EnginePtr;
		FAngelscriptEngineScope Scope(Engine);
		static const FName ModuleName(TEXT("PropertyAccessorRemovalDecoratorFailure"));
		ON_SCOPE_EXIT { Engine.DiscardModule(*ModuleName.ToString()); };

		const FString Source = TEXT(R"AS(
class FAutoAccessorDecoratorFailure
{
	int Stored = 3;
	int GetStored() )AS") TEXT("property") TEXT(R"AS(
	{
		return Stored;
	}
}
)AS");

		FAngelscriptCompileTraceSummary Summary;
		const bool bCompiled = CompileModuleWithSummary(
			&Engine,
			ECompileType::FullReload,
			ModuleName,
			TEXT("PropertyAccessorRemovalDecoratorFailure.as"),
			Source,
			true,
			Summary,
			true);

		TestRunner->TestFalse(TEXT("property decorator syntax should stop compiling"), bCompiled);
		TestRunner->TestFalse(TEXT("property decorator syntax should not report success in the compile summary"), Summary.bCompileSucceeded);
		TestRunner->TestEqual(TEXT("property decorator syntax should surface a compile error"), Summary.CompileResult, ECompileResult::Error);
		TestRunner->TestNotNull(
			TEXT("property decorator failure should mention the removal diagnostic"),
			PropertyAccessorRemovalTest::FindDiagnosticContaining(Summary.Diagnostics, TEXT("property' decorator has been removed")));
	}

	TEST_METHOD(VirtualPropertyBlockDoesNotCompile)
	{
		TUniquePtr<FAngelscriptEngine> OwnedEngine;
		FAngelscriptEngine* EnginePtr = nullptr;
		if (!PropertyAccessorRemovalTest::CreateEditorEngineForTest(*TestRunner, OwnedEngine, EnginePtr))
		{
			return;
		}
		FAngelscriptEngine& Engine = *EnginePtr;
		FAngelscriptEngineScope Scope(Engine);
		static const FName ModuleName(TEXT("PropertyAccessorRemovalVirtualPropertyFailure"));
		ON_SCOPE_EXIT { Engine.DiscardModule(*ModuleName.ToString()); };

		const FString Source = TEXT(R"AS(
class FAutoAccessorVirtualPropertyFailure
{
	int Stored = 3;
	int StoredValue { )AS") TEXT("get") TEXT(R"AS(; set; }
}
)AS");

		FAngelscriptCompileTraceSummary Summary;
		const bool bCompiled = CompileModuleWithSummary(
			&Engine,
			ECompileType::FullReload,
			ModuleName,
			TEXT("PropertyAccessorRemovalVirtualPropertyFailure.as"),
			Source,
			true,
			Summary,
			true);

		TestRunner->TestFalse(TEXT("virtual property syntax should stop compiling"), bCompiled);
		TestRunner->TestFalse(TEXT("virtual property syntax should not report success in the compile summary"), Summary.bCompileSucceeded);
		TestRunner->TestEqual(TEXT("virtual property syntax should surface a compile error"), Summary.CompileResult, ECompileResult::Error);
		TestRunner->TestNotNull(
			TEXT("virtual property failure should mention the removal diagnostic"),
			PropertyAccessorRemovalTest::FindDiagnosticContaining(Summary.Diagnostics, TEXT("Virtual property syntax has been removed")));
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
