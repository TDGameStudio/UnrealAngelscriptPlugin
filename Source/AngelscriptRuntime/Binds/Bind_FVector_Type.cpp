#include "Bind_FVector.h"
#include "Misc/DefaultValueHelper.h"
#include "Engine/NetSerialization.h"

FString FVectorType::GetAngelscriptTypeName() const
{
	return TEXT("FVector");
}

void FVectorType::ConstructValue(const FAngelscriptTypeUsage& Usage, void* DestinationPtr) const
{
	new(DestinationPtr) FVector(0.f, 0.f, 0.f);
}

bool FVectorType::NeedConstruct(const FAngelscriptTypeUsage& Usage) const
{
	return false;
}

bool FVectorType::NeedDestruct(const FAngelscriptTypeUsage& Usage) const
{
	return false;
}

bool FVectorType::MatchesProperty(const FAngelscriptTypeUsage& Usage, const FProperty* Property, EPropertyMatchType MatchType) const
{
	const FStructProperty* StructProp = CastField<FStructProperty>(Property);
	if (StructProp == nullptr)
		return false;
	if (StructProp->Struct == GetStruct(Usage))
		return true;
	if (StructProp->Struct == FVector_NetQuantize::StaticStruct())
		return true;
	if (StructProp->Struct == FVector_NetQuantize10::StaticStruct())
		return true;
	if (StructProp->Struct == FVector_NetQuantize100::StaticStruct())
		return true;
	if (StructProp->Struct == FVector_NetQuantizeNormal::StaticStruct())
		return true;
	return false;
}

bool FVectorType::DefaultValue_UnrealToAngelscript(const FAngelscriptTypeUsage& Usage, const FString& InValue, FString& OutValue) const
{
	if (InValue.IsEmpty())
	{
		OutValue = TEXT("FVector()");
		return true;
	}
	FVector Value;
	if (FDefaultValueHelper::ParseVector(InValue, Value))
	{
		OutValue = FString::Printf(TEXT("FVector(%f,%f,%f)"), Value.X, Value.Y, Value.Z);
		return true;
	}
	return false;
}

bool FVectorType::DefaultValue_AngelscriptToUnreal(const FAngelscriptTypeUsage& Usage, const FString& CppForm, FString& OutForm) const
{
	FString Parameters;
	if(FDefaultValueHelper::Is( CppForm, TEXT("FVector :: ZeroVector") ))
	{
		return true;
	}
	else if(FDefaultValueHelper::Is(CppForm, TEXT("FVector :: UpVector")))
	{
		OutForm = FString::Printf(TEXT("%f,%f,%f"),
			FVector::UpVector.X, FVector::UpVector.Y, FVector::UpVector.Z);
	}
	else if(FDefaultValueHelper::Is(CppForm, TEXT("FVector :: ForwardVector")))
	{
		OutForm = FString::Printf(TEXT("%f,%f,%f"),
			FVector::ForwardVector.X, FVector::ForwardVector.Y, FVector::ForwardVector.Z);
	}
	else if(FDefaultValueHelper::Is(CppForm, TEXT("FVector :: RightVector")))
	{
		OutForm = FString::Printf(TEXT("%f,%f,%f"),
			FVector::RightVector.X, FVector::RightVector.Y, FVector::RightVector.Z);
	}
	else if(FDefaultValueHelper::Is(CppForm, TEXT("FVector :: OneVector")))
	{
		OutForm = FString::Printf(TEXT("%f,%f,%f"),
			FVector::OneVector.X, FVector::OneVector.Y, FVector::OneVector.Z);
	}
	else if( FDefaultValueHelper::GetParameters(CppForm, TEXT("FVector"), Parameters) )
	{
		FVector Vector;
		double Value;
		if (FDefaultValueHelper::ParseVector(Parameters, Vector))
		{
			OutForm = FString::Printf(TEXT("%f,%f,%f"),
				Vector.X, Vector.Y, Vector.Z);
		}
		else if (FDefaultValueHelper::ParseDouble(Parameters, Value))
		{
			OutForm = FString::Printf(TEXT("%f,%f,%f"),
				Value, Value, Value);
		}
	}

	return !OutForm.IsEmpty();
}

bool FVectorType::GetCppForm(const FAngelscriptTypeUsage& Usage, FCppForm& OutCppForm) const
{
	OutCppForm.CppType = GetAngelscriptTypeName();
	return true;
}
