#include "AngelscriptTestMacros.h"
#include "../Support/AngelscriptNativeCoreTestSupport.h"
#include "../Support/AngelscriptNativeLanguageCaseTestSupport.h"
#include "../Support/AngelscriptNativeParserDepthTestSupport.h"

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

template <typename TDerived, typename TAsserter>
class TParserCartesianDepthTestSupport : public TParserDepthLifecycleTestSupport<TDerived, TAsserter>
{
protected:
	struct FDeclarationCase
	{
		const TCHAR* Id;
		const TCHAR* Source;
		eScriptNode ExpectedNode;
		int32 ExpectedCount;
	};

	struct FSnippetCase
	{
		const TCHAR* Id;
		const TCHAR* Source;
		eScriptNode ExpectedRoot;
		eScriptNode ExpectedChild;
	};

	struct FParameterShape
	{
		const TCHAR* Id;
		const TCHAR* Parameters;
		const TCHAR* ParameterReturn;
	};

	struct FBodyShape
	{
		const TCHAR* Id;
		const TCHAR* Body;
	};

	struct FExpressionShape
	{
		const TCHAR* Id;
		const TCHAR* Source;
		int32 MinimumOperatorCount;
	};

	struct FExpressionGrouping
	{
		const TCHAR* Id;
		const TCHAR* Prefix;
		const TCHAR* Suffix;
		int32 AdditionalOperatorCount;
	};

	struct FSemanticExpressionShape
	{
		const TCHAR* Id;
		eScriptNode ExpectedNode;
	};

	static FString MakeSemanticExpression(
		const int32 ShapeIndex,
		const int32 PlacementIndex)
	{
		using namespace AngelscriptNativeTestSupport;

		FString Expression;
		switch (ShapeIndex)
		{
		case 0:
			Expression = TEXT("Cast<int>(Value)");
			break;
		case 1:
			Expression = TEXT("Values[Index + 1]");
			break;
		case 2:
			Expression = TEXT("DoWork(Value: 3)");
			break;
		case 3:
			Expression = TEXT("{ 1, 2, 3 }");
			break;
		default:
			AppendGeneratedAsLine(Expression, TEXT("function()"));
			AppendGeneratedAsLine(Expression, TEXT("{"));
			AppendGeneratedAsLine(Expression, TEXT("\treturn 1;"));
			AppendGeneratedAsLine(Expression, TEXT("}"));
			Expression.RemoveFromEnd(LINE_TERMINATOR);
			break;
		}

		if (PlacementIndex == 0)
		{
			return Expression;
		}

		const TCHAR* Prefix = PlacementIndex == 1
			? TEXT("(")
			: TEXT("Consume(");
		FString Source = Prefix;
		Source += Expression;
		Source += TEXT(")");
		return Source;
	}

	static FString MakeDeclarationSource(const int32 CaseIndex)
	{
		using namespace AngelscriptNativeTestSupport;

		FString Source;
		switch (CaseIndex)
		{
		case 0:
			AppendGeneratedAsLine(Source, TEXT("int Add(int A, int B = 2)"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\treturn A + B;"));
			AppendGeneratedAsLine(Source, TEXT("}"));
			break;
		case 1:
			AppendGeneratedAsLine(Source, TEXT("void Mutate(int& in A, int& out B, int& inout C)"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\tB = A;"));
			AppendGeneratedAsLine(Source, TEXT("\tC = C + 1;"));
			AppendGeneratedAsLine(Source, TEXT("}"));
			break;
		case 2:
			AppendGeneratedAsLine(Source, TEXT("class FBase"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("}"));
			AppendGeneratedAsLine(Source);
			AppendGeneratedAsLine(Source, TEXT("class FDerived : FBase"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("}"));
			break;
		case 3:
			AppendGeneratedAsLine(Source, TEXT("namespace Gameplay"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\tint Value = 1;"));
			AppendGeneratedAsLine(Source, TEXT("}"));
			break;
		case 4:
			AppendGeneratedAsLine(Source, TEXT("enum EMode"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\tIdle = 0,"));
			AppendGeneratedAsLine(Source, TEXT("\tRun = 1"));
			AppendGeneratedAsLine(Source, TEXT("}"));
			break;
		case 5:
			AppendGeneratedAsLine(Source, TEXT("class FCarrier"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\tint Value;"));
			AppendGeneratedAsLine(Source);
			AppendGeneratedAsLine(Source, TEXT("\tint Read()"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t\treturn Value;"));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
			AppendGeneratedAsLine(Source, TEXT("}"));
			break;
		default:
			AppendGeneratedAsLine(Source, TEXT("const int Score = 7;"));
			break;
		}

		return Source;
	}

};

TEST_CLASS_WITH_BASE_AND_FLAGS(FParserCartesianDepthTests,
	"Angelscript.TestModule.AngelScriptSDK.Frontend.ParserCartesianDepth",
	TParserCartesianDepthTestSupport,
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	TEST_METHOD(DeclarationFamiliesRetainNodeKinds)
	{
		using namespace AngelscriptNativeTestSupport;
		AS_NATIVE_PRODUCT("FRONTEND-PARSER-DECLARATION-FAMILIES",
			ENativeEvidence::Compile | ENativeEvidence::Metadata | ENativeEvidence::Cleanup);

		const FDeclarationCase Cases[] =
		{
			{ TEXT("default_parameter"), TEXT(""), snFunction, 1 },
			{ TEXT("reference_directions"), TEXT(""), snFunction, 1 },
			{ TEXT("inheritance"), TEXT(""), snClass, 2 },
			{ TEXT("namespace"), TEXT(""), snNamespace, 1 },
			{ TEXT("enum"), TEXT(""), snEnum, 1 },
			{ TEXT("class_member_function"), TEXT(""), snClass, 1 },
			{ TEXT("const_global"), TEXT(""), snDeclaration, 1 },
		};

		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};

		asCScriptEngine* ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Parser declaration products should use the case-owned raw SDK engine")));
		if (ScriptEngine == nullptr)
		{
			return;
		}

		for (int32 CaseIndex = 0; CaseIndex < UE_ARRAY_COUNT(Cases); ++CaseIndex)
		{
			const FDeclarationCase& Case = Cases[CaseIndex];
			const FString Source = MakeDeclarationSource(CaseIndex);
			const FString SourceId = FString::Printf(TEXT("FRONTEND-PARSER-DECLARATION-FAMILIES-%s"), Case.Id);
			const FString ModuleName = FString::Printf(TEXT("ParserDeclarationFamily_%d"), CaseIndex);
			PrintGeneratedAsSource(*TestRunner, SourceId, ModuleName, Source);

			TUniquePtr<asCBuilder> Builder;
			TUniquePtr<FParserAccessor> Parser;
			asCScriptNode* Root = nullptr;
			const bool bParsed = ParseScriptCase(this->Assert, ScriptEngine, ModuleName, Source, Root, Builder, Parser);
			ASSERT_THAT(IsTrue(bParsed, FString::Printf(TEXT("Declaration family %s should parse"), Case.Id)));
			if (bParsed)
			{
				ASSERT_THAT(AreEqual(Case.ExpectedCount, CountNodesOfType(Root, Case.ExpectedNode),
					FString::Printf(TEXT("Declaration family %s should retain its expected node count"), Case.Id)));
				ASSERT_THAT(IsTrue(ValidateSiblingLinks(Root),
					FString::Printf(TEXT("Declaration family %s should retain valid parent and sibling links"), Case.Id)));
			}
			ASSERT_THAT(IsTrue(ReleaseParserCase(
				this->Assert,
				ScriptEngine,
				ModuleName,
				Root,
				Parser,
				Builder),
				FString::Printf(TEXT("Declaration family %s should release its parser state"), Case.Id)));
		}
	}

};

TEST_CLASS_WITH_BASE_AND_FLAGS(FParserFunctionCartesianDepthTests,
	"Angelscript.TestModule.AngelScriptSDK.Frontend.ParserCartesianDepth",
	TParserCartesianDepthTestSupport,
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	TEST_METHOD(FunctionParameterBodyAndLineEndingCartesianProduct)
	{
		using namespace AngelscriptNativeTestSupport;
		AS_NATIVE_PRODUCT("FRONTEND-PARSER-FUNCTION-PARAMETER-BODY-LINE-ENDINGS",
			ENativeEvidence::Compile | ENativeEvidence::Metadata | ENativeEvidence::Cleanup);

		const FParameterShape ParameterShapes[] =
		{
			{ TEXT("scalar"), TEXT("int A"), TEXT("return A;") },
			{ TEXT("default"), TEXT("int A, int B = 2"), TEXT("return A + B;") },
			{ TEXT("reference_directions"), TEXT("int& in A, int& out B, int& inout C"), TEXT("B = A; C = B; return C;") },
			{ TEXT("multiple_default"), TEXT("int A, int B, int C = 3"), TEXT("return A + B + C;") },
		};
		const FBodyShape BodyShapes[] =
		{
			{ TEXT("literal"), TEXT("\treturn 1;") },
			{ TEXT("parameter_use"), TEXT("") },
			{ TEXT("branch_return"), TEXT("\tif (true)\n\t{\n\t\treturn 1;\n\t}\n\treturn 0;") },
		};
		const TCHAR* LineEndingIds[] = { TEXT("lf"), TEXT("crlf") };
		const FString LineFeed = FString::Chr(10);
		const FString CarriageReturnLineFeed = FString::Chr(13) + LineFeed;

		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};

		asCScriptEngine* ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Function parameter products should use the case-owned raw SDK engine")));
		if (ScriptEngine == nullptr)
		{
			return;
		}

		int32 ObservedCaseCount = 0;
		for (const FParameterShape& ParameterShape : ParameterShapes)
		{
			for (const FBodyShape& BodyShape : BodyShapes)
			{
				for (int32 LineEndingIndex = 0; LineEndingIndex < UE_ARRAY_COUNT(LineEndingIds); ++LineEndingIndex)
				{
					FString Body = BodyShape.Body;
					if (FCString::Strcmp(BodyShape.Id, TEXT("parameter_use")) == 0)
					{
						Body = FString::Printf(TEXT("\t%s"), ParameterShape.ParameterReturn);
					}

					FString Source;
					AppendGeneratedAsLine(Source, FString::Printf(TEXT("int Generated(%s)"), ParameterShape.Parameters));
					AppendGeneratedAsLine(Source, TEXT("{"));
					TArray<FString> BodyLines;
					Body.ParseIntoArrayLines(BodyLines, false);
					for (const FString& BodyLine : BodyLines)
					{
						AppendGeneratedAsLine(Source, BodyLine);
					}
					AppendGeneratedAsLine(Source, TEXT("}"));
					if (LineEndingIndex == 1)
					{
						Source.ReplaceInline(*LineFeed, *CarriageReturnLineFeed);
					}

					const FString SourceId = FString::Printf(
						TEXT("FRONTEND-PARSER-FUNCTION-PARAMETER-BODY-LINE-ENDINGS-%s-%s-%s"),
						ParameterShape.Id,
						BodyShape.Id,
						LineEndingIds[LineEndingIndex]);
					const FString ModuleName = FString::Printf(
						TEXT("ParserFunctionParameter_%s_%s_%s"),
						ParameterShape.Id,
						BodyShape.Id,
						LineEndingIds[LineEndingIndex]);
					PrintGeneratedAsSource(*TestRunner, SourceId, ModuleName, Source);

					TUniquePtr<asCBuilder> Builder;
					TUniquePtr<FParserAccessor> Parser;
					asCScriptNode* Root = nullptr;
					const bool bParsed = ParseScriptCase(this->Assert, ScriptEngine, ModuleName, Source, Root, Builder, Parser);
					ASSERT_THAT(IsTrue(bParsed, FString::Printf(TEXT("Function parameter cell %s should parse"), *SourceId)));
					if (bParsed)
					{
						ASSERT_THAT(AreEqual(1, CountNodesOfType(Root, snFunction),
							FString::Printf(TEXT("Function parameter cell %s should publish one function node"), *SourceId)));
						ASSERT_THAT(IsTrue(ValidateSiblingLinks(Root),
							FString::Printf(TEXT("Function parameter cell %s should preserve AST links"), *SourceId)));
					}
					ASSERT_THAT(IsTrue(ReleaseParserCase(
						this->Assert,
						ScriptEngine,
						ModuleName,
						Root,
						Parser,
						Builder),
						FString::Printf(TEXT("Function parameter cell %s should release its parser state"), *SourceId)));
					++ObservedCaseCount;
				}
			}
		}

		ASSERT_THAT(AreEqual(24, ObservedCaseCount,
			TEXT("Parameter shape × body shape × line ending should execute every cell")));
	}

};

TEST_CLASS_WITH_BASE_AND_FLAGS(FParserExpressionGroupingDepthTests,
	"Angelscript.TestModule.AngelScriptSDK.Frontend.ParserCartesianDepth",
	TParserCartesianDepthTestSupport,
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	TEST_METHOD(ExpressionOperatorGroupingCartesianProduct)
	{
		using namespace AngelscriptNativeTestSupport;
		AS_NATIVE_PRODUCT("FRONTEND-PARSER-EXPRESSION-OPERATOR-GROUPING",
			ENativeEvidence::Compile | ENativeEvidence::Metadata | ENativeEvidence::Cleanup);

		const FExpressionShape ExpressionShapes[] =
		{
			{ TEXT("additive"), TEXT("1 + 2 - 3"), 2 },
			{ TEXT("multiplicative"), TEXT("1 * 2 / 3"), 2 },
			{ TEXT("shift_additive"), TEXT("1 << 2 + 1"), 2 },
			{ TEXT("comparison_equality"), TEXT("1 < 2 == true"), 2 },
			{ TEXT("logical"), TEXT("true && false || true"), 2 },
			{ TEXT("bitwise"), TEXT("1 & 2 ^ 3 | 4"), 3 },
			{ TEXT("power"), TEXT("2 ** 3 ** 2"), 2 },
			{ TEXT("member_chain"), TEXT("Object.Member.Value"), 0 },
		};
		const FExpressionGrouping Groupings[] =
		{
			{ TEXT("bare"), TEXT(""), TEXT(""), 0 },
			{ TEXT("parenthesized"), TEXT("("), TEXT(")"), 0 },
			{ TEXT("chained"), TEXT("("), TEXT(") + 0"), 1 },
		};

		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};

		asCScriptEngine* ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Expression grouping products should use the case-owned raw SDK engine")));
		if (ScriptEngine == nullptr)
		{
			return;
		}

		int32 ObservedCaseCount = 0;
		for (const FExpressionShape& ExpressionShape : ExpressionShapes)
		{
			for (const FExpressionGrouping& Grouping : Groupings)
			{
				const FString Source = FString::Printf(TEXT("%s%s%s"), Grouping.Prefix, ExpressionShape.Source, Grouping.Suffix);
				const FString SourceId = FString::Printf(
					TEXT("FRONTEND-PARSER-EXPRESSION-OPERATOR-GROUPING-%s-%s"),
					ExpressionShape.Id,
					Grouping.Id);
				const FString ModuleName = FString::Printf(TEXT("ParserExpressionGrouping_%s_%s"), ExpressionShape.Id, Grouping.Id);
				PrintGeneratedAsSource(*TestRunner, SourceId, ModuleName, Source);

				asCModule* Module = CreateSdkModule(ScriptEngine, TCHAR_TO_UTF8(*ModuleName));
				ASSERT_THAT(IsNotNull(Module, FString::Printf(TEXT("Expression grouping cell %s should create a module"), *SourceId)));
				if (Module == nullptr)
				{
					continue;
				}

				TUniquePtr<asCBuilder> Builder = MakeUnique<asCBuilder>(ScriptEngine, Module);
				const FTCHARToUTF8 SourceUtf8(*Source);
				asCScriptCode Code;
				Code.SetCode(TCHAR_TO_UTF8(*ModuleName), SourceUtf8.Get(), true);
				TUniquePtr<FParserAccessor> Parser = MakeUnique<FParserAccessor>(Builder.Get());
				asCScriptNode* Root = Parser->ParseExpressionSnippet(&Code);
				ASSERT_THAT(IsNotNull(Root, FString::Printf(TEXT("Expression grouping cell %s should publish a root"), *SourceId)));
				if (Root != nullptr)
				{
					ASSERT_THAT(AreEqual(static_cast<int32>(snExpression), static_cast<int32>(Root->nodeType),
						FString::Printf(TEXT("Expression grouping cell %s should retain an expression root"), *SourceId)));
					const int32 MinimumOperators = ExpressionShape.MinimumOperatorCount + Grouping.AdditionalOperatorCount;
					ASSERT_THAT(IsTrue(CountNodesOfType(Root, snExprOperator) >= MinimumOperators,
						FString::Printf(TEXT("Expression grouping cell %s should retain operator precedence nodes"), *SourceId)));
				}
				ASSERT_THAT(IsTrue(ReleaseParserCase(
					this->Assert,
					ScriptEngine,
					ModuleName,
					Root,
					Parser,
					Builder),
					FString::Printf(TEXT("Expression grouping cell %s should release its parser state"), *SourceId)));
				++ObservedCaseCount;
			}
		}

		ASSERT_THAT(AreEqual(24, ObservedCaseCount,
			TEXT("Expression operator shape × grouping should execute every cell")));
	}

};

TEST_CLASS_WITH_BASE_AND_FLAGS(FParserSemanticExpressionDepthTests,
	"Angelscript.TestModule.AngelScriptSDK.Frontend.ParserCartesianDepth",
	TParserCartesianDepthTestSupport,
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	TEST_METHOD(SemanticExpressionShapesByPlacement)
	{
		using namespace AngelscriptNativeTestSupport;
		AS_NATIVE_PRODUCT("FRONTEND-PARSER-SEMANTIC-EXPRESSION-PLACEMENT",
			ENativeEvidence::Compile | ENativeEvidence::Metadata | ENativeEvidence::Cleanup | ENativeEvidence::Isolation);

		const FSemanticExpressionShape Shapes[] =
		{
			{ TEXT("cast"), snCast },
			{ TEXT("index"), snArgList },
			{ TEXT("named_argument"), snNamedArgument },
			{ TEXT("initializer_list"), snInitList },
			{ TEXT("anonymous_function"), snFunction },
		};
		const TCHAR* PlacementIds[] =
		{
			TEXT("bare"),
			TEXT("parenthesized"),
			TEXT("call_argument"),
		};

		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};

		asCScriptEngine* ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Semantic expression products should use the case-owned raw SDK engine")));
		if (ScriptEngine == nullptr)
		{
			return;
		}

		int32 ObservedCaseCount = 0;
		for (int32 ShapeIndex = 0; ShapeIndex < UE_ARRAY_COUNT(Shapes); ++ShapeIndex)
		{
			const FSemanticExpressionShape& Shape = Shapes[ShapeIndex];
			for (int32 PlacementIndex = 0; PlacementIndex < UE_ARRAY_COUNT(PlacementIds); ++PlacementIndex)
			{
				const FString Source = MakeSemanticExpression(ShapeIndex, PlacementIndex);
				const FString SourceId = FString::Printf(
					TEXT("FRONTEND-PARSER-SEMANTIC-EXPRESSION-PLACEMENT-%s-%s"),
					Shape.Id,
					PlacementIds[PlacementIndex]);
				const FString ModuleName = FString::Printf(
					TEXT("ParserSemanticExpression_%s_%s"),
					Shape.Id,
					PlacementIds[PlacementIndex]);
				PrintGeneratedAsSource(*TestRunner, SourceId, ModuleName, Source);

				asCModule* Module = CreateSdkModule(ScriptEngine, TCHAR_TO_UTF8(*ModuleName));
				ASSERT_THAT(IsNotNull(Module, FString::Printf(TEXT("Semantic expression cell %s should create a parser module"), *SourceId)));
				if (Module == nullptr)
				{
					continue;
				}

				TUniquePtr<asCBuilder> Builder = MakeUnique<asCBuilder>(ScriptEngine, Module);
				const FTCHARToUTF8 SourceUtf8(*Source);
				asCScriptCode Code;
				Code.SetCode(TCHAR_TO_UTF8(*ModuleName), SourceUtf8.Get(), true);
				TUniquePtr<FParserAccessor> Parser = MakeUnique<FParserAccessor>(Builder.Get());
				asCScriptNode* Root = Parser->ParseExpressionSnippet(&Code);
				ASSERT_THAT(IsNotNull(Root, FString::Printf(TEXT("Semantic expression cell %s should publish a root"), *SourceId)));
				if (Root != nullptr)
				{
					ASSERT_THAT(AreEqual(
						static_cast<int32>(snExpression),
						static_cast<int32>(Root->nodeType),
						FString::Printf(TEXT("Semantic expression cell %s should retain an expression root"), *SourceId)));
					ASSERT_THAT(IsTrue(
						CountNodesOfType(Root, Shape.ExpectedNode) >= 1,
						FString::Printf(TEXT("Semantic expression cell %s should retain its owning AST node"), *SourceId)));
					if (PlacementIndex == 2)
					{
						ASSERT_THAT(IsTrue(
							CountNodesOfType(Root, snFunctionCall) >= 1,
							FString::Printf(TEXT("Semantic expression cell %s should retain its outer call placement"), *SourceId)));
					}
					ASSERT_THAT(IsTrue(
						ValidateSiblingLinks(Root),
						FString::Printf(TEXT("Semantic expression cell %s should preserve parent and sibling links"), *SourceId)));
				}
				ASSERT_THAT(IsTrue(ReleaseParserCase(
					this->Assert,
					ScriptEngine,
					ModuleName,
					Root,
					Parser,
					Builder),
					FString::Printf(TEXT("Semantic expression cell %s should release its parser state"), *SourceId)));
				++ObservedCaseCount;
			}
		}

		ASSERT_THAT(AreEqual(
			15,
			ObservedCaseCount,
			TEXT("Semantic expression shape × placement should execute every legal cell")));

		const FString IsolationSource = TEXT("IsolationValue + 1");
		const FString IsolationModuleName = TEXT("ParserSemanticExpressionIsolationControl");
		PrintGeneratedAsSource(
			*TestRunner,
			TEXT("FRONTEND-PARSER-SEMANTIC-EXPRESSION-PLACEMENT-isolation-control"),
			IsolationModuleName,
			IsolationSource);
		asCModule* IsolationModule = CreateSdkModule(ScriptEngine, TCHAR_TO_UTF8(*IsolationModuleName));
		ASSERT_THAT(IsNotNull(
			IsolationModule,
			TEXT("Semantic expression isolation control should create a clean module after every generated cell")));
		if (IsolationModule != nullptr)
		{
			TUniquePtr<asCBuilder> IsolationBuilder = MakeUnique<asCBuilder>(ScriptEngine, IsolationModule);
			const FTCHARToUTF8 IsolationSourceUtf8(*IsolationSource);
			asCScriptCode IsolationCode;
			IsolationCode.SetCode(TCHAR_TO_UTF8(*IsolationModuleName), IsolationSourceUtf8.Get(), true);
			TUniquePtr<FParserAccessor> IsolationParser = MakeUnique<FParserAccessor>(IsolationBuilder.Get());
			asCScriptNode* IsolationRoot = IsolationParser->ParseExpressionSnippet(&IsolationCode);
			ASSERT_THAT(IsNotNull(
				IsolationRoot,
				TEXT("Semantic expression isolation control should parse after the generated product")));
			if (IsolationRoot != nullptr)
			{
				ASSERT_THAT(AreEqual(
					1,
					CountNodesOfType(IsolationRoot, snExprOperator),
					TEXT("Semantic expression isolation control should retain only its own additive operator")));
				ASSERT_THAT(AreEqual(
					0,
					CountNodesOfType(IsolationRoot, snFunctionCall),
					TEXT("Semantic expression isolation control should not retain a prior call placement")));
			}
			ASSERT_THAT(IsTrue(ReleaseParserCase(
				this->Assert,
				ScriptEngine,
				IsolationModuleName,
				IsolationRoot,
				IsolationParser,
				IsolationBuilder),
				TEXT("Semantic expression isolation control should release its independent parser state")));
		}
	}

};

TEST_CLASS_WITH_BASE_AND_FLAGS(FParserExpressionStatementDepthTests,
	"Angelscript.TestModule.AngelScriptSDK.Frontend.ParserCartesianDepth",
	TParserCartesianDepthTestSupport,
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	TEST_METHOD(ExpressionAndStatementFamiliesRetainRoots)
	{
		using namespace AngelscriptNativeTestSupport;
		AS_NATIVE_PRODUCT("FRONTEND-PARSER-EXPRESSION-STATEMENT-FAMILIES",
			ENativeEvidence::Compile | ENativeEvidence::Metadata | ENativeEvidence::Cleanup);

		const FSnippetCase ExpressionCases[] =
		{
			{ TEXT("arithmetic_precedence"), TEXT("1 + 2 * 3"), snExpression, snExprOperator },
			{ TEXT("nested_ternary"), TEXT("true ? 1 : (false ? 2 : 3)"), snCondition, snCondition },
			{ TEXT("member_chain"), TEXT("Object.Member.Value"), snExpression, snVariableAccess },
		};
		const FSnippetCase StatementCases[] =
		{
			{ TEXT("if_branch"), TEXT("if (true) { int Value = 1; }"), snIf, snDeclaration },
			{ TEXT("for_loop"), TEXT("for (int Index = 0; Index < 2; ++Index) { }"), snFor, snExpressionStatement },
			{ TEXT("while_loop"), TEXT("while (false) { break; }"), snWhile, snBreak },
		};

		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};

		asCScriptEngine* ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Parser expression products should use the case-owned raw SDK engine")));
		if (ScriptEngine == nullptr)
		{
			return;
		}

		for (int32 CaseIndex = 0; CaseIndex < UE_ARRAY_COUNT(ExpressionCases); ++CaseIndex)
		{
			const FSnippetCase& Case = ExpressionCases[CaseIndex];
			const FString Source = Case.Source;
			const FString SourceId = FString::Printf(TEXT("FRONTEND-PARSER-EXPRESSION-STATEMENT-FAMILIES-%s"), Case.Id);
			const FString ModuleName = FString::Printf(TEXT("ParserExpressionFamily_%d"), CaseIndex);
			PrintGeneratedAsSource(*TestRunner, SourceId, ModuleName, Source);

			asCModule* Module = CreateSdkModule(ScriptEngine, TCHAR_TO_UTF8(*ModuleName));
			ASSERT_THAT(IsNotNull(Module, TEXT("Expression family should create a parser module")));
			if (Module == nullptr)
			{
				continue;
			}
			TUniquePtr<asCBuilder> Builder = MakeUnique<asCBuilder>(ScriptEngine, Module);
			asCScriptCode Code;
			const FTCHARToUTF8 SourceUtf8(*Source);
			Code.SetCode(TCHAR_TO_UTF8(*ModuleName), SourceUtf8.Get(), true);
			TUniquePtr<FParserAccessor> Parser = MakeUnique<FParserAccessor>(Builder.Get());
			asCScriptNode* Root = Case.ExpectedRoot == snCondition
				? Parser->ParseConditionSnippet(&Code)
				: Parser->ParseExpressionSnippet(&Code);
			ASSERT_THAT(IsNotNull(Root, FString::Printf(TEXT("Expression family %s should produce a root"), Case.Id)));
			if (Root != nullptr)
			{
				ASSERT_THAT(AreEqual(static_cast<int32>(Case.ExpectedRoot), static_cast<int32>(Root->nodeType),
					FString::Printf(TEXT("Expression family %s should retain its root node"), Case.Id)));
				ASSERT_THAT(IsTrue(CountNodesOfType(Root, Case.ExpectedChild) >= 1,
					FString::Printf(TEXT("Expression family %s should retain its child node"), Case.Id)));
			}
			ASSERT_THAT(IsTrue(ReleaseParserCase(
				this->Assert,
				ScriptEngine,
				ModuleName,
				Root,
				Parser,
				Builder),
				FString::Printf(TEXT("Expression family %s should release its parser state"), Case.Id)));
		}

		for (int32 CaseIndex = 0; CaseIndex < UE_ARRAY_COUNT(StatementCases); ++CaseIndex)
		{
			const FSnippetCase& Case = StatementCases[CaseIndex];
			const FString Source = Case.Source;
			const FString SourceId = FString::Printf(TEXT("FRONTEND-PARSER-EXPRESSION-STATEMENT-FAMILIES-%s"), Case.Id);
			const FString ModuleName = FString::Printf(TEXT("ParserStatementFamily_%d"), CaseIndex);
			PrintGeneratedAsSource(*TestRunner, SourceId, ModuleName, Source);

			asCModule* Module = CreateSdkModule(ScriptEngine, TCHAR_TO_UTF8(*ModuleName));
			ASSERT_THAT(IsNotNull(Module, TEXT("Statement family should create a parser module")));
			if (Module == nullptr)
			{
				continue;
			}
			TUniquePtr<asCBuilder> Builder = MakeUnique<asCBuilder>(ScriptEngine, Module);
			asCScriptCode Code;
			const FTCHARToUTF8 SourceUtf8(*Source);
			Code.SetCode(TCHAR_TO_UTF8(*ModuleName), SourceUtf8.Get(), true);
			TUniquePtr<FParserAccessor> Parser = MakeUnique<FParserAccessor>(Builder.Get());
			asCScriptNode* Root = Parser->ParseStatementSnippet(&Code);
			ASSERT_THAT(IsNotNull(Root, FString::Printf(TEXT("Statement family %s should produce a root"), Case.Id)));
			if (Root != nullptr)
			{
				ASSERT_THAT(AreEqual(static_cast<int32>(Case.ExpectedRoot), static_cast<int32>(Root->nodeType),
					FString::Printf(TEXT("Statement family %s should retain its root node"), Case.Id)));
				ASSERT_THAT(IsTrue(CountNodesOfType(Root, Case.ExpectedChild) >= 1,
					FString::Printf(TEXT("Statement family %s should retain its child node"), Case.Id)));
			}
			ASSERT_THAT(IsTrue(ReleaseParserCase(
				this->Assert,
				ScriptEngine,
				ModuleName,
				Root,
				Parser,
				Builder),
				FString::Printf(TEXT("Statement family %s should release its parser state"), Case.Id)));
		}
	}

};

#endif // WITH_ANGELSCRIPT_UNITTESTS
