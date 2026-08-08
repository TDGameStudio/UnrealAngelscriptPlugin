#include "Bind_UAssetManager_Functions.h"

#include "Engine/AssetManager.h"

void FAngelscriptUAssetManagerBinds::ConstructPrimaryAssetType(FPrimaryAssetType* Address, const FName InName)
{
	new (Address) FPrimaryAssetType(InName);
}

void FAngelscriptUAssetManagerBinds::ConstructPrimaryAssetId(FPrimaryAssetType* Address, const FString& InString)
{
	new (Address) FPrimaryAssetId(InString);
}

void FAngelscriptUAssetManagerBinds::AppendPrimaryAssetTypeToString(void* Ptr, FString& Str)
{
	Str += static_cast<FPrimaryAssetType*>(Ptr)->ToString();
}

void FAngelscriptUAssetManagerBinds::AppendPrimaryAssetIdToString(void* Ptr, FString& Str)
{
	Str += static_cast<FPrimaryAssetId*>(Ptr)->ToString();
}

void FAngelscriptUAssetManagerBinds::LoadPrimaryAsset(
	UAssetManager* AssetManager,
	const FPrimaryAssetId& AssetToLoad,
	const TArray<FName>& LoadBundles,
	const int32 Priority,
	UObject* OptionalCallbackObject,
	const FName OptionalFinishedCallbackFunctionName,
	const FName OptionalCanceledCallbackName)
{
	TArray<FPrimaryAssetId> AssetsToLoad;
	AssetsToLoad.Add(AssetToLoad);
	LoadPrimaryAssetsInternal(
		AssetManager,
		AssetsToLoad,
		LoadBundles,
		Priority,
		OptionalCallbackObject,
		OptionalFinishedCallbackFunctionName,
		OptionalCanceledCallbackName);
}

void FAngelscriptUAssetManagerBinds::LoadPrimaryAssets(
	UAssetManager* AssetManager,
	const TArray<FPrimaryAssetId>& AssetsToLoad,
	const TArray<FName>& LoadBundles,
	const int32 Priority,
	UObject* OptionalCallbackObject,
	const FName OptionalFinishedCallbackFunctionName,
	const FName OptionalCanceledCallbackName)
{
	LoadPrimaryAssetsInternal(
		AssetManager,
		AssetsToLoad,
		LoadBundles,
		Priority,
		OptionalCallbackObject,
		OptionalFinishedCallbackFunctionName,
		OptionalCanceledCallbackName);
}

void FAngelscriptUAssetManagerBinds::LoadPrimaryAssetsInternal(
	UAssetManager* AssetManager,
	const TArray<FPrimaryAssetId>& AssetsToLoad,
	const TArray<FName>& LoadBundles,
	const int32 Priority,
	UObject* OptionalCallbackObject,
	const FName OptionalFinishedCallbackFunctionName,
	const FName OptionalCanceledCallbackName)
{
	bool bShouldLoad = true;
	for (const FPrimaryAssetId& Asset : AssetsToLoad)
	{
		if (!ensureMsgf(Asset.IsValid(), TEXT("Tried to load invalid asset!")))
		{
			bShouldLoad = false;
			break;
		}
	}

	if (!bShouldLoad)
	{
		return;
	}

	FStreamableDelegate CompleteDelegate;
	if (OptionalCallbackObject != nullptr && !OptionalFinishedCallbackFunctionName.IsNone())
	{
		CompleteDelegate.BindUFunction(OptionalCallbackObject, OptionalFinishedCallbackFunctionName);
	}

	TSharedPtr<FStreamableHandle> Handle = AssetManager->LoadPrimaryAssets(AssetsToLoad, LoadBundles, CompleteDelegate, Priority);
	if (Handle.IsValid() && OptionalCallbackObject != nullptr && !OptionalCanceledCallbackName.IsNone())
	{
		FStreamableDelegate CancelDelegate;
		CancelDelegate.BindUFunction(OptionalCallbackObject, OptionalCanceledCallbackName);
		Handle->BindCancelDelegate(CancelDelegate);
	}
}
