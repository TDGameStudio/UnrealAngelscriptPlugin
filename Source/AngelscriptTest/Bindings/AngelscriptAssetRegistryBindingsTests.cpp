// ============================================================================
// AngelscriptAssetRegistryBindingsTests.cpp
//
// AssetRegistry binding coverage — CQTest refactor. Automation IDs:
//   Angelscript.TestModule.Bindings.AssetRegistry.FAngelscriptAssetRegistryBindingsTest.*
//
// Sections:
//   TopLevelPathAndNullParent — FTopLevelAssetPath round-trip + null parent exception
//   QueryCompat              — deterministic AssetRegistry query vs native baselines
//
// CQTest adaptation notes:
//   Two IMPLEMENT_SIMPLE_AUTOMATION_TEST merged into one TEST_CLASS.
//   Both sections use token-replacement patterns with native baseline computation.
//   TopLevelPathAndNullParent uses WorldCollisionExecuteFunctionExpectingException for negative path.
// ============================================================================

#include "CQTest.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestModuleScope.h"
#include "AngelscriptTestExecute.h"
#include "Bindings/AngelscriptWorldCollisionBindingsTestHelpers.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/Blueprint.h"
#include "GameFramework/Actor.h"
#include "Misc/ScopeExit.h"
#include "Templates/Function.h"
#include "UObject/SoftObjectPath.h"

#if WITH_DEV_AUTOMATION_TESTS


// ----------------------------------------------------------------------------
// Helper utilities (retained from original)
// ----------------------------------------------------------------------------

namespace AngelscriptTest_Bindings_AngelscriptAssetRegistryBindingsTests_Private
{
	IAssetRegistry& GetAssetRegistryChecked()
	{
		FAssetRegistryModule& AssetRegistryModule = FModuleManager::Get().LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
		return AssetRegistryModule.Get();
	}

	bool ContainsAssetObjectPath(const TArray<FAssetData>& Assets, const FString& ObjectPath)
	{
		return Assets.ContainsByPredicate([&ObjectPath](const FAssetData& AssetData)
		{
			return AssetData.GetObjectPathString() == ObjectPath;
		});
	}

}


// ----------------------------------------------------------------------------
// Profile
// ----------------------------------------------------------------------------


// ----------------------------------------------------------------------------
// Test class
// ----------------------------------------------------------------------------

TEST_CLASS_WITH_FLAGS(FAngelscriptAssetRegistryBindingsTest,
	"Angelscript.TestModule.Bindings.AssetRegistry",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	BEFORE_ALL()
	{
		ASTEST_CREATE_ENGINE();
	}

	AFTER_ALL() { FAngelscriptEngine& Engine = ASTEST_GET_ENGINE(); ASTEST_RESET_ENGINE(Engine); }

	// ====================================================================
	// Section: TopLevelPathAndNullParent
	// ====================================================================

	TEST_METHOD(TopLevelPathAndNullParent)
	{
		using namespace AngelscriptTest_Bindings_AngelscriptAssetRegistryBindingsTests_Private;
		TestRunner->AddExpectedError(TEXT("A null Class was passed to GetBlueprintCDOsByParentClass."), EAutomationExpectedErrorFlags::Contains, 0);
		TestRunner->AddExpectedError(TEXT("ASAssetRegistry_TopLevelPathAndNullParent"), EAutomationExpectedErrorFlags::Contains, 0);
		TestRunner->AddExpectedError(TEXT("void TriggerNullParent(UObject[]&)"), EAutomationExpectedErrorFlags::Contains, 0, false);

		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		const FString Script = TEXT(R"(
int Entry()
{
	FTopLevelAssetPath PathFromClass(AActor::StaticClass());
	if (!PathFromClass.IsValid())
		return 10;
	if (PathFromClass.IsNull())
		return 20;

	FString PathString = PathFromClass.ToString();
	if (PathString.IsEmpty())
		return 30;

	FTopLevelAssetPath PathFromString(PathString);
	if (!PathFromString.IsValid())
		return 40;
	if (!(PathFromString == PathFromClass))
		return 50;

	FTopLevelAssetPath AssignedPath;
	AssignedPath = PathString;
	if (!(AssignedPath == PathFromClass))
		return 60;

	TArray<FAssetData> Assets;
	if (!AssetRegistry::GetAssetsByClass(FTopLevelAssetPath(UBlueprint::StaticClass()), Assets))
		return 70;

	for (int Index = 0; Index < Assets.Num(); ++Index)
	{
		if (Assets[Index].GetObjectPathString().IsEmpty())
			return 80;
		if (Assets[Index].GetSoftObjectPath().ToString().IsEmpty())
			return 90;
	}

	return Assets.Num() + 1;
}

void TriggerNullParent(TArray<UObject>& OutAssets)
{
	UClass NullClass;
	AssetRegistry::GetBlueprintCDOsByParentClass(NullClass, OutAssets);
}
)");

		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASAssetRegistry_TopLevelPathAndNullParent"), Script);
		if (!Mod.IsValid()) return;
		auto& M = Mod.GetModule();

		// Compute native baseline
		TArray<FAssetData> NativeAssets;
		const bool bNativeQuerySucceeded = GetAssetRegistryChecked().GetAssetsByClass(FTopLevelAssetPath(UBlueprint::StaticClass()), NativeAssets);
		TestRunner->TestTrue(TEXT("Native AssetRegistry GetAssetsByClass(UBlueprint) baseline should succeed"), bNativeQuerySucceeded);

		// Validate Entry result matches native count
		FASGlobalFunctionInvoker EntryInvoker(*TestRunner, Engine, M, TEXT("int Entry()"));
		if (EntryInvoker.IsValid())
		{
			const int32 ScriptResult = EntryInvoker.CallAndReturn<int32>(INDEX_NONE);
			TestRunner->TestEqual(
				TEXT("FTopLevelAssetPath round-trip and AssetRegistry::GetAssetsByClass should preserve native UBlueprint asset count"),
				ScriptResult,
				NativeAssets.Num() + 1);
		}

		// Negative path: null parent exception
		TArray<UObject*> NativeBlueprintCDOs;
		if (WorldCollisionExecuteFunctionExpectingException(
			*TestRunner,
			Engine,
			M,
			TEXT("void TriggerNullParent(TArray<UObject>& OutAssets)"),
			[this, &NativeBlueprintCDOs](asIScriptContext& Context)
			{
				return WorldCollisionSetArgAddressChecked(*TestRunner, Context, 0, &NativeBlueprintCDOs, TEXT("TriggerNullParent"));
			},
			TEXT("TriggerNullParent"),
			TEXT("A null Class was passed to GetBlueprintCDOsByParentClass.")))
		{
			TestRunner->TestEqual(
				TEXT("GetBlueprintCDOsByParentClass(null) should leave output array empty"),
				NativeBlueprintCDOs.Num(),
				0);
		}
	}

	// ====================================================================
	// Section: QueryCompat
	// ====================================================================

	TEST_METHOD(QueryCompat)
	{
		using namespace AngelscriptTest_Bindings_AngelscriptAssetRegistryBindingsTests_Private;
		static const FName EngineMaterialsPath(TEXT("/Engine/EngineMaterials"));
		static const FString TargetObjectPath(TEXT("/Engine/EngineMaterials/DefaultMaterial.DefaultMaterial"));

		IAssetRegistry& AssetRegistry = GetAssetRegistryChecked();
		const bool bNativeHasAssets = AssetRegistry.HasAssets(EngineMaterialsPath, false);

		TArray<FAssetData> NativeAssetsByPath;
		const bool bNativeGetAssetsByPath = AssetRegistry.GetAssetsByPath(EngineMaterialsPath, NativeAssetsByPath, false, false);

		const FAssetData NativeAssetByObjectPath = AssetRegistry.GetAssetByObjectPath(FSoftObjectPath(TargetObjectPath), false);
		const FString NativeObjectPathString = NativeAssetByObjectPath.GetObjectPathString();
		const FString NativeSoftObjectPathString = NativeAssetByObjectPath.GetSoftObjectPath().ToString();

		TArray<FAssetData> NativeAllAssets;
		const bool bNativeGetAllAssets = AssetRegistry.GetAllAssets(NativeAllAssets, false);
		const bool bNativeAllAssetsContainTarget = ContainsAssetObjectPath(NativeAllAssets, TargetObjectPath);

		const FTopLevelAssetPath NativeTopLevelPath(TargetObjectPath);
		const FString NativeTopLevelPathString = NativeTopLevelPath.ToString();

		if (!TestRunner->TestTrue(TEXT("Native HasAssets baseline"), bNativeHasAssets)
			|| !TestRunner->TestTrue(TEXT("Native GetAssetsByPath baseline"), bNativeGetAssetsByPath)
			|| !TestRunner->TestTrue(TEXT("Native GetAssetByObjectPath baseline"), NativeObjectPathString == TargetObjectPath)
			|| !TestRunner->TestTrue(TEXT("Native GetAllAssets baseline"), bNativeGetAllAssets)
			|| !TestRunner->TestTrue(TEXT("Native GetAllAssets contains target"), bNativeAllAssetsContainTarget)
			|| !TestRunner->TestTrue(TEXT("Native FTopLevelAssetPath valid"), NativeTopLevelPath.IsValid()))
		{
			return;
		}

		FString Script = TEXT(R"(
int VerifyTopLevelPathRoundTrip()
{
	const FString TargetObjectPath = "__TARGET_OBJECT_PATH__";
	const FString ExpectedTopLevelPath = "__EXPECTED_TOP_LEVEL_PATH__";

	Log(n"AssetRegistryBindings", "ASAssetRegistryQueryCompat.TopLevelPathRoundTrip: begin");

	FTopLevelAssetPath PathFromString(TargetObjectPath);
	if (!PathFromString.IsValid())
		return 10;
	if (PathFromString.IsNull())
		return 20;
	Log(n"AssetRegistryBindings", "ASAssetRegistryQueryCompat.TopLevelPathRoundTrip: value=" + PathFromString.ToString() + " expected=" + ExpectedTopLevelPath);
	if (PathFromString.ToString() != ExpectedTopLevelPath)
		return 30;
	return 0;
}

int VerifyHasAssets()
{
	const bool bExpectedHasAssets = __EXPECTED_HAS_ASSETS__;

	Log(n"AssetRegistryBindings", "ASAssetRegistryQueryCompat.HasAssets: begin");

	const bool bActualHasAssets = AssetRegistry::HasAssets(n"__ENGINE_MATERIALS_PATH__", false);
	Log(n"AssetRegistryBindings", "ASAssetRegistryQueryCompat.HasAssets: actual=" + bActualHasAssets + " expected=" + bExpectedHasAssets);
	return bActualHasAssets == bExpectedHasAssets ? 0 : 40;
}

int VerifyGetAssetsByPath()
{
	const bool bExpectedGetAssetsByPath = __EXPECTED_GET_ASSETS_BY_PATH__;
	const int ExpectedAssetsByPathCount = __EXPECTED_ASSETS_BY_PATH_COUNT__;
	const FString ExpectedObjectPathString = "__EXPECTED_OBJECT_PATH_STRING__";
	const FString ExpectedSoftObjectPathString = "__EXPECTED_SOFT_OBJECT_PATH_STRING__";

	Log(n"AssetRegistryBindings", "ASAssetRegistryQueryCompat.GetAssetsByPath: begin");

	TArray<FAssetData> AssetsByPath;
	const bool bActualGetAssetsByPath = AssetRegistry::GetAssetsByPath(n"__ENGINE_MATERIALS_PATH__", AssetsByPath, false, false);
	Log(n"AssetRegistryBindings", "ASAssetRegistryQueryCompat.GetAssetsByPath: actual=" + bActualGetAssetsByPath + " expected=" + bExpectedGetAssetsByPath + " count=" + AssetsByPath.Num());
	if (bActualGetAssetsByPath != bExpectedGetAssetsByPath)
		return 50;
	if (AssetsByPath.Num() != ExpectedAssetsByPathCount)
		return 60;

	bool bFoundTargetByPath = false;
	for (int Index = 0; Index < AssetsByPath.Num(); ++Index)
	{
		FString AssetObjectPathString = AssetsByPath[Index].GetObjectPathString();
		if (AssetObjectPathString == ExpectedObjectPathString)
		{
			bFoundTargetByPath = true;
			if (AssetsByPath[Index].GetSoftObjectPath().ToString() != ExpectedSoftObjectPathString)
				return 70;
		}
	}
	return bFoundTargetByPath ? 0 : 80;
}

int VerifyGetAssetByObjectPath()
{
	const FString TargetObjectPath = "__TARGET_OBJECT_PATH__";
	const FString ExpectedObjectPathString = "__EXPECTED_OBJECT_PATH_STRING__";
	const FString ExpectedSoftObjectPathString = "__EXPECTED_SOFT_OBJECT_PATH_STRING__";

	Log(n"AssetRegistryBindings", "ASAssetRegistryQueryCompat.GetAssetByObjectPath: begin");

	FAssetData AssetByObjectPath = AssetRegistry::GetAssetByObjectPath(FSoftObjectPath(TargetObjectPath), false);
	Log(n"AssetRegistryBindings", "ASAssetRegistryQueryCompat.GetAssetByObjectPath: objectPath=" + AssetByObjectPath.GetObjectPathString());
	if (AssetByObjectPath.GetObjectPathString() != ExpectedObjectPathString)
		return 90;
	if (AssetByObjectPath.GetSoftObjectPath().ToString() != ExpectedSoftObjectPathString)
		return 100;
	return 0;
}

int VerifyGetAllAssets()
{
	const bool bExpectedGetAllAssets = __EXPECTED_GET_ALL_ASSETS__;
	const bool bExpectedAllAssetsContainTarget = __EXPECTED_ALL_ASSETS_CONTAIN_TARGET__;
	const int ExpectedAllAssetsCount = __EXPECTED_ALL_ASSETS_COUNT__;
	const FString ExpectedObjectPathString = "__EXPECTED_OBJECT_PATH_STRING__";

	Log(n"AssetRegistryBindings", "ASAssetRegistryQueryCompat.GetAllAssets: begin");

	TArray<FAssetData> AllAssets;
	const bool bActualGetAllAssets = AssetRegistry::GetAllAssets(AllAssets, false);
	Log(n"AssetRegistryBindings", "ASAssetRegistryQueryCompat.GetAllAssets: actual=" + bActualGetAllAssets + " expected=" + bExpectedGetAllAssets + " count=" + AllAssets.Num());
	if (bActualGetAllAssets != bExpectedGetAllAssets)
		return 110;
	if (AllAssets.Num() != ExpectedAllAssetsCount)
		return 120;

	bool bFoundTargetInAllAssets = false;
	for (int Index = 0; Index < AllAssets.Num(); ++Index)
	{
		if (AllAssets[Index].GetObjectPathString() == ExpectedObjectPathString)
		{
			bFoundTargetInAllAssets = true;
			break;
		}
	}
	return bFoundTargetInAllAssets == bExpectedAllAssetsContainTarget ? 0 : 130;
}
)");

		Script.ReplaceInline(TEXT("__TARGET_OBJECT_PATH__"), *TargetObjectPath, ESearchCase::CaseSensitive);
		Script.ReplaceInline(TEXT("__EXPECTED_TOP_LEVEL_PATH__"), *NativeTopLevelPathString, ESearchCase::CaseSensitive);
		Script.ReplaceInline(TEXT("__EXPECTED_HAS_ASSETS__"), bNativeHasAssets ? TEXT("true") : TEXT("false"), ESearchCase::CaseSensitive);
		Script.ReplaceInline(TEXT("__EXPECTED_GET_ASSETS_BY_PATH__"), bNativeGetAssetsByPath ? TEXT("true") : TEXT("false"), ESearchCase::CaseSensitive);
		Script.ReplaceInline(TEXT("__EXPECTED_GET_ALL_ASSETS__"), bNativeGetAllAssets ? TEXT("true") : TEXT("false"), ESearchCase::CaseSensitive);
		Script.ReplaceInline(TEXT("__EXPECTED_ALL_ASSETS_CONTAIN_TARGET__"), bNativeAllAssetsContainTarget ? TEXT("true") : TEXT("false"), ESearchCase::CaseSensitive);
		Script.ReplaceInline(TEXT("__EXPECTED_ASSETS_BY_PATH_COUNT__"), *FString::FromInt(NativeAssetsByPath.Num()), ESearchCase::CaseSensitive);
		Script.ReplaceInline(TEXT("__EXPECTED_ALL_ASSETS_COUNT__"), *FString::FromInt(NativeAllAssets.Num()), ESearchCase::CaseSensitive);
		Script.ReplaceInline(TEXT("__EXPECTED_OBJECT_PATH_STRING__"), *NativeObjectPathString, ESearchCase::CaseSensitive);
		Script.ReplaceInline(TEXT("__EXPECTED_SOFT_OBJECT_PATH_STRING__"), *NativeSoftObjectPathString, ESearchCase::CaseSensitive);
		Script.ReplaceInline(TEXT("__ENGINE_MATERIALS_PATH__"), *EngineMaterialsPath.ToString(), ESearchCase::CaseSensitive);

		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASAssetRegistry_QueryCompat"), Script);
		if (!Mod.IsValid()) return;
		auto& M = Mod.GetModule();

		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int VerifyTopLevelPathRoundTrip()"), TEXT("FTopLevelAssetPath should round-trip script values"), 0);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int VerifyHasAssets()"), TEXT("AssetRegistry::HasAssets should match the native baseline"), 0);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int VerifyGetAssetsByPath()"), TEXT("AssetRegistry::GetAssetsByPath should match the native baseline"), 0);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int VerifyGetAssetByObjectPath()"), TEXT("AssetRegistry::GetAssetByObjectPath should match the native baseline"), 0);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int VerifyGetAllAssets()"), TEXT("AssetRegistry::GetAllAssets should match the native baseline"), 0);
	}
};

#endif
