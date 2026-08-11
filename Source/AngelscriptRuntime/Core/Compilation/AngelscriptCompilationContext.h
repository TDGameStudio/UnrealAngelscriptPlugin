#pragma once

#include "CoreMinimal.h"
#include "AngelscriptEngine.h"

struct FAngelscriptCompilationEvent;

struct ANGELSCRIPTRUNTIME_API FAngelscriptCompilationModuleSummary
{
	FString ModuleName;
	int32 FileCount = 0;
	int32 ImportCount = 0;
	int32 ClassCount = 0;
	int32 FunctionCount = 0;
	bool bLoadedPrecompiledCode = false;
	bool bLoadedIncrementalCache = false;
	TArray<FString> FileNames;
	TArray<FString> ImportedModuleNames;

	static FAngelscriptCompilationModuleSummary FromModule(const TSharedRef<FAngelscriptModuleDesc>& Module);
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptCompilationContext
{
	explicit FAngelscriptCompilationContext(
		ECompileType InCompileType,
		const FAngelscriptCompileOptions& InCompileOptions,
		const TArray<TSharedRef<FAngelscriptModuleDesc>>& InInputModules);

	uint64 GetRunId() const
	{
		return RunId;
	}

	ECompileType GetCompileType() const
	{
		return CompileType;
	}

	EAngelscriptCompileCachePolicy GetCachePolicy() const
	{
		return CachePolicy;
	}

	ECompileResult GetCompileResult() const
	{
		return CompileResult;
	}

	int32 GetCompiledModuleCount() const
	{
		return CompiledModuleSummaries.Num();
	}

	void CaptureCompiledModules(const TArray<TSharedRef<FAngelscriptModuleDesc>>& InCompiledModules);
	void SetResult(ECompileResult InCompileResult);
	void PopulateInputSummary(FAngelscriptCompilationEvent& Event) const;
	void PopulateCompiledSummary(FAngelscriptCompilationEvent& Event) const;
	void PopulateResult(FAngelscriptCompilationEvent& Event) const;

private:
	uint64 RunId = 0;
	ECompileType CompileType = ECompileType::SoftReloadOnly;
	EAngelscriptCompileCachePolicy CachePolicy =
		EAngelscriptCompileCachePolicy::Default;
	ECompileResult CompileResult = ECompileResult::Error;
	TArray<FAngelscriptCompilationModuleSummary> InputModuleSummaries;
	TArray<FAngelscriptCompilationModuleSummary> CompiledModuleSummaries;

	static uint64 AllocateRunId();
	static void PopulateEventSummary(FAngelscriptCompilationEvent& Event, const TArray<FAngelscriptCompilationModuleSummary>& Summaries);
};
