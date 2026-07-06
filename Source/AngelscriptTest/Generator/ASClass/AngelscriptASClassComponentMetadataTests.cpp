#include "AngelscriptFunctionalTestUtils.h"
#include "AngelscriptTestMacros.h"

#include "CQTest.h"
#include "ClassGenerator/ASClass.h"
#include "Misc/ScopeExit.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FAngelscriptASClassComponentMetadataTests,
	"Angelscript.TestModule.Generator.ASClass.ComponentMetadata",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
static bool CheckTrue(FAutomationTestBase& Test, const TCHAR* Message, bool bActual)
{
	FNoDiscardAsserter LocalAssert(Test);
	return LocalAssert.IsTrue(bActual, Message);
}

static bool CheckFalse(FAutomationTestBase& Test, const TCHAR* Message, bool bActual)
{
	FNoDiscardAsserter LocalAssert(Test);
	return LocalAssert.IsFalse(bActual, Message);
}

template <typename ActualType, typename ExpectedType>
static bool CheckEqual(FAutomationTestBase& Test, const TCHAR* Message, const ActualType& Actual, const ExpectedType& Expected)
{
	FNoDiscardAsserter LocalAssert(Test);
	return LocalAssert.AreEqual(Expected, Actual, Message);
}

template <typename ValueType>
static bool CheckNotNull(FAutomationTestBase& Test, const TCHAR* Message, const ValueType& Value)
{
	FNoDiscardAsserter LocalAssert(Test);
	return LocalAssert.IsNotNull(Value, Message);
}

inline static const FName ASClassComponentMetadataModuleName = FName(TEXT("ASClassComponentMetadata"));
inline static const FString ASClassComponentMetadataFilename = FString(TEXT("ASClassComponentMetadata.as"));
inline static const FName ASClassComponentMetadataDerivedClassName = FName(TEXT("AMetadataDerivedActor"));
inline static const FName ASClassComponentMetadataBaseClassName = FName(TEXT("AMetadataBaseActor"));
inline static const FName ASClassRootComponentName = FName(TEXT("RootScene"));
inline static const FName ASClassBillboardComponentName = FName(TEXT("Billboard"));
inline static const FName ASClassOverrideVariableName = FName(TEXT("ReplacementBillboard"));

inline static const FName ASClassComponentMetadataSoftReloadModuleName = FName(TEXT("ASClassComponentMetadataSoftReload"));
inline static const FString ASClassComponentMetadataSoftReloadFilename = FString(TEXT("ASClassComponentMetadataSoftReload.as"));
inline static const FName ASClassComponentMetadataSoftReloadDerivedClassName = FName(TEXT("ASoftMetadataDerivedActor"));
inline static const FName ASClassComponentMetadataSoftReloadBaseClassName = FName(TEXT("ASoftMetadataBaseActor"));
inline static const FName ASClassSoftReloadRootComponentClassName = FName(TEXT("USoftMetadataRootComponent"));
inline static const FName ASClassSoftReloadReplacementComponentClassName = FName(TEXT("USoftMetadataReplacementBillboardComponent"));

static bool IsHandledReloadResult(const ECompileResult ReloadResult)
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

static const UASClass::FDefaultComponent* FindDefaultComponentEntryByName(const UASClass* ScriptClass, FName ComponentName)
{
	if (ScriptClass == nullptr) { return nullptr; }
	return ScriptClass->DefaultComponents.FindByPredicate([ComponentName](const UASClass::FDefaultComponent& Entry)
	{ return Entry.ComponentName == ComponentName; });
}

static const UASClass::FOverrideComponent* FindOverrideComponentEntryByVariableName(const UASClass* ScriptClass, FName VariableName)
{
	if (ScriptClass == nullptr) { return nullptr; }
	return ScriptClass->OverrideComponents.FindByPredicate([VariableName](const UASClass::FOverrideComponent& Entry)
	{ return Entry.VariableName == VariableName; });
}

static TArray<FDefaultComponentMetadataSnapshot> SnapshotDefaultComponentLayoutMetadata(const UASClass* ScriptClass)
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

static TArray<FOverrideComponentMetadataSnapshot> SnapshotOverrideComponentLayoutMetadata(const UASClass* ScriptClass)
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

static USceneComponent* FindSceneComponentByName(const AActor* Actor, FName ComponentName)
{
	if (Actor == nullptr) { return nullptr; }
	for (UActorComponent* Component : Actor->GetComponents())
	{
		if (Component != nullptr && Component->GetFName() == ComponentName)
		{ return Cast<USceneComponent>(Component); }
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

	TEST_METHOD(DefaultComponentMetadataCapturesRootAndAttachLayout)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope EngineScope(Engine);

		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ASClassComponentMetadataModuleName.ToString());
		};

		const FString ScriptSource = ASTEST_AS(R"AS(
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

		UClass* DerivedActorClass = AngelscriptFunctionalTestUtils::CompileScriptModule(*TestRunner, Engine, ASClassComponentMetadataModuleName, ASClassComponentMetadataFilename, ScriptSource, ASClassComponentMetadataDerivedClassName);
		if (DerivedActorClass == nullptr) { return; }

		UASClass* BaseActorClass = Cast<UASClass>(FindGeneratedClass(&Engine, ASClassComponentMetadataBaseClassName));
		UASClass* DerivedASClass = Cast<UASClass>(DerivedActorClass);
		UClass* RootComponentClass = FindGeneratedClass(&Engine, TEXT("UMetadataRootComponent"));
		UClass* BillboardComponentClass = FindGeneratedClass(&Engine, TEXT("UMetadataBillboardComponent"));
		UClass* ReplacementBillboardComponentClass = FindGeneratedClass(&Engine, TEXT("UMetadataReplacementBillboardComponent"));
		if (!CheckNotNull(*TestRunner, TEXT("ASClass component metadata test should resolve the generated base actor class"), BaseActorClass)
			|| !CheckNotNull(*TestRunner, TEXT("ASClass component metadata test should compile the derived actor to a UASClass"), DerivedASClass)
			|| !CheckNotNull(*TestRunner, TEXT("ASClass component metadata test should resolve the generated root component class"), RootComponentClass)
			|| !CheckNotNull(*TestRunner, TEXT("ASClass component metadata test should resolve the generated billboard component class"), BillboardComponentClass)
			|| !CheckNotNull(*TestRunner, TEXT("ASClass component metadata test should resolve the generated replacement component class"), ReplacementBillboardComponentClass))
		{ return; }

		const UASClass::FDefaultComponent* RootEntry = FindDefaultComponentEntryByName(BaseActorClass, ASClassRootComponentName);
		const UASClass::FDefaultComponent* BillboardEntry = FindDefaultComponentEntryByName(BaseActorClass, ASClassBillboardComponentName);
		const UASClass::FOverrideComponent* OverrideEntry = FindOverrideComponentEntryByVariableName(DerivedASClass, ASClassOverrideVariableName);

		ASSERT_THAT(AreEqual(2, BaseActorClass->DefaultComponents.Num(), TEXT("ASClass component metadata test should record exactly two default components on the base class")));
		ASSERT_THAT(AreEqual(1, DerivedASClass->OverrideComponents.Num(), TEXT("ASClass component metadata test should record exactly one override component on the derived class")));

		if (!CheckNotNull(*TestRunner, TEXT("ASClass component metadata test should record a root-scene default component entry"), RootEntry)
			|| !CheckNotNull(*TestRunner, TEXT("ASClass component metadata test should record a billboard default component entry"), BillboardEntry)
			|| !CheckNotNull(*TestRunner, TEXT("ASClass component metadata test should record the derived override entry"), OverrideEntry))
		{ return; }

		ASSERT_THAT(IsTrue(RootEntry->bIsRoot, TEXT("ASClass component metadata test should mark RootScene as the root component")));
		ASSERT_THAT(IsTrue(RootEntry->Attach.IsNone(), TEXT("ASClass component metadata test should keep RootScene unattached")));
		ASSERT_THAT(IsTrue(RootEntry->ComponentClass == RootComponentClass, TEXT("ASClass component metadata test should preserve the generated root component class")));
		ASSERT_THAT(IsFalse(BillboardEntry->bIsRoot, TEXT("ASClass component metadata test should keep Billboard out of the root slot")));
		ASSERT_THAT(AreEqual(ASClassRootComponentName, BillboardEntry->Attach, TEXT("ASClass component metadata test should attach Billboard to RootScene")));
		ASSERT_THAT(IsTrue(BillboardEntry->ComponentClass == BillboardComponentClass, TEXT("ASClass component metadata test should preserve the generated billboard component class")));
		ASSERT_THAT(AreEqual(ASClassBillboardComponentName, OverrideEntry->OverrideComponentName, TEXT("ASClass component metadata test should record which base component gets overridden")));
		ASSERT_THAT(AreEqual(ASClassOverrideVariableName, OverrideEntry->VariableName, TEXT("ASClass component metadata test should record the overriding property name")));
		ASSERT_THAT(IsTrue(OverrideEntry->ComponentClass == ReplacementBillboardComponentClass, TEXT("ASClass component metadata test should preserve the generated override component class")));
	}

	TEST_METHOD(SoftReloadPreservesDefaultComponentMetadataWithoutDuplication)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope EngineScope(Engine);

		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ASClassComponentMetadataSoftReloadModuleName.ToString());
		};

		const FString ScriptV1 = ASTEST_AS(R"AS(
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

		const FString ScriptV2 = ASTEST_AS(R"AS(
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

		UClass* InitialDerivedClass = AngelscriptFunctionalTestUtils::CompileScriptModule(*TestRunner, Engine, ASClassComponentMetadataSoftReloadModuleName, ASClassComponentMetadataSoftReloadFilename, ScriptV1, ASClassComponentMetadataSoftReloadDerivedClassName);
		if (InitialDerivedClass == nullptr) { return; }

		UASClass* InitialBaseClass = Cast<UASClass>(FindGeneratedClass(&Engine, ASClassComponentMetadataSoftReloadBaseClassName));
		UASClass* InitialDerivedASClass = Cast<UASClass>(InitialDerivedClass);
		UClass* InitialRootComponentClass = FindGeneratedClass(&Engine, ASClassSoftReloadRootComponentClassName);
		UClass* InitialReplacementComponentClass = FindGeneratedClass(&Engine, ASClassSoftReloadReplacementComponentClassName);
		if (!CheckNotNull(*TestRunner, TEXT("Soft-reload test should resolve base actor class"), InitialBaseClass)
			|| !CheckNotNull(*TestRunner, TEXT("Soft-reload test should compile derived as UASClass"), InitialDerivedASClass)
			|| !CheckNotNull(*TestRunner, TEXT("Soft-reload test should resolve root component class"), InitialRootComponentClass)
			|| !CheckNotNull(*TestRunner, TEXT("Soft-reload test should resolve replacement component class"), InitialReplacementComponentClass))
		{ return; }

		const TArray<FDefaultComponentMetadataSnapshot> InitialDefaultSnapshot = SnapshotDefaultComponentLayoutMetadata(InitialBaseClass);
		const TArray<FOverrideComponentMetadataSnapshot> InitialOverrideSnapshot = SnapshotOverrideComponentLayoutMetadata(InitialDerivedASClass);
		ASSERT_THAT(AreEqual(2, InitialDefaultSnapshot.Num(), TEXT("Soft-reload test should start with two default-component entries")));
		ASSERT_THAT(AreEqual(1, InitialOverrideSnapshot.Num(), TEXT("Soft-reload test should start with one override-component entry")));

		ECompileResult ReloadResult = ECompileResult::Error;
		if (!CheckTrue(
				*TestRunner,
				TEXT("Soft-reload test should compile the body-only update"),
				::CompileModuleWithResult(&Engine, ECompileType::SoftReloadOnly, ASClassComponentMetadataSoftReloadModuleName, ASClassComponentMetadataSoftReloadFilename, ScriptV2, ReloadResult)))
		{ return; }
		if (!CheckTrue(*TestRunner, TEXT("Soft-reload test should stay on a handled path"), IsHandledReloadResult(ReloadResult)))
		{ return; }

		UASClass* ReloadedBaseClass = Cast<UASClass>(FindGeneratedClass(&Engine, ASClassComponentMetadataSoftReloadBaseClassName));
		UASClass* ReloadedDerivedClass = Cast<UASClass>(FindGeneratedClass(&Engine, ASClassComponentMetadataSoftReloadDerivedClassName));
		UFunction* GetVersionAfterReload = ReloadedDerivedClass != nullptr ? FindGeneratedFunction(ReloadedDerivedClass, TEXT("GetVersion")) : nullptr;
		if (!CheckNotNull(*TestRunner, TEXT("Soft-reload test should still expose base class"), ReloadedBaseClass)
			|| !CheckNotNull(*TestRunner, TEXT("Soft-reload test should still expose derived class"), ReloadedDerivedClass)
			|| !CheckNotNull(*TestRunner, TEXT("Soft-reload test should still expose GetVersion"), GetVersionAfterReload))
		{ return; }

		ASSERT_THAT(IsTrue(ReloadedBaseClass == InitialBaseClass, TEXT("Soft-reload test should preserve base UASClass instance")));
		ASSERT_THAT(IsTrue(ReloadedDerivedClass == InitialDerivedASClass, TEXT("Soft-reload test should preserve derived UASClass instance")));

		const TArray<FDefaultComponentMetadataSnapshot> ReloadedDefaultSnapshot = SnapshotDefaultComponentLayoutMetadata(ReloadedBaseClass);
		const TArray<FOverrideComponentMetadataSnapshot> ReloadedOverrideSnapshot = SnapshotOverrideComponentLayoutMetadata(ReloadedDerivedClass);
		ASSERT_THAT(AreEqual(InitialDefaultSnapshot.Num(), ReloadedDefaultSnapshot.Num(), TEXT("Soft-reload test should keep default-component count stable")));
		ASSERT_THAT(AreEqual(InitialOverrideSnapshot.Num(), ReloadedOverrideSnapshot.Num(), TEXT("Soft-reload test should keep override-component count stable")));
		ASSERT_THAT(IsTrue(ReloadedDefaultSnapshot == InitialDefaultSnapshot, TEXT("Soft-reload test should preserve default-component metadata")));
		ASSERT_THAT(IsTrue(ReloadedOverrideSnapshot == InitialOverrideSnapshot, TEXT("Soft-reload test should preserve override-component metadata")));

		const UASClass::FDefaultComponent* RootEntryAfterReload = FindDefaultComponentEntryByName(ReloadedBaseClass, ASClassRootComponentName);
		const UASClass::FDefaultComponent* BillboardEntryAfterReload = FindDefaultComponentEntryByName(ReloadedBaseClass, ASClassBillboardComponentName);
		const UASClass::FOverrideComponent* OverrideEntryAfterReload = FindOverrideComponentEntryByVariableName(ReloadedDerivedClass, ASClassOverrideVariableName);
		if (!CheckNotNull(*TestRunner, TEXT("Soft-reload test should keep root metadata entry"), RootEntryAfterReload)
			|| !CheckNotNull(*TestRunner, TEXT("Soft-reload test should keep billboard metadata entry"), BillboardEntryAfterReload)
			|| !CheckNotNull(*TestRunner, TEXT("Soft-reload test should keep override metadata entry"), OverrideEntryAfterReload))
		{ return; }

		ASSERT_THAT(IsTrue(RootEntryAfterReload->bIsRoot, TEXT("Soft-reload test should keep RootScene as unique root")));
		ASSERT_THAT(IsTrue(RootEntryAfterReload->Attach.IsNone(), TEXT("Soft-reload test should keep RootScene unattached")));
		ASSERT_THAT(AreEqual(ASClassRootComponentName, BillboardEntryAfterReload->Attach, TEXT("Soft-reload test should keep Billboard attached to RootScene")));
		ASSERT_THAT(AreEqual(ASClassBillboardComponentName, OverrideEntryAfterReload->OverrideComponentName, TEXT("Soft-reload test should keep override pointed at Billboard")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* ReloadedActor = AngelscriptFunctionalTestUtils::SpawnScriptActor(*TestRunner, Spawner, ReloadedDerivedClass);
		if (!CheckNotNull(*TestRunner, TEXT("Soft-reload test should spawn the reloaded actor"), ReloadedActor)) { return; }

		USceneComponent* RuntimeRootComponent = ReloadedActor->GetRootComponent();
		USceneComponent* RuntimeBillboardComponent = FindSceneComponentByName(ReloadedActor, ASClassBillboardComponentName);
		if (!CheckNotNull(*TestRunner, TEXT("Soft-reload test should create runtime root component"), RuntimeRootComponent)
			|| !CheckNotNull(*TestRunner, TEXT("Soft-reload test should create overridden Billboard component"), RuntimeBillboardComponent))
		{ return; }

		ASSERT_THAT(IsTrue(RuntimeRootComponent->GetClass() == FindGeneratedClass(&Engine, ASClassSoftReloadRootComponentClassName), TEXT("Soft-reload test should keep root component class aligned with metadata")));
		ASSERT_THAT(IsTrue(RuntimeBillboardComponent->GetAttachParent() == RuntimeRootComponent, TEXT("Soft-reload test should keep Billboard attached to root")));

		int32 VersionAfterReload = 0;
		if (!CheckTrue(
				*TestRunner,
				TEXT("Soft-reload test should execute the reloaded function"),
				ExecuteGeneratedIntEventOnGameThread(&Engine, ReloadedActor, GetVersionAfterReload, VersionAfterReload)))
		{ return; }
		ASSERT_THAT(AreEqual(2, VersionAfterReload, TEXT("Soft-reload test should observe updated function body")));
	}
};

#endif
