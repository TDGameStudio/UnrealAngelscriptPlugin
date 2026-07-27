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
class TParserScriptNodeDepthTestSupport : public TParserDepthLifecycleTestSupport<TDerived, TAsserter>
{
protected:
	static int32 CountSiblings(const asCScriptNode* Node)
	{
		int32 Count = 0;
		for (const asCScriptNode* Current = Node; Current != nullptr; Current = Current->next)
		{
			++Count;
		}
		return Count;
	}

	static bool HistogramsMatch(const TMap<eScriptNode, int32>& Left, const TMap<eScriptNode, int32>& Right)
	{
		if (Left.Num() != Right.Num())
		{
			return false;
		}

		for (const TPair<eScriptNode, int32>& Pair : Left)
		{
			if (Right.FindRef(Pair.Key) != Pair.Value)
			{
				return false;
			}
		}

		return true;
	}

	static FString MakeNestedSource(const TCHAR* Shape, const int32 Depth)
	{
		using namespace AngelscriptNativeTestSupport;

		FString Source;
		FString Indent;
		if (FCString::Strcmp(Shape, TEXT("namespace")) == 0)
		{
			for (int32 Index = 0; Index < Depth; ++Index)
			{
				AppendGeneratedAsLine(Source, FString::Printf(TEXT("%snamespace Scope%d"), *Indent, Index));
				AppendGeneratedAsLine(Source, FString::Printf(TEXT("%s{"), *Indent));
				Indent += TEXT("\t");
			}

			AppendGeneratedAsLine(Source, FString::Printf(TEXT("%sint Value = 1;"), *Indent));
			for (int32 Index = 0; Index < Depth; ++Index)
			{
				Indent.LeftChopInline(1);
				AppendGeneratedAsLine(Source, FString::Printf(TEXT("%s}"), *Indent));
			}
			return Source;
		}

		Indent.Reset();
		for (int32 Index = 0; Index < Depth; ++Index)
		{
			const TCHAR* Keyword = FCString::Strcmp(Shape, TEXT("while")) == 0
				? TEXT("while (false)")
				: TEXT("if (true)");
			AppendGeneratedAsLine(Source, FString::Printf(TEXT("%s%s"), *Indent, Keyword));
			AppendGeneratedAsLine(Source, FString::Printf(TEXT("%s{"), *Indent));
			Indent += TEXT("\t");
		}

		AppendGeneratedAsLine(Source, FString::Printf(TEXT("%sint Value = 1;"), *Indent));
		for (int32 Index = 0; Index < Depth; ++Index)
		{
			Indent.LeftChopInline(1);
			AppendGeneratedAsLine(Source, FString::Printf(TEXT("%s}"), *Indent));
		}
		return Source;
	}
};

TEST_CLASS_WITH_BASE_AND_FLAGS(FParserNodeNestingDepthTests,
	"Angelscript.TestModule.AngelScriptSDK.Frontend.ParserCartesianDepth",
	TParserScriptNodeDepthTestSupport,
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	TEST_METHOD(NodeDeepNestingAndCopyCartesianProduct)
	{
		using namespace AngelscriptNativeTestSupport;
		AS_NATIVE_PRODUCT("FRONTEND-NODE-DEEP-NESTING-COPY",
			ENativeEvidence::Compile | ENativeEvidence::Metadata | ENativeEvidence::Cleanup);

		struct FNestingShape
		{
			const TCHAR* Id;
			eScriptNode ExpectedNode;
		};
		const FNestingShape Shapes[] =
		{
			{ TEXT("if"), snIf },
			{ TEXT("while"), snWhile },
			{ TEXT("namespace"), snNamespace },
		};
		const int32 Depths[] = { 1, 2, 4, 8 };

		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};

		asCScriptEngine* ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Deep node products should use the case-owned raw SDK engine")));
		if (ScriptEngine == nullptr)
		{
			return;
		}

		int32 ObservedCaseCount = 0;
		for (const FNestingShape& Shape : Shapes)
		{
			for (const int32 Depth : Depths)
			{
				const FString Source = MakeNestedSource(Shape.Id, Depth);
				const FString SourceId = FString::Printf(TEXT("FRONTEND-NODE-DEEP-NESTING-COPY-%s-%d"), Shape.Id, Depth);
				const FString ModuleName = FString::Printf(TEXT("ParserNodeDeep_%s_%d"), Shape.Id, Depth);
				PrintGeneratedAsSource(*TestRunner, SourceId, ModuleName, Source);

				TUniquePtr<asCBuilder> Builder;
				TUniquePtr<FParserAccessor> Parser;
				asCScriptNode* Root = nullptr;
				bool bParsed = false;
				if (FCString::Strcmp(Shape.Id, TEXT("namespace")) == 0)
				{
					bParsed = ParseScriptCase(this->Assert, ScriptEngine, ModuleName, Source, Root, Builder, Parser);
				}
				else
				{
					asCModule* Module = CreateSdkModule(ScriptEngine, TCHAR_TO_UTF8(*ModuleName));
					ASSERT_THAT(IsNotNull(Module, FString::Printf(TEXT("Deep statement cell %s should create a module"), *SourceId)));
					if (Module != nullptr)
					{
						Builder = MakeUnique<asCBuilder>(ScriptEngine, Module);
						const FTCHARToUTF8 SourceUtf8(*Source);
						asCScriptCode Code;
						Code.SetCode(TCHAR_TO_UTF8(*ModuleName), SourceUtf8.Get(), true);
						Parser = MakeUnique<FParserAccessor>(Builder.Get());
						Root = Parser->ParseStatementSnippet(&Code);
						bParsed = Root != nullptr;
					}
				}
				ASSERT_THAT(IsTrue(bParsed, FString::Printf(TEXT("Deep node cell %s should parse"), *SourceId)));
				if (bParsed)
				{
					ASSERT_THAT(AreEqual(Depth, CountNodesOfType(Root, Shape.ExpectedNode),
						FString::Printf(TEXT("Deep node cell %s should retain each nested %s node"), *SourceId, Shape.Id)));
					ASSERT_THAT(IsTrue(MaxNodeDepth(Root) >= Depth + 2,
						FString::Printf(TEXT("Deep node cell %s should retain its requested depth"), *SourceId)));

					asCScriptNode* Copy = Root->CreateCopy(Parser->MemStack, ScriptEngine);
					ASSERT_THAT(IsNotNull(Copy, FString::Printf(TEXT("Deep node cell %s should create a copy"), *SourceId)));
					if (Copy != nullptr)
					{
						ASSERT_THAT(IsTrue(ValidateSiblingLinks(Copy),
							FString::Printf(TEXT("Deep node cell %s copy should retain sibling links"), *SourceId)));
						ASSERT_THAT(IsTrue(HistogramsMatch(NodeTypeHistogram(Root), NodeTypeHistogram(Copy)),
							FString::Printf(TEXT("Deep node cell %s copy should retain its node histogram"), *SourceId)));
						ASSERT_THAT(AreEqual(MaxNodeDepth(Root), MaxNodeDepth(Copy),
							FString::Printf(TEXT("Deep node cell %s copy should retain its maximum depth"), *SourceId)));
					}
				}
				ASSERT_THAT(IsTrue(ReleaseParserCase(
					this->Assert,
					ScriptEngine,
					ModuleName,
					Root,
					Parser,
					Builder),
					FString::Printf(TEXT("Deep node cell %s should release its parser state"), *SourceId)));
				++ObservedCaseCount;
			}
		}

		ASSERT_THAT(AreEqual(12, ObservedCaseCount,
			TEXT("Node shape × depth should execute every deep-copy cell")));
	}

};

TEST_CLASS_WITH_BASE_AND_FLAGS(FParserNodeTraversalDepthTests,
	"Angelscript.TestModule.AngelScriptSDK.Frontend.ParserCartesianDepth",
	TParserScriptNodeDepthTestSupport,
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	TEST_METHOD(NodeTraversalAndCopyPreserveStructure)
	{
		using namespace AngelscriptNativeTestSupport;
		AS_NATIVE_PRODUCT("FRONTEND-NODE-TRAVERSAL-COPY",
			ENativeEvidence::Compile | ENativeEvidence::Metadata | ENativeEvidence::Cleanup);

		const FString Sources[] =
		{
			TEXT("int A = 1; int B = 2; int C = 3;"),
			TEXT("void Run() { if (true) { return; } while (false) { break; } }"),
			TEXT("class FNode { int Value; int Read() { return Value; } }"),
			TEXT("enum ENode { One, Two } namespace Scope { int Value = 4; }"),
		};

		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};

		asCScriptEngine* ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Node products should use the case-owned raw SDK engine")));
		if (ScriptEngine == nullptr)
		{
			return;
		}

		for (int32 CaseIndex = 0; CaseIndex < UE_ARRAY_COUNT(Sources); ++CaseIndex)
		{
			const FString Source = Sources[CaseIndex];
			const FString SourceId = FString::Printf(TEXT("FRONTEND-NODE-TRAVERSAL-COPY-%d"), CaseIndex);
			const FString ModuleName = FString::Printf(TEXT("ParserNodeCopy_%d"), CaseIndex);
			PrintGeneratedAsSource(*TestRunner, SourceId, ModuleName, Source);

			asCModule* Module = CreateSdkModule(ScriptEngine, TCHAR_TO_UTF8(*ModuleName));
			ASSERT_THAT(IsNotNull(Module, TEXT("Node traversal source should create a parser module")));
			if (Module == nullptr)
			{
				continue;
			}
			{
				asCBuilder Builder(ScriptEngine, Module);
				const FTCHARToUTF8 SourceUtf8(*Source);
				asCScriptCode Code;
				Code.SetCode(TCHAR_TO_UTF8(*ModuleName), SourceUtf8.Get(), true);
				FParserAccessor Parser(&Builder);
				const int ParseResult = Parser.ParseScript(&Code);
				ASSERT_THAT(AreEqual(0, ParseResult, FString::Printf(TEXT("Node source %d should parse"), CaseIndex)));
				asCScriptNode* Root = Parser.GetScriptNode();
				ASSERT_THAT(IsNotNull(Root, TEXT("Node source should expose a root")));
				if (Root != nullptr)
				{
					ASSERT_THAT(IsTrue(ValidateSiblingLinks(Root), TEXT("Original AST links should be internally consistent")));
					asCScriptNode* Copy = Root->CreateCopy(Parser.MemStack, ScriptEngine);
					ASSERT_THAT(IsNotNull(Copy, TEXT("CreateCopy should duplicate the parsed tree")));
					if (Copy != nullptr)
					{
						ASSERT_THAT(IsTrue(HistogramsMatch(NodeTypeHistogram(Root), NodeTypeHistogram(Copy)),
							TEXT("Copied AST should preserve the complete node-type histogram")));
						ASSERT_THAT(AreEqual(MaxNodeDepth(Root), MaxNodeDepth(Copy),
							TEXT("Copied AST should preserve maximum nesting depth")));
						ASSERT_THAT(AreEqual(CountSiblings(Root->firstChild), CountSiblings(Copy->firstChild),
							TEXT("Copied AST should preserve top-level sibling count")));
					}
				}
			}

			const FTCHARToUTF8 ModuleNameUtf8(*ModuleName);
			ASSERT_THAT(AreEqual(
				asSUCCESS,
				ScriptEngine->DiscardModule(ModuleNameUtf8.Get()),
				FString::Printf(TEXT("Node source %d should discard after its parser and copied tree are released"), CaseIndex)));
			ASSERT_THAT(IsNull(
				ScriptEngine->GetModule(ModuleNameUtf8.Get(), asGM_ONLY_IF_EXISTS),
				FString::Printf(TEXT("Node source %d should leave no name-visible module"), CaseIndex)));
		}
	}

};

#endif // WITH_ANGELSCRIPT_UNITTESTS
