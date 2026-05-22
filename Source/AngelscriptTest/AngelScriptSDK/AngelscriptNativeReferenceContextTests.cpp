#include "AngelscriptNativeTestSupport.h"
#include "CQTest.h"
#include "Misc/ScopeExit.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace AngelscriptTest_AngelScriptSDK_ReferenceContext_Private
{
	bool BuildContextModule(
		FAutomationTestBase& Test,
		const char* ModuleName,
		const char* Source,
		AngelscriptNativeTestSupport::FNativeMessageCollector& Messages,
		asIScriptEngine*& OutScriptEngine,
		asIScriptModule*& OutModule)
	{
		OutScriptEngine = AngelscriptNativeTestSupport::CreateNativeEngine(&Messages);
		if (!Test.TestNotNull(TEXT("Reference context test should create a native engine"), OutScriptEngine))
		{
			return false;
		}

		OutModule = AngelscriptNativeTestSupport::BuildNativeModule(OutScriptEngine, ModuleName, Source);
		if (!Test.TestNotNull(TEXT("Reference context test should build the module"), OutModule))
		{
			Test.AddInfo(AngelscriptNativeTestSupport::CollectMessages(Messages));
			return false;
		}

		return true;
	}
}

TEST_CLASS_WITH_FLAGS(FAngelscriptNativeReferenceContextTests,
	"Angelscript.TestModule.AngelScriptSDK.Reference.Context",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(ContextCanBeReusedAfterDeepStackException)
	{
		using namespace AngelscriptTest_AngelScriptSDK_ReferenceContext_Private;
		AngelscriptNativeTestSupport::FNativeMessageCollector Messages;
		asIScriptEngine* ScriptEngine = nullptr;
		asIScriptModule* Module = nullptr;
		if (!BuildContextModule(*TestRunner, "ReferenceContextDeepException", R"(
int BlowStack(int Depth)
{
	if (Depth == 0)
	{
		int Zero = 0;
		return 1 / Zero;
	}

	return BlowStack(Depth - 1) + Depth;
}

int Throwing()
{
	return BlowStack(24);
}

int Safe(int Value)
{
	return Value + 2;
}
)",
			Messages,
			ScriptEngine,
			Module))
		{
			return;
		}

		ON_SCOPE_EXIT
		{
			AngelscriptNativeTestSupport::DestroyNativeEngine(ScriptEngine);
		};

		asIScriptFunction* ThrowingFunction = AngelscriptNativeTestSupport::GetNativeFunctionByDecl(Module, "int Throwing()");
		asIScriptFunction* SafeFunction = AngelscriptNativeTestSupport::GetNativeFunctionByDecl(Module, "int Safe(int)");
		if (!TestRunner->TestNotNull(TEXT("Reference context test should resolve the throwing function"), ThrowingFunction)
			|| !TestRunner->TestNotNull(TEXT("Reference context test should resolve the safe function"), SafeFunction))
		{
			return;
		}

		asIScriptContext* Context = ScriptEngine->CreateContext();
		if (!TestRunner->TestNotNull(TEXT("Reference context test should create a context"), Context))
		{
			return;
		}

		ON_SCOPE_EXIT
		{
			Context->Release();
		};

		TestRunner->TestEqual(TEXT("Reference context test should prepare the throwing function"), Context->Prepare(ThrowingFunction), static_cast<int32>(asSUCCESS));
		TestRunner->TestEqual(TEXT("Reference context test should observe the expected deep-stack exception"), Context->Execute(), static_cast<int32>(asEXECUTION_EXCEPTION));
		TestRunner->TestTrue(TEXT("Reference context test should keep exception line metadata"), Context->GetExceptionLineNumber() > 0);
		TestRunner->TestNotNull(TEXT("Reference context test should keep exception text before reuse"), Context->GetExceptionString());

		TestRunner->TestEqual(TEXT("Reference context test should prepare the safe function after an exception"), Context->Prepare(SafeFunction), static_cast<int32>(asSUCCESS));
		TestRunner->TestEqual(TEXT("Reference context test should accept safe function argument after an exception"), Context->SetArgDWord(0, 40), static_cast<int32>(asSUCCESS));
		TestRunner->TestEqual(TEXT("Reference context test should execute safely after exception reuse"), Context->Execute(), static_cast<int32>(asEXECUTION_FINISHED));
		TestRunner->TestEqual(TEXT("Reference context test should return the safe function result after reuse"), static_cast<int32>(Context->GetReturnDWord()), 42);
	}

	TEST_METHOD(ContextCanSwitchSignaturesAfterException)
	{
		using namespace AngelscriptTest_AngelScriptSDK_ReferenceContext_Private;
		AngelscriptNativeTestSupport::FNativeMessageCollector Messages;
		asIScriptEngine* ScriptEngine = nullptr;
		asIScriptModule* Module = nullptr;
		if (!BuildContextModule(*TestRunner, "ReferenceContextSignatureSwitch", R"(
int Throwing()
{
	int Zero = 0;
	return 1 / Zero;
}

int NoArg()
{
	return 11;
}

int OneArg(int Value)
{
	return Value * 2;
}
)",
			Messages,
			ScriptEngine,
			Module))
		{
			return;
		}

		ON_SCOPE_EXIT
		{
			AngelscriptNativeTestSupport::DestroyNativeEngine(ScriptEngine);
		};

		asIScriptFunction* ThrowingFunction = AngelscriptNativeTestSupport::GetNativeFunctionByDecl(Module, "int Throwing()");
		asIScriptFunction* NoArgFunction = AngelscriptNativeTestSupport::GetNativeFunctionByDecl(Module, "int NoArg()");
		asIScriptFunction* OneArgFunction = AngelscriptNativeTestSupport::GetNativeFunctionByDecl(Module, "int OneArg(int)");
		if (!TestRunner->TestNotNull(TEXT("Reference context signature switch should resolve throwing function"), ThrowingFunction)
			|| !TestRunner->TestNotNull(TEXT("Reference context signature switch should resolve no-arg function"), NoArgFunction)
			|| !TestRunner->TestNotNull(TEXT("Reference context signature switch should resolve one-arg function"), OneArgFunction))
		{
			return;
		}

		asIScriptContext* Context = ScriptEngine->CreateContext();
		if (!TestRunner->TestNotNull(TEXT("Reference context signature switch should create a context"), Context))
		{
			return;
		}

		ON_SCOPE_EXIT
		{
			Context->Release();
		};

		TestRunner->TestEqual(TEXT("Reference context signature switch should prepare the throwing function"), Context->Prepare(ThrowingFunction), static_cast<int32>(asSUCCESS));
		TestRunner->TestEqual(TEXT("Reference context signature switch should observe the exception"), Context->Execute(), static_cast<int32>(asEXECUTION_EXCEPTION));

		TestRunner->TestEqual(TEXT("Reference context signature switch should prepare no-arg function after exception"), Context->Prepare(NoArgFunction), static_cast<int32>(asSUCCESS));
		TestRunner->TestEqual(TEXT("Reference context signature switch should execute no-arg function"), Context->Execute(), static_cast<int32>(asEXECUTION_FINISHED));
		TestRunner->TestEqual(TEXT("Reference context signature switch should return no-arg result"), static_cast<int32>(Context->GetReturnDWord()), 11);

		TestRunner->TestEqual(TEXT("Reference context signature switch should prepare one-arg function after no-arg execution"), Context->Prepare(OneArgFunction), static_cast<int32>(asSUCCESS));
		TestRunner->TestEqual(TEXT("Reference context signature switch should accept one-arg value"), Context->SetArgDWord(0, 21), static_cast<int32>(asSUCCESS));
		TestRunner->TestEqual(TEXT("Reference context signature switch should execute one-arg function"), Context->Execute(), static_cast<int32>(asEXECUTION_FINISHED));
		TestRunner->TestEqual(TEXT("Reference context signature switch should return one-arg result"), static_cast<int32>(Context->GetReturnDWord()), 42);
	}
};

#endif
