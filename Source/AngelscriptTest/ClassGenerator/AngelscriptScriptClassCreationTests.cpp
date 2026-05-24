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
#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptFunctionalTestUtils;

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
		if (!Test.TestNotNull(TEXT("Blueprint parent class should be valid"), ParentClass))
		{
			return nullptr;
		}

		const FString PackagePath = FString::Printf(
			TEXT("/Temp/AngelscriptScriptClass_%.*s_%s"),
			Suffix.Len(),
			Suffix.GetData(),
			*FGuid::NewGuid().ToString(EGuidFormats::Digits));
		UPackage* BlueprintPackage = CreatePackage(*PackagePath);
		if (!Test.TestNotNull(TEXT("Transient blueprint package should be created"), BlueprintPackage))
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
		if (!Test.TestNotNull(TEXT("Transient blueprint asset should be created"), Blueprint))
		{
			return nullptr;
		}

		return Blueprint;
	}

	bool CompileAndValidateBlueprint(FAutomationTestBase& Test, UBlueprint& Blueprint)
	{
		FKismetEditorUtilities::CompileBlueprint(&Blueprint);
		return Test.TestNotNull(TEXT("Blueprint should compile to a generated class"), Blueprint.GeneratedClass.Get());
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
	TEST_METHOD(CompilesToUClass)
	{
		using namespace ScriptClassCreationTest;
		FAngelscriptEngine& Engine = ScriptClassCreationTest::AcquireFreshScriptClassEngine();
		FAngelscriptEngineScope EngineScope(Engine);
		static const FName ModuleName(TEXT("TestScriptClassCompilesToUClass"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
			ASTEST_RESET_ENGINE(Engine);
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("TestScriptClassCompilesToUClass.as"),
			TEXT(R"AS(
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

		TestRunner->TestTrue(TEXT("Script-class compile test case should produce an actor-derived generated UClass"), ScriptClass->IsChildOf(AActor::StaticClass()));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		if (Actor == nullptr)
		{
			return;
		}

		int32 SpawnMarker = 0;
		if (!ReadPropertyValue<FIntProperty>(*TestRunner, Actor, TEXT("SpawnMarker"), SpawnMarker))
		{
			return;
		}

		TestRunner->TestEqual(TEXT("Script-class compile test case should instantiate an actor with script property defaults"), SpawnMarker, 7);
	}

	TEST_METHOD(CanSpawnInTestWorld)
	{
		using namespace ScriptClassCreationTest;
		FAngelscriptEngine& Engine = ScriptClassCreationTest::AcquireFreshScriptClassEngine();
		FAngelscriptEngineScope EngineScope(Engine);
		static const FName ModuleName(TEXT("TestScriptClassCanSpawnInTestWorld"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
			ASTEST_RESET_ENGINE(Engine);
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("TestScriptClassCanSpawnInTestWorld.as"),
			TEXT(R"AS(
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
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		if (Actor == nullptr)
		{
			return;
		}

		BeginPlayActor(Engine, *Actor);

		int32 BeginPlayObserved = 0;
		if (!ReadPropertyValue<FIntProperty>(*TestRunner, Actor, TEXT("BeginPlayObserved"), BeginPlayObserved))
		{
			return;
		}

		TestRunner->TestEqual(TEXT("Script-class spawn test case should observe BeginPlay after entering the test world"), BeginPlayObserved, 1);
	}

	TEST_METHOD(MultiSpawnKeepsStateIsolation)
	{
		using namespace ScriptClassCreationTest;
		FAngelscriptEngine& Engine = ScriptClassCreationTest::AcquireFreshScriptClassEngine();
		FAngelscriptEngineScope EngineScope(Engine);
		static const FName ModuleName(TEXT("TestScriptClassMultiSpawnKeepsStateIsolation"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
			ASTEST_RESET_ENGINE(Engine);
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner, Engine, ModuleName,
			TEXT("TestScriptClassMultiSpawnKeepsStateIsolation.as"),
			TEXT(R"AS(
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
		AActor* FirstActor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		AActor* SecondActor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		if (!TestRunner->TestNotNull(TEXT("State-isolation test case should spawn first actor"), FirstActor)
			|| !TestRunner->TestNotNull(TEXT("State-isolation test case should spawn second actor"), SecondActor))
		{ return; }

		FIntProperty* LocalStateProperty = FindFProperty<FIntProperty>(ScriptClass, TEXT("LocalState"));
		if (!TestRunner->TestNotNull(TEXT("State-isolation test case should expose LocalState property"), LocalStateProperty))
		{ return; }

		LocalStateProperty->SetPropertyValue_InContainer(FirstActor, 11);

		int32 FirstValue = 0;
		if (!ReadPropertyValue<FIntProperty>(*TestRunner, FirstActor, TEXT("LocalState"), FirstValue)) { return; }
		int32 SecondValue = 0;
		if (!ReadPropertyValue<FIntProperty>(*TestRunner, SecondActor, TEXT("LocalState"), SecondValue)) { return; }

		TestRunner->TestTrue(TEXT("State-isolation test case should spawn distinct actor instances"), FirstActor != SecondActor);
		TestRunner->TestEqual(TEXT("State-isolation test case should keep the mutated value on the first actor"), FirstValue, 11);
		TestRunner->TestEqual(TEXT("State-isolation test case should keep the second actor at its own default state"), SecondValue, 3);
	}

	TEST_METHOD(BlueprintChildCompiles)
	{
		using namespace ScriptClassCreationTest;
		FAngelscriptEngine& Engine = ScriptClassCreationTest::AcquireFreshScriptClassEngine();
		FAngelscriptEngineScope EngineScope(Engine);
		static const FName ModuleName(TEXT("TestScriptClassBlueprintChildCompiles"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
			ASTEST_RESET_ENGINE(Engine);
		};

		UClass* ScriptClass = CompileScriptModule(*TestRunner, Engine, ModuleName,
			TEXT("TestScriptClassBlueprintChildCompiles.as"),
			TEXT(R"AS(
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
		if (!TestRunner->TestNotNull(TEXT("Blueprint-child test case should provide a generated blueprint class"), BlueprintClass)) { return; }

		TestRunner->TestTrue(TEXT("Blueprint-child test case should generate a blueprint class inheriting from the script parent"), BlueprintClass->IsChildOf(ScriptClass));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, BlueprintClass);
		if (Actor == nullptr) { return; }

		BeginPlayActor(Engine, *Actor);

		int32 BeginPlayCount = 0;
		if (!ReadPropertyValue<FIntProperty>(*TestRunner, Actor, TEXT("BeginPlayCount"), BeginPlayCount)) { return; }

		TestRunner->TestEqual(TEXT("Blueprint-child test case should preserve the script BeginPlay override when spawned"), BeginPlayCount, 1);
	}

	TEST_METHOD(CDOHasExpectedDefaults)
	{
		using namespace ScriptClassCreationTest;
		FAngelscriptEngine& Engine = ScriptClassCreationTest::AcquireFreshScriptClassEngine();
		FAngelscriptEngineScope EngineScope(Engine);
		static const FName ModuleName(TEXT("TestScriptClassCDOHasExpectedDefaults"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
			ASTEST_RESET_ENGINE(Engine);
		};

		UClass* ScriptClass = CompileScriptModule(*TestRunner, Engine, ModuleName,
			TEXT("TestScriptClassCDOHasExpectedDefaults.as"),
			TEXT(R"AS(
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
		if (!TestRunner->TestNotNull(TEXT("CDO-defaults test case should provide a generated class default object"), DefaultObject)) { return; }

		int32 DefaultCounter = 0;
		if (!ReadPropertyValue<FIntProperty>(*TestRunner, DefaultObject, TEXT("DefaultCounter"), DefaultCounter)) { return; }
		bool bDefaultFlag = false;
		if (!ReadPropertyValue<FBoolProperty>(*TestRunner, DefaultObject, TEXT("bDefaultFlag"), bDefaultFlag)) { return; }
		FString DefaultLabel;
		if (!ReadPropertyValue<FStrProperty>(*TestRunner, DefaultObject, TEXT("DefaultLabel"), DefaultLabel)) { return; }

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* SpawnedActor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		if (SpawnedActor == nullptr) { return; }

		int32 SpawnedDefaultCounter = 0;
		if (!ReadPropertyValue<FIntProperty>(*TestRunner, SpawnedActor, TEXT("DefaultCounter"), SpawnedDefaultCounter)) { return; }
		FString SpawnedDefaultLabel;
		if (!ReadPropertyValue<FStrProperty>(*TestRunner, SpawnedActor, TEXT("DefaultLabel"), SpawnedDefaultLabel)) { return; }

		TestRunner->TestEqual(TEXT("CDO-defaults test case should preserve integer defaults on the class default object"), DefaultCounter, 21);
		TestRunner->TestTrue(TEXT("CDO-defaults test case should preserve boolean defaults on the class default object"), bDefaultFlag);
		TestRunner->TestEqual(TEXT("CDO-defaults test case should preserve string defaults on the class default object"), DefaultLabel, FString(TEXT("CDOStable")));
		TestRunner->TestEqual(TEXT("CDO-defaults test case should apply class default integer values to spawned actor instances"), SpawnedDefaultCounter, 21);
		TestRunner->TestEqual(TEXT("CDO-defaults test case should apply class default string values to spawned actor instances"), SpawnedDefaultLabel, FString(TEXT("CDOStable")));
	}

	TEST_METHOD(RecompileDoesNotCrashClassSwitch)
	{
		using namespace ScriptClassCreationTest;
		FAngelscriptEngine& Engine = ScriptClassCreationTest::AcquireFreshScriptClassEngine();
		FAngelscriptEngineScope EngineScope(Engine);
		static const FName ModuleName(TEXT("TestScriptClassRecompileDoesNotCrashClassSwitch"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
			ASTEST_RESET_ENGINE(Engine);
		};

		UClass* InitialClass = CompileScriptModule(*TestRunner, Engine, ModuleName,
			TEXT("TestScriptClassRecompileDoesNotCrashClassSwitch.as"),
			TEXT(R"AS(
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
		AActor* FirstGenerationActor = SpawnScriptActor(*TestRunner, Spawner, InitialClass);
		if (FirstGenerationActor == nullptr) { return; }

		int32 InitialGenerationValue = 0;
		if (!ReadPropertyValue<FIntProperty>(*TestRunner, FirstGenerationActor, TEXT("GenerationValue"), InitialGenerationValue)) { return; }

		UClass* RecompiledClass = CompileScriptModule(*TestRunner, Engine, ModuleName,
			TEXT("TestScriptClassRecompileDoesNotCrashClassSwitch.as"),
			TEXT(R"AS(
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

		AActor* RecompiledActor = SpawnScriptActor(*TestRunner, Spawner, RecompiledClass);
		if (RecompiledActor == nullptr) { return; }

		int32 RecompiledGenerationValue = 0;
		if (!ReadPropertyValue<FIntProperty>(*TestRunner, RecompiledActor, TEXT("GenerationValue"), RecompiledGenerationValue)) { return; }
		int32 AddedAfterRecompile = 0;
		if (!ReadPropertyValue<FIntProperty>(*TestRunner, RecompiledActor, TEXT("AddedAfterRecompile"), AddedAfterRecompile)) { return; }

		TestRunner->TestEqual(TEXT("Recompile test case should produce the initial default before class switch"), InitialGenerationValue, 1);
		TestRunner->TestEqual(TEXT("Recompile test case should expose updated defaults after recompiling the same script class"), RecompiledGenerationValue, 2);
		TestRunner->TestEqual(TEXT("Recompile test case should expose newly added reflected properties after class switch"), AddedAfterRecompile, 17);
	}

	TEST_METHOD(NonUClassTypeCannotSpawn)
	{
		using namespace ScriptClassCreationTest;
		FAngelscriptEngine& Engine = ScriptClassCreationTest::AcquireFreshScriptClassEngine();
		FAngelscriptEngineScope EngineScope(Engine);
		static const FName ModuleName(TEXT("TestScriptClassNonUClassTypeCannotSpawn"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
			ASTEST_RESET_ENGINE(Engine);
		};

		UClass* NonActorClass = CompileScriptModule(*TestRunner, Engine, ModuleName,
			TEXT("TestScriptClassNonUClassTypeCannotSpawn.as"),
			TEXT(R"AS(
UCLASS()
class UTestScriptClassNonUClassTypeCannotSpawn : UObject
{
	UPROPERTY()
	int Value = 5;
}
)AS"),
			TEXT("UTestScriptClassNonUClassTypeCannotSpawn"));
		if (NonActorClass == nullptr) { return; }

		TestRunner->TestFalse(TEXT("Non-uclass-type spawn test case should compile a generated class that is not actor-derived"), NonActorClass->IsChildOf(AActor::StaticClass()));

		UObject* ObjectInstance = NewObject<UObject>(GetTransientPackage(), NonActorClass);
		if (!TestRunner->TestNotNull(TEXT("Non-uclass-type spawn test case should still allow plain UObject creation"), ObjectInstance)) { return; }

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		FActorSpawnParameters SpawnParameters;
		AActor* SpawnedActor = Spawner.GetWorld().SpawnActor<AActor>(NonActorClass, FVector::ZeroVector, FRotator::ZeroRotator, SpawnParameters);
		TestRunner->TestNull(TEXT("Non-uclass-type spawn test case should reject spawning non-actor generated classes into the world"), SpawnedActor);
	}

	TEST_METHOD(RenameReplacesOldClass)
	{
		using namespace ScriptClassCreationTest;
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine);
		static const FName ModuleName(TEXT("TestScriptClassRenameReplacesOldClass"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
			ResetSharedCloneEngine(Engine);
		};

		UClass* OldClass = CompileScriptModule(*TestRunner, Engine, ModuleName,
			TEXT("TestScriptClassRenameReplacesOldClass.as"),
			TEXT(R"AS(
UCLASS()
class ATestScriptClassRenameOld : AActor
{
	UPROPERTY()
	int Version = 1;
}
)AS"),
			TEXT("ATestScriptClassRenameOld"));
		if (OldClass == nullptr) { return; }

		UClass* NewClass = CompileScriptModule(*TestRunner, Engine, ModuleName,
			TEXT("TestScriptClassRenameReplacesOldClass.as"),
			TEXT(R"AS(
UCLASS()
class ATestScriptClassRenameNew : AActor
{
	UPROPERTY()
	int Version = 2;
}
)AS"),
			TEXT("ATestScriptClassRenameNew"));
		if (!TestRunner->TestNotNull(TEXT("Rename test case should compile the renamed generated class"), NewClass)) { return; }

		TestRunner->TestTrue(TEXT("Rename test case should expose the new generated class by its new name"), FindGeneratedClass(&Engine, TEXT("ATestScriptClassRenameNew")) == NewClass);
		TestRunner->TestTrue(TEXT("Rename test case should keep the old generated class address distinct from the new class"), OldClass != NewClass);
		TestRunner->TestTrue(TEXT("Rename test case should move the old generated class out of the active class name"), OldClass->GetName().Contains(TEXT("REPLACED")) || OldClass->GetName() != TEXT("ATestScriptClassRenameOld"));

		FIntProperty* VersionProperty = FindFProperty<FIntProperty>(NewClass, TEXT("Version"));
		if (!TestRunner->TestNotNull(TEXT("Rename test case should expose the new reflected property on the renamed class"), VersionProperty)) { return; }

		TestRunner->TestEqual(TEXT("Rename test case should apply the renamed class default value after replacement"), VersionProperty->GetPropertyValue_InContainer(NewClass->GetDefaultObject()), 2);
		}
	}
};

#endif
