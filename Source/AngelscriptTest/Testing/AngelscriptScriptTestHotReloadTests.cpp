#include "CQTest.h"
#include "AngelscriptTestEngineHelper.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestModuleScope.h"
#include "AngelscriptScriptTestTestHelpers.h"

#include "HotReload/AngelscriptScriptTestAutomationRefresh.h"
#include "Core/AngelscriptEngineExtensionRegistry.h"
#include "Engine/Engine.h"
#include "Misc/ScopeExit.h"
#include "Testing/AngelscriptScriptTestAutomation.h"
#include "Testing/AngelscriptScriptTestHotReloadRunner.h"
#include "Testing/AngelscriptScriptTestRegistry.h"
#include "Testing/AngelscriptScriptTestRunner.h"
#include "UObject/GarbageCollection.h"
#include "UObject/UObjectIterator.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FAngelscriptScriptTestHotReloadTests,
	"Angelscript.TestModule.Testing.ScriptTestFramework.HotReload",
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

	TEST_METHOD(EditorRefreshIsImmediateWhenControllerIsIdle)
	{
		bool bLoaded = true;
		bool bRunning = false;
		int32 RequestCount = 0;
		FAngelscriptScriptTestAutomationRefresh Refresh(
			{
				[&bLoaded]() { return bLoaded; },
				[&bRunning]() { return bRunning; },
				[&RequestCount]() { ++RequestCount; },
			});

		Refresh.NotifyRegistryChanged(2);
		ASSERT_THAT(IsTrue(Refresh.HasPendingRefresh()));
		ASSERT_THAT(IsTrue(Refresh.Tick(0.0f)));
		ASSERT_THAT(AreEqual(1, RequestCount));
		ASSERT_THAT(IsFalse(Refresh.HasPendingRefresh()));
	}

	TEST_METHOD(SynchronousCommandletRunnerRejectsEmptyEligibleRegistry)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		const FString ScriptSource = ASTEST_AS(R"AS(
			UCLASS(meta=(AngelscriptTestFlags="EditorContext;EngineFilter"))
			class UEditorOnlyDirectRunnerScriptTests
				: UAngelscriptTestSuite
			{
				UFUNCTION(meta=(AngelscriptTest))
					void EditorOnlyLeaf()
					{
					}
			}
			)AS");
		FScopedAngelscriptModule ModuleScope(
			*TestRunner,
			Engine,
			TEXT("ASTesting_ScriptTestCommandlet_Empty"),
			ScriptSource);
		ASSERT_THAT(IsTrue(ModuleScope.IsValid()));
		const FAngelscriptScriptTestRegistryBuildResult Build =
			FAngelscriptScriptTestRegistry::Get().Rebuild(
				Engine.GetActiveModules(),
				true);
		ASSERT_THAT(IsTrue(Build.Snapshot.IsValid()));
		ASSERT_THAT(AreEqual(1, Build.Snapshot->Tests.Num()));

		FAngelscriptScriptTestHotReloadRunner DirectRunner;
		FAngelscriptScriptTestRunSummary Summary;
		ASSERT_THAT(IsFalse(
			DirectRunner.RunCurrentRegistrySynchronously(
				Engine,
				&Summary)));
		ASSERT_THAT(AreEqual(0, Summary.Selected));
		ASSERT_THAT(AreEqual(0, Summary.Executed));
		ASSERT_THAT(AreEqual(0, Summary.Passed));
		ASSERT_THAT(AreEqual(0, Summary.Failed));
	}

	TEST_METHOD(SynchronousCommandletRunnerReportsNonemptyCounts)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		const FString ScriptSource = ASTEST_AS(R"AS(
			UCLASS(meta=(AngelscriptTestFlags="CommandletContext;EngineFilter"))
			class UCommandletDirectRunnerScriptTests
				: UAngelscriptTestSuite
			{
				UFUNCTION(meta=(AngelscriptTest))
					void CommandletLeaf()
					{
						AssertTrue(true);
					}
			}
			)AS");
		FScopedAngelscriptModule ModuleScope(
			*TestRunner,
			Engine,
			TEXT("ASTesting_ScriptTestCommandlet_Nonempty"),
			ScriptSource);
		ASSERT_THAT(IsTrue(ModuleScope.IsValid()));
		const FAngelscriptScriptTestRegistryBuildResult Build =
			FAngelscriptScriptTestRegistry::Get().Rebuild(
				Engine.GetActiveModules(),
				true);
		ASSERT_THAT(IsTrue(Build.Snapshot.IsValid()));
		ASSERT_THAT(AreEqual(1, Build.Snapshot->Tests.Num()));

		FAngelscriptScriptTestHotReloadRunner DirectRunner;
		FAngelscriptScriptTestRunSummary Summary;
		ASSERT_THAT(IsTrue(
			DirectRunner.RunCurrentRegistrySynchronously(
				Engine,
				&Summary)));
		ASSERT_THAT(AreEqual(1, Summary.Selected));
		ASSERT_THAT(AreEqual(1, Summary.Executed));
		ASSERT_THAT(AreEqual(1, Summary.Passed));
		ASSERT_THAT(AreEqual(0, Summary.Failed));
	}

	TEST_METHOD(SynchronousCommandletRunnerHonorsExpectedErrors)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		const FString ScriptSource = ASTEST_AS(R"AS(
			UCLASS(meta=(AngelscriptTestFlags="CommandletContext;EngineFilter"))
			class UCommandletExpectedErrorScriptTests
				: UAngelscriptTestSuite
			{
				UFUNCTION(meta=(AngelscriptTest))
					void ExpectedLogDoesNotFailTheLeaf()
					{
						ExpectError("intentional commandlet expected log", 1);
						Error("intentional commandlet expected log");
					}
			}
			)AS");
		FScopedAngelscriptModule ModuleScope(
			*TestRunner,
			Engine,
			TEXT("ASTesting_ScriptTestCommandlet_Expected"),
			ScriptSource);
		ASSERT_THAT(IsTrue(ModuleScope.IsValid()));
		const FAngelscriptScriptTestRegistryBuildResult Build =
			FAngelscriptScriptTestRegistry::Get().Rebuild(
				Engine.GetActiveModules(),
				true);
		ASSERT_THAT(IsTrue(Build.Snapshot.IsValid()));
		ASSERT_THAT(AreEqual(1, Build.Snapshot->Tests.Num()));

		// The nested detached result owns the script-test expectation. The
		// enclosing CQTest also observes the original UE log, so consume it
		// here without changing the detached result's accounting.
		TestRunner->AddExpectedError(
			TEXT("intentional commandlet expected log"),
			EAutomationExpectedErrorFlags::Contains,
			1);
		FAngelscriptScriptTestHotReloadRunner DirectRunner;
		FAngelscriptScriptTestRunSummary Summary;
		ASSERT_THAT(IsTrue(
			DirectRunner.RunCurrentRegistrySynchronously(
				Engine,
				&Summary)));
		ASSERT_THAT(AreEqual(1, Summary.Selected));
		ASSERT_THAT(AreEqual(1, Summary.Executed));
		ASSERT_THAT(AreEqual(1, Summary.Passed));
		ASSERT_THAT(AreEqual(0, Summary.Failed));
	}

	TEST_METHOD(SynchronousCommandletRunnerCountsAfterAllFailure)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		const FString ScriptSource = ASTEST_AS(R"AS(
			UCLASS(meta=(AngelscriptTestFlags="CommandletContext;EngineFilter"))
			class UCommandletAfterAllFailureScriptTests
				: UAngelscriptTestSuite
			{
				UFUNCTION(meta=(AngelscriptTest))
					void PassingLeaf()
					{
					}

				UFUNCTION(BlueprintOverride)
					void AfterAll()
					{
						throw("intentional commandlet AfterAll failure");
					}
			}
			)AS");
		FScopedAngelscriptModule ModuleScope(
			*TestRunner,
			Engine,
			TEXT("ASTesting_ScriptTestCommandlet_AfterAll"),
			ScriptSource);
		ASSERT_THAT(IsTrue(ModuleScope.IsValid()));
		const FAngelscriptScriptTestRegistryBuildResult Build =
			FAngelscriptScriptTestRegistry::Get().Rebuild(
				Engine.GetActiveModules(),
				true);
		ASSERT_THAT(IsTrue(Build.Snapshot.IsValid()));
		ASSERT_THAT(AreEqual(1, Build.Snapshot->Tests.Num()));

		TestRunner->AddExpectedError(
			TEXT("ASTesting_ScriptTestCommandlet_AfterAll_.*\\.as:"
				"[0-9]+: AfterAll threw an AngelScript exception: "
				"intentional commandlet AfterAll failure"),
			EAutomationExpectedErrorFlags::Contains,
			1,
			true);
		FAngelscriptScriptTestHotReloadRunner DirectRunner;
		FAngelscriptScriptTestRunSummary Summary;
		ASSERT_THAT(IsFalse(
			DirectRunner.RunCurrentRegistrySynchronously(
				Engine,
				&Summary)));
		ASSERT_THAT(AreEqual(1, Summary.Selected));
		ASSERT_THAT(AreEqual(1, Summary.Executed));
		ASSERT_THAT(AreEqual(0, Summary.Passed));
		ASSERT_THAT(AreEqual(1, Summary.Failed));
	}

	TEST_METHOD(SynchronousCommandletRunnerKeepsUnstartedLeafOutOfFailedCount)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		const FString ScriptSource = ASTEST_AS(R"AS(
			UCLASS(meta=(AngelscriptTestFlags="CommandletContext;EngineFilter"))
			class UCommandletBeforeAllFailureScriptTests
				: UAngelscriptTestSuite
			{
				UFUNCTION(BlueprintOverride)
					void BeforeAll()
					{
						throw("intentional commandlet BeforeAll failure");
					}

				UFUNCTION(meta=(AngelscriptTest))
					void NeverStartedLeaf()
					{
					}
			}
			)AS");
		FScopedAngelscriptModule ModuleScope(
			*TestRunner,
			Engine,
			TEXT("ASTesting_ScriptTestCommandlet_BeforeAll"),
			ScriptSource);
		ASSERT_THAT(IsTrue(ModuleScope.IsValid()));
		const FAngelscriptScriptTestRegistryBuildResult Build =
			FAngelscriptScriptTestRegistry::Get().Rebuild(
				Engine.GetActiveModules(),
				true);
		ASSERT_THAT(IsTrue(Build.Snapshot.IsValid()));
		ASSERT_THAT(AreEqual(1, Build.Snapshot->Tests.Num()));

		TestRunner->AddExpectedError(
			TEXT("ASTesting_ScriptTestCommandlet_BeforeAll_.*\\.as:"
				"[0-9]+: BeforeAll threw an AngelScript exception: "
				"intentional commandlet BeforeAll failure"),
			EAutomationExpectedErrorFlags::Contains,
			1,
			true);
		FAngelscriptScriptTestHotReloadRunner DirectRunner;
		FAngelscriptScriptTestRunSummary Summary;
		ASSERT_THAT(IsFalse(
			DirectRunner.RunCurrentRegistrySynchronously(
				Engine,
				&Summary)));
		ASSERT_THAT(AreEqual(1, Summary.Selected));
		ASSERT_THAT(AreEqual(0, Summary.Executed));
		ASSERT_THAT(AreEqual(0, Summary.Passed));
		ASSERT_THAT(AreEqual(0, Summary.Failed));
	}

	TEST_METHOD(SynchronousCommandletRunnerSurfacesSourceLocatedFailure)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		const FString ScriptSource = ASTEST_AS(R"AS(
			UCLASS(meta=(AngelscriptTestFlags="CommandletContext;EngineFilter"))
			class UCommandletFailureScriptTests : UAngelscriptTestSuite
			{
				UFUNCTION(meta=(AngelscriptTest))
					void FailingLeaf()
					{
						Fail("intentional commandlet runner failure");
					}
			}
			)AS");

		FScopedAngelscriptModule ModuleScope(
			*TestRunner,
			Engine,
			TEXT("ASTesting_ScriptTestCommandlet_Failure"),
			ScriptSource);
		ASSERT_THAT(IsTrue(ModuleScope.IsValid()));
		const FAngelscriptScriptTestRegistryBuildResult Build =
			FAngelscriptScriptTestRegistry::Get().Rebuild(
				Engine.GetActiveModules(),
				true);
		const FAngelscriptScriptTestDescriptor* Descriptor =
			FindOnlyAngelscriptScriptTestDescriptor(Build);
		ASSERT_THAT(IsNotNull(Descriptor));

		TestRunner->AddExpectedError(
			TEXT("ASTesting_ScriptTestCommandlet_Failure_.*\\.as:"
				"[0-9]+: intentional commandlet runner failure"),
			EAutomationExpectedErrorFlags::Contains,
			1,
			true);
		FAngelscriptScriptTestHotReloadRunner DirectRunner;
		FAngelscriptScriptTestRunSummary Summary;
		ASSERT_THAT(IsFalse(
			DirectRunner.RunCurrentRegistrySynchronously(
				Engine,
				&Summary)));
		ASSERT_THAT(AreEqual(1, Summary.Selected));
		ASSERT_THAT(AreEqual(1, Summary.Executed));
		ASSERT_THAT(AreEqual(0, Summary.Passed));
		ASSERT_THAT(AreEqual(1, Summary.Failed));
	}

	TEST_METHOD(EditorRefreshDefersAndCoalescesWhileTestsRun)
	{
		bool bLoaded = true;
		bool bRunning = true;
		int32 RequestCount = 0;
		FAngelscriptScriptTestAutomationRefresh Refresh(
			{
				[&bLoaded]() { return bLoaded; },
				[&bRunning]() { return bRunning; },
				[&RequestCount]() { ++RequestCount; },
			});

		Refresh.NotifyRegistryChanged(3);
		Refresh.NotifyRegistryChanged(4);
		Refresh.NotifyRegistryChanged(5);
		Refresh.Tick(0.0f);
		ASSERT_THAT(AreEqual(0, RequestCount));
		ASSERT_THAT(IsTrue(Refresh.HasPendingRefresh()));

		bRunning = false;
		Refresh.Tick(0.0f);
		ASSERT_THAT(AreEqual(1, RequestCount));
		ASSERT_THAT(IsFalse(Refresh.HasPendingRefresh()));
	}

	TEST_METHOD(EditorRefreshNeverLoadsAnUnopenedController)
	{
		bool bLoaded = false;
		int32 RunningQueryCount = 0;
		int32 RequestCount = 0;
		FAngelscriptScriptTestAutomationRefresh Refresh(
			{
				[&bLoaded]() { return bLoaded; },
				[&RunningQueryCount]()
				{
					++RunningQueryCount;
					return false;
				},
				[&RequestCount]() { ++RequestCount; },
			});

		Refresh.NotifyRegistryChanged(6);
		Refresh.Tick(0.0f);
		ASSERT_THAT(AreEqual(0, RunningQueryCount));
		ASSERT_THAT(AreEqual(0, RequestCount));
		ASSERT_THAT(IsFalse(Refresh.HasPendingRefresh()));
	}

	TEST_METHOD(CallbackScopeExposesTheSafeReloadBoundary)
	{
		ASSERT_THAT(IsFalse(
			FAngelscriptScriptTestRunner::IsExecutingScriptCallback()));
		{
			FAngelscriptScriptTestCallbackScope OuterScope;
			ASSERT_THAT(IsTrue(
				FAngelscriptScriptTestRunner::
					IsExecutingScriptCallback()));
			{
				FAngelscriptScriptTestCallbackScope NestedScope;
				ASSERT_THAT(IsTrue(
					FAngelscriptScriptTestRunner::
						IsExecutingScriptCallback()));
			}
			ASSERT_THAT(IsTrue(
				FAngelscriptScriptTestRunner::
					IsExecutingScriptCallback()));
		}
		ASSERT_THAT(IsFalse(
			FAngelscriptScriptTestRunner::IsExecutingScriptCallback()));
	}

	TEST_METHOD(AutomaticSchedulerIsAsyncAndCleansBeforeReload)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		const FString ScriptSource = ASTEST_AS(R"AS(
			UCLASS(meta=(AngelscriptTestFlags="EditorContext;EngineFilter"))
			class UAutomaticReloadScriptTests : UAngelscriptTestSuite
			{
				UFUNCTION(meta=(AngelscriptTest))
					void WaitsWithoutBlockingTheCompilerThread()
					{
						FAngelscriptTest::CreateTestWorld(false);
						FAngelscriptTest::Commands()
							.WaitDelay(5.0);
					}
			}
			)AS");
		FScopedAngelscriptModule ModuleScope(
			*TestRunner,
			Engine,
			TEXT("ASTesting_ScriptTestHotReload_Scheduler"),
			ScriptSource);
		ASSERT_THAT(IsTrue(ModuleScope.IsValid()));
		const FAngelscriptScriptTestRegistryBuildResult Build =
			FAngelscriptScriptTestRegistry::Get().Rebuild(
				Engine.GetActiveModules(),
				true);
		const FAngelscriptScriptTestDescriptor* Descriptor =
			FindOnlyAngelscriptScriptTestDescriptor(Build);
		ASSERT_THAT(IsNotNull(Descriptor));
		const TSharedPtr<FAngelscriptModuleDesc> Module =
			Engine.GetModule(Descriptor->Id.ModuleName);
		ASSERT_THAT(IsTrue(Module.IsValid()));

		const int32 WorldContextsBefore =
			GEngine->GetWorldContexts().Num();
		FAngelscriptScriptTestHotReloadRunner Scheduler;
		TArray<TSharedRef<FAngelscriptModuleDesc>> CompiledModules;
		CompiledModules.Add(Module.ToSharedRef());
		Scheduler.PrepareTests(CompiledModules);
		ASSERT_THAT(AreEqual(
			1,
			Scheduler.GetPendingCountForTesting()));

		ASSERT_THAT(IsTrue(Scheduler.RunTests(&Engine)));
		ASSERT_THAT(IsTrue(Scheduler.HasWork()));
		ASSERT_THAT(AreEqual(
			WorldContextsBefore + 1,
			GEngine->GetWorldContexts().Num()));

		TSet<FString> AffectedModules;
		AffectedModules.Add(Descriptor->Id.ModuleName);
		Scheduler.CancelModulesBeforeReload(AffectedModules);
		ASSERT_THAT(IsFalse(Scheduler.HasWork()));
		ASSERT_THAT(AreEqual(
			WorldContextsBefore,
			GEngine->GetWorldContexts().Num()));
	}

	TEST_METHOD(AutomaticSchedulerReportsCompletedFailureOnlyOnce)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		const FString ScriptSource = ASTEST_AS(R"AS(
			UCLASS(meta=(AngelscriptTestFlags="EditorContext;EngineFilter"))
			class UAutomaticFailureScriptTests : UAngelscriptTestSuite
			{
				UFUNCTION(meta=(AngelscriptTest))
					void FailingLeaf()
					{
						Fail("intentional automatic scheduler failure");
					}
			}
			)AS");
		FScopedAngelscriptModule ModuleScope(
			*TestRunner,
			Engine,
			TEXT("ASTesting_ScriptTestHotReload_StickyFailure"),
			ScriptSource);
		ASSERT_THAT(IsTrue(ModuleScope.IsValid()));
		const FAngelscriptScriptTestRegistryBuildResult Build =
			FAngelscriptScriptTestRegistry::Get().Rebuild(
				Engine.GetActiveModules(),
				true);
		const FAngelscriptScriptTestDescriptor* Descriptor =
			FindOnlyAngelscriptScriptTestDescriptor(Build);
		ASSERT_THAT(IsNotNull(Descriptor));
		const TSharedPtr<FAngelscriptModuleDesc> Module =
			Engine.GetModule(Descriptor->Id.ModuleName);
		ASSERT_THAT(IsTrue(Module.IsValid()));

		FAngelscriptScriptTestHotReloadRunner Scheduler;
		TArray<TSharedRef<FAngelscriptModuleDesc>> CompiledModules;
		CompiledModules.Add(Module.ToSharedRef());
		Scheduler.PrepareTests(CompiledModules);
		TestRunner->AddExpectedError(
			TEXT("ASTesting_ScriptTestHotReload_StickyFailure_.*\\.as:"
				"[0-9]+: intentional automatic scheduler failure"),
			EAutomationExpectedErrorFlags::Contains,
			1,
			true);

		int32 FailedPolls = 0;
		int32 Polls = 0;
		do
		{
			if (!Scheduler.RunTests(&Engine))
			{
				++FailedPolls;
			}
			++Polls;
		}
		while (Scheduler.HasWork() && Polls < 16);
		ASSERT_THAT(IsFalse(Scheduler.HasWork()));
		ASSERT_THAT(IsTrue(Polls < 16));

		for (int32 IdlePoll = 0; IdlePoll < 3; ++IdlePoll)
		{
			if (!Scheduler.RunTests(&Engine))
			{
				++FailedPolls;
			}
		}
		ASSERT_THAT(AreEqual(1, FailedPolls));
	}

	TEST_METHOD(AutomaticSchedulerHonorsExpectedErrors)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		const FString ScriptSource = ASTEST_AS(R"AS(
			UCLASS(meta=(AngelscriptTestFlags="EditorContext;EngineFilter"))
			class UAutomaticExpectedErrorScriptTests
				: UAngelscriptTestSuite
			{
				UFUNCTION(meta=(AngelscriptTest))
					void ExpectedLogDoesNotFailTheRun()
					{
						ExpectError("intentional automatic expected log", 1);
						Error("intentional automatic expected log");
					}
			}
			)AS");
		FScopedAngelscriptModule ModuleScope(
			*TestRunner,
			Engine,
			TEXT("ASTesting_ScriptTestHotReload_Expected"),
			ScriptSource);
		ASSERT_THAT(IsTrue(ModuleScope.IsValid()));
		const FAngelscriptScriptTestRegistryBuildResult Build =
			FAngelscriptScriptTestRegistry::Get().Rebuild(
				Engine.GetActiveModules(),
				true);
		const FAngelscriptScriptTestDescriptor* Descriptor =
			FindOnlyAngelscriptScriptTestDescriptor(Build);
		ASSERT_THAT(IsNotNull(Descriptor));
		const TSharedPtr<FAngelscriptModuleDesc> Module =
			Engine.GetModule(Descriptor->Id.ModuleName);
		ASSERT_THAT(IsTrue(Module.IsValid()));

		TestRunner->AddExpectedError(
			TEXT("intentional automatic expected log"),
			EAutomationExpectedErrorFlags::Contains,
			1);
		FAngelscriptScriptTestHotReloadRunner Scheduler;
		TArray<TSharedRef<FAngelscriptModuleDesc>> CompiledModules;
		CompiledModules.Add(Module.ToSharedRef());
		Scheduler.PrepareTests(CompiledModules);

		bool bPassed = true;
		int32 Polls = 0;
		do
		{
			bPassed &= Scheduler.RunTests(&Engine);
			++Polls;
		}
		while (Scheduler.HasWork() && Polls < 16);
		ASSERT_THAT(IsFalse(Scheduler.HasWork()));
		ASSERT_THAT(IsTrue(Polls < 16));
		ASSERT_THAT(IsTrue(bPassed));
	}

	TEST_METHOD(SuccessfulReloadUsesCurrentBodyMarkerNameAndFlags)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		const FName ModuleName(TEXT("ScriptTestHotReloadCurrent"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		const FString InitialSource = ASTEST_AS(R"AS(
			UCLASS(meta=(AngelscriptTestFlags="EditorContext;EngineFilter"))
			class UReloadedScriptTests : UAngelscriptTestSuite
			{
				UFUNCTION(meta=(AngelscriptTest))
					void OldName()
					{
						AssertEquals(1, BodyValue());
					}

				int BodyValue()
				{
					return 1;
				}
			}
			)AS");
		ASSERT_THAT(IsTrue(CompileAnnotatedModuleFromMemory(
			&Engine,
			ModuleName,
			TEXT("ScriptTestHotReloadCurrent.as"),
			InitialSource)));
		const FAngelscriptScriptTestRegistryBuildResult InitialBuild =
			FAngelscriptScriptTestRegistry::Get().Rebuild(
				Engine.GetActiveModules(),
				true);
		const FAngelscriptScriptTestDescriptor* InitialDescriptor =
			InitialBuild.Snapshot->Tests.FindByPredicate(
				[&ModuleName](
					const FAngelscriptScriptTestDescriptor& Candidate)
				{
					return Candidate.Id.ModuleName
							== ModuleName.ToString()
						&& Candidate.Id.SuiteName
							== TEXT("UReloadedScriptTests")
						&& Candidate.Id.MethodName
							== TEXT("OldName");
				});
		ASSERT_THAT(IsNotNull(InitialDescriptor));
		const uint64 InitialGeneration =
			InitialBuild.Snapshot->Generation;

		const FString ReloadedSource = ASTEST_AS(R"AS(
			UCLASS(meta=(AngelscriptTestFlags="ClientContext;ProductFilter"))
			class UReloadedScriptTests : UAngelscriptTestSuite
			{
				void OldName()
				{
					Fail("removed marker was still discovered");
				}

				UFUNCTION(meta=(AngelscriptTest))
					void NewName()
					{
						AssertEquals(2, BodyValue());
					}

				int BodyValue()
				{
					return 2;
				}
			}
			)AS");
		ECompileResult ReloadResult = ECompileResult::Error;
		ASSERT_THAT(IsTrue(CompileModuleWithResult(
			&Engine,
			ECompileType::FullReload,
			ModuleName,
			TEXT("ScriptTestHotReloadCurrent.as"),
			ReloadedSource,
			ReloadResult)));
		ASSERT_THAT(IsTrue(
			ReloadResult == ECompileResult::FullyHandled
			|| ReloadResult == ECompileResult::PartiallyHandled));

		const FAngelscriptScriptTestRegistryBuildResult ReloadedBuild =
			FAngelscriptScriptTestRegistry::Get().Rebuild(
				Engine.GetActiveModules(),
				true);
		const FAngelscriptScriptTestDescriptor* ReloadedDescriptor =
			ReloadedBuild.Snapshot->Tests.FindByPredicate(
				[&ModuleName](
					const FAngelscriptScriptTestDescriptor& Candidate)
				{
					return Candidate.Id.ModuleName
							== ModuleName.ToString()
						&& Candidate.Id.SuiteName
							== TEXT("UReloadedScriptTests")
						&& Candidate.Id.MethodName
							== TEXT("NewName");
				});
		ASSERT_THAT(IsNotNull(ReloadedDescriptor));
		ASSERT_THAT(IsTrue(
			ReloadedBuild.Snapshot->Generation > InitialGeneration));
		ASSERT_THAT(AreEqual(
			FString(TEXT("NewName")),
			ReloadedDescriptor->Id.MethodName));
		ASSERT_THAT(AreEqual(
			EAutomationTestFlags::ClientContext
				| EAutomationTestFlags::ProductFilter,
			ReloadedDescriptor->Flags));

		FAngelscriptScriptTestProbe Probe(
			FString::Printf(
				TEXT("FAngelscriptScriptTestReloadBodyProbe_%p"),
				this));
		const TSharedPtr<FAngelscriptScriptTestExecutionContext> Context =
			FAngelscriptScriptTestRunner::Start(
				ReloadedDescriptor->Id,
				Probe,
				false);
		ASSERT_THAT(IsTrue(Context.IsValid()));
		ASSERT_THAT(IsTrue(Context->IsComplete()));
		ASSERT_THAT(IsFalse(Context->HasFailed()));
	}

	TEST_METHOD(ActiveAutomationSectionReopensAllHooksAfterReload)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		const FName ModuleName(TEXT("ScriptTestHotReloadAllHooks"));
		FAngelscriptScriptTestAutomation& Automation =
			FAngelscriptScriptTestAutomation::Get();
		FString Section;
		ON_SCOPE_EXIT
		{
			if (!Section.IsEmpty())
			{
				Automation.LeaveSectionForTesting(Section);
			}
			Engine.DiscardModule(*ModuleName.ToString());
		};

		const FString InitialSource = ASTEST_AS(R"AS(
			UCLASS(meta=(AngelscriptTestFlags="EditorContext;EngineFilter"))
			class UReloadedAllHookScriptTests : UAngelscriptTestSuite
			{
				UFUNCTION(BlueprintOverride)
				void BeforeAll()
				{
				}

				UFUNCTION(meta=(AngelscriptTest))
				void Leaf()
				{
				}
			}
			)AS");
		ASSERT_THAT(IsTrue(CompileAnnotatedModuleFromMemory(
			&Engine,
			ModuleName,
			TEXT("ScriptTestHotReloadAllHooks.as"),
			InitialSource)));
		const FAngelscriptScriptTestRegistryBuildResult InitialBuild =
			FAngelscriptScriptTestRegistry::Get().Rebuild(
				Engine.GetActiveModules(),
				true);
		const FAngelscriptScriptTestDescriptor* InitialDescriptor =
			InitialBuild.Snapshot->Tests.FindByPredicate(
				[&ModuleName](
					const FAngelscriptScriptTestDescriptor& Candidate)
				{
					return Candidate.Id.ModuleName
							== ModuleName.ToString()
						&& Candidate.Id.SuiteName
							== TEXT("UReloadedAllHookScriptTests")
						&& Candidate.Id.MethodName == TEXT("Leaf");
				});
		ASSERT_THAT(IsNotNull(InitialDescriptor));

		Section = FString::Printf(
			TEXT("Angelscript.ScriptTests.%s.%s"),
			*InitialDescriptor->Id.ModuleName,
			*InitialDescriptor->Id.SuiteName);
		Automation.EnterSectionForTesting(Section);
		ASSERT_THAT(IsTrue(Automation.HasActiveSessionForTesting()));
		ASSERT_THAT(AreEqual(
			InitialBuild.Snapshot->Generation,
			Automation.GetActiveSessionGenerationForTesting()));

		const FString ReloadedSource = ASTEST_AS(R"AS(
			UCLASS(meta=(AngelscriptTestFlags="EditorContext;EngineFilter"))
			class UReloadedAllHookScriptTests : UAngelscriptTestSuite
			{
				UFUNCTION(BlueprintOverride)
				void BeforeAll()
				{
				}

				UFUNCTION(meta=(AngelscriptTest))
				void Leaf()
				{
				}
			}
			)AS");
		ECompileResult ReloadResult = ECompileResult::Error;
		ASSERT_THAT(IsTrue(CompileModuleWithResult(
			&Engine,
			ECompileType::FullReload,
			ModuleName,
			TEXT("ScriptTestHotReloadAllHooks.as"),
			ReloadedSource,
			ReloadResult)));
		ASSERT_THAT(IsTrue(
			ReloadResult == ECompileResult::FullyHandled
			|| ReloadResult == ECompileResult::PartiallyHandled));

		const FAngelscriptScriptTestRegistryBuildResult ReloadedBuild =
			FAngelscriptScriptTestRegistry::Get().Rebuild(
				Engine.GetActiveModules(),
				true);
		const FAngelscriptScriptTestDescriptor* ReloadedDescriptor =
			ReloadedBuild.Snapshot->Tests.FindByPredicate(
				[&ModuleName](
					const FAngelscriptScriptTestDescriptor& Candidate)
				{
					return Candidate.Id.ModuleName
							== ModuleName.ToString()
						&& Candidate.Id.SuiteName
							== TEXT("UReloadedAllHookScriptTests")
						&& Candidate.Id.MethodName == TEXT("Leaf");
				});
		ASSERT_THAT(IsNotNull(ReloadedDescriptor));

		// UE can keep running leaves under the same cached section without a
		// second enter notification. The bridge must lazily open the current
		// generation before the next leaf.
		TArray<FAutomationExecutionEntry> Entries;
		ASSERT_THAT(IsTrue(Automation.ExecuteBridgeCommandForTesting(
			ReloadedDescriptor->Flags,
			ReloadedDescriptor->Id.ToCommandString(
				ReloadedDescriptor->Generation),
			Entries)));
		ASSERT_THAT(AreEqual(0, Entries.Num()));
		ASSERT_THAT(IsTrue(Automation.HasActiveSessionForTesting()));
		ASSERT_THAT(AreEqual(
			ReloadedBuild.Snapshot->Generation,
			Automation.GetActiveSessionGenerationForTesting()));
	}

	TEST_METHOD(FailedCompileRetainsLastGoodRegistryGeneration)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		const FName ModuleName(TEXT("ScriptTestHotReloadLastGood"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		const FString GoodSource = ASTEST_AS(R"AS(
			UCLASS(meta=(AngelscriptTestFlags="EditorContext;EngineFilter"))
			class ULastGoodScriptTests : UAngelscriptTestSuite
			{
				UFUNCTION(meta=(AngelscriptTest))
					void StillAvailable()
					{
					}
			}
			)AS");
		ASSERT_THAT(IsTrue(CompileAnnotatedModuleFromMemory(
			&Engine,
			ModuleName,
			TEXT("ScriptTestHotReloadLastGood.as"),
			GoodSource)));
		const FAngelscriptScriptTestRegistryBuildResult GoodBuild =
			FAngelscriptScriptTestRegistry::Get().Rebuild(
				Engine.GetActiveModules(),
				true);
		ASSERT_THAT(AreEqual(1, GoodBuild.Snapshot->Tests.Num()));
		const uint64 LastGoodGeneration =
			GoodBuild.Snapshot->Generation;

		TestRunner->AddExpectedError(
			TEXT("ScriptTestHotReloadLastGood.as:"),
			EAutomationExpectedErrorFlags::Contains,
			2);
		TestRunner->AddExpectedError(
			TEXT("MissingReloadType"),
			EAutomationExpectedErrorFlags::Contains,
			2);
		TestRunner->AddExpectedError(
			TEXT("Hot reload failed due to script compile errors"),
			EAutomationExpectedErrorFlags::Contains,
			1);
		const FString BrokenSource = ASTEST_AS(R"AS(
			UCLASS(meta=(AngelscriptTestFlags="EditorContext;EngineFilter"))
			class ULastGoodScriptTests : UAngelscriptTestSuite
			{
				UFUNCTION(meta=(AngelscriptTest))
					MissingReloadType Broken()
					{
						MissingReloadType Value;
						return Value;
					}
			}
			)AS");
		ECompileResult BrokenResult = ECompileResult::FullyHandled;
		ASSERT_THAT(IsFalse(CompileModuleWithResult(
			&Engine,
			ECompileType::SoftReloadOnly,
			ModuleName,
			TEXT("ScriptTestHotReloadLastGood.as"),
			BrokenSource,
			BrokenResult)));
		ASSERT_THAT(IsTrue(
			BrokenResult == ECompileResult::Error
			|| BrokenResult == ECompileResult::ErrorNeedFullReload));

		const TSharedPtr<const FAngelscriptScriptTestRegistrySnapshot>
			AfterFailure =
				FAngelscriptScriptTestRegistry::Get().GetSnapshot();
		ASSERT_THAT(IsTrue(AfterFailure.IsValid()));
		ASSERT_THAT(AreEqual(
			LastGoodGeneration,
			AfterFailure->Generation));
		ASSERT_THAT(AreEqual(1, AfterFailure->Tests.Num()));
		ASSERT_THAT(AreEqual(
			FString(TEXT("StillAvailable")),
			AfterFailure->Tests[0].Id.MethodName));
	}

	TEST_METHOD(CancelAffectedLatentLeafCleansWithOldGeneration)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		const FString ScriptSource = ASTEST_AS(R"AS(
			UCLASS(meta=(AngelscriptTestFlags="EditorContext;EngineFilter"))
			class UCancelOnReloadScriptTests : UAngelscriptTestSuite
			{
				int Value = 0;

				UFUNCTION(meta=(AngelscriptTest))
					void WaitingLeaf()
					{
						Value = 1;
						FAngelscriptTest::CreateTestWorld();
						FAngelscriptTest::Commands()
							.OnCleanup(n"CleanupOne")
							.OnCleanup(n"CleanupTwo")
							.WaitDelay(5.0);
					}

				UFUNCTION(BlueprintOverride)
					void AfterEach()
					{
						AssertEquals(1, Value);
						Value = 2;
					}

				void CleanupOne()
					{
						AssertEquals(3, Value);
						Value = 4;
					}

				void CleanupTwo()
					{
						AssertEquals(2, Value);
						Value = 3;
					}
			}
			)AS");
		FScopedAngelscriptModule ModuleScope(
			*TestRunner,
			Engine,
			TEXT("ASTesting_ScriptTestHotReload_Cancel"),
			ScriptSource);
		ASSERT_THAT(IsTrue(ModuleScope.IsValid()));
		const FAngelscriptScriptTestRegistryBuildResult Build =
			FAngelscriptScriptTestRegistry::Get().Rebuild(
				Engine.GetActiveModules(),
				true);
		ASSERT_THAT(IsTrue(Build.Snapshot.IsValid()));
		const FAngelscriptScriptTestDescriptor* Descriptor =
			FindOnlyAngelscriptScriptTestDescriptor(Build);
		ASSERT_THAT(IsNotNull(Descriptor));

		FAngelscriptScriptTestProbe Probe(
			FString::Printf(
				TEXT("FAngelscriptScriptTestReloadCancelProbe_%p"),
				this));
		const TSharedPtr<FAngelscriptScriptTestExecutionContext> Context =
			FAngelscriptScriptTestRunner::Start(
				Descriptor->Id,
				Probe,
				false);
		ASSERT_THAT(IsTrue(Context.IsValid()));
		ASSERT_THAT(IsFalse(Context->IsComplete()));
		ASSERT_THAT(IsNotNull(Context->GetWorld()));

		TSet<FString> AffectedModules;
		AffectedModules.Add(Descriptor->Id.ModuleName);
		FAngelscriptScriptTestRunner::CancelModules(
			AffectedModules,
			TEXT("test reload boundary"));
		ASSERT_THAT(IsTrue(Context->IsComplete()));
		ASSERT_THAT(IsNull(Context->GetWorld()));
		ASSERT_THAT(AreEqual(5, Context->GetTrace().Num()));
		ASSERT_THAT(AreEqual(
			FName(TEXT("AfterEach")),
			Context->GetTrace()[2]));
		ASSERT_THAT(AreEqual(
			FName(TEXT("CleanupTwo")),
			Context->GetTrace()[3]));
		ASSERT_THAT(AreEqual(
			FName(TEXT("CleanupOne")),
			Context->GetTrace()[4]));
		const TArray<FAutomationExecutionEntry> Entries =
			Probe.GetExecutionEntries();
		ASSERT_THAT(AreEqual(1, Entries.Num()));
		ASSERT_THAT(IsTrue(
			Entries[0].Event.Message.Contains(
				TEXT("Refresh and rerun"))));
	}

	TEST_METHOD(CancelAffectedAdvancedCommandRunsAfterAndReleasesCommand)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		const FString ScriptSource = ASTEST_AS(R"AS(
			UCLASS()
			class UCancelOnReloadAdvancedCommand : ULatentAutomationCommand
			{
				UFUNCTION(BlueprintOverride)
					void Before()
					{
						UCancelAdvancedOnReloadScriptTests Suite =
							Cast<UCancelAdvancedOnReloadScriptTests>(
								GetCurrentSuite());
						Suite.AssertEquals(0, Suite.Value);
						Suite.Value = 1;
					}

				UFUNCTION(BlueprintOverride)
					bool Update()
					{
						return false;
					}

				UFUNCTION(BlueprintOverride)
					void After()
					{
						UCancelAdvancedOnReloadScriptTests Suite =
							Cast<UCancelAdvancedOnReloadScriptTests>(
								GetCurrentSuite());
						Suite.AssertEquals(1, Suite.Value);
						Suite.Value = 2;
					}
			}

			UCLASS(meta=(AngelscriptTestFlags="EditorContext;EngineFilter"))
			class UCancelAdvancedOnReloadScriptTests : UAngelscriptTestSuite
			{
				int Value = 0;

				UFUNCTION(meta=(AngelscriptTest))
					void WaitingAdvancedLeaf()
					{
						UCancelOnReloadAdvancedCommand Command =
							Cast<UCancelOnReloadAdvancedCommand>(
								NewObject(
									this,
									UCancelOnReloadAdvancedCommand::
										StaticClass()));
						FAngelscriptTest::Commands()
							.AddLatentCommand(Command, 5.0)
							.OnCleanup(n"VerifyAdvancedAfter");
					}

				UFUNCTION(BlueprintOverride)
					void AfterEach()
					{
						AssertEquals(2, Value);
						Value = 3;
					}

				void VerifyAdvancedAfter()
					{
						AssertEquals(3, Value);
						Value = 4;
					}
			}
			)AS");
		FScopedAngelscriptModule ModuleScope(
			*TestRunner,
			Engine,
			TEXT("ASTesting_ScriptTestHotReload_AdvancedCancel"),
			ScriptSource);
		ASSERT_THAT(IsTrue(ModuleScope.IsValid()));
		const FAngelscriptScriptTestRegistryBuildResult Build =
			FAngelscriptScriptTestRegistry::Get().Rebuild(
				Engine.GetActiveModules(),
				true);
		ASSERT_THAT(IsTrue(Build.Snapshot.IsValid()));
		const FAngelscriptScriptTestDescriptor* Descriptor =
			FindOnlyAngelscriptScriptTestDescriptor(Build);
		ASSERT_THAT(IsNotNull(Descriptor));

		FAngelscriptScriptTestProbe Probe(
			FString::Printf(
				TEXT("FAngelscriptScriptTestAdvancedReloadCancelProbe_%p"),
				this));
		const TSharedPtr<FAngelscriptScriptTestExecutionContext> Context =
			FAngelscriptScriptTestRunner::Start(
				Descriptor->Id,
				Probe,
				false);
		ASSERT_THAT(IsTrue(Context.IsValid()));
		ASSERT_THAT(IsFalse(Context->IsComplete()));

		// The first scheduler update starts the advanced command and executes
		// Before, while Update remains pending.
		Context->Update();
		ASSERT_THAT(IsFalse(Context->IsComplete()));

		TWeakObjectPtr<ULatentAutomationCommand> ActiveCommand;
		for (TObjectIterator<ULatentAutomationCommand> It; It; ++It)
		{
			if (!It->HasAnyFlags(RF_ClassDefaultObject)
				&& It->GetOuter() == Context->GetSuite())
			{
				ASSERT_THAT(IsFalse(ActiveCommand.IsValid()));
				ActiveCommand = *It;
			}
		}
		ASSERT_THAT(IsTrue(ActiveCommand.IsValid()));

		TSet<FString> AffectedModules;
		AffectedModules.Add(Descriptor->Id.ModuleName);
		FAngelscriptScriptTestRunner::CancelModules(
			AffectedModules,
			TEXT("advanced command reload boundary"));

		ASSERT_THAT(IsTrue(Context->IsComplete()));
		// Reload invalidation is intentionally reported as the terminal leaf
		// failure; cleanup assertions above must not add another failure.
		ASSERT_THAT(IsTrue(Context->HasFailed()));
		ASSERT_THAT(IsTrue(ActiveCommand.IsValid()));
		ASSERT_THAT(IsTrue(
			Context->GetTrace().Contains(
				TEXT("VerifyAdvancedAfter"))));
		const TArray<FAutomationExecutionEntry> Entries =
			Probe.GetExecutionEntries();
		ASSERT_THAT(AreEqual(1, Entries.Num()));
		ASSERT_THAT(IsTrue(
			Entries[0].Event.Message.Contains(
				TEXT("advanced command reload boundary"))));

		CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
		ASSERT_THAT(IsFalse(ActiveCommand.IsValid()));
	}

	TEST_METHOD(EngineShutdownCancelsActiveLeafBeforeReleasingScriptFunctions)
	{
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE_FULL();
		FAngelscriptEngineScope Scope(Engine);
		const FString ScriptSource = ASTEST_AS(R"AS(
			UCLASS(meta=(AngelscriptTestFlags="EditorContext;EngineFilter"))
			class UEngineShutdownScriptTests : UAngelscriptTestSuite
			{
				UFUNCTION(meta=(AngelscriptTest))
					void WaitingLeaf()
					{
						FAngelscriptTest::Commands()
							.WaitDelay(5.0)
							.OnCleanup(
								n"CleanupBeforeEngineRelease");
					}

				void CleanupBeforeEngineRelease()
					{
					}
			}
			)AS");
		asIScriptModule* Module = BuildModule(
			*TestRunner,
			Engine,
			"ASTesting_ScriptTestShutdown_ActiveLeaf",
			ScriptSource);
		ASSERT_THAT(IsNotNull(Module));
		const FAngelscriptScriptTestRegistryBuildResult Build =
			FAngelscriptScriptTestRegistry::Get().Rebuild(
				Engine.GetActiveModules(),
				true);
		ASSERT_THAT(IsTrue(Build.Snapshot.IsValid()));
		const FAngelscriptScriptTestDescriptor* Descriptor =
			FindOnlyAngelscriptScriptTestDescriptor(Build);
		ASSERT_THAT(IsNotNull(Descriptor));

		FAngelscriptScriptTestAutomation& Automation =
			FAngelscriptScriptTestAutomation::Get();
		Automation.EnsureBridges(*Build.Snapshot);
		Automation.EnterSectionForTesting(
			FString::Printf(
				TEXT("Angelscript.ScriptTests.%s.%s"),
				*Descriptor->Id.ModuleName,
				*Descriptor->Id.SuiteName));
		ASSERT_THAT(IsTrue(
			Automation.HasActiveSessionForTesting()));

		FAngelscriptScriptTestProbe Probe(
			FString::Printf(
				TEXT("FAngelscriptEngineShutdownProbe_%p"),
				this));
		const TSharedPtr<FAngelscriptScriptTestExecutionContext> Context =
			FAngelscriptScriptTestRunner::Start(
				Descriptor->Id,
				Probe,
				false);
		ASSERT_THAT(IsTrue(Context.IsValid()));
		ASSERT_THAT(IsFalse(Context->IsComplete()));

		Engine.Shutdown();

		ASSERT_THAT(IsFalse(
			Automation.HasActiveSessionForTesting()));
		ASSERT_THAT(IsTrue(Context->IsComplete()));
		ASSERT_THAT(IsTrue(Context->HasFailed()));
		ASSERT_THAT(IsTrue(
			Context->GetTrace().Contains(
				TEXT("CleanupBeforeEngineRelease"))));
	}

	TEST_METHOD(EngineShutdownClosesAutomaticSessionBeforeExtensionDetach)
	{
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE_FULL();
		FAngelscriptEngineScope Scope(Engine);
		const FString ScriptSource = ASTEST_AS(R"AS(
			UCLASS(meta=(AngelscriptTestFlags="EditorContext;EngineFilter"))
			class UAutomaticShutdownScriptTests : UAngelscriptTestSuite
			{
				UFUNCTION(BlueprintOverride)
					void AfterAll()
					{
					}

				UFUNCTION(meta=(AngelscriptTest))
					void WaitingLeaf()
					{
						FAngelscriptTest::Commands().WaitDelay(5.0);
					}
			}
			)AS");
		asIScriptModule* ScriptModule = BuildModule(
			*TestRunner,
			Engine,
			"ASTesting_ScriptTestShutdown_AutomaticSession",
			ScriptSource);
		ASSERT_THAT(IsNotNull(ScriptModule));
		const FAngelscriptScriptTestRegistryBuildResult Build =
			FAngelscriptScriptTestRegistry::Get().Rebuild(
				Engine.GetActiveModules(),
				true);
		ASSERT_THAT(IsTrue(Build.Snapshot.IsValid()));
		const FAngelscriptScriptTestDescriptor* Descriptor =
			FindOnlyAngelscriptScriptTestDescriptor(Build);
		ASSERT_THAT(IsNotNull(Descriptor));
		const TSharedPtr<FAngelscriptModuleDesc> Module =
			Engine.GetModule(Descriptor->Id.ModuleName);
		ASSERT_THAT(IsTrue(Module.IsValid()));

		Engine.EnsureScriptTestHotReloadRunnerForTesting();
		FAngelscriptScriptTestHotReloadRunner* Scheduler =
			Engine.GetScriptTestHotReloadRunnerForTesting();
		ASSERT_THAT(IsNotNull(Scheduler));
		TArray<TSharedRef<FAngelscriptModuleDesc>> CompiledModules;
		CompiledModules.Add(Module.ToSharedRef());
		Scheduler->PrepareTests(CompiledModules);
		ASSERT_THAT(IsTrue(Scheduler->RunTests(&Engine)));
		ASSERT_THAT(IsTrue(Scheduler->HasWork()));
		ASSERT_THAT(IsTrue(Scheduler->HasSuiteSessionForTesting()));
		int32 AfterAllInvocationCount = 0;
		bool bAfterAllPassed = false;
		Scheduler->SetSuiteSessionClosedObserverForTesting(
			[&AfterAllInvocationCount, &bAfterAllPassed](bool bPassed)
			{
				++AfterAllInvocationCount;
				bAfterAllPassed = bPassed;
			});

		struct FShutdownOrderExtension final : IAngelscriptExtension
		{
			FAngelscriptEngine* ExpectedEngine = nullptr;
			bool bSawDetach = false;
			bool bRunnerExistedAtDetach = false;
			bool bSuiteSessionExistedAtDetach = false;

			void OnEngineAttached(FAngelscriptEngine&) override
			{
			}

			void OnEngineDetached(FAngelscriptEngine& DetachedEngine) override
			{
				if (&DetachedEngine != ExpectedEngine)
				{
					return;
				}
				bSawDetach = true;
				FAngelscriptScriptTestHotReloadRunner* ActiveScheduler =
					DetachedEngine.GetScriptTestHotReloadRunnerForTesting();
				bRunnerExistedAtDetach = ActiveScheduler != nullptr;
				bSuiteSessionExistedAtDetach = ActiveScheduler != nullptr
					&& ActiveScheduler->HasSuiteSessionForTesting();
			}
		};

		TSharedRef<FShutdownOrderExtension> Extension =
			MakeShared<FShutdownOrderExtension>();
		Extension->ExpectedEngine = &Engine;
		FAngelscriptEngineExtensionRegistry& ExtensionRegistry =
			FAngelscriptEngineExtensionRegistry::Get();
		const FDelegateHandle ExtensionHandle =
			ExtensionRegistry.RegisterExtension(Extension);
		ON_SCOPE_EXIT
		{
			ExtensionRegistry.UnregisterExtension(ExtensionHandle);
		};

		Engine.Shutdown();

		ASSERT_THAT(IsTrue(Extension->bSawDetach));
		ASSERT_THAT(IsFalse(Extension->bRunnerExistedAtDetach));
		ASSERT_THAT(IsFalse(Extension->bSuiteSessionExistedAtDetach));
		ASSERT_THAT(AreEqual(1, AfterAllInvocationCount));
		ASSERT_THAT(IsTrue(bAfterAllPassed));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
