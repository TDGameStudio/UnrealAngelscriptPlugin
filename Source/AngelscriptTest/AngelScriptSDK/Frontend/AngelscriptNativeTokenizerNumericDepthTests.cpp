#include "../Support/AngelscriptNativeTokenizerTestSupport.h"

#include "CQTest.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FTokenizerNumericDepthTests,
	"Angelscript.TestModule.AngelScriptSDK.Frontend.Tokenizer.DeepCoverage",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	struct FNumericLiteralCase
	{
		const char* CatalogName;
		const char* Input;
		eTokenType ExpectedType;
		int32 ExpectedLength;
		const TCHAR* Description;
	};

	struct FNumericSignCase
	{
		const char* CatalogName;
		const char* Prefix;
		eTokenType ExpectedType;
		const TCHAR* Description;
	};

	struct FNumericTerminationCase
	{
		const char* CatalogName;
		const char* Suffix;
		eTokenType ExpectedType;
		int32 ExpectedLength;
		const TCHAR* Description;
	};

	struct FMalformedNumericCase
	{
		const char* CatalogName;
		const char* Input;
		eTokenType ExpectedType;
		int32 ExpectedLength;
		bool bHasRecoveryToken;
		eTokenType RecoveryType;
		int32 RecoveryLength;
		const TCHAR* Description;
	};

	static FString BuildNumericSignTerminationSource(
		const FString& CaseId,
		const std::string& Input)
	{
		FString Source;
		AngelscriptNativeTestSupport::AppendGeneratedAsLine(Source, TEXT("int GeneratedNumericTokenCell()"));
		AngelscriptNativeTestSupport::AppendGeneratedAsLine(Source, TEXT("{"));
		AngelscriptNativeTestSupport::AppendGeneratedAsLine(Source,
			FString::Printf(TEXT("\t// CaseId: %s"), *CaseId));
		AngelscriptNativeTestSupport::AppendCommentedCase(Source, TEXT("signed numeric input"), Input.c_str());
		AngelscriptNativeTestSupport::AppendGeneratedAsLine(Source, TEXT("\treturn 0;"));
		AngelscriptNativeTestSupport::AppendGeneratedAsLine(Source, TEXT("}"));
		return Source;
	}

public:
	TEST_METHOD(NumericLiteralFormsCoverRadixExponentAndSuffixBoundaries)
	{
		using namespace AngelscriptNativeTestSupport;
		AS_NATIVE_PRODUCT("FRONTEND-TOKEN-NUMERIC-BOUNDARIES",
			ENativeEvidence::Metadata
			| ENativeEvidence::Diagnostic);

		const FTokenCase Cases[] =
		{
			{ "0", ttIntConstant, 1, TEXT("decimal zero") },
			{ "00042", ttIntConstant, 5, TEXT("decimal leading zero") },
			{ "12345678901234567890", ttIntConstant, 20, TEXT("large decimal") },
			{ "0b0", ttBitsConstant, 3, TEXT("binary zero") },
			{ "0b101010", ttBitsConstant, 8, TEXT("binary value") },
			{ "0o0", ttBitsConstant, 3, TEXT("octal zero") },
			{ "0o755", ttBitsConstant, 5, TEXT("octal value") },
			{ "0d42", ttBitsConstant, 4, TEXT("explicit decimal radix") },
			{ "0x0", ttBitsConstant, 3, TEXT("hex zero") },
			{ "0xDEADbeef", ttBitsConstant, 10, TEXT("mixed-case hex value") },
			{ "1.0", ttFloat64Constant, 3, TEXT("float64 decimal") },
			{ ".5", ttFloat64Constant, 2, TEXT("float64 leading dot") },
			{ "5.", ttFloat64Constant, 2, TEXT("float64 trailing dot") },
			{ "1.0f", ttFloat32Constant, 4, TEXT("float32 suffix") },
			{ "1.0F", ttFloat32Constant, 4, TEXT("uppercase float32 suffix") },
			{ "1e10", ttFloat64Constant, 4, TEXT("positive exponent") },
			{ "1e+10", ttFloat64Constant, 5, TEXT("explicit positive exponent") },
			{ "1e-10", ttFloat64Constant, 5, TEXT("negative exponent") },
			{ "1.5e+3f", ttFloat32Constant, 7, TEXT("float32 exponent") },
			{ "1.5e-3", ttFloat64Constant, 6, TEXT("float64 exponent") },
		};

		FString GeneratedSource;
		AngelscriptNativeTestSupport::AppendGeneratedAsLine(GeneratedSource, TEXT("float64 GeneratedNumericCorpus()"));
		AngelscriptNativeTestSupport::AppendGeneratedAsLine(GeneratedSource, TEXT("{"));
		AngelscriptNativeTestSupport::AppendGeneratedAsLine(GeneratedSource, TEXT("\tfloat64 Result = 0.0;"));

		for (const FTokenCase& Case : Cases)
		{
			AppendCommentedCase(GeneratedSource, TEXT("numeric literal"), Case.Input);
			const FTokenObservation Observation = ReadToken(Case.Input);
			ASSERT_THAT(AreEqual(static_cast<int32>(Case.ExpectedType), static_cast<int32>(Observation.Type),
				FString::Printf(TEXT("%s should use its radix or floating token kind"), Case.Description)));
			ASSERT_THAT(AreEqual(Case.ExpectedLength, Observation.Length,
				FString::Printf(TEXT("%s should consume exactly the numeric spelling"), Case.Description)));
		}

		const FTokenCase TailCases[] =
		{
			{ "0b102", ttBitsConstant, 4, TEXT("binary invalid digit tail") },
			{ "0o789", ttBitsConstant, 3, TEXT("octal invalid digit tail") },
			{ "0xG", ttBitsConstant, 2, TEXT("hex missing digit tail") },
			{ "1e", ttFloat64Constant, 2, TEXT("incomplete exponent") },
			{ "1e+", ttFloat64Constant, 3, TEXT("sign-only exponent") },
			{ "1.25fValue", ttFloat32Constant, 5, TEXT("float suffix before identifier") },
		};
		for (const FTokenCase& Case : TailCases)
		{
			AppendCommentedCase(GeneratedSource, TEXT("numeric boundary input"), Case.Input);
			const FTokenObservation Observation = ReadToken(Case.Input);
			ASSERT_THAT(AreEqual(static_cast<int32>(Case.ExpectedType), static_cast<int32>(Observation.Type),
				FString::Printf(TEXT("%s should retain the numeric prefix"), Case.Description)));
			ASSERT_THAT(AreEqual(Case.ExpectedLength, Observation.Length,
				FString::Printf(TEXT("%s should stop at the first nonnumeric boundary"), Case.Description)));
		}

		AngelscriptNativeTestSupport::AppendGeneratedAsLine(GeneratedSource, TEXT("\treturn Result;"));
		AngelscriptNativeTestSupport::AppendGeneratedAsLine(GeneratedSource, TEXT("}"));
		AngelscriptNativeTestSupport::PrintGeneratedAsSource(
			*TestRunner,
			TEXT("FRONTEND-TOKEN-NUMERIC-BOUNDARIES"),
			TEXT("AS_SDK_FrontendTokenizerDeep_Numbers"),
			GeneratedSource);
	}

	TEST_METHOD(NumericSignAndTerminationCartesianProduct)
	{
		using namespace AngelscriptNativeTestSupport;
		AS_NATIVE_PRODUCT("FRONTEND-TOKEN-NUMERIC-SIGN-TERMINATION",
			ENativeEvidence::Metadata
			| ENativeEvidence::Diagnostic);

		const FNumericLiteralCase NumericCases[] =
		{
			{ "decimal_zero", "0", ttIntConstant, 1, TEXT("decimal zero") },
			{ "decimal_value", "42", ttIntConstant, 2, TEXT("decimal value") },
			{ "signed_32_max", "2147483647", ttIntConstant, 10, TEXT("signed 32-bit maximum") },
			{ "signed_32_rollover", "2147483648", ttIntConstant, 10, TEXT("signed 32-bit rollover spelling") },
			{ "signed_64_max", "9223372036854775807", ttIntConstant, 19, TEXT("signed 64-bit maximum") },
			{ "signed_64_rollover", "9223372036854775808", ttIntConstant, 19, TEXT("signed 64-bit rollover spelling") },
			{ "binary_zero", "0b0", ttBitsConstant, 3, TEXT("binary zero") },
			{ "binary_value", "0b101010", ttBitsConstant, 8, TEXT("binary value") },
			{ "octal_zero", "0o0", ttBitsConstant, 3, TEXT("octal zero") },
			{ "octal_value", "0o755", ttBitsConstant, 5, TEXT("octal value") },
			{ "explicit_decimal", "0d42", ttBitsConstant, 4, TEXT("explicit decimal radix") },
			{ "hex_zero", "0x0", ttBitsConstant, 3, TEXT("hex zero") },
			{ "hex_value", "0xDEADbeef", ttBitsConstant, 10, TEXT("mixed-case hexadecimal") },
			{ "float64_decimal", "1.0", ttFloat64Constant, 3, TEXT("float64 decimal") },
			{ "float64_leading_dot", ".5", ttFloat64Constant, 2, TEXT("leading-dot float64") },
			{ "float64_trailing_dot", "5.", ttFloat64Constant, 2, TEXT("trailing-dot float64") },
			{ "exponent_positive", "1e10", ttFloat64Constant, 4, TEXT("positive exponent") },
			{ "exponent_negative", "1e-10", ttFloat64Constant, 5, TEXT("negative exponent") },
			{ "float32_exponent", "1.5e+3f", ttFloat32Constant, 7, TEXT("float32 exponent") },
			{ "float32_upper_exponent", "1.5e-3F", ttFloat32Constant, 7, TEXT("uppercase float32 exponent") },
		};

		const FNumericSignCase SignCases[] =
		{
			{ "positive", "+", ttPlus, TEXT("positive sign") },
			{ "negative", "-", ttMinus, TEXT("negative sign") },
		};

		const FNumericTerminationCase TerminationCases[] =
		{
			{ "statement_terminator", ";", ttEndStatement, 1, TEXT("statement terminator") },
			{ "identifier_after_whitespace", " Value", ttWhiteSpace, 1, TEXT("identifier after whitespace") },
			{ "newline", "\x0A", ttWhiteSpace, 1, TEXT("newline terminator") },
			{ "closing_parenthesis", ")", ttCloseParanthesis, 1, TEXT("closing parenthesis") },
		};

		int32 ObservedCaseCount = 0;
		for (const FNumericLiteralCase& NumericCase : NumericCases)
		{
			for (const FNumericSignCase& SignCase : SignCases)
			{
				for (const FNumericTerminationCase& TerminationCase : TerminationCases)
				{
					std::string Input(SignCase.Prefix);
					Input += NumericCase.Input;
					Input += TerminationCase.Suffix;

					const FString CaseId = MakeNativeCaseId(
						"FRONTEND-TOKEN-NUMERIC-SIGN-TERMINATION",
						{ ANSI_TO_TCHAR(SignCase.CatalogName), ANSI_TO_TCHAR(NumericCase.CatalogName), ANSI_TO_TCHAR(TerminationCase.CatalogName) });
					const FTokenObservation SignObservation = ReadToken(Input.c_str());
					ASSERT_THAT(AreEqual(static_cast<int32>(SignCase.ExpectedType), static_cast<int32>(SignObservation.Type),
						*FString::Printf(TEXT("%s should tokenize before %s"), SignCase.Description, *CaseId)));
					ASSERT_THAT(AreEqual(1, SignObservation.Length,
						*FString::Printf(TEXT("%s should consume one byte before %s"), SignCase.Description, *CaseId)));

					const FTokenObservation NumericObservation = ReadToken(Input.c_str() + SignObservation.Length);
					ASSERT_THAT(AreEqual(static_cast<int32>(NumericCase.ExpectedType), static_cast<int32>(NumericObservation.Type),
						*FString::Printf(TEXT("%s should preserve its numeric token kind in %s"), NumericCase.Description, *CaseId)));
					ASSERT_THAT(AreEqual(NumericCase.ExpectedLength, NumericObservation.Length,
						*FString::Printf(TEXT("%s should consume its exact spelling in %s"), NumericCase.Description, *CaseId)));

					const FTokenObservation TerminationObservation = ReadToken(
						Input.c_str() + SignObservation.Length + NumericObservation.Length);
					ASSERT_THAT(AreEqual(static_cast<int32>(TerminationCase.ExpectedType), static_cast<int32>(TerminationObservation.Type),
						*FString::Printf(TEXT("%s should remain after %s"), TerminationCase.Description, *CaseId)));
					ASSERT_THAT(AreEqual(TerminationCase.ExpectedLength, TerminationObservation.Length,
						*FString::Printf(TEXT("%s should retain its exact boundary in %s"), TerminationCase.Description, *CaseId)));

					const FString Source = BuildNumericSignTerminationSource(CaseId, Input);
					PrintGeneratedAsSource(
						*TestRunner,
						CaseId,
						TEXT("AS_SDK_FrontendTokenizerDeep_NumericSignTermination"),
						Source);
					++ObservedCaseCount;
				}
			}
		}

		ASSERT_THAT(AreEqual(
			UE_ARRAY_COUNT(NumericCases) * UE_ARRAY_COUNT(SignCases) * UE_ARRAY_COUNT(TerminationCases),
			ObservedCaseCount,
			TEXT("numeric sign and termination product should execute every source cell")));
	}

	TEST_METHOD(NumericMalformedRecoveryCartesianProduct)
	{
		using namespace AngelscriptNativeTestSupport;
		AS_NATIVE_PRODUCT("FRONTEND-TOKEN-NUMERIC-MALFORMED-RECOVERY",
			ENativeEvidence::Metadata
			| ENativeEvidence::Diagnostic);

		const FMalformedNumericCase MalformedCases[] =
		{
			{ "binary_invalid_tail", "0b102", ttBitsConstant, 4, true, ttIntConstant, 1, TEXT("binary invalid digit recovery") },
			{ "octal_invalid_tail", "0o789", ttBitsConstant, 3, true, ttIntConstant, 2, TEXT("octal invalid digit recovery") },
			{ "hex_missing_digit", "0xG", ttBitsConstant, 2, true, ttIdentifier, 1, TEXT("hex missing digit recovery") },
			{ "incomplete_exponent", "1e", ttFloat64Constant, 2, false, ttUnrecognizedToken, 0, TEXT("incomplete exponent at end of input") },
			{ "sign_only_exponent", "1e+", ttFloat64Constant, 3, false, ttUnrecognizedToken, 0, TEXT("sign-only exponent at end of input") },
			{ "float_suffix_identifier_tail", "1.25fValue", ttFloat32Constant, 5, true, ttIdentifier, 5, TEXT("float suffix identifier recovery") },
		};

		const FNumericSignCase SignCases[] =
		{
			{ "positive", "+", ttPlus, TEXT("positive sign") },
			{ "negative", "-", ttMinus, TEXT("negative sign") },
		};

		int32 ObservedCaseCount = 0;
		for (const FMalformedNumericCase& MalformedCase : MalformedCases)
		{
			for (const FNumericSignCase& SignCase : SignCases)
			{
				std::string Input(SignCase.Prefix);
				Input += MalformedCase.Input;

				const FString CaseId = MakeNativeCaseId(
					"FRONTEND-TOKEN-NUMERIC-MALFORMED-RECOVERY",
					{ ANSI_TO_TCHAR(SignCase.CatalogName), ANSI_TO_TCHAR(MalformedCase.CatalogName) });
				const FTokenObservation SignObservation = ReadToken(Input.c_str());
				ASSERT_THAT(AreEqual(static_cast<int32>(SignCase.ExpectedType), static_cast<int32>(SignObservation.Type),
					*FString::Printf(TEXT("%s should tokenize before malformed input %s"), SignCase.Description, *CaseId)));
				ASSERT_THAT(AreEqual(1, SignObservation.Length,
					*FString::Printf(TEXT("%s should consume one byte before malformed input %s"), SignCase.Description, *CaseId)));

				const FTokenObservation NumericObservation = ReadToken(Input.c_str() + SignObservation.Length);
				ASSERT_THAT(AreEqual(static_cast<int32>(MalformedCase.ExpectedType), static_cast<int32>(NumericObservation.Type),
					*FString::Printf(TEXT("%s should retain its malformed prefix token kind in %s"), MalformedCase.Description, *CaseId)));
				ASSERT_THAT(AreEqual(MalformedCase.ExpectedLength, NumericObservation.Length,
					*FString::Printf(TEXT("%s should consume only its valid prefix in %s"), MalformedCase.Description, *CaseId)));

				if (MalformedCase.bHasRecoveryToken)
				{
					const FTokenObservation RecoveryObservation = ReadToken(
						Input.c_str() + SignObservation.Length + NumericObservation.Length);
					ASSERT_THAT(AreEqual(static_cast<int32>(MalformedCase.RecoveryType), static_cast<int32>(RecoveryObservation.Type),
						*FString::Printf(TEXT("%s should expose the recovery token in %s"), MalformedCase.Description, *CaseId)));
					ASSERT_THAT(AreEqual(MalformedCase.RecoveryLength, RecoveryObservation.Length,
						*FString::Printf(TEXT("%s should preserve the recovery token length in %s"), MalformedCase.Description, *CaseId)));
				}

				const FString Source = BuildNumericSignTerminationSource(CaseId, Input);
				PrintGeneratedAsSource(
					*TestRunner,
					CaseId,
					TEXT("AS_SDK_FrontendTokenizerDeep_NumericMalformedRecovery"),
					Source);
				++ObservedCaseCount;
			}
		}

		ASSERT_THAT(AreEqual(
			UE_ARRAY_COUNT(MalformedCases) * UE_ARRAY_COUNT(SignCases),
			ObservedCaseCount,
			TEXT("numeric malformed recovery product should execute every source cell")));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
