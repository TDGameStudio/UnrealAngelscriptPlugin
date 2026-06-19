#include "ContentBrowser/AngelscriptContentBrowserDataSource.h"

#include "AngelscriptEngine.h"
#include "AngelscriptRuntimeModule.h"
#include "AngelscriptRuntimeModule.h"

#include "AssetRegistry/AssetData.h"
#include "AssetThumbnail.h"
#include "CQTest.h"
#include "ContentBrowserItem.h"
#include "Curves/CurveBase.h"
#include "Curves/CurveFloat.h"
#include "Curves/CurveVector.h"
#include "Misc/AutomationTest.h"
#include "Misc/Guid.h"
#include "Misc/ScopeExit.h"
#include "UObject/GarbageCollection.h"
#include "UObject/UObjectGlobals.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace AngelscriptEditor_Private_Tests_AngelscriptContentBrowserDataSourceTests_Private
{
	template <typename AssetType>
	AssetType* CreateContentBrowserTestAsset(FAutomationTestBase& Test, UPackage* AssetsPackage, const TCHAR* BaseName)
	{
		if (!Test.TestNotNull(TEXT("ContentBrowserDataSource test should have a valid assets package"), AssetsPackage))
		{
			return nullptr;
		}

		const FString UniqueSuffix = FGuid::NewGuid().ToString(EGuidFormats::Digits);
		const FName DesiredName(*FString::Printf(TEXT("%s_%s"), BaseName, *UniqueSuffix));
		const FName UniqueName = MakeUniqueObjectName(AssetsPackage, AssetType::StaticClass(), DesiredName);
		return NewObject<AssetType>(AssetsPackage, UniqueName, RF_Public | RF_Standalone | RF_Transient);
	}

	void CleanupContentBrowserTestAsset(UObject*& Asset)
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

	FString MakeExpectedDisplayName(const UObject& Asset)
	{
		FString DisplayName = Asset.GetName();
		DisplayName.RemoveFromStart(TEXT("Asset_"));
		return DisplayName;
	}

	FString MakeExpectedVirtualPath(const UObject& Asset)
	{
		return FString::Printf(TEXT("/All/Angelscript/%s"), *Asset.GetName());
	}

	FContentBrowserDataCompiledFilter MakeCompiledFilter(UAngelscriptContentBrowserDataSource& DataSource, const UClass& IncludeClass, const UClass& ExcludeClass)
	{
		FContentBrowserDataFilter Filter;
		Filter.ItemTypeFilter = EContentBrowserItemTypeFilter::IncludeFiles;
		Filter.ItemCategoryFilter = EContentBrowserItemCategoryFilter::IncludeAssets;

		FContentBrowserDataClassFilter& ClassFilter = Filter.ExtraFilters.FindOrAddFilter<FContentBrowserDataClassFilter>();
		ClassFilter.ClassNamesToInclude.Add(IncludeClass.GetPathName());
		ClassFilter.ClassNamesToExclude.Add(ExcludeClass.GetPathName());

		FContentBrowserDataCompiledFilter CompiledFilter;
		CompiledFilter.ItemTypeFilter = Filter.ItemTypeFilter;
		CompiledFilter.ItemCategoryFilter = Filter.ItemCategoryFilter;
		CompiledFilter.ItemAttributeFilter = Filter.ItemAttributeFilter;

		DataSource.CompileFilter(TEXT("/"), Filter, CompiledFilter);
		return CompiledFilter;
	}

	TArray<FContentBrowserItem> CollectMatchingItems(UAngelscriptContentBrowserDataSource& DataSource, const FContentBrowserDataCompiledFilter& CompiledFilter)
	{
		TArray<FContentBrowserItem> Items;
		DataSource.EnumerateItemsMatchingFilter(CompiledFilter, [&Items](FContentBrowserItemData&& ItemData)
		{
			Items.Emplace(MoveTemp(ItemData));
			return true;
		});
		return Items;
	}

	const FContentBrowserItem* FindItemByName(const TArray<FContentBrowserItem>& Items, const FName ItemName)
	{
		return Items.FindByPredicate([ItemName](const FContentBrowserItem& Item)
		{
			return Item.GetItemName() == ItemName;
		});
	}

	struct FAssetDataSnapshot
	{
		FName AssetName = NAME_None;
		FTopLevelAssetPath AssetClassPath;
		FString ObjectPath;
	};

	FAssetDataSnapshot CaptureAssetDataSnapshot(const FAssetData& AssetData)
	{
		FAssetDataSnapshot Snapshot;
		Snapshot.AssetName = AssetData.AssetName;
		Snapshot.AssetClassPath = AssetData.AssetClassPath;
		Snapshot.ObjectPath = AssetData.GetSoftObjectPath().ToString();
		return Snapshot;
	}

	bool TestAssetDataSnapshotEquals(
		FAutomationTestBase& Test,
		const TCHAR* ContextLabel,
		const FAssetData& ActualAssetData,
		const FAssetDataSnapshot& ExpectedAssetData)
	{
		bool bMatched = true;
		bMatched &= Test.TestEqual(
			*FString::Printf(TEXT("%s should preserve the expected asset name"), ContextLabel),
			ActualAssetData.AssetName,
			ExpectedAssetData.AssetName);
		bMatched &= Test.TestEqual(
			*FString::Printf(TEXT("%s should preserve the expected asset class path"), ContextLabel),
			ActualAssetData.AssetClassPath,
			ExpectedAssetData.AssetClassPath);
		bMatched &= Test.TestEqual(
			*FString::Printf(TEXT("%s should preserve the expected object path"), ContextLabel),
			ActualAssetData.GetSoftObjectPath().ToString(),
			ExpectedAssetData.ObjectPath);
		return bMatched;
	}
}

#define TestEqual(...) Test.TestEqual(__VA_ARGS__)
#define TestFalse(...) Test.TestFalse(__VA_ARGS__)
#define TestNotNull(...) Test.TestNotNull(__VA_ARGS__)
#define TestTrue(...) Test.TestTrue(__VA_ARGS__)

static bool RunFiltersAssetsAndBuildsExpectedAttributes(FAutomationTestBase& Test)
{
	using namespace AngelscriptEditor_Private_Tests_AngelscriptContentBrowserDataSourceTests_Private;
	FAngelscriptRuntimeModule::InitializeAngelscript();
	FAngelscriptRuntimeModule::InitializeAngelscript();
	if (!TestTrue(TEXT("ContentBrowserDataSource test requires the Angelscript engine to be initialized"), FAngelscriptEngine::IsInitialized()))
	{
		return false;
	}

	FAngelscriptEngine& Engine = FAngelscriptEngine::Get();
	UPackage* AssetsPackage = Engine.AssetsPackage;
	if (!TestNotNull(TEXT("ContentBrowserDataSource test should resolve the script assets package"), AssetsPackage))
	{
		return false;
	}

	UAngelscriptContentBrowserDataSource* DataSource = NewObject<UAngelscriptContentBrowserDataSource>(GetTransientPackage());
	if (!TestNotNull(TEXT("ContentBrowserDataSource test should create a data source instance"), DataSource))
	{
		return false;
	}
	DataSource->Initialize();

	UObject* IncludedAssetObject = CreateContentBrowserTestAsset<UCurveFloat>(Test, AssetsPackage, TEXT("Asset_FilteredCurve"));
	UObject* ExcludedAssetObject = CreateContentBrowserTestAsset<UCurveVector>(Test, AssetsPackage, TEXT("Asset_ExcludedCurve"));
	UCurveFloat* IncludedAsset = Cast<UCurveFloat>(IncludedAssetObject);
	UCurveVector* ExcludedAsset = Cast<UCurveVector>(ExcludedAssetObject);
	if (!TestNotNull(TEXT("ContentBrowserDataSource test should create the included asset"), IncludedAsset)
		|| !TestNotNull(TEXT("ContentBrowserDataSource test should create the excluded asset"), ExcludedAsset))
	{
		CleanupContentBrowserTestAsset(IncludedAssetObject);
		CleanupContentBrowserTestAsset(ExcludedAssetObject);
		CollectGarbage(RF_NoFlags, true);
		return false;
	}

	ON_SCOPE_EXIT
	{
		CleanupContentBrowserTestAsset(IncludedAssetObject);
		CleanupContentBrowserTestAsset(ExcludedAssetObject);
		CollectGarbage(RF_NoFlags, true);
	};

	const FContentBrowserDataCompiledFilter CompiledFilter = MakeCompiledFilter(*DataSource, *UCurveBase::StaticClass(), *UCurveVector::StaticClass());
	const TArray<FContentBrowserItem> Items = CollectMatchingItems(*DataSource, CompiledFilter);

	const FContentBrowserItem* IncludedItem = FindItemByName(Items, IncludedAsset->GetFName());
	if (!TestNotNull(TEXT("ContentBrowserDataSource should enumerate the include-matching asset"), IncludedItem))
	{
		return false;
	}

	if (!TestFalse(
			TEXT("ContentBrowserDataSource should not enumerate the excluded asset"),
			Items.ContainsByPredicate([ExcludedAsset](const FContentBrowserItem& Item)
			{
				return Item.GetItemName() == ExcludedAsset->GetFName();
			})))
	{
		return false;
	}

	if (!TestEqual(
			TEXT("ContentBrowserDataSource should build the expected virtual path"),
			IncludedItem->GetVirtualPath(),
			FName(*MakeExpectedVirtualPath(*IncludedAsset))))
	{
		return false;
	}

	if (!TestEqual(
			TEXT("ContentBrowserDataSource should strip the Asset_ prefix from the display name"),
			IncludedItem->GetDisplayName().ToString(),
			MakeExpectedDisplayName(*IncludedAsset)))
	{
		return false;
	}

	const FContentBrowserItemDataAttributeValue TypeNameAttribute = IncludedItem->GetItemAttribute(ContentBrowserItemAttributes::ItemTypeName);
	if (!TestTrue(TEXT("ContentBrowserDataSource should expose ItemTypeName"), TypeNameAttribute.IsValid()))
	{
		return false;
	}
	if (!TestEqual(
			TEXT("ContentBrowserDataSource should report the script asset item type name"),
			TypeNameAttribute.GetValue<FString>(),
			FString(TEXT("Script Asset"))))
	{
		return false;
	}

	const FContentBrowserItemDataAttributeValue ProjectContentAttribute = IncludedItem->GetItemAttribute(ContentBrowserItemAttributes::ItemIsProjectContent);
	if (!TestTrue(TEXT("ContentBrowserDataSource should expose ItemIsProjectContent"), ProjectContentAttribute.IsValid()))
	{
		return false;
	}
	if (!TestTrue(
			TEXT("ContentBrowserDataSource should report script assets as project content"),
			ProjectContentAttribute.GetValue<bool>()))
	{
		return false;
	}

	FName PackagePath = NAME_None;
	if (!TestTrue(TEXT("ContentBrowserDataSource should provide a legacy package path"), IncludedItem->Legacy_TryGetPackagePath(PackagePath)))
	{
		return false;
	}
	if (!TestEqual(
			TEXT("ContentBrowserDataSource should return the payload path through Legacy_TryGetPackagePath"),
			PackagePath,
			FName(*IncludedAsset->GetPathName())))
	{
		return false;
	}

	FAssetData AssetData;
	if (!TestTrue(TEXT("ContentBrowserDataSource should provide asset data for enumerated items"), IncludedItem->Legacy_TryGetAssetData(AssetData)))
	{
		return false;
	}
	if (!TestEqual(
			TEXT("ContentBrowserDataSource should preserve the asset name in legacy asset data"),
			AssetData.AssetName,
			IncludedAsset->GetFName()))
	{
		return false;
	}
	if (!TestEqual(
			TEXT("ContentBrowserDataSource should preserve the class path in legacy asset data"),
			AssetData.AssetClassPath,
			IncludedAsset->GetClass()->GetClassPathName()))
	{
		return false;
	}
	return TestEqual(
		TEXT("ContentBrowserDataSource should preserve the object path in legacy asset data"),
		AssetData.GetSoftObjectPath().ToString(),
		IncludedAsset->GetPathName());
}

static bool RunRejectsStalePayloadAcrossAttributesLegacyLookupAndThumbnail(FAutomationTestBase& Test)
{
	using namespace AngelscriptEditor_Private_Tests_AngelscriptContentBrowserDataSourceTests_Private;
	FAngelscriptRuntimeModule::InitializeAngelscript();
	if (!TestTrue(TEXT("Stale payload ContentBrowserDataSource test requires the Angelscript engine to be initialized"), FAngelscriptEngine::IsInitialized()))
	{
		return false;
	}

	FAngelscriptEngine& Engine = FAngelscriptEngine::Get();
	UPackage* AssetsPackage = Engine.AssetsPackage;
	if (!TestNotNull(TEXT("Stale payload ContentBrowserDataSource test should resolve the script assets package"), AssetsPackage))
	{
		return false;
	}

	UAngelscriptContentBrowserDataSource* DataSource = NewObject<UAngelscriptContentBrowserDataSource>(GetTransientPackage());
	if (!TestNotNull(TEXT("Stale payload ContentBrowserDataSource test should create a data source instance"), DataSource))
	{
		return false;
	}
	DataSource->Initialize();
	DataSource->AddToRoot();

	UObject* StaleAssetObject = CreateContentBrowserTestAsset<UCurveFloat>(Test, AssetsPackage, TEXT("Asset_StalePayloadCurve"));
	UCurveFloat* StaleAsset = Cast<UCurveFloat>(StaleAssetObject);
	if (!TestNotNull(TEXT("Stale payload ContentBrowserDataSource test should create the payload asset"), StaleAsset))
	{
		DataSource->RemoveFromRoot();
		CleanupContentBrowserTestAsset(StaleAssetObject);
		CollectGarbage(RF_NoFlags, true);
		return false;
	}

	ON_SCOPE_EXIT
	{
		if (DataSource != nullptr)
		{
			DataSource->RemoveFromRoot();
		}

		CleanupContentBrowserTestAsset(StaleAssetObject);
		CollectGarbage(RF_NoFlags, true);
	};

	const FContentBrowserDataCompiledFilter CompiledFilter = MakeCompiledFilter(*DataSource, *UCurveBase::StaticClass(), *UCurveVector::StaticClass());
	const TArray<FContentBrowserItem> Items = CollectMatchingItems(*DataSource, CompiledFilter);
	const FContentBrowserItem* StaleItem = FindItemByName(Items, StaleAsset->GetFName());
	if (!TestNotNull(TEXT("Stale payload ContentBrowserDataSource test should enumerate the canonical asset item"), StaleItem))
	{
		return false;
	}

	const FContentBrowserItemData* StaleItemData = StaleItem->GetPrimaryInternalItem();
	if (!TestNotNull(TEXT("Stale payload ContentBrowserDataSource test should expose the primary internal item"), StaleItemData))
	{
		return false;
	}

	bool bPassed = true;

	FContentBrowserItemDataAttributeValue LiveAttributeValue;
	bPassed &= TestTrue(
		TEXT("Canonical payload should expose ItemTypeName before the asset becomes stale"),
		DataSource->GetItemAttribute(*StaleItemData, false, ContentBrowserItemAttributes::ItemTypeName, LiveAttributeValue));
	bPassed &= TestTrue(
		TEXT("Canonical payload should produce a valid ItemTypeName attribute before the asset becomes stale"),
		LiveAttributeValue.IsValid());
	if (LiveAttributeValue.IsValid())
	{
		bPassed &= TestEqual(
			TEXT("Canonical payload should still report the script asset item type before going stale"),
			LiveAttributeValue.GetValue<FString>(),
			FString(TEXT("Script Asset")));
	}

	FAssetData LiveAssetData;
	bPassed &= TestTrue(
		TEXT("Canonical payload should provide legacy asset data before the asset becomes stale"),
		DataSource->Legacy_TryGetAssetData(*StaleItemData, LiveAssetData));
	bPassed &= TestEqual(
		TEXT("Canonical payload should preserve the asset name before the asset becomes stale"),
		LiveAssetData.AssetName,
		StaleAsset->GetFName());
	bPassed &= TestEqual(
		TEXT("Canonical payload should preserve the object path before the asset becomes stale"),
		LiveAssetData.GetSoftObjectPath().ToString(),
		StaleAsset->GetPathName());

	const TSharedRef<FAssetThumbnailPool> ThumbnailPool = MakeShared<FAssetThumbnailPool>(1);
	FAssetThumbnail Thumbnail(LiveAssetData, 32, 32, ThumbnailPool);
	bPassed &= TestTrue(
		TEXT("Canonical payload should update thumbnails before the asset becomes stale"),
		DataSource->UpdateThumbnail(*StaleItemData, Thumbnail));

	const FAssetDataSnapshot ExpectedLegacySnapshot = CaptureAssetDataSnapshot(LiveAssetData);
	const FAssetDataSnapshot ExpectedThumbnailSnapshot = CaptureAssetDataSnapshot(Thumbnail.GetAssetData());
	TWeakObjectPtr<UObject> StaleAssetWeak = StaleAssetObject;

	CleanupContentBrowserTestAsset(StaleAssetObject);
	CollectGarbage(RF_NoFlags, true);

	bPassed &= TestFalse(
		TEXT("Payload asset should become invalid after cleanup and garbage collection"),
		StaleAssetWeak.IsValid());

	FContentBrowserItemDataAttributeValue StaleAttributeValue;
	bPassed &= TestFalse(
		TEXT("Stale payload should reject ItemTypeName lookup"),
		DataSource->GetItemAttribute(*StaleItemData, false, ContentBrowserItemAttributes::ItemTypeName, StaleAttributeValue));
	bPassed &= TestFalse(
		TEXT("Stale payload should keep the attribute output invalid when ItemTypeName lookup is rejected"),
		StaleAttributeValue.IsValid());

	FAssetData StaleLegacyAssetData = LiveAssetData;
	bPassed &= TestFalse(
		TEXT("Stale payload should reject legacy asset-data lookup"),
		DataSource->Legacy_TryGetAssetData(*StaleItemData, StaleLegacyAssetData));
	bPassed &= TestAssetDataSnapshotEquals(
		Test,
		TEXT("Rejected legacy asset-data lookup"),
		StaleLegacyAssetData,
		ExpectedLegacySnapshot);

	bPassed &= TestFalse(
		TEXT("Stale payload should reject thumbnail updates"),
		DataSource->UpdateThumbnail(*StaleItemData, Thumbnail));
	bPassed &= TestAssetDataSnapshotEquals(
		Test,
		TEXT("Rejected thumbnail update"),
		Thumbnail.GetAssetData(),
		ExpectedThumbnailSnapshot);

	return bPassed;
}

#undef TestEqual
#undef TestFalse
#undef TestNotNull
#undef TestTrue

TEST_CLASS_WITH_FLAGS(FAngelscriptContentBrowserDataSourceTests,
	"Angelscript.Editor.ContentBrowserDataSource",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(FiltersAssetsAndBuildsExpectedAttributes)
	{
		ASSERT_THAT(IsTrue(RunFiltersAssetsAndBuildsExpectedAttributes(*TestRunner)));
	}

	TEST_METHOD(RejectsStalePayloadAcrossAttributesLegacyLookupAndThumbnail)
	{
		ASSERT_THAT(IsTrue(RunRejectsStalePayloadAcrossAttributesLegacyLookupAndThumbnail(*TestRunner)));
	}
};

#endif
