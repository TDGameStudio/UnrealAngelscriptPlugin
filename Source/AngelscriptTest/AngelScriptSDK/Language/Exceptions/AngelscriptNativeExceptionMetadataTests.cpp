#include "../../Support/AngelscriptNativeCaseTestSupport.h"
#include "../../Support/AngelscriptNativeCoreTestSupport.h"
#include "../../Support/AngelscriptNativeExecutionTestSupport.h"
#include "../../Support/AngelscriptNativeFixtureTestSupport.h"
#include "../../Support/AngelscriptNativeLanguageCaseTestSupport.h"

#include "CQTest.h"

using AngelscriptNativeTestSupport::AppendGeneratedAsLine;

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FExceptionMetadataTests,
	"Angelscript.TestModule.AngelScriptSDK.Language.Exceptions.Metadata",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	struct FNamedCase
	{
		const ANSICHAR* CatalogName;
	};

	inline static constexpr FNamedCase DepthCases[] =
	{
		{ "top" },
		{ "one_call" },
		{ "three_calls" },
		{ "method" },
	};

	inline static constexpr FNamedCase LayoutCases[] =
	{
		{ "lf" },
		{ "crlf" },
		{ "lf_comments" },
	};


	static void RaiseMetadataException()
	{
		if (asIScriptContext* const Context = asGetActiveContext())
		{
			Context->SetException("metadata exception");
		}
	}

	static bool RegisterMetadataBridge(asIScriptEngine& ScriptEngine)
	{
		const ASAutoCaller::FunctionCaller ExceptionCaller = ASAutoCaller::MakeFunctionCaller(RaiseMetadataException);
		return ScriptEngine.RegisterGlobalFunction(
			"void RaiseMetadataException()",
			asFUNCTION(RaiseMetadataException),
			asCALL_CDECL,
			*(asFunctionCaller*)&ExceptionCaller) >= 0;
	}

	static bool IsNamedCase(const FNamedCase& Case, const ANSICHAR* Name)
	{
		return FCStringAnsi::Strcmp(Case.CatalogName, Name) == 0;
	}

	static FString ExpectedExceptionFunction(const FNamedCase& DepthCase)
	{
		if (IsNamedCase(DepthCase, "one_call"))
		{
			return TEXT("DepthOne");
		}
		if (IsNamedCase(DepthCase, "three_calls"))
		{
			return TEXT("DepthThree");
		}
		if (IsNamedCase(DepthCase, "method"))
		{
			return TEXT("RaisePoint");
		}
		return TEXT("Entry");
	}

	static int32 ExpectedMinimumCallstackDepth(const FNamedCase& DepthCase)
	{
		if (IsNamedCase(DepthCase, "one_call"))
		{
			return 2;
		}
		if (IsNamedCase(DepthCase, "three_calls"))
		{
			return 4;
		}
		if (IsNamedCase(DepthCase, "method"))
		{
			return 2;
		}
		return 1;
	}

	static FString ApplyLayout(const FString& Source, const FNamedCase& LayoutCase)
	{
		if (!IsNamedCase(LayoutCase, "crlf"))
		{
			return Source;
		}

		FString Result;
		Result.Reserve(Source.Len() + 64);
		for (int32 Index = 0; Index < Source.Len(); ++Index)
		{
			const TCHAR Character = Source[Index];
			if (Character == TCHAR('\n'))
			{
				Result.AppendChar(TCHAR('\r'));
			}
			Result.AppendChar(Character);
		}
		return Result;
	}

	static FString BuildMetadataSource(const FNamedCase& DepthCase, const FNamedCase& LayoutCase)
	{
		FString Source;
		if (IsNamedCase(DepthCase, "one_call"))
		{
			AppendGeneratedAsLine(Source, TEXT("void DepthOne()"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\tRaiseMetadataException();"));
			AppendGeneratedAsLine(Source, TEXT("}"));
			AppendGeneratedAsLine(Source);
		}
		else if (IsNamedCase(DepthCase, "three_calls"))
		{
			AppendGeneratedAsLine(Source, TEXT("void DepthThree()"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\tRaiseMetadataException();"));
			AppendGeneratedAsLine(Source, TEXT("}"));
			AppendGeneratedAsLine(Source);
			AppendGeneratedAsLine(Source, TEXT("void DepthTwo()"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\tDepthThree();"));
			AppendGeneratedAsLine(Source, TEXT("}"));
			AppendGeneratedAsLine(Source);
			AppendGeneratedAsLine(Source, TEXT("void DepthOne()"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\tDepthTwo();"));
			AppendGeneratedAsLine(Source, TEXT("}"));
			AppendGeneratedAsLine(Source);
		}
		else if (IsNamedCase(DepthCase, "method"))
		{
			AppendGeneratedAsLine(Source, TEXT("struct FMetadataProbe"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\tvoid RaisePoint()"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t\tRaiseMetadataException();"));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
			AppendGeneratedAsLine(Source, TEXT("}"));
			AppendGeneratedAsLine(Source);
		}

		if (IsNamedCase(LayoutCase, "lf_comments"))
		{
			AppendGeneratedAsLine(Source, TEXT("// metadata layout boundary"));
		}

		AppendGeneratedAsLine(Source, TEXT("void Entry()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		if (IsNamedCase(DepthCase, "one_call"))
		{
			AppendGeneratedAsLine(Source, TEXT("\tDepthOne();"));
		}
		else if (IsNamedCase(DepthCase, "three_calls"))
		{
			AppendGeneratedAsLine(Source, TEXT("\tDepthOne();"));
		}
		else if (IsNamedCase(DepthCase, "method"))
		{
			AppendGeneratedAsLine(Source, TEXT("\tFMetadataProbe Value;"));
			AppendGeneratedAsLine(Source, TEXT("\tValue.RaisePoint();"));
		}
		else
		{
			AppendGeneratedAsLine(Source, TEXT("\tRaiseMetadataException();"));
		}
		AppendGeneratedAsLine(Source, TEXT("}"));
		return ApplyLayout(Source, LayoutCase);
	}

	static int32 FindLineContaining(const FString& Source, const TCHAR* Needle)
	{
		const int32 MatchIndex = Source.Find(Needle, ESearchCase::CaseSensitive, ESearchDir::FromStart);
		if (MatchIndex == INDEX_NONE)
		{
			return INDEX_NONE;
		}

		int32 Line = 1;
		for (int32 Index = 0; Index < MatchIndex; ++Index)
		{
			if (Source[Index] == TCHAR('\n'))
			{
				++Line;
			}
		}
		return Line;
	}

	static FString MakeModuleName(const AngelscriptNativeTestSupport::FNativeCaseContext& Case)
	{
		return TEXT("ExceptionMetadata_") + Case.GetId().Replace(TEXT("-"), TEXT("_"));
	}

public:
	TEST_METHOD(StackMetadataByDepthAndLayout)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT("LANG-EX-METADATA-STACK",
			ENativeEvidence::Compile
			| ENativeEvidence::Runtime
			| ENativeEvidence::Debug
			| ENativeEvidence::Cleanup
			| ENativeEvidence::Isolation);

		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};
		FNoDiscardAsserter Assertions(*TestRunner);

		asIScriptEngine* const ScriptEngine = Engine.Get();
		if (!Assertions.IsNotNull(ScriptEngine, TEXT("Exception metadata product should create a raw SDK engine")))
		{
			return;
		}

		if (!Assertions.IsTrue(RegisterMetadataBridge(*ScriptEngine),
			TEXT("Exception metadata should register the native exception bridge")))
		{
			return;
		}

		for (const FNamedCase& DepthCase : DepthCases)
		{
			for (const FNamedCase& LayoutCase : LayoutCases)
			{
				const FNativeCaseContext Case(MakeNativeCaseId(
					"LANG-EX-METADATA-STACK",
					{
						ANSI_TO_TCHAR(DepthCase.CatalogName),
						ANSI_TO_TCHAR(LayoutCase.CatalogName),
					}));
				const FString ModuleName = MakeModuleName(Case);
				const FString Source = BuildMetadataSource(DepthCase, LayoutCase);
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
				if (!Assertions.AreEqual(
					asSUCCESS,
					BuildResult,
					*FString::Printf(
						TEXT("%s; result=%d messages={%s}"),
						*Case.Describe(TEXT("exception metadata source should compile")),
						BuildResult,
						*Engine.GetMessagesText())))
				{
					ScriptEngine->DiscardModule(ModuleNameUtf8.Get());
					continue;
				}

				asIScriptFunction* const Entry = GetNativeFunctionByExactDecl(Module, "void Entry()");
				if (!Assertions.IsNotNull(Entry, *Case.Describe(TEXT("exception metadata source should publish Entry"))))
				{
					ScriptEngine->DiscardModule(ModuleNameUtf8.Get());
					continue;
				}

				asIScriptContext* const Context = ScriptEngine->CreateContext();
				if (!Assertions.IsNotNull(Context, *Case.Describe(TEXT("exception metadata should create a context"))))
				{
					ScriptEngine->DiscardModule(ModuleNameUtf8.Get());
					continue;
				}

				const bool bExecutionSucceeded = Assertions.AreEqual(
					asEXECUTION_EXCEPTION,
					PrepareAndExecute(Context, Entry),
					*Case.Describe(TEXT("exception metadata entry should finish in the exception state")));
				if (bExecutionSucceeded)
				{
					const FString ExceptionText = UTF8_TO_TCHAR(Context->GetExceptionString() != nullptr
						? Context->GetExceptionString()
						: "");
					(void)Assertions.AreEqual(
						FString(TEXT("metadata exception")),
						ExceptionText,
						*FString::Printf(TEXT("%s; actual={%s}"),
							*Case.Describe(TEXT("exception metadata should preserve the native exception text")),
							*ExceptionText));

					asIScriptFunction* const ExceptionFunction = Context->GetExceptionFunction();
					if (Assertions.IsNotNull(ExceptionFunction,
						*Case.Describe(TEXT("exception metadata should retain the throwing script function"))))
					{
						const FString ActualExceptionFunction = ExceptionFunction != nullptr
							? UTF8_TO_TCHAR(ExceptionFunction->GetName())
							: TEXT("<null>");
						(void)Assertions.AreEqual(
							ExpectedExceptionFunction(DepthCase),
							ActualExceptionFunction,
							*FString::Printf(TEXT("%s; actual={%s}"),
								*Case.Describe(TEXT("exception metadata should identify the exact throwing function")),
								*ActualExceptionFunction));
					}

					int Column = 0;
					const char* Section = nullptr;
					const int ExceptionLine = Context->GetExceptionLineNumber(&Column, &Section);
					const int ExpectedLine = FindLineContaining(Source, TEXT("RaiseMetadataException();"));
					(void)Assertions.AreEqual(
						ExpectedLine,
						ExceptionLine,
						*FString::Printf(TEXT("%s; expected=%d actual=%d"),
							*Case.Describe(TEXT("exception metadata should identify the generated source line")),
							ExpectedLine,
							ExceptionLine));
					(void)Assertions.IsTrue(
						Column > 0 && Section != nullptr && FCStringAnsi::Strlen(Section) > 0,
						*Case.Describe(TEXT("exception metadata should retain a positive column and source section")));

					const asUINT CallstackDepth = Context->GetCallstackSize();
					(void)Assertions.IsTrue(
						CallstackDepth >= static_cast<asUINT>(ExpectedMinimumCallstackDepth(DepthCase)),
						*FString::Printf(TEXT("%s; expected-at-least=%d actual=%d"),
							*Case.Describe(TEXT("exception metadata should retain every generated call frame")),
							ExpectedMinimumCallstackDepth(DepthCase),
							static_cast<int32>(CallstackDepth)));
					for (asUINT StackLevel = 0; StackLevel < CallstackDepth; ++StackLevel)
					{
						asIScriptFunction* const FrameFunction = Context->GetFunction(StackLevel);
						int FrameColumn = 0;
						const char* FrameSection = nullptr;
						const int FrameLine = Context->GetLineNumber(StackLevel, &FrameColumn, &FrameSection);
						(void)Assertions.IsNotNull(FrameFunction,
							*Case.Describe(TEXT("every retained exception stack frame should expose a function")));
						(void)Assertions.IsTrue(
							FrameLine > 0 && FrameColumn > 0 && FrameSection != nullptr,
							*Case.Describe(TEXT("every retained exception stack frame should expose source coordinates")));
					}
				}

				(void)Assertions.AreEqual(
					asSUCCESS,
					Context->Unprepare(),
					*Case.Describe(TEXT("exception metadata context should unprepare cleanly")));
				(void)Assertions.IsNull(
					Context->GetExceptionFunction(),
					*Case.Describe(TEXT("unprepared context should not retain a stale exception function")));
				(void)Assertions.AreEqual(
					static_cast<asUINT>(0),
					Context->GetCallstackSize(),
					*Case.Describe(TEXT("unprepared context should clear the retained exception callstack")));
				Context->Release();

				ScriptEngine->DiscardModule(ModuleNameUtf8.Get());
				(void)Assertions.IsNull(
					ScriptEngine->GetModule(ModuleNameUtf8.Get(), asGM_ONLY_IF_EXISTS),
					*Case.Describe(TEXT("exception metadata module should be discarded after observation")));
			}
		}
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
