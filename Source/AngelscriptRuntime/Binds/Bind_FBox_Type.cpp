#include "Bind_FBox.h"

FString FBoxType::GetAngelscriptTypeName() const
{
	return TEXT("FBox");
}

bool FBoxType::GetCppForm(const FAngelscriptTypeUsage& Usage, FCppForm& OutCppForm) const
{
	OutCppForm.CppType = GetAngelscriptTypeName();
	return true;
}
