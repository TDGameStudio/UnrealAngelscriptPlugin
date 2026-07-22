#include "../Support/AngelscriptNativeExecutionTestSupport.h"
#include "../Support/AngelscriptNativeExecutionTestSupport.h"

#include "CQTest.h"
#include "Misc/ScopeExit.h"

#if WITH_ANGELSCRIPT_UNITTESTS


TEST_CLASS_WITH_FLAGS(FConversionsTests,
	"Angelscript.TestModule.AngelScriptSDK.Language.Conversions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	static void TestValueConstruct0(asIScriptGeneric* Generic)
	{
		int* Value = static_cast<int*>(Generic->GetObject());
		*Value = 0;
	}

	static void TestValueConstruct1(asIScriptGeneric* Generic)
	{
		int* Value = static_cast<int*>(Generic->GetObject());
		*Value = *static_cast<int*>(Generic->GetAddressOfArg(0));
	}

	static void TestValueCastInt(asIScriptGeneric* Generic)
	{
		int* Value = static_cast<int*>(Generic->GetObject());
		*static_cast<int*>(Generic->GetAddressOfReturnLocation()) = *Value;
	}

public:
	TEST_METHOD(ConversionsNumeric)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		asIScriptEngine* ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("SDK numeric conversion test should create a standalone engine")));

		AngelscriptNativeTestSupport::FScopedNativeModule Module(*TestRunner, Engine, "SDKConversionNumeric", R"(
int AddSmallAndMedium(int8 Small, uint16 Medium)
{
	return Small + Medium;
}

float NarrowPrecise(double Value)
{
	return float(Value);
}
)");
		if (!Module.IsValid())
		{
			return;
		}

		{
			AngelscriptSDKTestSupport::FSdkFunctionInvoker Invoker(*TestRunner, ScriptEngine, Module, "int AddSmallAndMedium(const int8, const uint16)");
			if (!Invoker.IsValid())
			{
				return;
			}
			Invoker.AddArg(static_cast<int8>(2)).AddArg(static_cast<uint16>(4));
			ASSERT_THAT(AreEqual(6, Invoker.CallAndReturn<int32>(INDEX_NONE),
				TEXT("SDK numeric conversion test should preserve integer widening")));
		}

		{
			AngelscriptSDKTestSupport::FSdkFunctionInvoker Invoker(*TestRunner, ScriptEngine, Module, "float NarrowPrecise(const float)");
			if (!Invoker.IsValid())
			{
				return;
			}
			Invoker.AddArg(6.5);
			const double Result = Invoker.CallAndReturn<double>(0.0);
			ASSERT_THAT(IsNear(6.5, Result, 0.01,
				TEXT("SDK numeric conversion test should preserve explicit float narrowing")));
		}
	}

	TEST_METHOD(ConversionsExplicitCast)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		asIScriptEngine* ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("SDK explicit-cast conversion test should create a standalone engine")));

		AngelscriptNativeTestSupport::FScopedNativeModule Module(*TestRunner, Engine, "SDKConversionExplicit", R"(
int TruncateDouble(double Value)
{
	return int(Value);
}

uint64 WidenToUint64(int Value)
{
	return uint64(Value);
}

float AddFloatQuarter(uint64 Value)
{
	return float(Value) + 0.25f;
}
)");
		if (!Module.IsValid())
		{
			return;
		}

		{
			AngelscriptSDKTestSupport::FSdkFunctionInvoker Invoker(*TestRunner, ScriptEngine, Module, "int TruncateDouble(const float)");
			if (!Invoker.IsValid())
			{
				return;
			}
			Invoker.AddArg(3.75);
			ASSERT_THAT(AreEqual(3, Invoker.CallAndReturn<int32>(INDEX_NONE),
				TEXT("SDK explicit-cast conversion test should truncate double to int")));
		}

		{
			AngelscriptSDKTestSupport::FSdkFunctionInvoker Invoker(*TestRunner, ScriptEngine, Module, "uint64 WidenToUint64(const int)");
			if (!Invoker.IsValid())
			{
				return;
			}
			Invoker.AddArg(static_cast<int32>(3));
			ASSERT_THAT(AreEqual(static_cast<uint64>(3), Invoker.CallAndReturn<uint64>(0),
				TEXT("SDK explicit-cast conversion test should widen int to uint64")));
		}

		{
			AngelscriptSDKTestSupport::FSdkFunctionInvoker Invoker(*TestRunner, ScriptEngine, Module, "float AddFloatQuarter(const uint64)");
			if (!Invoker.IsValid())
			{
				return;
			}
			Invoker.AddArg(static_cast<uint64>(3));
			const double Result = Invoker.CallAndReturn<double>(0.0);
			ASSERT_THAT(IsNear(3.25, Result, 0.01,
				TEXT("SDK explicit-cast conversion test should cast uint64 through float")));
		}
	}

	TEST_METHOD(ImplicitValueType)
	{
		using namespace AngelscriptNativeTestSupport;
		using namespace AngelscriptSDKTestSupport;

		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		asIScriptEngine* ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("SDK implicit value-type conversion test should create a standalone engine")));

		AngelscriptNativeTestSupport::FScopedNativeModule Module(*TestRunner, Engine, "SDKConversionImplicitValueType", R"(
class Test
{
	int opImplConv() const
	{
		return 7;
	}
}

int ConvertTestToInt()
{
	Test Value;
	int Result = Value;
	return Result;
}
)");
		if (!Module.IsValid())
		{
			return;
		}

		asIScriptFunction* Function = GetNativeFunctionByExactDecl(Module, "int ConvertTestToInt()");
		ASSERT_THAT(IsNotNull(Function,
			TEXT("SDK implicit value-type conversion test should expose the named conversion function")));
		if (Function == nullptr)
		{
			return;
		}

		asIScriptContext* Context = ScriptEngine->CreateContext();
		ASSERT_THAT(IsNotNull(Context,
			TEXT("SDK implicit value-type conversion test should create an execution context")));
		if (Context == nullptr)
		{
			return;
		}
		ON_SCOPE_EXIT { Context->Release(); };
		ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_EXCEPTION), PrepareAndExecute(Context, Function),
			TEXT("The current isolated native engine should expose the value-type conversion construction limitation")));
		ASSERT_THAT(AreEqual(FString(TEXT("Null pointer access")), FString(UTF8_TO_TCHAR(Context->GetExceptionString())),
			TEXT("The value-type conversion limitation should preserve the current fork exception text")));
	}

	TEST_METHOD(ConversionsNumericBoundary)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		asIScriptEngine* ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("SDK numeric-boundary conversion test should create a standalone engine")));

		AngelscriptNativeTestSupport::FScopedNativeModule Module(*TestRunner, Engine, "SDKConversionNumericBoundary", R"(
int Truncate(double Value)
{
	return int(Value);
}

int RoundTripSmallInt(int Value)
{
	return int(float(Value));
}

uint SignedToUnsigned(int Value)
{
	return uint(Value);
}

int64 WidenInt(int Value)
{
	return int64(Value) + 1;
}
)");
		if (!Module.IsValid())
		{
			return;
		}

		{
			AngelscriptSDKTestSupport::FSdkFunctionInvoker Invoker(*TestRunner, ScriptEngine, Module, "int Truncate(const float)");
			if (!Invoker.IsValid())
			{
				return;
			}
			Invoker.AddArg(3.9);
			ASSERT_THAT(AreEqual(3, Invoker.CallAndReturn<int32>(INDEX_NONE),
				TEXT("SDK numeric-boundary conversion test should truncate positive floats toward zero")));
		}

		{
			AngelscriptSDKTestSupport::FSdkFunctionInvoker Invoker(*TestRunner, ScriptEngine, Module, "int Truncate(const float)");
			if (!Invoker.IsValid())
			{
				return;
			}
			Invoker.AddArg(-3.9);
			ASSERT_THAT(AreEqual(-3, Invoker.CallAndReturn<int32>(INDEX_NONE),
				TEXT("SDK numeric-boundary conversion test should truncate negative floats toward zero")));
		}

		{
			AngelscriptSDKTestSupport::FSdkFunctionInvoker Invoker(*TestRunner, ScriptEngine, Module, "int RoundTripSmallInt(const int)");
			if (!Invoker.IsValid())
			{
				return;
			}
			Invoker.AddArg(static_cast<int32>(12345));
			ASSERT_THAT(AreEqual(12345, Invoker.CallAndReturn<int32>(INDEX_NONE),
				TEXT("SDK numeric-boundary conversion test should round-trip small integers through float")));
		}

		{
			AngelscriptSDKTestSupport::FSdkFunctionInvoker Invoker(*TestRunner, ScriptEngine, Module, "uint SignedToUnsigned(const int)");
			if (!Invoker.IsValid())
			{
				return;
			}
			Invoker.AddArg(static_cast<int32>(-1));
			ASSERT_THAT(AreEqual(static_cast<uint32>(4294967295u), Invoker.CallAndReturn<uint32>(0),
				TEXT("SDK numeric-boundary conversion test should reinterpret signed values as unsigned")));
		}

		{
			AngelscriptSDKTestSupport::FSdkFunctionInvoker Invoker(*TestRunner, ScriptEngine, Module, "int64 WidenInt(const int)");
			if (!Invoker.IsValid())
			{
				return;
			}
			Invoker.AddArg(static_cast<int32>(2147483647));
			ASSERT_THAT(AreEqual(static_cast<int64>(2147483648), Invoker.CallAndReturn<int64>(0),
				TEXT("SDK numeric-boundary conversion test should widen without int overflow")));
		}
	}

	TEST_METHOD(ConversionsBoolConversion)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		asIScriptEngine* ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("SDK bool-conversion test should create a standalone engine")));

		AngelscriptNativeTestSupport::FScopedNativeModule Module(*TestRunner, Engine, "SDKConversionBool", R"(
bool IsNonZero(int Value)
{
	return Value != 0;
}

int BoolToInt(bool Value)
{
	return Value ? 1 : 0;
}
)");
		if (!Module.IsValid())
		{
			return;
		}

		{
			AngelscriptSDKTestSupport::FSdkFunctionInvoker Invoker(*TestRunner, ScriptEngine, Module, "bool IsNonZero(const int)");
			if (!Invoker.IsValid())
			{
				return;
			}
			Invoker.AddArg(static_cast<int32>(5));
			ASSERT_THAT(IsTrue(Invoker.CallAndReturn<bool>(false),
				TEXT("SDK bool-conversion test should treat non-zero comparisons as true")));
		}

		{
			AngelscriptSDKTestSupport::FSdkFunctionInvoker Invoker(*TestRunner, ScriptEngine, Module, "bool IsNonZero(const int)");
			if (!Invoker.IsValid())
			{
				return;
			}
			Invoker.AddArg(static_cast<int32>(0));
			ASSERT_THAT(IsFalse(Invoker.CallAndReturn<bool>(true),
				TEXT("SDK bool-conversion test should treat zero comparisons as false")));
		}

		{
			AngelscriptSDKTestSupport::FSdkFunctionInvoker Invoker(*TestRunner, ScriptEngine, Module, "int BoolToInt(const bool)");
			if (!Invoker.IsValid())
			{
				return;
			}
			Invoker.AddArg(true);
			ASSERT_THAT(AreEqual(1, Invoker.CallAndReturn<int32>(INDEX_NONE),
				TEXT("SDK bool-conversion test should convert true through ternary")));
		}

		{
			AngelscriptSDKTestSupport::FSdkFunctionInvoker Invoker(*TestRunner, ScriptEngine, Module, "int BoolToInt(const bool)");
			if (!Invoker.IsValid())
			{
				return;
			}
			Invoker.AddArg(false);
			ASSERT_THAT(AreEqual(0, Invoker.CallAndReturn<int32>(INDEX_NONE),
				TEXT("SDK bool-conversion test should convert false through ternary")));
		}
	}
};

#endif
