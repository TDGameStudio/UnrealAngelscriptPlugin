#pragma once

#include "CoreMinimal.h"
#include "AngelscriptEngine.h"
#include "Misc/AutomationTest.h"
#include "AngelscriptTestModuleScope.h"
#include "AngelscriptTestExecute.h"

/**
 * AngelscriptBindingsExampleSection — copy-paste starting point for writing
 * Coverage Section tests using the CQTest pattern.
 *
 * Canonical naming reference (post refactor-as-test-shared-layout-and-naming
 * Phase 4): this file is the official demonstration of the *new* call-side
 * API surface. New Coverage Sections should mirror the shape used here:
 *
 *   - namespace `` is the primary entry point for the
 *     C++-side AS invocation API (Executor + Execute* assertion family).
 *   - `ExecuteAndExpectInt` (Bool / Double / NearFloat /
 *     NearDouble / IntAtLeast / Validate<T> / ...) replaces the legacy
 *     `ExpectGlobal*` helpers; signatures are
 *     identical so migration is a pure rename. The old names still work
 *     via inline forwarders for source compatibility.
 *   - `ExecuteBatchAndExpectInt` + the `FExpectedInt`
 *     row struct replace `ExpectGlobalInts` + `FExpectedGlobalInt`.
 *   - `FAngelscriptTestExecutor` (with `.Execute()` /
 *     `.ExecuteAndGet<R>()` / `.ExecuteAndExtractStruct<T>()`) replaces
 *     `FASGlobalFunctionInvoker`. The old
 *     class name resolves through a `using` alias.
 *
 * The companion file `AngelscriptBindingsExampleSectionTests.cpp` registers
 * an Automation ID that runs this section end-to-end, proving the base
 * layer is sound. Per the main plan's "保留旧 ID" rule this test does NOT
 * replace any existing ID; it is purely additive.
 *
 * Pattern executors should mirror, NOT include, this file in their own
 * SubPlan implementation. This header lives only to be read as documentation.
 */

#if WITH_ANGELSCRIPT_UNITTESTS

// FScopedAngelscriptModule (AngelscriptTestModuleScope.h) is the RAII module
// wrapper used by every Section. Include that header directly when you need
// module lifetime without pulling the full Execute surface.

/** Run the example section and return aggregate pass/fail. */
inline bool RunBindingsExampleSection(
	FAutomationTestBase& Test,
	FAngelscriptEngine& Engine)
{
	// Each case is a no-arg `int F()` that asserts a single behavior and
	// returns 0 / 1 (or a small int). Keep the script side dumb -- all
	// branching/fallback logic stays here in C++ where the assertion
	// names live.
	FScopedAngelscriptModule ModuleScope(Test, Engine, TEXT("ASBindingsSharedExample_Example"), TEXT(R"(
int EchoZero()           { return 0; }
int EchoOne()            { return 1; }
int EchoSum()            { int A = 17; int B = 25; return A + B; }
int EchoMaxOf(int A, int B) { return A > B ? A : B; }

// Container case -- exercises a real bound type to prove the basics work
// against the actual Bindings layer (and to give the example a more
// realistic shape than pure arithmetic).
int CountFruits()
{
TArray<FString> Fruits;
Fruits.Add("apple");
Fruits.Add("banana");
Fruits.Add("cherry");
return Fruits.Num();
}
)"));
	if (!ModuleScope.IsValid())
	{
		return false;
	}
	asIScriptModule& Module = ModuleScope.GetModule();

	bool bPassed = true;
	bPassed &= ExecuteAndExpectInt(Test, Engine, Module,
		TEXT("int EchoZero()"), TEXT("EchoZero returns 0"), 0);
	bPassed &= ExecuteAndExpectInt(Test, Engine, Module,
		TEXT("int EchoOne()"), TEXT("EchoOne returns 1"), 1);
	bPassed &= ExecuteAndExpectInt(Test, Engine, Module,
		TEXT("int EchoSum()"), TEXT("EchoSum returns 17 + 25"), 42);
	bPassed &= ExecuteAndExpectInt(Test, Engine, Module,
		TEXT("int CountFruits()"), TEXT("CountFruits builds and counts a TArray<FString>"), 3);

	// Batched form -- useful when a section has many homogeneous cases.
	const FExpectedInt Cases[] = {
		{ TEXT("int EchoZero()"), TEXT("Batched EchoZero baseline"), 0 },
		{ TEXT("int EchoOne()"),  TEXT("Batched EchoOne baseline"),  1 },
	};
	bPassed &= ExecuteBatchAndExpectInt(Test, Engine, Module, Cases);

	return bPassed;
}


#endif // WITH_ANGELSCRIPT_UNITTESTS
