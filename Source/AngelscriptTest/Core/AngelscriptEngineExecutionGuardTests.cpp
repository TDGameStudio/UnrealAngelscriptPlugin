#include "AngelscriptEngine.h"
#include "AngelscriptTestUtilities.h"
#include "AngelscriptTestUtilities.h"

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
		if (!this->Assert.IsNotNull(EngineA.Get(), TEXT("PrepareContext cross-engine mismatch test should create the source full engine"))
			|| !this->Assert.IsNotNull(EngineB.Get(), TEXT("PrepareContext cross-engine mismatch test should create the target full engine")))
		{
			return;
		}

		asIScriptModule* ModuleA = BuildModule(
			*TestRunner,
			*EngineA,
			"ASPrepareContextMismatchSource",
			TEXT("int Entry() { return 1; }"));
		if (!this->Assert.IsNotNull(ModuleA, TEXT("PrepareContext cross-engine mismatch test should compile the source module")))
		{
			return;
		}

		asIScriptFunction* EntryA = GetFunctionByDecl(*TestRunner, *ModuleA, TEXT("int Entry()"));
		if (!this->Assert.IsNotNull(EntryA, TEXT("PrepareContext cross-engine mismatch test should resolve the source Entry() function")))
		{
			return;
		}

		asIScriptContext* ContextB = EngineB->CreateContext();
		if (!this->Assert.IsNotNull(ContextB, TEXT("PrepareContext cross-engine mismatch test should create a target-engine context")))
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
		if (!this->Assert.IsFalse(
				bMismatchPrepared,
				TEXT("PrepareContext cross-engine mismatch test should fail closed when a context prepares a function from another engine")))
		{
			return;
		}

		const asEContextState MismatchState = ContextB->GetState();
		if (!this->Assert.IsTrue(
				MismatchState != asEXECUTION_ACTIVE && MismatchState != asEXECUTION_SUSPENDED,
				TEXT("PrepareContext cross-engine mismatch test should not leave the mismatched context active or suspended")))
		{
			return;
		}

		if (!this->Assert.IsNull(
				FAngelscriptEngine::TryGetCurrentEngine(),
				TEXT("PrepareContext cross-engine mismatch test should not leak a current engine after the mismatch path")))
		{
			return;
		}

		asIScriptModule* ModuleB = BuildModule(
			*TestRunner,
			*EngineB,
			"ASPrepareContextMismatchControl",
			TEXT("int Entry() { return 2; }"));
		if (!this->Assert.IsNotNull(ModuleB, TEXT("PrepareContext cross-engine mismatch test should compile the control module on the target engine")))
		{
			return;
		}

		asIScriptFunction* EntryB = GetFunctionByDecl(*TestRunner, *ModuleB, TEXT("int Entry()"));
		if (!this->Assert.IsNotNull(EntryB, TEXT("PrepareContext cross-engine mismatch test should resolve the control Entry() function")))
		{
			return;
		}

		asIScriptContext* ContextB2 = EngineB->CreateContext();
		if (!this->Assert.IsNotNull(ContextB2, TEXT("PrepareContext cross-engine mismatch test should create a fresh control context")))
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
		if (!this->Assert.IsTrue(
				bControlPrepared,
				TEXT("PrepareContext cross-engine mismatch test should still prepare a same-engine control function after the mismatch")))
		{
			return;
		}

		if (!this->Assert.AreEqual(
				static_cast<int32>(asEXECUTION_FINISHED),
				ExecuteResult,
				TEXT("PrepareContext cross-engine mismatch test should execute the control function successfully after the mismatch")))
		{
			return;
		}

		ASSERT_THAT(AreEqual(
			2,
			static_cast<int32>(ContextB2->GetReturnDWord()),
			TEXT("PrepareContext cross-engine mismatch test should preserve a working control context return value after the mismatch")));
	}
};

#endif
