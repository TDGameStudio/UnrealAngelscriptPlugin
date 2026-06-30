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
// AngelscriptCoverageFQuatFunctionTests
// -----------------------------------------------------------------------------
// Coverage for AngelScript FQuat *function usage* -- parameters, return values,
// defaults, and UFUNCTION.
//
// Test patterns:
//   - Pattern B: Global functions via FASGlobalFunctionInvoker
//   - Pattern C: UFUNCTION via FFunctionInvoker
// -----------------------------------------------------------------------------

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptFunctionalTestUtils;

TEST_CLASS_WITH_FLAGS(FAngelscriptCoverageFQuatFunctionTest,
	"Angelscript.TestModule.Coverage.FQuatFunction",
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

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovFQuatFunc_ParamValue", ASTEST_AS(R"AS(
		FQuat AcceptQuat(FQuat q)
		{
			return q.Inverse();
		}

		FQuat AcceptTwoQuats(FQuat a, FQuat b)
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

		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("FQuat AcceptQuat(FQuat)"));
			FQuat Input = FQuat(FRotator(0, 90, 0));
			Invoker.AddArgRef(Input);
			FQuat Result;
			ASSERT_THAT(IsTrue(Invoker.ExecuteAndExtractStruct(Result)));
			FQuat Expected = Input.Inverse();
			TestRunner->TestTrue(TEXT("FQuat value parameter"), Result.Equals(Expected, 0.01));
		}
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("FQuat AcceptTwoQuats(FQuat, FQuat)"));
			FQuat A = FQuat(FRotator(0, 45, 0));
			FQuat B = FQuat(FRotator(0, 45, 0));
			Invoker.AddArgRef(A).AddArgRef(B);
			FQuat Result;
			ASSERT_THAT(IsTrue(Invoker.ExecuteAndExtractStruct(Result)));
			FQuat Expected = A * B;
			TestRunner->TestTrue(TEXT("two FQuat value parameters"), Result.Equals(Expected, 0.01));
		}
	}

	// -------------------------------------------------------------------------
	// Function parameters: &in (const reference).
	// -------------------------------------------------------------------------
	TEST_METHOD(FunctionParametersIn)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovFQuatFunc_ParamIn", ASTEST_AS(R"AS(
		FVector AcceptQuatIn(FQuat&in q)
		{
			return q.GetAxisX();
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
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("FVector AcceptQuatIn(FQuat&in)"));
			FQuat Input = FQuat(FRotator(0, 90, 0));
			Invoker.AddArgRef(Input);
			FVector Result;
			ASSERT_THAT(IsTrue(Invoker.ExecuteAndExtractStruct(Result)));
			FVector Expected = Input.GetAxisX();
			TestRunner->TestTrue(TEXT("FQuat &in parameter"), Result.Equals(Expected, 0.01));
		}
	}

	// -------------------------------------------------------------------------
	// Function parameters: &out (output parameter).
	// -------------------------------------------------------------------------
	TEST_METHOD(FunctionParametersOut)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovFQuatFunc_ParamOut", ASTEST_AS(R"AS(
		void WriteQuat(FQuat&out q)
		{
			q = FQuat(FRotator(0, 90, 0));
		}

		void WriteMultipleQuats(FQuat&out a, FQuat&out b)
		{
			a = FQuat::Identity;
			b = FQuat(FRotator(45, 0, 0));
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
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("void WriteQuat(FQuat&out)"));
			FQuat OutValue;
			Invoker.AddArgRef(OutValue);
			Invoker.Execute();
			FQuat Expected = FQuat(FRotator(0, 90, 0));
			TestRunner->TestTrue(TEXT("FQuat &out parameter"), OutValue.Equals(Expected, 0.01));
		}
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("void WriteMultipleQuats(FQuat&out, FQuat&out)"));
			FQuat OutA, OutB;
			Invoker.AddArgRef(OutA).AddArgRef(OutB);
			Invoker.Execute();
			TestRunner->TestTrue(TEXT("multiple &out parameter A"), OutA.Equals(FQuat::Identity, 0.001));
			FQuat ExpectedB = FQuat(FRotator(45, 0, 0));
			TestRunner->TestTrue(TEXT("multiple &out parameter B"), OutB.Equals(ExpectedB, 0.01));
		}
	}

	// -------------------------------------------------------------------------
	// Function parameters: &inout (read-write parameter).
	// -------------------------------------------------------------------------
	TEST_METHOD(FunctionParametersInOut)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovFQuatFunc_ParamInOut", ASTEST_AS(R"AS(
		void InverseQuat(FQuat&inout q)
		{
			q = q.Inverse();
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
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("void InverseQuat(FQuat&inout)"));
			FQuat Value = FQuat(FRotator(0, 90, 0));
			FQuat Expected = Value.Inverse();
			Invoker.AddArgRef(Value);
			Invoker.Execute();
			TestRunner->TestTrue(TEXT("FQuat &inout parameter inverses quat"), Value.Equals(Expected, 0.01));
		}
	}

	// -------------------------------------------------------------------------
	// Function return values.
	// -------------------------------------------------------------------------
	TEST_METHOD(FunctionReturnValues)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovFQuatFunc_Return", ASTEST_AS(R"AS(
		FQuat ReturnIdentity()
		{
			return FQuat::Identity;
		}

		FQuat ReturnCustomQuat()
		{
			return FQuat(FRotator(0, 90, 0));
		}

		FQuat ReturnComputedQuat()
		{
			FQuat a = FQuat(FRotator(0, 45, 0));
			FQuat b = FQuat(FRotator(0, 45, 0));
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

		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("FQuat ReturnIdentity()"));
			FQuat Result;
			ASSERT_THAT(IsTrue(Invoker.ExecuteAndExtractStruct(Result)));
			TestRunner->TestTrue(TEXT("FQuat return Identity"), Result.Equals(FQuat::Identity, 0.001));
		}
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("FQuat ReturnCustomQuat()"));
			FQuat Result;
			ASSERT_THAT(IsTrue(Invoker.ExecuteAndExtractStruct(Result)));
			FQuat Expected = FQuat(FRotator(0, 90, 0));
			TestRunner->TestTrue(TEXT("FQuat return custom"), Result.Equals(Expected, 0.01));
		}
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("FQuat ReturnComputedQuat()"));
			FQuat Result;
			ASSERT_THAT(IsTrue(Invoker.ExecuteAndExtractStruct(Result)));
			FQuat a = FQuat(FRotator(0, 45, 0));
			FQuat b = FQuat(FRotator(0, 45, 0));
			FQuat Expected = a * b;
			TestRunner->TestTrue(TEXT("FQuat return computed"), Result.Equals(Expected, 0.01));
		}
	}

	// -------------------------------------------------------------------------
	// Function default parameters.
	// -------------------------------------------------------------------------
	TEST_METHOD(FunctionDefaultParameters)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovFQuatFunc_Default", ASTEST_AS(R"AS(
		FQuat MultiplyWithDefault(FQuat a, FQuat b = FQuat::Identity)
		{
			return a * b;
		}

		FQuat MultiplyWithImplicitDefault(FQuat a)
		{
			return MultiplyWithDefault(a);
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
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("FQuat MultiplyWithDefault(FQuat, FQuat)"));
			FQuat Arg1 = FQuat(FRotator(0, 45, 0));
			FQuat Arg2 = FQuat(FRotator(0, 45, 0));
			Invoker.AddArgRef(Arg1).AddArgRef(Arg2);
			FQuat Result;
			ASSERT_THAT(IsTrue(Invoker.ExecuteAndExtractStruct(Result)));
			FQuat Expected = Arg1 * Arg2;
			TestRunner->TestTrue(TEXT("default parameter when explicitly provided"), Result.Equals(Expected, 0.01));
		}

		// Call with default (omit second argument)
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("FQuat MultiplyWithImplicitDefault(FQuat)"));
			FQuat Arg1 = FQuat(FRotator(0, 90, 0));
			Invoker.AddArgRef(Arg1);
			FQuat Result;
			ASSERT_THAT(IsTrue(Invoker.ExecuteAndExtractStruct(Result)));
			TestRunner->TestTrue(TEXT("default parameter used (identity)"), Result.Equals(Arg1, 0.01));
		}
	}

	// -------------------------------------------------------------------------
	// UFUNCTION parameter and return values.
	// -------------------------------------------------------------------------
	TEST_METHOD(UFunctionParametersAndReturn)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageFQuatFunction_UFUNCTION"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageFQuatFunctionUFUNCTION.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageFQuatFunctionActor : AActor
			{
				UFUNCTION()
				FQuat MultiplyQuats(FQuat a, FQuat b)
				{
					return a * b;
				}

				UFUNCTION()
				FVector QuatToAxisX(FQuat q)
				{
					return q.GetAxisX();
				}

				UFUNCTION()
				void WriteOut(FQuat&out result)
				{
					result = FQuat(FRotator(0, 90, 0));
				}

				UFUNCTION()
				FRotator QuatToRotator(FQuat q)
				{
					return q.Rotator();
				}
			}
			)AS"),
			TEXT("ACoverageFQuatFunctionActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("FQuat-function UFUNCTION actor class should compile")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("FQuat-function UFUNCTION actor should spawn")));
		BeginPlayActor(Engine, *Actor);

		// UFUNCTION with FQuat parameters and return
		{
			FFunctionInvoker Invoker(*TestRunner, Actor, TEXT("MultiplyQuats"));
			FQuat A = FQuat(FRotator(0, 45, 0));
			FQuat B = FQuat(FRotator(0, 45, 0));
			Invoker.AddParam(A);
			Invoker.AddParam(B);
			const FQuat Result = Invoker.CallAndReturn<FQuat>();
			FQuat Expected = A * B;
			TestRunner->TestTrue(TEXT("UFUNCTION FQuat parameters and return"), Result.Equals(Expected, 0.01));
		}

		// UFUNCTION with FQuat parameter and FVector return
		{
			FFunctionInvoker Invoker(*TestRunner, Actor, TEXT("QuatToAxisX"));
			FQuat Input = FQuat(FRotator(0, 90, 0));
			Invoker.AddParam(Input);
			const FVector Result = Invoker.CallAndReturn<FVector>();
			FVector Expected = Input.GetAxisX();
			TestRunner->TestTrue(TEXT("UFUNCTION FQuat to FVector"), Result.Equals(Expected, 0.01));
		}

		// UFUNCTION with &out parameter
		{
			FFunctionInvoker Invoker(*TestRunner, Actor, TEXT("WriteOut"));
			FQuat OutValue;
			Invoker.AddParam(FQuat::Identity);
			Invoker.Call();
			Invoker.ReadParamAfterCall(0, OutValue);
			FQuat Expected = FQuat(FRotator(0, 90, 0));
			TestRunner->TestTrue(TEXT("UFUNCTION FQuat &out parameter"), OutValue.Equals(Expected, 0.01));
		}

		// UFUNCTION with FQuat parameter and FRotator return
		{
			FFunctionInvoker Invoker(*TestRunner, Actor, TEXT("QuatToRotator"));
			FQuat Input = FQuat(FRotator(0, 90, 0));
			Invoker.AddParam(Input);
			const FRotator Result = Invoker.CallAndReturn<FRotator>();
			TestRunner->TestTrue(TEXT("UFUNCTION FQuat to FRotator"), Result.Equals(FRotator(0, 90, 0), 0.1));
		}
	}

	TEST_METHOD(UFunctionConstArrayAndOutPaths)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageFQuatFunction_ConstArrayOut"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageFQuatFunctionConstArrayOut.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageFQuatFunctionConstArrayActor : AActor
			{
				UPROPERTY()
				FQuat StoredQuat = FQuat::Identity;

				UPROPERTY()
				TArray<FQuat> StoredQuats;

				UPROPERTY()
				FQuat LastArrayProduct = FQuat::Identity;

				UPROPERTY()
				int LastArrayCount = 0;

				UFUNCTION()
				double ReadConstQuat(const FQuat&in Value)
				{
					return Value.GetAngle();
				}

				UFUNCTION()
				FQuat StoreAndReturn(FQuat Value)
				{
					StoredQuat = Value;
					return StoredQuat.Inverse();
				}

				UFUNCTION()
				int AcceptQuatArray(const TArray<FQuat>&in Values)
				{
					LastArrayCount = Values.Num();
					LastArrayProduct = FQuat::Identity;
					for (FQuat Value : Values)
					{
						LastArrayProduct *= Value;
					}
					return LastArrayCount;
				}

				UFUNCTION()
				TArray<FQuat> MakeQuatArray(FQuat First, FQuat Second)
				{
					TArray<FQuat> Result;
					Result.Add(First);
					Result.Add(Second);
					StoredQuats = Result;
					return Result;
				}

				UFUNCTION()
				void FillOutQuatArray(TArray<FQuat>&out Result)
				{
					Result.Add(FQuat::Identity);
					Result.Add(FQuat(FVector::UpVector, 1.5707963267948966));
					StoredQuats = Result;
				}
			}
			)AS"),
			TEXT("ACoverageFQuatFunctionConstArrayActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("FQuat const/array UFUNCTION actor class should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("FQuat const/array UFUNCTION actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		{
			const FQuat Input = FQuat(FVector::UpVector, UE_DOUBLE_HALF_PI);
			FFunctionInvoker Invoker(*TestRunner, Actor, TEXT("ReadConstQuat"));
			Invoker.AddParam<FQuat>(Input);
			const double Result = Invoker.CallAndReturn<double>(0.0);
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Result, Input.GetAngle(), 0.001),
				TEXT("const FQuat&in UFUNCTION parameter should read quaternion angle")));
		}
		{
			const FQuat Input = FQuat(FVector::UpVector, UE_DOUBLE_HALF_PI);
			FFunctionInvoker Invoker(*TestRunner, Actor, TEXT("StoreAndReturn"));
			Invoker.AddParam<FQuat>(Input);
			const FQuat Result = Invoker.CallAndReturn<FQuat>(FQuat::Identity);
			ASSERT_THAT(IsTrue(Result.Equals(Input.Inverse(), 0.001),
				TEXT("FQuat UFUNCTION return should carry inverse result")));

			FQuat Stored = FQuat::Identity;
			ASSERT_THAT(IsTrue(GetStructByPath<FQuat>(*TestRunner, Actor, TEXT("StoredQuat"), Stored),
				TEXT("StoredQuat should be readable through reflection")));
			ASSERT_THAT(IsTrue(Stored.Equals(Input, 0.001),
				TEXT("FQuat UFUNCTION should store value parameter in class member")));
		}
		{
			TArray<FQuat> Values;
			Values.Add(FQuat(FVector::UpVector, UE_DOUBLE_HALF_PI * 0.5));
			Values.Add(FQuat(FVector::UpVector, UE_DOUBLE_HALF_PI * 0.5));

			FFunctionInvoker Invoker(*TestRunner, Actor, TEXT("AcceptQuatArray"));
			Invoker.AddParam<TArray<FQuat>>(Values);
			const int32 Result = Invoker.CallAndReturn<int32>(INDEX_NONE);
			ASSERT_THAT(AreEqual(2, Result, TEXT("TArray<FQuat> UFUNCTION parameter should report element count")));

			FQuat Product = FQuat::Identity;
			ASSERT_THAT(IsTrue(GetStructByPath<FQuat>(*TestRunner, Actor, TEXT("LastArrayProduct"), Product),
				TEXT("LastArrayProduct should be readable through reflection")));
			ASSERT_THAT(IsTrue(Product.Equals(Values[0] * Values[1], 0.001),
				TEXT("TArray<FQuat> UFUNCTION parameter should preserve element values")));
		}
		{
			const FQuat First = FQuat::Identity;
			const FQuat Second = FQuat(FVector::UpVector, UE_DOUBLE_HALF_PI);
			FFunctionInvoker Invoker(*TestRunner, Actor, TEXT("MakeQuatArray"));
			Invoker.AddParam<FQuat>(First);
			Invoker.AddParam<FQuat>(Second);
			const TArray<FQuat> Result = Invoker.CallAndReturn<TArray<FQuat>>(TArray<FQuat>());
			ASSERT_THAT(AreEqual(2, Result.Num(), TEXT("TArray<FQuat> UFUNCTION return should contain two elements")));
			ASSERT_THAT(IsTrue(Result.IsValidIndex(1), TEXT("TArray<FQuat> UFUNCTION return should expose second element")));
			if (!Result.IsValidIndex(1))
			{
				return;
			}
			ASSERT_THAT(IsTrue(Result[1].Equals(Second, 0.001),
				TEXT("TArray<FQuat> UFUNCTION return should preserve second quaternion")));

			int32 StoredCount = 0;
			ASSERT_THAT(IsTrue(GetArrayNumByPath(*TestRunner, Actor, TEXT("StoredQuats"), StoredCount),
				TEXT("StoredQuats should expose returned TArray<FQuat> count through reflection")));
			ASSERT_THAT(AreEqual(2, StoredCount, TEXT("StoredQuats should contain returned elements")));
		}
		{
			FFunctionInvoker Invoker(*TestRunner, Actor, TEXT("FillOutQuatArray"));
			Invoker.AddParam<TArray<FQuat>>(TArray<FQuat>());
			ASSERT_THAT(IsTrue(Invoker.Call(), TEXT("TArray<FQuat>&out UFUNCTION should execute")));

			TArray<FQuat> OutValues;
			ASSERT_THAT(IsTrue(Invoker.ReadParamAfterCall<TArray<FQuat>>(0, OutValues),
				TEXT("TArray<FQuat>&out UFUNCTION should copy output buffer back")));
			ASSERT_THAT(AreEqual(2, OutValues.Num(), TEXT("TArray<FQuat>&out should contain two elements")));
			ASSERT_THAT(IsTrue(OutValues.IsValidIndex(1), TEXT("TArray<FQuat>&out should expose second element")));
			if (!OutValues.IsValidIndex(1))
			{
				return;
			}
			ASSERT_THAT(IsTrue(OutValues[1].Equals(FQuat(FVector::UpVector, UE_DOUBLE_HALF_PI), 0.001),
				TEXT("TArray<FQuat>&out should preserve written quaternion")));
		}
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
