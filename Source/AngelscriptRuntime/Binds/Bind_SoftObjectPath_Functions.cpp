#include "Bind_SoftObjectPath.h"

void FAngelscriptSoftObjectPathBinds::ConstructObjectPathFromString(void* Memory, const FString& Path)
{
	new (Memory) FSoftObjectPath(Path);
}

void FAngelscriptSoftObjectPathBinds::ConstructObjectPathFromObject(void* Memory, const UObject* Object)
{
	new (Memory) FSoftObjectPath(Object);
}

UObject* FAngelscriptSoftObjectPathBinds::TryLoadObject(FSoftObjectPath* Path)
{
	return Path->TryLoad();
}

void FAngelscriptSoftObjectPathBinds::AppendObjectPathToString(void* Address, FString& String)
{
	String += static_cast<FSoftObjectPath*>(Address)->ToString();
}

void FAngelscriptSoftObjectPathBinds::ConstructClassPathFromString(void* Memory, const FString& Path)
{
	new (Memory) FSoftClassPath(Path);
}

void FAngelscriptSoftObjectPathBinds::ConstructClassPathFromClass(void* Memory, const UClass* Class)
{
	new (Memory) FSoftClassPath(Class);
}

UClass* FAngelscriptSoftObjectPathBinds::ResolveClass(const FSoftClassPath& Path)
{
	return Path.ResolveClass();
}

UClass* FAngelscriptSoftObjectPathBinds::TryLoadClass(const FSoftClassPath& Path)
{
	return Path.TryLoadClass<UObject>();
}

void FAngelscriptSoftObjectPathBinds::AppendClassPathToString(void* Address, FString& String)
{
	String += static_cast<FSoftClassPath*>(Address)->ToString();
}
