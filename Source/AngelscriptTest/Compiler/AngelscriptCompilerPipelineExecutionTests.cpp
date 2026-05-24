#include "AngelscriptTestEngineHelper.h"
#include "AngelscriptTestMacros.h"

#include "CQTest.h"
#include "Misc/ScopeExit.h"
#include "UObject/Class.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UnrealType.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace CompilerPipelineExecutionTest
{
	static const FName ModuleName(TEXT("CompilerAnnotatedMethodExecutes"));
	static const FString ScriptFilename(TEXT("CompilerAnnotatedMethodExecutes.as"));
	static const FName GeneratedClassName(TEXT("UCompilerExecutionCarrier"));
	static const FName ExecutionFunctionName(TEXT("IncrementAndGetScore"));
	static const FName ScorePropertyName(TEXT("Score"));
}

namespace CompilerPipelinePlainSourceRoundTripTest
{
	static const FName ModuleName(TEXT("Tests.Compiler.PlainSourceRoundTrip"));
	static const FString RelativeScriptPath(TEXT("Tests/Compiler/PlainSourceRoundTrip.as"));
}

TEST_CLASS_WITH_FLAGS(FAngelscriptCompilerPipelineExecutionTest,
	"Angelscript.TestModule.Compiler.EndToEnd",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(AnnotatedMethodExecutes)
	{
		using namespace CompilerPipelineExecutionTest;

		const FString ScriptSource = TEXT(R"AS(
UCLASS()
class UCompilerExecutionCarrier : UObject
{
	UPROPERTY()
	int Score = 41;

	UFUNCTION()
	int IncrementAndGetScore()
	{
		Score += 1;
		return Score;
	}
}
)AS");

		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine);

		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*CompilerPipelineExecutionTest::ModuleName.ToString());
		};

		const bool bCompiled = CompileAnnotatedModuleFromMemory(
			&Engine,
			CompilerPipelineExecutionTest::ModuleName,
			CompilerPipelineExecutionTest::ScriptFilename,
			ScriptSource);
		if (!TestRunner->TestTrue(TEXT("Annotated execution test should compile the generated class module"), bCompiled))
		{
			return;
		}

		UClass* GeneratedClass = FindGeneratedClass(&Engine, CompilerPipelineExecutionTest::GeneratedClassName);
		if (!TestRunner->TestNotNull(TEXT("Annotated execution test should find the generated class"), GeneratedClass))
		{
			return;
		}

		UFunction* GeneratedFunction = FindGeneratedFunction(GeneratedClass, CompilerPipelineExecutionTest::ExecutionFunctionName);
		if (!TestRunner->TestNotNull(TEXT("Annotated execution test should find the generated execution function"), GeneratedFunction))
		{
			return;
		}

		FIntProperty* ScoreProperty = FindFProperty<FIntProperty>(GeneratedClass, CompilerPipelineExecutionTest::ScorePropertyName);
		if (!TestRunner->TestNotNull(TEXT("Annotated execution test should expose the generated Score property"), ScoreProperty))
		{
			return;
		}

		UObject* RuntimeObject = NewObject<UObject>(GetTransientPackage(), GeneratedClass, TEXT("CompilerExecutionCarrier"));
		if (!TestRunner->TestNotNull(TEXT("Annotated execution test should instantiate the generated class"), RuntimeObject))
		{
			return;
		}

		const int32 InitialScore = ScoreProperty->GetPropertyValue_InContainer(RuntimeObject);
		if (!TestRunner->TestEqual(TEXT("Annotated execution test should materialize the scripted default before invocation"), InitialScore, 41))
		{
			return;
		}

		int32 Result = 0;
		if (!TestRunner->TestTrue(
			TEXT("Annotated execution test should execute the generated method on the game thread"),
			ExecuteGeneratedIntEventOnGameThread(&Engine, RuntimeObject, GeneratedFunction, Result)))
		{
			return;
		}

		const int32 ScoreAfterCall = ScoreProperty->GetPropertyValue_InContainer(RuntimeObject);
		TestRunner->TestEqual(TEXT("Annotated execution test should return the updated scripted value"), Result, 42);
		TestRunner->TestEqual(TEXT("Annotated execution test should persist the scripted state mutation on the UObject instance"), ScoreAfterCall, 42);

		}
	}

	TEST_METHOD(PlainSourcePreprocessorRoundTrip)
	{
		using namespace CompilerPipelinePlainSourceRoundTripTest;

		const FString ScriptSource = TEXT(R"AS(
int Entry()
{
	return 42;
}
)AS");

		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine);

		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*CompilerPipelinePlainSourceRoundTripTest::ModuleName.ToString());
		};

		FAngelscriptCompileTraceSummary Summary;
		const bool bCompiled = CompileModuleWithSummary(
			&Engine,
			ECompileType::SoftReloadOnly,
			CompilerPipelinePlainSourceRoundTripTest::ModuleName,
			CompilerPipelinePlainSourceRoundTripTest::RelativeScriptPath,
			ScriptSource,
			true,
			Summary,
			true);

		TestRunner->TestTrue(
			TEXT("Plain source preprocessor round-trip should compile successfully"),
			bCompiled);
		TestRunner->TestTrue(
			TEXT("Plain source preprocessor round-trip should report preprocessor usage"),
			Summary.bUsedPreprocessor);
		TestRunner->TestTrue(
			TEXT("Plain source preprocessor round-trip should mark compile succeeded in the summary"),
			Summary.bCompileSucceeded);
		TestRunner->TestEqual(
			TEXT("Plain source preprocessor round-trip should report FullyHandled"),
			Summary.CompileResult,
			ECompileResult::FullyHandled);
		TestRunner->TestEqual(
			TEXT("Plain source preprocessor round-trip should produce exactly one module descriptor"),
			Summary.ModuleDescCount,
			1);
		TestRunner->TestEqual(
			TEXT("Plain source preprocessor round-trip should keep diagnostics empty"),
			Summary.Diagnostics.Num(),
			0);
		TestRunner->TestEqual(
			TEXT("Plain source preprocessor round-trip should record exactly one module name"),
			Summary.ModuleNames.Num(),
			1);
		if (Summary.ModuleNames.Num() > 0)
		{
			TestRunner->TestEqual(
				TEXT("Plain source preprocessor round-trip should normalize the module name from the relative script path"),
				Summary.ModuleNames[0],
				TEXT("Tests.Compiler.PlainSourceRoundTrip"));
		}

		int32 EntryResult = 0;
		const bool bExecuted = bCompiled
			&& ExecuteIntFunction(
				&Engine,
				CompilerPipelinePlainSourceRoundTripTest::RelativeScriptPath,
				CompilerPipelinePlainSourceRoundTripTest::ModuleName,
				TEXT("int Entry()"),
				EntryResult);
		TestRunner->TestTrue(
			TEXT("Plain source preprocessor round-trip should execute the compiled Entry function"),
			bExecuted);
		if (bExecuted)
		{
			TestRunner->TestEqual(
				TEXT("Plain source preprocessor round-trip should preserve the plain-source return value"),
				EntryResult,
				42);
		}

		}
	}
};

#endif
