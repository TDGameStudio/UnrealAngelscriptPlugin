#include "AngelscriptTestEngineHelper.h"
#include "AngelscriptTestMacros.h"

#include "CQTest.h"
#include "Misc/ScopeExit.h"
#include "UObject/Class.h"
#include "UObject/UObjectGlobals.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace CompilerPipelineClassLikeExecutionTest
{
	static const FName ModuleName(TEXT("Tests.Compiler.ClassLikeMethodExecutionRoundTrip"));
	static const FString ScriptFilename(TEXT("Tests/Compiler/ClassLikeMethodExecutionRoundTrip.as"));
	static const FName GeneratedClassName(TEXT("UCompilerClassLikeExecutionCarrier"));
	static const FName VerifyFunctionName(TEXT("VerifyRoundTrip"));
}

TEST_CLASS_WITH_FLAGS(FCompilerPipelineClassLikeExecutionTests,
	"Angelscript.TestModule.Compiler.EndToEnd",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(ClassLikeMethodExecutionRoundTrip)
	{


		const FString ScriptSource = TEXT(R"AS(
	UCLASS()
	class UCompilerClassLikeExecutionCarrier : UObject
	{
		UFUNCTION()
		UClass EchoPlainClass(UClass Value)
		{
			return Value;
		}

		UFUNCTION()
		TSubclassOf<AActor> EchoActorClass(TSubclassOf<AActor> Value)
		{
			return Value;
		}

		UFUNCTION()
		TSoftClassPtr<AActor> EchoSoftActorClass(TSoftClassPtr<AActor> Value)
		{
			return Value;
		}

		UFUNCTION()
		int VerifyRoundTrip()
		{
			if (!(EchoPlainClass(AActor::StaticClass()) == AActor::StaticClass()))
				return 10;

			if (!(EchoActorClass(ACameraActor::StaticClass()) == ACameraActor::StaticClass()))
				return 20;

			TSoftClassPtr<AActor> SoftActorClass = TSoftClassPtr<AActor>(AActor::StaticClass());
			if (!(EchoSoftActorClass(SoftActorClass).Get() == AActor::StaticClass()))
				return 30;

			return 1;
		}
	}
	)AS");

		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine);

		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*CompilerPipelineClassLikeExecutionTest::ModuleName.ToString());
		};

		FAngelscriptCompileTraceSummary Summary;
		const bool bCompiled = CompileModuleWithSummary(
			&Engine,
			ECompileType::FullReload,
			CompilerPipelineClassLikeExecutionTest::ModuleName,
			CompilerPipelineClassLikeExecutionTest::ScriptFilename,
			ScriptSource,
			true,
			Summary);

		ASSERT_THAT(IsTrue(bCompiled, TEXT("Class-like method execution round-trip should compile successfully")));
		ASSERT_THAT(IsTrue(Summary.bUsedPreprocessor, TEXT("Class-like method execution round-trip should run through the preprocessor path")));
		ASSERT_THAT(IsTrue(Summary.bCompileSucceeded, TEXT("Class-like method execution round-trip should mark compile succeeded in the summary")));
		ASSERT_THAT(AreEqual(0, Summary.Diagnostics.Num(), TEXT("Class-like method execution round-trip should not emit diagnostics")));
		if (!bCompiled)
		{
			return;
		}

		UClass* GeneratedClass = FindGeneratedClass(&Engine, CompilerPipelineClassLikeExecutionTest::GeneratedClassName);
		ASSERT_THAT(IsNotNull(GeneratedClass, TEXT("Class-like method execution round-trip should generate the annotated carrier class")));

		UFunction* EchoPlainClass = FindGeneratedFunction(GeneratedClass, TEXT("EchoPlainClass"));
		UFunction* EchoActorClass = FindGeneratedFunction(GeneratedClass, TEXT("EchoActorClass"));
		UFunction* EchoSoftActorClass = FindGeneratedFunction(GeneratedClass, TEXT("EchoSoftActorClass"));
		UFunction* VerifyRoundTrip = FindGeneratedFunction(GeneratedClass, CompilerPipelineClassLikeExecutionTest::VerifyFunctionName);
		ASSERT_THAT(IsNotNull(EchoPlainClass, TEXT("Class-like method execution round-trip should expose EchoPlainClass")));
		ASSERT_THAT(IsNotNull(EchoActorClass, TEXT("Class-like method execution round-trip should expose EchoActorClass")));
		ASSERT_THAT(IsNotNull(EchoSoftActorClass, TEXT("Class-like method execution round-trip should expose EchoSoftActorClass")));
		ASSERT_THAT(IsNotNull(VerifyRoundTrip, TEXT("Class-like method execution round-trip should expose VerifyRoundTrip")));

		UObject* RuntimeObject = NewObject<UObject>(GetTransientPackage(), GeneratedClass, TEXT("CompilerClassLikeExecutionCarrier"));
		ASSERT_THAT(IsNotNull(RuntimeObject, TEXT("Class-like method execution round-trip should instantiate the generated class")));

		int32 Result = 0;
		const bool bExecuted = ExecuteGeneratedIntEventOnGameThread(&Engine, RuntimeObject, VerifyRoundTrip, Result);
		ASSERT_THAT(IsTrue(
			bExecuted,
			TEXT("Class-like method execution round-trip should execute the generated verification method")));
		if (bExecuted)
		{
			ASSERT_THAT(AreEqual(
				1,
				Result,
				TEXT("Class-like method execution round-trip should preserve plain class, subclass and soft-class marshalling")));
		}

		}

	}

};

#endif
