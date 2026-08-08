#include "Binds/Bind_TOptional.h"

#include "AngelscriptEngine.h"
#include "ClassGenerator/ASClass.h"
#include "UObject/GarbageCollectionSchema.h"
#include "UObject/PropertyOptional.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_context.h"
#include "source/as_scriptengine.h"
#include "source/as_scriptfunction.h"
#include "EndAngelscriptHeaders.h"

FString FAngelscriptOptionalType::GetAngelscriptTypeName() const
{
	return TEXT("TOptional");
}

bool FAngelscriptOptionalType::CanQueryPropertyType() const
{
	return false;
}

bool FAngelscriptOptionalType::CanBeTemplateSubType() const
{
	return false;
}

bool FAngelscriptOptionalType::RequiresProperty(const FAngelscriptTypeUsage& Usage) const
{
	return false;
}

bool FAngelscriptOptionalType::HasReferences(const FAngelscriptTypeUsage& Usage) const
{
	return Usage.SubTypes.Num() == 1 && Usage.SubTypes[0].HasReferences();
}

void FAngelscriptOptionalType::EmitReferenceInfo(const FAngelscriptTypeUsage& Usage, FGCReferenceParams& Params) const
{
	check(HasReferences(Usage));

	UE::GC::FSchemaBuilder InnerSchema(Usage.SubTypes[0].GetValueSize());
	{
		FGCReferenceParams InnerParams = Params;
		InnerParams.AtOffset = 0;
		InnerParams.Schema = &InnerSchema;
		Usage.SubTypes[0].EmitReferenceInfo(InnerParams);
	}
	Params.Schema->Add(DeclareMember(Params.Names.Top(), Params.AtOffset, UE::GC::EMemberType::Optional, InnerSchema.Build()));
}

bool FAngelscriptOptionalType::CanCopy(const FAngelscriptTypeUsage& Usage) const
{
	return Usage.SubTypes.Num() == 1 && Usage.SubTypes[0].CanCopy()
		&& Usage.SubTypes[0].CanConstruct() && Usage.SubTypes[0].CanDestruct();
}

bool FAngelscriptOptionalType::CanCompare(const FAngelscriptTypeUsage& Usage) const
{
	return Usage.SubTypes.Num() == 1 && Usage.SubTypes[0].CanCompare();
}

bool FAngelscriptOptionalType::IsValueEqual(const FAngelscriptTypeUsage& Usage, void* SourcePtr, void* DestinationPtr) const
{
	const FAngelscriptTypeUsage& SubType = Usage.SubTypes[0];
	check(SubType.CanCompare());

	FOptionalOperations Ops(SubType);
	FAngelscriptOptional& SourceOptional = *static_cast<FAngelscriptOptional*>(SourcePtr);
	FAngelscriptOptional& DestinationOptional = *static_cast<FAngelscriptOptional*>(DestinationPtr);

	const bool bSourceSet = Ops.IsSet(SourceOptional);
	const bool bDestinationSet = Ops.IsSet(DestinationOptional);
	if (bSourceSet != bDestinationSet)
	{
		return false;
	}

	if (!bSourceSet)
	{
		return true;
	}

	return SubType.IsValueEqual(Ops.GetValuePtr(SourceOptional), Ops.GetValuePtr(DestinationOptional));
}

bool FAngelscriptOptionalType::NeedCopy(const FAngelscriptTypeUsage& Usage) const
{
	return true;
}

void FAngelscriptOptionalType::CopyValue(const FAngelscriptTypeUsage& Usage, void* SourcePtr, void* DestinationPtr) const
{
	int32 ElementSize = Usage.SubTypes[0].GetValueSize();
	if (*(bool*)((SIZE_T)SourcePtr + ElementSize))
	{
		if (!*(bool*)((SIZE_T)DestinationPtr + ElementSize))
		{
			Usage.SubTypes[0].ConstructValue(DestinationPtr);
			*(bool*)((SIZE_T)DestinationPtr + ElementSize) = true;
		}

		Usage.SubTypes[0].CopyValue(SourcePtr, DestinationPtr);
	}
	else
	{
		if (*(bool*)((SIZE_T)DestinationPtr + ElementSize))
		{
			Usage.SubTypes[0].DestructValue(DestinationPtr);
			*(bool*)((SIZE_T)DestinationPtr + ElementSize) = false;
		}
	}
}

bool FAngelscriptOptionalType::CanCreateProperty(const FAngelscriptTypeUsage& Usage) const
{
	if (Usage.SubTypes.Num() != 1)
		return false;
	return Usage.SubTypes[0].CanCreateProperty();
}

FProperty* FAngelscriptOptionalType::CreateProperty(const FAngelscriptTypeUsage& Usage, const FPropertyParams& Params) const
{
	auto* OptionalProp = new FOptionalProperty(Params.Outer, Params.PropertyName);

	FPropertyParams InnerParams = Params;
	InnerParams.Outer = OptionalProp;
	InnerParams.PropertyName = *(Params.PropertyName.ToString() + TEXT("_Inner"));

	OptionalProp->SetValueProperty(Usage.SubTypes[0].CreateProperty(InnerParams));

	return OptionalProp;
}

bool FAngelscriptOptionalType::MatchesProperty(const FAngelscriptTypeUsage& Usage, const FProperty* Property, EPropertyMatchType MatchType) const
{
	if (Usage.SubTypes.Num() != 1)
		return false;

	const FOptionalProperty* OptionalProp = CastField<FOptionalProperty>(Property);
	if (OptionalProp == nullptr)
		return false;

	return Usage.SubTypes[0].MatchesProperty(OptionalProp->GetValueProperty(), FAngelscriptType::EPropertyMatchType::InContainer);
}

bool FAngelscriptOptionalType::CanConstruct(const FAngelscriptTypeUsage& Usage) const
{
	return Usage.SubTypes.Num() == 1;
}

bool FAngelscriptOptionalType::NeedConstruct(const FAngelscriptTypeUsage& Usage) const
{
	return true;
}

void FAngelscriptOptionalType::ConstructValue(const FAngelscriptTypeUsage& Usage, void* DestinationPtr) const
{
	int32 ElementSize = Usage.SubTypes[0].GetValueSize();
	*(bool*)((SIZE_T)DestinationPtr + ElementSize) = false;
}

bool FAngelscriptOptionalType::CanDestruct(const FAngelscriptTypeUsage& Usage) const
{
	return Usage.SubTypes.Num() == 1 && Usage.SubTypes[0].CanDestruct();
}

bool FAngelscriptOptionalType::NeedDestruct(const FAngelscriptTypeUsage& Usage) const
{
	return Usage.SubTypes[0].NeedDestruct();
}

void FAngelscriptOptionalType::DestructValue(const FAngelscriptTypeUsage& Usage, void* DestinationPtr) const
{
	int32 ElementSize = Usage.SubTypes[0].GetValueSize();
	if (*(bool*)((SIZE_T)DestinationPtr + ElementSize))
		Usage.SubTypes[0].DestructValue(DestinationPtr);
}

int32 FAngelscriptOptionalType::GetValueSize(const FAngelscriptTypeUsage& Usage) const
{
	return Align(Usage.SubTypes[0].GetValueSize() + 1, Usage.SubTypes[0].GetValueAlignment());
}

int32 FAngelscriptOptionalType::GetValueAlignment(const FAngelscriptTypeUsage& Usage) const
{
	return Usage.SubTypes[0].GetValueAlignment();
}

bool FAngelscriptOptionalType::CanBeArgument(const FAngelscriptTypeUsage& Usage) const
{
	return false;
}

void FAngelscriptOptionalType::SetArgument(const FAngelscriptTypeUsage& Usage, int32 ArgumentIndex, class asIScriptContext* Context, struct FFrame& Stack, const FArgData& Data) const
{
	check(false);
}

bool FAngelscriptOptionalType::CanBeReturned(const FAngelscriptTypeUsage& Usage) const
{
	return true;
}

void FAngelscriptOptionalType::GetReturnValue(const FAngelscriptTypeUsage& Usage, class asIScriptContext* Context, void* Destination) const
{
	if (Usage.bIsReference)
	{
		*static_cast<void**>(Destination) = Context->GetReturnAddress();
	}
	else
	{
		if (void* ReturnedObject = Context->GetReturnObject())
		{
			CopyValue(Usage, ReturnedObject, Destination);
		}
	}
}

bool FAngelscriptOptionalType::GetDebuggerValue(const FAngelscriptTypeUsage& Usage, void* Address, struct FDebuggerValue& Value) const
{
	if (Usage.SubTypes.Num() != 1)
	{
		return false;
	}

	const FAngelscriptTypeUsage& SubType = Usage.SubTypes[0];
	FAngelscriptOptional& Optional = Usage.ResolvePrimitive<FAngelscriptOptional>(Address);

	FOptionalOperations Ops(SubType);

	Value.Usage = Usage;
	Value.Address = Address;
	Value.bHasMembers = true;
	Value.Type = Usage.GetAngelscriptDeclaration();

	if (Ops.IsSet(Optional))
	{
		FDebuggerValue InnerValue;
		if (SubType.GetDebuggerValue(Ops.GetValuePtr(Optional), InnerValue))
		{
			Value.Value = TEXT("Set: ");
			Value.Value += InnerValue.Value;
		}
		else
		{
			Value.Value = TEXT("Set");
		}
	}
	else
	{
		Value.Value = TEXT("Unset");
	}

	return true;
}

bool FAngelscriptOptionalType::GetDebuggerScope(const FAngelscriptTypeUsage& Usage, void* Address, struct FDebuggerScope& Scope) const
{
	if (Usage.SubTypes.Num() != 1)
	{
		return false;
	}

	const FAngelscriptTypeUsage& SubType = Usage.SubTypes[0];
	FAngelscriptOptional& Optional = Usage.ResolvePrimitive<FAngelscriptOptional>(Address);

	FOptionalOperations Ops(SubType);
	if (Ops.IsSet(Optional))
	{
		void* Data = Ops.GetValuePtr(Optional);

		FDebuggerValue Value;
		if (SubType.GetDebuggerValue(Data, Value))
		{
			Value.Name = TEXT("Value");
			Scope.Values.Add(MoveTemp(Value));
		}

		return true;
	}

	return false;
}

bool FAngelscriptOptionalType::GetDebuggerMember(const FAngelscriptTypeUsage& Usage, void* Address, const FString& Member, struct FDebuggerValue& Value) const
{
	if (Usage.SubTypes.Num() != 1)
	{
		return false;
	}

	const FAngelscriptTypeUsage& SubType = Usage.SubTypes[0];
	FAngelscriptOptional& Optional = Usage.ResolvePrimitive<FAngelscriptOptional>(Address);

	FOptionalOperations Ops(SubType);

	void* Data = Ops.GetValuePtr(Optional);
	if (SubType.GetDebuggerValue(Data, Value))
	{
		Value.Name = TEXT("Value");
		return true;
	}

	return false;
}

bool FAngelscriptOptionalType::GetCppForm(const FAngelscriptTypeUsage& Usage, FCppForm& OutCppForm) const
{
	if (Usage.SubTypes.Num() != 1)
		return false;

	FCppForm CppInner;
	if (Usage.SubTypes[0].GetCppForm(CppInner))
	{
		if (CppInner.CppType.Len() != 0 && !CppInner.bDisallowNativeNest)
		{
			OutCppForm.CppType = FString::Printf(TEXT("TOptional<%s>"), *CppInner.CppType);
			OutCppForm.CppHeader = CppInner.CppHeader;
		}

		if (CppInner.CppGenericType.Len() != 0)
		{
			OutCppForm.CppGenericType = FString::Printf(TEXT("TOptional<%s>"), *CppInner.CppGenericType);
		}
	}

	OutCppForm.TemplateObjectForm = TEXT("FAngelscriptOptional");
	return true;
}
