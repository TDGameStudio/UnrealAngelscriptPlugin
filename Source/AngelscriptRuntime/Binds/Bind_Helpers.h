#pragma once
#include "CoreMinimal.h"
#include "StructUtils/InstancedStruct.h"

#include "AngelscriptAnyStructParameter.h"

#include "StartAngelscriptHeaders.h"
//#include "as_context.h"
//#include "as_scriptfunction.h"
//#include "as_objecttype.h"
#include "source/as_generic.h"
#include "source/as_context.h"
#include "source/as_scriptengine.h"
#include "source/as_scriptfunction.h"
#include "source/as_objecttype.h"
#include "EndAngelscriptHeaders.h"

struct ANGELSCRIPTRUNTIME_API FAngelscriptBindHelpers
{
	static UClass* GetStaticClassFromClass(asCScriptFunction* Function)
	{
		return (UClass*)Function->userData;
	}

	static bool GetBoolFromProperty(void* Container, asCScriptFunction* Function)
	{
		FBoolProperty* Prop = (FBoolProperty*)Function->userData;
		return Prop->GetPropertyValue_InContainer(Container);
	}

	static void SetBoolFromProperty(void* Container, asCScriptFunction* Function, bool Value)
	{
		FBoolProperty* Prop = (FBoolProperty*)Function->userData;
		return Prop->SetPropertyValue_InContainer(Container, Value);
	}

	// Returns the correctly-offset interface pointer for a UObject that implements
	// a UInterface. For script-implemented interfaces `PointerOffset` is 0 and this
	// returns the object itself; for C++ native implementations the object embeds the
	// interface at some `PointerOffset`, and we must call `UObject::GetInterfaceAddress`
	// to obtain the true interface vtable pointer.
	//
	// This is the canonical bridge used by:
	//   - Bind_UObject.cpp opCast (interface branch) — stores a safely offset pointer
	//   - FInterfaceProperty Get/Set accessors (Phase 3) — drives `FScriptInterface::SetInterface`
	// Returns nullptr when the object doesn't implement the interface or when any
	// argument is null — callers treat nullptr as "cast failed".
	static void* GetInterfacePointerForCast(UObject* Object, UClass* InterfaceClass)
	{
		if (Object == nullptr || InterfaceClass == nullptr)
			return nullptr;
		if (!InterfaceClass->HasAnyClassFlags(CLASS_Interface))
			return nullptr;
		if (!Object->GetClass()->ImplementsInterface(InterfaceClass))
			return nullptr;

		// GetInterfaceAddress returns nullptr for Blueprint/script-only
		// implementations (where `PointerOffset == 0`); in that case fall back
		// to the UObject pointer itself so downstream consumers can dispatch
		// through reflection (FindFunction/ProcessEvent).
		void* NativeAddress = Object->GetInterfaceAddress(InterfaceClass);
		return NativeAddress != nullptr ? NativeAddress : static_cast<void*>(Object);
	}
};

struct FScriptStructType
{
	UScriptStruct* Struct = nullptr;
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptStructTypeHelpers
{
	static void Construct(FScriptStructType* Ptr)
	{
		new (Ptr) FScriptStructType();
	}

	static void CopyConstruct(FScriptStructType* Ptr, FScriptStructType& Other)
	{
		new (Ptr) FScriptStructType();
		Ptr->Struct = Other.Struct;
	}

	static UScriptStruct* GetStruct(FScriptStructType* Ptr)
	{
		return Ptr->Struct;
	}

	static bool IsValid(FScriptStructType* Ptr)
	{
		return Ptr->Struct != nullptr;
	}

	static bool OpEquals(FScriptStructType* Ptr, FScriptStructType& Other)
	{
		return Ptr->Struct == Other.Struct;
	}

	static bool OpEqualsStruct(FScriptStructType* Ptr, UScriptStruct* Other)
	{
		return Ptr->Struct == Other;
	}
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptInstancedStructHelpers
{
	static FInstancedStruct Make(void* Data, const int TypeId);
 	static void ImplicitConstructAnyStruct(FAngelscriptAnyStructParameter* Self, void* Data, const int TypeId);
	static void ImplicitConstructAnyStructFromInstancedStruct(FAngelscriptAnyStructParameter* Self, const FInstancedStruct& InstancedStruct);
	static void InitializeAs_Struct(FInstancedStruct* Self, void* Data, const int TypeId);
	static void InitializeAs_Default(FInstancedStruct* Self, UScriptStruct* StructType)
	{
		Self->InitializeAs(StructType);
	}

	static struct FScriptStructWildcard& GetMemory(FInstancedStruct* Self, const UScriptStruct* StructType);

	static bool Contains(FInstancedStruct* Self, const UScriptStruct* StructType)
	{
		if (!Self->IsValid())
			return false;

		if (StructType != Self->GetScriptStruct())
			return false;

		return true;
	}
};
