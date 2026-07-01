#include "AngelscriptFunctionalTestUtils.h"
#include "AngelscriptTestMacros.h"

#include "Components/ActorTestSpawner.h"
#include "Components/PointLightComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Actor.h"
#include "CQTest.h"
#include "Misc/ScopeExit.h"

// Test Layer: UE Functional - Round1 deep-fill (4-level DefaultComponent attach chain)
#if WITH_ANGELSCRIPT_UNITTESTS


TEST_CLASS_WITH_FLAGS(FAngelscriptComponentMultiLevelHierarchyTests,
	"Angelscript.TestModule.Functional.Component.MultiLevelHierarchy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(FourLevelAttachChainResolves)
	{
		using namespace AngelscriptFunctionalTestUtils;
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		FAngelscriptEngineScope EngineScope(Engine);

		static const FName ModuleName(TEXT("FunctionalComponentMultiLevelHierarchy"));
		ON_SCOPE_EXIT { Engine.DiscardModule(*ModuleName.ToString()); };

		UClass* ActorClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("FunctionalComponentMultiLevelHierarchy.as"),
			TEXT(R"AS(
UCLASS()
class AFunctionalMultiLevelActor : AActor
{
	UPROPERTY(DefaultComponent, RootComponent)
	USceneComponent Root;

	UPROPERTY(DefaultComponent, Attach = Root)
	USceneComponent Middle;

	UPROPERTY(DefaultComponent, Attach = Middle)
	UStaticMeshComponent LeafMesh;

	UPROPERTY(DefaultComponent, Attach = LeafMesh)
	UPointLightComponent DeepLight;
}
)AS"),
			TEXT("AFunctionalMultiLevelActor"));
		if (ActorClass == nullptr) { return; }

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ActorClass);
		if (Actor == nullptr) { return; }
		BeginPlayActor(Engine, *Actor);

		USceneComponent* RootSc = Actor->GetRootComponent();
		ASSERT_THAT(IsNotNull(RootSc, TEXT("Actor should expose a root SceneComponent")));

		auto FindChild = [&](USceneComponent* Parent, UClass* ChildClass) -> USceneComponent*
		{
			if (Parent == nullptr) return nullptr;
			TArray<USceneComponent*> Children;
			Parent->GetChildrenComponents(false, Children);
			for (USceneComponent* Child : Children)
			{
				if (Child != nullptr && Child->IsA(ChildClass))
				{
					return Child;
				}
			}
			return nullptr;
		};

		USceneComponent* Middle = FindChild(RootSc, USceneComponent::StaticClass());
		ASSERT_THAT(IsNotNull(Middle, TEXT("Middle SceneComponent should be attached to Root")));

		UStaticMeshComponent* LeafMesh = Cast<UStaticMeshComponent>(FindChild(Middle, UStaticMeshComponent::StaticClass()));
		ASSERT_THAT(IsNotNull(LeafMesh, TEXT("LeafMesh StaticMeshComponent should be attached to Middle")));

		UPointLightComponent* DeepLight = Cast<UPointLightComponent>(FindChild(LeafMesh, UPointLightComponent::StaticClass()));
		ASSERT_THAT(IsNotNull(DeepLight, TEXT("DeepLight PointLightComponent should be attached to LeafMesh")));

		ASSERT_THAT(AreEqual(RootSc, Middle->GetAttachParent(), TEXT("Middle's GetAttachParent should be Root")));
		ASSERT_THAT(AreEqual(static_cast<USceneComponent*>(Middle), LeafMesh->GetAttachParent(), TEXT("LeafMesh's GetAttachParent should be Middle")));
		ASSERT_THAT(AreEqual(static_cast<USceneComponent*>(LeafMesh), DeepLight->GetAttachParent(), TEXT("DeepLight's GetAttachParent should be LeafMesh")));

		TArray<USceneComponent*> DeepLightChildren;
		DeepLight->GetChildrenComponents(false, DeepLightChildren);
		ASSERT_THAT(AreEqual(0, DeepLightChildren.Num(), TEXT("DeepLight should have no children")));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
