#pragma once

#include "CoreMinimal.h"

struct FAngelscriptUObjectBinds
{
	static bool IsTransient(UObject* Object);
	static void SetTransactional(UObject* Object, bool bTransactional);
	static UObject* GetTypedOuter(const UObject* Object, const TSubclassOf<UObject>& Target);
	static FString GetFullName(const UObject* Object, const UObject* StopOuter);
	static bool IsA(UObject* Object, UClass* Class);
	static bool ImplementsInterface(UObject* Object, UClass* InterfaceClass);
	static void SaveConfig(UObject* Object);
	static void LoadConfig(UObject* Object);
	static void ReloadConfig(UObject* Object);
	static void CopyScriptPropertiesFrom(UObject* Object, const UObject* OtherObject);
	static void CastToType(UObject* Object, void* OutAddress, int TypeId);
	static void AppendToString(void* Address, FString& OutString);

	static UObject* GetDefaultObject(UClass* Class);
	static FString GetClassSourceFilePath(UClass* Class);
	static FString GetScriptModuleName(UClass* Class);
	static FString GetScriptTypeDeclaration(UClass* Class);
	static bool IsFunctionImplementedInScript(UClass* Class, FName FunctionName);
	static UFunction* FindFunctionByName(UClass* Class, FName FunctionName);
	static bool IsAbstract(UClass* Class);
	static UClass* GetSuperClass(UClass* Class);
	static UClass* FindClassByObjectName(const FString& Name);
	static void GetAllClasses(TArray<UClass*>& OutClasses);
	static TArray<UClass*> GetAllSubclassesOf(UClass* ParentClass, bool bIncludeAbstractClasses);
	static UClass* FindClassByScriptName(const FString& Name);

	static FString GetFunctionSourceFilePath(UFunction* Function);
	static int32 GetFunctionSourceLineNumber(UFunction* Function);
	static FString GetScriptFunctionDeclaration(UFunction* Function);

	static UPackage* GetAngelscriptPackage();
	static UObject* CreateObject(UObject* Outer, const TSubclassOf<UObject>& Class, FName Name, bool bTransient);
	static UObject* LoadObjectByName(UObject* Outer, const FString& Name);
	static UObject* FindObjectByName(const FString& Name);
	static UObject* FindObjectWithinOuter(UObject* Outer, const FString& Name);
	static UObject* CreateLiteralAsset(UClass* AssetClass, const FString& AssetName);
	static void PostLiteralAssetSetup(UObject* Asset, const FString& Name);
};
