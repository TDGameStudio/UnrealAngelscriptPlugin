#include "AngelscriptLearningTrace.h"
#include "AngelscriptTestEngineHelper.h"
#include "AngelscriptTestUtilities.h"
#include "AngelscriptTestMacros.h"

#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"

#include "StartAngelscriptHeaders.h"
#include "angelscript.h"
#include "EndAngelscriptHeaders.h"

#if WITH_DEV_AUTOMATION_TESTS


namespace AngelscriptTest_Learning_Runtime_AngelscriptLearningRestoreAndBytecodePersistenceTests_Private
{
	class FLearningRestoreMemoryBinaryStream final : public asIBinaryStream
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

		int32 Num() const
		{
			return Bytes.Num();
		}

	private:
		TArray<uint8> Bytes;
		int32 ReadOffset = 0;
	};

	struct FLearningRestoreOutcome
	{
		bool bCompiled = false;
		bool bRestoredFunctionAvailable = false;
		int32 SourceExecutionResult = INDEX_NONE;
		int32 StreamByteCount = 0;
		int32 SourceVarCount = 0;
		int32 RestoredVarCount = 0;
		int32 SaveResult = asERROR;
		int32 LoadResult = asERROR;
		bool bWasDebugInfoStripped = false;
		bool bRestoredModuleAvailable = false;
	};

	FString GetBoolLabel(bool bValue)
	{
		return bValue ? TEXT("true") : TEXT("false");
	}

	bool GetModuleFunctionDetails(asIScriptModule* Module, const ANSICHAR* Declaration, asIScriptFunction*& OutFunction, int32& OutVarCount)
	{
		OutFunction = Module != nullptr ? Module->GetFunctionByDecl(Declaration) : nullptr;
		OutVarCount = OutFunction != nullptr ? static_cast<int32>(OutFunction->GetVarCount()) : 0;
		return OutFunction != nullptr;
	}

	bool ExecuteLearningRestoreFunction(FAngelscriptEngine& Engine, asIScriptModule* Module, const TCHAR* Declaration, int32& OutValue)
	{
		if (Module == nullptr)
		{
			return false;
		}

		FTCHARToUTF8 DeclarationUtf8(Declaration);
		asIScriptFunction* Function = Module->GetFunctionByDecl(DeclarationUtf8.Get());
		if (Function == nullptr)
		{
			return false;
		}

		asIScriptContext* Context = Engine.GetScriptEngine()->CreateContext();
		if (Context == nullptr)
		{
			return false;
		}
		ON_SCOPE_EXIT
		{
			Context->Release();
		};

		const int PrepareResult = Context->Prepare(Function);
		if (PrepareResult != asSUCCESS)
		{
			return false;
		}

		const int ExecuteResult = Context->Execute();
		if (ExecuteResult != asEXECUTION_FINISHED)
		{
			return false;
		}

		OutValue = static_cast<int32>(Context->GetReturnDWord());
		return true;
	}

	FLearningRestoreOutcome RunLearningRestoreTestCase(const TCHAR* ModuleNameLiteral, bool bStripDebugInfo)
	{
		FLearningRestoreOutcome Outcome;
		TUniquePtr<FAngelscriptEngine> EngineOwner = CreateIsolatedCloneEngine();
		if (!EngineOwner.IsValid())
		{
			return Outcome;
		}

		FAngelscriptEngine& Engine = *EngineOwner;
		const FName ModuleName(ModuleNameLiteral);
		const FString Filename = FString::Printf(TEXT("%s.as"), ModuleNameLiteral);
		const FString Script = TEXT(R"AS(
int Test()
{
	int BaseValue = 41;
	int LocalValue = BaseValue + 1;
	return LocalValue;
}
)AS");

	Engine.DiscardModule(*ModuleName.ToString());
	Outcome.bCompiled = CompileModuleFromMemory(&Engine, ModuleName, Filename, Script);
	if (!Outcome.bCompiled)
		{
			return Outcome;
		}

		TSharedPtr<FAngelscriptModuleDesc> SourceModuleDesc = Engine.GetModuleByModuleName(ModuleName.ToString());
		asIScriptModule* SourceModule = SourceModuleDesc.IsValid() ? SourceModuleDesc->ScriptModule : nullptr;
		asIScriptFunction* SourceFunction = nullptr;
		GetModuleFunctionDetails(SourceModule, "int Test()", SourceFunction, Outcome.SourceVarCount);

		if (SourceFunction == nullptr || !ExecuteLearningRestoreFunction(Engine, SourceModule, TEXT("int Test()"), Outcome.SourceExecutionResult))
		{
			return Outcome;
		}

		FLearningRestoreMemoryBinaryStream Stream;
		Outcome.SaveResult = SourceModule->SaveByteCode(&Stream, bStripDebugInfo);
		Outcome.StreamByteCount = Stream.Num();
		if (Outcome.SaveResult != asSUCCESS)
		{
			return Outcome;
		}

		Stream.ResetReadOffset();
		Engine.DiscardModule(*ModuleName.ToString());

		asIScriptEngine* ScriptEngine = Engine.GetScriptEngine();
		const asPWORD PreviousInitGlobalsAfterBuild = ScriptEngine->GetEngineProperty(asEP_INIT_GLOBAL_VARS_AFTER_BUILD);
		ScriptEngine->SetEngineProperty(asEP_INIT_GLOBAL_VARS_AFTER_BUILD, 0);
		ON_SCOPE_EXIT
		{
			ScriptEngine->SetEngineProperty(asEP_INIT_GLOBAL_VARS_AFTER_BUILD, PreviousInitGlobalsAfterBuild);
		};

		asIScriptModule* RestoredModule = ScriptEngine->GetModule(TCHAR_TO_ANSI(*ModuleName.ToString()), asGM_ALWAYS_CREATE);
		Outcome.bRestoredModuleAvailable = RestoredModule != nullptr;
		if (RestoredModule == nullptr)
		{
			return Outcome;
		}

		Outcome.LoadResult = RestoredModule->LoadByteCode(&Stream, &Outcome.bWasDebugInfoStripped);
		if (Outcome.LoadResult != asSUCCESS)
		{
			return Outcome;
		}

		asIScriptFunction* RestoredFunction = nullptr;
		GetModuleFunctionDetails(RestoredModule, "int Test()", RestoredFunction, Outcome.RestoredVarCount);
		Outcome.bRestoredFunctionAvailable = RestoredFunction != nullptr;
		return Outcome;
	}

	void TraceLearningRestoreOutcome(FAngelscriptLearningTraceSession& Trace, const FString& TestCaseLabel, const FLearningRestoreOutcome& Outcome)
	{
		Trace.AddStep(TestCaseLabel, Outcome.LoadResult == asSUCCESS ? TEXT("Saved bytecode and restored it into a fresh module instance") : TEXT("Bytecode round-trip failed before the restored module reached a validated load state"));
		Trace.AddKeyValue(TEXT("Compiled"), GetBoolLabel(Outcome.bCompiled));
		Trace.AddKeyValue(TEXT("SaveResult"), FString::FromInt(Outcome.SaveResult));
		Trace.AddKeyValue(TEXT("LoadResult"), FString::FromInt(Outcome.LoadResult));
		Trace.AddKeyValue(TEXT("StreamByteCount"), FString::FromInt(Outcome.StreamByteCount));
		Trace.AddKeyValue(TEXT("SourceExecutionResult"), FString::FromInt(Outcome.SourceExecutionResult));
		Trace.AddKeyValue(TEXT("RestoredFunctionAvailable"), GetBoolLabel(Outcome.bRestoredFunctionAvailable));
		Trace.AddKeyValue(TEXT("SourceVarCount"), FString::FromInt(Outcome.SourceVarCount));
		Trace.AddKeyValue(TEXT("RestoredVarCount"), FString::FromInt(Outcome.RestoredVarCount));
		Trace.AddKeyValue(TEXT("WasDebugInfoStripped"), GetBoolLabel(Outcome.bWasDebugInfoStripped));
	}
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptLearningRestoreAndBytecodePersistenceTest,
	"Angelscript.TestModule.Learning.Runtime.RestoreAndBytecodePersistence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptLearningRestoreAndBytecodePersistenceTest::RunTest(const FString& Parameters)
{
	using namespace AngelscriptTest_Learning_Runtime_AngelscriptLearningRestoreAndBytecodePersistenceTests_Private;
	FAngelscriptLearningTraceSinkConfig SinkConfig;
	SinkConfig.bEmitToAutomation = true;
	SinkConfig.bEmitToLog = true;
	SinkConfig.bEmitToFile = false;

	FAngelscriptLearningTraceSession Trace(TEXT("LearningRestoreAndBytecodePersistence"), SinkConfig);
	Trace.BeginPhase(EAngelscriptLearningTracePhase::Compile);
	const FLearningRestoreOutcome PreserveDebugOutcome = RunLearningRestoreTestCase(TEXT("LearningRestorePreserveDebugModule"), false);
	Trace.AddStep(TEXT("CompileSourceModule"), TEXT("Compiled the source module once before any serialization so the baseline execution result and local-variable metadata are visible"));
	Trace.AddKeyValue(TEXT("PreserveDebugCompiled"), GetBoolLabel(PreserveDebugOutcome.bCompiled));

	Trace.BeginPhase(EAngelscriptLearningTracePhase::Bytecode);
	TraceLearningRestoreOutcome(Trace, TEXT("SaveAndLoadBytecode.PreserveDebug"), PreserveDebugOutcome);
	const FLearningRestoreOutcome StripDebugOutcome = RunLearningRestoreTestCase(TEXT("LearningRestoreStripDebugModule"), true);
	TraceLearningRestoreOutcome(Trace, TEXT("SaveAndLoadBytecode.StripDebug"), StripDebugOutcome);

	Trace.BeginPhase(EAngelscriptLearningTracePhase::Execution);
	Trace.AddStep(TEXT("CompareExecutionAndDebugInfo"), TEXT("Kept behavioral proof on the source module before serialization, then compared restore metadata to show how debug-info stripping changes what can be observed after load in this branch"));
	Trace.AddKeyValue(TEXT("PreserveDebugBehaviorVerifiedPreSave"), GetBoolLabel(PreserveDebugOutcome.SourceExecutionResult == 42));
	Trace.AddKeyValue(TEXT("StripDebugBehaviorVerifiedPreSave"), GetBoolLabel(StripDebugOutcome.SourceExecutionResult == 42));
	Trace.AddKeyValue(TEXT("PreserveDebugFunctionStillDiscoverable"), GetBoolLabel(PreserveDebugOutcome.bRestoredFunctionAvailable));
	Trace.AddKeyValue(TEXT("StripDebugFunctionStillDiscoverable"), GetBoolLabel(StripDebugOutcome.bRestoredFunctionAvailable));
	Trace.AddKeyValue(TEXT("PreserveDebugFlag"), GetBoolLabel(PreserveDebugOutcome.bWasDebugInfoStripped));
	Trace.AddKeyValue(TEXT("StripDebugFlag"), GetBoolLabel(StripDebugOutcome.bWasDebugInfoStripped));

	const bool bPreserveCompiled = TestTrue(TEXT("Preserve-debug restore test case should compile"), PreserveDebugOutcome.bCompiled);
	const bool bPreserveSaveSucceeded = TestEqual(TEXT("Preserve-debug restore test case should save bytecode successfully"), PreserveDebugOutcome.SaveResult, static_cast<int32>(asSUCCESS));
	const bool bPreserveLoadSucceeded = TestEqual(TEXT("Preserve-debug restore test case should load bytecode successfully"), PreserveDebugOutcome.LoadResult, static_cast<int32>(asSUCCESS));
	const bool bPreserveRunsBeforeRestore = TestEqual(TEXT("Preserve-debug source execution should return the expected value"), PreserveDebugOutcome.SourceExecutionResult, 42);
	const bool bPreserveFunctionStillVisible = TestTrue(TEXT("Preserve-debug restore should still expose the function declaration after load"), PreserveDebugOutcome.bRestoredFunctionAvailable);
	const bool bPreserveRetainsDebugFlag = TestFalse(TEXT("Preserve-debug restore should report intact debug info"), PreserveDebugOutcome.bWasDebugInfoStripped);

	const bool bStripCompiled = TestTrue(TEXT("Strip-debug restore test case should compile"), StripDebugOutcome.bCompiled);
	const bool bStripSaveSucceeded = TestEqual(TEXT("Strip-debug restore test case should save bytecode successfully"), StripDebugOutcome.SaveResult, static_cast<int32>(asSUCCESS));
	const bool bStripLoadSucceeded = TestEqual(TEXT("Strip-debug restore test case should load bytecode successfully"), StripDebugOutcome.LoadResult, static_cast<int32>(asSUCCESS));
	const bool bStripRunsBeforeRestore = TestEqual(TEXT("Strip-debug source execution should return the expected value"), StripDebugOutcome.SourceExecutionResult, 42);
	const bool bStripFunctionStillVisible = TestTrue(TEXT("Strip-debug restore should still expose the function declaration after load"), StripDebugOutcome.bRestoredFunctionAvailable);
	const bool bStripReportsDebugLoss = TestTrue(TEXT("Strip-debug restore should report stripped debug info"), StripDebugOutcome.bWasDebugInfoStripped);
	const bool bBytecodeProduced = TestTrue(TEXT("Both restore test cases should serialize non-empty bytecode"), PreserveDebugOutcome.StreamByteCount > 0 && StripDebugOutcome.StreamByteCount > 0);

	const bool bPhaseSequenceOk = AssertLearningTracePhaseSequence(*this, Trace.GetEvents(), {
		EAngelscriptLearningTracePhase::Compile,
		EAngelscriptLearningTracePhase::Bytecode,
		EAngelscriptLearningTracePhase::Execution,
	});
	const bool bContainsStripKeyword = AssertLearningTraceContainsKeyword(*this, Trace.GetEvents(), TEXT("WasDebugInfoStripped"));
	const bool bContainsPersistenceKeyword = AssertLearningTraceContainsKeyword(*this, Trace.GetEvents(), TEXT("StreamByteCount"));
	const bool bMinimumEventsOk = AssertLearningTraceMinimumEventCount(*this, Trace.GetEvents(), 4);

	Trace.FlushToAutomation(*this);
	Trace.FlushToLog();
	return bPreserveCompiled
		&& bPreserveSaveSucceeded
		&& bPreserveLoadSucceeded
		&& bPreserveRunsBeforeRestore
		&& bPreserveFunctionStillVisible
		&& bPreserveRetainsDebugFlag
		&& bStripCompiled
		&& bStripSaveSucceeded
		&& bStripLoadSucceeded
		&& bStripRunsBeforeRestore
		&& bStripFunctionStillVisible
		&& bStripReportsDebugLoss
		&& bBytecodeProduced
		&& bPhaseSequenceOk
		&& bContainsStripKeyword
		&& bContainsPersistenceKeyword
		&& bMinimumEventsOk;
}

#endif
