#include "AngelscriptSDKTestExecutionHelpers.h"
#include "AngelscriptTestAdapter.h"

#include "CQTest.h"
#include "Misc/ScopeExit.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptNativeTestSupport;
using namespace AngelscriptSDKTestSupport;

namespace
{
	void TestValueConstruct0(asIScriptGeneric* Generic)
	{
		int* Value = static_cast<int*>(Generic->GetObject());
		*Value = 0;
	}

	void TestValueConstruct1(asIScriptGeneric* Generic)
	{
		int* Value = static_cast<int*>(Generic->GetObject());
		*Value = *static_cast<int*>(Generic->GetAddressOfArg(0));
	}

	void TestValueCastInt(asIScriptGeneric* Generic)
	{
		int* Value = static_cast<int*>(Generic->GetObject());
		*static_cast<int*>(Generic->GetAddressOfReturnLocation()) = *Value;
	}
}


TEST_CLASS_WITH_FLAGS(FAngelscriptSDKConversionTests,
	"Angelscript.TestModule.AngelScriptSDK.Conversion",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	inline static FNativeTestEngine Engine;

	BEFORE_ALL()
	{
		Engine.Create(*TestRunner);
	}

	AFTER_ALL()
	{
		Engine.Destroy();
	}

	BEFORE_EACH()
	{
		Engine.ResetMessages();
	}
	TEST_METHOD(Numeric)
	{
		asIScriptEngine* ScriptEngine = Engine.Get();
		if (!TestRunner->TestNotNull(TEXT("SDK numeric conversion test should create a standalone engine"), ScriptEngine))
		{
			return;
		}

		FScopedNativeModule Module(*TestRunner, Engine, "SDKConversionNumeric", R"(
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
			FSdkFunctionInvoker Invoker(*TestRunner, ScriptEngine, Module, "int AddSmallAndMedium(int8, uint16)");
			if (!Invoker.IsValid())
			{
				return;
			}
			Invoker.AddArg(static_cast<int8>(2)).AddArg(static_cast<uint16>(4));
			TestRunner->TestEqual(TEXT("SDK numeric conversion test should preserve integer widening"), Invoker.CallAndReturn<int32>(INDEX_NONE), 6);
		}

		{
			FSdkFunctionInvoker Invoker(*TestRunner, ScriptEngine, Module, "float NarrowPrecise(double)");
			if (!Invoker.IsValid())
			{
				return;
			}
			Invoker.AddArg(6.5);
			const float Result = Invoker.CallAndReturn<float>(0.0f);
			TestRunner->TestTrue(TEXT("SDK numeric conversion test should preserve explicit float narrowing"), FMath::IsNearlyEqual(Result, 6.5f, 0.01f));
		}
	}

	TEST_METHOD(ExplicitCast)
	{
		asIScriptEngine* ScriptEngine = Engine.Get();
		if (!TestRunner->TestNotNull(TEXT("SDK explicit-cast conversion test should create a standalone engine"), ScriptEngine))
		{
			return;
		}

		FScopedNativeModule Module(*TestRunner, Engine, "SDKConversionExplicit", R"(
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
			FSdkFunctionInvoker Invoker(*TestRunner, ScriptEngine, Module, "int TruncateDouble(double)");
			if (!Invoker.IsValid())
			{
				return;
			}
			Invoker.AddArg(3.75);
			TestRunner->TestEqual(TEXT("SDK explicit-cast conversion test should truncate double to int"), Invoker.CallAndReturn<int32>(INDEX_NONE), 3);
		}

		{
			FSdkFunctionInvoker Invoker(*TestRunner, ScriptEngine, Module, "uint64 WidenToUint64(int)");
			if (!Invoker.IsValid())
			{
				return;
			}
			Invoker.AddArg(static_cast<int32>(3));
			TestRunner->TestEqual(TEXT("SDK explicit-cast conversion test should widen int to uint64"), Invoker.CallAndReturn<uint64>(0), static_cast<uint64>(3));
		}

		{
			FSdkFunctionInvoker Invoker(*TestRunner, ScriptEngine, Module, "float AddFloatQuarter(uint64)");
			if (!Invoker.IsValid())
			{
				return;
			}
			Invoker.AddArg(static_cast<uint64>(3));
			const float Result = Invoker.CallAndReturn<float>(0.0f);
			TestRunner->TestTrue(TEXT("SDK explicit-cast conversion test should cast uint64 through float"), FMath::IsNearlyEqual(Result, 3.25f, 0.01f));
		}
	}

	TEST_METHOD(ImplicitValueType)
	{
		asIScriptEngine* ScriptEngine = Engine.Get();
		if (!TestRunner->TestNotNull(TEXT("SDK implicit value-type conversion test should create a standalone engine"), ScriptEngine))
		{
			return;
		}

		FScopedNativeModule Module(*TestRunner, Engine, "SDKConversionImplicitValueType", R"(
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

		TestRunner->TestNotNull(
			TEXT("SDK implicit value-type conversion test should expose the named conversion function"),
			GetNativeFunctionByDecl(Module, "int ConvertTestToInt()"));
	}

	TEST_METHOD(NumericBoundary)
	{
		asIScriptEngine* ScriptEngine = Engine.Get();
		if (!TestRunner->TestNotNull(TEXT("SDK numeric-boundary conversion test should create a standalone engine"), ScriptEngine))
		{
			return;
		}

		FScopedNativeModule Module(*TestRunner, Engine, "SDKConversionNumericBoundary", R"(
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
			FSdkFunctionInvoker Invoker(*TestRunner, ScriptEngine, Module, "int Truncate(double)");
			if (!Invoker.IsValid())
			{
				return;
			}
			Invoker.AddArg(3.9);
			TestRunner->TestEqual(TEXT("SDK numeric-boundary conversion test should truncate positive floats toward zero"), Invoker.CallAndReturn<int32>(INDEX_NONE), 3);
		}

		{
			FSdkFunctionInvoker Invoker(*TestRunner, ScriptEngine, Module, "int Truncate(double)");
			if (!Invoker.IsValid())
			{
				return;
			}
			Invoker.AddArg(-3.9);
			TestRunner->TestEqual(TEXT("SDK numeric-boundary conversion test should truncate negative floats toward zero"), Invoker.CallAndReturn<int32>(INDEX_NONE), -3);
		}

		{
			FSdkFunctionInvoker Invoker(*TestRunner, ScriptEngine, Module, "int RoundTripSmallInt(int)");
			if (!Invoker.IsValid())
			{
				return;
			}
			Invoker.AddArg(static_cast<int32>(12345));
			TestRunner->TestEqual(TEXT("SDK numeric-boundary conversion test should round-trip small integers through float"), Invoker.CallAndReturn<int32>(INDEX_NONE), 12345);
		}

		{
			FSdkFunctionInvoker Invoker(*TestRunner, ScriptEngine, Module, "uint SignedToUnsigned(int)");
			if (!Invoker.IsValid())
			{
				return;
			}
			Invoker.AddArg(static_cast<int32>(-1));
			TestRunner->TestEqual(TEXT("SDK numeric-boundary conversion test should reinterpret signed values as unsigned"), Invoker.CallAndReturn<uint32>(0), static_cast<uint32>(4294967295u));
		}

		{
			FSdkFunctionInvoker Invoker(*TestRunner, ScriptEngine, Module, "int64 WidenInt(int)");
			if (!Invoker.IsValid())
			{
				return;
			}
			Invoker.AddArg(static_cast<int32>(2147483647));
			TestRunner->TestEqual(TEXT("SDK numeric-boundary conversion test should widen without int overflow"), Invoker.CallAndReturn<int64>(0), static_cast<int64>(2147483648));
		}
	}

	TEST_METHOD(BoolConversion)
	{
		asIScriptEngine* ScriptEngine = Engine.Get();
		if (!TestRunner->TestNotNull(TEXT("SDK bool-conversion test should create a standalone engine"), ScriptEngine))
		{
			return;
		}

		FScopedNativeModule Module(*TestRunner, Engine, "SDKConversionBool", R"(
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
			FSdkFunctionInvoker Invoker(*TestRunner, ScriptEngine, Module, "bool IsNonZero(int)");
			if (!Invoker.IsValid())
			{
				return;
			}
			Invoker.AddArg(static_cast<int32>(5));
			TestRunner->TestTrue(TEXT("SDK bool-conversion test should treat non-zero comparisons as true"), Invoker.CallAndReturn<bool>(false));
		}

		{
			FSdkFunctionInvoker Invoker(*TestRunner, ScriptEngine, Module, "bool IsNonZero(int)");
			if (!Invoker.IsValid())
			{
				return;
			}
			Invoker.AddArg(static_cast<int32>(0));
			TestRunner->TestFalse(TEXT("SDK bool-conversion test should treat zero comparisons as false"), Invoker.CallAndReturn<bool>(true));
		}

		{
			FSdkFunctionInvoker Invoker(*TestRunner, ScriptEngine, Module, "int BoolToInt(bool)");
			if (!Invoker.IsValid())
			{
				return;
			}
			Invoker.AddArg(true);
			TestRunner->TestEqual(TEXT("SDK bool-conversion test should convert true through ternary"), Invoker.CallAndReturn<int32>(INDEX_NONE), 1);
		}

		{
			FSdkFunctionInvoker Invoker(*TestRunner, ScriptEngine, Module, "int BoolToInt(bool)");
			if (!Invoker.IsValid())
			{
				return;
			}
			Invoker.AddArg(false);
			TestRunner->TestEqual(TEXT("SDK bool-conversion test should convert false through ternary"), Invoker.CallAndReturn<int32>(INDEX_NONE), 0);
		}
	}
};

#endif
