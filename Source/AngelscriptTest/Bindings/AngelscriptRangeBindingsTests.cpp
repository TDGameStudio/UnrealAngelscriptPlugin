#include "CQTest.h"
#include "AngelscriptTestExecute.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestModuleScope.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FAngelscriptRangeBindingsTest,
	"Angelscript.TestModule.Bindings.Range",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
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

	TEST_METHOD(FloatRangeAndIntervalValues)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule ModuleScope(*TestRunner, Engine, TEXT("ASRange_FloatValues"), ASTEST_AS(R"AS(
			int FloatRangeAndIntervalValues()
			{
				FFloatRangeBound Lower = FFloatRangeBound::Inclusive(1.0);
				FFloatRangeBound Upper = FFloatRangeBound::Exclusive(5.0);
				FFloatRange Range(Lower, Upper);
				if (!Lower.IsClosed() || Lower.GetValue() != 1.0 || !Upper.IsExclusive()
					|| !Range.Contains(1.0) || Range.Contains(5.0))
				{
					return 10;
				}

				TArray<FFloatRange> Pieces = Range.Split(3.0);
				TArray<FFloatRange> Difference = FFloatRange::Difference(Range, FFloatRange(2.0, 4.0));
				TArray<FFloatRange> Union = FFloatRange::Union(Range, FFloatRange(4.0, 7.0));
				FFloatRange Intersection = FFloatRange::Intersection(Range, FFloatRange(3.0, 7.0));
				if (Pieces.Num() != 2 || Difference.Num() != 2 || Union.Num() != 1
					|| !Intersection.Contains(3.0) || Intersection.Contains(5.0))
				{
					return 20;
				}

				FFloatInterval Interval(2.0, 6.0);
				Interval.Expand(1.0);
				Interval.Include(8.0);
				return Interval.IsValid() && Interval.Contains(1.0) && Interval.Contains(8.0)
					&& Interval.Size() == 7.0 && Interval.Interpolate(0.5) == 4.5 ? 1 : 30;
			}
			)AS"));
		if (!ModuleScope.IsValid())
		{
			return;
		}

		ASSERT_THAT(IsTrue(
			ExpectGlobalInt(*TestRunner, Engine, ModuleScope.GetModule(), TEXT("int FloatRangeAndIntervalValues()"), TEXT("Float range and interval bindings should preserve bounds and range algebra"), 1),
			TEXT("Float range and interval bindings should be callable from AngelScript")));
	}

	TEST_METHOD(Int32RangeAndIntervalValues)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule ModuleScope(*TestRunner, Engine, TEXT("ASRange_Int32Values"), ASTEST_AS(R"AS(
			int Int32RangeAndIntervalValues()
			{
				FInt32Range Range = FInt32Range::Inclusive(2, 6);
				if (!Range.Contains(2) || !Range.Contains(6) || Range.Contains(7)
					|| !Range.Adjoins(FInt32Range::Exclusive(6, 10)))
				{
					return 10;
				}

				FInt32RangeBound Bound = FInt32RangeBound::Open();
				if (!Bound.IsOpen())
				{
					return 20;
				}

				FInt32Interval Interval(3, 7);
				Interval.Expand(2);
				Interval.Include(12);
				return Interval.IsValid() && Interval.Contains(1) && Interval.Contains(12)
					&& Interval.Size() == 11 && Interval.Interpolate(0.5) == 6 ? 1 : 30;
			}
			)AS"));
		if (!ModuleScope.IsValid())
		{
			return;
		}

		ASSERT_THAT(IsTrue(
			ExpectGlobalInt(*TestRunner, Engine, ModuleScope.GetModule(), TEXT("int Int32RangeAndIntervalValues()"), TEXT("Int32 range and interval bindings should preserve discrete range semantics"), 1),
			TEXT("Int32 range and interval bindings should be callable from AngelScript")));
	}
};

#endif
