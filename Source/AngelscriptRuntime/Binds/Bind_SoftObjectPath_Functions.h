#pragma once

#include "CoreMinimal.h"
#include "UObject/SoftObjectPath.h"

struct FAngelscriptSoftObjectPathBinds
{
	static void ConstructObjectPathFromString(void* Memory, const FString& Path);
	static void ConstructObjectPathFromObject(void* Memory, const UObject* Object);
	static UObject* TryLoadObject(FSoftObjectPath* Path);
	static void AppendObjectPathToString(void* Address, FString& String);

	static void ConstructClassPathFromString(void* Memory, const FString& Path);
	static void ConstructClassPathFromClass(void* Memory, const UClass* Class);
	static UClass* ResolveClass(const FSoftClassPath& Path);
	static UClass* TryLoadClass(const FSoftClassPath& Path);
	static void AppendClassPathToString(void* Address, FString& String);
};
