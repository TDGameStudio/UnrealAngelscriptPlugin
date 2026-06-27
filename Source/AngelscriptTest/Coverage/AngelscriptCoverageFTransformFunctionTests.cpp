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
// AngelscriptCoverageFTransformFunctionTests
// -----------------------------------------------------------------------------
// Coverage for AngelScript FTransform *function usage* -- parameters, return values,
// defaults, and UFUNCTION.
//
// Test patterns:
//   - Pattern B: Global functions via FASGlobalFunctionInvoker
//   - Pattern C: UFUNCTION via FFunctionInvoker
// -----------------------------------------------------------------------------

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptFunctionalTestUtils;

TEST_CLASS_WITH_FLAGS(FAngelscriptCoverageFTransformFunctionTest,
	"Angelscript.TestModule.Coverage.FTransformFunction",
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

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovFTransformFunc_ParamValue", ASTEST_AS(R"AS(
		FTransform AcceptTransform(FTransform t)
		{
			// Modify the transform
			FTransform Modified = t;
			Modified.Location = Modified.Location + FVector(100, 0, 0);
			return Modified;
		}

		FVector AcceptTwoTransforms(FTransform a, FTransform b)
		{
			FVector PosA = a.TransformPosition(FVector::ZeroVector);
			FVector PosB = b.TransformPosition(FVector::ZeroVector);
			return PosB - PosA;
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
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("FTransform AcceptTransform(FTransform)"));
			FTransform Input(FVector(10, 20, 30));
			Invoker.AddArgRef(Input);
			FTransform Result;
			ASSERT_THAT(IsTrue(Invoker.ExecuteAndExtractStruct(Result)));
			TestRunner->TestTrue(TEXT("FTransform value parameter"), Result.GetLocation().Equals(FVector(110, 20, 30), 0.01));
		}
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("FVector AcceptTwoTransforms(FTransform, FTransform)"));
			FTransform A(FVector(100, 0, 0));
			FTransform B(FVector(400, 0, 0));
			Invoker.AddArgRef(A).AddArgRef(B);
			FVector Result;
			ASSERT_THAT(IsTrue(Invoker.ExecuteAndExtractStruct(Result)));
			TestRunner->TestTrue(TEXT("two FTransform value parameters"), Result.Equals(FVector(300, 0, 0), 0.01));
		}
	}

	// -------------------------------------------------------------------------
	// Function parameters: &in (const reference).
	// -------------------------------------------------------------------------
	TEST_METHOD(FunctionParametersIn)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovFTransformFunc_ParamIn", ASTEST_AS(R"AS(
		FVector AcceptTransformIn(FTransform&in t)
		{
			return t.Location;
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
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("FVector AcceptTransformIn(FTransform&in)"));
			FTransform Input(FVector(50, 100, 150));
			Invoker.AddArgRef(Input);
			FVector Result;
			ASSERT_THAT(IsTrue(Invoker.ExecuteAndExtractStruct(Result)));
			TestRunner->TestTrue(TEXT("FTransform &in parameter"), Result.Equals(FVector(50, 100, 150), 0.01));
		}
	}

	// -------------------------------------------------------------------------
	// Function parameters: &out (output parameter).
	// -------------------------------------------------------------------------
	TEST_METHOD(FunctionParametersOut)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovFTransformFunc_ParamOut", ASTEST_AS(R"AS(
		void WriteTransform(FTransform&out t)
		{
			t = FTransform(FVector(100, 200, 300));
		}

		void WriteMultipleTransforms(FTransform&out a, FTransform&out b)
		{
			a = FTransform(FVector(10, 0, 0));
			b = FTransform(FVector(0, 20, 0));
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
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("void WriteTransform(FTransform&out)"));
			FTransform OutValue;
			Invoker.AddArgRef(OutValue);
			Invoker.Execute();
			TestRunner->TestTrue(TEXT("FTransform &out parameter"), OutValue.GetLocation().Equals(FVector(100, 200, 300), 0.01));
		}
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("void WriteMultipleTransforms(FTransform&out, FTransform&out)"));
			FTransform OutA, OutB;
			Invoker.AddArgRef(OutA).AddArgRef(OutB);
			Invoker.Execute();
			TestRunner->TestTrue(TEXT("multiple &out parameter A"), OutA.GetLocation().Equals(FVector(10, 0, 0), 0.01));
			TestRunner->TestTrue(TEXT("multiple &out parameter B"), OutB.GetLocation().Equals(FVector(0, 20, 0), 0.01));
		}
	}

	// -------------------------------------------------------------------------
	// Function parameters: &inout (read-write parameter).
	// -------------------------------------------------------------------------
	TEST_METHOD(FunctionParametersInOut)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovFTransformFunc_ParamInOut", ASTEST_AS(R"AS(
		void ScaleTransform(FTransform&inout t, float scale)
		{
			t.Scale3D = t.Scale3D * scale;
		}

		void TranslateTransform(FTransform&inout t, FVector offset)
		{
			t.Location = t.Location + offset;
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
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("void ScaleTransform(FTransform&inout, float)"));
			FTransform Value(FQuat::Identity, FVector::ZeroVector, FVector(1, 1, 1));
			Invoker.AddArgRef(Value).AddArg(2.0f);
			Invoker.Execute();
			TestRunner->TestTrue(TEXT("FTransform &inout parameter scales"), Value.GetScale3D().Equals(FVector(2, 2, 2), 0.01));
		}
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("void TranslateTransform(FTransform&inout, FVector)"));
			FTransform Value(FVector(10, 20, 30));
			FVector Offset(5, 10, 15);
			Invoker.AddArgRef(Value).AddArgRef(Offset);
			Invoker.Execute();
			TestRunner->TestTrue(TEXT("FTransform &inout parameter translates"), Value.GetLocation().Equals(FVector(15, 30, 45), 0.01));
		}
	}

	// -------------------------------------------------------------------------
	// Function return values.
	// -------------------------------------------------------------------------
	TEST_METHOD(FunctionReturnValues)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovFTransformFunc_Return", ASTEST_AS(R"AS(
		FTransform ReturnIdentity()
		{
			return FTransform::Identity;
		}

		FTransform ReturnCustomTransform()
		{
			return FTransform(FVector(50, 100, 150));
		}

		FTransform ReturnComputedTransform()
		{
			FTransform A = FTransform(FVector(100, 0, 0));
			FTransform B = FTransform(FVector(0, 100, 0));
			return A * B;
		}

		FTransform ReturnInverse()
		{
			FTransform T = FTransform(FVector(10, 20, 30));
			return T.Inverse();
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
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("FTransform ReturnIdentity()"));
			FTransform Result;
			ASSERT_THAT(IsTrue(Invoker.ExecuteAndExtractStruct(Result)));
			TestRunner->TestTrue(TEXT("FTransform return Identity"), Result.Equals(FTransform::Identity, 0.01));
		}
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("FTransform ReturnCustomTransform()"));
			FTransform Result;
			ASSERT_THAT(IsTrue(Invoker.ExecuteAndExtractStruct(Result)));
			TestRunner->TestTrue(TEXT("FTransform return custom"), Result.GetLocation().Equals(FVector(50, 100, 150), 0.01));
		}
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("FTransform ReturnComputedTransform()"));
			FTransform Result;
			ASSERT_THAT(IsTrue(Invoker.ExecuteAndExtractStruct(Result)));
			FTransform A(FVector(100, 0, 0));
			FTransform B(FVector(0, 100, 0));
			FTransform Expected = A * B;
			TestRunner->TestTrue(TEXT("FTransform return computed"), Result.Equals(Expected, 0.01));
		}
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("FTransform ReturnInverse()"));
			FTransform Result;
			ASSERT_THAT(IsTrue(Invoker.ExecuteAndExtractStruct(Result)));
			FTransform T(FVector(10, 20, 30));
			FTransform Expected = T.Inverse();
			TestRunner->TestTrue(TEXT("FTransform return inverse"), Result.Equals(Expected, 0.01));
		}
	}

	// -------------------------------------------------------------------------
	// Function default parameters.
	// -------------------------------------------------------------------------
	TEST_METHOD(FunctionDefaultParameters)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovFTransformFunc_Default", ASTEST_AS(R"AS(
		FTransform ComposeWithDefault(FTransform a, FTransform b = FTransform::Identity)
		{
			return a * b;
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
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("FTransform ComposeWithDefault(FTransform, FTransform)"));
			FTransform Arg1(FVector(100, 0, 0));
			FTransform Arg2(FVector(0, 100, 0));
			Invoker.AddArgRef(Arg1).AddArgRef(Arg2);
			FTransform Result;
			ASSERT_THAT(IsTrue(Invoker.ExecuteAndExtractStruct(Result)));
			FTransform Expected = Arg1 * Arg2;
			TestRunner->TestTrue(TEXT("default parameter when explicitly provided"), Result.Equals(Expected, 0.01));
		}

		// Call with default (omit second argument)
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("FTransform ComposeWithDefault(FTransform)"));
			FTransform Arg1(FVector(100, 200, 300));
			Invoker.AddArgRef(Arg1);
			FTransform Result;
			ASSERT_THAT(IsTrue(Invoker.ExecuteAndExtractStruct(Result)));
			FTransform Expected = Arg1 * FTransform::Identity;
			TestRunner->TestTrue(TEXT("default parameter used"), Result.Equals(Expected, 0.01));
		}
	}

	// -------------------------------------------------------------------------
	// UFUNCTION parameter and return values.
	// -------------------------------------------------------------------------
	TEST_METHOD(UFunctionParametersAndReturn)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageFTransformFunction_UFUNCTION"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageFTransformFunctionUFUNCTION.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageFTransformFunctionActor : AActor
			{
				UFUNCTION()
				FTransform ComposeTransforms(FTransform a, FTransform b)
				{
					return a * b;
				}

				UFUNCTION()
				FVector GetTransformLocation(FTransform t)
				{
					return t.Location;
				}

				UFUNCTION()
				void WriteTransformOut(FTransform&out result)
				{
					result = FTransform(FVector(10, 20, 30));
				}

				UFUNCTION()
				FVector TransformPoint(FTransform t, FVector point)
				{
					return t.TransformPosition(point);
				}
			}
			)AS"),
			TEXT("ACoverageFTransformFunctionActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("FTransform-function UFUNCTION actor class should compile")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("FTransform-function UFUNCTION actor should spawn")));
		BeginPlayActor(Engine, *Actor);

		// UFUNCTION with FTransform parameters and return
		{
			FFunctionInvoker Invoker(*TestRunner, Actor, TEXT("ComposeTransforms"));
			FTransform T1(FVector(100, 0, 0));
			FTransform T2(FVector(0, 100, 0));
			Invoker.AddParam(T1);
			Invoker.AddParam(T2);
			const FTransform Result = Invoker.CallAndReturn<FTransform>();
			FTransform Expected = T1 * T2;
			TestRunner->TestTrue(TEXT("UFUNCTION FTransform parameters and return"), Result.Equals(Expected, 0.01));
		}

		// UFUNCTION with FTransform parameter and FVector return
		{
			FFunctionInvoker Invoker(*TestRunner, Actor, TEXT("GetTransformLocation"));
			FTransform T(FVector(50, 100, 150));
			Invoker.AddParam(T);
			const FVector Result = Invoker.CallAndReturn<FVector>();
			TestRunner->TestTrue(TEXT("UFUNCTION FTransform to FVector"), Result.Equals(FVector(50, 100, 150), 0.01));
		}

		// UFUNCTION with &out parameter
		{
			FFunctionInvoker Invoker(*TestRunner, Actor, TEXT("WriteTransformOut"));
			FTransform OutValue;
			Invoker.AddParam(FTransform::Identity);
			Invoker.Call();
			Invoker.ReadParamAfterCall(0, OutValue);
			TestRunner->TestTrue(TEXT("UFUNCTION FTransform &out parameter"), OutValue.GetLocation().Equals(FVector(10, 20, 30), 0.01));
		}

		// UFUNCTION with FTransform and FVector parameters
		{
			FFunctionInvoker Invoker(*TestRunner, Actor, TEXT("TransformPoint"));
			FTransform T(FVector(100, 0, 0));
			FVector Point(10, 0, 0);
			Invoker.AddParam(T);
			Invoker.AddParam(Point);
			const FVector Result = Invoker.CallAndReturn<FVector>();
			FVector Expected = T.TransformPosition(Point);
			TestRunner->TestTrue(TEXT("UFUNCTION FTransform and FVector"), Result.Equals(Expected, 0.01));
		}
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
