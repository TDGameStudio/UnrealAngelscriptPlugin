#include "../../Support/AngelscriptNativeCaseTestSupport.h"
#include "../../Support/AngelscriptNativeBuilderTestSupport.h"
#include "../../Support/AngelscriptNativeCoreTestSupport.h"
#include "../../Support/AngelscriptNativeFixtureTestSupport.h"
#include "../../Support/AngelscriptNativeLanguageCaseTestSupport.h"

#include "CQTest.h"
#include "Misc/ScopeExit.h"

using AngelscriptNativeTestSupport::AppendGeneratedAsLine;

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FVariableFailureBoundaryTests,
	"Angelscript.TestModule.AngelScriptSDK.Language.Variables.FailureBoundary",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	using FNativeCaseContext = AngelscriptNativeTestSupport::FNativeCaseContext;
	using FNativeLifecycleRecorder = AngelscriptNativeTestSupport::FNativeLifecycleRecorder;
	using FNativeLifecycleFaultController = AngelscriptNativeTestSupport::FNativeLifecycleFaultController;
	using ENativeLifecycleEvent = AngelscriptNativeTestSupport::ENativeLifecycleEvent;

	enum class EScenarioOutcome : uint8
	{
		CompileFailure,
		RuntimeSuccess,
		RuntimeException,
	};

	struct FScenarioCase
	{
		const ANSICHAR* CatalogName;
		EScenarioOutcome Outcome;
		int32 ExpectedReturn;
		bool bHasDeterministicReturn;
		int32 ExpectedLocals;
		int32 ExpectedTrackedValues;
	};

	struct FRecoveryCase
	{
		const ANSICHAR* CatalogName;
		bool bFreshModule;
	};

	inline static constexpr int32 SupportedLocalCount = 128;
	inline static constexpr int32 PracticalBoundaryLocalCount = 2048;
	inline static constexpr int32 PressureObjectCount = 128;
	inline static constexpr int32 LongIdentifierLength = 512;

	inline static constexpr FScenarioCase ScenarioCases[] =
	{
		{ "use_before_declaration", EScenarioOutcome::CompileFailure, 0, false, 0, 0 },
		{ "use_after_scope", EScenarioOutcome::CompileFailure, 0, false, 0, 0 },
		{ "duplicate_same_scope", EScenarioOutcome::CompileFailure, 0, false, 0, 0 },
		{ "incompatible_initializer", EScenarioOutcome::CompileFailure, 0, false, 0, 0 },
		{ "mutable_global", EScenarioOutcome::CompileFailure, 0, false, 0, 0 },
		{ "reference_global", EScenarioOutcome::CompileFailure, 0, false, 0, 0 },
		{ "uninitialized_read", EScenarioOutcome::RuntimeSuccess, 0, false, 1, 0 },
		{ "initializer_exception", EScenarioOutcome::RuntimeException, 0, false, 2, 0 },
		{ "failed_initializer_atomicity", EScenarioOutcome::RuntimeException, 0, false, 2, 2 },
		{ "long_identifier", EScenarioOutcome::RuntimeSuccess, 61, true, 1, 0 },
		{ "many_locals_supported", EScenarioOutcome::RuntimeSuccess, SupportedLocalCount, true, SupportedLocalCount + 1, 0 },
		{ "many_locals_boundary", EScenarioOutcome::RuntimeSuccess, PracticalBoundaryLocalCount, true, PracticalBoundaryLocalCount + 1, 0 },
		{ "stack_frame_pressure", EScenarioOutcome::RuntimeSuccess, PressureObjectCount, true, PressureObjectCount, PressureObjectCount },
		{ "module_discard", EScenarioOutcome::RuntimeSuccess, 61, true, 1, 0 },
	};

	inline static constexpr FRecoveryCase RecoveryCases[] =
	{
		{ "fresh_module", true },
		{ "same_module_or_context", false },
	};

	static bool IsScenario(const FScenarioCase& ScenarioCase, const ANSICHAR* Name)
	{
		return FCStringAnsi::Strcmp(ScenarioCase.CatalogName, Name) == 0;
	}

	static FString MakeSuffix(
		const FRecoveryCase& RecoveryCase,
		const FScenarioCase& ScenarioCase)
	{
		return FString::Printf(
			TEXT("%hs_%hs"),
			RecoveryCase.CatalogName,
			ScenarioCase.CatalogName);
	}

	static FString MakeLongIdentifier()
	{
		return TEXT("LongVariable_") + FString::ChrN(LongIdentifierLength, TEXT('L'));
	}

	static void AppendCompileFailureEntry(
		FString& Source,
		const FScenarioCase& ScenarioCase)
	{
		using namespace AngelscriptNativeTestSupport;

		if (IsScenario(ScenarioCase, "mutable_global"))
		{
			AppendGeneratedAsLine(Source, TEXT("int MutableBoundaryGlobal = 17;"));
			AppendGeneratedAsLine(Source);
		}
		else if (IsScenario(ScenarioCase, "reference_global"))
		{
			AppendGeneratedAsLine(Source, TEXT("FNativeCaseReference ReferenceBoundaryGlobal = CreateNativeCaseReference(17);"));
			AppendGeneratedAsLine(Source);
		}

		AppendGeneratedAsLine(Source, TEXT("int RunVariableBoundary()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		if (IsScenario(ScenarioCase, "use_before_declaration"))
		{
			AppendGeneratedAsLine(Source, TEXT("\tint Captured = Value;"));
			AppendGeneratedAsLine(Source, TEXT("\tint Value = 19;"));
			AppendGeneratedAsLine(Source, TEXT("\treturn Captured;"));
		}
		else if (IsScenario(ScenarioCase, "use_after_scope"))
		{
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t\tint ScopedValue = 23;"));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
			AppendGeneratedAsLine(Source, TEXT("\treturn ScopedValue;"));
		}
		else if (IsScenario(ScenarioCase, "duplicate_same_scope"))
		{
			AppendGeneratedAsLine(Source, TEXT("\tint DuplicateValue = 29;"));
			AppendGeneratedAsLine(Source, TEXT("\tint DuplicateValue = 31;"));
			AppendGeneratedAsLine(Source, TEXT("\treturn DuplicateValue;"));
		}
		else if (IsScenario(ScenarioCase, "incompatible_initializer"))
		{
			AppendGeneratedAsLine(Source, TEXT("\tint IncompatibleValue = FNativeCaseValue(37);"));
			AppendGeneratedAsLine(Source, TEXT("\treturn IncompatibleValue;"));
		}
		else if (IsScenario(ScenarioCase, "mutable_global"))
		{
			AppendGeneratedAsLine(Source, TEXT("\treturn MutableBoundaryGlobal;"));
		}
		else
		{
			AppendGeneratedAsLine(Source, TEXT("\treturn ReferenceBoundaryGlobal.Value;"));
		}
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
	}

	static void AppendUninitializedEntry(FString& Source)
	{
		using namespace AngelscriptNativeTestSupport;

		AppendGeneratedAsLine(Source, TEXT("int RunVariableBoundary()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tint Value;"));
		AppendGeneratedAsLine(Source, TEXT("\treturn Value;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
	}

	static void AppendInitializerExceptionEntry(FString& Source)
	{
		using namespace AngelscriptNativeTestSupport;

		AppendGeneratedAsLine(Source, TEXT("int RunVariableBoundary()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tint Zero = 0;"));
		AppendGeneratedAsLine(Source, TEXT("\tint Value = 41 / Zero;"));
		AppendGeneratedAsLine(Source, TEXT("\treturn Value;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
	}

	static void AppendAtomicInitializerEntry(FString& Source)
	{
		using namespace AngelscriptNativeTestSupport;

		AppendGeneratedAsLine(Source, TEXT("int RunVariableBoundary()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tFNativeCaseValue SourceValue(43);"));
		AppendGeneratedAsLine(Source, TEXT("\tArmNextNativeCaseValueCopyFault();"));
		AppendGeneratedAsLine(Source, TEXT("\tFNativeCaseValue TargetValue(SourceValue);"));
		AppendGeneratedAsLine(Source, TEXT("\treturn TargetValue.Value;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
	}

	static void AppendLongIdentifierEntry(FString& Source)
	{
		using namespace AngelscriptNativeTestSupport;

		const FString Identifier = MakeLongIdentifier();
		AppendGeneratedAsLine(Source, TEXT("int RunVariableBoundary()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, FString::Printf(TEXT("\tint %s = 61;"), *Identifier));
		AppendGeneratedAsLine(Source, FString::Printf(TEXT("\treturn %s;"), *Identifier));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
	}

	static void AppendManyLocalsEntry(FString& Source, const int32 LocalCount)
	{
		using namespace AngelscriptNativeTestSupport;

		AppendGeneratedAsLine(Source, TEXT("int RunVariableBoundary()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tint Sum = 0;"));
		for (int32 Index = 0; Index < LocalCount; ++Index)
		{
			AppendGeneratedAsLine(Source, FString::Printf(
				TEXT("\tint Local%04d = 1;"),
				Index));
			AppendGeneratedAsLine(Source, FString::Printf(
				TEXT("\tSum += Local%04d;"),
				Index));
		}
		AppendGeneratedAsLine(Source, TEXT("\treturn Sum;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
	}

	static void AppendStackPressureEntry(FString& Source)
	{
		using namespace AngelscriptNativeTestSupport;

		AppendGeneratedAsLine(Source, TEXT("int RunVariableBoundary()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		for (int32 Index = 0; Index < PressureObjectCount; ++Index)
		{
			AppendGeneratedAsLine(Source, FString::Printf(
				TEXT("\tFNativeCaseValue PressureValue%03d(%d);"),
				Index,
				1000 + Index));
		}
		AppendGeneratedAsLine(Source, TEXT("\tint Result = 0;"));
		for (int32 Index = 0; Index < PressureObjectCount; ++Index)
		{
			AppendGeneratedAsLine(Source, FString::Printf(
				TEXT("\tResult += PressureValue%03d.Value - %d;"),
				Index,
				999 + Index));
		}
		AppendGeneratedAsLine(Source, TEXT("\treturn Result;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
	}

	static void AppendModuleDiscardEntry(FString& Source)
	{
		using namespace AngelscriptNativeTestSupport;

		AppendGeneratedAsLine(Source, TEXT("class FDiscardVariableOwner"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tint Value = 61;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("int RunVariableBoundary()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tFDiscardVariableOwner Owner = FDiscardVariableOwner();"));
		AppendGeneratedAsLine(Source, TEXT("\treturn Owner.Value;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
	}

	static FString BuildBoundarySource(const FScenarioCase& ScenarioCase)
	{
		using namespace AngelscriptNativeTestSupport;

		FString Source;
		if (ScenarioCase.Outcome == EScenarioOutcome::CompileFailure)
		{
			AppendCompileFailureEntry(Source, ScenarioCase);
		}
		else if (IsScenario(ScenarioCase, "uninitialized_read"))
		{
			AppendUninitializedEntry(Source);
		}
		else if (IsScenario(ScenarioCase, "initializer_exception"))
		{
			AppendInitializerExceptionEntry(Source);
		}
		else if (IsScenario(ScenarioCase, "failed_initializer_atomicity"))
		{
			AppendAtomicInitializerEntry(Source);
		}
		else if (IsScenario(ScenarioCase, "long_identifier"))
		{
			AppendLongIdentifierEntry(Source);
		}
		else if (IsScenario(ScenarioCase, "many_locals_supported"))
		{
			AppendManyLocalsEntry(Source, SupportedLocalCount);
		}
		else if (IsScenario(ScenarioCase, "many_locals_boundary"))
		{
			AppendManyLocalsEntry(Source, PracticalBoundaryLocalCount);
		}
		else if (IsScenario(ScenarioCase, "stack_frame_pressure"))
		{
			AppendStackPressureEntry(Source);
		}
		else
		{
			AppendModuleDiscardEntry(Source);
		}

		AppendGeneratedAsLine(Source, TEXT("int CleanAfterVariableBoundary()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\treturn 83;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		return Source;
	}

	static FString BuildRecoverySource()
	{
		using namespace AngelscriptNativeTestSupport;

		FString Source;
		AppendGeneratedAsLine(Source, TEXT("int RunVariableBoundaryRecovery()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\treturn 97;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		return Source;
	}

	static bool HasLocatedError(const AngelscriptNativeTestSupport::FNativeMessageCollector& Messages)
	{
		return Messages.Entries.ContainsByPredicate([](const AngelscriptNativeTestSupport::FNativeMessageEntry& Entry)
		{
			return Entry.Type == asMSGTYPE_ERROR
				&& Entry.Row > 0
				&& Entry.Column > 0
				&& !Entry.Section.IsEmpty()
				&& !Entry.Message.IsEmpty();
		});
	}

	void VerifyBoundaryMetadata(
		const FNativeCaseContext& Case,
		const FScenarioCase& ScenarioCase,
		asIScriptModule& Module)
	{
		asIScriptFunction* const Entry = Module.GetFunctionByDecl("int RunVariableBoundary()");
		ASSERT_THAT(IsNotNull(Entry,
			*Case.Describe(TEXT("boundary scenario should expose its exact entry metadata"))));
		if (Entry == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(Entry->GetVarCount() >= static_cast<asUINT>(ScenarioCase.ExpectedLocals),
			*Case.Describe(TEXT("boundary entry should publish every expected local debug slot"))));

		if (IsScenario(ScenarioCase, "long_identifier"))
		{
			const FString ExpectedName = MakeLongIdentifier();
			bool bFound = false;
			for (asUINT Index = 0; Index < Entry->GetVarCount(); ++Index)
			{
				const char* Name = nullptr;
				if (Entry->GetVar(Index, &Name) >= 0
					&& Name != nullptr
					&& FString(UTF8_TO_TCHAR(Name)) == ExpectedName)
				{
					bFound = true;
				}
			}
			ASSERT_THAT(IsTrue(bFound,
				*Case.Describe(TEXT("long identifier should survive exactly in debug metadata"))));
		}
	}

	void VerifyTrackedLifecycle(
		const FNativeCaseContext& Case,
		const FScenarioCase& ScenarioCase,
		const FNativeLifecycleRecorder& Lifecycle)
	{
		ASSERT_THAT(AreEqual(ScenarioCase.ExpectedTrackedValues,
			Lifecycle.Num(ENativeLifecycleEvent::ValueConstruct)
				+ Lifecycle.Num(ENativeLifecycleEvent::CopyConstruct),
			*Case.Describe(TEXT("boundary scenario should construct the exact tracked-value count"))));
		ASSERT_THAT(AreEqual(ScenarioCase.ExpectedTrackedValues, Lifecycle.Num(ENativeLifecycleEvent::Destruct),
			*Case.Describe(TEXT("boundary scenario should destroy every initialized tracked value exactly once"))));
		ASSERT_THAT(AreEqual(0, Lifecycle.GetLiveObjectCount(),
			*Case.Describe(TEXT("boundary cleanup should leave no tracked value alive"))));

		TSet<int32> ConstructedIds;
		TSet<int32> DestructedIds;
		for (const AngelscriptNativeTestSupport::FNativeLifecycleEntry& Entry : Lifecycle.GetEntries())
		{
			if (Entry.Event == ENativeLifecycleEvent::ValueConstruct
				|| Entry.Event == ENativeLifecycleEvent::CopyConstruct)
			{
				ConstructedIds.Add(Entry.ObjectId);
			}
			else if (Entry.Event == ENativeLifecycleEvent::Destruct)
			{
				ASSERT_THAT(IsTrue(ConstructedIds.Contains(Entry.ObjectId),
					*Case.Describe(TEXT("boundary destructor should identify a constructed value"))));
				ASSERT_THAT(IsFalse(DestructedIds.Contains(Entry.ObjectId),
					*Case.Describe(TEXT("boundary value should be destroyed no more than once"))));
				DestructedIds.Add(Entry.ObjectId);
			}
		}
		ASSERT_THAT(AreEqual(ConstructedIds.Num(), DestructedIds.Num(),
			*Case.Describe(TEXT("boundary lifecycle identities should balance exactly"))));
	}

	void ExecuteRecoveryFunction(
		const FNativeCaseContext& Case,
		asIScriptEngine& ScriptEngine,
		asIScriptFunction& Recovery,
		asIScriptContext* ReusedContext = nullptr)
	{
		using namespace AngelscriptNativeTestSupport;

		asIScriptContext* const Context = ReusedContext != nullptr ? ReusedContext : ScriptEngine.CreateContext();
		ASSERT_THAT(IsNotNull(Context,
			*Case.Describe(TEXT("boundary recovery should obtain an execution context"))));
		if (Context == nullptr)
		{
			return;
		}
		ASSERT_THAT(AreEqual(asSUCCESS, Context->Prepare(&Recovery),
			*Case.Describe(TEXT("boundary recovery should prepare cleanly"))));
		ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_FINISHED), Context->Execute(),
			*Case.Describe(TEXT("boundary recovery should execute cleanly"))));
		ASSERT_THAT(AreEqual(ReusedContext != nullptr ? 83 : 97, static_cast<int32>(Context->GetReturnDWord()),
			*Case.Describe(TEXT("boundary recovery should not retain stale compile or execution state"))));
		if (ReusedContext == nullptr)
		{
			Context->Release();
		}
	}

	void CompileRecoveryModule(
		const FNativeCaseContext& Case,
		const FRecoveryCase& RecoveryCase,
		asIScriptEngine& ScriptEngine,
		AngelscriptNativeTestSupport::FNativeTestEngine& Engine,
		const FString& InitialModuleName)
	{
		using namespace AngelscriptNativeTestSupport;

		const FString RecoveryModuleName = RecoveryCase.bFreshModule
			? InitialModuleName + TEXT("_FreshRecovery")
			: InitialModuleName;
		const FString RecoverySource = BuildRecoverySource();
		PrintGeneratedAsSource(
			*TestRunner,
			Case.GetId() + TEXT("-RECOVERY"),
			RecoveryModuleName,
			RecoverySource);
		const FTCHARToUTF8 RecoveryModuleNameUtf8(*RecoveryModuleName);
		const FTCHARToUTF8 RecoverySourceUtf8(*RecoverySource);
		Engine.ResetMessages();
		asIScriptModule* RecoveryModule = nullptr;
		ASSERT_THAT(IsTrue(CompileNativeModule(
			&ScriptEngine,
			RecoveryModuleNameUtf8.Get(),
			RecoverySourceUtf8.Get(),
			RecoveryModule) >= 0,
			*Case.Describe(TEXT("boundary recovery module should compile"))));
		ASSERT_THAT(IsNotNull(RecoveryModule,
			*Case.Describe(TEXT("boundary recovery should publish a module"))));
		if (RecoveryModule != nullptr)
		{
			asIScriptFunction* const Recovery = RecoveryModule->GetFunctionByDecl("int RunVariableBoundaryRecovery()");
			ASSERT_THAT(IsNotNull(Recovery,
				*Case.Describe(TEXT("boundary recovery module should expose its exact entry"))));
			if (Recovery != nullptr)
			{
				ExecuteRecoveryFunction(Case, ScriptEngine, *Recovery);
			}
		}
		ScriptEngine.DiscardModule(RecoveryModuleNameUtf8.Get());
		ASSERT_THAT(IsNull(ScriptEngine.GetModule(RecoveryModuleNameUtf8.Get(), asGM_ONLY_IF_EXISTS),
			*Case.Describe(TEXT("boundary recovery should discard its module"))));
	}

	void ExecuteBoundary(
		const FNativeCaseContext& Case,
		const FScenarioCase& ScenarioCase,
		const FRecoveryCase& RecoveryCase,
		asIScriptEngine& ScriptEngine,
		asIScriptModule& Module,
		FNativeLifecycleRecorder& Lifecycle,
		FNativeLifecycleFaultController& FaultController,
		const FString& ModuleName,
		asIScriptFunction*& OutRetainedFunction)
	{
		using namespace AngelscriptNativeTestSupport;

		VerifyBoundaryMetadata(Case, ScenarioCase, Module);
		asIScriptFunction* const Entry = Module.GetFunctionByDecl("int RunVariableBoundary()");
		asIScriptFunction* const Clean = Module.GetFunctionByDecl("int CleanAfterVariableBoundary()");
		ASSERT_THAT(IsNotNull(Clean,
			*Case.Describe(TEXT("boundary source should expose its same-context recovery entry"))));
		if (Entry == nullptr || Clean == nullptr)
		{
			return;
		}

		if (IsScenario(ScenarioCase, "module_discard"))
		{
			OutRetainedFunction = Entry;
			OutRetainedFunction->AddRef();
		}

		asIScriptContext* const Context = ScriptEngine.CreateContext();
		ASSERT_THAT(IsNotNull(Context,
			*Case.Describe(TEXT("boundary scenario should create an execution context"))));
		if (Context == nullptr)
		{
			return;
		}

		const int32 ExecuteResult = PrepareAndExecute(Context, Entry);
		if (ScenarioCase.Outcome == EScenarioOutcome::RuntimeException)
		{
			ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_EXCEPTION), ExecuteResult,
				*Case.Describe(TEXT("configured initializer failure should raise an execution exception"))));
			ASSERT_THAT(IsNotNull(Context->GetExceptionFunction(),
				*Case.Describe(TEXT("initializer exception should identify its throwing function"))));
			const char* ExceptionSection = nullptr;
			int ExceptionColumn = 0;
			ASSERT_THAT(IsTrue(Context->GetExceptionLineNumber(&ExceptionColumn, &ExceptionSection) > 0,
				*Case.Describe(TEXT("initializer exception should retain a one-based source line"))));
			ASSERT_THAT(IsTrue(ExceptionColumn > 0,
				*Case.Describe(TEXT("initializer exception should retain a one-based source column"))));
			ASSERT_THAT(AreEqual(ModuleName, FString(UTF8_TO_TCHAR(ExceptionSection != nullptr ? ExceptionSection : "")),
				*Case.Describe(TEXT("initializer exception should retain the generated module section"))));
		}
		else
		{
			ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_FINISHED), ExecuteResult,
				*Case.Describe(TEXT("successful boundary scenario should finish"))));
			if (ExecuteResult == asEXECUTION_FINISHED && ScenarioCase.bHasDeterministicReturn)
			{
				ASSERT_THAT(AreEqual(ScenarioCase.ExpectedReturn, static_cast<int32>(Context->GetReturnDWord()),
					*Case.Describe(TEXT("boundary scenario should return its exact supported value"))));
			}
			else if (ExecuteResult == asEXECUTION_FINISHED)
			{
				TestRunner->AddInfo(FString::Printf(
					TEXT("[%s] primitive uninitialized-read observation returned %d; this fork/base API has no stable value contract, so completion and cleanup are the oracle"),
					*Case.GetId(),
					static_cast<int32>(Context->GetReturnDWord())));
			}
		}

		ASSERT_THAT(AreEqual(asSUCCESS, Context->Unprepare(),
			*Case.Describe(TEXT("boundary context should clean its complete or failed frame"))));
		VerifyTrackedLifecycle(Case, ScenarioCase, Lifecycle);
		if (IsScenario(ScenarioCase, "failed_initializer_atomicity"))
		{
			ASSERT_THAT(AreEqual(1, FaultController.GetTriggeredCopyCount(),
				*Case.Describe(TEXT("failed initializer should consume exactly one copy fault"))));
			ASSERT_THAT(IsFalse(FaultController.IsArmed(),
				*Case.Describe(TEXT("failed initializer should consume its one-shot fault"))));
		}

		if (!RecoveryCase.bFreshModule)
		{
			ExecuteRecoveryFunction(Case, ScriptEngine, *Clean, Context);
		}
		Context->Release();

	}

public:
	TEST_METHOD(ScenariosByRecovery)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT("LANG-VAR-FAILURE-BOUNDARY",
			ENativeEvidence::Compile
			| ENativeEvidence::Diagnostic
			| ENativeEvidence::Runtime
			| ENativeEvidence::Metadata
			| ENativeEvidence::Lifecycle
			| ENativeEvidence::Debug
			| ENativeEvidence::Cleanup
			| ENativeEvidence::Isolation);

		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };
		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine,
			TEXT("Variable-failure-boundary product should create a raw SDK engine")));
		if (ScriptEngine == nullptr)
		{
			return;
		}

		FNativeLifecycleRecorder Lifecycle;
		FNativeLifecycleFaultController FaultController;
		Lifecycle.Reset();
		FaultController.Reset();
		ASSERT_THAT(IsTrue(RegisterNativeCaseValue(*ScriptEngine, Lifecycle, &FaultController),
			TEXT("Variable-failure-boundary product should register its fault-injectable tracked value")));
		ASSERT_THAT(IsTrue(RegisterNativeCaseReference(*ScriptEngine, &Lifecycle),
			TEXT("Variable-failure-boundary product should register its tracked reference")));

		for (const FRecoveryCase& RecoveryCase : RecoveryCases)
		{
			for (const FScenarioCase& ScenarioCase : ScenarioCases)
			{
				Lifecycle.Reset();
				FaultController.Reset();
				const FNativeCaseContext Case(MakeNativeCaseId(
					"LANG-VAR-FAILURE-BOUNDARY",
					{
						ANSI_TO_TCHAR(RecoveryCase.CatalogName),
						ANSI_TO_TCHAR(ScenarioCase.CatalogName),
					}));
				const FString Suffix = MakeSuffix(RecoveryCase, ScenarioCase);
				const FString ModuleName = TEXT("VariableFailureBoundary_") + Suffix;
				const FString Source = BuildBoundarySource(ScenarioCase);
				PrintGeneratedAsSource(*TestRunner, Case.GetId(), ModuleName, Source);
				const FTCHARToUTF8 ModuleNameUtf8(*ModuleName);
				const FTCHARToUTF8 SourceUtf8(*Source);
				Engine.ResetMessages();
				asIScriptModule* Module = nullptr;
				asIScriptFunction* RetainedFunction = nullptr;
				const int BuildResult = CompileNativeModule(
					ScriptEngine,
					ModuleNameUtf8.Get(),
					SourceUtf8.Get(),
					Module);

				if (ScenarioCase.Outcome == EScenarioOutcome::CompileFailure)
				{
					ASSERT_THAT(IsTrue(BuildResult < 0,
						*Case.Describe(TEXT("named variable failure should be rejected during compilation"))));
					ASSERT_THAT(IsTrue(HasLocatedError(Engine.GetMessages()),
						*Case.Describe(TEXT("named variable failure should report a located diagnostic"))));
					if (Module != nullptr)
					{
						asIScriptFunction* const PartialEntry = Module->GetFunctionByDecl("int RunVariableBoundary()");
						ASSERT_THAT(IsTrue(PartialEntry == nullptr || !AngelscriptBuilderTestSupport::HasBytecode(PartialEntry),
							*Case.Describe(TEXT("failed variable source should publish no executable partial entry"))));
					}
					ASSERT_THAT(AreEqual(0, Lifecycle.GetLiveObjectCount(),
						*Case.Describe(TEXT("compile failure should create no runtime variable"))));
				}
				else
				{
					ASSERT_THAT(IsTrue(BuildResult >= 0,
						*Case.Describe(TEXT("supported variable boundary should compile"))));
					ASSERT_THAT(IsNotNull(Module,
						*Case.Describe(TEXT("supported variable boundary should publish a module"))));
					if (BuildResult >= 0 && Module != nullptr)
					{
						ExecuteBoundary(
							Case,
							ScenarioCase,
							RecoveryCase,
							*ScriptEngine,
							*Module,
							Lifecycle,
							FaultController,
							ModuleName,
							RetainedFunction);
					}
				}

				ScriptEngine->DiscardModule(ModuleNameUtf8.Get());
				ASSERT_THAT(IsNull(ScriptEngine->GetModule(ModuleNameUtf8.Get(), asGM_ONLY_IF_EXISTS),
					*Case.Describe(TEXT("variable boundary should discard its initial module"))));
				if (RetainedFunction != nullptr)
				{
					ASSERT_THAT(IsTrue(RetainedFunction->GetDeclaration() != nullptr
						&& FCStringAnsi::Strcmp(RetainedFunction->GetDeclaration(), "int RunVariableBoundary()") == 0,
						*Case.Describe(TEXT("discarded-module function should preserve its declaration until explicit release"))));
					RetainedFunction->Release();
				}
				if (ScenarioCase.Outcome == EScenarioOutcome::CompileFailure || RecoveryCase.bFreshModule)
				{
					CompileRecoveryModule(Case, RecoveryCase, *ScriptEngine, Engine, ModuleName);
				}
				ASSERT_THAT(AreEqual(0, Lifecycle.GetLiveObjectCount(),
					*Case.Describe(TEXT("boundary recovery should leave no tracked value alive"))));
			}
		}
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
