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

struct FAngelscriptMapType : public FAngelscriptType
{
	FString GetAngelscriptTypeName() const override
	{
		return TEXT("TMap");
	}

	virtual bool CanQueryPropertyType() const { return false; }
	virtual bool CanBeTemplateSubType() const override { return false; }

	bool HasReferences(const FAngelscriptTypeUsage& Usage) const override
	{
		if (Usage.SubTypes.Num() != 2)
			return false;
		return Usage.SubTypes[0].HasReferences() || Usage.SubTypes[1].HasReferences();
	}

	void EmitReferenceInfo(const FAngelscriptTypeUsage& Usage, FGCReferenceParams& Params) const override
	{
		check(HasReferences(Usage));

		auto MapLayout = FScriptMap::GetScriptLayout(
			Usage.SubTypes[0].GetValueSize(),
			Usage.SubTypes[0].GetValueAlignment(),
			Usage.SubTypes[1].GetValueSize(),
			Usage.SubTypes[1].GetValueAlignment()
		);

		UE::GC::FSchemaBuilder InnerSchema(MapLayout.SetLayout.Size);
		if (Usage.SubTypes[0].HasReferences())
		{
			FGCReferenceParams InnerParams = Params;
			InnerParams.Schema = &InnerSchema;
			InnerParams.AtOffset = 0;
			Usage.SubTypes[0].EmitReferenceInfo(InnerParams);
		}
		
		if (Usage.SubTypes[1].HasReferences())
		{
			FGCReferenceParams InnerParams = Params;
			InnerParams.Schema = &InnerSchema;
			InnerParams.AtOffset = MapLayout.ValueOffset;
			Usage.SubTypes[1].EmitReferenceInfo(InnerParams);
		}
		
		Params.Schema->Add(UE::GC::DeclareMember(Params.Names.Top(), Params.AtOffset, UE::GC::EMemberType::StructSet, InnerSchema.Build()));
	}

	bool CanCreateProperty(const FAngelscriptTypeUsage& Usage) const override
	{
		if (Usage.SubTypes.Num() != 2)
			return false;
		return Usage.SubTypes[0].CanCreateProperty() && Usage.SubTypes[0].CanHashValue()
			&& Usage.SubTypes[1].CanCreateProperty();
	}

	FProperty* CreateProperty(const FAngelscriptTypeUsage& Usage, const FPropertyParams& Params) const override
	{
		auto* MapProp = new FMapProperty(Params.Outer, Params.PropertyName);

		{
			FPropertyParams InnerParams = Params;
			InnerParams.Outer = MapProp;
			InnerParams.PropertyName = *(Params.PropertyName.ToString() + TEXT("_Key"));

			MapProp->KeyProp = Usage.SubTypes[0].CreateProperty(InnerParams);
		}

		{
			FPropertyParams InnerParams = Params;
			InnerParams.Outer = MapProp;
			InnerParams.PropertyName = *(Params.PropertyName.ToString() + TEXT("_Value"));

			MapProp->ValueProp = Usage.SubTypes[1].CreateProperty(InnerParams);
		}

		MapProp->MapLayout = FScriptMap::GetScriptLayout(
			Usage.SubTypes[0].GetValueSize(),
			Usage.SubTypes[0].GetValueAlignment(),
			Usage.SubTypes[1].GetValueSize(),
			Usage.SubTypes[1].GetValueAlignment()
		);

		return MapProp;
	}

	bool MatchesProperty(const FAngelscriptTypeUsage& Usage, const FProperty* Property, EPropertyMatchType MatchType) const override
	{
		if (Usage.SubTypes.Num() != 2)
			return false;

		const FMapProperty* MapProp = CastField<FMapProperty>(Property);
		if (MapProp == nullptr)
			return false;

		return Usage.SubTypes[0].MatchesProperty(MapProp->GetKeyProperty(), FAngelscriptType::EPropertyMatchType::InContainer)
		&& Usage.SubTypes[1].MatchesProperty(MapProp->GetValueProperty(), FAngelscriptType::EPropertyMatchType::InContainer);
	}

	bool CanCopy(const FAngelscriptTypeUsage& Usage) const override
	{
		return Usage.SubTypes.Num() == 2
			&& Usage.SubTypes[0].CanCopy()
			&& Usage.SubTypes[0].CanConstruct()
			&& Usage.SubTypes[0].CanDestruct()
			&& Usage.SubTypes[1].CanCopy()
			&& Usage.SubTypes[1].CanConstruct()
			&& Usage.SubTypes[1].CanDestruct();
	}

	bool NeedCopy(const FAngelscriptTypeUsage& Usage) const override { return true; }
	void CopyValue(const FAngelscriptTypeUsage& Usage, void* SourcePtr, void* DestinationPtr) const override
	{
		FScriptMap& Source = *(FScriptMap*)SourcePtr;
		FScriptMap& Destination = *(FScriptMap*)DestinationPtr;

		FMapOperations Ops(Usage.SubTypes[0], Usage.SubTypes[1]);
		Ops.Empty(Destination, Source.Num());
		for (int32 i = 0, Num = Source.GetMaxIndex(); i < Num; ++i)
		{
			if (Source.IsValidIndex(i))
				Ops.Add(Destination, Ops.GetKey(Source, i), Ops.GetValue(Source, i));
		}
	}

	bool CanConstruct(const FAngelscriptTypeUsage& Usage) const override
	{
		return Usage.SubTypes.Num() == 2 && Usage.SubTypes[0].CanConstruct() && Usage.SubTypes[1].CanConstruct();
	}
	bool NeedConstruct(const FAngelscriptTypeUsage& Usage) const override { return true; }
	void ConstructValue(const FAngelscriptTypeUsage& Usage, void* DestinationPtr) const override
	{
		new(DestinationPtr) FScriptMap();
	}

	bool CanDestruct(const FAngelscriptTypeUsage& Usage) const override
	{
		return Usage.SubTypes.Num() == 2 && Usage.SubTypes[0].CanDestruct() && Usage.SubTypes[1].CanDestruct();
	}
	bool NeedDestruct(const FAngelscriptTypeUsage& Usage) const override { return true; }
	void DestructValue(const FAngelscriptTypeUsage& Usage, void* DestinationPtr) const override
	{
		FScriptMap& Destination = *(FScriptMap*)DestinationPtr;

		FMapOperations Ops(Usage.SubTypes[0], Usage.SubTypes[1]);
		Ops.Empty(Destination, 0);

		Destination.~FScriptMap();
	}

	int32 GetValueSize(const FAngelscriptTypeUsage& Usage) const override
	{
		return sizeof(FScriptMap);
	}

	int32 GetValueAlignment(const FAngelscriptTypeUsage& Usage) const
	{
		return alignof(FScriptMap);
	}

	bool CanBeArgument(const FAngelscriptTypeUsage& Usage) const override { return true; }
	void SetArgument(const FAngelscriptTypeUsage& Usage, int32 ArgumentIndex, class asIScriptContext* Context, struct FFrame& Stack, const FArgData& Data) const override
	{
		FScriptMap* Arg = (FScriptMap*)Data.StackPtr;
		new(Arg) FScriptMap();

		if (Usage.bIsReference)
		{
			FScriptMap& Ref = Stack.StepCompiledInRef<FMapProperty,FScriptMap>(Arg);
			Context->SetArgAddress(ArgumentIndex, &Ref);
		}
		else
		{
			Stack.StepCompiledIn<FMapProperty>(Arg);
			Context->SetArgObject(ArgumentIndex, Arg);
		}
	}

	bool CanBeReturned(const FAngelscriptTypeUsage& Usage) const override
	{
		return true;
	}

	void GetReturnValue(const FAngelscriptTypeUsage& Usage, class asIScriptContext* Context, void* Destination) const override
	{
		if (Usage.bIsReference)
		{
			*(void**)Destination = Context->GetReturnAddress();
		}
		else
		{
			void* ReturnedObject = Context->GetReturnObject();
			if (ReturnedObject == nullptr)
				return;
			CopyValue(Usage, ReturnedObject, Destination);
		}
	}

	bool GetDebuggerValue(const FAngelscriptTypeUsage& Usage, void* Address, struct FDebuggerValue& Value) const override
	{
		if (Usage.SubTypes.Num() != 2)
			return false;

		const FAngelscriptTypeUsage& KeyType = Usage.SubTypes[0];
		const FAngelscriptTypeUsage& ValueType = Usage.SubTypes[1];

		FScriptMap& Map = Usage.ResolvePrimitive<FScriptMap>(Address);

		Value.Usage = Usage;
		Value.Usage.TypeIndex = 0;
		Value.Address = Address;
		Value.bHasMembers = true;
		Value.Type = Usage.GetAngelscriptDeclaration();

		int32 Num = Map.Num();
		if (Num == 0)
		{
			Value.Value = TEXT("Empty");
		}
		else
		{
			Value.Value = FString::Printf(TEXT("Num = %d"), Num);
		}

		return true;
	}

	bool GetDebuggerScope(const FAngelscriptTypeUsage& Usage, void* Address, struct FDebuggerScope& Scope) const override
	{
		if (Usage.SubTypes.Num() != 2)
			return false;

		const FAngelscriptTypeUsage& KeyType = Usage.SubTypes[0];
		const FAngelscriptTypeUsage& ValueType = Usage.SubTypes[1];

		FScriptMap& Map = Usage.ResolvePrimitive<FScriptMap>(Address);

		FMapOperations Ops(KeyType, ValueType);

		if (Usage.TypeIndex != 0)
		{
			// We're looking inside a key,value pair
			int32 MapIndex = Usage.TypeIndex - 1;
			if (!Map.IsValidIndex(MapIndex))
				return false;

			void* ValuePtr = Ops.GetValue(Map, MapIndex);
			void* KeyPtr = Ops.GetKey(Map, MapIndex);

			FDebuggerValue KeyValue;
			if (KeyType.GetDebuggerValue(KeyPtr, KeyValue))
			{
				KeyValue.Name = TEXT("Key");
				Scope.Values.Add(MoveTemp(KeyValue));
			}

			FDebuggerValue ElemValue;
			if (ValueType.GetDebuggerValue(ValuePtr, ElemValue))
			{
				ElemValue.Name = TEXT("Value");
				Scope.Values.Add(MoveTemp(ElemValue));
			}

			return true;
		}

		for (int32 i = 0, Num = Map.GetMaxIndex(); i < Num; ++i)
		{
			if (!Map.IsValidIndex(i))
				continue;

			void* ValuePtr = Ops.GetValue(Map, i);
			void* KeyPtr = Ops.GetKey(Map, i);

			FDebuggerValue ElemValue;
			if (ValueType.GetDebuggerValue(ValuePtr, ElemValue))
			{
				if (KeyType.GetStringIdentifier(KeyPtr, ElemValue.Name))
				{
					ElemValue.Name = FString::Printf(TEXT("[%s]"), *ElemValue.Name);
					Scope.Values.Add(MoveTemp(ElemValue));
				}
				else
				{
					FDebuggerValue KeyValue;
					if (KeyType.GetDebuggerValue(KeyPtr, KeyValue))
					{
						FDebuggerValue PairValue;
						PairValue.Type = FString::Printf(TEXT("<%s,%s>"), *KeyValue.Type, *ElemValue.Type);
						PairValue.Name = FString::Printf(TEXT("[%d]"), i);
						PairValue.Value = FString::Printf(TEXT("%s: %s"), *KeyValue.Value, *ElemValue.Value);
						PairValue.Address = Address;
						PairValue.Usage = Usage;
						PairValue.Usage.TypeIndex = 1 + i;
						PairValue.bHasMembers = true;

						Scope.Values.Add(MoveTemp(PairValue));
					}
				}
			}
		}

		{
			FDebuggerValue NumValue;
			NumValue.Name = TEXT("Num");
			NumValue.Type = TEXT("int");
			NumValue.Value = LexToString(Map.Num());
			Scope.Values.Add(MoveTemp(NumValue));
		}

		return true;
	}

	bool GetDebuggerMember(const FAngelscriptTypeUsage& Usage, void* Address, const FString& Member, struct FDebuggerValue& Value) const override
	{
		if (Usage.SubTypes.Num() != 2)
			return false;

		const FAngelscriptTypeUsage& KeyType = Usage.SubTypes[0];
		const FAngelscriptTypeUsage& ValueType = Usage.SubTypes[1];

		FScriptMap& Map = Usage.ResolvePrimitive<FScriptMap>(Address);

		FMapOperations Ops(KeyType, ValueType);

		if (Usage.TypeIndex != 0)
		{
			// We're looking inside a key,value pair
			int32 MapIndex = Usage.TypeIndex - 1;
			if (!Map.IsValidIndex(MapIndex))
				return false;

			if (Member == TEXT("Key"))
			{
				void* KeyPtr = Ops.GetKey(Map, MapIndex);
				if (KeyType.GetDebuggerValue(KeyPtr, Value))
				{
					Value.Name = TEXT("Key");
					return true;
				}

				return false;
			}
			else if (Member == TEXT("Value"))
			{
				void* ValuePtr = Ops.GetValue(Map, MapIndex);
				if (ValueType.GetDebuggerValue(ValuePtr, Value))
				{
					Value.Name = TEXT("Value");
					return true;
				}

				return false;
			}
		}
		else
		{
			if (Member == TEXT("Num"))
			{
				Value.Name = TEXT("Num");
				Value.Type = TEXT("int");
				Value.Value = LexToString(Map.Num());
				return true;
			}
			else if (Member.StartsWith(TEXT("[")) && Member.EndsWith(TEXT("]")))
			{
				FString Identifier = Member.Mid(1, Member.Len() - 2);

				int32 Index = -1;

				void* KeyBuffer = (void*)FMemory_Alloca(Ops.KeySize);
				bool bHasKeyBuffer = false;
				if (KeyType.FromStringIdentifier(Identifier, KeyBuffer))
				{
					Index = Ops.FindPairIndex(Map, KeyBuffer);
					bHasKeyBuffer = true;
				}
				else
				{
					LexFromString(Index, *Identifier);
				}

				bool bValidValue = false;
				if (Map.IsValidIndex(Index))
				{
					void* ValuePtr = Ops.GetValue(Map, Index);
					void* KeyPtr = Ops.GetKey(Map, Index);

					FDebuggerValue ElemValue;
					if (ValueType.GetDebuggerValue(ValuePtr, ElemValue))
					{
						if (KeyType.GetStringIdentifier(KeyPtr, ElemValue.Name))
						{
							ElemValue.Name = FString::Printf(TEXT("[%s]"), *ElemValue.Name);
							Value = MoveTemp(ElemValue);

							bValidValue = true;
						}
						else
						{
							FDebuggerValue KeyValue;
							if (KeyType.GetDebuggerValue(KeyPtr, KeyValue))
							{
								Value.Type = FString::Printf(TEXT("<%s,%s>"), *KeyValue.Type, *ElemValue.Type);
								Value.Name = FString::Printf(TEXT("[%d]"), Index);
								Value.Value = FString::Printf(TEXT("%s: %s"), *KeyValue.Value, *ElemValue.Value);
								Value.Address = Address;
								Value.Usage = Usage;
								Value.Usage.TypeIndex = 1 + Index;
								Value.bHasMembers = true;

								bValidValue = true;
							}
						}
					}
				}

				if(bHasKeyBuffer && KeyType.CanDestruct() && KeyType.NeedDestruct())
					KeyType.DestructValue(KeyBuffer);

				return bValidValue;
			}
		}

		return false;
	}

	bool GetCppForm(const FAngelscriptTypeUsage& Usage, FCppForm& OutCppForm) const override
	{
		if (Usage.SubTypes.Num() != 2)
			return false;

		FCppForm CppInnerKey;
		FCppForm CppInnerValue;
		if (Usage.SubTypes[0].GetCppForm(CppInnerKey) && Usage.SubTypes[1].GetCppForm(CppInnerValue))
		{
			if (CppInnerKey.CppType.Len() != 0 && CppInnerValue.CppType.Len() != 0
				&& !CppInnerKey.bDisallowNativeNest && !CppInnerValue.bDisallowNativeNest)
			{
				OutCppForm.CppType = FString::Printf(TEXT("TMap<%s,%s>"), *CppInnerKey.CppType, *CppInnerValue.CppType);
				OutCppForm.CppHeader = CppInnerKey.CppHeader + TEXT("\n") + CppInnerValue.CppHeader;
			}

			FString KeyGeneric = CppInnerKey.CppGenericType.Len() != 0 ? CppInnerKey.CppGenericType : CppInnerKey.CppType;
			FString ValueGeneric = CppInnerValue.CppGenericType.Len() != 0 ? CppInnerValue.CppGenericType : CppInnerValue.CppType;

			if (KeyGeneric.Len() != 0 && ValueGeneric.Len() != 0)
			{
				OutCppForm.CppGenericType = FString::Printf(TEXT("TMap<%s,%s>"), *KeyGeneric, *ValueGeneric);
			}
		}

		OutCppForm.TemplateObjectForm = TEXT("FScriptMap");
		return true;
	}

	bool CanCompare(const FAngelscriptTypeUsage& Usage) const override
	{
		return Usage.SubTypes.Num() == 2 && Usage.SubTypes[0].CanCompare() && Usage.SubTypes[1].CanCompare();
	}

	bool IsValueEqual(const FAngelscriptTypeUsage& Usage, void* SourcePtr, void* DestinationPtr) const override
	{
		const FAngelscriptTypeUsage& KeyType = Usage.SubTypes[0];
		const FAngelscriptTypeUsage& ValueType = Usage.SubTypes[1];

		FScriptMap& SourceMap = *(FScriptMap*)SourcePtr;
		FScriptMap& DestMap = *(FScriptMap*)DestinationPtr;

		FMapOperations Ops(KeyType, ValueType);
		return Ops.IsPermutation(SourceMap, DestMap);
	}

};

struct FAngelscriptMapIteratorType : public FAngelscriptType
{
	FString GetAngelscriptTypeName() const override
	{
		return TEXT("TMapIterator");
	}

	int32 GetValueSize(const FAngelscriptTypeUsage& Usage) const override
	{
		return sizeof(FMapIterator);
	}

	int32 GetValueAlignment(const FAngelscriptTypeUsage& Usage) const override
	{
		return alignof(FMapIterator);
	}

	bool GetDebuggerValue(const FAngelscriptTypeUsage& Usage, void* Address, struct FDebuggerValue& Value) const override
	{
		FMapIterator& MapIterator = Usage.ResolvePrimitive<FMapIterator>(Address);
		Value.Usage = Usage;
		Value.Usage.TypeIndex = 0;
		Value.Address = Address;
		Value.bHasMembers = true;
		Value.Type = Usage.GetAngelscriptDeclaration();

		if (Usage.SubTypes.Num() == 2)
		{
			const FAngelscriptTypeUsage& KeyType = Usage.SubTypes[0];
			const FAngelscriptTypeUsage& ValueType = Usage.SubTypes[1];
			Value.Value = FString::Printf(TEXT("%s → %s"), *KeyType.GetAngelscriptDeclaration(), *ValueType.GetAngelscriptDeclaration());
		}

		return true;
	}

	bool GetDebuggerScope(const FAngelscriptTypeUsage& Usage, void* Address, struct FDebuggerScope& Scope) const override
	{
		if (Usage.SubTypes.Num() != 2)
			return false;

		FMapIterator& MapIterator = Usage.ResolvePrimitive<FMapIterator>(Address);
		if (MapIterator.Map == nullptr)
			return false;

		const int32 MapIndex = MapIterator.Index;
		if (!MapIterator.Map->IsValidIndex(MapIndex))
			return false;

		const FAngelscriptTypeUsage& KeyType = Usage.SubTypes[0];
		const FAngelscriptTypeUsage& ValueType = Usage.SubTypes[1];
		FMapOperations Ops(KeyType, ValueType);

		void* KeyPtr = Ops.GetKey(*MapIterator.Map, MapIndex);
		void* ValuePtr = Ops.GetValue(*MapIterator.Map, MapIndex);

		FDebuggerValue KeyValue;
		if (KeyType.GetDebuggerValue(KeyPtr, KeyValue))
		{
			KeyValue.Name = TEXT("Key");
			Scope.Values.Add(MoveTemp(KeyValue));
		}

		FDebuggerValue ElemValue;
		if (ValueType.GetDebuggerValue(ValuePtr, ElemValue))
		{
			ElemValue.Name = TEXT("Value");
			Scope.Values.Add(MoveTemp(ElemValue));
		}

		return true;
	}

	bool GetDebuggerMember(const FAngelscriptTypeUsage& Usage, void* Address, const FString& Member, struct FDebuggerValue& Value) const override
	{
		if (Usage.SubTypes.Num() != 2)
			return false;

		FMapIterator& MapIterator = Usage.ResolvePrimitive<FMapIterator>(Address);
		if (MapIterator.Map == nullptr)
			return false;

		const int32 MapIndex = MapIterator.Index;
		if (!MapIterator.Map->IsValidIndex(MapIndex))
			return false;

		const FAngelscriptTypeUsage& KeyType = Usage.SubTypes[0];
		const FAngelscriptTypeUsage& ValueType = Usage.SubTypes[1];
		FMapOperations Ops(KeyType, ValueType);

		if (Member == TEXT("Key"))
		{
			void* KeyPtr = Ops.GetKey(*MapIterator.Map, MapIndex);
			if (KeyType.GetDebuggerValue(KeyPtr, Value))
			{
				Value.Name = TEXT("Key");
				return true;
			}
			return false;
		}

		if (Member == TEXT("Value"))
		{
			void* ValuePtr = Ops.GetValue(*MapIterator.Map, MapIndex);
			if (ValueType.GetDebuggerValue(ValuePtr, Value))
			{
				Value.Name = TEXT("Value");
				return true;
			}
			return false;
		}

		return false;
	}

	bool GetCppForm(const FAngelscriptTypeUsage& Usage, FCppForm& OutCppForm) const override
	{
		OutCppForm.TemplateObjectForm = TEXT("FMapIterator");
		return true;
	}
};

struct FAngelscriptMapConstIteratorType : public FAngelscriptType
{
	FString GetAngelscriptTypeName() const override
	{
		return TEXT("TMapConstIterator");
	}

	int32 GetValueSize(const FAngelscriptTypeUsage& Usage) const override
	{
		return sizeof(FMapIterator);
	}

	int32 GetValueAlignment(const FAngelscriptTypeUsage& Usage) const override
	{
		return alignof(FMapIterator);
	}

	bool GetDebuggerValue(const FAngelscriptTypeUsage& Usage, void* Address, struct FDebuggerValue& Value) const override
	{
		FMapIterator& MapIterator = Usage.ResolvePrimitive<FMapIterator>(Address);
		Value.Usage = Usage;
		Value.Usage.TypeIndex = 0;
		Value.Address = Address;
		Value.bHasMembers = true;
		Value.Type = Usage.GetAngelscriptDeclaration();

		if (Usage.SubTypes.Num() == 2)
		{
			const FAngelscriptTypeUsage& KeyType = Usage.SubTypes[0];
			const FAngelscriptTypeUsage& ValueType = Usage.SubTypes[1];
			Value.Value = FString::Printf(TEXT("const %s → %s"), *KeyType.GetAngelscriptDeclaration(), *ValueType.GetAngelscriptDeclaration());
		}

		return true;
	}

	bool GetDebuggerScope(const FAngelscriptTypeUsage& Usage, void* Address, struct FDebuggerScope& Scope) const override
	{
		if (Usage.SubTypes.Num() != 2)
			return false;

		FMapIterator& MapIterator = Usage.ResolvePrimitive<FMapIterator>(Address);
		if (MapIterator.Map == nullptr)
			return false;

		const int32 MapIndex = MapIterator.Index;
		if (!MapIterator.Map->IsValidIndex(MapIndex))
			return false;

		const FAngelscriptTypeUsage& KeyType = Usage.SubTypes[0];
		const FAngelscriptTypeUsage& ValueType = Usage.SubTypes[1];
		FMapOperations Ops(KeyType, ValueType);

		void* KeyPtr = Ops.GetKey(*MapIterator.Map, MapIndex);
		void* ValuePtr = Ops.GetValue(*MapIterator.Map, MapIndex);

		FDebuggerValue KeyValue;
		if (KeyType.GetDebuggerValue(KeyPtr, KeyValue))
		{
			KeyValue.Name = TEXT("Key");
			Scope.Values.Add(MoveTemp(KeyValue));
		}

		FDebuggerValue ElemValue;
		if (ValueType.GetDebuggerValue(ValuePtr, ElemValue))
		{
			ElemValue.Name = TEXT("Value");
			Scope.Values.Add(MoveTemp(ElemValue));
		}

		return true;
	}

	bool GetDebuggerMember(const FAngelscriptTypeUsage& Usage, void* Address, const FString& Member, struct FDebuggerValue& Value) const override
	{
		if (Usage.SubTypes.Num() != 2)
			return false;

		FMapIterator& MapIterator = Usage.ResolvePrimitive<FMapIterator>(Address);
		if (MapIterator.Map == nullptr)
			return false;

		const int32 MapIndex = MapIterator.Index;
		if (!MapIterator.Map->IsValidIndex(MapIndex))
			return false;

		const FAngelscriptTypeUsage& KeyType = Usage.SubTypes[0];
		const FAngelscriptTypeUsage& ValueType = Usage.SubTypes[1];
		FMapOperations Ops(KeyType, ValueType);

		if (Member == TEXT("Key"))
		{
			void* KeyPtr = Ops.GetKey(*MapIterator.Map, MapIndex);
			if (KeyType.GetDebuggerValue(KeyPtr, Value))
			{
				Value.Name = TEXT("Key");
				return true;
			}
			return false;
		}

		if (Member == TEXT("Value"))
		{
			void* ValuePtr = Ops.GetValue(*MapIterator.Map, MapIndex);
			if (ValueType.GetDebuggerValue(ValuePtr, Value))
			{
				Value.Name = TEXT("Value");
				return true;
			}
			return false;
		}

		return false;
	}

	bool GetCppForm(const FAngelscriptTypeUsage& Usage, FCppForm& OutCppForm) const override
	{
		OutCppForm.TemplateObjectForm = TEXT("FMapIterator");
		return true;
	}
};

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

AS_FORCE_LINK const FAngelscriptBinds::FBind Bind_TMap((int)FAngelscriptBinds::EOrder::Early+1, []
{
	FBindFlags Flags;
	Flags.bTemplate = true;
	Flags.TemplateType = "<K,V>";
	Flags.ExtraFlags = asOBJ_TEMPLATE_SUBTYPE_COVARIANT;

	auto TMap_ = FAngelscriptBinds::ValueClass<FScriptMap>("TMap<class K, class V>", Flags);
	TMap_.Constructor("void f()", FUNC_TRIVIAL(FAngelscriptMapBinds::Construct));

	TMap_.Destructor("void f()", &FAngelscriptMapBinds::Destruct);
	SCRIPT_NATIVE_TEMPLATED_CALL(TMap_, "FAngelscriptMapBinds::Destruct", false);
	FAngelscriptBinds::PreviousBindPassScriptObjectTypeAsFirstParam();

	TMap_.TemplateCallback("bool f(int&in Type, int&out ErrorMessage)",
		[](asITypeInfo* TemplateType, asCString* ErrorMessage) -> bool
	{
		return ValidateMapOperations(TemplateType, ErrorMessage);
	});

	TMap_.Method("V& opIndex(const K&in if_handle_then_const Key)", &FAngelscriptMapBinds::OpIndex);
	SCRIPT_NATIVE_TEMPLATED_CALL_NEEDSCOMPARE(TMap_, "FAngelscriptMapBinds::OpIndex", false);
	FAngelscriptBinds::PreviousBindPassScriptObjectTypeAsFirstParam();

	TMap_.Method("const V& opIndex(const K&in if_handle_then_const Key) const", &FAngelscriptMapBinds::OpIndex);
	SCRIPT_NATIVE_TEMPLATED_CALL_NEEDSCOMPARE(TMap_, "FAngelscriptMapBinds::OpIndex", false);
	FAngelscriptBinds::PreviousBindPassScriptObjectTypeAsFirstParam();

	TMap_.Method("void Add(const K&in if_handle_then_const Key, const V&in if_handle_then_const Value)", &FAngelscriptMapBinds::Add);
	SCRIPT_NATIVE_TEMPLATED_CALL_NEEDSCOPY_NEEDSCOMPARE(TMap_, "FAngelscriptMapBinds::Add", false);
	FAngelscriptBinds::PreviousBindPassScriptObjectTypeAsFirstParam();

	TMap_.Method("bool Contains(const K&in if_handle_then_const Key) const", &FAngelscriptMapBinds::Contains);
	SCRIPT_NATIVE_TEMPLATED_CALL_NEEDSCOMPARE(TMap_, "FAngelscriptMapBinds::Contains", true);
	FAngelscriptBinds::PreviousBindPassScriptObjectTypeAsFirstParam();

	TMap_.Method("bool RemoveAndCopyValue(const K&in if_handle_then_const Key, V&out OutValue)", &FAngelscriptMapBinds::RemoveAndCopyValue);
	SCRIPT_NATIVE_TEMPLATED_CALL_NEEDSCOPY_NEEDSCOMPARE(TMap_, "FAngelscriptMapBinds::RemoveAndCopyValue", false);
	FAngelscriptBinds::PreviousBindPassScriptObjectTypeAsFirstParam();

	TMap_.Method("bool Remove(const K&in if_handle_then_const Key)", &FAngelscriptMapBinds::Remove);
	SCRIPT_NATIVE_TEMPLATED_CALL_NEEDSCOPY_NEEDSCOMPARE(TMap_, "FAngelscriptMapBinds::Remove", true);
	FAngelscriptBinds::PreviousBindPassScriptObjectTypeAsFirstParam();

	TMap_.Method("int32 Num() const", FUNC_TRIVIAL(FAngelscriptMapBinds::Num));
	TMap_.Method("bool IsEmpty() const", FUNC_TRIVIAL(FAngelscriptMapBinds::IsEmpty));

	TMap_.Method("V& FindOrAdd(const K&in if_handle_then_const Key)", FUNC(FAngelscriptMapBinds::FindOrAdd_Defaulted));
	FAngelscriptBinds::PreviousBindPassScriptObjectTypeAsFirstParam();
	SCRIPT_BIND_DOCUMENTATION("Find the value associated with the key. If none exists, add and return a new value using the default constructor.")

	TMap_.Method("V& FindOrAdd(const K&in if_handle_then_const Key, const V&in if_handle_then_const DefaultValue)", &FAngelscriptMapBinds::FindOrAdd);
	SCRIPT_NATIVE_TEMPLATED_CALL(TMap_, "FAngelscriptMapBinds::FindOrAdd", false);
	FAngelscriptBinds::PreviousBindPassScriptObjectTypeAsFirstParam();
	SCRIPT_BIND_DOCUMENTATION("Find the value associated with the key. If none exists, add and return new value set to DefaultValue.")

	TMap_.Method("bool Find(const K&in if_handle_then_const Key, V&out OutValue) const", &FAngelscriptMapBinds::Find);
	SCRIPT_NATIVE_TEMPLATED_CALL_NEEDSCOPY_NEEDSCOMPARE(TMap_, "FAngelscriptMapBinds::Find", false);
	FAngelscriptBinds::PreviousBindPassScriptObjectTypeAsFirstParam();
	SCRIPT_BIND_DOCUMENTATION("Find the value associated with the key. If none exists, return false. Copies the found value to OutValue.")

	TMap_.Method("TMap<K,V>& opAssign(const TMap<K,V>& Other)", &FAngelscriptMapBinds::OpAssign);
	SCRIPT_NATIVE_TEMPLATED_CALL_NEEDSCOPY_NEEDSCOMPARE(TMap_, "FAngelscriptMapBinds::OpAssign", false);
	FAngelscriptBinds::PreviousBindPassScriptObjectTypeAsFirstParam();

	TMap_.Method("bool opEquals(const TMap<K,V>& Other) const", &FAngelscriptMapBinds::OpEquals);
	SCRIPT_NATIVE_TEMPLATED_CALL_NEEDSCOMPARE(TMap_, "FAngelscriptMapBinds::OpEquals", false);
	FAngelscriptBinds::PreviousBindPassScriptObjectTypeAsFirstParam();

	TMap_.Method("void Empty(int32 Slack = 0)", &FAngelscriptMapBinds::Empty);
	SCRIPT_NATIVE_TEMPLATED_CALL(TMap_, "FAngelscriptMapBinds::Empty", false);
	FAngelscriptBinds::PreviousBindPassScriptObjectTypeAsFirstParam();

	TMap_.Method("void Reset()", &FAngelscriptMapBinds::Reset);
	SCRIPT_NATIVE_TEMPLATED_CALL(TMap_, "FAngelscriptMapBinds::Reset", false);
	FAngelscriptBinds::PreviousBindPassScriptObjectTypeAsFirstParam();

	TMap_.Method("void GetKeys(TArray<K>& OutKeys) const", &FAngelscriptMapBinds::GetKeys);
	FAngelscriptBinds::PreviousBindPassScriptObjectTypeAsFirstParam();
	SCRIPT_BIND_DOCUMENTATION("Generates a list of the keys present in the map and stores them in the given array.")

	TMap_.Method("void GetValues(TArray<V>& OutValues) const", &FAngelscriptMapBinds::GetValues);
	FAngelscriptBinds::PreviousBindPassScriptObjectTypeAsFirstParam();
	SCRIPT_BIND_DOCUMENTATION("Generates a list of the values present in the map and stores them in the given array.")

	auto MapType = MakeShared<FAngelscriptMapType>();
	FAngelscriptType::Register(MapType);

	FAngelscriptType::RegisterTypeFinder([MapType](FProperty* Property, FAngelscriptTypeUsage& Usage) -> bool
	{
		FMapProperty* MapProp = CastField<FMapProperty>(Property);
		if (MapProp == nullptr)
			return false;

		FAngelscriptTypeUsage KeyType = FAngelscriptTypeUsage::FromProperty(MapProp->KeyProp);
		if (!KeyType.IsValid())
			return false;

		FAngelscriptTypeUsage ValueType = FAngelscriptTypeUsage::FromProperty(MapProp->ValueProp);
		if (!ValueType.IsValid())
			return false;

		Usage.Type = MapType;
		Usage.SubTypes.Add(KeyType);
		Usage.SubTypes.Add(ValueType);
		return true;
	});

	auto MapIteratorType = MakeShared<FAngelscriptMapIteratorType>();
	FAngelscriptType::Register(MapIteratorType);

	FBindFlags ItFlags;
	ItFlags.bTemplate = true;
	ItFlags.TemplateType = "<K,V>";

	auto TMapIterator_ = FAngelscriptBinds::ValueClass<FMapIterator>("TMapIterator<class K, class V>", ItFlags);
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

	auto TMapConstIterator_ = FAngelscriptBinds::ValueClass<FMapIterator>("TMapConstIterator<class K, class V>", ItFlags);
	TMapConstIterator_.Constructor("void f(const TMapConstIterator<K,V>& Other)", FUNC_TRIVIAL(FMapIterator::CopyConstruct));

	auto MapConstIteratorType = MakeShared<FAngelscriptMapConstIteratorType>();
	FAngelscriptType::Register(MapConstIteratorType);

#if AS_ITERATOR_DEBUGGING
	TMapConstIterator_.Destructor("void f()", &FMapIterator::Destruct);
#endif

	TMapConstIterator_.Method("TMapConstIterator<K,V>& opAssign(const TMapConstIterator<K,V>& Other)", METHOD_TRIVIAL(FMapIterator, Assignment));
	TMapConstIterator_.Property("bool CanProceed", &FMapIterator::bCanProceed);
	TMapConstIterator_.Method("const K& GetKey() const", METHOD(FMapIterator, GetKey));
	TMapConstIterator_.Method("const V& GetValue() const", METHOD(FMapIterator, GetValue));
	TMapConstIterator_.Method("TMapConstIterator<K,V>& Proceed()", METHOD(FMapIterator, Proceed));

	TMap_.Method("int opForBegin()", [](FScriptMap& Map, asCObjectType* Meta) -> int32
	{
		return FMapOperations::GetMapOperations(Meta)->FindNextIndex(Map, -1);
	});
	FAngelscriptBinds::PreviousBindPassScriptObjectTypeAsFirstParam();
	TMap_.Method("int opForBegin() const", [](FScriptMap& Map, asCObjectType* Meta) -> int32
	{
		return FMapOperations::GetMapOperations(Meta)->FindNextIndex(Map, -1);
	});
	FAngelscriptBinds::PreviousBindPassScriptObjectTypeAsFirstParam();

	TMap_.Method("bool opForEnd(const int Iterator) const", [](FScriptMap&, int32 Iterator) -> bool { return Iterator == -1; });

	TMap_.Method("void opForNext(int&inout Iterator)", [](FScriptMap& Map, asCObjectType* Meta, int32& Iterator)
	{
		if (Iterator == -1)
			return;
		Iterator = FMapOperations::GetMapOperations(Meta)->FindNextIndex(Map, Iterator);
	});
	FAngelscriptBinds::PreviousBindPassScriptObjectTypeAsFirstParam();
	TMap_.Method("void opForNext(int&inout Iterator) const", [](FScriptMap& Map, asCObjectType* Meta, int32& Iterator)
	{
		if (Iterator == -1)
			return;
		Iterator = FMapOperations::GetMapOperations(Meta)->FindNextIndex(Map, Iterator);
	});
	FAngelscriptBinds::PreviousBindPassScriptObjectTypeAsFirstParam();

	TMap_.Method("V& opForValue(const int Iterator)", [](FScriptMap& Map, asCObjectType* Meta, int32 Iterator) -> void*
	{
		auto* Ops = FMapOperations::GetMapOperations(Meta);
		if (!Map.IsValidIndex(Iterator))
		{
			FAngelscriptEngine::Throw("Iterator out of bounds.");
			return nullptr;
		}
		return Ops->GetValue(Map, Iterator);
	});
	FAngelscriptBinds::PreviousBindPassScriptObjectTypeAsFirstParam();
	TMap_.Method("const V& opForValue(const int Iterator) const", [](FScriptMap& Map, asCObjectType* Meta, int32 Iterator) -> void*
	{
		auto* Ops = FMapOperations::GetMapOperations(Meta);
		if (!Map.IsValidIndex(Iterator))
		{
			FAngelscriptEngine::Throw("Iterator out of bounds.");
			return nullptr;
		}
		return Ops->GetValue(Map, Iterator);
	});
	FAngelscriptBinds::PreviousBindPassScriptObjectTypeAsFirstParam();

	TMap_.Method("const K& opForKey(const int Iterator) const", [](FScriptMap& Map, asCObjectType* Meta, int32 Iterator) -> void*
	{
		auto* Ops = FMapOperations::GetMapOperations(Meta);
		if (!Map.IsValidIndex(Iterator))
		{
			FAngelscriptEngine::Throw("Iterator out of bounds.");
			return nullptr;
		}
		return Ops->GetKey(Map, Iterator);
	});
	FAngelscriptBinds::PreviousBindPassScriptObjectTypeAsFirstParam();

	TMap_.Method("TMapIterator<K,V> Iterator()", FUNC_TRIVIAL(FMapIterator::Create));
	FAngelscriptBinds::PreviousBindPassScriptObjectTypeAsFirstParam();
	SCRIPT_NATIVE_TARRAY_ITERATOR_CREATE(TMap_);

	TMap_.Method("TMapConstIterator<K,V> Iterator() const", FUNC_TRIVIAL(FMapIterator::Create));
	FAngelscriptBinds::PreviousBindPassScriptObjectTypeAsFirstParam();
	SCRIPT_NATIVE_TARRAY_ITERATOR_CREATE(TMap_);
});

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
				const FString HashDecl = TEXT("uint32 Hash() const");
				Ops->HashFunction = SubType->GetMethodByDecl(TCHAR_TO_ANSI(*HashDecl));
				bCanHash = Ops->HashFunction != nullptr;
			}
		}
	}
	
	Ops->bValid = KeyType.CanConstruct() && KeyType.CanDestruct() && KeyType.CanCopy() && KeyType.CanCompare() && bCanHash
		&& ValueType.CanConstruct() && ValueType.CanDestruct() && ValueType.CanCopy();

	TemplateType->SetUserData(Ops);

	if (!Ops->bValid && ErrorMessage != nullptr)
	{
		if (bCanHash)
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
