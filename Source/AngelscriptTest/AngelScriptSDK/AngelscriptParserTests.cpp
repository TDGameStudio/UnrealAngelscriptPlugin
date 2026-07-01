#include "AngelscriptTestMacros.h"
#include "AngelscriptNativeTestSupport.h"
#include "CQTest.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_builder.h"
#include "source/as_module.h"
#include "source/as_parser.h"
#include "source/as_scriptcode.h"
#include "source/as_scriptengine.h"
#include "source/as_scriptnode.h"
#include "EndAngelscriptHeaders.h"

#if WITH_ANGELSCRIPT_UNITTESTS


TEST_CLASS_WITH_FLAGS(FAngelscriptParserTests,
	"Angelscript.TestModule.AngelScriptSDK.Parser",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	static bool ContainsNodeType(const asCScriptNode* Node, eScriptNode ExpectedType)
	{
		for (const asCScriptNode* Current = Node; Current != nullptr; Current = Current->next)
		{
			if (Current->nodeType == ExpectedType)
			{
				return true;
			}

			if (Current->firstChild != nullptr && ContainsNodeType(Current->firstChild, ExpectedType))
			{
				return true;
			}
		}

		return false;
	}

	static asCModule* CreateParserModule(asCScriptEngine* ScriptEngine, const char* ModuleName)
	{
		return static_cast<asCModule*>(ScriptEngine->GetModule(ModuleName, asGM_ALWAYS_CREATE));
	}

public:
	TEST_METHOD(Declarations)
	{
		asCScriptEngine* BareEngine = reinterpret_cast<asCScriptEngine*>(ASTEST_CREATE_ENGINE_NATIVE());
		if (BareEngine == nullptr)
		{
			TestRunner->AddError(TEXT("Failed to create bare AngelScript SDK engine"));
			return;
		}

		ON_SCOPE_EXIT { BareEngine->ShutDownAndRelease(); };
		asCModule* Module = CreateParserModule(BareEngine, "ParserDeclarations");
		ASSERT_THAT(IsNotNull(Module,
			TEXT("Parser declaration test should create a backing module")));

		asCBuilder Builder(BareEngine, Module);
		asCScriptCode Code;
		Code.SetCode("ParserDeclarations", "int GlobalValue = 7; class FSample { int Value; }", true);

		asCParser Parser(&Builder);
		const int ParseResult = Parser.ParseScript(&Code);
		ASSERT_THAT(AreEqual(0, ParseResult,
			TEXT("Parser should successfully parse declarations")));

		asCScriptNode* Root = Parser.GetScriptNode();
		ASSERT_THAT(IsNotNull(Root,
			TEXT("Parser should produce a root script node")));

		ASSERT_THAT(AreEqual(static_cast<int32>(snScript), static_cast<int32>(Root->nodeType),
			TEXT("Root node should be a script node")));
		ASSERT_THAT(IsTrue(ContainsNodeType(Root, snDeclaration),
			TEXT("Parser should emit a declaration node for the global variable")));
		ASSERT_THAT(IsTrue(ContainsNodeType(Root, snClass),
			TEXT("Parser should emit a class node for the class declaration")));
	}

	TEST_METHOD(ExpressionAst)
	{
		asCScriptEngine* BareEngine = reinterpret_cast<asCScriptEngine*>(ASTEST_CREATE_ENGINE_NATIVE());
		if (BareEngine == nullptr)
		{
			TestRunner->AddError(TEXT("Failed to create bare AngelScript SDK engine"));
			return;
		}

		ON_SCOPE_EXIT { BareEngine->ShutDownAndRelease(); };
		asCModule* Module = CreateParserModule(BareEngine, "ParserExpressions");
		ASSERT_THAT(IsNotNull(Module,
			TEXT("Parser expression test should create a backing module")));

		asCBuilder Builder(BareEngine, Module);
		asCScriptCode Code;
		Code.SetCode("ParserExpressions", "1 + 2 * 3", true);

		AngelscriptNativeTestSupport::FParserAccessor Parser(&Builder);
		asCScriptNode* Root = Parser.ParseExpressionSnippet(&Code);
		ASSERT_THAT(IsNotNull(Root,
			TEXT("Parser should produce an AST for expression input")));

		ASSERT_THAT(AreEqual(static_cast<int32>(snExpression), static_cast<int32>(Root->nodeType),
			TEXT("Expression root should be an expression node")));
		ASSERT_THAT(IsTrue(ContainsNodeType(Root, snExprOperator),
			TEXT("Parser should emit an expression operator node")));
	}

	TEST_METHOD(ControlFlow)
	{
		asCScriptEngine* BareEngine = reinterpret_cast<asCScriptEngine*>(ASTEST_CREATE_ENGINE_NATIVE());
		if (BareEngine == nullptr)
		{
			TestRunner->AddError(TEXT("Failed to create bare AngelScript SDK engine"));
			return;
		}

		ON_SCOPE_EXIT { BareEngine->ShutDownAndRelease(); };
		asCModule* Module = CreateParserModule(BareEngine, "ParserControlFlow");
		ASSERT_THAT(IsNotNull(Module,
			TEXT("Parser control-flow test should create a backing module")));

		asCBuilder Builder(BareEngine, Module);
		asCScriptCode Code;
		Code.SetCode("ParserControlFlow", "if (true) { for (int i = 0; i < 3; i++) { while(false) { } } }", true);

		AngelscriptNativeTestSupport::FParserAccessor Parser(&Builder);
		asCScriptNode* Root = Parser.ParseStatementSnippet(&Code);
		ASSERT_THAT(IsNotNull(Root,
			TEXT("Parser should produce an AST for control flow input")));

		ASSERT_THAT(AreEqual(static_cast<int32>(snIf), static_cast<int32>(Root->nodeType),
			TEXT("Control-flow root should be an if node")));
		ASSERT_THAT(IsTrue(ContainsNodeType(Root, snIf),
			TEXT("Parser should emit an if node")));
		ASSERT_THAT(IsTrue(ContainsNodeType(Root, snFor),
			TEXT("Parser should emit a for node")));
		ASSERT_THAT(IsTrue(ContainsNodeType(Root, snWhile),
			TEXT("Parser should emit a while node")));
	}

	TEST_METHOD(SyntaxErrors)
	{
		asCScriptEngine* BareEngine = reinterpret_cast<asCScriptEngine*>(ASTEST_CREATE_ENGINE_NATIVE());
		if (BareEngine == nullptr)
		{
			TestRunner->AddError(TEXT("Failed to create bare AngelScript SDK engine"));
			return;
		}

		ON_SCOPE_EXIT { BareEngine->ShutDownAndRelease(); };
		asCModule* Module = CreateParserModule(BareEngine, "ParserSyntaxErrors");
		ASSERT_THAT(IsNotNull(Module,
			TEXT("Parser syntax-error test should create a backing module")));

		asCBuilder Builder(BareEngine, Module);
		Builder.silent = true;
		asCScriptCode Code;
		Code.SetCode("ParserSyntaxErrors", "void Broken( { return; }", true);

		asCParser Parser(&Builder);
		const int ParseResult = Parser.ParseScript(&Code);
		ASSERT_THAT(IsTrue(ParseResult < 0,
			TEXT("Parser should reject malformed syntax")));
	}

	TEST_METHOD(ReuseAfterSyntaxError)
	{
		asCScriptEngine* BareEngine = reinterpret_cast<asCScriptEngine*>(ASTEST_CREATE_ENGINE_NATIVE());
		if (BareEngine == nullptr)
		{
			TestRunner->AddError(TEXT("Failed to create bare AngelScript SDK engine"));
			return;
		}

		ON_SCOPE_EXIT { BareEngine->ShutDownAndRelease(); };
		asCModule* Module = CreateParserModule(BareEngine, "ParserReuseAfterSyntaxError");
		ASSERT_THAT(IsNotNull(Module,
			TEXT("Parser reuse-after-error test should create a backing module")));

		asCBuilder Builder(BareEngine, Module);
		Builder.silent = true;

		asCScriptCode InvalidCode;
		InvalidCode.SetCode("ParserReuseAfterSyntaxError_Invalid", "void Broken( { return; }", true);

		asCScriptCode ValidCode;
		ValidCode.SetCode("ParserReuseAfterSyntaxError_Valid", "int GlobalValue = 7; class FRecoveredSample { int Value; }", true);

		AngelscriptNativeTestSupport::FParserAccessor Parser(&Builder);
		const int InvalidParseResult = Parser.ParseScriptSnippetWithoutImplicitReset(&InvalidCode);
		ASSERT_THAT(IsTrue(InvalidParseResult < 0,
			TEXT("Parser.ReuseAfterSyntaxError should fail the malformed script on the first parse")));

		Parser.ResetParser();

		const int ValidParseResult = Parser.ParseScriptSnippetWithoutImplicitReset(&ValidCode);
		ASSERT_THAT(AreEqual(0, ValidParseResult,
			TEXT("Parser.ReuseAfterSyntaxError should succeed when the same parser is reused on valid script after Reset")));

		asCScriptNode* Root = Parser.GetScriptNode();
		ASSERT_THAT(IsNotNull(Root,
			TEXT("Parser.ReuseAfterSyntaxError should produce a root node for the recovered parse")));

		ASSERT_THAT(AreEqual(static_cast<int32>(snScript), static_cast<int32>(Root->nodeType),
			TEXT("Parser.ReuseAfterSyntaxError should recover a script root node after Reset")));
		ASSERT_THAT(IsTrue(ContainsNodeType(Root, snDeclaration),
			TEXT("Parser.ReuseAfterSyntaxError should recover a declaration node after Reset")));
		ASSERT_THAT(IsTrue(ContainsNodeType(Root, snClass),
			TEXT("Parser.ReuseAfterSyntaxError should recover a class node after Reset")));
	}
};

#endif
