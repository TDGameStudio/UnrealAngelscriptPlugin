#include "CQTest.h"
#include "AngelscriptFunctionalTestUtils.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestUtilities.h"

#include "ClassGenerator/ASClass.h"
#include "Components/ActorTestSpawner.h"
#include "Engine/Blueprint.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/AutomationTest.h"
#include "Misc/Guid.h"
#include "Misc/PackageName.h"
#include "Misc/ScopeExit.h"
#include "UObject/GarbageCollection.h"
#include "UObject/Package.h"
#include "UObject/UnrealType.h"

// Test Layer: UE Functional
#if WITH_DEV_AUTOMATION_TESTS

namespace AngelscriptHotReloadBlueprintChildTest
{
	static const FName EditSpecifierModuleName(TEXT("HotReloadBlueprintChildEditSpecifier"));
	static const FString EditSpecifierFilename(TEXT("HotReloadBlueprintChildEditSpecifier.as"));
	static const FName EditSpecifierParentClassName(TEXT("AHotReloadBlueprintChildEditSpecifierParent"));
	static const FName SoftReloadModuleName(TEXT("HotReloadBlueprintChildSoftReload"));
	static const FString SoftReloadFilename(TEXT("HotReloadBlueprintChildSoftReload.as"));
	static const FName SoftReloadParentClassName(TEXT("AHotReloadBlueprintChildSoftReloadParent"));

	static bool IsHandledReloadResult(const ECompileResult ReloadResult)
	{
		return ReloadResult == ECompileResult::FullyHandled || ReloadResult == ECompileResult::PartiallyHandled;
	}

	UBlueprint* CreateTransientBlueprintChild(FAutomationTestBase& Test, UClass* ParentClass)
	{
		if (!Test.TestNotNull(TEXT("HotReload Blueprint-child test should have a script parent class"), ParentClass))
		{
			return nullptr;
		}

		const FString PackagePath = FString::Printf(
			TEXT("/Temp/AngelscriptHotReloadBlueprintChild_%s"),
			*FGuid::NewGuid().ToString(EGuidFormats::Digits));
		UPackage* BlueprintPackage = CreatePackage(*PackagePath);
		if (!Test.TestNotNull(TEXT("HotReload Blueprint-child test should create a transient Blueprint package"), BlueprintPackage))
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
			TEXT("AngelscriptHotReloadBlueprintChildTest"));
		if (!Test.TestNotNull(TEXT("HotReload Blueprint-child test should create a transient Blueprint asset"), Blueprint))
		{
			return nullptr;
		}

		FKismetEditorUtilities::CompileBlueprint(Blueprint);
		if (!Test.TestNotNull(TEXT("HotReload Blueprint-child test should compile a generated Blueprint class"), Blueprint->GeneratedClass.Get()))
		{
			return nullptr;
		}

		return Blueprint;
	}

	void CleanupTransientBlueprint(UBlueprint*& Blueprint)
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
}

TEST_CLASS_WITH_FLAGS(FAngelscriptHotReloadBlueprintChildTests,
	"Angelscript.TestModule.HotReload.BlueprintChild",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(EditSpecifierReloadKeepsBlueprintChildInstanceAlive)
	{
		using namespace AngelscriptHotReloadBlueprintChildTest;

		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope EngineScope(Engine);
		UBlueprint* Blueprint = nullptr;
		ON_SCOPE_EXIT
		{
			CleanupTransientBlueprint(Blueprint);
			Engine.DiscardModule(*EditSpecifierModuleName.ToString());
			ASTEST_RESET_ENGINE(Engine);
		};

		const FString ScriptV1 = TEXT(R"AS(
UCLASS()
class AHotReloadBlueprintChildEditSpecifierParent : AActor
{
	UPROPERTY(NotEditable)
	int ExampleValue = 15;

	UFUNCTION()
	int GetValue()
	{
		return ExampleValue;
	}
}
)AS");

		const FString ScriptV2 = TEXT(R"AS(
UCLASS()
class AHotReloadBlueprintChildEditSpecifierParent : AActor
{
	UPROPERTY(EditAnywhere)
	int ExampleValue = 15;

	UFUNCTION()
	int GetValue()
	{
		return ExampleValue + 1;
	}
}
)AS");

		UClass* ParentClass = AngelscriptFunctionalTestUtils::CompileScriptModule(
			*TestRunner,
			Engine,
			EditSpecifierModuleName,
			EditSpecifierFilename,
			ScriptV1,
			EditSpecifierParentClassName);
		ASSERT_THAT(IsNotNull(ParentClass));

		Blueprint = CreateTransientBlueprintChild(*TestRunner, ParentClass);
		ASSERT_THAT(IsNotNull(Blueprint));

		UClass* BlueprintClass = Blueprint->GeneratedClass.Get();
		ASSERT_THAT(IsNotNull(BlueprintClass, TEXT("HotReload Blueprint-child test should expose the generated Blueprint class")));
		ASSERT_THAT(IsTrue(BlueprintClass->IsChildOf(ParentClass), TEXT("HotReload Blueprint-child test should derive from the script parent")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* BlueprintActor = AngelscriptFunctionalTestUtils::SpawnScriptActor(*TestRunner, Spawner, BlueprintClass);
		ASSERT_THAT(IsNotNull(BlueprintActor, TEXT("HotReload Blueprint-child test should spawn a Blueprint child actor instance before reload")));
		AngelscriptFunctionalTestUtils::BeginPlayActor(Engine, *BlueprintActor);

		FIntProperty* ExampleValueBeforeReload = FindFProperty<FIntProperty>(ParentClass, TEXT("ExampleValue"));
		ASSERT_THAT(IsNotNull(ExampleValueBeforeReload, TEXT("HotReload Blueprint-child test should expose ExampleValue before reload")));
		ASSERT_THAT(IsFalse(ExampleValueBeforeReload->HasAnyPropertyFlags(CPF_Edit), TEXT("NotEditable should clear CPF_Edit before reload")));

		ECompileResult ReloadResult = ECompileResult::Error;
		ASSERT_THAT(IsTrue(
			CompileModuleWithResult(&Engine, ECompileType::FullReload, EditSpecifierModuleName, EditSpecifierFilename, ScriptV2, ReloadResult),
			TEXT("HotReload Blueprint-child test should compile the EditAnywhere update")));
		ASSERT_THAT(IsTrue(IsHandledReloadResult(ReloadResult), TEXT("HotReload Blueprint-child test should finish on a handled reload path")));

		UClass* ReloadedParentClass = FindGeneratedClass(&Engine, EditSpecifierParentClassName);
		ASSERT_THAT(IsNotNull(ReloadedParentClass, TEXT("HotReload Blueprint-child test should resolve the parent class after reload")));

		FIntProperty* ExampleValueAfterReload = FindFProperty<FIntProperty>(ReloadedParentClass, TEXT("ExampleValue"));
		ASSERT_THAT(IsNotNull(ExampleValueAfterReload, TEXT("HotReload Blueprint-child test should keep ExampleValue after reload")));
		ASSERT_THAT(IsTrue(ExampleValueAfterReload->HasAnyPropertyFlags(CPF_Edit), TEXT("EditAnywhere should set CPF_Edit after reload")));

		ASSERT_THAT(IsNotNull(BlueprintActor, TEXT("HotReload Blueprint-child test should keep the pre-existing Blueprint actor pointer usable after reload")));
		}
	}

	TEST_METHOD(SoftReloadKeepsBlueprintChildInstanceOnUpdatedParentBody)
	{
		using namespace AngelscriptHotReloadBlueprintChildTest;

		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope EngineScope(Engine);
		UBlueprint* Blueprint = nullptr;
		ON_SCOPE_EXIT
		{
			CleanupTransientBlueprint(Blueprint);
			Engine.DiscardModule(*SoftReloadModuleName.ToString());
			ASTEST_RESET_ENGINE(Engine);
		};

		const FString ScriptV1 = TEXT(R"AS(
UCLASS()
class AHotReloadBlueprintChildSoftReloadParent : AActor
{
	UPROPERTY()
	int ExampleValue = 30;

	UFUNCTION()
	int GetValue()
	{
		return ExampleValue;
	}
}
)AS");

		const FString ScriptV2 = TEXT(R"AS(
UCLASS()
class AHotReloadBlueprintChildSoftReloadParent : AActor
{
	UPROPERTY()
	int ExampleValue = 30;

	UFUNCTION()
	int GetValue()
	{
		return ExampleValue + 12;
	}
}
)AS");

		UClass* ParentClass = AngelscriptFunctionalTestUtils::CompileScriptModule(
			*TestRunner,
			Engine,
			SoftReloadModuleName,
			SoftReloadFilename,
			ScriptV1,
			SoftReloadParentClassName);
		ASSERT_THAT(IsNotNull(ParentClass));

		UASClass* ParentASClass = Cast<UASClass>(ParentClass);
		ASSERT_THAT(IsNotNull(ParentASClass, TEXT("HotReload Blueprint-child soft test should start from an AS parent class")));

		Blueprint = CreateTransientBlueprintChild(*TestRunner, ParentClass);
		ASSERT_THAT(IsNotNull(Blueprint));

		UClass* BlueprintClass = Blueprint->GeneratedClass.Get();
		ASSERT_THAT(IsNotNull(BlueprintClass, TEXT("HotReload Blueprint-child soft test should expose the generated Blueprint class")));
		ASSERT_THAT(IsTrue(BlueprintClass->IsChildOf(ParentClass), TEXT("HotReload Blueprint-child soft test should derive from the script parent")));
		ASSERT_THAT(IsNull(Cast<UASClass>(BlueprintClass), TEXT("Generated Blueprint child should not be a UASClass")));
		ASSERT_THAT(AreEqual(ParentASClass, UASClass::GetFirstASClass(BlueprintClass), TEXT("Generated Blueprint child should resolve its first AS class through the parent chain")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* BlueprintActor = AngelscriptFunctionalTestUtils::SpawnScriptActor(*TestRunner, Spawner, BlueprintClass);
		ASSERT_THAT(IsNotNull(BlueprintActor, TEXT("HotReload Blueprint-child soft test should spawn a Blueprint child actor instance before reload")));
		AngelscriptFunctionalTestUtils::BeginPlayActor(Engine, *BlueprintActor);

		UFunction* GetValueBeforeReload = FindGeneratedFunction(ParentClass, TEXT("GetValue"));
		ASSERT_THAT(IsNotNull(GetValueBeforeReload, TEXT("HotReload Blueprint-child soft test should expose GetValue before reload")));
		int32 ValueBeforeReload = 0;
		ASSERT_THAT(IsTrue(
			ExecuteGeneratedIntEventOnGameThread(&Engine, BlueprintActor, GetValueBeforeReload, ValueBeforeReload),
			TEXT("HotReload Blueprint-child soft test should execute the parent function before reload")));
		ASSERT_THAT(AreEqual(30, ValueBeforeReload, TEXT("HotReload Blueprint-child soft test should observe the pre-reload parent body")));

		ECompileResult ReloadResult = ECompileResult::Error;
		ASSERT_THAT(IsTrue(
			CompileModuleWithResult(&Engine, ECompileType::SoftReloadOnly, SoftReloadModuleName, SoftReloadFilename, ScriptV2, ReloadResult),
			TEXT("HotReload Blueprint-child soft test should compile the body-only update")));
		ASSERT_THAT(AreEqual(ECompileResult::FullyHandled, ReloadResult, TEXT("HotReload Blueprint-child soft test should stay on the pure soft reload path")));

		UClass* ReloadedParentClass = FindGeneratedClass(&Engine, SoftReloadParentClassName);
		ASSERT_THAT(IsNotNull(ReloadedParentClass, TEXT("HotReload Blueprint-child soft test should resolve the parent class after reload")));
		ASSERT_THAT(AreEqual(ParentClass, ReloadedParentClass, TEXT("HotReload Blueprint-child soft test should keep the same parent UClass on soft reload")));
		ASSERT_THAT(AreEqual(ParentASClass, UASClass::GetFirstASClass(BlueprintClass), TEXT("Generated Blueprint child should still resolve the AS parent after soft reload")));
		ASSERT_THAT(AreEqual(BlueprintClass, BlueprintActor->GetClass(), TEXT("Existing Blueprint child actor should keep its generated class after soft reload")));

		UFunction* GetValueAfterReload = FindGeneratedFunction(ReloadedParentClass, TEXT("GetValue"));
		ASSERT_THAT(IsNotNull(GetValueAfterReload, TEXT("HotReload Blueprint-child soft test should expose GetValue after reload")));
		int32 ValueAfterReload = 0;
		ASSERT_THAT(IsTrue(
			ExecuteGeneratedIntEventOnGameThread(&Engine, BlueprintActor, GetValueAfterReload, ValueAfterReload),
			TEXT("HotReload Blueprint-child soft test should execute the updated parent function after reload")));
		ASSERT_THAT(AreEqual(42, ValueAfterReload, TEXT("HotReload Blueprint-child soft test should observe the post-reload parent body")));
		}
	}
};

#endif
