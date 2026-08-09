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

/**
 * TOptional template construction, value access, assignment, and state management.
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | AngelScript usage signature                                                                          | Purpose / parameter notes                                                                                        |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | TOptional<T> Optional();                                                                             | Constructs an unset optional.                                                                                    |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | TOptional<T> Optional(const T&in if_handle_then_const Other);                                        | Implicitly constructs a set optional from a value.                                                               |
 * |                                                                                                      | @param Other Initial contained value.                                                                            |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | TOptional<T> Optional(const TOptional<T>& Other);                                                    | Copy-constructs an optional, preserving its set state.                                                           |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | Left = Right;                                                                                        | Copies another optional, including an unset state.                                                               |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | Optional = Value;                                                                                    | Stores a value and marks the optional as set.                                                                    |
 * |                                                                                                      | @param Value Value to copy into the optional.                                                                    |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | bool bEqual = Left == Right;                                                                         | Compares set state and, when set, the contained value.                                                           |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | bool bIsSet = Optional.IsSet() const;                                                                | Reports whether a contained value is present.                                                                    |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | Optional.Set(const T&in if_handle_then_const Value) const;                                           | Stores a value and marks the optional as set.                                                                    |
 * |                                                                                                      | @param Value Value to copy into the optional.                                                                    |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | const T& Value = Optional.GetValue() const;                                                          | Returns a const reference to the contained value; throws when unset.                                             |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | T& Value = Optional.GetValue();                                                                      | Returns a mutable reference to the contained value; throws when unset.                                           |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | const T& Value = Optional.Get(const T&in if_handle_then_const DefaultValue) const;                   | Returns the contained value when set, otherwise the supplied reference.                                          |
 * |                                                                                                      | @param DefaultValue Fallback reference returned while the optional is unset.                                     |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | Optional.Reset();                                                                                    | Destroys the contained value and marks the optional as unset.                                                    |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 */

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

bool FAngelscriptOptionalBinds::ValidateTemplate(asITypeInfo* TemplateType, asCString* ErrorMessage)
{
	return FOptionalOperations::ValidateOptionalOperations(TemplateType, ErrorMessage) != nullptr;
}




AS_FORCE_LINK const FAngelscriptBind Bind_TOptional_TypeDeclarations(
	TEXT("TOptional.Declaration"),
	EAngelscriptBindPhase::TypeDeclarations,
	[](FAngelscriptBinds& Binds)
	{
		FBindFlags Flags;
		Flags.bTemplate = true;
		Flags.TemplateType = "<T>";
		Flags.ExtraFlags |= asOBJ_TEMPLATE_SUBTYPE_DETERMINES_SIZE;
		Flags.ExtraFlags |= asOBJ_TEMPLATE_SUBTYPE_COVARIANT;
		Flags.Alignment = 1;
		Binds.ValueClassForTarget("TOptional<class T>", sizeof(bool), Flags);
	});

AS_FORCE_LINK const FAngelscriptBind Bind_TOptional_MethodSurface(
	TEXT("TOptional.MethodSurface"),
	EAngelscriptBindPhase::TypeInfrastructure,
	[](FAngelscriptBinds& Binds)
	{
		auto TOptional_ = Binds.ExistingClassForTarget("TOptional<T>");

		TOptional_.Constructor("void f()", &FAngelscriptOptionalBinds::Construct)
			.PassScriptObjectTypeAsFirstParam()
			.NativeTemplateInstantiatedCall("FAngelscriptOptionalBinds::Construct", false, false, false);

		TOptional_.ImplicitConstructor("void f(const T&in if_handle_then_const Other)", &FAngelscriptOptionalBinds::InitConstruct)
			.PassScriptObjectTypeAsFirstParam()
			.NativeTemplateInstantiatedCall("FAngelscriptOptionalBinds::InitConstruct", false, false, true)
			.Documentation(TEXT("Initialize the optional with a valid value.\n"));

		TOptional_.Constructor("void f(const TOptional<T>& Other)", &FAngelscriptOptionalBinds::CopyConstruct)
			.PassScriptObjectTypeAsFirstParam()
			.NativeTemplateInstantiatedCall("FAngelscriptOptionalBinds::CopyConstruct", false, false, true);

		TOptional_.Destructor("void f()", &FAngelscriptOptionalBinds::Destruct)
			.PassScriptObjectTypeAsFirstParam()
			.NativeTemplateInstantiatedCall("FAngelscriptOptionalBinds::Destruct", false, false, false);

		TOptional_.TemplateCallback(
			"bool f(int&in Type, int&out ErrorMessage)",
			&FAngelscriptOptionalBinds::ValidateTemplate);

		TOptional_.Method("TOptional<T>& opAssign(const TOptional<T>& Other)", &FAngelscriptOptionalBinds::OpAssign)
			.PassScriptObjectTypeAsFirstParam()
			.NativeTemplateInstantiatedCall("FAngelscriptOptionalBinds::OpAssign", false, false, true);

		TOptional_.Method("TOptional<T>& opAssign(const T&in if_handle_then_const Value)", &FAngelscriptOptionalBinds::OpAssignValue)
			.PassScriptObjectTypeAsFirstParam()
			.NativeTemplateInstantiatedCall("FAngelscriptOptionalBinds::OpAssignValue", false, false, true);

		TOptional_.Method("bool opEquals(const TOptional<T>& Other) const", &FAngelscriptOptionalBinds::OpEquals)
			.PassScriptObjectTypeAsFirstParam()
			.NativeTemplateInstantiatedCall("FAngelscriptOptionalBinds::OpEquals", false, true, false);

		TOptional_.Method("bool IsSet() const", &FAngelscriptOptionalBinds::IsSet)
			.PassScriptObjectTypeAsFirstParam()
			.NativeTemplateInstantiatedCall("FAngelscriptOptionalBinds::IsSet", false, false, false)
			.Documentation(TEXT("Returns if the optional has a valid value. This must be true in order for Get() or GetValue() to be called.\n"));

		TOptional_.Method("void Set(const T&in if_handle_then_const Value) const", &FAngelscriptOptionalBinds::Set)
			.PassScriptObjectTypeAsFirstParam()
			.NativeTemplateInstantiatedCall("FAngelscriptOptionalBinds::Set", false, false, true);

		TOptional_.Method("const T& GetValue() const", &FAngelscriptOptionalBinds::GetValue)
			.PassScriptObjectTypeAsFirstParam()
			.NativeTemplateInstantiatedCall("FAngelscriptOptionalBinds::GetValue", false, false, false)
			.Documentation(TEXT("Gets a const reference to the optional's set value. IsSet() must return true for this function to be called.\n"));

		TOptional_.Method("T& GetValue()", &FAngelscriptOptionalBinds::GetValue)
			.PassScriptObjectTypeAsFirstParam()
			.NativeTemplateInstantiatedCall("FAngelscriptOptionalBinds::GetValue", false, false, false)
			.Documentation(TEXT("Gets a non-const reference to the optional's set value. IsSet() must return true for this function to be called.\n"));

		TOptional_.Method("const T& Get(const T&in if_handle_then_const DefaultValue) const", &FAngelscriptOptionalBinds::Get)
			.PassScriptObjectTypeAsFirstParam()
			.NativeTemplateInstantiatedCall("FAngelscriptOptionalBinds::Get", false, false, true)
			.Documentation(TEXT("If set returns the optional's set value, otherwise returns DefaultValue"));

		TOptional_.Method("void Reset()", &FAngelscriptOptionalBinds::Reset)
			.PassScriptObjectTypeAsFirstParam()
			.NativeTemplateInstantiatedCall("FAngelscriptOptionalBinds::Reset", false, false, false)
			.Documentation(TEXT("Destruct the value inside the optional and unset it.\n"));
	});

AS_FORCE_LINK const FAngelscriptBind Bind_TOptional_TypeInfrastructure(
	TEXT("TOptional.TypeInfrastructure"),
	EAngelscriptBindPhase::TypeInfrastructure,
	[](FAngelscriptBinds& Binds)
	{
		auto OptionalType = MakeShared<FAngelscriptOptionalType>();
		Binds.RegisterTypeForTarget(OptionalType);

		FAngelscriptTypeDatabase* TargetTypeDatabase = &Binds.GetTargetTypeDatabase();
		Binds.RegisterTypeFinderForTarget([OptionalType, TargetTypeDatabase](FProperty* Property, FAngelscriptTypeUsage& Usage) -> bool
		{
			FOptionalProperty* OptionalProp = CastField<FOptionalProperty>(Property);
			if (OptionalProp == nullptr)
				return false;
			if ((OptionalProp->GetPropertyFlags() & CPF_NonNullable) != 0)
				return false;

			FAngelscriptTypeUsage InnerUsage = FAngelscriptTypeUsage::FromProperty(*TargetTypeDatabase, OptionalProp->GetValueProperty());
			if (!InnerUsage.IsValid())
				return false;

			Usage.Type = OptionalType;
			Usage.SubTypes.Add(InnerUsage);
			return true;
		});
	});
