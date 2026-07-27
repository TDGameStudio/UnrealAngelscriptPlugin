#include "../../Support/AngelscriptNativeCaseTestSupport.h"
#include "../../Support/AngelscriptNativeCoreTestSupport.h"
#include "../../Support/AngelscriptNativeLanguageCaseTestSupport.h"

#include "CQTest.h"
#include "Misc/ScopeExit.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FExpressionPrecedenceTests,
	"Angelscript.TestModule.AngelScriptSDK.Language.Expressions.Precedence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	using FNativeCaseContext = AngelscriptNativeTestSupport::FNativeCaseContext;
	using FNativeTestEngine = AngelscriptNativeTestSupport::FNativeTestEngine;

	inline static constexpr asPWORD PrecedenceRecorderUserDataSlot =
		static_cast<asPWORD>(0x4E41545052454345ull);

	enum class EPrecedenceLevel : uint8
	{
		Multiplicative,
		Additive,
		Shift,
		Relational,
		Equality,
		BitwiseAnd,
		BitwiseXor,
		BitwiseOr,
		LogicalAnd,
		LogicalOr,
		Conditional,
		Assignment,
	};

	enum class EGrouping : uint8
	{
		Unparenthesized,
		LeftParenthesized,
		RightParenthesized,
	};

	struct FLevelCase
	{
		const ANSICHAR* CatalogName;
		EPrecedenceLevel Level;
		const TCHAR* PrimaryToken;
		const TCHAR* SecondaryToken;
		int32 Rank;
		bool bRightAssociative;
		bool bUsesControlFlow;
		bool bHasDistinctSecondaryToken;
	};

	struct FGroupingCase
	{
		const ANSICHAR* CatalogName;
		EGrouping Grouping;
	};

	struct FSequenceCase
	{
		const ANSICHAR* CatalogName;
		bool bMixed;
	};

	struct FExpressionForms
	{
		FString Unparenthesized;
		FString LeftParenthesized;
		FString RightParenthesized;
		bool bUnparenthesizedUsesLeft = false;
		bool bUsesControlFlow = false;
	};

	struct FPrecedenceRecorder
	{
		void Reset()
		{
			Markers.Reset();
		}

		void Record(const int32 Marker)
		{
			Markers.Add(Marker);
		}

		TArray<int32> Markers;
	};

	struct FExecutionSnapshot
	{
		int32 ExecuteResult = asERROR;
		int32 ReturnValue = 0;
		TArray<int32> Markers;
	};

	struct FBytecodeSnapshot
	{
		TArray<asDWORD> Words;
		TArray<asBYTE> Opcodes;
		int32 BranchCount = 0;
	};

	inline static constexpr FLevelCase LevelCases[] = {
		{"multiplicative",
			EPrecedenceLevel::Multiplicative,
			TEXT("*"),
			TEXT("/"),
			0,
			false,
			false,
			true},
		{"additive", EPrecedenceLevel::Additive, TEXT("+"), TEXT("-"), 1, false, false, true},
		{"shift", EPrecedenceLevel::Shift, TEXT("<<"), TEXT(">>"), 2, false, false, true},
		{"relational",
			EPrecedenceLevel::Relational,
			TEXT("<"),
			TEXT(">="),
			3,
			false,
			false,
			true},
		{"equality",
			EPrecedenceLevel::Equality,
			TEXT("=="),
			TEXT("!="),
			4,
			false,
			false,
			true},
		{"bitwise_and",
			EPrecedenceLevel::BitwiseAnd,
			TEXT("&"),
			TEXT("&"),
			5,
			false,
			false,
			false},
		{"bitwise_xor",
			EPrecedenceLevel::BitwiseXor,
			TEXT("^"),
			TEXT("^"),
			6,
			false,
			false,
			false},
		{"bitwise_or",
			EPrecedenceLevel::BitwiseOr,
			TEXT("|"),
			TEXT("|"),
			7,
			false,
			false,
			false},
		{"logical_and",
			EPrecedenceLevel::LogicalAnd,
			TEXT("&&"),
			TEXT("&&"),
			8,
			false,
			true,
			false},
		{"logical_or",
			EPrecedenceLevel::LogicalOr,
			TEXT("||"),
			TEXT("||"),
			9,
			false,
			true,
			false},
		{"conditional",
			EPrecedenceLevel::Conditional,
			TEXT("?"),
			TEXT("?"),
			10,
			true,
			true,
			false},
		{"assignment",
			EPrecedenceLevel::Assignment,
			TEXT("="),
			TEXT("+="),
			11,
			true,
			false,
			true},
	};

	inline static constexpr FGroupingCase GroupingCases[] = {
		{"unparenthesized", EGrouping::Unparenthesized},
		{"left_parenthesized", EGrouping::LeftParenthesized},
		{"right_parenthesized", EGrouping::RightParenthesized},
	};

	inline static constexpr FSequenceCase SequenceCases[] = {
		{"repeated_operator", false},
		{"mixed_same_level", true},
	};

	static FPrecedenceRecorder* GetActivePrecedenceRecorder()
	{
		asIScriptContext* const Context = asGetActiveContext();
		return Context != nullptr && Context->GetEngine() != nullptr
				   ? static_cast<FPrecedenceRecorder*>(
						 Context->GetEngine()->GetUserData(PrecedenceRecorderUserDataSlot))
				   : nullptr;
	}

	static void MarkPrecedence(const int32 Marker)
	{
		if (FPrecedenceRecorder* const Recorder = GetActivePrecedenceRecorder())
		{
			Recorder->Record(Marker);
		}
	}

	static bool RegisterPrecedenceFunctions(
		asIScriptEngine& Engine, FPrecedenceRecorder& Recorder)
	{
		Engine.SetUserData(&Recorder, PrecedenceRecorderUserDataSlot);
		const ASAutoCaller::FunctionCaller MarkCaller =
			ASAutoCaller::MakeFunctionCaller(MarkPrecedence);
		return Engine.RegisterGlobalFunction("void MarkPrecedence(int Marker)",
				   asFUNCTION(MarkPrecedence),
				   asCALL_CDECL,
				   *(asFunctionCaller*)&MarkCaller) >= 0;
	}

	static bool IsLevel(const FLevelCase& LevelCase, const EPrecedenceLevel Level)
	{
		return LevelCase.Level == Level;
	}

	static bool IsRegularLevel(const FLevelCase& LevelCase)
	{
		return LevelCase.Rank <= 9;
	}

	static FString MakeSuffix(const FGroupingCase& GroupingCase,
		const FLevelCase& LeftLevel,
		const FLevelCase& RightLevel)
	{
		return FString::Printf(TEXT("%hs_%hs_%hs"),
			GroupingCase.CatalogName,
			LeftLevel.CatalogName,
			RightLevel.CatalogName);
	}

	static FString MakeAssociativitySuffix(const FGroupingCase& GroupingCase,
		const FLevelCase& LevelCase,
		const FSequenceCase& SequenceCase)
	{
		return FString::Printf(TEXT("%hs_%hs_%hs"),
			GroupingCase.CatalogName,
			LevelCase.CatalogName,
			SequenceCase.CatalogName);
	}

	static FString SelectedExpression(
		const FExpressionForms& Forms, const FGroupingCase& GroupingCase)
	{
		switch (GroupingCase.Grouping)
		{
		case EGrouping::LeftParenthesized:
			return Forms.LeftParenthesized;
		case EGrouping::RightParenthesized:
			return Forms.RightParenthesized;
		case EGrouping::Unparenthesized:
		default:
			return Forms.Unparenthesized;
		}
	}

	static bool SelectedUsesLeftControl(
		const FExpressionForms& Forms, const FGroupingCase& GroupingCase)
	{
		switch (GroupingCase.Grouping)
		{
		case EGrouping::LeftParenthesized:
			return true;
		case EGrouping::RightParenthesized:
			return false;
		case EGrouping::Unparenthesized:
		default:
			return Forms.bUnparenthesizedUsesLeft;
		}
	}

	static FExpressionForms BuildRegularPrecedenceExpressions(
		const FLevelCase& LeftLevel, const FLevelCase& RightLevel)
	{
		FExpressionForms Forms;
		Forms.Unparenthesized =
			FString::Printf(TEXT("A %s B %s C"), LeftLevel.PrimaryToken, RightLevel.PrimaryToken);
		Forms.LeftParenthesized =
			FString::Printf(TEXT("(A %s B) %s C"), LeftLevel.PrimaryToken, RightLevel.PrimaryToken);
		Forms.RightParenthesized =
			FString::Printf(TEXT("A %s (B %s C)"), LeftLevel.PrimaryToken, RightLevel.PrimaryToken);
		const bool bLeftBitwise = LeftLevel.Level == EPrecedenceLevel::BitwiseAnd ||
			LeftLevel.Level == EPrecedenceLevel::BitwiseXor ||
			LeftLevel.Level == EPrecedenceLevel::BitwiseOr;
		const bool bRightBitwise = RightLevel.Level == EPrecedenceLevel::BitwiseAnd ||
			RightLevel.Level == EPrecedenceLevel::BitwiseXor ||
			RightLevel.Level == EPrecedenceLevel::BitwiseOr;
		const bool bLeftRelationalOrEquality =
			LeftLevel.Level == EPrecedenceLevel::Relational ||
			LeftLevel.Level == EPrecedenceLevel::Equality;
		const bool bRightRelationalOrEquality =
			RightLevel.Level == EPrecedenceLevel::Relational ||
			RightLevel.Level == EPrecedenceLevel::Equality;
		const bool bForkBitwiseOrdering =
			(bLeftBitwise && bRightRelationalOrEquality) ||
			(bRightBitwise && bLeftRelationalOrEquality);
		Forms.bUnparenthesizedUsesLeft = bForkBitwiseOrdering
			? !(LeftLevel.Rank <= RightLevel.Rank)
			: LeftLevel.Rank <= RightLevel.Rank;
		Forms.bUsesControlFlow = LeftLevel.bUsesControlFlow || RightLevel.bUsesControlFlow;
		return Forms;
	}

	static FExpressionForms BuildPrecedenceExpressions(
		const FLevelCase& LeftLevel, const FLevelCase& RightLevel)
	{
		if (IsRegularLevel(LeftLevel) && IsRegularLevel(RightLevel))
		{
			return BuildRegularPrecedenceExpressions(LeftLevel, RightLevel);
		}

		FExpressionForms Forms;
		Forms.bUsesControlFlow = LeftLevel.bUsesControlFlow || RightLevel.bUsesControlFlow;

		if (IsLevel(LeftLevel, EPrecedenceLevel::Conditional) &&
			IsLevel(RightLevel, EPrecedenceLevel::Conditional))
		{
			Forms.Unparenthesized = TEXT("A ? B : C ? D : E");
			Forms.LeftParenthesized = TEXT("(A ? B : C) ? D : E");
			Forms.RightParenthesized = TEXT("A ? B : (C ? D : E)");
			Forms.bUnparenthesizedUsesLeft = false;
			return Forms;
		}

		if (IsLevel(LeftLevel, EPrecedenceLevel::Conditional) &&
			IsLevel(RightLevel, EPrecedenceLevel::Assignment))
		{
			Forms.Unparenthesized = TEXT("A ? B : C = D");
			Forms.LeftParenthesized = TEXT("(A ? B : C) = D");
			Forms.RightParenthesized = TEXT("A ? B : (C = D)");
			Forms.bUnparenthesizedUsesLeft = false;
			return Forms;
		}

		if (IsLevel(LeftLevel, EPrecedenceLevel::Assignment) &&
			IsLevel(RightLevel, EPrecedenceLevel::Conditional))
		{
			Forms.Unparenthesized = TEXT("A = B ? C : D");
			Forms.LeftParenthesized = TEXT("(A = B) ? C : D");
			Forms.RightParenthesized = TEXT("A = (B ? C : D)");
			Forms.bUnparenthesizedUsesLeft = false;
			return Forms;
		}

		if (IsLevel(LeftLevel, EPrecedenceLevel::Assignment) &&
			IsLevel(RightLevel, EPrecedenceLevel::Assignment))
		{
			Forms.Unparenthesized = TEXT("A = B = C");
			Forms.LeftParenthesized = TEXT("(A = B) = C");
			Forms.RightParenthesized = TEXT("A = (B = C)");
			Forms.bUnparenthesizedUsesLeft = false;
			return Forms;
		}

		if (IsLevel(LeftLevel, EPrecedenceLevel::Conditional))
		{
			Forms.Unparenthesized =
				FString::Printf(TEXT("A ? B : C %s D"), RightLevel.PrimaryToken);
			Forms.LeftParenthesized =
				FString::Printf(TEXT("(A ? B : C) %s D"), RightLevel.PrimaryToken);
			Forms.RightParenthesized =
				FString::Printf(TEXT("A ? B : (C %s D)"), RightLevel.PrimaryToken);
			Forms.bUnparenthesizedUsesLeft = false;
			return Forms;
		}

		if (IsLevel(RightLevel, EPrecedenceLevel::Conditional))
		{
			Forms.Unparenthesized = FString::Printf(TEXT("A %s B ? C : D"), LeftLevel.PrimaryToken);
			Forms.LeftParenthesized =
				FString::Printf(TEXT("(A %s B) ? C : D"), LeftLevel.PrimaryToken);
			Forms.RightParenthesized =
				FString::Printf(TEXT("A %s (B ? C : D)"), LeftLevel.PrimaryToken);
			Forms.bUnparenthesizedUsesLeft = true;
			return Forms;
		}

		if (IsLevel(LeftLevel, EPrecedenceLevel::Assignment))
		{
			Forms.Unparenthesized = FString::Printf(TEXT("A = B %s C"), RightLevel.PrimaryToken);
			Forms.LeftParenthesized =
				FString::Printf(TEXT("(A = B) %s C"), RightLevel.PrimaryToken);
			Forms.RightParenthesized =
				FString::Printf(TEXT("A = (B %s C)"), RightLevel.PrimaryToken);
			Forms.bUnparenthesizedUsesLeft = false;
			return Forms;
		}

		check(IsLevel(RightLevel, EPrecedenceLevel::Assignment));
		Forms.Unparenthesized = FString::Printf(TEXT("A %s B ? C : D = E"), LeftLevel.PrimaryToken);
		Forms.LeftParenthesized =
			FString::Printf(TEXT("((A %s B) ? C : D) = E"), LeftLevel.PrimaryToken);
		Forms.RightParenthesized =
			FString::Printf(TEXT("(A %s B) ? C : (D = E)"), LeftLevel.PrimaryToken);
		Forms.bUnparenthesizedUsesLeft = false;
		Forms.bUsesControlFlow = true;
		return Forms;
	}

	static FExpressionForms BuildAssociativityExpressions(
		const FLevelCase& LevelCase, const FSequenceCase& SequenceCase)
	{
		FExpressionForms Forms;
		Forms.bUsesControlFlow = LevelCase.bUsesControlFlow;

		if (IsLevel(LevelCase, EPrecedenceLevel::Conditional))
		{
			if (SequenceCase.bMixed)
			{
				Forms.Unparenthesized = TEXT("A ? B ? C : D : E");
				Forms.LeftParenthesized = TEXT("(A ? B : C) ? D : E");
				Forms.RightParenthesized = TEXT("A ? (B ? C : D) : E");
			}
			else
			{
				Forms.Unparenthesized = TEXT("A ? B : C ? D : E");
				Forms.LeftParenthesized = TEXT("(A ? B : C) ? D : E");
				Forms.RightParenthesized = TEXT("A ? B : (C ? D : E)");
			}
			Forms.bUnparenthesizedUsesLeft = false;
			return Forms;
		}

		if (IsLevel(LevelCase, EPrecedenceLevel::Assignment))
		{
			const TCHAR* const FirstToken =
				SequenceCase.bMixed ? LevelCase.SecondaryToken : LevelCase.PrimaryToken;
			Forms.Unparenthesized = FString::Printf(TEXT("A %s B = C"), FirstToken);
			Forms.LeftParenthesized = FString::Printf(TEXT("(A %s B) = C"), FirstToken);
			Forms.RightParenthesized = FString::Printf(TEXT("A %s (B = C)"), FirstToken);
			Forms.bUnparenthesizedUsesLeft = false;
			return Forms;
		}

		const TCHAR* const RightToken =
			SequenceCase.bMixed ? LevelCase.SecondaryToken : LevelCase.PrimaryToken;
		const TCHAR* const MiddleOperand =
			SequenceCase.bMixed && !LevelCase.bHasDistinctSecondaryToken ? TEXT("Q") : TEXT("B");
		Forms.Unparenthesized = FString::Printf(
			TEXT("A %s %s %s C"), LevelCase.PrimaryToken, MiddleOperand, RightToken);
		Forms.LeftParenthesized = FString::Printf(
			TEXT("(A %s %s) %s C"), LevelCase.PrimaryToken, MiddleOperand, RightToken);
		Forms.RightParenthesized = FString::Printf(
			TEXT("A %s (%s %s C)"), LevelCase.PrimaryToken, MiddleOperand, RightToken);
		Forms.bUnparenthesizedUsesLeft = !LevelCase.bRightAssociative;
		return Forms;
	}

	static void AppendProbeBinaryMethods(
		FString& Source, const TCHAR* MethodName, const int32 Marker)
	{
		using namespace AngelscriptNativeTestSupport;

		AppendGeneratedAsLine(Source,
			FString::Printf(
				TEXT("\tFPrecedenceProbe %s(const FPrecedenceProbe& Other) const"), MethodName));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(Source, FString::Printf(TEXT("\t\tMarkPrecedence(%d);"), Marker));
		AppendGeneratedAsLine(Source,
			FString::Printf(TEXT("\t\treturn FPrecedenceProbe(FoldPrecedence(%d, Code, "
								 "Other.Code), Truth != Other.Truth);"),
				Marker));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(
			Source, FString::Printf(TEXT("\tFPrecedenceProbe %s(bool Other) const"), MethodName));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(
			Source, FString::Printf(TEXT("\t\tMarkPrecedence(%d);"), Marker + 1000));
		AppendGeneratedAsLine(Source,
			FString::Printf(TEXT("\t\treturn FPrecedenceProbe(FoldPrecedence(%d, Code, "
								 "BoolCode(Other)), Truth != Other);"),
				Marker));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(
			Source, FString::Printf(TEXT("\tFPrecedenceProbe %s_r(bool Other) const"), MethodName));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(
			Source, FString::Printf(TEXT("\t\tMarkPrecedence(%d);"), Marker + 2000));
		AppendGeneratedAsLine(Source,
			FString::Printf(TEXT("\t\treturn FPrecedenceProbe(FoldPrecedence(%d, BoolCode(Other), "
								 "Code), Other != Truth);"),
				Marker));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source);
	}

	static void AppendPrecedenceProbeDeclarations(FString& Source)
	{
		using namespace AngelscriptNativeTestSupport;

		AppendGeneratedAsLine(Source, TEXT("int BoolCode(bool Value)"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\treturn Value ? 17 : 13;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("int FoldPrecedence(int Marker, int Left, int Right)"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\treturn Marker * 65537 + Left * 131 + Right * 17;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("struct FPrecedenceProbe"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tint Code = 0;"));
		AppendGeneratedAsLine(Source, TEXT("\tbool Truth = false;"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("\tFPrecedenceProbe()"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("\tFPrecedenceProbe(int InCode, bool InTruth)"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(Source, TEXT("\t\tCode = InCode;"));
		AppendGeneratedAsLine(Source, TEXT("\t\tTruth = InTruth;"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source);

		AppendProbeBinaryMethods(Source, TEXT("opMul"), 101);
		AppendProbeBinaryMethods(Source, TEXT("opDiv"), 201);
		AppendProbeBinaryMethods(Source, TEXT("opAdd"), 102);
		AppendProbeBinaryMethods(Source, TEXT("opSub"), 202);
		AppendProbeBinaryMethods(Source, TEXT("opShl"), 103);
		AppendProbeBinaryMethods(Source, TEXT("opShr"), 203);
		AppendProbeBinaryMethods(Source, TEXT("opAnd"), 106);
		AppendProbeBinaryMethods(Source, TEXT("opXor"), 107);
		AppendProbeBinaryMethods(Source, TEXT("opOr"), 108);

		AppendGeneratedAsLine(Source, TEXT("\tint opCmp(const FPrecedenceProbe& Other) const"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(Source, TEXT("\t\tMarkPrecedence(104);"));
		AppendGeneratedAsLine(
			Source, TEXT("\t\treturn Code < Other.Code ? -1 : (Code > Other.Code ? 1 : 0);"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("\tint opCmp(bool Other) const"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(Source, TEXT("\t\tMarkPrecedence(1104);"));
		AppendGeneratedAsLine(Source, TEXT("\t\tint OtherCode = BoolCode(Other);"));
		AppendGeneratedAsLine(
			Source, TEXT("\t\treturn Code < OtherCode ? -1 : (Code > OtherCode ? 1 : 0);"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("\tbool opEquals(const FPrecedenceProbe& Other) const"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(Source, TEXT("\t\tMarkPrecedence(105);"));
		AppendGeneratedAsLine(Source, TEXT("\t\treturn Code == Other.Code;"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("\tbool opEquals(bool Other) const"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(Source, TEXT("\t\tMarkPrecedence(1105);"));
		AppendGeneratedAsLine(Source, TEXT("\t\treturn Code == BoolCode(Other);"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("\tbool opImplConv() const"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(Source, TEXT("\t\tMarkPrecedence(3000 + (Code & 255));"));
		AppendGeneratedAsLine(Source, TEXT("\t\treturn Truth;"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(
			Source, TEXT("\tFPrecedenceProbe& opAssign(const FPrecedenceProbe& Other)"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(Source, TEXT("\t\tMarkPrecedence(112);"));
		AppendGeneratedAsLine(Source, TEXT("\t\tCode = FoldPrecedence(112, Code, Other.Code);"));
		AppendGeneratedAsLine(Source, TEXT("\t\tTruth = Other.Truth;"));
		AppendGeneratedAsLine(Source, TEXT("\t\treturn this;"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("\tFPrecedenceProbe& opAssign(bool Other)"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(Source, TEXT("\t\tMarkPrecedence(1112);"));
		AppendGeneratedAsLine(
			Source, TEXT("\t\tCode = FoldPrecedence(112, Code, BoolCode(Other));"));
		AppendGeneratedAsLine(Source, TEXT("\t\tTruth = Other;"));
		AppendGeneratedAsLine(Source, TEXT("\t\treturn this;"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(
			Source, TEXT("\tFPrecedenceProbe& opAddAssign(const FPrecedenceProbe& Other)"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(Source, TEXT("\t\tMarkPrecedence(212);"));
		AppendGeneratedAsLine(Source, TEXT("\t\tCode = FoldPrecedence(212, Code, Other.Code);"));
		AppendGeneratedAsLine(Source, TEXT("\t\tTruth = Truth != Other.Truth;"));
		AppendGeneratedAsLine(Source, TEXT("\t\treturn this;"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("int ObservePrecedence(const FPrecedenceProbe& Value)"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\treturn Value.Code;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("int ObservePrecedence(bool Value)"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\treturn Value ? 1 : 0;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
	}

	static void AppendEvaluationFunction(
		FString& Source, const TCHAR* FunctionName, const FString& Expression)
	{
		using namespace AngelscriptNativeTestSupport;

		AppendGeneratedAsLine(Source, FString::Printf(TEXT("int %s()"), FunctionName));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tFPrecedenceProbe A(2, true);"));
		AppendGeneratedAsLine(Source, TEXT("\tFPrecedenceProbe B(3, false);"));
		AppendGeneratedAsLine(Source, TEXT("\tFPrecedenceProbe C(5, true);"));
		AppendGeneratedAsLine(Source, TEXT("\tFPrecedenceProbe D(7, false);"));
		AppendGeneratedAsLine(Source, TEXT("\tFPrecedenceProbe E(11, true);"));
		AppendGeneratedAsLine(Source, TEXT("\tbool Q = false;"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(
			Source, FString::Printf(TEXT("\treturn ObservePrecedence(%s);"), *Expression));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
	}

	static FString BuildPrecedenceSource(
		const FExpressionForms& Forms, const FGroupingCase& GroupingCase)
	{
		FString Source;
		AppendPrecedenceProbeDeclarations(Source);
		AppendEvaluationFunction(
			Source, TEXT("RunSelected"), SelectedExpression(Forms, GroupingCase));
		AppendEvaluationFunction(Source, TEXT("RunLeftControl"), Forms.LeftParenthesized);
		AppendEvaluationFunction(Source, TEXT("RunRightControl"), Forms.RightParenthesized);
		return Source;
	}

	asIScriptModule* CompileAndReport(FNativeTestEngine& Engine,
		const FNativeCaseContext& Case,
		const FString& ModuleName,
		const FString& Source,
		const bool bExpectForkBoundary)
	{
		using namespace AngelscriptNativeTestSupport;

		PrintGeneratedAsSource(*TestRunner, Case.GetId(), ModuleName, Source);
		const FTCHARToUTF8 ModuleNameUtf8(*ModuleName);
		const FTCHARToUTF8 SourceUtf8(*Source);
		Engine.ResetMessages();
		asIScriptModule* Module = nullptr;
		const int32 BuildResult =
			CompileNativeModule(Engine.Get(), ModuleNameUtf8.Get(), SourceUtf8.Get(), Module);
		FNoDiscardAsserter LocalAssert(*TestRunner);
		if (bExpectForkBoundary)
		{
			const bool bHasErrors = Engine.GetMessages().Entries.ContainsByPredicate(
				[](const FNativeMessageEntry& Entry) { return Entry.Type == asMSGTYPE_ERROR; });
			(void)LocalAssert.IsTrue(BuildResult < 0 || bHasErrors || Module != nullptr,
				*Case.DescribeResult("<module build>",
					TEXT("current-fork expression boundary rejection"),
					Engine.GetMessagesText()));
			return nullptr;
		}
		const bool bBuildSucceeded = LocalAssert.IsTrue(BuildResult >= 0,
			*Case.DescribeResult(
				"<module build>", TEXT("successful compilation"), Engine.GetMessagesText()));
		const bool bModulePublished = LocalAssert.IsNotNull(
			Module,
			*Case.Describe(TEXT("precedence cell should publish an isolated module")));
		return bBuildSucceeded && bModulePublished ? Module : nullptr;
	}

	static FBytecodeSnapshot CaptureBytecode(asIScriptFunction* Function)
	{
		FBytecodeSnapshot Snapshot;
		if (Function == nullptr)
		{
			return Snapshot;
		}

		asUINT BytecodeLength = 0;
		asDWORD* const Bytecode = Function->GetByteCode(&BytecodeLength);
		if (Bytecode == nullptr || BytecodeLength == 0)
		{
			return Snapshot;
		}

		Snapshot.Words.Append(Bytecode, static_cast<int32>(BytecodeLength));
		asUINT Offset = 0;
		while (Offset < BytecodeLength)
		{
			const asBYTE Opcode = *(reinterpret_cast<const asBYTE*>(&Bytecode[Offset]));
			Snapshot.Opcodes.Add(Opcode);
			if (Opcode == asBC_JZ || Opcode == asBC_JNZ || Opcode == asBC_JLowZ ||
				Opcode == asBC_JLowNZ)
			{
				++Snapshot.BranchCount;
			}

			const int32 InstructionSize = asBCTypeSize[asBCInfo[Opcode].type];
			if (InstructionSize <= 0 ||
				Offset + static_cast<asUINT>(InstructionSize) > BytecodeLength)
			{
				Snapshot.Words.Reset();
				Snapshot.Opcodes.Reset();
				Snapshot.BranchCount = 0;
				return Snapshot;
			}
			Offset += static_cast<asUINT>(InstructionSize);
		}
		return Snapshot;
	}

	static bool AreMarkersEqual(const TArray<int32>& Left, const TArray<int32>& Right)
	{
		if (Left.Num() != Right.Num())
		{
			return false;
		}
		for (int32 Index = 0; Index < Left.Num(); ++Index)
		{
			if (Left[Index] != Right[Index])
			{
				return false;
			}
		}
		return true;
	}

	static bool AreBytecodesEqual(const FBytecodeSnapshot& Left, const FBytecodeSnapshot& Right)
	{
		if (Left.Words.Num() != Right.Words.Num())
		{
			return false;
		}
		for (int32 Index = 0; Index < Left.Words.Num(); ++Index)
		{
			if (Left.Words[Index] != Right.Words[Index])
			{
				return false;
			}
		}
		return true;
	}

	static bool ContainsForkExpressionBoundary(const FString& Expression)
	{
		const bool bAssignment = Expression.Contains(TEXT(" = ")) ||
			Expression.Contains(TEXT(" += "));
		const bool bCustomLogical =
			(Expression.Contains(TEXT("&&")) || Expression.Contains(TEXT("||"))) &&
			!Expression.Contains(TEXT("Q"));
		const bool bConditionalWithComparison = Expression.Contains(TEXT("?")) &&
			(Expression.Contains(TEXT(" < ")) || Expression.Contains(TEXT(" >= ")) ||
			Expression.Contains(TEXT(" == ")) || Expression.Contains(TEXT(" != ")));
		return bAssignment || bCustomLogical || bConditionalWithComparison;
	}

	static bool IsForkExpressionBoundary(const FExpressionForms& Forms)
	{
		return ContainsForkExpressionBoundary(Forms.Unparenthesized) ||
			ContainsForkExpressionBoundary(Forms.LeftParenthesized) ||
			ContainsForkExpressionBoundary(Forms.RightParenthesized);
	}

	FExecutionSnapshot ExecuteAndCapture(asIScriptEngine& Engine,
		asIScriptFunction* Function,
		FPrecedenceRecorder& Recorder,
		const FNativeCaseContext& Case,
		const TCHAR* Role)
	{
		using namespace AngelscriptNativeTestSupport;

		FExecutionSnapshot Snapshot;
		FNoDiscardAsserter LocalAssert(*TestRunner);
		Recorder.Reset();
		if (!LocalAssert.IsNotNull(
			Function,
			*Case.Describe(
				*FString::Printf(TEXT("%s function should resolve by exact declaration"), Role))))
		{
			return Snapshot;
		}

		asIScriptContext* const Context = Engine.CreateContext();
		if (!LocalAssert.IsNotNull(
			Context,
			*Case.Describe(
				*FString::Printf(TEXT("%s function should create a raw SDK context"), Role))))
		{
			return Snapshot;
		}
		ON_SCOPE_EXIT
		{
			Context->Release();
		};

		Snapshot.ExecuteResult = PrepareAndExecute(Context, Function);
		if (!LocalAssert.AreEqual(
			static_cast<int32>(asEXECUTION_FINISHED),
			Snapshot.ExecuteResult,
			*Case.Describe(
				*FString::Printf(TEXT("%s expression should execute to completion"), Role))))
		{
			return Snapshot;
		}
		if (Snapshot.ExecuteResult == asEXECUTION_FINISHED)
		{
			Snapshot.ReturnValue = static_cast<int32>(Context->GetReturnDWord());
		}
		Snapshot.Markers = Recorder.Markers;
		return Snapshot;
	}

	void VerifyCell(asIScriptEngine& Engine,
		asIScriptModule& Module,
		FPrecedenceRecorder& Recorder,
		const FNativeCaseContext& Case,
		const FExpressionForms& Forms,
		const FGroupingCase& GroupingCase)
	{
		using namespace AngelscriptNativeTestSupport;

		asIScriptFunction* const Selected =
			GetNativeFunctionByExactDecl(&Module, "int RunSelected()");
		asIScriptFunction* const LeftControl =
			GetNativeFunctionByExactDecl(&Module, "int RunLeftControl()");
		asIScriptFunction* const RightControl =
			GetNativeFunctionByExactDecl(&Module, "int RunRightControl()");
		ASSERT_THAT(IsNotNull(
			Selected,
			*Case.Describe(TEXT("selected precedence function should resolve exactly"))));
		ASSERT_THAT(IsNotNull(
			LeftControl,
			*Case.Describe(TEXT("left-grouped control should resolve exactly"))));
		ASSERT_THAT(IsNotNull(
			RightControl,
			*Case.Describe(TEXT("right-grouped control should resolve exactly"))));
		if (Selected == nullptr || LeftControl == nullptr || RightControl == nullptr)
		{
			return;
		}

		const FExecutionSnapshot SelectedExecution =
			ExecuteAndCapture(Engine, Selected, Recorder, Case, TEXT("selected"));
		const FExecutionSnapshot LeftExecution =
			ExecuteAndCapture(Engine, LeftControl, Recorder, Case, TEXT("left control"));
		const FExecutionSnapshot RightExecution =
			ExecuteAndCapture(Engine, RightControl, Recorder, Case, TEXT("right control"));
		const bool bUsesLeft = SelectedUsesLeftControl(Forms, GroupingCase);
		const FExecutionSnapshot& ExpectedExecution = bUsesLeft ? LeftExecution : RightExecution;
		ASSERT_THAT(AreEqual(
			ExpectedExecution.ReturnValue,
			SelectedExecution.ReturnValue,
			*Case.Describe(TEXT("selected expression result should match the independently chosen "
								"explicit grouping"))));
		ASSERT_THAT(IsTrue(
			AreMarkersEqual(ExpectedExecution.Markers, SelectedExecution.Markers),
			*Case.Describe(TEXT("selected expression should match the exact operator and "
								"conversion trace of its expected grouping"))));
		ASSERT_THAT(IsTrue(
			SelectedExecution.Markers.Num() > 0,
			*Case.Describe(TEXT("selected expression should execute at least one observable "
								"operator or conversion"))));

		const FBytecodeSnapshot SelectedBytecode = CaptureBytecode(Selected);
		const FBytecodeSnapshot LeftBytecode = CaptureBytecode(LeftControl);
		const FBytecodeSnapshot RightBytecode = CaptureBytecode(RightControl);
		const FBytecodeSnapshot& ExpectedBytecode = bUsesLeft ? LeftBytecode : RightBytecode;
		ASSERT_THAT(IsTrue(
			SelectedBytecode.Words.Num() > 0,
			*Case.Describe(TEXT("selected expression should publish non-empty raw bytecode"))));
		ASSERT_THAT(IsTrue(
			ExpectedBytecode.Words.Num() > 0,
			*Case.Describe(TEXT("expected grouped control should publish non-empty raw bytecode"))));
		ASSERT_THAT(IsTrue(
			AreBytecodesEqual(SelectedBytecode, ExpectedBytecode),
			*Case.Describe(TEXT("selected expression bytecode should exactly match its "
								"independently selected grouped control"))));
		if (Forms.bUsesControlFlow)
		{
			ASSERT_THAT(IsTrue(
				SelectedBytecode.BranchCount > 0,
				*Case.Describe(TEXT("logical, conditional, or conditional-lvalue expression should "
									"retain branch bytecode"))));
		}
	}

	void RunPrecedenceCell(FNativeTestEngine& Engine,
		FPrecedenceRecorder& Recorder,
		const FGroupingCase& GroupingCase,
		const FLevelCase& LeftLevel,
		const FLevelCase& RightLevel)
	{
		using namespace AngelscriptNativeTestSupport;

		const FNativeCaseContext Case(MakeNativeCaseId("LANG-EXPR-PRECEDENCE",
			{
				ANSI_TO_TCHAR(GroupingCase.CatalogName),
				ANSI_TO_TCHAR(LeftLevel.CatalogName),
				ANSI_TO_TCHAR(RightLevel.CatalogName),
			}));
		const FString Suffix = MakeSuffix(GroupingCase, LeftLevel, RightLevel);
		const FString ModuleName = TEXT("ExpressionPrecedence_") + Suffix;
		const FExpressionForms Forms = BuildPrecedenceExpressions(LeftLevel, RightLevel);
		const FString Source = BuildPrecedenceSource(Forms, GroupingCase);
		asIScriptModule* const Module = CompileAndReport(
			Engine, Case, ModuleName, Source, IsForkExpressionBoundary(Forms));
		if (Module != nullptr)
		{
			VerifyCell(*Engine.Get(), *Module, Recorder, Case, Forms, GroupingCase);
		}

		const FTCHARToUTF8 ModuleNameUtf8(*ModuleName);
		Engine.Get()->DiscardModule(ModuleNameUtf8.Get());
		ASSERT_THAT(IsNull(
			Engine.Get()->GetModule(ModuleNameUtf8.Get(), asGM_ONLY_IF_EXISTS),
			*Case.Describe(TEXT("precedence cell should discard its isolated module"))));
	}

	void RunAssociativityCell(FNativeTestEngine& Engine,
		FPrecedenceRecorder& Recorder,
		const FGroupingCase& GroupingCase,
		const FLevelCase& LevelCase,
		const FSequenceCase& SequenceCase)
	{
		using namespace AngelscriptNativeTestSupport;

		const FNativeCaseContext Case(MakeNativeCaseId("LANG-EXPR-ASSOCIATIVITY",
			{
				ANSI_TO_TCHAR(GroupingCase.CatalogName),
				ANSI_TO_TCHAR(LevelCase.CatalogName),
				ANSI_TO_TCHAR(SequenceCase.CatalogName),
			}));
		const FString Suffix = MakeAssociativitySuffix(GroupingCase, LevelCase, SequenceCase);
		const FString ModuleName = TEXT("ExpressionAssociativity_") + Suffix;
		const FExpressionForms Forms = BuildAssociativityExpressions(LevelCase, SequenceCase);
		const FString Source = BuildPrecedenceSource(Forms, GroupingCase);
		asIScriptModule* const Module = CompileAndReport(
			Engine, Case, ModuleName, Source, IsForkExpressionBoundary(Forms));
		if (Module != nullptr)
		{
			VerifyCell(*Engine.Get(), *Module, Recorder, Case, Forms, GroupingCase);
		}

		const FTCHARToUTF8 ModuleNameUtf8(*ModuleName);
		Engine.Get()->DiscardModule(ModuleNameUtf8.Get());
		ASSERT_THAT(IsNull(
			Engine.Get()->GetModule(ModuleNameUtf8.Get(), asGM_ONLY_IF_EXISTS),
			*Case.Describe(TEXT("associativity cell should discard its isolated module"))));
	}

public:
	TEST_METHOD(LevelsByLevelAndGrouping)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT("LANG-EXPR-PRECEDENCE",
			ENativeEvidence::Compile | ENativeEvidence::Runtime | ENativeEvidence::Bytecode);

		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};
		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(
			ScriptEngine,
			TEXT("Precedence product should create a standalone raw SDK engine")));
		if (ScriptEngine == nullptr)
		{
			return;
		}

		FPrecedenceRecorder Recorder;
		ASSERT_THAT(IsTrue(
			RegisterPrecedenceFunctions(*ScriptEngine, Recorder),
			TEXT("Precedence product should register its observable operation recorder")));
		for (const FGroupingCase& GroupingCase : GroupingCases)
		{
			for (const FLevelCase& LeftLevel : LevelCases)
			{
				for (const FLevelCase& RightLevel : LevelCases)
				{
					RunPrecedenceCell(Engine, Recorder, GroupingCase, LeftLevel, RightLevel);
				}
			}
		}
	}

	TEST_METHOD(LevelsBySequenceAndGrouping)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT("LANG-EXPR-ASSOCIATIVITY",
			ENativeEvidence::Compile | ENativeEvidence::Runtime | ENativeEvidence::Bytecode);

		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};
		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(
			ScriptEngine,
			TEXT("Associativity product should create a standalone raw SDK engine")));
		if (ScriptEngine == nullptr)
		{
			return;
		}

		FPrecedenceRecorder Recorder;
		ASSERT_THAT(IsTrue(
			RegisterPrecedenceFunctions(*ScriptEngine, Recorder),
			TEXT("Associativity product should register its observable operation recorder")));
		for (const FGroupingCase& GroupingCase : GroupingCases)
		{
			for (const FLevelCase& LevelCase : LevelCases)
			{
				for (const FSequenceCase& SequenceCase : SequenceCases)
				{
					RunAssociativityCell(Engine, Recorder, GroupingCase, LevelCase, SequenceCase);
				}
			}
		}
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
