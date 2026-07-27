#include "../Support/AngelscriptNativeCoreTestSupport.h"
#include "AngelscriptTestMacros.h"
#include "CQTest.h"
#include "Misc/ScopeExit.h"

#if WITH_ANGELSCRIPT_UNITTESTS


TEST_CLASS_WITH_FLAGS(FSemanticRejectionTests,
	"Angelscript.TestModule.AngelScriptSDK.Language.SemanticRejection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(InvalidConstObjectAssignmentReportsTypeMismatch)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_NON_PRODUCT(
			"LegacyCompatibility",
			"LANG-VAR-FAILURE-BOUNDARY and LANG-CONV-FAILURE supersede this incompatible initializer predecessor with fresh/same-module recovery, exact diagnostics, and cleanup");

		AngelscriptNativeTestSupport::FNativeMessageCollector Messages;
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			class Foo
			{
			}

			void Main()
			{
				const Foo F = 10;
			}
			)AS");
		const int CompileResult = CompileSnippet("ReferenceCompilerConstObjectAssignment", ScriptSource.c_str(),
			Messages);

		ASSERT_THAT(IsTrue(CompileResult < 0, TEXT("Reference invalid const object assignment should fail to build")));
		ASSERT_THAT(IsTrue(ContainsError(Messages, TEXT("Foo")) || ContainsError(Messages, TEXT("convert")),
			TEXT("Reference invalid const object assignment should report the target type or conversion")));
	}

	TEST_METHOD(OutOfScopeLocalReferenceReportsIdentifier)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_NON_PRODUCT(
			"LegacyCompatibility",
			"LANG-VAR-FAILURE-BOUNDARY and LANG-VAR-SHADOW supersede this use-after-scope predecessor across recovery modes, scope relations, use paths, diagnostics, and cleanup");

		AngelscriptNativeTestSupport::FNativeMessageCollector Messages;
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			int Entry()
			{
				{
					int Inner = 2;
				}

				return Inner;
			}
			)AS");
		const int CompileResult = CompileSnippet("ReferenceCompilerOutOfScopeLocal", ScriptSource.c_str(),
			Messages);

		ASSERT_THAT(IsTrue(CompileResult < 0, TEXT("Reference out-of-scope local should fail to build")));
		ASSERT_THAT(IsTrue(ContainsError(Messages, TEXT("Inner")) || ContainsError(Messages, TEXT("not declared")),
			TEXT("Reference out-of-scope local should keep the missing identifier name")));
	}

	TEST_METHOD(UnknownFunctionCallReportsMissingSymbol)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_NON_PRODUCT(
			"LegacyCompatibility",
			"LANG-EXPR-RESOLUTION and LANG-EXPR-FAILURE supersede this missing-call predecessor across callable states, expression contexts, recovery, exact diagnostics, and cleanup");

		AngelscriptNativeTestSupport::FNativeMessageCollector Messages;
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			void Entry()
			{
				MissingCall(1, 2);
			}
			)AS");
		const int CompileResult = CompileSnippet("ReferenceCompilerUnknownCall", ScriptSource.c_str(),
			Messages);

		ASSERT_THAT(IsTrue(CompileResult < 0, TEXT("Reference unknown function call should fail to build")));
		ASSERT_THAT(IsTrue(ContainsError(Messages, TEXT("MissingCall")) || ContainsError(Messages, TEXT("No matching symbol")),
			TEXT("Reference unknown function call should keep the missing symbol name")));
	}

	TEST_METHOD(ReturnObjectFromIntFunctionIsRejected)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_NON_PRODUCT(
			"LegacyCompatibility",
			"LANG-FN-RETURN and LANG-CONV-FAILURE supersede this incompatible return predecessor across return types, control paths, conversion outcomes, recovery, and cleanup");

		AngelscriptNativeTestSupport::FNativeMessageCollector Messages;
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			class Foo
			{
			}

			int Entry()
			{
				Foo F;
				return F;
			}
			)AS");
		const int CompileResult = CompileSnippet("ReferenceCompilerReturnObjectAsInt", ScriptSource.c_str(),
			Messages);

		ASSERT_THAT(IsTrue(CompileResult < 0, TEXT("Reference object returned from int function should fail to build")));
		ASSERT_THAT(IsTrue(ContainsError(Messages, TEXT("Foo")) || ContainsError(Messages, TEXT("int")) || ContainsError(Messages, TEXT("convert")),
			TEXT("Reference object returned from int function should report type mismatch context")));
	}

	TEST_METHOD(LongIdentifierAssignmentReportsDiagnosticWithoutCrash)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_NON_PRODUCT(
			"LegacyCompatibility",
			"LANG-VAR-FAILURE-BOUNDARY supersedes this long-identifier predecessor with both recovery modes, exact generated source, diagnostic preservation, module isolation, and cleanup");

		const FString LongIdentifier = FString::ChrN(400, TEXT('a'));
		const FString SourceFormat = ASTEST_AS(R"AS(
			void Entry()
			{
				%s = 1;
			}
			)AS");
		FString Source = SourceFormat;
		ASSERT_THAT(AreEqual(1, Source.ReplaceInline(TEXT("%s"), *LongIdentifier, ESearchCase::CaseSensitive),
			TEXT("Reference long-token source should replace its single identifier placeholder")));
		const FTCHARToUTF8 SourceUtf8(*Source);

		AngelscriptNativeTestSupport::FNativeMessageCollector Messages;
		const int CompileResult = CompileSnippet("ReferenceCompilerLongIdentifier", SourceUtf8.Get(), Messages);

		ASSERT_THAT(IsTrue(CompileResult < 0, TEXT("Reference long-token assignment should fail to build without crashing")));
		ASSERT_THAT(IsTrue(ContainsError(Messages, TEXT("No matching symbol")) || ContainsError(Messages, TEXT("not declared")) || ContainsError(Messages, *LongIdentifier.Left(32)),
			TEXT("Reference long-token assignment should produce a useful missing-symbol diagnostic")));
	}
};

#endif
