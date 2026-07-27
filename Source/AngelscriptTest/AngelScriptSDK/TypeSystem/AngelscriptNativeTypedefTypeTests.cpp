#include "Support/AngelscriptNativeExecutionTestSupport.h"
#include "Support/AngelscriptNativeCaseTestSupport.h"
#include "Support/AngelscriptNativeLanguageCaseTestSupport.h"

#include "AngelscriptTestMacros.h"

#include "CQTest.h"
#include "Misc/ScopeExit.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FTypedefTypeTests,
	"Angelscript.TestModule.AngelScriptSDK.TypeSystem.Typedefs",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	inline static constexpr asPWORD EngineCleanupSlot = 0x54595044;
	inline static TArray<void*> CleanedEngineData;

	static void ObserveEngineCleanup(asIScriptEngine* Engine)
	{
		CleanedEngineData.Add(
			Engine != nullptr
				? Engine->GetUserData(EngineCleanupSlot)
				: nullptr);
	}

public:
	TEST_METHOD(TypedefTypeTypedefBytecode)
	{
		using namespace AngelscriptNativeTestSupport;
		using namespace AngelscriptSDKTestSupport;

		AS_NATIVE_PRODUCT("TYPE-TYPEDEF-BYTECODE-RUNTIME",
			ENativeEvidence::Compile
				| ENativeEvidence::Bytecode
				| ENativeEvidence::Metadata
				| ENativeEvidence::Runtime
				| ENativeEvidence::SaveLoad
				| ENativeEvidence::Cleanup
				| ENativeEvidence::Isolation);

		CleanedEngineData.Reset();
		ON_SCOPE_EXIT
		{
			CleanedEngineData.Reset();
		};

		FNativeMessageCollector SaveMessages;
		asIScriptEngine* SaveEngine = CreateNativeEngine(&SaveMessages);
		ASSERT_THAT(IsNotNull(SaveEngine, TEXT("Typedef bytecode test should create a save engine")));
		if (SaveEngine == nullptr)
		{
			return;
		}
		ON_SCOPE_EXIT
		{
			if (SaveEngine != nullptr)
			{
				DestroyNativeEngine(SaveEngine);
			}
		};
		int32 SaveCleanupData = 11;
		SaveEngine->SetEngineUserDataCleanupCallback(
			ObserveEngineCleanup,
			EngineCleanupSlot);
		ASSERT_THAT(IsNull(
			SaveEngine->SetUserData(
				&SaveCleanupData,
				EngineCleanupSlot),
			TEXT("Typedef save-engine cleanup slot should begin empty")));

		ASSERT_THAT(IsTrue(SaveEngine->RegisterTypedef("TestType1", "int8") >= 0, TEXT("Typedef bytecode test should register TestType1 on the save engine")));
		ASSERT_THAT(IsTrue(SaveEngine->RegisterTypedef("TestType4", "int64") >= 0, TEXT("Typedef bytecode test should register TestType4 on the save engine")));
		const FString Source = ASTEST_AS(R"AS(
			TestType4 Func(TestType1 value)
			{
				return value;
			}

			int ReturnTypedefRoundTrip()
			{
				TestType1 value = 1;
				return int(Func(value));
			}
			)AS");
		const TCHAR* Phases[] =
		{
			TEXT("save-registration"),
			TEXT("save-bytecode"),
			TEXT("load-bytecode"),
			TEXT("loaded-runtime"),
		};
		for (const TCHAR* Phase : Phases)
		{
			PrintGeneratedAsSource(
				*TestRunner,
				MakeNativeCaseId(
					"TYPE-TYPEDEF-BYTECODE-RUNTIME",
					{ Phase }),
				TEXT("TypeTypedefBytecodeRuntime"),
				Source);
		}
		const FTCHARToUTF8 SourceUtf8(*Source);
		asIScriptModule* const SaveModule =
			BuildNativeModule(
				SaveEngine,
				"TypedefTypeSave",
				SourceUtf8.Get());
		ASSERT_THAT(IsNotNull(SaveModule, TEXT("Typedef bytecode test should compile the save module")));
		if (SaveModule == nullptr)
		{
			TestRunner->AddInfo(CollectMessages(SaveMessages));
			return;
		}

		FSDKBytecodeStream Bytecode;
		ASSERT_THAT(AreEqual(asSUCCESS, SaveModule->SaveByteCode(&Bytecode), TEXT("Typedef bytecode test should save bytecode")));
		const TArray<asIScriptFunction*> SavedFunctions =
			FindNativeFunctionsByName(SaveModule, "Func");
		ASSERT_THAT(AreEqual(
			1,
			SavedFunctions.Num(),
			TEXT("Save module should publish exactly one typedef-backed Func")));
		asIScriptFunction* const SavedFunction =
			SavedFunctions.Num() == 1 ? SavedFunctions[0] : nullptr;
		FString SavedFunctionDeclaration;
		if (SavedFunction != nullptr)
		{
			SavedFunctionDeclaration =
				UTF8_TO_TCHAR(SavedFunction->GetDeclaration());
			ASSERT_THAT(AreEqual(
				static_cast<int32>(1),
				static_cast<int32>(SavedFunction->GetParamCount()),
				TEXT("Typedef function should preserve one parameter")));
			ASSERT_THAT(AreEqual(
				SaveEngine->GetTypeIdByDecl("TestType4"),
				SavedFunction->GetReturnTypeId(),
				TEXT("Typedef function should preserve the registered int64 alias type ID")));
			int ParameterTypeId = asINVALID_TYPE;
			ASSERT_THAT(AreEqual(
				asSUCCESS,
				SavedFunction->GetParam(0, &ParameterTypeId),
				TEXT("Typedef function should expose its first parameter metadata")));
			ASSERT_THAT(AreEqual(
				SaveEngine->GetTypeIdByDecl("TestType1"),
				ParameterTypeId,
				TEXT("Typedef function should preserve the registered int8 alias type ID")));
			ASSERT_THAT(AreEqual(
				FString(TEXT("int64 Func(const int8)")),
				FString(UTF8_TO_TCHAR(SavedFunction->GetDeclaration())),
				TEXT("Current fork should normalize typedef aliases and mark the by-value primitive parameter const")));
		}
		ASSERT_THAT(IsTrue(
			Bytecode.Num() > 0,
			TEXT("Typedef bytecode save should produce a non-empty independent stream")));
		ASSERT_THAT(AreEqual(
			asSUCCESS,
			SaveEngine->DiscardModule("TypedefTypeSave"),
			TEXT("Typedef bytecode test should discard the save module before loading independently")));
		ASSERT_THAT(IsNull(
			SaveEngine->GetModule(
				"TypedefTypeSave",
				asGM_ONLY_IF_EXISTS),
			TEXT("Typedef save module should be absent after explicit cleanup")));
		Bytecode.ResetReadPosition();

		FNativeMessageCollector LoadMessages;
		asIScriptEngine* LoadEngine = CreateNativeEngine(&LoadMessages);
		ASSERT_THAT(IsNotNull(LoadEngine, TEXT("Typedef bytecode test should create a load engine")));
		if (LoadEngine == nullptr)
		{
			return;
		}
		ON_SCOPE_EXIT
		{
			if (LoadEngine != nullptr)
			{
				DestroyNativeEngine(LoadEngine);
			}
		};
		int32 LoadCleanupData = 22;
		LoadEngine->SetEngineUserDataCleanupCallback(
			ObserveEngineCleanup,
			EngineCleanupSlot);
		ASSERT_THAT(IsNull(
			LoadEngine->SetUserData(
				&LoadCleanupData,
				EngineCleanupSlot),
			TEXT("Typedef load-engine cleanup slot should begin empty")));

		ASSERT_THAT(IsTrue(LoadEngine->RegisterTypedef("TestType1", "int8") >= 0, TEXT("Typedef bytecode test should register TestType1 on the load engine")));
		ASSERT_THAT(IsTrue(LoadEngine->RegisterTypedef("TestType4", "int64") >= 0, TEXT("Typedef bytecode test should register TestType4 on the load engine")));
		asIScriptModule* const LoadModule = LoadEngine->GetModule("TypedefTypeLoad", asGM_ALWAYS_CREATE);
		ASSERT_THAT(IsNotNull(LoadModule, TEXT("Typedef bytecode test should create a load module")));
		if (LoadModule == nullptr)
		{
			return;
		}

		ASSERT_THAT(AreEqual(asSUCCESS, LoadModule->LoadByteCode(&Bytecode), TEXT("Typedef bytecode test should load bytecode")));
		ASSERT_THAT(IsNotNull(GetNativeFunctionByExactDecl(LoadModule, "int ReturnTypedefRoundTrip()"),
			TEXT("Typedef bytecode test should preserve the loaded entry function")));
		const TArray<asIScriptFunction*> LoadedFunctions =
			FindNativeFunctionsByName(LoadModule, "Func");
		ASSERT_THAT(AreEqual(
			1,
			LoadedFunctions.Num(),
			TEXT("Loaded bytecode should publish exactly one typedef-backed Func")));
		asIScriptFunction* const LoadedTypedefFunction =
			LoadedFunctions.Num() == 1 ? LoadedFunctions[0] : nullptr;
		if (!SavedFunctionDeclaration.IsEmpty() && LoadedTypedefFunction != nullptr)
		{
			ASSERT_THAT(AreEqual(
				SavedFunctionDeclaration,
				FString(UTF8_TO_TCHAR(LoadedTypedefFunction->GetDeclaration())),
				TEXT("Bytecode load should preserve the canonical typedef declaration")));
		}

		{
			FSdkFunctionInvoker Invoker(
				*TestRunner,
				LoadEngine,
				LoadModule,
				"int ReturnTypedefRoundTrip()");
			ASSERT_THAT(IsTrue(
				Invoker.IsValid(),
				TEXT("Loaded typedef bytecode should prepare its exact runtime entry")));
			if (Invoker.IsValid())
			{
				ASSERT_THAT(AreEqual(
					1,
					Invoker.CallAndReturn<int32>(INDEX_NONE),
					TEXT("Loaded typedef bytecode should execute the int8-to-int64 round trip")));
			}
		}
		ASSERT_THAT(IsFalse(
			Bytecode.HasReadError(),
			TEXT("Typedef bytecode load should consume the complete stream without an ownership/read error")));
		ASSERT_THAT(AreEqual(
			asSUCCESS,
			LoadEngine->DiscardModule("TypedefTypeLoad"),
			TEXT("Typedef bytecode test should explicitly discard the loaded module")));
		ASSERT_THAT(IsNull(
			LoadEngine->GetModule(
				"TypedefTypeLoad",
				asGM_ONLY_IF_EXISTS),
			TEXT("Typedef load module should be absent after cleanup")));

		Bytecode.Truncate(0);
		ASSERT_THAT(AreEqual(
			0,
			Bytecode.Num(),
			TEXT("Typedef bytecode stream should return to an explicit empty baseline")));
		ASSERT_THAT(AreEqual(
			0,
			CleanedEngineData.Num(),
			TEXT("Typedef engine cleanup callbacks should remain deferred until explicit destruction")));

		DestroyNativeEngine(LoadEngine);
		LoadEngine = nullptr;
		ASSERT_THAT(AreEqual(
			1,
			CleanedEngineData.Num(),
			TEXT("Destroying the load engine should dispatch exactly one cleanup callback")));
		ASSERT_THAT(IsTrue(
			CleanedEngineData.Contains(&LoadCleanupData),
			TEXT("Load-engine cleanup should observe its exact sentinel")));

		DestroyNativeEngine(SaveEngine);
		SaveEngine = nullptr;
		ASSERT_THAT(AreEqual(
			2,
			CleanedEngineData.Num(),
			TEXT("Destroying both typedef engines should dispatch exactly two cleanup callbacks")));
		ASSERT_THAT(IsTrue(
			CleanedEngineData.Contains(&SaveCleanupData),
			TEXT("Save-engine cleanup should observe its exact sentinel")));
	}
};

#endif
