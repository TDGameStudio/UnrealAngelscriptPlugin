#include "HotReload/AngelscriptDirectoryWatcherInternal.h"

#include "HAL/FileManager.h"
#include "Misc/Paths.h"

namespace
{
	TArray<FAngelscriptSourceRoot> MakeScriptRootsForWatcher(const TArray<FString>& RootPaths, const FAngelscriptEngine& Engine)
	{
		TArray<FAngelscriptSourceRoot> ScriptRoots = Engine.GetEffectiveScriptRootDescriptors();
		if (ScriptRoots.Num() != 0)
		{
			return ScriptRoots;
		}

		ScriptRoots.Reserve(RootPaths.Num());
		for (const FString& RootPath : RootPaths)
		{
			ScriptRoots.Add(FAngelscriptSourceRoot::FromGameRoot(RootPath));
		}
		return ScriptRoots;
	}

	bool TryResolveScriptRootRelativePath(
		const FString& AbsolutePath,
		const TArray<FAngelscriptSourceRoot>& ScriptRoots,
		FAngelscriptSourceRoot& OutScriptRoot,
		FString& OutRelativePath)
	{
		const FString NormalizedAbsolutePath = FPaths::ConvertRelativePathToFull(AbsolutePath);
		int32 BestRootLength = INDEX_NONE;
		bool bFoundRoot = false;

		for (const FAngelscriptSourceRoot& ScriptRoot : ScriptRoots)
		{
			const FString NormalizedRootPath = FPaths::ConvertRelativePathToFull(ScriptRoot.AbsolutePath);
			const FString RootPathWithSeparator = FPaths::ConvertRelativePathToFull(NormalizedRootPath / TEXT(""));
			const bool bIsExactRoot = NormalizedAbsolutePath.Equals(NormalizedRootPath, ESearchCase::IgnoreCase);
			const bool bIsUnderRoot = NormalizedAbsolutePath.StartsWith(RootPathWithSeparator, ESearchCase::IgnoreCase);
			if (!bIsExactRoot && !bIsUnderRoot)
			{
				continue;
			}

			const int32 RootLength = RootPathWithSeparator.Len();
			if (RootLength <= BestRootLength)
			{
				continue;
			}

			OutScriptRoot = ScriptRoot;
			OutRelativePath = NormalizedAbsolutePath;
			if (bIsExactRoot)
			{
				OutRelativePath.Reset();
			}
			else
			{
				FPaths::MakePathRelativeTo(OutRelativePath, *RootPathWithSeparator);
			}
			BestRootLength = RootLength;
			bFoundRoot = true;
		}

		return bFoundRoot;
	}

	FAngelscriptEngine::FFilenamePair MakeFilenamePair(
		const FAngelscriptSourceRoot& ScriptRoot,
		const FString& AbsolutePath,
		const FString& RelativePath)
	{
		const FAngelscriptSource Source =
			ScriptRoot.SourceKind == EAngelscriptSourceKind::Plugin && !ScriptRoot.MountName.IsEmpty()
				? FAngelscriptSource::FromPluginFile(ScriptRoot.MountName, RelativePath, AbsolutePath)
				: FAngelscriptSource::FromGameFile(RelativePath, AbsolutePath);
		return FAngelscriptEngine::FFilenamePair{
			Source.AbsoluteFilename,
			Source.RelativeFilename,
			Source.VirtualPath.ToString()
		};
	}
}

namespace AngelscriptEditor::Private
{
	TArray<FAngelscriptEngine::FFilenamePair> GatherLoadedScriptsForFolder(FAngelscriptEngine& Engine, const FString& AbsoluteFolderPath)
	{
		TArray<FAngelscriptEngine::FFilenamePair> LoadedScripts;
		for (const TSharedRef<FAngelscriptModuleDesc>& Module : Engine.GetActiveModules())
		{
			for (const FAngelscriptModuleDesc::FCodeSection& CodeSection : Module->Code)
			{
				if (CodeSection.AbsoluteFilename.StartsWith(AbsoluteFolderPath))
				{
					LoadedScripts.AddUnique({ CodeSection.AbsoluteFilename, CodeSection.RelativeFilename, CodeSection.VirtualPath });
				}
			}
		}

		return LoadedScripts;
	}

	void QueueScriptFileChanges(const TArray<FFileChangeData>& Changes, const TArray<FString>& RootPaths, FAngelscriptEngine& Engine, IFileManager& FileManager, const FEnumerateLoadedScripts& EnumerateLoadedScripts)
	{
		const TArray<FAngelscriptSourceRoot> ScriptRoots = MakeScriptRootsForWatcher(RootPaths, Engine);
		for (const FFileChangeData& Change : Changes)
		{
			const FString AbsolutePath = FPaths::ConvertRelativePathToFull(Change.Filename);
			FAngelscriptSourceRoot ScriptRoot;
			FString RelativePath;

			if (!TryResolveScriptRootRelativePath(AbsolutePath, ScriptRoots, ScriptRoot, RelativePath))
			{
				continue;
			}

			Engine.LastFileChangeDetectedTime = FPlatformTime::Seconds();

			if (AbsolutePath.EndsWith(TEXT(".as")))
			{
				const FAngelscriptEngine::FFilenamePair ScriptFile = MakeFilenamePair(ScriptRoot, AbsolutePath, RelativePath);
				if (Change.Action == FFileChangeData::EFileChangeAction::FCA_Removed)
				{
					Engine.FileDeletionsDetectedForReload.AddUnique(ScriptFile);
				}
				else
				{
					Engine.FileChangesDetectedForReload.AddUnique(ScriptFile);
				}

				UE_LOG(Angelscript, Log, TEXT("Queued script file change for primary engine reload: %s"), *RelativePath);
				continue;
			}

			if (Change.Action == FFileChangeData::EFileChangeAction::FCA_Removed)
			{
				for (const FAngelscriptEngine::FFilenamePair& LoadedScript : EnumerateLoadedScripts(AbsolutePath / TEXT("")))
				{
					FAngelscriptEngine::FFilenamePair ScriptFile = LoadedScript;
					if (ScriptFile.VirtualPath.IsEmpty())
					{
						FAngelscriptSourceRoot LoadedScriptRoot;
						FString LoadedRelativePath;
						if (TryResolveScriptRootRelativePath(ScriptFile.AbsolutePath, ScriptRoots, LoadedScriptRoot, LoadedRelativePath))
						{
							ScriptFile = MakeFilenamePair(LoadedScriptRoot, ScriptFile.AbsolutePath, LoadedRelativePath);
						}
					}
					Engine.FileDeletionsDetectedForReload.AddUnique(ScriptFile);
				}
			}
			else if (Change.Action == FFileChangeData::EFileChangeAction::FCA_Added && FileManager.DirectoryExists(*AbsolutePath))
			{
				TArray<FAngelscriptEngine::FFilenamePair> ContainedScriptFiles;
				FAngelscriptEngine::FindScriptFiles(FileManager, RelativePath, AbsolutePath, TEXT("*.as"), ContainedScriptFiles, false, false);

				for (const FAngelscriptEngine::FFilenamePair& ScriptFile : ContainedScriptFiles)
				{
					Engine.FileChangesDetectedForReload.AddUnique(MakeFilenamePair(ScriptRoot, ScriptFile.AbsolutePath, ScriptFile.RelativePath));
				}
			}
		}
	}
}
