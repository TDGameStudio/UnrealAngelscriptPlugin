#include "Bind_FRandomStream.h"

FString FRandomStreamType::GetAngelscriptTypeName() const
{
	return TEXT("FRandomStream");
}

bool FRandomStreamType::GetCppForm(const FAngelscriptTypeUsage& Usage, FCppForm& OutCppForm) const
{
	OutCppForm.CppType = GetAngelscriptTypeName();
	return true;
}
