#include "../Support/AngelscriptNativeCoreTestSupport.h"
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

public:
	TEST_METHOD(UnfinishedClassReportsMissingBrace)
	{
		using namespace AngelscriptNativeTestSupport;

		AngelscriptNativeTestSupport::FNativeMessageCollector Messages;
		const int CompileResult = CompileSnippet("ReferenceParserUnfinishedClass", R"(
class myclass
{
)",
			Messages);

		ASSERT_THAT(IsTrue(CompileResult < 0, TEXT("Reference unfinished class should fail to build")));
		ASSERT_THAT(IsTrue(ContainsError(Messages, TEXT("Expected '}'")) || ContainsError(Messages, TEXT("<end of file>")),
			TEXT("Reference unfinished class should report a missing class body terminator")));
	}

	TEST_METHOD(CapitalConstInParameterIsRejected)
	{
		using namespace AngelscriptNativeTestSupport;

		AngelscriptNativeTestSupport::FNativeMessageCollector Messages;
		const int CompileResult = CompileSnippet("ReferenceParserCapitalConst", R"(
class myclass
{
	void f(Const int&in) {}
};
)",
			Messages);

		ASSERT_THAT(IsTrue(CompileResult < 0, TEXT("Reference capital Const parameter should fail to build")));
		ASSERT_THAT(IsTrue(CountErrors(Messages) > 0, TEXT("Reference capital Const parameter should emit at least one syntax error")));
		ASSERT_THAT(IsTrue(ContainsError(Messages, TEXT("Const")) || ContainsError(Messages, TEXT("int")) || ContainsError(Messages, TEXT("Expected")),
			TEXT("Reference capital Const parameter should keep useful diagnostic context")));
	}

	TEST_METHOD(UnclosedNamespaceReportsEndOfFile)
	{
		using namespace AngelscriptNativeTestSupport;

		AngelscriptNativeTestSupport::FNativeMessageCollector Messages;
		const int CompileResult = CompileSnippet("ReferenceParserUnclosedNamespace", R"(
namespace Outer
{
	class Inner
	{
)",
			Messages);

		ASSERT_THAT(IsTrue(CompileResult < 0, TEXT("Reference unclosed namespace should fail to build")));
		ASSERT_THAT(IsTrue(CountErrors(Messages) >= 1, TEXT("Reference unclosed namespace should report parser errors")));
		ASSERT_THAT(IsTrue(ContainsError(Messages, TEXT("Expected")) || ContainsError(Messages, TEXT("<end of file>")),
			TEXT("Reference unclosed namespace should mention expected structure or EOF")));
	}

	TEST_METHOD(BadParameterListAccumulatesSyntaxError)
	{
		using namespace AngelscriptNativeTestSupport;

		AngelscriptNativeTestSupport::FNativeMessageCollector Messages;
		const int CompileResult = CompileSnippet("ReferenceParserBadParameters", R"(
void Bad(int A,, int B)
{
}
)",
			Messages);

		ASSERT_THAT(IsTrue(CompileResult < 0, TEXT("Reference bad parameter list should fail to build")));
		ASSERT_THAT(IsTrue(ContainsError(Messages, TEXT("Expected")) || ContainsError(Messages, TEXT("Instead found")),
			TEXT("Reference bad parameter list should produce a syntax diagnostic")));
	}

	TEST_METHOD(MultipleMalformedDeclarationsReportMultipleErrors)
	{
		using namespace AngelscriptNativeTestSupport;

		AngelscriptNativeTestSupport::FNativeMessageCollector Messages;
		const int CompileResult = CompileSnippet("ReferenceParserMultipleMalformed", R"(
void Bad( { }
class Broken { int ; }
int Read() { return ; }
)",
			Messages);

		ASSERT_THAT(IsTrue(CompileResult < 0, TEXT("Reference malformed declarations should fail to build")));
		ASSERT_THAT(IsTrue(CountErrors(Messages) >= 2,
			TEXT("Reference malformed declarations should accumulate more than one error")));
	}
};

#endif
