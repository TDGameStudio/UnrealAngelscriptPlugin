#include "CQTest.h"
#include "AngelscriptTestMacros.h"
#include "Syntax/AngelscriptSyntaxTestHelpers.h"

// -----------------------------------------------------------------------------
// AngelscriptCoverageCommentTests
// -----------------------------------------------------------------------------
// Coverage landing file for comment syntax rows in Coverage_ControlFlow.md.
// -----------------------------------------------------------------------------

#if WITH_DEV_AUTOMATION_TESTS

TEST_CLASS_WITH_FLAGS(FAngelscriptCoverageCommentTest,
	"Angelscript.TestModule.Coverage.Comment",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	BEFORE_ALL()
	{
		ASTEST_CREATE_ENGINE();
	}

	AFTER_ALL()
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		ASTEST_RESET_ENGINE(Engine);
	}

	TEST_METHOD(CommentFormsCompile)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		const FString SingleLineSource = ASTEST_AS(R"AS(
			// Single-line comment before a declaration.
			int SingleLineComment()
			{
				int Value = 1; // Inline single-line comment.
				return Value;
			}
			)AS");
		ASSERT_THAT(IsTrue(SyntaxTestHelpers::AssertCompiles(
			*TestRunner,
			Engine,
			TEXT("ASCovComment_SingleLine"),
			*SingleLineSource,
			TEXT("single-line comments should compile"))));

		const FString MultiLineSource = ASTEST_AS(R"AS(
			/*
				Multi-line block comment before a declaration.
			*/
			int MultiLineComment()
			{
				int Value = 2;
				/* Inline block comment */ return Value;
			}
			)AS");
		ASSERT_THAT(IsTrue(SyntaxTestHelpers::AssertCompiles(
			*TestRunner,
			Engine,
			TEXT("ASCovComment_MultiLine"),
			*MultiLineSource,
			TEXT("multi-line block comments should compile"))));

		const FString DocumentationSource = ASTEST_AS(R"AS(
			/**
			 * Documentation-style comment before a function.
			 */
			int DocumentationComment()
			{
				return 3;
			}
			)AS");
		ASSERT_THAT(IsTrue(SyntaxTestHelpers::AssertCompiles(
			*TestRunner,
			Engine,
			TEXT("ASCovComment_Doc"),
			*DocumentationSource,
			TEXT("documentation comments should compile"))));
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
