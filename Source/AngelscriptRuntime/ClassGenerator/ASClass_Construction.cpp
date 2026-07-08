#include "ClassGenerator/ASFunction_CallHelpers.h"

#include "UObject/Package.h"
#include "UObject/UObjectThreadContext.h"

#include "GameFramework/Actor.h"
#include "Components/ActorComponent.h"
#include "Components/SceneComponent.h"

static TArray<FObjectInitializer> CurrentObjectInitializers;
bool GConstructASObjectWithoutDefaults = false;

UObject* UASClass::GetConstructingASObject()
{
	if (UASClass::OverrideConstructingObject != nullptr)
		return UASClass::OverrideConstructingObject;

	FUObjectThreadContext& ThreadContext = FUObjectThreadContext::Get();
	auto* Initializer = ThreadContext.TopInitializer();
	if (Initializer == nullptr)
		return nullptr;
	UObject* Object = Initializer->GetObj();
	if (Object == nullptr)
		return nullptr;

	//if (Object->GetClass()->ScriptTypePtr == nullptr)
	UASClass* asClass = UASClass::GetFirstASClass(Object);
	
	if (!asClass || asClass->ScriptTypePtr == nullptr)
		return nullptr;
	return Object;
}

static thread_local UObject* GASDefaultConstructorOuter = nullptr;
UASClass::FScopeSetDefaultConstructorOuter::FScopeSetDefaultConstructorOuter(UObject* NewOuter)
{
	PrevOuter = GASDefaultConstructorOuter;
	GASDefaultConstructorOuter = NewOuter;
}

UASClass::FScopeSetDefaultConstructorOuter::~FScopeSetDefaultConstructorOuter()
{
	GASDefaultConstructorOuter = PrevOuter;
}

UObject* UASClass::GetDefaultConstructorOuter()
{
	return GASDefaultConstructorOuter != nullptr ? GASDefaultConstructorOuter : GetTransientPackage();
}

void* UASClass::AllocScriptObject(class asITypeInfo* ScriptType, size_t Size)
{
	if (ScriptType->GetFlags() & asOBJ_VALUE)
	{
		void* Mem = FMemory::Malloc(Size, ScriptType->alignment);
		FMemory::Memzero(Mem, Size);
		return Mem;
	}

	UASClass* Class = (UASClass*)ScriptType->GetUserData();
	/*
	
		This code comes from StaticConstructObject_Internal.

		In order to split it into an allocate and a native construct part
		it has been copied and split into AllocScriptObject and FinishConstructObject.

	*/

	auto* InClass = Class;
	auto* InOuter = GetDefaultConstructorOuter();
	auto InName = NAME_None;
	EObjectFlags InFlags = RF_NoFlags;
	auto InternalSetFlags = EInternalObjectFlags::None;
	UObject* InTemplate = nullptr;
	bool bCopyTransientsFromClassDefaults = false;
	FObjectInstancingGraph* InInstanceGraph = nullptr;
	bool bAssumeTemplateIsArchetype = false;

	UObject* Result = NULL;

	if (InOuter == GetTransientPackage())
		InFlags |= RF_Transient;

	// Subobjects are always created in the constructor, no need to re-create them unless their archetype != CDO or they're blueprint generated.
	// If the existing subobject is to be re-used it can't have BeginDestroy called on it so we need to pass this information to StaticAllocateObject.	
	const bool bIsNativeClass = InClass->HasAnyClassFlags(CLASS_Native | CLASS_Intrinsic);
	const bool bIsNativeFromCDO = bIsNativeClass &&
		(
			!InTemplate || 
			(InName != NAME_None && (bAssumeTemplateIsArchetype || InTemplate == UObject::GetArchetypeFromRequiredInfo(InClass, InOuter, InName, InFlags)))
		);
	const bool bCanRecycleSubObjects = false; // No recycling, we won't know we've done it in FinishConstructObject
	bool bRecycledSubobject = false;	
	Result = StaticAllocateObject(InClass, InOuter, InName, InFlags, InternalSetFlags, bCanRecycleSubObjects, &bRecycledSubobject);
	check(Result != NULL);

	// We delay destroying the initializer until we know our constructor has been called,
	// since it's script calling the constructor instead of our ClassConstructor static function.
	FObjectInitializer& Initializer = CurrentObjectInitializers.Emplace_GetRef(
		Result, nullptr, EObjectInitializerOptions::InitializeProperties, nullptr
	);

	(*Class->ClassConstructor)( Initializer );

	return Result;
}

static FORCEINLINE_DEBUGGABLE void ExecuteDefaultsFunctions(UObject* Object, UASClass* Class)
{
	if (Class->OwnerScriptEngine == nullptr)
		return;

	UASClass* DefaultsClass = Class;
	UASClass* ParentDefaults = Cast<UASClass>(Class->GetSuperClass());

	if (ParentDefaults == nullptr)
	{
		if (Class->DefaultsFunction != nullptr)
		{
			FAngelscriptContext Context(Object, Class->DefaultsFunction->GetEngine());
			if (!PrepareAngelscriptContext(Context, Class->DefaultsFunction, *Class->GetPathName()))
				return;
			Context->m_executeVirtualCall = false;
			Context->SetObject(Object);
			Context->Execute();
		}
	}
	else
	{
		TArray<asIScriptFunction*, TFixedAllocator<32>> DefaultsFunctions;
		while (DefaultsClass != nullptr)
		{
			if (DefaultsClass->DefaultsFunction != nullptr && DefaultsClass->OwnerScriptEngine != nullptr)
				DefaultsFunctions.Add(DefaultsClass->DefaultsFunction);
			DefaultsClass = Cast<UASClass>(DefaultsClass->GetSuperClass());
		}

		for (int32 i = DefaultsFunctions.Num() - 1; i >= 0; --i)
		{
			FAngelscriptContext Context(Object, DefaultsFunctions[i]->GetEngine());
			if (!PrepareAngelscriptContext(Context, DefaultsFunctions[i], *Class->GetPathName()))
				return;
			Context->m_executeVirtualCall = false;
			Context->SetObject(Object);
			Context->Execute();
		}
	}
}

void UASClass::ApplyScriptDefaults(UObject* Object)
{
	if (Object == nullptr)
		return;

	ExecuteDefaultsFunctions(Object, this);
}

static FORCEINLINE_DEBUGGABLE void ExecuteConstructFunction(UObject* Object, UASClass* Class)
{
	if (Class->ConstructFunction != nullptr && Class->OwnerScriptEngine != nullptr)
	{
		FAngelscriptContext Context(Object, Class->ConstructFunction->GetEngine());
		if (!PrepareAngelscriptContext(Context, Class->ConstructFunction, *Class->GetPathName()))
			return;
		Context->SetObject(Object);
		Context->Execute();
	}
}

void UASClass::FinishConstructObject(class asIScriptObject* ScriptObject, class asITypeInfo* ScriptType)
{
	UObject* Object = FAngelscriptEngine::AngelscriptToUObject(ScriptObject);
	const bool bIsScriptAllocation = CurrentObjectInitializers.Num() != 0 && CurrentObjectInitializers.Last().GetObj() == Object;

	if (bIsScriptAllocation)
	{
		UASClass* TopClass = UASClass::GetFirstASClass(Object);

#if AS_CAN_HOTRELOAD
		bool bIsInTree = false;
		UASClass* CheckClass = TopClass;
		while (CheckClass != nullptr)
		{
			if (CheckClass->ScriptTypePtr == ScriptType)
			{
				bIsInTree = true;
				break;
			}
			CheckClass = Cast<UASClass>(CheckClass->GetSuperClass());
		}

		if (!bIsInTree)
		{
			CurrentObjectInitializers.RemoveAt(CurrentObjectInitializers.Num() - 1, 1, EAllowShrinking::No);
			return;
		}
#endif

		if (TopClass->ScriptTypePtr == ScriptType)
		{
			CurrentObjectInitializers.RemoveAt(CurrentObjectInitializers.Num() - 1, 1, EAllowShrinking::No);

			// Run the defaults function now that we've finished constructing the childmost script class
			ExecuteDefaultsFunctions(Object, TopClass);
		}
	}
}

static FORCEINLINE_DEBUGGABLE void ApplyOverrideComponents(const FObjectInitializer& Initializer, AActor* Actor, UASClass* ScriptClass)
{
	// Child classes should apply override components first, this is the expected order that C++ does it in
	// The object initializer understands that child overrides are applied before parents
	for(int32 i = 0, Count = ScriptClass->OverrideComponents.Num(); i < Count; ++i)
	{
		auto& Override = ScriptClass->OverrideComponents[i];

		UClass* ComponentClass = Override.ComponentClass;
#if AS_CAN_HOTRELOAD
		//ComponentClass = ComponentClass->GetMostUpToDateClass();
		UASClass* asClass = Cast<UASClass>(ComponentClass);
		if (asClass != nullptr)
			ComponentClass = asClass->GetMostUpToDateClass();
#endif

		Initializer.SetDefaultSubobjectClass(Override.OverrideComponentName, ComponentClass);
	}

	// Parent classes afterward
	if (UASClass* ParentClass = Cast<UASClass>(ScriptClass->GetSuperClass()))
	{
		ApplyOverrideComponents(Initializer, Actor, ParentClass);
	}
}
static FORCEINLINE_DEBUGGABLE void FillOverrideComponentVariablesForClass(AActor* Actor, UASClass* ScriptClass)
{
	for(int32 i = 0, Count = ScriptClass->OverrideComponents.Num(); i < Count; ++i)
	{
		auto& Override = ScriptClass->OverrideComponents[i];
		UActorComponent** VariablePtr = (UActorComponent**)((SIZE_T)Actor + Override.VariableOffset);
		UActorComponent* OverrideComponent = Cast<UActorComponent>(Actor->GetDefaultSubobjectByName(Override.OverrideComponentName));

		if (OverrideComponent == nullptr)
		{
			for (auto* CheckComponent : Actor->GetComponents())
			{
				if (CheckComponent->GetFName() == Override.OverrideComponentName)
				{
					OverrideComponent = CheckComponent;
					break;
				}
			}
		}

		if (OverrideComponent != nullptr)
			*VariablePtr = OverrideComponent;
	}
}

static FORCEINLINE_DEBUGGABLE void FillOverrideComponentVariables(AActor* Actor, UASClass* ScriptClass)
{
	if (UASClass* ParentClass = Cast<UASClass>(ScriptClass->GetSuperClass()))
		FillOverrideComponentVariables(Actor, ParentClass);

	FillOverrideComponentVariablesForClass(Actor, ScriptClass);
}


static FORCEINLINE_DEBUGGABLE void CreateDefaultComponents(const FObjectInitializer& Initializer, AActor* Actor, UASClass* ScriptClass)
{
	// Parent class should get a chance to create components first
	if (UASClass* ParentClass = Cast<UASClass>(ScriptClass->GetSuperClass()))
	{
		CreateDefaultComponents(Initializer, Actor, ParentClass);
	}

	TArray<TPair<USceneComponent*, int32>, TInlineAllocator<4>> DelayedComponentAttach;
	for(int32 i = 0, Count = ScriptClass->DefaultComponents.Num(); i < Count; ++i)
	{
		auto& DefaultComponent = ScriptClass->DefaultComponents[i];

		UActorComponent* Component;

		UClass* ComponentClass = DefaultComponent.ComponentClass;
#if AS_CAN_HOTRELOAD
		//ComponentClass = ComponentClass->GetMostUpToDateClass();
		UASClass* asClass = Cast<UASClass>(ComponentClass);
		if (asClass != nullptr)
			ComponentClass = asClass->GetMostUpToDateClass();
#endif

		if (WITH_EDITOR && DefaultComponent.bEditorOnly)
		{
			Component = (UActorComponent*)Initializer.CreateEditorOnlyDefaultSubobject(
				Actor,
				DefaultComponent.ComponentName,
				ComponentClass,
				false
			);
		}
		else
		{
			Component = (UActorComponent*)Initializer.CreateDefaultSubobject(
				Actor,
				DefaultComponent.ComponentName,
				ComponentClass,
				ComponentClass,
				true,
				false
			);
		}

		// Handle the case where we tried to create an abstract component on a non-abstract actor class,
		// this should give an error.
		AS_ENSURE(!ComponentClass->HasAnyClassFlags(CLASS_Abstract) || Actor->GetClass()->HasAnyClassFlags(CLASS_Abstract),
			TEXT("Attempted to instantiate abstract component of type %s on non-abstract actor of type %s"),
			*ComponentClass->GetName(), *Actor->GetClass()->GetName()
		);

		// Set the new component on the variable in the script class
		UActorComponent** VariablePtr = (UActorComponent**)((SIZE_T)Actor + DefaultComponent.VariableOffset);
		*VariablePtr = Component;

		// Handle attachments for scene components
		if (auto* SceneComponent = Cast<USceneComponent>(Component))
		{
			if (DefaultComponent.bIsRoot)
			{
				auto* PreviousRoot = Actor->GetRootComponent();

				// Component should become the root component
				SceneComponent->SetupAttachment(nullptr);
				Actor->SetRootComponent(SceneComponent);

				// Attach previous root component to this component
				if (PreviousRoot != nullptr)
					PreviousRoot->SetupAttachment(SceneComponent);
			}
			else if (DefaultComponent.Attach == NAME_None)
			{
				if (Actor->GetRootComponent() == nullptr)
				{
					// Component should become the root component, since we don't have any
					SceneComponent->SetupAttachment(nullptr);
					Actor->SetRootComponent(SceneComponent);
				}
				else
				{
					// Component should automatically be attached to the existing root component
					SceneComponent->SetupAttachment(Actor->GetRootComponent(), DefaultComponent.AttachSocket);
				}
			}
			else
			{
				// Attach the component later, when all components have been created
				DelayedComponentAttach.Add( TPair<USceneComponent*, int32>{ SceneComponent, i } );
			}
		}
	}

	for (auto& DelayedAttach : DelayedComponentAttach)
	{
		auto& DefaultComponent = ScriptClass->DefaultComponents[DelayedAttach.Value];

		// Find the component to attach to
		USceneComponent* AttachTo = nullptr;
		for (auto* CheckComponent : Actor->GetComponents())
		{
			if (CheckComponent->GetFName() == DefaultComponent.Attach)
			{
				if (auto* CheckSceneComponent = Cast<USceneComponent>(CheckComponent))
				{
					AttachTo = CheckSceneComponent;
					break;
				}
			}
		}

		// If we can't find the thing to attach to, attach to the root instead
		if (AttachTo == nullptr)
		{
			if (Actor->GetRootComponent() != nullptr)
			{
				DelayedAttach.Key->SetupAttachment(Actor->GetRootComponent(), DefaultComponent.AttachSocket);
			}
			else
			{
				DelayedAttach.Key->SetupAttachment(nullptr);
				Actor->SetRootComponent(DelayedAttach.Key);
			}
		}
		else
		{
			DelayedAttach.Key->SetupAttachment(AttachTo, DefaultComponent.AttachSocket);
		}
	}

	// Fill any override component variables with the right components
	FillOverrideComponentVariablesForClass(Actor, ScriptClass);
}

//WILL-EDIT



void UASClass::StaticActorConstructor(const FObjectInitializer& Initializer)
{
	UObject* Object = Initializer.GetObj();
	UASClass* Class = GetFirstASClass(Object);
	asCObjectType* ScriptType = (asCObjectType*)Class->ScriptTypePtr;
	AActor* Actor = (AActor*)Object;

#if AS_CAN_HOTRELOAD
	const bool bApplyDefaults = !GConstructASObjectWithoutDefaults;
	GConstructASObjectWithoutDefaults = false;
#else
	const bool bApplyDefaults = true;
#endif

	// Apply override components
	ApplyOverrideComponents(Initializer, Actor, Class);

	// We need to run the C++ constructor first so everything is valid
	Class->CodeSuperClass->ClassConstructor(Initializer);
	Actor->PrimaryActorTick.bCanEverTick = Class->bCanEverTick;
	Actor->PrimaryActorTick.bStartWithTickEnabled = Class->bStartWithTickEnabled;

	// Construct the C++ part of the angelscript scriptobject
	const bool bIsScriptAllocation = CurrentObjectInitializers.Num() != 0 && CurrentObjectInitializers.Last().GetObj() == Object;

	if (!bIsScriptAllocation && ScriptType != nullptr)
		new(Object) asCScriptObject(ScriptType);

#if WITH_AS_DEBUGVALUES
	// Init the object's debug value
	Object->Debug = Class->DebugValues.Instantiate(Object);
#endif

	//WILL-EDIT

	/*
	Right now I have two options it seems:
	1) Ensure all AS classes inherit from our own custom base so that events can be called
	2) Make one Custom component that we add to an actor that will call the actor and component
	events in place of the base.
	However neither of these are particularly good for firing off a BeginPlay with an actor that spawns
	later
	*/

	// Construct any default components we have marked in our hierarchy
	CreateDefaultComponents(Initializer, Actor, Class);

	// Call the script constructor function
	if (!bIsScriptAllocation)
		ExecuteConstructFunction(Object, Class);

	// Apply any default statements to the object
	if (bApplyDefaults && !bIsScriptAllocation)
		ExecuteDefaultsFunctions(Object, Class);

	if (!bIsScriptAllocation)
		const_cast<FObjectInitializer&>(Initializer).AddPropertyPostInitCallback([Actor, Class]() { FillOverrideComponentVariables(Actor, Class); });
}

void UASClass::StaticComponentConstructor(const FObjectInitializer& Initializer)
{
	UObject* Object = Initializer.GetObj();
	UASClass* Class = GetFirstASClass(Object);
	asCObjectType* ScriptType = (asCObjectType*)Class->ScriptTypePtr;
	UActorComponent* Component = (UActorComponent*)Object;

#if AS_CAN_HOTRELOAD
	const bool bApplyDefaults = !GConstructASObjectWithoutDefaults;
	GConstructASObjectWithoutDefaults = false;
#else
	const bool bApplyDefaults = true;
#endif

	// We need to run the C++ constructor first so everything is valid
	Class->CodeSuperClass->ClassConstructor(Initializer);
	Component->PrimaryComponentTick.bCanEverTick = Class->bCanEverTick;
	Component->PrimaryComponentTick.bStartWithTickEnabled = Class->bStartWithTickEnabled;

	// Construct the C++ part of the angelscript scriptobject
	const bool bIsScriptAllocation = CurrentObjectInitializers.Num() != 0 && CurrentObjectInitializers.Last().GetObj() == Object;

	if (!bIsScriptAllocation && ScriptType != nullptr)
		new(Object) asCScriptObject(ScriptType);

#if WITH_AS_DEBUGVALUES
	// Init the object's debug value
	Object->Debug = Class->DebugValues.Instantiate(Object);
#endif

	// Call the script constructor function
	if (!bIsScriptAllocation)
		ExecuteConstructFunction(Object, Class);

	// Apply any default statements to the object
	if (bApplyDefaults && !bIsScriptAllocation)
		ExecuteDefaultsFunctions(Object, Class);
}

void UASClass::StaticObjectConstructor(const FObjectInitializer& Initializer)
{
	UObject* Object = Initializer.GetObj();
	UASClass* Class = GetFirstASClass(Object);
	asCObjectType* ScriptType = (asCObjectType*)Class->ScriptTypePtr;

#if AS_CAN_HOTRELOAD
	const bool bApplyDefaults = !GConstructASObjectWithoutDefaults;
	GConstructASObjectWithoutDefaults = false;
#else
	const bool bApplyDefaults = true;
#endif

	// We need to run the C++ constructor first so everything is valid
	Class->CodeSuperClass->ClassConstructor(Initializer);

#if WITH_AS_DEBUGVALUES
	// Init the object's debug value
	Object->Debug = Class->DebugValues.Instantiate(Object);
#endif

	// Construct the C++ part of the angelscript scriptobject
	const bool bIsScriptAllocation = CurrentObjectInitializers.Num() != 0 && CurrentObjectInitializers.Last().GetObj() == Object;

	if (!bIsScriptAllocation && ScriptType != nullptr)
		new(Object) asCScriptObject(ScriptType);

	// Call the script constructor function
	if (!bIsScriptAllocation)
		ExecuteConstructFunction(Object, Class);

	// Apply any default statements to the object
	if (bApplyDefaults && !bIsScriptAllocation)
		ExecuteDefaultsFunctions(Object, Class);
}
