#pragma once

#include "CoreMinimal.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "ProfilingDebugging/CsvProfiler.h"
#include "Stats/Stats.h"

DECLARE_STATS_GROUP(TEXT("Angelscript"), STATGROUP_Angelscript, STATCAT_Advanced);

CSV_DECLARE_CATEGORY_MODULE_EXTERN(ANGELSCRIPTRUNTIME_API, Angelscript);

DECLARE_CYCLE_STAT_EXTERN(TEXT("Angelscript.Startup.BindDatabase"), STAT_AngelscriptStartupBindDatabase, STATGROUP_Angelscript, ANGELSCRIPTRUNTIME_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("Angelscript.Startup.BindScriptTypes"), STAT_AngelscriptStartupBindScriptTypes, STATGROUP_Angelscript, ANGELSCRIPTRUNTIME_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("Angelscript.Binds.ExecuteCallbacks"), STAT_AngelscriptBindsExecuteCallbacks, STATGROUP_Angelscript, ANGELSCRIPTRUNTIME_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("Angelscript.Compile.Initial"), STAT_AngelscriptCompileInitial, STATGROUP_Angelscript, ANGELSCRIPTRUNTIME_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("Angelscript.Compile.Modules"), STAT_AngelscriptCompileModules, STATGROUP_Angelscript, ANGELSCRIPTRUNTIME_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("Angelscript.Reload.HotReload"), STAT_AngelscriptReloadHotReload, STATGROUP_Angelscript, ANGELSCRIPTRUNTIME_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("Angelscript.ClassGenerator.Setup"), STAT_AngelscriptClassGeneratorSetup, STATGROUP_Angelscript, ANGELSCRIPTRUNTIME_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("Angelscript.ClassGenerator.Reload"), STAT_AngelscriptClassGeneratorReload, STATGROUP_Angelscript, ANGELSCRIPTRUNTIME_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("Angelscript.RuntimeCall.BPVM.JIT"), STAT_AngelscriptRuntimeCallBPVMJIT, STATGROUP_Angelscript, ANGELSCRIPTRUNTIME_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("Angelscript.RuntimeCall.Parms.Context"), STAT_AngelscriptRuntimeCallParmsContext, STATGROUP_Angelscript, ANGELSCRIPTRUNTIME_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("Angelscript.StaticJIT.PrecompiledData"), STAT_AngelscriptStaticJITPrecompiledData, STATGROUP_Angelscript, ANGELSCRIPTRUNTIME_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("Angelscript.DebugServer.Tick"), STAT_AngelscriptDebugServerTick, STATGROUP_Angelscript, ANGELSCRIPTRUNTIME_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("Angelscript.Dump.All"), STAT_AngelscriptDumpAll, STATGROUP_Angelscript, ANGELSCRIPTRUNTIME_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("Angelscript.Commandlet.BlueprintImpact"), STAT_AngelscriptCommandletBlueprintImpact, STATGROUP_Angelscript, ANGELSCRIPTRUNTIME_API);

struct ANGELSCRIPTRUNTIME_API FAngelscriptPerformanceStats
{
	static const TArray<FName>& GetKnownScopeNames();

#if WITH_DEV_AUTOMATION_TESTS
	static TArray<FName> GetKnownScopeNamesForTesting();
#endif
};

#define ANGELSCRIPT_PERF_SCOPE(ScopeNameLiteral, StatId, CsvStatName) \
	TRACE_CPUPROFILER_EVENT_SCOPE_STR(ScopeNameLiteral); \
	SCOPE_CYCLE_COUNTER(StatId); \
	CSV_SCOPED_TIMING_STAT(Angelscript, CsvStatName)

#define AS_PERF_SCOPE_STARTUP_BIND_DATABASE() ANGELSCRIPT_PERF_SCOPE("Angelscript.Startup.BindDatabase", STAT_AngelscriptStartupBindDatabase, Startup_BindDatabase)
#define AS_PERF_SCOPE_STARTUP_BIND_SCRIPT_TYPES() ANGELSCRIPT_PERF_SCOPE("Angelscript.Startup.BindScriptTypes", STAT_AngelscriptStartupBindScriptTypes, Startup_BindScriptTypes)
#define AS_PERF_SCOPE_BINDS_EXECUTE_CALLBACKS() ANGELSCRIPT_PERF_SCOPE("Angelscript.Binds.ExecuteCallbacks", STAT_AngelscriptBindsExecuteCallbacks, Binds_ExecuteCallbacks)
#define AS_PERF_SCOPE_COMPILE_INITIAL() ANGELSCRIPT_PERF_SCOPE("Angelscript.Compile.Initial", STAT_AngelscriptCompileInitial, Compile_Initial)
#define AS_PERF_SCOPE_COMPILE_MODULES() ANGELSCRIPT_PERF_SCOPE("Angelscript.Compile.Modules", STAT_AngelscriptCompileModules, Compile_Modules)
#define AS_PERF_SCOPE_RELOAD_HOT_RELOAD() ANGELSCRIPT_PERF_SCOPE("Angelscript.Reload.HotReload", STAT_AngelscriptReloadHotReload, Reload_HotReload)
#define AS_PERF_SCOPE_CLASS_GENERATOR_SETUP() ANGELSCRIPT_PERF_SCOPE("Angelscript.ClassGenerator.Setup", STAT_AngelscriptClassGeneratorSetup, ClassGenerator_Setup)
#define AS_PERF_SCOPE_CLASS_GENERATOR_RELOAD() ANGELSCRIPT_PERF_SCOPE("Angelscript.ClassGenerator.Reload", STAT_AngelscriptClassGeneratorReload, ClassGenerator_Reload)
#define AS_PERF_SCOPE_RUNTIME_CALL_BPVM_JIT() ANGELSCRIPT_PERF_SCOPE("Angelscript.RuntimeCall.BPVM.JIT", STAT_AngelscriptRuntimeCallBPVMJIT, RuntimeCall_BPVM_JIT)
#define AS_PERF_SCOPE_RUNTIME_CALL_PARMS_CONTEXT() ANGELSCRIPT_PERF_SCOPE("Angelscript.RuntimeCall.Parms.Context", STAT_AngelscriptRuntimeCallParmsContext, RuntimeCall_Parms_Context)
#define AS_PERF_SCOPE_STATIC_JIT_PRECOMPILED_DATA() ANGELSCRIPT_PERF_SCOPE("Angelscript.StaticJIT.PrecompiledData", STAT_AngelscriptStaticJITPrecompiledData, StaticJIT_PrecompiledData)
#define AS_PERF_SCOPE_DEBUG_SERVER_TICK() ANGELSCRIPT_PERF_SCOPE("Angelscript.DebugServer.Tick", STAT_AngelscriptDebugServerTick, DebugServer_Tick)
#define AS_PERF_SCOPE_DUMP_ALL() ANGELSCRIPT_PERF_SCOPE("Angelscript.Dump.All", STAT_AngelscriptDumpAll, Dump_All)
#define AS_PERF_SCOPE_COMMANDLET_BLUEPRINT_IMPACT() ANGELSCRIPT_PERF_SCOPE("Angelscript.Commandlet.BlueprintImpact", STAT_AngelscriptCommandletBlueprintImpact, Commandlet_BlueprintImpact)
