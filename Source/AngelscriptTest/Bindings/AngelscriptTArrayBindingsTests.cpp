#include "CQTest.h"
#include "AngelscriptTestExecute.h"
#include "AngelscriptTestEngineHelper.h"
#include "AngelscriptTestUtilities.h"
#include "AngelscriptTestMacros.h"
#include "Bindings/AngelscriptTArrayBindingsTestHelpers.h"

#include "Containers/ScriptArray.h"
#include "Camera/CameraActor.h"
#include "GameFramework/Actor.h"
#include "Misc/ScopeExit.h"

#if WITH_ANGELSCRIPT_UNITTESTS


static const FString TArrayBindingsNestedContainerDiagnostic = TEXT("Containers cannot be nested in other containers");
static const FArraySyntaxCoverageProfile TArrayProfile = TArrayBindingsCoverageProfile();


bool RunTArrayMutationCompatSection(
	FAutomationTestBase& Test,
	FAngelscriptEngine& Engine,
	const FArraySyntaxCoverageProfile& Profile)
{
	asIScriptModule* Module = TArrayBindingsBuildCoverageModule(
		Test,
		Engine,
		Profile,
		TEXT("MutationCompat"),
		TEXT(R"(
int Entry()
{
	TArray<int> Values;
	Values.Add(1);
	Values.Add(2);
	Values.Add(2);
	Values.Add(3);
	if (Values.FindIndex(2) != 1)
		return 10;
	if (Values.AddUnique(2))
		return 20;
	if (!Values.AddUnique(4))
		return 30;

	Values.Insert(9, 1);
	if (!(Values.Num() == 6 && Values[1] == 9 && Values.Last() == 4))
		return 40;

	if (Values.RemoveSingle(2) != 1)
		return 50;
	if (Values.Remove(2) != 1)
		return 60;
	if (Values.FindIndex(2) != -1)
		return 70;

	Values.Reset(4);
	if (!Values.IsEmpty() || Values.Max() < 4)
		return 80;

	return 1;
}
)"));
	if (Module == nullptr)
	{
		return false;
	}

	asIScriptFunction* Function = GetFunctionByDecl(Test, *Module, TEXT("int Entry()"));
	if (Function == nullptr)
	{
		return false;
	}

	int32 Result = 0;
	if (!ExecuteIntFunction(Test, Engine, *Function, Result))
	{
		return false;
	}

	FNoDiscardAsserter LocalAssert(Test);
	return LocalAssert.AreEqual(1, Result, TEXT("TArray mutation helpers should match expected script behaviour"));
}

bool RunTArrayIteratorCompatSection(
	FAutomationTestBase& Test,
	FAngelscriptEngine& Engine,
	const FArraySyntaxCoverageProfile& Profile)
{
	asIScriptModule* Module = TArrayBindingsBuildCoverageModule(
		Test,
		Engine,
		Profile,
		TEXT("IteratorCompat"),
		TEXT(R"(
int Entry()
{
	TArray<int> MutableValues;
	MutableValues.Add(1);
	MutableValues.Add(2);
	MutableValues.Add(3);
	TArrayIterator<int> MutableIt = MutableValues.Iterator();

	if (!MutableIt.CanProceed || MutableIt.Proceed() != 1)
		return 10;

	MutableIt.Proceed() = 20;
	if (MutableValues[1] != 20)
		return 20;

	int TailSum = 0;
	while (MutableIt.CanProceed)
	{
		TailSum += MutableIt.Proceed();
	}

	if (TailSum != 3 || MutableIt.CanProceed)
		return 30;

	TArray<int> ConstSource;
	ConstSource.Add(4);
	ConstSource.Add(5);
	ConstSource.Add(6);
	const TArray<int> ConstValues = ConstSource;
	TArrayConstIterator<int> ConstIt = ConstValues.Iterator();
	int ConstSum = 0;
	while (ConstIt.CanProceed)
	{
		ConstSum += ConstIt.Proceed();
	}

	if (ConstSum != 15)
		return 40;

	TArrayConstIterator<int> AliasIt = ConstIt;
	TArrayIterator<int> MutableAliasIt = MutableValues.Iterator();

	int AliasSum = 0;
	while (MutableAliasIt.CanProceed)
	{
		AliasSum += MutableAliasIt.Proceed();
	}

	return AliasSum == 24 ? 1 : 50;
}
)"));
	if (Module == nullptr)
	{
		return false;
	}

	asIScriptFunction* Function = GetFunctionByDecl(Test, *Module, TEXT("int Entry()"));
	if (Function == nullptr)
	{
		return false;
	}

	int32 Result = 0;
	if (!ExecuteIntFunction(Test, Engine, *Function, Result))
	{
		return false;
	}

	FNoDiscardAsserter LocalAssert(Test);
	return LocalAssert.AreEqual(1, Result, TEXT("TArray iterator helpers should match expected script behaviour"));
}

bool RunTArrayOperationsSection(
	FAutomationTestBase& Test,
	FAngelscriptEngine& Engine,
	const FArraySyntaxCoverageProfile& Profile)
{
	bool bPassed = false;

	const FString OperationsScript = FString(TEXT(R"(
void TraceTArrayCase(const FString&in CaseName)
{
	Log(FName("TArrayBindings"), "TArray.Operations: " + CaseName);
}

int IntDefaultIsEmpty()
{
	TArray<int> Values;
	return Values.IsEmpty() && Values.Num() == 0 && Values.Max() == 0 ? 1 : 0;
}

int IntReserveKeepsNum()
{
	TArray<int> Values;
	Values.Add(4);
	Values.Add(5);
	Values.Reserve(16);
	return Values.Num();
}

int IntReserveGrowsMax()
{
	TArray<int> Values;
	Values.Add(4);
	Values.Add(5);
	Values.Reserve(16);
	return Values.Max();
}

int IntGetSlackAfterReserve()
{
	TArray<int> Values;
	Values.Add(4);
	Values.Add(5);
	Values.Reserve(16);
	return Values.GetSlack();
}

int IntGetAllocatedSizeAfterReserve()
{
	TArray<int> Values;
	Values.Reserve(8);
	return Values.GetAllocatedSize() > 0 ? 1 : 0;
}

int IntShrinkReducesMax()
{
	TArray<int> Values;
	Values.Reserve(16);
	Values.Add(1);
	Values.Add(2);
	int MaxBefore = Values.Max();
	Values.Shrink();
	return Values.Max() <= MaxBefore && Values.Max() >= Values.Num() ? 1 : 0;
}

int IntEmptyReservedMax()
{
	TArray<int> Values;
	Values.Add(1);
	Values.Empty(6);
	return Values.Num() == 0 ? Values.Max() : 0;
}

int IntResetKeepsCapacity()
{
	TArray<int> Values;
	Values.Reserve(8);
	Values.Add(1);
	int MaxBefore = Values.Max();
	Values.Reset();
	return Values.Num() == 0 && Values.Max() >= MaxBefore ? 1 : 0;
}

int IntSetNumExtends()
{
	TArray<int> Values;
	Values.Add(9);
	Values.SetNum(4);
	return Values.Num();
}

int IntSetNumPreservesAndZeroes()
{
	TArray<int> Values;
	Values.Add(9);
	Values.SetNum(4);
	return Values[0] == 9 && Values[1] == 0 && Values[2] == 0 && Values[3] == 0 ? 1 : 0;
}

int IntSetNumZeroedPreservesAndZeroes()
{
	TArray<int> Values;
	Values.Add(9);
	Values.SetNumZeroed(4);
	return Values[0] == 9 && Values[1] == 0 && Values[2] == 0 && Values[3] == 0 ? 1 : 0;
}

int IntSetNumDefaultClears()
{
	TArray<int> Values;
	Values.Add(1);
	Values.SetNum();
	return Values.Num();
}

int IntSetNumZeroedDefaultClears()
{
	TArray<int> Values;
	Values.Add(1);
	Values.SetNumZeroed();
	return Values.Num();
}

int IntIndexWriteRead()
{
	TArray<int> Values;
	Values.SetNum(2);
	Values[0] = 19;
	Values[1] = 23;
	return Values[0] + Values[1];
}

int IntConstIndexAndLastRead()
{
	TArray<int> Source;
	Source.Add(13);
	Source.Add(21);
	const TArray<int> Values = Source;
	return Values[0] + Values.Last();
}

int IntLastReadWrite()
{
	TArray<int> Values;
	Values.Add(3);
	Values.Add(5);
	Values.Add(7);
	Values.Last() = 11;
	return Values.Last() + Values.Last(1);
}

int IntIsValidIndex()
{
	TArray<int> Values;
	Values.Add(1);
	return Values.IsValidIndex(0) && !Values.IsValidIndex(1) && !Values.IsValidIndex(-1) ? 1 : 0;
}

int IntAppend()
{
	TArray<int> Values;
	Values.Add(1);
	TArray<int> Other;
	Other.Add(2);
	Other.Add(3);
	Values.Append(Other);
	return Values[0] + Values[1] + Values[2];
}

int IntCopy()
{
	TArray<int> Source;
	Source.Add(4);
	Source.Add(5);
	Source.Add(6);
	TArray<int> Destination;
	Destination.SetNum(5);
	Destination.Copy(Source, 0, 3, 1);
	return Destination[0] == 0
		&& Destination[1] == 4
		&& Destination[2] == 5
		&& Destination[3] == 6
		&& Destination[4] == 0 ? 1 : 0;
}

int IntCopyDefaultTarget()
{
	TArray<int> Source;
	Source.Add(4);
	Source.Add(5);
	TArray<int> Destination;
	Destination.SetNum(2);
	Destination.Copy(Source, 0, 2);
	return Destination[0] == 4 && Destination[1] == 5 ? 1 : 0;
}

int IntAssignEquals()
{
	TArray<int> Source;
	Source.Add(8);
	Source.Add(13);
	TArray<int> Assigned;
	Assigned = Source;
	return Assigned == Source && Assigned.Num() == 2 && Assigned[1] == 13 ? 1 : 0;
}

int IntEqualsFalse()
{
	TArray<int> Left;
	Left.Add(1);
	TArray<int> Right;
	Right.Add(2);
	return Left == Right ? 0 : 1;
}

int IntMoveAssignFrom()
{
	TArray<int> Source;
	Source.Add(21);
	Source.Add(34);
	TArray<int> Destination;
	Destination.Add(1);
	Destination.MoveAssignFrom(Source);
	return Destination.Num() == 2
		&& Destination[0] == 21
		&& Destination[1] == 34
		&& Source.Num() == 0 ? 1 : 0;
}

int IntSwap()
{
	TArray<int> Values;
	Values.Add(1);
	Values.Add(2);
	Values.Add(3);
	Values.Swap(0, 2);
	return Values[0] == 3 && Values[1] == 2 && Values[2] == 1 ? 1 : 0;
}

int IntInsert()
{
	TArray<int> Values;
	Values.Add(1);
	Values.Add(3);
	Values.Insert(2, 1);
	return Values.Num() == 3 && Values[0] == 1 && Values[1] == 2 && Values[2] == 3 ? 1 : 0;
}

int IntInsertDefaultIndex()
{
	TArray<int> Values;
	Values.Add(2);
	Values.Insert(1);
	return Values.Num() == 2 && Values[0] == 1 && Values[1] == 2 ? 1 : 0;
}

int IntAddUnique()
{
	TArray<int> Values;
	bool bAddedFirst = Values.AddUnique(7);
	bool bAddedSecond = Values.AddUnique(7);
	return bAddedFirst && !bAddedSecond && Values.Num() == 1 && Values[0] == 7 ? 1 : 0;
}
)")) +
TEXT(R"(

int IntContains()
{
	TArray<int> Values;
	Values.Add(4);
	Values.Add(9);
	return Values.Contains(9) && !Values.Contains(5) ? 1 : 0;
}

int IntFindIndex()
{
	TArray<int> Values;
	Values.Add(4);
	Values.Add(9);
	Values.Add(12);
	return Values.FindIndex(12);
}

int IntFindIndexMissing()
{
	TArray<int> Values;
	Values.Add(4);
	return Values.FindIndex(12);
}

int IntRemoveSingle()
{
	TArray<int> Values;
	Values.Add(1);
	Values.Add(2);
	Values.Add(1);
	int Removed = Values.RemoveSingle(1);
	return Removed == 1 && Values.Num() == 2 && Values.Contains(1) && Values.Contains(2) ? 1 : 0;
}

int IntRemoveSingleMissing()
{
	TArray<int> Values;
	Values.Add(1);
	return Values.RemoveSingle(9);
}

int IntRemove()
{
	TArray<int> Values;
	Values.Add(1);
	Values.Add(2);
	Values.Add(1);
	int Removed = Values.Remove(1);
	return Removed == 2 && Values.Num() == 1 && Values[0] == 2 ? 1 : 0;
}

int IntRemoveMissing()
{
	TArray<int> Values;
	Values.Add(1);
	return Values.Remove(9);
}

int IntRemoveSingleSwap()
{
	TArray<int> Values;
	Values.Add(1);
	Values.Add(2);
	Values.Add(3);
	int Removed = Values.RemoveSingleSwap(2);
	return Removed == 1 && Values.Num() == 2 && !Values.Contains(2) && Values.Contains(1) && Values.Contains(3) ? 1 : 0;
}

int IntRemoveSingleSwapMissing()
{
	TArray<int> Values;
	Values.Add(1);
	return Values.RemoveSingleSwap(9);
}

int IntRemoveSwap()
{
	TArray<int> Values;
	Values.Add(1);
	Values.Add(2);
	Values.Add(1);
	Values.Add(3);
	int Removed = Values.RemoveSwap(1);
	return Removed == 2 && Values.Num() == 2 && !Values.Contains(1) && Values.Contains(2) && Values.Contains(3) ? 1 : 0;
}

int IntRemoveSwapMissing()
{
	TArray<int> Values;
	Values.Add(1);
	return Values.RemoveSwap(9);
}

int IntRemoveAt()
{
	TArray<int> Values;
	Values.Add(1);
	Values.Add(2);
	Values.Add(3);
	Values.RemoveAt(1);
	return Values.Num() == 2 && Values[0] == 1 && Values[1] == 3 ? 1 : 0;
}

int IntRemoveAtSwap()
{
	TArray<int> Values;
	Values.Add(1);
	Values.Add(2);
	Values.Add(3);
	Values.RemoveAtSwap(0);
	return Values.Num() == 2 && !Values.Contains(1) && Values.Contains(2) && Values.Contains(3) ? 1 : 0;
}

int IntSortAscending()
{
	TArray<int> Values;
	Values.Add(3);
	Values.Add(1);
	Values.Add(2);
	Values.Sort();
	return Values[0] * 100 + Values[1] * 10 + Values[2];
}

int IntSortDescending()
{
	TArray<int> Values;
	Values.Add(3);
	Values.Add(1);
	Values.Add(2);
	Values.Sort(true);
	return Values[0] * 100 + Values[1] * 10 + Values[2];
}

int IntShufflePreservesElements()
{
	TArray<int> Values;
	Values.Add(1);
	Values.Add(2);
	Values.Add(3);
	Values.Add(4);
	Values.Shuffle();
	int Sum = 0;
	int Product = 1;
	foreach (int Value : Values)
	{
		Sum += Value;
		Product *= Value;
	}
	return Values.Num() == 4 && Sum == 10 && Product == 24 ? 1 : 0;
}

int IntForeachValueSum()
{
	TArray<int> Values;
	Values.Add(2);
	Values.Add(4);
	Values.Add(6);
	int Sum = 0;
	foreach (int Value : Values)
	{
		Sum += Value;
	}
	return Sum;
}

int IntForeachValueIndexWeightedSum()
{
	TArray<int> Values;
	Values.Add(2);
	Values.Add(4);
	Values.Add(6);
	int Sum = 0;
	foreach (int Value, int Index : Values)
	{
		Sum += Value * (Index + 1);
	}
	return Sum;
}

int EmptyForeachVisitCount()
{
	TArray<int> Values;
	int Count = 0;
	foreach (int Value : Values)
	{
		Count += 1;
	}
	return Count;
}

int IntEmptyIteratorCannotProceed()
{
	TArray<int> Values;
	TArrayIterator<int> Iterator = Values.Iterator();
	return Iterator.CanProceed ? 0 : 1;
}

int IntIteratorProceedSum()
{
	TArray<int> Values;
	Values.Add(1);
	Values.Add(2);
	Values.Add(3);
	TArrayIterator<int> Iterator = Values.Iterator();
	int Sum = 0;
	while (Iterator.CanProceed)
	{
		Sum += Iterator.Proceed();
	}
	return Sum;
}

int IntMutableIteratorCanWrite()
{
	TArray<int> Values;
	Values.Add(1);
	Values.Add(2);
	TArrayIterator<int> Iterator = Values.Iterator();
	Iterator.Proceed() = 20;
	return Values[0] + Values[1];
}

int IntConstIteratorProceedSum()
{
	TArray<int> Source;
	Source.Add(4);
	Source.Add(5);
	Source.Add(6);
	const TArray<int> Values = Source;
	TArrayConstIterator<int> Iterator = Values.Iterator();
	int Sum = 0;
	while (Iterator.CanProceed)
	{
		Sum += Iterator.Proceed();
	}
	return Sum;
}

int IntIteratorCopyAssignStartsAtSameElement()
{
	TArray<int> Values;
	Values.Add(1);
	Values.Add(2);
	TArrayIterator<int> Original = Values.Iterator();
	TArrayIterator<int> Copied = Original;
	TArrayIterator<int> Assigned = Copied;
	return Original.Proceed() + Copied.Proceed() + Assigned.Proceed();
}
)");
	asIScriptModule* Module = TArrayBindingsBuildCoverageModule(Test, Engine, Profile, TEXT("Operations"), OperationsScript);
	if (Module == nullptr)
	{
		return false;
	}

	const TArray<FTArrayExpectedGlobalInt> ExactCases = {
		{ TEXT("int IntDefaultIsEmpty()"), TEXT("TArray<int> default construction and IsEmpty"), 1 },
		{ TEXT("int IntReserveKeepsNum()"), TEXT("TArray<int>.Reserve should preserve Num"), 2 },
		{ TEXT("int IntGetAllocatedSizeAfterReserve()"), TEXT("TArray<int>.GetAllocatedSize should observe allocation"), 1 },
		{ TEXT("int IntShrinkReducesMax()"), TEXT("TArray<int>.Shrink should keep valid capacity"), 1 },
		{ TEXT("int IntResetKeepsCapacity()"), TEXT("TArray<int>.Reset should clear while keeping capacity"), 1 },
		{ TEXT("int IntSetNumExtends()"), TEXT("TArray<int>.SetNum should extend Num"), 4 },
		{ TEXT("int IntSetNumPreservesAndZeroes()"), TEXT("TArray<int>.SetNum should preserve and zero primitive slots"), 1 },
		{ TEXT("int IntSetNumZeroedPreservesAndZeroes()"), TEXT("TArray<int>.SetNumZeroed should preserve and zero primitive slots"), 1 },
		{ TEXT("int IntSetNumDefaultClears()"), TEXT("TArray<int>.SetNum default argument should clear"), 0 },
		{ TEXT("int IntSetNumZeroedDefaultClears()"), TEXT("TArray<int>.SetNumZeroed default argument should clear"), 0 },
		{ TEXT("int IntIndexWriteRead()"), TEXT("TArray<int>.opIndex should read/write by index"), 42 },
		{ TEXT("int IntConstIndexAndLastRead()"), TEXT("const TArray<int>.opIndex and Last should read values"), 34 },
		{ TEXT("int IntLastReadWrite()"), TEXT("TArray<int>.Last should read and write tail values"), 16 },
		{ TEXT("int IntIsValidIndex()"), TEXT("TArray<int>.IsValidIndex should reject invalid indices"), 1 },
		{ TEXT("int IntAppend()"), TEXT("TArray<int>.Append should copy source elements"), 6 },
		{ TEXT("int IntCopy()"), TEXT("TArray<int>.Copy should copy a source slice into target"), 1 },
		{ TEXT("int IntCopyDefaultTarget()"), TEXT("TArray<int>.Copy default target index should copy at zero"), 1 },
		{ TEXT("int IntAssignEquals()"), TEXT("TArray<int> assignment and equality should round-trip"), 1 },
		{ TEXT("int IntEqualsFalse()"), TEXT("TArray<int>.opEquals should detect different arrays"), 1 },
		{ TEXT("int IntMoveAssignFrom()"), TEXT("TArray<int>.MoveAssignFrom should move values and empty source"), 1 },
		{ TEXT("int IntSwap()"), TEXT("TArray<int>.Swap should exchange elements"), 1 },
		{ TEXT("int IntInsert()"), TEXT("TArray<int>.Insert should place an element at index"), 1 },
		{ TEXT("int IntInsertDefaultIndex()"), TEXT("TArray<int>.Insert default index should insert at zero"), 1 },
		{ TEXT("int IntAddUnique()"), TEXT("TArray<int>.AddUnique should report duplicate suppression"), 1 },
		{ TEXT("int IntContains()"), TEXT("TArray<int>.Contains should detect present and missing values"), 1 },
		{ TEXT("int IntFindIndex()"), TEXT("TArray<int>.FindIndex should return the matching index"), 2 },
		{ TEXT("int IntFindIndexMissing()"), TEXT("TArray<int>.FindIndex should return -1 for missing values"), -1 },
		{ TEXT("int IntRemoveSingle()"), TEXT("TArray<int>.RemoveSingle should remove only the first match"), 1 },
		{ TEXT("int IntRemoveSingleMissing()"), TEXT("TArray<int>.RemoveSingle should return zero for missing values"), 0 },
		{ TEXT("int IntRemove()"), TEXT("TArray<int>.Remove should remove all matches"), 1 },
		{ TEXT("int IntRemoveMissing()"), TEXT("TArray<int>.Remove should return zero for missing values"), 0 },
		{ TEXT("int IntRemoveSingleSwap()"), TEXT("TArray<int>.RemoveSingleSwap should remove one match without preserving order"), 1 },
		{ TEXT("int IntRemoveSingleSwapMissing()"), TEXT("TArray<int>.RemoveSingleSwap should return zero for missing values"), 0 },
		{ TEXT("int IntRemoveSwap()"), TEXT("TArray<int>.RemoveSwap should remove all matches without preserving order"), 1 },
		{ TEXT("int IntRemoveSwapMissing()"), TEXT("TArray<int>.RemoveSwap should return zero for missing values"), 0 },
		{ TEXT("int IntRemoveAt()"), TEXT("TArray<int>.RemoveAt should preserve order"), 1 },
		{ TEXT("int IntRemoveAtSwap()"), TEXT("TArray<int>.RemoveAtSwap should remove by index without preserving order"), 1 },
		{ TEXT("int IntSortAscending()"), TEXT("TArray<int>.Sort ascending should order values"), 123 },
		{ TEXT("int IntSortDescending()"), TEXT("TArray<int>.Sort descending should order values"), 321 },
		{ TEXT("int IntShufflePreservesElements()"), TEXT("TArray<int>.Shuffle should preserve element set"), 1 },
		{ TEXT("int IntForeachValueSum()"), TEXT("TArray<int> foreach value traversal should visit every element"), 12 },
		{ TEXT("int IntForeachValueIndexWeightedSum()"), TEXT("TArray<int> foreach value/index traversal should expose stable indices"), 28 },
		{ TEXT("int EmptyForeachVisitCount()"), TEXT("TArray<int> foreach should skip empty arrays"), 0 },
		{ TEXT("int IntEmptyIteratorCannotProceed()"), TEXT("TArrayIterator<int> on an empty array should not proceed"), 1 },
		{ TEXT("int IntIteratorProceedSum()"), TEXT("TArrayIterator<int>.Proceed should walk mutable arrays"), 6 },
		{ TEXT("int IntMutableIteratorCanWrite()"), TEXT("TArrayIterator<int>.Proceed should return a writable reference"), 22 },
		{ TEXT("int IntConstIteratorProceedSum()"), TEXT("TArrayConstIterator<int>.Proceed should walk const arrays"), 15 },
		{ TEXT("int IntIteratorCopyAssignStartsAtSameElement()"), TEXT("TArrayIterator<int> copy and assignment should preserve iterator state"), 3 },
	};
	const TArray<FTArrayExpectedGlobalIntAtLeast> MinimumCases = {
		{ TEXT("int IntReserveGrowsMax()"), TEXT("TArray<int>.Reserve should grow Max"), 16 },
		{ TEXT("int IntGetSlackAfterReserve()"), TEXT("TArray<int>.GetSlack should expose reserved slack"), 14 },
		{ TEXT("int IntEmptyReservedMax()"), TEXT("TArray<int>.Empty should honor reserved size"), 6 },
	};

	bPassed = ExpectTArrayBindingsGlobalInts(Test, Engine, *Module, Profile, ExactCases);
	bPassed &= ExpectTArrayBindingsGlobalIntsAtLeast(Test, Engine, *Module, Profile, MinimumCases);

	return bPassed;
}

bool RunTArrayTypeMatrixSection(
	FAutomationTestBase& Test,
	FAngelscriptEngine& Engine,
	const FArraySyntaxCoverageProfile& Profile)
{
	bool bPassed = false;

	asIScriptModule* Module = TArrayBindingsBuildCoverageModule(
		Test,
		Engine,
		Profile,
		TEXT("TypeMatrix"),
		TEXT(R"(
void TraceTArrayCase(const FString&in CaseName)
{
	Log(FName("TArrayBindings"), "TArray.TypeMatrix: " + CaseName);
}

int UInt8ArraySum()
{
	TArray<uint8> Values;
	Values.Add(250);
	Values.Add(5);
	return int(Values[0]) + int(Values[1]);
}

int Int8ArrayKeepsSignedBytes()
{
	TArray<int8> Values;
	Values.Add(-8);
	Values.Add(7);
	return int(Values[0]) + int(Values[1]);
}

int UInt16ArraySum()
{
	TArray<uint16> Values;
	Values.Add(60000);
	Values.Add(123);
	return int(Values.Num() == 2 && Values[0] == 60000 && Values[1] == 123 ? 1 : 0);
}

int Int16ArrayKeepsSignedValues()
{
	TArray<int16> Values;
	Values.Add(-1234);
	Values.Add(4321);
	return Values[0] + Values[1];
}

int UInt32ArrayKeepsWideValues()
{
	TArray<uint32> Values;
	Values.Add(4000000000);
	Values.Add(42);
	return Values.Num() == 2 && Values[0] == 4000000000 && Values[1] == 42 ? 1 : 0;
}

int Int64ArrayKeepsWideValues()
{
	TArray<int64> Values;
	Values.Add(5000000000);
	Values.Add(-9);
	return Values.Num() == 2 && Values[0] == 5000000000 && Values[1] == -9 ? 1 : 0;
}

int UInt64ArrayKeepsWideValues()
{
	TArray<uint64> Values;
	Values.Add(9000000000);
	Values.Add(11);
	return Values.Num() == 2 && Values[0] == 9000000000 && Values[1] == 11 ? 1 : 0;
}

int FloatArrayScaledSum()
{
	TArray<float> Values;
	Values.Add(1.25f);
	Values.Add(2.50f);
	return int((Values[0] + Values[1]) * 100.0f);
}

int DoubleArrayScaledSum()
{
	TArray<double> Values;
	Values.Add(1.25);
	Values.Add(2.50);
	return int((Values[0] + Values[1]) * 100.0);
}

int BoolArrayTrueCount()
{
	TArray<bool> Values;
	Values.Add(true);
	Values.Add(false);
	Values.Add(true);
	int Count = 0;
	foreach (bool Value : Values)
	{
		if (Value)
			Count += 1;
	}
	return Count;
}

int FStringArrayOperations()
{
	TArray<FString> Values;
	Values.Add("Al");
	Values.Add("pha");
	FString Combined = Values[0] + Values[1];
	return Combined == "Alpha" && Values.Contains("pha") && Values.FindIndex("Al") == 0 ? 1 : 0;
}

int FNameArrayOperations()
{
	TArray<FName> Values;
	Values.Add(FName("Alpha"));
	Values.Add(FName("Beta"));
	return Values.Contains(FName("Beta")) && Values.FindIndex(FName("Alpha")) == 0 ? 1 : 0;
}

int FVectorArrayOperations()
{
	TArray<FVector> Values;
	Values.Add(FVector(1.0, 2.0, 3.0));
	Values.Add(FVector(4.0, 5.0, 6.0));
	double Sum = 0.0;
	foreach (const FVector& Value : Values)
	{
		Sum += Value.X + Value.Y + Value.Z;
	}
	return int(Sum);
}

int UObjectArrayZeroedAndForeach()
{
	TArray<UObject> Objects;
	Objects.SetNumZeroed(2);
	int NullCount = 0;
	foreach (UObject ObjectValue : Objects)
	{
		if (ObjectValue == nullptr)
			NullCount += 1;
	}
	return Objects.Num() == 2 && NullCount == 2 ? 1 : 0;
}

int FVector2DArrayOperations()
{
	TArray<FVector2D> Values;
	Values.Add(FVector2D(2.0, 3.0));
	Values.Add(FVector2D(5.0, 7.0));
	double Sum = 0.0;
	foreach (const FVector2D& Value : Values)
	{
		Sum += Value.X + Value.Y;
	}
	return int(Sum);
}

int FVector4ArrayOperations()
{
	TArray<FVector4> Values;
	Values.Add(FVector4(1.0, 2.0, 3.0, 4.0));
	Values.Add(FVector4(5.0, 6.0, 7.0, 8.0));
	double Sum = 0.0;
	foreach (const FVector4& Value : Values)
	{
		Sum += Value.X + Value.Y + Value.Z + Value.W;
	}
	return int(Sum);
}

int FRotatorArrayOperations()
{
	TArray<FRotator> Values;
	Values.Add(FRotator(10.0, 20.0, 30.0));
	Values.Add(FRotator(1.0, 2.0, 3.0));
	return int(Values[0].Pitch + Values[0].Yaw + Values[0].Roll + Values[1].Pitch + Values[1].Yaw + Values[1].Roll);
}

int FTransformArrayOperations()
{
	TArray<FTransform> Values;
	Values.Add(FTransform(FVector(10.0, 20.0, 30.0)));
	Values.Add(FTransform(FVector(1.0, 2.0, 3.0)));
	FVector Translation = Values[0].GetTranslation() + Values[1].GetTranslation();
	return int(Translation.X + Translation.Y + Translation.Z);
}

int FLinearColorArrayOperations()
{
	TArray<FLinearColor> Values;
	Values.Add(FLinearColor(0.25f, 0.50f, 0.75f, 1.0f));
	Values.Add(FLinearColor(1.0f, 0.75f, 0.50f, 0.25f));
	return int((Values[0].R + Values[0].G + Values[0].B + Values[0].A + Values[1].R + Values[1].G + Values[1].B + Values[1].A) * 100.0f);
}

int FColorArrayOperations()
{
	TArray<FColor> Values;
	Values.Add(FColor(1, 2, 3, 4));
	Values.Add(FColor(5, 6, 7, 8));
	return Values.Num() == 2 && Values[0] == FColor(1, 2, 3, 4) && Values[1].ToHex() == "05060708" ? 1 : 0;
}

int FIntPointArrayOperations()
{
	TArray<FIntPoint> Values;
	Values.Add(FIntPoint(3, 5));
	Values.Add(FIntPoint(7, 11));
	return Values[0].X + Values[0].Y + Values[1].X + Values[1].Y;
}

int FIntVectorArrayOperations()
{
	TArray<FIntVector> Values;
	Values.Add(FIntVector(1, 2, 3));
	Values.Add(FIntVector(4, 5, 6));
	return Values[0].X + Values[0].Y + Values[0].Z + Values[1].X + Values[1].Y + Values[1].Z;
}

int FIntVector4ArrayOperations()
{
	TArray<FIntVector4> Values;
	Values.Add(FIntVector4(1, 2, 3, 4));
	Values.Add(FIntVector4(5, 6, 7, 8));
	return Values[0].X + Values[0].Y + Values[0].Z + Values[0].W + Values[1].X + Values[1].Y + Values[1].Z + Values[1].W;
}

int FGuidArrayOperations()
{
	TArray<FGuid> Values;
	Values.Add(FGuid(1, 2, 3, 4));
	Values.Add(FGuid(5, 6, 7, 8));
	uint32 Sum = 0;
	foreach (const FGuid& Value : Values)
	{
		Sum += Value[0] + Value[1] + Value[2] + Value[3];
	}
	return int(Sum);
}

int FTextArrayOperations()
{
	Log(FName("TArrayBindings"), "TArray.TypeMatrix.FTextArrayOperations: begin");
	TArray<FText> Values;
	Values.SetNum(2);
	Log(FName("TArrayBindings"), "TArray.TypeMatrix.FTextArrayOperations: after SetNum Num=" + Values.Num());
	int EmptyCount = 0;
	foreach (const FText& Value : Values)
	{
		Log(FName("TArrayBindings"), "TArray.TypeMatrix.FTextArrayOperations: foreach IsEmpty=" + (Value.IsEmpty() ? "true" : "false"));
		if (Value.IsEmpty())
			EmptyCount += 1;
	}
	TArray<FText> Copy;
	Copy = Values;
	Log(FName("TArrayBindings"), "TArray.TypeMatrix.FTextArrayOperations: copy Num=" + Copy.Num() + ", EmptyCount=" + EmptyCount);
	return Copy.Num() == 2 && EmptyCount == 2 ? 1 : 0;
}
)"));
	if (Module == nullptr)
	{
		return false;
	}

	const TArray<FTArrayExpectedGlobalInt> Cases = {
		{ TEXT("int UInt8ArraySum()"), TEXT("TArray<uint8> should store byte values"), 255 },
		{ TEXT("int Int8ArrayKeepsSignedBytes()"), TEXT("TArray<int8> should preserve signed byte values"), -1 },
		{ TEXT("int UInt16ArraySum()"), TEXT("TArray<uint16> should preserve unsigned 16-bit values"), 1 },
		{ TEXT("int Int16ArrayKeepsSignedValues()"), TEXT("TArray<int16> should preserve signed 16-bit values"), 3087 },
		{ TEXT("int UInt32ArrayKeepsWideValues()"), TEXT("TArray<uint32> should preserve unsigned 32-bit values"), 1 },
		{ TEXT("int Int64ArrayKeepsWideValues()"), TEXT("TArray<int64> should preserve wide integer values"), 1 },
		{ TEXT("int UInt64ArrayKeepsWideValues()"), TEXT("TArray<uint64> should preserve unsigned 64-bit values"), 1 },
		{ TEXT("int FloatArrayScaledSum()"), TEXT("TArray<float> should preserve numeric values"), 375 },
		{ TEXT("int DoubleArrayScaledSum()"), TEXT("TArray<double> should preserve numeric values"), 375 },
		{ TEXT("int BoolArrayTrueCount()"), TEXT("TArray<bool> should preserve bool values through foreach"), 2 },
		{ TEXT("int FStringArrayOperations()"), TEXT("TArray<FString> should support add/index/search"), 1 },
		{ TEXT("int FNameArrayOperations()"), TEXT("TArray<FName> should support add/index/search"), 1 },
		{ TEXT("int FVectorArrayOperations()"), TEXT("TArray<FVector> should support struct values and const foreach"), 21 },
		{ TEXT("int UObjectArrayZeroedAndForeach()"), TEXT("TArray<UObject> should support object pointer slots and foreach"), 1 },
		{ TEXT("int FVector2DArrayOperations()"), TEXT("TArray<FVector2D> should support struct values and const foreach"), 17 },
		{ TEXT("int FVector4ArrayOperations()"), TEXT("TArray<FVector4> should support 4D struct values and const foreach"), 36 },
		{ TEXT("int FRotatorArrayOperations()"), TEXT("TArray<FRotator> should support rotator struct values"), 66 },
		{ TEXT("int FTransformArrayOperations()"), TEXT("TArray<FTransform> should support transform struct values"), 66 },
		{ TEXT("int FLinearColorArrayOperations()"), TEXT("TArray<FLinearColor> should support linear color struct values"), 500 },
		{ TEXT("int FColorArrayOperations()"), TEXT("TArray<FColor> should support color struct values"), 1 },
		{ TEXT("int FIntPointArrayOperations()"), TEXT("TArray<FIntPoint> should support int point struct values"), 26 },
		{ TEXT("int FIntVectorArrayOperations()"), TEXT("TArray<FIntVector> should support int vector struct values"), 21 },
		{ TEXT("int FIntVector4ArrayOperations()"), TEXT("TArray<FIntVector4> should support int vector4 struct values"), 36 },
		{ TEXT("int FGuidArrayOperations()"), TEXT("TArray<FGuid> should support guid struct values and foreach"), 36 },
		{ TEXT("int FTextArrayOperations()"), TEXT("TArray<FText> should default construct and copy text values"), 1 },
	};

	bPassed = ExpectTArrayBindingsGlobalInts(Test, Engine, *Module, Profile, Cases);

	return bPassed;
}

bool RunTArrayObjectTypesSection(
	FAutomationTestBase& Test,
	FAngelscriptEngine& Engine,
	const FArraySyntaxCoverageProfile& Profile)
{
	bool bPassed = false;

	asIScriptModule* Module = TArrayBindingsBuildCoverageModule(
		Test,
		Engine,
		Profile,
		TEXT("ObjectTypes"),
		TEXT(R"(
void TraceTArrayCase(const FString&in CaseName)
{
	Log(FName("TArrayBindings"), "TArray.ObjectTypes: " + CaseName);
}

int UObjectNullSlotsAndForeach()
{
	TArray<UObject> Objects;
	Objects.SetNumZeroed(2);
	int NullCount = 0;
	foreach (UObject ObjectValue : Objects)
	{
		if (ObjectValue == nullptr)
			NullCount += 1;
	}
	return Objects.Num() == 2 && NullCount == 2 ? 1 : 0;
}

int UObjectDefaultObjectArray()
{
	TSubclassOf<AActor> ActorClass = AActor::StaticClass();
	UObject DefaultObject = ActorClass.GetDefaultObject();
	TArray<UObject> Objects;
	Objects.Add(DefaultObject);
	Objects.Add(nullptr);
	return Objects.Num() == 2
		&& Objects[0] == DefaultObject
		&& Objects[1] == nullptr
		&& Objects.Contains(DefaultObject)
		&& Objects.FindIndex(DefaultObject) == 0 ? 1 : 0;
}

int UObjectAppendCopyAndMove()
{
	TSubclassOf<AActor> ActorClass = AActor::StaticClass();
	UObject DefaultObject = ActorClass.GetDefaultObject();
	TArray<UObject> Source;
	Source.Add(DefaultObject);
	TArray<UObject> Appended;
	Appended.Append(Source);
	TArray<UObject> Copied;
	Copied.SetNumZeroed(1);
	Copied.Copy(Source, 0, 1);
	TArray<UObject> Moved;
	Moved.MoveAssignFrom(Appended);
	return Moved.Num() == 1
		&& Moved[0] == DefaultObject
		&& Appended.Num() == 0
		&& Copied[0] == DefaultObject ? 1 : 0;
}

int UClassArrayStaticClasses()
{
	TArray<UClass> Classes;
	Classes.Add(UObject::StaticClass());
	Classes.Add(AActor::StaticClass());
	Classes.Add(UActorComponent::StaticClass());
	int ObjectChildCount = 0;
	foreach (UClass ClassValue : Classes)
	{
		if (ClassValue != nullptr && ClassValue.IsChildOf(UObject::StaticClass()))
			ObjectChildCount += 1;
	}
	return Classes.Num() == 3
		&& Classes.Contains(AActor::StaticClass())
		&& Classes.FindIndex(UActorComponent::StaticClass()) == 2
		&& ObjectChildCount == 3 ? 1 : 0;
}

int AActorArrayDefaultObjectOperations()
{
	TSubclassOf<AActor> ActorClass = AActor::StaticClass();
	AActor DefaultActor = ActorClass.GetDefaultObject();
	TArray<AActor> Actors;
	Actors.Add(DefaultActor);
	bool bAddedDuplicate = Actors.AddUnique(DefaultActor);
	Actors.Add(nullptr);
	int NonNullCount = 0;
	foreach (AActor ActorValue, int Index : Actors)
	{
		if (Index == 0 && ActorValue == DefaultActor)
			NonNullCount += 1;
		if (Index == 1 && ActorValue == nullptr)
			NonNullCount += 10;
	}
	return Actors.Num() == 2
		&& !bAddedDuplicate
		&& Actors.Contains(DefaultActor)
		&& Actors.FindIndex(DefaultActor) == 0
		&& NonNullCount == 11 ? 1 : 0;
}

int AActorArrayRemoveNullAndSwap()
{
	TSubclassOf<AActor> ActorClass = AActor::StaticClass();
	AActor DefaultActor = ActorClass.GetDefaultObject();
	TArray<AActor> Actors;
	Actors.Add(nullptr);
	Actors.Add(DefaultActor);
	int RemovedNull = Actors.RemoveSingle(nullptr);
	Actors.Swap(0, 0);
	return RemovedNull == 1 && Actors.Num() == 1 && Actors[0] == DefaultActor ? 1 : 0;
}

int UActorComponentArrayDefaultObjectOperations()
{
	TSubclassOf<UActorComponent> ComponentClass = UActorComponent::StaticClass();
	UActorComponent DefaultComponent = ComponentClass.GetDefaultObject();
	TArray<UActorComponent> Components;
	Components.Add(DefaultComponent);
	Components.Add(nullptr);
	Components.RemoveAtSwap(1);
	return Components.Num() == 1
		&& Components[0] == DefaultComponent
		&& Components.Contains(DefaultComponent) ? 1 : 0;
}

int USceneComponentArrayAppendAndRemove()
{
	TSubclassOf<USceneComponent> ComponentClass = USceneComponent::StaticClass();
	USceneComponent DefaultComponent = ComponentClass.GetDefaultObject();
	TArray<USceneComponent> Source;
	Source.Add(DefaultComponent);
	TArray<USceneComponent> Components;
	Components.Append(Source);
	Components.Add(nullptr);
	int RemovedNull = Components.Remove(nullptr);
	return Components.Num() == 1 && RemovedNull == 1 && Components[0] == DefaultComponent ? 1 : 0;
}

int TSubclassOfActorArrayOperations()
{
	TArray<TSubclassOf<AActor>> Classes;
	Classes.Add(AActor::StaticClass());
	Classes.Add(ACameraActor::StaticClass());
	bool bAddedDuplicate = Classes.AddUnique(AActor::StaticClass());
	TArray<TSubclassOf<AActor>> Copy;
	Copy = Classes;
	return Copy.Num() == 2
		&& !bAddedDuplicate
		&& Copy[0].IsChildOf(AActor::StaticClass())
		&& Copy[1].Get() == ACameraActor::StaticClass()
		&& Copy.Contains(AActor::StaticClass())
		&& Copy.FindIndex(ACameraActor::StaticClass()) == 1 ? 1 : 0;
}
)"));
	if (Module == nullptr)
	{
		return false;
	}

	const TArray<FTArrayExpectedGlobalInt> Cases = {
		{ TEXT("int UObjectNullSlotsAndForeach()"), TEXT("TArray<UObject> should support zeroed null slots and foreach"), 1 },
		{ TEXT("int UObjectDefaultObjectArray()"), TEXT("TArray<UObject> should store UObject references and search them"), 1 },
		{ TEXT("int UObjectAppendCopyAndMove()"), TEXT("TArray<UObject> should append, copy, and move object references"), 1 },
		{ TEXT("int UClassArrayStaticClasses()"), TEXT("TArray<UClass> should store and iterate class references"), 1 },
		{ TEXT("int AActorArrayDefaultObjectOperations()"), TEXT("TArray<AActor> should store actor references and foreach with keys"), 1 },
		{ TEXT("int AActorArrayRemoveNullAndSwap()"), TEXT("TArray<AActor> should remove null actor references and swap"), 1 },
		{ TEXT("int UActorComponentArrayDefaultObjectOperations()"), TEXT("TArray<UActorComponent> should store component references"), 1 },
		{ TEXT("int USceneComponentArrayAppendAndRemove()"), TEXT("TArray<USceneComponent> should append and remove component references"), 1 },
		{ TEXT("int TSubclassOfActorArrayOperations()"), TEXT("TArray<TSubclassOf<AActor>> should store actor class references"), 1 },
	};

	bPassed = ExpectTArrayBindingsGlobalInts(Test, Engine, *Module, Profile, Cases);

	return bPassed;
}

bool RunTArrayReturnValuesSection(
	FAutomationTestBase& Test,
	FAngelscriptEngine& Engine,
	const FArraySyntaxCoverageProfile& Profile)
{
	bool bPassed = false;

	asIScriptModule* Module = TArrayBindingsBuildCoverageModule(
		Test,
		Engine,
		Profile,
		TEXT("ReturnValues"),
		TEXT(R"(
void TraceTArrayCase(const FString&in CaseName)
{
	Log(FName("TArrayBindings"), "TArray.ReturnValues: " + CaseName);
}

TArray<int> ReturnIntArray()
{
	Log(FName("TArrayBindings"), "TArray.ReturnValues.ReturnIntArray: begin");
	TArray<int> Values;
	Values.Add(3);
	Values.Add(5);
	Values.Add(8);
	Log(FName("TArrayBindings"), "TArray.ReturnValues.ReturnIntArray: returning");
	return Values;
}

TArray<FVector> ReturnVectorArray()
{
	Log(FName("TArrayBindings"), "TArray.ReturnValues.ReturnVectorArray: begin");
	TArray<FVector> Values;
	Values.Add(FVector(1.0, 2.0, 3.0));
	Values.Add(FVector(4.0, 5.0, 6.0));
	Log(FName("TArrayBindings"), "TArray.ReturnValues.ReturnVectorArray: returning");
	return Values;
}

TArray<UObject> ReturnObjectArray()
{
	Log(FName("TArrayBindings"), "TArray.ReturnValues.ReturnObjectArray: begin");
	TSubclassOf<AActor> ActorClass = AActor::StaticClass();
	UObject DefaultObject = ActorClass.GetDefaultObject();
	TArray<UObject> Objects;
	Objects.Add(DefaultObject);
	Objects.Add(nullptr);
	Log(FName("TArrayBindings"), "TArray.ReturnValues.ReturnObjectArray: returning");
	return Objects;
}

TArray<AActor> ReturnActorArray()
{
	Log(FName("TArrayBindings"), "TArray.ReturnValues.ReturnActorArray: begin");
	TSubclassOf<AActor> ActorClass = AActor::StaticClass();
	AActor DefaultActor = ActorClass.GetDefaultObject();
	TArray<AActor> Actors;
	Actors.Add(DefaultActor);
	Actors.Add(nullptr);
	Log(FName("TArrayBindings"), "TArray.ReturnValues.ReturnActorArray: returning");
	return Actors;
}

TArray<TSubclassOf<AActor>> ReturnActorClassArray()
{
	Log(FName("TArrayBindings"), "TArray.ReturnValues.ReturnActorClassArray: begin");
	TArray<TSubclassOf<AActor>> Classes;
	Classes.Add(AActor::StaticClass());
	Classes.Add(ACameraActor::StaticClass());
	Log(FName("TArrayBindings"), "TArray.ReturnValues.ReturnActorClassArray: returning");
	return Classes;
}
)"));
	if (Module == nullptr)
	{
		return false;
	}

	bPassed = true;
	bPassed &= TArrayBindingsExecuteFunctionReturningScriptArray(
		Test,
		Engine,
		*Module,
		Profile,
		TArrayBindingsMakeArrayFunctionDecl(Profile, TEXT("int"), TEXT("ReturnIntArray")),
		TEXT("TArray<int> return value should expose returned primitive elements"),
		[&Test](const FScriptArray& ReturnedArray)
		{
			FNoDiscardAsserter LocalAssert(Test);
			const int32* Values = static_cast<const int32*>(ReturnedArray.GetData());
			return LocalAssert.AreEqual(3, ReturnedArray.Num(), TEXT("Returned TArray<int> should have Num=3"))
				&& LocalAssert.IsNotNull(Values, TEXT("Returned TArray<int> should expose data"))
				&& LocalAssert.AreEqual(3, Values[0], TEXT("Returned TArray<int>[0]"))
				&& LocalAssert.AreEqual(5, Values[1], TEXT("Returned TArray<int>[1]"))
				&& LocalAssert.AreEqual(8, Values[2], TEXT("Returned TArray<int>[2]"));
		});
	bPassed &= TArrayBindingsExecuteFunctionReturningScriptArray(
		Test,
		Engine,
		*Module,
		Profile,
		TArrayBindingsMakeArrayFunctionDecl(Profile, TEXT("FVector"), TEXT("ReturnVectorArray")),
		TEXT("TArray<FVector> return value should expose returned struct elements"),
		[&Test](const FScriptArray& ReturnedArray)
		{
			FNoDiscardAsserter LocalAssert(Test);
			const FVector* Values = static_cast<const FVector*>(ReturnedArray.GetData());
			return LocalAssert.AreEqual(2, ReturnedArray.Num(), TEXT("Returned TArray<FVector> should have Num=2"))
				&& LocalAssert.IsNotNull(Values, TEXT("Returned TArray<FVector> should expose data"))
				&& LocalAssert.AreEqual(FVector(1.0, 2.0, 3.0), Values[0], TEXT("Returned TArray<FVector>[0]"))
				&& LocalAssert.AreEqual(FVector(4.0, 5.0, 6.0), Values[1], TEXT("Returned TArray<FVector>[1]"));
		});
	bPassed &= TArrayBindingsExecuteFunctionReturningScriptArray(
		Test,
		Engine,
		*Module,
		Profile,
		TArrayBindingsMakeArrayFunctionDecl(Profile, TEXT("UObject"), TEXT("ReturnObjectArray")),
		TEXT("TArray<UObject> return value should expose returned object references"),
		[&Test](const FScriptArray& ReturnedArray)
		{
			FNoDiscardAsserter LocalAssert(Test);
			UObject* const* Objects = static_cast<UObject* const*>(ReturnedArray.GetData());
			UObject* ExpectedDefaultObject = AActor::StaticClass()->GetDefaultObject();
			return LocalAssert.AreEqual(2, ReturnedArray.Num(), TEXT("Returned TArray<UObject> should have Num=2"))
				&& LocalAssert.IsNotNull(Objects, TEXT("Returned TArray<UObject> should expose data"))
				&& LocalAssert.AreEqual(ExpectedDefaultObject, Objects[0], TEXT("Returned TArray<UObject>[0]"))
				&& LocalAssert.IsNull(Objects[1], TEXT("Returned TArray<UObject>[1] should be null"));
		});
	bPassed &= TArrayBindingsExecuteFunctionReturningScriptArray(
		Test,
		Engine,
		*Module,
		Profile,
		TArrayBindingsMakeArrayFunctionDecl(Profile, TEXT("AActor"), TEXT("ReturnActorArray")),
		TEXT("TArray<AActor> return value should expose returned actor references"),
		[&Test](const FScriptArray& ReturnedArray)
		{
			FNoDiscardAsserter LocalAssert(Test);
			AActor* const* Actors = static_cast<AActor* const*>(ReturnedArray.GetData());
			AActor* ExpectedDefaultActor = AActor::StaticClass()->GetDefaultObject<AActor>();
			return LocalAssert.AreEqual(2, ReturnedArray.Num(), TEXT("Returned TArray<AActor> should have Num=2"))
				&& LocalAssert.IsNotNull(Actors, TEXT("Returned TArray<AActor> should expose data"))
				&& LocalAssert.AreEqual(ExpectedDefaultActor, Actors[0], TEXT("Returned TArray<AActor>[0]"))
				&& LocalAssert.IsNull(Actors[1], TEXT("Returned TArray<AActor>[1] should be null"));
		});
	bPassed &= TArrayBindingsExecuteFunctionReturningScriptArray(
		Test,
		Engine,
		*Module,
		Profile,
		TArrayBindingsMakeArrayFunctionDecl(Profile, TEXT("TSubclassOf<AActor>"), TEXT("ReturnActorClassArray")),
		TEXT("TArray<TSubclassOf<AActor>> return value should expose returned class wrappers"),
		[&Test](const FScriptArray& ReturnedArray)
		{
			FNoDiscardAsserter LocalAssert(Test);
			const TSubclassOf<AActor>* Classes = static_cast<const TSubclassOf<AActor>*>(ReturnedArray.GetData());
			return LocalAssert.AreEqual(2, ReturnedArray.Num(), TEXT("Returned TArray<TSubclassOf<AActor>> should have Num=2"))
				&& LocalAssert.IsNotNull(Classes, TEXT("Returned TArray<TSubclassOf<AActor>> should expose data"))
				&& LocalAssert.AreEqual(AActor::StaticClass(), Classes[0].Get(), TEXT("Returned TArray<TSubclassOf<AActor>>[0]"))
				&& LocalAssert.AreEqual(ACameraActor::StaticClass(), Classes[1].Get(), TEXT("Returned TArray<TSubclassOf<AActor>>[1]"));
		});

	return bPassed;
}

bool RunTArrayErrorPathsSection(
	FAutomationTestBase& Test,
	FAngelscriptEngine& Engine,
	const FArraySyntaxCoverageProfile& Profile)
{
	bool bPassed = false;

	asIScriptModule* Module = TArrayBindingsBuildCoverageModule(
		Test,
		Engine,
		Profile,
		TEXT("ErrorPaths"),
		TEXT(R"(
void TraceTArrayCase(const FString&in CaseName)
{
	Log(FName("TArrayBindings"), "TArray.ErrorPaths: " + CaseName);
}

void TriggerIndexOutOfBounds()
{
	Log(FName("TArrayBindings"), "TArray.ErrorPaths.TriggerIndexOutOfBounds: before opIndex");
	TArray<int> Values;
	Values.Add(1);
	Values[1] = 2;
}

void TriggerLastOutOfBounds()
{
	Log(FName("TArrayBindings"), "TArray.ErrorPaths.TriggerLastOutOfBounds: before Last");
	TArray<int> Values;
	int Value = Values.Last();
}

void TriggerSwapOutOfBounds()
{
	Log(FName("TArrayBindings"), "TArray.ErrorPaths.TriggerSwapOutOfBounds: before Swap");
	TArray<int> Values;
	Values.Add(1);
	Values.Swap(0, 1);
}

void TriggerInsertOutOfBounds()
{
	Log(FName("TArrayBindings"), "TArray.ErrorPaths.TriggerInsertOutOfBounds: before Insert");
	TArray<int> Values;
	Values.Insert(1, 1);
}

void TriggerRemoveAtOutOfBounds()
{
	Log(FName("TArrayBindings"), "TArray.ErrorPaths.TriggerRemoveAtOutOfBounds: before RemoveAt");
	TArray<int> Values;
	Values.RemoveAt(0);
}

void TriggerRemoveAtSwapOutOfBounds()
{
	Log(FName("TArrayBindings"), "TArray.ErrorPaths.TriggerRemoveAtSwapOutOfBounds: before RemoveAtSwap");
	TArray<int> Values;
	Values.RemoveAtSwap(0);
}

void TriggerAddSelfAlias()
{
	Log(FName("TArrayBindings"), "TArray.ErrorPaths.TriggerAddSelfAlias: before Add");
	TArray<FString> Values;
	Values.Add("Alpha");
	Values.Add(Values[0]);
}

void TriggerInsertSelfAlias()
{
	Log(FName("TArrayBindings"), "TArray.ErrorPaths.TriggerInsertSelfAlias: before Insert");
	TArray<FString> Values;
	Values.Add("Alpha");
	Values.Insert(Values[0], 0);
}

void TriggerCopySelf()
{
	Log(FName("TArrayBindings"), "TArray.ErrorPaths.TriggerCopySelf: before Copy");
	TArray<int> Values;
	Values.Add(1);
	Values.Copy(Values, 0, 1, 0);
}

void TriggerCopyNegativeCount()
{
	Log(FName("TArrayBindings"), "TArray.ErrorPaths.TriggerCopyNegativeCount: before Copy");
	TArray<int> Source;
	Source.Add(1);
	TArray<int> Destination;
	Destination.Add(0);
	Destination.Copy(Source, 0, -1, 0);
}

void TriggerCopySourceOutOfBounds()
{
	Log(FName("TArrayBindings"), "TArray.ErrorPaths.TriggerCopySourceOutOfBounds: before Copy");
	TArray<int> Source;
	Source.Add(1);
	TArray<int> Destination;
	Destination.Add(0);
	Destination.Copy(Source, 1, 1, 0);
}

void TriggerCopyTargetOutOfBounds()
{
	Log(FName("TArrayBindings"), "TArray.ErrorPaths.TriggerCopyTargetOutOfBounds: before Copy");
	TArray<int> Source;
	Source.Add(1);
	TArray<int> Destination;
	Destination.Copy(Source, 0, 1, 0);
}

void TriggerMoveAssignSelf()
{
	Log(FName("TArrayBindings"), "TArray.ErrorPaths.TriggerMoveAssignSelf: before MoveAssignFrom");
	TArray<int> Values;
	Values.Add(1);
	Values.MoveAssignFrom(Values);
}

void TriggerSetNumNegative()
{
	Log(FName("TArrayBindings"), "TArray.ErrorPaths.TriggerSetNumNegative: before SetNum");
	TArray<int> Values;
	Values.SetNum(-1);
}

void TriggerSetNumZeroedNegative()
{
	Log(FName("TArrayBindings"), "TArray.ErrorPaths.TriggerSetNumZeroedNegative: before SetNumZeroed");
	TArray<int> Values;
	Values.SetNumZeroed(-1);
}

void TriggerSetNumZeroedString()
{
	Log(FName("TArrayBindings"), "TArray.ErrorPaths.TriggerSetNumZeroedString: before SetNumZeroed");
	TArray<FString> Values;
	Values.SetNumZeroed(1);
}

void TriggerIteratorProceedOutOfBounds()
{
	Log(FName("TArrayBindings"), "TArray.ErrorPaths.TriggerIteratorProceedOutOfBounds: before Proceed");
	TArray<int> Values;
	TArrayIterator<int> Iterator = Values.Iterator();
	Iterator.Proceed();
}

void TriggerConstIteratorProceedOutOfBounds()
{
	Log(FName("TArrayBindings"), "TArray.ErrorPaths.TriggerConstIteratorProceedOutOfBounds: before Proceed");
	TArray<int> Source;
	const TArray<int> Values = Source;
	TArrayConstIterator<int> Iterator = Values.Iterator();
	Iterator.Proceed();
}

void TriggerSortUnsupportedObject()
{
	Log(FName("TArrayBindings"), "TArray.ErrorPaths.TriggerSortUnsupportedObject: before Sort");
	TArray<UObject> Objects;
	Objects.Add(nullptr);
	Objects.Sort();
}
)"));
	if (Module == nullptr)
	{
		return false;
	}

	Test.AddExpectedError(TEXT("Array index out of bounds."), EAutomationExpectedErrorFlags::Exact, 5, false);
	Test.AddExpectedError(TEXT("Iterator out of bounds."), EAutomationExpectedErrorFlags::Contains, 2);
	Test.AddExpectedError(TEXT("Cannot Add an element from the same array by reference. Copy it to a temporary first."), EAutomationExpectedErrorFlags::Contains, 1);
	Test.AddExpectedError(TEXT("Cannot Insert an element from the same array by reference. Copy it to a temporary first."), EAutomationExpectedErrorFlags::Contains, 1);
	Test.AddExpectedError(TEXT("Array index out of bounds. Need to insert between 0 and ArraySize"), EAutomationExpectedErrorFlags::Contains, 1);
	Test.AddExpectedError(TEXT("Cannot copy an array into itself."), EAutomationExpectedErrorFlags::Contains, 1);
	Test.AddExpectedError(TEXT("Count should not be negative."), EAutomationExpectedErrorFlags::Contains, 1);
	Test.AddExpectedError(TEXT("Source array out of bounds."), EAutomationExpectedErrorFlags::Contains, 1);
	Test.AddExpectedError(TEXT("Target array out of bounds."), EAutomationExpectedErrorFlags::Contains, 1);
	Test.AddExpectedError(TEXT("Cannot move assign an array into itself."), EAutomationExpectedErrorFlags::Contains, 1);
	Test.AddExpectedError(TEXT("Invalid negative Num"), EAutomationExpectedErrorFlags::Contains, 2);
	Test.AddExpectedError(TEXT("SetNumZeroed is not valid for arrays of non-primitive types."), EAutomationExpectedErrorFlags::Contains, 1);
	Test.AddExpectedError(TEXT("Array element type not sortable."), EAutomationExpectedErrorFlags::Contains, 1);
	Test.AddExpectedError(TArrayBindingsMakeModuleName(Profile, TEXT("ErrorPaths")), EAutomationExpectedErrorFlags::Contains, 0);
	Test.AddExpectedError(TEXT("void Trigger"), EAutomationExpectedErrorFlags::Contains, 0, false);

	bPassed = true;
	bPassed &= TArrayBindingsExecuteFunctionExpectingScriptException(
		Test,
		Engine,
		*Module,
		Profile,
		TEXT("void TriggerIndexOutOfBounds()"),
		TEXT("Array index out of bounds."),
		TEXT("TArray<int>.opIndex out-of-bounds"));
	bPassed &= TArrayBindingsExecuteFunctionExpectingScriptException(
		Test,
		Engine,
		*Module,
		Profile,
		TEXT("void TriggerLastOutOfBounds()"),
		TEXT("Array index out of bounds."),
		TEXT("TArray<int>.Last out-of-bounds"));
	bPassed &= TArrayBindingsExecuteFunctionExpectingScriptException(
		Test,
		Engine,
		*Module,
		Profile,
		TEXT("void TriggerSwapOutOfBounds()"),
		TEXT("Array index out of bounds."),
		TEXT("TArray<int>.Swap out-of-bounds"));
	bPassed &= TArrayBindingsExecuteFunctionExpectingScriptException(
		Test,
		Engine,
		*Module,
		Profile,
		TEXT("void TriggerInsertOutOfBounds()"),
		TEXT("Array index out of bounds. Need to insert between 0 and ArraySize"),
		TEXT("TArray<int>.Insert out-of-bounds"));
	bPassed &= TArrayBindingsExecuteFunctionExpectingScriptException(
		Test,
		Engine,
		*Module,
		Profile,
		TEXT("void TriggerRemoveAtOutOfBounds()"),
		TEXT("Array index out of bounds."),
		TEXT("TArray<int>.RemoveAt out-of-bounds"));
	bPassed &= TArrayBindingsExecuteFunctionExpectingScriptException(
		Test,
		Engine,
		*Module,
		Profile,
		TEXT("void TriggerRemoveAtSwapOutOfBounds()"),
		TEXT("Array index out of bounds."),
		TEXT("TArray<int>.RemoveAtSwap out-of-bounds"));
	bPassed &= TArrayBindingsExecuteFunctionExpectingScriptException(
		Test,
		Engine,
		*Module,
		Profile,
		TEXT("void TriggerAddSelfAlias()"),
		TEXT("Cannot Add an element from the same array by reference. Copy it to a temporary first."),
		TEXT("TArray<FString>.Add self-alias guard"));
	bPassed &= TArrayBindingsExecuteFunctionExpectingScriptException(
		Test,
		Engine,
		*Module,
		Profile,
		TEXT("void TriggerInsertSelfAlias()"),
		TEXT("Cannot Insert an element from the same array by reference. Copy it to a temporary first."),
		TEXT("TArray<FString>.Insert self-alias guard"));
	bPassed &= TArrayBindingsExecuteFunctionExpectingScriptException(
		Test,
		Engine,
		*Module,
		Profile,
		TEXT("void TriggerCopySelf()"),
		TEXT("Cannot copy an array into itself."),
		TEXT("TArray<int>.Copy self guard"));
	bPassed &= TArrayBindingsExecuteFunctionExpectingScriptException(
		Test,
		Engine,
		*Module,
		Profile,
		TEXT("void TriggerCopyNegativeCount()"),
		TEXT("Count should not be negative."),
		TEXT("TArray<int>.Copy negative count guard"));
	bPassed &= TArrayBindingsExecuteFunctionExpectingScriptException(
		Test,
		Engine,
		*Module,
		Profile,
		TEXT("void TriggerCopySourceOutOfBounds()"),
		TEXT("Source array out of bounds."),
		TEXT("TArray<int>.Copy source bounds guard"));
	bPassed &= TArrayBindingsExecuteFunctionExpectingScriptException(
		Test,
		Engine,
		*Module,
		Profile,
		TEXT("void TriggerCopyTargetOutOfBounds()"),
		TEXT("Target array out of bounds."),
		TEXT("TArray<int>.Copy target bounds guard"));
	bPassed &= TArrayBindingsExecuteFunctionExpectingScriptException(
		Test,
		Engine,
		*Module,
		Profile,
		TEXT("void TriggerMoveAssignSelf()"),
		TEXT("Cannot move assign an array into itself."),
		TEXT("TArray<int>.MoveAssignFrom self guard"));
	bPassed &= TArrayBindingsExecuteFunctionExpectingScriptException(
		Test,
		Engine,
		*Module,
		Profile,
		TEXT("void TriggerSetNumNegative()"),
		TEXT("Invalid negative Num"),
		TEXT("TArray<int>.SetNum negative guard"));
	bPassed &= TArrayBindingsExecuteFunctionExpectingScriptException(
		Test,
		Engine,
		*Module,
		Profile,
		TEXT("void TriggerSetNumZeroedNegative()"),
		TEXT("Invalid negative Num"),
		TEXT("TArray<int>.SetNumZeroed negative guard"));
	bPassed &= TArrayBindingsExecuteFunctionExpectingScriptException(
		Test,
		Engine,
		*Module,
		Profile,
		TEXT("void TriggerSetNumZeroedString()"),
		TEXT("SetNumZeroed is not valid for arrays of non-primitive types."),
		TEXT("TArray<FString>.SetNumZeroed primitive-type guard"));
	bPassed &= TArrayBindingsExecuteFunctionExpectingScriptException(
		Test,
		Engine,
		*Module,
		Profile,
		TEXT("void TriggerIteratorProceedOutOfBounds()"),
		TEXT("Iterator out of bounds."),
		TEXT("TArrayIterator<int>.Proceed out-of-bounds"));
	bPassed &= TArrayBindingsExecuteFunctionExpectingScriptException(
		Test,
		Engine,
		*Module,
		Profile,
		TEXT("void TriggerConstIteratorProceedOutOfBounds()"),
		TEXT("Iterator out of bounds."),
		TEXT("TArrayConstIterator<int>.Proceed out-of-bounds"));
	bPassed &= TArrayBindingsExecuteFunctionExpectingScriptException(
		Test,
		Engine,
		*Module,
		Profile,
		TEXT("void TriggerSortUnsupportedObject()"),
		TEXT("Array element type not sortable."),
		TEXT("TArray<UObject>.Sort unsupported type guard"));

	return bPassed;
}

bool RunTArrayNestedContainerRejectionSection(
	FAutomationTestBase& Test,
	FAngelscriptEngine& Engine,
	const FArraySyntaxCoverageProfile& Profile)
{
	bool bPassed = false;

	const FString ModuleName = TArrayBindingsMakeModuleName(Profile, TEXT("NestedContainerRejection"));
	const FString SourceFilename = FString::Printf(TEXT("%s.as"), *ModuleName);
	const FString Source = TEXT(R"(
int Entry()
{
	TArray<TArray<int>> Nested;
	return Nested.Num();
}
)");

	FAngelscriptCompileTraceSummary Summary;
	const bool bCompiled = CompileModuleWithSummary(
		&Engine,
		ECompileType::SoftReloadOnly,
		FName(*ModuleName),
		SourceFilename,
		Source,
		false,
		Summary,
		true);

	TArrayBindingsReportCompileSummaryDiagnostics(Test, TEXT("TArray<TArray<int>> nested container rejection"), Summary);

	bPassed = true;
	FNoDiscardAsserter LocalAssert(Test);
	bPassed &= LocalAssert.IsFalse(
		bCompiled,
		TEXT("TArray<TArray<int>> should fail compilation because nested containers are currently unsupported"));
	bPassed &= LocalAssert.AreEqual(
		ECompileResult::Error,
		Summary.CompileResult,
		TEXT("TArray<TArray<int>> rejection should be a compile error"));
	bPassed &= LocalAssert.IsTrue(
		TArrayBindingsCompileSummaryContainsDiagnosticMessage(Summary, TArrayBindingsNestedContainerDiagnostic),
		TEXT("TArray<TArray<int>> rejection should report the nested container diagnostic or use the shorthand parser rejection path"));

	return bPassed;
}

// ----------------------------------------------------------------------------
// Test class
// ----------------------------------------------------------------------------

TEST_CLASS_WITH_FLAGS(FAngelscriptTArrayBindingsTest,
	"Angelscript.TestModule.Bindings.Container.TArray",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	BEFORE_ALL()
	{
		ASTEST_CREATE_ENGINE();
	}

	AFTER_ALL() { FAngelscriptEngine& Engine = ASTEST_GET_ENGINE(); ASTEST_RESET_ENGINE(Engine); }

	TEST_METHOD(TArrayCompat)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine);

		TestRunner->AddInfo(FString::Printf(TEXT("%s.MutationCompat: begin"), TArrayProfile.CasePrefix));
		RunTArrayMutationCompatSection(*TestRunner, Engine, TArrayProfile);

		TestRunner->AddInfo(FString::Printf(TEXT("%s.IteratorCompat: begin"), TArrayProfile.CasePrefix));
		RunTArrayIteratorCompatSection(*TestRunner, Engine, TArrayProfile);

		TestRunner->AddInfo(FString::Printf(TEXT("%s.Operations: begin"), TArrayProfile.CasePrefix));
		RunTArrayOperationsSection(*TestRunner, Engine, TArrayProfile);

		TestRunner->AddInfo(FString::Printf(TEXT("%s.TypeMatrix: begin"), TArrayProfile.CasePrefix));
		RunTArrayTypeMatrixSection(*TestRunner, Engine, TArrayProfile);

		TestRunner->AddInfo(FString::Printf(TEXT("%s.ObjectTypes: begin"), TArrayProfile.CasePrefix));
		RunTArrayObjectTypesSection(*TestRunner, Engine, TArrayProfile);

		TestRunner->AddInfo(FString::Printf(TEXT("%s.ReturnValues: begin"), TArrayProfile.CasePrefix));
		RunTArrayReturnValuesSection(*TestRunner, Engine, TArrayProfile);

		TestRunner->AddInfo(FString::Printf(TEXT("%s.ErrorPaths: begin"), TArrayProfile.CasePrefix));
		RunTArrayErrorPathsSection(*TestRunner, Engine, TArrayProfile);

		TestRunner->AddInfo(FString::Printf(TEXT("%s.NestedContainerRejection: begin"), TArrayProfile.CasePrefix));
		RunTArrayNestedContainerRejectionSection(*TestRunner, Engine, TArrayProfile);

		}
	}
};

#endif
