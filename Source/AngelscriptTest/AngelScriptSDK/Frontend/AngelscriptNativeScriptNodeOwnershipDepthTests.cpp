#include "../Support/AngelscriptNativeCoreTestSupport.h"
#include "../Support/AngelscriptNativeLanguageCaseTestSupport.h"
#include "AngelscriptTestMacros.h"

#include "CQTest.h"
#include "Misc/MemStack.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_builder.h"
#include "source/as_module.h"
#include "source/as_parser.h"
#include "source/as_scriptcode.h"
#include "source/as_scriptengine.h"
#include "source/as_scriptnode.h"
#include "EndAngelscriptHeaders.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FScriptNodeOwnershipDepthTests,
	"Angelscript.TestModule.AngelScriptSDK.Frontend.ScriptNode.OwnershipDepth",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	struct FRangeValidationResult
	{
		bool bValid = true;
		int32 TotalNodes = 0;
		int32 NodesWithRanges = 0;
		int32 StrictContainments = 0;
		int32 BoundaryContainments = 0;
		int32 MaximumDepth = 0;
		FString Failure;
	};

	struct FNodeFingerprint
	{
		int32 NodeType = 0;
		int32 TokenType = 0;
		uint64 TokenPos = 0;
		uint64 TokenLength = 0;
		int32 Row = 0;
		int32 Column = 0;
		int32 ParentIndex = -1;
		int32 PreviousIndex = -1;
		int32 NextIndex = -1;
	};

	static bool IsRangeContainer(const eScriptNode NodeType)
	{
		switch (NodeType)
		{
		case snFunction:
		case snClass:
		case snStatementBlock:
		case snIf:
		case snFor:
		case snWhile:
		case snExpression:
			return true;
		default:
			return false;
		}
	}

	static bool HasStrictDescendantRange(
		const asCScriptNode* Node,
		const uint64 ContainerStart,
		const uint64 ContainerEnd)
	{
		for (const asCScriptNode* Current = Node; Current != nullptr; Current = Current->next)
		{
			if (Current->tokenLength > 0)
			{
				const uint64 DescendantStart = static_cast<uint64>(Current->tokenPos);
				const uint64 DescendantEnd = DescendantStart + static_cast<uint64>(Current->tokenLength);
				if (DescendantStart > ContainerStart && DescendantEnd < ContainerEnd)
				{
					return true;
				}
			}

			if (HasStrictDescendantRange(Current->firstChild, ContainerStart, ContainerEnd))
			{
				return true;
			}
		}

		return false;
	}

	static void FailRange(
		FRangeValidationResult& Result,
		const FString& SourceId,
		const asCScriptNode& Parent,
		const asCScriptNode& Child,
		const TCHAR* Reason)
	{
		if (!Result.bValid)
		{
			return;
		}

		Result.bValid = false;
		Result.Failure = FString::Printf(
			TEXT("%s: %s parent=%d[%llu,%llu) child=%d[%llu,%llu)"),
			*SourceId,
			Reason,
			static_cast<int32>(Parent.nodeType),
			static_cast<unsigned long long>(Parent.tokenPos),
			static_cast<unsigned long long>(Parent.tokenPos + Parent.tokenLength),
			static_cast<int32>(Child.nodeType),
			static_cast<unsigned long long>(Child.tokenPos),
			static_cast<unsigned long long>(Child.tokenPos + Child.tokenLength));
	}

	static void ValidateNestedRanges(
		const asCScriptNode* Node,
		const asCScriptCode& Code,
		const FString& SourceId,
		const int32 Depth,
		FRangeValidationResult& Result)
	{
		for (const asCScriptNode* Current = Node;
			Current != nullptr && Result.bValid;
			Current = Current->next)
		{
			++Result.TotalNodes;
			Result.MaximumDepth = FMath::Max(Result.MaximumDepth, Depth);
			if (Current->tokenLength > 0)
			{
				++Result.NodesWithRanges;
				if (Current->tokenPos + Current->tokenLength > Code.codeLength)
				{
					Result.bValid = false;
					Result.Failure = FString::Printf(
						TEXT("%s: node range exceeds source length node=%d start=%llu length=%llu sourceLength=%llu"),
						*SourceId,
						static_cast<int32>(Current->nodeType),
						static_cast<unsigned long long>(Current->tokenPos),
						static_cast<unsigned long long>(Current->tokenLength),
						static_cast<unsigned long long>(Code.codeLength));
					break;
				}
			}

			const asCScriptNode* Child = Current->firstChild;
			bool bHasStrictChild = false;
			for (; Child != nullptr && Result.bValid; Child = Child->next)
			{
				if (Child->tokenLength == 0 || Current->tokenLength == 0)
				{
					continue;
				}

				const uint64 ParentStart = static_cast<uint64>(Current->tokenPos);
				const uint64 ParentEnd = ParentStart + static_cast<uint64>(Current->tokenLength);
				const uint64 ChildStart = static_cast<uint64>(Child->tokenPos);
				const uint64 ChildEnd = ChildStart + static_cast<uint64>(Child->tokenLength);
				if (ChildStart < ParentStart || ChildEnd > ParentEnd)
				{
					FailRange(Result, SourceId, *Current, *Child, TEXT("child range is outside parent range"));
					break;
				}

				if (ChildStart > ParentStart && ChildEnd < ParentEnd)
				{
					++Result.StrictContainments;
					bHasStrictChild = true;
				}
				else
				{
					++Result.BoundaryContainments;
				}
			}

			const bool bExpressionHasOperator = Current->nodeType == snExpression
				&& Current->firstChild != nullptr
				&& Current->firstChild->next != nullptr;
			if (Result.bValid
				&& IsRangeContainer(Current->nodeType)
				&& Current->firstChild != nullptr
				&& (Current->nodeType != snExpression || bExpressionHasOperator))
			{
				if (!bHasStrictChild)
				{
					// Expression wrappers can share one boundary with their first
					// term, but every semantic container must also expose at least
					// one genuinely nested source span.
					const uint64 ParentStart = static_cast<uint64>(Current->tokenPos);
					const uint64 ParentEnd = ParentStart + static_cast<uint64>(Current->tokenLength);
					bHasStrictChild = HasStrictDescendantRange(
						Current->firstChild,
						ParentStart,
						ParentEnd);

					if (!bHasStrictChild)
					{
						Result.bValid = false;
						Result.Failure = FString::Printf(
							TEXT("%s: semantic container has no strict nested source span node=%d range=[%llu,%llu)"),
							*SourceId,
							static_cast<int32>(Current->nodeType),
							static_cast<unsigned long long>(Current->tokenPos),
							static_cast<unsigned long long>(Current->tokenPos + Current->tokenLength));
					}
				}
			}

			if (Result.bValid)
			{
				ValidateNestedRanges(Current->firstChild, Code, SourceId, Depth + 1, Result);
			}
		}
	}

	static bool ValidateTreeLinks(
		const asCScriptNode* Node,
		const asCScriptNode* ExpectedParent,
		TSet<const asCScriptNode*>& Seen)
	{
		const asCScriptNode* Previous = nullptr;
		const asCScriptNode* Last = nullptr;
		for (const asCScriptNode* Current = Node; Current != nullptr; Current = Current->next)
		{
			if (Seen.Contains(Current)
				|| Current->parent != ExpectedParent
				|| Current->prev != Previous
				|| (Current->next != nullptr && Current->next->prev != Current)
				|| (Current->prev != nullptr && Current->prev->next != Current))
			{
				return false;
			}

			Seen.Add(Current);
			if (Current->firstChild == nullptr)
			{
				if (Current->lastChild != nullptr)
				{
					return false;
				}
			}
			else
			{
				if (Current->firstChild->prev != nullptr
					|| Current->lastChild == nullptr
					|| Current->lastChild->next != nullptr
					|| !ValidateTreeLinks(Current->firstChild, Current, Seen))
				{
					return false;
				}

				const asCScriptNode* ChildTail = Current->firstChild;
				while (ChildTail->next != nullptr)
				{
					ChildTail = ChildTail->next;
				}
				if (ChildTail != Current->lastChild)
				{
					return false;
				}
			}

			Previous = Current;
			Last = Current;
		}

		return Node == nullptr || Last == nullptr || Last->next == nullptr;
	}

	static void CaptureNodeFingerprints(
		const asCScriptNode* Node,
		asCScriptCode& Code,
		TArray<FNodeFingerprint>& OutRecords,
		TMap<const asCScriptNode*, int32>& OutIndices)
	{
		for (const asCScriptNode* Current = Node; Current != nullptr; Current = Current->next)
		{
			const int32 Index = OutRecords.AddDefaulted();
			OutIndices.Add(Current, Index);
			FNodeFingerprint& Record = OutRecords[Index];
			Record.NodeType = static_cast<int32>(Current->nodeType);
			Record.TokenType = static_cast<int32>(Current->tokenType);
			Record.TokenPos = static_cast<uint64>(Current->tokenPos);
			Record.TokenLength = static_cast<uint64>(Current->tokenLength);
			Code.ConvertPosToRowCol(Current->tokenPos, &Record.Row, &Record.Column);

			CaptureNodeFingerprints(Current->firstChild, Code, OutRecords, OutIndices);
		}

		for (int32 Index = 0; Index < OutRecords.Num(); ++Index)
		{
			const asCScriptNode* NodeForRecord = nullptr;
			for (const TPair<const asCScriptNode*, int32>& Pair : OutIndices)
			{
				if (Pair.Value == Index)
				{
					NodeForRecord = Pair.Key;
					break;
				}
			}

			if (NodeForRecord == nullptr)
			{
				continue;
			}

			const int32* ParentIndex = OutIndices.Find(NodeForRecord->parent);
			const int32* PreviousIndex = OutIndices.Find(NodeForRecord->prev);
			const int32* NextIndex = OutIndices.Find(NodeForRecord->next);
			FNodeFingerprint& MutableRecord = OutRecords[Index];
			MutableRecord.ParentIndex = ParentIndex != nullptr ? *ParentIndex : -1;
			MutableRecord.PreviousIndex = PreviousIndex != nullptr ? *PreviousIndex : -1;
			MutableRecord.NextIndex = NextIndex != nullptr ? *NextIndex : -1;
		}
	}

	static bool FingerprintsMatch(
		const TArray<FNodeFingerprint>& Left,
		const TArray<FNodeFingerprint>& Right)
	{
		if (Left.Num() != Right.Num())
		{
			return false;
		}

		for (int32 Index = 0; Index < Left.Num(); ++Index)
		{
			const FNodeFingerprint& L = Left[Index];
			const FNodeFingerprint& R = Right[Index];
			if (L.NodeType != R.NodeType
				|| L.TokenType != R.TokenType
				|| L.TokenPos != R.TokenPos
				|| L.TokenLength != R.TokenLength
				|| L.Row != R.Row
				|| L.Column != R.Column
				|| L.ParentIndex != R.ParentIndex
				|| L.PreviousIndex != R.PreviousIndex
				|| L.NextIndex != R.NextIndex)
			{
				return false;
			}
		}

		return true;
	}

	static bool HistogramsMatch(
		const TMap<eScriptNode, int32>& Left,
		const TMap<eScriptNode, int32>& Right)
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

	static FString BuildRangeSource(const int32 ShapeIndex)
	{
		using namespace AngelscriptNativeTestSupport;
		FString Source;
		switch (ShapeIndex)
		{
		case 0:
			AppendGeneratedAsLine(Source, TEXT("int Evaluate(int Value)"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\tif (Value > 0)"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t\twhile (Value > 1)"));
			AppendGeneratedAsLine(Source, TEXT("\t\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t\t\tValue = Value - 1;"));
			AppendGeneratedAsLine(Source, TEXT("\t\t}"));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
			AppendGeneratedAsLine(Source, TEXT("\treturn Value + 1;"));
			AppendGeneratedAsLine(Source, TEXT("}"));
			break;
		case 1:
			AppendGeneratedAsLine(Source, TEXT("class FRangeCarrier"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\tint Value;"));
			AppendGeneratedAsLine(Source);
			AppendGeneratedAsLine(Source, TEXT("\tint Read()"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t\tif (Value > 0)"));
			AppendGeneratedAsLine(Source, TEXT("\t\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t\t\treturn Value + 1;"));
			AppendGeneratedAsLine(Source, TEXT("\t\t}"));
			AppendGeneratedAsLine(Source, TEXT("\t\treturn 0;"));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
			AppendGeneratedAsLine(Source, TEXT("}"));
			break;
		case 2:
			AppendGeneratedAsLine(Source, TEXT("void Control()"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\tfor (int Index = 0; Index < 2; Index = Index + 1)"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t\tif (Index == 1)"));
			AppendGeneratedAsLine(Source, TEXT("\t\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t\t\tbreak;"));
			AppendGeneratedAsLine(Source, TEXT("\t\t}"));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
			AppendGeneratedAsLine(Source, TEXT("}"));
			break;
		default:
			AppendGeneratedAsLine(Source, TEXT("int Expression()"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\treturn (1 + 2) * (3 + 4) > 0 ? 5 : 6;"));
			AppendGeneratedAsLine(Source, TEXT("}"));
			break;
		}

		return Source;
	}

	static FString BuildOwnershipSource()
	{
		using namespace AngelscriptNativeTestSupport;
		FString Source;
		AppendGeneratedAsLine(Source, TEXT("class FOwnershipCarrier"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tint Value;"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("\tint Compute(int Input)"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(Source, TEXT("\t\tif (Input > 0)"));
		AppendGeneratedAsLine(Source, TEXT("\t\t{"));
		AppendGeneratedAsLine(Source, TEXT("\t\t\twhile (Input > 1)"));
		AppendGeneratedAsLine(Source, TEXT("\t\t\t{"));
		AppendGeneratedAsLine(Source, TEXT("\t\t\t\tInput = Input - 1;"));
		AppendGeneratedAsLine(Source, TEXT("\t\t\t}"));
		AppendGeneratedAsLine(Source, TEXT("\t\t}"));
		AppendGeneratedAsLine(Source, TEXT("\t\treturn (Input + Value) * 2;"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("int GlobalValue = 3;"));
		return Source;
	}

public:
	TEST_METHOD(NestedSourceRangesRemainStrictlyContained)
	{
		using namespace AngelscriptNativeTestSupport;
		AS_NATIVE_PRODUCT("FRONTEND-NODE-SOURCE-RANGE-CONTAINMENT",
			ENativeEvidence::Compile
			| ENativeEvidence::Metadata
			| ENativeEvidence::Cleanup
			| ENativeEvidence::Isolation);

		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};

		asCScriptEngine* ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("ScriptNode range product should use the case-owned raw SDK engine")));
		if (ScriptEngine == nullptr)
		{
			return;
		}

		const FString LineFeed = FString::Chr(10);
		const FString CarriageReturnLineFeed = FString::Chr(13) + LineFeed;
		const TCHAR* const ShapeIds[] = { TEXT("FUNCTION"), TEXT("CLASS"), TEXT("CONTROL"), TEXT("EXPRESSION") };
		const TCHAR* const LineEndingIds[] = { TEXT("LF"), TEXT("CRLF") };
		int32 ObservedCases = 0;

		for (int32 ShapeIndex = 0; ShapeIndex < UE_ARRAY_COUNT(ShapeIds); ++ShapeIndex)
		{
			for (int32 LineEndingIndex = 0; LineEndingIndex < UE_ARRAY_COUNT(LineEndingIds); ++LineEndingIndex)
			{
				FString Source = BuildRangeSource(ShapeIndex);
				if (LineEndingIndex == 1)
				{
					Source.ReplaceInline(*LineFeed, *CarriageReturnLineFeed);
				}

				const FString SourceId = FString::Printf(
					TEXT("FRONTEND-NODE-SOURCE-RANGE-CONTAINMENT-%s-%s"),
					ShapeIds[ShapeIndex],
					LineEndingIds[LineEndingIndex]);
				const FString ModuleName = FString::Printf(
					TEXT("ScriptNodeOwnershipRange_%s_%s"),
					ShapeIds[ShapeIndex],
					LineEndingIds[LineEndingIndex]);
				PrintGeneratedAsSource(*TestRunner, SourceId, ModuleName, Source);

				const FTCHARToUTF8 ModuleNameUtf8(*ModuleName);
				const FTCHARToUTF8 SourceUtf8(*Source);
				asCModule* Module = CreateSdkModule(ScriptEngine, ModuleNameUtf8.Get());
				ASSERT_THAT(IsNotNull(Module,
					FString::Printf(TEXT("%s should create a parser module"), *SourceId)));
				if (Module == nullptr)
				{
					continue;
				}

				bool bExplicitCleanupComplete = false;
				ON_SCOPE_EXIT
				{
					if (!bExplicitCleanupComplete)
					{
						ScriptEngine->DiscardModule(ModuleNameUtf8.Get());
					}
				};

				TUniquePtr<asCBuilder> Builder = MakeUnique<asCBuilder>(ScriptEngine, Module);
				asCScriptCode Code;
				Code.SetCode(ModuleNameUtf8.Get(), SourceUtf8.Get(), true);
				TUniquePtr<FParserAccessor> Parser = MakeUnique<FParserAccessor>(Builder.Get());
				const int ParseResult = Parser->ParseScript(&Code);
				ASSERT_THAT(AreEqual(0, ParseResult,
					FString::Printf(TEXT("%s should parse before range inspection"), *SourceId)));
				if (ParseResult != 0)
				{
					continue;
				}

				asCScriptNode* Root = Parser->GetScriptNode();
				ASSERT_THAT(IsNotNull(Root,
					FString::Printf(TEXT("%s should publish a script root"), *SourceId)));
				if (Root == nullptr)
				{
					continue;
				}

				FRangeValidationResult Validation;
				ValidateNestedRanges(Root, Code, SourceId, 1, Validation);
				ASSERT_THAT(IsTrue(Validation.bValid, Validation.Failure));
				ASSERT_THAT(IsTrue(Validation.TotalNodes >= 8,
					FString::Printf(TEXT("%s should expose a non-trivial nested node set"), *SourceId)));
				ASSERT_THAT(IsTrue(Validation.NodesWithRanges >= 4,
					FString::Printf(TEXT("%s should expose several independently ranged nodes"), *SourceId)));
				ASSERT_THAT(IsTrue(Validation.StrictContainments >= 1,
					FString::Printf(TEXT("%s should prove at least one strictly nested source span"), *SourceId)));
				ASSERT_THAT(IsTrue(
					Validation.StrictContainments + Validation.BoundaryContainments >= 3,
					FString::Printf(TEXT("%s should prove several parent/child range relationships"), *SourceId)));
				ASSERT_THAT(IsTrue(Validation.MaximumDepth >= 3,
					FString::Printf(TEXT("%s should retain nested semantic depth"), *SourceId)));

				Root = nullptr;
				Parser.Reset();
				Builder.Reset();
				ASSERT_THAT(AreEqual(
					asSUCCESS,
					ScriptEngine->DiscardModule(ModuleNameUtf8.Get()),
					FString::Printf(TEXT("%s should discard after its parser tree is released"), *SourceId)));
				bExplicitCleanupComplete = true;
				ASSERT_THAT(IsNull(
					ScriptEngine->GetModule(ModuleNameUtf8.Get(), asGM_ONLY_IF_EXISTS),
					FString::Printf(TEXT("%s should leave no name-visible source-range module"), *SourceId)));
				++ObservedCases;
			}
		}

		ASSERT_THAT(AreEqual(8, ObservedCases,
			TEXT("Source-range ownership coverage should execute every shape and line-ending case")));

		const FString ControlSourceId = TEXT("FRONTEND-NODE-SOURCE-RANGE-CONTAINMENT-ISOLATION-CONTROL");
		const FString ControlModuleName = TEXT("ScriptNodeOwnershipRangeIsolationControl");
		const FString ControlSource = TEXT("int ControlValue = 19;");
		PrintGeneratedAsSource(*TestRunner, ControlSourceId, ControlModuleName, ControlSource);
		const FTCHARToUTF8 ControlModuleNameUtf8(*ControlModuleName);
		const FTCHARToUTF8 ControlSourceUtf8(*ControlSource);
		asCModule* ControlModule = CreateSdkModule(ScriptEngine, ControlModuleNameUtf8.Get());
		ASSERT_THAT(IsNotNull(
			ControlModule,
			TEXT("Source-range containment isolation control should create an independent module")));
		if (ControlModule != nullptr)
		{
			{
				asCBuilder ControlBuilder(ScriptEngine, ControlModule);
				asCScriptCode ControlCode;
				ControlCode.SetCode(ControlModuleNameUtf8.Get(), ControlSourceUtf8.Get(), true);
				FParserAccessor ControlParser(&ControlBuilder);
				ASSERT_THAT(AreEqual(
					0,
					ControlParser.ParseScript(&ControlCode),
					TEXT("Source-range containment isolation control should parse after every generated cell")));
				const asCScriptNode* ControlRoot = ControlParser.GetScriptNode();
				ASSERT_THAT(IsNotNull(
					ControlRoot,
					TEXT("Source-range containment isolation control should publish its own root")));
				if (ControlRoot != nullptr)
				{
					ASSERT_THAT(AreEqual(
						1,
						CountNodesOfType(ControlRoot, snDeclaration),
						TEXT("Source-range containment isolation control should contain only its own declaration")));
					ASSERT_THAT(AreEqual(
						0,
						CountNodesOfType(ControlRoot, snFunction),
						TEXT("Source-range containment isolation control should not retain a prior function tree")));
				}
			}
			ASSERT_THAT(AreEqual(
				asSUCCESS,
				ScriptEngine->DiscardModule(ControlModuleNameUtf8.Get()),
				TEXT("Source-range containment isolation control should discard after parser release")));
			ASSERT_THAT(IsNull(
				ScriptEngine->GetModule(ControlModuleNameUtf8.Get(), asGM_ONLY_IF_EXISTS),
				TEXT("Source-range containment isolation control should leave no name-visible module")));
		}
	}

	TEST_METHOD(DeepCopyOwnsIndependentTreeAfterOriginalRelease)
	{
		using namespace AngelscriptNativeTestSupport;
		AS_NATIVE_PRODUCT("FRONTEND-NODE-COPY-OWNERSHIP-INDEPENDENCE",
			ENativeEvidence::Compile
			| ENativeEvidence::Metadata
			| ENativeEvidence::Cleanup
			| ENativeEvidence::Isolation);

		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};

		asCScriptEngine* ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("ScriptNode ownership product should use the case-owned raw SDK engine")));
		if (ScriptEngine == nullptr)
		{
			return;
		}

		const FString Source = BuildOwnershipSource();
		const FString SourceId = TEXT("FRONTEND-NODE-COPY-OWNERSHIP-INDEPENDENCE-DEEP-SOURCE");
		const FString ModuleName = TEXT("ScriptNodeOwnershipCopy");
		PrintGeneratedAsSource(*TestRunner, SourceId, ModuleName, Source);

		const FTCHARToUTF8 ModuleNameUtf8(*ModuleName);
		const FTCHARToUTF8 SourceUtf8(*Source);
		asCModule* Module = CreateSdkModule(ScriptEngine, ModuleNameUtf8.Get());
		ASSERT_THAT(IsNotNull(Module, TEXT("Deep-copy source should create a parser module")));
		if (Module == nullptr)
		{
			return;
		}

		ON_SCOPE_EXIT
		{
			ScriptEngine->DiscardModule(ModuleNameUtf8.Get());
		};

		asCScriptCode Code;
		Code.SetCode(ModuleNameUtf8.Get(), SourceUtf8.Get(), true);
		FMemStackBase CopyMemStack;
		asCScriptNode* CopiedRoot = nullptr;
		TArray<FNodeFingerprint> CopyBeforeRelease;
		TMap<const asCScriptNode*, int32> CopyIndices;
		TMap<eScriptNode, int32> CopyHistogramBeforeRelease;
		int32 CopyDepthBeforeRelease = 0;
		int32 OriginalNodeCount = 0;
		bool bOriginalMutationObserved = false;

		{
			asCBuilder Builder(ScriptEngine, Module);
			FParserAccessor Parser(&Builder);
			const int ParseResult = Parser.ParseScript(&Code);
			ASSERT_THAT(AreEqual(0, ParseResult, TEXT("Deep-copy source should parse successfully")));
			if (ParseResult != 0)
			{
				return;
			}

			asCScriptNode* OriginalRoot = Parser.GetScriptNode();
			ASSERT_THAT(IsNotNull(OriginalRoot, TEXT("Deep-copy source should expose its original root")));
			if (OriginalRoot == nullptr)
			{
				return;
			}

			TArray<FNodeFingerprint> OriginalBeforeCopy;
			TMap<const asCScriptNode*, int32> OriginalIndices;
			CaptureNodeFingerprints(OriginalRoot, Code, OriginalBeforeCopy, OriginalIndices);
			OriginalNodeCount = OriginalBeforeCopy.Num();
			ASSERT_THAT(IsTrue(OriginalNodeCount >= 20,
				TEXT("Deep-copy fixture should create enough nested nodes to exercise ownership")));
			TSet<const asCScriptNode*> OriginalSeen;
			ASSERT_THAT(IsTrue(ValidateTreeLinks(OriginalRoot, nullptr, OriginalSeen),
				TEXT("Original tree should expose complete parent and sibling links")));

			CopiedRoot = OriginalRoot->CreateCopy(CopyMemStack, ScriptEngine);
			ASSERT_THAT(IsNotNull(CopiedRoot, TEXT("CreateCopy should allocate an independent root")));
			if (CopiedRoot == nullptr)
			{
				return;
			}

			CaptureNodeFingerprints(CopiedRoot, Code, CopyBeforeRelease, CopyIndices);
			ASSERT_THAT(AreEqual(OriginalNodeCount, CopyBeforeRelease.Num(),
				TEXT("Copy should contain every original node")));
			ASSERT_THAT(IsTrue(FingerprintsMatch(OriginalBeforeCopy, CopyBeforeRelease),
				TEXT("Copy should preserve token data, positions, depth order, and links")));
			TSet<const asCScriptNode*> CopySeen;
			ASSERT_THAT(IsTrue(ValidateTreeLinks(CopiedRoot, nullptr, CopySeen),
				TEXT("Copy should expose complete parent and sibling links")));
			CopyHistogramBeforeRelease = NodeTypeHistogram(CopiedRoot);
			CopyDepthBeforeRelease = MaxNodeDepth(CopiedRoot);

			asCScriptNode* MutableOriginalChild = OriginalRoot->firstChild;
			ASSERT_THAT(IsNotNull(MutableOriginalChild,
				TEXT("Original tree should expose a child for mutation and disconnection")));
			if (MutableOriginalChild != nullptr)
			{
				const uint64 OriginalLength = static_cast<uint64>(MutableOriginalChild->tokenLength);
				MutableOriginalChild->DisconnectParent();
				MutableOriginalChild->UpdateSourcePos(
					MutableOriginalChild->tokenPos,
					static_cast<size_t>(OriginalLength + 1));
				bOriginalMutationObserved = MutableOriginalChild->parent == nullptr
					&& MutableOriginalChild->next == nullptr
					&& static_cast<uint64>(MutableOriginalChild->tokenLength) == OriginalLength + 1;
			}
			ASSERT_THAT(IsTrue(bOriginalMutationObserved,
				TEXT("Original tree should visibly change when detached and its source span is extended")));
			// Leaving this scope destroys FParserAccessor and releases its
			// parser-owned node stack. CopyMemStack intentionally remains alive.
		}

		ASSERT_THAT(IsTrue(bOriginalMutationObserved,
			TEXT("Original mutation should complete before parser ownership is released")));
		ASSERT_THAT(IsNotNull(CopiedRoot,
			TEXT("Copied root should remain addressable after the parser scope releases the original tree")));
		if (CopiedRoot == nullptr)
		{
			return;
		}

		TArray<FNodeFingerprint> CopyAfterRelease;
		TMap<const asCScriptNode*, int32> CopyAfterIndices;
		CaptureNodeFingerprints(CopiedRoot, Code, CopyAfterRelease, CopyAfterIndices);
		ASSERT_THAT(AreEqual(OriginalNodeCount, CopyAfterRelease.Num(),
			TEXT("Copied tree should retain its complete node count after original release")));
		ASSERT_THAT(IsTrue(FingerprintsMatch(CopyBeforeRelease, CopyAfterRelease),
			TEXT("Copied token data, source positions, depth, and links should remain unchanged after original release")));
		TSet<const asCScriptNode*> CopyAfterSeen;
		ASSERT_THAT(IsTrue(ValidateTreeLinks(CopiedRoot, nullptr, CopyAfterSeen),
			TEXT("Copied links should remain valid after original parser teardown")));
		ASSERT_THAT(IsTrue(HistogramsMatch(CopyHistogramBeforeRelease, NodeTypeHistogram(CopiedRoot)),
			TEXT("Copied node-type histogram should remain unchanged after original release")));
		ASSERT_THAT(AreEqual(CopyDepthBeforeRelease, MaxNodeDepth(CopiedRoot),
			TEXT("Copied maximum depth should remain unchanged after original release")));
	}
};

#endif
