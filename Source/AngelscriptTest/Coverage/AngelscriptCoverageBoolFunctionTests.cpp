#include "CQTest.h"
#include "AngelscriptFunctionalTestUtils.h"
#include "AngelscriptGlobalFunctionInvoker.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestModuleBuilder.h"

#include "Misc/ScopeExit.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptFunctionalTestUtils;

TEST_CLASS_WITH_FLAGS(FAngelscriptCoverageBoolFunctionTest,
	"Angelscript.TestModule.Coverage.BoolFunction",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	BEFORE_ALL()
	{
		ASTEST_CREATE_ENGINE();
	}

	AFTER_ALL()
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		ASTEST_RESET_ENGINE(Engine);
	}

	TEST_METHOD(FunctionParametersValue)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovBoolFunc_ParamValue", ASTEST_AS(R"AS(
		bool Negate(bool b)
		{
			return !b;
		}
		)AS"));
		ON_SCOPE_EXIT { if (Module) Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName())); };

		FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("bool Negate(bool)"));
		Invoker.AddArg(true);
		const bool Result = Invoker.CallAndReturn<bool>(false);
		TestRunner->TestFalse(TEXT("bool value parameter"), Result);
	}

	TEST_METHOD(FunctionParametersIn)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovBoolFunc_ParamIn", ASTEST_AS(R"AS(
		bool PassThrough(bool&in b)
		{
			return b;
		}
		)AS"));
		ON_SCOPE_EXIT { if (Module) Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName())); };

		FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("bool PassThrough(bool&in)"));
		Invoker.AddArg(true);
		const bool Result = Invoker.CallAndReturn<bool>(false);
		TestRunner->TestTrue(TEXT("bool &in parameter"), Result);
	}

	TEST_METHOD(FunctionParametersOut)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovBoolFunc_ParamOut", ASTEST_AS(R"AS(
		void SetTrue(bool&out b)
		{
			b = true;
		}
		)AS"));
		ON_SCOPE_EXIT { if (Module) Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName())); };

		FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("void SetTrue(bool&out)"));
		bool OutValue = false;
		Invoker.AddArgRef(OutValue);
		Invoker.Call();
		TestRunner->TestTrue(TEXT("bool &out parameter"), OutValue);
	}

	TEST_METHOD(FunctionParametersInOut)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovBoolFunc_ParamInOut", ASTEST_AS(R"AS(
		void Toggle(bool&inout b)
		{
			b = !b;
		}
		)AS"));
		ON_SCOPE_EXIT { if (Module) Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName())); };

		FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("void Toggle(bool&inout)"));
		bool Value = true;
		Invoker.AddArgRef(Value);
		Invoker.Call();
		TestRunner->TestFalse(TEXT("bool &inout parameter"), Value);
	}
};

#endif
