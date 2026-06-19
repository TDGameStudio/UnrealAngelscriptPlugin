#include "AngelscriptFunctionalTestUtils.h"
#include "AngelscriptTestMacros.h"

#include "Components/StaticMeshComponent.h"
#include "GameFramework/Actor.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "CQTest.h"
#include "Misc/ScopeExit.h"
#include "UObject/UnrealType.h"

// Test Layer: UE Functional - Round1 vacuum-fill (UMaterialInstanceDynamic AS surface)
#if WITH_DEV_AUTOMATION_TESTS


TEST_CLASS_WITH_FLAGS(FAngelscriptDynamicMaterialTests,
	"Angelscript.TestModule.Functional.Rendering.DynamicMaterial",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(ScriptCompilesDynamicMaterialAPI)
	{
		using namespace AngelscriptFunctionalTestUtils;
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		FAngelscriptEngineScope EngineScope(Engine);

		static const FName ModuleName(TEXT("FunctionalDynamicMaterial"));
		ON_SCOPE_EXIT { Engine.DiscardModule(*ModuleName.ToString()); };

		UClass* ActorClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("FunctionalDynamicMaterial.as"),
			TEXT(R"AS(
UCLASS()
class AFunctionalDynamicMaterialActor : AActor
{
	UPROPERTY(DefaultComponent, RootComponent)
	UStaticMeshComponent Mesh;

	UPROPERTY()
	UMaterialInstanceDynamic DynamicMaterial;

	UFUNCTION(BlueprintOverride)
	void BeginPlay()
	{
		DynamicMaterial = Mesh.CreateDynamicMaterialInstance(0);
		if (DynamicMaterial != nullptr)
		{
			DynamicMaterial.SetScalarParameterValue(n"Opacity", 0.5);
			DynamicMaterial.SetVectorParameterValue(n"Color", FLinearColor(1.0, 0.5, 0.0, 1.0));
		}
	}
}
)AS"),
			TEXT("AFunctionalDynamicMaterialActor"));
		if (ActorClass == nullptr) { return; }

		ASSERT_THAT(IsTrue(
			ActorClass->IsChildOf(AActor::StaticClass()),
			TEXT("AFunctionalDynamicMaterialActor should derive from AActor")));

		FObjectProperty* DynamicMaterialProp = FindFProperty<FObjectProperty>(ActorClass, TEXT("DynamicMaterial"));
		if (this->Assert.IsNotNull(DynamicMaterialProp, TEXT("DynamicMaterial FObjectProperty should be registered")))
		{
			ASSERT_THAT(IsTrue(
				DynamicMaterialProp->PropertyClass != nullptr
				&& DynamicMaterialProp->PropertyClass->IsChildOf(UMaterialInstanceDynamic::StaticClass()),
				TEXT("DynamicMaterial property class should reference UMaterialInstanceDynamic")));
		}

		FObjectProperty* MeshProp = FindFProperty<FObjectProperty>(ActorClass, TEXT("Mesh"));
		if (this->Assert.IsNotNull(MeshProp, TEXT("Mesh FObjectProperty should be registered")))
		{
			ASSERT_THAT(IsTrue(
				MeshProp->PropertyClass != nullptr
				&& MeshProp->PropertyClass->IsChildOf(UStaticMeshComponent::StaticClass()),
				TEXT("Mesh property class should reference UStaticMeshComponent")));
		}
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
