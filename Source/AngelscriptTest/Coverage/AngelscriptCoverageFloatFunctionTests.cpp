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
			float AcceptFloat(float X)
			{
				return X + 1.5f;
			}

			double AcceptDouble(double X)
			{
				return X + 2.5;
			}
			)AS"));
		ON_SCOPE_EXIT
		{
			if (Module != nullptr)
			{
				Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
			}
		};

		ASSERT_THAT(IsNotNull(Module, TEXT("Float value-parameter module should compile")));
		if (Module == nullptr)
		{
			return;
		}

		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("float AcceptFloat(float)"));
			ASSERT_THAT(IsTrue(Invoker.IsValid(), TEXT("AcceptFloat should be invokable")));
			if (!Invoker.IsValid())
			{
				return;
			}
			Invoker.AddArg(10.5f);
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(12.0f, Invoker.CallAndReturn<float>(0.0f), 0.001f), TEXT("float value parameter should pass exact value")));
		}
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("double AcceptDouble(double)"));
			ASSERT_THAT(IsTrue(Invoker.IsValid(), TEXT("AcceptDouble should be invokable")));
			if (!Invoker.IsValid())
			{
				return;
			}
			Invoker.AddArg(20.5);
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(23.0, Invoker.CallAndReturn<double>(0.0), 0.001), TEXT("double value parameter should pass exact value")));
		}
	}

	TEST_METHOD(FunctionParametersIn)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovFloatFunc_ParamIn", ASTEST_AS(R"AS(
			float AcceptFloatIn(const float&in X)
			{
				return X * 2.0f;
			}

			double AcceptDoubleIn(const double&in X)
			{
				return X * 3.0;
			}
			)AS"));
		ON_SCOPE_EXIT
		{
			if (Module != nullptr)
			{
				Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
			}
		};

		ASSERT_THAT(IsNotNull(Module, TEXT("Float const&in parameter module should compile")));
		if (Module == nullptr)
		{
			return;
		}

		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("float AcceptFloatIn(const float&in)"));
			ASSERT_THAT(IsTrue(Invoker.IsValid(), TEXT("AcceptFloatIn should be invokable")));
			if (!Invoker.IsValid())
			{
				return;
			}
			const double Input = 5.5;
			Invoker.AddArgRef(Input);
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(11.0f, Invoker.CallAndReturn<float>(0.0f), 0.001f), TEXT("float const&in parameter should read input value")));
		}
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("double AcceptDoubleIn(const double&in)"));
			ASSERT_THAT(IsTrue(Invoker.IsValid(), TEXT("AcceptDoubleIn should be invokable")));
			if (!Invoker.IsValid())
			{
				return;
			}
			const double Input = 10.5;
			Invoker.AddArgRef(Input);
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(31.5, Invoker.CallAndReturn<double>(0.0), 0.001), TEXT("double const&in parameter should read input value")));
		}
	}

	TEST_METHOD(FunctionParametersOut)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovFloatFunc_ParamOut", ASTEST_AS(R"AS(
			void WriteFloat(float&out X)
			{
				X = 3.14159f;
			}

			void WriteDouble(double&out X)
			{
				X = 2.71828;
			}

			void WriteFloatPair(float Seed, float&out A, float&out B)
			{
				A = Seed + 1.0f;
				B = Seed + 2.0f;
			}

			void WriteDoublePair(double Seed, double&out A, double&out B)
			{
				A = Seed + 1.0;
				B = Seed + 2.0;
			}
			)AS"));
		ON_SCOPE_EXIT
		{
			if (Module != nullptr)
			{
				Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
			}
		};

		ASSERT_THAT(IsNotNull(Module, TEXT("Float out-parameter module should compile")));
		if (Module == nullptr)
		{
			return;
		}

		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("void WriteFloat(float&out)"));
			ASSERT_THAT(IsTrue(Invoker.IsValid(), TEXT("WriteFloat should be invokable")));
			if (!Invoker.IsValid())
			{
				return;
			}
			double OutValue = 0.0;
			Invoker.AddArgRef(OutValue);
			ASSERT_THAT(IsTrue(Invoker.Call(), TEXT("float &out function should execute")));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(3.14159, OutValue, 0.00001), TEXT("float &out parameter should copy double-backed output value")));
		}
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("void WriteDouble(double&out)"));
			ASSERT_THAT(IsTrue(Invoker.IsValid(), TEXT("WriteDouble should be invokable")));
			if (!Invoker.IsValid())
			{
				return;
			}
			double OutValue = 0.0;
			Invoker.AddArgRef(OutValue);
			ASSERT_THAT(IsTrue(Invoker.Call(), TEXT("double &out function should execute")));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(2.71828, OutValue, 0.00001), TEXT("double &out parameter should copy output value")));
		}
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("void WriteFloatPair(float, float&out, float&out)"));
			ASSERT_THAT(IsTrue(Invoker.IsValid(), TEXT("WriteFloatPair should be invokable")));
			if (!Invoker.IsValid())
			{
				return;
			}
			double A = 0.0;
			double B = 0.0;
			Invoker.AddArg(10.0f).AddArgRef(A).AddArgRef(B);
			ASSERT_THAT(IsTrue(Invoker.Call(), TEXT("float multi-&out function should execute")));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(11.0, A, 0.001), TEXT("first float &out parameter should preserve order in double-backed storage")));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(12.0, B, 0.001), TEXT("second float &out parameter should preserve order in double-backed storage")));
		}
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("void WriteDoublePair(double, double&out, double&out)"));
			ASSERT_THAT(IsTrue(Invoker.IsValid(), TEXT("WriteDoublePair should be invokable")));
			if (!Invoker.IsValid())
			{
				return;
			}
			double A = 0.0;
			double B = 0.0;
			Invoker.AddArg(20.0).AddArgRef(A).AddArgRef(B);
			ASSERT_THAT(IsTrue(Invoker.Call(), TEXT("double multi-&out function should execute")));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(21.0, A, 0.001), TEXT("first double &out parameter should preserve order")));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(22.0, B, 0.001), TEXT("second double &out parameter should preserve order")));
		}
	}

	TEST_METHOD(FunctionParametersInOut)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovFloatFunc_ParamInOut", ASTEST_AS(R"AS(
			void SquareFloat(float&inout X)
			{
				X = X * X;
			}

			void SquareDouble(double&inout X)
			{
				X = X * X;
			}
			)AS"));
		ON_SCOPE_EXIT
		{
			if (Module != nullptr)
			{
				Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
			}
		};

		ASSERT_THAT(IsNotNull(Module, TEXT("Float inout-parameter module should compile")));
		if (Module == nullptr)
		{
			return;
		}

		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("void SquareFloat(float&inout)"));
			ASSERT_THAT(IsTrue(Invoker.IsValid(), TEXT("SquareFloat should be invokable")));
			if (!Invoker.IsValid())
			{
				return;
			}
			double Value = 5.0;
			Invoker.AddArgRef(Value);
			ASSERT_THAT(IsTrue(Invoker.Call(), TEXT("float &inout function should execute")));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(25.0, Value, 0.001), TEXT("float &inout parameter should mutate double-backed storage")));
		}
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("void SquareDouble(double&inout)"));
			ASSERT_THAT(IsTrue(Invoker.IsValid(), TEXT("SquareDouble should be invokable")));
			if (!Invoker.IsValid())
			{
				return;
			}
			double Value = 10.0;
			Invoker.AddArgRef(Value);
			ASSERT_THAT(IsTrue(Invoker.Call(), TEXT("double &inout function should execute")));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(100.0, Value, 0.001), TEXT("double &inout parameter should mutate original value")));
		}
	}

	TEST_METHOD(FunctionReturnValues)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovFloatFunc_ReturnValues", ASTEST_AS(R"AS(
			float ReturnFloat()
			{
				return 42.25f;
			}

			double ReturnDouble()
			{
				return 84.5;
			}
			)AS"));
		ON_SCOPE_EXIT
		{
			if (Module != nullptr)
			{
				Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
			}
		};

		ASSERT_THAT(IsNotNull(Module, TEXT("Float return-value module should compile")));
		if (Module == nullptr)
		{
			return;
		}

		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("float ReturnFloat()"));
			ASSERT_THAT(IsTrue(Invoker.IsValid(), TEXT("ReturnFloat should be invokable")));
			if (!Invoker.IsValid())
			{
				return;
			}
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(42.25f, Invoker.CallAndReturn<float>(0.0f), 0.001f), TEXT("float return value should cross global invoker")));
		}
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("double ReturnDouble()"));
			ASSERT_THAT(IsTrue(Invoker.IsValid(), TEXT("ReturnDouble should be invokable")));
			if (!Invoker.IsValid())
			{
				return;
			}
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(84.5, Invoker.CallAndReturn<double>(0.0), 0.001), TEXT("double return value should cross global invoker")));
		}
	}

	TEST_METHOD(FunctionDefaultParameters)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovFloatFunc_DefaultParams", ASTEST_AS(R"AS(
			float AddFloatDefault(float X, float Y = 1.5f)
			{
				return X + Y;
			}

			double AddDoubleDefault(double X, double Y = 2.5)
			{
				return X + Y;
			}
			)AS"));
		ON_SCOPE_EXIT
		{
			if (Module != nullptr)
			{
				Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
			}
		};

		ASSERT_THAT(IsNotNull(Module, TEXT("Float default-parameter module should compile")));
		if (Module == nullptr)
		{
			return;
		}

		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("float AddFloatDefault(float)"));
			ASSERT_THAT(IsTrue(Invoker.IsValid(), TEXT("AddFloatDefault one-arg overload should be invokable")));
			if (!Invoker.IsValid())
			{
				return;
			}
			Invoker.AddArg(10.0f);
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(11.5f, Invoker.CallAndReturn<float>(0.0f), 0.001f), TEXT("float default parameter should use f-suffix literal")));
		}
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("float AddFloatDefault(float, float)"));
			ASSERT_THAT(IsTrue(Invoker.IsValid(), TEXT("AddFloatDefault two-arg overload should be invokable")));
			if (!Invoker.IsValid())
			{
				return;
			}
			Invoker.AddArg(10.0f).AddArg(4.0f);
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(14.0f, Invoker.CallAndReturn<float>(0.0f), 0.001f), TEXT("explicit float parameter should override default")));
		}
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("double AddDoubleDefault(double)"));
			ASSERT_THAT(IsTrue(Invoker.IsValid(), TEXT("AddDoubleDefault one-arg overload should be invokable")));
			if (!Invoker.IsValid())
			{
				return;
			}
			Invoker.AddArg(20.0);
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(22.5, Invoker.CallAndReturn<double>(0.0), 0.001), TEXT("double default parameter should use default double literal")));
		}
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("double AddDoubleDefault(double, double)"));
			ASSERT_THAT(IsTrue(Invoker.IsValid(), TEXT("AddDoubleDefault two-arg overload should be invokable")));
			if (!Invoker.IsValid())
			{
				return;
			}
			Invoker.AddArg(20.0).AddArg(5.0);
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(25.0, Invoker.CallAndReturn<double>(0.0), 0.001), TEXT("explicit double parameter should override default")));
		}
	}

	TEST_METHOD(FunctionOverloading)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovFloatFunc_Overloading", ASTEST_AS(R"AS(
			int ProcessFloat(float X, bool bUseFloatPath)
			{
				return bUseFloatPath ? int(X * 10.0f) + 1 : -1;
			}

			int ProcessDouble(double X, int Bias)
			{
				return int(X * 10.0) + Bias;
			}

			float ReturnFloatByPrecision(float X, bool bUseFloatPath)
			{
				return bUseFloatPath ? X + 1.0f : -1.0f;
			}

			double ReturnDoubleByPrecision(double X, int Bias)
			{
				return X + double(Bias);
			}
			)AS"));
		ON_SCOPE_EXIT
		{
			if (Module != nullptr)
			{
				Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
			}
		};

		ASSERT_THAT(IsNotNull(Module, TEXT("Float overload module should compile")));
		if (Module == nullptr)
		{
			return;
		}

		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("int ProcessFloat(float, bool)"));
			ASSERT_THAT(IsTrue(Invoker.IsValid(), TEXT("ProcessFloat should be invokable")));
			if (!Invoker.IsValid())
			{
				return;
			}
			Invoker.AddArg(4.0f).AddArg(true);
			ASSERT_THAT(AreEqual(41, Invoker.CallAndReturn<int32>(INDEX_NONE), TEXT("float parameter signature should resolve with explicit discriminator")));
		}
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("int ProcessDouble(double, int)"));
			ASSERT_THAT(IsTrue(Invoker.IsValid(), TEXT("ProcessDouble should be invokable")));
			if (!Invoker.IsValid())
			{
				return;
			}
			Invoker.AddArg(4.0).AddArg(2);
			ASSERT_THAT(AreEqual(42, Invoker.CallAndReturn<int32>(INDEX_NONE), TEXT("double parameter signature should resolve with explicit discriminator")));
		}
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("float ReturnFloatByPrecision(float, bool)"));
			ASSERT_THAT(IsTrue(Invoker.IsValid(), TEXT("ReturnFloatByPrecision should be invokable")));
			if (!Invoker.IsValid())
			{
				return;
			}
			Invoker.AddArg(10.0f).AddArg(true);
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(11.0f, Invoker.CallAndReturn<float>(0.0f), 0.001f), TEXT("float signature should return float value")));
		}
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("double ReturnDoubleByPrecision(double, int)"));
			ASSERT_THAT(IsTrue(Invoker.IsValid(), TEXT("ReturnDoubleByPrecision should be invokable")));
			if (!Invoker.IsValid())
			{
				return;
			}
			Invoker.AddArg(10.0).AddArg(2);
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(12.0, Invoker.CallAndReturn<double>(0.0), 0.001), TEXT("double signature should return double value")));
		}
	}

	TEST_METHOD(UFunctionParametersAndReturn)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCovFloatFunc_UFunction"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		const FString ScriptSource = ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageFloatFunctionActor : AActor
			{
				UFUNCTION()
				float AddFloats(float A, float B)
				{
					return A + B;
				}

				UFUNCTION()
				double MultiplyDoubles(double A, double B)
				{
					return A * B;
				}

				UFUNCTION()
				void WriteFloatOut(float Seed, float&out Result)
				{
					Result = Seed + 0.75f;
				}

				UFUNCTION()
				void WriteDoubleOut(double Seed, double&out Result)
				{
					Result = Seed + 1.25;
				}

				UFUNCTION()
				float MutateFloat(float&inout Value)
				{
					Value = Value * 2.0f;
					return Value + 1.0f;
				}

				UFUNCTION()
				double MutateDouble(double&inout Value)
				{
					Value = Value * 3.0;
					return Value + 1.0;
				}
			}
			)AS");

		ASSERT_THAT(IsTrue(CompileAnnotatedModuleFromMemory(
			&Engine,
			ModuleName,
			TEXT("ASCovFloatFunc_UFunction.as"),
			ScriptSource),
			TEXT("Float UFUNCTION module should compile")));

		UClass* ScriptClass = FindGeneratedClass(&Engine, TEXT("ACoverageFloatFunctionActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Float UFUNCTION actor class should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Float UFUNCTION actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}

		BeginPlayActor(Engine, *Actor);

		{
			FFunctionInvoker Invoker(*TestRunner, Actor, TEXT("AddFloats"));
			ASSERT_THAT(IsTrue(Invoker.IsValid(), TEXT("AddFloats should be invokable")));
			if (!Invoker.IsValid())
			{
				return;
			}
			Invoker.AddParam<double>(10.5).AddParam<double>(1.5);
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(12.0, Invoker.CallAndReturn<double>(0.0), 0.001), TEXT("UFUNCTION AS float value parameters and return should execute through FDoubleProperty storage")));
		}
		{
			FFunctionInvoker Invoker(*TestRunner, Actor, TEXT("MultiplyDoubles"));
			ASSERT_THAT(IsTrue(Invoker.IsValid(), TEXT("MultiplyDoubles should be invokable")));
			if (!Invoker.IsValid())
			{
				return;
			}
			Invoker.AddParam<double>(4.25).AddParam<double>(2.0);
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(8.5, Invoker.CallAndReturn<double>(0.0), 0.001), TEXT("UFUNCTION double value parameters and return should execute")));
		}
		{
			FFunctionInvoker Invoker(*TestRunner, Actor, TEXT("WriteFloatOut"));
			ASSERT_THAT(IsTrue(Invoker.IsValid(), TEXT("WriteFloatOut should be invokable")));
			if (!Invoker.IsValid())
			{
				return;
			}
			Invoker.AddParam<double>(20.0).AddParam<double>(0.0);
			ASSERT_THAT(IsTrue(Invoker.Call(), TEXT("UFUNCTION float &out should execute through FFunctionInvoker")));

			double OutValue = 0.0;
			ASSERT_THAT(IsTrue(Invoker.ReadParamAfterCall(1, OutValue), TEXT("WriteFloatOut should expose float out value after call")));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(20.75, OutValue, 0.001), TEXT("UFUNCTION AS float &out should copy result by parameter index through FDoubleProperty storage")));
		}
		{
			FFunctionInvoker Invoker(*TestRunner, Actor, TEXT("WriteDoubleOut"));
			ASSERT_THAT(IsTrue(Invoker.IsValid(), TEXT("WriteDoubleOut should be invokable")));
			if (!Invoker.IsValid())
			{
				return;
			}
			Invoker.AddParam<double>(30.0).AddParam<double>(0.0);
			ASSERT_THAT(IsTrue(Invoker.Call(), TEXT("UFUNCTION double &out should execute through FFunctionInvoker")));

			double OutValue = 0.0;
			ASSERT_THAT(IsTrue(Invoker.ReadParamAfterCall(1, OutValue), TEXT("WriteDoubleOut should expose double out value after call")));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(31.25, OutValue, 0.001), TEXT("UFUNCTION double &out should copy result by parameter index")));
		}
		{
			FFunctionInvoker Invoker(*TestRunner, Actor, TEXT("MutateFloat"));
			ASSERT_THAT(IsTrue(Invoker.IsValid(), TEXT("MutateFloat should be invokable")));
			if (!Invoker.IsValid())
			{
				return;
			}
			Invoker.AddParam<double>(7.0);
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(15.0, Invoker.CallAndReturn<double>(0.0), 0.001), TEXT("UFUNCTION AS float &inout return should execute through FDoubleProperty storage")));

			double Value = 0.0;
			ASSERT_THAT(IsTrue(Invoker.ReadParamAfterCall(0, Value), TEXT("MutateFloat should expose float inout value after call")));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(14.0, Value, 0.001), TEXT("UFUNCTION AS float &inout should mutate reflected FDoubleProperty parameter buffer")));
		}
		{
			FFunctionInvoker Invoker(*TestRunner, Actor, TEXT("MutateDouble"));
			ASSERT_THAT(IsTrue(Invoker.IsValid(), TEXT("MutateDouble should be invokable")));
			if (!Invoker.IsValid())
			{
				return;
			}
			Invoker.AddParam<double>(8.0);
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(25.0, Invoker.CallAndReturn<double>(0.0), 0.001), TEXT("UFUNCTION double &inout return should execute")));

			double Value = 0.0;
			ASSERT_THAT(IsTrue(Invoker.ReadParamAfterCall(0, Value), TEXT("MutateDouble should expose double inout value after call")));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(24.0, Value, 0.001), TEXT("UFUNCTION double &inout should mutate reflected parameter buffer")));
		}
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
