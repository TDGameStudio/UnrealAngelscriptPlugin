#include "Bind_FQuat4f.h"

FString FQuat4fType::GetAngelscriptTypeName() const
{
	return TEXT("FQuat4f");
}

void FQuat4fType::ConstructValue(const FAngelscriptTypeUsage& Usage, void* DestinationPtr) const
{
	new (DestinationPtr) FQuat4f(0.f, 0.f, 0.f, 1.f);
}

bool FQuat4fType::GetCppForm(const FAngelscriptTypeUsage& Usage, FCppForm& OutCppForm) const
{
	OutCppForm.CppType = GetAngelscriptTypeName();
	return true;
}
