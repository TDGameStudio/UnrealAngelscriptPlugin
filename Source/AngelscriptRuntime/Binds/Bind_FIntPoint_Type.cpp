#include "Bind_FIntPoint.h"

FString FIntPointType::GetAngelscriptTypeName() const
{
	return TEXT("FIntPoint");
}

void FIntPointType::ConstructValue(const FAngelscriptTypeUsage& Usage, void* DestinationPtr) const
{
	new (DestinationPtr) FIntPoint(0);
}

bool FIntPointType::NeedConstruct(const FAngelscriptTypeUsage& Usage) const
{
	return false;
}

bool FIntPointType::NeedDestruct(const FAngelscriptTypeUsage& Usage) const
{
	return false;
}

bool FIntPointType::GetCppForm(const FAngelscriptTypeUsage& Usage, FCppForm& OutCppForm) const
{
	OutCppForm.CppType = GetAngelscriptTypeName();
	return true;
}
