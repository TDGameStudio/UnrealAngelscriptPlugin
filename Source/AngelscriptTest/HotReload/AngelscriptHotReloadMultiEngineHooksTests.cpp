// ============================================================================
// AngelscriptHotReloadMultiEngineHooksTests.cpp
//
// Multi-engine isolation coverage for the reload-lifecycle hooks moved from
// the deleted `FAngelscriptClassGenerator` static delegates onto per-engine
// `FAngelscriptEngineHooks` (refactor-as-runtime-deglobalize-completion /
// Sections 4-6).
//
// Pre-fix, all 8 reload hooks (OnClassReload, OnEnumCreated, OnEnumChanged,
// OnStructReload, OnDelegateReload, OnFullReload, OnPostReload,
// OnLiteralAssetReload) were process-wide multicast statics. Subscribers
// always received events from every engine in the process — there was no
// way to scope a listener to a single engine.
//
// Post-fix, each engine owns its own hooks. A listener bound on Engine A's
// hook MUST NOT fire when Engine B broadcasts the same hook, and vice versa.
// These tests assert that observable property by binding handlers on two
// concurrently-alive engines and broadcasting on each engine's hook
// independently.
//
// Automation IDs:
//   Angelscript.TestModule.HotReload.MultiEngineHooks.*
// ============================================================================

#include "AngelscriptEngine.h"
#include "AngelscriptTestEngine.h"
#include "Core/AngelscriptEngineHooks.h"
#include "CQTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace AngelscriptTest_HotReload_MultiEngineHooks_Private
{

// Lightweight counter pair used by every test below: each engine gets a
// FHookCounters instance that tracks how many times each hook fires for that
// engine. Tests then broadcast on one engine's hook and assert only that
// engine's counter advanced.
struct FHookCounters
{
	int32 PostReload = 0;
	int32 FullReload = 0;
	int32 ClassReload = 0;
	int32 StructReload = 0;
	int32 EnumCreated = 0;
	int32 EnumChanged = 0;
	int32 DelegateReload = 0;
	int32 LiteralAssetReload = 0;
};

// Subscribes a counter to every hook on a single engine. RAII unsubscribes on
// destruction so the per-engine hook subscription doesn't outlive the engine.
struct FScopedAllHookSubscriptions
{
	FScopedAllHookSubscriptions(FAngelscriptEngine& InEngine, FHookCounters& InCounters)
		: Engine(&InEngine)
	{
		FAngelscriptEngineHooks& Hooks = InEngine.GetHooks();
		FHookCounters* Counters = &InCounters;

		PostReloadHandle = Hooks.GetOnPostReload().AddLambda(
			[Counters](bool) { ++Counters->PostReload; });
		FullReloadHandle = Hooks.GetOnFullReload().AddLambda(
			[Counters]() { ++Counters->FullReload; });
		ClassReloadHandle = Hooks.GetOnClassReload().AddLambda(
			[Counters](UClass*, UClass*) { ++Counters->ClassReload; });
		StructReloadHandle = Hooks.GetOnStructReload().AddLambda(
			[Counters](UScriptStruct*, UScriptStruct*) { ++Counters->StructReload; });
		EnumCreatedHandle = Hooks.GetOnEnumCreated().AddLambda(
			[Counters](UEnum*) { ++Counters->EnumCreated; });
		EnumChangedHandle = Hooks.GetOnEnumChanged().AddLambda(
			[Counters](UEnum*, EnumNameList) { ++Counters->EnumChanged; });
		DelegateReloadHandle = Hooks.GetOnDelegateReload().AddLambda(
			[Counters](UDelegateFunction*, UDelegateFunction*) { ++Counters->DelegateReload; });
		LiteralAssetReloadHandle = Hooks.GetOnLiteralAssetReload().AddLambda(
			[Counters](UObject*, UObject*) { ++Counters->LiteralAssetReload; });
	}

	~FScopedAllHookSubscriptions()
	{
		if (Engine == nullptr)
		{
			return;
		}
		FAngelscriptEngineHooks& Hooks = Engine->GetHooks();
		Hooks.GetOnPostReload().Remove(PostReloadHandle);
		Hooks.GetOnFullReload().Remove(FullReloadHandle);
		Hooks.GetOnClassReload().Remove(ClassReloadHandle);
		Hooks.GetOnStructReload().Remove(StructReloadHandle);
		Hooks.GetOnEnumCreated().Remove(EnumCreatedHandle);
		Hooks.GetOnEnumChanged().Remove(EnumChangedHandle);
		Hooks.GetOnDelegateReload().Remove(DelegateReloadHandle);
		Hooks.GetOnLiteralAssetReload().Remove(LiteralAssetReloadHandle);
	}

	FAngelscriptEngine* Engine = nullptr;
	FDelegateHandle PostReloadHandle;
	FDelegateHandle FullReloadHandle;
	FDelegateHandle ClassReloadHandle;
	FDelegateHandle StructReloadHandle;
	FDelegateHandle EnumCreatedHandle;
	FDelegateHandle EnumChangedHandle;
	FDelegateHandle DelegateReloadHandle;
	FDelegateHandle LiteralAssetReloadHandle;
};

} // namespace

TEST_CLASS_WITH_FLAGS(FAngelscriptHotReloadMultiEngineHooksTests,
	"Angelscript.TestModule.HotReload.MultiEngineHooks",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	// Two engines alive simultaneously. A listener bound on Engine A's hook
	// MUST NOT fire when Engine B broadcasts the same hook, and a listener on
	// Engine B MUST NOT fire when Engine A broadcasts. Pre-deglobalization
	// these would have been one process-wide multicast and both listeners
	// would always fire.
	TEST_METHOD(BroadcastOnEngineA_DoesNotFireEngineBListeners)
	{
		using namespace AngelscriptTest_HotReload_MultiEngineHooks_Private;

		const FAngelscriptEngineConfig Config;
		const FAngelscriptEngineDependencies Dependencies = FAngelscriptEngineDependencies::CreateDefault();

		TUniquePtr<FAngelscriptEngine> EngineA = FAngelscriptTestEngine::Create(Config, Dependencies);
		TUniquePtr<FAngelscriptEngine> EngineB = FAngelscriptTestEngine::Create(Config, Dependencies);
		if (!TestRunner->TestNotNull(TEXT("MultiEngineHooks: engine A"), EngineA.Get())
			|| !TestRunner->TestNotNull(TEXT("MultiEngineHooks: engine B"), EngineB.Get()))
		{
			return;
		}

		FHookCounters CountersA;
		FHookCounters CountersB;
		FScopedAllHookSubscriptions SubsA(*EngineA, CountersA);
		FScopedAllHookSubscriptions SubsB(*EngineB, CountersB);

		// Broadcast every hook on Engine A. Engine B's counters must stay 0.
		EngineA->GetHooks().GetOnPostReload().Broadcast(false);
		EngineA->GetHooks().GetOnFullReload().Broadcast();
		EngineA->GetHooks().GetOnClassReload().Broadcast(nullptr, nullptr);
		EngineA->GetHooks().GetOnStructReload().Broadcast(nullptr, nullptr);
		EngineA->GetHooks().GetOnEnumCreated().Broadcast(nullptr);
		{
			TArray<TPair<FName, int64>> Names;
			EngineA->GetHooks().GetOnEnumChanged().Broadcast(nullptr, Names);
		}
		EngineA->GetHooks().GetOnDelegateReload().Broadcast(nullptr, nullptr);
		EngineA->GetHooks().GetOnLiteralAssetReload().Broadcast(nullptr, nullptr);

		// Engine A's counters should each be 1.
		TestRunner->TestEqual(TEXT("MultiEngineHooks: A.PostReload"), CountersA.PostReload, 1);
		TestRunner->TestEqual(TEXT("MultiEngineHooks: A.FullReload"), CountersA.FullReload, 1);
		TestRunner->TestEqual(TEXT("MultiEngineHooks: A.ClassReload"), CountersA.ClassReload, 1);
		TestRunner->TestEqual(TEXT("MultiEngineHooks: A.StructReload"), CountersA.StructReload, 1);
		TestRunner->TestEqual(TEXT("MultiEngineHooks: A.EnumCreated"), CountersA.EnumCreated, 1);
		TestRunner->TestEqual(TEXT("MultiEngineHooks: A.EnumChanged"), CountersA.EnumChanged, 1);
		TestRunner->TestEqual(TEXT("MultiEngineHooks: A.DelegateReload"), CountersA.DelegateReload, 1);
		TestRunner->TestEqual(TEXT("MultiEngineHooks: A.LiteralAssetReload"), CountersA.LiteralAssetReload, 1);

		// Engine B's counters must NOT have advanced.
		TestRunner->TestEqual(TEXT("MultiEngineHooks: B.PostReload (no leak)"), CountersB.PostReload, 0);
		TestRunner->TestEqual(TEXT("MultiEngineHooks: B.FullReload (no leak)"), CountersB.FullReload, 0);
		TestRunner->TestEqual(TEXT("MultiEngineHooks: B.ClassReload (no leak)"), CountersB.ClassReload, 0);
		TestRunner->TestEqual(TEXT("MultiEngineHooks: B.StructReload (no leak)"), CountersB.StructReload, 0);
		TestRunner->TestEqual(TEXT("MultiEngineHooks: B.EnumCreated (no leak)"), CountersB.EnumCreated, 0);
		TestRunner->TestEqual(TEXT("MultiEngineHooks: B.EnumChanged (no leak)"), CountersB.EnumChanged, 0);
		TestRunner->TestEqual(TEXT("MultiEngineHooks: B.DelegateReload (no leak)"), CountersB.DelegateReload, 0);
		TestRunner->TestEqual(TEXT("MultiEngineHooks: B.LiteralAssetReload (no leak)"), CountersB.LiteralAssetReload, 0);

		// Reverse direction: broadcast on Engine B, only Engine B counters
		// should advance (pre-existing 1s on A stay at 1).
		EngineB->GetHooks().GetOnPostReload().Broadcast(true);
		EngineB->GetHooks().GetOnFullReload().Broadcast();

		TestRunner->TestEqual(TEXT("MultiEngineHooks: B.PostReload (after B broadcast)"), CountersB.PostReload, 1);
		TestRunner->TestEqual(TEXT("MultiEngineHooks: B.FullReload (after B broadcast)"), CountersB.FullReload, 1);
		TestRunner->TestEqual(TEXT("MultiEngineHooks: A.PostReload (no echo from B)"), CountersA.PostReload, 1);
		TestRunner->TestEqual(TEXT("MultiEngineHooks: A.FullReload (no echo from B)"), CountersA.FullReload, 1);
	}

	// After Engine A is destroyed, broadcasting on Engine B's hook must still
	// work and must not touch any state lingering from Engine A. Pre-fix this
	// scenario could not even arise (one global delegate), but the per-engine
	// model invites teardown-order bugs (e.g. dangling lambda captures); this
	// test guards against that.
	TEST_METHOD(BroadcastOnEngineB_AfterEngineADestroyed_StillWorks)
	{
		using namespace AngelscriptTest_HotReload_MultiEngineHooks_Private;

		const FAngelscriptEngineConfig Config;
		const FAngelscriptEngineDependencies Dependencies = FAngelscriptEngineDependencies::CreateDefault();

		FHookCounters CountersB;

		// Engine A is created, subscribed, then destroyed before B even starts
		// listening. Engine A's subscription must die with Engine A.
		{
			TUniquePtr<FAngelscriptEngine> EngineA = FAngelscriptTestEngine::Create(Config, Dependencies);
			if (!TestRunner->TestNotNull(TEXT("MultiEngineHooks: engine A (transient)"), EngineA.Get()))
			{
				return;
			}

			FHookCounters CountersA;
			FScopedAllHookSubscriptions SubsA(*EngineA, CountersA);

			// One broadcast on A so we know A's path is functional before teardown.
			EngineA->GetHooks().GetOnFullReload().Broadcast();
			TestRunner->TestEqual(TEXT("MultiEngineHooks: A.FullReload pre-teardown"), CountersA.FullReload, 1);

			// SubsA destructor unsubscribes; EngineA destructor releases the
			// hook container.
		}

		// Now create Engine B and verify its hooks fire normally with no
		// residue from Engine A's prior subscription (which has been gone for
		// the lifetime of Engine B).
		TUniquePtr<FAngelscriptEngine> EngineB = FAngelscriptTestEngine::Create(Config, Dependencies);
		if (!TestRunner->TestNotNull(TEXT("MultiEngineHooks: engine B"), EngineB.Get()))
		{
			return;
		}
		FScopedAllHookSubscriptions SubsB(*EngineB, CountersB);

		EngineB->GetHooks().GetOnPostReload().Broadcast(true);
		EngineB->GetHooks().GetOnFullReload().Broadcast();

		TestRunner->TestEqual(TEXT("MultiEngineHooks: B.PostReload after A teardown"), CountersB.PostReload, 1);
		TestRunner->TestEqual(TEXT("MultiEngineHooks: B.FullReload after A teardown"), CountersB.FullReload, 1);
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
