#include "../Support/AngelscriptNativeCoreTestSupport.h"
#include "CQTest.h"
#include "Misc/ScopeExit.h"

#if WITH_ANGELSCRIPT_UNITTESTS

namespace GlobalRegistrationTest
{
	static int32 DoubleValue(int32 Value)
	{
		return Value * 2;
	}
}

TEST_CLASS_WITH_FLAGS(FGlobalRegistrationTests, "Angelscript.TestModule.AngelScriptSDK.Embedding.GlobalRegistration", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(GlobalRegistrationGlobalFunction)
	{
		using namespace AngelscriptNativeTestSupport;
		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		asIScriptEngine* ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Global function registration should create a raw SDK engine")));
		if (ScriptEngine == nullptr) return;
		const ASAutoCaller::FunctionCaller Caller = ASAutoCaller::MakeFunctionCaller(GlobalRegistrationTest::DoubleValue);
		ASSERT_THAT(IsTrue(ScriptEngine->RegisterGlobalFunction("int DoubleValue(int Value)", asFUNCTION(GlobalRegistrationTest::DoubleValue), asCALL_CDECL, *(asFunctionCaller*)&Caller) >= 0,
			TEXT("Global function registration should accept the native declaration")));
		FScopedNativeModule Module(*TestRunner, Engine, "GlobalFunctionRegistration", "int Entry() { return DoubleValue(21); }");
		if (!Module.IsValid()) return;
		asIScriptFunction* Function = GetNativeFunctionByExactDecl(Module, "int Entry()");
		if (!this->Assert.IsNotNull(Function, TEXT("Global function registration should resolve the script entry"))) return;
		asIScriptContext* Context = ScriptEngine->CreateContext();
		if (!this->Assert.IsNotNull(Context, TEXT("Global function registration should create a context"))) return;
		ON_SCOPE_EXIT { Context->Release(); };
		ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_FINISHED), PrepareAndExecute(Context, Function), TEXT("Registered global function should execute from script")));
		ASSERT_THAT(AreEqual(42, static_cast<int32>(Context->GetReturnDWord()), TEXT("Registered global function should marshal its return value")));
	}

	TEST_METHOD(GlobalRegistrationGlobalProperty)
	{
		using namespace AngelscriptNativeTestSupport;
		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		asIScriptEngine* ScriptEngine = Engine.Get();
		int32 NativeValue = 21;
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Global property registration should create a raw SDK engine")));
		if (ScriptEngine == nullptr) return;
		ASSERT_THAT(IsTrue(ScriptEngine->RegisterGlobalProperty("int NativeValue", &NativeValue) >= 0, TEXT("Global property registration should accept the native address")));
		FScopedNativeModule Module(*TestRunner, Engine, "GlobalPropertyRegistration", "int Entry() { NativeValue += 1; return NativeValue * 2; }");
		if (!Module.IsValid()) return;
		asIScriptFunction* Function = GetNativeFunctionByExactDecl(Module, "int Entry()");
		if (!this->Assert.IsNotNull(Function, TEXT("Global property registration should resolve the script entry"))) return;
		asIScriptContext* Context = ScriptEngine->CreateContext();
		if (!this->Assert.IsNotNull(Context, TEXT("Global property registration should create a context"))) return;
		ON_SCOPE_EXIT { Context->Release(); };
		ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_FINISHED), PrepareAndExecute(Context, Function), TEXT("Registered global property should execute from script")));
		ASSERT_THAT(AreEqual(44, static_cast<int32>(Context->GetReturnDWord()), TEXT("Registered global property should preserve native address mutation")));
		ASSERT_THAT(AreEqual(22, NativeValue, TEXT("Script mutation should update the registered native property")));
	}
};

#endif
