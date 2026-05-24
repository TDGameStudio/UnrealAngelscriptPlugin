// ============================================================================
// AngelscriptGuidBindingsTests.cpp
//
// FGuid binding coverage — CQTest refactor. Automation IDs:
//   Angelscript.TestModule.Bindings.Guid.FAngelscriptGuidBindingsTest.*
//
// Sections:
//   FormatAndSlots    — construction, ToString formats, operator[] access
//   ParseSuccess      — Parse/ParseExact with valid DigitsWithHyphens and Digits
//   ParseFailure      — ParseExact wrong format, Parse invalid string
//   StringConstructor — FGuid(string) construction from formatted strings
//
// CQTest adaptation notes:
//   FGuid literal values are computed from native baselines and substituted via
//   ReplaceInline into the script source at test time.
// ============================================================================

#include "CQTest.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestModuleScope.h"
#include "AngelscriptTestExecute.h"

#include "Misc/Guid.h"

#if WITH_DEV_AUTOMATION_TESTS


// ----------------------------------------------------------------------------
// Profile
// ----------------------------------------------------------------------------


// ----------------------------------------------------------------------------
// Helpers
// ----------------------------------------------------------------------------

namespace AngelscriptGuidTestHelpers
{
	static FString ToScriptUIntLiteral(const uint32 Value)
	{
		return FString::Printf(TEXT("%u"), Value);
	}

	static void ReplaceGuidTokens(FString& Source, const TCHAR* Prefix, const FGuid& Value)
	{
		Source.ReplaceInline(*FString::Printf(TEXT("__%s_A__"), Prefix), *ToScriptUIntLiteral(Value[0]), ESearchCase::CaseSensitive);
		Source.ReplaceInline(*FString::Printf(TEXT("__%s_B__"), Prefix), *ToScriptUIntLiteral(Value[1]), ESearchCase::CaseSensitive);
		Source.ReplaceInline(*FString::Printf(TEXT("__%s_C__"), Prefix), *ToScriptUIntLiteral(Value[2]), ESearchCase::CaseSensitive);
		Source.ReplaceInline(*FString::Printf(TEXT("__%s_D__"), Prefix), *ToScriptUIntLiteral(Value[3]), ESearchCase::CaseSensitive);
	}
}

// ----------------------------------------------------------------------------
// Test class
// ----------------------------------------------------------------------------

TEST_CLASS_WITH_FLAGS(FAngelscriptGuidBindingsTest,
	"Angelscript.TestModule.Bindings.Guid",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	BEFORE_ALL()
	{
		ASTEST_CREATE_ENGINE();
	}

	AFTER_ALL() { FAngelscriptEngine& Engine = ASTEST_GET_ENGINE(); ASTEST_RESET_ENGINE(Engine); }

	// ====================================================================
	// Section: FormatAndSlots
	// ====================================================================

	TEST_METHOD(FormatAndSlots)
	{
		using namespace AngelscriptGuidTestHelpers;

		const FGuid ExplicitGuid(1, 2, 3, 4);
		const FString WithHyphens = ExplicitGuid.ToString(EGuidFormats::DigitsWithHyphens);
		const FString Digits = ExplicitGuid.ToString(EGuidFormats::Digits);

		FString ScriptSource = TEXT(R"(
int Guid_ToStringHyphens()
{
	const FGuid G(__GUID_A__, __GUID_B__, __GUID_C__, __GUID_D__);
	return (G.ToString(EGuidFormats::DigitsWithHyphens) == "__HYPHENS__") ? 1 : 0;
}
int Guid_ToStringDigits()
{
	const FGuid G(__GUID_A__, __GUID_B__, __GUID_C__, __GUID_D__);
	return (G.ToString(EGuidFormats::Digits) == "__DIGITS__") ? 1 : 0;
}
int Guid_SlotAccess()
{
	const FGuid G(__GUID_A__, __GUID_B__, __GUID_C__, __GUID_D__);
	return (G[0] == __GUID_A__ && G[1] == __GUID_B__ && G[2] == __GUID_C__ && G[3] == __GUID_D__) ? 1 : 0;
}
)");
		ReplaceGuidTokens(ScriptSource, TEXT("GUID"), ExplicitGuid);
		ScriptSource.ReplaceInline(TEXT("__HYPHENS__"), *WithHyphens, ESearchCase::CaseSensitive);
		ScriptSource.ReplaceInline(TEXT("__DIGITS__"), *Digits, ESearchCase::CaseSensitive);

		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASGuid_FormatSlots"), ScriptSource);
		if (!Mod.IsValid()) return;
		auto& M = Mod.GetModule();

		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int Guid_ToStringHyphens()"), TEXT("FGuid ToString DigitsWithHyphens should match native"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int Guid_ToStringDigits()"), TEXT("FGuid ToString Digits should match native"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int Guid_SlotAccess()"), TEXT("FGuid operator[] should return correct components"), 1);
	}

	// ====================================================================
	// Section: ParseSuccess
	// ====================================================================

	TEST_METHOD(ParseSuccess)
	{
		using namespace AngelscriptGuidTestHelpers;

		const FGuid ExplicitGuid(1, 2, 3, 4);
		const FGuid SentinelGuid(91, 92, 93, 94);
		const FString WithHyphens = ExplicitGuid.ToString(EGuidFormats::DigitsWithHyphens);
		const FString Digits = ExplicitGuid.ToString(EGuidFormats::Digits);

		FString ScriptSource = TEXT(R"(
int Guid_ParseHyphens()
{
	FGuid Parsed(__SENTINEL_A__, __SENTINEL_B__, __SENTINEL_C__, __SENTINEL_D__);
	if (!FGuid::Parse("__HYPHENS__", Parsed))
		return 0;
	const FGuid Expected(__GUID_A__, __GUID_B__, __GUID_C__, __GUID_D__);
	return (Parsed == Expected) ? 1 : 0;
}
int Guid_ParseDigits()
{
	FGuid Parsed(__SENTINEL_A__, __SENTINEL_B__, __SENTINEL_C__, __SENTINEL_D__);
	if (!FGuid::Parse("__DIGITS__", Parsed))
		return 0;
	const FGuid Expected(__GUID_A__, __GUID_B__, __GUID_C__, __GUID_D__);
	return (Parsed == Expected) ? 1 : 0;
}
int Guid_ParseExactHyphens()
{
	FGuid Parsed(__SENTINEL_A__, __SENTINEL_B__, __SENTINEL_C__, __SENTINEL_D__);
	if (!FGuid::ParseExact("__HYPHENS__", EGuidFormats::DigitsWithHyphens, Parsed))
		return 0;
	const FGuid Expected(__GUID_A__, __GUID_B__, __GUID_C__, __GUID_D__);
	return (Parsed == Expected) ? 1 : 0;
}
int Guid_ParseExactDigits()
{
	FGuid Parsed(__SENTINEL_A__, __SENTINEL_B__, __SENTINEL_C__, __SENTINEL_D__);
	if (!FGuid::ParseExact("__DIGITS__", EGuidFormats::Digits, Parsed))
		return 0;
	const FGuid Expected(__GUID_A__, __GUID_B__, __GUID_C__, __GUID_D__);
	return (Parsed == Expected) ? 1 : 0;
}
)");
		ReplaceGuidTokens(ScriptSource, TEXT("GUID"), ExplicitGuid);
		ReplaceGuidTokens(ScriptSource, TEXT("SENTINEL"), SentinelGuid);
		ScriptSource.ReplaceInline(TEXT("__HYPHENS__"), *WithHyphens, ESearchCase::CaseSensitive);
		ScriptSource.ReplaceInline(TEXT("__DIGITS__"), *Digits, ESearchCase::CaseSensitive);

		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASGuid_ParseOk"), ScriptSource);
		if (!Mod.IsValid()) return;
		auto& M = Mod.GetModule();

		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int Guid_ParseHyphens()"), TEXT("FGuid::Parse should accept DigitsWithHyphens format"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int Guid_ParseDigits()"), TEXT("FGuid::Parse should accept Digits format"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int Guid_ParseExactHyphens()"), TEXT("FGuid::ParseExact should accept DigitsWithHyphens"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int Guid_ParseExactDigits()"), TEXT("FGuid::ParseExact should accept Digits"), 1);
	}

	// ====================================================================
	// Section: ParseFailure
	// ====================================================================

	TEST_METHOD(ParseFailure)
	{
		using namespace AngelscriptGuidTestHelpers;

		const FGuid ExplicitGuid(1, 2, 3, 4);
		const FGuid SentinelGuid(91, 92, 93, 94);
		const FString WithHyphens = ExplicitGuid.ToString(EGuidFormats::DigitsWithHyphens);
		const FString InvalidInput = TEXT("not-a-guid");

		FString ScriptSource = TEXT(R"(
int Guid_ParseExactWrongFormat()
{
	FGuid Parsed(__SENTINEL_A__, __SENTINEL_B__, __SENTINEL_C__, __SENTINEL_D__);
	const FGuid Sentinel(__SENTINEL_A__, __SENTINEL_B__, __SENTINEL_C__, __SENTINEL_D__);
	if (FGuid::ParseExact("__HYPHENS__", EGuidFormats::Digits, Parsed))
		return 0;
	return (Parsed == Sentinel) ? 1 : 0;
}
int Guid_ParseInvalid()
{
	FGuid Parsed(__SENTINEL_A__, __SENTINEL_B__, __SENTINEL_C__, __SENTINEL_D__);
	const FGuid Sentinel(__SENTINEL_A__, __SENTINEL_B__, __SENTINEL_C__, __SENTINEL_D__);
	if (FGuid::Parse("__INVALID__", Parsed))
		return 0;
	return (Parsed == Sentinel) ? 1 : 0;
}
)");
		ReplaceGuidTokens(ScriptSource, TEXT("SENTINEL"), SentinelGuid);
		ScriptSource.ReplaceInline(TEXT("__HYPHENS__"), *WithHyphens, ESearchCase::CaseSensitive);
		ScriptSource.ReplaceInline(TEXT("__INVALID__"), *InvalidInput, ESearchCase::CaseSensitive);

		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASGuid_ParseFail"), ScriptSource);
		if (!Mod.IsValid()) return;
		auto& M = Mod.GetModule();

		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int Guid_ParseExactWrongFormat()"), TEXT("FGuid::ParseExact should reject wrong format and preserve sentinel"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int Guid_ParseInvalid()"), TEXT("FGuid::Parse should reject invalid text and preserve sentinel"), 1);
	}

	// ====================================================================
	// Section: StringConstructor
	// ====================================================================

	TEST_METHOD(StringConstructor)
	{
		using namespace AngelscriptGuidTestHelpers;

		const FGuid ExplicitGuid(1, 2, 3, 4);
		const FString WithHyphens = ExplicitGuid.ToString(EGuidFormats::DigitsWithHyphens);
		const FString Digits = ExplicitGuid.ToString(EGuidFormats::Digits);

		FString ScriptSource = TEXT(R"(
int Guid_CtorFromHyphens()
{
	const FGuid FromStr("__HYPHENS__");
	const FGuid Expected(__GUID_A__, __GUID_B__, __GUID_C__, __GUID_D__);
	return (FromStr == Expected) ? 1 : 0;
}
int Guid_CtorFromDigits()
{
	const FGuid FromStr("__DIGITS__");
	const FGuid Expected(__GUID_A__, __GUID_B__, __GUID_C__, __GUID_D__);
	return (FromStr == Expected) ? 1 : 0;
}
)");
		ReplaceGuidTokens(ScriptSource, TEXT("GUID"), ExplicitGuid);
		ScriptSource.ReplaceInline(TEXT("__HYPHENS__"), *WithHyphens, ESearchCase::CaseSensitive);
		ScriptSource.ReplaceInline(TEXT("__DIGITS__"), *Digits, ESearchCase::CaseSensitive);

		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASGuid_StrCtor"), ScriptSource);
		if (!Mod.IsValid()) return;
		auto& M = Mod.GetModule();

		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int Guid_CtorFromHyphens()"), TEXT("FGuid string ctor should parse DigitsWithHyphens"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int Guid_CtorFromDigits()"), TEXT("FGuid string ctor should parse Digits"), 1);
	}
};

#endif
