#include "AngelscriptLearningTrace.h"
#include "AngelScriptSDK/AngelscriptNativeTestSupport.h"

#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptNativeTestSupport;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptLearningNativeBytecodeTraceTest,
	"Angelscript.TestModule.Learning.Native.Bytecode",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptLearningNativeBytecodeTraceTest::RunTest(const FString& Parameters)
{
	FAngelscriptLearningTraceSinkConfig SinkConfig;
	SinkConfig.bEmitToAutomation = true;
	SinkConfig.bEmitToLog = true;
	SinkConfig.bEmitToFile = false;

	FAngelscriptLearningTraceSession Trace(TEXT("LearningNativeBytecode"), SinkConfig);
	FNativeMessageCollector Messages;
	asIScriptEngine* ScriptEngine = CreateNativeEngine(&Messages);
	ON_SCOPE_EXIT
	{
		DestroyNativeEngine(ScriptEngine);
	};

	if (!TestNotNull(TEXT("CreateNativeEngine should provide a script engine for bytecode tracing"), ScriptEngine))
	{
		return false;
	}

	Trace.BeginPhase(EAngelscriptLearningTracePhase::Compile);
	const ANSICHAR* Source =
		"int DoubleAfterIncrement(int Start)\n"
		"{\n"
		"  int Local = Start + 1;\n"
		"  return Local * 2;\n"
		"}\n";
	asIScriptModule* Module = nullptr;
	const int32 BuildResult = CompileNativeModule(ScriptEngine, "LearningNativeBytecode", Source, Module);
	Trace.AddStep(TEXT("BuildModule"), BuildResult >= 0 ? TEXT("Compiled the bytecode teaching sample") : TEXT("Failed to compile the bytecode teaching sample"));
	Trace.AddKeyValue(TEXT("BuildResult"), FString::FromInt(BuildResult));
	Trace.AddKeyValue(TEXT("Functions"), Module != nullptr ? CollectFunctionDeclarations(Module) : TEXT("<null module>"));

	if (!TestTrue(TEXT("The bytecode teaching sample should compile"), BuildResult >= 0) || !TestNotNull(TEXT("Compiled module should be valid"), Module))
	{
		Trace.FlushToAutomation(*this);
		Trace.FlushToLog();
		return false;
	}

	asIScriptFunction* Function = GetNativeFunctionByDecl(Module, "int DoubleAfterIncrement(int)");
	if (!TestNotNull(TEXT("The bytecode teaching sample should expose DoubleAfterIncrement(int)"), Function))
	{
		Trace.FlushToAutomation(*this);
		Trace.FlushToLog();
		return false;
	}

	Trace.BeginPhase(EAngelscriptLearningTracePhase::Bytecode);
	Trace.AddStep(TEXT("ReadFunctionMetadata"), TEXT("Read declaration, parameter count, local variable count and executable-line info from the compiled function"));
	Trace.AddKeyValue(TEXT("Declaration"), UTF8_TO_TCHAR(Function->GetDeclaration()));
	Trace.AddKeyValue(TEXT("ParamCount"), FString::FromInt(Function->GetParamCount()));
	Trace.AddKeyValue(TEXT("VarCount"), FString::FromInt(Function->GetVarCount()));
	Trace.AddKeyValue(TEXT("NextLineWithCodeFrom1"), FString::FromInt(Function->FindNextLineWithCode(1)));

	TArray<FString> VariableDeclarations;
	for (asUINT VarIndex = 0; VarIndex < Function->GetVarCount(); ++VarIndex)
	{
		VariableDeclarations.Add(UTF8_TO_TCHAR(Function->GetVarDecl(VarIndex, false)));
	}
	if (VariableDeclarations.Num() > 0)
	{
		Trace.AddCodeBlock(FormatLearningTraceStringList(VariableDeclarations, TEXT("Locals")));
	}

	asUINT BytecodeLength = 0;
	asDWORD* Bytecode = Function->GetByteCode(&BytecodeLength);
	TArray<uint32> BytecodeWords;
	for (asUINT Index = 0; Index < BytecodeLength; ++Index)
	{
		BytecodeWords.Add(Bytecode[Index]);
	}
	Trace.AddStep(TEXT("ReadByteCode"), TEXT("Captured the raw bytecode buffer and summarized its first dwords"));
	Trace.AddKeyValue(TEXT("BytecodeLength"), FString::FromInt(static_cast<int32>(BytecodeLength)));
	Trace.AddCodeBlock(FormatLearningTraceBytecode(BytecodeWords, 6));

	asIScriptContext* Context = ScriptEngine->CreateContext();
	if (!TestNotNull(TEXT("The bytecode teaching sample should create a context"), Context))
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
	const int32 PrepareResult = Context->Prepare(Function);
	Trace.AddStep(TEXT("Prepare"), PrepareResult >= 0 ? TEXT("Prepared the compiled function for execution") : TEXT("Failed to prepare the compiled function"));
	Trace.AddKeyValue(TEXT("PrepareResult"), FString::FromInt(PrepareResult));
	if (!TestEqual(TEXT("Preparing the native bytecode sample should succeed"), PrepareResult, static_cast<int32>(asSUCCESS)))
	{
		Trace.FlushToAutomation(*this);
		Trace.FlushToLog();
		return false;
	}

	Context->SetArgDWord(0, 20);
	const int32 ExecuteResult = Context->Execute();
	const int32 ReturnValue = static_cast<int32>(Context->GetReturnDWord());
	Trace.AddStep(TEXT("Execute"), ExecuteResult >= 0 ? TEXT("Executed the compiled function and captured the return value") : TEXT("Execution failed"));
	Trace.AddKeyValue(TEXT("ExecuteResult"), FString::FromInt(ExecuteResult));
	Trace.AddKeyValue(TEXT("ReturnValue"), FString::FromInt(ReturnValue));

	const bool bParamCountValid = TestEqual(TEXT("The native bytecode sample should expose one parameter"), static_cast<int32>(Function->GetParamCount()), 1);
	const bool bVarCountValid = TestTrue(TEXT("The native bytecode sample should expose at least one local variable"), Function->GetVarCount() > 0);
	const bool bExecutableLineValid = TestTrue(TEXT("The native bytecode sample should report at least one executable line"), Function->FindNextLineWithCode(1) > 0);
	const bool bBytecodePresent = TestTrue(TEXT("The native bytecode sample should expose a non-empty bytecode buffer"), Bytecode != nullptr && BytecodeLength > 0);
	const bool bExecuted = TestEqual(TEXT("The native bytecode sample should return the expected value"), ReturnValue, 42);
	const bool bExecuteResultValid = TestEqual(TEXT("The native bytecode sample should finish execution successfully"), ExecuteResult, static_cast<int32>(asEXECUTION_FINISHED));
	const bool bPhaseSequenceOk = AssertLearningTracePhaseSequence(*this, Trace.GetEvents(), {
		EAngelscriptLearningTracePhase::Compile,
		EAngelscriptLearningTracePhase::Bytecode,
		EAngelscriptLearningTracePhase::Execution,
	});
	const bool bContainsKeyword = AssertLearningTraceContainsKeyword(*this, Trace.GetEvents(), TEXT("BytecodeLength"));
	const bool bMinimumEventsOk = AssertLearningTraceMinimumEventCount(*this, Trace.GetEvents(), 5);

	Trace.FlushToAutomation(*this);
	Trace.FlushToLog();
	return bParamCountValid && bVarCountValid && bExecutableLineValid && bBytecodePresent && bExecuteResultValid && bExecuted && bPhaseSequenceOk && bContainsKeyword && bMinimumEventsOk;
}

#endif
