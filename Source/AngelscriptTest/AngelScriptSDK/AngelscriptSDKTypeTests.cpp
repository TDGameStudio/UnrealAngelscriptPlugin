#include "AngelscriptSDKTestUtilities.h"
#include "AngelscriptTestAdapter.h"

#include "CQTest.h"
#include "Misc/ScopeExit.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptNativeTestSupport;
using namespace AngelscriptSDKTestSupport;

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

	int FindEngineGlobalPropertyIndexByName(asIScriptEngine* ScriptEngine, const char* Name)
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
}


TEST_CLASS_WITH_FLAGS(FAngelscriptSDKTypeTests, "Angelscript.TestModule.AngelScriptSDK.Type", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	inline static FNativeSdkEngineFixture EngineFixture;

	BEFORE_ALL()
	{
		EngineFixture.Create(*TestRunner);
	}

	AFTER_ALL()
	{
		EngineFixture.Destroy();
	}

	BEFORE_EACH()
	{
		EngineFixture.ResetMessages();
		GEnumValue = 0;
		GInt8Value = 0;
	}

	TEST_METHOD(Bool)
	{
		asIScriptEngine* const ScriptEngine = EngineFixture.Get();
		if (!TestRunner->TestNotNull(TEXT("SDK bool type test should create a standalone engine"), ScriptEngine))
		{
			return;
		}

		FScopedNativeModule Module(
			*TestRunner,
			EngineFixture,
			"SDKTypeBool",
			"bool AllTrue() { bool a = true; bool b = false; return a && !b && (a ^^ b); }");
		if (!Module.IsValid())
		{
			TestRunner->AddInfo(EngineFixture.GetMessagesText());
			return;
		}

		FSdkFunctionInvoker Invoker(*TestRunner, ScriptEngine, Module, "bool AllTrue()");
		if (!Invoker.IsValid())
		{
			return;
		}
		TestRunner->TestTrue(TEXT("SDK bool type test should preserve basic boolean logic"), Invoker.CallAndReturn<bool>(false));
	}

	TEST_METHOD(Bits)
	{
		asIScriptEngine* const ScriptEngine = EngineFixture.Get();
		if (!TestRunner->TestNotNull(TEXT("SDK bits type test should create a standalone engine"), ScriptEngine))
		{
			return;
		}

		FScopedNativeModule Module(*TestRunner, EngineFixture, "SDKTypeBits", R"(
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
			TestRunner->AddInfo(EngineFixture.GetMessagesText());
			return;
		}

		FSdkFunctionInvoker Invoker(*TestRunner, ScriptEngine, Module, "bool CheckBits()");
		if (!Invoker.IsValid())
		{
			return;
		}
		TestRunner->TestTrue(TEXT("SDK bits type test should preserve numeric literals and bitwise masks"), Invoker.CallAndReturn<bool>(false));
	}

	TEST_METHOD(Int8)
	{
		asIScriptEngine* const ScriptEngine = EngineFixture.Get();
		if (!TestRunner->TestNotNull(TEXT("SDK int8 type test should create a standalone engine"), ScriptEngine))
		{
			return;
		}

		if (ScriptEngine->GetGlobalFunctionByDecl("int8 RetInt8(int8 value)") == nullptr)
		{
			const ASAutoCaller::FunctionCaller Caller = ASAutoCaller::MakeFunctionCaller(RetInt8);
			const int RegisterFunctionResult = ScriptEngine->RegisterGlobalFunction("int8 RetInt8(int8 value)", asFUNCTION(RetInt8), asCALL_CDECL, *(asFunctionCaller*)&Caller);
			if (!TestRunner->TestTrue(TEXT("SDK int8 type test should register the native int8 callback"), RegisterFunctionResult >= 0))
			{
				return;
			}
		}

		if (FindEngineGlobalPropertyIndexByName(ScriptEngine, "gvar") < 0)
		{
			const int RegisterPropertyResult = ScriptEngine->RegisterGlobalProperty("int8 gvar", &GInt8Value);
			if (!TestRunner->TestTrue(TEXT("SDK int8 type test should register the int8 global property"), RegisterPropertyResult >= 0))
			{
				return;
			}
		}

		FScopedNativeModule Module(*TestRunner, EngineFixture, "SDKTypeInt8", R"(
int ReadInt8RoundTrip()
{
	gvar = RetInt8(1);
	return gvar;
}
)");
		if (!Module.IsValid())
		{
			TestRunner->AddInfo(EngineFixture.GetMessagesText());
			return;
		}

		FSdkFunctionInvoker Invoker(*TestRunner, ScriptEngine, Module, "int ReadInt8RoundTrip()");
		if (!Invoker.IsValid())
		{
			return;
		}
		TestRunner->TestEqual(TEXT("SDK int8 type test should preserve the int8 return through the global property"), Invoker.CallAndReturn<int32>(INDEX_NONE), 1);
	}

	TEST_METHOD(Float)
	{
		asIScriptEngine* const ScriptEngine = EngineFixture.Get();
		if (!TestRunner->TestNotNull(TEXT("SDK float type test should create a standalone engine"), ScriptEngine))
		{
			return;
		}

		const bool bFloatUsesFloat64 = ScriptEngine->GetEngineProperty(asEP_FLOAT_IS_FLOAT64) != 0;
		const char* Source = bFloatUsesFloat64
			? "double CheckFloat() { double a = 1e5; double b = 1.0e5; return (a == b) ? 3.14 : 0.0; }"
			: "double CheckFloat() { float a = 1e5; float b = 1.0e5; return (a == b) ? 3.14f : 0.0f; }";

		FScopedNativeModule Module(*TestRunner, EngineFixture, "SDKTypeFloat", Source);
		if (!Module.IsValid())
		{
			TestRunner->AddInfo(EngineFixture.GetMessagesText());
			return;
		}

		FSdkFunctionInvoker Invoker(*TestRunner, ScriptEngine, Module, "double CheckFloat()");
		if (!Invoker.IsValid())
		{
			return;
		}
		TestRunner->TestTrue(TEXT("SDK float type test should preserve scientific literals and floating equality"), FMath::IsNearlyEqual(Invoker.CallAndReturn<double>(0.0), 3.14, 0.0001));
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

int ReturnTypedefRoundTrip()
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

		TestRunner->TestNotNull(TEXT("SDK typedef bytecode test should preserve the loaded entry function"), GetNativeFunctionByDecl(LoadModule, "int ReturnTypedefRoundTrip()"));
	}

	TEST_METHOD(Enum)
	{
		asIScriptEngine* const ScriptEngine = EngineFixture.Get();
		if (!TestRunner->TestNotNull(TEXT("SDK enum type test should create a standalone engine"), ScriptEngine))
		{
			return;
		}

		if (ScriptEngine->GetTypeIdByDecl("myenum") < 0)
		{
			if (!TestRunner->TestTrue(TEXT("SDK enum type test should register the first enum namespace"), ScriptEngine->RegisterEnum("myenum") >= 0))
			{
				return;
			}

			if (!TestRunner->TestTrue(TEXT("SDK enum type test should register the first enum value"), ScriptEngine->RegisterEnumValue("myenum", "value", 1) >= 0))
			{
				return;
			}
		}

		ScriptEngine->SetDefaultNamespace("foo");
		if (ScriptEngine->GetTypeIdByDecl("myenum") < 0)
		{
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
		}
		ScriptEngine->SetDefaultNamespace("");

		if (ScriptEngine->GetTypeIdByDecl("TEST_ENUM") < 0)
		{
			if (!TestRunner->TestTrue(TEXT("SDK enum type test should register TEST_ENUM"), ScriptEngine->RegisterEnum("TEST_ENUM") >= 0))
			{
				return;
			}

			if (!TestRunner->TestTrue(TEXT("SDK enum type test should register ENUM1"), ScriptEngine->RegisterEnumValue("TEST_ENUM", "ENUM1", 1) >= 0))
			{
				return;
			}
		}

		FScopedNativeModule Module(*TestRunner, EngineFixture, "SDKTypeEnum", R"(
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
			TestRunner->AddInfo(EngineFixture.GetMessagesText());
			return;
		}

		FSdkFunctionInvoker Invoker(*TestRunner, ScriptEngine, Module, "int ReturnLocalEnumValue()");
		if (!Invoker.IsValid())
		{
			return;
		}
		TestRunner->TestEqual(TEXT("SDK enum type test should preserve local enum equality"), Invoker.CallAndReturn<int32>(INDEX_NONE), 1);

		TestRunner->TestEqual(TEXT("SDK enum type test should keep the registered enum declaration accessible"), FString(UTF8_TO_TCHAR(ScriptEngine->GetTypeDeclaration(ScriptEngine->GetTypeIdByDecl("TEST_ENUM")))), FString(TEXT("TEST_ENUM")));
	}

	TEST_METHOD(Auto)
	{
		asIScriptEngine* const ScriptEngine = EngineFixture.Get();
		if (!TestRunner->TestNotNull(TEXT("SDK auto type test should create a standalone engine"), ScriptEngine))
		{
			return;
		}

		FScopedNativeModule Module(*TestRunner, EngineFixture, "SDKTypeAuto", R"(
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
			TestRunner->AddInfo(EngineFixture.GetMessagesText());
			return;
		}

		TestRunner->TestNotNull(TEXT("SDK auto type test should expose the named auto function"), GetNativeFunctionByDecl(Module, "int CreateAutoValue()"));
	}

	TEST_METHOD(IntegerWidths)
	{
		asIScriptEngine* const ScriptEngine = EngineFixture.Get();
		if (!TestRunner->TestNotNull(TEXT("SDK integer-width type test should create a standalone engine"), ScriptEngine))
		{
			return;
		}

		FScopedNativeModule Module(*TestRunner, EngineFixture, "SDKTypeIntegerWidths", R"(
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
			TestRunner->AddInfo(EngineFixture.GetMessagesText());
			return;
		}

		FSdkFunctionInvoker Invoker(*TestRunner, ScriptEngine, Module, "bool CheckIntegerWidths()");
		if (!Invoker.IsValid())
		{
			return;
		}
		TestRunner->TestTrue(TEXT("SDK integer-width type test should preserve every integer width's extreme values"), Invoker.CallAndReturn<bool>(false));
	}

	TEST_METHOD(IntegerOverflowWrap)
	{
		asIScriptEngine* const ScriptEngine = EngineFixture.Get();
		if (!TestRunner->TestNotNull(TEXT("SDK integer-overflow type test should create a standalone engine"), ScriptEngine))
		{
			return;
		}

		FScopedNativeModule Module(*TestRunner, EngineFixture, "SDKTypeIntegerOverflow", R"(
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
			TestRunner->AddInfo(EngineFixture.GetMessagesText());
			return;
		}

		FSdkFunctionInvoker Invoker(*TestRunner, ScriptEngine, Module, "bool CheckIntegerOverflow()");
		if (!Invoker.IsValid())
		{
			return;
		}
		TestRunner->TestTrue(TEXT("SDK integer-overflow type test should preserve modular wraparound and truncation semantics"), Invoker.CallAndReturn<bool>(false));
	}

	TEST_METHOD(EnumUnderlyingValues)
	{
		asIScriptEngine* const ScriptEngine = EngineFixture.Get();
		if (!TestRunner->TestNotNull(TEXT("SDK enum-underlying type test should create a standalone engine"), ScriptEngine))
		{
			return;
		}

		FScopedNativeModule Module(*TestRunner, EngineFixture, "SDKTypeEnumUnderlying", R"(
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
			TestRunner->AddInfo(EngineFixture.GetMessagesText());
			return;
		}

		FSdkFunctionInvoker Invoker(*TestRunner, ScriptEngine, Module, "bool CheckEnumUnderlyingValues()");
		if (!Invoker.IsValid())
		{
			return;
		}
		TestRunner->TestTrue(TEXT("SDK enum-underlying type test should preserve enum/int conversion and flag composition"), Invoker.CallAndReturn<bool>(false));
	}
};

#endif
