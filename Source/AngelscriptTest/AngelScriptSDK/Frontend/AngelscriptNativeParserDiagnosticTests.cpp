#include "../Support/AngelscriptNativeCoreTestSupport.h"
#include "../Support/AngelscriptNativeCaseTestSupport.h"
#include "AngelscriptTestMacros.h"
#include "CQTest.h"
#include "Misc/ScopeExit.h"

#if WITH_ANGELSCRIPT_UNITTESTS


TEST_CLASS_WITH_FLAGS(FParserDiagnosticTests,
	"Angelscript.TestModule.AngelScriptSDK.Frontend.ParserDiagnostics",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	static int32 CountErrors(const AngelscriptNativeTestSupport::FNativeMessageCollector& Messages)
	{
		int32 Count = 0;
		for (const AngelscriptNativeTestSupport::FNativeMessageEntry& Entry : Messages.Entries)
		{
			if (Entry.Type == asMSGTYPE_ERROR)
			{
				++Count;
			}
		}

		return Count;
	}

	static int CompileSnippetWithCleanup(
		FNoDiscardAsserter& Assert,
		const char* ModuleName,
		const char* Source,
		AngelscriptNativeTestSupport::FNativeMessageCollector& Messages)
	{
		using namespace AngelscriptNativeTestSupport;

		asIScriptEngine* ScriptEngine = CreateNativeEngine(&Messages);
		if (!Assert.IsNotNull(
			ScriptEngine,
			FString::Printf(TEXT("%s should create an independent diagnostic engine"), UTF8_TO_TCHAR(ModuleName))))
		{
			return asERROR;
		}

		ON_SCOPE_EXIT
		{
			DestroyNativeEngine(ScriptEngine);
		};

		asIScriptModule* Module = nullptr;
		const int CompileResult = CompileNativeModule(ScriptEngine, ModuleName, Source, Module);
		const bool bDiscarded = Assert.AreEqual(
			asSUCCESS,
			ScriptEngine->DiscardModule(ModuleName),
			FString::Printf(TEXT("%s should discard after diagnostic collection"), UTF8_TO_TCHAR(ModuleName)));
		const bool bLookupCleared = Assert.IsNull(
			ScriptEngine->GetModule(ModuleName, asGM_ONLY_IF_EXISTS),
			FString::Printf(TEXT("%s should leave no name-visible diagnostic module"), UTF8_TO_TCHAR(ModuleName)));
		if (!bDiscarded || !bLookupCleared)
		{
			return asERROR;
		}
		return CompileResult;
	}

public:
	TEST_METHOD(MalformedDeclarationsByShapeAndLineEnding)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT("FRONTEND-PARSER-DIAGNOSTIC-LINE-ENDINGS",
			ENativeEvidence::Compile
			| ENativeEvidence::Diagnostic
			| ENativeEvidence::Cleanup
			| ENativeEvidence::Isolation);

		struct FDiagnosticCase
		{
			const TCHAR* Id;
			int32 MinimumErrors;
			const TCHAR* ExpectedFragment;
		};
		const FDiagnosticCase Cases[] =
		{
			{
				TEXT("unfinished_class"),
				1,
				TEXT("Expected"),
			},
			{
				TEXT("capital_const_parameter"),
				1,
				TEXT("Expected"),
			},
			{
				TEXT("unclosed_namespace"),
				1,
				TEXT("Expected"),
			},
			{
				TEXT("bad_parameter_list"),
				1,
				TEXT("Expected"),
			},
			{
				TEXT("multiple_malformed"),
				2,
				nullptr,
			},
		};
		const TCHAR* LineEndingIds[] =
		{
			TEXT("lf"),
			TEXT("crlf"),
		};
		const FString LineFeed = FString::Chr(10);
		const FString CarriageReturnLineFeed = FString::Chr(13) + FString::Chr(10);

		int32 ObservedCells = 0;
		for (int32 CaseIndex = 0; CaseIndex < UE_ARRAY_COUNT(Cases); ++CaseIndex)
		{
			const FDiagnosticCase& Case = Cases[CaseIndex];
			FString BaseSource;
			switch (CaseIndex)
			{
			case 0:
				AppendGeneratedAsLine(BaseSource, TEXT("class FBroken"));
				AppendGeneratedAsLine(BaseSource, TEXT("{"));
				break;
			case 1:
				AppendGeneratedAsLine(BaseSource, TEXT("class FCarrier"));
				AppendGeneratedAsLine(BaseSource, TEXT("{"));
				AppendGeneratedAsLine(BaseSource, TEXT("\tvoid Run(Const int& in Value)"));
				AppendGeneratedAsLine(BaseSource, TEXT("\t{"));
				AppendGeneratedAsLine(BaseSource, TEXT("\t}"));
				AppendGeneratedAsLine(BaseSource, TEXT("}"));
				break;
			case 2:
				AppendGeneratedAsLine(BaseSource, TEXT("namespace Outer"));
				AppendGeneratedAsLine(BaseSource, TEXT("{"));
				AppendGeneratedAsLine(BaseSource, TEXT("\tclass FInner"));
				AppendGeneratedAsLine(BaseSource, TEXT("\t{"));
				AppendGeneratedAsLine(BaseSource, TEXT("\t}"));
				break;
			case 3:
				AppendGeneratedAsLine(BaseSource, TEXT("void Bad(int A,, int B)"));
				AppendGeneratedAsLine(BaseSource, TEXT("{"));
				AppendGeneratedAsLine(BaseSource, TEXT("}"));
				break;
			default:
				AppendGeneratedAsLine(BaseSource, TEXT("void Bad("));
				AppendGeneratedAsLine(BaseSource, TEXT("{"));
				AppendGeneratedAsLine(BaseSource, TEXT("}"));
				AppendGeneratedAsLine(BaseSource);
				AppendGeneratedAsLine(BaseSource, TEXT("class FBroken"));
				AppendGeneratedAsLine(BaseSource, TEXT("{"));
				AppendGeneratedAsLine(BaseSource, TEXT("\tint;"));
				AppendGeneratedAsLine(BaseSource, TEXT("}"));
				AppendGeneratedAsLine(BaseSource);
				AppendGeneratedAsLine(BaseSource, TEXT("int Read()"));
				AppendGeneratedAsLine(BaseSource, TEXT("{"));
				AppendGeneratedAsLine(BaseSource, TEXT("\treturn;"));
				AppendGeneratedAsLine(BaseSource, TEXT("}"));
				break;
			}

			for (int32 LineEndingIndex = 0; LineEndingIndex < UE_ARRAY_COUNT(LineEndingIds); ++LineEndingIndex)
			{
				FString Source = BaseSource;
				if (LineEndingIndex == 1)
				{
					Source.ReplaceInline(*LineFeed, *CarriageReturnLineFeed);
				}

				const FString SourceId = FString::Printf(
					TEXT("FRONTEND-PARSER-DIAGNOSTIC-LINE-ENDINGS-%s-%s"),
					Case.Id,
					LineEndingIds[LineEndingIndex]);
				const FString ModuleName = FString::Printf(
					TEXT("ParserDiagnostic_%s_%s"),
					Case.Id,
					LineEndingIds[LineEndingIndex]);
				PrintGeneratedAsSource(*TestRunner, SourceId, ModuleName, Source);

				const FTCHARToUTF8 ModuleNameUtf8(*ModuleName);
				const FTCHARToUTF8 SourceUtf8(*Source);
				FNativeMessageCollector Messages;
				const int CompileResult = CompileSnippetWithCleanup(
					this->Assert,
					ModuleNameUtf8.Get(),
					SourceUtf8.Get(),
					Messages);

				ASSERT_THAT(IsTrue(CompileResult < 0,
					FString::Printf(TEXT("%s should fail compilation"), *SourceId)));
				ASSERT_THAT(IsTrue(CountErrors(Messages) >= Case.MinimumErrors,
					FString::Printf(TEXT("%s should retain its minimum diagnostic count"), *SourceId)));
				if (Case.ExpectedFragment != nullptr)
				{
					ASSERT_THAT(IsTrue(ContainsError(Messages, Case.ExpectedFragment),
						FString::Printf(TEXT("%s should retain diagnostic fragment '%s'"), *SourceId, Case.ExpectedFragment)));
				}
				++ObservedCells;
			}
		}

		ASSERT_THAT(AreEqual(10, ObservedCells,
			TEXT("Parser diagnostic product should execute every malformed-shape and line-ending cell")));

		const FString ControlSourceId = TEXT("FRONTEND-PARSER-DIAGNOSTIC-LINE-ENDINGS-isolation-control");
		const FString ControlModuleName = TEXT("ParserDiagnosticIsolationControl");
		const FString ControlSource = TEXT("int Control() { return 13; }");
		PrintGeneratedAsSource(*TestRunner, ControlSourceId, ControlModuleName, ControlSource);
		const FTCHARToUTF8 ControlModuleNameUtf8(*ControlModuleName);
		const FTCHARToUTF8 ControlSourceUtf8(*ControlSource);
		FNativeMessageCollector ControlMessages;
		ASSERT_THAT(AreEqual(
			asSUCCESS,
			CompileSnippetWithCleanup(
				this->Assert,
				ControlModuleNameUtf8.Get(),
				ControlSourceUtf8.Get(),
				ControlMessages),
			TEXT("Parser diagnostic isolation control should compile on a clean engine after all malformed cells")));
		ASSERT_THAT(AreEqual(
			0,
			CountErrors(ControlMessages),
			TEXT("Parser diagnostic isolation control should not inherit prior errors")));
	}

	TEST_METHOD(UnfinishedClassReportsMissingBrace)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_NON_PRODUCT("LegacyCompatibility",
			"Retained unfinished-class diagnostic smoke; FRONTEND-PARSER-DIAGNOSTIC-LINE-ENDINGS owns malformed declaration shapes across LF and CRLF.");

		AngelscriptNativeTestSupport::FNativeMessageCollector Messages;
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			class myclass
			{
			)AS");
		const int CompileResult = CompileSnippet("ReferenceParserUnfinishedClass", ScriptSource.c_str(),
			Messages);

		ASSERT_THAT(IsTrue(CompileResult < 0, TEXT("Reference unfinished class should fail to build")));
		ASSERT_THAT(IsTrue(ContainsError(Messages, TEXT("Expected '}'")) || ContainsError(Messages, TEXT("<end of file>")),
			TEXT("Reference unfinished class should report a missing class body terminator")));
	}

	TEST_METHOD(CapitalConstInParameterIsRejected)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_NON_PRODUCT("LegacyCompatibility",
			"Retained capital-Const parameter diagnostic smoke; FRONTEND-PARSER-DIAGNOSTIC-LINE-ENDINGS owns malformed declaration shapes across LF and CRLF.");

		AngelscriptNativeTestSupport::FNativeMessageCollector Messages;
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			class myclass
			{
				void f(Const int&in)
				{
				}
			};
			)AS");
		const int CompileResult = CompileSnippet("ReferenceParserCapitalConst", ScriptSource.c_str(),
			Messages);

		ASSERT_THAT(IsTrue(CompileResult < 0, TEXT("Reference capital Const parameter should fail to build")));
		ASSERT_THAT(IsTrue(CountErrors(Messages) > 0, TEXT("Reference capital Const parameter should emit at least one syntax error")));
		ASSERT_THAT(IsTrue(ContainsError(Messages, TEXT("Const")) || ContainsError(Messages, TEXT("int")) || ContainsError(Messages, TEXT("Expected")),
			TEXT("Reference capital Const parameter should keep useful diagnostic context")));
	}

	TEST_METHOD(UnclosedNamespaceReportsEndOfFile)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_NON_PRODUCT("LegacyCompatibility",
			"Retained unclosed-namespace diagnostic smoke; FRONTEND-PARSER-DIAGNOSTIC-LINE-ENDINGS owns malformed declaration shapes across LF and CRLF.");

		AngelscriptNativeTestSupport::FNativeMessageCollector Messages;
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			namespace Outer
			{
				class Inner
				{
			)AS");
		const int CompileResult = CompileSnippet("ReferenceParserUnclosedNamespace", ScriptSource.c_str(),
			Messages);

		ASSERT_THAT(IsTrue(CompileResult < 0, TEXT("Reference unclosed namespace should fail to build")));
		ASSERT_THAT(IsTrue(CountErrors(Messages) >= 1, TEXT("Reference unclosed namespace should report parser errors")));
		ASSERT_THAT(IsTrue(ContainsError(Messages, TEXT("Expected")) || ContainsError(Messages, TEXT("<end of file>")),
			TEXT("Reference unclosed namespace should mention expected structure or EOF")));
	}

	TEST_METHOD(BadParameterListAccumulatesSyntaxError)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_NON_PRODUCT("LegacyCompatibility",
			"Retained bad-parameter-list diagnostic smoke; FRONTEND-PARSER-DIAGNOSTIC-LINE-ENDINGS owns malformed declaration shapes across LF and CRLF.");

		AngelscriptNativeTestSupport::FNativeMessageCollector Messages;
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			void Bad(int A,, int B)
			{
			}
			)AS");
		const int CompileResult = CompileSnippet("ReferenceParserBadParameters", ScriptSource.c_str(),
			Messages);

		ASSERT_THAT(IsTrue(CompileResult < 0, TEXT("Reference bad parameter list should fail to build")));
		ASSERT_THAT(IsTrue(ContainsError(Messages, TEXT("Expected")) || ContainsError(Messages, TEXT("Instead found")),
			TEXT("Reference bad parameter list should produce a syntax diagnostic")));
	}

	TEST_METHOD(MultipleMalformedDeclarationsReportMultipleErrors)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_NON_PRODUCT("LegacyCompatibility",
			"Retained multiple-error accumulation smoke; FRONTEND-PARSER-DIAGNOSTIC-LINE-ENDINGS owns malformed declaration shapes, error-count expectations, and LF/CRLF behavior.");

		AngelscriptNativeTestSupport::FNativeMessageCollector Messages;
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			void Bad(
			{
			}

			class Broken
			{
				int;
			}

			int Read()
			{
				return;
			}
			)AS");
		const int CompileResult = CompileSnippet("ReferenceParserMultipleMalformed", ScriptSource.c_str(),
			Messages);

		ASSERT_THAT(IsTrue(CompileResult < 0, TEXT("Reference malformed declarations should fail to build")));
		ASSERT_THAT(IsTrue(CountErrors(Messages) >= 2,
			TEXT("Reference malformed declarations should accumulate more than one error")));
	}
};

#endif
