#include "Bind_FSphere3f.h"

UScriptStruct* FGetSphere3f::Get()
{
	static UScriptStruct* ScriptStruct = FindObject<UScriptStruct>(nullptr, TEXT("/Script/CoreUObject.Sphere3f"));
	return ScriptStruct;
}

FString FSphere3fType::GetAngelscriptTypeName() const
{
	return TEXT("FSphere3f");
}

bool FSphere3fType::GetCppForm(const FAngelscriptTypeUsage& Usage, FCppForm& OutCppForm) const
{
	OutCppForm.CppType = GetAngelscriptTypeName();
	return true;
}
