// ============================================================================
// AngelscriptPreprocessorLiteralTests.cpp
//
// Preprocessor tests for literal handling: name literals (n"..."), format
// strings (f"..."), prefixed literal token boundaries, and literal asset
// declarations (asset X of Type).
//
// Migrated from:
//   - AngelscriptPreprocessorLiteralTests.cpp (NameLiteralRoundTrip, PrefixedBoundary)
//   - AngelscriptPreprocessorLiteralAssetTests.cpp (GetterPostInit, StringCommentDecoys, MissingType, InsideFunctionBody)
//
// Automation prefix: Angelscript.TestModule.Preprocessor.Literals.*
// ============================================================================

#include "CQTest.h"
#include "Shared/AngelscriptTestMacros.h"
#include "Preprocessor/AngelscriptPreprocessorTestHelpers.h"

#include "Misc/ScopeExit.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace PreprocessorTestHelpers;
using namespace AngelscriptTestSupport;

// ============================================================================
// Test class
// ============================================================================

TEST_CLASS_WITH_FLAGS(FAngelscriptPreprocessorLiteralTest,
	"Angelscript.TestModule.Preprocessor.Literals",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	// ========================================================================
	// NameLiteralRoundTrip — n"Alpha" and n"Beta" are rewritten to
	// __STATIC_NAME references, duplicate names share indices, compiles & executes
	// ========================================================================
	TEST_METHOD(NameLiteralRoundTrip)
	{
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine); AngelscriptTestSupport::FScopedModuleCleanEngine _AutoModuleClean(Engine);

		static const FName ModuleName(TEXT("Tests.Preprocessor.Literals.NameLiteralRoundTrip"));
		ON_SCOPE_EXIT { Engine.DiscardModule(*ModuleName.ToString()); };

		const FString RelativeScriptPath = TEXT("Tests/Preprocessor/Literals/NameLiteralRoundTrip.as");
		const FString ScriptSource = TEXT(R"AS(
int Entry()
{
	FName A = n"Alpha";
	FName B = n"Alpha";
	FName C = n"Beta";
	return A == B && A != C ? 42 : 0;
}
)AS");

		FFixtureFile File(RelativeScriptPath, ScriptSource);
		auto Result = RunPreprocess(Engine, File);
		LogProcessedCode(Result, TEXT("NameLiteralRoundTrip"));

		AssertPreprocessSucceeded(*TestRunner, Result);
		AssertModuleCount(*TestRunner, Result, 1);
		AssertNoDiagnostics(*TestRunner, Result);

		const FAngelscriptModuleDesc* Module = AssertModuleExists(
			*TestRunner, Result, TEXT("Tests.Preprocessor.Literals.NameLiteralRoundTrip"));
		if (Module != nullptr)
		{
			const FString Code = Result.JoinedCode(*Module);
			TestRunner->TestFalse(TEXT("Should remove n\"Alpha\" text"), Code.Contains(TEXT("n\"Alpha\"")));
			TestRunner->TestFalse(TEXT("Should remove n\"Beta\" text"), Code.Contains(TEXT("n\"Beta\"")));
			TestRunner->TestTrue(TEXT("Should contain __STATIC_NAME references"), Code.Contains(TEXT("__STATIC_NAME(")));

			// Count __STATIC_NAME occurrences
			TArray<int32> Indices = ExtractStaticNameIndices(Code);
			TestRunner->TestEqual(TEXT("Should have 3 static name refs"), Indices.Num(), 3);
			if (Indices.Num() == 3)
			{
				TestRunner->TestEqual(TEXT("Duplicate Alpha refs share index"), Indices[0], Indices[1]);
				TestRunner->TestTrue(TEXT("Beta has different index"), Indices[2] != Indices[0]);
			}
		}

		// Compile and execute
		Engine.ResetDiagnostics();
		FAngelscriptCompileTraceSummary Summary;
		const bool bCompiled = CompileModuleWithSummary(
			&Engine, ECompileType::SoftReloadOnly, ModuleName,
			RelativeScriptPath, ScriptSource, true, Summary, true);

		TestRunner->TestTrue(TEXT("Should compile"), bCompiled);
		TestRunner->TestTrue(TEXT("Should use preprocessor"), Summary.bUsedPreprocessor);
		TestRunner->TestEqual(TEXT("No compile diagnostics"), Summary.Diagnostics.Num(), 0);

		int32 EntryResult = 0;
		const bool bExecuted = bCompiled
			&& ExecuteIntFunction(&Engine, RelativeScriptPath, ModuleName, TEXT("int Entry()"), EntryResult);
		TestRunner->TestTrue(TEXT("Should execute"), bExecuted);
		if (bExecuted)
		{
			TestRunner->TestEqual(TEXT("Name equality check → 42"), EntryResult, 42);
		}

		}
	}

	// ========================================================================
	// PrefixedLiteralsRequireTokenBoundary — "Actionn\"Tag\"" is NOT treated
	// as a name literal; "Valuef\"{123}\"" is NOT treated as a format string
	// ========================================================================
	TEST_METHOD(PrefixedLiteralsRequireTokenBoundary)
	{
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine); AngelscriptTestSupport::FScopedModuleCleanEngine _AutoModuleClean(Engine);

		struct FBoundaryCase
		{
			const TCHAR* Label;
			const TCHAR* RelativePath;
			const TCHAR* Source;
			const TCHAR* PreservedToken;
			const TCHAR* UnexpectedRewrite;
			int32 ErrorRow;
		};

		const TArray<FBoundaryCase> Cases = {
			{
				TEXT("Name literal boundary"),
				TEXT("Tests/Preprocessor/Literals/PrefixedNameBoundary.as"),
				TEXT("void Probe()\n{\n    Actionn\"Tag\";\n}\n"),
				TEXT("Actionn\"Tag\""),
				TEXT("__STATIC_NAME("),
				3
			},
			{
				TEXT("Format string boundary"),
				TEXT("Tests/Preprocessor/Literals/PrefixedFormatBoundary.as"),
				TEXT("void Probe()\n{\n    Valuef\"{123}\";\n}\n"),
				TEXT("Valuef\"{123}\""),
				TEXT("(FString()"),
				3
			}
		};

		for (const FBoundaryCase& Case : Cases)
		{
			FFixtureFile File(Case.RelativePath, Case.Source);
			auto Result = RunPreprocess(Engine, File);
			LogProcessedCode(Result, *FString::Printf(TEXT("PrefixedBoundary_%s"), Case.Label));

			AssertPreprocessSucceeded(*TestRunner, Result);
			AssertModuleCount(*TestRunner, Result, 1);

			const FAngelscriptModuleDesc* Module = Result.Modules.Num() > 0 ? &Result.Modules[0].Get() : nullptr;
			if (Module != nullptr)
			{
				const FString Code = Result.JoinedCode(*Module);
				TestRunner->TestTrue(
					FString::Printf(TEXT("%s: should preserve malformed token"), Case.Label),
					Code.Contains(Case.PreservedToken));
				TestRunner->TestFalse(
					FString::Printf(TEXT("%s: should NOT rewrite to helper"), Case.Label),
					Code.Contains(Case.UnexpectedRewrite));
			}

			// Verify it fails at compile time (not preprocess)
			Engine.ResetDiagnostics();
			FAngelscriptCompileTraceSummary Summary;
			FString FixtureModuleName = FPaths::ChangeExtension(FString(Case.RelativePath), TEXT(""))
				.Replace(TEXT("/"), TEXT("."));
			const bool bCompiled = CompileModuleWithSummary(
				&Engine, ECompileType::SoftReloadOnly, FName(*FixtureModuleName),
				Case.RelativePath, Case.Source, true, Summary, true);

			TestRunner->TestFalse(
				FString::Printf(TEXT("%s: should fail at compile"), Case.Label), bCompiled);
			TestRunner->TestTrue(
				FString::Printf(TEXT("%s: should have compile diagnostics"), Case.Label),
				Summary.Diagnostics.Num() > 0);

			Engine.DiscardModule(*FixtureModuleName);
		}

		}
	}

	// ========================================================================
	// LiteralAsset_GenerateGetterAndPostInitRegistration — "asset X of Type"
	// generates backing field, getter, and registers PostInitFunction
	// ========================================================================
	TEST_METHOD(LiteralAsset_GenerateGetterAndPostInitRegistration)
	{
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine); AngelscriptTestSupport::FScopedModuleCleanEngine _AutoModuleClean(Engine);

		FFixtureFile File(TEXT("Tests/Preprocessor/LiteralAssets/GenerateGetterAndPostInitRegistration.as"), TEXT(R"(
asset PreviewAsset of UObject
{
}

int Entry()
{
    return 7;
}
)"));

		auto Session = RunPreprocessSession(Engine, File);

		AssertPreprocessSucceeded(*TestRunner, Session.Result);
		AssertModuleCount(*TestRunner, Session.Result, 1);
		AssertNoDiagnostics(*TestRunner, Session.Result);

		const FAngelscriptModuleDesc* Module = AssertModuleExists(
			*TestRunner, Session.Result,
			TEXT("Tests.Preprocessor.LiteralAssets.GenerateGetterAndPostInitRegistration"));
		if (Module != nullptr)
		{
			const FString Code = Session.Result.JoinedCode(*Module);

			TestRunner->TestFalse(TEXT("Should strip original asset declaration"),
				Code.Contains(TEXT("asset PreviewAsset of UObject")));
			TestRunner->TestTrue(TEXT("Should keep Entry function"),
				Code.Contains(TEXT("int Entry()")));
			TestRunner->TestTrue(TEXT("Should generate backing field"),
				Code.Contains(TEXT("UObject __Asset_PreviewAsset;")));
			TestRunner->TestTrue(TEXT("Should generate explicit asset getter"),
				Code.Contains(TEXT("UObject GetPreviewAsset()")));
			TestRunner->TestTrue(TEXT("Should generate __CreateLiteralAsset call"),
				Code.Contains(TEXT("__CreateLiteralAsset(UObject, \"PreviewAsset\")")));
			TestRunner->TestTrue(TEXT("Should generate __PostLiteralAssetSetup call"),
				Code.Contains(TEXT("__PostLiteralAssetSetup(__Asset_PreviewAsset, \"PreviewAsset\");")));
			TestRunner->TestEqual(TEXT("Should register one PostInitFunction"),
				Module->PostInitFunctions.Num(), 1);
			if (Module->PostInitFunctions.Num() == 1)
			{
				TestRunner->TestEqual(TEXT("PostInitFunction should be GetPreviewAsset"),
					Module->PostInitFunctions[0], FString(TEXT("GetPreviewAsset")));
			}
		}

		}
	}

	// ========================================================================
	// LiteralAsset_SkipStringAndCommentDecoys — "asset X" inside strings or
	// comments is NOT expanded; only real top-level declarations are processed
	// ========================================================================
	TEST_METHOD(LiteralAsset_SkipStringAndCommentDecoys)
	{
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine); AngelscriptTestSupport::FScopedModuleCleanEngine _AutoModuleClean(Engine);

		FFixtureFile File(TEXT("Tests/Preprocessor/LiteralAssets/SkipStringAndCommentDecoys.as"), TEXT(R"(
asset RealAsset of UObject

FString BuildAssetText()
{
    return "asset FakeAsset of UObject";
}

// asset CommentAsset of UObject
int Entry()
{
    return BuildAssetText().Len();
}
)"));

		auto Session = RunPreprocessSession(Engine, File);

		AssertPreprocessSucceeded(*TestRunner, Session.Result);
		AssertNoDiagnostics(*TestRunner, Session.Result);

		const FAngelscriptModuleDesc* Module = AssertModuleExists(
			*TestRunner, Session.Result,
			TEXT("Tests.Preprocessor.LiteralAssets.SkipStringAndCommentDecoys"));
		if (Module != nullptr)
		{
			const FString Code = Session.Result.JoinedCode(*Module);

			// Real asset should expand
			TestRunner->TestTrue(TEXT("Should generate real asset field"),
				Code.Contains(TEXT("UObject __Asset_RealAsset;")));
			TestRunner->TestTrue(TEXT("Should generate real asset getter"),
				Code.Contains(TEXT("UObject GetRealAsset()")));

			// Fake/comment assets should NOT expand
			TestRunner->TestFalse(TEXT("Should not generate fake asset field"),
				Code.Contains(TEXT("__Asset_FakeAsset")));
			TestRunner->TestFalse(TEXT("Should not generate fake asset getter"),
				Code.Contains(TEXT("GetFakeAsset()")));
			TestRunner->TestFalse(TEXT("Should not generate comment asset field"),
				Code.Contains(TEXT("__Asset_CommentAsset")));
			TestRunner->TestFalse(TEXT("Should not generate comment asset getter"),
				Code.Contains(TEXT("GetCommentAsset()")));

			// String literal should be preserved
			TestRunner->TestTrue(TEXT("Should preserve string literal text"),
				Code.Contains(TEXT("\"asset FakeAsset of UObject\"")));

			// Only one PostInitFunction
			TestRunner->TestEqual(TEXT("Should register one PostInitFunction"),
				Module->PostInitFunctions.Num(), 1);
			if (Module->PostInitFunctions.Num() == 1)
			{
				TestRunner->TestEqual(TEXT("PostInitFunction should be GetRealAsset"),
					Module->PostInitFunctions[0], FString(TEXT("GetRealAsset")));
			}
		}

		}
	}

	// ========================================================================
	// LiteralAsset_MissingTypeFails — "asset X of" with no type fails
	// ========================================================================
	TEST_METHOD(LiteralAsset_MissingTypeFails)
	{
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine); AngelscriptTestSupport::FScopedModuleCleanEngine _AutoModuleClean(Engine);

		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(TEXT("ASLiteralAssetMissingType"));
			ASTEST_RESET_ENGINE(Engine);
		};

		TestRunner->AddExpectedError(TEXT(""), EAutomationExpectedErrorFlags::Contains, 0);

		ECompileResult CompileResult = ECompileResult::Error;
		const bool bCompiled = CompileModuleWithResult(
			&Engine,
			ECompileType::FullReload,
			FName(TEXT("ASLiteralAssetMissingType")),
			TEXT("ASLiteralAssetMissingType.as"),
			TEXT("UCLASS()\nclass UMissingTypeAssetOwner : UObject\n{\n\tasset BrokenAsset of\n}\n"),
			CompileResult);

		TestRunner->TestFalse(TEXT("asset with missing type should fail"), bCompiled);

		}
	}

	// ========================================================================
	// LiteralAsset_InsideFunctionBodyIgnored — "asset X of Type" inside a
	// function body is not expanded and compilation fails
	// ========================================================================
	TEST_METHOD(LiteralAsset_InsideFunctionBodyIgnored)
	{
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine); AngelscriptTestSupport::FScopedModuleCleanEngine _AutoModuleClean(Engine);

		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(TEXT("ASLiteralAssetInsideFunction"));
			ASTEST_RESET_ENGINE(Engine);
		};

		TestRunner->AddExpectedError(TEXT(""), EAutomationExpectedErrorFlags::Contains, 0);

		ECompileResult CompileResult = ECompileResult::Error;
		const bool bCompiled = CompileModuleWithResult(
			&Engine,
			ECompileType::FullReload,
			FName(TEXT("ASLiteralAssetInsideFunction")),
			TEXT("ASLiteralAssetInsideFunction.as"),
			TEXT("UCLASS()\nclass UFunctionBodyAssetOwner : UObject\n{\n\tUFUNCTION()\n\tvoid TryDeclareAsset()\n\t{\n\t\tasset LocalAsset of UObject\n\t}\n}\n"),
			CompileResult);

		TestRunner->TestFalse(TEXT("asset inside function body should fail"), bCompiled);

		}
	}

	// ========================================================================
	// FormatStringExpansion — f"Hello {Name}" is rewritten into string
	// concatenation; the original f"..." text is removed from output
	// ========================================================================
	TEST_METHOD(FormatStringExpansion)
	{
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine); AngelscriptTestSupport::FScopedModuleCleanEngine _AutoModuleClean(Engine);

		static const FName ModuleName(TEXT("Tests.Preprocessor.Literals.FormatStringExpansion"));
		ON_SCOPE_EXIT { Engine.DiscardModule(*ModuleName.ToString()); };

		const FString RelativeScriptPath = TEXT("Tests/Preprocessor/Literals/FormatStringExpansion.as");
		const FString ScriptSource = TEXT(R"AS(
FString BuildGreeting(FString Name)
{
	return f"Hello {Name}!";
}
int Entry()
{
	return BuildGreeting("World").Len();
}
)AS");

		FFixtureFile File(RelativeScriptPath, ScriptSource);
		auto Result = RunPreprocess(Engine, File);
		LogProcessedCode(Result, TEXT("FormatStringExpansion"));

		AssertPreprocessSucceeded(*TestRunner, Result);
		AssertModuleCount(*TestRunner, Result, 1);
		AssertNoDiagnostics(*TestRunner, Result);

		const FAngelscriptModuleDesc* Module = AssertModuleExists(
			*TestRunner, Result, TEXT("Tests.Preprocessor.Literals.FormatStringExpansion"));
		if (Module != nullptr)
		{
			const FString Code = Result.JoinedCode(*Module);
			// The f"..." syntax should be rewritten — original text removed
			TestRunner->TestFalse(TEXT("Should remove f\"...\" text"),
				Code.Contains(TEXT("f\"Hello {Name}!\"")));
			// Should contain string concatenation or FString construction
			TestRunner->TestTrue(TEXT("Should contain rewritten string code"),
				Code.Contains(TEXT("FString()")) || Code.Contains(TEXT("\"Hello \""))
				|| Code.Contains(TEXT("Name")) );
		}

		// Compile and execute
		Engine.ResetDiagnostics();
		FAngelscriptCompileTraceSummary Summary;
		const bool bCompiled = CompileModuleWithSummary(
			&Engine, ECompileType::SoftReloadOnly, ModuleName,
			RelativeScriptPath, ScriptSource, true, Summary, true);

		TestRunner->TestTrue(TEXT("Should compile"), bCompiled);
		TestRunner->TestTrue(TEXT("Should use preprocessor"), Summary.bUsedPreprocessor);

		int32 EntryResult = 0;
		const bool bExecuted = bCompiled
			&& ExecuteIntFunction(&Engine, RelativeScriptPath, ModuleName, TEXT("int Entry()"), EntryResult);
		TestRunner->TestTrue(TEXT("Should execute"), bExecuted);
		if (bExecuted)
		{
			// "Hello World!" = 12 chars
			TestRunner->TestEqual(TEXT("f-string should produce 'Hello World!' → Len 12"), EntryResult, 12);
		}

		}
	}

private:
	static TArray<int32> ExtractStaticNameIndices(const FString& ProcessedCode)
	{
		static const FString Marker(TEXT("__STATIC_NAME("));
		TArray<int32> Indices;
		int32 SearchFrom = 0;
		while (true)
		{
			const int32 MarkerIndex = ProcessedCode.Find(Marker, ESearchCase::CaseSensitive, ESearchDir::FromStart, SearchFrom);
			if (MarkerIndex == INDEX_NONE) break;
			const int32 NumberStart = MarkerIndex + Marker.Len();
			const int32 NumberEnd = ProcessedCode.Find(TEXT(")"), ESearchCase::CaseSensitive, ESearchDir::FromStart, NumberStart);
			if (NumberEnd == INDEX_NONE) break;
			Indices.Add(FCString::Atoi(*ProcessedCode.Mid(NumberStart, NumberEnd - NumberStart)));
			SearchFrom = NumberEnd + 1;
		}
		return Indices;
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
