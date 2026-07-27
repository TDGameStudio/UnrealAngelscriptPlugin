#include "../Support/AngelscriptNativeLanguageCaseTestSupport.h"

#include "AngelscriptTestMacros.h"
#include "CQTest.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_tokenizer.h"
#include "source/as_tokendef.h"
#include "EndAngelscriptHeaders.h"

#if WITH_ANGELSCRIPT_UNITTESTS

namespace AngelscriptTokenizerInternalPrivate
{
	struct FTokenizerAccessor : asCTokenizer
	{
		FTokenizerAccessor()
			: asCTokenizer()
		{
		}

		using asCTokenizer::IsComment;
		using asCTokenizer::IsConstant;
		using asCTokenizer::IsDigitInRadix;
		using asCTokenizer::IsIdentifier;
		using asCTokenizer::IsKeyWord;
		using asCTokenizer::IsWhiteSpace;
		using asCTokenizer::ParseToken;
	};

	inline FString MakeReviewSource(const FString& CaseId, const FString& Input)
	{
		FString Source;
		AngelscriptNativeTestSupport::AppendGeneratedAsLine(Source, TEXT("void TokenizerInternalCase()"));
		AngelscriptNativeTestSupport::AppendGeneratedAsLine(Source, TEXT("{"));
		AngelscriptNativeTestSupport::AppendGeneratedAsLine(
			Source,
			FString::Printf(TEXT("\t// case: %s"), *CaseId));
		AngelscriptNativeTestSupport::AppendGeneratedAsLine(
			Source,
			FString::Printf(TEXT("\t// lexical input: %s"), *Input));
		AngelscriptNativeTestSupport::AppendGeneratedAsLine(Source, TEXT("}"));
		return Source;
	}
}

TEST_CLASS_WITH_FLAGS(FTokenizerInternalTests,
	"Angelscript.TestModule.AngelScriptSDK.Frontend.TokenizerInternal",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(ProtectedHelpersByInputFamily)
	{
		using namespace AngelscriptNativeTestSupport;
		AS_NATIVE_PRODUCT("FRONTEND-TOKENIZER-INTERNAL-HELPERS",
			ENativeEvidence::Metadata
			| ENativeEvidence::Isolation);

		enum class EHelper : uint8
		{
			Whitespace,
			Comment,
			Constant,
			Identifier,
			Keyword,
			Dispatch,
		};

		struct FHelperCase
		{
			const TCHAR* Id;
			const char* Input;
			EHelper Helper;
			asETokenClass ExpectedClass;
			eTokenType ExpectedToken;
			size_t ExpectedLength;
		};
		const char WhitespaceInput[] = { ' ', static_cast<char>(9), static_cast<char>(13), static_cast<char>(10), static_cast<char>(0) };
		const char LineCommentInput[] = { '/', '/', ' ', 'n', 'o', 't', 'e', static_cast<char>(10), static_cast<char>(0) };

		const FHelperCase Cases[] =
		{
			{ TEXT("whitespace"), WhitespaceInput, EHelper::Whitespace, asTC_WHITESPACE, ttWhiteSpace, 4 },
			{ TEXT("line_comment"), LineCommentInput, EHelper::Comment, asTC_COMMENT, ttOnelineComment, 8 },
			{ TEXT("integer_constant"), "42", EHelper::Constant, asTC_VALUE, ttIntConstant, 2 },
			{ TEXT("identifier"), "Value_2", EHelper::Identifier, asTC_IDENTIFIER, ttIdentifier, 7 },
			{ TEXT("keyword"), "class", EHelper::Keyword, asTC_KEYWORD, ttClass, 5 },
			{ TEXT("dispatch"), "0x2A", EHelper::Dispatch, asTC_VALUE, ttBitsConstant, 4 },
		};

		AngelscriptTokenizerInternalPrivate::FTokenizerAccessor Tokenizer;
		int32 ObservedCaseCount = 0;
		for (const FHelperCase& Case : Cases)
		{
			const FString CaseId = MakeNativeCaseId(
				"FRONTEND-TOKENIZER-INTERNAL-HELPERS",
				{ Case.Id });
			const FString InputText = UTF8_TO_TCHAR(Case.Input);
			PrintGeneratedAsSource(
				*TestRunner,
				CaseId,
				FString::Printf(TEXT("TokenizerInternal_%s"), Case.Id),
				AngelscriptTokenizerInternalPrivate::MakeReviewSource(CaseId, InputText));

			size_t TokenLength = 0;
			eTokenType TokenType = ttUnrecognizedToken;
			asETokenClass TokenClass = asTC_UNKNOWN;
			bool bMatched = false;
			switch (Case.Helper)
			{
			case EHelper::Whitespace:
				bMatched = Tokenizer.IsWhiteSpace(Case.Input, std::strlen(Case.Input), TokenLength, TokenType);
				TokenClass = asTC_WHITESPACE;
				break;
			case EHelper::Comment:
				bMatched = Tokenizer.IsComment(Case.Input, std::strlen(Case.Input), TokenLength, TokenType);
				TokenClass = asTC_COMMENT;
				break;
			case EHelper::Constant:
				bMatched = Tokenizer.IsConstant(Case.Input, std::strlen(Case.Input), TokenLength, TokenType);
				TokenClass = asTC_VALUE;
				break;
			case EHelper::Identifier:
				bMatched = Tokenizer.IsIdentifier(Case.Input, std::strlen(Case.Input), TokenLength, TokenType);
				TokenClass = asTC_IDENTIFIER;
				break;
			case EHelper::Keyword:
				bMatched = Tokenizer.IsKeyWord(Case.Input, std::strlen(Case.Input), TokenLength, TokenType);
				TokenClass = asTC_KEYWORD;
				break;
			case EHelper::Dispatch:
				TokenClass = Tokenizer.ParseToken(Case.Input, std::strlen(Case.Input), TokenLength, TokenType);
				bMatched = true;
				break;
			}

			ASSERT_THAT(IsTrue(bMatched, FString::Printf(TEXT("%s should match its internal tokenizer helper"), *CaseId)));
			ASSERT_THAT(AreEqual(static_cast<int32>(Case.ExpectedClass), static_cast<int32>(TokenClass),
				FString::Printf(TEXT("%s should publish the expected token class"), *CaseId)));
			ASSERT_THAT(AreEqual(static_cast<int32>(Case.ExpectedToken), static_cast<int32>(TokenType),
				FString::Printf(TEXT("%s should publish the expected token kind"), *CaseId)));
			ASSERT_THAT(AreEqual(static_cast<int32>(Case.ExpectedLength), static_cast<int32>(TokenLength),
				FString::Printf(TEXT("%s should consume the expected span"), *CaseId)));
			++ObservedCaseCount;
		}

		ASSERT_THAT(AreEqual(6, ObservedCaseCount,
			TEXT("The tokenizer internal-helper catalog should execute every helper family once")));
	}

	TEST_METHOD(RadixDigitHelperByCharacterAndRadix)
	{
		using namespace AngelscriptNativeTestSupport;
		AS_NATIVE_PRODUCT("FRONTEND-TOKENIZER-RADIX-HELPER",
			ENativeEvidence::Metadata
			| ENativeEvidence::Isolation);

		struct FCharacterCase
		{
			const TCHAR* Id;
			char Value;
			int32 NumericValue;
		};
		const FCharacterCase Characters[] =
		{
			{ TEXT("zero"), '0', 0 },
			{ TEXT("seven"), '7', 7 },
			{ TEXT("eight"), '8', 8 },
			{ TEXT("nine"), '9', 9 },
			{ TEXT("upper_a"), 'A', 10 },
			{ TEXT("upper_f"), 'F', 15 },
			{ TEXT("lower_a"), 'a', 10 },
			{ TEXT("lower_f"), 'f', 15 },
			{ TEXT("upper_g"), 'G', 16 },
			{ TEXT("lower_g"), 'g', 16 },
			{ TEXT("minus"), '-', -1 },
			{ TEXT("space"), ' ', -1 },
		};
		const int32 Radices[] = { 2, 8, 10, 16 };

		AngelscriptTokenizerInternalPrivate::FTokenizerAccessor Tokenizer;
		int32 ObservedCaseCount = 0;
		for (const FCharacterCase& Character : Characters)
		{
			for (const int32 Radix : Radices)
			{
				const FString RadixText = FString::FromInt(Radix);
				const FString CaseId = MakeNativeCaseId(
					"FRONTEND-TOKENIZER-RADIX-HELPER",
					{ Character.Id, *RadixText });
				const FString InputText = FString::Printf(TEXT("%c in radix %d"), Character.Value, Radix);
				PrintGeneratedAsSource(
					*TestRunner,
					CaseId,
					FString::Printf(TEXT("TokenizerRadix_%s_%d"), Character.Id, Radix),
					AngelscriptTokenizerInternalPrivate::MakeReviewSource(CaseId, InputText));

				const bool bExpected = Character.NumericValue >= 0 && Character.NumericValue < Radix;
				ASSERT_THAT(AreEqual(bExpected, Tokenizer.IsDigitInRadix(Character.Value, Radix),
					FString::Printf(TEXT("%s should classify the digit for radix %d"), *CaseId, Radix)));
				++ObservedCaseCount;
			}
		}

		ASSERT_THAT(AreEqual(48, ObservedCaseCount,
			TEXT("The radix helper should execute every character/radix pair")));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
