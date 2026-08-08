#include "Bind_AssetRegistry.h"

#include "AngelscriptBinds.h"
#include "Helper_ToString.h"

/**
 * Asset data, top-level paths, registry queries, and formatter contribution.
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | AngelScript usage signature                                                                          | Purpose / parameter notes                                                                                        |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FSoftObjectPath AssetData.GetSoftObjectPath() const;                                                 | Returns the asset object path.                                                                                   |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FString AssetData.GetObjectPathString() const;                                                       | Returns the object path as text.                                                                                 |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | bool AssetData.IsInstanceOf(const UClass BaseClass, bool bResolveClass = false) const;               | Reports whether the asset class derives from BaseClass.                                                          |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FTopLevelAssetPath Path(const UObject AssetObject);                                                  | Constructs a top-level path from an object.                                                                      |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FTopLevelAssetPath Path(const FString& AssetPath);                                                   | Parses a top-level asset path.                                                                                   |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FTopLevelAssetPath Path(const FName& PackageName, const FName& AssetName);                           | Constructs from package and asset names.                                                                         |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | bool Path.IsValid() const;                                                                           | Reports whether both path names are valid.                                                                       |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | bool Path.IsNull() const;                                                                            | Reports whether the path is null.                                                                                |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | void Path.Reset();                                                                                   | Clears the path.                                                                                                 |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | bool bEqual = Left == Right;                                                                         | Compares top-level asset paths.                                                                                  |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | Path = AssetPath;                                                                                    | Assigns a parsed string path.                                                                                    |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | bool AssetRegistry::IsLoadingAssets();                                                               | Reports whether initial asset discovery is active.                                                               |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | bool AssetRegistry::HasAssets(const FName PackagePath, const bool bRecursive = false);               | Reports whether a package path contains assets.                                                                  |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | bool AssetRegistry::GetAssetsByPackageName(FName PackageName,                                        | Finds assets in a package.                                                                                       |
 * |     TArray<FAssetData>& OutAssetData,                                                                | @param OutAssetData Receives matching assets.                                                                    |
 * |     bool bIncludeOnlyOnDiskAssets = false);                                                          |                                                                                                                  |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | bool AssetRegistry::GetAssetsByPath(FName PackagePath,                                               | Finds assets below a package path.                                                                               |
 * |     TArray<FAssetData>& OutAssetData,                                                                | @param OutAssetData Receives matching assets.                                                                    |
 * |     bool bRecursive = false,                                                                         |                                                                                                                  |
 * |     bool bIncludeOnlyOnDiskAssets = false);                                                          |                                                                                                                  |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | bool AssetRegistry::GetAssetsByClass(const FTopLevelAssetPath& ClassPath,                            | Finds assets by class.                                                                                           |
 * |     TArray<FAssetData>& OutAssetData,                                                                | @param OutAssetData Receives matching assets.                                                                    |
 * |     bool bSearchSubClasses = false);                                                                 |                                                                                                                  |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | void AssetRegistry::GetBlueprintCDOsByParentClass(UClass Class, TArray<UObject>& OutAssets);         | Finds Blueprint CDOs derived from a class.                                                                       |
 * |                                                                                                      | @param OutAssets Receives matching CDOs.                                                                         |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | void AssetRegistry::GetWidgetBlueprintCDOsByParentClass(UClass Class, TArray<UObject>& OutAssets);   | Editor-only widget Blueprint variant.                                                                            |
 * |                                                                                                      | @param OutAssets Receives matching CDOs.                                                                         |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | bool AssetRegistry::GetAssetsByTags(const TArray<FName>& AssetTags,                                  | Finds assets containing requested tags.                                                                          |
 * |     TArray<FAssetData>& OutAssetData);                                                               | @param OutAssetData Receives matching assets.                                                                    |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FAssetData AssetRegistry::GetAssetByObjectPath(const FSoftObjectPath& ObjectPath,                    | Returns asset data for an object path.                                                                           |
 * |     bool bIncludeOnlyOnDiskAssets = false);                                                          |                                                                                                                  |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | bool AssetRegistry::GetAllAssets(TArray<FAssetData>& OutAssetData,                                   | Returns every known asset.                                                                                       |
 * |     bool bIncludeOnlyOnDiskAssets = false);                                                          | @param OutAssetData Receives asset data.                                                                         |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | bool AssetRegistry::GetAssets(const FARFilter& Filter,                                               | Runs an asset-registry filter.                                                                                   |
 * |     TArray<FAssetData>& OutAssetData,                                                                | @param OutAssetData Receives matching assets.                                                                    |
 * |     bool bSkipARFilteredAssets = true);                                                              |                                                                                                                  |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | bool AssetRegistry::GetDependencies(FName PackageName,                                               | Finds package dependencies.                                                                                      |
 * |     const FAssetRegistryDependencyOptions& DependencyOptions,                                        | @param OutDependencies Receives dependency package names.                                                        |
 * |     TArray<FName>& OutDependencies);                                                                 |                                                                                                                  |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | bool AssetRegistry::GetReferencers(FName PackageName,                                                | Finds package referencers.                                                                                       |
 * |     const FAssetRegistryDependencyOptions& ReferenceOptions,                                         | @param OutReferencers Receives referencing package names.                                                        |
 * |     TArray<FName>& OutReferencers);                                                                  |                                                                                                                  |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | void AssetRegistry::GetDerivedClassNames(const TArray<FTopLevelAssetPath>& ClassNames,               | Expands derived class paths.                                                                                     |
 * |     const TSet<FTopLevelAssetPath>& ExcludedClassNames,                                              | @param OutDerivedClassNames Receives derived classes.                                                            |
 * |     TSet<FTopLevelAssetPath>&OutDerivedClassNames);                                                  |                                                                                                                  |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | bool AssetRegistry::GetGeneratedClassName(const FAssetData& AssetData,                               | Reads a Blueprint generated-class path.                                                                          |
 * |     FTopLevelAssetPath& OutGeneratedClassName);                                                      | @param OutGeneratedClassName Receives the generated class path.                                                  |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | void AssetRegistry::AssetCreated(UObject NewAsset);                                                  | Notifies the registry that an asset was created.                                                                 |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | void AssetRegistry::LoadAllBlueprintsUnderPath(FName PathToLoadFrom,                                 | Loads Blueprint assets below a path, optionally filtered by filename regex.                                      |
 * |     FString OptionalFileIncludeRegex = "");                                                          |                                                                                                                  |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FString Text = f"{Path}";                                                                            | Formats the top-level asset path through the shared formatter contribution.                                      |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 */

namespace
{
	void BindAssetRegistryToStringContribution(FAngelscriptBinds& Binds)
	{
		FToStringHelper::Register(
			Binds,
			TEXT("FTopLevelAssetPath"),
			&FAngelscriptAssetRegistryBinds::AppendTopLevelAssetPathToString);
	}

	void BindAssetRegistry(FAngelscriptBinds& Binds)
	{
		auto AssetDataType = Binds.ExistingClassForTarget("FAssetData");
		AssetDataType.Method("FSoftObjectPath GetSoftObjectPath() const", METHOD_TRIVIAL(FAssetData, GetSoftObjectPath));
		AssetDataType.Method("FString GetObjectPathString() const", METHOD_TRIVIAL(FAssetData, GetObjectPathString));
		AssetDataType.Method(
			"bool IsInstanceOf(const UClass BaseClass, bool bResolveClass = false) const",
			&FAngelscriptAssetRegistryBinds::IsAssetDataInstanceOf);

		auto TopLevelAssetPathType = Binds.ExistingClassForTarget("FTopLevelAssetPath");
		TopLevelAssetPathType.Constructor(
			"void f(const UObject AssetObject)",
			&FAngelscriptAssetRegistryBinds::ConstructTopLevelAssetPathFromObject);
		TopLevelAssetPathType.Constructor(
			"void f(const FString& AssetPath)",
			&FAngelscriptAssetRegistryBinds::ConstructTopLevelAssetPathFromString);
		TopLevelAssetPathType.Constructor(
			"void f(const FName& PackageName, const FName& AssetName)",
			&FAngelscriptAssetRegistryBinds::ConstructTopLevelAssetPathFromNames);
		TopLevelAssetPathType.Method("bool IsValid() const", &FTopLevelAssetPath::IsValid);
		TopLevelAssetPathType.Method("bool IsNull() const", &FTopLevelAssetPath::IsNull);
		TopLevelAssetPathType.Method("void Reset()", &FTopLevelAssetPath::Reset);
		TopLevelAssetPathType.Method("bool opEquals(const FTopLevelAssetPath& Other) const", &FTopLevelAssetPath::operator==);
		TopLevelAssetPathType.Method(
			"FTopLevelAssetPath& opAssign(const FString& AssetPath)",
			&FAngelscriptAssetRegistryBinds::AssignTopLevelAssetPath);

		FAngelscriptBinds::FNamespace Namespace(Binds.GetTargetEngine(), "AssetRegistry");
		Binds.BindGlobalFunctionForTarget("bool IsLoadingAssets()", &FAngelscriptAssetRegistryBinds::IsLoadingAssets);
		Binds.BindGlobalFunctionForTarget(
			"bool HasAssets(const FName PackagePath, const bool bRecursive = false)",
			&FAngelscriptAssetRegistryBinds::HasAssets);
		Binds.BindGlobalFunctionForTarget(
			"bool GetAssetsByPackageName(FName PackageName, TArray<FAssetData>& OutAssetData, bool bIncludeOnlyOnDiskAssets = false)",
			&FAngelscriptAssetRegistryBinds::GetAssetsByPackageName);
		Binds.BindGlobalFunctionForTarget(
			"bool GetAssetsByPath(FName PackagePath, TArray<FAssetData>& OutAssetData, bool bRecursive = false, bool bIncludeOnlyOnDiskAssets = false)",
			&FAngelscriptAssetRegistryBinds::GetAssetsByPath);
		Binds.BindGlobalFunctionForTarget(
			"bool GetAssetsByClass(const FTopLevelAssetPath& ClassPath, TArray<FAssetData>& OutAssetData, bool bSearchSubClasses = false)",
			&FAngelscriptAssetRegistryBinds::GetAssetsByClass);
		Binds.BindGlobalFunctionForTarget(
			"void GetBlueprintCDOsByParentClass(UClass Class, TArray<UObject>& OutAssets)",
			&FAngelscriptAssetRegistryBinds::GetBlueprintCDOsByParentClass);
#if WITH_EDITOR
		Binds.BindGlobalFunctionForTarget(
			"void GetWidgetBlueprintCDOsByParentClass(UClass Class, TArray<UObject>& OutAssets)",
			&FAngelscriptAssetRegistryBinds::GetWidgetBlueprintCDOsByParentClass);
#endif
		Binds.BindGlobalFunctionForTarget(
			"bool GetAssetsByTags(const TArray<FName>& AssetTags, TArray<FAssetData>& OutAssetData)",
			&FAngelscriptAssetRegistryBinds::GetAssetsByTags);
		Binds.BindGlobalFunctionForTarget(
			"FAssetData GetAssetByObjectPath(const FSoftObjectPath& ObjectPath, bool bIncludeOnlyOnDiskAssets = false)",
			&FAngelscriptAssetRegistryBinds::GetAssetByObjectPath);
		Binds.BindGlobalFunctionForTarget(
			"bool GetAllAssets(TArray<FAssetData>& OutAssetData, bool bIncludeOnlyOnDiskAssets = false)",
			&FAngelscriptAssetRegistryBinds::GetAllAssets);
		Binds.BindGlobalFunctionForTarget(
			"bool GetAssets(const FARFilter& Filter, TArray<FAssetData>& OutAssetData, bool bSkipARFilteredAssets = true)",
			&FAngelscriptAssetRegistryBinds::GetAssets);
		Binds.BindGlobalFunctionForTarget(
			"bool GetDependencies(FName PackageName, const FAssetRegistryDependencyOptions& DependencyOptions, TArray<FName>& OutDependencies)",
			&FAngelscriptAssetRegistryBinds::GetDependencies);
		Binds.BindGlobalFunctionForTarget(
			"bool GetReferencers(FName PackageName, const FAssetRegistryDependencyOptions& ReferenceOptions, TArray<FName>& OutReferencers)",
			&FAngelscriptAssetRegistryBinds::GetReferencers);
		Binds.BindGlobalFunctionForTarget(
			"void GetDerivedClassNames(const TArray<FTopLevelAssetPath>& ClassNames, const TSet<FTopLevelAssetPath>& ExcludedClassNames, TSet<FTopLevelAssetPath>&OutDerivedClassNames)",
			&FAngelscriptAssetRegistryBinds::GetDerivedClassNames);
		Binds.BindGlobalFunctionForTarget(
			"bool GetGeneratedClassName(const FAssetData& AssetData, FTopLevelAssetPath& OutGeneratedClassName)",
			&FAngelscriptAssetRegistryBinds::GetGeneratedClassName);
		Binds.BindGlobalFunctionForTarget("void AssetCreated(UObject NewAsset)", &FAngelscriptAssetRegistryBinds::AssetCreated);
		Binds.BindGlobalFunctionForTarget(
			"void LoadAllBlueprintsUnderPath(FName PathToLoadFrom, FString OptionalFileIncludeRegex = \"\")",
			&FAngelscriptAssetRegistryBinds::LoadAllBlueprintsUnderPath);
	}
}

AS_FORCE_LINK const FAngelscriptBind Bind_AssetRegistry_ToStringContribution(
	TEXT("AssetRegistry.TopLevelAssetPathToStringContribution"),
	EAngelscriptBindPhase::TypeInfrastructure,
	&BindAssetRegistryToStringContribution);

AS_FORCE_LINK const FAngelscriptBind Bind_AssetRegistry(
	TEXT("AssetRegistry.Manual"),
	EAngelscriptBindPhase::ManualBindings,
	&BindAssetRegistry);
