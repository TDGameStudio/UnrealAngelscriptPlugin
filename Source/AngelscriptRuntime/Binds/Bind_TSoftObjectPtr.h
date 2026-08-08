#pragma once

#include "CoreMinimal.h"
#include "AngelscriptType.h"
#include "Binds/Helper_StructType.h"
#include "FunctionLibraries/SoftReferenceStatics.h"
#include "Templates/SubclassOf.h"
#include "UObject/SoftObjectPtr.h"

class asCString;
class asITypeInfo;
class FAngelscriptBindDatabase;

struct FBaseSoftReferenceType : public TAngelscriptCppType<FSoftObjectPtr>
{
	explicit FBaseSoftReferenceType(const FAngelscriptBindDatabase& InBindDatabase);

	UClass* GetSubTypeClass(const FAngelscriptTypeUsage& Usage) const;
	virtual UClass* GetClassOfObject(const FAngelscriptTypeUsage& Usage) const;
	bool DescribesCompleteType(const FAngelscriptTypeUsage& Usage) const override;
	bool CanCreateProperty(const FAngelscriptTypeUsage& Usage) const override;
	bool CanBeArgument(const FAngelscriptTypeUsage& Usage) const override;
	void SetArgument(const FAngelscriptTypeUsage& Usage, int32 ArgumentIndex, asIScriptContext* Context, FFrame& Stack, const FAngelscriptType::FArgData& Data) const override;
	bool CanBeReturned(const FAngelscriptTypeUsage& Usage) const override;
	void GetReturnValue(const FAngelscriptTypeUsage& Usage, asIScriptContext* Context, void* Destination) const override;
	bool GetDebuggerValue(const FAngelscriptTypeUsage& Usage, void* Address, FDebuggerValue& Value) const override;
	bool GetDebuggerScope(const FAngelscriptTypeUsage& Usage, void* Address, FDebuggerScope& Scope) const override;
	bool GetDebuggerMember(const FAngelscriptTypeUsage& Usage, void* Address, const FString& Member, FDebuggerValue& Value) const override;

	const FAngelscriptBindDatabase* BindDatabase;
};

struct FSoftObjectPtrType : public FBaseSoftReferenceType
{
	explicit FSoftObjectPtrType(const FAngelscriptBindDatabase& InBindDatabase);

	FString GetAngelscriptTypeName() const override;
	virtual UClass* GetClassOfObject(const FAngelscriptTypeUsage& Usage) const;
	FProperty* CreateProperty(const FAngelscriptTypeUsage& Usage, const FAngelscriptType::FPropertyParams& Params) const override;
	bool MatchesProperty(const FAngelscriptTypeUsage& Usage, const FProperty* Property, EPropertyMatchType MatchType) const override;
	bool CanQueryPropertyType() const override;
	bool GetCppForm(const FAngelscriptTypeUsage& Usage, FCppForm& OutCppForm) const override;
};

struct FSoftClassPtrType : public FBaseSoftReferenceType
{
	explicit FSoftClassPtrType(const FAngelscriptBindDatabase& InBindDatabase);

	FString GetAngelscriptTypeName() const override;
	virtual UClass* GetClassOfObject(const FAngelscriptTypeUsage& Usage) const;
	FProperty* CreateProperty(const FAngelscriptTypeUsage& Usage, const FAngelscriptType::FPropertyParams& Params) const override;
	bool MatchesProperty(const FAngelscriptTypeUsage& Usage, const FProperty* Property, EPropertyMatchType MatchType) const override;
	bool CanQueryPropertyType() const override;
	bool GetCppForm(const FAngelscriptTypeUsage& Usage, FCppForm& OutCppForm) const override;
};

struct FAngelscriptTSoftObjectPtrBinds
{
	static bool ValidateTemplate(asITypeInfo* TemplateType, asCString* ErrorMessage);

	static void ConstructDefault(FSoftObjectPtr* Ptr);
	static void ConstructFromPath(FSoftObjectPtr* Ptr, FSoftObjectPath& Path);
	static void Destruct(FSoftObjectPtr* Self);
	static FSoftObjectPath ToSoftObjectPath(FSoftObjectPtr* Self);
	static FString ToString(FSoftObjectPtr* Self);
	static FString GetLongPackageName(FSoftObjectPtr* Self);
	static FString GetAssetName(FSoftObjectPtr* Self);
	static bool IsValid(FSoftObjectPtr* Self);
	static bool IsPending(FSoftObjectPtr* Self);
	static bool IsNull(FSoftObjectPtr* Self);
	static void Reset(FSoftObjectPtr* Self);
	static FSoftObjectPtr& AssignPath(FSoftObjectPtr* Self, FSoftObjectPath& Path);

	static void ConstructFromObject(FSoftObjectPtr* Ptr, UObject* Object);
	static void CopyConstruct(FSoftObjectPtr* Ptr, FSoftObjectPtr& Other);
	static FSoftObjectPtr& AssignObject(FSoftObjectPtr* Self, UObject* Object);
	static FSoftObjectPtr& AssignOther(FSoftObjectPtr* Self, FSoftObjectPtr& Other);
	static bool EqualsOther(FSoftObjectPtr* Self, const FSoftObjectPtr& Other);
	static bool EqualsObject(FSoftObjectPtr* Self, UObject* Object);
	static UObject* GetObject(FSoftObjectPtr* Self);
	static void LoadObjectAsync(FSoftObjectPtr* Self, FOnSoftObjectLoaded OnLoaded);
	static UObject* EditorOnlyLoadSynchronous(FSoftObjectPtr* Self);

	static void ConstructFromClass(FSoftObjectPtr* Ptr, UClass* Object);
	static void ConstructFromSubclass(FSoftObjectPtr* Ptr, TSubclassOf<UObject>& Other);
	static FSoftObjectPtr& AssignClass(FSoftObjectPtr* Self, UClass* NewClass);
	static FSoftObjectPtr& AssignSubclass(FSoftObjectPtr* Self, TSubclassOf<UObject>& Other);
	static bool EqualsSubclass(FSoftObjectPtr* Self, const TSubclassOf<UObject>& Other);
	static bool EqualsClass(FSoftObjectPtr* Self, UClass* Object);
	static TSubclassOf<UObject> GetClass(FSoftObjectPtr* Self);
	static void LoadClassAsync(FSoftObjectPtr* Self, FOnSoftClassLoaded OnLoaded);
};
