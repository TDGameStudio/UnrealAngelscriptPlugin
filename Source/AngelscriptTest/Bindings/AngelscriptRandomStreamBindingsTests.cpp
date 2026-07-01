// ============================================================================
// AngelscriptRandomStreamBindingsTests.cpp
//
// FRandomStream binding coverage �?CQTest refactor. Automation IDs:
//   Angelscript.TestModule.Bindings.RandomStream.FAngelscriptRandomStreamBindingsTest.*
//
// Sections:
//   IntSeedSequence   �?int32 seed construction, GetInitialSeed, GetCurrentSeed,
//                       GetUnsignedInt, RandRange, GetFraction, FRandRange, copy parity
//   IntSeedReset      �?Reset behaviour after sequence consumption
//   UintSeedSequence  �?uint32 seed construction, GetInitialSeed, GetCurrentSeed,
//                       GetUnsignedInt, Reset
//   NameSeedSequence  �?Initialize(FName), GetCurrentSeed, RandRange
//
// CQTest adaptation notes:
//   Native C++ expectations are computed at test time and substituted into script
//   via ReplaceInline tokens so that parity is verified against deterministic output.
// ============================================================================

#include "CQTest.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestModuleScope.h"
#include "AngelscriptTestExecute.h"

#if WITH_ANGELSCRIPT_UNITTESTS


// ----------------------------------------------------------------------------
// Helpers
// ----------------------------------------------------------------------------


// ----------------------------------------------------------------------------
// Profile
// ----------------------------------------------------------------------------


// ----------------------------------------------------------------------------
// Test class
// ----------------------------------------------------------------------------

TEST_CLASS_WITH_FLAGS(FAngelscriptRandomStreamBindingsTest,
	"Angelscript.TestModule.Bindings.RandomStream",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
static FString ToScriptFloatLiteral(double Value)
{
	FString Literal = FString::Printf(TEXT("%.17g"), Value);
	if (!Literal.Contains(TEXT(".")) && !Literal.Contains(TEXT("e")) && !Literal.Contains(TEXT("E")))
	{
		Literal += TEXT(".0");
	}
	return Literal;
}

public:
	BEFORE_ALL()
	{
		ASTEST_CREATE_ENGINE();
	}

	AFTER_ALL() { FAngelscriptEngine& Engine = ASTEST_GET_ENGINE(); ASTEST_RESET_ENGINE(Engine); }

	// ====================================================================
	// Section: IntSeedSequence
	// ====================================================================

	TEST_METHOD(IntSeedSequence)
	{
FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		// Compute native expectations
		FRandomStream Native(123);
		const int32 ExpInitialSeed = Native.GetInitialSeed();
		const int32 ExpCurrentSeed = Native.GetCurrentSeed();
		const uint32 ExpUnsigned = Native.GetUnsignedInt();
		const int32 ExpRange = Native.RandRange(1, 1000);
		const double ExpFraction = Native.GetFraction();
		const double ExpDoubleRange = Native.FRandRange(0.0, 10.0);
		const int32 ExpPostSeed = Native.GetCurrentSeed();

		FRandomStream NativeCopy = Native;
		const int32 ExpCopyNext = NativeCopy.RandRange(1, 1000);
		const int32 ExpStreamNext = Native.RandRange(1, 1000);

		FString Script = TEXT(R"(
bool NearlyEqual(float64 A, float64 B) { return Math::Abs(A - B) <= 0.000001; }

int IntSeed_InitialSeed()
{
	FRandomStream S(123);
	return (S.GetInitialSeed() == __EXP_INITIAL_SEED__) ? 1 : 0;
}
int IntSeed_CurrentSeed()
{
	FRandomStream S(123);
	return (S.GetCurrentSeed() == __EXP_CURRENT_SEED__) ? 1 : 0;
}
int IntSeed_GetUnsignedInt()
{
	FRandomStream S(123);
	return (S.GetUnsignedInt() == __EXP_UNSIGNED__) ? 1 : 0;
}
int IntSeed_RandRange()
{
	FRandomStream S(123);
	S.GetUnsignedInt();
	return (S.RandRange(1, 1000) == __EXP_RANGE__) ? 1 : 0;
}
int IntSeed_GetFraction()
{
	FRandomStream S(123);
	S.GetUnsignedInt();
	S.RandRange(1, 1000);
	return NearlyEqual(S.GetFraction(), __EXP_FRACTION__) ? 1 : 0;
}
int IntSeed_FRandRange()
{
	FRandomStream S(123);
	S.GetUnsignedInt();
	S.RandRange(1, 1000);
	S.GetFraction();
	return NearlyEqual(S.RandRange(0.0, 10.0), __EXP_DOUBLE_RANGE__) ? 1 : 0;
}
int IntSeed_PostSequenceSeed()
{
	FRandomStream S(123);
	S.GetUnsignedInt();
	S.RandRange(1, 1000);
	S.GetFraction();
	S.RandRange(0.0, 10.0);
	return (S.GetCurrentSeed() == __EXP_POST_SEED__) ? 1 : 0;
}
int IntSeed_CopyParity()
{
	FRandomStream S(123);
	S.GetUnsignedInt();
	S.RandRange(1, 1000);
	S.GetFraction();
	S.RandRange(0.0, 10.0);

	FRandomStream Copy = S;
	int CopyNext = Copy.RandRange(1, 1000);
	int StreamNext = S.RandRange(1, 1000);
	if (CopyNext != __EXP_COPY_NEXT__) return 0;
	if (StreamNext != __EXP_STREAM_NEXT__) return 0;
	if (CopyNext != StreamNext) return 0;
	return 1;
}
)");

		Script.ReplaceInline(TEXT("__EXP_INITIAL_SEED__"), *LexToString(ExpInitialSeed), ESearchCase::CaseSensitive);
		Script.ReplaceInline(TEXT("__EXP_CURRENT_SEED__"), *LexToString(ExpCurrentSeed), ESearchCase::CaseSensitive);
		Script.ReplaceInline(TEXT("__EXP_UNSIGNED__"), *LexToString(ExpUnsigned), ESearchCase::CaseSensitive);
		Script.ReplaceInline(TEXT("__EXP_RANGE__"), *LexToString(ExpRange), ESearchCase::CaseSensitive);
		Script.ReplaceInline(TEXT("__EXP_FRACTION__"), *ToScriptFloatLiteral(ExpFraction), ESearchCase::CaseSensitive);
		Script.ReplaceInline(TEXT("__EXP_DOUBLE_RANGE__"), *ToScriptFloatLiteral(ExpDoubleRange), ESearchCase::CaseSensitive);
		Script.ReplaceInline(TEXT("__EXP_POST_SEED__"), *LexToString(ExpPostSeed), ESearchCase::CaseSensitive);
		Script.ReplaceInline(TEXT("__EXP_COPY_NEXT__"), *LexToString(ExpCopyNext), ESearchCase::CaseSensitive);
		Script.ReplaceInline(TEXT("__EXP_STREAM_NEXT__"), *LexToString(ExpStreamNext), ESearchCase::CaseSensitive);

		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASRandomStream_IntSeedSequence"), Script);
		if (!Mod.IsValid()) return;
		auto& M = Mod.GetModule();

		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int IntSeed_InitialSeed()"), TEXT("GetInitialSeed parity"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int IntSeed_CurrentSeed()"), TEXT("GetCurrentSeed parity"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int IntSeed_GetUnsignedInt()"), TEXT("GetUnsignedInt parity"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int IntSeed_RandRange()"), TEXT("RandRange parity"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int IntSeed_GetFraction()"), TEXT("GetFraction parity"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int IntSeed_FRandRange()"), TEXT("FRandRange parity"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int IntSeed_PostSequenceSeed()"), TEXT("post-sequence seed parity"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int IntSeed_CopyParity()"), TEXT("copy produces identical sequence"), 1);
	}

	// ====================================================================
	// Section: IntSeedReset
	// ====================================================================

	TEST_METHOD(IntSeedReset)
	{
FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		// Compute native expectation
		FRandomStream Native(123);
		Native.GetUnsignedInt();
		Native.RandRange(1, 1000);
		Native.GetFraction();
		Native.FRandRange(0.0, 10.0);
		Native.RandRange(1, 1000);
		Native.Reset();
		const int32 ExpResetValue = Native.RandRange(1, 1000);

		FString Script = TEXT(R"(
int IntSeed_Reset()
{
	FRandomStream S(123);
	S.GetUnsignedInt();
	S.RandRange(1, 1000);
	S.GetFraction();
	S.RandRange(0.0, 10.0);
	S.RandRange(1, 1000);
	S.Reset();
	return (S.RandRange(1, 1000) == __EXP_RESET__) ? 1 : 0;
}
)");
		Script.ReplaceInline(TEXT("__EXP_RESET__"), *LexToString(ExpResetValue), ESearchCase::CaseSensitive);

		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASRandomStream_IntSeedReset"), Script);
		if (!Mod.IsValid()) return;
		auto& M = Mod.GetModule();

		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int IntSeed_Reset()"), TEXT("Reset restores initial sequence"), 1);
	}

	// ====================================================================
	// Section: UintSeedSequence
	// ====================================================================

	TEST_METHOD(UintSeedSequence)
	{
FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FRandomStream Native(uint32(123));
		const int32 ExpInitialSeed = Native.GetInitialSeed();
		const int32 ExpCurrentSeed = Native.GetCurrentSeed();
		const uint32 ExpUnsigned = Native.GetUnsignedInt();
		Native.Reset();
		const int32 ExpResetValue = Native.RandRange(1, 1000);

		FString Script = TEXT(R"(
int UintSeed_InitialSeed()
{
	FRandomStream S(uint32(123));
	return (S.GetInitialSeed() == __EXP_INITIAL_SEED__) ? 1 : 0;
}
int UintSeed_CurrentSeed()
{
	FRandomStream S(uint32(123));
	return (S.GetCurrentSeed() == __EXP_CURRENT_SEED__) ? 1 : 0;
}
int UintSeed_GetUnsignedInt()
{
	FRandomStream S(uint32(123));
	return (S.GetUnsignedInt() == __EXP_UNSIGNED__) ? 1 : 0;
}
int UintSeed_Reset()
{
	FRandomStream S(uint32(123));
	S.GetUnsignedInt();
	S.Reset();
	return (S.RandRange(1, 1000) == __EXP_RESET__) ? 1 : 0;
}
)");
		Script.ReplaceInline(TEXT("__EXP_INITIAL_SEED__"), *LexToString(ExpInitialSeed), ESearchCase::CaseSensitive);
		Script.ReplaceInline(TEXT("__EXP_CURRENT_SEED__"), *LexToString(ExpCurrentSeed), ESearchCase::CaseSensitive);
		Script.ReplaceInline(TEXT("__EXP_UNSIGNED__"), *LexToString(ExpUnsigned), ESearchCase::CaseSensitive);
		Script.ReplaceInline(TEXT("__EXP_RESET__"), *LexToString(ExpResetValue), ESearchCase::CaseSensitive);

		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASRandomStream_UintSeedSequence"), Script);
		if (!Mod.IsValid()) return;
		auto& M = Mod.GetModule();

		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int UintSeed_InitialSeed()"), TEXT("uint32 seed GetInitialSeed"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int UintSeed_CurrentSeed()"), TEXT("uint32 seed GetCurrentSeed"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int UintSeed_GetUnsignedInt()"), TEXT("uint32 seed GetUnsignedInt"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int UintSeed_Reset()"), TEXT("uint32 seed Reset"), 1);
	}

	// ====================================================================
	// Section: NameSeedSequence
	// ====================================================================

	TEST_METHOD(NameSeedSequence)
	{
FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FRandomStream Native;
		Native.Initialize(FName(TEXT("RandomSeedName")));
		const int32 ExpCurrentSeed = Native.GetCurrentSeed();
		const int32 ExpFirstRange = Native.RandRange(1, 1000);

		FString Script = TEXT(R"(
int NameSeed_CurrentSeed()
{
	FRandomStream S;
	S.Initialize(n"RandomSeedName");
	return (S.GetCurrentSeed() == __EXP_CURRENT_SEED__) ? 1 : 0;
}
int NameSeed_RandRange()
{
	FRandomStream S;
	S.Initialize(n"RandomSeedName");
	return (S.RandRange(1, 1000) == __EXP_FIRST_RANGE__) ? 1 : 0;
}
)");
		Script.ReplaceInline(TEXT("__EXP_CURRENT_SEED__"), *LexToString(ExpCurrentSeed), ESearchCase::CaseSensitive);
		Script.ReplaceInline(TEXT("__EXP_FIRST_RANGE__"), *LexToString(ExpFirstRange), ESearchCase::CaseSensitive);

		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASRandomStream_NameSeedSequence"), Script);
		if (!Mod.IsValid()) return;
		auto& M = Mod.GetModule();

		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int NameSeed_CurrentSeed()"), TEXT("FName seed GetCurrentSeed"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int NameSeed_RandRange()"), TEXT("FName seed RandRange"), 1);
	}
};

#endif
