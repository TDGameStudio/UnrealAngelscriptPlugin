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

namespace
{
	struct FParserAccessor : asCParser
	{
		explicit FParserAccessor(asCBuilder* Builder)
			: asCParser(Builder)
		{
		}

		asCScriptNode* ParseStatementSnippet(asCScriptCode* InScript)
		{
			Reset();
			script = InScript;
			return ParseStatement();
		}
	};

	asCModule* CreateModule(asCScriptEngine* ScriptEngine, const char* ModuleName)
	{
		return static_cast<asCModule*>(ScriptEngine->GetModule(ModuleName, asGM_ALWAYS_CREATE));
	}

	bool ParseScript(FAutomationTestBase& Test, asCScriptEngine* ScriptEngine, const char* ModuleName, const char* Source, TFunctionRef<void(const asCScriptNode&)> Verify)
	{
		asCModule* Module = CreateModule(ScriptEngine, ModuleName);
		if (!Test.TestNotNull(FString::Printf(TEXT("%s should create a script-node module"), UTF8_TO_TCHAR(ModuleName)), Module))
		{
			return false;
		}

		asCBuilder Builder(ScriptEngine, Module);
		asCScriptCode Code;
		Code.SetCode(ModuleName, Source, true);

		FParserAccessor Parser(&Builder);
		const int ParseResult = Parser.ParseScript(&Code);
		if (!Test.TestEqual(FString::Printf(TEXT("%s should parse successfully"), UTF8_TO_TCHAR(ModuleName)), ParseResult, 0))
		{
			return false;
		}

		const asCScriptNode* Root = Parser.GetScriptNode();
		if (!Test.TestNotNull(FString::Printf(TEXT("%s should produce a script root"), UTF8_TO_TCHAR(ModuleName)), Root))
		{
			return false;
		}

		Verify(*Root);
		return true;
	}

	bool ParseStatement(FAutomationTestBase& Test, asCScriptEngine* ScriptEngine, const char* ModuleName, const char* Source, TFunctionRef<void(const asCScriptNode&)> Verify)
	{
		asCModule* Module = CreateModule(ScriptEngine, ModuleName);
		if (!Test.TestNotNull(FString::Printf(TEXT("%s should create a statement parser module"), UTF8_TO_TCHAR(ModuleName)), Module))
		{
			return false;
		}

		asCBuilder Builder(ScriptEngine, Module);
		asCScriptCode Code;
		Code.SetCode(ModuleName, Source, true);

		FParserAccessor Parser(&Builder);
		const asCScriptNode* Root = Parser.ParseStatementSnippet(&Code);
		if (!Test.TestNotNull(FString::Printf(TEXT("%s should parse a statement root"), UTF8_TO_TCHAR(ModuleName)), Root))
		{
			return false;
		}

		Verify(*Root);
		return true;
	}

	int32 CountDirectChildren(const asCScriptNode* Node)
	{
		int32 Count = 0;
		for (const asCScriptNode* Child = Node != nullptr ? Node->firstChild : nullptr; Child != nullptr; Child = Child->next)
		{
			++Count;
		}
		return Count;
	}
}

TEST_CLASS_WITH_FLAGS(FAngelscriptNativeScriptNodeShapeTests,
	"Angelscript.TestModule.AngelScriptSDK.ScriptNode.Shape",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(FunctionNodeChildrenLayout)
	{
		asCScriptEngine* BareEngine = AngelscriptNativeTestSupport::CreateBareSdkEngine(&*TestRunner);
		if (!TestRunner->TestNotNull(TEXT("ScriptNode shape test should create a bare engine"), BareEngine))
		{
			return;
		}
		ON_SCOPE_EXIT { BareEngine->ShutDownAndRelease(); };

		ParseScript(*TestRunner, BareEngine, "ScriptNodeShapeFunction", "int Add(int A, int B) { return A + B; }", [&](const asCScriptNode& Root)
		{
			TestRunner->TestEqual(TEXT("Function script should produce one function node"), AngelscriptNativeTestSupport::CountNodesOfType(&Root, snFunction), 1);
			TestRunner->TestTrue(TEXT("Function node should carry return type, identifier, parameters, and block children"), CountDirectChildren(Root.firstChild) >= 4);
		});
	}

	TEST_METHOD(ParameterListNodeShape)
	{
		asCScriptEngine* BareEngine = AngelscriptNativeTestSupport::CreateBareSdkEngine(&*TestRunner);
		if (!TestRunner->TestNotNull(TEXT("ScriptNode shape test should create a bare engine"), BareEngine))
		{
			return;
		}
		ON_SCOPE_EXIT { BareEngine->ShutDownAndRelease(); };

		ParseScript(*TestRunner, BareEngine, "ScriptNodeShapeParameters", "void Visit(int A, float B, const string& in Name) { }", [&](const asCScriptNode& Root)
		{
			TestRunner->TestEqual(TEXT("Parameter list should be represented once"), AngelscriptNativeTestSupport::CountNodesOfType(&Root, snParameterList), 1);
			TestRunner->TestTrue(TEXT("Parameter list should include data type and identifier nodes"), AngelscriptNativeTestSupport::CountNodesOfType(&Root, snDataType) >= 4);
		});
	}

	TEST_METHOD(StatementBlockHoldsStatements)
	{
		asCScriptEngine* BareEngine = AngelscriptNativeTestSupport::CreateBareSdkEngine(&*TestRunner);
		if (!TestRunner->TestNotNull(TEXT("ScriptNode shape test should create a bare engine"), BareEngine))
		{
			return;
		}
		ON_SCOPE_EXIT { BareEngine->ShutDownAndRelease(); };

		ParseStatement(*TestRunner, BareEngine, "ScriptNodeShapeStatementBlock", "{ int A = 1; A += 2; return; }", [&](const asCScriptNode& Root)
		{
			TestRunner->TestEqual(TEXT("Statement block should parse to a statement-block root"), static_cast<int32>(Root.nodeType), static_cast<int32>(snStatementBlock));
			TestRunner->TestEqual(TEXT("Function body should produce one statement block"), AngelscriptNativeTestSupport::CountNodesOfType(&Root, snStatementBlock), 1);
			TestRunner->TestEqual(TEXT("Statement block should retain all direct child statements"), CountDirectChildren(&Root), 3);
			TestRunner->TestEqual(TEXT("Statement block should still contain the return statement"), AngelscriptNativeTestSupport::CountNodesOfType(&Root, snReturn), 1);
		});
	}

	TEST_METHOD(ReturnNodeHasOptionalExpression)
	{
		asCScriptEngine* BareEngine = AngelscriptNativeTestSupport::CreateBareSdkEngine(&*TestRunner);
		if (!TestRunner->TestNotNull(TEXT("ScriptNode shape test should create a bare engine"), BareEngine))
		{
			return;
		}
		ON_SCOPE_EXIT { BareEngine->ShutDownAndRelease(); };

		ParseStatement(*TestRunner, BareEngine, "ScriptNodeShapeReturn", "return 42;", [&](const asCScriptNode& Root)
		{
			TestRunner->TestEqual(TEXT("Return statement should parse to a return root node"), static_cast<int32>(Root.nodeType), static_cast<int32>(snReturn));
			TestRunner->TestTrue(TEXT("Value-return statement should include an expression child"), AngelscriptNativeTestSupport::CountNodesOfType(&Root, snExpression) >= 1);
		});
	}

	TEST_METHOD(BreakAndContinueAreLeafNodes)
	{
		asCScriptEngine* BareEngine = AngelscriptNativeTestSupport::CreateBareSdkEngine(&*TestRunner);
		if (!TestRunner->TestNotNull(TEXT("ScriptNode shape test should create a bare engine"), BareEngine))
		{
			return;
		}
		ON_SCOPE_EXIT { BareEngine->ShutDownAndRelease(); };

		ParseStatement(*TestRunner, BareEngine, "ScriptNodeShapeBreak", "break;", [&](const asCScriptNode& Root)
		{
			TestRunner->TestEqual(TEXT("Break statement should parse to a break root node"), static_cast<int32>(Root.nodeType), static_cast<int32>(snBreak));
			TestRunner->TestEqual(TEXT("Break statement should remain a leaf node"), CountDirectChildren(&Root), 0);
		});

		ParseStatement(*TestRunner, BareEngine, "ScriptNodeShapeContinue", "continue;", [&](const asCScriptNode& Root)
		{
			TestRunner->TestEqual(TEXT("Continue statement should parse to a continue root node"), static_cast<int32>(Root.nodeType), static_cast<int32>(snContinue));
			TestRunner->TestEqual(TEXT("Continue statement should remain a leaf node"), CountDirectChildren(&Root), 0);
		});
	}

	TEST_METHOD(DoWhileShape)
	{
		asCScriptEngine* BareEngine = AngelscriptNativeTestSupport::CreateBareSdkEngine(&*TestRunner);
		if (!TestRunner->TestNotNull(TEXT("ScriptNode shape test should create a bare engine"), BareEngine))
		{
			return;
		}
		ON_SCOPE_EXIT { BareEngine->ShutDownAndRelease(); };

		ParseStatement(*TestRunner, BareEngine, "ScriptNodeShapeDoWhile", "do { continue; } while (true);", [&](const asCScriptNode& Root)
		{
			TestRunner->TestEqual(TEXT("Do/while statement root should be a do-while node"), static_cast<int32>(Root.nodeType), static_cast<int32>(snDoWhile));
			TestRunner->TestEqual(TEXT("Do/while statement should contain a continue node"), AngelscriptNativeTestSupport::CountNodesOfType(&Root, snContinue), 1);
			TestRunner->TestTrue(TEXT("Do/while statement should include a condition subtree"), AngelscriptNativeTestSupport::CountNodesOfType(&Root, snCondition) >= 1);
		});
	}

	TEST_METHOD(SwitchAndCaseShape)
	{
		asCScriptEngine* BareEngine = AngelscriptNativeTestSupport::CreateBareSdkEngine(&*TestRunner);
		if (!TestRunner->TestNotNull(TEXT("ScriptNode shape test should create a bare engine"), BareEngine))
		{
			return;
		}
		ON_SCOPE_EXIT { BareEngine->ShutDownAndRelease(); };

		ParseStatement(*TestRunner, BareEngine, "ScriptNodeShapeSwitch", "switch (Value) { case 1: break; default: break; }", [&](const asCScriptNode& Root)
		{
			TestRunner->TestEqual(TEXT("Switch statement root should be a switch node"), static_cast<int32>(Root.nodeType), static_cast<int32>(snSwitch));
			TestRunner->TestEqual(TEXT("Switch should carry two case/default nodes"), AngelscriptNativeTestSupport::CountNodesOfType(&Root, snCase), 2);
			TestRunner->TestEqual(TEXT("Switch cases should each include a break node"), AngelscriptNativeTestSupport::CountNodesOfType(&Root, snBreak), 2);
		});
	}

	TEST_METHOD(EnumNodeAndEnumValueChildren)
	{
		asCScriptEngine* BareEngine = AngelscriptNativeTestSupport::CreateBareSdkEngine(&*TestRunner);
		if (!TestRunner->TestNotNull(TEXT("ScriptNode shape test should create a bare engine"), BareEngine))
		{
			return;
		}
		ON_SCOPE_EXIT { BareEngine->ShutDownAndRelease(); };

		ParseScript(*TestRunner, BareEngine, "ScriptNodeShapeEnum", "enum EMode { Idle = 0, Run = 1, Jump }", [&](const asCScriptNode& Root)
		{
			TestRunner->TestEqual(TEXT("Enum declaration should produce one enum node"), AngelscriptNativeTestSupport::CountNodesOfType(&Root, snEnum), 1);
			TestRunner->TestTrue(TEXT("Enum declaration should keep enum and value identifiers"), AngelscriptNativeTestSupport::CountNodesOfType(&Root, snIdentifier) >= 4);
		});
	}

	TEST_METHOD(InterfaceNodeShape)
	{
		asCScriptEngine* BareEngine = AngelscriptNativeTestSupport::CreateBareSdkEngine(&*TestRunner);
		if (!TestRunner->TestNotNull(TEXT("ScriptNode shape test should create a bare engine"), BareEngine))
		{
			return;
		}
		ON_SCOPE_EXIT { BareEngine->ShutDownAndRelease(); };

		asCModule* Module = CreateModule(BareEngine, "ScriptNodeShapeInterface");
		asCBuilder Builder(BareEngine, Module);
		Builder.silent = true;
		asCScriptCode Code;
		Code.SetCode("ScriptNodeShapeInterface", "interface IThing { void Run(); }", true);
		FParserAccessor Parser(&Builder);
		TestRunner->TestTrue(TEXT("Script-level interface declaration should remain rejected in this native parser mode"), Parser.ParseScript(&Code) < 0);
	}

	TEST_METHOD(ImportNodeShape)
	{
		asCScriptEngine* BareEngine = AngelscriptNativeTestSupport::CreateBareSdkEngine(&*TestRunner);
		if (!TestRunner->TestNotNull(TEXT("ScriptNode shape test should create a bare engine"), BareEngine))
		{
			return;
		}
		ON_SCOPE_EXIT { BareEngine->ShutDownAndRelease(); };

		ParseScript(*TestRunner, BareEngine, "ScriptNodeShapeImport", "import int SharedValue() from \"OtherModule\";", [&](const asCScriptNode& Root)
		{
			TestRunner->TestEqual(TEXT("Import declaration should produce one import node"), AngelscriptNativeTestSupport::CountNodesOfType(&Root, snImport), 1);
			TestRunner->TestTrue(TEXT("Import declaration should carry a function-definition subtree"), AngelscriptNativeTestSupport::CountNodesOfType(&Root, snDataType) >= 1 && AngelscriptNativeTestSupport::CountNodesOfType(&Root, snParameterList) >= 1);
		});
	}

	TEST_METHOD(FuncDefNodeShape)
	{
		asCScriptEngine* BareEngine = AngelscriptNativeTestSupport::CreateBareSdkEngine(&*TestRunner);
		if (!TestRunner->TestNotNull(TEXT("ScriptNode shape test should create a bare engine"), BareEngine))
		{
			return;
		}
		ON_SCOPE_EXIT { BareEngine->ShutDownAndRelease(); };

		asCModule* Module = CreateModule(BareEngine, "ScriptNodeShapeFuncDef");
		asCBuilder Builder(BareEngine, Module);
		Builder.silent = true;
		asCScriptCode Code;
		Code.SetCode("ScriptNodeShapeFuncDef", "funcdef void FCallback(int Value);", true);
		FParserAccessor Parser(&Builder);
		TestRunner->TestTrue(TEXT("Script-level funcdef declaration should remain rejected in this native parser mode"), Parser.ParseScript(&Code) < 0);
	}

	TEST_METHOD(TypedefNodeShape)
	{
		asCScriptEngine* BareEngine = AngelscriptNativeTestSupport::CreateBareSdkEngine(&*TestRunner);
		if (!TestRunner->TestNotNull(TEXT("ScriptNode shape test should create a bare engine"), BareEngine))
		{
			return;
		}
		ON_SCOPE_EXIT { BareEngine->ShutDownAndRelease(); };

		asCModule* Module = CreateModule(BareEngine, "ScriptNodeShapeTypedef");
		asCBuilder Builder(BareEngine, Module);
		Builder.silent = true;
		asCScriptCode Code;
		Code.SetCode("ScriptNodeShapeTypedef", "typedef int32 FScore;", true);
		FParserAccessor Parser(&Builder);
		TestRunner->TestTrue(TEXT("Script-level typedef declaration should remain rejected in this native parser mode"), Parser.ParseScript(&Code) < 0);
	}

	TEST_METHOD(VirtualPropertyNodeShape)
	{
		asCScriptEngine* BareEngine = AngelscriptNativeTestSupport::CreateBareSdkEngine(&*TestRunner);
		if (!TestRunner->TestNotNull(TEXT("ScriptNode shape test should create a bare engine"), BareEngine))
		{
			return;
		}
		ON_SCOPE_EXIT { BareEngine->ShutDownAndRelease(); };

		// Virtual-property syntax `int X { get { ... } set { } }` was removed by the
		// autoaccessor refactor (see openspec/changes/archive/2026-05-22-refactor-as-remove-autoaccessor).
		// The parser now rejects this form, so no virtual-property script-node shape exists to inspect.
		asCModule* Module = CreateModule(BareEngine, "ScriptNodeShapeVirtualProperty");
		asCBuilder Builder(BareEngine, Module);
		Builder.silent = true;
		asCScriptCode Code;
		Code.SetCode("ScriptNodeShapeVirtualProperty", "int Value { get { return 1; } set { } }", true);
		FParserAccessor Parser(&Builder);
		TestRunner->TestTrue(TEXT("Virtual property declaration should be rejected after autoaccessor removal"), Parser.ParseScript(&Code) < 0);
	}
};

#endif
