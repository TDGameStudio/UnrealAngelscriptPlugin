#include "AngelscriptNativeTestSupport.h"

#include "CQTest.h"
#include "Misc/ScopeExit.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptNativeTestSupport;

TEST_CLASS_WITH_FLAGS(FAngelscriptNativeCompileTests,
	"Angelscript.TestModule.AngelScriptSDK.Compile",
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

	TEST_METHOD(SimpleFunction)
	{
		asIScriptEngine* ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Native compile simple-function test should create a standalone engine")));

		FScopedNativeModule Module(*TestRunner, Engine, "NativeCompileSimpleFunction", "int Test() { return 42; }");
		if (!Module.IsValid())
		{
			return;
		}

		ASSERT_THAT(IsNotNull(GetNativeFunctionByDecl(Module, "int Test()"),
			TEXT("Native compile simple-function test should expose the compiled function")));
	}

	TEST_METHOD(MultipleFunctions)
	{
		asIScriptEngine* ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Native compile multiple-functions test should create a standalone engine")));

		FScopedNativeModule Module(*TestRunner, Engine, "NativeCompileMultipleFunctions", "void A() {} void B() {} int C() { return 42; }");
		if (!Module.IsValid())
		{
			return;
		}

		ASSERT_THAT(AreEqual(3, static_cast<int32>(Module->GetFunctionCount()),
			TEXT("Native compile multiple-functions test should expose every compiled function")));
	}

	TEST_METHOD(GlobalVariables)
	{
		asIScriptEngine* ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Native compile global-variables test should create a standalone engine")));

		FScopedNativeModule Module(*TestRunner, Engine, "NativeCompileGlobalVariables", "const int First = 40; const int Second = 2; int Read() { return First + Second; }");
		if (!Module.IsValid())
		{
			return;
		}

		ASSERT_THAT(AreEqual(2, static_cast<int32>(Module->GetGlobalVarCount()),
			TEXT("Native compile global-variables test should preserve both global declarations")));
	}

	TEST_METHOD(SyntaxError)
	{
		asIScriptEngine* ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Native compile syntax-error test should create a standalone engine")));

		FScopedNativeModuleName ModuleScope(Engine, "NativeCompileSyntaxError");
		asIScriptModule* Module = nullptr;
		const int BuildResult = CompileNativeModule(ScriptEngine, "NativeCompileSyntaxError", "int Broken( { return 1; }", Module);
		if (!this->Assert.IsTrue(BuildResult < 0, TEXT("Native compile syntax-error test should fail with a negative build result")))
		{
			TestRunner->AddInfo(Engine.GetMessagesText());
			return;
		}

		ASSERT_THAT(IsNotNull(Module, TEXT("Native compile syntax-error test should still expose a module handle for diagnostics")));
	}

	TEST_METHOD(ErrorMessage)
	{
		asIScriptEngine* ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Native compile error-message test should create a standalone engine")));

		FScopedNativeModuleName ModuleScope(Engine, "NativeCompileErrorMessage");
		asIScriptModule* Module = nullptr;
		const int BuildResult = CompileNativeModule(ScriptEngine, "NativeCompileErrorMessage", "int Broken( { return 1; }", Module);
		if (!this->Assert.IsTrue(BuildResult < 0, TEXT("Native compile error-message test should fail with a negative build result")))
		{
			TestRunner->AddInfo(Engine.GetMessagesText());
			return;
		}

		const FNativeMessageCollector& Messages = Engine.GetMessages();
		ASSERT_THAT(IsTrue(Messages.Entries.Num() > 0,
			TEXT("Native compile error-message test should capture at least one diagnostic entry")));

		const FNativeMessageEntry& FirstMessage = Messages.Entries[0];
		ASSERT_THAT(IsTrue(!FirstMessage.Message.IsEmpty(),
			TEXT("Native compile error-message test should capture a non-empty message text")));
		ASSERT_THAT(IsTrue(FirstMessage.Row > 0,
			TEXT("Native compile error-message test should capture a valid source row")));
		ASSERT_THAT(IsTrue(!Engine.GetMessagesText().IsEmpty(),
			TEXT("Native compile error-message test should format the diagnostics for debugging")));
	}
};

#endif
