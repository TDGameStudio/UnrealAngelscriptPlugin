#include "CQTest.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestModuleScope.h"
#include "AngelscriptScriptTestTestHelpers.h"

#include "HAL/PlatformProcess.h"
#include "GameFramework/PlayerController.h"
#include "Testing/AngelscriptScriptTestRegistry.h"
#include "Testing/AngelscriptScriptTestRunner.h"
#include "Testing/LatentAutomationCommandClientExecutor.h"
#include "UObject/UObjectIterator.h"
#include "UObject/UnrealType.h"

#if WITH_ANGELSCRIPT_UNITTESTS

namespace
{
	void DrainScriptTestContext(
		const TSharedPtr<FAngelscriptScriptTestExecutionContext>& Context,
		int32 MaximumUpdates = 32)
	{
		for (int32 Index = 0;
			Context.IsValid()
				&& !Context->IsComplete()
				&& Index < MaximumUpdates;
			++Index)
		{
			Context->Update();
		}
	}
}

TEST_CLASS_WITH_FLAGS(FAngelscriptScriptTestCommandTests,
	"Angelscript.TestModule.Testing.ScriptTestFramework.Commands",
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

	TEST_METHOD(FluentAliasesRunFifoAndCleanupRunsLifo)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		const FString ScriptSource = ASTEST_AS(R"AS(
			UCLASS(meta=(AngelscriptTestFlags="EditorContext;EngineFilter"))
			class UFluentCommandScriptTests : UAngelscriptTestSuite
			{
				int Value = 0;

				UFUNCTION(meta=(AngelscriptTest))
				void BuildsTheQueueImmediately()
				{
					FAngelscriptTestCommandBuilder Builder =
						FAngelscriptTest::Commands();
					Builder.Do(n"First", "first action")
						.StartWhen(n"Ready", 5.0, "ready condition")
						.Then(n"Second", "second action");
					FAngelscriptTest::Commands()
						.OnTearDown(n"CleanupOne")
						.OnCleanup(n"CleanupTwo");
					Value = 10;
				}

				UFUNCTION(BlueprintOverride)
				void AfterEach()
				{
					AssertEquals(12, Value);
					Value = 13;
					FAngelscriptTest::Commands()
						.OnCleanup(n"CleanupFromAfterEach");
				}

				void First()
				{
					AssertEquals(10, Value);
					Value = 11;
				}

				bool Ready()
				{
					return Value == 11;
				}

				void Second()
				{
					AssertEquals(11, Value);
					Value = 12;
				}

				void CleanupOne()
				{
					AssertEquals(15, Value);
					Value = 16;
				}

				void CleanupTwo()
				{
					AssertEquals(14, Value);
					Value = 15;
				}

				void CleanupFromAfterEach()
				{
					AssertEquals(13, Value);
					Value = 14;
				}
			}
			)AS");

		FScopedAngelscriptModule ModuleScope(
			*TestRunner,
			Engine,
			TEXT("ASTesting_ScriptTestCommands_Fluent"),
			ScriptSource);
		ASSERT_THAT(IsTrue(ModuleScope.IsValid()));
		const FAngelscriptScriptTestRegistryBuildResult Build =
			FAngelscriptScriptTestRegistry::Get().Rebuild(
				Engine.GetActiveModules(),
				true);
		const FAngelscriptScriptTestDescriptor* Descriptor =
			FindOnlyAngelscriptScriptTestDescriptor(Build);
		ASSERT_THAT(IsNotNull(Descriptor));

		const TSharedPtr<FAngelscriptScriptTestExecutionContext> Context =
			FAngelscriptScriptTestRunner::Start(
				Descriptor->Id,
				*TestRunner);
		ASSERT_THAT(IsTrue(Context.IsValid()));
		ASSERT_THAT(IsFalse(Context->IsComplete()));
		DrainScriptTestContext(Context);
		ASSERT_THAT(IsTrue(Context->IsComplete()));
		ASSERT_THAT(IsFalse(Context->HasFailed()));

		const TArray<FName>& Trace = Context->GetTrace();
		const TArray<FName> ExpectedTrace = {
			TEXT("BeforeEach"),
			TEXT("BuildsTheQueueImmediately"),
			TEXT("First"),
			TEXT("Ready"),
			TEXT("Second"),
			TEXT("AfterEach"),
			TEXT("CleanupFromAfterEach"),
			TEXT("CleanupTwo"),
			TEXT("CleanupOne"),
		};
		ASSERT_THAT(AreEqual(ExpectedTrace.Num(), Trace.Num()));
		for (int32 Index = 0; Index < ExpectedTrace.Num(); ++Index)
		{
			ASSERT_THAT(AreEqual(ExpectedTrace[Index], Trace[Index]));
		}
	}

	TEST_METHOD(WaitDelayUsesElapsedTimeWithoutAdvancingWorld)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		const FString ScriptSource = ASTEST_AS(R"AS(
			UCLASS(meta=(AngelscriptTestFlags="EditorContext;EngineFilter"))
			class UDelayCommandScriptTests : UAngelscriptTestSuite
			{
				UFUNCTION(meta=(AngelscriptTest))
				void WaitsThenContinues()
				{
					FAngelscriptTest::Commands()
						.WaitDelay(0.01, "short real-time delay")
						.Then(n"AfterDelay");
				}

				void AfterDelay()
				{
					AssertNull(FAngelscriptTest::GetTestWorld());
				}
			}
			)AS");

		FScopedAngelscriptModule ModuleScope(
			*TestRunner,
			Engine,
			TEXT("ASTesting_ScriptTestCommands_Delay"),
			ScriptSource);
		ASSERT_THAT(IsTrue(ModuleScope.IsValid()));
		const FAngelscriptScriptTestRegistryBuildResult Build =
			FAngelscriptScriptTestRegistry::Get().Rebuild(
				Engine.GetActiveModules(),
				true);
		const FAngelscriptScriptTestDescriptor* Descriptor =
			FindOnlyAngelscriptScriptTestDescriptor(Build);
		ASSERT_THAT(IsNotNull(Descriptor));

		const TSharedPtr<FAngelscriptScriptTestExecutionContext> Context =
			FAngelscriptScriptTestRunner::Start(
				Descriptor->Id,
				*TestRunner);
		ASSERT_THAT(IsTrue(Context.IsValid()));
		ASSERT_THAT(IsFalse(Context->Update()));
		ASSERT_THAT(IsFalse(Context->IsComplete()));
		FPlatformProcess::SleepNoStats(0.02f);
		DrainScriptTestContext(Context);
		ASSERT_THAT(IsTrue(Context->IsComplete()));
		ASSERT_THAT(IsFalse(Context->HasFailed()));
		ASSERT_THAT(IsTrue(
			Context->GetTrace().Contains(TEXT("AfterDelay"))));
	}

	TEST_METHOD(CallbackScopeRoutesBetweenMultipleWaitingLeaves)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		const FString ScriptSource = ASTEST_AS(R"AS(
			UCLASS()
			class UCallbackRouteObject : UObject
			{
			}

			UCLASS(meta=(AngelscriptTestFlags="EditorContext;EngineFilter"))
			class UCallbackRouteScriptTests : UAngelscriptTestSuite
			{
				UFUNCTION(meta=(AngelscriptTest))
				void FirstLeaf()
				{
					FAngelscriptTest::Commands()
						.Do(n"VerifyCurrentLeaf");
				}

				UFUNCTION(meta=(AngelscriptTest))
				void SecondLeaf()
				{
					FAngelscriptTest::Commands()
						.Do(n"VerifyCurrentLeaf");
				}

				void VerifyCurrentLeaf()
				{
					UObject Object = FAngelscriptTest::SpawnObject(
						UCallbackRouteObject::StaticClass());
					AssertSame(this, Object.GetOuter());
				}
			}
			)AS");

		FScopedAngelscriptModule ModuleScope(
			*TestRunner,
			Engine,
			TEXT("ASTesting_ScriptTestCommands_CallbackRouting"),
			ScriptSource);
		ASSERT_THAT(IsTrue(ModuleScope.IsValid()));
		const FAngelscriptScriptTestRegistryBuildResult Build =
			FAngelscriptScriptTestRegistry::Get().Rebuild(
				Engine.GetActiveModules(),
				true);
		ASSERT_THAT(IsTrue(Build.Snapshot.IsValid()));
		ASSERT_THAT(AreEqual(2, Build.Snapshot->Tests.Num()));

		auto FindDescriptor = [&Build](const TCHAR* MethodName)
			-> const FAngelscriptScriptTestDescriptor*
		{
			return Build.Snapshot->Tests.FindByPredicate(
				[MethodName](
					const FAngelscriptScriptTestDescriptor& Candidate)
				{
					return Candidate.Id.MethodName == MethodName;
				});
		};
		const FAngelscriptScriptTestDescriptor* First =
			FindDescriptor(TEXT("FirstLeaf"));
		const FAngelscriptScriptTestDescriptor* Second =
			FindDescriptor(TEXT("SecondLeaf"));
		ASSERT_THAT(IsNotNull(First));
		ASSERT_THAT(IsNotNull(Second));

		FAngelscriptScriptTestProbe FirstProbe(
			FString::Printf(
				TEXT("FAngelscriptScriptTestFirstRouteProbe_%p"),
				this));
		FAngelscriptScriptTestProbe SecondProbe(
			FString::Printf(
				TEXT("FAngelscriptScriptTestSecondRouteProbe_%p"),
				this));
		const TSharedPtr<FAngelscriptScriptTestExecutionContext> FirstContext =
			FAngelscriptScriptTestRunner::Start(
				First->Id,
				FirstProbe,
				false);
		const TSharedPtr<FAngelscriptScriptTestExecutionContext> SecondContext =
			FAngelscriptScriptTestRunner::Start(
				Second->Id,
				SecondProbe,
				false);
		ASSERT_THAT(IsTrue(FirstContext.IsValid()));
		ASSERT_THAT(IsTrue(SecondContext.IsValid()));
		ASSERT_THAT(IsFalse(FirstContext->IsComplete()));
		ASSERT_THAT(IsFalse(SecondContext->IsComplete()));

		DrainScriptTestContext(SecondContext);
		DrainScriptTestContext(FirstContext);
		ASSERT_THAT(IsFalse(FirstContext->HasFailed()));
		ASSERT_THAT(IsFalse(SecondContext->HasFailed()));
	}

	TEST_METHOD(InvalidTimeoutAndQueueMutationFailAtTheirCallSites)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		const FString ScriptSource = ASTEST_AS(R"AS(
			UCLASS(meta=(AngelscriptTestFlags="EditorContext;EngineFilter"))
			class UInvalidCommandScriptTests : UAngelscriptTestSuite
			{
				UFUNCTION(meta=(AngelscriptTest))
				void RejectsExcessiveTimeout()
				{
					FAngelscriptTest::Commands()
						.Until(n"NeverReady", 15.01);
				}

				bool NeverReady()
				{
					return false;
				}
			}
			)AS");

		FScopedAngelscriptModule ModuleScope(
			*TestRunner,
			Engine,
			TEXT("ASTesting_ScriptTestCommands_Invalid"),
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
				TEXT("FAngelscriptScriptTestInvalidTimeoutProbe_%p"),
				this));
		const TSharedPtr<FAngelscriptScriptTestExecutionContext> Context =
			FAngelscriptScriptTestRunner::Start(
				Descriptor->Id,
				Probe);
		ASSERT_THAT(IsTrue(Context.IsValid()));
		ASSERT_THAT(IsTrue(Context->HasFailed()));
		const TArray<FAutomationExecutionEntry> Entries =
			Probe.GetExecutionEntries();
		ASSERT_THAT(AreEqual(1, Entries.Num()));
		ASSERT_THAT(IsTrue(
			Entries[0].Event.Message.Contains(
				TEXT("at most 15 seconds"))));
	}

	TEST_METHOD(CommandCallbackCannotMutateActiveQueue)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		const FString ScriptSource = ASTEST_AS(R"AS(
			UCLASS(meta=(AngelscriptTestFlags="EditorContext;EngineFilter"))
			class UMutatingCommandScriptTests : UAngelscriptTestSuite
			{
				UFUNCTION(meta=(AngelscriptTest))
				void QueuesMutatingCallback()
				{
					FAngelscriptTest::Commands()
						.Do(n"MutateQueue")
						.OnCleanup(n"Cleanup");
				}

				void MutateQueue()
				{
					FAngelscriptTest::Commands()
						.Then(n"NeverRuns");
				}

				void NeverRuns()
				{
					Fail("mutated command unexpectedly ran");
				}

				void Cleanup()
				{
				}
			}
			)AS");

		FScopedAngelscriptModule ModuleScope(
			*TestRunner,
			Engine,
			TEXT("ASTesting_ScriptTestCommands_Mutation"),
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
				TEXT("FAngelscriptScriptTestMutationProbe_%p"),
				this));
		const TSharedPtr<FAngelscriptScriptTestExecutionContext> Context =
			FAngelscriptScriptTestRunner::Start(
				Descriptor->Id,
				Probe);
		ASSERT_THAT(IsTrue(Context.IsValid()));
		DrainScriptTestContext(Context);
		ASSERT_THAT(IsTrue(Context->HasFailed()));
		ASSERT_THAT(IsTrue(
			Context->GetTrace().Contains(TEXT("Cleanup"))));
		ASSERT_THAT(IsFalse(
			Context->GetTrace().Contains(TEXT("NeverRuns"))));
	}

	TEST_METHOD(AdvancedCommandRunsBeforeUpdateAfterAndUsesCurrentSuite)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		const FString ScriptSource = ASTEST_AS(R"AS(
			UCLASS()
			class UCountingAdvancedCommand : ULatentAutomationCommand
			{
				UFUNCTION(BlueprintOverride)
				void Before()
				{
					UAdvancedCommandScriptTests Suite =
						Cast<UAdvancedCommandScriptTests>(GetCurrentSuite());
					Suite.AssertNotNull(Suite);
					Suite.AssertEquals(0, Suite.Value);
					Suite.AssertNull(
						FAngelscriptTest::GetTestWorld());
					Suite.Value = 1;
				}

				UFUNCTION(BlueprintOverride)
				bool Update()
				{
					UAdvancedCommandScriptTests Suite =
						Cast<UAdvancedCommandScriptTests>(GetCurrentSuite());
					Suite.Value += 1;
					return Suite.Value == 3;
				}

				UFUNCTION(BlueprintOverride)
				void After()
				{
					UAdvancedCommandScriptTests Suite =
						Cast<UAdvancedCommandScriptTests>(GetCurrentSuite());
					Suite.AssertEquals(3, Suite.Value);
					Suite.Value = 4;
				}

				UFUNCTION(BlueprintOverride)
				FString Describe() const
				{
					return "counting command";
				}
			}

			UCLASS(meta=(AngelscriptTestFlags="EditorContext;EngineFilter"))
			class UAdvancedCommandScriptTests : UAngelscriptTestSuite
			{
				int Value = 0;

				UFUNCTION(meta=(AngelscriptTest))
				void QueuesAdvancedCommand()
				{
					UCountingAdvancedCommand Command =
						Cast<UCountingAdvancedCommand>(
							NewObject(
								this,
								UCountingAdvancedCommand::StaticClass()));
					FAngelscriptTest::Commands()
						.AddLatentCommand(Command, 1.0)
						.Then(n"AfterAdvancedCommand");
				}

				void AfterAdvancedCommand()
				{
					AssertEquals(4, Value);
					Value = 5;
				}

				UFUNCTION(BlueprintOverride)
				void AfterEach()
				{
					AssertEquals(5, Value);
				}
			}
			)AS");

		FScopedAngelscriptModule ModuleScope(
			*TestRunner,
			Engine,
			TEXT("ASTesting_ScriptTestCommands_Advanced"),
			ScriptSource);
		ASSERT_THAT(IsTrue(ModuleScope.IsValid()));
		const FAngelscriptScriptTestRegistryBuildResult Build =
			FAngelscriptScriptTestRegistry::Get().Rebuild(
				Engine.GetActiveModules(),
				true);
		const FAngelscriptScriptTestDescriptor* Descriptor =
			FindOnlyAngelscriptScriptTestDescriptor(Build);
		ASSERT_THAT(IsNotNull(Descriptor));

		const TSharedPtr<FAngelscriptScriptTestExecutionContext> Context =
			FAngelscriptScriptTestRunner::Start(
				Descriptor->Id,
				*TestRunner);
		ASSERT_THAT(IsTrue(Context.IsValid()));
		DrainScriptTestContext(Context);
		ASSERT_THAT(IsTrue(Context->IsComplete()));
		ASSERT_THAT(IsFalse(Context->HasFailed()));
		ASSERT_THAT(IsTrue(
			Context->GetTrace().Contains(
				TEXT("AfterAdvancedCommand"))));
	}

	TEST_METHOD(AdvancedCommandOrdinaryExceptionFailsDetachedRunner)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		const FString ScriptSource = ASTEST_AS(R"AS(
			UCLASS()
			class UThrowingAdvancedCommand : ULatentAutomationCommand
			{
				UFUNCTION(BlueprintOverride)
				void Before()
				{
					throw("advanced Before exception");
				}

				UFUNCTION(BlueprintOverride)
				bool Update()
				{
					return true;
				}
			}

			UCLASS(meta=(AngelscriptTestFlags="EditorContext;EngineFilter"))
			class UThrowingAdvancedCommandScriptTests : UAngelscriptTestSuite
			{
				UFUNCTION(meta=(AngelscriptTest))
				void QueuesThrowingAdvancedCommand()
				{
					UThrowingAdvancedCommand Command =
						Cast<UThrowingAdvancedCommand>(
							NewObject(
								this,
								UThrowingAdvancedCommand::StaticClass()));
					FAngelscriptTest::Commands()
						.AddLatentCommand(Command, 1.0);
				}
			}
			)AS");

		FScopedAngelscriptModule ModuleScope(
			*TestRunner,
			Engine,
			TEXT("ASTesting_ScriptTestCommands_AdvancedException"),
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
				TEXT("FAngelscriptAdvancedCommandExceptionProbe_%p"),
				this));
		const TSharedPtr<FAngelscriptScriptTestExecutionContext> Context =
			FAngelscriptScriptTestRunner::Start(
				Descriptor->Id,
				Probe);
		ASSERT_THAT(IsTrue(Context.IsValid()));
		DrainScriptTestContext(Context);
		ASSERT_THAT(IsTrue(Context->IsComplete()));
		ASSERT_THAT(IsTrue(Context->HasFailed()));

		const TArray<FAutomationExecutionEntry> Entries =
			Probe.GetExecutionEntries();
		ASSERT_THAT(IsNotNull(
			Entries.FindByPredicate(
				[](const FAutomationExecutionEntry& Entry)
				{
					return Entry.Event.Message.Contains(
						TEXT("advanced Before exception"));
				})));
	}

	TEST_METHOD(AllowedClientTimeoutCannotHangInFinishClient)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		const FString ScriptSource = ASTEST_AS(R"AS(
			UCLASS()
			class UAllowedTimeoutClientCommand : ULatentAutomationCommand
			{
				UFUNCTION(BlueprintOverride)
				bool Update()
				{
					return false;
				}
			}

			UCLASS(meta=(AngelscriptTestFlags="EditorContext;EngineFilter"))
			class UAllowedTimeoutClientScriptTests : UAngelscriptTestSuite
			{
				UFUNCTION(meta=(AngelscriptTest))
				void QueuesClientCommand()
				{
					FAngelscriptTest::CreateTestWorld(false);
					FAngelscriptTest::SpawnActor(
						APlayerController::StaticClass());
					UAllowedTimeoutClientCommand Command =
						Cast<UAllowedTimeoutClientCommand>(
							NewObject(
								this,
								UAllowedTimeoutClientCommand::StaticClass()));
					FAngelscriptTest::Commands()
						.AddLatentCommand(Command, 0.01);
				}
			}
			)AS");

		FScopedAngelscriptModule ModuleScope(
			*TestRunner,
			Engine,
			TEXT("ASTesting_ScriptTestCommands_ClientTimeout"),
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
				TEXT("FAngelscriptAllowedClientTimeoutProbe_%p"),
				this));
		const TSharedPtr<FAngelscriptScriptTestExecutionContext> Context =
			FAngelscriptScriptTestRunner::Start(
				Descriptor->Id,
				Probe,
				false);
		ASSERT_THAT(IsTrue(Context.IsValid()));
		ASSERT_THAT(IsFalse(Context->IsComplete()));

		ULatentAutomationCommand* Command = nullptr;
		for (TObjectIterator<ULatentAutomationCommand> It; It; ++It)
		{
			if (!It->HasAnyFlags(RF_ClassDefaultObject)
				&& It->GetClass()->GetName().Contains(
					TEXT("AllowedTimeoutClientCommand")))
			{
				ASSERT_THAT(IsNull(Command));
				Command = *It;
			}
		}
		ASSERT_THAT(IsNotNull(Command));

		FBoolProperty* AllowsTimeout =
			FindFProperty<FBoolProperty>(
				ULatentAutomationCommand::StaticClass(),
				TEXT("bAllowTimeout"));
		FBoolProperty* RunsOnClient =
			FindFProperty<FBoolProperty>(
				ULatentAutomationCommand::StaticClass(),
				TEXT("bAlsoRunOnClient"));
		ASSERT_THAT(IsNotNull(AllowsTimeout));
		ASSERT_THAT(IsNotNull(RunsOnClient));
		AllowsTimeout->SetPropertyValue_InContainer(Command, true);
		RunsOnClient->SetPropertyValue_InContainer(Command, true);

		// Start the command, create its executor, and enter SetupClient while
		// the reflected command is still associated with this leaf.
		Context->Update();
		Context->Update();
		Context->Update();
		ALatentAutomationCommandClientExecutor* Executor = nullptr;
		for (TObjectIterator<ALatentAutomationCommandClientExecutor> It;
			It;
			++It)
		{
			if (!It->HasAnyFlags(RF_ClassDefaultObject)
				&& It->GetWorld() == Context->GetWorld())
			{
				ASSERT_THAT(IsNull(Executor));
				Executor = *It;
			}
		}
		ASSERT_THAT(IsNotNull(Executor));

		FPlatformProcess::Sleep(0.02f);
		DrainScriptTestContext(Context, 16);
		ASSERT_THAT(IsTrue(Context->IsComplete()));
		ASSERT_THAT(IsFalse(Context->HasFailed()));
		ASSERT_THAT(IsFalse(IsValid(Executor)));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
