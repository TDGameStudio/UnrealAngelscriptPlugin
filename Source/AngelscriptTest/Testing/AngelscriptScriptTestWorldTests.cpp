#include "CQTest.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestModuleScope.h"
#include "AngelscriptScriptTestTestHelpers.h"

#include "Engine/Engine.h"
#include "HAL/PlatformProcess.h"
#include "Testing/AngelscriptScriptTestRegistry.h"
#include "Testing/AngelscriptScriptTestRunner.h"
#include "UObject/GarbageCollection.h"
#include "UObject/UObjectIterator.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FAngelscriptScriptTestWorldTests,
	"Angelscript.TestModule.Testing.ScriptTestFramework.World",
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

	TEST_METHOD(FacadeTypesAreBoundAndSuiteEnvironmentAliasesAreRemoved)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		asIScriptEngine* ScriptEngine = Engine.GetScriptEngine();
		ASSERT_THAT(IsNotNull(ScriptEngine));

		asITypeInfo* SuiteType =
			ScriptEngine->GetTypeInfoByName("UAngelscriptTestSuite");
		ASSERT_THAT(IsNotNull(SuiteType));
		ASSERT_THAT(IsNotNull(SuiteType->GetMethodByName("AssertTrue")));
		for (const ANSICHAR* RemovedMethod : {
			"CreateTestWorld",
			"GetTestWorld",
			"SpawnActor",
			"WaitDelay",
			"AddLatentCommand"})
		{
			ASSERT_THAT(IsNull(
				SuiteType->GetMethodByName(RemovedMethod)));
		}

		ASSERT_THAT(IsNotNull(
			ScriptEngine->GetTypeInfoByName("FAngelscriptTest")));
		ASSERT_THAT(IsNotNull(
			ScriptEngine->GetTypeInfoByName(
				"FAngelscriptTestCommandBuilder")));
	}

	TEST_METHOD(PureLeafHasNoWorldAndCanSpawnPlainObject)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		const FString ScriptSource = ASTEST_AS(R"AS(
			UCLASS()
			class UPureLeafObject : UObject
			{
			}

			UObject SpawnPlainObjectForActiveLeaf()
			{
				return FAngelscriptTest::SpawnObject(
					UPureLeafObject::StaticClass());
			}

			UCLASS(meta=(AngelscriptTestFlags="EditorContext;EngineFilter"))
			class UPureWorldScriptTests : UAngelscriptTestSuite
			{
				UFUNCTION(meta=(AngelscriptTest))
				void HasNoImplicitWorld()
				{
					AssertNull(FAngelscriptTest::GetTestWorld());
					AssertNull(GetWorld());
					UObject Object = SpawnPlainObjectForActiveLeaf();
					AssertNotNull(Object);
					AssertSame(this, Object.GetOuter());
					AssertNull(FAngelscriptTest::GetTestWorld());
				}
			}
			)AS");

		FScopedAngelscriptModule ModuleScope(
			*TestRunner,
			Engine,
			TEXT("ASTesting_ScriptTestWorld_Pure"),
			ScriptSource);
		ASSERT_THAT(IsTrue(ModuleScope.IsValid()));
		const FAngelscriptScriptTestRegistryBuildResult Build =
			FAngelscriptScriptTestRegistry::Get().Rebuild(
				Engine.GetActiveModules(),
				true);
		const FAngelscriptScriptTestDescriptor* Descriptor =
			FindOnlyAngelscriptScriptTestDescriptor(Build);
		ASSERT_THAT(IsNotNull(Descriptor));

		const int32 WorldContextsBefore =
			GEngine->GetWorldContexts().Num();
		const TSharedPtr<FAngelscriptScriptTestExecutionContext> Context =
			FAngelscriptScriptTestRunner::Start(
				Descriptor->Id,
				*TestRunner);
		ASSERT_THAT(IsTrue(Context.IsValid()));
		ASSERT_THAT(IsFalse(Context->HasFailed()));
		ASSERT_THAT(IsNull(Context->GetWorld()));
		ASSERT_THAT(AreEqual(
			WorldContextsBefore,
			GEngine->GetWorldContexts().Num()));
	}

	TEST_METHOD(WorldSpawnBeginPlayTickAndExplicitDestroy)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		const FString ScriptSource = ASTEST_AS(R"AS(
			UCLASS()
			class ULocalWorldProbeObject : UObject
			{
			}

			UCLASS()
			class ALocalWorldProbeActor : AActor
			{
				int BeginPlayCount = 0;
				int TickCount = 0;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					BeginPlayCount += 1;
				}

				UFUNCTION(BlueprintOverride)
				void Tick(float DeltaSeconds)
				{
					TickCount += 1;
				}
			}

			UCLASS()
			class ULocalWorldProbeComponent : UActorComponent
			{
				int TickCount = 0;

				UFUNCTION(BlueprintOverride)
				void Tick(float DeltaSeconds)
				{
					TickCount += 1;
				}
			}

			UCLASS(meta=(AngelscriptTestFlags="EditorContext;EngineFilter"))
			class ULocalWorldScriptTests : UAngelscriptTestSuite
			{
				UFUNCTION(meta=(AngelscriptTest))
				void DrivesOwnedWorldState()
				{
					FAngelscriptTest::CreateTestWorld(false);
					UWorld World = FAngelscriptTest::GetTestWorld();
					AssertNotNull(World);
					AssertSame(World, GetWorld());

					ULocalWorldProbeObject Object = Cast<ULocalWorldProbeObject>(
						FAngelscriptTest::SpawnObject(
							ULocalWorldProbeObject::StaticClass()));
					AssertNotNull(Object);
					AssertSame(this, Object.GetOuter());

					ALocalWorldProbeActor Actor = Cast<ALocalWorldProbeActor>(
						FAngelscriptTest::SpawnActor(
							ALocalWorldProbeActor::StaticClass(),
							FVector(10.0, 20.0, 30.0)));
					AssertNotNull(Actor);
					AssertSame(World, Actor.GetWorld());

					ULocalWorldProbeComponent Component =
						Cast<ULocalWorldProbeComponent>(
							FAngelscriptTest::SpawnComponent(
								ULocalWorldProbeComponent::StaticClass(),
								Actor,
								true));
					AssertNotNull(Component);
					AssertSame(Actor, Component.GetOwner());
					AssertSame(World, Component.GetWorld());

					FAngelscriptTest::BeginPlay(Actor);
					FAngelscriptTest::BeginPlay(Actor);
					AssertEquals(1, Actor.BeginPlayCount);

					FAngelscriptTest::TickActor(Actor, 0.01, 3);
					AssertEquals(3, Actor.TickCount);
					FAngelscriptTest::TickComponent(
						Component,
						0.01,
						4);
					AssertEquals(4, Component.TickCount);

					const float TimeBefore = World.GetTimeSeconds();
					FAngelscriptTest::AdvanceTime(0.05, 2);
					AssertGreaterThanOrEqual(
						World.GetTimeSeconds(),
						TimeBefore + 0.1);

					FAngelscriptTest::DestroyActor(Actor, true);
					FAngelscriptTest::DestroyTestWorld();
					AssertNull(FAngelscriptTest::GetTestWorld());
					AssertNull(GetWorld());
					FAngelscriptTest::DestroyTestWorld();
				}
			}
			)AS");

		FScopedAngelscriptModule ModuleScope(
			*TestRunner,
			Engine,
			TEXT("ASTesting_ScriptTestWorld_Drive"),
			ScriptSource);
		ASSERT_THAT(IsTrue(ModuleScope.IsValid()));
		const FAngelscriptScriptTestRegistryBuildResult Build =
			FAngelscriptScriptTestRegistry::Get().Rebuild(
				Engine.GetActiveModules(),
				true);
		const FAngelscriptScriptTestDescriptor* Descriptor =
			FindOnlyAngelscriptScriptTestDescriptor(Build);
		ASSERT_THAT(IsNotNull(Descriptor));

		const int32 WorldContextsBefore =
			GEngine->GetWorldContexts().Num();
		const TSharedPtr<FAngelscriptScriptTestExecutionContext> Context =
			FAngelscriptScriptTestRunner::Start(
				Descriptor->Id,
				*TestRunner);
		ASSERT_THAT(IsTrue(Context.IsValid()));
		ASSERT_THAT(IsFalse(Context->HasFailed()));
		ASSERT_THAT(IsNull(Context->GetWorld()));
		ASSERT_THAT(AreEqual(
			WorldContextsBefore,
			GEngine->GetWorldContexts().Num()));
	}

	TEST_METHOD(GameInstanceWorldIsExplicitAndAutomaticallyCleaned)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		const FString ScriptSource = ASTEST_AS(R"AS(
			UCLASS(meta=(AngelscriptTestFlags="EditorContext;EngineFilter"))
			class UGameInstanceWorldScriptTests : UAngelscriptTestSuite
			{
				UFUNCTION(meta=(AngelscriptTest))
				void CreatesGameInstanceWorld()
				{
					FAngelscriptTest::CreateTestWorld(true);
					AssertNotNull(FAngelscriptTest::GetTestWorld());
					AssertNotNull(
						FAngelscriptTest::GetTestWorld().GetGameInstance());
				}
			}
			)AS");

		FScopedAngelscriptModule ModuleScope(
			*TestRunner,
			Engine,
			TEXT("ASTesting_ScriptTestWorld_GameInstance"),
			ScriptSource);
		ASSERT_THAT(IsTrue(ModuleScope.IsValid()));
		const FAngelscriptScriptTestRegistryBuildResult Build =
			FAngelscriptScriptTestRegistry::Get().Rebuild(
				Engine.GetActiveModules(),
				true);
		const FAngelscriptScriptTestDescriptor* Descriptor =
			FindOnlyAngelscriptScriptTestDescriptor(Build);
		ASSERT_THAT(IsNotNull(Descriptor));

		const int32 WorldContextsBefore =
			GEngine->GetWorldContexts().Num();
		const TSharedPtr<FAngelscriptScriptTestExecutionContext> Context =
			FAngelscriptScriptTestRunner::Start(
				Descriptor->Id,
				*TestRunner);
		ASSERT_THAT(IsTrue(Context.IsValid()));
		ASSERT_THAT(IsFalse(Context->HasFailed()));
		ASSERT_THAT(IsNull(Context->GetWorld()));
		ASSERT_THAT(AreEqual(
			WorldContextsBefore,
			GEngine->GetWorldContexts().Num()));
	}

	TEST_METHOD(DuplicateCreationFailsWithoutLeakingFirstWorld)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		const FString ScriptSource = ASTEST_AS(R"AS(
			UCLASS(meta=(AngelscriptTestFlags="EditorContext;EngineFilter"))
			class UDuplicateWorldScriptTests : UAngelscriptTestSuite
			{
				UFUNCTION(meta=(AngelscriptTest))
				void RejectsReplacement()
				{
					FAngelscriptTest::CreateTestWorld(false);
					FAngelscriptTest::CreateTestWorld(false);
				}
			}
			)AS");

		FScopedAngelscriptModule ModuleScope(
			*TestRunner,
			Engine,
			TEXT("ASTesting_ScriptTestWorld_Duplicate"),
			ScriptSource);
		ASSERT_THAT(IsTrue(ModuleScope.IsValid()));
		const FAngelscriptScriptTestRegistryBuildResult Build =
			FAngelscriptScriptTestRegistry::Get().Rebuild(
				Engine.GetActiveModules(),
				true);
		const FAngelscriptScriptTestDescriptor* Descriptor =
			FindOnlyAngelscriptScriptTestDescriptor(Build);
		ASSERT_THAT(IsNotNull(Descriptor));

		const int32 WorldContextsBefore =
			GEngine->GetWorldContexts().Num();
		FAngelscriptScriptTestProbe Probe(
			FString::Printf(
				TEXT("FAngelscriptScriptTestDuplicateWorldProbe_%p"),
				this));
		const TSharedPtr<FAngelscriptScriptTestExecutionContext> Context =
			FAngelscriptScriptTestRunner::Start(
				Descriptor->Id,
				Probe);
		ASSERT_THAT(IsTrue(Context.IsValid()));
		ASSERT_THAT(IsTrue(Context->HasFailed()));
		ASSERT_THAT(IsNull(Context->GetWorld()));
		ASSERT_THAT(AreEqual(
			WorldContextsBefore,
			GEngine->GetWorldContexts().Num()));
		const TArray<FAutomationExecutionEntry> Entries =
			Probe.GetExecutionEntries();
		ASSERT_THAT(AreEqual(1, Entries.Num()));
		ASSERT_THAT(IsTrue(
			Entries[0].Event.Message.Contains(
				TEXT("cannot replace an active test World"))));
	}

	TEST_METHOD(PureLeafReleasesTrackedObjectsWithoutCreatingWorld)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		const FString ScriptSource = ASTEST_AS(R"AS(
			UCLASS()
			class UNoWorldTrackedObject : UObject
			{
			}

			UCLASS(meta=(AngelscriptTestFlags="EditorContext;EngineFilter"))
			class UNoWorldCleanupScriptTests : UAngelscriptTestSuite
			{
				UFUNCTION(meta=(AngelscriptTest))
					void ReleasesTrackedObject()
					{
						FAngelscriptTest::SpawnObject(
							UNoWorldTrackedObject::StaticClass());
					}
			}
			)AS");

		FScopedAngelscriptModule ModuleScope(
			*TestRunner,
			Engine,
			TEXT("ASTesting_ScriptTestWorld_NoWorldCleanup"),
			ScriptSource);
		ASSERT_THAT(IsTrue(ModuleScope.IsValid()));
		const FAngelscriptScriptTestRegistryBuildResult Build =
			FAngelscriptScriptTestRegistry::Get().Rebuild(
				Engine.GetActiveModules(),
				true);
		const FAngelscriptScriptTestDescriptor* Descriptor =
			FindOnlyAngelscriptScriptTestDescriptor(Build);
		ASSERT_THAT(IsNotNull(Descriptor));

		const int32 WorldContextsBefore =
			GEngine->GetWorldContexts().Num();
		FAngelscriptScriptTestProbe Probe(
			FString::Printf(
				TEXT("FAngelscriptScriptTestNoWorldCleanupProbe_%p"),
				this));
		const TSharedPtr<FAngelscriptScriptTestExecutionContext> Context =
			FAngelscriptScriptTestRunner::Start(
				Descriptor->Id,
				Probe,
				false);
		ASSERT_THAT(IsTrue(Context.IsValid()));
		ASSERT_THAT(IsTrue(Context->IsComplete()));
		ASSERT_THAT(IsFalse(Context->HasFailed()));
		ASSERT_THAT(IsNull(Context->GetSuite()));
		ASSERT_THAT(IsNull(Context->GetWorld()));
		ASSERT_THAT(AreEqual(
			WorldContextsBefore,
			GEngine->GetWorldContexts().Num()));

		UClass* TrackedClass = FindObject<UClass>(
			nullptr,
			TEXT("/Script/AngelscriptProject.UNoWorldTrackedObject"));
		if (TrackedClass == nullptr)
		{
			for (TObjectIterator<UClass> It; It; ++It)
			{
				if (It->GetName() == TEXT("UNoWorldTrackedObject"))
				{
					TrackedClass = *It;
					break;
				}
			}
		}
		ASSERT_THAT(IsNotNull(TrackedClass));

		TWeakObjectPtr<UObject> TrackedObject;
		for (TObjectIterator<UObject> It; It; ++It)
		{
			if (!It->HasAnyFlags(RF_ClassDefaultObject)
				&& It->GetClass() == TrackedClass)
			{
				TrackedObject = *It;
				break;
			}
		}
		ASSERT_THAT(IsTrue(TrackedObject.IsValid()));
		CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
		ASSERT_THAT(IsFalse(TrackedObject.IsValid()));
		ASSERT_THAT(IsNull(Context->GetSuite()));
		ASSERT_THAT(AreEqual(
			WorldContextsBefore,
			GEngine->GetWorldContexts().Num()));
	}

	TEST_METHOD(TerminalFailuresReleaseEveryOwnedWorldResource)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		const FString ScriptSource = ASTEST_AS(R"AS(
			UCLASS()
			class UTerminalCleanupObject : UObject
			{
			}

			UCLASS()
			class ATerminalCleanupActor : AActor
			{
			}

			UCLASS()
			class UTerminalCleanupComponent : UActorComponent
			{
			}

			UCLASS(meta=(AngelscriptTestFlags="EditorContext;EngineFilter"))
			class UTerminalCleanupScriptTests : UAngelscriptTestSuite
			{
				void CreateOwnedResources()
				{
					FAngelscriptTest::CreateTestWorld(true);
					FAngelscriptTest::SpawnObject(
						UTerminalCleanupObject::StaticClass());
					ATerminalCleanupActor Actor =
						Cast<ATerminalCleanupActor>(
							FAngelscriptTest::SpawnActor(
								ATerminalCleanupActor::StaticClass()));
					FAngelscriptTest::SpawnComponent(
						UTerminalCleanupComponent::StaticClass(),
						Actor,
						true);
				}

				bool NeverReady()
				{
					return false;
				}

				UFUNCTION(meta=(AngelscriptTest))
					void AssertionFailure()
					{
						CreateOwnedResources();
						Fail("intentional cleanup assertion");
					}

				UFUNCTION(meta=(AngelscriptTest))
					void OrdinaryException()
					{
						CreateOwnedResources();
						throw("intentional cleanup exception");
					}

				UFUNCTION(meta=(AngelscriptTest))
					void TimeoutFailure()
					{
						CreateOwnedResources();
						FAngelscriptTest::Commands()
							.Until(n"NeverReady", 0.01);
					}

				UFUNCTION(meta=(AngelscriptTest))
					void ExplicitCancellation()
					{
						CreateOwnedResources();
						FAngelscriptTest::Commands()
							.WaitDelay(5.0);
					}
			}
			)AS");

		FScopedAngelscriptModule ModuleScope(
			*TestRunner,
			Engine,
			TEXT("ASTesting_ScriptTestWorld_TerminalCleanup"),
			ScriptSource);
		ASSERT_THAT(IsTrue(ModuleScope.IsValid()));
		const FAngelscriptScriptTestRegistryBuildResult Build =
			FAngelscriptScriptTestRegistry::Get().Rebuild(
				Engine.GetActiveModules(),
				true);
		ASSERT_THAT(IsTrue(Build.Snapshot.IsValid()));
		ASSERT_THAT(AreEqual(4, Build.Snapshot->Tests.Num()));

		const int32 WorldContextsBefore =
			GEngine->GetWorldContexts().Num();
		const FString TimeoutMethod(TEXT("TimeoutFailure"));
		const FString CancellationMethod(TEXT("ExplicitCancellation"));
		int32 Serial = 0;
		for (const FAngelscriptScriptTestDescriptor& Descriptor :
			Build.Snapshot->Tests)
		{
			FAngelscriptScriptTestProbe Probe(
				FString::Printf(
					TEXT("FAngelscriptScriptTestTerminalCleanup_%d_%p"),
					Serial++,
					this));
			const TSharedPtr<FAngelscriptScriptTestExecutionContext> Context =
				FAngelscriptScriptTestRunner::Start(
					Descriptor.Id,
					Probe,
					false);
			ASSERT_THAT(IsTrue(Context.IsValid()));

			if (Descriptor.Id.MethodName == CancellationMethod)
			{
				ASSERT_THAT(IsFalse(Context->IsComplete()));
				ASSERT_THAT(IsNotNull(Context->GetWorld()));
				ASSERT_THAT(AreEqual(
					WorldContextsBefore + 1,
					GEngine->GetWorldContexts().Num()));
				Context->Cancel(TEXT(
					"intentional terminal cleanup cancellation"));
			}
			else if (Descriptor.Id.MethodName == TimeoutMethod)
			{
				const double Deadline =
					FPlatformTime::Seconds() + 1.0;
				while (!Context->IsComplete()
					&& FPlatformTime::Seconds() < Deadline)
				{
					Context->Update();
					FPlatformProcess::SleepNoStats(0.001f);
				}
			}

			ASSERT_THAT(IsTrue(Context->IsComplete()));
			ASSERT_THAT(IsTrue(Context->HasFailed()));
			ASSERT_THAT(IsNull(Context->GetSuite()));
			ASSERT_THAT(IsNull(Context->GetWorld()));
			ASSERT_THAT(AreEqual(
				WorldContextsBefore,
				GEngine->GetWorldContexts().Num()));
		}

		CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
		const TSet<FString> ResourceClassNames = {
			TEXT("UTerminalCleanupObject"),
			TEXT("ATerminalCleanupActor"),
			TEXT("UTerminalCleanupComponent"),
		};
		for (TObjectIterator<UObject> It; It; ++It)
		{
			if (!It->HasAnyFlags(RF_ClassDefaultObject)
				&& ResourceClassNames.Contains(
					It->GetClass()->GetName()))
			{
				TestRunner->AddError(FString::Printf(
					TEXT("Terminal cleanup retained resource '%s' of "
						"class '%s'."),
					*It->GetPathName(),
					*It->GetClass()->GetName()));
			}
		}
		ASSERT_THAT(AreEqual(
			WorldContextsBefore,
			GEngine->GetWorldContexts().Num()));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
