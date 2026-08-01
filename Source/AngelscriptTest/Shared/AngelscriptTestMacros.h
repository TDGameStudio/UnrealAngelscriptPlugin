#pragma once

#include "AngelscriptTestUtilities.h"
#include "AngelscriptTestEnginePool.h"
#include "AngelscriptTestEngineHelper.h"
#include "AngelscriptTestEngine.h"
#include "Containers/StringConv.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"

#include <string>

// ============================================================================
// Angelscript Test Macros
// ============================================================================
//
// Engine Creation:
//   ASTEST_CREATE_ENGINE()       — shared engine, reset to clean state
//   ASTEST_GET_ENGINE()          — shared engine, no reset (use in TEST_METHOD)
//   ASTEST_CREATE_ENGINE_FULL()  — fresh isolated full engine
//   ASTEST_CREATE_ENGINE_NATIVE()— raw asIScriptEngine* from SDK
//
// Engine Reset:
//   ASTEST_RESET_ENGINE(Engine)  — reset shared engine (use in AFTER_ALL)
//
// CQTest standard pattern:
//   BEFORE_ALL()  { ASTEST_CREATE_ENGINE(); }
//   TEST_METHOD() { FAngelscriptEngine& Engine = ASTEST_GET_ENGINE(); ... }
//   AFTER_ALL()   { FAngelscriptEngine& E = ASTEST_GET_ENGINE(); ASTEST_RESET_ENGINE(E); }
// ============================================================================

// ============================================================================
// Inline AngelScript Source Macros
// ============================================================================

namespace AngelscriptTest
{
	inline bool IsInlineASWhitespace(const TCHAR Character)
	{
		return Character == TEXT(' ') || Character == TEXT('\t') || Character == TEXT('\r');
	}

	inline bool IsInlineASWhitespaceOnlyLine(const FString& Line)
	{
		for (const TCHAR Character : Line)
		{
			if (!IsInlineASWhitespace(Character))
			{
				return false;
			}
		}

		return true;
	}

	inline int32 CountInlineASIndent(const FString& Line)
	{
		int32 Count = 0;
		for (const TCHAR Character : Line)
		{
			if (!IsInlineASWhitespace(Character))
			{
				break;
			}

			++Count;
		}

		return Count;
	}

	inline FString NormalizeInlineASSource(const TCHAR* Source)
	{
		if (Source == nullptr)
		{
			return FString();
		}

		FString Text(Source);
		Text.ReplaceInline(TEXT("\r\n"), TEXT("\n"));
		Text.ReplaceInline(TEXT("\r"), TEXT("\n"));

		TArray<FString> Lines;
		Text.ParseIntoArrayLines(Lines, false);
		while (Lines.Num() > 0 && IsInlineASWhitespaceOnlyLine(Lines[0]))
		{
			Lines.RemoveAt(0, 1, EAllowShrinking::No);
		}
		while (Lines.Num() > 0 && IsInlineASWhitespaceOnlyLine(Lines.Last()))
		{
			Lines.Pop(EAllowShrinking::No);
		}

		int32 CommonIndent = MAX_int32;
		for (const FString& Line : Lines)
		{
			if (!IsInlineASWhitespaceOnlyLine(Line))
			{
				CommonIndent = FMath::Min(CommonIndent, CountInlineASIndent(Line));
			}
		}
		if (CommonIndent == MAX_int32)
		{
			CommonIndent = 0;
		}

		FString Result;
		for (int32 LineIndex = 0; LineIndex < Lines.Num(); ++LineIndex)
		{
			FString Line = Lines[LineIndex];
			if (!IsInlineASWhitespaceOnlyLine(Line) && CommonIndent > 0)
			{
				Line.RightChopInline(CommonIndent, EAllowShrinking::No);
			}

			if (LineIndex > 0)
			{
				Result += TEXT("\n");
			}
			Result += Line;
		}

		return Result;
	}

	inline std::string NormalizeInlineASSourceAnsi(const TCHAR* Source)
	{
		const FString Normalized = NormalizeInlineASSource(Source);
		const FTCHARToUTF8 Utf8(*Normalized);
		return std::string(Utf8.Get(), Utf8.Length());
	}

	inline FString NormalizeInlineASSourcePreserveLines(const TCHAR* Source)
	{
		if (Source == nullptr)
		{
			return FString();
		}

		FString Text(Source);
		const bool bUseCrLf = Text.Contains(TEXT("\r\n"));
		Text.ReplaceInline(TEXT("\r\n"), TEXT("\n"));
		Text.ReplaceInline(TEXT("\r"), TEXT("\n"));

		if (Text.StartsWith(TEXT("\n")))
		{
			Text.RightChopInline(1, EAllowShrinking::No);
		}

		const int32 LastLineStart = Text.Find(TEXT("\n"), ESearchCase::CaseSensitive, ESearchDir::FromEnd);
		if (LastLineStart != INDEX_NONE && IsInlineASWhitespaceOnlyLine(Text.Mid(LastLineStart + 1)))
		{
			Text.LeftInline(LastLineStart, EAllowShrinking::No);
		}

		TArray<FString> Lines;
		Text.ParseIntoArray(Lines, TEXT("\n"), false);
		int32 CommonIndent = MAX_int32;
		for (const FString& Line : Lines)
		{
			if (!IsInlineASWhitespaceOnlyLine(Line))
			{
				CommonIndent = FMath::Min(CommonIndent, CountInlineASIndent(Line));
			}
		}
		if (CommonIndent == MAX_int32)
		{
			CommonIndent = 0;
		}

		for (FString& Line : Lines)
		{
			if (!IsInlineASWhitespaceOnlyLine(Line) && CommonIndent > 0)
			{
				Line.RightChopInline(CommonIndent, EAllowShrinking::No);
			}
		}

		FString Result = FString::Join(Lines, TEXT("\n"));
		if (bUseCrLf)
		{
			Result.ReplaceInline(TEXT("\n"), TEXT("\r\n"));
		}
		return Result;
	}
}

#define ASTEST_AS(SourceLiteral) \
	AngelscriptTest::NormalizeInlineASSource(TEXT(SourceLiteral))

#define ASTEST_AS_ANSI(SourceLiteral) \
	AngelscriptTest::NormalizeInlineASSourceAnsi(TEXT(SourceLiteral))

// ============================================================================
// Engine Creation Macros
// ============================================================================

// CREATE - Acquires the shared engine after resetting it to a clean state.
// Use for: first-time acquisition in BEFORE_ALL, or standalone tests needing
//          a guaranteed clean shared engine.
// Returns: FAngelscriptEngine&
#define ASTEST_CREATE_ENGINE() \
	([]() -> FAngelscriptEngine& { \
		FAngelscriptEngine& Engine = FAngelscriptTestEngine::GetSharedEngine(); \
		FAngelscriptTestEngine::ResetModules(Engine); \
		return Engine; \
	}())

// GET - Acquires the shared engine without resetting.
// Use for: TEST_METHOD bodies where the engine was already cleaned by
//          BEFORE_ALL / ASTEST_CREATE_ENGINE(). Pair with FCoverageModuleScope
//          for per-test module isolation.
// Returns: FAngelscriptEngine&
#define ASTEST_GET_ENGINE() \
	FAngelscriptTestEngine::GetSharedEngine()

// FULL - Creates a fresh isolated Full engine each time.
// Use for: engine core self-tests, bind environment testing, hot-reload tests.
// Returns: FAngelscriptEngine&
#define ASTEST_CREATE_ENGINE_FULL() \
	AcquireTransientFullTestEngine()

// NATIVE - Raw asIScriptEngine from the AngelScript SDK.
// Use for: testing AngelScript SDK APIs directly.
// Returns: asIScriptEngine*
#define ASTEST_CREATE_ENGINE_NATIVE() \
	asCreateScriptEngine(ANGELSCRIPT_VERSION)

// ============================================================================
// Engine Reset Macro
// ============================================================================

// RESET - Resets the shared engine to a clean state.
// Use for: AFTER_ALL / AFTER_EACH to leave the shared engine clean for
//          subsequent test classes.
#define ASTEST_RESET_ENGINE(Engine) \
	FAngelscriptTestEngine::ResetModules(Engine)
