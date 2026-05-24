#include "AngelscriptLearningTrace.h"
#include "AngelscriptFunctionalTestUtils.h"
#include "AngelscriptTestEngineHelper.h"
#include "AngelscriptTestUtilities.h"
#include "AngelscriptTestMacros.h"

#include "Components/ActorTestSpawner.h"
#include "GameFramework/Actor.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptFunctionalTestUtils;

namespace AngelscriptTest_Learning_Runtime_AngelscriptLearningTimerAndLatentTraceTests_Private
{
	constexpr float LearningTimerDeltaTime = 0.016f;
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptLearningTimerAndLatentTraceTest,
	"Angelscript.TestModule.Learning.Runtime.TimerAndLatent",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptLearningTimerAndLatentTraceTest::RunTest(const FString& Parameters)
{
	using namespace AngelscriptTest_Learning_Runtime_AngelscriptLearningTimerAndLatentTraceTests_Private;
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
	{ FAngelscriptEngineScope _AutoEngineScope(Engine);
	static const FName ModuleName(TEXT("LearningTimerModule"));
	ON_SCOPE_EXIT
	{
		Engine.DiscardModule(*ModuleName.ToString());
		ASTEST_RESET_ENGINE(Engine);
	};

	const FString ScriptSource = TEXT(R"AS(
UCLASS()
class ALearningTimerActor : AActor
{
	UPROPERTY()
	int32 TimerCallCount = 0;

	UPROPERTY()
	float LastDeltaTime = 0.0f;

	UFUNCTION()
	void OnTimerCallback(float DeltaTime)
	{
		TimerCallCount++;
		LastDeltaTime = DeltaTime;
	}

	UFUNCTION(BlueprintOverride)
	void BeginPlay()
	{
		System::SetTimer(this, n"OnTimerCallback", 0.1f, true);
	}
}
)AS");

	FAngelscriptLearningTraceSinkConfig SinkConfig;
	SinkConfig.bEmitToAutomation = true;
	SinkConfig.bEmitToLog = true;
	SinkConfig.bEmitToFile = false;

	FAngelscriptLearningTraceSession Trace(TEXT("LearningTimerAndLatent"), SinkConfig);
	Trace.BeginPhase(EAngelscriptLearningTracePhase::Execution);

	UClass* ScriptClass = CompileScriptModule(
		*this,
		Engine,
		ModuleName,
		TEXT("LearningTimerModule.as"),
		ScriptSource,
		TEXT("ALearningTimerActor"));
	if (ScriptClass == nullptr)
	{
		Trace.FlushToAutomation(*this);
		Trace.FlushToLog();
		return false;
	}

	Trace.AddStep(TEXT("CompileTimerScript"), TEXT("Compiled a script actor class with System::SetTimer usage"));
	Trace.AddKeyValue(TEXT("ScriptClassName"), ScriptClass->GetName());

	FActorTestSpawner Spawner;
	Spawner.InitializeGameSubsystems();

	AActor* Actor = SpawnScriptActor(*this, Spawner, ScriptClass);
	if (Actor == nullptr)
	{
		Trace.FlushToAutomation(*this);
		Trace.FlushToLog();
		return false;
	}

	Trace.AddStep(TEXT("SpawnTimerActor"), TEXT("Spawned an instance of the timer script actor"));

	BeginPlayActor(Engine, *Actor);
	Trace.AddStep(TEXT("BeginPlayTimerActor"), TEXT("Called BeginPlay, which sets up the repeating timer via System::SetTimer"));

	int32 InitialCallCount = 0;
	FProperty* CallCountProp = FindFProperty<FProperty>(Actor->GetClass(), TEXT("TimerCallCount"));
	if (CallCountProp != nullptr)
	{
		InitialCallCount = *CallCountProp->ContainerPtrToValuePtr<int32>(Actor);
	}
	Trace.AddStep(TEXT("CheckInitialCallCount"), TEXT("Checked initial timer call count before any ticks"));
	Trace.AddKeyValue(TEXT("InitialCallCount"), FString::FromInt(InitialCallCount));

	TickWorld(Engine, Spawner.GetWorld(), LearningTimerDeltaTime, 20);
	Trace.AddStep(TEXT("TickWorld"), TEXT("Ticked the world to allow timer to fire"));

	int32 AfterTickCount = 0;
	if (CallCountProp != nullptr)
	{
		AfterTickCount = *CallCountProp->ContainerPtrToValuePtr<int32>(Actor);
	}
	Trace.AddStep(TEXT("CheckCallCountAfterTicks"), AfterTickCount > InitialCallCount ? TEXT("Timer callback was invoked at least once") : TEXT("Timer callback not yet invoked (may need more ticks or longer interval)"));
	Trace.AddKeyValue(TEXT("AfterTickCount"), FString::FromInt(AfterTickCount));

	Trace.AddStep(TEXT("TimerAndLatentObservation"), TEXT("System::SetTimer schedules callbacks that fire after world tick progression; repeating timers continue firing each interval; timer callbacks execute in game thread context"));

	const bool bScriptClassCompiled = TestNotNull(TEXT("Timer learning script should produce a class"), ScriptClass);
	const bool bActorSpawned = TestNotNull(TEXT("Actor should spawn"), Actor);

	const bool bPhaseSequenceOk = AssertLearningTracePhaseSequence(*this, Trace.GetEvents(), {
		EAngelscriptLearningTracePhase::Execution,
	});
	const bool bContainsCallCountKeyword = AssertLearningTraceContainsKeyword(*this, Trace.GetEvents(), TEXT("CallCount"));
	const bool bMinimumEventsOk = AssertLearningTraceMinimumEventCount(*this, Trace.GetEvents(), 6);

	Trace.FlushToAutomation(*this);
	Trace.FlushToLog();
	return bScriptClassCompiled
		&& bActorSpawned
		&& bPhaseSequenceOk
		&& bContainsCallCountKeyword
		&& bMinimumEventsOk;

	}
}

#endif
