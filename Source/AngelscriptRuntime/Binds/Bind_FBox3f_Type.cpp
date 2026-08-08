#include "Bind_FBox3f.h"

UScriptStruct* FGetBox3f::Get()
{
	static UScriptStruct* ScriptStruct = FindObject<UScriptStruct>(nullptr, TEXT("/Script/CoreUObject.Box3f"));
	return ScriptStruct;
}

FString FBox3fType::GetAngelscriptTypeName() const
{
	return TEXT("FBox3f");
}

bool FBox3fType::GetCppForm(const FAngelscriptTypeUsage& Usage, FCppForm& OutCppForm) const
{
	OutCppForm.CppType = GetAngelscriptTypeName();
	return true;
}
