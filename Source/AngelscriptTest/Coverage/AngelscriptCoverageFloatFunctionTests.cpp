#include "CQTest.h"
#include "AngelscriptFunctionalTestUtils.h"
#include "AngelscriptGlobalFunctionInvoker.h"
#include "AngelscriptReflectiveAccess.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestModuleBuilder.h"
#include "AngelscriptTestUtilities.h"

#include "Components/ActorTestSpawner.h"
#include "GameFramework/Actor.h"
#include "Misc/ScopeExit.h"

// -----------------------------------------------------------------------------
// AngelscriptCoverageFloatFunctionTests
// -----------------------------------------------------------------------------
// Coverage for AngelScript float/double function usage
// -----------------------------------------------------------------------------

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptFunctionalTestUtils;

TEST_CLASS_WITH_FLAGS(FAngelscriptCoverageFloatFunctionTest,
	"Angelscript.TestModule.Coverage.FloatFunction",
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

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovFloatFunc_ParamValue", ASTEST_AS(R"AS(
		float AcceptFloat(float x)
		{
			return x + 1.5f;
		}

		double AcceptDouble(double x)
		{
			return x + 2.5;
		}
		)AS"));
		ON_SCOPE_EXIT
		{
			if (Module != nullptr)
			{
				Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
			}
		};

		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("float AcceptFloat(float)"));
			Invoker.AddArg(10.5f);
			const float Result = Invoker.CallAndReturn<float>(0.0f);
			TestRunner->TestEqual(TEXT("float value parameter"), Result, 12.0f, 0.001f);
		}
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("double AcceptDouble(double)"));
			Invoker.AddArg(20.5);
			const double Result = Invoker.CallAndReturn<double>(0.0);
			TestRunner->TestEqual(TEXT("double value parameter"), Result, 23.0, 0.001);
		}
	}

	TEST_METHOD(FunctionParametersIn)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovFloatFunc_ParamIn", ASTEST_AS(R"AS(
		float AcceptFloatIn(float&in x)
		{
			return x * 2.0f;
		}

		double AcceptDoubleIn(double&in x)
		{
			return x * 3.0;
		}
		)AS"));
		ON_SCOPE_EXIT
		{
			if (Module != nullptr)
			{
				Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
			}
		};

		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("float AcceptFloatIn(float&in)"));
			Invoker.AddArg(5.5f);
			const float Result = Invoker.CallAndReturn<float>(0.0f);
			TestRunner->TestEqual(TEXT("float &in parameter"), Result, 11.0f, 0.001f);
		}
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("double AcceptDoubleIn(double&in)"));
			Invoker.AddArg(10.5);
			const double Result = Invoker.CallAndReturn<double>(0.0);
			TestRunner->TestEqual(TEXT("double &in parameter"), Result, 31.5, 0.001);
		}
	}

	TEST_METHOD(FunctionParametersOut)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovFloatFunc_ParamOut", ASTEST_AS(R"AS(
		void WriteFloat(float&out x)
		{
			x = 3.14159f;
		}

		void WriteDouble(double&out x)
		{
			x = 2.71828;
		}
		)AS"));
		ON_SCOPE_EXIT
		{
			if (Module != nullptr)
			{
				Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
			}
		};

		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("void WriteFloat(float&out)"));
			float OutValue = 0.0f;
			Invoker.AddArgRef(OutValue);
			Invoker.Call();
			TestRunner->TestEqual(TEXT("float &out parameter"), OutValue, 3.14159f, 0.00001f);
		}
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("void WriteDouble(double&out)"));
			double OutValue = 0.0;
			Invoker.AddArgRef(OutValue);
			Invoker.Call();
			TestRunner->TestEqual(TEXT("double &out parameter"), OutValue, 2.71828, 0.00001);
		}
	}

	TEST_METHOD(FunctionParametersInOut)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovFloatFunc_ParamInOut", ASTEST_AS(R"AS(
		void SquareFloat(float&inout x)
		{
			x = x * x;
		}

		void SquareDouble(double&inout x)
		{
			x = x * x;
		}
		)AS"));
		ON_SCOPE_EXIT
		{
			if (Module != nullptr)
			{
				Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
			}
		};

		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("void SquareFloat(float&inout)"));
			float Value = 5.0f;
			Invoker.AddArgRef(Value);
			Invoker.Call();
			TestRunner->TestEqual(TEXT("float &inout parameter"), Value, 25.0f, 0.001f);
		}
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("void SquareDouble(double&inout)"));
			double Value = 10.0;
			Invoker.AddArgRef(Value);
			Invoker.Call();
			TestRunner->TestEqual(TEXT("double &inout parameter"), Value, 100.0, 0.001);
		}
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
