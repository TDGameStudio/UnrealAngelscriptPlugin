#include "Bind_UObject.h"

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
 * UObject, UClass, UFunction, and global object-operation binding surface.
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | AngelScript usage signature                                                                | Purpose / parameter notes                                                                                            |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | void UObject.AddToRoot();                                                                  | Adds this object to the GC root set.                                                                                 |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | void UObject.RemoveFromRoot();                                                             | Removes this object from the GC root set.                                                                            |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | bool UObject.GetIsRooted() const;                                                          | Returns whether this object is in the GC root set.                                                                   |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | bool UObject.IsTransient() const;                                                          | Returns whether this object has the transient object flag.                                                           |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | bool UObject.IsEditorOnly() const;                                                         | Returns whether this object is editor-only.                                                                          |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | bool UObject.Modify(bool bAlwaysMarkDirty = true);                                         | Records a transaction snapshot for editor undo/redo.                                                                 |
 * |                                                                                            | @param bAlwaysMarkDirty Marks the package dirty even when no transaction is active.                                  |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | void UObject.SetTransactional(bool bTransactional);                                        | Adds or removes the transactional object flag.                                                                       |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | bool UObject.IsSupportedForNetworking() const;                                             | Returns whether this object supports networking references.                                                          |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | UClass UObject.GetClass() const;                                                           | Returns the runtime class.                                                                                           |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | UObject UObject.GetOuter() const;                                                          | Returns the immediate outer object.                                                                                  |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | UObject UObject.GetTypedOuter(const TSubclassOf<UObject>& Target) const;                   | Returns the first outer compatible with Target, typed to the requested class.                                        |
 * |                                                                                            | @param Target Class searched for while traversing the outer chain.                                                   |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | UPackage UObject.GetOutermost() const;                                                     | Returns the outermost package.                                                                                       |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | UPackage UObject.GetPackage() const;                                                       | Returns the package containing this object.                                                                          |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | bool UObject.MarkPackageDirty() const;                                                     | Marks the containing package dirty and returns whether the state changed.                                            |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | UWorld UObject.GetWorld() const;                                                           | Returns the world associated with this object, when available.                                                       |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FName UObject.GetName() const;                                                             | Returns the object short name.                                                                                       |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FString UObject.GetFullName(const UObject StopOuter = nullptr) const;                      | Returns the class-qualified object name, optionally stopping at StopOuter.                                           |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FString UObject.GetPathName(const UObject StopOuter = nullptr) const;                      | Returns the object path, optionally stopping at StopOuter.                                                           |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | bool UObject.IsA(const UClass Class) const;                                                | Returns whether the object class is Class or derives from it.                                                        |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | bool UObject.ImplementsInterface(const UClass InterfaceClass) const;                       | Returns whether the object class implements InterfaceClass.                                                          |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | void UObject.SaveConfig();                                                                 | Writes config-backed properties to configuration files.                                                              |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | void UObject.LoadConfig();                                                                 | Loads config-backed properties from configuration files.                                                             |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | void UObject.ReloadConfig();                                                               | Reloads config-backed properties from configuration files.                                                           |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | void UObject.CopyScriptPropertiesFrom(const UObject OtherObject);                          | Copies compatible script properties from OtherObject.                                                                |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | TargetType CastObject = Cast<TargetType>(Object);                                          | Dynamically casts a UObject handle to the requested registered UObject type.                                         |
 * |                                                                                            | The generic opCast expands through AngelScript Cast<TargetType> syntax.                                              |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | bool IsValid(const UObject Object);                                                        | Returns whether Object is non-null and not pending destruction.                                                      |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | Text + Object;                                                                             | Appends the UObject text representation to a string and returns the result.                                          |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | Text += Object;                                                                            | Appends the UObject text representation to a string in place.                                                        |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | Text.Append(Object);                                                                       | Appends the UObject text representation to a temporary or existing string.                                           |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FString UObject.ToString() const;                                                          | Returns the UObject text representation.                                                                             |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | UObject UClass.GetDefaultObject() const;                                                   | Returns the class default object.                                                                                    |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FString UClass.GetSourceFilePath() const;                                                  | Returns the source file that declares a script class, or an empty string.                                            |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FString UClass.GetScriptModuleName() const;                                                | Returns the owning AngelScript module name for a script class.                                                       |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FString UClass.GetScriptTypeDeclaration() const;                                           | Returns the AngelScript type declaration for a script class.                                                         |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | bool UClass.IsFunctionImplementedInScript(FName InFunctionName) const;                     | Returns whether a named function has a script implementation on this class.                                          |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | UFunction UClass.FindFunctionByName(FName InFunctionName) const;                           | Finds a reflected function on this class by name.                                                                    |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | bool UClass.IsChildOf(UClass Other) const;                                                 | Returns whether this class equals or derives from Other.                                                             |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | bool UClass.IsAbstract() const;                                                            | Returns whether the class has the abstract class flag.                                                               |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | UClass UClass.GetSuperClass() const;                                                       | Returns the immediate superclass.                                                                                    |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | UClass UClass::FindClass(const FString& Name);                                             | Finds a loaded class by reflected object name.                                                                       |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | void UClass::GetAllClasses(TArray<UClass>& OutClasses);                                    | Collects all loaded classes.                                                                                         |
 * |                                                                                            | @param OutClasses Receives the loaded class handles.                                                                 |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | TArray<UClass> UClass::GetAllSubclassesOf(                                                 | Returns loaded subclasses of Class.                                                                                  |
 * |     UClass Class, bool bIncludeAbstractClasses = false);                                   | @param bIncludeAbstractClasses Includes abstract subclasses when true.                                               |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | UClass UClass::__StaticClass(const FString& Name);                                         | Resolves an internal static-class request by script type name.                                                       |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FString UFunction.GetSourceFilePath() const;                                               | Returns the source file that declares a script function, or an empty string.                                         |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | int UFunction.GetSourceLineNumber() const;                                                 | Returns the source line that declares a script function.                                                             |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FString UFunction.GetScriptFunctionDeclaration() const;                                    | Returns the AngelScript declaration for a script function.                                                           |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | const UObject null;                                                                        | Exposes the canonical null UObject handle constant.                                                                  |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | UPackage GetTransientPackage();                                                            | Returns the engine transient package.                                                                                |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | UPackage GetAngelscriptPackage();                                                          | Returns the package containing generated AngelScript types.                                                          |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | UClass FindClass(const FString& Name);                                                     | Finds a class by AngelScript type name.                                                                              |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | void GetAllClasses(TArray<UClass>& OutClasses);                                            | Collects all loaded classes.                                                                                         |
 * |                                                                                            | @param OutClasses Receives the loaded class handles.                                                                 |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | UObject NewObject(UObject Outer, const TSubclassOf<UObject>& Class,                        | Creates an object typed to Class within Outer.                                                                       |
 * |     FName Name = NAME_None, bool bTransient = false);                                      | @param bTransient Applies transient lifetime/persistence semantics when true.                                        |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | UObject LoadObject(UObject Outer, const FString& Name);                                    | Loads an object by path or name, using Outer as the resolution context.                                              |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | UObject FindObject(const FString& Name);                                                   | Finds a loaded object by path or name.                                                                               |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | UObject FindObject(UObject Outer, const FString& Name);                                    | Finds a loaded object with the requested name inside Outer.                                                          |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | UObject __CreateLiteralAsset(UClass AssetClass, const FString& Name);                      | Creates or resolves the internal asset object used by an asset literal.                                              |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | void __PostLiteralAssetSetup(UObject Asset, const FString& Name);                          | Completes internal asset-literal setup after object creation.                                                        |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 */

/*
 * Binds default methods that all UObjects have
 */
AS_FORCE_LINK const FAngelscriptBind Bind_UObject_Base(
	TEXT("UObject.Base"),
	EAngelscriptBindPhase::ExplicitBindings,
	[](FAngelscriptBinds& Binds)
	{
		auto UObject_ = Binds.ExistingClassForTarget("UObject");
		UObject_.Method("void AddToRoot()", METHOD_TRIVIAL(UObject, AddToRoot));
		UObject_.Method("void RemoveFromRoot()", METHOD_TRIVIAL(UObject, RemoveFromRoot));
		UObject_.Method("bool GetIsRooted() const", METHOD_TRIVIAL(UObject, IsRooted));
		UObject_.Method("bool IsTransient() const", &FAngelscriptUObjectBinds::IsTransient);
		UObject_.Method("bool IsEditorOnly() const", METHOD_TRIVIAL(UObject, IsEditorOnly));
		UObject_.Method("bool Modify(bool bAlwaysMarkDirty = true)", METHOD(UObject, Modify));
		UObject_.Method("void SetTransactional(bool bTransactional)", &FAngelscriptUObjectBinds::SetTransactional);

		UObject_.Method("bool IsSupportedForNetworking() const", METHOD_TRIVIAL(UObject, IsSupportedForNetworking));

		UObject_.Method("UClass GetClass() const", METHOD_TRIVIAL(UObject, GetClass));
		UObject_.Method("UObject GetOuter() const", METHOD_TRIVIAL(UObject, GetOuter));

		UObject_.Method(
			"UObject GetTypedOuter(const TSubclassOf<UObject>& Target) const",
			&FAngelscriptUObjectBinds::GetTypedOuter)
			.Documentation(TEXT(
				"Traverses the outer chain searching for the next object of a certain type.\n"
				"@param Target class to search for.\n"
				"@return The first object in this object's Outer chain which is of the correct type.\n"))
			.DeterminesOutputType(0);

		UObject_.Method("UPackage GetOutermost() const", METHOD_TRIVIAL(UObject, GetOutermost));
		UObject_.Method("UPackage GetPackage() const", METHOD_TRIVIAL(UObject, GetPackage));
		UObject_.Method("bool MarkPackageDirty() const allow_discard", METHOD_TRIVIAL(UObject, MarkPackageDirty));
		UObject_.Method("UWorld GetWorld() const", METHOD_TRIVIAL(UObject, GetWorld));

		UObject_.Method("FName GetName() const", METHODPR_TRIVIAL(FName, UObject, GetFName, () const));
		UObject_.Method("FString GetFullName(const UObject StopOuter = nullptr) const", &FAngelscriptUObjectBinds::GetFullName);
		UObject_.Method("FString GetPathName(const UObject StopOuter = nullptr) const", METHODPR_TRIVIAL(FString, UObject, GetPathName, (const UObject*) const));

		UObject_.Method("bool IsA(const UClass Class) const", &FAngelscriptUObjectBinds::IsA)
			.Documentation(TEXT("Returns true if this object is of the specified type, or a child of that type."));

		UObject_.Method("bool ImplementsInterface(const UClass InterfaceClass) const", &FAngelscriptUObjectBinds::ImplementsInterface)
			.Documentation(TEXT("Returns true if this object's class implements the specified interface."));

		// Save config property changes to ini files
		UObject_.Method("void SaveConfig()", &FAngelscriptUObjectBinds::SaveConfig);

		// Load config property changes from ini files
		UObject_.Method("void LoadConfig()", &FAngelscriptUObjectBinds::LoadConfig);

		// Reload config property changes from ini files
		UObject_.Method("void ReloadConfig()", &FAngelscriptUObjectBinds::ReloadConfig);

		// Ability to copy all properties from a different object
		UObject_.Method("void CopyScriptPropertiesFrom(const UObject OtherObject)", &FAngelscriptUObjectBinds::CopyScriptPropertiesFrom);

		// Down-casting is handled generically so we don't have to register a gajillion different functions
		UObject_.Method("void opCast(?& Address) const", &FAngelscriptUObjectBinds::CastToType)
			.NativeUObjectCast(TEXT("?"), false);

		Binds.BindGlobalFunctionForTarget("bool IsValid(const UObject Object) no_discard", FUNCPR_TRIVIAL(bool, ::IsValid, (UObject*)))
			.Documentation(TEXT("Returns true if the object is usable: non-null and not pending kill"));
	});

AS_FORCE_LINK const FAngelscriptBind Bind_UObject_ToStringContribution(
	TEXT("UObject.ToStringContribution"),
	EAngelscriptBindPhase::TypeInfrastructure,
	[](FAngelscriptBinds& Binds)
	{
		FToStringHelper::Register(Binds, TEXT("UObject"), &FAngelscriptUObjectBinds::AppendToString,
			/*bImplicitConversion = */false, /*bIsHandleType = */true);
	});

/*
 * Binds default methods that all UClasses have
 */
AS_FORCE_LINK const FAngelscriptBind Bind_UClass_Base(
	TEXT("UClass.Base"),
	EAngelscriptBindPhase::ExplicitBindings,
	[](FAngelscriptBinds& Binds)
	{
		auto UClass_ = Binds.ExistingClassForTarget("UClass");
		UClass_.Method("UObject GetDefaultObject() const", &FAngelscriptUObjectBinds::GetDefaultObject);
		UClass_.Method("FString GetSourceFilePath() const", &FAngelscriptUObjectBinds::GetClassSourceFilePath);
		UClass_.Method("FString GetScriptModuleName() const", &FAngelscriptUObjectBinds::GetScriptModuleName);
		UClass_.Method("FString GetScriptTypeDeclaration() const", &FAngelscriptUObjectBinds::GetScriptTypeDeclaration);
		UClass_.Method("bool IsFunctionImplementedInScript(FName InFunctionName) const", &FAngelscriptUObjectBinds::IsFunctionImplementedInScript);
		UClass_.Method("UFunction FindFunctionByName(FName InFunctionName) const", &FAngelscriptUObjectBinds::FindFunctionByName);

		UClass_.Method("bool IsChildOf(UClass Other) const", METHODPR_TRIVIAL(bool, UClass, IsChildOf, (const UStruct*) const))
			.Documentation(TEXT("Returns true if this class either is the same class, or is a child class of the other class."));

		UClass_.Method("bool IsAbstract() const", &FAngelscriptUObjectBinds::IsAbstract);
		UClass_.Method("UClass GetSuperClass() const", &FAngelscriptUObjectBinds::GetSuperClass);

		{
			FAngelscriptBinds::FNamespace Namespace(Binds.GetTargetEngine(), "UClass");
			Binds.BindGlobalFunctionForTarget("UClass FindClass(const FString& Name)", &FAngelscriptUObjectBinds::FindClassByObjectName);
			Binds.BindGlobalFunctionForTarget("void GetAllClasses(TArray<UClass>& OutClasses)", &FAngelscriptUObjectBinds::GetAllClasses);
			Binds.BindGlobalFunctionForTarget(
				"TArray<UClass> GetAllSubclassesOf(UClass Class, bool bIncludeAbstractClasses = false)",
				&FAngelscriptUObjectBinds::GetAllSubclassesOf);
			Binds.BindGlobalFunctionForTarget("UClass __StaticClass(const FString& Name)", &FAngelscriptUObjectBinds::FindClassByScriptName);
		}
	});

AS_FORCE_LINK const FAngelscriptBind Bind_UFunction_Base(
	TEXT("UFunction.Base"),
	EAngelscriptBindPhase::ExplicitBindings,
	[](FAngelscriptBinds& Binds)
	{
		auto UFunction_ = Binds.ExistingClassForTarget("UFunction");
		UFunction_.Method("FString GetSourceFilePath() const", &FAngelscriptUObjectBinds::GetFunctionSourceFilePath);
		UFunction_.Method("int GetSourceLineNumber() const", &FAngelscriptUObjectBinds::GetFunctionSourceLineNumber);
		UFunction_.Method("FString GetScriptFunctionDeclaration() const", &FAngelscriptUObjectBinds::GetScriptFunctionDeclaration);
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

/*
 * Bind global operations manipulating or finding UObjects.
 */
static IAssetRegistry* GetBindAssetRegistry();
AS_FORCE_LINK const FAngelscriptBind Bind_UObject_Operations(
	TEXT("UObject.Operations"),
	EAngelscriptBindPhase::ExplicitBindings,
	[](FAngelscriptBinds& Binds)
	{
		Binds.BindGlobalVariableForTarget("const UObject null", &GAngelscriptNullObject);

		Binds.BindGlobalFunctionForTarget("UPackage GetTransientPackage()", FUNC_TRIVIAL(GetTransientPackage));
		Binds.BindGlobalFunctionForTarget("UPackage GetAngelscriptPackage()", &FAngelscriptUObjectBinds::GetAngelscriptPackage);
		Binds.BindGlobalFunctionForTarget("UClass FindClass(const FString& Name)", &FAngelscriptUObjectBinds::FindClassByScriptName);
		Binds.BindGlobalFunctionForTarget("void GetAllClasses(TArray<UClass>& OutClasses)", &FAngelscriptUObjectBinds::GetAllClasses);

		Binds.BindGlobalFunctionForTarget(
			"UObject NewObject(UObject Outer, const TSubclassOf<UObject>& Class, FName Name = NAME_None, bool bTransient = false)",
			&FAngelscriptUObjectBinds::CreateObject)
			.DeterminesOutputType(1);

		Binds.BindGlobalFunctionForTarget(
			"UObject LoadObject(UObject Outer, const FString& Name)",
			&FAngelscriptUObjectBinds::LoadObjectByName);

		Binds.BindGlobalFunctionForTarget(
			"UObject FindObject(const FString& Name)",
			&FAngelscriptUObjectBinds::FindObjectByName);

		Binds.BindGlobalFunctionForTarget(
			"UObject FindObject(UObject Outer, const FString& Name)",
			&FAngelscriptUObjectBinds::FindObjectWithinOuter);

		// Used by asset literal blocks to create their asset objects
		Binds.BindGlobalFunctionForTarget(
			"UObject __CreateLiteralAsset(UClass AssetClass, const FString& Name)",
			&FAngelscriptUObjectBinds::CreateLiteralAsset);

		Binds.BindGlobalFunctionForTarget(
			"void __PostLiteralAssetSetup(UObject Asset, const FString& Name)",
			&FAngelscriptUObjectBinds::PostLiteralAssetSetup);
	});

IAssetRegistry* GetBindAssetRegistry()
{
	static auto* AssetRegistryModule = FModuleManager::GetModulePtr<FAssetRegistryModule>(FName(TEXT("AssetRegistry")));
	static IAssetRegistry* AssetRegistry = AssetRegistryModule ? &AssetRegistryModule->Get() : nullptr;

	return AssetRegistry;
}
