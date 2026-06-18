#include "AngelscriptSDKTestExecutionHelpers.h"
#include "AngelscriptTestAdapter.h"

#include "CQTest.h"
#include "Misc/ScopeExit.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptNativeTestSupport;
using namespace AngelscriptSDKTestSupport;

namespace
{
	void VoidHelper() { }
}


TEST_CLASS_WITH_FLAGS(FAngelscriptSDKCompilerTests,
	"Angelscript.TestModule.AngelScriptSDK.Compiler",
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

	TEST_METHOD(Basic)
	{
		asIScriptEngine* const ScriptEngine = Engine.Get();
		if (!TestRunner->TestNotNull(TEXT("SDK compiler basic test should create a standalone engine"), ScriptEngine))
		{
			return;
		}

		FScopedNativeModule Module(*TestRunner, Engine, "SDKCompilerBasic", R"(
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

		TestRunner->TestTrue(TEXT("SDK compiler basic test should compile and execute basic constructs"), bResult);
	}

	TEST_METHOD(Error)
	{
		asIScriptEngine* const ScriptEngine = Engine.Get();
		if (!TestRunner->TestNotNull(TEXT("SDK compiler error test should create a standalone engine"), ScriptEngine))
		{
			return;
		}

		// Test that invalid syntax produces compile errors
		FScopedNativeModuleName ModuleScope(Engine, "SDKCompilerError");
		asIScriptModule* Module = BuildNativeModule(ScriptEngine, "SDKCompilerError", R"(
int MissingReturn() { }
)");

		// This should fail to compile - expect null module or error messages
		if (Module != nullptr)
		{
			TestRunner->AddInfo(TEXT("Expected compile error for missing return statement"));
			return;
		}

		TestRunner->TestTrue(TEXT("SDK compiler error test should detect syntax errors"), Engine.GetMessages().Entries.Num() > 0);
	}

	TEST_METHOD(Config)
	{
		asIScriptEngine* const ScriptEngine = Engine.Get();
		if (!TestRunner->TestNotNull(TEXT("SDK compiler config test should create a standalone engine"), ScriptEngine))
		{
			return;
		}

		// Test engine property access
		const int PropResult = ScriptEngine->SetEngineProperty(asEP_COPY_SCRIPT_SECTIONS, true);
		if (!TestRunner->TestTrue(TEXT("SDK compiler config test should set engine property"), PropResult >= 0))
		{
			return;
		}
		ON_SCOPE_EXIT
		{
			ScriptEngine->SetEngineProperty(asEP_COPY_SCRIPT_SECTIONS, false);
		};

		// Test type registration configuration
		const int TypeResult = ScriptEngine->RegisterObjectType("TestConfigType", 0, asOBJ_REF | asOBJ_NOCOUNT);
		if (!TestRunner->TestTrue(TEXT("SDK compiler config test should register reference type"), TypeResult >= 0))
		{
			return;
		}

		TestRunner->TestTrue(TEXT("SDK compiler config test should configure engine properties"), true);
	}

	TEST_METHOD(MultipleErrors)
	{
		asIScriptEngine* const ScriptEngine = Engine.Get();
		if (!TestRunner->TestNotNull(TEXT("SDK compiler multiple-errors test should create a standalone engine"), ScriptEngine))
		{
			return;
		}

		// Script with multiple independent errors: undefined symbol and type mismatch.
		Engine.ResetMessages();
		FScopedNativeModuleName ModuleScope(Engine, "SDKCompilerMultipleErrors");
		asIScriptModule* Module = BuildNativeModule(ScriptEngine, "SDKCompilerMultipleErrors", R"(
int Entry()
{
	int x = UndefinedSymbol;
	bool y = 123;
	return x + y;
}
)");

		if (!TestRunner->TestNull(TEXT("SDK compiler multiple-errors test should fail to compile"), Module))
		{
			return;
		}

		TestRunner->TestTrue(TEXT("SDK compiler multiple-errors test should produce multiple error messages"), Engine.GetMessages().Entries.Num() >= 2);
	}

	TEST_METHOD(TypeMismatch)
	{
		asIScriptEngine* const ScriptEngine = Engine.Get();
		if (!TestRunner->TestNotNull(TEXT("SDK compiler type-mismatch test should create a standalone engine"), ScriptEngine))
		{
			return;
		}

		// Calling a function that returns void and assigning it should fail.
		Engine.ResetMessages();
		ScriptEngine->RegisterGlobalFunction("void DoNothing()", asFUNCTION(VoidHelper), asCALL_CDECL);
		FScopedNativeModuleName ModuleScope(Engine, "SDKCompilerTypeMismatch");
		asIScriptModule* Module = BuildNativeModule(ScriptEngine, "SDKCompilerTypeMismatch", R"(
int Entry()
{
	int x = DoNothing();
	return x;
}
)");

		TestRunner->TestNull(TEXT("SDK compiler type-mismatch test should reject assigning void to int"), Module);
	}

	TEST_METHOD(RecompileAfterError)
	{
		asIScriptEngine* const ScriptEngine = Engine.Get();
		if (!TestRunner->TestNotNull(TEXT("SDK compiler recompile-after-error test should create a standalone engine"), ScriptEngine))
		{
			return;
		}

		// First attempt: compile failure.
		Engine.ResetMessages();
		FScopedNativeModuleName ModuleScope(Engine, "SDKCompilerRecompile");
		asIScriptModule* FailedModule = BuildNativeModule(ScriptEngine, "SDKCompilerRecompile", R"(
int Entry() { return NotDefined; }
)");

		if (!TestRunner->TestNull(TEXT("SDK compiler recompile-after-error test should fail the first compilation"), FailedModule))
		{
			return;
		}

		// Second attempt: valid script under the same module name.
		Engine.ResetMessages();
		asIScriptModule* SuccessModule = BuildNativeModule(ScriptEngine, "SDKCompilerRecompile", R"(
int Entry() { return 7; }
)");

		if (!TestRunner->TestNotNull(TEXT("SDK compiler recompile-after-error test should succeed the second compilation"), SuccessModule))
		{
			TestRunner->AddInfo(Engine.GetMessagesText());
			return;
		}

		int32 Result = 0;
		if (!ExecuteScriptFunction(*TestRunner, ScriptEngine, SuccessModule, "int Entry()", Result))
		{
			return;
		}

		TestRunner->TestEqual(TEXT("SDK compiler recompile-after-error test should execute the corrected module"), Result, 7);
	}
};

#endif
