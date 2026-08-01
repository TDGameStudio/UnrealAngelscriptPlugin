#include "CQTest.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestModuleScope.h"

#include "Testing/AngelscriptScriptTestAutomation.h"
#include "Testing/AngelscriptScriptTestRegistry.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FAngelscriptScriptTestAutomationTests,
	"Angelscript.TestModule.Testing.ScriptTestFramework.Automation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	BEFORE_ALL()
	{
		ASTEST_CREATE_ENGINE();
		FAngelscriptScriptTestAutomation::Get().Startup();
	}

	AFTER_ALL()
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		ASTEST_RESET_ENGINE(Engine);
	}

	TEST_METHOD(ExactFlagMasksUsePersistentDeterministicBridges)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		const FString ScriptSource = ASTEST_AS(R"AS(
			UCLASS(meta=(AngelscriptTestFlags="EditorContext;EngineFilter"))
			class UEditorBridgeScriptTests : UAngelscriptTestSuite
			{
				UFUNCTION(meta=(AngelscriptTest))
				void Beta()
				{
				}

				UFUNCTION(meta=(AngelscriptTest))
				void Alpha()
				{
				}
			}

			UCLASS(meta=(AngelscriptTestFlags="ClientContext;ProductFilter"))
			class UClientBridgeScriptTests : UAngelscriptTestSuite
			{
				UFUNCTION(meta=(AngelscriptTest))
				void RuntimeLeaf()
				{
				}
			}
			)AS");

		FScopedAngelscriptModule ModuleScope(
			*TestRunner,
			Engine,
			TEXT("ASTesting_ScriptTestAutomation_Bridges"),
			ScriptSource);
		ASSERT_THAT(IsTrue(ModuleScope.IsValid()));
		const FAngelscriptScriptTestRegistryBuildResult Build =
			FAngelscriptScriptTestRegistry::Get().Rebuild(
				Engine.GetActiveModules(),
				true);
		ASSERT_THAT(IsTrue(Build.Snapshot.IsValid()));
		ASSERT_THAT(AreEqual(3, Build.Snapshot->Tests.Num()));

		const EAutomationTestFlags EditorFlags =
			EAutomationTestFlags::EditorContext
			| EAutomationTestFlags::EngineFilter;
		const EAutomationTestFlags ClientFlags =
			EAutomationTestFlags::ClientContext
			| EAutomationTestFlags::ProductFilter;
		FAngelscriptScriptTestAutomation& Automation =
			FAngelscriptScriptTestAutomation::Get();
		ASSERT_THAT(IsTrue(Automation.HasBridge(EditorFlags)));
		ASSERT_THAT(IsTrue(Automation.HasBridge(ClientFlags)));

		TArray<FString> EditorLeaves;
		TArray<FString> EditorCommands;
		Automation.GetLeavesForMask(
			EditorFlags,
			EditorLeaves,
			EditorCommands);
		ASSERT_THAT(AreEqual(2, EditorLeaves.Num()));
		ASSERT_THAT(AreEqual(2, EditorCommands.Num()));
		ASSERT_THAT(AreEqual(
			FString(TEXT(
				"ASTesting_ScriptTestAutomation_Bridges."
				"UEditorBridgeScriptTests.Beta")),
			EditorLeaves[0]));
		ASSERT_THAT(AreEqual(
			FString(TEXT(
				"ASTesting_ScriptTestAutomation_Bridges."
				"UEditorBridgeScriptTests.Alpha")),
			EditorLeaves[1]));

		TArray<FString> ClientLeaves;
		TArray<FString> ClientCommands;
		Automation.GetLeavesForMask(
			ClientFlags,
			ClientLeaves,
			ClientCommands);
		ASSERT_THAT(AreEqual(1, ClientLeaves.Num()));
		ASSERT_THAT(AreEqual(1, ClientCommands.Num()));

		for (const FAngelscriptScriptTestDescriptor& Descriptor :
			Build.Snapshot->Tests)
		{
			ASSERT_THAT(IsTrue(
				Descriptor.SourceFile.Contains(
					TEXT("ASTesting_ScriptTestAutomation_Bridges"))));
			ASSERT_THAT(IsTrue(Descriptor.SourceLine > 0));
		}

		const int32 BridgeCountBeforeEmpty =
			Automation.GetBridgeCount();
		const FAngelscriptScriptTestRegistryBuildResult EmptyBuild =
			FAngelscriptScriptTestRegistry::Get().Rebuild(
				TArray<TSharedRef<FAngelscriptModuleDesc>>(),
				true);
		ASSERT_THAT(IsTrue(EmptyBuild.Snapshot.IsValid()));
		ASSERT_THAT(AreEqual(0, EmptyBuild.Snapshot->Tests.Num()));
		ASSERT_THAT(AreEqual(
			BridgeCountBeforeEmpty,
			Automation.GetBridgeCount()));

		EditorLeaves.Reset();
		EditorCommands.Reset();
		Automation.GetLeavesForMask(
			EditorFlags,
			EditorLeaves,
			EditorCommands);
		ASSERT_THAT(AreEqual(0, EditorLeaves.Num()));

		FAngelscriptScriptTestRegistry::Get().Rebuild(
			Engine.GetActiveModules(),
			true);
	}

	TEST_METHOD(SectionSessionUsesSeparateAllHookInstance)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		const FString ScriptSource = ASTEST_AS(R"AS(
			UCLASS(meta=(AngelscriptTestFlags="EditorContext;EngineFilter"))
			class UAllHookSessionScriptTests : UAngelscriptTestSuite
			{
				int Value = 0;

				UFUNCTION(BlueprintOverride)
				void BeforeAll()
				{
					Value = 100;
				}

				UFUNCTION(meta=(AngelscriptTest))
				void MethodInstanceIsFresh()
				{
					AssertEquals(0, Value);
				}

				UFUNCTION(BlueprintOverride)
				void AfterAll()
				{
					Value = 200;
				}
			}
			)AS");

		const FString ModuleName(
			TEXT("ASTesting_ScriptTestAutomation_AllHooks"));
		FScopedAngelscriptModule ModuleScope(
			*TestRunner,
			Engine,
			*ModuleName,
			ScriptSource);
		ASSERT_THAT(IsTrue(ModuleScope.IsValid()));
		const FAngelscriptScriptTestRegistryBuildResult Build =
			FAngelscriptScriptTestRegistry::Get().Rebuild(
				Engine.GetActiveModules(),
				true);
		ASSERT_THAT(IsTrue(Build.Snapshot.IsValid()));
		ASSERT_THAT(AreEqual(1, Build.Snapshot->Tests.Num()));
		const FAngelscriptScriptTestDescriptor& Descriptor =
			Build.Snapshot->Tests[0];

		FAngelscriptScriptTestAutomation& Automation =
			FAngelscriptScriptTestAutomation::Get();
		const FString Section = FString::Printf(
			TEXT("Angelscript.ScriptTests.%s.%s"),
			*Descriptor.Id.ModuleName,
			*Descriptor.Id.SuiteName);
		Automation.EnterSectionForTesting(Section);
		ASSERT_THAT(IsTrue(
			Automation.HasActiveSessionForTesting()));
		ASSERT_THAT(AreEqual(
			Build.Snapshot->Generation,
			Automation.GetActiveSessionGenerationForTesting()));

		TArray<FAutomationExecutionEntry> Entries;
		const bool bExecuted =
			Automation.ExecuteBridgeCommandForTesting(
				Descriptor.Flags,
				Descriptor.Id.ToCommandString(
					Descriptor.Generation),
				Entries);
		ASSERT_THAT(IsTrue(bExecuted));
		ASSERT_THAT(AreEqual(0, Entries.Num()));

		Automation.LeaveSectionForTesting(Section);
		ASSERT_THAT(IsFalse(
			Automation.HasActiveSessionForTesting()));
	}

	TEST_METHOD(BeforeAllOrdinaryExceptionSkipsLeafAndStillClosesSession)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		const FString ScriptSource = ASTEST_AS(R"AS(
			UCLASS(meta=(AngelscriptTestFlags="EditorContext;EngineFilter"))
			class UThrowingAllHookScriptTests : UAngelscriptTestSuite
			{
				UFUNCTION(BlueprintOverride)
				void BeforeAll()
				{
					throw("intentional BeforeAll exception");
				}

				UFUNCTION(meta=(AngelscriptTest))
				void MustNotExecute()
				{
					Fail("leaf executed after BeforeAll failure");
				}

				UFUNCTION(BlueprintOverride)
				void AfterAll()
				{
				}
			}
			)AS");

		FScopedAngelscriptModule ModuleScope(
			*TestRunner,
			Engine,
			TEXT("ASTesting_ScriptTestAutomation_BeforeAllThrow"),
			ScriptSource);
		ASSERT_THAT(IsTrue(ModuleScope.IsValid()));
		const FAngelscriptScriptTestRegistryBuildResult Build =
			FAngelscriptScriptTestRegistry::Get().Rebuild(
				Engine.GetActiveModules(),
				true);
		ASSERT_THAT(IsTrue(Build.Snapshot.IsValid()));
		ASSERT_THAT(AreEqual(1, Build.Snapshot->Tests.Num()));
		const FAngelscriptScriptTestDescriptor& Descriptor =
			Build.Snapshot->Tests[0];

		FAngelscriptScriptTestAutomation& Automation =
			FAngelscriptScriptTestAutomation::Get();
		const FString Section = FString::Printf(
			TEXT("Angelscript.ScriptTests.%s.%s"),
			*Descriptor.Id.ModuleName,
			*Descriptor.Id.SuiteName);
		Automation.EnterSectionForTesting(Section);

		TArray<FAutomationExecutionEntry> Entries;
		ASSERT_THAT(IsFalse(
			Automation.ExecuteBridgeCommandForTesting(
				Descriptor.Flags,
				Descriptor.Id.ToCommandString(
					Descriptor.Generation),
				Entries)));
		ASSERT_THAT(AreEqual(1, Entries.Num()));
		ASSERT_THAT(IsTrue(
			Entries[0].Event.Message.Contains(
				TEXT("intentional BeforeAll exception"))));
		ASSERT_THAT(IsTrue(
			Entries[0].Event.Message.Contains(
				TEXT("ASTesting_ScriptTestAutomation_BeforeAllThrow"))));

		Automation.LeaveSectionForTesting(Section);
		ASSERT_THAT(IsFalse(
			Automation.HasActiveSessionForTesting()));
	}

	TEST_METHOD(AfterAllExceptionIsSurfacedAsSessionDiagnostic)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		const FString ScriptSource = ASTEST_AS(R"AS(
			UCLASS(meta=(AngelscriptTestFlags="EditorContext;EngineFilter"))
			class UThrowingAfterAllScriptTests : UAngelscriptTestSuite
			{
				UFUNCTION(meta=(AngelscriptTest))
					void PassingLeaf()
					{
					}

				UFUNCTION(BlueprintOverride)
					void AfterAll()
					{
						throw("intentional AfterAll exception");
					}
			}
			)AS");

		FScopedAngelscriptModule ModuleScope(
			*TestRunner,
			Engine,
			TEXT("ASTesting_ScriptTestAutomation_AfterAllThrow"),
			ScriptSource);
		ASSERT_THAT(IsTrue(ModuleScope.IsValid()));
		const FAngelscriptScriptTestRegistryBuildResult Build =
			FAngelscriptScriptTestRegistry::Get().Rebuild(
				Engine.GetActiveModules(),
				true);
		ASSERT_THAT(IsTrue(Build.Snapshot.IsValid()));
		ASSERT_THAT(AreEqual(1, Build.Snapshot->Tests.Num()));
		const FAngelscriptScriptTestDescriptor& Descriptor =
			Build.Snapshot->Tests[0];

		FAngelscriptScriptTestAutomation& Automation =
			FAngelscriptScriptTestAutomation::Get();
		const FString Section = FString::Printf(
			TEXT("Angelscript.ScriptTests.%s.%s"),
			*Descriptor.Id.ModuleName,
			*Descriptor.Id.SuiteName);
		Automation.EnterSectionForTesting(Section);
		TArray<FAutomationExecutionEntry> Entries;
		ASSERT_THAT(IsTrue(
			Automation.ExecuteBridgeCommandForTesting(
				Descriptor.Flags,
				Descriptor.Id.ToCommandString(
					Descriptor.Generation),
				Entries)));
		ASSERT_THAT(AreEqual(0, Entries.Num()));

		TestRunner->AddExpectedError(
			TEXT("intentional AfterAll exception"),
			EAutomationExpectedErrorFlags::Contains,
			1);
		Automation.LeaveSectionForTesting(Section);
		ASSERT_THAT(IsFalse(
			Automation.HasActiveSessionForTesting()));
	}

	TEST_METHOD(AllHooksRejectGlobalLeafFacadeCalls)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		const FString ScriptSource = ASTEST_AS(R"AS(
			UCLASS(meta=(AngelscriptTestFlags="EditorContext;EngineFilter"))
			class UAllHookMisuseScriptTests : UAngelscriptTestSuite
			{
				UFUNCTION(BlueprintOverride)
				void BeforeAll()
				{
					FAngelscriptTest::CreateTestWorld();
				}

				UFUNCTION(meta=(AngelscriptTest))
				void MustNotExecute()
				{
					Fail("leaf executed after All-hook helper misuse");
				}
			}
			)AS");

		FScopedAngelscriptModule ModuleScope(
			*TestRunner,
			Engine,
			TEXT("ASTesting_ScriptTestAutomation_AllHookMisuse"),
			ScriptSource);
		ASSERT_THAT(IsTrue(ModuleScope.IsValid()));
		const FAngelscriptScriptTestRegistryBuildResult Build =
			FAngelscriptScriptTestRegistry::Get().Rebuild(
				Engine.GetActiveModules(),
				true);
		ASSERT_THAT(IsTrue(Build.Snapshot.IsValid()));
		ASSERT_THAT(AreEqual(1, Build.Snapshot->Tests.Num()));
		const FAngelscriptScriptTestDescriptor& Descriptor =
			Build.Snapshot->Tests[0];

		FAngelscriptScriptTestAutomation& Automation =
			FAngelscriptScriptTestAutomation::Get();
		const FString Section = FString::Printf(
			TEXT("Angelscript.ScriptTests.%s.%s"),
			*Descriptor.Id.ModuleName,
			*Descriptor.Id.SuiteName);
		Automation.EnterSectionForTesting(Section);

		TArray<FAutomationExecutionEntry> Entries;
		ASSERT_THAT(IsFalse(
			Automation.ExecuteBridgeCommandForTesting(
				Descriptor.Flags,
				Descriptor.Id.ToCommandString(
					Descriptor.Generation),
				Entries)));
		ASSERT_THAT(AreEqual(1, Entries.Num()));
		ASSERT_THAT(IsTrue(
			Entries[0].Event.Message.Contains(
				TEXT("BeforeAll cannot use method-local test helpers"))));
		ASSERT_THAT(IsTrue(
			Entries[0].Event.Message.Contains(
				TEXT("ASTesting_ScriptTestAutomation_AllHookMisuse"))));

		Automation.LeaveSectionForTesting(Section);
		ASSERT_THAT(IsFalse(
			Automation.HasActiveSessionForTesting()));
	}

	TEST_METHOD(StaleAndMovedCommandsFailSafelyWhileBridgesPersist)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		const FString ModuleName(
			TEXT("ASTesting_ScriptTestAutomation_Stale"));
		const EAutomationTestFlags OldFlags =
			EAutomationTestFlags::EditorContext
			| EAutomationTestFlags::EngineFilter;
		FString OldCommand;
		int32 BridgeCountAfterOld = 0;
		{
			const FString OldSource = ASTEST_AS(R"AS(
				UCLASS(meta=(AngelscriptTestFlags="EditorContext;EngineFilter"))
				class UMovingBridgeScriptTests : UAngelscriptTestSuite
				{
					UFUNCTION(meta=(AngelscriptTest))
						void StableLeaf()
						{
						}
				}
				)AS");
			FScopedAngelscriptModule OldScope(
				*TestRunner,
				Engine,
				*ModuleName,
				OldSource);
			ASSERT_THAT(IsTrue(OldScope.IsValid()));
			const FAngelscriptScriptTestRegistryBuildResult OldBuild =
				FAngelscriptScriptTestRegistry::Get().Rebuild(
					Engine.GetActiveModules(),
					true);
			ASSERT_THAT(IsTrue(OldBuild.Snapshot.IsValid()));
			ASSERT_THAT(AreEqual(1, OldBuild.Snapshot->Tests.Num()));
			OldCommand =
				OldBuild.Snapshot->Tests[0].Id.ToCommandString(
					OldBuild.Snapshot->Generation);
			BridgeCountAfterOld =
				FAngelscriptScriptTestAutomation::Get()
					.GetBridgeCount();
		}

		FAngelscriptScriptTestRegistry::Get().Rebuild(
			Engine.GetActiveModules(),
			true);
		TArray<FAutomationExecutionEntry> RemovedEntries;
		ASSERT_THAT(IsFalse(
			FAngelscriptScriptTestAutomation::Get()
				.ExecuteBridgeCommandForTesting(
					OldFlags,
					OldCommand,
					RemovedEntries)));
		ASSERT_THAT(AreEqual(1, RemovedEntries.Num()));
		ASSERT_THAT(IsTrue(
			RemovedEntries[0].Event.Message.Contains(
				TEXT("removed or renamed"))));
		ASSERT_THAT(AreEqual(
			BridgeCountAfterOld,
			FAngelscriptScriptTestAutomation::Get()
				.GetBridgeCount()));

		const FString NewSource = ASTEST_AS(R"AS(
			UCLASS(meta=(AngelscriptTestFlags="ClientContext;ProductFilter"))
			class UMovingBridgeScriptTests : UAngelscriptTestSuite
			{
				UFUNCTION(meta=(AngelscriptTest))
					void StableLeaf()
					{
					}
			}
			)AS");
		FScopedAngelscriptModule NewScope(
			*TestRunner,
			Engine,
			*ModuleName,
			NewSource);
		ASSERT_THAT(IsTrue(NewScope.IsValid()));
		const FAngelscriptScriptTestRegistryBuildResult NewBuild =
			FAngelscriptScriptTestRegistry::Get().Rebuild(
				Engine.GetActiveModules(),
				true);
		ASSERT_THAT(IsTrue(NewBuild.Snapshot.IsValid()));
		ASSERT_THAT(AreEqual(1, NewBuild.Snapshot->Tests.Num()));

		TArray<FAutomationExecutionEntry> MovedEntries;
		ASSERT_THAT(IsFalse(
			FAngelscriptScriptTestAutomation::Get()
				.ExecuteBridgeCommandForTesting(
					OldFlags,
					OldCommand,
					MovedEntries)));
		ASSERT_THAT(AreEqual(1, MovedEntries.Num()));
		ASSERT_THAT(IsTrue(
			MovedEntries[0].Event.Message.Contains(
				TEXT("different flag bucket"))));
		ASSERT_THAT(IsTrue(
			FAngelscriptScriptTestAutomation::Get().HasBridge(
				EAutomationTestFlags::ClientContext
				| EAutomationTestFlags::ProductFilter)));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
