#include "AngelscriptSDKTestUtilities.h"
#include "AngelscriptTestAdapter.h"

#include "CQTest.h"
#include "Misc/ScopeExit.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptNativeTestSupport;
using namespace AngelscriptSDKTestSupport;
using namespace AngelscriptSDKTestUtilities;

namespace
{
	int32 GEnumValue = 0;
	asBYTE GInt8Value = 0;

	asBYTE RetInt8(asBYTE InValue)
	{
		return InValue;
	}

	void CaptureEnum(asIScriptGeneric* Generic)
	{
		if (Generic != nullptr)
		{
			GEnumValue = static_cast<int32>(Generic->GetArgDWord(0));
		}
	}
}


TEST_CLASS_WITH_FLAGS(FAngelscriptSDKTypeTests, "Angelscript.TestModule.AngelScriptSDK.Type", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(Bool)
	{
		
		FNativeMessageCollector Messages;
		asIScriptEngine* ScriptEngine = CreateNativeEngine(&Messages);
		if (!TestRunner->TestNotNull(TEXT("SDK bool type test should create a standalone engine"), ScriptEngine))
		{
			return;
		}

		ON_SCOPE_EXIT
		{
			DestroyNativeEngine(ScriptEngine);
		};

		asIScriptModule* Module = BuildNativeModule(
			ScriptEngine,
			"SDKTypeBool",
			"bool Entry() { bool a = true; bool b = false; return a && !b && (a ^^ b); }");
		if (!TestRunner->TestNotNull(TEXT("SDK bool type test should compile the module"), Module))
		{
			TestRunner->AddInfo(CollectMessages(Messages));
			return;
		}

		bool bResult = false;
		if (!ExecuteScriptBoolFunction(*TestRunner, ScriptEngine, Module, "bool Entry()", bResult))
		{
			return;
		}

		TestRunner->TestTrue(TEXT("SDK bool type test should preserve basic boolean logic"), bResult);
	}

	TEST_METHOD(Bits)
	{
		
		FNativeMessageCollector Messages;
		asIScriptEngine* ScriptEngine = CreateNativeEngine(&Messages);
		if (!TestRunner->TestNotNull(TEXT("SDK bits type test should create a standalone engine"), ScriptEngine))
		{
			return;
		}

		ON_SCOPE_EXIT
		{
			DestroyNativeEngine(ScriptEngine);
		};

		asIScriptModule* Module = BuildNativeModule(ScriptEngine, "SDKTypeBits", R"(
bool Entry()
{
	uint oct = 0o777;
	uint bin = 0b10101010;
	uint dec = 0d255;
	uint8 newmask = 0xFF;
	uint8 mask2 = 1 << 2;
	uint8 mask3 = 1 << 3;
	uint8 mask5 = 1 << 5;
	newmask = newmask & (~mask2) & (~mask3) & (~mask5);
	return oct == 0x1FF && bin == 0xAA && dec == 0xFF && newmask == 0xD3;
}
)");
		if (!TestRunner->TestNotNull(TEXT("SDK bits type test should compile the module"), Module))
		{
			TestRunner->AddInfo(CollectMessages(Messages));
			return;
		}

		bool bResult = false;
		if (!ExecuteScriptBoolFunction(*TestRunner, ScriptEngine, Module, "bool Entry()", bResult))
		{
			return;
		}

		TestRunner->TestTrue(TEXT("SDK bits type test should preserve numeric literals and bitwise masks"), bResult);
	}

	TEST_METHOD(Int8)
	{
		
		FNativeMessageCollector Messages;
		asIScriptEngine* ScriptEngine = CreateNativeEngine(&Messages);
		if (!TestRunner->TestNotNull(TEXT("SDK int8 type test should create a standalone engine"), ScriptEngine))
		{
			return;
		}

		ON_SCOPE_EXIT
		{
			DestroyNativeEngine(ScriptEngine);
		};

		const ASAutoCaller::FunctionCaller Caller = ASAutoCaller::MakeFunctionCaller(RetInt8);
		const int RegisterFunctionResult = ScriptEngine->RegisterGlobalFunction("int8 RetInt8(int8 value)", asFUNCTION(RetInt8), asCALL_CDECL, *(asFunctionCaller*)&Caller);
		if (!TestRunner->TestTrue(TEXT("SDK int8 type test should register the native int8 callback"), RegisterFunctionResult >= 0))
		{
			return;
		}

		const int RegisterPropertyResult = ScriptEngine->RegisterGlobalProperty("int8 gvar", &GInt8Value);
		if (!TestRunner->TestTrue(TEXT("SDK int8 type test should register the int8 global property"), RegisterPropertyResult >= 0))
		{
			return;
		}

		asIScriptModule* Module = BuildNativeModule(ScriptEngine, "SDKTypeInt8", "int Entry() { gvar = RetInt8(1); return gvar; }");
		if (!TestRunner->TestNotNull(TEXT("SDK int8 type test should compile the module"), Module))
		{
			TestRunner->AddInfo(CollectMessages(Messages));
			return;
		}

		int32 Result = 0;
		if (!ExecuteScriptIntFunction(*TestRunner, ScriptEngine, Module, "int Entry()", Result))
		{
			return;
		}

		TestRunner->TestEqual(TEXT("SDK int8 type test should preserve the int8 return through the global property"), Result, 1);
	}

	TEST_METHOD(Float)
	{
		
		FNativeMessageCollector Messages;
		asIScriptEngine* ScriptEngine = CreateNativeEngine(&Messages);
		if (!TestRunner->TestNotNull(TEXT("SDK float type test should create a standalone engine"), ScriptEngine))
		{
			return;
		}

		ON_SCOPE_EXIT
		{
			DestroyNativeEngine(ScriptEngine);
		};

		const bool bFloatUsesFloat64 = ScriptEngine->GetEngineProperty(asEP_FLOAT_IS_FLOAT64) != 0;
		const char* Source = bFloatUsesFloat64
			? "double Entry() { double a = 1e5; double b = 1.0e5; return (a == b) ? 3.14 : 0.0; }"
			: "double Entry() { float a = 1e5; float b = 1.0e5; return (a == b) ? 3.14f : 0.0f; }";

		asIScriptModule* Module = BuildNativeModule(ScriptEngine, "SDKTypeFloat", Source);
		if (!TestRunner->TestNotNull(TEXT("SDK float type test should compile the module"), Module))
		{
			TestRunner->AddInfo(CollectMessages(Messages));
			return;
		}

		double Result = 0.0;
		if (!ExecuteScriptDoubleFunction(*TestRunner, ScriptEngine, Module, "double Entry()", Result))
		{
			return;
		}

		TestRunner->TestTrue(TEXT("SDK float type test should preserve scientific literals and floating equality"), FMath::IsNearlyEqual(Result, 3.14, 0.0001));
	}

	TEST_METHOD(TypedefBytecode)
	{
		
		FNativeMessageCollector Messages;
		asIScriptEngine* SaveEngine = CreateNativeEngine(&Messages);
		if (!TestRunner->TestNotNull(TEXT("SDK typedef bytecode test should create the save engine"), SaveEngine))
		{
			return;
		}

		ON_SCOPE_EXIT
		{
			DestroyNativeEngine(SaveEngine);
		};

		if (!TestRunner->TestTrue(TEXT("SDK typedef bytecode test should register TestType1 on the save engine"), SaveEngine->RegisterTypedef("TestType1", "int8") >= 0))
		{
			return;
		}

		if (!TestRunner->TestTrue(TEXT("SDK typedef bytecode test should register TestType4 on the save engine"), SaveEngine->RegisterTypedef("TestType4", "int64") >= 0))
		{
			return;
		}

		asIScriptModule* SaveModule = BuildNativeModule(SaveEngine, "SDKTypeTypedefSave", R"(
TestType4 Func(TestType1 a)
{
	return a;
}

int Entry()
{
	TestType1 v = 1;
	return int(Func(v));
}
)");
		if (!TestRunner->TestNotNull(TEXT("SDK typedef bytecode test should compile the save module"), SaveModule))
		{
			TestRunner->AddInfo(CollectMessages(Messages));
			return;
		}

		FSDKBytecodeStream Bytecode;
		if (!TestRunner->TestEqual(TEXT("SDK typedef bytecode test should save bytecode successfully"), SaveModule->SaveByteCode(&Bytecode), static_cast<int32>(asSUCCESS)))
		{
			return;
		}

		Bytecode.Restart();

		FNativeMessageCollector LoadMessages;
		asIScriptEngine* LoadEngine = CreateNativeEngine(&LoadMessages);
		if (!TestRunner->TestNotNull(TEXT("SDK typedef bytecode test should create the load engine"), LoadEngine))
		{
			return;
		}

		ON_SCOPE_EXIT
		{
			DestroyNativeEngine(LoadEngine);
		};

		if (!TestRunner->TestTrue(TEXT("SDK typedef bytecode test should register TestType1 on the load engine"), LoadEngine->RegisterTypedef("TestType1", "int8") >= 0))
		{
			return;
		}

		if (!TestRunner->TestTrue(TEXT("SDK typedef bytecode test should register TestType4 on the load engine"), LoadEngine->RegisterTypedef("TestType4", "int64") >= 0))
		{
			return;
		}

		asIScriptModule* LoadModule = LoadEngine->GetModule("SDKTypeTypedefLoad", asGM_ALWAYS_CREATE);
		if (!TestRunner->TestNotNull(TEXT("SDK typedef bytecode test should create the load module"), LoadModule))
		{
			return;
		}

		if (!TestRunner->TestEqual(TEXT("SDK typedef bytecode test should load bytecode successfully"), LoadModule->LoadByteCode(&Bytecode), static_cast<int32>(asSUCCESS)))
		{
			return;
		}

		TestRunner->TestNotNull(TEXT("SDK typedef bytecode test should preserve the loaded entry function"), GetNativeFunctionByDecl(LoadModule, "int Entry()"));
	}

	TEST_METHOD(Enum)
	{
		
		FNativeMessageCollector Messages;
		asIScriptEngine* ScriptEngine = CreateNativeEngine(&Messages);
		if (!TestRunner->TestNotNull(TEXT("SDK enum type test should create a standalone engine"), ScriptEngine))
		{
			return;
		}

		ON_SCOPE_EXIT
		{
			DestroyNativeEngine(ScriptEngine);
		};

		if (!TestRunner->TestTrue(TEXT("SDK enum type test should register the first enum namespace"), ScriptEngine->RegisterEnum("myenum") >= 0))
		{
			return;
		}

		if (!TestRunner->TestTrue(TEXT("SDK enum type test should register the first enum value"), ScriptEngine->RegisterEnumValue("myenum", "value", 1) >= 0))
		{
			return;
		}

		ScriptEngine->SetDefaultNamespace("foo");
		if (!TestRunner->TestTrue(TEXT("SDK enum type test should register a namespaced enum with the same name"), ScriptEngine->RegisterEnum("myenum") >= 0))
		{
			ScriptEngine->SetDefaultNamespace("");
			return;
		}

		if (!TestRunner->TestTrue(TEXT("SDK enum type test should register the namespaced enum value"), ScriptEngine->RegisterEnumValue("myenum", "value", 1) >= 0))
		{
			ScriptEngine->SetDefaultNamespace("");
			return;
		}
		ScriptEngine->SetDefaultNamespace("");

		if (!TestRunner->TestTrue(TEXT("SDK enum type test should register TEST_ENUM"), ScriptEngine->RegisterEnum("TEST_ENUM") >= 0))
		{
			return;
		}

		if (!TestRunner->TestTrue(TEXT("SDK enum type test should register ENUM1"), ScriptEngine->RegisterEnumValue("TEST_ENUM", "ENUM1", 1) >= 0))
		{
			return;
		}

		asIScriptModule* Module = BuildNativeModule(ScriptEngine, "SDKTypeEnum", R"(
enum LocalEnum
{
	LocalValue = 1
}

bool Entry()
{
	LocalEnum Value = LocalEnum::LocalValue;
	return Value == LocalEnum::LocalValue;
}
)");
		if (!TestRunner->TestNotNull(TEXT("SDK enum type test should compile the enum entry module"), Module))
		{
			TestRunner->AddInfo(CollectMessages(Messages));
			return;
		}

		bool bResult = false;
		if (!ExecuteScriptBoolFunction(*TestRunner, ScriptEngine, Module, "bool Entry()", bResult))
		{
			return;
		}

		if (!TestRunner->TestTrue(TEXT("SDK enum type test should preserve local enum equality"), bResult))
		{
			return;
		}

		TestRunner->TestEqual(TEXT("SDK enum type test should keep the registered enum declaration accessible"), FString(UTF8_TO_TCHAR(ScriptEngine->GetTypeDeclaration(ScriptEngine->GetTypeIdByDecl("TEST_ENUM")))), FString(TEXT("TEST_ENUM")));
	}

	TEST_METHOD(Auto)
	{
		
		FNativeMessageCollector Messages;
		asIScriptEngine* ScriptEngine = CreateNativeEngine(&Messages);
		if (!TestRunner->TestNotNull(TEXT("SDK auto type test should create a standalone engine"), ScriptEngine))
		{
			return;
		}

		ON_SCOPE_EXIT
		{
			DestroyNativeEngine(ScriptEngine);
		};

		asIScriptModule* Module = BuildNativeModule(ScriptEngine, "SDKTypeAuto", R"(
namespace A
{
	class X
	{
		X()
		{
		}
	}
}

bool Entry()
{
	auto value = A::X();
	return true;
}
)");
		if (!TestRunner->TestNotNull(TEXT("SDK auto type test should compile the module"), Module))
		{
			TestRunner->AddInfo(CollectMessages(Messages));
			return;
		}
	}

	TEST_METHOD(IntegerWidths)
	{
		FNativeMessageCollector Messages;
		asIScriptEngine* ScriptEngine = CreateNativeEngine(&Messages);
		if (!TestRunner->TestNotNull(TEXT("SDK integer-width type test should create a standalone engine"), ScriptEngine))
		{
			return;
		}

		ON_SCOPE_EXIT
		{
			DestroyNativeEngine(ScriptEngine);
		};

		asIScriptModule* Module = BuildNativeModule(ScriptEngine, "SDKTypeIntegerWidths", R"(
bool Entry()
{
	// Signed extremes
	int8 i8 = 127;
	int16 i16 = 32767;
	int i32 = 2147483647;
	int64 i64 = 9223372036854775807;

	// Unsigned extremes
	uint8 u8 = 255;
	uint16 u16 = 65535;
	uint u32 = 4294967295;
	uint64 u64 = 18446744073709551615;

	// Signed minimums
	int8 i8min = -128;
	int16 i16min = -32768;
	int i32min = -2147483647 - 1;
	int64 i64min = -9223372036854775807 - 1;

	return i8 == 127 && i16 == 32767 && i32 == 2147483647 && i64 == 9223372036854775807
		&& u8 == 255 && u16 == 65535 && u32 == 4294967295 && u64 == 18446744073709551615
		&& i8min == -128 && i16min == -32768 && i32min == -2147483648 && i64min == -9223372036854775807 - 1;
}
)");
		if (!TestRunner->TestNotNull(TEXT("SDK integer-width type test should compile the module"), Module))
		{
			TestRunner->AddInfo(CollectMessages(Messages));
			return;
		}

		bool bResult = false;
		if (!ExecuteScriptBoolFunction(*TestRunner, ScriptEngine, Module, "bool Entry()", bResult))
		{
			return;
		}

		TestRunner->TestTrue(TEXT("SDK integer-width type test should preserve every integer width's extreme values"), bResult);
	}

	TEST_METHOD(IntegerOverflowWrap)
	{
		FNativeMessageCollector Messages;
		asIScriptEngine* ScriptEngine = CreateNativeEngine(&Messages);
		if (!TestRunner->TestNotNull(TEXT("SDK integer-overflow type test should create a standalone engine"), ScriptEngine))
		{
			return;
		}

		ON_SCOPE_EXIT
		{
			DestroyNativeEngine(ScriptEngine);
		};

		asIScriptModule* Module = BuildNativeModule(ScriptEngine, "SDKTypeIntegerOverflow", R"(
bool Entry()
{
	// Unsigned wraparound is well-defined (modular)
	uint8 u8 = 255; u8 += 1;        // wraps to 0
	uint16 u16 = 65535; u16 += 1;   // wraps to 0
	uint u32 = 4294967295; u32 += 1; // wraps to 0

	// Unsigned underflow wraps to max
	uint8 u8u = 0; u8u -= 1;        // 255
	uint u32u = 0; u32u -= 1;       // 4294967295

	// Truncation on narrowing assignment
	int wide = 0x1FF;
	uint8 narrow = uint8(wide);     // keeps low 8 bits = 0xFF = 255

	return u8 == 0 && u16 == 0 && u32 == 0
		&& u8u == 255 && u32u == 4294967295
		&& narrow == 255;
}
)");
		if (!TestRunner->TestNotNull(TEXT("SDK integer-overflow type test should compile the module"), Module))
		{
			TestRunner->AddInfo(CollectMessages(Messages));
			return;
		}

		bool bResult = false;
		if (!ExecuteScriptBoolFunction(*TestRunner, ScriptEngine, Module, "bool Entry()", bResult))
		{
			return;
		}

		TestRunner->TestTrue(TEXT("SDK integer-overflow type test should preserve modular wraparound and truncation semantics"), bResult);
	}

	TEST_METHOD(EnumUnderlyingValues)
	{
		FNativeMessageCollector Messages;
		asIScriptEngine* ScriptEngine = CreateNativeEngine(&Messages);
		if (!TestRunner->TestNotNull(TEXT("SDK enum-underlying type test should create a standalone engine"), ScriptEngine))
		{
			return;
		}

		ON_SCOPE_EXIT
		{
			DestroyNativeEngine(ScriptEngine);
		};

		asIScriptModule* Module = BuildNativeModule(ScriptEngine, "SDKTypeEnumUnderlying", R"(
enum EFlags
{
	None = 0,
	A = 1,
	B = 2,
	C = 4,
	D = 8
}

bool Entry()
{
	// Enum-to-int conversion
	int none = int(EFlags::None);
	int c = int(EFlags::C);

	// Bitwise composition through int, then back to enum
	int composed = int(EFlags::A) | int(EFlags::B);
	bool roundTrips = EFlags(composed) == EFlags(3);

	// int-to-enum and ordering
	bool ordered = int(EFlags::D) > int(EFlags::C);

	return none == 0 && c == 4 && composed == 3 && roundTrips && ordered;
}
)");
		if (!TestRunner->TestNotNull(TEXT("SDK enum-underlying type test should compile the module"), Module))
		{
			TestRunner->AddInfo(CollectMessages(Messages));
			return;
		}

		bool bResult = false;
		if (!ExecuteScriptBoolFunction(*TestRunner, ScriptEngine, Module, "bool Entry()", bResult))
		{
			return;
		}

		TestRunner->TestTrue(TEXT("SDK enum-underlying type test should preserve enum/int conversion and flag composition"), bResult);
	}
};

#endif
