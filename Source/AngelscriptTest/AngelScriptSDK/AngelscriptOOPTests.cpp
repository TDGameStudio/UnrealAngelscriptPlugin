#include "AngelscriptTestAdapter.h"

#include "CQTest.h"
#include "Misc/ScopeExit.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptNativeTestSupport;
using namespace AngelscriptSDKTestSupport;


TEST_CLASS_WITH_FLAGS(FAngelscriptSDKOOPTests, "Angelscript.TestModule.AngelScriptSDK.OOP", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	inline static FSDKBufferedOutStream Buffered;
	inline static FNativeTestEngine Engine;

	BEFORE_ALL()
	{
		Engine.Create(*TestRunner, &Buffered);
	}

	AFTER_ALL()
	{
		Engine.Destroy();
	}

	BEFORE_EACH()
	{
		Engine.Reset(*TestRunner);
	}

	TEST_METHOD(InterfaceBridge)
	{
		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("SDK OOP interface test should create a standalone engine")));

		const int InterfaceResult = ScriptEngine->RegisterInterface("appintf");
		const int MethodResult = InterfaceResult >= 0
			? ScriptEngine->RegisterInterfaceMethod("appintf", "void test()")
			: InterfaceResult;
		ASSERT_THAT(IsTrue(InterfaceResult >= 0 && MethodResult >= 0,
			TEXT("SDK OOP interface test should register the application interface")));

		const FString InterfaceDeclaration = UTF8_TO_TCHAR(ScriptEngine->GetTypeDeclaration(InterfaceResult));
		ASSERT_THAT(AreEqual(FString(TEXT("appintf")), InterfaceDeclaration,
			TEXT("SDK OOP interface test should preserve the registered interface declaration")));

		asITypeInfo* InterfaceType = ScriptEngine->GetTypeInfoByName("appintf");
		ASSERT_THAT(IsNotNull(InterfaceType, TEXT("SDK OOP interface test should expose the registered interface type")));

		ASSERT_THAT(AreEqual(1, static_cast<int32>(InterfaceType->GetMethodCount()),
			TEXT("SDK OOP interface test should expose the registered interface method count")));
	}

	TEST_METHOD(MixinNamespace)
	{
		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("SDK OOP mixin test should create a standalone engine")));

		FScopedNativeModule Module(*TestRunner, Engine, "SDKOOPMixinNamespace", R"(
struct Counter
{
	int Value = 0;
}

mixin void AddToCounter(Counter& Self, int Delta)
{
	Self.Value += Delta;
}

bool ApplyMixin()
{
	Counter Value;
	Value.AddToCounter(3);
	return Value.Value == 3;
}
)");
		if (!Module.IsValid())
		{
			TestRunner->AddInfo(UTF8_TO_TCHAR(Buffered.Buffer.c_str()));
			return;
		}

		ASSERT_THAT(IsNotNull(GetNativeFunctionByDecl(Module, "bool ApplyMixin()"),
			TEXT("SDK OOP mixin test should expose the named mixin wrapper")));
	}

	TEST_METHOD(InheritedInterfaceMethod)
	{
		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("SDK OOP inherited-interface-method test should create a standalone engine")));

		FScopedNativeModule Module(*TestRunner, Engine, "SDKOOPInheritedInterfaceMethod", R"(
class B
{
	bool touched = false;

	void Touch()
	{
		touched = true;
	}
}

class D : B
{
}

bool TouchInheritedMember()
{
	D value = D();
	value.Touch();
	return value.touched;
}
)");
		if (!Module.IsValid())
		{
			TestRunner->AddInfo(UTF8_TO_TCHAR(Buffered.Buffer.c_str()));
			return;
		}

		ASSERT_THAT(IsNotNull(GetNativeFunctionByDecl(Module, "bool TouchInheritedMember()"),
			TEXT("SDK OOP inheritance test should expose the named inherited-member wrapper")));
	}
};

#endif
