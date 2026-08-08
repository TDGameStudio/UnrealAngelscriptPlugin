#include "AngelscriptBinds.h"

#include "Bind_FTimespan_Functions.h"

namespace
{
	void BindFTimespanFunctions(FAngelscriptBinds& Binds)
	{
		auto FTimespan_ = Binds.ExistingClassForTarget("FTimespan");
		FTimespan_.Constructor(
			"void f(int64 Ticks)",
			&FAngelscriptFTimespanBinds::ConstructTicks,
			"FTimespan",
			true)
			.NoDiscard();
		FTimespan_.Constructor(
			"void f(int32 Hours, int32 Minutes, int32 Seconds)",
			&FAngelscriptFTimespanBinds::ConstructTime,
			"FTimespan",
			true)
			.NoDiscard();
		FTimespan_.Constructor(
			"void f(int32 Days, int32 Hours, int32 Minutes, int32 Seconds)",
			&FAngelscriptFTimespanBinds::ConstructDays,
			"FTimespan",
			true)
			.NoDiscard();
		FTimespan_.Constructor(
			"void f(int32 Days, int32 Hours, int32 Minutes, int32 Seconds, int32 FractionNano)",
			&FAngelscriptFTimespanBinds::ConstructNanoseconds,
			"FTimespan",
			true)
			.NoDiscard();

		FTimespan_.Method("FTimespan opAdd(const FTimespan& Other) const", METHODPR_TRIVIAL(FTimespan, FTimespan, operator+, (const FTimespan&) const));
		FTimespan_.Method("FTimespan& opAddAssign(const FTimespan& Other)", METHODPR_TRIVIAL(FTimespan&, FTimespan, operator+=, (const FTimespan&)));
		FTimespan_.Method("FTimespan opNeg() const", METHODPR_TRIVIAL(FTimespan, FTimespan, operator-, () const));
		FTimespan_.Method("FTimespan opSub(const FTimespan& Other) const", METHODPR_TRIVIAL(FTimespan, FTimespan, operator-, (const FTimespan&) const));
		FTimespan_.Method("FTimespan& opSubAssign(const FTimespan& Other)", METHODPR_TRIVIAL(FTimespan&, FTimespan, operator-=, (const FTimespan&)));
		FTimespan_.Method("FTimespan opMul(float64 Scalar) const", METHODPR_TRIVIAL(FTimespan, FTimespan, operator*, (double) const));
		FTimespan_.Method("FTimespan& opMulAssign(float64 Scalar)", METHODPR_TRIVIAL(FTimespan&, FTimespan, operator*=, (double)));
		FTimespan_.Method("FTimespan opDiv(float64 Scalar) const", METHODPR_TRIVIAL(FTimespan, FTimespan, operator/, (double) const));
		FTimespan_.Method("FTimespan& opDivAssign(float64 Scalar)", METHODPR_TRIVIAL(FTimespan&, FTimespan, operator/=, (double)));
		FTimespan_.Method("FTimespan opMod(const FTimespan& Other) const", METHODPR_TRIVIAL(FTimespan, FTimespan, operator%, (const FTimespan&) const));
		FTimespan_.Method("FTimespan& opModAssign(const FTimespan& Other)", METHODPR_TRIVIAL(FTimespan&, FTimespan, operator%=, (const FTimespan&)));
		FTimespan_.Method("int opCmp(const FTimespan& Other) const", &FAngelscriptFTimespanBinds::Compare);
		FTimespan_.Method("bool opEquals(const FTimespan& Other) const", METHODPR_TRIVIAL(bool, FTimespan, operator==, (const FTimespan&) const));
		FTimespan_.Method("int32 GetDays() const", METHODPR_TRIVIAL(int32, FTimespan, GetDays, () const));
		FTimespan_.Method("FTimespan GetDuration()", METHODPR_TRIVIAL(FTimespan, FTimespan, GetDuration, ()));
		FTimespan_.Method("int32 GetFractionMicro() const", METHODPR_TRIVIAL(int32, FTimespan, GetFractionMicro, () const));
		FTimespan_.Method("int32 GetFractionMilli() const", METHODPR_TRIVIAL(int32, FTimespan, GetFractionMilli, () const));
		FTimespan_.Method("int32 GetFractionNano() const", METHODPR_TRIVIAL(int32, FTimespan, GetFractionNano, () const));
		FTimespan_.Method("int32 GetFractionTicks() const", METHODPR_TRIVIAL(int32, FTimespan, GetFractionTicks, () const));
		FTimespan_.Method("int32 GetHours() const", METHODPR_TRIVIAL(int32, FTimespan, GetHours, () const));
		FTimespan_.Method("int32 GetMinutes() const", METHODPR_TRIVIAL(int32, FTimespan, GetMinutes, () const));
		FTimespan_.Method("int32 GetSeconds() const", METHODPR_TRIVIAL(int32, FTimespan, GetSeconds, () const));
		FTimespan_.Method("int64 GetTicks() const", METHODPR_TRIVIAL(int64, FTimespan, GetTicks, () const));
		FTimespan_.Method("float64 GetTotalDays() const", METHODPR_TRIVIAL(double, FTimespan, GetTotalDays, () const));
		FTimespan_.Method("float64 GetTotalHours() const", METHODPR_TRIVIAL(double, FTimespan, GetTotalHours, () const));
		FTimespan_.Method("float64 GetTotalMicroseconds() const", METHODPR_TRIVIAL(double, FTimespan, GetTotalMicroseconds, () const));
		FTimespan_.Method("float64 GetTotalMilliseconds() const", METHODPR_TRIVIAL(double, FTimespan, GetTotalMilliseconds, () const));
		FTimespan_.Method("float64 GetTotalMinutes() const", METHODPR_TRIVIAL(double, FTimespan, GetTotalMinutes, () const));
		FTimespan_.Method("float64 GetTotalSeconds() const", METHODPR_TRIVIAL(double, FTimespan, GetTotalSeconds, () const));
		FTimespan_.Method("bool IsZero() const", METHODPR_TRIVIAL(bool, FTimespan, IsZero, () const));

		{
			FAngelscriptBinds::FNamespace Namespace(Binds.GetTargetEngine(), "FTimespan");
			Binds.BindGlobalFunctionForTarget("FTimespan FromDays(float64 Days) no_discard", FUNCPR_TRIVIAL(FTimespan, FTimespan::FromDays, (double)));
			Binds.BindGlobalFunctionForTarget("FTimespan FromHours(float64 Hours) no_discard", FUNCPR_TRIVIAL(FTimespan, FTimespan::FromHours, (double)));
			Binds.BindGlobalFunctionForTarget("FTimespan FromMicroseconds(float64 Microseconds) no_discard", FUNCPR_TRIVIAL(FTimespan, FTimespan::FromMicroseconds, (double)));
			Binds.BindGlobalFunctionForTarget("FTimespan FromMilliseconds(float64 Milliseconds) no_discard", FUNCPR_TRIVIAL(FTimespan, FTimespan::FromMilliseconds, (double)));
			Binds.BindGlobalFunctionForTarget("FTimespan FromMinutes(float64 Minutes) no_discard", FUNCPR_TRIVIAL(FTimespan, FTimespan::FromMinutes, (double)));
			Binds.BindGlobalFunctionForTarget("FTimespan FromSeconds(float64 Seconds) no_discard", FUNCPR_TRIVIAL(FTimespan, FTimespan::FromSeconds, (double)));
			Binds.BindGlobalFunctionForTarget("FTimespan MaxValue() no_discard", FUNCPR_TRIVIAL(FTimespan, FTimespan::MaxValue, ()));
			Binds.BindGlobalFunctionForTarget("FTimespan MinValue() no_discard", FUNCPR_TRIVIAL(FTimespan, FTimespan::MinValue, ()));
			Binds.BindGlobalFunctionForTarget("FTimespan Zero() no_discard", FUNCPR_TRIVIAL(FTimespan, FTimespan::Zero, ()));
			Binds.BindGlobalFunctionForTarget("float64 Ratio(FTimespan Dividend, FTimespan Divisor) no_discard", FUNCPR_TRIVIAL(double, FTimespan::Ratio, (FTimespan, FTimespan)));
		}

		FTimespan_.Method("FString ToString() const", METHODPR_TRIVIAL(FString, FTimespan, ToString, () const));
		FTimespan_.Method("FString ToString(const FString Format) const", &FAngelscriptFTimespanBinds::ToStringFormat);
	}
}

AS_FORCE_LINK const FAngelscriptBind Bind_FTimespan(
	TEXT("FTimespan.Functions"),
	EAngelscriptBindPhase::ManualBindings,
	&BindFTimespanFunctions);
