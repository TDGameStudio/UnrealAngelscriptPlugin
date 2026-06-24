// ============================================================================
// AngelscriptHotReloadMultiEngineHooksTests.cpp
//
// Multi-engine isolation coverage for the reload-lifecycle hooks moved from
// the deleted `FAngelscriptClassGenerator` static delegates onto per-engine
// FAngelscriptEngine accessors (refactor-as-runtime-deglobalize-completion /
// Sections 4-6).
//
// Pre-fix, all 8 reload hooks (OnClassReload, OnEnumCreated, OnEnumChanged,
// OnStructReload, OnDelegateReload, OnFullReload, OnPostReload,
// OnLiteralAssetReload) were process-wide multicast statics. Subscribers
// always received events from every engine in the process â€?there was no
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
#include "CQTest.h"

#if WITH_DEV_AUTOMATION_TESTS

 // namespace

TEST_CLASS_WITH_FLAGS(FAngelscriptHotReloadMultiEngineHooksTests,
	"Angelscript.TestModule.HotReload.MultiEngineHooks",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
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
	FHookCounters* Counters = &InCounters;

	PostReloadHandle = InEngine.GetOnPostReload().AddLambda(
		[Counters](bool) { ++Counters->PostReload; });
	FullReloadHandle = InEngine.GetOnFullReload().AddLambda(
		[Counters]() { ++Counters->FullReload; });
	ClassReloadHandle = InEngine.GetOnClassReload().AddLambda(
		[Counters](UClass*, UClass*) { ++Counters->ClassReload; });
	StructReloadHandle = InEngine.GetOnStructReload().AddLambda(
		[Counters](UScriptStruct*, UScriptStruct*) { ++Counters->StructReload; });
	EnumCreatedHandle = InEngine.GetOnEnumCreated().AddLambda(
		[Counters](UEnum*) { ++Counters->EnumCreated; });
	EnumChangedHandle = InEngine.GetOnEnumChanged().AddLambda(
		[Counters](UEnum*, EnumNameList) { ++Counters->EnumChanged; });
	DelegateReloadHandle = InEngine.GetOnDelegateReload().AddLambda(
		[Counters](UDelegateFunction*, UDelegateFunction*) { ++Counters->DelegateReload; });
	LiteralAssetReloadHandle = InEngine.GetOnLiteralAssetReload().AddLambda(
		[Counters](UObject*, UObject*) { ++Counters->LiteralAssetReload; });
}

~FScopedAllHookSubscriptions()
{
	if (Engine == nullptr)
	{
		return;
	}
	Engine->GetOnPostReload().Remove(PostReloadHandle);
	Engine->GetOnFullReload().Remove(FullReloadHandle);
	Engine->GetOnClassReload().Remove(ClassReloadHandle);
	Engine->GetOnStructReload().Remove(StructReloadHandle);
	Engine->GetOnEnumCreated().Remove(EnumCreatedHandle);
	Engine->GetOnEnumChanged().Remove(EnumChangedHandle);
	Engine->GetOnDelegateReload().Remove(DelegateReloadHandle);
	Engine->GetOnLiteralAssetReload().Remove(LiteralAssetReloadHandle);
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

public:
	// Two engines alive simultaneously. A listener bound on Engine A's hook
	// MUST NOT fire when Engine B broadcasts the same hook, and a listener on
	// Engine B MUST NOT fire when Engine A broadcasts. Pre-deglobalization
	// these would have been one process-wide multicast and both listeners
	// would always fire.
	TEST_METHOD(BroadcastOnEngineA_DoesNotFireEngineBListeners)
	{
const FAngelscriptEngineConfig Config;
		const FAngelscriptEngineDependencies Dependencies = FAngelscriptEngineDependencies::CreateDefault();

		TUniquePtr<FAngelscriptEngine> EngineA = FAngelscriptTestEngine::Create(Config, Dependencies);
		TUniquePtr<FAngelscriptEngine> EngineB = FAngelscriptTestEngine::Create(Config, Dependencies);
		ASSERT_THAT(IsNotNull(EngineA.Get(), TEXT("MultiEngineHooks: engine A")));
		ASSERT_THAT(IsNotNull(EngineB.Get(), TEXT("MultiEngineHooks: engine B")));

		FHookCounters CountersA;
		FHookCounters CountersB;
		FScopedAllHookSubscriptions SubsA(*EngineA, CountersA);
		FScopedAllHookSubscriptions SubsB(*EngineB, CountersB);

		// Broadcast every hook on Engine A. Engine B's counters must stay 0.
		EngineA->GetOnPostReload().Broadcast(false);
		EngineA->GetOnFullReload().Broadcast();
		EngineA->GetOnClassReload().Broadcast(nullptr, nullptr);
		EngineA->GetOnStructReload().Broadcast(nullptr, nullptr);
		EngineA->GetOnEnumCreated().Broadcast(nullptr);
		{
			TArray<TPair<FName, int64>> Names;
			EngineA->GetOnEnumChanged().Broadcast(nullptr, Names);
		}
		EngineA->GetOnDelegateReload().Broadcast(nullptr, nullptr);
		EngineA->GetOnLiteralAssetReload().Broadcast(nullptr, nullptr);

		// Engine A's counters should each be 1.
		ASSERT_THAT(AreEqual(1, CountersA.PostReload, TEXT("MultiEngineHooks: A.PostReload")));
		ASSERT_THAT(AreEqual(1, CountersA.FullReload, TEXT("MultiEngineHooks: A.FullReload")));
		ASSERT_THAT(AreEqual(1, CountersA.ClassReload, TEXT("MultiEngineHooks: A.ClassReload")));
		ASSERT_THAT(AreEqual(1, CountersA.StructReload, TEXT("MultiEngineHooks: A.StructReload")));
		ASSERT_THAT(AreEqual(1, CountersA.EnumCreated, TEXT("MultiEngineHooks: A.EnumCreated")));
		ASSERT_THAT(AreEqual(1, CountersA.EnumChanged, TEXT("MultiEngineHooks: A.EnumChanged")));
		ASSERT_THAT(AreEqual(1, CountersA.DelegateReload, TEXT("MultiEngineHooks: A.DelegateReload")));
		ASSERT_THAT(AreEqual(1, CountersA.LiteralAssetReload, TEXT("MultiEngineHooks: A.LiteralAssetReload")));

		// Engine B's counters must NOT have advanced.
		ASSERT_THAT(AreEqual(0, CountersB.PostReload, TEXT("MultiEngineHooks: B.PostReload (no leak)")));
		ASSERT_THAT(AreEqual(0, CountersB.FullReload, TEXT("MultiEngineHooks: B.FullReload (no leak)")));
		ASSERT_THAT(AreEqual(0, CountersB.ClassReload, TEXT("MultiEngineHooks: B.ClassReload (no leak)")));
		ASSERT_THAT(AreEqual(0, CountersB.StructReload, TEXT("MultiEngineHooks: B.StructReload (no leak)")));
		ASSERT_THAT(AreEqual(0, CountersB.EnumCreated, TEXT("MultiEngineHooks: B.EnumCreated (no leak)")));
		ASSERT_THAT(AreEqual(0, CountersB.EnumChanged, TEXT("MultiEngineHooks: B.EnumChanged (no leak)")));
		ASSERT_THAT(AreEqual(0, CountersB.DelegateReload, TEXT("MultiEngineHooks: B.DelegateReload (no leak)")));
		ASSERT_THAT(AreEqual(0, CountersB.LiteralAssetReload, TEXT("MultiEngineHooks: B.LiteralAssetReload (no leak)")));

		// Reverse direction: broadcast on Engine B, only Engine B counters
		// should advance (pre-existing 1s on A stay at 1).
		EngineB->GetOnPostReload().Broadcast(true);
		EngineB->GetOnFullReload().Broadcast();

		ASSERT_THAT(AreEqual(1, CountersB.PostReload, TEXT("MultiEngineHooks: B.PostReload (after B broadcast)")));
		ASSERT_THAT(AreEqual(1, CountersB.FullReload, TEXT("MultiEngineHooks: B.FullReload (after B broadcast)")));
		ASSERT_THAT(AreEqual(1, CountersA.PostReload, TEXT("MultiEngineHooks: A.PostReload (no echo from B)")));
		ASSERT_THAT(AreEqual(1, CountersA.FullReload, TEXT("MultiEngineHooks: A.FullReload (no echo from B)")));
	}

	// After Engine A is destroyed, broadcasting on Engine B's hook must still
	// work and must not touch any state lingering from Engine A. Pre-fix this
	// scenario could not even arise (one global delegate), but the per-engine
	// model invites teardown-order bugs (e.g. dangling lambda captures); this
	// test guards against that.
	TEST_METHOD(BroadcastOnEngineB_AfterEngineADestroyed_StillWorks)
	{
const FAngelscriptEngineConfig Config;
		const FAngelscriptEngineDependencies Dependencies = FAngelscriptEngineDependencies::CreateDefault();

		FHookCounters CountersB;

		// Engine A is created, subscribed, then destroyed before B even starts
		// listening. Engine A's subscription must die with Engine A.
		{
			TUniquePtr<FAngelscriptEngine> EngineA = FAngelscriptTestEngine::Create(Config, Dependencies);
			ASSERT_THAT(IsNotNull(EngineA.Get(), TEXT("MultiEngineHooks: engine A (transient)")));

			FHookCounters CountersA;
			FScopedAllHookSubscriptions SubsA(*EngineA, CountersA);

			// One broadcast on A so we know A's path is functional before teardown.
			EngineA->GetOnFullReload().Broadcast();
			ASSERT_THAT(AreEqual(1, CountersA.FullReload, TEXT("MultiEngineHooks: A.FullReload pre-teardown")));

			// SubsA destructor unsubscribes; EngineA destructor releases the
			// hook container.
		}

		// Now create Engine B and verify its hooks fire normally with no
		// residue from Engine A's prior subscription (which has been gone for
		// the lifetime of Engine B).
		TUniquePtr<FAngelscriptEngine> EngineB = FAngelscriptTestEngine::Create(Config, Dependencies);
		ASSERT_THAT(IsNotNull(EngineB.Get(), TEXT("MultiEngineHooks: engine B")));
		FScopedAllHookSubscriptions SubsB(*EngineB, CountersB);

		EngineB->GetOnPostReload().Broadcast(true);
		EngineB->GetOnFullReload().Broadcast();

		ASSERT_THAT(AreEqual(1, CountersB.PostReload, TEXT("MultiEngineHooks: B.PostReload after A teardown")));
		ASSERT_THAT(AreEqual(1, CountersB.FullReload, TEXT("MultiEngineHooks: B.FullReload after A teardown")));
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
