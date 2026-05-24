#include "AngelscriptLearningTrace.h"
#include "AngelscriptFunctionalTestUtils.h"
#include "AngelscriptTestEngineHelper.h"
#include "AngelscriptTestUtilities.h"
#include "AngelscriptTestMacros.h"

#include "Components/ActorTestSpawner.h"
#include "GameFramework/Actor.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "UObject/GarbageCollection.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptFunctionalTestUtils;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptLearningGCTraceTest,
	"Angelscript.TestModule.Learning.Runtime.GC",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptLearningGCTraceTest::RunTest(const FString& Parameters)
{
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
	{ FAngelscriptEngineScope _AutoEngineScope(Engine);
	static const FName ModuleName(TEXT("LearningGCModule"));
	ON_SCOPE_EXIT
	{
		Engine.DiscardModule(*ModuleName.ToString());
		ASTEST_RESET_ENGINE(Engine);
	};

	const FString ScriptSource = TEXT(R"AS(
UCLASS()
class ALearningGCTestActor : AActor
{
	UPROPERTY()
	int32 InstanceId = 0;

	UPROPERTY()
	FString ActorLabel;

	UFUNCTION(BlueprintOverride)
	void BeginPlay()
	{
		ActorLabel = "GCTestActor";
	}
}
)AS");

	FAngelscriptLearningTraceSinkConfig SinkConfig;
	SinkConfig.bEmitToAutomation = true;
	SinkConfig.bEmitToLog = true;
	SinkConfig.bEmitToFile = false;

	FAngelscriptLearningTraceSession Trace(TEXT("LearningGC"), SinkConfig);
	Trace.BeginPhase(EAngelscriptLearningTracePhase::GC);

	UClass* ScriptClass = CompileScriptModule(
		*this,
		Engine,
		ModuleName,
		TEXT("LearningGCModule.as"),
		ScriptSource,
		TEXT("ALearningGCTestActor"));
	if (ScriptClass == nullptr)
	{
		Trace.FlushToAutomation(*this);
		Trace.FlushToLog();
		return false;
	}

	Trace.AddStep(TEXT("CompileGCScript"), TEXT("Compiled a script actor class for GC testing"));
	Trace.AddKeyValue(TEXT("ScriptClassName"), ScriptClass->GetName());

	FActorTestSpawner Spawner;
	Spawner.InitializeGameSubsystems();

	AActor* Actor1 = SpawnScriptActor(*this, Spawner, ScriptClass);
	if (Actor1 == nullptr)
	{
		Trace.FlushToAutomation(*this);
		Trace.FlushToLog();
		return false;
	}

	Trace.AddStep(TEXT("SpawnFirstActor"), TEXT("Spawned first script actor instance"));
	Trace.AddKeyValue(TEXT("Actor1Name"), Actor1->GetName());

	FWeakObjectPtr WeakActor1(Actor1);
	Trace.AddStep(TEXT("CreateWeakReference"), TEXT("Created a weak reference to track GC behavior"));
	Trace.AddKeyValue(TEXT("WeakReferenceValid"), WeakActor1.IsValid() ? TEXT("true") : TEXT("false"));

	AActor* Actor2 = SpawnScriptActor(*this, Spawner, ScriptClass);
	if (Actor2 == nullptr)
	{
		Trace.FlushToAutomation(*this);
		Trace.FlushToLog();
		return false;
	}

	Trace.AddStep(TEXT("SpawnSecondActor"), TEXT("Spawned second script actor instance"));
	Trace.AddKeyValue(TEXT("Actor2Name"), Actor2->GetName());

	Trace.AddStep(TEXT("GCBeforeDestroy"), TEXT("Both actors are live before explicit destruction"));

	Actor1->K2_DestroyActor();
	Trace.AddStep(TEXT("DestroyFirstActor"), TEXT("Called K2_DestroyActor on the first actor"));
	Trace.AddKeyValue(TEXT("Actor1Destroyed"), TEXT("true"));

	CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS, true);
	Trace.AddStep(TEXT("ForceGarbageCollection"), TEXT("Forced a full garbage collection pass"));

	bool bActor1Collected = !WeakActor1.IsValid();
	Trace.AddStep(TEXT("CheckWeakReferenceAfterGC"), bActor1Collected ? TEXT("Weak reference invalidated - actor was garbage collected") : TEXT("Weak reference still valid - actor not yet collected"));
	Trace.AddKeyValue(TEXT("WeakReferenceAfterGC"), WeakActor1.IsValid() ? TEXT("valid") : TEXT("invalidated"));

	bool bActor2StillValid = Actor2 != nullptr && !Actor2->IsPendingKillPending();
	Trace.AddStep(TEXT("VerifySecondActorAlive"), bActor2StillValid ? TEXT("Second actor remains alive after GC") : TEXT("Second actor was unexpectedly affected"));
	Trace.AddKeyValue(TEXT("Actor2StillValid"), bActor2StillValid ? TEXT("true") : TEXT("false"));

	Trace.AddStep(TEXT("GCObservation"), TEXT("UE garbage collection properly cleans up destroyed script actors; weak references detect when objects are collected; undestroyed actors survive GC cycles"));

	const bool bScriptClassCompiled = TestNotNull(TEXT("GC learning script should produce a class"), ScriptClass);
	const bool bActorsSpawned = TestNotNull(TEXT("Both actors should spawn"), Actor1) && TestNotNull(TEXT("Second actor should spawn"), Actor2);
	const bool bGCBehaviorObserved = TestTrue(TEXT("Destroyed actor should be collected after GC"), bActor1Collected);

	const bool bPhaseSequenceOk = AssertLearningTracePhaseSequence(*this, Trace.GetEvents(), {
		EAngelscriptLearningTracePhase::GC,
	});
	const bool bContainsWeakKeyword = AssertLearningTraceContainsKeyword(*this, Trace.GetEvents(), TEXT("WeakReference"));
	const bool bContainsGCKeyword = AssertLearningTraceContainsKeyword(*this, Trace.GetEvents(), TEXT("GarbageCollection"));
	const bool bMinimumEventsOk = AssertLearningTraceMinimumEventCount(*this, Trace.GetEvents(), 7);

	Trace.FlushToAutomation(*this);
	Trace.FlushToLog();
	return bScriptClassCompiled
		&& bActorsSpawned
		&& bGCBehaviorObserved
		&& bPhaseSequenceOk
		&& bContainsWeakKeyword
		&& bContainsGCKeyword
		&& bMinimumEventsOk;

	}
}

#endif
