#include "AngelscriptPerformanceStats.h"

CSV_DEFINE_CATEGORY_MODULE(ANGELSCRIPTRUNTIME_API, Angelscript, true);

DEFINE_STAT(STAT_AngelscriptStartupBindDatabase);
DEFINE_STAT(STAT_AngelscriptStartupBindScriptTypes);
DEFINE_STAT(STAT_AngelscriptBindsExecuteCallbacks);
DEFINE_STAT(STAT_AngelscriptCompileInitial);
DEFINE_STAT(STAT_AngelscriptCompileModules);
DEFINE_STAT(STAT_AngelscriptReloadHotReload);
DEFINE_STAT(STAT_AngelscriptClassGeneratorSetup);
DEFINE_STAT(STAT_AngelscriptClassGeneratorReload);
DEFINE_STAT(STAT_AngelscriptRuntimeCallBPVMJIT);
DEFINE_STAT(STAT_AngelscriptRuntimeCallParmsContext);
DEFINE_STAT(STAT_AngelscriptStaticJITPrecompiledData);
DEFINE_STAT(STAT_AngelscriptDebugServerTick);
DEFINE_STAT(STAT_AngelscriptDumpAll);
DEFINE_STAT(STAT_AngelscriptCommandletBlueprintImpact);

namespace
{
	const TArray<FName>& GetAngelscriptPerformanceScopeNames()
	{
		static const TArray<FName> ScopeNames = {
			TEXT("Angelscript.Startup.BindDatabase"),
			TEXT("Angelscript.Startup.BindScriptTypes"),
			TEXT("Angelscript.Binds.ExecuteCallbacks"),
			TEXT("Angelscript.Compile.Initial"),
			TEXT("Angelscript.Compile.Modules"),
			TEXT("Angelscript.Reload.HotReload"),
			TEXT("Angelscript.ClassGenerator.Setup"),
			TEXT("Angelscript.ClassGenerator.Reload"),
			TEXT("Angelscript.RuntimeCall.BPVM.JIT"),
			TEXT("Angelscript.RuntimeCall.Parms.Context"),
			TEXT("Angelscript.StaticJIT.PrecompiledData"),
			TEXT("Angelscript.DebugServer.Tick"),
			TEXT("Angelscript.Dump.All"),
			TEXT("Angelscript.Commandlet.BlueprintImpact")
		};
		return ScopeNames;
	}
}

const TArray<FName>& FAngelscriptPerformanceStats::GetKnownScopeNames()
{
	return GetAngelscriptPerformanceScopeNames();
}

#if WITH_DEV_AUTOMATION_TESTS
TArray<FName> FAngelscriptPerformanceStats::GetKnownScopeNamesForTesting()
{
	return GetAngelscriptPerformanceScopeNames();
}
#endif
