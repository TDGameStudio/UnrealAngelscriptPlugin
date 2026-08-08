#include "Engine/AssetManager.h"

#include "AngelscriptBinds.h"

#include "Helper_ToString.h"

#include "Bind_UAssetManager_Functions.h"

namespace
{
	void BindPrimaryAssetToStringContributions(FAngelscriptBinds& Binds)
	{
		FToStringHelper::Register(Binds, TEXT("FPrimaryAssetType"), &FAngelscriptUAssetManagerBinds::AppendPrimaryAssetTypeToString);
		FToStringHelper::Register(Binds, TEXT("FPrimaryAssetId"), &FAngelscriptUAssetManagerBinds::AppendPrimaryAssetIdToString);
	}

	void BindAssetManagerFunctions(FAngelscriptBinds& Binds)
	{
		auto FPrimaryAssetType_ = Binds.ExistingClassForTarget("FPrimaryAssetType");
		FPrimaryAssetType_.Constructor("void f(FName InName)", &FAngelscriptUAssetManagerBinds::ConstructPrimaryAssetType, "FPrimaryAssetType", true);
		FPrimaryAssetType_.Method("FName GetName() const", METHOD_TRIVIAL(FPrimaryAssetType, GetName));
		FPrimaryAssetType_.Method("bool IsValid() const", METHOD_TRIVIAL(FPrimaryAssetType, IsValid));
		FPrimaryAssetType_.Method("bool opEquals(const FPrimaryAssetType& Other) const", METHODPR_TRIVIAL(bool, FPrimaryAssetType, operator==, (const FPrimaryAssetType&) const));

		auto FPrimaryAssetId_ = Binds.ExistingClassForTarget("FPrimaryAssetId");
		FPrimaryAssetId_.Constructor("void f(const FString& InString)", &FAngelscriptUAssetManagerBinds::ConstructPrimaryAssetId, "FPrimaryAssetId", true);
		FPrimaryAssetId_.Method("bool IsValid() const", METHOD_TRIVIAL(FPrimaryAssetId, IsValid));
		FPrimaryAssetId_.Method("bool opEquals(const FPrimaryAssetId& Other) const", METHODPR_TRIVIAL(bool, FPrimaryAssetId, operator==, (const FPrimaryAssetId&) const));

		auto UAssetManager_ = Binds.ExistingClassForTarget("UAssetManager");
		UAssetManager_.Method("FPrimaryAssetId GetPrimaryAssetIdForPath(const FSoftObjectPath& ObjectPath) const", METHODPR_TRIVIAL(FPrimaryAssetId, UAssetManager, GetPrimaryAssetIdForPath, (const FSoftObjectPath&) const));
		UAssetManager_.Method("FSoftObjectPath GetPrimaryAssetPath(const FPrimaryAssetId& PrimaryAssetId) const", METHOD_TRIVIAL(UAssetManager, GetPrimaryAssetPath));
		UAssetManager_.Method("FPrimaryAssetId GetPrimaryAssetIdForData(const FAssetData& AssetData) const", METHODPR_TRIVIAL(FPrimaryAssetId, UAssetManager, GetPrimaryAssetIdForData, (const FAssetData& AssetData) const));
		UAssetManager_.Method("int UnloadPrimaryAsset(const FPrimaryAssetId& AssetToUnload)", METHODPR_TRIVIAL(int32, UAssetManager, UnloadPrimaryAsset, (const FPrimaryAssetId& AssetToUnload)));
		UAssetManager_.Method("int UnloadPrimaryAssets(const TArray<FPrimaryAssetId>& AssetsToUnload)", METHODPR_TRIVIAL(int32, UAssetManager, UnloadPrimaryAssets, (const TArray<FPrimaryAssetId>& AssetsToUnload)));
		UAssetManager_.Method(
			"void LoadPrimaryAsset(const FPrimaryAssetId& AssetToLoad, const TArray<FName>& LoadBundles, int32 Priority = 0, UObject OptionalCallbackObject = nullptr, FName OptionalFinishedCallbackFunctionName = NAME_None, FName OptionalCanceledCallbackName = NAME_None)",
			&FAngelscriptUAssetManagerBinds::LoadPrimaryAsset);
		UAssetManager_.Method(
			"void LoadPrimaryAssets(const TArray<FPrimaryAssetId>& AssetsToLoad, const TArray<FName>& LoadBundles, int32 Priority = 0, UObject OptionalCallbackObject = nullptr, FName OptionalFinishedCallbackFunctionName = NAME_None, FName OptionalCanceledCallbackName = NAME_None)",
			&FAngelscriptUAssetManagerBinds::LoadPrimaryAssets);
	}
}

AS_FORCE_LINK const FAngelscriptBind Bind_UAssetManager_ToStringContributions(
	TEXT("UAssetManager.PrimaryAssetToStringContributions"),
	EAngelscriptBindPhase::TypeInfrastructure,
	&BindPrimaryAssetToStringContributions);

AS_FORCE_LINK const FAngelscriptBind Bind_UAssetManager(
	TEXT("UAssetManager.Functions"),
	EAngelscriptBindPhase::ManualBindings,
	&BindAssetManagerFunctions);
