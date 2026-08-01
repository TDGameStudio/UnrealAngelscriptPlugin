#include "AngelscriptOfflineAssetExporter.h"

#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "AssetRegistry/ARFilter.h"
#include "Dump/AngelscriptOfflineContractIdentity.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/App.h"
#include "Misc/PackageName.h"
#include "UObject/Package.h"

namespace AngelscriptEditor::Offline
{
	namespace
	{
		bool IsWithinRoot(
			const FString& Path,
			const FString& Root)
		{
			const FString NormalizedRoot =
				NormalizeAssetObjectPath(Root);
			return Path == NormalizedRoot
				|| Path.StartsWith(
					NormalizedRoot + TEXT("/"),
					ESearchCase::CaseSensitive);
		}

		FString GetPackagePath(const FString& ObjectPath)
		{
			const FString PackageName =
				FPackageName::ObjectPathToPackageName(ObjectPath);
			return FPackageName::GetLongPackagePath(PackageName);
		}

		FString GetMountPoint(const FString& PackagePath)
		{
			if (!PackagePath.StartsWith(TEXT("/")))
			{
				return FString();
			}
			int32 Separator = INDEX_NONE;
			if (PackagePath.FindChar(
				TEXT('/'),
				Separator)
				&& Separator == 0)
			{
				Separator = PackagePath.Find(
					TEXT("/"),
					ESearchCase::CaseSensitive,
					ESearchDir::FromStart,
					1);
			}
			return Separator == INDEX_NONE
				? PackagePath
				: PackagePath.Left(Separator);
		}

		void ResolvePluginOrigin(
			const FString& MountPoint,
			FString& InOutPlugin)
		{
			if (!InOutPlugin.IsEmpty()
				|| MountPoint == TEXT("/Game")
				|| MountPoint == TEXT("/Engine")
				|| MountPoint == TEXT("/Script"))
			{
				return;
			}
			for (const TSharedRef<IPlugin>& Plugin :
				IPluginManager::Get().GetEnabledPlugins())
			{
				FString Mounted =
					NormalizeAssetObjectPath(
						Plugin->GetMountedAssetPath());
				while (Mounted.EndsWith(TEXT("/")))
				{
					Mounted.LeftChopInline(1);
				}
				if (Mounted == MountPoint)
				{
					InOutPlugin = Plugin->GetName();
					return;
				}
			}
		}

		bool IsIncluded(
			const FString& ObjectPath,
			const FOfflineAssetExportRequest& Request)
		{
			bool bIncluded = Request.Roots.IsEmpty();
			for (const FString& Root : Request.Roots)
			{
				bIncluded |= IsWithinRoot(ObjectPath, Root);
			}
			if (!bIncluded)
			{
				return false;
			}
			for (const FString& Root : Request.ExcludedRoots)
			{
				if (IsWithinRoot(ObjectPath, Root))
				{
					return false;
				}
			}
			return true;
		}

		FString GetTag(
			const FAssetData& Asset,
			const FName Name)
		{
			FString Value;
			Asset.GetTagValue(Name, Value);
			return Value;
		}

		FOfflineAssetSourceRecord ConvertAsset(
			const FAssetData& Asset)
		{
			FOfflineAssetSourceRecord Result;
			Result.PackagePath = Asset.PackagePath.ToString();
			Result.ObjectPath = Asset.GetObjectPathString();
			Result.AssetClassPath =
				Asset.AssetClassPath.ToString();
			Result.Availability =
				(Asset.PackageFlags & PKG_EditorOnly) != 0
					? AngelscriptOfflineContract::EAvailability::EditorOnly
					: AngelscriptOfflineContract::EAvailability::Available;

			Result.GeneratedClassPath = GetTag(
				Asset,
				TEXT("GeneratedClass"));
			Result.BaseClassPath = GetTag(
				Asset,
				TEXT("ParentClass"));
			if (Result.BaseClassPath.IsEmpty())
			{
				Result.BaseClassPath = GetTag(
					Asset,
					TEXT("NativeParentClass"));
			}
			const FString Destination = GetTag(
				Asset,
				TEXT("DestinationObject"));
			if (!Destination.IsEmpty())
			{
				Result.RedirectSource = Result.ObjectPath;
				Result.RedirectTarget = Destination;
			}

			// This allowlist is deliberately type-check-only. Asset payload,
			// source data, addresses, and arbitrary searchable tags never enter
			// the offline contract.
			if (!Result.GeneratedClassPath.IsEmpty())
			{
				Result.TypeCheckTags.Add(
					TEXT("generatedClass"),
					Result.GeneratedClassPath);
			}
			if (!Result.BaseClassPath.IsEmpty())
			{
				Result.TypeCheckTags.Add(
					TEXT("baseClass"),
					Result.BaseClassPath);
			}
			return Result;
		}
	}

	FString NormalizeAssetObjectPath(const FStringView Value)
	{
		FString Result(Value);
		Result.TrimStartAndEndInline();
		Result.ReplaceInline(
			TEXT("\\"),
			TEXT("/"),
			ESearchCase::CaseSensitive);
		Result = FPackageName::ExportTextPathToObjectPath(Result);
		while (Result.Contains(TEXT("//")))
		{
			Result.ReplaceInline(TEXT("//"), TEXT("/"));
		}
		while (Result.EndsWith(TEXT("/")) && Result.Len() > 1)
		{
			Result.LeftChopInline(1);
		}
		return Result;
	}

	FOfflineAssetExportResult ExportAssetRecords(
		const TArray<FOfflineAssetSourceRecord>& Source,
		const FOfflineAssetExportRequest& Request)
	{
		using namespace AngelscriptOfflineContract;

		FOfflineAssetExportResult Result;
		Result.Scope.State = Request.bRegistryComplete
			? TEXT("asset-registry-complete")
			: TEXT("asset-registry-incomplete");
		for (const FString& Root : Request.Roots)
		{
			Result.Scope.Included.Add(
				NormalizeAssetObjectPath(Root));
		}
		for (const FString& Root : Request.ExcludedRoots)
		{
			Result.Scope.Excluded.Add(
				NormalizeAssetObjectPath(Root));
		}

		TSet<FString> StableIds;
		for (const FOfflineAssetSourceRecord& Input : Source)
		{
			FAssetRecord Asset;
			Asset.ObjectPath =
				NormalizeAssetObjectPath(Input.ObjectPath);
			if (Asset.ObjectPath.IsEmpty()
				|| !Asset.ObjectPath.StartsWith(TEXT("/"))
				|| !Asset.ObjectPath.Contains(TEXT(".")))
			{
				Result.Scope.Skipped.Add(
					Input.ObjectPath + TEXT(":invalid-object-path"));
				continue;
			}
			if (!IsIncluded(Asset.ObjectPath, Request))
			{
				continue;
			}

			Asset.PackagePath =
				NormalizeAssetObjectPath(Input.PackagePath);
			if (Asset.PackagePath.IsEmpty())
			{
				Asset.PackagePath =
					GetPackagePath(Asset.ObjectPath);
			}
			Asset.GeneratedClassPath =
				NormalizeAssetObjectPath(
					Input.GeneratedClassPath);
			Asset.AssetClassPath =
				NormalizeAssetObjectPath(
					Input.AssetClassPath);
			Asset.BaseClassPath =
				NormalizeAssetObjectPath(
					Input.BaseClassPath);
			Asset.RedirectSource =
				NormalizeAssetObjectPath(
					Input.RedirectSource);
			Asset.RedirectTarget =
				NormalizeAssetObjectPath(
					Input.RedirectTarget);
			Asset.MountPoint = GetMountPoint(Asset.PackagePath);
			Asset.OriginModule = Input.OriginModule;
			Asset.OriginPlugin = Input.OriginPlugin;
			if (Asset.OriginModule.IsEmpty()
				&& Asset.MountPoint == TEXT("/Script"))
			{
				const FString ScriptPrefix = TEXT("/Script/");
				const FString Remainder =
					Asset.PackagePath.RightChop(ScriptPrefix.Len());
				FString Module;
				if (Remainder.Split(
					TEXT("/"),
					&Module,
					nullptr,
					ESearchCase::CaseSensitive))
				{
					Asset.OriginModule = Module;
				}
				else
				{
					Asset.OriginModule = Remainder;
				}
			}
			else if (Asset.OriginModule.IsEmpty()
				&& Asset.MountPoint == TEXT("/Engine"))
			{
				Asset.OriginModule = TEXT("Engine");
			}
			else if (Asset.OriginModule.IsEmpty()
				&& Asset.MountPoint == TEXT("/Game"))
			{
				Asset.OriginModule = FApp::GetProjectName();
			}
			ResolvePluginOrigin(
				Asset.MountPoint,
				Asset.OriginPlugin);
			Asset.Availability = Input.Availability;
			Asset.StableId =
				AngelscriptOfflineContract::MakeStableAssetId(
					Asset.ObjectPath);

			for (const TPair<FString, FString>& Tag :
				Input.TypeCheckTags)
			{
				if (Tag.Key == TEXT("generatedClass")
					|| Tag.Key == TEXT("baseClass")
					|| Tag.Key == TEXT("assetClass"))
				{
					Asset.TypeCheckTags.Add(
						Tag.Key,
						NormalizeAssetObjectPath(Tag.Value));
				}
			}

			if (StableIds.Contains(Asset.StableId))
			{
				Result.Error = FString::Printf(
					TEXT("Duplicate normalized asset '%s'"),
					*Asset.ObjectPath);
				return Result;
			}
			StableIds.Add(Asset.StableId);
			Result.Assets.Add(MoveTemp(Asset));
		}

		Result.Assets.Sort([](
			const FAssetRecord& Left,
			const FAssetRecord& Right)
		{
			return Left.StableId < Right.StableId;
		});
		Result.Scope.Included.Sort();
		Result.Scope.Excluded.Sort();
		Result.Scope.Skipped.Sort();
		Result.Scope.bComplete =
			Request.bRegistryComplete
			&& Result.Scope.Skipped.IsEmpty();
		Result.bSuccess = true;
		return Result;
	}

	FOfflineAssetExportResult ExportAssetRegistry(
		IAssetRegistry& AssetRegistry,
		const FOfflineAssetExportRequest& Request)
	{
		FARFilter Filter;
		Filter.bRecursivePaths = true;
		for (const FString& Root : Request.Roots)
		{
			Filter.PackagePaths.Add(
				FName(*NormalizeAssetObjectPath(Root)));
		}

		TArray<FAssetData> Assets;
		if (!AssetRegistry.GetAssets(Filter, Assets, true))
		{
			FOfflineAssetExportResult Result;
			Result.Error = TEXT("Asset registry query failed");
			return Result;
		}
		Assets.Sort([](
			const FAssetData& Left,
			const FAssetData& Right)
		{
			return Left.GetObjectPathString()
				< Right.GetObjectPathString();
		});

		TArray<FOfflineAssetSourceRecord> Source;
		Source.Reserve(Assets.Num());
		for (const FAssetData& Asset : Assets)
		{
			Source.Add(ConvertAsset(Asset));
		}

		FOfflineAssetExportRequest Effective = Request;
		Effective.bRegistryComplete =
			Request.bRegistryComplete
			&& !AssetRegistry.IsLoadingAssets();
		return ExportAssetRecords(Source, Effective);
	}
}
