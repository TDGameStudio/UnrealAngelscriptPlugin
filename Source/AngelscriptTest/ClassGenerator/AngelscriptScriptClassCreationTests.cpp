#include "AngelscriptFunctionalTestUtils.h"
#include "AngelscriptTestMacros.h"

#include "CQTest.h"
#include "Components/ActorTestSpawner.h"
#include "Engine/Blueprint.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/Guid.h"
#include "Misc/PackageName.h"
#include "Misc/ScopeExit.h"
#include "UObject/GarbageCollection.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UnrealType.h"

// Test Layer: UE Functional
#if WITH_ANGELSCRIPT_UNITTESTS

namespace ScriptClassCreationTest
{
	FAngelscriptEngine& AcquireFreshScriptClassEngine()
	{
		DestroySharedAndStrayGlobalTestEngine();
		return AcquireCleanSharedCloneEngine();
	}

	UBlueprint* CreateTransientBlueprintChild(
		FAutomationTestBase& Test,
		UClass* ParentClass,
		FStringView Suffix,
		const TCHAR* CallingContext = TEXT("AngelscriptScriptClassCreationTests"))
	{
		FNoDiscardAsserter LocalAssert(Test);
		if (!LocalAssert.IsNotNull(ParentClass, TEXT("Blueprint parent class should be valid")))
		{
			return nullptr;
		}

		const FString PackagePath = FString::Printf(
			TEXT("/Temp/AngelscriptScriptClass_%.*s_%s"),
			Suffix.Len(),
			Suffix.GetData(),
			*FGuid::NewGuid().ToString(EGuidFormats::Digits));
		UPackage* BlueprintPackage = CreatePackage(*PackagePath);
		if (!LocalAssert.IsNotNull(BlueprintPackage, TEXT("Transient blueprint package should be created")))
		{
			return nullptr;
		}

		BlueprintPackage->SetFlags(RF_Transient);
		const FName BlueprintName(*FPackageName::GetLongPackageAssetName(PackagePath));

		UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprint(
			ParentClass,
			BlueprintPackage,
			BlueprintName,
			BPTYPE_Normal,
			UBlueprint::StaticClass(),
			UBlueprintGeneratedClass::StaticClass(),
			CallingContext);
		if (!LocalAssert.IsNotNull(Blueprint, TEXT("Transient blueprint asset should be created")))
		{
			return nullptr;
		}

		return Blueprint;
	}

	bool CompileAndValidateBlueprint(FAutomationTestBase& Test, UBlueprint& Blueprint)
	{
		FKismetEditorUtilities::CompileBlueprint(&Blueprint);
		FNoDiscardAsserter LocalAssert(Test);
		return LocalAssert.IsNotNull(Blueprint.GeneratedClass.Get(), TEXT("Blueprint should compile to a generated class"));
	}

	void CleanupBlueprint(UBlueprint*& Blueprint)
	{
		if (Blueprint == nullptr)
		{
			return;
		}

		if (UClass* BlueprintClass = Blueprint->GeneratedClass)
		{
			BlueprintClass->MarkAsGarbage();
		}

		if (UPackage* BlueprintPackage = Blueprint->GetOutermost())
		{
			BlueprintPackage->MarkAsGarbage();
		}

		Blueprint->MarkAsGarbage();
		CollectGarbage(RF_NoFlags, true);
		Blueprint = nullptr;
	}

	struct FScopedTransientBlueprint
	{
		UBlueprint* BlueprintAsset = nullptr;

		~FScopedTransientBlueprint()
		{
			CleanupBlueprint(BlueprintAsset);
		}

		UClass* GetGeneratedClass() const
		{
			return BlueprintAsset != nullptr ? BlueprintAsset->GeneratedClass.Get() : nullptr;
		}
	};
}

TEST_CLASS_WITH_FLAGS(FAngelscriptScriptClassCreationTests,
	"Angelscript.TestModule.ScriptClass",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	TEST_METHOD(CompilesToUClass)
	{
		FAngelscriptEngine& Engine = ScriptClassCreationTest::AcquireFreshScriptClassEngine();
		FAngelscriptEngineScope EngineScope(Engine);
		static const FName ModuleName(TEXT("TestScriptClassCompilesToUClass"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
			ResetSharedCloneEngine(Engine);
		};

		UClass* ScriptClass = AngelscriptFunctionalTestUtils::CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("TestScriptClassCompilesToUClass.as"),
			ASTEST_AS(R"AS(
				UCLASS()
				class ATestScriptClassCompilesToUClass : AActor
				{
					UPROPERTY()
					int SpawnMarker = 7;
				}
				)AS"),
			TEXT("ATestScriptClassCompilesToUClass"));
		if (ScriptClass == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsTrue(ScriptClass->IsChildOf(AActor::StaticClass()), TEXT("Script-class compile test case should produce an actor-derived generated UClass")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = AngelscriptFunctionalTestUtils::SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		if (Actor == nullptr)
		{
			return;
		}

		int32 SpawnMarker = 0;
		if (!AngelscriptFunctionalTestUtils::ReadPropertyValue<FIntProperty>(*TestRunner, Actor, TEXT("SpawnMarker"), SpawnMarker))
		{
			return;
		}

		ASSERT_THAT(AreEqual(7, SpawnMarker, TEXT("Script-class compile test case should instantiate an actor with script property defaults")));
	}

	TEST_METHOD(CanSpawnInTestWorld)
	{
		FAngelscriptEngine& Engine = ScriptClassCreationTest::AcquireFreshScriptClassEngine();
		FAngelscriptEngineScope EngineScope(Engine);
		static const FName ModuleName(TEXT("TestScriptClassCanSpawnInTestWorld"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
			ResetSharedCloneEngine(Engine);
		};

		UClass* ScriptClass = AngelscriptFunctionalTestUtils::CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("TestScriptClassCanSpawnInTestWorld.as"),
			ASTEST_AS(R"AS(
				UCLASS()
				class ATestScriptClassCanSpawnInTestWorld : AActor
				{
					UPROPERTY()
					int BeginPlayObserved = 0;

					UFUNCTION(BlueprintOverride)
					void BeginPlay()
					{
						BeginPlayObserved = 1;
					}
				}
				)AS"),
			TEXT("ATestScriptClassCanSpawnInTestWorld"));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = AngelscriptFunctionalTestUtils::SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		if (Actor == nullptr)
		{
			return;
		}

		AngelscriptFunctionalTestUtils::BeginPlayActor(Engine, *Actor);

		int32 BeginPlayObserved = 0;
		if (!AngelscriptFunctionalTestUtils::ReadPropertyValue<FIntProperty>(*TestRunner, Actor, TEXT("BeginPlayObserved"), BeginPlayObserved))
		{
			return;
		}

		ASSERT_THAT(AreEqual(1, BeginPlayObserved, TEXT("Script-class spawn test case should observe BeginPlay after entering the test world")));
	}

	TEST_METHOD(MultiSpawnKeepsStateIsolation)
	{
		FAngelscriptEngine& Engine = ScriptClassCreationTest::AcquireFreshScriptClassEngine();
		FAngelscriptEngineScope EngineScope(Engine);
		static const FName ModuleName(TEXT("TestScriptClassMultiSpawnKeepsStateIsolation"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
			ResetSharedCloneEngine(Engine);
		};

		UClass* ScriptClass = AngelscriptFunctionalTestUtils::CompileScriptModule(
			*TestRunner, Engine, ModuleName,
			TEXT("TestScriptClassMultiSpawnKeepsStateIsolation.as"),
			ASTEST_AS(R"AS(
				UCLASS()
				class ATestScriptClassMultiSpawnKeepsStateIsolation : AActor
				{
					UPROPERTY()
					int LocalState = 3;
				}
				)AS"),
			TEXT("ATestScriptClassMultiSpawnKeepsStateIsolation"));
		if (ScriptClass == nullptr) { return; }

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* FirstActor = AngelscriptFunctionalTestUtils::SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		AActor* SecondActor = AngelscriptFunctionalTestUtils::SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(FirstActor, TEXT("State-isolation test case should spawn first actor")));
		ASSERT_THAT(IsNotNull(SecondActor, TEXT("State-isolation test case should spawn second actor")));
		if (FirstActor == nullptr || SecondActor == nullptr)
		{ return; }

		FIntProperty* LocalStateProperty = FindFProperty<FIntProperty>(ScriptClass, TEXT("LocalState"));
		ASSERT_THAT(IsNotNull(LocalStateProperty, TEXT("State-isolation test case should expose LocalState property")));
		if (LocalStateProperty == nullptr)
		{ return; }

		LocalStateProperty->SetPropertyValue_InContainer(FirstActor, 11);

		int32 FirstValue = 0;
		if (!AngelscriptFunctionalTestUtils::ReadPropertyValue<FIntProperty>(*TestRunner, FirstActor, TEXT("LocalState"), FirstValue)) { return; }
		int32 SecondValue = 0;
		if (!AngelscriptFunctionalTestUtils::ReadPropertyValue<FIntProperty>(*TestRunner, SecondActor, TEXT("LocalState"), SecondValue)) { return; }

		ASSERT_THAT(IsTrue(FirstActor != SecondActor, TEXT("State-isolation test case should spawn distinct actor instances")));
		ASSERT_THAT(AreEqual(11, FirstValue, TEXT("State-isolation test case should keep the mutated value on the first actor")));
		ASSERT_THAT(AreEqual(3, SecondValue, TEXT("State-isolation test case should keep the second actor at its own default state")));
	}

	TEST_METHOD(BlueprintChildCompiles)
	{
		FAngelscriptEngine& Engine = ScriptClassCreationTest::AcquireFreshScriptClassEngine();
		FAngelscriptEngineScope EngineScope(Engine);
		static const FName ModuleName(TEXT("TestScriptClassBlueprintChildCompiles"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
			ResetSharedCloneEngine(Engine);
		};

		UClass* ScriptClass = AngelscriptFunctionalTestUtils::CompileScriptModule(*TestRunner, Engine, ModuleName,
			TEXT("TestScriptClassBlueprintChildCompiles.as"),
			ASTEST_AS(R"AS(
				UCLASS()
				class ATestScriptClassBlueprintChildCompiles : AActor
				{
					UPROPERTY()
					int BeginPlayCount = 0;

					UFUNCTION(BlueprintOverride)
					void BeginPlay()
					{
						BeginPlayCount += 1;
					}
				}
				)AS"),
			TEXT("ATestScriptClassBlueprintChildCompiles"));
		if (ScriptClass == nullptr) { return; }

		ScriptClassCreationTest::FScopedTransientBlueprint Blueprint;
		Blueprint.BlueprintAsset = ScriptClassCreationTest::CreateTransientBlueprintChild(*TestRunner, ScriptClass, TEXT("ScriptClassBlueprintChild"));
		if (Blueprint.BlueprintAsset == nullptr) { return; }
		if (!ScriptClassCreationTest::CompileAndValidateBlueprint(*TestRunner, *Blueprint.BlueprintAsset)) { return; }

		UClass* BlueprintClass = Blueprint.GetGeneratedClass();
		ASSERT_THAT(IsNotNull(BlueprintClass, TEXT("Blueprint-child test case should provide a generated blueprint class")));
		if (BlueprintClass == nullptr) { return; }

		ASSERT_THAT(IsTrue(BlueprintClass->IsChildOf(ScriptClass), TEXT("Blueprint-child test case should generate a blueprint class inheriting from the script parent")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = AngelscriptFunctionalTestUtils::SpawnScriptActor(*TestRunner, Spawner, BlueprintClass);
		if (Actor == nullptr) { return; }

		AngelscriptFunctionalTestUtils::BeginPlayActor(Engine, *Actor);

		int32 BeginPlayCount = 0;
		if (!AngelscriptFunctionalTestUtils::ReadPropertyValue<FIntProperty>(*TestRunner, Actor, TEXT("BeginPlayCount"), BeginPlayCount)) { return; }

		ASSERT_THAT(AreEqual(1, BeginPlayCount, TEXT("Blueprint-child test case should preserve the script BeginPlay override when spawned")));
	}

	TEST_METHOD(CDOHasExpectedDefaults)
	{
		FAngelscriptEngine& Engine = ScriptClassCreationTest::AcquireFreshScriptClassEngine();
		FAngelscriptEngineScope EngineScope(Engine);
		static const FName ModuleName(TEXT("TestScriptClassCDOHasExpectedDefaults"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
			ResetSharedCloneEngine(Engine);
		};

		UClass* ScriptClass = AngelscriptFunctionalTestUtils::CompileScriptModule(*TestRunner, Engine, ModuleName,
			TEXT("TestScriptClassCDOHasExpectedDefaults.as"),
			ASTEST_AS(R"AS(
				UCLASS()
				class ATestScriptClassCDOHasExpectedDefaults : AActor
				{
					UPROPERTY()
					int DefaultCounter = 21;

					UPROPERTY()
					bool bDefaultFlag = true;

					UPROPERTY()
					FString DefaultLabel = "CDOStable";
				}
				)AS"),
			TEXT("ATestScriptClassCDOHasExpectedDefaults"));
		if (ScriptClass == nullptr) { return; }

		UObject* DefaultObject = ScriptClass->GetDefaultObject();
		ASSERT_THAT(IsNotNull(DefaultObject, TEXT("CDO-defaults test case should provide a generated class default object")));
		if (DefaultObject == nullptr) { return; }

		int32 DefaultCounter = 0;
		if (!AngelscriptFunctionalTestUtils::ReadPropertyValue<FIntProperty>(*TestRunner, DefaultObject, TEXT("DefaultCounter"), DefaultCounter)) { return; }
		bool bDefaultFlag = false;
		if (!AngelscriptFunctionalTestUtils::ReadPropertyValue<FBoolProperty>(*TestRunner, DefaultObject, TEXT("bDefaultFlag"), bDefaultFlag)) { return; }
		FString DefaultLabel;
		if (!AngelscriptFunctionalTestUtils::ReadPropertyValue<FStrProperty>(*TestRunner, DefaultObject, TEXT("DefaultLabel"), DefaultLabel)) { return; }

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* SpawnedActor = AngelscriptFunctionalTestUtils::SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		if (SpawnedActor == nullptr) { return; }

		int32 SpawnedDefaultCounter = 0;
		if (!AngelscriptFunctionalTestUtils::ReadPropertyValue<FIntProperty>(*TestRunner, SpawnedActor, TEXT("DefaultCounter"), SpawnedDefaultCounter)) { return; }
		FString SpawnedDefaultLabel;
		if (!AngelscriptFunctionalTestUtils::ReadPropertyValue<FStrProperty>(*TestRunner, SpawnedActor, TEXT("DefaultLabel"), SpawnedDefaultLabel)) { return; }

		ASSERT_THAT(AreEqual(21, DefaultCounter, TEXT("CDO-defaults test case should preserve integer defaults on the class default object")));
		ASSERT_THAT(IsTrue(bDefaultFlag, TEXT("CDO-defaults test case should preserve boolean defaults on the class default object")));
		ASSERT_THAT(AreEqual(FString(TEXT("CDOStable")), DefaultLabel, TEXT("CDO-defaults test case should preserve string defaults on the class default object")));
		ASSERT_THAT(AreEqual(21, SpawnedDefaultCounter, TEXT("CDO-defaults test case should apply class default integer values to spawned actor instances")));
		ASSERT_THAT(AreEqual(FString(TEXT("CDOStable")), SpawnedDefaultLabel, TEXT("CDO-defaults test case should apply class default string values to spawned actor instances")));
	}

	TEST_METHOD(RecompileDoesNotCrashClassSwitch)
	{
		FAngelscriptEngine& Engine = ScriptClassCreationTest::AcquireFreshScriptClassEngine();
		FAngelscriptEngineScope EngineScope(Engine);
		static const FName ModuleName(TEXT("TestScriptClassRecompileDoesNotCrashClassSwitch"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
			ResetSharedCloneEngine(Engine);
		};

		UClass* InitialClass = AngelscriptFunctionalTestUtils::CompileScriptModule(*TestRunner, Engine, ModuleName,
			TEXT("TestScriptClassRecompileDoesNotCrashClassSwitch.as"),
			ASTEST_AS(R"AS(
				UCLASS()
				class ATestScriptClassRecompileDoesNotCrashClassSwitch : AActor
				{
					UPROPERTY()
					int GenerationValue = 1;
				}
				)AS"),
			TEXT("ATestScriptClassRecompileDoesNotCrashClassSwitch"));
		if (InitialClass == nullptr) { return; }

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* FirstGenerationActor = AngelscriptFunctionalTestUtils::SpawnScriptActor(*TestRunner, Spawner, InitialClass);
		if (FirstGenerationActor == nullptr) { return; }

		int32 InitialGenerationValue = 0;
		if (!AngelscriptFunctionalTestUtils::ReadPropertyValue<FIntProperty>(*TestRunner, FirstGenerationActor, TEXT("GenerationValue"), InitialGenerationValue)) { return; }

		UClass* RecompiledClass = AngelscriptFunctionalTestUtils::CompileScriptModule(*TestRunner, Engine, ModuleName,
			TEXT("TestScriptClassRecompileDoesNotCrashClassSwitch.as"),
			ASTEST_AS(R"AS(
				UCLASS()
				class ATestScriptClassRecompileDoesNotCrashClassSwitch : AActor
				{
					UPROPERTY()
					int GenerationValue = 2;

					UPROPERTY()
					int AddedAfterRecompile = 17;
				}
				)AS"),
			TEXT("ATestScriptClassRecompileDoesNotCrashClassSwitch"));
		if (RecompiledClass == nullptr) { return; }

		AActor* RecompiledActor = AngelscriptFunctionalTestUtils::SpawnScriptActor(*TestRunner, Spawner, RecompiledClass);
		if (RecompiledActor == nullptr) { return; }

		int32 RecompiledGenerationValue = 0;
		if (!AngelscriptFunctionalTestUtils::ReadPropertyValue<FIntProperty>(*TestRunner, RecompiledActor, TEXT("GenerationValue"), RecompiledGenerationValue)) { return; }
		int32 AddedAfterRecompile = 0;
		if (!AngelscriptFunctionalTestUtils::ReadPropertyValue<FIntProperty>(*TestRunner, RecompiledActor, TEXT("AddedAfterRecompile"), AddedAfterRecompile)) { return; }

		ASSERT_THAT(AreEqual(1, InitialGenerationValue, TEXT("Recompile test case should produce the initial default before class switch")));
		ASSERT_THAT(AreEqual(2, RecompiledGenerationValue, TEXT("Recompile test case should expose updated defaults after recompiling the same script class")));
		ASSERT_THAT(AreEqual(17, AddedAfterRecompile, TEXT("Recompile test case should expose newly added reflected properties after class switch")));
	}

	TEST_METHOD(NonUClassTypeCannotSpawn)
	{
		FAngelscriptEngine& Engine = ScriptClassCreationTest::AcquireFreshScriptClassEngine();
		FAngelscriptEngineScope EngineScope(Engine);
		static const FName ModuleName(TEXT("TestScriptClassNonUClassTypeCannotSpawn"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
			ResetSharedCloneEngine(Engine);
		};

		UClass* NonActorClass = AngelscriptFunctionalTestUtils::CompileScriptModule(*TestRunner, Engine, ModuleName,
			TEXT("TestScriptClassNonUClassTypeCannotSpawn.as"),
			ASTEST_AS(R"AS(
				UCLASS()
				class UTestScriptClassNonUClassTypeCannotSpawn : UObject
				{
					UPROPERTY()
					int Value = 5;
				}
				)AS"),
			TEXT("UTestScriptClassNonUClassTypeCannotSpawn"));
		if (NonActorClass == nullptr) { return; }

		ASSERT_THAT(IsFalse(NonActorClass->IsChildOf(AActor::StaticClass()), TEXT("Non-uclass-type spawn test case should compile a generated class that is not actor-derived")));

		UObject* ObjectInstance = NewObject<UObject>(GetTransientPackage(), NonActorClass);
		ASSERT_THAT(IsNotNull(ObjectInstance, TEXT("Non-uclass-type spawn test case should still allow plain UObject creation")));
		if (ObjectInstance == nullptr) { return; }

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		FActorSpawnParameters SpawnParameters;
		AActor* SpawnedActor = Spawner.GetWorld().SpawnActor<AActor>(NonActorClass, FVector::ZeroVector, FRotator::ZeroRotator, SpawnParameters);
		ASSERT_THAT(IsNull(SpawnedActor, TEXT("Non-uclass-type spawn test case should reject spawning non-actor generated classes into the world")));
	}

	TEST_METHOD(RenameReplacesOldClass)
	{
		FAngelscriptEngine& Engine = ScriptClassCreationTest::AcquireFreshScriptClassEngine();
		FAngelscriptEngineScope EngineScope(Engine);
		static const FName ModuleName(TEXT("TestScriptClassRenameReplacesOldClass"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
			ResetSharedCloneEngine(Engine);
		};

		UClass* OldClass = AngelscriptFunctionalTestUtils::CompileScriptModule(*TestRunner, Engine, ModuleName,
			TEXT("TestScriptClassRenameReplacesOldClass.as"),
			ASTEST_AS(R"AS(
				UCLASS()
				class ATestScriptClassRenameOld : AActor
				{
					UPROPERTY()
					int Version = 1;
				}
				)AS"),
			TEXT("ATestScriptClassRenameOld"));
		if (OldClass == nullptr) { return; }

		UClass* NewClass = AngelscriptFunctionalTestUtils::CompileScriptModule(*TestRunner, Engine, ModuleName,
			TEXT("TestScriptClassRenameReplacesOldClass.as"),
			ASTEST_AS(R"AS(
				UCLASS()
				class ATestScriptClassRenameNew : AActor
				{
					UPROPERTY()
					int Version = 2;
				}
				)AS"),
			TEXT("ATestScriptClassRenameNew"));
		ASSERT_THAT(IsNotNull(NewClass, TEXT("Rename test case should compile the renamed generated class")));
		if (NewClass == nullptr) { return; }

		ASSERT_THAT(IsTrue(::FindGeneratedClass(&Engine, TEXT("ATestScriptClassRenameNew")) == NewClass, TEXT("Rename test case should expose the new generated class by its new name")));
		ASSERT_THAT(IsTrue(OldClass != NewClass, TEXT("Rename test case should keep the old generated class address distinct from the new class")));
		ASSERT_THAT(IsTrue(OldClass->GetName().Contains(TEXT("REPLACED")) || OldClass->GetName() != TEXT("ATestScriptClassRenameOld"), TEXT("Rename test case should move the old generated class out of the active class name")));

		FIntProperty* VersionProperty = FindFProperty<FIntProperty>(NewClass, TEXT("Version"));
		ASSERT_THAT(IsNotNull(VersionProperty, TEXT("Rename test case should expose the new reflected property on the renamed class")));
		if (VersionProperty == nullptr) { return; }

		ASSERT_THAT(AreEqual(2, VersionProperty->GetPropertyValue_InContainer(NewClass->GetDefaultObject()), TEXT("Rename test case should apply the renamed class default value after replacement")));
	}
};

#endif
