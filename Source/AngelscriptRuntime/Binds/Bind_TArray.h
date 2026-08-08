#pragma once
#include "AngelscriptBinds.h"
#include "AngelscriptType.h"
#include "Bind_TArray_Structs.h"
#include "Containers/ScriptArray.h"

#include "StartAngelscriptHeaders.h"
//#include "as_context.h"
//#include "as_scriptfunction.h"
//#include "as_objecttype.h"
#include "source/as_context.h"
#include "source/as_scriptfunction.h"
#include "source/as_objecttype.h"
#include "EndAngelscriptHeaders.h"

struct ANGELSCRIPTRUNTIME_API FAngelscriptArrayType : public FAngelscriptType
{
	bool HasReferences(const FAngelscriptTypeUsage& Usage) const override;
	FString GetAngelscriptTypeName() const override;
	virtual bool CanQueryPropertyType() const;
	void EmitReferenceInfo(const FAngelscriptTypeUsage& Usage, FGCReferenceParams& Params) const override;
	bool CanCreateProperty(const FAngelscriptTypeUsage& Usage) const override;
	FProperty* CreateProperty(const FAngelscriptTypeUsage& Usage, const FPropertyParams& Params) const override;
	bool MatchesProperty(const FAngelscriptTypeUsage& Usage, const FProperty* Property, EPropertyMatchType MatchType) const;
	bool CanCopy(const FAngelscriptTypeUsage& Usage) const override;
	bool NeedCopy(const FAngelscriptTypeUsage& Usage) const override;
	void CopyValue(const FAngelscriptTypeUsage& Usage, void* SourcePtr, void* DestinationPtr) const override;
	bool CanConstruct(const FAngelscriptTypeUsage& Usage) const override;
	bool NeedConstruct(const FAngelscriptTypeUsage& Usage) const override;
	void ConstructValue(const FAngelscriptTypeUsage& Usage, void* DestinationPtr) const override;
	bool CanDestruct(const FAngelscriptTypeUsage& Usage) const override;
	bool NeedDestruct(const FAngelscriptTypeUsage& Usage) const override;
	void DestructValue(const FAngelscriptTypeUsage& Usage, void* DestinationPtr) const override;
	int32 GetValueSize(const FAngelscriptTypeUsage& Usage) const override;
	int32 GetValueAlignment(const FAngelscriptTypeUsage& Usage) const;
	bool CanBeArgument(const FAngelscriptTypeUsage& Usage) const override;
	void SetArgument(const FAngelscriptTypeUsage& Usage, int32 ArgumentIndex, class asIScriptContext* Context, struct FFrame& Stack, const FArgData& Data) const override;
	bool CanBeReturned(const FAngelscriptTypeUsage& Usage) const override;
	void GetReturnValue(const FAngelscriptTypeUsage& Usage, class asIScriptContext* Context, void* Destination) const override;
	bool CanCompare(const FAngelscriptTypeUsage& Usage) const override;
	bool IsValueEqual(const FAngelscriptTypeUsage& Usage, void* SourcePtr, void* DestinationPtr) const override;
	FASDebugValue* CreateDebugValue(const FAngelscriptTypeUsage& Usage, FDebugValuePrototype& Values, int32 Offset) const override;
	bool GetDebuggerValue(const FAngelscriptTypeUsage& Usage, void* Address, struct FDebuggerValue& Value) const override;
	bool GetDebuggerScope(const FAngelscriptTypeUsage& Usage, void* Address, struct FDebuggerScope& Scope) const override;
	bool GetDebuggerMember(const FAngelscriptTypeUsage& Usage, void* Address, const FString& Member, struct FDebuggerValue& Value) const override;
	bool GetCppForm(const FAngelscriptTypeUsage& Usage, FCppForm& OutCppForm) const override;
	bool IsParamForcedOutParam() const override { return true; }
	bool CanBeTemplateSubType() const override { return false; }
};

struct FAngelscriptArrayIteratorType : public FAngelscriptType
{
	FString GetAngelscriptTypeName() const override;
	bool CanQueryPropertyType() const override;
	bool GetCppForm(const FAngelscriptTypeUsage& Usage, FCppForm& OutCppForm) const override;
};

struct FAngelscriptArrayConstIteratorType : public FAngelscriptType
{
	FString GetAngelscriptTypeName() const override;
	bool CanQueryPropertyType() const override;
	bool GetCppForm(const FAngelscriptTypeUsage& Usage, FCppForm& OutCppForm) const override;
};

ANGELSCRIPTRUNTIME_API struct FArrayOperations* ValidateArrayOperations(class asITypeInfo* TemplateType, class asCString* ErrorMessage);

struct ANGELSCRIPTRUNTIME_API FArrayOperations
{
	int32 NumBytesPerElement;
	int32 Alignment;
	bool bValid = false;
	FAngelscriptTypeUsage Type;
	class asCScriptFunction* CompareFunction = nullptr;

	bool bNeedConstruct;
	bool bNeedDestruct;
	bool bNeedCopy;
	bool bIsObjectPointer;

	FORCEINLINE void* Get(FScriptArray& Arr, int32 Index)
	{
		void* ElementRef = (void*)((SIZE_T)Arr.GetData() + (((SIZE_T)Index) * ((SIZE_T)NumBytesPerElement)));
#ifdef SCRIPT_ASSUME
		SCRIPT_ASSUME(ElementRef != nullptr);
#endif
		return ElementRef;
	}

	FORCEINLINE static FArrayOperations* GetArrayOperations(asCObjectType* Meta)
	{
		return (FArrayOperations*)Meta->plainUserData;
	}

	FORCEINLINE static void Construct(FScriptArray* Address)
	{
		new(Address) FScriptArray();
	}

	static void Destruct(FScriptArray& Arr, asCObjectType* Meta)
	{
		// Sometimes destructors get called directly, outside of a script context
		auto* Ops = GetArrayOperations(Meta);

		if (Ops->bNeedDestruct)
		{
			for (int32 i = 0, Num = Arr.Num(); i < Num; ++i)
				Ops->Type.DestructValue(Ops->Get(Arr, i));
		}

		Arr.~FScriptArray();
	}

	template<typename T>
	static void Destruct_Template(TArray<T>& Arr)
	{
		Arr.~TArray();
	}

	FORCEINLINE static bool IsEmpty(FScriptArray& Arr)
	{
		return Arr.IsEmpty();
	}

	FORCEINLINE static int32 Num(FScriptArray& Arr)
	{
		return Arr.Num();
	}

	FORCEINLINE static int32 Max(FScriptArray& Arr)
	{
		//return Arr.ArrayMax;
		return Arr.GetSlack() + Arr.Num();
	}

	FORCEINLINE static int32 GetSlack(FScriptArray& Arr)
	{
		//return Arr.ArrayMax - Arr.ArrayNum;
		return Arr.GetSlack();
	}

	FORCEINLINE static int64 GetAllocatedSize(FScriptArray& Arr, asCObjectType* Meta)
	{
		auto* Ops = GetArrayOperations(Meta);
		//return Arr.ArrayMax * Ops->NumBytesPerElement;
		return Arr.GetAllocatedSize(Ops->NumBytesPerElement);
	}

	template<typename T>
	FORCEINLINE static int64 GetAllocatedSize_Template(TArray<T>& Arr)
	{
		return Arr.GetAllocatedSize();
	}

	FORCEINLINE static bool IsValidIndex(FScriptArray& Arr, int32 Index)
	{
		return ((uint32)Index) < ((uint32)Arr.Num());
	}

	FORCEINLINE static void* OpIndex(FScriptArray& Arr, asCObjectType* Meta, int32 Index)
	{
		auto* Ops = GetArrayOperations(Meta);
		if (!IsValidIndex(Arr, Index)) [[unlikely]]
		{
			ThrowOutOfBounds();
			return nullptr;
		}
		else
		{
			void* ElementRef = Ops->Get(Arr, Index);
#ifdef SCRIPT_ASSUME
			SCRIPT_ASSUME(ElementRef != nullptr);
#endif
			return ElementRef;
		}
	}

	FORCEINLINE static void* OpIndex_Unchecked(FScriptArray& Arr, asCObjectType* Meta, int32 Index)
	{
		auto* Ops = GetArrayOperations(Meta);
		void* ElementRef = Ops->Get(Arr, Index);
		return ElementRef;
	}

	FORCEINLINE static void* OpIndex_Stride_Unchecked(FScriptArray& Arr, int32 Stride, int32 Index)
	{
		void* ElementRef = (void*)((SIZE_T)Arr.GetData() + (((SIZE_T)Index) * ((SIZE_T)Stride)));
		return ElementRef;
	}

	template<typename T>
	FORCEINLINE static void* OpIndex_Template_Unchecked(TArray<T>& Arr, int32 Index)
	{
		void* ElementRef = &Arr.GetData()[(uint32)Index];
		return ElementRef;
	}

	static void* Last(FScriptArray& Arr, asCObjectType* Meta, int32 IndexFromEnd = 0)
	{
		if (!IsValidIndex(Arr, Arr.Num() - IndexFromEnd - 1)) [[unlikely]]
		{
			ThrowOutOfBounds();
			return nullptr;
		}
		else
		{
			auto* Ops = GetArrayOperations(Meta);
			void* ElementRef = Ops->Get(Arr, Arr.Num() - IndexFromEnd - 1);
#ifdef SCRIPT_ASSUME
			SCRIPT_ASSUME(ElementRef != nullptr);
#endif
			return ElementRef;
		}
	}

	template<typename T>
	static void* Last_Template(TArray<T>& Arr, int32 IndexFromEnd = 0)
	{
		if (!IsValidIndex((FScriptArray&)Arr, Arr.Num() - IndexFromEnd - 1)) [[unlikely]]
		{
			ThrowOutOfBounds();
			return nullptr;
		}
		else
		{
			void* ElementRef = &Arr.GetData()[(uint32)(Arr.Num() - IndexFromEnd - 1)];
#ifdef SCRIPT_ASSUME
			SCRIPT_ASSUME(ElementRef != nullptr);
#endif
			return ElementRef;
		}
	}

	static int32 FindIndex(FScriptArray& Arr, asCObjectType* Meta, void* Value)
	{
		auto* Ops = GetArrayOperations(Meta);
		if (!Ops->Type.CanCompare())
			return -1;

		for (int32 i = 0, Num = Arr.Num(); i < Num; ++i)
		{
			if (Ops->Type.IsValueEqual(Ops->Get(Arr, i), Value))
				return i;
		}
		return -1;
	}

	template<typename T>
	static int32 FindIndex_Template(TArray<T>& Arr, void* Value)
	{
		return Arr.IndexOfByKey(*(T*)Value);
	}

	static bool Contains(FScriptArray& Arr, asCObjectType* Meta, void* Value)
	{
		auto* Ops = GetArrayOperations(Meta);
		if (!Ops->Type.CanCompare())
			return false;

		for (int32 i = 0, Num = Arr.Num(); i < Num; ++i)
		{
			if (Ops->Type.IsValueEqual(Ops->Get(Arr, i), Value))
				return true;
		}
		return false;
	}

	template<typename T>
	static bool Contains_Template(TArray<T>& Arr, void* Value)
	{
		return Arr.Contains(*(T*)Value);
	}

	static FScriptArray& OpAssign(FScriptArray& DestinationArray, asCObjectType* Meta, FScriptArray& SourceArray);
	static bool OpEquals(FScriptArray& ArrayA, asCObjectType* Meta, FScriptArray& ArrayB);
	static void Add(FScriptArray& Arr, asCObjectType* Meta, void* Value);
	static bool AddUnique(FScriptArray& Arr, asCObjectType* Meta, void* Value);
	static void Append(FScriptArray& DestinationArray, asCObjectType* Meta, FScriptArray& SourceArray);
	static void Shuffle(FScriptArray& Array, asCObjectType* Meta);
	static void Swap(FScriptArray& Arr, asCObjectType* Meta, int32 FirstIndexToSwap, int32 SecondIndexToSwap);
	static void MoveAssignFrom(FScriptArray& Arr, asCObjectType* Meta, FScriptArray& OtherArray);
	static void Insert(FScriptArray& Arr, asCObjectType* Meta, void* Value, int32 Index);
	static void Empty(FScriptArray& Arr, asCObjectType* Meta, int32 ReservedSize);
	static void Reset(FScriptArray& Arr, asCObjectType* Meta, int32 ReservedSize);
	static void Reserve(FScriptArray& Arr, asCObjectType* Meta, int32 ReservedSize);
	static void SetNum(FScriptArray& Arr, asCObjectType* Meta, int32 NewNum);
	static void SetNumZeroed(FScriptArray& Arr, asCObjectType* Meta, int32 NewNum);
	static void Copy(FScriptArray& Arr, asCObjectType* Meta, FScriptArray& SourceArray, int32 SourceIndex, int32 Count, int32 TargetIndex);
	static int32 RemoveSingle(FScriptArray& Arr, asCObjectType* Meta, void* Value);
	static int32 Remove(FScriptArray& Arr, asCObjectType* Meta, void* Value);
	static int32 RemoveSingleSwap(FScriptArray& Arr, asCObjectType* Meta, void* Value);
	static int32 RemoveSwap(FScriptArray& Arr, asCObjectType* Meta, void* Value);
	static void RemoveAt(FScriptArray& Arr, asCObjectType* Meta, int32 Index);
	static void RemoveAtSwap(FScriptArray& Arr, asCObjectType* Meta, int32 Index);
	static void Sort(FScriptArray& Arr, asCObjectType* Meta, bool bDescendingOrder);
	static void Shrink(FScriptArray& Arr, asCObjectType* Meta);

	template<typename T>
	static FScriptArray& OpAssign_Template(TArray<T>& DestinationArray, FScriptArray& SourceArray)
	{
		DestinationArray = *(TArray<T>*)&SourceArray;
		return *(FScriptArray*)&DestinationArray;
	}

	template<typename T>
	static bool OpEquals_Template(TArray<T>& ArrayA, FScriptArray& ArrayB)
	{
		return ArrayA == *(TArray<T>*)&ArrayB;
	}

	template<typename T>
	static void Add_Template(TArray<T>& Arr, void* Value)
	{
		Arr.Add(*(T*)Value);
	}

	template<typename T>
	static bool AddUnique_Template(TArray<T>& Arr, void* Value)
	{
		int32 Num = Arr.Num();
		int32 Index = Arr.AddUnique(*(T*)Value);
		return Index >= Num;
	}

	template<typename T>
	static void Append_Template(TArray<T>& DestinationArray, FScriptArray& SourceArray)
	{
		DestinationArray.Append(*(TArray<T>*)&SourceArray);
	}

	template<typename T>
	static void Swap_Template(TArray<T>& Arr, int32 FirstIndexToSwap, int32 SecondIndexToSwap)
	{
		if (!IsValidIndex((FScriptArray&)Arr, FirstIndexToSwap) || !IsValidIndex((FScriptArray&)Arr, SecondIndexToSwap))
		{
			FAngelscriptEngine::Throw("Array index out of bounds.");
			return;
		}

		Arr.SwapMemory(FirstIndexToSwap, SecondIndexToSwap);
	}

	template<typename T>
	static void MoveAssignFrom_Template(TArray<T>& Arr, FScriptArray& OtherArray)
	{
		if ((void*)&OtherArray == (void*)&Arr)
		{
			FAngelscriptEngine::Throw("Cannot move assign an array into itself.");
			return;
		}

		Arr = MoveTemp(*(TArray<T>*)&OtherArray);
	}

	template<typename T>
	static void Insert_Template(TArray<T>& Arr, void* Value, int32 Index)
	{
		if (Index > Arr.Num() || Index < 0)
		{
			FAngelscriptEngine::Throw("Array index out of bounds. Need to insert between 0 and ArraySize");
			return;
		}

		Arr.Insert(*(T*)Value, Index);
	}

	template<typename T>
	static void Empty_Template(TArray<T>& Arr, int32 ReservedSize)
	{
		Arr.Empty(ReservedSize);
	}

	template<typename T>
	static void Reset_Template(TArray<T>& Arr, int32 ReservedSize)
	{
		Arr.Reset(ReservedSize);
	}

	template<typename T>
	static void Reserve_Template(TArray<T>& Arr, int32 ReservedSize)
	{
		Arr.Reserve(ReservedSize);
	}

	template<typename T>
	static int32 RemoveSingle_Template(TArray<T>& Arr, void* Value)
	{
		return Arr.RemoveSingle(*(T*)Value);
	}

	template<typename T>
	static int32 Remove_Template(TArray<T>& Arr, void* Value)
	{
		return Arr.Remove(*(T*)Value);
	}

	template<typename T>
	static int32 RemoveSingleSwap_Template(TArray<T>& Arr, void* Value)
	{
		return Arr.RemoveSingleSwap(*(T*)Value);
	}

	template<typename T>
	static int32 RemoveSwap_Template(TArray<T>& Arr, void* Value)
	{
		return Arr.RemoveSwap(*(T*)Value);
	}

	template<typename T>
	static void RemoveAt_Template(TArray<T>& Arr, int32 Index)
	{
		if (!IsValidIndex((FScriptArray&)Arr, Index))
		{
			FAngelscriptEngine::Throw("Array index out of bounds.");
			return;
		}

		Arr.RemoveAt(Index);
	}

	template<typename T>
	static void RemoveAtSwap_Template(TArray<T>& Arr, int32 Index)
	{
		if (!IsValidIndex((FScriptArray&)Arr, Index))
		{
			FAngelscriptEngine::Throw("Array index out of bounds.");
			return;
		}

		Arr.RemoveAtSwap(Index);
	}

	template<typename T>
	static void Shrink_Template(TArray<T>& Arr)
	{
		Arr.Shrink();
	}

	FORCENOINLINE static void ThrowOutOfBounds();

	FORCEINLINE static void* Iterator_Proceed(FArrayIterator* It)
	{
		uint32 Num = (uint32)It->Array->Num();
		if (It->Index >= Num) [[unlikely]]
		{
			ThrowOutOfBounds();
			return nullptr;
		}

		uint32 RetIndex = It->Index;
		uint32 NewIndex = RetIndex + 1;

		It->bCanProceed = NewIndex < Num;
		It->Index = NewIndex;

		void* ElementRef = (void*)((SIZE_T)It->Array->GetData() + (((SIZE_T)RetIndex) * ((SIZE_T)It->Stride)));

#ifdef SCRIPT_ASSUME
		SCRIPT_ASSUME(ElementRef != nullptr);
#endif

		return ElementRef;
	}

	FORCEINLINE static void* Iterator_Proceed_Unchecked(FArrayIterator* It, uint32 Num)
	{
		uint32 RetIndex = It->Index;
		uint32 NewIndex = RetIndex + 1;

		It->bCanProceed = NewIndex < Num;
		It->Index = NewIndex;

		void* ElementRef = (void*)((SIZE_T)It->Array->GetData() + (((SIZE_T)RetIndex) * ((SIZE_T)It->Stride)));

#ifdef SCRIPT_ASSUME
		SCRIPT_ASSUME(ElementRef != nullptr);
#endif

		return ElementRef;
	}

	template<typename T>
	FORCEINLINE static void* Iterator_Proceed_Template_Unchecked(FArrayIterator* It, uint32 Num)
	{
		uint32 RetIndex = It->Index;
		uint32 NewIndex = RetIndex + 1;

		It->bCanProceed = NewIndex < Num;
		It->Index = NewIndex;

		void* ElementRef = (void*)((SIZE_T)It->Array->GetData() + (((SIZE_T)RetIndex) * sizeof(T)));

#ifdef SCRIPT_ASSUME
		SCRIPT_ASSUME(ElementRef != nullptr);
#endif

		return ElementRef;
	}
};
