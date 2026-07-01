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

#if WITH_ANGELSCRIPT_UNITTESTS


TEST_CLASS_WITH_FLAGS(FAngelscriptNativeScriptNodeSourceRangeTests,
	"Angelscript.TestModule.AngelScriptSDK.ScriptNode.SourceRange",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	static bool ParseRangeScript(FAutomationTestBase& Test, FNoDiscardAsserter& Assert, asCScriptEngine* ScriptEngine, const char* ModuleName, const char* Source, TFunctionRef<void(asCScriptCode&, const asCScriptNode&)> Verify)
	{
		using namespace AngelscriptNativeTestSupport;

		asCModule* Module = CreateSdkModule(ScriptEngine, ModuleName);
		if (!Assert.IsNotNull(Module, FString::Printf(TEXT("%s should create a source-range module"), UTF8_TO_TCHAR(ModuleName))))
		{
			return false;
		}

		asCBuilder Builder(ScriptEngine, Module);
		asCScriptCode Code;
		Code.SetCode(ModuleName, Source, true);

		AngelscriptNativeTestSupport::FParserAccessor Parser(&Builder);
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

		Verify(Code, *Root);
		return true;
	}

	static FIntPoint RowColFor(asCScriptCode& Code, const asCScriptNode& Node)
	{
		int Row = 0;
		int Column = 0;
		Code.ConvertPosToRowCol(Node.tokenPos, &Row, &Column);
		return FIntPoint(Row, Column);
	}

public:
	TEST_METHOD(LineColPropagatedToFunction)
	{
		using namespace AngelscriptNativeTestSupport;

		asCScriptEngine* BareEngine = AngelscriptNativeTestSupport::CreateBareSdkEngine(&*TestRunner);
		ASSERT_THAT(IsNotNull(BareEngine, TEXT("ScriptNode source-range test should create a bare engine")));
		ON_SCOPE_EXIT { BareEngine->ShutDownAndRelease(); };

		ParseRangeScript(*TestRunner, this->Assert, BareEngine, "ScriptNodeRangeFunction", "\n\nint Read() { return 7; }", [&](asCScriptCode& Code, const asCScriptNode& Root)
		{
			const asCScriptNode* FunctionNode = FindFirstNodeOfType(&Root, snFunction);
			if (!this->Assert.IsNotNull(FunctionNode, TEXT("Function source-range test should find a function node")))
			{
				return;
			}

			const FIntPoint RowCol = RowColFor(Code, *FunctionNode);
			if (!this->Assert.AreEqual(3, RowCol.X,
				TEXT("Function node should start on the third source line")))
			{
				return;
			}
			if (!this->Assert.AreEqual(1, RowCol.Y,
				TEXT("Function node should start at the first source column")))
			{
				return;
			}
			if (!this->Assert.IsTrue(FunctionNode->tokenLength >= std::strlen("int Read() { return 7; }"),
				TEXT("Function node source range should cover the function body")))
			{
				return;
			}
		});
	}

	TEST_METHOD(LineColPropagatedToClassMember)
	{
		using namespace AngelscriptNativeTestSupport;

		asCScriptEngine* BareEngine = AngelscriptNativeTestSupport::CreateBareSdkEngine(&*TestRunner);
		ASSERT_THAT(IsNotNull(BareEngine, TEXT("ScriptNode source-range test should create a bare engine")));
		ON_SCOPE_EXIT { BareEngine->ShutDownAndRelease(); };

		ParseRangeScript(*TestRunner, this->Assert, BareEngine, "ScriptNodeRangeClassMember", "class FRange\n{\n\tint Value;\n}", [&](asCScriptCode& Code, const asCScriptNode& Root)
		{
			const asCScriptNode* DeclarationNode = FindFirstNodeOfType(&Root, snDeclaration);
			if (!this->Assert.IsNotNull(DeclarationNode, TEXT("Class-member source-range test should find a declaration node")))
			{
				return;
			}

			const FIntPoint RowCol = RowColFor(Code, *DeclarationNode);
			if (!this->Assert.AreEqual(3, RowCol.X,
				TEXT("Class member declaration should start on the third source line")))
			{
				return;
			}
			if (!this->Assert.AreEqual(2, RowCol.Y,
				TEXT("Class member declaration should start after the tab indentation")))
			{
				return;
			}
		});
	}

	TEST_METHOD(MultilineStatementSpansCorrectRange)
	{
		using namespace AngelscriptNativeTestSupport;

		asCScriptEngine* BareEngine = AngelscriptNativeTestSupport::CreateBareSdkEngine(&*TestRunner);
		ASSERT_THAT(IsNotNull(BareEngine, TEXT("ScriptNode source-range test should create a bare engine")));
		ON_SCOPE_EXIT { BareEngine->ShutDownAndRelease(); };

		ParseRangeScript(*TestRunner, this->Assert, BareEngine, "ScriptNodeRangeMultiline", "int Value =\n\t1 +\n\t2;", [&](asCScriptCode& Code, const asCScriptNode& Root)
		{
			const asCScriptNode* DeclarationNode = FindFirstNodeOfType(&Root, snDeclaration);
			if (!this->Assert.IsNotNull(DeclarationNode, TEXT("Multiline source-range test should find a declaration node")))
			{
				return;
			}

			const FIntPoint Start = RowColFor(Code, *DeclarationNode);
			int EndRow = 0;
			int EndColumn = 0;
			Code.ConvertPosToRowCol(DeclarationNode->tokenPos + DeclarationNode->tokenLength - 1, &EndRow, &EndColumn);
			if (!this->Assert.AreEqual(1, Start.X,
				TEXT("Multiline declaration should start on the first line")))
			{
				return;
			}
			if (!this->Assert.IsTrue(EndRow >= 3,
				TEXT("Multiline declaration source range should reach the third line")))
			{
				return;
			}
			if (!this->Assert.IsTrue(DeclarationNode->tokenLength > std::strlen("Value"),
				TEXT("Multiline declaration source range should include more than the identifier")))
			{
				return;
			}
		});
	}

	TEST_METHOD(CommentSkippedDoesNotShiftNextNodeLine)
	{
		using namespace AngelscriptNativeTestSupport;

		asCScriptEngine* BareEngine = AngelscriptNativeTestSupport::CreateBareSdkEngine(&*TestRunner);
		ASSERT_THAT(IsNotNull(BareEngine, TEXT("ScriptNode source-range test should create a bare engine")));
		ON_SCOPE_EXIT { BareEngine->ShutDownAndRelease(); };

		ParseRangeScript(*TestRunner, this->Assert, BareEngine, "ScriptNodeRangeComment", "// skipped comment\nint Value = 1;", [&](asCScriptCode& Code, const asCScriptNode& Root)
		{
			const asCScriptNode* DeclarationNode = FindFirstNodeOfType(&Root, snDeclaration);
			if (!this->Assert.IsNotNull(DeclarationNode, TEXT("Comment source-range test should find a declaration node")))
			{
				return;
			}

			const FIntPoint RowCol = RowColFor(Code, *DeclarationNode);
			if (!this->Assert.AreEqual(2, RowCol.X,
				TEXT("Declaration after a line comment should start on the second line")))
			{
				return;
			}
			if (!this->Assert.AreEqual(1, RowCol.Y,
				TEXT("Declaration after a line comment should start at column one")))
			{
				return;
			}
		});
	}

	TEST_METHOD(BomDoesNotPoisonFirstNodeColumn)
	{
		using namespace AngelscriptNativeTestSupport;

		asCScriptEngine* BareEngine = AngelscriptNativeTestSupport::CreateBareSdkEngine(&*TestRunner);
		ASSERT_THAT(IsNotNull(BareEngine, TEXT("ScriptNode source-range test should create a bare engine")));
		ON_SCOPE_EXIT { BareEngine->ShutDownAndRelease(); };

		const char SourceWithBom[] = "\xEF\xBB\xBFint Value = 1;";
		ParseRangeScript(*TestRunner, this->Assert, BareEngine, "ScriptNodeRangeBom", SourceWithBom, [&](asCScriptCode& Code, const asCScriptNode& Root)
		{
			const asCScriptNode* DeclarationNode = FindFirstNodeOfType(&Root, snDeclaration);
			if (!this->Assert.IsNotNull(DeclarationNode, TEXT("BOM source-range test should find a declaration node")))
			{
				return;
			}

			const FIntPoint RowCol = RowColFor(Code, *DeclarationNode);
			if (!this->Assert.AreEqual(1, RowCol.X,
				TEXT("Declaration after a UTF-8 BOM should remain on the first line")))
			{
				return;
			}
			if (!this->Assert.AreEqual(4, RowCol.Y,
				TEXT("Declaration after a UTF-8 BOM should start after the three BOM bytes")))
			{
				return;
			}
		});
	}
};

#endif
