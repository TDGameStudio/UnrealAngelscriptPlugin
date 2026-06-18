#pragma once
#include "AngelscriptType.h"
#include "AngelscriptEngine.h"

#include "UObject/UObjectGlobals.h"
#include "UObject/UnrealType.h"
#include "UObject/ScriptMacros.h"

#include "Helper_CppType.h"

template<typename NativeType, typename StructureType, bool THasToString = true>
struct TAngelscriptCoreStructType : public TAngelscriptCppType<NativeType>
{
	UStruct* GetUnrealStruct(const FAngelscriptTypeUsage& Usage) const override
	{
		return StructureType::Get();
	}

	FORCEINLINE UScriptStruct* GetStruct(const FAngelscriptTypeUsage& Usage) const
	{
		return StructureType::Get();
	}

	bool CanCreateProperty(const FAngelscriptTypeUsage& Usage) const override
	{
		return true;
	}

	FProperty* CreateProperty(const FAngelscriptTypeUsage& Usage, const FAngelscriptType::FPropertyParams& Params) const override
	{
		auto* StructProp = new FStructProperty(Params.Outer, Params.PropertyName);
		StructProp->Struct = GetStruct(Usage);
		return StructProp;
	}

	bool CanBeArgument(const FAngelscriptTypeUsage& Usage) const override { return true; }
	void SetArgument(const FAngelscriptTypeUsage& Usage, int32 ArgumentIndex, class asIScriptContext* Context, struct FFrame& Stack, const FAngelscriptType::FArgData& Data) const override
	{
		NativeType* ValuePtr = (NativeType*)Data.StackPtr;
		new(ValuePtr) NativeType();

		if (Usage.bIsReference)
		{
			NativeType& ObjRef = Stack.StepCompiledInRef<FStructProperty, NativeType>(ValuePtr);
			Context->SetArgAddress(ArgumentIndex, &ObjRef);
		}
		else
		{
			Stack.StepCompiledIn<FStructProperty>(ValuePtr);
			Context->SetArgObject(ArgumentIndex, ValuePtr);
		}
	}

	bool CanBeReturned(const FAngelscriptTypeUsage& Usage) const override
	{
		return true;
	}

	void GetReturnValue(const FAngelscriptTypeUsage& Usage, class asIScriptContext* Context, void* Destination) const override
	{
		if(Usage.bIsReference)
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
		const FStructProperty* StructProp = CastField<FStructProperty>(Property);
		if (StructProp == nullptr)
			return false;
		if (StructProp->Struct != GetStruct(Usage))
			return false;
		return true;
	}

	bool GetDebuggerValue(const FAngelscriptTypeUsage& Usage, void* Address, struct FDebuggerValue& Value) const override
	{
		NativeType& NativeValue = Usage.ResolvePrimitive<NativeType>(Address);
		auto* Struct = GetStruct(Usage);

		if constexpr (THasToString)
			Value.Value = NativeValue.ToString();
		else
			Value.Value = TEXT("{}");

		Value.Type = Usage.GetAngelscriptDeclaration();
		Value.Usage = Usage;
		Value.Address = Address;
		Value.bHasMembers = Struct->PropertyLink !=  nullptr;
		return true;
	}

	bool GetDebuggerScope(const FAngelscriptTypeUsage& Usage, void* Address, struct FDebuggerScope& Scope) const override
	{
		NativeType& NativeValue = Usage.ResolvePrimitive<NativeType>(Address);
		bool bHasMembers = false;
		auto* Struct = GetStruct(Usage);

		for (TFieldIterator<FProperty> It(Struct); It; ++It)
		{
			FProperty* Property = *It;
			if (!Property->HasAnyPropertyFlags(CPF_BlueprintVisible | CPF_Edit))
				continue;

			// Can't bind static arrays. SAD!
			if (Property->ArrayDim != 1)
				continue;

			FAngelscriptTypeUsage PropUsage = FAngelscriptTypeUsage::FromProperty(Property);
			if (!PropUsage.IsValid())
				continue;

			FDebuggerValue DbgValue;
			if (PropUsage.GetDebuggerValue(Property->ContainerPtrToValuePtr<void>(&NativeValue), DbgValue, Property))
			{
				DbgValue.Name = Property->GetName();
				Scope.Values.Add(MoveTemp(DbgValue));
				bHasMembers = true;
			}
		}

		return bHasMembers;
	}

	bool GetDebuggerMember(const FAngelscriptTypeUsage& Usage, void* Address, const FString& Member, struct FDebuggerValue& Value) const override
	{
		NativeType& NativeValue = Usage.ResolvePrimitive<NativeType>(Address);
		auto* Struct = GetStruct(Usage);

		for (TFieldIterator<FProperty> It(Struct); It; ++It)
		{
			FProperty* Property = *It;
			if (!Property->HasAnyPropertyFlags(CPF_BlueprintVisible | CPF_Edit))
				continue;

			if (Property->GetName() != Member)
				continue;

			if (Property->ArrayDim != 1)
				continue;

			FAngelscriptTypeUsage PropUsage = FAngelscriptTypeUsage::FromProperty(Property);
			if (!PropUsage.IsValid())
				continue;

			if (PropUsage.GetDebuggerValue(Property->ContainerPtrToValuePtr<void>(&NativeValue), Value, Property))
				return true;
		}

		auto* ScriptType = FAngelscriptEngine::Get().GetScriptEngine()->GetTypeInfoByName(
			TCHAR_TO_ANSI(*this->GetAngelscriptTypeName(Usage))
		);
		if (ScriptType != nullptr)
		{
			FString FunctionName = TEXT("Get") + Member;
			asIScriptFunction* ScriptFunction = ScriptType->GetMethodByName(TCHAR_TO_ANSI(*FunctionName));
			if (ScriptFunction != nullptr && ScriptFunction->IsReadOnly())
			{
				if (this->GetDebuggerValueFromFunction(ScriptFunction, &NativeValue, Value, ScriptType, Struct, Member))
				{
					Value.Name = Member;
					return true;
				}
			}
		}

		return false;
	}
};

struct FGetBox
{
	static UScriptStruct* Get()
	{
		static UScriptStruct* ScriptStruct = FindObject<UScriptStruct>(nullptr, TEXT("/Script/CoreUObject.Box"));
		return ScriptStruct;
	}
};

struct FGetBoxSphereBounds
{
	static UScriptStruct* Get()
	{
		static UScriptStruct* ScriptStruct = FindObject<UScriptStruct>(nullptr, TEXT("/Script/CoreUObject.BoxSphereBounds"));
		return ScriptStruct;
	}
};

template<typename NativeType>
struct TAngelscriptBaseStructType : public TAngelscriptCoreStructType<NativeType, TBaseStructure<NativeType>>
{
};

template<typename NativeType>
struct TAngelscriptVariantStructType : public TAngelscriptCoreStructType<NativeType, TVariantStructure<NativeType>>
{
};
