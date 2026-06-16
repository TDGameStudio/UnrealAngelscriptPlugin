#include "AngelscriptNativeTestSupport.h"
#include "CQTest.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_tokendef.h"
#include "source/as_tokenizer.h"
#include "EndAngelscriptHeaders.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptNativeTestSupport;

TEST_CLASS_WITH_FLAGS(FAngelscriptNativeReferenceTokenizerTests,
	"Angelscript.TestModule.AngelScriptSDK.Reference.Tokenizer",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(LongIdentifierBoundaryFromReference)
	{
		const FString LongIdentifier = FString::ChrN(400, TEXT('a'));
		const FTCHARToUTF8 LongIdentifierUtf8(*LongIdentifier);

		FTokenizerAccessor Tokenizer;
		size_t TokenLength = 0;
		const eTokenType TokenType = Tokenizer.GetToken(
			LongIdentifierUtf8.Get(),
			static_cast<size_t>(LongIdentifierUtf8.Length()),
			&TokenLength);

		TestRunner->TestEqual(TEXT("Reference long-token identifier should remain a single identifier token"), static_cast<int32>(TokenType), static_cast<int32>(ttIdentifier));
		TestRunner->TestEqual(TEXT("Reference long-token identifier should preserve the full token length"), static_cast<int32>(TokenLength), 400);
	}

	TEST_METHOD(LongIdentifierFollowedByAssignmentTokenizesInOrder)
	{
		const FString LongIdentifier = FString::ChrN(400, TEXT('a'));
		const FString Source = FString::Printf(TEXT("%s = 1"), *LongIdentifier);
		const FTCHARToUTF8 SourceUtf8(*Source);

		const TArray<TPair<eTokenType, size_t>> Tokens = AngelscriptNativeTestSupport::TokenizeAll(
			SourceUtf8.Get(),
			static_cast<size_t>(SourceUtf8.Length()));

		TestRunner->TestTrue(TEXT("Reference long-token assignment should emit at least five raw tokens"), Tokens.Num() >= 5);
		if (Tokens.Num() >= 5)
		{
			TestRunner->TestEqual(TEXT("Reference long-token assignment should start with the long identifier"), static_cast<int32>(Tokens[0].Key), static_cast<int32>(ttIdentifier));
			TestRunner->TestEqual(TEXT("Reference long-token assignment should keep the long identifier length"), static_cast<int32>(Tokens[0].Value), 400);
			TestRunner->TestEqual(TEXT("Reference long-token assignment should keep whitespace after the identifier"), static_cast<int32>(Tokens[1].Key), static_cast<int32>(ttWhiteSpace));
			TestRunner->TestEqual(TEXT("Reference long-token assignment should expose the assignment operator"), static_cast<int32>(Tokens[2].Key), static_cast<int32>(ttAssignment));
			TestRunner->TestEqual(TEXT("Reference long-token assignment should expose the integer literal"), static_cast<int32>(Tokens[4].Key), static_cast<int32>(ttIntConstant));
		}
	}

	TEST_METHOD(UnrecognizedTokenDoesNotPoisonFollowingIdentifier)
	{
		FTokenizerAccessor Tokenizer;
		const char* Source = "`Value";
		size_t TokenLength = 0;

		const eTokenType FirstToken = Tokenizer.GetToken(Source, std::strlen(Source), &TokenLength);
		TestRunner->TestEqual(TEXT("Reference tokenizer recovery should classify the leading bad token"), static_cast<int32>(FirstToken), static_cast<int32>(ttUnrecognizedToken));
		TestRunner->TestEqual(TEXT("Reference tokenizer recovery should consume only the bad token"), static_cast<int32>(TokenLength), 1);

		const eTokenType SecondToken = Tokenizer.GetToken(Source + TokenLength, std::strlen(Source) - TokenLength, &TokenLength);
		TestRunner->TestEqual(TEXT("Reference tokenizer recovery should resume with the following identifier"), static_cast<int32>(SecondToken), static_cast<int32>(ttIdentifier));
		TestRunner->TestEqual(TEXT("Reference tokenizer recovery should preserve the following identifier length"), static_cast<int32>(TokenLength), 5);
	}

	TEST_METHOD(UnterminatedStringReportsDedicatedToken)
	{
		FTokenizerAccessor Tokenizer;
		const char* Source = "\"unterminated";
		size_t TokenLength = 0;

		const eTokenType TokenType = Tokenizer.GetToken(Source, std::strlen(Source), &TokenLength);
		TestRunner->TestEqual(TEXT("Reference unterminated string should use the dedicated token type"), static_cast<int32>(TokenType), static_cast<int32>(ttNonTerminatedStringConstant));
		TestRunner->TestEqual(TEXT("Reference unterminated string should consume the full input"), static_cast<int32>(TokenLength), static_cast<int32>(std::strlen(Source)));
	}
};

#endif
