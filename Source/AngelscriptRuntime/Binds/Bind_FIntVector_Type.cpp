#include "Bind_FIntVector.h"

FString FIntVectorType::GetAngelscriptTypeName() const
{
	return TEXT("FIntVector");
}

void FIntVectorType::ConstructValue(const FAngelscriptTypeUsage& Usage, void* DestinationPtr) const
{
	new (DestinationPtr) FIntVector(0);
}

bool FIntVectorType::NeedConstruct(const FAngelscriptTypeUsage& Usage) const
{
	return false;
}

bool FIntVectorType::NeedDestruct(const FAngelscriptTypeUsage& Usage) const
{
	return false;
}

bool FIntVectorType::GetCppForm(const FAngelscriptTypeUsage& Usage, FCppForm& OutCppForm) const
{
	OutCppForm.CppType = GetAngelscriptTypeName();
	return true;
}
