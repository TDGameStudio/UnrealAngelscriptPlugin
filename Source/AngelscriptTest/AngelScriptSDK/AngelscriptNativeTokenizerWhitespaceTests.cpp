#include "CQTest.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_tokendef.h"
#include "source/as_tokenizer.h"
#include "EndAngelscriptHeaders.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace AngelscriptTest_AngelScriptSDK_NativeTokenizerWhitespace_Private
{
	struct FTokenizerAccessor : asCTokenizer
	{
		using asCTokenizer::GetToken;
	};
}

TEST_CLASS_WITH_FLAGS(FAngelscriptNativeTokenizerWhitespaceTests,
	"Angelscript.TestModule.AngelScriptSDK.Tokenizer.Whitespace",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(LineCommentEmpty)
	{
		using namespace AngelscriptTest_AngelScriptSDK_NativeTokenizerWhitespace_Private;
		FTokenizerAccessor Tokenizer;
		size_t TokenLength = 0;

		TestRunner->TestEqual(TEXT("Empty line comment should be recognized"), static_cast<int32>(Tokenizer.GetToken("//\n", 3, &TokenLength)), static_cast<int32>(ttOnelineComment));
		TestRunner->TestEqual(TEXT("Empty line comment should consume through the newline"), static_cast<int32>(TokenLength), 3);
	}

	TEST_METHOD(BlockCommentNested_DocumentBehavior)
	{
		using namespace AngelscriptTest_AngelScriptSDK_NativeTokenizerWhitespace_Private;
		FTokenizerAccessor Tokenizer;
		const char* Input = "/* outer /* inner */ tail */";
		size_t TokenLength = 0;

		TestRunner->TestEqual(TEXT("Block comments should be recognized"), static_cast<int32>(Tokenizer.GetToken(Input, std::strlen(Input), &TokenLength)), static_cast<int32>(ttMultilineComment));
		TestRunner->TestTrue(TEXT("Tokenizer should stop at the first closing block-comment marker"), TokenLength < std::strlen(Input));
	}

	TEST_METHOD(UnterminatedBlockCommentReachesEOF)
	{
		using namespace AngelscriptTest_AngelScriptSDK_NativeTokenizerWhitespace_Private;
		FTokenizerAccessor Tokenizer;
		const char* Input = "/* comment";
		size_t TokenLength = 0;

		TestRunner->TestEqual(TEXT("Unterminated block comment should still be classified as a multiline comment"), static_cast<int32>(Tokenizer.GetToken(Input, std::strlen(Input), &TokenLength)), static_cast<int32>(ttMultilineComment));
		TestRunner->TestEqual(TEXT("Unterminated block comment should consume to end of input"), static_cast<int32>(TokenLength), static_cast<int32>(std::strlen(Input)));
	}

	TEST_METHOD(MixedCRLFWhitespace)
	{
		using namespace AngelscriptTest_AngelScriptSDK_NativeTokenizerWhitespace_Private;
		FTokenizerAccessor Tokenizer;
		const char* Input = " \t\r\n  ";
		size_t TokenLength = 0;

		TestRunner->TestEqual(TEXT("Mixed whitespace should be grouped into one whitespace token"), static_cast<int32>(Tokenizer.GetToken(Input, std::strlen(Input), &TokenLength)), static_cast<int32>(ttWhiteSpace));
		TestRunner->TestEqual(TEXT("Mixed whitespace should consume the full whitespace span"), static_cast<int32>(TokenLength), static_cast<int32>(std::strlen(Input)));
	}

	TEST_METHOD(BomAtStartOfSource)
	{
		using namespace AngelscriptTest_AngelScriptSDK_NativeTokenizerWhitespace_Private;
		FTokenizerAccessor Tokenizer;
		const char Input[] = "\xEF\xBB\xBF" "class";
		size_t TokenLength = 0;

		TestRunner->TestEqual(TEXT("UTF-8 BOM should be recognized as whitespace"), static_cast<int32>(Tokenizer.GetToken(Input, sizeof(Input) - 1, &TokenLength)), static_cast<int32>(ttWhiteSpace));
		TestRunner->TestEqual(TEXT("UTF-8 BOM token should consume exactly three bytes"), static_cast<int32>(TokenLength), 3);
	}

	TEST_METHOD(IdentifierLeadingUnderscore)
	{
		using namespace AngelscriptTest_AngelScriptSDK_NativeTokenizerWhitespace_Private;
		FTokenizerAccessor Tokenizer;
		size_t TokenLength = 0;

		TestRunner->TestEqual(TEXT("Identifier may start with underscore"), static_cast<int32>(Tokenizer.GetToken("_Value", 6, &TokenLength)), static_cast<int32>(ttIdentifier));
		TestRunner->TestEqual(TEXT("Identifier starting with underscore should consume all identifier characters"), static_cast<int32>(TokenLength), 6);
	}

	TEST_METHOD(IdentifierWithDigits)
	{
		using namespace AngelscriptTest_AngelScriptSDK_NativeTokenizerWhitespace_Private;
		FTokenizerAccessor Tokenizer;
		size_t TokenLength = 0;

		TestRunner->TestEqual(TEXT("Identifier may contain trailing digits"), static_cast<int32>(Tokenizer.GetToken("Value123", 8, &TokenLength)), static_cast<int32>(ttIdentifier));
		TestRunner->TestEqual(TEXT("Identifier with digits should consume the full identifier"), static_cast<int32>(TokenLength), 8);
	}

	TEST_METHOD(KeywordVsIdentifierBoundary)
	{
		using namespace AngelscriptTest_AngelScriptSDK_NativeTokenizerWhitespace_Private;
		FTokenizerAccessor Tokenizer;
		size_t TokenLength = 0;

		TestRunner->TestEqual(TEXT("Keyword followed by an identifier character should remain an identifier"), static_cast<int32>(Tokenizer.GetToken("className", 9, &TokenLength)), static_cast<int32>(ttIdentifier));
		TestRunner->TestEqual(TEXT("Keyword-boundary identifier should consume all characters"), static_cast<int32>(TokenLength), 9);
	}

	TEST_METHOD(ZeroLengthInputReturnsEnd)
	{
		using namespace AngelscriptTest_AngelScriptSDK_NativeTokenizerWhitespace_Private;
		FTokenizerAccessor Tokenizer;
		size_t TokenLength = 0;
		const eTokenType TokenType = Tokenizer.GetToken("", 0, &TokenLength);

		TestRunner->TestTrue(TEXT("Raw zero-length tokenizer input should return a stable sentinel token"), TokenType == ttEnd || TokenType == ttUnrecognizedToken);
	}

	TEST_METHOD(PastEofGracefulHandling)
	{
		using namespace AngelscriptTest_AngelScriptSDK_NativeTokenizerWhitespace_Private;
		FTokenizerAccessor Tokenizer;
		size_t TokenLength = 0;
		const char Input[] = "";
		const eTokenType TokenType = Tokenizer.GetToken(Input, 0, &TokenLength);

		TestRunner->TestTrue(TEXT("Past-EOF style input should not produce an ordinary source token"), TokenType == ttEnd || TokenType == ttUnrecognizedToken);
		TestRunner->TestTrue(TEXT("Past-EOF style input should report a bounded token length"), TokenLength <= 1);
	}
};

#endif
