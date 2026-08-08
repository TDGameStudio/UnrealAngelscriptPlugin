#include "Binds/Bind_TArray.h"

#include "AngelscriptBinds.h"
#include "AngelscriptDocs.h"
#include "AngelscriptEngine.h"
#include "AngelscriptSort.h"

#include "Helper_Reification.h"

#include "ClassGenerator/ASClass.h"

#include "Containers/ScriptArray.h"
#include "UObject/UnrealType.h"
//#include "UObject/GarbageCollectionSchema.h"
#include "UObject/GarbageCollection.h"

#include "StartAngelscriptHeaders.h"
//#include "as_context.h"
//#include "as_scriptengine.h"
//#include "as_scriptfunction.h"
#include "source/as_context.h"
#include "source/as_scriptengine.h"
#include "source/as_scriptfunction.h"
#include "EndAngelscriptHeaders.h"

/**
 * TArray template construction, indexed access, mutation, iteration, and allocation queries.
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | AngelScript usage signature                                                                          | Purpose / parameter notes                                                                                        |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | TArray<T> Array();                                                                                   | Constructs an empty array.                                                                                       |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | T& Value = Array[Index];                                                                             | Returns a mutable element reference.                                                                             |
 * |                                                                                                      | @param Index Zero-based element index; an invalid index raises a script exception.                               |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | const T& Value = Array[Index];                                                                       | Returns a const element reference.                                                                               |
 * |                                                                                                      | @param Index Zero-based element index; an invalid index raises a script exception.                               |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | Left = Right;                                                                                        | Copies every element from another array.                                                                         |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | bool bEqual = Left == Right;                                                                         | Compares array lengths and corresponding elements.                                                               |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | Array.Add(const T&in Value);                                                                         | Appends one element.                                                                                             |
 * |                                                                                                      | @param Value Element copied or referenced according to T.                                                        |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | Array.Append(const TArray<T>& Other);                                                                | Appends all elements from another array.                                                                         |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | Array.Shuffle();                                                                                     | Randomly permutes the elements.                                                                                  |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | Array.Swap(int32 FirstIndexToSwap, int32 SecondIndexToSwap);                                         | Exchanges two elements.                                                                                          |
 * |                                                                                                      | @param FirstIndexToSwap Zero-based index of the first element.                                                   |
 * |                                                                                                      | @param SecondIndexToSwap Zero-based index of the second element.                                                 |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | Array.MoveAssignFrom(TArray<T>& OtherArray);                                                         | Moves storage from another array and empties the source.                                                         |
 * |                                                                                                      | @param OtherArray Source whose allocation and elements are consumed.                                             |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | bool bValid = Array.IsValidIndex(int32 Index) const;                                                 | Reports whether an index addresses an existing element.                                                          |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | const T& Value = Array.Last(int32 IndexFromEnd = 0) const;                                           | Returns a const element counted backward from the last element.                                                  |
 * |                                                                                                      | @param IndexFromEnd Zero selects the last element.                                                               |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | T& Value = Array.Last(int32 IndexFromEnd = 0);                                                       | Returns a mutable element counted backward from the last element.                                                |
 * |                                                                                                      | @param IndexFromEnd Zero selects the last element.                                                               |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | Array.Insert(const T&in Value, int32 Index = 0);                                                     | Inserts an element and shifts following elements.                                                                |
 * |                                                                                                      | @param Value Element to insert.                                                                                  |
 * |                                                                                                      | @param Index Zero-based insertion position.                                                                      |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | bool bAdded = Array.AddUnique(const T&in Value);                                                     | Adds the value only when no equal element exists; returns whether insertion occurred.                            |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | Array.Empty(int32 ReservedSize = 0);                                                                 | Removes all elements and sets the requested slack.                                                               |
 * |                                                                                                      | @param ReservedSize Capacity to retain after clearing.                                                           |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | Array.Reset(int32 ReservedSize = 0);                                                                 | Removes all elements while reusing allocation when possible.                                                     |
 * |                                                                                                      | @param ReservedSize Minimum capacity to retain.                                                                  |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | Array.Reserve(int32 ReservedSize = 0);                                                               | Ensures capacity for at least the requested number of elements.                                                  |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | Array.SetNum(int32 NewNum = 0);                                                                      | Resizes the array, constructing or removing elements as needed.                                                  |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | Array.Copy(const TArray<T>& SourceArray, int32 SourceIndex, int32 Count, int TargetIndex = 0);       | Copies a range into this array.                                                                                  |
 * |                                                                                                      | @param SourceArray Array supplying elements.                                                                     |
 * |                                                                                                      | @param SourceIndex First source element.                                                                         |
 * |                                                                                                      | @param Count Number of elements to copy.                                                                         |
 * |                                                                                                      | @param TargetIndex First destination element.                                                                    |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | Array.SetNumZeroed(int32 NewNum = 0);                                                                | Resizes the array and zero-initializes newly added storage.                                                      |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | int32 Index = Array.FindIndex(const T&in Value) const;                                               | Returns the first matching index, or -1 when absent.                                                             |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | bool bContains = Array.Contains(const T&in Value) const;                                             | Reports whether an equal element exists.                                                                         |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | int Removed = Array.RemoveSingle(const T&in Value);                                                  | Removes the first matching element while preserving order.                                                       |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | int Removed = Array.Remove(const T&in Value);                                                        | Removes every matching element while preserving order.                                                           |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | int Removed = Array.RemoveSingleSwap(const T&in Value);                                              | Removes the first match by swapping from the end; order may change.                                              |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | int Removed = Array.RemoveSwap(const T&in Value);                                                    | Removes every match using swap removal; order may change.                                                        |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | Array.RemoveAt(int32 Index);                                                                         | Removes one indexed element while preserving order.                                                              |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | Array.RemoveAtSwap(int32 Index);                                                                     | Removes one indexed element by swapping from the end; order may change.                                          |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | Array.Sort(bool bDescendingOrder = false);                                                           | Sorts elements using T's comparison operator.                                                                    |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | int Count = Array.Num() const;                                                                       | Returns the number of elements.                                                                                  |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | int Capacity = Array.Max() const;                                                                    | Returns the allocated element capacity.                                                                          |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | int64 Bytes = Array.GetAllocatedSize() const;                                                        | Returns heap storage consumed by the array in bytes.                                                             |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | bool bEmpty = Array.IsEmpty() const;                                                                 | Reports whether the array has no elements.                                                                       |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | int Slack = Array.GetSlack() const;                                                                  | Returns unused allocated element capacity.                                                                       |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | for (T& Value : Array);                                                                              | Iterates mutable values through the compiler-facing array iteration protocol.                                    |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | for (const T& Value : Array);                                                                        | Iterates const values through the compiler-facing array iteration protocol.                                      |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | for (int Index, T& Value : Array);                                                                   | Iterates mutable values together with their zero-based indices.                                                  |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | for (int Index, const T& Value : Array);                                                             | Iterates const values together with their zero-based indices.                                                    |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | Array.Shrink();                                                                                      | Releases slack so capacity approaches the current element count.                                                 |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | TArrayIterator<T> Iterator(const TArrayIterator<T>& Other);                                          | Copy-constructs a mutable array iterator.                                                                        |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | Iterator = Other;                                                                                    | Assigns a mutable array iterator.                                                                                |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | bool Iterator.CanProceed;                                                                            | Reports whether the mutable iterator can produce another element.                                                |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | T& Value = Iterator.Proceed();                                                                       | Returns the current mutable element and advances the iterator.                                                   |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | TArrayConstIterator<T> ConstIterator(const TArrayConstIterator<T>& Other);                           | Copy-constructs a const array iterator.                                                                          |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | ConstIterator = Other;                                                                               | Assigns a const array iterator.                                                                                  |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | bool ConstIterator.CanProceed;                                                                       | Reports whether the const iterator can produce another element.                                                  |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | const T& Value = ConstIterator.Proceed();                                                            | Returns the current const element and advances the iterator.                                                     |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | TArrayIterator<T> Iterator = Array.Iterator();                                                       | Creates a mutable iterator for the array.                                                                        |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | TArrayConstIterator<T> Iterator = Array.Iterator() const;                                            | Creates a const iterator for the array.                                                                          |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 */

#if AS_ITERATOR_DEBUGGING
thread_local static TArray<void*, TInlineAllocator<16>> GArraysBeingIterated;

static bool CheckArrayIteratorDebug(FScriptArray& Array)
{
	if (GArraysBeingIterated.Contains(&Array))
	{
		FAngelscriptEngine::Throw("TArray is being modified during for loop iteration");
		return false;
	}

	return true;
}
#endif

#if AS_REFERENCE_DEBUGGING
static void InvalidateReferencesToArray(FScriptArray& Array, FArrayOperations* Ops)
{
	asCContext* Context = (asCContext*)asGetActiveContext();
	if (Context != nullptr)
	{
		Context->InvalidateReferencesToMemoryBlock(Array.GetData(), Array.GetAllocatedSize(Ops->NumBytesPerElement));
	}
}
#endif

static bool CheckArrayValueDoesNotAliasStorage(FScriptArray& Array, void* Value, FArrayOperations* Ops, const ANSICHAR* ErrorMessage)
{
	if (Value == nullptr || Array.GetData() == nullptr)
	{
		return true;
	}

	const UPTRINT LowerBound = reinterpret_cast<UPTRINT>(Array.GetData());
	const SIZE_T AllocatedSize = Array.GetAllocatedSize(Ops->NumBytesPerElement);
	const UPTRINT UpperBound = LowerBound + AllocatedSize;
	const UPTRINT ValueAddress = reinterpret_cast<UPTRINT>(Value);
	if (ValueAddress >= LowerBound && ValueAddress < UpperBound)
	{
		FAngelscriptEngine::Throw(ErrorMessage);
		return false;
	}

	return true;
}

template<int Size = 1>
struct FScriptArraySorter
{
	struct FElement
	{
		uint8 Data[Size];
	};

	bool bDescendingOrder = false;
	FArrayOperations* Ops;

	FScriptArraySorter(FArrayOperations* InOps, bool InDescendingOrder)
		: bDescendingOrder(InDescendingOrder)
		, Ops(InOps)
	{
	}

	bool operator()(const FElement& A, const FElement& B) const
	{
		const int32 Order = Ops->CompareFunction != nullptr
							? InvokeCompareFunction((void*)&A, (void*)&B) // We have an explicit compare function, use that.
							: Ops->Type.CompareOrder((void*)&A, (void*)&B); // Use the default CompareOrder for this type.

		return bDescendingOrder ? Order > 0 : Order < 0;
	}

	static int DynamicCompareFunction_NonObjectPointer(void* Context, void const* A, void const* B)
	{
		FScriptArraySorter<>* Sorter = static_cast<FScriptArraySorter<>*>(Context);
		const int32 DescendingOrderSign = Sorter->bDescendingOrder ? -1.0f : 1.0f;
		return Sorter->InvokeCompareFunction(const_cast<void*>(A), const_cast<void*>(B)) * DescendingOrderSign;
	}
	
	static int DynamicCompareFunction_ObjectPointer(void* Context, void const* A, void const* B)
	{
		FScriptArraySorter<>* Sorter = static_cast<FScriptArraySorter<>*>(Context);
		const int32 DescendingOrderSign = Sorter->bDescendingOrder ? -1.0f : 1.0f;

		void* AObjectPtr = *static_cast<void**>(const_cast<void*>(A));
		void* BObjectPtr = *static_cast<void**>(const_cast<void*>(B));

		int32 Result;
		if (AObjectPtr == nullptr || BObjectPtr == nullptr)
		{
			Result = AObjectPtr == nullptr ? -1 : 1;
		}
		else
		{
			Result = Sorter->InvokeCompareFunction(AObjectPtr, BObjectPtr);
		}

		return Result * DescendingOrderSign;
	}

	int32 InvokeCompareFunction(void* A, void* B) const
	{
		ensure(Ops->CompareFunction != nullptr);
		FAngelscriptContext Context(Ops->CompareFunction->GetEngine());
		if (!PrepareAngelscriptContextWithLog(Context, Ops->CompareFunction, TEXT("FScriptArraySorter::InvokeCompareFunction")))
		{
			return 0;
		}
		Context->SetObject(A);
		Context->SetArgAddress(0, B);
		Context->Execute();
		return Context->GetReturnDWord();
	}

	static void Sort(FArrayOperations* Ops, FScriptArray& Arr, bool bDescendingOrder)
	{
		TArrayRange<FElement> ArrayRange( (FElement*)Ops->Get(Arr, 0), Arr.Num() );
		Algo::Sort(ArrayRange, FScriptArraySorter<Size>(Ops, bDescendingOrder));
	}
};

FScriptArray& FArrayOperations::OpAssign(FScriptArray& DestinationArray, asCObjectType* Meta, FScriptArray& SourceArray)
{
#if AS_ITERATOR_DEBUGGING
	if (!CheckArrayIteratorDebug(DestinationArray))
		return DestinationArray;
#endif

	auto* Ops = GetArrayOperations(Meta);

#if AS_REFERENCE_DEBUGGING
	InvalidateReferencesToArray(DestinationArray, Ops);
#endif

	int32 ElementSize = Ops->NumBytesPerElement;
	int32 ElementAlignment = Ops->Alignment;
	int32 SourceNum = SourceArray.Num();
	int32 DestNum = DestinationArray.Num();
	if (!Ops->bNeedCopy)
	{
		// Totally POD-typed, so just do a direct copy instead of shenanigans
		if (SourceNum > DestNum)
			DestinationArray.Add(SourceNum - DestNum, ElementSize, ElementAlignment);
		else if(DestNum > SourceNum)
			DestinationArray.Remove(SourceNum, DestNum - SourceNum, ElementSize, ElementAlignment);
		FMemory::Memcpy(DestinationArray.GetData(), SourceArray.GetData(), SourceNum * ElementSize);
		return DestinationArray;
	}

	if (SourceNum > DestNum)
	{
		DestinationArray.Add(SourceNum - DestNum, ElementSize, ElementAlignment);

		if (Ops->bNeedConstruct)
		{
			for (int32 i = DestNum; i < SourceNum; ++i)
				Ops->Type.ConstructValue((void*)((SIZE_T)DestinationArray.GetData() + (i * ElementSize)));
		}
	}
	else if (DestNum > SourceNum)
	{
		if (Ops->bNeedDestruct)
		{
			for (int32 i = SourceNum; i < DestNum; ++i)
				Ops->Type.DestructValue((void*)((SIZE_T)DestinationArray.GetData() + (i * ElementSize)));
		}

		DestinationArray.Remove(SourceNum, DestNum - SourceNum, ElementSize, ElementAlignment);
	}

	for (int32 i = 0; i < SourceNum; ++i)
	{
		Ops->Type.CopyValue(
			(void*)((SIZE_T)SourceArray.GetData() + (i * ElementSize)),
			(void*)((SIZE_T)DestinationArray.GetData() + (i * ElementSize)));
	}

	return DestinationArray;
}

bool FArrayOperations::OpEquals(FScriptArray& ArrayA, asCObjectType* Meta, FScriptArray& ArrayB)
{
	auto* Ops = GetArrayOperations(Meta);
	if (!Ops->Type.CanCompare())
	{
		FAngelscriptEngine::Throw("Cannot compare element type for equality.");
		return false;
	}

	if (ArrayA.Num() != ArrayB.Num())
		return false;

	for (int32 i = 0, Count = ArrayA.Num(); i < Count; ++i)
	{
		if (!Ops->Type.IsValueEqual(Ops->Get(ArrayA, i), Ops->Get(ArrayB, i)))
			return false;
	}

	return true;
}

void FArrayOperations::Add(FScriptArray& Arr, asCObjectType* Meta, void* Value)
{
#if AS_ITERATOR_DEBUGGING
	if (!CheckArrayIteratorDebug(Arr))
		return;
#endif

	auto* Ops = GetArrayOperations(Meta);

	if (!CheckArrayValueDoesNotAliasStorage(Arr, Value, Ops, "Cannot Add an element from the same array by reference. Copy it to a temporary first."))
		return;

#if AS_REFERENCE_DEBUGGING
	InvalidateReferencesToArray(Arr, Ops);
#endif

	int32 AddIndex = Arr.Add(1, Ops->NumBytesPerElement, Ops->Alignment);
	void* DestinationAddr = Ops->Get(Arr, AddIndex);
	if (Ops->bNeedConstruct)
		Ops->Type.ConstructValue(DestinationAddr);

	if (Ops->bNeedCopy)
		Ops->Type.CopyValue(Value, DestinationAddr);
	else
		FMemory::Memcpy(DestinationAddr, Value, Ops->NumBytesPerElement);
}

void FArrayOperations::Append(FScriptArray& DestinationArray, asCObjectType* Meta, FScriptArray& SourceArray)
{
#if AS_ITERATOR_DEBUGGING
	if (!CheckArrayIteratorDebug(DestinationArray))
		return;
#endif

	auto* Ops = GetArrayOperations(Meta);

#if AS_REFERENCE_DEBUGGING
	InvalidateReferencesToArray(DestinationArray, Ops);
#endif

	int32 ElementSize = Ops->NumBytesPerElement;
	int32 SourceNum = SourceArray.Num();
	int32 DestNum = DestinationArray.Num();

	DestinationArray.Add(SourceNum, ElementSize, Ops->Alignment);

	if (!Ops->bNeedCopy)
	{
		// Totally POD-typed, so just do a direct copy instead of shenanigans
		FMemory::Memcpy(Ops->Get(DestinationArray, DestNum), SourceArray.GetData(), SourceNum * ElementSize);
	}
	else
	{
		for (int32 i = 0; i < SourceNum; ++i)
		{
			if (Ops->bNeedConstruct)
			{
				Ops->Type.ConstructValue(Ops->Get(DestinationArray, DestNum + i));
			}

			Ops->Type.CopyValue(
				Ops->Get(SourceArray, i),
				Ops->Get(DestinationArray, DestNum + i));
		}
	}
}

void FArrayOperations::Shuffle(FScriptArray& Array, asCObjectType* Meta)
{
#if AS_ITERATOR_DEBUGGING
	if (!CheckArrayIteratorDebug(Array))
		return;
#endif

	auto* Ops = GetArrayOperations(Meta);

#if AS_REFERENCE_DEBUGGING
	InvalidateReferencesToArray(Array, Ops);
#endif

	int32 ElementSize = Ops->NumBytesPerElement;
	int32 ArrayNum = Array.Num();

	int32 LastIndex = ArrayNum - 1;
	for (int32 i = 0; i <= LastIndex; ++i)
	{
		int32 Index = FMath::RandRange(i, LastIndex);
		if (i != Index)
		{
			FMemory::Memswap(
				Ops->Get(Array, i),
				Ops->Get(Array, Index),
				ElementSize
			);
		}
	}
}

void FArrayOperations::Swap(FScriptArray& Arr, asCObjectType* Meta, int32 FirstIndexToSwap, int32 SecondIndexToSwap)
{
#if AS_ITERATOR_DEBUGGING
	if (!CheckArrayIteratorDebug(Arr))
		return;
#endif

	auto* Ops = GetArrayOperations(Meta);

#if AS_REFERENCE_DEBUGGING
	InvalidateReferencesToArray(Arr, Ops);
#endif

	if (!Arr.IsValidIndex(FirstIndexToSwap) || !Arr.IsValidIndex(SecondIndexToSwap))
	{
		FAngelscriptEngine::Throw("Array index out of bounds.");
		return;
	}

	Arr.SwapMemory(FirstIndexToSwap, SecondIndexToSwap, Ops->NumBytesPerElement);
}

void FArrayOperations::MoveAssignFrom(FScriptArray& Arr, asCObjectType* Meta, FScriptArray& OtherArray)
{
#if AS_ITERATOR_DEBUGGING
	if (!CheckArrayIteratorDebug(Arr))
		return;
	if (!CheckArrayIteratorDebug(OtherArray))
		return;
#endif

	if (&OtherArray == &Arr)
	{
		FAngelscriptEngine::Throw("Cannot move assign an array into itself.");
		return;
	}

	auto* Ops = GetArrayOperations(Meta);

#if AS_REFERENCE_DEBUGGING
	InvalidateReferencesToArray(Arr, Ops);
	InvalidateReferencesToArray(OtherArray, Ops);
#endif

	Arr.MoveAssign(OtherArray, Ops->NumBytesPerElement, Ops->Alignment);
}

void FArrayOperations::Insert(FScriptArray& Arr, asCObjectType* Meta, void* Value, int32 Index)
{
#if AS_ITERATOR_DEBUGGING
	if (!CheckArrayIteratorDebug(Arr))
		return;
#endif

	if (Index > Arr.Num() || Index < 0)
	{
		FAngelscriptEngine::Throw("Array index out of bounds. Need to insert between 0 and ArraySize");
		return;
	}

	auto* Ops = GetArrayOperations(Meta);

	if (!CheckArrayValueDoesNotAliasStorage(Arr, Value, Ops, "Cannot Insert an element from the same array by reference. Copy it to a temporary first."))
		return;

#if AS_REFERENCE_DEBUGGING
	InvalidateReferencesToArray(Arr, Ops);
#endif

	const int32 ElementSize = Ops->NumBytesPerElement;
	Arr.Insert(Index, 1, ElementSize, Ops->Alignment);

	void* DestinationAddr = Ops->Get(Arr, Index);
	if (Ops->bNeedConstruct)
		Ops->Type.ConstructValue(DestinationAddr);

	if (Ops->bNeedCopy)
		Ops->Type.CopyValue(Value, DestinationAddr);
	else
		FMemory::Memcpy(DestinationAddr, Value, Ops->NumBytesPerElement);	
}

bool FArrayOperations::AddUnique(FScriptArray& Arr, asCObjectType* Meta, void* Value)
{
#if AS_ITERATOR_DEBUGGING
	if (!CheckArrayIteratorDebug(Arr))
		return false;
#endif

	auto* Ops = GetArrayOperations(Meta);

#if AS_REFERENCE_DEBUGGING
	InvalidateReferencesToArray(Arr, Ops);
#endif

	if (Ops->Type.CanCompare())
	{
		for (int32 i = 0, Num = Arr.Num(); i < Num; ++i)
		{
			if (Ops->Type.IsValueEqual(Ops->Get(Arr, i), Value))
				return false;
		}
	}
	else
	{
		FAngelscriptEngine::Throw("Cannot AddUnique, array element type cannot be compared for equality");
		return false;
	}

	int32 AddIndex = Arr.Add(1, Ops->NumBytesPerElement, Ops->Alignment);
	void* DestinationAddr = Ops->Get(Arr, AddIndex);
	if (Ops->bNeedConstruct)
		Ops->Type.ConstructValue(DestinationAddr);

	if (Ops->bNeedCopy)
		Ops->Type.CopyValue(Value, DestinationAddr);
	else
		FMemory::Memcpy(DestinationAddr, Value, Ops->NumBytesPerElement);

	return true;
}

void FArrayOperations::Empty(FScriptArray& Arr, asCObjectType* Meta, int32 ReservedSize)
{
#if AS_ITERATOR_DEBUGGING
	if (!CheckArrayIteratorDebug(Arr))
		return;
#endif

	auto* Ops = GetArrayOperations(Meta);

#if AS_REFERENCE_DEBUGGING
	InvalidateReferencesToArray(Arr, Ops);
#endif

	if (Ops->bNeedDestruct)
	{
		for (int32 i = 0, Num = Arr.Num(); i < Num; ++i)
			Ops->Type.DestructValue(Ops->Get(Arr, i));
	}
	Arr.Empty(ReservedSize, Ops->NumBytesPerElement, Ops->Alignment);
}

void FArrayOperations::Reset(FScriptArray& Arr, asCObjectType* Meta, int32 ReservedSize)
{
#if AS_ITERATOR_DEBUGGING
	if (!CheckArrayIteratorDebug(Arr))
		return;
#endif

	auto* Ops = GetArrayOperations(Meta);

#if AS_REFERENCE_DEBUGGING
	InvalidateReferencesToArray(Arr, Ops);
#endif

	if (Ops->bNeedDestruct)
	{
		for (int32 i = 0, Num = Arr.Num(); i < Num; ++i)
			Ops->Type.DestructValue(Ops->Get(Arr, i));
	}
	Arr.Empty(FMath::Max(Arr.GetSlack() + Arr.Num(), ReservedSize), Ops->NumBytesPerElement, Ops->Alignment);
}

void FArrayOperations::Reserve(FScriptArray& Arr, asCObjectType* Meta, int32 ReservedSize)
{
#if AS_ITERATOR_DEBUGGING
	if (!CheckArrayIteratorDebug(Arr))
		return;
#endif

	if (Arr.Num() > ReservedSize)
		ReservedSize = Arr.Num();

	auto* Ops = GetArrayOperations(Meta);

#if AS_REFERENCE_DEBUGGING
	InvalidateReferencesToArray(Arr, Ops);
#endif

	if (ReservedSize > Arr.Max())
	{
		const int32 ExistingNum = Arr.Num();
		const int32 ReserveDelta = ReservedSize - ExistingNum;
		Arr.Add(ReserveDelta, Ops->NumBytesPerElement, Ops->Alignment);
		Arr.Remove(ExistingNum, ReserveDelta, Ops->NumBytesPerElement, Ops->Alignment, EAllowShrinking::No);
	}
}

void FArrayOperations::SetNum(FScriptArray& Arr, asCObjectType* Meta, int32 NewNum)
{
#if AS_ITERATOR_DEBUGGING
	if (!CheckArrayIteratorDebug(Arr))
		return;
#endif

	if (NewNum < 0)
	{
		FAngelscriptEngine::Throw("Invalid negative Num");
		return;
	}

	auto* Ops = GetArrayOperations(Meta);

#if AS_REFERENCE_DEBUGGING
	InvalidateReferencesToArray(Arr, Ops);
#endif

	// Destruct values we no longer have
	if (Ops->bNeedDestruct && NewNum < Arr.Num())
	{
		for (int32 i = NewNum, Num = Arr.Num(); i < Num; ++i)
			Ops->Type.DestructValue(Ops->Get(Arr, i));
	}

	int32 PrevNum = Arr.Num();
	//Arr.ResizeTo(NewNum, Ops->NumBytesPerElement, Ops->Alignment);
	//Arr.ArrayNum = NewNum;
	Arr.SetNumUninitialized(NewNum, Ops->NumBytesPerElement, Ops->Alignment);

	// Default construct values we added
	if (Ops->bNeedConstruct && NewNum > PrevNum)
	{
		for (int32 i = PrevNum; i < NewNum; ++i)
			Ops->Type.ConstructValue(Ops->Get(Arr, i));
	}
	else if (NewNum > PrevNum)
	{
		FMemory::Memzero(Ops->Get(Arr, PrevNum), (NewNum - PrevNum) * Ops->NumBytesPerElement);
	}
}

void FArrayOperations::ThrowOutOfBounds()
{
	FAngelscriptEngine::Throw("Array index out of bounds.");
}

void FArrayOperations::Copy(FScriptArray& Arr, asCObjectType* Meta, FScriptArray& SourceArray, int32 SourceIndex, int32 Count, int32 TargetIndex)
{
#if AS_ITERATOR_DEBUGGING
	if (!CheckArrayIteratorDebug(Arr))
		return;
#endif

	if (&SourceArray == &Arr)
	{
		FAngelscriptEngine::Throw("Cannot copy an array into itself.");
		return;
	}

	if (Count < 0)
	{
		FAngelscriptEngine::Throw("Count should not be negative.");
		return;
	}

	//if (SourceIndex < 0 || (SourceIndex + Count) > SourceArray.ArrayNum)
	if (SourceIndex < 0 || (SourceIndex + Count) > SourceArray.Num())
	{
		FAngelscriptEngine::Throw("Source array out of bounds.");
		return;
	}

	//if (TargetIndex < 0 || (TargetIndex + Count) > Arr.ArrayNum)
	if (TargetIndex < 0 || (TargetIndex + Count) > Arr.Num())
	{
		FAngelscriptEngine::Throw("Target array out of bounds.");
		return;
	}

	auto* Ops = GetArrayOperations(Meta);

#if AS_REFERENCE_DEBUGGING
	InvalidateReferencesToArray(Arr, Ops);
#endif

	for (int32 i = 0; i < Count; ++i)
	{
		auto SourceAddr = Ops->Get(SourceArray, i + SourceIndex);
		auto DestinationAddr = Ops->Get(Arr, i + TargetIndex);

		if (Ops->bNeedCopy)
			Ops->Type.CopyValue(SourceAddr, DestinationAddr);
		else
			FMemory::Memcpy(DestinationAddr, SourceAddr, Ops->NumBytesPerElement);
	}
}

void FArrayOperations::SetNumZeroed(FScriptArray& Arr, asCObjectType* Meta, int32 NewNum)
{
#if AS_ITERATOR_DEBUGGING
	if (!CheckArrayIteratorDebug(Arr))
		return;
#endif

	if (NewNum < 0)
	{
		FAngelscriptEngine::Throw("Invalid negative Num");
		return;
	}

	auto* Ops = GetArrayOperations(Meta);

#if AS_REFERENCE_DEBUGGING
	InvalidateReferencesToArray(Arr, Ops);
#endif

	if ((Ops->bNeedDestruct || Ops->bNeedConstruct) && !Ops->bIsObjectPointer)
	{
		FAngelscriptEngine::Throw("SetNumZeroed is not valid for arrays of non-primitive types.");
		return;
	}

	int32 PrevNum = Arr.Num();
	//Arr.ResizeTo(NewNum, Ops->NumBytesPerElement, Ops->Alignment);	
	//Arr.ArrayNum = NewNum;
	Arr.SetNumUninitialized(NewNum, Ops->NumBytesPerElement, Ops->Alignment);

	if(NewNum > PrevNum)
		FMemory::Memzero(Ops->Get(Arr, PrevNum), (NewNum - PrevNum) * Ops->NumBytesPerElement);
}

int32 FArrayOperations::RemoveSingle(FScriptArray& Arr, asCObjectType* Meta, void* Value)
{
#if AS_ITERATOR_DEBUGGING
	if (!CheckArrayIteratorDebug(Arr))
		return 0;
#endif

	auto* Ops = GetArrayOperations(Meta);

#if AS_REFERENCE_DEBUGGING
	InvalidateReferencesToArray(Arr, Ops);
#endif

	if (!Ops->Type.CanCompare())
	{
		FAngelscriptEngine::Throw("Cannot RemoveSingle, array element type cannot be compared for equality");
		return 0;
	}

	for (int32 i = 0, Num = Arr.Num(); i < Num; ++i)
	{
		if (Ops->Type.IsValueEqual(Ops->Get(Arr, i), Value))
		{
			Ops->Type.DestructValue(Ops->Get(Arr, i));
			Arr.Remove(i, 1, Ops->NumBytesPerElement, Ops->Alignment);
			return 1;
		}
	}
	return 0;
}

int32 FArrayOperations::Remove(FScriptArray& Arr, asCObjectType* Meta, void* Value)
{
#if AS_ITERATOR_DEBUGGING
	if (!CheckArrayIteratorDebug(Arr))
		return 0;
#endif

	auto* Ops = GetArrayOperations(Meta);

#if AS_REFERENCE_DEBUGGING
	InvalidateReferencesToArray(Arr, Ops);
#endif

	if (!Ops->Type.CanCompare())
	{
		FAngelscriptEngine::Throw("Cannot Remove, array element type cannot be compared for equality");
		return 0;
	}

	int NumRemoved = 0;
	for (int32 i = Arr.Num() - 1; i >= 0; --i)
	{
		if (Ops->Type.IsValueEqual(Ops->Get(Arr, i), Value))
		{
			Ops->Type.DestructValue(Ops->Get(Arr, i));
			Arr.Remove(i, 1, Ops->NumBytesPerElement, Ops->Alignment);
			NumRemoved++;
		}
	}
	return NumRemoved;
}

int32 FArrayOperations::RemoveSingleSwap(FScriptArray& Arr, asCObjectType* Meta, void* Value)
{
#if AS_ITERATOR_DEBUGGING
	if (!CheckArrayIteratorDebug(Arr))
		return 0;
#endif

	auto* Ops = GetArrayOperations(Meta);

#if AS_REFERENCE_DEBUGGING
	InvalidateReferencesToArray(Arr, Ops);
#endif

	if (!Ops->Type.CanCompare())
	{
		FAngelscriptEngine::Throw("Cannot RemoveSingleSwap, array element type cannot be compared for equality");
		return 0;
	}

	for (int32 i = 0, Num = Arr.Num(); i < Num; ++i)
	{
		if (Ops->Type.IsValueEqual(Ops->Get(Arr, i), Value))
		{
			Ops->Type.DestructValue(Ops->Get(Arr, i));

			int32 SwapWith = Arr.Num() - 1;
			if (SwapWith != i)
				FMemory::Memcpy(Ops->Get(Arr, i), Ops->Get(Arr, SwapWith), Ops->NumBytesPerElement);
			Arr.Remove(SwapWith, 1, Ops->NumBytesPerElement, Ops->Alignment);
			return 1;
		}
	}
	return 0;
}

int32 FArrayOperations::RemoveSwap(FScriptArray& Arr, asCObjectType* Meta, void* Value)
{
#if AS_ITERATOR_DEBUGGING
	if (!CheckArrayIteratorDebug(Arr))
		return 0;
#endif

	auto* Ops = GetArrayOperations(Meta);

#if AS_REFERENCE_DEBUGGING
	InvalidateReferencesToArray(Arr, Ops);
#endif

	if (!Ops->Type.CanCompare())
	{
		FAngelscriptEngine::Throw("Cannot RemoveSwap, array element type cannot be compared for equality");
		return 0;
	}

	int32 NumRemoved = 0;
	for (int32 i = 0, Num = Arr.Num(); i < Num; ++i)
	{
		if (Ops->Type.IsValueEqual(Ops->Get(Arr, i), Value))
		{
			Ops->Type.DestructValue(Ops->Get(Arr, i));

			int32 SwapWith = Arr.Num() - 1;
			if (SwapWith != i)
				FMemory::Memcpy(Ops->Get(Arr, i), Ops->Get(Arr, SwapWith), Ops->NumBytesPerElement);
			Arr.Remove(SwapWith, 1, Ops->NumBytesPerElement, Ops->Alignment);
			++NumRemoved;

			Num--;
			i--;
		}
	}
	return NumRemoved;
}

void FArrayOperations::RemoveAt(FScriptArray& Arr, asCObjectType* Meta, int32 Index)
{
#if AS_ITERATOR_DEBUGGING
	if (!CheckArrayIteratorDebug(Arr))
		return;
#endif

	auto* Ops = GetArrayOperations(Meta);

#if AS_REFERENCE_DEBUGGING
	InvalidateReferencesToArray(Arr, Ops);
#endif

	if (!Arr.IsValidIndex(Index))
	{
		FAngelscriptEngine::Throw("Array index out of bounds.");
		return;
	}

	if (Ops->bNeedDestruct)
	{
		Ops->Type.DestructValue(Ops->Get(Arr, Index));
	}

	Arr.Remove(Index, 1, Ops->NumBytesPerElement, Ops->Alignment);
}

void FArrayOperations::RemoveAtSwap(FScriptArray& Arr, asCObjectType* Meta, int32 Index)
{
#if AS_ITERATOR_DEBUGGING
	if (!CheckArrayIteratorDebug(Arr))
		return;
#endif

	auto* Ops = GetArrayOperations(Meta);

#if AS_REFERENCE_DEBUGGING
	InvalidateReferencesToArray(Arr, Ops);
#endif

	if (!Arr.IsValidIndex(Index))
	{
		FAngelscriptEngine::Throw("Array index out of bounds.");
		return;
	}

	if (Ops->bNeedDestruct)
	{
		Ops->Type.DestructValue(Ops->Get(Arr, Index));
	}

	int32 SwapWith = Arr.Num() - 1;
	if (SwapWith != Index)
		FMemory::Memcpy(Ops->Get(Arr, Index), Ops->Get(Arr, SwapWith), Ops->NumBytesPerElement);
	Arr.Remove(SwapWith, 1, Ops->NumBytesPerElement, Ops->Alignment);
}

void FArrayOperations::Sort(FScriptArray& Arr, asCObjectType* Meta, bool bDescendingOrder)
{
#if AS_ITERATOR_DEBUGGING
	if (!CheckArrayIteratorDebug(Arr))
		return;
#endif

	auto* Ops = GetArrayOperations(Meta);
	const int32 Size = Ops->NumBytesPerElement;

#if AS_REFERENCE_DEBUGGING
	InvalidateReferencesToArray(Arr, Ops);
#endif

	if (Ops->CompareFunction != nullptr)
	{
		FScriptArraySorter<> Sorter(Ops, bDescendingOrder);
		if (Ops->bIsObjectPointer)
		{
			AngelscriptSort::QuickSort<FScriptArraySorter<>::DynamicCompareFunction_ObjectPointer>(Arr.GetData(), Arr.Num(), Size, &Sorter);
		}
		else
		{
			AngelscriptSort::QuickSort<FScriptArraySorter<>::DynamicCompareFunction_NonObjectPointer>(Arr.GetData(), Arr.Num(), Size, &Sorter);
		}
		return;
	}
	
	if (!Ops->Type.IsOrdered())
	{
		const FString ParamType = Ops->bIsObjectPointer ? TEXT("{0}") : TEXT("const {0}&");
		const FString Parameter = FString::Format(*ParamType, { *Ops->Type.GetFriendlyTypeName() });
		const FString FunctionDecl = FString::Format(TEXT("int opCmp({0}) const"), { Parameter });

		const FString Error = FString::Format(TEXT("Array element type not sortable. To sort TArray<{0}>, {1} needs to be implemented."), { *Ops->Type.GetFriendlyTypeName(), FunctionDecl });
		FAngelscriptEngine::Throw(TCHAR_TO_ANSI(*Error));
		return;
	}

	if (Size == 1)
		FScriptArraySorter<1>::Sort(Ops, Arr, bDescendingOrder);
	else if (Size == 2)
		FScriptArraySorter<2>::Sort(Ops, Arr, bDescendingOrder);
	else if (Size == 4)
		FScriptArraySorter<4>::Sort(Ops, Arr, bDescendingOrder);
	else if (Size == 8)
		FScriptArraySorter<8>::Sort(Ops, Arr, bDescendingOrder);
	else if (Size == 12)
		FScriptArraySorter<12>::Sort(Ops, Arr, bDescendingOrder);
	else if (Size == 16)
		FScriptArraySorter<16>::Sort(Ops, Arr, bDescendingOrder);
	else
		FAngelscriptEngine::Throw("Array element is too large to sort.");
}

void FArrayOperations::Shrink(FScriptArray& Arr, asCObjectType* Meta)
{
	auto* Ops = GetArrayOperations(Meta);
	return Arr.Shrink(Ops->NumBytesPerElement, Ops->Alignment);
}

namespace
{
	bool ValidateArrayTemplate(asITypeInfo* TemplateType, asCString* ErrorMessage)
	{
		// Allow generic template subtypes.
		if (TemplateType->GetSubType(0) != nullptr
			&& (TemplateType->GetSubType(0)->GetFlags() & asOBJ_TEMPLATE_SUBTYPE) != 0)
		{
			return true;
		}

		return ValidateArrayOperations(TemplateType, ErrorMessage) != nullptr;
	}

	struct FAngelscriptArrayIterationBinds
	{
		static int32 ForBegin(FScriptArray& Array)
		{
			return Array.Num() > 0 ? 0 : -1;
		}

		static bool ForEnd(FScriptArray&, int32 Iterator)
		{
			return Iterator == -1;
		}

		static void ForNext(FScriptArray& Array, int32& Iterator)
		{
			if (Iterator == -1)
			{
				return;
			}

			const int32 NextIndex = Iterator + 1;
			Iterator = NextIndex < Array.Num() ? NextIndex : -1;
		}

		static void* ForValue(FScriptArray& Array, asCObjectType* Meta, int32 Iterator)
		{
			auto* Ops = FArrayOperations::GetArrayOperations(Meta);
			if (!Array.IsValidIndex(Iterator))
			{
				FAngelscriptEngine::Throw("Iterator out of bounds.");
				return nullptr;
			}

			return Ops->Get(Array, Iterator);
		}

		static int32 ForKey(FScriptArray&, int32 Iterator)
		{
			return Iterator;
		}
	};
}

static void BindTArrayTypeDeclarations(FAngelscriptBinds& Binds)
{
	FBindFlags Flags;
	Flags.bTemplate = true;
	Flags.TemplateType = "<T>";
	Flags.ExtraFlags = asOBJ_TEMPLATE_SUBTYPE_COVARIANT;
	Binds.ValueClassForTarget<FScriptArray>("TArray<class T>", Flags);

	FBindFlags IteratorFlags;
	IteratorFlags.bTemplate = true;
	IteratorFlags.TemplateType = "<T>";
	Binds.ValueClassForTarget<FArrayIterator>("TArrayIterator<class T>", IteratorFlags);
	Binds.ValueClassForTarget<FArrayIterator>("TArrayConstIterator<class T>", IteratorFlags);
}

static void BindTArrayMethodSurface(FAngelscriptBinds& Binds)
{
	auto TArray_ = Binds.ExistingClassForTarget("TArray<T>");
	TArray_.Constructor("void f()", FUNC_TRIVIAL(FArrayOperations::Construct));

	TArray_.Destructor("void f()", &FArrayOperations::Destruct)
		.PassScriptObjectTypeAsFirstParam()
		.NativeTemplateInstantiatedCall("FArrayOperations::Destruct", false, false, false);

	TArray_.TemplateCallback(
		"bool f(int&in Type, int&out ErrorMessage)",
		&ValidateArrayTemplate);

	TArray_.Method("T& opIndex(int __any_implicit_integer Index)", &FArrayOperations::OpIndex)
		.PassScriptObjectTypeAsFirstParam()
		.NativeTArrayIndex();

	TArray_.Method("const T& opIndex(int __any_implicit_integer Index) const", &FArrayOperations::OpIndex)
		.PassScriptObjectTypeAsFirstParam()
		.NativeTArrayIndex();

	TArray_.Method("TArray<T>& opAssign(const TArray<T>& Other)", &FArrayOperations::OpAssign)
		.PassScriptObjectTypeAsFirstParam()
		.NativeTemplateInstantiatedCall("FArrayOperations::OpAssign", false, false, true);

	TArray_.Method("bool opEquals(const TArray<T>& Other) const", &FArrayOperations::OpEquals)
		.PassScriptObjectTypeAsFirstParam()
		.NativeTemplateInstantiatedCall("FArrayOperations::OpEquals", false, true, false);

	TArray_.Method("void Add(const T&in if_handle_then_const Value)", &FArrayOperations::Add)
		.PassScriptObjectTypeAsFirstParam()
		.NativeTemplateInstantiatedCall("FArrayOperations::Add", false, false, true);

	TArray_.Method("void Append(const TArray<T>& Other)", &FArrayOperations::Append)
		.PassScriptObjectTypeAsFirstParam()
		.NativeTemplateInstantiatedCall("FArrayOperations::Append", false, false, true);

	TArray_.Method("void Shuffle()", FUNC(FArrayOperations::Shuffle)).PassScriptObjectTypeAsFirstParam();

	TArray_.Method(
		"void Swap(int32 __any_implicit_integer FirstIndexToSwap, int32 __any_implicit_integer SecondIndexToSwap)",
		&FArrayOperations::Swap)
		.PassScriptObjectTypeAsFirstParam()
		.NativeTemplateInstantiatedCall("FArrayOperations::Swap", true, false, true)
		.Documentation(TEXT("Swap the element at index FirstIndexToSwap with the element at index SecondIndexToSwap.\n"));

	TArray_.Method("void MoveAssignFrom(TArray<T>& OtherArray)", &FArrayOperations::MoveAssignFrom)
		.PassScriptObjectTypeAsFirstParam()
		.NativeTemplateInstantiatedCall("FArrayOperations::MoveAssignFrom", true, false, false)
		.Documentation(TEXT(
			"Perform a move-assign from the passed in array into this array.\n"
			"The passed in array will be emptied in the process as its memory is moved over."));

	TArray_.Method("bool IsValidIndex(int32 __any_implicit_integer Index) const", FUNC_TRIVIAL(FArrayOperations::IsValidIndex));

	TArray_.Method("const T& Last(int32 __any_implicit_integer IndexFromEnd = 0) const", &FArrayOperations::Last)
		.PassScriptObjectTypeAsFirstParam()
		.NativeTemplateInstantiatedCall("FArrayOperations::Last", false, false, false);

	TArray_.Method("T& Last(int32 __any_implicit_integer IndexFromEnd = 0)", &FArrayOperations::Last)
		.PassScriptObjectTypeAsFirstParam()
		.NativeTemplateInstantiatedCall("FArrayOperations::Last", false, false, false);

	TArray_.Method("void Insert(const T&in if_handle_then_const Value, int32 __any_implicit_integer Index = 0)", &FArrayOperations::Insert)
		.PassScriptObjectTypeAsFirstParam()
		.NativeTemplateInstantiatedCall("FArrayOperations::Insert", false, false, true);
	
	TArray_.Method("bool AddUnique(const T&in if_handle_then_const Value)", &FArrayOperations::AddUnique)
		.PassScriptObjectTypeAsFirstParam()
		.NativeTemplateInstantiatedCall("FArrayOperations::AddUnique", false, true, true)
		.Documentation(TEXT(
			"Will first do a check if the object already is in the array.\n"
			"Returns 'True' if the object is added.\n"));

	TArray_.Method("void Empty(int32 __any_implicit_integer ReservedSize = 0)", &FArrayOperations::Empty)
		.PassScriptObjectTypeAsFirstParam()
		.NativeTemplateInstantiatedCall("FArrayOperations::Empty", false, false, false);

	TArray_.Method("void Reset(int32 __any_implicit_integer ReservedSize = 0)", &FArrayOperations::Reset)
		.PassScriptObjectTypeAsFirstParam()
		.NativeTemplateInstantiatedCall("FArrayOperations::Reset", false, false, false);

	TArray_.Method("void Reserve(int32 __any_implicit_integer ReservedSize = 0)", &FArrayOperations::Reserve)
		.PassScriptObjectTypeAsFirstParam()
		.NativeTemplateInstantiatedCall("FArrayOperations::Reserve", false, false, false);

	TArray_.Method("void SetNum(int32 __any_implicit_integer NewNum = 0)", FUNC(FArrayOperations::SetNum))
		.PassScriptObjectTypeAsFirstParam();

	TArray_.Method(
		"void Copy(const TArray<T>& SourceArray, int32 __any_implicit_integer SourceIndex, int32 __any_implicit_integer Count, int __any_implicit_integer TargetIndex = 0)",
		FUNC(FArrayOperations::Copy))
		.PassScriptObjectTypeAsFirstParam();

	TArray_.Method("void SetNumZeroed(int32 __any_implicit_integer NewNum = 0)", FUNC(FArrayOperations::SetNumZeroed))
		.PassScriptObjectTypeAsFirstParam();

	TArray_.Method("int32 FindIndex(const T&in if_handle_then_const Value) const", &FArrayOperations::FindIndex)
		.PassScriptObjectTypeAsFirstParam()
		.NativeTemplateInstantiatedCall("FArrayOperations::FindIndex", false, true, false)
		.Documentation(TEXT(
			"Find the first index that contains an element with the given value.\n"
			"If no element matches the value, it will return -1."));

	TArray_.Method("bool Contains(const T&in if_handle_then_const Value) const", &FArrayOperations::Contains)
		.PassScriptObjectTypeAsFirstParam()
		.NativeTemplateInstantiatedCall("FArrayOperations::Contains", false, true, false);

	TArray_.Method("int RemoveSingle(const T&in if_handle_then_const Value)", &FArrayOperations::RemoveSingle)
		.PassScriptObjectTypeAsFirstParam()
		.NativeTemplateInstantiatedCall("FArrayOperations::RemoveSingle", false, true, false);

	TArray_.Method("int Remove(const T&in if_handle_then_const Value)", &FArrayOperations::Remove)
		.PassScriptObjectTypeAsFirstParam()
		.NativeTemplateInstantiatedCall("FArrayOperations::Remove", false, true, false);

	TArray_.Method("int RemoveSingleSwap(const T&in if_handle_then_const Value)", &FArrayOperations::RemoveSingleSwap)
		.PassScriptObjectTypeAsFirstParam()
		.NativeTemplateInstantiatedCall("FArrayOperations::RemoveSingleSwap", false, true, false);
	
	TArray_.Method("int RemoveSwap(const T&in if_handle_then_const Value)", &FArrayOperations::RemoveSwap)
		.PassScriptObjectTypeAsFirstParam()
		.NativeTemplateInstantiatedCall("FArrayOperations::RemoveSwap", false, true, false);

	TArray_.Method("void RemoveAt(int32 __any_implicit_integer Index)", &FArrayOperations::RemoveAt)
		.PassScriptObjectTypeAsFirstParam()
		.NativeTemplateInstantiatedCall("FArrayOperations::RemoveAt", false, false, true);

	TArray_.Method("void RemoveAtSwap(int32 __any_implicit_integer Index)", &FArrayOperations::RemoveAtSwap)
		.PassScriptObjectTypeAsFirstParam()
		.NativeTemplateInstantiatedCall("FArrayOperations::RemoveAtSwap", false, false, true);

	TArray_.Method("void Sort(bool bDescendingOrder = false)", FUNC(FArrayOperations::Sort))
		.PassScriptObjectTypeAsFirstParam();

	TArray_.Method("int Num() const", FUNC_TRIVIAL(FArrayOperations::Num));
	TArray_.Method("int Max() const", FUNC_TRIVIAL(FArrayOperations::Max));

	TArray_.Method("int64 GetAllocatedSize() const", &FArrayOperations::GetAllocatedSize)
		.PassScriptObjectTypeAsFirstParam()
		.NativeTemplateInstantiatedCall("FArrayOperations::GetAllocatedSize", true, false, false);

	TArray_.Method("bool IsEmpty() const", FUNC_TRIVIAL(FArrayOperations::IsEmpty));
	TArray_.Method("int GetSlack() const", FUNC_TRIVIAL(FArrayOperations::GetSlack));

	TArray_.Method("int opForBegin()", &FAngelscriptArrayIterationBinds::ForBegin);
	TArray_.Method("int opForBegin() const", &FAngelscriptArrayIterationBinds::ForBegin);
	TArray_.Method("bool opForEnd(const int Iterator) const", &FAngelscriptArrayIterationBinds::ForEnd);
	TArray_.Method("void opForNext(int&inout Iterator)", &FAngelscriptArrayIterationBinds::ForNext);
	TArray_.Method("void opForNext(int&inout Iterator) const", &FAngelscriptArrayIterationBinds::ForNext);
	TArray_.Method("T& opForValue(const int Iterator)", &FAngelscriptArrayIterationBinds::ForValue)
		.PassScriptObjectTypeAsFirstParam();
	TArray_.Method("const T& opForValue(const int Iterator) const", &FAngelscriptArrayIterationBinds::ForValue)
		.PassScriptObjectTypeAsFirstParam();
	TArray_.Method("int opForKey(const int Iterator) const", &FAngelscriptArrayIterationBinds::ForKey);

	TArray_.Method("void Shrink()", &FArrayOperations::Shrink)
		.PassScriptObjectTypeAsFirstParam()
		.NativeTemplateInstantiatedCall("FArrayOperations::Shrink", true, false, false);

	auto TArrayIterator_ = Binds.ExistingClassForTarget("TArrayIterator<T>");
#if AS_ITERATOR_DEBUGGING
	TArrayIterator_.Destructor("void f()", &FArrayIterator::Destruct);
#endif

	TArrayIterator_.Constructor("void f(const TArrayIterator<T>& Other)", &FArrayIterator::CopyConstruct);
	TArrayIterator_.Method("TArrayIterator<T>& opAssign(const TArrayIterator<T>& Other)", &FArrayIterator::Assignment);
	TArrayIterator_.Property("bool CanProceed", &FArrayIterator::bCanProceed);

	TArrayIterator_.Method("T& Proceed()", &FArrayIterator::Proceed).NativeTArrayIteratorProceed();

	auto TArrayConstIterator_ = Binds.ExistingClassForTarget("TArrayConstIterator<T>");
#if AS_ITERATOR_DEBUGGING
	TArrayConstIterator_.Destructor("void f()", &FArrayIterator::Destruct);
#endif

	TArrayConstIterator_.Constructor("void f(const TArrayConstIterator<T>& Other)", &FArrayIterator::CopyConstruct);
	TArrayConstIterator_.Method("TArrayConstIterator<T>& opAssign(const TArrayConstIterator<T>& Other)", &FArrayIterator::Assignment);
	TArrayConstIterator_.Property("bool CanProceed", &FArrayIterator::bCanProceed);

	TArrayConstIterator_.Method("const T& Proceed()", &FArrayIterator::Proceed).NativeTArrayIteratorProceed();

	TArray_.Method("TArrayIterator<T> Iterator()", &FArrayIterator::Create)
		.PassScriptObjectTypeAsFirstParam()
		.NativeTArrayIteratorCreate();

	TArray_.Method("TArrayConstIterator<T> Iterator() const", &FArrayIterator::Create)
		.PassScriptObjectTypeAsFirstParam()
		.NativeTArrayIteratorCreate();
}

static void BindTArrayTypeInfrastructure(FAngelscriptBinds& Binds)
{
	Binds.GetTargetTypeDatabase().ArrayTemplateTypeInfo = Binds.GetTargetScriptEngine().GetTypeInfoByName("TArray");
	Binds.GetTargetScriptEngine().RegisterDefaultArrayType("TArray<T>");

	auto ArrayType = MakeShared<FAngelscriptArrayType>();
	Binds.RegisterTypeForTarget(ArrayType);

	FAngelscriptTypeDatabase* TargetTypeDatabase = &Binds.GetTargetTypeDatabase();
	Binds.RegisterTypeFinderForTarget([ArrayType, TargetTypeDatabase](FProperty* Property, FAngelscriptTypeUsage& Usage) -> bool
	{
		FArrayProperty* ArrayProperty = CastField<FArrayProperty>(Property);
		if (ArrayProperty == nullptr)
		{
			return false;
		}

		FAngelscriptTypeUsage InnerUsage = FAngelscriptTypeUsage::FromProperty(*TargetTypeDatabase, ArrayProperty->Inner);
		if (!InnerUsage.IsValid())
		{
			return false;
		}

		Usage.Type = ArrayType;
		Usage.SubTypes.Add(InnerUsage);
		return true;
	});

	Binds.RegisterTypeForTarget(MakeShared<FAngelscriptArrayIteratorType>());
	Binds.RegisterTypeForTarget(MakeShared<FAngelscriptArrayConstIteratorType>());
}

AS_FORCE_LINK const FAngelscriptBind Bind_TArray_TypeDeclarations(
	TEXT("TArray.Declaration"),
	EAngelscriptBindPhase::TypeDeclarations,
	&BindTArrayTypeDeclarations);

AS_FORCE_LINK const FAngelscriptBind Bind_TArray_MethodSurface(
	TEXT("TArray.MethodSurface"),
	EAngelscriptBindPhase::TypeInfrastructure,
	&BindTArrayMethodSurface);

AS_FORCE_LINK const FAngelscriptBind Bind_TArray_TypeInfrastructure(
	TEXT("TArray.TypeInfrastructure"),
	EAngelscriptBindPhase::TypeInfrastructure,
	&BindTArrayTypeInfrastructure);

void FArrayIterator::CopyConstruct(FArrayIterator& Iterator, const FArrayIterator& Other)
{
#if AS_ITERATOR_DEBUGGING
	if (Other.Array != nullptr)
		GArraysBeingIterated.Add(Other.Array);
#endif

	Iterator = Other;
}

FArrayIterator& FArrayIterator::Assignment(const FArrayIterator& Other)
{
#if AS_ITERATOR_DEBUGGING
	if (Array != nullptr)
		GArraysBeingIterated.RemoveSingle(Array);
	if (Other.Array != nullptr)
		GArraysBeingIterated.Add(Other.Array);
#endif

	*this = Other;
	return *this;
}

void FArrayIterator::Destruct(FArrayIterator& Iterator)
{
	// OBS: This does not get bound at all when iterator debugging is turned off,
	// so don't use it for anything else.
#if AS_ITERATOR_DEBUGGING
	if (Iterator.Array != nullptr)
		GArraysBeingIterated.RemoveSingle(Iterator.Array);
#endif
}

FArrayIterator FArrayIterator::Create(FScriptArray& Array, asCObjectType* Meta)
{
	auto* Ops = FArrayOperations::GetArrayOperations(Meta);

	FArrayIterator It;
	It.Array = &Array;
	It.Stride = (uint32)Ops->NumBytesPerElement;
	It.Index = 0;
	It.bCanProceed = Array.Num() > 0;

#if AS_ITERATOR_DEBUGGING
	GArraysBeingIterated.Add(&Array);
#endif

	return It;
}

FArrayIterator FArrayIterator::CreateForEach(FScriptArray& Array, asCObjectType* Meta)
{
	return Create(Array, Meta);
}

void* FArrayIterator::Proceed()
{
	uint32 Num = (uint32)Array->Num();
	if (Index >= Num)
	{
		FAngelscriptEngine::Throw("Iterator out of bounds.");
		return nullptr;
	}

	uint32 RetIndex = Index;
	Index += 1;
	bCanProceed = Index < Num;

	return (void*)((SIZE_T)Array->GetData() + ((SIZE_T)Stride * RetIndex));
}

void* FArrayIterator::GetCurrent() const
{
	uint32 Num = (uint32)Array->Num();
	if (!bCanProceed || Index >= Num)
	{
		FAngelscriptEngine::Throw("Iterator out of bounds.");
		return nullptr;
	}

	return (void*)((SIZE_T)Array->GetData() + ((SIZE_T)Stride * Index));
}

void FArrayIterator::Advance()
{
	if (!bCanProceed)
		return;

	Index += 1;
	bCanProceed = Index < (uint32)Array->Num();
}

void FArrayIterator::AddArrayToIteratorDebugging(void* Array)
{
#if AS_ITERATOR_DEBUGGING
	GArraysBeingIterated.Add(Array);
#endif
}

void FArrayIterator::VerifyArrayIteratorDebuggingModification(void* Array)
{
#if AS_ITERATOR_DEBUGGING
	if (GArraysBeingIterated.Contains(Array))
	{
		FAngelscriptEngine::Throw("TArray is being modified during for loop iteration");
	}
#endif
}

asCScriptFunction* GetCompareFunction(asCTypeInfo* Type, bool bIsObjectPointer)
{
	auto* ObjectType = CastToObjectType(Type);
	if (ObjectType == nullptr)
		return nullptr;
	if (ObjectType->GetFirstMethod("opCmp") == nullptr)
		return nullptr;

	const FString MutableParam = TEXT("{0}");
	const FString ConstRefParam = TEXT("const {0}&");

	const FString ParamType = bIsObjectPointer ? MutableParam : ConstRefParam;
	const FString Parameter = FString::Format(*ParamType, { ANSI_TO_TCHAR(Type->GetName()) });
	const FString CmpDecl = FString::Format(TEXT("int opCmp({0} Other) const"), { Parameter });
	return (asCScriptFunction*)ObjectType->GetMethodByDecl(TCHAR_TO_ANSI(*CmpDecl));
}

FArrayOperations* ValidateArrayOperations(asITypeInfo* TemplateType, asCString* ErrorMessage)
{
	FArrayOperations* Ops = (FArrayOperations*)TemplateType->GetUserData();
	if (Ops != nullptr)
	{
		return Ops->bValid ? Ops : nullptr;
	}

	int32 SubTypeId = TemplateType->GetSubTypeId(0);
	auto Type = FAngelscriptTypeUsage::FromTypeId(SubTypeId);

	// We don't allow containers of templated types,
	if (!Type.CanBeTemplateSubType())
	{
		if (ErrorMessage != nullptr)
			*ErrorMessage = "Containers cannot be nested in other containers";
		return nullptr;
	}
		
	Ops = new FArrayOperations();
	TemplateType->SetUserData(Ops);

	if (!Type.IsValid())
	{
		if (ErrorMessage != nullptr)
			*ErrorMessage = "Subtype could not be found";
		return nullptr;
	}

	if (!(Type.CanConstruct() && Type.CanDestruct() && Type.CanCopy()))
	{
		if (ErrorMessage != nullptr)
			*ErrorMessage = "Subtype cannot be constructed or copied";
		return nullptr;
	}
	
	Ops->NumBytesPerElement = Type.GetValueSize();
	Ops->Alignment = Type.GetValueAlignment();
	Ops->Type = Type;
	Ops->bNeedConstruct = Type.NeedConstruct();
	Ops->bIsObjectPointer = Type.IsObjectPointer();
	Ops->bNeedDestruct = Type.NeedDestruct();
	Ops->bNeedCopy = Type.NeedCopy();

	Ops->bValid = Ops->NumBytesPerElement > 0;

	if (!Ops->bValid)
	{
		if (ErrorMessage != nullptr)
			*ErrorMessage = "Subtype is an empty struct";
		return nullptr;
	}
	
	if (asITypeInfo* SubType = TemplateType->GetSubType())
	{
		Ops->CompareFunction = GetCompareFunction((asCTypeInfo*)SubType, Type.IsObjectPointer());
		if (Ops->CompareFunction != nullptr)
			Ops->CompareFunction->isInUse = true;
	}
	
	return Ops;
}
