#pragma once

#include "AngelscriptEngine.h"
#include "Core/AngelscriptEngineExtensionRegistry.h"
#include "LineCoverage.h"

#if WITH_EDITOR
#include "IAutomationControllerModule.h"
#include "IAutomationControllerManager.h"
#endif

class asIScriptModule;
class asCScriptFunction;

// Manages per-module line coverage and writes the machine-readable
// coverage_summary.json package consumed by external tools.
struct FAngelscriptCodeCoverage
{
	ANGELSCRIPTRUNTIME_API ~FAngelscriptCodeCoverage();

	static ANGELSCRIPTRUNTIME_API bool CoverageEnabled();

	// Starts recording (HitLine calls are ignored unless we're recording).
	ANGELSCRIPTRUNTIME_API void StartRecording();

	// Stops recording and writes coverage to OutputDir.
	ANGELSCRIPTRUNTIME_API void StopRecordingAndWriteReport(const FString& OutputDir);

	// Adds the .as module to our internal coverage map. This must be done before trying
	// to record line hits in it.
	ANGELSCRIPTRUNTIME_API void MapExecutableLines(FAngelscriptModuleDesc& Module);

	// Call when the line in the given file has been executed.
	ANGELSCRIPTRUNTIME_API void HitLine(FAngelscriptModuleDesc& Module, int Line);

	// Retrieves what we got so far on a given module.
	ANGELSCRIPTRUNTIME_API const FLineCoverage* GetLineCoverage(FAngelscriptModuleDesc& Module) const;

	// Hooks up StartRecording and StopRecordingAndWriteReport so we reset coverage
	// between test runs and writes a report at the end. This must be called post
	// engine init since the  automation controller module must be loaded. You don't
	// have to call Start/StopRecording yourself if you use this.
	//
	// A "test run" is considered to be between when the test automation controller says
	// tests have been refreshed and when the tests end.
#if WITH_EDITOR
	void AddTestFrameworkHooks();
#endif

	// Clear all line hits so far.
	ANGELSCRIPTRUNTIME_API void ResetHits();

private:
#if WITH_EDITOR
	// Starts recording when run from the test framework.
	void OnTestsStarting(EAutomationControllerModuleState::Type Type);

	// Writes a report for the case we were run from the tests.
	void OnTestsStopping();

	void RemoveTestFrameworkHooks();
#endif

	bool WriteCoverageJson(const FString& OutputDir);

	void MapFunction(asCScriptFunction* F, TMap<int, int>& HitCounts);

	bool IgnoredForCodeCoverage(const FString& AsFilePath) const;

	// Map of relative filename to coverage info.
	TMap<FString, FLineCoverage> FilesToCoverage;

	bool bRecording = false;

#if WITH_EDITOR
	IAutomationControllerManagerPtr AutomationController;
	FDelegateHandle TestsAvailableHandle;
	FDelegateHandle TestsCompleteHandle;
#endif
};

// Owns code coverage recorders per FAngelscriptEngine instance.
class ANGELSCRIPTRUNTIME_API FAngelscriptCodeCoverageExtension : public IAngelscriptExtension
{
public:
	FAngelscriptCodeCoverageExtension() = default;
	FAngelscriptCodeCoverageExtension(const FAngelscriptCodeCoverageExtension&) = delete;
	FAngelscriptCodeCoverageExtension& operator=(const FAngelscriptCodeCoverageExtension&) = delete;

	virtual void OnEngineAttached(FAngelscriptEngine& Engine) override;
	virtual void OnEngineDetached(FAngelscriptEngine& Engine) override;

	FAngelscriptCodeCoverage* GetCoverage(FAngelscriptEngine& Engine) const;

	static FAngelscriptCodeCoverage* GetForEngine(FAngelscriptEngine& Engine);
	static FDelegateHandle Startup();
	static void Shutdown(FDelegateHandle& Handle);
	static void EnsureAttached(FAngelscriptEngine& Engine);
	static void EnsureDetached(FAngelscriptEngine& Engine);

private:
	struct FEngineCoverage
	{
		FAngelscriptEngine* Engine = nullptr;
		TUniquePtr<FAngelscriptCodeCoverage> Coverage;
	};

	TArray<FEngineCoverage> Coverages;
};
