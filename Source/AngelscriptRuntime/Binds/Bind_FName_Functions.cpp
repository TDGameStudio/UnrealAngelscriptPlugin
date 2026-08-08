#include "Bind_FName_Functions.h"

void FAngelscriptFNameBinds::ConstructDefault(FName* Address)
{
	new (Address) FName();
}

void FAngelscriptFNameBinds::ConstructCopy(FName* Address, const FName& Other)
{
	new (Address) FName(Other);
}

void FAngelscriptFNameBinds::ConstructFromString(FName* Address, const FString& Other)
{
	new (Address) FName(*Other);
}

bool FAngelscriptFNameBinds::IsEqual(
	const FName& Self,
	const FName& Other,
	const bool bIgnoreCase,
	const bool bCompareNumber)
{
	return Self.IsEqual(
		Other,
		bIgnoreCase ? ENameCase::IgnoreCase : ENameCase::CaseSensitive,
		bCompareNumber);
}

uint32 FAngelscriptFNameBinds::GetHash(const FName& Name)
{
	return GetTypeHash(Name);
}

void FAngelscriptFNameBinds::AppendToString(void* Address, FString& OutString)
{
	OutString += static_cast<FName*>(Address)->ToString();
}

FString FAngelscriptFNameBinds::PrefixName(FString& String, const FName& Value)
{
	return Value.ToString() + String;
}

FString& FAngelscriptFNameBinds::PrefixNameAssign(FString& String, const FName& Value)
{
	String = Value.ToString() + String;
	return String;
}

bool FAngelscriptFNameBinds::EqualsString(const FName& Name, const FString& String)
{
	return Name == *String;
}
