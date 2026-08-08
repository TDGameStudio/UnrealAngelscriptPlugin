#include "Binds/Bind_TMap.h"
#include "AngelscriptBinds.h"
#include "AngelscriptEngine.h"
#include "AngelscriptDocs.h"

#include "ClassGenerator/ASClass.h"

#include "Containers/Set.h"
#include "Containers/Map.h"
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
 * Generic TMap and iterator binding surface.
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | AngelScript usage signature                                                                | Purpose / parameter notes                                                                                            |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | template<class K, class V> struct TMap;                                                    | Declares the generic map type.                                                                                       |
 * |                                                                                            | Each TMap<K,V> instantiation validates key hashing/equality and value operations.                                    |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | template<class K, class V> struct TMapIterator;                                            | Declares the mutable map iterator type.                                                                              |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | template<class K, class V> struct TMapConstIterator;                                       | Declares the read-only map iterator type.                                                                            |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | TMap<K,V> Map;                                                                             | Constructs an empty map; element lifetimes are managed automatically.                                                |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | Map[Key];                                                                                  | Returns a mutable value reference, adding a default value when Key is absent.                                        |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | ConstMap[Key];                                                                             | Returns a read-only value reference for Key; a missing key raises a script exception.                                |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | void TMap<K,V>.Add(const K& Key, const V& Value);                                          | Adds or replaces the value associated with Key.                                                                      |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | bool TMap<K,V>.Contains(const K& Key) const;                                               | Returns whether Key is present.                                                                                      |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | bool TMap<K,V>.RemoveAndCopyValue(const K& Key, V& OutValue);                              | Removes Key and copies its previous value when found.                                                                |
 * |                                                                                            | @param OutValue Receives the removed value on success.                                                               |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | bool TMap<K,V>.Remove(const K& Key);                                                       | Removes Key and reports whether an entry existed.                                                                    |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | int32 TMap<K,V>.Num() const;                                                               | Returns the number of key-value pairs.                                                                               |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | bool TMap<K,V>.IsEmpty() const;                                                            | Returns whether the map has no entries.                                                                              |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | V& TMap<K,V>.FindOrAdd(const K& Key);                                                      | Returns the value for Key, default-constructing it when absent.                                                      |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | V& TMap<K,V>.FindOrAdd(const K& Key, const V& DefaultValue);                               | Returns the value for Key, inserting DefaultValue when absent.                                                       |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | bool TMap<K,V>.Find(const K& Key, V& OutValue) const;                                      | Copies the value for Key and reports whether it was found.                                                           |
 * |                                                                                            | @param OutValue Receives the found value on success.                                                                 |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | Map = Other;                                                                               | Replaces this map with a copy of Other.                                                                              |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | Map == Other;                                                                              | Compares maps by key-value contents.                                                                                 |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | void TMap<K,V>.Empty(int32 Slack = 0);                                                     | Removes every entry and reserves optional capacity.                                                                  |
 * |                                                                                            | @param Slack Desired post-clear allocation capacity.                                                                 |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | void TMap<K,V>.Reset();                                                                    | Removes every entry while retaining reusable allocation.                                                             |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | void TMap<K,V>.GetKeys(TArray<K>& OutKeys) const;                                          | Copies all keys into an array.                                                                                       |
 * |                                                                                            | @param OutKeys Receives keys in native map iteration order.                                                          |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | void TMap<K,V>.GetValues(TArray<V>& OutValues) const;                                      | Copies all values into an array.                                                                                     |
 * |                                                                                            | @param OutValues Receives values in native map iteration order.                                                      |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | TMapIterator<K,V> It(const TMapIterator<K,V>& Other);                                      | Copy-constructs a mutable iterator.                                                                                  |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | It = Other;                                                                                | Assigns mutable iterator state.                                                                                      |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | bool TMapIterator<K,V>.CanProceed;                                                         | Reports whether the mutable iterator refers to a valid entry.                                                        |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | void TMapIterator<K,V>.RemoveCurrent() const;                                              | Removes the current entry; iterator-debug builds enforce mutation safety.                                            |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | const K& TMapIterator<K,V>.GetKey() const;                                                 | Returns the current key.                                                                                             |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | V& TMapIterator<K,V>.GetValue() const;                                                     | Returns a mutable reference to the current value.                                                                    |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | void TMapIterator<K,V>.SetValue(const V& NewValue) const;                                  | Replaces the current value.                                                                                          |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | TMapIterator<K,V>& TMapIterator<K,V>.Proceed();                                            | Advances to the next entry and returns this iterator.                                                                |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | TMapConstIterator<K,V> It(const TMapConstIterator<K,V>& Other);                            | Copy-constructs a read-only iterator.                                                                                |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | It = Other;                                                                                | Assigns read-only iterator state.                                                                                    |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | bool TMapConstIterator<K,V>.CanProceed;                                                    | Reports whether the read-only iterator refers to a valid entry.                                                      |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | const K& TMapConstIterator<K,V>.GetKey() const;                                            | Returns the current key.                                                                                             |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | const V& TMapConstIterator<K,V>.GetValue() const;                                          | Returns the current value as read-only.                                                                              |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | TMapConstIterator<K,V>& TMapConstIterator<K,V>.Proceed();                                  | Advances to the next entry and returns this iterator.                                                                |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | for (auto Value : Map) { Use(Value); }                                                     | Iterates mutable or read-only values through the opFor protocol.                                                     |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | for (auto Key, auto Value : Map) { Use(Key, Value); }                                      | Iterates keys and values together through the opFor protocol.                                                        |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | TMapIterator<K,V> TMap<K,V>.Iterator();                                                    | Creates a mutable explicit iterator.                                                                                 |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | TMapConstIterator<K,V> TMap<K,V>.Iterator() const;                                         | Creates a read-only explicit iterator.                                                                               |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 */

#if AS_ITERATOR_DEBUGGING
static thread_local TArray<void*, TInlineAllocator<16>> GMapsBeingIterated;

static bool CheckMapIteratorDebug(FScriptMap& Map)
{
	if (GMapsBeingIterated.Contains(&Map))
	{
		FAngelscriptEngine::Throw("TMap is being modified during for loop iteration");
		return false;
	}

	return true;
}

void FMapOperations::MarkMapBeingIterated(FScriptMap& Map)
{
	GMapsBeingIterated.Add(&Map);
}

void FMapOperations::UnmarkMapBeingIterated(FScriptMap& Map)
{
	GMapsBeingIterated.RemoveSingle(&Map);
}
#endif

#if AS_REFERENCE_DEBUGGING
static void InvalidateReferencesToMap(FScriptMap& Map, FMapOperations* Ops)
{
	asCContext* Context = (asCContext*)asGetActiveContext();
	if (Context != nullptr)
	{
		Context->InvalidateReferencesToMemoryBlock(Map.GetData(0, Ops->Layout), Map.GetMaxIndex() * Ops->Layout.SetLayout.SparseArrayLayout.Size);
	}
}
#endif

bool ValidateMapOperations(asITypeInfo* TemplateType, asCString* ErrorMessage);

void FAngelscriptMapBinds::Add(FScriptMap& Map, asCObjectType* Meta, void* Key, void* Value)
{
#if AS_ITERATOR_DEBUGGING
	if (!CheckMapIteratorDebug(Map))
		return;
#endif

	auto* Ops = FMapOperations::GetMapOperations(Meta);

#if AS_REFERENCE_DEBUGGING
	InvalidateReferencesToMap(Map, Ops);
#endif

	Ops->Add(Map, Key, Value);
}

bool FAngelscriptMapBinds::Contains(FScriptMap& Map, asCObjectType* Meta, void* Key)
{
	auto* Ops = FMapOperations::GetMapOperations(Meta);
	int32 Index = Ops->FindPairIndex(Map, Key);
	return Index != INDEX_NONE;
}

bool FAngelscriptMapBinds::RemoveAndCopyValue(FScriptMap& Map, asCObjectType* Meta, void* Key, void* OutValue)
{
#if AS_ITERATOR_DEBUGGING
	if (!CheckMapIteratorDebug(Map))
		return false;
#endif

	auto* Ops = FMapOperations::GetMapOperations(Meta);

#if AS_REFERENCE_DEBUGGING
	InvalidateReferencesToMap(Map, Ops);
#endif

	int32 Index = Ops->FindPairIndex(Map, Key);
	if (Index == INDEX_NONE)
		return false;

	void* FoundValue = Ops->GetValue(Map, Index);
	if (Ops->bValueNeedCopy)
		Ops->ValueType.CopyValue(FoundValue, OutValue);
	else
		FMemory::Memcpy(OutValue, FoundValue, Ops->ValueSize);

	Ops->RemoveAt(Map, Index);
	return true;
}

bool FAngelscriptMapBinds::Remove(FScriptMap& Map, asCObjectType* Meta, void* Key)
{
#if AS_ITERATOR_DEBUGGING
	if (!CheckMapIteratorDebug(Map))
		return false;
#endif

	auto* Ops = FMapOperations::GetMapOperations(Meta);

#if AS_REFERENCE_DEBUGGING
	InvalidateReferencesToMap(Map, Ops);
#endif

	int32 Index = Ops->FindPairIndex(Map, Key);
	if (Index == INDEX_NONE)
		return false;

	Ops->RemoveAt(Map, Index);
	return true;
}

void* FAngelscriptMapBinds::FindOrAdd_Defaulted(FScriptMap& Map, asCObjectType* Meta, void* Key)
{
	auto* Ops = FMapOperations::GetMapOperations(Meta);

	int32 Index = Ops->FindPairIndex(Map, Key);
	if (Index == INDEX_NONE)
	{
#if AS_ITERATOR_DEBUGGING
		CheckMapIteratorDebug(Map);
#endif

#if AS_REFERENCE_DEBUGGING
		InvalidateReferencesToMap(Map, Ops);
#endif

		TArray<uint8, TInlineAllocator<64>> TempValue;
		TempValue.SetNumZeroed(Ops->ValueSize + 16);

		void* ValuePtr = Align(TempValue.GetData(), Ops->ValueAlignment);
		if (Ops->bValueNeedConstruct)
			Ops->ValueType.ConstructValue(ValuePtr);

		Ops->Add(Map, Key, ValuePtr);
		Index = Ops->FindPairIndex(Map, Key);

		if (Ops->bValueNeedDestruct)
			Ops->ValueType.DestructValue(ValuePtr);
	}

	return Ops->GetValue(Map, Index);
}

void* FAngelscriptMapBinds::FindOrAdd(FScriptMap& Map, asCObjectType* Meta, void* Key, void* ValuePtr)
{
	auto* Ops = FMapOperations::GetMapOperations(Meta);

	int32 Index = Ops->FindPairIndex(Map, Key);
	if (Index == INDEX_NONE)
	{
#if AS_ITERATOR_DEBUGGING
		CheckMapIteratorDebug(Map);
#endif
#if AS_REFERENCE_DEBUGGING
		InvalidateReferencesToMap(Map, Ops);
#endif

		Ops->Add(Map, Key, ValuePtr);
		Index = Ops->FindPairIndex(Map, Key);
	}

	return Ops->GetValue(Map, Index);
}

bool FAngelscriptMapBinds::Find(FScriptMap& Map, asCObjectType* Meta, void* Key, void* OutValue)
{
	auto* Ops = FMapOperations::GetMapOperations(Meta);

	int32 Index = Ops->FindPairIndex(Map, Key);
	if (Index == INDEX_NONE)
		return false;

	void* FoundValue = Ops->GetValue(Map, Index);
	if (Ops->bValueNeedCopy)
		Ops->ValueType.CopyValue(FoundValue, OutValue);
	else
		FMemory::Memcpy(OutValue, FoundValue, Ops->ValueSize);
	return true;
}

FScriptMap& FAngelscriptMapBinds::OpAssign(FScriptMap& Destination, asCObjectType* Meta, FScriptMap& Source)
{
#if AS_ITERATOR_DEBUGGING
	if (!CheckMapIteratorDebug(Destination))
		return Destination;
#endif

	auto* Ops = FMapOperations::GetMapOperations(Meta);

#if AS_REFERENCE_DEBUGGING
	InvalidateReferencesToMap(Destination, Ops);
#endif

	Ops->Empty(Destination, Source.Num());

	for (int32 i = 0, Num = Source.GetMaxIndex(); i < Num; ++i)
	{
		if (Source.IsValidIndex(i))
			Ops->Add(Destination, Ops->GetKey(Source, i), Ops->GetValue(Source, i));
	}

	return Destination;
}

bool FAngelscriptMapBinds::OpEquals(FScriptMap& MapA, asCObjectType* Meta, FScriptMap& MapB)
{
	auto* Ops = FMapOperations::GetMapOperations(Meta);
	if (!Ops->KeyType.CanCompare() || !Ops->ValueType.CanCompare())
	{
		FAngelscriptEngine::Throw("Cannot compare map key/value type for equality.");
		return false;
	}

	return Ops->IsPermutation(MapA, MapB);
}

void FAngelscriptMapBinds::Empty(FScriptMap& Map, asCObjectType* Meta, int32 Slack)
{
#if AS_ITERATOR_DEBUGGING
	if (!CheckMapIteratorDebug(Map))
		return;
#endif

	auto* Ops = FMapOperations::GetMapOperations(Meta);

#if AS_REFERENCE_DEBUGGING
	InvalidateReferencesToMap(Map, Ops);
#endif

	Ops->Empty(Map, Slack);
}

void FAngelscriptMapBinds::Reset(FScriptMap& Map, asCObjectType* Meta)
{
#if AS_ITERATOR_DEBUGGING
	if (!CheckMapIteratorDebug(Map))
		return;
#endif

	auto* Ops = FMapOperations::GetMapOperations(Meta);

#if AS_REFERENCE_DEBUGGING
	InvalidateReferencesToMap(Map, Ops);
#endif

	Ops->Empty(Map, Map.Num());
}

void FAngelscriptMapBinds::GetKeys(FScriptMap& Map, asCObjectType* Meta, FScriptArray& OutKeys)
{
#if AS_ITERATOR_DEBUGGING
	if (!CheckMapIteratorDebug(Map))
		return;
#endif

	auto* Ops = FMapOperations::GetMapOperations(Meta);

#if AS_REFERENCE_DEBUGGING
	InvalidateReferencesToMap(Map, Ops);
#endif

	int32 ArrayIndex = 0;
	for (int32 SlotIndex = 0; SlotIndex < Map.GetMaxIndex(); ++SlotIndex)
	{
		if (Map.IsValidIndex(SlotIndex))
		{
			void* KeyPtr = Ops->GetKey(Map, SlotIndex);

			OutKeys.Insert(ArrayIndex, 1, Ops->KeySize, Ops->KeyAlignment);
			uint8* DestPtr = static_cast<uint8*>(OutKeys.GetData()) + (ArrayIndex * Ops->KeySize);
			
			if (Ops->bKeyNeedConstruct)
			{
				Ops->KeyType.ConstructValue(DestPtr);
			}
			
			if (Ops->bKeyNeedCopy)
			{
				Ops->KeyType.CopyValue(KeyPtr, DestPtr);
			}
			else
			{
				FMemory::Memcpy(DestPtr, KeyPtr, Ops->KeySize);
			}

			ArrayIndex++;
		}
	}
}

void FAngelscriptMapBinds::GetValues(FScriptMap& Map, asCObjectType* Meta, FScriptArray& OutValues)
{
#if AS_ITERATOR_DEBUGGING
	if (!CheckMapIteratorDebug(Map))
		return;
#endif

	auto* Ops = FMapOperations::GetMapOperations(Meta);

#if AS_REFERENCE_DEBUGGING
	InvalidateReferencesToMap(Map, Ops);
#endif

	int32 ArrayIndex = 0;
	for (int32 SlotIndex = 0; SlotIndex < Map.GetMaxIndex(); ++SlotIndex)
	{
		if (Map.IsValidIndex(SlotIndex))
		{
			void* ValuePtr = Ops->GetValue(Map, SlotIndex);

			OutValues.Insert(ArrayIndex, 1, Ops->ValueSize, Ops->ValueAlignment);
			uint8* DestPtr = static_cast<uint8*>(OutValues.GetData()) + (ArrayIndex * Ops->ValueSize);
			
			if (Ops->bValueNeedConstruct)
			{
				Ops->ValueType.ConstructValue(DestPtr);
			}
			
			if (Ops->bValueNeedCopy)
			{
				Ops->ValueType.CopyValue(ValuePtr, DestPtr);
			}
			else
			{
				FMemory::Memcpy(DestPtr, ValuePtr, Ops->ValueSize);
			}

			ArrayIndex++;
		}
	}
}

namespace
{
	struct FAngelscriptMapForeachBinds
	{
		static int32 Begin(FScriptMap& Map, asCObjectType* Meta)
		{
			return FMapOperations::GetMapOperations(Meta)->FindNextIndex(Map, -1);
		}

		static bool End(FScriptMap&, int32 Iterator)
		{
			return Iterator == -1;
		}

		static void Next(FScriptMap& Map, asCObjectType* Meta, int32& Iterator)
		{
			if (Iterator == -1)
				return;
			Iterator = FMapOperations::GetMapOperations(Meta)->FindNextIndex(Map, Iterator);
		}

		static void* Value(FScriptMap& Map, asCObjectType* Meta, int32 Iterator)
		{
			auto* Ops = FMapOperations::GetMapOperations(Meta);
			if (!Map.IsValidIndex(Iterator))
			{
				FAngelscriptEngine::Throw("Iterator out of bounds.");
				return nullptr;
			}
			return Ops->GetValue(Map, Iterator);
		}

		static void* Key(FScriptMap& Map, asCObjectType* Meta, int32 Iterator)
		{
			auto* Ops = FMapOperations::GetMapOperations(Meta);
			if (!Map.IsValidIndex(Iterator))
			{
				FAngelscriptEngine::Throw("Iterator out of bounds.");
				return nullptr;
			}
			return Ops->GetKey(Map, Iterator);
		}
	};

	void BindTMapTypeDeclarations(FAngelscriptBinds& Binds)
	{
		FBindFlags Flags;
		Flags.bTemplate = true;
		Flags.TemplateType = "<K,V>";
		Flags.ExtraFlags = asOBJ_TEMPLATE_SUBTYPE_COVARIANT;
		Binds.ValueClassForTarget<FScriptMap>("TMap<class K, class V>", Flags);

		FBindFlags IteratorFlags;
		IteratorFlags.bTemplate = true;
		IteratorFlags.TemplateType = "<K,V>";
		Binds.ValueClassForTarget<FMapIterator>("TMapIterator<class K, class V>", IteratorFlags);
		Binds.ValueClassForTarget<FMapIterator>("TMapConstIterator<class K, class V>", IteratorFlags);
	}

	void BindTMapMethodSurface(FAngelscriptBinds& Binds)
	{
		auto TMap_ = Binds.ExistingClassForTarget("TMap<K,V>");
		TMap_.Constructor("void f()", FUNC_TRIVIAL(FAngelscriptMapBinds::Construct));

		TMap_.Destructor("void f()", &FAngelscriptMapBinds::Destruct)
			.PassScriptObjectTypeAsFirstParam()
			.NativeTemplateInstantiatedCall("FAngelscriptMapBinds::Destruct", false, false, false);

		TMap_.TemplateCallback("bool f(int&in Type, int&out ErrorMessage)", &ValidateMapOperations);

		TMap_.Method("V& opIndex(const K&in if_handle_then_const Key)", &FAngelscriptMapBinds::OpIndex)
			.PassScriptObjectTypeAsFirstParam()
			.NativeTemplateInstantiatedCall("FAngelscriptMapBinds::OpIndex", false, true, false);

		TMap_.Method(
			"const V& opIndex(const K&in if_handle_then_const Key) const",
			&FAngelscriptMapBinds::OpIndex)
			.PassScriptObjectTypeAsFirstParam()
			.NativeTemplateInstantiatedCall("FAngelscriptMapBinds::OpIndex", false, true, false);

		TMap_.Method(
			"void Add(const K&in if_handle_then_const Key, const V&in if_handle_then_const Value)",
			&FAngelscriptMapBinds::Add)
			.PassScriptObjectTypeAsFirstParam()
			.NativeTemplateInstantiatedCall("FAngelscriptMapBinds::Add", false, true, true);

		TMap_.Method("bool Contains(const K&in if_handle_then_const Key) const", &FAngelscriptMapBinds::Contains)
			.PassScriptObjectTypeAsFirstParam()
			.NativeTemplateInstantiatedCall("FAngelscriptMapBinds::Contains", true, true, false);

		TMap_.Method(
			"bool RemoveAndCopyValue(const K&in if_handle_then_const Key, V&out OutValue)",
			&FAngelscriptMapBinds::RemoveAndCopyValue)
			.PassScriptObjectTypeAsFirstParam()
			.NativeTemplateInstantiatedCall(
				"FAngelscriptMapBinds::RemoveAndCopyValue",
				false,
				true,
				true);

		TMap_.Method("bool Remove(const K&in if_handle_then_const Key)", &FAngelscriptMapBinds::Remove)
			.PassScriptObjectTypeAsFirstParam()
			.NativeTemplateInstantiatedCall("FAngelscriptMapBinds::Remove", true, true, true);

		TMap_.Method("int32 Num() const", FUNC_TRIVIAL(FAngelscriptMapBinds::Num));
		TMap_.Method("bool IsEmpty() const", FUNC_TRIVIAL(FAngelscriptMapBinds::IsEmpty));

		TMap_.Method("V& FindOrAdd(const K&in if_handle_then_const Key)", FUNC(FAngelscriptMapBinds::FindOrAdd_Defaulted))
			.PassScriptObjectTypeAsFirstParam()
			.Documentation(TEXT("Find the value associated with the key. If none exists, add and return a new value using the default constructor."));

		TMap_.Method(
			"V& FindOrAdd(const K&in if_handle_then_const Key, const V&in if_handle_then_const DefaultValue)",
			&FAngelscriptMapBinds::FindOrAdd)
			.PassScriptObjectTypeAsFirstParam()
			.NativeTemplateInstantiatedCall("FAngelscriptMapBinds::FindOrAdd", false, false, false)
			.Documentation(TEXT("Find the value associated with the key. If none exists, add and return new value set to DefaultValue."));

		TMap_.Method(
			"bool Find(const K&in if_handle_then_const Key, V&out OutValue) const",
			&FAngelscriptMapBinds::Find)
			.PassScriptObjectTypeAsFirstParam()
			.NativeTemplateInstantiatedCall("FAngelscriptMapBinds::Find", false, true, true)
			.Documentation(TEXT("Find the value associated with the key. If none exists, return false. Copies the found value to OutValue."));

		TMap_.Method("TMap<K,V>& opAssign(const TMap<K,V>& Other)", &FAngelscriptMapBinds::OpAssign)
			.PassScriptObjectTypeAsFirstParam()
			.NativeTemplateInstantiatedCall("FAngelscriptMapBinds::OpAssign", false, true, true);

		TMap_.Method("bool opEquals(const TMap<K,V>& Other) const", &FAngelscriptMapBinds::OpEquals)
			.PassScriptObjectTypeAsFirstParam()
			.NativeTemplateInstantiatedCall("FAngelscriptMapBinds::OpEquals", false, true, false);

		TMap_.Method("void Empty(int32 Slack = 0)", &FAngelscriptMapBinds::Empty)
			.PassScriptObjectTypeAsFirstParam()
			.NativeTemplateInstantiatedCall("FAngelscriptMapBinds::Empty", false, false, false);

		TMap_.Method("void Reset()", &FAngelscriptMapBinds::Reset)
			.PassScriptObjectTypeAsFirstParam()
			.NativeTemplateInstantiatedCall("FAngelscriptMapBinds::Reset", false, false, false);

		TMap_.Method("void GetKeys(TArray<K>& OutKeys) const", &FAngelscriptMapBinds::GetKeys)
			.PassScriptObjectTypeAsFirstParam()
			.Documentation(TEXT("Generates a list of the keys present in the map and stores them in the given array."));

		TMap_.Method("void GetValues(TArray<V>& OutValues) const", &FAngelscriptMapBinds::GetValues)
			.PassScriptObjectTypeAsFirstParam()
			.Documentation(TEXT("Generates a list of the values present in the map and stores them in the given array."));

		auto TMapIterator_ = Binds.ExistingClassForTarget("TMapIterator<K,V>");
		TMapIterator_.Constructor("void f(const TMapIterator<K,V>& Other)", FUNC_TRIVIAL(FMapIterator::CopyConstruct));

#if AS_ITERATOR_DEBUGGING
		TMapIterator_.Destructor("void f()", &FMapIterator::Destruct);
#endif

		TMapIterator_.Method("TMapIterator<K,V>& opAssign(const TMapIterator<K,V>& Other)", METHOD_TRIVIAL(FMapIterator, Assignment));
		TMapIterator_.Property("bool CanProceed", &FMapIterator::bCanProceed);
		TMapIterator_.Method("void RemoveCurrent() const", METHOD(FMapIterator, RemoveCurrent));

		TMapIterator_.Method("const K& GetKey() const", METHOD(FMapIterator, GetKey));
		TMapIterator_.Method("V& GetValue() const", METHOD(FMapIterator, GetValue));

		TMapIterator_.Method("void SetValue(const V& NewValue) const", METHOD(FMapIterator, SetValue));
		TMapIterator_.Method("TMapIterator<K,V>& Proceed()", METHOD(FMapIterator, Proceed));

		auto TMapConstIterator_ = Binds.ExistingClassForTarget("TMapConstIterator<K,V>");
		TMapConstIterator_.Constructor("void f(const TMapConstIterator<K,V>& Other)", FUNC_TRIVIAL(FMapIterator::CopyConstruct));

#if AS_ITERATOR_DEBUGGING
		TMapConstIterator_.Destructor("void f()", &FMapIterator::Destruct);
#endif

		TMapConstIterator_.Method("TMapConstIterator<K,V>& opAssign(const TMapConstIterator<K,V>& Other)", METHOD_TRIVIAL(FMapIterator, Assignment));
		TMapConstIterator_.Property("bool CanProceed", &FMapIterator::bCanProceed);
		TMapConstIterator_.Method("const K& GetKey() const", METHOD(FMapIterator, GetKey));
		TMapConstIterator_.Method("const V& GetValue() const", METHOD(FMapIterator, GetValue));
		TMapConstIterator_.Method("TMapConstIterator<K,V>& Proceed()", METHOD(FMapIterator, Proceed));

		TMap_.Method("int opForBegin()", &FAngelscriptMapForeachBinds::Begin).PassScriptObjectTypeAsFirstParam();
		TMap_.Method("int opForBegin() const", &FAngelscriptMapForeachBinds::Begin).PassScriptObjectTypeAsFirstParam();

		TMap_.Method("bool opForEnd(const int Iterator) const", &FAngelscriptMapForeachBinds::End);

		TMap_.Method("void opForNext(int&inout Iterator)", &FAngelscriptMapForeachBinds::Next)
			.PassScriptObjectTypeAsFirstParam();
		TMap_.Method("void opForNext(int&inout Iterator) const", &FAngelscriptMapForeachBinds::Next)
			.PassScriptObjectTypeAsFirstParam();

		TMap_.Method("V& opForValue(const int Iterator)", &FAngelscriptMapForeachBinds::Value)
			.PassScriptObjectTypeAsFirstParam();
		TMap_.Method("const V& opForValue(const int Iterator) const", &FAngelscriptMapForeachBinds::Value)
			.PassScriptObjectTypeAsFirstParam();

		TMap_.Method("const K& opForKey(const int Iterator) const", &FAngelscriptMapForeachBinds::Key)
			.PassScriptObjectTypeAsFirstParam();

		TMap_.Method("TMapIterator<K,V> Iterator()", FUNC_TRIVIAL(FMapIterator::Create))
			.PassScriptObjectTypeAsFirstParam()
			.NativeTArrayIteratorCreate();

		TMap_.Method("TMapConstIterator<K,V> Iterator() const", FUNC_TRIVIAL(FMapIterator::Create))
			.PassScriptObjectTypeAsFirstParam()
			.NativeTArrayIteratorCreate();
	}

	void BindTMapTypeInfrastructure(FAngelscriptBinds& Binds)
	{
		auto MapType = MakeShared<FAngelscriptMapType>();
		Binds.RegisterTypeForTarget(MapType);

		FAngelscriptTypeDatabase* TargetTypeDatabase = &Binds.GetTargetTypeDatabase();
		Binds.RegisterTypeFinderForTarget([MapType, TargetTypeDatabase](FProperty* Property, FAngelscriptTypeUsage& Usage) -> bool
		{
			FMapProperty* MapProp = CastField<FMapProperty>(Property);
			if (MapProp == nullptr)
				return false;

			FAngelscriptTypeUsage KeyType = FAngelscriptTypeUsage::FromProperty(*TargetTypeDatabase, MapProp->KeyProp);
			if (!KeyType.IsValid())
				return false;

			FAngelscriptTypeUsage ValueType = FAngelscriptTypeUsage::FromProperty(*TargetTypeDatabase, MapProp->ValueProp);
			if (!ValueType.IsValid())
				return false;

			Usage.Type = MapType;
			Usage.SubTypes.Add(KeyType);
			Usage.SubTypes.Add(ValueType);
			return true;
		});

		Binds.RegisterTypeForTarget(MakeShared<FAngelscriptMapIteratorType>());
		Binds.RegisterTypeForTarget(MakeShared<FAngelscriptMapConstIteratorType>());
	}
}

AS_FORCE_LINK const FAngelscriptBind Bind_TMap_TypeDeclarations(
	TEXT("TMap.Declaration"),
	EAngelscriptBindPhase::TypeDeclarations,
	&BindTMapTypeDeclarations);

AS_FORCE_LINK const FAngelscriptBind Bind_TMap_MethodSurface(
	TEXT("TMap.MethodSurface"),
	EAngelscriptBindPhase::TypeInfrastructure,
	&BindTMapMethodSurface);

AS_FORCE_LINK const FAngelscriptBind Bind_TMap_TypeInfrastructure(
	TEXT("TMap.TypeInfrastructure"),
	EAngelscriptBindPhase::TypeInfrastructure,
	&BindTMapTypeInfrastructure);

bool ValidateMapOperations(asITypeInfo* TemplateType, asCString* ErrorMessage)
{
	FMapOperations* Ops = (FMapOperations*)TemplateType->GetUserData();
	if (Ops != nullptr)
		return Ops->bValid;
		
	int32 KeyTypeId = TemplateType->GetSubTypeId(0);
	int32 ValueTypeId = TemplateType->GetSubTypeId(1);

	auto KeyType = FAngelscriptTypeUsage::FromTypeId(KeyTypeId);
	auto ValueType = FAngelscriptTypeUsage::FromTypeId(ValueTypeId);

	// We don't allow containers of templated types,
	// except for TSubclassOf<> which is just an object pointer.
	if (!KeyType.CanBeTemplateSubType() || !ValueType.CanBeTemplateSubType())
	{
		if (ErrorMessage != nullptr)
			*ErrorMessage = "Containers cannot be nested in other containers";
		return false;
	}

	Ops = new FMapOperations(KeyType, ValueType);

	bool bCanHash = KeyType.CanHashValue();
	if (!bCanHash)
	{
		if (asCTypeInfo* SubType = (asCTypeInfo*)TemplateType->GetSubType(0))
		{
			auto* ObjectType = CastToObjectType(SubType);
			if (ObjectType != nullptr && ObjectType->GetFirstMethod("Hash") != nullptr)
			{
				Ops->HashFunction = FAngelscriptType::FindScriptStructHashFunction(SubType);
				bCanHash = Ops->HashFunction != nullptr;
			}
		}
	}
	
	Ops->bValid = KeyType.CanConstruct() && KeyType.CanDestruct() && KeyType.CanCopy() && KeyType.CanCompare() && bCanHash
		&& ValueType.CanConstruct() && ValueType.CanDestruct() && ValueType.CanCopy();

	TemplateType->SetUserData(Ops);

	if (!Ops->bValid && ErrorMessage != nullptr)
	{
		if (!bCanHash)
		{
			*ErrorMessage = "Key type does not have a hash function defined";
		}
		else
		{
			*ErrorMessage = "Subtype cannot be constructed or copied";
		}
	}

	return Ops->bValid;
}
