#include "CQTest.h"
#include "AngelscriptFunctionalTestUtils.h"
#include "AngelscriptGlobalFunctionInvoker.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestModuleBuilder.h"
#include "AngelscriptTestUtilities.h"

#include "Misc/ScopeExit.h"

#include <type_traits>

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
//   - Format: Format()
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
		ASSERT_THAT(IsNotNull(Module, TEXT("FString method module should compile before executing global function")));
		if (Module == nullptr)
		{
			return;
		}

		FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, Declaration);
		ASSERT_THAT(IsTrue(Invoker.IsValid(), TEXT("FString method global function should resolve and prepare")));
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

		bool TestToBoolTrue()
		{
			FString s = "true";
			return s.ToBool();
		}

		bool TestToBoolFalse()
		{
			FString s = "false";
			return s.ToBool();
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
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool TestToBoolTrue()"), true, TEXT("FString.ToBool() on true literal"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool TestToBoolFalse()"), false, TEXT("FString.ToBool() on false literal"));
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

		int TestFindCaseSensitiveMiss()
		{
			FString s = "Hello World";
			return s.Find("world", ESearchCase::CaseSensitive);
		}

		int TestFindFromEnd()
		{
			FString s = "One Two One";
			return s.Find("One", ESearchCase::CaseSensitive, ESearchDir::FromEnd);
		}

		bool TestFindChar()
		{
			FString s = "Hello";
			int Index = -1;
			return s.FindChar(0x65, Index) && Index == 1;
		}

		bool TestFindLastChar()
		{
			FString s = "banana";
			int Index = -1;
			return s.FindLastChar(0x61, Index) && Index == 5;
		}

		bool TestMatchesWildcard()
		{
			FString s = "CoverageString";
			return s.MatchesWildcard("Coverage*");
		}

		bool TestEqualsIgnoreCase()
		{
			FString s = "Hello";
			return s.Equals("hello", ESearchCase::IgnoreCase);
		}

		bool TestEqualsCaseSensitiveMiss()
		{
			FString s = "Hello";
			return !s.Equals("hello", ESearchCase::CaseSensitive);
		}

		bool TestCompareOrdersValues()
		{
			FString a = "Alpha";
			FString b = "Beta";
			return a.Compare(b) < 0 && b.Compare(a) > 0;
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
		ExpectGlobalReturn<int32>(Engine, Module, TEXT("int TestFindCaseSensitiveMiss()"), -1, TEXT("Find() honors case-sensitive misses"));
		ExpectGlobalReturn<int32>(Engine, Module, TEXT("int TestFindFromEnd()"), 8, TEXT("Find() from end returns the last matching index"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool TestFindChar()"), true, TEXT("FindChar() should expose found character index"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool TestFindLastChar()"), true, TEXT("FindLastChar() should expose last character index"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool TestMatchesWildcard()"), true, TEXT("MatchesWildcard() should match wildcard patterns"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool TestEqualsIgnoreCase()"), true, TEXT("Equals() should support ignore-case comparison"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool TestEqualsCaseSensitiveMiss()"), true, TEXT("Equals() should support case-sensitive comparison"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool TestCompareOrdersValues()"), true, TEXT("Compare() should expose ordering semantics"));
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

		FString TestTrimChar()
		{
			FString s = "***Hello***";
			return s.TrimChar(0x2A);
		}

		FString TestTrimQuotes()
		{
			FString s = "\"Quoted\"";
			bool bQuotesRemoved = false;
			FString Result = s.TrimQuotes(bQuotesRemoved);
			return bQuotesRemoved ? Result : "failed";
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
		ExpectGlobalReturn<FString>(Engine, Module, TEXT("FString TestTrimChar()"), FString(TEXT("Hello")), TEXT("TrimChar() should trim matching leading and trailing characters"));
		ExpectGlobalReturn<FString>(Engine, Module, TEXT("FString TestTrimQuotes()"), FString(TEXT("Quoted")), TEXT("TrimQuotes() should trim quotes and report removal"));
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

		FString TestLeftChop()
		{
			FString s = "Hello World";
			return s.LeftChop(6);
		}

		FString TestRightChop()
		{
			FString s = "Hello World";
			return s.RightChop(6);
		}

		FString TestRemoveFromStart()
		{
			FString s = "PrefixValue";
			bool bRemoved = s.RemoveFromStart("Prefix", ESearchCase::CaseSensitive);
			return bRemoved ? s : "failed";
		}

		FString TestRemoveFromEnd()
		{
			FString s = "ValueSuffix";
			bool bRemoved = s.RemoveFromEnd("Suffix", ESearchCase::CaseSensitive);
			return bRemoved ? s : "failed";
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
		ExpectGlobalReturn<FString>(Engine, Module, TEXT("FString TestLeftChop()"), FString(TEXT("Hello")), TEXT("LeftChop()"));
		ExpectGlobalReturn<FString>(Engine, Module, TEXT("FString TestRightChop()"), FString(TEXT("World")), TEXT("RightChop()"));
		ExpectGlobalReturn<FString>(Engine, Module, TEXT("FString TestRemoveFromStart()"), FString(TEXT("Value")), TEXT("RemoveFromStart() should mutate by prefix"));
		ExpectGlobalReturn<FString>(Engine, Module, TEXT("FString TestRemoveFromEnd()"), FString(TEXT("Value")), TEXT("RemoveFromEnd() should mutate by suffix"));
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

		FString TestReplaceInline()
		{
			FString s = "red red blue";
			int Count = s.ReplaceInline("red", "green");
			return FString::Format("{0}:{1}", Count, s);
		}

		FString TestReplaceCaseSensitiveMiss()
		{
			FString s = "Hello hello";
			return s.Replace("hello", "World", ESearchCase::CaseSensitive);
		}

		FString TestEscapedCharacters()
		{
			FString s = "Line\nTab\t";
			return s.ReplaceCharWithEscapedChar();
		}

		FString TestUnescapedCharacters()
		{
			FString s = "Line\\nTab\\t";
			return s.ReplaceEscapedCharWithChar();
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
		ExpectGlobalReturn<FString>(Engine, Module, TEXT("FString TestReplaceInline()"), FString(TEXT("2:green green blue")), TEXT("ReplaceInline() should mutate and report replacement count"));
		ExpectGlobalReturn<FString>(Engine, Module, TEXT("FString TestReplaceCaseSensitiveMiss()"), FString(TEXT("Hello World")), TEXT("Replace() should honor case-sensitive search"));
		ExpectGlobalReturn<FString>(Engine, Module, TEXT("FString TestEscapedCharacters()"), FString(TEXT("Line\\nTab\\t")), TEXT("ReplaceCharWithEscapedChar()"));
		ExpectGlobalReturn<FString>(Engine, Module, TEXT("FString TestUnescapedCharacters()"), FString(TEXT("Line\nTab\t")), TEXT("ReplaceEscapedCharWithChar()"));
	}

	// -------------------------------------------------------------------------
	// Mutable methods: Append(), AppendChar(), AppendInt(), InsertAt(),
	// RemoveAt(), RemoveSpacesInline(), Empty(), Reset(), Reserve(), Shrink().
	// -------------------------------------------------------------------------
	TEST_METHOD(MutableStringMethods)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovFStringMethod_Mutable", ASTEST_AS(R"AS(
		FString TestAppendAndAppendInt()
		{
			FString s = "Score";
			s.Append(": ");
			s.AppendInt(42);
			return s;
		}

		FString TestAppendCharAndInsertAt()
		{
			FString s = "AC";
			s.InsertAt(1, 0x42);
			s.AppendChar(0x44);
			s.InsertAt(0, "Start-");
			return s;
		}

		FString TestRemoveAt()
		{
			FString s = "ABCDEF";
			s.RemoveAt(2, 2);
			return s;
		}

		FString TestRemoveSpacesInline()
		{
			FString s = "A B  C";
			s.RemoveSpacesInline();
			return s;
		}

		int TestEmptyResetReserveShrink()
		{
			FString s = "abcdef";
			s.Reserve(64);
			s.Empty();
			int AfterEmpty = s.Len();

			s.Append("xy");
			s.Reset(32);
			int AfterReset = s.Len();

			s.Append("z");
			s.Shrink();
			return AfterEmpty * 100 + AfterReset * 10 + s.Len();
		}

		bool TestIndexMutationAndValidation()
		{
			FString s = "ABC";
			if (!s.IsValidIndex(2) || s.IsValidIndex(3))
			{
				return false;
			}

			s[1] = 0x5A;
			return s == "AZC";
		}
		)AS"));
		ON_SCOPE_EXIT
		{
			if (Module != nullptr)
			{
				Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
			}
		};
		ASSERT_THAT(IsNotNull(Module, TEXT("mutable FString methods module should compile")));
		if (Module == nullptr)
		{
			return;
		}

		ExpectGlobalReturn<FString>(Engine, Module, TEXT("FString TestAppendAndAppendInt()"), FString(TEXT("Score: 42")), TEXT("Append and AppendInt should mutate the string"));
		ExpectGlobalReturn<FString>(Engine, Module, TEXT("FString TestAppendCharAndInsertAt()"), FString(TEXT("Start-ABCD")), TEXT("AppendChar and InsertAt should mutate by character and string"));
		ExpectGlobalReturn<FString>(Engine, Module, TEXT("FString TestRemoveAt()"), FString(TEXT("ABEF")), TEXT("RemoveAt should delete the requested range"));
		ExpectGlobalReturn<FString>(Engine, Module, TEXT("FString TestRemoveSpacesInline()"), FString(TEXT("ABC")), TEXT("RemoveSpacesInline should remove spaces in place"));
		ExpectGlobalReturn<int32>(Engine, Module, TEXT("int TestEmptyResetReserveShrink()"), 1, TEXT("Empty/Reset/Reserve/Shrink should preserve usable string state"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool TestIndexMutationAndValidation()"), true, TEXT("operator[] should support valid index mutation"));
	}

	// -------------------------------------------------------------------------
	// Mutable method edge cases: InsertAt(), RemoveAt(), RemoveSpacesInline(),
	// ReplaceInline(), Empty(), Reset(), Reserve(), Shrink().
	// -------------------------------------------------------------------------
	TEST_METHOD(MutableStringEdgeCases)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovFStringMethod_MutableEdgeCases", ASTEST_AS(R"AS(
		FString TestInsertAtBoundaries()
		{
			FString s = "Center";
			s.InsertAt(0, "Start-");
			s.InsertAt(s.Len(), "-End");
			s.InsertAt(6, 0x7C);
			return s;
		}

		FString TestRemoveAtFirstAndLast()
		{
			FString s = "[payload]";
			s.RemoveAt(0, 1);
			s.RemoveAt(s.Len() - 1, 1);
			return s;
		}

		FString TestRemoveSpacesInlinePreservesWhitespaceKinds()
		{
			FString s = " A\tB C ";
			s.RemoveSpacesInline();
			return s;
		}

		FString TestReplaceInlineCaseSensitiveCount()
		{
			FString s = "Token token TOKEN";
			int Count = s.ReplaceInline("Token", "Hit", ESearchCase::CaseSensitive);
			return FString::Format("{0}:{1}", Count, s);
		}

		FString TestMemoryMethodsRemainUsable()
		{
			FString s = "carry";
			s.Reserve(128);
			s.Empty(16);
			s.Append("A");
			s.Reset(32);
			s.Append("B");
			s.Shrink();
			return FString::Format("{0}:{1}", s.Len(), s);
		}
		)AS"));
		ON_SCOPE_EXIT
		{
			if (Module != nullptr)
			{
				Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
			}
		};
		ASSERT_THAT(IsNotNull(Module, TEXT("mutable FString edge case module should compile")));
		if (Module == nullptr)
		{
			return;
		}

		ExpectGlobalReturn<FString>(Engine, Module, TEXT("FString TestInsertAtBoundaries()"), FString(TEXT("Start-|Center-End")), TEXT("InsertAt should support string and character insertion at boundaries"));
		ExpectGlobalReturn<FString>(Engine, Module, TEXT("FString TestRemoveAtFirstAndLast()"), FString(TEXT("payload")), TEXT("RemoveAt should remove boundary ranges"));
		ExpectGlobalReturn<FString>(Engine, Module, TEXT("FString TestRemoveSpacesInlinePreservesWhitespaceKinds()"), FString(TEXT("A\tBC")), TEXT("RemoveSpacesInline should remove literal spaces without stripping tabs"));
		ExpectGlobalReturn<FString>(Engine, Module, TEXT("FString TestReplaceInlineCaseSensitiveCount()"), FString(TEXT("1:Hit token TOKEN")), TEXT("ReplaceInline should report case-sensitive replacement count"));
		ExpectGlobalReturn<FString>(Engine, Module, TEXT("FString TestMemoryMethodsRemainUsable()"), FString(TEXT("1:B")), TEXT("Empty/Reset/Reserve/Shrink should leave the string reusable"));
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

		FString TestSplitLeftRight()
		{
			FString s = "left:right";
			FString left;
			FString right;
			bool bSplit = s.Split(":", left, right);
			return bSplit ? left + "|" + right : "failed";
		}

		FString TestParseIntoArrayKeepsEmpty()
		{
			FString s = "a,,b";
			TArray<FString> parts;
			int Count = s.ParseIntoArray(parts, ",", false);
			return FString::Format("{0}:{1}", Count, parts[1]);
		}

		int TestParseIntoArrayLines()
		{
			FString s = "Line1\nLine2\n";
			TArray<FString> parts;
			return s.ParseIntoArrayLines(parts);
		}

		FString TestParseIntoArrayWS()
		{
			FString s = "Alpha Beta\tGamma";
			TArray<FString> parts;
			int Count = s.ParseIntoArrayWS(parts);
			return FString::Format("{0}:{1}", Count, parts[2]);
		}

		FString TestJoin()
		{
			TArray<FString> parts;
			parts.Add("A");
			parts.Add("B");
			parts.Add("C");
			return FString::Join(parts, "|");
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
		ExpectGlobalReturn<FString>(Engine, Module, TEXT("FString TestSplitLeftRight()"), FString(TEXT("left|right")), TEXT("Split() should fill left and right out parameters"));
		ExpectGlobalReturn<FString>(Engine, Module, TEXT("FString TestParseIntoArrayKeepsEmpty()"), FString(TEXT("3:")), TEXT("ParseIntoArray() should optionally preserve empty entries"));
		ExpectGlobalReturn<int32>(Engine, Module, TEXT("int TestParseIntoArrayLines()"), 2, TEXT("ParseIntoArrayLines() should split newline-delimited text"));
		ExpectGlobalReturn<FString>(Engine, Module, TEXT("FString TestParseIntoArrayWS()"), FString(TEXT("3:Gamma")), TEXT("ParseIntoArrayWS() should split whitespace-delimited text"));
		ExpectGlobalReturn<FString>(Engine, Module, TEXT("FString TestJoin()"), FString(TEXT("A|B|C")), TEXT("FString::Join() should join string arrays"));
	}

	// -------------------------------------------------------------------------
	// ParseIntoArray overloads and delimiter edge cases.
	// -------------------------------------------------------------------------
	TEST_METHOD(ParseIntoArrayDelimiterVariants)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovFStringMethod_ParseIntoArrayDelimiterVariants", ASTEST_AS(R"AS(
		FString TestParseIntoArrayWithDelimiterArray()
		{
			FString s = "alpha,beta;gamma|delta";
			TArray<FString> delimiters;
			delimiters.Add(",");
			delimiters.Add(";");
			delimiters.Add("|");

			TArray<FString> parts;
			int Count = s.ParseIntoArray(parts, delimiters);
			return FString::Format("{0}:{1}:{2}", Count, parts[1], parts[3]);
		}

		FString TestParseIntoArrayKeepsBoundaryEmptyValues()
		{
			FString s = "|middle|";
			TArray<FString> parts;
			int Count = s.ParseIntoArray(parts, "|", false);
			return FString::Format("{0}:{1}", Count, parts[1]);
		}
		)AS"));
		ON_SCOPE_EXIT
		{
			if (Module != nullptr)
			{
				Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
			}
		};
		ASSERT_THAT(IsNotNull(Module, TEXT("ParseIntoArray delimiter variants module should compile")));
		if (Module == nullptr)
		{
			return;
		}

		ExpectGlobalReturn<FString>(Engine, Module, TEXT("FString TestParseIntoArrayWithDelimiterArray()"), FString(TEXT("4:beta:delta")), TEXT("ParseIntoArray should accept an array of delimiters"));
		ExpectGlobalReturn<FString>(Engine, Module, TEXT("FString TestParseIntoArrayKeepsBoundaryEmptyValues()"), FString(TEXT("3:middle")), TEXT("ParseIntoArray should keep leading and trailing empty values when requested"));
	}

	// -------------------------------------------------------------------------
	// Format methods: Format().
	// -------------------------------------------------------------------------
	TEST_METHOD(FormatMethods)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovFStringMethod_Format", ASTEST_AS(R"AS(
		FString TestFormatInt()
		{
			return FString::Format("{0}", 42);
		}

		FString TestFormatFloat()
		{
			return FString::Format("{0}", 3.14f);
		}

		FString TestFormatString()
		{
			return FString::Format("Hello {0}", "World");
		}

		FString TestFormatMultiple()
		{
			return FString::Format("{0} + {1} = {2}", 2, 3, 5);
		}

		FString TestFromInt()
		{
			return FString::FromInt(-17);
		}

		FString TestChrAndChrN()
		{
			return FString::Chr(0x41) + FString::ChrN(3, 0x42);
		}

		FString TestLeftPadAndRightPad()
		{
			FString left = "7";
			FString right = "7";
			return left.LeftPad(3) + "|" + right.RightPad(3);
		}

		FString TestConvertTabsToSpaces()
		{
			FString value = "A\tB";
			return value.ConvertTabsToSpaces(2);
		}
		)AS"));
		ON_SCOPE_EXIT
		{
			if (Module != nullptr)
			{
				Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
			}
		};

		ExpectGlobalReturn<FString>(Engine, Module, TEXT("FString TestFormatInt()"), FString(TEXT("42")), TEXT("Format() with int"));
		ExpectGlobalReturn<FString>(Engine, Module, TEXT("FString TestFormatFloat()"), FString(TEXT("3.14")), TEXT("Format() with float"));
		ExpectGlobalReturn<FString>(Engine, Module, TEXT("FString TestFormatString()"), FString(TEXT("Hello World")), TEXT("Format() with string"));
		ExpectGlobalReturn<FString>(Engine, Module, TEXT("FString TestFormatMultiple()"), FString(TEXT("2 + 3 = 5")), TEXT("Format() with multiple args"));
		ExpectGlobalReturn<FString>(Engine, Module, TEXT("FString TestFromInt()"), FString(TEXT("-17")), TEXT("FString::FromInt() should format signed integers"));
		ExpectGlobalReturn<FString>(Engine, Module, TEXT("FString TestChrAndChrN()"), FString(TEXT("ABBB")), TEXT("FString::Chr()/ChrN() should construct character strings"));
		ExpectGlobalReturn<FString>(Engine, Module, TEXT("FString TestLeftPadAndRightPad()"), FString(TEXT("  7|7  ")), TEXT("LeftPad()/RightPad() should pad strings to target length"));
		ExpectGlobalReturn<FString>(Engine, Module, TEXT("FString TestConvertTabsToSpaces()"), FString(TEXT("A  B")), TEXT("ConvertTabsToSpaces() should replace tabs with requested spaces"));
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
		ASSERT_THAT(IsNotNull(Module, TEXT("FString conversion module should compile")));
		if (Module == nullptr)
		{
			return;
		}

		ExpectGlobalReturn<int32>(Engine, Module, TEXT("int TestToInt()"), 123, TEXT("FCString::Atoi()"));
		ExpectGlobalReturn<int32>(Engine, Module, TEXT("int TestToIntNegative()"), -456, TEXT("FCString::Atoi() negative"));

		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("float TestToFloat()"));
			ASSERT_THAT(IsTrue(Invoker.IsValid(), TEXT("TestToFloat should resolve and prepare")));
			if (!Invoker.IsValid())
			{
				return;
			}
			float Result = Invoker.ExecuteAndGet<float>(0.0f);
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Result, 3.14f, 0.01f), TEXT("FCString::Atof()")));
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
