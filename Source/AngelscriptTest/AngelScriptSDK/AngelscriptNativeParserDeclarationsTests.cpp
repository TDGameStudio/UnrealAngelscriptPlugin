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

namespace AngelscriptTest_AngelScriptSDK_ParserDeclarations_Private
{
	struct FParserAccessor : asCParser
	{
		explicit FParserAccessor(asCBuilder* Builder)
			: asCParser(Builder)
		{
		}

		asCScriptNode* ParseExpressionSnippet(asCScriptCode* InScript)
		{
			Reset();
			script = InScript;
			return ParseExpression();
		}
	};

	asCModule* CreateModule(asCScriptEngine* ScriptEngine, const char* ModuleName)
	{
		return static_cast<asCModule*>(ScriptEngine->GetModule(ModuleName, asGM_ALWAYS_CREATE));
	}

	bool ParseScript(FAutomationTestBase& Test, asCScriptEngine* ScriptEngine, const char* ModuleName, const char* Source, TFunctionRef<void(const asCScriptNode&)> Verify)
	{
		asCModule* Module = CreateModule(ScriptEngine, ModuleName);
		if (!Test.TestNotNull(FString::Printf(TEXT("%s should create a parser module"), UTF8_TO_TCHAR(ModuleName)), Module))
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
		if (!Test.TestNotNull(FString::Printf(TEXT("%s should produce a root script node"), UTF8_TO_TCHAR(ModuleName)), Root))
		{
			return false;
		}

		Verify(*Root);
		return true;
	}

	int ParseScriptWithResult(asCScriptEngine* ScriptEngine, const char* ModuleName, const char* Source)
	{
		asCModule* Module = CreateModule(ScriptEngine, ModuleName);
		asCBuilder Builder(ScriptEngine, Module);
		Builder.silent = true;

		asCScriptCode Code;
		Code.SetCode(ModuleName, Source, true);

		FParserAccessor Parser(&Builder);
		return Parser.ParseScript(&Code);
	}

	bool ParseExpression(FAutomationTestBase& Test, asCScriptEngine* ScriptEngine, const char* ModuleName, const char* Source, TFunctionRef<void(const asCScriptNode&)> Verify)
	{
		asCModule* Module = CreateModule(ScriptEngine, ModuleName);
		if (!Test.TestNotNull(FString::Printf(TEXT("%s should create a parser-expression module"), UTF8_TO_TCHAR(ModuleName)), Module))
		{
			return false;
		}

		asCBuilder Builder(ScriptEngine, Module);
		asCScriptCode Code;
		Code.SetCode(ModuleName, Source, true);

		FParserAccessor Parser(&Builder);
		asCScriptNode* Root = Parser.ParseExpressionSnippet(&Code);
		if (!Test.TestNotNull(FString::Printf(TEXT("%s should parse an expression root"), UTF8_TO_TCHAR(ModuleName)), Root))
		{
			return false;
		}

		Verify(*Root);
		return true;
	}

	bool RegisterArrayTemplate(FAutomationTestBase& Test, asCScriptEngine* ScriptEngine, const TCHAR* Context)
	{
		const int RegisterResult = ScriptEngine->RegisterObjectType("array<class T>", 0, asOBJ_REF | asOBJ_TEMPLATE | asOBJ_NOCOUNT);
		return Test.TestTrue(FString::Printf(TEXT("%s should register the parser-only array<T> template type"), Context), RegisterResult >= 0);
	}
}

TEST_CLASS_WITH_FLAGS(FAngelscriptNativeParserDeclarationsTests,
	"Angelscript.TestModule.AngelScriptSDK.Parser.Declarations",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(FunctionWithDefaultParam)
	{
		using namespace AngelscriptTest_AngelScriptSDK_ParserDeclarations_Private;
		asCScriptEngine* BareEngine = AngelscriptNativeTestSupport::CreateBareSdkEngine(&*TestRunner);
		if (!TestRunner->TestNotNull(TEXT("Parser declaration test should create a bare engine"), BareEngine))
		{
			return;
		}
		ON_SCOPE_EXIT { BareEngine->ShutDownAndRelease(); };

		ParseScript(*TestRunner, BareEngine, "ParserDeclDefaultParam", "int Add(int A, int B = 2) { return A + B; }", [&](const asCScriptNode& Root)
		{
			TestRunner->TestEqual(TEXT("Default-param function parse should produce one function node"), AngelscriptNativeTestSupport::CountNodesOfType(&Root, snFunction), 1);
			TestRunner->TestEqual(TEXT("Default-param function parse should produce a parameter list"), AngelscriptNativeTestSupport::CountNodesOfType(&Root, snParameterList), 1);
		});
	}

	TEST_METHOD(FunctionWithInOutInoutRefs)
	{
		using namespace AngelscriptTest_AngelScriptSDK_ParserDeclarations_Private;
		asCScriptEngine* BareEngine = AngelscriptNativeTestSupport::CreateBareSdkEngine(&*TestRunner);
		if (!TestRunner->TestNotNull(TEXT("Parser declaration test should create a bare engine"), BareEngine))
		{
			return;
		}
		ON_SCOPE_EXIT { BareEngine->ShutDownAndRelease(); };

		ParseScript(*TestRunner, BareEngine, "ParserDeclRefParams", "void Mutate(int& in A, int& out B, int& inout C) { }", [&](const asCScriptNode& Root)
		{
			TestRunner->TestEqual(TEXT("Ref-param parse should produce one function node"), AngelscriptNativeTestSupport::CountNodesOfType(&Root, snFunction), 1);
			TestRunner->TestTrue(TEXT("Ref-param parse should include multiple data type nodes"), AngelscriptNativeTestSupport::CountNodesOfType(&Root, snDataType) >= 4);
		});
	}

	TEST_METHOD(ClassWithInheritance)
	{
		using namespace AngelscriptTest_AngelScriptSDK_ParserDeclarations_Private;
		asCScriptEngine* BareEngine = AngelscriptNativeTestSupport::CreateBareSdkEngine(&*TestRunner);
		if (!TestRunner->TestNotNull(TEXT("Parser declaration test should create a bare engine"), BareEngine))
		{
			return;
		}
		ON_SCOPE_EXIT { BareEngine->ShutDownAndRelease(); };

		ParseScript(*TestRunner, BareEngine, "ParserDeclInheritance", "class FBase { } class FDerived : FBase { }", [&](const asCScriptNode& Root)
		{
			TestRunner->TestEqual(TEXT("Inheritance parse should produce two class nodes"), AngelscriptNativeTestSupport::CountNodesOfType(&Root, snClass), 2);
		});
	}

	TEST_METHOD(ClassImplementsInterface)
	{
		using namespace AngelscriptTest_AngelScriptSDK_ParserDeclarations_Private;
		asCScriptEngine* BareEngine = AngelscriptNativeTestSupport::CreateBareSdkEngine(&*TestRunner);
		if (!TestRunner->TestNotNull(TEXT("Parser declaration test should create a bare engine"), BareEngine))
		{
			return;
		}
		ON_SCOPE_EXIT { BareEngine->ShutDownAndRelease(); };

		const int ParseResult = ParseScriptWithResult(BareEngine, "ParserDeclInterfaceImpl", "interface IThing { void Run(); } class FThing : IThing { void Run() { } }");
		TestRunner->TestTrue(TEXT("Script-level interface declarations are currently rejected by the fork tokenizer/parser boundary"), ParseResult < 0);
	}

	TEST_METHOD(ClassFinalAbstract)
	{
		using namespace AngelscriptTest_AngelScriptSDK_ParserDeclarations_Private;
		asCScriptEngine* BareEngine = AngelscriptNativeTestSupport::CreateBareSdkEngine(&*TestRunner);
		if (!TestRunner->TestNotNull(TEXT("Parser declaration test should create a bare engine"), BareEngine))
		{
			return;
		}
		ON_SCOPE_EXIT { BareEngine->ShutDownAndRelease(); };

		ParseScript(*TestRunner, BareEngine, "ParserDeclClassModifiers", "class FFinal { } class FAbstract { }", [&](const asCScriptNode& Root)
		{
			TestRunner->TestEqual(TEXT("Class modifier parse should keep both class declarations"), AngelscriptNativeTestSupport::CountNodesOfType(&Root, snClass), 2);
		});
	}

	TEST_METHOD(MixinClassDeclaration)
	{
		using namespace AngelscriptTest_AngelScriptSDK_ParserDeclarations_Private;
		asCScriptEngine* BareEngine = AngelscriptNativeTestSupport::CreateBareSdkEngine(&*TestRunner);
		if (!TestRunner->TestNotNull(TEXT("Parser declaration test should create a bare engine"), BareEngine))
		{
			return;
		}
		ON_SCOPE_EXIT { BareEngine->ShutDownAndRelease(); };

		ParseScript(*TestRunner, BareEngine, "ParserDeclMixin", "mixin void SharedUtility() { }", [&](const asCScriptNode& Root)
		{
			TestRunner->TestEqual(TEXT("Mixin parse should produce a function node"), AngelscriptNativeTestSupport::CountNodesOfType(&Root, snFunction), 1);
		});
	}

	TEST_METHOD(NamespaceDeclaration)
	{
		using namespace AngelscriptTest_AngelScriptSDK_ParserDeclarations_Private;
		asCScriptEngine* BareEngine = AngelscriptNativeTestSupport::CreateBareSdkEngine(&*TestRunner);
		if (!TestRunner->TestNotNull(TEXT("Parser declaration test should create a bare engine"), BareEngine))
		{
			return;
		}
		ON_SCOPE_EXIT { BareEngine->ShutDownAndRelease(); };

		ParseScript(*TestRunner, BareEngine, "ParserDeclNamespace", "namespace Gameplay { int Value = 1; }", [&](const asCScriptNode& Root)
		{
			TestRunner->TestEqual(TEXT("Namespace parse should produce a namespace node"), AngelscriptNativeTestSupport::CountNodesOfType(&Root, snNamespace), 1);
			TestRunner->TestEqual(TEXT("Namespace parse should keep nested declarations"), AngelscriptNativeTestSupport::CountNodesOfType(&Root, snDeclaration), 1);
		});
	}

	TEST_METHOD(NestedNamespace)
	{
		using namespace AngelscriptTest_AngelScriptSDK_ParserDeclarations_Private;
		asCScriptEngine* BareEngine = AngelscriptNativeTestSupport::CreateBareSdkEngine(&*TestRunner);
		if (!TestRunner->TestNotNull(TEXT("Parser declaration test should create a bare engine"), BareEngine))
		{
			return;
		}
		ON_SCOPE_EXIT { BareEngine->ShutDownAndRelease(); };

		ParseScript(*TestRunner, BareEngine, "ParserDeclNestedNamespace", "namespace Gameplay::AI { void Tick() { } }", [&](const asCScriptNode& Root)
		{
			TestRunner->TestEqual(TEXT("Nested namespace parse should produce one namespace node"), AngelscriptNativeTestSupport::CountNodesOfType(&Root, snNamespace), 1);
			TestRunner->TestTrue(TEXT("Nested namespace parse should keep both namespace identifiers"), AngelscriptNativeTestSupport::CountNodesOfType(&Root, snIdentifier) >= 2);
		});
	}

	TEST_METHOD(EnumTypedAndUntyped)
	{
		using namespace AngelscriptTest_AngelScriptSDK_ParserDeclarations_Private;
		asCScriptEngine* BareEngine = AngelscriptNativeTestSupport::CreateBareSdkEngine(&*TestRunner);
		if (!TestRunner->TestNotNull(TEXT("Parser declaration test should create a bare engine"), BareEngine))
		{
			return;
		}
		ON_SCOPE_EXIT { BareEngine->ShutDownAndRelease(); };

		ParseScript(*TestRunner, BareEngine, "ParserDeclEnum", "enum EMode { Idle = 0, Run = 1 } enum EOther { One }", [&](const asCScriptNode& Root)
		{
			TestRunner->TestEqual(TEXT("Enum parse should produce two enum nodes"), AngelscriptNativeTestSupport::CountNodesOfType(&Root, snEnum), 2);
		});
	}

	TEST_METHOD(EnumScopedRequireEnumScope)
	{
		using namespace AngelscriptTest_AngelScriptSDK_ParserDeclarations_Private;
		asCScriptEngine* BareEngine = AngelscriptNativeTestSupport::CreateBareSdkEngine(&*TestRunner);
		if (!TestRunner->TestNotNull(TEXT("Parser declaration test should create a bare engine"), BareEngine))
		{
			return;
		}
		ON_SCOPE_EXIT { BareEngine->ShutDownAndRelease(); };

		ParseScript(*TestRunner, BareEngine, "ParserDeclScopedEnum", "enum EMode { Idle, Run }", [&](const asCScriptNode& Root)
		{
			TestRunner->TestEqual(TEXT("Scoped enum parse should produce an enum node"), AngelscriptNativeTestSupport::CountNodesOfType(&Root, snEnum), 1);
		});

		ParseExpression(*TestRunner, BareEngine, "ParserDeclScopedEnumExpr", "EMode::Idle", [&](const asCScriptNode& Root)
		{
			TestRunner->TestTrue(TEXT("Scoped enum expression should produce a dedicated scope node"), AngelscriptNativeTestSupport::CountNodesOfType(&Root, snScope) >= 1);
		});
	}

	TEST_METHOD(TypedefDeclaration)
	{
		using namespace AngelscriptTest_AngelScriptSDK_ParserDeclarations_Private;
		asCScriptEngine* BareEngine = AngelscriptNativeTestSupport::CreateBareSdkEngine(&*TestRunner);
		if (!TestRunner->TestNotNull(TEXT("Parser declaration test should create a bare engine"), BareEngine))
		{
			return;
		}
		ON_SCOPE_EXIT { BareEngine->ShutDownAndRelease(); };

		const int ParseResult = ParseScriptWithResult(BareEngine, "ParserDeclTypedef", "typedef int32 FScore;");
		TestRunner->TestTrue(TEXT("Script-level typedef declarations are currently rejected by the fork tokenizer/parser boundary"), ParseResult < 0);
	}

	TEST_METHOD(FuncdefDeclaration)
	{
		using namespace AngelscriptTest_AngelScriptSDK_ParserDeclarations_Private;
		asCScriptEngine* BareEngine = AngelscriptNativeTestSupport::CreateBareSdkEngine(&*TestRunner);
		if (!TestRunner->TestNotNull(TEXT("Parser declaration test should create a bare engine"), BareEngine))
		{
			return;
		}
		ON_SCOPE_EXIT { BareEngine->ShutDownAndRelease(); };

		const int ParseResult = ParseScriptWithResult(BareEngine, "ParserDeclFuncdef", "funcdef void FCallback(int Value);");
		TestRunner->TestTrue(TEXT("Script-level funcdef declarations are currently rejected by the fork tokenizer/parser boundary"), ParseResult < 0);
	}

	TEST_METHOD(ImportFromDeclaration)
	{
		using namespace AngelscriptTest_AngelScriptSDK_ParserDeclarations_Private;
		asCScriptEngine* BareEngine = AngelscriptNativeTestSupport::CreateBareSdkEngine(&*TestRunner);
		if (!TestRunner->TestNotNull(TEXT("Parser declaration test should create a bare engine"), BareEngine))
		{
			return;
		}
		ON_SCOPE_EXIT { BareEngine->ShutDownAndRelease(); };

		ParseScript(*TestRunner, BareEngine, "ParserDeclImport", "import int SharedValue() from \"OtherModule\";", [&](const asCScriptNode& Root)
		{
			TestRunner->TestEqual(TEXT("Import parse should produce an import node"), AngelscriptNativeTestSupport::CountNodesOfType(&Root, snImport), 1);
		});
	}

	TEST_METHOD(PropertyAccessorGetSet)
	{
		using namespace AngelscriptTest_AngelScriptSDK_ParserDeclarations_Private;
		asCScriptEngine* BareEngine = AngelscriptNativeTestSupport::CreateBareSdkEngine(&*TestRunner);
		if (!TestRunner->TestNotNull(TEXT("Parser declaration test should create a bare engine"), BareEngine))
		{
			return;
		}
		ON_SCOPE_EXIT { BareEngine->ShutDownAndRelease(); };

		// Virtual-property syntax `int X { get { ... } set { } }` was removed by the
		// autoaccessor refactor (see openspec/changes/archive/2026-05-22-refactor-as-remove-autoaccessor).
		// The parser now rejects this form so authors must declare explicit GetX()/SetX() methods.
		const int ParseResult = ParseScriptWithResult(BareEngine, "ParserDeclVirtualProperty", "int Value { get { return 1; } set { } }");
		TestRunner->TestTrue(TEXT("Virtual property syntax should be rejected after autoaccessor removal"), ParseResult < 0);
	}

	TEST_METHOD(OperatorOverloadParse)
	{
		using namespace AngelscriptTest_AngelScriptSDK_ParserDeclarations_Private;
		asCScriptEngine* BareEngine = AngelscriptNativeTestSupport::CreateBareSdkEngine(&*TestRunner);
		if (!TestRunner->TestNotNull(TEXT("Parser declaration test should create a bare engine"), BareEngine))
		{
			return;
		}
		ON_SCOPE_EXIT { BareEngine->ShutDownAndRelease(); };

		ParseScript(*TestRunner, BareEngine, "ParserDeclOperator", "class FNumber { FNumber opAdd(const FNumber& in Other) const { return this; } }", [&](const asCScriptNode& Root)
		{
			TestRunner->TestEqual(TEXT("Operator overload parse should produce one class node"), AngelscriptNativeTestSupport::CountNodesOfType(&Root, snClass), 1);
			TestRunner->TestEqual(TEXT("Operator overload parse should produce one function node"), AngelscriptNativeTestSupport::CountNodesOfType(&Root, snFunction), 1);
		});
	}

	TEST_METHOD(GlobalConstDeclaration)
	{
		using namespace AngelscriptTest_AngelScriptSDK_ParserDeclarations_Private;
		asCScriptEngine* BareEngine = AngelscriptNativeTestSupport::CreateBareSdkEngine(&*TestRunner);
		if (!TestRunner->TestNotNull(TEXT("Parser declaration test should create a bare engine"), BareEngine))
		{
			return;
		}
		ON_SCOPE_EXIT { BareEngine->ShutDownAndRelease(); };

		ParseScript(*TestRunner, BareEngine, "ParserDeclGlobalConst", "const int GlobalValue = 7;", [&](const asCScriptNode& Root)
		{
			TestRunner->TestEqual(TEXT("Global const parse should produce a declaration node"), AngelscriptNativeTestSupport::CountNodesOfType(&Root, snDeclaration), 1);
		});
	}

	TEST_METHOD(ArrayTypeAndHandleType)
	{
		using namespace AngelscriptTest_AngelScriptSDK_ParserDeclarations_Private;
		asCScriptEngine* BareEngine = AngelscriptNativeTestSupport::CreateBareSdkEngine(&*TestRunner);
		if (!TestRunner->TestNotNull(TEXT("Parser declaration test should create a bare engine"), BareEngine))
		{
			return;
		}
		ON_SCOPE_EXIT { BareEngine->ShutDownAndRelease(); };

		if (!RegisterArrayTemplate(*TestRunner, BareEngine, TEXT("Array/handle declaration parse")))
		{
			return;
		}

		const int HandleParseResult = ParseScriptWithResult(BareEngine, "ParserDeclHandleType", "FNode@ Node;");
		TestRunner->TestTrue(TEXT("Bare native parser currently rejects handle declarations because raw @ tokenization is disabled in this fork"), HandleParseResult < 0);

		ParseScript(*TestRunner, BareEngine, "ParserDeclTemplateType", "array<int> Values;", [&](const asCScriptNode& Root)
		{
			TestRunner->TestEqual(TEXT("Template declaration parse should produce one declaration"), AngelscriptNativeTestSupport::CountNodesOfType(&Root, snDeclaration), 1);
			TestRunner->TestTrue(TEXT("Template declaration parse should produce a data type node"), AngelscriptNativeTestSupport::CountNodesOfType(&Root, snDataType) >= 1);
		});
	}

	TEST_METHOD(TemplateInstantiationDeclaration)
	{
		using namespace AngelscriptTest_AngelScriptSDK_ParserDeclarations_Private;
		asCScriptEngine* BareEngine = AngelscriptNativeTestSupport::CreateBareSdkEngine(&*TestRunner);
		if (!TestRunner->TestNotNull(TEXT("Parser declaration test should create a bare engine"), BareEngine))
		{
			return;
		}
		ON_SCOPE_EXIT { BareEngine->ShutDownAndRelease(); };

		if (!RegisterArrayTemplate(*TestRunner, BareEngine, TEXT("Template declaration parse")))
		{
			return;
		}

		ParseScript(*TestRunner, BareEngine, "ParserDeclTemplate", "array<array<int>> Matrix;", [&](const asCScriptNode& Root)
		{
			TestRunner->TestTrue(TEXT("Nested template declaration should produce a declaration node once template support is registered"), AngelscriptNativeTestSupport::CountNodesOfType(&Root, snDeclaration) >= 1);
			TestRunner->TestTrue(TEXT("Nested template declaration should produce nested data type nodes"), AngelscriptNativeTestSupport::CountNodesOfType(&Root, snDataType) >= 2);
		});
	}
};

#endif
