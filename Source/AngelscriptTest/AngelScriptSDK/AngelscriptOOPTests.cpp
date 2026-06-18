#include "AngelscriptTestAdapter.h"

#include "CQTest.h"
#include "Misc/ScopeExit.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptNativeTestSupport;
using namespace AngelscriptSDKTestSupport;


TEST_CLASS_WITH_FLAGS(FAngelscriptSDKOOPTests, "Angelscript.TestModule.AngelScriptSDK.OOP", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	inline static FSDKBufferedOutStream Buffered;
	inline static FNativeSdkAdapterEngineFixture EngineFixture;

	BEFORE_ALL()
	{
		EngineFixture.Create(*TestRunner, &Buffered);
	}

	AFTER_ALL()
	{
		EngineFixture.Destroy();
	}

	BEFORE_EACH()
	{
		EngineFixture.Reset(*TestRunner);
	}

	TEST_METHOD(InterfaceBridge)
	{
		asIScriptEngine* const ScriptEngine = EngineFixture.Get();
		if (!TestRunner->TestNotNull(TEXT("SDK OOP interface test should create a standalone engine"), ScriptEngine))
		{
			return;
		}

		const int InterfaceResult = ScriptEngine->RegisterInterface("appintf");
		const int MethodResult = InterfaceResult >= 0
			? ScriptEngine->RegisterInterfaceMethod("appintf", "void test()")
			: InterfaceResult;
		if (!TestRunner->TestTrue(TEXT("SDK OOP interface test should register the application interface"), InterfaceResult >= 0 && MethodResult >= 0))
		{
			return;
		}

		const FString InterfaceDeclaration = UTF8_TO_TCHAR(ScriptEngine->GetTypeDeclaration(InterfaceResult));
		if (!TestRunner->TestEqual(TEXT("SDK OOP interface test should preserve the registered interface declaration"), InterfaceDeclaration, FString(TEXT("appintf"))))
		{
			return;
		}

		asITypeInfo* InterfaceType = ScriptEngine->GetTypeInfoByName("appintf");
		if (!TestRunner->TestNotNull(TEXT("SDK OOP interface test should expose the registered interface type"), InterfaceType))
		{
			return;
		}

		TestRunner->TestEqual(TEXT("SDK OOP interface test should expose the registered interface method count"), static_cast<int32>(InterfaceType->GetMethodCount()), 1);
	}

	TEST_METHOD(MixinNamespace)
	{
		asIScriptEngine* const ScriptEngine = EngineFixture.Get();
		if (!TestRunner->TestNotNull(TEXT("SDK OOP mixin test should create a standalone engine"), ScriptEngine))
		{
			return;
		}

		FScopedNativeModule Module(*TestRunner, EngineFixture, "SDKOOPMixinNamespace", R"(
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

		TestRunner->TestNotNull(TEXT("SDK OOP mixin test should expose the named mixin wrapper"), GetNativeFunctionByDecl(Module, "bool ApplyMixin()"));
	}

	TEST_METHOD(InheritedInterfaceMethod)
	{
		asIScriptEngine* const ScriptEngine = EngineFixture.Get();
		if (!TestRunner->TestNotNull(TEXT("SDK OOP inherited-interface-method test should create a standalone engine"), ScriptEngine))
		{
			return;
		}

		FScopedNativeModule Module(*TestRunner, EngineFixture, "SDKOOPInheritedInterfaceMethod", R"(
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

		TestRunner->TestNotNull(TEXT("SDK OOP inheritance test should expose the named inherited-member wrapper"), GetNativeFunctionByDecl(Module, "bool TouchInheritedMember()"));
	}
};

#endif
