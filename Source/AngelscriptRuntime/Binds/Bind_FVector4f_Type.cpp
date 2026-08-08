#include "Bind_FVector4f.h"

FString FVector4fType::GetAngelscriptTypeName() const
{
	return TEXT("FVector4f");
}

void FVector4fType::ConstructValue(const FAngelscriptTypeUsage& Usage, void* DestinationPtr) const
{
	new (DestinationPtr) FVector4f(0.f, 0.f, 0.f, 0.f);
}

bool FVector4fType::NeedConstruct(const FAngelscriptTypeUsage& Usage) const
{
	return false;
}

bool FVector4fType::NeedDestruct(const FAngelscriptTypeUsage& Usage) const
{
	return false;
}

bool FVector4fType::GetCppForm(const FAngelscriptTypeUsage& Usage, FCppForm& OutCppForm) const
{
	OutCppForm.CppType = GetAngelscriptTypeName();
	return true;
}
