#include "AngelscriptNativeTestSupport.h"

#include "CQTest.h"
#include "Misc/ScopeExit.h"

#if WITH_ANGELSCRIPT_UNITTESTS




TEST_CLASS_WITH_FLAGS(FAngelscriptNativeRegistrationTests,
	"Angelscript.TestModule.AngelScriptSDK.Register",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	inline static int32 GNativeGlobalValue = 21;

	static int32 NativeDoubleValue(int32 Value)
	{
		return Value * 2;
	}

	struct FNativeCounter
	{
		int32 Value;
	};

	static void ConstructNativeCounter(FNativeCounter* Address)
	{
		new(Address) FNativeCounter{0};
	}

	static bool RegisterNativeCounter(asIScriptEngine* ScriptEngine)
	{
		if (ScriptEngine == nullptr)
		{
			return false;
		}

		const int TypeResult = ScriptEngine->RegisterObjectType(
			"NativeCounter",
			sizeof(FNativeCounter),
			asOBJ_VALUE | asOBJ_POD | asGetTypeTraits<FNativeCounter>() | asOBJ_APP_CLASS_ALLINTS);
		if (TypeResult < 0)
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
		if (ConstructResult < 0)
		{
			return false;
		}

		const int PropertyResult = ScriptEngine->RegisterObjectProperty(
			"NativeCounter",
			"int Value",
			asOFFSET(FNativeCounter, Value));
		return PropertyResult >= 0;
	}

	static bool ExecuteRegisteredScript(
		asIScriptEngine* ScriptEngine,
		const char* ModuleName,
		const char* Source,
		const char* Declaration,
		AngelscriptNativeTestSupport::FNativeTestEngine& NativeEngine,
		int32& OutValue,
		FString& OutDiagnostics)
	{
		using namespace AngelscriptNativeTestSupport;

		asIScriptModule* Module = BuildNativeModule(ScriptEngine, ModuleName, Source);
		if (Module == nullptr)
		{
			OutDiagnostics = NativeEngine.GetMessagesText();
			return false;
		}

		asIScriptFunction* Function = GetNativeFunctionByDecl(Module, Declaration);
		if (Function == nullptr)
		{
			OutDiagnostics = TEXT("Native registration tests should resolve the script entry point");
			return false;
		}

		asIScriptContext* Context = ScriptEngine->CreateContext();
		if (Context == nullptr)
		{
			OutDiagnostics = TEXT("Native registration tests should create a script context");
			return false;
		}
		ON_SCOPE_EXIT
		{
			Context->Release();
		};

		const int ExecuteResult = PrepareAndExecute(Context, Function);
		if (ExecuteResult != static_cast<int32>(asEXECUTION_FINISHED))
		{
			if (ExecuteResult == asEXECUTION_EXCEPTION)
			{
				const int ExceptionLine = Context->GetExceptionLineNumber();
				const FString ExceptionString = UTF8_TO_TCHAR(Context->GetExceptionString() != nullptr ? Context->GetExceptionString() : "");
				OutDiagnostics += FString::Printf(TEXT("Native registration exception at line %d: %s"), ExceptionLine, *ExceptionString);
			}
			const FString Diagnostics = NativeEngine.GetMessagesText();
			if (!Diagnostics.IsEmpty())
			{
				if (!OutDiagnostics.IsEmpty())
				{
					OutDiagnostics += LINE_TERMINATOR;
				}
				OutDiagnostics += Diagnostics;
			}
			return false;
		}

		OutValue = static_cast<int32>(Context->GetReturnDWord());
		return true;
	}
public:
	inline static AngelscriptNativeTestSupport::FNativeTestEngine Engine;
	inline static bool bGlobalFunctionRegistered = false;
	inline static bool bGlobalPropertyRegistered = false;
	inline static bool bNativeCounterRegistered = false;

	BEFORE_ALL()
	{
		Engine.Create(*TestRunner);
		asIScriptEngine* const ScriptEngine = Engine.Get();
		if (ScriptEngine == nullptr)
		{
			return;
		}

		const ASAutoCaller::FunctionCaller Caller = ASAutoCaller::MakeFunctionCaller(NativeDoubleValue);
		const int RegisterResult = ScriptEngine->RegisterGlobalFunction(
			"int DoubleNative(int Value)",
			asFUNCTION(NativeDoubleValue),
			asCALL_CDECL,
			*(asFunctionCaller*)&Caller);
		bGlobalFunctionRegistered = RegisterResult >= 0;
		if (!bGlobalFunctionRegistered)
		{
			TestRunner->AddError(TEXT("Native global-function registration test should register the C++ function"));
			TestRunner->AddInfo(FString::Printf(TEXT("RegisterGlobalFunction returned %d"), RegisterResult));
		}

		const int GlobalPropertyResult = ScriptEngine->RegisterGlobalProperty("int NativeGlobalValue", &GNativeGlobalValue);
		bGlobalPropertyRegistered = GlobalPropertyResult >= 0;
		if (!bGlobalPropertyRegistered)
		{
			TestRunner->AddError(TEXT("Native global-property registration test should register the C++ property"));
		}

		bNativeCounterRegistered = RegisterNativeCounter(ScriptEngine);
		if (!bNativeCounterRegistered)
		{
			TestRunner->AddError(TEXT("Native value-type registration should register the POD object type, constructor, and property"));
		}
	}

	AFTER_ALL()
	{
		Engine.Destroy();
		bGlobalFunctionRegistered = false;
		bGlobalPropertyRegistered = false;
		bNativeCounterRegistered = false;
	}

	BEFORE_EACH()
	{
		Engine.ResetMessages();
		GNativeGlobalValue = 21;
	}

	TEST_METHOD(GlobalFunction)
	{
		asIScriptEngine* ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Native global-function registration test should create a standalone engine")));
		if (!bGlobalFunctionRegistered)
		{
			return;
		}

		int32 Result = 0;
		FString Diagnostics;
		AngelscriptNativeTestSupport::FScopedNativeModuleName ModuleScope(Engine, "NativeRegisterGlobalFunction");
		if (!this->Assert.IsTrue(ExecuteRegisteredScript(ScriptEngine, "NativeRegisterGlobalFunction", "int Entry() { return DoubleNative(21); }", "int Entry()", Engine, Result, Diagnostics),
			TEXT("Native registration tests should compile, resolve, and execute the script module")))
		{
			if (!Diagnostics.IsEmpty())
			{
				TestRunner->AddInfo(Diagnostics);
			}
			return;
		}

		ASSERT_THAT(AreEqual(42, Result,
			TEXT("Native global-function registration test should allow script code to call the registered function")));
	}

	TEST_METHOD(GlobalProperty)
	{
		asIScriptEngine* ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Native global-property registration test should create a standalone engine")));
		if (!bGlobalPropertyRegistered)
		{
			return;
		}

		int32 Result = 0;
		FString Diagnostics;
		AngelscriptNativeTestSupport::FScopedNativeModuleName ModuleScope(Engine, "NativeRegisterGlobalProperty");
		if (!this->Assert.IsTrue(ExecuteRegisteredScript(ScriptEngine, "NativeRegisterGlobalProperty", "int Entry() { return NativeGlobalValue * 2; }", "int Entry()", Engine, Result, Diagnostics),
			TEXT("Native registration tests should compile, resolve, and execute the script module")))
		{
			if (!Diagnostics.IsEmpty())
			{
				TestRunner->AddInfo(Diagnostics);
			}
			return;
		}

		ASSERT_THAT(AreEqual(42, Result,
			TEXT("Native global-property registration test should expose the registered property to script code")));
	}

	TEST_METHOD(SimpleValueType)
	{
		asIScriptEngine* ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Native value-type registration test should create a standalone engine")));
		if (!bNativeCounterRegistered)
		{
			return;
		}

		int32 Result = 0;
		FString Diagnostics;
		AngelscriptNativeTestSupport::FScopedNativeModuleName ModuleScope(Engine, "NativeRegisterSimpleValueType");
		if (!this->Assert.IsTrue(ExecuteRegisteredScript(ScriptEngine, "NativeRegisterSimpleValueType", "int Entry() { NativeCounter Counter; Counter.Value = 19; return Counter.Value + 23; }", "int Entry()", Engine, Result, Diagnostics),
			TEXT("Native registration tests should compile, resolve, and execute the script module")))
		{
			if (!Diagnostics.IsEmpty())
			{
				TestRunner->AddInfo(Diagnostics);
			}
			return;
		}

		ASSERT_THAT(AreEqual(42, Result,
			TEXT("Native value-type registration test should allow script code to construct and use the registered POD type")));
	}
};

#endif
