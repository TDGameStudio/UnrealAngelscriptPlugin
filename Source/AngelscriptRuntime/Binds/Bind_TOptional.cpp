#include "Binds/Bind_TOptional.h"
#include "AngelscriptBinds.h"
#include "AngelscriptDocs.h"
#include "AngelscriptEngine.h"
#include "ClassGenerator/ASClass.h"
#include "UObject/GarbageCollectionSchema.h"
#include "UObject/PropertyOptional.h"

#include "StartAngelscriptHeaders.h"
//#include "as_context.h"
//#include "as_scriptengine.h"
//#include "as_scriptfunction.h"
#include "source/as_context.h"
#include "source/as_scriptengine.h"
#include "source/as_scriptfunction.h"
#include "EndAngelscriptHeaders.h"

struct ANGELSCRIPTRUNTIME_API FAngelscriptOptionalType final : FAngelscriptType
{
	virtual FString GetAngelscriptTypeName() const override
	{
		return TEXT("TOptional");
	}

	virtual bool CanQueryPropertyType() const override { return false; }
	virtual bool CanBeTemplateSubType() const override { return false; }
	virtual bool RequiresProperty(const FAngelscriptTypeUsage& Usage) const override { return false; }

	virtual bool HasReferences(const FAngelscriptTypeUsage& Usage) const override
	{
		return Usage.SubTypes.Num() == 1 && Usage.SubTypes[0].HasReferences();
	}

	void EmitReferenceInfo(const FAngelscriptTypeUsage& Usage, FGCReferenceParams& Params) const 
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

	virtual bool CanCopy(const FAngelscriptTypeUsage& Usage) const override
	{
    	return Usage.SubTypes.Num() == 1 && Usage.SubTypes[0].CanCopy()
			&& Usage.SubTypes[0].CanConstruct() && Usage.SubTypes[0].CanDestruct();
    }

	virtual bool CanCompare(const FAngelscriptTypeUsage& Usage) const override
	{
		return Usage.SubTypes.Num() == 1 && Usage.SubTypes[0].CanCompare();
	}

	virtual bool IsValueEqual(const FAngelscriptTypeUsage& Usage, void* SourcePtr, void* DestinationPtr) const override
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

	virtual bool NeedCopy(const FAngelscriptTypeUsage& Usage) const override { return true; }

	virtual void CopyValue(const FAngelscriptTypeUsage& Usage, void* SourcePtr, void* DestinationPtr) const override
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

	virtual bool CanCreateProperty(const FAngelscriptTypeUsage& Usage) const override
	{
		if (Usage.SubTypes.Num() != 1)
			return false;
		return Usage.SubTypes[0].CanCreateProperty();
	}

	virtual FProperty* CreateProperty(const FAngelscriptTypeUsage& Usage, const FPropertyParams& Params) const override
	{
		auto* OptionalProp = new FOptionalProperty(Params.Outer, Params.PropertyName);

		FPropertyParams InnerParams = Params;
		InnerParams.Outer = OptionalProp;
		InnerParams.PropertyName = *(Params.PropertyName.ToString() + TEXT("_Inner"));

		OptionalProp->SetValueProperty(Usage.SubTypes[0].CreateProperty(InnerParams));

		return OptionalProp;
	}

	virtual bool MatchesProperty(const FAngelscriptTypeUsage& Usage, const FProperty* Property, EPropertyMatchType MatchType) const override
	{
		if (Usage.SubTypes.Num() != 1)
			return false;

		const FOptionalProperty* OptionalProp = CastField<FOptionalProperty>(Property);
		if (OptionalProp == nullptr)
			return false;

		return Usage.SubTypes[0].MatchesProperty(OptionalProp->GetValueProperty(), FAngelscriptType::EPropertyMatchType::InContainer);
	}

	virtual bool CanConstruct(const FAngelscriptTypeUsage& Usage) const override
	{
		return Usage.SubTypes.Num() == 1;
	}

	virtual bool NeedConstruct(const FAngelscriptTypeUsage& Usage) const override { return true; }

	virtual void ConstructValue(const FAngelscriptTypeUsage& Usage, void* DestinationPtr) const override
	{
		int32 ElementSize = Usage.SubTypes[0].GetValueSize();
		*(bool*)((SIZE_T)DestinationPtr + ElementSize) = false;
	}

	virtual bool CanDestruct(const FAngelscriptTypeUsage& Usage) const override
	{
		return Usage.SubTypes.Num() == 1 && Usage.SubTypes[0].CanDestruct();
	}

	virtual bool NeedDestruct(const FAngelscriptTypeUsage& Usage) const override
	{
		return Usage.SubTypes[0].NeedDestruct();
	}

	virtual void DestructValue(const FAngelscriptTypeUsage& Usage, void* DestinationPtr) const override
	{
		int32 ElementSize = Usage.SubTypes[0].GetValueSize();
		if (*(bool*)((SIZE_T)DestinationPtr + ElementSize))
			Usage.SubTypes[0].DestructValue(DestinationPtr);
	}

	virtual int32 GetValueSize(const FAngelscriptTypeUsage& Usage) const override
	{
		return Align(Usage.SubTypes[0].GetValueSize() + 1, Usage.SubTypes[0].GetValueAlignment());
	}

	virtual int32 GetValueAlignment(const FAngelscriptTypeUsage& Usage) const override
	{
		return Usage.SubTypes[0].GetValueAlignment();
	}

	virtual bool CanBeArgument(const FAngelscriptTypeUsage& Usage) const override { return false; }

	virtual void SetArgument(const FAngelscriptTypeUsage& Usage, int32 ArgumentIndex, class asIScriptContext* Context, struct FFrame& Stack, const FArgData& Data) const override
	{
		check(false);
	}

	virtual bool CanBeReturned(const FAngelscriptTypeUsage& Usage) const override { return true; }

	virtual void GetReturnValue(const FAngelscriptTypeUsage& Usage, class asIScriptContext* Context, void* Destination) const override
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

	virtual bool GetDebuggerValue(const FAngelscriptTypeUsage& Usage, void* Address, struct FDebuggerValue& Value) const override
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

	virtual bool GetDebuggerScope(const FAngelscriptTypeUsage& Usage, void* Address, struct FDebuggerScope& Scope) const override
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

	virtual bool GetDebuggerMember(const FAngelscriptTypeUsage& Usage, void* Address, const FString& Member, struct FDebuggerValue& Value) const override
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

	bool GetCppForm(const FAngelscriptTypeUsage& Usage, FCppForm& OutCppForm) const override
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
};

FOptionalOperations::FOptionalOperations(const FAngelscriptTypeUsage& Usage)
{
	bValid = Usage.IsValid() && Usage.CanConstruct() && Usage.CanDestruct() && Usage.CanCopy();

	if (!bValid)
		return;

	Type = Usage;
	bNeedConstruct = Usage.NeedConstruct();
	bNeedDestruct = Usage.NeedDestruct();
	bNeedCopy = Usage.NeedCopy();
	TypeSize = Usage.GetValueSize();
	Alignment = Usage.GetValueAlignment();
}

FOptionalOperations* FOptionalOperations::ValidateOptionalOperations(asITypeInfo* TemplateType, asCString* ErrorMessage)
{
	if (FOptionalOperations* Ops = static_cast<FOptionalOperations*>(TemplateType->GetUserData()))
	{
		return Ops->bValid ? Ops : nullptr;
	}

	const FAngelscriptTypeUsage Type = FAngelscriptTypeUsage::FromTypeId(TemplateType->GetSubTypeId(0));

	if (!Type.CanBeTemplateSubType())
	{
		if (ErrorMessage != nullptr)
			*ErrorMessage = "Containers cannot be nested in other containers";
		return nullptr;
	}

	FOptionalOperations* Ops = new FOptionalOperations(Type);
	TemplateType->SetUserData(Ops);

	return Ops->bValid ? Ops : nullptr;
}

void FOptionalOperations::Reset(FAngelscriptOptional& Optional)
{
	if (IsSet(Optional))
	{
		if (bNeedDestruct)
			Type.DestructValue(GetValuePtr(Optional));
		*GetIsSetPtr(Optional) = false;
	}
}

void FOptionalOperations::StoreValue(void* DestinationPtr, void* ValuePtr)
{
	if (ValuePtr == nullptr && Type.IsObjectPointer())
	{
		FMemory::Memzero(DestinationPtr, TypeSize);
		return;
	}

	if (bNeedCopy)
		Type.CopyValue(ValuePtr, DestinationPtr);
	else
		FMemory::Memcpy(DestinationPtr, ValuePtr, TypeSize);
}

void FOptionalOperations::Set(FAngelscriptOptional& Optional, void* ValuePtr)
{
	void* DestinationPtr = GetValuePtr(Optional);
	if (!IsSet(Optional))
	{
		*GetIsSetPtr(Optional) = true;
		if (bNeedConstruct)
		Type.ConstructValue(DestinationPtr);
	}

	StoreValue(DestinationPtr, ValuePtr);
}

void FAngelscriptOptionalBinds::CopyConstruct(FAngelscriptOptional& Optional, asCObjectType* Meta, FAngelscriptOptional& Other)
{
	auto* Ops = FOptionalOperations::GetOptionalOperations(Meta);

	if (Ops->IsSet(Other))
	{
		void* DestinationPtr = Ops->GetValuePtr(Optional);
		void* SourcePtr = Ops->GetValuePtr(Other);

		if (Ops->bNeedConstruct)
			Ops->Type.ConstructValue(DestinationPtr);

		if (Ops->bNeedCopy)
			Ops->Type.CopyValue(SourcePtr, DestinationPtr);
		else
			FMemory::Memcpy(DestinationPtr, SourcePtr, Ops->TypeSize);

		*Ops->GetIsSetPtr(Optional) = true;
	}
	else
	{
		*Ops->GetIsSetPtr(Optional) = false;
	}
}

void FAngelscriptOptionalBinds::InitConstruct(FAngelscriptOptional& Optional, asCObjectType* Meta, void* ValuePtr)
{
	auto* Ops = FOptionalOperations::GetOptionalOperations(Meta);
	void* DestinationPtr = Ops->GetValuePtr(Optional);

	if (Ops->bNeedConstruct)
		Ops->Type.ConstructValue(DestinationPtr);

	Ops->StoreValue(DestinationPtr, ValuePtr);

	*Ops->GetIsSetPtr(Optional) = true;
}

bool FAngelscriptOptionalBinds::IsSet(FAngelscriptOptional& Optional, asCObjectType* Meta)
{
	auto* Ops = FOptionalOperations::GetOptionalOperations(Meta);
	return Ops->IsSet(Optional);
}

bool FAngelscriptOptionalBinds::OpEquals(FAngelscriptOptional& Optional, asCObjectType* Meta, FAngelscriptOptional& Other)
{
	auto* Ops = FOptionalOperations::GetOptionalOperations(Meta);
	if (*Ops->GetIsSetPtr(Optional))
	{
		if (!*Ops->GetIsSetPtr(Other))
			return false;

		if (!Ops->Type.IsValueEqual(Ops->GetValuePtr(Optional), Ops->GetValuePtr(Other)))
			return false;
	}
	else
	{
		if (*Ops->GetIsSetPtr(Other))
			return false;
	}

	return true;
}

FAngelscriptOptional& FAngelscriptOptionalBinds::OpAssign(FAngelscriptOptional& Optional, asCObjectType* Meta, FAngelscriptOptional& Other)
{
	auto* Ops = FOptionalOperations::GetOptionalOperations(Meta);

	if (Ops->IsSet(Other))
		Ops->Set(Optional, Ops->GetValuePtr(Other));
	else
		Ops->Reset(Optional);

	return Optional;
}

FAngelscriptOptional& FAngelscriptOptionalBinds::OpAssignValue(FAngelscriptOptional& Optional, asCObjectType* Meta, void* ValuePtr)
{
	auto* Ops = FOptionalOperations::GetOptionalOperations(Meta);
	Ops->Set(Optional, ValuePtr);
	return Optional;
}

void FAngelscriptOptionalBinds::Set(FAngelscriptOptional& Optional, asCObjectType* Meta, void* ValuePtr)
{
	auto* Ops = FOptionalOperations::GetOptionalOperations(Meta);
	Ops->Set(Optional, ValuePtr);
}

void FAngelscriptOptionalBinds::Reset(FAngelscriptOptional& Optional, asCObjectType* Meta)
{
	auto* Ops = FOptionalOperations::GetOptionalOperations(Meta);
	Ops->Reset(Optional);
}

void* FAngelscriptOptionalBinds::GetValue(FAngelscriptOptional& Optional, asCObjectType* Meta)
{
	auto* Ops = FOptionalOperations::GetOptionalOperations(Meta);
	if (!Ops->IsSet(Optional))
		FAngelscriptEngine::Throw("GetValue() called on Optional when not set! Check the optional with IsSet() first.");
	return Ops->GetValuePtr(Optional);
}

void* FAngelscriptOptionalBinds::Get(FAngelscriptOptional& Optional, asCObjectType* Meta, void* DefaultValuePtr)
{
	auto* Ops = FOptionalOperations::GetOptionalOperations(Meta);
	if (!Ops->IsSet(Optional))
		return DefaultValuePtr;
	return Ops->GetValuePtr(Optional);
}

AS_FORCE_LINK const FAngelscriptBinds::FBind Bind_TOptional(FAngelscriptBinds::EOrder::Early, []
{
	auto OptionalType = MakeShared<FAngelscriptOptionalType>();
	FAngelscriptType::Register(OptionalType);
	FAngelscriptType::RegisterTypeFinder([OptionalType](FProperty* Property, FAngelscriptTypeUsage& Usage) -> bool
	{
		FOptionalProperty* OptionalProp = CastField<FOptionalProperty>(Property);
		if (OptionalProp == nullptr)
			return false;
		if ((OptionalProp->GetPropertyFlags() & CPF_NonNullable) != 0)
			return false;

		FAngelscriptTypeUsage InnerUsage = FAngelscriptTypeUsage::FromProperty(OptionalProp->GetValueProperty());
		if (!InnerUsage.IsValid())
			return false;

		Usage.Type = OptionalType;
		Usage.SubTypes.Add(InnerUsage);
		return true;
	});

	FBindFlags Flags;
	Flags.bTemplate = true;
	Flags.TemplateType = "<T>";
	Flags.ExtraFlags |= asOBJ_TEMPLATE_SUBTYPE_DETERMINES_SIZE;
	Flags.ExtraFlags |= asOBJ_TEMPLATE_SUBTYPE_COVARIANT;
	Flags.Alignment = 1;
	auto TOptional_ = FAngelscriptBinds::ValueClass("TOptional<class T>", sizeof(bool), Flags);

	TOptional_.Constructor("void f()", &FAngelscriptOptionalBinds::Construct);
	SCRIPT_NATIVE_TEMPLATED_CALL(TOptional_, "FAngelscriptOptionalBinds::Construct", false);
	FAngelscriptBinds::PreviousBindPassScriptObjectTypeAsFirstParam();

	TOptional_.ImplicitConstructor("void f(const T&in if_handle_then_const Other)", &FAngelscriptOptionalBinds::InitConstruct);
	SCRIPT_NATIVE_TEMPLATED_CALL_NEEDSCOPY(TOptional_, "FAngelscriptOptionalBinds::InitConstruct", false);
	FAngelscriptBinds::PreviousBindPassScriptObjectTypeAsFirstParam();
	SCRIPT_BIND_DOCUMENTATION("Initialize the optional with a valid value.\n")

	TOptional_.Constructor("void f(const TOptional<T>& Other)", &FAngelscriptOptionalBinds::CopyConstruct);
	SCRIPT_NATIVE_TEMPLATED_CALL_NEEDSCOPY(TOptional_, "FAngelscriptOptionalBinds::CopyConstruct", false);
	FAngelscriptBinds::PreviousBindPassScriptObjectTypeAsFirstParam();

	TOptional_.Destructor("void f()", &FAngelscriptOptionalBinds::Destruct);
	SCRIPT_NATIVE_TEMPLATED_CALL(TOptional_, "FAngelscriptOptionalBinds::Destruct", false);
	FAngelscriptBinds::PreviousBindPassScriptObjectTypeAsFirstParam();

	TOptional_.TemplateCallback("bool f(int&in Type, int&out ErrorMessage)",
		[](asITypeInfo* TemplateType, asCString* ErrorMessage) -> bool
	{
		return FOptionalOperations::ValidateOptionalOperations(TemplateType, ErrorMessage) != nullptr;
	});

	TOptional_.Method("TOptional<T>& opAssign(const TOptional<T>& Other)", &FAngelscriptOptionalBinds::OpAssign);
	SCRIPT_NATIVE_TEMPLATED_CALL_NEEDSCOPY(TOptional_, "FAngelscriptOptionalBinds::OpAssign", false);
	FAngelscriptBinds::PreviousBindPassScriptObjectTypeAsFirstParam();

	TOptional_.Method("TOptional<T>& opAssign(const T&in if_handle_then_const Value)", &FAngelscriptOptionalBinds::OpAssignValue);
	SCRIPT_NATIVE_TEMPLATED_CALL_NEEDSCOPY(TOptional_, "FAngelscriptOptionalBinds::OpAssignValue", false);
	FAngelscriptBinds::PreviousBindPassScriptObjectTypeAsFirstParam();

	TOptional_.Method("bool opEquals(const TOptional<T>& Other) const", &FAngelscriptOptionalBinds::OpEquals);
	SCRIPT_NATIVE_TEMPLATED_CALL_NEEDSCOMPARE(TOptional_, "FAngelscriptOptionalBinds::OpEquals", false);
	FAngelscriptBinds::PreviousBindPassScriptObjectTypeAsFirstParam();

	TOptional_.Method("bool IsSet() const", &FAngelscriptOptionalBinds::IsSet);
	SCRIPT_NATIVE_TEMPLATED_CALL(TOptional_, "FAngelscriptOptionalBinds::IsSet", false);
	FAngelscriptBinds::PreviousBindPassScriptObjectTypeAsFirstParam();
	SCRIPT_BIND_DOCUMENTATION("Returns if the optional has a valid value. This must be true in order for Get() or GetValue() to be called.\n")

	TOptional_.Method("void Set(const T&in if_handle_then_const Value) const", &FAngelscriptOptionalBinds::Set);
	SCRIPT_NATIVE_TEMPLATED_CALL_NEEDSCOPY(TOptional_, "FAngelscriptOptionalBinds::Set", false);
	FAngelscriptBinds::PreviousBindPassScriptObjectTypeAsFirstParam();

	TOptional_.Method("const T& GetValue() const", &FAngelscriptOptionalBinds::GetValue);
	SCRIPT_NATIVE_TEMPLATED_CALL(TOptional_, "FAngelscriptOptionalBinds::GetValue", false);
	FAngelscriptBinds::PreviousBindPassScriptObjectTypeAsFirstParam();
	SCRIPT_BIND_DOCUMENTATION("Gets a const reference to the optional's set value. IsSet() must return true for this function to be called.\n")

	TOptional_.Method("T& GetValue()", &FAngelscriptOptionalBinds::GetValue);
	SCRIPT_NATIVE_TEMPLATED_CALL(TOptional_, "FAngelscriptOptionalBinds::GetValue", false);
	FAngelscriptBinds::PreviousBindPassScriptObjectTypeAsFirstParam();
	SCRIPT_BIND_DOCUMENTATION("Gets a non-const reference to the optional's set value. IsSet() must return true for this function to be called.\n")

	TOptional_.Method("const T& Get(const T&in if_handle_then_const DefaultValue) const", &FAngelscriptOptionalBinds::Get);
	SCRIPT_NATIVE_TEMPLATED_CALL_NEEDSCOPY(TOptional_, "FAngelscriptOptionalBinds::Get", false);
	FAngelscriptBinds::PreviousBindPassScriptObjectTypeAsFirstParam();
	SCRIPT_BIND_DOCUMENTATION("If set returns the optional's set value, otherwise returns DefaultValue")

	TOptional_.Method("void Reset()", &FAngelscriptOptionalBinds::Reset);
	SCRIPT_NATIVE_TEMPLATED_CALL(TOptional_, "FAngelscriptOptionalBinds::Reset", false);
	FAngelscriptBinds::PreviousBindPassScriptObjectTypeAsFirstParam();
	SCRIPT_BIND_DOCUMENTATION("Destruct the value inside the optional and unset it.\n")

});
