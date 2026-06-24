#include "AngelscriptNativeTestSupport.h"
#include "CQTest.h"
#include "Misc/ScopeExit.h"

#if WITH_DEV_AUTOMATION_TESTS

TEST_CLASS_WITH_FLAGS(FAngelscriptNativeReferenceContextTests,
	"Angelscript.TestModule.AngelScriptSDK.Reference.Context",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	static bool BuildContextModule(
		FAutomationTestBase& Test,
		const char* ModuleName,
		const char* Source,
		AngelscriptNativeTestSupport::FNativeMessageCollector& Messages,
		asIScriptEngine*& OutScriptEngine,
		asIScriptModule*& OutModule)
	{
		FNoDiscardAsserter LocalAssert(Test);

		OutScriptEngine = AngelscriptNativeTestSupport::CreateNativeEngine(&Messages);
		if (!LocalAssert.IsNotNull(OutScriptEngine, TEXT("Reference context test should create a native engine")))
		{
			return false;
		}

		OutModule = AngelscriptNativeTestSupport::BuildNativeModule(OutScriptEngine, ModuleName, Source);
		if (!LocalAssert.IsNotNull(OutModule, TEXT("Reference context test should build the module")))
		{
			Test.AddInfo(AngelscriptNativeTestSupport::CollectMessages(Messages));
			return false;
		}

		return true;
	}

public:
	TEST_METHOD(ContextCanBeReusedAfterDeepStackException)
	{
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
		ASSERT_THAT(IsNotNull(ThrowingFunction, TEXT("Reference context test should resolve the throwing function")));
		ASSERT_THAT(IsNotNull(SafeFunction, TEXT("Reference context test should resolve the safe function")));

		asIScriptContext* Context = ScriptEngine->CreateContext();
		ASSERT_THAT(IsNotNull(Context, TEXT("Reference context test should create a context")));

		ON_SCOPE_EXIT
		{
			Context->Release();
		};

		ASSERT_THAT(AreEqual(static_cast<int32>(asSUCCESS), Context->Prepare(ThrowingFunction), TEXT("Reference context test should prepare the throwing function")));
		ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_EXCEPTION), Context->Execute(), TEXT("Reference context test should observe the expected deep-stack exception")));
		ASSERT_THAT(IsTrue(Context->GetExceptionLineNumber() > 0, TEXT("Reference context test should keep exception line metadata")));
		ASSERT_THAT(IsNotNull(Context->GetExceptionString(), TEXT("Reference context test should keep exception text before reuse")));

		ASSERT_THAT(AreEqual(static_cast<int32>(asSUCCESS), Context->Prepare(SafeFunction), TEXT("Reference context test should prepare the safe function after an exception")));
		ASSERT_THAT(AreEqual(static_cast<int32>(asSUCCESS), Context->SetArgDWord(0, 40), TEXT("Reference context test should accept safe function argument after an exception")));
		ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_FINISHED), Context->Execute(), TEXT("Reference context test should execute safely after exception reuse")));
		ASSERT_THAT(AreEqual(42, static_cast<int32>(Context->GetReturnDWord()), TEXT("Reference context test should return the safe function result after reuse")));
	}

	TEST_METHOD(ContextCanSwitchSignaturesAfterException)
	{
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
		ASSERT_THAT(IsNotNull(ThrowingFunction, TEXT("Reference context signature switch should resolve throwing function")));
		ASSERT_THAT(IsNotNull(NoArgFunction, TEXT("Reference context signature switch should resolve no-arg function")));
		ASSERT_THAT(IsNotNull(OneArgFunction, TEXT("Reference context signature switch should resolve one-arg function")));

		asIScriptContext* Context = ScriptEngine->CreateContext();
		ASSERT_THAT(IsNotNull(Context, TEXT("Reference context signature switch should create a context")));

		ON_SCOPE_EXIT
		{
			Context->Release();
		};

		ASSERT_THAT(AreEqual(static_cast<int32>(asSUCCESS), Context->Prepare(ThrowingFunction), TEXT("Reference context signature switch should prepare the throwing function")));
		ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_EXCEPTION), Context->Execute(), TEXT("Reference context signature switch should observe the exception")));

		ASSERT_THAT(AreEqual(static_cast<int32>(asSUCCESS), Context->Prepare(NoArgFunction), TEXT("Reference context signature switch should prepare no-arg function after exception")));
		ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_FINISHED), Context->Execute(), TEXT("Reference context signature switch should execute no-arg function")));
		ASSERT_THAT(AreEqual(11, static_cast<int32>(Context->GetReturnDWord()), TEXT("Reference context signature switch should return no-arg result")));

		ASSERT_THAT(AreEqual(static_cast<int32>(asSUCCESS), Context->Prepare(OneArgFunction), TEXT("Reference context signature switch should prepare one-arg function after no-arg execution")));
		ASSERT_THAT(AreEqual(static_cast<int32>(asSUCCESS), Context->SetArgDWord(0, 21), TEXT("Reference context signature switch should accept one-arg value")));
		ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_FINISHED), Context->Execute(), TEXT("Reference context signature switch should execute one-arg function")));
		ASSERT_THAT(AreEqual(42, static_cast<int32>(Context->GetReturnDWord()), TEXT("Reference context signature switch should return one-arg result")));
	}
};

#endif
