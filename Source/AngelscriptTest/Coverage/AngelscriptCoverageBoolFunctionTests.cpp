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
// AngelscriptCoverageBoolFunctionTests
// -----------------------------------------------------------------------------
// Coverage for AngelScript bool *function usage* -- parameters, return values,
// defaults, and UFUNCTION.
//
// Test patterns:
//   - Pattern B: Global functions via FASGlobalFunctionInvoker
//   - Pattern C: UFUNCTION via FFunctionInvoker
// -----------------------------------------------------------------------------

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

	// -------------------------------------------------------------------------
	// Function parameters: value passing.
	// -------------------------------------------------------------------------
	TEST_METHOD(FunctionParametersValue)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovBoolFunc_ParamValue", ASTEST_AS(R"AS(
		bool AcceptBool(bool x)
		{
			return !x;
		}

		bool AcceptTwoBools(bool a, bool b)
		{
			return a && b;
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
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("bool AcceptBool(bool)"));
			Invoker.AddArg(true);
			const bool Result = Invoker.ExecuteAndGet<bool>(false);
			TestRunner->TestEqual(TEXT("bool value parameter (true)"), Result, false);
		}
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("bool AcceptBool(bool)"));
			Invoker.AddArg(false);
			const bool Result = Invoker.ExecuteAndGet<bool>(false);
			TestRunner->TestEqual(TEXT("bool value parameter (false)"), Result, true);
		}
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("bool AcceptTwoBools(bool, bool)"));
			Invoker.AddArg(true);
			Invoker.AddArg(true);
			const bool Result = Invoker.ExecuteAndGet<bool>(false);
			TestRunner->TestEqual(TEXT("two bool value parameters"), Result, true);
		}
	}

	// -------------------------------------------------------------------------
	// Function parameters: &in (const reference).
	// -------------------------------------------------------------------------
	TEST_METHOD(FunctionParametersIn)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovBoolFunc_ParamIn", ASTEST_AS(R"AS(
		bool AcceptBoolIn(bool&in x)
		{
			return x;
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
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("bool AcceptBoolIn(bool&in)"));
			Invoker.AddArg(true);
			const bool Result = Invoker.ExecuteAndGet<bool>(false);
			TestRunner->TestEqual(TEXT("bool &in parameter"), Result, true);
		}
	}

	// -------------------------------------------------------------------------
	// Function parameters: &out (output parameter).
	// -------------------------------------------------------------------------
	TEST_METHOD(FunctionParametersOut)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovBoolFunc_ParamOut", ASTEST_AS(R"AS(
		void WriteBool(bool&out x)
		{
			x = true;
		}

		void WriteMultipleBools(bool&out a, bool&out b)
		{
			a = true;
			b = false;
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
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("void WriteBool(bool&out)"));
			bool OutValue = false;
			Invoker.AddArgRef(OutValue);
			Invoker.Execute();
			TestRunner->TestEqual(TEXT("bool &out parameter"), OutValue, true);
		}
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("void WriteMultipleBools(bool&out, bool&out)"));
			bool OutA = false;
			bool OutB = true;
			Invoker.AddArgRef(OutA).AddArgRef(OutB);
			Invoker.Execute();
			TestRunner->TestEqual(TEXT("multiple &out parameter A"), OutA, true);
			TestRunner->TestEqual(TEXT("multiple &out parameter B"), OutB, false);
		}
	}

	// -------------------------------------------------------------------------
	// Function parameters: &inout (read-write parameter).
	// -------------------------------------------------------------------------
	TEST_METHOD(FunctionParametersInOut)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovBoolFunc_ParamInOut", ASTEST_AS(R"AS(
		void ToggleBool(bool&inout x)
		{
			x = !x;
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
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("void ToggleBool(bool&inout)"));
			bool Value = true;
			Invoker.AddArgRef(Value);
			Invoker.Execute();
			TestRunner->TestEqual(TEXT("bool &inout parameter toggles true->false"), Value, false);
		}
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("void ToggleBool(bool&inout)"));
			bool Value = false;
			Invoker.AddArgRef(Value);
			Invoker.Execute();
			TestRunner->TestEqual(TEXT("bool &inout parameter toggles false->true"), Value, true);
		}
	}

	// -------------------------------------------------------------------------
	// Function return values.
	// -------------------------------------------------------------------------
	TEST_METHOD(FunctionReturnValues)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovBoolFunc_Return", ASTEST_AS(R"AS(
		bool ReturnTrue()
		{
			return true;
		}

		bool ReturnFalse()
		{
			return false;
		}

		bool ReturnExpression()
		{
			return 5 > 3;
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
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("bool ReturnTrue()"));
			const bool Result = Invoker.ExecuteAndGet<bool>(false);
			TestRunner->TestEqual(TEXT("bool return true"), Result, true);
		}
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("bool ReturnFalse()"));
			const bool Result = Invoker.ExecuteAndGet<bool>(true);
			TestRunner->TestEqual(TEXT("bool return false"), Result, false);
		}
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("bool ReturnExpression()"));
			const bool Result = Invoker.ExecuteAndGet<bool>(false);
			TestRunner->TestEqual(TEXT("bool return expression"), Result, true);
		}
	}

	// -------------------------------------------------------------------------
	// Function default parameters.
	// -------------------------------------------------------------------------
	TEST_METHOD(FunctionDefaultParameters)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovBoolFunc_Default", ASTEST_AS(R"AS(
		bool AndWithDefault(bool a, bool b = true)
		{
			return a && b;
		}

		bool OrWithDefault(bool a, bool b = false)
		{
			return a || b;
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
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("bool AndWithDefault(bool, bool)"));
			Invoker.AddArg(true).AddArg(true);
			const bool Result = Invoker.ExecuteAndGet<bool>(false);
			TestRunner->TestEqual(TEXT("default parameter when explicitly provided"), Result, true);
		}

		// Call with default (omit second argument)
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("bool AndWithDefault(bool)"));
			Invoker.AddArg(true);
			const bool Result = Invoker.ExecuteAndGet<bool>(false);
			TestRunner->TestEqual(TEXT("default parameter used (true)"), Result, true);
		}

		// Call with default false
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("bool OrWithDefault(bool)"));
			Invoker.AddArg(true);
			const bool Result = Invoker.ExecuteAndGet<bool>(false);
			TestRunner->TestEqual(TEXT("default parameter used (false)"), Result, true);
		}
	}

	// -------------------------------------------------------------------------
	// UFUNCTION parameter and return values.
	// -------------------------------------------------------------------------
	TEST_METHOD(UFunctionParametersAndReturn)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageBoolFunction_UFUNCTION"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageBoolFunctionUFUNCTION.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageBoolFunctionActor : AActor
			{
				UFUNCTION()
				bool AndOperation(bool a, bool b)
				{
					return a && b;
				}

				UFUNCTION()
				bool NotOperation(bool x)
				{
					return !x;
				}

				UFUNCTION()
				void WriteOut(bool&out result)
				{
					result = true;
				}
			}
			)AS"),
			TEXT("ACoverageBoolFunctionActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Bool-function UFUNCTION actor class should compile")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Bool-function UFUNCTION actor should spawn")));
		BeginPlayActor(Engine, *Actor);

		// UFUNCTION with bool parameters and return
		{
			FFunctionInvoker Invoker(*TestRunner, Actor, TEXT("AndOperation"));
			Invoker.AddParam(true);
			Invoker.AddParam(true);
			const bool Result = Invoker.CallAndReturn<bool>();
			TestRunner->TestEqual(TEXT("UFUNCTION bool parameters and return"), Result, true);
		}

		// UFUNCTION with single bool
		{
			FFunctionInvoker Invoker(*TestRunner, Actor, TEXT("NotOperation"));
			Invoker.AddParam(true);
			const bool Result = Invoker.CallAndReturn<bool>();
			TestRunner->TestEqual(TEXT("UFUNCTION bool NOT operation"), Result, false);
		}

		// UFUNCTION with &out parameter
		{
			FFunctionInvoker Invoker(*TestRunner, Actor, TEXT("WriteOut"));
			bool OutValue = false;
			Invoker.AddParam(false);
			Invoker.Call();
			Invoker.ReadParamAfterCall(0, OutValue);
			TestRunner->TestEqual(TEXT("UFUNCTION bool &out parameter"), OutValue, true);
		}
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
