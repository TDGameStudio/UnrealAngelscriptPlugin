#include "AngelscriptLearningTrace.h"
#include "AngelScriptSDK/AngelscriptNativeTestSupport.h"

#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptNativeTestSupport;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptLearningDebuggerContextTraceTest,
	"Angelscript.TestModule.Learning.Native.DebuggerContext",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptLearningDebuggerContextTraceTest::RunTest(const FString& Parameters)
{
	FAngelscriptLearningTraceSinkConfig SinkConfig;
	SinkConfig.bEmitToAutomation = true;
	SinkConfig.bEmitToLog = true;
	SinkConfig.bEmitToFile = false;

	FAngelscriptLearningTraceSession Trace(TEXT("LearningDebuggerContext"), SinkConfig);
	FNativeMessageCollector Messages;
	asIScriptEngine* ScriptEngine = CreateNativeEngine(&Messages);
	ON_SCOPE_EXIT
	{
		DestroyNativeEngine(ScriptEngine);
	};

	if (!TestNotNull(TEXT("CreateNativeEngine should provide a script engine for debugger-context tracing"), ScriptEngine))
	{
		return false;
	}

	Trace.BeginPhase(EAngelscriptLearningTracePhase::Compile);
	const ANSICHAR* Source =
		"void FailInner(int Value)\n"
		"{\n"
		"  int Inner = Value * 2;\n"
		"  int Zero = 0;\n"
		"  int Crash = Inner / Zero;\n"
		"}\n"
		"void TriggerFailure(int Seed)\n"
		"{\n"
		"  int Local = Seed + 1;\n"
		"  FailInner(Local);\n"
		"}\n"
		"int Entry()\n"
		"{\n"
		"  TriggerFailure(20);\n"
		"  return 0;\n"
		"}";

	asIScriptModule* Module = nullptr;
	const int32 BuildResult = CompileNativeModule(ScriptEngine, "LearningDebuggerContext", Source, Module);
	Trace.AddStep(TEXT("BuildModule"), BuildResult >= 0 ? TEXT("Compiled an exception-driven debugger-context sample") : TEXT("Failed to compile the debugger-context sample"));
	Trace.AddKeyValue(TEXT("BuildResult"), FString::FromInt(BuildResult));

	if (!TestTrue(TEXT("The debugger-context sample should compile"), BuildResult >= 0) || !TestNotNull(TEXT("Compiled module should be valid"), Module))
	{
		Trace.FlushToAutomation(*this);
		Trace.FlushToLog();
		return false;
	}

	asIScriptFunction* EntryFunction = GetNativeFunctionByDecl(Module, "int Entry()");
	if (!TestNotNull(TEXT("The debugger-context sample should expose Entry()"), EntryFunction))
	{
		Trace.FlushToAutomation(*this);
		Trace.FlushToLog();
		return false;
	}

	asIScriptContext* Context = ScriptEngine->CreateContext();
	if (!TestNotNull(TEXT("The debugger-context sample should create a context"), Context))
	{
		Trace.FlushToAutomation(*this);
		Trace.FlushToLog();
		return false;
	}

	ON_SCOPE_EXIT
	{
		Context->Release();
	};

	Trace.BeginPhase(EAngelscriptLearningTracePhase::Execution);
	const int32 PrepareResult = Context->Prepare(EntryFunction);
	Trace.AddStep(TEXT("Prepare"), PrepareResult >= 0 ? TEXT("Prepared Entry() for execution") : TEXT("Failed to prepare Entry()"));
	Trace.AddKeyValue(TEXT("PrepareResult"), FString::FromInt(PrepareResult));
	if (!TestEqual(TEXT("Preparing the debugger-context sample should succeed"), PrepareResult, static_cast<int32>(asSUCCESS)))
	{
		Trace.FlushToAutomation(*this);
		Trace.FlushToLog();
		return false;
	}

	const int32 ExecuteResult = Context->Execute();
	Trace.AddStep(TEXT("Execute"), ExecuteResult == asEXECUTION_EXCEPTION ? TEXT("Execution raised an exception and preserved debugger context") : TEXT("Execution finished without the expected exception"));
	Trace.AddKeyValue(TEXT("ExecuteResult"), FString::FromInt(ExecuteResult));

	const FString ExceptionString = UTF8_TO_TCHAR(Context->GetExceptionString() != nullptr ? Context->GetExceptionString() : "");
	const int32 ExceptionLine = Context->GetExceptionLineNumber();
	Trace.AddStep(TEXT("InspectException"), TEXT("Captured exception text, line number, callstack depth and local-variable summaries"));
	Trace.AddKeyValue(TEXT("ExceptionString"), ExceptionString);
	Trace.AddKeyValue(TEXT("ExceptionLine"), FString::FromInt(ExceptionLine));
	Trace.AddKeyValue(TEXT("CallstackSize"), FString::FromInt(static_cast<int32>(Context->GetCallstackSize())));

	TArray<FString> CallstackLines;
	for (asUINT StackLevel = 0; StackLevel < Context->GetCallstackSize(); ++StackLevel)
	{
		asIScriptFunction* StackFunction = Context->GetFunction(StackLevel);
		const int32 LineNumber = Context->GetLineNumber(StackLevel);
		CallstackLines.Add(FString::Printf(TEXT("#%u %s @ line %d"), StackLevel, StackFunction != nullptr ? UTF8_TO_TCHAR(StackFunction->GetDeclaration()) : TEXT("<null>"), LineNumber));
	}
	if (CallstackLines.Num() > 0)
	{
		Trace.AddCodeBlock(FormatLearningTraceStringList(CallstackLines, TEXT("Callstack")));
	}

	TArray<FString> LocalLines;
	for (asUINT StackLevel = 0; StackLevel < Context->GetCallstackSize(); ++StackLevel)
	{
		const int32 VarCount = Context->GetVarCount(StackLevel);
		for (int32 VarIndex = 0; VarIndex < VarCount; ++VarIndex)
		{
			const FString VarDecl = UTF8_TO_TCHAR(Context->GetVarDeclaration(VarIndex, StackLevel, false));
			LocalLines.Add(FString::Printf(TEXT("Frame %u: %s"), StackLevel, *VarDecl));
		}
	}
	if (LocalLines.Num() > 0)
	{
		Trace.AddCodeBlock(FormatLearningTraceStringList(LocalLines, TEXT("Locals")));
	}

	const bool bExceptionRaised = TestEqual(TEXT("The debugger-context sample should stop with an exception"), ExecuteResult, static_cast<int32>(asEXECUTION_EXCEPTION));
	const bool bExceptionInfoPresent = TestTrue(TEXT("The debugger-context sample should expose a non-empty exception string"), !ExceptionString.IsEmpty());
	const bool bCallstackPresent = TestTrue(TEXT("The debugger-context sample should expose at least two stack frames"), Context->GetCallstackSize() >= 2);
	const bool bLocalsPresent = TestTrue(TEXT("The debugger-context sample should expose at least one local variable in the exception context"), LocalLines.Num() > 0);
	const bool bPhaseSequenceOk = AssertLearningTracePhaseSequence(*this, Trace.GetEvents(), {
		EAngelscriptLearningTracePhase::Compile,
		EAngelscriptLearningTracePhase::Execution,
	});
	const bool bContainsKeyword = AssertLearningTraceContainsKeyword(*this, Trace.GetEvents(), TEXT("ExceptionString"));
	const bool bMinimumEventsOk = AssertLearningTraceMinimumEventCount(*this, Trace.GetEvents(), 4);

	Trace.FlushToAutomation(*this);
	Trace.FlushToLog();
	return bExceptionRaised && bExceptionInfoPresent && bCallstackPresent && bLocalsPresent && bPhaseSequenceOk && bContainsKeyword && bMinimumEventsOk;
}

#endif
