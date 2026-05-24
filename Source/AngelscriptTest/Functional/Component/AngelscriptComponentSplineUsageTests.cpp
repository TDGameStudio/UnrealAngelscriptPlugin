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

		TestRunner->TestTrue(
			TEXT("AFunctionalSplineActor should derive from AActor"),
			ActorClass->IsChildOf(AActor::StaticClass()));

		FObjectProperty* SplineProp = FindFProperty<FObjectProperty>(ActorClass, TEXT("Spline"));
		if (TestRunner->TestNotNull(TEXT("Spline FObjectProperty should be registered"), SplineProp))
		{
			TestRunner->TestTrue(
				TEXT("Spline property class should reference USplineComponent"),
				SplineProp->PropertyClass != nullptr
				&& SplineProp->PropertyClass->IsChildOf(USplineComponent::StaticClass()));
		}

		FObjectProperty* RootProp = FindFProperty<FObjectProperty>(ActorClass, TEXT("Root"));
		if (TestRunner->TestNotNull(TEXT("Root FObjectProperty should be registered"), RootProp))
		{
			TestRunner->TestTrue(
				TEXT("Root property class should reference USceneComponent"),
				RootProp->PropertyClass != nullptr
				&& RootProp->PropertyClass->IsChildOf(USceneComponent::StaticClass()));
		}

		FIntProperty* RootChildCountProp = FindFProperty<FIntProperty>(ActorClass, TEXT("RootChildCountAtBeginPlay"));
		TestRunner->TestNotNull(TEXT("RootChildCountAtBeginPlay FIntProperty should be registered"), RootChildCountProp);

		FBoolProperty* SawSplineProp = FindFProperty<FBoolProperty>(ActorClass, TEXT("bSawSplineAtBeginPlay"));
		TestRunner->TestNotNull(TEXT("bSawSplineAtBeginPlay FBoolProperty should be registered"), SawSplineProp);

		FAngelscriptTestWorld W(*TestRunner, Engine);
		if (!W.IsValid()) { return; }

		AActor* Actor = W.SpawnActorOfClass(ActorClass);
		if (!TestRunner->TestNotNull(TEXT("Spline actor should spawn"), Actor)) { return; }

		USceneComponent* Root = RootProp != nullptr
			? Cast<USceneComponent>(RootProp->GetObjectPropertyValue_InContainer(Actor))
			: nullptr;
		USplineComponent* Spline = SplineProp != nullptr
			? Cast<USplineComponent>(SplineProp->GetObjectPropertyValue_InContainer(Actor))
			: nullptr;
		if (!TestRunner->TestNotNull(TEXT("Root default component should materialize"), Root)
			|| !TestRunner->TestNotNull(TEXT("Spline default component should materialize"), Spline))
		{
			return;
		}

		TestRunner->TestEqual(TEXT("Root property should point to the actor root component"), Root, Actor->GetRootComponent());
		TestRunner->TestEqual(TEXT("Spline default component should attach to the scripted root"), Spline->GetAttachParent(), Root);
		TestRunner->TestTrue(TEXT("Spline default component should register with the actor"), Spline->IsRegistered());
		TestRunner->TestTrue(TEXT("Spline component should expose native spline state after materialization"), Spline->GetNumberOfSplinePoints() >= 0);

		W.BeginPlay(*Actor);

		int32 RootChildCountAtBeginPlay = 0;
		bool bSawSplineAtBeginPlay = false;
		if (!ReadPropertyValue<FIntProperty>(*TestRunner, Actor, TEXT("RootChildCountAtBeginPlay"), RootChildCountAtBeginPlay)
			|| !ReadPropertyValue<FBoolProperty>(*TestRunner, Actor, TEXT("bSawSplineAtBeginPlay"), bSawSplineAtBeginPlay))
		{
			return;
		}

		TestRunner->TestTrue(TEXT("Script BeginPlay should see the attached spline through the root component"), RootChildCountAtBeginPlay >= 1);
		TestRunner->TestTrue(TEXT("Script BeginPlay should see a non-null spline component reference"), bSawSplineAtBeginPlay);
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
