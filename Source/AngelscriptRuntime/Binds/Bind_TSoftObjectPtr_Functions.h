#pragma once

#include "CoreMinimal.h"
#include "FunctionLibraries/SoftReferenceStatics.h"
#include "Templates/SubclassOf.h"
#include "UObject/SoftObjectPtr.h"

class asCString;
class asITypeInfo;

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
