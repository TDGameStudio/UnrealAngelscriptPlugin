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

#if WITH_ANGELSCRIPT_UNITTESTS

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
		using namespace PreprocessorTestHelpers;

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

		ASSERT_THAT(IsNotNull(ClassChunk, TEXT("Should still parse the class chunk before fail-closed cleanup")));
		ASSERT_THAT(IsNotNull(EntryChunk, TEXT("Should still keep the trailing Entry chunk available for inspection")));

		ASSERT_THAT(IsFalse(
			ClassChunk->Namespace.IsSet(),
			TEXT("Gameplay namespace should not leak into the class chunk")));

		ASSERT_THAT(IsFalse(
			EntryChunk->Namespace.IsSet(),
			TEXT("Gameplay namespace should not leak into the trailing global chunk")));

		if (Result.Modules.Num() == 1)
		{
			FAngelscriptModuleDesc& Module = Result.Modules[0].Get();
			ASSERT_THAT(AreEqual(0, Module.Code.Num(), TEXT("Should not emit any processed code sections")));
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
		using namespace PreprocessorTestHelpers;

		TUniquePtr<FAngelscriptEngine> OwnedEngine = NamespaceTestHelpers::CreateEditorEngine();
		if (!this->Assert.IsNotNull(
				OwnedEngine.Get(),
				TEXT("Should create an editor-configured engine")))
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

		ASSERT_THAT(IsTrue(
			FAngelscriptEngine::ShouldUseEditorScriptsForCurrentContext(),
			TEXT("Should run with EDITOR enabled")));

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
				ASSERT_THAT(IsFalse(
					Module.Code[0].Code.Contains(TEXT("#restrict")),
					TEXT("Should strip raw #restrict text from processed code")));
				ASSERT_THAT(IsFalse(
					Module.Code[0].Code.Contains(TEXT("Runtime.*")),
					TEXT("Should not leak the dead-branch pattern into processed code")));
			}

#if WITH_EDITOR
			ASSERT_THAT(AreEqual(
				0,
				Module.UsageRestrictions.Num(),
				TEXT("Should not record usage restriction metadata for inactive branch")));
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

		ASSERT_THAT(IsTrue(bCompiled, TEXT("Should compile through the preprocessor pipeline")));
		ASSERT_THAT(AreEqual(0, Summary.Diagnostics.Num(), TEXT("Should have no compile diagnostics")));

		int32 EntryResult = 0;
		const bool bExecuted = bCompiled
			&& ExecuteIntFunction(&Engine, File.RelativePath, ModuleName, TEXT("int Entry()"), EntryResult);
		ASSERT_THAT(IsTrue(bExecuted, TEXT("Should execute the compiled Entry function")));
		ASSERT_THAT(AreEqual(7, EntryResult, TEXT("Entry should return the active branch result")));

		}
	}

	// ========================================================================
	// RestrictUsageAllowPattern — #restrict usage allow in an active branch
	// records the usage restriction with the correct pattern
	// ========================================================================
	TEST_METHOD(RestrictUsageAllowPattern)
	{
		using namespace PreprocessorTestHelpers;

		TUniquePtr<FAngelscriptEngine> OwnedEngine = NamespaceTestHelpers::CreateEditorEngine();
		if (!this->Assert.IsNotNull(OwnedEngine.Get(), TEXT("Should create editor engine")))
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
		ASSERT_THAT(IsNotNull(Module, TEXT("Should find module")));
		AssertModuleCodeNotContains(*TestRunner, Result, *Module, TEXT("#restrict"));

#if WITH_EDITOR
		ASSERT_THAT(AreEqual(2, Module->UsageRestrictions.Num(), TEXT("Should record two usage restrictions")));
		ASSERT_THAT(IsTrue(Module->UsageRestrictions[0].bIsAllow, TEXT("First restriction should be allow")));
		ASSERT_THAT(AreEqual(FString(TEXT("Game.UI.*")), Module->UsageRestrictions[0].Pattern, TEXT("First pattern should be Game.UI.*")));
		ASSERT_THAT(IsFalse(Module->UsageRestrictions[1].bIsAllow, TEXT("Second restriction should be disallow")));
		ASSERT_THAT(AreEqual(FString(TEXT("Game.Internal.*")), Module->UsageRestrictions[1].Pattern, TEXT("Second pattern should be Game.Internal.*")));
#endif

		}
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
