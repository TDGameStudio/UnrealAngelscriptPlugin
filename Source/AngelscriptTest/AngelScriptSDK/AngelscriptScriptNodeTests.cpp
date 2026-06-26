#include "AngelscriptNativeTestSupport.h"
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

// ---------------------------------------------------------------------------
// ScriptNode coverage map
// ---------------------------------------------------------------------------
//
// +-----------+--------------------------+----------------------------------+
// | Test      | Focus                    | Verifies                         |
// +-----------+--------------------------+----------------------------------+
// | Types     | enum stability           | root and node enum values        |
// | Traversal | root/child wiring        | child count, first/last/prev     |
// | Copy      | tree cloning semantics   | deep copy keeps shape, not id    |
// +-----------+--------------------------+----------------------------------+
//
// These cases stay deliberately small. They validate the parser's internal
// AST shape before any builder output or runtime execution comes into play.

TEST_CLASS_WITH_FLAGS(FAngelscriptScriptNodeTests,
	"Angelscript.TestModule.AngelScriptSDK.ScriptNode",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	static asCModule* CreateScriptNodeModule(asCScriptEngine* ScriptEngine, const char* ModuleName)
	{
		return static_cast<asCModule*>(ScriptEngine->GetModule(ModuleName, asGM_ALWAYS_CREATE));
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
	TEST_METHOD(Types)
	{
		// Enum smoke test: these values are consumed by parser/tree walkers.
		ASSERT_THAT(AreEqual(1, static_cast<int32>(snScript),
			TEXT("snScript should remain the root script node enum value")));
		ASSERT_THAT(IsTrue(static_cast<int32>(snFunction) > 0,
			TEXT("snFunction should remain a positive enum value")));
		ASSERT_THAT(IsTrue(static_cast<int32>(snClass) > 0,
			TEXT("snClass should remain a positive enum value")));
		ASSERT_THAT(IsTrue(static_cast<int32>(snExpression) > 0,
			TEXT("snExpression should remain a positive enum value")));
	}

	TEST_METHOD(Traversal)
	{
		asCScriptEngine* BareEngine = AngelscriptNativeTestSupport::CreateBareSdkEngine(TestRunner);
		if (BareEngine == nullptr)
		{
			return;
		}

		ON_SCOPE_EXIT { BareEngine->ShutDownAndRelease(); };

		asCModule* Module = CreateScriptNodeModule(BareEngine, "ScriptNodeTraversal");
		ASSERT_THAT(IsNotNull(Module,
			TEXT("ScriptNode traversal test should create a backing module")));

		// The test script is intentionally tiny; only the parser tree matters.
		const std::string TraversalSource = ASTEST_AS_ANSI(R"AS(
			int GlobalValue = 1;

			class FNodeType
			{
				int Value;
			}
			)AS");
		asCBuilder Builder(BareEngine, Module);
		asCScriptCode Code;
		Code.SetCode("ScriptNodeTraversal", TraversalSource.c_str(), true);
		asCParser Parser(&Builder);
		ASSERT_THAT(AreEqual(0, Parser.ParseScript(&Code),
			TEXT("ScriptNode traversal parser run should succeed")));

		asCScriptNode* Root = Parser.GetScriptNode();
		ASSERT_THAT(IsNotNull(Root,
			TEXT("ScriptNode traversal should produce a root node")));

		// The root node should own a flat list of top-level declarations.
		ASSERT_THAT(AreEqual(static_cast<int32>(snScript), static_cast<int32>(Root->nodeType),
			TEXT("Root should be a script node")));
		ASSERT_THAT(AreEqual(2, CountDirectChildren(Root),
			TEXT("Two top-level declarations should produce two direct children")));
		ASSERT_THAT(IsNotNull(Root->firstChild,
			TEXT("Root should expose the first child")));
		ASSERT_THAT(IsNotNull(Root->lastChild,
			TEXT("Root should expose the last child")));
		ASSERT_THAT(IsTrue(Root->firstChild != Root->lastChild,
			TEXT("First and last child should differ when multiple declarations exist")));
		ASSERT_THAT(AreEqual(Root->firstChild, Root->lastChild->prev,
			TEXT("Second child should point back to the first child as prev")));
	}

	TEST_METHOD(Copy)
	{
		asCScriptEngine* BareEngine = AngelscriptNativeTestSupport::CreateBareSdkEngine(TestRunner);
		if (BareEngine == nullptr)
		{
			return;
		}

		ON_SCOPE_EXIT { BareEngine->ShutDownAndRelease(); };

		asCModule* Module = CreateScriptNodeModule(BareEngine, "ScriptNodeCopy");
		ASSERT_THAT(IsNotNull(Module,
			TEXT("ScriptNode copy test should create a backing module")));

		// Parse a single declaration so CreateCopy can be checked in isolation.
		const std::string CopySource = ASTEST_AS_ANSI(R"AS(
			int Value = 3;
			)AS");
		asCBuilder Builder(BareEngine, Module);
		asCScriptCode Code;
		Code.SetCode("ScriptNodeCopy", CopySource.c_str(), true);
		asCParser Parser(&Builder);
		ASSERT_THAT(AreEqual(0, Parser.ParseScript(&Code),
			TEXT("ScriptNode copy parser run should succeed")));

		asCScriptNode* Root = Parser.GetScriptNode();
		ASSERT_THAT(IsNotNull(Root,
			TEXT("ScriptNode copy test should produce a root node")));

		asCScriptNode* Copy = Root->CreateCopy(Parser.MemStack, BareEngine);
		ASSERT_THAT(IsNotNull(Copy,
			TEXT("CreateCopy should produce a duplicate node tree")));

		// Copy should preserve structure, but not reuse any node instances.
		ASSERT_THAT(AreEqual(static_cast<int32>(Root->nodeType), static_cast<int32>(Copy->nodeType),
			TEXT("Copied root should keep the same node type")));
		ASSERT_THAT(IsTrue(Copy != Root,
			TEXT("Copied node should be a different instance")));
		ASSERT_THAT(IsTrue(Copy->firstChild != nullptr && Copy->firstChild != Root->firstChild,
			TEXT("Copied first child should be a different instance")));
	}
};

#endif
