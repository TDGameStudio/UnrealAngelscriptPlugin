#include "Bind_FQuat.h"

FString FQuatType::GetAngelscriptTypeName() const
{
	return TEXT("FQuat");
}

void FQuatType::ConstructValue(const FAngelscriptTypeUsage& Usage, void* DestinationPtr) const
{
	new (DestinationPtr) FQuat(0.f, 0.f, 0.f, 1.f);
}

bool FQuatType::GetCppForm(const FAngelscriptTypeUsage& Usage, FCppForm& OutCppForm) const
{
	OutCppForm.CppType = GetAngelscriptTypeName();
	return true;
}
