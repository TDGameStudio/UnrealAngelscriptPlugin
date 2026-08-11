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
#include "AngelscriptTestMacros.h"
#include "Preprocessor/AngelscriptPreprocessorTestHelpers.h"

#include "Misc/ScopeExit.h"

#if WITH_ANGELSCRIPT_UNITTESTS

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
		using namespace PreprocessorTestHelpers;

		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine); FScopedModuleCleanEngine _AutoModuleClean(Engine);

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
			ASSERT_THAT(IsFalse(Code.Contains(TEXT("n\"Alpha\"")), TEXT("Should remove n\"Alpha\" text")));
			ASSERT_THAT(IsFalse(Code.Contains(TEXT("n\"Beta\"")), TEXT("Should remove n\"Beta\" text")));
			ASSERT_THAT(IsTrue(Code.Contains(TEXT("__STATIC_NAME(")), TEXT("Should contain __STATIC_NAME references")));
			ASSERT_THAT(IsTrue(
				Code.Contains(TEXT(", \"Alpha\")")),
				TEXT("Static-name lowering should carry canonical Alpha text")));
			ASSERT_THAT(IsTrue(
				Code.Contains(TEXT(", \"Beta\")")),
				TEXT("Static-name lowering should carry canonical Beta text")));

			// Count __STATIC_NAME occurrences
			TArray<int32> Indices = ExtractStaticNameIndices(Code);
			ASSERT_THAT(AreEqual(3, Indices.Num(), TEXT("Should have 3 static name refs")));
			if (Indices.Num() == 3)
			{
				ASSERT_THAT(AreEqual(Indices[1], Indices[0], TEXT("Duplicate Alpha refs share index")));
				ASSERT_THAT(IsTrue(Indices[2] != Indices[0], TEXT("Beta has different index")));
			}
		}

		// Compile and execute
		Engine.ResetDiagnostics();
		FAngelscriptCompileTraceSummary Summary;
		const bool bCompiled = CompileModuleWithSummary(
			&Engine, ECompileType::SoftReloadOnly, ModuleName,
			RelativeScriptPath, ScriptSource, true, Summary, true);

		ASSERT_THAT(IsTrue(bCompiled, TEXT("Should compile")));
		ASSERT_THAT(IsTrue(Summary.bUsedPreprocessor, TEXT("Should use preprocessor")));
		ASSERT_THAT(AreEqual(0, Summary.Diagnostics.Num(), TEXT("No compile diagnostics")));

		int32 EntryResult = 0;
		const bool bExecuted = bCompiled
			&& ExecuteIntFunction(&Engine, RelativeScriptPath, ModuleName, TEXT("int Entry()"), EntryResult);
		ASSERT_THAT(IsTrue(bExecuted, TEXT("Should execute")));
		if (bExecuted)
		{
			ASSERT_THAT(AreEqual(42, EntryResult, TEXT("Name equality check → 42")));
		}

		}
	}

	// ========================================================================
	// PrefixedLiteralsRequireTokenBoundary — "Actionn\"Tag\"" is NOT treated
	// as a name literal; "Valuef\"{123}\"" is NOT treated as a format string
	// ========================================================================
	TEST_METHOD(PrefixedLiteralsRequireTokenBoundary)
	{
		using namespace PreprocessorTestHelpers;

		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine); FScopedModuleCleanEngine _AutoModuleClean(Engine);

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
				ASSERT_THAT(IsTrue(
					Code.Contains(Case.PreservedToken),
					FString::Printf(TEXT("%s: should preserve malformed token"), Case.Label)));
				ASSERT_THAT(IsFalse(
					Code.Contains(Case.UnexpectedRewrite),
					FString::Printf(TEXT("%s: should NOT rewrite to helper"), Case.Label)));
			}

			// Verify it fails at compile time (not preprocess)
			Engine.ResetDiagnostics();
			FAngelscriptCompileTraceSummary Summary;
			FString FixtureModuleName = FPaths::ChangeExtension(FString(Case.RelativePath), TEXT(""))
				.Replace(TEXT("/"), TEXT("."));
			const bool bCompiled = CompileModuleWithSummary(
				&Engine, ECompileType::SoftReloadOnly, FName(*FixtureModuleName),
				Case.RelativePath, Case.Source, true, Summary, true);

			ASSERT_THAT(IsFalse(
				bCompiled,
				FString::Printf(TEXT("%s: should fail at compile"), Case.Label)));
			ASSERT_THAT(IsTrue(
				Summary.Diagnostics.Num() > 0,
				FString::Printf(TEXT("%s: should have compile diagnostics"), Case.Label)));

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
		using namespace PreprocessorTestHelpers;

		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine); FScopedModuleCleanEngine _AutoModuleClean(Engine);

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

			ASSERT_THAT(IsFalse(Code.Contains(TEXT("asset PreviewAsset of UObject")), TEXT("Should strip original asset declaration")));
			ASSERT_THAT(IsTrue(Code.Contains(TEXT("int Entry()")), TEXT("Should keep Entry function")));
			ASSERT_THAT(IsTrue(Code.Contains(TEXT("UObject __Asset_PreviewAsset;")), TEXT("Should generate backing field")));
			ASSERT_THAT(IsTrue(Code.Contains(TEXT("UObject GetPreviewAsset()")), TEXT("Should generate explicit asset getter")));
			ASSERT_THAT(IsTrue(Code.Contains(TEXT("__CreateLiteralAsset(UObject, \"PreviewAsset\")")), TEXT("Should generate __CreateLiteralAsset call")));
			ASSERT_THAT(IsTrue(Code.Contains(TEXT("__PostLiteralAssetSetup(__Asset_PreviewAsset, \"PreviewAsset\");")), TEXT("Should generate __PostLiteralAssetSetup call")));
			ASSERT_THAT(AreEqual(1, Module->PostInitFunctions.Num(), TEXT("Should register one PostInitFunction")));
			if (Module->PostInitFunctions.Num() == 1)
			{
				ASSERT_THAT(AreEqual(FString(TEXT("GetPreviewAsset")), Module->PostInitFunctions[0], TEXT("PostInitFunction should be GetPreviewAsset")));
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
		using namespace PreprocessorTestHelpers;

		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine); FScopedModuleCleanEngine _AutoModuleClean(Engine);

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
			ASSERT_THAT(IsTrue(Code.Contains(TEXT("UObject __Asset_RealAsset;")), TEXT("Should generate real asset field")));
			ASSERT_THAT(IsTrue(Code.Contains(TEXT("UObject GetRealAsset()")), TEXT("Should generate real asset getter")));

			// Fake/comment assets should NOT expand
			ASSERT_THAT(IsFalse(Code.Contains(TEXT("__Asset_FakeAsset")), TEXT("Should not generate fake asset field")));
			ASSERT_THAT(IsFalse(Code.Contains(TEXT("GetFakeAsset()")), TEXT("Should not generate fake asset getter")));
			ASSERT_THAT(IsFalse(Code.Contains(TEXT("__Asset_CommentAsset")), TEXT("Should not generate comment asset field")));
			ASSERT_THAT(IsFalse(Code.Contains(TEXT("GetCommentAsset()")), TEXT("Should not generate comment asset getter")));

			// String literal should be preserved
			ASSERT_THAT(IsTrue(Code.Contains(TEXT("\"asset FakeAsset of UObject\"")), TEXT("Should preserve string literal text")));

			// Only one PostInitFunction
			ASSERT_THAT(AreEqual(1, Module->PostInitFunctions.Num(), TEXT("Should register one PostInitFunction")));
			if (Module->PostInitFunctions.Num() == 1)
			{
				ASSERT_THAT(AreEqual(FString(TEXT("GetRealAsset")), Module->PostInitFunctions[0], TEXT("PostInitFunction should be GetRealAsset")));
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
		{ FAngelscriptEngineScope _AutoEngineScope(Engine); FScopedModuleCleanEngine _AutoModuleClean(Engine);

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

		ASSERT_THAT(IsFalse(bCompiled, TEXT("asset with missing type should fail")));

		}
	}

	// ========================================================================
	// LiteralAsset_InsideFunctionBodyIgnored — "asset X of Type" inside a
	// function body is not expanded and compilation fails
	// ========================================================================
	TEST_METHOD(LiteralAsset_InsideFunctionBodyIgnored)
	{
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine); FScopedModuleCleanEngine _AutoModuleClean(Engine);

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

		ASSERT_THAT(IsFalse(bCompiled, TEXT("asset inside function body should fail")));

		}
	}

	// ========================================================================
	// FormatStringExpansion — f"Hello {Name}" is rewritten into string
	// concatenation; the original f"..." text is removed from output
	// ========================================================================
	TEST_METHOD(FormatStringExpansion)
	{
		using namespace PreprocessorTestHelpers;

		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine); FScopedModuleCleanEngine _AutoModuleClean(Engine);

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
			ASSERT_THAT(IsFalse(Code.Contains(TEXT("f\"Hello {Name}!\"")), TEXT("Should remove f\"...\" text")));
			// Should contain string concatenation or FString construction
			ASSERT_THAT(IsTrue(
				Code.Contains(TEXT("FString()")) || Code.Contains(TEXT("\"Hello \""))
				|| Code.Contains(TEXT("Name")),
				TEXT("Should contain rewritten string code")));
		}

		// Compile and execute
		Engine.ResetDiagnostics();
		FAngelscriptCompileTraceSummary Summary;
		const bool bCompiled = CompileModuleWithSummary(
			&Engine, ECompileType::SoftReloadOnly, ModuleName,
			RelativeScriptPath, ScriptSource, true, Summary, true);

		ASSERT_THAT(IsTrue(bCompiled, TEXT("Should compile")));
		ASSERT_THAT(IsTrue(Summary.bUsedPreprocessor, TEXT("Should use preprocessor")));

		int32 EntryResult = 0;
		const bool bExecuted = bCompiled
			&& ExecuteIntFunction(&Engine, RelativeScriptPath, ModuleName, TEXT("int Entry()"), EntryResult);
		ASSERT_THAT(IsTrue(bExecuted, TEXT("Should execute")));
		if (bExecuted)
		{
			// "Hello World!" = 12 chars
			ASSERT_THAT(AreEqual(12, EntryResult, TEXT("f-string should produce 'Hello World!' → Len 12")));
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

#endif // WITH_ANGELSCRIPT_UNITTESTS
