#include "Binds/Bind_TSet.h"

#include "AngelscriptEngine.h"
#include "ClassGenerator/ASClass.h"
#include "UObject/UnrealType.h"
#include "UObject/GarbageCollection.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_context.h"
#include "source/as_scriptengine.h"
#include "source/as_scriptfunction.h"
#include "EndAngelscriptHeaders.h"

FString FAngelscriptSetType::GetAngelscriptTypeName() const
{
	return TEXT("TSet");
}

bool FAngelscriptSetType::CanQueryPropertyType() const
{
	return false;
}

bool FAngelscriptSetType::CanBeTemplateSubType() const
{
	return false;
}

bool FAngelscriptSetType::HasReferences(const FAngelscriptTypeUsage& Usage) const
{
	if (Usage.SubTypes.Num() != 1)
		return false;
	return Usage.SubTypes[0].HasReferences();
}

void FAngelscriptSetType::EmitReferenceInfo(const FAngelscriptTypeUsage& Usage, FGCReferenceParams& Params) const
{
	check(HasReferences(Usage));

	auto SetLayout = FScriptSet::GetScriptLayout(Usage.SubTypes[0].GetValueSize(), Usage.SubTypes[0].GetValueAlignment());

	UE::GC::FSchemaBuilder InnerSchema(SetLayout.Size);
	{
		FGCReferenceParams InnerParams = Params;
		InnerParams.Schema = &InnerSchema;
		InnerParams.AtOffset = 0;
		Usage.SubTypes[0].EmitReferenceInfo(InnerParams);
	}

	Params.Schema->Add(UE::GC::DeclareMember(Params.Names.Top(), Params.AtOffset, UE::GC::EMemberType::StructSet, InnerSchema.Build()));
}

bool FAngelscriptSetType::CanCreateProperty(const FAngelscriptTypeUsage& Usage) const
{
	if (Usage.SubTypes.Num() != 1)
		return false;
	return Usage.SubTypes[0].CanCreateProperty() && Usage.SubTypes[0].CanHashValue();
}

FProperty* FAngelscriptSetType::CreateProperty(const FAngelscriptTypeUsage& Usage, const FPropertyParams& Params) const
{
	auto* SetProp = new FSetProperty(Params.Outer, Params.PropertyName);

	FPropertyParams InnerParams = Params;
	InnerParams.Outer = SetProp;
	InnerParams.PropertyName = *(Params.PropertyName.ToString() + TEXT("_Element"));

	SetProp->SetLayout = FScriptSet::GetScriptLayout(Usage.SubTypes[0].GetValueSize(), Usage.SubTypes[0].GetValueAlignment());
	SetProp->ElementProp = Usage.SubTypes[0].CreateProperty(InnerParams);

	return SetProp;
}

bool FAngelscriptSetType::MatchesProperty(const FAngelscriptTypeUsage& Usage, const FProperty* Property, EPropertyMatchType MatchType) const
{
	if (Usage.SubTypes.Num() != 1)
		return false;

	const FSetProperty* SetProp = CastField<FSetProperty>(Property);
	if (SetProp == nullptr)
		return false;

	return Usage.SubTypes[0].MatchesProperty(SetProp->ElementProp, FAngelscriptType::EPropertyMatchType::InContainer);
}

bool FAngelscriptSetType::CanCopy(const FAngelscriptTypeUsage& Usage) const
{
	return Usage.SubTypes.Num() == 1 && Usage.SubTypes[0].CanCopy()
		&& Usage.SubTypes[0].CanConstruct() && Usage.SubTypes[0].CanDestruct();
}

bool FAngelscriptSetType::NeedCopy(const FAngelscriptTypeUsage& Usage) const
{
	return true;
}

void FAngelscriptSetType::CopyValue(const FAngelscriptTypeUsage& Usage, void* SourcePtr, void* DestinationPtr) const
{
	FScriptSet& Source = *(FScriptSet*)SourcePtr;
	FScriptSet& Destination = *(FScriptSet*)DestinationPtr;

	FSetOperations Ops(Usage.SubTypes[0]);
	Ops.Empty(Destination, Source.Num());
	for (int32 i = 0, Num = Source.GetMaxIndex(); i < Num; ++i)
	{
		if (Source.IsValidIndex(i))
			Ops.Add(Destination, Ops.GetElement(Source, i));
	}
}

bool FAngelscriptSetType::CanConstruct(const FAngelscriptTypeUsage& Usage) const
{
	return Usage.SubTypes.Num() == 1 && Usage.SubTypes[0].CanConstruct();
}

bool FAngelscriptSetType::NeedConstruct(const FAngelscriptTypeUsage& Usage) const
{
	return true;
}

void FAngelscriptSetType::ConstructValue(const FAngelscriptTypeUsage& Usage, void* DestinationPtr) const
{
	new(DestinationPtr) FScriptSet();
}

bool FAngelscriptSetType::CanDestruct(const FAngelscriptTypeUsage& Usage) const
{
	return Usage.SubTypes.Num() == 1 && Usage.SubTypes[0].CanDestruct();
}

bool FAngelscriptSetType::NeedDestruct(const FAngelscriptTypeUsage& Usage) const
{
	return true;
}

void FAngelscriptSetType::DestructValue(const FAngelscriptTypeUsage& Usage, void* DestinationPtr) const
{
	FScriptSet& Destination = *(FScriptSet*)DestinationPtr;

	FSetOperations Ops(Usage.SubTypes[0]);
	Ops.Empty(Destination, 0);

	Destination.~FScriptSet();
}

int32 FAngelscriptSetType::GetValueSize(const FAngelscriptTypeUsage& Usage) const
{
	return sizeof(FScriptSet);
}

int32 FAngelscriptSetType::GetValueAlignment(const FAngelscriptTypeUsage& Usage) const
{
	return alignof(FScriptSet);
}

bool FAngelscriptSetType::CanBeArgument(const FAngelscriptTypeUsage& Usage) const
{
	return true;
}

void FAngelscriptSetType::SetArgument(const FAngelscriptTypeUsage& Usage, int32 ArgumentIndex, class asIScriptContext* Context, struct FFrame& Stack, const FArgData& Data) const
{
	FScriptSet* Arg = (FScriptSet*)Data.StackPtr;
	new(Arg) FScriptSet();

	if (Usage.bIsReference)
	{
		FScriptSet& Ref = Stack.StepCompiledInRef<FSetProperty,FScriptSet>(Arg);
		Context->SetArgAddress(ArgumentIndex, &Ref);
	}
	else
	{
		Stack.StepCompiledIn<FSetProperty>(Arg);
		Context->SetArgObject(ArgumentIndex, Arg);
	}
}

bool FAngelscriptSetType::CanBeReturned(const FAngelscriptTypeUsage& Usage) const
{
	return true;
}

void FAngelscriptSetType::GetReturnValue(const FAngelscriptTypeUsage& Usage, class asIScriptContext* Context, void* Destination) const
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

bool FAngelscriptSetType::GetDebuggerValue(const FAngelscriptTypeUsage& Usage, void* Address, struct FDebuggerValue& Value) const
{
	if (Usage.SubTypes.Num() != 1)
		return false;

	const FAngelscriptTypeUsage& SubType = Usage.SubTypes[0];
	FScriptSet& Set = Usage.ResolvePrimitive<FScriptSet>(Address);

	Value.Usage = Usage;
	Value.Address = Address;
	Value.bHasMembers = true;
	Value.Type = Usage.GetAngelscriptDeclaration();

	int32 Num = Set.Num();
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

bool FAngelscriptSetType::GetDebuggerScope(const FAngelscriptTypeUsage& Usage, void* Address, struct FDebuggerScope& Scope) const
{
	if (Usage.SubTypes.Num() != 1)
		return false;

	const FAngelscriptTypeUsage& SubType = Usage.SubTypes[0];
	FScriptSet& Set = Usage.ResolvePrimitive<FScriptSet>(Address);

	FSetOperations Ops(SubType);
	for (int32 i = 0, Num = Set.GetMaxIndex(); i < Num; ++i)
	{
		if (!Set.IsValidIndex(i))
			continue;

		void* ElemPtr = Ops.GetElement(Set, i);

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
		NumValue.Value = LexToString(Set.Num());
		Scope.Values.Add(MoveTemp(NumValue));
	}

	return true;
}

bool FAngelscriptSetType::GetDebuggerMember(const FAngelscriptTypeUsage& Usage, void* Address, const FString& Member, struct FDebuggerValue& Value) const
{
	if (Usage.SubTypes.Num() != 1)
		return false;

	const FAngelscriptTypeUsage& SubType = Usage.SubTypes[0];
	FScriptSet& Set = Usage.ResolvePrimitive<FScriptSet>(Address);

	if (Member.StartsWith(TEXT("[")) && Member.EndsWith(TEXT("]")))
	{
		int32 Index = -1;
		LexFromString(Index, *Member.Mid(1, Member.Len() - 2));

		if (!Set.IsValidIndex(Index))
			return false;

		FSetOperations Ops(SubType);
		void* ElemPtr = Ops.GetElement(Set, Index);

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
		Value.Value = LexToString(Set.Num());
		return true;
	}

	return true;
}

bool FAngelscriptSetType::GetCppForm(const FAngelscriptTypeUsage& Usage, FCppForm& OutCppForm) const
{
	if (Usage.SubTypes.Num() != 1)
		return false;

	FCppForm CppInner;
	if (Usage.SubTypes[0].GetCppForm(CppInner))
	{
		if (CppInner.CppType.Len() != 0 && !CppInner.bDisallowNativeNest)
		{
			OutCppForm.CppType = FString::Printf(TEXT("TSet<%s>"), *CppInner.CppType);
			OutCppForm.CppHeader = CppInner.CppHeader;
		}

		if (CppInner.CppGenericType.Len() != 0)
		{
			OutCppForm.CppGenericType = FString::Printf(TEXT("TSet<%s>"), *CppInner.CppGenericType);
		}
	}

	OutCppForm.TemplateObjectForm = TEXT("FScriptSet");
	return true;
}

bool FAngelscriptSetType::CanCompare(const FAngelscriptTypeUsage& Usage) const
{
	return Usage.SubTypes.Num() == 1 && Usage.SubTypes[0].CanCompare();
}

bool FAngelscriptSetType::IsValueEqual(const FAngelscriptTypeUsage& Usage, void* SourcePtr, void* DestinationPtr) const
{
	const FAngelscriptTypeUsage& SubType = Usage.SubTypes[0];
	FScriptSet& SourceSet = *(FScriptSet*)SourcePtr;
	FScriptSet& DestSet = *(FScriptSet*)DestinationPtr;

	check(SubType.CanCompare());

	FSetOperations Ops(SubType);
	return Ops.IsPermutation(SourceSet, DestSet);
}

FString FAngelscriptSetIteratorType::GetAngelscriptTypeName() const
{
	return TEXT("TSetIterator");
}

bool FAngelscriptSetIteratorType::CanQueryPropertyType() const
{
	return false;
}

bool FAngelscriptSetIteratorType::GetCppForm(const FAngelscriptTypeUsage& Usage, FCppForm& OutCppForm) const
{
	OutCppForm.TemplateObjectForm = TEXT("FSetIterator");
	return true;
}

FString FAngelscriptSetConstIteratorType::GetAngelscriptTypeName() const
{
	return TEXT("TSetConstIterator");
}

bool FAngelscriptSetConstIteratorType::CanQueryPropertyType() const
{
	return false;
}

bool FAngelscriptSetConstIteratorType::GetCppForm(const FAngelscriptTypeUsage& Usage, FCppForm& OutCppForm) const
{
	OutCppForm.TemplateObjectForm = TEXT("FSetIterator");
	return true;
}
