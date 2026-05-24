#include "Shared/AngelscriptLearningTrace.h"
#include "Shared/AngelscriptFunctionalTestUtils.h"
#include "Shared/AngelscriptTestEngineHelper.h"
#include "Shared/AngelscriptTestUtilities.h"
#include "Shared/AngelscriptTestMacros.h"

#include "Components/ActorTestSpawner.h"
#include "Core/AngelscriptComponent.h"
#include "Components/SceneComponent.h"
#include "GameFramework/Actor.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptFunctionalTestUtils;

namespace AngelscriptTest_Learning_Runtime_AngelscriptLearningComponentHierarchyTraceTests_Private
{
	constexpr float LearningComponentDeltaTime = 0.016f;

	void InitializeLearningComponentSpawner(FActorTestSpawner& Spawner)
	{
		Spawner.InitializeGameSubsystems();
	}
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptLearningComponentHierarchyTraceTest,
	"Angelscript.TestModule.Learning.Runtime.ComponentHierarchy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptLearningComponentHierarchyTraceTest::RunTest(const FString& Parameters)
{
	using namespace AngelscriptTest_Learning_Runtime_AngelscriptLearningComponentHierarchyTraceTests_Private;
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
	{ FAngelscriptEngineScope _AutoEngineScope(Engine);
	static const FName ModuleName(TEXT("LearningComponentHierarchyModule"));
	ON_SCOPE_EXIT
	{
		Engine.DiscardModule(*ModuleName.ToString());
		
	};

	const FString ScriptSource = TEXT(R"AS(
UCLASS()
class ULearningSampleComponent : UAngelscriptComponent
{
	UPROPERTY()
	float SampleValue = 42.0f;

	UPROPERTY()
	FString ComponentLabel;

	UFUNCTION(BlueprintOverride)
	void BeginPlay()
	{
		ComponentLabel = "LearningComponent";
	}

	UFUNCTION(BlueprintOverride)
	void Tick(float DeltaTime)
	{
	}
}

UCLASS()
class ALearningComponentHierarchyActor : AActor
{
	UPROPERTY(DefaultComponent)
	ULearningSampleComponent FirstComponent;

	UPROPERTY(DefaultComponent)
	ULearningSampleComponent SecondComponent;

	UFUNCTION(BlueprintOverride)
	void BeginPlay()
	{
	}
}
)AS");

	FAngelscriptLearningTraceSinkConfig SinkConfig;
	SinkConfig.bEmitToAutomation = true;
	SinkConfig.bEmitToLog = true;
	SinkConfig.bEmitToFile = false;

	FAngelscriptLearningTraceSession Trace(TEXT("LearningComponentHierarchy"), SinkConfig);
	Trace.BeginPhase(EAngelscriptLearningTracePhase::Compile);

	UClass* ActorClass = CompileScriptModule(
		*this,
		Engine,
		ModuleName,
		TEXT("LearningComponentHierarchyModule.as"),
		ScriptSource,
		TEXT("ALearningComponentHierarchyActor"));
	if (ActorClass == nullptr)
	{
		Trace.FlushToAutomation(*this);
		Trace.FlushToLog();
		return false;
	}

	Trace.AddStep(TEXT("CompileActorWithComponents"), TEXT("Compiled the actor class with default components and component hierarchy"));
	Trace.AddKeyValue(TEXT("ActorClassName"), ActorClass->GetName());

	UClass* ComponentClass = FindGeneratedClass(&Engine, TEXT("ULearningSampleComponent"));
	Trace.AddStep(TEXT("FindGeneratedComponentClass"), ComponentClass != nullptr ? TEXT("Located the generated component class from the script") : TEXT("Failed to find generated component class"));
	Trace.AddKeyValue(TEXT("ComponentClassName"), ComponentClass != nullptr ? ComponentClass->GetName() : TEXT("<null>"));

	FActorTestSpawner Spawner;
	InitializeLearningComponentSpawner(Spawner);
	AActor* Actor = SpawnScriptActor(*this, Spawner, ActorClass);
	if (Actor == nullptr)
	{
		Trace.FlushToAutomation(*this);
		Trace.FlushToLog();
		return false;
	}

	Trace.AddStep(TEXT("SpawnActorWithComponents"), TEXT("Spawned an actor instance with default components created"));

	TArray<UActorComponent*> Components;
	Actor->GetComponents(Components);

	int32 ComponentCount = Components.Num();
	Trace.AddStep(TEXT("InspectComponentCount"), TEXT("Enumerated all components attached to the actor"));
	Trace.AddKeyValue(TEXT("TotalComponentCount"), FString::FromInt(ComponentCount));

	int32 ScriptComponentCount = 0;
	for (UActorComponent* Comp : Components)
	{
		if (Comp->GetClass()->GetName().Contains(TEXT("LearningSampleComponent")))
		{
			ScriptComponentCount++;
		}
	}
	Trace.AddKeyValue(TEXT("ScriptComponentCount"), FString::FromInt(ScriptComponentCount));

	BeginPlayActor(Engine, *Actor);
	Trace.AddStep(TEXT("BeginPlayComponents"), TEXT("Called BeginPlay on the actor, which triggers BeginPlay on components"));

	TickWorld(Engine, Spawner.GetWorld(), LearningComponentDeltaTime, 1);
	Trace.AddStep(TEXT("TickComponents"), TEXT("Ticked the world with a delta time, which propagates to component Tick"));

	Trace.AddStep(TEXT("ComponentHierarchyObservation"), TEXT("Component hierarchy in Angelscript supports DefaultComponent specifier, RootComponent designation, and Attach relationships; component lifecycle mirrors actor lifecycle with BeginPlay/Tick"));

	const bool bActorClassCompiled = TestNotNull(TEXT("Component hierarchy learning script should produce an actor class"), ActorClass);
	const bool bComponentClassFound = TestNotNull(TEXT("Component class should be generated"), ComponentClass);
	const bool bHasComponents = TestTrue(TEXT("Actor should have script components"), ScriptComponentCount >= 2);

	const bool bPhaseSequenceOk = AssertLearningTracePhaseSequence(*this, Trace.GetEvents(), {
		EAngelscriptLearningTracePhase::Compile,
	});
	const bool bContainsComponentKeyword = AssertLearningTraceContainsKeyword(*this, Trace.GetEvents(), TEXT("ComponentClassName"));
	const bool bContainsCountKeyword = AssertLearningTraceContainsKeyword(*this, Trace.GetEvents(), TEXT("ScriptComponentCount"));
	const bool bMinimumEventsOk = AssertLearningTraceMinimumEventCount(*this, Trace.GetEvents(), 7);

	Trace.FlushToAutomation(*this);
	Trace.FlushToLog();
	return bActorClassCompiled
		&& bComponentClassFound
		&& bHasComponents
		&& bPhaseSequenceOk
		&& bContainsComponentKeyword
		&& bContainsCountKeyword
		&& bMinimumEventsOk;

	}
}

#endif
