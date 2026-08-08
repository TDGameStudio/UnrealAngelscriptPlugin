#include "Bind_Primitives.h"

#include "AngelscriptBinds.h"
#include "Binds/Bind_Helpers.h"

static const FName NAME_Property_ToolTip("ToolTip");
static const FName NAME_Property_Bool_DeprecatedProperty("DeprecatedProperty");
static const FName NAME_Property_Bool_DeprecationMessage("DeprecationMessage");

FString FIntType::GetAngelscriptTypeName() const
{
	return TEXT("int");
}

bool FIntType::GetCppForm(const FAngelscriptTypeUsage& Usage, FCppForm& OutCppForm) const
{
	OutCppForm.bIsPrimitive = true;
	OutCppForm.CppType = TEXT("int32");
	return true;
}

FString FUIntType::GetAngelscriptTypeName() const
{
	return TEXT("uint");
}

bool FUIntType::GetCppForm(const FAngelscriptTypeUsage& Usage, FCppForm& OutCppForm) const
{
	OutCppForm.bIsPrimitive = true;
	OutCppForm.CppType = TEXT("uint32");
	return true;
}

FString FBoolType::GetAngelscriptTypeName() const
{
	return TEXT("bool");
}

FProperty* FBoolType::CreateProperty(const FAngelscriptTypeUsage& Usage, const FAngelscriptType::FPropertyParams& Params) const
{
	auto* Property = new FBoolProperty(Params.Outer, Params.PropertyName);
	Property->SetPropertyFlags(CPF_HasGetValueTypeHash);
	Property->SetBoolSize(1, true, 255);
	return Property;
}

void FBoolType::SetArgument(const FAngelscriptTypeUsage& Usage, int32 ArgumentIndex, class asIScriptContext* Context, struct FFrame& Stack, const FArgData& Data) const
{
	bool* ValuePtr = (bool*)Data.StackPtr;

	// We need to clear the bool here, because our stack memory isn't necessarily
	// cleared, and bool properties only copy 1 bit, not the full byte for some reason.
	*ValuePtr = false;

	if (Usage.bIsReference)
	{
		bool& ObjRef = Stack.StepCompiledInRef<FBoolProperty, bool>(ValuePtr);
		Context->SetArgAddress(ArgumentIndex, &ObjRef);
	}
	else
	{
		Stack.StepCompiledIn<FBoolProperty>(ValuePtr);
		TSetAngelscriptArgument<bool>(Context, ArgumentIndex, *ValuePtr);
	}
}

bool FBoolType::BindProperty(const FAngelscriptTypeUsage& Usage, const FBindParams& Params, FProperty* NativeProperty) const
{
	// We need to do some special stuff for bool properties with bitmasks
	FBoolProperty* BoolProp = CastField<FBoolProperty>(NativeProperty);
	if (!ensure(BoolProp != nullptr))
		return false;
	if (BoolProp->IsNativeBool())
		return false;

	FString PropName;
	if (Params.NameOverride.Len() != 0)
		PropName = Params.NameOverride;
	else
		PropName = NativeProperty->GetName();

#if WITH_EDITOR
	bool bIsDeprecated = BoolProp->HasMetaData(NAME_Property_Bool_DeprecatedProperty);
	FString DeprecationMessage;
	if (bIsDeprecated)
		DeprecationMessage = BoolProp->GetMetaData(NAME_Property_Bool_DeprecationMessage);
#endif

	auto& BindClass = *Params.BindClass;
	if (Params.bCanRead && !BindClass.HasGetter(NativeProperty->GetName()))
	{
		FString Decl = FString::Printf(TEXT("bool Get%s() const"), *PropName);
		FAngelscriptBoundFunction Getter = BindClass.Method(Decl, FUNC_TRIVIAL(FAngelscriptBindHelpers::GetBoolFromProperty), (void*)NativeProperty)
			.PassScriptFunctionAsFirstParam();

#if WITH_EDITOR
		if (bIsDeprecated)
			Getter.Deprecated(TCHAR_TO_ANSI(*DeprecationMessage));

		Getter.GeneratedAccessor();

		const FString& Tooltip = BoolProp->GetMetaData(NAME_Property_ToolTip);
		if (Tooltip.Len() != 0)
			Getter.Documentation(Tooltip);

		if (BoolProp->HasAnyPropertyFlags(CPF_EditorOnly))
			Getter.EditorOnly();
#endif
	}

	if ((Params.bCanWrite || Params.bCanEdit) && !BindClass.HasSetter(NativeProperty->GetName()))
	{
		FString Decl = FString::Printf(TEXT("void Set%s(bool Value)"), *PropName);
		FAngelscriptBoundFunction Setter = BindClass.Method(Decl, FUNC_TRIVIAL(FAngelscriptBindHelpers::SetBoolFromProperty), Params, (void*)NativeProperty)
			.PassScriptFunctionAsFirstParam();

#if WITH_EDITOR
		if (bIsDeprecated)
			Setter.Deprecated(TCHAR_TO_ANSI(*DeprecationMessage));

		Setter.GeneratedAccessor();

		const FString& Tooltip = BoolProp->GetMetaData(NAME_Property_ToolTip);
		if (Tooltip.Len() != 0)
			Setter.Documentation(Tooltip);

		if (BoolProp->HasAnyPropertyFlags(CPF_EditorOnly))
			Setter.EditorOnly();
#endif
	}

	return true;
}

bool FBoolType::GetDebuggerValue(const FAngelscriptTypeUsage& Usage, void* Address, struct FDebuggerValue& Value) const
{
	Value.Type = Usage.GetAngelscriptDeclaration();
	Value.Usage = Usage;
	Value.Value = LexToString(Usage.ResolvePrimitive<bool>(Address));
	Value.Address = Address;
	Value.bHasMembers = false;
	return true;
}

bool FBoolType::GetDebuggerValue(const FAngelscriptTypeUsage& Usage, void* Address, struct FDebuggerValue& Value, FProperty* NativeProperty) const
{
	FBoolProperty* BoolProp = CastField<FBoolProperty>(NativeProperty);
	if (!ensure(BoolProp != nullptr))
		return false;
	if (BoolProp->IsNativeBool())
		return GetDebuggerValue(Usage, Address, Value);

	uint8* MemPtr = &Usage.ResolvePrimitive<uint8>(Address);
	bool BoolValue = BoolProp->GetPropertyValue(MemPtr);

	Value.Type = Usage.GetAngelscriptDeclaration();
	Value.Usage = Usage;
	Value.Value = LexToString(BoolValue);
	Value.Address = Address;
	Value.bHasMembers = false;
	return true;
}

FFloatType::FFloatType(const FString& InTypename) : Typename(InTypename)
{}

FString FFloatType::GetAngelscriptTypeName() const
{
	return Typename;
}

FString FFloatType::GetAngelscriptDeclaration(const FAngelscriptTypeUsage& Usage, EAngelscriptDeclarationMode Mode) const
{
	switch (Mode)
	{
	case EAngelscriptDeclarationMode::MemberVariable:
	case EAngelscriptDeclarationMode::MemberVariable_InContainer:
	case EAngelscriptDeclarationMode::FunctionArgument:
	case EAngelscriptDeclarationMode::FunctionReturnValue:
		return TEXT("float32");
	break;
	}

	return Typename;
}

#if AS_CAN_GENERATE_JIT
bool FFloatType::GetCppForm(const FAngelscriptTypeUsage& Usage, FAngelscriptType::FCppForm& OutCppForm) const
{
	OutCppForm.bIsPrimitive = true;
	OutCppForm.CppType = TEXT("float");
	return true;
}
#endif
FDoubleType::FDoubleType(const FString& InTypename) : Typename(InTypename)
{}

FString FDoubleType::GetAngelscriptTypeName() const
{
	return Typename;
}

FString FDoubleType::GetAngelscriptDeclaration(const FAngelscriptTypeUsage& Usage, EAngelscriptDeclarationMode Mode) const
{
	switch (Mode)
	{
	case EAngelscriptDeclarationMode::MemberVariable:
	case EAngelscriptDeclarationMode::MemberVariable_InContainer:
	case EAngelscriptDeclarationMode::FunctionArgument:
	case EAngelscriptDeclarationMode::FunctionReturnValue:
		return TEXT("float64");
	break;
	}

	return Typename;
}

#if AS_CAN_GENERATE_JIT
bool FDoubleType::GetCppForm(const FAngelscriptTypeUsage& Usage, FAngelscriptType::FCppForm& OutCppForm) const
{
	OutCppForm.bIsPrimitive = true;
	OutCppForm.CppType = TEXT("double");
	return true;
}
#endif

FString FInt64Type::GetAngelscriptTypeName() const
{
	return TEXT("int64");
}

FString FUInt64Type::GetAngelscriptTypeName() const
{
	return TEXT("uint64");
}

FString FInt16Type::GetAngelscriptTypeName() const
{
	return TEXT("int16");
}

FString FUInt16Type::GetAngelscriptTypeName() const
{
	return TEXT("uint16");
}

FString FInt8Type::GetAngelscriptTypeName() const
{
	return TEXT("int8");
}

FString FUInt8Type::GetAngelscriptTypeName() const
{
	return TEXT("uint8");
}

bool FUInt8Type::MatchesProperty(const FAngelscriptTypeUsage& Usage, const FProperty* Property, EPropertyMatchType MatchType) const
{
	if (auto* ByteProp = CastField<FByteProperty>(Property))
	{
		if (ByteProp->Enum == nullptr)
			return true;
	}

	return false;
}

FUnrealFloatParamExtendedToDoubleType::FUnrealFloatParamExtendedToDoubleType(const FString& InTypename) : Typename(InTypename)
{}

FString FUnrealFloatParamExtendedToDoubleType::GetAngelscriptTypeName() const
{
	return Typename;
}

bool FUnrealFloatParamExtendedToDoubleType::GetDebuggerValue(const FAngelscriptTypeUsage& Usage, void* Address, struct FDebuggerValue& Value) const
{
	Value.Usage = FAngelscriptType::ScriptDoubleType();
	Value.Type = Value.Usage.GetAngelscriptDeclaration();
	Value.Value = LexToString(Usage.ResolvePrimitive<double>(Address));
	Value.Address = Address;
	Value.bHasMembers = false;
	return true;
}

bool FUnrealFloatParamExtendedToDoubleType::MatchesProperty(const FAngelscriptTypeUsage& Usage, const FProperty* Property, EPropertyMatchType MatchType) const
{
	// A BlueprintEvent in C++ that has a float return or argument can be overridden with a double in script
	if (!Usage.bIsReference)
	{
		if (MatchType == EPropertyMatchType::OverrideArgument || MatchType == EPropertyMatchType::OverrideReturnValue)
		{
			if (Property->IsA<FFloatProperty>())
				return true;
		}
	}

	return false;
}

bool FUnrealFloatParamExtendedToDoubleType::CanBeArgument(const FAngelscriptTypeUsage& Usage) const
{
	return true;
}

void FUnrealFloatParamExtendedToDoubleType::SetArgument(const FAngelscriptTypeUsage& Usage, int32 ArgumentIndex, class asIScriptContext* Context, struct FFrame& Stack, const FAngelscriptType::FArgData& Data) const
{
	check(!Usage.bIsReference);

	float* ValuePtr = (float*)Data.StackPtr;
	Stack.StepCompiledIn<FFloatProperty>(ValuePtr);

	TSetAngelscriptArgument<double>(Context, ArgumentIndex, (double)*ValuePtr);
}

bool FUnrealFloatParamExtendedToDoubleType::CanBeReturned(const FAngelscriptTypeUsage& Usage) const
{
	return true;
}

void FUnrealFloatParamExtendedToDoubleType::GetReturnValue(const FAngelscriptTypeUsage& Usage, class asIScriptContext* Context, void* Destination) const
{
	check(!Usage.bIsReference);

	*(float*)Destination = (float)TGetAngelscriptReturnValue<double>(Context);
}

int32 FUnrealFloatParamExtendedToDoubleType::GetValueSize(const FAngelscriptTypeUsage& Usage) const
{
	return sizeof(float);
}

int32 FUnrealFloatParamExtendedToDoubleType::GetValueAlignment(const FAngelscriptTypeUsage& Usage) const
{
	return alignof(float);
}

bool FUnrealFloatParamExtendedToDoubleType::CanBeTemplateSubType() const
{
	return false;
}

#if AS_CAN_GENERATE_JIT
bool FUnrealFloatParamExtendedToDoubleType::GetCppForm(const FAngelscriptTypeUsage& Usage, FAngelscriptType::FCppForm& OutCppForm) const
{
	OutCppForm.bIsPrimitive = true;
	OutCppForm.CppType = TEXT("double");
	return true;
}
#endif
