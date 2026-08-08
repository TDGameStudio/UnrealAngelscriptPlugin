#include "Bind_FName.h"

#include "AngelscriptEngine.h"

FString FNameType::GetAngelscriptTypeName() const
{
	return TEXT("FName");
}

bool FNameType::CanConstruct(const FAngelscriptTypeUsage& Usage) const
{
	return true;
}

bool FNameType::NeedConstruct(const FAngelscriptTypeUsage& Usage) const
{
	return true;
}

void FNameType::ConstructValue(const FAngelscriptTypeUsage& Usage, void* Address) const
{
	new(Address) FName();
}

bool FNameType::DefaultValue_UnrealToAngelscript(const FAngelscriptTypeUsage& Usage, const FString& InValue, FString& OutValue) const
{
	if (InValue == TEXT("None") || InValue == TEXT("NAME_None") || InValue.IsEmpty())
	{
		OutValue = TEXT("NAME_None");
		return true;
	}

	OutValue = FString::Printf(TEXT("FName(\"%s\")"), *OutValue);
	return true;
}

bool FNameType::DefaultValue_AngelscriptToUnreal(const FAngelscriptTypeUsage& Usage, const FString& InValue, FString& OutValue) const
{
	OutValue = InValue;
	OutValue.TrimStartAndEndInline();
	if (OutValue == TEXT("None") || OutValue == TEXT("NAME_None"))
	{
		OutValue = TEXT("None");
		return true;
	}

	if (OutValue.RemoveFromStart(TEXT("FName")))
	{
		OutValue.TrimStartAndEndInline();
		OutValue.RemoveFromStart(TEXT("("));
		OutValue.RemoveFromEnd(TEXT(")"));
	}

	// FName literals have been turned into __STATIC_NAME calls by the script preprocessor.
	if (OutValue.RemoveFromStart(TEXT("__STATIC_NAME (")))
	{
		int32 Index = -1;
		LexFromString(Index, *OutValue);

		FName StaticName;
		if (FAngelscriptEngine::TryGetStaticName(Index, StaticName))
		{
			OutValue = StaticName.ToString();
			return true;
		}

		return false;
	}

	OutValue.TrimStartAndEndInline();
	OutValue = OutValue.TrimQuotes();
	return true;
}

bool FNameType::GetDebuggerValue(const FAngelscriptTypeUsage& Usage, void* Address, FDebuggerValue& Value) const
{
	FName& NativeValue = Usage.ResolvePrimitive<FName>(Address);

	Value.Type = Usage.GetAngelscriptDeclaration();
	Value.Usage = Usage;
	Value.Address = Address;
	// The comparison index is stored at the start of FName but is not exposed directly.
	Value.SetAddressToMonitor(&NativeValue, sizeof(FNameEntryId));
	Value.Value = TEXT("n\"") + NativeValue.ToString() + TEXT("\"");
	return true;
}

bool FNameType::GetStringIdentifier(const FAngelscriptTypeUsage& Usage, void* Address, FString& OutString) const
{
	if (((FName*)Address)->GetNumber() != 0)
	{
		return false;
	}

	OutString = ((FName*)Address)->GetPlainNameString();
	return true;
}

bool FNameType::FromStringIdentifier(const FAngelscriptTypeUsage& Usage, const FString& InString, void* BufferPtr) const
{
	new(BufferPtr) FName(*InString);
	return true;
}

bool FNameType::GetCppForm(const FAngelscriptTypeUsage& Usage, FCppForm& OutCppForm) const
{
	OutCppForm.CppType = GetAngelscriptTypeName();
	return true;
}

bool FNameType::IsOrdered(const FAngelscriptTypeUsage& Usage) const
{
	return true;
}

int32 FNameType::CompareOrder(const FAngelscriptTypeUsage& Usage, void* Value, void* OtherValue) const
{
	FName& A = Usage.ResolvePrimitive<FName>(Value);
	FName& B = Usage.ResolvePrimitive<FName>(OtherValue);
	return A.Compare(B);
}
