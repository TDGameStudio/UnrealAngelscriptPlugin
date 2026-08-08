#include "Bind_FBoxSphereBounds3f.h"

UScriptStruct* FGetBoxSphereBounds3f::Get()
{
	static UScriptStruct* ScriptStruct = FindObject<UScriptStruct>(nullptr, TEXT("/Script/CoreUObject.BoxSphereBounds3f"));
	return ScriptStruct;
}

FString FBoxSphereBounds3fType::GetAngelscriptTypeName() const
{
	return TEXT("FBoxSphereBounds3f");
}

bool FBoxSphereBounds3fType::GetCppForm(const FAngelscriptTypeUsage& Usage, FCppForm& OutCppForm) const
{
	OutCppForm.CppType = GetAngelscriptTypeName();
	return true;
}
