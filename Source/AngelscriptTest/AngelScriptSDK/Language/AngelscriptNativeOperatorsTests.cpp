#include "../Support/AngelscriptNativeExecutionTestSupport.h"

#include "CQTest.h"
#include "Misc/ScopeExit.h"

#if WITH_ANGELSCRIPT_UNITTESTS

namespace
{
	template<typename T>
	bool ExpectOperatorValue(FAutomationTestBase& Test, asIScriptEngine* Engine, asIScriptModule* Module, const char* Declaration, T Expected, const TCHAR* Context)
	{
		FNoDiscardAsserter Assert(Test);
		AngelscriptSDKTestSupport::FSdkFunctionInvoker Invoker(Test, Engine, Module, Declaration);
		return Assert.IsTrue(Invoker.IsValid(), Context) && Assert.AreEqual(Expected, Invoker.CallAndReturn<T>(T{}), Context);
	}
}

TEST_CLASS_WITH_FLAGS(FOperatorsTests, "Angelscript.TestModule.AngelScriptSDK.Language.Operators", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(OperatorsBitwise)
	{
		using namespace AngelscriptNativeTestSupport;
		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };
		FScopedNativeModule Module(*TestRunner, Engine, "SDKOperatorBitwise", R"AS(
uint BitAnd() { uint a = 0b11001100; uint b = 0b10101010; return a & b; }
uint BitOr() { uint a = 0b11001100; uint b = 0b10101010; return a | b; }
uint BitXor() { uint a = 0b11001100; uint b = 0b10101010; return a ^ b; }
uint8 BitNot() { uint8 c = 0b11110000; return ~c; }
uint ShiftLeft() { return 5 << 2; }
uint ShiftRight() { return 20 >> 2; }
uint CompoundBitwise() { uint x = 0xFF; x &= 0x0F; x |= 0xF0; x ^= 0x55; x <<= 1; x >>= 2; return x; }
)AS");
		if (!Module.IsValid()) return;
		if (!ExpectOperatorValue(*TestRunner, Engine.Get(), Module, "uint BitAnd()", uint32(136), TEXT("Bitwise and should execute"))) return;
		if (!ExpectOperatorValue(*TestRunner, Engine.Get(), Module, "uint BitOr()", uint32(238), TEXT("Bitwise or should execute"))) return;
		if (!ExpectOperatorValue(*TestRunner, Engine.Get(), Module, "uint BitXor()", uint32(102), TEXT("Bitwise xor should execute"))) return;
		if (!ExpectOperatorValue(*TestRunner, Engine.Get(), Module, "uint8 BitNot()", uint8(15), TEXT("Bitwise not should execute"))) return;
		if (!ExpectOperatorValue(*TestRunner, Engine.Get(), Module, "uint ShiftLeft()", uint32(20), TEXT("Left shift should execute"))) return;
		if (!ExpectOperatorValue(*TestRunner, Engine.Get(), Module, "uint ShiftRight()", uint32(5), TEXT("Right shift should execute"))) return;
		ExpectOperatorValue(*TestRunner, Engine.Get(), Module, "uint CompoundBitwise()", uint32(0x55), TEXT("Compound bitwise operators should execute in order"));
	}

	TEST_METHOD(OperatorsAssignment)
	{
		using namespace AngelscriptNativeTestSupport;
		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };
		FScopedNativeModule Module(*TestRunner, Engine, "SDKOperatorAssignment", R"AS(
int SimpleAssign() { int a = 10; return a; }
int AddAssign() { int b = 10; b += 5; return b; }
int SubAssign() { int c = 10; c -= 3; return c; }
int MulAssign() { int d = 10; d *= 2; return d; }
int DivAssign() { int e = 10; e /= 2; return e; }
int ModAssign() { int f = 10; f %= 3; return f; }
int ChainedAssign() { int x = 0, y = 0, z = 0; x = y = z = 42; return x + y + z; }
)AS");
		if (!Module.IsValid()) return;
		const TPair<const char*, int32> Cases[] = { { "int SimpleAssign()", 10 }, { "int AddAssign()", 15 }, { "int SubAssign()", 7 }, { "int MulAssign()", 20 }, { "int DivAssign()", 5 }, { "int ModAssign()", 1 }, { "int ChainedAssign()", 126 } };
		for (const TPair<const char*, int32>& Case : Cases)
		{
			if (!ExpectOperatorValue(*TestRunner, Engine.Get(), Module, Case.Key, Case.Value, TEXT("Assignment expression should produce its expected value"))) return;
		}
	}

	TEST_METHOD(OperatorsPow)
	{
		using namespace AngelscriptNativeTestSupport;
		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };
		FScopedNativeModule Module(*TestRunner, Engine, "SDKOperatorPow", R"AS(
int IntPow() { return 3 ** 2; }
double SqrtPow() { return 9.0 ** 0.5; }
double FractionalPow() { return 2.5 ** 2; }
int WidePow() { return 2 ** 10; }
void Overflow() { double x = 1.0e100; x = x ** 6.0; }
)AS");
		if (!Module.IsValid()) return;
		if (!ExpectOperatorValue(*TestRunner, Engine.Get(), Module, "int IntPow()", 9, TEXT("Integer exponentiation should execute"))) return;
		if (!ExpectOperatorValue(*TestRunner, Engine.Get(), Module, "int WidePow()", 1024, TEXT("Wide integer exponentiation should execute"))) return;
		AngelscriptSDKTestSupport::FSdkFunctionInvoker SqrtInvoker(*TestRunner, Engine.Get(), Module, "double SqrtPow()");
		ASSERT_THAT(IsTrue(SqrtInvoker.IsValid(), TEXT("Square-root exponent function should resolve")));
		ASSERT_THAT(IsNear(3.0, SqrtInvoker.CallAndReturn<double>(0.0), 0.0001, TEXT("Fractional exponent should execute")));
		AngelscriptSDKTestSupport::FSdkFunctionInvoker FractionalInvoker(*TestRunner, Engine.Get(), Module, "double FractionalPow()");
		ASSERT_THAT(IsTrue(FractionalInvoker.IsValid(), TEXT("Fractional exponent function should resolve")));
		ASSERT_THAT(IsNear(6.25, FractionalInvoker.CallAndReturn<double>(0.0), 0.0001, TEXT("Fractional base exponent should execute")));
		asIScriptFunction* Function = GetNativeFunctionByDecl(Module, "void Overflow()");
		ASSERT_THAT(IsNotNull(Function, TEXT("Overflow function should resolve")));
		asIScriptContext* Context = Engine.Get()->CreateContext();
		ASSERT_THAT(IsNotNull(Context, TEXT("Overflow test should create a context")));
		ON_SCOPE_EXIT { Context->Release(); };
		ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_EXCEPTION), PrepareAndExecute(Context, Function), TEXT("Exponent overflow should raise an exception")));
		ASSERT_THAT(AreEqual(FString(TEXT("Overflow in exponent operation")), FString(UTF8_TO_TCHAR(Context->GetExceptionString())), TEXT("Exponent overflow should preserve its exception text")));
	}

};

#endif
