#include "AngelscriptFunctionalTestUtils.h"
#include "AngelscriptTestMacros.h"

#include "CQTest.h"
#include "ClassGenerator/ASClass.h"
#include "Misc/ScopeExit.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptFunctionalTestUtils;

namespace AngelscriptTest_ClassGenerator_AngelscriptASClassComponentMetadataTests_Private
{
	static const FName ASClassComponentMetadataModuleName(TEXT("ASClassComponentMetadata"));
	static const FString ASClassComponentMetadataFilename(TEXT("ASClassComponentMetadata.as"));
	static const FName ASClassComponentMetadataDerivedClassName(TEXT("AMetadataDerivedActor"));
	static const FName ASClassComponentMetadataBaseClassName(TEXT("AMetadataBaseActor"));
	static const FName ASClassRootComponentName(TEXT("RootScene"));
	static const FName ASClassBillboardComponentName(TEXT("Billboard"));
	static const FName ASClassOverrideVariableName(TEXT("ReplacementBillboard"));

	static const FName ASClassComponentMetadataSoftReloadModuleName(TEXT("ASClassComponentMetadataSoftReload"));
	static const FString ASClassComponentMetadataSoftReloadFilename(TEXT("ASClassComponentMetadataSoftReload.as"));
	static const FName ASClassComponentMetadataSoftReloadDerivedClassName(TEXT("ASoftMetadataDerivedActor"));
	static const FName ASClassComponentMetadataSoftReloadBaseClassName(TEXT("ASoftMetadataBaseActor"));
	static const FName ASClassSoftReloadRootComponentClassName(TEXT("USoftMetadataRootComponent"));
	static const FName ASClassSoftReloadReplacementComponentClassName(TEXT("USoftMetadataReplacementBillboardComponent"));

	bool IsHandledReloadResult(const ECompileResult ReloadResult)
	{
		return ReloadResult == ECompileResult::FullyHandled || ReloadResult == ECompileResult::PartiallyHandled;
	}

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

	const UASClass::FDefaultComponent* FindDefaultComponentEntryByName(const UASClass* ScriptClass, FName ComponentName)
	{
		if (ScriptClass == nullptr) { return nullptr; }
		return ScriptClass->DefaultComponents.FindByPredicate([ComponentName](const UASClass::FDefaultComponent& Entry)
		{ return Entry.ComponentName == ComponentName; });
	}

	const UASClass::FOverrideComponent* FindOverrideComponentEntryByVariableName(const UASClass* ScriptClass, FName VariableName)
	{
		if (ScriptClass == nullptr) { return nullptr; }
		return ScriptClass->OverrideComponents.FindByPredicate([VariableName](const UASClass::FOverrideComponent& Entry)
		{ return Entry.VariableName == VariableName; });
	}

	TArray<FDefaultComponentMetadataSnapshot> SnapshotDefaultComponentLayoutMetadata(const UASClass* ScriptClass)
	{
		TArray<FDefaultComponentMetadataSnapshot> Snapshot;
		if (ScriptClass == nullptr) { return Snapshot; }
		Snapshot.Reserve(ScriptClass->DefaultComponents.Num());
		for (const UASClass::FDefaultComponent& Entry : ScriptClass->DefaultComponents)
		{
			Snapshot.Add({
				Entry.ComponentClass != nullptr ? Entry.ComponentClass->GetFName() : NAME_None,
				Entry.ComponentName, Entry.Attach, Entry.AttachSocket, Entry.bIsRoot, Entry.bEditorOnly
			});
		}
		return Snapshot;
	}

	TArray<FOverrideComponentMetadataSnapshot> SnapshotOverrideComponentLayoutMetadata(const UASClass* ScriptClass)
	{
		TArray<FOverrideComponentMetadataSnapshot> Snapshot;
		if (ScriptClass == nullptr) { return Snapshot; }
		Snapshot.Reserve(ScriptClass->OverrideComponents.Num());
		for (const UASClass::FOverrideComponent& Entry : ScriptClass->OverrideComponents)
		{
			Snapshot.Add({
				Entry.ComponentClass != nullptr ? Entry.ComponentClass->GetFName() : NAME_None,
				Entry.OverrideComponentName, Entry.VariableName
			});
		}
		return Snapshot;
	}

	USceneComponent* FindSceneComponentByName(const AActor* Actor, FName ComponentName)
	{
		if (Actor == nullptr) { return nullptr; }
		for (UActorComponent* Component : Actor->GetComponents())
		{
			if (Component != nullptr && Component->GetFName() == ComponentName)
			{ return Cast<USceneComponent>(Component); }
		}
		return nullptr;
	}
}

TEST_CLASS_WITH_FLAGS(FAngelscriptASClassComponentMetadataTests,
	"Angelscript.TestModule.ClassGenerator.ASClass",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(DefaultComponentMetadataCapturesRootAndAttachLayout)
	{
		using namespace AngelscriptTest_ClassGenerator_AngelscriptASClassComponentMetadataTests_Private;
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine);
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ASClassComponentMetadataModuleName.ToString());
			ASTEST_RESET_ENGINE(Engine);
		};

		const FString ScriptSource = TEXT(R"AS(
UCLASS()
class UMetadataRootComponent : USceneComponent
{
}

UCLASS()
class UMetadataBillboardComponent : UBillboardComponent
{
}

UCLASS()
class UMetadataReplacementBillboardComponent : UMetadataBillboardComponent
{
}

UCLASS()
class AMetadataBaseActor : AActor
{
	UPROPERTY(DefaultComponent, RootComponent)
	UMetadataRootComponent RootScene;

	UPROPERTY(DefaultComponent, Attach = RootScene)
	UMetadataBillboardComponent Billboard;
}

UCLASS()
class AMetadataDerivedActor : AMetadataBaseActor
{
	UPROPERTY(OverrideComponent = Billboard)
	UMetadataReplacementBillboardComponent ReplacementBillboard;
}
)AS");

		UClass* DerivedActorClass = CompileScriptModule(*TestRunner, Engine, ASClassComponentMetadataModuleName, ASClassComponentMetadataFilename, ScriptSource, ASClassComponentMetadataDerivedClassName);
		if (DerivedActorClass == nullptr) { return; }

		UASClass* BaseActorClass = Cast<UASClass>(FindGeneratedClass(&Engine, ASClassComponentMetadataBaseClassName));
		UASClass* DerivedASClass = Cast<UASClass>(DerivedActorClass);
		UClass* RootComponentClass = FindGeneratedClass(&Engine, TEXT("UMetadataRootComponent"));
		UClass* BillboardComponentClass = FindGeneratedClass(&Engine, TEXT("UMetadataBillboardComponent"));
		UClass* ReplacementBillboardComponentClass = FindGeneratedClass(&Engine, TEXT("UMetadataReplacementBillboardComponent"));
		if (!TestRunner->TestNotNull(TEXT("ASClass component metadata test should resolve the generated base actor class"), BaseActorClass)
			|| !TestRunner->TestNotNull(TEXT("ASClass component metadata test should compile the derived actor to a UASClass"), DerivedASClass)
			|| !TestRunner->TestNotNull(TEXT("ASClass component metadata test should resolve the generated root component class"), RootComponentClass)
			|| !TestRunner->TestNotNull(TEXT("ASClass component metadata test should resolve the generated billboard component class"), BillboardComponentClass)
			|| !TestRunner->TestNotNull(TEXT("ASClass component metadata test should resolve the generated replacement component class"), ReplacementBillboardComponentClass))
		{ return; }

		const UASClass::FDefaultComponent* RootEntry = FindDefaultComponentEntryByName(BaseActorClass, ASClassRootComponentName);
		const UASClass::FDefaultComponent* BillboardEntry = FindDefaultComponentEntryByName(BaseActorClass, ASClassBillboardComponentName);
		const UASClass::FOverrideComponent* OverrideEntry = FindOverrideComponentEntryByVariableName(DerivedASClass, ASClassOverrideVariableName);

		TestRunner->TestEqual(TEXT("ASClass component metadata test should record exactly two default components on the base class"), BaseActorClass->DefaultComponents.Num(), 2);
		TestRunner->TestEqual(TEXT("ASClass component metadata test should record exactly one override component on the derived class"), DerivedASClass->OverrideComponents.Num(), 1);

		if (!TestRunner->TestNotNull(TEXT("ASClass component metadata test should record a root-scene default component entry"), RootEntry)
			|| !TestRunner->TestNotNull(TEXT("ASClass component metadata test should record a billboard default component entry"), BillboardEntry)
			|| !TestRunner->TestNotNull(TEXT("ASClass component metadata test should record the derived override entry"), OverrideEntry))
		{ return; }

		TestRunner->TestTrue(TEXT("ASClass component metadata test should mark RootScene as the root component"), RootEntry->bIsRoot);
		TestRunner->TestTrue(TEXT("ASClass component metadata test should keep RootScene unattached"), RootEntry->Attach.IsNone());
		TestRunner->TestTrue(TEXT("ASClass component metadata test should preserve the generated root component class"), RootEntry->ComponentClass == RootComponentClass);
		TestRunner->TestFalse(TEXT("ASClass component metadata test should keep Billboard out of the root slot"), BillboardEntry->bIsRoot);
		TestRunner->TestEqual(TEXT("ASClass component metadata test should attach Billboard to RootScene"), BillboardEntry->Attach, ASClassRootComponentName);
		TestRunner->TestTrue(TEXT("ASClass component metadata test should preserve the generated billboard component class"), BillboardEntry->ComponentClass == BillboardComponentClass);
		TestRunner->TestEqual(TEXT("ASClass component metadata test should record which base component gets overridden"), OverrideEntry->OverrideComponentName, ASClassBillboardComponentName);
		TestRunner->TestEqual(TEXT("ASClass component metadata test should record the overriding property name"), OverrideEntry->VariableName, ASClassOverrideVariableName);
		TestRunner->TestTrue(TEXT("ASClass component metadata test should preserve the generated override component class"), OverrideEntry->ComponentClass == ReplacementBillboardComponentClass);
		}
	}

	TEST_METHOD(SoftReloadPreservesDefaultComponentMetadataWithoutDuplication)
	{
		using namespace AngelscriptTest_ClassGenerator_AngelscriptASClassComponentMetadataTests_Private;
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine);
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ASClassComponentMetadataSoftReloadModuleName.ToString());
			ASTEST_RESET_ENGINE(Engine);
		};

		const FString ScriptV1 = TEXT(R"AS(
UCLASS()
class USoftMetadataRootComponent : USceneComponent { }
UCLASS()
class USoftMetadataBillboardComponent : UBillboardComponent { }
UCLASS()
class USoftMetadataReplacementBillboardComponent : USoftMetadataBillboardComponent { }
UCLASS()
class ASoftMetadataBaseActor : AActor
{
	UPROPERTY(DefaultComponent, RootComponent) USoftMetadataRootComponent RootScene;
	UPROPERTY(DefaultComponent, Attach = RootScene) USoftMetadataBillboardComponent Billboard;
}
UCLASS()
class ASoftMetadataDerivedActor : ASoftMetadataBaseActor
{
	UPROPERTY(OverrideComponent = Billboard) USoftMetadataReplacementBillboardComponent ReplacementBillboard;
	UFUNCTION() int GetVersion() { return 1; }
}
)AS");

		const FString ScriptV2 = TEXT(R"AS(
UCLASS()
class USoftMetadataRootComponent : USceneComponent { }
UCLASS()
class USoftMetadataBillboardComponent : UBillboardComponent { }
UCLASS()
class USoftMetadataReplacementBillboardComponent : USoftMetadataBillboardComponent { }
UCLASS()
class ASoftMetadataBaseActor : AActor
{
	UPROPERTY(DefaultComponent, RootComponent) USoftMetadataRootComponent RootScene;
	UPROPERTY(DefaultComponent, Attach = RootScene) USoftMetadataBillboardComponent Billboard;
}
UCLASS()
class ASoftMetadataDerivedActor : ASoftMetadataBaseActor
{
	UPROPERTY(OverrideComponent = Billboard) USoftMetadataReplacementBillboardComponent ReplacementBillboard;
	UFUNCTION() int GetVersion() { return 2; }
}
)AS");

		UClass* InitialDerivedClass = CompileScriptModule(*TestRunner, Engine, ASClassComponentMetadataSoftReloadModuleName, ASClassComponentMetadataSoftReloadFilename, ScriptV1, ASClassComponentMetadataSoftReloadDerivedClassName);
		if (InitialDerivedClass == nullptr) { return; }

		UASClass* InitialBaseClass = Cast<UASClass>(FindGeneratedClass(&Engine, ASClassComponentMetadataSoftReloadBaseClassName));
		UASClass* InitialDerivedASClass = Cast<UASClass>(InitialDerivedClass);
		UClass* InitialRootComponentClass = FindGeneratedClass(&Engine, ASClassSoftReloadRootComponentClassName);
		UClass* InitialReplacementComponentClass = FindGeneratedClass(&Engine, ASClassSoftReloadReplacementComponentClassName);
		if (!TestRunner->TestNotNull(TEXT("Soft-reload test should resolve base actor class"), InitialBaseClass)
			|| !TestRunner->TestNotNull(TEXT("Soft-reload test should compile derived as UASClass"), InitialDerivedASClass)
			|| !TestRunner->TestNotNull(TEXT("Soft-reload test should resolve root component class"), InitialRootComponentClass)
			|| !TestRunner->TestNotNull(TEXT("Soft-reload test should resolve replacement component class"), InitialReplacementComponentClass))
		{ return; }

		const TArray<FDefaultComponentMetadataSnapshot> InitialDefaultSnapshot = SnapshotDefaultComponentLayoutMetadata(InitialBaseClass);
		const TArray<FOverrideComponentMetadataSnapshot> InitialOverrideSnapshot = SnapshotOverrideComponentLayoutMetadata(InitialDerivedASClass);
		TestRunner->TestEqual(TEXT("Soft-reload test should start with two default-component entries"), InitialDefaultSnapshot.Num(), 2);
		TestRunner->TestEqual(TEXT("Soft-reload test should start with one override-component entry"), InitialOverrideSnapshot.Num(), 1);

		ECompileResult ReloadResult = ECompileResult::Error;
		if (!TestRunner->TestTrue(TEXT("Soft-reload test should compile the body-only update"),
			CompileModuleWithResult(&Engine, ECompileType::SoftReloadOnly, ASClassComponentMetadataSoftReloadModuleName, ASClassComponentMetadataSoftReloadFilename, ScriptV2, ReloadResult)))
		{ return; }
		if (!TestRunner->TestTrue(TEXT("Soft-reload test should stay on a handled path"), IsHandledReloadResult(ReloadResult)))
		{ return; }

		UASClass* ReloadedBaseClass = Cast<UASClass>(FindGeneratedClass(&Engine, ASClassComponentMetadataSoftReloadBaseClassName));
		UASClass* ReloadedDerivedClass = Cast<UASClass>(FindGeneratedClass(&Engine, ASClassComponentMetadataSoftReloadDerivedClassName));
		UFunction* GetVersionAfterReload = ReloadedDerivedClass != nullptr ? FindGeneratedFunction(ReloadedDerivedClass, TEXT("GetVersion")) : nullptr;
		if (!TestRunner->TestNotNull(TEXT("Soft-reload test should still expose base class"), ReloadedBaseClass)
			|| !TestRunner->TestNotNull(TEXT("Soft-reload test should still expose derived class"), ReloadedDerivedClass)
			|| !TestRunner->TestNotNull(TEXT("Soft-reload test should still expose GetVersion"), GetVersionAfterReload))
		{ return; }

		TestRunner->TestTrue(TEXT("Soft-reload test should preserve base UASClass instance"), ReloadedBaseClass == InitialBaseClass);
		TestRunner->TestTrue(TEXT("Soft-reload test should preserve derived UASClass instance"), ReloadedDerivedClass == InitialDerivedASClass);

		const TArray<FDefaultComponentMetadataSnapshot> ReloadedDefaultSnapshot = SnapshotDefaultComponentLayoutMetadata(ReloadedBaseClass);
		const TArray<FOverrideComponentMetadataSnapshot> ReloadedOverrideSnapshot = SnapshotOverrideComponentLayoutMetadata(ReloadedDerivedClass);
		TestRunner->TestEqual(TEXT("Soft-reload test should keep default-component count stable"), ReloadedDefaultSnapshot.Num(), InitialDefaultSnapshot.Num());
		TestRunner->TestEqual(TEXT("Soft-reload test should keep override-component count stable"), ReloadedOverrideSnapshot.Num(), InitialOverrideSnapshot.Num());
		TestRunner->TestTrue(TEXT("Soft-reload test should preserve default-component metadata"), ReloadedDefaultSnapshot == InitialDefaultSnapshot);
		TestRunner->TestTrue(TEXT("Soft-reload test should preserve override-component metadata"), ReloadedOverrideSnapshot == InitialOverrideSnapshot);

		const UASClass::FDefaultComponent* RootEntryAfterReload = FindDefaultComponentEntryByName(ReloadedBaseClass, ASClassRootComponentName);
		const UASClass::FDefaultComponent* BillboardEntryAfterReload = FindDefaultComponentEntryByName(ReloadedBaseClass, ASClassBillboardComponentName);
		const UASClass::FOverrideComponent* OverrideEntryAfterReload = FindOverrideComponentEntryByVariableName(ReloadedDerivedClass, ASClassOverrideVariableName);
		if (!TestRunner->TestNotNull(TEXT("Soft-reload test should keep root metadata entry"), RootEntryAfterReload)
			|| !TestRunner->TestNotNull(TEXT("Soft-reload test should keep billboard metadata entry"), BillboardEntryAfterReload)
			|| !TestRunner->TestNotNull(TEXT("Soft-reload test should keep override metadata entry"), OverrideEntryAfterReload))
		{ return; }

		TestRunner->TestTrue(TEXT("Soft-reload test should keep RootScene as unique root"), RootEntryAfterReload->bIsRoot);
		TestRunner->TestTrue(TEXT("Soft-reload test should keep RootScene unattached"), RootEntryAfterReload->Attach.IsNone());
		TestRunner->TestEqual(TEXT("Soft-reload test should keep Billboard attached to RootScene"), BillboardEntryAfterReload->Attach, ASClassRootComponentName);
		TestRunner->TestEqual(TEXT("Soft-reload test should keep override pointed at Billboard"), OverrideEntryAfterReload->OverrideComponentName, ASClassBillboardComponentName);

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* ReloadedActor = SpawnScriptActor(*TestRunner, Spawner, ReloadedDerivedClass);
		if (!TestRunner->TestNotNull(TEXT("Soft-reload test should spawn the reloaded actor"), ReloadedActor)) { return; }

		USceneComponent* RuntimeRootComponent = ReloadedActor->GetRootComponent();
		USceneComponent* RuntimeBillboardComponent = FindSceneComponentByName(ReloadedActor, ASClassBillboardComponentName);
		if (!TestRunner->TestNotNull(TEXT("Soft-reload test should create runtime root component"), RuntimeRootComponent)
			|| !TestRunner->TestNotNull(TEXT("Soft-reload test should create overridden Billboard component"), RuntimeBillboardComponent))
		{ return; }

		TestRunner->TestTrue(TEXT("Soft-reload test should keep root component class aligned with metadata"), RuntimeRootComponent->GetClass() == FindGeneratedClass(&Engine, ASClassSoftReloadRootComponentClassName));
		TestRunner->TestTrue(TEXT("Soft-reload test should keep Billboard attached to root"), RuntimeBillboardComponent->GetAttachParent() == RuntimeRootComponent);

		int32 VersionAfterReload = 0;
		if (!TestRunner->TestTrue(TEXT("Soft-reload test should execute the reloaded function"),
			ExecuteGeneratedIntEventOnGameThread(&Engine, ReloadedActor, GetVersionAfterReload, VersionAfterReload)))
		{ return; }
		TestRunner->TestEqual(TEXT("Soft-reload test should observe updated function body"), VersionAfterReload, 2);
		}
	}
};

#endif
