#include "CQTest.h"
#include "AngelscriptFunctionalTestUtils.h"
#include "AngelscriptTestEngineHelper.h"
#include "AngelscriptTestMacros.h"

#include "ClassGenerator/ASClass.h"
#include "Components/ActorTestSpawner.h"
#include "Components/SceneComponent.h"
#include "Misc/ScopeExit.h"

#if WITH_DEV_AUTOMATION_TESTS

TEST_CLASS_WITH_FLAGS(FAngelscriptHotReloadComponentTests,
	"Angelscript.TestModule.HotReload.Component",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	inline static const FName SoftReloadModuleName = FName(TEXT("HotReloadComponentSoftMetadata"));
	inline static const FString SoftReloadFilename = FString(TEXT("HotReloadComponentSoftMetadata.as"));
	inline static const FName SoftReloadBaseClassName = FName(TEXT("AHotReloadComponentSoftBaseActor"));
	inline static const FName SoftReloadDerivedClassName = FName(TEXT("AHotReloadComponentSoftDerivedActor"));
	inline static const FName RootComponentName = FName(TEXT("RootScene"));
	inline static const FName BillboardComponentName = FName(TEXT("Billboard"));
	inline static const FName ReplacementComponentName = FName(TEXT("ReplacementBillboard"));
	inline static const FName RootComponentClassName = FName(TEXT("UHotReloadComponentRootComponent"));

	struct FDefaultComponentMetadataSnapshot
	{
		FName ComponentClassName = NAME_None;
		FName ComponentName = NAME_None;
		FName Attach = NAME_None;
		FName AttachSocket = NAME_None;
		bool bIsRoot = false;
		bool bEditorOnly = false;

		bool operator==(const FDefaultComponentMetadataSnapshot& Other) const
		{
			return ComponentClassName == Other.ComponentClassName
				&& ComponentName == Other.ComponentName
				&& Attach == Other.Attach
				&& AttachSocket == Other.AttachSocket
				&& bIsRoot == Other.bIsRoot
				&& bEditorOnly == Other.bEditorOnly;
		}
	};

	struct FOverrideComponentMetadataSnapshot
	{
		FName ComponentClassName = NAME_None;
		FName OverrideComponentName = NAME_None;
		FName VariableName = NAME_None;

		bool operator==(const FOverrideComponentMetadataSnapshot& Other) const
		{
			return ComponentClassName == Other.ComponentClassName
				&& OverrideComponentName == Other.OverrideComponentName
				&& VariableName == Other.VariableName;
		}
	};

	static bool IsHandledReloadResult(const ECompileResult ReloadResult)
	{
		return ReloadResult == ECompileResult::FullyHandled || ReloadResult == ECompileResult::PartiallyHandled;
	}

	static const UASClass::FDefaultComponent* FindDefaultComponentEntryByName(const UASClass* ScriptClass, FName ComponentName)
	{
		if (ScriptClass == nullptr)
		{
			return nullptr;
		}

		return ScriptClass->DefaultComponents.FindByPredicate(
			[ComponentName](const UASClass::FDefaultComponent& Entry)
			{
				return Entry.ComponentName == ComponentName;
			});
	}

	static const UASClass::FOverrideComponent* FindOverrideComponentEntryByVariableName(const UASClass* ScriptClass, FName VariableName)
	{
		if (ScriptClass == nullptr)
		{
			return nullptr;
		}

		return ScriptClass->OverrideComponents.FindByPredicate(
			[VariableName](const UASClass::FOverrideComponent& Entry)
			{
				return Entry.VariableName == VariableName;
			});
	}

	static TArray<FDefaultComponentMetadataSnapshot> SnapshotDefaultComponents(const UASClass* ScriptClass)
	{
		TArray<FDefaultComponentMetadataSnapshot> Snapshot;
		if (ScriptClass == nullptr)
		{
			return Snapshot;
		}

		Snapshot.Reserve(ScriptClass->DefaultComponents.Num());
		for (const UASClass::FDefaultComponent& Entry : ScriptClass->DefaultComponents)
		{
			Snapshot.Add({
				Entry.ComponentClass != nullptr ? Entry.ComponentClass->GetFName() : NAME_None,
				Entry.ComponentName,
				Entry.Attach,
				Entry.AttachSocket,
				Entry.bIsRoot,
				Entry.bEditorOnly
			});
		}
		return Snapshot;
	}

	static TArray<FOverrideComponentMetadataSnapshot> SnapshotOverrideComponents(const UASClass* ScriptClass)
	{
		TArray<FOverrideComponentMetadataSnapshot> Snapshot;
		if (ScriptClass == nullptr)
		{
			return Snapshot;
		}

		Snapshot.Reserve(ScriptClass->OverrideComponents.Num());
		for (const UASClass::FOverrideComponent& Entry : ScriptClass->OverrideComponents)
		{
			Snapshot.Add({
				Entry.ComponentClass != nullptr ? Entry.ComponentClass->GetFName() : NAME_None,
				Entry.OverrideComponentName,
				Entry.VariableName
			});
		}
		return Snapshot;
	}

	static USceneComponent* FindSceneComponentByName(const AActor* Actor, FName ComponentName)
	{
		if (Actor == nullptr)
		{
			return nullptr;
		}

		for (UActorComponent* Component : Actor->GetComponents())
		{
			if (Component != nullptr && Component->GetFName() == ComponentName)
			{
				return Cast<USceneComponent>(Component);
			}
		}
		return nullptr;
	}

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

	TEST_METHOD(DefaultComponentMetadataAndRuntimeHierarchySurviveSoftReload)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope EngineScope(Engine);

		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*SoftReloadModuleName.ToString());
		};

		const FString ReloadV1Source = ASTEST_AS(R"AS(
			UCLASS()
			class UHotReloadComponentRootComponent : USceneComponent
			{
			}

			UCLASS()
			class UHotReloadComponentBillboardComponent : UBillboardComponent
			{
			}

			UCLASS()
			class UHotReloadComponentReplacementComponent : UHotReloadComponentBillboardComponent
			{
			}

			UCLASS()
			class AHotReloadComponentSoftBaseActor : AActor
			{
				UPROPERTY(DefaultComponent, RootComponent)
				UHotReloadComponentRootComponent RootScene;

				UPROPERTY(DefaultComponent, Attach = RootScene)
				UHotReloadComponentBillboardComponent Billboard;
			}

			UCLASS()
			class AHotReloadComponentSoftDerivedActor : AHotReloadComponentSoftBaseActor
			{
				UPROPERTY(OverrideComponent = Billboard)
				UHotReloadComponentReplacementComponent ReplacementBillboard;

				UFUNCTION()
				int GetVersion()
				{
					return 1;
				}
			}
			)AS");

		ASSERT_THAT(IsTrue(
			CompileAnnotatedModuleFromMemory(&Engine, SoftReloadModuleName, SoftReloadFilename, ReloadV1Source),
			TEXT("Initial component hot reload module should compile")));

		UASClass* BaseClassBeforeReload = Cast<UASClass>(FindGeneratedClass(&Engine, SoftReloadBaseClassName));
		UASClass* DerivedClassBeforeReload = Cast<UASClass>(FindGeneratedClass(&Engine, SoftReloadDerivedClassName));
		ASSERT_THAT(IsNotNull(BaseClassBeforeReload, TEXT("Component base class should exist before reload")));
		ASSERT_THAT(IsNotNull(DerivedClassBeforeReload, TEXT("Component derived class should exist before reload")));

		const TArray<FDefaultComponentMetadataSnapshot> DefaultSnapshotBeforeReload = SnapshotDefaultComponents(BaseClassBeforeReload);
		const TArray<FOverrideComponentMetadataSnapshot> OverrideSnapshotBeforeReload = SnapshotOverrideComponents(DerivedClassBeforeReload);
		ASSERT_THAT(AreEqual(2, DefaultSnapshotBeforeReload.Num(), TEXT("Base class should start with two default component entries")));
		ASSERT_THAT(AreEqual(1, OverrideSnapshotBeforeReload.Num(), TEXT("Derived class should start with one override component entry")));

		const UASClass::FDefaultComponent* RootEntryBeforeReload = FindDefaultComponentEntryByName(BaseClassBeforeReload, RootComponentName);
		const UASClass::FDefaultComponent* BillboardEntryBeforeReload = FindDefaultComponentEntryByName(BaseClassBeforeReload, BillboardComponentName);
		const UASClass::FOverrideComponent* OverrideEntryBeforeReload = FindOverrideComponentEntryByVariableName(DerivedClassBeforeReload, ReplacementComponentName);
		ASSERT_THAT(IsNotNull(RootEntryBeforeReload, TEXT("Root default component entry should exist before reload")));
		ASSERT_THAT(IsNotNull(BillboardEntryBeforeReload, TEXT("Billboard default component entry should exist before reload")));
		ASSERT_THAT(IsNotNull(OverrideEntryBeforeReload, TEXT("Replacement override component entry should exist before reload")));
		ASSERT_THAT(IsTrue(RootEntryBeforeReload->bIsRoot, TEXT("RootScene should be the root component before reload")));
		ASSERT_THAT(AreEqual(RootComponentName, BillboardEntryBeforeReload->Attach, TEXT("Billboard should attach to RootScene before reload")));
		ASSERT_THAT(AreEqual(BillboardComponentName, OverrideEntryBeforeReload->OverrideComponentName, TEXT("Replacement should override Billboard before reload")));

		const FString ReloadV2Source = ASTEST_AS(R"AS(
			UCLASS()
			class UHotReloadComponentRootComponent : USceneComponent
			{
			}

			UCLASS()
			class UHotReloadComponentBillboardComponent : UBillboardComponent
			{
			}

			UCLASS()
			class UHotReloadComponentReplacementComponent : UHotReloadComponentBillboardComponent
			{
			}

			UCLASS()
			class AHotReloadComponentSoftBaseActor : AActor
			{
				UPROPERTY(DefaultComponent, RootComponent)
				UHotReloadComponentRootComponent RootScene;

				UPROPERTY(DefaultComponent, Attach = RootScene)
				UHotReloadComponentBillboardComponent Billboard;
			}

			UCLASS()
			class AHotReloadComponentSoftDerivedActor : AHotReloadComponentSoftBaseActor
			{
				UPROPERTY(OverrideComponent = Billboard)
				UHotReloadComponentReplacementComponent ReplacementBillboard;

				UFUNCTION()
				int GetVersion()
				{
					return 2;
				}
			}
			)AS");

		ECompileResult ReloadResult = ECompileResult::Error;
		ASSERT_THAT(IsTrue(
			CompileModuleWithResult(&Engine, ECompileType::SoftReloadOnly, SoftReloadModuleName, SoftReloadFilename, ReloadV2Source, ReloadResult),
			TEXT("Component body-only soft reload should compile")));
		ASSERT_THAT(IsTrue(IsHandledReloadResult(ReloadResult), TEXT("Component soft reload should be handled")));

		UASClass* BaseClassAfterReload = Cast<UASClass>(FindGeneratedClass(&Engine, SoftReloadBaseClassName));
		UASClass* DerivedClassAfterReload = Cast<UASClass>(FindGeneratedClass(&Engine, SoftReloadDerivedClassName));
		ASSERT_THAT(IsNotNull(BaseClassAfterReload, TEXT("Component base class should exist after reload")));
		ASSERT_THAT(IsNotNull(DerivedClassAfterReload, TEXT("Component derived class should exist after reload")));
		ASSERT_THAT(AreEqual(BaseClassBeforeReload, BaseClassAfterReload, TEXT("Soft reload should preserve component base class identity")));
		ASSERT_THAT(AreEqual(DerivedClassBeforeReload, DerivedClassAfterReload, TEXT("Soft reload should preserve component derived class identity")));

		const TArray<FDefaultComponentMetadataSnapshot> DefaultSnapshotAfterReload = SnapshotDefaultComponents(BaseClassAfterReload);
		const TArray<FOverrideComponentMetadataSnapshot> OverrideSnapshotAfterReload = SnapshotOverrideComponents(DerivedClassAfterReload);
		ASSERT_THAT(AreEqual(DefaultSnapshotBeforeReload.Num(), DefaultSnapshotAfterReload.Num(), TEXT("Soft reload should not duplicate default component entries")));
		ASSERT_THAT(AreEqual(OverrideSnapshotBeforeReload.Num(), OverrideSnapshotAfterReload.Num(), TEXT("Soft reload should not duplicate override component entries")));
		ASSERT_THAT(IsTrue(DefaultSnapshotBeforeReload == DefaultSnapshotAfterReload, TEXT("Soft reload should preserve default component metadata")));
		ASSERT_THAT(IsTrue(OverrideSnapshotBeforeReload == OverrideSnapshotAfterReload, TEXT("Soft reload should preserve override component metadata")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* ReloadedActor = AngelscriptFunctionalTestUtils::SpawnScriptActor(*TestRunner, Spawner, DerivedClassAfterReload);
		ASSERT_THAT(IsNotNull(ReloadedActor, TEXT("Reloaded component actor should spawn")));

		USceneComponent* RuntimeRootComponent = ReloadedActor->GetRootComponent();
		USceneComponent* RuntimeBillboardComponent = FindSceneComponentByName(ReloadedActor, BillboardComponentName);
		ASSERT_THAT(IsNotNull(RuntimeRootComponent, TEXT("Reloaded actor should create a runtime root component")));
		ASSERT_THAT(IsNotNull(RuntimeBillboardComponent, TEXT("Reloaded actor should create the overridden Billboard component")));
		ASSERT_THAT(AreEqual(FindGeneratedClass(&Engine, RootComponentClassName), RuntimeRootComponent->GetClass(), TEXT("Runtime root component should use the generated root component class")));
		ASSERT_THAT(AreEqual(RuntimeRootComponent, RuntimeBillboardComponent->GetAttachParent(), TEXT("Runtime Billboard component should remain attached to the root")));

		UFunction* GetVersionAfterReload = FindGeneratedFunction(DerivedClassAfterReload, TEXT("GetVersion"));
		ASSERT_THAT(IsNotNull(GetVersionAfterReload, TEXT("Component derived class should expose GetVersion after reload")));

		int32 VersionAfterReload = 0;
		ASSERT_THAT(IsTrue(
			ExecuteGeneratedIntEventOnGameThread(&Engine, ReloadedActor, GetVersionAfterReload, VersionAfterReload),
			TEXT("Reloaded component actor should execute GetVersion")));
		ASSERT_THAT(AreEqual(2, VersionAfterReload, TEXT("Component actor should observe the reloaded function body")));
	}
};

#endif
