#include "AngelscriptOfflineBundleWriter.h"

#include "AngelscriptOfflineContractIdentity.h"
#include "AngelscriptOfflineContractJson.h"
#include "AngelscriptOfflineContractSerializer.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"

namespace AngelscriptOfflineContract
{
	namespace
	{
		FFileRecord MakeFileRecord(
			const TCHAR* Name,
			const TArray<uint8>& Bytes,
			const int64 RecordCount)
		{
			FFileRecord Result;
			Result.Name = Name;
			Result.Sha256 = Sha256Bytes(Bytes);
			Result.RecordCount = RecordCount;
			Result.ByteCount = Bytes.Num();
			return Result;
		}

		bool SaveBytes(
			const FString& Filename,
			const TArray<uint8>& Bytes,
			FString& OutError)
		{
			if (!FFileHelper::SaveArrayToFile(Bytes, *Filename))
			{
				OutError = FString::Printf(
					TEXT("Failed to write '%s'"),
					*Filename);
				return false;
			}
			return true;
		}

		bool IsUsableOutputPath(const FString& OutputDirectory)
		{
			if (OutputDirectory.IsEmpty())
			{
				return false;
			}
			const FString Full =
				FPaths::ConvertRelativePathToFull(OutputDirectory);
			return Full != FPaths::RootDir()
				&& Full != FPaths::ProjectDir()
				&& Full != FPaths::EngineDir();
		}

		void DeleteDirectoryTree(const FString& Directory)
		{
			if (!Directory.IsEmpty())
			{
				IFileManager::Get().DeleteDirectory(
					*Directory,
					false,
				true);
			}
		}

		bool ValidateStableIds(
			const TArray<FSymbolRecord>& Symbols,
			const TArray<FAssetRecord>& Assets,
			FString& OutError)
		{
			TSet<FString> SymbolIds;
			for (const FSymbolRecord& Symbol : Symbols)
			{
				if (Symbol.StableId.IsEmpty())
				{
					OutError = TEXT(
						"Offline bundle symbols require a non-empty stable ID");
					return false;
				}
				if (SymbolIds.Contains(Symbol.StableId))
				{
					OutError = FString::Printf(
						TEXT("Duplicate symbol stable ID '%s'"),
						*Symbol.StableId);
					return false;
				}
				SymbolIds.Add(Symbol.StableId);
			}

			TSet<FString> AssetIds;
			for (const FAssetRecord& Asset : Assets)
			{
				if (Asset.StableId.IsEmpty())
				{
					OutError = TEXT(
						"Offline bundle assets require a non-empty stable ID");
					return false;
				}
				if (AssetIds.Contains(Asset.StableId))
				{
					OutError = FString::Printf(
						TEXT("Duplicate asset stable ID '%s'"),
						*Asset.StableId);
					return false;
				}
				AssetIds.Add(Asset.StableId);
			}
			return true;
		}
	}

	FBundleWriteResult FAngelscriptOfflineBundleWriter::Write(
		const FBundleWriteRequest& Request)
	{
		FBundleWriteResult Result;
		if (!IsUsableOutputPath(Request.OutputDirectory))
		{
			Result.Error = TEXT("Output directory is empty or too broad");
			return Result;
		}
		if (!Request.Manifest.SymbolScope.bComplete)
		{
			Result.Error =
				TEXT("Offline bundles require a complete symbol scope");
			return Result;
		}
		if (!ValidateStableIds(
			Request.Symbols,
			Request.Assets,
			Result.Error))
		{
			return Result;
		}

		const FString OutputDirectory =
			FPaths::ConvertRelativePathToFull(Request.OutputDirectory);
		const FString ParentDirectory =
			FPaths::GetPath(OutputDirectory);
		if (!IFileManager::Get().DirectoryExists(*ParentDirectory)
			&& !IFileManager::Get().MakeDirectory(
				*ParentDirectory,
				true))
		{
			Result.Error = FString::Printf(
				TEXT("Failed to create bundle parent '%s'"),
				*ParentDirectory);
			return Result;
		}

		const FString PublicationId =
			FGuid::NewGuid().ToString(EGuidFormats::Digits);
		const FString StagingDirectory =
			OutputDirectory + TEXT(".staging-") + PublicationId;
		const FString BackupDirectory =
			OutputDirectory + TEXT(".backup-") + PublicationId;
		DeleteDirectoryTree(StagingDirectory);
		DeleteDirectoryTree(BackupDirectory);
		if (!IFileManager::Get().MakeDirectory(
			*StagingDirectory,
			true))
		{
			Result.Error = FString::Printf(
				TEXT("Failed to create staging directory '%s'"),
				*StagingDirectory);
			return Result;
		}

		bool bKeepStaging = false;
		ON_SCOPE_EXIT
		{
			if (!bKeepStaging)
			{
				DeleteDirectoryTree(StagingDirectory);
			}
		};

		const TArray<uint8> SymbolBytes =
			SerializeSymbolRecords(Request.Symbols);
		const TArray<uint8> AssetBytes =
			SerializeAssetRecords(Request.Assets);
		Result.SymbolFile = MakeFileRecord(
			TEXT("symbols.jsonl"),
			SymbolBytes,
			Request.Symbols.Num());
		Result.AssetFile = MakeFileRecord(
			TEXT("assets.jsonl"),
			AssetBytes,
			Request.Assets.Num());

		FManifestRecord Manifest = Request.Manifest;
		Manifest.Files = {Result.AssetFile, Result.SymbolFile};
		Manifest.BundleIdentity = Sha256Utf8(FString::Printf(
			TEXT("offline-bundle-v1\n%s\n%d.%d\n%s\n%s"),
			LexToString(Manifest.BundleKind),
			Manifest.Schema.Major,
			Manifest.Schema.Minor,
			*Result.SymbolFile.Sha256,
			*Result.AssetFile.Sha256));
		Result.BundleIdentity = Manifest.BundleIdentity;

		const TArray<uint8> ManifestBytes =
			SerializeCanonicalJsonDocument(ToCanonicalJson(Manifest));
		Result.ManifestFile = MakeFileRecord(
			TEXT("manifest.json"),
			ManifestBytes,
			1);

		if (!SaveBytes(
				FPaths::Combine(
					StagingDirectory,
					Result.SymbolFile.Name),
				SymbolBytes,
				Result.Error)
			|| !SaveBytes(
				FPaths::Combine(
					StagingDirectory,
					Result.AssetFile.Name),
				AssetBytes,
				Result.Error)
			|| !SaveBytes(
				FPaths::Combine(
					StagingDirectory,
					Result.ManifestFile.Name),
				ManifestBytes,
				Result.Error))
		{
			return Result;
		}

		const bool bHadExisting =
			IFileManager::Get().DirectoryExists(*OutputDirectory);
		if (bHadExisting
			&& !IFileManager::Get().Move(
				*BackupDirectory,
				*OutputDirectory,
				false,
				true))
		{
			Result.Error = FString::Printf(
				TEXT("Failed to move existing bundle '%s' aside"),
				*OutputDirectory);
			return Result;
		}

		if (!IFileManager::Get().Move(
			*OutputDirectory,
			*StagingDirectory,
			false,
			true))
		{
			if (bHadExisting)
			{
				IFileManager::Get().Move(
					*OutputDirectory,
					*BackupDirectory,
					false,
					true);
			}
			Result.Error = FString::Printf(
				TEXT("Failed to atomically publish bundle '%s'"),
				*OutputDirectory);
			return Result;
		}
		bKeepStaging = true;

		if (bHadExisting)
		{
			DeleteDirectoryTree(BackupDirectory);
		}

		Result.bSuccess = true;
		Result.PublishedDirectory = OutputDirectory;
		return Result;
	}
}
