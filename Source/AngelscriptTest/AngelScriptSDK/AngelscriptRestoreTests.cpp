#include "Shared/AngelscriptTestUtilities.h"
#include "CQTest.h"
#include "Misc/ScopeExit.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_context.h"
#include "source/as_module.h"
#include "source/as_restore.h"
#include "source/as_scriptengine.h"
#include "EndAngelscriptHeaders.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace AngelscriptTest_AngelScriptSDK_AngelscriptRestoreTests_Private
{
	class FMemoryBinaryStream final : public asIBinaryStream
	{
	public:
		int Write(const void* Ptr, asUINT Size) override
		{
			if (Ptr == nullptr)
			{
				return asINVALID_ARG;
			}

			const int32 StartIndex = Bytes.Num();
			Bytes.AddUninitialized(static_cast<int32>(Size));
			FMemory::Memcpy(Bytes.GetData() + StartIndex, Ptr, static_cast<SIZE_T>(Size));
			return asSUCCESS;
		}

		int Read(void* Ptr, asUINT Size) override
		{
			if (Ptr == nullptr)
			{
				return asINVALID_ARG;
			}

			const int32 RemainingBytes = Bytes.Num() - ReadOffset;
			if (RemainingBytes < static_cast<int32>(Size))
			{
				return asERROR;
			}

			FMemory::Memcpy(Ptr, Bytes.GetData() + ReadOffset, static_cast<SIZE_T>(Size));
			ReadOffset += static_cast<int32>(Size);
			return asSUCCESS;
		}

		void ResetReadOffset()
		{
			ReadOffset = 0;
		}

		void Truncate(int32 NewSize)
		{
			Bytes.SetNum(FMath::Max(NewSize, 0), EAllowShrinking::No);
			ReadOffset = FMath::Min(ReadOffset, Bytes.Num());
		}

		int32 Num() const
		{
			return Bytes.Num();
		}

	private:
		TArray<uint8> Bytes;
		int32 ReadOffset = 0;
	};

	asCModule* CreateRestoreModule(asCScriptEngine* ScriptEngine, const char* ModuleName)
	{
		return static_cast<asCModule*>(ScriptEngine->GetModule(ModuleName, asGM_ALWAYS_CREATE));
	}

	asCModule* BuildRestoreModule(FAutomationTestBase& Test, FAngelscriptEngine& Engine, const char* ModuleName)
	{
		asIScriptModule* Module = BuildModule(
			Test,
			Engine,
			ModuleName,
			TEXT("const int GlobalValue = 41; int Test() { return GlobalValue + 1; }"));
		return static_cast<asCModule*>(Module);
	}

	bool ExecuteRestoreFunction(FAutomationTestBase& Test, FAngelscriptEngine& Engine, asCModule& Module, int32& OutValue)
	{
		asIScriptFunction* Function = GetFunctionByDecl(Test, Module, TEXT("int Test()"));
		if (Function == nullptr)
		{
			return false;
		}

		return ExecuteIntFunction(Test, Engine, *Function, OutValue);
	}
}


TEST_CLASS_WITH_FLAGS(FAngelscriptRestoreTests,
	"Angelscript.TestModule.AngelScriptSDK.Restore",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(RoundTrip)
	{
		using namespace AngelscriptTest_AngelScriptSDK_AngelscriptRestoreTests_Private;
		TUniquePtr<FAngelscriptEngine> SourceEngineOwner = CreateIsolatedCloneEngine();
		if (!TestRunner->TestNotNull(TEXT("Restore roundtrip should create an isolated clone test engine"), SourceEngineOwner.Get()))
		{
			return;
		}
		FAngelscriptEngineScope EngineScope(*SourceEngineOwner);

		FAngelscriptEngine& SourceEngine = *SourceEngineOwner;
		ON_SCOPE_EXIT
		{
			SourceEngine.DiscardModule(TEXT("RestoreSourceModule"));
		};

		asCModule* SourceModule = BuildRestoreModule(*TestRunner, SourceEngine, "RestoreSourceModule");
		if (!TestRunner->TestNotNull(TEXT("Restore roundtrip should compile a source module"), SourceModule))
		{
			return;
		}

		int32 SourceValue = 0;
		if (!ExecuteRestoreFunction(*TestRunner, SourceEngine, *SourceModule, SourceValue))
		{
			return;
		}
		if (!TestRunner->TestEqual(TEXT("Restore roundtrip should execute before serialization"), SourceValue, 42))
		{
			return;
		}

		FMemoryBinaryStream Stream;
		const int SaveResult = SourceModule->SaveByteCode(&Stream, false);
		if (!TestRunner->TestEqual(TEXT("Restore roundtrip should save bytecode successfully"), SaveResult, static_cast<int>(asSUCCESS)))
		{
			return;
		}
		if (!TestRunner->TestTrue(TEXT("Restore roundtrip should emit bytecode bytes"), Stream.Num() > 0))
		{
			return;
		}

		Stream.ResetReadOffset();
		SourceModule->Discard();
		bool bWasDebugInfoStripped = true;
		asCScriptEngine* ScriptEngine = static_cast<asCScriptEngine*>(SourceEngine.GetScriptEngine());
		const asPWORD PreviousInitGlobalsAfterBuild = ScriptEngine->GetEngineProperty(asEP_INIT_GLOBAL_VARS_AFTER_BUILD);
		ScriptEngine->SetEngineProperty(asEP_INIT_GLOBAL_VARS_AFTER_BUILD, 0);
		ON_SCOPE_EXIT
		{
			ScriptEngine->SetEngineProperty(asEP_INIT_GLOBAL_VARS_AFTER_BUILD, PreviousInitGlobalsAfterBuild);
		};
		asCModule* RestoredModule = CreateRestoreModule(ScriptEngine, "RestoreSourceModule");
		if (!TestRunner->TestNotNull(TEXT("Restore roundtrip should create a destination module"), RestoredModule))
		{
			return;
		}

		const int LoadResult = RestoredModule->LoadByteCode(&Stream, &bWasDebugInfoStripped);
		if (!TestRunner->TestEqual(TEXT("Restore roundtrip should load bytecode successfully"), LoadResult, static_cast<int>(asSUCCESS)))
		{
			return;
		}
		if (!TestRunner->TestFalse(TEXT("Restore roundtrip should preserve debug info when not stripping"), bWasDebugInfoStripped))
		{
			return;
		}
	}

	TEST_METHOD(StripDebugInfoRoundTrip)
	{
		using namespace AngelscriptTest_AngelScriptSDK_AngelscriptRestoreTests_Private;
		TUniquePtr<FAngelscriptEngine> SourceEngineOwner = CreateIsolatedCloneEngine();
		if (!TestRunner->TestNotNull(TEXT("Restore strip roundtrip should create an isolated clone test engine"), SourceEngineOwner.Get()))
		{
			return;
		}
		FAngelscriptEngineScope EngineScope(*SourceEngineOwner);

		FAngelscriptEngine& SourceEngine = *SourceEngineOwner;
		ON_SCOPE_EXIT
		{
			SourceEngine.DiscardModule(TEXT("RestoreStripSourceModule"));
		};

		asCModule* SourceModule = BuildRestoreModule(*TestRunner, SourceEngine, "RestoreStripSourceModule");
		if (!TestRunner->TestNotNull(TEXT("Restore strip roundtrip should compile a source module"), SourceModule))
		{
			return;
		}

		FMemoryBinaryStream Stream;
		const int SaveResult = SourceModule->SaveByteCode(&Stream, true);
		if (!TestRunner->TestEqual(TEXT("Restore strip roundtrip should save bytecode successfully"), SaveResult, static_cast<int>(asSUCCESS)))
		{
			return;
		}

		Stream.ResetReadOffset();
		SourceModule->Discard();
		bool bWasDebugInfoStripped = false;
		asCScriptEngine* ScriptEngine = static_cast<asCScriptEngine*>(SourceEngine.GetScriptEngine());
		const asPWORD PreviousInitGlobalsAfterBuild = ScriptEngine->GetEngineProperty(asEP_INIT_GLOBAL_VARS_AFTER_BUILD);
		ScriptEngine->SetEngineProperty(asEP_INIT_GLOBAL_VARS_AFTER_BUILD, 0);
		ON_SCOPE_EXIT
		{
			ScriptEngine->SetEngineProperty(asEP_INIT_GLOBAL_VARS_AFTER_BUILD, PreviousInitGlobalsAfterBuild);
		};
		asCModule* RestoredModule = CreateRestoreModule(ScriptEngine, "RestoreStripSourceModule");
		if (!TestRunner->TestNotNull(TEXT("Restore strip roundtrip should create a destination module"), RestoredModule))
		{
			return;
		}

		const int LoadResult = RestoredModule->LoadByteCode(&Stream, &bWasDebugInfoStripped);
		if (!TestRunner->TestEqual(TEXT("Restore strip roundtrip should load bytecode successfully"), LoadResult, static_cast<int>(asSUCCESS)))
		{
			return;
		}
		if (!TestRunner->TestTrue(TEXT("Restore strip roundtrip should report stripped debug info"), bWasDebugInfoStripped))
		{
			return;
		}
	}

	TEST_METHOD(EmptyStreamFails)
	{
		using namespace AngelscriptTest_AngelScriptSDK_AngelscriptRestoreTests_Private;
		TUniquePtr<FAngelscriptEngine> EngineOwner = CreateIsolatedCloneEngine();
		if (!TestRunner->TestNotNull(TEXT("Restore empty stream test should create an isolated clone test engine"), EngineOwner.Get()))
		{
			return;
		}
		FAngelscriptEngineScope EngineScope(*EngineOwner);

		asCScriptEngine* ScriptEngine = static_cast<asCScriptEngine*>(EngineOwner->GetScriptEngine());
		asCModule* RestoredModule = CreateRestoreModule(ScriptEngine, "RestoreEmptyStream");
		if (!TestRunner->TestNotNull(TEXT("Restore empty stream test should create a destination module"), RestoredModule))
		{
			return;
		}

		FMemoryBinaryStream Stream;
		bool bWasDebugInfoStripped = false;
		TestRunner->AddExpectedErrorPlain(TEXT("Unexpected end of file"), EAutomationExpectedErrorFlags::Contains, -1);
		const int LoadResult = RestoredModule->LoadByteCode(&Stream, &bWasDebugInfoStripped);
		TestRunner->TestNotEqual(TEXT("Restore should reject an empty bytecode stream"), LoadResult, static_cast<int>(asSUCCESS));
	}

	TEST_METHOD(TruncatedStreamFails)
	{
		using namespace AngelscriptTest_AngelScriptSDK_AngelscriptRestoreTests_Private;
		TUniquePtr<FAngelscriptEngine> SourceEngineOwner = CreateIsolatedCloneEngine();
		if (!TestRunner->TestNotNull(TEXT("Restore truncated stream test should create an isolated clone test engine"), SourceEngineOwner.Get()))
		{
			return;
		}
		FAngelscriptEngineScope EngineScope(*SourceEngineOwner);

		FAngelscriptEngine& SourceEngine = *SourceEngineOwner;
		ON_SCOPE_EXIT
		{
			SourceEngine.DiscardModule(TEXT("RestoreTruncatedSourceModule"));
		};

		asCModule* SourceModule = BuildRestoreModule(*TestRunner, SourceEngine, "RestoreTruncatedSourceModule");
		if (!TestRunner->TestNotNull(TEXT("Restore truncated stream test should compile a source module"), SourceModule))
		{
			return;
		}

		FMemoryBinaryStream Stream;
		const int SaveResult = SourceModule->SaveByteCode(&Stream, false);
		if (!TestRunner->TestEqual(TEXT("Restore truncated stream test should save bytecode successfully"), SaveResult, static_cast<int>(asSUCCESS)))
		{
			return;
		}
		if (!TestRunner->TestTrue(TEXT("Restore truncated stream test should emit enough bytes to truncate"), Stream.Num() > 16))
		{
			return;
		}

		Stream.Truncate(Stream.Num() - 16);
		Stream.ResetReadOffset();
		SourceModule->Discard();

		asCScriptEngine* ScriptEngine = static_cast<asCScriptEngine*>(SourceEngine.GetScriptEngine());
		const asPWORD PreviousInitGlobalsAfterBuild = ScriptEngine->GetEngineProperty(asEP_INIT_GLOBAL_VARS_AFTER_BUILD);
		ScriptEngine->SetEngineProperty(asEP_INIT_GLOBAL_VARS_AFTER_BUILD, 0);
		ON_SCOPE_EXIT
		{
			ScriptEngine->SetEngineProperty(asEP_INIT_GLOBAL_VARS_AFTER_BUILD, PreviousInitGlobalsAfterBuild);
		};

		asCModule* RestoredModule = CreateRestoreModule(ScriptEngine, "RestoreTruncatedSourceModule");
		if (!TestRunner->TestNotNull(TEXT("Restore truncated stream test should create a destination module"), RestoredModule))
		{
			return;
		}

		bool bWasDebugInfoStripped = false;
		TestRunner->AddExpectedErrorPlain(TEXT("Unexpected end of file"), EAutomationExpectedErrorFlags::Contains, -1);
		const int LoadResult = RestoredModule->LoadByteCode(&Stream, &bWasDebugInfoStripped);
		TestRunner->TestNotEqual(TEXT("Restore should reject a truncated bytecode stream"), LoadResult, static_cast<int>(asSUCCESS));
	}

	TEST_METHOD(FailureLeavesModuleClean)
	{
		using namespace AngelscriptTest_AngelScriptSDK_AngelscriptRestoreTests_Private;
		TUniquePtr<FAngelscriptEngine> SourceEngineOwner = CreateIsolatedCloneEngine();
		if (!TestRunner->TestNotNull(TEXT("Restore failure cleanup test should create an isolated clone test engine"), SourceEngineOwner.Get()))
		{
			return;
		}
		FAngelscriptEngineScope EngineScope(*SourceEngineOwner);

		FAngelscriptEngine& SourceEngine = *SourceEngineOwner;
		ON_SCOPE_EXIT
		{
			SourceEngine.DiscardModule(TEXT("RestoreFailureCleanupSourceModule"));
		};

		asCModule* SourceModule = BuildRestoreModule(*TestRunner, SourceEngine, "RestoreFailureCleanupSourceModule");
		if (!TestRunner->TestNotNull(TEXT("Restore failure cleanup test should compile a source module"), SourceModule))
		{
			return;
		}

		FMemoryBinaryStream CompleteStream;
		const int SaveResult = SourceModule->SaveByteCode(&CompleteStream, false);
		if (!TestRunner->TestEqual(TEXT("Restore failure cleanup test should save bytecode successfully"), SaveResult, static_cast<int>(asSUCCESS)))
		{
			return;
		}
		if (!TestRunner->TestTrue(TEXT("Restore failure cleanup test should emit enough bytes to truncate"), CompleteStream.Num() > 16))
		{
			return;
		}

		FMemoryBinaryStream TruncatedStream = CompleteStream;
		TruncatedStream.Truncate(TruncatedStream.Num() - 16);
		TruncatedStream.ResetReadOffset();
		SourceModule->Discard();

		asCScriptEngine* ScriptEngine = static_cast<asCScriptEngine*>(SourceEngine.GetScriptEngine());
		const asPWORD PreviousInitGlobalsAfterBuild = ScriptEngine->GetEngineProperty(asEP_INIT_GLOBAL_VARS_AFTER_BUILD);
		ScriptEngine->SetEngineProperty(asEP_INIT_GLOBAL_VARS_AFTER_BUILD, 0);
		ON_SCOPE_EXIT
		{
			ScriptEngine->SetEngineProperty(asEP_INIT_GLOBAL_VARS_AFTER_BUILD, PreviousInitGlobalsAfterBuild);
		};

		asCModule* RestoredModule = CreateRestoreModule(ScriptEngine, "RestoreFailureCleanupSourceModule");
		if (!TestRunner->TestNotNull(TEXT("Restore failure cleanup test should create a destination module"), RestoredModule))
		{
			return;
		}

		bool bWasDebugInfoStripped = false;
		TestRunner->AddExpectedErrorPlain(TEXT("Unexpected end of file"), EAutomationExpectedErrorFlags::Contains, -1);
		const int FailedLoadResult = RestoredModule->LoadByteCode(&TruncatedStream, &bWasDebugInfoStripped);
		if (!TestRunner->TestNotEqual(TEXT("Restore failure cleanup test should reject the truncated bytecode stream"), FailedLoadResult, static_cast<int>(asSUCCESS)))
		{
			return;
		}

		if (!TestRunner->TestEqual(TEXT("Restore failure cleanup test should leave the failed module with zero functions"), RestoredModule->GetFunctionCount(), 0))
		{
			return;
		}
		if (!TestRunner->TestEqual(TEXT("Restore failure cleanup test should leave the failed module with zero globals"), RestoredModule->GetGlobalVarCount(), 0))
		{
			return;
		}
		if (!TestRunner->TestNull(TEXT("Restore failure cleanup test should not leave the failed function declaration behind"), RestoredModule->GetFunctionByDecl("int Test()")))
		{
			return;
		}

		RestoredModule->Discard();
		CompleteStream.ResetReadOffset();
		bWasDebugInfoStripped = true;

		asCModule* RetryModule = CreateRestoreModule(ScriptEngine, "RestoreFailureCleanupSourceModule");
		if (!TestRunner->TestNotNull(TEXT("Restore failure cleanup test should recreate the destination module after failure"), RetryModule))
		{
			return;
		}

		const int RetryLoadResult = RetryModule->LoadByteCode(&CompleteStream, &bWasDebugInfoStripped);
		if (!TestRunner->TestEqual(TEXT("Restore failure cleanup test should load the complete bytecode stream after retry"), RetryLoadResult, static_cast<int>(asSUCCESS)))
		{
			return;
		}
		if (!TestRunner->TestFalse(TEXT("Restore failure cleanup test should preserve debug info on the successful retry"), bWasDebugInfoStripped))
		{
			return;
		}

		const int ResetGlobalsResult = RetryModule->ResetGlobalVars(nullptr);
		if (!TestRunner->TestEqual(TEXT("Restore failure cleanup test should initialize globals before executing the retried module"), ResetGlobalsResult, static_cast<int>(asSUCCESS)))
		{
			return;
		}

		int32 RestoredValue = 0;
		if (!ExecuteRestoreFunction(*TestRunner, SourceEngine, *RetryModule, RestoredValue))
		{
			return;
		}

		TestRunner->TestEqual(TEXT("Restore failure cleanup test should execute the retried module successfully"), RestoredValue, 42);
	}
};

#endif
