#include "../Support/AngelscriptNativeCoreTestSupport.h"
#include "AngelscriptTestMacros.h"
#include "CQTest.h"
#include "Misc/ScopeExit.h"

#if WITH_ANGELSCRIPT_UNITTESTS


TEST_CLASS_WITH_FLAGS(FModuleSaveLoadTests,
	"Angelscript.TestModule.AngelScriptSDK.Module.SaveLoad",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	struct FObjectLastSaveLoadProbe
	{
		int32 Value = 0;
	};

	struct FObjectLastSaveLoadObservation
	{
		asIScriptEngine* ExpectedEngine = nullptr;
		asIScriptEngine* ObservedEngine = nullptr;
		int32 LiveObjects = 0;
		int32 ConstructorCount = 0;
		int32 DestructorCount = 0;
		int32 CallCount = 0;
		int32 LastObjectValue = INDEX_NONE;
		int32 LastDelta = INDEX_NONE;
		int64 LastWide = MIN_int64;

		void Reset(asIScriptEngine* InExpectedEngine)
		{
			*this = FObjectLastSaveLoadObservation();
			ExpectedEngine = InExpectedEngine;
		}
	};

	static void ConstructObjectLastSaveLoadProbe(
		const int32 Value,
		FObjectLastSaveLoadProbe* Address)
	{
		new (Address) FObjectLastSaveLoadProbe();
		Address->Value = Value;
		ObjectLastSaveLoadObservation.LiveObjects++;
		ObjectLastSaveLoadObservation.ConstructorCount++;
	}

	static void DestructObjectLastSaveLoadProbe(FObjectLastSaveLoadProbe* Address)
	{
		ObjectLastSaveLoadObservation.LiveObjects--;
		ObjectLastSaveLoadObservation.DestructorCount++;
		Address->~FObjectLastSaveLoadProbe();
	}

	static int32 EvaluateObjectLastSaveLoadProbe(
		const int32 Delta,
		const int64 Wide,
		const FObjectLastSaveLoadProbe* Object)
	{
		asIScriptContext* const Context = asGetActiveContext();
		ObjectLastSaveLoadObservation.ObservedEngine =
			Context != nullptr ? Context->GetEngine() : nullptr;
		ObjectLastSaveLoadObservation.CallCount++;
		ObjectLastSaveLoadObservation.LastObjectValue =
			Object != nullptr ? Object->Value : INDEX_NONE;
		ObjectLastSaveLoadObservation.LastDelta = Delta;
		ObjectLastSaveLoadObservation.LastWide = Wide;
		return ObjectLastSaveLoadObservation.LastObjectValue
			+ Delta
			+ static_cast<int32>(Wide);
	}

	static int32 RegisterObjectLastSaveLoadSurface(asIScriptEngine& ScriptEngine)
	{
		const ASAutoCaller::FunctionCaller ConstructorCaller =
			ASAutoCaller::MakeFunctionCaller(ConstructObjectLastSaveLoadProbe);
		const ASAutoCaller::FunctionCaller DestructorCaller =
			ASAutoCaller::MakeFunctionCaller(DestructObjectLastSaveLoadProbe);
		const ASAutoCaller::FunctionCaller EvaluateCaller =
			ASAutoCaller::MakeFunctionCaller(EvaluateObjectLastSaveLoadProbe);
		if (ScriptEngine.RegisterObjectType(
				"ObjectLastSaveLoadProbe",
				sizeof(FObjectLastSaveLoadProbe),
				asOBJ_VALUE
					| asGetTypeTraits<FObjectLastSaveLoadProbe>()
					| asOBJ_APP_CLASS_ALLINTS) < 0
			|| ScriptEngine.RegisterObjectBehaviour(
				"ObjectLastSaveLoadProbe",
				asBEHAVE_CONSTRUCT,
				"void f(int Value)",
				asFUNCTION(ConstructObjectLastSaveLoadProbe),
				asCALL_CDECL_OBJLAST,
				*(asFunctionCaller*)&ConstructorCaller) < 0
			|| ScriptEngine.RegisterObjectBehaviour(
				"ObjectLastSaveLoadProbe",
				asBEHAVE_DESTRUCT,
				"void f()",
				asFUNCTION(DestructObjectLastSaveLoadProbe),
				asCALL_CDECL_OBJLAST,
				*(asFunctionCaller*)&DestructorCaller) < 0)
		{
			return asERROR;
		}

		return ScriptEngine.RegisterObjectMethod(
			"ObjectLastSaveLoadProbe",
			"int Evaluate(int Delta, int64 Wide) const",
			asFUNCTION(EvaluateObjectLastSaveLoadProbe),
			asCALL_CDECL_OBJLAST,
			*(asFunctionCaller*)&EvaluateCaller);
	}

	inline static FObjectLastSaveLoadObservation ObjectLastSaveLoadObservation;

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
		const bool bExecuted = LocalAssert.AreEqual(
			static_cast<int32>(asEXECUTION_FINISHED),
			ExecuteResult,
			TEXT("Reference save/load test should execute successfully"));
		const int UnprepareResult = Context->Unprepare();
		Context->Release();
		return bExecuted && LocalAssert.AreEqual(
			static_cast<int32>(asSUCCESS),
			UnprepareResult,
			TEXT("Reference save/load test should unprepare its execution context before module cleanup"));
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
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT("MOD-SAVELOAD-FUNCTION-RESTORE",
			ENativeEvidence::Compile
				| ENativeEvidence::Diagnostic
				| ENativeEvidence::Runtime
				| ENativeEvidence::Metadata
				| ENativeEvidence::Bytecode
				| ENativeEvidence::Lifecycle
				| ENativeEvidence::Cleanup
				| ENativeEvidence::Isolation);

		AngelscriptNativeTestSupport::FNativeMessageCollector Messages;
		asIScriptEngine* ScriptEngine = AngelscriptNativeTestSupport::CreateNativeEngine(&Messages);
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Reference save/load roundtrip should create a native engine")));

		ON_SCOPE_EXIT
		{
			AngelscriptNativeTestSupport::DestroyNativeEngine(ScriptEngine);
		};

		asIScriptModule* SourceModule = AngelscriptNativeTestSupport::BuildNativeModule(ScriptEngine, "ReferenceSaveLoadSource", ASTEST_AS_ANSI(R"AS(
			int Add(int A, int B)
			{
				return A + B;
			}

			int Entry()
			{
				return Add(20, 22);
			}
		)AS"));
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

		ASSERT_THAT(AreEqual(static_cast<int32>(asSUCCESS), ScriptEngine->DiscardModule("ReferenceSaveLoadSource"),
			TEXT("Reference save/load roundtrip should explicitly discard the source module")));
		ASSERT_THAT(IsNull(ScriptEngine->GetModule("ReferenceSaveLoadSource", asGM_ONLY_IF_EXISTS),
			TEXT("Reference save/load roundtrip should remove the source module before restoring")));

		bool bWasDebugInfoStripped = true;
		int LoadResult = asERROR;
		asIScriptModule* RestoredModule = LoadModuleFromStream(ScriptEngine, "ReferenceSaveLoadRestored", Stream, bWasDebugInfoStripped, LoadResult);
		ASSERT_THAT(IsNotNull(RestoredModule, TEXT("Reference save/load roundtrip should create the restored module")));
		ASSERT_THAT(AreEqual(static_cast<int32>(asSUCCESS), LoadResult, TEXT("Reference save/load roundtrip should load bytecode successfully")));
		ASSERT_THAT(IsFalse(bWasDebugInfoStripped, TEXT("Reference save/load roundtrip should preserve debug information when not stripping")));

		ASSERT_THAT(IsNotNull(AngelscriptNativeTestSupport::GetNativeFunctionByDecl(RestoredModule, "int Add(const int, const int)"), TEXT("Reference save/load roundtrip should resolve Add after deserialization")));
		ASSERT_THAT(IsNotNull(AngelscriptNativeTestSupport::GetNativeFunctionByDecl(RestoredModule, "int Entry()"), TEXT("Reference save/load roundtrip should resolve Entry after deserialization")));
		ASSERT_THAT(AreEqual(2, RestoredModule->GetFunctionCount(), TEXT("Reference save/load roundtrip should preserve the function count after deserialization")));

		int32 RestoredValue = 0;
		if (!ExecuteIntFunction(*TestRunner, ScriptEngine, RestoredModule, "int Entry()", RestoredValue))
		{
			return;
		}
		ASSERT_THAT(AreEqual(42, RestoredValue, TEXT("Reference save/load roundtrip should execute restored bytecode")));
		ASSERT_THAT(AreEqual(static_cast<int32>(asSUCCESS), ScriptEngine->DiscardModule("ReferenceSaveLoadRestored"),
			TEXT("Reference save/load roundtrip should explicitly discard the restored module")));
		ASSERT_THAT(IsNull(ScriptEngine->GetModule("ReferenceSaveLoadRestored", asGM_ONLY_IF_EXISTS),
			TEXT("Reference save/load roundtrip should remove the restored module from name lookup")));

		AngelscriptNativeTestSupport::FNativeMessageCollector IsolatedMessages;
		asIScriptEngine* IsolatedEngine = AngelscriptNativeTestSupport::CreateNativeEngine(&IsolatedMessages);
		ON_SCOPE_EXIT
		{
			AngelscriptNativeTestSupport::DestroyNativeEngine(IsolatedEngine);
		};
		ASSERT_THAT(IsNotNull(IsolatedEngine, TEXT("Reference save/load roundtrip should create an independent engine")));
		if (IsolatedEngine == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(IsolatedEngine != ScriptEngine, TEXT("Reference save/load roundtrip should isolate restored state by engine")));
		ASSERT_THAT(IsNull(IsolatedEngine->GetModule("ReferenceSaveLoadRestored", asGM_ONLY_IF_EXISTS),
			TEXT("Reference save/load roundtrip should not publish restored functions into an independent engine")));
	}

	TEST_METHOD(StripDebugInfoReportsStrippedFlag)
	{
		AS_NATIVE_PRODUCT_PART("MOD-SAVELOAD-FUNCTION-RESTORE", "stripped_debug_flag");

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
			ASTEST_AS_ANSI(R"AS(
				int Entry()
				{
					return 42;
				}
				)AS"));
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
		int32 RestoredValue = 0;
		if (!ExecuteIntFunction(*TestRunner, ScriptEngine, RestoredModule, "int Entry()", RestoredValue))
		{
			return;
		}
		ASSERT_THAT(AreEqual(42, RestoredValue, TEXT("Reference stripped save/load should execute restored bytecode")));
	}

	TEST_METHOD(TruncatedStreamFailsThenCompleteStreamStillLoads)
	{
		AS_NATIVE_PRODUCT_PART("MOD-SAVELOAD-FUNCTION-RESTORE", "truncated_then_complete_retry");

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
			ASTEST_AS_ANSI(R"AS(
				int Entry()
				{
					return 42;
				}
				)AS"));
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
		ASSERT_THAT(AreEqual(0, FailedModule->GetFunctionCount(), TEXT("Reference truncated save/load should publish no functions after failure")));
		ASSERT_THAT(AreEqual(0, FailedModule->GetGlobalVarCount(), TEXT("Reference truncated save/load should publish no globals after failure")));
		ASSERT_THAT(AreEqual(0, FailedModule->GetObjectTypeCount(), TEXT("Reference truncated save/load should publish no object types after failure")));
		ASSERT_THAT(AreEqual(0, FailedModule->GetEnumCount(), TEXT("Reference truncated save/load should publish no enums after failure")));
		ASSERT_THAT(AreEqual(0, FailedModule->GetImportedFunctionCount(), TEXT("Reference truncated save/load should publish no imports after failure")));

		ASSERT_THAT(AreEqual(static_cast<int32>(asSUCCESS), ScriptEngine->DiscardModule("ReferenceSaveLoadTruncated"),
			TEXT("Reference truncated save/load should explicitly discard the failed module before retry")));
		ASSERT_THAT(IsNull(ScriptEngine->GetModule("ReferenceSaveLoadTruncated", asGM_ONLY_IF_EXISTS),
			TEXT("Reference truncated save/load should remove the failed module before retry")));

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
		AS_NATIVE_PRODUCT_PART("MOD-SAVELOAD-FUNCTION-RESTORE", "multi_function_restore_execute");

		AngelscriptNativeTestSupport::FNativeMessageCollector Messages;
		asIScriptEngine* ScriptEngine = AngelscriptNativeTestSupport::CreateNativeEngine(&Messages);
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Reference multi-function save/load should create a native engine")));

		ON_SCOPE_EXIT
		{
			AngelscriptNativeTestSupport::DestroyNativeEngine(ScriptEngine);
		};

		asIScriptModule* SourceModule = AngelscriptNativeTestSupport::BuildNativeModule(ScriptEngine, "ReferenceSaveLoadMultiSource", ASTEST_AS_ANSI(R"AS(
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
		)AS"));
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
		ASSERT_THAT(AreEqual(3, RestoredModule->GetFunctionCount(), TEXT("Reference multi-function save/load should restore exactly three functions")));

		int32 LeftValue = 0;
		int32 RightValue = 0;
		int32 RestoredValue = 0;
		if (!ExecuteIntFunction(*TestRunner, ScriptEngine, RestoredModule, "int Left()", LeftValue)
			|| !ExecuteIntFunction(*TestRunner, ScriptEngine, RestoredModule, "int Right()", RightValue)
			|| !ExecuteIntFunction(*TestRunner, ScriptEngine, RestoredModule, "int Entry()", RestoredValue))
		{
			return;
		}
		ASSERT_THAT(AreEqual(20, LeftValue, TEXT("Reference multi-function save/load should execute restored Left")));
		ASSERT_THAT(AreEqual(22, RightValue, TEXT("Reference multi-function save/load should execute restored Right")));
		ASSERT_THAT(AreEqual(42, RestoredValue, TEXT("Reference multi-function save/load should execute after load")));
	}

	TEST_METHOD(ObjectLastNativeCallIdentitySurvivesCompatibleDestinationLoad)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT_PART(
			"MOD-SAVELOAD-FUNCTION-RESTORE",
			"object_last_native_call_identity");

		FNativeMessageCollector SourceMessages;
		FNativeMessageCollector DestinationMessages;
		asIScriptEngine* SourceEngine = CreateNativeEngine(&SourceMessages);
		asIScriptEngine* DestinationEngine = CreateNativeEngine(&DestinationMessages);
		ASSERT_THAT(IsNotNull(
			SourceEngine,
			TEXT("Object-last save/load test should create a source engine")));
		ASSERT_THAT(IsNotNull(
			DestinationEngine,
			TEXT("Object-last save/load test should create a destination engine")));
		ON_SCOPE_EXIT
		{
			DestroyNativeEngine(DestinationEngine);
			DestroyNativeEngine(SourceEngine);
		};
		if (SourceEngine == nullptr || DestinationEngine == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(
			SourceEngine != DestinationEngine,
			TEXT("Object-last save/load test should use isolated engine instances")));

		const int32 SourceNativeFunctionId =
			RegisterObjectLastSaveLoadSurface(*SourceEngine);
		const int32 DestinationNativeFunctionId =
			RegisterObjectLastSaveLoadSurface(*DestinationEngine);
		ASSERT_THAT(IsTrue(
			SourceNativeFunctionId >= 0,
			TEXT("Object-last save/load source should register its native surface")));
		ASSERT_THAT(IsTrue(
			DestinationNativeFunctionId >= 0,
			TEXT("Object-last save/load destination should register a compatible native surface")));
		if (SourceNativeFunctionId < 0 || DestinationNativeFunctionId < 0)
		{
			return;
		}

		asIScriptFunction* const SourceNativeFunction =
			SourceEngine->GetFunctionById(SourceNativeFunctionId);
		asIScriptFunction* const DestinationNativeFunction =
			DestinationEngine->GetFunctionById(DestinationNativeFunctionId);
		ASSERT_THAT(IsNotNull(
			SourceNativeFunction,
			TEXT("Object-last save/load source should publish its native function identity")));
		ASSERT_THAT(IsNotNull(
			DestinationNativeFunction,
			TEXT("Object-last save/load destination should publish its native function identity")));
		ASSERT_THAT(IsTrue(
			SourceNativeFunction != DestinationNativeFunction,
			TEXT("Compatible engines should retain distinct process objects for the same declaration")));

		const std::string Source = ASTEST_AS_ANSI(R"AS(
			int Entry()
			{
				ObjectLastSaveLoadProbe Value(31);
				return Value.Evaluate(7, int64(1000000009));
			}
			)AS");
		PrintGeneratedAsSource(
			*TestRunner,
			TEXT("MOD-SAVELOAD-FUNCTION-RESTORE-object-last-native-call-identity"),
			TEXT("ObjectLastNativeCallIdentitySource"),
			UTF8_TO_TCHAR(Source.c_str()));
		asIScriptModule* const SourceModule =
			BuildNativeModule(
				SourceEngine,
				"ObjectLastNativeCallIdentitySource",
				Source);
		ASSERT_THAT(IsNotNull(
			SourceModule,
			TEXT("Object-last save/load source should compile")));
		if (SourceModule == nullptr)
		{
			TestRunner->AddInfo(CollectMessages(SourceMessages));
			return;
		}

		ObjectLastSaveLoadObservation.Reset(SourceEngine);
		int32 SourceResult = 0;
		ASSERT_THAT(IsTrue(
			ExecuteIntFunction(
				*TestRunner,
				SourceEngine,
				SourceModule,
				"int Entry()",
				SourceResult),
			TEXT("Object-last save/load source should execute before serialization")));
		ASSERT_THAT(AreEqual(
			1000000047,
			SourceResult,
			TEXT("Object-last save/load source should preserve its sentinel result")));
		ASSERT_THAT(AreEqual(
			SourceEngine,
			ObjectLastSaveLoadObservation.ObservedEngine,
			TEXT("Object-last save/load source should dispatch through its source engine")));
		ASSERT_THAT(AreEqual(
			0,
			ObjectLastSaveLoadObservation.LiveObjects,
			TEXT("Object-last save/load source should clean every native value")));

		FMemoryBinaryStream Stream;
		ASSERT_THAT(AreEqual(
			static_cast<int32>(asSUCCESS),
			SourceModule->SaveByteCode(&Stream, false),
			TEXT("Object-last save/load source should serialize its native call")));
		ASSERT_THAT(IsTrue(
			Stream.Num() > 0,
			TEXT("Object-last save/load source should emit a non-empty stable stream")));
		ASSERT_THAT(AreEqual(
			static_cast<int32>(asSUCCESS),
			SourceEngine->DiscardModule("ObjectLastNativeCallIdentitySource"),
			TEXT("Object-last save/load source should discard before destination load")));

		bool bWasDebugInfoStripped = true;
		int32 LoadResult = asERROR;
		asIScriptModule* const DestinationModule = LoadModuleFromStream(
			DestinationEngine,
			"ObjectLastNativeCallIdentityDestination",
			Stream,
			bWasDebugInfoStripped,
			LoadResult);
		ASSERT_THAT(IsNotNull(
			DestinationModule,
			TEXT("Object-last save/load destination should create its module")));
		ASSERT_THAT(AreEqual(
			static_cast<int32>(asSUCCESS),
			LoadResult,
			TEXT("Object-last save/load destination should restore a compatible native call")));
		ASSERT_THAT(IsFalse(
			bWasDebugInfoStripped,
			TEXT("Object-last save/load destination should preserve debug information")));
		if (DestinationModule == nullptr || LoadResult != asSUCCESS)
		{
			TestRunner->AddInfo(CollectMessages(DestinationMessages));
			return;
		}

		ObjectLastSaveLoadObservation.Reset(DestinationEngine);
		int32 DestinationResult = 0;
		ASSERT_THAT(IsTrue(
			ExecuteIntFunction(
				*TestRunner,
				DestinationEngine,
				DestinationModule,
				"int Entry()",
				DestinationResult),
			TEXT("Object-last save/load destination should execute restored bytecode")));
		ASSERT_THAT(AreEqual(
			SourceResult,
			DestinationResult,
			TEXT("Object-last save/load destination should preserve exact native behavior")));
		ASSERT_THAT(AreEqual(
			DestinationEngine,
			ObjectLastSaveLoadObservation.ObservedEngine,
			TEXT("Restored native call should resolve through the destination engine identity")));
		ASSERT_THAT(AreEqual(
			31,
			ObjectLastSaveLoadObservation.LastObjectValue,
			TEXT("Restored object-last call should preserve the object sentinel")));
		ASSERT_THAT(AreEqual(
			7,
			ObjectLastSaveLoadObservation.LastDelta,
			TEXT("Restored object-last call should preserve its scalar sentinel")));
		ASSERT_THAT(AreEqual(
			static_cast<int64>(1000000009),
			ObjectLastSaveLoadObservation.LastWide,
			TEXT("Restored object-last call should preserve its wide sentinel")));
		ASSERT_THAT(AreEqual(
			1,
			ObjectLastSaveLoadObservation.CallCount,
			TEXT("Restored object-last call should dispatch exactly once")));
		ASSERT_THAT(AreEqual(
			ObjectLastSaveLoadObservation.ConstructorCount,
			ObjectLastSaveLoadObservation.DestructorCount,
			TEXT("Restored object-last call should clean its native value exactly once")));
		ASSERT_THAT(AreEqual(
			0,
			ObjectLastSaveLoadObservation.LiveObjects,
			TEXT("Restored object-last call should leave no live native values")));
		ASSERT_THAT(AreEqual(
			static_cast<int32>(asSUCCESS),
			DestinationEngine->DiscardModule("ObjectLastNativeCallIdentityDestination"),
			TEXT("Object-last save/load destination should explicitly discard its module")));
		ASSERT_THAT(IsNull(
			DestinationEngine->GetModule(
				"ObjectLastNativeCallIdentityDestination",
				asGM_ONLY_IF_EXISTS),
			TEXT("Object-last save/load destination module should be absent after cleanup")));
	}
};

#endif
