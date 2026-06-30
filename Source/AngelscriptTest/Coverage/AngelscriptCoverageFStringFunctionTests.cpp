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

		FString ConcatWithImplicitDefault(FString a)
		{
			return ConcatWithDefault(a);
		}

		FString GreetWithDefault(FString name = "World")
		{
			return "Hello " + name;
		}

		FString GreetWithImplicitDefault()
		{
			return GreetWithDefault();
		}

		FString TextWithDefault(FText text)
		{
			return text.ToString();
		}

		FString NameWithDefault(FName name = n"DefaultName")
		{
			return name.ToString();
		}

		FString NameWithImplicitDefault()
		{
			return NameWithDefault();
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
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("FString ConcatWithImplicitDefault(FString)"));
			ASSERT_THAT(IsTrue(Invoker.IsValid(), TEXT("ConcatWithImplicitDefault should resolve and prepare")));
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
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("FString GreetWithImplicitDefault()"));
			ASSERT_THAT(IsTrue(Invoker.IsValid(), TEXT("GreetWithImplicitDefault should resolve and prepare")));
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

		// FName default
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("FString NameWithImplicitDefault()"));
			ASSERT_THAT(IsTrue(Invoker.IsValid(), TEXT("NameWithImplicitDefault should resolve and prepare")));
			if (!Invoker.IsValid())
			{
				return;
			}
			FString Result;
			ASSERT_THAT(IsTrue(Invoker.ExecuteAndExtractStruct(Result)));
			ASSERT_THAT(AreEqual(FString(TEXT("DefaultName")), Result, TEXT("FName default parameter used")));
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

		FString Process(FText x)
		{
			return "Text: " + x.ToString();
		}

		FString CallProcessString()
		{
			return Process("Test");
		}

		FString CallProcessName()
		{
			return Process(n"Test");
		}

		FString CallProcessText()
		{
			return Process(FText::FromString("Test"));
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
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("FString CallProcessText()"));
			ASSERT_THAT(IsTrue(Invoker.IsValid(), TEXT("CallProcessText should resolve and prepare")));
			if (!Invoker.IsValid())
			{
				return;
			}
			FString Result;
			ASSERT_THAT(IsTrue(Invoker.ExecuteAndExtractStruct(Result)));
			ASSERT_THAT(AreEqual(FString(TEXT("Text: Test")), Result, TEXT("overload resolves to FText version")));
		}
	}

	TEST_METHOD(UnsupportedFunctionSignatureBoundaries)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		TestRunner->AddExpectedError(
			TEXT("Default argument value \"DefaultText\" has type const FString"),
			EAutomationExpectedErrorFlags::Contains,
			2);
		TestRunner->AddExpectedError(
			TEXT("Failed while compiling default arg for parameter 0"),
			EAutomationExpectedErrorFlags::Contains,
			2);
		TestRunner->AddExpectedError(
			TEXT("Failed to compile script module 'ASCovFStringFunc_FTextLiteralDefault'"),
			EAutomationExpectedErrorFlags::Contains,
			1);
		TestRunner->AddExpectedError(
			TEXT("ASCovFStringFunc_FTextLiteralDefault"),
			EAutomationExpectedErrorFlags::Contains,
			1);
		TestRunner->AddExpectedError(
			TEXT("Hot reload failed due to script compile errors"),
			EAutomationExpectedErrorFlags::Contains,
			1);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovFStringFunc_FTextLiteralDefault", ASTEST_AS(R"AS(
			FString TextDefaultLiteral(FText text = "DefaultText")
			{
				return text.ToString();
			}

			FString TextDefaultLiteralImplicit()
			{
				return TextDefaultLiteral();
			}
			)AS"));
		ON_SCOPE_EXIT
		{
			if (Module != nullptr)
			{
				Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
			}
		};
		ASSERT_THAT(IsNull(Module, TEXT("FText string-literal defaults should remain an unsupported boundary")));
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
				FString DescribeReferences(const FString&in label, FName&in name, FText&in text)
				{
					return label + "|" + name.ToString() + "|" + text.ToString();
				}

				UFUNCTION()
				FString FormatTextResult()
				{
					return FText::Format(FText::FromString("{0}:{1}"), FText::FromString("Count"), 7).ToString();
				}

				UFUNCTION()
				FString DefaultString(FString value = "UFunctionDefaultString")
				{
					return value + "|Seen";
				}

				UFUNCTION()
				FString DefaultName(FName value = n"UFunctionDefaultName")
				{
					return value.ToString() + "|Seen";
				}

				UFUNCTION()
				void WriteOut(FString&out result)
				{
					result = "UFUNCTION Output";
				}

				UFUNCTION()
				void WriteNameAndText(FName&out outName, FText&out outText)
				{
					outName = n"UFunctionNameOut";
					outText = FText::FromString("UFunctionTextOut");
				}

				UFUNCTION()
				void MutateAll(FString&inout label, FName&inout name, FText&inout text)
				{
					label += "|Mutated";
					name = FName(name.ToString() + "Mutated");
					text = FText::FromString(text.ToString() + "Mutated");
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

		UFunction* DescribeFunction = FindGeneratedFunction(ScriptClass, TEXT("DescribeReferences"));
		ASSERT_THAT(IsNotNull(DescribeFunction, TEXT("DescribeReferences UFUNCTION should be generated")));
		if (DescribeFunction == nullptr)
		{
			return;
		}

		UFunction* ConcatFunction = FindGeneratedFunction(ScriptClass, TEXT("ConcatStrings"));
		UFunction* GetNameFunction = FindGeneratedFunction(ScriptClass, TEXT("GetName"));
		UFunction* EchoTextFunction = FindGeneratedFunction(ScriptClass, TEXT("EchoText"));
		ASSERT_THAT(IsNotNull(ConcatFunction, TEXT("ConcatStrings UFUNCTION should be generated")));
		ASSERT_THAT(IsNotNull(GetNameFunction, TEXT("GetName UFUNCTION should be generated")));
		ASSERT_THAT(IsNotNull(EchoTextFunction, TEXT("EchoText UFUNCTION should be generated")));
		if (ConcatFunction == nullptr || GetNameFunction == nullptr || EchoTextFunction == nullptr)
		{
			return;
		}

		const FProperty* ConcatAParam = FindFProperty<FProperty>(ConcatFunction, TEXT("a"));
		const FProperty* ConcatBParam = FindFProperty<FProperty>(ConcatFunction, TEXT("b"));
		const FProperty* ConcatReturnParam = ConcatFunction->GetReturnProperty();
		const FProperty* GetNameReturnParam = GetNameFunction->GetReturnProperty();
		const FProperty* EchoTextValueParam = FindFProperty<FProperty>(EchoTextFunction, TEXT("value"));
		const FProperty* EchoTextReturnParam = EchoTextFunction->GetReturnProperty();
		ASSERT_THAT(IsNotNull(CastField<FStrProperty>(ConcatAParam), TEXT("ConcatStrings FString parameter a should reflect as FStrProperty")));
		ASSERT_THAT(IsNotNull(CastField<FStrProperty>(ConcatBParam), TEXT("ConcatStrings FString parameter b should reflect as FStrProperty")));
		ASSERT_THAT(IsNotNull(CastField<FStrProperty>(ConcatReturnParam), TEXT("ConcatStrings FString return should reflect as FStrProperty")));
		ASSERT_THAT(IsNotNull(CastField<FNameProperty>(GetNameReturnParam), TEXT("GetName FName return should reflect as FNameProperty")));
		ASSERT_THAT(IsNotNull(CastField<FTextProperty>(EchoTextValueParam), TEXT("EchoText FText parameter should reflect as FTextProperty")));
		ASSERT_THAT(IsNotNull(CastField<FTextProperty>(EchoTextReturnParam), TEXT("EchoText FText return should reflect as FTextProperty")));
		if (ConcatAParam == nullptr || ConcatBParam == nullptr || ConcatReturnParam == nullptr || GetNameReturnParam == nullptr
			|| EchoTextValueParam == nullptr || EchoTextReturnParam == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(ConcatReturnParam->HasAnyPropertyFlags(CPF_ReturnParm), TEXT("ConcatStrings FString return should carry CPF_ReturnParm")));
		ASSERT_THAT(IsTrue(GetNameReturnParam->HasAnyPropertyFlags(CPF_ReturnParm), TEXT("GetName FName return should carry CPF_ReturnParm")));
		ASSERT_THAT(IsTrue(EchoTextReturnParam->HasAnyPropertyFlags(CPF_ReturnParm), TEXT("EchoText FText return should carry CPF_ReturnParm")));

		const FProperty* LabelParam = FindFProperty<FProperty>(DescribeFunction, TEXT("label"));
		const FProperty* NameParam = FindFProperty<FProperty>(DescribeFunction, TEXT("name"));
		const FProperty* TextParam = FindFProperty<FProperty>(DescribeFunction, TEXT("text"));
		ASSERT_THAT(IsNotNull(LabelParam, TEXT("DescribeReferences label parameter should exist")));
		ASSERT_THAT(IsNotNull(NameParam, TEXT("DescribeReferences name parameter should exist")));
		ASSERT_THAT(IsNotNull(TextParam, TEXT("DescribeReferences text parameter should exist")));
		if (LabelParam == nullptr || NameParam == nullptr || TextParam == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(LabelParam->HasAnyPropertyFlags(CPF_ConstParm), TEXT("const FString&in UFUNCTION parameter should carry CPF_ConstParm")));
		ASSERT_THAT(IsTrue(LabelParam->HasAnyPropertyFlags(CPF_OutParm), TEXT("const FString&in UFUNCTION parameter should carry CPF_OutParm reference metadata")));
		ASSERT_THAT(IsTrue(NameParam->HasAnyPropertyFlags(CPF_OutParm), TEXT("FName&in UFUNCTION parameter should carry CPF_OutParm reference metadata")));
		ASSERT_THAT(IsTrue(TextParam->HasAnyPropertyFlags(CPF_OutParm), TEXT("FText&in UFUNCTION parameter should carry CPF_OutParm reference metadata")));

		UFunction* WriteOutFunction = FindGeneratedFunction(ScriptClass, TEXT("WriteOut"));
		UFunction* WriteNameAndTextFunction = FindGeneratedFunction(ScriptClass, TEXT("WriteNameAndText"));
		UFunction* MutateAllFunction = FindGeneratedFunction(ScriptClass, TEXT("MutateAll"));
		ASSERT_THAT(IsNotNull(WriteOutFunction, TEXT("WriteOut UFUNCTION should be generated")));
		ASSERT_THAT(IsNotNull(WriteNameAndTextFunction, TEXT("WriteNameAndText UFUNCTION should be generated")));
		ASSERT_THAT(IsNotNull(MutateAllFunction, TEXT("MutateAll UFUNCTION should be generated")));
		if (WriteOutFunction == nullptr || WriteNameAndTextFunction == nullptr || MutateAllFunction == nullptr)
		{
			return;
		}

		const FProperty* WriteOutResultParam = FindFProperty<FProperty>(WriteOutFunction, TEXT("result"));
		const FProperty* WriteNameOutParam = FindFProperty<FProperty>(WriteNameAndTextFunction, TEXT("outName"));
		const FProperty* WriteTextOutParam = FindFProperty<FProperty>(WriteNameAndTextFunction, TEXT("outText"));
		const FProperty* MutateLabelParam = FindFProperty<FProperty>(MutateAllFunction, TEXT("label"));
		const FProperty* MutateNameParam = FindFProperty<FProperty>(MutateAllFunction, TEXT("name"));
		const FProperty* MutateTextParam = FindFProperty<FProperty>(MutateAllFunction, TEXT("text"));
		ASSERT_THAT(IsNotNull(CastField<FStrProperty>(WriteOutResultParam), TEXT("FString &out UFUNCTION parameter should reflect as FStrProperty")));
		ASSERT_THAT(IsNotNull(CastField<FNameProperty>(WriteNameOutParam), TEXT("FName &out UFUNCTION parameter should reflect as FNameProperty")));
		ASSERT_THAT(IsNotNull(CastField<FTextProperty>(WriteTextOutParam), TEXT("FText &out UFUNCTION parameter should reflect as FTextProperty")));
		ASSERT_THAT(IsNotNull(CastField<FStrProperty>(MutateLabelParam), TEXT("FString &inout UFUNCTION parameter should reflect as FStrProperty")));
		ASSERT_THAT(IsNotNull(CastField<FNameProperty>(MutateNameParam), TEXT("FName &inout UFUNCTION parameter should reflect as FNameProperty")));
		ASSERT_THAT(IsNotNull(CastField<FTextProperty>(MutateTextParam), TEXT("FText &inout UFUNCTION parameter should reflect as FTextProperty")));
		if (WriteOutResultParam == nullptr || WriteNameOutParam == nullptr || WriteTextOutParam == nullptr
			|| MutateLabelParam == nullptr || MutateNameParam == nullptr || MutateTextParam == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(WriteOutResultParam->HasAnyPropertyFlags(CPF_OutParm), TEXT("FString &out UFUNCTION parameter should carry CPF_OutParm")));
		ASSERT_THAT(IsTrue(WriteNameOutParam->HasAnyPropertyFlags(CPF_OutParm), TEXT("FName &out UFUNCTION parameter should carry CPF_OutParm")));
		ASSERT_THAT(IsTrue(WriteTextOutParam->HasAnyPropertyFlags(CPF_OutParm), TEXT("FText &out UFUNCTION parameter should carry CPF_OutParm")));
		ASSERT_THAT(IsTrue(MutateLabelParam->HasAnyPropertyFlags(CPF_OutParm), TEXT("FString &inout UFUNCTION parameter should carry CPF_OutParm")));
		ASSERT_THAT(IsTrue(MutateNameParam->HasAnyPropertyFlags(CPF_OutParm), TEXT("FName &inout UFUNCTION parameter should carry CPF_OutParm")));
		ASSERT_THAT(IsTrue(MutateTextParam->HasAnyPropertyFlags(CPF_OutParm), TEXT("FText &inout UFUNCTION parameter should carry CPF_OutParm")));

		UFunction* DefaultStringFunction = FindGeneratedFunction(ScriptClass, TEXT("DefaultString"));
		UFunction* DefaultNameFunction = FindGeneratedFunction(ScriptClass, TEXT("DefaultName"));
		ASSERT_THAT(IsNotNull(DefaultStringFunction, TEXT("DefaultString UFUNCTION should be generated")));
		ASSERT_THAT(IsNotNull(DefaultNameFunction, TEXT("DefaultName UFUNCTION should be generated")));
		if (DefaultStringFunction == nullptr || DefaultNameFunction == nullptr)
		{
			return;
		}

		const FProperty* DefaultStringValueParam = FindFProperty<FProperty>(DefaultStringFunction, TEXT("value"));
		const FProperty* DefaultNameValueParam = FindFProperty<FProperty>(DefaultNameFunction, TEXT("value"));
		ASSERT_THAT(IsNotNull(CastField<FStrProperty>(DefaultStringValueParam), TEXT("DefaultString FString default parameter should reflect as FStrProperty")));
		ASSERT_THAT(IsNotNull(CastField<FNameProperty>(DefaultNameValueParam), TEXT("DefaultName FName default parameter should reflect as FNameProperty")));
		if (DefaultStringValueParam == nullptr || DefaultNameValueParam == nullptr)
		{
			return;
		}

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

		// UFUNCTION with string-family const-reference parameters
		{
			FFunctionInvoker Invoker(*TestRunner, Actor, TEXT("DescribeReferences"));
			ASSERT_THAT(IsTrue(Invoker.IsValid(), TEXT("DescribeReferences UFUNCTION should resolve")));
			if (!Invoker.IsValid())
			{
				return;
			}
			Invoker.AddParam(FString(TEXT("Label")));
			Invoker.AddParam(FName(TEXT("NameValue")));
			Invoker.AddParam(FText::FromString(TEXT("TextValue")));
			const FString Result = Invoker.CallAndReturn<FString>();
			ASSERT_THAT(AreEqual(FString(TEXT("Label|NameValue|TextValue")), Result, TEXT("UFUNCTION string-family const-reference parameters")));
		}

		// UFUNCTION with FText formatting return
		{
			FFunctionInvoker Invoker(*TestRunner, Actor, TEXT("FormatTextResult"));
			ASSERT_THAT(IsTrue(Invoker.IsValid(), TEXT("FormatTextResult UFUNCTION should resolve")));
			if (!Invoker.IsValid())
			{
				return;
			}
			const FString Result = Invoker.CallAndReturn<FString>();
			ASSERT_THAT(AreEqual(FString(TEXT("Count:7")), Result, TEXT("UFUNCTION FText format result should round-trip")));
		}

		// UFUNCTION with string-family default parameters populated through reflection
		{
			FFunctionInvoker Invoker(*TestRunner, Actor, TEXT("DefaultString"));
			ASSERT_THAT(IsTrue(Invoker.IsValid(), TEXT("DefaultString UFUNCTION should resolve")));
			if (!Invoker.IsValid())
			{
				return;
			}
			Invoker.AddParam(FString(TEXT("ExplicitString")));
			const FString Result = Invoker.CallAndReturn<FString>();
			ASSERT_THAT(AreEqual(FString(TEXT("ExplicitString|Seen")), Result, TEXT("UFUNCTION FString default-parameter signature should execute when populated explicitly")));
		}
		{
			FFunctionInvoker Invoker(*TestRunner, Actor, TEXT("DefaultName"));
			ASSERT_THAT(IsTrue(Invoker.IsValid(), TEXT("DefaultName UFUNCTION should resolve")));
			if (!Invoker.IsValid())
			{
				return;
			}
			Invoker.AddParam(FName(TEXT("ExplicitName")));
			const FString Result = Invoker.CallAndReturn<FString>();
			ASSERT_THAT(AreEqual(FString(TEXT("ExplicitName|Seen")), Result, TEXT("UFUNCTION FName default-parameter signature should execute when populated explicitly")));
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
			ASSERT_THAT(IsTrue(Invoker.Call(), TEXT("WriteOut UFUNCTION should execute")));
			ASSERT_THAT(IsTrue(Invoker.ReadParamAfterCall(0, OutValue), TEXT("WriteOut should expose FString out value after call")));
			ASSERT_THAT(AreEqual(FString(TEXT("UFUNCTION Output")), OutValue, TEXT("UFUNCTION FString &out parameter")));
		}

		// UFUNCTION with FName/FText &out parameters
		{
			FFunctionInvoker Invoker(*TestRunner, Actor, TEXT("WriteNameAndText"));
			ASSERT_THAT(IsTrue(Invoker.IsValid(), TEXT("WriteNameAndText UFUNCTION should resolve")));
			if (!Invoker.IsValid())
			{
				return;
			}
			FName OutName;
			FText OutText;
			Invoker.AddParam(FName());
			Invoker.AddParam(FText::GetEmpty());
			ASSERT_THAT(IsTrue(Invoker.Call(), TEXT("WriteNameAndText UFUNCTION should execute")));
			ASSERT_THAT(IsTrue(Invoker.ReadParamAfterCall(0, OutName), TEXT("WriteNameAndText should expose FName out value after call")));
			ASSERT_THAT(IsTrue(Invoker.ReadParamAfterCall(1, OutText), TEXT("WriteNameAndText should expose FText out value after call")));
			ASSERT_THAT(AreEqual(FName(TEXT("UFunctionNameOut")), OutName, TEXT("UFUNCTION FName &out parameter")));
			ASSERT_THAT(AreEqual(FString(TEXT("UFunctionTextOut")), OutText.ToString(), TEXT("UFUNCTION FText &out parameter")));
		}

		// UFUNCTION with string-family &inout parameters
		{
			FFunctionInvoker Invoker(*TestRunner, Actor, TEXT("MutateAll"));
			ASSERT_THAT(IsTrue(Invoker.IsValid(), TEXT("MutateAll UFUNCTION should resolve")));
			if (!Invoker.IsValid())
			{
				return;
			}
			FString Label = TEXT("Label");
			FName Name(TEXT("Name"));
			FText Text = FText::FromString(TEXT("Text"));
			Invoker.AddParam(Label);
			Invoker.AddParam(Name);
			Invoker.AddParam(Text);
			ASSERT_THAT(IsTrue(Invoker.Call(), TEXT("MutateAll UFUNCTION should execute")));
			ASSERT_THAT(IsTrue(Invoker.ReadParamAfterCall(0, Label), TEXT("MutateAll should expose FString inout value after call")));
			ASSERT_THAT(IsTrue(Invoker.ReadParamAfterCall(1, Name), TEXT("MutateAll should expose FName inout value after call")));
			ASSERT_THAT(IsTrue(Invoker.ReadParamAfterCall(2, Text), TEXT("MutateAll should expose FText inout value after call")));
			ASSERT_THAT(AreEqual(FString(TEXT("Label|Mutated")), Label, TEXT("UFUNCTION FString &inout parameter")));
			ASSERT_THAT(AreEqual(FName(TEXT("NameMutated")), Name, TEXT("UFUNCTION FName &inout parameter")));
			ASSERT_THAT(AreEqual(FString(TEXT("TextMutated")), Text.ToString(), TEXT("UFUNCTION FText &inout parameter")));
		}
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
