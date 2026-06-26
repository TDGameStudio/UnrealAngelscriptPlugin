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
// AngelscriptCoverageFVectorFunctionTests
// -----------------------------------------------------------------------------
// Coverage for AngelScript FVector *function usage* -- parameters, return values,
// defaults, and UFUNCTION.
//
// Test patterns:
//   - Pattern B: Global functions via FASGlobalFunctionInvoker
//   - Pattern C: UFUNCTION via FFunctionInvoker
// -----------------------------------------------------------------------------

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptFunctionalTestUtils;

TEST_CLASS_WITH_FLAGS(FAngelscriptCoverageFVectorFunctionTest,
	"Angelscript.TestModule.Coverage.FVectorFunction",
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

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovFVectorFunc_ParamValue", ASTEST_AS(R"AS(
		FVector AcceptVector(FVector v)
		{
			return v * 2.0;
		}

		float AcceptTwoVectors(FVector a, FVector b)
		{
			return FVector::Distance(a, b);
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
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("FVector AcceptVector(FVector)"));
			FVector Input = FVector(1, 2, 3);
			Invoker.AddArgRef(Input);
			FVector Result;
			ASSERT_THAT(IsTrue(Invoker.ExecuteAndExtractStruct(Result)));
			TestRunner->TestEqual(TEXT("FVector value parameter"), Result, FVector(2, 4, 6));
		}
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("float AcceptTwoVectors(FVector, FVector)"));
			FVector A = FVector(0, 0, 0);
			FVector B = FVector(3, 4, 0);
			Invoker.AddArgRef(A).AddArgRef(B);
			float Result = Invoker.ExecuteAndGet<float>(0.0f);
			TestRunner->TestTrue(TEXT("two FVector value parameters"), FMath::IsNearlyEqual(Result, 5.0f, 0.001f));
		}
	}

	// -------------------------------------------------------------------------
	// Function parameters: &in (const reference).
	// -------------------------------------------------------------------------
	TEST_METHOD(FunctionParametersIn)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovFVectorFunc_ParamIn", ASTEST_AS(R"AS(
		float AcceptVectorIn(FVector&in v)
		{
			return v.Length();
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
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("float AcceptVectorIn(FVector&in)"));
			FVector Input = FVector(3, 4, 0);
			Invoker.AddArgRef(Input);
			float Result = Invoker.ExecuteAndGet<float>(0.0f);
			TestRunner->TestTrue(TEXT("FVector &in parameter"), FMath::IsNearlyEqual(Result, 5.0f, 0.001f));
		}
	}

	// -------------------------------------------------------------------------
	// Function parameters: &out (output parameter).
	// -------------------------------------------------------------------------
	TEST_METHOD(FunctionParametersOut)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovFVectorFunc_ParamOut", ASTEST_AS(R"AS(
		void WriteVector(FVector&out v)
		{
			v = FVector(10, 20, 30);
		}

		void WriteMultipleVectors(FVector&out a, FVector&out b)
		{
			a = FVector::ForwardVector;
			b = FVector::UpVector;
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
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("void WriteVector(FVector&out)"));
			FVector OutValue;
			Invoker.AddArgRef(OutValue);
			Invoker.Execute();
			TestRunner->TestEqual(TEXT("FVector &out parameter"), OutValue, FVector(10, 20, 30));
		}
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("void WriteMultipleVectors(FVector&out, FVector&out)"));
			FVector OutA, OutB;
			Invoker.AddArgRef(OutA).AddArgRef(OutB);
			Invoker.Execute();
			TestRunner->TestEqual(TEXT("multiple &out parameter A"), OutA, FVector::ForwardVector);
			TestRunner->TestEqual(TEXT("multiple &out parameter B"), OutB, FVector::UpVector);
		}
	}

	// -------------------------------------------------------------------------
	// Function parameters: &inout (read-write parameter).
	// -------------------------------------------------------------------------
	TEST_METHOD(FunctionParametersInOut)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovFVectorFunc_ParamInOut", ASTEST_AS(R"AS(
		void ScaleVector(FVector&inout v, float scale)
		{
			v = v * scale;
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
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("void ScaleVector(FVector&inout, float)"));
			FVector Value = FVector(1, 2, 3);
			Invoker.AddArgRef(Value).AddArg(2.0f);
			Invoker.Execute();
			TestRunner->TestEqual(TEXT("FVector &inout parameter scales vector"), Value, FVector(2, 4, 6));
		}
	}

	// -------------------------------------------------------------------------
	// Function return values.
	// -------------------------------------------------------------------------
	TEST_METHOD(FunctionReturnValues)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovFVectorFunc_Return", ASTEST_AS(R"AS(
		FVector ReturnForwardVector()
		{
			return FVector::ForwardVector;
		}

		FVector ReturnCustomVector()
		{
			return FVector(5, 10, 15);
		}

		FVector ReturnComputedVector()
		{
			FVector a = FVector(1, 2, 3);
			FVector b = FVector(4, 5, 6);
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
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("FVector ReturnForwardVector()"));
			FVector Result;
			ASSERT_THAT(IsTrue(Invoker.ExecuteAndExtractStruct(Result)));
			TestRunner->TestEqual(TEXT("FVector return ForwardVector"), Result, FVector::ForwardVector);
		}
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("FVector ReturnCustomVector()"));
			FVector Result;
			ASSERT_THAT(IsTrue(Invoker.ExecuteAndExtractStruct(Result)));
			TestRunner->TestEqual(TEXT("FVector return custom"), Result, FVector(5, 10, 15));
		}
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("FVector ReturnComputedVector()"));
			FVector Result;
			ASSERT_THAT(IsTrue(Invoker.ExecuteAndExtractStruct(Result)));
			TestRunner->TestEqual(TEXT("FVector return computed"), Result, FVector(5, 7, 9));
		}
	}

	// -------------------------------------------------------------------------
	// Function default parameters.
	// -------------------------------------------------------------------------
	TEST_METHOD(FunctionDefaultParameters)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovFVectorFunc_Default", ASTEST_AS(R"AS(
		FVector AddWithDefault(FVector a, FVector b = FVector::OneVector)
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
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("FVector AddWithDefault(FVector, FVector)"));
			FVector Arg1 = FVector(1, 2, 3);
			FVector Arg2 = FVector(4, 5, 6);
			Invoker.AddArgRef(Arg1).AddArgRef(Arg2);
			FVector Result;
			ASSERT_THAT(IsTrue(Invoker.ExecuteAndExtractStruct(Result)));
			TestRunner->TestEqual(TEXT("default parameter when explicitly provided"), Result, FVector(5, 7, 9));
		}

		// Call with default (omit second argument)
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("FVector AddWithDefault(FVector)"));
			FVector Arg1 = FVector(1, 2, 3);
			Invoker.AddArgRef(Arg1);
			FVector Result;
			ASSERT_THAT(IsTrue(Invoker.ExecuteAndExtractStruct(Result)));
			TestRunner->TestEqual(TEXT("default parameter used"), Result, FVector(2, 3, 4));
		}
	}

	// -------------------------------------------------------------------------
	// UFUNCTION parameter and return values.
	// -------------------------------------------------------------------------
	TEST_METHOD(UFunctionParametersAndReturn)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageFVectorFunction_UFUNCTION"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageFVectorFunctionUFUNCTION.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageFVectorFunctionActor : AActor
			{
				UFUNCTION()
				FVector AddVectors(FVector a, FVector b)
				{
					return a + b;
				}

				UFUNCTION()
				float VectorLength(FVector v)
				{
					return v.Length();
				}

				UFUNCTION()
				void WriteOut(FVector&out result)
				{
					result = FVector::UpVector;
				}
			}
			)AS"),
			TEXT("ACoverageFVectorFunctionActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("FVector-function UFUNCTION actor class should compile")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("FVector-function UFUNCTION actor should spawn")));
		BeginPlayActor(Engine, *Actor);

		// UFUNCTION with FVector parameters and return
		{
			FFunctionInvoker Invoker(*TestRunner, Actor, TEXT("AddVectors"));
			Invoker.AddParam(FVector(1, 2, 3));
			Invoker.AddParam(FVector(4, 5, 6));
			const FVector Result = Invoker.CallAndReturn<FVector>();
			TestRunner->TestEqual(TEXT("UFUNCTION FVector parameters and return"), Result, FVector(5, 7, 9));
		}

		// UFUNCTION with FVector parameter and float return
		{
			FFunctionInvoker Invoker(*TestRunner, Actor, TEXT("VectorLength"));
			Invoker.AddParam(FVector(3, 4, 0));
			const float Result = Invoker.CallAndReturn<float>();
			TestRunner->TestTrue(TEXT("UFUNCTION FVector to float"), FMath::IsNearlyEqual(Result, 5.0f, 0.001f));
		}

		// UFUNCTION with &out parameter
		{
			FFunctionInvoker Invoker(*TestRunner, Actor, TEXT("WriteOut"));
			FVector OutValue;
			Invoker.AddParam(FVector::ZeroVector);
			Invoker.Call();
			Invoker.ReadParamAfterCall(0, OutValue);
			TestRunner->TestEqual(TEXT("UFUNCTION FVector &out parameter"), OutValue, FVector::UpVector);
		}
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
