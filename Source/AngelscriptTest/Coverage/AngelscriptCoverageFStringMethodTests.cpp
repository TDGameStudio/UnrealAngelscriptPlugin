#include "CQTest.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestModuleBuilder.h"
#include "AngelscriptTestUtilities.h"

#include "Misc/ScopeExit.h"

// -----------------------------------------------------------------------------
// AngelscriptCoverageFStringMethodTests
// -----------------------------------------------------------------------------
// Coverage for FString methods - the unique feature of FString that FName and
// FText don't have. This tests the rich API for string manipulation.
//
// Key method groups:
//   - Length & empty: Len(), IsEmpty()
//   - Search: Find(), Contains(), StartsWith(), EndsWith()
//   - Manipulation: ToUpper(), ToLower(), Trim*()
//   - Substring: Left(), Right(), Mid()
//   - Split: Split(), ParseIntoArray()
//   - Replace: Replace(), ReplaceInline()
//   - Format: Printf()
//   - Conversion: ToInt(), ToFloat()
//
// Test pattern: Pattern B/F (global functions)
// -----------------------------------------------------------------------------

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptFunctionalTestUtils;

TEST_CLASS_WITH_FLAGS(FAngelscriptCoverageFStringMethodTest,
	"Angelscript.TestModule.Coverage.FStringMethod",
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

	// Helper
	template <typename T>
	void ExpectGlobalReturn(FAngelscriptEngine& Engine, asIScriptModule* Module, const TCHAR* Declaration, const T& Expected, const TCHAR* Message)
	{
		FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, Declaration);
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
		TestRunner->TestEqual(Message, Result, Expected);
	}

	// -------------------------------------------------------------------------
	// Length and empty checks: Len(), IsEmpty().
	// -------------------------------------------------------------------------
	TEST_METHOD(LengthAndEmpty)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovFStringMethod_Length", ASTEST_AS(R"AS(
		int TestLen()
		{
			FString s = "Hello";
			return s.Len();
		}

		bool TestIsEmpty_Empty()
		{
			FString s = "";
			return s.IsEmpty();
		}

		bool TestIsEmpty_NonEmpty()
		{
			FString s = "Test";
			return s.IsEmpty();
		}

		int TestLenLong()
		{
			FString s = "This is a longer string";
			return s.Len();
		}
		)AS"));
		ON_SCOPE_EXIT
		{
			if (Module != nullptr)
			{
				Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
			}
		};

		ExpectGlobalReturn<int32>(Engine, Module, TEXT("int TestLen()"), 5, TEXT("FString.Len()"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool TestIsEmpty_Empty()"), true, TEXT("FString.IsEmpty() on empty"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool TestIsEmpty_NonEmpty()"), false, TEXT("FString.IsEmpty() on non-empty"));
		ExpectGlobalReturn<int32>(Engine, Module, TEXT("int TestLenLong()"), 23, TEXT("FString.Len() on long string"));
	}

	// -------------------------------------------------------------------------
	// Search methods: Contains(), StartsWith(), EndsWith(), Find().
	// -------------------------------------------------------------------------
	TEST_METHOD(SearchMethods)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovFStringMethod_Search", ASTEST_AS(R"AS(
		bool TestContains_Found()
		{
			FString s = "Hello World";
			return s.Contains("World");
		}

		bool TestContains_NotFound()
		{
			FString s = "Hello World";
			return s.Contains("Test");
		}

		bool TestStartsWith_True()
		{
			FString s = "Hello World";
			return s.StartsWith("Hello");
		}

		bool TestStartsWith_False()
		{
			FString s = "Hello World";
			return s.StartsWith("World");
		}

		bool TestEndsWith_True()
		{
			FString s = "Hello World";
			return s.EndsWith("World");
		}

		bool TestEndsWith_False()
		{
			FString s = "Hello World";
			return s.EndsWith("Hello");
		}

		int TestFind_Found()
		{
			FString s = "Hello World";
			return s.Find("World");
		}

		int TestFind_NotFound()
		{
			FString s = "Hello World";
			return s.Find("Test");
		}
		)AS"));
		ON_SCOPE_EXIT
		{
			if (Module != nullptr)
			{
				Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
			}
		};

		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool TestContains_Found()"), true, TEXT("Contains() found"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool TestContains_NotFound()"), false, TEXT("Contains() not found"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool TestStartsWith_True()"), true, TEXT("StartsWith() true"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool TestStartsWith_False()"), false, TEXT("StartsWith() false"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool TestEndsWith_True()"), true, TEXT("EndsWith() true"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool TestEndsWith_False()"), false, TEXT("EndsWith() false"));
		ExpectGlobalReturn<int32>(Engine, Module, TEXT("int TestFind_Found()"), 6, TEXT("Find() returns index"));
		ExpectGlobalReturn<int32>(Engine, Module, TEXT("int TestFind_NotFound()"), -1, TEXT("Find() returns -1 when not found"));
	}

	// -------------------------------------------------------------------------
	// Case conversion: ToUpper(), ToLower().
	// -------------------------------------------------------------------------
	TEST_METHOD(CaseConversion)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovFStringMethod_Case", ASTEST_AS(R"AS(
		FString TestToUpper()
		{
			FString s = "hello world";
			return s.ToUpper();
		}

		FString TestToLower()
		{
			FString s = "HELLO WORLD";
			return s.ToLower();
		}

		FString TestToUpperMixed()
		{
			FString s = "HeLLo WoRLd";
			return s.ToUpper();
		}
		)AS"));
		ON_SCOPE_EXIT
		{
			if (Module != nullptr)
			{
				Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
			}
		};

		ExpectGlobalReturn<FString>(Engine, Module, TEXT("FString TestToUpper()"), FString(TEXT("HELLO WORLD")), TEXT("ToUpper()"));
		ExpectGlobalReturn<FString>(Engine, Module, TEXT("FString TestToLower()"), FString(TEXT("hello world")), TEXT("ToLower()"));
		ExpectGlobalReturn<FString>(Engine, Module, TEXT("FString TestToUpperMixed()"), FString(TEXT("HELLO WORLD")), TEXT("ToUpper() on mixed case"));
	}

	// -------------------------------------------------------------------------
	// Trim methods: TrimStart(), TrimEnd(), TrimStartAndEnd().
	// -------------------------------------------------------------------------
	TEST_METHOD(TrimMethods)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovFStringMethod_Trim", ASTEST_AS(R"AS(
		FString TestTrimStart()
		{
			FString s = "   Hello";
			return s.TrimStart();
		}

		FString TestTrimEnd()
		{
			FString s = "Hello   ";
			return s.TrimEnd();
		}

		FString TestTrimStartAndEnd()
		{
			FString s = "   Hello   ";
			return s.TrimStartAndEnd();
		}

		FString TestTrimNone()
		{
			FString s = "Hello";
			return s.TrimStartAndEnd();
		}
		)AS"));
		ON_SCOPE_EXIT
		{
			if (Module != nullptr)
			{
				Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
			}
		};

		ExpectGlobalReturn<FString>(Engine, Module, TEXT("FString TestTrimStart()"), FString(TEXT("Hello")), TEXT("TrimStart()"));
		ExpectGlobalReturn<FString>(Engine, Module, TEXT("FString TestTrimEnd()"), FString(TEXT("Hello")), TEXT("TrimEnd()"));
		ExpectGlobalReturn<FString>(Engine, Module, TEXT("FString TestTrimStartAndEnd()"), FString(TEXT("Hello")), TEXT("TrimStartAndEnd()"));
		ExpectGlobalReturn<FString>(Engine, Module, TEXT("FString TestTrimNone()"), FString(TEXT("Hello")), TEXT("TrimStartAndEnd() with no spaces"));
	}

	// -------------------------------------------------------------------------
	// Substring methods: Left(), Right(), Mid().
	// -------------------------------------------------------------------------
	TEST_METHOD(SubstringMethods)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovFStringMethod_Substring", ASTEST_AS(R"AS(
		FString TestLeft()
		{
			FString s = "Hello World";
			return s.Left(5);
		}

		FString TestRight()
		{
			FString s = "Hello World";
			return s.Right(5);
		}

		FString TestMid()
		{
			FString s = "Hello World";
			return s.Mid(6, 5);
		}

		FString TestMidToEnd()
		{
			FString s = "Hello World";
			return s.Mid(6);
		}
		)AS"));
		ON_SCOPE_EXIT
		{
			if (Module != nullptr)
			{
				Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
			}
		};

		ExpectGlobalReturn<FString>(Engine, Module, TEXT("FString TestLeft()"), FString(TEXT("Hello")), TEXT("Left()"));
		ExpectGlobalReturn<FString>(Engine, Module, TEXT("FString TestRight()"), FString(TEXT("World")), TEXT("Right()"));
		ExpectGlobalReturn<FString>(Engine, Module, TEXT("FString TestMid()"), FString(TEXT("World")), TEXT("Mid() with count"));
		ExpectGlobalReturn<FString>(Engine, Module, TEXT("FString TestMidToEnd()"), FString(TEXT("World")), TEXT("Mid() to end"));
	}

	// -------------------------------------------------------------------------
	// Replace methods: Replace().
	// -------------------------------------------------------------------------
	TEST_METHOD(ReplaceMethods)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovFStringMethod_Replace", ASTEST_AS(R"AS(
		FString TestReplace()
		{
			FString s = "Hello World";
			return s.Replace("World", "Universe");
		}

		FString TestReplaceMultiple()
		{
			FString s = "apple apple apple";
			return s.Replace("apple", "orange");
		}

		FString TestReplaceNotFound()
		{
			FString s = "Hello World";
			return s.Replace("Test", "New");
		}
		)AS"));
		ON_SCOPE_EXIT
		{
			if (Module != nullptr)
			{
				Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
			}
		};

		ExpectGlobalReturn<FString>(Engine, Module, TEXT("FString TestReplace()"), FString(TEXT("Hello Universe")), TEXT("Replace() single"));
		ExpectGlobalReturn<FString>(Engine, Module, TEXT("FString TestReplaceMultiple()"), FString(TEXT("orange orange orange")), TEXT("Replace() multiple"));
		ExpectGlobalReturn<FString>(Engine, Module, TEXT("FString TestReplaceNotFound()"), FString(TEXT("Hello World")), TEXT("Replace() not found"));
	}

	// -------------------------------------------------------------------------
	// Split methods: Split().
	// -------------------------------------------------------------------------
	TEST_METHOD(SplitMethods)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovFStringMethod_Split", ASTEST_AS(R"AS(
		int TestSplitCount()
		{
			FString s = "apple,banana,cherry";
			TArray<FString> parts;
			s.ParseIntoArray(parts, ",");
			return parts.Num();
		}

		FString TestSplitFirst()
		{
			FString s = "apple,banana,cherry";
			TArray<FString> parts;
			s.ParseIntoArray(parts, ",");
			return parts[0];
		}

		FString TestSplitLast()
		{
			FString s = "apple,banana,cherry";
			TArray<FString> parts;
			s.ParseIntoArray(parts, ",");
			return parts[2];
		}
		)AS"));
		ON_SCOPE_EXIT
		{
			if (Module != nullptr)
			{
				Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
			}
		};

		ExpectGlobalReturn<int32>(Engine, Module, TEXT("int TestSplitCount()"), 3, TEXT("ParseIntoArray() count"));
		ExpectGlobalReturn<FString>(Engine, Module, TEXT("FString TestSplitFirst()"), FString(TEXT("apple")), TEXT("ParseIntoArray() first element"));
		ExpectGlobalReturn<FString>(Engine, Module, TEXT("FString TestSplitLast()"), FString(TEXT("cherry")), TEXT("ParseIntoArray() last element"));
	}

	// -------------------------------------------------------------------------
	// Format methods: Printf(), Format().
	// -------------------------------------------------------------------------
	TEST_METHOD(FormatMethods)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovFStringMethod_Format", ASTEST_AS(R"AS(
		FString TestPrintfInt()
		{
			return FString::Printf("{0}", 42);
		}

		FString TestPrintfFloat()
		{
			return FString::Printf("{0}", 3.14f);
		}

		FString TestPrintfString()
		{
			return FString::Printf("Hello {0}", "World");
		}

		FString TestPrintfMultiple()
		{
			return FString::Printf("{0} + {1} = {2}", 2, 3, 5);
		}
		)AS"));
		ON_SCOPE_EXIT
		{
			if (Module != nullptr)
			{
				Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
			}
		};

		ExpectGlobalReturn<FString>(Engine, Module, TEXT("FString TestPrintfInt()"), FString(TEXT("42")), TEXT("Printf() with int"));
		ExpectGlobalReturn<FString>(Engine, Module, TEXT("FString TestPrintfFloat()"), FString(TEXT("3.14")), TEXT("Printf() with float"));
		ExpectGlobalReturn<FString>(Engine, Module, TEXT("FString TestPrintfString()"), FString(TEXT("Hello World")), TEXT("Printf() with string"));
		ExpectGlobalReturn<FString>(Engine, Module, TEXT("FString TestPrintfMultiple()"), FString(TEXT("2 + 3 = 5")), TEXT("Printf() with multiple args"));
	}

	// -------------------------------------------------------------------------
	// Conversion methods: ToInt(), ToFloat() (via FCString).
	// -------------------------------------------------------------------------
	TEST_METHOD(ConversionMethods)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovFStringMethod_Conversion", ASTEST_AS(R"AS(
		int TestToInt()
		{
			FString s = "123";
			return FCString::Atoi(s);
		}

		int TestToIntNegative()
		{
			FString s = "-456";
			return FCString::Atoi(s);
		}

		float TestToFloat()
		{
			FString s = "3.14";
			return FCString::Atof(s);
		}

		int TestFromInt()
		{
			FString s = FString::FromInt(999);
			return FCString::Atoi(s);
		}
		)AS"));
		ON_SCOPE_EXIT
		{
			if (Module != nullptr)
			{
				Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
			}
		};

		ExpectGlobalReturn<int32>(Engine, Module, TEXT("int TestToInt()"), 123, TEXT("FCString::Atoi()"));
		ExpectGlobalReturn<int32>(Engine, Module, TEXT("int TestToIntNegative()"), -456, TEXT("FCString::Atoi() negative"));

		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("float TestToFloat()"));
			float Result = Invoker.ExecuteAndGet<float>(0.0f);
			TestRunner->TestTrue(TEXT("FCString::Atof()"), FMath::IsNearlyEqual(Result, 3.14f, 0.01f));
		}

		ExpectGlobalReturn<int32>(Engine, Module, TEXT("int TestFromInt()"), 999, TEXT("FString::FromInt() round-trip"));
	}

	// -------------------------------------------------------------------------
	// Reverse method.
	// -------------------------------------------------------------------------
	TEST_METHOD(ReverseMethods)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovFStringMethod_Reverse", ASTEST_AS(R"AS(
		FString TestReverse()
		{
			FString s = "Hello";
			return s.Reverse();
		}

		FString TestReversePalindrome()
		{
			FString s = "racecar";
			return s.Reverse();
		}
		)AS"));
		ON_SCOPE_EXIT
		{
			if (Module != nullptr)
			{
				Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
			}
		};

		ExpectGlobalReturn<FString>(Engine, Module, TEXT("FString TestReverse()"), FString(TEXT("olleH")), TEXT("Reverse()"));
		ExpectGlobalReturn<FString>(Engine, Module, TEXT("FString TestReversePalindrome()"), FString(TEXT("racecar")), TEXT("Reverse() palindrome"));
	}

	// -------------------------------------------------------------------------
	// IsNumeric predicate.
	// -------------------------------------------------------------------------
	TEST_METHOD(IsNumericMethod)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovFStringMethod_IsNumeric", ASTEST_AS(R"AS(
		bool TestIsNumeric_True()
		{
			FString s = "12345";
			return s.IsNumeric();
		}

		bool TestIsNumeric_False()
		{
			FString s = "Hello123";
			return s.IsNumeric();
		}

		bool TestIsNumeric_Negative()
		{
			FString s = "-456";
			return s.IsNumeric();
		}

		bool TestIsNumeric_Float()
		{
			FString s = "3.14";
			return s.IsNumeric();
		}
		)AS"));
		ON_SCOPE_EXIT
		{
			if (Module != nullptr)
			{
				Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
			}
		};

		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool TestIsNumeric_True()"), true, TEXT("IsNumeric() on digits"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool TestIsNumeric_False()"), false, TEXT("IsNumeric() on mixed"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool TestIsNumeric_Negative()"), true, TEXT("IsNumeric() on negative"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool TestIsNumeric_Float()"), true, TEXT("IsNumeric() on float"));
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
