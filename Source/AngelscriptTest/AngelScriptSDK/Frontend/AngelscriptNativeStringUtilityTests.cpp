#include "CQTest.h"

// Raw SDK string utility coverage.

#include "StartAngelscriptHeaders.h"
#include "source/as_string_util.h"
#include "EndAngelscriptHeaders.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FStringUtilityTests,
	"Angelscript.TestModule.AngelScriptSDK.Frontend.StringUtilities",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(CompareStringsOrdersEqualAndDistinctValues)
	{
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
		ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(asStringScanDouble("-42.125"), -42.125),
			TEXT("Double scanner should preserve a signed fractional literal")));
	}

	TEST_METHOD(ScanFloatConsumesFractionalLiteral)
	{
		ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(asStringScanFloat("3.5"), 3.5f),
			TEXT("Float scanner should parse a fractional literal through the fork runtime entry point")));
	}

	TEST_METHOD(Utf8AndUtf16EncodingRoundTripsBoundaryValues)
	{
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
