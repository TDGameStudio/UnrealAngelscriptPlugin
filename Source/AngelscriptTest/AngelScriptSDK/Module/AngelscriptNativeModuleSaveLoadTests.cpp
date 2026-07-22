#include "../Support/AngelscriptNativeCoreTestSupport.h"
#include "CQTest.h"
#include "Misc/ScopeExit.h"

#if WITH_ANGELSCRIPT_UNITTESTS


TEST_CLASS_WITH_FLAGS(FModuleSaveLoadTests,
	"Angelscript.TestModule.AngelScriptSDK.Module.SaveLoad",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	static bool ExecuteIntFunction(FAutomationTestBase& Test, asIScriptEngine* ScriptEngine, asIScriptModule* Module, const char* Declaration, int32& OutValue)
	{
		FNoDiscardAsserter LocalAssert(Test);

		asIScriptFunction* Function = AngelscriptNativeTestSupport::GetNativeFunctionByExactDecl(Module, Declaration);
		if (!LocalAssert.IsNotNull(Function, TEXT("Reference save/load test should resolve the requested function")))
		{
			return false;
		}

		asIScriptContext* Context = ScriptEngine->CreateContext();
		if (!LocalAssert.IsNotNull(Context, TEXT("Reference save/load test should create an execution context")))
		{
			return false;
		}

		const int ExecuteResult = AngelscriptNativeTestSupport::PrepareAndExecute(Context, Function);
		OutValue = static_cast<int32>(Context->GetReturnDWord());
		Context->Release();
		return LocalAssert.AreEqual(static_cast<int32>(asEXECUTION_FINISHED), ExecuteResult, TEXT("Reference save/load test should execute successfully"));
	}

	static asIScriptModule* LoadModuleFromStream(asIScriptEngine* ScriptEngine, const char* ModuleName, AngelscriptNativeTestSupport::FMemoryBinaryStream& Stream, bool& bWasDebugInfoStripped, int& OutLoadResult)
	{
		Stream.ResetReadOffset();
		asIScriptModule* Module = ScriptEngine != nullptr ? ScriptEngine->GetModule(ModuleName, asGM_ALWAYS_CREATE) : nullptr;
		OutLoadResult = Module != nullptr ? Module->LoadByteCode(&Stream, &bWasDebugInfoStripped) : asNO_MODULE;
		return Module;
	}

public:
	TEST_METHOD(RoundTripPreservesFunctionDeclarations)
	{
		AngelscriptNativeTestSupport::FNativeMessageCollector Messages;
		asIScriptEngine* ScriptEngine = AngelscriptNativeTestSupport::CreateNativeEngine(&Messages);
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Reference save/load roundtrip should create a native engine")));

		ON_SCOPE_EXIT
		{
			AngelscriptNativeTestSupport::DestroyNativeEngine(ScriptEngine);
		};

		asIScriptModule* SourceModule = AngelscriptNativeTestSupport::BuildNativeModule(ScriptEngine, "ReferenceSaveLoadSource", R"(
int Add(int A, int B)
{
	return A + B;
}

		int Entry()
		{
			return Add(20, 22);
		}
)");
		if (!this->Assert.IsNotNull(SourceModule, TEXT("Reference save/load roundtrip should build the source module")))
		{
			TestRunner->AddInfo(AngelscriptNativeTestSupport::CollectMessages(Messages));
			return;
		}

		int32 SourceValue = 0;
		if (!ExecuteIntFunction(*TestRunner, ScriptEngine, SourceModule, "int Entry()", SourceValue))
		{
			return;
		}
		ASSERT_THAT(AreEqual(42, SourceValue, TEXT("Reference save/load roundtrip should execute before serialization")));

		AngelscriptNativeTestSupport::FMemoryBinaryStream Stream;
		const int SaveResult = SourceModule->SaveByteCode(&Stream, false);
		ASSERT_THAT(AreEqual(static_cast<int32>(asSUCCESS), SaveResult, TEXT("Reference save/load roundtrip should save bytecode successfully")));
		ASSERT_THAT(IsTrue(Stream.Num() > 0, TEXT("Reference save/load roundtrip should emit a non-empty byte stream")));

		ScriptEngine->DiscardModule("ReferenceSaveLoadSource");

		bool bWasDebugInfoStripped = true;
		int LoadResult = asERROR;
		asIScriptModule* RestoredModule = LoadModuleFromStream(ScriptEngine, "ReferenceSaveLoadRestored", Stream, bWasDebugInfoStripped, LoadResult);
		ASSERT_THAT(IsNotNull(RestoredModule, TEXT("Reference save/load roundtrip should create the restored module")));
		ASSERT_THAT(AreEqual(static_cast<int32>(asSUCCESS), LoadResult, TEXT("Reference save/load roundtrip should load bytecode successfully")));
		ASSERT_THAT(IsFalse(bWasDebugInfoStripped, TEXT("Reference save/load roundtrip should preserve debug information when not stripping")));

		ASSERT_THAT(IsNotNull(AngelscriptNativeTestSupport::GetNativeFunctionByDecl(RestoredModule, "int Add(const int, const int)"), TEXT("Reference save/load roundtrip should resolve Add after deserialization")));
		ASSERT_THAT(IsNotNull(AngelscriptNativeTestSupport::GetNativeFunctionByDecl(RestoredModule, "int Entry()"), TEXT("Reference save/load roundtrip should resolve Entry after deserialization")));
		ASSERT_THAT(AreEqual(2, RestoredModule->GetFunctionCount(), TEXT("Reference save/load roundtrip should preserve the function count after deserialization")));
	}

	TEST_METHOD(StripDebugInfoReportsStrippedFlag)
	{
		AngelscriptNativeTestSupport::FNativeMessageCollector Messages;
		asIScriptEngine* ScriptEngine = AngelscriptNativeTestSupport::CreateNativeEngine(&Messages);
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Reference stripped save/load should create a native engine")));

		ON_SCOPE_EXIT
		{
			AngelscriptNativeTestSupport::DestroyNativeEngine(ScriptEngine);
		};

		asIScriptModule* SourceModule = AngelscriptNativeTestSupport::BuildNativeModule(
			ScriptEngine,
			"ReferenceSaveLoadStripSource",
			"int Entry() { return 42; }");
		if (!this->Assert.IsNotNull(SourceModule, TEXT("Reference stripped save/load should build the source module")))
		{
			TestRunner->AddInfo(AngelscriptNativeTestSupport::CollectMessages(Messages));
			return;
		}

		AngelscriptNativeTestSupport::FMemoryBinaryStream Stream;
		const int SaveResult = SourceModule->SaveByteCode(&Stream, true);
		ASSERT_THAT(AreEqual(static_cast<int32>(asSUCCESS), SaveResult, TEXT("Reference stripped save/load should save bytecode successfully")));

		ScriptEngine->DiscardModule("ReferenceSaveLoadStripSource");

		bool bWasDebugInfoStripped = false;
		int LoadResult = asERROR;
		asIScriptModule* RestoredModule = LoadModuleFromStream(ScriptEngine, "ReferenceSaveLoadStripRestored", Stream, bWasDebugInfoStripped, LoadResult);
		ASSERT_THAT(IsNotNull(RestoredModule, TEXT("Reference stripped save/load should create the restored module")));
		ASSERT_THAT(AreEqual(static_cast<int32>(asSUCCESS), LoadResult, TEXT("Reference stripped save/load should load bytecode successfully")));
		ASSERT_THAT(IsTrue(bWasDebugInfoStripped, TEXT("Reference stripped save/load should report stripped debug information")));
	}

	TEST_METHOD(TruncatedStreamFailsThenCompleteStreamStillLoads)
	{
		AngelscriptNativeTestSupport::FNativeMessageCollector Messages;
		asIScriptEngine* ScriptEngine = AngelscriptNativeTestSupport::CreateNativeEngine(&Messages);
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Reference truncated save/load should create a native engine")));

		ON_SCOPE_EXIT
		{
			AngelscriptNativeTestSupport::DestroyNativeEngine(ScriptEngine);
		};

		asIScriptModule* SourceModule = AngelscriptNativeTestSupport::BuildNativeModule(
			ScriptEngine,
			"ReferenceSaveLoadTruncateSource",
			"int Entry() { return 42; }");
		if (!this->Assert.IsNotNull(SourceModule, TEXT("Reference truncated save/load should build the source module")))
		{
			TestRunner->AddInfo(AngelscriptNativeTestSupport::CollectMessages(Messages));
			return;
		}

		AngelscriptNativeTestSupport::FMemoryBinaryStream CompleteStream;
		const int SaveResult = SourceModule->SaveByteCode(&CompleteStream, false);
		ASSERT_THAT(AreEqual(static_cast<int32>(asSUCCESS), SaveResult, TEXT("Reference truncated save/load should save bytecode successfully")));
		ScriptEngine->DiscardModule("ReferenceSaveLoadTruncateSource");

		AngelscriptNativeTestSupport::FMemoryBinaryStream TruncatedStream = CompleteStream;
		TruncatedStream.TruncateBy(16);
		bool bWasDebugInfoStripped = false;
		int LoadResult = asSUCCESS;
		asIScriptModule* FailedModule = LoadModuleFromStream(ScriptEngine, "ReferenceSaveLoadTruncated", TruncatedStream, bWasDebugInfoStripped, LoadResult);
		ASSERT_THAT(IsNotNull(FailedModule, TEXT("Reference truncated save/load should still create the target module object")));
		ASSERT_THAT(IsTrue(LoadResult < 0, TEXT("Reference truncated save/load should reject incomplete bytecode")));

		ScriptEngine->DiscardModule("ReferenceSaveLoadTruncated");

		int RetryLoadResult = asERROR;
		asIScriptModule* RetryModule = LoadModuleFromStream(ScriptEngine, "ReferenceSaveLoadTruncated", CompleteStream, bWasDebugInfoStripped, RetryLoadResult);
		ASSERT_THAT(IsNotNull(RetryModule, TEXT("Reference truncated save/load should create the retry module")));
		ASSERT_THAT(AreEqual(static_cast<int32>(asSUCCESS), RetryLoadResult, TEXT("Reference truncated save/load should load complete bytecode after a failed load")));

		int32 RetryValue = 0;
		if (!ExecuteIntFunction(*TestRunner, ScriptEngine, RetryModule, "int Entry()", RetryValue))
		{
			return;
		}
		ASSERT_THAT(AreEqual(42, RetryValue, TEXT("Reference truncated save/load should execute after a successful retry")));
	}

	TEST_METHOD(MultipleFunctionsRemainResolvableAfterLoad)
	{
		AngelscriptNativeTestSupport::FNativeMessageCollector Messages;
		asIScriptEngine* ScriptEngine = AngelscriptNativeTestSupport::CreateNativeEngine(&Messages);
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Reference multi-function save/load should create a native engine")));

		ON_SCOPE_EXIT
		{
			AngelscriptNativeTestSupport::DestroyNativeEngine(ScriptEngine);
		};

		asIScriptModule* SourceModule = AngelscriptNativeTestSupport::BuildNativeModule(ScriptEngine, "ReferenceSaveLoadMultiSource", R"(
int Left()
{
	return 20;
}

int Right()
{
	return 22;
}

		int Entry()
		{
			return Left() + Right();
		}
)");
		if (!this->Assert.IsNotNull(SourceModule, TEXT("Reference multi-function save/load should build the source module")))
		{
			TestRunner->AddInfo(AngelscriptNativeTestSupport::CollectMessages(Messages));
			return;
		}

		AngelscriptNativeTestSupport::FMemoryBinaryStream Stream;
		ASSERT_THAT(AreEqual(static_cast<int32>(asSUCCESS), SourceModule->SaveByteCode(&Stream, false), TEXT("Reference multi-function save/load should save bytecode")));
		ScriptEngine->DiscardModule("ReferenceSaveLoadMultiSource");

		bool bWasDebugInfoStripped = true;
		int LoadResult = asERROR;
		asIScriptModule* RestoredModule = LoadModuleFromStream(ScriptEngine, "ReferenceSaveLoadMultiRestored", Stream, bWasDebugInfoStripped, LoadResult);
		ASSERT_THAT(IsNotNull(RestoredModule, TEXT("Reference multi-function save/load should create restored module")));
		ASSERT_THAT(AreEqual(static_cast<int32>(asSUCCESS), LoadResult, TEXT("Reference multi-function save/load should load bytecode")));
		ASSERT_THAT(IsFalse(bWasDebugInfoStripped, TEXT("Reference multi-function save/load should preserve debug information when not stripping")));
		ASSERT_THAT(IsNotNull(AngelscriptNativeTestSupport::GetNativeFunctionByDecl(RestoredModule, "int Left()"), TEXT("Reference multi-function save/load should resolve Left after load")));
		ASSERT_THAT(IsNotNull(AngelscriptNativeTestSupport::GetNativeFunctionByDecl(RestoredModule, "int Right()"), TEXT("Reference multi-function save/load should resolve Right after load")));
		ASSERT_THAT(IsNotNull(AngelscriptNativeTestSupport::GetNativeFunctionByDecl(RestoredModule, "int Entry()"), TEXT("Reference multi-function save/load should resolve Entry after load")));

		int32 RestoredValue = 0;
		if (!ExecuteIntFunction(*TestRunner, ScriptEngine, RestoredModule, "int Entry()", RestoredValue))
		{
			return;
		}
		ASSERT_THAT(AreEqual(42, RestoredValue, TEXT("Reference multi-function save/load should execute after load")));
	}
};

#endif
