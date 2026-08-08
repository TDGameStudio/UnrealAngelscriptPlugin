#include "Binds/Bind_TArray.h"

#include "AngelscriptEngine.h"
#include "Helper_Reification.h"

#include "ClassGenerator/ASClass.h"

#include "UObject/UnrealType.h"
#include "UObject/GarbageCollection.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_context.h"
#include "source/as_scriptengine.h"
#include "source/as_scriptfunction.h"
#include "EndAngelscriptHeaders.h"

FString FAngelscriptArrayType::GetAngelscriptTypeName() const
{
	return TEXT("TArray");
}

bool FAngelscriptArrayType::CanQueryPropertyType() const { return false; }

bool FAngelscriptArrayType::HasReferences(const FAngelscriptTypeUsage& Usage) const
{
	return Usage.SubTypes.Num() == 1 && Usage.SubTypes[0].HasReferences();
}

void FAngelscriptArrayType::EmitReferenceInfo(const FAngelscriptTypeUsage& Usage, FGCReferenceParams& Params) const
{
	check(HasReferences(Usage));

	int32 ElementSize = Usage.SubTypes[0].Type->GetValueSize(Usage.SubTypes[0]);
	UE::GC::FSchemaBuilder InnerSchema(ElementSize);

	if (Usage.SubTypes[0].Type->IsObjectPointer())
	{
		Params.Schema->Add(UE::GC::DeclareMember(Params.Names.Top(), Params.AtOffset, UE::GC::EMemberType::ReferenceArray, InnerSchema.Build()));
	}
	else
	{
		FGCReferenceParams InnerParams = Params;
		InnerParams.Schema = &InnerSchema;
		InnerParams.AtOffset = 0;
		Usage.SubTypes[0].EmitReferenceInfo(InnerParams);

		Params.Schema->Add(UE::GC::DeclareMember(Params.Names.Top(), Params.AtOffset, UE::GC::EMemberType::StructArray, InnerSchema.Build()));
	}
}

bool FAngelscriptArrayType::CanCreateProperty(const FAngelscriptTypeUsage& Usage) const
{
	if (Usage.SubTypes.Num() != 1)
		return false;
	return Usage.SubTypes[0].CanCreateProperty();
}

FProperty* FAngelscriptArrayType::CreateProperty(const FAngelscriptTypeUsage& Usage, const FPropertyParams& Params) const
{
	auto* ArrayProp = new FArrayProperty(Params.Outer, Params.PropertyName);

	FPropertyParams InnerParams = Params;
	InnerParams.Outer = ArrayProp;
	InnerParams.PropertyName = *(Params.PropertyName.ToString() + TEXT("_Inner"));
	ArrayProp->Inner = Usage.SubTypes[0].CreateProperty(InnerParams);

	return ArrayProp;
}

bool FAngelscriptArrayType::MatchesProperty(const FAngelscriptTypeUsage& Usage, const FProperty* Property, EPropertyMatchType MatchType) const
{
	if (Usage.SubTypes.Num() != 1)
		return false;

	const FArrayProperty* ArrayProp = CastField<FArrayProperty>(Property);
	if (ArrayProp == nullptr)
		return false;

	return Usage.SubTypes[0].MatchesProperty(ArrayProp->Inner, FAngelscriptType::EPropertyMatchType::InContainer);
}

bool FAngelscriptArrayType::CanCopy(const FAngelscriptTypeUsage& Usage) const
{
	return Usage.SubTypes.Num() == 1 && Usage.SubTypes[0].CanCopy()
		&& Usage.SubTypes[0].CanConstruct() && Usage.SubTypes[0].CanDestruct();
}
bool FAngelscriptArrayType::NeedCopy(const FAngelscriptTypeUsage& Usage) const  { return true; }
void FAngelscriptArrayType::CopyValue(const FAngelscriptTypeUsage& Usage, void* SourcePtr, void* DestinationPtr) const
{
	const FAngelscriptTypeUsage& SubType = Usage.SubTypes[0];
	FScriptArray& SourceArray = *(FScriptArray*)SourcePtr;
	FScriptArray& DestinationArray = *(FScriptArray*)DestinationPtr;
	int32 ElementSize = Usage.SubTypes[0].GetValueSize();
	int32 ElementAlignment = Usage.SubTypes[0].GetValueAlignment();

	int32 SourceNum = SourceArray.Num();
	int32 DestNum = DestinationArray.Num();

	if (!SubType.NeedCopy())
	{
		// Totally POD-typed, so just do a direct copy instead of shenanigans
		if (SourceNum > DestNum)
			DestinationArray.Add(SourceNum - DestNum, ElementSize, ElementAlignment);
		else if(DestNum > SourceNum)
			DestinationArray.Remove(SourceNum, DestNum - SourceNum, ElementSize, ElementAlignment);
		FMemory::Memcpy(DestinationArray.GetData(), SourceArray.GetData(), SourceNum * ElementSize);
		return;
	}

	if (SourceNum > DestNum)
	{
		DestinationArray.Add(SourceNum - DestNum, ElementSize, ElementAlignment);

		if (SubType.NeedConstruct())
		{
			for (int32 i = DestNum; i < SourceNum; ++i)
				SubType.ConstructValue((void*)((SIZE_T)DestinationArray.GetData() + (i * ElementSize)));
		}
	}
	else if (DestNum > SourceNum)
	{
		if (SubType.NeedDestruct())
		{
			for (int32 i = SourceNum; i < DestNum; ++i)
				SubType.DestructValue((void*)((SIZE_T)DestinationArray.GetData() + (i * ElementSize)));
		}

		DestinationArray.Remove(SourceNum, DestNum - SourceNum, ElementSize, ElementAlignment);
	}

	for (int32 i = 0; i < SourceNum; ++i)
	{
		SubType.CopyValue(
			(void*)((SIZE_T)SourceArray.GetData() + (i * ElementSize)),
			(void*)((SIZE_T)DestinationArray.GetData() + (i * ElementSize)));
	}
}

bool FAngelscriptArrayType::CanConstruct(const FAngelscriptTypeUsage& Usage) const
{
	return Usage.SubTypes.Num() == 1;
}
bool FAngelscriptArrayType::NeedConstruct(const FAngelscriptTypeUsage& Usage) const  { return true; }
void FAngelscriptArrayType::ConstructValue(const FAngelscriptTypeUsage& Usage, void* DestinationPtr) const
{
	new(DestinationPtr) FScriptArray();
}

bool FAngelscriptArrayType::CanDestruct(const FAngelscriptTypeUsage& Usage) const
{
	return Usage.SubTypes.Num() == 1 && Usage.SubTypes[0].CanDestruct();
}
bool FAngelscriptArrayType::NeedDestruct(const FAngelscriptTypeUsage& Usage) const  { return true; }
void FAngelscriptArrayType::DestructValue(const FAngelscriptTypeUsage& Usage, void* DestinationPtr) const
{
	const FAngelscriptTypeUsage& SubType = Usage.SubTypes[0];
	FScriptArray& Array = *(FScriptArray*)DestinationPtr;

	int32 ElementSize = SubType.GetValueSize();
	int32 SourceNum = Array.Num();

	if (SubType.NeedDestruct())
	{
		for (int32 i = 0; i < SourceNum; ++i)
			SubType.DestructValue((void*)((SIZE_T)Array.GetData() + (i * ElementSize)));
	}


	Array.~FScriptArray();
}

int32 FAngelscriptArrayType::GetValueSize(const FAngelscriptTypeUsage& Usage) const
{
	return sizeof(FScriptArray);
}

int32 FAngelscriptArrayType::GetValueAlignment(const FAngelscriptTypeUsage& Usage) const
{
	return alignof(FScriptArray);
}

bool FAngelscriptArrayType::CanBeArgument(const FAngelscriptTypeUsage& Usage) const  { return true; }
void FAngelscriptArrayType::SetArgument(const FAngelscriptTypeUsage& Usage, int32 ArgumentIndex, class asIScriptContext* Context, struct FFrame& Stack, const FArgData& Data) const
{
	FScriptArray* Arg = (FScriptArray*)Data.StackPtr;
	new(Arg) FScriptArray();

	if (Usage.bIsReference)
	{
		FScriptArray& Ref = Stack.StepCompiledInRef<FArrayProperty,FScriptArray>(Arg);
		Context->SetArgAddress(ArgumentIndex, &Ref);
	}
	else
	{
		Stack.StepCompiledIn<FArrayProperty>(Arg);
		Context->SetArgObject(ArgumentIndex, Arg);
	}
}

bool FAngelscriptArrayType::CanBeReturned(const FAngelscriptTypeUsage& Usage) const
{
	return true;
}

void FAngelscriptArrayType::GetReturnValue(const FAngelscriptTypeUsage& Usage, class asIScriptContext* Context, void* Destination) const
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

bool FAngelscriptArrayType::CanCompare(const FAngelscriptTypeUsage& Usage) const
{
	return Usage.SubTypes.Num() == 1 && Usage.SubTypes[0].CanCompare();
}

bool FAngelscriptArrayType::IsValueEqual(const FAngelscriptTypeUsage& Usage, void* SourcePtr, void* DestinationPtr) const
{
	const FAngelscriptTypeUsage& SubType = Usage.SubTypes[0];
	FScriptArray& SourceArray = *(FScriptArray*)SourcePtr;
	FScriptArray& DestArray = *(FScriptArray*)DestinationPtr;

	check(SubType.CanCompare());

	int32 ElementSize = SubType.GetValueSize();
	int32 SourceNum = SourceArray.Num();
	int32 DestNum = DestArray.Num();

	if (SourceNum != DestNum)
		return false;

	for (int32 i = 0; i < SourceNum; ++i)
	{
		void* SourceValue = (void*)((SIZE_T)SourceArray.GetData() + (i * ElementSize));
		void* DestValue = (void*)((SIZE_T)DestArray.GetData() + (i * ElementSize));

		if (!SubType.IsValueEqual(SourceValue, DestValue))
			return false;
	}

	return true;
}

template<typename T>
struct TNativeDebugArray : FASDebugValue
{
	TArray<T>* Value;

	TNativeDebugArray(SIZE_T Offset)
		: Value((TArray<T>*)(void*)Offset)
	{
	}

	void Instantiate(void* ForObject) override
	{
		Value = (TArray<T>*)((SIZE_T)Value + (SIZE_T)ForObject);
	}
};

struct FGenericDebugArray : FASDebugValue
{
	FScriptArray* Value;
	int32 ElementSize;

	FGenericDebugArray(SIZE_T Offset, int32 InElementSize)
		: Value((FScriptArray*)(void*)Offset)
		, ElementSize(InElementSize)
	{
	}

	void Instantiate(void* ForObject) override
	{
		Value = (FScriptArray*)((SIZE_T)Value + (SIZE_T)ForObject);
	}
};

template<typename T>
struct TNativeDebugArrayPtr : FASDebugValue
{
	TArray<T>** Value;

	TNativeDebugArrayPtr(SIZE_T Offset)
		: Value((TArray<T>**)(void*)Offset)
	{
	}

	void Instantiate(void* ForObject) override
	{
		Value = (TArray<T>**)((SIZE_T)Value + (SIZE_T)ForObject);
	}
};

struct FGenericDebugArrayPtr : FASDebugValue
{
	FScriptArray** Value;
	int32 ElementSize;

	FGenericDebugArrayPtr(SIZE_T Offset, int32 InElementSize)
		: Value((FScriptArray**)(void*)Offset)
		, ElementSize(InElementSize)
	{
	}

	void Instantiate(void* ForObject) override
	{
		Value = (FScriptArray**)((SIZE_T)Value + (SIZE_T)ForObject);
	}
};

FASDebugValue* FAngelscriptArrayType::CreateDebugValue(const FAngelscriptTypeUsage& Usage, FDebugValuePrototype& Values, int32 Offset) const
{
	if (Usage.SubTypes.Num() != 1)
		return nullptr;
	if (Usage.bIsReference)
	{
		return ReifyDebugValueTemplate<TNativeDebugArrayPtr, FGenericDebugArrayPtr>(
			Usage.SubTypes[0].GetReifyType(), Values, Offset,
			Usage.SubTypes[0].GetValueSize());
	}
	else
	{
		return ReifyDebugValueTemplate<TNativeDebugArray, FGenericDebugArray>(
			Usage.SubTypes[0].GetReifyType(), Values, Offset,
			Usage.SubTypes[0].GetValueSize());
	}
}

bool FAngelscriptArrayType::GetDebuggerValue(const FAngelscriptTypeUsage& Usage, void* Address, struct FDebuggerValue& Value) const
{
	if (Usage.SubTypes.Num() != 1)
		return false;

	const FAngelscriptTypeUsage& SubType = Usage.SubTypes[0];
	FScriptArray& Array = Usage.ResolvePrimitive<FScriptArray>(Address);

	Value.Usage = Usage;
	Value.Address = Address;
	Value.bHasMembers = true;
	Value.Type = Usage.GetAngelscriptDeclaration();

	int32 Num = Array.Num();
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

bool FAngelscriptArrayType::GetDebuggerScope(const FAngelscriptTypeUsage& Usage, void* Address, struct FDebuggerScope& Scope) const
{
	if (Usage.SubTypes.Num() != 1)
		return false;

	const FAngelscriptTypeUsage& SubType = Usage.SubTypes[0];
	FScriptArray& Array = Usage.ResolvePrimitive<FScriptArray>(Address);

	int32 Num = Array.Num();
	int32 ElementSize = SubType.GetValueSize();

	for (int32 i = 0; i < Num; ++i)
	{
		void* ElemPtr = (void*)((SIZE_T)Array.GetData() + (i * ElementSize));

		FDebuggerValue ElemValue;
		if (SubType.GetDebuggerValue(ElemPtr, ElemValue))
		{
			ElemValue.Name = FString::Printf(TEXT("[%d]"), i);
			Scope.Values.Add(MoveTemp(ElemValue));
		}
	}

	{
		FDebuggerValue NumValue;
		NumValue.Name = TEXT("Num");
		NumValue.Type = TEXT("int");
		NumValue.Value = LexToString(Array.Num());
		Scope.Values.Add(MoveTemp(NumValue));
	}

	return true;
}

bool FAngelscriptArrayType::GetDebuggerMember(const FAngelscriptTypeUsage& Usage, void* Address, const FString& Member, struct FDebuggerValue& Value) const
{
	if (Usage.SubTypes.Num() != 1)
		return false;

	const FAngelscriptTypeUsage& SubType = Usage.SubTypes[0];
	FScriptArray& Array = Usage.ResolvePrimitive<FScriptArray>(Address);

	if (Member.StartsWith(TEXT("[")) && Member.EndsWith(TEXT("]")))
	{
		FString Number = Member.Mid(1, Member.Len() - 2);
		if (!Number.IsNumeric())
			return false;

		int32 Index = -1;
		LexFromString(Index, *Number);

		if (!Array.IsValidIndex(Index))
			return false;

		int32 ElementSize = SubType.GetValueSize();
		void* ElemPtr = (void*)((SIZE_T)Array.GetData() + (Index * ElementSize));

		if (SubType.GetDebuggerValue(ElemPtr, Value))
		{
			Value.Name = FString::Printf(TEXT("[%d]"), Index);
			return true;
		}

		return false;
	}
	else if (Member == TEXT("Num"))
	{
		Value.Name = TEXT("Num");
		Value.Type = TEXT("int");
		Value.Value = LexToString(Array.Num());
		return true;
	}

	return true;
}

bool FAngelscriptArrayType::GetCppForm(const FAngelscriptTypeUsage& Usage, FCppForm& OutCppForm) const
{
	if (Usage.SubTypes.Num() != 1)
		return false;

	FCppForm CppInner;
	if (Usage.SubTypes[0].GetCppForm(CppInner))
	{
		if (CppInner.CppType.Len() != 0 && !CppInner.bDisallowNativeNest)
		{
			OutCppForm.CppType = FString::Printf(TEXT("TArray<%s>"), *CppInner.CppType);
			OutCppForm.CppHeader = CppInner.CppHeader;
		}

		if (CppInner.CppGenericType.Len() != 0)
		{
			OutCppForm.CppGenericType = FString::Printf(TEXT("TArray<%s>"), *CppInner.CppGenericType);
		}
	}

	OutCppForm.TemplateObjectForm = TEXT("FScriptArray");
	return true;
}

FString FAngelscriptArrayIteratorType::GetAngelscriptTypeName() const
{
	return TEXT("TArrayIterator");
}

bool FAngelscriptArrayIteratorType::CanQueryPropertyType() const
{
	return false;
}

bool FAngelscriptArrayIteratorType::GetCppForm(const FAngelscriptTypeUsage& Usage, FCppForm& OutCppForm) const
{
	OutCppForm.TemplateObjectForm = TEXT("FArrayIterator");
	return true;
}

FString FAngelscriptArrayConstIteratorType::GetAngelscriptTypeName() const
{
	return TEXT("TArrayConstIterator");
}

bool FAngelscriptArrayConstIteratorType::CanQueryPropertyType() const
{
	return false;
}

bool FAngelscriptArrayConstIteratorType::GetCppForm(const FAngelscriptTypeUsage& Usage, FCppForm& OutCppForm) const
{
	OutCppForm.TemplateObjectForm = TEXT("FArrayIterator");
	return true;
}
