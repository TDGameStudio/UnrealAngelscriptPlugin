#include "Bind_UObject.h"

#include "AngelscriptEngine.h"
#include "AngelscriptType.h"
#include "ClassGenerator/ASClass.h"
#include "Engine/Engine.h"
#include "GameFramework/Actor.h"
#include "Serialization/AsyncLoadingEvents.h"
#if WITH_EDITOR
#include "UObject/MetaData.h"
#endif
#include "UObject/ObjectRedirector.h"
#include "UObject/Package.h"
#include "UObject/UnrealType.h"
#include "UObject/UObjectIterator.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_objecttype.h"
#include "source/as_scriptengine.h"
#include "EndAngelscriptHeaders.h"

bool FAngelscriptUObjectBinds::IsTransient(UObject* Object)
{
	return Object->HasAnyFlags(RF_Transient);
}

void FAngelscriptUObjectBinds::SetTransactional(UObject* Object, bool bTransactional)
{
	if (bTransactional)
		Object->SetFlags(RF_Transactional);
	else
		Object->ClearFlags(RF_Transactional);
}

UObject* FAngelscriptUObjectBinds::GetTypedOuter(const UObject* Object, const TSubclassOf<UObject>& Target)
{
	if (Target == nullptr)
		return nullptr;

	return Object->GetTypedOuter(Target.Get());
}

FString FAngelscriptUObjectBinds::GetFullName(const UObject* Object, const UObject* StopOuter)
{
	return Object->GetFullName(StopOuter);
}

bool FAngelscriptUObjectBinds::IsA(UObject* Object, UClass* Class)
{
	if (Class == nullptr)
	{
		FAngelscriptEngine::Throw("Class passed in to IsA was nullptr.");
		return false;
	}

	return Object->IsA(Class);
}

bool FAngelscriptUObjectBinds::ImplementsInterface(UObject* Object, UClass* InterfaceClass)
{
	if (Object == nullptr || InterfaceClass == nullptr)
		return false;
	return Object->GetClass()->ImplementsInterface(InterfaceClass);
}

void FAngelscriptUObjectBinds::SaveConfig(UObject* Object)
{
	Object->SaveConfig();
}

void FAngelscriptUObjectBinds::LoadConfig(UObject* Object)
{
	Object->LoadConfig();
}

void FAngelscriptUObjectBinds::ReloadConfig(UObject* Object)
{
	Object->ReloadConfig();
}

void FAngelscriptUObjectBinds::CopyScriptPropertiesFrom(UObject* Object, const UObject* OtherObject)
{
	*(asIScriptObject*)Object = *(asIScriptObject*)OtherObject;
}

void FAngelscriptUObjectBinds::CastToType(UObject* Object, void* OutAddress, int TypeId)
{
	auto& Manager = FAngelscriptEngine::Get();
	asITypeInfo* RequestedType = Manager.Engine->GetTypeInfoById(TypeId);
	const bool bLogDamageableCast = RequestedType != nullptr && FCStringAnsi::Strstr(RequestedType->GetName(), "DamageableCast") != nullptr;
	if (bLogDamageableCast)
	{
		UE_LOG(
			Angelscript,
			Display,
			TEXT("UObject::opCast entry typeId=%d isHandle=%s requestedType=%hs"),
			TypeId,
			(TypeId & asTYPEID_OBJHANDLE) != 0 ? TEXT("true") : TEXT("false"),
			RequestedType->GetName());
	}

	// Can't cast if it's not a handle, need to store somewhere
	if (!(TypeId & asTYPEID_OBJHANDLE))
	{
		if (bLogDamageableCast)
		{
			UE_LOG(Angelscript, Display, TEXT("UObject::opCast rejected non-handle target typeId=%d"), TypeId);
		}
		return;
	}

	asCObjectType* ScriptType = static_cast<asCObjectType*>(Manager.Engine->GetTypeInfoById(TypeId));
	checkSlow(ScriptType != nullptr);

	// Structs cannot be cast to uobjects
	if (ScriptType->GetFlags() & asOBJ_VALUE)
		return;

	UClass* AssociatedClass = static_cast<UClass*>(ScriptType->GetUserData());
	checkSlow(AssociatedClass != nullptr);

	const bool bIsA = Object->IsA(AssociatedClass);
	const bool bAssociatedClassIsInterface = AssociatedClass->HasAnyClassFlags(CLASS_Interface);
	const bool bImplementsInterface = bAssociatedClassIsInterface && Object->GetClass()->ImplementsInterface(AssociatedClass);

	if (bAssociatedClassIsInterface)
	{
		UE_LOG(
			Angelscript,
			Display,
			TEXT("UObject::opCast target=%s objectClass=%s isA=%s implements=%s"),
			*AssociatedClass->GetName(),
			*Object->GetClass()->GetName(),
			bIsA ? TEXT("true") : TEXT("false"),
			bImplementsInterface ? TEXT("true") : TEXT("false"));
	}

	// The cast is valid if the script type we're casting to has a UClass associated with it that is one of the
	// parent classes of our UObject, or if the object implements the target interface.
	if (bIsA || bImplementsInterface)
	{
		*static_cast<UObject**>(OutAddress) = Object;
	}
	else
	{
		*static_cast<UObject**>(OutAddress) = nullptr;
	}
}

void FAngelscriptUObjectBinds::AppendToString(void* Address, FString& OutString)
{
	UObject* Object = static_cast<UObject*>(Address);
	if (Object == nullptr)
	{
		OutString += TEXT("{ nullptr }");
		return;
	}

	UClass* ObjectClass = Object->GetClass();
	const UASClass* ScriptClass = Cast<UASClass>(ObjectClass);
	const bool bUseClassPrefix =
		ObjectClass->HasAnyClassFlags(CLASS_Native)
		|| (ScriptClass != nullptr && ScriptClass->bIsScriptClass);
	const TCHAR* ClassPrefix = bUseClassPrefix ? ObjectClass->GetPrefixCPP() : TEXT("");

	FString Suffix;
	auto& Delegate = FAngelscriptEngine::Get().GetDebugObjectSuffix();
	if (Delegate.IsBound())
	{
		Delegate.Execute(Object, Suffix);
	}

#if WITH_EDITOR
	if (AActor* Actor = Cast<AActor>(Object))
	{
		OutString += FString::Printf(TEXT("{ %s %s(%s%s) (ID: %s) }"),
			*Actor->GetActorLabel(),
			*Suffix,
			ClassPrefix,
			*ObjectClass->GetName(),
			*Object->GetName());
	}
	else
#endif
	{
		OutString += FString::Printf(TEXT("{ %s %s(%s%s) }"),
			*Object->GetName(),
			*Suffix,
			ClassPrefix,
			*ObjectClass->GetName());
	}
}

UObject* FAngelscriptUObjectBinds::GetDefaultObject(UClass* Class)
{
	return Class->GetDefaultObject();
}

FString FAngelscriptUObjectBinds::GetClassSourceFilePath(UClass* Class)
{
	if (const UASClass* ScriptClass = Cast<UASClass>(Class))
	{
		return ScriptClass->GetSourceFilePath();
	}
	return FString();
}

FString FAngelscriptUObjectBinds::GetScriptModuleName(UClass* Class)
{
	const UASClass* ScriptClass = Cast<UASClass>(Class);
	if (ScriptClass == nullptr || ScriptClass->ScriptTypePtr == nullptr)
	{
		return FString();
	}
	auto& Manager = FAngelscriptEngine::Get();
	auto Module = Manager.GetModule(static_cast<asITypeInfo*>(ScriptClass->ScriptTypePtr)->GetModule());
	return Module.IsValid() ? Module->ModuleName : FString();
}

FString FAngelscriptUObjectBinds::GetScriptTypeDeclaration(UClass* Class)
{
	return Cast<UASClass>(Class) != nullptr ? FString::Printf(TEXT("%s%s"), Class->GetPrefixCPP(), *Class->GetName()) : FString();
}

bool FAngelscriptUObjectBinds::IsFunctionImplementedInScript(UClass* Class, FName FunctionName)
{
	if (const UASClass* ScriptClass = Cast<UASClass>(Class))
	{
		return ScriptClass->IsFunctionImplementedInScript(FunctionName);
	}
	return false;
}

UFunction* FAngelscriptUObjectBinds::FindFunctionByName(UClass* Class, FName FunctionName)
{
	return Class != nullptr ? Class->FindFunctionByName(FunctionName) : nullptr;
}

bool FAngelscriptUObjectBinds::IsAbstract(UClass* Class)
{
	return Class->HasAnyClassFlags(CLASS_Abstract);
}

UClass* FAngelscriptUObjectBinds::GetSuperClass(UClass* Class)
{
	return Class->GetSuperClass();
}

UClass* FAngelscriptUObjectBinds::FindClassByObjectName(const FString& Name)
{
	return FindObject<UClass>(nullptr, *Name);
}

void FAngelscriptUObjectBinds::GetAllClasses(TArray<UClass*>& OutClasses)
{
	OutClasses.Reset();
	for (TObjectIterator<UClass> ClassIt; ClassIt; ++ClassIt)
	{
		UClass* Class = *ClassIt;
		if (Class == nullptr)
		{
			continue;
		}
		if (Class->HasAnyClassFlags(CLASS_Deprecated | CLASS_NewerVersionExists))
		{
			continue;
		}
		OutClasses.Add(Class);
	}
}

TArray<UClass*> FAngelscriptUObjectBinds::GetAllSubclassesOf(UClass* ParentClass, bool bIncludeAbstractClasses)
{
	TArray<UClass*> Subclasses;
	if (ParentClass == nullptr)
		return Subclasses;

	for (TObjectIterator<UClass> ClassIt; ClassIt; ++ClassIt)
	{
		UClass* Class = *ClassIt;
		if (!ensure(Class))
			continue;
		if (Class->HasAnyClassFlags(CLASS_Deprecated | CLASS_NewerVersionExists))
			continue;
		if (!bIncludeAbstractClasses && Class->HasAnyClassFlags(CLASS_Abstract))
			continue;
		if (!Class->IsChildOf(ParentClass))
			continue;

		Subclasses.Add(Class);
	}

	return Subclasses;
}

UClass* FAngelscriptUObjectBinds::FindClassByScriptName(const FString& Name)
{
	for (TObjectIterator<UClass> ClassIt; ClassIt; ++ClassIt)
	{
		UClass* Class = *ClassIt;
		if (Class == nullptr)
		{
			continue;
		}
		if (Class->GetName() == Name || FAngelscriptType::GetBoundClassName(Class) == Name)
		{
			return Class;
		}
	}
	return nullptr;
}

FString FAngelscriptUObjectBinds::GetFunctionSourceFilePath(UFunction* Function)
{
	if (const UASFunction* ScriptFunction = Cast<UASFunction>(Function))
	{
		return ScriptFunction->GetSourceFilePath();
	}
	return FString();
}

int32 FAngelscriptUObjectBinds::GetFunctionSourceLineNumber(UFunction* Function)
{
	if (const UASFunction* ScriptFunction = Cast<UASFunction>(Function))
	{
		return ScriptFunction->GetSourceLineNumber();
	}
	return -1;
}

FString FAngelscriptUObjectBinds::GetScriptFunctionDeclaration(UFunction* Function)
{
	if (const UASFunction* ScriptFunction = Cast<UASFunction>(Function))
	{
		if (ScriptFunction->ScriptFunction != nullptr)
		{
			return UTF8_TO_TCHAR(ScriptFunction->ScriptFunction->GetDeclaration(false, false, false));
		}
	}
	return FString();
}

UPackage* FAngelscriptUObjectBinds::GetAngelscriptPackage()
{
	return FAngelscriptEngine::GetPackage();
}

UObject* FAngelscriptUObjectBinds::CreateObject(UObject* Outer, const TSubclassOf<UObject>& Class, FName Name, bool bTransient)
{
	if (Class.Get() == nullptr)
	{
		FAngelscriptEngine::Throw("Class was nullptr.");
		return nullptr;
	}

	if (Outer == nullptr)
	{
		Outer = GetTransientPackage();
		bTransient = true;
	}

	EObjectFlags Flags = RF_NoFlags;
	if (bTransient)
		Flags |= RF_Transient;

	FAngelscriptExcludeScopeFromLoopTimeout TimeoutExclusion;
	return NewObject<UObject>(Outer, Class.Get(), Name, Flags);
}

UObject* FAngelscriptUObjectBinds::LoadObjectByName(UObject* Outer, const FString& Name)
{
	FAngelscriptExcludeScopeFromLoopTimeout TimeoutExclusion;
	return LoadObject<UObject>(Outer, *Name);
}

UObject* FAngelscriptUObjectBinds::FindObjectByName(const FString& Name)
{
	return FindObject<UObject>(nullptr, *Name);
}

UObject* FAngelscriptUObjectBinds::FindObjectWithinOuter(UObject* Outer, const FString& Name)
{
	return FindObject<UObject>(Outer, *Name);
}

UObject* FAngelscriptUObjectBinds::CreateLiteralAsset(UClass* AssetClass, const FString& AssetName)
{
	auto* AssetsPackage = FAngelscriptEngine::Get().AssetsPackage;
	auto* ExistingObject = FindObject<UObject>(AssetsPackage, *AssetName);

#if AS_CAN_HOTRELOAD
	UObject* ReloadedObject = nullptr;
#endif

	if (ExistingObject != nullptr)
	{
#if AS_CAN_HOTRELOAD
		if (ExistingObject->GetClass()->HasAnyClassFlags(CLASS_NewerVersionExists))
		{
			static int32 AssetReplacementCounter = 1;
			FString NewName = FString::Printf(TEXT("REPLACED_ASSET_%s_%d"), *AssetName, AssetReplacementCounter++);
			ExistingObject->Rename(*NewName, GetTransientPackage(), REN_DontCreateRedirectors);

			ReloadedObject = ExistingObject;
			ExistingObject = nullptr;
		}
		else
#endif
		if (!ExistingObject->IsA(AssetClass))
		{
			FAngelscriptEngine::Throw(TCHAR_TO_ANSI(*FString::Printf(TEXT("Script literal asset %s of type %s was already declared before as a different type (%s)."),
				*AssetName, *AssetClass->GetName(),
				*ExistingObject->GetClass()->GetName())));
			return nullptr;
		}
	}

	if (ExistingObject == nullptr)
	{
		// Create a new object for this asset
		ExistingObject = NewObject<UObject>(
			AssetsPackage,
			AssetClass,
			*AssetName,
			RF_Public | RF_Standalone | RF_MarkAsRootSet);

#if WITH_EDITOR
		// Add a redirector from the old location where these assets used to live
		auto* ScriptPackage = FAngelscriptEngine::Get().AngelscriptPackage;

		FString RedirectorName = TEXT("Asset_") + AssetName;
		UObjectRedirector* Redirector = FindObject<UObjectRedirector>(ScriptPackage, *RedirectorName);
		if (Redirector == nullptr)
			Redirector = NewObject<UObjectRedirector>(ScriptPackage, *RedirectorName, RF_Standalone | RF_Public);
		Redirector->DestinationObject = ExistingObject;

		// Add metadata so the script can be located
		FString Filename;
		int LineNumber;

		FAngelscriptEngine::GetAngelscriptExecutionFileAndLine(Filename, LineNumber);

		AssetsPackage->GetMetaData().SetValue(ExistingObject, TEXT("ScriptAssetFilename"), *Filename);
		AssetsPackage->GetMetaData().SetValue(ExistingObject, TEXT("ScriptAssetLineNumber"), *FString::Printf(TEXT("%d"), LineNumber));
#endif

#if AS_CAN_HOTRELOAD
		if (ReloadedObject != nullptr)
		{
			if (FAngelscriptEngine* HookEngine = FAngelscriptEngine::TryGetCurrentEngine())
				HookEngine->GetOnLiteralAssetReload().Broadcast(ReloadedObject, ExistingObject);
		}
#endif
	}
	else
	{
		// Reset all the properties in the asset, we're soft reloading it and want new data
		auto* CDO = AssetClass->GetDefaultObject();
		for (TFieldIterator<FProperty> It(AssetClass); It; ++It)
		{
			FProperty* Property = *It;
			Property->CopyCompleteValue_InContainer(ExistingObject, CDO);
		}
	}

	FAngelscriptEngine::Get().GetOnLiteralAssetCreated().Broadcast(ExistingObject, AssetName);
	return ExistingObject;
}

void FAngelscriptUObjectBinds::PostLiteralAssetSetup(UObject* Asset, const FString& Name)
{
	// Tell the loading system the literal asset exists
	NotifyRegistrationEvent(TEXT("/Script/AngelscriptAssets"), *Asset->GetName(), ENotifyRegistrationType::NRT_NoExportObject,
		ENotifyRegistrationPhase::NRP_Finished, nullptr, false, Asset);

	FAngelscriptEngine::Get().GetPostLiteralAssetSetup().Broadcast(Asset, Name);
}
