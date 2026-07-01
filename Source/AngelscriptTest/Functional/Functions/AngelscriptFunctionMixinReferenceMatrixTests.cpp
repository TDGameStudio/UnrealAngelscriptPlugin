#include "AngelscriptFunctionalTestUtils.h"
#include "AngelscriptTestMacros.h"

#include "Components/ActorTestSpawner.h"
#include "GameFramework/Actor.h"
#include "CQTest.h"
#include "Misc/ScopeExit.h"
#include "UObject/UnrealType.h"

// Test Layer: UE Functional - Round1 deep-fill (mixin signature matrix and default args)
#if WITH_ANGELSCRIPT_UNITTESTS


TEST_CLASS_WITH_FLAGS(FAngelscriptFunctionMixinReferenceMatrixTests,
	"Angelscript.TestModule.Functional.Functions.MixinReferenceMatrix",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(MixinSignaturesCompileAndDispatchAtRuntime)
	{
		using namespace AngelscriptFunctionalTestUtils;
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		FAngelscriptEngineScope EngineScope(Engine);

		static const FName ModuleName(TEXT("FunctionalMixinReferenceMatrix"));
		ON_SCOPE_EXIT { Engine.DiscardModule(*ModuleName.ToString()); };

		UClass* ActorClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("FunctionalMixinReferenceMatrix.as"),
			TEXT(R"AS(
mixin void TagMixin(AFunctionalMixinHostActor Self)
{
	Self.SimpleTagged = true;
}

mixin void TagDefaultedMixin(AFunctionalMixinHostActor Self, int Increment = 5)
{
	Self.AccumulatedScore += Increment;
}

mixin void TagPairMixin(AFunctionalMixinHostActor Self, AFunctionalMixinHostActor Other, float Threshold = 500.0)
{
	Self.PairResult = (Threshold > 0.0 && Other != nullptr);
}

UCLASS()
class AFunctionalMixinHostActor : AActor
{
	UPROPERTY()
	bool SimpleTagged = false;

	UPROPERTY()
	int32 AccumulatedScore = 0;

	UPROPERTY()
	bool PairResult = false;

	UFUNCTION(BlueprintOverride)
	void BeginPlay()
	{
		this.TagMixin();
		this.TagDefaultedMixin();
		this.TagDefaultedMixin(7);

		AFunctionalMixinHostActor Other = this;
		this.TagPairMixin(Other);
	}
}
)AS"),
			TEXT("AFunctionalMixinHostActor"));
		if (ActorClass == nullptr) { return; }

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ActorClass);
		if (Actor == nullptr) { return; }
		BeginPlayActor(Engine, *Actor);

		bool bSimpleTagged = false;
		ReadPropertyValue<FBoolProperty>(*TestRunner, Actor, TEXT("SimpleTagged"), bSimpleTagged);
		ASSERT_THAT(IsTrue(bSimpleTagged, TEXT("Single-arg mixin should set SimpleTagged")));

		int32 Score = 0;
		ReadPropertyValue<FIntProperty>(*TestRunner, Actor, TEXT("AccumulatedScore"), Score);
		ASSERT_THAT(AreEqual(12, Score, TEXT("Default-argument mixin should accumulate 5 + 7 = 12")));

		bool bPairResult = false;
		ReadPropertyValue<FBoolProperty>(*TestRunner, Actor, TEXT("PairResult"), bPairResult);
		ASSERT_THAT(IsTrue(bPairResult, TEXT("Pair mixin with default Threshold should evaluate true")));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
