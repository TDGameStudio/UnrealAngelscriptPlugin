#include "../Support/AngelscriptNativeCoreTestSupport.h"
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

		AngelscriptNativeTestSupport::FNativeMessageCollector Messages;
		const int CompileResult = CompileSnippet("ReferenceCompilerConstObjectAssignment", R"(
class Foo
{
}

void Main()
{
	const Foo F = 10;
}
)",
			Messages);

		ASSERT_THAT(IsTrue(CompileResult < 0, TEXT("Reference invalid const object assignment should fail to build")));
		ASSERT_THAT(IsTrue(ContainsError(Messages, TEXT("Foo")) || ContainsError(Messages, TEXT("convert")),
			TEXT("Reference invalid const object assignment should report the target type or conversion")));
	}

	TEST_METHOD(OutOfScopeLocalReferenceReportsIdentifier)
	{
		using namespace AngelscriptNativeTestSupport;

		AngelscriptNativeTestSupport::FNativeMessageCollector Messages;
		const int CompileResult = CompileSnippet("ReferenceCompilerOutOfScopeLocal", R"(
int Entry()
{
	{
		int Inner = 2;
	}
	return Inner;
}
)",
			Messages);

		ASSERT_THAT(IsTrue(CompileResult < 0, TEXT("Reference out-of-scope local should fail to build")));
		ASSERT_THAT(IsTrue(ContainsError(Messages, TEXT("Inner")) || ContainsError(Messages, TEXT("not declared")),
			TEXT("Reference out-of-scope local should keep the missing identifier name")));
	}

	TEST_METHOD(UnknownFunctionCallReportsMissingSymbol)
	{
		using namespace AngelscriptNativeTestSupport;

		AngelscriptNativeTestSupport::FNativeMessageCollector Messages;
		const int CompileResult = CompileSnippet("ReferenceCompilerUnknownCall", R"(
void Entry()
{
	MissingCall(1, 2);
}
)",
			Messages);

		ASSERT_THAT(IsTrue(CompileResult < 0, TEXT("Reference unknown function call should fail to build")));
		ASSERT_THAT(IsTrue(ContainsError(Messages, TEXT("MissingCall")) || ContainsError(Messages, TEXT("No matching symbol")),
			TEXT("Reference unknown function call should keep the missing symbol name")));
	}

	TEST_METHOD(ReturnObjectFromIntFunctionIsRejected)
	{
		using namespace AngelscriptNativeTestSupport;

		AngelscriptNativeTestSupport::FNativeMessageCollector Messages;
		const int CompileResult = CompileSnippet("ReferenceCompilerReturnObjectAsInt", R"(
class Foo
{
}

int Entry()
{
	Foo F;
	return F;
}
)",
			Messages);

		ASSERT_THAT(IsTrue(CompileResult < 0, TEXT("Reference object returned from int function should fail to build")));
		ASSERT_THAT(IsTrue(ContainsError(Messages, TEXT("Foo")) || ContainsError(Messages, TEXT("int")) || ContainsError(Messages, TEXT("convert")),
			TEXT("Reference object returned from int function should report type mismatch context")));
	}

	TEST_METHOD(LongIdentifierAssignmentReportsDiagnosticWithoutCrash)
	{
		using namespace AngelscriptNativeTestSupport;

		const FString LongIdentifier = FString::ChrN(400, TEXT('a'));
		const FString Source = FString::Printf(TEXT(R"(
void Entry()
{
	%s = 1;
}
)"),
			*LongIdentifier);
		const FTCHARToUTF8 SourceUtf8(*Source);

		AngelscriptNativeTestSupport::FNativeMessageCollector Messages;
		const int CompileResult = CompileSnippet("ReferenceCompilerLongIdentifier", SourceUtf8.Get(), Messages);

		ASSERT_THAT(IsTrue(CompileResult < 0, TEXT("Reference long-token assignment should fail to build without crashing")));
		ASSERT_THAT(IsTrue(ContainsError(Messages, TEXT("No matching symbol")) || ContainsError(Messages, TEXT("not declared")) || ContainsError(Messages, *LongIdentifier.Left(32)),
			TEXT("Reference long-token assignment should produce a useful missing-symbol diagnostic")));
	}
};

#endif
