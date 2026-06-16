#include "AngelscriptNativeTestSupport.h"
#include "CQTest.h"
#include "Misc/ScopeExit.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_builder.h"
#include "source/as_module.h"
#include "source/as_parser.h"
#include "source/as_scriptcode.h"
#include "source/as_scriptengine.h"
#include "source/as_scriptnode.h"
#include "EndAngelscriptHeaders.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptNativeTestSupport;

namespace
{
	bool ParseExpression(FAutomationTestBase& Test, asCScriptEngine* ScriptEngine, const char* ModuleName, const char* Source, TFunctionRef<void(const asCScriptNode&)> Verify)
	{
		asCModule* Module = CreateSdkModule(ScriptEngine, ModuleName);
		if (!Test.TestNotNull(FString::Printf(TEXT("%s should create a parser module"), UTF8_TO_TCHAR(ModuleName)), Module))
		{
			return false;
		}

		asCBuilder Builder(ScriptEngine, Module);
		asCScriptCode Code;
		Code.SetCode(ModuleName, Source, true);

		FParserAccessor Parser(&Builder);
		asCScriptNode* Root = Parser.ParseExpressionSnippet(&Code);
		if (!Test.TestNotNull(FString::Printf(TEXT("%s should parse an expression root"), UTF8_TO_TCHAR(ModuleName)), Root))
		{
			return false;
		}

		Test.TestEqual(FString::Printf(TEXT("%s expression root type"), UTF8_TO_TCHAR(ModuleName)), static_cast<int32>(Root->nodeType), static_cast<int32>(snExpression));
		Verify(*Root);
		return true;
	}

	bool ParseAssignment(FAutomationTestBase& Test, asCScriptEngine* ScriptEngine, const char* ModuleName, const char* Source, TFunctionRef<void(const asCScriptNode&)> Verify)
	{
		asCModule* Module = CreateSdkModule(ScriptEngine, ModuleName);
		if (!Test.TestNotNull(FString::Printf(TEXT("%s should create a parser module"), UTF8_TO_TCHAR(ModuleName)), Module))
		{
			return false;
		}

		asCBuilder Builder(ScriptEngine, Module);
		asCScriptCode Code;
		Code.SetCode(ModuleName, Source, true);

		FParserAccessor Parser(&Builder);
		asCScriptNode* Root = Parser.ParseAssignmentSnippet(&Code);
		if (!Test.TestNotNull(FString::Printf(TEXT("%s should parse an assignment root"), UTF8_TO_TCHAR(ModuleName)), Root))
		{
			return false;
		}

		Test.TestEqual(FString::Printf(TEXT("%s assignment root type"), UTF8_TO_TCHAR(ModuleName)), static_cast<int32>(Root->nodeType), static_cast<int32>(snAssignment));
		Verify(*Root);
		return true;
	}

	bool ParseCondition(FAutomationTestBase& Test, asCScriptEngine* ScriptEngine, const char* ModuleName, const char* Source, TFunctionRef<void(const asCScriptNode&)> Verify)
	{
		asCModule* Module = CreateSdkModule(ScriptEngine, ModuleName);
		if (!Test.TestNotNull(FString::Printf(TEXT("%s should create a parser module"), UTF8_TO_TCHAR(ModuleName)), Module))
		{
			return false;
		}

		asCBuilder Builder(ScriptEngine, Module);
		asCScriptCode Code;
		Code.SetCode(ModuleName, Source, true);

		FParserAccessor Parser(&Builder);
		asCScriptNode* Root = Parser.ParseConditionSnippet(&Code);
		if (!Test.TestNotNull(FString::Printf(TEXT("%s should parse a condition root"), UTF8_TO_TCHAR(ModuleName)), Root))
		{
			return false;
		}

		Test.TestEqual(FString::Printf(TEXT("%s condition root type"), UTF8_TO_TCHAR(ModuleName)), static_cast<int32>(Root->nodeType), static_cast<int32>(snCondition));
		Verify(*Root);
		return true;
	}
}

TEST_CLASS_WITH_FLAGS(FAngelscriptNativeParserExpressionsTests,
	"Angelscript.TestModule.AngelScriptSDK.Parser.Expressions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(PrecedenceMulOverAdd)
	{
		asCScriptEngine* BareEngine = AngelscriptNativeTestSupport::CreateBareSdkEngine(&*TestRunner);
		if (!TestRunner->TestNotNull(TEXT("Parser expression test should create a bare engine"), BareEngine))
		{
			return;
		}
		ON_SCOPE_EXIT { BareEngine->ShutDownAndRelease(); };

		ParseExpression(*TestRunner, BareEngine, "ParserExprMulAdd", "1 + 2 * 3", [&](const asCScriptNode& Root)
		{
			TestRunner->TestTrue(TEXT("Mul-over-add expression should contain operator nodes"), AngelscriptNativeTestSupport::CountNodesOfType(&Root, snExprOperator) >= 2);
		});
	}

	TEST_METHOD(PrecedenceShiftOverAdd)
	{
		asCScriptEngine* BareEngine = AngelscriptNativeTestSupport::CreateBareSdkEngine(&*TestRunner);
		if (!TestRunner->TestNotNull(TEXT("Parser expression test should create a bare engine"), BareEngine))
		{
			return;
		}
		ON_SCOPE_EXIT { BareEngine->ShutDownAndRelease(); };

		ParseExpression(*TestRunner, BareEngine, "ParserExprShiftAdd", "1 + 2 << 3", [&](const asCScriptNode& Root)
		{
			TestRunner->TestTrue(TEXT("Shift/add expression should contain operator nodes"), AngelscriptNativeTestSupport::CountNodesOfType(&Root, snExprOperator) >= 2);
		});
	}

	TEST_METHOD(RightAssocAssignment)
	{
		asCScriptEngine* BareEngine = AngelscriptNativeTestSupport::CreateBareSdkEngine(&*TestRunner);
		if (!TestRunner->TestNotNull(TEXT("Parser expression test should create a bare engine"), BareEngine))
		{
			return;
		}
		ON_SCOPE_EXIT { BareEngine->ShutDownAndRelease(); };

		ParseAssignment(*TestRunner, BareEngine, "ParserExprAssign", "A = B = C", [&](const asCScriptNode& Root)
		{
			TestRunner->TestTrue(TEXT("Right-associative assignment should contain nested assignment nodes"), AngelscriptNativeTestSupport::CountNodesOfType(&Root, snAssignment) >= 2);
		});
	}

	TEST_METHOD(TernaryNesting)
	{
		asCScriptEngine* BareEngine = AngelscriptNativeTestSupport::CreateBareSdkEngine(&*TestRunner);
		if (!TestRunner->TestNotNull(TEXT("Parser expression test should create a bare engine"), BareEngine))
		{
			return;
		}
		ON_SCOPE_EXIT { BareEngine->ShutDownAndRelease(); };

		ParseCondition(*TestRunner, BareEngine, "ParserExprTernary", "A ? B : (C ? D : E)", [&](const asCScriptNode& Root)
		{
			TestRunner->TestTrue(TEXT("Nested ternary expression should contain nested condition nodes"), AngelscriptNativeTestSupport::CountNodesOfType(&Root, snCondition) >= 2);
		});
	}

	TEST_METHOD(CastExpression)
	{
		asCScriptEngine* BareEngine = AngelscriptNativeTestSupport::CreateBareSdkEngine(&*TestRunner);
		if (!TestRunner->TestNotNull(TEXT("Parser expression test should create a bare engine"), BareEngine))
		{
			return;
		}
		ON_SCOPE_EXIT { BareEngine->ShutDownAndRelease(); };

		ParseExpression(*TestRunner, BareEngine, "ParserExprCast", "Cast<int>(Value)", [&](const asCScriptNode& Root)
		{
			TestRunner->TestEqual(TEXT("Cast expression should produce one cast node"), AngelscriptNativeTestSupport::CountNodesOfType(&Root, snCast), 1);
		});
	}

	TEST_METHOD(MemberAccessChain)
	{
		asCScriptEngine* BareEngine = AngelscriptNativeTestSupport::CreateBareSdkEngine(&*TestRunner);
		if (!TestRunner->TestNotNull(TEXT("Parser expression test should create a bare engine"), BareEngine))
		{
			return;
		}
		ON_SCOPE_EXIT { BareEngine->ShutDownAndRelease(); };

		ParseExpression(*TestRunner, BareEngine, "ParserExprMemberAccess", "Object.Component.Value", [&](const asCScriptNode& Root)
		{
			TestRunner->TestTrue(TEXT("Member access chain should produce variable-access nodes"), AngelscriptNativeTestSupport::CountNodesOfType(&Root, snVariableAccess) >= 1);
		});
	}

	TEST_METHOD(IndexExpression)
	{
		asCScriptEngine* BareEngine = AngelscriptNativeTestSupport::CreateBareSdkEngine(&*TestRunner);
		if (!TestRunner->TestNotNull(TEXT("Parser expression test should create a bare engine"), BareEngine))
		{
			return;
		}
		ON_SCOPE_EXIT { BareEngine->ShutDownAndRelease(); };

		ParseExpression(*TestRunner, BareEngine, "ParserExprIndex", "Values[Index + 1]", [&](const asCScriptNode& Root)
		{
			TestRunner->TestTrue(TEXT("Index expression should produce an argument list or expression operators"), AngelscriptNativeTestSupport::CountNodesOfType(&Root, snArgList) >= 1 || AngelscriptNativeTestSupport::CountNodesOfType(&Root, snExprOperator) >= 1);
		});
	}

	TEST_METHOD(FunctionCallWithNamedArg)
	{
		asCScriptEngine* BareEngine = AngelscriptNativeTestSupport::CreateBareSdkEngine(&*TestRunner);
		if (!TestRunner->TestNotNull(TEXT("Parser expression test should create a bare engine"), BareEngine))
		{
			return;
		}
		ON_SCOPE_EXIT { BareEngine->ShutDownAndRelease(); };

		ParseExpression(*TestRunner, BareEngine, "ParserExprNamedArg", "DoWork(Value: 3)", [&](const asCScriptNode& Root)
		{
			TestRunner->TestEqual(TEXT("Named-arg call should produce one function call node"), AngelscriptNativeTestSupport::CountNodesOfType(&Root, snFunctionCall), 1);
			TestRunner->TestEqual(TEXT("Named-arg call should produce one named argument node"), AngelscriptNativeTestSupport::CountNodesOfType(&Root, snNamedArgument), 1);
		});
	}

	TEST_METHOD(AnonymousInitializerList)
	{
		asCScriptEngine* BareEngine = AngelscriptNativeTestSupport::CreateBareSdkEngine(&*TestRunner);
		if (!TestRunner->TestNotNull(TEXT("Parser expression test should create a bare engine"), BareEngine))
		{
			return;
		}
		ON_SCOPE_EXIT { BareEngine->ShutDownAndRelease(); };

		ParseExpression(*TestRunner, BareEngine, "ParserExprInitList", "{ 1, 2, 3 }", [&](const asCScriptNode& Root)
		{
			TestRunner->TestEqual(TEXT("Anonymous initializer list should produce one init-list node"), AngelscriptNativeTestSupport::CountNodesOfType(&Root, snInitList), 1);
		});
	}

	TEST_METHOD(LambdaIfSupported_OrDocumentReject)
	{
		asCScriptEngine* BareEngine = AngelscriptNativeTestSupport::CreateBareSdkEngine(&*TestRunner);
		if (!TestRunner->TestNotNull(TEXT("Parser expression test should create a bare engine"), BareEngine))
		{
			return;
		}
		ON_SCOPE_EXIT { BareEngine->ShutDownAndRelease(); };

		ParseExpression(*TestRunner, BareEngine, "ParserExprLambda", "function() { return 1; }", [&](const asCScriptNode& Root)
		{
			TestRunner->TestEqual(TEXT("Lambda expression should parse to one function node under the current parser"), AngelscriptNativeTestSupport::CountNodesOfType(&Root, snFunction), 1);
		});
	}
};

#endif
