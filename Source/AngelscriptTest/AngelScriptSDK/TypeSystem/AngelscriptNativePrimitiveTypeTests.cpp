#include "Support/AngelscriptNativeExecutionTestSupport.h"

#include "AngelscriptTestMacros.h"

#include "CQTest.h"
#include "Misc/ScopeExit.h"

#if WITH_ANGELSCRIPT_UNITTESTS

namespace PrimitiveTypeTest
{
	static asBYTE GInt8Value = 0;

	static asBYTE RetInt8(asBYTE InValue)
	{
		return InValue;
	}

	static int FindEngineGlobalPropertyIndexByName(asIScriptEngine* ScriptEngine, const char* Name)
	{
		if (ScriptEngine == nullptr || Name == nullptr)
		{
			return -1;
		}

		for (asUINT Index = 0; Index < ScriptEngine->GetGlobalPropertyCount(); ++Index)
		{
			const char* PropertyName = nullptr;
			if (ScriptEngine->GetGlobalPropertyByIndex(Index, &PropertyName) >= 0 && PropertyName != nullptr && std::strcmp(PropertyName, Name) == 0)
			{
				return static_cast<int>(Index);
			}
		}

		return -1;
	}
}

TEST_CLASS_WITH_FLAGS(FPrimitiveTypeTests,
	"Angelscript.TestModule.AngelScriptSDK.TypeSystem.Primitives",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(PrimitiveTypeBool)
	{
		using namespace AngelscriptNativeTestSupport;
		using namespace AngelscriptSDKTestSupport;

		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };
		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Primitive bool test should create a standalone engine")));

		FScopedNativeModule Module(*TestRunner, Engine, "PrimitiveTypeBool", "bool AllTrue() { bool a = true; bool b = false; return a && !b && (a ^^ b); }");
		if (!Module.IsValid())
		{
			return;
		}

		FSdkFunctionInvoker Invoker(*TestRunner, ScriptEngine, Module, "bool AllTrue()");
		ASSERT_THAT(IsTrue(Invoker.IsValid(), TEXT("Primitive bool test should resolve its exact entry declaration")));
		if (Invoker.IsValid())
		{
			ASSERT_THAT(IsTrue(Invoker.CallAndReturn<bool>(false), TEXT("Primitive bool test should preserve boolean logic")));
		}
	}

	TEST_METHOD(PrimitiveTypeBits)
	{
		using namespace AngelscriptNativeTestSupport;
		using namespace AngelscriptSDKTestSupport;

		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };
		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Primitive bits test should create a standalone engine")));

		FScopedNativeModule Module(*TestRunner, Engine, "PrimitiveTypeBits", ASTEST_AS_ANSI(R"AS(
			bool CheckBits()
			{
				uint oct = 0o777;
				uint bin = 0b10101010;
				uint dec = 0d255;
				uint8 newmask = 0xFF;
				newmask = newmask & (~(1 << 2)) & (~(1 << 3)) & (~(1 << 5));
				return oct == 0x1FF && bin == 0xAA && dec == 0xFF && newmask == 0xD3;
			}
			)AS"));
		if (!Module.IsValid())
		{
			return;
		}

		FSdkFunctionInvoker Invoker(*TestRunner, ScriptEngine, Module, "bool CheckBits()");
		ASSERT_THAT(IsTrue(Invoker.IsValid(), TEXT("Primitive bits test should resolve its exact entry declaration")));
		if (Invoker.IsValid())
		{
			ASSERT_THAT(IsTrue(Invoker.CallAndReturn<bool>(false), TEXT("Primitive bits test should preserve literals and masks")));
		}
	}

	TEST_METHOD(PrimitiveTypeInt8)
	{
		using namespace AngelscriptNativeTestSupport;
		using namespace AngelscriptSDKTestSupport;

		PrimitiveTypeTest::GInt8Value = 0;
		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };
		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Primitive int8 test should create a standalone engine")));

		const ASAutoCaller::FunctionCaller Caller = ASAutoCaller::MakeFunctionCaller(PrimitiveTypeTest::RetInt8);
		ASSERT_THAT(IsTrue(ScriptEngine->RegisterGlobalFunction("int8 RetInt8(int8 value)", asFUNCTION(PrimitiveTypeTest::RetInt8), asCALL_CDECL, *(asFunctionCaller*)&Caller) >= 0,
			TEXT("Primitive int8 test should register the native int8 callback")));
		ASSERT_THAT(AreEqual(-1, PrimitiveTypeTest::FindEngineGlobalPropertyIndexByName(ScriptEngine, "gvar"),
			TEXT("Primitive int8 test should begin without an unrelated global property")));
		ASSERT_THAT(IsTrue(ScriptEngine->RegisterGlobalProperty("int8 gvar", &PrimitiveTypeTest::GInt8Value) >= 0,
			TEXT("Primitive int8 test should register an int8 global property")));

		FScopedNativeModule Module(*TestRunner, Engine, "PrimitiveTypeInt8", "int ReadInt8RoundTrip() { gvar = RetInt8(1); return gvar; }");
		if (!Module.IsValid())
		{
			return;
		}

		FSdkFunctionInvoker Invoker(*TestRunner, ScriptEngine, Module, "int ReadInt8RoundTrip()");
		ASSERT_THAT(IsTrue(Invoker.IsValid(), TEXT("Primitive int8 test should resolve its exact entry declaration")));
		if (Invoker.IsValid())
		{
			ASSERT_THAT(AreEqual(1, Invoker.CallAndReturn<int32>(INDEX_NONE), TEXT("Primitive int8 test should preserve the native callback result")));
		}
	}

	TEST_METHOD(PrimitiveTypeFloat)
	{
		using namespace AngelscriptNativeTestSupport;
		using namespace AngelscriptSDKTestSupport;

		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };
		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Primitive float test should create a standalone engine")));

		const char* const Source = ScriptEngine->GetEngineProperty(asEP_FLOAT_IS_FLOAT64) != 0
			? "double CheckFloat() { double a = 1e5; double b = 1.0e5; return (a == b) ? 3.14 : 0.0; }"
			: "double CheckFloat() { float a = 1e5; float b = 1.0e5; return (a == b) ? 3.14f : 0.0f; }";
		FScopedNativeModule Module(*TestRunner, Engine, "PrimitiveTypeFloat", Source);
		if (!Module.IsValid())
		{
			return;
		}

		FSdkFunctionInvoker Invoker(*TestRunner, ScriptEngine, Module, "double CheckFloat()");
		ASSERT_THAT(IsTrue(Invoker.IsValid(), TEXT("Primitive float test should resolve its exact entry declaration")));
		if (Invoker.IsValid())
		{
			ASSERT_THAT(IsNear(3.14, Invoker.CallAndReturn<double>(0.0), 0.0001, TEXT("Primitive float test should preserve scientific literals and equality")));
		}
	}

	TEST_METHOD(PrimitiveTypeAuto)
	{
		using namespace AngelscriptNativeTestSupport;

		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };
		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Primitive auto test should create a standalone engine")));

		FScopedNativeModule Module(*TestRunner, Engine, "PrimitiveTypeAuto", ASTEST_AS_ANSI(R"AS(
			namespace A
			{
				class X {}
			}

			int CreateAutoValue()
			{
				auto value = A::X();
				return 1;
			}
			)AS"));
		if (!Module.IsValid())
		{
			return;
		}

		ASSERT_THAT(IsNotNull(GetNativeFunctionByDecl(Module, "int CreateAutoValue()"),
			TEXT("Primitive auto test should expose the named auto function")));
	}

	TEST_METHOD(PrimitiveTypeIntegerWidths)
	{
		using namespace AngelscriptNativeTestSupport;
		using namespace AngelscriptSDKTestSupport;

		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };
		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Primitive integer-width test should create a standalone engine")));

		FScopedNativeModule Module(*TestRunner, Engine, "PrimitiveTypeIntegerWidths", ASTEST_AS_ANSI(R"AS(
			bool CheckIntegerWidths()
			{
				int8 i8 = 127; int16 i16 = 32767; int i32 = 2147483647; int64 i64 = 9223372036854775807;
				uint8 u8 = 255; uint16 u16 = 65535; uint u32 = 4294967295; uint64 u64 = 18446744073709551615;
				int8 i8min = -128; int16 i16min = -32768; int i32min = -2147483647 - 1; int64 i64min = -9223372036854775807 - 1;
				return i8 == 127 && i16 == 32767 && i32 == 2147483647 && i64 == 9223372036854775807
					&& u8 == 255 && u16 == 65535 && u32 == 4294967295 && u64 == 18446744073709551615
					&& i8min == -128 && i16min == -32768 && i32min == -2147483648 && i64min == -9223372036854775807 - 1;
			}
			)AS"));
		if (!Module.IsValid())
		{
			return;
		}

		FSdkFunctionInvoker Invoker(*TestRunner, ScriptEngine, Module, "bool CheckIntegerWidths()");
		ASSERT_THAT(IsTrue(Invoker.IsValid(), TEXT("Primitive integer-width test should resolve its exact entry declaration")));
		if (Invoker.IsValid())
		{
			ASSERT_THAT(IsTrue(Invoker.CallAndReturn<bool>(false), TEXT("Primitive integer-width test should preserve all integer-width extrema")));
		}
	}

	TEST_METHOD(IntegerOverflowWrap)
	{
		using namespace AngelscriptNativeTestSupport;
		using namespace AngelscriptSDKTestSupport;

		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };
		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Integer overflow test should create a standalone engine")));

		FScopedNativeModule Module(*TestRunner, Engine, "PrimitiveTypeIntegerOverflow", ASTEST_AS_ANSI(R"AS(
			bool CheckIntegerOverflow()
			{
				uint8 u8 = 255; u8 += 1; uint16 u16 = 65535; u16 += 1; uint u32 = 4294967295; u32 += 1;
				uint8 u8u = 0; u8u -= 1; uint u32u = 0; u32u -= 1;
				return u8 == 0 && u16 == 0 && u32 == 0 && u8u == 255 && u32u == 4294967295 && uint8(0x1FF) == 255;
			}
			)AS"));
		if (!Module.IsValid())
		{
			return;
		}

		FSdkFunctionInvoker Invoker(*TestRunner, ScriptEngine, Module, "bool CheckIntegerOverflow()");
		ASSERT_THAT(IsTrue(Invoker.IsValid(), TEXT("Integer overflow test should resolve its exact entry declaration")));
		if (Invoker.IsValid())
		{
			ASSERT_THAT(IsTrue(Invoker.CallAndReturn<bool>(false), TEXT("Integer overflow test should preserve modular wraparound and truncation")));
		}
	}
};

#endif
