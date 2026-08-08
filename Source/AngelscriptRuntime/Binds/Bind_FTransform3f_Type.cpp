#include "Bind_FTransform3f.h"

FString FTransform3fType::GetAngelscriptTypeName() const
{
	return TEXT("FTransform3f");
}

bool FTransform3fType::CanCompare(const FAngelscriptTypeUsage& Usage) const
{
	return false;
}

bool FTransform3fType::IsValueEqual(const FAngelscriptTypeUsage& Usage, void* SourcePtr, void* DestinationPtr) const
{
	return false;
}

bool FTransform3fType::GetCppForm(const FAngelscriptTypeUsage& Usage, FCppForm& OutCppForm) const
{
	OutCppForm.CppType = GetAngelscriptTypeName();
	return true;
}
