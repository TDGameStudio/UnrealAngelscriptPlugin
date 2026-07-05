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

int32 FAngelscriptClassGenerator::AddClassProperties(TSharedPtr<FAngelscriptClassDesc> ClassDesc)
{
	auto* ScriptType = (asCObjectType*)ClassDesc->ScriptType;
	if (ScriptType == nullptr)
		return ClassDesc->CodeSuperClass->GetPropertiesSize();

	int32 PropertiesSize = ScriptType->GetSize();
	FArchive ArDummy;

	UStruct* InStruct = ClassDesc->Class ? ClassDesc->Class : ClassDesc->Struct;

	auto MarkUStructContainsReference = [InStruct]() {
		if (UClass* ObjClass = Cast<UClass>(InStruct))
		{
			ObjClass->ClassFlags |= CLASS_HasInstancedReference;
		}
		else if (UScriptStruct* Struct = Cast<UScriptStruct>(InStruct))
		{
			Struct->StructFlags = EStructFlags(Struct->StructFlags | STRUCT_HasInstancedReference);
		}
	};

	auto BubbleUpInstanceReferenceFlags = [MarkUStructContainsReference](FProperty* Property, FProperty* InnerProperty) {
		// If a struct is marked as STRUCT_HasInstancedReference then the struct property must be marked with
		// CPF_ContainsInstancedReference and our owning UStruct (Class or Struct) must be marked appropriately as well
		if (FStructProperty* StructProperty = CastField<FStructProperty>(InnerProperty))
		{
			if (StructProperty->Struct->StructFlags & STRUCT_HasInstancedReference)
			{
				// If the struct was contained within a container, that container must be marked as containing references as well
				if (Property)
				{
					Property->SetPropertyFlags(CPF_ContainsInstancedReference);
				}
				StructProperty->SetPropertyFlags(CPF_ContainsInstancedReference);
				MarkUStructContainsReference();
			}
		}
	};

	auto ApplyInstancedPropertyFlags = [](FProperty* Property, FProperty* InnerProperty) {
		const FName NAME_EditInline(TEXT("EditInline"));

		if (Property)
		{
			Property->SetPropertyFlags(CPF_ContainsInstancedReference);
#if WITH_EDITOR
			Property->SetMetaData(NAME_EditInline, FString("true"));
#endif
		}

		if (InnerProperty)
		{
			InnerProperty->SetPropertyFlags(CPF_InstancedReference | CPF_ExportObject | CPF_PersistentInstance);
#if WITH_EDITOR
			InnerProperty->SetMetaData(NAME_EditInline, FString("true"));
#endif
		}
	};

	// Add any properties from angelscript as FProperty to the class
	for (int32 i = ScriptType->properties.GetLength() - 1; i >= 0; --i)
	{
		asCObjectProperty* ScriptProp = ScriptType->properties[i];
		int PropertyOffset = ScriptProp->byteOffset;

		// Don't create new UProperties for inherited properties, our superclass will have them
		if (ScriptType->IsPropertyInherited(i))
			continue;

		// Make sure the reload for the property type has completed
		if (ScriptProp->type.IsObject() && !ScriptProp->type.IsReferenceType())
			EnsureReloaded(ScriptType->engine->GetTypeIdFromDataType(ScriptProp->type));

		TSharedPtr<FAngelscriptPropertyDesc> PropDesc = ClassDesc->GetProperty(ScriptProp->name);
		if (PropDesc.IsValid())
		{
			// Add the property as an exported UPROPERTY() on the class
			FAngelscriptTypeUsage PropertyType = PropDesc->PropertyType;

			FAngelscriptType::FPropertyParams Params;
			Params.Struct = InStruct;
			Params.Outer = InStruct;
			Params.PropertyName = FName(ScriptProp->name.AddressOf());

			FProperty* NewProperty = PropertyType.CreateProperty(Params);

			PropDesc->bHasUnrealProperty = true;

#if WITH_EDITOR
			for (auto& Elem : PropDesc->Meta)
				NewProperty->SetMetaData(Elem.Key, *Elem.Value);

			if (PropDesc->bIsProtected)
				NewProperty->SetMetaData(FUNCMETA_BlueprintProtected, TEXT("true"));
#endif

			//NewProperty->SetPropertyFlags(CPF_RuntimeGenerated);

			if (PropDesc->bReplicated)
			{
				NewProperty->SetPropertyFlags(CPF_Net);
				NewProperty->SetBlueprintReplicationCondition(PropDesc->ReplicationCondition);

				if (PropDesc->bRepNotify)
				{
					FString* RepNotifyFunc = PropDesc->Meta.Find(TEXT("ReplicatedUsing"));
					if (RepNotifyFunc != nullptr)
					{
						NewProperty->SetPropertyFlags(CPF_RepNotify);
						NewProperty->RepNotifyFunc = FName(**RepNotifyFunc);
					}
				}
			}
			else
			{
				if (PropDesc->bSkipReplication)
				{
					NewProperty->SetPropertyFlags(CPF_RepSkip);
				}
			}

			if (PropDesc->bSkipSerialization)
			{
				NewProperty->SetPropertyFlags(CPF_SkipSerialization);
			}

			if (PropDesc->bSaveGame)
			{
				NewProperty->SetPropertyFlags(CPF_SaveGame);
			}

			// Read property specifiers from descriptor
			if ((PropDesc->bBlueprintReadable || PropDesc->bBlueprintWritable) && (!PropDesc->bIsPrivate || PropDesc->Meta.Find(NAME_AllowPrivateAccess)))
			{
				NewProperty->SetPropertyFlags(CPF_BlueprintVisible);
				if (!PropDesc->bBlueprintWritable)
					NewProperty->SetPropertyFlags(CPF_BlueprintReadOnly);
			}

			if (!NewProperty->HasAnyPropertyFlags(CPF_BlueprintAssignable))
			{
				if (PropDesc->bEditableOnInstance || PropDesc->bEditableOnDefaults)
				{
					NewProperty->SetPropertyFlags(CPF_Edit);
					if (!PropDesc->bEditableOnInstance)
						NewProperty->SetPropertyFlags(CPF_DisableEditOnInstance);
					if (!PropDesc->bEditableOnDefaults)
						NewProperty->SetPropertyFlags(CPF_DisableEditOnTemplate);
					if (PropDesc->bEditConst)
						NewProperty->SetPropertyFlags(CPF_EditConst);
				}
			}
			else if (PropDesc->Meta.Find(TEXT("BPCannotCallEvent")) || PropDesc->bIsPrivate || PropDesc->bIsProtected)
			{
				NewProperty->ClearPropertyFlags(CPF_BlueprintCallable);
			}

			if (PropDesc->bInstancedReference)
			{
				NewProperty->SetPropertyFlags(CPF_InstancedReference | CPF_ExportObject | CPF_EditConst);
			}

			if (PropDesc->bPersistentInstance)
			{
				MarkUStructContainsReference();
			}

			if (PropDesc->bAdvancedDisplay)
				NewProperty->SetPropertyFlags(CPF_AdvancedDisplay);

			if (PropDesc->bTransient)
				NewProperty->SetPropertyFlags(CPF_Transient);

			if (PropDesc->bConfig)
				NewProperty->SetPropertyFlags(CPF_Config);

			if (PropDesc->bInterp)
				NewProperty->SetPropertyFlags(CPF_Interp);

			if (PropDesc->bAssetRegistrySearchable)
				NewProperty->SetPropertyFlags(CPF_AssetRegistrySearchable);

			if (PropDesc->bNoClear)
				NewProperty->SetPropertyFlags(CPF_NoClear);

			if (PropDesc->Meta.Contains(NAME_ExposeOnSpawn))
				NewProperty->SetPropertyFlags(CPF_ExposeOnSpawn);

			if (PropDesc->Meta.Contains(NAME_EditFixedSize))
				NewProperty->SetPropertyFlags(CPF_EditFixedSize);

			if (PropDesc->Meta.Contains(NAME_Meta_EditorOnly))
				NewProperty->SetPropertyFlags(CPF_EditorOnly);

			// If any containers contain instanced references, make sure to bubble up their instance reference flags
			if (FArrayProperty* ArrayProp = CastField<FArrayProperty>(NewProperty))
			{
				BubbleUpInstanceReferenceFlags(ArrayProp, ArrayProp->Inner);

				ArrayProp->Inner->ClearPropertyFlags(CPF_PropagateToArrayInner);
				if(PropDesc->bPersistentInstance)
				{
					ApplyInstancedPropertyFlags(ArrayProp, ArrayProp->Inner);
				}
				ArrayProp->Inner->SetPropertyFlags( ArrayProp->GetPropertyFlags() & CPF_PropagateToArrayInner );
			}
			else if (FMapProperty* MapProp = CastField<FMapProperty>(NewProperty))
			{
				BubbleUpInstanceReferenceFlags(MapProp, MapProp->ValueProp);
				BubbleUpInstanceReferenceFlags(MapProp, MapProp->KeyProp);

				MapProp->ValueProp->ClearPropertyFlags(CPF_PropagateToMapValue);
					
				if(PropDesc->bPersistentInstance)
				{
					ApplyInstancedPropertyFlags(MapProp, MapProp->ValueProp);
				}
				MapProp->ValueProp->SetPropertyFlags( MapProp->GetPropertyFlags() & CPF_PropagateToMapValue );

				MapProp->KeyProp->ClearPropertyFlags(CPF_PropagateToMapKey);
				if(PropDesc->bPersistentInstance)
				{
					ApplyInstancedPropertyFlags(MapProp, MapProp->KeyProp);
				}
				MapProp->KeyProp->SetPropertyFlags( MapProp->GetPropertyFlags() & CPF_PropagateToMapKey );
			}
			else if (FSetProperty* SetProp = CastField<FSetProperty>(NewProperty))
			{
				BubbleUpInstanceReferenceFlags(SetProp, SetProp->ElementProp);

				SetProp->ElementProp->ClearPropertyFlags(CPF_PropagateToSetElement);
				if(PropDesc->bPersistentInstance)
				{
					ApplyInstancedPropertyFlags(SetProp, SetProp->ElementProp);
				}
				SetProp->ElementProp->SetPropertyFlags( SetProp->GetPropertyFlags() & CPF_PropagateToSetElement );
			}
			else if (FStructProperty* StructProperty = CastField<FStructProperty>(NewProperty))
			{
				if(PropDesc->bPersistentInstance)
				{
					ApplyInstancedPropertyFlags(StructProperty, nullptr);
				}
			}
			else
			{
				BubbleUpInstanceReferenceFlags(nullptr, NewProperty);
				if (PropDesc->bPersistentInstance)
				{
					ApplyInstancedPropertyFlags(nullptr, NewProperty);
				}
			}

			// Add property to class children
			NewProperty->Next = InStruct->ChildProperties;
			InStruct->ChildProperties = NewProperty;

			// Link at the right place in the script 
			// We set the properties size so the property links correctly,
			// then override it with the proper one later. Note that none
			// of these properties take up any 'size' in unreal, because
			// they're all included in the ScriptSize of the angelscript object.
			InStruct->SetPropertiesSize(PropertyOffset);
			NewProperty->Link(ArDummy);

			// If this check fails, there is most likely an alignment difference
			// between the CPP type and the angelscript type. Set the 'alignment'
			// member of the appropriate asITypeInfo to get angelscript to align
			// its properties in accordance with unreal alignment rules.
			check(NewProperty->GetOffset_ForUFunction() == PropertyOffset);
		}
	}

	return PropertiesSize;
}

bool FAngelscriptClassGenerator::GetDataFor(int TypeId, FModuleData*& OutModule, FClassData*& OutClass, FDelegateData*& OutDelegate)
{
	asITypeInfo* ScriptType = FAngelscriptEngine::Get().Engine->GetTypeInfoById(TypeId);
	if (ScriptType == nullptr)
		return false;
	return GetDataFor(ScriptType, OutModule, OutClass, OutDelegate);
}

bool FAngelscriptClassGenerator::GetDataFor(class asITypeInfo* ScriptType, FModuleData*& OutModule, FClassData*& OutClass, FDelegateData*& OutDelegate)
{
	FDataRef* Ref = DataRefByNewScriptType.Find(ScriptType);

	if (Ref != nullptr)
	{
		FModuleData& ModuleData = Modules[Ref->ModuleIndex];
		if (Ref->bIsClass)
		{
			FClassData& ClassData = ModuleData.Classes[Ref->DataIndex];
			check(ClassData.NewClass->ScriptType == ScriptType);

			OutModule = &ModuleData;
			OutClass = &ClassData;
			OutDelegate = nullptr;

			return true;
		}
		else if (Ref->bIsDelegate)
		{
			FDelegateData& DelegateData = ModuleData.Delegates[Ref->DataIndex];
			check(DelegateData.NewDelegate->ScriptType == ScriptType);

			OutModule = &ModuleData;
			OutClass = nullptr;
			OutDelegate = &DelegateData;

			return true;
		}
	}

	return false;
}

UClass* FAngelscriptClassGenerator::ResolveCodeSuperForProperty(const FAngelscriptTypeUsage& Usage)
{
	UClass* ClassOfProperty = Usage.GetClass();
	if (ClassOfProperty != nullptr)
	{
		while (UASClass* AsClass = Cast<UASClass>(ClassOfProperty))
		{
			if (!AsClass->bIsScriptClass)
				break;

			ClassOfProperty = ClassOfProperty->GetSuperClass();
		}

		return ClassOfProperty;
	}

	if (Usage.ScriptClass != nullptr)
	{
		if (UClass* AssociatedClass = static_cast<UClass*>(Usage.ScriptClass->GetUserData()))
		{
			ClassOfProperty = AssociatedClass;
			while (UASClass* AsClass = Cast<UASClass>(ClassOfProperty))
			{
				if (!AsClass->bIsScriptClass)
					break;

				ClassOfProperty = ClassOfProperty->GetSuperClass();
			}

			if (ClassOfProperty != nullptr)
				return ClassOfProperty;
		}

		FModuleData* ModuleData = nullptr;
		FClassData* ClassData = nullptr;
		FDelegateData* DelegateData = nullptr;
		GetDataFor(Usage.ScriptClass, ModuleData, ClassData, DelegateData);

		if (ClassData != nullptr)
			return ClassData->NewClass->CodeSuperClass;
	}

	return nullptr;
}

FProperty* FAngelscriptClassGenerator::AddFunctionReturnType(UFunction* NewFunction, const FAngelscriptTypeUsage& ReturnType)
{
	auto* ASFunction = Cast<UASFunction>(NewFunction);

	// Add return property to the function if it doesn't return void
	if (ReturnType.IsValid())
	{
		FAngelscriptType::FPropertyParams Params;
		Params.Struct = nullptr;
		Params.Outer = NewFunction;
		Params.PropertyName = TEXT("ReturnValue");

		// Create FProperty for argument
		FProperty* NewProperty = ReturnType.CreateProperty(Params);
		NewProperty->SetPropertyFlags(CPF_Parm | CPF_OutParm | CPF_ReturnParm);
		//NewProperty->SetPropertyFlags(CPF_RuntimeGenerated);

		// Add property into function
		NewProperty->Next = NewFunction->ChildProperties;
		NewFunction->ChildProperties = NewProperty;

		// Store it in the function for easy lookup
		if(ASFunction != nullptr)
			ASFunction->ReturnArgument = UASFunction::FArgument{NewProperty, ReturnType};
		NewFunction->NumParms += 1;
		return NewProperty;
	}

	return nullptr;
}

FProperty* FAngelscriptClassGenerator::AddFunctionArgument(UFunction* NewFunction, const FAngelscriptArgumentDesc& ArgDesc, bool bAddToArgList)
{
	auto* ASFunction = Cast<UASFunction>(NewFunction);

	FAngelscriptType::FPropertyParams Params;
	Params.Struct = nullptr;
	Params.Outer = NewFunction;
	Params.PropertyName = *ArgDesc.ArgumentName;

	// Create FProperty for argument
	FProperty* NewProperty = ArgDesc.Type.CreateProperty(Params);
	NewProperty->SetPropertyFlags(CPF_Parm);
	//NewProperty->SetPropertyFlags(CPF_RuntimeGenerated);

	if (ArgDesc.bBlueprintOutRef)
	{
		NewProperty->SetPropertyFlags(CPF_OutParm);
		if (ArgDesc.Type.bIsConst)
			NewProperty->SetPropertyFlags(CPF_ConstParm);
	}
	else if (ArgDesc.bBlueprintInRef)
	{
		NewProperty->SetPropertyFlags(CPF_ReferenceParm);
		NewProperty->SetPropertyFlags(CPF_OutParm);
		if (ArgDesc.Type.bIsConst)
			NewProperty->SetPropertyFlags(CPF_ConstParm);
	}

#if WITH_EDITOR
	// Handle default values from angelscript
	if (ArgDesc.DefaultValue.Len() != 0)
	{
		FString UnrealDefaultValue;
		if (ArgDesc.Type.DefaultValue_AngelscriptToUnreal(ArgDesc.DefaultValue, UnrealDefaultValue))
		{
			FString DefaultValueMeta = TEXT("CPP_Default_");
			DefaultValueMeta += ArgDesc.ArgumentName;
			NewFunction->SetMetaData(*DefaultValueMeta, *UnrealDefaultValue);
		}
	}
#endif

	// Store it in the function for easy lookup
	if (ASFunction != nullptr && bAddToArgList)
		ASFunction->Arguments.Add(UASFunction::FArgument{NewProperty, ArgDesc.Type});
	NewFunction->NumParms += 1;

	return NewProperty;
}

