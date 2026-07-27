#include "../../Support/AngelscriptNativeCaseTestSupport.h"
#include "../../Support/AngelscriptNativeCoreTestSupport.h"
#include "../../Support/AngelscriptNativeFixtureTestSupport.h"
#include "../../Support/AngelscriptNativeLanguageCaseTestSupport.h"
#include "../../Support/AngelscriptNativeExpressionEvaluationTestSupport.h"

#include "CQTest.h"
#include "Misc/ScopeExit.h"


#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FEagerExpressionOrderTests,
	"Angelscript.TestModule.AngelScriptSDK.Language.Expressions.Evaluation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	using FNativeCaseContext = AngelscriptNativeTestSupport::FNativeCaseContext;

	static void AppendGeneratedAsLine(
		FString& Source,
		const FString& Line = FString())
	{
		AngelscriptNativeTestSupport::AppendGeneratedAsLine(Source, Line);
	}

	enum class EEagerComposition : uint8
	{
		Binary,
		Assignment,
		CompoundAssignment,
		CallArguments,
		ConstructorArguments,
		IndexArguments,
		CallChain,
		MemberIndexChain,
		NestedCast,
	};

	enum class EEagerOutcome : uint8
	{
		Complete,
		ExceptionFirst,
		ExceptionMiddle,
		ExceptionLast,
	};

	struct FEagerCompositionCase
	{
		const ANSICHAR* CatalogName;
		EEagerComposition Composition;
	};

	struct FEagerOperandCountCase
	{
		const ANSICHAR* CatalogName;
		int32 Count;
	};

	struct FEagerOutcomeCase
	{
		const ANSICHAR* CatalogName;
		EEagerOutcome Outcome;
	};

	struct FEagerSourceShapeCase
	{
		const ANSICHAR* CatalogName;
	};


	inline static constexpr FEagerCompositionCase EagerCompositionCases[] = {
		{"binary", EEagerComposition::Binary},
		{"assignment", EEagerComposition::Assignment},
		{"compound_assignment", EEagerComposition::CompoundAssignment},
		{"call_arguments", EEagerComposition::CallArguments},
		{"constructor_arguments", EEagerComposition::ConstructorArguments},
		{"index_arguments", EEagerComposition::IndexArguments},
		{"call_chain", EEagerComposition::CallChain},
		{"member_index_chain", EEagerComposition::MemberIndexChain},
		{"nested_cast", EEagerComposition::NestedCast},
	};

	inline static constexpr FEagerOperandCountCase EagerOperandCountCases[] = {
		{"two", 2},
		{"three", 3},
		{"eight", 8},
	};

	inline static constexpr FEagerOutcomeCase EagerOutcomeCases[] = {
		{"complete", EEagerOutcome::Complete},
		{"exception_first", EEagerOutcome::ExceptionFirst},
		{"exception_middle", EEagerOutcome::ExceptionMiddle},
		{"exception_last", EEagerOutcome::ExceptionLast},
	};

	inline static constexpr FEagerSourceShapeCase EagerSourceShapeCases[] = {
		{"single_line"},
		{"whitespace"},
		{"comments"},
		{"multiline"},
		{"nested_parentheses"},
	};


	static bool IsEagerShape(const FEagerSourceShapeCase& SourceShapeCase, const ANSICHAR* Name)
	{
		return FCStringAnsi::Strcmp(SourceShapeCase.CatalogName, Name) == 0;
	}

	static FString MakeEagerSuffix(const FEagerCompositionCase& CompositionCase,
		const FEagerOperandCountCase& OperandCountCase,
		const FEagerOutcomeCase& OutcomeCase,
		const FEagerSourceShapeCase& SourceShapeCase)
	{
		return FString::Printf(TEXT("%hs_%hs_%hs_%hs"),
			CompositionCase.CatalogName,
			OperandCountCase.CatalogName,
			OutcomeCase.CatalogName,
			SourceShapeCase.CatalogName);
	}

	static bool UsesReverseEagerOperandOrder(const FEagerCompositionCase& CompositionCase)
	{
		return CompositionCase.Composition == EEagerComposition::CallArguments ||
			   CompositionCase.Composition == EEagerComposition::ConstructorArguments ||
			   CompositionCase.Composition == EEagerComposition::IndexArguments ||
			   CompositionCase.Composition == EEagerComposition::CallChain ||
			   CompositionCase.Composition == EEagerComposition::MemberIndexChain;
	}

	static int32 EagerStageAtExecutionPosition(const FEagerCompositionCase& CompositionCase,
		const FEagerOperandCountCase& OperandCountCase,
		const int32 Position)
	{
		return UsesReverseEagerOperandOrder(CompositionCase) ? OperandCountCase.Count - Position
															 : Position + 1;
	}

	static int32 EagerExceptionStage(const FEagerCompositionCase& CompositionCase,
		const FEagerOperandCountCase& OperandCountCase,
		const FEagerOutcomeCase& OutcomeCase)
	{
		switch (OutcomeCase.Outcome)
		{
		case EEagerOutcome::ExceptionFirst:
			return EagerStageAtExecutionPosition(CompositionCase, OperandCountCase, 0);
		case EEagerOutcome::ExceptionMiddle:
			return OperandCountCase.Count == 2
					   ? INDEX_NONE
					   : EagerStageAtExecutionPosition(
							 CompositionCase, OperandCountCase, (OperandCountCase.Count - 1) / 2);
		case EEagerOutcome::ExceptionLast:
			return EagerStageAtExecutionPosition(
				CompositionCase, OperandCountCase, OperandCountCase.Count - 1);
		case EEagerOutcome::Complete:
		default:
			return INDEX_NONE;
		}
	}

	static bool UsesTwoStageCompletionException(
		const FEagerOperandCountCase& OperandCountCase, const FEagerOutcomeCase& OutcomeCase)
	{
		return OperandCountCase.Count == 2 && OutcomeCase.Outcome == EEagerOutcome::ExceptionMiddle;
	}

	static FString MakeEagerStageExpression(
		const int32 Stage, const int32 ExceptionStage, const FEagerSourceShapeCase& SourceShapeCase)
	{
		FString Expression = FString::Printf(TEXT("RecordEagerStage(%d, %d, %s)"),
			Stage,
			Stage,
			Stage == ExceptionStage ? TEXT("true") : TEXT("false"));
		if (IsEagerShape(SourceShapeCase, "comments"))
		{
			Expression = FString::Printf(TEXT("/* stage_%d */ %s"), Stage, *Expression);
		}
		else if (IsEagerShape(SourceShapeCase, "nested_parentheses"))
		{
			Expression = FString::Printf(TEXT("(((%s)))"), *Expression);
		}
		return Expression;
	}

	static FString JoinEagerTerms(const TArray<FString>& Terms,
		const FEagerSourceShapeCase& SourceShapeCase,
		const TCHAR* OperatorToken,
		const bool bArgumentList)
	{
		FString Separator;
		if (IsEagerShape(SourceShapeCase, "whitespace"))
		{
			Separator =
				bArgumentList ? TEXT("  ,\t ") : FString::Printf(TEXT("  %s\t "), OperatorToken);
		}
		else if (IsEagerShape(SourceShapeCase, "comments"))
		{
			Separator = bArgumentList ? TEXT(" /* argument boundary */ , /* next argument */ ")
									  : FString::Printf(
											TEXT(" /* operator boundary */ %s /* next operand */ "),
											OperatorToken);
		}
		else if (IsEagerShape(SourceShapeCase, "multiline"))
		{
			Separator = bArgumentList ? FString(TEXT(",")) + LINE_TERMINATOR + TEXT("\t\t")
									  : FString::Printf(TEXT(" %s"), OperatorToken) +
											LINE_TERMINATOR + TEXT("\t\t");
		}
		else
		{
			Separator = bArgumentList ? TEXT(", ") : FString::Printf(TEXT(" %s "), OperatorToken);
		}

		FString Result = FString::Join(Terms, *Separator);
		if (IsEagerShape(SourceShapeCase, "nested_parentheses") && !bArgumentList)
		{
			Result = FString::Printf(TEXT("(((%s)))"), *Result);
		}
		return Result;
	}

	static TArray<FString> MakeEagerStageExpressions(const FEagerCompositionCase& CompositionCase,
		const FEagerOperandCountCase& OperandCountCase,
		const FEagerOutcomeCase& OutcomeCase,
		const FEagerSourceShapeCase& SourceShapeCase)
	{
		const int32 ExceptionStage =
			EagerExceptionStage(CompositionCase, OperandCountCase, OutcomeCase);
		TArray<FString> Terms;
		Terms.Reserve(OperandCountCase.Count);
		for (int32 Stage = 1; Stage <= OperandCountCase.Count; ++Stage)
		{
			Terms.Add(MakeEagerStageExpression(Stage, ExceptionStage, SourceShapeCase));
		}
		return Terms;
	}

	static FString MakeParameterDeclarations(const int32 Count)
	{
		TArray<FString> Parameters;
		Parameters.Reserve(Count);
		for (int32 Index = 0; Index < Count; ++Index)
		{
			Parameters.Add(FString::Printf(TEXT("int Value%d"), Index));
		}
		return FString::Join(Parameters, TEXT(", "));
	}

	static FString MakeParameterSum(const int32 Count)
	{
		TArray<FString> Values;
		Values.Reserve(Count);
		for (int32 Index = 0; Index < Count; ++Index)
		{
			Values.Add(FString::Printf(TEXT("Value%d"), Index));
		}
		return FString::Join(Values, TEXT(" + "));
	}

	static void AppendEagerChainType(FString& Source)
	{
		AppendGeneratedAsLine(Source, TEXT("struct FEagerChain"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tint Value = 0;"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("\tFEagerChain()"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("\tFEagerChain(int InValue)"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(Source, TEXT("\t\tValue = InValue;"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("\tFEagerChain Step(int InValue) const"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(Source, TEXT("\t\treturn FEagerChain(Value + InValue);"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("\tFEagerChain opIndex(int InValue) const"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(Source, TEXT("\t\treturn FEagerChain(Value + InValue);"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("\tint Read() const"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(Source, TEXT("\t\treturn Value;"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
	}

	static void AppendEagerCompositionDeclarations(FString& Source,
		const FEagerCompositionCase& CompositionCase,
		const FEagerOperandCountCase& OperandCountCase)
	{
		const FString Parameters = MakeParameterDeclarations(OperandCountCase.Count);
		const FString Sum = MakeParameterSum(OperandCountCase.Count);
		switch (CompositionCase.Composition)
		{
		case EEagerComposition::CallArguments:
			AppendGeneratedAsLine(
				Source, FString::Printf(TEXT("int CollectEager(%s)"), *Parameters));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, FString::Printf(TEXT("\treturn %s;"), *Sum));
			AppendGeneratedAsLine(Source, TEXT("}"));
			AppendGeneratedAsLine(Source);
			break;
		case EEagerComposition::ConstructorArguments:
			AppendGeneratedAsLine(Source, TEXT("struct FEagerConstructed"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\tint Value = 0;"));
			AppendGeneratedAsLine(Source);
			AppendGeneratedAsLine(
				Source, FString::Printf(TEXT("\tFEagerConstructed(%s)"), *Parameters));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, FString::Printf(TEXT("\t\tValue = %s;"), *Sum));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
			AppendGeneratedAsLine(Source, TEXT("}"));
			AppendGeneratedAsLine(Source);
			break;
		case EEagerComposition::IndexArguments:
			AppendGeneratedAsLine(Source, TEXT("struct FEagerIndexer"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(
				Source, FString::Printf(TEXT("\tint opIndex(%s) const"), *Parameters));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, FString::Printf(TEXT("\t\treturn %s;"), *Sum));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
			AppendGeneratedAsLine(Source, TEXT("}"));
			AppendGeneratedAsLine(Source);
			break;
		case EEagerComposition::CallChain:
		case EEagerComposition::MemberIndexChain:
			AppendEagerChainType(Source);
			break;
		default:
			break;
		}
	}

	static FString MakeEagerArgumentList(
		const TArray<FString>& Terms, const FEagerSourceShapeCase& SourceShapeCase)
	{
		return JoinEagerTerms(Terms, SourceShapeCase, TEXT(","), true);
	}

	static FString WrapEagerCall(const FString& Prefix,
		const FString& Arguments,
		const FString& Suffix,
		const FEagerSourceShapeCase& SourceShapeCase)
	{
		if (IsEagerShape(SourceShapeCase, "multiline"))
		{
			return Prefix + TEXT("(") + LINE_TERMINATOR + TEXT("\t\t") + Arguments +
				   LINE_TERMINATOR + TEXT("\t)") + Suffix;
		}
		return Prefix + TEXT("(") + Arguments + TEXT(")") + Suffix;
	}

	static FString MakeEagerCallChain(const TArray<FString>& Terms,
		const FEagerSourceShapeCase& SourceShapeCase,
		const bool bMemberIndex)
	{
		FString Expression = FString::Printf(TEXT("FEagerChain(%s)"), *Terms[0]);
		for (int32 Index = 1; Index < Terms.Num(); ++Index)
		{
			FString Segment = FString::Printf(TEXT(".Step(%s)"), *Terms[Index]);
			if (bMemberIndex)
			{
				Segment += TEXT("[0]");
			}
			if (IsEagerShape(SourceShapeCase, "comments"))
			{
				Segment = FString::Printf(TEXT(" /* chain_%d */ %s"), Index + 1, *Segment);
			}
			else if (IsEagerShape(SourceShapeCase, "whitespace"))
			{
				Segment = TEXT("  ") + Segment;
			}
			else if (IsEagerShape(SourceShapeCase, "multiline"))
			{
				Segment = FString(LINE_TERMINATOR) + TEXT("\t\t") + Segment;
			}
			Expression += Segment;
			if (IsEagerShape(SourceShapeCase, "nested_parentheses"))
			{
				Expression = FString::Printf(TEXT("(%s)"), *Expression);
			}
		}
		Expression += bMemberIndex ? TEXT(".Value") : TEXT(".Read()");
		return Expression;
	}

	static FString MakeNestedCastExpression(const FString& SumExpression,
		const int32 CastDepth,
		const FEagerSourceShapeCase& SourceShapeCase)
	{
		FString Expression = SumExpression;
		for (int32 Depth = 0; Depth < CastDepth; ++Depth)
		{
			const TCHAR* const TypeName =
				Depth % 3 == 0 ? TEXT("double") : (Depth % 3 == 1 ? TEXT("float") : TEXT("int"));
			if (IsEagerShape(SourceShapeCase, "multiline"))
			{
				FString IndentedExpression = Expression;
				const FString IndentedLineBreak =
					FString(LINE_TERMINATOR) + TEXT("\t\t");
				IndentedExpression.ReplaceInline(
					LINE_TERMINATOR,
					*IndentedLineBreak);
				Expression = FString::Printf(
					TEXT("%s(%s\t\t%s%s\t)"),
					TypeName,
					LINE_TERMINATOR,
					*IndentedExpression,
					LINE_TERMINATOR);
			}
			else
			{
				Expression = FString::Printf(TEXT("%s(%s)"), TypeName, *Expression);
			}
		}
		return FString::Printf(TEXT("int(%s)"), *Expression);
	}

	static FString MakeEagerCompositionExpression(const FEagerCompositionCase& CompositionCase,
		const FEagerOperandCountCase& OperandCountCase,
		const FEagerOutcomeCase& OutcomeCase,
		const FEagerSourceShapeCase& SourceShapeCase)
	{
		const TArray<FString> Terms = MakeEagerStageExpressions(
			CompositionCase, OperandCountCase, OutcomeCase, SourceShapeCase);
		const FString SumExpression = JoinEagerTerms(Terms, SourceShapeCase, TEXT("+"), false);
		const FString Arguments = MakeEagerArgumentList(Terms, SourceShapeCase);
		FString Expression;
		switch (CompositionCase.Composition)
		{
		case EEagerComposition::Binary:
			Expression = SumExpression;
			break;
		case EEagerComposition::Assignment:
			if (IsEagerShape(SourceShapeCase, "comments"))
			{
				Expression = TEXT("Target /* assignment */ = /* eager value */ ") + SumExpression;
			}
			else if (IsEagerShape(SourceShapeCase, "whitespace"))
			{
				Expression = TEXT("Target   =\t ") + SumExpression;
			}
			else if (IsEagerShape(SourceShapeCase, "multiline"))
			{
				Expression =
					TEXT("Target") + FString(LINE_TERMINATOR) + TEXT("\t\t= ") + SumExpression;
			}
			else
			{
				Expression = TEXT("Target = ") + SumExpression;
			}
			break;
		case EEagerComposition::CompoundAssignment:
			if (IsEagerShape(SourceShapeCase, "comments"))
			{
				Expression = TEXT("Target /* compound */ += /* eager value */ ") + SumExpression;
			}
			else if (IsEagerShape(SourceShapeCase, "whitespace"))
			{
				Expression = TEXT("Target   +=\t ") + SumExpression;
			}
			else if (IsEagerShape(SourceShapeCase, "multiline"))
			{
				Expression =
					TEXT("Target") + FString(LINE_TERMINATOR) + TEXT("\t\t+= ") + SumExpression;
			}
			else
			{
				Expression = TEXT("Target += ") + SumExpression;
			}
			break;
		case EEagerComposition::CallArguments:
			Expression = WrapEagerCall(TEXT("CollectEager"), Arguments, TEXT(""), SourceShapeCase);
			break;
		case EEagerComposition::ConstructorArguments:
			Expression = WrapEagerCall(
				TEXT("FEagerConstructed"), Arguments, TEXT(".Value"), SourceShapeCase);
			break;
		case EEagerComposition::IndexArguments:
			if (IsEagerShape(SourceShapeCase, "multiline"))
			{
				Expression = TEXT("Indexer[") + FString(LINE_TERMINATOR) + TEXT("\t\t") +
							 Arguments + FString(LINE_TERMINATOR) + TEXT("\t]");
			}
			else
			{
				Expression = TEXT("Indexer[") + Arguments + TEXT("]");
			}
			break;
		case EEagerComposition::CallChain:
			Expression = MakeEagerCallChain(Terms, SourceShapeCase, false);
			break;
		case EEagerComposition::MemberIndexChain:
			Expression = MakeEagerCallChain(Terms, SourceShapeCase, true);
			break;
		case EEagerComposition::NestedCast:
			Expression =
				MakeNestedCastExpression(SumExpression, OperandCountCase.Count, SourceShapeCase);
			break;
		}

		// The fork treats assignment as a statement rather than an expression;
		// wrapping the whole assignment in parentheses therefore changes it into
		// invalid syntax. Keep nested parentheses on the RHS terms instead.
		if (IsEagerShape(SourceShapeCase, "nested_parentheses") &&
			CompositionCase.Composition != EEagerComposition::Assignment &&
			CompositionCase.Composition != EEagerComposition::CompoundAssignment)
		{
			Expression = FString::Printf(TEXT("(((%s)))"), *Expression);
		}
		if (UsesTwoStageCompletionException(OperandCountCase, OutcomeCase))
		{
			if (CompositionCase.Composition == EEagerComposition::Assignment ||
				CompositionCase.Composition == EEagerComposition::CompoundAssignment)
			{
				// Assignment expressions are statements in the fork grammar and
				// cannot be passed as a function argument. Wrap only the RHS so the
				// boundary still observes the same completion path.
				const FString Boundary = FString::Printf(
					TEXT("CompleteEagerBoundary(%s, true)"), *SumExpression);
				Expression.ReplaceInline(*SumExpression, *Boundary, ESearchCase::CaseSensitive);
			}
			else
			{
				Expression = FString::Printf(TEXT("CompleteEagerBoundary(%s, true)"), *Expression);
			}
		}
		return Expression;
	}

	static FString BuildEagerEvaluationSource(const FEagerCompositionCase& CompositionCase,
		const FEagerOperandCountCase& OperandCountCase,
		const FEagerOutcomeCase& OutcomeCase,
		const FEagerSourceShapeCase& SourceShapeCase,
		const FString& Suffix)
	{
		FString Source;
		AppendEagerCompositionDeclarations(Source, CompositionCase, OperandCountCase);
		AppendGeneratedAsLine(Source, FString::Printf(TEXT("int RunEager_%s()"), *Suffix));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tFNativeCaseValue ScopeValue(77);"));
		if (CompositionCase.Composition == EEagerComposition::Assignment)
		{
			AppendGeneratedAsLine(Source, TEXT("\tint Target = 0;"));
		}
		else if (CompositionCase.Composition == EEagerComposition::CompoundAssignment)
		{
			AppendGeneratedAsLine(Source, TEXT("\tint Target = 10;"));
		}
		else if (CompositionCase.Composition == EEagerComposition::IndexArguments)
		{
			AppendGeneratedAsLine(Source, TEXT("\tFEagerIndexer Indexer;"));
		}
		AppendGeneratedAsLine(Source);
		const FString CompositionExpression = MakeEagerCompositionExpression(
			CompositionCase, OperandCountCase, OutcomeCase, SourceShapeCase);
		if (CompositionCase.Composition == EEagerComposition::Assignment ||
			CompositionCase.Composition == EEagerComposition::CompoundAssignment)
		{
			AppendGeneratedAsLine(Source, TEXT("\t") + CompositionExpression + TEXT(";"));
			AppendGeneratedAsLine(Source, TEXT("\treturn Target;"));
		}
		else
		{
			AppendGeneratedAsLine(Source,
				FString::Printf(TEXT("\treturn %s;"), *CompositionExpression));
		}
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("int CleanAfterEagerExpression()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\treturn 137;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		return Source;
	}

	static int32 ExpectedEagerResult(const FEagerCompositionCase& CompositionCase,
		const FEagerOperandCountCase& OperandCountCase)
	{
		const int32 Sum = OperandCountCase.Count * (OperandCountCase.Count + 1) / 2;
		return CompositionCase.Composition == EEagerComposition::CompoundAssignment ? 10 + Sum
																					: Sum;
	}

	static TArray<int32> ExpectedEagerMarkers(const FEagerCompositionCase& CompositionCase,
		const FEagerOperandCountCase& OperandCountCase,
		const FEagerOutcomeCase& OutcomeCase)
	{
		int32 RecordedStageCount = OperandCountCase.Count;
		switch (OutcomeCase.Outcome)
		{
		case EEagerOutcome::ExceptionFirst:
			RecordedStageCount = 1;
			break;
		case EEagerOutcome::ExceptionMiddle:
			RecordedStageCount = OperandCountCase.Count == 2 ? OperandCountCase.Count
															 : (OperandCountCase.Count + 1) / 2;
			break;
		case EEagerOutcome::ExceptionLast:
		case EEagerOutcome::Complete:
		default:
			break;
		}

		TArray<int32> Markers;
		for (int32 Position = 0; Position < RecordedStageCount; ++Position)
		{
			Markers.Add(EagerStageAtExecutionPosition(CompositionCase, OperandCountCase, Position));
		}
		if (UsesTwoStageCompletionException(OperandCountCase, OutcomeCase))
		{
			Markers.Add(1000);
		}
		return Markers;
	}

	static bool ExpectsEagerException(const FEagerOutcomeCase& OutcomeCase)
	{
		return OutcomeCase.Outcome != EEagerOutcome::Complete;
	}

	void VerifyEagerMarkers(
		const FNativeCaseContext& Case, const TArray<int32>& Expected, const TArray<int32>& Actual)
	{
		ASSERT_THAT(AreEqual(Expected.Num(),
			Actual.Num(),
			*Case.Describe(
				TEXT("eager expression should execute exactly the expected observable stages"))));
		for (int32 Index = 0; Index < FMath::Min(Expected.Num(), Actual.Num()); ++Index)
		{
			ASSERT_THAT(AreEqual(Expected[Index],
				Actual[Index],
				*Case.Describe(
					TEXT("eager expression should preserve exact left-to-right stage order"))));
		}
	}

public:
	TEST_METHOD(CompositionsByCountOutcomeAndSourceShape)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT("LANG-EXPR-EVAL-ORDER",
			ENativeEvidence::Compile | ENativeEvidence::Runtime | ENativeEvidence::Diagnostic |
				ENativeEvidence::Lifecycle);

		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};
		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(
			ScriptEngine, TEXT("Eager evaluation product should create a raw SDK engine")));
		if (ScriptEngine == nullptr)
		{
			return;
		}

		FExpressionEvaluationRecorder EvaluationRecorder;
		FNativeLifecycleRecorder Lifecycle;
		ASSERT_THAT(IsTrue(RegisterExpressionEvaluationFunctions(*ScriptEngine, EvaluationRecorder),
			TEXT("Eager evaluation product should register observable operand stages")));
		ASSERT_THAT(IsTrue(RegisterNativeCaseValue(*ScriptEngine, Lifecycle),
			TEXT("Eager evaluation product should register its tracked scope value")));

		for (const FEagerCompositionCase& CompositionCase : EagerCompositionCases)
		{
			for (const FEagerOperandCountCase& OperandCountCase : EagerOperandCountCases)
			{
				for (const FEagerOutcomeCase& OutcomeCase : EagerOutcomeCases)
				{
					for (const FEagerSourceShapeCase& SourceShapeCase : EagerSourceShapeCases)
					{
						EvaluationRecorder.Reset();
						Lifecycle.Reset();
						const FNativeCaseContext Case(MakeNativeCaseId("LANG-EXPR-EVAL-ORDER",
							{
								ANSI_TO_TCHAR(CompositionCase.CatalogName),
								ANSI_TO_TCHAR(OperandCountCase.CatalogName),
								ANSI_TO_TCHAR(OutcomeCase.CatalogName),
								ANSI_TO_TCHAR(SourceShapeCase.CatalogName),
							}));
						const FString Suffix = MakeEagerSuffix(
							CompositionCase, OperandCountCase, OutcomeCase, SourceShapeCase);
						const FString ModuleName = TEXT("ExpressionEager_") + Suffix;
						const FString Source = BuildEagerEvaluationSource(CompositionCase,
							OperandCountCase,
							OutcomeCase,
							SourceShapeCase,
							Suffix);
						PrintGeneratedAsSource(*TestRunner, Case.GetId(), ModuleName, Source);
						const FTCHARToUTF8 ModuleNameUtf8(*ModuleName);
						const FTCHARToUTF8 SourceUtf8(*Source);
						Engine.ResetMessages();
						asIScriptModule* Module = nullptr;
						const int32 BuildResult = CompileNativeModule(
							ScriptEngine, ModuleNameUtf8.Get(), SourceUtf8.Get(), Module);
						ASSERT_THAT(IsTrue(BuildResult >= 0,
							*Case.DescribeResult("<module build>",
								TEXT("successful compilation"),
								Engine.GetMessagesText())));
						ASSERT_THAT(IsNotNull(Module,
							*Case.Describe(TEXT("eager evaluation cell should publish a module"))));
						if (BuildResult >= 0 && Module != nullptr)
						{
							const FString EntryDeclaration =
								FString::Printf(TEXT("int RunEager_%s()"), *Suffix);
							asIScriptFunction* const Entry = GetNativeFunctionByExactDecl(
								Module, TCHAR_TO_ANSI(*EntryDeclaration));
							asIScriptFunction* const Clean = GetNativeFunctionByExactDecl(
								Module, "int CleanAfterEagerExpression()");
							ASSERT_THAT(IsNotNull(Entry,
								*Case.Describe(
									TEXT("eager entry should resolve by exact declaration"))));
							ASSERT_THAT(IsNotNull(Clean,
								*Case.Describe(
									TEXT("eager cell should expose its context-reuse probe"))));
							if (Entry != nullptr && Clean != nullptr)
							{
								asIScriptContext* const Context = ScriptEngine->CreateContext();
								ASSERT_THAT(IsNotNull(Context,
									*Case.Describe(
										TEXT("eager cell should create an execution context"))));
								if (Context != nullptr)
								{
									const bool bExpectedException =
										ExpectsEagerException(OutcomeCase);
									const int32 ExecuteResult = PrepareAndExecute(Context, Entry);
									if (bExpectedException)
									{
										ASSERT_THAT(AreEqual(
											static_cast<int32>(asEXECUTION_EXCEPTION),
											ExecuteResult,
											*Case.Describe(TEXT("selected eager exception stage "
																"should stop execution"))));
										const FString ExpectedException =
											UsesTwoStageCompletionException(
												OperandCountCase, OutcomeCase)
												? TEXT("Eager expression completion boundary "
													   "exception")
												: TEXT("Eager expression stage exception");
										ASSERT_THAT(AreEqual(ExpectedException,
											FString(UTF8_TO_TCHAR(Context->GetExceptionString())),
											*Case.Describe(TEXT("eager exception should retain its "
																"exact native cause"))));
										asIScriptFunction* const ExceptionFunction =
											Context->GetExceptionFunction();
										ASSERT_THAT(IsNotNull(ExceptionFunction,
											*Case.Describe(TEXT("eager exception should retain its "
																"script entry frame"))));
										if (ExceptionFunction != nullptr)
										{
											ASSERT_THAT(IsTrue(
												FString(UTF8_TO_TCHAR(ExceptionFunction->GetName()))
													.StartsWith(TEXT("RunEager_")),
												*Case.Describe(
													TEXT("eager exception frame should identify "
														 "the generated entry"))));
										}
										const char* ExceptionSection = nullptr;
										int32 ExceptionColumn = INDEX_NONE;
										ASSERT_THAT(
											IsTrue(Context->GetExceptionLineNumber(
													   &ExceptionColumn, &ExceptionSection) > 0,
												*Case.Describe(TEXT("eager exception should retain "
																	"a generated source line"))));
										ASSERT_THAT(AreEqual(ModuleName,
											FString(UTF8_TO_TCHAR(ExceptionSection != nullptr
																	  ? ExceptionSection
																	  : "")),
											*Case.Describe(TEXT("eager exception should retain its "
																"generated section"))));
										ASSERT_THAT(IsTrue(ExceptionColumn > 0,
											*Case.Describe(TEXT(
												"eager exception should retain a source column"))));
									}
									else
									{
										ASSERT_THAT(
											AreEqual(static_cast<int32>(asEXECUTION_FINISHED),
												ExecuteResult,
												*Case.Describe(TEXT(
													"complete eager expression should finish"))));
										if (ExecuteResult == asEXECUTION_FINISHED)
										{
											ASSERT_THAT(
												AreEqual(ExpectedEagerResult(
															 CompositionCase, OperandCountCase),
													static_cast<int32>(Context->GetReturnDWord()),
													*Case.Describe(
														TEXT("complete eager expression should "
															 "preserve its composition result"))));
										}
									}

									VerifyEagerMarkers(Case,
										ExpectedEagerMarkers(
											CompositionCase, OperandCountCase, OutcomeCase),
										EvaluationRecorder.Markers);
									ASSERT_THAT(AreEqual(asSUCCESS,
										Context->Unprepare(),
										*Case.Describe(TEXT("eager context should unprepare after "
															"completion or exception"))));
									ASSERT_THAT(AreEqual(0,
										Lifecycle.GetLiveObjectCount(),
										*Case.Describe(TEXT("eager cleanup should release its "
															"tracked scope value"))));
									ASSERT_THAT(AreEqual(1,
										Lifecycle.Num(ENativeLifecycleEvent::ValueConstruct),
										*Case.Describe(TEXT("eager entry should construct one "
															"tracked scope value"))));
									ASSERT_THAT(AreEqual(1,
										Lifecycle.Num(ENativeLifecycleEvent::Destruct),
										*Case.Describe(TEXT("eager entry should destroy its "
															"tracked scope value once"))));

									ASSERT_THAT(AreEqual(asSUCCESS,
										Context->Prepare(Clean),
										*Case.Describe(TEXT(
											"eager context should prepare a clean follow-up"))));
									ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_FINISHED),
										Context->Execute(),
										*Case.Describe(TEXT(
											"eager context should execute after prior state"))));
									ASSERT_THAT(AreEqual(137,
										static_cast<int32>(Context->GetReturnDWord()),
										*Case.Describe(TEXT("eager context reuse should not retain "
															"stale result state"))));
									Context->Release();
								}
							}
						}

						ScriptEngine->DiscardModule(ModuleNameUtf8.Get());
						ASSERT_THAT(IsNull(
							ScriptEngine->GetModule(ModuleNameUtf8.Get(), asGM_ONLY_IF_EXISTS),
							*Case.Describe(
								TEXT("eager evaluation cell should discard its isolated module"))));
					}
				}
			}
		}
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
