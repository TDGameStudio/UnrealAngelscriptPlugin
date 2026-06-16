#include "AngelscriptNativeTestSupport.h"
#include "CQTest.h"
#include "Misc/ScopeExit.h"

#include <string>

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
	bool ParseCopyScript(FAutomationTestBase& Test, asCScriptEngine* ScriptEngine, const char* ModuleName, const char* Source, TFunctionRef<void(FParserAccessor&, asCScriptNode&)> Verify)
	{
		asCModule* Module = CreateSdkModule(ScriptEngine, ModuleName);
		if (!Test.TestNotNull(FString::Printf(TEXT("%s should create a script-node copy module"), UTF8_TO_TCHAR(ModuleName)), Module))
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

		asCScriptNode* Root = Parser.GetScriptNode();
		if (!Test.TestNotNull(FString::Printf(TEXT("%s should produce a script root"), UTF8_TO_TCHAR(ModuleName)), Root))
		{
			return false;
		}

		Verify(Parser, *Root);
		return true;
	}

	bool HistogramsMatch(const TMap<eScriptNode, int32>& Left, const TMap<eScriptNode, int32>& Right)
	{
		if (Left.Num() != Right.Num())
		{
			return false;
		}

		for (const TPair<eScriptNode, int32>& Pair : Left)
		{
			const int32* OtherCount = Right.Find(Pair.Key);
			if (OtherCount == nullptr || *OtherCount != Pair.Value)
			{
				return false;
			}
		}

		return true;
	}

	int32 CountSiblings(const asCScriptNode* Node)
	{
		int32 Count = 0;
		for (const asCScriptNode* Current = Node; Current != nullptr; Current = Current->next)
		{
			++Count;
		}
		return Count;
	}

	std::string MakeDeepBlockSource(const int32 Depth)
	{
		std::string Source = "void Run() ";
		for (int32 Index = 0; Index < Depth; ++Index)
		{
			Source += "{ ";
		}
		Source += "return;";
		for (int32 Index = 0; Index < Depth; ++Index)
		{
			Source += " }";
		}
		return Source;
	}
}

TEST_CLASS_WITH_FLAGS(FAngelscriptNativeScriptNodeCopyTests,
	"Angelscript.TestModule.AngelScriptSDK.ScriptNode.Copy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(CreateCopyPreservesNodeTypes)
	{
		asCScriptEngine* BareEngine = AngelscriptNativeTestSupport::CreateBareSdkEngine(&*TestRunner);
		if (!TestRunner->TestNotNull(TEXT("ScriptNode copy test should create a bare engine"), BareEngine))
		{
			return;
		}
		ON_SCOPE_EXIT { BareEngine->ShutDownAndRelease(); };

		ParseCopyScript(*TestRunner, BareEngine, "ScriptNodeCopyTypes", "int Value = 1; class FNode { int Read() { return Value; } }", [&](FParserAccessor& Parser, asCScriptNode& Root)
		{
			asCScriptNode* Copy = Root.CreateCopy(Parser.MemStack, BareEngine);
			if (!TestRunner->TestNotNull(TEXT("CreateCopy should duplicate the script root"), Copy))
			{
				return;
			}

			TestRunner->TestTrue(TEXT("Copied tree should preserve the node-type histogram"), HistogramsMatch(AngelscriptNativeTestSupport::NodeTypeHistogram(&Root), AngelscriptNativeTestSupport::NodeTypeHistogram(Copy)));
		});
	}

	TEST_METHOD(CreateCopyPreservesChildOrdering)
	{
		asCScriptEngine* BareEngine = AngelscriptNativeTestSupport::CreateBareSdkEngine(&*TestRunner);
		if (!TestRunner->TestNotNull(TEXT("ScriptNode copy test should create a bare engine"), BareEngine))
		{
			return;
		}
		ON_SCOPE_EXIT { BareEngine->ShutDownAndRelease(); };

		ParseCopyScript(*TestRunner, BareEngine, "ScriptNodeCopyOrdering", "int A = 1; int B = 2; int C = 3;", [&](FParserAccessor& Parser, asCScriptNode& Root)
		{
			asCScriptNode* Copy = Root.CreateCopy(Parser.MemStack, BareEngine);
			if (!TestRunner->TestNotNull(TEXT("CreateCopy should duplicate top-level declarations"), Copy))
			{
				return;
			}

			TestRunner->TestEqual(TEXT("Copied root should preserve top-level child count"), CountSiblings(Copy->firstChild), CountSiblings(Root.firstChild));
			TestRunner->TestEqual(TEXT("First copied child should keep the first child type"), static_cast<int32>(Copy->firstChild->nodeType), static_cast<int32>(Root.firstChild->nodeType));
			TestRunner->TestEqual(TEXT("Second copied child should keep sibling order"), static_cast<int32>(Copy->firstChild->next->nodeType), static_cast<int32>(Root.firstChild->next->nodeType));
			TestRunner->TestTrue(TEXT("Copied siblings should point back through prev"), Copy->firstChild->next->prev == Copy->firstChild);
		});
	}

	TEST_METHOD(CreateCopyPreservesSourceRange)
	{
		asCScriptEngine* BareEngine = AngelscriptNativeTestSupport::CreateBareSdkEngine(&*TestRunner);
		if (!TestRunner->TestNotNull(TEXT("ScriptNode copy test should create a bare engine"), BareEngine))
		{
			return;
		}
		ON_SCOPE_EXIT { BareEngine->ShutDownAndRelease(); };

		ParseCopyScript(*TestRunner, BareEngine, "ScriptNodeCopyRange", "\nint Value = 9;\n", [&](FParserAccessor& Parser, asCScriptNode& Root)
		{
			asCScriptNode* Copy = Root.CreateCopy(Parser.MemStack, BareEngine);
			if (!TestRunner->TestNotNull(TEXT("CreateCopy should duplicate source ranges"), Copy))
			{
				return;
			}

			const asCScriptNode* OriginalDeclaration = FindFirstNodeOfType(&Root, snDeclaration);
			const asCScriptNode* CopiedDeclaration = FindFirstNodeOfType(Copy, snDeclaration);
			if (!TestRunner->TestNotNull(TEXT("Original tree should contain a declaration node"), OriginalDeclaration) || !TestRunner->TestNotNull(TEXT("Copied tree should contain a declaration node"), CopiedDeclaration))
			{
				return;
			}

			TestRunner->TestEqual(TEXT("Copied declaration node should preserve token position"), static_cast<int32>(CopiedDeclaration->tokenPos), static_cast<int32>(OriginalDeclaration->tokenPos));
			TestRunner->TestEqual(TEXT("Copied declaration node should preserve token length"), static_cast<int32>(CopiedDeclaration->tokenLength), static_cast<int32>(OriginalDeclaration->tokenLength));
			TestRunner->TestEqual(TEXT("Copied declaration node should preserve token type"), static_cast<int32>(CopiedDeclaration->tokenType), static_cast<int32>(OriginalDeclaration->tokenType));
		});
	}

	TEST_METHOD(CreateCopyDeepNestingNoStackBlow)
	{
		asCScriptEngine* BareEngine = AngelscriptNativeTestSupport::CreateBareSdkEngine(&*TestRunner);
		if (!TestRunner->TestNotNull(TEXT("ScriptNode copy test should create a bare engine"), BareEngine))
		{
			return;
		}
		ON_SCOPE_EXIT { BareEngine->ShutDownAndRelease(); };

		const std::string Source = MakeDeepBlockSource(50);
		ParseCopyScript(*TestRunner, BareEngine, "ScriptNodeCopyDeepNesting", Source.c_str(), [&](FParserAccessor& Parser, asCScriptNode& Root)
		{
			asCScriptNode* Copy = Root.CreateCopy(Parser.MemStack, BareEngine);
			if (!TestRunner->TestNotNull(TEXT("CreateCopy should duplicate a deeply nested tree"), Copy))
			{
				return;
			}

			TestRunner->TestTrue(TEXT("Deep source should produce a non-trivial AST before copy"), AngelscriptNativeTestSupport::MaxNodeDepth(&Root) >= 2);
			TestRunner->TestEqual(TEXT("Copied deep tree should preserve maximum depth"), AngelscriptNativeTestSupport::MaxNodeDepth(Copy), AngelscriptNativeTestSupport::MaxNodeDepth(&Root));
		});
	}

	TEST_METHOD(SiblingTraversalVisitsAllNodes)
	{
		asCScriptEngine* BareEngine = AngelscriptNativeTestSupport::CreateBareSdkEngine(&*TestRunner);
		if (!TestRunner->TestNotNull(TEXT("ScriptNode copy test should create a bare engine"), BareEngine))
		{
			return;
		}
		ON_SCOPE_EXIT { BareEngine->ShutDownAndRelease(); };

		ParseCopyScript(*TestRunner, BareEngine, "ScriptNodeCopySiblings", "int A = 1; int B = 2; class FNode { } enum ENode { One }", [&](FParserAccessor& Parser, asCScriptNode& Root)
		{
			asCScriptNode* Copy = Root.CreateCopy(Parser.MemStack, BareEngine);
			if (!TestRunner->TestNotNull(TEXT("CreateCopy should duplicate sibling traversal fixture"), Copy))
			{
				return;
			}

			TestRunner->TestEqual(TEXT("Original root should expose four top-level siblings"), CountSiblings(Root.firstChild), 4);
			TestRunner->TestEqual(TEXT("Copied root sibling traversal should visit every top-level node"), CountSiblings(Copy->firstChild), 4);
			TestRunner->TestEqual(TEXT("Copied lastChild should match the final traversed sibling type"), static_cast<int32>(Copy->lastChild->nodeType), static_cast<int32>(Root.lastChild->nodeType));
		});
	}

	TEST_METHOD(EnumeratePerNodeTypeViaWalker)
	{
		asCScriptEngine* BareEngine = AngelscriptNativeTestSupport::CreateBareSdkEngine(&*TestRunner);
		if (!TestRunner->TestNotNull(TEXT("ScriptNode copy test should create a bare engine"), BareEngine))
		{
			return;
		}
		ON_SCOPE_EXIT { BareEngine->ShutDownAndRelease(); };

		ParseCopyScript(*TestRunner, BareEngine, "ScriptNodeCopyHistogram", "int A = 1; void Run() { if (A > 0) { A += 1; } }", [&](FParserAccessor& Parser, asCScriptNode& Root)
		{
			asCScriptNode* Copy = Root.CreateCopy(Parser.MemStack, BareEngine);
			if (!TestRunner->TestNotNull(TEXT("CreateCopy should duplicate histogram fixture"), Copy))
			{
				return;
			}

			const TMap<eScriptNode, int32> OriginalHistogram = AngelscriptNativeTestSupport::NodeTypeHistogram(&Root);
			const TMap<eScriptNode, int32> CopiedHistogram = AngelscriptNativeTestSupport::NodeTypeHistogram(Copy);
			TestRunner->TestTrue(TEXT("Copied histogram should exactly match the original parsed tree"), HistogramsMatch(OriginalHistogram, CopiedHistogram));
			TestRunner->TestEqual(TEXT("Copied histogram should include one function node"), CopiedHistogram.FindRef(snFunction), 1);
		});
	}

	TEST_METHOD(DisconnectAndReattachIfExposed)
	{
		asCScriptEngine* BareEngine = AngelscriptNativeTestSupport::CreateBareSdkEngine(&*TestRunner);
		if (!TestRunner->TestNotNull(TEXT("ScriptNode copy test should create a bare engine"), BareEngine))
		{
			return;
		}
		ON_SCOPE_EXIT { BareEngine->ShutDownAndRelease(); };

		ParseCopyScript(*TestRunner, BareEngine, "ScriptNodeCopyReattach", "int A = 1; int B = 2;", [&](FParserAccessor& Parser, asCScriptNode& Root)
		{
			asCScriptNode* Copy = Root.CreateCopy(Parser.MemStack, BareEngine);
			if (!TestRunner->TestNotNull(TEXT("CreateCopy should duplicate reattach fixture"), Copy))
			{
				return;
			}

			asCScriptNode* Detached = Copy->firstChild;
			if (!TestRunner->TestNotNull(TEXT("Copied root should have a first child to detach"), Detached))
			{
				return;
			}

			Detached->DisconnectParent();
			TestRunner->TestNull(TEXT("Detached node should clear parent pointer"), Detached->parent);
			TestRunner->TestNull(TEXT("Detached node should clear next pointer"), Detached->next);
			TestRunner->TestEqual(TEXT("Copied root should expose one remaining child after detach"), CountSiblings(Copy->firstChild), 1);

			Copy->AddChildLast(Detached);
			TestRunner->TestEqual(TEXT("Reattached node should restore root as parent"), Detached->parent, Copy);
			TestRunner->TestEqual(TEXT("Reattached node should become the copied root last child"), Copy->lastChild, Detached);
			TestRunner->TestEqual(TEXT("Reattach should restore two top-level children"), CountSiblings(Copy->firstChild), 2);
		});
	}
};

#endif
