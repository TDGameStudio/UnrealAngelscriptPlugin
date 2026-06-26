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
// Coverage for AngelScript float-family *function usage* -- the function
// parameter / return value half of the float matrix. This file covers:
//
//   * Function parameters (value / &in / &out / &inout)
//   * Return values
//   * Default parameters
//   * Multiple return values (&out)
//   * Function overloading by width
//   * UFUNCTION parameter/return
//
// Test patterns:
//   - Pattern B: Global functions via FASGlobalFunctionInvoker
//   - Pattern C: UFUNCTION via FFunctionInvoker
//
// float family under test:
//   float / double
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

	// -------------------------------------------------------------------------
	// Function parameters: value passing (float / double).
	// -------------------------------------------------------------------------
	TEST_METHOD(FunctionParametersValue)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovFloatFunc_ParamValue", ASTEST_AS(R"AS(
		float AcceptFloat(float x)
		{
			return x * 2.0f;
		}

		double AcceptDouble(double x)
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
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("float AcceptFloat(float)"));
			Invoker.AddArg(2.5f);
			const float Result = Invoker.ExecuteAndGet<float>(0.0f);
			TestRunner->TestTrue(TEXT("float value parameter"), FMath::IsNearlyEqual(Result, 5.0f, 0.001f));
		}
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("double AcceptDouble(double)"));
			Invoker.AddArg(1.5);
			const double Result = Invoker.ExecuteAndGet<double>(0.0);
			TestRunner->TestTrue(TEXT("double value parameter"), FMath::IsNearlyEqual(Result, 4.5, 0.0001));
		}
	}

	// -------------------------------------------------------------------------
	// Function parameters: &in (const reference semantics).
	// -------------------------------------------------------------------------
	TEST_METHOD(FunctionParametersIn)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovFloatFunc_ParamIn", ASTEST_AS(R"AS(
		float AcceptFloatIn(float&in x)
		{
			return x + 10.0f;
		}

		double AcceptDoubleIn(double&in x)
		{
			return x + 100.0;
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
			Invoker.AddArg(32.0f);
			const float Result = Invoker.ExecuteAndGet<float>(0.0f);
			TestRunner->TestTrue(TEXT("float &in parameter"), FMath::IsNearlyEqual(Result, 42.0f, 0.001f));
		}
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("double AcceptDoubleIn(double&in)"));
			Invoker.AddArg(23.14);
			const double Result = Invoker.ExecuteAndGet<double>(0.0);
			TestRunner->TestTrue(TEXT("double &in parameter"), FMath::IsNearlyEqual(Result, 123.14, 0.0001));
		}
	}

	// -------------------------------------------------------------------------
	// Function parameters: &out (output parameter).
	// -------------------------------------------------------------------------
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
			x = 2.718281828459045;
		}

		void MultipleOut(float&out a, float&out b)
		{
			a = 1.1f;
			b = 2.2f;
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
			Invoker.Execute();
			TestRunner->TestTrue(TEXT("float &out parameter writes value"), FMath::IsNearlyEqual(OutValue, 3.14159f, 0.00001f));
		}
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("void WriteDouble(double&out)"));
			double OutValue = 0.0;
			Invoker.AddArgRef(OutValue);
			Invoker.Execute();
			TestRunner->TestTrue(TEXT("double &out parameter writes value"), FMath::IsNearlyEqual(OutValue, 2.718281828459045, 0.000001));
		}
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("void MultipleOut(float&out, float&out)"));
			float OutA = 0.0f;
			float OutB = 0.0f;
			Invoker.AddArgRef(OutA).AddArgRef(OutB);
			Invoker.Execute();
			TestRunner->TestTrue(TEXT("multiple &out parameter A"), FMath::IsNearlyEqual(OutA, 1.1f, 0.001f));
			TestRunner->TestTrue(TEXT("multiple &out parameter B"), FMath::IsNearlyEqual(OutB, 2.2f, 0.001f));
		}
	}

	// -------------------------------------------------------------------------
	// Function parameters: &inout (read-write parameter).
	// -------------------------------------------------------------------------
	TEST_METHOD(FunctionParametersInOut)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovFloatFunc_ParamInOut", ASTEST_AS(R"AS(
		void DoubleFloat(float&inout x)
		{
			x *= 2.0f;
		}

		void IncrementDouble(double&inout x)
		{
			x += 100.0;
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
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("void DoubleFloat(float&inout)"));
			float Value = 21.0f;
			Invoker.AddArgRef(Value);
			Invoker.Execute();
			TestRunner->TestTrue(TEXT("float &inout parameter modifies in place"), FMath::IsNearlyEqual(Value, 42.0f, 0.001f));
		}
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("void IncrementDouble(double&inout)"));
			double Value = 23.14;
			Invoker.AddArgRef(Value);
			Invoker.Execute();
			TestRunner->TestTrue(TEXT("double &inout parameter modifies in place"), FMath::IsNearlyEqual(Value, 123.14, 0.0001));
		}
	}

	// -------------------------------------------------------------------------
	// Function return values (float / double).
	// -------------------------------------------------------------------------
	TEST_METHOD(FunctionReturnValues)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovFloatFunc_Return", ASTEST_AS(R"AS(
		float ReturnFloat()
		{
			return 123.456f;
		}

		double ReturnDouble()
		{
			return 987.654321;
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
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("float ReturnFloat()"));
			const float Result = Invoker.ExecuteAndGet<float>(0.0f);
			TestRunner->TestTrue(TEXT("float return value"), FMath::IsNearlyEqual(Result, 123.456f, 0.001f));
		}
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("double ReturnDouble()"));
			const double Result = Invoker.ExecuteAndGet<double>(0.0);
			TestRunner->TestTrue(TEXT("double return value"), FMath::IsNearlyEqual(Result, 987.654321, 0.000001));
		}
	}

	// -------------------------------------------------------------------------
	// Default parameters.
	// -------------------------------------------------------------------------
	TEST_METHOD(FunctionDefaultParameters)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovFloatFunc_Default", ASTEST_AS(R"AS(
		float AddWithDefault(float a, float b = 10.0f)
		{
			return a + b;
		}

		double MultiplyWithDefault(double x, double y = 2.0)
		{
			return x * y;
		}
		)AS"));
		ON_SCOPE_EXIT
		{
			if (Module != nullptr)
			{
				Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
			}
		};

		// Call with all arguments
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("float AddWithDefault(float, float)"));
			Invoker.AddArg(32.0f).AddArg(10.0f);
			const float Result = Invoker.ExecuteAndGet<float>(0.0f);
			TestRunner->TestTrue(TEXT("default parameter when explicitly provided"), FMath::IsNearlyEqual(Result, 42.0f, 0.001f));
		}

		// Call with default (omit second argument)
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("float AddWithDefault(float)"));
			Invoker.AddArg(32.0f);
			const float Result = Invoker.ExecuteAndGet<float>(0.0f);
			TestRunner->TestTrue(TEXT("default parameter used"), FMath::IsNearlyEqual(Result, 42.0f, 0.001f));
		}

		// double with default
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("double MultiplyWithDefault(double)"));
			Invoker.AddArg(61.5);
			const double Result = Invoker.ExecuteAndGet<double>(0.0);
			TestRunner->TestTrue(TEXT("double default parameter"), FMath::IsNearlyEqual(Result, 123.0, 0.0001));
		}
	}

	// -------------------------------------------------------------------------
	// Function overloading by width (float vs double).
	// -------------------------------------------------------------------------
	TEST_METHOD(FunctionOverloading)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovFloatFunc_Overload", ASTEST_AS(R"AS(
		float Process(float x)
		{
			return x + 100.0f;
		}

		double Process(double x)
		{
			return x + 1000.0;
		}

		float CallProcessFloat()
		{
			return Process(42.5f);
		}

		double CallProcessDouble()
		{
			return Process(123.456);
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
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("float CallProcessFloat()"));
			const float Result = Invoker.ExecuteAndGet<float>(0.0f);
			TestRunner->TestTrue(TEXT("overload resolves to float version"), FMath::IsNearlyEqual(Result, 142.5f, 0.001f));
		}
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("double CallProcessDouble()"));
			const double Result = Invoker.ExecuteAndGet<double>(0.0);
			TestRunner->TestTrue(TEXT("overload resolves to double version"), FMath::IsNearlyEqual(Result, 1123.456, 0.0001));
		}
	}

	// -------------------------------------------------------------------------
	// UFUNCTION parameter and return values (Pattern C: requires Actor context).
	// -------------------------------------------------------------------------
	TEST_METHOD(UFunctionParametersAndReturn)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageFloatFunction_UFUNCTION"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageFloatFunctionUFUNCTION.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageFloatFunctionActor : AActor
			{
				UFUNCTION()
				float AddFloats(float a, float b)
				{
					return a + b;
				}

				UFUNCTION()
				double MultiplyDouble(double x, double y)
				{
					return x * y;
				}

				UFUNCTION()
				void WriteOut(float&out result)
				{
					result = 999.999f;
				}
			}
			)AS"),
			TEXT("ACoverageFloatFunctionActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Float-function UFUNCTION actor class should compile")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Float-function UFUNCTION actor should spawn")));
		BeginPlayActor(Engine, *Actor);

		// UFUNCTION with float parameters and return
		{
			FFunctionInvoker Invoker(*TestRunner, Actor, TEXT("AddFloats"));
			Invoker.AddParam(20.5f);
			Invoker.AddParam(21.5f);
			const float Result = Invoker.CallAndReturn<float>();
			TestRunner->TestTrue(TEXT("UFUNCTION float parameters and return"), FMath::IsNearlyEqual(Result, 42.0f, 0.001f));
		}

		// UFUNCTION with double
		{
			FFunctionInvoker Invoker(*TestRunner, Actor, TEXT("MultiplyDouble"));
			Invoker.AddParam(6.15);
			Invoker.AddParam(20.0);
			const double Result = Invoker.CallAndReturn<double>();
			TestRunner->TestTrue(TEXT("UFUNCTION double parameters and return"), FMath::IsNearlyEqual(Result, 123.0, 0.0001));
		}

		// UFUNCTION with &out parameter
		{
			FFunctionInvoker Invoker(*TestRunner, Actor, TEXT("WriteOut"));
			float OutValue = 0.0f;
			Invoker.AddParam(0.0f);
			Invoker.Call();
			Invoker.ReadParamAfterCall(0, OutValue);
			TestRunner->TestTrue(TEXT("UFUNCTION float &out parameter"), FMath::IsNearlyEqual(OutValue, 999.999f, 0.001f));
		}
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
