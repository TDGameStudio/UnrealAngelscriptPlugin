#include "Bind_FCollisionQueryParams.h"

FString FCollisionQueryParamsType::GetAngelscriptTypeName() const
{
	return TEXT("FCollisionQueryParams");
}

bool FCollisionQueryParamsType::GetCppForm(const FAngelscriptTypeUsage& Usage, FCppForm& OutCppForm) const
{
	OutCppForm.CppType = GetAngelscriptTypeName();
	return true;
}

FString FCollisionEnabledMaskType::GetAngelscriptTypeName() const
{
	return TEXT("FCollisionEnabledMask");
}

bool FCollisionEnabledMaskType::GetCppForm(const FAngelscriptTypeUsage& Usage, FCppForm& OutCppForm) const
{
	OutCppForm.CppType = GetAngelscriptTypeName();
	return true;
}

FString FComponentQueryParamsType::GetAngelscriptTypeName() const
{
	return TEXT("FComponentQueryParams");
}

bool FComponentQueryParamsType::GetCppForm(const FAngelscriptTypeUsage& Usage, FCppForm& OutCppForm) const
{
	OutCppForm.CppType = GetAngelscriptTypeName();
	return true;
}

FString FCollisionResponseParamsType::GetAngelscriptTypeName() const
{
	return TEXT("FCollisionResponseParams");
}

bool FCollisionResponseParamsType::GetCppForm(const FAngelscriptTypeUsage& Usage, FCppForm& OutCppForm) const
{
	OutCppForm.CppType = GetAngelscriptTypeName();
	return true;
}

FString FCollisionObjectQueryParamsType::GetAngelscriptTypeName() const
{
	return TEXT("FCollisionObjectQueryParams");
}

bool FCollisionObjectQueryParamsType::GetCppForm(const FAngelscriptTypeUsage& Usage, FCppForm& OutCppForm) const
{
	OutCppForm.CppType = GetAngelscriptTypeName();
	return true;
}
