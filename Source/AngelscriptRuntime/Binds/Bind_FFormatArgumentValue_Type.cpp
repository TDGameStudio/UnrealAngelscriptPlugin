#include "Bind_FFormatArgumentValue.h"

FString FFormatArgumentValueType::GetAngelscriptTypeName() const
{
	return TEXT("FFormatArgumentValue");
}

bool FFormatArgumentValueType::GetCppForm(const FAngelscriptTypeUsage& Usage, FCppForm& OutCppForm) const
{
	OutCppForm.CppType = GetAngelscriptTypeName();
	return true;
}
