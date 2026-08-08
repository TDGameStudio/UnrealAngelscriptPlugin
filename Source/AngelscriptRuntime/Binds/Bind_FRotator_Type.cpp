#include "Bind_FRotator.h"
#include "Misc/DefaultValueHelper.h"

FString FRotatorType::GetAngelscriptTypeName() const
{
	return TEXT("FRotator");
}

void FRotatorType::ConstructValue(const FAngelscriptTypeUsage& Usage, void* DestinationPtr) const
{
	new (DestinationPtr) FRotator(0.f);
}

bool FRotatorType::NeedConstruct(const FAngelscriptTypeUsage& Usage) const
{
	return false;
}

bool FRotatorType::NeedDestruct(const FAngelscriptTypeUsage& Usage) const
{
	return false;
}

bool FRotatorType::DefaultValue_UnrealToAngelscript(
		const FAngelscriptTypeUsage& Usage,
		const FString& InValue,
		FString& OutValue) const
{
	if (InValue.IsEmpty())
	{
		OutValue = TEXT("FRotator()");
		return true;
	}

	FRotator Value;
	if (FDefaultValueHelper::ParseRotator(InValue, Value))
	{
		OutValue = FString::Printf(TEXT("FRotator(%f,%f,%f)"), Value.Pitch, Value.Yaw, Value.Roll);
		return true;
	}
	return false;
}

bool FRotatorType::DefaultValue_AngelscriptToUnreal(
		const FAngelscriptTypeUsage& Usage,
		const FString& CppForm,
		FString& OutForm) const
{
	if (FDefaultValueHelper::Is(CppForm, TEXT("FRotator :: ZeroRotator")))
	{
		return true;
	}

	FString Parameters;
	if (FDefaultValueHelper::GetParameters(CppForm, TEXT("FRotator"), Parameters))
	{
		FRotator Rotator;
		if (FDefaultValueHelper::ParseRotator(Parameters, Rotator))
		{
			OutForm = FString::Printf(TEXT("%f,%f,%f"), Rotator.Pitch, Rotator.Yaw, Rotator.Roll);
		}
	}
	return !OutForm.IsEmpty();
}

bool FRotatorType::GetCppForm(const FAngelscriptTypeUsage& Usage, FCppForm& OutCppForm) const
{
	OutCppForm.CppType = GetAngelscriptTypeName();
	return true;
}
