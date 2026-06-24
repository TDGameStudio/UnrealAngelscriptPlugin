#include "AngelscriptTestUtilities.h"

#include "CQTest.h"
#include "Misc/Guid.h"
#include "Misc/ScopeExit.h"

#if WITH_DEV_AUTOMATION_TESTS


TEST_CLASS_WITH_FLAGS(FAngelscriptContextPoolTests,
	"Angelscript.TestModule.AngelScriptSDK.ContextPool",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	struct FContextPoolEngineStackGuard
	{
		TArray<FAngelscriptEngine*> SavedStack;

		FContextPoolEngineStackGuard()
		{
			SavedStack = FAngelscriptEngineContextStack::SnapshotAndClear();
		}

		~FContextPoolEngineStackGuard()
		{
			FAngelscriptEngineContextStack::RestoreSnapshot(MoveTemp(SavedStack));
		}
	};

	static FString MakeContextPoolModuleName(const TCHAR* Prefix)
	{
		return FString::Printf(TEXT("%s_%s"), Prefix, *FGuid::NewGuid().ToString(EGuidFormats::Digits));
	}

	static asIScriptFunction* CompileContextPoolFunction(
		FAutomationTestBase& Test,
		FAngelscriptEngine& Engine,
		const FString& ModuleName,
		const ANSICHAR* Source,
		const ANSICHAR* Declaration)
	{
		FAngelscriptEngineScope EngineScope(Engine);
		FNoDiscardAsserter LocalAssert(Test);

		asIScriptModule* Module = Engine.GetScriptEngine()->GetModule(TCHAR_TO_ANSI(*ModuleName), asGM_ALWAYS_CREATE);
		if (!LocalAssert.IsNotNull(
				Module,
				*FString::Printf(TEXT("Context pool helper should create module '%s'"), *ModuleName)))
		{
			return nullptr;
		}

		asIScriptFunction* Function = nullptr;
		const int32 CompileResult = Module->CompileFunction(TCHAR_TO_ANSI(*ModuleName), Source, 0, 0, &Function);
		if (!LocalAssert.AreEqual(
				asSUCCESS,
				CompileResult,
				*FString::Printf(TEXT("Context pool helper should compile '%s'"), *ModuleName)))
		{
			return nullptr;
		}

		(void)LocalAssert.IsNotNull(
			Function,
			*FString::Printf(TEXT("Context pool helper should resolve '%s'"), ANSI_TO_TCHAR(Declaration)));
		return Function;
	}

	static int32 GetLocalPooledContextCount(asIScriptEngine* ScriptEngine)
	{
		return FAngelscriptEngine::GetLocalPooledContextCountForTesting(ScriptEngine);
	}

public:
	TEST_METHOD(ReuseAndResetPerEngine)
	{
		FContextPoolEngineStackGuard ContextStackGuard;

		const FAngelscriptEngineConfig Config;
		const FAngelscriptEngineDependencies Dependencies = FAngelscriptEngineDependencies::CreateDefault();
		TUniquePtr<FAngelscriptEngine> EngineA = CreateScriptScanFreeEngineForTesting(
			Config,
			Dependencies);
		TUniquePtr<FAngelscriptEngine> EngineB = CreateScriptScanFreeEngineForTesting(
			Config,
			Dependencies);

		ASSERT_THAT(IsNotNull(EngineA.Get(), TEXT("ContextPool.ReuseAndResetPerEngine should create EngineA")));
		ASSERT_THAT(IsNotNull(EngineB.Get(), TEXT("ContextPool.ReuseAndResetPerEngine should create EngineB")));

		asIScriptFunction* RunFunction = CompileContextPoolFunction(
			*TestRunner,
			*EngineA,
			MakeContextPoolModuleName(TEXT("ContextPoolReuseAndResetPerEngine")),
			"void Run() {}",
			"void Run()");
		ASSERT_THAT(IsNotNull(RunFunction, TEXT("ContextPool.ReuseAndResetPerEngine should compile the EngineA helper function")));

		ON_SCOPE_EXIT
		{
			RunFunction->Release();
		};

		const int32 EngineABaselineCount = GetLocalPooledContextCount(EngineA->GetScriptEngine());
		ASSERT_THAT(AreEqual(0, EngineABaselineCount, TEXT("ContextPool.ReuseAndResetPerEngine should start EngineA with an empty local pool")));

		asIScriptContext* SeedContext = nullptr;
		{
			FAngelscriptEngineScope EngineScope(*EngineA);
			SeedContext = EngineA->GetScriptEngine()->RequestContext();
			ASSERT_THAT(IsNotNull(SeedContext, TEXT("ContextPool.ReuseAndResetPerEngine should request a seed context from EngineA")));

			ASSERT_THAT(AreEqual(0, GetLocalPooledContextCount(EngineA->GetScriptEngine()), TEXT("ContextPool.ReuseAndResetPerEngine should keep EngineA pool empty while the seed context is checked out")));

			const int32 PrepareResult = SeedContext->Prepare(RunFunction);
			ASSERT_THAT(AreEqual(asSUCCESS, PrepareResult, TEXT("ContextPool.ReuseAndResetPerEngine should prepare the EngineA seed context successfully")));

			const int32 ExecuteResult = PrepareResult == asSUCCESS ? SeedContext->Execute() : PrepareResult;
			ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, ExecuteResult, TEXT("ContextPool.ReuseAndResetPerEngine should execute the EngineA seed context successfully")));

			EngineA->GetScriptEngine()->ReturnContext(SeedContext);
		}

		ASSERT_THAT(AreEqual(1, GetLocalPooledContextCount(EngineA->GetScriptEngine()), TEXT("ContextPool.ReuseAndResetPerEngine should pool the returned EngineA context")));

		asIScriptContext* ReusedContext = nullptr;
		{
			FAngelscriptEngineScope EngineScope(*EngineA);
			ReusedContext = EngineA->GetScriptEngine()->RequestContext();
			ASSERT_THAT(IsNotNull(ReusedContext, TEXT("ContextPool.ReuseAndResetPerEngine should request a reused context from EngineA")));

			ASSERT_THAT(IsTrue(ReusedContext == SeedContext, TEXT("ContextPool.ReuseAndResetPerEngine should reuse the same pooled EngineA context")));
			ASSERT_THAT(AreEqual(0, GetLocalPooledContextCount(EngineA->GetScriptEngine()), TEXT("ContextPool.ReuseAndResetPerEngine should pop EngineA pool count back to zero when re-borrowing")));
			ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_UNINITIALIZED), static_cast<int32>(ReusedContext->GetState()), TEXT("ContextPool.ReuseAndResetPerEngine should reset the reused EngineA context to the uninitialized state")));

			EngineA->GetScriptEngine()->ReturnContext(ReusedContext);
		}

		ASSERT_THAT(AreEqual(1, GetLocalPooledContextCount(EngineA->GetScriptEngine()), TEXT("ContextPool.ReuseAndResetPerEngine should restore EngineA pooled count after returning the reused context")));
		ASSERT_THAT(AreEqual(0, GetLocalPooledContextCount(EngineB->GetScriptEngine()), TEXT("ContextPool.ReuseAndResetPerEngine should keep EngineB baseline pool count at zero before it borrows a context")));

		{
			FAngelscriptEngineScope EngineScope(*EngineB);
			asIScriptContext* EngineBContext = EngineB->GetScriptEngine()->RequestContext();
			ASSERT_THAT(IsNotNull(EngineBContext, TEXT("ContextPool.ReuseAndResetPerEngine should request a context from EngineB")));

			ASSERT_THAT(IsTrue(EngineBContext != SeedContext, TEXT("ContextPool.ReuseAndResetPerEngine should never hand EngineB the pooled EngineA context")));

			EngineB->GetScriptEngine()->ReturnContext(EngineBContext);
		}

		ASSERT_THAT(AreEqual(1, GetLocalPooledContextCount(EngineA->GetScriptEngine()), TEXT("ContextPool.ReuseAndResetPerEngine should keep EngineA pooled count unchanged after EngineB returns its context")));
		ASSERT_THAT(AreEqual(1, GetLocalPooledContextCount(EngineB->GetScriptEngine()), TEXT("ContextPool.ReuseAndResetPerEngine should track EngineB pooled count independently after its own return")));
	}
};

#endif
