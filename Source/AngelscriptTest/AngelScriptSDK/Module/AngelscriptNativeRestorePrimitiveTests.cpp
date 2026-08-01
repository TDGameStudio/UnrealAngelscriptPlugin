#include "../Support/AngelscriptNativeCoreTestSupport.h"
#include "AngelscriptTestMacros.h"
#include "../Support/AngelscriptNativeFixtureTestSupport.h"
#include "../Support/AngelscriptNativeLanguageCaseTestSupport.h"

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
		const std::string Source = ASTEST_AS_ANSI(R"AS(
			const int GlobalValue = 41;

			int Test()
			{
				return GlobalValue + 1;
			}
			)AS");
		AngelscriptNativeTestSupport::PrintGeneratedAsSource(
			Test,
			FString::Printf(TEXT("MOD-BYTECODE-STREAM-RESTORE-%hs"), ModuleName),
			FString(UTF8_TO_TCHAR(ModuleName)),
			FString(UTF8_TO_TCHAR(Source.c_str())));
		asIScriptModule* Module = AngelscriptNativeTestSupport::BuildNativeModule(
			Engine.Get(), ModuleName, Source);
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
		ON_SCOPE_EXIT
		{
			Context->Release();
		};

		if (AngelscriptNativeTestSupport::PrepareAndExecute(Context, Function) != asEXECUTION_FINISHED)
		{
			Test.AddError(TEXT("Restore test should finish executing the restored function"));
			return false;
		}

		OutValue = static_cast<int32>(Context->GetReturnDWord());
		if (Context->Unprepare() != asSUCCESS)
		{
			Test.AddError(TEXT("Restore test should unprepare the execution context before module cleanup"));
			return false;
		}
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

	static bool ContainsBytecodeOpcode(asIScriptFunction& Function, const asEBCInstr ExpectedOpcode)
	{
		asUINT BytecodeLength = 0;
		const asDWORD* const Bytecode = Function.GetByteCode(&BytecodeLength);
		if (Bytecode == nullptr || BytecodeLength == 0)
		{
			return false;
		}

		for (asUINT DwordIndex = 0; DwordIndex < BytecodeLength;)
		{
			const asEBCInstr Opcode =
				static_cast<asEBCInstr>(*reinterpret_cast<const asBYTE*>(&Bytecode[DwordIndex]));
			if (Opcode == ExpectedOpcode)
			{
				return true;
			}
			if (static_cast<int32>(Opcode) > static_cast<int32>(asBC_MAXBYTECODE))
			{
				return false;
			}

			const int32 InstructionSize = asBCTypeSize[asBCInfo[Opcode].type];
			if (InstructionSize <= 0
				|| DwordIndex + static_cast<asUINT>(InstructionSize) > BytecodeLength)
			{
				return false;
			}
			DwordIndex += static_cast<asUINT>(InstructionSize);
		}

		return false;
	}

public:
	TEST_METHOD(RestorePrimitiveRoundTrip)
	{
		AS_NATIVE_PRODUCT_PART("MOD-BYTECODE-STREAM-RESTORE", "primitive_round_trip");

		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};
		asCScriptEngine* ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Restore roundtrip should create a raw SDK engine")));
		if (ScriptEngine == nullptr)
		{
			return;
		}

		asCModule* SourceModule = BuildRestoreModule(*TestRunner, Engine, "RestoreRoundTrip");
		ASSERT_THAT(IsNotNull(SourceModule, TEXT("Restore roundtrip should build the source module")));
		if (SourceModule == nullptr)
		{
			return;
		}

		int32 SourceValue = 0;
		if (!ExecuteRestoreFunction(*TestRunner, *ScriptEngine, *SourceModule, SourceValue))
		{
			return;
		}
		ASSERT_THAT(AreEqual(42, SourceValue, TEXT("Restore roundtrip should execute before serialization")));

		AngelscriptNativeTestSupport::FMemoryBinaryStream Stream;
		ASSERT_THAT(AreEqual(static_cast<int32>(asSUCCESS), SourceModule->SaveByteCode(&Stream, false),
			TEXT("Restore roundtrip should save bytecode successfully")));
		ASSERT_THAT(IsTrue(Stream.Num() > 0, TEXT("Restore roundtrip should produce bytecode bytes")));

		ASSERT_THAT(AreEqual(static_cast<int32>(asSUCCESS), ScriptEngine->DiscardModule("RestoreRoundTrip"),
			TEXT("Restore roundtrip should explicitly discard the source module")));
		ASSERT_THAT(IsNull(ScriptEngine->GetModule("RestoreRoundTrip", asGM_ONLY_IF_EXISTS),
			TEXT("Restore roundtrip should remove the source module before loading")));
		Stream.ResetReadPosition();
		asPWORD PreviousInit = 0;
		DisableAutomaticGlobalInitialization(*ScriptEngine, PreviousInit);
		ON_SCOPE_EXIT
		{
			ScriptEngine->SetEngineProperty(asEP_INIT_GLOBAL_VARS_AFTER_BUILD, PreviousInit);
		};
		asCModule* RestoredModule = CreateDestinationModule(*ScriptEngine, "RestoreRoundTrip");
		ASSERT_THAT(IsNotNull(RestoredModule, TEXT("Restore roundtrip should create a destination module")));
		if (RestoredModule == nullptr)
		{
			return;
		}
		bool bWasDebugInfoStripped = true;
		ASSERT_THAT(AreEqual(static_cast<int32>(asSUCCESS), RestoredModule->LoadByteCode(&Stream, &bWasDebugInfoStripped),
			TEXT("Restore roundtrip should load bytecode successfully")));
		ASSERT_THAT(IsFalse(bWasDebugInfoStripped, TEXT("Restore roundtrip should retain debug information")));
		ASSERT_THAT(AreEqual(1, static_cast<int32>(RestoredModule->GetFunctionCount()),
			TEXT("Restore roundtrip should publish exactly one restored function")));
		ASSERT_THAT(AreEqual(1, static_cast<int32>(RestoredModule->GetGlobalVarCount()),
			TEXT("Restore roundtrip should publish exactly one restored global")));
		ASSERT_THAT(IsNotNull(RestoredModule->GetFunctionByDecl("int Test()"),
			TEXT("Restore roundtrip should restore the exact function declaration")));
		ASSERT_THAT(AreEqual(static_cast<int32>(asSUCCESS), RestoredModule->ResetGlobalVars(nullptr),
			TEXT("Restore roundtrip should initialize restored globals")));
		int32 RestoredValue = 0;
		if (!ExecuteRestoreFunction(*TestRunner, *ScriptEngine, *RestoredModule, RestoredValue))
		{
			return;
		}
		ASSERT_THAT(AreEqual(42, RestoredValue, TEXT("Restore roundtrip should execute restored bytecode")));
		ASSERT_THAT(AreEqual(static_cast<int32>(asSUCCESS), ScriptEngine->DiscardModule("RestoreRoundTrip"),
			TEXT("Restore roundtrip should explicitly discard the restored module")));
		ASSERT_THAT(IsNull(ScriptEngine->GetModule("RestoreRoundTrip", asGM_ONLY_IF_EXISTS),
			TEXT("Restore roundtrip should remove restored state from name lookup")));
	}

	TEST_METHOD(LegacyVersionIsRejected)
	{
		AS_NATIVE_PRODUCT_PART("MOD-BYTECODE-STREAM-RESTORE", "legacy_version_one_rejected");

		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};
		asCScriptEngine* ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Legacy bytecode test should create a raw SDK engine")));
		if (ScriptEngine == nullptr)
		{
			return;
		}

		asCModule* SourceModule = BuildRestoreModule(*TestRunner, Engine, "RestoreLegacyVersion");
		ASSERT_THAT(IsNotNull(SourceModule, TEXT("Legacy bytecode test should build a source module")));
		if (SourceModule == nullptr)
		{
			return;
		}

		AngelscriptNativeTestSupport::FMemoryBinaryStream Stream;
		ASSERT_THAT(AreEqual(static_cast<int32>(asSUCCESS), SourceModule->SaveByteCode(&Stream, false),
			TEXT("Legacy bytecode test should save a current-version stream")));
		const TArray<asBYTE>& CurrentBytes = Stream.GetBytes();
		ASSERT_THAT(IsTrue(CurrentBytes.Num() >= 2,
			TEXT("Legacy bytecode test requires a framed stream header")));
		if (CurrentBytes.Num() < 2)
		{
			return;
		}
		ASSERT_THAT(AreEqual(2, static_cast<int32>(CurrentBytes[1]),
			TEXT("Current bytecode writer should publish stream version two")));

		TArray<asBYTE> LegacyBytes = CurrentBytes;
		LegacyBytes[1] = 1;
		AngelscriptNativeTestSupport::FMemoryBinaryStream LegacyStream;
		ASSERT_THAT(AreEqual(
			static_cast<int32>(asSUCCESS),
			LegacyStream.Write(LegacyBytes.GetData(), static_cast<asUINT>(LegacyBytes.Num())),
			TEXT("Legacy bytecode test should construct a version-one payload")));
		LegacyStream.ResetReadPosition();

		ASSERT_THAT(AreEqual(static_cast<int32>(asSUCCESS), ScriptEngine->DiscardModule("RestoreLegacyVersion"),
			TEXT("Legacy bytecode test should explicitly discard the source module")));
		asCModule* const DestinationModule = CreateDestinationModule(*ScriptEngine, "RestoreLegacyVersion");
		ASSERT_THAT(IsNotNull(DestinationModule,
			TEXT("Legacy bytecode test should create a destination module")));
		if (DestinationModule == nullptr)
		{
			return;
		}
		bool bWasDebugInfoStripped = false;
		ASSERT_THAT(AreNotEqual(
			static_cast<int32>(asSUCCESS),
			DestinationModule->LoadByteCode(&LegacyStream, &bWasDebugInfoStripped),
			TEXT("Reader must reject version-one bytecode that may encode live type addresses")));
		ASSERT_THAT(AreEqual(0, static_cast<int32>(DestinationModule->GetFunctionCount()),
			TEXT("Rejected legacy stream should leave no functions")));
		ASSERT_THAT(AreEqual(0, static_cast<int32>(DestinationModule->GetGlobalVarCount()),
			TEXT("Rejected legacy stream should leave no globals")));
		ASSERT_THAT(AreEqual(0, static_cast<int32>(DestinationModule->GetObjectTypeCount()),
			TEXT("Rejected legacy stream should leave no object types")));
		ASSERT_THAT(AreEqual(0, static_cast<int32>(DestinationModule->GetEnumCount()),
			TEXT("Rejected legacy stream should leave no enums")));
		ASSERT_THAT(AreEqual(0, static_cast<int32>(DestinationModule->GetTypedefCount()),
			TEXT("Rejected legacy stream should leave no typedefs")));
		ASSERT_THAT(AreEqual(0, static_cast<int32>(DestinationModule->GetImportedFunctionCount()),
			TEXT("Rejected legacy stream should leave no imports")));
		ASSERT_THAT(AreEqual(static_cast<int32>(asSUCCESS), ScriptEngine->DiscardModule("RestoreLegacyVersion"),
			TEXT("Legacy bytecode test should explicitly discard the rejected destination")));
		ASSERT_THAT(IsNull(ScriptEngine->GetModule("RestoreLegacyVersion", asGM_ONLY_IF_EXISTS),
			TEXT("Legacy bytecode test should leave no module publication after cleanup")));
	}

	TEST_METHOD(CopyScriptSaveLoadRoundTripIsDeterministic)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT("MOD-BYTECODE-STREAM-RESTORE",
			ENativeEvidence::Compile
			| ENativeEvidence::Diagnostic
			| ENativeEvidence::Runtime
			| ENativeEvidence::Metadata
			| ENativeEvidence::Bytecode
			| ENativeEvidence::Lifecycle
			| ENativeEvidence::Cleanup
			| ENativeEvidence::Isolation);

		const std::string Source = ASTEST_AS_ANSI(R"AS(
			struct FCopyPayload
			{
				FNativeCaseValue Value;
			}

			int CopyScriptEntry()
			{
				FCopyPayload Original = FCopyPayload();
				Original.Value.Value = 73;
				FCopyPayload Copied = Original;
				return Copied.Value.Value;
			}
			)AS");
		const char* const ModuleName = "RestoreCopyScript";
		AngelscriptNativeTestSupport::PrintGeneratedAsSource(
			*TestRunner,
			TEXT("MOD-BYTECODE-STREAM-RESTORE-copy-script"),
			TEXT("RestoreCopyScript"),
			FString(UTF8_TO_TCHAR(Source.c_str())));

		AngelscriptNativeTestSupport::FNativeTestEngine SourceEngine;
		AngelscriptNativeTestSupport::FNativeTestEngine ComparisonEngine;
		AngelscriptNativeTestSupport::FNativeLifecycleRecorder SourceRecorder;
		AngelscriptNativeTestSupport::FNativeLifecycleRecorder ComparisonRecorder;
		ON_SCOPE_EXIT
		{
			ComparisonEngine.Destroy();
			SourceEngine.Destroy();
		};
		SourceEngine.Create(*TestRunner);
		asCScriptEngine* const SourceScriptEngine = static_cast<asCScriptEngine*>(SourceEngine.Get());
		ASSERT_THAT(IsNotNull(SourceScriptEngine,
			TEXT("CopyScript restore test should create a source engine")));
		if (SourceScriptEngine == nullptr)
		{
			return;
		}
		const bool bSourceTypeRegistered = AngelscriptNativeTestSupport::RegisterNativeCaseValue(
			*SourceScriptEngine,
			SourceRecorder);
		ASSERT_THAT(IsTrue(bSourceTypeRegistered,
			TEXT("CopyScript restore test should register its non-POD native field type in the source engine")));
		if (!bSourceTypeRegistered)
		{
			return;
		}

		asIScriptModule* const SourceModule =
			AngelscriptNativeTestSupport::BuildNativeModule(SourceScriptEngine, ModuleName, Source);
		ASSERT_THAT(IsNotNull(SourceModule,
			TEXT("CopyScript restore test should compile the generated source")));
		if (SourceModule == nullptr)
		{
			TestRunner->AddError(SourceEngine.GetMessagesText());
			return;
		}
		asIScriptFunction* const SourceEntry = SourceModule->GetFunctionByDecl("int CopyScriptEntry()");
		ASSERT_THAT(IsNotNull(SourceEntry,
			TEXT("CopyScript restore test should resolve its exact entry")));
		if (SourceEntry == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(ContainsBytecodeOpcode(*SourceEntry, asBC_CopyScript),
			TEXT("CopyScript restore test must compile the CopyScript bytecode opcode")));
		asIScriptContext* SourceContext = SourceScriptEngine->CreateContext();
		ASSERT_THAT(IsNotNull(SourceContext,
			TEXT("CopyScript restore test should create an explicit execution context")));
		if (SourceContext == nullptr)
		{
			return;
		}
		const int PrepareResult = SourceContext->Prepare(SourceEntry);
		ASSERT_THAT(AreEqual(static_cast<int32>(asSUCCESS), static_cast<int32>(PrepareResult),
			TEXT("CopyScript restore test should prepare its exact entry function")));
		if (PrepareResult != asSUCCESS)
		{
			SourceContext->Release();
			return;
		}
		const int ExecuteResult = SourceContext->Execute();
		ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_FINISHED), static_cast<int32>(ExecuteResult),
			TEXT("CopyScript restore test should execute the non-POD copy before serialization")));
		if (ExecuteResult != asEXECUTION_FINISHED)
		{
			SourceContext->Unprepare();
			SourceContext->Release();
			return;
		}
		ASSERT_THAT(AreEqual(73, static_cast<int32>(SourceContext->GetReturnDWord()),
			TEXT("CopyScript restore test should preserve the copied native payload value")));
		ASSERT_THAT(AreEqual(static_cast<int32>(asSUCCESS), static_cast<int32>(SourceContext->Unprepare()),
			TEXT("CopyScript restore test should unprepare its context before module cleanup")));
		SourceContext->Release();
		ASSERT_THAT(AreEqual(0, SourceRecorder.GetLiveObjectCount(),
			TEXT("CopyScript restore test should destroy all temporary native payloads after execution")));

		AngelscriptNativeTestSupport::FMemoryBinaryStream Stream;
		ASSERT_THAT(AreEqual(static_cast<int32>(asSUCCESS), SourceModule->SaveByteCode(&Stream, false),
			TEXT("CopyScript restore test should save bytecode")));

		AngelscriptNativeTestSupport::PrintGeneratedAsSource(
			*TestRunner,
			TEXT("MOD-BYTECODE-STREAM-RESTORE-copy-script-comparison"),
			TEXT("RestoreCopyScript"),
			FString(UTF8_TO_TCHAR(Source.c_str())));
		ComparisonEngine.Create(*TestRunner);
		asCScriptEngine* const ComparisonScriptEngine =
			static_cast<asCScriptEngine*>(ComparisonEngine.Get());
		ASSERT_THAT(IsNotNull(ComparisonScriptEngine,
			TEXT("CopyScript save comparison should create a second raw engine")));
		if (ComparisonScriptEngine == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(ComparisonScriptEngine != SourceScriptEngine,
			TEXT("CopyScript save comparison should use an isolated engine instance")));
		const bool bComparisonTypeRegistered = AngelscriptNativeTestSupport::RegisterNativeCaseValue(
			*ComparisonScriptEngine,
			ComparisonRecorder);
		ASSERT_THAT(IsTrue(bComparisonTypeRegistered,
			TEXT("CopyScript save comparison should register its non-POD native field type")));
		if (!bComparisonTypeRegistered)
		{
			return;
		}
		asIScriptModule* const ComparisonModule =
			AngelscriptNativeTestSupport::BuildNativeModule(ComparisonScriptEngine, ModuleName, Source);
		ASSERT_THAT(IsNotNull(ComparisonModule,
			TEXT("CopyScript save comparison should compile the identical source")));
		if (ComparisonModule == nullptr)
		{
			TestRunner->AddError(ComparisonEngine.GetMessagesText());
			return;
		}
		AngelscriptNativeTestSupport::FMemoryBinaryStream ComparisonStream;
		ASSERT_THAT(AreEqual(static_cast<int32>(asSUCCESS), ComparisonModule->SaveByteCode(&ComparisonStream, false),
			TEXT("CopyScript save comparison should save the identical module")));
		ASSERT_THAT(IsTrue(Stream.GetBytes() == ComparisonStream.GetBytes(),
			TEXT("CopyScript bytecode must be deterministic across concurrent raw engines")));
		ASSERT_THAT(AreEqual(static_cast<int32>(asSUCCESS), ComparisonScriptEngine->DiscardModule(ModuleName),
			TEXT("CopyScript save comparison should explicitly discard its module")));
		ASSERT_THAT(IsNull(ComparisonScriptEngine->GetModule(ModuleName, asGM_ONLY_IF_EXISTS),
			TEXT("CopyScript save comparison should remove its module without affecting the source engine")));
		ASSERT_THAT(IsNotNull(SourceScriptEngine->GetModule(ModuleName, asGM_ONLY_IF_EXISTS),
			TEXT("CopyScript save comparison cleanup should not remove the source engine module")));

		ASSERT_THAT(AreEqual(static_cast<int32>(asSUCCESS), SourceScriptEngine->DiscardModule(ModuleName),
			TEXT("CopyScript restore test should explicitly discard the source module")));
		ASSERT_THAT(IsNull(SourceScriptEngine->GetModule(ModuleName, asGM_ONLY_IF_EXISTS),
			TEXT("CopyScript restore test should remove the source module before loading")));
		Stream.ResetReadPosition();
		asCModule* const DestinationModule = CreateDestinationModule(*SourceScriptEngine, ModuleName);
		ASSERT_THAT(IsNotNull(DestinationModule,
			TEXT("CopyScript restore test should create a replacement module")));
		if (DestinationModule == nullptr)
		{
			return;
		}
		bool bWasDebugInfoStripped = true;
		SourceEngine.ResetMessages();
		const int LoadResult = DestinationModule->LoadByteCode(&Stream, &bWasDebugInfoStripped);
		if (LoadResult != asSUCCESS)
		{
			TestRunner->AddInfo(FString::Printf(
				TEXT("[AS-COPY-SCRIPT-LOAD] Result=%d Diagnostics=\n%s"),
				LoadResult,
				*SourceEngine.GetMessagesText()));
		}
		ASSERT_THAT(AreEqual(static_cast<int32>(asSUCCESS), LoadResult,
			TEXT("Current fork should restore nested non-POD CopyScript streams without serializing value-type factories")));
		ASSERT_THAT(IsFalse(bWasDebugInfoStripped,
			TEXT("CopyScript round trip should preserve the requested debug information")));
		asIScriptFunction* const RestoredEntry =
			DestinationModule->GetFunctionByDecl("int CopyScriptEntry()");
		ASSERT_THAT(IsNotNull(RestoredEntry,
			TEXT("CopyScript round trip should restore its exact entry")));
		if (RestoredEntry == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(ContainsBytecodeOpcode(*RestoredEntry, asBC_CopyScript),
			TEXT("CopyScript round trip should preserve the CopyScript opcode")));
		asIScriptContext* DestinationContext =
			SourceScriptEngine->CreateContext();
		ASSERT_THAT(IsNotNull(DestinationContext,
			TEXT("CopyScript round trip should create a restored execution context")));
		if (DestinationContext == nullptr)
		{
			return;
		}
		const int RestoredPrepareResult =
			DestinationContext->Prepare(RestoredEntry);
		ASSERT_THAT(AreEqual(
			static_cast<int32>(asSUCCESS),
			static_cast<int32>(RestoredPrepareResult),
			TEXT("CopyScript round trip should prepare the restored entry")));
		if (RestoredPrepareResult == asSUCCESS)
		{
			const int RestoredExecuteResult =
				DestinationContext->Execute();
			ASSERT_THAT(AreEqual(
				static_cast<int32>(asEXECUTION_FINISHED),
				static_cast<int32>(RestoredExecuteResult),
				TEXT("CopyScript round trip should execute the restored non-POD copy")));
			if (RestoredExecuteResult == asEXECUTION_FINISHED)
			{
				ASSERT_THAT(AreEqual(
					73,
					static_cast<int32>(
						DestinationContext->GetReturnDWord()),
					TEXT("CopyScript round trip should preserve the copied payload value")));
			}
			ASSERT_THAT(AreEqual(
				static_cast<int32>(asSUCCESS),
				static_cast<int32>(DestinationContext->Unprepare()),
				TEXT("CopyScript round trip should unprepare the restored context")));
		}
		DestinationContext->Release();
		ASSERT_THAT(AreEqual(0, SourceRecorder.GetLiveObjectCount(),
			TEXT("CopyScript restored execution should destroy all temporary native payloads")));
		ASSERT_THAT(AreEqual(static_cast<int32>(asSUCCESS), SourceScriptEngine->DiscardModule(ModuleName),
			TEXT("CopyScript restore test should explicitly discard the restored destination")));
		ASSERT_THAT(IsNull(SourceScriptEngine->GetModule(ModuleName, asGM_ONLY_IF_EXISTS),
			TEXT("CopyScript restore test should leave no source-engine module after cleanup")));
		ASSERT_THAT(AreEqual(0, SourceRecorder.GetLiveObjectCount(),
			TEXT("CopyScript restore cleanup should leave no live source-engine native payloads")));
		ASSERT_THAT(AreEqual(0, ComparisonRecorder.GetLiveObjectCount(),
			TEXT("CopyScript restore cleanup should leave no live comparison-engine native payloads")));
	}

	TEST_METHOD(StripDebugInfoRoundTrip)
	{
		AS_NATIVE_PRODUCT_PART("MOD-BYTECODE-STREAM-RESTORE", "debug_info_stripped_round_trip");

		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};
		asCScriptEngine* ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		if (ScriptEngine == nullptr)
		{
			TestRunner->AddError(TEXT("Restore strip test should create a raw SDK engine"));
			return;
		}
		asCModule* SourceModule = BuildRestoreModule(*TestRunner, Engine, "RestoreStrip");
		if (SourceModule == nullptr)
		{
			return;
		}
		AngelscriptNativeTestSupport::FMemoryBinaryStream Stream;
		ASSERT_THAT(AreEqual(static_cast<int32>(asSUCCESS), SourceModule->SaveByteCode(&Stream, true),
			TEXT("Restore strip test should save stripped bytecode")));
		ASSERT_THAT(AreEqual(static_cast<int32>(asSUCCESS), ScriptEngine->DiscardModule("RestoreStrip"),
			TEXT("Restore strip test should explicitly discard the source module")));
		Stream.ResetReadPosition();
		asPWORD PreviousInit = 0;
		DisableAutomaticGlobalInitialization(*ScriptEngine, PreviousInit);
		ON_SCOPE_EXIT
		{
			ScriptEngine->SetEngineProperty(asEP_INIT_GLOBAL_VARS_AFTER_BUILD, PreviousInit);
		};
		asCModule* RestoredModule = CreateDestinationModule(*ScriptEngine, "RestoreStrip");
		if (RestoredModule == nullptr)
		{
			TestRunner->AddError(TEXT("Restore strip test should create a destination module"));
			return;
		}
		bool bWasDebugInfoStripped = false;
		ASSERT_THAT(AreEqual(static_cast<int32>(asSUCCESS), RestoredModule->LoadByteCode(&Stream, &bWasDebugInfoStripped),
			TEXT("Restore strip test should load stripped bytecode")));
		ASSERT_THAT(IsTrue(bWasDebugInfoStripped, TEXT("Restore strip test should report stripped debug information")));
		ASSERT_THAT(AreEqual(static_cast<int32>(asSUCCESS), RestoredModule->ResetGlobalVars(nullptr),
			TEXT("Restore strip test should initialize restored globals")));
		int32 RestoredValue = 0;
		if (!ExecuteRestoreFunction(*TestRunner, *ScriptEngine, *RestoredModule, RestoredValue))
		{
			return;
		}
		ASSERT_THAT(AreEqual(42, RestoredValue, TEXT("Restore strip test should execute stripped restored bytecode")));
		ASSERT_THAT(AreEqual(static_cast<int32>(asSUCCESS), ScriptEngine->DiscardModule("RestoreStrip"),
			TEXT("Restore strip test should explicitly discard the restored module")));
		ASSERT_THAT(IsNull(ScriptEngine->GetModule("RestoreStrip", asGM_ONLY_IF_EXISTS),
			TEXT("Restore strip test should remove restored state from name lookup")));
	}

	TEST_METHOD(EmptyStreamFails)
	{
		AS_NATIVE_PRODUCT_PART("MOD-BYTECODE-STREAM-RESTORE", "empty_stream_rejected");

		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};
		asCScriptEngine* ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		if (ScriptEngine == nullptr)
		{
			TestRunner->AddError(TEXT("Restore empty-stream test should create a raw SDK engine"));
			return;
		}
		asCModule* Module = CreateDestinationModule(*ScriptEngine, "RestoreEmpty");
		if (Module == nullptr)
		{
			TestRunner->AddError(TEXT("Restore empty-stream test should create a destination module"));
			return;
		}
		AngelscriptNativeTestSupport::FMemoryBinaryStream Stream;
		bool bWasDebugInfoStripped = false;
		ASSERT_THAT(AreNotEqual(static_cast<int32>(asSUCCESS), Module->LoadByteCode(&Stream, &bWasDebugInfoStripped),
			TEXT("Restore should reject an empty bytecode stream")));
		ASSERT_THAT(AreEqual(0, static_cast<int32>(Module->GetFunctionCount()), TEXT("Empty stream rejection should leave no functions")));
		ASSERT_THAT(AreEqual(0, static_cast<int32>(Module->GetGlobalVarCount()), TEXT("Empty stream rejection should leave no globals")));
		ASSERT_THAT(AreEqual(0, static_cast<int32>(Module->GetObjectTypeCount()), TEXT("Empty stream rejection should leave no object types")));
		ASSERT_THAT(AreEqual(0, static_cast<int32>(Module->GetEnumCount()), TEXT("Empty stream rejection should leave no enums")));
		ASSERT_THAT(AreEqual(0, static_cast<int32>(Module->GetTypedefCount()), TEXT("Empty stream rejection should leave no typedefs")));
		ASSERT_THAT(AreEqual(0, static_cast<int32>(Module->GetImportedFunctionCount()), TEXT("Empty stream rejection should leave no imports")));
		ASSERT_THAT(AreEqual(static_cast<int32>(asSUCCESS), ScriptEngine->DiscardModule("RestoreEmpty"),
			TEXT("Restore empty-stream test should explicitly discard the rejected destination")));
		ASSERT_THAT(IsNull(ScriptEngine->GetModule("RestoreEmpty", asGM_ONLY_IF_EXISTS),
			TEXT("Restore empty-stream test should leave no module publication after cleanup")));
	}

	TEST_METHOD(TruncatedStreamFails)
	{
		AS_NATIVE_PRODUCT_PART("MOD-BYTECODE-STREAM-RESTORE", "truncated_stream_rejected");

		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};
		asCScriptEngine* ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		if (ScriptEngine == nullptr)
		{
			TestRunner->AddError(TEXT("Restore truncation test should create a raw SDK engine"));
			return;
		}
		asCModule* SourceModule = BuildRestoreModule(*TestRunner, Engine, "RestoreTruncated");
		if (SourceModule == nullptr)
		{
			return;
		}
		AngelscriptNativeTestSupport::FMemoryBinaryStream Stream;
		ASSERT_THAT(AreEqual(static_cast<int32>(asSUCCESS), SourceModule->SaveByteCode(&Stream, false),
			TEXT("Restore truncation test should save bytecode")));
		ASSERT_THAT(IsTrue(Stream.Num() > 16, TEXT("Restore truncation test needs enough bytes to truncate")));
		Stream.TruncateBy(16);
		Stream.ResetReadPosition();
		ASSERT_THAT(AreEqual(static_cast<int32>(asSUCCESS), ScriptEngine->DiscardModule("RestoreTruncated"),
			TEXT("Restore truncation test should explicitly discard the source module")));
		asPWORD PreviousInit = 0;
		DisableAutomaticGlobalInitialization(*ScriptEngine, PreviousInit);
		ON_SCOPE_EXIT
		{
			ScriptEngine->SetEngineProperty(asEP_INIT_GLOBAL_VARS_AFTER_BUILD, PreviousInit);
		};
		asCModule* Module = CreateDestinationModule(*ScriptEngine, "RestoreTruncated");
		if (Module == nullptr)
		{
			TestRunner->AddError(TEXT("Restore truncation test should create a destination module"));
			return;
		}
		bool bWasDebugInfoStripped = false;
		ASSERT_THAT(AreNotEqual(static_cast<int32>(asSUCCESS), Module->LoadByteCode(&Stream, &bWasDebugInfoStripped),
			TEXT("Restore should reject a truncated bytecode stream")));
		ASSERT_THAT(AreEqual(0, static_cast<int32>(Module->GetFunctionCount()), TEXT("Truncated stream rejection should leave no functions")));
		ASSERT_THAT(AreEqual(0, static_cast<int32>(Module->GetGlobalVarCount()), TEXT("Truncated stream rejection should leave no globals")));
		ASSERT_THAT(AreEqual(0, static_cast<int32>(Module->GetObjectTypeCount()), TEXT("Truncated stream rejection should leave no object types")));
		ASSERT_THAT(AreEqual(0, static_cast<int32>(Module->GetEnumCount()), TEXT("Truncated stream rejection should leave no enums")));
		ASSERT_THAT(AreEqual(0, static_cast<int32>(Module->GetTypedefCount()), TEXT("Truncated stream rejection should leave no typedefs")));
		ASSERT_THAT(AreEqual(0, static_cast<int32>(Module->GetImportedFunctionCount()), TEXT("Truncated stream rejection should leave no imports")));
		ASSERT_THAT(AreEqual(static_cast<int32>(asSUCCESS), ScriptEngine->DiscardModule("RestoreTruncated"),
			TEXT("Restore truncation test should explicitly discard the rejected destination")));
		ASSERT_THAT(IsNull(ScriptEngine->GetModule("RestoreTruncated", asGM_ONLY_IF_EXISTS),
			TEXT("Restore truncation test should leave no module publication after cleanup")));
	}

	TEST_METHOD(FailureLeavesModuleClean)
	{
		AS_NATIVE_PRODUCT_PART("MOD-BYTECODE-STREAM-RESTORE", "failed_load_leaves_module_clean");

		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};
		asCScriptEngine* ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		if (ScriptEngine == nullptr)
		{
			TestRunner->AddError(TEXT("Restore cleanup test should create a raw SDK engine"));
			return;
		}
		asCModule* SourceModule = BuildRestoreModule(*TestRunner, Engine, "RestoreCleanup");
		if (SourceModule == nullptr)
		{
			return;
		}
		AngelscriptNativeTestSupport::FMemoryBinaryStream CompleteStream;
		ASSERT_THAT(AreEqual(static_cast<int32>(asSUCCESS), SourceModule->SaveByteCode(&CompleteStream, false),
			TEXT("Restore cleanup test should save bytecode")));
		AngelscriptNativeTestSupport::FMemoryBinaryStream TruncatedStream = CompleteStream;
		ASSERT_THAT(IsTrue(TruncatedStream.Num() > 16, TEXT("Restore cleanup test needs enough bytes to truncate")));
		TruncatedStream.TruncateBy(16);
		TruncatedStream.ResetReadPosition();
		ASSERT_THAT(AreEqual(static_cast<int32>(asSUCCESS), ScriptEngine->DiscardModule("RestoreCleanup"),
			TEXT("Restore cleanup test should explicitly discard the source module")));
		asPWORD PreviousInit = 0;
		DisableAutomaticGlobalInitialization(*ScriptEngine, PreviousInit);
		ON_SCOPE_EXIT
		{
			ScriptEngine->SetEngineProperty(asEP_INIT_GLOBAL_VARS_AFTER_BUILD, PreviousInit);
		};
		asCModule* FailedModule = CreateDestinationModule(*ScriptEngine, "RestoreCleanup");
		if (FailedModule == nullptr)
		{
			TestRunner->AddError(TEXT("Restore cleanup test should create a destination module"));
			return;
		}
		bool bWasDebugInfoStripped = false;
		ASSERT_THAT(AreNotEqual(static_cast<int32>(asSUCCESS), FailedModule->LoadByteCode(&TruncatedStream, &bWasDebugInfoStripped),
			TEXT("Restore cleanup test should reject truncated bytecode")));
		ASSERT_THAT(AreEqual(0, FailedModule->GetFunctionCount(), TEXT("Failed load should leave no functions")));
		ASSERT_THAT(AreEqual(0, FailedModule->GetGlobalVarCount(), TEXT("Failed load should leave no globals")));
		ASSERT_THAT(AreEqual(0, FailedModule->GetObjectTypeCount(), TEXT("Failed load should leave no object types")));
		ASSERT_THAT(AreEqual(0, FailedModule->GetEnumCount(), TEXT("Failed load should leave no enums")));
		ASSERT_THAT(AreEqual(0, FailedModule->GetTypedefCount(), TEXT("Failed load should leave no typedefs")));
		ASSERT_THAT(AreEqual(0, FailedModule->GetImportedFunctionCount(), TEXT("Failed load should leave no imports")));

		ASSERT_THAT(AreEqual(static_cast<int32>(asSUCCESS), ScriptEngine->DiscardModule("RestoreCleanup"),
			TEXT("Restore cleanup test should explicitly discard the failed destination")));
		ASSERT_THAT(IsNull(ScriptEngine->GetModule("RestoreCleanup", asGM_ONLY_IF_EXISTS),
			TEXT("Restore cleanup test should remove failed state before retry")));
		CompleteStream.ResetReadPosition();
		asCModule* RetryModule = CreateDestinationModule(*ScriptEngine, "RestoreCleanup");
		if (RetryModule == nullptr)
		{
			TestRunner->AddError(TEXT("Restore cleanup retry should create a destination module"));
			return;
		}
		bWasDebugInfoStripped = true;
		ASSERT_THAT(AreEqual(static_cast<int32>(asSUCCESS), RetryModule->LoadByteCode(&CompleteStream, &bWasDebugInfoStripped),
			TEXT("Restore cleanup retry should load complete bytecode")));
		ASSERT_THAT(IsFalse(bWasDebugInfoStripped, TEXT("Restore cleanup retry should retain debug information")));
		ASSERT_THAT(AreEqual(static_cast<int32>(asSUCCESS), RetryModule->ResetGlobalVars(nullptr),
			TEXT("Restore cleanup retry should initialize globals")));
		int32 RestoredValue = 0;
		if (!ExecuteRestoreFunction(*TestRunner, *ScriptEngine, *RetryModule, RestoredValue))
		{
			return;
		}
		ASSERT_THAT(AreEqual(42, RestoredValue, TEXT("Restore cleanup retry should execute successfully")));
		ASSERT_THAT(AreEqual(static_cast<int32>(asSUCCESS), ScriptEngine->DiscardModule("RestoreCleanup"),
			TEXT("Restore cleanup retry should explicitly discard the restored module")));
		ASSERT_THAT(IsNull(ScriptEngine->GetModule("RestoreCleanup", asGM_ONLY_IF_EXISTS),
			TEXT("Restore cleanup retry should leave no published module state")));
	}
};

#endif
