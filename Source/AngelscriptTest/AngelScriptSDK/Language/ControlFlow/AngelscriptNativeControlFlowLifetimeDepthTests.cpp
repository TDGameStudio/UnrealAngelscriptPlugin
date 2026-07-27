#include "../../Support/AngelscriptNativeCaseTestSupport.h"
#include "../../Support/AngelscriptNativeCoreTestSupport.h"
#include "../../Support/AngelscriptNativeFixtureTestSupport.h"
#include "../../Support/AngelscriptNativeLanguageCaseTestSupport.h"

#include "CQTest.h"

using AngelscriptNativeTestSupport::AppendGeneratedAsLine;

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FControlFlowLifetimeDepthTests,
	"Angelscript.TestModule.AngelScriptSDK.Language.ControlFlow.LifetimeDepth",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	using ENativeLifecycleEvent = AngelscriptNativeTestSupport::ENativeLifecycleEvent;
	using FNativeCaseContext = AngelscriptNativeTestSupport::FNativeCaseContext;
	using FNativeLifecycleEntry = AngelscriptNativeTestSupport::FNativeLifecycleEntry;
	using FNativeLifecycleRecorder = AngelscriptNativeTestSupport::FNativeLifecycleRecorder;

	struct FNamedCase
	{
		const ANSICHAR* CatalogName;
		int32 Weight = 0;
	};

	struct FTraceResult
	{
		int32 Value = 0;
		bool bReturned = false;
	};

	inline static constexpr FNamedCase ScopeCases[] =
	{
		{ "loop", 1 },
		{ "branch_loop", 2 },
		{ "nested_loop", 3 },
		{ "switch_loop", 4 },
	};

	inline static constexpr FNamedCase ExitCases[] =
	{
		{ "normal", 0 },
		{ "break", 7 },
		{ "continue", 11 },
		{ "return", 13 },
		{ "exception", 17 },
	};

	inline static constexpr FNamedCase DepthCases[] =
	{
		{ "one", 1 },
		{ "two", 2 },
		{ "three", 3 },
	};

	inline static constexpr FNamedCase LocalCountCases[] =
	{
		{ "one", 1 },
		{ "two", 2 },
	};

	inline static constexpr FNamedCase LineEndingCases[] =
	{
		{ "lf", 0 },
		{ "crlf", 1 },
	};


	static bool IsCase(const FNamedCase& Case, const ANSICHAR* Name)
	{
		return FCStringAnsi::Strcmp(Case.CatalogName, Name) == 0;
	}

	static int32 GetDepth(const FNamedCase& DepthCase)
	{
		return DepthCase.Weight;
	}

	static int32 GetLocalCount(const FNamedCase& LocalCountCase)
	{
		return LocalCountCase.Weight;
	}

	static int32 MakeLocalValue(const int32 Level, const int32 Slot)
	{
		return (Level + 1) * 100 + Slot + 1;
	}

	static void AppendProbeDeclarations(FString& Source)
	{
		AppendGeneratedAsLine(Source, TEXT("struct FScopedLifetimeProbe"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tint Value = 0;"));
		AppendGeneratedAsLine(Source, TEXT("\tint ObjectId = 0;"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("\tFScopedLifetimeProbe(int InValue)"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(Source, TEXT("\t\tValue = InValue;"));
		AppendGeneratedAsLine(Source, TEXT("\t\tObjectId = BeginNativeScriptLifecycle(Value);"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("\t~FScopedLifetimeProbe()"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(Source, TEXT("\t\tEndNativeScriptLifecycle(ObjectId, Value);"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
	}

	static void AppendLocalDeclarations(
		FString& Source,
		const int32 Level,
		const int32 LocalCount,
		const FString& Indent)
	{
		for (int32 Slot = 0; Slot < LocalCount; ++Slot)
		{
			AppendGeneratedAsLine(
				Source,
				FString::Printf(
					TEXT("%sFScopedLifetimeProbe Local_%d_%d(%d);"),
					*Indent,
					Level + 1,
					Slot + 1,
					MakeLocalValue(Level, Slot)));
		}
	}

	static void AppendDeepestExit(
		FString& Source,
		const FNamedCase& ExitCase,
		const FString& Indent)
	{
		if (IsCase(ExitCase, "break"))
		{
			AppendGeneratedAsLine(Source, Indent + TEXT("Trace += 7;"));
			AppendGeneratedAsLine(Source, Indent + TEXT("break;"));
		}
		else if (IsCase(ExitCase, "continue"))
		{
			AppendGeneratedAsLine(Source, Indent + TEXT("Trace += 11;"));
			AppendGeneratedAsLine(Source, Indent + TEXT("continue;"));
		}
		else if (IsCase(ExitCase, "return"))
		{
			AppendGeneratedAsLine(Source, Indent + TEXT("Trace += 13;"));
			AppendGeneratedAsLine(Source, Indent + TEXT("return Trace;"));
		}
		else if (IsCase(ExitCase, "exception"))
		{
			AppendGeneratedAsLine(Source, Indent + TEXT("Trace += 17;"));
			AppendGeneratedAsLine(Source, Indent + TEXT("int ExceptionDivisor = 0;"));
			AppendGeneratedAsLine(Source, Indent + TEXT("Trace += 1 / ExceptionDivisor;"));
		}
	}

	static void AppendNestedScopes(
		FString& Source,
		const int32 CurrentLevel,
		const int32 Depth,
		const int32 LocalCount,
		const FNamedCase& ExitCase,
		const FString& Indent)
	{
		AppendGeneratedAsLine(
			Source,
			FString::Printf(
				TEXT("%sfor (int Level%d = 0; Level%d < 1; ++Level%d)"),
				*Indent,
				CurrentLevel + 1,
				CurrentLevel + 1,
				CurrentLevel + 1));
		AppendGeneratedAsLine(Source, Indent + TEXT("{"));

		const FString BodyIndent = Indent + TEXT("\t");
		AppendLocalDeclarations(Source, CurrentLevel, LocalCount, BodyIndent);
		AppendGeneratedAsLine(
			Source,
			FString::Printf(
				TEXT("%sTrace += %d;"),
				*BodyIndent,
				(CurrentLevel + 1) * 100));

		if (CurrentLevel + 1 == Depth)
		{
			AppendDeepestExit(Source, ExitCase, BodyIndent);
		}
		else
		{
			AppendNestedScopes(
				Source,
				CurrentLevel + 1,
				Depth,
				LocalCount,
				ExitCase,
				BodyIndent);
		}

		AppendGeneratedAsLine(Source, Indent + TEXT("}"));
		AppendGeneratedAsLine(
			Source,
			FString::Printf(
				TEXT("%sTrace += %d;"),
				*Indent,
				(CurrentLevel + 1) * 1000));
	}

	static void AppendShapeWrapper(
		FString& Source,
		const FNamedCase& ScopeCase,
		const int32 Depth,
		const int32 LocalCount,
		const FNamedCase& ExitCase)
	{
		if (IsCase(ScopeCase, "loop"))
		{
			AppendNestedScopes(Source, 0, Depth, LocalCount, ExitCase, TEXT("\t"));
			return;
		}

		if (IsCase(ScopeCase, "branch_loop"))
		{
			AppendGeneratedAsLine(Source, TEXT("\tif (true)"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendNestedScopes(Source, 0, Depth, LocalCount, ExitCase, TEXT("\t\t"));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
			return;
		}

		if (IsCase(ScopeCase, "nested_loop"))
		{
			AppendGeneratedAsLine(Source, TEXT("\tfor (int ShapeLoop = 0; ShapeLoop < 1; ++ShapeLoop)"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t\tif (ShapeLoop >= 0)"));
			AppendGeneratedAsLine(Source, TEXT("\t\t{"));
			AppendNestedScopes(Source, 0, Depth, LocalCount, ExitCase, TEXT("\t\t\t"));
			AppendGeneratedAsLine(Source, TEXT("\t\t}"));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
			return;
		}

		AppendGeneratedAsLine(Source, TEXT("\tswitch (0)"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(Source, TEXT("\tcase 0:"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendNestedScopes(Source, 0, Depth, LocalCount, ExitCase, TEXT("\t\t"));
		AppendGeneratedAsLine(Source, TEXT("\t\tbreak;"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source, TEXT("\tdefault:"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(Source, TEXT("\t\tbreak;"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
	}

	static FString BuildSource(
		const FNamedCase& ScopeCase,
		const FNamedCase& ExitCase,
		const FNamedCase& DepthCase,
		const FNamedCase& LocalCountCase,
		const FNamedCase& LineEndingCase)
	{
		const int32 Depth = GetDepth(DepthCase);
		const int32 LocalCount = GetLocalCount(LocalCountCase);
		FString Source;
		AppendGeneratedAsLine(Source, FString::Printf(
			TEXT("// scope=%hs exit=%hs depth=%hs locals=%hs layout=%hs"),
			ScopeCase.CatalogName,
			ExitCase.CatalogName,
			DepthCase.CatalogName,
			LocalCountCase.CatalogName,
			LineEndingCase.CatalogName));
		AppendProbeDeclarations(Source);
		AppendGeneratedAsLine(Source, TEXT("int Entry()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tint Trace = 0;"));
		AppendShapeWrapper(Source, ScopeCase, Depth, LocalCount, ExitCase);
		AppendGeneratedAsLine(Source, TEXT("\treturn Trace + 97;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("int Recovery()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\treturn 97;"));
		AppendGeneratedAsLine(Source, TEXT("}"));

		if (IsCase(LineEndingCase, "crlf"))
		{
			const FString LineFeed = FString::Chr(10);
			const FString CarriageReturnLineFeed = FString::Chr(13) + FString::Chr(10);
			Source.ReplaceInline(*LineFeed, *CarriageReturnLineFeed);
		}
		return Source;
	}

	static FTraceResult SimulateNestedScopes(
		const int32 CurrentLevel,
		const int32 Depth,
		const FNamedCase& ExitCase)
	{
		FTraceResult Result;
		Result.Value += (CurrentLevel + 1) * 100;
		if (CurrentLevel + 1 == Depth)
		{
			if (IsCase(ExitCase, "return"))
			{
				Result.Value += 13;
				Result.bReturned = true;
				return Result;
			}
			if (IsCase(ExitCase, "break"))
			{
				Result.Value += 7;
			}
			else if (IsCase(ExitCase, "continue"))
			{
				Result.Value += 11;
			}
			else if (IsCase(ExitCase, "exception"))
			{
				Result.Value += 17;
				return Result;
			}
		}
		else
		{
			const FTraceResult Nested = SimulateNestedScopes(CurrentLevel + 1, Depth, ExitCase);
			Result.Value += Nested.Value;
			if (Nested.bReturned)
			{
				Result.bReturned = true;
				return Result;
			}
		}
		Result.Value += (CurrentLevel + 1) * 1000;
		return Result;
	}

	static int32 ExpectedTrace(
		const FNamedCase& ExitCase,
		const FNamedCase& DepthCase)
	{
		const FTraceResult Nested = SimulateNestedScopes(0, GetDepth(DepthCase), ExitCase);
		return Nested.bReturned ? Nested.Value : Nested.Value + 97;
	}

	static bool VerifyLifecycle(
		FAutomationTestBase& Test,
		const FNativeCaseContext& Case,
		const FNativeLifecycleRecorder& Recorder,
		const int32 Depth,
		const int32 LocalCount)
	{
		FNoDiscardAsserter Assert(Test);
		const int32 ExpectedObjectCount = Depth * LocalCount;
		bool bValid = true;
		bValid &= Recorder.Num(ENativeLifecycleEvent::ValueConstruct) == ExpectedObjectCount;
		bValid &= Recorder.Num(ENativeLifecycleEvent::Destruct) == ExpectedObjectCount;
		bValid &= Recorder.GetLiveObjectCount() == 0;
		(void)Assert.AreEqual(ExpectedObjectCount,
			Recorder.Num(ENativeLifecycleEvent::ValueConstruct),
			*Case.Describe(TEXT("every generated local should construct exactly once")));
		(void)Assert.AreEqual(ExpectedObjectCount,
			Recorder.Num(ENativeLifecycleEvent::Destruct),
			*Case.Describe(TEXT("every generated local should destruct exactly once")));
		(void)Assert.AreEqual(0, Recorder.GetLiveObjectCount(),
			*Case.Describe(TEXT("all control-flow exits should return the live count to zero")));

		TArray<int32> ConstructValues;
		TArray<int32> DestructValues;
		TSet<int32> ConstructIds;
		TSet<int32> DestructIds;
		for (const FNativeLifecycleEntry& Entry : Recorder.GetEntries())
		{
			if (Entry.Event == ENativeLifecycleEvent::ValueConstruct)
			{
				ConstructValues.Add(Entry.Value);
				bValid &= !ConstructIds.Contains(Entry.ObjectId);
				ConstructIds.Add(Entry.ObjectId);
			}
			else if (Entry.Event == ENativeLifecycleEvent::Destruct)
			{
				DestructValues.Add(Entry.Value);
				bValid &= !DestructIds.Contains(Entry.ObjectId);
				bValid &= ConstructIds.Contains(Entry.ObjectId);
				DestructIds.Add(Entry.ObjectId);
			}
			else
			{
				bValid = false;
			}
		}

		for (int32 Index = 0; Index < ConstructValues.Num(); ++Index)
		{
			const int32 ExpectedValue = MakeLocalValue(Index / LocalCount, Index % LocalCount);
			bValid &= ConstructValues[Index] == ExpectedValue;
		}
		for (int32 Index = 0; Index < DestructValues.Num(); ++Index)
		{
			bValid &= DestructValues[Index] == ConstructValues[ConstructValues.Num() - 1 - Index];
		}
		bValid &= ConstructIds.Num() == ExpectedObjectCount;
		bValid &= DestructIds.Num() == ExpectedObjectCount;
		const FString LifecycleDescription = FString::Printf(
			TEXT("lifecycle entries should be unique and reverse ordered: %s"),
			*CollectNativeLifecycleEntries(Recorder));
		(void)Assert.IsTrue(bValid, *Case.Describe(*LifecycleDescription));
		return bValid;
	}

public:
	TEST_METHOD(LocalsByScopeExitDepthAndLayout)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT("LANG-CF-LIVE-LOCAL-CLEANUP",
			ENativeEvidence::Compile
			| ENativeEvidence::Runtime
			| ENativeEvidence::Diagnostic
			| ENativeEvidence::Lifecycle
			| ENativeEvidence::Cleanup
			| ENativeEvidence::Isolation);

		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};

		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine,
			TEXT("control-flow lifetime product should create a raw SDK engine")));
		if (ScriptEngine == nullptr)
		{
			return;
		}

		FNativeLifecycleRecorder BridgeRecorder;
		BridgeRecorder.Reset();
		ASSERT_THAT(IsTrue(RegisterNativeScriptLifecycleBridge(*ScriptEngine, BridgeRecorder),
			TEXT("lifetime product should register its lifecycle bridge once per engine")));

		int32 ObservedCaseCount = 0;
		for (const FNamedCase& ScopeCase : ScopeCases)
		{
			for (const FNamedCase& ExitCase : ExitCases)
			{
				for (const FNamedCase& DepthCase : DepthCases)
				{
					for (const FNamedCase& LocalCountCase : LocalCountCases)
					{
						for (const FNamedCase& LineEndingCase : LineEndingCases)
						{
							++ObservedCaseCount;
							const FNativeCaseContext Case(MakeNativeCaseId(
								"LANG-CF-LIVE-LOCAL-CLEANUP",
								{
									ANSI_TO_TCHAR(ScopeCase.CatalogName),
									ANSI_TO_TCHAR(ExitCase.CatalogName),
									ANSI_TO_TCHAR(DepthCase.CatalogName),
									ANSI_TO_TCHAR(LocalCountCase.CatalogName),
									ANSI_TO_TCHAR(LineEndingCase.CatalogName),
								}));
							const FString ModuleName = Case.MakeModuleName(TEXT("ControlFlowLifetime"));
							const FString Source = BuildSource(
								ScopeCase,
								ExitCase,
								DepthCase,
								LocalCountCase,
								LineEndingCase);
							const FTCHARToUTF8 ModuleNameUtf8(*ModuleName);
							const FTCHARToUTF8 SourceUtf8(*Source);

							Engine.Reset(*TestRunner);
							FNativeLifecycleRecorder Lifecycle;
							Lifecycle.Reset();
							ScriptEngine->SetUserData(&Lifecycle, NativeLifecycleRecorderUserDataSlot);
							PrintGeneratedAsSource(*TestRunner, Case.GetId(), ModuleName, Source);

							asIScriptModule* Module = nullptr;
							const int BuildResult = CompileNativeModule(
								ScriptEngine,
								ModuleNameUtf8.Get(),
								SourceUtf8.Get(),
								Module);
							const bool bBuilt = BuildResult >= 0 && Module != nullptr;
							const FString BuildDescription = FString::Printf(
								TEXT("%s result=%d messages=%s"),
								*Case.Describe(TEXT("every generated lifetime source should compile")),
								BuildResult,
								*Engine.GetMessagesText());
							ASSERT_THAT(AreEqual(asSUCCESS, BuildResult, *BuildDescription));
							ASSERT_THAT(IsNotNull(Module,
								*Case.Describe(TEXT("every generated lifetime source should publish a module"))));

							if (bBuilt)
							{
								asIScriptFunction* const Entry = GetNativeFunctionByExactDecl(Module, "int Entry()");
								asIScriptFunction* const Recovery = GetNativeFunctionByExactDecl(Module, "int Recovery()");
								ASSERT_THAT(IsNotNull(Entry,
									*Case.Describe(TEXT("lifetime source should resolve exact int Entry()"))));
								ASSERT_THAT(IsNotNull(Recovery,
									*Case.Describe(TEXT("lifetime source should resolve exact int Recovery()"))));
								if (Entry != nullptr && Recovery != nullptr)
								{
									asIScriptContext* const Context = ScriptEngine->CreateContext();
									ASSERT_THAT(IsNotNull(Context,
										*Case.Describe(TEXT("lifetime source should create an execution context"))));
									if (Context != nullptr)
									{
										const bool bException = IsCase(ExitCase, "exception");
										const int ExecutionResult = PrepareAndExecute(Context, Entry);
										ASSERT_THAT(AreEqual(
											bException ? asEXECUTION_EXCEPTION : asEXECUTION_FINISHED,
											ExecutionResult,
											*Case.Describe(TEXT("exit path should produce its exact execution state"))));
										if (!bException)
										{
											const int32 Expected = ExpectedTrace(ExitCase, DepthCase);
											const int32 Actual = static_cast<int32>(Context->GetReturnDWord());
											const FString TraceDescription = FString::Printf(
												TEXT("%s expected=%d actual=%d"),
												*Case.Describe(TEXT("control-flow transfer should preserve the exact trace")),
												Expected,
												Actual);
											ASSERT_THAT(AreEqual(Expected, Actual, *TraceDescription));
										}
										ASSERT_THAT(AreEqual(asSUCCESS, Context->Unprepare(),
											*Case.Describe(TEXT("lifetime context should unprepare before recovery"))));
										const int RecoveryResult = PrepareAndExecute(Context, Recovery);
										ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, RecoveryResult,
											*Case.Describe(TEXT("same context should execute the recovery function"))));
										ASSERT_THAT(AreEqual(97, static_cast<int32>(Context->GetReturnDWord()),
											*Case.Describe(TEXT("recovery should return its stable marker"))));
										ASSERT_THAT(AreEqual(asSUCCESS, Context->Unprepare(),
											*Case.Describe(TEXT("recovery context should unprepare cleanly"))));
										Context->Release();
									}
								}
							}
							VerifyLifecycle(*TestRunner, Case, Lifecycle, GetDepth(DepthCase), GetLocalCount(LocalCountCase));
							ScriptEngine->DiscardModule(ModuleNameUtf8.Get());
							ASSERT_THAT(IsNull(ScriptEngine->GetModule(ModuleNameUtf8.Get(), asGM_ONLY_IF_EXISTS),
								*Case.Describe(TEXT("lifetime module should be discarded after every cell"))));
						}
					}
				}
			}
		}

		ASSERT_THAT(AreEqual(
			UE_ARRAY_COUNT(ScopeCases)
			* UE_ARRAY_COUNT(ExitCases)
			* UE_ARRAY_COUNT(DepthCases)
			* UE_ARRAY_COUNT(LocalCountCases)
			* UE_ARRAY_COUNT(LineEndingCases),
			ObservedCaseCount,
			TEXT("LANG-CF-LIVE-LOCAL-CLEANUP must execute every scope, exit, depth, local-count, and layout cell")));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
