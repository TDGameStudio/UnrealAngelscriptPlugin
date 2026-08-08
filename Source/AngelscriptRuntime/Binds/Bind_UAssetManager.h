#pragma once

#include "CoreMinimal.h"

class UAssetManager;
struct FPrimaryAssetId;
struct FPrimaryAssetType;

struct FAngelscriptUAssetManagerBinds
{
	static void ConstructPrimaryAssetType(FPrimaryAssetType* Address, FName InName);
	static void ConstructPrimaryAssetId(FPrimaryAssetType* Address, const FString& InString);
	static void AppendPrimaryAssetTypeToString(void* Ptr, FString& Str);
	static void AppendPrimaryAssetIdToString(void* Ptr, FString& Str);
	static void LoadPrimaryAsset(
		UAssetManager* AssetManager,
		const FPrimaryAssetId& AssetToLoad,
		const TArray<FName>& LoadBundles,
		int32 Priority,
		UObject* OptionalCallbackObject,
		FName OptionalFinishedCallbackFunctionName,
		FName OptionalCanceledCallbackName);
	static void LoadPrimaryAssets(
		UAssetManager* AssetManager,
		const TArray<FPrimaryAssetId>& AssetsToLoad,
		const TArray<FName>& LoadBundles,
		int32 Priority,
		UObject* OptionalCallbackObject,
		FName OptionalFinishedCallbackFunctionName,
		FName OptionalCanceledCallbackName);

private:
	static void LoadPrimaryAssetsInternal(
		UAssetManager* AssetManager,
		const TArray<FPrimaryAssetId>& AssetsToLoad,
		const TArray<FName>& LoadBundles,
		int32 Priority,
		UObject* OptionalCallbackObject,
		FName OptionalFinishedCallbackFunctionName,
		FName OptionalCanceledCallbackName);
};
