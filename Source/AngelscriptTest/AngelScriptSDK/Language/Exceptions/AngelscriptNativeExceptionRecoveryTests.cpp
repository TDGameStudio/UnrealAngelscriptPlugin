#include "../../Support/AngelscriptNativeCaseTestSupport.h"
#include "../../Support/AngelscriptNativeCoreTestSupport.h"
#include "../../Support/AngelscriptNativeExecutionTestSupport.h"
#include "../../Support/AngelscriptNativeFixtureTestSupport.h"
#include "../../Support/AngelscriptNativeLanguageCaseTestSupport.h"

#include "CQTest.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FExceptionRecoveryTests,
	"Angelscript.TestModule.AngelScriptSDK.Language.Exceptions.Recovery",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	struct FNamedCase
	{
		const ANSICHAR* CatalogName;
	};

	inline static constexpr FNamedCase LiveStateCases[] =
	{
		{ "none" }, { "locals" }, { "nested_scopes" }, { "arguments" }, { "partial_construction" }, { "base_members" }, { "iterator" }, { "reference" },
	};
	inline static constexpr FNamedCase FollowUpCases[] =
	{
		{ "unprepare" }, { "same_function" }, { "different_arity" }, { "different_type" }, { "different_return" }, { "rebuild" }, { "release" }, { "clean_execute" },
	};
	inline static constexpr FNamedCase NestedStateCases[] =
	{
		{ "none" }, { "one" }, { "two" },
	};


	static bool IsNamedCase(const FNamedCase& Case, const ANSICHAR* Name)
	{
		return FCStringAnsi::Strcmp(Case.CatalogName, Name) == 0;
	}

	static const char* GetFollowUpDeclaration(const FNamedCase& FollowUpCase)
	{
		if (IsNamedCase(FollowUpCase, "different_arity"))
		{
			return "int RecoveryWithArg(const int)";
		}
		if (IsNamedCase(FollowUpCase, "different_type"))
		{
			return "int RecoveryWithBool(const bool)";
		}
		if (IsNamedCase(FollowUpCase, "different_return"))
		{
			return "bool RecoveryBool()";
		}
		if (IsNamedCase(FollowUpCase, "clean_execute"))
		{
			return "int CleanExecute()";
		}
		if (IsNamedCase(FollowUpCase, "rebuild"))
		{
			return "int RecoveryAfterRebuild()";
		}
		return "int RecoverySame()";
	}

	static FString BuildExceptionRecoverySource(
		const FNamedCase& LiveStateCase,
		const FNamedCase& FollowUpCase,
		const FNamedCase& NestedStateCase)
	{
		FString Source;
		AppendGeneratedAsLine(Source, TEXT("void RaiseCleanup()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tRaiseNativeCaseException();"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("void NestOne()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tRaiseCleanup();"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("void NestTwo()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tNestOne();"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("void ConsumeValue(FNativeCaseValue Value)"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("struct FRecoveryAggregate"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tFNativeCaseValue First;"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("\tFNativeCaseValue Second;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("void FailingEntry()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		if (IsNamedCase(LiveStateCase, "locals"))
		{
			AppendGeneratedAsLine(Source, TEXT("\tFNativeCaseValue Value(7);"));
		}
		else if (IsNamedCase(LiveStateCase, "nested_scopes"))
		{
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t\tFNativeCaseValue Outer(7);"));
			AppendGeneratedAsLine(Source, TEXT("\t\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t\t\tFNativeCaseValue Inner(8);"));
			AppendGeneratedAsLine(Source, TEXT("\t\t\tRaiseCleanup();"));
			AppendGeneratedAsLine(Source, TEXT("\t\t}"));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
		}
		else if (IsNamedCase(LiveStateCase, "arguments"))
		{
			AppendGeneratedAsLine(Source, TEXT("\tConsumeValue(FNativeCaseValue(7));"));
		}
		else if (IsNamedCase(LiveStateCase, "partial_construction"))
		{
			AppendGeneratedAsLine(Source, TEXT("\tFNativeCaseValue First(7);"));
			AppendGeneratedAsLine(Source, TEXT("\tArmNextNativeCaseValueCopyFault();"));
			AppendGeneratedAsLine(Source, TEXT("\tFNativeCaseValue Second(First);"));
		}
		else if (IsNamedCase(LiveStateCase, "base_members"))
		{
			AppendGeneratedAsLine(Source, TEXT("\tFRecoveryAggregate Aggregate;"));
			AppendGeneratedAsLine(Source, TEXT("\tAggregate.First.Value = 7;"));
		}
		else if (IsNamedCase(LiveStateCase, "iterator"))
		{
			AppendGeneratedAsLine(Source, TEXT("\tFNativeCaseRange Range;"));
			AppendGeneratedAsLine(Source, TEXT("\tRange.Count = 2;"));
			AppendGeneratedAsLine(Source, TEXT("\tforeach (int Value : Range)"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t\tif (Value == 0)"));
			AppendGeneratedAsLine(Source, TEXT("\t\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t\t\tRaiseCleanup();"));
			AppendGeneratedAsLine(Source, TEXT("\t\t}"));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
		}
		else if (IsNamedCase(LiveStateCase, "reference"))
		{
			AppendGeneratedAsLine(Source, TEXT("\tFNativeCaseReference Value = CreateNativeCaseReference(7);"));
			AppendGeneratedAsLine(Source, TEXT("\tValue.Value += 1;"));
		}
		if (FCStringAnsi::Strcmp(NestedStateCase.CatalogName, "two") == 0)
		{
			AppendGeneratedAsLine(Source, TEXT("\tNestTwo();"));
		}
		else if (FCStringAnsi::Strcmp(NestedStateCase.CatalogName, "one") == 0)
		{
			AppendGeneratedAsLine(Source, TEXT("\tNestOne();"));
		}
		else
		{
			AppendGeneratedAsLine(Source, TEXT("\tRaiseCleanup();"));
		}
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("int RecoverySame()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\treturn 91;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("int RecoveryWithArg(int Value)"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\treturn 82 + Value;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("int RecoveryWithBool(bool Value)"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\treturn Value ? 91 : 0;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("bool RecoveryBool()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\treturn true;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("int CleanExecute()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\treturn 91;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("int RecoveryAfterRebuild()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\treturn 91;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		return Source;
	}

public:
	TEST_METHOD(LiveStatesByFollowUpAndNesting)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT("LANG-EX-CLEANUP-REUSE",
			ENativeEvidence::Runtime
			| ENativeEvidence::Lifecycle
			| ENativeEvidence::Debug
			| ENativeEvidence::Cleanup);

		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};
		FNoDiscardAsserter Assertions(*TestRunner);

		asIScriptEngine* const ScriptEngine = Engine.Get();
		if (!Assertions.IsNotNull(ScriptEngine, TEXT("Exception recovery product should create a raw SDK engine")))
		{
			return;
		}

		FNativeLifecycleRecorder Lifecycle;
		FNativeLifecycleFaultController Faults;
		Lifecycle.Reset();
		if (!Assertions.IsTrue(RegisterNativeCaseValue(*ScriptEngine, Lifecycle, &Faults), TEXT("Exception recovery should register the tracked native value fixture"))
			|| !Assertions.IsTrue(RegisterNativeCaseReference(*ScriptEngine, &Lifecycle), TEXT("Exception recovery should register the tracked native reference fixture"))
			|| !Assertions.IsTrue(RegisterNativeCaseRange(*ScriptEngine, Lifecycle), TEXT("Exception recovery should register the tracked native iterator fixture")))
		{
			return;
		}

		for (const FNamedCase& LiveStateCase : LiveStateCases)
		{
			for (const FNamedCase& FollowUpCase : FollowUpCases)
			{
				for (const FNamedCase& NestedStateCase : NestedStateCases)
				{
					Lifecycle.Reset();
					Faults.Reset();
					const FNativeCaseContext Case(MakeNativeCaseId("LANG-EX-CLEANUP-REUSE",
						{ ANSI_TO_TCHAR(LiveStateCase.CatalogName), ANSI_TO_TCHAR(FollowUpCase.CatalogName), ANSI_TO_TCHAR(NestedStateCase.CatalogName) }));
					const FString ModuleName = TEXT("ExceptionRecovery_") + Case.GetId().RightChop(20).Replace(TEXT("-"), TEXT("_"));
					const FString Source = BuildExceptionRecoverySource(LiveStateCase, FollowUpCase, NestedStateCase);
					PrintGeneratedAsSource(*TestRunner, Case.GetId(), ModuleName, Source);
					const FTCHARToUTF8 ModuleNameUtf8(*ModuleName);
					const FTCHARToUTF8 SourceUtf8(*Source);
					Engine.ResetMessages();
					asIScriptModule* Module = nullptr;
					const int BuildResult = CompileNativeModule(ScriptEngine, ModuleNameUtf8.Get(), SourceUtf8.Get(), Module);
					const FString BuildDescription = FString::Printf(TEXT("%s; result=%d messages=%s"),
						*Case.Describe(TEXT("exception recovery cell should compile")), BuildResult, *Engine.GetMessagesText());
					const bool bBuildSucceeded = Assertions.AreEqual(asSUCCESS, BuildResult, *BuildDescription);
					asIScriptFunction* FailingEntry = nullptr;
					asIScriptFunction* Recovery = nullptr;
					bool bDeclarationsAvailable = false;
					if (bBuildSucceeded)
					{
						FailingEntry = GetNativeFunctionByExactDecl(Module, "void FailingEntry()");
						Recovery = GetNativeFunctionByExactDecl(Module, GetFollowUpDeclaration(FollowUpCase));
						const bool bFailingEntryAvailable = Assertions.IsNotNull(FailingEntry, *Case.Describe(TEXT("exception recovery should publish exact failing declaration")));
						const bool bRecoveryAvailable = Assertions.IsNotNull(Recovery, *Case.Describe(TEXT("exception recovery should publish the selected follow-up declaration")));
						bDeclarationsAvailable = bFailingEntryAvailable && bRecoveryAvailable;
					}
					if (bBuildSucceeded && bDeclarationsAvailable)
					{
						asIScriptContext* const Context = ScriptEngine->CreateContext();
						const bool bContextAvailable = Assertions.IsNotNull(Context, *Case.Describe(TEXT("exception recovery should create a context")));
						if (bContextAvailable)
						{
							const bool bFailureExecuted = Assertions.AreEqual(asEXECUTION_EXCEPTION, PrepareAndExecute(Context, FailingEntry), *Case.Describe(TEXT("failing path should raise its exception")));
							(void)Assertions.IsNotNull(Context->GetExceptionFunction(), *Case.Describe(TEXT("failing path should retain exception function metadata")));
							asIScriptContext* RecoveryContext = Context;
							asIScriptFunction* SelectedRecovery = Recovery;
							bool bSelectedRecoveryReady = bFailureExecuted;
							if (IsNamedCase(FollowUpCase, "rebuild"))
							{
								bSelectedRecoveryReady &= Assertions.AreEqual(asSUCCESS, Context->Unprepare(), *Case.Describe(TEXT("failing rebuild path should unprepare before module replacement")));
								Context->Release();
								ScriptEngine->DiscardModule(ModuleNameUtf8.Get());
								Module = nullptr;
								Engine.ResetMessages();
								const int RebuildResult = CompileNativeModule(ScriptEngine, ModuleNameUtf8.Get(), SourceUtf8.Get(), Module);
								bSelectedRecoveryReady &= Assertions.AreEqual(asSUCCESS, RebuildResult, *FString::Printf(TEXT("%s; result=%d messages=%s"), *Case.Describe(TEXT("rebuild follow-up should compile the same printed source under the discarded module name")), RebuildResult, *Engine.GetMessagesText()));
								SelectedRecovery = GetNativeFunctionByExactDecl(Module, GetFollowUpDeclaration(FollowUpCase));
								bSelectedRecoveryReady &= Assertions.IsNotNull(SelectedRecovery, *Case.Describe(TEXT("rebuild follow-up should publish its exact recovery declaration")));
								RecoveryContext = ScriptEngine->CreateContext();
							}
							else if (IsNamedCase(FollowUpCase, "release"))
							{
								Context->Release();
								RecoveryContext = ScriptEngine->CreateContext();
							}
							else
							{
								bSelectedRecoveryReady &= Assertions.AreEqual(asSUCCESS, Context->Unprepare(), *Case.Describe(TEXT("failing path should unprepare before selected context reuse")));
							}

							bSelectedRecoveryReady &= Assertions.IsNotNull(RecoveryContext, *Case.Describe(TEXT("selected recovery path should own a valid context")));
							if (bSelectedRecoveryReady && RecoveryContext != nullptr && SelectedRecovery != nullptr)
							{
								const bool bPrepared = Assertions.AreEqual(asSUCCESS, RecoveryContext->Prepare(SelectedRecovery), *Case.Describe(TEXT("selected recovery context should prepare its exact follow-up declaration")));
								if (IsNamedCase(FollowUpCase, "different_arity"))
								{
									(void)Assertions.AreEqual(asSUCCESS, RecoveryContext->SetArgDWord(0, 9), *Case.Describe(TEXT("different-arity recovery should receive its integer argument")));
								}
								else if (IsNamedCase(FollowUpCase, "different_type"))
								{
									(void)Assertions.AreEqual(asSUCCESS, RecoveryContext->SetArgByte(0, 1), *Case.Describe(TEXT("different-type recovery should receive its boolean argument")));
								}
								if (bPrepared)
								{
									const bool bRecoveryExecuted = Assertions.AreEqual(asEXECUTION_FINISHED, RecoveryContext->Execute(), *Case.Describe(TEXT("selected follow-up should execute after exception cleanup")));
									if (bRecoveryExecuted && IsNamedCase(FollowUpCase, "different_return"))
									{
										(void)Assertions.IsTrue(RecoveryContext->GetReturnByte() != 0, *Case.Describe(TEXT("different-return recovery should expose its boolean result")));
									}
									else if (bRecoveryExecuted)
									{
										(void)Assertions.AreEqual(91, static_cast<int32>(RecoveryContext->GetReturnDWord()), *Case.Describe(TEXT("selected recovery should not retain stale return storage")));
									}
								}
								RecoveryContext->Release();
							}
						}
					}
					ScriptEngine->DiscardModule(ModuleNameUtf8.Get());
					(void)Assertions.IsNull(ScriptEngine->GetModule(ModuleNameUtf8.Get(), asGM_ONLY_IF_EXISTS), *Case.Describe(TEXT("exception recovery cell should discard its module")));
					(void)Assertions.AreEqual(0, Lifecycle.GetLiveObjectCount(), *Case.Describe(TEXT("exception cleanup should release all tracked native values")));
				}
			}
		}
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
