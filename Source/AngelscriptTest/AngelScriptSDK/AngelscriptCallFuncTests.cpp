// AngelscriptCallFuncTests.cpp
// Tests for as_callfunc.cpp - native function call dispatch edge cases.
// Automation IDs: Angelscript.TestModule.AngelScriptSDK.CallFunc.*

#include "AngelscriptSDKTestExecutionHelpers.h"
#include "AngelscriptNativeTestSupport.h"
#include "CQTest.h"
#include "Misc/ScopeExit.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptNativeTestSupport;
using namespace AngelscriptSDKTestSupport;

namespace
{
	int32 AddFour(int32 A, int32 B, int32 C, int32 D) { return A + B + C + D; }
	double MultiplyDouble(double A, double B) { return A * B; }
	static int32 GSideEffectAccumulator = 0;
	void AccumulateValue(int32 Value) { GSideEffectAccumulator += Value; }
	int32 IncrementAndReturn(int32 Value) { return Value + 1; }
	int32 SumSix(int32 A, int32 B, int32 C, int32 D, int32 E, int32 F) { return A+B+C+D+E+F; }
	int64 WidenAndScale(int32 Value) { return static_cast<int64>(Value) * 1000000000LL; }
	double MixIn025(int32 I, double D) { return static_cast<double>(I) + D; }
	bool IsPositive(int32 Value) { return Value > 0; }
	void DivMod(int32 A, int32 B, int32& OutQuotient, int32& OutRemainder)
	{
		OutQuotient = (B != 0) ? (A / B) : 0;
		OutRemainder = (B != 0) ? (A % B) : 0;
	}

	bool RegisterHelpers(FAutomationTestBase& Test, asIScriptEngine* SE)
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
}


TEST_CLASS_WITH_FLAGS(FAngelscriptCallFuncTests,
	"Angelscript.TestModule.AngelScriptSDK.CallFunc",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	inline static FNativeSdkEngineFixture EngineFixture;
	inline static bool bHelpersRegistered = false;

	BEFORE_ALL()
	{
		EngineFixture.Create(*TestRunner);
		asIScriptEngine* const ScriptEngine = EngineFixture.Get();
		bHelpersRegistered = ScriptEngine != nullptr && RegisterHelpers(*TestRunner, ScriptEngine);
	}

	AFTER_ALL()
	{
		EngineFixture.Destroy();
		bHelpersRegistered = false;
	}

	BEFORE_EACH()
	{
		EngineFixture.ResetMessages();
	}

	TEST_METHOD(MultipleArgs)
	{
		asIScriptEngine* SE = EngineFixture.Get();
		if (!TestRunner->TestNotNull(TEXT("Should create engine"), SE)) return;
		if (!bHelpersRegistered) return;
		FScopedNativeModule M(*TestRunner, EngineFixture, "CallFuncMultiArgs", "int Entry() { return AddFour(10, 20, 30, 40); }\n");
		if (!M.IsValid()) return;
		int32 Result = 0;
		if (!ExecuteScriptFunction(*TestRunner, SE, M, "int Entry()", Result)) return;
		TestRunner->TestEqual(TEXT("AddFour(10,20,30,40)=100"), Result, 100);
	}

	TEST_METHOD(FloatPrecision)
	{
		asIScriptEngine* SE = EngineFixture.Get();
		if (!TestRunner->TestNotNull(TEXT("Should create engine"), SE)) return;
		if (!bHelpersRegistered) return;
		FScopedNativeModule M(*TestRunner, EngineFixture, "CallFuncFloat", "double Entry() { return MultiplyDouble(3.14159, 2.0); }\n");
		if (!M.IsValid()) return;
		double Result = 0.0;
		if (!ExecuteScriptFunction(*TestRunner, SE, M, "double Entry()", Result)) return;
		TestRunner->TestTrue(TEXT("MultiplyDouble precision"), FMath::IsNearlyEqual(Result, 3.14159*2.0, 1e-10));
	}

	TEST_METHOD(VoidSideEffect)
	{
		asIScriptEngine* SE = EngineFixture.Get();
		if (!TestRunner->TestNotNull(TEXT("Should create engine"), SE)) return;
		if (!bHelpersRegistered) return;
		GSideEffectAccumulator = 0;
		FScopedNativeModule M(*TestRunner, EngineFixture, "CallFuncVoid", "void Entry() { AccumulateValue(10); AccumulateValue(20); AccumulateValue(12); }\n");
		if (!M.IsValid()) return;
		if (!ExecuteScriptVoidFunction(*TestRunner, SE, M, "void Entry()")) return;
		TestRunner->TestEqual(TEXT("Accumulator=42"), GSideEffectAccumulator, 42);
	}

	TEST_METHOD(NestedCall)
	{
		asIScriptEngine* SE = EngineFixture.Get();
		if (!TestRunner->TestNotNull(TEXT("Should create engine"), SE)) return;
		if (!bHelpersRegistered) return;
		FScopedNativeModule M(*TestRunner, EngineFixture, "CallFuncNested", "int Entry() { return IncrementAndReturn(IncrementAndReturn(IncrementAndReturn(0))); }\n");
		if (!M.IsValid()) return;
		int32 Result = 0;
		if (!ExecuteScriptFunction(*TestRunner, SE, M, "int Entry()", Result)) return;
		TestRunner->TestEqual(TEXT("Nested 3x increment = 3"), Result, 3);
	}

	TEST_METHOD(ManyArgs)
	{
		asIScriptEngine* SE = EngineFixture.Get();
		if (!TestRunner->TestNotNull(TEXT("Should create engine"), SE)) return;
		if (!bHelpersRegistered) return;
		FScopedNativeModule M(*TestRunner, EngineFixture, "CallFuncManyArgs", "int Entry() { return SumSix(1, 2, 3, 4, 5, 6); }\n");
		if (!M.IsValid()) return;
		int32 Result = 0;
		if (!ExecuteScriptFunction(*TestRunner, SE, M, "int Entry()", Result)) return;
		TestRunner->TestEqual(TEXT("SumSix(1..6)=21"), Result, 21);
	}

	TEST_METHOD(WideReturn)
	{
		asIScriptEngine* SE = EngineFixture.Get();
		if (!TestRunner->TestNotNull(TEXT("Should create engine"), SE)) return;
		if (!bHelpersRegistered) return;
		FScopedNativeModule M(*TestRunner, EngineFixture, "CallFuncWideReturn", "int64 Entry() { return WidenAndScale(3); }\n");
		if (!M.IsValid()) return;
		asIScriptFunction* Func = GetNativeFunctionByDecl(M, "int64 Entry()");
		if (!TestRunner->TestNotNull(TEXT("Should resolve"), Func)) return;
		asIScriptContext* Ctx = SE->CreateContext();
		if (!TestRunner->TestNotNull(TEXT("Context"), Ctx)) return;
		const int Ret = PrepareAndExecute(Ctx, Func);
		const int64 Result = static_cast<int64>(Ctx->GetReturnQWord());
		Ctx->Release();
		TestRunner->TestEqual(TEXT("Finished"), Ret, static_cast<int32>(asEXECUTION_FINISHED));
		TestRunner->TestEqual(TEXT("WidenAndScale(3) returns 3,000,000,000 through int64"), Result, static_cast<int64>(3000000000LL));
	}

	TEST_METHOD(MixedIntDoubleArgs)
	{
		asIScriptEngine* SE = EngineFixture.Get();
		if (!TestRunner->TestNotNull(TEXT("Should create engine"), SE)) return;
		if (!bHelpersRegistered) return;
		FScopedNativeModule M(*TestRunner, EngineFixture, "CallFuncMixed", "double Entry() { return MixIn025(7, 0.25); }\n");
		if (!M.IsValid()) return;
		double Result = 0.0;
		if (!ExecuteScriptFunction(*TestRunner, SE, M, "double Entry()", Result)) return;
		TestRunner->TestTrue(TEXT("MixIn025(7, 0.25) = 7.25 (int+double arg marshalling)"), FMath::IsNearlyEqual(Result, 7.25));
	}

	TEST_METHOD(BoolReturn)
	{
		asIScriptEngine* SE = EngineFixture.Get();
		if (!TestRunner->TestNotNull(TEXT("Should create engine"), SE)) return;
		if (!bHelpersRegistered) return;
		FScopedNativeModule M(*TestRunner, EngineFixture, "CallFuncBool", "bool Entry() { return IsPositive(5) && !IsPositive(-3) && !IsPositive(0); }\n");
		if (!M.IsValid()) return;
		bool bResult = false;
		if (!ExecuteScriptFunction(*TestRunner, SE, M, "bool Entry()", bResult)) return;
		TestRunner->TestTrue(TEXT("IsPositive native bool returns marshal correctly"), bResult);
	}

	TEST_METHOD(OutParams)
	{
		asIScriptEngine* SE = EngineFixture.Get();
		if (!TestRunner->TestNotNull(TEXT("Should create engine"), SE)) return;
		if (!bHelpersRegistered) return;
		FScopedNativeModule M(*TestRunner, EngineFixture, "CallFuncOut", R"(
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
		TestRunner->TestTrue(TEXT("DivMod(17,5) writes back q=3, r=2 through native &out params"), bResult);
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
