#include "AngelscriptOfflineExportCommand.h"

#include "AngelscriptOfflineAssetExporter.h"
#include "Commandlets/Commandlet.h"
#include "Dump/AngelscriptOfflineExportService.h"
#include "Misc/Paths.h"

namespace AngelscriptEditor::Offline
{
	namespace
	{
		bool IsCommandletHostSwitch(const FString& Key)
		{
			static const TCHAR* const HostSwitches[] = {
				TEXT("run"),
				TEXT("buildmachine"),
				TEXT("unattended"),
				TEXT("nopause"),
				TEXT("nosplash"),
				TEXT("stdout"),
				TEXT("fullstdoutlogoutput"),
				TEXT("utf8output"),
				TEXT("abslog"),
				TEXT("nosound"),
				TEXT("nullrhi"),
			};
			for (const TCHAR* HostSwitch : HostSwitches)
			{
				if (Key.Equals(
					HostSwitch,
					ESearchCase::IgnoreCase))
				{
					return true;
				}
			}
			return false;
		}

		bool TrySplitSwitch(
			const FString& Switch,
			FString& OutKey,
			FString& OutValue,
			bool& bOutHasValue)
		{
			int32 Separator = INDEX_NONE;
			bOutHasValue = Switch.FindChar(TEXT('='), Separator);
			if (bOutHasValue)
			{
				OutKey = Switch.Left(Separator);
				OutValue = Switch.RightChop(Separator + 1);
				OutValue.TrimQuotesInline();
			}
			else
			{
				OutKey = Switch;
				OutValue.Reset();
			}
			OutKey.TrimStartAndEndInline();
			return !OutKey.IsEmpty();
		}

		bool ParseRoots(
			FString Value,
			TArray<FString>& OutRoots,
			FString& OutError)
		{
			Value.ReplaceInline(TEXT(";"), TEXT(","));
			TArray<FString> Parts;
			Value.ParseIntoArray(Parts, TEXT(","), true);
			if (Parts.IsEmpty())
			{
				OutError =
					TEXT("Asset root list must contain at least one root.");
				return false;
			}

			TSet<FString> Seen;
			for (FString Part : Parts)
			{
				const FString Root = NormalizeAssetObjectPath(Part);
				if (!Root.StartsWith(TEXT("/"))
					|| Root.Contains(TEXT(".")))
				{
					OutError = FString::Printf(
						TEXT("Invalid asset root '%s'."),
						*Part);
					return false;
				}
				if (!Seen.Contains(Root))
				{
					Seen.Add(Root);
					OutRoots.Add(Root);
				}
			}
			OutRoots.Sort();
			return true;
		}
	}

	FString GetDefaultOfflineExportDirectory(
		const AngelscriptOfflineContract::EBundleKind BundleKind)
	{
		return FPaths::ConvertRelativePathToFull(FPaths::Combine(
			FPaths::ProjectSavedDir(),
			TEXT("AngelscriptStandalone"),
			AngelscriptOfflineContract::LexToString(BundleKind)));
	}

	bool TryParseOfflineExportCommandOptions(
		const FString& Params,
		FOfflineExportCommandOptions& OutOptions,
		FString& OutError)
	{
		using namespace AngelscriptOfflineContract;

		OutOptions = FOfflineExportCommandOptions();
		OutError.Reset();

		TArray<FString> Tokens;
		TArray<FString> Switches;
		UCommandlet::ParseCommandLine(*Params, Tokens, Switches);
		if (!Tokens.IsEmpty())
		{
			OutError = FString::Printf(
				TEXT("Unexpected positional argument '%s'."),
				*Tokens[0]);
			return false;
		}

		TSet<FString> SeenKeys;
		for (const FString& Switch : Switches)
		{
			FString Key;
			FString Value;
			bool bHasValue = false;
			if (!TrySplitSwitch(
				Switch,
				Key,
				Value,
				bHasValue))
			{
				OutError = TEXT("Empty commandlet switch.");
				return false;
			}
			const FString CanonicalKey = Key.ToLower();
			if (SeenKeys.Contains(CanonicalKey))
			{
				OutError = FString::Printf(
					TEXT("Duplicate commandlet switch '-%s'."),
					*Key);
				return false;
			}
			SeenKeys.Add(CanonicalKey);

			// UCommandlet::Main receives launch-profile switches as well as
			// command-specific arguments. Accept only the fixed host set used
			// by the standard runner; all other unknown switches still fail.
			if (IsCommandletHostSwitch(Key))
			{
				continue;
			}

			if (Key.Equals(
				TEXT("AllowIncompleteAssets"),
				ESearchCase::IgnoreCase))
			{
				if (bHasValue)
				{
					OutError =
						TEXT("-AllowIncompleteAssets does not take a value.");
					return false;
				}
				OutOptions.bAllowIncompleteAssets = true;
				continue;
			}
			if (!bHasValue || Value.IsEmpty())
			{
				OutError = FString::Printf(
					TEXT("Switch '-%s' requires a value."),
					*Key);
				return false;
			}

			if (Key.Equals(TEXT("Output"), ESearchCase::IgnoreCase))
			{
				OutOptions.OutputDirectory =
					FPaths::ConvertRelativePathToFull(Value);
				OutOptions.bOutputWasExplicit = true;
			}
			else if (Key.Equals(
				TEXT("BundleKind"),
				ESearchCase::IgnoreCase))
			{
				if (Value.Equals(
					TEXT("Project"),
					ESearchCase::IgnoreCase))
				{
					OutOptions.BundleKind = EBundleKind::Project;
				}
				else if (Value.Equals(
					TEXT("DefaultEngine"),
					ESearchCase::IgnoreCase)
					|| Value.Equals(
						TEXT("default-engine"),
						ESearchCase::IgnoreCase))
				{
					OutOptions.BundleKind =
						EBundleKind::DefaultEngine;
				}
				else
				{
					OutError = FString::Printf(
						TEXT(
							"Unsupported BundleKind '%s'; expected Project or DefaultEngine."),
						*Value);
					return false;
				}
			}
			else if (Key.Equals(
				TEXT("AssetRoots"),
				ESearchCase::IgnoreCase))
			{
				OutOptions.bAssetRootsWereExplicit = true;
				if (!ParseRoots(
					Value,
					OutOptions.AssetRoots,
					OutError))
				{
					return false;
				}
			}
			else if (Key.Equals(
				TEXT("AssetExcludeRoots"),
				ESearchCase::IgnoreCase))
			{
				if (!ParseRoots(
					Value,
					OutOptions.ExcludedAssetRoots,
					OutError))
				{
					return false;
				}
			}
			else
			{
				OutError = FString::Printf(
					TEXT("Unknown commandlet switch '-%s'."),
					*Key);
				return false;
			}
		}

		if (!OutOptions.bAssetRootsWereExplicit
			&& OutOptions.BundleKind == EBundleKind::Project)
		{
			OutOptions.AssetRoots = {TEXT("/Game")};
		}
		if (!OutOptions.bOutputWasExplicit)
		{
			OutOptions.OutputDirectory =
				GetDefaultOfflineExportDirectory(
					OutOptions.BundleKind);
		}
		return true;
	}

	FOfflineExportPublicationResult PublishPreparedOfflineBundle(
		AngelscriptOfflineContract::FBundleWriteRequest Bundle,
		const FOfflineExportCommandOptions& Options,
		const bool bEngineReady)
	{
		using namespace AngelscriptOfflineContract;

		FOfflineExportPublicationResult Result;
		if (!bEngineReady)
		{
			Result.ExitCode =
				EOfflineExportCommandletExitCode::EngineNotReady;
			Result.Error =
				TEXT("AngelScript final engine is not ready.");
			return Result;
		}
		if (!Bundle.Manifest.SymbolScope.bComplete)
		{
			Result.ExitCode =
				EOfflineExportCommandletExitCode::IncompleteSymbolScope;
			Result.Error =
				TEXT("Final symbol scope is incomplete.");
			return Result;
		}
		if (!Bundle.Manifest.AssetScope.bComplete
			&& !Options.bAllowIncompleteAssets)
		{
			Result.ExitCode =
				EOfflineExportCommandletExitCode::IncompleteAssetScope;
			Result.Error =
				TEXT(
					"Asset scope is incomplete; pass -AllowIncompleteAssets to publish it explicitly.");
			return Result;
		}

		Bundle.OutputDirectory = Options.OutputDirectory;
		Bundle.Manifest.BundleKind = Options.BundleKind;
		Result.WriteResult =
			FAngelscriptOfflineBundleWriter::Write(Bundle);
		if (!Result.WriteResult.bSuccess)
		{
			Result.ExitCode =
				EOfflineExportCommandletExitCode::PublicationFailure;
			Result.Error = Result.WriteResult.Error;
			return Result;
		}
		Result.ExitCode =
			EOfflineExportCommandletExitCode::Success;
		return Result;
	}
}
