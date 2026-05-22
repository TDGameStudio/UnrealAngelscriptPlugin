#include "Core/AngelscriptTestModule.h"

#include "AngelscriptEngineSubsystem.h"
#include "Preprocessor/AngelscriptPreprocessorTestHelpers.h"
#include "Shared/AngelscriptTestEnginePool.h"

#include "Logging/LogMacros.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

IMPLEMENT_MODULE(FAngelscriptTestModule, AngelscriptTest);

DEFINE_LOG_CATEGORY_STATIC(LogAngelscriptTest, Log, All);

namespace
{
	TUniquePtr<FAngelscriptEngine> GAngelscriptTestStartupOverrideEngine;

	FAngelscriptEngineConfig CreateEditorScanFreeStartupConfig()
	{
		FAngelscriptEngineConfig Config = FAngelscriptEngineConfig::FromCurrentProcess();
		Config.bIsEditor = true;
		Config.bForcePreprocessEditorCode = true;
		return Config;
	}
}

#if WITH_DEV_AUTOMATION_TESTS
// Definition for the log category declared in AngelscriptPreprocessorTestHelpers.h.
// Default verbosity is NoLogging; enable on demand via -LogCmds or
// LogPreprocessorDump.SetVerbosity(...) inside a TEST_METHOD.
DEFINE_LOG_CATEGORY(LogPreprocessorDump);
#endif

void FAngelscriptTestModule::StartupModule()
{
	const bool bUseScanFreeStartupEngine = FParse::Param(FCommandLine::Get(), TEXT("AngelscriptTestUseScanFreeStartupEngine"));
	if (bUseScanFreeStartupEngine)
	{
		GAngelscriptTestStartupOverrideEngine = AngelscriptTestSupport::CreateScriptScanFreeFullEngineForTesting(
			CreateEditorScanFreeStartupConfig(),
			FAngelscriptEngineDependencies::CreateDefault());
		UAngelscriptEngineSubsystem::SetInitializeOverrideForTesting([]() -> FAngelscriptEngine*
		{
			return GAngelscriptTestStartupOverrideEngine.Get();
		});
	}

	const bool bPrewarmEngine = FParse::Param(FCommandLine::Get(), TEXT("AngelscriptTestPrewarmEngine"));
	AngelscriptTestSupport::StartupTestEnginePool(bPrewarmEngine);
	UE_LOG(LogAngelscriptTest, Log, TEXT("AngelscriptTest module started."));
}

void FAngelscriptTestModule::ShutdownModule()
{
	UAngelscriptEngineSubsystem::ResetInitializeStateForTesting();
	AngelscriptTestSupport::ShutdownTestEnginePool();
	GAngelscriptTestStartupOverrideEngine.Reset();
	UE_LOG(LogAngelscriptTest, Log, TEXT("AngelscriptTest module shut down."));
}
