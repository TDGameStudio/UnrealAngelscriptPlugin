#include "Bind_FString.h"

FString FStringType::GetAngelscriptTypeName() const
{
	return TEXT("FString");
}

bool FStringType::DefaultValue_UnrealToAngelscript(const FAngelscriptTypeUsage& Usage, const FString& InValue, FString& OutValue) const
{
	OutValue = FString::Printf(TEXT("\"%s\""), *InValue);
	return true;
}

bool FStringType::DefaultValue_AngelscriptToUnreal(const FAngelscriptTypeUsage& Usage, const FString& InValue, FString& OutValue) const
{
	OutValue = InValue.TrimQuotes();
	return true;
}

bool FStringType::GetDebuggerValue(const FAngelscriptTypeUsage& Usage, void* Address, FDebuggerValue& Value) const
{
	FString& NativeValue = Usage.ResolvePrimitive<FString>(Address);

	Value.Type = Usage.GetAngelscriptDeclaration();
	Value.Usage = Usage;
	Value.Address = Address;
	Value.Value = TEXT("\"") + NativeValue + TEXT("\"");
	return true;
}

bool FStringType::GetStringIdentifier(const FAngelscriptTypeUsage& Usage, void* Address, FString& OutString) const
{
	OutString = *(FString*)Address;
	return true;
}

bool FStringType::FromStringIdentifier(const FAngelscriptTypeUsage& Usage, const FString& InString, void* BufferPtr) const
{
	new(BufferPtr) FString(InString);
	return true;
}

bool FStringType::GetCppForm(const FAngelscriptTypeUsage& Usage, FCppForm& OutCppForm) const
{
	OutCppForm.CppType = GetAngelscriptTypeName();
	return true;
}

bool FStringType::IsOrdered(const FAngelscriptTypeUsage& Usage) const
{
	return true;
}

int32 FStringType::CompareOrder(const FAngelscriptTypeUsage& Usage, void* Value, void* OtherValue) const
{
	FString& A = Usage.ResolvePrimitive<FString>(Value);
	FString& B = Usage.ResolvePrimitive<FString>(OtherValue);
	return A.Compare(B);
}
