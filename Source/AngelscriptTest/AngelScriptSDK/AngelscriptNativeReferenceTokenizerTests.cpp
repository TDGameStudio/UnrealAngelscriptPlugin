#include "AngelscriptNativeTestSupport.h"
#include "CQTest.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_tokendef.h"
#include "source/as_tokenizer.h"
#include "EndAngelscriptHeaders.h"

#if WITH_DEV_AUTOMATION_TESTS


TEST_CLASS_WITH_FLAGS(FAngelscriptNativeReferenceTokenizerTests,
	"Angelscript.TestModule.AngelScriptSDK.Reference.Tokenizer",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(LongIdentifierBoundaryFromReference)
	{
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
