#include "Shared/AngelscriptLearningTrace.h"
#include "AngelScriptSDK/AngelscriptNativeTestSupport.h"

#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptNativeTestSupport;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptLearningNativeBootstrapTest,
	"Angelscript.TestModule.Learning.Native.Bootstrap",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptLearningNativeBootstrapTest::RunTest(const FString& Parameters)
{
	FAngelscriptLearningTraceSinkConfig SinkConfig;
	SinkConfig.bEmitToAutomation = true;
	SinkConfig.bEmitToLog = true;
	SinkConfig.bEmitToFile = false;

	FAngelscriptLearningTraceSession Trace(TEXT("LearningNativeBootstrap"), SinkConfig);
	FNativeMessageCollector Messages;
	asIScriptEngine* ScriptEngine = nullptr;
	asIScriptModule* Module = nullptr;

	ON_SCOPE_EXIT
	{
		if (ScriptEngine != nullptr)
		{
			DestroyNativeEngine(ScriptEngine);
		}
	};

	Trace.BeginPhase(EAngelscriptLearningTracePhase::EngineBootstrap);
	ScriptEngine = CreateNativeEngine(&Messages);
	Trace.AddStep(TEXT("asCreateScriptEngine"), ScriptEngine != nullptr ? TEXT("Created a native AngelScript engine instance") : TEXT("Failed to create a native AngelScript engine instance"));
	Trace.AddKeyValue(TEXT("OptimizeBytecode"), ScriptEngine != nullptr ? FString::FromInt(ScriptEngine->GetEngineProperty(asEP_OPTIMIZE_BYTECODE)) : TEXT("<null>"));
	Trace.AddKeyValue(TEXT("AllowDoubleType"), ScriptEngine != nullptr ? FString::FromInt(ScriptEngine->GetEngineProperty(asEP_ALLOW_DOUBLE_TYPE)) : TEXT("<null>"));

	if (!TestNotNull(TEXT("CreateNativeEngine should return a valid script engine"), ScriptEngine))
	{
		Trace.FlushToAutomation(*this);
		Trace.FlushToLog();
		return false;
	}

	const ANSICHAR* Source =
		"int DoubleValue(int Value) { return Value * 2; }\n"
		"int Entry() { return DoubleValue(21); }";

	Trace.BeginPhase(EAngelscriptLearningTracePhase::Compile);
	const int32 BuildResult = CompileNativeModule(ScriptEngine, "LearningNativeBootstrap", Source, Module);
	Trace.AddStep(TEXT("BuildModule"), BuildResult >= 0 ? TEXT("Compiled the in-memory module successfully") : TEXT("Module compilation failed"));
	Trace.AddKeyValue(TEXT("BuildResult"), FString::FromInt(BuildResult));
	Trace.AddKeyValue(TEXT("Functions"), Module != nullptr ? CollectFunctionDeclarations(Module) : TEXT("<null module>"));

	const FString Diagnostics = CollectMessages(Messages);
	if (!Diagnostics.IsEmpty())
	{
		Trace.AddCodeBlock(Diagnostics);
		Trace.AddKeyValue(TEXT("DiagnosticsPresent"), TEXT("true"));
		Trace.AddKeyValue(TEXT("DiagnosticCount"), FString::FromInt(Messages.Entries.Num()));
		Trace.AddKeyValue(TEXT("FirstDiagnosticType"), ToMessageTypeString(Messages.Entries[0].Type));
	}
	else
	{
		Trace.AddKeyValue(TEXT("DiagnosticsPresent"), TEXT("false"));
	}

	const TArray<EAngelscriptLearningTracePhase> ExpectedPhases = {
		EAngelscriptLearningTracePhase::EngineBootstrap,
		EAngelscriptLearningTracePhase::Compile,
	};

	const bool bPhaseSequenceOk = AssertLearningTracePhaseSequence(*this, Trace.GetEvents(), ExpectedPhases);
	const bool bMinimumEventsOk = AssertLearningTraceMinimumEventCount(*this, Trace.GetEvents(), 2);
	const bool bContainsBuildToken = AssertLearningTraceContainsKeyword(*this, Trace.GetEvents(), TEXT("BuildModule"));
	const bool bCompiled = TestTrue(TEXT("CompileNativeModule should compile the tutorial bootstrap module"), BuildResult >= 0);
	const bool bHasModule = TestNotNull(TEXT("CompileNativeModule should output a valid module"), Module);

	Trace.FlushToAutomation(*this);
	Trace.FlushToLog();

	return bPhaseSequenceOk && bMinimumEventsOk && bContainsBuildToken && bCompiled && bHasModule;
}

#endif
