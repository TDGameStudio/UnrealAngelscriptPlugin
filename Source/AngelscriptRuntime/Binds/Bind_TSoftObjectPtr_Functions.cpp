#include "Bind_TSoftObjectPtr_Functions.h"

#include "AngelscriptEngine.h"
#include "Components/ActorComponent.h"
#include "GameFramework/Actor.h"
#include "Misc/PackageName.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_scriptengine.h"
#include "EndAngelscriptHeaders.h"

namespace
{
	UClass* GetSoftPtrSubType()
	{
		asITypeInfo* TemplateType = FAngelscriptEngine::GetCurrentFunctionObjectType();
		asITypeInfo* SubType = TemplateType->GetSubType(0);
		return static_cast<UClass*>(SubType->GetUserData());
	}
}

bool FAngelscriptTSoftObjectPtrBinds::ValidateTemplate(asITypeInfo* TemplateType, asCString* ErrorMessage)
{
	if (TemplateType->GetSubTypeCount() != 1)
		return false;

	asITypeInfo* SubType = TemplateType->GetSubType(0);
	if (SubType == nullptr || (SubType->GetFlags() & asOBJ_VALUE) != 0)
	{
		if (ErrorMessage != nullptr)
			*ErrorMessage = "Subtype must be a class type";
		return false;
	}

	return true;
}

void FAngelscriptTSoftObjectPtrBinds::ConstructDefault(FSoftObjectPtr* Ptr)
{
	new (Ptr) FSoftObjectPtr();
}

void FAngelscriptTSoftObjectPtrBinds::ConstructFromPath(FSoftObjectPtr* Ptr, FSoftObjectPath& Path)
{
	new (Ptr) FSoftObjectPtr(Path);
}

void FAngelscriptTSoftObjectPtrBinds::Destruct(FSoftObjectPtr* Self)
{
	Self->~FSoftObjectPtr();
}

FSoftObjectPath FAngelscriptTSoftObjectPtrBinds::ToSoftObjectPath(FSoftObjectPtr* Self)
{
	return Self->ToSoftObjectPath();
}

FString FAngelscriptTSoftObjectPtrBinds::ToString(FSoftObjectPtr* Self)
{
	return Self->ToSoftObjectPath().ToString();
}

FString FAngelscriptTSoftObjectPtrBinds::GetLongPackageName(FSoftObjectPtr* Self)
{
	return Self->GetLongPackageName();
}

FString FAngelscriptTSoftObjectPtrBinds::GetAssetName(FSoftObjectPtr* Self)
{
	return Self->GetAssetName();
}

bool FAngelscriptTSoftObjectPtrBinds::IsValid(FSoftObjectPtr* Self)
{
	return Self->IsValid();
}

bool FAngelscriptTSoftObjectPtrBinds::IsPending(FSoftObjectPtr* Self)
{
	return Self->IsPending();
}

bool FAngelscriptTSoftObjectPtrBinds::IsNull(FSoftObjectPtr* Self)
{
	return Self->IsNull();
}

void FAngelscriptTSoftObjectPtrBinds::Reset(FSoftObjectPtr* Self)
{
	Self->Reset();
}

FSoftObjectPtr& FAngelscriptTSoftObjectPtrBinds::AssignPath(FSoftObjectPtr* Self, FSoftObjectPath& Path)
{
	*Self = Path;
	return *Self;
}

void FAngelscriptTSoftObjectPtrBinds::ConstructFromObject(FSoftObjectPtr* Ptr, UObject* Object)
{
	new (Ptr) FSoftObjectPtr(Object);
}

void FAngelscriptTSoftObjectPtrBinds::CopyConstruct(FSoftObjectPtr* Ptr, FSoftObjectPtr& Other)
{
	new (Ptr) FSoftObjectPtr(Other);
}

FSoftObjectPtr& FAngelscriptTSoftObjectPtrBinds::AssignObject(FSoftObjectPtr* Self, UObject* Object)
{
	*Self = Object;
	return *Self;
}

FSoftObjectPtr& FAngelscriptTSoftObjectPtrBinds::AssignOther(FSoftObjectPtr* Self, FSoftObjectPtr& Other)
{
	*Self = Other;
	return *Self;
}

bool FAngelscriptTSoftObjectPtrBinds::EqualsOther(FSoftObjectPtr* Self, const FSoftObjectPtr& Other)
{
	return *Self == Other;
}

bool FAngelscriptTSoftObjectPtrBinds::EqualsObject(FSoftObjectPtr* Self, UObject* Object)
{
	return Self->Get() == Object;
}

UObject* FAngelscriptTSoftObjectPtrBinds::GetObject(FSoftObjectPtr* Self)
{
	UObject* Object = Self->Get();
	if (Object != nullptr && !Object->IsA(GetSoftPtrSubType()))
		return nullptr;
	return Object;
}

void FAngelscriptTSoftObjectPtrBinds::LoadObjectAsync(FSoftObjectPtr* Self, FOnSoftObjectLoaded OnLoaded)
{
	UClass* ObjectClass = GetSoftPtrSubType();
	if (ObjectClass != nullptr)
	{
		// We don't allow loading references to actors or components; levels are
		// supposed to be streamed in using the streaming system instead.
		if (ObjectClass->IsChildOf(AActor::StaticClass()))
		{
			FAngelscriptEngine::Throw("Actor soft references cannot be loaded, stream the level in instead.");
			return;
		}
		if (ObjectClass->IsChildOf(UActorComponent::StaticClass()))
		{
			FAngelscriptEngine::Throw("Component soft references cannot be loaded, stream the level in instead.");
			return;
		}
	}

	if (UObject* Object = Self->Get())
	{
		if (!Object->IsA(ObjectClass))
			Object = nullptr;
		OnLoaded.ExecuteIfBound(Object);
		return;
	}

	TWeakObjectPtr<UClass> WeakClass = ObjectClass;
	FSoftObjectPtr ObjectCopy(*Self);
	const FString PackageName = FPackageName::ObjectPathToPackageName(ObjectCopy.ToString());
	if (PackageName.IsEmpty() || (FindPackage(nullptr, *PackageName) == nullptr && !FPackageName::DoesPackageExist(PackageName)))
	{
		OnLoaded.ExecuteIfBound(nullptr);
		return;
	}

	FLoadPackageAsyncDelegate Delegate;
	Delegate.BindLambda([ObjectCopy, OnLoaded, WeakClass](const FName& Package, UPackage* LoadedPackage, EAsyncLoadingResult::Type Result)
	{
		UObject* Object = ObjectCopy.Get();
		if (Object != nullptr && (!WeakClass.IsValid() || !Object->IsA(WeakClass.Get())))
			Object = nullptr;
		OnLoaded.ExecuteIfBound(Object);
	});

	LoadPackageAsync(*PackageName, Delegate, 100);
}

UObject* FAngelscriptTSoftObjectPtrBinds::EditorOnlyLoadSynchronous(FSoftObjectPtr* Self)
{
	UClass* ObjectClass = GetSoftPtrSubType();
	if (ObjectClass != nullptr)
	{
		// We don't allow loading references to actors or components; levels are
		// supposed to be streamed in using the streaming system instead.
		if (ObjectClass->IsChildOf(AActor::StaticClass()))
		{
			FAngelscriptEngine::Throw("Actor soft references cannot be loaded, stream the level in instead.");
			return nullptr;
		}
		if (ObjectClass->IsChildOf(UActorComponent::StaticClass()))
		{
			FAngelscriptEngine::Throw("Component soft references cannot be loaded, stream the level in instead.");
			return nullptr;
		}
	}

	return Self->LoadSynchronous();
}

void FAngelscriptTSoftObjectPtrBinds::ConstructFromClass(FSoftObjectPtr* Ptr, UClass* Object)
{
	new (Ptr) FSoftObjectPtr(Object);
}

void FAngelscriptTSoftObjectPtrBinds::ConstructFromSubclass(FSoftObjectPtr* Ptr, TSubclassOf<UObject>& Other)
{
	new (Ptr) FSoftObjectPtr(Other.Get());
}

FSoftObjectPtr& FAngelscriptTSoftObjectPtrBinds::AssignClass(FSoftObjectPtr* Self, UClass* NewClass)
{
	if (NewClass != nullptr && !NewClass->IsChildOf(GetSoftPtrSubType()))
	{
		FAngelscriptEngine::Throw("Provided class is does not inherit from TSoftClassPtr subtype.");
		return *Self;
	}

	*Self = NewClass;
	return *Self;
}

FSoftObjectPtr& FAngelscriptTSoftObjectPtrBinds::AssignSubclass(FSoftObjectPtr* Self, TSubclassOf<UObject>& Other)
{
	*Self = Other.Get();
	return *Self;
}

bool FAngelscriptTSoftObjectPtrBinds::EqualsSubclass(FSoftObjectPtr* Self, const TSubclassOf<UObject>& Other)
{
	return Self->Get() == Other.Get();
}

bool FAngelscriptTSoftObjectPtrBinds::EqualsClass(FSoftObjectPtr* Self, UClass* Object)
{
	return Self->Get() == Object;
}

TSubclassOf<UObject> FAngelscriptTSoftObjectPtrBinds::GetClass(FSoftObjectPtr* Self)
{
	UClass* Object = Cast<UClass>(Self->Get());
	if (Object != nullptr && !Object->IsChildOf(GetSoftPtrSubType()))
		return TSubclassOf<UObject>();
	return TSubclassOf<UObject>(Object);
}

void FAngelscriptTSoftObjectPtrBinds::LoadClassAsync(FSoftObjectPtr* Self, FOnSoftClassLoaded OnLoaded)
{
	UClass* ObjectClass = GetSoftPtrSubType();

	if (UClass* Object = Cast<UClass>(Self->Get()))
	{
		if (!Object->IsChildOf(ObjectClass))
			Object = nullptr;
		OnLoaded.ExecuteIfBound(Object);
		return;
	}

	TWeakObjectPtr<UClass> WeakClass = ObjectClass;
	FSoftObjectPtr ObjectCopy(*Self);
	const FString PackageName = FPackageName::ObjectPathToPackageName(ObjectCopy.ToString());
	if (PackageName.IsEmpty() || (FindPackage(nullptr, *PackageName) == nullptr && !FPackageName::DoesPackageExist(PackageName)))
	{
		OnLoaded.ExecuteIfBound(nullptr);
		return;
	}

	FLoadPackageAsyncDelegate Delegate;
	Delegate.BindLambda([ObjectCopy, OnLoaded, WeakClass](const FName& Package, UPackage* LoadedPackage, EAsyncLoadingResult::Type Result)
	{
		UClass* Object = Cast<UClass>(ObjectCopy.Get());
		if (Object != nullptr && (!WeakClass.IsValid() || !Object->IsChildOf(WeakClass.Get())))
			Object = nullptr;
		OnLoaded.ExecuteIfBound(Object);
	});

	LoadPackageAsync(*PackageName, Delegate, 100);
}
