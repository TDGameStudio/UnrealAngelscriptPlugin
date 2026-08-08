#include "Bind_FTransform.h"

FString FTransformType::GetAngelscriptTypeName() const
{
	return TEXT("FTransform");
}

bool FTransformType::CanCompare(const FAngelscriptTypeUsage& Usage) const
{
	return false;
}

bool FTransformType::IsValueEqual(const FAngelscriptTypeUsage& Usage, void* SourcePtr, void* DestinationPtr) const
{
	return false;
}

bool FTransformType::GetCppForm(const FAngelscriptTypeUsage& Usage, FCppForm& OutCppForm) const
{
	OutCppForm.CppType = GetAngelscriptTypeName();
	return true;
}
