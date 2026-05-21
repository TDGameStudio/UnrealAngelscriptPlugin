#include "Engine/Engine.h"

#include "UObject/Package.h"
#include "UObject/UObjectIterator.h"
#include "UObject/ObjectRedirector.h"

#include "AngelscriptEngine.h"
#include "AngelscriptType.h"
#include "AngelscriptBinds.h"
#include "AngelscriptDocs.h"
#include "AngelscriptSettings.h"
#include "ClassGenerator/AngelscriptClassGenerator.h"

#include "AssetRegistry/IAssetRegistry.h"
#include "AssetRegistry/AssetRegistryModule.h"

#include "ClassGenerator/ASClass.h"
#include "Engine/SimpleConstructionScript.h"

#include "Helper_ToString.h"

#if WITH_EDITOR
#include "UObject/MetaData.h"
#endif

#include "StartAngelscriptHeaders.h"
//#include "as_scriptengine.h"
//#include "as_objecttype.h"
#include "source/as_scriptengine.h"
#include "source/as_objecttype.h"
#include "EndAngelscriptHeaders.h"

/**
 * Binds default methods that all UObjects have
 */
AS_FORCE_LINK const FAngelscriptBinds::FBind Bind_UObject_Base((int32)FAngelscriptBinds::EOrder::Late-1, []
{
	auto UObject_ = FAngelscriptBinds::ExistingClass("UObject");
	UObject_.Method("void AddToRoot()", METHOD_TRIVIAL(UObject, AddToRoot));
	UObject_.Method("void RemoveFromRoot()", METHOD_TRIVIAL(UObject, RemoveFromRoot));
	UObject_.Method("bool GetIsRooted() const", METHOD_TRIVIAL(UObject, IsRooted));
	UObject_.Method("bool IsTransient() const", [](UObject* Object) { return Object->HasAnyFlags(RF_Transient); });
	UObject_.Method("bool IsEditorOnly() const", METHOD_TRIVIAL(UObject, IsEditorOnly));
	UObject_.Method("bool Modify(bool bAlwaysMarkDirty = true)", METHOD(UObject, Modify));
	UObject_.Method("void SetTransactional(bool bTransactional)", [](UObject* Object, bool bTransactional)
	{
		if (bTransactional)
			Object->SetFlags(RF_Transactional);
		else
			Object->ClearFlags(RF_Transactional);
	});

#if !WITH_ANGELSCRIPT_HAZE
	UObject_.Method("bool IsSupportedForNetworking() const", METHOD_TRIVIAL(UObject, IsSupportedForNetworking));
#endif

	UObject_.Method("UClass GetClass() const", METHOD_TRIVIAL(UObject, GetClass));
	UObject_.Method("UObject GetOuter() const", METHOD_TRIVIAL(UObject, GetOuter));

	UObject_.Method("UObject GetTypedOuter(const TSubclassOf<UObject>& Target) const", [](const UObject* Object, const TSubclassOf<UObject>& Target) -> UObject* {
		if (Target == nullptr)
			return nullptr;

		return Object->GetTypedOuter(Target.Get());
	});
	SCRIPT_BIND_DOCUMENTATION(
		"Traverses the outer chain searching for the next object of a certain type.\n"
		"@param Target class to search for.\n"
		"@return The first object in this object's Outer chain which is of the correct type.\n"
	)
	FAngelscriptBinds::SetPreviousBindArgumentDeterminesOutputType(0);

	UObject_.Method("UPackage GetOutermost() const", METHOD_TRIVIAL(UObject, GetOutermost));
	UObject_.Method("UPackage GetPackage() const", METHOD_TRIVIAL(UObject, GetPackage));
	UObject_.Method("bool MarkPackageDirty() const allow_discard", METHOD_TRIVIAL(UObject, MarkPackageDirty));
	UObject_.Method("UWorld GetWorld() const", METHOD_TRIVIAL(UObject, GetWorld));

	UObject_.Method("FName GetName() const", METHODPR_TRIVIAL(FName, UObject, GetFName, () const));

	UObject_.Method("FString GetFullName(const UObject StopOuter = nullptr) const", [](const UObject* Object, const UObject* StopOuter) -> FString
	{
		return Object->GetFullName(StopOuter);
	});

	UObject_.Method("FString GetPathName(const UObject StopOuter = nullptr) const", METHODPR_TRIVIAL(FString, UObject, GetPathName, (const UObject*) const));

	UObject_.Method("bool IsA(const UClass Class) const", [](UObject* Object, UClass* Class)
	{
		if (Class == nullptr)
		{
			FAngelscriptEngine::Throw("Class passed in to IsA was nullptr.");
			return false;
		}

		return Object->IsA(Class);
	});
	SCRIPT_BIND_DOCUMENTATION("Returns true if this object is of the specified type, or a child of that type.")

	UObject_.Method("bool ImplementsInterface(const UClass InterfaceClass) const",
	[](UObject* Object, UClass* InterfaceClass)
	{
		if (Object == nullptr || InterfaceClass == nullptr)
			return false;
		return Object->GetClass()->ImplementsInterface(InterfaceClass);
	});
	SCRIPT_BIND_DOCUMENTATION("Returns true if this object's class implements the specified interface.")

	// Save config property changes to ini files
	UObject_.Method("void SaveConfig()", [](UObject* Object)
	{
		Object->SaveConfig();
	});

	// Load config property changes from ini files
	UObject_.Method("void LoadConfig()", [](UObject* Object)
	{
		Object->LoadConfig();
	});

	// Reload config property changes from ini files
	UObject_.Method("void ReloadConfig()", [](UObject* Object)
	{
		Object->ReloadConfig();
	});

	// Ability to copy all properties from a different object
	UObject_.Method("void CopyScriptPropertiesFrom(const UObject OtherObject)",
	[](UObject* Object, const UObject* OtherObject)
	{
		*(asIScriptObject*)Object = *(asIScriptObject*)OtherObject;
	});

	// Down-casting is handled generically so we don't have to register a gajillion different functions
	UObject_.Method("void opCast(?& Address) const",
	[](UObject* Object, void* OutAddress, int TypeId)
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

		asCObjectType* ScriptType = (asCObjectType*)Manager.Engine->GetTypeInfoById(TypeId);
		checkSlow(ScriptType != nullptr);

		// Structs cannot be cast to uobjects
		if (ScriptType->GetFlags() & asOBJ_VALUE)
			return;

		UClass* AssociatedClass = (UClass*)ScriptType->GetUserData();
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

		// The cast is valid if the script type we're casting to
		// has a UClass associated with it that is one of the
		// parent classes of our UObject.
		if (bIsA)
		{
			*(UObject**)OutAddress = Object;
		}
		else if (bImplementsInterface)
		{
			*(UObject**)OutAddress = Object;
		}
		else
		{
			*(UObject**)OutAddress = nullptr;
		}
	});
	SCRIPT_TRIVIAL_NATIVE_UOBJECT_CAST(UObject_, TEXT("?"), false);

	FToStringHelper::Register(TEXT("UObject"), [](void* Ptr, FString& Str)
	{
		UObject* Object = (UObject*)Ptr;
		if (Object == nullptr)
		{
			Str += TEXT("{ nullptr }");
		}
		else
		{
			UClass* ObjClass = Object->GetClass();
			const UASClass* ASClass = Cast<UASClass>(ObjClass);
			const bool bUseClassPrefix =
				ObjClass->HasAnyClassFlags(CLASS_Native)
				|| (ASClass != nullptr && ASClass->bIsScriptClass);
			const TCHAR* ClassPrefix = bUseClassPrefix ? ObjClass->GetPrefixCPP() : TEXT("");

			FString Suffix;
			auto& Delegate = FAngelscriptEngine::Get().GetHooks().GetDebugObjectSuffix();
			if (Delegate.IsBound())
			{
				Delegate.Execute(Object, Suffix);
			}

#if WITH_EDITOR
			if (AActor* Actor = Cast<AActor>(Object))
			{
				Str += FString::Printf(TEXT("{ %s %s(%s%s) (ID: %s) }"),
					*Actor->GetActorLabel(),
					*Suffix,
					//(ObjClass->HasAnyClassFlags(CLASS_Native) || ObjClass->bIsScriptClass) ? ObjClass->GetPrefixCPP() : TEXT(""),
					ClassPrefix,
					*ObjClass->GetName(),
					*Object->GetName());
			}
			else
#endif
			{

				Str += FString::Printf(TEXT("{ %s %s(%s%s) }"),
					*Object->GetName(),
					*Suffix,
					//(ObjClass->HasAnyClassFlags(CLASS_Native) || ObjClass->bIsScriptClass) ? ObjClass->GetPrefixCPP() : TEXT(""),
					ClassPrefix,
					*ObjClass->GetName());
			}
		}
	}, /*bImplicitConversion = */false, /*bIsHandleType = */true);

	FAngelscriptBinds::BindGlobalFunction("bool IsValid(const UObject Object) no_discard", FUNCPR_TRIVIAL(bool, ::IsValid, (UObject*)));
	SCRIPT_BIND_DOCUMENTATION("Returns true if the object is usable: non-null and not pending kill");
});

/**
 * Binds default methods that all UClasses have
 */
AS_FORCE_LINK const FAngelscriptBinds::FBind Bind_UClass_Base((int32)FAngelscriptBinds::EOrder::Late-1, []
{
	auto UClass_ = FAngelscriptBinds::ExistingClass("UClass");
	UClass_.Method("UObject GetDefaultObject() const", [](UClass* Class)
	{
		return Class->GetDefaultObject();
	});
	UClass_.Method("FString GetSourceFilePath() const", [](UClass* Class) -> FString
	{
		if (const UASClass* ScriptClass = Cast<UASClass>(Class))
		{
			return ScriptClass->GetSourceFilePath();
		}
		return FString();
	});
	UClass_.Method("FString GetScriptModuleName() const", [](UClass* Class) -> FString
	{
		const UASClass* ScriptClass = Cast<UASClass>(Class);
		if (ScriptClass == nullptr || ScriptClass->ScriptTypePtr == nullptr)
		{
			return FString();
		}
		auto& Manager = FAngelscriptEngine::Get();
		auto Module = Manager.GetModule(((asITypeInfo*)ScriptClass->ScriptTypePtr)->GetModule());
		return Module.IsValid() ? Module->ModuleName : FString();
	});
	UClass_.Method("FString GetScriptTypeDeclaration() const", [](UClass* Class) -> FString
	{
		return Cast<UASClass>(Class) != nullptr ? FString::Printf(TEXT("%s%s"), Class->GetPrefixCPP(), *Class->GetName()) : FString();
	});
	UClass_.Method("bool IsFunctionImplementedInScript(FName InFunctionName) const", [](UClass* Class, FName InFunctionName) -> bool
	{
		if (const UASClass* ScriptClass = Cast<UASClass>(Class))
		{
			return ScriptClass->IsFunctionImplementedInScript(InFunctionName);
		}
		return false;
	});
	UClass_.Method("UFunction FindFunctionByName(FName InFunctionName) const", [](UClass* Class, FName InFunctionName) -> UFunction*
	{
		return Class != nullptr ? Class->FindFunctionByName(InFunctionName) : nullptr;
	});

	UClass_.Method("bool IsChildOf(UClass Other) const", METHODPR_TRIVIAL(bool, UClass, IsChildOf, (const UStruct*) const));
	SCRIPT_BIND_DOCUMENTATION("Returns true if this class either is the same class, or is a child class of the other class.")

	UClass_.Method("bool IsAbstract() const", [](UClass* Class)
	{
		return Class->HasAnyClassFlags(CLASS_Abstract);
	});

	UClass_.Method("UClass GetSuperClass() const", [](UClass* Class) -> UClass*
	{
		return Class->GetSuperClass();
	});

	{
		FAngelscriptBinds::FNamespace ns("UClass");
		FAngelscriptBinds::BindGlobalFunction("UClass FindClass(const FString& Name)",
		[](const FString& Name) -> UClass*
		{
			return FindObject<UClass>(nullptr, *Name);
		});

		FAngelscriptBinds::BindGlobalFunction("void GetAllClasses(TArray<UClass>& OutClasses)",
		[](TArray<UClass*>& OutClasses)
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
		});

		FAngelscriptBinds::BindGlobalFunction("TArray<UClass> GetAllSubclassesOf(UClass Class, bool bIncludeAbstractClasses = false)",
		[](UClass* ParentClass, bool bIncludeAbstractClasses) -> TArray<UClass*>
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
		});

		FAngelscriptBinds::BindGlobalFunction("UClass __StaticClass(const FString& Name)",
		[](const FString& Name) -> UClass*
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
		});
	}
});

AS_FORCE_LINK const FAngelscriptBinds::FBind Bind_UFunction_Base((int32)FAngelscriptBinds::EOrder::Late-1, []
{
	auto UFunction_ = FAngelscriptBinds::ExistingClass("UFunction");
	UFunction_.Method("FString GetSourceFilePath() const", [](UFunction* Function) -> FString
	{
		if (const UASFunction* ScriptFunction = Cast<UASFunction>(Function))
		{
			return ScriptFunction->GetSourceFilePath();
		}
		return FString();
	});
	UFunction_.Method("int GetSourceLineNumber() const", [](UFunction* Function) -> int32
	{
		if (const UASFunction* ScriptFunction = Cast<UASFunction>(Function))
		{
			return ScriptFunction->GetSourceLineNumber();
		}
		return -1;
	});
	UFunction_.Method("FString GetScriptFunctionDeclaration() const", [](UFunction* Function) -> FString
	{
		if (const UASFunction* ScriptFunction = Cast<UASFunction>(Function))
		{
			if (ScriptFunction->ScriptFunction != nullptr)
			{
				return UTF8_TO_TCHAR(ScriptFunction->ScriptFunction->GetDeclaration(false, false, false));
			}
		}
		return FString();
	});
});

static UObject* GetASConstructionScriptObject()
{
	auto* Context = asGetActiveContext();
	if (Context == nullptr)
		return nullptr;

	int32 StackDepth = Context->GetCallstackSize();
	for (int32 Frame = 0; Frame < StackDepth; ++Frame)
	{
		int ThisType = Context->GetThisTypeId(Frame);
		if (ThisType == 0)
			continue;

		auto* TypeInfo = Context->GetEngine()->GetTypeInfoById(ThisType);
		if (TypeInfo == nullptr)
			continue;

		const bool bIsScriptObject = (TypeInfo->GetFlags() & asOBJ_SCRIPT_OBJECT) != 0;
		const bool bIsRefObject = (TypeInfo->GetFlags() & asOBJ_REF) != 0;

		if (!bIsScriptObject || !bIsRefObject)
			continue;

		UObject* ThisObj = (UObject*)Context->GetThisPointer(Frame);
		if (ThisObj == nullptr)
			continue;

		// Check for functions being construct or defaults
		asIScriptFunction* Func = Context->GetFunction(Frame);
		UClass* CheckClass = ThisObj->GetClass();
		while (CheckClass != nullptr)
		{
			UASClass* ASClass = Cast<UASClass>(CheckClass);
			if (ASClass != nullptr)
			{
				if (ASClass->ConstructFunction == Func)
					return ThisObj;
				if (ASClass->DefaultsFunction == Func)
					return ThisObj;
			}

			CheckClass = CheckClass->GetSuperClass();
		}

		// Check for actors running construction scripts
		AActor* Actor = Cast<AActor>(ThisObj);
		if (Actor != nullptr)
		{
			if (Actor->IsRunningUserConstructionScript())
				return Actor;
		}
	}

	return nullptr;
}

static UObject* GAngelscriptNullObject = nullptr;

static bool IsPlayingPIE()
{
#if WITH_EDITOR
	if (GEngine == nullptr)
		return false;
	for (auto& WorldContext : GEngine->GetWorldContexts())
	{
		if (WorldContext.WorldType == EWorldType::PIE)
			return true;
	}
#endif
	return false;
}

/**
 * Bind global operations manipulating or finding UObjects.
 */
static IAssetRegistry* GetBindAssetRegistry();
AS_FORCE_LINK const FAngelscriptBinds::FBind Bind_UObject_Operations((int32)FAngelscriptBinds::EOrder::Late-1, []
{
	FAngelscriptBinds::BindGlobalVariable("const UObject null", &GAngelscriptNullObject);

	FAngelscriptBinds::BindGlobalFunction("UPackage GetTransientPackage()", FUNC_TRIVIAL(GetTransientPackage));
	FAngelscriptBinds::BindGlobalFunction("UPackage GetAngelscriptPackage()", []() {
		return FAngelscriptEngine::GetPackage();
	});

	FAngelscriptBinds::BindGlobalFunction("UClass FindClass(const FString& Name)",
	[](const FString& Name) -> UClass*
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
	});

	FAngelscriptBinds::BindGlobalFunction("void GetAllClasses(TArray<UClass>& OutClasses)",
	[](TArray<UClass*>& OutClasses)
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
	});

	FAngelscriptBinds::BindGlobalFunction(
	  "UObject NewObject(UObject Outer, const TSubclassOf<UObject>& Class, FName Name = NAME_None, bool bTransient = false)",
	[](UObject* Outer, const TSubclassOf<UObject>& Class, FName Name, bool bTransient) -> UObject*
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
	});
	FAngelscriptBinds::SetPreviousBindArgumentDeterminesOutputType(1);

#if !WITH_ANGELSCRIPT_HAZE
	FAngelscriptBinds::BindGlobalFunction(
  	  "UObject LoadObject(UObject Outer, const FString& Name)",
	[](UObject* Outer, const FString& Name) -> UObject*
	{
		FAngelscriptExcludeScopeFromLoopTimeout TimeoutExclusion;
		return LoadObject<UObject>(Outer, *Name);
	});
#endif

	FAngelscriptBinds::BindGlobalFunction(
  	  "UObject FindObject(const FString& Name)",
	[](const FString& Name) -> UObject*
	{
		return FindObject<UObject>(nullptr, *Name);
	});

	FAngelscriptBinds::BindGlobalFunction(
	  "UObject FindObject(UObject Outer, const FString& Name)",
	[](UObject* Outer, const FString& Name) -> UObject*
	{
		return FindObject<UObject>(Outer, *Name);
	});

	// Used by asset literal blocks to create their asset objects
	FAngelscriptBinds::BindGlobalFunction(
	  "UObject __CreateLiteralAsset(UClass AssetClass, const FString& Name)",
	[](UClass* AssetClass, const FString& AssetName) -> UObject*
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
				FAngelscriptClassGenerator::OnLiteralAssetReload.Broadcast(ReloadedObject, ExistingObject);
#endif
		}
		else
		{
			// Reset all the properties in the asset, we're soft reloading it and want new data
			auto* CDO = AssetClass->GetDefaultObject();
			for (TFieldIterator<FProperty> It(AssetClass); It; ++It)
			{
				FProperty* Prop = *It;
				Prop->CopyCompleteValue_InContainer(ExistingObject, CDO);
			}
		}

		FAngelscriptEngine::Get().GetHooks().GetOnLiteralAssetCreated().Broadcast(ExistingObject, AssetName);
		return ExistingObject;
	});

	FAngelscriptBinds::BindGlobalFunction(
	  "void __PostLiteralAssetSetup(UObject Asset, const FString& Name)",
	[](UObject* Asset, const FString& Name)
	{
		// Tell the loading system the literal asset exists
		NotifyRegistrationEvent(TEXT("/Script/AngelscriptAssets"), *Asset->GetName(), ENotifyRegistrationType::NRT_NoExportObject,
			ENotifyRegistrationPhase::NRP_Finished, nullptr, false, Asset);

		FAngelscriptEngine::Get().GetHooks().GetPostLiteralAssetSetup().Broadcast(Asset, Name);
	});
});

IAssetRegistry* GetBindAssetRegistry()
{
	static auto* AssetRegistryModule = FModuleManager::GetModulePtr<FAssetRegistryModule>(FName(TEXT("AssetRegistry")));
	static IAssetRegistry* AssetRegistry = AssetRegistryModule ? &AssetRegistryModule->Get() : nullptr;

	return AssetRegistry;
}
