// =============================================================================
// AngelscriptCpuProfilerBindingsTests.cpp
//
// CQTest coverage for FCpuProfilerTraceScoped binding.
// Validates that the scoped profiler type compiles and can be used in AS.
// Automation IDs: Angelscript.TestModule.Bindings.CpuProfiler.*
// =============================================================================

#include "CQTest.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestModuleScope.h"
#include "AngelscriptTestExecute.h"

#if WITH_ANGELSCRIPT_UNITTESTS



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

		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASCpuProfiler_Scoped"), TEXT(R"(
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

		ExpectGlobalInt(*TestRunner, Engine, M, 
			TEXT("int ProfilerScope_CompileAndRun()"),
			TEXT("FCpuProfilerTraceScoped FName constructor compiles and executes"),
			45); // sum 0..9
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
