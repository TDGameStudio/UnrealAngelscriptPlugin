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
			FTransform Modified = t;
			Modified.AddToTranslation(FVector(100, 0, 0));
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
		ASSERT_THAT(IsNotNull(Module, TEXT("FTransform value parameter module should compile")));
		if (Module == nullptr)
		{
			return;
		}

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
			return t.GetLocation();
		}
		)AS"));
		ON_SCOPE_EXIT
		{
			if (Module != nullptr)
			{
				Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
			}
		};
		ASSERT_THAT(IsNotNull(Module, TEXT("FTransform &in parameter module should compile")));
		if (Module == nullptr)
		{
			return;
		}

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
		ASSERT_THAT(IsNotNull(Module, TEXT("FTransform &out parameter module should compile")));
		if (Module == nullptr)
		{
			return;
		}

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
		void AssignScaleTransform(FTransform&inout t)
		{
			t = FTransform(t.GetRotation(), t.GetLocation(), FVector(2, 2, 2));
		}

		void AssignTranslateTransform(FTransform&inout t, FVector offset)
		{
			t = FTransform(t.GetRotation(), t.GetLocation() + offset, t.GetScale3D());
		}

		void MutateScaleTransform(FTransform&inout t)
		{
			t.SetScale3D(FVector(2, 2, 2));
		}
		)AS"));
		ON_SCOPE_EXIT
		{
			if (Module != nullptr)
			{
				Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
			}
		};
		ASSERT_THAT(IsNotNull(Module, TEXT("FTransform &inout parameter module should compile")));
		if (Module == nullptr)
		{
			return;
		}

		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("void AssignScaleTransform(FTransform&inout)"));
			FTransform Value(FQuat::Identity, FVector::ZeroVector, FVector(1, 1, 1));
			Invoker.AddArgRef(Value);
			ASSERT_THAT(IsTrue(Invoker.Execute(), TEXT("FTransform assignment &inout scale should execute")));
			ASSERT_THAT(IsTrue(Value.GetScale3D().Equals(FVector(2, 2, 2), 0.01), TEXT("FTransform &inout assignment should scale caller value")));
		}
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("void AssignTranslateTransform(FTransform&inout, FVector)"));
			FTransform Value(FVector(10, 20, 30));
			FVector Offset(5, 10, 15);
			Invoker.AddArgRef(Value).AddArgRef(Offset);
			ASSERT_THAT(IsTrue(Invoker.Execute(), TEXT("FTransform assignment &inout translation should execute")));
			ASSERT_THAT(IsTrue(Value.GetLocation().Equals(FVector(15, 30, 45), 0.01), TEXT("FTransform &inout assignment should translate caller value")));
		}
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("void MutateScaleTransform(FTransform&inout)"));
			FTransform Value(FQuat::Identity, FVector::ZeroVector, FVector(1, 1, 1));
			Invoker.AddArgRef(Value);
			ASSERT_THAT(IsTrue(Invoker.Execute(), TEXT("FTransform mutator &inout scale should execute")));
			ASSERT_THAT(IsTrue(Value.GetScale3D().Equals(FVector(2, 2, 2), 0.01), TEXT("FTransform &inout mutator should scale caller value")));
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
		ASSERT_THAT(IsNotNull(Module, TEXT("FTransform return value module should compile")));
		if (Module == nullptr)
		{
			return;
		}

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

		FTransform ComposeUsingDefault(FTransform a)
		{
			return ComposeWithDefault(a);
		}
		)AS"));
		ON_SCOPE_EXIT
		{
			if (Module != nullptr)
			{
				Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
			}
		};
		ASSERT_THAT(IsNotNull(Module, TEXT("FTransform default parameter module should compile")));
		if (Module == nullptr)
		{
			return;
		}

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

		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("FTransform ComposeUsingDefault(FTransform)"));
			FTransform Arg1(FVector(100, 200, 300));
			Invoker.AddArgRef(Arg1);
			FTransform Result;
			ASSERT_THAT(IsTrue(Invoker.ExecuteAndExtractStruct(Result)));
			FTransform Expected = Arg1 * FTransform::Identity;
			TestRunner->TestTrue(TEXT("default parameter used through script call"), Result.Equals(Expected, 0.01));
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
					return t.GetLocation();
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

	TEST_METHOD(FunctionConstArrayAndStoredMemberRoundTrip)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovFTransformFunc_ArrayGlobal", ASTEST_AS(R"AS(
		TArray<FTransform> MakeTransformArray()
		{
			TArray<FTransform> Values;
			Values.Add(FTransform::Identity);
			Values.Add(FTransform(FVector(10, 20, 30)));
			Values.Add(FTransform(FQuat::Identity, FVector(1, 2, 3), FVector(2, 3, 4)));
			return Values;
		}

		FTransform CombineTransformArray(const TArray<FTransform>&in Values)
		{
			FTransform Result = FTransform::Identity;
			for (int Index = 0; Index < Values.Num(); ++Index)
			{
				Result *= Values[Index];
			}
			return Result;
		}

		int ValidateArrayReturn()
		{
			TArray<FTransform> Values = MakeTransformArray();
			return Values.Num() == 3
				&& Values[0].Equals(FTransform::Identity, 0.001)
				&& Values[1].GetLocation().Equals(FVector(10, 20, 30), 0.001)
				&& Values[2].GetScale3D().Equals(FVector(2, 3, 4), 0.001)
				? 1 : 0;
		}

		int ValidateArrayInput()
		{
			TArray<FTransform> Values;
			Values.Add(FTransform(FVector(1, 0, 0)));
			Values.Add(FTransform(FVector(0, 2, 0)));
			return CombineTransformArray(Values).GetLocation().Equals(FVector(1, 2, 0), 0.001) ? 1 : 0;
		}

		FTransform ReadConstTransform(const FTransform&in Value)
		{
			return Value.Inverse();
		}
		)AS"));
		ON_SCOPE_EXIT
		{
			if (Module != nullptr)
			{
				Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
			}
		};
		ASSERT_THAT(IsNotNull(Module, TEXT("FTransform array global function module should compile")));
		if (Module == nullptr)
		{
			return;
		}

		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("int ValidateArrayReturn()"));
			ASSERT_THAT(AreEqual(1, Invoker.ExecuteAndGet<int32>(0), TEXT("TArray<FTransform> return should preserve values inside AS")));
		}
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("int ValidateArrayInput()"));
			ASSERT_THAT(AreEqual(1, Invoker.ExecuteAndGet<int32>(0), TEXT("const TArray<FTransform>&in should pass values inside AS")));
		}
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("FTransform ReadConstTransform(const FTransform&in)"));
			FTransform Input(FQuat::Identity, FVector(10, 20, 30), FVector(2, 2, 2));
			Invoker.AddArgRef(Input);
			FTransform Result;
			ASSERT_THAT(IsTrue(Invoker.ExecuteAndExtractStruct(Result), TEXT("const FTransform&in global function should execute")));
			ASSERT_THAT(IsTrue(Result.Equals(Input.Inverse(), 0.001), TEXT("const FTransform&in global function should read input without mutation")));
			ASSERT_THAT(IsTrue(Input.Equals(FTransform(FQuat::Identity, FVector(10, 20, 30), FVector(2, 2, 2)), 0.001), TEXT("const FTransform&in global function should leave caller value unchanged")));
		}

		static const FName ModuleName(TEXT("ASCoverageFTransformFunction_ArrayUFUNCTION"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageFTransformFunctionArrayUFUNCTION.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageFTransformFunctionArrayActor : AActor
			{
				UPROPERTY()
				FTransform StoredTransform;

				UPROPERTY()
				TArray<FTransform> StoredTransforms;

				UPROPERTY()
				int LastArrayCount = 0;

				UFUNCTION()
				FTransform StoreAndReturnInverse(FTransform Value)
				{
					StoredTransform = Value;
					return StoredTransform.Inverse();
				}

				UFUNCTION()
				int AcceptTransformArray(const TArray<FTransform>&in Values)
				{
					LastArrayCount = Values.Num();
					StoredTransform = FTransform::Identity;
					for (FTransform Value : Values)
					{
						StoredTransform *= Value;
					}
					return LastArrayCount;
				}

				UFUNCTION()
				TArray<FTransform> MakeTransformArray(FTransform First, FTransform Second)
				{
					TArray<FTransform> Result;
					Result.Add(First);
					Result.Add(Second);
					StoredTransforms = Result;
					return Result;
				}

				UFUNCTION()
				void FillOutTransformArray(TArray<FTransform>&out Result)
				{
					Result.Add(FTransform::Identity);
					Result.Add(FTransform(FQuat::Identity, FVector(30, 60, 90), FVector(2, 2, 2)));
					StoredTransforms = Result;
				}
			}
			)AS"),
			TEXT("ACoverageFTransformFunctionArrayActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("FTransform array UFUNCTION actor class should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("FTransform array UFUNCTION actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		{
			const FTransform Input(FQuat::Identity, FVector(10, 20, 30), FVector(2, 2, 2));
			FFunctionInvoker Invoker(*TestRunner, Actor, TEXT("StoreAndReturnInverse"));
			Invoker.AddParam<FTransform>(Input);
			const FTransform Result = Invoker.CallAndReturn<FTransform>(FTransform::Identity);
			ASSERT_THAT(IsTrue(Result.Equals(Input.Inverse(), 0.001), TEXT("FTransform UFUNCTION return should carry inverse result")));

			FTransform Stored = FTransform::Identity;
			ASSERT_THAT(IsTrue(GetStructByPath<FTransform>(*TestRunner, Actor, TEXT("StoredTransform"), Stored), TEXT("StoredTransform should be readable through reflection")));
			ASSERT_THAT(IsTrue(Stored.Equals(Input, 0.001), TEXT("FTransform UFUNCTION should store value parameter in class member")));
		}
		{
			TArray<FTransform> Values;
			Values.Add(FTransform(FVector(1, 0, 0)));
			Values.Add(FTransform(FVector(0, 2, 0)));

			FFunctionInvoker Invoker(*TestRunner, Actor, TEXT("AcceptTransformArray"));
			Invoker.AddParam<TArray<FTransform>>(Values);
			const int32 Result = Invoker.CallAndReturn<int32>(INDEX_NONE);
			ASSERT_THAT(AreEqual(2, Result, TEXT("TArray<FTransform> UFUNCTION parameter should report element count")));

			FTransform Stored = FTransform::Identity;
			ASSERT_THAT(IsTrue(GetStructByPath<FTransform>(*TestRunner, Actor, TEXT("StoredTransform"), Stored), TEXT("StoredTransform should be readable after array parameter call")));
			ASSERT_THAT(IsTrue(Stored.Equals(FTransform(FVector(1, 2, 0)), 0.001), TEXT("TArray<FTransform> UFUNCTION parameter should preserve element values")));
		}
		{
			const FTransform First = FTransform::Identity;
			const FTransform Second(FQuat::Identity, FVector(10, 20, 30), FVector(2, 3, 4));
			FFunctionInvoker Invoker(*TestRunner, Actor, TEXT("MakeTransformArray"));
			Invoker.AddParam<FTransform>(First);
			Invoker.AddParam<FTransform>(Second);
			const TArray<FTransform> Result = Invoker.CallAndReturn<TArray<FTransform>>(TArray<FTransform>());
			ASSERT_THAT(AreEqual(2, Result.Num(), TEXT("TArray<FTransform> UFUNCTION return should contain two elements")));
			ASSERT_THAT(IsTrue(Result.IsValidIndex(1), TEXT("TArray<FTransform> UFUNCTION return should expose second element")));
			if (!Result.IsValidIndex(1))
			{
				return;
			}
			ASSERT_THAT(IsTrue(Result[1].Equals(Second, 0.001), TEXT("TArray<FTransform> UFUNCTION return should preserve second transform")));

			int32 StoredCount = 0;
			ASSERT_THAT(IsTrue(GetArrayNumByPath(*TestRunner, Actor, TEXT("StoredTransforms"), StoredCount), TEXT("StoredTransforms should expose returned TArray<FTransform> count through reflection")));
			ASSERT_THAT(AreEqual(2, StoredCount, TEXT("StoredTransforms should contain returned elements")));
		}
		{
			FFunctionInvoker Invoker(*TestRunner, Actor, TEXT("FillOutTransformArray"));
			Invoker.AddParam<TArray<FTransform>>(TArray<FTransform>());
			ASSERT_THAT(IsTrue(Invoker.Call(), TEXT("TArray<FTransform>&out UFUNCTION should execute")));

			TArray<FTransform> OutValues;
			ASSERT_THAT(IsTrue(Invoker.ReadParamAfterCall<TArray<FTransform>>(0, OutValues), TEXT("TArray<FTransform>&out UFUNCTION should copy output buffer back")));
			ASSERT_THAT(AreEqual(2, OutValues.Num(), TEXT("TArray<FTransform>&out should contain two elements")));
			ASSERT_THAT(IsTrue(OutValues.IsValidIndex(1), TEXT("TArray<FTransform>&out should expose second element")));
			if (!OutValues.IsValidIndex(1))
			{
				return;
			}
			ASSERT_THAT(IsTrue(OutValues[1].Equals(FTransform(FQuat::Identity, FVector(30, 60, 90), FVector(2, 2, 2)), 0.001), TEXT("TArray<FTransform>&out should preserve written transform")));
		}
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
