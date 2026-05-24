#include "Shared/AngelscriptFunctionalTestUtils.h"
#include "Shared/AngelscriptTestMacros.h"

#include "Components/ActorTestSpawner.h"
#include "Components/PointLightComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Actor.h"
#include "CQTest.h"
#include "Misc/ScopeExit.h"

// Test Layer: UE Functional - Round1 deep-fill (4-level DefaultComponent attach chain)
#if WITH_DEV_AUTOMATION_TESTS


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
		if (!TestRunner->TestNotNull(TEXT("Actor should expose a root SceneComponent"), RootSc)) { return; }

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
		if (!TestRunner->TestNotNull(TEXT("Middle SceneComponent should be attached to Root"), Middle)) { return; }

		UStaticMeshComponent* LeafMesh = Cast<UStaticMeshComponent>(FindChild(Middle, UStaticMeshComponent::StaticClass()));
		if (!TestRunner->TestNotNull(TEXT("LeafMesh StaticMeshComponent should be attached to Middle"), LeafMesh)) { return; }

		UPointLightComponent* DeepLight = Cast<UPointLightComponent>(FindChild(LeafMesh, UPointLightComponent::StaticClass()));
		if (!TestRunner->TestNotNull(TEXT("DeepLight PointLightComponent should be attached to LeafMesh"), DeepLight)) { return; }

		TestRunner->TestEqual(TEXT("Middle's GetAttachParent should be Root"), Middle->GetAttachParent(), RootSc);
		TestRunner->TestEqual(TEXT("LeafMesh's GetAttachParent should be Middle"), LeafMesh->GetAttachParent(), static_cast<USceneComponent*>(Middle));
		TestRunner->TestEqual(TEXT("DeepLight's GetAttachParent should be LeafMesh"), DeepLight->GetAttachParent(), static_cast<USceneComponent*>(LeafMesh));

		TArray<USceneComponent*> DeepLightChildren;
		DeepLight->GetChildrenComponents(false, DeepLightChildren);
		TestRunner->TestEqual(TEXT("DeepLight should have no children"), DeepLightChildren.Num(), 0);
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
