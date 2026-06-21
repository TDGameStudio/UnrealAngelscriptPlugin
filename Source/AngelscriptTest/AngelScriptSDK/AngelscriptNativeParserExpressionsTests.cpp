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

TEST_CLASS_WITH_FLAGS(FAngelscriptNativeParserExpressionsTests,
	"Angelscript.TestModule.AngelScriptSDK.Parser.Expressions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	static bool ParseExpression(FAutomationTestBase& Test, FNoDiscardAsserter& Assert, asCScriptEngine* ScriptEngine, const char* ModuleName, const char* Source, TFunctionRef<void(const asCScriptNode&)> Verify)
	{
		asCModule* Module = CreateSdkModule(ScriptEngine, ModuleName);
		if (!Assert.IsNotNull(Module, FString::Printf(TEXT("%s should create a parser module"), UTF8_TO_TCHAR(ModuleName))))
		{
			return false;
		}

		asCBuilder Builder(ScriptEngine, Module);
		asCScriptCode Code;
		Code.SetCode(ModuleName, Source, true);

		FParserAccessor Parser(&Builder);
		asCScriptNode* Root = Parser.ParseExpressionSnippet(&Code);
		if (!Assert.IsNotNull(Root, FString::Printf(TEXT("%s should parse an expression root"), UTF8_TO_TCHAR(ModuleName))))
		{
			return false;
		}

		if (!Assert.AreEqual(static_cast<int32>(snExpression), static_cast<int32>(Root->nodeType), FString::Printf(TEXT("%s expression root type"), UTF8_TO_TCHAR(ModuleName))))
		{
			return false;
		}
		Verify(*Root);
		return true;
	}

	static bool ParseAssignment(FAutomationTestBase& Test, FNoDiscardAsserter& Assert, asCScriptEngine* ScriptEngine, const char* ModuleName, const char* Source, TFunctionRef<void(const asCScriptNode&)> Verify)
	{
		asCModule* Module = CreateSdkModule(ScriptEngine, ModuleName);
		if (!Assert.IsNotNull(Module, FString::Printf(TEXT("%s should create a parser module"), UTF8_TO_TCHAR(ModuleName))))
		{
			return false;
		}

		asCBuilder Builder(ScriptEngine, Module);
		asCScriptCode Code;
		Code.SetCode(ModuleName, Source, true);

		FParserAccessor Parser(&Builder);
		asCScriptNode* Root = Parser.ParseAssignmentSnippet(&Code);
		if (!Assert.IsNotNull(Root, FString::Printf(TEXT("%s should parse an assignment root"), UTF8_TO_TCHAR(ModuleName))))
		{
			return false;
		}

		if (!Assert.AreEqual(static_cast<int32>(snAssignment), static_cast<int32>(Root->nodeType), FString::Printf(TEXT("%s assignment root type"), UTF8_TO_TCHAR(ModuleName))))
		{
			return false;
		}
		Verify(*Root);
		return true;
	}

	static bool ParseCondition(FAutomationTestBase& Test, FNoDiscardAsserter& Assert, asCScriptEngine* ScriptEngine, const char* ModuleName, const char* Source, TFunctionRef<void(const asCScriptNode&)> Verify)
	{
		asCModule* Module = CreateSdkModule(ScriptEngine, ModuleName);
		if (!Assert.IsNotNull(Module, FString::Printf(TEXT("%s should create a parser module"), UTF8_TO_TCHAR(ModuleName))))
		{
			return false;
		}

		asCBuilder Builder(ScriptEngine, Module);
		asCScriptCode Code;
		Code.SetCode(ModuleName, Source, true);

		FParserAccessor Parser(&Builder);
		asCScriptNode* Root = Parser.ParseConditionSnippet(&Code);
		if (!Assert.IsNotNull(Root, FString::Printf(TEXT("%s should parse a condition root"), UTF8_TO_TCHAR(ModuleName))))
		{
			return false;
		}

		if (!Assert.AreEqual(static_cast<int32>(snCondition), static_cast<int32>(Root->nodeType), FString::Printf(TEXT("%s condition root type"), UTF8_TO_TCHAR(ModuleName))))
		{
			return false;
		}
		Verify(*Root);
		return true;
	}

public:
	TEST_METHOD(PrecedenceMulOverAdd)
	{
		asCScriptEngine* BareEngine = AngelscriptNativeTestSupport::CreateBareSdkEngine(&*TestRunner);
		ASSERT_THAT(IsNotNull(BareEngine, TEXT("Parser expression test should create a bare engine")));
		ON_SCOPE_EXIT { BareEngine->ShutDownAndRelease(); };

		ParseExpression(*TestRunner, this->Assert, BareEngine, "ParserExprMulAdd", "1 + 2 * 3", [&](const asCScriptNode& Root)
		{
			ASSERT_THAT(IsTrue(AngelscriptNativeTestSupport::CountNodesOfType(&Root, snExprOperator) >= 2,
				TEXT("Mul-over-add expression should contain operator nodes")));
		});
	}

	TEST_METHOD(PrecedenceShiftOverAdd)
	{
		asCScriptEngine* BareEngine = AngelscriptNativeTestSupport::CreateBareSdkEngine(&*TestRunner);
		ASSERT_THAT(IsNotNull(BareEngine, TEXT("Parser expression test should create a bare engine")));
		ON_SCOPE_EXIT { BareEngine->ShutDownAndRelease(); };

		ParseExpression(*TestRunner, this->Assert, BareEngine, "ParserExprShiftAdd", "1 + 2 << 3", [&](const asCScriptNode& Root)
		{
			ASSERT_THAT(IsTrue(AngelscriptNativeTestSupport::CountNodesOfType(&Root, snExprOperator) >= 2,
				TEXT("Shift/add expression should contain operator nodes")));
		});
	}

	TEST_METHOD(RightAssocAssignment)
	{
		asCScriptEngine* BareEngine = AngelscriptNativeTestSupport::CreateBareSdkEngine(&*TestRunner);
		ASSERT_THAT(IsNotNull(BareEngine, TEXT("Parser expression test should create a bare engine")));
		ON_SCOPE_EXIT { BareEngine->ShutDownAndRelease(); };

		ParseAssignment(*TestRunner, this->Assert, BareEngine, "ParserExprAssign", "A = B = C", [&](const asCScriptNode& Root)
		{
			ASSERT_THAT(IsTrue(AngelscriptNativeTestSupport::CountNodesOfType(&Root, snAssignment) >= 2,
				TEXT("Right-associative assignment should contain nested assignment nodes")));
		});
	}

	TEST_METHOD(TernaryNesting)
	{
		asCScriptEngine* BareEngine = AngelscriptNativeTestSupport::CreateBareSdkEngine(&*TestRunner);
		ASSERT_THAT(IsNotNull(BareEngine, TEXT("Parser expression test should create a bare engine")));
		ON_SCOPE_EXIT { BareEngine->ShutDownAndRelease(); };

		ParseCondition(*TestRunner, this->Assert, BareEngine, "ParserExprTernary", "A ? B : (C ? D : E)", [&](const asCScriptNode& Root)
		{
			ASSERT_THAT(IsTrue(AngelscriptNativeTestSupport::CountNodesOfType(&Root, snCondition) >= 2,
				TEXT("Nested ternary expression should contain nested condition nodes")));
		});
	}

	TEST_METHOD(CastExpression)
	{
		asCScriptEngine* BareEngine = AngelscriptNativeTestSupport::CreateBareSdkEngine(&*TestRunner);
		ASSERT_THAT(IsNotNull(BareEngine, TEXT("Parser expression test should create a bare engine")));
		ON_SCOPE_EXIT { BareEngine->ShutDownAndRelease(); };

		ParseExpression(*TestRunner, this->Assert, BareEngine, "ParserExprCast", "Cast<int>(Value)", [&](const asCScriptNode& Root)
		{
			ASSERT_THAT(AreEqual(1, AngelscriptNativeTestSupport::CountNodesOfType(&Root, snCast),
				TEXT("Cast expression should produce one cast node")));
		});
	}

	TEST_METHOD(MemberAccessChain)
	{
		asCScriptEngine* BareEngine = AngelscriptNativeTestSupport::CreateBareSdkEngine(&*TestRunner);
		ASSERT_THAT(IsNotNull(BareEngine, TEXT("Parser expression test should create a bare engine")));
		ON_SCOPE_EXIT { BareEngine->ShutDownAndRelease(); };

		ParseExpression(*TestRunner, this->Assert, BareEngine, "ParserExprMemberAccess", "Object.Component.Value", [&](const asCScriptNode& Root)
		{
			ASSERT_THAT(IsTrue(AngelscriptNativeTestSupport::CountNodesOfType(&Root, snVariableAccess) >= 1,
				TEXT("Member access chain should produce variable-access nodes")));
		});
	}

	TEST_METHOD(IndexExpression)
	{
		asCScriptEngine* BareEngine = AngelscriptNativeTestSupport::CreateBareSdkEngine(&*TestRunner);
		ASSERT_THAT(IsNotNull(BareEngine, TEXT("Parser expression test should create a bare engine")));
		ON_SCOPE_EXIT { BareEngine->ShutDownAndRelease(); };

		ParseExpression(*TestRunner, this->Assert, BareEngine, "ParserExprIndex", "Values[Index + 1]", [&](const asCScriptNode& Root)
		{
			ASSERT_THAT(IsTrue(AngelscriptNativeTestSupport::CountNodesOfType(&Root, snArgList) >= 1 || AngelscriptNativeTestSupport::CountNodesOfType(&Root, snExprOperator) >= 1,
				TEXT("Index expression should produce an argument list or expression operators")));
		});
	}

	TEST_METHOD(FunctionCallWithNamedArg)
	{
		asCScriptEngine* BareEngine = AngelscriptNativeTestSupport::CreateBareSdkEngine(&*TestRunner);
		ASSERT_THAT(IsNotNull(BareEngine, TEXT("Parser expression test should create a bare engine")));
		ON_SCOPE_EXIT { BareEngine->ShutDownAndRelease(); };

		ParseExpression(*TestRunner, this->Assert, BareEngine, "ParserExprNamedArg", "DoWork(Value: 3)", [&](const asCScriptNode& Root)
		{
			ASSERT_THAT(AreEqual(1, AngelscriptNativeTestSupport::CountNodesOfType(&Root, snFunctionCall),
				TEXT("Named-arg call should produce one function call node")));
			ASSERT_THAT(AreEqual(1, AngelscriptNativeTestSupport::CountNodesOfType(&Root, snNamedArgument),
				TEXT("Named-arg call should produce one named argument node")));
		});
	}

	TEST_METHOD(AnonymousInitializerList)
	{
		asCScriptEngine* BareEngine = AngelscriptNativeTestSupport::CreateBareSdkEngine(&*TestRunner);
		ASSERT_THAT(IsNotNull(BareEngine, TEXT("Parser expression test should create a bare engine")));
		ON_SCOPE_EXIT { BareEngine->ShutDownAndRelease(); };

		ParseExpression(*TestRunner, this->Assert, BareEngine, "ParserExprInitList", "{ 1, 2, 3 }", [&](const asCScriptNode& Root)
		{
			ASSERT_THAT(AreEqual(1, AngelscriptNativeTestSupport::CountNodesOfType(&Root, snInitList),
				TEXT("Anonymous initializer list should produce one init-list node")));
		});
	}

	TEST_METHOD(LambdaIfSupported_OrDocumentReject)
	{
		asCScriptEngine* BareEngine = AngelscriptNativeTestSupport::CreateBareSdkEngine(&*TestRunner);
		ASSERT_THAT(IsNotNull(BareEngine, TEXT("Parser expression test should create a bare engine")));
		ON_SCOPE_EXIT { BareEngine->ShutDownAndRelease(); };

		ParseExpression(*TestRunner, this->Assert, BareEngine, "ParserExprLambda", "function() { return 1; }", [&](const asCScriptNode& Root)
		{
			ASSERT_THAT(AreEqual(1, AngelscriptNativeTestSupport::CountNodesOfType(&Root, snFunction),
				TEXT("Lambda expression should parse to one function node under the current parser")));
		});
	}
};

#endif
