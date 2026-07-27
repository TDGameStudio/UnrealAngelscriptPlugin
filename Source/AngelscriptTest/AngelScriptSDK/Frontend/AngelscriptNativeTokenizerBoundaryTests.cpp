#include "../Support/AngelscriptNativeCoreTestSupport.h"
#include "../Support/AngelscriptNativeCaseTestSupport.h"
#include "../Support/AngelscriptNativeLanguageCaseTestSupport.h"
#include "CQTest.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_tokendef.h"
#include "source/as_tokenizer.h"
#include "EndAngelscriptHeaders.h"

#if WITH_ANGELSCRIPT_UNITTESTS


TEST_CLASS_WITH_FLAGS(FTokenizerBoundaryTests,
	"Angelscript.TestModule.AngelScriptSDK.Frontend.TokenizerBoundary",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(LongIdentifiersByLengthAndTrailingToken)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT("FRONTEND-TOKEN-LONG-IDENTIFIER-BOUNDARIES",
			ENativeEvidence::Metadata
			| ENativeEvidence::Diagnostic
			| ENativeEvidence::Isolation);

		struct FTailCase
		{
			const TCHAR* Id;
			const TCHAR* Text;
			eTokenType ExpectedSecondToken;
			int32 ExpectedMinimumTokens;
		};
		const int32 IdentifierLengths[] =
		{
			1,
			63,
			255,
			400,
			1024,
			4096,
		};
		const FTailCase Tails[] =
		{
			{ TEXT("eof"), TEXT(""), ttEnd, 1 },
			{ TEXT("space_identifier"), TEXT(" Next"), ttWhiteSpace, 3 },
			{ TEXT("assignment_integer"), TEXT("=1"), ttAssignment, 3 },
			{ TEXT("newline_keyword"), TEXT("\nreturn"), ttWhiteSpace, 3 },
		};

		int32 ObservedCells = 0;
		for (const int32 IdentifierLength : IdentifierLengths)
		{
			for (const FTailCase& Tail : Tails)
			{
				const FString Identifier = FString::ChrN(IdentifierLength, TEXT('a'));
				const FString Source = Identifier + Tail.Text;
				const FString SourceId = FString::Printf(
					TEXT("FRONTEND-TOKEN-LONG-IDENTIFIER-BOUNDARIES-%d-%s"),
					IdentifierLength,
					Tail.Id);
				const FString ModuleName = FString::Printf(
					TEXT("TokenizerLongIdentifier_%d_%s"),
					IdentifierLength,
					Tail.Id);
				PrintGeneratedAsSource(*TestRunner, SourceId, ModuleName, Source);

				const FTCHARToUTF8 SourceUtf8(*Source);
				const TArray<TPair<eTokenType, size_t>> Tokens = TokenizeAll(
					SourceUtf8.Get(),
					static_cast<size_t>(SourceUtf8.Length()));
				ASSERT_THAT(IsTrue(Tokens.Num() >= Tail.ExpectedMinimumTokens,
					FString::Printf(TEXT("%s should publish the expected token sequence"), *SourceId)));
				if (Tokens.Num() < Tail.ExpectedMinimumTokens)
				{
					continue;
				}

				ASSERT_THAT(AreEqual(static_cast<int32>(ttIdentifier), static_cast<int32>(Tokens[0].Key),
					FString::Printf(TEXT("%s should begin with one identifier token"), *SourceId)));
				ASSERT_THAT(AreEqual(IdentifierLength, static_cast<int32>(Tokens[0].Value),
					FString::Printf(TEXT("%s should retain the complete identifier byte length"), *SourceId)));
				if (Tail.ExpectedSecondToken != ttEnd)
				{
					ASSERT_THAT(AreEqual(static_cast<int32>(Tail.ExpectedSecondToken), static_cast<int32>(Tokens[1].Key),
						FString::Printf(TEXT("%s should preserve the trailing token boundary"), *SourceId)));
				}
				++ObservedCells;
			}
		}

		ASSERT_THAT(AreEqual(24, ObservedCells,
			TEXT("Long-identifier product should execute every length and trailing-token cell")));
	}

	TEST_METHOD(LongIdentifierBoundaryFromReference)
	{
		AS_NATIVE_NON_PRODUCT("LegacyCompatibility",
			"Retained 400-byte identifier smoke; FRONTEND-TOKEN-LONG-IDENTIFIER-BOUNDARIES owns length and trailing-token combinations.");

		const FString LongIdentifier = FString::ChrN(400, TEXT('a'));
		const FTCHARToUTF8 LongIdentifierUtf8(*LongIdentifier);

		AngelscriptNativeTestSupport::FTokenizerAccessor Tokenizer;
		size_t TokenLength = 0;
		const eTokenType TokenType = Tokenizer.GetToken(
			LongIdentifierUtf8.Get(),
			static_cast<size_t>(LongIdentifierUtf8.Length()),
			&TokenLength);

		ASSERT_THAT(AreEqual(static_cast<int32>(ttIdentifier), static_cast<int32>(TokenType),
			TEXT("Reference long-token identifier should remain a single identifier token")));
		ASSERT_THAT(AreEqual(400, static_cast<int32>(TokenLength),
			TEXT("Reference long-token identifier should preserve the full token length")));
	}

	TEST_METHOD(LongIdentifierFollowedByAssignmentTokenizesInOrder)
	{
		AS_NATIVE_NON_PRODUCT("LegacyCompatibility",
			"Retained 400-byte assignment-boundary smoke; FRONTEND-TOKEN-LONG-IDENTIFIER-BOUNDARIES owns length and trailing-token combinations.");

		const FString LongIdentifier = FString::ChrN(400, TEXT('a'));
		const FString Source = FString::Printf(TEXT("%s = 1"), *LongIdentifier);
		const FTCHARToUTF8 SourceUtf8(*Source);

		const TArray<TPair<eTokenType, size_t>> Tokens = AngelscriptNativeTestSupport::TokenizeAll(
			SourceUtf8.Get(),
			static_cast<size_t>(SourceUtf8.Length()));

		ASSERT_THAT(IsTrue(Tokens.Num() >= 5,
			TEXT("Reference long-token assignment should emit at least five raw tokens")));
		if (Tokens.Num() >= 5)
		{
			ASSERT_THAT(AreEqual(static_cast<int32>(ttIdentifier), static_cast<int32>(Tokens[0].Key),
				TEXT("Reference long-token assignment should start with the long identifier")));
			ASSERT_THAT(AreEqual(400, static_cast<int32>(Tokens[0].Value),
				TEXT("Reference long-token assignment should keep the long identifier length")));
			ASSERT_THAT(AreEqual(static_cast<int32>(ttWhiteSpace), static_cast<int32>(Tokens[1].Key),
				TEXT("Reference long-token assignment should keep whitespace after the identifier")));
			ASSERT_THAT(AreEqual(static_cast<int32>(ttAssignment), static_cast<int32>(Tokens[2].Key),
				TEXT("Reference long-token assignment should expose the assignment operator")));
			ASSERT_THAT(AreEqual(static_cast<int32>(ttIntConstant), static_cast<int32>(Tokens[4].Key),
				TEXT("Reference long-token assignment should expose the integer literal")));
		}
	}

	TEST_METHOD(UnrecognizedTokenDoesNotPoisonFollowingIdentifier)
	{
		AS_NATIVE_NON_PRODUCT("LegacyCompatibility",
			"Retained backtick recovery smoke; FRONTEND-TOKEN-OPERATOR-MALFORMED-RECOVERY owns five malformed prefixes across identifier, integer, operator, and punctuation tails.");

		AngelscriptNativeTestSupport::FTokenizerAccessor Tokenizer;
		const char* Source = "`Value";
		size_t TokenLength = 0;

		const eTokenType FirstToken = Tokenizer.GetToken(Source, std::strlen(Source), &TokenLength);
		ASSERT_THAT(AreEqual(static_cast<int32>(ttUnrecognizedToken), static_cast<int32>(FirstToken),
			TEXT("Reference tokenizer recovery should classify the leading bad token")));
		ASSERT_THAT(AreEqual(1, static_cast<int32>(TokenLength),
			TEXT("Reference tokenizer recovery should consume only the bad token")));

		const eTokenType SecondToken = Tokenizer.GetToken(Source + TokenLength, std::strlen(Source) - TokenLength, &TokenLength);
		ASSERT_THAT(AreEqual(static_cast<int32>(ttIdentifier), static_cast<int32>(SecondToken),
			TEXT("Reference tokenizer recovery should resume with the following identifier")));
		ASSERT_THAT(AreEqual(5, static_cast<int32>(TokenLength),
			TEXT("Reference tokenizer recovery should preserve the following identifier length")));
	}

	TEST_METHOD(UnterminatedStringReportsDedicatedToken)
	{
		AS_NATIVE_NON_PRODUCT("LegacyCompatibility",
			"Retained unterminated-string smoke; FRONTEND-TOKEN-TEXT-COMMENT-WHITESPACE-BOUNDARIES owns exact text/comment token kinds and consumed ranges.");

		AngelscriptNativeTestSupport::FTokenizerAccessor Tokenizer;
		const char* Source = "\"unterminated";
		size_t TokenLength = 0;

		const eTokenType TokenType = Tokenizer.GetToken(Source, std::strlen(Source), &TokenLength);
		ASSERT_THAT(AreEqual(static_cast<int32>(ttNonTerminatedStringConstant), static_cast<int32>(TokenType),
			TEXT("Reference unterminated string should use the dedicated token type")));
		ASSERT_THAT(AreEqual(static_cast<int32>(std::strlen(Source)), static_cast<int32>(TokenLength),
			TEXT("Reference unterminated string should consume the full input")));
	}
};

#endif
