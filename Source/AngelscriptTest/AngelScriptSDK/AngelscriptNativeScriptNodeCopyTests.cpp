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



TEST_CLASS_WITH_FLAGS(FAngelscriptNativeScriptNodeCopyTests,
	"Angelscript.TestModule.AngelScriptSDK.ScriptNode.Copy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	static bool ParseCopyScript(
		FAutomationTestBase& Test,
		FNoDiscardAsserter& Assert,
		asCScriptEngine* ScriptEngine,
		const char* ModuleName,
		const char* Source,
		TFunctionRef<void(AngelscriptNativeTestSupport::FParserAccessor&, asCScriptNode&)> Verify)
	{
		asCModule* Module = AngelscriptNativeTestSupport::CreateSdkModule(ScriptEngine, ModuleName);
		if (!Assert.IsNotNull(Module, FString::Printf(TEXT("%s should create a script-node copy module"), UTF8_TO_TCHAR(ModuleName))))
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

		asCScriptNode* Root = Parser.GetScriptNode();
		if (!Assert.IsNotNull(Root, FString::Printf(TEXT("%s should produce a script root"), UTF8_TO_TCHAR(ModuleName))))
		{
			return false;
		}

		Verify(Parser, *Root);
		return true;
	}

	static bool HistogramsMatch(const TMap<eScriptNode, int32>& Left, const TMap<eScriptNode, int32>& Right)
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

	static int32 CountSiblings(const asCScriptNode* Node)
	{
		int32 Count = 0;
		for (const asCScriptNode* Current = Node; Current != nullptr; Current = Current->next)
		{
			++Count;
		}
		return Count;
	}

	static std::string MakeDeepBlockSource(const int32 Depth)
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

public:
	TEST_METHOD(CreateCopyPreservesNodeTypes)
	{
		asCScriptEngine* BareEngine = AngelscriptNativeTestSupport::CreateBareSdkEngine(&*TestRunner);
		ASSERT_THAT(IsNotNull(BareEngine, TEXT("ScriptNode copy test should create a bare engine")));
		ON_SCOPE_EXIT { BareEngine->ShutDownAndRelease(); };

		ParseCopyScript(*TestRunner, this->Assert, BareEngine, "ScriptNodeCopyTypes", "int Value = 1; class FNode { int Read() { return Value; } }", [&](AngelscriptNativeTestSupport::FParserAccessor& Parser, asCScriptNode& Root)
		{
			asCScriptNode* Copy = Root.CreateCopy(Parser.MemStack, BareEngine);
			if (!this->Assert.IsNotNull(Copy, TEXT("CreateCopy should duplicate the script root")))
			{
				return;
			}

			if (!this->Assert.IsTrue(HistogramsMatch(AngelscriptNativeTestSupport::NodeTypeHistogram(&Root), AngelscriptNativeTestSupport::NodeTypeHistogram(Copy)),
				TEXT("Copied tree should preserve the node-type histogram")))
			{
				return;
			}
		});
	}

	TEST_METHOD(CreateCopyPreservesChildOrdering)
	{
		asCScriptEngine* BareEngine = AngelscriptNativeTestSupport::CreateBareSdkEngine(&*TestRunner);
		ASSERT_THAT(IsNotNull(BareEngine, TEXT("ScriptNode copy test should create a bare engine")));
		ON_SCOPE_EXIT { BareEngine->ShutDownAndRelease(); };

		ParseCopyScript(*TestRunner, this->Assert, BareEngine, "ScriptNodeCopyOrdering", "int A = 1; int B = 2; int C = 3;", [&](AngelscriptNativeTestSupport::FParserAccessor& Parser, asCScriptNode& Root)
		{
			asCScriptNode* Copy = Root.CreateCopy(Parser.MemStack, BareEngine);
			if (!this->Assert.IsNotNull(Copy, TEXT("CreateCopy should duplicate top-level declarations")))
			{
				return;
			}

			if (!this->Assert.AreEqual(CountSiblings(Root.firstChild), CountSiblings(Copy->firstChild),
				TEXT("Copied root should preserve top-level child count")))
			{
				return;
			}
			if (!this->Assert.AreEqual(static_cast<int32>(Root.firstChild->nodeType), static_cast<int32>(Copy->firstChild->nodeType),
				TEXT("First copied child should keep the first child type")))
			{
				return;
			}
			if (!this->Assert.AreEqual(static_cast<int32>(Root.firstChild->next->nodeType), static_cast<int32>(Copy->firstChild->next->nodeType),
				TEXT("Second copied child should keep sibling order")))
			{
				return;
			}
			if (!this->Assert.IsTrue(Copy->firstChild->next->prev == Copy->firstChild,
				TEXT("Copied siblings should point back through prev")))
			{
				return;
			}
		});
	}

	TEST_METHOD(CreateCopyPreservesSourceRange)
	{
		using namespace AngelscriptNativeTestSupport;

		asCScriptEngine* BareEngine = AngelscriptNativeTestSupport::CreateBareSdkEngine(&*TestRunner);
		ASSERT_THAT(IsNotNull(BareEngine, TEXT("ScriptNode copy test should create a bare engine")));
		ON_SCOPE_EXIT { BareEngine->ShutDownAndRelease(); };

		ParseCopyScript(*TestRunner, this->Assert, BareEngine, "ScriptNodeCopyRange", "\nint Value = 9;\n", [&](AngelscriptNativeTestSupport::FParserAccessor& Parser, asCScriptNode& Root)
		{
			asCScriptNode* Copy = Root.CreateCopy(Parser.MemStack, BareEngine);
			if (!this->Assert.IsNotNull(Copy, TEXT("CreateCopy should duplicate source ranges")))
			{
				return;
			}

			const asCScriptNode* OriginalDeclaration = FindFirstNodeOfType(&Root, snDeclaration);
			const asCScriptNode* CopiedDeclaration = FindFirstNodeOfType(Copy, snDeclaration);
			if (!this->Assert.IsNotNull(OriginalDeclaration, TEXT("Original tree should contain a declaration node"))
				|| !this->Assert.IsNotNull(CopiedDeclaration, TEXT("Copied tree should contain a declaration node")))
			{
				return;
			}

			if (!this->Assert.AreEqual(static_cast<int32>(OriginalDeclaration->tokenPos), static_cast<int32>(CopiedDeclaration->tokenPos),
				TEXT("Copied declaration node should preserve token position")))
			{
				return;
			}
			if (!this->Assert.AreEqual(static_cast<int32>(OriginalDeclaration->tokenLength), static_cast<int32>(CopiedDeclaration->tokenLength),
				TEXT("Copied declaration node should preserve token length")))
			{
				return;
			}
			if (!this->Assert.AreEqual(static_cast<int32>(OriginalDeclaration->tokenType), static_cast<int32>(CopiedDeclaration->tokenType),
				TEXT("Copied declaration node should preserve token type")))
			{
				return;
			}
		});
	}

	TEST_METHOD(CreateCopyDeepNestingNoStackBlow)
	{
		asCScriptEngine* BareEngine = AngelscriptNativeTestSupport::CreateBareSdkEngine(&*TestRunner);
		ASSERT_THAT(IsNotNull(BareEngine, TEXT("ScriptNode copy test should create a bare engine")));
		ON_SCOPE_EXIT { BareEngine->ShutDownAndRelease(); };

		const std::string Source = MakeDeepBlockSource(50);
		ParseCopyScript(*TestRunner, this->Assert, BareEngine, "ScriptNodeCopyDeepNesting", Source.c_str(), [&](AngelscriptNativeTestSupport::FParserAccessor& Parser, asCScriptNode& Root)
		{
			asCScriptNode* Copy = Root.CreateCopy(Parser.MemStack, BareEngine);
			if (!this->Assert.IsNotNull(Copy, TEXT("CreateCopy should duplicate a deeply nested tree")))
			{
				return;
			}

			if (!this->Assert.IsTrue(AngelscriptNativeTestSupport::MaxNodeDepth(&Root) >= 2,
				TEXT("Deep source should produce a non-trivial AST before copy")))
			{
				return;
			}
			if (!this->Assert.AreEqual(AngelscriptNativeTestSupport::MaxNodeDepth(&Root), AngelscriptNativeTestSupport::MaxNodeDepth(Copy),
				TEXT("Copied deep tree should preserve maximum depth")))
			{
				return;
			}
		});
	}

	TEST_METHOD(SiblingTraversalVisitsAllNodes)
	{
		asCScriptEngine* BareEngine = AngelscriptNativeTestSupport::CreateBareSdkEngine(&*TestRunner);
		ASSERT_THAT(IsNotNull(BareEngine, TEXT("ScriptNode copy test should create a bare engine")));
		ON_SCOPE_EXIT { BareEngine->ShutDownAndRelease(); };

		ParseCopyScript(*TestRunner, this->Assert, BareEngine, "ScriptNodeCopySiblings", "int A = 1; int B = 2; class FNode { } enum ENode { One }", [&](AngelscriptNativeTestSupport::FParserAccessor& Parser, asCScriptNode& Root)
		{
			asCScriptNode* Copy = Root.CreateCopy(Parser.MemStack, BareEngine);
			if (!this->Assert.IsNotNull(Copy, TEXT("CreateCopy should duplicate sibling traversal fixture")))
			{
				return;
			}

			if (!this->Assert.AreEqual(4, CountSiblings(Root.firstChild),
				TEXT("Original root should expose four top-level siblings")))
			{
				return;
			}
			if (!this->Assert.AreEqual(4, CountSiblings(Copy->firstChild),
				TEXT("Copied root sibling traversal should visit every top-level node")))
			{
				return;
			}
			if (!this->Assert.AreEqual(static_cast<int32>(Root.lastChild->nodeType), static_cast<int32>(Copy->lastChild->nodeType),
				TEXT("Copied lastChild should match the final traversed sibling type")))
			{
				return;
			}
		});
	}

	TEST_METHOD(EnumeratePerNodeTypeViaWalker)
	{
		asCScriptEngine* BareEngine = AngelscriptNativeTestSupport::CreateBareSdkEngine(&*TestRunner);
		ASSERT_THAT(IsNotNull(BareEngine, TEXT("ScriptNode copy test should create a bare engine")));
		ON_SCOPE_EXIT { BareEngine->ShutDownAndRelease(); };

		ParseCopyScript(*TestRunner, this->Assert, BareEngine, "ScriptNodeCopyHistogram", "int A = 1; void Run() { if (A > 0) { A += 1; } }", [&](AngelscriptNativeTestSupport::FParserAccessor& Parser, asCScriptNode& Root)
		{
			asCScriptNode* Copy = Root.CreateCopy(Parser.MemStack, BareEngine);
			if (!this->Assert.IsNotNull(Copy, TEXT("CreateCopy should duplicate histogram fixture")))
			{
				return;
			}

			const TMap<eScriptNode, int32> OriginalHistogram = AngelscriptNativeTestSupport::NodeTypeHistogram(&Root);
			const TMap<eScriptNode, int32> CopiedHistogram = AngelscriptNativeTestSupport::NodeTypeHistogram(Copy);
			if (!this->Assert.IsTrue(HistogramsMatch(OriginalHistogram, CopiedHistogram),
				TEXT("Copied histogram should exactly match the original parsed tree")))
			{
				return;
			}
			if (!this->Assert.AreEqual(1, CopiedHistogram.FindRef(snFunction),
				TEXT("Copied histogram should include one function node")))
			{
				return;
			}
		});
	}

	TEST_METHOD(DisconnectAndReattachIfExposed)
	{
		asCScriptEngine* BareEngine = AngelscriptNativeTestSupport::CreateBareSdkEngine(&*TestRunner);
		ASSERT_THAT(IsNotNull(BareEngine, TEXT("ScriptNode copy test should create a bare engine")));
		ON_SCOPE_EXIT { BareEngine->ShutDownAndRelease(); };

		ParseCopyScript(*TestRunner, this->Assert, BareEngine, "ScriptNodeCopyReattach", "int A = 1; int B = 2;", [&](AngelscriptNativeTestSupport::FParserAccessor& Parser, asCScriptNode& Root)
		{
			asCScriptNode* Copy = Root.CreateCopy(Parser.MemStack, BareEngine);
			if (!this->Assert.IsNotNull(Copy, TEXT("CreateCopy should duplicate reattach fixture")))
			{
				return;
			}

			asCScriptNode* Detached = Copy->firstChild;
			if (!this->Assert.IsNotNull(Detached, TEXT("Copied root should have a first child to detach")))
			{
				return;
			}

			Detached->DisconnectParent();
			if (!this->Assert.IsNull(Detached->parent, TEXT("Detached node should clear parent pointer")))
			{
				return;
			}
			if (!this->Assert.IsNull(Detached->next, TEXT("Detached node should clear next pointer")))
			{
				return;
			}
			if (!this->Assert.AreEqual(1, CountSiblings(Copy->firstChild),
				TEXT("Copied root should expose one remaining child after detach")))
			{
				return;
			}

			Copy->AddChildLast(Detached);
			if (!this->Assert.AreEqual(Copy, Detached->parent,
				TEXT("Reattached node should restore root as parent")))
			{
				return;
			}
			if (!this->Assert.AreEqual(Detached, Copy->lastChild,
				TEXT("Reattached node should become the copied root last child")))
			{
				return;
			}
			if (!this->Assert.AreEqual(2, CountSiblings(Copy->firstChild),
				TEXT("Reattach should restore two top-level children")))
			{
				return;
			}
		});
	}
};

#endif
