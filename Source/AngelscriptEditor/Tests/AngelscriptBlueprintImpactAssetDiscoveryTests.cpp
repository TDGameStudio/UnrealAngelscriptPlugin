#include "BlueprintImpact/AngelscriptBlueprintImpactScanner.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "CQTest.h"
#include "GameFramework/Actor.h"
#include "HAL/FileManager.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/AutomationTest.h"
#include "Misc/Guid.h"
#include "Misc/PackageName.h"
#include "Misc/ScopeExit.h"
#include "UObject/GarbageCollection.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace AngelscriptEditor_Private_Tests_AngelscriptBlueprintImpactAssetDiscoveryTests_Private
{
	bool IsBlueprintCompiled(const UBlueprint& Blueprint)
	{
		return Blueprint.GeneratedClass != nullptr && Blueprint.SkeletonGeneratedClass != nullptr;
	}

	bool RegisterBlueprintAsset(
		FAutomationTestBase& Test,
		FAssetRegistryModule& AssetRegistryModule,
		UBlueprint& Blueprint,
		const TCHAR* Context)
	{
		if (!Test.TestTrue(
				Context,
				IsBlueprintCompiled(Blueprint)))
		{
			return false;
		}

		AssetRegistryModule.AssetCreated(&Blueprint);
		return true;
	}

	UBlueprint* CreateUnsavedBlueprintChild(
		FAutomationTestBase& Test,
		FAssetRegistryModule& AssetRegistryModule,
		UClass* ParentClass,
		FStringView Suffix,
		FString& OutPackagePath)
	{
		OutPackagePath = FString::Printf(
			TEXT("/Game/Automation/BP_EditorImpactDiscovery_Unsaved_%.*s_%s"),
			Suffix.Len(),
			Suffix.GetData(),
			*FGuid::NewGuid().ToString(EGuidFormats::Digits));

		UPackage* BlueprintPackage = CreatePackage(*OutPackagePath);
		if (!Test.TestNotNull(TEXT("BlueprintImpact asset discovery test should create a transient package"), BlueprintPackage))
		{
			return nullptr;
		}

		const FName BlueprintName(*FPackageName::GetLongPackageAssetName(OutPackagePath));
		UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprint(
			ParentClass,
			BlueprintPackage,
			BlueprintName,
			BPTYPE_Normal,
			UBlueprint::StaticClass(),
			UBlueprintGeneratedClass::StaticClass(),
			TEXT("AngelscriptBlueprintImpactAssetDiscoveryTests"));
		if (Blueprint == nullptr)
		{
			return nullptr;
		}

		FKismetEditorUtilities::CompileBlueprint(Blueprint);
		if (!RegisterBlueprintAsset(
				Test,
				AssetRegistryModule,
				*Blueprint,
				TEXT("BlueprintImpact asset discovery test should compile and register the unsaved in-memory blueprint")))
		{
			return nullptr;
		}

		Blueprint->AddToRoot();
		return Blueprint;
	}

	bool SaveBlueprintToDisk(
		FAutomationTestBase& Test,
		UBlueprint& Blueprint,
		const FString& PackagePath,
		FString& OutPackageFilename)
	{
		UPackage* Package = Blueprint.GetOutermost();
		if (!Test.TestNotNull(TEXT("BlueprintImpact asset discovery test should resolve the blueprint package before save"), Package))
		{
			return false;
		}

		Package->SetDirtyFlag(true);
		OutPackageFilename = FPackageName::LongPackageNameToFilename(PackagePath, FPackageName::GetAssetPackageExtension());

		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
		SaveArgs.SaveFlags = SAVE_NoError;
		return Test.TestTrue(
			TEXT("BlueprintImpact asset discovery test should save the disk-backed blueprint package"),
			UPackage::SavePackage(Package, &Blueprint, *OutPackageFilename, SaveArgs));
	}

	UBlueprint* CreateDiskBackedBlueprintChild(
		FAutomationTestBase& Test,
		FAssetRegistryModule& AssetRegistryModule,
		UClass* ParentClass,
		FStringView Suffix,
		FString& OutPackagePath,
		FString& OutPackageFilename)
	{
		const FString AssetName = FString::Printf(
			TEXT("BP_EditorImpactDiscovery_%.*s_%s"),
			Suffix.Len(),
			Suffix.GetData(),
			*FGuid::NewGuid().ToString(EGuidFormats::Digits));
		OutPackagePath = FString::Printf(TEXT("/Game/Automation/%s"), *AssetName);

		UPackage* Package = CreatePackage(*OutPackagePath);
		if (!Test.TestNotNull(TEXT("BlueprintImpact asset discovery test should create a disk-backed package"), Package))
		{
			return nullptr;
		}

		UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprint(
			ParentClass,
			Package,
			FName(*AssetName),
			BPTYPE_Normal,
			UBlueprint::StaticClass(),
			UBlueprintGeneratedClass::StaticClass(),
			TEXT("AngelscriptBlueprintImpactAssetDiscoveryTests"));
		if (Blueprint == nullptr)
		{
			return nullptr;
		}

		FKismetEditorUtilities::CompileBlueprint(Blueprint);
		if (!RegisterBlueprintAsset(
				Test,
				AssetRegistryModule,
				*Blueprint,
				TEXT("BlueprintImpact asset discovery test should compile and register the disk-backed blueprint")))
		{
			return nullptr;
		}

		if (!SaveBlueprintToDisk(Test, *Blueprint, OutPackagePath, OutPackageFilename))
		{
			return nullptr;
		}

		Blueprint->AddToRoot();
		return Blueprint;
	}

	void CleanupBlueprintAsset(
		FAutomationTestBase& Test,
		FAssetRegistryModule& AssetRegistryModule,
		UBlueprint*& Blueprint,
		const FString& PackageFilename)
	{
		if (Blueprint == nullptr)
		{
			return;
		}

		AssetRegistryModule.AssetDeleted(Blueprint);
		if (Blueprint->IsRooted())
		{
			Blueprint->RemoveFromRoot();
		}

		if (!PackageFilename.IsEmpty() && IFileManager::Get().FileExists(*PackageFilename))
		{
			Test.TestTrue(
				TEXT("BlueprintImpact asset discovery test should delete the saved blueprint package during cleanup"),
				IFileManager::Get().Delete(*PackageFilename, false, true, true));
		}

		if (UClass* BlueprintClass = Blueprint->GeneratedClass)
		{
			BlueprintClass->MarkAsGarbage();
		}

		if (UPackage* BlueprintPackage = Blueprint->GetOutermost())
		{
			BlueprintPackage->MarkAsGarbage();
		}

		Blueprint->MarkAsGarbage();
		CollectGarbage(RF_NoFlags, true);
		Blueprint = nullptr;
	}

	TArray<FString> CollectPackagePaths(const TArray<FAssetData>& Assets)
	{
		TArray<FString> PackagePaths;
		PackagePaths.Reserve(Assets.Num());
		for (const FAssetData& Asset : Assets)
		{
			PackagePaths.Add(Asset.PackageName.ToString());
		}
		return PackagePaths;
	}

	TSet<FString> CollectPackagePathSet(const TArray<FAssetData>& Assets)
	{
		TSet<FString> PackagePaths;
		for (const FAssetData& Asset : Assets)
		{
			PackagePaths.Add(Asset.PackageName.ToString());
		}
		return PackagePaths;
	}

	TSet<FString> SubtractPackageSets(const TSet<FString>& Left, const TSet<FString>& Right)
	{
		TSet<FString> Difference = Left;
		for (const FString& RightPath : Right)
		{
			Difference.Remove(RightPath);
		}
		return Difference;
	}
}

#define TestEqual(...) Test.TestEqual(__VA_ARGS__)
#define TestFalse(...) Test.TestFalse(__VA_ARGS__)
#define TestNotNull(...) Test.TestNotNull(__VA_ARGS__)
#define TestTrue(...) Test.TestTrue(__VA_ARGS__)

static bool RunDiskOnlyExcludesTransientButAllAssetsIncludesBoth(FAutomationTestBase& Test)
{
	using namespace AngelscriptEditor_Private_Tests_AngelscriptBlueprintImpactAssetDiscoveryTests_Private;
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));

	const TArray<FAssetData> BaselineDiskOnlyAssets =
		AngelscriptEditor::BlueprintImpact::FindBlueprintAssets(AssetRegistryModule.Get(), true);
	const TArray<FAssetData> BaselineAllAssets =
		AngelscriptEditor::BlueprintImpact::FindBlueprintAssets(AssetRegistryModule.Get(), false);
	const TSet<FString> BaselineDiskOnlyPackagePaths = CollectPackagePathSet(BaselineDiskOnlyAssets);
	const TSet<FString> BaselineAllPackagePaths = CollectPackagePathSet(BaselineAllAssets);

	FString DiskPackagePath;
	FString DiskPackageFilename;
	UBlueprint* DiskBlueprint = CreateDiskBackedBlueprintChild(
		Test,
		AssetRegistryModule,
		AActor::StaticClass(),
		TEXT("DiskOnly"),
		DiskPackagePath,
		DiskPackageFilename);

	FString UnsavedPackagePath;
	UBlueprint* UnsavedBlueprint = CreateUnsavedBlueprintChild(
		Test,
		AssetRegistryModule,
		AActor::StaticClass(),
		TEXT("MemoryOnly"),
		UnsavedPackagePath);

	ON_SCOPE_EXIT
	{
		CleanupBlueprintAsset(Test, AssetRegistryModule, UnsavedBlueprint, FString());
		CleanupBlueprintAsset(Test, AssetRegistryModule, DiskBlueprint, DiskPackageFilename);
	};

	if (!TestNotNull(TEXT("BlueprintImpact.FindBlueprintAssets mixed-mode test should create the disk-backed blueprint"), DiskBlueprint))
	{
		return false;
	}
	if (!TestNotNull(TEXT("BlueprintImpact.FindBlueprintAssets mixed-mode test should create the unsaved in-memory blueprint"), UnsavedBlueprint))
	{
		return false;
	}
	if (!TestFalse(
			TEXT("BlueprintImpact.FindBlueprintAssets mixed-mode test should keep the unsaved in-memory blueprint off disk"),
			FPackageName::DoesPackageExist(UnsavedPackagePath)))
	{
		return false;
	}

	AssetRegistryModule.Get().ScanModifiedAssetFiles({ DiskPackageFilename });

	const TArray<FAssetData> DiskOnlyAssets =
		AngelscriptEditor::BlueprintImpact::FindBlueprintAssets(AssetRegistryModule.Get(), true);
	const TArray<FAssetData> AllAssets =
		AngelscriptEditor::BlueprintImpact::FindBlueprintAssets(AssetRegistryModule.Get(), false);

	const TArray<FString> DiskOnlyPackagePaths = CollectPackagePaths(DiskOnlyAssets);
	const TArray<FString> AllPackagePaths = CollectPackagePaths(AllAssets);
	const TSet<FString> DiskOnlyPackagePathSet = CollectPackagePathSet(DiskOnlyAssets);
	const TSet<FString> AllPackagePathSet = CollectPackagePathSet(AllAssets);
	const TSet<FString> DiskOnlyDelta = SubtractPackageSets(DiskOnlyPackagePathSet, BaselineDiskOnlyPackagePaths);
	const TSet<FString> AllAssetsDelta = SubtractPackageSets(AllPackagePathSet, BaselineAllPackagePaths);

	if (!TestEqual(
			TEXT("BlueprintImpact.FindBlueprintAssets disk-only results should not contain duplicate package paths"),
			DiskOnlyPackagePaths.Num(),
			DiskOnlyPackagePathSet.Num()))
	{
		return false;
	}
	if (!TestEqual(
			TEXT("BlueprintImpact.FindBlueprintAssets all-assets results should not contain duplicate package paths"),
			AllPackagePaths.Num(),
			AllPackagePathSet.Num()))
	{
		return false;
	}
	if (!TestTrue(
			TEXT("BlueprintImpact.FindBlueprintAssets disk-only delta should include the saved blueprint package"),
			DiskOnlyDelta.Contains(DiskPackagePath)))
	{
		return false;
	}
	if (!TestFalse(
			TEXT("BlueprintImpact.FindBlueprintAssets disk-only results should exclude the unsaved in-memory blueprint package"),
			DiskOnlyPackagePathSet.Contains(UnsavedPackagePath)))
	{
		return false;
	}
	if (!TestEqual(
			TEXT("BlueprintImpact.FindBlueprintAssets disk-only delta should only add the saved blueprint package"),
			DiskOnlyDelta.Num(),
			1))
	{
		return false;
	}
	if (!TestTrue(
			TEXT("BlueprintImpact.FindBlueprintAssets all-assets results should include the saved blueprint package"),
			AllPackagePathSet.Contains(DiskPackagePath)))
	{
		return false;
	}
	if (!TestTrue(
			TEXT("BlueprintImpact.FindBlueprintAssets all-assets results should include the unsaved in-memory blueprint package"),
			AllPackagePathSet.Contains(UnsavedPackagePath)))
	{
		return false;
	}
	if (!TestEqual(
			TEXT("BlueprintImpact.FindBlueprintAssets all-assets delta should add both the saved and unsaved blueprint packages"),
			AllAssetsDelta.Num(),
			2))
	{
		return false;
	}
	if (!TestTrue(
			TEXT("BlueprintImpact.FindBlueprintAssets all-assets delta should include the saved blueprint package"),
			AllAssetsDelta.Contains(DiskPackagePath)))
	{
		return false;
	}

	return TestTrue(
		TEXT("BlueprintImpact.FindBlueprintAssets all-assets delta should include the unsaved in-memory blueprint package"),
		AllAssetsDelta.Contains(UnsavedPackagePath));
}

#undef TestTrue
#undef TestNotNull
#undef TestFalse
#undef TestEqual

TEST_CLASS_WITH_FLAGS(FAngelscriptBlueprintImpactAssetDiscoveryTests,
	"Angelscript.Editor.BlueprintImpact.FindBlueprintAssets",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(DiskOnlyExcludesTransientButAllAssetsIncludesBoth)
	{
		ASSERT_THAT(IsTrue(RunDiskOnlyExcludesTransientButAllAssetsIncludesBoth(*TestRunner)));
	}
};

#endif
