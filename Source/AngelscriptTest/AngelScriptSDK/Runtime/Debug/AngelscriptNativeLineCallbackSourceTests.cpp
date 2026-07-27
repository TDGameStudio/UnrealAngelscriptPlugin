#include "../../Support/AngelscriptNativeDebugTestSupport.h"
#include "../../Support/AngelscriptNativeLanguageCaseTestSupport.h"

#include "CQTest.h"
#include "Misc/ScopeExit.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FNativeLineCallbackSourceTests,
	"Angelscript.TestModule.AngelScriptSDK.Runtime.Debug.LineCallbackSource",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	struct FPathCase
	{
		const ANSICHAR* Name;
		const TCHAR* ActiveNeedle;
		const TCHAR* InactiveNeedle;
		int32 ExpectedResult;
		int32 MinimumActiveHits;
	};

	struct FLayoutCase
	{
		const ANSICHAR* Name;
		bool bCarriageReturnLineFeed;
	};

	struct FCallbackStateCase
	{
		const ANSICHAR* Name;
		bool bInstall;
		bool bReplaceRecorder;
		bool bClearBeforeExecute;
	};


	inline static constexpr FPathCase PathCases[] =
	{
		{ "straight", TEXT("Trace += 1;"), nullptr, 1, 1 },
		{ "branch_true", TEXT("Trace += 10;"), TEXT("Trace += 20;"), 10, 1 },
		{ "branch_false", TEXT("Trace += 20;"), TEXT("Trace += 10;"), 20, 1 },
		{ "loop_zero", nullptr, TEXT("Trace += Index + 1;"), 0, 0 },
		{ "loop_two", TEXT("Trace += Index + 1;"), nullptr, 3, 2 },
	};

	inline static constexpr FLayoutCase LayoutCases[] =
	{
		{ "lf", false },
		{ "crlf", true },
	};

	inline static constexpr FCallbackStateCase CallbackStateCases[] =
	{
		{ "absent", false, false, false },
		{ "installed", true, false, false },
		{ "replaced", true, true, false },
		{ "cleared", true, false, true },
	};

	static FString MakeCaseId(
		const FLayoutCase& LayoutCase,
		const FPathCase& PathCase,
		const FCallbackStateCase& StateCase)
	{
		return FString::Printf(
			TEXT("DBG-LINE-CALLBACK-SOURCE-PATH-%hs-%hs-%hs"),
			LayoutCase.Name,
			PathCase.Name,
			StateCase.Name);
	}

	static FString MakeModuleName(
		const FLayoutCase& LayoutCase,
		const FPathCase& PathCase,
		const FCallbackStateCase& StateCase)
	{
		return FString::Printf(
			TEXT("NativeDebugLine_%hs_%hs_%hs"),
			LayoutCase.Name,
			PathCase.Name,
			StateCase.Name);
	}

	static FString BuildSource(
		const FLayoutCase& LayoutCase,
		const FPathCase& PathCase)
	{
		using namespace AngelscriptNativeTestSupport;

		FString Source;
		AppendGeneratedAsLine(Source, FString::Printf(
			TEXT("// path=%hs layout=%hs"),
			PathCase.Name,
			LayoutCase.Name));
		AppendGeneratedAsLine(Source, TEXT("int LineProbe()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tint Trace = 0;"));

		if (FCStringAnsi::Strcmp(PathCase.Name, "straight") == 0)
		{
			AppendGeneratedAsLine(Source, TEXT("\tTrace += 1;"));
		}
		else if (FCStringAnsi::Strcmp(PathCase.Name, "branch_true") == 0)
		{
			AppendGeneratedAsLine(Source, TEXT("\tif (true)"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t\tTrace += 10;"));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
			AppendGeneratedAsLine(Source, TEXT("\telse"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t\tTrace += 20;"));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
		}
		else if (FCStringAnsi::Strcmp(PathCase.Name, "branch_false") == 0)
		{
			AppendGeneratedAsLine(Source, TEXT("\tif (false)"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t\tTrace += 10;"));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
			AppendGeneratedAsLine(Source, TEXT("\telse"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t\tTrace += 20;"));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
		}
		else if (FCStringAnsi::Strcmp(PathCase.Name, "loop_zero") == 0)
		{
			AppendGeneratedAsLine(Source, TEXT("\tfor (int Index = 0; Index < 0; ++Index)"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t\tTrace += Index + 1;"));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
		}
		else
		{
			AppendGeneratedAsLine(Source, TEXT("\tfor (int Index = 0; Index < 2; ++Index)"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t\tTrace += Index + 1;"));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
		}

		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("\treturn Trace;"));
		AppendGeneratedAsLine(Source, TEXT("}"));

		if (LayoutCase.bCarriageReturnLineFeed)
		{
			const FString LineFeed = FString::Chr(10);
			const FString CarriageReturnLineFeed = FString::Chr(13) + LineFeed;
			Source.ReplaceInline(*LineFeed, *CarriageReturnLineFeed);
		}
		return Source;
	}

	static int32 FindLineContaining(const FString& Source, const TCHAR* Needle)
	{
		if (Needle == nullptr)
		{
			return INDEX_NONE;
		}

		TArray<FString> Lines;
		Source.ParseIntoArrayLines(Lines, false);
		for (int32 LineIndex = 0; LineIndex < Lines.Num(); ++LineIndex)
		{
			if (Lines[LineIndex].Contains(Needle, ESearchCase::CaseSensitive))
			{
				return LineIndex + 1;
			}
		}
		return INDEX_NONE;
	}

	static int32 CountLineEvents(
		const AngelscriptNativeTestSupport::FNativeDebugRecorder& Recorder,
		const int32 Line)
	{
		int32 Count = 0;
		for (const AngelscriptNativeTestSupport::FNativeDebugEvent& Event : Recorder.GetEvents())
		{
			if (Event.Kind == AngelscriptNativeTestSupport::ENativeDebugEventKind::Line
				&& Event.Line == Line)
			{
				++Count;
			}
		}
		return Count;
	}

public:
	TEST_METHOD(PathsByLayoutAndCallbackState)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT("DBG-LINE-CALLBACK-SOURCE-PATH",
			ENativeEvidence::Compile
			| ENativeEvidence::Runtime
			| ENativeEvidence::Metadata
			| ENativeEvidence::Debug
			| ENativeEvidence::Cleanup
			| ENativeEvidence::Isolation);

		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};

		FScopedNativeDebugCallbacks DebugCallbacks;
		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Line callback source product should create a raw SDK engine")));
		if (ScriptEngine == nullptr)
		{
			return;
		}

		ASSERT_THAT(AreEqual(asSUCCESS, ScriptEngine->SetEngineProperty(asEP_OPTIMIZE_BYTECODE, 0),
			TEXT("Line callback source product should disable bytecode optimization for stable line evidence")));
		ASSERT_THAT(AreEqual(asSUCCESS, ScriptEngine->SetEngineProperty(asEP_BUILD_WITHOUT_LINE_CUES, 0),
			TEXT("Line callback source product should retain source line cues")));

		for (const FLayoutCase& LayoutCase : LayoutCases)
		{
			for (const FPathCase& PathCase : PathCases)
			{
				for (const FCallbackStateCase& StateCase : CallbackStateCases)
				{
					const FString CaseId = MakeCaseId(LayoutCase, PathCase, StateCase);
					const FString ModuleName = MakeModuleName(LayoutCase, PathCase, StateCase);
					const FString Source = BuildSource(LayoutCase, PathCase);
					PrintGeneratedAsSource(*TestRunner, CaseId, ModuleName, Source);

					const FTCHARToUTF8 ModuleNameUtf8(*ModuleName);
					const FTCHARToUTF8 SourceUtf8(*Source);
					asIScriptModule* Module = nullptr;
					const int BuildResult = CompileNativeModule(
						ScriptEngine,
						ModuleNameUtf8.Get(),
						SourceUtf8.Get(),
						Module);
					ASSERT_THAT(IsTrue(BuildResult >= 0,
						*FString::Printf(TEXT("%s should compile. Build=%d Messages={%s}"),
							*CaseId,
							BuildResult,
							*Engine.GetMessagesText())));
					ASSERT_THAT(IsNotNull(Module, *FString::Printf(TEXT("%s should publish a module"), *CaseId)));
					if (BuildResult < 0 || Module == nullptr)
					{
						continue;
					}
					ON_SCOPE_EXIT
					{
						ScriptEngine->DiscardModule(ModuleNameUtf8.Get());
					};

					asIScriptFunction* const Probe = GetNativeFunctionByExactDecl(Module, "int LineProbe()");
					ASSERT_THAT(IsNotNull(Probe, *FString::Printf(TEXT("%s should resolve its exact probe"), *CaseId)));
					if (Probe == nullptr)
					{
						continue;
					}

					asIScriptContext* const Context = ScriptEngine->CreateContext();
					ASSERT_THAT(IsNotNull(Context, *FString::Printf(TEXT("%s should create a context"), *CaseId)));
					if (Context == nullptr)
					{
						continue;
					}
					ON_SCOPE_EXIT
					{
						Context->Release();
					};

					asCContext* const RawContext = static_cast<asCContext*>(Context);
					FNativeDebugRecorder PrimaryRecorder;
					FNativeDebugRecorder ReplacementRecorder;
					if (StateCase.bInstall)
					{
						Context->SetUserData(&PrimaryRecorder, NativeDebugRecorderUserDataSlot);
						ASSERT_THAT(AreEqual(asSUCCESS, RawContext->SetLineCallback(CaptureNativeLine),
							*FString::Printf(TEXT("%s should install the line callback"), *CaseId)));
					}
					if (StateCase.bReplaceRecorder)
					{
						ASSERT_THAT(AreEqual(asSUCCESS, RawContext->SetLineCallback(CaptureNativeLine),
							*FString::Printf(TEXT("%s should replace the line callback registration"), *CaseId)));
						Context->SetUserData(&ReplacementRecorder, NativeDebugRecorderUserDataSlot);
					}
					if (StateCase.bClearBeforeExecute)
					{
						RawContext->ClearLineCallback();
					}

					ASSERT_THAT(AreEqual(asSUCCESS, Context->Prepare(Probe),
						*FString::Printf(TEXT("%s should prepare its probe"), *CaseId)));
					const int ExecuteResult = Context->Execute();
					ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_FINISHED), ExecuteResult,
						*FString::Printf(TEXT("%s should finish execution"), *CaseId)));
					ASSERT_THAT(AreEqual(PathCase.ExpectedResult, static_cast<int32>(Context->GetReturnDWord()),
						*FString::Printf(TEXT("%s should retain its independent runtime result"), *CaseId)));
					ASSERT_THAT(AreEqual(asSUCCESS, Context->Unprepare(),
						*FString::Printf(TEXT("%s should unprepare for the next isolated cell"), *CaseId)));

					FNativeDebugRecorder* ActiveRecorder = nullptr;
					if (StateCase.bReplaceRecorder)
					{
						ActiveRecorder = &ReplacementRecorder;
					}
					else if (StateCase.bInstall && !StateCase.bClearBeforeExecute)
					{
						ActiveRecorder = &PrimaryRecorder;
					}

					const int32 ActiveLine = FindLineContaining(Source, PathCase.ActiveNeedle);
					const int32 InactiveLine = FindLineContaining(Source, PathCase.InactiveNeedle);
					if (ActiveRecorder == nullptr)
					{
						ASSERT_THAT(AreEqual(0, PrimaryRecorder.Num(ENativeDebugEventKind::Line),
							*FString::Printf(TEXT("%s should not emit line events when absent or cleared"), *CaseId)));
						ASSERT_THAT(AreEqual(0, ReplacementRecorder.Num(ENativeDebugEventKind::Line),
							*FString::Printf(TEXT("%s should not emit replacement events when absent or cleared"), *CaseId)));
					}
					else
					{
						ASSERT_THAT(IsTrue(ActiveRecorder->Num(ENativeDebugEventKind::Line) > 0,
							*FString::Printf(TEXT("%s should emit line events"), *CaseId)));
						ASSERT_THAT(IsTrue(CountLineEvents(*ActiveRecorder, ActiveLine) >= PathCase.MinimumActiveHits,
							*FString::Printf(TEXT("%s should expose its selected executable marker line %d"),
								*CaseId,
								ActiveLine)));
						if (InactiveLine != INDEX_NONE)
						{
							ASSERT_THAT(AreEqual(0, CountLineEvents(*ActiveRecorder, InactiveLine),
								*FString::Printf(TEXT("%s should not execute its excluded marker line %d"),
									*CaseId,
									InactiveLine)));
						}
						for (const FNativeDebugEvent& Event : ActiveRecorder->GetEvents())
						{
							if (Event.Kind != ENativeDebugEventKind::Line)
							{
								continue;
							}
							ASSERT_THAT(IsTrue(Event.Line > 0 && Event.Column > 0,
								*FString::Printf(TEXT("%s should expose positive line/column metadata"), *CaseId)));
							ASSERT_THAT(AreEqual(ModuleName, Event.Section,
								*FString::Printf(TEXT("%s should expose its exact source section"), *CaseId)));
							ASSERT_THAT(AreEqual(FString(TEXT("int LineProbe()")), Event.FunctionDeclaration,
								*FString::Printf(TEXT("%s should expose its exact current function"), *CaseId)));
						}
						if (StateCase.bReplaceRecorder)
						{
							ASSERT_THAT(AreEqual(0, PrimaryRecorder.Num(ENativeDebugEventKind::Line),
								*FString::Printf(TEXT("%s should stop delivering events to the replaced recorder"), *CaseId)));
						}
					}

					RawContext->ClearLineCallback();
					Context->SetUserData(nullptr, NativeDebugRecorderUserDataSlot);
				}
			}
		}
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
