#include "../../Support/AngelscriptNativeCaseTestSupport.h"
#include "../../Support/AngelscriptNativeCoreTestSupport.h"
#include "../../Support/AngelscriptNativeFixtureTestSupport.h"
#include "../../Support/AngelscriptNativeLanguageCaseTestSupport.h"

#include "CQTest.h"

using AngelscriptNativeTestSupport::AppendGeneratedAsLine;
using namespace AngelscriptNativeTestSupport;

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FFunctionValueLifecycleTests,
	"Angelscript.TestModule.AngelScriptSDK.Language.Functions.ValueLifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{

private:
	struct FFailureCase
	{
		const ANSICHAR* CatalogName;
		bool bException;
		bool bArgumentEvaluation;
		bool bBody;
		bool bReturnConstruction;
	};

	struct FInitializedCase
	{
		const ANSICHAR* CatalogName;
		int32 Count;
	};

	struct FTransferCase
	{
		const ANSICHAR* CatalogName;
		bool bValueArgument;
	};

	inline static constexpr FFailureCase FailureCases[] =
	{
		{ "none", false, false, false, false },
		{ "argument_evaluation", true, true, false, false },
		{ "body", true, false, true, false },
		{ "return_construction", true, false, false, true },
	};

	inline static constexpr FInitializedCase InitializedCases[] =
	{
		{ "zero", 0 },
		{ "one", 1 },
		{ "many", 4 },
	};

	inline static constexpr FTransferCase TransferCases[] =
	{
		{ "value_argument", true },
		{ "value_return", false },
	};

	static FString MakeSuffix(
		const FFailureCase& FailureCase,
		const FInitializedCase& InitializedCase,
		const FTransferCase& TransferCase)
	{
		return FString::Printf(
			TEXT("%hs_%hs_%hs"),
			FailureCase.CatalogName,
			InitializedCase.CatalogName,
			TransferCase.CatalogName);
	}

	static void AppendValueArgumentFunctions(FString& Source)
	{
		AppendGeneratedAsLine(Source, TEXT("FNativeCaseValue ProduceArgumentValue(int Seed, bool ArmCopyFault)"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tFNativeCaseValue Result(Seed);"));
		AppendGeneratedAsLine(Source, TEXT("\tif (ArmCopyFault)"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(Source, TEXT("\t\tArmNextNativeCaseValueCopyFault();"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source, TEXT("\treturn Result;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("int ConsumeValueArgument(FNativeCaseValue Value, int Divisor)"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\treturn Value.Value / Divisor;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
	}

	static void AppendValueReturnFunctions(FString& Source)
	{
		AppendGeneratedAsLine(Source, TEXT("FNativeCaseValue ProduceReturnValue(int Seed, int Divisor, bool ArmCopyFault)"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tFNativeCaseValue Result(Seed);"));
		AppendGeneratedAsLine(Source, TEXT("\tResult.Value = Result.Value / Divisor;"));
		AppendGeneratedAsLine(Source, TEXT("\tif (ArmCopyFault)"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(Source, TEXT("\t\tArmNextNativeCaseValueCopyFault();"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source, TEXT("\treturn Result;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
	}

	static void AppendInitializedValues(FString& Source, const FInitializedCase& InitializedCase)
	{
		for (int32 Index = 0; Index < InitializedCase.Count; ++Index)
		{
			AppendGeneratedAsLine(Source, FString::Printf(
				TEXT("\tFNativeCaseValue Initialized%d(%d);"),
				Index,
				100 + Index));
		}
		if (InitializedCase.Count > 0)
		{
			AppendGeneratedAsLine(Source);
		}
	}

	static void AppendValueArgumentEntry(
		FString& Source,
		const FFailureCase& FailureCase)
	{
		if (FailureCase.bArgumentEvaluation)
		{
			AppendGeneratedAsLine(Source, TEXT("\tint Zero = 0;"));
			AppendGeneratedAsLine(Source, TEXT("\treturn ConsumeValueArgument(FNativeCaseValue(41), 1 / Zero);"));
		}
		else if (FailureCase.bBody)
		{
			AppendGeneratedAsLine(Source, TEXT("\treturn ConsumeValueArgument(FNativeCaseValue(41), 0);"));
		}
		else if (FailureCase.bReturnConstruction)
		{
			AppendGeneratedAsLine(Source, TEXT("\treturn ConsumeValueArgument(ProduceArgumentValue(41, true), 1);"));
		}
		else
		{
			AppendGeneratedAsLine(Source, TEXT("\treturn ConsumeValueArgument(ProduceArgumentValue(41, false), 1);"));
		}
	}

	static void AppendValueReturnEntry(
		FString& Source,
		const FFailureCase& FailureCase)
	{
		if (FailureCase.bArgumentEvaluation)
		{
			AppendGeneratedAsLine(Source, TEXT("\tint Zero = 0;"));
			AppendGeneratedAsLine(Source, TEXT("\tFNativeCaseValue Value = ProduceReturnValue(41, 1 / Zero, false);"));
		}
		else if (FailureCase.bBody)
		{
			AppendGeneratedAsLine(Source, TEXT("\tFNativeCaseValue Value = ProduceReturnValue(41, 0, false);"));
		}
		else if (FailureCase.bReturnConstruction)
		{
			AppendGeneratedAsLine(Source, TEXT("\tFNativeCaseValue Value = ProduceReturnValue(41, 1, true);"));
		}
		else
		{
			AppendGeneratedAsLine(Source, TEXT("\tFNativeCaseValue Value = ProduceReturnValue(41, 1, false);"));
		}
		AppendGeneratedAsLine(Source, TEXT("\treturn Value.Value;"));
	}

	static FString BuildSource(
		const FFailureCase& FailureCase,
		const FInitializedCase& InitializedCase,
		const FTransferCase& TransferCase,
		const FString& Suffix)
	{
		FString Source;
		if (TransferCase.bValueArgument)
		{
			AppendValueArgumentFunctions(Source);
		}
		else
		{
			AppendValueReturnFunctions(Source);
		}

		AppendGeneratedAsLine(Source, FString::Printf(TEXT("int Run_%s()"), *Suffix));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendInitializedValues(Source, InitializedCase);
		if (TransferCase.bValueArgument)
		{
			AppendValueArgumentEntry(Source, FailureCase);
		}
		else
		{
			AppendValueReturnEntry(Source, FailureCase);
		}
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("int CleanAfterValueLifecycle()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\treturn 73;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		return Source;
	}

	static int32 CountEventsForValue(
		const FNativeLifecycleRecorder& Recorder,
		const ENativeLifecycleEvent Event,
		const int32 Value)
	{
		int32 Count = 0;
		for (const FNativeLifecycleEntry& Entry : Recorder.GetEntries())
		{
			if (Entry.Event == Event && Entry.Value == Value)
			{
				++Count;
			}
		}
		return Count;
	}

	static int32 FindEventIndexForValue(
		const FNativeLifecycleRecorder& Recorder,
		const ENativeLifecycleEvent Event,
		const int32 Value)
	{
		return Recorder.GetEntries().IndexOfByPredicate([Event, Value](const FNativeLifecycleEntry& Entry)
		{
			return Entry.Event == Event && Entry.Value == Value;
		});
	}

	void VerifyInitializedValueCleanup(
		const FNativeCaseContext& Case,
		const FInitializedCase& InitializedCase,
		const FNativeLifecycleRecorder& Recorder)
	{
		for (int32 Index = 0; Index < InitializedCase.Count; ++Index)
		{
			const int32 Value = 100 + Index;
			ASSERT_THAT(AreEqual(1, CountEventsForValue(Recorder, ENativeLifecycleEvent::ValueConstruct, Value),
				*Case.Describe(TEXT("each pre-failure sentinel should be constructed exactly once"))));
			ASSERT_THAT(AreEqual(0, CountEventsForValue(Recorder, ENativeLifecycleEvent::CopyConstruct, Value),
				*Case.Describe(TEXT("unused pre-failure sentinels should not be copied"))));
			ASSERT_THAT(AreEqual(0, CountEventsForValue(Recorder, ENativeLifecycleEvent::Assign, Value),
				*Case.Describe(TEXT("unused pre-failure sentinels should not be assigned"))));
			ASSERT_THAT(AreEqual(1, CountEventsForValue(Recorder, ENativeLifecycleEvent::Destruct, Value),
				*Case.Describe(TEXT("each pre-failure sentinel should be destroyed exactly once"))));
			ASSERT_THAT(IsTrue(
				FindEventIndexForValue(Recorder, ENativeLifecycleEvent::ValueConstruct, Value)
					< FindEventIndexForValue(Recorder, ENativeLifecycleEvent::Destruct, Value),
				*Case.Describe(TEXT("each pre-failure sentinel should be destroyed after its construction"))));
		}

		TArray<int32> SentinelDestructionOrder;
		for (const FNativeLifecycleEntry& Entry : Recorder.GetEntries())
		{
			if (Entry.Event == ENativeLifecycleEvent::Destruct
				&& Entry.Value >= 100
				&& Entry.Value < 100 + InitializedCase.Count)
			{
				SentinelDestructionOrder.Add(Entry.Value);
			}
		}
		ASSERT_THAT(AreEqual(InitializedCase.Count, SentinelDestructionOrder.Num(),
			*Case.Describe(TEXT("cleanup trace should contain every initialized sentinel"))));
		for (int32 Index = 0; Index < SentinelDestructionOrder.Num(); ++Index)
		{
			ASSERT_THAT(AreEqual(100 + InitializedCase.Count - Index - 1, SentinelDestructionOrder[Index],
				*Case.Describe(TEXT("initialized sentinels should unwind in reverse construction order"))));
		}
	}

	void VerifyTransferTrace(
		const FNativeCaseContext& Case,
		const FFailureCase& FailureCase,
		const FNativeLifecycleRecorder& Recorder)
	{
		const int32 TransferConstructCount = CountEventsForValue(
			Recorder,
			ENativeLifecycleEvent::ValueConstruct,
			41);
		const int32 TransferCopyCount = CountEventsForValue(
			Recorder,
			ENativeLifecycleEvent::CopyConstruct,
			41);

		if (FailureCase.bArgumentEvaluation)
		{
			ASSERT_THAT(IsTrue(TransferConstructCount == 0 || TransferConstructCount == 1,
				*Case.Describe(TEXT("argument evaluation should stop before or during the first transfer operand"))));
		}
		else
		{
			ASSERT_THAT(AreEqual(1, TransferConstructCount,
				*Case.Describe(TEXT("a reached value-transfer path should construct its source exactly once"))));
		}

		if (FailureCase.bReturnConstruction)
		{
			ASSERT_THAT(IsTrue(TransferCopyCount >= 1,
				*Case.Describe(TEXT("return-construction failure should record the faulting copy before the exception"))));
		}

		TSet<int32> ConstructedObjectIds;
		TSet<int32> DestructedObjectIds;
		for (const FNativeLifecycleEntry& Entry : Recorder.GetEntries())
		{
			if (Entry.Event == ENativeLifecycleEvent::DefaultConstruct
				|| Entry.Event == ENativeLifecycleEvent::ValueConstruct
				|| Entry.Event == ENativeLifecycleEvent::CopyConstruct)
			{
				ASSERT_THAT(IsFalse(ConstructedObjectIds.Contains(Entry.ObjectId),
					*Case.Describe(TEXT("each tracked construction should allocate a unique object identity"))));
				if (Entry.Event == ENativeLifecycleEvent::CopyConstruct)
				{
					ASSERT_THAT(IsTrue(ConstructedObjectIds.Contains(Entry.RelatedObjectId),
						*Case.Describe(TEXT("each copy should name an already constructed source object"))));
				}
				ConstructedObjectIds.Add(Entry.ObjectId);
			}
			else if (Entry.Event == ENativeLifecycleEvent::Destruct)
			{
				ASSERT_THAT(IsTrue(ConstructedObjectIds.Contains(Entry.ObjectId),
					*Case.Describe(TEXT("each destructor should name a constructed object"))));
				ASSERT_THAT(IsFalse(DestructedObjectIds.Contains(Entry.ObjectId),
					*Case.Describe(TEXT("each tracked object should be destroyed no more than once"))));
				DestructedObjectIds.Add(Entry.ObjectId);
			}
		}
		ASSERT_THAT(AreEqual(ConstructedObjectIds.Num(), DestructedObjectIds.Num(),
			*Case.Describe(TEXT("every constructed value should have one matching destructor"))));
	}

public:
	TEST_METHOD(FailuresByInitializedCountAndTransfer)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT("LANG-FN-VALUE-LIFECYCLE",
			ENativeEvidence::Runtime
			| ENativeEvidence::Diagnostic
			| ENativeEvidence::Lifecycle
			| ENativeEvidence::Debug
			| ENativeEvidence::Cleanup);

		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};

		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine,
			TEXT("Function value-lifecycle product should create a raw SDK engine")));
		if (ScriptEngine == nullptr)
		{
			return;
		}

		FNativeLifecycleRecorder Lifecycle;
		FNativeLifecycleFaultController FaultController;
		Lifecycle.Reset();
		FaultController.Reset();
		ASSERT_THAT(IsTrue(RegisterNativeCaseValue(*ScriptEngine, Lifecycle, &FaultController),
			TEXT("Function value-lifecycle product should register its fault-injectable tracked value type")));

		for (const FFailureCase& FailureCase : FailureCases)
		{
			for (const FInitializedCase& InitializedCase : InitializedCases)
			{
				for (const FTransferCase& TransferCase : TransferCases)
				{
					Lifecycle.Reset();
					FaultController.Reset();
					const FNativeCaseContext Case(MakeNativeCaseId(
						"LANG-FN-VALUE-LIFECYCLE",
						{
							ANSI_TO_TCHAR(FailureCase.CatalogName),
							ANSI_TO_TCHAR(InitializedCase.CatalogName),
							ANSI_TO_TCHAR(TransferCase.CatalogName),
						}));
					const FString Suffix = MakeSuffix(FailureCase, InitializedCase, TransferCase);
					const FString ModuleName = TEXT("FunctionValueLifecycle_") + Suffix;
					const FString Source = BuildSource(FailureCase, InitializedCase, TransferCase, Suffix);
					PrintGeneratedAsSource(*TestRunner, Case.GetId(), ModuleName, Source);
					const FTCHARToUTF8 ModuleNameUtf8(*ModuleName);
					const FTCHARToUTF8 SourceUtf8(*Source);
					Engine.ResetMessages();
					asIScriptModule* Module = nullptr;
					const int BuildResult = CompileNativeModule(
						ScriptEngine,
						ModuleNameUtf8.Get(),
						SourceUtf8.Get(),
						Module);
					ASSERT_THAT(IsTrue(BuildResult >= 0,
						*Case.Describe(TEXT("value-lifecycle cell should compile"))));
					ASSERT_THAT(IsNotNull(Module,
						*Case.Describe(TEXT("value-lifecycle cell should publish a module"))));
					if (BuildResult >= 0 && Module != nullptr)
					{
						const FString EntryDeclaration = FString::Printf(TEXT("int Run_%s()"), *Suffix);
						asIScriptFunction* const Entry = GetNativeFunctionByExactDecl(
							Module,
							TCHAR_TO_ANSI(*EntryDeclaration));
						asIScriptFunction* const Clean = GetNativeFunctionByExactDecl(
							Module,
							"int CleanAfterValueLifecycle()");
						ASSERT_THAT(IsNotNull(Entry,
							*Case.Describe(TEXT("value-lifecycle entry should resolve by exact declaration"))));
						ASSERT_THAT(IsNotNull(Clean,
							*Case.Describe(TEXT("value-lifecycle cell should expose its context-reuse probe"))));
						if (Entry != nullptr && Clean != nullptr)
						{
							asIScriptContext* const Context = ScriptEngine->CreateContext();
							ASSERT_THAT(IsNotNull(Context,
								*Case.Describe(TEXT("value-lifecycle cell should create an execution context"))));
							if (Context != nullptr)
							{
								const int ExecuteResult = PrepareAndExecute(Context, Entry);
								if (FailureCase.bException)
								{
									ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_EXCEPTION), ExecuteResult,
										*Case.Describe(TEXT("configured failure stage should stop with an execution exception"))));
									ASSERT_THAT(IsTrue(Context->GetExceptionString() != nullptr && Context->GetExceptionString()[0] != '\0',
										*Case.Describe(TEXT("configured failure should expose exception text"))));
									ASSERT_THAT(IsNotNull(Context->GetExceptionFunction(),
										*Case.Describe(TEXT("configured failure should identify the throwing script frame"))));
									ASSERT_THAT(IsTrue(Context->GetCallstackSize() > 0,
										*Case.Describe(TEXT("configured failure should retain callstack metadata before cleanup"))));
									const char* ExceptionSection = nullptr;
									int ExceptionColumn = INDEX_NONE;
									ASSERT_THAT(IsTrue(Context->GetExceptionLineNumber(&ExceptionColumn, &ExceptionSection) > 0,
										*Case.Describe(TEXT("configured failure should report a one-based script line"))));
									ASSERT_THAT(AreEqual(ModuleName, FString(UTF8_TO_TCHAR(ExceptionSection != nullptr ? ExceptionSection : "")),
										*Case.Describe(TEXT("configured failure should report the generated module section"))));
									ASSERT_THAT(IsTrue(ExceptionColumn > 0,
										*Case.Describe(TEXT("configured failure should report a one-based source column"))));
								}
								else
								{
									ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_FINISHED), ExecuteResult,
										*Case.Describe(TEXT("non-failure value transfer should finish"))));
									if (ExecuteResult == asEXECUTION_FINISHED)
									{
										ASSERT_THAT(AreEqual(41, static_cast<int32>(Context->GetReturnDWord()),
											*Case.Describe(TEXT("non-failure transfer should preserve the tracked value"))));
									}
								}

								if (FailureCase.bReturnConstruction)
								{
									ASSERT_THAT(AreEqual(1, FaultController.GetTriggeredCopyCount(),
										*Case.Describe(TEXT("return construction should trigger exactly one armed native copy fault"))));
									ASSERT_THAT(IsFalse(FaultController.IsArmed(),
										*Case.Describe(TEXT("faulting return copy should consume the one-shot fault"))));
									ASSERT_THAT(AreEqual(
										FString(TEXT("Native case value copy construction fault")),
										FString(UTF8_TO_TCHAR(Context->GetExceptionString() != nullptr ? Context->GetExceptionString() : "")),
										*Case.Describe(TEXT("return construction should expose the native copy fault text"))));
								}
								else
								{
									ASSERT_THAT(AreEqual(0, FaultController.GetTriggeredCopyCount(),
										*Case.Describe(TEXT("non-return-fault cells should never consume a copy fault"))));
								}

								ASSERT_THAT(AreEqual(asSUCCESS, Context->Unprepare(),
									*Case.Describe(TEXT("value-lifecycle context should unprepare after return or exception"))));
								ASSERT_THAT(AreEqual(0, Lifecycle.GetLiveObjectCount(),
									*Case.Describe(TEXT("context cleanup should leave no live tracked values"))));
								VerifyInitializedValueCleanup(Case, InitializedCase, Lifecycle);
								VerifyTransferTrace(Case, FailureCase, Lifecycle);

								ASSERT_THAT(AreEqual(asSUCCESS, Context->Prepare(Clean),
									*Case.Describe(TEXT("clean follow-up should prepare on the same context"))));
								ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_FINISHED), Context->Execute(),
									*Case.Describe(TEXT("clean follow-up should execute after value cleanup"))));
								ASSERT_THAT(AreEqual(73, static_cast<int32>(Context->GetReturnDWord()),
									*Case.Describe(TEXT("reused context should not retain stale return or exception state"))));
								Context->Release();
							}
						}
					}

					ScriptEngine->DiscardModule(ModuleNameUtf8.Get());
					ASSERT_THAT(IsNull(ScriptEngine->GetModule(ModuleNameUtf8.Get(), asGM_ONLY_IF_EXISTS),
						*Case.Describe(TEXT("value-lifecycle cell should discard its isolated module"))));
				}
			}
		}
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
