// =============================================================================
// AngelscriptPlatformMiscBindingsTests.cpp
//
// CQTest coverage for FGenericPlatformMisc, CoreGlobals, SystemTimers,
// ConfigEnums bindings.
// Automation IDs: Angelscript.TestModule.Bindings.PlatformMisc.*
// =============================================================================

#include "CQTest.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestModuleScope.h"
#include "AngelscriptTestExecute.h"

#if WITH_DEV_AUTOMATION_TESTS



TEST_CLASS_WITH_FLAGS(FAngelscriptPlatformMiscBindingsTest,
	"Angelscript.TestModule.Bindings.PlatformMisc",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	BEFORE_ALL()
	{
		ASTEST_CREATE_ENGINE();
	}

	AFTER_ALL() { FAngelscriptEngine& Engine = ASTEST_GET_ENGINE(); ASTEST_RESET_ENGINE(Engine); }

	TEST_METHOD(CoreGlobals)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		const FString ExpectedFragments[] = {
			TEXT("GIsEditor"),
		};
		ExpectBindingCompileFailure(
			*TestRunner,
			Engine,
			TEXT("ASPlatformMisc_PlatMiscCoreGlobalsMissing"),
			TEXT(R"(
int IsEditor()
{
	return GIsEditor ? 1 : 0;
}
)"),
			TEXT("GIsEditor is not bound as a script-visible global on this branch"),
			MakeArrayView(ExpectedFragments));
	}

	TEST_METHOD(PlatformMisc)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		const FString ExpectedFragments[] = {
			TEXT("NumberOfCores"),
			TEXT("FGenericPlatformMisc"),
		};
		ExpectBindingCompileFailure(
			*TestRunner,
			Engine,
			TEXT("ASPlatformMisc_PlatMiscNumberOfCoresMissing"),
			TEXT(R"(
int GetNumCores()
{
	return FGenericPlatformMisc::NumberOfCores();
}
int GetNumCoresIncludingHyperthreads()
{
	return FGenericPlatformMisc::NumberOfCoresIncludingHyperthreads();
}
)"),
			TEXT("FGenericPlatformMisc currently exposes RequestExit only; core-count helpers remain an explicit unsupported binding boundary"),
			MakeArrayView(ExpectedFragments));
	}

	TEST_METHOD(SystemTimers)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		const FString ExpectedFragments[] = {
			TEXT("GetCurrentTime"),
			TEXT("GetDeltaTime"),
			TEXT("FApp"),
		};
		ExpectBindingCompileFailure(
			*TestRunner,
			Engine,
			TEXT("ASPlatformMisc_TimersMissing"),
			TEXT(R"(
int Seconds_Positive()
{
	double S = FApp::GetCurrentTime();
	return (S > 0.0) ? 1 : 0;
}
int DeltaTime_NonNegative()
{
	float DT = FApp::GetDeltaTime();
	return (DT >= 0.0) ? 1 : 0;
}
)"),
			TEXT("FApp time helpers are not bound on the current branch"),
			MakeArrayView(ExpectedFragments));
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
