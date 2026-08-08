#include "Bind_FIntVector2.h"

UScriptStruct* FGetIntVector2::Get()
{
	static UScriptStruct* ScriptStruct = FindObject<UScriptStruct>(nullptr, TEXT("/Script/CoreUObject.IntVector2"));
	return ScriptStruct;
}

FString FIntVector2Type::GetAngelscriptTypeName() const
{
	return TEXT("FIntVector2");
}

void FIntVector2Type::ConstructValue(const FAngelscriptTypeUsage& Usage, void* DestinationPtr) const
{
	new (DestinationPtr) FIntVector2(0);
}

bool FIntVector2Type::NeedConstruct(const FAngelscriptTypeUsage& Usage) const
{
	return false;
}

bool FIntVector2Type::NeedDestruct(const FAngelscriptTypeUsage& Usage) const
{
	return false;
}

bool FIntVector2Type::GetCppForm(const FAngelscriptTypeUsage& Usage, FCppForm& OutCppForm) const
{
	OutCppForm.CppType = GetAngelscriptTypeName();
	return true;
}
