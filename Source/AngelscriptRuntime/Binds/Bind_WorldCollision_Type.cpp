#include "Bind_WorldCollision.h"

FString FTraceHandleType::GetAngelscriptTypeName() const
{
	return TEXT("FTraceHandle");
}

bool FTraceHandleType::GetCppForm(const FAngelscriptTypeUsage& Usage, FCppForm& OutCppForm) const
{
	OutCppForm.CppType = GetAngelscriptTypeName();
	return true;
}

bool FTraceHandleType::NeverRequiresGC(const FAngelscriptTypeUsage& Usage) const
{
	return true;
}

FString FTraceDatumType::GetAngelscriptTypeName() const
{
	return TEXT("FTraceDatum");
}

bool FTraceDatumType::GetCppForm(const FAngelscriptTypeUsage& Usage, FCppForm& OutCppForm) const
{
	OutCppForm.CppType = GetAngelscriptTypeName();
	return true;
}

bool FTraceDatumType::NeverRequiresGC(const FAngelscriptTypeUsage& Usage) const
{
	return true;
}

FString FOverlapDatumType::GetAngelscriptTypeName() const
{
	return TEXT("FOverlapDatum");
}

bool FOverlapDatumType::GetCppForm(const FAngelscriptTypeUsage& Usage, FCppForm& OutCppForm) const
{
	OutCppForm.CppType = GetAngelscriptTypeName();
	return true;
}

bool FOverlapDatumType::NeverRequiresGC(const FAngelscriptTypeUsage& Usage) const
{
	return true;
}
