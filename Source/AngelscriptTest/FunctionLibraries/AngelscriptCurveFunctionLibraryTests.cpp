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
//   Two original legacy automation classes merged into one
//   TEST_CLASS with two TEST_METHODs. The address-based invocation helpers
//   and $TOKEN$ replacement (for UCurveFloat path) are preserved.
// ============================================================================

#include "CQTest.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestUtilities.h"
#include "AngelscriptTestModuleScope.h"
#include "AngelscriptTestExecute.h"

#include "FunctionLibraries/RuntimeFloatCurveMixinLibrary.h"

#include "Curves/CurveFloat.h"
#include "Curves/CurveLinearColor.h"
#include "Curves/RichCurve.h"
#include "Misc/ScopeExit.h"

#if WITH_ANGELSCRIPT_UNITTESTS


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
		FNoDiscardAsserter LocalAssert(Test);
		if (!LocalAssert.IsTrue(
			Curve.Keys.IsValidIndex(KeyIndex),
			FString::Printf(TEXT("%s channel should contain key index %d"), ChannelLabel, KeyIndex)))
		{
			return false;
		}

		const FRichCurveKey& Key = Curve.Keys[KeyIndex];
		const bool bTimeMatches = LocalAssert.IsNear(
			ExpectedTime,
			Key.Time,
			UE_KINDA_SMALL_NUMBER,
			FString::Printf(TEXT("%s channel key %d should preserve its timestamp"), ChannelLabel, KeyIndex));
		const bool bValueMatches = LocalAssert.IsNear(
			ExpectedValue,
			Key.Value,
			UE_KINDA_SMALL_NUMBER,
			FString::Printf(TEXT("%s channel key %d should preserve its channel value"), ChannelLabel, KeyIndex));
		return bTimeMatches && bValueMatches;
	}
}

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

	AFTER_ALL()
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		ASTEST_RESET_ENGINE(Engine);
	}

	// ====================================================================
	// Section: RuntimeCurveLinearColorAddDefaultKey
	// ====================================================================

	TEST_METHOD(RuntimeCurveLinearColorAddDefaultKey)
	{
		using namespace CurveTestHelpers;

		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASCurve_LinearColorAddKey"), ASTEST_AS(R"AS(
			int PopulateCurve(FRuntimeCurveLinearColor& Curve)
			{
				Curve.AddDefaultKey(0.0f, FLinearColor(1.0f, 0.0f, 0.0f, 0.25f));
				Curve.AddDefaultKey(2.5f, FLinearColor(0.125f, 0.5f, 0.75f, 1.0f));
				return 1;
			}
			)AS"));
		if (!Mod.IsValid()) return;
		auto& M = Mod.GetModule();

		FRuntimeCurveLinearColor Curve;
		for (int32 ChannelIndex = 0; ChannelIndex < UE_ARRAY_COUNT(Curve.ColorCurves); ++ChannelIndex)
		{
			ASSERT_THAT(AreEqual(
				0,
				Curve.ColorCurves[ChannelIndex].Keys.Num(),
				FString::Printf(TEXT("Curve channel %d should start empty"), ChannelIndex)));
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

		ASSERT_THAT(AreEqual(
			1,
			Result,
			TEXT("RuntimeCurveLinearColor AddDefaultKey helper should execute successfully")));

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
			ASSERT_THAT(AreEqual(
				2,
				RichCurve.Keys.Num(),
				FString::Printf(TEXT("%s channel should contain two keys after two AddDefaultKey calls"), ChannelLabels[ChannelIndex])));
			ExpectCurveKey(*TestRunner, RichCurve, 0, ExpectedTimes[0], ExpectedValues[ChannelIndex][0], ChannelLabels[ChannelIndex]);
			ExpectCurveKey(*TestRunner, RichCurve, 1, ExpectedTimes[1], ExpectedValues[ChannelIndex][1], ChannelLabels[ChannelIndex]);
		}
	}

	// ====================================================================
	// Section: RuntimeFloatCurveInstanceSurface
	// ====================================================================

	TEST_METHOD(RuntimeFloatCurveInstanceSurface)
	{
		using namespace CurveTestHelpers;

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

		FString RuntimeCurveSource = ASTEST_AS(R"AS(
			int PopulateCurve(FRuntimeFloatCurve& RuntimeCurve)
			{
				RuntimeCurve.AddDefaultKey(0.5f, 1.25f);
				RuntimeCurve.AddDefaultKey(3.0f, 9.5f);
				if (RuntimeCurve.GetNumKeys() != 2)
				{
					return 10;
				}

				float32 MinTime = -1.0f;
				float32 MaxTime = -1.0f;
				RuntimeCurve.GetTimeRange(MinTime, MaxTime);
				if (MinTime != 0.5f || MaxTime != 3.0f)
				{
					return 20;
				}

				UObject CurveObject = FindObject("__CURVE_PATH__");
				UCurveFloat CurveAsset = Cast<UCurveFloat>(CurveObject);
				if (CurveAsset == null)
				{
					return 30;
				}

				FCurveKeyHandle Handle = CurveAsset.AddAutoCurveKey(1.5f, 7.5f);
				if (CurveAsset.GetFloatValue(1.5f) != 7.5f)
				{
					return 40;
				}
				CurveAsset.SetKeyInterpMode(Handle, ERichCurveInterpMode::RCIM_Constant, false);
				return 1;
			}
			)AS");
		RuntimeCurveSource.ReplaceInline(TEXT("__CURVE_PATH__"), *CurveAsset->GetPathName().ReplaceCharWithEscapedChar());

		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASCurve_FloatCurveInstance"), RuntimeCurveSource);
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

		ASSERT_THAT(AreEqual(
			1,
			Result,
			TEXT("RuntimeFloatCurve and UCurveFloat instance helper surface should compile and execute through instance syntax")));

		const FRichCurve& RuntimeRichCurve = RuntimeCurve.EditorCurveData;
		ASSERT_THAT(AreEqual(
			2,
			RuntimeRichCurve.Keys.Num(),
			TEXT("FRuntimeFloatCurve.AddDefaultKey instance syntax should add two keys to the runtime curve")));
		ExpectCurveKey(*TestRunner, RuntimeRichCurve, 0, 0.5f, 1.25f, TEXT("RuntimeFloatCurve"));
		ExpectCurveKey(*TestRunner, RuntimeRichCurve, 1, 3.0f, 9.5f, TEXT("RuntimeFloatCurve"));

		float MinTime = 0.0f;
		float MaxTime = 0.0f;
		RuntimeCurve.GetRichCurveConst()->GetTimeRange(MinTime, MaxTime);
		ASSERT_THAT(IsNear(
			0.5f,
			MinTime,
			UE_KINDA_SMALL_NUMBER,
			TEXT("FRuntimeFloatCurve.GetTimeRange instance syntax should preserve the native minimum time")));
		ASSERT_THAT(IsNear(
			3.0f,
			MaxTime,
			UE_KINDA_SMALL_NUMBER,
			TEXT("FRuntimeFloatCurve.GetTimeRange instance syntax should preserve the native maximum time")));

		if (Result == 1)
		{
			const FRichCurve& AssetCurve = CurveAsset->FloatCurve;
			const int32 AssetKeyCount = AssetCurve.Keys.Num();
			ASSERT_THAT(AreEqual(
				1,
				AssetKeyCount,
				TEXT("UCurveFloat.AddAutoCurveKey instance syntax should add one key to the asset curve")));
			if (AssetKeyCount > 0)
			{
				ExpectCurveKey(*TestRunner, AssetCurve, 0, 1.5f, 7.5f, TEXT("UCurveFloat"));
				ASSERT_THAT(AreEqual(
					ERichCurveInterpMode::RCIM_Constant,
					AssetCurve.Keys[0].InterpMode,
					TEXT("UCurveFloat.SetKeyInterpMode instance syntax should update the native key interpolation mode")));
			}
		}
	}

	TEST_METHOD(RuntimeFloatCurveTargetsExternalAsset)
	{
		UCurveFloat* CurveAsset = NewObject<UCurveFloat>(GetTransientPackage(), NAME_None, RF_Transient);
		ASSERT_THAT(IsNotNull(CurveAsset));

		int32 UpdateCount = 0;
#if WITH_EDITOR
		const FDelegateHandle UpdateHandle = CurveAsset->OnUpdateCurve.AddLambda(
			[&UpdateCount](UCurveBase*, EPropertyChangeType::Type)
			{
				++UpdateCount;
			});
		ON_SCOPE_EXIT
		{
			CurveAsset->OnUpdateCurve.Remove(UpdateHandle);
		};
#endif

		FRuntimeFloatCurve RuntimeCurve;
		RuntimeCurve.ExternalCurve = CurveAsset;
		URuntimeFloatCurveMixinLibrary::AddDefaultKey(RuntimeCurve, 2.0f, 8.0f);

		ASSERT_THAT(AreEqual(0, RuntimeCurve.EditorCurveData.GetNumKeys(),
			TEXT("External runtime curves should not mutate their embedded fallback")));
		ASSERT_THAT(AreEqual(1, CurveAsset->FloatCurve.GetNumKeys(),
			TEXT("External runtime curves should mutate the external asset")));
#if WITH_EDITOR
		ASSERT_THAT(AreEqual(1, UpdateCount,
			TEXT("External runtime curve mutation should notify its asset once")));
#endif
	}

	TEST_METHOD(AutoAndSmartAutoUseDistinctTangentModes)
	{
		UCurveFloat* CurveAsset = NewObject<UCurveFloat>(GetTransientPackage(), NAME_None, RF_Transient);
		ASSERT_THAT(IsNotNull(CurveAsset));

		const FCurveKeyHandle AutoHandle = URuntimeFloatCurveMixinLibrary::AddAutoCurveKey(CurveAsset, 1.0f, 2.0f);
		const FCurveKeyHandle SmartAutoHandle = URuntimeFloatCurveMixinLibrary::AddSmartAutoCurveKey(CurveAsset, 2.0f, 3.0f);

		ASSERT_THAT(AreEqual(
			static_cast<int32>(ERichCurveTangentMode::RCTM_Auto),
			static_cast<int32>(CurveAsset->FloatCurve.GetKey(AutoHandle.KeyHandle).TangentMode)));
		ASSERT_THAT(AreEqual(
			static_cast<int32>(ERichCurveTangentMode::RCTM_SmartAuto),
			static_cast<int32>(CurveAsset->FloatCurve.GetKey(SmartAutoHandle.KeyHandle).TangentMode)));
	}

	TEST_METHOD(NullMutatorsAreNoOps)
	{
		FCurveKeyHandle InvalidHandle;
		InvalidHandle.KeyHandle = FKeyHandle::Invalid();
		ASSERT_THAT(IsFalse(URuntimeFloatCurveMixinLibrary::AddCurveKey(nullptr, 0.0f, 0.0f).KeyHandle.IsValid()));
		ASSERT_THAT(IsFalse(URuntimeFloatCurveMixinLibrary::AddConstantCurveKey(nullptr, 0.0f, 0.0f).KeyHandle.IsValid()));
		ASSERT_THAT(IsFalse(URuntimeFloatCurveMixinLibrary::AddLinearCurveKey(nullptr, 0.0f, 0.0f).KeyHandle.IsValid()));
		ASSERT_THAT(IsFalse(URuntimeFloatCurveMixinLibrary::AddAutoCurveKey(nullptr, 0.0f, 0.0f).KeyHandle.IsValid()));
		ASSERT_THAT(IsFalse(URuntimeFloatCurveMixinLibrary::AddSmartAutoCurveKey(nullptr, 0.0f, 0.0f).KeyHandle.IsValid()));
		ASSERT_THAT(IsFalse(URuntimeFloatCurveMixinLibrary::AddCurveKeyTangent(nullptr, 0.0f, 0.0f, 0.0f).KeyHandle.IsValid()));
		ASSERT_THAT(IsFalse(URuntimeFloatCurveMixinLibrary::AddCurveKeyBrokenTangent(nullptr, 0.0f, 0.0f, 0.0f, 0.0f).KeyHandle.IsValid()));
		ASSERT_THAT(IsFalse(URuntimeFloatCurveMixinLibrary::AddCurveKeyWeightedArriveTangent(nullptr, 0.0f, 0.0f, false, 0.0f, 0.0f, 0.0f, 0.0f).KeyHandle.IsValid()));
		ASSERT_THAT(IsFalse(URuntimeFloatCurveMixinLibrary::AddCurveKeyWeightedLeaveTangent(nullptr, 0.0f, 0.0f, false, 0.0f, 0.0f, 0.0f, 0.0f).KeyHandle.IsValid()));
		ASSERT_THAT(IsFalse(URuntimeFloatCurveMixinLibrary::AddCurveKeyWeightedBothTangent(nullptr, 0.0f, 0.0f, false, 0.0f, 0.0f, 0.0f, 0.0f).KeyHandle.IsValid()));

		URuntimeFloatCurveMixinLibrary::AutoSetTangents(nullptr);
		URuntimeFloatCurveMixinLibrary::SetDefaultValue(nullptr, 0.0f);
		URuntimeFloatCurveMixinLibrary::SetPreInfinityExtrap(nullptr, ERichCurveExtrapolation::RCCE_Constant);
		URuntimeFloatCurveMixinLibrary::SetPostInfinityExtrap(nullptr, ERichCurveExtrapolation::RCCE_Constant);
		URuntimeFloatCurveMixinLibrary::SetKeyInterpMode(nullptr, InvalidHandle, ERichCurveInterpMode::RCIM_Linear, false);
		URuntimeFloatCurveMixinLibrary::SetKeyTangentMode(nullptr, InvalidHandle, ERichCurveTangentMode::RCTM_User, false);
		URuntimeFloatCurveMixinLibrary::SetKeyTangentWeightMode(nullptr, InvalidHandle, ERichCurveTangentWeightMode::RCTWM_WeightedBoth, false);
		URuntimeFloatCurveMixinLibrary::SetKeyUserTangents(nullptr, InvalidHandle, 1.0f, 2.0f);
		URuntimeFloatCurveMixinLibrary::SetKeyUserTangentWeights(nullptr, InvalidHandle, 1.0f, 2.0f);
		ASSERT_THAT(IsTrue(true, TEXT("Every null UCurveFloat mutator should return without dereferencing")));
	}

	TEST_METHOD(InvalidHandleDoesNotNotifyOrDirty)
	{
		UCurveFloat* CurveAsset = NewObject<UCurveFloat>(GetTransientPackage(), NAME_None, RF_Transient);
		ASSERT_THAT(IsNotNull(CurveAsset));
		FCurveKeyHandle InvalidHandle;
		InvalidHandle.KeyHandle = FKeyHandle::Invalid();
		int32 UpdateCount = 0;
#if WITH_EDITOR
		const FDelegateHandle UpdateHandle = CurveAsset->OnUpdateCurve.AddLambda(
			[&UpdateCount](UCurveBase*, EPropertyChangeType::Type)
			{
				++UpdateCount;
			});
		ON_SCOPE_EXIT
		{
			CurveAsset->OnUpdateCurve.Remove(UpdateHandle);
		};
#endif

		URuntimeFloatCurveMixinLibrary::SetKeyInterpMode(CurveAsset, InvalidHandle, ERichCurveInterpMode::RCIM_Linear, false);
		URuntimeFloatCurveMixinLibrary::SetKeyTangentMode(CurveAsset, InvalidHandle, ERichCurveTangentMode::RCTM_User, false);
		URuntimeFloatCurveMixinLibrary::SetKeyTangentWeightMode(CurveAsset, InvalidHandle, ERichCurveTangentWeightMode::RCTWM_WeightedBoth, false);
		URuntimeFloatCurveMixinLibrary::SetKeyUserTangents(CurveAsset, InvalidHandle, 1.0f, 2.0f);
		URuntimeFloatCurveMixinLibrary::SetKeyUserTangentWeights(CurveAsset, InvalidHandle, 1.0f, 2.0f);

		ASSERT_THAT(AreEqual(0, CurveAsset->FloatCurve.GetNumKeys()));
#if WITH_EDITOR
		ASSERT_THAT(AreEqual(0, UpdateCount,
			TEXT("Invalid key handles should not emit curve update notifications")));
#endif
	}

	TEST_METHOD(AssetMutationNotifiesAndMarksDirty)
	{
		const FString PackageName = FString::Printf(
			TEXT("/Temp/AngelscriptCurveMutation_%s"),
			*FGuid::NewGuid().ToString(EGuidFormats::Digits));
		UPackage* Package = CreatePackage(*PackageName);
		ASSERT_THAT(IsNotNull(Package));
		UCurveFloat* CurveAsset = NewObject<UCurveFloat>(Package);
		ASSERT_THAT(IsNotNull(CurveAsset));
		Package->SetDirtyFlag(false);
		ON_SCOPE_EXIT
		{
			CurveAsset->MarkAsGarbage();
			Package->SetDirtyFlag(false);
			Package->MarkAsGarbage();
		};

		int32 UpdateCount = 0;
#if WITH_EDITOR
		const FDelegateHandle UpdateHandle = CurveAsset->OnUpdateCurve.AddLambda(
			[&UpdateCount](UCurveBase*, EPropertyChangeType::Type)
			{
				++UpdateCount;
			});
		ON_SCOPE_EXIT
		{
			CurveAsset->OnUpdateCurve.Remove(UpdateHandle);
		};
#endif

		URuntimeFloatCurveMixinLibrary::AddCurveKey(CurveAsset, 4.0f, 16.0f);

		ASSERT_THAT(AreEqual(1, CurveAsset->FloatCurve.GetNumKeys()));
#if WITH_EDITOR
		ASSERT_THAT(AreEqual(1, UpdateCount,
			TEXT("Successful asset mutation should emit one update notification")));
		ASSERT_THAT(IsTrue(Package->IsDirty(),
			TEXT("Successful asset mutation should mark the owning package dirty")));
#endif
	}
};

#endif
