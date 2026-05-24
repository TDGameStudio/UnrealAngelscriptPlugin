// ============================================================================
// AngelscriptPreprocessorDirectiveTests.cpp
//
// Preprocessor tests for conditional directives (#if, #ifdef, #ifndef, #elif,
// #else, #endif), structural error reporting, whitespace tolerance, and
// unsupported directives (#include).
//
// Migrated from:
//   - AngelscriptPreprocessorDirectiveTests.cpp (IfdefBoolean, InactiveBranch, Elif, StringLiteral, Compound)
//   - AngelscriptPreprocessorDirectiveErrorTests.cpp (StructuralErrors)
//   - AngelscriptPreprocessorDirectiveWhitespaceTests.cpp (TabSeparated)
//   - AngelscriptPreprocessorIncludeDirectiveTests.cpp (IncludeDirective)
//
// Automation prefix: Angelscript.TestModule.Preprocessor.Directives.*
// ============================================================================

#include "CQTest.h"
#include "Preprocessor/AngelscriptPreprocessorTestHelpers.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace PreprocessorTestHelpers;

// ============================================================================
// Test class
// ============================================================================

TEST_CLASS_WITH_FLAGS(FAngelscriptPreprocessorDirectiveTest,
	"Angelscript.TestModule.Preprocessor.Directives",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	// ========================================================================
	// IfdefRespectsBooleanFlagValue — false flag routes #ifdef to else branch,
	// #ifndef to body; compiles and executes correctly
	// ========================================================================
	TEST_METHOD(IfdefRespectsBooleanFlagValue)
	{
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine); FScopedModuleCleanEngine _AutoModuleClean(Engine);

		static const FName ModuleName(TEXT("Tests.Preprocessor.Directives.IfdefBooleanFlagValue"));
		ON_SCOPE_EXIT { Engine.DiscardModule(*ModuleName.ToString()); };

		const FString RelativeScriptPath = TEXT("Tests/Preprocessor/Directives/IfdefBooleanFlagValue.as");
		const FString ScriptSource = TEXT(
			"#ifdef MYFLAG\n"
			"int IfdefValue()\n"
			"{\n"
			"    return 1;\n"
			"}\n"
			"#else\n"
			"int IfdefValue()\n"
			"{\n"
			"    return 2;\n"
			"}\n"
			"#endif\n"
			"#ifndef MYFLAG\n"
			"int IfndefValue()\n"
			"{\n"
			"    return 3;\n"
			"}\n"
			"#else\n"
			"int IfndefValue()\n"
			"{\n"
			"    return 4;\n"
			"}\n"
			"#endif\n"
			"int Entry()\n"
			"{\n"
			"    return IfdefValue() * 10 + IfndefValue();\n"
			"}\n");

		FFixtureFile File(RelativeScriptPath, ScriptSource);

		auto Result = RunPreprocess(Engine, File, {{TEXT("MYFLAG"), false}});
		LogProcessedCode(Result, TEXT("IfdefBoolean"));

		AssertPreprocessSucceeded(*TestRunner, Result);
		AssertModuleCount(*TestRunner, Result, 1);
		AssertErrorCount(*TestRunner, Result, 0);
		AssertNoDiagnostics(*TestRunner, Result);

		const FAngelscriptModuleDesc* Module = AssertModuleExists(
			*TestRunner, Result, TEXT("Tests.Preprocessor.Directives.IfdefBooleanFlagValue"));
		if (Module != nullptr)
		{
			AssertModuleCodeContains(*TestRunner, Result, *Module, TEXT("return 2;"));
			AssertModuleCodeContains(*TestRunner, Result, *Module, TEXT("return 3;"));
			AssertModuleCodeNotContains(*TestRunner, Result, *Module, TEXT("return 1;"));
			AssertModuleCodeNotContains(*TestRunner, Result, *Module, TEXT("return 4;"));
		}

		// Compile and execute
		FAngelscriptCompileTraceSummary Summary;
		const bool bCompiled = CompileModuleWithSummary(
			&Engine, ECompileType::SoftReloadOnly, ModuleName,
			RelativeScriptPath, ScriptSource, true, Summary);

		TestRunner->TestTrue(TEXT("Should compile after preprocessing"), bCompiled);
		TestRunner->TestEqual(TEXT("Should emit no compile diagnostics"), Summary.Diagnostics.Num(), 0);

		int32 EntryResult = 0;
		const bool bExecuted = bCompiled
			&& ExecuteIntFunction(&Engine, RelativeScriptPath, ModuleName, TEXT("int Entry()"), EntryResult);
		TestRunner->TestTrue(TEXT("Entry should execute"), bExecuted);
		if (bExecuted)
		{
			TestRunner->TestEqual(
				TEXT("False MYFLAG: #ifdef→else(2), #ifndef→body(3) → 23"),
				EntryResult, 23);
		}

		}
	}

	// ========================================================================
	// InactiveBranchSkipsUnknownConditions — when #if EDITOR is taken,
	// later #elif UNKNOWN_FLAG in the dead branch does not produce errors
	// ========================================================================
	TEST_METHOD(InactiveBranchSkipsUnknownConditions)
	{
		TUniquePtr<FAngelscriptEngine> OwnedEngine = CreateEditorEngine();
		if (!TestRunner->TestNotNull(TEXT("Should create editor engine"), OwnedEngine.Get()))
		{
			return;
		}

		FAngelscriptEngine& Engine = *OwnedEngine;
		{
		FAngelscriptEngineScope _AutoEngineScope(Engine);
		ON_SCOPE_EXIT
		{
			const TArray<TSharedRef<FAngelscriptModuleDesc>> _ActiveModules = Engine.GetActiveModules();
			for (const TSharedRef<FAngelscriptModuleDesc>& _Module : _ActiveModules)
			{
				Engine.DiscardModule(*_Module->ModuleName);
			}
		};

		TestRunner->TestTrue(
			TEXT("Should run with EDITOR flag enabled"),
			FAngelscriptEngine::ShouldUseEditorScriptsForCurrentContext());

		static const FName ModuleName(TEXT("Tests.Preprocessor.Directives.InactiveUnknownConditions"));
		ON_SCOPE_EXIT { Engine.DiscardModule(*ModuleName.ToString()); };

		const FString RelativeScriptPath = TEXT("Tests/Preprocessor/Directives/InactiveUnknownConditions.as");
		const FString ScriptSource = TEXT(
			"#if EDITOR\n"
			"int Entry()\n"
			"{\n"
			"    return 7;\n"
			"}\n"
			"#else\n"
			"#if UNKNOWN_FLAG\n"
			"int Entry()\n"
			"{\n"
			"    return 1;\n"
			"}\n"
			"#elif UNKNOWN_FLAG\n"
			"int Entry()\n"
			"{\n"
			"    return 2;\n"
			"}\n"
			"#else\n"
			"int Entry()\n"
			"{\n"
			"    return 3;\n"
			"}\n"
			"#endif\n"
			"#endif\n");

		FFixtureFile File(RelativeScriptPath, ScriptSource);

		auto Result = RunPreprocess(Engine, File);
		LogProcessedCode(Result, TEXT("InactiveBranchSkipsUnknown"));

		AssertPreprocessSucceeded(*TestRunner, Result);
		AssertModuleCount(*TestRunner, Result, 1);
		AssertErrorCount(*TestRunner, Result, 0);
		AssertDiagnosticNotContains(*TestRunner, Result, TEXT("Invalid preprocessor condition"));

		const FAngelscriptModuleDesc* Module = Result.FindModule(
			TEXT("Tests.Preprocessor.Directives.InactiveUnknownConditions"));
		if (Module != nullptr)
		{
			AssertModuleCodeContains(*TestRunner, Result, *Module, TEXT("return 7;"));
			AssertModuleCodeNotContains(*TestRunner, Result, *Module, TEXT("UNKNOWN_FLAG"));
			AssertModuleCodeNotContains(*TestRunner, Result, *Module, TEXT("return 3;"));
		}

		// Compile and execute through the full pipeline
		Engine.ResetDiagnostics();
		FAngelscriptCompileTraceSummary Summary;
		const bool bCompiled = CompileModuleWithSummary(
			&Engine, ECompileType::SoftReloadOnly, ModuleName,
			RelativeScriptPath, ScriptSource, true, Summary, true);

		TestRunner->TestTrue(TEXT("Should compile successfully"), bCompiled);
		TestRunner->TestTrue(TEXT("Should report preprocessor usage"), Summary.bUsedPreprocessor);

		const bool bSummaryHasInvalidCondition = Summary.Diagnostics.ContainsByPredicate(
			[](const FAngelscriptCompileTraceDiagnosticSummary& D)
			{ return D.Message.Contains(TEXT("Invalid preprocessor condition")); });
		TestRunner->TestFalse(TEXT("Compile summary should not report invalid-condition"), bSummaryHasInvalidCondition);

		int32 EntryResult = 0;
		const bool bExecuted = bCompiled
			&& ExecuteIntFunction(&Engine, RelativeScriptPath, ModuleName, TEXT("int Entry()"), EntryResult);
		TestRunner->TestTrue(TEXT("Entry should execute"), bExecuted);
		if (bExecuted)
		{
			TestRunner->TestEqual(TEXT("Active EDITOR branch should return 7"), EntryResult, 7);
		}

		}
	}

	// ========================================================================
	// ElifShortCircuitsAfterTakenBranch — when #if EDITOR is true,
	// the following #elif UNKNOWN_FLAG is never evaluated
	// ========================================================================
	TEST_METHOD(ElifShortCircuitsAfterTakenBranch)
	{
		TUniquePtr<FAngelscriptEngine> OwnedEngine = CreateEditorEngine();
		if (!TestRunner->TestNotNull(TEXT("Should create editor engine"), OwnedEngine.Get()))
		{
			return;
		}

		FAngelscriptEngine& Engine = *OwnedEngine;
		{
		FAngelscriptEngineScope _AutoEngineScope(Engine);
		ON_SCOPE_EXIT
		{
			const TArray<TSharedRef<FAngelscriptModuleDesc>> _ActiveModules = Engine.GetActiveModules();
			for (const TSharedRef<FAngelscriptModuleDesc>& _Module : _ActiveModules)
			{
				Engine.DiscardModule(*_Module->ModuleName);
			}
		};

		static const FName ModuleName(TEXT("Tests.Preprocessor.Directives.ElifShortCircuitsAfterTakenBranch"));
		ON_SCOPE_EXIT { Engine.DiscardModule(*ModuleName.ToString()); };

		const FString RelativeScriptPath = TEXT("Tests/Preprocessor/Directives/ElifShortCircuitsAfterTakenBranch.as");
		const FString ScriptSource = TEXT(
			"#if EDITOR\n"
			"int Entry()\n"
			"{\n"
			"    return 7;\n"
			"}\n"
			"#elif UNKNOWN_FLAG\n"
			"int Entry()\n"
			"{\n"
			"    return 9;\n"
			"}\n"
			"#else\n"
			"int Entry()\n"
			"{\n"
			"    return 3;\n"
			"}\n"
			"#endif\n");

		FFixtureFile File(RelativeScriptPath, ScriptSource);

		auto Result = RunPreprocess(Engine, File);
		LogProcessedCode(Result, TEXT("ElifShortCircuits"));

		AssertPreprocessSucceeded(*TestRunner, Result);
		AssertModuleCount(*TestRunner, Result, 1);
		AssertErrorCount(*TestRunner, Result, 0);
		AssertNoDiagnostics(*TestRunner, Result);

		const FAngelscriptModuleDesc* Module = Result.FindModule(
			TEXT("Tests.Preprocessor.Directives.ElifShortCircuitsAfterTakenBranch"));
		if (Module != nullptr)
		{
			AssertModuleCodeContains(*TestRunner, Result, *Module, TEXT("return 7;"));
			AssertModuleCodeNotContains(*TestRunner, Result, *Module, TEXT("UNKNOWN_FLAG"));
		}

		// Compile and execute
		Engine.ResetDiagnostics();
		FAngelscriptCompileTraceSummary Summary;
		const bool bCompiled = CompileModuleWithSummary(
			&Engine, ECompileType::SoftReloadOnly, ModuleName,
			RelativeScriptPath, ScriptSource, true, Summary, true);

		TestRunner->TestTrue(TEXT("Should compile successfully"), bCompiled);
		TestRunner->TestTrue(TEXT("Should report preprocessor usage"), Summary.bUsedPreprocessor);
		TestRunner->TestEqual(TEXT("Should emit no compile diagnostics"), Summary.Diagnostics.Num(), 0);

		int32 EntryResult = 0;
		const bool bExecuted = bCompiled
			&& ExecuteIntFunction(&Engine, RelativeScriptPath, ModuleName, TEXT("int Entry()"), EntryResult);
		TestRunner->TestTrue(TEXT("Entry should execute"), bExecuted);
		if (bExecuted)
		{
			TestRunner->TestEqual(TEXT("Active EDITOR branch should return 7"), EntryResult, 7);
		}

		}
	}

	// ========================================================================
	// StringLiteralDoesNotTriggerDirectiveLexer — #if/#else tokens inside
	// string literals are not parsed as preprocessor directives
	// ========================================================================
	TEST_METHOD(StringLiteralDoesNotTriggerDirectiveLexer)
	{
		TUniquePtr<FAngelscriptEngine> OwnedEngine = CreateEditorEngine();
		if (!TestRunner->TestNotNull(TEXT("Should create editor engine"), OwnedEngine.Get()))
		{
			return;
		}

		FAngelscriptEngine& Engine = *OwnedEngine;
		{
		FAngelscriptEngineScope _AutoEngineScope(Engine);
		ON_SCOPE_EXIT
		{
			const TArray<TSharedRef<FAngelscriptModuleDesc>> _ActiveModules = Engine.GetActiveModules();
			for (const TSharedRef<FAngelscriptModuleDesc>& _Module : _ActiveModules)
			{
				Engine.DiscardModule(*_Module->ModuleName);
			}
		};

		static const FName ModuleName(TEXT("Tests.Preprocessor.Directives.StringLiteralDoesNotTriggerDirectiveLexer"));
		ON_SCOPE_EXIT { Engine.DiscardModule(*ModuleName.ToString()); };

		const FString RelativeScriptPath = TEXT("Tests/Preprocessor/Directives/StringLiteralDoesNotTriggerDirectiveLexer.as");
		const FString ScriptSource = TEXT(R"AS(
FString BuildMarker()
{
	return "debug #if RELEASE #else keep";
}
int Entry()
{
	return BuildMarker() == "debug #if RELEASE #else keep" ? 42 : 0;
}
)AS");

		FFixtureFile File(RelativeScriptPath, ScriptSource);

		auto Result = RunPreprocess(Engine, File);
		LogProcessedCode(Result, TEXT("StringLiteralDirective"));

		AssertPreprocessSucceeded(*TestRunner, Result);
		AssertErrorCount(*TestRunner, Result, 0);
		AssertNoDiagnostics(*TestRunner, Result);

		const FAngelscriptModuleDesc* Module = Result.FindModule(
			TEXT("Tests.Preprocessor.Directives.StringLiteralDoesNotTriggerDirectiveLexer"));
		if (Module != nullptr)
		{
			AssertModuleCodeContains(*TestRunner, Result, *Module,
				TEXT("\"debug #if RELEASE #else keep\""));
		}

		// Compile and execute
		Engine.ResetDiagnostics();
		FAngelscriptCompileTraceSummary Summary;
		const bool bCompiled = CompileModuleWithSummary(
			&Engine, ECompileType::SoftReloadOnly, ModuleName,
			RelativeScriptPath, ScriptSource, true, Summary, true);

		TestRunner->TestTrue(TEXT("Should compile successfully"), bCompiled);
		TestRunner->TestEqual(TEXT("Should emit no compile diagnostics"), Summary.Diagnostics.Num(), 0);

		int32 EntryResult = 0;
		const bool bExecuted = bCompiled
			&& ExecuteIntFunction(&Engine, RelativeScriptPath, ModuleName, TEXT("int Entry()"), EntryResult);
		TestRunner->TestTrue(TEXT("Entry should execute"), bExecuted);
		if (bExecuted)
		{
			TestRunner->TestEqual(TEXT("String comparison should match → 42"), EntryResult, 42);
		}

		}
	}

	// ========================================================================
	// CompoundConditionReportsUnsupportedSyntax — "#if EDITOR && TEST"
	// fails preprocessing with a stable diagnostic at row 1
	// ========================================================================
	TEST_METHOD(CompoundConditionReportsUnsupportedSyntax)
	{
		TestRunner->AddExpectedError(TEXT("Invalid preprocessor condition:"), EAutomationExpectedErrorFlags::Contains, 1);

		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine); FScopedModuleCleanEngine _AutoModuleClean(Engine);

		const FString RelativeScriptPath = TEXT("Tests/Preprocessor/Directives/CompoundConditionReportsUnsupportedSyntax.as");
		const FString ScriptSource = TEXT(
			"#if EDITOR && TEST\n"
			"int Entry()\n"
			"{\n"
			"    return 7;\n"
			"}\n"
			"#endif\n");

		FFixtureFile File(RelativeScriptPath, ScriptSource);

		// Use EDITOR flag so the compound condition is the actual error, not a missing flag
		auto Result = RunPreprocess(Engine, File, {{TEXT("EDITOR"), true}});
		LogProcessedCode(Result, TEXT("CompoundCondition"));

		AssertPreprocessFailed(*TestRunner, Result);
		AssertErrorCount(*TestRunner, Result, 1);
		AssertDiagnosticContains(*TestRunner, Result, TEXT("Invalid preprocessor condition: EDITOR && TEST"));
		AssertDiagnosticAt(*TestRunner, Result, TEXT("Invalid preprocessor condition"), 1, 1);
		AssertNoCompilableCode(*TestRunner, Result);

		}
	}

	// ========================================================================
	// StructuralErrorsReportStableDiagnostics — isolated #elif/#else/#endif
	// and missing #endif all produce stable error diagnostics
	// ========================================================================
	TEST_METHOD(StructuralErrorsReportStableDiagnostics)
	{
		TestRunner->AddExpectedError(TEXT("Invalid #elif, no matching #if found."), EAutomationExpectedErrorFlags::Contains, 1);
		TestRunner->AddExpectedError(TEXT("Invalid #else, no matching #if found."), EAutomationExpectedErrorFlags::Contains, 1);
		TestRunner->AddExpectedError(TEXT("Invalid #endif, no matching #if found."), EAutomationExpectedErrorFlags::Contains, 1);
		TestRunner->AddExpectedError(TEXT("Preceding preprocessor #if/#ifdef/#else was not closed, missing #endif."), EAutomationExpectedErrorFlags::Contains, 1);

		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine); FScopedModuleCleanEngine _AutoModuleClean(Engine);

		struct FDirectiveErrorCase
		{
			const TCHAR* Label;
			const TCHAR* RelativePath;
			const TCHAR* Source;
			const TCHAR* ExpectedMessage;
			int32 ExpectedRow;
		};

		const TArray<FDirectiveErrorCase> Cases = {
			{
				TEXT("Isolated #elif"),
				TEXT("Tests/Preprocessor/DirectiveErrors/InvalidElif.as"),
				TEXT("#elif EDITOR\nint Value = 1;\n"),
				TEXT("Invalid #elif, no matching #if found."),
				1
			},
			{
				TEXT("Isolated #else"),
				TEXT("Tests/Preprocessor/DirectiveErrors/InvalidElse.as"),
				TEXT("#else\nint Value = 1;\n"),
				TEXT("Invalid #else, no matching #if found."),
				1
			},
			{
				TEXT("Isolated #endif"),
				TEXT("Tests/Preprocessor/DirectiveErrors/InvalidEndif.as"),
				TEXT("#endif\nint Value = 1;\n"),
				TEXT("Invalid #endif, no matching #if found."),
				1
			},
			{
				TEXT("Missing #endif"),
				TEXT("Tests/Preprocessor/DirectiveErrors/MissingEndif.as"),
				TEXT("#if EDITOR\nint Value = 1;\n"),
				TEXT("Preceding preprocessor #if/#ifdef/#else was not closed, missing #endif."),
				1
			}
		};

		for (const FDirectiveErrorCase& Case : Cases)
		{
			Engine.ResetDiagnostics();
			Engine.LastEmittedDiagnostics.Empty();

			FFixtureFile File(Case.RelativePath, Case.Source);

			auto Result = RunPreprocess(Engine, File);
			LogProcessedCode(Result, *FString::Printf(TEXT("StructuralError_%s"), Case.Label));

			TestRunner->TestFalse(
				FString::Printf(TEXT("%s should fail preprocessing"), Case.Label),
				Result.bSuccess);
			TestRunner->TestEqual(
				FString::Printf(TEXT("%s should emit exactly one error"), Case.Label),
				Result.ErrorCount, 1);
			AssertDiagnosticContains(*TestRunner, Result, Case.ExpectedMessage);
			AssertDiagnosticAt(*TestRunner, Result, Case.ExpectedMessage, Case.ExpectedRow, 1);
			TestRunner->TestFalse(
				FString::Printf(TEXT("%s should not leave compilable code"), Case.Label),
				ContainsCompilableCode(Result));
		}

		}
	}

	// ========================================================================
	// TabSeparatedDirectiveParsing — tab between "#if" and "EDITOR" is valid;
	// nested #ifdef with tabs also works; #restrict usage with tab is parsed
	// ========================================================================
	TEST_METHOD(TabSeparatedDirectiveParsing)
	{
		TUniquePtr<FAngelscriptEngine> OwnedEngine = CreateEditorEngine();
		if (!TestRunner->TestNotNull(TEXT("Should create editor engine"), OwnedEngine.Get()))
		{
			return;
		}

		FAngelscriptEngine& Engine = *OwnedEngine;
		{
		FAngelscriptEngineScope _AutoEngineScope(Engine);
		ON_SCOPE_EXIT
		{
			const TArray<TSharedRef<FAngelscriptModuleDesc>> _ActiveModules = Engine.GetActiveModules();
			for (const TSharedRef<FAngelscriptModuleDesc>& _Module : _ActiveModules)
			{
				Engine.DiscardModule(*_Module->ModuleName);
			}
		};

		static const FName ModuleName(TEXT("Game.Preprocessor.Directives.TabSeparatedDirectiveParsing"));
		ON_SCOPE_EXIT { Engine.DiscardModule(*ModuleName.ToString()); };

		const FString RelativeScriptPath = TEXT("Game/Preprocessor/Directives/TabSeparatedDirectiveParsing.as");
		const FString ScriptSource = TEXT(
			"#if\tEDITOR\n"
			"#ifdef\tEDITOR\n"
			"int Entry()\n"
			"{\n"
			"    return 74;\n"
			"}\n"
			"#else\n"
			"int Entry()\n"
			"{\n"
			"    return 704;\n"
			"}\n"
			"#endif\n"
			"#else\n"
			"int Entry()\n"
			"{\n"
			"    return 0;\n"
			"}\n"
			"#endif\n"
			"#restrict usage allow\tGame.*\n"
			);

		FFixtureFile File(RelativeScriptPath, ScriptSource);

		auto Result = RunPreprocess(Engine, File);
		LogProcessedCode(Result, TEXT("TabSeparated"));

		AssertPreprocessSucceeded(*TestRunner, Result);
		AssertModuleCount(*TestRunner, Result, 1);
		AssertErrorCount(*TestRunner, Result, 0);
		AssertNoDiagnostics(*TestRunner, Result);

		const FAngelscriptModuleDesc* Module = Result.FindModule(
			TEXT("Game.Preprocessor.Directives.TabSeparatedDirectiveParsing"));
		if (Module != nullptr)
		{
			AssertModuleCodeContains(*TestRunner, Result, *Module, TEXT("return 74;"));
			AssertModuleCodeContains(*TestRunner, Result, *Module, TEXT("int Entry()"));
			AssertModuleCodeNotContains(*TestRunner, Result, *Module, TEXT("return 704;"));
			AssertModuleCodeNotContains(*TestRunner, Result, *Module, TEXT("return 0;"));
			AssertModuleCodeNotContains(*TestRunner, Result, *Module, TEXT("#if"));
			AssertModuleCodeNotContains(*TestRunner, Result, *Module, TEXT("#restrict"));

#if WITH_EDITOR
			TestRunner->TestEqual(
				TEXT("Should record one usage restriction"),
				Module->UsageRestrictions.Num(), 1);
			if (Module->UsageRestrictions.Num() == 1)
			{
				TestRunner->TestTrue(TEXT("Should be an allow restriction"),
					Module->UsageRestrictions[0].bIsAllow);
				TestRunner->TestEqual(TEXT("Pattern should be Game.*"),
					Module->UsageRestrictions[0].Pattern, FString(TEXT("Game.*")));
			}
#endif
		}

		// Compile and execute
		Engine.ResetDiagnostics();
		FAngelscriptCompileTraceSummary Summary;
		const bool bCompiled = CompileModuleWithSummary(
			&Engine, ECompileType::SoftReloadOnly, ModuleName,
			RelativeScriptPath, ScriptSource, true, Summary, true);

		TestRunner->TestTrue(TEXT("Should compile successfully"), bCompiled);
		TestRunner->TestTrue(TEXT("Should report preprocessor usage"), Summary.bUsedPreprocessor);
		TestRunner->TestEqual(TEXT("Should emit no compile diagnostics"), Summary.Diagnostics.Num(), 0);

		int32 EntryResult = 0;
		const bool bExecuted = bCompiled
			&& ExecuteIntFunction(&Engine, RelativeScriptPath, ModuleName, TEXT("int Entry()"), EntryResult);
		TestRunner->TestTrue(TEXT("Entry should execute"), bExecuted);
		if (bExecuted)
		{
			TestRunner->TestEqual(TEXT("Nested tab-separated EDITOR branch should return 74"), EntryResult, 74);
		}

		}
	}

	// ========================================================================
	// IncludeDirectiveProducesDeterministicResult — #include "Shared.as"
	// always fails with a stable unsupported-directive diagnostic
	// ========================================================================
	TEST_METHOD(IncludeDirectiveProducesDeterministicResult)
	{
		static const FString ExpectedDiagnostic(TEXT("Unsupported preprocessor directive '#include'. Use import or automatic imports instead."));

		TestRunner->AddExpectedError(*ExpectedDiagnostic, EAutomationExpectedErrorFlags::Contains, 2);

		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine); FScopedModuleCleanEngine _AutoModuleClean(Engine);

		static const FName ModuleName(TEXT("Tests.Preprocessor.Directives.IncludeDirectiveProducesDeterministicResult"));
		const FString RelativeScriptPath = TEXT("Tests/Preprocessor/Directives/IncludeDirectiveProducesDeterministicResult.as");
		const FString ScriptSource = TEXT(
			"#include \"Shared.as\"\n"
			"int Entry()\n"
			"{\n"
			"    return 42;\n"
			"}\n");

		FFixtureFile File(RelativeScriptPath, ScriptSource);

		auto Result = RunPreprocess(Engine, File);
		LogProcessedCode(Result, TEXT("IncludeDirective"));

		AssertPreprocessFailed(*TestRunner, Result);
		AssertErrorCount(*TestRunner, Result, 1);
		AssertDiagnosticContains(*TestRunner, Result, ExpectedDiagnostic);
		AssertDiagnosticAt(*TestRunner, Result, ExpectedDiagnostic, 1);

		// Verify it also fails through the compile pipeline
		Engine.ResetDiagnostics();
		FAngelscriptCompileTraceSummary Summary;
		const bool bCompiled = CompileModuleWithSummary(
			&Engine, ECompileType::SoftReloadOnly, ModuleName,
			RelativeScriptPath, ScriptSource, true, Summary, true);

		TestRunner->TestFalse(TEXT("Should fail through compile pipeline"), bCompiled);
		TestRunner->TestTrue(TEXT("Should report preprocessor usage"), Summary.bUsedPreprocessor);
		TestRunner->TestEqual(TEXT("CompileResult should be Error"), Summary.CompileResult, ECompileResult::Error);
		TestRunner->TestEqual(TEXT("Should not compile any module"), Summary.CompiledModuleCount, 0);

		// Verify the same diagnostic is collected by the compile-trace summary.
		// Note: CompileModuleWithSummary uses an internal preprocessor instance,
		// so the per-engine Diagnostics map is not the right place to look —
		// the summary's Diagnostics array is.
		const bool bSummaryHasIncludeDiag = Summary.Diagnostics.ContainsByPredicate(
			[](const FAngelscriptCompileTraceDiagnosticSummary& D)
			{ return D.bIsError && D.Message.Contains(ExpectedDiagnostic); });
		TestRunner->TestTrue(
			TEXT("Compile-trace summary should record the #include diagnostic"),
			bSummaryHasIncludeDiag);

		}
	}

	// ========================================================================
	// DeeplyNestedConditionals — 3+ layers of nested #if/#ifdef/#ifndef
	// all resolve correctly: only the innermost active branch survives
	//
	// Note: flag overrides are only honored by FAngelscriptPreprocessor instances
	// passed to RunPreprocess. The compile pipeline (CompileModuleWithSummary)
	// constructs its own preprocessor without overrides, so we cannot validate
	// branch selection through ExecuteIntFunction here without altering the
	// compile helper. We therefore validate branch selection purely on the
	// preprocessor output (module code text).
	// ========================================================================
	TEST_METHOD(DeeplyNestedConditionals)
	{
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine); FScopedModuleCleanEngine _AutoModuleClean(Engine);

		const FString RelativeScriptPath = TEXT("Tests/Preprocessor/Directives/DeeplyNestedConditionals.as");
		const FString ScriptSource = TEXT(
			"#ifdef OUTER\n"
			"  #ifdef MIDDLE\n"
			"    #ifdef INNER\n"
			"    int Entry() { return 1; }\n"
			"    #else\n"
			"    int Entry() { return 2; }\n"
			"    #endif\n"
			"  #else\n"
			"  int Entry() { return 3; }\n"
			"  #endif\n"
			"#else\n"
			"  #ifndef MIDDLE\n"
			"    #ifdef INNER\n"
			"    int Entry() { return 4; }\n"
			"    #else\n"
			"    int Entry() { return 5; }\n"
			"    #endif\n"
			"  #else\n"
			"  int Entry() { return 6; }\n"
			"  #endif\n"
			"#endif\n");

		// OUTER=false → #else branch → MIDDLE not defined → #ifndef taken → INNER=true → return 4
		FFixtureFile File(RelativeScriptPath, ScriptSource);
		auto Result = RunPreprocess(Engine, File,
			{{TEXT("OUTER"), false}, {TEXT("MIDDLE"), false}, {TEXT("INNER"), true}});
		LogProcessedCode(Result, TEXT("DeeplyNested_Custom"));

		AssertPreprocessSucceeded(*TestRunner, Result);
		AssertModuleCount(*TestRunner, Result, 1);
		AssertErrorCount(*TestRunner, Result, 0);
		AssertNoDiagnostics(*TestRunner, Result);

		FAngelscriptModuleDesc* Module = AssertModuleExists(
			*TestRunner, Result, TEXT("Tests.Preprocessor.Directives.DeeplyNestedConditionals"));
		if (Module != nullptr)
		{
			AssertModuleCodeContains(*TestRunner, Result, *Module, TEXT("return 4;"));
			AssertModuleCodeNotContains(*TestRunner, Result, *Module, TEXT("return 1;"));
			AssertModuleCodeNotContains(*TestRunner, Result, *Module, TEXT("return 2;"));
			AssertModuleCodeNotContains(*TestRunner, Result, *Module, TEXT("return 3;"));
			AssertModuleCodeNotContains(*TestRunner, Result, *Module, TEXT("return 5;"));
			AssertModuleCodeNotContains(*TestRunner, Result, *Module, TEXT("return 6;"));
		}

		// Cross-check: with no flag overrides, the default-undefined path
		// (OUTER undef → #else; MIDDLE undef → #ifndef true; INNER undef → #else)
		// should select 'return 5;'. This validates that flag overrides actually
		// drove the result above, instead of being a no-op.
		auto DefaultResult = RunPreprocess(Engine, File);
		LogProcessedCode(DefaultResult, TEXT("DeeplyNested_Default"));
		AssertPreprocessSucceeded(*TestRunner, DefaultResult);
		FAngelscriptModuleDesc* DefaultModule = AssertModuleExists(
			*TestRunner, DefaultResult, TEXT("Tests.Preprocessor.Directives.DeeplyNestedConditionals"));
		if (DefaultModule != nullptr)
		{
			AssertModuleCodeContains(*TestRunner, DefaultResult, *DefaultModule, TEXT("return 5;"));
			AssertModuleCodeNotContains(*TestRunner, DefaultResult, *DefaultModule, TEXT("return 4;"));
		}

		}
	}

private:
	// Helper to create an editor-configured engine for tests that need #if EDITOR
	static TUniquePtr<FAngelscriptEngine> CreateEditorEngine()
	{
		FAngelscriptEngineConfig Config;
		Config.bIsEditor = true;
		FAngelscriptEngineDependencies Dependencies = FAngelscriptEngineDependencies::CreateDefault();
		return CreateScriptScanFreeFullEngineForTesting(Config, Dependencies);
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
