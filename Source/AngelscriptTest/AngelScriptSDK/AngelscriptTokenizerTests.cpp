#include "CQTest.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_tokenizer.h"
#include "source/as_tokendef.h"
#include "EndAngelscriptHeaders.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	struct FTokenizerAccessor : asCTokenizer
	{
		using asCTokenizer::GetToken;
	};
}


TEST_CLASS_WITH_FLAGS(FAngelscriptTokenizerTests,
	"Angelscript.TestModule.AngelScriptSDK.Tokenizer",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(BasicTokens)
	{
		FTokenizerAccessor Tokenizer;
		size_t TokenLength = 0;

		TestRunner->TestEqual(TEXT("Identifier token type should be recognized"), static_cast<int32>(Tokenizer.GetToken("Identifier123", 13, &TokenLength)), static_cast<int32>(ttIdentifier));
		TestRunner->TestEqual(TEXT("Identifier token length should be returned"), static_cast<int32>(TokenLength), 13);

		TestRunner->TestEqual(TEXT("Integer literal token type should be recognized"), static_cast<int32>(Tokenizer.GetToken("12345", 5, &TokenLength)), static_cast<int32>(ttIntConstant));
		TestRunner->TestEqual(TEXT("Integer literal token length should be returned"), static_cast<int32>(TokenLength), 5);

		TestRunner->TestEqual(TEXT("String literal token type should be recognized"), static_cast<int32>(Tokenizer.GetToken("\"abc\"", 5, &TokenLength)), static_cast<int32>(ttStringConstant));
		TestRunner->TestEqual(TEXT("String literal token length should be returned"), static_cast<int32>(TokenLength), 5);

		TestRunner->TestEqual(TEXT("Operator token type should be recognized"), static_cast<int32>(Tokenizer.GetToken("+", 1, &TokenLength)), static_cast<int32>(ttPlus));
		TestRunner->TestEqual(TEXT("Operator token length should be returned"), static_cast<int32>(TokenLength), 1);
	}

	TEST_METHOD(Keywords)
	{
		FTokenizerAccessor Tokenizer;
		size_t TokenLength = 0;

		TestRunner->TestEqual(TEXT("class should be recognized as a keyword token"), static_cast<int32>(Tokenizer.GetToken("class", 5, &TokenLength)), static_cast<int32>(ttClass));
		TestRunner->TestEqual(TEXT("void should be recognized as a keyword token"), static_cast<int32>(Tokenizer.GetToken("void", 4, &TokenLength)), static_cast<int32>(ttVoid));
		TestRunner->TestEqual(TEXT("int should be recognized as a keyword token"), static_cast<int32>(Tokenizer.GetToken("int", 3, &TokenLength)), static_cast<int32>(ttInt));
		TestRunner->TestEqual(TEXT("float32 should be recognized as a keyword token"), static_cast<int32>(Tokenizer.GetToken("float32", 7, &TokenLength)), static_cast<int32>(ttFloat32));
	}

	TEST_METHOD(CommentsAndStrings)
	{
		FTokenizerAccessor Tokenizer;
		size_t TokenLength = 0;

		TestRunner->TestEqual(TEXT("Single line comment should be recognized"), static_cast<int32>(Tokenizer.GetToken("// hello\n", 9, &TokenLength)), static_cast<int32>(ttOnelineComment));
		TestRunner->TestEqual(TEXT("Multi line comment should be recognized"), static_cast<int32>(Tokenizer.GetToken("/* hi */", 8, &TokenLength)), static_cast<int32>(ttMultilineComment));
		TestRunner->TestEqual(TEXT("Multiline string should be recognized"), static_cast<int32>(Tokenizer.GetToken("\"first\\nsecond\"", 15, &TokenLength)), static_cast<int32>(ttStringConstant));
	}

	TEST_METHOD(ErrorRecovery)
	{
		FTokenizerAccessor Tokenizer;
		size_t TokenLength = 0;

		TestRunner->TestEqual(TEXT("Unterminated string should produce the dedicated token type"), static_cast<int32>(Tokenizer.GetToken("\"unterminated", 13, &TokenLength)), static_cast<int32>(ttNonTerminatedStringConstant));
		TestRunner->TestEqual(TEXT("Unknown characters should produce an unrecognized token"), static_cast<int32>(Tokenizer.GetToken("`", 1, &TokenLength)), static_cast<int32>(ttUnrecognizedToken));
	}

	TEST_METHOD(ErrorRecoveryAdvancesAndContinues)
	{
		FTokenizerAccessor Tokenizer;
		size_t TokenLength = 0;
		const char* Input = "`class";
		const size_t InputLength = 6;

		const int32 FirstTokenType = static_cast<int32>(Tokenizer.GetToken(Input, InputLength, &TokenLength));
		if (!TestRunner->TestEqual(TEXT("Tokenizer recovery should classify the leading invalid character as an unrecognized token"), FirstTokenType, static_cast<int32>(ttUnrecognizedToken)))
		{
			return;
		}

		if (!TestRunner->TestEqual(TEXT("Tokenizer recovery should still advance by one character for the invalid token"), static_cast<int32>(TokenLength), 1))
		{
			return;
		}

		const char* ContinuedInput = Input + TokenLength;
		const size_t ContinuedLength = InputLength - TokenLength;
		const int32 ContinuedTokenType = static_cast<int32>(Tokenizer.GetToken(ContinuedInput, ContinuedLength, &TokenLength));
		TestRunner->TestEqual(TEXT("Tokenizer recovery should continue scanning and recognize the trailing class keyword"), ContinuedTokenType, static_cast<int32>(ttClass));
		TestRunner->TestEqual(TEXT("Tokenizer recovery should return the full trailing keyword length after advancing"), static_cast<int32>(TokenLength), 5);
	}

	TEST_METHOD(BasicLiteralAndPunctuationMatrix)
	{
		FTokenizerAccessor Tokenizer;
		size_t TokenLength = 0;

		struct FTokenCase
		{
			const char* Input;
			size_t InputLength;
			int32 ExpectedType;
			int32 ExpectedLength;
			const TCHAR* Description;
		};

		const FTokenCase Cases[] = {
			{ "1.25f", 5, static_cast<int32>(ttFloat32Constant), 5, TEXT("Float32 literal token") },
			{ "1.25", 4, static_cast<int32>(ttFloat64Constant), 4, TEXT("Float64 literal token") },
			{ "0xFF", 4, static_cast<int32>(ttBitsConstant), 4, TEXT("Bits literal token") },
			{ "(", 1, static_cast<int32>(ttOpenParanthesis), 1, TEXT("Open parenthesis token") },
			{ ")", 1, static_cast<int32>(ttCloseParanthesis), 1, TEXT("Close parenthesis token") },
			{ ";", 1, static_cast<int32>(ttEndStatement), 1, TEXT("Statement terminator token") },
			{ ",", 1, static_cast<int32>(ttListSeparator), 1, TEXT("List separator token") },
		};

		for (const FTokenCase& Case : Cases)
		{
			TestRunner->TestEqual(FString::Printf(TEXT("%s should use the expected token type"), Case.Description), static_cast<int32>(Tokenizer.GetToken(Case.Input, Case.InputLength, &TokenLength)), Case.ExpectedType);
			TestRunner->TestEqual(FString::Printf(TEXT("%s should use the expected token length"), Case.Description), static_cast<int32>(TokenLength), Case.ExpectedLength);
		}
	}

	TEST_METHOD(UnterminatedBlockCommentAndEscapes)
	{
		FTokenizerAccessor Tokenizer;
		size_t TokenLength = 0;

		const char UnterminatedBlockComment[] = "/* comment";
		const size_t UnterminatedBlockCommentLength = sizeof(UnterminatedBlockComment) - 1;
		TestRunner->TestEqual(TEXT("Unterminated block comment should still be classified as a multiline comment token"), static_cast<int32>(Tokenizer.GetToken(UnterminatedBlockComment, UnterminatedBlockCommentLength, &TokenLength)), static_cast<int32>(ttMultilineComment));
		TestRunner->TestEqual(TEXT("Unterminated block comment should consume the entire source length"), static_cast<int32>(TokenLength), static_cast<int32>(UnterminatedBlockCommentLength));

		const char EscapedStringInput[] = "\"escaped \\\"quote\\\" and \\\\ slash\"+";
		const size_t EscapedStringInputLength = sizeof(EscapedStringInput) - 1;
		const size_t ExpectedEscapedStringTokenLength = sizeof("\"escaped \\\"quote\\\" and \\\\ slash\"") - 1;
		TestRunner->TestEqual(TEXT("Escaped quote and backslash string should remain a string token"), static_cast<int32>(Tokenizer.GetToken(EscapedStringInput, EscapedStringInputLength, &TokenLength)), static_cast<int32>(ttStringConstant));
		TestRunner->TestEqual(TEXT("Escaped quote and backslash string should stop at the closing quote rather than the trailing operator"), static_cast<int32>(TokenLength), static_cast<int32>(ExpectedEscapedStringTokenLength));
		TestRunner->TestTrue(TEXT("Escaped quote and backslash string should leave trailing input for the next token"), TokenLength < EscapedStringInputLength);
	}
};

#endif
