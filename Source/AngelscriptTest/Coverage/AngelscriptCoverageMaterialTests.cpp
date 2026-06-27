#include "CQTest.h"
#include "AngelscriptFunctionalTestUtils.h"
#include "AngelscriptReflectiveAccess.h"
#include "AngelscriptTestMacros.h"

#include "Components/ActorTestSpawner.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Actor.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/ScopeExit.h"
#include "UObject/UnrealType.h"

// -----------------------------------------------------------------------------
// AngelscriptCoverageMaterialTests
// -----------------------------------------------------------------------------
// Coverage for the Material slice from:
//
//   Documents/Coverage/Coverage_Material.md
//
// The tests avoid requiring a mesh asset or real RHI. They inject the engine
// default material from C++ and let AS exercise UPrimitiveComponent material APIs
// plus UMaterialInstanceDynamic parameter calls.
// -----------------------------------------------------------------------------

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptFunctionalTestUtils;

namespace
{
	UClass* CompileCoverageMaterialActor(FAutomationTestBase& Test, FAngelscriptEngine& Engine, FName ModuleName)
	{
		return CompileScriptModule(
			Test,
			Engine,
			ModuleName,
			TEXT("ASCoverageMaterial.as"),
			ASTEST_AS(R"AS(
UCLASS()
class ACoverageMaterialActor : AActor
{
	UPROPERTY(DefaultComponent, RootComponent)
	UStaticMeshComponent Mesh;

	UPROPERTY()
	UMaterialInterface SourceMaterial;

	UPROPERTY()
	UMaterialInterface MaterialBeforeDynamic;

	UPROPERTY()
	UMaterialInstanceDynamic DynamicMaterial;

	UPROPERTY()
	bool bMaterialSlotRoundTrip = false;

	UPROPERTY()
	int NumMaterialsAfterOverride = -1;

	UPROPERTY()
	bool bDynamicMaterialCreated = false;

	UPROPERTY()
	bool bDynamicMaterialAssigned = false;

	UPROPERTY()
	bool bParameterControlsCallable = false;

	UFUNCTION(BlueprintOverride)
	void BeginPlay()
	{
		if (Mesh == nullptr || SourceMaterial == nullptr)
		{
			return;
		}

		Mesh.SetMaterial(0, SourceMaterial);
		MaterialBeforeDynamic = Mesh.GetMaterial(0);
		NumMaterialsAfterOverride = Mesh.GetNumMaterials();
		bMaterialSlotRoundTrip = MaterialBeforeDynamic == SourceMaterial;

		DynamicMaterial = Mesh.CreateDynamicMaterialInstance(0, SourceMaterial);
		bDynamicMaterialCreated = DynamicMaterial != nullptr;
		bDynamicMaterialAssigned = Mesh.GetMaterial(0) == DynamicMaterial;

		if (DynamicMaterial != nullptr)
		{
			DynamicMaterial.SetScalarParameterValue(n"CoverageScalar", 0.5f);
			DynamicMaterial.SetVectorParameterValue(n"CoverageColor", FLinearColor(0.25f, 0.50f, 0.75f, 1.0f));
			bParameterControlsCallable = true;
		}
	}
}
)AS"),
			TEXT("ACoverageMaterialActor"));
	}

	AActor* SpawnMaterialActor(
		FAutomationTestBase& Test,
		FAngelscriptEngine& Engine,
		FActorTestSpawner& Spawner,
		UClass* ScriptClass,
		UMaterialInterface* SourceMaterial)
	{
		AActor* Actor = SpawnScriptActor(Test, Spawner, ScriptClass);
		if (!Test.TestNotNull(TEXT("Material coverage actor should spawn"), Actor))
		{
			return nullptr;
		}

		if (!SetObjectByPath(Test, Actor, TEXT("SourceMaterial"), SourceMaterial))
		{
			return nullptr;
		}

		BeginPlayActor(Engine, *Actor);
		return Actor;
	}
}

TEST_CLASS_WITH_FLAGS(FAngelscriptCoverageMaterialTest,
	"Angelscript.TestModule.Coverage.Material",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	BEFORE_ALL()
	{
		ASTEST_CREATE_ENGINE();
	}

	AFTER_ALL()
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		ASTEST_RESET_ENGINE(Engine);
	}

	TEST_METHOD(ComponentMaterialSlotRoundTrip)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageMaterial_SlotRoundTrip"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileCoverageMaterialActor(*TestRunner, Engine, ModuleName);
		if (ScriptClass == nullptr)
		{
			return;
		}

		FObjectProperty* MeshProperty = FindFProperty<FObjectProperty>(ScriptClass, TEXT("Mesh"));
		ASSERT_THAT(IsNotNull(MeshProperty, TEXT("Mesh component property should be generated")));
		ASSERT_THAT(IsTrue(MeshProperty->PropertyClass != nullptr && MeshProperty->PropertyClass->IsChildOf(UStaticMeshComponent::StaticClass()),
			TEXT("Mesh property should reference UStaticMeshComponent")));

		UMaterialInterface* DefaultMaterial = UMaterial::GetDefaultMaterial(MD_Surface);
		ASSERT_THAT(IsNotNull(DefaultMaterial, TEXT("Engine default surface material should be available")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnMaterialActor(*TestRunner, Engine, Spawner, ScriptClass, DefaultMaterial);
		if (Actor == nullptr)
		{
			return;
		}

		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bMaterialSlotRoundTrip"), true,
			TEXT("AS GetMaterial/SetMaterial should round-trip the injected material"));
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("NumMaterialsAfterOverride"), 0,
			TEXT("StaticMeshComponent without a mesh should keep GetNumMaterials at the asset-slot boundary"));

		UObject* MaterialBeforeDynamic = nullptr;
		ASSERT_THAT(IsTrue(GetObjectByPath(*TestRunner, Actor, TEXT("MaterialBeforeDynamic"), MaterialBeforeDynamic),
			TEXT("MaterialBeforeDynamic should be readable")));
		ASSERT_THAT(AreEqual(static_cast<UObject*>(DefaultMaterial), MaterialBeforeDynamic,
			TEXT("GetMaterial should return the injected source material before MID creation")));
	}

	TEST_METHOD(DynamicMaterialParametersAndAssignment)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageMaterial_DynamicParameters"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileCoverageMaterialActor(*TestRunner, Engine, ModuleName);
		if (ScriptClass == nullptr)
		{
			return;
		}

		UMaterialInterface* DefaultMaterial = UMaterial::GetDefaultMaterial(MD_Surface);
		ASSERT_THAT(IsNotNull(DefaultMaterial, TEXT("Engine default surface material should be available")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnMaterialActor(*TestRunner, Engine, Spawner, ScriptClass, DefaultMaterial);
		if (Actor == nullptr)
		{
			return;
		}

		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bDynamicMaterialCreated"), true,
			TEXT("CreateDynamicMaterialInstance should create a UMaterialInstanceDynamic from AS"));
		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bDynamicMaterialAssigned"), true,
			TEXT("CreateDynamicMaterialInstance should assign the MID back to the component slot"));
		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bParameterControlsCallable"), true,
			TEXT("AS should call scalar and vector MID parameter APIs"));

		UObject* DynamicMaterialObject = nullptr;
		ASSERT_THAT(IsTrue(GetObjectByPath(*TestRunner, Actor, TEXT("DynamicMaterial"), DynamicMaterialObject),
			TEXT("DynamicMaterial should be readable")));
		UMaterialInstanceDynamic* DynamicMaterial = Cast<UMaterialInstanceDynamic>(DynamicMaterialObject);
		ASSERT_THAT(IsNotNull(DynamicMaterial, TEXT("DynamicMaterial should be a UMaterialInstanceDynamic")));

		UObject* MeshObject = nullptr;
		ASSERT_THAT(IsTrue(GetObjectByPath(*TestRunner, Actor, TEXT("Mesh"), MeshObject),
			TEXT("Mesh component should be readable")));
		UStaticMeshComponent* Mesh = Cast<UStaticMeshComponent>(MeshObject);
		ASSERT_THAT(IsNotNull(Mesh, TEXT("Mesh should be a UStaticMeshComponent")));
		ASSERT_THAT(AreEqual(static_cast<UMaterialInterface*>(DynamicMaterial), Mesh->GetMaterial(0),
			TEXT("Component material slot should contain the dynamic material created by AS")));
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
