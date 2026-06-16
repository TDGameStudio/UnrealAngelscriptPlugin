#include "AngelscriptNativeTestSupport.h"
#include "CQTest.h"
#include "Misc/ScopeExit.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptNativeTestSupport;

namespace
{
	int32 CountErrors(const AngelscriptNativeTestSupport::FNativeMessageCollector& Messages)
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
}

TEST_CLASS_WITH_FLAGS(FAngelscriptNativeReferenceParserErrorTests,
	"Angelscript.TestModule.AngelScriptSDK.Reference.ParserErrors",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(UnfinishedClassReportsMissingBrace)
	{
		AngelscriptNativeTestSupport::FNativeMessageCollector Messages;
		const int CompileResult = CompileSnippet("ReferenceParserUnfinishedClass", R"(
class myclass
{
)",
			Messages);

		TestRunner->TestTrue(TEXT("Reference unfinished class should fail to build"), CompileResult < 0);
		TestRunner->TestTrue(TEXT("Reference unfinished class should report a missing class body terminator"), ContainsError(Messages, TEXT("Expected '}'")) || ContainsError(Messages, TEXT("<end of file>")));
	}

	TEST_METHOD(CapitalConstInParameterIsRejected)
	{
		AngelscriptNativeTestSupport::FNativeMessageCollector Messages;
		const int CompileResult = CompileSnippet("ReferenceParserCapitalConst", R"(
class myclass
{
	void f(Const int&in) {}
};
)",
			Messages);

		TestRunner->TestTrue(TEXT("Reference capital Const parameter should fail to build"), CompileResult < 0);
		TestRunner->TestTrue(TEXT("Reference capital Const parameter should emit at least one syntax error"), CountErrors(Messages) > 0);
		TestRunner->TestTrue(TEXT("Reference capital Const parameter should keep useful diagnostic context"), ContainsError(Messages, TEXT("Const")) || ContainsError(Messages, TEXT("int")) || ContainsError(Messages, TEXT("Expected")));
	}

	TEST_METHOD(UnclosedNamespaceReportsEndOfFile)
	{
		AngelscriptNativeTestSupport::FNativeMessageCollector Messages;
		const int CompileResult = CompileSnippet("ReferenceParserUnclosedNamespace", R"(
namespace Outer
{
	class Inner
	{
)",
			Messages);

		TestRunner->TestTrue(TEXT("Reference unclosed namespace should fail to build"), CompileResult < 0);
		TestRunner->TestTrue(TEXT("Reference unclosed namespace should report parser errors"), CountErrors(Messages) >= 1);
		TestRunner->TestTrue(TEXT("Reference unclosed namespace should mention expected structure or EOF"), ContainsError(Messages, TEXT("Expected")) || ContainsError(Messages, TEXT("<end of file>")));
	}

	TEST_METHOD(BadParameterListAccumulatesSyntaxError)
	{
		AngelscriptNativeTestSupport::FNativeMessageCollector Messages;
		const int CompileResult = CompileSnippet("ReferenceParserBadParameters", R"(
void Bad(int A,, int B)
{
}
)",
			Messages);

		TestRunner->TestTrue(TEXT("Reference bad parameter list should fail to build"), CompileResult < 0);
		TestRunner->TestTrue(TEXT("Reference bad parameter list should produce a syntax diagnostic"), ContainsError(Messages, TEXT("Expected")) || ContainsError(Messages, TEXT("Instead found")));
	}

	TEST_METHOD(MultipleMalformedDeclarationsReportMultipleErrors)
	{
		AngelscriptNativeTestSupport::FNativeMessageCollector Messages;
		const int CompileResult = CompileSnippet("ReferenceParserMultipleMalformed", R"(
void Bad( { }
class Broken { int ; }
int Read() { return ; }
)",
			Messages);

		TestRunner->TestTrue(TEXT("Reference malformed declarations should fail to build"), CompileResult < 0);
		TestRunner->TestTrue(TEXT("Reference malformed declarations should accumulate more than one error"), CountErrors(Messages) >= 2);
	}
};

#endif
