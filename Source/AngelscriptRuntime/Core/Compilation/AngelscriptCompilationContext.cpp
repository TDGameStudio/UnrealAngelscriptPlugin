#include "Compilation/AngelscriptCompilationContext.h"

#include "Compilation/AngelscriptCompilationEvents.h"
#include "HAL/PlatformAtomics.h"

namespace AngelscriptCompilationContext_Private
{
	int64 GNextCompilationRunId = 0;
}

FAngelscriptCompilationModuleSummary FAngelscriptCompilationModuleSummary::FromModule(const TSharedRef<FAngelscriptModuleDesc>& Module)
{
	FAngelscriptCompilationModuleSummary Summary;
	Summary.ModuleName = Module->ModuleName;
	Summary.ImportCount = Module->ImportedModules.Num();
	Summary.ClassCount = Module->Classes.Num();
	Summary.bLoadedPrecompiledCode = Module->bLoadedPrecompiledCode;
	Summary.bLoadedIncrementalCache = Module->bLoadedIncrementalCache;

	for (const FAngelscriptModuleDesc::FCodeSection& Section : Module->Code)
	{
		Summary.FileNames.AddUnique(Section.RelativeFilename);
	}
	Summary.FileCount = Summary.FileNames.Num();

	for (const FString& ImportedModuleName : Module->ImportedModules)
	{
		Summary.ImportedModuleNames.AddUnique(ImportedModuleName);
	}

	for (const TSharedRef<FAngelscriptClassDesc>& ClassDesc : Module->Classes)
	{
		Summary.FunctionCount += ClassDesc->Methods.Num();
	}

	return Summary;
}

FAngelscriptCompilationContext::FAngelscriptCompilationContext(
	ECompileType InCompileType,
	const FAngelscriptCompileOptions& InCompileOptions,
	const TArray<TSharedRef<FAngelscriptModuleDesc>>& InInputModules)
	: RunId(AllocateRunId())
	, CompileType(InCompileType)
	, CachePolicy(InCompileOptions.CachePolicy)
{
	InputModuleSummaries.Reserve(InInputModules.Num());
	for (const TSharedRef<FAngelscriptModuleDesc>& Module : InInputModules)
	{
		InputModuleSummaries.Add(FAngelscriptCompilationModuleSummary::FromModule(Module));
	}
}

void FAngelscriptCompilationContext::CaptureCompiledModules(const TArray<TSharedRef<FAngelscriptModuleDesc>>& InCompiledModules)
{
	CompiledModuleSummaries.Reset(InCompiledModules.Num());
	for (const TSharedRef<FAngelscriptModuleDesc>& Module : InCompiledModules)
	{
		CompiledModuleSummaries.Add(FAngelscriptCompilationModuleSummary::FromModule(Module));
	}
}

void FAngelscriptCompilationContext::SetResult(ECompileResult InCompileResult)
{
	CompileResult = InCompileResult;
}

void FAngelscriptCompilationContext::PopulateInputSummary(FAngelscriptCompilationEvent& Event) const
{
	Event.CachePolicy = CachePolicy;
	PopulateEventSummary(Event, InputModuleSummaries);
}

void FAngelscriptCompilationContext::PopulateCompiledSummary(FAngelscriptCompilationEvent& Event) const
{
	Event.CachePolicy = CachePolicy;
	PopulateEventSummary(Event, CompiledModuleSummaries);
}

void FAngelscriptCompilationContext::PopulateResult(FAngelscriptCompilationEvent& Event) const
{
	Event.CompilationRunId = RunId;
	Event.CompileType = CompileType;
	Event.CachePolicy = CachePolicy;
	Event.CompileResult = CompileResult;
	Event.bSucceeded = CompileResult == ECompileResult::FullyHandled || CompileResult == ECompileResult::PartiallyHandled;
	Event.bFailed = !Event.bSucceeded;
	Event.CompiledModuleCount = CompiledModuleSummaries.Num();
}

uint64 FAngelscriptCompilationContext::AllocateRunId()
{
	return static_cast<uint64>(FPlatformAtomics::InterlockedIncrement(&AngelscriptCompilationContext_Private::GNextCompilationRunId));
}

void FAngelscriptCompilationContext::PopulateEventSummary(FAngelscriptCompilationEvent& Event, const TArray<FAngelscriptCompilationModuleSummary>& Summaries)
{
	for (const FAngelscriptCompilationModuleSummary& Summary : Summaries)
	{
		Event.ModuleNames.AddUnique(Summary.ModuleName);

		for (const FString& FileName : Summary.FileNames)
		{
			Event.FileNames.AddUnique(FileName);
		}

		for (const FString& ImportedModuleName : Summary.ImportedModuleNames)
		{
			Event.ImportedModuleNames.AddUnique(ImportedModuleName);
		}

		Event.ImportCount += Summary.ImportCount;
		Event.ClassCount += Summary.ClassCount;
		Event.FunctionCount += Summary.FunctionCount;
		Event.bLoadedPrecompiledCode |= Summary.bLoadedPrecompiledCode;
		Event.bLoadedIncrementalCache |= Summary.bLoadedIncrementalCache;
	}

	Event.ModuleCount = Event.ModuleNames.Num();
	Event.FileCount = Event.FileNames.Num();
}
