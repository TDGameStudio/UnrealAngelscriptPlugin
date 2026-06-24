#include "CQTest.h"
#include "AngelscriptTestEngineHelper.h"
#include "AngelscriptTestUtilities.h"
#include "AngelscriptTestMacros.h"

#include "Containers/ScriptArray.h"
#include "Camera/CameraActor.h"
#include "GameFramework/Actor.h"
#include "Misc/ScopeExit.h"

#if WITH_DEV_AUTOMATION_TESTS


#include "AngelscriptTestExecute.h"
#include "Bindings/AngelscriptTArrayBindingsTestHelpers.h"

static const FString TArraySyntaxCompatNestedContainerDiagnostic = TEXT("Containers cannot be nested in other containers");
static const FArraySyntaxCoverageProfile TArraySyntaxCompatProfile = TArraySyntaxCompatCoverageProfile();


bool RunIntArrayMutationCompatSection(FAutomationTestBase& Test, FAngelscriptEngine& Engine)
{
	asIScriptModule* Module = TArrayBindingsBuildCoverageModule(Test, Engine, TArraySyntaxCompatProfile,
		TEXT("MutationCompat"),
		TEXT(R"(
int Entry()
{
	int[] Values;
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
	return LocalAssert.AreEqual(1, Result, TEXT("int[] mutation helpers should match TArray<int> script behaviour"));
}

bool RunIntArrayIteratorCompatSection(FAutomationTestBase& Test, FAngelscriptEngine& Engine)
{
	asIScriptModule* Module = TArrayBindingsBuildCoverageModule(Test, Engine, TArraySyntaxCompatProfile,
		TEXT("IteratorCompat"),
		TEXT(R"(
int Entry()
{
	int[] MutableValues;
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

	int[] ConstSource;
	ConstSource.Add(4);
	ConstSource.Add(5);
	ConstSource.Add(6);
	const int[] ConstValues = ConstSource;
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
	return LocalAssert.AreEqual(1, Result, TEXT("int[] iterator helpers should match TArray<int> script behaviour"));
}

bool RunIntArrayOperationsSection(FAutomationTestBase& Test, FAngelscriptEngine& Engine)
{
	const FString OperationsScript = FString(TEXT(R"(
void TraceSyntaxCase(const FString&in CaseName)
{
	Log(FName("TArraySyntaxCompatBindings"), "TArraySyntaxCompat.Operations: " + CaseName);
}

int IntDefaultIsEmpty()
{
	int[] Values;
	return Values.IsEmpty() && Values.Num() == 0 && Values.Max() == 0 ? 1 : 0;
}

int IntReserveKeepsNum()
{
	int[] Values;
	Values.Add(4);
	Values.Add(5);
	Values.Reserve(16);
	return Values.Num();
}

int IntReserveGrowsMax()
{
	int[] Values;
	Values.Add(4);
	Values.Add(5);
	Values.Reserve(16);
	return Values.Max();
}

int IntGetSlackAfterReserve()
{
	int[] Values;
	Values.Add(4);
	Values.Add(5);
	Values.Reserve(16);
	return Values.GetSlack();
}

int IntGetAllocatedSizeAfterReserve()
{
	int[] Values;
	Values.Reserve(8);
	return Values.GetAllocatedSize() > 0 ? 1 : 0;
}

int IntShrinkReducesMax()
{
	int[] Values;
	Values.Reserve(16);
	Values.Add(1);
	Values.Add(2);
	int MaxBefore = Values.Max();
	Values.Shrink();
	return Values.Max() <= MaxBefore && Values.Max() >= Values.Num() ? 1 : 0;
}

int IntEmptyReservedMax()
{
	int[] Values;
	Values.Add(1);
	Values.Empty(6);
	return Values.Num() == 0 ? Values.Max() : 0;
}

int IntResetKeepsCapacity()
{
	int[] Values;
	Values.Reserve(8);
	Values.Add(1);
	int MaxBefore = Values.Max();
	Values.Reset();
	return Values.Num() == 0 && Values.Max() >= MaxBefore ? 1 : 0;
}

int IntSetNumExtends()
{
	int[] Values;
	Values.Add(7);
	Values.SetNum(4);
	return Values.Num();
}

int IntSetNumPreservesAndZeroes()
{
	int[] Values;
	Values.Add(7);
	Values.SetNum(4);
	return Values[0] == 7 && Values[1] == 0 && Values[2] == 0 && Values[3] == 0 ? 1 : 0;
}

int IntSetNumZeroedPreservesAndZeroes()
{
	int[] Values;
	Values.Add(7);
	Values.SetNumZeroed(4);
	return Values[0] == 7 && Values[1] == 0 && Values[2] == 0 && Values[3] == 0 ? 1 : 0;
}

int IntSetNumDefaultClears()
{
	int[] Values;
	Values.Add(7);
	Values.SetNum();
	return Values.Num();
}

int IntSetNumZeroedDefaultClears()
{
	int[] Values;
	Values.Add(7);
	Values.SetNumZeroed();
	return Values.Num();
}

int IntIndexWriteRead()
{
	int[] Values;
	Values.SetNum(2);
	Values[0] = 42;
	return Values[0];
}

int IntConstIndexAndLastRead()
{
	int[] Source;
	Source.Add(12);
	Source.Add(22);
	const int[] Values = Source;
	return Values[0] + Values.Last();
}

int IntLastReadWrite()
{
	int[] Values;
	Values.Add(3);
	Values.Add(4);
	Values.Last() = 13;
	return Values[0] + Values.Last();
}

int IntIsValidIndex()
{
	int[] Values;
	Values.Add(9);
	return Values.IsValidIndex(0) && !Values.IsValidIndex(1) && !Values.IsValidIndex(-1) ? 1 : 0;
}

int IntAppend()
{
	int[] Values;
	Values.Add(1);
	int[] Other;
	Other.Add(2);
	Other.Add(3);
	Values.Append(Other);
	return Values[0] + Values[1] + Values[2];
}

int IntCopy()
{
	int[] Source;
	Source.Add(5);
	Source.Add(6);
	Source.Add(7);
	int[] Destination;
	Destination.Add(0);
	Destination.Add(0);
	Destination.Add(0);
	Destination.Copy(Source, 1, 2, 1);
	return Destination[0] == 0 && Destination[1] == 6 && Destination[2] == 7 ? 1 : 0;
}

int IntCopyDefaultTarget()
{
	int[] Source;
	Source.Add(8);
	int[] Destination;
	Destination.Add(0);
	Destination.Copy(Source, 0, 1);
	return Destination[0] == 8 ? 1 : 0;
}

int IntAssignEquals()
{
	int[] Source;
	Source.Add(1);
	int[] Assigned;
	Assigned = Source;
	return Assigned == Source && Assigned.Num() == 1 && Assigned[0] == 1 ? 1 : 0;
}

int IntEqualsFalse()
{
	int[] Left;
	Left.Add(1);
	int[] Right;
	Right.Add(2);
	return Left == Right ? 0 : 1;
}

int IntMoveAssignFrom()
{
	int[] Source;
	Source.Add(4);
	Source.Add(5);
	int[] Destination;
	Destination.Add(0);
	Destination.MoveAssignFrom(Source);
	return Destination.Num() == 2 && Destination[0] == 4 && Destination[1] == 5 && Source.Num() == 0 ? 1 : 0;
}

int IntSwap()
{
	int[] Values;
	Values.Add(1);
	Values.Add(2);
	Values.Swap(0, 1);
	return Values[0] == 2 && Values[1] == 1 ? 1 : 0;
}

int IntInsert()
{
	int[] Values;
	Values.Add(1);
	Values.Add(3);
	Values.Insert(2, 1);
	return Values[0] == 1 && Values[1] == 2 && Values[2] == 3 ? 1 : 0;
}

int IntInsertDefaultIndex()
{
	int[] Values;
	Values.Add(2);
	Values.Insert(1);
	return Values[0] == 1 && Values[1] == 2 ? 1 : 0;
}

int IntAddUnique()
{
	int[] Values;
	Values.Add(1);
	return !Values.AddUnique(1) && Values.AddUnique(2) && Values.Num() == 2 ? 1 : 0;
}

int IntContains()
{
	int[] Values;
	Values.Add(1);
	return Values.Contains(1) && !Values.Contains(2) ? 1 : 0;
}

int IntFindIndex()
{
	int[] Values;
	Values.Add(4);
	Values.Add(5);
	Values.Add(6);
	return Values.FindIndex(6);
}

int IntFindIndexMissing()
{
	int[] Values;
	Values.Add(4);
	return Values.FindIndex(6);
}
)")) + TEXT(R"(

int IntRemoveSingle()
{
	int[] Values;
	Values.Add(1);
	Values.Add(2);
	Values.Add(1);
	int Removed = Values.RemoveSingle(1);
	return Removed == 1 && Values.Num() == 2 && Values.Contains(1) && Values.Contains(2) ? 1 : 0;
}

int IntRemoveSingleMissing()
{
	int[] Values;
	Values.Add(1);
	return Values.RemoveSingle(9);
}

int IntRemove()
{
	int[] Values;
	Values.Add(1);
	Values.Add(2);
	Values.Add(1);
	int Removed = Values.Remove(1);
	return Removed == 2 && Values.Num() == 1 && Values[0] == 2 ? 1 : 0;
}

int IntRemoveMissing()
{
	int[] Values;
	Values.Add(1);
	return Values.Remove(9);
}

int IntRemoveSingleSwap()
{
	int[] Values;
	Values.Add(1);
	Values.Add(2);
	Values.Add(3);
	int Removed = Values.RemoveSingleSwap(2);
	return Removed == 1 && Values.Num() == 2 && !Values.Contains(2) && Values.Contains(1) && Values.Contains(3) ? 1 : 0;
}

int IntRemoveSingleSwapMissing()
{
	int[] Values;
	Values.Add(1);
	return Values.RemoveSingleSwap(9);
}

int IntRemoveSwap()
{
	int[] Values;
	Values.Add(1);
	Values.Add(2);
	Values.Add(1);
	Values.Add(3);
	int Removed = Values.RemoveSwap(1);
	return Removed == 2 && Values.Num() == 2 && !Values.Contains(1) && Values.Contains(2) && Values.Contains(3) ? 1 : 0;
}

int IntRemoveSwapMissing()
{
	int[] Values;
	Values.Add(1);
	return Values.RemoveSwap(9);
}

int IntRemoveAt()
{
	int[] Values;
	Values.Add(1);
	Values.Add(2);
	Values.Add(3);
	Values.RemoveAt(1);
	return Values.Num() == 2 && Values[0] == 1 && Values[1] == 3 ? 1 : 0;
}

int IntRemoveAtSwap()
{
	int[] Values;
	Values.Add(1);
	Values.Add(2);
	Values.Add(3);
	Values.RemoveAtSwap(0);
	return Values.Num() == 2 && !Values.Contains(1) && Values.Contains(2) && Values.Contains(3) ? 1 : 0;
}

int IntSortAscending()
{
	int[] Values;
	Values.Add(3);
	Values.Add(1);
	Values.Add(2);
	Values.Sort();
	return Values[0] * 100 + Values[1] * 10 + Values[2];
}

int IntSortDescending()
{
	int[] Values;
	Values.Add(3);
	Values.Add(1);
	Values.Add(2);
	Values.Sort(true);
	return Values[0] * 100 + Values[1] * 10 + Values[2];
}

int IntShufflePreservesElements()
{
	int[] Values;
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
	int[] Values;
	Values.Add(4);
	Values.Add(2);
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
	int[] Values;
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
	int[] Values;
	int Count = 0;
	foreach (int Value : Values)
	{
		Count += 1;
	}
	return Count;
}

int IntEmptyIteratorCannotProceed()
{
	int[] Values;
	TArrayIterator<int> Iterator = Values.Iterator();
	return Iterator.CanProceed ? 0 : 1;
}

int IntIteratorProceedSum()
{
	int[] Values;
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
	int[] Values;
	Values.Add(1);
	Values.Add(2);
	TArrayIterator<int> Iterator = Values.Iterator();
	Iterator.Proceed() = 20;
	return Values[0] + Values[1];
}

int IntConstIteratorProceedSum()
{
	int[] Source;
	Source.Add(4);
	Source.Add(5);
	Source.Add(6);
	const int[] Values = Source;
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
	int[] Values;
	Values.Add(1);
	Values.Add(2);
	TArrayIterator<int> Original = Values.Iterator();
	TArrayIterator<int> Copied = Original;
	TArrayIterator<int> Assigned = Copied;
	return Original.Proceed() + Copied.Proceed() + Assigned.Proceed();
}
)");

	asIScriptModule* Module = TArrayBindingsBuildCoverageModule(Test, Engine, TArraySyntaxCompatProfile,
		TEXT("Operations"),
		OperationsScript);
	if (Module == nullptr)
	{
		return false;
	}

	const TArray<FTArrayExpectedGlobalInt> ExactCases = {
		{ TEXT("int IntDefaultIsEmpty()"), TEXT("int[] default construction and IsEmpty"), 1 },
		{ TEXT("int IntReserveKeepsNum()"), TEXT("int[].Reserve should preserve Num"), 2 },
		{ TEXT("int IntGetAllocatedSizeAfterReserve()"), TEXT("int[].GetAllocatedSize should observe allocation"), 1 },
		{ TEXT("int IntShrinkReducesMax()"), TEXT("int[].Shrink should keep valid capacity"), 1 },
		{ TEXT("int IntResetKeepsCapacity()"), TEXT("int[].Reset should clear while keeping capacity"), 1 },
		{ TEXT("int IntSetNumExtends()"), TEXT("int[].SetNum should extend Num"), 4 },
		{ TEXT("int IntSetNumPreservesAndZeroes()"), TEXT("int[].SetNum should preserve and zero primitive slots"), 1 },
		{ TEXT("int IntSetNumZeroedPreservesAndZeroes()"), TEXT("int[].SetNumZeroed should preserve and zero primitive slots"), 1 },
		{ TEXT("int IntSetNumDefaultClears()"), TEXT("int[].SetNum default argument should clear"), 0 },
		{ TEXT("int IntSetNumZeroedDefaultClears()"), TEXT("int[].SetNumZeroed default argument should clear"), 0 },
		{ TEXT("int IntIndexWriteRead()"), TEXT("int[].opIndex should read/write by index"), 42 },
		{ TEXT("int IntConstIndexAndLastRead()"), TEXT("const int[].opIndex and Last should read values"), 34 },
		{ TEXT("int IntLastReadWrite()"), TEXT("int[].Last should read and write tail values"), 16 },
		{ TEXT("int IntIsValidIndex()"), TEXT("int[].IsValidIndex should reject invalid indices"), 1 },
		{ TEXT("int IntAppend()"), TEXT("int[].Append should copy source elements"), 6 },
		{ TEXT("int IntCopy()"), TEXT("int[].Copy should copy a source slice into target"), 1 },
		{ TEXT("int IntCopyDefaultTarget()"), TEXT("int[].Copy default target index should copy at zero"), 1 },
		{ TEXT("int IntAssignEquals()"), TEXT("int[] assignment and equality should round-trip"), 1 },
		{ TEXT("int IntEqualsFalse()"), TEXT("int[].opEquals should detect different arrays"), 1 },
		{ TEXT("int IntMoveAssignFrom()"), TEXT("int[].MoveAssignFrom should move values and empty source"), 1 },
		{ TEXT("int IntSwap()"), TEXT("int[].Swap should exchange elements"), 1 },
		{ TEXT("int IntInsert()"), TEXT("int[].Insert should place an element at index"), 1 },
		{ TEXT("int IntInsertDefaultIndex()"), TEXT("int[].Insert default index should insert at zero"), 1 },
		{ TEXT("int IntAddUnique()"), TEXT("int[].AddUnique should report duplicate suppression"), 1 },
		{ TEXT("int IntContains()"), TEXT("int[].Contains should detect present and missing values"), 1 },
		{ TEXT("int IntFindIndex()"), TEXT("int[].FindIndex should return the matching index"), 2 },
		{ TEXT("int IntFindIndexMissing()"), TEXT("int[].FindIndex should return -1 for missing values"), -1 },
		{ TEXT("int IntRemoveSingle()"), TEXT("int[].RemoveSingle should remove only the first match"), 1 },
		{ TEXT("int IntRemoveSingleMissing()"), TEXT("int[].RemoveSingle should return zero for missing values"), 0 },
		{ TEXT("int IntRemove()"), TEXT("int[].Remove should remove all matches"), 1 },
		{ TEXT("int IntRemoveMissing()"), TEXT("int[].Remove should return zero for missing values"), 0 },
		{ TEXT("int IntRemoveSingleSwap()"), TEXT("int[].RemoveSingleSwap should remove one match without preserving order"), 1 },
		{ TEXT("int IntRemoveSingleSwapMissing()"), TEXT("int[].RemoveSingleSwap should return zero for missing values"), 0 },
		{ TEXT("int IntRemoveSwap()"), TEXT("int[].RemoveSwap should remove all matches without preserving order"), 1 },
		{ TEXT("int IntRemoveSwapMissing()"), TEXT("int[].RemoveSwap should return zero for missing values"), 0 },
		{ TEXT("int IntRemoveAt()"), TEXT("int[].RemoveAt should preserve order"), 1 },
		{ TEXT("int IntRemoveAtSwap()"), TEXT("int[].RemoveAtSwap should remove by index without preserving order"), 1 },
		{ TEXT("int IntSortAscending()"), TEXT("int[].Sort ascending should order values"), 123 },
		{ TEXT("int IntSortDescending()"), TEXT("int[].Sort descending should order values"), 321 },
		{ TEXT("int IntShufflePreservesElements()"), TEXT("int[].Shuffle should preserve element set"), 1 },
		{ TEXT("int IntForeachValueSum()"), TEXT("int[] foreach value traversal should visit every element"), 12 },
		{ TEXT("int IntForeachValueIndexWeightedSum()"), TEXT("int[] foreach value/index traversal should expose stable indices"), 28 },
		{ TEXT("int EmptyForeachVisitCount()"), TEXT("int[] foreach should skip empty arrays"), 0 },
		{ TEXT("int IntEmptyIteratorCannotProceed()"), TEXT("TArrayIterator<int> on an int[] should not proceed when empty"), 1 },
		{ TEXT("int IntIteratorProceedSum()"), TEXT("TArrayIterator<int>.Proceed should walk int[] arrays"), 6 },
		{ TEXT("int IntMutableIteratorCanWrite()"), TEXT("TArrayIterator<int>.Proceed should return a writable int[] reference"), 22 },
		{ TEXT("int IntConstIteratorProceedSum()"), TEXT("TArrayConstIterator<int>.Proceed should walk const int[] arrays"), 15 },
		{ TEXT("int IntIteratorCopyAssignStartsAtSameElement()"), TEXT("TArrayIterator<int> copy and assignment should preserve int[] iterator state"), 3 },
	};
	const TArray<FTArrayExpectedGlobalIntAtLeast> MinimumCases = {
		{ TEXT("int IntReserveGrowsMax()"), TEXT("int[].Reserve should grow Max"), 16 },
		{ TEXT("int IntGetSlackAfterReserve()"), TEXT("int[].GetSlack should expose reserved slack"), 14 },
		{ TEXT("int IntEmptyReservedMax()"), TEXT("int[].Empty should honor reserved size"), 6 },
	};

	bool bPassed = ExpectTArrayBindingsGlobalInts(Test, Engine, *Module, TArraySyntaxCompatProfile, ExactCases);
	bPassed &= ExpectTArrayBindingsGlobalIntsAtLeast(Test, Engine, *Module, TArraySyntaxCompatProfile, MinimumCases);

	return bPassed;
}

bool RunSyntaxTypeMatrixSection(FAutomationTestBase& Test, FAngelscriptEngine& Engine)
{
	asIScriptModule* Module = TArrayBindingsBuildCoverageModule(Test, Engine, TArraySyntaxCompatProfile,
		TEXT("TypeMatrix"),
		TEXT(R"(
void TraceSyntaxCase(const FString&in CaseName)
{
	Log(FName("TArraySyntaxCompatBindings"), "TArraySyntaxCompat.TypeMatrix: " + CaseName);
}

int UInt8ArraySum()
{
	uint8[] Values;
	Values.Add(250);
	Values.Add(5);
	return int(Values[0]) + int(Values[1]);
}

int Int8ArrayKeepsSignedBytes()
{
	int8[] Values;
	Values.Add(-8);
	Values.Add(7);
	return int(Values[0]) + int(Values[1]);
}

int UInt16ArraySum()
{
	uint16[] Values;
	Values.Add(60000);
	Values.Add(123);
	return int(Values.Num() == 2 && Values[0] == 60000 && Values[1] == 123 ? 1 : 0);
}

int Int16ArrayKeepsSignedValues()
{
	int16[] Values;
	Values.Add(-1234);
	Values.Add(4321);
	return Values[0] + Values[1];
}

int UInt32ArrayKeepsWideValues()
{
	uint32[] Values;
	Values.Add(4000000000);
	Values.Add(42);
	return Values.Num() == 2 && Values[0] == 4000000000 && Values[1] == 42 ? 1 : 0;
}

int Int64ArrayKeepsWideValues()
{
	int64[] Values;
	Values.Add(5000000000);
	Values.Add(-9);
	return Values.Num() == 2 && Values[0] == 5000000000 && Values[1] == -9 ? 1 : 0;
}

int UInt64ArrayKeepsWideValues()
{
	uint64[] Values;
	Values.Add(9000000000);
	Values.Add(11);
	return Values.Num() == 2 && Values[0] == 9000000000 && Values[1] == 11 ? 1 : 0;
}

int FloatArrayScaledSum()
{
	float[] Values;
	Values.Add(1.25f);
	Values.Add(2.50f);
	return int((Values[0] + Values[1]) * 100.0f);
}

int DoubleArrayScaledSum()
{
	double[] Values;
	Values.Add(1.25);
	Values.Add(2.50);
	return int((Values[0] + Values[1]) * 100.0);
}

int BoolArrayTrueCount()
{
	bool[] Values;
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
	FString[] Values;
	Values.Add("Al");
	Values.Add("pha");
	FString Combined = Values[0] + Values[1];
	return Combined == "Alpha" && Values.Contains("pha") && Values.FindIndex("Al") == 0 ? 1 : 0;
}

int FNameArrayOperations()
{
	FName[] Values;
	Values.Add(FName("Alpha"));
	Values.Add(FName("Beta"));
	return Values.Contains(FName("Beta")) && Values.FindIndex(FName("Alpha")) == 0 ? 1 : 0;
}

int FVectorArrayOperations()
{
	FVector[] Values;
	Values.Add(FVector(1.0, 2.0, 3.0));
	Values.Add(FVector(4.0, 5.0, 6.0));
	double Sum = 0.0;
	foreach (const FVector& Value : Values)
	{
		Sum += Value.X + Value.Y + Value.Z;
	}
	return int(Sum);
}

int FVector2DArrayOperations()
{
	FVector2D[] Values;
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
	FVector4[] Values;
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
	FRotator[] Values;
	Values.Add(FRotator(10.0, 20.0, 30.0));
	Values.Add(FRotator(1.0, 2.0, 3.0));
	return int(Values[0].Pitch + Values[0].Yaw + Values[0].Roll + Values[1].Pitch + Values[1].Yaw + Values[1].Roll);
}

int FTransformArrayOperations()
{
	FTransform[] Values;
	Values.Add(FTransform(FVector(10.0, 20.0, 30.0)));
	Values.Add(FTransform(FVector(1.0, 2.0, 3.0)));
	FVector Translation = Values[0].GetTranslation() + Values[1].GetTranslation();
	return int(Translation.X + Translation.Y + Translation.Z);
}

int FLinearColorArrayOperations()
{
	FLinearColor[] Values;
	Values.Add(FLinearColor(0.25f, 0.50f, 0.75f, 1.0f));
	Values.Add(FLinearColor(1.0f, 0.75f, 0.50f, 0.25f));
	return int((Values[0].R + Values[0].G + Values[0].B + Values[0].A + Values[1].R + Values[1].G + Values[1].B + Values[1].A) * 100.0f);
}

int FColorArrayOperations()
{
	FColor[] Values;
	Values.Add(FColor(1, 2, 3, 4));
	Values.Add(FColor(5, 6, 7, 8));
	return Values.Num() == 2 && Values[0] == FColor(1, 2, 3, 4) && Values[1].ToHex() == "05060708" ? 1 : 0;
}

int FIntPointArrayOperations()
{
	FIntPoint[] Values;
	Values.Add(FIntPoint(3, 5));
	Values.Add(FIntPoint(7, 11));
	return Values[0].X + Values[0].Y + Values[1].X + Values[1].Y;
}

int FIntVectorArrayOperations()
{
	FIntVector[] Values;
	Values.Add(FIntVector(1, 2, 3));
	Values.Add(FIntVector(4, 5, 6));
	return Values[0].X + Values[0].Y + Values[0].Z + Values[1].X + Values[1].Y + Values[1].Z;
}

int FIntVector4ArrayOperations()
{
	FIntVector4[] Values;
	Values.Add(FIntVector4(1, 2, 3, 4));
	Values.Add(FIntVector4(5, 6, 7, 8));
	return Values[0].X + Values[0].Y + Values[0].Z + Values[0].W + Values[1].X + Values[1].Y + Values[1].Z + Values[1].W;
}

int FGuidArrayOperations()
{
	FGuid[] Values;
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
	Log(FName("TArraySyntaxCompatBindings"), "TArraySyntaxCompat.TypeMatrix.FTextArrayOperations: begin");
	FText[] Values;
	Values.SetNum(2);
	Log(FName("TArraySyntaxCompatBindings"), "TArraySyntaxCompat.TypeMatrix.FTextArrayOperations: after SetNum Num=" + Values.Num());
	int EmptyCount = 0;
	foreach (const FText& Value : Values)
	{
		Log(FName("TArraySyntaxCompatBindings"), "TArraySyntaxCompat.TypeMatrix.FTextArrayOperations: foreach IsEmpty=" + (Value.IsEmpty() ? "true" : "false"));
		if (Value.IsEmpty())
			EmptyCount += 1;
	}
	FText[] Copy;
	Copy = Values;
	Log(FName("TArraySyntaxCompatBindings"), "TArraySyntaxCompat.TypeMatrix.FTextArrayOperations: copy Num=" + Copy.Num() + ", EmptyCount=" + EmptyCount);
	return Copy.Num() == 2 && EmptyCount == 2 ? 1 : 0;
}

int TSubclassOfActorArrayOperations()
{
	TSubclassOf<AActor>[] Classes;
	Classes.Add(AActor::StaticClass());
	Classes.Add(ACameraActor::StaticClass());
	bool bAddedDuplicate = Classes.AddUnique(AActor::StaticClass());
	TSubclassOf<AActor>[] Copy;
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
		{ TEXT("int UInt8ArraySum()"), TEXT("uint8[] should store byte values"), 255 },
		{ TEXT("int Int8ArrayKeepsSignedBytes()"), TEXT("int8[] should preserve signed byte values"), -1 },
		{ TEXT("int UInt16ArraySum()"), TEXT("uint16[] should preserve unsigned 16-bit values"), 1 },
		{ TEXT("int Int16ArrayKeepsSignedValues()"), TEXT("int16[] should preserve signed 16-bit values"), 3087 },
		{ TEXT("int UInt32ArrayKeepsWideValues()"), TEXT("uint32[] should preserve unsigned 32-bit values"), 1 },
		{ TEXT("int Int64ArrayKeepsWideValues()"), TEXT("int64[] should preserve wide integer values"), 1 },
		{ TEXT("int UInt64ArrayKeepsWideValues()"), TEXT("uint64[] should preserve unsigned 64-bit values"), 1 },
		{ TEXT("int FloatArrayScaledSum()"), TEXT("float[] should preserve numeric values"), 375 },
		{ TEXT("int DoubleArrayScaledSum()"), TEXT("double[] should preserve numeric values"), 375 },
		{ TEXT("int BoolArrayTrueCount()"), TEXT("bool[] should preserve bool values through foreach"), 2 },
		{ TEXT("int FStringArrayOperations()"), TEXT("FString[] should support add/index/search"), 1 },
		{ TEXT("int FNameArrayOperations()"), TEXT("FName[] should support add/index/search"), 1 },
		{ TEXT("int FVectorArrayOperations()"), TEXT("FVector[] should support struct values and const foreach"), 21 },
		{ TEXT("int FVector2DArrayOperations()"), TEXT("FVector2D[] should support struct values and const foreach"), 17 },
		{ TEXT("int FVector4ArrayOperations()"), TEXT("FVector4[] should support 4D struct values and const foreach"), 36 },
		{ TEXT("int FRotatorArrayOperations()"), TEXT("FRotator[] should support rotator struct values"), 66 },
		{ TEXT("int FTransformArrayOperations()"), TEXT("FTransform[] should support transform struct values"), 66 },
		{ TEXT("int FLinearColorArrayOperations()"), TEXT("FLinearColor[] should support linear color struct values"), 500 },
		{ TEXT("int FColorArrayOperations()"), TEXT("FColor[] should support color struct values"), 1 },
		{ TEXT("int FIntPointArrayOperations()"), TEXT("FIntPoint[] should support int point struct values"), 26 },
		{ TEXT("int FIntVectorArrayOperations()"), TEXT("FIntVector[] should support int vector struct values"), 21 },
		{ TEXT("int FIntVector4ArrayOperations()"), TEXT("FIntVector4[] should support int vector4 struct values"), 36 },
		{ TEXT("int FGuidArrayOperations()"), TEXT("FGuid[] should support guid struct values and foreach"), 36 },
		{ TEXT("int FTextArrayOperations()"), TEXT("FText[] should default construct and copy text values"), 1 },
		{ TEXT("int TSubclassOfActorArrayOperations()"), TEXT("TSubclassOf<AActor>[] should store actor class wrappers"), 1 },
	};

	return ExpectTArrayBindingsGlobalInts(Test, Engine, *Module, TArraySyntaxCompatProfile, Cases);
}

bool RunSyntaxReturnValuesSection(FAutomationTestBase& Test, FAngelscriptEngine& Engine)
{
	asIScriptModule* Module = TArrayBindingsBuildCoverageModule(Test, Engine, TArraySyntaxCompatProfile,
		TEXT("ReturnValues"),
		TEXT(R"(
void TraceSyntaxCase(const FString&in CaseName)
{
	Log(FName("TArraySyntaxCompatBindings"), "TArraySyntaxCompat.ReturnValues: " + CaseName);
}

int[] ReturnIntArray()
{
	Log(FName("TArraySyntaxCompatBindings"), "TArraySyntaxCompat.ReturnValues.ReturnIntArray: begin");
	int[] Values;
	Values.Add(3);
	Values.Add(5);
	Values.Add(8);
	Log(FName("TArraySyntaxCompatBindings"), "TArraySyntaxCompat.ReturnValues.ReturnIntArray: returning");
	return Values;
}

FVector[] ReturnVectorArray()
{
	Log(FName("TArraySyntaxCompatBindings"), "TArraySyntaxCompat.ReturnValues.ReturnVectorArray: begin");
	FVector[] Values;
	Values.Add(FVector(1.0, 2.0, 3.0));
	Values.Add(FVector(4.0, 5.0, 6.0));
	Log(FName("TArraySyntaxCompatBindings"), "TArraySyntaxCompat.ReturnValues.ReturnVectorArray: returning");
	return Values;
}

FString[] ReturnStringArray()
{
	Log(FName("TArraySyntaxCompatBindings"), "TArraySyntaxCompat.ReturnValues.ReturnStringArray: begin");
	FString[] Values;
	Values.Add("Alpha");
	Values.Add("Beta");
	Log(FName("TArraySyntaxCompatBindings"), "TArraySyntaxCompat.ReturnValues.ReturnStringArray: returning");
	return Values;
}

TSubclassOf<AActor>[] ReturnActorClassArray()
{
	Log(FName("TArraySyntaxCompatBindings"), "TArraySyntaxCompat.ReturnValues.ReturnActorClassArray: begin");
	TSubclassOf<AActor>[] Classes;
	Classes.Add(AActor::StaticClass());
	Classes.Add(ACameraActor::StaticClass());
	Log(FName("TArraySyntaxCompatBindings"), "TArraySyntaxCompat.ReturnValues.ReturnActorClassArray: returning");
	return Classes;
}
)"));
	if (Module == nullptr)
	{
		return false;
	}

	bool bPassed = true;
	bPassed &= TArrayBindingsExecuteFunctionReturningScriptArray(
		Test,
		Engine,
		*Module,
		TArraySyntaxCompatProfile,
		TArrayBindingsMakeArrayFunctionDecl(TArraySyntaxCompatProfile, TEXT("int"), TEXT("ReturnIntArray")),
		TEXT("int[] return value should expose returned primitive elements"),
		[&Test](const FScriptArray& ReturnedArray)
		{
			FNoDiscardAsserter LocalAssert(Test);
			const int32* Values = static_cast<const int32*>(ReturnedArray.GetData());
			return LocalAssert.AreEqual(3, ReturnedArray.Num(), TEXT("Returned int[] should have Num=3"))
				&& LocalAssert.IsNotNull(Values, TEXT("Returned int[] should expose data"))
				&& LocalAssert.AreEqual(3, Values[0], TEXT("Returned int[][0]"))
				&& LocalAssert.AreEqual(5, Values[1], TEXT("Returned int[][1]"))
				&& LocalAssert.AreEqual(8, Values[2], TEXT("Returned int[][2]"));
		});
	bPassed &= TArrayBindingsExecuteFunctionReturningScriptArray(
		Test,
		Engine,
		*Module,
		TArraySyntaxCompatProfile,
		TArrayBindingsMakeArrayFunctionDecl(TArraySyntaxCompatProfile, TEXT("FVector"), TEXT("ReturnVectorArray")),
		TEXT("FVector[] return value should expose returned struct elements"),
		[&Test](const FScriptArray& ReturnedArray)
		{
			FNoDiscardAsserter LocalAssert(Test);
			const FVector* Values = static_cast<const FVector*>(ReturnedArray.GetData());
			return LocalAssert.AreEqual(2, ReturnedArray.Num(), TEXT("Returned FVector[] should have Num=2"))
				&& LocalAssert.IsNotNull(Values, TEXT("Returned FVector[] should expose data"))
				&& LocalAssert.AreEqual(FVector(1.0, 2.0, 3.0), Values[0], TEXT("Returned FVector[][0]"))
				&& LocalAssert.AreEqual(FVector(4.0, 5.0, 6.0), Values[1], TEXT("Returned FVector[][1]"));
		});
	bPassed &= TArrayBindingsExecuteFunctionReturningScriptArray(
		Test,
		Engine,
		*Module,
		TArraySyntaxCompatProfile,
		TArrayBindingsMakeArrayFunctionDecl(TArraySyntaxCompatProfile, TEXT("FString"), TEXT("ReturnStringArray")),
		TEXT("FString[] return value should expose returned string elements"),
		[&Test](const FScriptArray& ReturnedArray)
		{
			FNoDiscardAsserter LocalAssert(Test);
			const FString* Values = static_cast<const FString*>(ReturnedArray.GetData());
			return LocalAssert.AreEqual(2, ReturnedArray.Num(), TEXT("Returned FString[] should have Num=2"))
				&& LocalAssert.IsNotNull(Values, TEXT("Returned FString[] should expose data"))
				&& LocalAssert.AreEqual(FString(TEXT("Alpha")), Values[0], TEXT("Returned FString[][0]"))
				&& LocalAssert.AreEqual(FString(TEXT("Beta")), Values[1], TEXT("Returned FString[][1]"));
		});
	bPassed &= TArrayBindingsExecuteFunctionReturningScriptArray(
		Test,
		Engine,
		*Module,
		TArraySyntaxCompatProfile,
		TArrayBindingsMakeArrayFunctionDecl(TArraySyntaxCompatProfile, TEXT("TSubclassOf<AActor>"), TEXT("ReturnActorClassArray")),
		TEXT("TSubclassOf<AActor>[] return value should expose returned class wrappers"),
		[&Test](const FScriptArray& ReturnedArray)
		{
			FNoDiscardAsserter LocalAssert(Test);
			const TSubclassOf<AActor>* Classes = static_cast<const TSubclassOf<AActor>*>(ReturnedArray.GetData());
			return LocalAssert.AreEqual(2, ReturnedArray.Num(), TEXT("Returned TSubclassOf<AActor>[] should have Num=2"))
				&& LocalAssert.IsNotNull(Classes, TEXT("Returned TSubclassOf<AActor>[] should expose data"))
				&& LocalAssert.AreEqual(AActor::StaticClass(), Classes[0].Get(), TEXT("Returned TSubclassOf<AActor>[][0]"))
				&& LocalAssert.AreEqual(ACameraActor::StaticClass(), Classes[1].Get(), TEXT("Returned TSubclassOf<AActor>[][1]"));
		});

	return bPassed;
}

bool RunSyntaxErrorPathsSection(FAutomationTestBase& Test, FAngelscriptEngine& Engine)
{
	asIScriptModule* Module = TArrayBindingsBuildCoverageModule(Test, Engine, TArraySyntaxCompatProfile,
		TEXT("ErrorPaths"),
		TEXT(R"(
void TraceSyntaxCase(const FString&in CaseName)
{
	Log(FName("TArraySyntaxCompatBindings"), "TArraySyntaxCompat.ErrorPaths: " + CaseName);
}

void TriggerIndexOutOfBounds()
{
	Log(FName("TArraySyntaxCompatBindings"), "TArraySyntaxCompat.ErrorPaths.TriggerIndexOutOfBounds: before opIndex");
	int[] Values;
	Values.Add(1);
	Values[1] = 2;
}

void TriggerLastOutOfBounds()
{
	Log(FName("TArraySyntaxCompatBindings"), "TArraySyntaxCompat.ErrorPaths.TriggerLastOutOfBounds: before Last");
	int[] Values;
	int Value = Values.Last();
}

void TriggerSwapOutOfBounds()
{
	Log(FName("TArraySyntaxCompatBindings"), "TArraySyntaxCompat.ErrorPaths.TriggerSwapOutOfBounds: before Swap");
	int[] Values;
	Values.Add(1);
	Values.Swap(0, 1);
}

void TriggerInsertOutOfBounds()
{
	Log(FName("TArraySyntaxCompatBindings"), "TArraySyntaxCompat.ErrorPaths.TriggerInsertOutOfBounds: before Insert");
	int[] Values;
	Values.Insert(1, 1);
}

void TriggerRemoveAtOutOfBounds()
{
	Log(FName("TArraySyntaxCompatBindings"), "TArraySyntaxCompat.ErrorPaths.TriggerRemoveAtOutOfBounds: before RemoveAt");
	int[] Values;
	Values.RemoveAt(0);
}

void TriggerRemoveAtSwapOutOfBounds()
{
	Log(FName("TArraySyntaxCompatBindings"), "TArraySyntaxCompat.ErrorPaths.TriggerRemoveAtSwapOutOfBounds: before RemoveAtSwap");
	int[] Values;
	Values.RemoveAtSwap(0);
}

void TriggerAddSelfAlias()
{
	Log(FName("TArraySyntaxCompatBindings"), "TArraySyntaxCompat.ErrorPaths.TriggerAddSelfAlias: before Add");
	FString[] Values;
	Values.Add("Alpha");
	Values.Add(Values[0]);
}

void TriggerInsertSelfAlias()
{
	Log(FName("TArraySyntaxCompatBindings"), "TArraySyntaxCompat.ErrorPaths.TriggerInsertSelfAlias: before Insert");
	FString[] Values;
	Values.Add("Alpha");
	Values.Insert(Values[0], 0);
}

void TriggerCopySelf()
{
	Log(FName("TArraySyntaxCompatBindings"), "TArraySyntaxCompat.ErrorPaths.TriggerCopySelf: before Copy");
	int[] Values;
	Values.Add(1);
	Values.Copy(Values, 0, 1, 0);
}

void TriggerCopyNegativeCount()
{
	Log(FName("TArraySyntaxCompatBindings"), "TArraySyntaxCompat.ErrorPaths.TriggerCopyNegativeCount: before Copy");
	int[] Source;
	Source.Add(1);
	int[] Destination;
	Destination.Add(0);
	Destination.Copy(Source, 0, -1, 0);
}

void TriggerCopySourceOutOfBounds()
{
	Log(FName("TArraySyntaxCompatBindings"), "TArraySyntaxCompat.ErrorPaths.TriggerCopySourceOutOfBounds: before Copy");
	int[] Source;
	Source.Add(1);
	int[] Destination;
	Destination.Add(0);
	Destination.Copy(Source, 1, 1, 0);
}

void TriggerCopyTargetOutOfBounds()
{
	Log(FName("TArraySyntaxCompatBindings"), "TArraySyntaxCompat.ErrorPaths.TriggerCopyTargetOutOfBounds: before Copy");
	int[] Source;
	Source.Add(1);
	int[] Destination;
	Destination.Copy(Source, 0, 1, 0);
}

void TriggerMoveAssignSelf()
{
	Log(FName("TArraySyntaxCompatBindings"), "TArraySyntaxCompat.ErrorPaths.TriggerMoveAssignSelf: before MoveAssignFrom");
	int[] Values;
	Values.Add(1);
	Values.MoveAssignFrom(Values);
}

void TriggerSetNumNegative()
{
	Log(FName("TArraySyntaxCompatBindings"), "TArraySyntaxCompat.ErrorPaths.TriggerSetNumNegative: before SetNum");
	int[] Values;
	Values.SetNum(-1);
}

void TriggerSetNumZeroedNegative()
{
	Log(FName("TArraySyntaxCompatBindings"), "TArraySyntaxCompat.ErrorPaths.TriggerSetNumZeroedNegative: before SetNumZeroed");
	int[] Values;
	Values.SetNumZeroed(-1);
}

void TriggerSetNumZeroedString()
{
	Log(FName("TArraySyntaxCompatBindings"), "TArraySyntaxCompat.ErrorPaths.TriggerSetNumZeroedString: before SetNumZeroed");
	FString[] Values;
	Values.SetNumZeroed(1);
}

void TriggerIteratorProceedOutOfBounds()
{
	Log(FName("TArraySyntaxCompatBindings"), "TArraySyntaxCompat.ErrorPaths.TriggerIteratorProceedOutOfBounds: before Proceed");
	int[] Values;
	TArrayIterator<int> Iterator = Values.Iterator();
	Iterator.Proceed();
}

void TriggerConstIteratorProceedOutOfBounds()
{
	Log(FName("TArraySyntaxCompatBindings"), "TArraySyntaxCompat.ErrorPaths.TriggerConstIteratorProceedOutOfBounds: before Proceed");
	int[] Source;
	const int[] Values = Source;
	TArrayConstIterator<int> Iterator = Values.Iterator();
	Iterator.Proceed();
}

void TriggerSortUnsupportedText()
{
	Log(FName("TArraySyntaxCompatBindings"), "TArraySyntaxCompat.ErrorPaths.TriggerSortUnsupportedText: before Sort");
	FText[] Values;
	Values.SetNum(1);
	Values.Sort();
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
	Test.AddExpectedError(TArrayBindingsMakeModuleName(TArraySyntaxCompatProfile, TEXT("ErrorPaths")), EAutomationExpectedErrorFlags::Contains, 0);
	Test.AddExpectedError(TEXT("void Trigger"), EAutomationExpectedErrorFlags::Contains, 0, false);

	bool bPassed = true;
	bPassed &= TArrayBindingsExecuteFunctionExpectingScriptException(
		Test,
		Engine,
		*Module,
		TArraySyntaxCompatProfile,
		TEXT("void TriggerIndexOutOfBounds()"),
		TEXT("Array index out of bounds."),
		TEXT("int[].opIndex out-of-bounds"));
	bPassed &= TArrayBindingsExecuteFunctionExpectingScriptException(
		Test,
		Engine,
		*Module,
		TArraySyntaxCompatProfile,
		TEXT("void TriggerLastOutOfBounds()"),
		TEXT("Array index out of bounds."),
		TEXT("int[].Last out-of-bounds"));
	bPassed &= TArrayBindingsExecuteFunctionExpectingScriptException(
		Test,
		Engine,
		*Module,
		TArraySyntaxCompatProfile,
		TEXT("void TriggerSwapOutOfBounds()"),
		TEXT("Array index out of bounds."),
		TEXT("int[].Swap out-of-bounds"));
	bPassed &= TArrayBindingsExecuteFunctionExpectingScriptException(
		Test,
		Engine,
		*Module,
		TArraySyntaxCompatProfile,
		TEXT("void TriggerInsertOutOfBounds()"),
		TEXT("Array index out of bounds. Need to insert between 0 and ArraySize"),
		TEXT("int[].Insert out-of-bounds"));
	bPassed &= TArrayBindingsExecuteFunctionExpectingScriptException(
		Test,
		Engine,
		*Module,
		TArraySyntaxCompatProfile,
		TEXT("void TriggerRemoveAtOutOfBounds()"),
		TEXT("Array index out of bounds."),
		TEXT("int[].RemoveAt out-of-bounds"));
	bPassed &= TArrayBindingsExecuteFunctionExpectingScriptException(
		Test,
		Engine,
		*Module,
		TArraySyntaxCompatProfile,
		TEXT("void TriggerRemoveAtSwapOutOfBounds()"),
		TEXT("Array index out of bounds."),
		TEXT("int[].RemoveAtSwap out-of-bounds"));
	bPassed &= TArrayBindingsExecuteFunctionExpectingScriptException(
		Test,
		Engine,
		*Module,
		TArraySyntaxCompatProfile,
		TEXT("void TriggerAddSelfAlias()"),
		TEXT("Cannot Add an element from the same array by reference. Copy it to a temporary first."),
		TEXT("FString[].Add self-alias guard"));
	bPassed &= TArrayBindingsExecuteFunctionExpectingScriptException(
		Test,
		Engine,
		*Module,
		TArraySyntaxCompatProfile,
		TEXT("void TriggerInsertSelfAlias()"),
		TEXT("Cannot Insert an element from the same array by reference. Copy it to a temporary first."),
		TEXT("FString[].Insert self-alias guard"));
	bPassed &= TArrayBindingsExecuteFunctionExpectingScriptException(
		Test,
		Engine,
		*Module,
		TArraySyntaxCompatProfile,
		TEXT("void TriggerCopySelf()"),
		TEXT("Cannot copy an array into itself."),
		TEXT("int[].Copy self guard"));
	bPassed &= TArrayBindingsExecuteFunctionExpectingScriptException(
		Test,
		Engine,
		*Module,
		TArraySyntaxCompatProfile,
		TEXT("void TriggerCopyNegativeCount()"),
		TEXT("Count should not be negative."),
		TEXT("int[].Copy negative count guard"));
	bPassed &= TArrayBindingsExecuteFunctionExpectingScriptException(
		Test,
		Engine,
		*Module,
		TArraySyntaxCompatProfile,
		TEXT("void TriggerCopySourceOutOfBounds()"),
		TEXT("Source array out of bounds."),
		TEXT("int[].Copy source bounds guard"));
	bPassed &= TArrayBindingsExecuteFunctionExpectingScriptException(
		Test,
		Engine,
		*Module,
		TArraySyntaxCompatProfile,
		TEXT("void TriggerCopyTargetOutOfBounds()"),
		TEXT("Target array out of bounds."),
		TEXT("int[].Copy target bounds guard"));
	bPassed &= TArrayBindingsExecuteFunctionExpectingScriptException(
		Test,
		Engine,
		*Module,
		TArraySyntaxCompatProfile,
		TEXT("void TriggerMoveAssignSelf()"),
		TEXT("Cannot move assign an array into itself."),
		TEXT("int[].MoveAssignFrom self guard"));
	bPassed &= TArrayBindingsExecuteFunctionExpectingScriptException(
		Test,
		Engine,
		*Module,
		TArraySyntaxCompatProfile,
		TEXT("void TriggerSetNumNegative()"),
		TEXT("Invalid negative Num"),
		TEXT("int[].SetNum negative guard"));
	bPassed &= TArrayBindingsExecuteFunctionExpectingScriptException(
		Test,
		Engine,
		*Module,
		TArraySyntaxCompatProfile,
		TEXT("void TriggerSetNumZeroedNegative()"),
		TEXT("Invalid negative Num"),
		TEXT("int[].SetNumZeroed negative guard"));
	bPassed &= TArrayBindingsExecuteFunctionExpectingScriptException(
		Test,
		Engine,
		*Module,
		TArraySyntaxCompatProfile,
		TEXT("void TriggerSetNumZeroedString()"),
		TEXT("SetNumZeroed is not valid for arrays of non-primitive types."),
		TEXT("FString[].SetNumZeroed primitive-type guard"));
	bPassed &= TArrayBindingsExecuteFunctionExpectingScriptException(
		Test,
		Engine,
		*Module,
		TArraySyntaxCompatProfile,
		TEXT("void TriggerIteratorProceedOutOfBounds()"),
		TEXT("Iterator out of bounds."),
		TEXT("TArrayIterator<int> over int[] Proceed out-of-bounds"));
	bPassed &= TArrayBindingsExecuteFunctionExpectingScriptException(
		Test,
		Engine,
		*Module,
		TArraySyntaxCompatProfile,
		TEXT("void TriggerConstIteratorProceedOutOfBounds()"),
		TEXT("Iterator out of bounds."),
		TEXT("TArrayConstIterator<int> over int[] Proceed out-of-bounds"));
	bPassed &= TArrayBindingsExecuteFunctionExpectingScriptException(
		Test,
		Engine,
		*Module,
		TArraySyntaxCompatProfile,
		TEXT("void TriggerSortUnsupportedText()"),
		TEXT("Array element type not sortable."),
		TEXT("FText[].Sort unsupported type guard"));

	return bPassed;
}

bool RunSyntaxNestedContainerRejectionSection(FAutomationTestBase& Test, FAngelscriptEngine& Engine)
{
	const FString Source = TEXT(R"(
int Entry()
{
	int[][] Nested;
	return Nested.Num();
}
)");

	const FString ModuleName = TArrayBindingsMakeModuleName(TArraySyntaxCompatProfile, TEXT("NestedContainerRejection"));
	const FString SourceFilename = FString::Printf(TEXT("%s.as"), *ModuleName);

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

	TArrayBindingsReportCompileSummaryDiagnostics(Test, TEXT("int[][] nested container rejection"), Summary);

	bool bPassed = true;
	FNoDiscardAsserter LocalAssert(Test);
	bPassed &= LocalAssert.IsFalse(
		bCompiled,
		TEXT("int[][] should fail compilation because nested containers are currently unsupported"));
	bPassed &= LocalAssert.AreEqual(
		ECompileResult::Error,
		Summary.CompileResult,
		TEXT("int[][] rejection should be a compile error"));
	if (Summary.Diagnostics.Num() > 0)
	{
		bPassed &= LocalAssert.IsTrue(
			TArrayBindingsCompileSummaryContainsDiagnosticMessage(Summary, TArraySyntaxCompatNestedContainerDiagnostic),
			TEXT("int[][] rejection should report nested container diagnostics when diagnostics are available"));
	}

	return bPassed;
}

bool RunObjectArraySyntaxBoundarySection(FAutomationTestBase& Test, FAngelscriptEngine& Engine)
{
	bool bPassed = true;
	bPassed &= TArraySyntaxCompatExpectCompileFailure(
		Test,
		Engine,
		TArraySyntaxCompatProfile,
		TEXT("UObjectArrayBoundary"),
		TEXT(R"(
int Entry()
{
	UObject[] Objects;
	return Objects.Num();
}
)"),
		TEXT("UObject[] local variable boundary"));
	bPassed &= TArraySyntaxCompatExpectCompileFailure(
		Test,
		Engine,
		TArraySyntaxCompatProfile,
		TEXT("AActorArrayBoundary"),
		TEXT(R"(
int Entry()
{
	AActor[] Actors;
	return Actors.Num();
}
)"),
		TEXT("AActor[] local variable boundary"));
	bPassed &= TArraySyntaxCompatExpectCompileFailure(
		Test,
		Engine,
		TArraySyntaxCompatProfile,
		TEXT("UActorComponentArrayBoundary"),
		TEXT(R"(
int Entry()
{
	UActorComponent[] Components;
	return Components.Num();
}
)"),
		TEXT("UActorComponent[] local variable boundary"));
	bPassed &= TArraySyntaxCompatExpectCompileFailure(
		Test,
		Engine,
		TArraySyntaxCompatProfile,
		TEXT("USceneComponentArrayBoundary"),
		TEXT(R"(
int Entry()
{
	USceneComponent[] Components;
	return Components.Num();
}
)"),
		TEXT("USceneComponent[] local variable boundary"));
	bPassed &= TArraySyntaxCompatExpectCompileFailure(
		Test,
		Engine,
		TArraySyntaxCompatProfile,
		TEXT("UClassArrayBoundary"),
		TEXT(R"(
int Entry()
{
	UClass[] Classes;
	return Classes.Num();
}
)"),
		TEXT("UClass[] local variable boundary"));

	return bPassed;
}

// ----------------------------------------------------------------------------
// Test class
// ----------------------------------------------------------------------------

TEST_CLASS_WITH_FLAGS(FAngelscriptTArraySyntaxCompatBindingsTest,
	"Angelscript.TestModule.Bindings.Container.TArraySyntaxCompat",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	BEFORE_ALL()
	{
		ASTEST_CREATE_ENGINE();
	}

	AFTER_ALL() { FAngelscriptEngine& Engine = ASTEST_GET_ENGINE(); ASTEST_RESET_ENGINE(Engine); }

	TEST_METHOD(TArraySyntaxCompat)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine);

		TestRunner->AddInfo(TEXT("TArraySyntaxCompat.MutationCompat: begin"));
		RunIntArrayMutationCompatSection(*TestRunner, Engine);

		TestRunner->AddInfo(TEXT("TArraySyntaxCompat.IteratorCompat: begin"));
		RunIntArrayIteratorCompatSection(*TestRunner, Engine);

		TestRunner->AddInfo(TEXT("TArraySyntaxCompat.Operations: begin"));
		RunIntArrayOperationsSection(*TestRunner, Engine);

		TestRunner->AddInfo(TEXT("TArraySyntaxCompat.TypeMatrix: begin"));
		RunSyntaxTypeMatrixSection(*TestRunner, Engine);

		TestRunner->AddInfo(TEXT("TArraySyntaxCompat.ReturnValues: begin"));
		RunSyntaxReturnValuesSection(*TestRunner, Engine);

		TestRunner->AddInfo(TEXT("TArraySyntaxCompat.ErrorPaths: begin"));
		RunSyntaxErrorPathsSection(*TestRunner, Engine);

		TestRunner->AddInfo(TEXT("TArraySyntaxCompat.NestedContainerRejection: begin"));
		RunSyntaxNestedContainerRejectionSection(*TestRunner, Engine);

		TestRunner->AddInfo(TEXT("TArraySyntaxCompat.ObjectArraySyntaxBoundary: begin"));
		RunObjectArraySyntaxBoundarySection(*TestRunner, Engine);

		}
	}
};

#endif
