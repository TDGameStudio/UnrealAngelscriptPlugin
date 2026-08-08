#include "AngelscriptBinds.h"
#include "Bind_AssetRegistry_Functions.h"
#include "Helper_ToString.h"

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
