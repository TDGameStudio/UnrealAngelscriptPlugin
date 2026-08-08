#pragma once

#include "Helper_AngelscriptArguments.h"
#include "Helper_PODType.h"

template<typename T>
static FString FormatPrimitiveDebuggerValue(T Value)
{
	return LexToString(Value);
}

template<>
inline FString FormatPrimitiveDebuggerValue<float>(float Value)
{
	return FString::Printf(TEXT("%.9g"), static_cast<double>(Value));
}

template<>
inline FString FormatPrimitiveDebuggerValue<double>(double Value)
{
	return FString::Printf(TEXT("%.17g"), Value);
}

template<typename NativeType, typename PropertyType>
struct TPrimitiveAngelscriptType : public TAngelscriptPODType<NativeType>
{
	bool IsPrimitive() const override
	{
		return true;
	}

	FString GetAngelscriptTypeName() const override
	{
		return TEXT("");
	}

	bool CanCreateProperty(const FAngelscriptTypeUsage& Usage) const override { return true; }

	FProperty* CreateProperty(const FAngelscriptTypeUsage& Usage, const FAngelscriptType::FPropertyParams& Params) const override
	{
		auto* Property = new PropertyType(Params.Outer, Params.PropertyName);
		Property->SetPropertyFlags(CPF_HasGetValueTypeHash);
		return Property;
	}

	bool MatchesProperty(const FAngelscriptTypeUsage& Usage, const FProperty* Property, FAngelscriptType::EPropertyMatchType MatchType) const override
	{
		return Property->IsA<PropertyType>();
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
			TSetAngelscriptArgument<NativeType>(Context, ArgumentIndex, *ValuePtr);
		}
	}

	bool CanBeReturned(const FAngelscriptTypeUsage& Usage) const override
	{
		return true;
	}

	void GetReturnValue(const FAngelscriptTypeUsage& Usage, class asIScriptContext* Context, void* Destination) const override
	{
		if (Usage.bIsReference)
			*(NativeType**)Destination = (NativeType*)Context->GetReturnAddress();
		else
			*(NativeType*)Destination = TGetAngelscriptReturnValue<NativeType>(Context);
	}

	bool DefaultValue_UnrealToAngelscript(const FAngelscriptTypeUsage& Usage, const FString& UnrealValue, FString& OutAngelscriptValue) const override
	{
		if (Usage.bIsReference)
			return false;

		// Numbers are the same in both systems
		OutAngelscriptValue = UnrealValue;
		return true;
	}

	// Makes an unreal default value string from an angelscript default value
	bool DefaultValue_AngelscriptToUnreal(const FAngelscriptTypeUsage& Usage, const FString& AngelscriptValue, FString& OutUnrealValue) const override
	{
		if (Usage.bIsReference)
			return false;

		// Numbers are the same in both systems
		OutUnrealValue = AngelscriptValue;
		if (OutUnrealValue.StartsWith("- "))
			OutUnrealValue = TEXT("-") + AngelscriptValue.Mid(1).TrimStartAndEnd();

		return true;
	}

	bool GetDebuggerValue(const FAngelscriptTypeUsage& Usage, void* Address, struct FDebuggerValue& Value) const
	{
		Value.Type = Usage.GetAngelscriptDeclaration();
		Value.Usage = Usage;
		Value.Value = FormatPrimitiveDebuggerValue(Usage.ResolvePrimitive<NativeType>(Address));
		Value.Address = Address;
		Value.bHasMembers = false;
		return true;
	}

#if AS_CAN_GENERATE_JIT
	bool GetCppForm(const FAngelscriptTypeUsage& Usage, FAngelscriptType::FCppForm& OutCppForm) const override
	{
		OutCppForm.bIsPrimitive = true;
		OutCppForm.CppType = GetAngelscriptTypeName();
		return true;
	}
#endif
};

template<typename NativeType, typename PropertyType>
struct TNumericAngelscriptType : TPrimitiveAngelscriptType<NativeType, PropertyType>
{
	virtual bool IsOrdered(const FAngelscriptTypeUsage& Usage) const override
	{
		return true;
	}

	virtual int32 CompareOrder(const FAngelscriptTypeUsage& Usage, void* Value, void* OtherValue) const override
	{
		NativeType A = *(NativeType*)Value;
		NativeType B = *(NativeType*)OtherValue;
		if (A < B)
			return -1;
		else if (A == B)
			return 0;
		else
			return 1;
	}
};

template<typename NativeType, typename PropertyType>
struct TIntegralAngelscriptType : TNumericAngelscriptType<NativeType, PropertyType>
{
	bool GetStringIdentifier(const FAngelscriptTypeUsage& Usage, void* Address, FString& OutString) const override
	{
		OutString = LexToString(*(NativeType*)Address);
		return true;
	}

	bool FromStringIdentifier(const FAngelscriptTypeUsage& Usage, const FString& InString, void* BufferPtr) const
	{
		LexFromString(*(NativeType*)BufferPtr, *InString);
		return true;
	}
};

struct FIntType : TIntegralAngelscriptType<int32, FIntProperty>
{
	FString GetAngelscriptTypeName() const override;

	bool GetCppForm(const FAngelscriptTypeUsage& Usage, FCppForm& OutCppForm) const override;
};

struct FUIntType : TIntegralAngelscriptType<uint32, FUInt32Property>
{
	FString GetAngelscriptTypeName() const override;

	bool GetCppForm(const FAngelscriptTypeUsage& Usage, FCppForm& OutCppForm) const override;
};

struct FBoolType : TPrimitiveAngelscriptType<bool, FBoolProperty>
{
	FString GetAngelscriptTypeName() const override;

	FProperty* CreateProperty(const FAngelscriptTypeUsage& Usage, const FAngelscriptType::FPropertyParams& Params) const override;

	void SetArgument(const FAngelscriptTypeUsage& Usage, int32 ArgumentIndex, class asIScriptContext* Context, struct FFrame& Stack, const FArgData& Data) const override;

	bool BindProperty(const FAngelscriptTypeUsage& Usage, const FBindParams& Params, FProperty* NativeProperty) const override;

	bool GetDebuggerValue(const FAngelscriptTypeUsage& Usage, void* Address, struct FDebuggerValue& Value) const override;

	bool GetDebuggerValue(const FAngelscriptTypeUsage& Usage, void* Address, struct FDebuggerValue& Value, FProperty* NativeProperty) const override;
};

struct FFloatType : TNumericAngelscriptType<float, FFloatProperty>
{
	FString Typename;

	FFloatType(const FString& InTypename);

	FString GetAngelscriptTypeName() const override;

	FString GetAngelscriptDeclaration(const FAngelscriptTypeUsage& Usage, EAngelscriptDeclarationMode Mode) const override;

#if AS_CAN_GENERATE_JIT
	bool GetCppForm(const FAngelscriptTypeUsage& Usage, FAngelscriptType::FCppForm& OutCppForm) const override;
#endif
};
struct FDoubleType : TNumericAngelscriptType<double, FDoubleProperty>
{
	FString Typename;

	FDoubleType(const FString& InTypename);

	FString GetAngelscriptTypeName() const override;

	FString GetAngelscriptDeclaration(const FAngelscriptTypeUsage& Usage, EAngelscriptDeclarationMode Mode) const override;

#if AS_CAN_GENERATE_JIT
	bool GetCppForm(const FAngelscriptTypeUsage& Usage, FAngelscriptType::FCppForm& OutCppForm) const override;
#endif
};

struct FInt64Type : TIntegralAngelscriptType<int64, FInt64Property>
{
	virtual FString GetAngelscriptTypeName() const;
};

struct FUInt64Type : TIntegralAngelscriptType<uint64, FUInt64Property>
{
	virtual FString GetAngelscriptTypeName() const;
};

struct FInt16Type : TIntegralAngelscriptType<int16, FInt16Property>
{
	virtual FString GetAngelscriptTypeName() const;
};

struct FUInt16Type : TIntegralAngelscriptType<uint16, FUInt16Property>
{
	virtual FString GetAngelscriptTypeName() const;
};

struct FInt8Type : TIntegralAngelscriptType<int8, FInt8Property>
{
	virtual FString GetAngelscriptTypeName() const;
};

struct FUInt8Type : TIntegralAngelscriptType<uint8, FByteProperty>
{
	virtual FString GetAngelscriptTypeName() const;

	bool MatchesProperty(const FAngelscriptTypeUsage& Usage, const FProperty* Property, EPropertyMatchType MatchType) const override;
};

// Type to describe when a property (function parameterns / return values only!) is a float in unreal, but extended to a double in script
struct FUnrealFloatParamExtendedToDoubleType : TNumericAngelscriptType<float, FFloatProperty>
{
	FString Typename;

	FUnrealFloatParamExtendedToDoubleType(const FString& InTypename);

	FString GetAngelscriptTypeName() const override;

	bool GetDebuggerValue(const FAngelscriptTypeUsage& Usage, void* Address, struct FDebuggerValue& Value) const;

	bool MatchesProperty(const FAngelscriptTypeUsage& Usage, const FProperty* Property, EPropertyMatchType MatchType) const override;

	bool CanBeArgument(const FAngelscriptTypeUsage& Usage) const override;

	void SetArgument(const FAngelscriptTypeUsage& Usage, int32 ArgumentIndex, class asIScriptContext* Context, struct FFrame& Stack, const FAngelscriptType::FArgData& Data) const override;

	bool CanBeReturned(const FAngelscriptTypeUsage& Usage) const override;

	void GetReturnValue(const FAngelscriptTypeUsage& Usage, class asIScriptContext* Context, void* Destination) const override;

	int32 GetValueSize(const FAngelscriptTypeUsage& Usage) const override;

	int32 GetValueAlignment(const FAngelscriptTypeUsage& Usage) const;

	bool CanBeTemplateSubType() const override;

#if AS_CAN_GENERATE_JIT
	bool GetCppForm(const FAngelscriptTypeUsage& Usage, FAngelscriptType::FCppForm& OutCppForm) const override;
#endif
};
