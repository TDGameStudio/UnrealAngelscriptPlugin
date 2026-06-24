#include "AngelscriptNativeTestSupport.h"
#include "AngelscriptTestUtilities.h"
#include "CQTest.h"
#include "Misc/ScopeExit.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_context.h"
#include "source/as_module.h"
#include "source/as_restore.h"
#include "source/as_scriptengine.h"
#include "EndAngelscriptHeaders.h"

#if WITH_DEV_AUTOMATION_TESTS


TEST_CLASS_WITH_FLAGS(FAngelscriptRestoreTests,
	"Angelscript.TestModule.AngelScriptSDK.Restore",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	static asCModule* CreateRestoreModule(asCScriptEngine* ScriptEngine, const char* ModuleName)
	{
		return static_cast<asCModule*>(ScriptEngine->GetModule(ModuleName, asGM_ALWAYS_CREATE));
	}

	static asCModule* BuildRestoreModule(FAutomationTestBase& Test, FAngelscriptEngine& Engine, const char* ModuleName)
	{
		asIScriptModule* Module = BuildModule(
			Test,
			Engine,
			ModuleName,
			TEXT("const int GlobalValue = 41; int Test() { return GlobalValue + 1; }"));
		return static_cast<asCModule*>(Module);
	}

	static bool ExecuteRestoreFunction(FAutomationTestBase& Test, FAngelscriptEngine& Engine, asCModule& Module, int32& OutValue)
	{
		asIScriptFunction* Function = GetFunctionByDecl(Test, Module, TEXT("int Test()"));
		if (Function == nullptr)
		{
			return false;
		}

		return ExecuteIntFunction(Test, Engine, *Function, OutValue);
	}

public:
	TEST_METHOD(RoundTrip)
	{
		TUniquePtr<FAngelscriptEngine> SourceEngineOwner = CreateIsolatedCloneEngine();
		ASSERT_THAT(IsNotNull(SourceEngineOwner.Get(), TEXT("Restore roundtrip should create an isolated clone test engine")));
		FAngelscriptEngineScope EngineScope(*SourceEngineOwner);

		FAngelscriptEngine& SourceEngine = *SourceEngineOwner;
		ON_SCOPE_EXIT
		{
			SourceEngine.DiscardModule(TEXT("RestoreSourceModule"));
		};

		asCModule* SourceModule = BuildRestoreModule(*TestRunner, SourceEngine, "RestoreSourceModule");
		ASSERT_THAT(IsNotNull(SourceModule, TEXT("Restore roundtrip should compile a source module")));

		int32 SourceValue = 0;
		if (!ExecuteRestoreFunction(*TestRunner, SourceEngine, *SourceModule, SourceValue))
		{
			return;
		}
		ASSERT_THAT(AreEqual(42, SourceValue, TEXT("Restore roundtrip should execute before serialization")));

		AngelscriptNativeTestSupport::FMemoryBinaryStream Stream;
		const int SaveResult = SourceModule->SaveByteCode(&Stream, false);
		ASSERT_THAT(AreEqual(static_cast<int>(asSUCCESS), SaveResult, TEXT("Restore roundtrip should save bytecode successfully")));
		ASSERT_THAT(IsTrue(Stream.Num() > 0, TEXT("Restore roundtrip should emit bytecode bytes")));

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
		ASSERT_THAT(IsNotNull(RestoredModule, TEXT("Restore roundtrip should create a destination module")));

		const int LoadResult = RestoredModule->LoadByteCode(&Stream, &bWasDebugInfoStripped);
		ASSERT_THAT(AreEqual(static_cast<int>(asSUCCESS), LoadResult, TEXT("Restore roundtrip should load bytecode successfully")));
		ASSERT_THAT(IsFalse(bWasDebugInfoStripped, TEXT("Restore roundtrip should preserve debug info when not stripping")));
	}

	TEST_METHOD(StripDebugInfoRoundTrip)
	{
		TUniquePtr<FAngelscriptEngine> SourceEngineOwner = CreateIsolatedCloneEngine();
		ASSERT_THAT(IsNotNull(SourceEngineOwner.Get(), TEXT("Restore strip roundtrip should create an isolated clone test engine")));
		FAngelscriptEngineScope EngineScope(*SourceEngineOwner);

		FAngelscriptEngine& SourceEngine = *SourceEngineOwner;
		ON_SCOPE_EXIT
		{
			SourceEngine.DiscardModule(TEXT("RestoreStripSourceModule"));
		};

		asCModule* SourceModule = BuildRestoreModule(*TestRunner, SourceEngine, "RestoreStripSourceModule");
		ASSERT_THAT(IsNotNull(SourceModule, TEXT("Restore strip roundtrip should compile a source module")));

		AngelscriptNativeTestSupport::FMemoryBinaryStream Stream;
		const int SaveResult = SourceModule->SaveByteCode(&Stream, true);
		ASSERT_THAT(AreEqual(static_cast<int>(asSUCCESS), SaveResult, TEXT("Restore strip roundtrip should save bytecode successfully")));

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
		ASSERT_THAT(IsNotNull(RestoredModule, TEXT("Restore strip roundtrip should create a destination module")));

		const int LoadResult = RestoredModule->LoadByteCode(&Stream, &bWasDebugInfoStripped);
		ASSERT_THAT(AreEqual(static_cast<int>(asSUCCESS), LoadResult, TEXT("Restore strip roundtrip should load bytecode successfully")));
		ASSERT_THAT(IsTrue(bWasDebugInfoStripped, TEXT("Restore strip roundtrip should report stripped debug info")));
	}

	TEST_METHOD(EmptyStreamFails)
	{
		TUniquePtr<FAngelscriptEngine> EngineOwner = CreateIsolatedCloneEngine();
		ASSERT_THAT(IsNotNull(EngineOwner.Get(), TEXT("Restore empty stream test should create an isolated clone test engine")));
		FAngelscriptEngineScope EngineScope(*EngineOwner);

		asCScriptEngine* ScriptEngine = static_cast<asCScriptEngine*>(EngineOwner->GetScriptEngine());
		asCModule* RestoredModule = CreateRestoreModule(ScriptEngine, "RestoreEmptyStream");
		ASSERT_THAT(IsNotNull(RestoredModule, TEXT("Restore empty stream test should create a destination module")));

		AngelscriptNativeTestSupport::FMemoryBinaryStream Stream;
		bool bWasDebugInfoStripped = false;
		TestRunner->AddExpectedErrorPlain(TEXT("Unexpected end of file"), EAutomationExpectedErrorFlags::Contains, -1);
		const int LoadResult = RestoredModule->LoadByteCode(&Stream, &bWasDebugInfoStripped);
		ASSERT_THAT(AreNotEqual(static_cast<int>(asSUCCESS), LoadResult, TEXT("Restore should reject an empty bytecode stream")));
	}

	TEST_METHOD(TruncatedStreamFails)
	{
		TUniquePtr<FAngelscriptEngine> SourceEngineOwner = CreateIsolatedCloneEngine();
		ASSERT_THAT(IsNotNull(SourceEngineOwner.Get(), TEXT("Restore truncated stream test should create an isolated clone test engine")));
		FAngelscriptEngineScope EngineScope(*SourceEngineOwner);

		FAngelscriptEngine& SourceEngine = *SourceEngineOwner;
		ON_SCOPE_EXIT
		{
			SourceEngine.DiscardModule(TEXT("RestoreTruncatedSourceModule"));
		};

		asCModule* SourceModule = BuildRestoreModule(*TestRunner, SourceEngine, "RestoreTruncatedSourceModule");
		ASSERT_THAT(IsNotNull(SourceModule, TEXT("Restore truncated stream test should compile a source module")));

		AngelscriptNativeTestSupport::FMemoryBinaryStream Stream;
		const int SaveResult = SourceModule->SaveByteCode(&Stream, false);
		ASSERT_THAT(AreEqual(static_cast<int>(asSUCCESS), SaveResult, TEXT("Restore truncated stream test should save bytecode successfully")));
		ASSERT_THAT(IsTrue(Stream.Num() > 16, TEXT("Restore truncated stream test should emit enough bytes to truncate")));

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
		ASSERT_THAT(IsNotNull(RestoredModule, TEXT("Restore truncated stream test should create a destination module")));

		bool bWasDebugInfoStripped = false;
		TestRunner->AddExpectedErrorPlain(TEXT("Unexpected end of file"), EAutomationExpectedErrorFlags::Contains, -1);
		const int LoadResult = RestoredModule->LoadByteCode(&Stream, &bWasDebugInfoStripped);
		ASSERT_THAT(AreNotEqual(static_cast<int>(asSUCCESS), LoadResult, TEXT("Restore should reject a truncated bytecode stream")));
	}

	TEST_METHOD(FailureLeavesModuleClean)
	{
		TUniquePtr<FAngelscriptEngine> SourceEngineOwner = CreateIsolatedCloneEngine();
		ASSERT_THAT(IsNotNull(SourceEngineOwner.Get(), TEXT("Restore failure cleanup test should create an isolated clone test engine")));
		FAngelscriptEngineScope EngineScope(*SourceEngineOwner);

		FAngelscriptEngine& SourceEngine = *SourceEngineOwner;
		ON_SCOPE_EXIT
		{
			SourceEngine.DiscardModule(TEXT("RestoreFailureCleanupSourceModule"));
		};

		asCModule* SourceModule = BuildRestoreModule(*TestRunner, SourceEngine, "RestoreFailureCleanupSourceModule");
		ASSERT_THAT(IsNotNull(SourceModule, TEXT("Restore failure cleanup test should compile a source module")));

		AngelscriptNativeTestSupport::FMemoryBinaryStream CompleteStream;
		const int SaveResult = SourceModule->SaveByteCode(&CompleteStream, false);
		ASSERT_THAT(AreEqual(static_cast<int>(asSUCCESS), SaveResult, TEXT("Restore failure cleanup test should save bytecode successfully")));
		ASSERT_THAT(IsTrue(CompleteStream.Num() > 16, TEXT("Restore failure cleanup test should emit enough bytes to truncate")));

		AngelscriptNativeTestSupport::FMemoryBinaryStream TruncatedStream = CompleteStream;
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
		ASSERT_THAT(IsNotNull(RestoredModule, TEXT("Restore failure cleanup test should create a destination module")));

		bool bWasDebugInfoStripped = false;
		TestRunner->AddExpectedErrorPlain(TEXT("Unexpected end of file"), EAutomationExpectedErrorFlags::Contains, -1);
		const int FailedLoadResult = RestoredModule->LoadByteCode(&TruncatedStream, &bWasDebugInfoStripped);
		ASSERT_THAT(AreNotEqual(static_cast<int>(asSUCCESS), FailedLoadResult, TEXT("Restore failure cleanup test should reject the truncated bytecode stream")));

		ASSERT_THAT(AreEqual(0, RestoredModule->GetFunctionCount(), TEXT("Restore failure cleanup test should leave the failed module with zero functions")));
		ASSERT_THAT(AreEqual(0, RestoredModule->GetGlobalVarCount(), TEXT("Restore failure cleanup test should leave the failed module with zero globals")));
		ASSERT_THAT(IsNull(RestoredModule->GetFunctionByDecl("int Test()"), TEXT("Restore failure cleanup test should not leave the failed function declaration behind")));

		RestoredModule->Discard();
		CompleteStream.ResetReadOffset();
		bWasDebugInfoStripped = true;

		asCModule* RetryModule = CreateRestoreModule(ScriptEngine, "RestoreFailureCleanupSourceModule");
		ASSERT_THAT(IsNotNull(RetryModule, TEXT("Restore failure cleanup test should recreate the destination module after failure")));

		const int RetryLoadResult = RetryModule->LoadByteCode(&CompleteStream, &bWasDebugInfoStripped);
		ASSERT_THAT(AreEqual(static_cast<int>(asSUCCESS), RetryLoadResult, TEXT("Restore failure cleanup test should load the complete bytecode stream after retry")));
		ASSERT_THAT(IsFalse(bWasDebugInfoStripped, TEXT("Restore failure cleanup test should preserve debug info on the successful retry")));

		const int ResetGlobalsResult = RetryModule->ResetGlobalVars(nullptr);
		ASSERT_THAT(AreEqual(static_cast<int>(asSUCCESS), ResetGlobalsResult, TEXT("Restore failure cleanup test should initialize globals before executing the retried module")));

		int32 RestoredValue = 0;
		if (!ExecuteRestoreFunction(*TestRunner, SourceEngine, *RetryModule, RestoredValue))
		{
			return;
		}

		ASSERT_THAT(AreEqual(42, RestoredValue, TEXT("Restore failure cleanup test should execute the retried module successfully")));
	}
};

#endif
