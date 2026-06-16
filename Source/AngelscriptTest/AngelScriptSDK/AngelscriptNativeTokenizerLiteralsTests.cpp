#include "AngelscriptNativeTestSupport.h"
#include "CQTest.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_tokendef.h"
#include "source/as_tokenizer.h"
#include "EndAngelscriptHeaders.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptNativeTestSupport;

namespace
{
	void ExpectToken(FAutomationTestBase& Test, const char* Input, const eTokenType ExpectedType, const int32 ExpectedLength, const TCHAR* CaseName)
	{
		FTokenizerAccessor Tokenizer;
		size_t TokenLength = 0;
		const eTokenType TokenType = Tokenizer.GetToken(Input, std::strlen(Input), &TokenLength);

		Test.TestEqual(FString::Printf(TEXT("%s should use the expected token type"), CaseName), static_cast<int32>(TokenType), static_cast<int32>(ExpectedType));
		Test.TestEqual(FString::Printf(TEXT("%s should use the expected token length"), CaseName), static_cast<int32>(TokenLength), ExpectedLength);
	}
}

TEST_CLASS_WITH_FLAGS(FAngelscriptNativeTokenizerLiteralsTests,
	"Angelscript.TestModule.AngelScriptSDK.Tokenizer.Literals",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(HexIntegerLiteral)
	{
		ExpectToken(*TestRunner, "0x2A", ttBitsConstant, 4, TEXT("Hex integer literal"));
	}

	TEST_METHOD(HexUppercaseAndLowercase)
	{
		ExpectToken(*TestRunner, "0XfF", ttBitsConstant, 4, TEXT("Mixed-case hex integer literal"));
	}

	TEST_METHOD(OctalLiteralIfSupported_OrDocumentReject)
	{
		ExpectToken(*TestRunner, "0o755", ttBitsConstant, 5, TEXT("Octal radix literal"));
	}

	TEST_METHOD(BinaryLiteralIfSupported_OrDocumentReject)
	{
		ExpectToken(*TestRunner, "0b1010", ttBitsConstant, 6, TEXT("Binary radix literal"));
	}

	TEST_METHOD(DecimalIntegerVarieties)
	{
		ExpectToken(*TestRunner, "0", ttIntConstant, 1, TEXT("Zero integer literal"));
		ExpectToken(*TestRunner, "42", ttIntConstant, 2, TEXT("Small integer literal"));
		ExpectToken(*TestRunner, "1234567890", ttIntConstant, 10, TEXT("Long integer literal"));
	}

	TEST_METHOD(Float64WithoutSuffix)
	{
		ExpectToken(*TestRunner, "1.25", ttFloat64Constant, 4, TEXT("Float64 literal without suffix"));
	}

	TEST_METHOD(Float32WithFSuffix)
	{
		ExpectToken(*TestRunner, "1.25f", ttFloat32Constant, 5, TEXT("Float32 literal with f suffix"));
	}

	TEST_METHOD(Float64WithDSuffix)
	{
		ExpectToken(*TestRunner, "1.25d", ttFloat64Constant, 4, TEXT("Float64 literal leaves d suffix for the next token"));
	}

	TEST_METHOD(FloatExponentPositive)
	{
		ExpectToken(*TestRunner, "1e+10", ttFloat64Constant, 5, TEXT("Float exponent with positive sign"));
	}

	TEST_METHOD(FloatExponentNegative)
	{
		ExpectToken(*TestRunner, "1e-10", ttFloat64Constant, 5, TEXT("Float exponent with negative sign"));
	}

	TEST_METHOD(FloatLeadingDot)
	{
		ExpectToken(*TestRunner, ".5", ttFloat64Constant, 2, TEXT("Float literal with leading dot"));
	}

	TEST_METHOD(FloatTrailingDot)
	{
		ExpectToken(*TestRunner, "5.", ttFloat64Constant, 2, TEXT("Float literal with trailing dot"));
	}

	TEST_METHOD(StringEscape_NTRBackslashQuote)
	{
		ExpectToken(*TestRunner, "\"\\n\\t\\r\\\\\\\"\"", ttStringConstant, 12, TEXT("Escaped newline tab carriage-return backslash and quote"));
	}

	TEST_METHOD(StringEscape_HexByte_xNN)
	{
		ExpectToken(*TestRunner, "\"\\x41\"", ttStringConstant, 6, TEXT("Hex byte escape sequence"));
	}

	TEST_METHOD(StringEscape_Unicode_uNNNN)
	{
		ExpectToken(*TestRunner, "\"\\u0041\"", ttStringConstant, 8, TEXT("Unicode escape sequence"));
	}

	TEST_METHOD(HeredocStringIfEnabled)
	{
		ExpectToken(*TestRunner, "\"\"\"line\ntext\"\"\"", ttHeredocStringConstant, 15, TEXT("Heredoc string literal"));
	}

	TEST_METHOD(CharLiteralBasic)
	{
		ExpectToken(*TestRunner, "'a'", ttStringConstant, 3, TEXT("Character literal token"));
	}

	TEST_METHOD(CharLiteralEscape)
	{
		ExpectToken(*TestRunner, "'\\n'", ttStringConstant, 4, TEXT("Escaped character literal token"));
	}

	TEST_METHOD(EmptyStringLiteral)
	{
		ExpectToken(*TestRunner, "\"\"", ttStringConstant, 2, TEXT("Empty string literal"));
	}

	TEST_METHOD(AdjacentStringConcatNotMerged)
	{
		FTokenizerAccessor Tokenizer;
		const char* Input = "\"a\"\"b\"";
		size_t TokenLength = 0;

		TestRunner->TestEqual(TEXT("Adjacent string scan should return the first string token only"), static_cast<int32>(Tokenizer.GetToken(Input, std::strlen(Input), &TokenLength)), static_cast<int32>(ttStringConstant));
		TestRunner->TestEqual(TEXT("Adjacent string scan should not merge the following string token"), static_cast<int32>(TokenLength), 3);
	}
};

#endif
