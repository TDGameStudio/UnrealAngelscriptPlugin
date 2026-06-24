#include "AngelscriptEngine.h"
#include "AngelscriptBinds.h"
#include "CQTest.h"
#include "Misc/Guid.h"
#include "Misc/ScopeExit.h"
#include "AngelscriptTestEngine.h"
#include "Testing/AngelscriptBindExecutionObservation.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_scriptengine.h"
#include "source/as_module.h"
#include "EndAngelscriptHeaders.h"

#if WITH_DEV_AUTOMATION_TESTS

struct FAngelscriptMultiEngineTestAccess
{
	static void DestroyGlobalEngine()
	{
		FAngelscriptEngine::DestroyGlobal();
	}

	static FAngelscriptEngine* GetGlobalEngine()
	{
		return FAngelscriptEngine::TryGetGlobalEngine();
	}

	static FString MakeModuleName(const FAngelscriptEngine& Engine, const FString& ModuleName)
	{
		return Engine.MakeModuleName(ModuleName);
	}

	static asIScriptModule* CreateNamedModule(FAngelscriptEngine& Engine, const FString& ModuleName)
	{
		return Engine.Engine->GetModule(TCHAR_TO_ANSI(*Engine.MakeModuleName(ModuleName)), asGM_ALWAYS_CREATE);
	}

	static asIScriptModule* FindNamedModule(FAngelscriptEngine& Engine, const FString& ModuleName)
	{
		return Engine.Engine->GetModule(TCHAR_TO_ANSI(*Engine.MakeModuleName(ModuleName)), asGM_ONLY_IF_EXISTS);
	}

	static void TrackNamedModule(FAngelscriptEngine& Engine, const FString& ModuleName, asIScriptModule* ScriptModule)
	{
		TSharedRef<FAngelscriptModuleDesc> ModuleDesc = MakeShared<FAngelscriptModuleDesc>();
		ModuleDesc->ModuleName = ModuleName;
		ModuleDesc->ScriptModule = static_cast<asCModule*>(ScriptModule);
		Engine.ActiveModules.Add(Engine.MakeModuleName(ModuleName), ModuleDesc);
		Engine.ModulesByScriptModule.Add(ScriptModule, ModuleDesc);
	}

	static int32 GetLocalPooledContextCount(asIScriptEngine* ScriptEngine)
	{
		return FAngelscriptEngine::GetLocalPooledContextCountForTesting(ScriptEngine);
	}
};

TEST_CLASS_WITH_FLAGS(FAngelscriptMultiEngineLifecycleTests,
	"Angelscript.TestModule.Engine.MultiEngine",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
struct FMultiEngineContextStackGuard
{
TArray<FAngelscriptEngine*> SavedStack;
FMultiEngineContextStackGuard()
{
	SavedStack = FAngelscriptEngineContextStack::SnapshotAndClear();
}
~FMultiEngineContextStackGuard()
{
	FAngelscriptEngineContextStack::RestoreSnapshot(MoveTemp(SavedStack));
}
};

static void ResetToIsolatedEngineState()
{
if (FAngelscriptEngine::IsInitialized())
{
	FAngelscriptMultiEngineTestAccess::DestroyGlobalEngine();
}
}

static FName MakeUniqueStartupBindName(const TCHAR* Prefix)
{
return FName(*FString::Printf(TEXT("%s.%s"), Prefix, *FGuid::NewGuid().ToString(EGuidFormats::Digits)));
}

static bool RunCloneModuleIsolation(FAutomationTestBase& Test)
{
FNoDiscardAsserter LocalAssert(Test);
// Note: the historic name is preserved while the test method name in
// the TEST_CLASS still references "CloneModuleIsolation" so test report
// archives stay greppable. The semantics are now "two independent test
// engines must keep their script modules from leaking into each other"
// �?the "Clone" terminology is historical.
ResetToIsolatedEngineState();

const FString ModuleName = TEXT("Tests.SharedModule");
const FAngelscriptEngineConfig Config;
const FAngelscriptEngineDependencies Dependencies = FAngelscriptEngineDependencies::CreateDefault();
TUniquePtr<FAngelscriptEngine> EngineA = FAngelscriptTestEngine::Create(Config, Dependencies);
TUniquePtr<FAngelscriptEngine> EngineB = FAngelscriptTestEngine::Create(Config, Dependencies);

if (!LocalAssert.IsNotNull(EngineA.Get(), TEXT("MultiEngine.IndependentEngines should create the first test engine"))
	|| !LocalAssert.IsNotNull(EngineB.Get(), TEXT("MultiEngine.IndependentEngines should create the second test engine")))
{
	return false;
}

asIScriptModule* EngineAModule = FAngelscriptMultiEngineTestAccess::CreateNamedModule(*EngineA, ModuleName);
asIScriptModule* EngineBModule = FAngelscriptMultiEngineTestAccess::CreateNamedModule(*EngineB, ModuleName);
FAngelscriptMultiEngineTestAccess::TrackNamedModule(*EngineA, ModuleName, EngineAModule);
FAngelscriptMultiEngineTestAccess::TrackNamedModule(*EngineB, ModuleName, EngineBModule);

bool bOk = true;
bOk &= LocalAssert.IsNotNull(EngineAModule, TEXT("MultiEngine.IndependentEngines should create the first underlying script module"));
bOk &= LocalAssert.IsNotNull(EngineBModule, TEXT("MultiEngine.IndependentEngines should create the second underlying script module"));
bOk &= LocalAssert.IsTrue(EngineA->GetModuleByModuleName(ModuleName).IsValid(), TEXT("MultiEngine.IndependentEngines should keep external lookup working for engine A"));
bOk &= LocalAssert.IsTrue(EngineB->GetModuleByModuleName(ModuleName).IsValid(), TEXT("MultiEngine.IndependentEngines should keep external lookup working for engine B"));

// The structural invariant: distinct asCScriptEngine instances allocate
// their modules independently, so two engines compiling under the same
// module name must produce distinct underlying objects.
bOk &= LocalAssert.IsTrue(EngineAModule != EngineBModule, TEXT("MultiEngine.IndependentEngines should produce distinct underlying script modules per engine"));

// Cross-engine lookup must miss: if engine A discards its module, engine B's
// module is still reachable via engine B and the asCScriptEngine of B never
// learns about engine A's discard.
EngineA->DiscardModule(*ModuleName);
bOk &= LocalAssert.IsFalse(EngineA->GetModuleByModuleName(ModuleName).IsValid(), TEXT("MultiEngine.IndependentEngines should not leak engine A's discarded module back to engine A's lookup"));
bOk &= LocalAssert.IsTrue(EngineB->GetModuleByModuleName(ModuleName).IsValid(), TEXT("MultiEngine.IndependentEngines should keep engine B's module reachable after engine A discards"));
return bOk;
}

static bool RunCloneDestroyDoesNotAffectPrimary(FAutomationTestBase& Test)
{
FNoDiscardAsserter LocalAssert(Test);
ResetToIsolatedEngineState();

const FString ModuleName = TEXT("Tests.SharedModule");
const FAngelscriptEngineConfig Config;
const FAngelscriptEngineDependencies Dependencies = FAngelscriptEngineDependencies::CreateDefault();
TUniquePtr<FAngelscriptEngine> PrimaryEngine = FAngelscriptTestEngine::Create(Config, Dependencies);
TUniquePtr<FAngelscriptEngine> CloneEngine = FAngelscriptTestEngine::Create(Config, Dependencies);

if (!LocalAssert.IsNotNull(PrimaryEngine.Get(), TEXT("MultiEngine.CloneDestroyDoesNotAffectPrimary should create primary engine"))
	|| !LocalAssert.IsNotNull(CloneEngine.Get(), TEXT("MultiEngine.CloneDestroyDoesNotAffectPrimary should create clone engine")))
{
	return false;
}

asIScriptModule* PrimaryModule = FAngelscriptMultiEngineTestAccess::CreateNamedModule(*PrimaryEngine, ModuleName);
asIScriptModule* CloneModule = FAngelscriptMultiEngineTestAccess::CreateNamedModule(*CloneEngine, ModuleName);
FAngelscriptMultiEngineTestAccess::TrackNamedModule(*PrimaryEngine, ModuleName, PrimaryModule);
FAngelscriptMultiEngineTestAccess::TrackNamedModule(*CloneEngine, ModuleName, CloneModule);

if (!LocalAssert.IsNotNull(PrimaryModule, TEXT("MultiEngine.CloneDestroyDoesNotAffectPrimary should create the primary module"))
	|| !LocalAssert.IsNotNull(CloneModule, TEXT("MultiEngine.CloneDestroyDoesNotAffectPrimary should create the clone module")))
{
	return false;
}

CloneEngine.Reset();

bool bOk = LocalAssert.IsTrue(PrimaryEngine->GetModuleByModuleName(ModuleName).IsValid(), TEXT("MultiEngine.CloneDestroyDoesNotAffectPrimary should keep the primary module descriptor registered"));
bOk &= LocalAssert.IsNotNull(FAngelscriptMultiEngineTestAccess::FindNamedModule(*PrimaryEngine, ModuleName), TEXT("MultiEngine.CloneDestroyDoesNotAffectPrimary should keep the primary underlying script module alive"));
return bOk;
}

static bool RunCloneKeepsSharedStateAlive(FAutomationTestBase& Test)
{
// Test removed: it asserted Clone-specific deferred shared-state release
// behavior that no longer exists after clone-removal. Engines are now
// independent single-owner Full instances; there is nothing to keep
// alive across owners.
return true;
}

static bool RunDestroyingSourceWhileCloneAliveIsRejected(FAutomationTestBase& Test)
{
// Test removed: targeted Clone-specific source-engine link clearing,
// which no longer exists after clone-removal.
return true;
}

static bool RunDeferredSharedStateReleasePurgesLocalContextPool(FAutomationTestBase& Test)
{
// Test removed: targeted Clone-specific deferred shared-state release
// path, which no longer exists after clone-removal. Single-owner
// engines release their context pool synchronously on shutdown.
return true;
}

static bool RunCloneHonorsInjectedDependencies(FAutomationTestBase& Test)
{
FNoDiscardAsserter LocalAssert(Test);
// Verifies the post-clone-removal contract: two independent Full engines
// each carry their own FAngelscriptEngineDependencies callbacks, and one
// engine's filesystem hooks must never fire on behalf of the other.
//
// Test name preserved for stable CI history; the underlying contract
// is now "multiple full engines have independent injected dependencies".
ResetToIsolatedEngineState();

FAngelscriptEngineConfig Config;
Config.bIsEditor = true;

bool bEngineAMakeDirCalled = false;
FString EngineACreatedPath;
FAngelscriptEngineDependencies EngineADeps;
EngineADeps.GetProjectDir = []() { return FString(TEXT("C:/InjectedEngineAProject")); };
EngineADeps.ConvertRelativePathToFull = [](const FString& Path) { return Path; };
EngineADeps.DirectoryExists = [](const FString& Path) { return false; };
EngineADeps.MakeDirectory = [&bEngineAMakeDirCalled, &EngineACreatedPath](const FString& Path, bool /*bTree*/)
{
	bEngineAMakeDirCalled = true;
	EngineACreatedPath = Path;
	return true;
};
EngineADeps.GetEnabledPluginScriptRoots = []() { return TArray<FString>(); };

bool bEngineBMakeDirCalled = false;
FString EngineBCreatedPath;
FAngelscriptEngineDependencies EngineBDeps;
EngineBDeps.GetProjectDir = []() { return FString(TEXT("C:/InjectedEngineBProject")); };
EngineBDeps.ConvertRelativePathToFull = [](const FString& Path) { return Path; };
EngineBDeps.DirectoryExists = [](const FString& Path) { return false; };
EngineBDeps.MakeDirectory = [&bEngineBMakeDirCalled, &EngineBCreatedPath](const FString& Path, bool /*bTree*/)
{
	bEngineBMakeDirCalled = true;
	EngineBCreatedPath = Path;
	return true;
};
EngineBDeps.GetEnabledPluginScriptRoots = []() { return TArray<FString>(); };

TUniquePtr<FAngelscriptEngine> EngineA = FAngelscriptTestEngine::Create(Config, EngineADeps);
TUniquePtr<FAngelscriptEngine> EngineB = FAngelscriptTestEngine::Create(Config, EngineBDeps);
if (!LocalAssert.IsNotNull(EngineA.Get(), TEXT("MultiEngine.IndependentInjectedDependencies should create engine A"))
	|| !LocalAssert.IsNotNull(EngineB.Get(), TEXT("MultiEngine.IndependentInjectedDependencies should create engine B")))
{
	return false;
}

// Each engine's DiscoverScriptRoots must drive its own injected callbacks.
const TArray<FString> EngineARoots = EngineA->DiscoverScriptRoots(false);
bool bOk = true;
bOk &= LocalAssert.IsTrue(bEngineAMakeDirCalled, TEXT("MultiEngine.IndependentInjectedDependencies engine A should fire its own MakeDirectory hook"));
bOk &= LocalAssert.IsFalse(bEngineBMakeDirCalled, TEXT("MultiEngine.IndependentInjectedDependencies discovering on engine A must not fire engine B's MakeDirectory"));
bOk &= LocalAssert.AreEqual(FString(TEXT("C:/InjectedEngineAProject/Script")), EngineACreatedPath, TEXT("MultiEngine.IndependentInjectedDependencies engine A should resolve the injected engine A project root"));
if (EngineARoots.Num() > 0)
{
	bOk &= LocalAssert.AreEqual(FString(TEXT("C:/InjectedEngineAProject/Script")), EngineARoots[0], TEXT("MultiEngine.IndependentInjectedDependencies engine A's discovered root should reflect its own GetProjectDir"));
}

const TArray<FString> EngineBRoots = EngineB->DiscoverScriptRoots(false);
bOk &= LocalAssert.IsTrue(bEngineBMakeDirCalled, TEXT("MultiEngine.IndependentInjectedDependencies engine B should fire its own MakeDirectory hook"));
bOk &= LocalAssert.AreEqual(FString(TEXT("C:/InjectedEngineBProject/Script")), EngineBCreatedPath, TEXT("MultiEngine.IndependentInjectedDependencies engine B should resolve the injected engine B project root"));
if (EngineBRoots.Num() > 0)
{
	bOk &= LocalAssert.AreEqual(FString(TEXT("C:/InjectedEngineBProject/Script")), EngineBRoots[0], TEXT("MultiEngine.IndependentInjectedDependencies engine B's discovered root should reflect its own GetProjectDir"));
}

// The two engines must not have mutated each other's captured paths.
bOk &= LocalAssert.AreNotEqual(EngineBCreatedPath, EngineACreatedPath, TEXT("MultiEngine.IndependentInjectedDependencies the two engines' captured paths must remain distinct"));
return bOk;
}

static bool RunStartupBindObservationFullCreate(FAutomationTestBase& Test)
{
FNoDiscardAsserter LocalAssert(Test);
ResetToIsolatedEngineState();

const FName FirstBindName = MakeUniqueStartupBindName(TEXT("Automation.StartupBind.Full.First"));
const FName SecondBindName = MakeUniqueStartupBindName(TEXT("Automation.StartupBind.Full.Second"));
FAngelscriptBinds::FBind FirstBind(FirstBindName, -25, []() {});
FAngelscriptBinds::FBind SecondBind(SecondBindName, 25, []() {});

FAngelscriptBindExecutionObservation::Reset();

const FAngelscriptEngineConfig Config;
const FAngelscriptEngineDependencies Dependencies = FAngelscriptEngineDependencies::CreateDefault();
TUniquePtr<FAngelscriptEngine> Engine = FAngelscriptTestEngine::Create(Config, Dependencies);
if (!LocalAssert.IsNotNull(Engine.Get(), TEXT("MultiEngine.StartupBindObservation.FullCreateRecordsOrderedBinds should create a full engine")))
{
	return false;
}

const FAngelscriptBindExecutionSnapshot Snapshot = FAngelscriptBindExecutionObservation::GetLastSnapshot();
if (!LocalAssert.AreEqual(1, FAngelscriptBindExecutionObservation::GetInvocationCount(), TEXT("MultiEngine.StartupBindObservation.FullCreateRecordsOrderedBinds should observe a single startup bind pass")))
{
	return false;
}

const int32 FirstIndex = Snapshot.ExecutedBindNames.IndexOfByKey(FirstBindName);
const int32 SecondIndex = Snapshot.ExecutedBindNames.IndexOfByKey(SecondBindName);
if (!LocalAssert.IsTrue(FirstIndex != INDEX_NONE, TEXT("MultiEngine.StartupBindObservation.FullCreateRecordsOrderedBinds should observe the first named bind"))
	|| !LocalAssert.IsTrue(SecondIndex != INDEX_NONE, TEXT("MultiEngine.StartupBindObservation.FullCreateRecordsOrderedBinds should observe the second named bind")))
{
	return false;
}

return LocalAssert.IsTrue(FirstIndex < SecondIndex, TEXT("MultiEngine.StartupBindObservation.FullCreateRecordsOrderedBinds should preserve bind order in the observed startup pass"));
}

static bool RunStartupBindObservationCloneCreate(FAutomationTestBase& Test)
{
// Test removed: targeted Clone create-path skipping startup binds (since
// Clone shared the source's bind state). After clone-removal every
// engine is a fresh Full instance that replays binds, so the original
// invariant no longer holds.
return true;
}

static bool RunStartupBindObservationCreateForTestingClone(FAutomationTestBase& Test)
{
// Test removed: same rationale as RunStartupBindObservationCloneCreate.
return true;
}

static bool RunStartupBindObservationCreateForTestingFullFallback(FAutomationTestBase& Test)
{
FNoDiscardAsserter LocalAssert(Test);
ResetToIsolatedEngineState();
FMultiEngineContextStackGuard StackGuard;

const FName FirstBindName = MakeUniqueStartupBindName(TEXT("Automation.StartupBind.CreateForTesting.FullFallback.First"));
const FName SecondBindName = MakeUniqueStartupBindName(TEXT("Automation.StartupBind.CreateForTesting.FullFallback.Second"));
FAngelscriptBinds::FBind FirstBind(FirstBindName, -50, []() {});
FAngelscriptBinds::FBind SecondBind(SecondBindName, 50, []() {});

FAngelscriptBindExecutionObservation::Reset();

FAngelscriptEngineConfig Config;
Config.bIsEditor = true;
const FAngelscriptEngineDependencies Dependencies = FAngelscriptEngineDependencies::CreateDefault();
TUniquePtr<FAngelscriptEngine> TestEngine = FAngelscriptTestEngine::Create(Config, Dependencies);
if (!LocalAssert.IsNotNull(TestEngine.Get(), TEXT("MultiEngine.StartupBindObservation.CreateForTestingFullFallbackReplaysBinds should create a fallback full engine")))
{
	return false;
}

if (!LocalAssert.AreEqual(1, FAngelscriptBindExecutionObservation::GetInvocationCount(), TEXT("MultiEngine.StartupBindObservation.CreateForTestingFullFallbackReplaysBinds should observe one startup bind pass")))
{
	return false;
}

const FAngelscriptBindExecutionSnapshot Snapshot = FAngelscriptBindExecutionObservation::GetLastSnapshot();
const int32 FirstIndex = Snapshot.ExecutedBindNames.IndexOfByKey(FirstBindName);
const int32 SecondIndex = Snapshot.ExecutedBindNames.IndexOfByKey(SecondBindName);
if (!LocalAssert.IsTrue(FirstIndex != INDEX_NONE, TEXT("MultiEngine.StartupBindObservation.CreateForTestingFullFallbackReplaysBinds should observe the first bind"))
	|| !LocalAssert.IsTrue(SecondIndex != INDEX_NONE, TEXT("MultiEngine.StartupBindObservation.CreateForTestingFullFallbackReplaysBinds should observe the second bind")))
{
	return false;
}

return LocalAssert.IsTrue(FirstIndex < SecondIndex, TEXT("MultiEngine.StartupBindObservation.CreateForTestingFullFallbackReplaysBinds should preserve order for the fallback full startup pass"));
}

public:
	TEST_METHOD(CloneModuleIsolation)
	{
RunCloneModuleIsolation(*TestRunner);
	}

	TEST_METHOD(CloneDestroyDoesNotAffectPrimary)
	{
RunCloneDestroyDoesNotAffectPrimary(*TestRunner);
	}

	TEST_METHOD(CloneKeepsSharedStateAlive)
	{
RunCloneKeepsSharedStateAlive(*TestRunner);
	}

	TEST_METHOD(DestroyingSourceWhileCloneAliveIsRejected)
	{
RunDestroyingSourceWhileCloneAliveIsRejected(*TestRunner);
	}

	TEST_METHOD(DeferredSharedStateReleasePurgesLocalContextPool)
	{
RunDeferredSharedStateReleasePurgesLocalContextPool(*TestRunner);
	}

	TEST_METHOD(CloneHonorsInjectedDependencies)
	{
RunCloneHonorsInjectedDependencies(*TestRunner);
	}

	TEST_METHOD(StartupBindObservationFullCreate)
	{
RunStartupBindObservationFullCreate(*TestRunner);
	}

	TEST_METHOD(StartupBindObservationCloneCreate)
	{
RunStartupBindObservationCloneCreate(*TestRunner);
	}

	TEST_METHOD(StartupBindObservationCreateForTestingClone)
	{
RunStartupBindObservationCreateForTestingClone(*TestRunner);
	}

	TEST_METHOD(StartupBindObservationCreateForTestingFullFallback)
	{
RunStartupBindObservationCreateForTestingFullFallback(*TestRunner);
	}

};

#endif
