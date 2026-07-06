#include "AngelscriptCodeCoverage.h"

#include "CoverageReportGenerator.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
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

namespace
{
	FCriticalSection GCodeCoverageExtensionCriticalSection;
	TMap<FAngelscriptEngine*, FAngelscriptCodeCoverageExtension*> GCodeCoverageExtensionsByEngine;
	TSharedPtr<FAngelscriptCodeCoverageExtension> GCodeCoverageExtension;
}

FAngelscriptCodeCoverage::~FAngelscriptCodeCoverage()
{
#if WITH_EDITOR
	RemoveTestFrameworkHooks();
#endif
}

#if WITH_EDITOR
void FAngelscriptCodeCoverage::AddTestFrameworkHooks()
{
	if (AutomationController.IsValid())
	{
		return;
	}

	IAutomationControllerModule* AutomationModule =
		FModuleManager::GetModulePtr<IAutomationControllerModule>(TEXT("AutomationController"));
	if (AutomationModule == nullptr)
	{
		UE_LOG(Angelscript, Warning, TEXT("Code coverage test framework hooks were not registered because AutomationController is not loaded."));
		return;
	}

	AutomationController = AutomationModule->GetAutomationController();
	TestsAvailableHandle = AutomationController->OnTestsAvailable().AddRaw(this, &FAngelscriptCodeCoverage::OnTestsStarting);
	TestsCompleteHandle = AutomationController->OnTestsComplete().AddRaw(this, &FAngelscriptCodeCoverage::OnTestsStopping);
	UE_LOG(Angelscript, Display, TEXT("Code coverage test framework hooks registered."));
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

void FAngelscriptCodeCoverage::RemoveTestFrameworkHooks()
{
	if (!AutomationController.IsValid())
	{
		return;
	}

	if (TestsAvailableHandle.IsValid())
	{
		AutomationController->OnTestsAvailable().Remove(TestsAvailableHandle);
		TestsAvailableHandle.Reset();
	}

	if (TestsCompleteHandle.IsValid())
	{
		AutomationController->OnTestsComplete().Remove(TestsCompleteHandle);
		TestsCompleteHandle.Reset();
	}

	AutomationController.Reset();
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
	if (!WriteCoverageJson(OutputDir))
	{
		UE_LOG(Angelscript, Warning, TEXT("Failed writing code coverage JSON"));
	}
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

bool FAngelscriptCodeCoverage::WriteCoverageJson(const FString& OutputDir)
{
	FCoverageJsonExportOptions Options;
	Options.ExcludePatterns = GetDefault<UAngelscriptTestSettings>()->CoverageExcludePatterns;
	return WriteCoverageSummaryJson(FilesToCoverage, OutputDir, Options);
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

void FAngelscriptCodeCoverageExtension::OnEngineAttached(FAngelscriptEngine& Engine)
{
	if (GetCoverage(Engine) != nullptr)
	{
		Engine.UpdateLineCallbackState();
		return;
	}

	if (FAngelscriptCodeCoverage::CoverageEnabled())
	{
		FEngineCoverage& EngineCoverage = Coverages.AddDefaulted_GetRef();
		EngineCoverage.Engine = &Engine;
		EngineCoverage.Coverage = MakeUnique<FAngelscriptCodeCoverage>();
		{
			FScopeLock Lock(&GCodeCoverageExtensionCriticalSection);
			GCodeCoverageExtensionsByEngine.Add(&Engine, this);
		}

		UE_LOG(Angelscript, Display, TEXT("Code coverage enabled for engine instance %p."), &Engine);

#if WITH_EDITOR
		EngineCoverage.Coverage->AddTestFrameworkHooks();
#endif
	}

	Engine.UpdateLineCallbackState();
}

void FAngelscriptCodeCoverageExtension::OnEngineDetached(FAngelscriptEngine& Engine)
{
	{
		FScopeLock Lock(&GCodeCoverageExtensionCriticalSection);
		GCodeCoverageExtensionsByEngine.Remove(&Engine);
	}

	for (int32 Index = Coverages.Num() - 1; Index >= 0; --Index)
	{
		if (Coverages[Index].Engine == &Engine)
		{
			Coverages.RemoveAtSwap(Index, 1, EAllowShrinking::No);
			break;
		}
	}
	UE_LOG(Angelscript, Verbose, TEXT("Code coverage detached from engine instance %p."), &Engine);
	Engine.UpdateLineCallbackState();
}

FAngelscriptCodeCoverage* FAngelscriptCodeCoverageExtension::GetCoverage(FAngelscriptEngine& Engine) const
{
	for (const FEngineCoverage& EngineCoverage : Coverages)
	{
		if (EngineCoverage.Engine == &Engine)
		{
			return EngineCoverage.Coverage.Get();
		}
	}

	return nullptr;
}

FAngelscriptCodeCoverage* FAngelscriptCodeCoverageExtension::GetForEngine(FAngelscriptEngine& Engine)
{
	FScopeLock Lock(&GCodeCoverageExtensionCriticalSection);
	if (FAngelscriptCodeCoverageExtension* const* Extension = GCodeCoverageExtensionsByEngine.Find(&Engine))
	{
		return (*Extension)->GetCoverage(Engine);
	}

	return nullptr;
}

FDelegateHandle FAngelscriptCodeCoverageExtension::Startup()
{
	GCodeCoverageExtension = MakeShared<FAngelscriptCodeCoverageExtension>();
	return FAngelscriptEngineExtensionRegistry::Get().RegisterExtension(GCodeCoverageExtension.ToSharedRef());
}

void FAngelscriptCodeCoverageExtension::Shutdown(FDelegateHandle& Handle)
{
	if (Handle.IsValid())
	{
		FAngelscriptEngineExtensionRegistry::Get().UnregisterExtension(Handle);
		Handle.Reset();
	}

	FScopeLock Lock(&GCodeCoverageExtensionCriticalSection);
	GCodeCoverageExtensionsByEngine.Empty();
	GCodeCoverageExtension.Reset();
}

void FAngelscriptCodeCoverageExtension::EnsureAttached(FAngelscriptEngine& Engine)
{
	if (!GCodeCoverageExtension.IsValid())
	{
		return;
	}

	if (GetForEngine(Engine) != nullptr)
	{
		return;
	}

	GCodeCoverageExtension->OnEngineAttached(Engine);
}
