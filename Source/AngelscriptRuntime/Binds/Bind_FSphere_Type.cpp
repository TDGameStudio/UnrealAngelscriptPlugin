#include "Bind_FSphere.h"

UScriptStruct* FGetSphere::Get()
{
	static UScriptStruct* ScriptStruct = FindObject<UScriptStruct>(nullptr, TEXT("/Script/CoreUObject.Sphere"));
	return ScriptStruct;
}

FString FSphereType::GetAngelscriptTypeName() const
{
	return TEXT("FSphere");
}

bool FSphereType::GetCppForm(const FAngelscriptTypeUsage& Usage, FCppForm& OutCppForm) const
{
	OutCppForm.CppType = GetAngelscriptTypeName();
	return true;
}
