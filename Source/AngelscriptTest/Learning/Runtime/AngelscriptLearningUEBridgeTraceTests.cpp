#include "Shared/AngelscriptLearningTrace.h"
#include "Shared/AngelscriptFunctionalTestUtils.h"
#include "Shared/AngelscriptTestEngineHelper.h"
#include "Shared/AngelscriptTestUtilities.h"
#include "Shared/AngelscriptTestMacros.h"

#include "Components/ActorTestSpawner.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptFunctionalTestUtils;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptLearningUEBridgeTraceTest,
	"Angelscript.TestModule.Learning.Runtime.UEBridge",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptLearningUEBridgeTraceTest::RunTest(const FString& Parameters)
{
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
	{ FAngelscriptEngineScope _AutoEngineScope(Engine);
	static const FName ModuleName(TEXT("LearningUEBridgeModule"));
	ON_SCOPE_EXIT
	{
		Engine.DiscardModule(*ModuleName.ToString());
		
	};

	const FString ScriptSource = TEXT(R"AS(
UCLASS()
class ALearningUEBridgeActor : AActor
{
	UPROPERTY()
	float HealthValue = 100.0f;

	UPROPERTY()
	FString ActorName = "BridgeActor";

	UPROPERTY()
	int32 EventCallCount = 0;

	UFUNCTION()
	void ApplyDamage(float Damage)
	{
		HealthValue -= Damage;
		EventCallCount++;
	}

	UFUNCTION()
	float GetHealth()
	{
		return HealthValue;
	}

	UFUNCTION(BlueprintOverride)
	void BeginPlay()
	{
		EventCallCount++;
	}
}
)AS");

	FAngelscriptLearningTraceSinkConfig SinkConfig;
	SinkConfig.bEmitToAutomation = true;
	SinkConfig.bEmitToLog = true;
	SinkConfig.bEmitToFile = false;

	FAngelscriptLearningTraceSession Trace(TEXT("LearningUEBridge"), SinkConfig);
	Trace.BeginPhase(EAngelscriptLearningTracePhase::UEBridge);

	UClass* ScriptClass = CompileScriptModule(
		*this,
		Engine,
		ModuleName,
		TEXT("LearningUEBridgeModule.as"),
		ScriptSource,
		TEXT("ALearningUEBridgeActor"));
	if (ScriptClass == nullptr)
	{
		Trace.FlushToAutomation(*this);
		Trace.FlushToLog();
		return false;
	}

	Trace.AddStep(TEXT("CompileBridgeScript"), TEXT("Compiled script class that will demonstrate UE object bridging"));
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

	Trace.AddStep(TEXT("SpawnBridgeActor"), TEXT("Spawned a UE actor instance from the script class"));

	BeginPlayActor(Engine, *Actor);
	Trace.AddStep(TEXT("BeginPlayBridgeActor"), TEXT("Called BeginPlay on the actor to initialize properties"));

	FProperty* HealthProp = FindFProperty<FProperty>(Actor->GetClass(), TEXT("HealthValue"));
	Trace.AddStep(TEXT("InspectProperty"), HealthProp != nullptr ? TEXT("Located a script property on the generated UE object") : TEXT("Failed to find property"));
	Trace.AddKeyValue(TEXT("PropertyName"), HealthProp != nullptr ? HealthProp->GetName() : TEXT("<null>"));
	Trace.AddKeyValue(TEXT("PropertyType"), HealthProp != nullptr ? HealthProp->GetClass()->GetName() : TEXT("<null>"));

	float InitialHealth = 0.0f;
	if (HealthProp != nullptr)
	{
		InitialHealth = *HealthProp->ContainerPtrToValuePtr<float>(Actor);
		Trace.AddStep(TEXT("ReadPropertyValue"), TEXT("Read the initial property value from the UE object"));
		Trace.AddKeyValue(TEXT("InitialHealth"), FString::SanitizeFloat(InitialHealth));
	}

	UFunction* ApplyDamageFunc = FindGeneratedFunction(ScriptClass, TEXT("ApplyDamage"));
	Trace.AddStep(TEXT("FindScriptUFunction"), ApplyDamageFunc != nullptr ? TEXT("Located the script function as a UE UFunction") : TEXT("Failed to find UFunction"));
	Trace.AddKeyValue(TEXT("FunctionName"), ApplyDamageFunc != nullptr ? ApplyDamageFunc->GetName() : TEXT("<null>"));

	if (ApplyDamageFunc != nullptr)
	{
		struct FApplyDamageParams
		{
			float Damage;
		};
		FApplyDamageParams Params;
		Params.Damage = 30.0f;

		FAngelscriptEngineScope ExecutionScope(Engine, Actor);
		Actor->ProcessEvent(ApplyDamageFunc, &Params);
		Trace.AddStep(TEXT("CallScriptFunction"), TEXT("Invoked the script function via UE ProcessEvent"));
		Trace.AddKeyValue(TEXT("CalledWithDamage"), FString::SanitizeFloat(Params.Damage));
	}

	float AfterDamageHealth = 0.0f;
	if (HealthProp != nullptr)
	{
		AfterDamageHealth = *HealthProp->ContainerPtrToValuePtr<float>(Actor);
		Trace.AddStep(TEXT("ReadModifiedProperty"), TEXT("Read the modified property value after script execution"));
		Trace.AddKeyValue(TEXT("AfterDamageHealth"), FString::SanitizeFloat(AfterDamageHealth));
	}

	Trace.AddStep(TEXT("UEBridgeObservation"), TEXT("UE bridge allows script classes to become UE actors; ProcessEvent enables calling script functions from UE; property changes are reflected on UE objects"));

	const bool bScriptClassCompiled = TestNotNull(TEXT("UEBridge learning script should produce a class"), ScriptClass);
	const bool bActorSpawned = TestNotNull(TEXT("Actor should spawn"), Actor);
	const bool bPropertyFound = TestNotNull(TEXT("Property should exist"), HealthProp);
	const bool bFunctionFound = TestNotNull(TEXT("Function should exist"), ApplyDamageFunc);

	const bool bPhaseSequenceOk = AssertLearningTracePhaseSequence(*this, Trace.GetEvents(), {
		EAngelscriptLearningTracePhase::UEBridge,
	});
	const bool bContainsPropertyKeyword = AssertLearningTraceContainsKeyword(*this, Trace.GetEvents(), TEXT("PropertyName"));
	const bool bContainsFunctionKeyword = AssertLearningTraceContainsKeyword(*this, Trace.GetEvents(), TEXT("FunctionName"));
	const bool bMinimumEventsOk = AssertLearningTraceMinimumEventCount(*this, Trace.GetEvents(), 7);

	Trace.FlushToAutomation(*this);
	Trace.FlushToLog();
	return bScriptClassCompiled
		&& bActorSpawned
		&& bPropertyFound
		&& bFunctionFound
		&& bPhaseSequenceOk
		&& bContainsPropertyKeyword
		&& bContainsFunctionKeyword
		&& bMinimumEventsOk;

	}
}

#endif
