#include "Bind_AssetRegistry_Functions.h"

#include "AngelscriptEngine.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/Blueprint.h"
#include "Internationalization/Regex.h"

#if WITH_EDITOR
#include "WidgetBlueprint.h"
#endif

namespace
{
	IAssetRegistry& GetAssetRegistry()
	{
		FAssetRegistryModule& AssetRegistryModule = FModuleManager::Get().LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
		return AssetRegistryModule.Get();
	}

	template <typename BlueprintType>
	void AppendBlueprintCDOs(UClass* ParentClass, TArray<UObject*>& OutAssets)
	{
		IAssetRegistry& AssetRegistry = GetAssetRegistry();
		TSet<FTopLevelAssetPath> DerivedNames;
		TArray<FTopLevelAssetPath> BaseNames;
		BaseNames.Add(FTopLevelAssetPath(ParentClass));
		TSet<FTopLevelAssetPath> Excluded;
		AssetRegistry.GetDerivedClassNames(BaseNames, Excluded, DerivedNames);

		TArray<FAssetData> AssetList;
		AssetRegistry.GetAssetsByClass(FTopLevelAssetPath(BlueprintType::StaticClass()), AssetList);
		for (const FAssetData& Asset : AssetList)
		{
			FString AssetClassName;
			if (!Asset.GetTagValue(FBlueprintTags::GeneratedClassPath, AssetClassName))
				continue;

			const FString ClassObjectPath = FPackageName::ExportTextPathToObjectPath(AssetClassName);
			if (!DerivedNames.Contains(FTopLevelAssetPath(ClassObjectPath)))
				continue;

			BlueprintType* Blueprint = Cast<BlueprintType>(Asset.GetAsset());
			UObject* BlueprintDefaultObject = Blueprint->GeneratedClass->GetDefaultObject();
			if (BlueprintDefaultObject != nullptr)
				OutAssets.Add(BlueprintDefaultObject);
		}
	}
}

bool FAngelscriptAssetRegistryBinds::IsAssetDataInstanceOf(
	const FAssetData* AssetData,
	const UClass* BaseClass,
	bool bResolveClass)
{
	return AssetData->IsInstanceOf(BaseClass, bResolveClass ? EResolveClass::Yes : EResolveClass::No);
}

void FAngelscriptAssetRegistryBinds::ConstructTopLevelAssetPathFromObject(
	FTopLevelAssetPath* Memory,
	const UObject* AssetObject)
{
	new (Memory) FTopLevelAssetPath(AssetObject);
}

void FAngelscriptAssetRegistryBinds::ConstructTopLevelAssetPathFromString(
	FTopLevelAssetPath* Memory,
	const FString& AssetPath)
{
	new (Memory) FTopLevelAssetPath(AssetPath);
}

void FAngelscriptAssetRegistryBinds::ConstructTopLevelAssetPathFromNames(
	FTopLevelAssetPath* Memory,
	const FName& PackageName,
	const FName& AssetName)
{
	new (Memory) FTopLevelAssetPath(PackageName, AssetName);
}

FTopLevelAssetPath* FAngelscriptAssetRegistryBinds::AssignTopLevelAssetPath(
	FTopLevelAssetPath* Self,
	const FString& AssetPath)
{
	*Self = AssetPath;
	return Self;
}

void FAngelscriptAssetRegistryBinds::AppendTopLevelAssetPathToString(void* Value, FString& OutString)
{
	OutString += static_cast<FTopLevelAssetPath*>(Value)->ToString();
}

bool FAngelscriptAssetRegistryBinds::IsLoadingAssets()
{
	return GetAssetRegistry().IsLoadingAssets();
}

bool FAngelscriptAssetRegistryBinds::HasAssets(FName PackagePath, bool bRecursive)
{
	return GetAssetRegistry().HasAssets(PackagePath, bRecursive);
}

bool FAngelscriptAssetRegistryBinds::GetAssetsByPackageName(
	FName PackageName,
	TArray<FAssetData>& OutAssetData,
	bool bIncludeOnlyOnDiskAssets)
{
	return GetAssetRegistry().GetAssetsByPackageName(PackageName, OutAssetData, bIncludeOnlyOnDiskAssets);
}

bool FAngelscriptAssetRegistryBinds::GetAssetsByPath(
	FName PackagePath,
	TArray<FAssetData>& OutAssetData,
	bool bRecursive,
	bool bIncludeOnlyOnDiskAssets)
{
	return GetAssetRegistry().GetAssetsByPath(PackagePath, OutAssetData, bRecursive, bIncludeOnlyOnDiskAssets);
}

bool FAngelscriptAssetRegistryBinds::GetAssetsByClass(
	const FTopLevelAssetPath& ClassPath,
	TArray<FAssetData>& OutAssetData,
	bool bSearchSubClasses)
{
	return GetAssetRegistry().GetAssetsByClass(ClassPath, OutAssetData, bSearchSubClasses);
}

void FAngelscriptAssetRegistryBinds::GetBlueprintCDOsByParentClass(
	UClass* Class,
	TArray<UObject*>& OutAssets)
{
	if (Class == nullptr)
	{
		FAngelscriptEngine::Throw("A null Class was passed to GetBlueprintCDOsByParentClass.");
		return;
	}
	AppendBlueprintCDOs<UBlueprint>(Class, OutAssets);
}

#if WITH_EDITOR
void FAngelscriptAssetRegistryBinds::GetWidgetBlueprintCDOsByParentClass(
	UClass* Class,
	TArray<UObject*>& OutAssets)
{
	AppendBlueprintCDOs<UWidgetBlueprint>(Class, OutAssets);
}
#endif

bool FAngelscriptAssetRegistryBinds::GetAssetsByTags(
	const TArray<FName>& AssetTags,
	TArray<FAssetData>& OutAssetData)
{
	return GetAssetRegistry().GetAssetsByTags(AssetTags, OutAssetData);
}

FAssetData FAngelscriptAssetRegistryBinds::GetAssetByObjectPath(
	const FSoftObjectPath& ObjectPath,
	bool bIncludeOnlyOnDiskAssets)
{
	return GetAssetRegistry().GetAssetByObjectPath(ObjectPath, bIncludeOnlyOnDiskAssets);
}

bool FAngelscriptAssetRegistryBinds::GetAllAssets(
	TArray<FAssetData>& OutAssetData,
	bool bIncludeOnlyOnDiskAssets)
{
	return GetAssetRegistry().GetAllAssets(OutAssetData, bIncludeOnlyOnDiskAssets);
}

bool FAngelscriptAssetRegistryBinds::GetAssets(
	const FARFilter& Filter,
	TArray<FAssetData>& OutAssetData,
	bool bSkipARFilteredAssets)
{
	return GetAssetRegistry().GetAssets(Filter, OutAssetData, bSkipARFilteredAssets);
}

bool FAngelscriptAssetRegistryBinds::GetDependencies(
	FName PackageName,
	const FAssetRegistryDependencyOptions& DependencyOptions,
	TArray<FName>& OutDependencies)
{
	return GetAssetRegistry().K2_GetDependencies(PackageName, DependencyOptions, OutDependencies);
}

bool FAngelscriptAssetRegistryBinds::GetReferencers(
	FName PackageName,
	const FAssetRegistryDependencyOptions& ReferenceOptions,
	TArray<FName>& OutReferencers)
{
	return GetAssetRegistry().K2_GetReferencers(PackageName, ReferenceOptions, OutReferencers);
}

void FAngelscriptAssetRegistryBinds::GetDerivedClassNames(
	const TArray<FTopLevelAssetPath>& ClassNames,
	const TSet<FTopLevelAssetPath>& ExcludedClassNames,
	TSet<FTopLevelAssetPath>& OutDerivedClassNames)
{
	GetAssetRegistry().GetDerivedClassNames(ClassNames, ExcludedClassNames, OutDerivedClassNames);
}

bool FAngelscriptAssetRegistryBinds::GetGeneratedClassName(
	const FAssetData& AssetData,
	FTopLevelAssetPath& OutGeneratedClassName)
{
	const FAssetDataTagMapSharedView::FFindTagResult Result = AssetData.TagsAndValues.FindTag(TEXT("GeneratedClass"));
	if (!Result.IsSet())
		return false;
	const FString& GeneratedClassPath = Result.GetValue();
	OutGeneratedClassName = FTopLevelAssetPath(FPackageName::ExportTextPathToObjectPath(*GeneratedClassPath));
	return true;
}

void FAngelscriptAssetRegistryBinds::AssetCreated(UObject* NewAsset)
{
	GetAssetRegistry().AssetCreated(NewAsset);
}

void FAngelscriptAssetRegistryBinds::LoadAllBlueprintsUnderPath(
	FName PathToLoadFrom,
	FString OptionalFileIncludeRegex)
{
	const FRegexPattern InclusiveRegex(OptionalFileIncludeRegex);
	TArray<FAssetData> AllSubAssets;
	GetAssetRegistry().GetAssetsByPath(PathToLoadFrom, AllSubAssets, true);
	for (FAssetData& AssetData : AllSubAssets)
	{
		if (!OptionalFileIncludeRegex.IsEmpty())
		{
			FRegexMatcher InclusiveRegexMatcher(InclusiveRegex, AssetData.AssetName.ToString());
			if (!InclusiveRegexMatcher.FindNext())
				continue;
		}
		AssetData.GetAsset();
	}
}
