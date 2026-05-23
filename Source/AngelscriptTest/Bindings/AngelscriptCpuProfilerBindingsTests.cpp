// =============================================================================
// AngelscriptCpuProfilerBindingsTests.cpp
//
// CQTest coverage for FCpuProfilerTraceScoped binding.
// Validates that the scoped profiler type compiles and can be used in AS.
// Automation IDs: Angelscript.TestModule.Bindings.CpuProfiler.*
// =============================================================================

#include "CQTest.h"
#include "Shared/AngelscriptTestMacros.h"
#include "Shared/AngelscriptBindingsCoverage.h"
#include "Shared/AngelscriptBindingsModuleBuilder.h"
#include "Shared/AngelscriptBindingsAssertions.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptTestSupport;
using namespace AngelscriptTestBindings;

static const FBindingsCoverageProfile GCpuProfilerProfile{
	TEXT("CpuProfiler"),
	TEXT(""),
	TEXT("ASCpuProfiler"),
	TEXT("CpuProfiler"),
	TEXT("CpuProfilerBindings"),
};

TEST_CLASS_WITH_FLAGS(FAngelscriptCpuProfilerBindingsTest,
	"Angelscript.TestModule.Bindings.CpuProfiler",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	BEFORE_ALL()
	{
		ASTEST_CREATE_ENGINE();
	}

	AFTER_ALL() { FAngelscriptEngine& Engine = ASTEST_GET_ENGINE(); ASTEST_RESET_ENGINE(Engine); }

	TEST_METHOD(ScopedUsage)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FCoverageModuleScope Mod(*TestRunner, Engine, GCpuProfilerProfile, TEXT("Scoped"), TEXT(R"(
int ProfilerScope_CompileAndRun()
{
	FCpuProfilerTraceScoped Scope(n"TestScope");
	int Sum = 0;
	for (int I = 0; I < 10; I++)
		Sum += I;
	return Sum;
}
)"));
		if (!Mod.IsValid()) return;
		auto& M = Mod.GetModule();

		ExpectGlobalInt(*TestRunner, Engine, M, GCpuProfilerProfile,
			TEXT("int ProfilerScope_CompileAndRun()"),
			TEXT("FCpuProfilerTraceScoped FName constructor compiles and executes"),
			45); // sum 0..9
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
