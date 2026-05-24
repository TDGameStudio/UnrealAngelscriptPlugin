#include "AngelscriptLearningTrace.h"
#include "AngelScriptSDK/AngelscriptNativeTestSupport.h"

#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptNativeTestSupport;

namespace AngelscriptTest_Learning_Native_AngelscriptLearningNativeBindingTraceTests_Private
{
	struct FLearningNativeBindingCounter
	{
		int32 Value = 0;
	};

	void ConstructLearningNativeBindingCounter(FLearningNativeBindingCounter* Address)
	{
		new(Address) FLearningNativeBindingCounter{0};
	}

	void DestructLearningNativeBindingCounter(FLearningNativeBindingCounter* Address)
	{
		Address->~FLearningNativeBindingCounter();
	}

	int32 LearningNativeBindingDoubleValue(int32 Value)
	{
		return Value * 2;
	}
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptLearningNativeBindingTraceTest,
	"Angelscript.TestModule.Learning.Native.Binding",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptLearningNativeBindingTraceTest::RunTest(const FString& Parameters)
{
	using namespace AngelscriptTest_Learning_Native_AngelscriptLearningNativeBindingTraceTests_Private;
	FAngelscriptLearningTraceSinkConfig SinkConfig;
	SinkConfig.bEmitToAutomation = true;
	SinkConfig.bEmitToLog = true;
	SinkConfig.bEmitToFile = false;

	FAngelscriptLearningTraceSession Trace(TEXT("LearningNativeBinding"), SinkConfig);
	FNativeMessageCollector Messages;
	asIScriptEngine* ScriptEngine = CreateNativeEngine(&Messages);
	ON_SCOPE_EXIT
	{
		DestroyNativeEngine(ScriptEngine);
	};

	if (!TestNotNull(TEXT("CreateNativeEngine should provide a script engine for binding trace"), ScriptEngine))
	{
		Trace.BeginPhase(EAngelscriptLearningTracePhase::Binding);
		Trace.AddStep(TEXT("CreateNativeEngine"), TEXT("Failed to create the native engine before bindings were registered"));
		Trace.FlushToAutomation(*this);
		Trace.FlushToLog();
		return false;
	}

	int32 NativeSeed = 21;
	const ASAutoCaller::FunctionCaller NativeDoubleCaller = ASAutoCaller::MakeFunctionCaller(LearningNativeBindingDoubleValue);
	const ASAutoCaller::FunctionCaller NativeCounterConstructorCaller = ASAutoCaller::MakeFunctionCaller(ConstructLearningNativeBindingCounter);

	Trace.BeginPhase(EAngelscriptLearningTracePhase::Binding);
	const int32 RegisterFunctionResult = ScriptEngine->RegisterGlobalFunction(
		"int DoubleNative(int Value)",
		asFUNCTION(LearningNativeBindingDoubleValue),
		asCALL_CDECL,
		*(asFunctionCaller*)&NativeDoubleCaller);
	Trace.AddStep(TEXT("RegisterGlobalFunction"), RegisterFunctionResult >= 0 ? TEXT("Registered DoubleNative as a global function") : TEXT("Failed to register DoubleNative"));
	Trace.AddKeyValue(TEXT("ReturnCode"), FString::FromInt(RegisterFunctionResult));

	const int32 RegisterPropertyResult = ScriptEngine->RegisterGlobalProperty("const int NativeSeed", (void*)&NativeSeed);
	Trace.AddStep(TEXT("RegisterGlobalProperty"), RegisterPropertyResult >= 0 ? TEXT("Registered NativeSeed as a global property") : TEXT("Failed to register NativeSeed"));
	Trace.AddKeyValue(TEXT("ReturnCode"), FString::FromInt(RegisterPropertyResult));

	const int32 RegisterTypeResult = ScriptEngine->RegisterObjectType(
		"NativeCounter",
		sizeof(FLearningNativeBindingCounter),
		asOBJ_VALUE | asOBJ_POD | asGetTypeTraits<FLearningNativeBindingCounter>() | asOBJ_APP_CLASS_ALLINTS);
	Trace.AddStep(TEXT("RegisterObjectType"), RegisterTypeResult >= 0 ? TEXT("Registered NativeCounter as a value type") : TEXT("Failed to register NativeCounter"));
	Trace.AddKeyValue(TEXT("ReturnCode"), FString::FromInt(RegisterTypeResult));

	const int32 RegisterConstructResult = ScriptEngine->RegisterObjectBehaviour(
		"NativeCounter",
		asBEHAVE_CONSTRUCT,
		"void f()",
		asFUNCTION(ConstructLearningNativeBindingCounter),
		asCALL_CDECL_OBJLAST,
		*(asFunctionCaller*)&NativeCounterConstructorCaller);
	Trace.AddStep(TEXT("RegisterObjectBehaviour.Construct"), RegisterConstructResult >= 0 ? TEXT("Registered the default constructor for NativeCounter") : TEXT("Failed to register the constructor"));
	Trace.AddKeyValue(TEXT("ReturnCode"), FString::FromInt(RegisterConstructResult));

	const int32 RegisterDestructResult = ScriptEngine->RegisterObjectBehaviour("NativeCounter", asBEHAVE_DESTRUCT, "void f()", asFUNCTION(DestructLearningNativeBindingCounter), asCALL_CDECL_OBJLAST);
	Trace.AddStep(TEXT("RegisterObjectBehaviour.Destruct"), RegisterDestructResult >= 0 ? TEXT("Registered the destructor for NativeCounter") : TEXT("Failed to register the destructor"));
	Trace.AddKeyValue(TEXT("ReturnCode"), FString::FromInt(RegisterDestructResult));

	const int32 RegisterValuePropertyResult = ScriptEngine->RegisterObjectProperty("NativeCounter", "int Value", asOFFSET(FLearningNativeBindingCounter, Value));
	Trace.AddStep(TEXT("RegisterObjectProperty"), RegisterValuePropertyResult >= 0 ? TEXT("Registered Value as an object property") : TEXT("Failed to register the Value property"));
	Trace.AddKeyValue(TEXT("ReturnCode"), FString::FromInt(RegisterValuePropertyResult));

	asIScriptModule* Module = nullptr;
	Trace.BeginPhase(EAngelscriptLearningTracePhase::Compile);
	const ANSICHAR* Source =
		"int FunctionAndPropertyEntry()\n"
		"{\n"
		"  return DoubleNative(NativeSeed);\n"
		"}\n"
		"int ValueTypeEntry()\n"
		"{\n"
		"  NativeCounter Counter;\n"
		"  Counter.Value = 19;\n"
		"  return Counter.Value + 23;\n"
		"}";
	const int32 BuildResult = CompileNativeModule(ScriptEngine, "LearningNativeBinding", Source, Module);
	Trace.AddStep(TEXT("BuildModule"), BuildResult >= 0 ? TEXT("Compiled a module that consumes the native bindings") : TEXT("Failed to compile the binding example module"));
	Trace.AddKeyValue(TEXT("BuildResult"), FString::FromInt(BuildResult));
	Trace.AddKeyValue(TEXT("Functions"), Module != nullptr ? CollectFunctionDeclarations(Module) : TEXT("<null module>"));

	int32 FunctionAndPropertyResult = INDEX_NONE;
	if (BuildResult >= 0 && Module != nullptr)
	{
		if (asIScriptFunction* FunctionAndPropertyEntry = GetNativeFunctionByDecl(Module, "int FunctionAndPropertyEntry()"))
		{
			asIScriptContext* Context = ScriptEngine->CreateContext();
			if (Context != nullptr)
			{
				ON_SCOPE_EXIT
				{
					Context->Release();
				};

				const int32 ExecuteResult = PrepareAndExecute(Context, FunctionAndPropertyEntry);
				Trace.BeginPhase(EAngelscriptLearningTracePhase::Execution);
				Trace.AddStep(TEXT("PrepareAndExecute.FunctionAndProperty"), ExecuteResult >= 0 ? TEXT("Executed FunctionAndPropertyEntry() through global function/property bindings") : TEXT("Failed to execute FunctionAndPropertyEntry()"));
				Trace.AddKeyValue(TEXT("ReturnCode"), FString::FromInt(ExecuteResult));
				if (ExecuteResult >= 0)
				{
					FunctionAndPropertyResult = Context->GetReturnDWord();
					Trace.AddKeyValue(TEXT("ReturnValue"), FString::FromInt(FunctionAndPropertyResult));
				}
			}
		}

		const bool bHasValueTypeEntry = GetNativeFunctionByDecl(Module, "int ValueTypeEntry()") != nullptr;
		Trace.AddStep(TEXT("ValueTypeVisibility"), bHasValueTypeEntry ? TEXT("Compiled module exposes ValueTypeEntry that consumes the registered value type") : TEXT("Failed to resolve ValueTypeEntry after compilation"));
		Trace.AddKeyValue(TEXT("ValueTypeEntryFound"), bHasValueTypeEntry ? TEXT("true") : TEXT("false"));
	}

	const bool bBindingResultsValid =
		TestTrue(TEXT("Global function registration should succeed"), RegisterFunctionResult >= 0) &&
		TestTrue(TEXT("Global property registration should succeed"), RegisterPropertyResult >= 0) &&
		TestTrue(TEXT("Object type registration should succeed"), RegisterTypeResult >= 0) &&
		TestTrue(TEXT("Object behaviour registration should succeed"), RegisterConstructResult >= 0 && RegisterDestructResult >= 0) &&
		TestTrue(TEXT("Object property registration should succeed"), RegisterValuePropertyResult >= 0);

	const bool bCompiled = TestTrue(TEXT("The native binding example script should compile"), BuildResult >= 0);
	const bool bFunctionAndPropertyExecuted = TestEqual(TEXT("The script should compute the expected result through global function/property bindings"), FunctionAndPropertyResult, 42);
	const bool bValueTypePathVisible = AssertLearningTraceContainsKeyword(*this, Trace.GetEvents(), TEXT("ValueTypeVisibility"));
	const bool bPhaseSequenceOk = AssertLearningTracePhaseSequence(*this, Trace.GetEvents(), {
		EAngelscriptLearningTracePhase::Binding,
		EAngelscriptLearningTracePhase::Compile,
		EAngelscriptLearningTracePhase::Execution,
	});
	const bool bContainsKeyword = AssertLearningTraceContainsKeyword(*this, Trace.GetEvents(), TEXT("RegisterGlobalFunction"));
	const bool bMinimumEventsOk = AssertLearningTraceMinimumEventCount(*this, Trace.GetEvents(), 8);

	Trace.FlushToAutomation(*this);
	Trace.FlushToLog();
	return bBindingResultsValid && bCompiled && bFunctionAndPropertyExecuted && bValueTypePathVisible && bPhaseSequenceOk && bContainsKeyword && bMinimumEventsOk;
}

#endif
