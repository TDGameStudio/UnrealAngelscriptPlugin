#include "../Support/AngelscriptNativeCoreTestSupport.h"
#include "../Support/AngelscriptNativeCaseTestSupport.h"
#include "../Support/AngelscriptNativeLanguageCaseTestSupport.h"

// Script-node source-range coverage.
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


TEST_CLASS_WITH_FLAGS(FScriptNodeSourceRangeTests,
	"Angelscript.TestModule.AngelScriptSDK.Frontend.ScriptNode.SourceRange",
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

		bool bVerified = false;
		{
			asCBuilder Builder(ScriptEngine, Module);
			asCScriptCode Code;
			Code.SetCode(ModuleName, Source, true);

			AngelscriptNativeTestSupport::FParserAccessor Parser(&Builder);
			const int ParseResult = Parser.ParseScript(&Code);
			if (Assert.AreEqual(
				0,
				ParseResult,
				FString::Printf(TEXT("%s should parse successfully"), UTF8_TO_TCHAR(ModuleName))))
			{
				const asCScriptNode* Root = Parser.GetScriptNode();
				if (Assert.IsNotNull(
					Root,
					FString::Printf(TEXT("%s should produce a script root"), UTF8_TO_TCHAR(ModuleName))))
				{
					Verify(Code, *Root);
					bVerified = true;
				}
			}
		}

		const bool bDiscarded = Assert.AreEqual(
			asSUCCESS,
			ScriptEngine->DiscardModule(ModuleName),
			FString::Printf(TEXT("%s should discard after its parser tree is released"), UTF8_TO_TCHAR(ModuleName)));
		const bool bLookupCleared = Assert.IsNull(
			ScriptEngine->GetModule(ModuleName, asGM_ONLY_IF_EXISTS),
			FString::Printf(TEXT("%s should leave no name-visible source-range module"), UTF8_TO_TCHAR(ModuleName)));
		return bVerified && bDiscarded && bLookupCleared;
	}

	static FIntPoint RowColFor(asCScriptCode& Code, const asCScriptNode& Node)
	{
		int Row = 0;
		int Column = 0;
		Code.ConvertPosToRowCol(Node.tokenPos, &Row, &Column);
		return FIntPoint(Row, Column);
	}

public:
	TEST_METHOD(SourceRangesByShapeAndLineEnding)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT("FRONTEND-NODE-SOURCE-RANGE-LINE-ENDINGS",
			ENativeEvidence::Compile
			| ENativeEvidence::Metadata
			| ENativeEvidence::Cleanup
			| ENativeEvidence::Isolation);

		struct FRangeCase
		{
			const TCHAR* Id;
			eScriptNode NodeType;
			int32 ExpectedRow;
			int32 ExpectedColumn;
			int32 MinimumEndRow;
		};
		const FRangeCase Cases[] =
		{
			{
				TEXT("leading_lines_function"),
				snFunction,
				3,
				1,
				3,
			},
			{
				TEXT("indented_class_member"),
				snDeclaration,
				3,
				2,
				3,
			},
			{
				TEXT("multiline_declaration"),
				snDeclaration,
				1,
				1,
				3,
			},
			{
				TEXT("comment_then_declaration"),
				snDeclaration,
				2,
				1,
				2,
			},
			{
				TEXT("utf8_bom_declaration"),
				snDeclaration,
				1,
				4,
				1,
			},
		};
		const TCHAR* LineEndingIds[] =
		{
			TEXT("lf"),
			TEXT("crlf"),
		};
		const FString LineFeed = FString::Chr(10);
		const FString CarriageReturnLineFeed = FString::Chr(13) + FString::Chr(10);

		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};

		asCScriptEngine* ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsNotNull(ScriptEngine,
			TEXT("Source-range product should use the class-owned raw SDK engine")));
		if (ScriptEngine == nullptr)
		{
			return;
		}

		int32 ObservedCells = 0;
		for (int32 CaseIndex = 0; CaseIndex < UE_ARRAY_COUNT(Cases); ++CaseIndex)
		{
			const FRangeCase& Case = Cases[CaseIndex];
			FString BaseSource;
			switch (CaseIndex)
			{
			case 0:
				AppendGeneratedAsLine(BaseSource);
				AppendGeneratedAsLine(BaseSource);
				AppendGeneratedAsLine(BaseSource, TEXT("int Read()"));
				AppendGeneratedAsLine(BaseSource, TEXT("{"));
				AppendGeneratedAsLine(BaseSource, TEXT("\treturn 7;"));
				AppendGeneratedAsLine(BaseSource, TEXT("}"));
				break;
			case 1:
				AppendGeneratedAsLine(BaseSource, TEXT("class FRange"));
				AppendGeneratedAsLine(BaseSource, TEXT("{"));
				AppendGeneratedAsLine(BaseSource, TEXT("\tint Value;"));
				AppendGeneratedAsLine(BaseSource, TEXT("}"));
				break;
			case 2:
				AppendGeneratedAsLine(BaseSource, TEXT("int Value ="));
				AppendGeneratedAsLine(BaseSource, TEXT("\t1 +"));
				AppendGeneratedAsLine(BaseSource, TEXT("\t2;"));
				break;
			case 3:
				AppendGeneratedAsLine(BaseSource, TEXT("// skipped comment"));
				AppendGeneratedAsLine(BaseSource, TEXT("int Value = 1;"));
				break;
			default:
				AppendGeneratedAsLine(
					BaseSource,
					FString::Chr(0xFEFF) + TEXT("int Value = 1;"));
				break;
			}

			for (int32 LineEndingIndex = 0; LineEndingIndex < UE_ARRAY_COUNT(LineEndingIds); ++LineEndingIndex)
			{
				FString Source = BaseSource;
				if (LineEndingIndex == 1)
				{
					Source.ReplaceInline(*LineFeed, *CarriageReturnLineFeed);
				}

				const FString SourceId = FString::Printf(
					TEXT("FRONTEND-NODE-SOURCE-RANGE-LINE-ENDINGS-%s-%s"),
					Case.Id,
					LineEndingIds[LineEndingIndex]);
				const FString ModuleName = FString::Printf(
					TEXT("ScriptNodeRange_%s_%s"),
					Case.Id,
					LineEndingIds[LineEndingIndex]);
				PrintGeneratedAsSource(*TestRunner, SourceId, ModuleName, Source);

				const FTCHARToUTF8 ModuleNameUtf8(*ModuleName);
				const FTCHARToUTF8 SourceUtf8(*Source);
				const bool bParsed = ParseRangeScript(
					*TestRunner,
					this->Assert,
					ScriptEngine,
					ModuleNameUtf8.Get(),
					SourceUtf8.Get(),
					[&](asCScriptCode& Code, const asCScriptNode& Root)
					{
						const asCScriptNode* Node = FindFirstNodeOfType(&Root, Case.NodeType);
						ASSERT_THAT(IsNotNull(Node,
							FString::Printf(TEXT("%s should publish its selected node type"), *SourceId)));
						if (Node == nullptr)
						{
							return;
						}

						const FIntPoint Start = RowColFor(Code, *Node);
						ASSERT_THAT(AreEqual(Case.ExpectedRow, Start.X,
							FString::Printf(TEXT("%s should preserve the start row"), *SourceId)));
						ASSERT_THAT(AreEqual(Case.ExpectedColumn, Start.Y,
							FString::Printf(TEXT("%s should preserve the start column"), *SourceId)));
						ASSERT_THAT(IsTrue(Node->tokenLength > 0,
							FString::Printf(TEXT("%s should publish a non-empty source span"), *SourceId)));

						int EndRow = 0;
						int EndColumn = 0;
						Code.ConvertPosToRowCol(
							Node->tokenPos + Node->tokenLength - 1,
							&EndRow,
							&EndColumn);
						ASSERT_THAT(IsTrue(EndRow >= Case.MinimumEndRow,
							FString::Printf(TEXT("%s should reach its minimum end row"), *SourceId)));
						ASSERT_THAT(IsTrue(EndColumn >= 1,
							FString::Printf(TEXT("%s should publish a one-based end column"), *SourceId)));
					});
				ASSERT_THAT(IsTrue(bParsed,
					FString::Printf(TEXT("%s should complete parser source-range verification"), *SourceId)));
				++ObservedCells;
			}
		}

		ASSERT_THAT(AreEqual(10, ObservedCells,
			TEXT("Source-range product should execute every source-shape and line-ending cell")));

		const FString ControlSourceId = TEXT("FRONTEND-NODE-SOURCE-RANGE-LINE-ENDINGS-isolation-control");
		const FString ControlModuleName = TEXT("ScriptNodeRangeIsolationControl");
		const FString ControlSource = TEXT("int ControlValue = 17;");
		PrintGeneratedAsSource(*TestRunner, ControlSourceId, ControlModuleName, ControlSource);
		const FTCHARToUTF8 ControlModuleNameUtf8(*ControlModuleName);
		const FTCHARToUTF8 ControlSourceUtf8(*ControlSource);
		ASSERT_THAT(IsTrue(ParseRangeScript(
			*TestRunner,
			this->Assert,
			ScriptEngine,
			ControlModuleNameUtf8.Get(),
			ControlSourceUtf8.Get(),
			[&](asCScriptCode&, const asCScriptNode& Root)
			{
				ASSERT_THAT(AreEqual(
					1,
					CountNodesOfType(&Root, snDeclaration),
					TEXT("Source-range isolation control should contain only its own declaration")));
				ASSERT_THAT(AreEqual(
					0,
					CountNodesOfType(&Root, snFunction),
					TEXT("Source-range isolation control should not retain a prior function tree")));
			}),
			TEXT("Source-range isolation control should parse and clean up after every generated cell")));
	}

	TEST_METHOD(LineColPropagatedToFunction)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_NON_PRODUCT("LegacyCompatibility",
			"Retained function source-range smoke; FRONTEND-NODE-SOURCE-RANGE-LINE-ENDINGS owns source shapes across LF and CRLF.");

		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};

		asCScriptEngine* BareEngine = static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsNotNull(BareEngine, TEXT("ScriptNode source-range test should use the case-owned raw SDK engine")));
		if (BareEngine == nullptr)
		{
			return;
		}

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

		AS_NATIVE_NON_PRODUCT("LegacyCompatibility",
			"Retained class-member source-range smoke; FRONTEND-NODE-SOURCE-RANGE-LINE-ENDINGS owns source shapes across LF and CRLF.");

		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};

		asCScriptEngine* BareEngine = static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsNotNull(BareEngine, TEXT("ScriptNode source-range test should use the case-owned raw SDK engine")));
		if (BareEngine == nullptr)
		{
			return;
		}

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

		AS_NATIVE_NON_PRODUCT("LegacyCompatibility",
			"Retained multiline source-span smoke; FRONTEND-NODE-SOURCE-RANGE-LINE-ENDINGS owns start/end positions across LF and CRLF.");

		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};

		asCScriptEngine* BareEngine = static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsNotNull(BareEngine, TEXT("ScriptNode source-range test should use the case-owned raw SDK engine")));
		if (BareEngine == nullptr)
		{
			return;
		}

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

		AS_NATIVE_NON_PRODUCT("LegacyCompatibility",
			"Retained comment-position smoke; FRONTEND-NODE-SOURCE-RANGE-LINE-ENDINGS owns comment and declaration positions across LF and CRLF.");

		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};

		asCScriptEngine* BareEngine = static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsNotNull(BareEngine, TEXT("ScriptNode source-range test should use the case-owned raw SDK engine")));
		if (BareEngine == nullptr)
		{
			return;
		}

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

		AS_NATIVE_NON_PRODUCT("LegacyCompatibility",
			"Retained UTF-8 BOM source-column smoke; FRONTEND-NODE-SOURCE-RANGE-LINE-ENDINGS owns BOM positions across LF and CRLF.");

		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};

		asCScriptEngine* BareEngine = static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsNotNull(BareEngine, TEXT("ScriptNode source-range test should use the case-owned raw SDK engine")));
		if (BareEngine == nullptr)
		{
			return;
		}

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
