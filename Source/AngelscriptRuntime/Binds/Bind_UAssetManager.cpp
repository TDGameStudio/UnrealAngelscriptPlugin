#include "Bind_UAssetManager.h"

#include "Engine/AssetManager.h"

#include "AngelscriptBinds.h"

#include "Helper_ToString.h"

/**
 * Primary asset identifiers, path lookup, unload/load requests, callbacks, and formatting.
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | AngelScript usage signature                                                                          | Purpose / parameter notes                                                                                        |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FPrimaryAssetType Type(FName InName);                                                                | Constructs a primary-asset type from its registered name.                                                        |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FName Name = Type.GetName() const;                                                                   | Returns the primary-asset type name.                                                                             |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | bool bValid = Type.IsValid() const;                                                                  | Reports whether the type name is non-empty.                                                                      |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | bool bEqual = LeftType == RightType;                                                                 | Compares primary-asset type names.                                                                               |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FPrimaryAssetId Id(const FString& InString);                                                         | Parses a Type:Name primary-asset identifier.                                                                     |
 * |                                                                                                      | @param InString Serialized primary-asset identifier.                                                             |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | bool bValid = Id.IsValid() const;                                                                    | Reports whether both type and asset name are valid.                                                              |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | bool bEqual = LeftId == RightId;                                                                     | Compares primary-asset type and name.                                                                            |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FPrimaryAssetId Id = AssetManager.GetPrimaryAssetIdForPath(const FSoftObjectPath& ObjectPath) const; | Returns the primary-asset identifier registered for an object path.                                              |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FSoftObjectPath Path = AssetManager.GetPrimaryAssetPath(const FPrimaryAssetId& PrimaryAssetId)       | Returns the object path registered for a primary-asset identifier.                                               |
 * |     const;                                                                                           |                                                                                                                  |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FPrimaryAssetId Id = AssetManager.GetPrimaryAssetIdForData(const FAssetData& AssetData) const;       | Derives the primary-asset identifier represented by asset-registry data.                                         |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | int Unloaded = AssetManager.UnloadPrimaryAsset(const FPrimaryAssetId& AssetToUnload);                | Releases one primary asset and returns the number of affected handles.                                           |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | int Unloaded = AssetManager.UnloadPrimaryAssets(const TArray<FPrimaryAssetId>& AssetsToUnload);      | Releases multiple primary assets and returns the number of affected handles.                                     |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | AssetManager.LoadPrimaryAsset(const FPrimaryAssetId& AssetToLoad, const TArray<FName>& LoadBundles,  | Starts an asynchronous load for one primary asset.                                                               |
 * |     int32 Priority = 0, UObject OptionalCallbackObject = nullptr,                                    | @param AssetToLoad Primary asset identifier.                                                                     |
 * |     FName OptionalFinishedCallbackFunctionName = NAME_None,                                          | @param LoadBundles Bundle names to load.                                                                         |
 * |     FName OptionalCanceledCallbackName = NAME_None);                                                 | @param Priority Async loading priority; larger values are scheduled first.                                       |
 * |                                                                                                      | @param OptionalCallbackObject Object that owns the optional completion and cancellation UFUNCTIONs.              |
 * |                                                                                                      | @param OptionalFinishedCallbackFunctionName Zero-argument completion UFUNCTION name.                             |
 * |                                                                                                      | @param OptionalCanceledCallbackName Zero-argument cancellation UFUNCTION name.                                   |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | AssetManager.LoadPrimaryAssets(const TArray<FPrimaryAssetId>& AssetsToLoad,                          | Starts one asynchronous request for multiple primary assets.                                                     |
 * |     const TArray<FName>& LoadBundles, int32 Priority = 0, UObject OptionalCallbackObject = nullptr,  | @param AssetsToLoad Primary asset identifiers.                                                                   |
 * |     FName OptionalFinishedCallbackFunctionName = NAME_None,                                          | @param LoadBundles Bundle names to load.                                                                         |
 * |     FName OptionalCanceledCallbackName = NAME_None);                                                 | @param Priority Async loading priority; larger values are scheduled first.                                       |
 * |                                                                                                      | @param OptionalCallbackObject Object that owns the optional completion and cancellation UFUNCTIONs.              |
 * |                                                                                                      | @param OptionalFinishedCallbackFunctionName Zero-argument completion UFUNCTION name.                             |
 * |                                                                                                      | @param OptionalCanceledCallbackName Zero-argument cancellation UFUNCTION name.                                   |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FString Text = f"{Type}";                                                                            | Formats a primary-asset type name.                                                                               |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FString Text = f"{Id}";                                                                              | Formats a primary-asset identifier as Type:Name.                                                                 |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 */

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
