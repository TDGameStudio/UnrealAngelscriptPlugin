#pragma once

#include "CoreMinimal.h"
#include "AngelscriptType.h"
#include "UObject/Class.h"

class FAngelscriptBindDatabase;
class asITypeInfo;

struct FAngelscriptUStructBinds
{
	static void NoopConstruct(void* Destination);
	static void NoopDestruct(void* Destination);
	static void ZeroConstruct(void* Destination);
	static void Construct(void* Destination);
	static void Destruct(void* Destination);
	static void PodCopyConstruct(void* Destination, void* Source);
	static void* PodAssign(void* Destination, void* Source);
	static void CopyConstructWithoutInitialization(void* Destination, void* Source);
	static void CopyConstructWithZeroInitialization(void* Destination, void* Source);
	static void CopyConstructWithInitialization(void* Destination, void* Source);
	static void* CopyAssign(void* Destination, void* Source);
	static void GenericConstruct(void* Destination);
	static void GenericDestruct(void* Destination);
	static void GenericCopyConstruct(void* Destination, void* Source);
	static void* GenericAssign(void* Destination, void* Source);
};

struct FUStructType : FAngelscriptType
{
	UScriptStruct* Struct = nullptr;
	asITypeInfo* ScriptTypeInfo = nullptr;
	FString StructName;
	const FAngelscriptBindDatabase* BindDatabase = nullptr;

	FUStructType(
		UScriptStruct* InStruct,
		const FString& InStructName,
		const FAngelscriptBindDatabase& InBindDatabase);

	virtual bool IsUnrealStruct() const override;

	bool IsValidType(const FAngelscriptTypeUsage& Usage) const;

	UStruct* GetUnrealStruct(const FAngelscriptTypeUsage& Usage) const override;

	bool IsTypeEquivalent(const FAngelscriptTypeUsage& Usage, const FAngelscriptTypeUsage& Other) const override;

	UScriptStruct* GetStruct(const FAngelscriptTypeUsage& Usage) const;

	asITypeInfo* GetScriptType(const FAngelscriptTypeUsage& Usage) const;

	UScriptStruct::ICppStructOps* GetOps(const FAngelscriptTypeUsage& Usage) const;

	virtual FString GetAngelscriptTypeName() const override;

	FString GetAngelscriptTypeName(const FAngelscriptTypeUsage& Usage) const override;

	virtual void* GetData() const override;

	bool HasReferences(const FAngelscriptTypeUsage& Usage) const override;

	void EmitReferenceInfo(const FAngelscriptTypeUsage& Usage, FGCReferenceParams& Params) const override;

	bool CanCreateProperty(const FAngelscriptTypeUsage& Usage) const override;

	FProperty* CreateProperty(const FAngelscriptTypeUsage& Usage, const FPropertyParams& Params) const override;

	bool CanQueryPropertyType() const override;

	bool MatchesProperty(const FAngelscriptTypeUsage& Usage, const FProperty* Property, EPropertyMatchType MatchType) const override;

	bool CanCopy(const FAngelscriptTypeUsage& Usage) const override;

	bool NeedCopy(const FAngelscriptTypeUsage& Usage) const override;

	bool CanHashValue(const FAngelscriptTypeUsage& Usage) const override;

	uint32 GetHash(const FAngelscriptTypeUsage& Usage, const void* Address) const;

	void CopyValue(const FAngelscriptTypeUsage& Usage, void* SourcePtr, void* DestinationPtr) const override;

	bool CanConstruct(const FAngelscriptTypeUsage& Usage) const override;

	bool NeedConstruct(const FAngelscriptTypeUsage& Usage) const override;

	void ConstructValue(const FAngelscriptTypeUsage& Usage, void* DestinationPtr) const override;

	bool CanDestruct(const FAngelscriptTypeUsage& Usage) const override;

	bool NeedDestruct(const FAngelscriptTypeUsage& Usage) const override;

	void DestructValue(const FAngelscriptTypeUsage& Usage, void* DestinationPtr) const override;

	int32 GetValueSize(const FAngelscriptTypeUsage& Usage) const override;

	bool CanCompare(const FAngelscriptTypeUsage& Usage) const override;

	bool IsValueEqual(const FAngelscriptTypeUsage& Usage, void* SourcePtr, void* DestinationPtr) const override;

	bool CanBeArgument(const FAngelscriptTypeUsage& Usage) const override;
	void SetArgument(const FAngelscriptTypeUsage& Usage, int32 ArgumentIndex, class asIScriptContext* Context, struct FFrame& Stack, const FArgData& Data) const override;

	bool CanBeReturned(const FAngelscriptTypeUsage& Usage) const override;

	void GetReturnValue(const FAngelscriptTypeUsage& Usage, class asIScriptContext* Context, void* Destination) const override;

	int32 GetValueAlignment(const FAngelscriptTypeUsage& Usage) const;

	bool GetDebuggerValue(const FAngelscriptTypeUsage& Usage, void* Address, struct FDebuggerValue& Value) const override;

	bool GetDebuggerScope(const FAngelscriptTypeUsage& Usage, void* Address, struct FDebuggerScope& Scope) const override;

	bool GetDebuggerMember(const FAngelscriptTypeUsage& Usage, void* Address, const FString& Member, struct FDebuggerValue& Value) const override;

	bool GetCppForm(const FAngelscriptTypeUsage& Usage, FCppForm& OutCppForm) const override;
};
