#include "Bind_FLinearColor.h"
#include "Misc/DefaultValueHelper.h"

FString FLinearColorType::GetAngelscriptTypeName() const
{
	return TEXT("FLinearColor");
}

void FLinearColorType::ConstructValue(const FAngelscriptTypeUsage& Usage, void* DestinationPtr) const
{
	new(DestinationPtr) FLinearColor(0.f, 0.f, 0.f, 1.f);
}

bool FLinearColorType::DefaultValue_UnrealToAngelscript(
	const FAngelscriptTypeUsage& Usage,
	const FString& InValue,
	FString& OutValue) const
{
	if (InValue.IsEmpty())
	{
		OutValue = TEXT("FLinearColor()");
		return true;
	}
	FLinearColor Value;
	if (Value.InitFromString(InValue))
	{
		OutValue = FString::Printf(TEXT("FLinearColor(%f,%f,%f,%f)"), Value.R, Value.G, Value.B, Value.A);
		return true;
	}
	return false;
}

bool FLinearColorType::DefaultValue_AngelscriptToUnreal(
	const FAngelscriptTypeUsage& Usage,
	const FString& CppForm,
	FString& OutForm) const
{
	if (FDefaultValueHelper::Is(CppForm, TEXT("FLinearColor :: White")))
	{
		OutForm = FLinearColor::White.ToString();
	}
	else if (FDefaultValueHelper::Is(CppForm, TEXT("FLinearColor :: Gray")))
	{
		OutForm = FLinearColor::Gray.ToString();
	}
	else if (FDefaultValueHelper::Is(CppForm, TEXT("FLinearColor :: Black")))
	{
		OutForm = FLinearColor::Black.ToString();
	}
	else if (FDefaultValueHelper::Is(CppForm, TEXT("FLinearColor :: Transparent")))
	{
		OutForm = FLinearColor::Transparent.ToString();
	}
	else if (FDefaultValueHelper::Is(CppForm, TEXT("FLinearColor :: Red")))
	{
		OutForm = FLinearColor::Red.ToString();
	}
	else if (FDefaultValueHelper::Is(CppForm, TEXT("FLinearColor :: Green")))
	{
		OutForm = FLinearColor::Green.ToString();
	}
	else if (FDefaultValueHelper::Is(CppForm, TEXT("FLinearColor :: Blue")))
	{
		OutForm = FLinearColor::Blue.ToString();
	}
	else if (FDefaultValueHelper::Is(CppForm, TEXT("FLinearColor :: Yellow")))
	{
		OutForm = FLinearColor::Yellow.ToString();
	}
	else
	{
		FString Parameters;
		if (FDefaultValueHelper::GetParameters(CppForm, TEXT("FLinearColor"), Parameters))
		{
			FLinearColor Color;
			if (FDefaultValueHelper::ParseLinearColor(Parameters, Color))
			{
				OutForm = Color.ToString();
			}
		}
	}

	return !OutForm.IsEmpty();
}

bool FLinearColorType::GetCppForm(const FAngelscriptTypeUsage& Usage, FCppForm& OutCppForm) const
{
	OutCppForm.CppType = GetAngelscriptTypeName();
	return true;
}
