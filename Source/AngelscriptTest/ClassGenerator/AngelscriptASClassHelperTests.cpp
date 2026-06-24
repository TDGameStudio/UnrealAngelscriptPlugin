#include "AngelscriptFunctionalTestUtils.h"
#include "AngelscriptTestMacros.h"

#include "CQTest.h"
#include "ClassGenerator/ASClass.h"
#include "Components/ActorTestSpawner.h"
#include "Engine/Blueprint.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/Guid.h"
#include "Misc/PackageName.h"
#include "Misc/ScopeExit.h"
#include "UObject/GarbageCollection.h"
#include "UObject/Package.h"

// Test Layer: UE Functional
#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptFunctionalTestUtils;

namespace ASClassHelperTest
{
	UBlueprint* CreateTransientBlueprintChild(
		FAutomationTestBase& Test,
		UClass* ParentClass,
		FStringView Suffix,
		const TCHAR* CallingContext = TEXT("AngelscriptASClassHelperTests"))
	{
		FNoDiscardAsserter LocalAssert(Test);
		if (!LocalAssert.IsNotNull(ParentClass, TEXT("ASClass helper test case should receive a valid script parent class")))
		{
			return nullptr;
		}

		const FString PackagePath = FString::Printf(
			TEXT("/Temp/AngelscriptASClassHelper_%.*s_%s"),
			Suffix.Len(),
			Suffix.GetData(),
			*FGuid::NewGuid().ToString(EGuidFormats::Digits));
		UPackage* BlueprintPackage = CreatePackage(*PackagePath);
		if (!LocalAssert.IsNotNull(BlueprintPackage, TEXT("ASClass helper test case should create a transient blueprint package")))
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
		if (!LocalAssert.IsNotNull(Blueprint, TEXT("ASClass helper test case should create a transient blueprint asset")))
		{
			return nullptr;
		}

		return Blueprint;
	}

	bool CompileAndValidateBlueprint(FAutomationTestBase& Test, UBlueprint& Blueprint)
	{
		FKismetEditorUtilities::CompileBlueprint(&Blueprint);
		FNoDiscardAsserter LocalAssert(Test);
		return LocalAssert.IsNotNull(Blueprint.GeneratedClass.Get(), TEXT("ASClass helper test case should compile the transient blueprint"));
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

TEST_CLASS_WITH_FLAGS(FAngelscriptASClassHelperTests,
	"Angelscript.TestModule.ClassGenerator.ASClass",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(HierarchyHelpersResolveScriptAndNativeAncestors)
	{
		using namespace ASClassHelperTest;
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine);
		static const FName ModuleName(TEXT("TestASClassHierarchyHelpers"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
			ASTEST_RESET_ENGINE(Engine);
		};

		UClass* ScriptParentClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("TestASClassHierarchyHelpers.as"),
			TEXT(R"AS(
UCLASS()
class AScriptHierarchyHelperParent : AActor
{
	UPROPERTY()
	int Marker = 17;
}
)AS"),
			TEXT("AScriptHierarchyHelperParent"));
		if (ScriptParentClass == nullptr)
		{
			return;
		}

		UASClass* ScriptASClass = Cast<UASClass>(ScriptParentClass);
		ASSERT_THAT(IsNotNull(ScriptASClass, TEXT("ASClass helper test case should compile the script parent as a UASClass")));

		ASClassHelperTest::FScopedTransientBlueprint Blueprint;
		Blueprint.BlueprintAsset = ASClassHelperTest::CreateTransientBlueprintChild(*TestRunner, ScriptParentClass, TEXT("HierarchyHelpers"));
		if (Blueprint.BlueprintAsset == nullptr)
		{
			return;
		}

		if (!ASClassHelperTest::CompileAndValidateBlueprint(*TestRunner, *Blueprint.BlueprintAsset))
		{
			return;
		}

		UClass* BlueprintChildClass = Blueprint.GetGeneratedClass();
		ASSERT_THAT(IsNotNull(BlueprintChildClass, TEXT("ASClass helper test case should produce a generated Blueprint child class")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* BlueprintChildActor = SpawnScriptActor(*TestRunner, Spawner, BlueprintChildClass);
		ASSERT_THAT(IsNotNull(BlueprintChildActor, TEXT("ASClass helper test case should spawn the Blueprint child actor")));

		UASClass* ScriptAncestorFromScriptClass = UASClass::GetFirstASClass(ScriptParentClass);
		UASClass* ScriptAncestorFromBlueprintClass = UASClass::GetFirstASClass(BlueprintChildClass);
		UASClass* ScriptAncestorFromBlueprintActor = UASClass::GetFirstASClass(BlueprintChildActor);
		UClass* ScriptOrNativeFromBlueprintClass = UASClass::GetFirstASOrNativeClass(BlueprintChildClass);
		UClass* ScriptOrNativeFromNativeActor = UASClass::GetFirstASOrNativeClass(AActor::StaticClass());

		ASSERT_THAT(IsTrue(ScriptAncestorFromScriptClass == ScriptParentClass, TEXT("ASClass helper test case should resolve the script parent from the script class itself")));
		ASSERT_THAT(IsTrue(ScriptAncestorFromBlueprintClass == ScriptParentClass, TEXT("ASClass helper test case should resolve the script parent from the Blueprint child class")));
		ASSERT_THAT(IsTrue(ScriptAncestorFromBlueprintActor == ScriptParentClass, TEXT("ASClass helper test case should resolve the script parent from the Blueprint child actor instance")));
		ASSERT_THAT(IsTrue(ScriptOrNativeFromBlueprintClass == ScriptParentClass, TEXT("ASClass helper test case should prefer the script ancestor over the generated Blueprint class")));
		ASSERT_THAT(IsTrue(ScriptOrNativeFromNativeActor == AActor::StaticClass(), TEXT("ASClass helper test case should return AActor for native AActor fallback")));
		}
	}
};

#endif
