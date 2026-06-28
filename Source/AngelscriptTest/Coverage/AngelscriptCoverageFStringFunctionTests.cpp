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
		ASSERT_THAT(IsNotNull(Module, TEXT("FString function value-parameter module should compile")));
		if (Module == nullptr)
		{
			return;
		}

		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("FString AcceptString(FString)"));
			ASSERT_THAT(IsTrue(Invoker.IsValid(), TEXT("AcceptString should resolve and prepare")));
			if (!Invoker.IsValid())
			{
				return;
			}
			FString InputString = TEXT("Hello");
			Invoker.AddArgStruct(InputString);
			FString Result;
			ASSERT_THAT(IsTrue(Invoker.ExecuteAndExtractStruct(Result)));
			ASSERT_THAT(AreEqual(FString(TEXT("Hello World")), Result, TEXT("FString value parameter")));
		}
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("FName AcceptName(FName)"));
			ASSERT_THAT(IsTrue(Invoker.IsValid(), TEXT("AcceptName should resolve and prepare")));
			if (!Invoker.IsValid())
			{
				return;
			}
			FName InputName = TEXT("Test");
			Invoker.AddArgStruct(InputName);
			FName Result;
			ASSERT_THAT(IsTrue(Invoker.ExecuteAndExtractStruct(Result)));
			ASSERT_THAT(AreEqual(FName(TEXT("Test")), Result, TEXT("FName value parameter")));
		}
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("FString AcceptText(FText)"));
			ASSERT_THAT(IsTrue(Invoker.IsValid(), TEXT("AcceptText should resolve and prepare")));
			if (!Invoker.IsValid())
			{
				return;
			}
			FText InputText = FText::FromString(TEXT("Text"));
			Invoker.AddArgStruct(InputText);
			FString Result;
			ASSERT_THAT(IsTrue(Invoker.ExecuteAndExtractStruct(Result)));
			ASSERT_THAT(AreEqual(FString(TEXT("Text")), Result, TEXT("FText value parameter")));
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

		FString AcceptTextIn(FText&in x)
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
		ASSERT_THAT(IsNotNull(Module, TEXT("FString function &in-parameter module should compile")));
		if (Module == nullptr)
		{
			return;
		}

		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("FString AcceptStringIn(FString&in)"));
			ASSERT_THAT(IsTrue(Invoker.IsValid(), TEXT("AcceptStringIn should resolve and prepare")));
			if (!Invoker.IsValid())
			{
				return;
			}
			FString InputString = TEXT("Test");
			Invoker.AddArgRef(InputString);
			FString Result;
			ASSERT_THAT(IsTrue(Invoker.ExecuteAndExtractStruct(Result)));
			ASSERT_THAT(AreEqual(FString(TEXT("Received: Test")), Result, TEXT("FString &in parameter")));
		}
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("FName AcceptNameIn(FName&in)"));
			ASSERT_THAT(IsTrue(Invoker.IsValid(), TEXT("AcceptNameIn should resolve and prepare")));
			if (!Invoker.IsValid())
			{
				return;
			}
			FName InputName = TEXT("MyName");
			Invoker.AddArgRef(InputName);
			FName Result;
			ASSERT_THAT(IsTrue(Invoker.ExecuteAndExtractStruct(Result)));
			ASSERT_THAT(AreEqual(FName(TEXT("MyName")), Result, TEXT("FName &in parameter")));
		}
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("FString AcceptTextIn(FText&in)"));
			ASSERT_THAT(IsTrue(Invoker.IsValid(), TEXT("AcceptTextIn should resolve and prepare")));
			if (!Invoker.IsValid())
			{
				return;
			}
			FText InputText = FText::FromString(TEXT("InputText"));
			Invoker.AddArgRef(InputText);
			FString Result;
			ASSERT_THAT(IsTrue(Invoker.ExecuteAndExtractStruct(Result)));
			ASSERT_THAT(AreEqual(FString(TEXT("InputText")), Result, TEXT("FText &in parameter")));
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

		void WriteText(FText&out x)
		{
			x = FText::FromString("OutputText");
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
		ASSERT_THAT(IsNotNull(Module, TEXT("FString function &out-parameter module should compile")));
		if (Module == nullptr)
		{
			return;
		}

		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("void WriteString(FString&out)"));
			ASSERT_THAT(IsTrue(Invoker.IsValid(), TEXT("WriteString should resolve and prepare")));
			if (!Invoker.IsValid())
			{
				return;
			}
			FString OutValue;
			Invoker.AddArgRef(OutValue);
			ASSERT_THAT(IsTrue(Invoker.Execute(), TEXT("WriteString should execute")));
			ASSERT_THAT(AreEqual(FString(TEXT("Output")), OutValue, TEXT("FString &out parameter")));
		}
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("void WriteName(FName&out)"));
			ASSERT_THAT(IsTrue(Invoker.IsValid(), TEXT("WriteName should resolve and prepare")));
			if (!Invoker.IsValid())
			{
				return;
			}
			FName OutValue;
			Invoker.AddArgRef(OutValue);
			ASSERT_THAT(IsTrue(Invoker.Execute(), TEXT("WriteName should execute")));
			ASSERT_THAT(AreEqual(FName(TEXT("OutputName")), OutValue, TEXT("FName &out parameter")));
		}
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("void WriteText(FText&out)"));
			ASSERT_THAT(IsTrue(Invoker.IsValid(), TEXT("WriteText should resolve and prepare")));
			if (!Invoker.IsValid())
			{
				return;
			}
			FText OutValue;
			Invoker.AddArgRef(OutValue);
			ASSERT_THAT(IsTrue(Invoker.Execute(), TEXT("WriteText should execute")));
			ASSERT_THAT(IsTrue(OutValue.EqualTo(FText::FromString(TEXT("OutputText"))), TEXT("FText &out parameter")));
		}
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("void WriteMultiple(FString&out, FString&out)"));
			ASSERT_THAT(IsTrue(Invoker.IsValid(), TEXT("WriteMultiple should resolve and prepare")));
			if (!Invoker.IsValid())
			{
				return;
			}
			FString OutA, OutB;
			Invoker.AddArgRef(OutA).AddArgRef(OutB);
			ASSERT_THAT(IsTrue(Invoker.Execute(), TEXT("WriteMultiple should execute")));
			ASSERT_THAT(AreEqual(FString(TEXT("First")), OutA, TEXT("multiple &out parameter A")));
			ASSERT_THAT(AreEqual(FString(TEXT("Second")), OutB, TEXT("multiple &out parameter B")));
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

		void ReplaceName(FName&inout x)
		{
			if (x == n"OriginalName")
			{
				x = n"UpdatedName";
			}
		}

		void ReplaceText(FText&inout x)
		{
			if (x.ToString() == "OriginalText")
			{
				x = FText::FromString("UpdatedText");
			}
		}
		)AS"));
		ON_SCOPE_EXIT
		{
			if (Module != nullptr)
			{
				Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
			}
		};
		ASSERT_THAT(IsNotNull(Module, TEXT("FString function &inout-parameter module should compile")));
		if (Module == nullptr)
		{
			return;
		}

		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("void AppendToString(FString&inout)"));
			ASSERT_THAT(IsTrue(Invoker.IsValid(), TEXT("AppendToString should resolve and prepare")));
			if (!Invoker.IsValid())
			{
				return;
			}
			FString Value = TEXT("Original");
			Invoker.AddArgRef(Value);
			ASSERT_THAT(IsTrue(Invoker.Execute(), TEXT("AppendToString should execute")));
			ASSERT_THAT(AreEqual(FString(TEXT("Original Appended")), Value, TEXT("FString &inout parameter modifies in place")));
		}
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("void ReplaceName(FName&inout)"));
			ASSERT_THAT(IsTrue(Invoker.IsValid(), TEXT("ReplaceName should resolve and prepare")));
			if (!Invoker.IsValid())
			{
				return;
			}
			FName Value(TEXT("OriginalName"));
			Invoker.AddArgRef(Value);
			ASSERT_THAT(IsTrue(Invoker.Execute(), TEXT("ReplaceName should execute")));
			ASSERT_THAT(AreEqual(FName(TEXT("UpdatedName")), Value, TEXT("FName &inout parameter modifies in place")));
		}
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("void ReplaceText(FText&inout)"));
			ASSERT_THAT(IsTrue(Invoker.IsValid(), TEXT("ReplaceText should resolve and prepare")));
			if (!Invoker.IsValid())
			{
				return;
			}
			FText Value = FText::FromString(TEXT("OriginalText"));
			Invoker.AddArgRef(Value);
			ASSERT_THAT(IsTrue(Invoker.Execute(), TEXT("ReplaceText should execute")));
			ASSERT_THAT(IsTrue(Value.EqualTo(FText::FromString(TEXT("UpdatedText"))), TEXT("FText &inout parameter modifies in place")));
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

		FText ReturnText()
		{
			return FText::FromString("ReturnText");
		}
		)AS"));
		ON_SCOPE_EXIT
		{
			if (Module != nullptr)
			{
				Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
			}
		};
		ASSERT_THAT(IsNotNull(Module, TEXT("FString function return-value module should compile")));
		if (Module == nullptr)
		{
			return;
		}

		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("FString ReturnString()"));
			ASSERT_THAT(IsTrue(Invoker.IsValid(), TEXT("ReturnString should resolve and prepare")));
			if (!Invoker.IsValid())
			{
				return;
			}
			FString Result;
			ASSERT_THAT(IsTrue(Invoker.ExecuteAndExtractStruct(Result)));
			ASSERT_THAT(AreEqual(FString(TEXT("Hello World")), Result, TEXT("FString return value")));
		}
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("FName ReturnName()"));
			ASSERT_THAT(IsTrue(Invoker.IsValid(), TEXT("ReturnName should resolve and prepare")));
			if (!Invoker.IsValid())
			{
				return;
			}
			FName Result;
			ASSERT_THAT(IsTrue(Invoker.ExecuteAndExtractStruct(Result)));
			ASSERT_THAT(AreEqual(FName(TEXT("MyName")), Result, TEXT("FName return value")));
		}
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("FString ReturnEmpty()"));
			ASSERT_THAT(IsTrue(Invoker.IsValid(), TEXT("ReturnEmpty should resolve and prepare")));
			if (!Invoker.IsValid())
			{
				return;
			}
			FString Result;
			ASSERT_THAT(IsTrue(Invoker.ExecuteAndExtractStruct(Result)));
			ASSERT_THAT(AreEqual(FString(TEXT("")), Result, TEXT("FString return empty")));
		}
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("FText ReturnText()"));
			ASSERT_THAT(IsTrue(Invoker.IsValid(), TEXT("ReturnText should resolve and prepare")));
			if (!Invoker.IsValid())
			{
				return;
			}
			FText Result;
			ASSERT_THAT(IsTrue(Invoker.ExecuteAndExtractStruct(Result)));
			ASSERT_THAT(IsTrue(Result.EqualTo(FText::FromString(TEXT("ReturnText"))), TEXT("FText return value")));
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

		FString TextWithDefault(FText text)
		{
			return text.ToString();
		}
		)AS"));
		ON_SCOPE_EXIT
		{
			if (Module != nullptr)
			{
				Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
			}
		};
		ASSERT_THAT(IsNotNull(Module, TEXT("FString function default-parameter module should compile")));
		if (Module == nullptr)
		{
			return;
		}

		// Call with all arguments
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("FString ConcatWithDefault(FString, FString)"));
			ASSERT_THAT(IsTrue(Invoker.IsValid(), TEXT("ConcatWithDefault with explicit args should resolve and prepare")));
			if (!Invoker.IsValid())
			{
				return;
			}
			FString Arg1 = TEXT("Test");
			FString Arg2 = TEXT(" Custom");
			Invoker.AddArgStruct(Arg1).AddArgStruct(Arg2);
			FString Result;
			ASSERT_THAT(IsTrue(Invoker.ExecuteAndExtractStruct(Result)));
			ASSERT_THAT(AreEqual(FString(TEXT("Test Custom")), Result, TEXT("default parameter when explicitly provided")));
		}

		// Call with default (omit second argument)
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("FString ConcatWithDefault(FString)"));
			ASSERT_THAT(IsTrue(Invoker.IsValid(), TEXT("ConcatWithDefault with default arg should resolve and prepare")));
			if (!Invoker.IsValid())
			{
				return;
			}
			FString Arg1 = TEXT("Test");
			Invoker.AddArgStruct(Arg1);
			FString Result;
			ASSERT_THAT(IsTrue(Invoker.ExecuteAndExtractStruct(Result)));
			ASSERT_THAT(AreEqual(FString(TEXT("Test Default")), Result, TEXT("default parameter used")));
		}

		// Call with default name
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("FString GreetWithDefault()"));
			ASSERT_THAT(IsTrue(Invoker.IsValid(), TEXT("GreetWithDefault should resolve and prepare")));
			if (!Invoker.IsValid())
			{
				return;
			}
			FString Result;
			ASSERT_THAT(IsTrue(Invoker.ExecuteAndExtractStruct(Result)));
			ASSERT_THAT(AreEqual(FString(TEXT("Hello World")), Result, TEXT("default parameter (no args)")));
		}

		// FText explicit default-like path
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("FString TextWithDefault(FText)"));
			ASSERT_THAT(IsTrue(Invoker.IsValid(), TEXT("TextWithDefault should resolve and prepare")));
			if (!Invoker.IsValid())
			{
				return;
			}
			FText TextValue = FText::FromString(TEXT("DefaultText"));
			Invoker.AddArgStruct(TextValue);
			FString Result;
			ASSERT_THAT(IsTrue(Invoker.ExecuteAndExtractStruct(Result)));
			ASSERT_THAT(AreEqual(FString(TEXT("DefaultText")), Result, TEXT("FText explicit parameter mirrors default-value coverage path")));
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
		ASSERT_THAT(IsNotNull(Module, TEXT("FString function overload module should compile")));
		if (Module == nullptr)
		{
			return;
		}

		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("FString CallProcessString()"));
			ASSERT_THAT(IsTrue(Invoker.IsValid(), TEXT("CallProcessString should resolve and prepare")));
			if (!Invoker.IsValid())
			{
				return;
			}
			FString Result;
			ASSERT_THAT(IsTrue(Invoker.ExecuteAndExtractStruct(Result)));
			ASSERT_THAT(AreEqual(FString(TEXT("String: Test")), Result, TEXT("overload resolves to FString version")));
		}
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("FString CallProcessName()"));
			ASSERT_THAT(IsTrue(Invoker.IsValid(), TEXT("CallProcessName should resolve and prepare")));
			if (!Invoker.IsValid())
			{
				return;
			}
			FString Result;
			ASSERT_THAT(IsTrue(Invoker.ExecuteAndExtractStruct(Result)));
			ASSERT_THAT(AreEqual(FString(TEXT("Name: Test")), Result, TEXT("overload resolves to FName version")));
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
				FText EchoText(FText value)
				{
					return value;
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
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("FString-function UFUNCTION actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		// UFUNCTION with FString parameters and return
		{
			FFunctionInvoker Invoker(*TestRunner, Actor, TEXT("ConcatStrings"));
			ASSERT_THAT(IsTrue(Invoker.IsValid(), TEXT("ConcatStrings UFUNCTION should resolve")));
			if (!Invoker.IsValid())
			{
				return;
			}
			Invoker.AddParam(FString(TEXT("Hello")));
			Invoker.AddParam(FString(TEXT(" World")));
			const FString Result = Invoker.CallAndReturn<FString>();
			ASSERT_THAT(AreEqual(FString(TEXT("Hello World")), Result, TEXT("UFUNCTION FString parameters and return")));
		}

		// UFUNCTION with FName return
		{
			FFunctionInvoker Invoker(*TestRunner, Actor, TEXT("GetName"));
			ASSERT_THAT(IsTrue(Invoker.IsValid(), TEXT("GetName UFUNCTION should resolve")));
			if (!Invoker.IsValid())
			{
				return;
			}
			const FName Result = Invoker.CallAndReturn<FName>();
			ASSERT_THAT(AreEqual(FName(TEXT("ActorName")), Result, TEXT("UFUNCTION FName return")));
		}

		// UFUNCTION with FText parameter and return
		{
			FFunctionInvoker Invoker(*TestRunner, Actor, TEXT("EchoText"));
			ASSERT_THAT(IsTrue(Invoker.IsValid(), TEXT("EchoText UFUNCTION should resolve")));
			if (!Invoker.IsValid())
			{
				return;
			}
			Invoker.AddParam(FText::FromString(TEXT("EchoedText")));
			const FText Result = Invoker.CallAndReturn<FText>(FText::GetEmpty());
			ASSERT_THAT(IsTrue(Result.EqualTo(FText::FromString(TEXT("EchoedText"))), TEXT("UFUNCTION FText parameter and return")));
		}

		// UFUNCTION with &out parameter
		{
			FFunctionInvoker Invoker(*TestRunner, Actor, TEXT("WriteOut"));
			ASSERT_THAT(IsTrue(Invoker.IsValid(), TEXT("WriteOut UFUNCTION should resolve")));
			if (!Invoker.IsValid())
			{
				return;
			}
			FString OutValue;
			Invoker.AddParam(FString());
			Invoker.Call();
			Invoker.ReadParamAfterCall(0, OutValue);
			ASSERT_THAT(AreEqual(FString(TEXT("UFUNCTION Output")), OutValue, TEXT("UFUNCTION FString &out parameter")));
		}
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
