#include "AngelscriptTestMacros.h"
#include "CQTest.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_builder.h"
#include "source/as_module.h"
#include "source/as_parser.h"
#include "source/as_scriptcode.h"
#include "source/as_scriptengine.h"
#include "source/as_scriptnode.h"
#include "EndAngelscriptHeaders.h"

#if WITH_DEV_AUTOMATION_TESTS

	namespace
	{
		struct FParserAccessor : asCParser
		{
			explicit FParserAccessor(asCBuilder* Builder)
				: asCParser(Builder)
			{
			}

			void ResetParser()
			{
				Reset();
			}

			int ParseScriptSnippetWithoutImplicitReset(asCScriptCode* InScript)
			{
				script = InScript;
				scriptNode = asCParser::ParseScript(false);
				return errorWhileParsing ? -1 : 0;
			}

			asCScriptNode* ParseExpressionSnippet(asCScriptCode* InScript)
			{
				Reset();
				script = InScript;
				return ParseExpression();
			}

			asCScriptNode* ParseStatementSnippet(asCScriptCode* InScript)
			{
				Reset();
				script = InScript;
				return ParseStatement();
			}
		};

		bool ContainsNodeType(const asCScriptNode* Node, eScriptNode ExpectedType)
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

	asCModule* CreateParserModule(asCScriptEngine* ScriptEngine, const char* ModuleName)
	{
		return static_cast<asCModule*>(ScriptEngine->GetModule(ModuleName, asGM_ALWAYS_CREATE));
	}
}

TEST_CLASS_WITH_FLAGS(FAngelscriptParserTests,
	"Angelscript.TestModule.AngelScriptSDK.Parser",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(Declarations)
	{
		asCScriptEngine* BareEngine = reinterpret_cast<asCScriptEngine*>(ASTEST_CREATE_ENGINE_NATIVE());
		if (BareEngine == nullptr)
	{
		TestRunner->AddError(TEXT("Failed to create bare AngelScript SDK engine"));
		return;
	}
	{
		ON_SCOPE_EXIT { BareEngine->ShutDownAndRelease(); };
		asCModule* Module = CreateParserModule(BareEngine, "ParserDeclarations");
		if (!TestRunner->TestNotNull(TEXT("Parser declaration test should create a backing module"), Module))
		{
			return;
		}

		asCBuilder Builder(BareEngine, Module);
		asCScriptCode Code;
		Code.SetCode("ParserDeclarations", "int GlobalValue = 7; class FSample { int Value; }", true);

		asCParser Parser(&Builder);
		const int ParseResult = Parser.ParseScript(&Code);
		if (!TestRunner->TestEqual(TEXT("Parser should successfully parse declarations"), ParseResult, 0))
		{
			return;
		}

		asCScriptNode* Root = Parser.GetScriptNode();
		if (!TestRunner->TestNotNull(TEXT("Parser should produce a root script node"), Root))
		{
			return;
		}

		TestRunner->TestEqual(TEXT("Root node should be a script node"), static_cast<int32>(Root->nodeType), static_cast<int32>(snScript));
		TestRunner->TestTrue(TEXT("Parser should emit a declaration node for the global variable"), ContainsNodeType(Root, snDeclaration));
		TestRunner->TestTrue(TEXT("Parser should emit a class node for the class declaration"), ContainsNodeType(Root, snClass));
		}
	}

	TEST_METHOD(ExpressionAst)
	{
		asCScriptEngine* BareEngine = reinterpret_cast<asCScriptEngine*>(ASTEST_CREATE_ENGINE_NATIVE());
		if (BareEngine == nullptr)
	{
		TestRunner->AddError(TEXT("Failed to create bare AngelScript SDK engine"));
		return;
	}
	{
		ON_SCOPE_EXIT { BareEngine->ShutDownAndRelease(); };
		asCModule* Module = CreateParserModule(BareEngine, "ParserExpressions");
		if (!TestRunner->TestNotNull(TEXT("Parser expression test should create a backing module"), Module))
		{
			return;
		}

		asCBuilder Builder(BareEngine, Module);
		asCScriptCode Code;
		Code.SetCode("ParserExpressions", "1 + 2 * 3", true);

		FParserAccessor Parser(&Builder);
		asCScriptNode* Root = Parser.ParseExpressionSnippet(&Code);
		if (!TestRunner->TestNotNull(TEXT("Parser should produce an AST for expression input"), Root))
		{
			return;
		}

		TestRunner->TestEqual(TEXT("Expression root should be an expression node"), static_cast<int32>(Root->nodeType), static_cast<int32>(snExpression));
		TestRunner->TestTrue(TEXT("Parser should emit an expression operator node"), ContainsNodeType(Root, snExprOperator));
		}
	}

	TEST_METHOD(ControlFlow)
	{
		asCScriptEngine* BareEngine = reinterpret_cast<asCScriptEngine*>(ASTEST_CREATE_ENGINE_NATIVE());
		if (BareEngine == nullptr)
	{
		TestRunner->AddError(TEXT("Failed to create bare AngelScript SDK engine"));
		return;
	}
	{
		ON_SCOPE_EXIT { BareEngine->ShutDownAndRelease(); };
		asCModule* Module = CreateParserModule(BareEngine, "ParserControlFlow");
		if (!TestRunner->TestNotNull(TEXT("Parser control-flow test should create a backing module"), Module))
		{
			return;
		}

		asCBuilder Builder(BareEngine, Module);
		asCScriptCode Code;
		Code.SetCode("ParserControlFlow", "if (true) { for (int i = 0; i < 3; i++) { while(false) { } } }", true);

		FParserAccessor Parser(&Builder);
		asCScriptNode* Root = Parser.ParseStatementSnippet(&Code);
		if (!TestRunner->TestNotNull(TEXT("Parser should produce an AST for control flow input"), Root))
		{
			return;
		}

		TestRunner->TestEqual(TEXT("Control-flow root should be an if node"), static_cast<int32>(Root->nodeType), static_cast<int32>(snIf));
		TestRunner->TestTrue(TEXT("Parser should emit an if node"), ContainsNodeType(Root, snIf));
		TestRunner->TestTrue(TEXT("Parser should emit a for node"), ContainsNodeType(Root, snFor));
		TestRunner->TestTrue(TEXT("Parser should emit a while node"), ContainsNodeType(Root, snWhile));
		}
	}

	TEST_METHOD(SyntaxErrors)
	{
		asCScriptEngine* BareEngine = reinterpret_cast<asCScriptEngine*>(ASTEST_CREATE_ENGINE_NATIVE());
		if (BareEngine == nullptr)
	{
		TestRunner->AddError(TEXT("Failed to create bare AngelScript SDK engine"));
		return;
	}
	{
		ON_SCOPE_EXIT { BareEngine->ShutDownAndRelease(); };
		asCModule* Module = CreateParserModule(BareEngine, "ParserSyntaxErrors");
		if (!TestRunner->TestNotNull(TEXT("Parser syntax-error test should create a backing module"), Module))
		{
			return;
		}

		asCBuilder Builder(BareEngine, Module);
		Builder.silent = true;
		asCScriptCode Code;
		Code.SetCode("ParserSyntaxErrors", "void Broken( { return; }", true);

		asCParser Parser(&Builder);
		const int ParseResult = Parser.ParseScript(&Code);
		TestRunner->TestTrue(TEXT("Parser should reject malformed syntax"), ParseResult < 0);
		}
	}

	TEST_METHOD(ReuseAfterSyntaxError)
	{
		asCScriptEngine* BareEngine = reinterpret_cast<asCScriptEngine*>(ASTEST_CREATE_ENGINE_NATIVE());
		if (BareEngine == nullptr)
	{
		TestRunner->AddError(TEXT("Failed to create bare AngelScript SDK engine"));
		return;
	}
	{
		ON_SCOPE_EXIT { BareEngine->ShutDownAndRelease(); };
		asCModule* Module = CreateParserModule(BareEngine, "ParserReuseAfterSyntaxError");
		if (!TestRunner->TestNotNull(TEXT("Parser reuse-after-error test should create a backing module"), Module))
		{
			return;
		}

		asCBuilder Builder(BareEngine, Module);
		Builder.silent = true;

		asCScriptCode InvalidCode;
		InvalidCode.SetCode("ParserReuseAfterSyntaxError_Invalid", "void Broken( { return; }", true);

		asCScriptCode ValidCode;
		ValidCode.SetCode("ParserReuseAfterSyntaxError_Valid", "int GlobalValue = 7; class FRecoveredSample { int Value; }", true);

		FParserAccessor Parser(&Builder);
		const int InvalidParseResult = Parser.ParseScriptSnippetWithoutImplicitReset(&InvalidCode);
		TestRunner->TestTrue(
			TEXT("Parser.ReuseAfterSyntaxError should fail the malformed script on the first parse"),
			InvalidParseResult < 0);

		Parser.ResetParser();

		const int ValidParseResult = Parser.ParseScriptSnippetWithoutImplicitReset(&ValidCode);
		TestRunner->TestEqual(
			TEXT("Parser.ReuseAfterSyntaxError should succeed when the same parser is reused on valid script after Reset"),
			ValidParseResult,
			0);

		asCScriptNode* Root = Parser.GetScriptNode();
		if (!TestRunner->TestNotNull(TEXT("Parser.ReuseAfterSyntaxError should produce a root node for the recovered parse"), Root))
		{
			return;
		}

		TestRunner->TestEqual(
			TEXT("Parser.ReuseAfterSyntaxError should recover a script root node after Reset"),
			static_cast<int32>(Root->nodeType),
			static_cast<int32>(snScript));
		TestRunner->TestTrue(
			TEXT("Parser.ReuseAfterSyntaxError should recover a declaration node after Reset"),
			ContainsNodeType(Root, snDeclaration));
		TestRunner->TestTrue(
			TEXT("Parser.ReuseAfterSyntaxError should recover a class node after Reset"),
			ContainsNodeType(Root, snClass));

		}
	}
};

#endif
