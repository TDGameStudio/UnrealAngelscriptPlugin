#include "AngelscriptSDKTestUtilities.h"
#include "AngelscriptTestAdapter.h"

#include "CQTest.h"
#include "Misc/ScopeExit.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptNativeTestSupport;
using namespace AngelscriptSDKTestSupport;

TEST_CLASS_WITH_FLAGS(FAngelscriptSDKTypeTests, "Angelscript.TestModule.AngelScriptSDK.Type", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	inline static int32 GEnumValue = 0;
	inline static asBYTE GInt8Value = 0;

	static asBYTE RetInt8(asBYTE InValue)
	{
		return InValue;
	}

	static void CaptureEnum(asIScriptGeneric* Generic)
	{
		if (Generic != nullptr)
		{
			GEnumValue = static_cast<int32>(Generic->GetArgDWord(0));
		}
	}

	static int FindEngineGlobalPropertyIndexByName(asIScriptEngine* ScriptEngine, const char* Name)
	{
		if (ScriptEngine == nullptr || Name == nullptr)
		{
			return -1;
		}

		const asUINT PropertyCount = ScriptEngine->GetGlobalPropertyCount();
		for (asUINT Index = 0; Index < PropertyCount; ++Index)
		{
			const char* PropertyName = nullptr;
			if (ScriptEngine->GetGlobalPropertyByIndex(Index, &PropertyName) >= 0 && PropertyName != nullptr && std::strcmp(PropertyName, Name) == 0)
			{
				return static_cast<int>(Index);
			}
		}

		return -1;
	}

public:
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
		GEnumValue = 0;
		GInt8Value = 0;
	}

	TEST_METHOD(Bool)
	{
		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("SDK bool type test should create a standalone engine")));

		FScopedNativeModule Module(
			*TestRunner,
			Engine,
			"SDKTypeBool",
			"bool AllTrue() { bool a = true; bool b = false; return a && !b && (a ^^ b); }");
		if (!Module.IsValid())
		{
			TestRunner->AddInfo(Engine.GetMessagesText());
			return;
		}

		FSdkFunctionInvoker Invoker(*TestRunner, ScriptEngine, Module, "bool AllTrue()");
		if (!Invoker.IsValid())
		{
			return;
		}
		ASSERT_THAT(IsTrue(Invoker.CallAndReturn<bool>(false),
			TEXT("SDK bool type test should preserve basic boolean logic")));
	}

	TEST_METHOD(Bits)
	{
		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("SDK bits type test should create a standalone engine")));

		FScopedNativeModule Module(*TestRunner, Engine, "SDKTypeBits", R"(
bool CheckBits()
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
		if (!Module.IsValid())
		{
			TestRunner->AddInfo(Engine.GetMessagesText());
			return;
		}

		FSdkFunctionInvoker Invoker(*TestRunner, ScriptEngine, Module, "bool CheckBits()");
		if (!Invoker.IsValid())
		{
			return;
		}
		ASSERT_THAT(IsTrue(Invoker.CallAndReturn<bool>(false),
			TEXT("SDK bits type test should preserve numeric literals and bitwise masks")));
	}

	TEST_METHOD(Int8)
	{
		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("SDK int8 type test should create a standalone engine")));

		if (ScriptEngine->GetGlobalFunctionByDecl("int8 RetInt8(int8 value)") == nullptr)
		{
			const ASAutoCaller::FunctionCaller Caller = ASAutoCaller::MakeFunctionCaller(RetInt8);
			const int RegisterFunctionResult = ScriptEngine->RegisterGlobalFunction("int8 RetInt8(int8 value)", asFUNCTION(RetInt8), asCALL_CDECL, *(asFunctionCaller*)&Caller);
			ASSERT_THAT(IsTrue(RegisterFunctionResult >= 0,
				TEXT("SDK int8 type test should register the native int8 callback")));
		}

		if (FindEngineGlobalPropertyIndexByName(ScriptEngine, "gvar") < 0)
		{
			const int RegisterPropertyResult = ScriptEngine->RegisterGlobalProperty("int8 gvar", &GInt8Value);
			ASSERT_THAT(IsTrue(RegisterPropertyResult >= 0,
				TEXT("SDK int8 type test should register the int8 global property")));
		}

		FScopedNativeModule Module(*TestRunner, Engine, "SDKTypeInt8", R"(
int ReadInt8RoundTrip()
{
	gvar = RetInt8(1);
	return gvar;
}
)");
		if (!Module.IsValid())
		{
			TestRunner->AddInfo(Engine.GetMessagesText());
			return;
		}

		FSdkFunctionInvoker Invoker(*TestRunner, ScriptEngine, Module, "int ReadInt8RoundTrip()");
		if (!Invoker.IsValid())
		{
			return;
		}
		ASSERT_THAT(AreEqual(1, Invoker.CallAndReturn<int32>(INDEX_NONE),
			TEXT("SDK int8 type test should preserve the int8 return through the global property")));
	}

	TEST_METHOD(Float)
	{
		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("SDK float type test should create a standalone engine")));

		const bool bFloatUsesFloat64 = ScriptEngine->GetEngineProperty(asEP_FLOAT_IS_FLOAT64) != 0;
		const char* Source = bFloatUsesFloat64
			? "double CheckFloat() { double a = 1e5; double b = 1.0e5; return (a == b) ? 3.14 : 0.0; }"
			: "double CheckFloat() { float a = 1e5; float b = 1.0e5; return (a == b) ? 3.14f : 0.0f; }";

		FScopedNativeModule Module(*TestRunner, Engine, "SDKTypeFloat", Source);
		if (!Module.IsValid())
		{
			TestRunner->AddInfo(Engine.GetMessagesText());
			return;
		}

		FSdkFunctionInvoker Invoker(*TestRunner, ScriptEngine, Module, "double CheckFloat()");
		if (!Invoker.IsValid())
		{
			return;
		}
		ASSERT_THAT(IsNear(3.14, Invoker.CallAndReturn<double>(0.0), 0.0001,
			TEXT("SDK float type test should preserve scientific literals and floating equality")));
	}

	TEST_METHOD(TypedefBytecode)
	{
		FNativeMessageCollector Messages;
		asIScriptEngine* SaveEngine = CreateNativeEngine(&Messages);
		ASSERT_THAT(IsNotNull(SaveEngine, TEXT("SDK typedef bytecode test should create the save engine")));

		ON_SCOPE_EXIT
		{
			DestroyNativeEngine(SaveEngine);
		};

		ASSERT_THAT(IsTrue(SaveEngine->RegisterTypedef("TestType1", "int8") >= 0,
			TEXT("SDK typedef bytecode test should register TestType1 on the save engine")));

		ASSERT_THAT(IsTrue(SaveEngine->RegisterTypedef("TestType4", "int64") >= 0,
			TEXT("SDK typedef bytecode test should register TestType4 on the save engine")));

		asIScriptModule* SaveModule = BuildNativeModule(SaveEngine, "SDKTypeTypedefSave", R"(
TestType4 Func(TestType1 a)
{
	return a;
}

int ReturnTypedefRoundTrip()
{
	TestType1 v = 1;
	return int(Func(v));
}
)");
		if (!this->Assert.IsNotNull(SaveModule, TEXT("SDK typedef bytecode test should compile the save module")))
		{
			TestRunner->AddInfo(CollectMessages(Messages));
			return;
		}

		FSDKBytecodeStream Bytecode;
		ASSERT_THAT(AreEqual(static_cast<int32>(asSUCCESS), SaveModule->SaveByteCode(&Bytecode),
			TEXT("SDK typedef bytecode test should save bytecode successfully")));

		Bytecode.Restart();

		FNativeMessageCollector LoadMessages;
		asIScriptEngine* LoadEngine = CreateNativeEngine(&LoadMessages);
		ASSERT_THAT(IsNotNull(LoadEngine, TEXT("SDK typedef bytecode test should create the load engine")));

		ON_SCOPE_EXIT
		{
			DestroyNativeEngine(LoadEngine);
		};

		ASSERT_THAT(IsTrue(LoadEngine->RegisterTypedef("TestType1", "int8") >= 0,
			TEXT("SDK typedef bytecode test should register TestType1 on the load engine")));

		ASSERT_THAT(IsTrue(LoadEngine->RegisterTypedef("TestType4", "int64") >= 0,
			TEXT("SDK typedef bytecode test should register TestType4 on the load engine")));

		asIScriptModule* LoadModule = LoadEngine->GetModule("SDKTypeTypedefLoad", asGM_ALWAYS_CREATE);
		ASSERT_THAT(IsNotNull(LoadModule, TEXT("SDK typedef bytecode test should create the load module")));

		ASSERT_THAT(AreEqual(static_cast<int32>(asSUCCESS), LoadModule->LoadByteCode(&Bytecode),
			TEXT("SDK typedef bytecode test should load bytecode successfully")));

		ASSERT_THAT(IsNotNull(GetNativeFunctionByDecl(LoadModule, "int ReturnTypedefRoundTrip()"),
			TEXT("SDK typedef bytecode test should preserve the loaded entry function")));
	}

	TEST_METHOD(Enum)
	{
		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("SDK enum type test should create a standalone engine")));

		if (ScriptEngine->GetTypeIdByDecl("myenum") < 0)
		{
			ASSERT_THAT(IsTrue(ScriptEngine->RegisterEnum("myenum") >= 0,
				TEXT("SDK enum type test should register the first enum namespace")));

			ASSERT_THAT(IsTrue(ScriptEngine->RegisterEnumValue("myenum", "value", 1) >= 0,
				TEXT("SDK enum type test should register the first enum value")));
		}

		ScriptEngine->SetDefaultNamespace("foo");
		ON_SCOPE_EXIT
		{
			ScriptEngine->SetDefaultNamespace("");
		};
		if (ScriptEngine->GetTypeIdByDecl("myenum") < 0)
		{
			ASSERT_THAT(IsTrue(ScriptEngine->RegisterEnum("myenum") >= 0,
				TEXT("SDK enum type test should register a namespaced enum with the same name")));

			ASSERT_THAT(IsTrue(ScriptEngine->RegisterEnumValue("myenum", "value", 1) >= 0,
				TEXT("SDK enum type test should register the namespaced enum value")));
		}
		ScriptEngine->SetDefaultNamespace("");

		if (ScriptEngine->GetTypeIdByDecl("TEST_ENUM") < 0)
		{
			ASSERT_THAT(IsTrue(ScriptEngine->RegisterEnum("TEST_ENUM") >= 0,
				TEXT("SDK enum type test should register TEST_ENUM")));

			ASSERT_THAT(IsTrue(ScriptEngine->RegisterEnumValue("TEST_ENUM", "ENUM1", 1) >= 0,
				TEXT("SDK enum type test should register ENUM1")));
		}

		FScopedNativeModule Module(*TestRunner, Engine, "SDKTypeEnum", R"(
enum LocalEnum
{
	LocalValue = 1
}

int ReturnLocalEnumValue()
{
	LocalEnum Value = LocalEnum::LocalValue;
	return Value == LocalEnum::LocalValue ? 1 : 0;
}
)");
		if (!Module.IsValid())
		{
			TestRunner->AddInfo(Engine.GetMessagesText());
			return;
		}

		FSdkFunctionInvoker Invoker(*TestRunner, ScriptEngine, Module, "int ReturnLocalEnumValue()");
		if (!Invoker.IsValid())
		{
			return;
		}
		ASSERT_THAT(AreEqual(1, Invoker.CallAndReturn<int32>(INDEX_NONE),
			TEXT("SDK enum type test should preserve local enum equality")));

		ASSERT_THAT(AreEqual(FString(TEXT("TEST_ENUM")), FString(UTF8_TO_TCHAR(ScriptEngine->GetTypeDeclaration(ScriptEngine->GetTypeIdByDecl("TEST_ENUM")))),
			TEXT("SDK enum type test should keep the registered enum declaration accessible")));
	}

	TEST_METHOD(Auto)
	{
		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("SDK auto type test should create a standalone engine")));

		FScopedNativeModule Module(*TestRunner, Engine, "SDKTypeAuto", R"(
namespace A
{
	class X
	{
		X()
		{
		}
	}
}

int CreateAutoValue()
{
	auto value = A::X();
	return 1;
}
)");
		if (!Module.IsValid())
		{
			TestRunner->AddInfo(Engine.GetMessagesText());
			return;
		}

		ASSERT_THAT(IsNotNull(GetNativeFunctionByDecl(Module, "int CreateAutoValue()"),
			TEXT("SDK auto type test should expose the named auto function")));
	}

	TEST_METHOD(IntegerWidths)
	{
		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("SDK integer-width type test should create a standalone engine")));

		FScopedNativeModule Module(*TestRunner, Engine, "SDKTypeIntegerWidths", R"(
bool CheckIntegerWidths()
{
	int8 i8 = 127;
	int16 i16 = 32767;
	int i32 = 2147483647;
	int64 i64 = 9223372036854775807;

	uint8 u8 = 255;
	uint16 u16 = 65535;
	uint u32 = 4294967295;
	uint64 u64 = 18446744073709551615;

	int8 i8min = -128;
	int16 i16min = -32768;
	int i32min = -2147483647 - 1;
	int64 i64min = -9223372036854775807 - 1;

	return i8 == 127 && i16 == 32767 && i32 == 2147483647 && i64 == 9223372036854775807
		&& u8 == 255 && u16 == 65535 && u32 == 4294967295 && u64 == 18446744073709551615
		&& i8min == -128 && i16min == -32768 && i32min == -2147483648 && i64min == -9223372036854775807 - 1;
}
)");
		if (!Module.IsValid())
		{
			TestRunner->AddInfo(Engine.GetMessagesText());
			return;
		}

		FSdkFunctionInvoker Invoker(*TestRunner, ScriptEngine, Module, "bool CheckIntegerWidths()");
		if (!Invoker.IsValid())
		{
			return;
		}
		ASSERT_THAT(IsTrue(Invoker.CallAndReturn<bool>(false),
			TEXT("SDK integer-width type test should preserve every integer width's extreme values")));
	}

	TEST_METHOD(IntegerOverflowWrap)
	{
		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("SDK integer-overflow type test should create a standalone engine")));

		FScopedNativeModule Module(*TestRunner, Engine, "SDKTypeIntegerOverflow", R"(
bool CheckIntegerOverflow()
{
	uint8 u8 = 255; u8 += 1;
	uint16 u16 = 65535; u16 += 1;
	uint u32 = 4294967295; u32 += 1;

	uint8 u8u = 0; u8u -= 1;
	uint u32u = 0; u32u -= 1;

	int wide = 0x1FF;
	uint8 narrow = uint8(wide);

	return u8 == 0 && u16 == 0 && u32 == 0
		&& u8u == 255 && u32u == 4294967295
		&& narrow == 255;
}
)");
		if (!Module.IsValid())
		{
			TestRunner->AddInfo(Engine.GetMessagesText());
			return;
		}

		FSdkFunctionInvoker Invoker(*TestRunner, ScriptEngine, Module, "bool CheckIntegerOverflow()");
		if (!Invoker.IsValid())
		{
			return;
		}
		ASSERT_THAT(IsTrue(Invoker.CallAndReturn<bool>(false),
			TEXT("SDK integer-overflow type test should preserve modular wraparound and truncation semantics")));
	}

	TEST_METHOD(EnumUnderlyingValues)
	{
		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("SDK enum-underlying type test should create a standalone engine")));

		FScopedNativeModule Module(*TestRunner, Engine, "SDKTypeEnumUnderlying", R"(
enum EFlags
{
	None = 0,
	A = 1,
	B = 2,
	C = 4,
	D = 8
}

bool CheckEnumUnderlyingValues()
{
	int none = int(EFlags::None);
	int c = int(EFlags::C);
	int composed = int(EFlags::A) | int(EFlags::B);
	bool roundTrips = EFlags(composed) == EFlags(3);
	bool ordered = int(EFlags::D) > int(EFlags::C);
	return none == 0 && c == 4 && composed == 3 && roundTrips && ordered;
}
)");
		if (!Module.IsValid())
		{
			TestRunner->AddInfo(Engine.GetMessagesText());
			return;
		}

		FSdkFunctionInvoker Invoker(*TestRunner, ScriptEngine, Module, "bool CheckEnumUnderlyingValues()");
		if (!Invoker.IsValid())
		{
			return;
		}
		ASSERT_THAT(IsTrue(Invoker.CallAndReturn<bool>(false),
			TEXT("SDK enum-underlying type test should preserve enum/int conversion and flag composition")));
	}
};

#endif
