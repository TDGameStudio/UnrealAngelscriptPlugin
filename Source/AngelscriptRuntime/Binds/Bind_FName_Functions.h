#pragma once

#include "CoreMinimal.h"

struct FAngelscriptFNameBinds
{
	static void ConstructDefault(FName* Address);
	static void ConstructCopy(FName* Address, const FName& Other);
	static void ConstructFromString(FName* Address, const FString& Other);
	static bool IsEqual(const FName& Self, const FName& Other, bool bIgnoreCase, bool bCompareNumber);
	static uint32 GetHash(const FName& Name);
	static void AppendToString(void* Address, FString& OutString);
	static FString PrefixName(FString& String, const FName& Value);
	static FString& PrefixNameAssign(FString& String, const FName& Value);
	static bool EqualsString(const FName& Name, const FString& String);
};
