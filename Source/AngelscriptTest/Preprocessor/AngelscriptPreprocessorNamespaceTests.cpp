// ============================================================================
// AngelscriptPreprocessorNamespaceTests.cpp
//
// Preprocessor tests for namespace handling and restrict-usage directives.
//
// Migrated from:
//   - AngelscriptPreprocessorNamespaceTests.cpp (InvalidDeclarationReportsSyntax)
//   - AngelscriptPreprocessorRestrictUsageTests.cpp (InactiveBranchIgnored)
//
// Automation prefix: Angelscript.TestModule.Preprocessor.Namespace.*
// ============================================================================

#include "CQTest.h"
#include "Preprocessor/AngelscriptPreprocessorTestHelpers.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace PreprocessorTestHelpers;

// ============================================================================
// Helpers
// ============================================================================

namespace NamespaceTestHelpers
{
	static TUniquePtr<FAngelscriptEngine> CreateEditorEngine()
	{
		FAngelscriptEngineConfig Config;
		Config.bIsEditor = true;
		FAngelscriptEngineDependencies Dependencies = FAngelscriptEngineDependencies::CreateDefault();
		return CreateScriptScanFreeFullEngineForTesting(Config, Dependencies);
	}
}

// ============================================================================
// Test class
// ============================================================================

TEST_CLASS_WITH_FLAGS(FAngelscriptPreprocessorNamespaceTest,
	"Angelscript.TestModule.Preprocessor.Namespace",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	// ========================================================================
	// InvalidDeclarationReportsSyntax — missing '{' after namespace name
	// ========================================================================
	TEST_METHOD(InvalidDeclarationReportsSyntax)
	{
		static const TCHAR* InvalidNamespaceMessage =
			TEXT("Invalid namespace declaration, expected '{' after namespace name.");

		TestRunner->AddExpectedErrorPlain(InvalidNamespaceMessage, EAutomationExpectedErrorFlags::Contains, 1);

		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine); FScopedModuleCleanEngine _AutoModuleClean(Engine);

		FFixtureFile File(TEXT("Tests/Preprocessor/Namespace/InvalidDeclarationReportsSyntax.as"), TEXT(R"(
namespace Gameplay
UCLASS()
class UBrokenNamespaceCarrier : UObject
{
}

int Entry()
{
    return 7;
}
)"));

		auto Session = RunPreprocessSession(Engine, File);
		const auto& Result = Session.Result;

		AssertPreprocessFailed(*TestRunner, Result);
		AssertModuleCount(*TestRunner, Result, 1);
		AssertErrorCount(*TestRunner, Result, 1);
		AssertDiagnosticContains(*TestRunner, Result, FString(InvalidNamespaceMessage));
		AssertDiagnosticAt(*TestRunner, Result, FString(InvalidNamespaceMessage), /*Row=*/1, /*Column=*/1);

		// Verify chunk-level state: class and trailing code chunks should exist
		// but neither should have a namespace set (error prevents namespace propagation).
		const FAngelscriptPreprocessor::FChunk* ClassChunk =
			Session.FindFirstChunkOfType(FAngelscriptPreprocessor::EChunkType::Class);
		const FAngelscriptPreprocessor::FChunk* EntryChunk = nullptr;
		for (const FAngelscriptPreprocessor::FFile& PPFile : Session.GetFiles())
		{
			for (const FAngelscriptPreprocessor::FChunk& Chunk : PPFile.ChunkedCode)
			{
				if (Chunk.Content.Contains(TEXT("int Entry()")))
				{
					EntryChunk = &Chunk;
					break;
				}
			}
			if (EntryChunk != nullptr)
			{
				break;
			}
		}

		TestRunner->TestNotNull(
			TEXT("Should still parse the class chunk before fail-closed cleanup"),
			ClassChunk);
		TestRunner->TestNotNull(
			TEXT("Should still keep the trailing Entry chunk available for inspection"),
			EntryChunk);

		if (ClassChunk != nullptr)
		{
			TestRunner->TestFalse(
				TEXT("Gameplay namespace should not leak into the class chunk"),
				ClassChunk->Namespace.IsSet());
		}

		if (EntryChunk != nullptr)
		{
			TestRunner->TestFalse(
				TEXT("Gameplay namespace should not leak into the trailing global chunk"),
				EntryChunk->Namespace.IsSet());
		}

		if (Result.Modules.Num() == 1)
		{
			FAngelscriptModuleDesc& Module = Result.Modules[0].Get();
			TestRunner->TestEqual(
				TEXT("Should not emit any processed code sections"),
				Module.Code.Num(), 0);
			AssertModuleNotDeclaresClass(*TestRunner, Module, TEXT("UBrokenNamespaceCarrier"));
		}

		AssertNoCompilableCode(*TestRunner, Result);

		}
	}

	// ========================================================================
	// RestrictUsageInactiveBranchIgnored — #restrict usage inside #if !EDITOR
	// is skipped when running in editor context
	// ========================================================================
	TEST_METHOD(RestrictUsageInactiveBranchIgnored)
	{
		TUniquePtr<FAngelscriptEngine> OwnedEngine = NamespaceTestHelpers::CreateEditorEngine();
		if (!TestRunner->TestNotNull(
				TEXT("Should create an editor-configured engine"), OwnedEngine.Get()))
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
			TEXT("Should run with EDITOR enabled"),
			FAngelscriptEngine::ShouldUseEditorScriptsForCurrentContext());

		FFixtureFile File(
			TEXT("Game/Preprocessor/RestrictUsage/InactiveBranchIgnored.as"), TEXT(R"(
#if !EDITOR
#restrict usage disallow Runtime.*
#endif
int Entry()
{
    return 7;
}
)"));

		auto Result = RunPreprocess(Engine, File);
		LogProcessedCode(Result, TEXT("RestrictUsageInactiveBranch"));

		AssertPreprocessSucceeded(*TestRunner, Result);
		AssertNoDiagnostics(*TestRunner, Result);
		AssertModuleCount(*TestRunner, Result, 1);

		if (Result.Modules.Num() > 0)
		{
			const FAngelscriptModuleDesc& Module = Result.Modules[0].Get();

			if (Module.Code.Num() > 0)
			{
				TestRunner->TestFalse(
					TEXT("Should strip raw #restrict text from processed code"),
					Module.Code[0].Code.Contains(TEXT("#restrict")));
				TestRunner->TestFalse(
					TEXT("Should not leak the dead-branch pattern into processed code"),
					Module.Code[0].Code.Contains(TEXT("Runtime.*")));
			}

#if WITH_EDITOR
			TestRunner->TestEqual(
				TEXT("Should not record usage restriction metadata for inactive branch"),
				Module.UsageRestrictions.Num(), 0);
#endif
		}

		// Compile and execute to confirm the active branch works end-to-end
		static const FName ModuleName(TEXT("Game.Preprocessor.RestrictUsage.InactiveBranchIgnored"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		Engine.ResetDiagnostics();

		const FString ScriptSource = TEXT(R"(
#if !EDITOR
#restrict usage disallow Runtime.*
#endif
int Entry()
{
    return 7;
}
)");

		FAngelscriptCompileTraceSummary Summary;
		const bool bCompiled = CompileModuleWithSummary(
			&Engine,
			ECompileType::SoftReloadOnly,
			ModuleName,
			File.RelativePath,
			ScriptSource,
			true,
			Summary,
			true);

		TestRunner->TestTrue(
			TEXT("Should compile through the preprocessor pipeline"), bCompiled);
		TestRunner->TestEqual(
			TEXT("Should have no compile diagnostics"),
			Summary.Diagnostics.Num(), 0);

		int32 EntryResult = 0;
		const bool bExecuted = bCompiled
			&& ExecuteIntFunction(&Engine, File.RelativePath, ModuleName, TEXT("int Entry()"), EntryResult);
		TestRunner->TestTrue(
			TEXT("Should execute the compiled Entry function"), bExecuted);
		if (bExecuted)
		{
			TestRunner->TestEqual(
				TEXT("Entry should return the active branch result"),
				EntryResult, 7);
		}

		}
	}

	// ========================================================================
	// RestrictUsageAllowPattern — #restrict usage allow in an active branch
	// records the usage restriction with the correct pattern
	// ========================================================================
	TEST_METHOD(RestrictUsageAllowPattern)
	{
		TUniquePtr<FAngelscriptEngine> OwnedEngine = NamespaceTestHelpers::CreateEditorEngine();
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

		FFixtureFile File(TEXT("Game/Preprocessor/Namespace/RestrictAllow.as"), TEXT(R"(
#restrict usage allow Game.UI.*
#restrict usage disallow Game.Internal.*
int Entry()
{
    return 42;
}
)"));

		auto Result = RunPreprocess(Engine, File);
		LogProcessedCode(Result, TEXT("RestrictUsageAllow"));

		AssertPreprocessSucceeded(*TestRunner, Result);
		AssertModuleCount(*TestRunner, Result, 1);
		AssertNoDiagnostics(*TestRunner, Result);

		FAngelscriptModuleDesc* Module = Result.FindModule(
			TEXT("Game.Preprocessor.Namespace.RestrictAllow"));
		if (TestRunner->TestNotNull(TEXT("Should find module"), Module))
		{
			AssertModuleCodeNotContains(*TestRunner, Result, *Module, TEXT("#restrict"));

#if WITH_EDITOR
			TestRunner->TestEqual(TEXT("Should record two usage restrictions"),
				Module->UsageRestrictions.Num(), 2);
			if (Module->UsageRestrictions.Num() >= 2)
			{
				TestRunner->TestTrue(TEXT("First restriction should be allow"),
					Module->UsageRestrictions[0].bIsAllow);
				TestRunner->TestEqual(TEXT("First pattern should be Game.UI.*"),
					Module->UsageRestrictions[0].Pattern, FString(TEXT("Game.UI.*")));

				TestRunner->TestFalse(TEXT("Second restriction should be disallow"),
					Module->UsageRestrictions[1].bIsAllow);
				TestRunner->TestEqual(TEXT("Second pattern should be Game.Internal.*"),
					Module->UsageRestrictions[1].Pattern, FString(TEXT("Game.Internal.*")));
			}
#endif
		}

		}
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
