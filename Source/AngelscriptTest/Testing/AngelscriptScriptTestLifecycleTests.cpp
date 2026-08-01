#include "CQTest.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestModuleScope.h"
#include "AngelscriptScriptTestTestHelpers.h"

#include "Testing/AngelscriptScriptTestRegistry.h"
#include "Testing/AngelscriptScriptTestRunner.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FAngelscriptScriptTestLifecycleTests,
	"Angelscript.TestModule.Testing.ScriptTestFramework.Lifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	BEFORE_ALL()
	{
		ASTEST_CREATE_ENGINE();
	}

	AFTER_ALL()
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		ASTEST_RESET_ENGINE(Engine);
	}

	TEST_METHOD(FreshInstanceEachLifecycleAndLifoTeardown)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		const FString ScriptSource = ASTEST_AS(R"AS(
			UCLASS(meta=(AngelscriptTestFlags="EditorContext;EngineFilter"))
			class ULifecycleScriptTests : UAngelscriptTestSuite
			{
				int Value = 0;

				UFUNCTION(BlueprintOverride)
				void BeforeEach()
				{
					AssertEquals(0, Value);
					Value = 1;
					FAngelscriptTest::Commands()
						.OnCleanup(n"CleanupOne")
						.OnCleanup(n"CleanupTwo");
				}

				UFUNCTION(meta=(AngelscriptTest))
				void ExecutesWithFixtureState()
				{
					AssertEquals(1, Value);
					Value = 2;
				}

				UFUNCTION(BlueprintOverride)
				void AfterEach()
				{
					AssertEquals(2, Value);
					Value = 3;
				}

				void CleanupOne()
				{
					AssertEquals(4, Value);
					Value = 5;
				}

				void CleanupTwo()
				{
					AssertEquals(3, Value);
					Value = 4;
				}
			}
			)AS");

		FScopedAngelscriptModule ModuleScope(
			*TestRunner,
			Engine,
			TEXT("ASTesting_ScriptTestLifecycle_Fresh"),
			ScriptSource);
		ASSERT_THAT(IsTrue(ModuleScope.IsValid()));

		const FAngelscriptScriptTestRegistryBuildResult Build =
			FAngelscriptScriptTestRegistry::Get().Rebuild(
				Engine.GetActiveModules(),
				true);
		const FAngelscriptScriptTestDescriptor* Descriptor =
			FindOnlyAngelscriptScriptTestDescriptor(Build);
		ASSERT_THAT(IsNotNull(Descriptor));

		for (int32 RunIndex = 0; RunIndex < 2; ++RunIndex)
		{
			FAngelscriptScriptTestProbe Probe(
				FString::Printf(
					TEXT("FAngelscriptScriptTestLifecycleProbe_%d_%p"),
					RunIndex,
					this));
			const TSharedPtr<FAngelscriptScriptTestExecutionContext> Context =
				FAngelscriptScriptTestRunner::Start(
					Descriptor->Id,
					Probe);
			ASSERT_THAT(IsTrue(Context.IsValid()));
			ASSERT_THAT(IsTrue(Context->IsComplete()));
			ASSERT_THAT(IsFalse(Context->HasFailed()));
			ASSERT_THAT(AreEqual(5, Context->GetTrace().Num()));
			ASSERT_THAT(AreEqual(FName(TEXT("BeforeEach")), Context->GetTrace()[0]));
			ASSERT_THAT(AreEqual(FName(TEXT("ExecutesWithFixtureState")), Context->GetTrace()[1]));
			ASSERT_THAT(AreEqual(FName(TEXT("AfterEach")), Context->GetTrace()[2]));
			ASSERT_THAT(AreEqual(FName(TEXT("CleanupTwo")), Context->GetTrace()[3]));
			ASSERT_THAT(AreEqual(FName(TEXT("CleanupOne")), Context->GetTrace()[4]));
		}
	}

	TEST_METHOD(ControlledAssertionStopsCallAndStillRunsAfterEach)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		const FString ScriptSource = ASTEST_AS(R"AS(
			UCLASS(meta=(AngelscriptTestFlags="EditorContext;EngineFilter"))
			class UFailFastLifecycleScriptTests : UAngelscriptTestSuite
			{
				int Value = 0;

				UFUNCTION(BlueprintOverride)
				void BeforeEach()
				{
					Value = 1;
				}

				UFUNCTION(meta=(AngelscriptTest))
				void StopsAtAssertion()
				{
					AssertTrue(false, "intentional failure");
					Value = 99;
				}

				UFUNCTION(BlueprintOverride)
				void AfterEach()
				{
					if (Value != 1)
						Fail("statement after assertion executed");
				}
			}
			)AS");

		FScopedAngelscriptModule ModuleScope(
			*TestRunner,
			Engine,
			TEXT("ASTesting_ScriptTestLifecycle_FailFast"),
			ScriptSource);
		ASSERT_THAT(IsTrue(ModuleScope.IsValid()));
		const FAngelscriptScriptTestRegistryBuildResult Build =
			FAngelscriptScriptTestRegistry::Get().Rebuild(
				Engine.GetActiveModules(),
				true);
		const FAngelscriptScriptTestDescriptor* Descriptor =
			FindOnlyAngelscriptScriptTestDescriptor(Build);
		ASSERT_THAT(IsNotNull(Descriptor));

		FAngelscriptScriptTestProbe Probe(
			FString::Printf(
				TEXT("FAngelscriptScriptTestFailFastProbe_%p"),
				this));
		const TSharedPtr<FAngelscriptScriptTestExecutionContext> Context =
			FAngelscriptScriptTestRunner::Start(
				Descriptor->Id,
				Probe);
		ASSERT_THAT(IsTrue(Context.IsValid()));
		ASSERT_THAT(IsTrue(Context->IsComplete()));
		ASSERT_THAT(IsTrue(Context->HasFailed()));
		ASSERT_THAT(IsTrue(Probe.HasAnyErrors()));
		ASSERT_THAT(AreEqual(3, Context->GetTrace().Num()));
		ASSERT_THAT(AreEqual(FName(TEXT("AfterEach")), Context->GetTrace().Last()));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
