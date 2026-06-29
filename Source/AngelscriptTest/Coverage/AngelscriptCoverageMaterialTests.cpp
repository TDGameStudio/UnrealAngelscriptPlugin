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
//   OpenSpec: test-coverage-matrix-consolidation/coverage-matrix.md
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

	UPROPERTY()
	bool bParameterReadbackCallable = false;

	UPROPERTY()
	double ScalarParameterReadback = -1.0;

	UPROPERTY()
	FLinearColor VectorParameterReadback = FLinearColor::Black;

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

			ScalarParameterReadback = DynamicMaterial.GetScalarParameterValue(n"CoverageScalar");
			VectorParameterReadback = DynamicMaterial.GetVectorParameterValue(n"CoverageColor");
			bParameterReadbackCallable =
				Math::IsNearlyEqual(ScalarParameterReadback, 0.5f, 0.001f)
				&& Math::IsNearlyEqual(VectorParameterReadback.R, 0.25f, 0.001f)
				&& Math::IsNearlyEqual(VectorParameterReadback.G, 0.50f, 0.001f)
				&& Math::IsNearlyEqual(VectorParameterReadback.B, 0.75f, 0.001f)
				&& Math::IsNearlyEqual(VectorParameterReadback.A, 1.0f, 0.001f);
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

	TEST_METHOD(DynamicMaterialParameterReadback)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageMaterial_DynamicReadback"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileCoverageMaterialActor(*TestRunner, Engine, ModuleName);
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("DynamicMaterialParameterReadback module should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		UMaterialInterface* DefaultMaterial = UMaterial::GetDefaultMaterial(MD_Surface);
		ASSERT_THAT(IsNotNull(DefaultMaterial, TEXT("Engine default surface material should be available")));
		if (DefaultMaterial == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnMaterialActor(*TestRunner, Engine, Spawner, ScriptClass, DefaultMaterial);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Material coverage actor should spawn for parameter readback")));
		if (Actor == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bParameterReadbackCallable"), true,
			TEXT("AS should read back scalar and vector MID parameters through getter bindings"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("ScalarParameterReadback"), 0.5,
			TEXT("AS scalar parameter getter should return the value set on the MID"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FFloatProperty, float>(*TestRunner, Actor, TEXT("VectorParameterReadback.R"), 0.25f,
			TEXT("AS vector parameter getter should return the red channel set on the MID"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FFloatProperty, float>(*TestRunner, Actor, TEXT("VectorParameterReadback.G"), 0.5f,
			TEXT("AS vector parameter getter should return the green channel set on the MID"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FFloatProperty, float>(*TestRunner, Actor, TEXT("VectorParameterReadback.B"), 0.75f,
			TEXT("AS vector parameter getter should return the blue channel set on the MID"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FFloatProperty, float>(*TestRunner, Actor, TEXT("VectorParameterReadback.A"), 1.0f,
			TEXT("AS vector parameter getter should return the alpha channel set on the MID"))));

		UObject* DynamicMaterialObject = nullptr;
		ASSERT_THAT(IsTrue(GetObjectByPath(*TestRunner, Actor, TEXT("DynamicMaterial"), DynamicMaterialObject),
			TEXT("DynamicMaterial should be readable for native parameter validation")));
		UMaterialInstanceDynamic* DynamicMaterial = Cast<UMaterialInstanceDynamic>(DynamicMaterialObject);
		ASSERT_THAT(IsNotNull(DynamicMaterial, TEXT("DynamicMaterial should be a UMaterialInstanceDynamic for native parameter validation")));
		if (DynamicMaterial == nullptr)
		{
			return;
		}

		const FName ScalarParameterName(TEXT("CoverageScalar"));
		const FScalarParameterValue* ScalarParameter = DynamicMaterial->ScalarParameterValues.FindByPredicate(
			[ScalarParameterName](const FScalarParameterValue& Parameter)
			{
				return Parameter.ParameterInfo.Name == ScalarParameterName;
			});
		ASSERT_THAT(IsNotNull(ScalarParameter, TEXT("AS SetScalarParameterValue should create a scalar MID override")));
		if (ScalarParameter == nullptr)
		{
			return;
		}

		const FName VectorParameterName(TEXT("CoverageColor"));
		const FVectorParameterValue* VectorParameter = DynamicMaterial->VectorParameterValues.FindByPredicate(
			[VectorParameterName](const FVectorParameterValue& Parameter)
			{
				return Parameter.ParameterInfo.Name == VectorParameterName;
			});
		ASSERT_THAT(IsNotNull(VectorParameter, TEXT("AS SetVectorParameterValue should create a vector MID override")));
		if (VectorParameter == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(ScalarParameter->ParameterValue, 0.5f, 0.001f),
			TEXT("Native MID scalar override should match the AS-written value")));
		ASSERT_THAT(IsTrue(VectorParameter->ParameterValue.Equals(FLinearColor(0.25f, 0.50f, 0.75f, 1.0f), 0.001f),
			TEXT("Native MID vector override should match the AS-written value")));
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
