#include "AngelscriptSourceProvider.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"

#define XXH_PRIVATE_API
#include "Hash/xxhash.h"

namespace AngelscriptSourceProvider_Private
{
	void AddSourceForRoot(
		const FAngelscriptSourceRoot& Root,
		const FString& RelativeRoot,
		const FString& SearchDirectory,
		const FString& FoundFile,
		TArray<FAngelscriptSource>& OutSources)
	{
		const FString RelativePath = RelativeRoot / FoundFile;
		const FString AbsolutePath = SearchDirectory / FoundFile;

		if (Root.SourceKind == EAngelscriptSourceKind::Plugin && !Root.MountName.IsEmpty())
		{
			OutSources.Add(FAngelscriptSource::FromPluginFile(Root.MountName, RelativePath, AbsolutePath));
		}
		else
		{
			OutSources.Add(FAngelscriptSource::FromGameFile(RelativePath, AbsolutePath));
		}
	}
}

void FAngelscriptDiskSourceProvider::FindScriptFiles(
	IFileManager& FileManager,
	const FAngelscriptSourceRoot& Root,
	const FString& RelativeRoot,
	const FString& SearchDirectory,
	const TCHAR* Pattern,
	TArray<FAngelscriptSource>& OutSources,
	bool bSkipDevelopmentScripts,
	bool bSkipEditorScripts)
{
	const FString CurrentSearch = SearchDirectory / Pattern;

	TArray<FString> LocalFiles;
	FileManager.FindFiles(LocalFiles, *CurrentSearch, true, false);

	for (const FString& FoundFile : LocalFiles)
	{
		AngelscriptSourceProvider_Private::AddSourceForRoot(Root, RelativeRoot, SearchDirectory, FoundFile, OutSources);
	}

	TArray<FString> LocalDirs;
	const FString RecursiveDirSearch = SearchDirectory / TEXT("*");
	FileManager.FindFiles(LocalDirs, *RecursiveDirSearch, false, true);

	for (const FString& FoundDirectory : TSet<FString>(LocalDirs))
	{
		if (bSkipDevelopmentScripts)
		{
			if (FoundDirectory == TEXT("Examples"))
				continue;
			if (FoundDirectory == TEXT("Dev"))
				continue;
		}

		if (bSkipEditorScripts)
		{
			if (FoundDirectory == TEXT("Editor"))
				continue;
		}

		FindScriptFiles(
			FileManager,
			Root,
			RelativeRoot / FoundDirectory,
			SearchDirectory / FoundDirectory,
			Pattern,
			OutSources,
			bSkipDevelopmentScripts,
			bSkipEditorScripts);
	}
}

void FAngelscriptDiskSourceProvider::FindSources(
	const TArray<FAngelscriptSourceRoot>& ScriptRoots,
	bool bSkipDevelopmentScripts,
	bool bSkipEditorScripts,
	TArray<FAngelscriptSource>& OutSources)
{
	IFileManager& FileManager = IFileManager::Get();

	for (const FAngelscriptSourceRoot& Root : ScriptRoots)
	{
		if (Root.AbsolutePath.IsEmpty())
		{
			continue;
		}

		FindScriptFiles(
			FileManager,
			Root,
			TEXT(""),
			Root.AbsolutePath,
			TEXT("*.as"),
			OutSources,
			bSkipDevelopmentScripts,
			bSkipEditorScripts);
	}
}

bool FAngelscriptDiskSourceProvider::LoadSourceText(const FAngelscriptSource& Source, FString& OutSourceText)
{
	if (Source.AbsoluteFilename.IsEmpty())
	{
		return false;
	}

	return FFileHelper::LoadFileToString(OutSourceText, *Source.AbsoluteFilename);
}

bool FAngelscriptDiskSourceProvider::QuerySourceState(const FAngelscriptSource& Source, FAngelscriptSourceState& OutState)
{
	if (Source.AbsoluteFilename.IsEmpty() || !IFileManager::Get().FileExists(*Source.AbsoluteFilename))
	{
		return false;
	}

	OutState.Timestamp = IFileManager::Get().GetTimeStamp(*Source.AbsoluteFilename);
	OutState.ContentHash = 0;
	OutState.bHasContentHash = false;

	FString SourceText;
	if (!LoadSourceText(Source, SourceText))
	{
		return false;
	}

	OutState.ContentHash = SourceText.Len() > 0
		? static_cast<uint64>(XXH64(*SourceText, SourceText.Len() * sizeof(TCHAR), 0))
		: 0;
	OutState.bHasContentHash = true;
	return true;
}
