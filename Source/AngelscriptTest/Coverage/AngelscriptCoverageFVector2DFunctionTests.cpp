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
// AngelscriptCoverageFVector2DFunctionTests
// -----------------------------------------------------------------------------
// Coverage for AngelScript FVector2D *function usage* -- parameters, return values,
// defaults, and UFUNCTION.
//
// Test patterns:
//   - Pattern B: Global functions via FASGlobalFunctionInvoker
//   - Pattern C: UFUNCTION via FFunctionInvoker
// -----------------------------------------------------------------------------

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptFunctionalTestUtils;

TEST_CLASS_WITH_FLAGS(FAngelscriptCoverageFVector2DFunctionTest,
	"Angelscript.TestModule.Coverage.FVector2DFunction",
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

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovFVector2DFunc_ParamValue", ASTEST_AS(R"AS(
		FVector2D AcceptVector(FVector2D v)
		{
			return v * 2.0;
		}

		float AcceptTwoVectors(FVector2D a, FVector2D b)
		{
			return a.Distance(b);
		}
		)AS"));
		ON_SCOPE_EXIT
		{
			if (Module != nullptr)
			{
				Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
			}
		};
		ASSERT_THAT(IsNotNull(Module, TEXT("FVector2D value parameter module should compile")));
		if (Module == nullptr)
		{
			return;
		}

		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("FVector2D AcceptVector(FVector2D)"));
			FVector2D Input = FVector2D(5, 10);
			Invoker.AddArgRef(Input);
			FVector2D Result;
			ASSERT_THAT(IsTrue(Invoker.ExecuteAndExtractStruct(Result)));
			TestRunner->TestEqual(TEXT("FVector2D value parameter"), Result, FVector2D(10, 20));
		}
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("float AcceptTwoVectors(FVector2D, FVector2D)"));
			FVector2D A = FVector2D(0, 0);
			FVector2D B = FVector2D(3, 4);
			Invoker.AddArgRef(A).AddArgRef(B);
			double Result = Invoker.ExecuteAndGet<double>(0.0);
			TestRunner->TestTrue(TEXT("two FVector2D value parameters"), FMath::IsNearlyEqual(Result, 5.0, 0.001));
		}

		{
			const TArray<FString> ExpectedDiagnostics = {
				TEXT("No matching signatures to 'FVector2D::Distance(FVector2D, FVector2D)'")
			};
			ASSERT_THAT(IsTrue(CompileAndExpectFailure(
				*TestRunner,
				Engine,
				TEXT("ASCovFVector2DFunc_StaticDistanceUnsupported"),
				ASTEST_AS(R"AS(
				float TryStaticDistance(FVector2D A, FVector2D B)
				{
					return FVector2D::Distance(A, B);
				}
				)AS"),
				TEXT("FVector2D static Distance should remain an explicit unsupported boundary; use the member method"),
				MakeArrayView(ExpectedDiagnostics))));
		}
	}

	// -------------------------------------------------------------------------
	// Function parameters: &in (const reference).
	// -------------------------------------------------------------------------
	TEST_METHOD(FunctionParametersIn)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovFVector2DFunc_ParamIn", ASTEST_AS(R"AS(
		float AcceptVectorIn(FVector2D&in v)
		{
			return v.Size();
		}
		)AS"));
		ON_SCOPE_EXIT
		{
			if (Module != nullptr)
			{
				Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
			}
		};
		ASSERT_THAT(IsNotNull(Module, TEXT("FVector2D &in parameter module should compile")));
		if (Module == nullptr)
		{
			return;
		}

		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("float AcceptVectorIn(FVector2D&in)"));
			FVector2D Input = FVector2D(3, 4);
			Invoker.AddArgRef(Input);
			double Result = Invoker.ExecuteAndGet<double>(0.0);
			TestRunner->TestTrue(TEXT("FVector2D &in parameter"), FMath::IsNearlyEqual(Result, 5.0, 0.001));
		}

		{
			const TArray<FString> ExpectedDiagnostics = {
				TEXT("No matching signatures to 'FVector2D::Length()'")
			};
			ASSERT_THAT(IsTrue(CompileAndExpectFailure(
				*TestRunner,
				Engine,
				TEXT("ASCovFVector2DFunc_ParamInLengthUnsupported"),
				ASTEST_AS(R"AS(
				float TryVectorLength(FVector2D&in v)
				{
					return v.Length();
				}
				)AS"),
				TEXT("FVector2D.Length() should remain an explicit unsupported boundary; use Size()"),
				MakeArrayView(ExpectedDiagnostics))));
		}
	}

	// -------------------------------------------------------------------------
	// Function parameters: &out (output parameter).
	// -------------------------------------------------------------------------
	TEST_METHOD(FunctionParametersOut)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovFVector2DFunc_ParamOut", ASTEST_AS(R"AS(
		void WriteVector(FVector2D&out v)
		{
			v = FVector2D(100, 200);
		}

		void WriteMultipleVectors(FVector2D&out a, FVector2D&out b)
		{
			a = FVector2D(1, 0);
			b = FVector2D(0, 1);
		}
		)AS"));
		ON_SCOPE_EXIT
		{
			if (Module != nullptr)
			{
				Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
			}
		};
		ASSERT_THAT(IsNotNull(Module, TEXT("FVector2D &out parameter module should compile")));
		if (Module == nullptr)
		{
			return;
		}

		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("void WriteVector(FVector2D&out)"));
			FVector2D OutValue;
			Invoker.AddArgRef(OutValue);
			Invoker.Execute();
			TestRunner->TestEqual(TEXT("FVector2D &out parameter"), OutValue, FVector2D(100, 200));
		}
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("void WriteMultipleVectors(FVector2D&out, FVector2D&out)"));
			FVector2D OutA, OutB;
			Invoker.AddArgRef(OutA).AddArgRef(OutB);
			Invoker.Execute();
			TestRunner->TestEqual(TEXT("multiple &out parameter A"), OutA, FVector2D(1, 0));
			TestRunner->TestEqual(TEXT("multiple &out parameter B"), OutB, FVector2D(0, 1));
		}
	}

	// -------------------------------------------------------------------------
	// Function parameters: &inout (read-write parameter).
	// -------------------------------------------------------------------------
	TEST_METHOD(FunctionParametersInOut)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovFVector2DFunc_ParamInOut", ASTEST_AS(R"AS(
		void ScaleVector(FVector2D&inout v, float scale)
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
		ASSERT_THAT(IsNotNull(Module, TEXT("FVector2D &inout parameter module should compile")));
		if (Module == nullptr)
		{
			return;
		}

		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("void ScaleVector(FVector2D&inout, float)"));
			FVector2D Value = FVector2D(10, 20);
			Invoker.AddArgRef(Value).AddArg(3.0);
			ASSERT_THAT(IsTrue(Invoker.Execute(), TEXT("FVector2D &inout scale should execute")));
			ASSERT_THAT(IsTrue(Value.Equals(FVector2D(30, 60), 0.001), TEXT("FVector2D &inout parameter should scale vector")));
		}
	}

	// -------------------------------------------------------------------------
	// Function return values.
	// -------------------------------------------------------------------------
	TEST_METHOD(FunctionReturnValues)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovFVector2DFunc_Return", ASTEST_AS(R"AS(
		FVector2D ReturnZeroVector()
		{
			return FVector2D::ZeroVector;
		}

		FVector2D ReturnCustomVector()
		{
			return FVector2D(50, 75);
		}

		FVector2D ReturnComputedVector()
		{
			FVector2D a = FVector2D(10, 20);
			FVector2D b = FVector2D(5, 10);
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
		ASSERT_THAT(IsNotNull(Module, TEXT("FVector2D return value module should compile")));
		if (Module == nullptr)
		{
			return;
		}

		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("FVector2D ReturnZeroVector()"));
			FVector2D Result;
			ASSERT_THAT(IsTrue(Invoker.ExecuteAndExtractStruct(Result)));
			TestRunner->TestEqual(TEXT("FVector2D return ZeroVector"), Result, FVector2D::ZeroVector);
		}
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("FVector2D ReturnCustomVector()"));
			FVector2D Result;
			ASSERT_THAT(IsTrue(Invoker.ExecuteAndExtractStruct(Result)));
			TestRunner->TestEqual(TEXT("FVector2D return custom"), Result, FVector2D(50, 75));
		}
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("FVector2D ReturnComputedVector()"));
			FVector2D Result;
			ASSERT_THAT(IsTrue(Invoker.ExecuteAndExtractStruct(Result)));
			TestRunner->TestEqual(TEXT("FVector2D return computed"), Result, FVector2D(15, 30));
		}
	}

	// -------------------------------------------------------------------------
	// Function default parameters.
	// -------------------------------------------------------------------------
	TEST_METHOD(FunctionDefaultParameters)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovFVector2DFunc_Default", ASTEST_AS(R"AS(
		FVector2D AddWithDefault(FVector2D a, FVector2D b = FVector2D::UnitVector)
		{
			return a + b;
		}

		FVector2D AddUsingDefault(FVector2D a)
		{
			return AddWithDefault(a);
		}
		)AS"));
		ON_SCOPE_EXIT
		{
			if (Module != nullptr)
			{
				Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
			}
		};
		ASSERT_THAT(IsNotNull(Module, TEXT("FVector2D default parameter module should compile")));
		if (Module == nullptr)
		{
			return;
		}

		// Call with all arguments
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("FVector2D AddWithDefault(FVector2D, FVector2D)"));
			FVector2D Arg1 = FVector2D(10, 20);
			FVector2D Arg2 = FVector2D(5, 10);
			Invoker.AddArgRef(Arg1).AddArgRef(Arg2);
			FVector2D Result;
			ASSERT_THAT(IsTrue(Invoker.ExecuteAndExtractStruct(Result)));
			TestRunner->TestEqual(TEXT("default parameter when explicitly provided"), Result, FVector2D(15, 30));
		}

		// Call with default (omit second argument)
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("FVector2D AddUsingDefault(FVector2D)"));
			FVector2D Arg1 = FVector2D(10, 20);
			Invoker.AddArgRef(Arg1);
			FVector2D Result;
			ASSERT_THAT(IsTrue(Invoker.ExecuteAndExtractStruct(Result)));
			TestRunner->TestEqual(TEXT("default parameter used through script call"), Result, FVector2D(11, 21));
		}
	}

	// -------------------------------------------------------------------------
	// UFUNCTION parameter and return values.
	// -------------------------------------------------------------------------
	TEST_METHOD(UFunctionParametersAndReturn)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageFVector2DFunction_UFUNCTION"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageFVector2DFunctionUFUNCTION.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageFVector2DFunctionActor : AActor
			{
				UFUNCTION()
				FVector2D AddVectors(FVector2D a, FVector2D b)
				{
					return a + b;
				}

				UFUNCTION()
				float VectorLength(FVector2D v)
				{
					return v.Size();
				}

				UFUNCTION()
				void WriteOut(FVector2D&out result)
				{
					result = FVector2D(99, 88);
				}
			}
			)AS"),
			TEXT("ACoverageFVector2DFunctionActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("FVector2D-function UFUNCTION actor class should compile")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("FVector2D-function UFUNCTION actor should spawn")));
		BeginPlayActor(Engine, *Actor);

		// UFUNCTION with FVector2D parameters and return
		{
			FFunctionInvoker Invoker(*TestRunner, Actor, TEXT("AddVectors"));
			Invoker.AddParam(FVector2D(10, 20));
			Invoker.AddParam(FVector2D(5, 10));
			const FVector2D Result = Invoker.CallAndReturn<FVector2D>();
			TestRunner->TestEqual(TEXT("UFUNCTION FVector2D parameters and return"), Result, FVector2D(15, 30));
		}

		// UFUNCTION with FVector2D parameter and float return
		{
			FFunctionInvoker Invoker(*TestRunner, Actor, TEXT("VectorLength"));
			Invoker.AddParam(FVector2D(3, 4));
			const double Result = Invoker.CallAndReturn<double>();
			TestRunner->TestTrue(TEXT("UFUNCTION FVector2D to float"), FMath::IsNearlyEqual(Result, 5.0, 0.001));
		}

		// UFUNCTION with &out parameter
		{
			FFunctionInvoker Invoker(*TestRunner, Actor, TEXT("WriteOut"));
			FVector2D OutValue;
			Invoker.AddParam(FVector2D::ZeroVector);
			Invoker.Call();
			Invoker.ReadParamAfterCall(0, OutValue);
			TestRunner->TestEqual(TEXT("UFUNCTION FVector2D &out parameter"), OutValue, FVector2D(99, 88));
		}
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
