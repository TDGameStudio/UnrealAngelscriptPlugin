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
// AngelscriptCoverageFLinearColorFunctionTests
// -----------------------------------------------------------------------------
// Coverage for AngelScript FLinearColor *function usage* -- parameters, return values,
// defaults, and UFUNCTION.
//
// Test patterns:
//   - Pattern B: Global functions via FASGlobalFunctionInvoker
//   - Pattern C: UFUNCTION via FFunctionInvoker
// -----------------------------------------------------------------------------

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptFunctionalTestUtils;

TEST_CLASS_WITH_FLAGS(FAngelscriptCoverageFLinearColorFunctionTest,
	"Angelscript.TestModule.Coverage.FLinearColorFunction",
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

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovFLinearColorFunc_ParamValue", ASTEST_AS(R"AS(
		FLinearColor AcceptColor(FLinearColor c)
		{
			return c * 2.0;
		}

		FLinearColor BlendColors(FLinearColor a, FLinearColor b)
		{
			return a * 0.5 + b * 0.5;
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
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("FLinearColor AcceptColor(FLinearColor)"));
			FLinearColor Input = FLinearColor(0.2f, 0.3f, 0.4f, 0.5f);
			Invoker.AddArgRef(Input);
			FLinearColor Result;
			ASSERT_THAT(IsTrue(Invoker.ExecuteAndExtractStruct(Result)));
			TestRunner->TestTrue(TEXT("FLinearColor value parameter"), Result.Equals(FLinearColor(0.4f, 0.6f, 0.8f, 1.0f), 0.001f));
		}
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("FLinearColor BlendColors(FLinearColor, FLinearColor)"));
			FLinearColor A = FLinearColor::Red;
			FLinearColor B = FLinearColor::Blue;
			Invoker.AddArgRef(A).AddArgRef(B);
			FLinearColor Result;
			ASSERT_THAT(IsTrue(Invoker.ExecuteAndExtractStruct(Result)));
			TestRunner->TestTrue(TEXT("two FLinearColor value parameters"), Result.R > 0.4f && Result.B > 0.4f);
		}
	}

	// -------------------------------------------------------------------------
	// Function parameters: &in (const reference).
	// -------------------------------------------------------------------------
	TEST_METHOD(FunctionParametersIn)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovFLinearColorFunc_ParamIn", ASTEST_AS(R"AS(
		float AcceptColorIn(FLinearColor&in c)
		{
			return c.GetLuminance();
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
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("float AcceptColorIn(FLinearColor&in)"));
			FLinearColor Input = FLinearColor::White;
			Invoker.AddArgRef(Input);
			float Result = Invoker.ExecuteAndGet<float>(0.0f);
			TestRunner->TestTrue(TEXT("FLinearColor &in parameter"), Result > 0.9f);
		}
	}

	// -------------------------------------------------------------------------
	// Function parameters: &out (output parameter).
	// -------------------------------------------------------------------------
	TEST_METHOD(FunctionParametersOut)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovFLinearColorFunc_ParamOut", ASTEST_AS(R"AS(
		void WriteColor(FLinearColor&out c)
		{
			c = FLinearColor(0.25, 0.5, 0.75, 1.0);
		}

		void WriteMultipleColors(FLinearColor&out a, FLinearColor&out b)
		{
			a = FLinearColor::Red;
			b = FLinearColor::Green;
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
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("void WriteColor(FLinearColor&out)"));
			FLinearColor OutValue;
			Invoker.AddArgRef(OutValue);
			Invoker.Execute();
			TestRunner->TestTrue(TEXT("FLinearColor &out parameter"), OutValue.Equals(FLinearColor(0.25f, 0.5f, 0.75f, 1.0f), 0.001f));
		}
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("void WriteMultipleColors(FLinearColor&out, FLinearColor&out)"));
			FLinearColor OutA, OutB;
			Invoker.AddArgRef(OutA).AddArgRef(OutB);
			Invoker.Execute();
			TestRunner->TestEqual(TEXT("multiple &out parameter A"), OutA, FLinearColor::Red);
			TestRunner->TestEqual(TEXT("multiple &out parameter B"), OutB, FLinearColor::Green);
		}
	}

	// -------------------------------------------------------------------------
	// Function parameters: &inout (read-write parameter).
	// -------------------------------------------------------------------------
	TEST_METHOD(FunctionParametersInOut)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovFLinearColorFunc_ParamInOut", ASTEST_AS(R"AS(
		void BrightenColor(FLinearColor&inout c, float amount)
		{
			c = c * amount;
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
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("void BrightenColor(FLinearColor&inout, float)"));
			FLinearColor Value = FLinearColor(0.5f, 0.5f, 0.5f, 1.0f);
			Invoker.AddArgRef(Value).AddArg(2.0f);
			Invoker.Execute();
			TestRunner->TestTrue(TEXT("FLinearColor &inout parameter brightens color"), Value.Equals(FLinearColor(1.0f, 1.0f, 1.0f, 2.0f), 0.001f));
		}
	}

	// -------------------------------------------------------------------------
	// Function return values.
	// -------------------------------------------------------------------------
	TEST_METHOD(FunctionReturnValues)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovFLinearColorFunc_Return", ASTEST_AS(R"AS(
		FLinearColor ReturnWhite()
		{
			return FLinearColor::White;
		}

		FLinearColor ReturnCustomColor()
		{
			return FLinearColor(0.3, 0.6, 0.9, 1.0);
		}

		FLinearColor ReturnComputedColor()
		{
			FLinearColor a = FLinearColor::Red;
			FLinearColor b = FLinearColor::Blue;
			return a * 0.5 + b * 0.5;
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
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("FLinearColor ReturnWhite()"));
			FLinearColor Result;
			ASSERT_THAT(IsTrue(Invoker.ExecuteAndExtractStruct(Result)));
			TestRunner->TestEqual(TEXT("FLinearColor return White"), Result, FLinearColor::White);
		}
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("FLinearColor ReturnCustomColor()"));
			FLinearColor Result;
			ASSERT_THAT(IsTrue(Invoker.ExecuteAndExtractStruct(Result)));
			TestRunner->TestTrue(TEXT("FLinearColor return custom"), Result.Equals(FLinearColor(0.3f, 0.6f, 0.9f, 1.0f), 0.001f));
		}
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("FLinearColor ReturnComputedColor()"));
			FLinearColor Result;
			ASSERT_THAT(IsTrue(Invoker.ExecuteAndExtractStruct(Result)));
			TestRunner->TestTrue(TEXT("FLinearColor return computed"), Result.R > 0.4f && Result.B > 0.4f);
		}
	}

	// -------------------------------------------------------------------------
	// Function default parameters.
	// -------------------------------------------------------------------------
	TEST_METHOD(FunctionDefaultParameters)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovFLinearColorFunc_Default", ASTEST_AS(R"AS(
		FLinearColor BlendWithDefault(FLinearColor a, FLinearColor b = FLinearColor::Black)
		{
			return a * 0.5 + b * 0.5;
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
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("FLinearColor BlendWithDefault(FLinearColor, FLinearColor)"));
			FLinearColor Arg1 = FLinearColor::Red;
			FLinearColor Arg2 = FLinearColor::Blue;
			Invoker.AddArgRef(Arg1).AddArgRef(Arg2);
			FLinearColor Result;
			ASSERT_THAT(IsTrue(Invoker.ExecuteAndExtractStruct(Result)));
			TestRunner->TestTrue(TEXT("default parameter when explicitly provided"), Result.R > 0.4f && Result.B > 0.4f);
		}

		// Call with default (omit second argument)
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("FLinearColor BlendWithDefault(FLinearColor)"));
			FLinearColor Arg1 = FLinearColor::White;
			Invoker.AddArgRef(Arg1);
			FLinearColor Result;
			ASSERT_THAT(IsTrue(Invoker.ExecuteAndExtractStruct(Result)));
			TestRunner->TestTrue(TEXT("default parameter used (blends with black)"), Result.R > 0.4f && Result.R < 0.6f);
		}
	}

	// -------------------------------------------------------------------------
	// UFUNCTION parameter and return values.
	// -------------------------------------------------------------------------
	TEST_METHOD(UFunctionParametersAndReturn)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageFLinearColorFunction_UFUNCTION"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageFLinearColorFunctionUFUNCTION.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageFLinearColorFunctionActor : AActor
			{
				UFUNCTION()
				FLinearColor MixColors(FLinearColor a, FLinearColor b)
				{
					return a * 0.5 + b * 0.5;
				}

				UFUNCTION()
				float GetColorLuminance(FLinearColor c)
				{
					return c.GetLuminance();
				}

				UFUNCTION()
				void WriteOut(FLinearColor&out result)
				{
					result = FLinearColor::Yellow;
				}
			}
			)AS"),
			TEXT("ACoverageFLinearColorFunctionActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("FLinearColor-function UFUNCTION actor class should compile")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("FLinearColor-function UFUNCTION actor should spawn")));
		BeginPlayActor(Engine, *Actor);

		// UFUNCTION with FLinearColor parameters and return
		{
			FFunctionInvoker Invoker(*TestRunner, Actor, TEXT("MixColors"));
			Invoker.AddParam(FLinearColor::Red);
			Invoker.AddParam(FLinearColor::Blue);
			const FLinearColor Result = Invoker.CallAndReturn<FLinearColor>();
			TestRunner->TestTrue(TEXT("UFUNCTION FLinearColor parameters and return"), Result.R > 0.4f && Result.B > 0.4f);
		}

		// UFUNCTION with FLinearColor parameter and float return
		{
			FFunctionInvoker Invoker(*TestRunner, Actor, TEXT("GetColorLuminance"));
			Invoker.AddParam(FLinearColor::White);
			const float Result = Invoker.CallAndReturn<float>();
			TestRunner->TestTrue(TEXT("UFUNCTION FLinearColor to float"), Result > 0.9f);
		}

		// UFUNCTION with &out parameter
		{
			FFunctionInvoker Invoker(*TestRunner, Actor, TEXT("WriteOut"));
			FLinearColor OutValue;
			Invoker.AddParam(FLinearColor::Black);
			Invoker.Call();
			Invoker.ReadParamAfterCall(0, OutValue);
			TestRunner->TestEqual(TEXT("UFUNCTION FLinearColor &out parameter"), OutValue, FLinearColor::Yellow);
		}
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
