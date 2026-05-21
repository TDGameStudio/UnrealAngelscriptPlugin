#include "AngelscriptEngine.h"
#include "AngelscriptGameInstanceSubsystem.h"
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
#include "Shared/AngelscriptTestEngine.h"
#include "UObject/UObjectGlobals.h"

#include "AngelscriptInclude.h"

#if WITH_DEV_AUTOMATION_TESTS

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

namespace AngelscriptTest_Core_AngelscriptEngineIsolationTests_Private
{

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
	if (!UAngelscriptGameInstanceSubsystem::HasAnyTickOwner() && FAngelscriptEngine::IsInitialized())
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
	FAngelscriptEngineScope GlobalScope(Engine);

	asIScriptModule* Module = Engine.GetScriptEngine()->GetModule(TCHAR_TO_ANSI(*ModuleName), asGM_ALWAYS_CREATE);
	if (!Test.TestNotNull(*FString::Printf(TEXT("Isolation helper should create module '%s'"), *ModuleName), Module))
	{
		return nullptr;
	}

	asIScriptFunction* Function = nullptr;
	const int32 CompileResult = Module->CompileFunction(TCHAR_TO_ANSI(*ModuleName), Source, 0, 0, &Function);
	if (!Test.TestEqual(*FString::Printf(TEXT("Isolation helper should compile '%s'"), *ModuleName), CompileResult, asSUCCESS))
	{
		return nullptr;
	}

	Test.TestNotNull(*FString::Printf(TEXT("Isolation helper should resolve '%s'"), ANSI_TO_TCHAR(Declaration)), Function);
	return Function;
}

bool RunContextStackScopedResolution(FAutomationTestBase& Test)
{
	ResetIsolationRuntime();
	FIsolationContextStackGuard StackGuard;

	const FAngelscriptEngineConfig Config;
	const FAngelscriptEngineDependencies Dependencies = FAngelscriptEngineDependencies::CreateDefault();
	TUniquePtr<FAngelscriptEngine> PrimaryEngine = FAngelscriptTestEngine::Create(Config, Dependencies);
	TUniquePtr<FAngelscriptEngine> SecondaryEngine = FAngelscriptTestEngine::Create(Config, Dependencies);

	if (!Test.TestNotNull(TEXT("Context stack scoped resolution should create a primary engine"), PrimaryEngine.Get())
		|| !Test.TestNotNull(TEXT("Context stack scoped resolution should create a secondary engine"), SecondaryEngine.Get()))
	{
		return false;
	}

	Test.TestTrue(TEXT("Context stack should start empty after guard clears it"), FAngelscriptEngineContextStack::IsEmpty());

	{
		FAngelscriptEngineScope PrimaryScope(*PrimaryEngine);
		Test.TestTrue(TEXT("Scoped resolution should return the primary engine while its scope is active"), &FAngelscriptEngine::Get() == PrimaryEngine.Get());
		Test.TestTrue(TEXT("Context stack should expose the active primary engine"), FAngelscriptEngineContextStack::Peek() == PrimaryEngine.Get());

		{
			FAngelscriptEngineScope SecondaryScope(*SecondaryEngine);
			Test.TestTrue(TEXT("Nested scoped resolution should prefer the nested engine"), &FAngelscriptEngine::Get() == SecondaryEngine.Get());
			Test.TestTrue(TEXT("Context stack should update its top entry for nested scopes"), FAngelscriptEngineContextStack::Peek() == SecondaryEngine.Get());
		}

		Test.TestTrue(TEXT("Nested scope teardown should restore the previous engine"), &FAngelscriptEngine::Get() == PrimaryEngine.Get());
		Test.TestTrue(TEXT("Context stack should restore the previous engine after nested scope teardown"), FAngelscriptEngineContextStack::Peek() == PrimaryEngine.Get());
	}

	return Test.TestTrue(TEXT("Context stack should be empty after all scopes leave"), FAngelscriptEngineContextStack::IsEmpty());
}

bool RunEngineScopeRestoresWorldContext(FAutomationTestBase& Test)
{
	ResetIsolationRuntime();

	const FAngelscriptEngineConfig Config;
	const FAngelscriptEngineDependencies Dependencies = FAngelscriptEngineDependencies::CreateDefault();
	TUniquePtr<FAngelscriptEngine> PrimaryEngine = FAngelscriptTestEngine::Create(Config, Dependencies);
	TUniquePtr<FAngelscriptEngine> SecondaryEngine = FAngelscriptTestEngine::Create(Config, Dependencies);

	if (!Test.TestNotNull(TEXT("Engine scope restore test should create a primary engine"), PrimaryEngine.Get())
		|| !Test.TestNotNull(TEXT("Engine scope restore test should create a secondary engine"), SecondaryEngine.Get()))
	{
		return false;
	}

	UObject* OuterContext = NewObject<UCurveFloat>();
	UObject* InnerContext = NewObject<UCurveFloat>();
	if (!Test.TestNotNull(TEXT("Engine scope restore test should create an outer context object"), OuterContext)
		|| !Test.TestNotNull(TEXT("Engine scope restore test should create an inner context object"), InnerContext))
	{
		return false;
	}

	{
		FAngelscriptEngineScope OuterScope(*PrimaryEngine, OuterContext);
		Test.TestTrue(TEXT("Outer scope should expose its world context through the active engine"), PrimaryEngine->GetCurrentWorldContextObject() == OuterContext);

		{
			FAngelscriptEngineScope InnerScope(*SecondaryEngine, InnerContext);
			Test.TestTrue(TEXT("Inner scope should expose its world context through the nested engine"), SecondaryEngine->GetCurrentWorldContextObject() == InnerContext);
		}

		Test.TestTrue(TEXT("Leaving the inner scope should restore the outer world context"), PrimaryEngine->GetCurrentWorldContextObject() == OuterContext);
	}

	return Test.TestNull(TEXT("Leaving the outer scope should clear the world context"), PrimaryEngine->GetCurrentWorldContextObject());
}

bool RunFullEnginesKeepStateSeparate(FAutomationTestBase& Test)
{
	ResetIsolationRuntime();

	const FAngelscriptEngineConfig Config;
	const FAngelscriptEngineDependencies Dependencies = FAngelscriptEngineDependencies::CreateDefault();
	TUniquePtr<FAngelscriptEngine> EngineA = FAngelscriptTestEngine::Create(Config, Dependencies);
	TUniquePtr<FAngelscriptEngine> EngineB = FAngelscriptTestEngine::Create(Config, Dependencies);

	if (!Test.TestNotNull(TEXT("Full engine isolation test should create engine A"), EngineA.Get())
		|| !Test.TestNotNull(TEXT("Full engine isolation test should create engine B"), EngineB.Get()))
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
		if (!Test.TestTrue(TEXT("Full engine isolation test should resolve the built-in int type inside engine A"), IntType.IsValid()))
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
		Test.TestNull(TEXT("Engine B should not see aliases registered through engine A"), FAngelscriptType::GetByAngelscriptTypeName(AliasName).Get());
		Test.TestFalse(TEXT("Engine B should not inherit skip entries registered through engine A"), FAngelscriptBinds::CheckForSkipEntry(FName(TEXT("EngineIsolationActor")), FName(TEXT("OnlyEngineA"))));
		Test.TestEqual(TEXT("Engine B should keep its original ToString registry baseline"), FAngelscriptEngineIsolationTestAccess::GetToStringCount(*EngineB), BaselineToStringCountB);
		Test.TestEqual(TEXT("Engine B should keep its original bind database baseline"), FAngelscriptEngineIsolationTestAccess::GetBindDatabaseClassCount(*EngineB), BaselineBindDatabaseClassCountB);
	}

	{
		FAngelscriptEngineScope ScopeA(*EngineA);
		Test.TestNotNull(TEXT("Engine A should keep its alias registration"), FAngelscriptType::GetByAngelscriptTypeName(AliasName).Get());
		Test.TestTrue(TEXT("Engine A should keep its skip entry registration"), FAngelscriptBinds::CheckForSkipEntry(FName(TEXT("EngineIsolationActor")), FName(TEXT("OnlyEngineA"))));
		Test.TestEqual(TEXT("Engine A should retain its extra ToString registry entry"), FAngelscriptEngineIsolationTestAccess::GetToStringCount(*EngineA), BaselineToStringCountA + 1);
		Test.TestEqual(TEXT("Engine A should retain its extra bind database class"), FAngelscriptEngineIsolationTestAccess::GetBindDatabaseClassCount(*EngineA), BaselineBindDatabaseClassCountA + 1);
	}

	return true;
}

bool RunCloneSharesSourceState(FAutomationTestBase& Test)
{
	// Test removed: targeted Clone engines sharing aliases / skip entries /
	// ToString registry with their source engine, which no longer exists
	// after clone-removal. Independent Full engines have independent state.
	return true;
}

bool RunRequestContextUsesRequestedEngine(FAutomationTestBase& Test)
{
	ResetIsolationRuntime();

	const FAngelscriptEngineConfig Config;
	const FAngelscriptEngineDependencies Dependencies = FAngelscriptEngineDependencies::CreateDefault();
	TUniquePtr<FAngelscriptEngine> EngineA = FAngelscriptTestEngine::Create(Config, Dependencies);
	TUniquePtr<FAngelscriptEngine> EngineB = FAngelscriptTestEngine::Create(Config, Dependencies);

	if (!Test.TestNotNull(TEXT("RequestContext isolation test should create engine A"), EngineA.Get())
		|| !Test.TestNotNull(TEXT("RequestContext isolation test should create engine B"), EngineB.Get()))
	{
		return false;
	}

	{
		FAngelscriptEngineScope ScopeA(*EngineA);
		asIScriptContext* ContextA = EngineA->GetScriptEngine()->RequestContext();
		if (!Test.TestNotNull(TEXT("RequestContext isolation test should acquire a context from engine A"), ContextA))
		{
			return false;
		}

		Test.TestTrue(TEXT("Requested context A should belong to engine A"), ContextA->GetEngine() == EngineA->GetScriptEngine());
		EngineA->GetScriptEngine()->ReturnContext(ContextA);
	}

	{
		FAngelscriptEngineScope ScopeB(*EngineB);
		asIScriptContext* ContextB = EngineB->GetScriptEngine()->RequestContext();
		if (!Test.TestNotNull(TEXT("RequestContext isolation test should acquire a context from engine B"), ContextB))
		{
			return false;
		}

		const bool bMatchesRequestedEngine = Test.TestTrue(
			TEXT("RequestContext should not recycle a context from another engine"),
			ContextB->GetEngine() == EngineB->GetScriptEngine());
		EngineB->GetScriptEngine()->ReturnContext(ContextB);
		return bMatchesRequestedEngine;
	}
}

bool RunRequestContextReusedStartsUnprepared(FAutomationTestBase& Test)
{
	ResetIsolationRuntime();

	const FAngelscriptEngineConfig Config;
	const FAngelscriptEngineDependencies Dependencies = FAngelscriptEngineDependencies::CreateDefault();
	TUniquePtr<FAngelscriptEngine> Engine = FAngelscriptTestEngine::Create(Config, Dependencies);
	if (!Test.TestNotNull(TEXT("RequestContext reuse test should create an engine"), Engine.Get()))
	{
		return false;
	}

	const FString ModuleName = MakeIsolationName(TEXT("RequestContextReuse"));
	asIScriptFunction* Function = CompileIsolationFunction(Test, *Engine, ModuleName, "void Run() {}", "void Run()");
	if (!Test.TestNotNull(TEXT("RequestContext reuse test should compile its helper function"), Function))
	{
		return false;
	}

	ON_SCOPE_EXIT
	{
		Function->Release();
	};

	FAngelscriptEngineScope Scope(*Engine);

	asIScriptContext* SeedRawContext = Engine->GetScriptEngine()->RequestContext();
	if (!Test.TestNotNull(TEXT("RequestContext reuse test should acquire a seed context"), SeedRawContext))
	{
		return false;
	}

	const int32 PrepareResult = SeedRawContext->Prepare(Function);
	const int32 ExecuteResult = PrepareResult == asSUCCESS ? SeedRawContext->Execute() : PrepareResult;
	if (!Test.TestEqual(TEXT("Seed RequestContext should prepare successfully"), PrepareResult, asSUCCESS)
		|| !Test.TestEqual(TEXT("Seed RequestContext should execute successfully"), ExecuteResult, asEXECUTION_FINISHED))
	{
		Engine->GetScriptEngine()->ReturnContext(SeedRawContext);
		return false;
	}

	Engine->GetScriptEngine()->ReturnContext(SeedRawContext);

	asIScriptContext* ReusedContext = Engine->GetScriptEngine()->RequestContext();
	if (!Test.TestNotNull(TEXT("RequestContext reuse test should reacquire a context"), ReusedContext))
	{
		return false;
	}

	const bool bReusedSameContext = Test.TestTrue(
		TEXT("RequestContext reuse test should reacquire the pooled context"),
		ReusedContext == SeedRawContext);
	const bool bStartsUnprepared = Test.TestEqual(
		TEXT("Reused RequestContext should start unprepared after pool reuse"),
		(int32)ReusedContext->GetState(),
		(int32)asEXECUTION_UNINITIALIZED);
	Engine->GetScriptEngine()->ReturnContext(ReusedContext);
	return bReusedSameContext && bStartsUnprepared;
}

bool RunRequestContextAfterReturningUnpreparedScopedContext(FAutomationTestBase& Test)
{
	ResetIsolationRuntime();

	const FAngelscriptEngineConfig Config;
	const FAngelscriptEngineDependencies Dependencies = FAngelscriptEngineDependencies::CreateDefault();
	TUniquePtr<FAngelscriptEngine> Engine = FAngelscriptTestEngine::Create(Config, Dependencies);
	if (!Test.TestNotNull(TEXT("RequestContext after unprepared scoped context test should create an engine"), Engine.Get()))
	{
		return false;
	}

	const FString ModuleName = MakeIsolationName(TEXT("RequestContextAfterUnpreparedScopedContext"));
	asIScriptFunction* Function = CompileIsolationFunction(Test, *Engine, ModuleName, "void Run() {}", "void Run()");
	if (!Test.TestNotNull(TEXT("RequestContext after unprepared scoped context test should compile its helper function"), Function))
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
		if (!Test.TestNotNull(TEXT("RequestContext after unprepared scoped context test should reacquire a context"), RequestedContext))
		{
			return false;
		}

		const bool bReusedReturnedScopedContext = Test.TestTrue(
			TEXT("RequestContext after unprepared scoped context test should reuse the returned scoped context"),
			RequestedContext == ReturnedScopedRawContext);
		const int32 PrepareResult = RequestedContext->Prepare(Function);
		const int32 ExecuteResult = PrepareResult == asSUCCESS ? RequestedContext->Execute() : PrepareResult;
		Engine->GetScriptEngine()->ReturnContext(RequestedContext);
		return bReusedReturnedScopedContext
			&& Test.TestEqual(TEXT("RequestContext after unprepared scoped context test should prepare successfully"), PrepareResult, asSUCCESS)
			&& Test.TestEqual(TEXT("RequestContext after unprepared scoped context test should execute successfully"), ExecuteResult, asEXECUTION_FINISHED);
	}
}

bool RunFullEngineCreateClearsThreadLocalPool(FAutomationTestBase& Test)
{
	ResetIsolationRuntime();

	const FAngelscriptEngineConfig Config;
	const FAngelscriptEngineDependencies Dependencies = FAngelscriptEngineDependencies::CreateDefault();
	TUniquePtr<FAngelscriptEngine> EngineA = FAngelscriptTestEngine::Create(Config, Dependencies);
	if (!Test.TestNotNull(TEXT("Full engine create pool reset test should create engine A"), EngineA.Get()))
	{
		return false;
	}

	const FString ModuleNameA = MakeIsolationName(TEXT("FullCreatePoolResetA"));
	asIScriptFunction* FunctionA = CompileIsolationFunction(Test, *EngineA, ModuleNameA, "void Run() {}", "void Run()");
	if (!Test.TestNotNull(TEXT("Full engine create pool reset test should compile function A"), FunctionA))
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
		if (!Test.TestEqual(TEXT("Full engine create pool reset test should seed engine A into the local pool"), PrepareResult, asSUCCESS)
			|| !Test.TestEqual(TEXT("Full engine create pool reset test should execute the seeded function"), ExecuteResult, asEXECUTION_FINISHED))
		{
			return false;
		}
	}

	if (!Test.TestTrue(
		TEXT("Full engine create pool reset test should leave a free pooled context for engine A before creating a new full engine"),
		FAngelscriptEngineIsolationTestAccess::GetLocalPooledContextCount(EngineA->GetScriptEngine()) > 0))
	{
		return false;
	}

	TUniquePtr<FAngelscriptEngine> EngineB = FAngelscriptTestEngine::Create(Config, Dependencies);
	if (!Test.TestNotNull(TEXT("Full engine create pool reset test should create engine B"), EngineB.Get()))
	{
		return false;
	}

	if (!Test.TestEqual(
		TEXT("Creating a new full engine should clear stale free contexts from the thread-local pool"),
		FAngelscriptEngineIsolationTestAccess::GetLocalPooledContextCount(nullptr),
		0))
	{
		return false;
	}

	{
		FAngelscriptEngineScope ScopeB(*EngineB);
		FAngelscriptPooledContextBase FreshContext;
		return Test.TestTrue(
			TEXT("Creating a new full engine should acquire a context bound to that engine"),
			ScriptContextOf(FreshContext)->GetEngine() == EngineB->GetScriptEngine());
	}
}

bool RunContextPoolResetSequenceKeepsRequestedContextReusable(FAutomationTestBase& Test)
{
	ResetIsolationRuntime();

	const FAngelscriptEngineConfig Config;
	const FAngelscriptEngineDependencies Dependencies = FAngelscriptEngineDependencies::CreateDefault();

	{
		TUniquePtr<FAngelscriptEngine> EngineA = FAngelscriptTestEngine::Create(Config, Dependencies);
		if (!Test.TestNotNull(TEXT("Sequence test should create engine A"), EngineA.Get()))
		{
			return false;
		}

		const FString ModuleNameA = MakeIsolationName(TEXT("SequenceFullCreatePoolResetA"));
		asIScriptFunction* FunctionA = CompileIsolationFunction(Test, *EngineA, ModuleNameA, "void Run() {}", "void Run()");
		if (!Test.TestNotNull(TEXT("Sequence test should compile function A"), FunctionA))
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
			if (!Test.TestEqual(TEXT("Sequence test should seed engine A into the local pool"), PrepareResult, asSUCCESS)
				|| !Test.TestEqual(TEXT("Sequence test should execute the seeded function"), ExecuteResult, asEXECUTION_FINISHED))
			{
				return false;
			}
		}

		if (!Test.TestTrue(
			TEXT("Sequence test should leave a free pooled context for engine A before creating engine B"),
			FAngelscriptEngineIsolationTestAccess::GetLocalPooledContextCount(EngineA->GetScriptEngine()) > 0))
		{
			return false;
		}

		TUniquePtr<FAngelscriptEngine> EngineB = FAngelscriptTestEngine::Create(Config, Dependencies);
		if (!Test.TestNotNull(TEXT("Sequence test should create engine B"), EngineB.Get()))
		{
			return false;
		}

		if (!Test.TestEqual(
			TEXT("Sequence test should clear stale free contexts when engine B starts"),
			FAngelscriptEngineIsolationTestAccess::GetLocalPooledContextCount(nullptr),
			0))
		{
			return false;
		}

		{
			FAngelscriptEngineScope ScopeB(*EngineB);
			FAngelscriptPooledContextBase FreshContext;
			if (!Test.TestTrue(
				TEXT("Sequence test should acquire a context bound to engine B"),
				ScriptContextOf(FreshContext)->GetEngine() == EngineB->GetScriptEngine()))
			{
				return false;
			}
		}
	}

	if (!Test.TestEqual(
		TEXT("Sequence test should leave no pooled contexts behind after the full-engine phase"),
		FAngelscriptEngineIsolationTestAccess::GetLocalPooledContextCount(nullptr),
		0))
	{
		return false;
	}

	TUniquePtr<FAngelscriptEngine> Engine = FAngelscriptTestEngine::Create(Config, Dependencies);
	if (!Test.TestNotNull(TEXT("Sequence test should create the follow-up engine"), Engine.Get()))
	{
		return false;
	}

	const FString ModuleName = MakeIsolationName(TEXT("SequenceRequestContextAfterUnpreparedScopedContext"));
	asIScriptFunction* Function = CompileIsolationFunction(Test, *Engine, ModuleName, "void Run() {}", "void Run()");
	if (!Test.TestNotNull(TEXT("Sequence test should compile the follow-up helper function"), Function))
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
		if (!Test.TestNotNull(TEXT("Sequence test should reacquire a context"), RequestedContext))
		{
			return false;
		}

		const bool bReusedReturnedScopedContext = Test.TestTrue(
			TEXT("Sequence test should reuse the returned scoped context"),
			RequestedContext == ReturnedScopedRawContext);
		const bool bContextTargetsCurrentEngine = Test.TestTrue(
			TEXT("Sequence test should reacquire a context for the current engine"),
			RequestedContext->GetEngine() == Engine->GetScriptEngine());
		const int32 PrepareResult = RequestedContext->Prepare(Function);
		const int32 ExecuteResult = PrepareResult == asSUCCESS ? RequestedContext->Execute() : PrepareResult;
		Engine->GetScriptEngine()->ReturnContext(RequestedContext);
		return bReusedReturnedScopedContext
			&& bContextTargetsCurrentEngine
			&& Test.TestEqual(TEXT("Sequence test should prepare successfully"), PrepareResult, asSUCCESS)
			&& Test.TestEqual(TEXT("Sequence test should execute successfully"), ExecuteResult, asEXECUTION_FINISHED);
	}
}

bool RunScopedPooledContextUsesScopedEngine(FAutomationTestBase& Test)
{
	ResetIsolationRuntime();

	const FAngelscriptEngineConfig Config;
	const FAngelscriptEngineDependencies Dependencies = FAngelscriptEngineDependencies::CreateDefault();
	TUniquePtr<FAngelscriptEngine> EngineA = FAngelscriptTestEngine::Create(Config, Dependencies);
	TUniquePtr<FAngelscriptEngine> EngineB = FAngelscriptTestEngine::Create(Config, Dependencies);

	if (!Test.TestNotNull(TEXT("Scoped pooled context test should create engine A"), EngineA.Get())
		|| !Test.TestNotNull(TEXT("Scoped pooled context test should create engine B"), EngineB.Get()))
	{
		return false;
	}

	const FString ModuleNameA = MakeIsolationName(TEXT("ContextPoolA"));
	const FString ModuleNameB = MakeIsolationName(TEXT("ContextPoolB"));
	asIScriptFunction* FunctionA = CompileIsolationFunction(Test, *EngineA, ModuleNameA, "void Run() {}", "void Run()");
	asIScriptFunction* FunctionB = CompileIsolationFunction(Test, *EngineB, ModuleNameB, "void Run() {}", "void Run()");
	if (!Test.TestNotNull(TEXT("Scoped pooled context test should compile function A"), FunctionA)
		|| !Test.TestNotNull(TEXT("Scoped pooled context test should compile function B"), FunctionB))
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
		if (!Test.TestEqual(TEXT("Scoped pooled context test should seed engine A into the local pool"), SeedPrepareResult, asSUCCESS))
		{
			return false;
		}
	}

	{
		FAngelscriptEngineScope ScopeB(*EngineB);
		FAngelscriptPooledContextBase Context;
		asIScriptContext* ScriptContext = ScriptContextOf(Context);
		Test.TestTrue(TEXT("Scoped pooled context should resolve to engine B under engine B scope"), ScriptContext->GetEngine() == EngineB->GetScriptEngine());

		const int32 PrepareResult = ScriptContext->Prepare(FunctionB);
		const int32 ExecuteResult = PrepareResult == asSUCCESS ? ScriptContext->Execute() : PrepareResult;
		Test.TestEqual(TEXT("Scoped pooled context should prepare engine B function successfully"), PrepareResult, asSUCCESS);
		return Test.TestEqual(TEXT("Scoped pooled context should execute engine B function successfully"), ExecuteResult, asEXECUTION_FINISHED);
	}
}

bool RunReusedPooledContextStartsUnprepared(FAutomationTestBase& Test)
{
	ResetIsolationRuntime();

	const FAngelscriptEngineConfig Config;
	const FAngelscriptEngineDependencies Dependencies = FAngelscriptEngineDependencies::CreateDefault();
	TUniquePtr<FAngelscriptEngine> Engine = FAngelscriptTestEngine::Create(Config, Dependencies);
	if (!Test.TestNotNull(TEXT("Reused pooled context test should create an engine"), Engine.Get()))
	{
		return false;
	}

	const FString ModuleName = MakeIsolationName(TEXT("ReusedContext"));
	asIScriptFunction* Function = CompileIsolationFunction(Test, *Engine, ModuleName, "void Run() {}", "void Run()");
	if (!Test.TestNotNull(TEXT("Reused pooled context test should compile its helper function"), Function))
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
			if (!Test.TestEqual(TEXT("Seed pooled context should prepare successfully"), PrepareResult, asSUCCESS)
				|| !Test.TestEqual(TEXT("Seed pooled context should execute successfully"), ExecuteResult, asEXECUTION_FINISHED))
			{
				return false;
			}
		}

		if (!Test.TestNull(TEXT("Reused pooled context test should clear the thread-local active context before reacquiring"), FAngelscriptEngineIsolationTestAccess::GetActiveContext()))
		{
			return false;
		}

		FAngelscriptPooledContextBase ReusedContext;
		Test.TestTrue(TEXT("Reused pooled context test should reacquire the pooled context"), ScriptContextOf(ReusedContext) == SeedRawContext);
		// Go through the asIScriptContext* handle (obtained via the initial RequestContext call above)
		// instead of FAngelscriptPooledContextBase::operator->() which returns the incomplete asCContext* type.
		asIScriptContext* ReusedScriptContext = SeedRawContext;
		Test.TestEqual(TEXT("Reused pooled context should start unprepared after pool reuse"), (int32)ReusedScriptContext->GetState(), (int32)asEXECUTION_UNINITIALIZED);

		const int32 PrepareResult = ReusedScriptContext->Prepare(Function);
		const int32 ExecuteResult = PrepareResult == asSUCCESS ? ReusedScriptContext->Execute() : PrepareResult;
		Test.TestEqual(TEXT("Reused pooled context should prepare successfully"), PrepareResult, asSUCCESS);
		return Test.TestEqual(TEXT("Reused pooled context should execute successfully"), ExecuteResult, asEXECUTION_FINISHED);
	}
}

bool RunEngineLocalFlagsIsolation(FAutomationTestBase& Test)
{
	FIsolationContextStackGuard ContextGuard;

	FAngelscriptEngineConfig ConfigA;
	ConfigA.bSimulateCooked = true;
	ConfigA.bTestErrors = true;
	FAngelscriptEngineDependencies Deps = FAngelscriptEngineDependencies::CreateDefault();
	TUniquePtr<FAngelscriptEngine> EngineA = FAngelscriptTestEngine::Create(ConfigA, Deps);
	if (!Test.TestNotNull(TEXT("Should create engine A with custom config"), EngineA.Get()))
	{
		return false;
	}

	FAngelscriptEngineConfig ConfigB;
	ConfigB.bSimulateCooked = false;
	ConfigB.bTestErrors = false;
	TUniquePtr<FAngelscriptEngine> EngineB = FAngelscriptTestEngine::Create(ConfigB, Deps);
	if (!Test.TestNotNull(TEXT("Should create engine B with different config"), EngineB.Get()))
	{
		return false;
	}

	EngineA->bGeneratePrecompiledData = true;
	EngineB->bGeneratePrecompiledData = false;

	Test.TestTrue(TEXT("Engine A bSimulateCooked should be true"), EngineA->bSimulateCooked);
	Test.TestTrue(TEXT("Engine A bTestErrors should be true"), EngineA->bTestErrors);
	Test.TestFalse(TEXT("Engine B bSimulateCooked should be false"), EngineB->bSimulateCooked);
	Test.TestFalse(TEXT("Engine B bTestErrors should be false"), EngineB->bTestErrors);
	Test.TestTrue(TEXT("Engine A bGeneratePrecompiledData should be true"), EngineA->bGeneratePrecompiledData);
	Test.TestFalse(TEXT("Engine B bGeneratePrecompiledData should be false"), EngineB->bGeneratePrecompiledData);

	{
		FAngelscriptEngineScope ScopeA(*EngineA);
		Test.TestTrue(TEXT("IsSimulatingCookedForCurrentContext should reflect engine A"), FAngelscriptEngine::IsSimulatingCookedForCurrentContext());
		Test.TestTrue(TEXT("IsTestingErrorsForCurrentContext should reflect engine A"), FAngelscriptEngine::IsTestingErrorsForCurrentContext());
		Test.TestTrue(TEXT("IsGeneratingPrecompiledData should reflect engine A"), FAngelscriptEngine::IsGeneratingPrecompiledData());
	}
	{
		FAngelscriptEngineScope ScopeB(*EngineB);
		Test.TestFalse(TEXT("IsSimulatingCookedForCurrentContext should reflect engine B"), FAngelscriptEngine::IsSimulatingCookedForCurrentContext());
		Test.TestFalse(TEXT("IsTestingErrorsForCurrentContext should reflect engine B"), FAngelscriptEngine::IsTestingErrorsForCurrentContext());
		Test.TestFalse(TEXT("IsGeneratingPrecompiledData should reflect engine B"), FAngelscriptEngine::IsGeneratingPrecompiledData());
	}

	return true;
}

bool RunEngineLocalBlueprintLibraryNamespaceRuleConsistency(FAutomationTestBase& Test)
{
	FIsolationContextStackGuard ContextGuard;

	const FAngelscriptEngineConfig Config;
	const FAngelscriptEngineDependencies Deps = FAngelscriptEngineDependencies::CreateDefault();
	TUniquePtr<FAngelscriptEngine> EngineA = FAngelscriptTestEngine::Create(Config, Deps);
	TUniquePtr<FAngelscriptEngine> EngineB = FAngelscriptTestEngine::Create(Config, Deps);
	if (!Test.TestNotNull(TEXT("Namespace isolation should create engine A"), EngineA.Get())
		|| !Test.TestNotNull(TEXT("Namespace isolation should create engine B"), EngineB.Get()))
	{
		return false;
	}

	UFunction* Function = USubsystemLibrary::StaticClass()->FindFunctionByName(TEXT("GetEngineSubsystem"));
	if (!Test.TestNotNull(TEXT("Blueprint namespace rule should find GetEngineSubsystem"), Function))
	{
		return false;
	}

	{
		FAngelscriptEngineScope ScopeA(*EngineA);
		TSharedPtr<FAngelscriptType> HostType = FAngelscriptType::GetByClass(USubsystemLibrary::StaticClass());
		if (!Test.TestTrue(TEXT("Engine A should resolve the subsystem library host type"), HostType.IsValid()))
		{
			return false;
		}

		FAngelscriptFunctionSignature Signature(HostType.ToSharedRef(), Function);
		Test.TestEqual(TEXT("Engine A should use the full registered AS type namespace"), Signature.ClassName, FString(TEXT("USubsystemLibrary")));
		Test.TestTrue(TEXT("Engine A should bind the helper as a static script function"), Signature.bStaticInScript);
	}

	{
		FAngelscriptEngineScope ScopeB(*EngineB);
		TSharedPtr<FAngelscriptType> HostType = FAngelscriptType::GetByClass(USubsystemLibrary::StaticClass());
		if (!Test.TestTrue(TEXT("Engine B should resolve the subsystem library host type"), HostType.IsValid()))
		{
			return false;
		}

		FAngelscriptFunctionSignature Signature(HostType.ToSharedRef(), Function);
		Test.TestEqual(TEXT("Engine B should use the same full registered AS type namespace"), Signature.ClassName, FString(TEXT("USubsystemLibrary")));
		Test.TestTrue(TEXT("Engine B should bind the helper as a static script function"), Signature.bStaticInScript);
	}

	return true;
}

bool RunEngineLocalStaticNamesIsolation(FAutomationTestBase& Test)
{
	FIsolationContextStackGuard ContextGuard;

	const FAngelscriptEngineConfig Config;
	const FAngelscriptEngineDependencies Deps = FAngelscriptEngineDependencies::CreateDefault();
	TUniquePtr<FAngelscriptEngine> EngineA = FAngelscriptTestEngine::Create(Config, Deps);
	TUniquePtr<FAngelscriptEngine> EngineB = FAngelscriptTestEngine::Create(Config, Deps);
	if (!Test.TestNotNull(TEXT("Static-name isolation should create engine A"), EngineA.Get())
		|| !Test.TestNotNull(TEXT("Static-name isolation should create engine B"), EngineB.Get()))
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
		Test.TestEqual(TEXT("Engine A should append its own static name"), FAngelscriptEngine::GetStaticNameCount(), EngineABaselineCount + 1);

		FName ResolvedName;
		Test.TestTrue(TEXT("Engine A should resolve its static name by index"), FAngelscriptEngine::TryGetStaticName(EngineANameIndex, ResolvedName));
		Test.TestEqual(TEXT("Engine A static-name index should resolve to the added name"), ResolvedName.ToString(), EngineAName.ToString());
	}

	{
		FAngelscriptEngineScope ScopeB(*EngineB);
		const int32 EngineBBaselineCount = FAngelscriptEngine::GetStaticNameCount();
		FName ResolvedName;
		const bool bEngineBSeesEngineAName = FAngelscriptEngine::TryGetStaticName(EngineANameIndex, ResolvedName) && ResolvedName == EngineAName;
		Test.TestFalse(TEXT("Engine B should not see static names added through engine A"), bEngineBSeesEngineAName);
		Test.TestEqual(TEXT("Engine B static-name count should stay isolated"), FAngelscriptEngine::GetStaticNameCount(), EngineBBaselineCount);
	}

	// The Clone-shares-source-state portion of this test was removed when
	// the Clone mechanism was deleted. Two independent Full engines have
	// independent static-name registries by construction; the EngineA / EngineB
	// assertions above already cover that contract.

	return true;
}

}

TEST_CLASS_WITH_FLAGS(FAngelscriptEngineIsolationTests,
	"Angelscript.TestModule.Engine.Isolation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(ContextStackScopedResolution)
	{
		using namespace AngelscriptTest_Core_AngelscriptEngineIsolationTests_Private;
		RunContextStackScopedResolution(*TestRunner);
	}

	TEST_METHOD(EngineScopeRestoresWorldContext)
	{
		using namespace AngelscriptTest_Core_AngelscriptEngineIsolationTests_Private;
		RunEngineScopeRestoresWorldContext(*TestRunner);
	}

	TEST_METHOD(FullEnginesKeepStateSeparate)
	{
		using namespace AngelscriptTest_Core_AngelscriptEngineIsolationTests_Private;
		RunFullEnginesKeepStateSeparate(*TestRunner);
	}

	TEST_METHOD(CloneSharesSourceState)
	{
		using namespace AngelscriptTest_Core_AngelscriptEngineIsolationTests_Private;
		RunCloneSharesSourceState(*TestRunner);
	}

	TEST_METHOD(RequestContextUsesRequestedEngine)
	{
		using namespace AngelscriptTest_Core_AngelscriptEngineIsolationTests_Private;
		RunRequestContextUsesRequestedEngine(*TestRunner);
	}

	TEST_METHOD(RequestContextReusedStartsUnprepared)
	{
		using namespace AngelscriptTest_Core_AngelscriptEngineIsolationTests_Private;
		RunRequestContextReusedStartsUnprepared(*TestRunner);
	}

	TEST_METHOD(RequestContextAfterReturningUnpreparedScopedContext)
	{
		using namespace AngelscriptTest_Core_AngelscriptEngineIsolationTests_Private;
		RunRequestContextAfterReturningUnpreparedScopedContext(*TestRunner);
	}

	TEST_METHOD(FullEngineCreateClearsThreadLocalPool)
	{
		using namespace AngelscriptTest_Core_AngelscriptEngineIsolationTests_Private;
		RunFullEngineCreateClearsThreadLocalPool(*TestRunner);
	}

	TEST_METHOD(ContextPoolResetSequenceKeepsRequestedContextReusable)
	{
		using namespace AngelscriptTest_Core_AngelscriptEngineIsolationTests_Private;
		RunContextPoolResetSequenceKeepsRequestedContextReusable(*TestRunner);
	}

	TEST_METHOD(ScopedPooledContextUsesScopedEngine)
	{
		using namespace AngelscriptTest_Core_AngelscriptEngineIsolationTests_Private;
		RunScopedPooledContextUsesScopedEngine(*TestRunner);
	}

	TEST_METHOD(ReusedPooledContextStartsUnprepared)
	{
		using namespace AngelscriptTest_Core_AngelscriptEngineIsolationTests_Private;
		RunReusedPooledContextStartsUnprepared(*TestRunner);
	}

	TEST_METHOD(EngineLocalFlagsIsolation)
	{
		using namespace AngelscriptTest_Core_AngelscriptEngineIsolationTests_Private;
		RunEngineLocalFlagsIsolation(*TestRunner);
	}

	TEST_METHOD(EngineLocalBlueprintLibraryNamespaceRuleConsistency)
	{
		using namespace AngelscriptTest_Core_AngelscriptEngineIsolationTests_Private;
		RunEngineLocalBlueprintLibraryNamespaceRuleConsistency(*TestRunner);
	}

	TEST_METHOD(EngineLocalStaticNamesIsolation)
	{
		using namespace AngelscriptTest_Core_AngelscriptEngineIsolationTests_Private;
		RunEngineLocalStaticNamesIsolation(*TestRunner);
	}

};

#endif
