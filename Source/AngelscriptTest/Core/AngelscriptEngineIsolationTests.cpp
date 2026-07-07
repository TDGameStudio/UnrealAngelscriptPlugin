#include "AngelscriptEngine.h"
#include "AngelscriptSubsystem.h"
#include "AngelscriptBinds.h"
#include "AngelscriptBindDatabase.h"
#include "AngelscriptType.h"
#include "Binds/Helper_FunctionSignature.h"
#include "Binds/Helper_ToString.h"
#include "FunctionLibraries/SubsystemLibrary.h"
#include "CQTest.h"
#include "Misc/Guid.h"
#include "Misc/ScopeExit.h"
#include "Curves/CurveFloat.h"
#include "AngelscriptTestEngine.h"
#include "UObject/UObjectGlobals.h"

#include "AngelscriptInclude.h"

#if WITH_ANGELSCRIPT_UNITTESTS

struct FAngelscriptEngineIsolationTestAccess
{
	static bool DestroyGlobalEngine()
	{
		return FAngelscriptEngine::DestroyGlobal();
	}

	static int32 GetToStringCount(const FAngelscriptEngine& Engine)
	{
		return Engine.GetToStringEntryCountForTesting();
	}

	static int32 GetBindDatabaseClassCount(const FAngelscriptEngine& Engine)
	{
		return Engine.GetBindDatabaseForTesting().Classes.Num();
	}

	static int32 GetLocalPooledContextCount(asIScriptEngine* ScriptEngine)
	{
		return FAngelscriptEngine::GetLocalPooledContextCountForTesting(ScriptEngine);
	}

	static asIScriptContext* GetActiveContext()
	{
		// Uses the AS public SDK accessor so this test can link from outside the Runtime DLL.
		return asGetActiveContext();
	}
};

TEST_CLASS_WITH_FLAGS(FAngelscriptEngineIsolationTests,
	"Angelscript.TestModule.Engine.Isolation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
struct FIsolationContextStackGuard
{
TArray<FAngelscriptEngine*> SavedStack;
FIsolationContextStackGuard()
{
	SavedStack = FAngelscriptEngineContextStack::SnapshotAndClear();
}
~FIsolationContextStackGuard()
{
	FAngelscriptEngineContextStack::RestoreSnapshot(MoveTemp(SavedStack));
}
};

static void ResetIsolationRuntime()
{
if (FAngelscriptEngine::IsInitialized() && UAngelscriptSubsystem::Get() == nullptr)
{
	FAngelscriptEngineIsolationTestAccess::DestroyGlobalEngine();
}
}

static FString MakeIsolationName(const TCHAR* Prefix)
{
return FString::Printf(TEXT("%s_%s"), Prefix, *FGuid::NewGuid().ToString(EGuidFormats::Digits));
}

// FAngelscriptPooledContextBase::operator->() returns asCContext* (incomplete here because
// <source/as_context.h> is intentionally not pulled into this test). asCContext inherits from
// asIScriptContext, and reinterpret_cast between pointer types is well-defined in C++ without
// requiring a complete type, so we use this helper to reach the public AS interface.
static FORCEINLINE asIScriptContext* ScriptContextOf(const FAngelscriptPooledContextBase& Pooled)
{
return reinterpret_cast<asIScriptContext*>(Pooled.operator->());
}

static asIScriptFunction* CompileIsolationFunction(
FAutomationTestBase& Test,
FAngelscriptEngine& Engine,
const FString& ModuleName,
const ANSICHAR* Source,
const ANSICHAR* Declaration)
{
FNoDiscardAsserter LocalAssert(Test);
FAngelscriptEngineScope GlobalScope(Engine);

asIScriptModule* Module = Engine.GetScriptEngine()->GetModule(TCHAR_TO_ANSI(*ModuleName), asGM_ALWAYS_CREATE);
if (!LocalAssert.IsNotNull(Module, *FString::Printf(TEXT("Isolation helper should create module '%s'"), *ModuleName)))
{
	return nullptr;
}

asIScriptFunction* Function = nullptr;
const int32 CompileResult = Module->CompileFunction(TCHAR_TO_ANSI(*ModuleName), Source, 0, 0, &Function);
if (!LocalAssert.AreEqual(static_cast<int32>(asSUCCESS), CompileResult, *FString::Printf(TEXT("Isolation helper should compile '%s'"), *ModuleName)))
{
	return nullptr;
}

(void)LocalAssert.IsNotNull(Function, *FString::Printf(TEXT("Isolation helper should resolve '%s'"), ANSI_TO_TCHAR(Declaration)));
return Function;
}

static bool RunContextStackScopedResolution(FAutomationTestBase& Test)
{
FNoDiscardAsserter LocalAssert(Test);
ResetIsolationRuntime();
FIsolationContextStackGuard StackGuard;

const FAngelscriptEngineConfig Config;
const FAngelscriptEngineDependencies Dependencies = FAngelscriptEngineDependencies::CreateDefault();
TUniquePtr<FAngelscriptEngine> PrimaryEngine = FAngelscriptTestEngine::Create(Config, Dependencies);
TUniquePtr<FAngelscriptEngine> SecondaryEngine = FAngelscriptTestEngine::Create(Config, Dependencies);

if (!LocalAssert.IsNotNull(PrimaryEngine.Get(), TEXT("Context stack scoped resolution should create a primary engine"))
	|| !LocalAssert.IsNotNull(SecondaryEngine.Get(), TEXT("Context stack scoped resolution should create a secondary engine")))
{
	return false;
}

bool bOk = LocalAssert.IsTrue(FAngelscriptEngineContextStack::IsEmpty(), TEXT("Context stack should start empty after guard clears it"));

{
	FAngelscriptEngineScope PrimaryScope(*PrimaryEngine);
	bOk &= LocalAssert.IsTrue(&FAngelscriptEngine::Get() == PrimaryEngine.Get(), TEXT("Scoped resolution should return the primary engine while its scope is active"));
	bOk &= LocalAssert.IsTrue(FAngelscriptEngineContextStack::Peek() == PrimaryEngine.Get(), TEXT("Context stack should expose the active primary engine"));

	{
		FAngelscriptEngineScope SecondaryScope(*SecondaryEngine);
		bOk &= LocalAssert.IsTrue(&FAngelscriptEngine::Get() == SecondaryEngine.Get(), TEXT("Nested scoped resolution should prefer the nested engine"));
		bOk &= LocalAssert.IsTrue(FAngelscriptEngineContextStack::Peek() == SecondaryEngine.Get(), TEXT("Context stack should update its top entry for nested scopes"));
	}

	bOk &= LocalAssert.IsTrue(&FAngelscriptEngine::Get() == PrimaryEngine.Get(), TEXT("Nested scope teardown should restore the previous engine"));
	bOk &= LocalAssert.IsTrue(FAngelscriptEngineContextStack::Peek() == PrimaryEngine.Get(), TEXT("Context stack should restore the previous engine after nested scope teardown"));
}

bOk &= LocalAssert.IsTrue(FAngelscriptEngineContextStack::IsEmpty(), TEXT("Context stack should be empty after all scopes leave"));
return bOk;
}

static bool RunEngineScopeRestoresWorldContext(FAutomationTestBase& Test)
{
FNoDiscardAsserter LocalAssert(Test);
ResetIsolationRuntime();

const FAngelscriptEngineConfig Config;
const FAngelscriptEngineDependencies Dependencies = FAngelscriptEngineDependencies::CreateDefault();
TUniquePtr<FAngelscriptEngine> PrimaryEngine = FAngelscriptTestEngine::Create(Config, Dependencies);
TUniquePtr<FAngelscriptEngine> SecondaryEngine = FAngelscriptTestEngine::Create(Config, Dependencies);

if (!LocalAssert.IsNotNull(PrimaryEngine.Get(), TEXT("Engine scope restore test should create a primary engine"))
	|| !LocalAssert.IsNotNull(SecondaryEngine.Get(), TEXT("Engine scope restore test should create a secondary engine")))
{
	return false;
}

UObject* OuterContext = NewObject<UCurveFloat>();
UObject* InnerContext = NewObject<UCurveFloat>();
if (!LocalAssert.IsNotNull(OuterContext, TEXT("Engine scope restore test should create an outer context object"))
	|| !LocalAssert.IsNotNull(InnerContext, TEXT("Engine scope restore test should create an inner context object")))
{
	return false;
}

bool bOk = true;
{
	FAngelscriptEngineScope OuterScope(*PrimaryEngine, OuterContext);
	bOk &= LocalAssert.IsTrue(PrimaryEngine->GetCurrentWorldContextObject() == OuterContext, TEXT("Outer scope should expose its world context through the active engine"));

	{
		FAngelscriptEngineScope InnerScope(*SecondaryEngine, InnerContext);
		bOk &= LocalAssert.IsTrue(SecondaryEngine->GetCurrentWorldContextObject() == InnerContext, TEXT("Inner scope should expose its world context through the nested engine"));
	}

	bOk &= LocalAssert.IsTrue(PrimaryEngine->GetCurrentWorldContextObject() == OuterContext, TEXT("Leaving the inner scope should restore the outer world context"));
}

bOk &= LocalAssert.IsNull(PrimaryEngine->GetCurrentWorldContextObject(), TEXT("Leaving the outer scope should clear the world context"));
return bOk;
}

static bool RunFullEnginesKeepStateSeparate(FAutomationTestBase& Test)
{
FNoDiscardAsserter LocalAssert(Test);
ResetIsolationRuntime();

const FAngelscriptEngineConfig Config;
const FAngelscriptEngineDependencies Dependencies = FAngelscriptEngineDependencies::CreateDefault();
TUniquePtr<FAngelscriptEngine> EngineA = FAngelscriptTestEngine::Create(Config, Dependencies);
TUniquePtr<FAngelscriptEngine> EngineB = FAngelscriptTestEngine::Create(Config, Dependencies);

if (!LocalAssert.IsNotNull(EngineA.Get(), TEXT("Full engine isolation test should create engine A"))
	|| !LocalAssert.IsNotNull(EngineB.Get(), TEXT("Full engine isolation test should create engine B")))
{
	return false;
}

const FString AliasName = MakeIsolationName(TEXT("Alias"));
const FString ToStringName = MakeIsolationName(TEXT("ToString"));
FAngelscriptClassBind BindClass;
BindClass.TypeName = MakeIsolationName(TEXT("BindDb"));
int32 BaselineToStringCountA = 0;
int32 BaselineBindDatabaseClassCountA = 0;
int32 BaselineToStringCountB = 0;
int32 BaselineBindDatabaseClassCountB = 0;

{
	FAngelscriptEngineScope ScopeA(*EngineA);
	BaselineToStringCountA = FAngelscriptEngineIsolationTestAccess::GetToStringCount(*EngineA);
	BaselineBindDatabaseClassCountA = FAngelscriptEngineIsolationTestAccess::GetBindDatabaseClassCount(*EngineA);
}

{
	FAngelscriptEngineScope ScopeB(*EngineB);
	BaselineToStringCountB = FAngelscriptEngineIsolationTestAccess::GetToStringCount(*EngineB);
	BaselineBindDatabaseClassCountB = FAngelscriptEngineIsolationTestAccess::GetBindDatabaseClassCount(*EngineB);
}

{
	FAngelscriptEngineScope ScopeA(*EngineA);
	TSharedPtr<FAngelscriptType> IntType = FAngelscriptType::GetByAngelscriptTypeName(TEXT("int"));
	if (!LocalAssert.IsTrue(IntType.IsValid(), TEXT("Full engine isolation test should resolve the built-in int type inside engine A")))
	{
		return false;
	}

	FAngelscriptType::RegisterAlias(AliasName, IntType.ToSharedRef());
	FAngelscriptBinds::AddSkipEntry(FName(TEXT("EngineIsolationActor")), FName(TEXT("OnlyEngineA")));
	FToStringHelper::Register(ToStringName, +[](void*, FString& OutString)
	{
		OutString = TEXT("EngineA");
	});
	FAngelscriptBindDatabase::Get().Classes.Add(BindClass);
}

{
	FAngelscriptEngineScope ScopeB(*EngineB);
	bool bOk = true;
	bOk &= LocalAssert.IsNull(FAngelscriptType::GetByAngelscriptTypeName(AliasName).Get(), TEXT("Engine B should not see aliases registered through engine A"));
	bOk &= LocalAssert.IsFalse(FAngelscriptBinds::CheckForSkipEntry(FName(TEXT("EngineIsolationActor")), FName(TEXT("OnlyEngineA"))), TEXT("Engine B should not inherit skip entries registered through engine A"));
	bOk &= LocalAssert.AreEqual(BaselineToStringCountB, FAngelscriptEngineIsolationTestAccess::GetToStringCount(*EngineB), TEXT("Engine B should keep its original ToString registry baseline"));
	bOk &= LocalAssert.AreEqual(BaselineBindDatabaseClassCountB, FAngelscriptEngineIsolationTestAccess::GetBindDatabaseClassCount(*EngineB), TEXT("Engine B should keep its original bind database baseline"));
	if (!bOk)
	{
		return false;
	}
}

{
	FAngelscriptEngineScope ScopeA(*EngineA);
	bool bOk = true;
	bOk &= LocalAssert.IsNotNull(FAngelscriptType::GetByAngelscriptTypeName(AliasName).Get(), TEXT("Engine A should keep its alias registration"));
	bOk &= LocalAssert.IsTrue(FAngelscriptBinds::CheckForSkipEntry(FName(TEXT("EngineIsolationActor")), FName(TEXT("OnlyEngineA"))), TEXT("Engine A should keep its skip entry registration"));
	bOk &= LocalAssert.AreEqual(BaselineToStringCountA + 1, FAngelscriptEngineIsolationTestAccess::GetToStringCount(*EngineA), TEXT("Engine A should retain its extra ToString registry entry"));
	bOk &= LocalAssert.AreEqual(BaselineBindDatabaseClassCountA + 1, FAngelscriptEngineIsolationTestAccess::GetBindDatabaseClassCount(*EngineA), TEXT("Engine A should retain its extra bind database class"));
	return bOk;
}
}

static bool RunCloneSharesSourceState(FAutomationTestBase& Test)
{
// Test removed: targeted Clone engines sharing aliases / skip entries /
// ToString registry with their source engine, which no longer exists
// after clone-removal. Independent Full engines have independent state.
return true;
}

static bool RunRequestContextUsesRequestedEngine(FAutomationTestBase& Test)
{
FNoDiscardAsserter LocalAssert(Test);
ResetIsolationRuntime();

const FAngelscriptEngineConfig Config;
const FAngelscriptEngineDependencies Dependencies = FAngelscriptEngineDependencies::CreateDefault();
TUniquePtr<FAngelscriptEngine> EngineA = FAngelscriptTestEngine::Create(Config, Dependencies);
TUniquePtr<FAngelscriptEngine> EngineB = FAngelscriptTestEngine::Create(Config, Dependencies);

if (!LocalAssert.IsNotNull(EngineA.Get(), TEXT("RequestContext isolation test should create engine A"))
	|| !LocalAssert.IsNotNull(EngineB.Get(), TEXT("RequestContext isolation test should create engine B")))
{
	return false;
}

{
	FAngelscriptEngineScope ScopeA(*EngineA);
	asIScriptContext* ContextA = EngineA->GetScriptEngine()->RequestContext();
	if (!LocalAssert.IsNotNull(ContextA, TEXT("RequestContext isolation test should acquire a context from engine A")))
	{
		return false;
	}

	(void)LocalAssert.IsTrue(ContextA->GetEngine() == EngineA->GetScriptEngine(), TEXT("Requested context A should belong to engine A"));
	EngineA->GetScriptEngine()->ReturnContext(ContextA);
}

{
	FAngelscriptEngineScope ScopeB(*EngineB);
	asIScriptContext* ContextB = EngineB->GetScriptEngine()->RequestContext();
	if (!LocalAssert.IsNotNull(ContextB, TEXT("RequestContext isolation test should acquire a context from engine B")))
	{
		return false;
	}

	const bool bMatchesRequestedEngine = LocalAssert.IsTrue(
		ContextB->GetEngine() == EngineB->GetScriptEngine(),
		TEXT("RequestContext should not recycle a context from another engine"));
	EngineB->GetScriptEngine()->ReturnContext(ContextB);
	return bMatchesRequestedEngine;
}
}

static bool RunRequestContextReusedStartsUnprepared(FAutomationTestBase& Test)
{
FNoDiscardAsserter LocalAssert(Test);
ResetIsolationRuntime();

const FAngelscriptEngineConfig Config;
const FAngelscriptEngineDependencies Dependencies = FAngelscriptEngineDependencies::CreateDefault();
TUniquePtr<FAngelscriptEngine> Engine = FAngelscriptTestEngine::Create(Config, Dependencies);
if (!LocalAssert.IsNotNull(Engine.Get(), TEXT("RequestContext reuse test should create an engine")))
{
	return false;
}

const FString ModuleName = MakeIsolationName(TEXT("RequestContextReuse"));
asIScriptFunction* Function = CompileIsolationFunction(Test, *Engine, ModuleName, "void Run() {}", "void Run()");
if (!LocalAssert.IsNotNull(Function, TEXT("RequestContext reuse test should compile its helper function")))
{
	return false;
}

ON_SCOPE_EXIT
{
	Function->Release();
};

FAngelscriptEngineScope Scope(*Engine);

asIScriptContext* SeedRawContext = Engine->GetScriptEngine()->RequestContext();
if (!LocalAssert.IsNotNull(SeedRawContext, TEXT("RequestContext reuse test should acquire a seed context")))
{
	return false;
}

const int32 PrepareResult = SeedRawContext->Prepare(Function);
const int32 ExecuteResult = PrepareResult == asSUCCESS ? SeedRawContext->Execute() : PrepareResult;
if (!LocalAssert.AreEqual(static_cast<int32>(asSUCCESS), PrepareResult, TEXT("Seed RequestContext should prepare successfully"))
	|| !LocalAssert.AreEqual(static_cast<int32>(asEXECUTION_FINISHED), ExecuteResult, TEXT("Seed RequestContext should execute successfully")))
{
	Engine->GetScriptEngine()->ReturnContext(SeedRawContext);
	return false;
}

Engine->GetScriptEngine()->ReturnContext(SeedRawContext);

asIScriptContext* ReusedContext = Engine->GetScriptEngine()->RequestContext();
if (!LocalAssert.IsNotNull(ReusedContext, TEXT("RequestContext reuse test should reacquire a context")))
{
	return false;
}

const bool bReusedSameContext = LocalAssert.IsTrue(
	ReusedContext == SeedRawContext,
	TEXT("RequestContext reuse test should reacquire the pooled context"));
const bool bStartsUnprepared = LocalAssert.AreEqual(
	static_cast<int32>(asEXECUTION_UNINITIALIZED),
	static_cast<int32>(ReusedContext->GetState()),
	TEXT("Reused RequestContext should start unprepared after pool reuse"));
Engine->GetScriptEngine()->ReturnContext(ReusedContext);
return bReusedSameContext && bStartsUnprepared;
}

static bool RunRequestContextAfterReturningUnpreparedScopedContext(FAutomationTestBase& Test)
{
FNoDiscardAsserter LocalAssert(Test);
ResetIsolationRuntime();

const FAngelscriptEngineConfig Config;
const FAngelscriptEngineDependencies Dependencies = FAngelscriptEngineDependencies::CreateDefault();
TUniquePtr<FAngelscriptEngine> Engine = FAngelscriptTestEngine::Create(Config, Dependencies);
if (!LocalAssert.IsNotNull(Engine.Get(), TEXT("RequestContext after unprepared scoped context test should create an engine")))
{
	return false;
}

const FString ModuleName = MakeIsolationName(TEXT("RequestContextAfterUnpreparedScopedContext"));
asIScriptFunction* Function = CompileIsolationFunction(Test, *Engine, ModuleName, "void Run() {}", "void Run()");
if (!LocalAssert.IsNotNull(Function, TEXT("RequestContext after unprepared scoped context test should compile its helper function")))
{
	return false;
}

ON_SCOPE_EXIT
{
	Function->Release();
};

asIScriptContext* RequestedContext = nullptr;
asIScriptContext* ReturnedScopedRawContext = nullptr;
{
	FAngelscriptEngineScope Scope(*Engine);
	{
		FAngelscriptPooledContextBase UnpreparedContext;
		ReturnedScopedRawContext = ScriptContextOf(UnpreparedContext);
	}

	RequestedContext = Engine->GetScriptEngine()->RequestContext();
	if (!LocalAssert.IsNotNull(RequestedContext, TEXT("RequestContext after unprepared scoped context test should reacquire a context")))
	{
		return false;
	}

	const bool bReusedReturnedScopedContext = LocalAssert.IsTrue(
		RequestedContext == ReturnedScopedRawContext,
		TEXT("RequestContext after unprepared scoped context test should reuse the returned scoped context"));
	const int32 PrepareResult = RequestedContext->Prepare(Function);
	const int32 ExecuteResult = PrepareResult == asSUCCESS ? RequestedContext->Execute() : PrepareResult;
	Engine->GetScriptEngine()->ReturnContext(RequestedContext);
	return bReusedReturnedScopedContext
		&& LocalAssert.AreEqual(static_cast<int32>(asSUCCESS), PrepareResult, TEXT("RequestContext after unprepared scoped context test should prepare successfully"))
		&& LocalAssert.AreEqual(static_cast<int32>(asEXECUTION_FINISHED), ExecuteResult, TEXT("RequestContext after unprepared scoped context test should execute successfully"));
}
}

static bool RunFullEngineCreateClearsThreadLocalPool(FAutomationTestBase& Test)
{
FNoDiscardAsserter LocalAssert(Test);
ResetIsolationRuntime();

const FAngelscriptEngineConfig Config;
const FAngelscriptEngineDependencies Dependencies = FAngelscriptEngineDependencies::CreateDefault();
TUniquePtr<FAngelscriptEngine> EngineA = FAngelscriptTestEngine::Create(Config, Dependencies);
if (!LocalAssert.IsNotNull(EngineA.Get(), TEXT("Full engine create pool reset test should create engine A")))
{
	return false;
}

const FString ModuleNameA = MakeIsolationName(TEXT("FullCreatePoolResetA"));
asIScriptFunction* FunctionA = CompileIsolationFunction(Test, *EngineA, ModuleNameA, "void Run() {}", "void Run()");
if (!LocalAssert.IsNotNull(FunctionA, TEXT("Full engine create pool reset test should compile function A")))
{
	return false;
}

ON_SCOPE_EXIT
{
	FunctionA->Release();
};

{
	FAngelscriptEngineScope ScopeA(*EngineA);
	FAngelscriptPooledContextBase SeedContext;
	asIScriptContext* SeedScriptContext = ScriptContextOf(SeedContext);
	const int32 PrepareResult = SeedScriptContext->Prepare(FunctionA);
	const int32 ExecuteResult = PrepareResult == asSUCCESS ? SeedScriptContext->Execute() : PrepareResult;
	if (!LocalAssert.AreEqual(static_cast<int32>(asSUCCESS), PrepareResult, TEXT("Full engine create pool reset test should seed engine A into the local pool"))
		|| !LocalAssert.AreEqual(static_cast<int32>(asEXECUTION_FINISHED), ExecuteResult, TEXT("Full engine create pool reset test should execute the seeded function")))
	{
		return false;
	}
}

if (!LocalAssert.IsTrue(
	FAngelscriptEngineIsolationTestAccess::GetLocalPooledContextCount(EngineA->GetScriptEngine()) > 0,
	TEXT("Full engine create pool reset test should leave a free pooled context for engine A before creating a new full engine")))
{
	return false;
}

TUniquePtr<FAngelscriptEngine> EngineB = FAngelscriptTestEngine::Create(Config, Dependencies);
if (!LocalAssert.IsNotNull(EngineB.Get(), TEXT("Full engine create pool reset test should create engine B")))
{
	return false;
}

if (!LocalAssert.AreEqual(
	0,
	FAngelscriptEngineIsolationTestAccess::GetLocalPooledContextCount(nullptr),
	TEXT("Creating a new full engine should clear stale free contexts from the thread-local pool")))
{
	return false;
}

{
	FAngelscriptEngineScope ScopeB(*EngineB);
	FAngelscriptPooledContextBase FreshContext;
	return LocalAssert.IsTrue(
		ScriptContextOf(FreshContext)->GetEngine() == EngineB->GetScriptEngine(),
		TEXT("Creating a new full engine should acquire a context bound to that engine"));
}
}

static bool RunContextPoolResetSequenceKeepsRequestedContextReusable(FAutomationTestBase& Test)
{
FNoDiscardAsserter LocalAssert(Test);
ResetIsolationRuntime();

const FAngelscriptEngineConfig Config;
const FAngelscriptEngineDependencies Dependencies = FAngelscriptEngineDependencies::CreateDefault();

{
	TUniquePtr<FAngelscriptEngine> EngineA = FAngelscriptTestEngine::Create(Config, Dependencies);
	if (!LocalAssert.IsNotNull(EngineA.Get(), TEXT("Sequence test should create engine A")))
	{
		return false;
	}

	const FString ModuleNameA = MakeIsolationName(TEXT("SequenceFullCreatePoolResetA"));
	asIScriptFunction* FunctionA = CompileIsolationFunction(Test, *EngineA, ModuleNameA, "void Run() {}", "void Run()");
	if (!LocalAssert.IsNotNull(FunctionA, TEXT("Sequence test should compile function A")))
	{
		return false;
	}

	ON_SCOPE_EXIT
	{
		FunctionA->Release();
	};

	{
		FAngelscriptEngineScope ScopeA(*EngineA);
		FAngelscriptPooledContextBase SeedContext;
		asIScriptContext* SeedScriptContext = ScriptContextOf(SeedContext);
		const int32 PrepareResult = SeedScriptContext->Prepare(FunctionA);
		const int32 ExecuteResult = PrepareResult == asSUCCESS ? SeedScriptContext->Execute() : PrepareResult;
		if (!LocalAssert.AreEqual(static_cast<int32>(asSUCCESS), PrepareResult, TEXT("Sequence test should seed engine A into the local pool"))
			|| !LocalAssert.AreEqual(static_cast<int32>(asEXECUTION_FINISHED), ExecuteResult, TEXT("Sequence test should execute the seeded function")))
		{
			return false;
		}
	}

	if (!LocalAssert.IsTrue(
		FAngelscriptEngineIsolationTestAccess::GetLocalPooledContextCount(EngineA->GetScriptEngine()) > 0,
		TEXT("Sequence test should leave a free pooled context for engine A before creating engine B")))
	{
		return false;
	}

	TUniquePtr<FAngelscriptEngine> EngineB = FAngelscriptTestEngine::Create(Config, Dependencies);
	if (!LocalAssert.IsNotNull(EngineB.Get(), TEXT("Sequence test should create engine B")))
	{
		return false;
	}

	if (!LocalAssert.AreEqual(
		0,
		FAngelscriptEngineIsolationTestAccess::GetLocalPooledContextCount(nullptr),
		TEXT("Sequence test should clear stale free contexts when engine B starts")))
	{
		return false;
	}

	{
		FAngelscriptEngineScope ScopeB(*EngineB);
		FAngelscriptPooledContextBase FreshContext;
		if (!LocalAssert.IsTrue(
			ScriptContextOf(FreshContext)->GetEngine() == EngineB->GetScriptEngine(),
			TEXT("Sequence test should acquire a context bound to engine B")))
		{
			return false;
		}
	}
}

if (!LocalAssert.AreEqual(
	0,
	FAngelscriptEngineIsolationTestAccess::GetLocalPooledContextCount(nullptr),
	TEXT("Sequence test should leave no pooled contexts behind after the full-engine phase")))
{
	return false;
}

TUniquePtr<FAngelscriptEngine> Engine = FAngelscriptTestEngine::Create(Config, Dependencies);
if (!LocalAssert.IsNotNull(Engine.Get(), TEXT("Sequence test should create the follow-up engine")))
{
	return false;
}

const FString ModuleName = MakeIsolationName(TEXT("SequenceRequestContextAfterUnpreparedScopedContext"));
asIScriptFunction* Function = CompileIsolationFunction(Test, *Engine, ModuleName, "void Run() {}", "void Run()");
if (!LocalAssert.IsNotNull(Function, TEXT("Sequence test should compile the follow-up helper function")))
{
	return false;
}

ON_SCOPE_EXIT
{
	Function->Release();
};

{
	FAngelscriptEngineScope Scope(*Engine);
	asIScriptContext* ReturnedScopedRawContext = nullptr;
	{
		FAngelscriptPooledContextBase UnpreparedContext;
		ReturnedScopedRawContext = ScriptContextOf(UnpreparedContext);
	}

	asIScriptContext* RequestedContext = Engine->GetScriptEngine()->RequestContext();
	if (!LocalAssert.IsNotNull(RequestedContext, TEXT("Sequence test should reacquire a context")))
	{
		return false;
	}

	const bool bReusedReturnedScopedContext = LocalAssert.IsTrue(
		RequestedContext == ReturnedScopedRawContext,
		TEXT("Sequence test should reuse the returned scoped context"));
	const bool bContextTargetsCurrentEngine = LocalAssert.IsTrue(
		RequestedContext->GetEngine() == Engine->GetScriptEngine(),
		TEXT("Sequence test should reacquire a context for the current engine"));
	const int32 PrepareResult = RequestedContext->Prepare(Function);
	const int32 ExecuteResult = PrepareResult == asSUCCESS ? RequestedContext->Execute() : PrepareResult;
	Engine->GetScriptEngine()->ReturnContext(RequestedContext);
	return bReusedReturnedScopedContext
		&& bContextTargetsCurrentEngine
		&& LocalAssert.AreEqual(static_cast<int32>(asSUCCESS), PrepareResult, TEXT("Sequence test should prepare successfully"))
		&& LocalAssert.AreEqual(static_cast<int32>(asEXECUTION_FINISHED), ExecuteResult, TEXT("Sequence test should execute successfully"));
}
}

static bool RunScopedPooledContextUsesScopedEngine(FAutomationTestBase& Test)
{
FNoDiscardAsserter LocalAssert(Test);
ResetIsolationRuntime();

const FAngelscriptEngineConfig Config;
const FAngelscriptEngineDependencies Dependencies = FAngelscriptEngineDependencies::CreateDefault();
TUniquePtr<FAngelscriptEngine> EngineA = FAngelscriptTestEngine::Create(Config, Dependencies);
TUniquePtr<FAngelscriptEngine> EngineB = FAngelscriptTestEngine::Create(Config, Dependencies);

if (!LocalAssert.IsNotNull(EngineA.Get(), TEXT("Scoped pooled context test should create engine A"))
	|| !LocalAssert.IsNotNull(EngineB.Get(), TEXT("Scoped pooled context test should create engine B")))
{
	return false;
}

const FString ModuleNameA = MakeIsolationName(TEXT("ContextPoolA"));
const FString ModuleNameB = MakeIsolationName(TEXT("ContextPoolB"));
asIScriptFunction* FunctionA = CompileIsolationFunction(Test, *EngineA, ModuleNameA, "void Run() {}", "void Run()");
asIScriptFunction* FunctionB = CompileIsolationFunction(Test, *EngineB, ModuleNameB, "void Run() {}", "void Run()");
if (!LocalAssert.IsNotNull(FunctionA, TEXT("Scoped pooled context test should compile function A"))
	|| !LocalAssert.IsNotNull(FunctionB, TEXT("Scoped pooled context test should compile function B")))
{
	if (FunctionA != nullptr)
	{
		FunctionA->Release();
	}
	if (FunctionB != nullptr)
	{
		FunctionB->Release();
	}
	return false;
}

ON_SCOPE_EXIT
{
	FunctionA->Release();
	FunctionB->Release();
};

{
	FAngelscriptEngineScope ScopeA(*EngineA);
	FAngelscriptPooledContextBase SeedContext;
	asIScriptContext* SeedScriptContext = ScriptContextOf(SeedContext);
	const int32 SeedPrepareResult = SeedScriptContext->Prepare(FunctionA);
	if (!LocalAssert.AreEqual(static_cast<int32>(asSUCCESS), SeedPrepareResult, TEXT("Scoped pooled context test should seed engine A into the local pool")))
	{
		return false;
	}
}

{
	FAngelscriptEngineScope ScopeB(*EngineB);
	FAngelscriptPooledContextBase Context;
	asIScriptContext* ScriptContext = ScriptContextOf(Context);
	bool bOk = LocalAssert.IsTrue(ScriptContext->GetEngine() == EngineB->GetScriptEngine(), TEXT("Scoped pooled context should resolve to engine B under engine B scope"));

	const int32 PrepareResult = ScriptContext->Prepare(FunctionB);
	const int32 ExecuteResult = PrepareResult == asSUCCESS ? ScriptContext->Execute() : PrepareResult;
	bOk &= LocalAssert.AreEqual(static_cast<int32>(asSUCCESS), PrepareResult, TEXT("Scoped pooled context should prepare engine B function successfully"));
	bOk &= LocalAssert.AreEqual(static_cast<int32>(asEXECUTION_FINISHED), ExecuteResult, TEXT("Scoped pooled context should execute engine B function successfully"));
	return bOk;
}
}

static bool RunReusedPooledContextStartsUnprepared(FAutomationTestBase& Test)
{
FNoDiscardAsserter LocalAssert(Test);
ResetIsolationRuntime();

const FAngelscriptEngineConfig Config;
const FAngelscriptEngineDependencies Dependencies = FAngelscriptEngineDependencies::CreateDefault();
TUniquePtr<FAngelscriptEngine> Engine = FAngelscriptTestEngine::Create(Config, Dependencies);
if (!LocalAssert.IsNotNull(Engine.Get(), TEXT("Reused pooled context test should create an engine")))
{
	return false;
}

const FString ModuleName = MakeIsolationName(TEXT("ReusedContext"));
asIScriptFunction* Function = CompileIsolationFunction(Test, *Engine, ModuleName, "void Run() {}", "void Run()");
if (!LocalAssert.IsNotNull(Function, TEXT("Reused pooled context test should compile its helper function")))
{
	return false;
}

ON_SCOPE_EXIT
{
	Function->Release();
};

asIScriptContext* SeedRawContext = nullptr;
{
	FAngelscriptEngineScope Scope(*Engine);

	{
		FAngelscriptPooledContextBase SeedContext;
		SeedRawContext = ScriptContextOf(SeedContext);

		asIScriptContext* SeedScriptContext = ScriptContextOf(SeedContext);
		const int32 PrepareResult = SeedScriptContext->Prepare(Function);
		const int32 ExecuteResult = PrepareResult == asSUCCESS ? SeedScriptContext->Execute() : PrepareResult;
		if (!LocalAssert.AreEqual(static_cast<int32>(asSUCCESS), PrepareResult, TEXT("Seed pooled context should prepare successfully"))
			|| !LocalAssert.AreEqual(static_cast<int32>(asEXECUTION_FINISHED), ExecuteResult, TEXT("Seed pooled context should execute successfully")))
		{
			return false;
		}
	}

	if (!LocalAssert.IsNull(FAngelscriptEngineIsolationTestAccess::GetActiveContext(), TEXT("Reused pooled context test should clear the thread-local active context before reacquiring")))
	{
		return false;
	}

	FAngelscriptPooledContextBase ReusedContext;
	bool bOk = LocalAssert.IsTrue(ScriptContextOf(ReusedContext) == SeedRawContext, TEXT("Reused pooled context test should reacquire the pooled context"));
	// Go through the asIScriptContext* handle (obtained via the initial RequestContext call above)
	// instead of FAngelscriptPooledContextBase::operator->() which returns the incomplete asCContext* type.
	asIScriptContext* ReusedScriptContext = SeedRawContext;
	bOk &= LocalAssert.AreEqual(static_cast<int32>(asEXECUTION_UNINITIALIZED), static_cast<int32>(ReusedScriptContext->GetState()), TEXT("Reused pooled context should start unprepared after pool reuse"));

	const int32 PrepareResult = ReusedScriptContext->Prepare(Function);
	const int32 ExecuteResult = PrepareResult == asSUCCESS ? ReusedScriptContext->Execute() : PrepareResult;
	bOk &= LocalAssert.AreEqual(static_cast<int32>(asSUCCESS), PrepareResult, TEXT("Reused pooled context should prepare successfully"));
	bOk &= LocalAssert.AreEqual(static_cast<int32>(asEXECUTION_FINISHED), ExecuteResult, TEXT("Reused pooled context should execute successfully"));
	return bOk;
}
}

static bool RunEngineLocalFlagsIsolation(FAutomationTestBase& Test)
{
FNoDiscardAsserter LocalAssert(Test);
FIsolationContextStackGuard ContextGuard;

FAngelscriptEngineConfig ConfigA;
ConfigA.bSimulateCooked = true;
ConfigA.bTestErrors = true;
FAngelscriptEngineDependencies Deps = FAngelscriptEngineDependencies::CreateDefault();
TUniquePtr<FAngelscriptEngine> EngineA = FAngelscriptTestEngine::Create(ConfigA, Deps);
if (!LocalAssert.IsNotNull(EngineA.Get(), TEXT("Should create engine A with custom config")))
{
	return false;
}

FAngelscriptEngineConfig ConfigB;
ConfigB.bSimulateCooked = false;
ConfigB.bTestErrors = false;
TUniquePtr<FAngelscriptEngine> EngineB = FAngelscriptTestEngine::Create(ConfigB, Deps);
if (!LocalAssert.IsNotNull(EngineB.Get(), TEXT("Should create engine B with different config")))
{
	return false;
}

EngineA->bGeneratePrecompiledData = true;
EngineB->bGeneratePrecompiledData = false;

bool bOk = true;
bOk &= LocalAssert.IsTrue(EngineA->bSimulateCooked, TEXT("Engine A bSimulateCooked should be true"));
bOk &= LocalAssert.IsTrue(EngineA->bTestErrors, TEXT("Engine A bTestErrors should be true"));
bOk &= LocalAssert.IsFalse(EngineB->bSimulateCooked, TEXT("Engine B bSimulateCooked should be false"));
bOk &= LocalAssert.IsFalse(EngineB->bTestErrors, TEXT("Engine B bTestErrors should be false"));
bOk &= LocalAssert.IsTrue(EngineA->bGeneratePrecompiledData, TEXT("Engine A bGeneratePrecompiledData should be true"));
bOk &= LocalAssert.IsFalse(EngineB->bGeneratePrecompiledData, TEXT("Engine B bGeneratePrecompiledData should be false"));

{
	FAngelscriptEngineScope ScopeA(*EngineA);
	bOk &= LocalAssert.IsTrue(FAngelscriptEngine::IsSimulatingCookedForCurrentContext(), TEXT("IsSimulatingCookedForCurrentContext should reflect engine A"));
	bOk &= LocalAssert.IsTrue(FAngelscriptEngine::IsTestingErrorsForCurrentContext(), TEXT("IsTestingErrorsForCurrentContext should reflect engine A"));
	bOk &= LocalAssert.IsTrue(FAngelscriptEngine::IsGeneratingPrecompiledData(), TEXT("IsGeneratingPrecompiledData should reflect engine A"));
}
{
	FAngelscriptEngineScope ScopeB(*EngineB);
	bOk &= LocalAssert.IsFalse(FAngelscriptEngine::IsSimulatingCookedForCurrentContext(), TEXT("IsSimulatingCookedForCurrentContext should reflect engine B"));
	bOk &= LocalAssert.IsFalse(FAngelscriptEngine::IsTestingErrorsForCurrentContext(), TEXT("IsTestingErrorsForCurrentContext should reflect engine B"));
	bOk &= LocalAssert.IsFalse(FAngelscriptEngine::IsGeneratingPrecompiledData(), TEXT("IsGeneratingPrecompiledData should reflect engine B"));
}

return bOk;
}

static bool RunEngineLocalBlueprintLibraryNamespaceRuleConsistency(FAutomationTestBase& Test)
{
FNoDiscardAsserter LocalAssert(Test);
FIsolationContextStackGuard ContextGuard;

const FAngelscriptEngineConfig Config;
const FAngelscriptEngineDependencies Deps = FAngelscriptEngineDependencies::CreateDefault();
TUniquePtr<FAngelscriptEngine> EngineA = FAngelscriptTestEngine::Create(Config, Deps);
TUniquePtr<FAngelscriptEngine> EngineB = FAngelscriptTestEngine::Create(Config, Deps);
if (!LocalAssert.IsNotNull(EngineA.Get(), TEXT("Namespace isolation should create engine A"))
	|| !LocalAssert.IsNotNull(EngineB.Get(), TEXT("Namespace isolation should create engine B")))
{
	return false;
}

UFunction* Function = USubsystemLibrary::StaticClass()->FindFunctionByName(TEXT("GetEngineSubsystem"));
if (!LocalAssert.IsNotNull(Function, TEXT("Blueprint namespace rule should find GetEngineSubsystem")))
{
	return false;
}

bool bOk = true;
{
	FAngelscriptEngineScope ScopeA(*EngineA);
	TSharedPtr<FAngelscriptType> HostType = FAngelscriptType::GetByClass(USubsystemLibrary::StaticClass());
	if (!LocalAssert.IsTrue(HostType.IsValid(), TEXT("Engine A should resolve the subsystem library host type")))
	{
		return false;
	}

	FAngelscriptFunctionSignature Signature(HostType.ToSharedRef(), Function);
	bOk &= LocalAssert.AreEqual(FString(TEXT("USubsystemLibrary")), Signature.ClassName, TEXT("Engine A should use the full registered AS type namespace"));
	bOk &= LocalAssert.IsTrue(Signature.bStaticInScript, TEXT("Engine A should bind the helper as a static script function"));
}

{
	FAngelscriptEngineScope ScopeB(*EngineB);
	TSharedPtr<FAngelscriptType> HostType = FAngelscriptType::GetByClass(USubsystemLibrary::StaticClass());
	if (!LocalAssert.IsTrue(HostType.IsValid(), TEXT("Engine B should resolve the subsystem library host type")))
	{
		return false;
	}

	FAngelscriptFunctionSignature Signature(HostType.ToSharedRef(), Function);
	bOk &= LocalAssert.AreEqual(FString(TEXT("USubsystemLibrary")), Signature.ClassName, TEXT("Engine B should use the same full registered AS type namespace"));
	bOk &= LocalAssert.IsTrue(Signature.bStaticInScript, TEXT("Engine B should bind the helper as a static script function"));
}

return bOk;
}

static bool RunEngineLocalStaticNamesIsolation(FAutomationTestBase& Test)
{
FNoDiscardAsserter LocalAssert(Test);
FIsolationContextStackGuard ContextGuard;

const FAngelscriptEngineConfig Config;
const FAngelscriptEngineDependencies Deps = FAngelscriptEngineDependencies::CreateDefault();
TUniquePtr<FAngelscriptEngine> EngineA = FAngelscriptTestEngine::Create(Config, Deps);
TUniquePtr<FAngelscriptEngine> EngineB = FAngelscriptTestEngine::Create(Config, Deps);
if (!LocalAssert.IsNotNull(EngineA.Get(), TEXT("Static-name isolation should create engine A"))
	|| !LocalAssert.IsNotNull(EngineB.Get(), TEXT("Static-name isolation should create engine B")))
{
	return false;
}

const FName EngineAName(*MakeIsolationName(TEXT("StaticNameA")));
const FName CloneName(*MakeIsolationName(TEXT("StaticNameClone")));
int32 EngineANameIndex = INDEX_NONE;
int32 CloneNameIndex = INDEX_NONE;
int32 EngineABaselineCount = 0;

{
	FAngelscriptEngineScope ScopeA(*EngineA);
	EngineABaselineCount = FAngelscriptEngine::GetStaticNameCount();
	EngineANameIndex = FAngelscriptEngine::GetOrAddStaticName(EngineAName);
	bool bOk = LocalAssert.AreEqual(EngineABaselineCount + 1, FAngelscriptEngine::GetStaticNameCount(), TEXT("Engine A should append its own static name"));

	FName ResolvedName;
	bOk &= LocalAssert.IsTrue(FAngelscriptEngine::TryGetStaticName(EngineANameIndex, ResolvedName), TEXT("Engine A should resolve its static name by index"));
	bOk &= LocalAssert.AreEqual(EngineAName.ToString(), ResolvedName.ToString(), TEXT("Engine A static-name index should resolve to the added name"));
	if (!bOk)
	{
		return false;
	}
}

{
	FAngelscriptEngineScope ScopeB(*EngineB);
	const int32 EngineBBaselineCount = FAngelscriptEngine::GetStaticNameCount();
	FName ResolvedName;
	const bool bEngineBSeesEngineAName = FAngelscriptEngine::TryGetStaticName(EngineANameIndex, ResolvedName) && ResolvedName == EngineAName;
	bool bOk = LocalAssert.IsFalse(bEngineBSeesEngineAName, TEXT("Engine B should not see static names added through engine A"));
	bOk &= LocalAssert.AreEqual(EngineBBaselineCount, FAngelscriptEngine::GetStaticNameCount(), TEXT("Engine B static-name count should stay isolated"));
	if (!bOk)
	{
		return false;
	}
}

// The Clone-shares-source-state portion of this test was removed when
// the Clone mechanism was deleted. Two independent Full engines have
// independent static-name registries by construction; the EngineA / EngineB
// assertions above already cover that contract.

return true;
}

public:
	TEST_METHOD(ContextStackScopedResolution)
	{
RunContextStackScopedResolution(*TestRunner);
	}

	TEST_METHOD(EngineScopeRestoresWorldContext)
	{
RunEngineScopeRestoresWorldContext(*TestRunner);
	}

	TEST_METHOD(FullEnginesKeepStateSeparate)
	{
RunFullEnginesKeepStateSeparate(*TestRunner);
	}

	TEST_METHOD(CloneSharesSourceState)
	{
RunCloneSharesSourceState(*TestRunner);
	}

	TEST_METHOD(RequestContextUsesRequestedEngine)
	{
RunRequestContextUsesRequestedEngine(*TestRunner);
	}

	TEST_METHOD(RequestContextReusedStartsUnprepared)
	{
RunRequestContextReusedStartsUnprepared(*TestRunner);
	}

	TEST_METHOD(RequestContextAfterReturningUnpreparedScopedContext)
	{
RunRequestContextAfterReturningUnpreparedScopedContext(*TestRunner);
	}

	TEST_METHOD(FullEngineCreateClearsThreadLocalPool)
	{
RunFullEngineCreateClearsThreadLocalPool(*TestRunner);
	}

	TEST_METHOD(ContextPoolResetSequenceKeepsRequestedContextReusable)
	{
RunContextPoolResetSequenceKeepsRequestedContextReusable(*TestRunner);
	}

	TEST_METHOD(ScopedPooledContextUsesScopedEngine)
	{
RunScopedPooledContextUsesScopedEngine(*TestRunner);
	}

	TEST_METHOD(ReusedPooledContextStartsUnprepared)
	{
RunReusedPooledContextStartsUnprepared(*TestRunner);
	}

	TEST_METHOD(EngineLocalFlagsIsolation)
	{
RunEngineLocalFlagsIsolation(*TestRunner);
	}

	TEST_METHOD(EngineLocalBlueprintLibraryNamespaceRuleConsistency)
	{
RunEngineLocalBlueprintLibraryNamespaceRuleConsistency(*TestRunner);
	}

	TEST_METHOD(EngineLocalStaticNamesIsolation)
	{
RunEngineLocalStaticNamesIsolation(*TestRunner);
	}

};

#endif
