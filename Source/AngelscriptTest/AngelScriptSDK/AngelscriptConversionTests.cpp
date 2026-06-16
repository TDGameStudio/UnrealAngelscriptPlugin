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
	TEST_METHOD(Numeric)
	{
		FNativeMessageCollector Messages;
		asIScriptEngine* ScriptEngine = CreateNativeEngine(&Messages);
		if (!TestRunner->TestNotNull(TEXT("SDK numeric conversion test should create a standalone engine"), ScriptEngine))
		{
			return;
		}

		ON_SCOPE_EXIT
		{
			DestroyNativeEngine(ScriptEngine);
		};

		asIScriptModule* Module = BuildNativeModule(ScriptEngine, "SDKConversionNumeric", R"(
bool Entry()
{
	int8 small = 2;
	uint16 medium = 4;
	int total = small + medium;
	double precise = total + 0.5;
	float narrow = float(precise);
	return total == 6 && narrow > 6.49f && narrow < 6.51f;
}
)");
		if (!TestRunner->TestNotNull(TEXT("SDK numeric conversion test should compile the module"), Module))
		{
			TestRunner->AddInfo(CollectMessages(Messages));
			return;
		}

		bool bResult = false;
		if (!ExecuteScriptFunction(*TestRunner, ScriptEngine, Module, "bool Entry()", bResult))
		{
			return;
		}

		TestRunner->TestTrue(TEXT("SDK numeric conversion test should preserve widening and narrowing conversions"), bResult);
	}

	TEST_METHOD(ExplicitCast)
	{
		FNativeMessageCollector Messages;
		asIScriptEngine* ScriptEngine = CreateNativeEngine(&Messages);
		if (!TestRunner->TestNotNull(TEXT("SDK explicit-cast conversion test should create a standalone engine"), ScriptEngine))
		{
			return;
		}

		ON_SCOPE_EXIT
		{
			DestroyNativeEngine(ScriptEngine);
		};

		asIScriptModule* Module = BuildNativeModule(ScriptEngine, "SDKConversionExplicit", R"(
bool Entry()
{
	double d = 3.75;
	int i = int(d);
	uint64 wide = uint64(i);
	float f = float(wide) + 0.25f;
	return i == 3 && wide == 3 && f > 3.24f && f < 3.26f;
}
)");
		if (!TestRunner->TestNotNull(TEXT("SDK explicit-cast conversion test should compile the module"), Module))
		{
			TestRunner->AddInfo(CollectMessages(Messages));
			return;
		}

		bool bResult = false;
		if (!ExecuteScriptFunction(*TestRunner, ScriptEngine, Module, "bool Entry()", bResult))
		{
			return;
		}

		TestRunner->TestTrue(TEXT("SDK explicit-cast conversion test should preserve explicit cast semantics"), bResult);
	}

	TEST_METHOD(ImplicitValueType)
	{
		FNativeMessageCollector Messages;
		asIScriptEngine* ScriptEngine = CreateNativeEngine(&Messages);
		if (!TestRunner->TestNotNull(TEXT("SDK implicit value-type conversion test should create a standalone engine"), ScriptEngine))
		{
			return;
		}

		ON_SCOPE_EXIT
		{
			DestroyNativeEngine(ScriptEngine);
		};

		asIScriptModule* Module = BuildNativeModule(ScriptEngine, "SDKConversionImplicitValueType", R"(
class Test
{
	int opImplConv() const
	{
		return 7;
	}
}

bool Entry()
{
	Test value;
	int i = value;
	return i == 7;
}
)");
		if (!TestRunner->TestNotNull(TEXT("SDK implicit value-type conversion test should compile the module"), Module))
		{
			TestRunner->AddInfo(CollectMessages(Messages));
			return;
		}

		TestRunner->TestNotNull(TEXT("SDK implicit value-type conversion test should expose the compiled entry function"), GetNativeFunctionByDecl(Module, "bool Entry()"));
	}

	TEST_METHOD(NumericBoundary)
	{
		FNativeMessageCollector Messages;
		asIScriptEngine* ScriptEngine = CreateNativeEngine(&Messages);
		if (!TestRunner->TestNotNull(TEXT("SDK numeric-boundary conversion test should create a standalone engine"), ScriptEngine))
		{
			return;
		}

		ON_SCOPE_EXIT
		{
			DestroyNativeEngine(ScriptEngine);
		};

		asIScriptModule* Module = BuildNativeModule(ScriptEngine, "SDKConversionNumericBoundary", R"(
bool Entry()
{
	// float -> int truncates toward zero (not rounds)
	int truncPos = int(3.9);     // 3
	int truncNeg = int(-3.9);    // -3

	// int -> float -> int round-trip for small magnitudes is exact
	int roundTrip = int(float(12345));

	// Signed -> unsigned reinterprets the bit pattern
	uint asUnsigned = uint(-1);  // 0xFFFFFFFF = 4294967295

	// Widening preserves value
	int64 widened = int64(2147483647) + 1;  // 2147483648, no overflow at 64-bit

	return truncPos == 3 && truncNeg == -3 && roundTrip == 12345
		&& asUnsigned == 4294967295 && widened == 2147483648;
}
)");
		if (!TestRunner->TestNotNull(TEXT("SDK numeric-boundary conversion test should compile the module"), Module))
		{
			TestRunner->AddInfo(CollectMessages(Messages));
			return;
		}

		bool bResult = false;
		if (!ExecuteScriptFunction(*TestRunner, ScriptEngine, Module, "bool Entry()", bResult))
		{
			return;
		}

		TestRunner->TestTrue(TEXT("SDK numeric-boundary conversion test should preserve truncation, round-trip, and sign-reinterpret semantics"), bResult);
	}

	TEST_METHOD(BoolConversion)
	{
		FNativeMessageCollector Messages;
		asIScriptEngine* ScriptEngine = CreateNativeEngine(&Messages);
		if (!TestRunner->TestNotNull(TEXT("SDK bool-conversion test should create a standalone engine"), ScriptEngine))
		{
			return;
		}

		ON_SCOPE_EXIT
		{
			DestroyNativeEngine(ScriptEngine);
		};

		asIScriptModule* Module = BuildNativeModule(ScriptEngine, "SDKConversionBool", R"(
bool Entry()
{
	// Explicit numeric -> bool via comparison (AngelScript has no implicit int->bool)
	bool fromNonZero = (5 != 0);
	bool fromZero = (0 != 0);

	// bool participates in ternary producing ints
	int t = fromNonZero ? 1 : 0;
	int f = fromZero ? 1 : 0;

	return fromNonZero && !fromZero && t == 1 && f == 0;
}
)");
		if (!TestRunner->TestNotNull(TEXT("SDK bool-conversion test should compile the module"), Module))
		{
			TestRunner->AddInfo(CollectMessages(Messages));
			return;
		}

		bool bResult = false;
		if (!ExecuteScriptFunction(*TestRunner, ScriptEngine, Module, "bool Entry()", bResult))
		{
			return;
		}

		TestRunner->TestTrue(TEXT("SDK bool-conversion test should preserve boolean truth semantics"), bResult);
	}
};

#endif
