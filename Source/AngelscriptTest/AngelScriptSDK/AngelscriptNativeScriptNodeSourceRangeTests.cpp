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
	};

	asCModule* CreateModule(asCScriptEngine* ScriptEngine, const char* ModuleName)
	{
		return static_cast<asCModule*>(ScriptEngine->GetModule(ModuleName, asGM_ALWAYS_CREATE));
	}

	const asCScriptNode* FindFirstNodeOfType(const asCScriptNode* Node, const eScriptNode Type)
	{
		for (const asCScriptNode* Current = Node; Current != nullptr; Current = Current->next)
		{
			if (Current->nodeType == Type)
			{
				return Current;
			}

			if (const asCScriptNode* Child = FindFirstNodeOfType(Current->firstChild, Type))
			{
				return Child;
			}
		}

		return nullptr;
	}

	bool ParseScript(FAutomationTestBase& Test, asCScriptEngine* ScriptEngine, const char* ModuleName, const char* Source, TFunctionRef<void(asCScriptCode&, const asCScriptNode&)> Verify)
	{
		asCModule* Module = CreateModule(ScriptEngine, ModuleName);
		if (!Test.TestNotNull(FString::Printf(TEXT("%s should create a source-range module"), UTF8_TO_TCHAR(ModuleName)), Module))
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

		Verify(Code, *Root);
		return true;
	}

	FIntPoint RowColFor(asCScriptCode& Code, const asCScriptNode& Node)
	{
		int Row = 0;
		int Column = 0;
		Code.ConvertPosToRowCol(Node.tokenPos, &Row, &Column);
		return FIntPoint(Row, Column);
	}
}

TEST_CLASS_WITH_FLAGS(FAngelscriptNativeScriptNodeSourceRangeTests,
	"Angelscript.TestModule.AngelScriptSDK.ScriptNode.SourceRange",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(LineColPropagatedToFunction)
	{
		asCScriptEngine* BareEngine = AngelscriptNativeTestSupport::CreateBareSdkEngine(&*TestRunner);
		if (!TestRunner->TestNotNull(TEXT("ScriptNode source-range test should create a bare engine"), BareEngine))
		{
			return;
		}
		ON_SCOPE_EXIT { BareEngine->ShutDownAndRelease(); };

		ParseScript(*TestRunner, BareEngine, "ScriptNodeRangeFunction", "\n\nint Read() { return 7; }", [&](asCScriptCode& Code, const asCScriptNode& Root)
		{
			const asCScriptNode* FunctionNode = FindFirstNodeOfType(&Root, snFunction);
			if (!TestRunner->TestNotNull(TEXT("Function source-range test should find a function node"), FunctionNode))
			{
				return;
			}

			const FIntPoint RowCol = RowColFor(Code, *FunctionNode);
			TestRunner->TestEqual(TEXT("Function node should start on the third source line"), RowCol.X, 3);
			TestRunner->TestEqual(TEXT("Function node should start at the first source column"), RowCol.Y, 1);
			TestRunner->TestTrue(TEXT("Function node source range should cover the function body"), FunctionNode->tokenLength >= std::strlen("int Read() { return 7; }"));
		});
	}

	TEST_METHOD(LineColPropagatedToClassMember)
	{
		asCScriptEngine* BareEngine = AngelscriptNativeTestSupport::CreateBareSdkEngine(&*TestRunner);
		if (!TestRunner->TestNotNull(TEXT("ScriptNode source-range test should create a bare engine"), BareEngine))
		{
			return;
		}
		ON_SCOPE_EXIT { BareEngine->ShutDownAndRelease(); };

		ParseScript(*TestRunner, BareEngine, "ScriptNodeRangeClassMember", "class FRange\n{\n\tint Value;\n}", [&](asCScriptCode& Code, const asCScriptNode& Root)
		{
			const asCScriptNode* DeclarationNode = FindFirstNodeOfType(&Root, snDeclaration);
			if (!TestRunner->TestNotNull(TEXT("Class-member source-range test should find a declaration node"), DeclarationNode))
			{
				return;
			}

			const FIntPoint RowCol = RowColFor(Code, *DeclarationNode);
			TestRunner->TestEqual(TEXT("Class member declaration should start on the third source line"), RowCol.X, 3);
			TestRunner->TestEqual(TEXT("Class member declaration should start after the tab indentation"), RowCol.Y, 2);
		});
	}

	TEST_METHOD(MultilineStatementSpansCorrectRange)
	{
		asCScriptEngine* BareEngine = AngelscriptNativeTestSupport::CreateBareSdkEngine(&*TestRunner);
		if (!TestRunner->TestNotNull(TEXT("ScriptNode source-range test should create a bare engine"), BareEngine))
		{
			return;
		}
		ON_SCOPE_EXIT { BareEngine->ShutDownAndRelease(); };

		ParseScript(*TestRunner, BareEngine, "ScriptNodeRangeMultiline", "int Value =\n\t1 +\n\t2;", [&](asCScriptCode& Code, const asCScriptNode& Root)
		{
			const asCScriptNode* DeclarationNode = FindFirstNodeOfType(&Root, snDeclaration);
			if (!TestRunner->TestNotNull(TEXT("Multiline source-range test should find a declaration node"), DeclarationNode))
			{
				return;
			}

			const FIntPoint Start = RowColFor(Code, *DeclarationNode);
			int EndRow = 0;
			int EndColumn = 0;
			Code.ConvertPosToRowCol(DeclarationNode->tokenPos + DeclarationNode->tokenLength - 1, &EndRow, &EndColumn);
			TestRunner->TestEqual(TEXT("Multiline declaration should start on the first line"), Start.X, 1);
			TestRunner->TestTrue(TEXT("Multiline declaration source range should reach the third line"), EndRow >= 3);
			TestRunner->TestTrue(TEXT("Multiline declaration source range should include more than the identifier"), DeclarationNode->tokenLength > std::strlen("Value"));
		});
	}

	TEST_METHOD(CommentSkippedDoesNotShiftNextNodeLine)
	{
		asCScriptEngine* BareEngine = AngelscriptNativeTestSupport::CreateBareSdkEngine(&*TestRunner);
		if (!TestRunner->TestNotNull(TEXT("ScriptNode source-range test should create a bare engine"), BareEngine))
		{
			return;
		}
		ON_SCOPE_EXIT { BareEngine->ShutDownAndRelease(); };

		ParseScript(*TestRunner, BareEngine, "ScriptNodeRangeComment", "// skipped comment\nint Value = 1;", [&](asCScriptCode& Code, const asCScriptNode& Root)
		{
			const asCScriptNode* DeclarationNode = FindFirstNodeOfType(&Root, snDeclaration);
			if (!TestRunner->TestNotNull(TEXT("Comment source-range test should find a declaration node"), DeclarationNode))
			{
				return;
			}

			const FIntPoint RowCol = RowColFor(Code, *DeclarationNode);
			TestRunner->TestEqual(TEXT("Declaration after a line comment should start on the second line"), RowCol.X, 2);
			TestRunner->TestEqual(TEXT("Declaration after a line comment should start at column one"), RowCol.Y, 1);
		});
	}

	TEST_METHOD(BomDoesNotPoisonFirstNodeColumn)
	{
		asCScriptEngine* BareEngine = AngelscriptNativeTestSupport::CreateBareSdkEngine(&*TestRunner);
		if (!TestRunner->TestNotNull(TEXT("ScriptNode source-range test should create a bare engine"), BareEngine))
		{
			return;
		}
		ON_SCOPE_EXIT { BareEngine->ShutDownAndRelease(); };

		const char SourceWithBom[] = "\xEF\xBB\xBFint Value = 1;";
		ParseScript(*TestRunner, BareEngine, "ScriptNodeRangeBom", SourceWithBom, [&](asCScriptCode& Code, const asCScriptNode& Root)
		{
			const asCScriptNode* DeclarationNode = FindFirstNodeOfType(&Root, snDeclaration);
			if (!TestRunner->TestNotNull(TEXT("BOM source-range test should find a declaration node"), DeclarationNode))
			{
				return;
			}

			const FIntPoint RowCol = RowColFor(Code, *DeclarationNode);
			TestRunner->TestEqual(TEXT("Declaration after a UTF-8 BOM should remain on the first line"), RowCol.X, 1);
			TestRunner->TestEqual(TEXT("Declaration after a UTF-8 BOM should start after the three BOM bytes"), RowCol.Y, 4);
		});
	}
};

#endif
