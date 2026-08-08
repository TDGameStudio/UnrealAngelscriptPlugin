#include "Bind_FIntVector4.h"

FString FIntVector4Type::GetAngelscriptTypeName() const
{
	return TEXT("FIntVector4");
}

void FIntVector4Type::ConstructValue(const FAngelscriptTypeUsage& Usage, void* DestinationPtr) const
{
	new (DestinationPtr) FIntVector4(0);
}

bool FIntVector4Type::NeedConstruct(const FAngelscriptTypeUsage& Usage) const
{
	return false;
}

bool FIntVector4Type::NeedDestruct(const FAngelscriptTypeUsage& Usage) const
{
	return false;
}

bool FIntVector4Type::GetCppForm(const FAngelscriptTypeUsage& Usage, FCppForm& OutCppForm) const
{
	OutCppForm.CppType = GetAngelscriptTypeName();
	return true;
}
