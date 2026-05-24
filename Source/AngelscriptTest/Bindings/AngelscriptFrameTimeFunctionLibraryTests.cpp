// ============================================================================
// AngelscriptFrameTimeFunctionLibraryTests.cpp
//
// FQualifiedFrameTime binding coverage — CQTest pattern. Automation ID:
//   Angelscript.TestModule.FunctionLibraries.FrameTime.FAngelscriptFrameTimeBindingsTest.*
//
// Sections:
//   NativeBaselines        — verify 3 native FQualifiedFrameTime.AsSeconds()
//                            cases using TestTrue + FMath::IsNearlyEqual
//                            (no AS engine needed)
//   AsSecondsMixinCompiles — verify the AsSeconds() mixin binding compiles
//                            and is callable from script
// ============================================================================

#include "CQTest.h"
#include "Shared/AngelscriptTestMacros.h"
#include "Shared/AngelscriptTestModuleScope.h"
#include "Shared/AngelscriptTestExecute.h"

#include "Misc/QualifiedFrameTime.h"

#if WITH_DEV_AUTOMATION_TESTS


// ----------------------------------------------------------------------------
// Profile
// ----------------------------------------------------------------------------


// ----------------------------------------------------------------------------
// Constants
// ----------------------------------------------------------------------------

namespace AngelscriptTest_FrameTime_Private
{
	static constexpr double FrameTimeTolerance = 0.000000001;

	struct FFrameTimeAsSecondsCase
	{
		const TCHAR* Label = TEXT("");
		FQualifiedFrameTime Value;
		double ExpectedSeconds = 0.0;
	};

	TArray<FFrameTimeAsSecondsCase> BuildNativeCases()
	{
		return {
			{ TEXT("48 @ 24fps"), FQualifiedFrameTime(FFrameTime(48), FFrameRate(24, 1)), 2.0 },
			{ TEXT("90 @ 30fps"), FQualifiedFrameTime(FFrameTime(90), FFrameRate(30, 1)), 3.0 },
			{ TEXT("12 @ 25fps"), FQualifiedFrameTime(FFrameTime(FFrameNumber(12)), FFrameRate(25, 1)), 0.48 }
		};
	}
}

// ----------------------------------------------------------------------------
// Test class
// ----------------------------------------------------------------------------

TEST_CLASS_WITH_FLAGS(FAngelscriptFrameTimeBindingsTest,
	"Angelscript.TestModule.FunctionLibraries.FrameTime",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	BEFORE_ALL()
	{
		ASTEST_CREATE_ENGINE();
	}

	AFTER_ALL() { FAngelscriptEngine& Engine = ASTEST_GET_ENGINE(); ASTEST_RESET_ENGINE(Engine); }

	// ====================================================================
	// Section: NativeBaselines
	// ====================================================================

	TEST_METHOD(NativeBaselines)
	{
		using namespace AngelscriptTest_FrameTime_Private;

		const TArray<FFrameTimeAsSecondsCase> Cases = BuildNativeCases();
		for (const FFrameTimeAsSecondsCase& C : Cases)
		{
			TestRunner->TestTrue(
				FString::Printf(TEXT("Native %s baseline should match expected seconds conversion"), C.Label),
				FMath::IsNearlyEqual(C.Value.AsSeconds(), C.ExpectedSeconds, FrameTimeTolerance));
		}
	}

	// ====================================================================
	// Section: AsSecondsMixinCompiles
	// ====================================================================

	TEST_METHOD(AsSecondsMixinCompiles)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASFrameTime_AsSecondsMixin"), TEXT(R"(
int AsSeconds_Compiles()
{
	FQualifiedFrameTime DefaultTime;
	// Just call AsSeconds to verify the mixin binding compiles and links.
	DefaultTime.AsSeconds();
	return 1;
}
)"));
		if (!Mod.IsValid()) return;
		auto& M = Mod.GetModule();

		ExpectGlobalInt(*TestRunner, Engine, M, 
			TEXT("int AsSeconds_Compiles()"),
			TEXT("FQualifiedFrameTime.AsSeconds mixin binding should compile and be callable"),
			1);
	}
};

#endif
