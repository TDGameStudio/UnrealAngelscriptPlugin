#include "Binds/Bind_TSet.h"
#include "AngelscriptBinds.h"
#include "AngelscriptEngine.h"

#include "ClassGenerator/ASClass.h"

#include "Containers/Set.h"
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
 * Generic TSet and iterator binding surface.
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | AngelScript usage signature                                                                | Purpose / parameter notes                                                                                            |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | template<class T> struct TSet;                                                             | Declares the generic set type.                                                                                       |
 * |                                                                                            | Each TSet<T> instantiation validates element hashing and equality operations.                                        |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | template<class T> struct TSetIterator;                                                     | Declares the mutable set iterator type.                                                                              |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | template<class T> struct TSetConstIterator;                                                | Declares the read-only set iterator type.                                                                            |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | TSet<T> Set;                                                                               | Constructs an empty set; element lifetimes are managed automatically.                                                |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | void TSet<T>.Add(const T& Value);                                                          | Adds Value when no equal element is already present.                                                                 |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | void TSet<T>.Append(const TArray<T>& Array);                                               | Adds every element from Array.                                                                                       |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | void TSet<T>.Append(const TSet<T>& Set);                                                   | Adds every element from another set.                                                                                 |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | bool TSet<T>.Contains(const T& Value) const;                                               | Returns whether an equal element is present.                                                                         |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | bool TSet<T>.Remove(const T& Value);                                                       | Removes an equal element and reports whether one existed.                                                            |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | Set = Other;                                                                               | Replaces this set with a copy of Other.                                                                              |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | Set == Other;                                                                              | Compares sets by element contents, independent of storage order.                                                     |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | void TSet<T>.Empty(int32 Slack = 0);                                                       | Removes every element and reserves optional capacity.                                                                |
 * |                                                                                            | @param Slack Desired post-clear allocation capacity.                                                                 |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | void TSet<T>.Reset();                                                                      | Removes every element while retaining reusable allocation.                                                           |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | int32 TSet<T>.Num() const;                                                                 | Returns the number of elements.                                                                                      |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | bool TSet<T>.IsEmpty() const;                                                              | Returns whether the set has no elements.                                                                             |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | TSetIterator<T> It(const TSetIterator<T>& Other);                                          | Copy-constructs a mutable iterator.                                                                                  |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | It = Other;                                                                                | Assigns mutable iterator state.                                                                                      |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | bool TSetIterator<T>.CanProceed;                                                           | Reports whether the mutable iterator refers to a valid element.                                                      |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | const T& TSetIterator<T>.Proceed();                                                        | Returns the current element and advances the iterator.                                                               |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | TSetConstIterator<T> It(const TSetConstIterator<T>& Other);                                | Copy-constructs a read-only iterator.                                                                                |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | It = Other;                                                                                | Assigns read-only iterator state.                                                                                    |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | bool TSetConstIterator<T>.CanProceed;                                                      | Reports whether the read-only iterator refers to a valid element.                                                    |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | const T& TSetConstIterator<T>.Proceed();                                                   | Returns the current element and advances the iterator.                                                               |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | for (auto Value : Set) { Use(Value); }                                                     | Iterates elements through the mutable or const opFor protocol.                                                       |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | TSetIterator<T> TSet<T>.Iterator();                                                        | Creates a mutable explicit iterator.                                                                                 |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | TSetConstIterator<T> TSet<T>.Iterator() const;                                             | Creates a read-only explicit iterator.                                                                               |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 */

#if AS_ITERATOR_DEBUGGING
static thread_local TArray<void*, TInlineAllocator<16>> GSetsBeingIterated;

static bool CheckSetIteratorDebug(FScriptSet& Set)
{
	if (GSetsBeingIterated.Contains(&Set))
	{
		FAngelscriptEngine::Throw("TSet is being modified during for loop iteration");
		return false;
	}

	return true;
}

void FSetOperations::MarkSetBeingIterated(FScriptSet& Set)
{
	GSetsBeingIterated.Add(&Set);
}

void FSetOperations::UnmarkSetBeingIterated(FScriptSet& Set)
{
	GSetsBeingIterated.RemoveSingle(&Set);
}
#endif

#if AS_REFERENCE_DEBUGGING
static void InvalidateReferencesToSet(FScriptSet& Set, FSetOperations* Ops)
{
	asCContext* Context = (asCContext*)asGetActiveContext();
	if (Context != nullptr)
	{
		Context->InvalidateReferencesToMemoryBlock(Set.GetData(0, Ops->Layout), Set.GetMaxIndex() * Ops->Layout.SparseArrayLayout.Size);
	}
}
#endif

bool ValidateSetOperations(asITypeInfo* TemplateType, asCString* ErrorMessage);

void FAngelscriptSetBinds::Destruct(FScriptSet& Set, asCObjectType* Meta)
{
   auto* Ops = FSetOperations::GetSetOperations(Meta);
   if (Ops->bNeedDestruct)
   {
   	int32 Count = Set.GetMaxIndex();
   	for (int32 i = 0; i < Count; ++i)
   	{
   		if (Set.IsValidIndex(i))
   			Ops->Type.DestructValue(Ops->GetElement(Set, i));
   	}
   }
   Set.~FScriptSet();
}

void FAngelscriptSetBinds::Add(FScriptSet& Set, asCObjectType* Meta, void* Value)
{
#if AS_ITERATOR_DEBUGGING
	if (!CheckSetIteratorDebug(Set))
		return;
#endif

	auto* Ops = FSetOperations::GetSetOperations(Meta);

#if AS_REFERENCE_DEBUGGING
	InvalidateReferencesToSet(Set, Ops);
#endif

	Ops->Add(Set, Value);
}

void FAngelscriptSetBinds::AppendArray(FScriptSet& Set, asCObjectType* Meta, FScriptArray& SourceArray)
{
#if AS_ITERATOR_DEBUGGING
	if (!CheckSetIteratorDebug(Set))
		return;
#endif

	auto* Ops = FSetOperations::GetSetOperations(Meta);

#if AS_REFERENCE_DEBUGGING
	InvalidateReferencesToSet(Set, Ops);
#endif

	for (int i = 0, Count = SourceArray.Num(); i < Count; ++i)
	{
		void* Value = (void*)((SIZE_T)SourceArray.GetData() + (i*Ops->ValueSize));
		Ops->Add(Set, Value);
	}
}

void FAngelscriptSetBinds::AppendSet(FScriptSet& Set, asCObjectType* Meta, FScriptSet& SourceSet)
{
#if AS_ITERATOR_DEBUGGING
	if (!CheckSetIteratorDebug(Set))
		return;
#endif

	auto* Ops = FSetOperations::GetSetOperations(Meta);

#if AS_REFERENCE_DEBUGGING
	InvalidateReferencesToSet(Set, Ops);
#endif

	for (int32 i = 0, Num = SourceSet.GetMaxIndex(); i < Num; ++i)
	{
		if (SourceSet.IsValidIndex(i))
			Ops->Add(Set, Ops->GetElement(SourceSet, i));
	}
}

bool FAngelscriptSetBinds::Contains(FScriptSet& Set, asCObjectType* Meta, void* Value)
{
	auto* Ops = FSetOperations::GetSetOperations(Meta);
	int32 Index = Ops->FindIndex(Set, Value);
	return Index != INDEX_NONE;
}

bool FAngelscriptSetBinds::Remove(FScriptSet& Set, asCObjectType* Meta, void* Value)
{
#if AS_ITERATOR_DEBUGGING
	if (!CheckSetIteratorDebug(Set))
		return false;
#endif

	auto* Ops = FSetOperations::GetSetOperations(Meta);

#if AS_REFERENCE_DEBUGGING
	InvalidateReferencesToSet(Set, Ops);
#endif

	int32 Index = Ops->FindIndex(Set, Value);
	if (Index == INDEX_NONE)
		return false;

	Ops->RemoveAt(Set, Index);
	return true;
}

FScriptSet& FAngelscriptSetBinds::Assign(FScriptSet& DestinationSet, asCObjectType* Meta, FScriptSet& SourceSet)
{
#if AS_ITERATOR_DEBUGGING
	if (!CheckSetIteratorDebug(DestinationSet))
		return DestinationSet;
#endif

	auto* Ops = FSetOperations::GetSetOperations(Meta);

#if AS_REFERENCE_DEBUGGING
	InvalidateReferencesToSet(DestinationSet, Ops);
#endif

	Ops->Empty(DestinationSet, SourceSet.Num());

	for (int32 i = 0, Num = SourceSet.GetMaxIndex(); i < Num; ++i)
	{
		if (SourceSet.IsValidIndex(i))
			Ops->Add(DestinationSet, Ops->GetElement(SourceSet, i));
	}

	return DestinationSet;
}

bool FAngelscriptSetBinds::OpEquals(FScriptSet& SetA, asCObjectType* Meta, FScriptSet& SetB)
{
	auto* Ops = FSetOperations::GetSetOperations(Meta);
	if (!Ops->Type.CanCompare())
	{
		FAngelscriptEngine::Throw("Cannot compare set element type for equality.");
		return false;
	}

	return Ops->IsPermutation(SetA, SetB);
}

void FAngelscriptSetBinds::Empty(FScriptSet& Set, asCObjectType* Meta, int32 Slack)
{
#if AS_ITERATOR_DEBUGGING
	if (!CheckSetIteratorDebug(Set))
		return;
#endif

	auto* Ops = FSetOperations::GetSetOperations(Meta);

#if AS_REFERENCE_DEBUGGING
	InvalidateReferencesToSet(Set, Ops);
#endif

	Ops->Empty(Set, Slack);
}

void FAngelscriptSetBinds::Reset(FScriptSet& Set, asCObjectType* Meta)
{
#if AS_ITERATOR_DEBUGGING
	if (!CheckSetIteratorDebug(Set))
		return;
#endif

	auto* Ops = FSetOperations::GetSetOperations(Meta);

#if AS_REFERENCE_DEBUGGING
	InvalidateReferencesToSet(Set, Ops);
#endif

	Ops->Empty(Set, Set.Num());
}

namespace
{
	struct FAngelscriptSetIterationBinds
	{
		static int32 OpForBegin(FScriptSet& Set, asCObjectType* Meta)
		{
			return FSetOperations::GetSetOperations(Meta)->FindNextIndex(Set, -1);
		}

		static bool OpForEnd(FScriptSet&, int32 Iterator)
		{
			return Iterator == -1;
		}

		static void OpForNext(FScriptSet& Set, asCObjectType* Meta, int32& Iterator)
		{
			if (Iterator == -1)
				return;
			Iterator = FSetOperations::GetSetOperations(Meta)->FindNextIndex(Set, Iterator);
		}

		static void* OpForValue(FScriptSet& Set, asCObjectType* Meta, int32 Iterator)
		{
			auto* Ops = FSetOperations::GetSetOperations(Meta);
			if (!Set.IsValidIndex(Iterator))
			{
				FAngelscriptEngine::Throw("Iterator out of bounds.");
				return nullptr;
			}
			return Ops->GetElement(Set, Iterator);
		}
	};

	void BindTSetTypeDeclarations(FAngelscriptBinds& Binds)
	{
		FBindFlags SetFlags;
		SetFlags.bTemplate = true;
		SetFlags.TemplateType = "<T>";
		SetFlags.ExtraFlags = asOBJ_TEMPLATE_SUBTYPE_COVARIANT;
		Binds.ValueClassForTarget<FScriptSet>("TSet<class T>", SetFlags);

		FBindFlags IteratorFlags;
		IteratorFlags.bTemplate = true;
		IteratorFlags.TemplateType = "<T>";
		Binds.ValueClassForTarget<FSetIterator>("TSetIterator<class T>", IteratorFlags);
		Binds.ValueClassForTarget<FSetIterator>("TSetConstIterator<class T>", IteratorFlags);
	}

	void BindTSetMethodSurface(FAngelscriptBinds& Binds)
	{
		auto TSet_ = Binds.ExistingClassForTarget("TSet<T>");
		TSet_.Constructor("void f()", FUNC_TRIVIAL(FAngelscriptSetBinds::Construct));

		TSet_.Destructor("void f()", &FAngelscriptSetBinds::Destruct)
			.PassScriptObjectTypeAsFirstParam()
			.NativeTemplateInstantiatedCall("FAngelscriptSetBinds::Destruct", false, false, false);

		TSet_.TemplateCallback(
			"bool f(int&in Type, int&out ErrorMessage)",
			&ValidateSetOperations);

		TSet_.Method("void Add(const T&in if_handle_then_const Value)", &FAngelscriptSetBinds::Add)
			.PassScriptObjectTypeAsFirstParam()
			.NativeTemplateInstantiatedCall("FAngelscriptSetBinds::Add", false, true, true);

		TSet_.Method("void Append(const TArray<T>& Array)", &FAngelscriptSetBinds::AppendArray)
			.PassScriptObjectTypeAsFirstParam()
			.NativeTemplateInstantiatedCall("FAngelscriptSetBinds::AppendArray", false, true, true);

		TSet_.Method("void Append(const TSet<T>& Set)", &FAngelscriptSetBinds::AppendSet)
			.PassScriptObjectTypeAsFirstParam()
			.NativeTemplateInstantiatedCall("FAngelscriptSetBinds::AppendSet", false, true, true);

		TSet_.Method("bool Contains(const T&in if_handle_then_const Value) const", &FAngelscriptSetBinds::Contains)
			.PassScriptObjectTypeAsFirstParam()
			.NativeTemplateInstantiatedCall("FAngelscriptSetBinds::Contains", true, true, false);

		TSet_.Method("bool Remove(const T&in if_handle_then_const Value)", &FAngelscriptSetBinds::Remove)
			.PassScriptObjectTypeAsFirstParam()
			.NativeTemplateInstantiatedCall("FAngelscriptSetBinds::Remove", false, true, true);

		TSet_.Method("TSet<T>& opAssign(const TSet<T>& Other)", &FAngelscriptSetBinds::Assign)
			.PassScriptObjectTypeAsFirstParam()
			.NativeTemplateInstantiatedCall("FAngelscriptSetBinds::Assign", false, true, true);

		TSet_.Method("bool opEquals(const TSet<T>& Other) const", &FAngelscriptSetBinds::OpEquals)
			.PassScriptObjectTypeAsFirstParam()
			.NativeTemplateInstantiatedCall("FAngelscriptSetBinds::OpEquals", false, true, false);

		TSet_.Method("void Empty(int32 Slack = 0)", &FAngelscriptSetBinds::Empty)
			.PassScriptObjectTypeAsFirstParam()
			.NativeTemplateInstantiatedCall("FAngelscriptSetBinds::Empty", false, false, false);

		TSet_.Method("void Reset()", &FAngelscriptSetBinds::Reset)
			.PassScriptObjectTypeAsFirstParam()
			.NativeTemplateInstantiatedCall("FAngelscriptSetBinds::Reset", false, false, false);

		TSet_.Method("int32 Num() const", FUNC_TRIVIAL(FAngelscriptSetBinds::Num));
		TSet_.Method("bool IsEmpty() const", FUNC_TRIVIAL(FAngelscriptSetBinds::IsEmpty));

		auto TSetIterator_ = Binds.ExistingClassForTarget("TSetIterator<T>");
		TSetIterator_.Constructor("void f(const TSetIterator<T>& Other)", FUNC_TRIVIAL(FSetIterator::CopyConstruct));

#if AS_ITERATOR_DEBUGGING
		TSetIterator_.Destructor("void f()", &FSetIterator::Destruct);
#endif

		TSetIterator_.Method("TSetIterator<T>& opAssign(const TSetIterator<T>& Other)", METHOD_TRIVIAL(FSetIterator, Assignment));
		TSetIterator_.Property("bool CanProceed", &FSetIterator::bCanProceed);
		TSetIterator_.Method("const T& Proceed()", METHOD(FSetIterator, Proceed));

		auto TSetConstIterator_ = Binds.ExistingClassForTarget("TSetConstIterator<T>");
		TSetConstIterator_.Constructor("void f(const TSetConstIterator<T>& Other)", FUNC_TRIVIAL(FSetIterator::CopyConstruct));

#if AS_ITERATOR_DEBUGGING
		TSetConstIterator_.Destructor("void f()", &FSetIterator::Destruct);
#endif

		TSetConstIterator_.Method("TSetConstIterator<T>& opAssign(const TSetConstIterator<T>& Other)", METHOD_TRIVIAL(FSetIterator, Assignment));
		TSetConstIterator_.Property("bool CanProceed", &FSetIterator::bCanProceed);
		TSetConstIterator_.Method("const T& Proceed()", METHOD(FSetIterator, Proceed));

		TSet_.Method("int opForBegin()", &FAngelscriptSetIterationBinds::OpForBegin).PassScriptObjectTypeAsFirstParam();
		TSet_.Method("int opForBegin() const", &FAngelscriptSetIterationBinds::OpForBegin).PassScriptObjectTypeAsFirstParam();
		TSet_.Method("bool opForEnd(const int Iterator) const", &FAngelscriptSetIterationBinds::OpForEnd);
		TSet_.Method("void opForNext(int&inout Iterator)", &FAngelscriptSetIterationBinds::OpForNext)
			.PassScriptObjectTypeAsFirstParam();
		TSet_.Method("void opForNext(int&inout Iterator) const", &FAngelscriptSetIterationBinds::OpForNext)
			.PassScriptObjectTypeAsFirstParam();
		TSet_.Method("T& opForValue(const int Iterator)", &FAngelscriptSetIterationBinds::OpForValue)
			.PassScriptObjectTypeAsFirstParam();
		TSet_.Method("const T& opForValue(const int Iterator) const", &FAngelscriptSetIterationBinds::OpForValue)
			.PassScriptObjectTypeAsFirstParam();

		TSet_.Method("TSetIterator<T> Iterator()", FUNC_TRIVIAL(FSetIterator::Create))
			.PassScriptObjectTypeAsFirstParam()
			.NativeTArrayIteratorCreate();

		TSet_.Method("TSetConstIterator<T> Iterator() const", FUNC_TRIVIAL(FSetIterator::Create))
			.PassScriptObjectTypeAsFirstParam()
			.NativeTArrayIteratorCreate();
	}

	void BindTSetTypeInfrastructure(FAngelscriptBinds& Binds)
	{
		auto SetType = MakeShared<FAngelscriptSetType>();
		Binds.RegisterTypeForTarget(SetType);

		FAngelscriptTypeDatabase* TargetTypeDatabase = &Binds.GetTargetTypeDatabase();
		Binds.RegisterTypeFinderForTarget([SetType, TargetTypeDatabase](FProperty* Property, FAngelscriptTypeUsage& Usage) -> bool
		{
			FSetProperty* SetProp = CastField<FSetProperty>(Property);
			if (SetProp == nullptr)
				return false;

			FAngelscriptTypeUsage InnerUsage = FAngelscriptTypeUsage::FromProperty(*TargetTypeDatabase, SetProp->ElementProp);
			if (!InnerUsage.IsValid())
				return false;

			Usage.Type = SetType;
			Usage.SubTypes.Add(InnerUsage);
			return true;
		});

		Binds.RegisterTypeForTarget(MakeShared<FAngelscriptSetIteratorType>());
		Binds.RegisterTypeForTarget(MakeShared<FAngelscriptSetConstIteratorType>());
	}
}

AS_FORCE_LINK const FAngelscriptBind Bind_TSet_TypeDeclarations(
	TEXT("TSet.Declaration"),
	EAngelscriptBindPhase::TypeDeclarations,
	&BindTSetTypeDeclarations);

AS_FORCE_LINK const FAngelscriptBind Bind_TSet_MethodSurface(
	TEXT("TSet.MethodSurface"),
	EAngelscriptBindPhase::TypeInfrastructure,
	&BindTSetMethodSurface);

AS_FORCE_LINK const FAngelscriptBind Bind_TSet_TypeInfrastructure(
	TEXT("TSet.TypeInfrastructure"),
	EAngelscriptBindPhase::TypeInfrastructure,
	&BindTSetTypeInfrastructure);

bool ValidateSetOperations(asITypeInfo* TemplateType, asCString* ErrorMessage)
{
	FSetOperations* Ops = (FSetOperations*)TemplateType->GetUserData();
	if (Ops != nullptr)
		return Ops->bValid;
		
	int32 SubTypeId = TemplateType->GetSubTypeId(0);
	auto Type = FAngelscriptTypeUsage::FromTypeId(SubTypeId);

	// We don't allow containers of templated types,
	if (!Type.CanBeTemplateSubType())
	{
		if (ErrorMessage != nullptr)
			*ErrorMessage = "Containers cannot be nested in other containers";
		return false;
	}

	Ops = new FSetOperations(Type);

	bool bCanHash = Type.CanHashValue();
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

	Ops->bValid = Type.CanConstruct() && Type.CanDestruct() && Type.CanCopy() && Type.CanCompare() && bCanHash;
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
