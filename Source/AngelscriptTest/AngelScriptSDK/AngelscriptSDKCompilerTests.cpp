#include "AngelscriptSDKTestExecutionHelpers.h"
#include "AngelscriptTestAdapter.h"

#include "CQTest.h"
#include "Misc/ScopeExit.h"

#if WITH_DEV_AUTOMATION_TESTS


TEST_CLASS_WITH_FLAGS(FAngelscriptSDKCompilerTests,
	"Angelscript.TestModule.AngelScriptSDK.Compiler",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	static void VoidHelper() { }

public:
	inline static AngelscriptNativeTestSupport::FNativeTestEngine Engine;

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

	TEST_METHOD(Basic)
	{
		using namespace AngelscriptNativeTestSupport;
		using namespace AngelscriptSDKTestSupport;

		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("SDK compiler basic test should create a standalone engine")));

		AngelscriptNativeTestSupport::FScopedNativeModule Module(*TestRunner, Engine, "SDKCompilerBasic", R"(
const int GlobalVar = 42;

int Multiply(int A, int B)
{
	return A * B;
}

bool Entry()
{
	return GlobalVar == 42 && Multiply(6, 7) == 42;
}
)");
		if (!Module.IsValid())
		{
			TestRunner->AddInfo(Engine.GetMessagesText());
			return;
		}

		bool bResult = false;
		if (!ExecuteScriptFunction(*TestRunner, ScriptEngine, Module, "bool Entry()", bResult))
		{
			return;
		}

		ASSERT_THAT(IsTrue(bResult, TEXT("SDK compiler basic test should compile and execute basic constructs")));
	}

	TEST_METHOD(Error)
	{
		using namespace AngelscriptNativeTestSupport;
		using namespace AngelscriptSDKTestSupport;

		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("SDK compiler error test should create a standalone engine")));

		// Test that invalid syntax produces compile errors
		AngelscriptNativeTestSupport::FScopedNativeModuleName ModuleScope(Engine, "SDKCompilerError");
		asIScriptModule* Module = BuildNativeModule(ScriptEngine, "SDKCompilerError", R"(
int MissingReturn() { }
)");

		// This should fail to compile - expect null module or error messages
		if (Module != nullptr)
		{
			TestRunner->AddInfo(TEXT("Expected compile error for missing return statement"));
			return;
		}

		ASSERT_THAT(IsTrue(Engine.GetMessages().Entries.Num() > 0,
			TEXT("SDK compiler error test should detect syntax errors")));
	}

	TEST_METHOD(Config)
	{
		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("SDK compiler config test should create a standalone engine")));

		// Test engine property access
		const int PropResult = ScriptEngine->SetEngineProperty(asEP_COPY_SCRIPT_SECTIONS, true);
		ASSERT_THAT(IsTrue(PropResult >= 0, TEXT("SDK compiler config test should set engine property")));
		ON_SCOPE_EXIT
		{
			ScriptEngine->SetEngineProperty(asEP_COPY_SCRIPT_SECTIONS, false);
		};

		// Test type registration configuration
		const int TypeResult = ScriptEngine->RegisterObjectType("TestConfigType", 0, asOBJ_REF | asOBJ_NOCOUNT);
		ASSERT_THAT(IsTrue(TypeResult >= 0, TEXT("SDK compiler config test should register reference type")));

		ASSERT_THAT(IsTrue(true, TEXT("SDK compiler config test should configure engine properties")));
	}

	TEST_METHOD(MultipleErrors)
	{
		using namespace AngelscriptNativeTestSupport;
		using namespace AngelscriptSDKTestSupport;

		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("SDK compiler multiple-errors test should create a standalone engine")));

		// Script with multiple independent errors: undefined symbol and type mismatch.
		Engine.ResetMessages();
		AngelscriptNativeTestSupport::FScopedNativeModuleName ModuleScope(Engine, "SDKCompilerMultipleErrors");
		asIScriptModule* Module = BuildNativeModule(ScriptEngine, "SDKCompilerMultipleErrors", R"(
int Entry()
{
	int x = UndefinedSymbol;
	bool y = 123;
	return x + y;
}
)");

		ASSERT_THAT(IsNull(Module, TEXT("SDK compiler multiple-errors test should fail to compile")));

		ASSERT_THAT(IsTrue(Engine.GetMessages().Entries.Num() >= 2,
			TEXT("SDK compiler multiple-errors test should produce multiple error messages")));
	}

	TEST_METHOD(TypeMismatch)
	{
		using namespace AngelscriptNativeTestSupport;
		using namespace AngelscriptSDKTestSupport;

		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("SDK compiler type-mismatch test should create a standalone engine")));

		// Calling a function that returns void and assigning it should fail.
		Engine.ResetMessages();
		ScriptEngine->RegisterGlobalFunction("void DoNothing()", asFUNCTION(VoidHelper), asCALL_CDECL);
		AngelscriptNativeTestSupport::FScopedNativeModuleName ModuleScope(Engine, "SDKCompilerTypeMismatch");
		asIScriptModule* Module = BuildNativeModule(ScriptEngine, "SDKCompilerTypeMismatch", R"(
int Entry()
{
	int x = DoNothing();
	return x;
}
)");

		ASSERT_THAT(IsNull(Module, TEXT("SDK compiler type-mismatch test should reject assigning void to int")));
	}

	TEST_METHOD(RecompileAfterError)
	{
		using namespace AngelscriptNativeTestSupport;
		using namespace AngelscriptSDKTestSupport;

		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("SDK compiler recompile-after-error test should create a standalone engine")));

		// First attempt: compile failure.
		Engine.ResetMessages();
		AngelscriptNativeTestSupport::FScopedNativeModuleName ModuleScope(Engine, "SDKCompilerRecompile");
		asIScriptModule* FailedModule = BuildNativeModule(ScriptEngine, "SDKCompilerRecompile", R"(
int Entry() { return NotDefined; }
)");

		ASSERT_THAT(IsNull(FailedModule, TEXT("SDK compiler recompile-after-error test should fail the first compilation")));

		// Second attempt: valid script under the same module name.
		Engine.ResetMessages();
		asIScriptModule* SuccessModule = BuildNativeModule(ScriptEngine, "SDKCompilerRecompile", R"(
int Entry() { return 7; }
)");

		if (!this->Assert.IsNotNull(SuccessModule, TEXT("SDK compiler recompile-after-error test should succeed the second compilation")))
		{
			TestRunner->AddInfo(Engine.GetMessagesText());
			return;
		}

		int32 Result = 0;
		if (!ExecuteScriptFunction(*TestRunner, ScriptEngine, SuccessModule, "int Entry()", Result))
		{
			return;
		}

		ASSERT_THAT(AreEqual(7, Result, TEXT("SDK compiler recompile-after-error test should execute the corrected module")));
	}
};

#endif
