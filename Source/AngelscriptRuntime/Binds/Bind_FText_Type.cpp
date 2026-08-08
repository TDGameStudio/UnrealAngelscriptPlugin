#include "Bind_FText.h"

FString FTextType::GetAngelscriptTypeName() const
{
	return TEXT("FText");
}

bool FTextType::GetDebuggerValue(const FAngelscriptTypeUsage& Usage, void* Address, FDebuggerValue& Value) const
{
	FText& NativeValue = Usage.ResolvePrimitive<FText>(Address);

	Value.Type = Usage.GetAngelscriptDeclaration();
	Value.Usage = Usage;
	Value.Address = Address;
	Value.Value = TEXT("FText: \"") + NativeValue.ToString() + TEXT("\"");
	return true;
}

bool FTextType::GetCppForm(const FAngelscriptTypeUsage& Usage, FCppForm& OutCppForm) const
{
	OutCppForm.CppType = GetAngelscriptTypeName();
	return true;
}

bool FTextType::CanCompare(const FAngelscriptTypeUsage& Usage) const
{
	return true;
}

bool FTextType::IsValueEqual(const FAngelscriptTypeUsage& Usage, void* SourcePtr, void* DestinationPtr) const
{
	return static_cast<FText*>(SourcePtr)->IdenticalTo(*static_cast<FText*>(DestinationPtr));
}
