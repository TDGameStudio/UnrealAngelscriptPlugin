#include "../Support/AngelscriptNativeTokenizerTestSupport.h"

#include "CQTest.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FTokenizerOperatorDepthTests,
	"Angelscript.TestModule.AngelScriptSDK.Frontend.Tokenizer.DeepCoverage",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	struct FOperatorOperandCase
	{
		const char* CatalogName;
		const char* Input;
		eTokenType ExpectedType;
		int32 ExpectedLength;
		const TCHAR* Description;
	};

	struct FOperatorSymbolCase
	{
		const char* CatalogName;
		const char* Input;
		eTokenType ExpectedType;
		int32 ExpectedLength;
		const TCHAR* Description;
	};

	struct FOperatorSpacingCase
	{
		const char* CatalogName;
		const char* Left;
		const char* Right;
		const TCHAR* Description;
	};

	struct FOperatorRecoveryCase
	{
		const char* CatalogName;
		const char* Prefix;
		const TCHAR* Description;
	};

	struct FRecoveryTailCase
	{
		const char* CatalogName;
		const char* Input;
		eTokenType ExpectedType;
		int32 ExpectedLength;
		const TCHAR* Description;
	};

public:
	TEST_METHOD(LongestMatchCoversOperatorPrefixAndSuffixBoundaries)
	{
		using namespace AngelscriptNativeTestSupport;
		AS_NATIVE_PRODUCT("FRONTEND-TOKEN-LONGEST-MATCH-OPERATORS",
			ENativeEvidence::Metadata
			| ENativeEvidence::Diagnostic);

		const FTokenCase OperatorCases[] =
		{
			{ "/", ttSlash, 1, TEXT("slash") },
			{ "/=", ttDivAssign, 2, TEXT("divide assignment") },
			{ "%", ttPercent, 1, TEXT("percent") },
			{ "%=", ttModAssign, 2, TEXT("modulo assignment") },
			{ "+", ttPlus, 1, TEXT("plus") },
			{ "+=", ttAddAssign, 2, TEXT("add assignment") },
			{ "++", ttInc, 2, TEXT("increment") },
			{ "-", ttMinus, 1, TEXT("minus") },
			{ "-=", ttSubAssign, 2, TEXT("subtract assignment") },
			{ "--", ttDec, 2, TEXT("decrement") },
			{ "*", ttStar, 1, TEXT("multiply") },
			{ "*=", ttMulAssign, 2, TEXT("multiply assignment") },
			{ "**", ttStarStar, 2, TEXT("power") },
			{ "**=", ttPowAssign, 3, TEXT("power assignment") },
			{ "|", ttBitOr, 1, TEXT("bitwise or") },
			{ "|=", ttOrAssign, 2, TEXT("or assignment") },
			{ "||", ttOr, 2, TEXT("logical or") },
			{ "&", ttAmp, 1, TEXT("bitwise and") },
			{ "&=", ttAndAssign, 2, TEXT("and assignment") },
			{ "&&", ttAnd, 2, TEXT("logical and") },
			{ "^", ttBitXor, 1, TEXT("bitwise xor") },
			{ "^=", ttXorAssign, 2, TEXT("xor assignment") },
			{ "^^", ttXor, 2, TEXT("logical xor") },
			{ "<", ttLessThan, 1, TEXT("less than") },
			{ "<=", ttLessThanOrEqual, 2, TEXT("less than or equal") },
			{ "<<", ttBitShiftLeft, 2, TEXT("left shift") },
			{ "<<=", ttShiftLeftAssign, 3, TEXT("left shift assignment") },
			{ ">", ttGreaterThan, 1, TEXT("greater than") },
			{ ">=", ttGreaterThanOrEqual, 2, TEXT("greater than or equal") },
			{ ">>", ttBitShiftRight, 2, TEXT("right shift") },
			{ ">>=", ttShiftRightLAssign, 3, TEXT("right shift assignment") },
			{ ">>>", ttBitShiftRightArith, 3, TEXT("arithmetic right shift") },
			{ ">>>=", ttShiftRightAAssign, 4, TEXT("arithmetic right shift assignment") },
			{ "=", ttAssignment, 1, TEXT("assignment") },
			{ "==", ttEqual, 2, TEXT("equality") },
			{ "!", ttNot, 1, TEXT("logical not") },
			{ "!=", ttNotEqual, 2, TEXT("not equal") },
			{ "@", ttUnrecognizedToken, 1, TEXT("handle sigil is fork-rejected"), false, true },
			{ "~", ttBitNot, 1, TEXT("bitwise not") },
			{ ".", ttDot, 1, TEXT("dot") },
			{ "::", ttScope, 2, TEXT("scope") },
			{ ";", ttEndStatement, 1, TEXT("statement terminator") },
			{ ",", ttListSeparator, 1, TEXT("list separator") },
			{ "{", ttStartStatementBlock, 1, TEXT("statement block start") },
			{ "}", ttEndStatementBlock, 1, TEXT("statement block end") },
			{ "(", ttOpenParanthesis, 1, TEXT("parenthesis open") },
			{ ")", ttCloseParanthesis, 1, TEXT("parenthesis close") },
			{ "[", ttOpenBracket, 1, TEXT("bracket open") },
			{ "]", ttCloseBracket, 1, TEXT("bracket close") },
			{ "?", ttQuestion, 1, TEXT("question") },
			{ ":", ttColon, 1, TEXT("colon") },
		};

		FString GeneratedSource;
		AngelscriptNativeTestSupport::AppendGeneratedAsLine(GeneratedSource, TEXT("int GeneratedOperatorCorpus()"));
		AngelscriptNativeTestSupport::AppendGeneratedAsLine(GeneratedSource, TEXT("{"));
		AngelscriptNativeTestSupport::AppendGeneratedAsLine(GeneratedSource, TEXT("\tint Result = 0;"));

		for (const FTokenCase& Case : OperatorCases)
		{
			AppendCommentedCase(GeneratedSource,
				Case.bForkRejected ? TEXT("fork-rejected operator spelling") : TEXT("operator"),
				Case.Input);
			const FTokenObservation Observation = ReadToken(Case.Input);
			ASSERT_THAT(AreEqual(static_cast<int32>(Case.ExpectedType), static_cast<int32>(Observation.Type),
				FString::Printf(TEXT("%s should retain its expected token kind"), Case.Description)));
			ASSERT_THAT(AreEqual(Case.ExpectedLength, Observation.Length,
				FString::Printf(TEXT("%s should consume its expected token span"), Case.Description)));

			std::string WithIdentifierSuffix(Case.Input);
			WithIdentifierSuffix += "Value";
			const FTokenObservation SuffixObservation = ReadToken(WithIdentifierSuffix.c_str());
			ASSERT_THAT(AreEqual(static_cast<int32>(Case.ExpectedType), static_cast<int32>(SuffixObservation.Type),
				FString::Printf(TEXT("%s followed by an identifier must preserve the operator"), Case.Description)));
			ASSERT_THAT(AreEqual(Case.ExpectedLength, SuffixObservation.Length,
				FString::Printf(TEXT("%s followed by an identifier must not consume the identifier"), Case.Description)));
		}

		const FTokenCase ShiftTailCases[] =
		{
			{ ">>>==", ttShiftRightAAssign, 4, TEXT("arithmetic shift assignment with assignment tail") },
			{ ">>>=Value", ttShiftRightAAssign, 4, TEXT("arithmetic shift assignment with name tail") },
			{ "<<=Value", ttShiftLeftAssign, 3, TEXT("left shift assignment with name tail") },
			{ "**=Value", ttPowAssign, 3, TEXT("power assignment with name tail") },
		};
		for (const FTokenCase& Case : ShiftTailCases)
		{
			AppendCommentedCase(GeneratedSource, TEXT("operator tail input"), Case.Input);
			const FTokenObservation Observation = ReadToken(Case.Input);
			ASSERT_THAT(AreEqual(static_cast<int32>(Case.ExpectedType), static_cast<int32>(Observation.Type),
				FString::Printf(TEXT("%s should use the longest operator token"), Case.Description)));
			ASSERT_THAT(AreEqual(Case.ExpectedLength, Observation.Length,
				FString::Printf(TEXT("%s should leave the tail for the next token"), Case.Description)));
		}

		AngelscriptNativeTestSupport::AppendGeneratedAsLine(GeneratedSource, TEXT("\treturn Result;"));
		AngelscriptNativeTestSupport::AppendGeneratedAsLine(GeneratedSource, TEXT("}"));
		AngelscriptNativeTestSupport::PrintGeneratedAsSource(
			*TestRunner,
			TEXT("FRONTEND-TOKEN-LONGEST-MATCH-OPERATORS"),
			TEXT("AS_SDK_FrontendTokenizerDeep_Operators"),
			GeneratedSource);
	}

	TEST_METHOD(OperatorOperandSpacingCartesianProduct)
	{
		using namespace AngelscriptNativeTestSupport;
		AS_NATIVE_PRODUCT("FRONTEND-TOKEN-OPERATOR-OPERAND-SPACING",
			ENativeEvidence::Metadata
			| ENativeEvidence::Diagnostic);

		const FOperatorSymbolCase OperatorCases[] =
		{
			{ "slash", "/", ttSlash, 1, TEXT("slash") },
			{ "slash_assign", "/=", ttDivAssign, 2, TEXT("divide assignment") },
			{ "percent", "%", ttPercent, 1, TEXT("percent") },
			{ "percent_assign", "%=", ttModAssign, 2, TEXT("modulo assignment") },
			{ "plus", "+", ttPlus, 1, TEXT("plus") },
			{ "plus_assign", "+=", ttAddAssign, 2, TEXT("add assignment") },
			{ "increment", "++", ttInc, 2, TEXT("increment") },
			{ "minus", "-", ttMinus, 1, TEXT("minus") },
			{ "minus_assign", "-=", ttSubAssign, 2, TEXT("subtract assignment") },
			{ "decrement", "--", ttDec, 2, TEXT("decrement") },
			{ "multiply", "*", ttStar, 1, TEXT("multiply") },
			{ "multiply_assign", "*=", ttMulAssign, 2, TEXT("multiply assignment") },
			{ "power", "**", ttStarStar, 2, TEXT("power") },
			{ "power_assign", "**=", ttPowAssign, 3, TEXT("power assignment") },
			{ "bitwise_or", "|", ttBitOr, 1, TEXT("bitwise or") },
			{ "or_assign", "|=", ttOrAssign, 2, TEXT("or assignment") },
			{ "logical_or", "||", ttOr, 2, TEXT("logical or") },
			{ "bitwise_and", "&", ttAmp, 1, TEXT("bitwise and") },
			{ "and_assign", "&=", ttAndAssign, 2, TEXT("and assignment") },
			{ "logical_and", "&&", ttAnd, 2, TEXT("logical and") },
			{ "bitwise_xor", "^", ttBitXor, 1, TEXT("bitwise xor") },
			{ "xor_assign", "^=", ttXorAssign, 2, TEXT("xor assignment") },
			{ "logical_xor", "^^", ttXor, 2, TEXT("logical xor") },
			{ "less", "<", ttLessThan, 1, TEXT("less than") },
			{ "less_equal", "<=", ttLessThanOrEqual, 2, TEXT("less than or equal") },
			{ "left_shift", "<<", ttBitShiftLeft, 2, TEXT("left shift") },
			{ "left_shift_assign", "<<=", ttShiftLeftAssign, 3, TEXT("left shift assignment") },
			{ "greater", ">", ttGreaterThan, 1, TEXT("greater than") },
			{ "greater_equal", ">=", ttGreaterThanOrEqual, 2, TEXT("greater than or equal") },
			{ "right_shift", ">>", ttBitShiftRight, 2, TEXT("right shift") },
			{ "right_shift_assign", ">>=", ttShiftRightLAssign, 3, TEXT("right shift assignment") },
			{ "arithmetic_right_shift", ">>>", ttBitShiftRightArith, 3, TEXT("arithmetic right shift") },
			{ "arithmetic_right_shift_assign", ">>>=", ttShiftRightAAssign, 4, TEXT("arithmetic right shift assignment") },
			{ "assignment", "=", ttAssignment, 1, TEXT("assignment") },
			{ "equality", "==", ttEqual, 2, TEXT("equality") },
			{ "logical_not", "!", ttNot, 1, TEXT("logical not") },
			{ "not_equal", "!=", ttNotEqual, 2, TEXT("not equal") },
			{ "bitwise_not", "~", ttBitNot, 1, TEXT("bitwise not") },
			{ "dot", ".", ttDot, 1, TEXT("dot") },
			{ "scope", "::", ttScope, 2, TEXT("scope") },
		};

		const FOperatorOperandCase LeftOperands[] =
		{
			{ "identifier", "Value", ttIdentifier, 5, TEXT("identifier left operand") },
			{ "integer", "42", ttIntConstant, 2, TEXT("integer left operand") },
			{ "close_parenthesis", ")", ttCloseParanthesis, 1, TEXT("closing parenthesis left operand") },
			{ "comma", ",", ttListSeparator, 1, TEXT("comma left operand") },
		};

		const FOperatorOperandCase RightOperands[] =
		{
			{ "identifier", "Next", ttIdentifier, 4, TEXT("identifier right operand") },
			{ "integer", "7", ttIntConstant, 1, TEXT("integer right operand") },
			{ "open_parenthesis", "(", ttOpenParanthesis, 1, TEXT("opening parenthesis right operand") },
			{ "statement_end", ";", ttEndStatement, 1, TEXT("statement terminator right operand") },
		};

		const FOperatorSpacingCase SpacingCases[] =
		{
			{ "none", "", "", TEXT("no whitespace") },
			{ "left", " ", "", TEXT("left whitespace") },
			{ "right", "", " ", TEXT("right whitespace") },
			{ "both", " ", " ", TEXT("both-side whitespace") },
		};

		FString GeneratedSource;
		AppendGeneratedAsLine(GeneratedSource, TEXT("int GeneratedOperatorOperandSpacingCell()"));
		AppendGeneratedAsLine(GeneratedSource, TEXT("{"));
		AppendGeneratedAsLine(GeneratedSource, TEXT("\treturn 0;"));
		AppendGeneratedAsLine(GeneratedSource, TEXT("}"));

		int32 ObservedCaseCount = 0;
		for (const FOperatorSymbolCase& OperatorCase : OperatorCases)
		{
			for (const FOperatorOperandCase& LeftOperand : LeftOperands)
			{
				for (const FOperatorOperandCase& RightOperand : RightOperands)
				{
					for (const FOperatorSpacingCase& SpacingCase : SpacingCases)
					{
						std::string Input(LeftOperand.Input);
						Input += SpacingCase.Left;
						Input += OperatorCase.Input;
						Input += SpacingCase.Right;
						Input += RightOperand.Input;

						const FString CaseId = MakeNativeCaseId(
							"FRONTEND-TOKEN-OPERATOR-OPERAND-SPACING",
							{ ANSI_TO_TCHAR(OperatorCase.CatalogName), ANSI_TO_TCHAR(LeftOperand.CatalogName), ANSI_TO_TCHAR(RightOperand.CatalogName), ANSI_TO_TCHAR(SpacingCase.CatalogName) });
						AppendCommentedCase(GeneratedSource, TEXT("operator context input"), Input.c_str());
						AppendGeneratedAsLine(GeneratedSource,
							FString::Printf(TEXT("\t// CaseId: %s"), *CaseId));

						const int32 LeftOffset = static_cast<int32>(std::strlen(LeftOperand.Input));
						const int32 OperatorOffset = LeftOffset + static_cast<int32>(std::strlen(SpacingCase.Left));
						const bool bDotOperator = FCStringAnsi::Strcmp(OperatorCase.CatalogName, "dot") == 0;
						const bool bLeftNumericDotMerge = bDotOperator
							&& FCStringAnsi::Strcmp(LeftOperand.CatalogName, "integer") == 0
							&& SpacingCase.Left[0] == '\0';
						const bool bRightNumericDotMerge = bDotOperator
							&& !bLeftNumericDotMerge
							&& FCStringAnsi::Strcmp(RightOperand.CatalogName, "integer") == 0
							&& SpacingCase.Right[0] == '\0';
						const int32 LeftNumericDotLength = bLeftNumericDotMerge
							? 3 + ((FCStringAnsi::Strcmp(RightOperand.CatalogName, "integer") == 0 && SpacingCase.Right[0] == '\0') ? RightOperand.ExpectedLength : 0)
							: 0;
						const int32 RightTokenOffset = bLeftNumericDotMerge
							? LeftNumericDotLength + static_cast<int32>(std::strlen(SpacingCase.Right))
							: OperatorOffset + (bRightNumericDotMerge ? 2 : OperatorCase.ExpectedLength) + static_cast<int32>(std::strlen(SpacingCase.Right));

						if (bLeftNumericDotMerge)
						{
							const FTokenObservation MergedObservation = ReadToken(Input.c_str());
							ASSERT_THAT(AreEqual(static_cast<int32>(ttFloat64Constant), static_cast<int32>(MergedObservation.Type),
								FString::Printf(TEXT("dot with a numeric left operand should enter the float-literal path in %s"), *CaseId)));
							ASSERT_THAT(AreEqual(LeftNumericDotLength, MergedObservation.Length,
								FString::Printf(TEXT("dot with a numeric left operand should consume the merged float spelling in %s"), *CaseId)));
						}
						else
						{
							const FTokenObservation LeftObservation = ReadToken(Input.c_str());
							ASSERT_THAT(AreEqual(static_cast<int32>(LeftOperand.ExpectedType), static_cast<int32>(LeftObservation.Type),
								FString::Printf(TEXT("%s should remain the left token in %s"), LeftOperand.Description, *CaseId)));
							ASSERT_THAT(AreEqual(LeftOperand.ExpectedLength, LeftObservation.Length,
								FString::Printf(TEXT("%s should retain its left length in %s"), LeftOperand.Description, *CaseId)));
						}

						if (SpacingCase.Left[0] != '\0')
						{
							const FTokenObservation LeftSpacingObservation = ReadToken(Input.c_str() + LeftOffset);
							ASSERT_THAT(AreEqual(static_cast<int32>(ttWhiteSpace), static_cast<int32>(LeftSpacingObservation.Type),
								FString::Printf(TEXT("left spacing should remain separate in %s"), *CaseId)));
							ASSERT_THAT(AreEqual(1, LeftSpacingObservation.Length,
								FString::Printf(TEXT("left spacing should consume one byte in %s"), *CaseId)));
						}

						if (!bLeftNumericDotMerge)
						{
							const FTokenObservation OperatorObservation = ReadToken(Input.c_str() + OperatorOffset);
							if (bRightNumericDotMerge)
							{
								ASSERT_THAT(AreEqual(static_cast<int32>(ttFloat64Constant), static_cast<int32>(OperatorObservation.Type),
									FString::Printf(TEXT("dot before a numeric right operand should enter the leading-dot float path in %s"), *CaseId)));
								ASSERT_THAT(AreEqual(2, OperatorObservation.Length,
									FString::Printf(TEXT("leading-dot float should consume the dot and numeric operand in %s"), *CaseId)));
							}
							else
							{
								ASSERT_THAT(AreEqual(static_cast<int32>(OperatorCase.ExpectedType), static_cast<int32>(OperatorObservation.Type),
									FString::Printf(TEXT("%s should win longest-match selection in %s"), OperatorCase.Description, *CaseId)));
								ASSERT_THAT(AreEqual(OperatorCase.ExpectedLength, OperatorObservation.Length,
									FString::Printf(TEXT("%s should consume its exact spelling in %s"), OperatorCase.Description, *CaseId)));
							}
						}

						if (SpacingCase.Right[0] != '\0')
						{
							const int32 RightSpacingOffset = bLeftNumericDotMerge
								? LeftNumericDotLength
								: OperatorOffset + (bRightNumericDotMerge ? 2 : OperatorCase.ExpectedLength);
							const FTokenObservation RightSpacingObservation = ReadToken(Input.c_str() + RightSpacingOffset);
							ASSERT_THAT(AreEqual(static_cast<int32>(ttWhiteSpace), static_cast<int32>(RightSpacingObservation.Type),
								FString::Printf(TEXT("right spacing should remain separate in %s"), *CaseId)));
							ASSERT_THAT(AreEqual(1, RightSpacingObservation.Length,
								FString::Printf(TEXT("right spacing should consume one byte in %s"), *CaseId)));
						}

						const bool bRightOperandConsumedByDot = bRightNumericDotMerge
							|| (bLeftNumericDotMerge && FCStringAnsi::Strcmp(RightOperand.CatalogName, "integer") == 0 && SpacingCase.Right[0] == '\0');
						if (!bRightOperandConsumedByDot)
						{
							const FTokenObservation RightObservation = ReadToken(Input.c_str() + RightTokenOffset);
							ASSERT_THAT(AreEqual(static_cast<int32>(RightOperand.ExpectedType), static_cast<int32>(RightObservation.Type),
								FString::Printf(TEXT("%s should remain the right token in %s"), RightOperand.Description, *CaseId)));
							ASSERT_THAT(AreEqual(RightOperand.ExpectedLength, RightObservation.Length,
								FString::Printf(TEXT("%s should retain its right length in %s"), RightOperand.Description, *CaseId)));
						}
						++ObservedCaseCount;
					}
				}
			}
		}

		ASSERT_THAT(AreEqual(2560, ObservedCaseCount,
			TEXT("Operator × operands × spacing product should execute every cell")));
		PrintGeneratedAsSource(
			*TestRunner,
			TEXT("FRONTEND-TOKEN-OPERATOR-OPERAND-SPACING"),
			TEXT("AS_SDK_FrontendTokenizerDeep_OperatorOperandSpacing"),
			GeneratedSource);
	}

	TEST_METHOD(OperatorMalformedRecoveryCartesianProduct)
	{
		using namespace AngelscriptNativeTestSupport;
		AS_NATIVE_PRODUCT("FRONTEND-TOKEN-OPERATOR-MALFORMED-RECOVERY",
			ENativeEvidence::Metadata
			| ENativeEvidence::Diagnostic);

		const FOperatorRecoveryCase PrefixCases[] =
		{
			{ "at", "@", TEXT("handle sigil rejected by the current fork") },
			{ "hash", "#", TEXT("preprocessor marker rejected by the raw tokenizer") },
			{ "dollar", "$", TEXT("dollar sigil rejected by the raw tokenizer") },
			{ "backtick", "`", TEXT("backtick rejected by the raw tokenizer") },
			{ "backslash", "\\", TEXT("backslash rejected by the raw tokenizer") },
		};

		const FRecoveryTailCase TailCases[] =
		{
			{ "identifier", "Value", ttIdentifier, 5, TEXT("identifier recovery tail") },
			{ "integer", "7", ttIntConstant, 1, TEXT("integer recovery tail") },
			{ "plus", "+", ttPlus, 1, TEXT("operator recovery tail") },
			{ "statement_end", ";", ttEndStatement, 1, TEXT("punctuation recovery tail") },
		};

		FString GeneratedSource;
		AppendGeneratedAsLine(GeneratedSource, TEXT("int GeneratedOperatorRecoveryCell()"));
		AppendGeneratedAsLine(GeneratedSource, TEXT("{"));
		AppendGeneratedAsLine(GeneratedSource, TEXT("\treturn 0;"));
		int32 ObservedCaseCount = 0;

		for (const FOperatorRecoveryCase& PrefixCase : PrefixCases)
		{
			for (const FRecoveryTailCase& TailCase : TailCases)
			{
				std::string Input(PrefixCase.Prefix);
				Input += TailCase.Input;
				const FString CaseId = MakeNativeCaseId(
					"FRONTEND-TOKEN-OPERATOR-MALFORMED-RECOVERY",
					{ ANSI_TO_TCHAR(PrefixCase.CatalogName), ANSI_TO_TCHAR(TailCase.CatalogName) });
				AppendCommentedCase(GeneratedSource, TEXT("malformed operator recovery input"), Input.c_str());
				AppendGeneratedAsLine(GeneratedSource,
					FString::Printf(TEXT("\t// CaseId: %s"), *CaseId));

				const FTokenObservation PrimaryObservation = ReadToken(Input.c_str());
				ASSERT_THAT(AreEqual(static_cast<int32>(ttUnrecognizedToken), static_cast<int32>(PrimaryObservation.Type),
					FString::Printf(TEXT("%s should retain an unrecognized primary token in %s"), PrefixCase.Description, *CaseId)));
				ASSERT_THAT(AreEqual(1, PrimaryObservation.Length,
					FString::Printf(TEXT("%s should consume one byte before recovery in %s"), PrefixCase.Description, *CaseId)));

				const FTokenObservation RecoveryObservation = ReadToken(Input.c_str() + 1);
				ASSERT_THAT(AreEqual(static_cast<int32>(TailCase.ExpectedType), static_cast<int32>(RecoveryObservation.Type),
					FString::Printf(TEXT("%s should remain observable after the malformed prefix in %s"), TailCase.Description, *CaseId)));
				ASSERT_THAT(AreEqual(TailCase.ExpectedLength, RecoveryObservation.Length,
					FString::Printf(TEXT("%s should retain its exact recovery span in %s"), TailCase.Description, *CaseId)));
				++ObservedCaseCount;
			}
		}

		ASSERT_THAT(AreEqual(20, ObservedCaseCount,
			TEXT("Malformed operator prefix × recovery tail should execute every cell")));
		AppendGeneratedAsLine(GeneratedSource, TEXT("}"));
		PrintGeneratedAsSource(
			*TestRunner,
			TEXT("FRONTEND-TOKEN-OPERATOR-MALFORMED-RECOVERY"),
			TEXT("AS_SDK_FrontendTokenizerDeep_OperatorMalformedRecovery"),
			GeneratedSource);
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
