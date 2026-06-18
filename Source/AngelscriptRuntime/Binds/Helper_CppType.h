#pragma once

#include "AngelscriptType.h"

#include "UObject/UnrealType.h"
#include "UObject/ScriptMacros.h"
#include "UObject/Class.h"
#include "Concepts/GetTypeHashable.h"
#include "Concepts/EqualityComparable.h"

#include "Helper_AngelscriptArguments.h"
#include "Helper_Reification.h"

template<typename CompareType, typename = void>
struct TComparer
{
	static FORCEINLINE bool DoCompare(void* SourcePtr, void* DestinationPtr);
};

template<typename CompareType>
struct TComparer<CompareType, typename TEnableIf<TModels<CEqualityComparable, CompareType>::Value>::Type>
{
	static FORCEINLINE bool DoCompare(void* SourcePtr, void* DestinationPtr)
	{
		return *(CompareType*)DestinationPtr == *(CompareType*)SourcePtr;
	}
};

template<typename CompareType>
struct TComparer<CompareType, typename TEnableIf<!TModels<CEqualityComparable, CompareType>::Value>::Type>
{
	static FORCEINLINE bool DoCompare(void* SourcePtr, void* DestinationPtr)
	{
		return false;
	}
};

template<typename NativeType>
struct TAngelscriptCppType : public FAngelscriptType
{
	bool CanCopy(const FAngelscriptTypeUsage& Usage) const override { return true; }
	bool NeedCopy(const FAngelscriptTypeUsage& Usage) const override { return true; }
	void CopyValue(const FAngelscriptTypeUsage& Usage, void* SourcePtr, void* DestinationPtr) const override
	{
		*(NativeType*)DestinationPtr = *(NativeType*)SourcePtr;
	}

	bool CanCompare(const FAngelscriptTypeUsage& Usage) const override
	{
		return TModels<CEqualityComparable, NativeType>::Value;
	}

	bool IsValueEqual(const FAngelscriptTypeUsage& Usage, void* SourcePtr, void* DestinationPtr) const override
	{
		return TComparer<NativeType>::DoCompare(SourcePtr, DestinationPtr);
	}

	bool CanConstruct(const FAngelscriptTypeUsage& Usage) const override { return true; }
	bool NeedConstruct(const FAngelscriptTypeUsage& Usage) const override { return true; }
	void ConstructValue(const FAngelscriptTypeUsage& Usage, void* DestinationPtr) const override
	{
		new(DestinationPtr) NativeType();
	}

	bool CanDestruct(const FAngelscriptTypeUsage& Usage) const override { return true; }
	bool NeedDestruct(const FAngelscriptTypeUsage& Usage) const override { return true; }
	void DestructValue(const FAngelscriptTypeUsage& Usage, void* DestinationPtr) const override
	{
		((NativeType*)DestinationPtr)->~NativeType();
	}

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
struct TAngelscriptCppPropertyType : public TAngelscriptCppType<typename PropertyType::TCppType>
{
	typedef typename PropertyType::TCppType NativeType;

	bool CanCreateProperty(const FAngelscriptTypeUsage& Usage) const override
	{
		return true;
	}

	FProperty* CreateProperty(const FAngelscriptTypeUsage& Usage, const FAngelscriptType::FPropertyParams& Params) const override
	{
		return new PropertyType(Params.Outer, Params.PropertyName);
	}

	bool CanBeArgument(const FAngelscriptTypeUsage& Usage) const override { return true; }
	void SetArgument(const FAngelscriptTypeUsage& Usage, int32 ArgumentIndex, class asIScriptContext* Context, struct FFrame& Stack, const FAngelscriptType::FArgData& Data) const override
	{
		NativeType* ValuePtr = (NativeType*)Data.StackPtr;
		new(ValuePtr) NativeType();

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

	bool MatchesProperty(const FAngelscriptTypeUsage& Usage, const FProperty* Property, FAngelscriptType::EPropertyMatchType MatchType) const override
	{
		return Property->IsA<PropertyType>();
	}
};
