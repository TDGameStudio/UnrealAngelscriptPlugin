// ============================================================================
// AngelscriptDateTimeBindingsTests.cpp
//
// FDateTime binding coverage — CQTest refactor. Automation IDs:
//   Angelscript.TestModule.Bindings.DateTime.FAngelscriptDateTimeBindingsTest.*
//
// Sections:
//   ParseIso8601       — ISO-8601 parsing with component verification
//   ParseHttpDate      — HTTP date parsing with component verification
//   ParseGeneric       — Generic Parse with component verification
//   ParseInvalid       — Parse/ParseHttpDate/ParseIso8601 with invalid input
//   Construction       — FDateTime construction, formatting, component access
//   RoundTrip          — Parse→Format→Parse round-trip fidelity
//
// CQTest adaptation notes:
//   All expected values are computed from native FDateTime baselines and
//   substituted via ReplaceInline into the script source at test time.
// ============================================================================

#include "CQTest.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestModuleScope.h"
#include "AngelscriptTestExecute.h"

#include "Misc/DateTime.h"

#if WITH_ANGELSCRIPT_UNITTESTS


// ----------------------------------------------------------------------------
// Profile
// ----------------------------------------------------------------------------


// ----------------------------------------------------------------------------
// Helpers
// ----------------------------------------------------------------------------

namespace AngelscriptDateTimeTestHelpers
{
	struct FDateTimeBaseline
	{
		bool bSuccess = false;
		int64 Ticks = 0;
		int32 Year = 0;
		int32 Month = 0;
		int32 Day = 0;
		int32 Hour12 = 0;
		int32 Millisecond = 0;
	};

	static FDateTimeBaseline Capture(const bool bSuccess, const FDateTime& Value)
	{
		FDateTimeBaseline B;
		B.bSuccess = bSuccess;
		B.Ticks = Value.GetTicks();
		int32 Y, M, D;
		Value.GetDate(Y, M, D);
		B.Year = Y;
		B.Month = M;
		B.Day = D;
		B.Hour12 = Value.GetHour12();
		B.Millisecond = Value.GetMillisecond();
		return B;
	}

	static void ReplaceValueTokens(FString& Source, const TCHAR* Prefix, const FDateTimeBaseline& B)
	{
		Source.ReplaceInline(*FString::Printf(TEXT("__%s_SUCCESS__"), Prefix), B.bSuccess ? TEXT("true") : TEXT("false"), ESearchCase::CaseSensitive);
		Source.ReplaceInline(*FString::Printf(TEXT("__%s_TICKS__"), Prefix), *FString::Printf(TEXT("%lld"), static_cast<long long>(B.Ticks)), ESearchCase::CaseSensitive);
		Source.ReplaceInline(*FString::Printf(TEXT("__%s_YEAR__"), Prefix), *FString::FromInt(B.Year), ESearchCase::CaseSensitive);
		Source.ReplaceInline(*FString::Printf(TEXT("__%s_MONTH__"), Prefix), *FString::FromInt(B.Month), ESearchCase::CaseSensitive);
		Source.ReplaceInline(*FString::Printf(TEXT("__%s_DAY__"), Prefix), *FString::FromInt(B.Day), ESearchCase::CaseSensitive);
		Source.ReplaceInline(*FString::Printf(TEXT("__%s_HOUR12__"), Prefix), *FString::FromInt(B.Hour12), ESearchCase::CaseSensitive);
		Source.ReplaceInline(*FString::Printf(TEXT("__%s_MILLISECOND__"), Prefix), *FString::FromInt(B.Millisecond), ESearchCase::CaseSensitive);
	}

	// Shared script helper function for verifying datetime component values
	static FString VerifyDateValueFunc()
	{
		return ASTEST_AS(R"AS(
			int VerifyDateValue(bool ObservedSuccess, bool ExpectedSuccess, FDateTime Value, int64 ExpectedTicks, int ExpectedYear, int ExpectedMonth, int ExpectedDay, int ExpectedHour12, int ExpectedMillisecond, int FailureBase)
			{
				if (ObservedSuccess != ExpectedSuccess)
				{
					return FailureBase + 0;
				}
				if (Value.GetTicks() != ExpectedTicks)
				{
					return FailureBase + 1;
				}
				int Year = -1;
				int Month = -1;
				int Day = -1;
				Value.GetDate(Year, Month, Day);
				if (Year != ExpectedYear || Month != ExpectedMonth || Day != ExpectedDay)
				{
					return FailureBase + 2;
				}
				if (Value.GetHour12() != ExpectedHour12)
				{
					return FailureBase + 3;
				}
				if (Value.GetMillisecond() != ExpectedMillisecond)
				{
					return FailureBase + 4;
				}
				return 0;
			}
			)AS");
	}
}

// ----------------------------------------------------------------------------
// Test class
// ----------------------------------------------------------------------------

TEST_CLASS_WITH_FLAGS(FAngelscriptDateTimeBindingsTest,
	"Angelscript.TestModule.Bindings.DateTime",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	BEFORE_ALL()
	{
		ASTEST_CREATE_ENGINE();
	}

	AFTER_ALL()
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		ASTEST_RESET_ENGINE(Engine);
	}

	// ====================================================================
	// Section: ParseIso8601
	// ====================================================================

	TEST_METHOD(ParseIso8601)
	{
		using namespace AngelscriptDateTimeTestHelpers;

		const FString IsoInput = TEXT("2024-12-25T14:30:15Z");
		FDateTime IsoValue = FDateTime::MinValue();
		const FDateTimeBaseline IsoBaseline = Capture(FDateTime::ParseIso8601(*IsoInput, IsoValue), IsoValue);

		FString ScriptSource = VerifyDateValueFunc() + ASTEST_AS(R"AS(
			int DateTime_ParseIso8601()
			{
				FDateTime Parsed = FDateTime::MinValue();
				const bool bOk = FDateTime::ParseIso8601("__ISO_INPUT__", Parsed);
				return (VerifyDateValue(bOk, __ISO_SUCCESS__, Parsed, int64(__ISO_TICKS__), __ISO_YEAR__, __ISO_MONTH__, __ISO_DAY__, __ISO_HOUR12__, __ISO_MILLISECOND__, 10) == 0) ? 1 : 0;
			}
			)AS");
		ScriptSource.ReplaceInline(TEXT("__ISO_INPUT__"), *IsoInput, ESearchCase::CaseSensitive);
		ReplaceValueTokens(ScriptSource, TEXT("ISO"), IsoBaseline);

		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASDateTime_Iso"), ScriptSource);
		if (!Mod.IsValid()) return;
		auto& M = Mod.GetModule();

		ASSERT_THAT(IsTrue(
			ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int DateTime_ParseIso8601()"), TEXT("FDateTime::ParseIso8601 should parse and match native components"), 1),
			TEXT("ExpectGlobalInt should pass")));
	}

	// ====================================================================
	// Section: ParseHttpDate
	// ====================================================================

	TEST_METHOD(ParseHttpDate)
	{
		using namespace AngelscriptDateTimeTestHelpers;

		const FString HttpInput = TEXT("Wed, 25 Dec 2024 14:30:15 GMT");
		FDateTime HttpValue = FDateTime::MinValue();
		const FDateTimeBaseline HttpBaseline = Capture(FDateTime::ParseHttpDate(HttpInput, HttpValue), HttpValue);

		FString ScriptSource = VerifyDateValueFunc() + ASTEST_AS(R"AS(
			int DateTime_ParseHttpDate()
			{
				FDateTime Parsed = FDateTime::MinValue();
				const bool bOk = FDateTime::ParseHttpDate("__HTTP_INPUT__", Parsed);
				return (VerifyDateValue(bOk, __HTTP_SUCCESS__, Parsed, int64(__HTTP_TICKS__), __HTTP_YEAR__, __HTTP_MONTH__, __HTTP_DAY__, __HTTP_HOUR12__, __HTTP_MILLISECOND__, 10) == 0) ? 1 : 0;
			}
			)AS");
		ScriptSource.ReplaceInline(TEXT("__HTTP_INPUT__"), *HttpInput, ESearchCase::CaseSensitive);
		ReplaceValueTokens(ScriptSource, TEXT("HTTP"), HttpBaseline);

		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASDateTime_Http"), ScriptSource);
		if (!Mod.IsValid()) return;
		auto& M = Mod.GetModule();

		ASSERT_THAT(IsTrue(
			ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int DateTime_ParseHttpDate()"), TEXT("FDateTime::ParseHttpDate should parse and match native components"), 1),
			TEXT("ExpectGlobalInt should pass")));
	}

	// ====================================================================
	// Section: ParseGeneric
	// ====================================================================

	TEST_METHOD(ParseGeneric)
	{
		using namespace AngelscriptDateTimeTestHelpers;

		const FString GenericInput = TEXT("2024-12-25 14:30:15.123");
		FDateTime GenericValue = FDateTime::MinValue();
		const FDateTimeBaseline GenericBaseline = Capture(FDateTime::Parse(GenericInput, GenericValue), GenericValue);

		FString ScriptSource = VerifyDateValueFunc() + ASTEST_AS(R"AS(
			int DateTime_ParseGeneric()
			{
				FDateTime Parsed = FDateTime::MinValue();
				const bool bOk = FDateTime::Parse("__GENERIC_INPUT__", Parsed);
				return (VerifyDateValue(bOk, __GENERIC_SUCCESS__, Parsed, int64(__GENERIC_TICKS__), __GENERIC_YEAR__, __GENERIC_MONTH__, __GENERIC_DAY__, __GENERIC_HOUR12__, __GENERIC_MILLISECOND__, 10) == 0) ? 1 : 0;
			}
			)AS");
		ScriptSource.ReplaceInline(TEXT("__GENERIC_INPUT__"), *GenericInput, ESearchCase::CaseSensitive);
		ReplaceValueTokens(ScriptSource, TEXT("GENERIC"), GenericBaseline);

		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASDateTime_Generic"), ScriptSource);
		if (!Mod.IsValid()) return;
		auto& M = Mod.GetModule();

		ASSERT_THAT(IsTrue(
			ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int DateTime_ParseGeneric()"), TEXT("FDateTime::Parse should parse generic format and match native components"), 1),
			TEXT("ExpectGlobalInt should pass")));
	}

	// ====================================================================
	// Section: ParseInvalid
	// ====================================================================

	TEST_METHOD(ParseInvalid)
	{
		using namespace AngelscriptDateTimeTestHelpers;

		const FString InvalidInput = TEXT("not-a-date");
		const FDateTime Sentinel(1999, 1, 2, 3, 4, 5, 6);

		FDateTime InvalidParseValue = Sentinel;
		const FDateTimeBaseline InvalidParse = Capture(FDateTime::Parse(InvalidInput, InvalidParseValue), InvalidParseValue);

		FDateTime InvalidHttpValue = Sentinel;
		const FDateTimeBaseline InvalidHttp = Capture(FDateTime::ParseHttpDate(InvalidInput, InvalidHttpValue), InvalidHttpValue);

		FDateTime InvalidIsoValue = Sentinel;
		const FDateTimeBaseline InvalidIso = Capture(FDateTime::ParseIso8601(*InvalidInput, InvalidIsoValue), InvalidIsoValue);

		FString ScriptSource = VerifyDateValueFunc() + ASTEST_AS(R"AS(
			int DateTime_ParseInvalidGeneric()
			{
				FDateTime Parsed(1999, 1, 2, 3, 4, 5, 6);
				const bool bOk = FDateTime::Parse("__INVALID__", Parsed);
				return (VerifyDateValue(bOk, __INVALID_PARSE_SUCCESS__, Parsed, int64(__INVALID_PARSE_TICKS__), __INVALID_PARSE_YEAR__, __INVALID_PARSE_MONTH__, __INVALID_PARSE_DAY__, __INVALID_PARSE_HOUR12__, __INVALID_PARSE_MILLISECOND__, 10) == 0) ? 1 : 0;
			}

			int DateTime_ParseInvalidHttp()
			{
				FDateTime Parsed(1999, 1, 2, 3, 4, 5, 6);
				const bool bOk = FDateTime::ParseHttpDate("__INVALID__", Parsed);
				return (VerifyDateValue(bOk, __INVALID_HTTP_SUCCESS__, Parsed, int64(__INVALID_HTTP_TICKS__), __INVALID_HTTP_YEAR__, __INVALID_HTTP_MONTH__, __INVALID_HTTP_DAY__, __INVALID_HTTP_HOUR12__, __INVALID_HTTP_MILLISECOND__, 10) == 0) ? 1 : 0;
			}

			int DateTime_ParseInvalidIso()
			{
				FDateTime Parsed(1999, 1, 2, 3, 4, 5, 6);
				const bool bOk = FDateTime::ParseIso8601("__INVALID__", Parsed);
				return (VerifyDateValue(bOk, __INVALID_ISO_SUCCESS__, Parsed, int64(__INVALID_ISO_TICKS__), __INVALID_ISO_YEAR__, __INVALID_ISO_MONTH__, __INVALID_ISO_DAY__, __INVALID_ISO_HOUR12__, __INVALID_ISO_MILLISECOND__, 10) == 0) ? 1 : 0;
			}
			)AS");
		ScriptSource.ReplaceInline(TEXT("__INVALID__"), *InvalidInput, ESearchCase::CaseSensitive);
		ReplaceValueTokens(ScriptSource, TEXT("INVALID_PARSE"), InvalidParse);
		ReplaceValueTokens(ScriptSource, TEXT("INVALID_HTTP"), InvalidHttp);
		ReplaceValueTokens(ScriptSource, TEXT("INVALID_ISO"), InvalidIso);

		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASDateTime_Invalid"), ScriptSource);
		if (!Mod.IsValid()) return;
		auto& M = Mod.GetModule();

		ASSERT_THAT(IsTrue(
			ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int DateTime_ParseInvalidGeneric()"), TEXT("FDateTime::Parse should reject invalid input"), 1),
			TEXT("ExpectGlobalInt should pass")));
		ASSERT_THAT(IsTrue(
			ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int DateTime_ParseInvalidHttp()"), TEXT("FDateTime::ParseHttpDate should reject invalid input"), 1),
			TEXT("ExpectGlobalInt should pass")));
		ASSERT_THAT(IsTrue(
			ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int DateTime_ParseInvalidIso()"), TEXT("FDateTime::ParseIso8601 should reject invalid input"), 1),
			TEXT("ExpectGlobalInt should pass")));
	}

	// ====================================================================
	// Section: Construction
	// ====================================================================

	TEST_METHOD(Construction)
	{
		using namespace AngelscriptDateTimeTestHelpers;

		const FDateTime Constructed(2024, 12, 25, 14, 30, 15, 123);
		const FDateTimeBaseline CtorBaseline = Capture(true, Constructed);
		const FString ConstructedHttp = Constructed.ToHttpDate();
		const FString ConstructedIso = Constructed.ToIso8601();
		const FString ConstructedFormatted = Constructed.ToString(TEXT("%Y-%m-%d %H:%M:%S.%s"));

		FString ScriptSource = VerifyDateValueFunc() + ASTEST_AS(R"AS(
			int DateTime_CtorComponents()
			{
				FDateTime Dt(2024, 12, 25, 14, 30, 15, 123);
				return (VerifyDateValue(true, true, Dt, int64(__CTOR_TICKS__), __CTOR_YEAR__, __CTOR_MONTH__, __CTOR_DAY__, __CTOR_HOUR12__, __CTOR_MILLISECOND__, 10) == 0) ? 1 : 0;
			}

			int DateTime_ToHttpDate()
			{
				FDateTime Dt(2024, 12, 25, 14, 30, 15, 123);
				return (Dt.ToHttpDate() == "__CTOR_HTTP__") ? 1 : 0;
			}

			int DateTime_ToIso8601()
			{
				FDateTime Dt(2024, 12, 25, 14, 30, 15, 123);
				return (Dt.ToIso8601() == "__CTOR_ISO__") ? 1 : 0;
			}

			int DateTime_ToStringFormatted()
			{
				FDateTime Dt(2024, 12, 25, 14, 30, 15, 123);
				return (Dt.ToString("%Y-%m-%d %H:%M:%S.%s") == "__CTOR_FORMATTED__") ? 1 : 0;
			}
			)AS");
		ReplaceValueTokens(ScriptSource, TEXT("CTOR"), CtorBaseline);
		ScriptSource.ReplaceInline(TEXT("__CTOR_HTTP__"), *ConstructedHttp, ESearchCase::CaseSensitive);
		ScriptSource.ReplaceInline(TEXT("__CTOR_ISO__"), *ConstructedIso, ESearchCase::CaseSensitive);
		ScriptSource.ReplaceInline(TEXT("__CTOR_FORMATTED__"), *ConstructedFormatted, ESearchCase::CaseSensitive);

		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASDateTime_Ctor"), ScriptSource);
		if (!Mod.IsValid()) return;
		auto& M = Mod.GetModule();

		ASSERT_THAT(IsTrue(
			ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int DateTime_CtorComponents()"), TEXT("FDateTime component ctor should match native ticks and fields"), 1),
			TEXT("ExpectGlobalInt should pass")));
		ASSERT_THAT(IsTrue(
			ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int DateTime_ToHttpDate()"), TEXT("FDateTime::ToHttpDate should match native output"), 1),
			TEXT("ExpectGlobalInt should pass")));
		ASSERT_THAT(IsTrue(
			ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int DateTime_ToIso8601()"), TEXT("FDateTime::ToIso8601 should match native output"), 1),
			TEXT("ExpectGlobalInt should pass")));
		ASSERT_THAT(IsTrue(
			ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int DateTime_ToStringFormatted()"), TEXT("FDateTime::ToString with format should match native output"), 1),
			TEXT("ExpectGlobalInt should pass")));
	}

	// ====================================================================
	// Section: RoundTrip
	// ====================================================================

	TEST_METHOD(RoundTrip)
	{
		using namespace AngelscriptDateTimeTestHelpers;

		const FDateTime Constructed(2024, 12, 25, 14, 30, 15, 123);
		const FDateTime Sentinel(1999, 1, 2, 3, 4, 5, 6);

		FDateTime HttpRtValue = Sentinel;
		const FDateTimeBaseline HttpRt = Capture(FDateTime::ParseHttpDate(Constructed.ToHttpDate(), HttpRtValue), HttpRtValue);

		FDateTime IsoRtValue = Sentinel;
		const FDateTimeBaseline IsoRt = Capture(FDateTime::ParseIso8601(*Constructed.ToIso8601(), IsoRtValue), IsoRtValue);

		const FString Formatted = Constructed.ToString(TEXT("%Y-%m-%d %H:%M:%S.%s"));
		FDateTime FormattedRtValue = Sentinel;
		const FDateTimeBaseline FormattedRt = Capture(FDateTime::Parse(Formatted, FormattedRtValue), FormattedRtValue);

		FString ScriptSource = VerifyDateValueFunc() + ASTEST_AS(R"AS(
			int DateTime_RoundTripHttp()
			{
				FDateTime Dt(2024, 12, 25, 14, 30, 15, 123);
				FDateTime Parsed = FDateTime::MinValue();
				const bool bOk = FDateTime::ParseHttpDate(Dt.ToHttpDate(), Parsed);
				return (VerifyDateValue(bOk, __HTTP_RT_SUCCESS__, Parsed, int64(__HTTP_RT_TICKS__), __HTTP_RT_YEAR__, __HTTP_RT_MONTH__, __HTTP_RT_DAY__, __HTTP_RT_HOUR12__, __HTTP_RT_MILLISECOND__, 10) == 0) ? 1 : 0;
			}

			int DateTime_RoundTripIso()
			{
				FDateTime Dt(2024, 12, 25, 14, 30, 15, 123);
				FDateTime Parsed = FDateTime::MinValue();
				const bool bOk = FDateTime::ParseIso8601(Dt.ToIso8601(), Parsed);
				return (VerifyDateValue(bOk, __ISO_RT_SUCCESS__, Parsed, int64(__ISO_RT_TICKS__), __ISO_RT_YEAR__, __ISO_RT_MONTH__, __ISO_RT_DAY__, __ISO_RT_HOUR12__, __ISO_RT_MILLISECOND__, 10) == 0) ? 1 : 0;
			}

			int DateTime_RoundTripFormatted()
			{
				FDateTime Dt(2024, 12, 25, 14, 30, 15, 123);
				const FString Fmt = Dt.ToString("%Y-%m-%d %H:%M:%S.%s");
				FDateTime Parsed = FDateTime::MinValue();
				const bool bOk = FDateTime::Parse(Fmt, Parsed);
				return (VerifyDateValue(bOk, __FMT_RT_SUCCESS__, Parsed, int64(__FMT_RT_TICKS__), __FMT_RT_YEAR__, __FMT_RT_MONTH__, __FMT_RT_DAY__, __FMT_RT_HOUR12__, __FMT_RT_MILLISECOND__, 10) == 0) ? 1 : 0;
			}
			)AS");
		ReplaceValueTokens(ScriptSource, TEXT("HTTP_RT"), HttpRt);
		ReplaceValueTokens(ScriptSource, TEXT("ISO_RT"), IsoRt);
		ReplaceValueTokens(ScriptSource, TEXT("FMT_RT"), FormattedRt);

		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASDateTime_RoundTrip"), ScriptSource);
		if (!Mod.IsValid()) return;
		auto& M = Mod.GetModule();

		ASSERT_THAT(IsTrue(
			ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int DateTime_RoundTripHttp()"), TEXT("FDateTime HTTP round-trip should preserve components"), 1),
			TEXT("ExpectGlobalInt should pass")));
		ASSERT_THAT(IsTrue(
			ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int DateTime_RoundTripIso()"), TEXT("FDateTime ISO round-trip should preserve components"), 1),
			TEXT("ExpectGlobalInt should pass")));
		ASSERT_THAT(IsTrue(
			ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int DateTime_RoundTripFormatted()"), TEXT("FDateTime formatted round-trip should preserve components"), 1),
			TEXT("ExpectGlobalInt should pass")));
	}
};

#endif
