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
		if (!this->Assert.IsTrue(bCompiled, TEXT("Annotated execution test should compile the generated class module")))
		{
			return;
		}

		UClass* GeneratedClass = FindGeneratedClass(&Engine, CompilerPipelineExecutionTest::GeneratedClassName);
		if (!this->Assert.IsNotNull(GeneratedClass, TEXT("Annotated execution test should find the generated class")))
		{
			return;
		}

		UFunction* GeneratedFunction = FindGeneratedFunction(GeneratedClass, CompilerPipelineExecutionTest::ExecutionFunctionName);
		if (!this->Assert.IsNotNull(GeneratedFunction, TEXT("Annotated execution test should find the generated execution function")))
		{
			return;
		}

		FIntProperty* ScoreProperty = FindFProperty<FIntProperty>(GeneratedClass, CompilerPipelineExecutionTest::ScorePropertyName);
		if (!this->Assert.IsNotNull(ScoreProperty, TEXT("Annotated execution test should expose the generated Score property")))
		{
			return;
		}

		UObject* RuntimeObject = NewObject<UObject>(GetTransientPackage(), GeneratedClass, TEXT("CompilerExecutionCarrier"));
		if (!this->Assert.IsNotNull(RuntimeObject, TEXT("Annotated execution test should instantiate the generated class")))
		{
			return;
		}

		const int32 InitialScore = ScoreProperty->GetPropertyValue_InContainer(RuntimeObject);
		if (!this->Assert.AreEqual(41, InitialScore, TEXT("Annotated execution test should materialize the scripted default before invocation")))
		{
			return;
		}

		int32 Result = 0;
		if (!this->Assert.IsTrue(
			ExecuteGeneratedIntEventOnGameThread(&Engine, RuntimeObject, GeneratedFunction, Result),
			TEXT("Annotated execution test should execute the generated method on the game thread")))
		{
			return;
		}

		const int32 ScoreAfterCall = ScoreProperty->GetPropertyValue_InContainer(RuntimeObject);
		ASSERT_THAT(AreEqual(42, Result, TEXT("Annotated execution test should return the updated scripted value")));
		ASSERT_THAT(AreEqual(42, ScoreAfterCall, TEXT("Annotated execution test should persist the scripted state mutation on the UObject instance")));

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

		ASSERT_THAT(IsTrue(
			bCompiled,
			TEXT("Plain source preprocessor round-trip should compile successfully")));
		ASSERT_THAT(IsTrue(
			Summary.bUsedPreprocessor,
			TEXT("Plain source preprocessor round-trip should report preprocessor usage")));
		ASSERT_THAT(IsTrue(
			Summary.bCompileSucceeded,
			TEXT("Plain source preprocessor round-trip should mark compile succeeded in the summary")));
		ASSERT_THAT(AreEqual(
			ECompileResult::FullyHandled,
			Summary.CompileResult,
			TEXT("Plain source preprocessor round-trip should report FullyHandled")));
		ASSERT_THAT(AreEqual(
			1,
			Summary.ModuleDescCount,
			TEXT("Plain source preprocessor round-trip should produce exactly one module descriptor")));
		ASSERT_THAT(AreEqual(
			0,
			Summary.Diagnostics.Num(),
			TEXT("Plain source preprocessor round-trip should keep diagnostics empty")));
		ASSERT_THAT(AreEqual(
			1,
			Summary.ModuleNames.Num(),
			TEXT("Plain source preprocessor round-trip should record exactly one module name")));
		if (Summary.ModuleNames.Num() > 0)
		{
			ASSERT_THAT(AreEqual(
				FString(TEXT("Tests.Compiler.PlainSourceRoundTrip")),
				Summary.ModuleNames[0],
				TEXT("Plain source preprocessor round-trip should normalize the module name from the relative script path")));
		}

		int32 EntryResult = 0;
		const bool bExecuted = bCompiled
			&& ExecuteIntFunction(
				&Engine,
				CompilerPipelinePlainSourceRoundTripTest::RelativeScriptPath,
				CompilerPipelinePlainSourceRoundTripTest::ModuleName,
				TEXT("int Entry()"),
				EntryResult);
		ASSERT_THAT(IsTrue(
			bExecuted,
			TEXT("Plain source preprocessor round-trip should execute the compiled Entry function")));
		if (bExecuted)
		{
			ASSERT_THAT(AreEqual(
				42,
				EntryResult,
				TEXT("Plain source preprocessor round-trip should preserve the plain-source return value")));
		}

		}
	}
};

#endif
