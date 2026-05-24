// ============================================================================
// AngelscriptUtilityBindingsTests.cpp
//
// Utility binding coverage — CQTest refactor. Automation IDs:
//   Angelscript.TestModule.Bindings.Utility.FAngelscriptUtilityBindingsTest.*
//
// Sections:
//   Hash          — CityHash32, CityHash64, CityHash64WithSeed(s)
//   CommandLine   — FCommandLine::Get, FCommandLine::Parse, FApp, FPlatformMisc
//   Parse         — FParse::Value (int, float, string), FParse::Bool
//   RandomStream  — seed, reset, range, fraction, copy, GenerateNewSeed
//   StringRemoveAt — FString::RemoveAt correctness
//
// CQTest adaptation notes:
//   CommandLine test requires runtime template substitution for expected values.
// ============================================================================

#include "CQTest.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestModuleScope.h"
#include "AngelscriptTestExecute.h"

#include "HAL/PlatformMisc.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"

#if WITH_DEV_AUTOMATION_TESTS


// ----------------------------------------------------------------------------
// Profile
// ----------------------------------------------------------------------------


// ----------------------------------------------------------------------------
// Test class
// ----------------------------------------------------------------------------

TEST_CLASS_WITH_FLAGS(FAngelscriptUtilityBindingsTest,
	"Angelscript.TestModule.Bindings.Utility",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	BEFORE_ALL()
	{
		ASTEST_CREATE_ENGINE();
	}

	AFTER_ALL() { FAngelscriptEngine& Engine = ASTEST_GET_ENGINE(); ASTEST_RESET_ENGINE(Engine); }

	// ====================================================================
	// Section: Hash
	// ====================================================================

	TEST_METHOD(Hash)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASUtility_Hash"), TEXT(R"(
int Hash_CityHash32Deterministic()
{
	uint Hash32A = Hash::CityHash32("Alpha");
	uint Hash32B = Hash::CityHash32("Alpha");
	return (Hash32A == Hash32B) ? 1 : 0;
}

int Hash_CityHash64Deterministic()
{
	uint64 Hash64A = Hash::CityHash64("Alpha");
	uint64 Hash64B = Hash::CityHash64("Alpha");
	return (Hash64A == Hash64B) ? 1 : 0;
}

int Hash_CityHash64WithSeedDeterministic()
{
	uint64 SeededA = Hash::CityHash64WithSeed("Alpha", 123);
	uint64 SeededB = Hash::CityHash64WithSeed("Alpha", 123);
	return (SeededA == SeededB) ? 1 : 0;
}

int Hash_CityHash64WithSeedsDeterministic()
{
	uint64 SeededC = Hash::CityHash64WithSeeds("Alpha", 1, 2);
	uint64 SeededD = Hash::CityHash64WithSeeds("Alpha", 1, 2);
	return (SeededC == SeededD) ? 1 : 0;
}

int Hash_SeededDiffersFromUnseeded()
{
	uint64 Hash64A = Hash::CityHash64("Alpha");
	uint64 SeededA = Hash::CityHash64WithSeed("Alpha", 123);
	return (Hash64A != SeededA) ? 1 : 0;
}
)"));
		if (!Mod.IsValid()) return;
		auto& M = Mod.GetModule();

		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int Hash_CityHash32Deterministic()"), TEXT("CityHash32 should be deterministic"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int Hash_CityHash64Deterministic()"), TEXT("CityHash64 should be deterministic"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int Hash_CityHash64WithSeedDeterministic()"), TEXT("CityHash64WithSeed should be deterministic"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int Hash_CityHash64WithSeedsDeterministic()"), TEXT("CityHash64WithSeeds should be deterministic"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int Hash_SeededDiffersFromUnseeded()"), TEXT("Seeded hash should differ from unseeded"), 1);
	}

	// ====================================================================
	// Section: CommandLine
	// ====================================================================

	TEST_METHOD(CommandLine)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		TArray<FString> ExpectedTokens;
		TArray<FString> ExpectedSwitches;
		FCommandLine::Parse(TEXT("-foo Alpha Beta"), ExpectedTokens, ExpectedSwitches);
		const FString ProjectName = FApp::GetProjectName();

		const FString Script = FString::Printf(
			TEXT(R"(
int CommandLine_GetNotEmpty()
{
	FString CmdLine = FCommandLine::Get();
	return (!CmdLine.IsEmpty()) ? 1 : 0;
}

int CommandLine_ParseTokenCount()
{
	TArray<FString> Tokens;
	TArray<FString> Switches;
	FCommandLine::Parse("-foo Alpha Beta", Tokens, Switches);
	return (Tokens.Num() == %d) ? 1 : 0;
}

int CommandLine_ParseSwitchCount()
{
	TArray<FString> Tokens;
	TArray<FString> Switches;
	FCommandLine::Parse("-foo Alpha Beta", Tokens, Switches);
	return (Switches.Num() == %d) ? 1 : 0;
}

int CommandLine_ParseSwitchValue()
{
	TArray<FString> Tokens;
	TArray<FString> Switches;
	FCommandLine::Parse("-foo Alpha Beta", Tokens, Switches);
	return (Switches.Num() > 0 && Switches[0] == "foo") ? 1 : 0;
}

int CommandLine_AppGetProjectName()
{
	FString RuntimeProjectName = FApp::GetProjectName();
	return (RuntimeProjectName == "%s") ? 1 : 0;
}

int CommandLine_PlatformMiscGetEnvVar()
{
	FString PathEnv = FPlatformMisc::GetEnvironmentVariable("PATH");
	return (!PathEnv.IsEmpty()) ? 1 : 0;
}
)"),
			ExpectedTokens.Num(),
			ExpectedSwitches.Num(),
			*ProjectName);

		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASUtility_CmdLine"), Script);
		if (!Mod.IsValid()) return;
		auto& M = Mod.GetModule();

		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int CommandLine_GetNotEmpty()"), TEXT("FCommandLine::Get should return non-empty string"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int CommandLine_ParseTokenCount()"), TEXT("FCommandLine::Parse should produce correct token count"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int CommandLine_ParseSwitchCount()"), TEXT("FCommandLine::Parse should produce correct switch count"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int CommandLine_ParseSwitchValue()"), TEXT("FCommandLine::Parse should extract switch name correctly"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int CommandLine_AppGetProjectName()"), TEXT("FApp::GetProjectName should match native value"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int CommandLine_PlatformMiscGetEnvVar()"), TEXT("FPlatformMisc::GetEnvironmentVariable should return PATH"), 1);
	}

	// ====================================================================
	// Section: Parse
	// ====================================================================

	TEST_METHOD(Parse)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASUtility_Parse"), TEXT(R"(
int Parse_ValueInt()
{
	FString Source = "Count=12 Ratio=3.5 Name=Alpha Enabled=true";
	int Count = 0;
	if (!FParse::Value(Source, "Count=", Count))
		return 0;
	return (Count == 12) ? 1 : 0;
}

int Parse_ValueFloat()
{
	FString Source = "Count=12 Ratio=3.5 Name=Alpha Enabled=true";
	float32 Ratio = 0.0f;
	if (!FParse::Value(Source, "Ratio=", Ratio))
		return 0;
	return (Ratio == 3.5f) ? 1 : 0;
}

int Parse_ValueString()
{
	FString Source = "Count=12 Ratio=3.5 Name=Alpha Enabled=true";
	FString Name;
	if (!FParse::Value(Source, "Name=", Name))
		return 0;
	return (Name == "Alpha") ? 1 : 0;
}

int Parse_Bool()
{
	FString Source = "Count=12 Ratio=3.5 Name=Alpha Enabled=true";
	bool bEnabled = false;
	if (!FParse::Bool(Source, "Enabled=", bEnabled))
		return 0;
	return (bEnabled) ? 1 : 0;
}
)"));
		if (!Mod.IsValid()) return;
		auto& M = Mod.GetModule();

		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int Parse_ValueInt()"), TEXT("FParse::Value should parse int correctly"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int Parse_ValueFloat()"), TEXT("FParse::Value should parse float correctly"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int Parse_ValueString()"), TEXT("FParse::Value should parse string correctly"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int Parse_Bool()"), TEXT("FParse::Bool should parse bool correctly"), 1);
	}

	// ====================================================================
	// Section: RandomStream
	// ====================================================================

	TEST_METHOD(RandomStream)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASUtility_RandStream"), TEXT(R"(
int RandStream_InitialSeed()
{
	FRandomStream Stream(123);
	return (Stream.GetInitialSeed() == 123) ? 1 : 0;
}

int RandStream_ResetDeterministic()
{
	FRandomStream Stream(123);
	int FirstValue = Stream.RandRange(1, 1000);
	Stream.Reset();
	int SecondValue = Stream.RandRange(1, 1000);
	return (FirstValue == SecondValue) ? 1 : 0;
}

int RandStream_CurrentSeedNonZero()
{
	FRandomStream Stream(123);
	Stream.RandRange(1, 1000);
	return (Stream.GetCurrentSeed() != 0) ? 1 : 0;
}

int RandStream_FractionInRange()
{
	FRandomStream Stream(123);
	float Fraction = Stream.GetFraction();
	return (Fraction >= 0.0 && Fraction <= 1.0) ? 1 : 0;
}

int RandStream_DoubleRangeInBounds()
{
	FRandomStream Stream(123);
	double DoubleValue = Stream.RandRange(0.0, 10.0);
	return (DoubleValue >= 0.0 && DoubleValue <= 10.0) ? 1 : 0;
}

int RandStream_CopyPreservesSeed()
{
	FRandomStream Stream(123);
	FRandomStream Copy = Stream;
	return (Copy.GetCurrentSeed() == Stream.GetCurrentSeed()) ? 1 : 0;
}

int RandStream_GenerateNewSeedNonZero()
{
	FRandomStream Stream(123);
	Stream.GenerateNewSeed();
	return (Stream.GetCurrentSeed() != 0) ? 1 : 0;
}

int RandStream_ToStringNotEmpty()
{
	FRandomStream Stream(123);
	return (!Stream.ToString().IsEmpty()) ? 1 : 0;
}
)"));
		if (!Mod.IsValid()) return;
		auto& M = Mod.GetModule();

		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int RandStream_InitialSeed()"), TEXT("FRandomStream initial seed should match constructor arg"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int RandStream_ResetDeterministic()"), TEXT("FRandomStream Reset should reproduce same sequence"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int RandStream_CurrentSeedNonZero()"), TEXT("FRandomStream current seed should be non-zero after use"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int RandStream_FractionInRange()"), TEXT("FRandomStream GetFraction should return value in [0,1]"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int RandStream_DoubleRangeInBounds()"), TEXT("FRandomStream double RandRange should be in bounds"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int RandStream_CopyPreservesSeed()"), TEXT("FRandomStream copy should preserve current seed"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int RandStream_GenerateNewSeedNonZero()"), TEXT("FRandomStream GenerateNewSeed should produce non-zero seed"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int RandStream_ToStringNotEmpty()"), TEXT("FRandomStream ToString should not be empty"), 1);
	}

	// ====================================================================
	// Section: StringRemoveAt
	// ====================================================================

	TEST_METHOD(StringRemoveAt)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASUtility_StrRemove"), TEXT(R"(
int StrRemove_MiddleChars()
{
	FString Value = "ABCDE";
	Value.RemoveAt(1, 2);
	return (Value == "ADE") ? 1 : 0;
}

int StrRemove_FirstChar()
{
	FString Value = "ADE";
	Value.RemoveAt(0, 1);
	return (Value == "DE") ? 1 : 0;
}
)"));
		if (!Mod.IsValid()) return;
		auto& M = Mod.GetModule();

		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int StrRemove_MiddleChars()"), TEXT("FString RemoveAt should remove middle characters"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int StrRemove_FirstChar()"), TEXT("FString RemoveAt should remove first character"), 1);
	}
};

#endif
