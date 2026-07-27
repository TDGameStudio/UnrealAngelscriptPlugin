#include "../Support/AngelscriptNativeCoreTestSupport.h"
#include "../Support/AngelscriptNativeCaseTestSupport.h"

// Core tokenizer behavior coverage.
#include "CQTest.h"
#include "Misc/ScopeExit.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_tokenizer.h"
#include "source/as_tokendef.h"
#include "EndAngelscriptHeaders.h"

#if WITH_ANGELSCRIPT_UNITTESTS



TEST_CLASS_WITH_FLAGS(FTokenizerCoreTests,
	"Angelscript.TestModule.AngelScriptSDK.Frontend.Tokenizer",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(TokenizerCoreBasicTokens)
	{
		AS_NATIVE_NON_PRODUCT("LegacyCompatibility",
			"Retained cross-family tokenizer smoke; the FRONTEND-TOKEN-NUMERIC, TEXT, OPERATOR, and TAXONOMY products own complete family contracts.");

		AngelscriptNativeTestSupport::FTokenizerAccessor Tokenizer;
		size_t TokenLength = 0;

		ASSERT_THAT(AreEqual(static_cast<int32>(ttIdentifier), static_cast<int32>(Tokenizer.GetToken("Identifier123", 13, &TokenLength)),
			TEXT("Identifier token type should be recognized")));
		ASSERT_THAT(AreEqual(13, static_cast<int32>(TokenLength),
			TEXT("Identifier token length should be returned")));

		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};
		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(
			ScriptEngine,
			TEXT("Public tokenizer contract should create a raw SDK engine")));
		if (ScriptEngine != nullptr)
		{
			asUINT PublicTokenLength = 0;
			ASSERT_THAT(AreEqual(
				static_cast<int32>(asTC_IDENTIFIER),
				static_cast<int32>(ScriptEngine->ParseToken(
					"Identifier123",
					13,
					&PublicTokenLength)),
				TEXT("asIScriptEngine ParseToken should publish the identifier token class")));
			ASSERT_THAT(AreEqual(
				13,
				static_cast<int32>(PublicTokenLength),
				TEXT("asIScriptEngine ParseToken should publish the exact consumed length")));
		}

		ASSERT_THAT(AreEqual(static_cast<int32>(ttIntConstant), static_cast<int32>(Tokenizer.GetToken("12345", 5, &TokenLength)),
			TEXT("Integer literal token type should be recognized")));
		ASSERT_THAT(AreEqual(5, static_cast<int32>(TokenLength),
			TEXT("Integer literal token length should be returned")));

		ASSERT_THAT(AreEqual(static_cast<int32>(ttStringConstant), static_cast<int32>(Tokenizer.GetToken("\"abc\"", 5, &TokenLength)),
			TEXT("String literal token type should be recognized")));
		ASSERT_THAT(AreEqual(5, static_cast<int32>(TokenLength),
			TEXT("String literal token length should be returned")));

		ASSERT_THAT(AreEqual(static_cast<int32>(ttPlus), static_cast<int32>(Tokenizer.GetToken("+", 1, &TokenLength)),
			TEXT("Operator token type should be recognized")));
		ASSERT_THAT(AreEqual(1, static_cast<int32>(TokenLength),
			TEXT("Operator token length should be returned")));
	}

	TEST_METHOD(TokenizerCoreKeywords)
	{
		AS_NATIVE_NON_PRODUCT("LegacyCompatibility",
			"Retained representative keyword smoke; FRONTEND-TOKEN-TAXONOMY-ACTIVE-KEYWORDS owns all active keyword spellings and identifier boundaries.");

		AngelscriptNativeTestSupport::FTokenizerAccessor Tokenizer;
		size_t TokenLength = 0;

		ASSERT_THAT(AreEqual(static_cast<int32>(ttClass), static_cast<int32>(Tokenizer.GetToken("class", 5, &TokenLength)),
			TEXT("class should be recognized as a keyword token")));
		ASSERT_THAT(AreEqual(static_cast<int32>(ttVoid), static_cast<int32>(Tokenizer.GetToken("void", 4, &TokenLength)),
			TEXT("void should be recognized as a keyword token")));
		ASSERT_THAT(AreEqual(static_cast<int32>(ttInt), static_cast<int32>(Tokenizer.GetToken("int", 3, &TokenLength)),
			TEXT("int should be recognized as a keyword token")));
		ASSERT_THAT(AreEqual(static_cast<int32>(ttFloat32), static_cast<int32>(Tokenizer.GetToken("float32", 7, &TokenLength)),
			TEXT("float32 should be recognized as a keyword token")));
	}

	TEST_METHOD(CommentsAndStrings)
	{
		AS_NATIVE_NON_PRODUCT("LegacyCompatibility",
			"Retained comment/string smoke; FRONTEND-TOKEN-COMMENT-WHITESPACE-EOF and FRONTEND-TOKEN-TEXT-ESCAPE-LINE-ENDINGS own complete boundaries.");

		AngelscriptNativeTestSupport::FTokenizerAccessor Tokenizer;
		size_t TokenLength = 0;

		ASSERT_THAT(AreEqual(static_cast<int32>(ttOnelineComment), static_cast<int32>(Tokenizer.GetToken("// hello\n", 9, &TokenLength)),
			TEXT("Single line comment should be recognized")));
		ASSERT_THAT(AreEqual(static_cast<int32>(ttMultilineComment), static_cast<int32>(Tokenizer.GetToken("/* hi */", 8, &TokenLength)),
			TEXT("Multi line comment should be recognized")));
		ASSERT_THAT(AreEqual(static_cast<int32>(ttStringConstant), static_cast<int32>(Tokenizer.GetToken("\"first\\nsecond\"", 15, &TokenLength)),
			TEXT("Multiline string should be recognized")));
	}

	TEST_METHOD(TokenizerCoreErrorRecovery)
	{
		AS_NATIVE_NON_PRODUCT("LegacyCompatibility",
			"Retained error token smoke; FRONTEND-TOKEN-TEXT-COMMENT-WHITESPACE-BOUNDARIES and malformed-recovery products own located token recovery.");

		AngelscriptNativeTestSupport::FTokenizerAccessor Tokenizer;
		size_t TokenLength = 0;

		ASSERT_THAT(AreEqual(static_cast<int32>(ttNonTerminatedStringConstant), static_cast<int32>(Tokenizer.GetToken("\"unterminated", 13, &TokenLength)),
			TEXT("Unterminated string should produce the dedicated token type")));
		ASSERT_THAT(AreEqual(static_cast<int32>(ttUnrecognizedToken), static_cast<int32>(Tokenizer.GetToken("`", 1, &TokenLength)),
			TEXT("Unknown characters should produce an unrecognized token")));
	}

	TEST_METHOD(ErrorRecoveryAdvancesAndContinues)
	{
		AS_NATIVE_NON_PRODUCT("LegacyCompatibility",
			"Retained invalid-character continuation smoke; FRONTEND-TOKEN-OPERATOR-MALFORMED-RECOVERY owns malformed prefix and following-token recovery.");

		AngelscriptNativeTestSupport::FTokenizerAccessor Tokenizer;
		size_t TokenLength = 0;
		const char* Input = "`class";
		const size_t InputLength = 6;

		const int32 FirstTokenType = static_cast<int32>(Tokenizer.GetToken(Input, InputLength, &TokenLength));
		ASSERT_THAT(AreEqual(static_cast<int32>(ttUnrecognizedToken), FirstTokenType,
			TEXT("Tokenizer recovery should classify the leading invalid character as an unrecognized token")));

		ASSERT_THAT(AreEqual(1, static_cast<int32>(TokenLength),
			TEXT("Tokenizer recovery should still advance by one character for the invalid token")));

		const char* ContinuedInput = Input + TokenLength;
		const size_t ContinuedLength = InputLength - TokenLength;
		const int32 ContinuedTokenType = static_cast<int32>(Tokenizer.GetToken(ContinuedInput, ContinuedLength, &TokenLength));
		ASSERT_THAT(AreEqual(static_cast<int32>(ttClass), ContinuedTokenType,
			TEXT("Tokenizer recovery should continue scanning and recognize the trailing class keyword")));
		ASSERT_THAT(AreEqual(5, static_cast<int32>(TokenLength),
			TEXT("Tokenizer recovery should return the full trailing keyword length after advancing")));
	}

	TEST_METHOD(LiteralAndPunctuationTokensHaveExpectedKinds)
	{
		AS_NATIVE_NON_PRODUCT("LegacyCompatibility",
			"Retained representative literal/punctuation smoke; numeric, text, operator, and token-definition products own complete published kinds.");

		AngelscriptNativeTestSupport::FTokenizerAccessor Tokenizer;
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
			ASSERT_THAT(AreEqual(Case.ExpectedType, static_cast<int32>(Tokenizer.GetToken(Case.Input, Case.InputLength, &TokenLength)),
				FString::Printf(TEXT("%s should use the expected token type"), Case.Description)));
			ASSERT_THAT(AreEqual(Case.ExpectedLength, static_cast<int32>(TokenLength),
				FString::Printf(TEXT("%s should use the expected token length"), Case.Description)));
		}
	}

	TEST_METHOD(UnterminatedBlockCommentAndEscapes)
	{
		AS_NATIVE_NON_PRODUCT("LegacyCompatibility",
			"Retained comment/string boundary smoke; FRONTEND-TOKEN-COMMENT-WHITESPACE-EOF and FRONTEND-TOKEN-TEXT-ESCAPE-LINE-ENDINGS own full termination and escape contracts.");

		AngelscriptNativeTestSupport::FTokenizerAccessor Tokenizer;
		size_t TokenLength = 0;

		const char UnterminatedBlockComment[] = "/* comment";
		const size_t UnterminatedBlockCommentLength = sizeof(UnterminatedBlockComment) - 1;
		ASSERT_THAT(AreEqual(static_cast<int32>(ttMultilineComment), static_cast<int32>(Tokenizer.GetToken(UnterminatedBlockComment, UnterminatedBlockCommentLength, &TokenLength)),
			TEXT("Unterminated block comment should still be classified as a multiline comment token")));
		ASSERT_THAT(AreEqual(static_cast<int32>(UnterminatedBlockCommentLength), static_cast<int32>(TokenLength),
			TEXT("Unterminated block comment should consume the entire source length")));

		const char EscapedStringInput[] = "\"escaped \\\"quote\\\" and \\\\ slash\"+";
		const size_t EscapedStringInputLength = sizeof(EscapedStringInput) - 1;
		const size_t ExpectedEscapedStringTokenLength = sizeof("\"escaped \\\"quote\\\" and \\\\ slash\"") - 1;
		ASSERT_THAT(AreEqual(static_cast<int32>(ttStringConstant), static_cast<int32>(Tokenizer.GetToken(EscapedStringInput, EscapedStringInputLength, &TokenLength)),
			TEXT("Escaped quote and backslash string should remain a string token")));
		ASSERT_THAT(AreEqual(static_cast<int32>(ExpectedEscapedStringTokenLength), static_cast<int32>(TokenLength),
			TEXT("Escaped quote and backslash string should stop at the closing quote rather than the trailing operator")));
		ASSERT_THAT(IsTrue(TokenLength < EscapedStringInputLength,
			TEXT("Escaped quote and backslash string should leave trailing input for the next token")));
	}
};

#endif
