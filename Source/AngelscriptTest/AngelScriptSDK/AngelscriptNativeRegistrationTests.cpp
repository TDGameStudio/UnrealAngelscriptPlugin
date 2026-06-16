#include "AngelscriptNativeTestSupport.h"

#include "CQTest.h"
#include "Misc/ScopeExit.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptNativeTestSupport;

namespace
{
	int32 GNativeGlobalValue = 21;

	int32 NativeDoubleValue(int32 Value)
	{
		return Value * 2;
	}

	struct FNativeCounter
	{
		int32 Value;
	};

	void ConstructNativeCounter(FNativeCounter* Address)
	{
		new(Address) FNativeCounter{0};
	}

	bool RegisterNativeCounter(FAutomationTestBase& Test, asIScriptEngine* ScriptEngine)
	{
		if (!Test.TestNotNull(TEXT("Native value-type registration should receive a valid script engine"), ScriptEngine))
		{
			return false;
		}

		const int TypeResult = ScriptEngine->RegisterObjectType(
			"NativeCounter",
			sizeof(FNativeCounter),
			asOBJ_VALUE | asOBJ_POD | asGetTypeTraits<FNativeCounter>() | asOBJ_APP_CLASS_ALLINTS);
		if (!Test.TestTrue(TEXT("Native value-type registration should register the POD object type"), TypeResult >= 0))
		{
			return false;
		}

		const ASAutoCaller::FunctionCaller ConstructorCaller = ASAutoCaller::MakeFunctionCaller(ConstructNativeCounter);
		const int ConstructResult = ScriptEngine->RegisterObjectBehaviour(
			"NativeCounter",
			asBEHAVE_CONSTRUCT,
			"void f()",
			asFUNCTION(ConstructNativeCounter),
			asCALL_CDECL_OBJLAST,
			*(asFunctionCaller*)&ConstructorCaller);
		if (!Test.TestTrue(TEXT("Native value-type registration should register the default constructor"), ConstructResult >= 0))
		{
			return false;
		}

		const int PropertyResult = ScriptEngine->RegisterObjectProperty(
			"NativeCounter",
			"int Value",
			asOFFSET(FNativeCounter, Value));
		return Test.TestTrue(TEXT("Native value-type registration should expose the POD field as a script property"), PropertyResult >= 0);
	}

	bool ExecuteRegisteredScript(
		FAutomationTestBase& Test,
		asIScriptEngine* ScriptEngine,
		const char* ModuleName,
		const char* Source,
		const char* Declaration,
		FNativeMessageCollector& Messages,
		int32& OutValue)
	{
		asIScriptModule* Module = BuildNativeModule(ScriptEngine, ModuleName, Source);
		if (!Test.TestNotNull(TEXT("Native registration tests should compile the script module"), Module))
		{
			Test.AddInfo(CollectMessages(Messages));
			return false;
		}

		asIScriptFunction* Function = GetNativeFunctionByDecl(Module, Declaration);
		if (!Test.TestNotNull(TEXT("Native registration tests should resolve the script entry point"), Function))
		{
			return false;
		}

		asIScriptContext* Context = ScriptEngine->CreateContext();
		if (!Test.TestNotNull(TEXT("Native registration tests should create a script context"), Context))
		{
			return false;
		}

		const int ExecuteResult = PrepareAndExecute(Context, Function);
		if (!Test.TestEqual(TEXT("Native registration tests should finish script execution successfully"), ExecuteResult, static_cast<int32>(asEXECUTION_FINISHED)))
		{
			if (ExecuteResult == asEXECUTION_EXCEPTION)
			{
				const int ExceptionLine = Context->GetExceptionLineNumber();
				const FString ExceptionString = UTF8_TO_TCHAR(Context->GetExceptionString() != nullptr ? Context->GetExceptionString() : "");
				Test.AddInfo(FString::Printf(TEXT("Native registration exception at line %d: %s"), ExceptionLine, *ExceptionString));
			}
			const FString Diagnostics = CollectMessages(Messages);
			if (!Diagnostics.IsEmpty())
			{
				Test.AddInfo(Diagnostics);
			}
			Context->Release();
			return false;
		}

		OutValue = static_cast<int32>(Context->GetReturnDWord());
		Context->Release();
		return true;
	}
}


TEST_CLASS_WITH_FLAGS(FAngelscriptNativeRegistrationTests,
	"Angelscript.TestModule.AngelScriptSDK.Register",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(GlobalFunction)
	{
		FNativeMessageCollector Messages;
		asIScriptEngine* ScriptEngine = CreateNativeEngine(&Messages);
		if (!TestRunner->TestNotNull(TEXT("Native global-function registration test should create a standalone engine"), ScriptEngine))
		{
			return;
		}

		ON_SCOPE_EXIT
		{
			DestroyNativeEngine(ScriptEngine);
		};

		const ASAutoCaller::FunctionCaller Caller = ASAutoCaller::MakeFunctionCaller(NativeDoubleValue);
		const int RegisterResult = ScriptEngine->RegisterGlobalFunction(
			"int DoubleNative(int Value)",
			asFUNCTION(NativeDoubleValue),
			asCALL_CDECL,
			*(asFunctionCaller*)&Caller);
		if (!TestRunner->TestTrue(TEXT("Native global-function registration test should register the C++ function"), RegisterResult >= 0))
		{
			TestRunner->AddInfo(FString::Printf(TEXT("RegisterGlobalFunction returned %d"), RegisterResult));
			return;
		}

		int32 Result = 0;
		if (!ExecuteRegisteredScript(*TestRunner, ScriptEngine, "NativeRegisterGlobalFunction", "int Entry() { return DoubleNative(21); }", "int Entry()", Messages, Result))
		{
			return;
		}

		TestRunner->TestEqual(TEXT("Native global-function registration test should allow script code to call the registered function"), Result, 42);
	}

	TEST_METHOD(GlobalProperty)
	{
		FNativeMessageCollector Messages;
		asIScriptEngine* ScriptEngine = CreateNativeEngine(&Messages);
		if (!TestRunner->TestNotNull(TEXT("Native global-property registration test should create a standalone engine"), ScriptEngine))
		{
			return;
		}

		ON_SCOPE_EXIT
		{
			DestroyNativeEngine(ScriptEngine);
		};

		const int RegisterResult = ScriptEngine->RegisterGlobalProperty("int NativeGlobalValue", &GNativeGlobalValue);
		if (!TestRunner->TestTrue(TEXT("Native global-property registration test should register the C++ property"), RegisterResult >= 0))
		{
			return;
		}

		int32 Result = 0;
		if (!ExecuteRegisteredScript(*TestRunner, ScriptEngine, "NativeRegisterGlobalProperty", "int Entry() { return NativeGlobalValue * 2; }", "int Entry()", Messages, Result))
		{
			return;
		}

		TestRunner->TestEqual(TEXT("Native global-property registration test should expose the registered property to script code"), Result, 42);
	}

	TEST_METHOD(SimpleValueType)
	{
		FNativeMessageCollector Messages;
		asIScriptEngine* ScriptEngine = CreateNativeEngine(&Messages);
		if (!TestRunner->TestNotNull(TEXT("Native value-type registration test should create a standalone engine"), ScriptEngine))
		{
			return;
		}

		ON_SCOPE_EXIT
		{
			DestroyNativeEngine(ScriptEngine);
		};

		if (!RegisterNativeCounter(*TestRunner, ScriptEngine))
		{
			return;
		}

		int32 Result = 0;
		if (!ExecuteRegisteredScript(*TestRunner, ScriptEngine, "NativeRegisterSimpleValueType", "int Entry() { NativeCounter Counter; Counter.Value = 19; return Counter.Value + 23; }", "int Entry()", Messages, Result))
		{
			return;
		}

		TestRunner->TestEqual(TEXT("Native value-type registration test should allow script code to construct and use the registered POD type"), Result, 42);
	}
};

#endif
