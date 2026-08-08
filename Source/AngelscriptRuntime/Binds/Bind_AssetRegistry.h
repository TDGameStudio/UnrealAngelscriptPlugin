#pragma once

#include "CoreMinimal.h"
#include "AssetRegistry/IAssetRegistry.h"

struct FAngelscriptAssetRegistryBinds
{
	static bool IsAssetDataInstanceOf(const FAssetData* AssetData, const UClass* BaseClass, bool bResolveClass);
	static void ConstructTopLevelAssetPathFromObject(FTopLevelAssetPath* Memory, const UObject* AssetObject);
	static void ConstructTopLevelAssetPathFromString(FTopLevelAssetPath* Memory, const FString& AssetPath);
	static void ConstructTopLevelAssetPathFromNames(FTopLevelAssetPath* Memory, const FName& PackageName, const FName& AssetName);
	static FTopLevelAssetPath* AssignTopLevelAssetPath(FTopLevelAssetPath* Self, const FString& AssetPath);
	static void AppendTopLevelAssetPathToString(void* Value, FString& OutString);

	static bool IsLoadingAssets();
	static bool HasAssets(FName PackagePath, bool bRecursive);
	static bool GetAssetsByPackageName(FName PackageName, TArray<FAssetData>& OutAssetData, bool bIncludeOnlyOnDiskAssets);
	static bool GetAssetsByPath(FName PackagePath, TArray<FAssetData>& OutAssetData, bool bRecursive, bool bIncludeOnlyOnDiskAssets);
	static bool GetAssetsByClass(const FTopLevelAssetPath& ClassPath, TArray<FAssetData>& OutAssetData, bool bSearchSubClasses);
	static void GetBlueprintCDOsByParentClass(UClass* Class, TArray<UObject*>& OutAssets);
#if WITH_EDITOR
	static void GetWidgetBlueprintCDOsByParentClass(UClass* Class, TArray<UObject*>& OutAssets);
#endif
	static bool GetAssetsByTags(const TArray<FName>& AssetTags, TArray<FAssetData>& OutAssetData);
	static FAssetData GetAssetByObjectPath(const FSoftObjectPath& ObjectPath, bool bIncludeOnlyOnDiskAssets);
	static bool GetAllAssets(TArray<FAssetData>& OutAssetData, bool bIncludeOnlyOnDiskAssets);
	static bool GetAssets(const FARFilter& Filter, TArray<FAssetData>& OutAssetData, bool bSkipARFilteredAssets);
	static bool GetDependencies(FName PackageName, const FAssetRegistryDependencyOptions& DependencyOptions, TArray<FName>& OutDependencies);
	static bool GetReferencers(FName PackageName, const FAssetRegistryDependencyOptions& ReferenceOptions, TArray<FName>& OutReferencers);
	static void GetDerivedClassNames(const TArray<FTopLevelAssetPath>& ClassNames, const TSet<FTopLevelAssetPath>& ExcludedClassNames, TSet<FTopLevelAssetPath>& OutDerivedClassNames);
	static bool GetGeneratedClassName(const FAssetData& AssetData, FTopLevelAssetPath& OutGeneratedClassName);
	static void AssetCreated(UObject* NewAsset);
	static void LoadAllBlueprintsUnderPath(FName PathToLoadFrom, FString OptionalFileIncludeRegex);
};
