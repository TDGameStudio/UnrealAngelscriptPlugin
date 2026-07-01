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

#if WITH_ANGELSCRIPT_UNITTESTS

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
			Invoker.AddArgRef(Value).AddArg(2.0);
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

		FRotator AddWithImplicitDefault(FRotator a)
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
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("FRotator AddWithImplicitDefault(FRotator)"));
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

	TEST_METHOD(FunctionConstArrayAndStoredMemberRoundTrip)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovFRotatorFunc_ArrayGlobal", ASTEST_AS(R"AS(
		TArray<FRotator> MakeRotatorArray()
		{
			TArray<FRotator> Values;
			Values.Add(FRotator::ZeroRotator);
			Values.Add(FRotator(0, 90, 0));
			Values.Add(FRotator(10, 20, 30).GetNormalized());
			return Values;
		}

		FRotator SumRotatorArray(const TArray<FRotator>&in Values)
		{
			FRotator Total = FRotator::ZeroRotator;
			for (int Index = 0; Index < Values.Num(); ++Index)
			{
				Total += Values[Index];
			}
			return Total;
		}

		int ValidateArrayReturn()
		{
			TArray<FRotator> Values = MakeRotatorArray();
			return Values.Num() == 3
				&& Values[0].Equals(FRotator::ZeroRotator, 0.001)
				&& Values[1].Equals(FRotator(0, 90, 0), 0.001)
				&& Values[2].Equals(FRotator(10, 20, 30), 0.001)
				? 1 : 0;
		}

		int ValidateArrayInput()
		{
			TArray<FRotator> Values;
			Values.Add(FRotator(1, 2, 3));
			Values.Add(FRotator(4, 5, 6));
			return SumRotatorArray(Values).Equals(FRotator(5, 7, 9), 0.001) ? 1 : 0;
		}

		FRotator ReadConstRotator(const FRotator&in Value)
		{
			return Value.GetInverse();
		}
		)AS"));
		ON_SCOPE_EXIT
		{
			if (Module != nullptr)
			{
				Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
			}
		};
		ASSERT_THAT(IsNotNull(Module, TEXT("FRotator array global function module should compile")));
		if (Module == nullptr)
		{
			return;
		}

		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("int ValidateArrayReturn()"));
			ASSERT_THAT(AreEqual(1, Invoker.ExecuteAndGet<int32>(0), TEXT("TArray<FRotator> return should preserve values inside AS")));
		}
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("int ValidateArrayInput()"));
			ASSERT_THAT(AreEqual(1, Invoker.ExecuteAndGet<int32>(0), TEXT("const TArray<FRotator>&in should pass values inside AS")));
		}
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("FRotator ReadConstRotator(const FRotator&in)"));
			FRotator Input(0, 90, 0);
			Invoker.AddArgRef(Input);
			FRotator Result;
			ASSERT_THAT(IsTrue(Invoker.ExecuteAndExtractStruct(Result), TEXT("const FRotator&in global function should execute")));
			ASSERT_THAT(IsTrue(Result.Equals(Input.GetInverse(), 0.001), TEXT("const FRotator&in global function should read input without mutation")));
			ASSERT_THAT(IsTrue(Input.Equals(FRotator(0, 90, 0), 0.001), TEXT("const FRotator&in global function should leave caller value unchanged")));
		}

		static const FName ModuleName(TEXT("ASCoverageFRotatorFunction_ArrayUFUNCTION"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageFRotatorFunctionArrayUFUNCTION.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageFRotatorFunctionArrayActor : AActor
			{
				UPROPERTY()
				FRotator StoredRotator;

				UPROPERTY()
				TArray<FRotator> StoredRotators;

				UPROPERTY()
				int LastArrayCount = 0;

				UFUNCTION()
				FRotator StoreAndReturn(FRotator Value)
				{
					StoredRotator = Value;
					return StoredRotator.GetInverse();
				}

				UFUNCTION()
				int AcceptRotatorArray(const TArray<FRotator>&in Values)
				{
					LastArrayCount = Values.Num();
					StoredRotator = FRotator::ZeroRotator;
					for (FRotator Value : Values)
					{
						StoredRotator += Value;
					}
					return LastArrayCount;
				}

				UFUNCTION()
				TArray<FRotator> MakeRotatorArray(FRotator First, FRotator Second)
				{
					TArray<FRotator> Result;
					Result.Add(First);
					Result.Add(Second);
					StoredRotators = Result;
					return Result;
				}

				UFUNCTION()
				void FillOutRotatorArray(TArray<FRotator>&out Result)
				{
					Result.Add(FRotator::ZeroRotator);
					Result.Add(FRotator(30, 60, 90));
					StoredRotators = Result;
				}
			}
			)AS"),
			TEXT("ACoverageFRotatorFunctionArrayActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("FRotator array UFUNCTION actor class should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("FRotator array UFUNCTION actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		{
			const FRotator Input(0, 90, 0);
			FFunctionInvoker Invoker(*TestRunner, Actor, TEXT("StoreAndReturn"));
			Invoker.AddParam<FRotator>(Input);
			const FRotator Result = Invoker.CallAndReturn<FRotator>(FRotator::ZeroRotator);
			ASSERT_THAT(IsTrue(Result.Equals(Input.GetInverse(), 0.001), TEXT("FRotator UFUNCTION return should carry inverse result")));

			FRotator Stored = FRotator::ZeroRotator;
			ASSERT_THAT(IsTrue(GetStructByPath<FRotator>(*TestRunner, Actor, TEXT("StoredRotator"), Stored), TEXT("StoredRotator should be readable through reflection")));
			ASSERT_THAT(IsTrue(Stored.Equals(Input, 0.001), TEXT("FRotator UFUNCTION should store value parameter in class member")));
		}
		{
			TArray<FRotator> Values;
			Values.Add(FRotator(1, 2, 3));
			Values.Add(FRotator(4, 5, 6));

			FFunctionInvoker Invoker(*TestRunner, Actor, TEXT("AcceptRotatorArray"));
			Invoker.AddParam<TArray<FRotator>>(Values);
			const int32 Result = Invoker.CallAndReturn<int32>(INDEX_NONE);
			ASSERT_THAT(AreEqual(2, Result, TEXT("TArray<FRotator> UFUNCTION parameter should report element count")));

			FRotator Stored = FRotator::ZeroRotator;
			ASSERT_THAT(IsTrue(GetStructByPath<FRotator>(*TestRunner, Actor, TEXT("StoredRotator"), Stored), TEXT("StoredRotator should be readable after array parameter call")));
			ASSERT_THAT(IsTrue(Stored.Equals(FRotator(5, 7, 9), 0.001), TEXT("TArray<FRotator> UFUNCTION parameter should preserve element values")));
		}
		{
			const FRotator First = FRotator::ZeroRotator;
			const FRotator Second(10, 20, 30);
			FFunctionInvoker Invoker(*TestRunner, Actor, TEXT("MakeRotatorArray"));
			Invoker.AddParam<FRotator>(First);
			Invoker.AddParam<FRotator>(Second);
			const TArray<FRotator> Result = Invoker.CallAndReturn<TArray<FRotator>>(TArray<FRotator>());
			ASSERT_THAT(AreEqual(2, Result.Num(), TEXT("TArray<FRotator> UFUNCTION return should contain two elements")));
			ASSERT_THAT(IsTrue(Result.IsValidIndex(1), TEXT("TArray<FRotator> UFUNCTION return should expose second element")));
			if (!Result.IsValidIndex(1))
			{
				return;
			}
			ASSERT_THAT(IsTrue(Result[1].Equals(Second, 0.001), TEXT("TArray<FRotator> UFUNCTION return should preserve second rotator")));

			int32 StoredCount = 0;
			ASSERT_THAT(IsTrue(GetArrayNumByPath(*TestRunner, Actor, TEXT("StoredRotators"), StoredCount), TEXT("StoredRotators should expose returned TArray<FRotator> count through reflection")));
			ASSERT_THAT(AreEqual(2, StoredCount, TEXT("StoredRotators should contain returned elements")));
		}
		{
			FFunctionInvoker Invoker(*TestRunner, Actor, TEXT("FillOutRotatorArray"));
			Invoker.AddParam<TArray<FRotator>>(TArray<FRotator>());
			ASSERT_THAT(IsTrue(Invoker.Call(), TEXT("TArray<FRotator>&out UFUNCTION should execute")));

			TArray<FRotator> OutValues;
			ASSERT_THAT(IsTrue(Invoker.ReadParamAfterCall<TArray<FRotator>>(0, OutValues), TEXT("TArray<FRotator>&out UFUNCTION should copy output buffer back")));
			ASSERT_THAT(AreEqual(2, OutValues.Num(), TEXT("TArray<FRotator>&out should contain two elements")));
			ASSERT_THAT(IsTrue(OutValues.IsValidIndex(1), TEXT("TArray<FRotator>&out should expose second element")));
			if (!OutValues.IsValidIndex(1))
			{
				return;
			}
			ASSERT_THAT(IsTrue(OutValues[1].Equals(FRotator(30, 60, 90), 0.001), TEXT("TArray<FRotator>&out should preserve written rotator")));
		}
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
