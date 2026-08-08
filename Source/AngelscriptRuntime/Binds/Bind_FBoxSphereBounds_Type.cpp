#include "Bind_FBoxSphereBounds.h"

FString FBoxSphereBoundsType::GetAngelscriptTypeName() const
{
	return TEXT("FBoxSphereBounds");
}

bool FBoxSphereBoundsType::GetCppForm(const FAngelscriptTypeUsage& Usage, FCppForm& OutCppForm) const
{
	OutCppForm.CppType = GetAngelscriptTypeName();
	return true;
}
