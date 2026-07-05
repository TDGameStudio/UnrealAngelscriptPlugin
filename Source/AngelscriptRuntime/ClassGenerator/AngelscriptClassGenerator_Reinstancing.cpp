#include "ClassGenerator/AngelscriptClassGenerator.h"
#include "ClassGenerator/AngelscriptClassGeneratorShared.h"
#include "ClassGenerator/AngelscriptClassRedirects.h"
#include "ClassGenerator/ASClass.h"
#include "ClassGenerator/ASStruct.h"

#include "UObject/Package.h"
#include "UObject/MetaData.h"
#include "UObject/UObjectHash.h"
#include "UObject/UObjectIterator.h"
#include "UObject/GarbageCollection.h"
#include "UObject/GarbageCollectionSchema.h"
#include "UObject/CoreRedirects.h"
#include "UnversionedPropertySerialization.h"

#include "Misc/ScopedSlowTask.h"

#include "GameFramework/Actor.h"
#include "Components/ActorComponent.h"
#include "Components/SceneComponent.h"
#include "Subsystems/SubsystemCollection.h"
#include "Subsystems/WorldSubsystem.h"

#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/UserDefinedEnum.h"

#include "AngelscriptType.h"
#include "AngelscriptDebugValue.h"
#include "AngelscriptInclude.h"
#include "AngelscriptMemoryTags.h"
#include "AngelscriptPerformanceStats.h"
#include "AngelscriptSettings.h"
#include "Binds/BlueprintCallableReflectiveFallback.h"
#include "Binds/Helper_FunctionSignature.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_config.h"
#include "source/as_scriptengine.h"
#include "source/as_scriptfunction.h"
#include "source/as_objecttype.h"
#include "source/as_scriptobject.h"
#include "source/as_context.h"
#include "source/as_generic.h"
#include "EndAngelscriptHeaders.h"

using namespace AngelscriptClassGeneratorNames;

void FAngelscriptClassGenerator::DestructScriptObject(class asCScriptObject* ScriptObject, UASClass* NewClass, class asCObjectType* ObjectTypeToDestruct)
{
	// Pretend-destroy old object
	if (ObjectTypeToDestruct != nullptr)
		ScriptObject->CallDestructor(ObjectTypeToDestruct);

	auto* NewObjectType = (asCObjectType*)NewClass->ScriptTypePtr;
	if (NewObjectType == nullptr)
		return;

	// Zero out the memory used for the script object, we don't want to have trash data in here still
	UObject* Object = FAngelscriptEngine::AngelscriptToUObject(ScriptObject);
	UASClass* ASClass = UASClass::GetFirstASClass(Object);
	FMemory::Memzero((void*)((SIZE_T)Object + ASClass->ScriptPropertyOffset),
		ASClass->GetPropertiesSize() - ASClass->ScriptPropertyOffset);
}

void FAngelscriptClassGenerator::ReinitializeScriptObject(class asCScriptObject* ScriptObject, UASClass* NewClass, class asCObjectType* ObjectTypeToConstruct)
{
	if (ObjectTypeToConstruct == nullptr)
		return;

	// Construct the C++ part of the angelscript scriptobject
	new(ScriptObject) asCScriptObject(ObjectTypeToConstruct);

	if (ObjectTypeToConstruct->beh.construct != 0)
	{
		// Call the angelscript constructor of the scriptobject
		auto& Manager = FAngelscriptEngine::Get();
		asIScriptFunction* ConstructFunction = Manager.Engine->GetFunctionById(ObjectTypeToConstruct->beh.construct);
		FAngelscriptContext Context((UObject*)ScriptObject, ConstructFunction->GetEngine());
		if (!PrepareAngelscriptContextWithLog(Context, ConstructFunction, TEXT("FAngelscriptClassGenerator::ReinitializeScriptObject")))
		{
			return;
		}
		Context->SetObject(ScriptObject);
		Context->Execute();
	}
	else
	{
		ensureMsgf(false, TEXT("Angelscript implemented class does not have a constructor with no arguments. This will crash soon."));
	}
}

static UE::GC::ObjectAROFn GetARO(UClass* Class)
{
	UE::GC::ObjectAROFn ARO = Class->CppClassStaticFunctions.GetAddReferencedObjects();
	check(ARO != nullptr);
	return ARO != &UObject::AddReferencedObjects ? ARO : nullptr;
}

void FAngelscriptClassGenerator::DetectAngelscriptReferences(TSharedPtr<FAngelscriptClassDesc> ClassDesc)
{
	UASClass* Class = (UASClass*)ClassDesc->Class;
	asITypeInfo* ScriptType = (asITypeInfo*)Class->ScriptTypePtr;
	if (ScriptType == nullptr)
		return;

	// Pop the End-Of-Stream token from the end of the class' existing stream
	UE::GC::FSchemaBuilder Schema(0);
	UE::GC::FPropertyStack PropertyStack;

	FAngelscriptType::FGCReferenceParams RefParams;
	RefParams.Class = Class;
	RefParams.DebugPath = &PropertyStack;
	RefParams.Schema = &Schema;

	Schema.Append(Class->ReferenceSchema.Get());
	const int32 NumPreviousMembers = Schema.NumMembers();

	for (int32 i = 0, PropertyCount = ScriptType->GetPropertyCount(); i < PropertyCount; ++i)
	{
		const char* Name;
		int PropertyOffset;
		int TypeId;

		ScriptType->GetProperty(i, &Name, &TypeId, nullptr, nullptr, &PropertyOffset);

		// We don't care about primitives for this
		if (TypeId <= asTYPEID_LAST_PRIMITIVE)
			continue;

		// Our super class will have dealt with inherited properties
		if (ScriptType->IsPropertyInherited(i))
			continue;

		bool bAddedAsUnrealProperty = false;
		FAngelscriptTypeUsage PropertyType;

		auto PropDesc = ClassDesc->GetProperty(ANSI_TO_TCHAR(Name));
		if (PropDesc.IsValid())
		{
			PropertyType = PropDesc->PropertyType;
			bAddedAsUnrealProperty = PropDesc->bHasUnrealProperty;
		}
		else
		{
			PropertyType = FAngelscriptTypeUsage::FromProperty(ScriptType, i);
		}

		if (!bAddedAsUnrealProperty)
		{
			if(PropertyType.HasReferences())
			{
				RefParams.AtOffset = PropertyOffset;
				RefParams.Names.Push(Name);
				PropertyType.EmitReferenceInfo(RefParams);
				RefParams.Names.Pop();
			}
		}
	}

	const bool bOverrideReferenceSchema = Schema.NumMembers() != NumPreviousMembers || NumPreviousMembers == 0;
	if (bOverrideReferenceSchema)
	{
		UE::GC::FSchemaView View(Schema.Build(GetARO(Class)), UE::GC::EOrigin::Other);
		Class->ReferenceSchema.Set(View);
	}
}

void FAngelscriptClassGenerator::CreateDebugValuePrototype(TSharedPtr<FAngelscriptClassDesc> ClassDesc)
{
	UASClass* Class = (UASClass*)ClassDesc->Class;
	asITypeInfo* ScriptType = (asITypeInfo*)Class->ScriptTypePtr;
	if (ScriptType == nullptr)
		return;

	int32 CodeClassSize = 0;
	if (ClassDesc->CodeSuperClass != nullptr)
		CodeClassSize = ClassDesc->CodeSuperClass->GetPropertiesSize();

	Class->DebugValues.Reset();

	for (int32 i = 0, PropertyCount = ScriptType->GetPropertyCount(); i < PropertyCount; ++i)
	{
		const char* Name;
		int PropertyOffset;
		int TypeId;

		ScriptType->GetProperty(i, &Name, &TypeId, nullptr, nullptr, &PropertyOffset);

		// Don't need to create debug values for code properties
		if (PropertyOffset < CodeClassSize)
			continue;

		FAngelscriptTypeUsage PropertyType = FAngelscriptTypeUsage::FromProperty(ScriptType, i);
		FASDebugValue* Value = PropertyType.CreateDebugValue(Class->DebugValues, PropertyOffset);
		if (Value != nullptr)
			Value->Name = FName(ANSI_TO_TCHAR(Name));
	}
}

void FAngelscriptClassGenerator::CleanupRemovedClass(TSharedPtr<FAngelscriptClassDesc> ClassDesc)
{
	UASClass* Class = (UASClass*)ClassDesc->Class;
	if (Class != nullptr)
	{
		FString RemovedClassName = FString::Printf(TEXT("%s_REPLACED_%d"), *Class->GetName(), UniqueCounter());
		Class->Rename(*RemovedClassName, nullptr, REN_DontCreateRedirectors);
		Class->ScriptTypePtr = nullptr;
		Class->OwnerScriptEngine = nullptr;
		Class->ConstructFunction = nullptr;
		Class->DefaultsFunction = nullptr;

		// Classes that no longer exist should be removed from placement and menus
		Class->ClassFlags |= CLASS_NotPlaceable;
		Class->ClassFlags |= CLASS_HideDropDown;
		Class->ClassFlags |= CLASS_Hidden;

		#if WITH_EDITOR
		TArray<FName> FuncNames;
		Class->GenerateFunctionList(FuncNames);

		for (const FName& Elem : FuncNames)
		{
			UFunction* Func = Class->FindFunctionByName(Elem);
			UASFunction* Function = Cast<UASFunction>(Func);
			if (Function == nullptr)
				continue;

			Function->ScriptFunction = nullptr;
		}
		#endif

		if (Class->IsRooted())
		{
			Class->RemoveFromRoot();
		}
		Class->ClearFlags(RF_Standalone);
	}

	UASStruct* Struct = (UASStruct*)ClassDesc->Struct;
	if (Struct != nullptr)
	{
		FString RemovedStructName = FString::Printf(TEXT("%s_REPLACED_%d"), *Struct->GetName(), UniqueCounter());
		Struct->Rename(*RemovedStructName, nullptr, REN_DontCreateRedirectors);
		Struct->ScriptType = nullptr;
		Struct->UpdateScriptType();
		if (Struct->IsRooted())
		{
			Struct->RemoveFromRoot();
		}
		Struct->ClearFlags(RF_Standalone);
	}
}

