// ============================================================================
// AngelscriptCurveFunctionLibraryTests.cpp
//
// Curve function library binding coverage — CQTest refactor.
// Automation ID:
//   Angelscript.TestModule.FunctionLibraries.Curve.*
//
// Sections:
//   RuntimeCurveLinearColorAddDefaultKey — FRuntimeCurveLinearColor channel population
//   RuntimeFloatCurveInstanceSurface     — FRuntimeFloatCurve + UCurveFloat instance API
//
// CQTest adaptation notes:
//   Two original IMPLEMENT_SIMPLE_AUTOMATION_TEST classes merged into one
//   TEST_CLASS with two TEST_METHODs. The address-based invocation helpers
//   and $TOKEN$ replacement (for UCurveFloat path) are preserved.
// ============================================================================

#include "CQTest.h"
#include "Shared/AngelscriptTestMacros.h"
#include "Shared/AngelscriptTestUtilities.h"
#include "Shared/AngelscriptTestModuleScope.h"
#include "Shared/AngelscriptTestExecute.h"

#include "Curves/CurveFloat.h"
#include "Curves/CurveLinearColor.h"
#include "Curves/RichCurve.h"
#include "Misc/ScopeExit.h"

#if WITH_DEV_AUTOMATION_TESTS


// ----------------------------------------------------------------------------
// Profile
// ----------------------------------------------------------------------------


// ----------------------------------------------------------------------------
// Shared helpers
// ----------------------------------------------------------------------------

namespace CurveTestHelpers
{
	bool ExpectCurveKey(
		FAutomationTestBase& Test,
		const FRichCurve& Curve,
		int32 KeyIndex,
		float ExpectedTime,
		float ExpectedValue,
		const TCHAR* ChannelLabel)
	{
		if (!Test.TestTrue(
			*FString::Printf(TEXT("%s channel should contain key index %d"), ChannelLabel, KeyIndex),
			Curve.Keys.IsValidIndex(KeyIndex)))
		{
			return false;
		}

		const FRichCurveKey& Key = Curve.Keys[KeyIndex];
		const bool bTimeMatches = Test.TestTrue(
			*FString::Printf(TEXT("%s channel key %d should preserve its timestamp"), ChannelLabel, KeyIndex),
			FMath::IsNearlyEqual(Key.Time, ExpectedTime));
		const bool bValueMatches = Test.TestTrue(
			*FString::Printf(TEXT("%s channel key %d should preserve its channel value"), ChannelLabel, KeyIndex),
			FMath::IsNearlyEqual(Key.Value, ExpectedValue));
		return bTimeMatches && bValueMatches;
	}
}

using namespace CurveTestHelpers;

// ----------------------------------------------------------------------------
// Test class
// ----------------------------------------------------------------------------

TEST_CLASS_WITH_FLAGS(FAngelscriptCurveFunctionLibraryBindingsTest,
	"Angelscript.TestModule.FunctionLibraries.Curve",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	BEFORE_ALL()
	{
		ASTEST_CREATE_ENGINE();
	}

	AFTER_ALL() { FAngelscriptEngine& Engine = ASTEST_GET_ENGINE(); ASTEST_RESET_ENGINE(Engine); }

	// ====================================================================
	// Section: RuntimeCurveLinearColorAddDefaultKey
	// ====================================================================

	TEST_METHOD(RuntimeCurveLinearColorAddDefaultKey)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASCurve_LinearColorAddKey"), TEXT(R"(
int PopulateCurve(FRuntimeCurveLinearColor& Curve)
{
	Curve.AddDefaultKey(0.0f, FLinearColor(1.0f, 0.0f, 0.0f, 0.25f));
	Curve.AddDefaultKey(2.5f, FLinearColor(0.125f, 0.5f, 0.75f, 1.0f));
	return 1;
}
)"));
		if (!Mod.IsValid()) return;
		auto& M = Mod.GetModule();

		FRuntimeCurveLinearColor Curve;
		for (int32 ChannelIndex = 0; ChannelIndex < UE_ARRAY_COUNT(Curve.ColorCurves); ++ChannelIndex)
		{
			TestRunner->TestEqual(
				*FString::Printf(TEXT("Curve channel %d should start empty"), ChannelIndex),
				Curve.ColorCurves[ChannelIndex].Keys.Num(), 0);
		}

		FAngelscriptTestExecutor PopulateCurveExecutor(
			*TestRunner,
			Engine,
			M,
			TEXT("int PopulateCurve(FRuntimeCurveLinearColor&)"));
		PopulateCurveExecutor.AddArgAddress(&Curve);
		const int32 Result = PopulateCurveExecutor.ExecuteAndGet<int32>(INDEX_NONE);
		if (!PopulateCurveExecutor.HasRun())
		{
			return;
		}

		TestRunner->TestEqual(
			TEXT("RuntimeCurveLinearColor AddDefaultKey helper should execute successfully"),
			Result, 1);

		static const TCHAR* ChannelLabels[] = { TEXT("R"), TEXT("G"), TEXT("B"), TEXT("A") };
		static const float ExpectedValues[4][2] =
		{
			{ 1.0f, 0.125f },
			{ 0.0f, 0.5f },
			{ 0.0f, 0.75f },
			{ 0.25f, 1.0f },
		};
		static const float ExpectedTimes[2] = { 0.0f, 2.5f };

		for (int32 ChannelIndex = 0; ChannelIndex < UE_ARRAY_COUNT(Curve.ColorCurves); ++ChannelIndex)
		{
			const FRichCurve& RichCurve = Curve.ColorCurves[ChannelIndex];
			TestRunner->TestEqual(
				*FString::Printf(TEXT("%s channel should contain two keys after two AddDefaultKey calls"), ChannelLabels[ChannelIndex]),
				RichCurve.Keys.Num(), 2);
			ExpectCurveKey(*TestRunner, RichCurve, 0, ExpectedTimes[0], ExpectedValues[ChannelIndex][0], ChannelLabels[ChannelIndex]);
			ExpectCurveKey(*TestRunner, RichCurve, 1, ExpectedTimes[1], ExpectedValues[ChannelIndex][1], ChannelLabels[ChannelIndex]);
		}
	}

	// ====================================================================
	// Section: RuntimeFloatCurveInstanceSurface
	// ====================================================================

	TEST_METHOD(RuntimeFloatCurveInstanceSurface)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		UCurveFloat* CurveAsset = NewObject<UCurveFloat>(
			GetTransientPackage(),
			MakeUniqueObjectName(GetTransientPackage(), UCurveFloat::StaticClass(), TEXT("RuntimeFloatCurveInstanceSurface")),
			RF_Transient);
		ASSERT_THAT(IsNotNull(CurveAsset));
		ON_SCOPE_EXIT
		{
			if (CurveAsset != nullptr)
			{
				CurveAsset->MarkAsGarbage();
			}
		};

		FString Script = TEXT(R"AS(
int PopulateCurve(FRuntimeFloatCurve& RuntimeCurve)
{
	RuntimeCurve.AddDefaultKey(0.5f, 1.25f);
	RuntimeCurve.AddDefaultKey(3.0f, 9.5f);
	if (RuntimeCurve.GetNumKeys() != 2)
		return 10;

	float32 MinTime = -1.0f;
	float32 MaxTime = -1.0f;
	RuntimeCurve.GetTimeRange(MinTime, MaxTime);
	if (MinTime != 0.5f || MaxTime != 3.0f)
		return 20;

	UObject CurveObject = FindObject("__CURVE_PATH__");
	UCurveFloat CurveAsset = Cast<UCurveFloat>(CurveObject);
	if (CurveAsset == null)
		return 30;

	FCurveKeyHandle Handle = CurveAsset.AddAutoCurveKey(1.5f, 7.5f);
	if (CurveAsset.GetFloatValue(1.5f) != 7.5f)
		return 40;
	CurveAsset.SetKeyInterpMode(Handle, ERichCurveInterpMode::RCIM_Constant, false);
	return 1;
}
)AS");
		Script.ReplaceInline(TEXT("__CURVE_PATH__"), *CurveAsset->GetPathName().ReplaceCharWithEscapedChar());

		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASCurve_FloatCurveInstance"), Script);
		if (!Mod.IsValid()) return;
		auto& M = Mod.GetModule();

		FRuntimeFloatCurve RuntimeCurve;
		FAngelscriptTestExecutor PopulateCurveExecutor(
			*TestRunner,
			Engine,
			M,
			TEXT("int PopulateCurve(FRuntimeFloatCurve&)"));
		PopulateCurveExecutor.AddArgAddress(&RuntimeCurve);
		const int32 Result = PopulateCurveExecutor.ExecuteAndGet<int32>(INDEX_NONE);
		if (!PopulateCurveExecutor.HasRun())
		{
			return;
		}

		TestRunner->TestEqual(
			TEXT("RuntimeFloatCurve and UCurveFloat instance helper surface should compile and execute through instance syntax"),
			Result, 1);

		const FRichCurve& RuntimeRichCurve = RuntimeCurve.EditorCurveData;
		TestRunner->TestEqual(
			TEXT("FRuntimeFloatCurve.AddDefaultKey instance syntax should add two keys to the runtime curve"),
			RuntimeRichCurve.Keys.Num(), 2);
		ExpectCurveKey(*TestRunner, RuntimeRichCurve, 0, 0.5f, 1.25f, TEXT("RuntimeFloatCurve"));
		ExpectCurveKey(*TestRunner, RuntimeRichCurve, 1, 3.0f, 9.5f, TEXT("RuntimeFloatCurve"));

		float MinTime = 0.0f;
		float MaxTime = 0.0f;
		RuntimeCurve.GetRichCurveConst()->GetTimeRange(MinTime, MaxTime);
		TestRunner->TestTrue(
			TEXT("FRuntimeFloatCurve.GetTimeRange instance syntax should preserve the native time range"),
			FMath::IsNearlyEqual(MinTime, 0.5f) && FMath::IsNearlyEqual(MaxTime, 3.0f));

		if (Result == 1)
		{
			const FRichCurve& AssetCurve = CurveAsset->FloatCurve;
			const int32 AssetKeyCount = AssetCurve.Keys.Num();
			TestRunner->TestEqual(
				TEXT("UCurveFloat.AddAutoCurveKey instance syntax should add one key to the asset curve"),
				AssetKeyCount, 1);
			if (AssetKeyCount > 0)
			{
				ExpectCurveKey(*TestRunner, AssetCurve, 0, 1.5f, 7.5f, TEXT("UCurveFloat"));
				TestRunner->TestEqual(
					TEXT("UCurveFloat.SetKeyInterpMode instance syntax should update the native key interpolation mode"),
					AssetCurve.Keys[0].InterpMode,
					ERichCurveInterpMode::RCIM_Constant);
			}
		}
	}
};

#endif
