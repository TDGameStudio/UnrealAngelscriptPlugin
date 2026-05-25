#include "AngelscriptBinds.h"
#include "AngelscriptEngine.h"
#include "AngelscriptDocs.h"

#include "Containers/UnrealString.h"
#include "UObject/TextProperty.h"

#include "Helper_CppType.h"
#include "Helper_GetTypeInfo.h"
#include "Helper_ToString.h"

#include "StartAngelscriptHeaders.h"
//#include "as_objecttype.h"
//#include "as_scriptengine.h"
#include "source/as_objecttype.h"
#include "source/as_scriptengine.h"
#include "EndAngelscriptHeaders.h"

struct FTextType : TAngelscriptCppPropertyType<FTextProperty>
{
	FString GetAngelscriptTypeName() const override
	{
		return TEXT("FText");
	}

	bool GetDebuggerValue(const FAngelscriptTypeUsage& Usage, void* Address, struct FDebuggerValue& Value) const override
	{
		FText& NativeValue = Usage.ResolvePrimitive<FText>(Address);

		Value.Type = Usage.GetAngelscriptDeclaration();
		Value.Usage = Usage;
		Value.Address = Address;
		Value.Value = TEXT("FText: \"") + NativeValue.ToString() + TEXT("\"");

		return true;
	}

	bool GetCppForm(const FAngelscriptTypeUsage& Usage, FCppForm& OutCppForm) const override
	{
		OutCppForm.CppType = GetAngelscriptTypeName();
		return true;
	}

	bool CanCompare(const FAngelscriptTypeUsage& Usage) const override
	{
		return true;
	}

	bool IsValueEqual(const FAngelscriptTypeUsage& Usage, void* SourcePtr, void* DestinationPtr) const override
	{
		// We use the 'is it the same locale string' compare here, instead of
		// the lexical compare, because properties of this type should not be
		// considered equal for reload purposes if they are the same string but
		// point to different locale entries.
		return (*(FText*)SourcePtr).IdenticalTo(*(FText*)DestinationPtr);
	}
};

AS_FORCE_LINK const FAngelscriptBinds::FBind Bind_FText(FAngelscriptBinds::EOrder::Early, []
{
	auto ETextIdenticalModeFlags_ = FAngelscriptBinds::Enum("ETextIdenticalModeFlags");
	ETextIdenticalModeFlags_["None"]  = ETextIdenticalModeFlags::None;
	ETextIdenticalModeFlags_["DeepCompare"] = ETextIdenticalModeFlags::DeepCompare;
	ETextIdenticalModeFlags_["LexicalCompareInvariants"] = ETextIdenticalModeFlags::LexicalCompareInvariants;

	auto EDateTimeStyle_ = FAngelscriptBinds::Enum("EDateTimeStyle");
	EDateTimeStyle_["Default"] = EDateTimeStyle::Default;
	EDateTimeStyle_["Short"] = EDateTimeStyle::Short;
	EDateTimeStyle_["Medium"] = EDateTimeStyle::Medium;
	EDateTimeStyle_["Long"] = EDateTimeStyle::Long;
	EDateTimeStyle_["Full"] = EDateTimeStyle::Full;

	// Register text type
	auto FText_ = FAngelscriptBinds::ValueClass<FText>("FText", FBindFlags());
	FAngelscriptType::Register(MakeShared<FTextType>());

	TGetStaticTypeInfo<FText>::SetForEngine(FAngelscriptEngine::Get().GetScriptEngine(), FText_.GetTypeInfo());

	FText_.Constructor("void f()", [](FText* Address)
	{
		new(Address) FText();
	});
	FAngelscriptBinds::SetPreviousBindNoDiscard(true);
	SCRIPT_TRIVIAL_NATIVE_CONSTRUCTOR(FText_, "FText");

	FText_.Constructor("void f(const FText& Other)", [](FText* Address, const FText& Other)
	{
		new(Address) FText(Other);
	});
	FAngelscriptBinds::SetPreviousBindNoDiscard(true);
	SCRIPT_TRIVIAL_NATIVE_CONSTRUCTOR(FText_, "FText");

	FText_.Destructor("void f()", [](FText& Str)
	{
		Str.~FText();
	});
	SCRIPT_TRIVIAL_NATIVE_DESTRUCTOR(FText_, "FText");

	FText_.Method("bool IsEmpty() const", METHOD_TRIVIAL(FText, IsEmpty));
	FText_.Method("bool IsEmptyOrWhitespace() const", METHOD_TRIVIAL(FText, IsEmptyOrWhitespace));
	FText_.Method("bool IsTransient() const", METHOD_TRIVIAL(FText, IsTransient));
	FText_.Method("bool IsCultureInvariant() const", METHOD_TRIVIAL(FText, IsCultureInvariant));
	FText_.Method("bool IsInitializedFromString() const", METHOD_TRIVIAL(FText, IsInitializedFromString));
	FText_.Method("bool IsFromStringTable() const", METHOD_TRIVIAL(FText, IsFromStringTable));
	FText_.Method("bool IdenticalTo( const FText& Other, const ETextIdenticalModeFlags CompareModeFlags = ETextIdenticalModeFlags::None ) const", METHOD_TRIVIAL(FText, IdenticalTo));

	FText_.Method("FText& opAssign(const FText& Other)", METHODPR_TRIVIAL(FText&, FText, operator=, (const FText&)));

	FToStringHelper::Register(TEXT("FText"), [](void* Ptr, FString& Str)
	{
		Str += ((FText*)Ptr)->ToString();
	});
});

template <typename InternalType, typename ExternalType>
FORCEINLINE static bool AddPrimitiveFormatOrderedArgument(FFormatOrderedArguments& OutFormatOrderedArguments, const void* Ptr)
{
	const InternalType Value = *reinterpret_cast<const ExternalType*>(Ptr);
	OutFormatOrderedArguments.Emplace(FFormatArgumentValue(Value));
	return true;
}

static bool AddFormatOrderedArgument(FFormatOrderedArguments& OutFormatOrderedArguments, const void* Ptr, int TypeId)
{
	// primitive types
	switch (TypeId & asTYPEID_MASK_SEQNBR)
	{
	case asTYPEID_INT8:		return AddPrimitiveFormatOrderedArgument<int32, int8>(OutFormatOrderedArguments, Ptr);
	case asTYPEID_INT16:	return AddPrimitiveFormatOrderedArgument<int32, int16>(OutFormatOrderedArguments, Ptr);
	case asTYPEID_INT32:	return AddPrimitiveFormatOrderedArgument<int32, int32>(OutFormatOrderedArguments, Ptr);
	case asTYPEID_INT64:	return AddPrimitiveFormatOrderedArgument<int64, int64>(OutFormatOrderedArguments, Ptr);
	case asTYPEID_UINT8:	return AddPrimitiveFormatOrderedArgument<uint32, uint8>(OutFormatOrderedArguments, Ptr);
	case asTYPEID_UINT16:	return AddPrimitiveFormatOrderedArgument<uint32, uint16>(OutFormatOrderedArguments, Ptr);
	case asTYPEID_UINT32:	return AddPrimitiveFormatOrderedArgument<uint32, uint32>(OutFormatOrderedArguments, Ptr);
	case asTYPEID_UINT64:	return AddPrimitiveFormatOrderedArgument<uint64, uint64>(OutFormatOrderedArguments, Ptr);
	case asTYPEID_FLOAT32:	return AddPrimitiveFormatOrderedArgument<float, float>(OutFormatOrderedArguments, Ptr);
	case asTYPEID_FLOAT64:	return AddPrimitiveFormatOrderedArgument<double, double>(OutFormatOrderedArguments, Ptr);
	}

	// custom types
	asITypeInfo* TypeInfo = FAngelscriptEngine::Get().Engine->GetTypeInfoById(TypeId);
	if (ensure(TypeInfo != nullptr))
	{
		// enum
		if ((TypeInfo->GetFlags() & asOBJ_ENUM) != 0)
		{
			const uint32 Value = *reinterpret_cast<const uint8*>(Ptr);
			OutFormatOrderedArguments.Emplace(FFormatArgumentValue(Value));
			return true;
		}

		// ftext
		if (TGetStaticTypeInfo<FText>::IsForEngine(FAngelscriptEngine::Get().GetScriptEngine(), TypeInfo))
		{
			const FText& Value = *reinterpret_cast<const FText*>(Ptr);
			OutFormatOrderedArguments.Emplace(FFormatArgumentValue(Value));
			return true;
		}

		const FString Message = FString::Printf(TEXT("Invalid argument type passed to FText::Format: %s"), ANSI_TO_TCHAR(TypeInfo->GetName()));
		FAngelscriptEngine::Throw(TCHAR_TO_ANSI(*Message));
		return false;
	}

	FAngelscriptEngine::Throw("Invalid argument type passed to FText::Format");
	return false;
}

static void Generic_TextFormat(asIScriptGeneric* Generic)
{
	const FText& Format = *reinterpret_cast<FText*>(Generic->GetArgAddress(0));

	bool bSuccess = true;
	FFormatOrderedArguments FormatOrderedArguments;
	for (int i = 1; bSuccess && i < Generic->GetArgCount(); ++i)
	{
		bSuccess &= AddFormatOrderedArgument(FormatOrderedArguments, Generic->GetArgAddress(i), Generic->GetArgTypeId(i));
	}

	const FText OutText = bSuccess ? FText::Format(Format, FormatOrderedArguments) : FText();
	new (Generic->GetAddressOfReturnLocation()) FText(OutText);
}

static FText Named_TextFormat(const FText& Format, const TMap<FString, FFormatArgumentValue>& Arguments)
{
	FFormatNamedArguments FormatArguments;
	FormatArguments.Reserve(Arguments.Num());
	for (auto& Argument : Arguments)
	{
		FormatArguments.Add(Argument.Key, Argument.Value);
	}
	return FText::Format(Format, FormatArguments);
}

template<typename Type>
static FText AsNumber_TextFormat(Type Val, const FNumberFormattingOptions& Options)
{
	return FText::AsNumber(Val, &Options);
}

static FText Ordered_TextFormat(const FText& Format, const TArray<FFormatArgumentValue>& Arguments)
{
	return FText::Format(Format, Arguments);
}

AS_FORCE_LINK const FAngelscriptBinds::FBind Bind_FText_Operations(FAngelscriptBinds::EOrder::Late, []
{
	auto FText_ = FAngelscriptBinds::ExistingClass("FText");

	{
		FAngelscriptBinds::FNamespace ns("FText");
		FAngelscriptBinds::BindGlobalFunction("FText FromStringTable(const FName InTableId, const FString& InKey, const EStringTableLoadingPolicy InLoadingPolicy = EStringTableLoadingPolicy::FindOrLoad) no_discard", FUNC_TRIVIAL(FText::FromStringTable));
		FAngelscriptBinds::BindGlobalFunction("FText FromName(const FName& Val) no_discard", FUNC_TRIVIAL(FText::FromName));
		FAngelscriptBinds::BindGlobalFunction("FText FromString(const FString& Val) no_discard", FUNCPR_TRIVIAL(FText, FText::FromString, (const FString&)));
		FAngelscriptBinds::BindGlobalFunction("FText AsCultureInvariant(const FString& Val) no_discard", [](const FString& Val) -> FText { return FText::AsCultureInvariant(Val); });
		FAngelscriptBinds::BindGlobalFunction("FText Join(const FText& Delimiter, const TArray<FFormatArgumentValue>& Args) no_discard", FUNCPR_TRIVIAL(FText, FText::Join, (const FText&, const TArray<FFormatArgumentValue>&)));
		FAngelscriptBinds::BindGlobalFunction("FText Join(const FText& Delimiter, const TArray<FText>& Args) no_discard", FUNCPR_TRIVIAL(FText, FText::Join, (const FText&, const TArray<FText>&)));

		FAngelscriptBinds::BindGlobalFunction("FText AsDate(const FDateTime& DateTime, const EDateTimeStyle::Type DateStyle = EDateTimeStyle::Default) no_discard",
			[](const FDateTime& DateTime, const EDateTimeStyle::Type DateStyle) -> FText { return FText::AsDate(DateTime, DateStyle); });
		FAngelscriptBinds::BindGlobalFunction("FText AsDateTime(const FDateTime& DateTime, const EDateTimeStyle::Type DateStyle = EDateTimeStyle::Default, const EDateTimeStyle::Type TimeStyle = EDateTimeStyle::Default) no_discard",
			[](const FDateTime& DateTime, const EDateTimeStyle::Type DateStyle, const EDateTimeStyle::Type TimeStyle) -> FText { return FText::AsDateTime(DateTime, DateStyle, TimeStyle); });
		FAngelscriptBinds::BindGlobalFunction("FText AsTime(const FDateTime& DateTime, const EDateTimeStyle::Type TimeStyle = EDateTimeStyle::Default) no_discard",
			[](const FDateTime& DateTime, const EDateTimeStyle::Type TimeStyle) -> FText { return FText::AsTime(DateTime, TimeStyle); });
		FAngelscriptBinds::BindGlobalFunction("FText AsTimespan(const FTimespan& Timespan) no_discard",
			[](const FTimespan& Timespan) -> FText { return FText::AsTimespan(Timespan); });

		FAngelscriptBinds::BindGlobalFunction("FText AsNumber(float32 Val, const FNumberFormattingOptions& Options) no_discard", &AsNumber_TextFormat<float>);
		FAngelscriptBinds::BindGlobalFunction("FText AsNumber(float64 Val, const FNumberFormattingOptions& Options) no_discard", &AsNumber_TextFormat<double>);
		FAngelscriptBinds::BindGlobalFunction("FText AsNumber(int8 Val, const FNumberFormattingOptions& Options) no_discard", &AsNumber_TextFormat<int8>);
		FAngelscriptBinds::BindGlobalFunction("FText AsNumber(int16 Val, const FNumberFormattingOptions& Options) no_discard", &AsNumber_TextFormat<int16>);
		FAngelscriptBinds::BindGlobalFunction("FText AsNumber(int32 Val, const FNumberFormattingOptions& Options) no_discard", &AsNumber_TextFormat<int32>);
		FAngelscriptBinds::BindGlobalFunction("FText AsNumber(int64 Val, const FNumberFormattingOptions& Options) no_discard", &AsNumber_TextFormat<int64>);
		FAngelscriptBinds::BindGlobalFunction("FText AsNumber(uint8 Val, const FNumberFormattingOptions& Options) no_discard", &AsNumber_TextFormat<uint8>);
		FAngelscriptBinds::BindGlobalFunction("FText AsNumber(uint16 Val, const FNumberFormattingOptions& Options) no_discard", &AsNumber_TextFormat<uint16>);
		FAngelscriptBinds::BindGlobalFunction("FText AsNumber(uint32 Val, const FNumberFormattingOptions& Options) no_discard", &AsNumber_TextFormat<uint32>);
		FAngelscriptBinds::BindGlobalFunction("FText AsNumber(uint64 Val, const FNumberFormattingOptions& Options) no_discard", &AsNumber_TextFormat<uint64>);
		
		FAngelscriptBinds::BindGlobalFunction("FText AsMemory(uint64 NumBytes) no_discard",
			[](uint64 NumBytes) -> FText { return FText::AsMemory(NumBytes); });

		FAngelscriptBinds::BindGlobalGenericFunction("FText Format(const FText& Format, const ?& Arg0) no_discard", &Generic_TextFormat);
		FAngelscriptBinds::BindGlobalGenericFunction("FText Format(const FText& Format, const ?& Arg0, const ?& Arg1) no_discard", &Generic_TextFormat);
		FAngelscriptBinds::BindGlobalGenericFunction("FText Format(const FText& Format, const ?& Arg0, const ?& Arg1, const ?& Arg2) no_discard", &Generic_TextFormat);
		FAngelscriptBinds::BindGlobalGenericFunction("FText Format(const FText& Format, const ?& Arg0, const ?& Arg1, const ?& Arg2, const ?& Arg3) no_discard", &Generic_TextFormat);
		FAngelscriptBinds::BindGlobalGenericFunction("FText Format(const FText& Format, const ?& Arg0, const ?& Arg1, const ?& Arg2, const ?& Arg3, const ?& Arg4) no_discard", &Generic_TextFormat);
		FAngelscriptBinds::BindGlobalFunction("FText Format(const FText& Format, const TMap<FString, FFormatArgumentValue>& Arguments) no_discard", &Named_TextFormat);
		FAngelscriptBinds::BindGlobalFunction("FText Format(const FText& Format, const TArray<FFormatArgumentValue>& Arguments) no_discard", &Ordered_TextFormat);

		FAngelscriptBinds::BindGlobalFunction("void GetFormatPatternParameters(const FText& Fmt, TArray<FString>&out ParameterNames)",
			[](const FText& Fmt, TArray<FString>& ParameterNames) -> void { FText::GetFormatPatternParameters(Fmt, ParameterNames); });
	}

	FAngelscriptBinds::BindGlobalFunction("FText NSLOCTEXT(const FString& Namespace, const FString& Key, const FString& Text) no_discard",
	[](const FString& Namespace, const FString& Key, const FString& Text) -> FText
	{
		// Call the internal function directly here so we don't gather this line for localization
		return FText::AsLocalizable_Advanced(*Namespace, *Key, Text);
	});
	SCRIPT_BIND_DOCUMENTATION(
	 "Function for using localization texts in Angelscript. Emulates NSLOCTEXT macro.\n"
	 "Only string literals can be used as input arguments from Angelscript.\n"
	 "Using variables (like FString) will run but will cause errors when strings are gathered for localization.")

});
