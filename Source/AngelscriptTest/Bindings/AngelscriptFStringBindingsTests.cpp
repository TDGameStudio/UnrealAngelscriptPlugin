// ============================================================================
// AngelscriptFStringBindingsTests.cpp
//
// FString binding contract smoke. Broad FString semantics live in Coverage
// (`01-basic-types`), while this file proves AS binding entrypoints exist and
// dispatch.
// ============================================================================

#include "CQTest.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestModuleScope.h"
#include "AngelscriptTestExecute.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FAngelscriptFStringBindingsTest,
	"Angelscript.TestModule.Bindings.FString",
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

	TEST_METHOD(Construction)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule ModuleScope(*TestRunner, Engine, TEXT("ASFString_ConstructionContract"), ASTEST_AS(R"AS(
			int VerifyConstructionContract()
			{
				FString Empty;
				FString Literal = "Hello";
				FString Copy = Literal;
				Copy.Append("!");
				return Empty.IsEmpty() && Literal == "Hello" && Copy == "Hello!" ? 1 : 0;
			}
			)AS"));
		if (!ModuleScope.IsValid()) return;

		ASSERT_THAT(IsTrue(ExpectGlobalInt(
			*TestRunner,
			Engine,
			ModuleScope.GetModule(),
			TEXT("int VerifyConstructionContract()"),
			TEXT("FString should construct from default, literal, and copy paths"),
			1)));
	}

	TEST_METHOD(Operators)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule ModuleScope(*TestRunner, Engine, TEXT("ASFString_OperatorContract"), ASTEST_AS(R"AS(
			int VerifyOperatorContract()
			{
				FString Value = "AB";
				Value += "C";
				return Value == "ABC" && Value.opCmp("ABD") < 0 && Value[1] == 0x42 ? 1 : 0;
			}
			)AS"));
		if (!ModuleScope.IsValid()) return;

		ASSERT_THAT(IsTrue(ExpectGlobalInt(
			*TestRunner,
			Engine,
			ModuleScope.GetModule(),
			TEXT("int VerifyOperatorContract()"),
			TEXT("FString equality, compare, concat-assign, and index operators should dispatch"),
			1)));
	}

	TEST_METHOD(OperatorIndexError)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule ModuleScope(*TestRunner, Engine, TEXT("ASFString_IndexContract"), ASTEST_AS(R"AS(
			void TriggerIndexOutOfBounds()
			{
				FString Value = "AB";
				int16 Character = Value[10];
			}
			)AS"));
		if (!ModuleScope.IsValid()) return;

		TestRunner->AddExpectedErrorPlain(TEXT("ASFString_IndexContract"), EAutomationExpectedErrorFlags::Contains, 0);
		TestRunner->AddExpectedErrorPlain(TEXT("String index out of bounds"), EAutomationExpectedErrorFlags::Contains, 0);
		TestRunner->AddExpectedErrorPlain(TEXT("void TriggerIndexOutOfBounds()"), EAutomationExpectedErrorFlags::Contains, 0);

		ASSERT_THAT(IsTrue(ExecuteFunctionExpectingScriptException(
			*TestRunner,
			Engine,
			ModuleScope.GetModule(),
			TEXT("void TriggerIndexOutOfBounds()"),
			TEXT("FString opIndex out-of-bounds should keep the binding guard"),
			FString(TEXT("String index out of bounds")))));
	}

	TEST_METHOD(TypeConcat)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule ModuleScope(*TestRunner, Engine, TEXT("ASFString_TypeConcatContract"), ASTEST_AS(R"AS(
			int VerifyTypeConcatContract()
			{
				FString Value = "Count=" + 42 + " Name=" + FName("BoundName") + " Flag=" + true;
				FString Mutable = "Index=";
				Mutable += 7;
				Mutable
					.Append(" Flag=")
					.Append(true);

				FString ConvertedName(FName("ConvertedName"));
				FString DirectName = FName("DirectName").ToString();
				return Value.Contains("Count=42")
					&& Value.Contains("BoundName")
					&& Value.Contains("True")
					&& Mutable == "Index=7 Flag=True"
					&& ConvertedName == "ConvertedName"
					&& DirectName == "DirectName" ? 1 : 0;
			}
			)AS"));
		if (!ModuleScope.IsValid()) return;

		ASSERT_THAT(IsTrue(ExpectGlobalInt(
			*TestRunner,
			Engine,
			ModuleScope.GetModule(),
			TEXT("int VerifyTypeConcatContract()"),
			TEXT("FString mixed-type operators, append, implicit construction, and ToString should dispatch through FToStringHelper paths"),
			1)));
	}

	TEST_METHOD(StaticConstruction)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule ModuleScope(*TestRunner, Engine, TEXT("ASFString_StaticContract"), ASTEST_AS(R"AS(
			int VerifyStaticFunctionContract()
			{
				TArray<FString> Parts;
				Parts.Add("A");
				Parts.Add("B");
				FString Joined = FString::Join(Parts, ",");
				FString Formatted = FString::Format("{0}:{1}", "Value", 7);
				FString Applied = FString::ApplyFormat(255, "x");
				FString FromInt = FString::FromInt(42);
				return Joined == "A,B"
					&& Formatted == "Value:7"
					&& Applied.ToLower().Contains("ff")
					&& FromInt == "42" ? 1 : 0;
			}
			)AS"));
		if (!ModuleScope.IsValid()) return;

		ASSERT_THAT(IsTrue(ExpectGlobalInt(
			*TestRunner,
			Engine,
			ModuleScope.GetModule(),
			TEXT("int VerifyStaticFunctionContract()"),
			TEXT("FString namespace/static helpers should resolve and dispatch"),
			1)));
	}

	TEST_METHOD(ManipulationAndParsing)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		const FString ScriptSource = ASTEST_AS(R"AS(
			int VerifyManipulationAndParsingContract()
			{
				FString Clean = "\t  Alpha-Beta  "
					.ConvertTabsToSpaces(2)
					.TrimStartAndEnd()
					.Replace("Beta", "Gamma", ESearchCase::CaseSensitive);

				TArray<FString> Parts;
				const int Count = Clean.ParseIntoArray(Parts, "-", true);

				bool bQuotesRemoved = false;
				const FString Unquoted = "\"Quoted\"".TrimQuotes(bQuotesRemoved);

				return Clean == "Alpha-Gamma"
					&& Count == 2
					&& Parts[0] == "Alpha"
					&& Parts[1] == "Gamma"
					&& bQuotesRemoved
					&& Unquoted == "Quoted" ? 1 : 0;
			}
			)AS");

		FScopedAngelscriptModule ModuleScope(
			*TestRunner,
			Engine,
			TEXT("ASFString_ManipulationAndParsingContract"),
			ScriptSource);
		ASSERT_THAT(IsTrue(ModuleScope.IsValid(), TEXT("FString manipulation and parsing surface should compile")));
		ASSERT_THAT(IsTrue(ExpectGlobalInt(
			*TestRunner,
			Engine,
			ModuleScope.GetModule(),
			TEXT("int VerifyManipulationAndParsingContract()"),
			TEXT("FString manipulation and parsing callables should dispatch"),
			1)));
	}

	TEST_METHOD(ReturnFString)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule ModuleScope(*TestRunner, Engine, TEXT("ASFString_ReturnContract"), ASTEST_AS(R"AS(
			FString MakeReturnedString()
			{
				return FString::Format("{0}-{1}", "Return", 7);
			}
			)AS"));
		if (!ModuleScope.IsValid()) return;

		ASSERT_THAT(IsTrue(ExpectGlobalReturnCustom<FString>(
			*TestRunner,
			Engine,
			ModuleScope.GetModule(),
			TEXT("FString MakeReturnedString()"),
			TEXT("FString return values should cross the AS/C++ boundary"),
			[](FAutomationTestBase& Test, const FString& Actual) -> bool
			{
				return Test.TestEqual(TEXT("Returned FString content"), Actual, FString(TEXT("Return-7")));
			})));
	}

	TEST_METHOD(PassFString)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule ModuleScope(*TestRunner, Engine, TEXT("ASFString_PassContract"), ASTEST_AS(R"AS(
			FString EchoWithSuffix(const FString& in Value)
			{
				return Value + "_AS";
			}
			)AS"));
		if (!ModuleScope.IsValid()) return;

		FString Input = TEXT("Native");
		FASGlobalFunctionInvoker Invoker(
			*TestRunner,
			Engine,
			ModuleScope.GetModule(),
			TEXT("FString EchoWithSuffix(const FString& in)"));
		Invoker.AddArgRef(Input);
		ASSERT_THAT(IsTrue(Invoker.Call(), TEXT("FString const-ref input call should execute")));

		FString Result;
		ASSERT_THAT(IsTrue(Invoker.ReadReturnStruct(Result), TEXT("FString return should be readable")));
		ASSERT_THAT(AreEqual(FString(TEXT("Native_AS")), Result, TEXT("FString const-ref input should reach script and return")));
	}
};

#endif
