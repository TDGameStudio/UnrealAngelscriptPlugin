#include "CQTest.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestModuleBuilder.h"
#include "AngelscriptTestUtilities.h"

#include "Misc/ScopeExit.h"

// -----------------------------------------------------------------------------
// AngelscriptCoverageFStringExpressionTests
// -----------------------------------------------------------------------------
// Coverage for AngelScript string-family *expression usage* -- the script-side
// half of the string matrix. This file covers:
//
//   * Local/global declarations
//   * Operators (assignment, concatenation, comparison, indexing)
//   * Literals (basic, empty, escape sequences, Unicode)
//   * Type conversions (String↔Name↔Text, String↔int/float)
//   * Class members (non-UPROPERTY)
//
// Test patterns:
//   - Pattern B: Global functions returning values
//   - Pattern F: ExpectGlobalReturn helper
//
// String family under test:
//   FString / FName / FText
// -----------------------------------------------------------------------------

#if WITH_DEV_AUTOMATION_TESTS

TEST_CLASS_WITH_FLAGS(FAngelscriptCoverageFStringExpressionTest,
	"Angelscript.TestModule.Coverage.FStringExpression",
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

	// Helper: build module + expect global return
	template <typename T>
	void ExpectGlobalReturn(FAngelscriptEngine& Engine, asIScriptModule* Module, const TCHAR* Declaration, const T& Expected, const TCHAR* Message)
	{
		ASSERT_THAT(IsNotNull(Module, TEXT("FString expression module should compile before executing global function")));
		if (Module == nullptr)
		{
			return;
		}

		FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, Declaration);
		ASSERT_THAT(IsTrue(Invoker.IsValid(), TEXT("FString expression global function should resolve and prepare")));
		if (!Invoker.IsValid())
		{
			return;
		}

		T Result{};
		if constexpr (std::is_same_v<T, bool>
			|| std::is_same_v<T, int32>
			|| std::is_same_v<T, float>
			|| std::is_same_v<T, double>)
		{
			Result = Invoker.ExecuteAndGet<T>(T{});
		}
		else
		{
			ASSERT_THAT(IsTrue(Invoker.ExecuteAndExtractStruct(Result)));
		}
		ASSERT_THAT(AreEqual(Expected, Result, Message));
	}

	// -------------------------------------------------------------------------
	// Local declaration contexts: default init, deferred init, const.
	// -------------------------------------------------------------------------
	TEST_METHOD(LocalDeclarations)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovFStringExpr_LocalDecl", ASTEST_AS(R"AS(
		FString LocalDefaultInit()
		{
			FString Value = "Hello";
			return Value;
		}

		FString LocalDeferredInit()
		{
			FString Value;
			Value = "World";
			return Value;
		}

		FString LocalConst()
		{
			const FString Value = "Const";
			return Value;
		}

		FString LocalEmpty()
		{
			FString Value = "";
			return Value;
		}

		FString LocalDefaultString()
		{
			FString Value;
			return Value;
		}

		FName LocalName()
		{
			FName Value = n"MyName";
			return Value;
		}

		FName LocalNameDefault()
		{
			FName Value;
			return Value;
		}

		FName LocalNameConst()
		{
			const FName Value = n"ConstName";
			return Value;
		}

		FString LocalNameToString()
		{
			FName Value = n"Convert";
			return Value.ToString();
		}

		FString LocalTextToString()
		{
			FText Value = FText::FromString("Visible Text");
			return Value.ToString();
		}

		FString LocalTextDefaultToString()
		{
			FText Value;
			return Value.ToString();
		}

		FString LocalTextConstToString()
		{
			const FText Value = FText::FromString("Const Text");
			return Value.ToString();
		}

		FString AutoStringLiteral()
		{
			auto Value = "AutoText";
			return Value;
		}
		)AS"));
		ON_SCOPE_EXIT
		{
			if (Module != nullptr)
			{
				Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
			}
		};

		ExpectGlobalReturn<FString>(Engine, Module, TEXT("FString LocalDefaultInit()"), FString(TEXT("Hello")), TEXT("local FString with default initializer"));
		ExpectGlobalReturn<FString>(Engine, Module, TEXT("FString LocalDeferredInit()"), FString(TEXT("World")), TEXT("local FString declared then assigned"));
		ExpectGlobalReturn<FString>(Engine, Module, TEXT("FString LocalConst()"), FString(TEXT("Const")), TEXT("local const FString"));
		ExpectGlobalReturn<FString>(Engine, Module, TEXT("FString LocalEmpty()"), FString(TEXT("")), TEXT("local FString empty"));
		ExpectGlobalReturn<FString>(Engine, Module, TEXT("FString LocalDefaultString()"), FString(TEXT("")), TEXT("local default FString should be empty"));
		ExpectGlobalReturn<FName>(Engine, Module, TEXT("FName LocalName()"), FName(TEXT("MyName")), TEXT("local FName"));
		ExpectGlobalReturn<FName>(Engine, Module, TEXT("FName LocalNameDefault()"), NAME_None, TEXT("local default FName should be NAME_None"));
		ExpectGlobalReturn<FName>(Engine, Module, TEXT("FName LocalNameConst()"), FName(TEXT("ConstName")), TEXT("local const FName"));
		ExpectGlobalReturn<FString>(Engine, Module, TEXT("FString LocalNameToString()"), FString(TEXT("Convert")), TEXT("FName.ToString()"));
		ExpectGlobalReturn<FString>(Engine, Module, TEXT("FString LocalTextToString()"), FString(TEXT("Visible Text")), TEXT("local FText converted to FString"));
		ExpectGlobalReturn<FString>(Engine, Module, TEXT("FString LocalTextDefaultToString()"), FString(TEXT("")), TEXT("local default FText converted to empty FString"));
		ExpectGlobalReturn<FString>(Engine, Module, TEXT("FString LocalTextConstToString()"), FString(TEXT("Const Text")), TEXT("local const FText converted to FString"));
		ExpectGlobalReturn<FString>(Engine, Module, TEXT("FString AutoStringLiteral()"), FString(TEXT("AutoText")), TEXT("auto should infer FString from a string literal"));
	}

	// -------------------------------------------------------------------------
	// Module-level const globals (FString / FName).
	// -------------------------------------------------------------------------
	TEST_METHOD(GlobalConstDeclarations)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovFStringExpr_GlobalConst", ASTEST_AS(R"AS(
		const FString GConstString = "Global";
		const FName GConstName = n"GlobalName";
		const FText GConstText;

		FString GetGlobalString()
		{
			return GConstString;
		}

		FName GetGlobalName()
		{
			return GConstName;
		}

		FString GetGlobalText()
		{
			return GConstText.ToString();
		}
		)AS"));
		ON_SCOPE_EXIT
		{
			if (Module != nullptr)
			{
				Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
			}
		};

		ExpectGlobalReturn<FString>(Engine, Module, TEXT("FString GetGlobalString()"), FString(TEXT("Global")), TEXT("global const FString"));
		ExpectGlobalReturn<FName>(Engine, Module, TEXT("FName GetGlobalName()"), FName(TEXT("GlobalName")), TEXT("global const FName"));
		ExpectGlobalReturn<FString>(Engine, Module, TEXT("FString GetGlobalText()"), FString(TEXT("")), TEXT("global const FText default construction"));
	}

	// -------------------------------------------------------------------------
	// String operators: assignment, concatenation, comparison.
	// -------------------------------------------------------------------------
	TEST_METHOD(StringOperators)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovFStringExpr_Operators", ASTEST_AS(R"AS(
		FString OpAssignment()
		{
			FString a = "First";
			FString b = a;
			return b;
		}

		FString OpConcatenation()
		{
			return "Hello" + " " + "World";
		}

		FString OpConcatAssignment()
		{
			FString s = "Hello";
			s += " World";
			return s;
		}

		bool OpEquals()
		{
			return "Test" == "Test";
		}

		bool OpNotEquals()
		{
			return "ABC" != "XYZ";
		}

		bool OpLessThan()
		{
			return "AAA" < "BBB";
		}

		bool OpGreaterThan()
		{
			return "ZZZ" > "AAA";
		}

		bool OpLessEqual()
		{
			return "AAA" <= "AAA";
		}

		bool OpGreaterEqual()
		{
			return "BBB" >= "AAA";
		}

		int OpIndex()
		{
			FString s = "AZ";
			return s[1];
		}

		bool OpNameEquals()
		{
			FName a = n"Test";
			FName b = n"Test";
			return a == b;
		}

		bool OpNameNotEquals()
		{
			FName a = n"Alpha";
			FName b = n"Beta";
			return a != b;
		}

		bool OpNameEqualsString()
		{
			FName a = n"StringMatch";
			return a.ToString() == "StringMatch";
		}

		bool OpNameReassignmentKeepsPreviousCopiesStable()
		{
			FName original = n"Original";
			FName copy = original;
			original = n"Updated";
			return copy == n"Original" && original == n"Updated";
		}

		bool OpTextIdentical()
		{
			FText a = FText::FromString("Display");
			FText b = FText::FromString("Display");
			return !a.IdenticalTo(b);
		}
		)AS"));
		ON_SCOPE_EXIT
		{
			if (Module != nullptr)
			{
				Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
			}
		};

		ExpectGlobalReturn<FString>(Engine, Module, TEXT("FString OpAssignment()"), FString(TEXT("First")), TEXT("string assignment"));
		ExpectGlobalReturn<FString>(Engine, Module, TEXT("FString OpConcatenation()"), FString(TEXT("Hello World")), TEXT("string concatenation (+)"));
		ExpectGlobalReturn<FString>(Engine, Module, TEXT("FString OpConcatAssignment()"), FString(TEXT("Hello World")), TEXT("string concat assignment (+=)"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool OpEquals()"), true, TEXT("string =="));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool OpNotEquals()"), true, TEXT("string !="));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool OpLessThan()"), true, TEXT("string <"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool OpGreaterThan()"), true, TEXT("string >"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool OpLessEqual()"), true, TEXT("string <="));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool OpGreaterEqual()"), true, TEXT("string >="));
		ExpectGlobalReturn<int32>(Engine, Module, TEXT("int OpIndex()"), static_cast<int32>(TCHAR('Z')), TEXT("string index operator"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool OpNameEquals()"), true, TEXT("FName =="));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool OpNameNotEquals()"), true, TEXT("FName !="));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool OpNameEqualsString()"), true, TEXT("FName.ToString() == FString-compatible literal"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool OpNameReassignmentKeepsPreviousCopiesStable()"), true, TEXT("FName value reassignment should not mutate previous copies"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool OpTextIdentical()"), true, TEXT("FText.IdenticalTo keeps UE identity semantics for separate FromString values"));
	}

	// -------------------------------------------------------------------------
	// String literals: basic, empty, escape sequences, Unicode.
	// -------------------------------------------------------------------------
	TEST_METHOD(StringLiterals)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovFStringExpr_Literals", ASTEST_AS(R"AS(
		FString LiteralBasic()
		{
			return "Hello World";
		}

		FString LiteralEmpty()
		{
			return "";
		}

		FString LiteralNewline()
		{
			return "Line1\nLine2";
		}

		FString LiteralTab()
		{
			return "A\tB";
		}

		FString LiteralQuote()
		{
			return "Say \"Hi\"";
		}

		FString LiteralBackslash()
		{
			return "C:\\Path\\File";
		}

		FString LiteralUnicode()
		{
			return "Hello 世界";
		}

		FName LiteralName()
		{
			return n"TestName";
		}

		FString LiteralLong()
		{
			FString s = "";
			for (int i = 0; i < 1100; ++i)
			{
				s += "x";
			}
			return s;
		}

		int LiteralLongLength()
		{
			return LiteralLong().Len();
		}

		FString LiteralTextConstructor()
		{
			return FText::FromString("TextLiteral").ToString();
		}
		)AS"));
		ON_SCOPE_EXIT
		{
			if (Module != nullptr)
			{
				Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
			}
		};

		ExpectGlobalReturn<FString>(Engine, Module, TEXT("FString LiteralBasic()"), FString(TEXT("Hello World")), TEXT("basic string literal"));
		ExpectGlobalReturn<FString>(Engine, Module, TEXT("FString LiteralEmpty()"), FString(TEXT("")), TEXT("empty string literal"));
		ExpectGlobalReturn<FString>(Engine, Module, TEXT("FString LiteralNewline()"), FString(TEXT("Line1\nLine2")), TEXT("newline escape sequence"));
		ExpectGlobalReturn<FString>(Engine, Module, TEXT("FString LiteralTab()"), FString(TEXT("A\tB")), TEXT("tab escape sequence"));
		ExpectGlobalReturn<FString>(Engine, Module, TEXT("FString LiteralQuote()"), FString(TEXT("Say \"Hi\"")), TEXT("quote escape sequence"));
		ExpectGlobalReturn<FString>(Engine, Module, TEXT("FString LiteralBackslash()"), FString(TEXT("C:\\Path\\File")), TEXT("backslash escape sequence"));
		ExpectGlobalReturn<FString>(Engine, Module, TEXT("FString LiteralUnicode()"), FString(TEXT("Hello 世界")), TEXT("Unicode literal"));
		ExpectGlobalReturn<FName>(Engine, Module, TEXT("FName LiteralName()"), FName(TEXT("TestName")), TEXT("FName literal (n\"\")"));
		ExpectGlobalReturn<int32>(Engine, Module, TEXT("int LiteralLongLength()"), 1100, TEXT("long string literal-equivalent construction"));
		ExpectGlobalReturn<FString>(Engine, Module, TEXT("FString LiteralTextConstructor()"), FString(TEXT("TextLiteral")), TEXT("FText literal via FromString"));
	}

	// -------------------------------------------------------------------------
	// Type conversions: String↔Name↔Text, String↔int/float.
	// -------------------------------------------------------------------------
	TEST_METHOD(StringConversions)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovFStringExpr_Conversion", ASTEST_AS(R"AS(
		FName StringToName()
		{
			FString s = "Convert";
			return FName(s);
		}

		FString NameToString()
		{
			FName n = n"MyName";
			return n.ToString();
		}

		FString TextToString()
		{
			FText t = FText::FromString("TextValue");
			return t.ToString();
		}

		FString StringToTextToString()
		{
			FString s = "FromString";
			FText t = FText::FromString(s);
			return t.ToString();
		}

		FString IntToString()
		{
			int x = 123;
			return FString::FromInt(x);
		}

		FString FloatToString()
		{
			return FString::SanitizeFloat(2.5);
		}
		)AS"));
		ON_SCOPE_EXIT
		{
			if (Module != nullptr)
			{
				Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
			}
		};

		ExpectGlobalReturn<FName>(Engine, Module, TEXT("FName StringToName()"), FName(TEXT("Convert")), TEXT("FString -> FName"));
		ExpectGlobalReturn<FString>(Engine, Module, TEXT("FString NameToString()"), FString(TEXT("MyName")), TEXT("FName -> FString"));
		ExpectGlobalReturn<FString>(Engine, Module, TEXT("FString TextToString()"), FString(TEXT("TextValue")), TEXT("FText -> FString"));
		ExpectGlobalReturn<FString>(Engine, Module, TEXT("FString StringToTextToString()"), FString(TEXT("FromString")), TEXT("FString -> FText -> FString"));
		ExpectGlobalReturn<FString>(Engine, Module, TEXT("FString IntToString()"), FString(TEXT("123")), TEXT("int -> FString"));
		ExpectGlobalReturn<FString>(Engine, Module, TEXT("FString FloatToString()"), FString(TEXT("2.5")), TEXT("float -> FString"));
	}

	TEST_METHOD(NameAndTextSpecificOperations)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovFStringExpr_NameTextSpecific", ASTEST_AS(R"AS(
		bool DefaultNameIsNone()
		{
			FName Value;
			return Value.IsNone();
		}

		FString PlainNameString()
		{
			FName Value = FName("Plain_17");
			return Value.GetPlainNameString();
		}

		bool NameCaseInsensitiveEquality()
		{
			FName Lower = FName("display");
			FName Upper = FName("DISPLAY");
			return Lower.IsEqual(Upper);
		}

		bool NameCaseSensitiveInequality()
		{
			FName Lower = FName("display");
			FName Upper = FName("DISPLAY");
			return !Lower.IsEqual(Upper, false);
		}

		bool NameCompareOrdersValues()
		{
			FName Alpha = n"Alpha";
			FName Beta = n"Beta";
			return Alpha.Compare(Beta) < 0 && Beta.Compare(Alpha) > 0;
		}

		bool NameHashIsStable()
		{
			FName Value = n"StableHash";
			return Value.GetHash() == Value.GetHash();
		}

		bool TextFromStringState()
		{
			FText Value = FText::FromString("State");
			return Value.IsInitializedFromString() && !Value.IsEmpty();
		}

		bool CultureInvariantTextState()
		{
			FText Value = FText::AsCultureInvariant("Invariant");
			return Value.IsCultureInvariant() && Value.ToString() == "Invariant";
		}

		FString TextFromName()
		{
			return FText::FromName(n"NameText").ToString();
		}

		FString TextFormatOrdered()
		{
			FText Pattern = FText::FromString("{0}:{1}");
			return FText::Format(Pattern, FText::FromString("A"), 7).ToString();
		}

		int TextFormatPatternParameterCount()
		{
			TArray<FString> Names;
			FText::GetFormatPatternParameters(FText::FromString("{First}-{Second}"), Names);
			return Names.Num();
		}

		FString TextJoin()
		{
			TArray<FText> Parts;
			Parts.Add(FText::FromString("A"));
			Parts.Add(FText::FromString("B"));
			return FText::Join(FText::FromString("|"), Parts).ToString();
		}
		)AS"));
		ON_SCOPE_EXIT
		{
			if (Module != nullptr)
			{
				Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
			}
		};

		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool DefaultNameIsNone()"), true, TEXT("default FName should be NAME_None"));
		ExpectGlobalReturn<FString>(Engine, Module, TEXT("FString PlainNameString()"), FString(TEXT("Plain")), TEXT("FName.GetPlainNameString() strips numbered suffixes"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool NameCaseInsensitiveEquality()"), true, TEXT("FName.IsEqual defaults to case-insensitive comparison"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool NameCaseSensitiveInequality()"), true, TEXT("FName.IsEqual can compare case-sensitively"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool NameCompareOrdersValues()"), true, TEXT("FName.Compare() should expose ordering semantics"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool NameHashIsStable()"), true, TEXT("FName.GetHash() should be stable for the same value"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool TextFromStringState()"), true, TEXT("FText.FromString state predicates"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool CultureInvariantTextState()"), true, TEXT("FText.AsCultureInvariant state predicates"));
		ExpectGlobalReturn<FString>(Engine, Module, TEXT("FString TextFromName()"), FString(TEXT("NameText")), TEXT("FText.FromName()"));
		ExpectGlobalReturn<FString>(Engine, Module, TEXT("FString TextFormatOrdered()"), FString(TEXT("A:7")), TEXT("FText.Format ordered arguments"));
		ExpectGlobalReturn<int32>(Engine, Module, TEXT("int TextFormatPatternParameterCount()"), 2, TEXT("FText.GetFormatPatternParameters()"));
		ExpectGlobalReturn<FString>(Engine, Module, TEXT("FString TextJoin()"), FString(TEXT("A|B")), TEXT("FText.Join(TArray<FText>)"));
	}

	TEST_METHOD(UnsupportedStringExpressionBoundaries)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		{
			const TArray<FString> ExpectedDiagnostics = { TEXT("Global variable 'GMutable' must be const. Mutable global variables are not supported.") };
			ASSERT_THAT(IsTrue(CompileAndExpectFailure(
				*TestRunner,
				Engine,
				TEXT("ASCovFStringExpr_GlobalMutableUnsupported"),
				ASTEST_AS(R"AS(
				FString GMutable = "Mutable";

				FString ReadMutable()
				{
					return GMutable;
				}
				)AS"),
				TEXT("mutable module-level FString globals should remain unsupported"),
				MakeArrayView(ExpectedDiagnostics))));
		}

		{
			const TArray<FString> ExpectedDiagnostics = { TEXT("ToInt") };
			ASSERT_THAT(IsTrue(CompileAndExpectFailure(
				*TestRunner,
				Engine,
				TEXT("ASCovFStringExpr_ToIntMethodUnsupported"),
				ASTEST_AS(R"AS(
				int TryStringToIntMethod()
				{
					FString Value = "42";
					return Value.ToInt();
				}
				)AS"),
				TEXT("FString.ToInt() should remain an explicit unsupported boundary"),
				MakeArrayView(ExpectedDiagnostics))));
		}

		{
			const TArray<FString> ExpectedDiagnostics = { TEXT("ToFloat") };
			ASSERT_THAT(IsTrue(CompileAndExpectFailure(
				*TestRunner,
				Engine,
				TEXT("ASCovFStringExpr_ToFloatMethodUnsupported"),
				ASTEST_AS(R"AS(
				float TryStringToFloatMethod()
				{
					FString Value = "3.14";
					return Value.ToFloat();
				}
				)AS"),
				TEXT("FString.ToFloat() should remain an explicit unsupported boundary"),
				MakeArrayView(ExpectedDiagnostics))));
		}

		{
			const TArray<FString> ExpectedDiagnostics = { TEXT("Namespace 'FCString' doesn't exist") };
			ASSERT_THAT(IsTrue(CompileAndExpectFailure(
				*TestRunner,
				Engine,
				TEXT("ASCovFStringExpr_FCStringAtoiUnsupported"),
				ASTEST_AS(R"AS(
				int TryFCStringAtoi()
				{
					FString Value = "42";
					return FCString::Atoi(Value);
				}
				)AS"),
				TEXT("FCString::Atoi should remain an explicit unsupported boundary"),
				MakeArrayView(ExpectedDiagnostics))));
		}

		{
			const TArray<FString> ExpectedDiagnostics = { TEXT("Namespace 'FCString' doesn't exist") };
			ASSERT_THAT(IsTrue(CompileAndExpectFailure(
				*TestRunner,
				Engine,
				TEXT("ASCovFStringExpr_FCStringAtofUnsupported"),
				ASTEST_AS(R"AS(
				float TryFCStringAtof()
				{
					FString Value = "3.14";
					return FCString::Atof(Value);
				}
				)AS"),
				TEXT("FCString::Atof should remain an explicit unsupported boundary"),
				MakeArrayView(ExpectedDiagnostics))));
		}

		{
			const TArray<FString> ExpectedDiagnostics = { TEXT("No matching operator") };
			ASSERT_THAT(IsTrue(CompileAndExpectFailure(
				*TestRunner,
				Engine,
				TEXT("ASCovFStringExpr_FNameOrderingOperatorUnsupported"),
				ASTEST_AS(R"AS(
				bool TryNameOrdering()
				{
					FName Left = n"Alpha";
					FName Right = n"Beta";
					return Left < Right;
				}
				)AS"),
				TEXT("FName ordering operator syntax should remain unsupported; use Compare instead"),
				MakeArrayView(ExpectedDiagnostics))));
		}

		{
			const TArray<FString> ExpectedDiagnostics = { TEXT("No matching operator") };
			ASSERT_THAT(IsTrue(CompileAndExpectFailure(
				*TestRunner,
				Engine,
				TEXT("ASCovFStringExpr_FTextEqualsOperatorUnsupported"),
				ASTEST_AS(R"AS(
				bool TryTextEqualsOperator()
				{
					FText Left = FText::FromString("A");
					FText Right = FText::FromString("A");
					return Left == Right;
				}
				)AS"),
				TEXT("FText equality operator syntax should remain unsupported; use IdenticalTo instead"),
				MakeArrayView(ExpectedDiagnostics))));
		}

		{
			const TArray<FString> ExpectedDiagnostics = { TEXT("No matching operator") };
			ASSERT_THAT(IsTrue(CompileAndExpectFailure(
				*TestRunner,
				Engine,
				TEXT("ASCovFStringExpr_FTextOrderingUnsupported"),
				ASTEST_AS(R"AS(
				bool TryTextOrdering()
				{
					FText Left = FText::FromString("A");
					FText Right = FText::FromString("B");
					return Left < Right;
				}
				)AS"),
				TEXT("FText ordering operators should remain unsupported"),
				MakeArrayView(ExpectedDiagnostics))));
		}
	}

	TEST_METHOD(NameAndTextComparisonOperators)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovFStringExpr_NameTextComparisons", ASTEST_AS(R"AS(
		bool NameCompareOrdersValues()
		{
			FName Left = n"Alpha";
			FName Right = n"Beta";
			return Left.Compare(Right) < 0 && Right.Compare(Left) > 0;
		}

		bool TextIdentical()
		{
			FText Left = FText::FromString("A");
			FText Right = FText::FromString("A");
			return !Left.IdenticalTo(Right);
		}
		)AS"));
		ON_SCOPE_EXIT
		{
			if (Module != nullptr)
			{
				Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
			}
		};

		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool NameCompareOrdersValues()"), true, TEXT("FName.Compare() should expose ordering semantics"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool TextIdentical()"), true, TEXT("FText.IdenticalTo keeps UE identity semantics for separate FromString values"));
	}

	// -------------------------------------------------------------------------
	// Class members (non-UPROPERTY): script-visible string fields without
	// reflection, accessed directly within script code.
	// -------------------------------------------------------------------------
	// Script class members are a current execution boundary in this fork for
	// FString/FName/FText, matching the primitive coverage files.
	TEST_METHOD(ClassMembersNonProperty)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovFStringExpr_ClassMember", ASTEST_AS(R"AS(
		class StringHolder
		{
			FString Value;
			FName NameValue;
			FText TextValue;

			StringHolder()
			{
				Value = "Initial";
				NameValue = n"Tag";
				TextValue = FText::FromString("TextMember");
			}

			FString GetValue() const
			{
				return Value;
			}

			void SetValue(FString v)
			{
				Value = v;
			}

			FName GetNameValue() const
			{
				return NameValue;
			}

			FString GetTextValue() const
			{
				return TextValue.ToString();
			}
		}

		FString TestClassMemberAccess()
		{
			StringHolder holder;
			return holder.Value;
		}

		FString TestClassMemberModify()
		{
			StringHolder holder;
			holder.Value = "Modified";
			return holder.GetValue();
		}

		FName TestClassMemberName()
		{
			StringHolder holder;
			return holder.NameValue;
		}

		FString TestClassMemberText()
		{
			StringHolder holder;
			return holder.GetTextValue();
		}
		)AS"));
		ON_SCOPE_EXIT
		{
			if (Module != nullptr)
			{
				Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
			}
		};

		ASSERT_THAT(IsNotNull(Module, TEXT("FString class member boundary module should compile")));
		if (Module == nullptr)
		{
			return;
		}

		TestRunner->AddExpectedError(TEXT("Null pointer access"), EAutomationExpectedErrorFlags::Contains, 0);
		TestRunner->AddExpectedError(TEXT("ASCovFStringExpr_ClassMember"), EAutomationExpectedErrorFlags::Contains, 0);
		TestRunner->AddExpectedError(TEXT("TestClassMember"), EAutomationExpectedErrorFlags::Contains, 4);

		ASSERT_THAT(IsTrue(ExecuteAndExpectException(
			*TestRunner,
			Engine,
			*Module,
			TEXT("FString TestClassMemberAccess()"),
			TEXT("FString class member direct access currently remains a script-class execution boundary"),
			TEXT("Null pointer access"))));
		ASSERT_THAT(IsTrue(ExecuteAndExpectException(
			*TestRunner,
			Engine,
			*Module,
			TEXT("FString TestClassMemberModify()"),
			TEXT("FString class member modify currently remains a script-class execution boundary"),
			TEXT("Null pointer access"))));
		ASSERT_THAT(IsTrue(ExecuteAndExpectException(
			*TestRunner,
			Engine,
			*Module,
			TEXT("FName TestClassMemberName()"),
			TEXT("FName class member access currently remains a script-class execution boundary"),
			TEXT("Null pointer access"))));
		ASSERT_THAT(IsTrue(ExecuteAndExpectException(
			*TestRunner,
			Engine,
			*Module,
			TEXT("FString TestClassMemberText()"),
			TEXT("FText class member access currently remains a script-class execution boundary"),
			TEXT("Null pointer access"))));
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
