#include "AngelscriptNativeTestSupport.h"
#include "CQTest.h"
#include "Misc/ScopeExit.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptNativeTestSupport;

namespace
{
	bool ExecuteIntFunction(FAutomationTestBase& Test, asIScriptEngine* ScriptEngine, asIScriptModule* Module, const char* Declaration, int32& OutValue)
	{
		asIScriptFunction* Function = AngelscriptNativeTestSupport::GetNativeFunctionByExactDecl(Module, Declaration);
		if (!Test.TestNotNull(TEXT("Reference save/load test should resolve the requested function"), Function))
		{
			return false;
		}

		asIScriptContext* Context = ScriptEngine->CreateContext();
		if (!Test.TestNotNull(TEXT("Reference save/load test should create an execution context"), Context))
		{
			return false;
		}

		const int ExecuteResult = AngelscriptNativeTestSupport::PrepareAndExecute(Context, Function);
		OutValue = static_cast<int32>(Context->GetReturnDWord());
		Context->Release();
		return Test.TestEqual(TEXT("Reference save/load test should execute successfully"), ExecuteResult, static_cast<int32>(asEXECUTION_FINISHED));
	}

	asIScriptModule* LoadModuleFromStream(asIScriptEngine* ScriptEngine, const char* ModuleName, FMemoryBinaryStream& Stream, bool& bWasDebugInfoStripped, int& OutLoadResult)
	{
		Stream.ResetReadOffset();
		asIScriptModule* Module = ScriptEngine != nullptr ? ScriptEngine->GetModule(ModuleName, asGM_ALWAYS_CREATE) : nullptr;
		OutLoadResult = Module != nullptr ? Module->LoadByteCode(&Stream, &bWasDebugInfoStripped) : asNO_MODULE;
		return Module;
	}
}

TEST_CLASS_WITH_FLAGS(FAngelscriptNativeReferenceSaveLoadTests,
	"Angelscript.TestModule.AngelScriptSDK.Reference.SaveLoad",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(RoundTripPreservesFunctionDeclarations)
	{
		AngelscriptNativeTestSupport::FNativeMessageCollector Messages;
		asIScriptEngine* ScriptEngine = AngelscriptNativeTestSupport::CreateNativeEngine(&Messages);
		if (!TestRunner->TestNotNull(TEXT("Reference save/load roundtrip should create a native engine"), ScriptEngine))
		{
			return;
		}

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
		if (!TestRunner->TestNotNull(TEXT("Reference save/load roundtrip should build the source module"), SourceModule))
		{
			TestRunner->AddInfo(AngelscriptNativeTestSupport::CollectMessages(Messages));
			return;
		}

		int32 SourceValue = 0;
		if (!ExecuteIntFunction(*TestRunner, ScriptEngine, SourceModule, "int Entry()", SourceValue))
		{
			return;
		}
		TestRunner->TestEqual(TEXT("Reference save/load roundtrip should execute before serialization"), SourceValue, 42);

		FMemoryBinaryStream Stream;
		const int SaveResult = SourceModule->SaveByteCode(&Stream, false);
		TestRunner->TestEqual(TEXT("Reference save/load roundtrip should save bytecode successfully"), SaveResult, static_cast<int32>(asSUCCESS));
		TestRunner->TestTrue(TEXT("Reference save/load roundtrip should emit a non-empty byte stream"), Stream.Num() > 0);

		ScriptEngine->DiscardModule("ReferenceSaveLoadSource");

		bool bWasDebugInfoStripped = true;
		int LoadResult = asERROR;
		asIScriptModule* RestoredModule = LoadModuleFromStream(ScriptEngine, "ReferenceSaveLoadRestored", Stream, bWasDebugInfoStripped, LoadResult);
		if (!TestRunner->TestNotNull(TEXT("Reference save/load roundtrip should create the restored module"), RestoredModule))
		{
			return;
		}
		TestRunner->TestEqual(TEXT("Reference save/load roundtrip should load bytecode successfully"), LoadResult, static_cast<int32>(asSUCCESS));
		TestRunner->TestFalse(TEXT("Reference save/load roundtrip should preserve debug information when not stripping"), bWasDebugInfoStripped);

		TestRunner->TestNotNull(TEXT("Reference save/load roundtrip should resolve Add after deserialization"),
			AngelscriptNativeTestSupport::GetNativeFunctionByDecl(RestoredModule, "int Add(int, int)"));
		TestRunner->TestNotNull(TEXT("Reference save/load roundtrip should resolve Entry after deserialization"),
			AngelscriptNativeTestSupport::GetNativeFunctionByDecl(RestoredModule, "int Entry()"));
		TestRunner->TestEqual(TEXT("Reference save/load roundtrip should preserve the function count after deserialization"),
			RestoredModule->GetFunctionCount(),
			2);
	}

	TEST_METHOD(StripDebugInfoReportsStrippedFlag)
	{
		AngelscriptNativeTestSupport::FNativeMessageCollector Messages;
		asIScriptEngine* ScriptEngine = AngelscriptNativeTestSupport::CreateNativeEngine(&Messages);
		if (!TestRunner->TestNotNull(TEXT("Reference stripped save/load should create a native engine"), ScriptEngine))
		{
			return;
		}

		ON_SCOPE_EXIT
		{
			AngelscriptNativeTestSupport::DestroyNativeEngine(ScriptEngine);
		};

		asIScriptModule* SourceModule = AngelscriptNativeTestSupport::BuildNativeModule(
			ScriptEngine,
			"ReferenceSaveLoadStripSource",
			"int Entry() { return 42; }");
		if (!TestRunner->TestNotNull(TEXT("Reference stripped save/load should build the source module"), SourceModule))
		{
			TestRunner->AddInfo(AngelscriptNativeTestSupport::CollectMessages(Messages));
			return;
		}

		FMemoryBinaryStream Stream;
		const int SaveResult = SourceModule->SaveByteCode(&Stream, true);
		TestRunner->TestEqual(TEXT("Reference stripped save/load should save bytecode successfully"), SaveResult, static_cast<int32>(asSUCCESS));

		ScriptEngine->DiscardModule("ReferenceSaveLoadStripSource");

		bool bWasDebugInfoStripped = false;
		int LoadResult = asERROR;
		asIScriptModule* RestoredModule = LoadModuleFromStream(ScriptEngine, "ReferenceSaveLoadStripRestored", Stream, bWasDebugInfoStripped, LoadResult);
		TestRunner->TestNotNull(TEXT("Reference stripped save/load should create the restored module"), RestoredModule);
		TestRunner->TestEqual(TEXT("Reference stripped save/load should load bytecode successfully"), LoadResult, static_cast<int32>(asSUCCESS));
		TestRunner->TestTrue(TEXT("Reference stripped save/load should report stripped debug information"), bWasDebugInfoStripped);
	}

	TEST_METHOD(TruncatedStreamFailsThenCompleteStreamStillLoads)
	{
		AngelscriptNativeTestSupport::FNativeMessageCollector Messages;
		asIScriptEngine* ScriptEngine = AngelscriptNativeTestSupport::CreateNativeEngine(&Messages);
		if (!TestRunner->TestNotNull(TEXT("Reference truncated save/load should create a native engine"), ScriptEngine))
		{
			return;
		}

		ON_SCOPE_EXIT
		{
			AngelscriptNativeTestSupport::DestroyNativeEngine(ScriptEngine);
		};

		asIScriptModule* SourceModule = AngelscriptNativeTestSupport::BuildNativeModule(
			ScriptEngine,
			"ReferenceSaveLoadTruncateSource",
			"int Entry() { return 42; }");
		if (!TestRunner->TestNotNull(TEXT("Reference truncated save/load should build the source module"), SourceModule))
		{
			TestRunner->AddInfo(AngelscriptNativeTestSupport::CollectMessages(Messages));
			return;
		}

		FMemoryBinaryStream CompleteStream;
		const int SaveResult = SourceModule->SaveByteCode(&CompleteStream, false);
		TestRunner->TestEqual(TEXT("Reference truncated save/load should save bytecode successfully"), SaveResult, static_cast<int32>(asSUCCESS));
		ScriptEngine->DiscardModule("ReferenceSaveLoadTruncateSource");

		FMemoryBinaryStream TruncatedStream = CompleteStream;
		TruncatedStream.TruncateBy(16);
		bool bWasDebugInfoStripped = false;
		int LoadResult = asSUCCESS;
		asIScriptModule* FailedModule = LoadModuleFromStream(ScriptEngine, "ReferenceSaveLoadTruncated", TruncatedStream, bWasDebugInfoStripped, LoadResult);
		TestRunner->TestNotNull(TEXT("Reference truncated save/load should still create the target module object"), FailedModule);
		TestRunner->TestTrue(TEXT("Reference truncated save/load should reject incomplete bytecode"), LoadResult < 0);

		ScriptEngine->DiscardModule("ReferenceSaveLoadTruncated");

		int RetryLoadResult = asERROR;
		asIScriptModule* RetryModule = LoadModuleFromStream(ScriptEngine, "ReferenceSaveLoadTruncated", CompleteStream, bWasDebugInfoStripped, RetryLoadResult);
		if (!TestRunner->TestNotNull(TEXT("Reference truncated save/load should create the retry module"), RetryModule))
		{
			return;
		}
		TestRunner->TestEqual(TEXT("Reference truncated save/load should load complete bytecode after a failed load"), RetryLoadResult, static_cast<int32>(asSUCCESS));

		int32 RetryValue = 0;
		if (!ExecuteIntFunction(*TestRunner, ScriptEngine, RetryModule, "int Entry()", RetryValue))
		{
			return;
		}
		TestRunner->TestEqual(TEXT("Reference truncated save/load should execute after a successful retry"), RetryValue, 42);
	}

	TEST_METHOD(MultipleFunctionsRemainResolvableAfterLoad)
	{
		AngelscriptNativeTestSupport::FNativeMessageCollector Messages;
		asIScriptEngine* ScriptEngine = AngelscriptNativeTestSupport::CreateNativeEngine(&Messages);
		if (!TestRunner->TestNotNull(TEXT("Reference multi-function save/load should create a native engine"), ScriptEngine))
		{
			return;
		}

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
		if (!TestRunner->TestNotNull(TEXT("Reference multi-function save/load should build the source module"), SourceModule))
		{
			TestRunner->AddInfo(AngelscriptNativeTestSupport::CollectMessages(Messages));
			return;
		}

		FMemoryBinaryStream Stream;
		TestRunner->TestEqual(TEXT("Reference multi-function save/load should save bytecode"), SourceModule->SaveByteCode(&Stream, false), static_cast<int32>(asSUCCESS));
		ScriptEngine->DiscardModule("ReferenceSaveLoadMultiSource");

		bool bWasDebugInfoStripped = true;
		int LoadResult = asERROR;
		asIScriptModule* RestoredModule = LoadModuleFromStream(ScriptEngine, "ReferenceSaveLoadMultiRestored", Stream, bWasDebugInfoStripped, LoadResult);
		if (!TestRunner->TestNotNull(TEXT("Reference multi-function save/load should create restored module"), RestoredModule))
		{
			return;
		}
		TestRunner->TestEqual(TEXT("Reference multi-function save/load should load bytecode"), LoadResult, static_cast<int32>(asSUCCESS));
		TestRunner->TestFalse(TEXT("Reference multi-function save/load should preserve debug information when not stripping"), bWasDebugInfoStripped);
		TestRunner->TestNotNull(TEXT("Reference multi-function save/load should resolve Left after load"), AngelscriptNativeTestSupport::GetNativeFunctionByDecl(RestoredModule, "int Left()"));
		TestRunner->TestNotNull(TEXT("Reference multi-function save/load should resolve Right after load"), AngelscriptNativeTestSupport::GetNativeFunctionByDecl(RestoredModule, "int Right()"));
		TestRunner->TestNotNull(TEXT("Reference multi-function save/load should resolve Entry after load"), AngelscriptNativeTestSupport::GetNativeFunctionByDecl(RestoredModule, "int Entry()"));

		int32 RestoredValue = 0;
		if (!ExecuteIntFunction(*TestRunner, ScriptEngine, RestoredModule, "int Entry()", RestoredValue))
		{
			return;
		}
		TestRunner->TestEqual(TEXT("Reference multi-function save/load should execute after load"), RestoredValue, 42);
	}
};

#endif
