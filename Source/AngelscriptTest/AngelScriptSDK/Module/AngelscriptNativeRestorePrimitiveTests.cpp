#include "../Support/AngelscriptNativeCoreTestSupport.h"

#include "CQTest.h"
#include "Misc/ScopeExit.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_module.h"
#include "source/as_restore.h"
#include "source/as_scriptengine.h"
#include "EndAngelscriptHeaders.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FRestorePrimitiveTests,
	"Angelscript.TestModule.AngelScriptSDK.Module.RestorePrimitives",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	static asCModule* BuildRestoreModule(
		FAutomationTestBase& Test,
		AngelscriptNativeTestSupport::FNativeTestEngine& Engine,
		const char* ModuleName)
	{
		asIScriptModule* Module = AngelscriptNativeTestSupport::BuildNativeModule(
			Engine.Get(), ModuleName,
			"const int GlobalValue = 41; int Test() { return GlobalValue + 1; }");
		if (Module == nullptr)
		{
			Test.AddError(Engine.GetMessagesText());
		}
		return static_cast<asCModule*>(Module);
	}

	static bool ExecuteRestoreFunction(
		FAutomationTestBase& Test,
		asIScriptEngine& Engine,
		asCModule& Module,
		int32& OutValue)
	{
		asIScriptFunction* Function = AngelscriptNativeTestSupport::GetNativeFunctionByExactDecl(&Module, "int Test()");
		if (Function == nullptr)
		{
			Test.AddError(TEXT("Restore test should resolve int Test() by exact declaration"));
			return false;
		}

		asIScriptContext* Context = Engine.CreateContext();
		if (Context == nullptr)
		{
			Test.AddError(TEXT("Restore test should create an execution context"));
			return false;
		}
		ON_SCOPE_EXIT { Context->Release(); };

		if (AngelscriptNativeTestSupport::PrepareAndExecute(Context, Function) != asEXECUTION_FINISHED)
		{
			Test.AddError(TEXT("Restore test should finish executing the restored function"));
			return false;
		}

		OutValue = static_cast<int32>(Context->GetReturnDWord());
		return true;
	}

	static asCModule* CreateDestinationModule(asCScriptEngine& Engine, const char* ModuleName)
	{
		return static_cast<asCModule*>(Engine.GetModule(ModuleName, asGM_ALWAYS_CREATE));
	}

	static void DisableAutomaticGlobalInitialization(asCScriptEngine& Engine, asPWORD& OutPreviousValue)
	{
		OutPreviousValue = Engine.GetEngineProperty(asEP_INIT_GLOBAL_VARS_AFTER_BUILD);
		Engine.SetEngineProperty(asEP_INIT_GLOBAL_VARS_AFTER_BUILD, 0);
	}

public:
	TEST_METHOD(RestorePrimitiveRoundTrip)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		asCScriptEngine* ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Restore roundtrip should create a raw SDK engine")));
		if (ScriptEngine == nullptr) return;

		asCModule* SourceModule = BuildRestoreModule(*TestRunner, Engine, "RestoreRoundTrip");
		ASSERT_THAT(IsNotNull(SourceModule, TEXT("Restore roundtrip should build the source module")));
		if (SourceModule == nullptr) return;

		int32 SourceValue = 0;
		if (!ExecuteRestoreFunction(*TestRunner, *ScriptEngine, *SourceModule, SourceValue)) return;
		ASSERT_THAT(AreEqual(42, SourceValue, TEXT("Restore roundtrip should execute before serialization")));

		AngelscriptNativeTestSupport::FMemoryBinaryStream Stream;
		ASSERT_THAT(AreEqual(static_cast<int32>(asSUCCESS), SourceModule->SaveByteCode(&Stream, false),
			TEXT("Restore roundtrip should save bytecode successfully")));
		ASSERT_THAT(IsTrue(Stream.Num() > 0, TEXT("Restore roundtrip should produce bytecode bytes")));

		SourceModule->Discard();
		Stream.ResetReadPosition();
		asPWORD PreviousInit = 0;
		DisableAutomaticGlobalInitialization(*ScriptEngine, PreviousInit);
		ON_SCOPE_EXIT { ScriptEngine->SetEngineProperty(asEP_INIT_GLOBAL_VARS_AFTER_BUILD, PreviousInit); };
		asCModule* RestoredModule = CreateDestinationModule(*ScriptEngine, "RestoreRoundTrip");
		ASSERT_THAT(IsNotNull(RestoredModule, TEXT("Restore roundtrip should create a destination module")));
		if (RestoredModule == nullptr) return;
		bool bWasDebugInfoStripped = true;
		ASSERT_THAT(AreEqual(static_cast<int32>(asSUCCESS), RestoredModule->LoadByteCode(&Stream, &bWasDebugInfoStripped),
			TEXT("Restore roundtrip should load bytecode successfully")));
		ASSERT_THAT(IsFalse(bWasDebugInfoStripped, TEXT("Restore roundtrip should retain debug information")));
	}

	TEST_METHOD(StripDebugInfoRoundTrip)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		asCScriptEngine* ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		if (ScriptEngine == nullptr) { TestRunner->AddError(TEXT("Restore strip test should create a raw SDK engine")); return; }
		asCModule* SourceModule = BuildRestoreModule(*TestRunner, Engine, "RestoreStrip");
		if (SourceModule == nullptr) return;
		AngelscriptNativeTestSupport::FMemoryBinaryStream Stream;
		ASSERT_THAT(AreEqual(static_cast<int32>(asSUCCESS), SourceModule->SaveByteCode(&Stream, true),
			TEXT("Restore strip test should save stripped bytecode")));
		SourceModule->Discard();
		Stream.ResetReadPosition();
		asPWORD PreviousInit = 0;
		DisableAutomaticGlobalInitialization(*ScriptEngine, PreviousInit);
		ON_SCOPE_EXIT { ScriptEngine->SetEngineProperty(asEP_INIT_GLOBAL_VARS_AFTER_BUILD, PreviousInit); };
		asCModule* RestoredModule = CreateDestinationModule(*ScriptEngine, "RestoreStrip");
		if (RestoredModule == nullptr) { TestRunner->AddError(TEXT("Restore strip test should create a destination module")); return; }
		bool bWasDebugInfoStripped = false;
		ASSERT_THAT(AreEqual(static_cast<int32>(asSUCCESS), RestoredModule->LoadByteCode(&Stream, &bWasDebugInfoStripped),
			TEXT("Restore strip test should load stripped bytecode")));
		ASSERT_THAT(IsTrue(bWasDebugInfoStripped, TEXT("Restore strip test should report stripped debug information")));
	}

	TEST_METHOD(EmptyStreamFails)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		asCScriptEngine* ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		if (ScriptEngine == nullptr) { TestRunner->AddError(TEXT("Restore empty-stream test should create a raw SDK engine")); return; }
		asCModule* Module = CreateDestinationModule(*ScriptEngine, "RestoreEmpty");
		if (Module == nullptr) { TestRunner->AddError(TEXT("Restore empty-stream test should create a destination module")); return; }
		AngelscriptNativeTestSupport::FMemoryBinaryStream Stream;
		bool bWasDebugInfoStripped = false;
		ASSERT_THAT(AreNotEqual(static_cast<int32>(asSUCCESS), Module->LoadByteCode(&Stream, &bWasDebugInfoStripped),
			TEXT("Restore should reject an empty bytecode stream")));
	}

	TEST_METHOD(TruncatedStreamFails)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		asCScriptEngine* ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		if (ScriptEngine == nullptr) { TestRunner->AddError(TEXT("Restore truncation test should create a raw SDK engine")); return; }
		asCModule* SourceModule = BuildRestoreModule(*TestRunner, Engine, "RestoreTruncated");
		if (SourceModule == nullptr) return;
		AngelscriptNativeTestSupport::FMemoryBinaryStream Stream;
		ASSERT_THAT(AreEqual(static_cast<int32>(asSUCCESS), SourceModule->SaveByteCode(&Stream, false),
			TEXT("Restore truncation test should save bytecode")));
		ASSERT_THAT(IsTrue(Stream.Num() > 16, TEXT("Restore truncation test needs enough bytes to truncate")));
		Stream.TruncateBy(16);
		Stream.ResetReadPosition();
		SourceModule->Discard();
		asPWORD PreviousInit = 0;
		DisableAutomaticGlobalInitialization(*ScriptEngine, PreviousInit);
		ON_SCOPE_EXIT { ScriptEngine->SetEngineProperty(asEP_INIT_GLOBAL_VARS_AFTER_BUILD, PreviousInit); };
		asCModule* Module = CreateDestinationModule(*ScriptEngine, "RestoreTruncated");
		if (Module == nullptr) { TestRunner->AddError(TEXT("Restore truncation test should create a destination module")); return; }
		bool bWasDebugInfoStripped = false;
		ASSERT_THAT(AreNotEqual(static_cast<int32>(asSUCCESS), Module->LoadByteCode(&Stream, &bWasDebugInfoStripped),
			TEXT("Restore should reject a truncated bytecode stream")));
	}

	TEST_METHOD(FailureLeavesModuleClean)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		asCScriptEngine* ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		if (ScriptEngine == nullptr) { TestRunner->AddError(TEXT("Restore cleanup test should create a raw SDK engine")); return; }
		asCModule* SourceModule = BuildRestoreModule(*TestRunner, Engine, "RestoreCleanup");
		if (SourceModule == nullptr) return;
		AngelscriptNativeTestSupport::FMemoryBinaryStream CompleteStream;
		ASSERT_THAT(AreEqual(static_cast<int32>(asSUCCESS), SourceModule->SaveByteCode(&CompleteStream, false),
			TEXT("Restore cleanup test should save bytecode")));
		AngelscriptNativeTestSupport::FMemoryBinaryStream TruncatedStream = CompleteStream;
		ASSERT_THAT(IsTrue(TruncatedStream.Num() > 16, TEXT("Restore cleanup test needs enough bytes to truncate")));
		TruncatedStream.TruncateBy(16);
		TruncatedStream.ResetReadPosition();
		SourceModule->Discard();
		asPWORD PreviousInit = 0;
		DisableAutomaticGlobalInitialization(*ScriptEngine, PreviousInit);
		ON_SCOPE_EXIT { ScriptEngine->SetEngineProperty(asEP_INIT_GLOBAL_VARS_AFTER_BUILD, PreviousInit); };
		asCModule* FailedModule = CreateDestinationModule(*ScriptEngine, "RestoreCleanup");
		if (FailedModule == nullptr) { TestRunner->AddError(TEXT("Restore cleanup test should create a destination module")); return; }
		bool bWasDebugInfoStripped = false;
		ASSERT_THAT(AreNotEqual(static_cast<int32>(asSUCCESS), FailedModule->LoadByteCode(&TruncatedStream, &bWasDebugInfoStripped),
			TEXT("Restore cleanup test should reject truncated bytecode")));
		ASSERT_THAT(AreEqual(0, FailedModule->GetFunctionCount(), TEXT("Failed load should leave no functions")));
		ASSERT_THAT(AreEqual(0, FailedModule->GetGlobalVarCount(), TEXT("Failed load should leave no globals")));

		FailedModule->Discard();
		CompleteStream.ResetReadPosition();
		asCModule* RetryModule = CreateDestinationModule(*ScriptEngine, "RestoreCleanup");
		if (RetryModule == nullptr) { TestRunner->AddError(TEXT("Restore cleanup retry should create a destination module")); return; }
		bWasDebugInfoStripped = true;
		ASSERT_THAT(AreEqual(static_cast<int32>(asSUCCESS), RetryModule->LoadByteCode(&CompleteStream, &bWasDebugInfoStripped),
			TEXT("Restore cleanup retry should load complete bytecode")));
		ASSERT_THAT(IsFalse(bWasDebugInfoStripped, TEXT("Restore cleanup retry should retain debug information")));
		ASSERT_THAT(AreEqual(static_cast<int32>(asSUCCESS), RetryModule->ResetGlobalVars(nullptr),
			TEXT("Restore cleanup retry should initialize globals")));
		int32 RestoredValue = 0;
		if (!ExecuteRestoreFunction(*TestRunner, *ScriptEngine, *RetryModule, RestoredValue)) return;
		ASSERT_THAT(AreEqual(42, RestoredValue, TEXT("Restore cleanup retry should execute successfully")));
	}
};

#endif
