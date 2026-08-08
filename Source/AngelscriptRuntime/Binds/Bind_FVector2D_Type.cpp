#include "Bind_FVector2D.h"
#include "Misc/DefaultValueHelper.h"

FString FVector2DType::GetAngelscriptTypeName() const
{
	return TEXT("FVector2D");
}

void FVector2DType::ConstructValue(const FAngelscriptTypeUsage& Usage, void* DestinationPtr) const
{
	new (DestinationPtr) FVector2D(0.f, 0.f);
}

bool FVector2DType::NeedConstruct(const FAngelscriptTypeUsage& Usage) const
{
	return false;
}

bool FVector2DType::NeedDestruct(const FAngelscriptTypeUsage& Usage) const
{
	return false;
}

bool FVector2DType::DefaultValue_UnrealToAngelscript(const FAngelscriptTypeUsage& Usage, const FString& InValue, FString& OutValue) const
{
	if (InValue.IsEmpty())
	{
		OutValue = TEXT("FVector2D()");
		return true;
	}
	FVector2D Value;
	if (FDefaultValueHelper::ParseVector2D(InValue, Value))
	{
		OutValue = FString::Printf(TEXT("FVector2D(%f,%f)"), Value.X, Value.Y);
		return true;
	}
	return false;
}

bool FVector2DType::DefaultValue_AngelscriptToUnreal(const FAngelscriptTypeUsage& Usage, const FString& CppForm, FString& OutForm) const
{
	FString Parameters;
	if (FDefaultValueHelper::Is(CppForm, TEXT("FVector2D :: ZeroVector")))
	{
		return true;
	}
	if (FDefaultValueHelper::Is(CppForm, TEXT("FVector2D :: UnitVector")))
	{
		OutForm = FString::Printf(TEXT("%f,%f"), FVector2D::UnitVector.X, FVector2D::UnitVector.Y);
	}
	else if (FDefaultValueHelper::GetParameters(CppForm, TEXT("FVector2D"), Parameters))
	{
		FVector2D Vector;
		double Value;
		if (FDefaultValueHelper::ParseVector2D(Parameters, Vector))
		{
			OutForm = FString::Printf(TEXT("%f,%f"), Vector.X, Vector.Y);
		}
		else if (FDefaultValueHelper::ParseDouble(Parameters, Value))
		{
			OutForm = FString::Printf(TEXT("%f,%f"), Value, Value);
		}
	}

	return !OutForm.IsEmpty();
}

bool FVector2DType::GetCppForm(const FAngelscriptTypeUsage& Usage, FCppForm& OutCppForm) const
{
	OutCppForm.CppType = GetAngelscriptTypeName();
	return true;
}
