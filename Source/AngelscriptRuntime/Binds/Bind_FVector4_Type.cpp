#include "Bind_FVector4.h"

FString FVector4Type::GetAngelscriptTypeName() const
{
	return TEXT("FVector4");
}

void FVector4Type::ConstructValue(const FAngelscriptTypeUsage& Usage, void* DestinationPtr) const
{
	new (DestinationPtr) FVector4(0.f, 0.f, 0.f, 0.f);
}

bool FVector4Type::NeedConstruct(const FAngelscriptTypeUsage& Usage) const
{
	return false;
}

bool FVector4Type::NeedDestruct(const FAngelscriptTypeUsage& Usage) const
{
	return false;
}

bool FVector4Type::GetCppForm(const FAngelscriptTypeUsage& Usage, FCppForm& OutCppForm) const
{
	OutCppForm.CppType = GetAngelscriptTypeName();
	return true;
}
