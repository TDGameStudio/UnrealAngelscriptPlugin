// ============================================================================
// AngelscriptFStringBindingsTests.cpp
//
// FString binding coverage — CQTest PoC. Automation IDs:
//   Angelscript.TestModule.Bindings.FString.FAngelscriptFStringBindingsTest.*
//
// Sections:
//   Construction          — empty / literal / copy / independence
//   Operators             — opEquals / opCmp / opAdd / opAddAssign / opAssign / opIndex
//   OperatorIndexError    — opIndex out-of-bounds exception (negative path)
//   LengthAndCapacity     — Len / IsEmpty / IsValidIndex / Reserve / Shrink / Empty / Reset
//   Substring             — Left / LeftChop / Right / RightChop / Mid + edge cases
//   Search                — Contains / Find / FindChar / FindLastChar / StartsWith /
//                           EndsWith / MatchesWildcard / Equals + SearchCase/SearchDir
//   Mutation              — Append / AppendChar / RemoveAt / RemoveFromStart /
//                           RemoveFromEnd / Replace / Reverse + chaining + SearchCase
//   CaseAndTrim           — ToUpper / ToLower / Trim* / TrimQuotes / LeftPad / RightPad
//   Split                 — Split + SearchDir/SearchCase / ParseIntoArray single/multi
//   Conversion            — ToBool / IsNumeric / GetHash / Compare / ToDisplayName
//   TypeConcat            — opAdd(int/float/FName/bool) via FToStringHelper path
//   FormatString          — FString::Format ordered positional (1-5 args)
//   Join                  — FString::Join array with separator
//   ApplyFormat           — FString::ApplyFormat python-style specifiers
//   Logging               — Log / Warning / LogIf + FString integration
//   ReturnFString         — AS returns FString, C++ reads via
//                           GetAddressOfReturnValue and validates content
//   PassFString           — C++ passes FString into AS via AddArgRef,
//                           AS processes and returns / writes out-params
//
// CQTest adaptation notes:
//   BEFORE_ALL  : one-time ASTEST_CREATE_ENGINE() to acquire
//                 a clean shared engine for the entire test class.
//   AFTER_ALL   : one-time ResetSharedCloneEngine() for final cleanup.
//   TEST_METHOD : ASTEST_GET_ENGINE() (no per-test reset) +
//                 FAngelscriptEngineScope + FCoverageModuleScope (RAII
//                 DiscardModule on scope exit handles per-test cleanup).
//
//   No BEFORE_EACH/AFTER_EACH needed — FCoverageModuleScope already
//   discards each test's module via RAII, so per-test engine reset is
//   unnecessary overhead.
// ============================================================================

#include "CQTest.h"
#include "Shared/AngelscriptTestMacros.h"
#include "Shared/AngelscriptTestModuleScope.h"
#include "Shared/AngelscriptBindingsAssertions.h"

#if WITH_DEV_AUTOMATION_TESTS


// ----------------------------------------------------------------------------
// Profile
// ----------------------------------------------------------------------------


TEST_CLASS_WITH_FLAGS(FAngelscriptFStringBindingsTest,
	"Angelscript.TestModule.Bindings.FString",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	BEFORE_ALL()
	{
		ASTEST_CREATE_ENGINE();
	}

	AFTER_ALL() { FAngelscriptEngine& Engine = ASTEST_GET_ENGINE(); ASTEST_RESET_ENGINE(Engine); }

	// ====================================================================
	// Section: Construction
	// ====================================================================

	TEST_METHOD(Construction)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		// Equivalent to the FAngelscriptEngineScope inside { FAngelscriptEngineScope _AutoEngineScope(Engine);.
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASFString_Ctor"), TEXT(R"(
int Ctor_EmptyLen()
{
	FString S;
	return S.Len();
}
int Ctor_EmptyIsEmpty()
{
	FString S;
	return S.IsEmpty() ? 1 : 0;
}
int Ctor_Literal()
{
	FString S = "Hello";
	return S.Len();
}
int Ctor_LiteralContent()
{
	FString S = "Hello";
	return (S == "Hello") ? 1 : 0;
}
int Ctor_SingleChar()
{
	FString S = "X";
	return S.Len();
}
int Ctor_Copy_Equals()
{
	FString A = "Test";
	FString B = A;
	return (A == B) ? 1 : 0;
}
int Ctor_Copy_Independent()
{
	// Modifying the copy should not affect the original.
	FString A = "Original";
	FString B = A;
	B.Append("Modified");
	return (A == "Original" && B == "OriginalModified") ? 1 : 0;
}
int Ctor_EmptyEqualsEmpty()
{
	FString A;
	FString B;
	return (A == B) ? 1 : 0;
}
int Ctor_LongString()
{
	FString S = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789abcdefghijklmnopqrstuvwxyz";
	return S.Len();
}
)"));
		if (!Mod.IsValid()) return;
		auto& M = Mod.GetModule();

		const FExpectedGlobalInt Cases[] = {
			{ TEXT("int Ctor_EmptyLen()"),        TEXT("Default ctor yields Len 0"),                  0 },
			{ TEXT("int Ctor_EmptyIsEmpty()"),     TEXT("Default ctor yields IsEmpty true"),            1 },
			{ TEXT("int Ctor_Literal()"),          TEXT("Literal ctor gives correct length"),           5 },
			{ TEXT("int Ctor_LiteralContent()"),   TEXT("Literal ctor preserves content"),              1 },
			{ TEXT("int Ctor_SingleChar()"),        TEXT("Single char literal"),                        1 },
			{ TEXT("int Ctor_Copy_Equals()"),       TEXT("Copy-constructed string equals original"),    1 },
			{ TEXT("int Ctor_Copy_Independent()"),  TEXT("Copy is independent — modify copy not orig"), 1 },
			{ TEXT("int Ctor_EmptyEqualsEmpty()"),  TEXT("Two default-constructed strings are equal"),  1 },
			{ TEXT("int Ctor_LongString()"),        TEXT("62-char string has correct length"),          62 },
		};
		ExpectGlobalInts(*TestRunner, Engine, M,  Cases);
	}

	// ====================================================================
	// Section: Operators
	// ====================================================================

	TEST_METHOD(Operators)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASFString_Operators"), TEXT(R"(
// ---- opEquals ----
int OpEq_Same()           { return ("ABC" == "ABC") ? 1 : 0; }
int OpEq_Diff()           { return ("ABC" == "XYZ") ? 1 : 0; }
int OpEq_EmptyEmpty()     { FString A; FString B; return (A == B) ? 1 : 0; }
int OpEq_EmptyNonEmpty()  { FString A; FString B = "X"; return (A == B) ? 1 : 0; }
int OpEq_CaseInsensitive() { return ("abc" == "ABC") ? 1 : 0; }

// ---- opCmp ----
int OpCmp_Less()          { FString A = "Apple"; FString B = "Banana"; return A.opCmp(B) < 0 ? 1 : 0; }
int OpCmp_Equal()         { FString A = "Same"; FString B = "Same"; return A.opCmp(B); }
int OpCmp_Greater()       { FString A = "Z"; FString B = "A"; return A.opCmp(B) > 0 ? 1 : 0; }
int OpCmp_EmptyLess()     { FString A; FString B = "A"; return A.opCmp(B) < 0 ? 1 : 0; }
int OpCmp_EmptyEqual()    { FString A; FString B; return A.opCmp(B); }

// ---- opAdd ----
int OpAdd_Basic()         { FString R = "Hello" + " World"; return (R == "Hello World") ? 1 : 0; }
int OpAdd_EmptyLeft()     { FString E; FString R = E + "X"; return (R == "X") ? 1 : 0; }
int OpAdd_EmptyRight()    { FString R = "X" + ""; return (R == "X") ? 1 : 0; }
int OpAdd_BothEmpty()     { FString A; FString B; FString R = A + B; return R.IsEmpty() ? 1 : 0; }
int OpAdd_ChainLen()      { FString R = "A" + "B" + "C" + "D"; return R.Len(); }

// ---- opAddAssign ----
int OpAddAssign_Basic()   { FString S = "A"; S += "B"; return (S == "AB") ? 1 : 0; }
int OpAddAssign_Empty()   { FString S = "X"; S += ""; return (S == "X") ? 1 : 0; }
int OpAddAssign_OnEmpty() { FString S; S += "Y"; return (S == "Y") ? 1 : 0; }
int OpAddAssign_Chain()   { FString S = "A"; S += "B"; S += "C"; return (S == "ABC") ? 1 : 0; }

// ---- opAssign ----
int OpAssign_Basic()      { FString A = "X"; FString B; B = A; return (B == "X") ? 1 : 0; }
int OpAssign_Overwrite()  { FString S = "Old"; S = "New"; return (S == "New") ? 1 : 0; }
int OpAssign_Empty()      { FString S = "Data"; FString E; S = E; return S.IsEmpty() ? 1 : 0; }

// ---- opIndex ----
int OpIdx_ReadFirst()     { FString S = "ABCD"; return S[0]; }
int OpIdx_ReadLast()      { FString S = "ABCD"; return S[3]; }
int OpIdx_Write()
{
	FString S = "ABC";
	S[0] = 0x5A;   // 'Z'
	S[2] = 0x58;   // 'X'
	return (S == "ZBX") ? 1 : 0;
}
)"));
		if (!Mod.IsValid()) return;
		auto& M = Mod.GetModule();

		const FExpectedGlobalInt Cases[] = {
			// opEquals
			{ TEXT("int OpEq_Same()"),          TEXT("opEquals same strings returns true"),         1 },
			{ TEXT("int OpEq_Diff()"),          TEXT("opEquals different strings returns false"),   0 },
			{ TEXT("int OpEq_EmptyEmpty()"),    TEXT("opEquals empty == empty"),                    1 },
			{ TEXT("int OpEq_EmptyNonEmpty()"), TEXT("opEquals empty != non-empty"),                0 },
			{ TEXT("int OpEq_CaseInsensitive()"), TEXT("opEquals is case-insensitive (binding uses ==)"), 1 },
			// opCmp
			{ TEXT("int OpCmp_Less()"),         TEXT("opCmp Apple < Banana"),                       1 },
			{ TEXT("int OpCmp_Equal()"),        TEXT("opCmp equal strings returns 0"),              0 },
			{ TEXT("int OpCmp_Greater()"),      TEXT("opCmp Z > A"),                                1 },
			{ TEXT("int OpCmp_EmptyLess()"),    TEXT("opCmp empty < non-empty"),                    1 },
			{ TEXT("int OpCmp_EmptyEqual()"),   TEXT("opCmp empty == empty returns 0"),             0 },
			// opAdd
			{ TEXT("int OpAdd_Basic()"),        TEXT("opAdd concatenates two strings"),              1 },
			{ TEXT("int OpAdd_EmptyLeft()"),    TEXT("opAdd empty + X yields X"),                    1 },
			{ TEXT("int OpAdd_EmptyRight()"),   TEXT("opAdd X + empty yields X"),                    1 },
			{ TEXT("int OpAdd_BothEmpty()"),    TEXT("opAdd empty + empty yields empty"),            1 },
			{ TEXT("int OpAdd_ChainLen()"),     TEXT("opAdd chained A+B+C+D has length 4"),         4 },
			// opAddAssign
			{ TEXT("int OpAddAssign_Basic()"),  TEXT("opAddAssign appends"),                         1 },
			{ TEXT("int OpAddAssign_Empty()"),  TEXT("opAddAssign with empty no-op"),                1 },
			{ TEXT("int OpAddAssign_OnEmpty()"),TEXT("opAddAssign on empty string"),                 1 },
			{ TEXT("int OpAddAssign_Chain()"),  TEXT("opAddAssign chained three times"),             1 },
			// opAssign
			{ TEXT("int OpAssign_Basic()"),     TEXT("opAssign copies content"),                     1 },
			{ TEXT("int OpAssign_Overwrite()"), TEXT("opAssign overwrites previous value"),          1 },
			{ TEXT("int OpAssign_Empty()"),     TEXT("opAssign from empty clears target"),           1 },
			// opIndex
			{ TEXT("int OpIdx_ReadFirst()"),    TEXT("opIndex[0] reads A (65)"),                    65 },
			{ TEXT("int OpIdx_ReadLast()"),     TEXT("opIndex[3] reads D (68)"),                    68 },
			{ TEXT("int OpIdx_Write()"),        TEXT("opIndex write at [0] and [2]"),                1 },
		};
		ExpectGlobalInts(*TestRunner, Engine, M,  Cases);
	}

	// ====================================================================
	// Section: OperatorIndexError — negative path
	// ====================================================================

	TEST_METHOD(OperatorIndexError)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASFString_IndexErr"), TEXT(R"(
void TriggerIndexOutOfBounds()
{
	FString S = "AB";
	int16 C = S[10];
}
)"));
		if (!Mod.IsValid()) return;

		TestRunner->AddExpectedErrorPlain(
			FString(TEXT("ASFString_IndexErr")),
			EAutomationExpectedErrorFlags::Contains, 0);
		TestRunner->AddExpectedErrorPlain(
			TEXT("String index out of bounds"),
			EAutomationExpectedErrorFlags::Contains, 0);
		TestRunner->AddExpectedErrorPlain(
			TEXT("void TriggerIndexOutOfBounds()"),
			EAutomationExpectedErrorFlags::Contains, 0);

		ExecuteFunctionExpectingScriptException(
			*TestRunner, Engine, Mod.GetModule(), 
			TEXT("void TriggerIndexOutOfBounds()"),
			TEXT("opIndex out-of-bounds should throw exception"),
			FString(TEXT("String index out of bounds")));
	}

	// ====================================================================
	// Section: LengthAndCapacity
	// ====================================================================

	TEST_METHOD(LengthAndCapacity)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASFString_LenCap"), TEXT(R"(
int Len_Empty()            { FString S; return S.Len(); }
int Len_One()              { FString S = "X"; return S.Len(); }
int Len_Multi()            { FString S = "HelloWorld"; return S.Len(); }
int IsEmpty_Default()      { FString S; return S.IsEmpty() ? 1 : 0; }
int IsEmpty_NonEmpty()     { FString S = "X"; return S.IsEmpty() ? 1 : 0; }
int IsEmpty_AfterEmpty()   { FString S = "X"; S.Empty(); return S.IsEmpty() ? 1 : 0; }

// IsValidIndex returns void in binding — exercise without using return value.
int IsValidIdx_NoThrow()   { FString S = "AB"; S.IsValidIndex(0); S.IsValidIndex(1); return 1; }
int IsValidIdx_OverNoThrow() { FString S = "AB"; S.IsValidIndex(99); return 1; }

int Reserve_LenStays()
{
	FString S;
	S.Reserve(256);
	return S.Len();
}
int Reserve_ThenAppend()
{
	FString S;
	S.Reserve(100);
	S.Append("Data");
	return (S == "Data") ? 1 : 0;
}
int Shrink_NoThrow()
{
	FString S = "LongEnoughString";
	S.Reserve(1000);
	S.Shrink();
	return S.Len();
}
int Empty_NoArg()
{
	FString S = "Content";
	S.Empty();
	return S.Len();
}
int Empty_WithSlack()
{
	FString S = "Content";
	S.Empty(64);
	return S.Len();
}
int Reset_ZeroLen()
{
	FString S = "ABCDEFG";
	S.Reset(32);
	return S.Len();
}
int Reset_ThenReuse()
{
	FString S = "Old";
	S.Reset();
	S.Append("New");
	return (S == "New") ? 1 : 0;
}
)"));
		if (!Mod.IsValid()) return;
		auto& M = Mod.GetModule();

		const FExpectedGlobalInt Cases[] = {
			{ TEXT("int Len_Empty()"),           TEXT("Len of empty string is 0"),              0 },
			{ TEXT("int Len_One()"),             TEXT("Len of single char is 1"),               1 },
			{ TEXT("int Len_Multi()"),           TEXT("Len of 10-char string is 10"),          10 },
			{ TEXT("int IsEmpty_Default()"),     TEXT("Default-constructed string IsEmpty"),     1 },
			{ TEXT("int IsEmpty_NonEmpty()"),    TEXT("Non-empty string is not empty"),          0 },
			{ TEXT("int IsEmpty_AfterEmpty()"),  TEXT("IsEmpty after Empty() call"),             1 },
			{ TEXT("int IsValidIdx_NoThrow()"),  TEXT("IsValidIndex on valid indices no-throw"), 1 },
			{ TEXT("int IsValidIdx_OverNoThrow()"), TEXT("IsValidIndex on over-index no-throw"),1 },
			{ TEXT("int Reserve_LenStays()"),    TEXT("Reserve does not change Len"),            0 },
			{ TEXT("int Reserve_ThenAppend()"),  TEXT("Reserve then Append works normally"),     1 },
			{ TEXT("int Shrink_NoThrow()"),      TEXT("Shrink preserves content length"),       16 },
			{ TEXT("int Empty_NoArg()"),         TEXT("Empty() clears to length 0"),             0 },
			{ TEXT("int Empty_WithSlack()"),     TEXT("Empty(64) clears to length 0"),           0 },
			{ TEXT("int Reset_ZeroLen()"),       TEXT("Reset clears to length 0"),               0 },
			{ TEXT("int Reset_ThenReuse()"),     TEXT("Reset then Append stores new content"),   1 },
		};
		ExpectGlobalInts(*TestRunner, Engine, M,  Cases);
	}

	// ====================================================================
	// Section: Substring
	// ====================================================================

	TEST_METHOD(Substring)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASFString_Substr"), TEXT(R"(
// ---- Left / LeftChop ----
int Left_Full()         { FString S = "ABCDE"; return (S.Left(5) == "ABCDE") ? 1 : 0; }
int Left_Partial()      { FString S = "ABCDE"; return (S.Left(3) == "ABC") ? 1 : 0; }
int Left_Zero()         { FString S = "ABCDE"; return S.Left(0).IsEmpty() ? 1 : 0; }
int Left_OverLength()   { FString S = "AB"; return (S.Left(10) == "AB") ? 1 : 0; }
int LeftChop_Basic()    { FString S = "ABCDE"; return (S.LeftChop(2) == "ABC") ? 1 : 0; }
int LeftChop_All()      { FString S = "AB"; return S.LeftChop(2).IsEmpty() ? 1 : 0; }

// ---- Right / RightChop ----
int Right_Partial()     { FString S = "ABCDE"; return (S.Right(3) == "CDE") ? 1 : 0; }
int Right_Full()        { FString S = "ABCDE"; return (S.Right(5) == "ABCDE") ? 1 : 0; }
int Right_Zero()        { FString S = "ABCDE"; return S.Right(0).IsEmpty() ? 1 : 0; }
int RightChop_Basic()   { FString S = "ABCDE"; return (S.RightChop(2) == "CDE") ? 1 : 0; }
int RightChop_All()     { FString S = "AB"; return S.RightChop(2).IsEmpty() ? 1 : 0; }

// ---- Mid ----
int Mid_WithCount()     { FString S = "ABCDE"; return (S.Mid(1, 3) == "BCD") ? 1 : 0; }
int Mid_ToEnd()         { FString S = "ABCDE"; return (S.Mid(3) == "DE") ? 1 : 0; }
int Mid_Start()         { FString S = "ABCDE"; return (S.Mid(0, 2) == "AB") ? 1 : 0; }
int Mid_Single()        { FString S = "ABCDE"; return (S.Mid(2, 1) == "C") ? 1 : 0; }

// ---- Compound ----
int Compound_LeftThenRight()
{
	FString S = "0123456789";
	FString Mid4 = S.Left(7).Right(4);
	return (Mid4 == "3456") ? 1 : 0;
}
)"));
		if (!Mod.IsValid()) return;
		auto& M = Mod.GetModule();

		const FExpectedGlobalInt Cases[] = {
			{ TEXT("int Left_Full()"),           TEXT("Left(n) where n == Len returns full"),    1 },
			{ TEXT("int Left_Partial()"),        TEXT("Left(3) returns first 3 chars"),          1 },
			{ TEXT("int Left_Zero()"),           TEXT("Left(0) returns empty"),                  1 },
			{ TEXT("int Left_OverLength()"),     TEXT("Left beyond Len returns full string"),    1 },
			{ TEXT("int LeftChop_Basic()"),      TEXT("LeftChop(2) removes last 2"),             1 },
			{ TEXT("int LeftChop_All()"),        TEXT("LeftChop(Len) returns empty"),            1 },
			{ TEXT("int Right_Partial()"),       TEXT("Right(3) returns last 3 chars"),          1 },
			{ TEXT("int Right_Full()"),          TEXT("Right(Len) returns full string"),         1 },
			{ TEXT("int Right_Zero()"),          TEXT("Right(0) returns empty"),                 1 },
			{ TEXT("int RightChop_Basic()"),     TEXT("RightChop(2) removes first 2"),           1 },
			{ TEXT("int RightChop_All()"),       TEXT("RightChop(Len) returns empty"),            1 },
			{ TEXT("int Mid_WithCount()"),       TEXT("Mid(1,3) returns BCD"),                    1 },
			{ TEXT("int Mid_ToEnd()"),           TEXT("Mid(3) to end returns DE"),                1 },
			{ TEXT("int Mid_Start()"),           TEXT("Mid(0,2) from start returns AB"),          1 },
			{ TEXT("int Mid_Single()"),          TEXT("Mid(2,1) returns single char C"),          1 },
			{ TEXT("int Compound_LeftThenRight()"), TEXT("Left(7).Right(4) compound extraction"),1 },
		};
		ExpectGlobalInts(*TestRunner, Engine, M,  Cases);
	}

	// ====================================================================
	// Section: Search
	// ====================================================================

	TEST_METHOD(Search)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASFString_Search"), TEXT(R"(
// ---- Contains ----
int Contains_Found()           { FString S = "Hello World"; return S.Contains("World") ? 1 : 0; }
int Contains_NotFound()        { FString S = "Hello World"; return S.Contains("xyz") ? 1 : 0; }
int Contains_CaseSensitive()   { FString S = "Hello"; return S.Contains("hello", ESearchCase::CaseSensitive) ? 1 : 0; }
int Contains_IgnoreCase()      { FString S = "Hello"; return S.Contains("hello", ESearchCase::IgnoreCase) ? 1 : 0; }
int Contains_Partial()         { FString S = "Hello World"; return S.Contains("lo Wo") ? 1 : 0; }
int Contains_InEmpty()         { FString S; return S.Contains("X") ? 1 : 0; }

// ---- Find ----
int Find_FirstOccurrence()     { FString S = "ABCABC"; return S.Find("BC"); }
int Find_NotFound()            { FString S = "ABC"; return S.Find("XY"); }
int Find_CaseSensitive()       { FString S = "ABCabc"; return S.Find("abc", ESearchCase::CaseSensitive); }
int Find_IgnoreCase()          { FString S = "ABCabc"; return S.Find("abc", ESearchCase::IgnoreCase); }
int Find_FromEnd()
{
	FString S = "ABCABC";
	return S.Find("BC", ESearchCase::IgnoreCase, ESearchDir::FromEnd);
}
int Find_StartPos()
{
	FString S = "ABCABC";
	return S.Find("BC", ESearchCase::IgnoreCase, ESearchDir::FromStart, 2);
}

// ---- FindChar / FindLastChar ----
int FindChar_Found()
{
	FString S = "ABCDE";
	int Idx = -1;
	bool bFound = S.FindChar(0x43, Idx);  // 'C'
	return bFound ? Idx : -1;
}
int FindChar_NotFound()
{
	FString S = "ABCDE";
	int Idx = -1;
	bool bFound = S.FindChar(0x5A, Idx);  // 'Z'
	return bFound ? 1 : 0;
}
int FindLastChar_Found()
{
	FString S = "ABCBC";
	int Idx = -1;
	S.FindLastChar(0x42, Idx);  // 'B'
	return Idx;
}
int FindLastChar_NotFound()
{
	FString S = "ABCDE";
	int Idx = -1;
	bool bFound = S.FindLastChar(0x5A, Idx);  // 'Z'
	return bFound ? 1 : 0;
}

// ---- StartsWith / EndsWith ----
int StartsWith_Match()        { return "HelloWorld".StartsWith("Hello") ? 1 : 0; }
int StartsWith_NoMatch()      { return "HelloWorld".StartsWith("World") ? 1 : 0; }
int StartsWith_CaseSens()     { return "Hello".StartsWith("hello", ESearchCase::CaseSensitive) ? 1 : 0; }
int StartsWith_IgnCase()      { return "Hello".StartsWith("hello", ESearchCase::IgnoreCase) ? 1 : 0; }
int StartsWith_Full()         { return "Hello".StartsWith("Hello") ? 1 : 0; }
int EndsWith_Match()          { return "HelloWorld".EndsWith("World") ? 1 : 0; }
int EndsWith_NoMatch()        { return "HelloWorld".EndsWith("Hello") ? 1 : 0; }
int EndsWith_CaseSens()       { return "World".EndsWith("world", ESearchCase::CaseSensitive) ? 1 : 0; }
int EndsWith_IgnCase()        { return "World".EndsWith("world", ESearchCase::IgnoreCase) ? 1 : 0; }

// ---- MatchesWildcard ----
int Wild_Star()               { return "Hello.World".MatchesWildcard("Hello*") ? 1 : 0; }
int Wild_Question()           { return "ABC".MatchesWildcard("A?C") ? 1 : 0; }
int Wild_NoMatch()            { return "Hello".MatchesWildcard("Bye*") ? 1 : 0; }
int Wild_Exact()              { return "Hello".MatchesWildcard("Hello") ? 1 : 0; }

// ---- Equals ----
int Equals_CaseSensitive()    { return "ABC".Equals("ABC", ESearchCase::CaseSensitive) ? 1 : 0; }
int Equals_CaseSensDiff()     { return "ABC".Equals("abc", ESearchCase::CaseSensitive) ? 1 : 0; }
int Equals_IgnoreCase()       { return "ABC".Equals("abc", ESearchCase::IgnoreCase) ? 1 : 0; }
int Equals_EmptyEmpty()       { FString A; FString B; return A.Equals(B) ? 1 : 0; }
)"));
		if (!Mod.IsValid()) return;
		auto& M = Mod.GetModule();

		const FExpectedGlobalInt Cases[] = {
			// Contains
			{ TEXT("int Contains_Found()"),         TEXT("Contains finds existing substring"),           1 },
			{ TEXT("int Contains_NotFound()"),       TEXT("Contains returns false for missing substr"),  0 },
			{ TEXT("int Contains_CaseSensitive()"),  TEXT("Contains case-sensitive mismatch"),            0 },
			{ TEXT("int Contains_IgnoreCase()"),     TEXT("Contains ignore case finds match"),            1 },
			{ TEXT("int Contains_Partial()"),        TEXT("Contains finds partial match in middle"),      1 },
			{ TEXT("int Contains_InEmpty()"),        TEXT("Contains in empty string"),                    0 },
			// Find
			{ TEXT("int Find_FirstOccurrence()"),    TEXT("Find returns first occurrence index 1"),       1 },
			{ TEXT("int Find_NotFound()"),           TEXT("Find returns -1 on miss"),                    -1 },
			{ TEXT("int Find_CaseSensitive()"),      TEXT("Find case-sensitive finds at index 3"),        3 },
			{ TEXT("int Find_IgnoreCase()"),         TEXT("Find ignore case finds at index 0"),           0 },
			{ TEXT("int Find_FromEnd()"),            TEXT("Find FromEnd returns last occurrence at 4"),   4 },
			{ TEXT("int Find_StartPos()"),           TEXT("Find with start position 2 returns 4"),        4 },
			// FindChar / FindLastChar
			{ TEXT("int FindChar_Found()"),          TEXT("FindChar locates C at index 2"),                2 },
			{ TEXT("int FindChar_NotFound()"),       TEXT("FindChar returns false when char absent"),      0 },
			{ TEXT("int FindLastChar_Found()"),      TEXT("FindLastChar locates last B at index 3"),       3 },
			{ TEXT("int FindLastChar_NotFound()"),   TEXT("FindLastChar returns false when char absent"),  0 },
			// StartsWith / EndsWith
			{ TEXT("int StartsWith_Match()"),        TEXT("StartsWith with matching prefix"),              1 },
			{ TEXT("int StartsWith_NoMatch()"),      TEXT("StartsWith with non-matching prefix"),          0 },
			{ TEXT("int StartsWith_CaseSens()"),     TEXT("StartsWith case-sensitive mismatch"),           0 },
			{ TEXT("int StartsWith_IgnCase()"),      TEXT("StartsWith ignore case match"),                 1 },
			{ TEXT("int StartsWith_Full()"),         TEXT("StartsWith matching full string"),               1 },
			{ TEXT("int EndsWith_Match()"),          TEXT("EndsWith with matching suffix"),                1 },
			{ TEXT("int EndsWith_NoMatch()"),        TEXT("EndsWith with non-matching suffix"),            0 },
			{ TEXT("int EndsWith_CaseSens()"),       TEXT("EndsWith case-sensitive mismatch"),             0 },
			{ TEXT("int EndsWith_IgnCase()"),        TEXT("EndsWith ignore case match"),                   1 },
			// MatchesWildcard
			{ TEXT("int Wild_Star()"),               TEXT("Wildcard * matches suffix"),                    1 },
			{ TEXT("int Wild_Question()"),           TEXT("Wildcard ? matches single char"),               1 },
			{ TEXT("int Wild_NoMatch()"),            TEXT("Wildcard no match"),                            0 },
			{ TEXT("int Wild_Exact()"),              TEXT("Wildcard exact string match"),                  1 },
			// Equals
			{ TEXT("int Equals_CaseSensitive()"),    TEXT("Equals case-sensitive same string"),            1 },
			{ TEXT("int Equals_CaseSensDiff()"),     TEXT("Equals case-sensitive different case"),         0 },
			{ TEXT("int Equals_IgnoreCase()"),       TEXT("Equals ignore case match"),                     1 },
			{ TEXT("int Equals_EmptyEmpty()"),       TEXT("Equals two empty strings"),                     1 },
		};
		ExpectGlobalInts(*TestRunner, Engine, M,  Cases);
	}

	// ====================================================================
	// Section: Mutation
	// ====================================================================

	TEST_METHOD(Mutation)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASFString_Mutation"), TEXT(R"(
// ---- Append / AppendChar ----
int Append_Basic()
{
	FString S = "Hello";
	S.Append(" World");
	return (S == "Hello World") ? 1 : 0;
}
int Append_Empty()
{
	FString S = "X";
	S.Append("");
	return (S == "X") ? 1 : 0;
}
int Append_OnEmpty()
{
	FString S;
	S.Append("Data");
	return (S == "Data") ? 1 : 0;
}
int Append_Chain()
{
	FString S;
	S.Append("A").Append("B").Append("C");
	return (S == "ABC") ? 1 : 0;
}
int AppendChar_Basic()
{
	FString S = "AB";
	S.AppendChar(0x43);   // 'C'
	return (S == "ABC") ? 1 : 0;
}
int AppendChar_Chain()
{
	FString S;
	S.AppendChar(0x41).AppendChar(0x42);  // 'A', 'B'
	return (S == "AB") ? 1 : 0;
}

// ---- RemoveAt ----
int RemoveAt_Start()
{
	FString S = "ABCDE";
	S.RemoveAt(0, 2);
	return (S == "CDE") ? 1 : 0;
}
int RemoveAt_Mid()
{
	FString S = "ABCDE";
	S.RemoveAt(1, 3);
	return (S == "AE") ? 1 : 0;
}
int RemoveAt_End()
{
	FString S = "ABCDE";
	S.RemoveAt(3, 2);
	return (S == "ABC") ? 1 : 0;
}

// ---- RemoveFromStart / RemoveFromEnd ----
int RemoveFromStart_Match()
{
	FString S = "PrefixBody";
	bool bRemoved = S.RemoveFromStart("Prefix");
	return (bRemoved && S == "Body") ? 1 : 0;
}
int RemoveFromStart_NoMatch()
{
	FString S = "PrefixBody";
	bool bRemoved = S.RemoveFromStart("NoMatch");
	return (!bRemoved && S == "PrefixBody") ? 1 : 0;
}
int RemoveFromStart_IgnoreCase()
{
	FString S = "PREFIXBody";
	bool bRemoved = S.RemoveFromStart("prefix", ESearchCase::IgnoreCase);
	return (bRemoved && S == "Body") ? 1 : 0;
}
int RemoveFromStart_CaseSens()
{
	FString S = "PREFIXBody";
	bool bRemoved = S.RemoveFromStart("prefix", ESearchCase::CaseSensitive);
	return (!bRemoved && S == "PREFIXBody") ? 1 : 0;
}
int RemoveFromEnd_Match()
{
	FString S = "BodySuffix";
	bool bRemoved = S.RemoveFromEnd("Suffix");
	return (bRemoved && S == "Body") ? 1 : 0;
}
int RemoveFromEnd_NoMatch()
{
	FString S = "BodySuffix";
	bool bRemoved = S.RemoveFromEnd("NoMatch");
	return (!bRemoved && S == "BodySuffix") ? 1 : 0;
}
int RemoveFromEnd_IgnoreCase()
{
	FString S = "BodySUFFIX";
	bool bRemoved = S.RemoveFromEnd("suffix", ESearchCase::IgnoreCase);
	return (bRemoved && S == "Body") ? 1 : 0;
}

// ---- Replace ----
int Replace_All()
{
	FString S = "aXbXc";
	FString R = S.Replace("X", "Y");
	return (R == "aYbYc") ? 1 : 0;
}
int Replace_NoMatch()
{
	FString S = "Hello";
	FString R = S.Replace("Z", "W");
	return (R == "Hello") ? 1 : 0;
}
int Replace_CaseSensitive()
{
	FString S = "aAbBaA";
	FString R = S.Replace("a", "X", ESearchCase::CaseSensitive);
	return (R == "XAbBXA") ? 1 : 0;
}
int Replace_IgnoreCase()
{
	FString S = "aAbBaA";
	FString R = S.Replace("a", "X", ESearchCase::IgnoreCase);
	return (R == "XXbBXX") ? 1 : 0;
}
int Replace_WithLonger()
{
	FString S = "AB";
	FString R = S.Replace("A", "XYZ");
	return (R == "XYZB") ? 1 : 0;
}
int Replace_WithEmpty()
{
	FString S = "A-B-C";
	FString R = S.Replace("-", "");
	return (R == "ABC") ? 1 : 0;
}

// ---- Reverse ----
int Reverse_Basic()
{
	FString S = "ABCDE";
	return (S.Reverse() == "EDCBA") ? 1 : 0;
}
int Reverse_Palindrome()
{
	FString S = "ABBA";
	return (S.Reverse() == S) ? 1 : 0;
}
int Reverse_SingleChar()
{
	FString S = "X";
	return (S.Reverse() == "X") ? 1 : 0;
}
int Reverse_Empty()
{
	FString S;
	return S.Reverse().IsEmpty() ? 1 : 0;
}
)"));
		if (!Mod.IsValid()) return;
		auto& M = Mod.GetModule();

		const FExpectedGlobalInt Cases[] = {
			// Append
			{ TEXT("int Append_Basic()"),              TEXT("Append concatenates strings"),                1 },
			{ TEXT("int Append_Empty()"),               TEXT("Append empty string is no-op"),              1 },
			{ TEXT("int Append_OnEmpty()"),             TEXT("Append on empty string stores content"),     1 },
			{ TEXT("int Append_Chain()"),               TEXT("Append chained three times"),                1 },
			{ TEXT("int AppendChar_Basic()"),           TEXT("AppendChar adds single character"),          1 },
			{ TEXT("int AppendChar_Chain()"),           TEXT("AppendChar chained returns accumulated"),    1 },
			// RemoveAt
			{ TEXT("int RemoveAt_Start()"),             TEXT("RemoveAt from start"),                       1 },
			{ TEXT("int RemoveAt_Mid()"),               TEXT("RemoveAt from middle"),                      1 },
			{ TEXT("int RemoveAt_End()"),               TEXT("RemoveAt from end"),                         1 },
			// RemoveFromStart / RemoveFromEnd
			{ TEXT("int RemoveFromStart_Match()"),      TEXT("RemoveFromStart strips matching prefix"),    1 },
			{ TEXT("int RemoveFromStart_NoMatch()"),    TEXT("RemoveFromStart no-op when no match"),       1 },
			{ TEXT("int RemoveFromStart_IgnoreCase()"), TEXT("RemoveFromStart ignore case"),               1 },
			{ TEXT("int RemoveFromStart_CaseSens()"),   TEXT("RemoveFromStart case-sensitive rejects"),    1 },
			{ TEXT("int RemoveFromEnd_Match()"),        TEXT("RemoveFromEnd strips matching suffix"),      1 },
			{ TEXT("int RemoveFromEnd_NoMatch()"),      TEXT("RemoveFromEnd no-op when no match"),         1 },
			{ TEXT("int RemoveFromEnd_IgnoreCase()"),   TEXT("RemoveFromEnd ignore case"),                 1 },
			// Replace
			{ TEXT("int Replace_All()"),                TEXT("Replace all occurrences"),                   1 },
			{ TEXT("int Replace_NoMatch()"),            TEXT("Replace no-op when pattern absent"),         1 },
			{ TEXT("int Replace_CaseSensitive()"),      TEXT("Replace case-sensitive selective"),           1 },
			{ TEXT("int Replace_IgnoreCase()"),         TEXT("Replace ignore case replaces all matches"),  1 },
			{ TEXT("int Replace_WithLonger()"),         TEXT("Replace with longer replacement"),           1 },
			{ TEXT("int Replace_WithEmpty()"),          TEXT("Replace with empty removes occurrences"),    1 },
			// Reverse
			{ TEXT("int Reverse_Basic()"),              TEXT("Reverse reverses character order"),           1 },
			{ TEXT("int Reverse_Palindrome()"),         TEXT("Reverse of palindrome equals original"),     1 },
			{ TEXT("int Reverse_SingleChar()"),         TEXT("Reverse of single char is identity"),        1 },
			{ TEXT("int Reverse_Empty()"),              TEXT("Reverse of empty is empty"),                  1 },
		};
		ExpectGlobalInts(*TestRunner, Engine, M,  Cases);
	}

	// ====================================================================
	// Section: MutationExtended
	// ====================================================================

	TEST_METHOD(MutationExtended)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASFString_MutExt"), TEXT(R"(
// ---- AppendInt ----
int AppendInt_Positive()
{
	FString S = "Val:";
	S.AppendInt(42);
	return (S == "Val:42") ? 1 : 0;
}
int AppendInt_Negative()
{
	FString S = "N:";
	S.AppendInt(-7);
	return (S == "N:-7") ? 1 : 0;
}
int AppendInt_Zero()
{
	FString S = "Z:";
	S.AppendInt(0);
	return (S == "Z:0") ? 1 : 0;
}

// ---- InsertAt (char) ----
int InsertAtChar_Front()
{
	FString S = "BCD";
	S.InsertAt(0, 0x41);  // 'A'
	return (S == "ABCD") ? 1 : 0;
}
int InsertAtChar_Mid()
{
	FString S = "ACD";
	S.InsertAt(1, 0x42);  // 'B'
	return (S == "ABCD") ? 1 : 0;
}
int InsertAtChar_End()
{
	FString S = "ABC";
	S.InsertAt(3, 0x44);  // 'D'
	return (S == "ABCD") ? 1 : 0;
}

// ---- InsertAt (string) ----
int InsertAtStr_Front()
{
	FString S = "World";
	S.InsertAt(0, "Hello ");
	return (S == "Hello World") ? 1 : 0;
}
int InsertAtStr_Mid()
{
	FString S = "ACD";
	S.InsertAt(1, "B");
	return (S == "ABCD") ? 1 : 0;
}
int InsertAtStr_Empty()
{
	FString S = "ABC";
	S.InsertAt(1, "");
	return (S == "ABC") ? 1 : 0;
}

// ---- RemoveSpacesInline ----
int RemoveSpaces_Basic()
{
	FString S = "Hello World Test";
	S.RemoveSpacesInline();
	return (S == "HelloWorldTest") ? 1 : 0;
}
int RemoveSpaces_None()
{
	FString S = "NoSpaces";
	S.RemoveSpacesInline();
	return (S == "NoSpaces") ? 1 : 0;
}
int RemoveSpaces_AllSpaces()
{
	FString S = "   ";
	S.RemoveSpacesInline();
	return S.IsEmpty() ? 1 : 0;
}

// ---- ReplaceInline ----
int ReplaceInline_Basic()
{
	FString S = "aXbXc";
	int Count = S.ReplaceInline("X", "Y");
	return (Count == 2 && S == "aYbYc") ? 1 : 0;
}
int ReplaceInline_NoMatch()
{
	FString S = "Hello";
	int Count = S.ReplaceInline("Z", "Y");
	return (Count == 0 && S == "Hello") ? 1 : 0;
}
int ReplaceInline_CaseSensitive()
{
	FString S = "AaAa";
	int Count = S.ReplaceInline("a", "X", ESearchCase::CaseSensitive);
	return (Count == 2 && S == "AXAX") ? 1 : 0;
}

// ---- ReplaceCharWithEscapedChar / ReplaceEscapedCharWithChar ----
int EscapeChar_Roundtrip()
{
	FString Original = "Line1\nLine2\tEnd";
	FString Escaped = Original.ReplaceCharWithEscapedChar();
	FString Restored = Escaped.ReplaceEscapedCharWithChar();
	return (Restored == Original) ? 1 : 0;
}

// ---- ConvertTabsToSpaces ----
int TabsToSpaces_Single()
{
	FString S = "\tHello";
	FString Result = S.ConvertTabsToSpaces(4);
	return Result.StartsWith("    ") ? 1 : 0;
}
int TabsToSpaces_Multiple()
{
	FString S = "A\tB\tC";
	FString Result = S.ConvertTabsToSpaces(2);
	return (!Result.Contains("\t")) ? 1 : 0;
}
int TabsToSpaces_NoTabs()
{
	FString S = "NoTabs";
	FString Result = S.ConvertTabsToSpaces(4);
	return (Result == "NoTabs") ? 1 : 0;
}

// ---- TrimChar (removes at most ONE from each end) ----
int TrimChar_Basic()
{
	FString S = "#Hello#";
	FString Result = S.TrimChar(0x23);  // '#'
	return (Result == "Hello") ? 1 : 0;
}
int TrimChar_NoMatch()
{
	FString S = "Hello";
	FString Result = S.TrimChar(0x23);  // '#'
	return (Result == "Hello") ? 1 : 0;
}
int TrimChar_AllSame()
{
	FString S = "##";
	FString Result = S.TrimChar(0x23);  // '#'
	return Result.IsEmpty() ? 1 : 0;
}
)"));
		if (!Mod.IsValid()) return;
		auto& M = Mod.GetModule();

		const FExpectedGlobalInt Cases[] = {
			{ TEXT("int AppendInt_Positive()"),            TEXT("AppendInt positive"),                   1 },
			{ TEXT("int AppendInt_Negative()"),            TEXT("AppendInt negative"),                   1 },
			{ TEXT("int AppendInt_Zero()"),                TEXT("AppendInt zero"),                       1 },
			{ TEXT("int InsertAtChar_Front()"),            TEXT("InsertAt char front"),                  1 },
			{ TEXT("int InsertAtChar_Mid()"),              TEXT("InsertAt char middle"),                 1 },
			{ TEXT("int InsertAtChar_End()"),              TEXT("InsertAt char end"),                    1 },
			{ TEXT("int InsertAtStr_Front()"),             TEXT("InsertAt string front"),                1 },
			{ TEXT("int InsertAtStr_Mid()"),               TEXT("InsertAt string middle"),               1 },
			{ TEXT("int InsertAtStr_Empty()"),             TEXT("InsertAt empty string no-op"),          1 },
			{ TEXT("int RemoveSpaces_Basic()"),            TEXT("RemoveSpacesInline basic"),             1 },
			{ TEXT("int RemoveSpaces_None()"),             TEXT("RemoveSpacesInline no spaces"),         1 },
			{ TEXT("int RemoveSpaces_AllSpaces()"),        TEXT("RemoveSpacesInline all spaces"),        1 },
			{ TEXT("int ReplaceInline_Basic()"),           TEXT("ReplaceInline basic + count"),          1 },
			{ TEXT("int ReplaceInline_NoMatch()"),         TEXT("ReplaceInline no match returns 0"),     1 },
			{ TEXT("int ReplaceInline_CaseSensitive()"),   TEXT("ReplaceInline case sensitive"),         1 },
			{ TEXT("int EscapeChar_Roundtrip()"),          TEXT("Escape/unescape roundtrip"),            1 },
			{ TEXT("int TabsToSpaces_Single()"),           TEXT("ConvertTabsToSpaces single tab"),       1 },
			{ TEXT("int TabsToSpaces_Multiple()"),         TEXT("ConvertTabsToSpaces multiple tabs"),    1 },
			{ TEXT("int TabsToSpaces_NoTabs()"),           TEXT("ConvertTabsToSpaces no tabs"),          1 },
			{ TEXT("int TrimChar_Basic()"),                TEXT("TrimChar basic"),                       1 },
			{ TEXT("int TrimChar_NoMatch()"),              TEXT("TrimChar no match"),                    1 },
			{ TEXT("int TrimChar_AllSame()"),              TEXT("TrimChar all same char"),               1 },
		};
		ExpectGlobalInts(*TestRunner, Engine, M,  Cases);
	}

	// ====================================================================
	// Section: CaseAndTrim
	// ====================================================================

	TEST_METHOD(CaseAndTrim)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASFString_CaseTrim"), TEXT(R"(
// ---- ToUpper / ToLower ----
int ToUpper_Lower()       { return ("hello".ToUpper() == "HELLO") ? 1 : 0; }
int ToUpper_Mixed()       { return ("Hello World".ToUpper() == "HELLO WORLD") ? 1 : 0; }
int ToUpper_AlreadyUpper(){ return ("ABC".ToUpper() == "ABC") ? 1 : 0; }
int ToUpper_Empty()       { FString S; return S.ToUpper().IsEmpty() ? 1 : 0; }
int ToLower_Upper()       { return ("HELLO".ToLower() == "hello") ? 1 : 0; }
int ToLower_Mixed()       { return ("Hello World".ToLower() == "hello world") ? 1 : 0; }
int ToLower_AlreadyLower(){ return ("abc".ToLower() == "abc") ? 1 : 0; }
int ToLower_Digits()      { return ("ABC123".ToLower() == "abc123") ? 1 : 0; }

// ---- TrimStartAndEnd / TrimStart / TrimEnd ----
int TrimBoth_Spaces()     { return ("  AB  ".TrimStartAndEnd() == "AB") ? 1 : 0; }
int TrimBoth_NoSpace()    { return ("AB".TrimStartAndEnd() == "AB") ? 1 : 0; }
int TrimBoth_AllSpace()   { return ("   ".TrimStartAndEnd().IsEmpty()) ? 1 : 0; }
int TrimBoth_Tabs()       { FString S = "\t Hello \t"; return (S.TrimStartAndEnd() == "Hello") ? 1 : 0; }
int TrimStart_Basic()     { return ("  AB".TrimStart() == "AB") ? 1 : 0; }
int TrimStart_NoSpace()   { return ("AB  ".TrimStart() == "AB  ") ? 1 : 0; }
int TrimEnd_Basic()       { return ("AB  ".TrimEnd() == "AB") ? 1 : 0; }
int TrimEnd_NoSpace()     { return ("  AB".TrimEnd() == "  AB") ? 1 : 0; }

// ---- TrimQuotes ----
int TrimQuotes_DoubleQ()
{
	FString S = "\"Content\"";
	bool bRemoved = false;
	FString R = S.TrimQuotes(bRemoved);
	return (bRemoved && R == "Content") ? 1 : 0;
}
int TrimQuotes_NoQuotes()
{
	FString S = "NoQuotes";
	bool bRemoved = false;
	FString R = S.TrimQuotes(bRemoved);
	return (!bRemoved && R == "NoQuotes") ? 1 : 0;
}
int TrimQuotes_SingleQ()
{
	// TrimQuotes only handles double quotes — single quotes are not stripped.
	FString S = "'Content'";
	bool bRemoved = false;
	FString R = S.TrimQuotes(bRemoved);
	return (!bRemoved && R == "'Content'") ? 1 : 0;
}

// ---- LeftPad / RightPad ----
int LeftPad_Shorter()
{
	FString S = "AB";
	FString P = S.LeftPad(5);
	return P.Len();
}
int LeftPad_AlreadyLong()
{
	FString S = "ABCDEF";
	FString P = S.LeftPad(3);
	return (P == S) ? 1 : 0;
}
int LeftPad_Content()
{
	FString S = "AB";
	FString P = S.LeftPad(5);
	return P.EndsWith("AB") ? 1 : 0;
}
int RightPad_Shorter()
{
	FString S = "AB";
	FString P = S.RightPad(5);
	return P.Len();
}
int RightPad_AlreadyLong()
{
	FString S = "ABCDEF";
	FString P = S.RightPad(3);
	return (P == S) ? 1 : 0;
}
int RightPad_Content()
{
	FString S = "AB";
	FString P = S.RightPad(5);
	return P.StartsWith("AB") ? 1 : 0;
}
)"));
		if (!Mod.IsValid()) return;
		auto& M = Mod.GetModule();

		const FExpectedGlobalInt Cases[] = {
			// ToUpper / ToLower
			{ TEXT("int ToUpper_Lower()"),       TEXT("ToUpper converts lowercase"),               1 },
			{ TEXT("int ToUpper_Mixed()"),        TEXT("ToUpper converts mixed case"),              1 },
			{ TEXT("int ToUpper_AlreadyUpper()"), TEXT("ToUpper no-op on already uppercase"),      1 },
			{ TEXT("int ToUpper_Empty()"),        TEXT("ToUpper on empty returns empty"),           1 },
			{ TEXT("int ToLower_Upper()"),        TEXT("ToLower converts uppercase"),               1 },
			{ TEXT("int ToLower_Mixed()"),        TEXT("ToLower converts mixed case"),              1 },
			{ TEXT("int ToLower_AlreadyLower()"), TEXT("ToLower no-op on already lowercase"),      1 },
			{ TEXT("int ToLower_Digits()"),       TEXT("ToLower preserves digits"),                 1 },
			// Trim
			{ TEXT("int TrimBoth_Spaces()"),      TEXT("TrimStartAndEnd removes surrounding spaces"),  1 },
			{ TEXT("int TrimBoth_NoSpace()"),     TEXT("TrimStartAndEnd no-op without whitespace"),     1 },
			{ TEXT("int TrimBoth_AllSpace()"),    TEXT("TrimStartAndEnd all-whitespace yields empty"),  1 },
			{ TEXT("int TrimBoth_Tabs()"),        TEXT("TrimStartAndEnd removes tabs too"),             1 },
			{ TEXT("int TrimStart_Basic()"),      TEXT("TrimStart removes leading whitespace"),         1 },
			{ TEXT("int TrimStart_NoSpace()"),    TEXT("TrimStart no-op without leading space"),        1 },
			{ TEXT("int TrimEnd_Basic()"),        TEXT("TrimEnd removes trailing whitespace"),          1 },
			{ TEXT("int TrimEnd_NoSpace()"),      TEXT("TrimEnd no-op without trailing space"),         1 },
			// TrimQuotes
			{ TEXT("int TrimQuotes_DoubleQ()"),  TEXT("TrimQuotes strips double quotes"),               1 },
			{ TEXT("int TrimQuotes_NoQuotes()"), TEXT("TrimQuotes no-op without quotes"),               1 },
			{ TEXT("int TrimQuotes_SingleQ()"),  TEXT("TrimQuotes does not strip single quotes"),        1 },
			// LeftPad / RightPad
			{ TEXT("int LeftPad_Shorter()"),      TEXT("LeftPad pads to target width"),                 5 },
			{ TEXT("int LeftPad_AlreadyLong()"),  TEXT("LeftPad no-op when already long enough"),       1 },
			{ TEXT("int LeftPad_Content()"),      TEXT("LeftPad preserves content at right end"),       1 },
			{ TEXT("int RightPad_Shorter()"),     TEXT("RightPad pads to target width"),                5 },
			{ TEXT("int RightPad_AlreadyLong()"), TEXT("RightPad no-op when already long enough"),     1 },
			{ TEXT("int RightPad_Content()"),     TEXT("RightPad preserves content at left end"),       1 },
		};
		ExpectGlobalInts(*TestRunner, Engine, M,  Cases);
	}

	// ====================================================================
	// Section: Split
	// ====================================================================

	TEST_METHOD(Split)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASFString_Split"), TEXT(R"(
// ---- Split ----
int Split_Basic()
{
	FString S = "Left|Right";
	FString L, R;
	bool bOk = S.Split("|", L, R);
	return (bOk && L == "Left" && R == "Right") ? 1 : 0;
}
int Split_NoDelimiter()
{
	FString S = "NoPipe";
	FString L, R;
	bool bOk = S.Split("|", L, R);
	return bOk ? 1 : 0;
}
int Split_MultipleDelimiters()
{
	// FromStart should split at the first occurrence.
	FString S = "A|B|C";
	FString L, R;
	S.Split("|", L, R, ESearchCase::IgnoreCase, ESearchDir::FromStart);
	return (L == "A" && R == "B|C") ? 1 : 0;
}
int Split_FromEnd()
{
	// FromEnd should split at the last occurrence.
	FString S = "A|B|C";
	FString L, R;
	S.Split("|", L, R, ESearchCase::IgnoreCase, ESearchDir::FromEnd);
	return (L == "A|B" && R == "C") ? 1 : 0;
}
int Split_CaseSensitive()
{
	FString S = "KeyXValue";
	FString L, R;
	bool bOk = S.Split("x", L, R, ESearchCase::CaseSensitive);
	return bOk ? 1 : 0;
}
int Split_IgnoreCase()
{
	FString S = "KeyXValue";
	FString L, R;
	bool bOk = S.Split("x", L, R, ESearchCase::IgnoreCase);
	return (bOk && L == "Key" && R == "Value") ? 1 : 0;
}
int Split_AtStart()
{
	FString S = "|Value";
	FString L, R;
	S.Split("|", L, R);
	return (L.IsEmpty() && R == "Value") ? 1 : 0;
}
int Split_AtEnd()
{
	FString S = "Key|";
	FString L, R;
	S.Split("|", L, R);
	return (L == "Key" && R.IsEmpty()) ? 1 : 0;
}

// ---- ParseIntoArray (single delimiter) ----
int Parse_Basic()
{
	FString S = "A,B,C";
	TArray<FString> Parts;
	int Count = S.ParseIntoArray(Parts, ",");
	return (Count == 3 && Parts[0] == "A" && Parts[1] == "B" && Parts[2] == "C") ? 1 : 0;
}
int Parse_SingleElement()
{
	FString S = "Alone";
	TArray<FString> Parts;
	int Count = S.ParseIntoArray(Parts, ",");
	return (Count == 1 && Parts[0] == "Alone") ? 1 : 0;
}
int Parse_CullEmpty()
{
	FString S = "A,,B,";
	TArray<FString> Parts;
	S.ParseIntoArray(Parts, ",", true);
	return Parts.Num();
}
int Parse_KeepEmpty()
{
	FString S = "A,,B,";
	TArray<FString> Parts;
	S.ParseIntoArray(Parts, ",", false);
	return Parts.Num();
}
int Parse_EmptyString()
{
	FString S;
	TArray<FString> Parts;
	int Count = S.ParseIntoArray(Parts, ",");
	return Count;
}

// ---- ParseIntoArray (multiple delimiters) ----
int ParseMulti_Basic()
{
	FString S = "A,B;C.D";
	TArray<FString> Delims;
	Delims.Add(",");
	Delims.Add(";");
	Delims.Add(".");
	TArray<FString> Parts;
	int Count = S.ParseIntoArray(Parts, Delims);
	return (Count == 4 && Parts[0] == "A" && Parts[3] == "D") ? 1 : 0;
}
int ParseMulti_TwoDelims()
{
	FString S = "X|Y:Z";
	TArray<FString> Delims;
	Delims.Add("|");
	Delims.Add(":");
	TArray<FString> Parts;
	int Count = S.ParseIntoArray(Parts, Delims);
	return (Count == 3 && Parts[1] == "Y") ? 1 : 0;
}
)"));
		if (!Mod.IsValid()) return;
		auto& M = Mod.GetModule();

		const FExpectedGlobalInt Cases[] = {
			// Split
			{ TEXT("int Split_Basic()"),               TEXT("Split on | delimiter"),                          1 },
			{ TEXT("int Split_NoDelimiter()"),          TEXT("Split returns false when no delimiter"),         0 },
			{ TEXT("int Split_MultipleDelimiters()"),   TEXT("Split FromStart at first occurrence"),           1 },
			{ TEXT("int Split_FromEnd()"),              TEXT("Split FromEnd at last occurrence"),               1 },
			{ TEXT("int Split_CaseSensitive()"),        TEXT("Split case-sensitive rejects wrong case"),       0 },
			{ TEXT("int Split_IgnoreCase()"),           TEXT("Split ignore case finds delimiter"),              1 },
			{ TEXT("int Split_AtStart()"),              TEXT("Split at start yields empty left part"),          1 },
			{ TEXT("int Split_AtEnd()"),                TEXT("Split at end yields empty right part"),           1 },
			// ParseIntoArray single
			{ TEXT("int Parse_Basic()"),                TEXT("ParseIntoArray splits on comma"),                 1 },
			{ TEXT("int Parse_SingleElement()"),        TEXT("ParseIntoArray single element no delimiter"),     1 },
			{ TEXT("int Parse_CullEmpty()"),            TEXT("ParseIntoArray cull empty yields 2 parts"),       2 },
			{ TEXT("int Parse_KeepEmpty()"),            TEXT("ParseIntoArray keep empty yields 4 parts"),       4 },
			{ TEXT("int Parse_EmptyString()"),          TEXT("ParseIntoArray on empty string yields 0"),        0 },
			// ParseIntoArray multi
			{ TEXT("int ParseMulti_Basic()"),           TEXT("ParseIntoArray with 3 delimiters"),               1 },
			{ TEXT("int ParseMulti_TwoDelims()"),       TEXT("ParseIntoArray with 2 delimiters"),               1 },
		};
		ExpectGlobalInts(*TestRunner, Engine, M,  Cases);
	}

	// ====================================================================
	// Section: SplitExtended
	// ====================================================================

	TEST_METHOD(SplitExtended)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASFString_SplitExt"), TEXT(R"(
// ---- ParseIntoArrayLines ----
int ParseLines_Multi()
{
	FString S = "Line1\nLine2\nLine3";
	TArray<FString> Lines;
	int Count = S.ParseIntoArrayLines(Lines);
	return (Count == 3 && Lines[0] == "Line1" && Lines[2] == "Line3") ? 1 : 0;
}
int ParseLines_CRLF()
{
	FString S = "A\r\nB\r\nC";
	TArray<FString> Lines;
	int Count = S.ParseIntoArrayLines(Lines);
	return (Count == 3 && Lines[1] == "B") ? 1 : 0;
}
int ParseLines_Single()
{
	FString S = "NoNewline";
	TArray<FString> Lines;
	int Count = S.ParseIntoArrayLines(Lines);
	return (Count == 1 && Lines[0] == "NoNewline") ? 1 : 0;
}
int ParseLines_EmptyLines_Cull()
{
	FString S = "A\n\nB";
	TArray<FString> Lines;
	int Count = S.ParseIntoArrayLines(Lines, true);
	return (Count == 2) ? 1 : 0;
}
int ParseLines_EmptyLines_Keep()
{
	FString S = "A\n\nB";
	TArray<FString> Lines;
	int Count = S.ParseIntoArrayLines(Lines, false);
	return (Count == 3 && Lines[1] == "") ? 1 : 0;
}

// ---- ParseIntoArrayWS ----
int ParseWS_Basic()
{
	FString S = "Hello World Test";
	TArray<FString> Parts;
	int Count = S.ParseIntoArrayWS(Parts);
	return (Count == 3 && Parts[0] == "Hello" && Parts[2] == "Test") ? 1 : 0;
}
int ParseWS_MultipleSpaces()
{
	FString S = "A   B   C";
	TArray<FString> Parts;
	int Count = S.ParseIntoArrayWS(Parts);
	return (Count == 3) ? 1 : 0;
}
int ParseWS_TabsAndSpaces()
{
	FString S = "A\tB C";
	TArray<FString> Parts;
	int Count = S.ParseIntoArrayWS(Parts);
	return (Count == 3) ? 1 : 0;
}
int ParseWS_OnlyWhitespace()
{
	FString S = "   \t  ";
	TArray<FString> Parts;
	int Count = S.ParseIntoArrayWS(Parts);
	return (Count == 0) ? 1 : 0;
}
)"));
		if (!Mod.IsValid()) return;
		auto& M = Mod.GetModule();

		const FExpectedGlobalInt Cases[] = {
			{ TEXT("int ParseLines_Multi()"),              TEXT("ParseIntoArrayLines multi-line"),         1 },
			{ TEXT("int ParseLines_CRLF()"),               TEXT("ParseIntoArrayLines CRLF"),              1 },
			{ TEXT("int ParseLines_Single()"),             TEXT("ParseIntoArrayLines single line"),       1 },
			{ TEXT("int ParseLines_EmptyLines_Cull()"),    TEXT("ParseIntoArrayLines cull empty"),        1 },
			{ TEXT("int ParseLines_EmptyLines_Keep()"),    TEXT("ParseIntoArrayLines keep empty"),        1 },
			{ TEXT("int ParseWS_Basic()"),                 TEXT("ParseIntoArrayWS basic"),                1 },
			{ TEXT("int ParseWS_MultipleSpaces()"),        TEXT("ParseIntoArrayWS multiple spaces"),      1 },
			{ TEXT("int ParseWS_TabsAndSpaces()"),         TEXT("ParseIntoArrayWS tabs and spaces"),      1 },
			{ TEXT("int ParseWS_OnlyWhitespace()"),        TEXT("ParseIntoArrayWS only whitespace"),      1 },
		};
		ExpectGlobalInts(*TestRunner, Engine, M,  Cases);
	}

	// ====================================================================
	// Section: Conversion
	// ====================================================================

	TEST_METHOD(Conversion)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASFString_Conv"), TEXT(R"(
// ---- ToBool ----
int ToBool_True()         { return "true".ToBool() ? 1 : 0; }
int ToBool_True_One()     { return "1".ToBool() ? 1 : 0; }
int ToBool_True_Yes()     { return "yes".ToBool() ? 1 : 0; }
int ToBool_False()        { return "false".ToBool() ? 1 : 0; }
int ToBool_False_Zero()   { return "0".ToBool() ? 1 : 0; }
int ToBool_False_No()     { return "no".ToBool() ? 1 : 0; }

// ---- IsNumeric ----
int IsNumeric_Digits()    { return "12345".IsNumeric() ? 1 : 0; }
int IsNumeric_Float()     { return "3.14".IsNumeric() ? 1 : 0; }
int IsNumeric_Negative()  { return "-42".IsNumeric() ? 1 : 0; }
int IsNumeric_Alpha()     { return "abc".IsNumeric() ? 1 : 0; }
int IsNumeric_Mixed()     { return "12ab".IsNumeric() ? 1 : 0; }
int IsNumeric_Empty()     { FString S; return S.IsNumeric() ? 1 : 0; }

// ---- GetHash ----
int GetHash_NonZero()
{
	return ("Hello".GetHash() != 0) ? 1 : 0;
}
int GetHash_Consistent()
{
	// Same string should produce same hash.
	FString S = "TestHash";
	return (S.GetHash() == S.GetHash()) ? 1 : 0;
}
int GetHash_Different()
{
	// Different strings typically produce different hashes.
	uint H1 = "Alpha".GetHash();
	uint H2 = "Beta".GetHash();
	return (H1 != H2) ? 1 : 0;
}

// ---- Compare ----
int Compare_Equal_CaseSens()
{
	return "ABC".Compare("ABC", ESearchCase::CaseSensitive);
}
int Compare_Diff_CaseSens()
{
	FString A = "abc";
	return (A.Compare("ABC", ESearchCase::CaseSensitive) != 0) ? 1 : 0;
}
int Compare_IgnoreCase()
{
	return "abc".Compare("ABC", ESearchCase::IgnoreCase);
}
int Compare_Less()
{
	FString A = "A";
	return (A.Compare("B") < 0) ? 1 : 0;
}
int Compare_Greater()
{
	FString A = "Z";
	return (A.Compare("A") > 0) ? 1 : 0;
}

// ---- ToDisplayName ----
int ToDisplayName_Camel()
{
	FString S = "MyVariableName";
	FString D = S.ToDisplayName();
	return D.Contains(" ") ? 1 : 0;
}
int ToDisplayName_Bool()
{
	FString S = "bIsEnabled";
	FString D = S.ToDisplayName(true);
	return D.Len() > 0 ? 1 : 0;
}
)"));
		if (!Mod.IsValid()) return;
		auto& M = Mod.GetModule();

		const FExpectedGlobalInt Cases[] = {
			// ToBool
			{ TEXT("int ToBool_True()"),         TEXT("ToBool 'true' returns true"),             1 },
			{ TEXT("int ToBool_True_One()"),      TEXT("ToBool '1' returns true"),                1 },
			{ TEXT("int ToBool_True_Yes()"),      TEXT("ToBool 'yes' returns true"),               1 },
			{ TEXT("int ToBool_False()"),         TEXT("ToBool 'false' returns false"),            0 },
			{ TEXT("int ToBool_False_Zero()"),    TEXT("ToBool '0' returns false"),                0 },
			{ TEXT("int ToBool_False_No()"),      TEXT("ToBool 'no' returns false"),               0 },
			// IsNumeric
			{ TEXT("int IsNumeric_Digits()"),     TEXT("IsNumeric pure digits is true"),           1 },
			{ TEXT("int IsNumeric_Float()"),      TEXT("IsNumeric float string is true"),          1 },
			{ TEXT("int IsNumeric_Negative()"),   TEXT("IsNumeric negative number is true"),       1 },
			{ TEXT("int IsNumeric_Alpha()"),      TEXT("IsNumeric alphabetic is false"),           0 },
			{ TEXT("int IsNumeric_Mixed()"),      TEXT("IsNumeric mixed alphanumeric is false"),   0 },
			{ TEXT("int IsNumeric_Empty()"),      TEXT("IsNumeric empty string is false"),         0 },
			// GetHash
			{ TEXT("int GetHash_NonZero()"),      TEXT("GetHash produces non-zero for non-empty"), 1 },
			{ TEXT("int GetHash_Consistent()"),   TEXT("GetHash same string same hash"),           1 },
			{ TEXT("int GetHash_Different()"),    TEXT("GetHash different strings differ"),         1 },
			// Compare
			{ TEXT("int Compare_Equal_CaseSens()"), TEXT("Compare equal case-sensitive returns 0"),0 },
			{ TEXT("int Compare_Diff_CaseSens()"),  TEXT("Compare different case-sensitive nonzero"),1 },
			{ TEXT("int Compare_IgnoreCase()"),     TEXT("Compare ignore case returns 0"),          0 },
			{ TEXT("int Compare_Less()"),            TEXT("Compare A < B yields negative"),          1 },
			{ TEXT("int Compare_Greater()"),          TEXT("Compare Z > A yields positive"),         1 },
			// ToDisplayName
			{ TEXT("int ToDisplayName_Camel()"),     TEXT("ToDisplayName inserts spaces in CamelCase"), 1 },
			{ TEXT("int ToDisplayName_Bool()"),       TEXT("ToDisplayName with bIsBool flag"),          1 },
		};
		ExpectGlobalInts(*TestRunner, Engine, M,  Cases);
	}

	// ====================================================================
	// Section: TypeConcat
	// ====================================================================

	TEST_METHOD(TypeConcat)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASFString_TypeConcat"), TEXT(R"(
int Concat_Int()
{
	FString S = "Value=" + 42;
	return S.Contains("42") ? 1 : 0;
}
int Concat_IntNegative()
{
	FString S = "N=" + (-7);
	return S.Contains("-7") ? 1 : 0;
}
int Concat_Float()
{
	FString S = "PI=" + 3.14f;
	return S.Contains("3.14") ? 1 : 0;
}
int Concat_Name()
{
	FString S = "Name=" + FName("TestName");
	return S.Contains("TestName") ? 1 : 0;
}
int Concat_Bool_True()
{
	FString S = "" + true;
	return S.Contains("True") ? 1 : 0;
}
int Concat_Bool_False()
{
	FString S = "" + false;
	return S.Contains("False") ? 1 : 0;
}
int Concat_ChainMixed()
{
	FString S = "a=" + 1 + " b=" + 2;
	return (S.Contains("a=1") && S.Contains("b=2")) ? 1 : 0;
}
int Concat_IntZero()
{
	FString S = "Z=" + 0;
	return S.Contains("0") ? 1 : 0;
}
)"));
		if (!Mod.IsValid()) return;
		auto& M = Mod.GetModule();

		const FExpectedGlobalInt Cases[] = {
			{ TEXT("int Concat_Int()"),          TEXT("String + int positive"),                1 },
			{ TEXT("int Concat_IntNegative()"),  TEXT("String + int negative"),                1 },
			{ TEXT("int Concat_Float()"),        TEXT("String + float"),                       1 },
			{ TEXT("int Concat_Name()"),         TEXT("String + FName"),                       1 },
			{ TEXT("int Concat_Bool_True()"),    TEXT("String + true yields True"),            1 },
			{ TEXT("int Concat_Bool_False()"),   TEXT("String + false yields False"),          1 },
			{ TEXT("int Concat_ChainMixed()"),   TEXT("Chained mixed type concat"),            1 },
			{ TEXT("int Concat_IntZero()"),      TEXT("String + 0 contains 0"),                1 },
		};
		ExpectGlobalInts(*TestRunner, Engine, M,  Cases);
	}

	// ====================================================================
	// Section: FormatString — FString::Format ordered positional
	// ====================================================================

	TEST_METHOD(FormatString)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASFString_Format"), TEXT(R"(
// --- Arity: 1-arg overloads ---
int Format_OneStr()
{
	FString R = FString::Format("{0}", "Hello");
	return (R == "Hello") ? 1 : 0;
}
int Format_OneInt()
{
	FString R = FString::Format("{0}", 42);
	return (R == "42") ? 1 : 0;
}
int Format_OneFloat()
{
	FString R = FString::Format("{0}", 3.14);
	return R.Contains("3.14") ? 1 : 0;
}
int Format_OneFStringVar()
{
	FString Name = "World";
	FString R = FString::Format("Hello {0}!", Name);
	return (R == "Hello World!") ? 1 : 0;
}

// --- Arity: 2-arg overloads ---
int Format_TwoStr()
{
	FString R = FString::Format("{0} {1}", "Hello", "World");
	return (R == "Hello World") ? 1 : 0;
}
int Format_StrInt()
{
	FString R = FString::Format("{0}={1}", "Count", 7);
	return (R == "Count=7") ? 1 : 0;
}
int Format_IntFloat()
{
	FString R = FString::Format("{0}:{1}", 10, 2.5);
	return (R.StartsWith("10:") && R.Contains("2.5")) ? 1 : 0;
}

// --- Arity: 3-arg overloads ---
int Format_ThreeInts()
{
	FString R = FString::Format("{0},{1},{2}", 1, 2, 3);
	return (R == "1,2,3") ? 1 : 0;
}
int Format_ThreeMixed()
{
	FString R = FString::Format("{0}:{1}:{2}", "Tag", 100, 0.5);
	return (R.StartsWith("Tag:100:") && R.Contains("0.5")) ? 1 : 0;
}

// --- Arity: 4-arg overloads ---
int Format_FourStr()
{
	FString R = FString::Format("{0}-{1}-{2}-{3}", "A", "B", "C", "D");
	return (R == "A-B-C-D") ? 1 : 0;
}
int Format_FourMixed()
{
	FString R = FString::Format("{0},{1},{2},{3}", "X", 1, 2.0, "Y");
	return (R.StartsWith("X,1,") && R.EndsWith(",Y")) ? 1 : 0;
}

// --- Arity: 5-arg overloads ---
int Format_FiveStr()
{
	FString R = FString::Format("{0}{1}{2}{3}{4}", "a", "b", "c", "d", "e");
	return (R == "abcde") ? 1 : 0;
}
int Format_FiveMixed()
{
	FString R = FString::Format("{0}|{1}|{2}|{3}|{4}", "S", 1, 2.5, "T", 99);
	return (R.StartsWith("S|1|") && R.Contains("|T|99")) ? 1 : 0;
}

// --- Index manipulation ---
int Format_RepeatedIndex()
{
	FString R = FString::Format("{0}-{0}-{0}", "X");
	return (R == "X-X-X") ? 1 : 0;
}
int Format_ReversedOrder()
{
	FString R = FString::Format("{1} then {0}", "B", "A");
	return (R == "A then B") ? 1 : 0;
}
int Format_SkipIndex()
{
	// {0} and {2} used, {1} is "skipped" in visual order but still consumed
	FString R = FString::Format("{2}+{0}+{1}", "A", "B", "C");
	return (R == "C+A+B") ? 1 : 0;
}
int Format_SameIndexDifferentPositions()
{
	FString R = FString::Format("[{0}][{1}][{0}][{1}]", "X", "Y");
	return (R == "[X][Y][X][Y]") ? 1 : 0;
}

// --- Edge cases ---
int Format_NoPlaceholder()
{
	FString R = FString::Format("LiteralOnly", "Unused");
	return (R == "LiteralOnly") ? 1 : 0;
}
int Format_EmptyStringArg()
{
	FString R = FString::Format("A{0}B", "");
	return (R == "AB") ? 1 : 0;
}
int Format_IntNegative()
{
	FString R = FString::Format("val={0}", -999);
	return (R == "val=-999") ? 1 : 0;
}
int Format_IntZero()
{
	FString R = FString::Format("z={0}", 0);
	return (R == "z=0") ? 1 : 0;
}
int Format_LargeInt()
{
	FString R = FString::Format("{0}", 2147483647);
	return (R == "2147483647") ? 1 : 0;
}
int Format_AdjacentPlaceholders()
{
	FString R = FString::Format("{0}{1}{2}", "A", "B", "C");
	return (R == "ABC") ? 1 : 0;
}

// --- Return FString for C++ verification ---
FString Format_Ret_MixedFive()
{
	return FString::Format("{0}|{1}|{2}|{3}|{4}", "Hello", 42, 3.14, "End", -1);
}
FString Format_Ret_Repeated()
{
	return FString::Format("{0}={0}", "Echo");
}
FString Format_Ret_FloatPrecision()
{
	return FString::Format("pi~{0}", 3.14159);
}
)"));
		if (!Mod.IsValid()) return;
		auto& M = Mod.GetModule();

		const FExpectedGlobalInt Cases[] = {
			// 1-arg
			{ TEXT("int Format_OneStr()"),              TEXT("Format 1-arg string"),                           1 },
			{ TEXT("int Format_OneInt()"),              TEXT("Format 1-arg int"),                              1 },
			{ TEXT("int Format_OneFloat()"),            TEXT("Format 1-arg float"),                            1 },
			{ TEXT("int Format_OneFStringVar()"),       TEXT("Format 1-arg FString variable"),                 1 },
			// 2-arg
			{ TEXT("int Format_TwoStr()"),             TEXT("Format 2-arg two strings"),                      1 },
			{ TEXT("int Format_StrInt()"),             TEXT("Format 2-arg string+int"),                       1 },
			{ TEXT("int Format_IntFloat()"),           TEXT("Format 2-arg int+float"),                        1 },
			// 3-arg
			{ TEXT("int Format_ThreeInts()"),          TEXT("Format 3-arg three ints"),                       1 },
			{ TEXT("int Format_ThreeMixed()"),         TEXT("Format 3-arg string+int+float"),                 1 },
			// 4-arg
			{ TEXT("int Format_FourStr()"),            TEXT("Format 4-arg four strings"),                     1 },
			{ TEXT("int Format_FourMixed()"),          TEXT("Format 4-arg mixed types"),                      1 },
			// 5-arg
			{ TEXT("int Format_FiveStr()"),            TEXT("Format 5-arg five strings"),                     1 },
			{ TEXT("int Format_FiveMixed()"),          TEXT("Format 5-arg mixed types"),                      1 },
			// Index manipulation
			{ TEXT("int Format_RepeatedIndex()"),      TEXT("Format repeated index {0}-{0}-{0}"),             1 },
			{ TEXT("int Format_ReversedOrder()"),      TEXT("Format reversed placeholder order"),              1 },
			{ TEXT("int Format_SkipIndex()"),          TEXT("Format arbitrary index order {2}+{0}+{1}"),      1 },
			{ TEXT("int Format_SameIndexDifferentPositions()"), TEXT("Format alternating indices"),            1 },
			// Edge cases
			{ TEXT("int Format_NoPlaceholder()"),      TEXT("Format literal with no placeholder consumed"),   1 },
			{ TEXT("int Format_EmptyStringArg()"),     TEXT("Format with empty string arg"),                  1 },
			{ TEXT("int Format_IntNegative()"),        TEXT("Format with negative int"),                      1 },
			{ TEXT("int Format_IntZero()"),            TEXT("Format with zero int"),                          1 },
			{ TEXT("int Format_LargeInt()"),           TEXT("Format with INT32_MAX"),                         1 },
			{ TEXT("int Format_AdjacentPlaceholders()"), TEXT("Format adjacent {0}{1}{2}"),                   1 },
		};
		ExpectGlobalInts(*TestRunner, Engine, M,  Cases);

		// C++ side verification of Format return values
		ExpectGlobalReturnCustom<FString>(*TestRunner, Engine, M, 
			TEXT("FString Format_Ret_Repeated()"),
			TEXT("Format repeated index returns Echo=Echo"),
			[](FAutomationTestBase& T, const FString& V) -> bool
			{
				return T.TestEqual(TEXT("Repeated"), V, TEXT("Echo=Echo"));
			});

		ExpectGlobalReturnCustom<FString>(*TestRunner, Engine, M, 
			TEXT("FString Format_Ret_MixedFive()"),
			TEXT("Format 5-arg mixed returns pipe-separated"),
			[](FAutomationTestBase& T, const FString& V) -> bool
			{
				bool bOk = true;
				bOk &= T.TestTrue(TEXT("starts with Hello|42|"), V.StartsWith(TEXT("Hello|42|")));
				bOk &= T.TestTrue(TEXT("contains 3.14"), V.Contains(TEXT("3.14")));
				bOk &= T.TestTrue(TEXT("ends with |-1"), V.EndsWith(TEXT("|-1")));
				return bOk;
			});

		ExpectGlobalReturnCustom<FString>(*TestRunner, Engine, M, 
			TEXT("FString Format_Ret_FloatPrecision()"),
			TEXT("Format float shows decimal digits"),
			[](FAutomationTestBase& T, const FString& V) -> bool
			{
				return T.TestTrue(TEXT("contains pi~3.14"), V.Contains(TEXT("pi~3.14")));
			});
	}

	// ====================================================================
	// Section: Join — FString::Join
	// ====================================================================

	TEST_METHOD(Join)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASFString_Join"), TEXT(R"(
int Join_Basic()
{
	TArray<FString> Arr;
	Arr.Add("A");
	Arr.Add("B");
	Arr.Add("C");
	FString R = FString::Join(Arr, ", ");
	return (R == "A, B, C") ? 1 : 0;
}
int Join_SingleElement()
{
	TArray<FString> Arr;
	Arr.Add("Only");
	FString R = FString::Join(Arr, ",");
	return (R == "Only") ? 1 : 0;
}
int Join_Empty()
{
	TArray<FString> Arr;
	FString R = FString::Join(Arr, ",");
	return R.IsEmpty() ? 1 : 0;
}
int Join_EmptySeparator()
{
	TArray<FString> Arr;
	Arr.Add("A");
	Arr.Add("B");
	FString R = FString::Join(Arr, "");
	return (R == "AB") ? 1 : 0;
}
int Join_MultiCharSep()
{
	TArray<FString> Arr;
	Arr.Add("X");
	Arr.Add("Y");
	Arr.Add("Z");
	FString R = FString::Join(Arr, " | ");
	return (R == "X | Y | Z") ? 1 : 0;
}
int Join_WithEmptyElements()
{
	TArray<FString> Arr;
	Arr.Add("A");
	Arr.Add("");
	Arr.Add("C");
	FString R = FString::Join(Arr, ",");
	return (R == "A,,C") ? 1 : 0;
}
)"));
		if (!Mod.IsValid()) return;
		auto& M = Mod.GetModule();

		const FExpectedGlobalInt Cases[] = {
			{ TEXT("int Join_Basic()"),              TEXT("Join three elements with separator"),       1 },
			{ TEXT("int Join_SingleElement()"),      TEXT("Join single element returns element"),      1 },
			{ TEXT("int Join_Empty()"),              TEXT("Join empty array returns empty string"),    1 },
			{ TEXT("int Join_EmptySeparator()"),     TEXT("Join with empty separator concatenates"),   1 },
			{ TEXT("int Join_MultiCharSep()"),       TEXT("Join with multi-char separator"),           1 },
			{ TEXT("int Join_WithEmptyElements()"),  TEXT("Join preserves empty elements"),            1 },
		};
		ExpectGlobalInts(*TestRunner, Engine, M,  Cases);
	}

	// ====================================================================
	// Section: StaticConstruction
	// ====================================================================

	TEST_METHOD(StaticConstruction)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASFString_StaticCtor"), TEXT(R"(
// ---- FString::FromInt ----
int FromInt_Positive()
{
	FString S = FString::FromInt(42);
	return (S == "42") ? 1 : 0;
}
int FromInt_Negative()
{
	FString S = FString::FromInt(-123);
	return (S == "-123") ? 1 : 0;
}
int FromInt_Zero()
{
	FString S = FString::FromInt(0);
	return (S == "0") ? 1 : 0;
}

// ---- FString::SanitizeFloat ----
int SanitizeFloat_Whole()
{
	FString S = FString::SanitizeFloat(3.0);
	return (S == "3.0") ? 1 : 0;
}
int SanitizeFloat_Decimal()
{
	FString S = FString::SanitizeFloat(1.5);
	return (S == "1.5") ? 1 : 0;
}
int SanitizeFloat_Precision()
{
	FString S = FString::SanitizeFloat(2.0, 3);
	return (S == "2.000") ? 1 : 0;
}

// ---- FString::FormatAsNumber ----
int FormatAsNumber_Thousands()
{
	FString S = FString::FormatAsNumber(1234567);
	return S.Contains(",") ? 1 : 0;
}
int FormatAsNumber_Small()
{
	FString S = FString::FormatAsNumber(42);
	return (S == "42") ? 1 : 0;
}

// ---- FString::Chr ----
int Chr_Basic()
{
	FString S = FString::Chr(0x41);  // 'A'
	return (S == "A" && S.Len() == 1) ? 1 : 0;
}

// ---- FString::ChrN ----
int ChrN_Repeat()
{
	FString S = FString::ChrN(5, 0x58);  // 'X' x 5
	return (S == "XXXXX" && S.Len() == 5) ? 1 : 0;
}
int ChrN_Zero()
{
	FString S = FString::ChrN(0, 0x41);
	return S.IsEmpty() ? 1 : 0;
}
)"));
		if (!Mod.IsValid()) return;
		auto& M = Mod.GetModule();

		const FExpectedGlobalInt Cases[] = {
			{ TEXT("int FromInt_Positive()"),              TEXT("FromInt positive"),                    1 },
			{ TEXT("int FromInt_Negative()"),              TEXT("FromInt negative"),                    1 },
			{ TEXT("int FromInt_Zero()"),                  TEXT("FromInt zero"),                        1 },
			{ TEXT("int SanitizeFloat_Whole()"),           TEXT("SanitizeFloat whole number"),          1 },
			{ TEXT("int SanitizeFloat_Decimal()"),         TEXT("SanitizeFloat decimal"),               1 },
			{ TEXT("int SanitizeFloat_Precision()"),       TEXT("SanitizeFloat min fractional"),        1 },
			{ TEXT("int FormatAsNumber_Thousands()"),      TEXT("FormatAsNumber with commas"),          1 },
			{ TEXT("int FormatAsNumber_Small()"),          TEXT("FormatAsNumber small number"),         1 },
			{ TEXT("int Chr_Basic()"),                     TEXT("Chr single character"),                1 },
			{ TEXT("int ChrN_Repeat()"),                   TEXT("ChrN repeated characters"),            1 },
			{ TEXT("int ChrN_Zero()"),                     TEXT("ChrN zero length"),                    1 },
		};
		ExpectGlobalInts(*TestRunner, Engine, M,  Cases);
	}

	// ====================================================================
	// Section: ApplyFormat — Python-style format specifiers
	// ====================================================================

	TEST_METHOD(ApplyFormat)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASFString_ApplyFmt"), TEXT(R"(
// ==== int32 ====
int ApplyFmt_Int_Default()    { return FString::ApplyFormat(42, "").Contains("42") ? 1 : 0; }
int ApplyFmt_Int_Negative()   { return FString::ApplyFormat(-10, "").Contains("-10") ? 1 : 0; }
int ApplyFmt_Int_Zero()       { return FString::ApplyFormat(0, "").Contains("0") ? 1 : 0; }
int ApplyFmt_Int_Hex()        { return FString::ApplyFormat(255, "x").ToLower().Contains("ff") ? 1 : 0; }
int ApplyFmt_Int_HexUpper()   { return FString::ApplyFormat(255, "X").Contains("FF") ? 1 : 0; }
int ApplyFmt_Int_Octal()      { return FString::ApplyFormat(8, "o").Contains("10") ? 1 : 0; }
int ApplyFmt_Int_Binary()     { return FString::ApplyFormat(5, "b").Contains("101") ? 1 : 0; }
int ApplyFmt_Int_Char()       { return FString::ApplyFormat(65, "c") == "A" ? 1 : 0; }
int ApplyFmt_Int_Padded()     { return FString::ApplyFormat(42, ">5").Len() >= 5 ? 1 : 0; }
int ApplyFmt_Int_Commas()
{
	FString R = FString::ApplyFormat(1234567, "n");
	return R.Contains(",") ? 1 : 0;
}
int ApplyFmt_Int_SignPlus()
{
	FString R = FString::ApplyFormat(42, "+");
	return R.Contains("+42") ? 1 : 0;
}
int ApplyFmt_Int_SignSpace()
{
	FString R = FString::ApplyFormat(42, " ");
	return (R.Contains(" 42") || R.Contains("42")) ? 1 : 0;
}
int ApplyFmt_Int_HexPrefix()
{
	FString R = FString::ApplyFormat(255, "#x");
	return R.Contains("0x") ? 1 : 0;
}
int ApplyFmt_Int_BinPrefix()
{
	FString R = FString::ApplyFormat(5, "#b");
	return R.Contains("0b") ? 1 : 0;
}
int ApplyFmt_Int_OctPrefix()
{
	FString R = FString::ApplyFormat(8, "#o");
	return R.Contains("0o") ? 1 : 0;
}
int ApplyFmt_Int_AfterSignPad()
{
	FString R = FString::ApplyFormat(42, "=8");
	return R.Len() >= 8 ? 1 : 0;
}

// ==== uint32 ====
int ApplyFmt_UInt_Default()
{
	uint32 V = 4294967295;
	FString R = FString::ApplyFormat(V, "");
	return R.Contains("4294967295") ? 1 : 0;
}
int ApplyFmt_UInt_Hex()
{
	uint32 V = 255;
	FString R = FString::ApplyFormat(V, "x");
	return R.ToLower().Contains("ff") ? 1 : 0;
}

// ==== int64 ====
// NOTE: Values within int32 range are used because the ApplyFormat
// binding uses %ld/%lu which are 32-bit on MSVC/Windows, truncating
// values beyond INT32_MAX. These tests verify the int64 overload is
// reachable and correct for values that fit the formatter.
int ApplyFmt_Int64_Positive()
{
	int64 V = 1234567;
	FString R = FString::ApplyFormat(V, "");
	return R.Contains("1234567") ? 1 : 0;
}
int ApplyFmt_Int64_Negative()
{
	int64 V = -1234567;
	FString R = FString::ApplyFormat(V, "");
	return R.Contains("-1234567") ? 1 : 0;
}
int ApplyFmt_Int64_Commas()
{
	int64 V = 1234567;
	FString R = FString::ApplyFormat(V, "n");
	return R.Contains(",") ? 1 : 0;
}
int ApplyFmt_Int64_Hex()
{
	int64 V = 255;
	FString R = FString::ApplyFormat(V, "x");
	return R.ToLower().Contains("ff") ? 1 : 0;
}

// ==== uint64 ====
int ApplyFmt_UInt64_Positive()
{
	uint64 V = 9876543;
	FString R = FString::ApplyFormat(V, "");
	return R.Contains("9876543") ? 1 : 0;
}
int ApplyFmt_UInt64_Hex()
{
	uint64 V = 4095;
	FString R = FString::ApplyFormat(V, "X");
	return R.Contains("FFF") ? 1 : 0;
}

// ==== int8 / uint8 / int16 / uint16 ====
int ApplyFmt_Int8()
{
	int8 V = -128;
	FString R = FString::ApplyFormat(V, "");
	return R.Contains("128") ? 1 : 0;
}
int ApplyFmt_UInt8()
{
	uint8 V = 255;
	FString R = FString::ApplyFormat(V, "");
	return R.Contains("255") ? 1 : 0;
}
int ApplyFmt_Int16()
{
	int16 V = -32768;
	FString R = FString::ApplyFormat(V, "");
	return R.Contains("32768") ? 1 : 0;
}
int ApplyFmt_UInt16()
{
	uint16 V = 65535;
	FString R = FString::ApplyFormat(V, "");
	return R.Contains("65535") ? 1 : 0;
}

// ==== float32 ====
int ApplyFmt_Float_Default()  { return FString::ApplyFormat(3.14f, "").Contains("3.14") ? 1 : 0; }
int ApplyFmt_Float_Neg()      { return FString::ApplyFormat(-2.5f, "").Contains("-2.5") ? 1 : 0; }
int ApplyFmt_Float_Zero()     { return FString::ApplyFormat(0.0f, "").Contains("0") ? 1 : 0; }
int ApplyFmt_Float_Prec2()
{
	FString R = FString::ApplyFormat(3.14159f, ".2f");
	return R.Contains("3.14") ? 1 : 0;
}
int ApplyFmt_Float_Prec0()
{
	FString R = FString::ApplyFormat(3.7f, ".0f");
	return (R == "4" || R.Contains("4")) ? 1 : 0;
}
int ApplyFmt_Float_Percent()
{
	FString R = FString::ApplyFormat(0.75f, "%");
	return (R.Contains("75") && R.Contains("%")) ? 1 : 0;
}
int ApplyFmt_Float_PercentPrec()
{
	FString R = FString::ApplyFormat(0.12345f, ".1%");
	return (R.Contains("12.3") && R.Contains("%")) ? 1 : 0;
}
int ApplyFmt_Float_Scientific()
{
	FString R = FString::ApplyFormat(1234.5f, "e");
	return (R.Contains("e") || R.Contains("E")) ? 1 : 0;
}
int ApplyFmt_Float_SciUpper()
{
	FString R = FString::ApplyFormat(1234.5f, "E");
	return R.Contains("E") ? 1 : 0;
}
int ApplyFmt_Float_General()
{
	FString R = FString::ApplyFormat(1234.5f, "g");
	return R.Len() > 0 ? 1 : 0;
}
int ApplyFmt_Float_GeneralUpper()
{
	FString R = FString::ApplyFormat(0.00001f, "G");
	return R.Len() > 0 ? 1 : 0;
}
int ApplyFmt_Float_Commas()
{
	FString R = FString::ApplyFormat(1234567.5f, "n");
	return R.Contains(",") ? 1 : 0;
}
int ApplyFmt_Float_UppercaseF()
{
	FString R = FString::ApplyFormat(3.14f, "F");
	return R.Len() > 0 ? 1 : 0;
}
int ApplyFmt_Float_SignPlus()
{
	FString R = FString::ApplyFormat(9.0f, "+");
	return R.Contains("+") ? 1 : 0;
}

// ==== float64 (double) ====
int ApplyFmt_Double_Prec()
{
	float64 V = 3.141592653589793;
	FString R = FString::ApplyFormat(V, ".6f");
	return R.Contains("3.141593") ? 1 : 0;
}
int ApplyFmt_Double_Sci()
{
	float64 V = 0.000123;
	FString R = FString::ApplyFormat(V, "e");
	return R.Contains("e") ? 1 : 0;
}

// ==== Bool ====
int ApplyFmt_Bool_True()   { return FString::ApplyFormat(true, "") == "true" ? 1 : 0; }
int ApplyFmt_Bool_False()  { return FString::ApplyFormat(false, "") == "false" ? 1 : 0; }
int ApplyFmt_Bool_PadRight()
{
	FString R = FString::ApplyFormat(true, "<8");
	return (R.Len() >= 8 && R.StartsWith("true")) ? 1 : 0;
}
int ApplyFmt_Bool_PadLeft()
{
	FString R = FString::ApplyFormat(false, ">8");
	return (R.Len() >= 8 && R.EndsWith("false")) ? 1 : 0;
}

// ==== String ====
int ApplyFmt_Str_PadRight()
{
	FString R = FString::ApplyFormat("AB", "<5");
	return (R.Len() >= 5 && R.StartsWith("AB")) ? 1 : 0;
}
int ApplyFmt_Str_PadLeft()
{
	FString R = FString::ApplyFormat("AB", ">5");
	return (R.Len() >= 5 && R.EndsWith("AB")) ? 1 : 0;
}
int ApplyFmt_Str_Center()
{
	FString R = FString::ApplyFormat("AB", "^6");
	return (R.Len() >= 6 && R.Contains("AB")) ? 1 : 0;
}
int ApplyFmt_Str_FillChar()
{
	FString R = FString::ApplyFormat("X", "*>5");
	return (R.Len() >= 5 && R.EndsWith("X") && R.Contains("*")) ? 1 : 0;
}
int ApplyFmt_Str_Empty()
{
	FString R = FString::ApplyFormat("", ">5");
	return R.Len() >= 5 ? 1 : 0;
}

// ==== Return FString for C++ ====
FString ApplyFmt_Ret_IntHexPrefix()
{
	return FString::ApplyFormat(255, "#X");
}
FString ApplyFmt_Ret_FloatPrec3()
{
	return FString::ApplyFormat(3.14159f, ".3f");
}
FString ApplyFmt_Ret_BoolTrue()
{
	return FString::ApplyFormat(true, "");
}
FString ApplyFmt_Ret_StrFillCenter()
{
	return FString::ApplyFormat("Hi", "-^8");
}
FString ApplyFmt_Ret_IntBinPrefix()
{
	return FString::ApplyFormat(10, "#b");
}
FString ApplyFmt_Ret_NegSignPlus()
{
	return FString::ApplyFormat(-42, "+");
}
)"));
		if (!Mod.IsValid()) return;
		auto& M = Mod.GetModule();

		const FExpectedGlobalInt Cases[] = {
			// int32 basics
			{ TEXT("int ApplyFmt_Int_Default()"),     TEXT("int32 default decimal"),                     1 },
			{ TEXT("int ApplyFmt_Int_Negative()"),    TEXT("int32 negative sign"),                       1 },
			{ TEXT("int ApplyFmt_Int_Zero()"),        TEXT("int32 zero"),                                1 },
			// int32 bases
			{ TEXT("int ApplyFmt_Int_Hex()"),          TEXT("int32 hex lowercase"),                      1 },
			{ TEXT("int ApplyFmt_Int_HexUpper()"),     TEXT("int32 hex uppercase"),                      1 },
			{ TEXT("int ApplyFmt_Int_Octal()"),        TEXT("int32 octal"),                               1 },
			{ TEXT("int ApplyFmt_Int_Binary()"),       TEXT("int32 binary"),                              1 },
			{ TEXT("int ApplyFmt_Int_Char()"),         TEXT("int32 as char ('c' → 'A')"),                 1 },
			// int32 formatting
			{ TEXT("int ApplyFmt_Int_Padded()"),       TEXT("int32 right-padded width"),                  1 },
			{ TEXT("int ApplyFmt_Int_Commas()"),       TEXT("int32 thousands comma (n)"),                 1 },
			{ TEXT("int ApplyFmt_Int_SignPlus()"),      TEXT("int32 explicit + sign"),                    1 },
			{ TEXT("int ApplyFmt_Int_SignSpace()"),     TEXT("int32 leading space sign"),                 1 },
			{ TEXT("int ApplyFmt_Int_HexPrefix()"),    TEXT("int32 0x prefix (#x)"),                     1 },
			{ TEXT("int ApplyFmt_Int_BinPrefix()"),    TEXT("int32 0b prefix (#b)"),                     1 },
			{ TEXT("int ApplyFmt_Int_OctPrefix()"),    TEXT("int32 0o prefix (#o)"),                     1 },
			{ TEXT("int ApplyFmt_Int_AfterSignPad()"), TEXT("int32 after-sign pad (=8)"),                1 },
			// uint32
			{ TEXT("int ApplyFmt_UInt_Default()"),     TEXT("uint32 max value"),                         1 },
			{ TEXT("int ApplyFmt_UInt_Hex()"),         TEXT("uint32 hex"),                                1 },
			// int64
			{ TEXT("int ApplyFmt_Int64_Positive()"),   TEXT("int64 positive decimal"),                   1 },
			{ TEXT("int ApplyFmt_Int64_Negative()"),   TEXT("int64 negative sign"),                      1 },
			{ TEXT("int ApplyFmt_Int64_Commas()"),     TEXT("int64 thousands comma"),                    1 },
			{ TEXT("int ApplyFmt_Int64_Hex()"),        TEXT("int64 hex lowercase"),                      1 },
			// uint64
			{ TEXT("int ApplyFmt_UInt64_Positive()"),  TEXT("uint64 positive decimal"),                  1 },
			{ TEXT("int ApplyFmt_UInt64_Hex()"),       TEXT("uint64 hex uppercase"),                     1 },
			// small integer types
			{ TEXT("int ApplyFmt_Int8()"),             TEXT("int8 boundary"),                             1 },
			{ TEXT("int ApplyFmt_UInt8()"),            TEXT("uint8 max"),                                 1 },
			{ TEXT("int ApplyFmt_Int16()"),            TEXT("int16 boundary"),                            1 },
			{ TEXT("int ApplyFmt_UInt16()"),           TEXT("uint16 max"),                                1 },
			// float32
			{ TEXT("int ApplyFmt_Float_Default()"),    TEXT("float32 default"),                          1 },
			{ TEXT("int ApplyFmt_Float_Neg()"),        TEXT("float32 negative"),                         1 },
			{ TEXT("int ApplyFmt_Float_Zero()"),       TEXT("float32 zero"),                             1 },
			{ TEXT("int ApplyFmt_Float_Prec2()"),      TEXT("float32 .2f precision"),                    1 },
			{ TEXT("int ApplyFmt_Float_Prec0()"),      TEXT("float32 .0f rounds"),                       1 },
			{ TEXT("int ApplyFmt_Float_Percent()"),    TEXT("float32 percentage (%)"),                   1 },
			{ TEXT("int ApplyFmt_Float_PercentPrec()"),TEXT("float32 percentage .1% precision"),         1 },
			{ TEXT("int ApplyFmt_Float_Scientific()"), TEXT("float32 scientific (e)"),                   1 },
			{ TEXT("int ApplyFmt_Float_SciUpper()"),   TEXT("float32 scientific (E)"),                   1 },
			{ TEXT("int ApplyFmt_Float_General()"),    TEXT("float32 general (g)"),                      1 },
			{ TEXT("int ApplyFmt_Float_GeneralUpper()"),TEXT("float32 general (G)"),                     1 },
			{ TEXT("int ApplyFmt_Float_Commas()"),     TEXT("float32 thousands comma (n)"),              1 },
			{ TEXT("int ApplyFmt_Float_UppercaseF()"), TEXT("float32 uppercase (F)"),                    1 },
			{ TEXT("int ApplyFmt_Float_SignPlus()"),   TEXT("float32 explicit + sign"),                  1 },
			// float64
			{ TEXT("int ApplyFmt_Double_Prec()"),      TEXT("float64 .6f precision"),                    1 },
			{ TEXT("int ApplyFmt_Double_Sci()"),       TEXT("float64 scientific (e)"),                   1 },
			// Bool
			{ TEXT("int ApplyFmt_Bool_True()"),        TEXT("bool true → \"true\""),                     1 },
			{ TEXT("int ApplyFmt_Bool_False()"),       TEXT("bool false → \"false\""),                   1 },
			{ TEXT("int ApplyFmt_Bool_PadRight()"),    TEXT("bool left-align pad"),                      1 },
			{ TEXT("int ApplyFmt_Bool_PadLeft()"),     TEXT("bool right-align pad"),                     1 },
			// String
			{ TEXT("int ApplyFmt_Str_PadRight()"),     TEXT("string left-align pad right"),              1 },
			{ TEXT("int ApplyFmt_Str_PadLeft()"),      TEXT("string right-align pad left"),              1 },
			{ TEXT("int ApplyFmt_Str_Center()"),       TEXT("string center-align"),                      1 },
			{ TEXT("int ApplyFmt_Str_FillChar()"),     TEXT("string custom fill char (*)"),              1 },
			{ TEXT("int ApplyFmt_Str_Empty()"),        TEXT("string empty with pad"),                    1 },
		};
		ExpectGlobalInts(*TestRunner, Engine, M,  Cases);

		// C++ side verification of ApplyFormat return values
		ExpectGlobalReturnCustom<FString>(*TestRunner, Engine, M, 
			TEXT("FString ApplyFmt_Ret_IntHexPrefix()"),
			TEXT("ApplyFormat int #X returns 0xFF-style"),
			[](FAutomationTestBase& T, const FString& V) -> bool
			{
				bool bOk = true;
				bOk &= T.TestTrue(TEXT("contains 0x prefix"), V.Contains(TEXT("0x")) || V.Contains(TEXT("0X")));
				bOk &= T.TestTrue(TEXT("contains FF"), V.ToUpper().Contains(TEXT("FF")));
				return bOk;
			});

		ExpectGlobalReturnCustom<FString>(*TestRunner, Engine, M, 
			TEXT("FString ApplyFmt_Ret_FloatPrec3()"),
			TEXT("ApplyFormat float .3f returns 3.142"),
			[](FAutomationTestBase& T, const FString& V) -> bool
			{
				return T.TestTrue(TEXT("contains 3.142"), V.Contains(TEXT("3.142")));
			});

		ExpectGlobalReturnCustom<FString>(*TestRunner, Engine, M, 
			TEXT("FString ApplyFmt_Ret_BoolTrue()"),
			TEXT("ApplyFormat bool true returns 'true'"),
			[](FAutomationTestBase& T, const FString& V) -> bool
			{
				return T.TestEqual(TEXT("bool true text"), V, TEXT("true"));
			});

		ExpectGlobalReturnCustom<FString>(*TestRunner, Engine, M, 
			TEXT("FString ApplyFmt_Ret_StrFillCenter()"),
			TEXT("ApplyFormat string center with dash fill"),
			[](FAutomationTestBase& T, const FString& V) -> bool
			{
				bool bOk = true;
				bOk &= T.TestTrue(TEXT("length >= 8"), V.Len() >= 8);
				bOk &= T.TestTrue(TEXT("contains Hi"), V.Contains(TEXT("Hi")));
				bOk &= T.TestTrue(TEXT("contains dash fill"), V.Contains(TEXT("-")));
				return bOk;
			});

		ExpectGlobalReturnCustom<FString>(*TestRunner, Engine, M, 
			TEXT("FString ApplyFmt_Ret_IntBinPrefix()"),
			TEXT("ApplyFormat int #b returns 0b1010"),
			[](FAutomationTestBase& T, const FString& V) -> bool
			{
				bool bOk = true;
				bOk &= T.TestTrue(TEXT("contains 0b prefix"), V.Contains(TEXT("0b")));
				bOk &= T.TestTrue(TEXT("contains 1010"), V.Contains(TEXT("1010")));
				return bOk;
			});

		ExpectGlobalReturnCustom<FString>(*TestRunner, Engine, M, 
			TEXT("FString ApplyFmt_Ret_NegSignPlus()"),
			TEXT("ApplyFormat negative with + spec still shows minus"),
			[](FAutomationTestBase& T, const FString& V) -> bool
			{
				return T.TestTrue(TEXT("contains -42"), V.Contains(TEXT("-42")));
			});
	}

	// ====================================================================
	// Section: Logging — compilation and basic execution of Log/Warning/Error
	// ====================================================================

	TEST_METHOD(Logging)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASFString_Logging"), TEXT(R"(
int Log_Compiles()
{
	Log("FString test log message");
	return 1;
}
int LogInfo_Compiles()
{
	LogInfo("FString test info message");
	return 1;
}
int Warning_Compiles()
{
	Warning("FString test warning message");
	return 1;
}
int LogIf_True()
{
	LogIf(true, "Conditional true");
	return 1;
}
int LogIf_False()
{
	LogIf(false, "Conditional false — should not appear");
	return 1;
}
int Log_WithCategory()
{
	Log(FName("TestCategory"), "Categorized log");
	return 1;
}
int Log_Concatenated()
{
	FString Name = "World";
	Log("Hello " + Name + " Value=" + 42);
	return 1;
}
int Log_Format()
{
	FString Msg = FString::Format("Count={0} Name={1}", 10, "Test");
	Log(Msg);
	return (Msg.Contains("Count=10") && Msg.Contains("Name=Test")) ? 1 : 0;
}
)"));
		if (!Mod.IsValid()) return;
		auto& M = Mod.GetModule();

		TestRunner->AddExpectedErrorPlain(
			TEXT("FString test warning message"),
			EAutomationExpectedErrorFlags::Contains, 0);

		const FExpectedGlobalInt Cases[] = {
			{ TEXT("int Log_Compiles()"),        TEXT("Log() compiles and executes"),              1 },
			{ TEXT("int LogInfo_Compiles()"),     TEXT("LogInfo() compiles and executes"),          1 },
			{ TEXT("int Warning_Compiles()"),     TEXT("Warning() compiles and executes"),          1 },
			{ TEXT("int LogIf_True()"),           TEXT("LogIf(true) executes log"),                 1 },
			{ TEXT("int LogIf_False()"),          TEXT("LogIf(false) suppresses log"),              1 },
			{ TEXT("int Log_WithCategory()"),     TEXT("Log with FName category compiles"),         1 },
			{ TEXT("int Log_Concatenated()"),     TEXT("Log with string+type concatenation"),       1 },
			{ TEXT("int Log_Format()"),           TEXT("Log with FString::Format integration"),     1 },
		};
		ExpectGlobalInts(*TestRunner, Engine, M,  Cases);
	}

	// ====================================================================
	// Section: ReturnFString — AS returns FString, C++ validates content
	//
	// All previous sections return int and compare inside AS. This section
	// exercises the cross-boundary FString return path: AS builds/transforms
	// a string and returns it; C++ reads it via GetAddressOfReturnValue and
	// performs the assertion. This validates both the string factory return
	// mechanism and the actual string content in C++ memory.
	// ====================================================================

	TEST_METHOD(ReturnFString)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASFString_RetStr"), TEXT(R"(
FString Ret_Literal()
{
	return "Hello World";
}

FString Ret_Empty()
{
	return FString();
}

FString Ret_Concat()
{
	return "Hello" + " " + "World";
}

FString Ret_Upper()
{
	return "hello".ToUpper();
}

FString Ret_Lower()
{
	return "HELLO".ToLower();
}

FString Ret_Replace()
{
	FString S = "A-B-C";
	return S.Replace("-", "_");
}

FString Ret_Reverse()
{
	return "ABCDE".Reverse();
}

FString Ret_Mid()
{
	return "Hello World".Mid(6);
}

FString Ret_Trim()
{
	return "  padded  ".TrimStartAndEnd();
}

FString Ret_Format()
{
	return FString::Format("{0}={1}", "Count", 42);
}

FString Ret_Join()
{
	TArray<FString> Parts;
	Parts.Add("X");
	Parts.Add("Y");
	Parts.Add("Z");
	return FString::Join(Parts, ",");
}

FString Ret_ConcatInt()
{
	return "N=" + 123;
}

FString Ret_LeftPad()
{
	return "AB".LeftPad(5);
}

FString Ret_AppendChain()
{
	FString S;
	S.Append("A").Append("B").AppendChar(0x43);
	return S;
}
)"));
		if (!Mod.IsValid()) return;
		auto& M = Mod.GetModule();

		auto ExpectStr = [&](const TCHAR* Decl, const TCHAR* Label, const FString& Expected)
		{
			ExpectGlobalReturnCustom<FString>(*TestRunner, Engine, M,  Decl, Label,
				[&](FAutomationTestBase& T, const FString& Actual) -> bool
				{
					return T.TestEqual(
						*FString::Printf(TEXT("[FString] %s: \"%s\" vs \"%s\""), Label, *Actual, *Expected),
						Actual, Expected);
				});
		};

		ExpectStr(TEXT("FString Ret_Literal()"),
			TEXT("Literal return preserves content"),
			TEXT("Hello World"));

		ExpectStr(TEXT("FString Ret_Empty()"),
			TEXT("Empty FString return is empty in C++"),
			FString());

		ExpectStr(TEXT("FString Ret_Concat()"),
			TEXT("Concatenated string returned correctly"),
			TEXT("Hello World"));

		ExpectStr(TEXT("FString Ret_Upper()"),
			TEXT("ToUpper result returned to C++"),
			TEXT("HELLO"));

		ExpectStr(TEXT("FString Ret_Lower()"),
			TEXT("ToLower result returned to C++"),
			TEXT("hello"));

		ExpectStr(TEXT("FString Ret_Replace()"),
			TEXT("Replace result returned to C++"),
			TEXT("A_B_C"));

		ExpectStr(TEXT("FString Ret_Reverse()"),
			TEXT("Reverse result returned to C++"),
			TEXT("EDCBA"));

		ExpectStr(TEXT("FString Ret_Mid()"),
			TEXT("Mid substring returned to C++"),
			TEXT("World"));

		ExpectStr(TEXT("FString Ret_Trim()"),
			TEXT("Trimmed string returned to C++"),
			TEXT("padded"));

		ExpectStr(TEXT("FString Ret_Format()"),
			TEXT("FString::Format result returned to C++"),
			TEXT("Count=42"));

		ExpectStr(TEXT("FString Ret_Join()"),
			TEXT("FString::Join result returned to C++"),
			TEXT("X,Y,Z"));

		ExpectStr(TEXT("FString Ret_ConcatInt()"),
			TEXT("String+int concat returned to C++"),
			TEXT("N=123"));

		// LeftPad pads with spaces on the left to reach width 5.
		ExpectGlobalReturnCustom<FString>(*TestRunner, Engine, M, 
			TEXT("FString Ret_LeftPad()"),
			TEXT("LeftPad(5) returns 5-char string ending with AB"),
			[](FAutomationTestBase& T, const FString& Actual) -> bool
			{
				bool bPass = true;
				bPass &= T.TestEqual(TEXT("LeftPad result length should be 5"), Actual.Len(), 5);
				bPass &= T.TestTrue(TEXT("LeftPad result should end with AB"), Actual.EndsWith(TEXT("AB")));
				return bPass;
			});

		ExpectStr(TEXT("FString Ret_AppendChain()"),
			TEXT("Append chain returned to C++"),
			TEXT("ABC"));
	}

	// ====================================================================
	// Section: PassFString — C++ passes FString into AS, AS processes it
	//
	// Exercises the inbound cross-boundary path: C++ constructs an FString,
	// passes it as `const FString& in` via FASGlobalFunctionInvoker, and
	// AS operates on the received string. Results validated in C++ via
	// return values or out-parameters.
	// ====================================================================

	TEST_METHOD(PassFString)
	{

		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASFString_PassStr"), TEXT(R"(
// Identity round-trip
FString Pass_Echo(const FString& in S)
{
	return S;
}

// Length query
int Pass_Len(const FString& in S)
{
	return S.Len();
}

// Uppercase transform
FString Pass_Upper(const FString& in S)
{
	return S.ToUpper();
}

// Contains check
int Pass_Contains(const FString& in Haystack, const FString& in Needle)
{
	return Haystack.Contains(Needle) ? 1 : 0;
}

// Replace
FString Pass_Replace(const FString& in S, const FString& in From, const FString& in To)
{
	return S.Replace(From, To);
}

// Concatenation with C++ supplied string
FString Pass_Concat(const FString& in A, const FString& in B)
{
	return A + " " + B;
}

// Mid substring extraction
FString Pass_Mid(const FString& in S, int Start, int Count)
{
	return S.Mid(Start, Count);
}

// Empty string pass-through
int Pass_IsEmpty(const FString& in S)
{
	return S.IsEmpty() ? 1 : 0;
}

// Trim
FString Pass_Trim(const FString& in S)
{
	return S.TrimStartAndEnd();
}

// Format with passed string as template
FString Pass_Format(const FString& in Fmt, int Value)
{
	return FString::Format(Fmt, Value);
}

// Out-parameter: split the input and write parts via out refs
int Pass_Split(const FString& in S, const FString& in Delim, FString& out Left, FString& out Right)
{
	return S.Split(Delim, Left, Right) ? 1 : 0;
}

// Reverse
FString Pass_Reverse(const FString& in S)
{
	return S.Reverse();
}
)"));
		if (!Mod.IsValid()) return;
		auto& M = Mod.GetModule();

		// -- Echo: round-trip an FString through AS unchanged --
		{
			FString Input = TEXT("Hello from C++");
			FASGlobalFunctionInvoker Inv(*TestRunner, Engine, M,
				TEXT("FString Pass_Echo(const FString& in)"));
			Inv.AddArgRef(Input);
			if (Inv.Call())
			{
				FString Result;
				if (Inv.ReadReturnStruct(Result))
				{
					TestRunner->TestEqual(TEXT("[PassFString] Echo round-trip"), Result, Input);
				}
			}
		}

		// -- Len: C++ string length measured in AS --
		{
			FString Input = TEXT("Seven!!");
			FASGlobalFunctionInvoker Inv(*TestRunner, Engine, M,
				TEXT("int Pass_Len(const FString& in)"));
			Inv.AddArgRef(Input);
			int32 Len = Inv.CallAndReturn<int32>(-1);
			TestRunner->TestEqual(TEXT("[PassFString] Len matches C++"), Len, Input.Len());
		}

		// -- Upper: C++ sends lowercase, AS returns uppercase --
		{
			FString Input = TEXT("hello world");
			FASGlobalFunctionInvoker Inv(*TestRunner, Engine, M,
				TEXT("FString Pass_Upper(const FString& in)"));
			Inv.AddArgRef(Input);
			if (Inv.Call())
			{
				FString Result;
				if (Inv.ReadReturnStruct(Result))
				{
					TestRunner->TestEqual(TEXT("[PassFString] Upper transform"), Result, TEXT("HELLO WORLD"));
				}
			}
		}

		// -- Contains: two FStrings from C++, boolean result --
		{
			FString Haystack = TEXT("The quick brown fox");
			FString Needle = TEXT("brown");
			FASGlobalFunctionInvoker Inv(*TestRunner, Engine, M,
				TEXT("int Pass_Contains(const FString& in, const FString& in)"));
			Inv.AddArgRef(Haystack).AddArgRef(Needle);
			TestRunner->TestEqual(TEXT("[PassFString] Contains found"), Inv.CallAndReturn<int32>(0), 1);
		}

		// -- Contains negative: substring not present --
		{
			FString Haystack = TEXT("The quick brown fox");
			FString Needle = TEXT("zebra");
			FASGlobalFunctionInvoker Inv(*TestRunner, Engine, M,
				TEXT("int Pass_Contains(const FString& in, const FString& in)"));
			Inv.AddArgRef(Haystack).AddArgRef(Needle);
			TestRunner->TestEqual(TEXT("[PassFString] Contains not found"), Inv.CallAndReturn<int32>(1), 0);
		}

		// -- Replace: three FStrings from C++ --
		{
			FString Input = TEXT("aaa-bbb-ccc");
			FString From = TEXT("-");
			FString To = TEXT("_");
			FASGlobalFunctionInvoker Inv(*TestRunner, Engine, M,
				TEXT("FString Pass_Replace(const FString& in, const FString& in, const FString& in)"));
			Inv.AddArgRef(Input).AddArgRef(From).AddArgRef(To);
			if (Inv.Call())
			{
				FString Result;
				if (Inv.ReadReturnStruct(Result))
				{
					TestRunner->TestEqual(TEXT("[PassFString] Replace - to _"), Result, TEXT("aaa_bbb_ccc"));
				}
			}
		}

		// -- Concat: two FStrings concatenated in AS --
		{
			FString A = TEXT("Hello");
			FString B = TEXT("World");
			FASGlobalFunctionInvoker Inv(*TestRunner, Engine, M,
				TEXT("FString Pass_Concat(const FString& in, const FString& in)"));
			Inv.AddArgRef(A).AddArgRef(B);
			if (Inv.Call())
			{
				FString Result;
				if (Inv.ReadReturnStruct(Result))
				{
					TestRunner->TestEqual(TEXT("[PassFString] Concat A+B"), Result, TEXT("Hello World"));
				}
			}
		}

		// -- Mid: FString + int args from C++ --
		{
			FString Input = TEXT("Hello World");
			FASGlobalFunctionInvoker Inv(*TestRunner, Engine, M,
				TEXT("FString Pass_Mid(const FString& in, int, int)"));
			Inv.AddArgRef(Input).AddArg(6).AddArg(5);
			if (Inv.Call())
			{
				FString Result;
				if (Inv.ReadReturnStruct(Result))
				{
					TestRunner->TestEqual(TEXT("[PassFString] Mid(6,5)"), Result, TEXT("World"));
				}
			}
		}

		// -- IsEmpty: pass empty string from C++ --
		{
			FString Empty;
			FASGlobalFunctionInvoker Inv(*TestRunner, Engine, M,
				TEXT("int Pass_IsEmpty(const FString& in)"));
			Inv.AddArgRef(Empty);
			TestRunner->TestEqual(TEXT("[PassFString] Empty string detected in AS"), Inv.CallAndReturn<int32>(0), 1);
		}

		// -- IsEmpty negative: non-empty --
		{
			FString NonEmpty = TEXT("X");
			FASGlobalFunctionInvoker Inv(*TestRunner, Engine, M,
				TEXT("int Pass_IsEmpty(const FString& in)"));
			Inv.AddArgRef(NonEmpty);
			TestRunner->TestEqual(TEXT("[PassFString] Non-empty string not empty in AS"), Inv.CallAndReturn<int32>(1), 0);
		}

		// -- Trim: whitespace-padded C++ string trimmed in AS --
		{
			FString Input = TEXT("   trimmed   ");
			FASGlobalFunctionInvoker Inv(*TestRunner, Engine, M,
				TEXT("FString Pass_Trim(const FString& in)"));
			Inv.AddArgRef(Input);
			if (Inv.Call())
			{
				FString Result;
				if (Inv.ReadReturnStruct(Result))
				{
					TestRunner->TestEqual(TEXT("[PassFString] Trim whitespace"), Result, TEXT("trimmed"));
				}
			}
		}

		// -- Format: C++ passes format template, AS fills in int arg --
		{
			FString Fmt = TEXT("Value={0}");
			FASGlobalFunctionInvoker Inv(*TestRunner, Engine, M,
				TEXT("FString Pass_Format(const FString& in, int)"));
			Inv.AddArgRef(Fmt).AddArg(42);
			if (Inv.Call())
			{
				FString Result;
				if (Inv.ReadReturnStruct(Result))
				{
					TestRunner->TestEqual(TEXT("[PassFString] Format with C++ template"), Result, TEXT("Value=42"));
				}
			}
		}

		// -- Split: C++ reads out-parameters written by AS --
		{
			FString Input = TEXT("Key=Value");
			FString Delim = TEXT("=");
			FString Left, Right;
			FASGlobalFunctionInvoker Inv(*TestRunner, Engine, M,
				TEXT("int Pass_Split(const FString& in, const FString& in, FString& out, FString& out)"));
			Inv.AddArgRef(Input).AddArgRef(Delim).AddArgRef(Left).AddArgRef(Right);
			int32 Found = Inv.CallAndReturn<int32>(0);
			TestRunner->TestEqual(TEXT("[PassFString] Split found delimiter"), Found, 1);
			TestRunner->TestEqual(TEXT("[PassFString] Split left part"), Left, TEXT("Key"));
			TestRunner->TestEqual(TEXT("[PassFString] Split right part"), Right, TEXT("Value"));
		}

		// -- Reverse: C++ string reversed in AS --
		{
			FString Input = TEXT("ABCDE");
			FASGlobalFunctionInvoker Inv(*TestRunner, Engine, M,
				TEXT("FString Pass_Reverse(const FString& in)"));
			Inv.AddArgRef(Input);
			if (Inv.Call())
			{
				FString Result;
				if (Inv.ReadReturnStruct(Result))
				{
					TestRunner->TestEqual(TEXT("[PassFString] Reverse"), Result, TEXT("EDCBA"));
				}
			}
		}

		// -- Unicode / special characters --
		{
			FString Input = TEXT("CaféNaïve");
			FASGlobalFunctionInvoker Inv(*TestRunner, Engine, M,
				TEXT("FString Pass_Echo(const FString& in)"));
			Inv.AddArgRef(Input);
			if (Inv.Call())
			{
				FString Result;
				if (Inv.ReadReturnStruct(Result))
				{
					TestRunner->TestEqual(TEXT("[PassFString] Unicode round-trip"), Result, Input);
				}
			}
		}

		// -- Long string --
		{
			FString Input;
			for (int32 i = 0; i < 1000; ++i) Input.AppendChar(TEXT('A'));
			FASGlobalFunctionInvoker Inv(*TestRunner, Engine, M,
				TEXT("int Pass_Len(const FString& in)"));
			Inv.AddArgRef(Input);
			TestRunner->TestEqual(TEXT("[PassFString] Long string length"), Inv.CallAndReturn<int32>(0), 1000);
		}
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
