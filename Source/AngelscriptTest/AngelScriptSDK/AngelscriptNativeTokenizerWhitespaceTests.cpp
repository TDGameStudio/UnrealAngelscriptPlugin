#include "AngelscriptNativeTestSupport.h"
#include "CQTest.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_tokendef.h"
#include "source/as_tokenizer.h"
#include "EndAngelscriptHeaders.h"

#if WITH_DEV_AUTOMATION_TESTS


TEST_CLASS_WITH_FLAGS(FAngelscriptNativeTokenizerWhitespaceTests,
	"Angelscript.TestModule.AngelScriptSDK.Tokenizer.Whitespace",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(LineCommentEmpty)
	{
		AngelscriptNativeTestSupport::FTokenizerAccessor Tokenizer;
		size_t TokenLength = 0;

		ASSERT_THAT(AreEqual(static_cast<int32>(ttOnelineComment), static_cast<int32>(Tokenizer.GetToken("//\n", 3, &TokenLength)),
			TEXT("Empty line comment should be recognized")));
		ASSERT_THAT(AreEqual(3, static_cast<int32>(TokenLength),
			TEXT("Empty line comment should consume through the newline")));
	}

	TEST_METHOD(BlockCommentNested_DocumentBehavior)
	{
		AngelscriptNativeTestSupport::FTokenizerAccessor Tokenizer;
		const char* Input = "/* outer /* inner */ tail */";
		size_t TokenLength = 0;

		ASSERT_THAT(AreEqual(static_cast<int32>(ttMultilineComment), static_cast<int32>(Tokenizer.GetToken(Input, std::strlen(Input), &TokenLength)),
			TEXT("Block comments should be recognized")));
		ASSERT_THAT(IsTrue(TokenLength < std::strlen(Input),
			TEXT("Tokenizer should stop at the first closing block-comment marker")));
	}

	TEST_METHOD(UnterminatedBlockCommentReachesEOF)
	{
		AngelscriptNativeTestSupport::FTokenizerAccessor Tokenizer;
		const char* Input = "/* comment";
		size_t TokenLength = 0;

		ASSERT_THAT(AreEqual(static_cast<int32>(ttMultilineComment), static_cast<int32>(Tokenizer.GetToken(Input, std::strlen(Input), &TokenLength)),
			TEXT("Unterminated block comment should still be classified as a multiline comment")));
		ASSERT_THAT(AreEqual(static_cast<int32>(std::strlen(Input)), static_cast<int32>(TokenLength),
			TEXT("Unterminated block comment should consume to end of input")));
	}

	TEST_METHOD(MixedCRLFWhitespace)
	{
		AngelscriptNativeTestSupport::FTokenizerAccessor Tokenizer;
		const char* Input = " \t\r\n  ";
		size_t TokenLength = 0;

		ASSERT_THAT(AreEqual(static_cast<int32>(ttWhiteSpace), static_cast<int32>(Tokenizer.GetToken(Input, std::strlen(Input), &TokenLength)),
			TEXT("Mixed whitespace should be grouped into one whitespace token")));
		ASSERT_THAT(AreEqual(static_cast<int32>(std::strlen(Input)), static_cast<int32>(TokenLength),
			TEXT("Mixed whitespace should consume the full whitespace span")));
	}

	TEST_METHOD(BomAtStartOfSource)
	{
		AngelscriptNativeTestSupport::FTokenizerAccessor Tokenizer;
		const char Input[] = "\xEF\xBB\xBF" "class";
		size_t TokenLength = 0;

		ASSERT_THAT(AreEqual(static_cast<int32>(ttWhiteSpace), static_cast<int32>(Tokenizer.GetToken(Input, sizeof(Input) - 1, &TokenLength)),
			TEXT("UTF-8 BOM should be recognized as whitespace")));
		ASSERT_THAT(AreEqual(3, static_cast<int32>(TokenLength),
			TEXT("UTF-8 BOM token should consume exactly three bytes")));
	}

	TEST_METHOD(IdentifierLeadingUnderscore)
	{
		AngelscriptNativeTestSupport::FTokenizerAccessor Tokenizer;
		size_t TokenLength = 0;

		ASSERT_THAT(AreEqual(static_cast<int32>(ttIdentifier), static_cast<int32>(Tokenizer.GetToken("_Value", 6, &TokenLength)),
			TEXT("Identifier may start with underscore")));
		ASSERT_THAT(AreEqual(6, static_cast<int32>(TokenLength),
			TEXT("Identifier starting with underscore should consume all identifier characters")));
	}

	TEST_METHOD(IdentifierWithDigits)
	{
		AngelscriptNativeTestSupport::FTokenizerAccessor Tokenizer;
		size_t TokenLength = 0;

		ASSERT_THAT(AreEqual(static_cast<int32>(ttIdentifier), static_cast<int32>(Tokenizer.GetToken("Value123", 8, &TokenLength)),
			TEXT("Identifier may contain trailing digits")));
		ASSERT_THAT(AreEqual(8, static_cast<int32>(TokenLength),
			TEXT("Identifier with digits should consume the full identifier")));
	}

	TEST_METHOD(KeywordVsIdentifierBoundary)
	{
		AngelscriptNativeTestSupport::FTokenizerAccessor Tokenizer;
		size_t TokenLength = 0;

		ASSERT_THAT(AreEqual(static_cast<int32>(ttIdentifier), static_cast<int32>(Tokenizer.GetToken("className", 9, &TokenLength)),
			TEXT("Keyword followed by an identifier character should remain an identifier")));
		ASSERT_THAT(AreEqual(9, static_cast<int32>(TokenLength),
			TEXT("Keyword-boundary identifier should consume all characters")));
	}

	TEST_METHOD(ZeroLengthInputReturnsEnd)
	{
		AngelscriptNativeTestSupport::FTokenizerAccessor Tokenizer;
		size_t TokenLength = 0;
		const eTokenType TokenType = Tokenizer.GetToken("", 0, &TokenLength);

		ASSERT_THAT(IsTrue(TokenType == ttEnd || TokenType == ttUnrecognizedToken,
			TEXT("Raw zero-length tokenizer input should return a stable sentinel token")));
	}

	TEST_METHOD(PastEofGracefulHandling)
	{
		AngelscriptNativeTestSupport::FTokenizerAccessor Tokenizer;
		size_t TokenLength = 0;
		const char Input[] = "";
		const eTokenType TokenType = Tokenizer.GetToken(Input, 0, &TokenLength);

		ASSERT_THAT(IsTrue(TokenType == ttEnd || TokenType == ttUnrecognizedToken,
			TEXT("Past-EOF style input should not produce an ordinary source token")));
		ASSERT_THAT(IsTrue(TokenLength <= 1,
			TEXT("Past-EOF style input should report a bounded token length")));
	}
};

#endif
