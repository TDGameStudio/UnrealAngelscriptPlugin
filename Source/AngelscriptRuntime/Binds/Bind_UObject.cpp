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

#include "Bind_UObject_Functions.h"
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

namespace
{
	void BindUObjectBase(FAngelscriptBinds& Binds)
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
	}

	void BindUObjectToStringContribution(FAngelscriptBinds& Binds)
	{
		FToStringHelper::Register(Binds, TEXT("UObject"), &FAngelscriptUObjectBinds::AppendToString,
			/*bImplicitConversion = */false, /*bIsHandleType = */true);
	}

	void BindUClassBase(FAngelscriptBinds& Binds)
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
	}

	void BindUFunctionBase(FAngelscriptBinds& Binds)
	{
		auto UFunction_ = Binds.ExistingClassForTarget("UFunction");
		UFunction_.Method("FString GetSourceFilePath() const", &FAngelscriptUObjectBinds::GetFunctionSourceFilePath);
		UFunction_.Method("int GetSourceLineNumber() const", &FAngelscriptUObjectBinds::GetFunctionSourceLineNumber);
		UFunction_.Method("FString GetScriptFunctionDeclaration() const", &FAngelscriptUObjectBinds::GetScriptFunctionDeclaration);
	}
}

/**
 * Binds default methods that all UObjects have
 */
AS_FORCE_LINK const FAngelscriptBind Bind_UObject_Base(
	TEXT("UObject.Base"),
	EAngelscriptBindPhase::ManualBindings,
	&BindUObjectBase);

AS_FORCE_LINK const FAngelscriptBind Bind_UObject_ToStringContribution(
	TEXT("UObject.ToStringContribution"),
	EAngelscriptBindPhase::TypeInfrastructure,
	&BindUObjectToStringContribution);

/**
 * Binds default methods that all UClasses have
 */
AS_FORCE_LINK const FAngelscriptBind Bind_UClass_Base(
	TEXT("UClass.Base"),
	EAngelscriptBindPhase::ManualBindings,
	&BindUClassBase);

AS_FORCE_LINK const FAngelscriptBind Bind_UFunction_Base(
	TEXT("UFunction.Base"),
	EAngelscriptBindPhase::ManualBindings,
	&BindUFunctionBase);

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
namespace
{
	void BindUObjectOperations(FAngelscriptBinds& Binds)
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
	}
}

AS_FORCE_LINK const FAngelscriptBind Bind_UObject_Operations(
	TEXT("UObject.Operations"),
	EAngelscriptBindPhase::ManualBindings,
	&BindUObjectOperations);

IAssetRegistry* GetBindAssetRegistry()
{
	static auto* AssetRegistryModule = FModuleManager::GetModulePtr<FAssetRegistryModule>(FName(TEXT("AssetRegistry")));
	static IAssetRegistry* AssetRegistry = AssetRegistryModule ? &AssetRegistryModule->Get() : nullptr;

	return AssetRegistry;
}
