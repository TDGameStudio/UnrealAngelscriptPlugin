#include "AngelscriptCodeCoverage.h"

#include "CoverageReportGenerator.h"
#include "Misc/Paths.h"
#include "Testing/AngelscriptTestSettings.h"

#include "StartAngelscriptHeaders.h"
#include "AngelscriptInclude.h"
//#include "as_objecttype.h"
//#include "as_module.h"
//#include "as_scriptfunction.h"
#include "source/as_objecttype.h"
#include "source/as_module.h"
#include "source/as_scriptfunction.h"
#include "EndAngelscriptHeaders.h"

#if WITH_EDITOR
#include "Misc/AutomationTest.h"
#endif

#if WITH_EDITOR
void FAngelscriptCodeCoverage::AddTestFrameworkHooks()
{
	IAutomationControllerModule& AutomationModule =
		FModuleManager::LoadModuleChecked<IAutomationControllerModule>("AutomationController");
	IAutomationControllerManagerRef AutomationController = AutomationModule.GetAutomationController();
	AutomationController->OnTestsAvailable().AddRaw(this, &FAngelscriptCodeCoverage::OnTestsStarting);
	AutomationController->OnTestsComplete().AddRaw(this, &FAngelscriptCodeCoverage::OnTestsStopping);
}

void FAngelscriptCodeCoverage::OnTestsStarting(EAutomationControllerModuleState::Type Type)
{
	if (Type == EAutomationControllerModuleState::Type::Running) {
		StartRecording();
	}
}

void FAngelscriptCodeCoverage::OnTestsStopping()
{
	FString OutputDir = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("CodeCoverage"));
	StopRecordingAndWriteReport(OutputDir);
}
#endif

bool FAngelscriptCodeCoverage::CoverageEnabled()
{
	// You can enable either on the command line (useful for CI) or with a config setting.
	return (GetDefault<UAngelscriptTestSettings>()->bEnableCodeCoverage ||
			FParse::Param(FCommandLine::Get(), TEXT("as-enable-code-coverage")));
}

void FAngelscriptCodeCoverage::StartRecording()
{
	UE_LOG(Angelscript, Display, TEXT("Starting new test run, resetting code coverage"));
	ResetHits();
	bRecording = true;
}

void FAngelscriptCodeCoverage::StopRecordingAndWriteReport(const FString& OutputDir)
{
	bRecording = false;
	UE_LOG(Angelscript, Display, TEXT("Tests complete, writing coverage report to %s."), *OutputDir);
	WriteReportHtml(OutputDir);
	WriteCoverageSummaries(OutputDir);
}

void FAngelscriptCodeCoverage::MapExecutableLines(FAngelscriptModuleDesc& Module)
{
	// MapExecutableLines is the main population path for `FilesToCoverage`,
	// the per-line hit-count store. Tag the resulting allocations under the
	// dedicated `Angelscript/CodeCoverage` LLM bucket so coverage memory shows
	// up separately from generic engine state.
	asCModule* ScriptModule = Module.ScriptModule;
	if (!ensure(ScriptModule != nullptr))
	{
		return;
	}

	FString& RelativeFilename = Module.Code[0].RelativeFilename;
	if (!FilesToCoverage.Contains(*RelativeFilename))
	{
		FilesToCoverage.Add(*RelativeFilename);
		FilesToCoverage[RelativeFilename].AbsoluteFilename = Module.Code[0].AbsoluteFilename;
	}

	TMap<int, int>& HitCounts = FilesToCoverage[RelativeFilename].HitCounts;
	HitCounts.Empty();
	for (unsigned int i = 0; i < ScriptModule->GetFunctionCount(); i++)
	{
		asCScriptFunction* GlobalFunction = (asCScriptFunction*) ScriptModule->GetFunctionByIndex(i);

		if (GlobalFunction->scriptData != nullptr)
		{
			MapFunction(GlobalFunction, HitCounts);
		}
	}

	for (unsigned int i = 0; i < ScriptModule->GetObjectTypeCount(); i++)
	{
		asITypeInfo* Type = ScriptModule->GetObjectTypeByIndex(i);
		for (unsigned int j = 0; j < Type->GetMethodCount(); j++)
		{
			asCScriptFunction* Method = (asCScriptFunction*) Type->GetMethodByIndex(j);
			if (Method->objectType != Type)
				continue;
			MapFunction(Method, HitCounts);
		}
	}
}

void FAngelscriptCodeCoverage::HitLine(FAngelscriptModuleDesc& Module, int Line)
{
	if (!bRecording)
	{
		return;
	}

	const FString& RelativeFilename = Module.Code[0].RelativeFilename;
	FLineCoverage* FileCoverage = FilesToCoverage.Find(RelativeFilename);
	if (FileCoverage == nullptr)
	{
		UE_LOG(Angelscript, Display, TEXT("Coverage: hit line %d in unmapped file %s"), Line, *RelativeFilename);
		return;
	}

	int* Count = FileCoverage->HitCounts.Find(Line);
	if (Count != nullptr)
	{
		(*Count)++;
	}
}

const FLineCoverage* FAngelscriptCodeCoverage::GetLineCoverage(FAngelscriptModuleDesc& Module) const
{
	return FilesToCoverage.Find(Module.Code[0].RelativeFilename);
}

void FAngelscriptCodeCoverage::MapFunction(asCScriptFunction* F, TMap<int, int>& HitCounts)
{
	int DeclaredAt = F->scriptData->declaredAt & 0xFFFFF;
	int FirstExecutableLine = F->FindNextLineWithCode(DeclaredAt);
	int Current = FirstExecutableLine;
	int Last = Current;
	while (Current != -1)
	{
		HitCounts.Add(Current, 0);
		Last = Current;
		Current = F->FindNextLineWithCode(Current + 1);
	}

	if (F->GetReturnTypeId() == asTYPEID_VOID)
	{
		// The return of a void function counts as an executable line,
		// but disregard that as it looks a bit weird. This means an
		// empty void function has 0 executable lines, but that's okay.
		HitCounts.Remove(Last);
	}
}

void FAngelscriptCodeCoverage::ResetHits()
{
	for (TPair<FString, FLineCoverage>& FileCoverage : FilesToCoverage)
	{
		for (TPair<int, int>& Line : FileCoverage.Value.HitCounts)
		{
			Line.Value = 0;
		}
	}
}

bool FAngelscriptCodeCoverage::WriteReportHtml(const FString& OutputDir)
{
	bool bOk = true;
	for (TPair<FString, FLineCoverage>& FileCoverage : FilesToCoverage)
	{
		int NumLinesInOriginal;
		bOk &= WriteFileCoverageReportHtml(FileCoverage.Key, FileCoverage.Value, OutputDir, NumLinesInOriginal);

		// Take this opportunity to prune out generated angelscript from the coverage now that we loaded the original
		// source file and know its length in lines. Angelscript puts generated code on lines outside the file.
		FileCoverage.Value.PruneGeneratedCode(NumLinesInOriginal);
	}
	return bOk;
}

bool FAngelscriptCodeCoverage::WriteCoverageSummaries(const FString& OutputDir) const
{
	TMap<FString, FCoverageNode> Subdirs;
	FCoverageNode Root;
	for (const TPair<FString, FLineCoverage>& FileCoverage : FilesToCoverage)
	{
		const FString& AsFilePath = FileCoverage.Key;
		if (!IgnoredForCodeCoverage(AsFilePath))
		{
			AddCoverageLeaf(Root, AsFilePath, FileCoverage.Value);
		}
	}
	ComputeCoverage(Root);
	if (!WriteTopLevelCoverageJson(Root, OutputDir))
	{
		UE_LOG(Angelscript, Warning, TEXT("Failed writing code coverage JSON"));
	}
	return WriteCoverageSummaryHtml(Root, OutputDir);
}

bool FAngelscriptCodeCoverage::IgnoredForCodeCoverage(const FString& AsFilePath) const
{
	for (const FString& IgnoredPattern : GetDefault<UAngelscriptTestSettings>()->CoverageExcludePatterns)
	{
		if (AsFilePath.MatchesWildcard(IgnoredPattern))
		{
			return true;
		}
	}
	return false;
}
