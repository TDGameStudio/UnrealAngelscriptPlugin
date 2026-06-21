#include "AngelscriptNativeTestSupport.h"
#include "CQTest.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_tokendef.h"
#include "source/as_tokenizer.h"
#include "EndAngelscriptHeaders.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptNativeTestSupport;

TEST_CLASS_WITH_FLAGS(FAngelscriptNativeTokenizerOperatorsTests,
	"Angelscript.TestModule.AngelScriptSDK.Tokenizer.Operators",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	struct FTokenCase
	{
		const char* Input;
		eTokenType ExpectedType;
		int32 ExpectedLength;
		const TCHAR* Description;
	};

	static bool ReadToken(const char* Input, eTokenType& OutTokenType, int32& OutTokenLength)
	{
		FTokenizerAccessor Tokenizer;
		size_t TokenLength = 0;
		OutTokenType = Tokenizer.GetToken(Input, std::strlen(Input), &TokenLength);
		OutTokenLength = static_cast<int32>(TokenLength);
		return true;
	}

	void ExpectTokens(const FTokenCase* Cases, const int32 NumCases)
	{
		for (int32 Index = 0; Index < NumCases; ++Index)
		{
			const FTokenCase& Case = Cases[Index];
			eTokenType TokenType = ttUnrecognizedToken;
			int32 TokenLength = 0;
			ReadToken(Case.Input, TokenType, TokenLength);

			ASSERT_THAT(AreEqual(static_cast<int32>(Case.ExpectedType), static_cast<int32>(TokenType),
				FString::Printf(TEXT("%s should use the expected token type"), Case.Description)));
			ASSERT_THAT(AreEqual(Case.ExpectedLength, TokenLength,
				FString::Printf(TEXT("%s should use the expected token length"), Case.Description)));
		}
	}

public:
	TEST_METHOD(ArithmeticOps_PlusMinusStarSlashPercent)
	{
		const FTokenCase Cases[] = {
			{ "+", ttPlus, 1, TEXT("Plus operator") },
			{ "-", ttMinus, 1, TEXT("Minus operator") },
			{ "*", ttStar, 1, TEXT("Multiply operator") },
			{ "/", ttSlash, 1, TEXT("Divide operator") },
			{ "%", ttPercent, 1, TEXT("Modulo operator") },
			{ "**", ttStarStar, 2, TEXT("Power operator") },
		};
		ExpectTokens(Cases, UE_ARRAY_COUNT(Cases));
	}

	TEST_METHOD(BitwiseOps_AndOrXorNotShifts)
	{
		const FTokenCase Cases[] = {
			{ "&", ttAmp, 1, TEXT("Bitwise and operator") },
			{ "|", ttBitOr, 1, TEXT("Bitwise or operator") },
			{ "^", ttBitXor, 1, TEXT("Bitwise xor operator") },
			{ "~", ttBitNot, 1, TEXT("Bitwise not operator") },
			{ "<<", ttBitShiftLeft, 2, TEXT("Left shift operator") },
			{ ">>", ttBitShiftRight, 2, TEXT("Right shift operator") },
			{ ">>>", ttBitShiftRightArith, 3, TEXT("Arithmetic right shift operator") },
		};
		ExpectTokens(Cases, UE_ARRAY_COUNT(Cases));
	}

	TEST_METHOD(ComparisonOps_EqNeLtLeGtGe)
	{
		const FTokenCase Cases[] = {
			{ "==", ttEqual, 2, TEXT("Equality operator") },
			{ "!=", ttNotEqual, 2, TEXT("Inequality operator") },
			{ "<", ttLessThan, 1, TEXT("Less-than operator") },
			{ "<=", ttLessThanOrEqual, 2, TEXT("Less-than-or-equal operator") },
			{ ">", ttGreaterThan, 1, TEXT("Greater-than operator") },
			{ ">=", ttGreaterThanOrEqual, 2, TEXT("Greater-than-or-equal operator") },
		};
		ExpectTokens(Cases, UE_ARRAY_COUNT(Cases));
	}

	TEST_METHOD(LogicalOps_AndOrNot)
	{
		const FTokenCase Cases[] = {
			{ "&&", ttAnd, 2, TEXT("Logical and operator") },
			{ "||", ttOr, 2, TEXT("Logical or operator") },
			{ "!", ttNot, 1, TEXT("Logical not operator") },
		};
		ExpectTokens(Cases, UE_ARRAY_COUNT(Cases));
	}

	TEST_METHOD(AssignmentOps_PlainAndCompound)
	{
		const FTokenCase Cases[] = {
			{ "=", ttAssignment, 1, TEXT("Plain assignment operator") },
			{ "+=", ttAddAssign, 2, TEXT("Add assignment operator") },
			{ "-=", ttSubAssign, 2, TEXT("Subtract assignment operator") },
			{ "*=", ttMulAssign, 2, TEXT("Multiply assignment operator") },
			{ "/=", ttDivAssign, 2, TEXT("Divide assignment operator") },
			{ "%=", ttModAssign, 2, TEXT("Modulo assignment operator") },
			{ "**=", ttPowAssign, 3, TEXT("Power assignment operator") },
			{ "|=", ttOrAssign, 2, TEXT("Or assignment operator") },
			{ "&=", ttAndAssign, 2, TEXT("And assignment operator") },
			{ "^=", ttXorAssign, 2, TEXT("Xor assignment operator") },
			{ "<<=", ttShiftLeftAssign, 3, TEXT("Left shift assignment operator") },
			{ ">>=", ttShiftRightLAssign, 3, TEXT("Right shift assignment operator") },
			{ ">>>=", ttShiftRightAAssign, 4, TEXT("Arithmetic right shift assignment operator") },
		};
		ExpectTokens(Cases, UE_ARRAY_COUNT(Cases));
	}

	TEST_METHOD(IncrementDecrement)
	{
		const FTokenCase Cases[] = {
			{ "++", ttInc, 2, TEXT("Increment operator") },
			{ "--", ttDec, 2, TEXT("Decrement operator") },
		};
		ExpectTokens(Cases, UE_ARRAY_COUNT(Cases));
	}

	TEST_METHOD(Ternary_Question_Colon)
	{
		const FTokenCase Cases[] = {
			{ "?", ttQuestion, 1, TEXT("Ternary question token") },
			{ ":", ttColon, 1, TEXT("Ternary colon token") },
		};
		ExpectTokens(Cases, UE_ARRAY_COUNT(Cases));
	}

	TEST_METHOD(ScopeColonColonAndDot)
	{
		const FTokenCase Cases[] = {
			{ "::", ttScope, 2, TEXT("Scope operator") },
			{ ".", ttDot, 1, TEXT("Dot operator") },
		};
		ExpectTokens(Cases, UE_ARRAY_COUNT(Cases));
	}

	TEST_METHOD(HandleOpAt)
	{
		FTokenizerAccessor Tokenizer;
		size_t TokenLength = 0;
		const eTokenType TokenType = Tokenizer.GetToken("@", 1, &TokenLength);

		ASSERT_THAT(AreEqual(static_cast<int32>(ttUnrecognizedToken), static_cast<int32>(TokenType),
			TEXT("Raw tokenizer currently leaves @ to parser-level handle handling")));
		ASSERT_THAT(AreEqual(1, static_cast<int32>(TokenLength),
			TEXT("Raw tokenizer should still advance over @")));
	}

	TEST_METHOD(LongestMatchPrefersShiftRAOverShiftR)
	{
		FTokenizerAccessor Tokenizer;
		size_t TokenLength = 0;

		ASSERT_THAT(AreEqual(static_cast<int32>(ttShiftRightAAssign), static_cast<int32>(Tokenizer.GetToken(">>>=", 4, &TokenLength)),
			TEXT("Tokenizer should greedily match >>>= as the arithmetic-right-shift assignment token")));
		ASSERT_THAT(AreEqual(4, static_cast<int32>(TokenLength),
			TEXT("Tokenizer should consume all four characters in >>>=")));
	}
};

#endif
