#include "ContentBrowser/AngelscriptContentBrowserDataSource.h"

#include "AngelscriptEngine.h"
#include "AngelscriptRuntimeModule.h"
#include "AngelscriptRuntimeModule.h"

#include "CQTest.h"
#include "ContentBrowserItem.h"
#include "ContentBrowserItemData.h"
#include "Curves/CurveFloat.h"
#include "Curves/CurveVector.h"
#include "Misc/AutomationTest.h"
#include "Misc/Guid.h"
#include "Misc/ScopeExit.h"
#include "UObject/GarbageCollection.h"
#include "UObject/UObjectGlobals.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace AngelscriptEditor_Private_Tests_AngelscriptContentBrowserDataSourceFilterTests_Private
{
	template <typename AssetType>
	AssetType* CreateContentBrowserFilterTestAsset(FAutomationTestBase& Test, UPackage* AssetsPackage, const TCHAR* BaseName)
	{
		if (!Test.TestNotNull(TEXT("ContentBrowserDataSource filter test should have a valid assets package"), AssetsPackage))
		{
			return nullptr;
		}

		const FString UniqueSuffix = FGuid::NewGuid().ToString(EGuidFormats::Digits);
		const FName DesiredName(*FString::Printf(TEXT("%s_%s"), BaseName, *UniqueSuffix));
		const FName UniqueName = MakeUniqueObjectName(AssetsPackage, AssetType::StaticClass(), DesiredName);
		return NewObject<AssetType>(AssetsPackage, UniqueName, RF_Public | RF_Standalone | RF_Transient);
	}

	void CleanupContentBrowserFilterTestAsset(UObject*& Asset)
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

	FContentBrowserDataFilter MakeFilterDefinition(
		const EContentBrowserItemTypeFilter ItemTypeFilter,
		const EContentBrowserItemCategoryFilter ItemCategoryFilter,
		const TArray<const UClass*>& IncludeClasses,
		const TArray<const UClass*>& ExcludeClasses,
		const bool bAddClassFilter = true)
	{
		FContentBrowserDataFilter Filter;
		Filter.ItemTypeFilter = ItemTypeFilter;
		Filter.ItemCategoryFilter = ItemCategoryFilter;

		if (!bAddClassFilter)
		{
			return Filter;
		}

		FContentBrowserDataClassFilter& ClassFilter = Filter.ExtraFilters.FindOrAddFilter<FContentBrowserDataClassFilter>();
		for (const UClass* IncludeClass : IncludeClasses)
		{
			ClassFilter.ClassNamesToInclude.Add(IncludeClass->GetPathName());
		}

		for (const UClass* ExcludeClass : ExcludeClasses)
		{
			ClassFilter.ClassNamesToExclude.Add(ExcludeClass->GetPathName());
		}

		return Filter;
	}

	void CompileIntoCompiledFilter(
		UAngelscriptContentBrowserDataSource& DataSource,
		const FName InPath,
		const FContentBrowserDataFilter& Filter,
		FContentBrowserDataCompiledFilter& InOutCompiledFilter)
	{
		InOutCompiledFilter.ItemTypeFilter = Filter.ItemTypeFilter;
		InOutCompiledFilter.ItemCategoryFilter = Filter.ItemCategoryFilter;
		InOutCompiledFilter.ItemAttributeFilter = Filter.ItemAttributeFilter;
		DataSource.CompileFilter(InPath, Filter, InOutCompiledFilter);
	}

	const FContentBrowserAngelscriptFilter* FindCompiledScriptFilter(
		const UAngelscriptContentBrowserDataSource& DataSource,
		const FContentBrowserDataCompiledFilter& CompiledFilter)
	{
		const FContentBrowserDataFilterList* FilterList = CompiledFilter.CompiledFilters.Find(&DataSource);
		return FilterList != nullptr
			? FilterList->FindFilter<FContentBrowserAngelscriptFilter>()
			: nullptr;
	}

	FContentBrowserDataCompiledFilter MakeCompiledFilter(
		UAngelscriptContentBrowserDataSource& DataSource,
		const TArray<const UClass*>& IncludeClasses,
		const TArray<const UClass*>& ExcludeClasses)
	{
		const FContentBrowserDataFilter Filter = MakeFilterDefinition(
			EContentBrowserItemTypeFilter::IncludeFiles,
			EContentBrowserItemCategoryFilter::IncludeAssets,
			IncludeClasses,
			ExcludeClasses);

		FContentBrowserDataCompiledFilter CompiledFilter;
		CompileIntoCompiledFilter(DataSource, TEXT("/"), Filter, CompiledFilter);
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

	FContentBrowserItemData MakeForeignOwnerItem(const FContentBrowserItemData& SourceItem, UContentBrowserDataSource& ForeignOwner)
	{
		return FContentBrowserItemData(
			&ForeignOwner,
			SourceItem.GetItemFlags(),
			SourceItem.GetVirtualPath(),
			SourceItem.GetItemName(),
			SourceItem.GetDisplayName(),
			SourceItem.GetPayload(),
			SourceItem.GetInternalPath());
	}
}

#define TestEqual(...) Test.TestEqual(__VA_ARGS__)
#define TestFalse(...) Test.TestFalse(__VA_ARGS__)
#define TestNotNull(...) Test.TestNotNull(__VA_ARGS__)
#define TestTrue(...) Test.TestTrue(__VA_ARGS__)

static bool RunDoesItemPassFilterHonorsCompiledClassFiltersAndOwner(FAutomationTestBase& Test)
{
	using namespace AngelscriptEditor_Private_Tests_AngelscriptContentBrowserDataSourceFilterTests_Private;
	FAngelscriptRuntimeModule::InitializeAngelscript();
	FAngelscriptRuntimeModule::InitializeAngelscript();
	if (!TestTrue(TEXT("ContentBrowserDataSource DoesItemPassFilter test requires the Angelscript engine to be initialized"), FAngelscriptEngine::IsInitialized()))
	{
		return false;
	}

	FAngelscriptEngine& Engine = FAngelscriptEngine::Get();
	UPackage* AssetsPackage = Engine.AssetsPackage;
	if (!TestNotNull(TEXT("ContentBrowserDataSource DoesItemPassFilter test should resolve the script assets package"), AssetsPackage))
	{
		return false;
	}

	UAngelscriptContentBrowserDataSource* DataSource = NewObject<UAngelscriptContentBrowserDataSource>(GetTransientPackage());
	UAngelscriptContentBrowserDataSource* ForeignOwnerDataSource = NewObject<UAngelscriptContentBrowserDataSource>(GetTransientPackage());
	if (!TestNotNull(TEXT("ContentBrowserDataSource DoesItemPassFilter test should create the primary data source"), DataSource)
		|| !TestNotNull(TEXT("ContentBrowserDataSource DoesItemPassFilter test should create the foreign-owner data source"), ForeignOwnerDataSource))
	{
		return false;
	}

	DataSource->Initialize();
	ForeignOwnerDataSource->Initialize();

	UObject* IncludedAssetObject = CreateContentBrowserFilterTestAsset<UCurveFloat>(Test, AssetsPackage, TEXT("Asset_FilterPassCurve"));
	UCurveFloat* IncludedAsset = Cast<UCurveFloat>(IncludedAssetObject);
	if (!TestNotNull(TEXT("ContentBrowserDataSource DoesItemPassFilter test should create the include-match asset"), IncludedAsset))
	{
		CleanupContentBrowserFilterTestAsset(IncludedAssetObject);
		CollectGarbage(RF_NoFlags, true);
		return false;
	}

	ON_SCOPE_EXIT
	{
		CleanupContentBrowserFilterTestAsset(IncludedAssetObject);
		CollectGarbage(RF_NoFlags, true);
	};

	TArray<const UClass*> IncludeOnlyClasses;
	IncludeOnlyClasses.Add(UCurveFloat::StaticClass());

	TArray<const UClass*> IncludeAndExcludeClasses;
	IncludeAndExcludeClasses.Add(UCurveFloat::StaticClass());

	TArray<const UClass*> MismatchedIncludeClasses;
	MismatchedIncludeClasses.Add(UCurveVector::StaticClass());

	const FContentBrowserDataCompiledFilter IncludeOnlyFilter = MakeCompiledFilter(*DataSource, IncludeOnlyClasses, {});
	const FContentBrowserDataCompiledFilter IncludeAndExcludeFilter = MakeCompiledFilter(*DataSource, IncludeAndExcludeClasses, IncludeAndExcludeClasses);
	const FContentBrowserDataCompiledFilter MismatchedIncludeFilter = MakeCompiledFilter(*DataSource, MismatchedIncludeClasses, {});

	const TArray<FContentBrowserItem> EnumeratedItems = CollectMatchingItems(*DataSource, IncludeOnlyFilter);
	const FContentBrowserItem* IncludedItem = FindItemByName(EnumeratedItems, IncludedAsset->GetFName());
	if (!TestNotNull(TEXT("ContentBrowserDataSource DoesItemPassFilter test should enumerate the include-match asset"), IncludedItem))
	{
		return false;
	}

	const FContentBrowserItemData* IncludedItemData = IncludedItem->GetPrimaryInternalItem();
	if (!TestNotNull(TEXT("ContentBrowserDataSource DoesItemPassFilter test should expose the primary internal item"), IncludedItemData))
	{
		return false;
	}

	const FContentBrowserItemData ForeignOwnerItem = MakeForeignOwnerItem(*IncludedItemData, *ForeignOwnerDataSource);

	bool bPassed = true;
	bPassed &= TestTrue(
		TEXT("ContentBrowserDataSource DoesItemPassFilter should accept include-only matching items"),
		DataSource->DoesItemPassFilter(*IncludedItemData, IncludeOnlyFilter));
	bPassed &= TestFalse(
		TEXT("ContentBrowserDataSource DoesItemPassFilter should reject items matched by both include and exclude classes"),
		DataSource->DoesItemPassFilter(*IncludedItemData, IncludeAndExcludeFilter));
	bPassed &= TestFalse(
		TEXT("ContentBrowserDataSource DoesItemPassFilter should reject items owned by a different data source instance"),
		DataSource->DoesItemPassFilter(ForeignOwnerItem, IncludeOnlyFilter));
	bPassed &= TestFalse(
		TEXT("ContentBrowserDataSource DoesItemPassFilter should reject items that miss the include classes"),
		DataSource->DoesItemPassFilter(*IncludedItemData, MismatchedIncludeFilter));
	return bPassed;
}

static bool RunCompileFilterClearsPreviousIncludeExcludeStateOnReusedCompiledFilter(FAutomationTestBase& Test)
{
	using namespace AngelscriptEditor_Private_Tests_AngelscriptContentBrowserDataSourceFilterTests_Private;
	FAngelscriptRuntimeModule::InitializeAngelscript();
	if (!TestTrue(TEXT("ContentBrowserDataSource CompileFilter reuse test requires the Angelscript engine to be initialized"), FAngelscriptEngine::IsInitialized()))
	{
		return false;
	}

	FAngelscriptEngine& Engine = FAngelscriptEngine::Get();
	UPackage* AssetsPackage = Engine.AssetsPackage;
	if (!TestNotNull(TEXT("ContentBrowserDataSource CompileFilter reuse test should resolve the script assets package"), AssetsPackage))
	{
		return false;
	}

	UAngelscriptContentBrowserDataSource* DataSource = NewObject<UAngelscriptContentBrowserDataSource>(GetTransientPackage());
	if (!TestNotNull(TEXT("ContentBrowserDataSource CompileFilter reuse test should create the data source"), DataSource))
	{
		return false;
	}

	DataSource->Initialize();

	UObject* IncludedAssetObject = CreateContentBrowserFilterTestAsset<UCurveFloat>(Test, AssetsPackage, TEXT("Asset_FilterReuseCurve"));
	UCurveFloat* IncludedAsset = Cast<UCurveFloat>(IncludedAssetObject);
	if (!TestNotNull(TEXT("ContentBrowserDataSource CompileFilter reuse test should create the include-match asset"), IncludedAsset))
	{
		CleanupContentBrowserFilterTestAsset(IncludedAssetObject);
		CollectGarbage(RF_NoFlags, true);
		return false;
	}

	ON_SCOPE_EXIT
	{
		CleanupContentBrowserFilterTestAsset(IncludedAssetObject);
		CollectGarbage(RF_NoFlags, true);
	};

	const TArray<const UClass*> IncludeClasses{ UCurveFloat::StaticClass() };
	const TArray<const UClass*> ExcludeClasses{ UCurveVector::StaticClass() };

	const FContentBrowserDataFilter ValidFilter = MakeFilterDefinition(
		EContentBrowserItemTypeFilter::IncludeFiles,
		EContentBrowserItemCategoryFilter::IncludeAssets,
		IncludeClasses,
		ExcludeClasses);
	const FContentBrowserDataFilter WrongPathFilter = ValidFilter;
	const FContentBrowserDataFilter FolderOnlyFilter = MakeFilterDefinition(
		EContentBrowserItemTypeFilter::IncludeFolders,
		EContentBrowserItemCategoryFilter::IncludeAssets,
		IncludeClasses,
		ExcludeClasses);
	const FContentBrowserDataFilter MissingClassFilter = MakeFilterDefinition(
		EContentBrowserItemTypeFilter::IncludeFiles,
		EContentBrowserItemCategoryFilter::IncludeAssets,
		{},
		{},
		false);

	FContentBrowserDataCompiledFilter ReusedCompiledFilter;
	CompileIntoCompiledFilter(*DataSource, TEXT("/"), ValidFilter, ReusedCompiledFilter);

	const FContentBrowserAngelscriptFilter* ScriptFilter = FindCompiledScriptFilter(*DataSource, ReusedCompiledFilter);
	if (!TestNotNull(TEXT("ContentBrowserDataSource CompileFilter reuse test should build an Angelscript-specific compiled filter for valid queries"), ScriptFilter))
	{
		return false;
	}

	bool bPassed = true;
	bPassed &= TestEqual(TEXT("ContentBrowserDataSource CompileFilter reuse test should record one include class for the valid baseline"), ScriptFilter->IncludeClasses.Num(), 1);
	bPassed &= TestEqual(TEXT("ContentBrowserDataSource CompileFilter reuse test should record one exclude class for the valid baseline"), ScriptFilter->ExcludeClasses.Num(), 1);
	bPassed &= TestEqual(TEXT("ContentBrowserDataSource CompileFilter reuse test should enumerate one asset for the valid baseline"), CollectMatchingItems(*DataSource, ReusedCompiledFilter).Num(), 1);

	CompileIntoCompiledFilter(*DataSource, TEXT("/All/Angelscript"), WrongPathFilter, ReusedCompiledFilter);
	ScriptFilter = FindCompiledScriptFilter(*DataSource, ReusedCompiledFilter);
	if (!TestNotNull(TEXT("ContentBrowserDataSource CompileFilter reuse test should preserve the compiled filter container for invalid-path reuse"), ScriptFilter))
	{
		return false;
	}

	bPassed &= TestEqual(TEXT("ContentBrowserDataSource CompileFilter reuse test should clear include classes after an invalid-path reuse"), ScriptFilter->IncludeClasses.Num(), 0);
	bPassed &= TestEqual(TEXT("ContentBrowserDataSource CompileFilter reuse test should clear exclude classes after an invalid-path reuse"), ScriptFilter->ExcludeClasses.Num(), 0);
	bPassed &= TestEqual(TEXT("ContentBrowserDataSource CompileFilter reuse test should enumerate no assets after an invalid-path reuse"), CollectMatchingItems(*DataSource, ReusedCompiledFilter).Num(), 0);

	CompileIntoCompiledFilter(*DataSource, TEXT("/"), FolderOnlyFilter, ReusedCompiledFilter);
	ScriptFilter = FindCompiledScriptFilter(*DataSource, ReusedCompiledFilter);
	if (!TestNotNull(TEXT("ContentBrowserDataSource CompileFilter reuse test should keep the compiled filter container for folder-only reuse"), ScriptFilter))
	{
		return false;
	}

	bPassed &= TestEqual(TEXT("ContentBrowserDataSource CompileFilter reuse test should keep include classes cleared after a folder-only reuse"), ScriptFilter->IncludeClasses.Num(), 0);
	bPassed &= TestEqual(TEXT("ContentBrowserDataSource CompileFilter reuse test should keep exclude classes cleared after a folder-only reuse"), ScriptFilter->ExcludeClasses.Num(), 0);
	bPassed &= TestEqual(TEXT("ContentBrowserDataSource CompileFilter reuse test should enumerate no assets after a folder-only reuse"), CollectMatchingItems(*DataSource, ReusedCompiledFilter).Num(), 0);

	CompileIntoCompiledFilter(*DataSource, TEXT("/"), MissingClassFilter, ReusedCompiledFilter);
	ScriptFilter = FindCompiledScriptFilter(*DataSource, ReusedCompiledFilter);
	if (!TestNotNull(TEXT("ContentBrowserDataSource CompileFilter reuse test should keep the compiled filter container for missing-class reuse"), ScriptFilter))
	{
		return false;
	}

	bPassed &= TestEqual(TEXT("ContentBrowserDataSource CompileFilter reuse test should keep include classes cleared after a missing-class reuse"), ScriptFilter->IncludeClasses.Num(), 0);
	bPassed &= TestEqual(TEXT("ContentBrowserDataSource CompileFilter reuse test should keep exclude classes cleared after a missing-class reuse"), ScriptFilter->ExcludeClasses.Num(), 0);
	bPassed &= TestEqual(TEXT("ContentBrowserDataSource CompileFilter reuse test should enumerate no assets after a missing-class reuse"), CollectMatchingItems(*DataSource, ReusedCompiledFilter).Num(), 0);

	return bPassed;
}

#undef TestEqual
#undef TestFalse
#undef TestNotNull
#undef TestTrue

TEST_CLASS_WITH_FLAGS(FAngelscriptContentBrowserDataSourceFilterTests,
	"Angelscript.Editor.ContentBrowserDataSource",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(DoesItemPassFilterHonorsCompiledClassFiltersAndOwner)
	{
		ASSERT_THAT(IsTrue(RunDoesItemPassFilterHonorsCompiledClassFiltersAndOwner(*TestRunner)));
	}

	TEST_METHOD(CompileFilterClearsPreviousIncludeExcludeStateOnReusedCompiledFilter)
	{
		ASSERT_THAT(IsTrue(RunCompileFilterClearsPreviousIncludeExcludeStateOnReusedCompiledFilter(*TestRunner)));
	}
};

#endif
