#include "CQTest.h"
#include "Core/AngelscriptEditorModule.h"

#include "AngelscriptEngine.h"
#include "AngelscriptRuntimeModule.h"
#include "AngelscriptRuntimeModule.h"

#include "Curves/CurveFloat.h"
#include "Curves/RichCurve.h"
#include "Misc/AutomationTest.h"
#include "Misc/Guid.h"
#include "Misc/ScopeExit.h"
#include "UObject/GarbageCollection.h"
#include "UObject/UObjectGlobals.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace AngelscriptEditor_Private_Tests_AngelscriptEditorModuleLiteralAssetTests_Private
{
	UCurveFloat* CreateLiteralCurveTestAsset(FAutomationTestBase& Test, UPackage* AssetsPackage, const TCHAR* BaseName)
	{
		if (!Test.TestNotNull(TEXT("LiteralAssetSaved test should have a valid assets package"), AssetsPackage))
		{
			return nullptr;
		}

		const FString UniqueSuffix = FGuid::NewGuid().ToString(EGuidFormats::Digits);
		const FName DesiredName(*FString::Printf(TEXT("%s_%s"), BaseName, *UniqueSuffix));
		const FName UniqueName = MakeUniqueObjectName(AssetsPackage, UCurveFloat::StaticClass(), DesiredName);
		return NewObject<UCurveFloat>(AssetsPackage, UniqueName, RF_Public | RF_Standalone | RF_Transient);
	}

	void CleanupLiteralCurveTestAsset(UObject*& Asset)
	{
		if (Asset == nullptr)
		{
			return;
		}

		Asset->ClearFlags(RF_Public | RF_Standalone);
		Asset->Rename(nullptr, GetTransientPackage(), REN_DontCreateRedirectors | REN_ForceNoResetLoaders | REN_NonTransactional);
		Asset->MarkAsGarbage();
		Asset = nullptr;
	}

	bool ContainsExactLine(const TArray<FString>& Lines, const FString& ExpectedLine)
	{
		return Lines.ContainsByPredicate([&ExpectedLine](const FString& Line)
		{
			return Line == ExpectedLine;
		});
	}

	bool ContainsLineFragment(const TArray<FString>& Lines, const FString& Fragment)
	{
		return Lines.ContainsByPredicate([&Fragment](const FString& Line)
		{
			return Line.Contains(Fragment);
		});
	}

	struct FLiteralAssetSaveCapture
	{
		FString AssetName;
		TArray<FString> Lines;
	};
}


#define TestTrue(...) Test.TestTrue(__VA_ARGS__)
#define TestFalse(...) Test.TestFalse(__VA_ARGS__)
#define TestEqual(...) Test.TestEqual(__VA_ARGS__)
#define TestNotNull(...) Test.TestNotNull(__VA_ARGS__)

static bool RunOnLiteralAssetSavedSerializesCurveKeysDefaultValueAndInfinityModes(FAutomationTestBase& Test)
{
	using namespace AngelscriptEditor_Private_Tests_AngelscriptEditorModuleLiteralAssetTests_Private;
	FAngelscriptRuntimeModule::InitializeAngelscript();
	FAngelscriptRuntimeModule::InitializeAngelscript();
	if (!TestTrue(TEXT("Editor.Module.OnLiteralAssetSavedSerializesCurveKeysDefaultValueAndInfinityModes requires the Angelscript engine to be initialized"), FAngelscriptEngine::IsInitialized()))
	{
		return false;
	}

	FAngelscriptEngine& Engine = FAngelscriptEngine::Get();
	UPackage* AssetsPackage = Engine.AssetsPackage;
	UObject* LiteralAssetObject = CreateLiteralCurveTestAsset(Test, AssetsPackage, TEXT("LiteralCurve"));
	UCurveFloat* LiteralCurve = Cast<UCurveFloat>(LiteralAssetObject);
	if (!TestNotNull(TEXT("Editor.Module.OnLiteralAssetSavedSerializesCurveKeysDefaultValueAndInfinityModes should create a literal curve asset"), LiteralCurve))
	{
		CleanupLiteralCurveTestAsset(LiteralAssetObject);
		return false;
	}

	ON_SCOPE_EXIT
	{
		FAngelscriptEditorModuleTestAccess::ResetLiteralAssetSaveTestHooks();
		CleanupLiteralCurveTestAsset(LiteralAssetObject);
		CollectGarbage(RF_NoFlags, true);
	};

	const float ExpectedTime = 1.25f;
	const float ExpectedValue = 2.5f;
	const float ExpectedDefaultValue = 3.75f;

	LiteralCurve->FloatCurve.Keys.Reset();
	FRichCurveKey& LinearKey = LiteralCurve->FloatCurve.Keys.AddDefaulted_GetRef();
	LinearKey.Time = ExpectedTime;
	LinearKey.Value = ExpectedValue;
	LinearKey.InterpMode = ERichCurveInterpMode::RCIM_Linear;
	LinearKey.TangentMode = ERichCurveTangentMode::RCTM_Auto;
	LinearKey.TangentWeightMode = ERichCurveTangentWeightMode::RCTWM_WeightedNone;
	LiteralCurve->FloatCurve.DefaultValue = ExpectedDefaultValue;
	LiteralCurve->FloatCurve.PreInfinityExtrap = ERichCurveExtrapolation::RCCE_Cycle;
	LiteralCurve->FloatCurve.PostInfinityExtrap = ERichCurveExtrapolation::RCCE_Linear;

	int32 ReplaceCallCount = 0;
	int32 MessageDialogCallCount = 0;
	FString CapturedAssetName;
	TArray<FString> CapturedLines;

	FAngelscriptEditorModuleLiteralAssetSaveTestHooks TestHooks;
	TestHooks.HasAnyDebugServerClients = []()
	{
		return true;
	};
	TestHooks.OpenMessageDialog = [&MessageDialogCallCount](const FText&)
	{
		++MessageDialogCallCount;
	};
	TestHooks.ReplaceScriptAssetContent = [&ReplaceCallCount, &CapturedAssetName, &CapturedLines](const FString& AssetName, const TArray<FString>& NewContent)
	{
		++ReplaceCallCount;
		CapturedAssetName = AssetName;
		CapturedLines = NewContent;
	};
	FAngelscriptEditorModuleTestAccess::SetLiteralAssetSaveTestHooks(MoveTemp(TestHooks));
	FAngelscriptEditorModuleTestAccess::InvokeOnLiteralAssetSaved(LiteralCurve);

	const FString ExpectedLinearKeyLine = FString::Printf(
		TEXT("AddLinearCurveKey(%s, %s);"),
		*FString::SanitizeFloat(ExpectedTime),
		*FString::SanitizeFloat(ExpectedValue));
	const FString ExpectedDefaultValueLine = FString::Printf(
		TEXT("DefaultValue = %s;"),
		*FString::SanitizeFloat(ExpectedDefaultValue));
	const FString ExpectedPreInfinityLine = TEXT("PreInfinityExtrap = ERichCurveExtrapolation::RCCE_Cycle;");
	const FString ExpectedPostInfinityLine = TEXT("PostInfinityExtrap = ERichCurveExtrapolation::RCCE_Linear;");

	if (!TestEqual(
			TEXT("Editor.Module.OnLiteralAssetSavedSerializesCurveKeysDefaultValueAndInfinityModes should replace the script asset text exactly once"),
			ReplaceCallCount,
			1)
		|| !TestEqual(
			TEXT("Editor.Module.OnLiteralAssetSavedSerializesCurveKeysDefaultValueAndInfinityModes should capture the literal curve asset name"),
			CapturedAssetName,
			LiteralCurve->GetName())
		|| !TestEqual(
			TEXT("Editor.Module.OnLiteralAssetSavedSerializesCurveKeysDefaultValueAndInfinityModes should not open an error dialog when a debug client is present"),
			MessageDialogCallCount,
			0)
		|| !TestTrue(
			TEXT("Editor.Module.OnLiteralAssetSavedSerializesCurveKeysDefaultValueAndInfinityModes should serialize a linear curve key"),
			ContainsExactLine(CapturedLines, ExpectedLinearKeyLine))
		|| !TestTrue(
			TEXT("Editor.Module.OnLiteralAssetSavedSerializesCurveKeysDefaultValueAndInfinityModes should serialize the default value"),
			ContainsExactLine(CapturedLines, ExpectedDefaultValueLine))
		|| !TestTrue(
			TEXT("Editor.Module.OnLiteralAssetSavedSerializesCurveKeysDefaultValueAndInfinityModes should serialize the pre-infinity mode"),
			ContainsExactLine(CapturedLines, ExpectedPreInfinityLine))
		|| !TestTrue(
			TEXT("Editor.Module.OnLiteralAssetSavedSerializesCurveKeysDefaultValueAndInfinityModes should serialize the post-infinity mode"),
			ContainsExactLine(CapturedLines, ExpectedPostInfinityLine)))
	{
		return false;
	}

	return true;
}

static bool RunOnLiteralAssetSavedFlatCurvesSkipAsciiGraphButPreserveSerializedKeys(FAutomationTestBase& Test)
{
	using namespace AngelscriptEditor_Private_Tests_AngelscriptEditorModuleLiteralAssetTests_Private;
	FAngelscriptRuntimeModule::InitializeAngelscript();
	FAngelscriptRuntimeModule::InitializeAngelscript();
	if (!TestTrue(TEXT("Editor.Module.OnLiteralAssetSavedFlatCurvesSkipAsciiGraphButPreserveSerializedKeys requires the Angelscript engine to be initialized"), FAngelscriptEngine::IsInitialized()))
	{
		return false;
	}

	FAngelscriptEngine& Engine = FAngelscriptEngine::Get();
	UPackage* AssetsPackage = Engine.AssetsPackage;
	UObject* SinglePointAssetObject = CreateLiteralCurveTestAsset(Test, AssetsPackage, TEXT("LiteralSinglePointCurve"));
	UObject* FlatCurveAssetObject = CreateLiteralCurveTestAsset(Test, AssetsPackage, TEXT("LiteralFlatCurve"));
	UCurveFloat* SinglePointCurve = Cast<UCurveFloat>(SinglePointAssetObject);
	UCurveFloat* FlatCurve = Cast<UCurveFloat>(FlatCurveAssetObject);
	if (!TestNotNull(TEXT("Editor.Module.OnLiteralAssetSavedFlatCurvesSkipAsciiGraphButPreserveSerializedKeys should create the single-point literal curve"), SinglePointCurve)
		|| !TestNotNull(TEXT("Editor.Module.OnLiteralAssetSavedFlatCurvesSkipAsciiGraphButPreserveSerializedKeys should create the flat literal curve"), FlatCurve))
	{
		CleanupLiteralCurveTestAsset(SinglePointAssetObject);
		CleanupLiteralCurveTestAsset(FlatCurveAssetObject);
		return false;
	}

	ON_SCOPE_EXIT
	{
		FAngelscriptEditorModuleTestAccess::ResetLiteralAssetSaveTestHooks();
		CleanupLiteralCurveTestAsset(SinglePointAssetObject);
		CleanupLiteralCurveTestAsset(FlatCurveAssetObject);
		CollectGarbage(RF_NoFlags, true);
	};

	const float SinglePointTime = 2.0f;
	const float SinglePointValue = 7.5f;
	const float SinglePointDefaultValue = 4.25f;
	SinglePointCurve->FloatCurve.Keys.Reset();
	FRichCurveKey& SinglePointKey = SinglePointCurve->FloatCurve.Keys.AddDefaulted_GetRef();
	SinglePointKey.Time = SinglePointTime;
	SinglePointKey.Value = SinglePointValue;
	SinglePointKey.InterpMode = ERichCurveInterpMode::RCIM_Constant;
	SinglePointKey.TangentMode = ERichCurveTangentMode::RCTM_Auto;
	SinglePointKey.TangentWeightMode = ERichCurveTangentWeightMode::RCTWM_WeightedNone;
	SinglePointCurve->FloatCurve.DefaultValue = SinglePointDefaultValue;
	SinglePointCurve->FloatCurve.PreInfinityExtrap = ERichCurveExtrapolation::RCCE_Cycle;
	SinglePointCurve->FloatCurve.PostInfinityExtrap = ERichCurveExtrapolation::RCCE_None;

	const float FlatCurveFirstTime = 1.0f;
	const float FlatCurveSecondTime = 5.0f;
	const float FlatCurveValue = 3.5f;
	const float FlatCurveDefaultValue = 1.5f;
	FlatCurve->FloatCurve.Keys.Reset();
	FRichCurveKey& FlatCurveFirstKey = FlatCurve->FloatCurve.Keys.AddDefaulted_GetRef();
	FlatCurveFirstKey.Time = FlatCurveFirstTime;
	FlatCurveFirstKey.Value = FlatCurveValue;
	FlatCurveFirstKey.InterpMode = ERichCurveInterpMode::RCIM_Linear;
	FlatCurveFirstKey.TangentMode = ERichCurveTangentMode::RCTM_Auto;
	FlatCurveFirstKey.TangentWeightMode = ERichCurveTangentWeightMode::RCTWM_WeightedNone;
	FRichCurveKey& FlatCurveSecondKey = FlatCurve->FloatCurve.Keys.AddDefaulted_GetRef();
	FlatCurveSecondKey.Time = FlatCurveSecondTime;
	FlatCurveSecondKey.Value = FlatCurveValue;
	FlatCurveSecondKey.InterpMode = ERichCurveInterpMode::RCIM_Linear;
	FlatCurveSecondKey.TangentMode = ERichCurveTangentMode::RCTM_Auto;
	FlatCurveSecondKey.TangentWeightMode = ERichCurveTangentWeightMode::RCTWM_WeightedNone;
	FlatCurve->FloatCurve.DefaultValue = FlatCurveDefaultValue;
	FlatCurve->FloatCurve.PreInfinityExtrap = ERichCurveExtrapolation::RCCE_Linear;
	FlatCurve->FloatCurve.PostInfinityExtrap = ERichCurveExtrapolation::RCCE_CycleWithOffset;

	int32 MessageDialogCallCount = 0;
	TArray<FLiteralAssetSaveCapture> CapturedWrites;

	FAngelscriptEditorModuleLiteralAssetSaveTestHooks TestHooks;
	TestHooks.HasAnyDebugServerClients = []()
	{
		return true;
	};
	TestHooks.OpenMessageDialog = [&MessageDialogCallCount](const FText&)
	{
		++MessageDialogCallCount;
	};
	TestHooks.ReplaceScriptAssetContent = [&CapturedWrites](const FString& AssetName, const TArray<FString>& NewContent)
	{
		CapturedWrites.Add({ AssetName, NewContent });
	};
	FAngelscriptEditorModuleTestAccess::SetLiteralAssetSaveTestHooks(MoveTemp(TestHooks));
	FAngelscriptEditorModuleTestAccess::InvokeOnLiteralAssetSaved(SinglePointCurve);
	FAngelscriptEditorModuleTestAccess::InvokeOnLiteralAssetSaved(FlatCurve);

	const FLiteralAssetSaveCapture* SinglePointCapture = CapturedWrites.FindByPredicate([SinglePointCurve](const FLiteralAssetSaveCapture& Capture)
	{
		return Capture.AssetName == SinglePointCurve->GetName();
	});
	const FLiteralAssetSaveCapture* FlatCurveCapture = CapturedWrites.FindByPredicate([FlatCurve](const FLiteralAssetSaveCapture& Capture)
	{
		return Capture.AssetName == FlatCurve->GetName();
	});

	if (!TestEqual(
			TEXT("Editor.Module.OnLiteralAssetSavedFlatCurvesSkipAsciiGraphButPreserveSerializedKeys should rewrite both literal curves"),
			CapturedWrites.Num(),
			2)
		|| !TestEqual(
			TEXT("Editor.Module.OnLiteralAssetSavedFlatCurvesSkipAsciiGraphButPreserveSerializedKeys should not open an error dialog when a debug client is present"),
			MessageDialogCallCount,
			0)
		|| !TestNotNull(
			TEXT("Editor.Module.OnLiteralAssetSavedFlatCurvesSkipAsciiGraphButPreserveSerializedKeys should capture the single-point curve content"),
			SinglePointCapture)
		|| !TestNotNull(
			TEXT("Editor.Module.OnLiteralAssetSavedFlatCurvesSkipAsciiGraphButPreserveSerializedKeys should capture the flat curve content"),
			FlatCurveCapture))
	{
		return false;
	}

	const FString ExpectedSinglePointKeyLine = FString::Printf(
		TEXT("AddConstantCurveKey(%s, %s);"),
		*FString::SanitizeFloat(SinglePointTime),
		*FString::SanitizeFloat(SinglePointValue));
	const FString ExpectedSinglePointDefaultValueLine = FString::Printf(
		TEXT("DefaultValue = %s;"),
		*FString::SanitizeFloat(SinglePointDefaultValue));
	const FString ExpectedFlatCurveFirstKeyLine = FString::Printf(
		TEXT("AddLinearCurveKey(%s, %s);"),
		*FString::SanitizeFloat(FlatCurveFirstTime),
		*FString::SanitizeFloat(FlatCurveValue));
	const FString ExpectedFlatCurveSecondKeyLine = FString::Printf(
		TEXT("AddLinearCurveKey(%s, %s);"),
		*FString::SanitizeFloat(FlatCurveSecondTime),
		*FString::SanitizeFloat(FlatCurveValue));
	const FString ExpectedFlatCurveDefaultValueLine = FString::Printf(
		TEXT("DefaultValue = %s;"),
		*FString::SanitizeFloat(FlatCurveDefaultValue));

	if (!TestTrue(
			TEXT("Editor.Module.OnLiteralAssetSavedFlatCurvesSkipAsciiGraphButPreserveSerializedKeys should serialize the single-point constant key"),
			ContainsExactLine(SinglePointCapture->Lines, ExpectedSinglePointKeyLine))
		|| !TestTrue(
			TEXT("Editor.Module.OnLiteralAssetSavedFlatCurvesSkipAsciiGraphButPreserveSerializedKeys should serialize the single-point default value"),
			ContainsExactLine(SinglePointCapture->Lines, ExpectedSinglePointDefaultValueLine))
		|| !TestTrue(
			TEXT("Editor.Module.OnLiteralAssetSavedFlatCurvesSkipAsciiGraphButPreserveSerializedKeys should serialize the single-point pre-infinity mode"),
			ContainsExactLine(SinglePointCapture->Lines, TEXT("PreInfinityExtrap = ERichCurveExtrapolation::RCCE_Cycle;")))
		|| !TestTrue(
			TEXT("Editor.Module.OnLiteralAssetSavedFlatCurvesSkipAsciiGraphButPreserveSerializedKeys should serialize the single-point post-infinity mode"),
			ContainsExactLine(SinglePointCapture->Lines, TEXT("PostInfinityExtrap = ERichCurveExtrapolation::RCCE_None;")))
		|| !TestFalse(
			TEXT("Editor.Module.OnLiteralAssetSavedFlatCurvesSkipAsciiGraphButPreserveSerializedKeys should skip ASCII graph markers for the single-point curve"),
			ContainsLineFragment(SinglePointCapture->Lines, TEXT("/*"))
			|| ContainsLineFragment(SinglePointCapture->Lines, TEXT("----"))
			|| ContainsLineFragment(SinglePointCapture->Lines, TEXT("|")))
		|| !TestTrue(
			TEXT("Editor.Module.OnLiteralAssetSavedFlatCurvesSkipAsciiGraphButPreserveSerializedKeys should serialize the flat curve first key"),
			ContainsExactLine(FlatCurveCapture->Lines, ExpectedFlatCurveFirstKeyLine))
		|| !TestTrue(
			TEXT("Editor.Module.OnLiteralAssetSavedFlatCurvesSkipAsciiGraphButPreserveSerializedKeys should serialize the flat curve second key"),
			ContainsExactLine(FlatCurveCapture->Lines, ExpectedFlatCurveSecondKeyLine))
		|| !TestTrue(
			TEXT("Editor.Module.OnLiteralAssetSavedFlatCurvesSkipAsciiGraphButPreserveSerializedKeys should serialize the flat curve default value"),
			ContainsExactLine(FlatCurveCapture->Lines, ExpectedFlatCurveDefaultValueLine))
		|| !TestTrue(
			TEXT("Editor.Module.OnLiteralAssetSavedFlatCurvesSkipAsciiGraphButPreserveSerializedKeys should serialize the flat curve pre-infinity mode"),
			ContainsExactLine(FlatCurveCapture->Lines, TEXT("PreInfinityExtrap = ERichCurveExtrapolation::RCCE_Linear;")))
		|| !TestTrue(
			TEXT("Editor.Module.OnLiteralAssetSavedFlatCurvesSkipAsciiGraphButPreserveSerializedKeys should serialize the flat curve post-infinity mode"),
			ContainsExactLine(FlatCurveCapture->Lines, TEXT("PostInfinityExtrap = ERichCurveExtrapolation::RCCE_CycleWithOffset;")))
		|| !TestFalse(
			TEXT("Editor.Module.OnLiteralAssetSavedFlatCurvesSkipAsciiGraphButPreserveSerializedKeys should skip ASCII graph markers for the flat curve"),
			ContainsLineFragment(FlatCurveCapture->Lines, TEXT("/*"))
			|| ContainsLineFragment(FlatCurveCapture->Lines, TEXT("----"))
			|| ContainsLineFragment(FlatCurveCapture->Lines, TEXT("|"))))
	{
		return false;
	}

	return true;
}

static bool RunOnLiteralAssetSavedWeightedTangentsEmitExpectedFunctionAndArgumentOrder(FAutomationTestBase& Test)
{
	using namespace AngelscriptEditor_Private_Tests_AngelscriptEditorModuleLiteralAssetTests_Private;
	FAngelscriptRuntimeModule::InitializeAngelscript();
	if (!TestTrue(TEXT("Editor.Module.OnLiteralAssetSavedWeightedTangentsEmitExpectedFunctionAndArgumentOrder requires the Angelscript engine to be initialized"), FAngelscriptEngine::IsInitialized()))
	{
		return false;
	}

	FAngelscriptEngine& Engine = FAngelscriptEngine::Get();
	UPackage* AssetsPackage = Engine.AssetsPackage;
	UObject* WeightedArriveAssetObject = CreateLiteralCurveTestAsset(Test, AssetsPackage, TEXT("LiteralWeightedArriveCurve"));
	UObject* WeightedLeaveAssetObject = CreateLiteralCurveTestAsset(Test, AssetsPackage, TEXT("LiteralWeightedLeaveCurve"));
	UObject* WeightedBothAssetObject = CreateLiteralCurveTestAsset(Test, AssetsPackage, TEXT("LiteralWeightedBothCurve"));
	UCurveFloat* WeightedArriveCurve = Cast<UCurveFloat>(WeightedArriveAssetObject);
	UCurveFloat* WeightedLeaveCurve = Cast<UCurveFloat>(WeightedLeaveAssetObject);
	UCurveFloat* WeightedBothCurve = Cast<UCurveFloat>(WeightedBothAssetObject);
	if (!TestNotNull(TEXT("Editor.Module.OnLiteralAssetSavedWeightedTangentsEmitExpectedFunctionAndArgumentOrder should create the weighted-arrive literal curve"), WeightedArriveCurve)
		|| !TestNotNull(TEXT("Editor.Module.OnLiteralAssetSavedWeightedTangentsEmitExpectedFunctionAndArgumentOrder should create the weighted-leave literal curve"), WeightedLeaveCurve)
		|| !TestNotNull(TEXT("Editor.Module.OnLiteralAssetSavedWeightedTangentsEmitExpectedFunctionAndArgumentOrder should create the weighted-both literal curve"), WeightedBothCurve))
	{
		CleanupLiteralCurveTestAsset(WeightedArriveAssetObject);
		CleanupLiteralCurveTestAsset(WeightedLeaveAssetObject);
		CleanupLiteralCurveTestAsset(WeightedBothAssetObject);
		return false;
	}

	ON_SCOPE_EXIT
	{
		FAngelscriptEditorModuleTestAccess::ResetLiteralAssetSaveTestHooks();
		CleanupLiteralCurveTestAsset(WeightedArriveAssetObject);
		CleanupLiteralCurveTestAsset(WeightedLeaveAssetObject);
		CleanupLiteralCurveTestAsset(WeightedBothAssetObject);
		CollectGarbage(RF_NoFlags, true);
	};

	auto ConfigureWeightedKey = [](UCurveFloat& Curve,
		const float Time,
		const float Value,
		const float ArriveTangent,
		const float LeaveTangent,
		const float ArriveWeight,
		const float LeaveWeight,
		const ERichCurveTangentMode TangentMode,
		const ERichCurveTangentWeightMode TangentWeightMode)
	{
		Curve.FloatCurve.Keys.Reset();
		FRichCurveKey& Key = Curve.FloatCurve.Keys.AddDefaulted_GetRef();
		Key.Time = Time;
		Key.Value = Value;
		Key.InterpMode = ERichCurveInterpMode::RCIM_Cubic;
		Key.TangentMode = TangentMode;
		Key.TangentWeightMode = TangentWeightMode;
		Key.ArriveTangent = ArriveTangent;
		Key.LeaveTangent = LeaveTangent;
		Key.ArriveTangentWeight = ArriveWeight;
		Key.LeaveTangentWeight = LeaveWeight;
	};

	ConfigureWeightedKey(*WeightedArriveCurve, 1.25f, 2.5f, 3.5f, 4.5f, 5.5f, 6.5f, ERichCurveTangentMode::RCTM_User, ERichCurveTangentWeightMode::RCTWM_WeightedArrive);
	ConfigureWeightedKey(*WeightedLeaveCurve, 7.25f, 8.5f, 9.5f, 10.5f, 11.5f, 12.5f, ERichCurveTangentMode::RCTM_User, ERichCurveTangentWeightMode::RCTWM_WeightedLeave);
	ConfigureWeightedKey(*WeightedBothCurve, 13.25f, 14.5f, 15.5f, 16.5f, 17.5f, 18.5f, ERichCurveTangentMode::RCTM_Break, ERichCurveTangentWeightMode::RCTWM_WeightedBoth);

	int32 MessageDialogCallCount = 0;
	TArray<FLiteralAssetSaveCapture> CapturedWrites;

	FAngelscriptEditorModuleLiteralAssetSaveTestHooks TestHooks;
	TestHooks.HasAnyDebugServerClients = []()
	{
		return true;
	};
	TestHooks.OpenMessageDialog = [&MessageDialogCallCount](const FText&)
	{
		++MessageDialogCallCount;
	};
	TestHooks.ReplaceScriptAssetContent = [&CapturedWrites](const FString& AssetName, const TArray<FString>& NewContent)
	{
		CapturedWrites.Add({ AssetName, NewContent });
	};
	FAngelscriptEditorModuleTestAccess::SetLiteralAssetSaveTestHooks(MoveTemp(TestHooks));
	FAngelscriptEditorModuleTestAccess::InvokeOnLiteralAssetSaved(WeightedArriveCurve);
	FAngelscriptEditorModuleTestAccess::InvokeOnLiteralAssetSaved(WeightedLeaveCurve);
	FAngelscriptEditorModuleTestAccess::InvokeOnLiteralAssetSaved(WeightedBothCurve);

	const FLiteralAssetSaveCapture* WeightedArriveCapture = CapturedWrites.FindByPredicate([WeightedArriveCurve](const FLiteralAssetSaveCapture& Capture)
	{
		return Capture.AssetName == WeightedArriveCurve->GetName();
	});
	const FLiteralAssetSaveCapture* WeightedLeaveCapture = CapturedWrites.FindByPredicate([WeightedLeaveCurve](const FLiteralAssetSaveCapture& Capture)
	{
		return Capture.AssetName == WeightedLeaveCurve->GetName();
	});
	const FLiteralAssetSaveCapture* WeightedBothCapture = CapturedWrites.FindByPredicate([WeightedBothCurve](const FLiteralAssetSaveCapture& Capture)
	{
		return Capture.AssetName == WeightedBothCurve->GetName();
	});

	if (!TestEqual(
			TEXT("Editor.Module.OnLiteralAssetSavedWeightedTangentsEmitExpectedFunctionAndArgumentOrder should rewrite all weighted literal curves"),
			CapturedWrites.Num(),
			3)
		|| !TestEqual(
			TEXT("Editor.Module.OnLiteralAssetSavedWeightedTangentsEmitExpectedFunctionAndArgumentOrder should not open an error dialog when a debug client is present"),
			MessageDialogCallCount,
			0)
		|| !TestNotNull(
			TEXT("Editor.Module.OnLiteralAssetSavedWeightedTangentsEmitExpectedFunctionAndArgumentOrder should capture the weighted-arrive curve content"),
			WeightedArriveCapture)
		|| !TestNotNull(
			TEXT("Editor.Module.OnLiteralAssetSavedWeightedTangentsEmitExpectedFunctionAndArgumentOrder should capture the weighted-leave curve content"),
			WeightedLeaveCapture)
		|| !TestNotNull(
			TEXT("Editor.Module.OnLiteralAssetSavedWeightedTangentsEmitExpectedFunctionAndArgumentOrder should capture the weighted-both curve content"),
			WeightedBothCapture))
	{
		return false;
	}

	auto CountLinesContainingFragment = [](const TArray<FString>& Lines, const FString& Fragment)
	{
		int32 Count = 0;
		for (const FString& Line : Lines)
		{
			if (Line.Contains(Fragment))
			{
				++Count;
			}
		}
		return Count;
	};

	const FString ExpectedWeightedArriveLine = FString::Printf(
		TEXT("AddCurveKeyWeightedArriveTangent(%s, %s, false, %s, %s, %s, %s);"),
		*FString::SanitizeFloat(1.25f),
		*FString::SanitizeFloat(2.5f),
		*FString::SanitizeFloat(3.5f),
		*FString::SanitizeFloat(4.5f),
		*FString::SanitizeFloat(5.5f),
		*FString::SanitizeFloat(6.5f));
	const FString ExpectedWeightedLeaveLine = FString::Printf(
		TEXT("AddCurveKeyWeightedLeaveTangent(%s, %s, false, %s, %s, %s, %s);"),
		*FString::SanitizeFloat(7.25f),
		*FString::SanitizeFloat(8.5f),
		*FString::SanitizeFloat(9.5f),
		*FString::SanitizeFloat(10.5f),
		*FString::SanitizeFloat(11.5f),
		*FString::SanitizeFloat(12.5f));
	const FString ExpectedWeightedBothLine = FString::Printf(
		TEXT("AddCurveKeyWeightedBothTangent(%s, %s, true, %s, %s, %s, %s);"),
		*FString::SanitizeFloat(13.25f),
		*FString::SanitizeFloat(14.5f),
		*FString::SanitizeFloat(15.5f),
		*FString::SanitizeFloat(16.5f),
		*FString::SanitizeFloat(17.5f),
		*FString::SanitizeFloat(18.5f));
	const bool bWeightedArriveLineMatched = ContainsExactLine(WeightedArriveCapture->Lines, ExpectedWeightedArriveLine);
	const bool bWeightedLeaveLineMatched = ContainsExactLine(WeightedLeaveCapture->Lines, ExpectedWeightedLeaveLine);
	const bool bWeightedBothLineMatched = ContainsExactLine(WeightedBothCapture->Lines, ExpectedWeightedBothLine);

	if (!TestTrue(
			TEXT("Editor.Module.OnLiteralAssetSavedWeightedTangentsEmitExpectedFunctionAndArgumentOrder should serialize the weighted-arrive function and argument order"),
			bWeightedArriveLineMatched)
		|| !TestEqual(
			TEXT("Editor.Module.OnLiteralAssetSavedWeightedTangentsEmitExpectedFunctionAndArgumentOrder should emit exactly one weighted-arrive call"),
			CountLinesContainingFragment(WeightedArriveCapture->Lines, TEXT("AddCurveKeyWeightedArriveTangent(")),
			1)
		|| !TestTrue(
			TEXT("Editor.Module.OnLiteralAssetSavedWeightedTangentsEmitExpectedFunctionAndArgumentOrder should serialize the weighted-leave function and argument order"),
			bWeightedLeaveLineMatched)
		|| !TestEqual(
			TEXT("Editor.Module.OnLiteralAssetSavedWeightedTangentsEmitExpectedFunctionAndArgumentOrder should emit exactly one weighted-leave call"),
			CountLinesContainingFragment(WeightedLeaveCapture->Lines, TEXT("AddCurveKeyWeightedLeaveTangent(")),
			1)
		|| !TestTrue(
			TEXT("Editor.Module.OnLiteralAssetSavedWeightedTangentsEmitExpectedFunctionAndArgumentOrder should serialize the weighted-both function and broken argument order"),
			bWeightedBothLineMatched)
		|| !TestEqual(
			TEXT("Editor.Module.OnLiteralAssetSavedWeightedTangentsEmitExpectedFunctionAndArgumentOrder should emit exactly one weighted-both call"),
			CountLinesContainingFragment(WeightedBothCapture->Lines, TEXT("AddCurveKeyWeightedBothTangent(")),
			1))
	{
		return false;
	}

	return true;
}

#undef TestTrue
#undef TestFalse
#undef TestEqual
#undef TestNotNull

TEST_CLASS_WITH_FLAGS(
	FAngelscriptEditorModuleLiteralAssetTests,
	"Angelscript.Editor.Module",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(OnLiteralAssetSavedSerializesCurveKeysDefaultValueAndInfinityModes)
	{
		ASSERT_THAT(IsTrue(RunOnLiteralAssetSavedSerializesCurveKeysDefaultValueAndInfinityModes(*TestRunner)));
	}

	TEST_METHOD(OnLiteralAssetSavedFlatCurvesSkipAsciiGraphButPreserveSerializedKeys)
	{
		ASSERT_THAT(IsTrue(RunOnLiteralAssetSavedFlatCurvesSkipAsciiGraphButPreserveSerializedKeys(*TestRunner)));
	}

	TEST_METHOD(OnLiteralAssetSavedWeightedTangentsEmitExpectedFunctionAndArgumentOrder)
	{
		ASSERT_THAT(IsTrue(RunOnLiteralAssetSavedWeightedTangentsEmitExpectedFunctionAndArgumentOrder(*TestRunner)));
	}
};

#endif
