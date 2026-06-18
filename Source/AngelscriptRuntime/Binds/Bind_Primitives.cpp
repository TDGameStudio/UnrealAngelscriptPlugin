#include "AngelscriptBinds.h"
#include "AngelscriptEngine.h"
#include "AngelscriptSettings.h"
#include "AngelscriptDocs.h"
#include "AngelscriptType.h"

#include "Helper_AngelscriptArguments.h"
#include "Helper_ToString.h"
#include "Binds/Bind_Helpers.h"
#include "Binds/Helper_PODType.h"

static const FName NAME_Property_ToolTip("ToolTip");

template<typename T>
static FString FormatPrimitiveDebuggerValue(T Value)
{
	return LexToString(Value);
}

template<>
FString FormatPrimitiveDebuggerValue<float>(float Value)
{
	return FString::Printf(TEXT("%.9g"), static_cast<double>(Value));
}

template<>
FString FormatPrimitiveDebuggerValue<double>(double Value)
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
	FString GetAngelscriptTypeName() const override
	{
		return TEXT("int");
	}

	bool GetCppForm(const FAngelscriptTypeUsage& Usage, FCppForm& OutCppForm) const override
	{
		OutCppForm.bIsPrimitive = true;
		OutCppForm.CppType = TEXT("int32");
		return true;
	}
};

struct FUIntType : TIntegralAngelscriptType<uint32, FUInt32Property>
{
	FString GetAngelscriptTypeName() const override
	{
		return TEXT("uint");
	}

	bool GetCppForm(const FAngelscriptTypeUsage& Usage, FCppForm& OutCppForm) const override
	{
		OutCppForm.bIsPrimitive = true;
		OutCppForm.CppType = TEXT("uint32");
		return true;
	}
};

static const FName NAME_Property_Bool_DeprecatedProperty("DeprecatedProperty");
static const FName NAME_Property_Bool_DeprecationMessage("DeprecationMessage");
struct FBoolType : TPrimitiveAngelscriptType<bool, FBoolProperty>
{
	FString GetAngelscriptTypeName() const override
	{
		return TEXT("bool");
	}

	FProperty* CreateProperty(const FAngelscriptTypeUsage& Usage, const FAngelscriptType::FPropertyParams& Params) const override
	{
		auto* Property = new FBoolProperty(Params.Outer, Params.PropertyName);
		Property->SetPropertyFlags(CPF_HasGetValueTypeHash);
		Property->SetBoolSize(1, true, 255);
		return Property;
	}

	void SetArgument(const FAngelscriptTypeUsage& Usage, int32 ArgumentIndex, class asIScriptContext* Context, struct FFrame& Stack, const FArgData& Data) const override
	{
		bool* ValuePtr = (bool*)Data.StackPtr;

		// We need to clear the bool here, because our stack memory isn't necessarily
		// cleared, and bool properties only copy 1 bit, not the full byte for some reason.
		*ValuePtr = false;

		if (Usage.bIsReference)
		{
			bool& ObjRef = Stack.StepCompiledInRef<FBoolProperty, bool>(ValuePtr);
			Context->SetArgAddress(ArgumentIndex, &ObjRef);
		}
		else
		{
			Stack.StepCompiledIn<FBoolProperty>(ValuePtr);
			TSetAngelscriptArgument<bool>(Context, ArgumentIndex, *ValuePtr);
		}
	}

	bool BindProperty(const FAngelscriptTypeUsage& Usage, const FBindParams& Params, FProperty* NativeProperty) const override
	{
		// We need to do some special stuff for bool properties with bitmasks
		FBoolProperty* BoolProp = CastField<FBoolProperty>(NativeProperty);
		if (!ensure(BoolProp != nullptr))
			return false;
		if (BoolProp->IsNativeBool())
			return false;

		FString PropName;
		if (Params.NameOverride.Len() != 0)
			PropName = Params.NameOverride;
		else
			PropName = NativeProperty->GetName();

#if WITH_EDITOR
		bool bIsDeprecated = BoolProp->HasMetaData(NAME_Property_Bool_DeprecatedProperty);
		FString DeprecationMessage;
		if (bIsDeprecated)
			DeprecationMessage = BoolProp->GetMetaData(NAME_Property_Bool_DeprecationMessage);
#endif

		auto& BindClass = *Params.BindClass;
		if (Params.bCanRead && !BindClass.HasGetter(NativeProperty->GetName()))
		{
			FString Decl = FString::Printf(TEXT("bool Get%s() const"), *PropName);
			BindClass.Method(Decl, FUNC_TRIVIAL(FAngelscriptBindHelpers::GetBoolFromProperty), (void*)NativeProperty);
			FAngelscriptBinds::PreviousBindPassScriptFunctionAsFirstParam();

#if WITH_EDITOR
			if (bIsDeprecated)
				BindClass.DeprecatePreviousBind(TCHAR_TO_ANSI(*DeprecationMessage));

			BindClass.SetPreviousBindIsGeneratedAccessor(true);

			const FString& Tooltip = BoolProp->GetMetaData(NAME_Property_ToolTip);
			if (Tooltip.Len() != 0)
				FAngelscriptDocs::AddUnrealDocumentation(FAngelscriptBinds::GetPreviousFunctionId(), Tooltip, TEXT(""), nullptr);

			if (BoolProp->HasAnyPropertyFlags(CPF_EditorOnly))
				BindClass.SetPreviousBindIsEditorOnly(true);
#endif
		}

		if ((Params.bCanWrite || Params.bCanEdit) && !BindClass.HasSetter(NativeProperty->GetName()))
		{
			FString Decl = FString::Printf(TEXT("void Set%s(bool Value)"), *PropName);
			BindClass.Method(Decl, FUNC_TRIVIAL(FAngelscriptBindHelpers::SetBoolFromProperty), Params, (void*)NativeProperty);
			FAngelscriptBinds::PreviousBindPassScriptFunctionAsFirstParam();

#if WITH_EDITOR
			if (bIsDeprecated)
				BindClass.DeprecatePreviousBind(TCHAR_TO_ANSI(*DeprecationMessage));

			BindClass.SetPreviousBindIsGeneratedAccessor(true);

			const FString& Tooltip = BoolProp->GetMetaData(NAME_Property_ToolTip);
			if (Tooltip.Len() != 0)
				FAngelscriptDocs::AddUnrealDocumentation(FAngelscriptBinds::GetPreviousFunctionId(), Tooltip, TEXT(""), nullptr);

			if (BoolProp->HasAnyPropertyFlags(CPF_EditorOnly))
				BindClass.SetPreviousBindIsEditorOnly(true);
#endif
		}

		return true;
	}

	bool GetDebuggerValue(const FAngelscriptTypeUsage& Usage, void* Address, struct FDebuggerValue& Value) const override
	{
		Value.Type = Usage.GetAngelscriptDeclaration();
		Value.Usage = Usage;
		Value.Value = LexToString(Usage.ResolvePrimitive<bool>(Address));
		Value.Address = Address;
		Value.bHasMembers = false;
		return true;
	}

	bool GetDebuggerValue(const FAngelscriptTypeUsage& Usage, void* Address, struct FDebuggerValue& Value, FProperty* NativeProperty) const override
	{
		FBoolProperty* BoolProp = CastField<FBoolProperty>(NativeProperty);
		if (!ensure(BoolProp != nullptr))
			return false;
		if (BoolProp->IsNativeBool())
			return GetDebuggerValue(Usage, Address, Value);

		uint8* MemPtr = &Usage.ResolvePrimitive<uint8>(Address);
		bool BoolValue = BoolProp->GetPropertyValue(MemPtr);

		Value.Type = Usage.GetAngelscriptDeclaration();
		Value.Usage = Usage;
		Value.Value = LexToString(BoolValue);
		Value.Address = Address;
		Value.bHasMembers = false;
		return true;
	}
};

struct FFloatType : TNumericAngelscriptType<float, FFloatProperty>
{
	FString Typename;
	FFloatType(const FString& InTypename) : Typename(InTypename) {}

	FString GetAngelscriptTypeName() const override
	{
		return Typename;
	}

	FString GetAngelscriptDeclaration(const FAngelscriptTypeUsage& Usage, EAngelscriptDeclarationMode Mode) const override
	{
		switch (Mode)
		{
		case EAngelscriptDeclarationMode::MemberVariable:
		case EAngelscriptDeclarationMode::MemberVariable_InContainer:
		case EAngelscriptDeclarationMode::FunctionArgument:
		case EAngelscriptDeclarationMode::FunctionReturnValue:
			return TEXT("float32");
		break;
		}

		return Typename;
	}

#if AS_CAN_GENERATE_JIT
	bool GetCppForm(const FAngelscriptTypeUsage& Usage, FAngelscriptType::FCppForm& OutCppForm) const override
	{
		OutCppForm.bIsPrimitive = true;
		OutCppForm.CppType = TEXT("float");
		return true;
	}
#endif
};

struct FDoubleType : TNumericAngelscriptType<double, FDoubleProperty>
{
	FString Typename;
	FDoubleType(const FString& InTypename) : Typename(InTypename) {}

	FString GetAngelscriptTypeName() const override
	{
		return Typename;
	}

	FString GetAngelscriptDeclaration(const FAngelscriptTypeUsage& Usage, EAngelscriptDeclarationMode Mode) const override
	{
		switch (Mode)
		{
		case EAngelscriptDeclarationMode::MemberVariable:
		case EAngelscriptDeclarationMode::MemberVariable_InContainer:
		case EAngelscriptDeclarationMode::FunctionArgument:
		case EAngelscriptDeclarationMode::FunctionReturnValue:
			return TEXT("float64");
		break;
		}

		return Typename;
	}

#if AS_CAN_GENERATE_JIT
	bool GetCppForm(const FAngelscriptTypeUsage& Usage, FAngelscriptType::FCppForm& OutCppForm) const override
	{
		OutCppForm.bIsPrimitive = true;
		OutCppForm.CppType = TEXT("double");
		return true;
	}
#endif
};

struct FInt64Type : TIntegralAngelscriptType<int64, FInt64Property>
{
	virtual FString GetAngelscriptTypeName() const
	{
		return TEXT("int64");
	}
};

struct FUInt64Type : TIntegralAngelscriptType<uint64, FUInt64Property>
{
	virtual FString GetAngelscriptTypeName() const
	{
		return TEXT("uint64");
	}
};

struct FInt16Type : TIntegralAngelscriptType<int16, FInt16Property>
{
	virtual FString GetAngelscriptTypeName() const
	{
		return TEXT("int16");
	}
};

struct FUInt16Type : TIntegralAngelscriptType<uint16, FUInt16Property>
{
	virtual FString GetAngelscriptTypeName() const
	{
		return TEXT("uint16");
	}
};

struct FInt8Type : TIntegralAngelscriptType<int8, FInt8Property>
{
	virtual FString GetAngelscriptTypeName() const
	{
		return TEXT("int8");
	}
};

struct FUInt8Type : TIntegralAngelscriptType<uint8, FByteProperty>
{
	virtual FString GetAngelscriptTypeName() const
	{
		return TEXT("uint8");
	}

	bool MatchesProperty(const FAngelscriptTypeUsage& Usage, const FProperty* Property, EPropertyMatchType MatchType) const override
	{
		if (auto* ByteProp = CastField<FByteProperty>(Property))
		{
			if (ByteProp->Enum == nullptr)
				return true;
		}

		return false;
	}
};

// Type to describe when a property (function parameterns / return values only!) is a float in unreal, but extended to a double in script
struct FUnrealFloatParamExtendedToDoubleType : TNumericAngelscriptType<float, FFloatProperty>
{
	FString Typename;
	FUnrealFloatParamExtendedToDoubleType(const FString& InTypename) : Typename(InTypename) {}

	FString GetAngelscriptTypeName() const override
	{
		return Typename;
	}

	bool GetDebuggerValue(const FAngelscriptTypeUsage& Usage, void* Address, struct FDebuggerValue& Value) const
	{
		Value.Usage = FAngelscriptType::ScriptDoubleType();
		Value.Type = Value.Usage.GetAngelscriptDeclaration();
		Value.Value = LexToString(Usage.ResolvePrimitive<double>(Address));
		Value.Address = Address;
		Value.bHasMembers = false;
		return true;
	}

	bool MatchesProperty(const FAngelscriptTypeUsage& Usage, const FProperty* Property, EPropertyMatchType MatchType) const override
	{
		// A BlueprintEvent in C++ that has a float return or argument can be overridden with a double in script
		if (!Usage.bIsReference)
		{
			if (MatchType == EPropertyMatchType::OverrideArgument || MatchType == EPropertyMatchType::OverrideReturnValue)
			{
				if (Property->IsA<FFloatProperty>())
					return true;
			}
		}

		return false;
	}

	bool CanBeArgument(const FAngelscriptTypeUsage& Usage) const override { return true; }
	void SetArgument(const FAngelscriptTypeUsage& Usage, int32 ArgumentIndex, class asIScriptContext* Context, struct FFrame& Stack, const FAngelscriptType::FArgData& Data) const override
	{
		check(!Usage.bIsReference);

		float* ValuePtr = (float*)Data.StackPtr;
		Stack.StepCompiledIn<FFloatProperty>(ValuePtr);

		TSetAngelscriptArgument<double>(Context, ArgumentIndex, (double)*ValuePtr);
	}

	bool CanBeReturned(const FAngelscriptTypeUsage& Usage) const override
	{
		return true;
	}

	void GetReturnValue(const FAngelscriptTypeUsage& Usage, class asIScriptContext* Context, void* Destination) const override
	{
		check(!Usage.bIsReference);

		*(float*)Destination = (float)TGetAngelscriptReturnValue<double>(Context);
	}

	int32 GetValueSize(const FAngelscriptTypeUsage& Usage) const override
	{
		return sizeof(float);
	}

	int32 GetValueAlignment(const FAngelscriptTypeUsage& Usage) const
	{
		return alignof(float);
	}

	bool CanBeTemplateSubType() const override
	{
		return false;
	}

#if AS_CAN_GENERATE_JIT
	bool GetCppForm(const FAngelscriptTypeUsage& Usage, FAngelscriptType::FCppForm& OutCppForm) const override
	{
		OutCppForm.bIsPrimitive = true;
		OutCppForm.CppType = TEXT("double");
		return true;
	}
#endif
};

static uint8 AS_MIN_uint8 = MIN_uint8;
static uint16 AS_MIN_uint16 = MIN_uint16;
static uint32 AS_MIN_uint32 = MIN_uint32;
static uint64 AS_MIN_uint64 = MIN_uint64;
static int8 AS_MIN_int8 = MIN_int8;
static int16 AS_MIN_int16 = MIN_int16;
static int32 AS_MIN_int32 = MIN_int32;
static int64 AS_MIN_int64 = MIN_int64;

static uint8 AS_MAX_uint8 = MAX_uint8;
static uint16 AS_MAX_uint16 = MAX_uint16;
static uint32 AS_MAX_uint32 = MAX_uint32;
static uint64 AS_MAX_uint64 = MAX_uint64;
static int8 AS_MAX_int8 = MAX_int8;
static int16 AS_MAX_int16 = MAX_int16;
static int32 AS_MAX_int32 = MAX_int32;
static int64 AS_MAX_int64 = MAX_int64;

static float AS_MIN_flt = MIN_flt;
static float AS_MAX_flt = MAX_flt;
static double AS_MIN_dbl = MIN_dbl;
static double AS_MAX_dbl = MAX_dbl;

static double AS_PI = PI;
static double AS_HALF_PI = HALF_PI;
static double AS_TWO_PI = 2.f*PI;
static double AS_SMALL_NUMBER = SMALL_NUMBER;
static double AS_KINDA_SMALL_NUMBER = KINDA_SMALL_NUMBER;
static double AS_BIG_NUMBER = BIG_NUMBER;
static double AS_EULERS_NUMBER = EULERS_NUMBER;
static double AS_THRESH_VECTOR_NORMALIZED = THRESH_VECTOR_NORMALIZED;
static double AS_THRESH_NORMALS_ARE_PARALLEL = THRESH_NORMALS_ARE_PARALLEL;
static double AS_THRESH_NORMALS_ARE_ORTHOGONAL = THRESH_NORMALS_ARE_ORTHOGONAL;

static float AS_PI_flt = PI;
static float AS_HALF_PI_flt = HALF_PI;
static float AS_TWO_PI_flt = 2.f*PI;
static float AS_SMALL_NUMBER_flt = SMALL_NUMBER;
static float AS_KINDA_SMALL_NUMBER_flt = KINDA_SMALL_NUMBER;
static float AS_BIG_NUMBER_flt = BIG_NUMBER;
static float AS_EULERS_NUMBER_flt = EULERS_NUMBER;
static float AS_THRESH_VECTOR_NORMALIZED_flt = THRESH_VECTOR_NORMALIZED;
static float AS_THRESH_NORMALS_ARE_PARALLEL_flt = THRESH_NORMALS_ARE_PARALLEL;
static float AS_THRESH_NORMALS_ARE_ORTHOGONAL_flt = THRESH_NORMALS_ARE_ORTHOGONAL;

static float AS_NAN_flt = NAN;
static double AS_NAN_dbl = NAN;

AS_FORCE_LINK const FAngelscriptBinds::FBind Bind_PrimitiveTypes(FAngelscriptBinds::EOrder::Early, []
{
	auto IntType = MakeShared<FIntType>();
	FAngelscriptType::Register(IntType);

	auto UIntType = MakeShared<FUIntType>();
	FAngelscriptType::Register(UIntType);

	FAngelscriptType::Register(MakeShared<FInt64Type>());
	FAngelscriptType::Register(MakeShared<FUInt64Type>());
	FAngelscriptType::Register(MakeShared<FInt16Type>());
	FAngelscriptType::Register(MakeShared<FUInt16Type>());
	FAngelscriptType::Register(MakeShared<FInt8Type>());
	FAngelscriptType::Register(MakeShared<FUInt8Type>());

	auto* ConfigSettings = GetDefault<UAngelscriptSettings>();

	auto BoolType = MakeShared<FBoolType>();
	FAngelscriptType::ScriptBoolType() = BoolType;
	FAngelscriptType::Register(BoolType);

	auto FloatType = MakeShared<FFloatType>(
		ConfigSettings->bScriptFloatIsFloat64 ? TEXT("float32") : TEXT("float")
	);
	FAngelscriptType::ScriptFloatType() = FloatType;
	FAngelscriptType::Register(FloatType);

	auto DoubleType = MakeShared<FDoubleType>(
		ConfigSettings->bScriptFloatIsFloat64 ? TEXT("float") : TEXT("float64")
	);
	FAngelscriptType::ScriptDoubleType() = DoubleType;
	FAngelscriptType::Register(DoubleType);

	auto ExtendedType = MakeShared<FUnrealFloatParamExtendedToDoubleType>(
		ConfigSettings->bScriptFloatIsFloat64 ? TEXT("float") : TEXT("float64")
	);
	FAngelscriptType::ScriptFloatParamExtendedToDoubleType() = ExtendedType;

	// Make sure all the aliased types will be found correctly
	FAngelscriptType::RegisterAlias(TEXT("double"), DoubleType);
	FAngelscriptType::RegisterAlias(TEXT("int32"), IntType);
	FAngelscriptType::RegisterAlias(TEXT("uint32"), UIntType);

	if (ConfigSettings->bScriptFloatIsFloat64)
		FAngelscriptType::RegisterAlias(TEXT("float64"), DoubleType);
	else
		FAngelscriptType::RegisterAlias(TEXT("float32"), FloatType);

	FAngelscriptBinds::BindGlobalVariable("const uint8 MIN_uint8", &AS_MIN_uint8);
	FAngelscriptBinds::SetPreviousBoundGlobalVariablePureConstant(AS_MIN_uint8);
	SCRIPT_GLOBAL_DOCUMENTATION("Lowest number a uint8 can hold (0)");

	FAngelscriptBinds::BindGlobalVariable("const uint16 MIN_uint16", &AS_MIN_uint16);
	FAngelscriptBinds::SetPreviousBoundGlobalVariablePureConstant(AS_MIN_uint16);
	SCRIPT_GLOBAL_DOCUMENTATION("Lowest number a uint16 can hold (0)");

	FAngelscriptBinds::BindGlobalVariable("const uint32 MIN_uint32", &AS_MIN_uint32);
	FAngelscriptBinds::SetPreviousBoundGlobalVariablePureConstant(AS_MIN_uint32);
	SCRIPT_GLOBAL_DOCUMENTATION("Lowest number a uint32 can hold (0)");
	
	FAngelscriptBinds::BindGlobalVariable("const uint64 MIN_uint64", &AS_MIN_uint64);
	FAngelscriptBinds::SetPreviousBoundGlobalVariablePureConstant(AS_MIN_uint64);
	SCRIPT_GLOBAL_DOCUMENTATION("Lowest number a uint64 can hold (0)");

	FAngelscriptBinds::BindGlobalVariable("const int8 MIN_int8", &AS_MIN_int8);
	FAngelscriptBinds::SetPreviousBoundGlobalVariablePureConstant(AS_MIN_int8);
	SCRIPT_GLOBAL_DOCUMENTATION("Lowest number an int8 can hold (-128)");

	FAngelscriptBinds::BindGlobalVariable("const int16 MIN_int16", &AS_MIN_int16);
	FAngelscriptBinds::SetPreviousBoundGlobalVariablePureConstant(AS_MIN_int16);
	SCRIPT_GLOBAL_DOCUMENTATION("Lowest number an int16 can hold (-32768)");

	FAngelscriptBinds::BindGlobalVariable("const int32 MIN_int32", &AS_MIN_int32);
	FAngelscriptBinds::SetPreviousBoundGlobalVariablePureConstant(AS_MIN_int32);
	SCRIPT_GLOBAL_DOCUMENTATION("Lowest number an int32 can hold (-2147483648)");

	FAngelscriptBinds::BindGlobalVariable("const int64 MIN_int64", &AS_MIN_int64);
	FAngelscriptBinds::SetPreviousBoundGlobalVariablePureConstant(AS_MIN_int64);
	SCRIPT_GLOBAL_DOCUMENTATION("Lowest number an int64 can hold (-9223372036854775808)");

	FAngelscriptBinds::BindGlobalVariable("const uint8 MAX_uint8", &AS_MAX_uint8);
	FAngelscriptBinds::SetPreviousBoundGlobalVariablePureConstant(AS_MAX_uint8);
	SCRIPT_GLOBAL_DOCUMENTATION("Highest number a uint8 can hold (255)");

	FAngelscriptBinds::BindGlobalVariable("const uint16 MAX_uint16", &AS_MAX_uint16);
	FAngelscriptBinds::SetPreviousBoundGlobalVariablePureConstant(AS_MAX_uint16);
	SCRIPT_GLOBAL_DOCUMENTATION("Highest number a uint16 can hold (65535)");

	FAngelscriptBinds::BindGlobalVariable("const uint32 MAX_uint32", &AS_MAX_uint32);
	FAngelscriptBinds::SetPreviousBoundGlobalVariablePureConstant(AS_MAX_uint32);
	SCRIPT_GLOBAL_DOCUMENTATION("Highest number a uint32 can hold (4294967295)");

	FAngelscriptBinds::BindGlobalVariable("const uint64 MAX_uint64", &AS_MAX_uint64);
	FAngelscriptBinds::SetPreviousBoundGlobalVariablePureConstant(AS_MAX_uint64);
	SCRIPT_GLOBAL_DOCUMENTATION("Highest number a uint64 can hold (18446744073709551615)");

	FAngelscriptBinds::BindGlobalVariable("const int8 MAX_int8", &AS_MAX_int8);
	FAngelscriptBinds::SetPreviousBoundGlobalVariablePureConstant(AS_MAX_int8);
	SCRIPT_GLOBAL_DOCUMENTATION("Highest number an int8 can hold (127)");

	FAngelscriptBinds::BindGlobalVariable("const int16 MAX_int16", &AS_MAX_int16);
	FAngelscriptBinds::SetPreviousBoundGlobalVariablePureConstant(AS_MAX_int16);
	SCRIPT_GLOBAL_DOCUMENTATION("Highest number an int8 can hold (32767)");

	FAngelscriptBinds::BindGlobalVariable("const int32 MAX_int32", &AS_MAX_int32);
	FAngelscriptBinds::SetPreviousBoundGlobalVariablePureConstant(AS_MAX_int32);
	SCRIPT_GLOBAL_DOCUMENTATION("Highest number an int32 can hold (2147483647)");

	FAngelscriptBinds::BindGlobalVariable("const int64 MAX_int64", &AS_MAX_int64);
	FAngelscriptBinds::SetPreviousBoundGlobalVariablePureConstant(AS_MAX_int64);
	SCRIPT_GLOBAL_DOCUMENTATION("Highest number an int64 can hold (9223372036854775807)");

	if (!ConfigSettings->bDeprecateDoubleType)
	{
		FAngelscriptBinds::BindGlobalVariable("const float64 MIN_dbl", &AS_MIN_dbl);
		FAngelscriptBinds::SetPreviousBoundGlobalVariablePureConstant(AS_MIN_dbl);
		SCRIPT_GLOBAL_DOCUMENTATION("Smallest positive number a double can hold (~0.0000...001)");

		FAngelscriptBinds::BindGlobalVariable("const float64 MAX_dbl", &AS_MAX_dbl);
		FAngelscriptBinds::SetPreviousBoundGlobalVariablePureConstant(AS_MAX_dbl);
		SCRIPT_GLOBAL_DOCUMENTATION("Largest positive number a double can hold (~10^308)");

		FAngelscriptBinds::BindGlobalVariable("const float64 NAN_dbl", &AS_NAN_dbl);
		FAngelscriptBinds::SetPreviousBoundGlobalVariablePureConstant(AS_NAN_dbl);
		SCRIPT_GLOBAL_DOCUMENTATION("Special Not-a-Number value for double floating point");
	}

	if (ConfigSettings->bScriptFloatIsFloat64)
	{
		FAngelscriptBinds::BindGlobalVariable("const float64 MIN_flt", &AS_MIN_dbl);
		FAngelscriptBinds::SetPreviousBoundGlobalVariablePureConstant(AS_MIN_dbl);
		SCRIPT_GLOBAL_DOCUMENTATION("Smallest positive number a float can hold (~0.0000...001)");

		FAngelscriptBinds::BindGlobalVariable("const float64 MAX_flt", &AS_MAX_dbl);
		FAngelscriptBinds::SetPreviousBoundGlobalVariablePureConstant(AS_MAX_dbl);
		SCRIPT_GLOBAL_DOCUMENTATION("Largest positive number a float can hold (~10^308)");

		FAngelscriptBinds::BindGlobalVariable("const float64 NAN_flt", &AS_NAN_dbl);
		FAngelscriptBinds::SetPreviousBoundGlobalVariablePureConstant(AS_NAN_dbl);
		SCRIPT_GLOBAL_DOCUMENTATION("Special Not-a-Number value for floating point");
	}
	else
	{
		FAngelscriptBinds::BindGlobalVariable("const float32 MIN_flt", &AS_MIN_flt);
		FAngelscriptBinds::SetPreviousBoundGlobalVariablePureConstant(AS_MIN_flt);
		SCRIPT_GLOBAL_DOCUMENTATION("Smallest positive number a float can hold (~0.0000...001)");

		FAngelscriptBinds::BindGlobalVariable("const float32 MAX_flt", &AS_MAX_flt);
		FAngelscriptBinds::SetPreviousBoundGlobalVariablePureConstant(AS_MAX_flt);
		SCRIPT_GLOBAL_DOCUMENTATION("Largest positive number a float can hold (~10^38)");

		FAngelscriptBinds::BindGlobalVariable("const float32 NAN_flt", &AS_NAN_flt);
		FAngelscriptBinds::SetPreviousBoundGlobalVariablePureConstant(AS_NAN_flt);
		SCRIPT_GLOBAL_DOCUMENTATION("Special Not-a-Number value for floating point");
	}

	FAngelscriptBinds::BindGlobalVariable("const float64 EULERS_NUMBER", &AS_EULERS_NUMBER);
	FAngelscriptBinds::SetPreviousBoundGlobalVariablePureConstant(AS_EULERS_NUMBER);
	SCRIPT_GLOBAL_DOCUMENTATION("Euler's number, also known as `e` (2.71828...)");

	FAngelscriptBinds::BindGlobalVariable("const float64 PI", &AS_PI);
	FAngelscriptBinds::SetPreviousBoundGlobalVariablePureConstant(AS_PI);
	SCRIPT_GLOBAL_DOCUMENTATION("Pi, also known as `π` (3.14159...)");

	FAngelscriptBinds::BindGlobalVariable("const float64 HALF_PI", &AS_HALF_PI);
	FAngelscriptBinds::SetPreviousBoundGlobalVariablePureConstant(AS_HALF_PI);
	SCRIPT_GLOBAL_DOCUMENTATION("Half the value of Pi (1.57079...)");

	FAngelscriptBinds::BindGlobalVariable("const float64 TWO_PI", &AS_TWO_PI);
	FAngelscriptBinds::SetPreviousBoundGlobalVariablePureConstant(AS_TWO_PI);
	SCRIPT_GLOBAL_DOCUMENTATION("Double the value of Pi (6.28318...)");

	FAngelscriptBinds::BindGlobalVariable("const float64 SMALL_NUMBER", &AS_SMALL_NUMBER);
	FAngelscriptBinds::SetPreviousBoundGlobalVariablePureConstant(AS_SMALL_NUMBER);
	SCRIPT_GLOBAL_DOCUMENTATION("A very small number (0.00000001, or 1e-8)");

	FAngelscriptBinds::BindGlobalVariable("const float64 KINDA_SMALL_NUMBER", &AS_KINDA_SMALL_NUMBER);
	FAngelscriptBinds::SetPreviousBoundGlobalVariablePureConstant(AS_KINDA_SMALL_NUMBER);
	SCRIPT_GLOBAL_DOCUMENTATION("A somewhat small number (0.0001, or 1e-4)");

	FAngelscriptBinds::BindGlobalVariable("const float64 BIG_NUMBER", &AS_BIG_NUMBER);
	FAngelscriptBinds::SetPreviousBoundGlobalVariablePureConstant(AS_BIG_NUMBER);
	SCRIPT_GLOBAL_DOCUMENTATION("A very large number (~10^38)");

	FAngelscriptBinds::BindGlobalVariable("const float64 THRESH_VECTOR_NORMALIZED", &AS_THRESH_VECTOR_NORMALIZED);
	FAngelscriptBinds::SetPreviousBoundGlobalVariablePureConstant(AS_THRESH_VECTOR_NORMALIZED);

	FAngelscriptBinds::BindGlobalVariable("const float64 THRESH_NORMALS_ARE_PARALLEL", &AS_THRESH_NORMALS_ARE_PARALLEL);
	FAngelscriptBinds::SetPreviousBoundGlobalVariablePureConstant(AS_THRESH_NORMALS_ARE_PARALLEL);

	FAngelscriptBinds::BindGlobalVariable("const float64 THRESH_NORMALS_ARE_ORTHOGONAL", &AS_THRESH_NORMALS_ARE_ORTHOGONAL);
	FAngelscriptBinds::SetPreviousBoundGlobalVariablePureConstant(AS_THRESH_NORMALS_ARE_ORTHOGONAL);

	FAngelscriptBinds::BindGlobalVariable("const float32 __EULERS_NUMBER_flt", &AS_EULERS_NUMBER_flt);
	FAngelscriptBinds::SetPreviousBoundGlobalVariablePureConstant(AS_EULERS_NUMBER_flt);

	FAngelscriptBinds::BindGlobalVariable("const float32 __PI_flt", &AS_PI_flt);
	FAngelscriptBinds::SetPreviousBoundGlobalVariablePureConstant(AS_PI_flt);

	FAngelscriptBinds::BindGlobalVariable("const float32 __HALF_PI_flt", &AS_HALF_PI_flt);
	FAngelscriptBinds::SetPreviousBoundGlobalVariablePureConstant(AS_HALF_PI_flt);

	FAngelscriptBinds::BindGlobalVariable("const float32 __TWO_PI_flt", &AS_TWO_PI_flt);
	FAngelscriptBinds::SetPreviousBoundGlobalVariablePureConstant(AS_TWO_PI_flt);

	FAngelscriptBinds::BindGlobalVariable("const float32 __SMALL_NUMBER_flt", &AS_SMALL_NUMBER_flt);
	FAngelscriptBinds::SetPreviousBoundGlobalVariablePureConstant(AS_SMALL_NUMBER_flt);

	FAngelscriptBinds::BindGlobalVariable("const float32 __KINDA_SMALL_NUMBER_flt", &AS_KINDA_SMALL_NUMBER_flt);
	FAngelscriptBinds::SetPreviousBoundGlobalVariablePureConstant(AS_KINDA_SMALL_NUMBER_flt);

	FAngelscriptBinds::BindGlobalVariable("const float32 __BIG_NUMBER_flt", &AS_BIG_NUMBER_flt);
	FAngelscriptBinds::SetPreviousBoundGlobalVariablePureConstant(AS_BIG_NUMBER_flt);

	FAngelscriptBinds::BindGlobalVariable("const float32 __THRESH_VECTOR_NORMALIZED_flt", &AS_THRESH_VECTOR_NORMALIZED_flt);
	FAngelscriptBinds::SetPreviousBoundGlobalVariablePureConstant(AS_THRESH_VECTOR_NORMALIZED_flt);

	FAngelscriptBinds::BindGlobalVariable("const float32 __THRESH_NORMALS_ARE_PARALLEL_flt", &AS_THRESH_NORMALS_ARE_PARALLEL_flt);
	FAngelscriptBinds::SetPreviousBoundGlobalVariablePureConstant(AS_THRESH_NORMALS_ARE_PARALLEL_flt);

	FAngelscriptBinds::BindGlobalVariable("const float32 __THRESH_NORMALS_ARE_ORTHOGONAL_flt", &AS_THRESH_NORMALS_ARE_ORTHOGONAL_flt);
	FAngelscriptBinds::SetPreviousBoundGlobalVariablePureConstant(AS_THRESH_NORMALS_ARE_ORTHOGONAL_flt);

	FToStringHelper::Register(TEXT("int8"), [](void* Ptr, FString& Str)
	{
		Str += FString::Printf(TEXT("%d"), *(int8*)Ptr);
	});

	FToStringHelper::Register(TEXT("int16"), [](void* Ptr, FString& Str)
	{
		Str += FString::Printf(TEXT("%d"), *(int16*)Ptr);
	});

	FToStringHelper::Register(TEXT("int32"), [](void* Ptr, FString& Str)
	{
		Str += FString::Printf(TEXT("%d"), *(int32*)Ptr);
	});

	FToStringHelper::Register(TEXT("int64"), [](void* Ptr, FString& Str)
	{
		Str += FString::Printf(TEXT("%lld"), *(int64*)Ptr);
	});

	FToStringHelper::Register(TEXT("uint8"), [](void* Ptr, FString& Str)
	{
		Str += FString::Printf(TEXT("%u"), *(uint8*)Ptr);
	});

	FToStringHelper::Register(TEXT("uint16"), [](void* Ptr, FString& Str)
	{
		Str += FString::Printf(TEXT("%u"), *(uint16*)Ptr);
	});

	FToStringHelper::Register(TEXT("uint32"), [](void* Ptr, FString& Str)
	{
		Str += FString::Printf(TEXT("%u"), *(uint32*)Ptr);
	});

	FToStringHelper::Register(TEXT("uint64"), [](void* Ptr, FString& Str)
	{
		Str += FString::Printf(TEXT("%llu"), *(uint64*)Ptr);
	});

	FToStringHelper::Register(TEXT("float32"), [](void* Ptr, FString& Str)
	{
		Str += FString::SanitizeFloat(*(float*)Ptr);
	});

	FToStringHelper::Register(TEXT("float64"), [](void* Ptr, FString& Str)
	{
		Str += FString::SanitizeFloat(*(double*)Ptr);
	});

	FToStringHelper::Register(TEXT("bool"), [](void* Ptr, FString& Str)
	{
		Str += *(bool*)Ptr ? TEXT("true") : TEXT("false");
	});
});

