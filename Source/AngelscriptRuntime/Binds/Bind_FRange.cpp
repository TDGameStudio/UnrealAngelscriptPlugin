#include "AngelscriptBinds.h"

#include "Math/Interval.h"
#include "Math/Range.h"
#include "Math/RangeBound.h"

template<typename BoundType, typename ValueType>
struct TAngelscriptRangeBoundBinds
{
	static void Construct(BoundType* Address)
	{
		new (Address) BoundType();
	}

	static void ConstructValue(BoundType* Address, ValueType Value)
	{
		new (Address) BoundType(Value);
	}

	static BoundType MaxLower(const BoundType& A, const BoundType& B)
	{
		return BoundType::MaxLower(A, B);
	}

	static BoundType MaxUpper(const BoundType& A, const BoundType& B)
	{
		return BoundType::MaxUpper(A, B);
	}

	static BoundType MinLower(const BoundType& A, const BoundType& B)
	{
		return BoundType::MinLower(A, B);
	}

	static BoundType MinUpper(const BoundType& A, const BoundType& B)
	{
		return BoundType::MinUpper(A, B);
	}
};

template<typename RangeType, typename BoundType, typename ValueType>
struct TAngelscriptRangeBinds
{
	static void ConstructValue(RangeType* Address, ValueType Value)
	{
		new (Address) RangeType(Value);
	}

	static void ConstructValues(RangeType* Address, ValueType Lower, ValueType Upper)
	{
		new (Address) RangeType(Lower, Upper);
	}

	static void ConstructBounds(RangeType* Address, const BoundType& Lower, const BoundType& Upper)
	{
		new (Address) RangeType(Lower, Upper);
	}

	static BoundType GetLowerBound(const RangeType& Range)
	{
		return Range.GetLowerBound();
	}

	static void SetLowerBound(RangeType& Range, const BoundType& Bound)
	{
		Range.SetLowerBound(Bound);
	}

	static BoundType GetUpperBound(const RangeType& Range)
	{
		return Range.GetUpperBound();
	}

	static void SetUpperBound(RangeType& Range, const BoundType& Bound)
	{
		Range.SetUpperBound(Bound);
	}

	static RangeType DifferenceHull(const RangeType& A, const RangeType& B)
	{
		return RangeType::Hull(A, B);
	}

	static RangeType Hull(const TArray<RangeType>& Ranges)
	{
		return RangeType::Hull(Ranges);
	}

	static RangeType Intersection(const RangeType& A, const RangeType& B)
	{
		return RangeType::Intersection(A, B);
	}

	static RangeType Intersection(const TArray<RangeType>& Ranges)
	{
		return RangeType::Intersection(Ranges);
	}

	static bool Contains(const RangeType& Range, const RangeType& Other)
	{
		return Range.Contains(Other);
	}

	static RangeType GreaterThan(ValueType Value)
	{
		return RangeType::GreaterThan(Value);
	}

	static RangeType LessThan(ValueType Value)
	{
		return RangeType::LessThan(Value);
	}
};

template<typename IntervalType, typename ValueType>
struct TAngelscriptIntervalBinds
{
	static void Construct(IntervalType* Address, ValueType Min, ValueType Max)
	{
		new (Address) IntervalType(Min, Max);
	}
};

using FFloatRangeBoundBinds = TAngelscriptRangeBoundBinds<FFloatRangeBound, float>;
using FInt32RangeBoundBinds = TAngelscriptRangeBoundBinds<FInt32RangeBound, int32>;
using FFloatRangeBinds = TAngelscriptRangeBinds<FFloatRange, FFloatRangeBound, float>;
using FInt32RangeBinds = TAngelscriptRangeBinds<FInt32Range, FInt32RangeBound, int32>;
using FFloatIntervalBinds = TAngelscriptIntervalBinds<FFloatInterval, float>;
using FInt32IntervalBinds = TAngelscriptIntervalBinds<FInt32Interval, int32>;

/**
 * Range and interval binding surface.
 * +--------------------------------------------------------------------------------------------------+-----------------------------------------------------------------------------------------------------------+
 * | AngelScript usage signature                                                                      | Purpose / parameter notes                                                                                 |
 * +--------------------------------------------------------------------------------------------------+-----------------------------------------------------------------------------------------------------------+
 * | FFloatRangeBound Bound(float32 Value);                                                          | Constructs an inclusive closed float bound.                                                                |
 * +--------------------------------------------------------------------------------------------------+-----------------------------------------------------------------------------------------------------------+
 * | float32 Bound.GetValue() const;                                                                  | Returns a closed bound value. Call only after IsClosed(); Open bounds violate UE's native precondition.   |
 * +--------------------------------------------------------------------------------------------------+-----------------------------------------------------------------------------------------------------------+
 * | FFloatRange Range(float32 Lower, float32 Upper);                                                 | Constructs the half-open range [Lower, Upper).                                                            |
 * +--------------------------------------------------------------------------------------------------+-----------------------------------------------------------------------------------------------------------+
 * | TArray<FFloatRange> FFloatRange::Difference(const FFloatRange& A, const FFloatRange& B);       | Returns pieces of A not covered by B.                                                                      |
 * +--------------------------------------------------------------------------------------------------+-----------------------------------------------------------------------------------------------------------+
 * | FFloatInterval Interval(float32 Min, float32 Max);                                               | Constructs an inclusive float interval.                                                                    |
 * +--------------------------------------------------------------------------------------------------+-----------------------------------------------------------------------------------------------------------+
 */
AS_FORCE_LINK const FAngelscriptBind Bind_FRange(
	TEXT("FRange"),
	EAngelscriptBindPhase::PostReflectionBindings,
	[](FAngelscriptBinds& Binds)
	{
		auto FloatBound = Binds.ExistingClassForTarget("FFloatRangeBound");
		FloatBound.Constructor("void f(float32 Value)", &FFloatRangeBoundBinds::ConstructValue, "FFloatRangeBound", true);
		FloatBound.Method("bool opEquals(const FFloatRangeBound& Other) const", METHODPR_TRIVIAL(bool, FFloatRangeBound, operator==, (const FFloatRangeBound&) const));
		FloatBound.Method("float32 GetValue() const", METHOD_TRIVIAL(FFloatRangeBound, GetValue));
		FloatBound.Method("void SetValue(float32 NewValue)", METHOD_TRIVIAL(FFloatRangeBound, SetValue));
		FloatBound.Method("bool IsClosed() const", METHOD_TRIVIAL(FFloatRangeBound, IsClosed));
		FloatBound.Method("bool IsExclusive() const", METHOD_TRIVIAL(FFloatRangeBound, IsExclusive));
		FloatBound.Method("bool IsInclusive() const", METHOD_TRIVIAL(FFloatRangeBound, IsInclusive));
		FloatBound.Method("bool IsOpen() const", METHOD_TRIVIAL(FFloatRangeBound, IsOpen));

		auto Int32Bound = Binds.ExistingClassForTarget("FInt32RangeBound");
		Int32Bound.Constructor("void f(int32 Value)", &FInt32RangeBoundBinds::ConstructValue, "FInt32RangeBound", true);
		Int32Bound.Method("bool opEquals(const FInt32RangeBound& Other) const", METHODPR_TRIVIAL(bool, FInt32RangeBound, operator==, (const FInt32RangeBound&) const));
		Int32Bound.Method("int32 GetValue() const", METHOD_TRIVIAL(FInt32RangeBound, GetValue));
		Int32Bound.Method("void SetValue(int32 NewValue)", METHOD_TRIVIAL(FInt32RangeBound, SetValue));
		Int32Bound.Method("bool IsClosed() const", METHOD_TRIVIAL(FInt32RangeBound, IsClosed));
		Int32Bound.Method("bool IsExclusive() const", METHOD_TRIVIAL(FInt32RangeBound, IsExclusive));
		Int32Bound.Method("bool IsInclusive() const", METHOD_TRIVIAL(FInt32RangeBound, IsInclusive));
		Int32Bound.Method("bool IsOpen() const", METHOD_TRIVIAL(FInt32RangeBound, IsOpen));

		auto FloatRange = Binds.ExistingClassForTarget("FFloatRange");
		FloatRange.Constructor("void f(float32 Value)", &FFloatRangeBinds::ConstructValue, "FFloatRange", true);
		FloatRange.Constructor("void f(float32 Lower, float32 Upper)", &FFloatRangeBinds::ConstructValues, "FFloatRange", true);
		FloatRange.Constructor("void f(const FFloatRangeBound& Lower, const FFloatRangeBound& Upper)", &FFloatRangeBinds::ConstructBounds, "FFloatRange", true);
		FloatRange.Method("bool opEquals(const FFloatRange& Other) const", METHODPR_TRIVIAL(bool, FFloatRange, operator==, (const FFloatRange&) const));
		FloatRange.Method("bool Adjoins(const FFloatRange& Other) const", METHOD_TRIVIAL(FFloatRange, Adjoins));
		FloatRange.Method("bool Conjoins(const FFloatRange& A, const FFloatRange& B) const", METHOD_TRIVIAL(FFloatRange, Conjoins));
		FloatRange.Method("bool Contains(float32 Value) const", METHODPR_TRIVIAL(bool, FFloatRange, Contains, (float) const));
		FloatRange.Method("bool Contains(const FFloatRange& Other) const", &FFloatRangeBinds::Contains);
		FloatRange.Method("bool Contiguous(const FFloatRange& Other) const", METHOD_TRIVIAL(FFloatRange, Contiguous));
		FloatRange.Method("FFloatRangeBound GetLowerBound() const", &FFloatRangeBinds::GetLowerBound);
		FloatRange.Method("void SetLowerBound(const FFloatRangeBound& Bound)", &FFloatRangeBinds::SetLowerBound);
		FloatRange.Method("void SetLowerBoundValue(float32 Value)", METHODPR_TRIVIAL(void, FFloatRange, SetLowerBoundValue, (float)));
		FloatRange.Method("float32 GetLowerBoundValue() const", METHODPR_TRIVIAL(float, FFloatRange, GetLowerBoundValue, () const));
		FloatRange.Method("FFloatRangeBound GetUpperBound() const", &FFloatRangeBinds::GetUpperBound);
		FloatRange.Method("void SetUpperBound(const FFloatRangeBound& Bound)", &FFloatRangeBinds::SetUpperBound);
		FloatRange.Method("void SetUpperBoundValue(float32 Value)", METHODPR_TRIVIAL(void, FFloatRange, SetUpperBoundValue, (float)));
		FloatRange.Method("float32 GetUpperBoundValue() const", METHODPR_TRIVIAL(float, FFloatRange, GetUpperBoundValue, () const));
		FloatRange.Method("bool HasLowerBound() const", METHOD_TRIVIAL(FFloatRange, HasLowerBound));
		FloatRange.Method("bool HasUpperBound() const", METHOD_TRIVIAL(FFloatRange, HasUpperBound));
		FloatRange.Method("bool IsDegenerate() const", METHOD_TRIVIAL(FFloatRange, IsDegenerate));
		FloatRange.Method("bool IsEmpty() const", METHOD_TRIVIAL(FFloatRange, IsEmpty));
		FloatRange.Method("bool Overlaps(const FFloatRange& Other) const", METHOD_TRIVIAL(FFloatRange, Overlaps));
		FloatRange.Method("TArray<FFloatRange> Split(float32 Value) const", METHODPR_TRIVIAL(TArray<FFloatRange>, FFloatRange, Split, (float) const));

		auto Int32Range = Binds.ExistingClassForTarget("FInt32Range");
		Int32Range.Constructor("void f(int32 Value)", &FInt32RangeBinds::ConstructValue, "FInt32Range", true);
		Int32Range.Constructor("void f(int32 Lower, int32 Upper)", &FInt32RangeBinds::ConstructValues, "FInt32Range", true);
		Int32Range.Constructor("void f(const FInt32RangeBound& Lower, const FInt32RangeBound& Upper)", &FInt32RangeBinds::ConstructBounds, "FInt32Range", true);
		Int32Range.Method("bool opEquals(const FInt32Range& Other) const", METHODPR_TRIVIAL(bool, FInt32Range, operator==, (const FInt32Range&) const));
		Int32Range.Method("bool Adjoins(const FInt32Range& Other) const", METHOD_TRIVIAL(FInt32Range, Adjoins));
		Int32Range.Method("bool Conjoins(const FInt32Range& A, const FInt32Range& B) const", METHOD_TRIVIAL(FInt32Range, Conjoins));
		Int32Range.Method("bool Contains(int32 Value) const", METHODPR_TRIVIAL(bool, FInt32Range, Contains, (int32) const));
		Int32Range.Method("bool Contains(const FInt32Range& Other) const", &FInt32RangeBinds::Contains);
		Int32Range.Method("bool Contiguous(const FInt32Range& Other) const", METHOD_TRIVIAL(FInt32Range, Contiguous));
		Int32Range.Method("FInt32RangeBound GetLowerBound() const", &FInt32RangeBinds::GetLowerBound);
		Int32Range.Method("void SetLowerBound(const FInt32RangeBound& Bound)", &FInt32RangeBinds::SetLowerBound);
		Int32Range.Method("void SetLowerBoundValue(int32 Value)", METHODPR_TRIVIAL(void, FInt32Range, SetLowerBoundValue, (int32)));
		Int32Range.Method("int32 GetLowerBoundValue() const", METHODPR_TRIVIAL(int32, FInt32Range, GetLowerBoundValue, () const));
		Int32Range.Method("FInt32RangeBound GetUpperBound() const", &FInt32RangeBinds::GetUpperBound);
		Int32Range.Method("void SetUpperBound(const FInt32RangeBound& Bound)", &FInt32RangeBinds::SetUpperBound);
		Int32Range.Method("void SetUpperBoundValue(int32 Value)", METHODPR_TRIVIAL(void, FInt32Range, SetUpperBoundValue, (int32)));
		Int32Range.Method("int32 GetUpperBoundValue() const", METHODPR_TRIVIAL(int32, FInt32Range, GetUpperBoundValue, () const));
		Int32Range.Method("bool HasLowerBound() const", METHOD_TRIVIAL(FInt32Range, HasLowerBound));
		Int32Range.Method("bool HasUpperBound() const", METHOD_TRIVIAL(FInt32Range, HasUpperBound));
		Int32Range.Method("bool IsDegenerate() const", METHOD_TRIVIAL(FInt32Range, IsDegenerate));
		Int32Range.Method("bool IsEmpty() const", METHOD_TRIVIAL(FInt32Range, IsEmpty));
		Int32Range.Method("bool Overlaps(const FInt32Range& Other) const", METHOD_TRIVIAL(FInt32Range, Overlaps));
		Int32Range.Method("TArray<FInt32Range> Split(int32 Value) const", METHODPR_TRIVIAL(TArray<FInt32Range>, FInt32Range, Split, (int32) const));

		auto FloatInterval = Binds.ExistingClassForTarget("FFloatInterval");
		FloatInterval.Constructor("void f(float32 Min, float32 Max)", &FFloatIntervalBinds::Construct, "FFloatInterval", true);
		FloatInterval.Method("bool opEquals(const FFloatInterval& Other) const", METHODPR_TRIVIAL(bool, FFloatInterval, operator==, (const FFloatInterval&) const));
		FloatInterval.Method("float32 Size() const", METHOD_TRIVIAL(FFloatInterval, Size));
		FloatInterval.Method("bool IsValid() const", METHOD_TRIVIAL(FFloatInterval, IsValid));
		FloatInterval.Method("bool Contains(float32 Value) const", METHODPR_TRIVIAL(bool, FFloatInterval, Contains, (const float&) const));
		FloatInterval.Method("void Expand(float32 Amount)", METHOD_TRIVIAL(FFloatInterval, Expand));
		FloatInterval.Method("void Include(float32 Value)", METHOD_TRIVIAL(FFloatInterval, Include));
		FloatInterval.Method("float32 Interpolate(float32 Alpha) const", METHOD_TRIVIAL(FFloatInterval, Interpolate));

		auto Int32Interval = Binds.ExistingClassForTarget("FInt32Interval");
		Int32Interval.Constructor("void f(int32 Min, int32 Max)", &FInt32IntervalBinds::Construct, "FInt32Interval", true);
		Int32Interval.Method("bool opEquals(const FInt32Interval& Other) const", METHODPR_TRIVIAL(bool, FInt32Interval, operator==, (const FInt32Interval&) const));
		Int32Interval.Method("int32 Size() const", METHOD_TRIVIAL(FInt32Interval, Size));
		Int32Interval.Method("bool IsValid() const", METHOD_TRIVIAL(FInt32Interval, IsValid));
		Int32Interval.Method("bool Contains(int32 Value) const", METHODPR_TRIVIAL(bool, FInt32Interval, Contains, (const int32&) const));
		Int32Interval.Method("void Expand(int32 Amount)", METHOD_TRIVIAL(FInt32Interval, Expand));
		Int32Interval.Method("void Include(int32 Value)", METHOD_TRIVIAL(FInt32Interval, Include));
		Int32Interval.Method("int32 Interpolate(float32 Alpha) const", METHOD_TRIVIAL(FInt32Interval, Interpolate));

		{
			FAngelscriptBinds::FNamespace FloatBoundNamespace(Binds.GetTargetEngine(), "FFloatRangeBound");
			Binds.BindGlobalFunctionForTarget("FFloatRangeBound Exclusive(float32 Value)", &FFloatRangeBound::Exclusive);
			Binds.BindGlobalFunctionForTarget("FFloatRangeBound Inclusive(float32 Value)", &FFloatRangeBound::Inclusive);
			Binds.BindGlobalFunctionForTarget("FFloatRangeBound Open()", &FFloatRangeBound::Open);
			Binds.BindGlobalFunctionForTarget("FFloatRangeBound FlipInclusion(const FFloatRangeBound& Bound)", &FFloatRangeBound::FlipInclusion);
			Binds.BindGlobalFunctionForTarget("FFloatRangeBound MaxLower(const FFloatRangeBound& A, const FFloatRangeBound& B)", &FFloatRangeBoundBinds::MaxLower);
			Binds.BindGlobalFunctionForTarget("FFloatRangeBound MaxUpper(const FFloatRangeBound& A, const FFloatRangeBound& B)", &FFloatRangeBoundBinds::MaxUpper);
			Binds.BindGlobalFunctionForTarget("FFloatRangeBound MinLower(const FFloatRangeBound& A, const FFloatRangeBound& B)", &FFloatRangeBoundBinds::MinLower);
			Binds.BindGlobalFunctionForTarget("FFloatRangeBound MinUpper(const FFloatRangeBound& A, const FFloatRangeBound& B)", &FFloatRangeBoundBinds::MinUpper);
		}

		{
			FAngelscriptBinds::FNamespace Int32BoundNamespace(Binds.GetTargetEngine(), "FInt32RangeBound");
			Binds.BindGlobalFunctionForTarget("FInt32RangeBound Exclusive(int32 Value)", &FInt32RangeBound::Exclusive);
			Binds.BindGlobalFunctionForTarget("FInt32RangeBound Inclusive(int32 Value)", &FInt32RangeBound::Inclusive);
			Binds.BindGlobalFunctionForTarget("FInt32RangeBound Open()", &FInt32RangeBound::Open);
			Binds.BindGlobalFunctionForTarget("FInt32RangeBound FlipInclusion(const FInt32RangeBound& Bound)", &FInt32RangeBound::FlipInclusion);
			Binds.BindGlobalFunctionForTarget("FInt32RangeBound MaxLower(const FInt32RangeBound& A, const FInt32RangeBound& B)", &FInt32RangeBoundBinds::MaxLower);
			Binds.BindGlobalFunctionForTarget("FInt32RangeBound MaxUpper(const FInt32RangeBound& A, const FInt32RangeBound& B)", &FInt32RangeBoundBinds::MaxUpper);
			Binds.BindGlobalFunctionForTarget("FInt32RangeBound MinLower(const FInt32RangeBound& A, const FInt32RangeBound& B)", &FInt32RangeBoundBinds::MinLower);
			Binds.BindGlobalFunctionForTarget("FInt32RangeBound MinUpper(const FInt32RangeBound& A, const FInt32RangeBound& B)", &FInt32RangeBoundBinds::MinUpper);
		}

		{
			FAngelscriptBinds::FNamespace FloatRangeNamespace(Binds.GetTargetEngine(), "FFloatRange");
			Binds.BindGlobalFunctionForTarget("TArray<FFloatRange> Difference(const FFloatRange& A, const FFloatRange& B)", &FFloatRange::Difference);
		Binds.BindGlobalFunctionForTarget("FFloatRange Hull(const FFloatRange& A, const FFloatRange& B)", &FFloatRangeBinds::DifferenceHull);
		Binds.BindGlobalFunctionForTarget("FFloatRange Hull(const TArray<FFloatRange>& Ranges)", &FFloatRangeBinds::Hull);
		Binds.BindGlobalFunctionForTarget("FFloatRange Intersection(const FFloatRange& A, const FFloatRange& B)", static_cast<FFloatRange(*)(const FFloatRange&, const FFloatRange&)>(&FFloatRangeBinds::Intersection));
		Binds.BindGlobalFunctionForTarget("FFloatRange Intersection(const TArray<FFloatRange>& Ranges)", static_cast<FFloatRange(*)(const TArray<FFloatRange>&)>(&FFloatRangeBinds::Intersection));
		Binds.BindGlobalFunctionForTarget("TArray<FFloatRange> Union(const FFloatRange& A, const FFloatRange& B)", &FFloatRange::Union);
		Binds.BindGlobalFunctionForTarget("FFloatRange All()", &FFloatRange::All);
		Binds.BindGlobalFunctionForTarget("FFloatRange AtLeast(float32 Value)", &FFloatRange::AtLeast);
		Binds.BindGlobalFunctionForTarget("FFloatRange AtMost(float32 Value)", &FFloatRange::AtMost);
		Binds.BindGlobalFunctionForTarget("FFloatRange Empty()", &FFloatRange::Empty);
		Binds.BindGlobalFunctionForTarget("FFloatRange Exclusive(float32 Min, float32 Max)", &FFloatRange::Exclusive);
		Binds.BindGlobalFunctionForTarget("FFloatRange GreaterThan(float32 Value)", &FFloatRangeBinds::GreaterThan);
		Binds.BindGlobalFunctionForTarget("FFloatRange Inclusive(float32 Min, float32 Max)", &FFloatRange::Inclusive);
			Binds.BindGlobalFunctionForTarget("FFloatRange LessThan(float32 Value)", &FFloatRangeBinds::LessThan);
		}

		{
			FAngelscriptBinds::FNamespace Int32RangeNamespace(Binds.GetTargetEngine(), "FInt32Range");
			Binds.BindGlobalFunctionForTarget("TArray<FInt32Range> Difference(const FInt32Range& A, const FInt32Range& B)", &FInt32Range::Difference);
		Binds.BindGlobalFunctionForTarget("FInt32Range Hull(const FInt32Range& A, const FInt32Range& B)", &FInt32RangeBinds::DifferenceHull);
		Binds.BindGlobalFunctionForTarget("FInt32Range Hull(const TArray<FInt32Range>& Ranges)", &FInt32RangeBinds::Hull);
		Binds.BindGlobalFunctionForTarget("FInt32Range Intersection(const FInt32Range& A, const FInt32Range& B)", static_cast<FInt32Range(*)(const FInt32Range&, const FInt32Range&)>(&FInt32RangeBinds::Intersection));
		Binds.BindGlobalFunctionForTarget("FInt32Range Intersection(const TArray<FInt32Range>& Ranges)", static_cast<FInt32Range(*)(const TArray<FInt32Range>&)>(&FInt32RangeBinds::Intersection));
		Binds.BindGlobalFunctionForTarget("TArray<FInt32Range> Union(const FInt32Range& A, const FInt32Range& B)", &FInt32Range::Union);
		Binds.BindGlobalFunctionForTarget("FInt32Range All()", &FInt32Range::All);
		Binds.BindGlobalFunctionForTarget("FInt32Range AtLeast(int32 Value)", &FInt32Range::AtLeast);
		Binds.BindGlobalFunctionForTarget("FInt32Range AtMost(int32 Value)", &FInt32Range::AtMost);
		Binds.BindGlobalFunctionForTarget("FInt32Range Empty()", &FInt32Range::Empty);
		Binds.BindGlobalFunctionForTarget("FInt32Range Exclusive(int32 Min, int32 Max)", &FInt32Range::Exclusive);
		Binds.BindGlobalFunctionForTarget("FInt32Range GreaterThan(int32 Value)", &FInt32RangeBinds::GreaterThan);
		Binds.BindGlobalFunctionForTarget("FInt32Range Inclusive(int32 Min, int32 Max)", &FInt32Range::Inclusive);
			Binds.BindGlobalFunctionForTarget("FInt32Range LessThan(int32 Value)", &FInt32RangeBinds::LessThan);
		}
	});
