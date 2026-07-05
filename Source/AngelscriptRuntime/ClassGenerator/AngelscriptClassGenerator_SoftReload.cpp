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

void FAngelscriptClassGenerator::LinkSoftReloadClasses(FModuleData& Module, FEnumData& Enum)
{
	if (!Enum.OldEnum.IsValid())
		return;
	Enum.NewEnum->Enum = Enum.OldEnum->Enum;
	Enum.NewEnum->ScriptType->SetUserData(Enum.NewEnum->Enum);
}

void FAngelscriptClassGenerator::LinkSoftReloadClasses(FModuleData& Module, FDelegateData& Delegate)
{
	if (!Delegate.OldDelegate.IsValid())
		return;
	Delegate.NewDelegate->Function = Delegate.OldDelegate->Function;
	Delegate.NewDelegate->ScriptType->SetUserData(Delegate.NewDelegate->Function);
}

void FAngelscriptClassGenerator::LinkSoftReloadClasses(FModuleData& ModuleData, FClassData& ClassData)
{
	auto ClassDesc = ClassData.NewClass;
	auto ModuleDesc = ModuleData.NewModule;

	asITypeInfo* ScriptType = GetNamespacedTypeInfoForClass(ClassData.NewClass, ModuleData.NewModule);

	if (!ClassData.NewClass->bIsStruct)
	{
		//[UE++]: Guard against invalid OldClass (can happen after DiscardModule invalidates shared state)
		if (!ClassData.OldClass.IsValid() || ClassData.OldClass->Class == nullptr)
			return;
		//[UE--]
		UASClass* Class = (UASClass*)ClassData.OldClass->Class;
		ClassDesc->Class = Class;
		if (ScriptType != nullptr)
			ScriptType->SetUserData(Class);

		SetScriptStaticClass(ClassDesc, Class);
	}
	else
	{
		UASStruct* Struct = (UASStruct*)ClassData.OldClass->Struct;
		if (ScriptType != nullptr)
			ScriptType->SetUserData(Struct);
		if (Struct != nullptr)
		{
			ClassData.NewClass->Struct = Struct;

			Struct->ScriptType = ScriptType;
			Struct->UpdateScriptType();
		}

		for (auto PropDesc : ClassDesc->Properties)
		{
			// Look up the old property so we know if we have a FProperty associated
			auto OldProperty = ClassData.OldClass->GetProperty(PropDesc->PropertyName);
			PropDesc->bHasUnrealProperty = OldProperty.IsValid() && OldProperty->bHasUnrealProperty;
		}
	}
}

void FAngelscriptClassGenerator::PrepareSoftReload(FModuleData& ModuleData, FClassData& ClassData)
{
	// Ignore any classes that don't exist yet. A queued full reload will create them later.
	if (!ClassData.OldClass.IsValid())
		return;

	UASClass* Class = (UASClass*)ClassData.OldClass->Class;

	// For soft reloads we need to allocate a CDO without any defaults, to diff to later
	extern bool GConstructASObjectWithoutDefaults;
	GConstructASObjectWithoutDefaults = true;

	UObject* CDONoDefaults;
	{
		FScopedAllowAbstractClassAllocation AllowAbstract;
		CDONoDefaults = NewObject<UObject>(GetTransientPackage(), Class,
			MakeUniqueObjectName(GetTransientPackage(), Class, *(Class->GetDefaultObjectName().ToString() + TEXT("_NoDefaults"))),
			RF_ArchetypeObject);
	}

	// Destroying the CDONoDefaults and reinitializing it makes sure we unload any Config properties in here,
	// which should be treated the same as properties modified by 'default'.
	DestructScriptObject((asCScriptObject*)CDONoDefaults, Class, (asCObjectType*)ClassData.OldClass->ScriptType);
	ReinitializeScriptObject((asCScriptObject*)CDONoDefaults, Class, (asCObjectType*)ClassData.OldClass->ScriptType);

	ClassData.CDONoDefaults = CDONoDefaults;
}

void FAngelscriptClassGenerator::DoSoftReload(FModuleData& ModuleData, FClassData& ClassData)
{
	// Ignore any classes that don't exist yet. A queued full reload will create them later.
	if (!ClassData.OldClass.IsValid())
		return;

	// Check if we've already performed the reload on this class
	if (ClassData.bReloaded)
		return;
	ClassData.bReloaded = true;

	// Log the reload if needed
	if (ClassData.OldClass.IsValid())
		UE_LOG(Angelscript, Log, TEXT("Soft Reload: %s"), *ClassData.NewClass->ClassName);

	auto ClassDesc = ClassData.NewClass;
	auto ModuleDesc = ModuleData.NewModule;

	UASClass* Class = (UASClass*)ClassData.OldClass->Class;
	UObject* CDONoDefaults = ClassData.CDONoDefaults;
	check(Class);

	// Re-create angelscript objects for all direct instances of the class
	TArray<UObject*> Instances;
	TArray<UObject*> CDOInstances;
	GetObjectsOfClass(Class, Instances, true, RF_NoFlags);

	// Soft reloads preserve defaults-code, so we shouldn't take the new defaults code yet
	ClassDesc->DefaultsCode = ClassData.OldClass->DefaultsCode;

	// Make sure our parent is soft reloaded so we don't have to re-link properties multiple times
	UClass* SuperClass = Class->GetSuperClass();
	if (auto* ParentASClass = Cast<UASClass>(SuperClass))
	{
		EnsureReloaded(ParentASClass);
	}

	// Re-link all the class' properties into the right place
	int32 PropertiesSize = Class->GetPropertiesSize();
	FArchive ArDummy;
	for (auto PropDesc : ClassDesc->Properties)
	{
		// Find FProperty for this
		FName PropName = *PropDesc->PropertyName;

		// Look up the old property so we know if we have a FProperty associated
		auto OldProperty = ClassData.OldClass->GetProperty(PropDesc->PropertyName);
		PropDesc->bHasUnrealProperty = OldProperty.IsValid() && OldProperty->bHasUnrealProperty;

		// Could be a new property that's not in the class. No big deal.
		if (!PropDesc->bHasUnrealProperty)
			continue;

		FProperty* Property = Class->FindPropertyByName(PropName);

		// Only re-link properties directly in this class
		if (Property->GetOwnerClass() != Class)
			continue;

		Class->SetPropertiesSize(PropDesc->ScriptPropertyOffset);
		Property->Link(ArDummy);
	}

	// After linking the properties, destroy the unversioned schema on the class so it refreshes
	// Properties may have changed offsets so the schema could be out of date
	//COREUOBJECT_API extern void DestroyUnversionedSchema(const UStruct* Struct);
	DestroyAngelscriptUnversionedSchema(Class);	

	// If the class has default components, update the offsets to match the new property offsets
	for (auto& DefaultComp : Class->DefaultComponents)
	{
		FProperty* Property = Class->FindPropertyByName(DefaultComp.ComponentName);
		check(Property != nullptr);

		DefaultComp.VariableOffset = Property->GetOffset_ForUFunction();
	}

	// If the class has override components, update the offsets to match the new property offsets
	for (auto& OverrideComp : Class->OverrideComponents)
	{
		FProperty* Property = Class->FindPropertyByName(OverrideComp.VariableName);
		check(Property != nullptr);

		OverrideComp.VariableOffset = Property->GetOffset_ForUFunction();
	}

	// The associated angelscript type should know which class it is so it can create objects
	asITypeInfo* OldScriptType = ClassData.OldClass->ScriptType;
	asITypeInfo* ScriptType = GetNamespacedTypeInfoForClass(ClassData.NewClass, ModuleData.NewModule);
	Class->ScriptTypePtr = ScriptType;
	Class->OwnerScriptEngine = ScriptType ? ScriptType->GetEngine() : nullptr;

	Class->SetPropertiesSize(PropertiesSize);
	if (ScriptType != nullptr)
		Class->ContainerSize = ScriptType->GetSize();

	// Update class flags so we catch when the specifiers have changed
	if(!ClassDesc->bPlaceable)
		Class->ClassFlags |= CLASS_NotPlaceable; 
	else
		Class->ClassFlags &= ~CLASS_NotPlaceable; 

	if (ClassDesc->bAbstract)
		Class->ClassFlags |= CLASS_Abstract;
	else
		Class->ClassFlags &= ~CLASS_Abstract;

	if (ClassDesc->bTransient)
		Class->ClassFlags |= CLASS_Transient;
	else if (!SuperClass->HasAnyClassFlags(CLASS_Transient))
		Class->ClassFlags &= ~CLASS_Transient;

	if (ClassDesc->bHideDropdown)
		Class->ClassFlags |= CLASS_HideDropDown;
	else
		Class->ClassFlags &= ~CLASS_HideDropDown;

	if (ClassDesc->bDefaultToInstanced)
		Class->ClassFlags |= CLASS_DefaultToInstanced;
	else if (!SuperClass->HasAnyClassFlags(CLASS_DefaultToInstanced))
		Class->ClassFlags &= ~CLASS_DefaultToInstanced;

	if (ClassDesc->bEditInlineNew)
		Class->ClassFlags |= CLASS_EditInlineNew;
	else
		Class->ClassFlags &= ~CLASS_EditInlineNew;

	if (ClassDesc->bIsDeprecatedClass)
		Class->ClassFlags |= CLASS_Deprecated;
	else if (!SuperClass->HasAnyClassFlags(CLASS_Deprecated))
		Class->ClassFlags &= ~CLASS_Deprecated;

	// Re-link all the class' functions so they point to the right script function
	for (auto FuncDesc : ClassDesc->Methods)
	{
		auto OldFuncDesc = ClassData.OldClass->GetMethod(FuncDesc->FunctionName);

		// New function, nothing to soft reload
		if (!OldFuncDesc.IsValid())
			continue;

		if (OldFuncDesc->Function != nullptr && OldFuncDesc->ScriptFunction != nullptr)
		{
			FuncDesc->Function = OldFuncDesc->Function;
			((UASFunction*)FuncDesc->Function)->ScriptFunction = FuncDesc->ScriptFunction;

			// We need to check the function's arguments and update the script types in them
			SoftReloadFunction(OldFuncDesc->Function);

#if WITH_EDITOR
			// Update the no-op flag
			if (FuncDesc->bIsNoOp != OldFuncDesc->bIsNoOp)
			{
				if (FuncDesc->bIsNoOp)
					OldFuncDesc->Function->SetMetaData(FUNCMETA_ScriptNoOp, TEXT("true"));
				else
					OldFuncDesc->Function->RemoveMetaData(FUNCMETA_ScriptNoOp);
			}
#endif
		}
	}

	// Re-assemble the class' tokens for garbage collection
	Class->AssembleReferenceTokenStream(true);

	// Record the old class in the new module
	ClassDesc->ScriptType = ScriptType;
	ClassDesc->Class = Class;

	UpdateConstructAndDefaultsFunctions(ClassDesc, Class);

	// Re-detect all angelscript properties that should be reference collected
	DetectAngelscriptReferences(ClassDesc);

	// We also need to update the script object type for all derived blueprint classes
	ForEachObjectOfClass(UBlueprintGeneratedClass::StaticClass(), [&](UObject* Obj)
	{
		UClass* CheckClass = (UClass*)Obj;
		if (!CheckClass->IsChildOf(Class))
			return;

		UASClass* ASClass = UASClass::GetFirstASClass(CheckClass);
		if (ASClass == nullptr || ASClass->ScriptTypePtr == nullptr)
			return;

		if (ASClass == Class)
		{
			// Refresh the serialization schema
			DestroyAngelscriptUnversionedSchema(CheckClass);

			// Poke the reference token stream so we update our property offsets
			CheckClass->AssembleReferenceTokenStream(true);
		}
	}, true);

#if WITH_AS_DEBUGVALUES
	CreateDebugValuePrototype(ClassDesc);
#endif

	struct FRawUnrealPropertyType : public FAngelscriptType
	{
		virtual bool CanCopy(const FAngelscriptTypeUsage& Usage) const override { return true; }
		virtual bool NeedCopy(const FAngelscriptTypeUsage& Usage) const override { return true; }
		virtual void CopyValue(const FAngelscriptTypeUsage& Usage, void* SourcePtr, void* DestinationPtr) const
		{
			Usage.UnrealProperty->CopyCompleteValue(DestinationPtr, SourcePtr);
		}

		virtual bool CanCompare(const FAngelscriptTypeUsage& Usage) const override { return true; }
		virtual bool IsValueEqual(const FAngelscriptTypeUsage& Usage, void* SourcePtr, void* DestinationPtr) const override
		{
			return Usage.UnrealProperty->Identical(SourcePtr, DestinationPtr);
		}

		virtual bool CanConstruct(const FAngelscriptTypeUsage& Usage) const override { return true; }
		virtual bool NeedConstruct(const FAngelscriptTypeUsage& Usage) const override { return true; }
		virtual void ConstructValue(const FAngelscriptTypeUsage& Usage, void* DestinationPtr) const override
		{
			Usage.UnrealProperty->InitializeValue(DestinationPtr);
		}

		virtual int32 GetValueSize(const FAngelscriptTypeUsage& Usage) const override
		{
			return Usage.UnrealProperty->GetSize();
		}

		virtual bool CanDestruct(const FAngelscriptTypeUsage& Usage) const override { return true; }
		virtual bool NeedDestruct(const FAngelscriptTypeUsage& Usage) const override { return true; }
		virtual void DestructValue(const FAngelscriptTypeUsage& Usage, void* DestinationPtr) const override
		{
			Usage.UnrealProperty->DestroyValue(DestinationPtr);
		}
	};

	// Helpers for flattening properties local to a class and expanding structs
	struct FLocalProperty
	{
		FString Name;
		FAngelscriptTypeUsage Type;
		int Offset = -1;
	};

	struct FLocalPropertyContext
	{
		int BaseOffset = 0;
		asITypeInfo* ScriptType = nullptr;
		UStruct* UnrealStruct = nullptr;
		int IgnoreBeforeOffset = 0;
		FString NamePrefix;

		static TSharedPtr<FAngelscriptType> GetRawUnrealPropertyType()
		{
			static TSharedPtr<FAngelscriptType> Type = MakeShared<FRawUnrealPropertyType>();
			return Type;
		}

		void Append(TArray<FLocalProperty>& Properties, const FString& Name, int Offset, const FAngelscriptTypeUsage& Type)
		{
			if (UStruct* InnerStruct = Type.Type->GetUnrealStruct(Type))
			{
				UScriptStruct* ScriptStruct = Cast<UScriptStruct>(InnerStruct);
				if (ScriptStruct == nullptr || (ScriptStruct->StructFlags & STRUCT_Atomic) == 0)
				{
					FLocalPropertyContext InnerContext;
					InnerContext.BaseOffset = Offset + BaseOffset;
					InnerContext.UnrealStruct = InnerStruct;
					InnerContext.NamePrefix = NamePrefix + Name + TEXT(";");

					InnerContext.Resolve(Properties);
					return;
				}
			}

			FLocalProperty LocalProp;
			LocalProp.Name = NamePrefix + Name;
			LocalProp.Type = Type;
			LocalProp.Offset = Offset + BaseOffset;
			Properties.Add(LocalProp);
		}

		void Resolve(TArray<FLocalProperty>& Properties)
		{
			if (ScriptType != nullptr)
			{
				for (int32 i = 0, PropertyCount = ScriptType->GetPropertyCount(); i < PropertyCount; ++i)
				{
					const char* Name;
					int PropertyOffset;
					int TypeId;

					ScriptType->GetProperty(i, &Name, &TypeId, nullptr, nullptr, &PropertyOffset);

					if (PropertyOffset < IgnoreBeforeOffset)
						continue;

					auto PropertyType = FAngelscriptTypeUsage::FromProperty(ScriptType, i);
					if (PropertyType.IsValid())
						Append(Properties, ANSI_TO_TCHAR(Name), PropertyOffset, PropertyType);
				}
			}

			if (UnrealStruct != nullptr)
			{
				for (TFieldIterator<FProperty> It(UnrealStruct); It; ++It)
				{
					FProperty* Property = *It;

					FAngelscriptTypeUsage Type = FAngelscriptTypeUsage::FromProperty(Property);
					if (!Type.IsValid())
					{
						Type.Type = GetRawUnrealPropertyType();
						Type.UnrealProperty = Property;
					}

					Append(Properties, Property->GetName(), Property->GetOffset_ForUFunction(), Type);
				}
			}
		}
	};


	// Detect which properties can be copied from the old instance to the new 
	struct FPropertyCopy
	{
		FString Name;
		FAngelscriptTypeUsage Type;
		SIZE_T OldOffset = 0;
		SIZE_T NewOffset = 0;
		bool bCanCompare = false;
		bool bNeedConstruct = false;
		bool bNeedDestruct = false;
		bool bIsInstanced = false;
		bool bModifiedByDefaults = false;
	};

	TMap<FString, FPropertyCopy> OldProperties;
	TArray<FPropertyCopy> PropertiesToCopy;

	if (OldScriptType != nullptr)
	{
		auto* BaseCDO = Class->GetDefaultObject();
		asCScriptObject* BaseCDOScriptObject = (asCScriptObject*)FAngelscriptEngine::UObjectToAngelscript(BaseCDO);

		FLocalPropertyContext Lookup;
		Lookup.ScriptType = OldScriptType;
		// Never copy c++ properties
		Lookup.IgnoreBeforeOffset = ClassData.OldClass->CodeSuperClass->GetPropertiesSize();

		TArray<FLocalProperty> LocalProperties;
		Lookup.Resolve(LocalProperties);

		for (const FLocalProperty& LocalProp : LocalProperties)
		{
			FAngelscriptTypeUsage PropertyType = LocalProp.Type;
			int PropertyOffset = LocalProp.Offset;

			if (PropertyType.CanCopy() && PropertyType.CanConstruct() && PropertyType.CanDestruct())
			{
				FPropertyCopy Copy;
				Copy.Name = LocalProp.Name;
				Copy.Type = PropertyType;
				Copy.OldOffset = PropertyOffset;
				Copy.bCanCompare = PropertyType.CanCompare();
				Copy.bNeedConstruct = PropertyType.NeedConstruct();
				Copy.bNeedDestruct = PropertyType.NeedDestruct();
				Copy.bIsInstanced = PropertyType.IsObjectPointer();

				// We need to determine whether the Base CDO has a different value from the
				// Base CDO without defaults. That tells us whether a default statement changes
				// this property meaning we need to copy it over regardless of the value.
				if (Copy.bCanCompare)
				{
					void* BaseCDOPtr = (void*)((SIZE_T)BaseCDO + Copy.OldOffset);
					void* CDONoDefaultsPtr = (void*)((SIZE_T)CDONoDefaults + Copy.OldOffset);
					if (!Copy.Type.IsValueEqual(BaseCDOPtr, CDONoDefaultsPtr))
					{
						Copy.bModifiedByDefaults = true;
					}
				}

				OldProperties.Add(LocalProp.Name, Copy);
			}
		}
	}

	if (ScriptType != nullptr)
	{
		FLocalPropertyContext Lookup;
		Lookup.ScriptType = ScriptType;
		// Never copy c++ properties
		Lookup.IgnoreBeforeOffset = ClassData.NewClass->CodeSuperClass->GetPropertiesSize();

		TArray<FLocalProperty> LocalProperties;
		Lookup.Resolve(LocalProperties);

		for (const FLocalProperty& LocalProp : LocalProperties)
		{
			FAngelscriptTypeUsage PropertyType = LocalProp.Type;
			int PropertyOffset = LocalProp.Offset;

			if (PropertyType.CanCopy() && PropertyType.CanConstruct() && PropertyType.CanDestruct())
			{
				// See if this property was in the old class
				FPropertyCopy* Copy = OldProperties.Find(LocalProp.Name);
				if (Copy != nullptr && Copy->Type == PropertyType)
				{
					Copy->NewOffset = PropertyOffset;
					PropertiesToCopy.Add(*Copy);
				}
			}
		}

		// Temp buffer is used to copy values to from the old script object
		// so we can copy them back in after constructing the new script object
		// in the same place in memory.
		TArray<uint8> TempBuffer;
		if (ScriptType != nullptr)
			TempBuffer.AddUninitialized(ScriptType->GetSize() + 32);

		uint8* TempData = Align(TempBuffer.GetData(), 16);

		// Temp flags whether values were stored for each property we can copy
		TArray<bool> TempShouldCopy;
		TempShouldCopy.AddUninitialized(PropertiesToCopy.Num());

		auto* BaseCDO = Class->GetDefaultObject();
		asCScriptObject* BaseCDOScriptObject = (asCScriptObject*)FAngelscriptEngine::UObjectToAngelscript(BaseCDO);

		for (UObject* Instance : Instances)
		{
			asCScriptObject* ScriptObject = (asCScriptObject*)FAngelscriptEngine::UObjectToAngelscript(Instance);

			// Re-create this only if the script object is directly our script type,
			// descendant types will be handled by that type's soft reload.
			if (ScriptObject->GetObjectType() != ScriptType)
				continue;
			if (Instance->HasAnyFlags(RF_FinishDestroyed))
				continue;

			// CDOs will be reinstanced later, so we can still check their property values
			if (Instance->HasAnyFlags(RF_ClassDefaultObject))
			{
				CDOInstances.Add(Instance);
				continue;
			}

			auto* AssociatedCDO = Instance->GetClass()->GetDefaultObject();
			asCScriptObject* CDOScriptObject = (asCScriptObject*)FAngelscriptEngine::UObjectToAngelscript(AssociatedCDO);

			// Save properties from old script object to temporary buffer where appropriate
			for (int32 i = 0, PropNum = PropertiesToCopy.Num(); i < PropNum; ++i)
			{
				auto& Copy = PropertiesToCopy[i];
				void* OriginalPtr = (void*)((SIZE_T)ScriptObject + Copy.OldOffset);
				void* CDOPtr = (void*)((SIZE_T)CDOScriptObject + Copy.OldOffset);
				void* NewPtr = (void*)((SIZE_T)TempData + Copy.NewOffset);

				// Only copy values that differ from the original CDO value, or are modified by default statements
				bool bShouldCopy = Copy.bModifiedByDefaults || !Copy.bCanCompare || !Copy.Type.IsValueEqual(CDOPtr, OriginalPtr);

				if (!bShouldCopy && AssociatedCDO != BaseCDO)
				{
					// If our CDO's value was different from the base CDO, we need
					// to copy the CDO's old value to our new instance.
					void* BaseCDOPtr = (void*)((SIZE_T)BaseCDOScriptObject + Copy.OldOffset);
					if (!Copy.Type.IsValueEqual(BaseCDOPtr, CDOPtr))
					{
						OriginalPtr = CDOPtr; // Copy CDO value
						bShouldCopy = true;
					}
				}

				TempShouldCopy[i] = bShouldCopy;
				if (bShouldCopy)
				{
					if (Copy.bNeedConstruct)
						Copy.Type.ConstructValue(NewPtr);
					Copy.Type.CopyValue(OriginalPtr, NewPtr);
				}
			}

			// Recreate the script instance
			DestructScriptObject(ScriptObject, Class, (asCObjectType*)OldScriptType);

			// Copy properties from temporary buffer to new script object at the right place
			for (int32 i = 0, PropNum = PropertiesToCopy.Num(); i < PropNum; ++i)
			{
				if (TempShouldCopy[i])
				{
					auto& Copy = PropertiesToCopy[i];
					if (!Copy.bIsInstanced)
						continue;

					void* OriginalPtr = (void*)((SIZE_T)TempData + Copy.NewOffset);
					void* NewPtr = (void*)((SIZE_T)ScriptObject + Copy.NewOffset);

					Copy.Type.CopyValue(OriginalPtr, NewPtr);
				}
			}

			ReinitializeScriptObject(ScriptObject, Class, (asCObjectType*)ScriptType);

			// Copy properties from temporary buffer to new script object at the right place
			for (int32 i = 0, PropNum = PropertiesToCopy.Num(); i < PropNum; ++i)
			{
				if (TempShouldCopy[i])
				{
					auto& Copy = PropertiesToCopy[i];

					void* OriginalPtr = (void*)((SIZE_T)TempData + Copy.NewOffset);
					void* NewPtr = (void*)((SIZE_T)ScriptObject + Copy.NewOffset);

					Copy.Type.CopyValue(OriginalPtr, NewPtr);
					if (Copy.bNeedDestruct)
						Copy.Type.DestructValue(OriginalPtr);
				}
			}
		}

		// Recreate all CDO script objects
		struct FCDOReinstanceData
		{
			TArray<uint8> TempBuffer;
			TArray<bool> TempShouldCopy;
			uint8* TempData;
		};

		TArray<FCDOReinstanceData> CDOReinstanceData;
		CDOReinstanceData.AddDefaulted(CDOInstances.Num());

		for (int32 i = 0, CDONum = CDOInstances.Num(); i < CDONum; ++i)
		{
			UObject* CDO = CDOInstances[i];
			asCScriptObject* CDOScriptObject = (asCScriptObject*)FAngelscriptEngine::UObjectToAngelscript(CDO);

			FCDOReinstanceData& Data = CDOReinstanceData[i];
			Data.TempBuffer.AddUninitialized(ScriptType->GetSize() + 32);
			Data.TempShouldCopy.AddUninitialized(PropertiesToCopy.Num());
			Data.TempData = Align(Data.TempBuffer.GetData(), 16);

			// If this is not the Base CDO of this script class, we have a parent CDO
			UObject* ParentCDO = nullptr;
			asCScriptObject* ParentCDOScriptObject = nullptr;
			if (CDO != BaseCDO)
			{
				ParentCDO = CDO->GetClass()->GetSuperClass()->GetDefaultObject();
				ParentCDOScriptObject = (asCScriptObject*)FAngelscriptEngine::UObjectToAngelscript(ParentCDO);
			}

			// Save properties from old script object to temporary buffer where appropriate
			for (int32 PropIndex = 0, PropNum = PropertiesToCopy.Num(); PropIndex < PropNum; ++PropIndex)
			{
				auto& Copy = PropertiesToCopy[PropIndex];
				void* CDOPtr = (void*)((SIZE_T)CDOScriptObject + Copy.OldOffset);
				void* SourcePtr = nullptr;

				bool bShouldCopy = false;

				if (Copy.bModifiedByDefaults)
				{
					bShouldCopy = true;
					SourcePtr = CDOPtr;
				}
				else if (ParentCDO != nullptr)
				{
					// CDO Values should be copied if this CDO is not the base CDO,
					// and then they should be copied if they differ from the parent CDO.
					void* ParentPtr = (void*)((SIZE_T)ParentCDOScriptObject + Copy.OldOffset);
					if (!Copy.bCanCompare || !Copy.Type.IsValueEqual(ParentPtr, CDOPtr))
					{
						SourcePtr = CDOPtr;
						bShouldCopy = true;
					}
					else
					{
						// If our parent's value is different from the base value,
						// we should copy it as well.
						if (ParentCDO != BaseCDO)
						{
							void* BasePtr = (void*)((SIZE_T)BaseCDOScriptObject + Copy.OldOffset);
							if (!Copy.Type.IsValueEqual(ParentPtr, BasePtr))
							{
								SourcePtr = ParentPtr;
								bShouldCopy = true;
							}
						}
					}
				}

				// Instanced properties should always be copied over to the new place first
				if (!bShouldCopy && Copy.bIsInstanced)
				{
					SourcePtr = CDOPtr;
					bShouldCopy = true;
				}

				Data.TempShouldCopy[PropIndex] = bShouldCopy;
				if (bShouldCopy)
				{
					void* DestPtr = (void*)((SIZE_T)Data.TempData + Copy.NewOffset);
					if (Copy.bNeedConstruct)
						Copy.Type.ConstructValue(DestPtr);
					Copy.Type.CopyValue(SourcePtr, DestPtr);
				}
			}
		}

		for (int32 i = 0, CDONum = CDOInstances.Num(); i < CDONum; ++i)
		{
			UObject* CDO = CDOInstances[i];
			FCDOReinstanceData& Data = CDOReinstanceData[i];

			// Actually reinstance the CDO script object
			asCScriptObject* ScriptObject = (asCScriptObject*)FAngelscriptEngine::UObjectToAngelscript(CDO);
			DestructScriptObject(ScriptObject, Class, (asCObjectType*)OldScriptType);

			// Copy properties from temporary buffer to new script object at the right place
			for (int32 PropIndex = 0, PropNum = PropertiesToCopy.Num(); PropIndex < PropNum; ++PropIndex)
			{
				if (Data.TempShouldCopy[PropIndex])
				{
					auto& Copy = PropertiesToCopy[PropIndex];
					if (!Copy.bIsInstanced)
						continue;

					void* OriginalPtr = (void*)((SIZE_T)Data.TempData + Copy.NewOffset);
					void* NewPtr = (void*)((SIZE_T)ScriptObject + Copy.NewOffset);

					Copy.Type.CopyValue(OriginalPtr, NewPtr);
				}
			}

			ReinitializeScriptObject(ScriptObject, Class, (asCObjectType*)ScriptType);

			// Copy properties from temporary buffer to new script object at the right place
			for (int32 PropIndex = 0, PropNum = PropertiesToCopy.Num(); PropIndex < PropNum; ++PropIndex)
			{
				if (Data.TempShouldCopy[PropIndex])
				{
					auto& Copy = PropertiesToCopy[PropIndex];

					void* OriginalPtr = (void*)((SIZE_T)Data.TempData + Copy.NewOffset);
					void* NewPtr = (void*)((SIZE_T)ScriptObject + Copy.NewOffset);

					Copy.Type.CopyValue(OriginalPtr, NewPtr);
					if (Copy.bNeedDestruct)
						Copy.Type.DestructValue(OriginalPtr);
				}
			}
		}
	}

	// Clean up the temporary CDO we created
	CDONoDefaults->MarkAsGarbage();
}

void FAngelscriptClassGenerator::SoftReloadFunction(UFunction* Function)
{
	auto* ASFunction = Cast<UASFunction>(Function);
	if (ASFunction == nullptr)
		return;

	for (int32 i = 0, Num = ASFunction->Arguments.Num(); i < Num; ++i)
		SoftReloadType(ASFunction->Arguments[i].Type);
	for (int32 i = 0, Num = ASFunction->DestroyArguments.Num(); i < Num; ++i)
		SoftReloadType(ASFunction->DestroyArguments[i].Type);
	
	SoftReloadType(ASFunction->ReturnArgument.Type);
}

void FAngelscriptClassGenerator::SoftReloadType(FAngelscriptTypeUsage& Usage)
{
	if (Usage.ScriptClass != nullptr)
	{
		asITypeInfo** NewType = UpdatedScriptTypeMap.Find(Usage.ScriptClass);
		if (NewType != nullptr)
			Usage.ScriptClass = *NewType;
	}

	for (auto& SubType : Usage.SubTypes)
	{
		SoftReloadType(SubType);
	}
}

