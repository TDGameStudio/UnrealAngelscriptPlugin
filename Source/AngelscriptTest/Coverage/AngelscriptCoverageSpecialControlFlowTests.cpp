#include "CQTest.h"
#include "AngelscriptFunctionalTestUtils.h"
#include "AngelscriptGlobalFunctionInvoker.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestModuleBuilder.h"
#include "Syntax/AngelscriptSyntaxTestHelpers.h"

#include "Misc/ScopeExit.h"

// -----------------------------------------------------------------------------
// AngelscriptCoverageSpecialControlFlowTests
// -----------------------------------------------------------------------------
// Coverage landing file for short-circuit and comma-expression rows in
// Coverage_ControlFlow.md.
// -----------------------------------------------------------------------------

#if WITH_ANGELSCRIPT_UNITTESTS

using namespace AngelscriptFunctionalTestUtils;

TEST_CLASS_WITH_FLAGS(FAngelscriptCoverageSpecialControlFlowTest,
	"Angelscript.TestModule.Coverage.SpecialControlFlow",
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

	template <typename T>
	void ExpectGlobalReturn(FAngelscriptEngine& Engine, asIScriptModule* Module, const TCHAR* Declaration, const T& Expected, const TCHAR* Message)
	{
		if (Module == nullptr)
		{
			TestRunner->AddError(FString::Printf(TEXT("%s: backing module failed to build"), Message));
			return;
		}

		FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, Declaration);
		ASSERT_THAT(IsTrue(Invoker.IsValid(), TEXT("special control flow global function should resolve and prepare")));
		if (!Invoker.IsValid())
		{
			return;
		}

		const T Result = Invoker.CallAndReturn<T>();
		ASSERT_THAT(AreEqual(Expected, Result, Message));
	}

	TEST_METHOD(ShortCircuitSkipsRightHandSide)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovSpecialControlFlow_ShortCircuit", ASTEST_AS(R"AS(
			bool RecordTrue(int&inout Calls)
			{
				Calls += 1;
				return true;
			}

			bool RecordFalse(int&inout Calls)
			{
				Calls += 1;
				return false;
			}

			int AndSkipsRightSide()
			{
				int Calls = 0;
				if (false && RecordTrue(Calls))
				{
					return -1;
				}
				return Calls;
			}

			int OrSkipsRightSide()
			{
				int Calls = 0;
				if (true || RecordFalse(Calls))
				{
					return Calls;
				}
				return -1;
			}

			int RightSideEvaluatesWhenNeeded()
			{
				int Calls = 0;
				if (true && RecordTrue(Calls))
				{
					return Calls;
				}
				return -1;
			}
			)AS"));
		ON_SCOPE_EXIT
		{
			if (Module != nullptr)
			{
				Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
			}
		};

		ExpectGlobalReturn<int32>(Engine, Module, TEXT("int AndSkipsRightSide()"), 0, TEXT("&& should not evaluate RHS when LHS is false"));
		ExpectGlobalReturn<int32>(Engine, Module, TEXT("int OrSkipsRightSide()"), 0, TEXT("|| should not evaluate RHS when LHS is true"));
		ExpectGlobalReturn<int32>(Engine, Module, TEXT("int RightSideEvaluatesWhenNeeded()"), 1, TEXT("&& should evaluate RHS when LHS is true"));
	}

	TEST_METHOD(CommaExpressionUnsupportedOutsideForClauses)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		const FString CommaExpressionSource = ASTEST_AS(R"AS(
			int CommaExpression()
			{
				int A = 1;
				int B = 2;
				int C = 3;
				int X = (A, B, C);
				return X;
			}
			)AS");
		ASSERT_THAT(IsTrue(SyntaxTestHelpers::AssertFailsWithError(
			*TestRunner,
			Engine,
			TEXT("ASCovSpecialControlFlow_CommaExpressionUnsupported"),
			*CommaExpressionSource,
			TEXT("Expected ')'"),
			TEXT("comma expression outside a for clause is not supported by this fork"))));
	}

	TEST_METHOD(ForCommaClausesCompileAndExecute)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovSpecialControlFlow_ForCommaClauses", ASTEST_AS(R"AS(
			int ForCommaClauses()
			{
				int Sum = 0;
				for (int i = 0, j = 10; i < 5; i++, j--)
				{
					Sum += i + j;
				}
				return Sum;
			}
			)AS"));
		ON_SCOPE_EXIT
		{
			if (Module != nullptr)
			{
				Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
			}
		};

		ExpectGlobalReturn<int32>(Engine, Module, TEXT("int ForCommaClauses()"), 50, TEXT("for init/update comma clauses should execute"));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
