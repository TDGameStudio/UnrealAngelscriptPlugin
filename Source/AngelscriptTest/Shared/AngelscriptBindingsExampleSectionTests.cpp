#include "AngelscriptBindingsExampleSection.h"

#include "AngelscriptTestEngineHelper.h"
#include "AngelscriptTestUtilities.h"
#include "AngelscriptTestMacros.h"

#include "Misc/ScopeExit.h"

/**
 * AngelscriptBindingsExampleSectionTests — registers a single Automation ID
 * that drives `RunBindingsExampleSection` end-to-end. This is the
 * "self-test" for the Coverage Section base layer: if this test stays
 * green across the rest of the refactor, the base layer is sound.
 *
 * The Automation ID is intentionally namespaced under
 * `Bindings.SharedExample` (not `Bindings.Example` or any topic name) so
 * that test discovery clearly distinguishes it from real coverage tests.
 *
 * Per the main plan's "保留旧 ID" rule this test does NOT replace any
 * existing ID — it is purely additive, paid back by zero-cost validation
 * of the base layer.
 */

#if WITH_DEV_AUTOMATION_TESTS

// Phase 5 of refactor-as-test-shared-layout-and-naming removes the old
// Bindings coverage profile. The example test resolves call-side helpers
// through namespace `AngelscriptTest`, while module lifetime stays in the
// explicit FCoverageModuleScope module-name API used by the example header.
// `AngelscriptTestSupport::` is still pulled in for engine-helper macros that
// drive the fixture (`ASTEST_CREATE_ENGINE` / `ASTEST_RESET_ENGINE` resolve
// against it), since those belong to the engine-lifecycle layer.
using namespace AngelscriptTestSupport;
using namespace AngelscriptTest;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptBindingsSharedExampleTest,
	"Angelscript.TestModule.Bindings.SharedExample",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptBindingsSharedExampleTest::RunTest(const FString& Parameters)
{
	bool bPassed = true;
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
	{ FAngelscriptEngineScope _AutoEngineScope(Engine);
	ON_SCOPE_EXIT { ASTEST_RESET_ENGINE(Engine); };


	bPassed &= RunBindingsExampleSection(*this, Engine);

	}
	return bPassed;
}

#endif // WITH_DEV_AUTOMATION_TESTS
