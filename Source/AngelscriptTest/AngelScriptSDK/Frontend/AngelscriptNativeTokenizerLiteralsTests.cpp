#include "../Support/AngelscriptNativeCoreTestSupport.h"

// Tokenizer literal behavior coverage.
#include "CQTest.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_tokendef.h"
#include "source/as_tokenizer.h"
#include "EndAngelscriptHeaders.h"

#if WITH_ANGELSCRIPT_UNITTESTS


TEST_CLASS_WITH_FLAGS(FTokenizerLiteralsTests,
	"Angelscript.TestModule.AngelScriptSDK.Frontend.Tokenizer.Literals",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	static bool ExpectTokenType(const char* Input, const eTokenType ExpectedType, int32& OutTokenLength)
	{
		AngelscriptNativeTestSupport::FTokenizerAccessor Tokenizer;
		size_t TokenLength = 0;
		const eTokenType TokenType = Tokenizer.GetToken(Input, std::strlen(Input), &TokenLength);
		OutTokenLength = static_cast<int32>(TokenLength);

		return TokenType == ExpectedType;
	}

public:
	TEST_METHOD(HexIntegerLiteral)
	{
		int32 TokenLength = 0;
		ASSERT_THAT(IsTrue(ExpectTokenType("0x2A", ttBitsConstant, TokenLength),
			TEXT("Hex integer literal should use the expected token type")));
		ASSERT_THAT(AreEqual(4, TokenLength, TEXT("Hex integer literal should use the expected token length")));
	}

	TEST_METHOD(HexUppercaseAndLowercase)
	{
		int32 TokenLength = 0;
		ASSERT_THAT(IsTrue(ExpectTokenType("0XfF", ttBitsConstant, TokenLength),
			TEXT("Mixed-case hex integer literal should use the expected token type")));
		ASSERT_THAT(AreEqual(4, TokenLength, TEXT("Mixed-case hex integer literal should use the expected token length")));
	}

	TEST_METHOD(OctalLiteralTokenizesAsBitsConstant)
	{
		int32 TokenLength = 0;
		ASSERT_THAT(IsTrue(ExpectTokenType("0o755", ttBitsConstant, TokenLength),
			TEXT("Octal radix literal should use the expected token type")));
		ASSERT_THAT(AreEqual(5, TokenLength, TEXT("Octal radix literal should use the expected token length")));
	}

	TEST_METHOD(BinaryLiteralTokenizesAsBitsConstant)
	{
		int32 TokenLength = 0;
		ASSERT_THAT(IsTrue(ExpectTokenType("0b1010", ttBitsConstant, TokenLength),
			TEXT("Binary radix literal should use the expected token type")));
		ASSERT_THAT(AreEqual(6, TokenLength, TEXT("Binary radix literal should use the expected token length")));
	}

	TEST_METHOD(DecimalIntegerVarieties)
	{
		int32 TokenLength = 0;
		ASSERT_THAT(IsTrue(ExpectTokenType("0", ttIntConstant, TokenLength),
			TEXT("Zero integer literal should use the expected token type")));
		ASSERT_THAT(AreEqual(1, TokenLength, TEXT("Zero integer literal should use the expected token length")));
		ASSERT_THAT(IsTrue(ExpectTokenType("42", ttIntConstant, TokenLength),
			TEXT("Small integer literal should use the expected token type")));
		ASSERT_THAT(AreEqual(2, TokenLength, TEXT("Small integer literal should use the expected token length")));
		ASSERT_THAT(IsTrue(ExpectTokenType("1234567890", ttIntConstant, TokenLength),
			TEXT("Long integer literal should use the expected token type")));
		ASSERT_THAT(AreEqual(10, TokenLength, TEXT("Long integer literal should use the expected token length")));
	}

	TEST_METHOD(Float64WithoutSuffix)
	{
		int32 TokenLength = 0;
		ASSERT_THAT(IsTrue(ExpectTokenType("1.25", ttFloat64Constant, TokenLength),
			TEXT("Float64 literal without suffix should use the expected token type")));
		ASSERT_THAT(AreEqual(4, TokenLength, TEXT("Float64 literal without suffix should use the expected token length")));
	}

	TEST_METHOD(Float32WithFSuffix)
	{
		int32 TokenLength = 0;
		ASSERT_THAT(IsTrue(ExpectTokenType("1.25f", ttFloat32Constant, TokenLength),
			TEXT("Float32 literal with f suffix should use the expected token type")));
		ASSERT_THAT(AreEqual(5, TokenLength, TEXT("Float32 literal with f suffix should use the expected token length")));
	}

	TEST_METHOD(Float64WithDSuffix)
	{
		int32 TokenLength = 0;
		ASSERT_THAT(IsTrue(ExpectTokenType("1.25d", ttFloat64Constant, TokenLength),
			TEXT("Float64 literal leaves d suffix for the next token should use the expected token type")));
		ASSERT_THAT(AreEqual(4, TokenLength, TEXT("Float64 literal leaves d suffix for the next token should use the expected token length")));
	}

	TEST_METHOD(FloatExponentPositive)
	{
		int32 TokenLength = 0;
		ASSERT_THAT(IsTrue(ExpectTokenType("1e+10", ttFloat64Constant, TokenLength),
			TEXT("Float exponent with positive sign should use the expected token type")));
		ASSERT_THAT(AreEqual(5, TokenLength, TEXT("Float exponent with positive sign should use the expected token length")));
	}

	TEST_METHOD(FloatExponentNegative)
	{
		int32 TokenLength = 0;
		ASSERT_THAT(IsTrue(ExpectTokenType("1e-10", ttFloat64Constant, TokenLength),
			TEXT("Float exponent with negative sign should use the expected token type")));
		ASSERT_THAT(AreEqual(5, TokenLength, TEXT("Float exponent with negative sign should use the expected token length")));
	}

	TEST_METHOD(TokenizerLiteralsFloatLeadingDot)
	{
		int32 TokenLength = 0;
		ASSERT_THAT(IsTrue(ExpectTokenType(".5", ttFloat64Constant, TokenLength),
			TEXT("Float literal with leading dot should use the expected token type")));
		ASSERT_THAT(AreEqual(2, TokenLength, TEXT("Float literal with leading dot should use the expected token length")));
	}

	TEST_METHOD(FloatTrailingDot)
	{
		int32 TokenLength = 0;
		ASSERT_THAT(IsTrue(ExpectTokenType("5.", ttFloat64Constant, TokenLength),
			TEXT("Float literal with trailing dot should use the expected token type")));
		ASSERT_THAT(AreEqual(2, TokenLength, TEXT("Float literal with trailing dot should use the expected token length")));
	}

	TEST_METHOD(StringEscape_NTRBackslashQuote)
	{
		int32 TokenLength = 0;
		ASSERT_THAT(IsTrue(ExpectTokenType("\"\\n\\t\\r\\\\\\\"\"", ttStringConstant, TokenLength),
			TEXT("Escaped newline tab carriage-return backslash and quote should use the expected token type")));
		ASSERT_THAT(AreEqual(12, TokenLength, TEXT("Escaped newline tab carriage-return backslash and quote should use the expected token length")));
	}

	TEST_METHOD(StringEscape_HexByte_xNN)
	{
		int32 TokenLength = 0;
		ASSERT_THAT(IsTrue(ExpectTokenType("\"\\x41\"", ttStringConstant, TokenLength),
			TEXT("Hex byte escape sequence should use the expected token type")));
		ASSERT_THAT(AreEqual(6, TokenLength, TEXT("Hex byte escape sequence should use the expected token length")));
	}

	TEST_METHOD(StringEscape_Unicode_uNNNN)
	{
		int32 TokenLength = 0;
		ASSERT_THAT(IsTrue(ExpectTokenType("\"\\u0041\"", ttStringConstant, TokenLength),
			TEXT("Unicode escape sequence should use the expected token type")));
		ASSERT_THAT(AreEqual(8, TokenLength, TEXT("Unicode escape sequence should use the expected token length")));
	}

	TEST_METHOD(HeredocStringIfEnabled)
	{
		int32 TokenLength = 0;
		ASSERT_THAT(IsTrue(ExpectTokenType("\"\"\"line\ntext\"\"\"", ttHeredocStringConstant, TokenLength),
			TEXT("Heredoc string literal should use the expected token type")));
		ASSERT_THAT(AreEqual(15, TokenLength, TEXT("Heredoc string literal should use the expected token length")));
	}

	TEST_METHOD(CharLiteralBasic)
	{
		int32 TokenLength = 0;
		ASSERT_THAT(IsTrue(ExpectTokenType("'a'", ttStringConstant, TokenLength),
			TEXT("Character literal token should use the expected token type")));
		ASSERT_THAT(AreEqual(3, TokenLength, TEXT("Character literal token should use the expected token length")));
	}

	TEST_METHOD(CharLiteralEscape)
	{
		int32 TokenLength = 0;
		ASSERT_THAT(IsTrue(ExpectTokenType("'\\n'", ttStringConstant, TokenLength),
			TEXT("Escaped character literal token should use the expected token type")));
		ASSERT_THAT(AreEqual(4, TokenLength, TEXT("Escaped character literal token should use the expected token length")));
	}

	TEST_METHOD(EmptyStringLiteral)
	{
		int32 TokenLength = 0;
		ASSERT_THAT(IsTrue(ExpectTokenType("\"\"", ttStringConstant, TokenLength),
			TEXT("Empty string literal should use the expected token type")));
		ASSERT_THAT(AreEqual(2, TokenLength, TEXT("Empty string literal should use the expected token length")));
	}

	TEST_METHOD(AdjacentStringConcatNotMerged)
	{
		AngelscriptNativeTestSupport::FTokenizerAccessor Tokenizer;
		const char* Input = "\"a\"\"b\"";
		size_t TokenLength = 0;

		ASSERT_THAT(AreEqual(static_cast<int32>(ttStringConstant), static_cast<int32>(Tokenizer.GetToken(Input, std::strlen(Input), &TokenLength)),
			TEXT("Adjacent string scan should return the first string token only")));
		ASSERT_THAT(AreEqual(3, static_cast<int32>(TokenLength),
			TEXT("Adjacent string scan should not merge the following string token")));
	}
};

#endif
