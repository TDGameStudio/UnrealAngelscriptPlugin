#include "Shared/AngelscriptFunctionalTestUtils.h"
#include "Shared/AngelscriptTestMacros.h"

#include "Components/StaticMeshComponent.h"
#include "GameFramework/Actor.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "CQTest.h"
#include "Misc/ScopeExit.h"
#include "UObject/UnrealType.h"

// Test Layer: UE Functional - Round1 vacuum-fill (UMaterialInstanceDynamic AS surface)
#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptTestSupport;

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

		TestRunner->TestTrue(
			TEXT("AFunctionalDynamicMaterialActor should derive from AActor"),
			ActorClass->IsChildOf(AActor::StaticClass()));

		FObjectProperty* DynamicMaterialProp = FindFProperty<FObjectProperty>(ActorClass, TEXT("DynamicMaterial"));
		if (TestRunner->TestNotNull(TEXT("DynamicMaterial FObjectProperty should be registered"), DynamicMaterialProp))
		{
			TestRunner->TestTrue(
				TEXT("DynamicMaterial property class should reference UMaterialInstanceDynamic"),
				DynamicMaterialProp->PropertyClass != nullptr
				&& DynamicMaterialProp->PropertyClass->IsChildOf(UMaterialInstanceDynamic::StaticClass()));
		}

		FObjectProperty* MeshProp = FindFProperty<FObjectProperty>(ActorClass, TEXT("Mesh"));
		if (TestRunner->TestNotNull(TEXT("Mesh FObjectProperty should be registered"), MeshProp))
		{
			TestRunner->TestTrue(
				TEXT("Mesh property class should reference UStaticMeshComponent"),
				MeshProp->PropertyClass != nullptr
				&& MeshProp->PropertyClass->IsChildOf(UStaticMeshComponent::StaticClass()));
		}
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
