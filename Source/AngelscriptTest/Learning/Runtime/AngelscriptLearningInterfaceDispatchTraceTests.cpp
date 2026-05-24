#include "AngelscriptLearningTrace.h"
#include "AngelscriptFunctionalTestUtils.h"
#include "AngelscriptNativeInterfaceTestHelpers.h"
#include "AngelscriptNativeInterfaceTestTypes.h"
#include "AngelscriptTestEngineHelper.h"
#include "AngelscriptTestUtilities.h"
#include "AngelscriptTestMacros.h"

#include "Components/ActorTestSpawner.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptFunctionalTestUtils;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptLearningInterfaceDispatchTraceTest,
	"Angelscript.TestModule.Learning.Runtime.InterfaceDispatch",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptLearningInterfaceDispatchTraceTest::RunTest(const FString& Parameters)
{
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
	{ FAngelscriptEngineScope _AutoEngineScope(Engine);
	static const FName ModuleName(TEXT("LearningInterfaceDispatchModule"));
	ON_SCOPE_EXIT
	{
		Engine.DiscardModule(*ModuleName.ToString());
		ASTEST_RESET_ENGINE(Engine);
	};

	AngelscriptNativeInterfaceTestHelpers::EnsureNativeInterfaceBound(UAngelscriptNativeParentInterface::StaticClass());

	const FString ScriptSource = TEXT(R"AS(
UCLASS()
class ALearningInterfaceDispatchActor : AActor, UAngelscriptNativeParentInterface
{
	UPROPERTY()
	int ScriptObservedValue = 0;

	UPROPERTY()
	int ScriptAdjustedValue = 0;

	UPROPERTY()
	FName ScriptObservedMarker = NAME_None;

	UFUNCTION()
	int GetNativeValue() const
	{
		return 42;
	}

	UFUNCTION()
	void SetNativeMarker(FName Marker)
	{
		ScriptObservedMarker = Marker;
	}

	UFUNCTION()
	void AdjustNativeValue(int Delta, int& Value)
	{
		Value += Delta;
	}

	UFUNCTION(BlueprintOverride)
	void BeginPlay()
	{
		UObject Self = this;
		UAngelscriptNativeParentInterface ParentRef = Cast<UAngelscriptNativeParentInterface>(Self);
		if (ParentRef == nullptr)
			return;

		ScriptObservedValue = ParentRef.GetNativeValue();

		int Value = 10;
		ParentRef.AdjustNativeValue(5, Value);
		ScriptAdjustedValue = Value;

		ParentRef.SetNativeMarker(n"LearningBridgeHit");
	}
}
)AS");

	FAngelscriptLearningTraceSinkConfig SinkConfig;
	SinkConfig.bEmitToAutomation = true;
	SinkConfig.bEmitToLog = true;
	SinkConfig.bEmitToFile = false;

	FAngelscriptLearningTraceSession Trace(TEXT("LearningInterfaceDispatch"), SinkConfig);
	Trace.BeginPhase(EAngelscriptLearningTracePhase::Compile);

	UClass* ScriptClass = CompileScriptModule(
		*this,
		Engine,
		ModuleName,
		TEXT("LearningInterfaceDispatchModule.as"),
		ScriptSource,
		TEXT("ALearningInterfaceDispatchActor"));
	if (ScriptClass == nullptr)
	{
		Trace.FlushToAutomation(*this);
		Trace.FlushToLog();
		return false;
	}

	Trace.AddStep(TEXT("CompileNativeInterfaceImplementer"), TEXT("Compiled an actor class that implements a native UInterface exposed through the Angelscript bridge"));
	Trace.AddKeyValue(TEXT("ScriptClassName"), ScriptClass->GetName());

	UClass* InterfaceClass = UAngelscriptNativeParentInterface::StaticClass();
	Trace.AddStep(TEXT("ResolveNativeInterface"), InterfaceClass != nullptr ? TEXT("Resolved the native UInterface class used by the script actor") : TEXT("Failed to resolve the native UInterface class"));
	Trace.AddKeyValue(TEXT("InterfaceClassName"), InterfaceClass != nullptr ? InterfaceClass->GetName() : TEXT("<null>"));

	FActorTestSpawner Spawner;
	Spawner.InitializeGameSubsystems();
	AActor* Actor = SpawnScriptActor(*this, Spawner, ScriptClass);
	if (Actor == nullptr)
	{
		Trace.FlushToAutomation(*this);
		Trace.FlushToLog();
		return false;
	}

	Trace.AddStep(TEXT("SpawnImplementerActor"), TEXT("Spawned an actor instance of the script class that implements the interface"));
	BeginPlayActor(Engine, *Actor);
	Trace.AddStep(TEXT("DispatchInterfaceCalls"), TEXT("Dispatched interface calls through the native bridge during BeginPlay"));

	const bool bImplements = InterfaceClass != nullptr && Actor->GetClass()->ImplementsInterface(InterfaceClass);
	Trace.AddStep(TEXT("CheckImplementsInterface"), bImplements ? TEXT("Verified that the spawned actor's class reports ImplementsInterface for the native UInterface") : TEXT("ImplementsInterface check failed"));
	Trace.AddKeyValue(TEXT("ImplementsInterface"), bImplements ? TEXT("true") : TEXT("false"));

	if (InterfaceClass != nullptr)
	{
		UFunction* GetNativeValueFunc = InterfaceClass->FindFunctionByName(TEXT("GetNativeValue"));
		Trace.AddStep(TEXT("InspectInterfaceUFunction"), GetNativeValueFunc != nullptr ? TEXT("Located the UFunction representing the native interface method") : TEXT("Failed to find interface UFunction"));
		Trace.AddKeyValue(TEXT("InterfaceUFunctionName"), GetNativeValueFunc != nullptr ? GetNativeValueFunc->GetName() : TEXT("<null>"));
		Trace.AddKeyValue(TEXT("InterfaceUFunctionFlags"), GetNativeValueFunc != nullptr ? FString::Printf(TEXT("0x%08X"), GetNativeValueFunc->FunctionFlags) : TEXT("<null>"));
	}

	int32 ScriptObservedValue = INDEX_NONE;
	int32 ScriptAdjustedValue = INDEX_NONE;
	FName ScriptObservedMarker = NAME_None;
	const bool bReadDispatchState =
		ReadPropertyValue<FIntProperty>(*this, Actor, TEXT("ScriptObservedValue"), ScriptObservedValue)
		&& ReadPropertyValue<FIntProperty>(*this, Actor, TEXT("ScriptAdjustedValue"), ScriptAdjustedValue)
		&& ReadPropertyValue<FNameProperty>(*this, Actor, TEXT("ScriptObservedMarker"), ScriptObservedMarker);

	Trace.AddKeyValue(TEXT("ScriptObservedValue"), FString::FromInt(ScriptObservedValue));
	Trace.AddKeyValue(TEXT("ScriptAdjustedValue"), FString::FromInt(ScriptAdjustedValue));
	Trace.AddKeyValue(TEXT("ScriptObservedMarker"), ScriptObservedMarker.ToString());
	Trace.AddStep(TEXT("InterfaceDispatchBoundary"), TEXT("Interface dispatch in Angelscript is handled by the native bridge; the trace verifies the observable script-side state after dispatch"));

	const bool bScriptClassCompiled = TestNotNull(TEXT("Interface dispatch learning script should produce a script actor class"), ScriptClass);
	const bool bInterfaceResolved = TestNotNull(TEXT("Interface dispatch learning script should use a native UInterface"), InterfaceClass);
	const bool bImplementsInterface = TestTrue(TEXT("Spawned actor should implement the native interface"), bImplements);
	const bool bDispatchReadSucceeded = TestTrue(TEXT("Interface dispatch learning test should read dispatch result properties"), bReadDispatchState);
	const bool bDispatchValueMatched = TestEqual(TEXT("Interface dispatch should call GetNativeValue on the script implementer"), ScriptObservedValue, 42);
	const bool bDispatchRefMatched = TestEqual(TEXT("Interface dispatch should round-trip ref parameters"), ScriptAdjustedValue, 15);
	const bool bDispatchMarkerMatched = TestEqual(TEXT("Interface dispatch should route payload arguments"), ScriptObservedMarker, FName(TEXT("LearningBridgeHit")));

	const bool bPhaseSequenceOk = AssertLearningTracePhaseSequence(*this, Trace.GetEvents(), {
		EAngelscriptLearningTracePhase::Compile,
	});
	const bool bContainsInterfaceKeyword = AssertLearningTraceContainsKeyword(*this, Trace.GetEvents(), TEXT("InterfaceClassName"));
	const bool bContainsImplementsKeyword = AssertLearningTraceContainsKeyword(*this, Trace.GetEvents(), TEXT("ImplementsInterface"));
	const bool bContainsDispatchKeyword = AssertLearningTraceContainsKeyword(*this, Trace.GetEvents(), TEXT("ScriptObservedValue"));
	const bool bMinimumEventsOk = AssertLearningTraceMinimumEventCount(*this, Trace.GetEvents(), 5);

	Trace.FlushToAutomation(*this);
	Trace.FlushToLog();
	return bScriptClassCompiled
		&& bInterfaceResolved
		&& bImplementsInterface
		&& bDispatchReadSucceeded
		&& bDispatchValueMatched
		&& bDispatchRefMatched
		&& bDispatchMarkerMatched
		&& bPhaseSequenceOk
		&& bContainsInterfaceKeyword
		&& bContainsImplementsKeyword
		&& bContainsDispatchKeyword
		&& bMinimumEventsOk;

	}
}

#endif
