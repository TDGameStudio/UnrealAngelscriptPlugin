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
// AngelscriptCoverageFRotatorFunctionTests
// -----------------------------------------------------------------------------
// Coverage for AngelScript FRotator *function usage* -- parameters, return values,
// defaults, and UFUNCTION.
//
// Test patterns:
//   - Pattern B: Global functions via FASGlobalFunctionInvoker
//   - Pattern C: UFUNCTION via FFunctionInvoker
// -----------------------------------------------------------------------------

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptFunctionalTestUtils;

TEST_CLASS_WITH_FLAGS(FAngelscriptCoverageFRotatorFunctionTest,
	"Angelscript.TestModule.Coverage.FRotatorFunction",
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

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovFRotatorFunc_ParamValue", ASTEST_AS(R"AS(
		FRotator AcceptRotator(FRotator r)
		{
			return r * 2.0;
		}

		FRotator AcceptTwoRotators(FRotator a, FRotator b)
		{
			return a + b;
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
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("FRotator AcceptRotator(FRotator)"));
			FRotator Input = FRotator(10, 20, 30);
			Invoker.AddArgRef(Input);
			FRotator Result;
			ASSERT_THAT(IsTrue(Invoker.ExecuteAndExtractStruct(Result)));
			TestRunner->TestEqual(TEXT("FRotator value parameter"), Result, FRotator(20, 40, 60));
		}
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("FRotator AcceptTwoRotators(FRotator, FRotator)"));
			FRotator A = FRotator(10, 20, 30);
			FRotator B = FRotator(5, 10, 15);
			Invoker.AddArgRef(A).AddArgRef(B);
			FRotator Result;
			ASSERT_THAT(IsTrue(Invoker.ExecuteAndExtractStruct(Result)));
			TestRunner->TestEqual(TEXT("two FRotator value parameters"), Result, FRotator(15, 30, 45));
		}
	}

	// -------------------------------------------------------------------------
	// Function parameters: &in (const reference).
	// -------------------------------------------------------------------------
	TEST_METHOD(FunctionParametersIn)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovFRotatorFunc_ParamIn", ASTEST_AS(R"AS(
		FVector AcceptRotatorIn(FRotator&in r)
		{
			return r.Vector();
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
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("FVector AcceptRotatorIn(FRotator&in)"));
			FRotator Input = FRotator(0, 90, 0);
			Invoker.AddArgRef(Input);
			FVector Result;
			ASSERT_THAT(IsTrue(Invoker.ExecuteAndExtractStruct(Result)));
			TestRunner->TestTrue(TEXT("FRotator &in parameter"), Result.Equals(FRotator(0, 90, 0).Vector(), 0.001));
		}
	}

	// -------------------------------------------------------------------------
	// Function parameters: &out (output parameter).
	// -------------------------------------------------------------------------
	TEST_METHOD(FunctionParametersOut)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovFRotatorFunc_ParamOut", ASTEST_AS(R"AS(
		void WriteRotator(FRotator&out r)
		{
			r = FRotator(45, 90, 180);
		}

		void WriteMultipleRotators(FRotator&out a, FRotator&out b)
		{
			a = FRotator::ZeroRotator;
			b = FRotator(10, 20, 30);
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
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("void WriteRotator(FRotator&out)"));
			FRotator OutValue;
			Invoker.AddArgRef(OutValue);
			Invoker.Execute();
			TestRunner->TestEqual(TEXT("FRotator &out parameter"), OutValue, FRotator(45, 90, 180));
		}
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("void WriteMultipleRotators(FRotator&out, FRotator&out)"));
			FRotator OutA, OutB;
			Invoker.AddArgRef(OutA).AddArgRef(OutB);
			Invoker.Execute();
			TestRunner->TestEqual(TEXT("multiple &out parameter A"), OutA, FRotator::ZeroRotator);
			TestRunner->TestEqual(TEXT("multiple &out parameter B"), OutB, FRotator(10, 20, 30));
		}
	}

	// -------------------------------------------------------------------------
	// Function parameters: &inout (read-write parameter).
	// -------------------------------------------------------------------------
	TEST_METHOD(FunctionParametersInOut)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovFRotatorFunc_ParamInOut", ASTEST_AS(R"AS(
		void ScaleRotator(FRotator&inout r, float scale)
		{
			r = r * scale;
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
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("void ScaleRotator(FRotator&inout, float)"));
			FRotator Value = FRotator(10, 20, 30);
			Invoker.AddArgRef(Value).AddArg(2.0f);
			Invoker.Execute();
			TestRunner->TestEqual(TEXT("FRotator &inout parameter scales rotator"), Value, FRotator(20, 40, 60));
		}
	}

	// -------------------------------------------------------------------------
	// Function return values.
	// -------------------------------------------------------------------------
	TEST_METHOD(FunctionReturnValues)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovFRotatorFunc_Return", ASTEST_AS(R"AS(
		FRotator ReturnZeroRotator()
		{
			return FRotator::ZeroRotator;
		}

		FRotator ReturnCustomRotator()
		{
			return FRotator(45, 90, 135);
		}

		FRotator ReturnComputedRotator()
		{
			FRotator a = FRotator(10, 20, 30);
			FRotator b = FRotator(5, 10, 15);
			return a + b;
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
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("FRotator ReturnZeroRotator()"));
			FRotator Result;
			ASSERT_THAT(IsTrue(Invoker.ExecuteAndExtractStruct(Result)));
			TestRunner->TestEqual(TEXT("FRotator return ZeroRotator"), Result, FRotator::ZeroRotator);
		}
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("FRotator ReturnCustomRotator()"));
			FRotator Result;
			ASSERT_THAT(IsTrue(Invoker.ExecuteAndExtractStruct(Result)));
			TestRunner->TestEqual(TEXT("FRotator return custom"), Result, FRotator(45, 90, 135));
		}
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("FRotator ReturnComputedRotator()"));
			FRotator Result;
			ASSERT_THAT(IsTrue(Invoker.ExecuteAndExtractStruct(Result)));
			TestRunner->TestEqual(TEXT("FRotator return computed"), Result, FRotator(15, 30, 45));
		}
	}

	// -------------------------------------------------------------------------
	// Function default parameters.
	// -------------------------------------------------------------------------
	TEST_METHOD(FunctionDefaultParameters)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovFRotatorFunc_Default", ASTEST_AS(R"AS(
		FRotator AddWithDefault(FRotator a, FRotator b = FRotator::ZeroRotator)
		{
			return a + b;
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
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("FRotator AddWithDefault(FRotator, FRotator)"));
			FRotator Arg1 = FRotator(10, 20, 30);
			FRotator Arg2 = FRotator(5, 10, 15);
			Invoker.AddArgRef(Arg1).AddArgRef(Arg2);
			FRotator Result;
			ASSERT_THAT(IsTrue(Invoker.ExecuteAndExtractStruct(Result)));
			TestRunner->TestEqual(TEXT("default parameter when explicitly provided"), Result, FRotator(15, 30, 45));
		}

		// Call with default (omit second argument)
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("FRotator AddWithDefault(FRotator)"));
			FRotator Arg1 = FRotator(10, 20, 30);
			Invoker.AddArgRef(Arg1);
			FRotator Result;
			ASSERT_THAT(IsTrue(Invoker.ExecuteAndExtractStruct(Result)));
			TestRunner->TestEqual(TEXT("default parameter used"), Result, FRotator(10, 20, 30));
		}
	}

	// -------------------------------------------------------------------------
	// UFUNCTION parameter and return values.
	// -------------------------------------------------------------------------
	TEST_METHOD(UFunctionParametersAndReturn)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageFRotatorFunction_UFUNCTION"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageFRotatorFunctionUFUNCTION.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageFRotatorFunctionActor : AActor
			{
				UFUNCTION()
				FRotator AddRotators(FRotator a, FRotator b)
				{
					return a + b;
				}

				UFUNCTION()
				FVector RotatorToVector(FRotator r)
				{
					return r.Vector();
				}

				UFUNCTION()
				void WriteOut(FRotator&out result)
				{
					result = FRotator(30, 60, 90);
				}
			}
			)AS"),
			TEXT("ACoverageFRotatorFunctionActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("FRotator-function UFUNCTION actor class should compile")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("FRotator-function UFUNCTION actor should spawn")));
		BeginPlayActor(Engine, *Actor);

		// UFUNCTION with FRotator parameters and return
		{
			FFunctionInvoker Invoker(*TestRunner, Actor, TEXT("AddRotators"));
			Invoker.AddParam(FRotator(10, 20, 30));
			Invoker.AddParam(FRotator(5, 10, 15));
			const FRotator Result = Invoker.CallAndReturn<FRotator>();
			TestRunner->TestEqual(TEXT("UFUNCTION FRotator parameters and return"), Result, FRotator(15, 30, 45));
		}

		// UFUNCTION with FRotator parameter and FVector return
		{
			FFunctionInvoker Invoker(*TestRunner, Actor, TEXT("RotatorToVector"));
			Invoker.AddParam(FRotator(0, 90, 0));
			const FVector Result = Invoker.CallAndReturn<FVector>();
			TestRunner->TestTrue(TEXT("UFUNCTION FRotator to FVector"), Result.Equals(FRotator(0, 90, 0).Vector(), 0.001));
		}

		// UFUNCTION with &out parameter
		{
			FFunctionInvoker Invoker(*TestRunner, Actor, TEXT("WriteOut"));
			FRotator OutValue;
			Invoker.AddParam(FRotator::ZeroRotator);
			Invoker.Call();
			Invoker.ReadParamAfterCall(0, OutValue);
			TestRunner->TestEqual(TEXT("UFUNCTION FRotator &out parameter"), OutValue, FRotator(30, 60, 90));
		}
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
