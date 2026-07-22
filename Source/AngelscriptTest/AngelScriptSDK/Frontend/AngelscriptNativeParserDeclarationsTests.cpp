#include "../Support/AngelscriptNativeCoreTestSupport.h"

// Parser declaration behavior coverage.
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



TEST_CLASS_WITH_FLAGS(FParserDeclarationsTests,
	"Angelscript.TestModule.AngelScriptSDK.Frontend.Parser.Declarations",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	static bool ParseDeclScript(FAutomationTestBase& Test, FNoDiscardAsserter& Assert, asCScriptEngine* ScriptEngine, const char* ModuleName, const char* Source, TFunctionRef<void(const asCScriptNode&)> Verify)
	{
		using namespace AngelscriptNativeTestSupport;

		asCModule* Module = CreateSdkModule(ScriptEngine, ModuleName);
		if (!Assert.IsNotNull(Module, FString::Printf(TEXT("%s should create a parser module"), UTF8_TO_TCHAR(ModuleName))))
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
		if (!Assert.IsNotNull(Root, FString::Printf(TEXT("%s should produce a root script node"), UTF8_TO_TCHAR(ModuleName))))
		{
			return false;
		}

		Verify(*Root);
		return true;
	}

	static int ParseDeclScriptWithResult(asCScriptEngine* ScriptEngine, const char* ModuleName, const char* Source)
	{
		using namespace AngelscriptNativeTestSupport;

		asCModule* Module = CreateSdkModule(ScriptEngine, ModuleName);
		asCBuilder Builder(ScriptEngine, Module);
		Builder.silent = true;

		asCScriptCode Code;
		Code.SetCode(ModuleName, Source, true);

		AngelscriptNativeTestSupport::FParserAccessor Parser(&Builder);
		return Parser.ParseScript(&Code);
	}

	static bool ParseDeclExpression(FAutomationTestBase& Test, FNoDiscardAsserter& Assert, asCScriptEngine* ScriptEngine, const char* ModuleName, const char* Source, TFunctionRef<void(const asCScriptNode&)> Verify)
	{
		using namespace AngelscriptNativeTestSupport;

		asCModule* Module = CreateSdkModule(ScriptEngine, ModuleName);
		if (!Assert.IsNotNull(Module, FString::Printf(TEXT("%s should create a parser-expression module"), UTF8_TO_TCHAR(ModuleName))))
		{
			return false;
		}

		asCBuilder Builder(ScriptEngine, Module);
		asCScriptCode Code;
		Code.SetCode(ModuleName, Source, true);

		AngelscriptNativeTestSupport::FParserAccessor Parser(&Builder);
		asCScriptNode* Root = Parser.ParseExpressionSnippet(&Code);
		if (!Assert.IsNotNull(Root, FString::Printf(TEXT("%s should parse an expression root"), UTF8_TO_TCHAR(ModuleName))))
		{
			return false;
		}

		Verify(*Root);
		return true;
	}

	static bool RegisterArrayTemplate(FNoDiscardAsserter& Assert, asCScriptEngine* ScriptEngine, const TCHAR* Context)
	{
		const int RegisterResult = ScriptEngine->RegisterObjectType("array<class T>", 0, asOBJ_REF | asOBJ_TEMPLATE | asOBJ_NOCOUNT);
		return Assert.IsTrue(RegisterResult >= 0, FString::Printf(TEXT("%s should register the parser-only array<T> template type"), Context));
	}

public:
	TEST_METHOD(FunctionWithDefaultParam)
	{
		asCScriptEngine* BareEngine = AngelscriptNativeTestSupport::CreateBareSdkEngine(&*TestRunner);
		ASSERT_THAT(IsNotNull(BareEngine, TEXT("Parser declaration test should create a bare engine")));
		ON_SCOPE_EXIT { BareEngine->ShutDownAndRelease(); };

		ParseDeclScript(*TestRunner, this->Assert, BareEngine, "ParserDeclDefaultParam", "int Add(int A, int B = 2) { return A + B; }", [&](const asCScriptNode& Root)
		{
			ASSERT_THAT(AreEqual(1, AngelscriptNativeTestSupport::CountNodesOfType(&Root, snFunction),
				TEXT("Default-param function parse should produce one function node")));
			ASSERT_THAT(AreEqual(1, AngelscriptNativeTestSupport::CountNodesOfType(&Root, snParameterList),
				TEXT("Default-param function parse should produce a parameter list")));
		});
	}

	TEST_METHOD(FunctionWithInOutInoutRefs)
	{
		asCScriptEngine* BareEngine = AngelscriptNativeTestSupport::CreateBareSdkEngine(&*TestRunner);
		ASSERT_THAT(IsNotNull(BareEngine, TEXT("Parser declaration test should create a bare engine")));
		ON_SCOPE_EXIT { BareEngine->ShutDownAndRelease(); };

		ParseDeclScript(*TestRunner, this->Assert, BareEngine, "ParserDeclRefParams", "void Mutate(int& in A, int& out B, int& inout C) { }", [&](const asCScriptNode& Root)
		{
			ASSERT_THAT(AreEqual(1, AngelscriptNativeTestSupport::CountNodesOfType(&Root, snFunction),
				TEXT("Ref-param parse should produce one function node")));
			ASSERT_THAT(IsTrue(AngelscriptNativeTestSupport::CountNodesOfType(&Root, snDataType) >= 4,
				TEXT("Ref-param parse should include multiple data type nodes")));
		});
	}

	TEST_METHOD(ClassWithInheritance)
	{
		asCScriptEngine* BareEngine = AngelscriptNativeTestSupport::CreateBareSdkEngine(&*TestRunner);
		ASSERT_THAT(IsNotNull(BareEngine, TEXT("Parser declaration test should create a bare engine")));
		ON_SCOPE_EXIT { BareEngine->ShutDownAndRelease(); };

		ParseDeclScript(*TestRunner, this->Assert, BareEngine, "ParserDeclInheritance", "class FBase { } class FDerived : FBase { }", [&](const asCScriptNode& Root)
		{
			ASSERT_THAT(AreEqual(2, AngelscriptNativeTestSupport::CountNodesOfType(&Root, snClass),
				TEXT("Inheritance parse should produce two class nodes")));
		});
	}

	TEST_METHOD(ClassImplementsInterface)
	{
		asCScriptEngine* BareEngine = AngelscriptNativeTestSupport::CreateBareSdkEngine(&*TestRunner);
		ASSERT_THAT(IsNotNull(BareEngine, TEXT("Parser declaration test should create a bare engine")));
		ON_SCOPE_EXIT { BareEngine->ShutDownAndRelease(); };

		const int ParseResult = ParseDeclScriptWithResult(BareEngine, "ParserDeclInterfaceImpl", "interface IThing { void Run(); } class FThing : IThing { void Run() { } }");
		ASSERT_THAT(IsTrue(ParseResult < 0,
			TEXT("Script-level interface declarations are currently rejected by the fork tokenizer/parser boundary")));
	}

	TEST_METHOD(ClassFinalAbstract)
	{
		asCScriptEngine* BareEngine = AngelscriptNativeTestSupport::CreateBareSdkEngine(&*TestRunner);
		ASSERT_THAT(IsNotNull(BareEngine, TEXT("Parser declaration test should create a bare engine")));
		ON_SCOPE_EXIT { BareEngine->ShutDownAndRelease(); };

		ParseDeclScript(*TestRunner, this->Assert, BareEngine, "ParserDeclClassModifiers", "class FFinal { } class FAbstract { }", [&](const asCScriptNode& Root)
		{
			ASSERT_THAT(AreEqual(2, AngelscriptNativeTestSupport::CountNodesOfType(&Root, snClass),
				TEXT("Class modifier parse should keep both class declarations")));
		});
	}

	TEST_METHOD(MixinClassDeclaration)
	{
		asCScriptEngine* BareEngine = AngelscriptNativeTestSupport::CreateBareSdkEngine(&*TestRunner);
		ASSERT_THAT(IsNotNull(BareEngine, TEXT("Parser declaration test should create a bare engine")));
		ON_SCOPE_EXIT { BareEngine->ShutDownAndRelease(); };

		ParseDeclScript(*TestRunner, this->Assert, BareEngine, "ParserDeclMixin", "mixin void SharedUtility() { }", [&](const asCScriptNode& Root)
		{
			ASSERT_THAT(AreEqual(1, AngelscriptNativeTestSupport::CountNodesOfType(&Root, snFunction),
				TEXT("Mixin parse should produce a function node")));
		});
	}

	TEST_METHOD(NamespaceDeclaration)
	{
		asCScriptEngine* BareEngine = AngelscriptNativeTestSupport::CreateBareSdkEngine(&*TestRunner);
		ASSERT_THAT(IsNotNull(BareEngine, TEXT("Parser declaration test should create a bare engine")));
		ON_SCOPE_EXIT { BareEngine->ShutDownAndRelease(); };

		ParseDeclScript(*TestRunner, this->Assert, BareEngine, "ParserDeclNamespace", "namespace Gameplay { int Value = 1; }", [&](const asCScriptNode& Root)
		{
			ASSERT_THAT(AreEqual(1, AngelscriptNativeTestSupport::CountNodesOfType(&Root, snNamespace),
				TEXT("Namespace parse should produce a namespace node")));
			ASSERT_THAT(AreEqual(1, AngelscriptNativeTestSupport::CountNodesOfType(&Root, snDeclaration),
				TEXT("Namespace parse should keep nested declarations")));
		});
	}

	TEST_METHOD(ParserDeclarationsNestedNamespace)
	{
		asCScriptEngine* BareEngine = AngelscriptNativeTestSupport::CreateBareSdkEngine(&*TestRunner);
		ASSERT_THAT(IsNotNull(BareEngine, TEXT("Parser declaration test should create a bare engine")));
		ON_SCOPE_EXIT { BareEngine->ShutDownAndRelease(); };

		ParseDeclScript(*TestRunner, this->Assert, BareEngine, "ParserDeclNestedNamespace", "namespace Gameplay::AI { void Tick() { } }", [&](const asCScriptNode& Root)
		{
			ASSERT_THAT(AreEqual(1, AngelscriptNativeTestSupport::CountNodesOfType(&Root, snNamespace),
				TEXT("Nested namespace parse should produce one namespace node")));
			ASSERT_THAT(IsTrue(AngelscriptNativeTestSupport::CountNodesOfType(&Root, snIdentifier) >= 2,
				TEXT("Nested namespace parse should keep both namespace identifiers")));
		});
	}

	TEST_METHOD(EnumTypedAndUntyped)
	{
		asCScriptEngine* BareEngine = AngelscriptNativeTestSupport::CreateBareSdkEngine(&*TestRunner);
		ASSERT_THAT(IsNotNull(BareEngine, TEXT("Parser declaration test should create a bare engine")));
		ON_SCOPE_EXIT { BareEngine->ShutDownAndRelease(); };

		ParseDeclScript(*TestRunner, this->Assert, BareEngine, "ParserDeclEnum", "enum EMode { Idle = 0, Run = 1 } enum EOther { One }", [&](const asCScriptNode& Root)
		{
			ASSERT_THAT(AreEqual(2, AngelscriptNativeTestSupport::CountNodesOfType(&Root, snEnum),
				TEXT("Enum parse should produce two enum nodes")));
		});
	}

	TEST_METHOD(EnumScopedRequireEnumScope)
	{
		asCScriptEngine* BareEngine = AngelscriptNativeTestSupport::CreateBareSdkEngine(&*TestRunner);
		ASSERT_THAT(IsNotNull(BareEngine, TEXT("Parser declaration test should create a bare engine")));
		ON_SCOPE_EXIT { BareEngine->ShutDownAndRelease(); };

		ParseDeclScript(*TestRunner, this->Assert, BareEngine, "ParserDeclScopedEnum", "enum EMode { Idle, Run }", [&](const asCScriptNode& Root)
		{
			ASSERT_THAT(AreEqual(1, AngelscriptNativeTestSupport::CountNodesOfType(&Root, snEnum),
				TEXT("Scoped enum parse should produce an enum node")));
		});

		ParseDeclExpression(*TestRunner, this->Assert, BareEngine, "ParserDeclScopedEnumExpr", "EMode::Idle", [&](const asCScriptNode& Root)
		{
			ASSERT_THAT(IsTrue(AngelscriptNativeTestSupport::CountNodesOfType(&Root, snScope) >= 1,
				TEXT("Scoped enum expression should produce a dedicated scope node")));
		});
	}

	TEST_METHOD(TypedefDeclaration)
	{
		asCScriptEngine* BareEngine = AngelscriptNativeTestSupport::CreateBareSdkEngine(&*TestRunner);
		ASSERT_THAT(IsNotNull(BareEngine, TEXT("Parser declaration test should create a bare engine")));
		ON_SCOPE_EXIT { BareEngine->ShutDownAndRelease(); };

		const int ParseResult = ParseDeclScriptWithResult(BareEngine, "ParserDeclTypedef", "typedef int32 FScore;");
		ASSERT_THAT(IsTrue(ParseResult < 0,
			TEXT("Script-level typedef declarations are currently rejected by the fork tokenizer/parser boundary")));
	}

	TEST_METHOD(FuncdefDeclaration)
	{
		asCScriptEngine* BareEngine = AngelscriptNativeTestSupport::CreateBareSdkEngine(&*TestRunner);
		ASSERT_THAT(IsNotNull(BareEngine, TEXT("Parser declaration test should create a bare engine")));
		ON_SCOPE_EXIT { BareEngine->ShutDownAndRelease(); };

		const int ParseResult = ParseDeclScriptWithResult(BareEngine, "ParserDeclFuncdef", "funcdef void FCallback(int Value);");
		ASSERT_THAT(IsTrue(ParseResult < 0,
			TEXT("Script-level funcdef declarations are currently rejected by the fork tokenizer/parser boundary")));
	}

	TEST_METHOD(ImportFromDeclaration)
	{
		asCScriptEngine* BareEngine = AngelscriptNativeTestSupport::CreateBareSdkEngine(&*TestRunner);
		ASSERT_THAT(IsNotNull(BareEngine, TEXT("Parser declaration test should create a bare engine")));
		ON_SCOPE_EXIT { BareEngine->ShutDownAndRelease(); };

		ParseDeclScript(*TestRunner, this->Assert, BareEngine, "ParserDeclImport", "import int SharedValue() from \"OtherModule\";", [&](const asCScriptNode& Root)
		{
			ASSERT_THAT(AreEqual(1, AngelscriptNativeTestSupport::CountNodesOfType(&Root, snImport),
				TEXT("Import parse should produce an import node")));
		});
	}

	TEST_METHOD(PropertyAccessorGetSet)
	{
		asCScriptEngine* BareEngine = AngelscriptNativeTestSupport::CreateBareSdkEngine(&*TestRunner);
		ASSERT_THAT(IsNotNull(BareEngine, TEXT("Parser declaration test should create a bare engine")));
		ON_SCOPE_EXIT { BareEngine->ShutDownAndRelease(); };

		// Virtual-property syntax `int X { get { ... } set { } }` was removed by the
		// autoaccessor refactor (see openspec/changes/archive/2026-05-22-refactor-as-remove-autoaccessor).
		// The parser now rejects this form so authors must declare explicit GetX()/SetX() methods.
		const int ParseResult = ParseDeclScriptWithResult(BareEngine, "ParserDeclVirtualProperty", "int Value { get { return 1; } set { } }");
		ASSERT_THAT(IsTrue(ParseResult < 0,
			TEXT("Virtual property syntax should be rejected after autoaccessor removal")));
	}

	TEST_METHOD(OperatorOverloadParse)
	{
		asCScriptEngine* BareEngine = AngelscriptNativeTestSupport::CreateBareSdkEngine(&*TestRunner);
		ASSERT_THAT(IsNotNull(BareEngine, TEXT("Parser declaration test should create a bare engine")));
		ON_SCOPE_EXIT { BareEngine->ShutDownAndRelease(); };

		ParseDeclScript(*TestRunner, this->Assert, BareEngine, "ParserDeclOperator", "class FNumber { FNumber opAdd(const FNumber& in Other) const { return this; } }", [&](const asCScriptNode& Root)
		{
			ASSERT_THAT(AreEqual(1, AngelscriptNativeTestSupport::CountNodesOfType(&Root, snClass),
				TEXT("Operator overload parse should produce one class node")));
			ASSERT_THAT(AreEqual(1, AngelscriptNativeTestSupport::CountNodesOfType(&Root, snFunction),
				TEXT("Operator overload parse should produce one function node")));
		});
	}

	TEST_METHOD(GlobalConstDeclaration)
	{
		asCScriptEngine* BareEngine = AngelscriptNativeTestSupport::CreateBareSdkEngine(&*TestRunner);
		ASSERT_THAT(IsNotNull(BareEngine, TEXT("Parser declaration test should create a bare engine")));
		ON_SCOPE_EXIT { BareEngine->ShutDownAndRelease(); };

		ParseDeclScript(*TestRunner, this->Assert, BareEngine, "ParserDeclGlobalConst", "const int GlobalValue = 7;", [&](const asCScriptNode& Root)
		{
			ASSERT_THAT(AreEqual(1, AngelscriptNativeTestSupport::CountNodesOfType(&Root, snDeclaration),
				TEXT("Global const parse should produce a declaration node")));
		});
	}

	TEST_METHOD(ArrayTypeAndHandleType)
	{
		asCScriptEngine* BareEngine = AngelscriptNativeTestSupport::CreateBareSdkEngine(&*TestRunner);
		ASSERT_THAT(IsNotNull(BareEngine, TEXT("Parser declaration test should create a bare engine")));
		ON_SCOPE_EXIT { BareEngine->ShutDownAndRelease(); };

		if (!RegisterArrayTemplate(this->Assert, BareEngine, TEXT("Array/handle declaration parse")))
		{
			return;
		}

		const int HandleParseResult = ParseDeclScriptWithResult(BareEngine, "ParserDeclHandleType", "FNode@ Node;");
		ASSERT_THAT(IsTrue(HandleParseResult < 0,
			TEXT("Bare native parser currently rejects handle declarations because raw @ tokenization is disabled in this fork")));

		ParseDeclScript(*TestRunner, this->Assert, BareEngine, "ParserDeclTemplateType", "array<int> Values;", [&](const asCScriptNode& Root)
		{
			ASSERT_THAT(AreEqual(1, AngelscriptNativeTestSupport::CountNodesOfType(&Root, snDeclaration),
				TEXT("Template declaration parse should produce one declaration")));
			ASSERT_THAT(IsTrue(AngelscriptNativeTestSupport::CountNodesOfType(&Root, snDataType) >= 1,
				TEXT("Template declaration parse should produce a data type node")));
		});
	}

	TEST_METHOD(TemplateInstantiationDeclaration)
	{
		asCScriptEngine* BareEngine = AngelscriptNativeTestSupport::CreateBareSdkEngine(&*TestRunner);
		ASSERT_THAT(IsNotNull(BareEngine, TEXT("Parser declaration test should create a bare engine")));
		ON_SCOPE_EXIT { BareEngine->ShutDownAndRelease(); };

		if (!RegisterArrayTemplate(this->Assert, BareEngine, TEXT("Template declaration parse")))
		{
			return;
		}

		ParseDeclScript(*TestRunner, this->Assert, BareEngine, "ParserDeclTemplate", "array<array<int>> NestedValues;", [&](const asCScriptNode& Root)
		{
			ASSERT_THAT(IsTrue(AngelscriptNativeTestSupport::CountNodesOfType(&Root, snDeclaration) >= 1,
				TEXT("Nested template declaration should produce a declaration node once template support is registered")));
			ASSERT_THAT(IsTrue(AngelscriptNativeTestSupport::CountNodesOfType(&Root, snDataType) >= 2,
				TEXT("Nested template declaration should produce nested data type nodes")));
		});
	}
};

#endif
