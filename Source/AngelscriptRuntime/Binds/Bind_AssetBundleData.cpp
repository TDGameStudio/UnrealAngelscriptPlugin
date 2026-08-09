#include "AngelscriptBinds.h"

#include "AssetRegistry/AssetBundleData.h"

struct FAngelscriptAssetBundleDataBinds
{
	static void ConstructEntryDefault(FAssetBundleEntry* Address)
	{
		new (Address) FAssetBundleEntry();
	}

	static void ConstructEntry(FAssetBundleEntry* Address, FName BundleName)
	{
		new (Address) FAssetBundleEntry(BundleName);
	}

	static void ConstructData(FAssetBundleData* Address)
	{
		new (Address) FAssetBundleData();
	}

	static int32 GetNumBundles(const FAssetBundleData* Data)
	{
		return Data->Bundles.Num();
	}

	static void SetBundleAssets(FAssetBundleData* Data, FName BundleName, const TArray<FTopLevelAssetPath>& AssetPaths)
	{
		Data->SetBundleAssets(BundleName, TArray<FTopLevelAssetPath>(AssetPaths));
	}

	static bool FindEntry(FAssetBundleData* Data, FName BundleName, FAssetBundleEntry& OutEntry)
	{
		if (const FAssetBundleEntry* Entry = Data->FindEntry(BundleName))
		{
			OutEntry = *Entry;
			return true;
		}

		return false;
	}
};

/**
 * Asset bundle value binding surface.
 * +--------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------+
 * | AngelScript usage signature                                                                      | Purpose / parameter notes                                                        |
 * +--------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------+
 * | FAssetBundleEntry Entry(FName BundleName);                                                       | Constructs a named bundle entry.                                                 |
 * +--------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------+
 * | void FAssetBundleData.AddBundleAsset(FName BundleName, const FTopLevelAssetPath& AssetPath);    | Adds one top-level asset path to BundleName.                                    |
 * +--------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------+
 * | void FAssetBundleData.SetBundleAssets(FName BundleName, const TArray<FTopLevelAssetPath>& Paths);| Replaces BundleName paths. @param Paths Paths copied into bundle data.          |
 * +--------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------+
 * | bool FAssetBundleData.FindEntry(FName BundleName, FAssetBundleEntry&out OutEntry) const;         | Copies a matching entry. @param OutEntry Receives an independent safe copy.     |
 * +--------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------+
 */
AS_FORCE_LINK const FAngelscriptBind Bind_AssetBundleData_Type(
	TEXT("AssetBundleData.Type"),
	EAngelscriptBindPhase::TypeDeclarations,
	[](FAngelscriptBinds& Binds)
	{
		FBindFlags Flags;
		Binds.ValueClassForTarget<FAssetBundleEntry>("FAssetBundleEntry", Flags);
		Binds.ValueClassForTarget<FAssetBundleData>("FAssetBundleData", Flags);
	});

AS_FORCE_LINK const FAngelscriptBind Bind_AssetBundleData(
	TEXT("AssetBundleData"),
	EAngelscriptBindPhase::ExplicitBindings,
	[](FAngelscriptBinds& Binds)
	{
		auto Entry = Binds.ExistingClassForTarget("FAssetBundleEntry");
		Entry.Constructor("void f()", &FAngelscriptAssetBundleDataBinds::ConstructEntryDefault, "FAssetBundleEntry", true);
		Entry.Constructor("void f(FName BundleName)", &FAngelscriptAssetBundleDataBinds::ConstructEntry, "FAssetBundleEntry", true);
		Entry.Property("FName BundleName", &FAssetBundleEntry::BundleName);
		Entry.Property("TArray<FTopLevelAssetPath> AssetPaths", &FAssetBundleEntry::AssetPaths);
		Entry.Method("bool IsValid() const", METHOD_TRIVIAL(FAssetBundleEntry, IsValid));
		Entry.Method("bool opEquals(const FAssetBundleEntry& Other) const", METHODPR_TRIVIAL(bool, FAssetBundleEntry, operator==, (const FAssetBundleEntry&) const));

		auto Data = Binds.ExistingClassForTarget("FAssetBundleData");
		Data.Constructor("void f()", &FAngelscriptAssetBundleDataBinds::ConstructData, "FAssetBundleData", true);
		Data.Method("int32 GetNumBundles() const", &FAngelscriptAssetBundleDataBinds::GetNumBundles);
		Data.Method("void AddBundleAsset(FName BundleName, const FTopLevelAssetPath& AssetPath)", METHOD_TRIVIAL(FAssetBundleData, AddBundleAsset));
		Data.Method("void AddBundleAssets(FName BundleName, const TArray<FTopLevelAssetPath>& AssetPaths)", METHOD_TRIVIAL(FAssetBundleData, AddBundleAssets));
		Data.Method("void SetBundleAssets(FName BundleName, const TArray<FTopLevelAssetPath>& AssetPaths)", &FAngelscriptAssetBundleDataBinds::SetBundleAssets);
		Data.Method("bool FindEntry(FName BundleName, FAssetBundleEntry&out OutEntry) const", &FAngelscriptAssetBundleDataBinds::FindEntry);
		Data.Method("void Reset()", METHOD_TRIVIAL(FAssetBundleData, Reset));
		Data.Method("FString ToDebugString() const", METHOD_TRIVIAL(FAssetBundleData, ToDebugString));
	});
