#include "AngelscriptEngine.h"
#include "Shared/AngelscriptTestUtilities.h"
#include "Shared/AngelscriptTestUtilities.h"

#include "CQTest.h"
#include "Misc/ScopeExit.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_context.h"
#include "EndAngelscriptHeaders.h"

#if WITH_DEV_AUTOMATION_TESTS


namespace AngelscriptTest_Core_AngelscriptEngineExecutionGuardTests_Private
{
	struct FEngineExecutionGuardContextStackGuard
	{
		TArray<FAngelscriptEngine*> SavedStack;

		FEngineExecutionGuardContextStackGuard()
		{
			SavedStack = FAngelscriptEngineContextStack::SnapshotAndClear();
		}

		~FEngineExecutionGuardContextStackGuard()
		{
			FAngelscriptEngineContextStack::RestoreSnapshot(MoveTemp(SavedStack));
		}

		void DiscardSavedStack()
		{
			SavedStack.Reset();
		}
	};

	template<typename TObjectType>
	struct TScopedAsRelease
	{
		TObjectType* Object = nullptr;

		explicit TScopedAsRelease(TObjectType* InObject)
			: Object(InObject)
		{
		}

		~TScopedAsRelease()
		{
			if (Object != nullptr)
			{
				Object->Release();
			}
		}
	};
}


TEST_CLASS_WITH_FLAGS(FAngelscriptEngineExecutionGuardTests,
	"Angelscript.TestModule.Engine.Context",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(PrepareContextLogsCrossEngineMismatch)
	{
		using namespace AngelscriptTest_Core_AngelscriptEngineExecutionGuardTests_Private;
		FEngineExecutionGuardContextStackGuard ContextGuard;
		DestroySharedTestEngine();
		if (FAngelscriptEngine::IsInitialized())
		{
			FAngelscriptTestEngineScopeAccess::DestroyGlobalEngine();
		}
		ContextGuard.DiscardSavedStack();

		ON_SCOPE_EXIT
		{
			FAngelscriptEngineContextStack::SnapshotAndClear();
			if (FAngelscriptEngine::IsInitialized())
			{
				FAngelscriptTestEngineScopeAccess::DestroyGlobalEngine();
			}
			DestroySharedTestEngine();
		};

		TUniquePtr<FAngelscriptEngine> EngineA = CreateFullTestEngine();
		TUniquePtr<FAngelscriptEngine> EngineB = CreateFullTestEngine();
		if (!TestRunner->TestNotNull(TEXT("PrepareContext cross-engine mismatch test should create the source full engine"), EngineA.Get())
			|| !TestRunner->TestNotNull(TEXT("PrepareContext cross-engine mismatch test should create the target full engine"), EngineB.Get()))
		{
			return;
		}

		asIScriptModule* ModuleA = BuildModule(
			*TestRunner,
			*EngineA,
			"ASPrepareContextMismatchSource",
			TEXT("int Entry() { return 1; }"));
		if (!TestRunner->TestNotNull(TEXT("PrepareContext cross-engine mismatch test should compile the source module"), ModuleA))
		{
			return;
		}

		asIScriptFunction* EntryA = GetFunctionByDecl(*TestRunner, *ModuleA, TEXT("int Entry()"));
		if (!TestRunner->TestNotNull(TEXT("PrepareContext cross-engine mismatch test should resolve the source Entry() function"), EntryA))
		{
			return;
		}

		asIScriptContext* ContextB = EngineB->CreateContext();
		if (!TestRunner->TestNotNull(TEXT("PrepareContext cross-engine mismatch test should create a target-engine context"), ContextB))
		{
			return;
		}

		TScopedAsRelease<asIScriptContext> ContextBScope(ContextB);

		TestRunner->AddExpectedErrorPlain(
			TEXT("Failed in call to function 'Prepare' with 'int Entry()' (Code: asINVALID_ARG"),
			EAutomationExpectedErrorFlags::Contains,
			1);
		TestRunner->AddExpectedErrorPlain(
			TEXT("Failed to prepare Angelscript context for 'Automation.PrepareMismatch'"),
			EAutomationExpectedErrorFlags::Contains,
			1);

		bool bMismatchPrepared = false;
		{
			FAngelscriptEngineScope PrepareScope(*EngineB);
			bMismatchPrepared = PrepareAngelscriptContextWithLog(
				ContextB,
				EntryA,
				TEXT("Automation.PrepareMismatch"));
		}
		if (!TestRunner->TestFalse(
				TEXT("PrepareContext cross-engine mismatch test should fail closed when a context prepares a function from another engine"),
				bMismatchPrepared))
		{
			return;
		}

		const asEContextState MismatchState = ContextB->GetState();
		if (!TestRunner->TestTrue(
				TEXT("PrepareContext cross-engine mismatch test should not leave the mismatched context active or suspended"),
				MismatchState != asEXECUTION_ACTIVE && MismatchState != asEXECUTION_SUSPENDED))
		{
			return;
		}

		if (!TestRunner->TestNull(
				TEXT("PrepareContext cross-engine mismatch test should not leak a current engine after the mismatch path"),
				FAngelscriptEngine::TryGetCurrentEngine()))
		{
			return;
		}

		asIScriptModule* ModuleB = BuildModule(
			*TestRunner,
			*EngineB,
			"ASPrepareContextMismatchControl",
			TEXT("int Entry() { return 2; }"));
		if (!TestRunner->TestNotNull(TEXT("PrepareContext cross-engine mismatch test should compile the control module on the target engine"), ModuleB))
		{
			return;
		}

		asIScriptFunction* EntryB = GetFunctionByDecl(*TestRunner, *ModuleB, TEXT("int Entry()"));
		if (!TestRunner->TestNotNull(TEXT("PrepareContext cross-engine mismatch test should resolve the control Entry() function"), EntryB))
		{
			return;
		}

		asIScriptContext* ContextB2 = EngineB->CreateContext();
		if (!TestRunner->TestNotNull(TEXT("PrepareContext cross-engine mismatch test should create a fresh control context"), ContextB2))
		{
			return;
		}

		TScopedAsRelease<asIScriptContext> ContextB2Scope(ContextB2);

		bool bControlPrepared = false;
		int32 ExecuteResult = asERROR;
		{
			FAngelscriptEngineScope PrepareAndExecuteScope(*EngineB);
			bControlPrepared = PrepareAngelscriptContextWithLog(
				ContextB2,
				EntryB,
				TEXT("Automation.PrepareControl"));
			if (bControlPrepared)
			{
				ExecuteResult = ContextB2->Execute();
			}
		}
		if (!TestRunner->TestTrue(
				TEXT("PrepareContext cross-engine mismatch test should still prepare a same-engine control function after the mismatch"),
				bControlPrepared))
		{
			return;
		}

		if (!TestRunner->TestEqual(
				TEXT("PrepareContext cross-engine mismatch test should execute the control function successfully after the mismatch"),
				ExecuteResult,
				static_cast<int32>(asEXECUTION_FINISHED)))
		{
			return;
		}

		TestRunner->TestEqual(
			TEXT("PrepareContext cross-engine mismatch test should preserve a working control context return value after the mismatch"),
			static_cast<int32>(ContextB2->GetReturnDWord()),
			2);
	}
};

#endif
