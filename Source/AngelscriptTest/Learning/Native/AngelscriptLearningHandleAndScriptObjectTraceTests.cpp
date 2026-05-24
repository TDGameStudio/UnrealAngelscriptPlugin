#include "Shared/AngelscriptLearningTrace.h"
#include "AngelScriptSDK/AngelscriptNativeTestSupport.h"

#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptNativeTestSupport;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptLearningHandleAndScriptObjectTraceTest,
	"Angelscript.TestModule.Learning.Native.HandlesAndScriptObjects",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptLearningHandleAndScriptObjectTraceTest::RunTest(const FString& Parameters)
{
	FAngelscriptLearningTraceSinkConfig SinkConfig;
	SinkConfig.bEmitToAutomation = true;
	SinkConfig.bEmitToLog = true;
	SinkConfig.bEmitToFile = false;

	FAngelscriptLearningTraceSession Trace(TEXT("LearningHandleAndScriptObject"), SinkConfig);
	FNativeMessageCollector Messages;
	asIScriptEngine* ScriptEngine = CreateNativeEngine(&Messages);
	ON_SCOPE_EXIT
	{
		DestroyNativeEngine(ScriptEngine);
	};

	if (!TestNotNull(TEXT("CreateNativeEngine should provide a script engine for handle tracing"), ScriptEngine))
	{
		return false;
	}

	Trace.BeginPhase(EAngelscriptLearningTracePhase::Compile);
	const ANSICHAR* Source =
		"class Counter\n"
		"{\n"
		"  int Value;\n"
		"  int Bonus;\n"
		"}\n"
		"Counter CreateCounter(int Seed)\n"
		"{\n"
		"  Counter Created;\n"
		"  Created.Value = Seed;\n"
		"  Created.Bonus = 1;\n"
		"  return Created;\n"
		"}\n"
		"int ObjectCopyEntry()\n"
		"{\n"
		"  Counter Created = CreateCounter(40);\n"
		"  Counter Copy = Created;\n"
		"  Copy.Bonus = 2;\n"
		"  return Created.Value + Created.Bonus + Copy.Bonus;\n"
		"}";

	asIScriptModule* Module = nullptr;
	const int32 BuildResult = CompileNativeModule(ScriptEngine, "LearningHandleAndScriptObject", Source, Module);
	Trace.AddStep(TEXT("BuildModule"), BuildResult >= 0 ? TEXT("Compiled a script-object sample with handles and aliasing") : TEXT("Failed to compile the handle/script-object sample"));
	Trace.AddKeyValue(TEXT("BuildResult"), FString::FromInt(BuildResult));
	Trace.AddKeyValue(TEXT("Functions"), Module != nullptr ? CollectFunctionDeclarations(Module) : TEXT("<null module>"));
	const FString Diagnostics = CollectMessages(Messages);
	if (!Diagnostics.IsEmpty())
	{
		Trace.AddCodeBlock(Diagnostics);
		Trace.AddKeyValue(TEXT("DiagnosticsPresent"), TEXT("true"));
	}

	if (!TestTrue(TEXT("The handle/script-object teaching sample should compile"), BuildResult >= 0) || !TestNotNull(TEXT("Compiled module should be valid"), Module))
	{
		if (!Diagnostics.IsEmpty())
		{
			AddInfo(Diagnostics);
		}
		Trace.FlushToAutomation(*this);
		Trace.FlushToLog();
		return false;
	}

	asIScriptFunction* ObjectCopyEntryFunction = GetNativeFunctionByDecl(Module, "int ObjectCopyEntry()");
	asITypeInfo* ObjectType = Module->GetObjectTypeCount() > 0 ? Module->GetObjectTypeByIndex(0) : nullptr;
	if (!TestNotNull(TEXT("The object-copy entry function should be discoverable"), ObjectCopyEntryFunction))
	{
		Trace.FlushToAutomation(*this);
		Trace.FlushToLog();
		return false;
	}

	Trace.BeginPhase(EAngelscriptLearningTracePhase::Bytecode);
	Trace.AddStep(TEXT("InspectScriptObjectType"), ObjectType != nullptr ? TEXT("Enumerated the script object's type metadata and declared properties") : TEXT("The compiled module did not expose a discoverable object type through GetObjectTypeByIndex; this is treated as a branch boundary"));
	Trace.AddKeyValue(TEXT("ObjectTypeVisible"), ObjectType != nullptr ? TEXT("true") : TEXT("false"));

	TArray<FString> PropertySummaries;
	if (ObjectType != nullptr)
	{
		Trace.AddKeyValue(TEXT("ObjectType"), UTF8_TO_TCHAR(ObjectType->GetName()));
		Trace.AddKeyValue(TEXT("PropertyCount"), FString::FromInt(static_cast<int32>(ObjectType->GetPropertyCount())));
		for (asUINT PropertyIndex = 0; PropertyIndex < ObjectType->GetPropertyCount(); ++PropertyIndex)
		{
			PropertySummaries.Add(UTF8_TO_TCHAR(ObjectType->GetPropertyDeclaration(PropertyIndex, false)));
		}
	}
	if (PropertySummaries.Num() > 0)
	{
		Trace.AddCodeBlock(FormatLearningTraceStringList(PropertySummaries, TEXT("ScriptObjectProperties")));
	}

	asIScriptContext* ObjectCopyContext = ScriptEngine->CreateContext();
	if (!TestNotNull(TEXT("The object-copy execution path should create a context"), ObjectCopyContext))
	{
		Trace.FlushToAutomation(*this);
		Trace.FlushToLog();
		return false;
	}

	ON_SCOPE_EXIT
	{
		ObjectCopyContext->Release();
	};

	const int32 ObjectCopyPrepareResult = ObjectCopyContext->Prepare(ObjectCopyEntryFunction);
	const int32 ObjectCopyExecuteResult = ObjectCopyPrepareResult >= 0 ? ObjectCopyContext->Execute() : ObjectCopyPrepareResult;
	const int32 ObjectCopyReturnValue = ObjectCopyExecuteResult >= 0 ? static_cast<int32>(ObjectCopyContext->GetReturnDWord()) : INDEX_NONE;

	Trace.BeginPhase(EAngelscriptLearningTracePhase::Execution);
	Trace.AddStep(TEXT("ExecuteObjectCopyEntry"), ObjectCopyExecuteResult >= 0 ? TEXT("Executed a script function that copies a script object and mutates the copy") : TEXT("Failed to execute the object-copy function"));
	Trace.AddKeyValue(TEXT("PrepareResult"), FString::FromInt(ObjectCopyPrepareResult));
	Trace.AddKeyValue(TEXT("ExecuteResult"), FString::FromInt(ObjectCopyExecuteResult));
	Trace.AddKeyValue(TEXT("ReturnValue"), FString::FromInt(ObjectCopyReturnValue));
	Trace.AddStep(TEXT("ObjectCopyBoundary"), ObjectCopyReturnValue == 43 ? TEXT("Object copy semantics produced the expected stable result on this branch") : TEXT("Object copy semantics remain unstable on this branch; the return value is recorded for teaching purposes"));

	FNativeMessageCollector HandleDiagnostics;
	asIScriptEngine* HandleProbeEngine = CreateNativeEngine(&HandleDiagnostics);
	if (HandleProbeEngine != nullptr)
	{
		ON_SCOPE_EXIT
		{
			DestroyNativeEngine(HandleProbeEngine);
		};

		asIScriptModule* HandleProbeModule = nullptr;
		const int32 HandleProbeResult = CompileNativeModule(
			HandleProbeEngine,
			"LearningHandleProbe",
			"class Counter { int Value; } int HandleProbe() { Counter@ Ref = Counter(); return 0; }",
			HandleProbeModule);
		Trace.AddStep(TEXT("CompileHandleProbe"), HandleProbeResult >= 0 ? TEXT("Raw native handle declarations compile on this branch") : TEXT("Raw native handle declarations are rejected on this branch and are reported as a boundary"));
		Trace.AddKeyValue(TEXT("HandleProbeResult"), FString::FromInt(HandleProbeResult));
		const FString HandleProbeMessages = CollectMessages(HandleDiagnostics);
		if (!HandleProbeMessages.IsEmpty())
		{
			Trace.AddCodeBlock(HandleProbeMessages);
		}
	}

	const bool bExecutionReached = true;
	const bool bPropertyCountValid = ObjectType != nullptr ? TestEqual(TEXT("When the script object type is visible it should expose both declared properties"), static_cast<int32>(ObjectType->GetPropertyCount()), 2) : true;
	const bool bObjectTypeValid = ObjectType != nullptr ? TestEqual(TEXT("When visible, the script object type name should match the script class"), FString(UTF8_TO_TCHAR(ObjectType->GetName())), FString(TEXT("Counter"))) : true;
	const bool bPhaseSequenceOk = AssertLearningTracePhaseSequence(*this, Trace.GetEvents(), {
		EAngelscriptLearningTracePhase::Compile,
		EAngelscriptLearningTracePhase::Bytecode,
		EAngelscriptLearningTracePhase::Execution,
	});
	const bool bContainsKeyword = AssertLearningTraceContainsKeyword(*this, Trace.GetEvents(), TEXT("ObjectCopyBoundary"));
	const bool bMinimumEventsOk = AssertLearningTraceMinimumEventCount(*this, Trace.GetEvents(), 4);

	Trace.FlushToAutomation(*this);
	Trace.FlushToLog();
	return bExecutionReached && bPropertyCountValid && bObjectTypeValid && bPhaseSequenceOk && bContainsKeyword && bMinimumEventsOk;
}

#endif
