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

void FAngelscriptClassGenerator::SetScriptStaticClass(TSharedPtr<FAngelscriptClassDesc> ClassDesc, UClass* Class)
{
	if (ClassDesc->ScriptType == nullptr)
		return; // Statics classes don't exist in angelscript

	// AngelscriptPreprocessor added a global UClass variable in script
	// that it expects us to fill in with the class we generate,
	// this way, ::StaticClass() works on script classes.
	asCModule* ScriptModule = (asCModule*)ClassDesc->ScriptType->GetModule();

	asSNameSpace* ScriptNamespace = nullptr;
	if (ClassDesc->Namespace.IsSet())
		ScriptNamespace = ScriptModule->engine->FindNameSpace(TCHAR_TO_ANSI(*ClassDesc->Namespace.GetValue()));
	else
		ScriptNamespace = ScriptModule->defaultNamespace;

	asCGlobalProperty* Property = ScriptModule->scriptGlobals.FindFirst(
		TCHAR_TO_ANSI(*ClassDesc->StaticClassGlobalVariableName),
		ScriptNamespace
	);
	if (Property == nullptr)
	{
		ensure(false);
		return;
	}

	void* VarAddr = Property->GetAddressOfValue();
	**(TSubclassOf<UObject>**)VarAddr = Class;
}

void FAngelscriptClassGenerator::FinalizeClass(FModuleData& ModuleData, FClassData& ClassData)
{
	auto ClassDesc = ClassData.NewClass;
	UClass* NewClass = ClassDesc->Class;
	if (NewClass == nullptr)
		return;
	if (ClassData.bFinalized)
		return;

	ClassData.bFinalized = true;
	NewClass->SetUpRuntimeReplicationData();

	// Check if we have anything marked composable-wise
	if (!ClassDesc->ComposeOntoClass.IsEmpty())
	{
		auto ComposeOntoClassDesc = GetClassDesc(ClassDesc->ComposeOntoClass);
		if (ComposeOntoClassDesc.IsValid())
			((UASClass*)NewClass)->ComposeOntoClass = ComposeOntoClassDesc->Class;
	}

	// Add implemented interfaces to this class
	if (ClassDesc->ImplementedInterfaces.Num() > 0)
	{
		// Helper lambda: resolve an interface name to its UClass
		auto ResolveInterfaceClass = [this](const FString& InterfaceName) -> UClass*
		{
			// Check if it's another angelscript interface
			auto InterfaceDesc = GetClassDesc(InterfaceName);
			if (InterfaceDesc.IsValid() && InterfaceDesc->Class != nullptr)
			{
				// Ensure the interface class has been fully reloaded (Children chain set up)
				for (auto& CheckModule : Modules)
				{
					bool bFound = false;
					for (auto& CheckClass : CheckModule.Classes)
					{
						if (CheckClass.NewClass->ClassName == InterfaceName)
						{
							EnsureReloaded(CheckModule, CheckClass);
							bFound = true;
							break;
						}
					}
					if (bFound)
						break;
				}
				return InterfaceDesc->Class;
			}

			// Try to find it as a C++ class via the AS type system
			auto InterfaceType = FAngelscriptType::GetByAngelscriptTypeName(InterfaceName);
			if (InterfaceType.IsValid())
			{
				UClass* Found = InterfaceType->GetClass(FAngelscriptTypeUsage::DefaultUsage);
				if (Found != nullptr)
					return Found;
			}

			// Fallback: search all loaded UClasses by name
			for (TObjectIterator<UClass> It; It; ++It)
			{
				if ((It->GetName() == InterfaceName
						|| (InterfaceName.Len() >= 2
							&& InterfaceName[0] == 'U'
							&& FChar::IsUpper(InterfaceName[1])
							&& It->GetName() == InterfaceName.Mid(1)))
					&& It->HasAnyClassFlags(CLASS_Interface))
					return *It;
			}

			return nullptr;
		};

		// Helper lambda: add an interface and recursively add its base interfaces
		TSet<UClass*> AddedInterfaces;
		TFunction<void(UClass*)> AddInterfaceRecursive = [&](UClass* InterfaceClass)
		{
			if (InterfaceClass == nullptr || InterfaceClass == UInterface::StaticClass())
				return;
			if (AddedInterfaces.Contains(InterfaceClass))
				return;
			AddedInterfaces.Add(InterfaceClass);

			// First, add base interfaces (walk up the superclass chain)
			UClass* SuperInterface = InterfaceClass->GetSuperClass();
			if (SuperInterface != nullptr && SuperInterface->HasAnyClassFlags(CLASS_Interface))
			{
				AddInterfaceRecursive(SuperInterface);
			}

			// Also add any interfaces that this interface itself implements
			for (const FImplementedInterface& ParentImpl : InterfaceClass->Interfaces)
			{
				AddInterfaceRecursive(ParentImpl.Class);
			}

			// Now add this interface
			FImplementedInterface ImplementedInterface;
			ImplementedInterface.Class = InterfaceClass;
			ImplementedInterface.PointerOffset = 0;
			ImplementedInterface.bImplementedByK2 = true;
			NewClass->Interfaces.Add(ImplementedInterface);
		};

		for (const FString& InterfaceName : ClassDesc->ImplementedInterfaces)
		{
			UClass* InterfaceClass = ResolveInterfaceClass(InterfaceName);

			if (InterfaceClass != nullptr && InterfaceClass->HasAnyClassFlags(CLASS_Interface))
			{
				AddInterfaceRecursive(InterfaceClass);
			}
			else
			{
				FAngelscriptEngine::Get().ScriptCompileError(
					ModuleData.NewModule, ClassDesc->LineNumber,
					FString::Printf(TEXT("Class %s implements %s, but it is not a valid UInterface."),
					*ClassDesc->ClassName, *InterfaceName));
				ModuleData.NewModule->bModuleSwapInError = true;
			}
		}

		// Verify that the implementing class provides all methods required by its interfaces
		for (const FImplementedInterface& Impl : NewClass->Interfaces)
		{
			UClass* InterfaceClass = Impl.Class;
			if (InterfaceClass == nullptr)
				continue;

			for (TFieldIterator<UFunction> FuncIt(InterfaceClass, EFieldIteratorFlags::ExcludeSuper); FuncIt; ++FuncIt)
			{
				UFunction* InterfaceFunc = *FuncIt;
				if (InterfaceFunc->GetOuter() == UInterface::StaticClass())
					continue;

				UFunction* ImplFunc = NewClass->FindFunctionByName(InterfaceFunc->GetFName());
				const bool bResolvedToInterfaceStub = ImplFunc != nullptr
					&& ImplFunc->GetOwnerClass() != nullptr
					&& ImplFunc->GetOwnerClass()->HasAnyClassFlags(CLASS_Interface);
				if (ImplFunc == nullptr || bResolvedToInterfaceStub)
				{
					FAngelscriptEngine::Get().ScriptCompileError(
						ModuleData.NewModule, ClassDesc->LineNumber,
						FString::Printf(TEXT("Class %s implements interface %s but is missing required method '%s'."),
						*ClassDesc->ClassName, *InterfaceClass->GetName(), *InterfaceFunc->GetName()));
					ModuleData.NewModule->bModuleSwapInError = true;
				}
			}
		}
	}

	// Actors and components can have some special stuff added to them
	if (NewClass->IsChildOf(AActor::StaticClass()))
		FinalizeActorClass(ModuleData, ClassDesc);
	else if (NewClass->IsChildOf(UActorComponent::StaticClass()))
		FinalizeComponentClass(ClassDesc);
	else
		FinalizeObjectClass(ClassDesc);

	// Tell the loading system the class exists
	NotifyRegistrationEvent(TEXT("/Script/Angelscript"), *NewClass->GetName(), ENotifyRegistrationType::NRT_Class,
		ENotifyRegistrationPhase::NRP_Finished, nullptr, false, NewClass);
}

void FAngelscriptClassGenerator::FinalizeActorClass(FModuleData& ModuleData, TSharedPtr<FAngelscriptClassDesc> ClassDesc)
{
	UASClass* ASClass = (UASClass*)ClassDesc->Class;
	check(ASClass->DefaultComponents.Num() == 0);

	// Actors use a special constructor
	ClassDesc->Class->ClassConstructor = &UASClass::StaticActorConstructor;

	// Look for any DefaultComponent properties in the class so we
	// can record them and construct them on the fly.
	for (auto Property : ClassDesc->Properties)
	{
		if (Property->Meta.Contains(NAME_Actor_DefaultComponent))
		{
			UASClass::FDefaultComponent Comp;
			Comp.ComponentClass = Property->PropertyType.GetClass();
			Comp.ComponentName = *Property->PropertyName;
			Comp.VariableOffset = Property->ScriptPropertyOffset;
			Comp.bIsRoot = Property->Meta.Contains(NAME_Actor_RootComponent);
#if WITH_EDITOR
			Comp.bEditorOnly = Property->Meta.Contains(NAME_Meta_EditorOnly);
#else
			Comp.bEditorOnly = false;
			ensure(!Property->Meta.Contains(NAME_Meta_EditorOnly));
#endif

			FString* FoundAttach = Property->Meta.Find(NAME_Actor_Attach);
			if (FoundAttach != nullptr)
				Comp.Attach = **FoundAttach;
			else
				Comp.Attach = NAME_None;

			FString* FoundSocket = Property->Meta.Find(NAME_Actor_AttachSocket);
			if (FoundSocket != nullptr)
				Comp.AttachSocket = **FoundSocket;
			else
				Comp.AttachSocket = NAME_None;

			if (Comp.ComponentClass == nullptr || !Comp.ComponentClass->IsChildOf(UActorComponent::StaticClass()))
			{
				FAngelscriptEngine::Get().ScriptCompileError(
					ModuleData.NewModule, Property->LineNumber,
					FString::Printf(TEXT("Property %s::%s was marked as DefaultComponent, but is not a type of component."),
					*ClassDesc->ClassName, *Property->PropertyName));
				ModuleData.NewModule->bModuleSwapInError = true;
				continue;
			}

			if (Comp.ComponentClass->HasAnyClassFlags(CLASS_Abstract) && !ClassDesc->bAbstract)
			{
				FAngelscriptEngine::Get().ScriptCompileError(
					ModuleData.NewModule, Property->LineNumber,
					FString::Printf(TEXT("Property %s::%s was marked as DefaultComponent, but the component class is abstract and cannot be added."),
					*ClassDesc->ClassName, *Property->PropertyName));
				ModuleData.NewModule->bModuleSwapInError = true;
				continue;
			}

			if (Comp.Attach != NAME_None && !Comp.ComponentClass->IsChildOf(USceneComponent::StaticClass()))
			{
				FAngelscriptEngine::Get().ScriptCompileError(
					ModuleData.NewModule, Property->LineNumber,
					FString::Printf(TEXT("Property %s::%s has a component attach set, but is not a type of scene component."),
					*ClassDesc->ClassName, *Property->PropertyName));
				ModuleData.NewModule->bModuleSwapInError = true;
			}

			if (Comp.bIsRoot && !Comp.ComponentClass->IsChildOf(USceneComponent::StaticClass()))
			{
				FAngelscriptEngine::Get().ScriptCompileError(
					ModuleData.NewModule, Property->LineNumber,
					FString::Printf(TEXT("Property %s::%s has RootComponent set, but is not a type of scene component."),
					*ClassDesc->ClassName, *Property->PropertyName));
				ModuleData.NewModule->bModuleSwapInError = true;
			}

#if WITH_EDITOR
			// For root components, make sure no other component is marked as root component
			if (Comp.bIsRoot)
			{
				FString OtherRoot;
				auto* CheckClass = ASClass;
				while (CheckClass != nullptr && OtherRoot.IsEmpty())
				{
					for (auto& OtherComponent : CheckClass->DefaultComponents)
					{
						if (OtherComponent.bIsRoot)
						{
							OtherRoot = OtherComponent.ComponentName.ToString();
							break;
						}
					}

					CheckClass = Cast<UASClass>(CheckClass->GetSuperClass());
					if (CheckClass != nullptr)
						EnsureClassFinalized(CheckClass);
				}

				if (!OtherRoot.IsEmpty())
				{
					FAngelscriptEngine::Get().ScriptCompileError(
						ModuleData.NewModule, Property->LineNumber,
						FString::Printf(TEXT("Property %s is RootComponent, but the actor already has root component %s."),
						*Property->PropertyName, *OtherRoot));
					ModuleData.NewModule->bModuleSwapInError = true;
				}
			}
#endif

			ASClass->DefaultComponents.Add(Comp);
			ASClass->ClassFlags |= CLASS_HasInstancedReference;
		}
		else if (Property->Meta.Contains(NAME_Actor_OverrideComponent))
		{
			UASClass::FOverrideComponent Comp;
			Comp.ComponentClass = Property->PropertyType.GetClass();
			Comp.OverrideComponentName = *Property->Meta[NAME_Actor_OverrideComponent];
			Comp.VariableName = *Property->PropertyName;
			Comp.VariableOffset = Property->ScriptPropertyOffset;

			if (Comp.ComponentClass == nullptr || !Comp.ComponentClass->IsChildOf(UActorComponent::StaticClass()))
			{
				FAngelscriptEngine::Get().ScriptCompileError(
					ModuleData.NewModule, Property->LineNumber,
					FString::Printf(TEXT("Property %s::%s was marked as OverrideComponent, but is not a type of component."),
					*ClassDesc->ClassName, *Property->PropertyName));
				ModuleData.NewModule->bModuleSwapInError = true;
				continue;
			}

			if (Comp.ComponentClass->HasAnyClassFlags(CLASS_Abstract) && !ClassDesc->bAbstract)
			{
				FAngelscriptEngine::Get().ScriptCompileError(
					ModuleData.NewModule, Property->LineNumber,
					FString::Printf(TEXT("Property %s::%s was marked as OverrideComponent, but the component class is abstract and cannot be used."),
					*ClassDesc->ClassName, *Property->PropertyName));
				ModuleData.NewModule->bModuleSwapInError = true;
				continue;
			}

#if WITH_EDITOR
			UClass* ClassOfOverrideComponent = nullptr;
			auto* ParentClass = ClassDesc->Class->GetSuperClass();
			while (ParentClass != nullptr)
			{
				// Check if this component lives in a parent script class
				auto* ParentASClass = Cast<UASClass>(ParentClass);
				if (ParentASClass != nullptr)
				{
					EnsureClassFinalized(ParentASClass);

					for (const auto& DefComp : ParentASClass->DefaultComponents)
					{
						if (DefComp.ComponentName == Comp.OverrideComponentName)
						{
							ClassOfOverrideComponent = DefComp.ComponentClass;
							break;
						}
					}

					if (ClassOfOverrideComponent != nullptr)
						break;
					ParentClass = ParentClass->GetSuperClass();
					continue;
				}

				// Check if this component lives in a parent C++ class
				auto* CppCDO = Cast<AActor>(ParentClass->GetDefaultObject());
				if (CppCDO != nullptr)
				{
					for (auto* ParentComponent : CppCDO->GetComponents())
					{
						if (ParentComponent != nullptr && ParentComponent->GetFName() == Comp.OverrideComponentName)
						{
							ClassOfOverrideComponent = ParentComponent->GetClass();
							break;
						}
					}

					if (ClassOfOverrideComponent == nullptr)
					{
						// if the parent component is an abstract class it won't show up in GetComponents, 
						// so iterate over all the fields and use the property name.
						for (TFieldIterator<FProperty> It(ParentClass, EFieldIteratorFlags::ExcludeSuper); It; ++It)
						{
							FProperty* Prop = *It;
							FObjectPropertyBase* ObjectProperty = CastField<FObjectPropertyBase>(Prop);

							if (ObjectProperty && ObjectProperty->HasAnyPropertyFlags(CPF_InstancedReference))
							{
								if (ObjectProperty != nullptr && ObjectProperty->GetFName() == Comp.OverrideComponentName)
								{
									if (ObjectProperty->PropertyClass != nullptr && ObjectProperty->PropertyClass->HasAllClassFlags(CLASS_Abstract))
									{
										ClassOfOverrideComponent = ObjectProperty->PropertyClass;
										break;
									}
								}
							}
						}
					}

					if (ClassOfOverrideComponent != nullptr)
						break;
				}
				ParentClass = ParentClass->GetSuperClass();
			}

			if (ClassOfOverrideComponent == nullptr)
			{
				FAngelscriptEngine::Get().ScriptCompileError(
					ModuleData.NewModule, Property->LineNumber,
					FString::Printf(TEXT("OverrideComponent %s::%s could not find component %s in base class to override."),
					*ClassDesc->ClassName, *Property->PropertyName, *Comp.OverrideComponentName.ToString()));
				ModuleData.NewModule->bModuleSwapInError = true;
				continue;
			}
			else if (!Comp.ComponentClass->IsChildOf(ClassOfOverrideComponent))
			{
				FAngelscriptEngine::Get().ScriptCompileError(
					ModuleData.NewModule, Property->LineNumber,
					FString::Printf(TEXT("OverrideComponent %s::%s type does not inherit from the base class's %s"),
					*ClassDesc->ClassName, *Property->PropertyName, *ClassOfOverrideComponent->GetName()));
				ModuleData.NewModule->bModuleSwapInError = true;
				continue;
			}
#endif

			ASClass->OverrideComponents.Add(Comp);
			ASClass->ClassFlags |= CLASS_HasInstancedReference;
		}
	}
}

void FAngelscriptClassGenerator::FinalizeComponentClass(TSharedPtr<FAngelscriptClassDesc> ClassDesc)
{
	// Components use a special constructor
	ClassDesc->Class->ClassConstructor = &UASClass::StaticComponentConstructor;
	
#if WITH_EDITOR
	// All components are blueprint spawnable
	if(ClassDesc->bPlaceable)
		ClassDesc->Class->SetMetaData(NAME_Component_Spawnable, TEXT(""));
#endif
}

void FAngelscriptClassGenerator::FinalizeObjectClass(TSharedPtr<FAngelscriptClassDesc> ClassDesc)
{
	// Objects use a generic static constructor
	ClassDesc->Class->ClassConstructor = &UASClass::StaticObjectConstructor;
}

void FAngelscriptClassGenerator::VerifyClass(FModuleData& ModuleData, TSharedPtr<FAngelscriptClassDesc> ClassDesc)
{
#if WITH_EDITOR
	auto* ASClass = (UASClass*)ClassDesc->Class;
	if (ASClass == nullptr)
		return;

	if (ASClass->IsChildOf(AActor::StaticClass()))
	{
		// if this class isn't abstract, verify that we've provided overrides for any components that are abstract in any abstract super classes
		if (!ClassDesc->bAbstract)
		{
			// We'll need to collect all AS override components to check against as we go up the class chain
			TArray<UASClass::FOverrideComponent> OverrideComponentsInHierarchy;
			OverrideComponentsInHierarchy.Append(ASClass->OverrideComponents);

			auto* ParentClass = ClassDesc->Class->GetSuperClass();
			while (ParentClass != nullptr)
			{
				// if we've hit a non-abstract parent class, we can assume that all abstract component classes have been dealt with.
				if (!ParentClass->HasAnyClassFlags(CLASS_Abstract))
				{
					break;
				}

				auto* ParentASClass = Cast<UASClass>(ParentClass);
				if (ParentASClass)
				{
					EnsureClassFinalized(ParentASClass);
				}

				for (TFieldIterator<FProperty> It(ParentClass, EFieldIteratorFlags::ExcludeSuper); It; ++It)
				{
					FProperty* Property = *It;
					FObjectPropertyBase* ObjectProperty = CastField<FObjectPropertyBase>(Property);

					if (ObjectProperty && ObjectProperty->HasAnyPropertyFlags(CPF_InstancedReference))
					{
						UClass* PropertyClass = ObjectProperty->PropertyClass;

						if (PropertyClass && PropertyClass->HasAnyClassFlags(CLASS_Abstract))
						{
							bool bFoundOverride = false;

							for (UASClass::FOverrideComponent& OverrideComponent : OverrideComponentsInHierarchy)
							{
								if (Property->GetFName() == OverrideComponent.OverrideComponentName)
								{
									bFoundOverride = true;
									break;
								}
							}

							if (!bFoundOverride)
							{
								FAngelscriptEngine::Get().ScriptCompileError(
									ModuleData.NewModule, ClassDesc->LineNumber,
									FString::Printf(TEXT("OverrideComponent for %s::%s missing that's specificed in base class %s. Component override is needed because component class %s is Abstract."),
										*ClassDesc->ClassName, *Property->GetName(), *ParentClass->GetName(), *PropertyClass->GetName()));
								ModuleData.NewModule->bModuleSwapInError = true;
							}
						}
					}
				}

				if (ParentASClass)
				{
					// add the override components from the parent AS class to the list for us to query as we go up the chain. 
					OverrideComponentsInHierarchy.Append(ParentASClass->OverrideComponents);
				}

				ParentClass = ParentClass->GetSuperClass();
			}
		}

		// Verify that the specified attachments exist for each component that is being attached
		AActor* ActorDefaultObject = CastChecked<AActor>(ASClass->GetDefaultObject(false));
		check(ActorDefaultObject != nullptr);

		for (auto& DefComp : ASClass->DefaultComponents)
		{
			if (DefComp.Attach != NAME_None)
			{
				FObjectPropertyBase* ParentComponentProperty = nullptr;
				UActorComponent* ParentComponent = nullptr;

				UActorComponent* ChildComponent = *(UActorComponent**)((SIZE_T)ActorDefaultObject + DefComp.VariableOffset);
				if (ChildComponent == nullptr)
					continue;

				for (TFieldIterator<FProperty> It(ASClass); It; ++It)
				{
					FObjectPropertyBase* Property = CastField<FObjectPropertyBase>(*It);
					if (Property == nullptr)
						continue;
					if (!Property->HasAnyPropertyFlags(CPF_InstancedReference))
						continue;

					UActorComponent* Component = Cast<UActorComponent>(Property->GetObjectPropertyValue_InContainer(ActorDefaultObject));
					//WILL-EDIT
					//if (Property->HasAnyPropertyFlags(CPF_RuntimeGenerated))
					if (Component != nullptr)
					{
						if (Property->GetFName() == DefComp.Attach)
						{
							ParentComponentProperty = Property;
							ParentComponent = Component;
							break;
						}
					}
					//else
					//{
					//	if (Component != nullptr && Component->GetFName() == DefComp.Attach)
					//	{
					//		ParentComponentProperty = Property;
					//		ParentComponent = Component;
					//		break;
					//	}
					//}
				}

				if (ParentComponentProperty != nullptr && ParentComponent != nullptr)
				{
					if (!ParentComponent->IsA<USceneComponent>())
					{
						int LineNumber = ClassDesc->LineNumber;
						auto PropDesc = ClassDesc->GetProperty(DefComp.ComponentName.ToString());
						if (PropDesc.IsValid())
							LineNumber = PropDesc->LineNumber;

						FAngelscriptEngine::Get().ScriptCompileError(
							ModuleData.NewModule, LineNumber,
							FString::Printf(TEXT("Attach parent %s is not a SceneComponent for DefaultComponent %s."),
								*DefComp.Attach.ToString(), *DefComp.ComponentName.ToString()), true);
						ModuleData.NewModule->bModuleSwapInError = true;
					}
					else if (ParentComponentProperty->HasAnyPropertyFlags(CPF_EditorOnly) || ParentComponent->bIsEditorOnly)
					{
						auto* ChildComponentProperty = ASClass->FindPropertyByName(DefComp.ComponentName);
						if (ChildComponentProperty != nullptr && !ChildComponentProperty->HasAnyPropertyFlags(CPF_EditorOnly)
							&& !ChildComponent->bIsEditorOnly)
						{
							bool bActorIsEditorOnly = ActorDefaultObject->bIsEditorOnlyActor || ASClass->IsDeveloperOnly();
							if (!bActorIsEditorOnly)
							{
								int LineNumber = ClassDesc->LineNumber;
								auto PropDesc = ClassDesc->GetProperty(DefComp.ComponentName.ToString());
								if (PropDesc.IsValid())
									LineNumber = PropDesc->LineNumber;

								FAngelscriptEngine::Get().ScriptCompileError(
									ModuleData.NewModule, LineNumber,
									FString::Printf(TEXT("Non-Editor DefaultComponent %s cannot be attached to Editor-Only attach parent %s."),
										*DefComp.ComponentName.ToString(), *DefComp.Attach.ToString()), true);
								ModuleData.NewModule->bModuleSwapInError = true;
							}
						}
					}
				}
				else
				{
					int LineNumber = ClassDesc->LineNumber;
					auto PropDesc = ClassDesc->GetProperty(DefComp.ComponentName.ToString());
					if (PropDesc.IsValid())
						LineNumber = PropDesc->LineNumber;

					FAngelscriptEngine::Get().ScriptCompileError(
						ModuleData.NewModule, LineNumber,
						FString::Printf(TEXT("Attach parent %s does not exist for DefaultComponent %s."),
							*DefComp.Attach.ToString(), *DefComp.ComponentName.ToString()), true);
					ModuleData.NewModule->bModuleSwapInError = true;
				}
			}
			else if (DefComp.bIsRoot)
			{
				auto* ChildComponentProperty = ASClass->FindPropertyByName(DefComp.ComponentName);
				UActorComponent* ChildComponent = *(UActorComponent**)((SIZE_T)ActorDefaultObject + DefComp.VariableOffset);

				if (ChildComponentProperty != nullptr && ChildComponent != nullptr && (ChildComponentProperty->HasAnyPropertyFlags(CPF_EditorOnly) || ChildComponent->bIsEditorOnly))
				{
					bool bActorIsEditorOnly = ActorDefaultObject->bIsEditorOnlyActor || ASClass->IsDeveloperOnly();
					if (!bActorIsEditorOnly)
					{
						int LineNumber = ClassDesc->LineNumber;
						auto PropDesc = ClassDesc->GetProperty(DefComp.ComponentName.ToString());
						if (PropDesc.IsValid())
							LineNumber = PropDesc->LineNumber;

						FAngelscriptEngine::Get().ScriptCompileError(
							ModuleData.NewModule, LineNumber,
							FString::Printf(TEXT("Editor-Only DefaultComponent %s cannot be the RootComponent of non-editor actor %s."),
								*DefComp.ComponentName.ToString(), *ClassDesc->ClassName), true);
						ModuleData.NewModule->bModuleSwapInError = true;
					}
				}
			}

			// Ensure that the component isn't marked so it can't be added in angelscript
			if (DefComp.ComponentClass != nullptr && DefComp.ComponentClass->HasMetaData(CLASSMETA_NotAngelscriptSpawnable))
			{
				int LineNumber = ClassDesc->LineNumber;
				auto PropDesc = ClassDesc->GetProperty(DefComp.ComponentName.ToString());
				if (PropDesc.IsValid())
					LineNumber = PropDesc->LineNumber;

				FAngelscriptEngine::Get().ScriptCompileError(
					ModuleData.NewModule, LineNumber,
					FString::Printf(TEXT("Component type %s has NotAngelscriptSpawnable meta tag and cannot be a DefaultComponent"),
						*DefComp.ComponentClass->GetName()), true);
				ModuleData.NewModule->bModuleSwapInError = true;
			}

			// Show a warning if the component type is marked as deprecated
			if (DefComp.ComponentClass != nullptr && DefComp.ComponentClass->HasAnyClassFlags(CLASS_Deprecated))
			{
				int LineNumber = ClassDesc->LineNumber;
				auto PropDesc = ClassDesc->GetProperty(DefComp.ComponentName.ToString());
				if (PropDesc.IsValid())
					LineNumber = PropDesc->LineNumber;

				FAngelscriptEngine::Get().ScriptCompileError(
					ModuleData.NewModule, LineNumber,
					FString::Printf(TEXT("%s is deprecated"),
						*DefComp.ComponentClass->GetName()), false);
			}
		}
	}
#endif
}

void FAngelscriptClassGenerator::InitClassTickSettings(FClassData& ClassData)
{
	auto& ClassDesc = ClassData.NewClass;
	UASClass* NewClass = (UASClass*)ClassDesc->Class;
	if (NewClass == nullptr)
		return;

	if (ClassData.bHasEvalTick)
	{
		return;
	}

	if (!NewClass->IsChildOf(AActor::StaticClass()) && !NewClass->IsChildOf(UActorComponent::StaticClass()))
	{
		return;
	}

	UClass* ParentClass = NewClass->GetSuperClass();

	bool bCanEverTick = false;
	bool bStartWithTickEnabled = false;

	if (UASClass* ParentASClass = Cast<UASClass>(ParentClass))
	{
		FModuleData* ParentModuleData;
		FClassData* ParentClassData;
		FDelegateData* ParentDelegateData;
		if (GetDataFor((asITypeInfo*)ParentASClass->ScriptTypePtr, ParentModuleData, ParentClassData, ParentDelegateData))
		{
			check(ParentClassData);

			if (!ParentClassData->bHasEvalTick)
			{
				// Make sure to update parents before this
				InitClassTickSettings(*ParentClassData);
			}
		}

		bCanEverTick = ParentASClass->bCanEverTick;
		bStartWithTickEnabled = ParentASClass->bStartWithTickEnabled;
	}
	else // Parent must be C++ class
	{
		FTickFunction* ParentTickFunction;
		if (NewClass->IsChildOf(AActor::StaticClass()))
		{
			ParentTickFunction = &ParentClass->GetDefaultObject<AActor>()->PrimaryActorTick;
		}
		else
		{
			check(NewClass->IsChildOf(UActorComponent::StaticClass()));
			ParentTickFunction = &ParentClass->GetDefaultObject<UActorComponent>()->PrimaryComponentTick;
		}

		bCanEverTick = ParentTickFunction->bCanEverTick;
		bStartWithTickEnabled = ParentTickFunction->bStartWithTickEnabled;
	}

	if (!bCanEverTick)
	{
		// If the class has a ReceiveTick or a Tick function, it can tick
		auto TickDesc = ClassDesc->GetMethod(TEXT("Tick"));
		if (!TickDesc.IsValid())
			TickDesc = ClassDesc->GetMethod(TEXT("ReceiveTick"));

		if (TickDesc.IsValid() && TickDesc->ScriptFunction != nullptr && (!TickDesc->bIsNoOp || (GIsEditor && !IsRunningCommandlet())))
		{
			bCanEverTick = true;
			bStartWithTickEnabled = true;
		}
	}

	NewClass->bCanEverTick = bCanEverTick;
	NewClass->bStartWithTickEnabled = bStartWithTickEnabled;

	ClassData.bHasEvalTick = true;
}

void FAngelscriptClassGenerator::CallPostInitFunctions()
{
	// Ensure that all literal assets have been created now that we can
	for (auto& ModuleData : Modules)
	{
		if (ModuleData.NewModule->ScriptModule == nullptr)
			continue;

		for (const FString& InitFunctionName : ModuleData.NewModule->PostInitFunctions)
		{
			auto AnsiFunctionName = StringCast<ANSICHAR>(*InitFunctionName);
			asCScriptFunction* MatchedFunction = nullptr;
			for (int i = 0, Count = ModuleData.NewModule->ScriptModule->globalFunctionList.GetLength(); i < Count; ++i)
			{
				asCScriptFunction* ScriptFunction = ModuleData.NewModule->ScriptModule->globalFunctionList[i];
				if (ScriptFunction->name != AnsiFunctionName.Get())
					continue;

				// Literal-asset post-init entries point at generated property getters.
				// Prefer the property candidate when a namespaced helper shares the same short name.
				if (ScriptFunction->IsProperty())
				{
					MatchedFunction = ScriptFunction;
					break;
				}

				if (MatchedFunction == nullptr)
					MatchedFunction = ScriptFunction;
			}

			if (MatchedFunction == nullptr)
				continue;

			FAngelscriptContext Context(MatchedFunction->GetEngine());
			if (!PrepareAngelscriptContextWithLog(Context, MatchedFunction, *InitFunctionName))
			{
				continue;
			}
			Context->Execute();
		}
	}
}

void FAngelscriptClassGenerator::InitDefaultObjects()
{
	// First do a prepass in which we figure out what the tick settings should be on each ASClass.
	// This is needed to be done before we create CDOs based on AS classes, as otherwise BP CDOs can trigger CDO creation for other classes
	//		, in which the tick settings haven't been figure out yet.
	// Also need to make sure we figure it out in the correct order (class parents need to be figured out first), as class children needs to
	//		enable tick if its parent has it enabled.
	{
		for (auto& ModuleData : Modules)
		{
			for (auto& ClassData : ModuleData.Classes)
			{
				if (!ClassData.NewClass->bIsStruct)
				{
					if (ShouldFullReload(ClassData))
					{
						InitClassTickSettings(ClassData);
					}
				}
			}
		}
	}

	// Initialize default objects for all classes after reload
	for (auto& ModuleData : Modules)
	{
		for (auto& ClassData : ModuleData.Classes)
		{
			if (!ClassData.NewClass->bIsStruct)
			{
				if (ShouldFullReload(ClassData))
				{
					InitDefaultObject(ModuleData, ClassData);
				}
			}
		}
	}
}

void FAngelscriptClassGenerator::InitDefaultObject(FModuleData& ModuleData, FClassData& ClassData)
{
	auto ClassDesc = ClassData.NewClass;
	UASClass* NewClass = (UASClass*)ClassDesc->Class;
	if (NewClass == nullptr)
		return;

	NewClass->GetDefaultObject(true);
}

void FAngelscriptClassGenerator::UpdateConstructAndDefaultsFunctions(TSharedPtr<FAngelscriptClassDesc> ClassDesc, UASClass* Class)
{
	asCObjectType* ObjType = (asCObjectType*)Class->ScriptTypePtr;
	if (ObjType != nullptr)
	{
		Class->ConstructFunction = ObjType->GetEngine()->GetFunctionById(ObjType->beh.construct);
		
		// Only take the defaults function if it was overridden by our class, otherwise we're going to call the parent manually anyway
		auto* DefaultsFunction = (asCScriptFunction*)ObjType->GetMethodByDecl("void __InitDefaults()");
		if (DefaultsFunction != nullptr && DefaultsFunction->objectType == ObjType)
			Class->DefaultsFunction = DefaultsFunction;

		((asCScriptFunction*)Class->ConstructFunction)->isInUse = true;
		if (Class->DefaultsFunction != nullptr)
			((asCScriptFunction*)Class->DefaultsFunction)->isInUse = true;
	}
	else
	{
		Class->ConstructFunction = nullptr;
		Class->DefaultsFunction = nullptr;
	}
}

asITypeInfo* FAngelscriptClassGenerator::GetNamespacedTypeInfoForClass(TSharedPtr<FAngelscriptClassDesc> ClassDesc, TSharedPtr<FAngelscriptModuleDesc> ModuleDesc) const
{
	asCScriptEngine* Engine = (asCScriptEngine*) FAngelscriptEngine::Get().Engine;
	asCModule* Module = (asCModule*) ModuleDesc->ScriptModule;

	check(Engine != nullptr);
	check(Module != nullptr);

	asSNameSpace* NameSpace = nullptr;
	if (ClassDesc->Namespace.IsSet())
	{
		NameSpace = Engine->FindNameSpace(TCHAR_TO_ANSI(*ClassDesc->Namespace.GetValue()));
	}

	// Default to the modules default namespace if we couldn't find the overridden namespace.
	if (NameSpace == nullptr)
	{
		NameSpace = Module->defaultNamespace;
	}

	check(NameSpace != nullptr);

	return Module->GetType(TCHAR_TO_ANSI(*ClassDesc->ClassName), NameSpace);
}
