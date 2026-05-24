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

namespace AngelscriptTest_AngelScriptSDK_AngelscriptScriptNodeTests_Private
{
	asCModule* CreateScriptNodeModule(asCScriptEngine* ScriptEngine, const char* ModuleName)
	{
		return static_cast<asCModule*>(ScriptEngine->GetModule(ModuleName, asGM_ALWAYS_CREATE));
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


TEST_CLASS_WITH_FLAGS(FAngelscriptScriptNodeTests,
	"Angelscript.TestModule.AngelScriptSDK.ScriptNode",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(Types)
	{
		using namespace AngelscriptTest_AngelScriptSDK_AngelscriptScriptNodeTests_Private;
		TestRunner->TestEqual(TEXT("snScript should remain the root script node enum value"), static_cast<int32>(snScript), 1);
		TestRunner->TestTrue(TEXT("snFunction should remain a positive enum value"), static_cast<int32>(snFunction) > 0);
		TestRunner->TestTrue(TEXT("snClass should remain a positive enum value"), static_cast<int32>(snClass) > 0);
		TestRunner->TestTrue(TEXT("snExpression should remain a positive enum value"), static_cast<int32>(snExpression) > 0);
	}

	TEST_METHOD(Traversal)
	{
		using namespace AngelscriptTest_AngelScriptSDK_AngelscriptScriptNodeTests_Private;
		asCScriptEngine* BareEngine = reinterpret_cast<asCScriptEngine*>(ASTEST_CREATE_ENGINE_NATIVE());
		if (BareEngine == nullptr)
	{
		TestRunner->AddError(TEXT("Failed to create bare AngelScript SDK engine"));
		return;
	}
	{
		ON_SCOPE_EXIT { BareEngine->ShutDownAndRelease(); };
		asCModule* Module = CreateScriptNodeModule(BareEngine, "ScriptNodeTraversal");
		if (!TestRunner->TestNotNull(TEXT("ScriptNode traversal test should create a backing module"), Module))
		{
			return;
		}

		asCBuilder Builder(BareEngine, Module);
		asCScriptCode Code;
		Code.SetCode("ScriptNodeTraversal", "int GlobalValue = 1; class FNodeType { int Value; }", true);

		asCParser Parser(&Builder);
		if (!TestRunner->TestEqual(TEXT("ScriptNode traversal parser run should succeed"), Parser.ParseScript(&Code), 0))
		{
			return;
		}

		asCScriptNode* Root = Parser.GetScriptNode();
		if (!TestRunner->TestNotNull(TEXT("ScriptNode traversal should produce a root node"), Root))
		{
			return;
		}

		TestRunner->TestEqual(TEXT("Root should be a script node"), static_cast<int32>(Root->nodeType), static_cast<int32>(snScript));
		TestRunner->TestEqual(TEXT("Two top-level declarations should produce two direct children"), CountDirectChildren(Root), 2);
		TestRunner->TestNotNull(TEXT("Root should expose the first child"), Root->firstChild);
		TestRunner->TestNotNull(TEXT("Root should expose the last child"), Root->lastChild);
		TestRunner->TestTrue(TEXT("First and last child should differ when multiple declarations exist"), Root->firstChild != Root->lastChild);
		TestRunner->TestEqual(TEXT("Second child should point back to the first child as prev"), Root->lastChild->prev, Root->firstChild);
		}
	}

	TEST_METHOD(Copy)
	{
		using namespace AngelscriptTest_AngelScriptSDK_AngelscriptScriptNodeTests_Private;
		asCScriptEngine* BareEngine = reinterpret_cast<asCScriptEngine*>(ASTEST_CREATE_ENGINE_NATIVE());
		if (BareEngine == nullptr)
	{
		TestRunner->AddError(TEXT("Failed to create bare AngelScript SDK engine"));
		return;
	}
	{
		ON_SCOPE_EXIT { BareEngine->ShutDownAndRelease(); };
		asCModule* Module = CreateScriptNodeModule(BareEngine, "ScriptNodeCopy");
		if (!TestRunner->TestNotNull(TEXT("ScriptNode copy test should create a backing module"), Module))
		{
			return;
		}

		asCBuilder Builder(BareEngine, Module);
		asCScriptCode Code;
		Code.SetCode("ScriptNodeCopy", "int Value = 3;", true);

		asCParser Parser(&Builder);
		if (!TestRunner->TestEqual(TEXT("ScriptNode copy parser run should succeed"), Parser.ParseScript(&Code), 0))
		{
			return;
		}

		asCScriptNode* Root = Parser.GetScriptNode();
		if (!TestRunner->TestNotNull(TEXT("ScriptNode copy test should produce a root node"), Root))
		{
			return;
		}

		asCScriptNode* Copy = Root->CreateCopy(Parser.MemStack, BareEngine);
		if (!TestRunner->TestNotNull(TEXT("CreateCopy should produce a duplicate node tree"), Copy))
		{
			return;
		}

		TestRunner->TestEqual(TEXT("Copied root should keep the same node type"), static_cast<int32>(Copy->nodeType), static_cast<int32>(Root->nodeType));
		TestRunner->TestTrue(TEXT("Copied node should be a different instance"), Copy != Root);
		TestRunner->TestTrue(TEXT("Copied first child should be a different instance"), Copy->firstChild != nullptr && Copy->firstChild != Root->firstChild);
		}
	}
};

#endif
