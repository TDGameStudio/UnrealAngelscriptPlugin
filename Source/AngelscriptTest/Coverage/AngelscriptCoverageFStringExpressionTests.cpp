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

		FName LocalName()
		{
			FName Value = n"MyName";
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
		ExpectGlobalReturn<FName>(Engine, Module, TEXT("FName LocalName()"), FName(TEXT("MyName")), TEXT("local FName"));
		ExpectGlobalReturn<FString>(Engine, Module, TEXT("FString LocalNameToString()"), FString(TEXT("Convert")), TEXT("FName.ToString()"));
		ExpectGlobalReturn<FString>(Engine, Module, TEXT("FString LocalTextToString()"), FString(TEXT("Visible Text")), TEXT("local FText converted to FString"));
		ExpectGlobalReturn<FString>(Engine, Module, TEXT("FString LocalTextDefaultToString()"), FString(TEXT("")), TEXT("local default FText converted to empty FString"));
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

		bool OpTextIdentical()
		{
			FText a = FText::FromString("Display");
			FText b = FText::FromString("Display");
			return a.IdenticalTo(b);
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
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool OpTextIdentical()"), true, TEXT("FText IdenticalTo comparison"));
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

		int StringToInt()
		{
			FString s = "42";
			return FCString::Atoi(s);
		}

		float StringToFloat()
		{
			FString s = "3.14";
			return FCString::Atof(s);
		}

		FString IntToString()
		{
			int x = 123;
			return FString::FromInt(x);
		}

		FString FloatToString()
		{
			return FString::Format("{0}", 2.5f);
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
		ExpectGlobalReturn<int32>(Engine, Module, TEXT("int StringToInt()"), 42, TEXT("FString -> int"));

		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("float StringToFloat()"));
			ASSERT_THAT(IsTrue(Invoker.IsValid(), TEXT("StringToFloat should resolve and prepare")));
			if (!Invoker.IsValid())
			{
				return;
			}
			float Result = Invoker.ExecuteAndGet<float>(0.0f);
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Result, 3.14f, 0.01f), TEXT("FString -> float")));
		}

		ExpectGlobalReturn<FString>(Engine, Module, TEXT("FString IntToString()"), FString(TEXT("123")), TEXT("int -> FString"));
		ExpectGlobalReturn<FString>(Engine, Module, TEXT("FString FloatToString()"), FString(TEXT("2.5")), TEXT("float -> FString"));
	}

	// -------------------------------------------------------------------------
	// Class members (non-UPROPERTY): script-visible string fields without
	// reflection, accessed directly within script code.
	// -------------------------------------------------------------------------
	TEST_METHOD(ClassMembersNonProperty)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovFStringExpr_ClassMember", ASTEST_AS(R"AS(
		class StringHolder
		{
			FString Value;
			FName NameValue;

			StringHolder()
			{
				Value = "Initial";
				NameValue = n"Tag";
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
		)AS"));
		ON_SCOPE_EXIT
		{
			if (Module != nullptr)
			{
				Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
			}
		};

		ExpectGlobalReturn<FString>(Engine, Module, TEXT("FString TestClassMemberAccess()"), FString(TEXT("Initial")), TEXT("class member FString direct access"));
		ExpectGlobalReturn<FString>(Engine, Module, TEXT("FString TestClassMemberModify()"), FString(TEXT("Modified")), TEXT("class member FString modify"));
		ExpectGlobalReturn<FName>(Engine, Module, TEXT("FName TestClassMemberName()"), FName(TEXT("Tag")), TEXT("class member FName"));
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
