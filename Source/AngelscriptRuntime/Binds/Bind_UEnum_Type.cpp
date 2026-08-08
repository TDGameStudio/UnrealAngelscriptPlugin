#include "Bind_UEnum.h"

#include "AngelscriptBindDatabase.h"
#include "AngelscriptDebugValue.h"

#include "Engine/UserDefinedEnum.h"
#include "UObject/ScriptMacros.h"
#include "UObject/UnrealType.h"

#include "StartAngelscriptHeaders.h"
#include "AngelscriptInclude.h"
#include "source/as_objecttype.h"
#include "EndAngelscriptHeaders.h"

static const FName NAME_ENUM_UnderlyingType("UnderlyingType");

FEnumType::FEnumType(UEnum* InEnum, const FAngelscriptBindDatabase& InBindDatabase)
	: Enum(InEnum)
	, BindDatabase(&InBindDatabase)
{}

bool FEnumType::IsPrimitive() const
{
	return true;
}

FString FEnumType::GetAngelscriptTypeName() const
{
	if (Enum == nullptr)
	{
		ensure(false); // Should not happen
		return "int";
	}
	return Enum->GetName();
}

FString FEnumType::GetAngelscriptTypeName(const FAngelscriptTypeUsage& Usage) const
{
	if (Enum != nullptr)
		return Enum->GetName();
	else if (Usage.ScriptClass != nullptr)
		return ANSI_TO_TCHAR(Usage.ScriptClass->GetName());

	ensure(false);
	return TEXT("");
}

void* FEnumType::GetData() const
{
	return Enum;
}

bool FEnumType::CanCreateProperty(const FAngelscriptTypeUsage& Usage) const
{
	return Enum != nullptr || Usage.ScriptClass != nullptr;
}

bool FEnumType::CanQueryPropertyType() const
{
	return false;
}

bool FEnumType::MatchesProperty(const FAngelscriptTypeUsage& Usage, const FProperty* Property, EPropertyMatchType MatchType) const
{
	auto* EnumProp = CastField<FEnumProperty>(Property);
	UEnum* PropertyEnum = nullptr;
	if (EnumProp != nullptr)
	{
		//PropertyEnum = EnumProp->Enum;
		PropertyEnum = EnumProp->GetEnum();
	}
	else
	{
		auto* ByteProp = CastField<FByteProperty>(Property);
		if (ByteProp != nullptr)
			PropertyEnum = ByteProp->Enum;
	}

	if (PropertyEnum == nullptr)
		return false;

	auto* UsedEnum = Enum != nullptr ? Enum : (UEnum*)Usage.ScriptClass->GetUserData();
	return PropertyEnum == UsedEnum;
}

FProperty* FEnumType::CreateProperty(const FAngelscriptTypeUsage& Usage, const FPropertyParams& Params) const
{
	auto* UsedEnum = Enum != nullptr ? Enum : (UEnum*)Usage.ScriptClass->GetUserData();
	check(UsedEnum);

	if (UsedEnum->GetCppForm() == UEnum::ECppForm::EnumClass || UsedEnum->IsA<UUserDefinedEnum>())
	{
		auto* EnumProp = new FEnumProperty(Params.Outer, Params.PropertyName);
		auto* ByteProp = new FByteProperty(EnumProp, NAME_ENUM_UnderlyingType);

		EnumProp->SetEnum(UsedEnum);
		EnumProp->AddCppProperty(ByteProp);

		return EnumProp;
	}
	else
	{
		auto* ByteProp = new FByteProperty(Params.Outer, Params.PropertyName);
		ByteProp->Enum = UsedEnum;
		return ByteProp;
	}
}

bool FEnumType::IsTypeEquivalent(const FAngelscriptTypeUsage& Usage, const FAngelscriptTypeUsage& Other) const
{
	// C++ enums have individual type instances, so we don't need to check this
	if (Enum != nullptr)
		return true;

	// If the scriptclass is identical we don't need to check it
	if (Usage.ScriptClass == Other.ScriptClass)
		return true;

	// Shouldn't happen, safety check
	if (Usage.ScriptClass == nullptr || Other.ScriptClass == nullptr)
		return false;

	// Compare script enums by name, because we are likely comparing for changes during a compile
	if (((asCTypeInfo*)Usage.ScriptClass)->name == ((asCTypeInfo*)Other.ScriptClass)->name)
		return true;

	return false;
}

bool FEnumType::CanCopy(const FAngelscriptTypeUsage& Usage) const
{ return true; }

bool FEnumType::NeedCopy(const FAngelscriptTypeUsage& Usage) const
{ return true; }

void FEnumType::CopyValue(const FAngelscriptTypeUsage& Usage, void* SourcePtr, void* DestinationPtr) const
{
	*(uint8*)DestinationPtr = *(uint8*)SourcePtr;
}

bool FEnumType::CanCompare(const FAngelscriptTypeUsage& Usage) const
{ return true; }

bool FEnumType::IsValueEqual(const FAngelscriptTypeUsage& Usage, void* SourcePtr, void* DestinationPtr) const
{
	return *(uint8*)DestinationPtr == *(uint8*)SourcePtr;
}

bool FEnumType::CanConstruct(const FAngelscriptTypeUsage& Usage) const
{ return true; }

bool FEnumType::NeedConstruct(const FAngelscriptTypeUsage& Usage) const
{ return false; }

void FEnumType::ConstructValue(const FAngelscriptTypeUsage& Usage, void* DestinationPtr) const
{}

bool FEnumType::CanDestruct(const FAngelscriptTypeUsage& Usage) const
{ return true; }

bool FEnumType::NeedDestruct(const FAngelscriptTypeUsage& Usage) const
{ return false; }

void FEnumType::DestructValue(const FAngelscriptTypeUsage& Usage, void* DestinationPtr) const
{}

int32 FEnumType::GetValueSize(const FAngelscriptTypeUsage& Usage) const
{
	return 1;
}

bool FEnumType::CanBeArgument(const FAngelscriptTypeUsage& Usage) const
{ return true; }

void FEnumType::SetArgument(const FAngelscriptTypeUsage& Usage, int32 ArgumentIndex, class asIScriptContext* Context, struct FFrame& Stack, const FArgData& Data) const
{
	uint8* ValuePtr = (uint8*)Data.StackPtr;
	if (Usage.bIsReference)
	{
		uint8& ObjRef = Stack.StepCompiledInRef<FEnumProperty, uint8>(ValuePtr);
		Context->SetArgAddress(ArgumentIndex, &ObjRef);
	}
	else
	{
		Stack.StepCompiledIn<FEnumProperty>(ValuePtr);
		Context->SetArgByte(ArgumentIndex, *ValuePtr);
	}
}

bool FEnumType::CanBeReturned(const FAngelscriptTypeUsage& Usage) const
{
	return !Usage.bIsReference;
}

void FEnumType::GetReturnValue(const FAngelscriptTypeUsage& Usage, class asIScriptContext* Context, void* Destination) const
{
	*(uint8*)Destination = (uint8)Context->GetReturnByte();
}

bool FEnumType::DefaultValue_UnrealToAngelscript(const FAngelscriptTypeUsage& Usage, const FString& InValue, FString& OutValue) const
{
	OutValue = InValue;
	if (!OutValue.Contains(TEXT("::")))
	{
		FString EnumName = Enum != nullptr ? Enum->GetName() : ANSI_TO_TCHAR(Usage.ScriptClass->GetName());
		if (OutValue.Len() == 0)
		{
			if (Enum == nullptr)
				return false;

			// Unreal can send us an empty value if this is the 0 value for the enum
			OutValue = Enum->GetNameStringByValue(0);
			OutValue = FString::Printf(TEXT("%s::%s"), *EnumName, *OutValue);
		}
		else
		{
			OutValue = FString::Printf(TEXT("%s::%s"), *EnumName, *OutValue);
		}
	}
	return true;
}

bool FEnumType::DefaultValue_AngelscriptToUnreal(const FAngelscriptTypeUsage& Usage, const FString& InValue, FString& OutValue) const
{
	OutValue = InValue;
	int32 ScopePos = OutValue.Find(TEXT("::"));
	if (ScopePos != -1)
	{
		OutValue = OutValue.Mid(ScopePos+2);
		OutValue.TrimStartAndEndInline();
	}
	return true;
}

int32 FEnumType::GetValueAlignment(const FAngelscriptTypeUsage& Usage) const
{
	return 1;
}

bool FEnumType::CanHashValue(const FAngelscriptTypeUsage& Usage) const
{
	return true;
}

uint32 FEnumType::GetHash(const FAngelscriptTypeUsage& Usage, const void* Address) const
{
	return GetTypeHash(*(uint8*)Address);
}

FASDebugValue* FEnumType::CreateDebugValue(const FAngelscriptTypeUsage& Usage, FDebugValuePrototype& Values, int32 Offset) const
{
	if(Usage.bIsReference)
		return Values.Create<TDebug<uint8*>>(Offset);
	else
		return Values.Create<TDebug<uint8>>(Offset);
}

bool FEnumType::GetDebuggerValue(const FAngelscriptTypeUsage& Usage, void* Address, struct FDebuggerValue& Value) const
{
	auto* UsedEnum = Enum != nullptr ? Enum : (UEnum*)Usage.ScriptClass->GetUserData();
	int32 EnumValue = 0;

	EnumValue = Usage.ResolvePrimitive<uint8>(Address);
	FString EnumName = UsedEnum->GetNameByValue(EnumValue).ToString();

	Value.Type = Usage.GetAngelscriptDeclaration();
	Value.Usage = Usage;
	Value.Address = Address;
	Value.Value = FString::Printf(TEXT("%s (%d)"), *EnumName, EnumValue);

	return true;
}

bool FEnumType::GetCppForm(const FAngelscriptTypeUsage& Usage, FCppForm& OutCppForm) const
{
	if (Enum == nullptr)
	{
		// Script enums are _always_ 1 byte
		check(GetValueSize(Usage) == 1);
		OutCppForm.CppType = TEXT("uint8");
		return true;
	}

	if (Enum->GetCppForm() == UEnum::ECppForm::EnumClass)
		OutCppForm.CppGenericType = TEXT("uint8");
	else
		OutCppForm.bDisallowNativeNest = true;

	if (!Usage.bIsReference)
	{
		FString HeaderPath = FAngelscriptBindDatabase::GetSourceHeader(Enum, *BindDatabase);
		if (HeaderPath.Len() != 0)
		{
			OutCppForm.CppType = Enum->GetName();
			if (Enum->GetCppForm() == UEnum::ECppForm::Namespaced)
				OutCppForm.CppType += TEXT("::Type");

			if (!HeaderPath.Contains(TEXT("NoExportTypes.h")))
				OutCppForm.CppHeader = FString::Printf(TEXT("#include \"%s\""), *HeaderPath);
		}
	}

	return true;
}

bool FEnumType::GetStringIdentifier(const FAngelscriptTypeUsage& Usage, void* Address, FString& OutString) const
{
	auto* UsedEnum = Enum != nullptr ? Enum : (UEnum*)Usage.ScriptClass->GetUserData();
	if (UsedEnum == nullptr)
		return false;

	int32 EnumValue = 0;
	EnumValue = Usage.ResolvePrimitive<uint8>(Address);

	FString EnumName = UsedEnum->GetNameByValue(EnumValue).ToString();
	OutString = EnumName;

	return !EnumName.IsEmpty();
}

bool FEnumType::FromStringIdentifier(const FAngelscriptTypeUsage& Usage, const FString& InString, void* BufferPtr) const
{
	auto* UsedEnum = Enum != nullptr ? Enum : (UEnum*)Usage.ScriptClass->GetUserData();
	if (UsedEnum == nullptr)
		return false;

	int EnumValue = UsedEnum->GetValueByName(*InString);
	if (EnumValue != -1)
	{
		*(uint8*)BufferPtr = EnumValue;
		return true;
	}

	LexFromString(EnumValue, *InString);
	*(uint8*)BufferPtr = EnumValue;
	return true;
}
