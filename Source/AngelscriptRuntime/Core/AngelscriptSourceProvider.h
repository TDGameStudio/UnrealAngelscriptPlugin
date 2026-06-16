#pragma once

#include "CoreMinimal.h"

#include "AngelscriptSource.h"

class IFileManager;

struct ANGELSCRIPTRUNTIME_API FAngelscriptSourceState
{
	FDateTime Timestamp;
	uint64 ContentHash = 0;
	bool bHasContentHash = false;
};

struct ANGELSCRIPTRUNTIME_API IAngelscriptSourceProvider
{
	virtual ~IAngelscriptSourceProvider() = default;

	virtual void FindSources(
		const TArray<FAngelscriptSourceRoot>& ScriptRoots,
		bool bSkipDevelopmentScripts,
		bool bSkipEditorScripts,
		TArray<FAngelscriptSource>& OutSources) = 0;

	virtual bool LoadSourceText(const FAngelscriptSource& Source, FString& OutSourceText) = 0;
	virtual bool QuerySourceState(const FAngelscriptSource& Source, FAngelscriptSourceState& OutState) = 0;
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptDiskSourceProvider final : IAngelscriptSourceProvider
{
	virtual void FindSources(
		const TArray<FAngelscriptSourceRoot>& ScriptRoots,
		bool bSkipDevelopmentScripts,
		bool bSkipEditorScripts,
		TArray<FAngelscriptSource>& OutSources) override;

	virtual bool LoadSourceText(const FAngelscriptSource& Source, FString& OutSourceText) override;
	virtual bool QuerySourceState(const FAngelscriptSource& Source, FAngelscriptSourceState& OutState) override;

private:
	static void FindScriptFiles(
		IFileManager& FileManager,
		const FAngelscriptSourceRoot& Root,
		const FString& RelativeRoot,
		const FString& SearchDirectory,
		const TCHAR* Pattern,
		TArray<FAngelscriptSource>& OutSources,
		bool bSkipDevelopmentScripts,
		bool bSkipEditorScripts);
};
