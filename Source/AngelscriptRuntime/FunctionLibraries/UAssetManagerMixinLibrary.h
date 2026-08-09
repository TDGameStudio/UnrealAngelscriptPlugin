#pragma once
#include "CoreMinimal.h"
#include "Engine/AssetManager.h"
#include "UAssetManagerMixinLibrary.generated.h"

UCLASS(MinimalAPI, Meta = (ScriptMixin = "UAssetManager"))
class UAssetManagerMixinLibrary : public UObject
{
	GENERATED_BODY()

public:
	/** Gets the FAssetData for a primary asset with the specified type/name, will only work for once that have been scanned for already. Returns true if it found a valid data */
	UFUNCTION(BlueprintCallable)
	static bool GetPrimaryAssetData(UAssetManager* AssetManager, const FPrimaryAssetId& PrimaryAssetId, FAssetData& AssetData)
	{
		if (AssetManager == nullptr)
		{
			AssetData = FAssetData();
			return false;
		}

		return AssetManager->GetPrimaryAssetData(PrimaryAssetId, AssetData);
	}

	/** Gets list of all FAssetData for a primary asset type, returns true if any were found */
	UFUNCTION(BlueprintCallable)
	static bool GetPrimaryAssetDataList(UAssetManager* AssetManager, FPrimaryAssetType PrimaryAssetType, TArray<FAssetData>& AssetDataList)
	{
		if (AssetManager == nullptr)
		{
			AssetDataList.Reset();
			return false;
		}

		return AssetManager->GetPrimaryAssetDataList(PrimaryAssetType, AssetDataList);
	}

	/** Gets the in-memory UObject for a primary asset id, returning nullptr if it's not in memory. Will return blueprint class for blueprint assets. This works even if the asset wasn't loaded explicitly */
	UFUNCTION(BlueprintCallable)
	static UObject* GetPrimaryAssetObject(UAssetManager* AssetManager, const FPrimaryAssetId& PrimaryAssetId)
	{
		if (AssetManager == nullptr)
		{
			return nullptr;
		}

		return AssetManager->GetPrimaryAssetObject(PrimaryAssetId);
	}

	/** Sees if the passed in object is a registered primary asset, if so return it. Returns invalid Identifier if not found */
	UFUNCTION(BlueprintCallable)
	static FPrimaryAssetId GetPrimaryAssetIdForObject(UAssetManager* AssetManager, UObject* Object)
	{
		if (AssetManager == nullptr)
		{
			return FPrimaryAssetId();
		}

		return AssetManager->GetPrimaryAssetIdForObject(Object);
	}

	/** Gets list of all FPrimaryAssetId for a primary asset type, returns true if any were found */
	UFUNCTION(BlueprintCallable)
	static bool GetPrimaryAssetIdList(UAssetManager* AssetManager, FPrimaryAssetType PrimaryAssetType, TArray<FPrimaryAssetId>& PrimaryAssetIdList)
	{
		if (AssetManager == nullptr)
		{
			PrimaryAssetIdList.Reset();
			return false;
		}

		return AssetManager->GetPrimaryAssetIdList(PrimaryAssetType, PrimaryAssetIdList);
	}

	/** Register a function to call when all types are scanned at startup, if this has already happened call immediately */
	UFUNCTION(BlueprintCallable, Meta = (DelegateObjectParam = "Object", DelegateFunctionParam = "FunctionName", DelegateBindType = "FSimpleDelegate"))
	static void CallOrRegister_OnCompletedInitialScan(UAssetManager* AssetManager, UObject* Object, const FName& FunctionName)
	{
		if (AssetManager == nullptr || Object == nullptr || FunctionName.IsNone() || Object->FindFunction(FunctionName) == nullptr)
		{
			return;
		}

		AssetManager->CallOrRegister_OnCompletedInitialScan(FSimpleDelegate::CreateUFunction(Object, FunctionName));
	}

	/** Scans a list of paths and reads asset data for all primary assets of a specific type.
	 * If done in the editor it will load the data off disk, in cooked games it will load out of the asset registry cache */
	UFUNCTION(BlueprintCallable)
	static int32 ScanPathForPrimaryAssets(UAssetManager* AssetManager, FPrimaryAssetType PrimaryAssetType, const FString& Path, UClass* BaseClass, bool bHasBlueprintClasses, bool bIsEditorOnly = false, bool bForceSynchronousScan = true)
	{
		if (AssetManager == nullptr)
		{
			return 0;
		}

		return AssetManager->ScanPathForPrimaryAssets(PrimaryAssetType, Path, BaseClass, bHasBlueprintClasses, bIsEditorOnly, bForceSynchronousScan);
	}
};
