#include "Shared/AngelscriptLearningTrace.h"
#include "AngelScriptSDK/AngelscriptNativeTestSupport.h"

#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptNativeTestSupport;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptLearningExecutionTraceTest,
	"Angelscript.TestModule.Learning.Runtime.ExecutionLifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptLearningExecutionTraceTest::RunTest(const FString& Parameters)
{
	FAngelscriptLearningTraceSinkConfig SinkConfig;
	SinkConfig.bEmitToAutomation = true;
	SinkConfig.bEmitToLog = true;
	SinkConfig.bEmitToFile = false;

	FAngelscriptLearningTraceSession Trace(TEXT("LearningExecutionLifecycle"), SinkConfig);
	Trace.BeginPhase(EAngelscriptLearningTracePhase::Execution);

	FNativeMessageCollector Messages;
	asIScriptEngine* Engine = CreateNativeEngine(&Messages);
	if (Engine == nullptr)
	{
		Trace.FlushToAutomation(*this);
		Trace.FlushToLog();
		return false;
	}

	ON_SCOPE_EXIT
	{
		DestroyNativeEngine(Engine);
	};

	Trace.AddStep(TEXT("CreateNativeEngine"), TEXT("Created native Angelscript engine instance for execution testing"));

	const char* ScriptCode = R"AS(
int LearningAdd(int a, int b)
{
	return a + b;
}
)AS";

	asIScriptModule* Module = BuildNativeModule(Engine, "LearningExecutionModule", ScriptCode);
	if (Module == nullptr)
	{
		Trace.FlushToAutomation(*this);
		Trace.FlushToLog();
		return false;
	}

	Trace.AddStep(TEXT("BuildExecutionModule"), TEXT("Built a script module with an executable function"));

	asIScriptFunction* Function = GetNativeFunctionByDecl(Module, "int LearningAdd(int, int)");
	Trace.AddStep(TEXT("FindExecutableFunction"), Function != nullptr ? TEXT("Located the function by its declaration signature") : TEXT("Failed to find function"));
	Trace.AddKeyValue(TEXT("FunctionDeclaration"), Function != nullptr ? UTF8_TO_TCHAR(Function->GetDeclaration(true, true)) : TEXT("<null>"));

	if (Function == nullptr)
	{
		Trace.FlushToAutomation(*this);
		Trace.FlushToLog();
		return false;
	}

	asIScriptContext* Context = Engine->CreateContext();
	Trace.AddStep(TEXT("CreateExecutionContext"), Context != nullptr ? TEXT("Created a script execution context") : TEXT("Failed to create context"));
	Trace.AddKeyValue(TEXT("ContextPointer"), Context != nullptr ? FString::Printf(TEXT("%p"), Context) : TEXT("<null>"));

	if (Context == nullptr)
	{
		Trace.FlushToAutomation(*this);
		Trace.FlushToLog();
		return false;
	}

	int PrepareResult = Context->Prepare(Function);
	Trace.AddStep(TEXT("PrepareContext"), PrepareResult >= 0 ? TEXT("Prepared the context with the target function") : TEXT("Failed to prepare context"));
	Trace.AddKeyValue(TEXT("PrepareResult"), FString::FromInt(PrepareResult));

	Context->SetArgDWord(0, 10);
	Context->SetArgDWord(1, 32);
	Trace.AddStep(TEXT("SetArguments"), TEXT("Set function arguments: a=10, b=32"));

	int ExecuteResult = Context->Execute();
	Trace.AddStep(TEXT("Execute"), ExecuteResult == asEXECUTION_FINISHED ? TEXT("Execution completed successfully") : TEXT("Execution did not complete normally"));
	Trace.AddKeyValue(TEXT("ExecuteResult"), FString::FromInt(ExecuteResult));
	Trace.AddKeyValue(TEXT("ExecuteResultMeaning"), ExecuteResult == asEXECUTION_FINISHED ? TEXT("asEXECUTION_FINISHED") : FString::Printf(TEXT("Other: %d"), ExecuteResult));

	if (ExecuteResult == asEXECUTION_FINISHED)
	{
		asDWORD ReturnValue = Context->GetReturnDWord();
		Trace.AddStep(TEXT("ReadReturnValue"), TEXT("Read the return value from the executed function"));
		Trace.AddKeyValue(TEXT("ReturnValue"), FString::FromInt(ReturnValue));
		Trace.AddKeyValue(TEXT("ExpectedValue"), TEXT("42"));
	}

	Context->Release();
	Trace.AddStep(TEXT("ReleaseContext"), TEXT("Released the execution context"));

	Trace.AddStep(TEXT("ExecutionLifecycleObservation"), TEXT("Execution lifecycle: CreateContext -> Prepare -> SetArguments -> Execute -> GetReturnValue -> Release; errors return non-zero result codes; exceptions change execution state"));

	const bool bEngineCreated = TestNotNull(TEXT("Native engine should be created"), Engine);
	const bool bModuleCreated = TestNotNull(TEXT("Module should be created"), Module);
	const bool bFunctionFound = TestNotNull(TEXT("Function should be found"), Function);
	const bool bContextCreated = TestNotNull(TEXT("Context should be created"), Context);
	const bool bPhaseSequenceOk = AssertLearningTracePhaseSequence(*this, Trace.GetEvents(), {
		EAngelscriptLearningTracePhase::Execution,
	});
	const bool bContainsExecuteKeyword = AssertLearningTraceContainsKeyword(*this, Trace.GetEvents(), TEXT("ExecuteResult"));
	const bool bContainsReturnKeyword = AssertLearningTraceContainsKeyword(*this, Trace.GetEvents(), TEXT("ReturnValue"));
	const bool bMinimumEventsOk = AssertLearningTraceMinimumEventCount(*this, Trace.GetEvents(), 8);

	Trace.FlushToAutomation(*this);
	Trace.FlushToLog();
	return bEngineCreated
		&& bModuleCreated
		&& bFunctionFound
		&& bContextCreated
		&& bPhaseSequenceOk
		&& bContainsExecuteKeyword
		&& bContainsReturnKeyword
		&& bMinimumEventsOk;
}

#endif
