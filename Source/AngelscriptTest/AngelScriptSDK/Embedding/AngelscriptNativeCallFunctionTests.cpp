// Raw SDK native call-function coverage.
// Tests for as_callfunc.cpp - native function call dispatch edge cases.
// Automation IDs: Angelscript.TestModule.AngelScriptSDK.Embedding.CallFunction.*

#include "Support/AngelscriptNativeExecutionTestSupport.h"
#include "../Support/AngelscriptNativeCoreTestSupport.h"
#include "CQTest.h"
#include "Misc/ScopeExit.h"

#if WITH_ANGELSCRIPT_UNITTESTS


TEST_CLASS_WITH_FLAGS(FCallFunctionTests,
	"Angelscript.TestModule.AngelScriptSDK.Embedding.CallFunction",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	static int32 AddFour(int32 A, int32 B, int32 C, int32 D) { return A + B + C + D; }
	static double MultiplyDouble(double A, double B) { return A * B; }
	inline static int32 GSideEffectAccumulator = 0;
	static void AccumulateValue(int32 Value) { GSideEffectAccumulator += Value; }
	static int32 IncrementAndReturn(int32 Value) { return Value + 1; }
	static int32 SumSix(int32 A, int32 B, int32 C, int32 D, int32 E, int32 F) { return A+B+C+D+E+F; }
	static int64 WidenAndScale(int32 Value) { return static_cast<int64>(Value) * 1000000000LL; }
	static double MixIn025(int32 I, double D) { return static_cast<double>(I) + D; }
	static bool IsPositive(int32 Value) { return Value > 0; }
	static void DivMod(int32 A, int32 B, int32& OutQuotient, int32& OutRemainder)
	{
		OutQuotient = (B != 0) ? (A / B) : 0;
		OutRemainder = (B != 0) ? (A % B) : 0;
	}

	static bool RegisterHelpers(FAutomationTestBase& Test, asIScriptEngine* SE)
	{
		ASAutoCaller::FunctionCaller Caller;
		int R;
		Caller = ASAutoCaller::MakeFunctionCaller(AddFour);
		R = SE->RegisterGlobalFunction("int AddFour(int,int,int,int)", asFUNCTION(AddFour), asCALL_CDECL, *(asFunctionCaller*)&Caller);
		if (R < 0) { Test.AddInfo(TEXT("Native function registration not available in headless mode, skipping")); return false; }
		Caller = ASAutoCaller::MakeFunctionCaller(MultiplyDouble);
		R = SE->RegisterGlobalFunction("double MultiplyDouble(double,double)", asFUNCTION(MultiplyDouble), asCALL_CDECL, *(asFunctionCaller*)&Caller);
		if (R < 0) { Test.AddInfo(TEXT("Native function registration not available in headless mode, skipping")); return false; }
		Caller = ASAutoCaller::MakeFunctionCaller(AccumulateValue);
		R = SE->RegisterGlobalFunction("void AccumulateValue(int)", asFUNCTION(AccumulateValue), asCALL_CDECL, *(asFunctionCaller*)&Caller);
		if (R < 0) { Test.AddInfo(TEXT("Native function registration not available in headless mode, skipping")); return false; }
		Caller = ASAutoCaller::MakeFunctionCaller(IncrementAndReturn);
		R = SE->RegisterGlobalFunction("int IncrementAndReturn(int)", asFUNCTION(IncrementAndReturn), asCALL_CDECL, *(asFunctionCaller*)&Caller);
		if (R < 0) { Test.AddInfo(TEXT("Native function registration not available in headless mode, skipping")); return false; }
		Caller = ASAutoCaller::MakeFunctionCaller(SumSix);
		R = SE->RegisterGlobalFunction("int SumSix(int,int,int,int,int,int)", asFUNCTION(SumSix), asCALL_CDECL, *(asFunctionCaller*)&Caller);
		if (R < 0) { Test.AddInfo(TEXT("Native function registration not available in headless mode, skipping")); return false; }
		Caller = ASAutoCaller::MakeFunctionCaller(WidenAndScale);
		R = SE->RegisterGlobalFunction("int64 WidenAndScale(int)", asFUNCTION(WidenAndScale), asCALL_CDECL, *(asFunctionCaller*)&Caller);
		if (R < 0) { Test.AddInfo(TEXT("Native function registration not available in headless mode, skipping")); return false; }
		Caller = ASAutoCaller::MakeFunctionCaller(MixIn025);
		R = SE->RegisterGlobalFunction("double MixIn025(int, double)", asFUNCTION(MixIn025), asCALL_CDECL, *(asFunctionCaller*)&Caller);
		if (R < 0) { Test.AddInfo(TEXT("Native function registration not available in headless mode, skipping")); return false; }
		Caller = ASAutoCaller::MakeFunctionCaller(IsPositive);
		R = SE->RegisterGlobalFunction("bool IsPositive(int)", asFUNCTION(IsPositive), asCALL_CDECL, *(asFunctionCaller*)&Caller);
		if (R < 0) { Test.AddInfo(TEXT("Native function registration not available in headless mode, skipping")); return false; }
		Caller = ASAutoCaller::MakeFunctionCaller(DivMod);
		R = SE->RegisterGlobalFunction("void DivMod(int, int, int &out, int &out)", asFUNCTION(DivMod), asCALL_CDECL, *(asFunctionCaller*)&Caller);
		if (R < 0) { Test.AddInfo(TEXT("Native function registration not available in headless mode, skipping")); return false; }
		return true;
	}

public:
	inline static AngelscriptNativeTestSupport::FNativeTestEngine Engine;
	inline static bool bHelpersRegistered = false;

	BEFORE_ALL()
	{
		Engine.Create(*TestRunner);
		asIScriptEngine* const ScriptEngine = Engine.Get();
		bHelpersRegistered = ScriptEngine != nullptr && RegisterHelpers(*TestRunner, ScriptEngine);
	}

	AFTER_ALL()
	{
		Engine.Destroy();
		bHelpersRegistered = false;
	}

	BEFORE_EACH()
	{
		Engine.ResetMessages();
	}

	TEST_METHOD(CallFunctionMultipleArgs)
	{
		using namespace AngelscriptNativeTestSupport;
		using namespace AngelscriptSDKTestSupport;

		asIScriptEngine* SE = Engine.Get();
		ASSERT_THAT(IsNotNull(SE, TEXT("Should create engine")));
		if (!bHelpersRegistered) return;
		AngelscriptNativeTestSupport::FScopedNativeModule M(*TestRunner, Engine, "CallFuncMultiArgs", "int Entry() { return AddFour(10, 20, 30, 40); }\n");
		if (!M.IsValid()) return;
		int32 Result = 0;
		if (!ExecuteScriptFunction(*TestRunner, SE, M, "int Entry()", Result)) return;
		ASSERT_THAT(AreEqual(100, Result, TEXT("AddFour(10,20,30,40)=100")));
	}

	TEST_METHOD(CallFunctionFloatPrecision)
	{
		using namespace AngelscriptNativeTestSupport;
		using namespace AngelscriptSDKTestSupport;

		asIScriptEngine* SE = Engine.Get();
		ASSERT_THAT(IsNotNull(SE, TEXT("Should create engine")));
		if (!bHelpersRegistered) return;
		AngelscriptNativeTestSupport::FScopedNativeModule M(*TestRunner, Engine, "CallFuncFloat", "double Entry() { return MultiplyDouble(3.14159, 2.0); }\n");
		if (!M.IsValid()) return;
		double Result = 0.0;
		if (!ExecuteScriptFunction(*TestRunner, SE, M, "double Entry()", Result)) return;
		ASSERT_THAT(IsNear(3.14159 * 2.0, Result, 1e-10, TEXT("MultiplyDouble precision")));
	}

	TEST_METHOD(CallFunctionVoidSideEffect)
	{
		using namespace AngelscriptNativeTestSupport;
		using namespace AngelscriptSDKTestSupport;

		asIScriptEngine* SE = Engine.Get();
		ASSERT_THAT(IsNotNull(SE, TEXT("Should create engine")));
		if (!bHelpersRegistered) return;
		GSideEffectAccumulator = 0;
		AngelscriptNativeTestSupport::FScopedNativeModule M(*TestRunner, Engine, "CallFuncVoid", "void Entry() { AccumulateValue(10); AccumulateValue(20); AccumulateValue(12); }\n");
		if (!M.IsValid()) return;
		if (!ExecuteScriptVoidFunction(*TestRunner, SE, M, "void Entry()")) return;
		ASSERT_THAT(AreEqual(42, GSideEffectAccumulator, TEXT("Accumulator=42")));
	}

	TEST_METHOD(CallFunctionNestedCall)
	{
		using namespace AngelscriptNativeTestSupport;
		using namespace AngelscriptSDKTestSupport;

		asIScriptEngine* SE = Engine.Get();
		ASSERT_THAT(IsNotNull(SE, TEXT("Should create engine")));
		if (!bHelpersRegistered) return;
		AngelscriptNativeTestSupport::FScopedNativeModule M(*TestRunner, Engine, "CallFuncNested", "int Entry() { return IncrementAndReturn(IncrementAndReturn(IncrementAndReturn(0))); }\n");
		if (!M.IsValid()) return;
		int32 Result = 0;
		if (!ExecuteScriptFunction(*TestRunner, SE, M, "int Entry()", Result)) return;
		ASSERT_THAT(AreEqual(3, Result, TEXT("Nested 3x increment = 3")));
	}

	TEST_METHOD(CallFunctionManyArgs)
	{
		using namespace AngelscriptNativeTestSupport;
		using namespace AngelscriptSDKTestSupport;

		asIScriptEngine* SE = Engine.Get();
		ASSERT_THAT(IsNotNull(SE, TEXT("Should create engine")));
		if (!bHelpersRegistered) return;
		AngelscriptNativeTestSupport::FScopedNativeModule M(*TestRunner, Engine, "CallFuncManyArgs", "int Entry() { return SumSix(1, 2, 3, 4, 5, 6); }\n");
		if (!M.IsValid()) return;
		int32 Result = 0;
		if (!ExecuteScriptFunction(*TestRunner, SE, M, "int Entry()", Result)) return;
		ASSERT_THAT(AreEqual(21, Result, TEXT("SumSix(1..6)=21")));
	}

	TEST_METHOD(CallFunctionWideReturn)
	{
		using namespace AngelscriptNativeTestSupport;
		using namespace AngelscriptSDKTestSupport;

		asIScriptEngine* SE = Engine.Get();
		ASSERT_THAT(IsNotNull(SE, TEXT("Should create engine")));
		if (!bHelpersRegistered) return;
		AngelscriptNativeTestSupport::FScopedNativeModule M(*TestRunner, Engine, "CallFuncWideReturn", "int64 Entry() { return WidenAndScale(3); }\n");
		if (!M.IsValid()) return;
		asIScriptFunction* Func = GetNativeFunctionByDecl(M, "int64 Entry()");
		ASSERT_THAT(IsNotNull(Func, TEXT("Should resolve")));
		asIScriptContext* Ctx = SE->CreateContext();
		ASSERT_THAT(IsNotNull(Ctx, TEXT("Context")));
		ON_SCOPE_EXIT
		{
			Ctx->Release();
		};
		const int Ret = PrepareAndExecute(Ctx, Func);
		const int64 Result = static_cast<int64>(Ctx->GetReturnQWord());
		ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_FINISHED), Ret, TEXT("Finished")));
		ASSERT_THAT(AreEqual(static_cast<int64>(3000000000LL), Result,
			TEXT("WidenAndScale(3) returns 3,000,000,000 through int64")));
	}

	TEST_METHOD(MixedIntDoubleArgs)
	{
		using namespace AngelscriptNativeTestSupport;
		using namespace AngelscriptSDKTestSupport;

		asIScriptEngine* SE = Engine.Get();
		ASSERT_THAT(IsNotNull(SE, TEXT("Should create engine")));
		if (!bHelpersRegistered) return;
		AngelscriptNativeTestSupport::FScopedNativeModule M(*TestRunner, Engine, "CallFuncMixed", "double Entry() { return MixIn025(7, 0.25); }\n");
		if (!M.IsValid()) return;
		double Result = 0.0;
		if (!ExecuteScriptFunction(*TestRunner, SE, M, "double Entry()", Result)) return;
		ASSERT_THAT(IsNear(7.25, Result, 1e-8,
			TEXT("MixIn025(7, 0.25) = 7.25 (int+double arg marshalling)")));
	}

	TEST_METHOD(CallFunctionBoolReturn)
	{
		using namespace AngelscriptNativeTestSupport;
		using namespace AngelscriptSDKTestSupport;

		asIScriptEngine* SE = Engine.Get();
		ASSERT_THAT(IsNotNull(SE, TEXT("Should create engine")));
		if (!bHelpersRegistered) return;
		AngelscriptNativeTestSupport::FScopedNativeModule M(*TestRunner, Engine, "CallFuncBool", "bool Entry() { return IsPositive(5) && !IsPositive(-3) && !IsPositive(0); }\n");
		if (!M.IsValid()) return;
		bool bResult = false;
		if (!ExecuteScriptFunction(*TestRunner, SE, M, "bool Entry()", bResult)) return;
		ASSERT_THAT(IsTrue(bResult, TEXT("IsPositive native bool returns marshal correctly")));
	}

	TEST_METHOD(CallFunctionOutParams)
	{
		using namespace AngelscriptNativeTestSupport;
		using namespace AngelscriptSDKTestSupport;

		asIScriptEngine* SE = Engine.Get();
		ASSERT_THAT(IsNotNull(SE, TEXT("Should create engine")));
		if (!bHelpersRegistered) return;
		AngelscriptNativeTestSupport::FScopedNativeModule M(*TestRunner, Engine, "CallFuncOut", R"(
bool Entry()
{
	int q = 0;
	int r = 0;
	DivMod(17, 5, q, r);
	return q == 3 && r == 2;
}
)");
		if (!M.IsValid()) return;
		bool bResult = false;
		if (!ExecuteScriptFunction(*TestRunner, SE, M, "bool Entry()", bResult)) return;
		ASSERT_THAT(IsTrue(bResult, TEXT("DivMod(17,5) writes back q=3, r=2 through native &out params")));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
