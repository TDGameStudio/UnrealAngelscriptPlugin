#include "CQTest.h"
#include "../Support/AngelscriptNativeCaseTestSupport.h"
#include "../Support/AngelscriptNativeLanguageCaseTestSupport.h"

// Raw SDK string utility coverage.

#include "StartAngelscriptHeaders.h"
#include "source/as_string_util.h"
#include "EndAngelscriptHeaders.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FStringUtilityTests,
	"Angelscript.TestModule.AngelScriptSDK.Frontend.StringUtilities",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(ComparisonsByLeftAndRightRange)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT("FRONTEND-STRING-UTILITY-COMPARISON-RANGES",
			ENativeEvidence::Metadata
			| ENativeEvidence::Isolation);

		struct FStringRange
		{
			const TCHAR* Id;
			const char* Value;
			size_t Length;
		};
		const char EmbeddedNullValue[] =
		{
			'A',
			'l',
			'\0',
			'p',
			'h',
			'a',
		};
		const FStringRange Ranges[] =
		{
			{ TEXT("empty"), "", 0 },
			{ TEXT("alpha"), "Alpha", 5 },
			{ TEXT("alphabet"), "Alphabet", 8 },
			{ TEXT("beta"), "Beta", 4 },
			{ TEXT("embedded_null"), EmbeddedNullValue, UE_ARRAY_COUNT(EmbeddedNullValue) },
		};

		int32 ObservedCells = 0;
		for (const FStringRange& Left : Ranges)
		{
			for (const FStringRange& Right : Ranges)
			{
				const FString SourceId = FString::Printf(
					TEXT("FRONTEND-STRING-UTILITY-COMPARISON-RANGES-%s-%s"),
					Left.Id,
					Right.Id);
				FString ReviewSource;
				AppendGeneratedAsLine(ReviewSource, FString::Printf(
					TEXT("// left range: %s (%d bytes)"),
					Left.Id,
					static_cast<int32>(Left.Length)));
				AppendGeneratedAsLine(ReviewSource, FString::Printf(
					TEXT("// right range: %s (%d bytes)"),
					Right.Id,
					static_cast<int32>(Right.Length)));
				PrintGeneratedAsSource(*TestRunner, SourceId, TEXT("StringUtilityComparison"), ReviewSource);

				int32 ExpectedSign = 0;
				if (Left.Length == 0)
				{
					ExpectedSign = Right.Length == 0 ? 0 : 1;
				}
				else if (Right.Length == 0)
				{
					ExpectedSign = -1;
				}
				else
				{
					const size_t CommonLength = FMath::Min(Left.Length, Right.Length);
					const int ByteComparison = std::memcmp(Left.Value, Right.Value, CommonLength);
					if (ByteComparison != 0)
					{
						ExpectedSign = ByteComparison < 0 ? -1 : 1;
					}
					else if (Left.Length != Right.Length)
					{
						ExpectedSign = Left.Length < Right.Length ? 1 : -1;
					}
				}

				const int Comparison = asCompareStrings(
					Left.Value,
					Left.Length,
					Right.Value,
					Right.Length);
				const int32 ActualSign = Comparison < 0 ? -1 : (Comparison > 0 ? 1 : 0);
				ASSERT_THAT(AreEqual(ExpectedSign, ActualSign,
					FString::Printf(TEXT("%s should preserve the SDK bounded-range comparison convention"), *SourceId)));
				++ObservedCells;
			}
		}

		ASSERT_THAT(AreEqual(25, ObservedCells,
			TEXT("String comparison product should execute every left-range and right-range cell")));
	}

	TEST_METHOD(UnsignedScanningByRadixAndMagnitude)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT("FRONTEND-STRING-UTILITY-UNSIGNED-SCAN",
			ENativeEvidence::Metadata
			| ENativeEvidence::Diagnostic
			| ENativeEvidence::Isolation);

		struct FScanCase
		{
			const TCHAR* RadixId;
			const TCHAR* MagnitudeId;
			const char* Input;
			int32 Base;
			asQWORD ExpectedValue;
			size_t ExpectedScanned;
			bool bExpectedOverflow;
		};
		const asQWORD MaximumValue = ~asQWORD(0);
		const FScanCase Cases[] =
		{
			{ TEXT("binary"), TEXT("zero"), "0b0", 0, 0, 3, false },
			{ TEXT("binary"), TEXT("value"), "0b101011", 0, 43, 8, false },
			{ TEXT("binary"), TEXT("maximum"), "0b1111111111111111111111111111111111111111111111111111111111111111", 0, MaximumValue, 66, false },
			{ TEXT("binary"), TEXT("overflow"), "0b11111111111111111111111111111111111111111111111111111111111111111", 0, MaximumValue, 67, true },
			{ TEXT("binary"), TEXT("invalid_tail"), "0b102", 0, 2, 4, false },
			{ TEXT("octal"), TEXT("zero"), "0o0", 0, 0, 3, false },
			{ TEXT("octal"), TEXT("value"), "0o52", 0, 42, 4, false },
			{ TEXT("octal"), TEXT("maximum"), "0o1777777777777777777777", 0, MaximumValue, 24, false },
			{ TEXT("octal"), TEXT("overflow"), "0o2000000000000000000000", 0, 0, 24, true },
			{ TEXT("octal"), TEXT("invalid_tail"), "0o78", 0, 7, 3, false },
			{ TEXT("decimal"), TEXT("zero"), "0", 10, 0, 1, false },
			{ TEXT("decimal"), TEXT("value"), "42", 10, 42, 2, false },
			{ TEXT("decimal"), TEXT("maximum"), "18446744073709551615", 10, MaximumValue, 20, false },
			{ TEXT("decimal"), TEXT("overflow"), "18446744073709551616", 10, 0, 20, true },
			{ TEXT("decimal"), TEXT("invalid_tail"), "42z", 10, 42, 2, false },
			{ TEXT("hexadecimal"), TEXT("zero"), "0x0", 0, 0, 3, false },
			{ TEXT("hexadecimal"), TEXT("value"), "0x2A", 0, 42, 4, false },
			{ TEXT("hexadecimal"), TEXT("maximum"), "0xFFFFFFFFFFFFFFFF", 0, MaximumValue, 18, false },
			{ TEXT("hexadecimal"), TEXT("overflow"), "0x10000000000000000", 0, 0, 19, true },
			{ TEXT("hexadecimal"), TEXT("invalid_tail"), "0x2G", 0, 2, 3, false },
		};

		int32 ObservedCells = 0;
		for (const FScanCase& Case : Cases)
		{
			const FString SourceId = FString::Printf(
				TEXT("FRONTEND-STRING-UTILITY-UNSIGNED-SCAN-%s-%s"),
				Case.RadixId,
				Case.MagnitudeId);
			FString ReviewSource;
			AppendGeneratedAsLine(ReviewSource, FString::Printf(
				TEXT("// unsigned scan input: %s"),
				UTF8_TO_TCHAR(Case.Input)));
			PrintGeneratedAsSource(*TestRunner, SourceId, TEXT("StringUtilityUnsignedScan"), ReviewSource);

			size_t Scanned = 0;
			bool bOverflow = false;
			const asQWORD Value = asStringScanUInt64(
				Case.Input,
				Case.Base,
				&Scanned,
				&bOverflow);
			ASSERT_THAT(AreEqual(static_cast<uint64>(Case.ExpectedValue), static_cast<uint64>(Value),
				FString::Printf(TEXT("%s should preserve the parsed value"), *SourceId)));
			ASSERT_THAT(AreEqual(static_cast<int32>(Case.ExpectedScanned), static_cast<int32>(Scanned),
				FString::Printf(TEXT("%s should preserve the consumed byte count"), *SourceId)));
			ASSERT_THAT(AreEqual(Case.bExpectedOverflow, bOverflow,
				FString::Printf(TEXT("%s should preserve overflow classification"), *SourceId)));
			++ObservedCells;
		}

		ASSERT_THAT(AreEqual(20, ObservedCells,
			TEXT("Unsigned scanner product should execute every radix and magnitude cell")));
	}

	TEST_METHOD(FloatingScanningByPrecisionAndSpelling)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT("FRONTEND-STRING-UTILITY-FLOAT-SCAN",
			ENativeEvidence::Metadata
			| ENativeEvidence::Isolation);

		struct FFloatingCase
		{
			const TCHAR* Id;
			const char* Input;
			double ExpectedValue;
		};
		const FFloatingCase Cases[] =
		{
			{ TEXT("signed_fraction"), "-42.125", -42.125 },
			{ TEXT("positive_exponent"), "1.25e3", 1250.0 },
			{ TEXT("negative_exponent"), "8.0e-2", 0.08 },
			{ TEXT("leading_dot"), ".5", 0.5 },
		};
		const TCHAR* PrecisionIds[] =
		{
			TEXT("float32"),
			TEXT("float64"),
		};

		int32 ObservedCells = 0;
		for (int32 PrecisionIndex = 0; PrecisionIndex < UE_ARRAY_COUNT(PrecisionIds); ++PrecisionIndex)
		{
			for (const FFloatingCase& Case : Cases)
			{
				const FString SourceId = FString::Printf(
					TEXT("FRONTEND-STRING-UTILITY-FLOAT-SCAN-%s-%s"),
					PrecisionIds[PrecisionIndex],
					Case.Id);
				FString ReviewSource;
				AppendGeneratedAsLine(ReviewSource, FString::Printf(
					TEXT("// floating scan input: %s"),
					UTF8_TO_TCHAR(Case.Input)));
				PrintGeneratedAsSource(*TestRunner, SourceId, TEXT("StringUtilityFloatingScan"), ReviewSource);

				if (PrecisionIndex == 0)
				{
					const float Value = asStringScanFloat(Case.Input);
					ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Value, static_cast<float>(Case.ExpectedValue)),
						FString::Printf(TEXT("%s should preserve the float32 result"), *SourceId)));
				}
				else
				{
					const double Value = asStringScanDouble(Case.Input);
					ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Value, Case.ExpectedValue),
						FString::Printf(TEXT("%s should preserve the float64 result"), *SourceId)));
				}
				++ObservedCells;
			}
		}

		ASSERT_THAT(AreEqual(8, ObservedCells,
			TEXT("Floating scanner product should execute every precision and spelling cell")));
	}

	TEST_METHOD(UnicodeEncodingByCodePointAndEncoding)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT("FRONTEND-STRING-UTILITY-UNICODE-ENCODING",
			ENativeEvidence::Metadata
			| ENativeEvidence::Diagnostic
			| ENativeEvidence::Isolation);

		struct FCodePointCase
		{
			const TCHAR* Id;
			uint32 CodePoint;
			int32 ExpectedUtf8Length;
			int32 ExpectedUtf16Length;
		};
		const FCodePointCase Cases[] =
		{
			{ TEXT("ascii_boundary"), 0x7F, 1, 2 },
			{ TEXT("two_byte_boundary"), 0x80, 2, 2 },
			{ TEXT("three_byte_value"), 0x20AC, 3, 2 },
			{ TEXT("supplementary_value"), 0x1F642, 4, 4 },
			{ TEXT("surrogate_current_fork"), 0xD800, -1, 2 },
			{ TEXT("out_of_range"), 0x110000, -1, 4 },
		};
		const TCHAR* EncodingIds[] =
		{
			TEXT("utf8"),
			TEXT("utf16"),
		};

		int32 ObservedCells = 0;
		for (const FCodePointCase& Case : Cases)
		{
			for (int32 EncodingIndex = 0; EncodingIndex < UE_ARRAY_COUNT(EncodingIds); ++EncodingIndex)
			{
				const FString SourceId = FString::Printf(
					TEXT("FRONTEND-STRING-UTILITY-UNICODE-ENCODING-%s-%s"),
					Case.Id,
					EncodingIds[EncodingIndex]);
				FString ReviewSource;
				AppendGeneratedAsLine(ReviewSource, FString::Printf(
					TEXT("// Unicode code point: U+%06X"),
					Case.CodePoint));
				PrintGeneratedAsSource(*TestRunner, SourceId, TEXT("StringUtilityUnicodeEncoding"), ReviewSource);

				char Encoded[8] = {};
				if (EncodingIndex == 0)
				{
					const int32 EncodedLength = asStringEncodeUTF8(Case.CodePoint, Encoded);
					ASSERT_THAT(AreEqual(Case.ExpectedUtf8Length, EncodedLength,
						FString::Printf(TEXT("%s should preserve UTF-8 validity and length"), *SourceId)));
					if (EncodedLength > 0)
					{
						unsigned int DecodedLength = 0;
						ASSERT_THAT(AreEqual(static_cast<int32>(Case.CodePoint), asStringDecodeUTF8(Encoded, &DecodedLength),
							FString::Printf(TEXT("%s should round-trip its UTF-8 code point"), *SourceId)));
						ASSERT_THAT(AreEqual(EncodedLength, static_cast<int32>(DecodedLength),
							FString::Printf(TEXT("%s should report its UTF-8 decoded length"), *SourceId)));
					}
				}
				else
				{
					const int32 EncodedLength = asStringEncodeUTF16(Case.CodePoint, Encoded);
					ASSERT_THAT(AreEqual(Case.ExpectedUtf16Length, EncodedLength,
						FString::Printf(TEXT("%s should preserve current-fork UTF-16 validity and length"), *SourceId)));
				}
				++ObservedCells;
			}
		}

		ASSERT_THAT(AreEqual(12, ObservedCells,
			TEXT("Unicode encoding product should execute every code-point and encoding cell")));
	}

	TEST_METHOD(CompareStringsOrdersEqualAndDistinctValues)
	{
		AS_NATIVE_NON_PRODUCT("LegacyCompatibility",
			"Retained representative bounded comparison smoke; FRONTEND-STRING-UTILITY-COMPARISON-RANGES owns every left-range and right-range combination.");

		ASSERT_THAT(AreEqual(0, asCompareStrings("equal", 5, "equal", 5),
			TEXT("Equal byte ranges should compare as equal")));
		ASSERT_THAT(IsTrue(asCompareStrings("alpha", 5, "alphabet", 8) > 0,
			TEXT("A proper prefix should sort before the longer range using SDK comparison convention")));
		ASSERT_THAT(IsTrue(asCompareStrings("alphabet", 8, "alpha", 5) < 0,
			TEXT("A longer range should sort after its proper prefix using SDK comparison convention")));
		ASSERT_THAT(IsTrue(asCompareStrings("beta", 4, "alpha", 5) > 0,
			TEXT("Different first bytes should preserve lexical ordering")));
	}

	TEST_METHOD(ScanUnsignedIntegerConsumesExpectedDigits)
	{
		AS_NATIVE_NON_PRODUCT("LegacyCompatibility",
			"Retained hexadecimal and decimal scanner smoke; FRONTEND-STRING-UTILITY-UNSIGNED-SCAN owns radix and magnitude combinations including overflow and invalid tails.");

		size_t Scanned = 0;
		bool bOverflow = false;
		const asQWORD HexValue = asStringScanUInt64("0x1Fz", 0, &Scanned, &bOverflow);
		ASSERT_THAT(AreEqual(static_cast<uint64>(31), static_cast<uint64>(HexValue),
			TEXT("Auto-base scanner should parse a hexadecimal prefix")));
		ASSERT_THAT(AreEqual(4, static_cast<int32>(Scanned),
			TEXT("Scanner should report exactly the consumed prefix and digits")));
		ASSERT_THAT(IsFalse(bOverflow, TEXT("Small hexadecimal value should not overflow")));

		const asQWORD DecimalValue = asStringScanUInt64("18446744073709551616", 10, &Scanned, &bOverflow);
		ASSERT_THAT(AreEqual(20, static_cast<int32>(Scanned),
			TEXT("Decimal scanner should consume every decimal digit")));
		ASSERT_THAT(IsTrue(bOverflow, TEXT("Value above uint64 maximum should report overflow")));
		ASSERT_THAT(AreEqual(static_cast<uint64>(0), static_cast<uint64>(DecimalValue),
			TEXT("Overflow result should retain the SDK's unsigned wraparound arithmetic")));
	}

	TEST_METHOD(ScanSignedDoubleConsumesLeadingSign)
	{
		AS_NATIVE_NON_PRODUCT("LegacyCompatibility",
			"Retained signed-double scanner smoke; FRONTEND-STRING-UTILITY-FLOAT-SCAN owns precision and spelling combinations.");

		ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(asStringScanDouble("-42.125"), -42.125),
			TEXT("Double scanner should preserve a signed fractional literal")));
	}

	TEST_METHOD(ScanFloatConsumesFractionalLiteral)
	{
		AS_NATIVE_NON_PRODUCT("LegacyCompatibility",
			"Retained float scanner smoke; FRONTEND-STRING-UTILITY-FLOAT-SCAN owns precision and spelling combinations.");

		ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(asStringScanFloat("3.5"), 3.5f),
			TEXT("Float scanner should parse a fractional literal through the fork runtime entry point")));
	}

	TEST_METHOD(Utf8AndUtf16EncodingRoundTripsBoundaryValues)
	{
		AS_NATIVE_NON_PRODUCT("LegacyCompatibility",
			"Retained representative UTF-8/UTF-16 smoke; FRONTEND-STRING-UTILITY-UNICODE-ENCODING owns code-point class and encoding combinations.");

		char Encoded[4] = {};
		const int32 EncodedLength = asStringEncodeUTF8(0x20AC, Encoded);
		ASSERT_THAT(AreEqual(3, EncodedLength, TEXT("Euro sign should encode to three UTF-8 bytes")));
		ASSERT_THAT(AreEqual(0xE2, static_cast<int32>(static_cast<uint8>(Encoded[0])),
			TEXT("UTF-8 first byte should carry the expected leading bits")));

		unsigned int DecodedLength = 0;
		ASSERT_THAT(AreEqual(0x20AC, asStringDecodeUTF8(Encoded, &DecodedLength),
			TEXT("UTF-8 decoder should recover the original code point")));
		ASSERT_THAT(AreEqual(3, static_cast<int32>(DecodedLength),
			TEXT("UTF-8 decoder should report consumed bytes")));
		ASSERT_THAT(AreEqual(-1, asStringEncodeUTF8(0xD800, Encoded),
			TEXT("UTF-8 encoder should reject surrogate code points")));
		ASSERT_THAT(AreEqual(2, asStringEncodeUTF16(0x0041, Encoded),
			TEXT("Basic multilingual-plane code point should use one UTF-16 code unit")));
		ASSERT_THAT(AreEqual(4, asStringEncodeUTF16(0x1F642, Encoded),
			TEXT("Supplementary code point should use a UTF-16 surrogate pair")));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
