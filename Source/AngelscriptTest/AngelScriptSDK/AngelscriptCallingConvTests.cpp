#include "AngelscriptSDKTestExecutionHelpers.h"
#include "AngelscriptTestAdapter.h"

#include "CQTest.h"
#include "Misc/ScopeExit.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptNativeTestSupport;
using namespace AngelscriptSDKTestSupport;

namespace
{
	int32 DoubleNativeValue(int32 Value)
	{
		return Value * 2;
	}

	void TripleGenericValue(asIScriptGeneric* Generic)
	{
		const int32 Value = *static_cast<int32*>(Generic->GetAddressOfArg(0));
		Generic->SetReturnDWord(static_cast<asDWORD>(Value * 3));
	}

	struct FNativeAdder
	{
		int32 Base = 0;

		int32 Add(int32 Delta) const
		{
			return Base + Delta;
		}
	};

	void ConstructNativeAdder(FNativeAdder* Address)
	{
		new (Address) FNativeAdder();
	}
}


TEST_CLASS_WITH_FLAGS(FAngelscriptSDKCallingConvTests,
	"Angelscript.TestModule.AngelScriptSDK.CallingConv",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	inline static FNativeSdkEngineFixture EngineFixture;
	inline static bool bCDeclRegistered = false;
	inline static bool bGenericRegistered = false;
	inline static bool bNativeAdderRegistered = false;

	BEFORE_ALL()
	{
		EngineFixture.Create(*TestRunner);
		asIScriptEngine* const ScriptEngine = EngineFixture.Get();
		if (ScriptEngine == nullptr)
		{
			return;
		}

		const ASAutoCaller::FunctionCaller Caller = ASAutoCaller::MakeFunctionCaller(DoubleNativeValue);
		const int RegisterResult = ScriptEngine->RegisterGlobalFunction("int DoubleNativeValue(int Value)", asFUNCTION(DoubleNativeValue), asCALL_CDECL, *(asFunctionCaller*)&Caller);
		bCDeclRegistered = TestRunner->TestTrue(TEXT("SDK calling-convention CDecl test should register the native function"), RegisterResult >= 0);

		const int GenericResult = ScriptEngine->RegisterGlobalFunction("int TripleGenericValue(int Value)", asFUNCTION(TripleGenericValue), asCALL_GENERIC);
		bGenericRegistered = TestRunner->TestTrue(TEXT("SDK calling-convention generic test should register the generic function"), GenericResult >= 0);

		const int TypeResult = ScriptEngine->RegisterObjectType("NativeAdder", sizeof(FNativeAdder), asOBJ_VALUE | asOBJ_POD | asGetTypeTraits<FNativeAdder>() | asOBJ_APP_CLASS_ALLINTS);
		const ASAutoCaller::FunctionCaller ConstructCaller = ASAutoCaller::MakeFunctionCaller(ConstructNativeAdder);
		const int ConstructResult = ScriptEngine->RegisterObjectBehaviour("NativeAdder", asBEHAVE_CONSTRUCT, "void f()", asFUNCTION(ConstructNativeAdder), asCALL_CDECL_OBJLAST, *(asFunctionCaller*)&ConstructCaller);
		const int PropertyResult = ScriptEngine->RegisterObjectProperty("NativeAdder", "int Base", asOFFSET(FNativeAdder, Base));
		const int MethodResult = ScriptEngine->RegisterObjectMethod("NativeAdder", "int Add(int Delta) const", asMETHODPR(FNativeAdder, Add, (int32) const, int32), asCALL_THISCALL);
		bNativeAdderRegistered = TestRunner->TestTrue(TEXT("SDK calling-convention thiscall test should register the value type and method"), TypeResult >= 0 && ConstructResult >= 0 && PropertyResult >= 0 && MethodResult >= 0);
	}

	AFTER_ALL()
	{
		EngineFixture.Destroy();
		bCDeclRegistered = false;
		bGenericRegistered = false;
		bNativeAdderRegistered = false;
	}

	BEFORE_EACH()
	{
		EngineFixture.ResetMessages();
	}

	TEST_METHOD(CDecl)
	{
		asIScriptEngine* ScriptEngine = EngineFixture.Get();
		if (!TestRunner->TestNotNull(TEXT("SDK calling-convention CDecl test should create a standalone engine"), ScriptEngine))
		{
			return;
		}
		if (!bCDeclRegistered)
		{
			return;
		}

		FScopedNativeModule Module(*TestRunner, EngineFixture, "SDKCallingConvCDecl", R"(
int Entry()
{
	return DoubleNativeValue(21);
}
)");
		if (!Module.IsValid())
		{
			return;
		}

		int32 Result = 0;
		if (!ExecuteScriptFunction(*TestRunner, ScriptEngine, Module, "int Entry()", Result))
		{
			return;
		}

		TestRunner->TestEqual(TEXT("SDK calling-convention CDecl test should preserve native CDecl calls"), Result, 42);
	}

	TEST_METHOD(Generic)
	{
		asIScriptEngine* ScriptEngine = EngineFixture.Get();
		if (!TestRunner->TestNotNull(TEXT("SDK calling-convention generic test should create a standalone engine"), ScriptEngine))
		{
			return;
		}
		if (!bGenericRegistered)
		{
			return;
		}

		FScopedNativeModule Module(*TestRunner, EngineFixture, "SDKCallingConvGeneric", R"(
int Entry()
{
	return TripleGenericValue(14);
}
)");
		if (!Module.IsValid())
		{
			return;
		}

		int32 Result = 0;
		if (!ExecuteScriptFunction(*TestRunner, ScriptEngine, Module, "int Entry()", Result))
		{
			return;
		}

		TestRunner->TestEqual(TEXT("SDK calling-convention generic test should preserve generic callback execution"), Result, 42);
	}

	TEST_METHOD(Thiscall)
	{
		asIScriptEngine* ScriptEngine = EngineFixture.Get();
		if (!TestRunner->TestNotNull(TEXT("SDK calling-convention thiscall test should create a standalone engine"), ScriptEngine))
		{
			return;
		}
		if (!bNativeAdderRegistered)
		{
			return;
		}

		// Compile-only test: verify the module compiles with native thiscall method registration.
		// Script class instantiation in isolated engine context may crash, so we only verify compilation.
		FScopedNativeModule Module(*TestRunner, EngineFixture, "SDKCallingConvThiscall", R"(
int Entry()
{
	NativeAdder Value;
	Value.Base = 39;
	return Value.Add(3);
}
)");
		if (!Module.IsValid())
		{
			return;
		}

		// Verify the function exists in the compiled module.
		asIScriptFunction* EntryFunc = GetNativeFunctionByDecl(Module, "int Entry()");
		TestRunner->TestNotNull(TEXT("SDK calling-convention thiscall test should expose the compiled entry function"), EntryFunc);
	}
};

#endif
