#include "Bind_FCollisionShape.h"

FString FCollisionShapeType::GetAngelscriptTypeName() const
{
	return TEXT("FCollisionShape");
}

bool FCollisionShapeType::GetCppForm(const FAngelscriptTypeUsage& Usage, FCppForm& OutCppForm) const
{
	OutCppForm.CppType = GetAngelscriptTypeName();
	return true;
}
