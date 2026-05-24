// ============================================================================
// AngelscriptPreprocessorImportTests.cpp
//
// Preprocessor tests for import system: circular dependencies, automatic import
// compatibility, missing semicolons, trailing comments, deduplication,
// topological ordering, and warning config.
//
// Migrated from:
//   - AngelscriptPreprocessorImportTests.cpp (Circular, AutomaticCompat, MissingSemicolon, TrailingComment)
//   - AngelscriptPreprocessorImportDedupTests.cpp (DuplicateDedup)
//   - AngelscriptPreprocessorImportTopologyTests.cpp (TopologicalOrder)
//   - AngelscriptPreprocessorImportModeTests.cpp (AutomaticWarningConfig)
//
// Automation prefix: Angelscript.TestModule.Preprocessor.Import.*
// ============================================================================

#include "CQTest.h"
#include "Preprocessor/AngelscriptPreprocessorTestHelpers.h"

#include "AngelscriptSettings.h"
#include "Misc/ScopeExit.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace PreprocessorTestHelpers;

// ============================================================================
// Test class
// ============================================================================

TEST_CLASS_WITH_FLAGS(FAngelscriptPreprocessorImportTest,
	"Angelscript.TestModule.Preprocessor.Import",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	// ========================================================================
	// CircularDependencyReportsChain — A imports B, B imports A → failure
	// with a diagnostic chain listing both modules
	// ========================================================================
	TEST_METHOD(CircularDependencyReportsChain)
	{
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine); FScopedModuleCleanEngine _AutoModuleClean(Engine);

		TestRunner->AddExpectedError(
			TEXT("Detected circular import of module Tests.Preprocessor.ImportCycles.CircularA. Import chain:"),
			EAutomationExpectedErrorFlags::Contains, 1);
		TestRunner->AddExpectedError(
			TEXT("=> Tests.Preprocessor.ImportCycles.CircularB"),
			EAutomationExpectedErrorFlags::Contains, 1);
		TestRunner->AddExpectedError(
			TEXT("=> Tests.Preprocessor.ImportCycles.CircularA"),
			EAutomationExpectedErrorFlags::Contains, 1);

		FFixtureFile FileA(TEXT("Tests/Preprocessor/ImportCycles/CircularA.as"), TEXT(R"(
import Tests.Preprocessor.ImportCycles.CircularB;
int FromA()
{
    return FromB();
}
)"));

		FFixtureFile FileB(TEXT("Tests/Preprocessor/ImportCycles/CircularB.as"), TEXT(R"(
import Tests.Preprocessor.ImportCycles.CircularA;
int FromB()
{
    return FromA();
}
)"));

		TArray<FFixtureFile> Files;
		Files.Emplace(MoveTemp(FileA));
		Files.Emplace(MoveTemp(FileB));

		auto Result = RunPreprocess(Engine, Files);
		LogProcessedCode(Result, TEXT("CircularDependency"));

		AssertPreprocessFailed(*TestRunner, Result);
		AssertDiagnosticContains(*TestRunner, Result, TEXT("Detected circular import"));
		AssertDiagnosticContains(*TestRunner, Result, TEXT("Tests.Preprocessor.ImportCycles.CircularA"));
		AssertDiagnosticContains(*TestRunner, Result, TEXT("Tests.Preprocessor.ImportCycles.CircularB"));
		TestRunner->TestTrue(
			TEXT("Circular import should emit at least 3 error diagnostics (headline + chain)"),
			Result.ErrorCount >= 3);
		TestRunner->TestEqual(
			TEXT("Circular import should not register active modules"),
			Engine.GetActiveModules().Num(), 0);

		}
	}

	// ========================================================================
	// AutomaticModeManualImportCompatibility — in automatic import mode,
	// a manual import statement is still parsed and tracked
	// ========================================================================
	TEST_METHOD(AutomaticModeManualImportCompatibility)
	{
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine); FScopedModuleCleanEngine _AutoModuleClean(Engine);

		TestRunner->TestTrue(
			TEXT("Should run with automatic imports enabled"),
			Engine.ShouldUseAutomaticImportMethod());

		FFixtureFile SharedFile(TEXT("Tests/Preprocessor/AutomaticImportCompat/Shared.as"), TEXT(R"(
int SharedValue()
{
    return 11;
}
)"));

		FFixtureFile ImportingFile(TEXT("Tests/Preprocessor/AutomaticImportCompat/UsesManualImport.as"), TEXT(R"(
import Tests.Preprocessor.AutomaticImportCompat.Shared;
int UseShared()
{
    return SharedValue();
}
)"));

		TArray<FFixtureFile> Files;
		Files.Emplace(MoveTemp(SharedFile));
		Files.Emplace(MoveTemp(ImportingFile));

		// Do NOT disable automatic imports for this test
		auto Result = RunPreprocess(Engine, Files, {}, /*bDisableAutomaticImports=*/ false);
		LogProcessedCode(Result, TEXT("AutomaticModeManualImport"));

		AssertPreprocessSucceeded(*TestRunner, Result);
		AssertModuleCount(*TestRunner, Result, 2);

		const FAngelscriptModuleDesc* SharedModule = AssertModuleExists(
			*TestRunner, Result, TEXT("Tests.Preprocessor.AutomaticImportCompat.Shared"));
		const FAngelscriptModuleDesc* ImportingModule = AssertModuleExists(
			*TestRunner, Result, TEXT("Tests.Preprocessor.AutomaticImportCompat.UsesManualImport"));

		if (SharedModule != nullptr)
		{
			AssertImportCount(*TestRunner, *SharedModule, 0);
		}

		if (ImportingModule != nullptr)
		{
			AssertImportCount(*TestRunner, *ImportingModule, 1);
			AssertModuleImports(*TestRunner, *ImportingModule,
				TEXT("Tests.Preprocessor.AutomaticImportCompat.Shared"));
			AssertModuleCodeNotContains(*TestRunner, Result, *ImportingModule,
				TEXT("import Tests.Preprocessor.AutomaticImportCompat.Shared;"));
		}

		// Check warning behavior based on settings
		const bool bWarningsEnabled = GetDefault<UAngelscriptSettings>()->bWarnOnManualImportStatements;
		const int32 ExpectedWarningCount = bWarningsEnabled ? 1 : 0;
		const int32 NonErrorCount = Result.AllDiagnostics.Num() - Result.ErrorCount;

		TestRunner->TestEqual(TEXT("Should emit no errors"), Result.ErrorCount, 0);
		TestRunner->TestEqual(
			TEXT("Warning count should match warning policy"),
			NonErrorCount, ExpectedWarningCount);

		if (bWarningsEnabled && Result.AllDiagnostics.Num() > 0)
		{
			AssertDiagnosticContains(*TestRunner, Result,
				TEXT("Automatic imports are active, import statements will be ignored."));
		}

		}
	}

	// ========================================================================
	// MissingSemicolonReportsSyntax — "import Foo.Bar\n" (no semicolon)
	// fails with a dedicated syntax diagnostic at row 1
	// ========================================================================
	TEST_METHOD(MissingSemicolonReportsSyntax)
	{
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine); FScopedModuleCleanEngine _AutoModuleClean(Engine);

		TestRunner->AddExpectedError(
			TEXT("Import statement is missing terminating ';'."),
			EAutomationExpectedErrorFlags::Contains, 1);

		FFixtureFile SharedFile(TEXT("Tests/Preprocessor/MissingSemicolon/Shared.as"), TEXT(R"(
int SharedValue()
{
    return 11;
}
)"));

		FFixtureFile BrokenFile(TEXT("Tests/Preprocessor/MissingSemicolon/BrokenImport.as"), TEXT(R"(
import Tests.Preprocessor.MissingSemicolon.Shared
int UseShared()
{
    return SharedValue();
}
)"));

		TArray<FFixtureFile> Files;
		Files.Emplace(MoveTemp(SharedFile));
		Files.Emplace(MoveTemp(BrokenFile));

		auto Result = RunPreprocess(Engine, Files);
		LogProcessedCode(Result, TEXT("MissingSemicolon"));

		AssertPreprocessFailed(*TestRunner, Result);
		AssertErrorCount(*TestRunner, Result, 1);
		AssertDiagnosticContains(*TestRunner, Result, TEXT("Import statement is missing terminating ';'."));
		AssertDiagnosticAt(*TestRunner, Result, TEXT("Import statement is missing terminating"), 1);

		const FAngelscriptModuleDesc* BrokenModule = Result.FindModule(
			TEXT("Tests.Preprocessor.MissingSemicolon.BrokenImport"));
		if (TestRunner->TestNotNull(TEXT("Broken module should still exist for diagnostics"), BrokenModule))
		{
			TestRunner->TestEqual(
				TEXT("Broken module should not record malformed imports"),
				BrokenModule->ImportedModules.Num(), 0);
		}

		}
	}

	// ========================================================================
	// TrailingBlockCommentDoesNotPolluteModuleName — "import Foo /* comment */;"
	// correctly strips the comment from the module name
	// ========================================================================
	TEST_METHOD(TrailingBlockCommentDoesNotPolluteModuleName)
	{
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine); FScopedModuleCleanEngine _AutoModuleClean(Engine);

		FFixtureFile SharedFile(TEXT("Tests/Preprocessor/ImportTrailingBlockComment/Shared.as"), TEXT(R"(
int SharedValue()
{
    return 11;
}
)"));

		FFixtureFile ImportingFile(TEXT("Tests/Preprocessor/ImportTrailingBlockComment/UsesShared.as"), TEXT(R"(
import Tests.Preprocessor.ImportTrailingBlockComment.Shared /* shared helpers */;
int Entry()
{
    return SharedValue();
}
)"));

		TArray<FFixtureFile> Files;
		Files.Emplace(MoveTemp(SharedFile));
		Files.Emplace(MoveTemp(ImportingFile));

		auto Result = RunPreprocess(Engine, Files);
		LogProcessedCode(Result, TEXT("TrailingBlockComment"));

		AssertPreprocessSucceeded(*TestRunner, Result);
		AssertErrorCount(*TestRunner, Result, 0);
		AssertNoDiagnostics(*TestRunner, Result);
		AssertModuleCount(*TestRunner, Result, 2);

		const FAngelscriptModuleDesc* ImportingModule = AssertModuleExists(
			*TestRunner, Result, TEXT("Tests.Preprocessor.ImportTrailingBlockComment.UsesShared"));
		if (ImportingModule != nullptr)
		{
			AssertImportCount(*TestRunner, *ImportingModule, 1);
			AssertModuleImports(*TestRunner, *ImportingModule,
				TEXT("Tests.Preprocessor.ImportTrailingBlockComment.Shared"));
			// Verify comment is NOT in the import name
			TestRunner->TestFalse(
				TEXT("Should not include block comment in module name"),
				ImportingModule->ImportedModules.Contains(
					TEXT("Tests.Preprocessor.ImportTrailingBlockComment.Shared /* shared helpers */")));
			AssertModuleCodeNotContains(*TestRunner, Result, *ImportingModule,
				TEXT("import Tests.Preprocessor.ImportTrailingBlockComment.Shared /* shared helpers */;"));
		}

		// Compile and verify execution
		Engine.ResetDiagnostics();
		TGuardValue<bool> AutomaticImportGuard(Engine.bUseAutomaticImportMethod, false);
		FScopedAutomaticImportsOverride AutomaticImportsOverride(Engine.GetScriptEngine());

		TArray<TSharedRef<FAngelscriptModuleDesc>> CompiledModules;
		const ECompileResult CompileResult = Engine.CompileModules(
			ECompileType::SoftReloadOnly, Result.Modules, CompiledModules);

		TestRunner->TestEqual(TEXT("Should compile as FullyHandled"),
			CompileResult, ECompileResult::FullyHandled);
		TestRunner->TestEqual(TEXT("Should compile two modules"), CompiledModules.Num(), 2);

		if (CompileResult == ECompileResult::FullyHandled)
		{
			TSharedPtr<FAngelscriptModuleDesc> CompiledConsumer =
				Engine.GetModule(TEXT("Tests.Preprocessor.ImportTrailingBlockComment.UsesShared"));
			if (TestRunner->TestTrue(TEXT("Consumer should be registered"), CompiledConsumer.IsValid())
				&& TestRunner->TestNotNull(TEXT("Should have script module"), CompiledConsumer->ScriptModule))
			{
				asIScriptFunction* EntryFunction = GetFunctionByDecl(
					*TestRunner, *CompiledConsumer->ScriptModule, TEXT("int Entry()"));
				if (EntryFunction != nullptr)
				{
					int32 EntryResult = 0;
					if (ExecuteIntFunction(*TestRunner, Engine, *EntryFunction, EntryResult))
					{
						TestRunner->TestEqual(TEXT("Should execute imported function → 11"), EntryResult, 11);
					}
				}
			}
		}

		}
	}

	// ========================================================================
	// DuplicateStatementsDeduplicateDependency — "import Foo;\nimport Foo;\n"
	// deduplicates to one ImportedModules entry
	// ========================================================================
	TEST_METHOD(DuplicateStatementsDeduplicateDependency)
	{
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine); FScopedModuleCleanEngine _AutoModuleClean(Engine);

		FFixtureFile SharedFile(TEXT("Tests/Preprocessor/ImportDedup/Shared.as"), TEXT(R"(
int SharedValue()
{
    return 17;
}
)"));

		FFixtureFile ConsumerFile(TEXT("Tests/Preprocessor/ImportDedup/Consumer.as"), TEXT(R"(
import Tests.Preprocessor.ImportDedup.Shared;
import Tests.Preprocessor.ImportDedup.Shared;
int Entry()
{
    return SharedValue();
}
)"));

		TArray<FFixtureFile> Files;
		Files.Emplace(MoveTemp(ConsumerFile));
		Files.Emplace(MoveTemp(SharedFile));

		auto Result = RunPreprocess(Engine, Files);
		LogProcessedCode(Result, TEXT("DuplicateDedup"));

		AssertPreprocessSucceeded(*TestRunner, Result);
		AssertModuleCount(*TestRunner, Result, 2);
		AssertErrorCount(*TestRunner, Result, 0);
		AssertNoDiagnostics(*TestRunner, Result);

		// Verify topological order: Shared before Consumer
		TestRunner->TestEqual(
			TEXT("Module order should be provider-first"),
			Result.ModuleOrder(),
			FString(TEXT("Tests.Preprocessor.ImportDedup.Shared -> Tests.Preprocessor.ImportDedup.Consumer")));

		const FAngelscriptModuleDesc* SharedModule = AssertModuleExists(
			*TestRunner, Result, TEXT("Tests.Preprocessor.ImportDedup.Shared"));
		const FAngelscriptModuleDesc* ConsumerModule = AssertModuleExists(
			*TestRunner, Result, TEXT("Tests.Preprocessor.ImportDedup.Consumer"));

		if (SharedModule != nullptr)
		{
			AssertImportCount(*TestRunner, *SharedModule, 0);
		}

		if (ConsumerModule != nullptr)
		{
			AssertImportCount(*TestRunner, *ConsumerModule, 1);
			AssertModuleImports(*TestRunner, *ConsumerModule, TEXT("Tests.Preprocessor.ImportDedup.Shared"));
			AssertModuleCodeNotContains(*TestRunner, Result, *ConsumerModule,
				TEXT("import Tests.Preprocessor.ImportDedup.Shared;"));
		}

		// Compile and execute
		if (Result.bSuccess)
		{
			TGuardValue<bool> AutomaticImportGuard(Engine.bUseAutomaticImportMethod, false);
			FScopedAutomaticImportsOverride AutomaticImportsOverride(Engine.GetScriptEngine());

			TArray<TSharedRef<FAngelscriptModuleDesc>> CompiledModules;
			const ECompileResult CompileResult = Engine.CompileModules(
				ECompileType::SoftReloadOnly, Result.Modules, CompiledModules);

			TestRunner->TestTrue(TEXT("Deduplicated imports should compile"),
				CompileResult == ECompileResult::FullyHandled || CompileResult == ECompileResult::PartiallyHandled);
			TestRunner->TestEqual(TEXT("Should compile two modules"), CompiledModules.Num(), 2);

			if (ConsumerModule != nullptr && ConsumerModule->ScriptModule != nullptr)
			{
				asIScriptFunction* EntryFunction = GetFunctionByDecl(
					*TestRunner, *ConsumerModule->ScriptModule, TEXT("int Entry()"));
				if (EntryFunction != nullptr)
				{
					int32 EntryResult = 0;
					if (ExecuteIntFunction(*TestRunner, Engine, *EntryFunction, EntryResult))
					{
						TestRunner->TestEqual(TEXT("Should execute through deduplicated import → 17"),
							EntryResult, 17);
					}
				}
			}
		}

		}
	}

	// ========================================================================
	// TopologicalOrderRespectsDependencyChain — A→B→C chain gets ordered C→B→A
	// (providers before consumers)
	// ========================================================================
	TEST_METHOD(TopologicalOrderRespectsDependencyChain)
	{
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine); FScopedModuleCleanEngine _AutoModuleClean(Engine);

		FFixtureFile BaseFile(TEXT("Tests/Preprocessor/ImportTopology/Base.as"), TEXT(R"(
int BaseValue()
{
    return 2;
}
)"));

		FFixtureFile SharedFile(TEXT("Tests/Preprocessor/ImportTopology/Shared.as"), TEXT(R"(
import Tests.Preprocessor.ImportTopology.Base;
int SharedValue()
{
    return BaseValue() + 3;
}
)"));

		FFixtureFile ConsumerFile(TEXT("Tests/Preprocessor/ImportTopology/Consumer.as"), TEXT(R"(
import Tests.Preprocessor.ImportTopology.Shared;
int Entry()
{
    return SharedValue() + 5;
}
)"));

		// Add in reverse order to test topological sort
		TArray<FFixtureFile> Files;
		Files.Emplace(MoveTemp(ConsumerFile));
		Files.Emplace(MoveTemp(SharedFile));
		Files.Emplace(MoveTemp(BaseFile));

		auto Result = RunPreprocess(Engine, Files);
		LogProcessedCode(Result, TEXT("TopologicalOrder"));

		AssertPreprocessSucceeded(*TestRunner, Result);
		AssertModuleCount(*TestRunner, Result, 3);
		AssertErrorCount(*TestRunner, Result, 0);
		AssertNoDiagnostics(*TestRunner, Result);

		TestRunner->TestEqual(
			TEXT("Module order should be Base -> Shared -> Consumer"),
			Result.ModuleOrder(),
			FString(TEXT("Tests.Preprocessor.ImportTopology.Base -> Tests.Preprocessor.ImportTopology.Shared -> Tests.Preprocessor.ImportTopology.Consumer")));

		const FAngelscriptModuleDesc* BaseModule = AssertModuleExists(
			*TestRunner, Result, TEXT("Tests.Preprocessor.ImportTopology.Base"));
		const FAngelscriptModuleDesc* SharedModule = AssertModuleExists(
			*TestRunner, Result, TEXT("Tests.Preprocessor.ImportTopology.Shared"));
		const FAngelscriptModuleDesc* ConsumerModule = AssertModuleExists(
			*TestRunner, Result, TEXT("Tests.Preprocessor.ImportTopology.Consumer"));

		if (BaseModule != nullptr)
		{
			AssertImportCount(*TestRunner, *BaseModule, 0);
		}

		if (SharedModule != nullptr)
		{
			AssertImportCount(*TestRunner, *SharedModule, 1);
			AssertModuleImports(*TestRunner, *SharedModule, TEXT("Tests.Preprocessor.ImportTopology.Base"));
			AssertModuleCodeNotContains(*TestRunner, Result, *SharedModule,
				TEXT("import Tests.Preprocessor.ImportTopology.Base;"));
		}

		if (ConsumerModule != nullptr)
		{
			AssertImportCount(*TestRunner, *ConsumerModule, 1);
			AssertModuleImports(*TestRunner, *ConsumerModule, TEXT("Tests.Preprocessor.ImportTopology.Shared"));
			AssertModuleCodeNotContains(*TestRunner, Result, *ConsumerModule,
				TEXT("import Tests.Preprocessor.ImportTopology.Shared;"));
		}

		}
	}

	// ========================================================================
	// AutomaticWarningRespectsConfig — bWarnOnManualImportStatements toggles
	// whether a compatibility warning is emitted
	// ========================================================================
	TEST_METHOD(AutomaticWarningRespectsConfig)
	{
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine); FScopedModuleCleanEngine _AutoModuleClean(Engine);

		if (!TestRunner->TestTrue(
			TEXT("Should run with automatic imports enabled"),
			Engine.ShouldUseAutomaticImportMethod()))
		{
			return;
		}

		UAngelscriptSettings* Settings = GetMutableDefault<UAngelscriptSettings>();
		if (!TestRunner->TestNotNull(TEXT("Should access mutable settings"), Settings))
		{
			return;
		}

		const bool PreviousWarnSetting = Settings->bWarnOnManualImportStatements;
		ON_SCOPE_EXIT { Settings->bWarnOnManualImportStatements = PreviousWarnSetting; };

		struct FWarningTestCase
		{
			const TCHAR* Label;
			bool bWarnOnManualImport;
			int32 ExpectedWarningCount;
		};

		const TArray<FWarningTestCase> Cases = {
			{TEXT("WarningsEnabled"), true, 1},
			{TEXT("WarningsDisabled"), false, 0},
		};

		for (const FWarningTestCase& Case : Cases)
		{
			Settings->bWarnOnManualImportStatements = Case.bWarnOnManualImport;
			Engine.ResetDiagnostics();

			FFixtureFile ProviderFile(TEXT("Tests/Preprocessor/ImportMode/Shared.as"), TEXT(R"(
int SharedValue()
{
    return 11;
}
)"));

			FFixtureFile ConsumerFile(TEXT("Tests/Preprocessor/ImportMode/Consumer.as"), TEXT(R"(
import Tests.Preprocessor.ImportMode.Shared;
int Entry()
{
    return SharedValue();
}
)"));

			TArray<FFixtureFile> Files;
			Files.Emplace(MoveTemp(ProviderFile));
			Files.Emplace(MoveTemp(ConsumerFile));

			auto Result = RunPreprocess(Engine, Files, {}, /*bDisableAutomaticImports=*/ false);
			LogProcessedCode(Result, *FString::Printf(TEXT("AutomaticWarning_%s"), Case.Label));

			TestRunner->TestTrue(
				FString::Printf(TEXT("%s should preprocess successfully"), Case.Label),
				Result.bSuccess);
			TestRunner->TestEqual(
				FString::Printf(TEXT("%s should emit two modules"), Case.Label),
				Result.Modules.Num(), 2);
			TestRunner->TestEqual(
				FString::Printf(TEXT("%s should emit no errors"), Case.Label),
				Result.ErrorCount, 0);
			TestRunner->TestEqual(
				FString::Printf(TEXT("%s should emit expected warning count"), Case.Label),
				Result.AllDiagnostics.Num(), Case.ExpectedWarningCount);

			const FAngelscriptModuleDesc* ConsumerModule = Result.FindModule(
				TEXT("Tests.Preprocessor.ImportMode.Consumer"));
			if (ConsumerModule != nullptr)
			{
				AssertImportCount(*TestRunner, *ConsumerModule, 1);
				AssertModuleImports(*TestRunner, *ConsumerModule,
					TEXT("Tests.Preprocessor.ImportMode.Shared"));
				AssertModuleCodeNotContains(*TestRunner, Result, *ConsumerModule,
					TEXT("import Tests.Preprocessor.ImportMode.Shared;"));
			}

			if (Case.bWarnOnManualImport && Result.AllDiagnostics.Num() > 0)
			{
				TestRunner->TestFalse(
					FString::Printf(TEXT("%s warning should not be an error"), Case.Label),
					Result.AllDiagnostics[0].bIsError);
				TestRunner->TestTrue(
					FString::Printf(TEXT("%s warning should mention automatic imports"), Case.Label),
					Result.AllDiagnostics[0].Message.Contains(
						TEXT("Automatic imports are active, import statements will be ignored.")));
			}
		}

		}
	}

	// ========================================================================
	// ImportInsideConditionalBranch — import statement inside an active #if
	// branch is honored; import inside a dead branch is ignored
	// ========================================================================
	TEST_METHOD(ImportInsideConditionalBranch)
	{
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine); FScopedModuleCleanEngine _AutoModuleClean(Engine);

		FFixtureFile SharedFile(TEXT("Tests/Preprocessor/ImportConditional/Shared.as"), TEXT(R"(
int SharedValue()
{
    return 42;
}
)"));

		FFixtureFile ConsumerFile(TEXT("Tests/Preprocessor/ImportConditional/Consumer.as"), TEXT(R"(
#ifdef USESHARED
import Tests.Preprocessor.ImportConditional.Shared;
#endif
int Entry()
{
#ifdef USESHARED
    return SharedValue();
#else
    return 99;
#endif
}
)"));

		// Case 1: USESHARED=true → import is active
		{
			TArray<FFixtureFile> Files;
			Files.Emplace(TEXT("Tests/Preprocessor/ImportConditional/Shared.as"), TEXT(R"(
int SharedValue()
{
    return 42;
}
)"));
			Files.Emplace(TEXT("Tests/Preprocessor/ImportConditional/Consumer.as"), TEXT(R"(
#ifdef USESHARED
import Tests.Preprocessor.ImportConditional.Shared;
#endif
int Entry()
{
#ifdef USESHARED
    return SharedValue();
#else
    return 99;
#endif
}
)"));

			auto Result = RunPreprocess(Engine, Files, {{TEXT("USESHARED"), true}});
			LogProcessedCode(Result, TEXT("ImportConditional_Active"));

			AssertPreprocessSucceeded(*TestRunner, Result);
			AssertModuleCount(*TestRunner, Result, 2);

			const FAngelscriptModuleDesc* Consumer = Result.FindModule(
				TEXT("Tests.Preprocessor.ImportConditional.Consumer"));
			if (Consumer != nullptr)
			{
				AssertImportCount(*TestRunner, *Consumer, 1);
				AssertModuleImports(*TestRunner, *Consumer,
					TEXT("Tests.Preprocessor.ImportConditional.Shared"));
				AssertModuleCodeContains(*TestRunner, Result, *Consumer, TEXT("return SharedValue();"));
				AssertModuleCodeNotContains(*TestRunner, Result, *Consumer, TEXT("return 99;"));
			}
		}

		// Case 2: USESHARED=false → import is in dead branch, ignored
		{
			TArray<FFixtureFile> Files2;
			Files2.Emplace(TEXT("Tests/Preprocessor/ImportConditional/Shared2.as"), TEXT(R"(
int SharedValue()
{
    return 42;
}
)"));
			Files2.Emplace(TEXT("Tests/Preprocessor/ImportConditional/Consumer2.as"), TEXT(R"(
#ifdef USESHARED
import Tests.Preprocessor.ImportConditional.Shared2;
#endif
int Entry()
{
#ifdef USESHARED
    return SharedValue();
#else
    return 99;
#endif
}
)"));

			auto Result2 = RunPreprocess(Engine, Files2, {{TEXT("USESHARED"), false}});
			LogProcessedCode(Result2, TEXT("ImportConditional_Dead"));

			AssertPreprocessSucceeded(*TestRunner, Result2);

			const FAngelscriptModuleDesc* Consumer2 = Result2.FindModule(
				TEXT("Tests.Preprocessor.ImportConditional.Consumer2"));
			if (Consumer2 != nullptr)
			{
				AssertImportCount(*TestRunner, *Consumer2, 0);
				AssertModuleCodeContains(*TestRunner, Result2, *Consumer2, TEXT("return 99;"));
				AssertModuleCodeNotContains(*TestRunner, Result2, *Consumer2, TEXT("SharedValue"));
			}
		}

		}
	}

	// ========================================================================
	// WideImportGraph — 5 modules in a fan-out/fan-in pattern all resolve
	// correctly with proper topological order
	// ========================================================================
	TEST_METHOD(WideImportGraph)
	{
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine); FScopedModuleCleanEngine _AutoModuleClean(Engine);

		// Graph: Root has no imports. A/B/C all import Root. Consumer imports A, B, C.
		// Expected order: Root → A/B/C (any order) → Consumer
		FFixtureFile RootFile(TEXT("Tests/Preprocessor/WideGraph/Root.as"), TEXT(R"(
int RootValue()
{
    return 1;
}
)"));

		FFixtureFile FileA(TEXT("Tests/Preprocessor/WideGraph/A.as"), TEXT(R"(
import Tests.Preprocessor.WideGraph.Root;
int ValueA()
{
    return RootValue() + 10;
}
)"));

		FFixtureFile FileB(TEXT("Tests/Preprocessor/WideGraph/B.as"), TEXT(R"(
import Tests.Preprocessor.WideGraph.Root;
int ValueB()
{
    return RootValue() + 20;
}
)"));

		FFixtureFile FileC(TEXT("Tests/Preprocessor/WideGraph/C.as"), TEXT(R"(
import Tests.Preprocessor.WideGraph.Root;
int ValueC()
{
    return RootValue() + 30;
}
)"));

		FFixtureFile ConsumerFile(TEXT("Tests/Preprocessor/WideGraph/Consumer.as"), TEXT(R"(
import Tests.Preprocessor.WideGraph.A;
import Tests.Preprocessor.WideGraph.B;
import Tests.Preprocessor.WideGraph.C;
int Entry()
{
    return ValueA() + ValueB() + ValueC();
}
)"));

		// Add in reverse order to test topological sort
		TArray<FFixtureFile> Files;
		Files.Emplace(MoveTemp(ConsumerFile));
		Files.Emplace(MoveTemp(FileC));
		Files.Emplace(MoveTemp(FileB));
		Files.Emplace(MoveTemp(FileA));
		Files.Emplace(MoveTemp(RootFile));

		auto Result = RunPreprocess(Engine, Files);
		LogProcessedCode(Result, TEXT("WideImportGraph"));

		AssertPreprocessSucceeded(*TestRunner, Result);
		AssertModuleCount(*TestRunner, Result, 5);
		AssertErrorCount(*TestRunner, Result, 0);
		AssertNoDiagnostics(*TestRunner, Result);

		// Root should be first (no deps), Consumer should be last (depends on all)
		TestRunner->TestEqual(TEXT("First module should be Root"),
			Result.Modules[0]->ModuleName, FString(TEXT("Tests.Preprocessor.WideGraph.Root")));
		TestRunner->TestEqual(TEXT("Last module should be Consumer"),
			Result.Modules[4]->ModuleName, FString(TEXT("Tests.Preprocessor.WideGraph.Consumer")));

		// Consumer should import exactly A, B, C
		const FAngelscriptModuleDesc* Consumer = Result.FindModule(TEXT("Tests.Preprocessor.WideGraph.Consumer"));
		if (Consumer != nullptr)
		{
			AssertImportCount(*TestRunner, *Consumer, 3);
			AssertModuleImports(*TestRunner, *Consumer, TEXT("Tests.Preprocessor.WideGraph.A"));
			AssertModuleImports(*TestRunner, *Consumer, TEXT("Tests.Preprocessor.WideGraph.B"));
			AssertModuleImports(*TestRunner, *Consumer, TEXT("Tests.Preprocessor.WideGraph.C"));
		}

		}
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
