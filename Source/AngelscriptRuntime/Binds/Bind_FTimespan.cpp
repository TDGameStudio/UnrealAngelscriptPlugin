#include "Bind_FTimespan.h"

#include "AngelscriptBinds.h"

/**
 * FTimespan binding surface.
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | AngelScript usage signature                                                                                                  | Purpose / parameter notes                                                                                            |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FTimespan Value(int64 Ticks);                                                                                                | Constructs a duration from 100-nanosecond ticks.                                                                     |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FTimespan Value(int32 Hours, int32 Minutes, int32 Seconds);                                                                  | Constructs a duration from hour, minute, and second components.                                                      |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FTimespan Value(int32 Days, int32 Hours, int32 Minutes, int32 Seconds);                                                      | Constructs a duration from day through second components.                                                            |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FTimespan Value(int32 Days, int32 Hours, int32 Minutes, int32 Seconds, int32 FractionNano);                                  | Constructs a duration with nanosecond sub-second precision.                                                          |
 * |                                                                                                                              | @param FractionNano Sub-second nanoseconds added to the duration.                                                    |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FTimespan Result = Span + Other;                                                                                             | Adds two durations.                                                                                                  |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | Span += Other;                                                                                                               | Adds Other to Span in place.                                                                                         |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FTimespan Result = -Span;                                                                                                    | Returns the negated duration.                                                                                        |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FTimespan Result = Span - Other;                                                                                             | Subtracts Other from Span.                                                                                           |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | Span -= Other;                                                                                                               | Subtracts Other from Span in place.                                                                                  |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FTimespan Result = Span * Scalar;                                                                                            | Scales the duration by Scalar.                                                                                       |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | Span *= Scalar;                                                                                                              | Scales Span by Scalar in place.                                                                                      |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FTimespan Result = Span / Scalar;                                                                                            | Divides the duration by Scalar.                                                                                      |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | Span /= Scalar;                                                                                                              | Divides Span by Scalar in place.                                                                                     |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FTimespan Result = Span % Other;                                                                                             | Returns the remainder after division by Other.                                                                       |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | Span %= Other;                                                                                                               | Replaces Span with its remainder after division by Other.                                                            |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | bool bLess = Span < Other;                                                                                                   | Reports whether Span is shorter than Other.                                                                          |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | bool bLessOrEqual = Span <= Other;                                                                                           | Reports whether Span is no longer than Other.                                                                        |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | bool bGreater = Span > Other;                                                                                                | Reports whether Span is longer than Other.                                                                           |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | bool bGreaterOrEqual = Span >= Other;                                                                                        | Reports whether Span is no shorter than Other.                                                                       |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | bool bEqual = Span == Other;                                                                                                 | Reports whether both durations contain the same tick count.                                                          |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | int32 FTimespan.GetDays() const;                                                                                             | Returns the signed whole-day component.                                                                              |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FTimespan FTimespan.GetDuration();                                                                                           | Returns the absolute duration.                                                                                       |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | int32 FTimespan.GetFractionMicro() const;                                                                                    | Returns the sub-millisecond remainder in microseconds.                                                               |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | int32 FTimespan.GetFractionMilli() const;                                                                                    | Returns the sub-second remainder in milliseconds.                                                                    |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | int32 FTimespan.GetFractionNano() const;                                                                                     | Returns the sub-microsecond remainder in nanoseconds.                                                                |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | int32 FTimespan.GetFractionTicks() const;                                                                                    | Returns the sub-second remainder in 100-nanosecond ticks.                                                            |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | int32 FTimespan.GetHours() const;                                                                                            | Returns the signed hour component after whole days.                                                                  |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | int32 FTimespan.GetMinutes() const;                                                                                          | Returns the signed minute component after whole hours.                                                               |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | int32 FTimespan.GetSeconds() const;                                                                                          | Returns the signed second component after whole minutes.                                                             |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | int64 FTimespan.GetTicks() const;                                                                                            | Returns the total duration in 100-nanosecond ticks.                                                                  |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | float64 FTimespan.GetTotalDays() const;                                                                                      | Returns the complete duration expressed in fractional days.                                                          |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | float64 FTimespan.GetTotalHours() const;                                                                                     | Returns the complete duration expressed in fractional hours.                                                         |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | float64 FTimespan.GetTotalMicroseconds() const;                                                                              | Returns the complete duration expressed in microseconds.                                                             |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | float64 FTimespan.GetTotalMilliseconds() const;                                                                              | Returns the complete duration expressed in milliseconds.                                                             |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | float64 FTimespan.GetTotalMinutes() const;                                                                                   | Returns the complete duration expressed in fractional minutes.                                                       |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | float64 FTimespan.GetTotalSeconds() const;                                                                                   | Returns the complete duration expressed in fractional seconds.                                                       |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | bool FTimespan.IsZero() const;                                                                                               | Reports whether the duration contains zero ticks.                                                                    |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FTimespan FTimespan::FromDays(float64 Days);                                                                                 | Builds a duration from fractional days.                                                                              |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FTimespan FTimespan::FromHours(float64 Hours);                                                                               | Builds a duration from fractional hours.                                                                             |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FTimespan FTimespan::FromMicroseconds(float64 Microseconds);                                                                 | Builds a duration from microseconds.                                                                                 |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FTimespan FTimespan::FromMilliseconds(float64 Milliseconds);                                                                 | Builds a duration from milliseconds.                                                                                 |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FTimespan FTimespan::FromMinutes(float64 Minutes);                                                                           | Builds a duration from fractional minutes.                                                                           |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FTimespan FTimespan::FromSeconds(float64 Seconds);                                                                           | Builds a duration from fractional seconds.                                                                           |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FTimespan FTimespan::MaxValue();                                                                                             | Returns the greatest representable positive duration.                                                                |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FTimespan FTimespan::MinValue();                                                                                             | Returns the least representable negative duration.                                                                   |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FTimespan FTimespan::Zero();                                                                                                 | Returns a zero-tick duration.                                                                                        |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | float64 FTimespan::Ratio(FTimespan Dividend, FTimespan Divisor);                                                             | Returns Dividend divided by Divisor as a scalar ratio.                                                               |
 * |                                                                                                                              | @param Divisor Duration used as the denominator.                                                                     |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FString FTimespan.ToString() const;                                                                                          | Formats the duration using UE's default timespan representation.                                                     |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FString FTimespan.ToString(const FString Format) const;                                                                      | Formats the duration with a supplied UE timespan pattern.                                                            |
 * |                                                                                                                              | @param Format UE timespan formatting pattern.                                                                        |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 */

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
