#include "AngelscriptSDKTestExecutionHelpers.h"
#include "AngelscriptTestAdapter.h"

#include "CQTest.h"
#include "Misc/ScopeExit.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptNativeTestSupport;
using namespace AngelscriptSDKTestSupport;


TEST_CLASS_WITH_FLAGS(FAngelscriptSDKModuleTests,
	"Angelscript.TestModule.AngelScriptSDK.Module",
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
	TEST_METHOD(Create)
	{
		asIScriptEngine* ScriptEngine = Engine.Get();
		if (!TestRunner->TestNotNull(TEXT("SDK module create test should create a standalone engine"), ScriptEngine))
		{
			return;
		}

		FScopedNativeModuleName ModuleScope(Engine, "SDKModuleCreate");
		asIScriptModule* Module = ScriptEngine->GetModule("SDKModuleCreate", asGM_ALWAYS_CREATE);
		if (!TestRunner->TestNotNull(TEXT("SDK module create test should create a module"), Module))
		{
			return;
		}

		const int AddResult = Module->AddScriptSection("test", "const int Value = 42; bool Entry() { return Value == 42; }");
		if (!TestRunner->TestTrue(TEXT("SDK module create test should add script section"), AddResult >= 0))
		{
			return;
		}

		const int BuildResult = Module->Build();
		if (!TestRunner->TestEqual(TEXT("SDK module create test should build successfully"), BuildResult, 0))
		{
			TestRunner->AddInfo(Engine.GetMessagesText());
			return;
		}

		asIScriptFunction* Function = GetNativeFunctionByDecl(Module, "bool Entry()");
		TestRunner->TestNotNull(TEXT("SDK module create test should find entry function"), Function);
	}

	TEST_METHOD(Discard)
	{
		asIScriptEngine* ScriptEngine = Engine.Get();
		if (!TestRunner->TestNotNull(TEXT("SDK module discard test should create a standalone engine"), ScriptEngine))
		{
			return;
		}

		FScopedNativeModule Module(*TestRunner, Engine, "SDKModuleDiscard", "const int Value = 100;                         \n");
		if (!Module.IsValid())
		{
			return;
		}

		// Discard the module
		ScriptEngine->DiscardModule("SDKModuleDiscard");

		// Verify module is gone
		asIScriptModule* DiscardedModule = ScriptEngine->GetModule("SDKModuleDiscard", asGM_ONLY_IF_EXISTS);
		TestRunner->TestNull(TEXT("SDK module discard test should discard the module"), DiscardedModule);
	}

	TEST_METHOD(Multi)
	{
		asIScriptEngine* ScriptEngine = Engine.Get();
		if (!TestRunner->TestNotNull(TEXT("SDK module multi test should create a standalone engine"), ScriptEngine))
		{
			return;
		}

		// Create first module
		FScopedNativeModule Module1(*TestRunner, Engine, "SDKModuleMulti1", "int GetValue() { return 1; }                   \n");
		if (!Module1.IsValid())
		{
			return;
		}

		// Create second module
		FScopedNativeModule Module2(*TestRunner, Engine, "SDKModuleMulti2", "int GetValue() { return 2; }                   \n");
		if (!Module2.IsValid())
		{
			return;
		}

		// Verify both modules exist and have distinct functions
		asIScriptFunction* Func1 = GetNativeFunctionByDecl(Module1, "int GetValue()");
		asIScriptFunction* Func2 = GetNativeFunctionByDecl(Module2, "int GetValue()");

		if (!TestRunner->TestNotNull(TEXT("SDK module multi test should find first module function"), Func1))
		{
			return;
		}

		if (!TestRunner->TestNotNull(TEXT("SDK module multi test should find second module function"), Func2))
		{
			return;
		}

		TestRunner->TestNotEqual(TEXT("SDK module multi test should have distinct functions"), Func1, Func2);
	}

	TEST_METHOD(MultiSection)
	{
		asIScriptEngine* ScriptEngine = Engine.Get();
		if (!TestRunner->TestNotNull(TEXT("SDK module multi-section test should create a standalone engine"), ScriptEngine))
		{
			return;
		}

		FScopedNativeModuleName ModuleScope(Engine, "SDKModuleMultiSection");
		asIScriptModule* Module = ScriptEngine->GetModule("SDKModuleMultiSection", asGM_ALWAYS_CREATE);
		if (!TestRunner->TestNotNull(TEXT("SDK module multi-section test should create a module"), Module))
		{
			return;
		}

		// Two sections that reference each other: cross-section symbol resolution.
		const int Add1 = Module->AddScriptSection("sectionA", "int Helper() { return 40; }");
		const int Add2 = Module->AddScriptSection("sectionB", "int Entry() { return Helper() + 2; }");
		if (!TestRunner->TestTrue(TEXT("SDK module multi-section test should add both sections"), Add1 >= 0 && Add2 >= 0))
		{
			return;
		}

		if (!TestRunner->TestEqual(TEXT("SDK module multi-section test should build across sections"), Module->Build(), 0))
		{
			TestRunner->AddInfo(Engine.GetMessagesText());
			return;
		}

		int32 Result = 0;
		if (!ExecuteScriptFunction(*TestRunner, ScriptEngine, Module, "int Entry()", Result))
		{
			return;
		}

		TestRunner->TestEqual(TEXT("SDK module multi-section test should resolve a symbol across sections (40+2=42)"), Result, 42);
	}

	TEST_METHOD(EnumerateFunctions)
	{
		asIScriptEngine* ScriptEngine = Engine.Get();
		if (!TestRunner->TestNotNull(TEXT("SDK module enumerate test should create a standalone engine"), ScriptEngine))
		{
			return;
		}

		FScopedNativeModule Module(*TestRunner, Engine, "SDKModuleEnumerate", R"(
int Alpha() { return 1; }
int Beta() { return 2; }
int Gamma() { return 3; }
)");
		if (!Module.IsValid())
		{
			return;
		}

		if (!TestRunner->TestEqual(TEXT("SDK module enumerate test should report three module functions"), static_cast<int32>(Module->GetFunctionCount()), 3))
		{
			return;
		}

		bool bFoundBeta = false;
		for (asUINT Index = 0; Index < Module->GetFunctionCount(); ++Index)
		{
			asIScriptFunction* Function = Module->GetFunctionByIndex(Index);
			if (Function != nullptr && FString(UTF8_TO_TCHAR(Function->GetName())) == TEXT("Beta"))
			{
				bFoundBeta = true;
			}
		}

		TestRunner->TestTrue(TEXT("SDK module enumerate test should find Beta via GetFunctionByIndex"), bFoundBeta);
		TestRunner->TestNotNull(TEXT("SDK module enumerate test should resolve Gamma by declaration"), Module->GetFunctionByDecl("int Gamma()"));
	}

	TEST_METHOD(RecompileAfterDiscard)
	{
		asIScriptEngine* ScriptEngine = Engine.Get();
		if (!TestRunner->TestNotNull(TEXT("SDK module recompile test should create a standalone engine"), ScriptEngine))
		{
			return;
		}

		// Build, discard, then rebuild the same module name with different content.
		FScopedNativeModuleName ModuleScope(Engine, "SDKModuleRecompile");
		asIScriptModule* First = BuildNativeModule(ScriptEngine, "SDKModuleRecompile", "int Entry() { return 1; }");
		if (!TestRunner->TestNotNull(TEXT("SDK module recompile test should build the first module"), First))
		{
			TestRunner->AddInfo(Engine.GetMessagesText());
			return;
		}

		ScriptEngine->DiscardModule("SDKModuleRecompile");

		asIScriptModule* Second = BuildNativeModule(ScriptEngine, "SDKModuleRecompile", "int Entry() { return 2; }");
		if (!TestRunner->TestNotNull(TEXT("SDK module recompile test should build the second module"), Second))
		{
			TestRunner->AddInfo(Engine.GetMessagesText());
			return;
		}

		int32 Result = 0;
		if (!ExecuteScriptFunction(*TestRunner, ScriptEngine, Second, "int Entry()", Result))
		{
			return;
		}

		TestRunner->TestEqual(TEXT("SDK module recompile test should run the rebuilt definition (returns 2)"), Result, 2);
	}
};

#endif
