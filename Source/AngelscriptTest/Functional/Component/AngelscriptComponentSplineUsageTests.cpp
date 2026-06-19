#include "AngelscriptFunctionalTestUtils.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestWorld.h"

#include "Components/SceneComponent.h"
#include "Components/SplineComponent.h"
#include "GameFramework/Actor.h"
#include "CQTest.h"
#include "Misc/ScopeExit.h"
#include "UObject/UnrealType.h"

// Test Layer: UE Functional - Round1 vacuum-fill (USplineComponent default + AS API surface)
#if WITH_DEV_AUTOMATION_TESTS


TEST_CLASS_WITH_FLAGS(FAngelscriptComponentSplineUsageTests,
	"Angelscript.TestModule.Functional.Component.SplineUsage",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(SplineDefaultComponentRegistersAndMaterializes)
	{
		using namespace AngelscriptFunctionalTestUtils;
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		FAngelscriptEngineScope EngineScope(Engine);

		static const FName ModuleName(TEXT("FunctionalSplineUsage"));
		ON_SCOPE_EXIT { Engine.DiscardModule(*ModuleName.ToString()); };

		UClass* ActorClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("FunctionalSplineUsage.as"),
			TEXT(R"AS(
UCLASS()
class AFunctionalSplineActor : AActor
{
	UPROPERTY(DefaultComponent, RootComponent)
	USceneComponent Root;

	UPROPERTY(DefaultComponent, Attach = Root)
	USplineComponent Spline;

	UPROPERTY()
	int RootChildCountAtBeginPlay = 0;

	UPROPERTY()
	bool bSawSplineAtBeginPlay = false;

	UFUNCTION(BlueprintOverride)
	void BeginPlay()
	{
		RootChildCountAtBeginPlay = Root.GetNumChildrenComponents();
		bSawSplineAtBeginPlay = Spline != null;
	}
}
)AS"),
			TEXT("AFunctionalSplineActor"));
		if (ActorClass == nullptr) { return; }

		ASSERT_THAT(IsTrue(
			ActorClass->IsChildOf(AActor::StaticClass()),
			TEXT("AFunctionalSplineActor should derive from AActor")));

		FObjectProperty* SplineProp = FindFProperty<FObjectProperty>(ActorClass, TEXT("Spline"));
		if (this->Assert.IsNotNull(SplineProp, TEXT("Spline FObjectProperty should be registered")))
		{
			ASSERT_THAT(IsTrue(
				SplineProp->PropertyClass != nullptr
				&& SplineProp->PropertyClass->IsChildOf(USplineComponent::StaticClass()),
				TEXT("Spline property class should reference USplineComponent")));
		}

		FObjectProperty* RootProp = FindFProperty<FObjectProperty>(ActorClass, TEXT("Root"));
		if (this->Assert.IsNotNull(RootProp, TEXT("Root FObjectProperty should be registered")))
		{
			ASSERT_THAT(IsTrue(
				RootProp->PropertyClass != nullptr
				&& RootProp->PropertyClass->IsChildOf(USceneComponent::StaticClass()),
				TEXT("Root property class should reference USceneComponent")));
		}

		FIntProperty* RootChildCountProp = FindFProperty<FIntProperty>(ActorClass, TEXT("RootChildCountAtBeginPlay"));
		ASSERT_THAT(IsNotNull(RootChildCountProp, TEXT("RootChildCountAtBeginPlay FIntProperty should be registered")));

		FBoolProperty* SawSplineProp = FindFProperty<FBoolProperty>(ActorClass, TEXT("bSawSplineAtBeginPlay"));
		ASSERT_THAT(IsNotNull(SawSplineProp, TEXT("bSawSplineAtBeginPlay FBoolProperty should be registered")));

		FAngelscriptTestWorld W(*TestRunner, Engine);
		if (!W.IsValid()) { return; }

		AActor* Actor = W.SpawnActorOfClass(ActorClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Spline actor should spawn")));

		USceneComponent* Root = RootProp != nullptr
			? Cast<USceneComponent>(RootProp->GetObjectPropertyValue_InContainer(Actor))
			: nullptr;
		USplineComponent* Spline = SplineProp != nullptr
			? Cast<USplineComponent>(SplineProp->GetObjectPropertyValue_InContainer(Actor))
			: nullptr;
		ASSERT_THAT(IsNotNull(Root, TEXT("Root default component should materialize")));
		ASSERT_THAT(IsNotNull(Spline, TEXT("Spline default component should materialize")));

		ASSERT_THAT(AreEqual(Actor->GetRootComponent(), Root, TEXT("Root property should point to the actor root component")));
		ASSERT_THAT(AreEqual(Root, Spline->GetAttachParent(), TEXT("Spline default component should attach to the scripted root")));
		ASSERT_THAT(IsTrue(Spline->IsRegistered(), TEXT("Spline default component should register with the actor")));
		ASSERT_THAT(IsTrue(Spline->GetNumberOfSplinePoints() >= 0, TEXT("Spline component should expose native spline state after materialization")));

		W.BeginPlay(*Actor);

		int32 RootChildCountAtBeginPlay = 0;
		bool bSawSplineAtBeginPlay = false;
		if (!ReadPropertyValue<FIntProperty>(*TestRunner, Actor, TEXT("RootChildCountAtBeginPlay"), RootChildCountAtBeginPlay)
			|| !ReadPropertyValue<FBoolProperty>(*TestRunner, Actor, TEXT("bSawSplineAtBeginPlay"), bSawSplineAtBeginPlay))
		{
			return;
		}

		ASSERT_THAT(IsTrue(RootChildCountAtBeginPlay >= 1, TEXT("Script BeginPlay should see the attached spline through the root component")));
		ASSERT_THAT(IsTrue(bSawSplineAtBeginPlay, TEXT("Script BeginPlay should see a non-null spline component reference")));
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
