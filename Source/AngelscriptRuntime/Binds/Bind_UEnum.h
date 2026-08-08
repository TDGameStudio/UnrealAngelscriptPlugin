#pragma once

#include "CoreMinimal.h"
#include "AngelscriptType.h"

class FAngelscriptBindDatabase;

struct FEnumType : FAngelscriptType
{
	UEnum* Enum;
	const FAngelscriptBindDatabase* BindDatabase;

	FEnumType(UEnum* InEnum, const FAngelscriptBindDatabase& InBindDatabase);

	bool IsPrimitive() const override;

	FString GetAngelscriptTypeName() const override;

	FString GetAngelscriptTypeName(const FAngelscriptTypeUsage& Usage) const override;

	void* GetData() const override;

	bool CanCreateProperty(const FAngelscriptTypeUsage& Usage) const override;

	bool CanQueryPropertyType() const override;

	bool MatchesProperty(const FAngelscriptTypeUsage& Usage, const FProperty* Property, EPropertyMatchType MatchType) const override;

	FProperty* CreateProperty(const FAngelscriptTypeUsage& Usage, const FPropertyParams& Params) const override;

	bool IsTypeEquivalent(const FAngelscriptTypeUsage& Usage, const FAngelscriptTypeUsage& Other) const override;

	bool CanCopy(const FAngelscriptTypeUsage& Usage) const override;
	bool NeedCopy(const FAngelscriptTypeUsage& Usage) const override;
	void CopyValue(const FAngelscriptTypeUsage& Usage, void* SourcePtr, void* DestinationPtr) const override;

	bool CanCompare(const FAngelscriptTypeUsage& Usage) const override;
	bool IsValueEqual(const FAngelscriptTypeUsage& Usage, void* SourcePtr, void* DestinationPtr) const override;

	bool CanConstruct(const FAngelscriptTypeUsage& Usage) const override;
	bool NeedConstruct(const FAngelscriptTypeUsage& Usage) const override;
	void ConstructValue(const FAngelscriptTypeUsage& Usage, void* DestinationPtr) const override;

	bool CanDestruct(const FAngelscriptTypeUsage& Usage) const override;
	bool NeedDestruct(const FAngelscriptTypeUsage& Usage) const override;
	void DestructValue(const FAngelscriptTypeUsage& Usage, void* DestinationPtr) const override;

	int32 GetValueSize(const FAngelscriptTypeUsage& Usage) const override;

	bool CanBeArgument(const FAngelscriptTypeUsage& Usage) const override;
	void SetArgument(const FAngelscriptTypeUsage& Usage, int32 ArgumentIndex, class asIScriptContext* Context, struct FFrame& Stack, const FArgData& Data) const override;

	bool CanBeReturned(const FAngelscriptTypeUsage& Usage) const override;

	void GetReturnValue(const FAngelscriptTypeUsage& Usage, class asIScriptContext* Context, void* Destination) const override;

	bool DefaultValue_UnrealToAngelscript(const FAngelscriptTypeUsage& Usage, const FString& InValue, FString& OutValue) const override;

	bool DefaultValue_AngelscriptToUnreal(const FAngelscriptTypeUsage& Usage, const FString& InValue, FString& OutValue) const override;

	int32 GetValueAlignment(const FAngelscriptTypeUsage& Usage) const;

	bool CanHashValue(const FAngelscriptTypeUsage& Usage) const;

	uint32 GetHash(const FAngelscriptTypeUsage& Usage, const void* Address) const;

	FASDebugValue* CreateDebugValue(const FAngelscriptTypeUsage& Usage, FDebugValuePrototype& Values, int32 Offset) const override;

	bool GetDebuggerValue(const FAngelscriptTypeUsage& Usage, void* Address, struct FDebuggerValue& Value) const override;

	bool GetCppForm(const FAngelscriptTypeUsage& Usage, FCppForm& OutCppForm) const override;

	bool GetStringIdentifier(const FAngelscriptTypeUsage& Usage, void* Address, FString& OutString) const override;

	bool FromStringIdentifier(const FAngelscriptTypeUsage& Usage, const FString& InString, void* BufferPtr) const override;
};
