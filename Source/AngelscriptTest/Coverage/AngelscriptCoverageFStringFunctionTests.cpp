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
// AngelscriptCoverageFStringFunctionTests
// -----------------------------------------------------------------------------
// Coverage for AngelScript string-family *function usage*.
//
// Test patterns:
//   - Pattern B: Global functions via FASGlobalFunctionInvoker
//   - Pattern C: UFUNCTION via FFunctionInvoker
//
// String family under test:
//   FString / FName / FText
// -----------------------------------------------------------------------------

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptFunctionalTestUtils;

TEST_CLASS_WITH_FLAGS(FAngelscriptCoverageFStringFunctionTest,
	"Angelscript.TestModule.Coverage.FStringFunction",
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
	// Function parameters: value passing (FString / FName / FText).
	// -------------------------------------------------------------------------
	TEST_METHOD(FunctionParametersValue)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovFStringFunc_ParamValue", ASTEST_AS(R"AS(
		FString AcceptString(FString x)
		{
			return x + " World";
		}

		FName AcceptName(FName x)
		{
			return x;
		}

		FString AcceptText(FText x)
		{
			return x.ToString();
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
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("FString AcceptString(FString)"));
			FString InputString = TEXT("Hello");
			Invoker.AddArgRef(InputString);
			FString Result;
			ASSERT_THAT(IsTrue(Invoker.ExecuteAndExtractStruct(Result)));
			TestRunner->TestEqual(TEXT("FString value parameter"), Result, FString(TEXT("Hello World")));
		}
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("FName AcceptName(FName)"));
			FName InputName = TEXT("Test");
			Invoker.AddArgRef(InputName);
			FName Result;
			ASSERT_THAT(IsTrue(Invoker.ExecuteAndExtractStruct(Result)));
			TestRunner->TestEqual(TEXT("FName value parameter"), Result, FName(TEXT("Test")));
		}
	}

	// -------------------------------------------------------------------------
	// Function parameters: &in (const reference).
	// -------------------------------------------------------------------------
	TEST_METHOD(FunctionParametersIn)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovFStringFunc_ParamIn", ASTEST_AS(R"AS(
		FString AcceptStringIn(FString&in x)
		{
			return "Received: " + x;
		}

		FName AcceptNameIn(FName&in x)
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
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("FString AcceptStringIn(FString&in)"));
			FString InputString = TEXT("Test");
			Invoker.AddArgRef(InputString);
			FString Result;
			ASSERT_THAT(IsTrue(Invoker.ExecuteAndExtractStruct(Result)));
			TestRunner->TestEqual(TEXT("FString &in parameter"), Result, FString(TEXT("Received: Test")));
		}
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("FName AcceptNameIn(FName&in)"));
			FName InputName = TEXT("MyName");
			Invoker.AddArgRef(InputName);
			FName Result;
			ASSERT_THAT(IsTrue(Invoker.ExecuteAndExtractStruct(Result)));
			TestRunner->TestEqual(TEXT("FName &in parameter"), Result, FName(TEXT("MyName")));
		}
	}

	// -------------------------------------------------------------------------
	// Function parameters: &out (output parameter).
	// -------------------------------------------------------------------------
	TEST_METHOD(FunctionParametersOut)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovFStringFunc_ParamOut", ASTEST_AS(R"AS(
		void WriteString(FString&out x)
		{
			x = "Output";
		}

		void WriteName(FName&out x)
		{
			x = n"OutputName";
		}

		void WriteMultiple(FString&out a, FString&out b)
		{
			a = "First";
			b = "Second";
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
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("void WriteString(FString&out)"));
			FString OutValue;
			Invoker.AddArgRef(OutValue);
			Invoker.Execute();
			TestRunner->TestEqual(TEXT("FString &out parameter"), OutValue, FString(TEXT("Output")));
		}
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("void WriteName(FName&out)"));
			FName OutValue;
			Invoker.AddArgRef(OutValue);
			Invoker.Execute();
			TestRunner->TestEqual(TEXT("FName &out parameter"), OutValue, FName(TEXT("OutputName")));
		}
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("void WriteMultiple(FString&out, FString&out)"));
			FString OutA, OutB;
			Invoker.AddArgRef(OutA).AddArgRef(OutB);
			Invoker.Execute();
			TestRunner->TestEqual(TEXT("multiple &out parameter A"), OutA, FString(TEXT("First")));
			TestRunner->TestEqual(TEXT("multiple &out parameter B"), OutB, FString(TEXT("Second")));
		}
	}

	// -------------------------------------------------------------------------
	// Function parameters: &inout (read-write parameter).
	// -------------------------------------------------------------------------
	TEST_METHOD(FunctionParametersInOut)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovFStringFunc_ParamInOut", ASTEST_AS(R"AS(
		void AppendToString(FString&inout x)
		{
			x += " Appended";
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
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("void AppendToString(FString&inout)"));
			FString Value = TEXT("Original");
			Invoker.AddArgRef(Value);
			Invoker.Execute();
			TestRunner->TestEqual(TEXT("FString &inout parameter modifies in place"), Value, FString(TEXT("Original Appended")));
		}
	}

	// -------------------------------------------------------------------------
	// Function return values.
	// -------------------------------------------------------------------------
	TEST_METHOD(FunctionReturnValues)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovFStringFunc_Return", ASTEST_AS(R"AS(
		FString ReturnString()
		{
			return "Hello World";
		}

		FName ReturnName()
		{
			return n"MyName";
		}

		FString ReturnEmpty()
		{
			return "";
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
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("FString ReturnString()"));
			FString Result;
			ASSERT_THAT(IsTrue(Invoker.ExecuteAndExtractStruct(Result)));
			TestRunner->TestEqual(TEXT("FString return value"), Result, FString(TEXT("Hello World")));
		}
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("FName ReturnName()"));
			FName Result;
			ASSERT_THAT(IsTrue(Invoker.ExecuteAndExtractStruct(Result)));
			TestRunner->TestEqual(TEXT("FName return value"), Result, FName(TEXT("MyName")));
		}
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("FString ReturnEmpty()"));
			FString Result;
			ASSERT_THAT(IsTrue(Invoker.ExecuteAndExtractStruct(Result)));
			TestRunner->TestEqual(TEXT("FString return empty"), Result, FString(TEXT("")));
		}
	}

	// -------------------------------------------------------------------------
	// Function default parameters.
	// -------------------------------------------------------------------------
	TEST_METHOD(FunctionDefaultParameters)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovFStringFunc_Default", ASTEST_AS(R"AS(
		FString ConcatWithDefault(FString a, FString b = " Default")
		{
			return a + b;
		}

		FString GreetWithDefault(FString name = "World")
		{
			return "Hello " + name;
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
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("FString ConcatWithDefault(FString, FString)"));
			FString Arg1 = TEXT("Test");
			FString Arg2 = TEXT(" Custom");
			Invoker.AddArgRef(Arg1).AddArgRef(Arg2);
			FString Result;
			ASSERT_THAT(IsTrue(Invoker.ExecuteAndExtractStruct(Result)));
			TestRunner->TestEqual(TEXT("default parameter when explicitly provided"), Result, FString(TEXT("Test Custom")));
		}

		// Call with default (omit second argument)
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("FString ConcatWithDefault(FString)"));
			FString Arg1 = TEXT("Test");
			Invoker.AddArgRef(Arg1);
			FString Result;
			ASSERT_THAT(IsTrue(Invoker.ExecuteAndExtractStruct(Result)));
			TestRunner->TestEqual(TEXT("default parameter used"), Result, FString(TEXT("Test Default")));
		}

		// Call with default name
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("FString GreetWithDefault()"));
			FString Result;
			ASSERT_THAT(IsTrue(Invoker.ExecuteAndExtractStruct(Result)));
			TestRunner->TestEqual(TEXT("default parameter (no args)"), Result, FString(TEXT("Hello World")));
		}
	}

	// -------------------------------------------------------------------------
	// Function overloading (FString vs FName).
	// -------------------------------------------------------------------------
	TEST_METHOD(FunctionOverloading)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovFStringFunc_Overload", ASTEST_AS(R"AS(
		FString Process(FString x)
		{
			return "String: " + x;
		}

		FString Process(FName x)
		{
			return "Name: " + x.ToString();
		}

		FString CallProcessString()
		{
			return Process("Test");
		}

		FString CallProcessName()
		{
			return Process(n"Test");
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
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("FString CallProcessString()"));
			FString Result;
			ASSERT_THAT(IsTrue(Invoker.ExecuteAndExtractStruct(Result)));
			TestRunner->TestEqual(TEXT("overload resolves to FString version"), Result, FString(TEXT("String: Test")));
		}
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("FString CallProcessName()"));
			FString Result;
			ASSERT_THAT(IsTrue(Invoker.ExecuteAndExtractStruct(Result)));
			TestRunner->TestEqual(TEXT("overload resolves to FName version"), Result, FString(TEXT("Name: Test")));
		}
	}

	// -------------------------------------------------------------------------
	// UFUNCTION parameter and return values.
	// -------------------------------------------------------------------------
	TEST_METHOD(UFunctionParametersAndReturn)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageFStringFunction_UFUNCTION"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageFStringFunctionUFUNCTION.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageFStringFunctionActor : AActor
			{
				UFUNCTION()
				FString ConcatStrings(FString a, FString b)
				{
					return a + b;
				}

				UFUNCTION()
				FName GetName()
				{
					return n"ActorName";
				}

				UFUNCTION()
				void WriteOut(FString&out result)
				{
					result = "UFUNCTION Output";
				}
			}
			)AS"),
			TEXT("ACoverageFStringFunctionActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("FString-function UFUNCTION actor class should compile")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("FString-function UFUNCTION actor should spawn")));
		BeginPlayActor(Engine, *Actor);

		// UFUNCTION with FString parameters and return
		{
			FFunctionInvoker Invoker(*TestRunner, Actor, TEXT("ConcatStrings"));
			Invoker.AddParam(FString(TEXT("Hello")));
			Invoker.AddParam(FString(TEXT(" World")));
			const FString Result = Invoker.CallAndReturn<FString>();
			TestRunner->TestEqual(TEXT("UFUNCTION FString parameters and return"), Result, FString(TEXT("Hello World")));
		}

		// UFUNCTION with FName return
		{
			FFunctionInvoker Invoker(*TestRunner, Actor, TEXT("GetName"));
			const FName Result = Invoker.CallAndReturn<FName>();
			TestRunner->TestEqual(TEXT("UFUNCTION FName return"), Result, FName(TEXT("ActorName")));
		}

		// UFUNCTION with &out parameter
		{
			FFunctionInvoker Invoker(*TestRunner, Actor, TEXT("WriteOut"));
			FString OutValue;
			Invoker.AddParam(FString());
			Invoker.Call();
			Invoker.ReadParamAfterCall(0, OutValue);
			TestRunner->TestEqual(TEXT("UFUNCTION FString &out parameter"), OutValue, FString(TEXT("UFUNCTION Output")));
		}
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
