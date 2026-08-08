#include "Bind_FVector2f.h"
#include "Misc/DefaultValueHelper.h"
#include "Types/SlateVector2.h"

FString FVector2fType::GetAngelscriptTypeName() const
{
	return TEXT("FVector2f");
}

bool FVector2fType::MatchesProperty(const FAngelscriptTypeUsage& Usage, const FProperty* Property, EPropertyMatchType MatchType) const
{
	const FStructProperty* StructProp = CastField<FStructProperty>(Property);
	if (StructProp == nullptr)
		return false;
	if (StructProp->Struct == GetStruct(Usage))
		return true;
	if (StructProp->Struct == FDeprecateSlateVector2D::StaticStruct())
		return true;
	return false;
}

void FVector2fType::ConstructValue(const FAngelscriptTypeUsage& Usage, void* DestinationPtr) const
{
	new (DestinationPtr) FVector2f(0.f, 0.f);
}

bool FVector2fType::NeedConstruct(const FAngelscriptTypeUsage& Usage) const
{
	return false;
}

bool FVector2fType::NeedDestruct(const FAngelscriptTypeUsage& Usage) const
{
	return false;
}

bool FVector2fType::DefaultValue_UnrealToAngelscript(const FAngelscriptTypeUsage& Usage, const FString& InValue, FString& OutValue) const
{
	if (InValue.IsEmpty())
	{
		OutValue = TEXT("FVector2f()");
		return true;
	}
	FVector2f Value;
	if (FDefaultValueHelper::ParseVector2D(InValue, Value))
	{
		OutValue = FString::Printf(TEXT("FVector2f(%f,%f)"), Value.X, Value.Y);
		return true;
	}
	return false;
}

bool FVector2fType::DefaultValue_AngelscriptToUnreal(const FAngelscriptTypeUsage& Usage, const FString& CppForm, FString& OutForm) const
{
	FString Parameters;
	if (FDefaultValueHelper::Is(CppForm, TEXT("FVector2f :: ZeroVector")))
	{
		return true;
	}
	if (FDefaultValueHelper::Is(CppForm, TEXT("FVector2f :: UnitVector")))
	{
		OutForm = FString::Printf(TEXT("%f,%f"), FVector2f::UnitVector.X, FVector2f::UnitVector.Y);
	}
	else if (FDefaultValueHelper::GetParameters(CppForm, TEXT("FVector2f"), Parameters))
	{
		FVector2f Vector;
		float Value;
		if (FDefaultValueHelper::ParseVector2D(Parameters, Vector))
		{
			OutForm = FString::Printf(TEXT("%f,%f"), Vector.X, Vector.Y);
		}
		else if (FDefaultValueHelper::ParseFloat(Parameters, Value))
		{
			OutForm = FString::Printf(TEXT("%f,%f"), Value, Value);
		}
	}

	return !OutForm.IsEmpty();
}

bool FVector2fType::GetCppForm(const FAngelscriptTypeUsage& Usage, FCppForm& OutCppForm) const
{
	OutCppForm.CppType = GetAngelscriptTypeName();
	return true;
}
