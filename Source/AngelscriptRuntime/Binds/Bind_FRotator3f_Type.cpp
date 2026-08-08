#include "Bind_FRotator3f.h"
#include "Misc/DefaultValueHelper.h"

FString FRotator3fType::GetAngelscriptTypeName() const
{
	return TEXT("FRotator3f");
}

void FRotator3fType::ConstructValue(const FAngelscriptTypeUsage& Usage, void* DestinationPtr) const
{
	new (DestinationPtr) FRotator3f(0.f);
}

bool FRotator3fType::NeedConstruct(const FAngelscriptTypeUsage& Usage) const
{
	return false;
}

bool FRotator3fType::NeedDestruct(const FAngelscriptTypeUsage& Usage) const
{
	return false;
}

bool FRotator3fType::DefaultValue_UnrealToAngelscript(
		const FAngelscriptTypeUsage& Usage,
		const FString& InValue,
		FString& OutValue) const
{
	if (InValue.IsEmpty())
	{
		OutValue = TEXT("FRotator3f()");
		return true;
	}
	FRotator3f Value;
	if (FDefaultValueHelper::ParseRotator(InValue, Value))
	{
		OutValue = FString::Printf(TEXT("FRotator3f(%f,%f,%f)"), Value.Pitch, Value.Yaw, Value.Roll);
		return true;
	}
	return false;
}

bool FRotator3fType::DefaultValue_AngelscriptToUnreal(
		const FAngelscriptTypeUsage& Usage,
		const FString& CppForm,
		FString& OutForm) const
{
	if (FDefaultValueHelper::Is(CppForm, TEXT("FRotator3f :: ZeroRotator")))
	{
		return true;
	}
	FString Parameters;
	if (FDefaultValueHelper::GetParameters(CppForm, TEXT("FRotator3f"), Parameters))
	{
		FRotator3f Rotator;
		if (FDefaultValueHelper::ParseRotator(Parameters, Rotator))
		{
			OutForm = FString::Printf(TEXT("%f,%f,%f"), Rotator.Pitch, Rotator.Yaw, Rotator.Roll);
		}
	}
	return !OutForm.IsEmpty();
}

bool FRotator3fType::GetCppForm(const FAngelscriptTypeUsage& Usage, FCppForm& OutCppForm) const
{
	OutCppForm.CppType = GetAngelscriptTypeName();
	return true;
}
