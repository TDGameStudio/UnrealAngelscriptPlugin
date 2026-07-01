#include "AngelscriptEngine.h"
#include "AngelscriptTestEngine.h"
#include "CQTest.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_scriptengine.h"
#include "EndAngelscriptHeaders.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FAngelscriptTestEngineLifecycleTests,
	"Angelscript.TestModule.Engine.TestEngine",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
// Capture the pointer before reset so we can assert "the engine was real,
// then it went away" symbolically. We do NOT dereference the captured
// pointer post-reset �?that would be UB. The pointer is used only to
// (a) assert non-null pre-reset and (b) document the lifetime boundary
// in test logs.
struct FCapturedEnginePointers
{
asIScriptEngine* ScriptEngine = nullptr;
};

static FCapturedEnginePointers CapturePointers(FAngelscriptEngine& Engine)
{
FCapturedEnginePointers Captured;
Captured.ScriptEngine = Engine.GetScriptEngine();
return Captured;
}

public:
	// The single-owner shutdown contract: every test engine is the sole owner
	// of its shared state, so destroying the TUniquePtr must complete teardown
	// synchronously �?no deferred-release handshake. The observable contract:
	// (a) the first reset returns synchronously without deadlock, and (b) two
	// engines held simultaneously have distinct underlying asCScriptEngine
	// objects.
	//
	// Note: we do NOT compare a captured pre-reset pointer against a
	// post-reset newly-allocated pointer �?the OS allocator is free to
	// reuse the just-freed address for the next allocation, so that
	// comparison is intrinsically flaky. Holding both engines at the
	// same time gives a deterministic distinctness check.
	TEST_METHOD(SharedStateReleaseIsImmediate)
	{
FAngelscriptEngineConfig Config;
		const FAngelscriptEngineDependencies Dependencies = FAngelscriptEngineDependencies::CreateDefault();

		// Hold both engines simultaneously: the heap can't give them the
		// same address while both are alive, so a distinctness check is
		// deterministic.
		TUniquePtr<FAngelscriptEngine> EngineA = FAngelscriptTestEngine::Create(Config, Dependencies);
		TUniquePtr<FAngelscriptEngine> EngineB = FAngelscriptTestEngine::Create(Config, Dependencies);
		ASSERT_THAT(IsNotNull(EngineA.Get(), TEXT("TestEngine.SharedStateReleaseIsImmediate should construct engine A")));
		ASSERT_THAT(IsNotNull(EngineB.Get(), TEXT("TestEngine.SharedStateReleaseIsImmediate should construct engine B")));

		asIScriptEngine* ScriptEngineA = EngineA->GetScriptEngine();
		asIScriptEngine* ScriptEngineB = EngineB->GetScriptEngine();
		ASSERT_THAT(IsNotNull(ScriptEngineA, TEXT("TestEngine.SharedStateReleaseIsImmediate engine A should expose a non-null asIScriptEngine")));
		ASSERT_THAT(IsNotNull(ScriptEngineB, TEXT("TestEngine.SharedStateReleaseIsImmediate engine B should expose a non-null asIScriptEngine")));
		ASSERT_THAT(IsTrue(
			ScriptEngineA != ScriptEngineB,
			TEXT("TestEngine.SharedStateReleaseIsImmediate two simultaneously-held engines should have distinct asIScriptEngine objects")));

		// Synchronous teardown: dropping engine A must complete without
		// any coordination handshake. If the destructor blocked on a
		// deferred-release path, execution would deadlock or emit a
		// "release deferred" log. Reaching the next assertion proves the
		// synchronous-shutdown contract holds.
		EngineA.Reset();

		// Engine B must remain fully functional after engine A's teardown
		// �?its asCScriptEngine pointer is unchanged, and its database
		// fields still answer non-null. This is the "no cross-engine
		// contamination" half of the contract.
		ASSERT_THAT(AreEqual(
			static_cast<void*>(ScriptEngineB),
			static_cast<void*>(EngineB->GetScriptEngine()),
			TEXT("TestEngine.SharedStateReleaseIsImmediate engine B's script engine should be unchanged after engine A is reset")));
		ASSERT_THAT(IsNotNull(
			EngineB->GetTypeDatabase(),
			TEXT("TestEngine.SharedStateReleaseIsImmediate engine B's TypeDatabase should still be reachable after engine A is reset")));
	}

	// Documents that DestroySharedEngine() is reentrant-safe and the
	// next GetSharedEngine() call constructs a fresh, working engine.
	// Together with SharedStateReleaseIsImmediate, this covers the full
	// "single-owner, synchronous teardown" contract for the shared
	// test engine singleton.
	//
	// Note: we deliberately do NOT compare the pre-destroy pointer
	// against the post-recreate pointer �?the OS allocator is free to
	// reuse the just-freed address, making such a comparison flaky.
	// The observable contract here is "destroy + recreate produces a
	// usable engine", which is what we assert.
	TEST_METHOD(DestroySharedEngineThenReconstructProducesFreshEngine)
	{
		FAngelscriptTestEngine::DestroySharedEngine();
		FAngelscriptEngine& First = FAngelscriptTestEngine::GetSharedEngine();
		ASSERT_THAT(IsNotNull(
			static_cast<void*>(First.GetScriptEngine()),
			TEXT("TestEngine.DestroySharedEngineThenReconstruct should expose a non-null asIScriptEngine on the first shared engine")));
		// First engine's TypeDatabase is live �?proves Initialize* fully
		// wired the inlined database fields after construction.
		ASSERT_THAT(IsNotNull(
			First.GetTypeDatabase(),
			TEXT("TestEngine.DestroySharedEngineThenReconstruct first shared engine should have a non-null TypeDatabase")));

		FAngelscriptTestEngine::DestroySharedEngine();

		FAngelscriptEngine& Second = FAngelscriptTestEngine::GetSharedEngine();
		ASSERT_THAT(IsNotNull(
			static_cast<void*>(Second.GetScriptEngine()),
			TEXT("TestEngine.DestroySharedEngineThenReconstruct should expose a non-null asIScriptEngine on the recreated shared engine")));
		ASSERT_THAT(IsNotNull(
			Second.GetTypeDatabase(),
			TEXT("TestEngine.DestroySharedEngineThenReconstruct recreated shared engine should have a non-null TypeDatabase")));

		// Leave the harness in a known-clean state for whichever test runs next.
		FAngelscriptTestEngine::DestroySharedEngine();
	}
};

#endif
