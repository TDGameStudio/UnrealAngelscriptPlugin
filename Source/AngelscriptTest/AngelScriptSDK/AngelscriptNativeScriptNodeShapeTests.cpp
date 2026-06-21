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


TEST_CLASS_WITH_FLAGS(FAngelscriptNativeScriptNodeShapeTests,
	"Angelscript.TestModule.AngelScriptSDK.ScriptNode.Shape",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	static bool ParseShapeScript(FAutomationTestBase& Test, FNoDiscardAsserter& Assert, asCScriptEngine* ScriptEngine, const char* ModuleName, const char* Source, TFunctionRef<void(const asCScriptNode&)> Verify)
	{
		asCModule* Module = CreateSdkModule(ScriptEngine, ModuleName);
		if (!Assert.IsNotNull(Module, FString::Printf(TEXT("%s should create a script-node module"), UTF8_TO_TCHAR(ModuleName))))
		{
			return false;
		}

		asCBuilder Builder(ScriptEngine, Module);
		asCScriptCode Code;
		Code.SetCode(ModuleName, Source, true);

		FParserAccessor Parser(&Builder);
		const int ParseResult = Parser.ParseScript(&Code);
		if (!Assert.AreEqual(0, ParseResult, FString::Printf(TEXT("%s should parse successfully"), UTF8_TO_TCHAR(ModuleName))))
		{
			return false;
		}

		const asCScriptNode* Root = Parser.GetScriptNode();
		if (!Assert.IsNotNull(Root, FString::Printf(TEXT("%s should produce a script root"), UTF8_TO_TCHAR(ModuleName))))
		{
			return false;
		}

		Verify(*Root);
		return true;
	}

	static bool ParseStatement(FAutomationTestBase& Test, FNoDiscardAsserter& Assert, asCScriptEngine* ScriptEngine, const char* ModuleName, const char* Source, TFunctionRef<void(const asCScriptNode&)> Verify)
	{
		asCModule* Module = CreateSdkModule(ScriptEngine, ModuleName);
		if (!Assert.IsNotNull(Module, FString::Printf(TEXT("%s should create a statement parser module"), UTF8_TO_TCHAR(ModuleName))))
		{
			return false;
		}

		asCBuilder Builder(ScriptEngine, Module);
		asCScriptCode Code;
		Code.SetCode(ModuleName, Source, true);

		FParserAccessor Parser(&Builder);
		const asCScriptNode* Root = Parser.ParseStatementSnippet(&Code);
		if (!Assert.IsNotNull(Root, FString::Printf(TEXT("%s should parse a statement root"), UTF8_TO_TCHAR(ModuleName))))
		{
			return false;
		}

		Verify(*Root);
		return true;
	}

	static int32 CountDirectChildren(const asCScriptNode* Node)
	{
		int32 Count = 0;
		for (const asCScriptNode* Child = Node != nullptr ? Node->firstChild : nullptr; Child != nullptr; Child = Child->next)
		{
			++Count;
		}
		return Count;
	}

public:
	TEST_METHOD(FunctionNodeChildrenLayout)
	{
		asCScriptEngine* BareEngine = AngelscriptNativeTestSupport::CreateBareSdkEngine(&*TestRunner);
		ASSERT_THAT(IsNotNull(BareEngine, TEXT("ScriptNode shape test should create a bare engine")));
		ON_SCOPE_EXIT { BareEngine->ShutDownAndRelease(); };

		ParseShapeScript(*TestRunner, this->Assert, BareEngine, "ScriptNodeShapeFunction", "int Add(int A, int B) { return A + B; }", [&](const asCScriptNode& Root)
		{
			if (!this->Assert.AreEqual(1, AngelscriptNativeTestSupport::CountNodesOfType(&Root, snFunction),
				TEXT("Function script should produce one function node")))
			{
				return;
			}
			if (!this->Assert.IsTrue(CountDirectChildren(Root.firstChild) >= 4,
				TEXT("Function node should carry return type, identifier, parameters, and block children")))
			{
				return;
			}
		});
	}

	TEST_METHOD(ParameterListNodeShape)
	{
		asCScriptEngine* BareEngine = AngelscriptNativeTestSupport::CreateBareSdkEngine(&*TestRunner);
		ASSERT_THAT(IsNotNull(BareEngine, TEXT("ScriptNode shape test should create a bare engine")));
		ON_SCOPE_EXIT { BareEngine->ShutDownAndRelease(); };

		ParseShapeScript(*TestRunner, this->Assert, BareEngine, "ScriptNodeShapeParameters", "void Visit(int A, float B, const string& in Name) { }", [&](const asCScriptNode& Root)
		{
			if (!this->Assert.AreEqual(1, AngelscriptNativeTestSupport::CountNodesOfType(&Root, snParameterList),
				TEXT("Parameter list should be represented once")))
			{
				return;
			}
			if (!this->Assert.IsTrue(AngelscriptNativeTestSupport::CountNodesOfType(&Root, snDataType) >= 4,
				TEXT("Parameter list should include data type and identifier nodes")))
			{
				return;
			}
		});
	}

	TEST_METHOD(StatementBlockHoldsStatements)
	{
		asCScriptEngine* BareEngine = AngelscriptNativeTestSupport::CreateBareSdkEngine(&*TestRunner);
		ASSERT_THAT(IsNotNull(BareEngine, TEXT("ScriptNode shape test should create a bare engine")));
		ON_SCOPE_EXIT { BareEngine->ShutDownAndRelease(); };

		ParseStatement(*TestRunner, this->Assert, BareEngine, "ScriptNodeShapeStatementBlock", "{ int A = 1; A += 2; return; }", [&](const asCScriptNode& Root)
		{
			if (!this->Assert.AreEqual(static_cast<int32>(snStatementBlock), static_cast<int32>(Root.nodeType),
				TEXT("Statement block should parse to a statement-block root")))
			{
				return;
			}
			if (!this->Assert.AreEqual(1, AngelscriptNativeTestSupport::CountNodesOfType(&Root, snStatementBlock),
				TEXT("Function body should produce one statement block")))
			{
				return;
			}
			if (!this->Assert.AreEqual(3, CountDirectChildren(&Root),
				TEXT("Statement block should retain all direct child statements")))
			{
				return;
			}
			if (!this->Assert.AreEqual(1, AngelscriptNativeTestSupport::CountNodesOfType(&Root, snReturn),
				TEXT("Statement block should still contain the return statement")))
			{
				return;
			}
		});
	}

	TEST_METHOD(ReturnNodeHasOptionalExpression)
	{
		asCScriptEngine* BareEngine = AngelscriptNativeTestSupport::CreateBareSdkEngine(&*TestRunner);
		ASSERT_THAT(IsNotNull(BareEngine, TEXT("ScriptNode shape test should create a bare engine")));
		ON_SCOPE_EXIT { BareEngine->ShutDownAndRelease(); };

		ParseStatement(*TestRunner, this->Assert, BareEngine, "ScriptNodeShapeReturn", "return 42;", [&](const asCScriptNode& Root)
		{
			if (!this->Assert.AreEqual(static_cast<int32>(snReturn), static_cast<int32>(Root.nodeType),
				TEXT("Return statement should parse to a return root node")))
			{
				return;
			}
			if (!this->Assert.IsTrue(AngelscriptNativeTestSupport::CountNodesOfType(&Root, snExpression) >= 1,
				TEXT("Value-return statement should include an expression child")))
			{
				return;
			}
		});
	}

	TEST_METHOD(BreakAndContinueAreLeafNodes)
	{
		asCScriptEngine* BareEngine = AngelscriptNativeTestSupport::CreateBareSdkEngine(&*TestRunner);
		ASSERT_THAT(IsNotNull(BareEngine, TEXT("ScriptNode shape test should create a bare engine")));
		ON_SCOPE_EXIT { BareEngine->ShutDownAndRelease(); };

		ParseStatement(*TestRunner, this->Assert, BareEngine, "ScriptNodeShapeBreak", "break;", [&](const asCScriptNode& Root)
		{
			if (!this->Assert.AreEqual(static_cast<int32>(snBreak), static_cast<int32>(Root.nodeType),
				TEXT("Break statement should parse to a break root node")))
			{
				return;
			}
			if (!this->Assert.AreEqual(0, CountDirectChildren(&Root),
				TEXT("Break statement should remain a leaf node")))
			{
				return;
			}
		});

		ParseStatement(*TestRunner, this->Assert, BareEngine, "ScriptNodeShapeContinue", "continue;", [&](const asCScriptNode& Root)
		{
			if (!this->Assert.AreEqual(static_cast<int32>(snContinue), static_cast<int32>(Root.nodeType),
				TEXT("Continue statement should parse to a continue root node")))
			{
				return;
			}
			if (!this->Assert.AreEqual(0, CountDirectChildren(&Root),
				TEXT("Continue statement should remain a leaf node")))
			{
				return;
			}
		});
	}

	TEST_METHOD(DoWhileShape)
	{
		asCScriptEngine* BareEngine = AngelscriptNativeTestSupport::CreateBareSdkEngine(&*TestRunner);
		ASSERT_THAT(IsNotNull(BareEngine, TEXT("ScriptNode shape test should create a bare engine")));
		ON_SCOPE_EXIT { BareEngine->ShutDownAndRelease(); };

		ParseStatement(*TestRunner, this->Assert, BareEngine, "ScriptNodeShapeDoWhile", "do { continue; } while (true);", [&](const asCScriptNode& Root)
		{
			if (!this->Assert.AreEqual(static_cast<int32>(snDoWhile), static_cast<int32>(Root.nodeType),
				TEXT("Do/while statement root should be a do-while node")))
			{
				return;
			}
			if (!this->Assert.AreEqual(1, AngelscriptNativeTestSupport::CountNodesOfType(&Root, snContinue),
				TEXT("Do/while statement should contain a continue node")))
			{
				return;
			}
			if (!this->Assert.IsTrue(AngelscriptNativeTestSupport::CountNodesOfType(&Root, snCondition) >= 1,
				TEXT("Do/while statement should include a condition subtree")))
			{
				return;
			}
		});
	}

	TEST_METHOD(SwitchAndCaseShape)
	{
		asCScriptEngine* BareEngine = AngelscriptNativeTestSupport::CreateBareSdkEngine(&*TestRunner);
		ASSERT_THAT(IsNotNull(BareEngine, TEXT("ScriptNode shape test should create a bare engine")));
		ON_SCOPE_EXIT { BareEngine->ShutDownAndRelease(); };

		ParseStatement(*TestRunner, this->Assert, BareEngine, "ScriptNodeShapeSwitch", "switch (Value) { case 1: break; default: break; }", [&](const asCScriptNode& Root)
		{
			if (!this->Assert.AreEqual(static_cast<int32>(snSwitch), static_cast<int32>(Root.nodeType),
				TEXT("Switch statement root should be a switch node")))
			{
				return;
			}
			if (!this->Assert.AreEqual(2, AngelscriptNativeTestSupport::CountNodesOfType(&Root, snCase),
				TEXT("Switch should carry two case/default nodes")))
			{
				return;
			}
			if (!this->Assert.AreEqual(2, AngelscriptNativeTestSupport::CountNodesOfType(&Root, snBreak),
				TEXT("Switch cases should each include a break node")))
			{
				return;
			}
		});
	}

	TEST_METHOD(EnumNodeAndEnumValueChildren)
	{
		asCScriptEngine* BareEngine = AngelscriptNativeTestSupport::CreateBareSdkEngine(&*TestRunner);
		ASSERT_THAT(IsNotNull(BareEngine, TEXT("ScriptNode shape test should create a bare engine")));
		ON_SCOPE_EXIT { BareEngine->ShutDownAndRelease(); };

		ParseShapeScript(*TestRunner, this->Assert, BareEngine, "ScriptNodeShapeEnum", "enum EMode { Idle = 0, Run = 1, Jump }", [&](const asCScriptNode& Root)
		{
			if (!this->Assert.AreEqual(1, AngelscriptNativeTestSupport::CountNodesOfType(&Root, snEnum),
				TEXT("Enum declaration should produce one enum node")))
			{
				return;
			}
			if (!this->Assert.IsTrue(AngelscriptNativeTestSupport::CountNodesOfType(&Root, snIdentifier) >= 4,
				TEXT("Enum declaration should keep enum and value identifiers")))
			{
				return;
			}
		});
	}

	TEST_METHOD(InterfaceNodeShape)
	{
		asCScriptEngine* BareEngine = AngelscriptNativeTestSupport::CreateBareSdkEngine(&*TestRunner);
		ASSERT_THAT(IsNotNull(BareEngine, TEXT("ScriptNode shape test should create a bare engine")));
		ON_SCOPE_EXIT { BareEngine->ShutDownAndRelease(); };

		asCModule* Module = CreateSdkModule(BareEngine, "ScriptNodeShapeInterface");
		asCBuilder Builder(BareEngine, Module);
		Builder.silent = true;
		asCScriptCode Code;
		Code.SetCode("ScriptNodeShapeInterface", "interface IThing { void Run(); }", true);
		FParserAccessor Parser(&Builder);
		ASSERT_THAT(IsTrue(Parser.ParseScript(&Code) < 0,
			TEXT("Script-level interface declaration should remain rejected in this native parser mode")));
	}

	TEST_METHOD(ImportNodeShape)
	{
		asCScriptEngine* BareEngine = AngelscriptNativeTestSupport::CreateBareSdkEngine(&*TestRunner);
		ASSERT_THAT(IsNotNull(BareEngine, TEXT("ScriptNode shape test should create a bare engine")));
		ON_SCOPE_EXIT { BareEngine->ShutDownAndRelease(); };

		ParseShapeScript(*TestRunner, this->Assert, BareEngine, "ScriptNodeShapeImport", "import int SharedValue() from \"OtherModule\";", [&](const asCScriptNode& Root)
		{
			if (!this->Assert.AreEqual(1, AngelscriptNativeTestSupport::CountNodesOfType(&Root, snImport),
				TEXT("Import declaration should produce one import node")))
			{
				return;
			}
			if (!this->Assert.IsTrue(AngelscriptNativeTestSupport::CountNodesOfType(&Root, snDataType) >= 1 && AngelscriptNativeTestSupport::CountNodesOfType(&Root, snParameterList) >= 1,
				TEXT("Import declaration should carry a function-definition subtree")))
			{
				return;
			}
		});
	}

	TEST_METHOD(FuncDefNodeShape)
	{
		asCScriptEngine* BareEngine = AngelscriptNativeTestSupport::CreateBareSdkEngine(&*TestRunner);
		ASSERT_THAT(IsNotNull(BareEngine, TEXT("ScriptNode shape test should create a bare engine")));
		ON_SCOPE_EXIT { BareEngine->ShutDownAndRelease(); };

		asCModule* Module = CreateSdkModule(BareEngine, "ScriptNodeShapeFuncDef");
		asCBuilder Builder(BareEngine, Module);
		Builder.silent = true;
		asCScriptCode Code;
		Code.SetCode("ScriptNodeShapeFuncDef", "funcdef void FCallback(int Value);", true);
		FParserAccessor Parser(&Builder);
		ASSERT_THAT(IsTrue(Parser.ParseScript(&Code) < 0,
			TEXT("Script-level funcdef declaration should remain rejected in this native parser mode")));
	}

	TEST_METHOD(TypedefNodeShape)
	{
		asCScriptEngine* BareEngine = AngelscriptNativeTestSupport::CreateBareSdkEngine(&*TestRunner);
		ASSERT_THAT(IsNotNull(BareEngine, TEXT("ScriptNode shape test should create a bare engine")));
		ON_SCOPE_EXIT { BareEngine->ShutDownAndRelease(); };

		asCModule* Module = CreateSdkModule(BareEngine, "ScriptNodeShapeTypedef");
		asCBuilder Builder(BareEngine, Module);
		Builder.silent = true;
		asCScriptCode Code;
		Code.SetCode("ScriptNodeShapeTypedef", "typedef int32 FScore;", true);
		FParserAccessor Parser(&Builder);
		ASSERT_THAT(IsTrue(Parser.ParseScript(&Code) < 0,
			TEXT("Script-level typedef declaration should remain rejected in this native parser mode")));
	}

	TEST_METHOD(VirtualPropertyNodeShape)
	{
		asCScriptEngine* BareEngine = AngelscriptNativeTestSupport::CreateBareSdkEngine(&*TestRunner);
		ASSERT_THAT(IsNotNull(BareEngine, TEXT("ScriptNode shape test should create a bare engine")));
		ON_SCOPE_EXIT { BareEngine->ShutDownAndRelease(); };

		// Virtual-property syntax `int X { get { ... } set { } }` was removed by the
		// autoaccessor refactor (see openspec/changes/archive/2026-05-22-refactor-as-remove-autoaccessor).
		// The parser now rejects this form, so no virtual-property script-node shape exists to inspect.
		asCModule* Module = CreateSdkModule(BareEngine, "ScriptNodeShapeVirtualProperty");
		asCBuilder Builder(BareEngine, Module);
		Builder.silent = true;
		asCScriptCode Code;
		Code.SetCode("ScriptNodeShapeVirtualProperty", "int Value { get { return 1; } set { } }", true);
		FParserAccessor Parser(&Builder);
		ASSERT_THAT(IsTrue(Parser.ParseScript(&Code) < 0,
			TEXT("Virtual property declaration should be rejected after autoaccessor removal")));
	}
};

#endif
