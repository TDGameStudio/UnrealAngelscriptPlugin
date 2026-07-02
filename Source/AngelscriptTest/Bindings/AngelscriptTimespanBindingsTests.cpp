// ============================================================================
// AngelscriptTimespanBindingsTests.cpp
//
// FTimespan binding coverage — CQTest refactor. Automation IDs:
//   Angelscript.TestModule.Bindings.Timespan.FAngelscriptTimespanBindingsTest.*
//
// Sections:
//   Construction       — tick-based, whole-component, detailed (5-arg) ctors
//   ComponentAccess    — GetDays/Hours/Minutes/Seconds/FractionXxx/TotalXxx
//   Formatting         — ToString default, ToString with custom format
//   Arithmetic         — negate, GetDuration, modulo, Ratio
//   MutableOperators   — +=, -=, *=, /=, %=
//   Comparison         — MaxValue/MinValue ordering via opCmp
//
// CQTest adaptation notes:
//   Uses $TOKEN$ substitution from native FTimespan baselines computed at test time.
// ============================================================================

#include "CQTest.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestModuleScope.h"
#include "AngelscriptTestExecute.h"

#include "Misc/Timespan.h"

#if WITH_ANGELSCRIPT_UNITTESTS


// ----------------------------------------------------------------------------
// Profile
// ----------------------------------------------------------------------------


// ----------------------------------------------------------------------------
// Test class
// ----------------------------------------------------------------------------

TEST_CLASS_WITH_FLAGS(FAngelscriptTimespanBindingsTest,
	"Angelscript.TestModule.Bindings.Timespan",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	struct FTSBaseline
	{
		int64 Ticks = 0;
		int32 Days = 0;
		int32 Hours = 0;
		int32 Minutes = 0;
		int32 Seconds = 0;
		int32 FractionMicro = 0;
		int32 FractionMilli = 0;
		int32 FractionNano = 0;
		int32 FractionTicks = 0;
		double TotalHours = 0.0;
		double TotalMinutes = 0.0;
		double TotalSeconds = 0.0;
		FString DefaultString;
		FString FormattedString;
	};

	static FTSBaseline CaptureTS(const FTimespan& V, const TCHAR* Fmt = nullptr)
	{
		FTSBaseline B;
		B.Ticks = V.GetTicks();
		B.Days = V.GetDays();
		B.Hours = V.GetHours();
		B.Minutes = V.GetMinutes();
		B.Seconds = V.GetSeconds();
		B.FractionMicro = V.GetFractionMicro();
		B.FractionMilli = V.GetFractionMilli();
		B.FractionNano = V.GetFractionNano();
		B.FractionTicks = V.GetFractionTicks();
		B.TotalHours = V.GetTotalHours();
		B.TotalMinutes = V.GetTotalMinutes();
		B.TotalSeconds = V.GetTotalSeconds();
		B.DefaultString = V.ToString();
		if (Fmt) B.FormattedString = V.ToString(Fmt);
		return B;
	}

	static FString I64(int64 V) { return FString::Printf(TEXT("%lld"), static_cast<long long>(V)); }
	static FString F64(double V) { return FString::Printf(TEXT("%.17g"), V); }

	static void InjectTSTokens(FString& S, const TCHAR* Prefix, const FTSBaseline& B)
	{
		S.ReplaceInline(*FString::Printf(TEXT("$%s_TICKS$"), Prefix), *I64(B.Ticks), ESearchCase::CaseSensitive);
		S.ReplaceInline(*FString::Printf(TEXT("$%s_DAYS$"), Prefix), *FString::FromInt(B.Days), ESearchCase::CaseSensitive);
		S.ReplaceInline(*FString::Printf(TEXT("$%s_HOURS$"), Prefix), *FString::FromInt(B.Hours), ESearchCase::CaseSensitive);
		S.ReplaceInline(*FString::Printf(TEXT("$%s_MINUTES$"), Prefix), *FString::FromInt(B.Minutes), ESearchCase::CaseSensitive);
		S.ReplaceInline(*FString::Printf(TEXT("$%s_SECONDS$"), Prefix), *FString::FromInt(B.Seconds), ESearchCase::CaseSensitive);
		S.ReplaceInline(*FString::Printf(TEXT("$%s_FMICRO$"), Prefix), *FString::FromInt(B.FractionMicro), ESearchCase::CaseSensitive);
		S.ReplaceInline(*FString::Printf(TEXT("$%s_FMILLI$"), Prefix), *FString::FromInt(B.FractionMilli), ESearchCase::CaseSensitive);
		S.ReplaceInline(*FString::Printf(TEXT("$%s_FNANO$"), Prefix), *FString::FromInt(B.FractionNano), ESearchCase::CaseSensitive);
		S.ReplaceInline(*FString::Printf(TEXT("$%s_FTICKS$"), Prefix), *FString::FromInt(B.FractionTicks), ESearchCase::CaseSensitive);
		S.ReplaceInline(*FString::Printf(TEXT("$%s_THOURS$"), Prefix), *F64(B.TotalHours), ESearchCase::CaseSensitive);
		S.ReplaceInline(*FString::Printf(TEXT("$%s_TMINUTES$"), Prefix), *F64(B.TotalMinutes), ESearchCase::CaseSensitive);
		S.ReplaceInline(*FString::Printf(TEXT("$%s_TSECONDS$"), Prefix), *F64(B.TotalSeconds), ESearchCase::CaseSensitive);
		S.ReplaceInline(*FString::Printf(TEXT("$%s_DEFSTR$"), Prefix), *B.DefaultString.ReplaceCharWithEscapedChar(), ESearchCase::CaseSensitive);
		S.ReplaceInline(*FString::Printf(TEXT("$%s_FMTSTR$"), Prefix), *B.FormattedString.ReplaceCharWithEscapedChar(), ESearchCase::CaseSensitive);
	}

public:
	BEFORE_ALL()
	{
		ASTEST_CREATE_ENGINE();
	}

	AFTER_ALL()
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		ASTEST_RESET_ENGINE(Engine);
	}

	// ====================================================================
	// Section: Construction
	// ====================================================================

	TEST_METHOD(Construction)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		const FTimespan TicksBased(900000000);
		const FTimespan Whole(1, 2, 3, 4);
		const FTimespan Detailed(1, 2, 3, 4, 500000000);
		FTSBaseline TB = CaptureTS(TicksBased);
		FTSBaseline WB = CaptureTS(Whole);
		FTSBaseline DB = CaptureTS(Detailed);

		FString ConstructionSource = ASTEST_AS(R"AS(
			int Timespan_TicksCtor_Ticks()
			{
				FTimespan T(int64($TB_TICKS$));
				return (T.GetTicks() == int64($TB_TICKS$)) ? 1 : 0;
			}

			int Timespan_WholeCtor_Ticks()
			{
				FTimespan T(1, 2, 3, 4);
				return (T.GetTicks() == int64($WB_TICKS$)) ? 1 : 0;
			}

			int Timespan_DetailedCtor_Ticks()
			{
				FTimespan T(1, 2, 3, 4, 500000000);
				return (T.GetTicks() == int64($DB_TICKS$)) ? 1 : 0;
			}
			)AS");

		InjectTSTokens(ConstructionSource, TEXT("TB"), TB);
		InjectTSTokens(ConstructionSource, TEXT("WB"), WB);
		InjectTSTokens(ConstructionSource, TEXT("DB"), DB);

		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASTimespan_Construction"), ConstructionSource);
		if (!Mod.IsValid()) return;
		auto& M = Mod.GetModule();

		ASSERT_THAT(IsTrue(ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int Timespan_TicksCtor_Ticks()"), TEXT("Tick-based ctor should produce correct ticks"), 1), TEXT("Tick-based ctor should produce correct ticks")));
		ASSERT_THAT(IsTrue(ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int Timespan_WholeCtor_Ticks()"), TEXT("4-arg ctor should produce correct ticks"), 1), TEXT("4-arg ctor should produce correct ticks")));
		ASSERT_THAT(IsTrue(ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int Timespan_DetailedCtor_Ticks()"), TEXT("5-arg ctor should produce correct ticks"), 1), TEXT("5-arg ctor should produce correct ticks")));
	}

	// ====================================================================
	// Section: ComponentAccess
	// ====================================================================

	TEST_METHOD(ComponentAccess)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		const FTimespan TicksBased(900000000);
		const FTimespan Detailed(1, 2, 3, 4, 500000000);
		FTSBaseline TB = CaptureTS(TicksBased);
		FTSBaseline DB = CaptureTS(Detailed);

		FString ComponentAccessSource = ASTEST_AS(R"AS(
			bool NearlyEqual(float64 A, float64 B, float64 Tol)
			{
				return A >= B - Tol && A <= B + Tol;
			}

			int Timespan_TicksBased_Components()
			{
				FTimespan T(int64($TB_TICKS$));
				return (T.GetDays() == $TB_DAYS$ && T.GetHours() == $TB_HOURS$ && T.GetMinutes() == $TB_MINUTES$ && T.GetSeconds() == $TB_SECONDS$) ? 1 : 0;
			}

			int Timespan_TicksBased_Fractions()
			{
				FTimespan T(int64($TB_TICKS$));
				return (T.GetFractionMicro() == $TB_FMICRO$ && T.GetFractionMilli() == $TB_FMILLI$ && T.GetFractionNano() == $TB_FNANO$ && T.GetFractionTicks() == $TB_FTICKS$) ? 1 : 0;
			}

			int Timespan_TicksBased_Totals()
			{
				FTimespan T(int64($TB_TICKS$));
				return (NearlyEqual(T.GetTotalHours(), $TB_THOURS$, 0.000000001) && NearlyEqual(T.GetTotalMinutes(), $TB_TMINUTES$, 0.000000001) && NearlyEqual(T.GetTotalSeconds(), $TB_TSECONDS$, 0.000000001)) ? 1 : 0;
			}

			int Timespan_Detailed_Components()
			{
				FTimespan T(1, 2, 3, 4, 500000000);
				return (T.GetDays() == $DB_DAYS$ && T.GetHours() == $DB_HOURS$ && T.GetMinutes() == $DB_MINUTES$ && T.GetSeconds() == $DB_SECONDS$) ? 1 : 0;
			}

			int Timespan_Detailed_Fractions()
			{
				FTimespan T(1, 2, 3, 4, 500000000);
				return (T.GetFractionMicro() == $DB_FMICRO$ && T.GetFractionMilli() == $DB_FMILLI$ && T.GetFractionNano() == $DB_FNANO$ && T.GetFractionTicks() == $DB_FTICKS$) ? 1 : 0;
			}

			int Timespan_Detailed_Totals()
			{
				FTimespan T(1, 2, 3, 4, 500000000);
				return (NearlyEqual(T.GetTotalHours(), $DB_THOURS$, 0.000000001) && NearlyEqual(T.GetTotalMinutes(), $DB_TMINUTES$, 0.000000001) && NearlyEqual(T.GetTotalSeconds(), $DB_TSECONDS$, 0.000000001)) ? 1 : 0;
			}
			)AS");

		InjectTSTokens(ComponentAccessSource, TEXT("TB"), TB);
		InjectTSTokens(ComponentAccessSource, TEXT("DB"), DB);

		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASTimespan_ComponentAccess"), ComponentAccessSource);
		if (!Mod.IsValid()) return;
		auto& M = Mod.GetModule();

		ASSERT_THAT(IsTrue(ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int Timespan_TicksBased_Components()"), TEXT("Tick-based Days/Hours/Minutes/Seconds should match native"), 1), TEXT("Tick-based Days/Hours/Minutes/Seconds should match native")));
		ASSERT_THAT(IsTrue(ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int Timespan_TicksBased_Fractions()"), TEXT("Tick-based fraction accessors should match native"), 1), TEXT("Tick-based fraction accessors should match native")));
		ASSERT_THAT(IsTrue(ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int Timespan_TicksBased_Totals()"), TEXT("Tick-based total accessors should match native"), 1), TEXT("Tick-based total accessors should match native")));
		ASSERT_THAT(IsTrue(ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int Timespan_Detailed_Components()"), TEXT("Detailed Days/Hours/Minutes/Seconds should match native"), 1), TEXT("Detailed Days/Hours/Minutes/Seconds should match native")));
		ASSERT_THAT(IsTrue(ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int Timespan_Detailed_Fractions()"), TEXT("Detailed fraction accessors should match native"), 1), TEXT("Detailed fraction accessors should match native")));
		ASSERT_THAT(IsTrue(ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int Timespan_Detailed_Totals()"), TEXT("Detailed total accessors should match native"), 1), TEXT("Detailed total accessors should match native")));
	}

	// ====================================================================
	// Section: Formatting
	// ====================================================================

	TEST_METHOD(Formatting)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		const FTimespan TicksBased(900000000);
		const FTimespan Detailed(1, 2, 3, 4, 500000000);
		const TCHAR* DetailedFormat = TEXT("%d.%h:%m:%s");
		FTSBaseline TB = CaptureTS(TicksBased);
		FTSBaseline DB = CaptureTS(Detailed, DetailedFormat);

		FString FormattingSource = ASTEST_AS(R"AS(
			int Timespan_TicksBased_DefaultString()
			{
				FTimespan T(int64($TB_TICKS$));
				return (T.ToString() == "$TB_DEFSTR$") ? 1 : 0;
			}

			int Timespan_Detailed_DefaultString()
			{
				FTimespan T(1, 2, 3, 4, 500000000);
				return (T.ToString() == "$DB_DEFSTR$") ? 1 : 0;
			}

			int Timespan_Detailed_FormattedString()
			{
				FTimespan T(1, 2, 3, 4, 500000000);
				return (T.ToString("$DFMT$") == "$DB_FMTSTR$") ? 1 : 0;
			}
			)AS");

		InjectTSTokens(FormattingSource, TEXT("TB"), TB);
		InjectTSTokens(FormattingSource, TEXT("DB"), DB);
		FormattingSource.ReplaceInline(TEXT("$DFMT$"), *FString(DetailedFormat).ReplaceCharWithEscapedChar(), ESearchCase::CaseSensitive);

		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASTimespan_Formatting"), FormattingSource);
		if (!Mod.IsValid()) return;
		auto& M = Mod.GetModule();

		ASSERT_THAT(IsTrue(ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int Timespan_TicksBased_DefaultString()"), TEXT("Tick-based ToString should match native"), 1), TEXT("Tick-based ToString should match native")));
		ASSERT_THAT(IsTrue(ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int Timespan_Detailed_DefaultString()"), TEXT("Detailed ToString should match native"), 1), TEXT("Detailed ToString should match native")));
		ASSERT_THAT(IsTrue(ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int Timespan_Detailed_FormattedString()"), TEXT("Detailed formatted ToString should match native"), 1), TEXT("Detailed formatted ToString should match native")));
	}

	// ====================================================================
	// Section: Arithmetic
	// ====================================================================

	TEST_METHOD(Arithmetic)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		const FTimespan Dividend = FTimespan::FromMinutes(95.0);
		const FTimespan Divisor = FTimespan::FromMinutes(30.0);
		FTSBaseline NegDur = CaptureTS((-Dividend).GetDuration());
		FTSBaseline ModRes = CaptureTS(Dividend % Divisor);
		double Ratio = FTimespan::Ratio(Dividend, Divisor);

		FString ArithmeticSource = ASTEST_AS(R"AS(
			bool NearlyEqual(float64 A, float64 B, float64 Tol)
			{
				return A >= B - Tol && A <= B + Tol;
			}

			int Timespan_Negate_Duration()
			{
				FTimespan Dividend = FTimespan::FromMinutes(95.0);
				FTimespan Neg = -Dividend;
				return (Neg.GetDuration().GetTicks() == int64($NEGDUR_TICKS$)) ? 1 : 0;
			}

			int Timespan_Modulo()
			{
				FTimespan Dividend = FTimespan::FromMinutes(95.0);
				FTimespan Divisor = FTimespan::FromMinutes(30.0);
				FTimespan R = Dividend % Divisor;
				return (R.GetTicks() == int64($MOD_TICKS$)) ? 1 : 0;
			}

			int Timespan_Ratio()
			{
				FTimespan Dividend = FTimespan::FromMinutes(95.0);
				FTimespan Divisor = FTimespan::FromMinutes(30.0);
				return NearlyEqual(FTimespan::Ratio(Dividend, Divisor), $RATIO$, 0.000000001) ? 1 : 0;
			}
			)AS");

		ArithmeticSource.ReplaceInline(TEXT("$NEGDUR_TICKS$"), *I64(NegDur.Ticks), ESearchCase::CaseSensitive);
		ArithmeticSource.ReplaceInline(TEXT("$MOD_TICKS$"), *I64(ModRes.Ticks), ESearchCase::CaseSensitive);
		ArithmeticSource.ReplaceInline(TEXT("$RATIO$"), *F64(Ratio), ESearchCase::CaseSensitive);

		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASTimespan_Arithmetic"), ArithmeticSource);
		if (!Mod.IsValid()) return;
		auto& M = Mod.GetModule();

		ASSERT_THAT(IsTrue(ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int Timespan_Negate_Duration()"), TEXT("Negate then GetDuration should match native"), 1), TEXT("Negate then GetDuration should match native")));
		ASSERT_THAT(IsTrue(ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int Timespan_Modulo()"), TEXT("Modulo operator should match native"), 1), TEXT("Modulo operator should match native")));
		ASSERT_THAT(IsTrue(ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int Timespan_Ratio()"), TEXT("Ratio should match native"), 1), TEXT("Ratio should match native")));
	}

	// ====================================================================
	// Section: MutableOperators
	// ====================================================================

	TEST_METHOD(MutableOperators)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		// Compute native baselines for the mutable chain
		FTimespan Mutable = FTimespan::FromMinutes(10.0);
		Mutable += FTimespan::FromSeconds(45.0);
		FTSBaseline AfterAdd = CaptureTS(Mutable);

		Mutable -= FTimespan::FromSeconds(15.0);
		FTSBaseline AfterSub = CaptureTS(Mutable);

		Mutable *= 2.0;
		FTSBaseline AfterMul = CaptureTS(Mutable);

		Mutable /= 4.0;
		FTSBaseline AfterDiv = CaptureTS(Mutable);

		Mutable %= FTimespan::FromMinutes(3.0);
		FTSBaseline AfterMod = CaptureTS(Mutable);

		FString MutableOperatorsSource = ASTEST_AS(R"AS(
			int Timespan_MutAdd()
			{
				FTimespan T = FTimespan::FromMinutes(10.0);
				T += FTimespan::FromSeconds(45.0);
				return (T.GetTicks() == int64($ADD_TICKS$)) ? 1 : 0;
			}

			int Timespan_MutSub()
			{
				FTimespan T = FTimespan::FromMinutes(10.0);
				T += FTimespan::FromSeconds(45.0);
				T -= FTimespan::FromSeconds(15.0);
				return (T.GetTicks() == int64($SUB_TICKS$)) ? 1 : 0;
			}

			int Timespan_MutMul()
			{
				FTimespan T = FTimespan::FromMinutes(10.0);
				T += FTimespan::FromSeconds(45.0);
				T -= FTimespan::FromSeconds(15.0);
				T *= 2.0;
				return (T.GetTicks() == int64($MUL_TICKS$)) ? 1 : 0;
			}

			int Timespan_MutDiv()
			{
				FTimespan T = FTimespan::FromMinutes(10.0);
				T += FTimespan::FromSeconds(45.0);
				T -= FTimespan::FromSeconds(15.0);
				T *= 2.0;
				T /= 4.0;
				return (T.GetTicks() == int64($DIV_TICKS$)) ? 1 : 0;
			}

			int Timespan_MutMod()
			{
				FTimespan T = FTimespan::FromMinutes(10.0);
				T += FTimespan::FromSeconds(45.0);
				T -= FTimespan::FromSeconds(15.0);
				T *= 2.0;
				T /= 4.0;
				T %= FTimespan::FromMinutes(3.0);
				return (T.GetTicks() == int64($MOD_TICKS$)) ? 1 : 0;
			}
			)AS");

		MutableOperatorsSource.ReplaceInline(TEXT("$ADD_TICKS$"), *I64(AfterAdd.Ticks), ESearchCase::CaseSensitive);
		MutableOperatorsSource.ReplaceInline(TEXT("$SUB_TICKS$"), *I64(AfterSub.Ticks), ESearchCase::CaseSensitive);
		MutableOperatorsSource.ReplaceInline(TEXT("$MUL_TICKS$"), *I64(AfterMul.Ticks), ESearchCase::CaseSensitive);
		MutableOperatorsSource.ReplaceInline(TEXT("$DIV_TICKS$"), *I64(AfterDiv.Ticks), ESearchCase::CaseSensitive);
		MutableOperatorsSource.ReplaceInline(TEXT("$MOD_TICKS$"), *I64(AfterMod.Ticks), ESearchCase::CaseSensitive);

		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASTimespan_MutableOperators"), MutableOperatorsSource);
		if (!Mod.IsValid()) return;
		auto& M = Mod.GetModule();

		ASSERT_THAT(IsTrue(ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int Timespan_MutAdd()"), TEXT("+= should match native"), 1), TEXT("+= should match native")));
		ASSERT_THAT(IsTrue(ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int Timespan_MutSub()"), TEXT("-= should match native"), 1), TEXT("-= should match native")));
		ASSERT_THAT(IsTrue(ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int Timespan_MutMul()"), TEXT("*= should match native"), 1), TEXT("*= should match native")));
		ASSERT_THAT(IsTrue(ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int Timespan_MutDiv()"), TEXT("/= should match native"), 1), TEXT("/= should match native")));
		ASSERT_THAT(IsTrue(ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int Timespan_MutMod()"), TEXT("%= should match native"), 1), TEXT("%= should match native")));
	}

	// ====================================================================
	// Section: Comparison
	// ====================================================================

	TEST_METHOD(Comparison)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASTimespan_Comparison"), ASTEST_AS(R"AS(
			int Timespan_MaxCmpMin()
			{
				return (FTimespan::MaxValue().opCmp(FTimespan::MinValue()) > 0) ? 1 : 0;
			}

			int Timespan_MinCmpMax()
			{
				return (FTimespan::MinValue().opCmp(FTimespan::MaxValue()) < 0) ? 1 : 0;
			}
			)AS"));
		if (!Mod.IsValid()) return;
		auto& M = Mod.GetModule();

		ASSERT_THAT(IsTrue(ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int Timespan_MaxCmpMin()"), TEXT("MaxValue should compare greater than MinValue"), 1), TEXT("MaxValue should compare greater than MinValue")));
		ASSERT_THAT(IsTrue(ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int Timespan_MinCmpMax()"), TEXT("MinValue should compare less than MaxValue"), 1), TEXT("MinValue should compare less than MaxValue")));
	}
};

#endif
