#pragma once

#include "CoreMinimal.h"
#include "Delegates/Delegate.h"

struct FAngelscriptEngine;
struct FAngelscriptStateDiff;
struct FAngelscriptStateSnapshot;

struct ANGELSCRIPTRUNTIME_API FAngelscriptStateDump
{
	struct FTableResult
	{
		FString TableName;
		int32 RowCount = 0;
		FString Status = TEXT("Success");
		FString ErrorMessage;
	};

	using FDumpExtensionsDelegate = TMulticastDelegate<void(const FString&)>;

	static FString DumpAll(FAngelscriptEngine& Engine, const FString& OutputDir = TEXT(""));
	static FAngelscriptStateSnapshot CaptureSnapshot(FAngelscriptEngine& Engine);
	static FAngelscriptStateDiff DiffSnapshots(const FAngelscriptStateSnapshot& Before, const FAngelscriptStateSnapshot& After);
	static FTableResult DumpSnapshot(const FAngelscriptStateSnapshot& Snapshot, const FString& OutputDir);
	static TArray<FTableResult> DumpSnapshotCategoryTables(const FAngelscriptStateSnapshot& Snapshot, const FString& OutputDir);
	static TArray<FTableResult> DumpDiff(const FAngelscriptStateDiff& Diff, const FString& OutputDir);

	static FDumpExtensionsDelegate OnDumpExtensions;

private:
	static FString ResolveOutputDir(const FString& OutputDir);
	static bool EnsureOutputDir(const FString& OutputDir);
	static FString MakeTimestampDirectoryName();
	static FTableResult DumpEngineOverview(FAngelscriptEngine& Engine, const FString& OutputDir);
	static FTableResult DumpRuntimeConfig(FAngelscriptEngine& Engine, const FString& OutputDir);
	static FTableResult DumpModules(FAngelscriptEngine& Engine, const FString& OutputDir);
	static FTableResult DumpClasses(FAngelscriptEngine& Engine, const FString& OutputDir);
	static FTableResult DumpProperties(FAngelscriptEngine& Engine, const FString& OutputDir);
	static FTableResult DumpFunctions(FAngelscriptEngine& Engine, const FString& OutputDir);
	static FTableResult DumpEnums(FAngelscriptEngine& Engine, const FString& OutputDir);
	static FTableResult DumpDelegates(FAngelscriptEngine& Engine, const FString& OutputDir);
	static FTableResult DumpRegisteredTypes(FAngelscriptEngine& Engine, const FString& OutputDir);
	static FTableResult DumpDiagnostics(FAngelscriptEngine& Engine, const FString& OutputDir);
	static FTableResult DumpScriptEngineState(FAngelscriptEngine& Engine, const FString& OutputDir);
	static FTableResult DumpSummary(const TArray<FTableResult>& TableResults, const FString& OutputDir);
	static FTableResult DumpBindRegistrations(FAngelscriptEngine& Engine, const FString& OutputDir);
	static FTableResult DumpBindDatabaseStructs(FAngelscriptEngine& Engine, const FString& OutputDir);
	static FTableResult DumpBindDatabaseClasses(FAngelscriptEngine& Engine, const FString& OutputDir);
	static FTableResult DumpToStringTypes(FAngelscriptEngine& Engine, const FString& OutputDir);
	static FTableResult DumpDocumentationStats(FAngelscriptEngine& Engine, const FString& OutputDir);
	static FTableResult DumpEngineSettings(FAngelscriptEngine& Engine, const FString& OutputDir);
	static FTableResult DumpHotReloadState(FAngelscriptEngine& Engine, const FString& OutputDir);
	static FTableResult DumpJITDatabase(FAngelscriptEngine& Engine, const FString& OutputDir);
	static FTableResult DumpPrecompiledData(FAngelscriptEngine& Engine, const FString& OutputDir);
	static FTableResult DumpStaticJITState(FAngelscriptEngine& Engine, const FString& OutputDir);
	static FTableResult DumpDebugServerState(FAngelscriptEngine& Engine, const FString& OutputDir);
	static FTableResult DumpDebugBreakpoints(FAngelscriptEngine& Engine, const FString& OutputDir);
	static FTableResult DumpCodeCoverage(FAngelscriptEngine& Engine, const FString& OutputDir);
};
