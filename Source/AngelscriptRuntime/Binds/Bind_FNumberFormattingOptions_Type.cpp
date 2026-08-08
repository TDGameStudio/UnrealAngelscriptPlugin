#include "Bind_FNumberFormattingOptions.h"

FString FNumberFormattingOptionsType::GetAngelscriptTypeName() const
{
	return TEXT("FNumberFormattingOptions");
}

bool FNumberFormattingOptionsType::GetCppForm(const FAngelscriptTypeUsage& Usage, FCppForm& OutCppForm) const
{
	OutCppForm.CppType = GetAngelscriptTypeName();
	return true;
}
