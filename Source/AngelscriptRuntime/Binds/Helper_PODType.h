#pragma once

#include "AngelscriptType.h"
#include "AngelscriptDebugValue.h"
#include "Helper_Reification.h"

#include "UObject/UnrealType.h"
#include "UObject/ScriptMacros.h"
#include "UObject/Class.h"

#include "Concepts/GetTypeHashable.h"


template<typename NativeType>
struct TAngelscriptPODType : FAngelscriptType
{
	bool CanCopy(const FAngelscriptTypeUsage& Usage) const override { return true; }
	bool NeedCopy(const FAngelscriptTypeUsage& Usage) const override { return false; }
	void CopyValue(const FAngelscriptTypeUsage& Usage, void* SourcePtr, void* DestinationPtr) const override
	{
		*(NativeType*)DestinationPtr = *(NativeType*)SourcePtr;
	}

	bool CanCompare(const FAngelscriptTypeUsage& Usage) const override { return true; }
	bool IsValueEqual(const FAngelscriptTypeUsage& Usage, void* SourcePtr, void* DestinationPtr) const override
	{
		return *(NativeType*)DestinationPtr == *(NativeType*)SourcePtr;
	}

	bool CanConstruct(const FAngelscriptTypeUsage& Usage) const override { return true; }
	bool NeedConstruct(const FAngelscriptTypeUsage& Usage) const override { return false; }

	bool CanDestruct(const FAngelscriptTypeUsage& Usage) const override { return true; }
	bool NeedDestruct(const FAngelscriptTypeUsage& Usage) const override { return false; }

	int32 GetValueSize(const FAngelscriptTypeUsage& Usage) const override
	{
		return sizeof(NativeType);
	}

	int32 GetValueAlignment(const FAngelscriptTypeUsage& Usage) const
	{
		return alignof(NativeType);
	}

	bool CanHashValue(const FAngelscriptTypeUsage& Usage) const
	{
		return TModels<CGetTypeHashable, NativeType>::Value;
	}

	uint32 GetHash(const FAngelscriptTypeUsage& Usage, const void* Address) const
	{
		if constexpr (TModels<CGetTypeHashable, NativeType>::Value)
		{
			return GetTypeHash(*(NativeType*)Address);
		}
		else
		{
			return 0;
		}
	}

	FASDebugValue* CreateDebugValue(const FAngelscriptTypeUsage& Usage, FDebugValuePrototype& Values, int32 Offset) const override
	{
		if(Usage.bIsReference)
			return Values.Create<TDebug<NativeType*>>(Offset);
		else
			return Values.Create<TDebug<NativeType>>(Offset);
	}

	int32 GetReifyType(const FAngelscriptTypeUsage& Usage) const
	{
		return ::GetReifyType<NativeType>();
	}
};

template<typename PropertyType>
struct TAngelscriptPODPropertyType : public TAngelscriptPODType<typename PropertyType::TCppType>
{
	typedef typename PropertyType::TCppType NativeType;

	bool MatchesProperty(const FAngelscriptTypeUsage& Usage, const FProperty* Property, FAngelscriptType::EPropertyMatchType MatchType) const override
	{
		return Property->IsA<PropertyType>();
	}

	bool CanCreateProperty(const FAngelscriptTypeUsage& Usage) const override { return true; }

	FProperty* CreateProperty(const FAngelscriptTypeUsage& Usage, const FAngelscriptType::FPropertyParams& Params) const override
	{
		auto* Property = new PropertyType(Params.Outer, Params.PropertyName);
		return Property;
	}

	bool CanBeArgument(const FAngelscriptTypeUsage& Usage) const override { return true; }
	void SetArgument(const FAngelscriptTypeUsage& Usage, int32 ArgumentIndex, class asIScriptContext* Context, struct FFrame& Stack, const FAngelscriptType::FArgData& Data) const override
	{
		NativeType* ValuePtr = (NativeType*)Data.StackPtr;
		if (Usage.bIsReference)
		{
			NativeType& ObjRef = Stack.StepCompiledInRef<PropertyType, NativeType>(ValuePtr);
			Context->SetArgAddress(ArgumentIndex, &ObjRef);
		}
		else
		{
			Stack.StepCompiledIn<PropertyType>(ValuePtr);
			Context->SetArgObject(ArgumentIndex, ValuePtr);
		}
	}

	bool CanBeReturned(const FAngelscriptTypeUsage& Usage) const override
	{
		return true;
	}

	void GetReturnValue(const FAngelscriptTypeUsage& Usage, class asIScriptContext* Context, void* Destination) const override
	{
		if (Usage.bIsReference)
		{
			*(NativeType**)Destination = (NativeType*)Context->GetReturnAddress();
		}
		else
		{
			void* ReturnedObject = Context->GetReturnObject();
			if (ReturnedObject == nullptr)
				return;
			*(NativeType*)Destination = *(NativeType*)ReturnedObject;
		}
	}
};