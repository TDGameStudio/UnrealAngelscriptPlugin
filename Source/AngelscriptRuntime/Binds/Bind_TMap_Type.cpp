#include "Binds/Bind_TMap.h"

#include "AngelscriptEngine.h"
#include "ClassGenerator/ASClass.h"
#include "UObject/UnrealType.h"
#include "UObject/GarbageCollection.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_context.h"
#include "source/as_scriptengine.h"
#include "source/as_scriptfunction.h"
#include "EndAngelscriptHeaders.h"

FString FAngelscriptMapType::GetAngelscriptTypeName() const
{
	return TEXT("TMap");
}

bool FAngelscriptMapType::CanQueryPropertyType() const
{
	return false;
}

bool FAngelscriptMapType::CanBeTemplateSubType() const
{
	return false;
}

bool FAngelscriptMapType::HasReferences(const FAngelscriptTypeUsage& Usage) const
{
	if (Usage.SubTypes.Num() != 2)
		return false;
	return Usage.SubTypes[0].HasReferences() || Usage.SubTypes[1].HasReferences();
}

void FAngelscriptMapType::EmitReferenceInfo(const FAngelscriptTypeUsage& Usage, FGCReferenceParams& Params) const
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

bool FAngelscriptMapType::CanCreateProperty(const FAngelscriptTypeUsage& Usage) const
{
	if (Usage.SubTypes.Num() != 2)
		return false;
	return Usage.SubTypes[0].CanCreateProperty() && Usage.SubTypes[0].CanHashValue()
		&& Usage.SubTypes[1].CanCreateProperty();
}

FProperty* FAngelscriptMapType::CreateProperty(const FAngelscriptTypeUsage& Usage, const FPropertyParams& Params) const
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

bool FAngelscriptMapType::MatchesProperty(const FAngelscriptTypeUsage& Usage, const FProperty* Property, EPropertyMatchType MatchType) const
{
	if (Usage.SubTypes.Num() != 2)
		return false;

	const FMapProperty* MapProp = CastField<FMapProperty>(Property);
	if (MapProp == nullptr)
		return false;

	return Usage.SubTypes[0].MatchesProperty(MapProp->GetKeyProperty(), FAngelscriptType::EPropertyMatchType::InContainer)
	&& Usage.SubTypes[1].MatchesProperty(MapProp->GetValueProperty(), FAngelscriptType::EPropertyMatchType::InContainer);
}

bool FAngelscriptMapType::CanCopy(const FAngelscriptTypeUsage& Usage) const
{
	return Usage.SubTypes.Num() == 2
		&& Usage.SubTypes[0].CanCopy()
		&& Usage.SubTypes[0].CanConstruct()
		&& Usage.SubTypes[0].CanDestruct()
		&& Usage.SubTypes[1].CanCopy()
		&& Usage.SubTypes[1].CanConstruct()
		&& Usage.SubTypes[1].CanDestruct();
}

bool FAngelscriptMapType::NeedCopy(const FAngelscriptTypeUsage& Usage) const
{
	return true;
}

void FAngelscriptMapType::CopyValue(const FAngelscriptTypeUsage& Usage, void* SourcePtr, void* DestinationPtr) const
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

bool FAngelscriptMapType::CanConstruct(const FAngelscriptTypeUsage& Usage) const
{
	return Usage.SubTypes.Num() == 2 && Usage.SubTypes[0].CanConstruct() && Usage.SubTypes[1].CanConstruct();
}

bool FAngelscriptMapType::NeedConstruct(const FAngelscriptTypeUsage& Usage) const
{
	return true;
}

void FAngelscriptMapType::ConstructValue(const FAngelscriptTypeUsage& Usage, void* DestinationPtr) const
{
	new(DestinationPtr) FScriptMap();
}

bool FAngelscriptMapType::CanDestruct(const FAngelscriptTypeUsage& Usage) const
{
	return Usage.SubTypes.Num() == 2 && Usage.SubTypes[0].CanDestruct() && Usage.SubTypes[1].CanDestruct();
}

bool FAngelscriptMapType::NeedDestruct(const FAngelscriptTypeUsage& Usage) const
{
	return true;
}

void FAngelscriptMapType::DestructValue(const FAngelscriptTypeUsage& Usage, void* DestinationPtr) const
{
	FScriptMap& Destination = *(FScriptMap*)DestinationPtr;

	FMapOperations Ops(Usage.SubTypes[0], Usage.SubTypes[1]);
	Ops.Empty(Destination, 0);

	Destination.~FScriptMap();
}

int32 FAngelscriptMapType::GetValueSize(const FAngelscriptTypeUsage& Usage) const
{
	return sizeof(FScriptMap);
}

int32 FAngelscriptMapType::GetValueAlignment(const FAngelscriptTypeUsage& Usage) const
{
	return alignof(FScriptMap);
}

bool FAngelscriptMapType::CanBeArgument(const FAngelscriptTypeUsage& Usage) const
{
	return true;
}

void FAngelscriptMapType::SetArgument(const FAngelscriptTypeUsage& Usage, int32 ArgumentIndex, class asIScriptContext* Context, struct FFrame& Stack, const FArgData& Data) const
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

bool FAngelscriptMapType::CanBeReturned(const FAngelscriptTypeUsage& Usage) const
{
	return true;
}

void FAngelscriptMapType::GetReturnValue(const FAngelscriptTypeUsage& Usage, class asIScriptContext* Context, void* Destination) const
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

bool FAngelscriptMapType::GetDebuggerValue(const FAngelscriptTypeUsage& Usage, void* Address, struct FDebuggerValue& Value) const
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

bool FAngelscriptMapType::GetDebuggerScope(const FAngelscriptTypeUsage& Usage, void* Address, struct FDebuggerScope& Scope) const
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

bool FAngelscriptMapType::GetDebuggerMember(const FAngelscriptTypeUsage& Usage, void* Address, const FString& Member, struct FDebuggerValue& Value) const
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

bool FAngelscriptMapType::GetCppForm(const FAngelscriptTypeUsage& Usage, FCppForm& OutCppForm) const
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

bool FAngelscriptMapType::CanCompare(const FAngelscriptTypeUsage& Usage) const
{
	return Usage.SubTypes.Num() == 2 && Usage.SubTypes[0].CanCompare() && Usage.SubTypes[1].CanCompare();
}

bool FAngelscriptMapType::IsValueEqual(const FAngelscriptTypeUsage& Usage, void* SourcePtr, void* DestinationPtr) const
{
	const FAngelscriptTypeUsage& KeyType = Usage.SubTypes[0];
	const FAngelscriptTypeUsage& ValueType = Usage.SubTypes[1];

	FScriptMap& SourceMap = *(FScriptMap*)SourcePtr;
	FScriptMap& DestMap = *(FScriptMap*)DestinationPtr;

	FMapOperations Ops(KeyType, ValueType);
	return Ops.IsPermutation(SourceMap, DestMap);
}

FString FAngelscriptMapIteratorType::GetAngelscriptTypeName() const
{
	return TEXT("TMapIterator");
}

int32 FAngelscriptMapIteratorType::GetValueSize(const FAngelscriptTypeUsage& Usage) const
{
	return sizeof(FMapIterator);
}

int32 FAngelscriptMapIteratorType::GetValueAlignment(const FAngelscriptTypeUsage& Usage) const
{
	return alignof(FMapIterator);
}

bool FAngelscriptMapIteratorType::GetDebuggerValue(const FAngelscriptTypeUsage& Usage, void* Address, struct FDebuggerValue& Value) const
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

bool FAngelscriptMapIteratorType::GetDebuggerScope(const FAngelscriptTypeUsage& Usage, void* Address, struct FDebuggerScope& Scope) const
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

bool FAngelscriptMapIteratorType::GetDebuggerMember(const FAngelscriptTypeUsage& Usage, void* Address, const FString& Member, struct FDebuggerValue& Value) const
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

bool FAngelscriptMapIteratorType::GetCppForm(const FAngelscriptTypeUsage& Usage, FCppForm& OutCppForm) const
{
	OutCppForm.TemplateObjectForm = TEXT("FMapIterator");
	return true;
}

FString FAngelscriptMapConstIteratorType::GetAngelscriptTypeName() const
{
	return TEXT("TMapConstIterator");
}

int32 FAngelscriptMapConstIteratorType::GetValueSize(const FAngelscriptTypeUsage& Usage) const
{
	return sizeof(FMapIterator);
}

int32 FAngelscriptMapConstIteratorType::GetValueAlignment(const FAngelscriptTypeUsage& Usage) const
{
	return alignof(FMapIterator);
}

bool FAngelscriptMapConstIteratorType::GetDebuggerValue(const FAngelscriptTypeUsage& Usage, void* Address, struct FDebuggerValue& Value) const
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

bool FAngelscriptMapConstIteratorType::GetDebuggerScope(const FAngelscriptTypeUsage& Usage, void* Address, struct FDebuggerScope& Scope) const
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

bool FAngelscriptMapConstIteratorType::GetDebuggerMember(const FAngelscriptTypeUsage& Usage, void* Address, const FString& Member, struct FDebuggerValue& Value) const
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

bool FAngelscriptMapConstIteratorType::GetCppForm(const FAngelscriptTypeUsage& Usage, FCppForm& OutCppForm) const
{
	OutCppForm.TemplateObjectForm = TEXT("FMapIterator");
	return true;
}
