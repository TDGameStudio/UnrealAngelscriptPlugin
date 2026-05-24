#include "Shared/AngelscriptLearningTrace.h"
#include "Shared/AngelscriptFunctionalTestUtils.h"
#include "Shared/AngelscriptTestEngineHelper.h"
#include "Shared/AngelscriptTestUtilities.h"
#include "Shared/AngelscriptTestMacros.h"
#include "Shared/AngelscriptNativeScriptTestObject.h"

#include "Components/ActorTestSpawner.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "UObject/ScriptDelegates.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptFunctionalTestUtils;

namespace AngelscriptTest_Learning_Runtime_AngelscriptLearningDelegateBridgeTraceTests_Private
{
	struct FLearningDelegateIntStringParams
	{
		int32 Value = 0;
		FString Label;
	};
void InitializeLearningDelegateSpawner(FActorTestSpawner& Spawner)
	{
		Spawner.InitializeGameSubsystems();
	}
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptLearningDelegateBridgeTraceTest,
	"Angelscript.TestModule.Learning.Runtime.DelegateBridge",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptLearningDelegateBridgeTraceTest::RunTest(const FString& Parameters)
{
	using namespace AngelscriptTest_Learning_Runtime_AngelscriptLearningDelegateBridgeTraceTests_Private;
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
	{ FAngelscriptEngineScope _AutoEngineScope(Engine);
	static const FName ModuleName(TEXT("LearningDelegateBridgeModule"));
	ON_SCOPE_EXIT
	{
		Engine.DiscardModule(*ModuleName.ToString());
		
	};

	const FString ScriptSource = TEXT(R"AS(
delegate void FLearningOnHealthChanged(int32 NewHealth, const FString& Label);

UCLASS()
class ALearningDelegateBridgeActor : AActor
{
	UPROPERTY()
	FLearningOnHealthChanged OnHealthChanged;

	UPROPERTY()
	float LastHealthValue = 0.0;

	UPROPERTY()
	FString LastHealthLabel;

	UFUNCTION()
	void TriggerHealthChanged(int32 NewHealth, const FString& Label)
	{
		LastHealthValue = NewHealth;
		LastHealthLabel = Label;
		if (OnHealthChanged.IsBound())
		{
			OnHealthChanged.Execute(NewHealth, Label);
		}
	}
}
)AS");

	FAngelscriptLearningTraceSinkConfig SinkConfig;
	SinkConfig.bEmitToAutomation = true;
	SinkConfig.bEmitToLog = true;
	SinkConfig.bEmitToFile = false;

	FAngelscriptLearningTraceSession Trace(TEXT("LearningDelegateBridge"), SinkConfig);
	Trace.BeginPhase(EAngelscriptLearningTracePhase::Compile);

	UClass* ScriptClass = CompileScriptModule(
		*this,
		Engine,
		ModuleName,
		TEXT("LearningDelegateBridgeModule.as"),
		ScriptSource,
		TEXT("ALearningDelegateBridgeActor"));
	if (ScriptClass == nullptr)
	{
		Trace.FlushToAutomation(*this);
		Trace.FlushToLog();
		return false;
	}

	Trace.AddStep(TEXT("CompileDelegateScript"), TEXT("Compiled the script class with a unicast delegate property"));
	Trace.AddKeyValue(TEXT("ScriptClassName"), ScriptClass->GetName());

	FDelegateProperty* DelegateProperty = FindFProperty<FDelegateProperty>(ScriptClass, TEXT("OnHealthChanged"));
	Trace.AddStep(TEXT("FindDelegateProperty"), DelegateProperty != nullptr ? TEXT("Located the delegate property on the generated script class") : TEXT("Failed to find delegate property"));
	Trace.AddKeyValue(TEXT("DelegatePropertyName"), DelegateProperty != nullptr ? DelegateProperty->GetName() : TEXT("<null>"));
	
	if (DelegateProperty != nullptr)
	{
		Trace.AddKeyValue(TEXT("DelegateSignatureFunction"), DelegateProperty->SignatureFunction != nullptr ? DelegateProperty->SignatureFunction->GetName() : TEXT("<null>"));
	}

	FActorTestSpawner Spawner;
	InitializeLearningDelegateSpawner(Spawner);
	AActor* Actor = SpawnScriptActor(*this, Spawner, ScriptClass);
	if (Actor == nullptr)
	{
		Trace.FlushToAutomation(*this);
		Trace.FlushToLog();
		return false;
	}

	Trace.AddStep(TEXT("SpawnDelegateActor"), TEXT("Spawned an instance of the script actor with delegate"));

	BeginPlayActor(*Actor);
	Trace.AddStep(TEXT("BeginPlayDelegateActor"), TEXT("Called BeginPlay on the spawned actor"));

	UAngelscriptNativeScriptTestObject* NativeReceiver = NewObject<UAngelscriptNativeScriptTestObject>(GetTransientPackage());
	if (NativeReceiver == nullptr)
	{
		Trace.FlushToAutomation(*this);
		Trace.FlushToLog();
		return false;
	}

	NativeReceiver->NameCounts.Reset();
	Trace.AddStep(TEXT("CreateNativeReceiver"), TEXT("Created a native test object to receive delegate callbacks"));

	if (DelegateProperty != nullptr)
	{
		FScriptDelegate BoundDelegate;
		BoundDelegate.BindUFunction(NativeReceiver, TEXT("SetIntStringFromDelegate"));
		*DelegateProperty->ContainerPtrToValuePtr<FScriptDelegate>(Actor) = BoundDelegate;

		Trace.AddStep(TEXT("BindNativeToDelegate"), TEXT("Bound the native test object's UFunction to the script delegate property"));
		Trace.AddKeyValue(TEXT("BoundFunctionName"), TEXT("SetIntStringFromDelegate"));
		Trace.AddKeyValue(TEXT("BindTargetClass"), NativeReceiver->GetClass()->GetName());
	}

	UFunction* TriggerFunction = FindGeneratedFunction(ScriptClass, TEXT("TriggerHealthChanged"));
	Trace.AddStep(TEXT("FindTriggerFunction"), TriggerFunction != nullptr ? TEXT("Located the script function that triggers the delegate execution") : TEXT("Failed to find trigger function"));
	Trace.AddKeyValue(TEXT("TriggerFunctionName"), TriggerFunction != nullptr ? TriggerFunction->GetName() : TEXT("<null>"));

	if (TriggerFunction != nullptr)
	{
		FLearningDelegateIntStringParams Params;
		Params.Value = 99;
		Params.Label = TEXT("BridgeTest");
		Actor->ProcessEvent(TriggerFunction, &Params);

		Trace.AddStep(TEXT("ExecuteTrigger"), TEXT("Invoked the script trigger function which should execute the bound delegate"));
		Trace.AddKeyValue(TEXT("ExecutedWithValue"), FString::FromInt(Params.Value));
		Trace.AddKeyValue(TEXT("ExecutedWithLabel"), Params.Label);
	}

	const int32 CallbackValue = NativeReceiver->NameCounts.FindRef(TEXT("BridgeTest"));
	Trace.AddStep(TEXT("VerifyCallbackReceived"), CallbackValue == 99 ? TEXT("Verified that the native callback received the correct value from script delegate execution") : TEXT("Callback did not receive expected value"));
	Trace.AddKeyValue(TEXT("CallbackReceivedValue"), FString::FromInt(CallbackValue));

	Trace.AddStep(TEXT("DelegateBridgeObservation"), TEXT("Delegate bridge in Angelscript stores signature information, allows BindUFunction binding from C++ side, and dispatches through script Execute() -> native UFunction call chain"));

	const bool bScriptClassCompiled = TestNotNull(TEXT("Delegate bridge learning script should produce a script actor class"), ScriptClass);
	const bool bDelegatePropertyFound = TestNotNull(TEXT("Delegate property should exist on generated class"), DelegateProperty);
	const bool bTriggerFunctionFound = TestNotNull(TEXT("Trigger function should exist"), TriggerFunction);
	const bool bCallbackCorrect = TestEqual(TEXT("Native callback should receive correct value from script delegate"), CallbackValue, 99);

	const bool bPhaseSequenceOk = AssertLearningTracePhaseSequence(*this, Trace.GetEvents(), {
		EAngelscriptLearningTracePhase::Compile,
	});
	const bool bContainsDelegateKeyword = AssertLearningTraceContainsKeyword(*this, Trace.GetEvents(), TEXT("DelegatePropertyName"));
	const bool bContainsBindKeyword = AssertLearningTraceContainsKeyword(*this, Trace.GetEvents(), TEXT("BoundFunctionName"));
	const bool bMinimumEventsOk = AssertLearningTraceMinimumEventCount(*this, Trace.GetEvents(), 8);

	Trace.FlushToAutomation(*this);
	Trace.FlushToLog();
	return bScriptClassCompiled
		&& bDelegatePropertyFound
		&& bTriggerFunctionFound
		&& bCallbackCorrect
		&& bPhaseSequenceOk
		&& bContainsDelegateKeyword
		&& bContainsBindKeyword
		&& bMinimumEventsOk;

	}
}

#endif
