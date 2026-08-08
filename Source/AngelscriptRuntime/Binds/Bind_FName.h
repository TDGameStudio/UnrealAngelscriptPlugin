#pragma once

#include "CoreMinimal.h"
#include "Helper_PODType.h"

struct FNameType : TAngelscriptPODPropertyType<FNameProperty>
{
	FString GetAngelscriptTypeName() const override;
	bool CanConstruct(const FAngelscriptTypeUsage& Usage) const override;
	bool NeedConstruct(const FAngelscriptTypeUsage& Usage) const override;
	void ConstructValue(const FAngelscriptTypeUsage& Usage, void* Address) const override;
	bool DefaultValue_UnrealToAngelscript(const FAngelscriptTypeUsage& Usage, const FString& InValue, FString& OutValue) const override;
	bool DefaultValue_AngelscriptToUnreal(const FAngelscriptTypeUsage& Usage, const FString& InValue, FString& OutValue) const override;
	bool GetDebuggerValue(const FAngelscriptTypeUsage& Usage, void* Address, FDebuggerValue& Value) const override;
	bool GetStringIdentifier(const FAngelscriptTypeUsage& Usage, void* Address, FString& OutString) const override;
	bool FromStringIdentifier(const FAngelscriptTypeUsage& Usage, const FString& InString, void* BufferPtr) const;
	bool GetCppForm(const FAngelscriptTypeUsage& Usage, FCppForm& OutCppForm) const override;
	bool IsOrdered(const FAngelscriptTypeUsage& Usage) const override;
	int32 CompareOrder(const FAngelscriptTypeUsage& Usage, void* Value, void* OtherValue) const override;
};

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
