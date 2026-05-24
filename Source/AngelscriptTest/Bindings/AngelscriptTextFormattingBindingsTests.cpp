// ============================================================================
// AngelscriptTextFormattingBindingsTests.cpp
//
// FFormatArgumentValue / FText::Format binding coverage — CQTest pattern.
// Automation ID:
//   Angelscript.TestModule.Bindings.TextFormatting.FAngelscriptTextFormattingBindingsTest.*
//
// Sections:
//   OrderedFormat — ordered FFormatArgumentValue args + FText::Format
//   NamedFormat   — named FFormatArgumentValue args + FText::Format
//
// Each section computes the C++ expected string at runtime, injects it into
// the AS source via ReplaceInline, and verifies the AS-side formatted result
// matches (returns 1 on match, 0 on mismatch).
// ============================================================================

#include "CQTest.h"
#include "Shared/AngelscriptTestMacros.h"
#include "Shared/AngelscriptTestModuleScope.h"
#include "Shared/AngelscriptBindingsAssertions.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptTestSupport;
using namespace AngelscriptTestBindings;
using namespace AngelscriptReflectiveAccess;

// ----------------------------------------------------------------------------
// Profile
// ----------------------------------------------------------------------------


// ----------------------------------------------------------------------------
// Helpers — compute C++ baselines at runtime
// ----------------------------------------------------------------------------

namespace AngelscriptTest_TextFormatting_Private
{
	FString BuildOrderedExpected()
	{
		const FText Pattern = FText::FromString(TEXT("{0}|{1}|{2}|{3}|{4}|{5}|{6}"));
		FFormatOrderedArguments Args;
		Args.Add(FFormatArgumentValue(int32(-7)));
		Args.Add(FFormatArgumentValue(uint32(42)));
		Args.Add(FFormatArgumentValue(int64(9000000000ll)));
		Args.Add(FFormatArgumentValue(uint64(15)));
		Args.Add(FFormatArgumentValue(float(3.25f)));
		Args.Add(FFormatArgumentValue(double(6.5)));
		Args.Add(FFormatArgumentValue(FText::FromString(TEXT("Alpha"))));
		return FText::Format(Pattern, Args).ToString().ReplaceCharWithEscapedChar();
	}

	FString BuildNamedExpected()
	{
		const FText Pattern = FText::FromString(TEXT("{Int32}|{UInt32}|{Int64}|{UInt64}|{Float32}|{Float64}|{Text}"));
		FFormatNamedArguments Args;
		Args.Add(TEXT("Int32"), FFormatArgumentValue(int32(-7)));
		Args.Add(TEXT("UInt32"), FFormatArgumentValue(uint32(42)));
		Args.Add(TEXT("Int64"), FFormatArgumentValue(int64(9000000000ll)));
		Args.Add(TEXT("UInt64"), FFormatArgumentValue(uint64(15)));
		Args.Add(TEXT("Float32"), FFormatArgumentValue(float(3.25f)));
		Args.Add(TEXT("Float64"), FFormatArgumentValue(double(6.5)));
		Args.Add(TEXT("Text"), FFormatArgumentValue(FText::FromString(TEXT("Alpha"))));
		return FText::Format(Pattern, Args).ToString().ReplaceCharWithEscapedChar();
	}
}

// ----------------------------------------------------------------------------
// Test class
// ----------------------------------------------------------------------------

TEST_CLASS_WITH_FLAGS(FAngelscriptTextFormattingBindingsTest,
	"Angelscript.TestModule.Bindings.TextFormatting",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	BEFORE_ALL()
	{
		ASTEST_CREATE_ENGINE();
	}

	AFTER_ALL() { FAngelscriptEngine& Engine = ASTEST_GET_ENGINE(); ASTEST_RESET_ENGINE(Engine); }

	// ====================================================================
	// Section: OrderedFormat
	// ====================================================================

	TEST_METHOD(OrderedFormat)
	{
		using namespace AngelscriptTest_TextFormatting_Private;

		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		const FString OrderedExpected = BuildOrderedExpected();

		FString Source = TEXT(R"(
int OrderedFormat_Match()
{
	TArray<FFormatArgumentValue> OrderedArgs;
	OrderedArgs.Add(FFormatArgumentValue(int32(-7)));
	OrderedArgs.Add(FFormatArgumentValue(uint32(42)));
	OrderedArgs.Add(FFormatArgumentValue(int64(9000000000)));
	OrderedArgs.Add(FFormatArgumentValue(uint64(15)));
	OrderedArgs.Add(FFormatArgumentValue(float32(3.25)));
	OrderedArgs.Add(FFormatArgumentValue(float64(6.5)));
	OrderedArgs.Add(FFormatArgumentValue(FText::FromString("Alpha")));

	FText Result = FText::Format(FText::FromString("{0}|{1}|{2}|{3}|{4}|{5}|{6}"), OrderedArgs);
	if (Result.ToString() == "__ORDERED_EXPECTED__")
		return 1;
	return 0;
}
)");
		Source.ReplaceInline(TEXT("__ORDERED_EXPECTED__"), *OrderedExpected, ESearchCase::CaseSensitive);

		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASTextFormatting_OrderedFormat"), Source);
		if (!Mod.IsValid()) return;
		auto& M = Mod.GetModule();

		AngelscriptTestBindings::ExpectGlobalInt(*TestRunner, Engine, M, 
			TEXT("int OrderedFormat_Match()"),
			TEXT("Ordered FFormatArgumentValue args should produce expected FText::Format output"),
			1);
	}

	// ====================================================================
	// Section: NamedFormat
	// ====================================================================

	TEST_METHOD(NamedFormat)
	{
		using namespace AngelscriptTest_TextFormatting_Private;

		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		const FString NamedExpected = BuildNamedExpected();

		FString Source = TEXT(R"(
int NamedFormat_Match()
{
	TMap<FString, FFormatArgumentValue> NamedArgs;
	NamedArgs.Add("Int32", FFormatArgumentValue(int32(-7)));
	NamedArgs.Add("UInt32", FFormatArgumentValue(uint32(42)));
	NamedArgs.Add("Int64", FFormatArgumentValue(int64(9000000000)));
	NamedArgs.Add("UInt64", FFormatArgumentValue(uint64(15)));
	NamedArgs.Add("Float32", FFormatArgumentValue(float32(3.25)));
	NamedArgs.Add("Float64", FFormatArgumentValue(float64(6.5)));
	NamedArgs.Add("Text", FFormatArgumentValue(FText::FromString("Alpha")));

	FText Result = FText::Format(FText::FromString("{Int32}|{UInt32}|{Int64}|{UInt64}|{Float32}|{Float64}|{Text}"), NamedArgs);
	if (Result.ToString() == "__NAMED_EXPECTED__")
		return 1;
	return 0;
}
)");
		Source.ReplaceInline(TEXT("__NAMED_EXPECTED__"), *NamedExpected, ESearchCase::CaseSensitive);

		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASTextFormatting_NamedFormat"), Source);
		if (!Mod.IsValid()) return;
		auto& M = Mod.GetModule();

		AngelscriptTestBindings::ExpectGlobalInt(*TestRunner, Engine, M, 
			TEXT("int NamedFormat_Match()"),
			TEXT("Named FFormatArgumentValue args should produce expected FText::Format output"),
			1);
	}
};

#endif
