#include "Bind_FVector3f.h"
#include "Misc/DefaultValueHelper.h"

FString FVector3fType::GetAngelscriptTypeName() const
{
	return TEXT("FVector3f");
}

void FVector3fType::ConstructValue(const FAngelscriptTypeUsage& Usage, void* DestinationPtr) const
{
	new(DestinationPtr) FVector3f(0.f, 0.f, 0.f);
}

bool FVector3fType::NeedConstruct(const FAngelscriptTypeUsage& Usage) const
{
	return false;
}

bool FVector3fType::NeedDestruct(const FAngelscriptTypeUsage& Usage) const
{
	return false;
}

bool FVector3fType::MatchesProperty(const FAngelscriptTypeUsage& Usage, const FProperty* Property, EPropertyMatchType MatchType) const
{
	const FStructProperty* StructProp = CastField<FStructProperty>(Property);
	if (StructProp == nullptr)
		return false;
	if (StructProp->Struct == GetStruct(Usage))
		return true;
	return false;
}

bool FVector3fType::DefaultValue_UnrealToAngelscript(const FAngelscriptTypeUsage& Usage, const FString& InValue, FString& OutValue) const
{
	if (InValue.IsEmpty())
	{
		OutValue = TEXT("FVector3f()");
		return true;
	}
	FVector3f Value;
	if (FDefaultValueHelper::ParseVector(InValue, Value))
	{
		OutValue = FString::Printf(TEXT("FVector3f(%f,%f,%f)"), Value.X, Value.Y, Value.Z);
		return true;
	}
	return false;
}

bool FVector3fType::DefaultValue_AngelscriptToUnreal(const FAngelscriptTypeUsage& Usage, const FString& CppForm, FString& OutForm) const
{
	FString Parameters;
	if(FDefaultValueHelper::Is( CppForm, TEXT("FVector3f :: ZeroVector") ))
	{
		return true;
	}
	else if(FDefaultValueHelper::Is(CppForm, TEXT("FVector3f :: UpVector")))
	{
		OutForm = FString::Printf(TEXT("%f,%f,%f"),
			FVector3f::UpVector.X, FVector3f::UpVector.Y, FVector3f::UpVector.Z);
	}
	else if(FDefaultValueHelper::Is(CppForm, TEXT("FVector3f :: ForwardVector")))
	{
		OutForm = FString::Printf(TEXT("%f,%f,%f"),
			FVector3f::ForwardVector.X, FVector3f::ForwardVector.Y, FVector3f::ForwardVector.Z);
	}
	else if(FDefaultValueHelper::Is(CppForm, TEXT("FVector3f :: RightVector")))
	{
		OutForm = FString::Printf(TEXT("%f,%f,%f"),
			FVector3f::RightVector.X, FVector3f::RightVector.Y, FVector3f::RightVector.Z);
	}
	else if(FDefaultValueHelper::Is(CppForm, TEXT("FVector3f :: OneVector")))
	{
		OutForm = FString::Printf(TEXT("%f,%f,%f"),
			FVector3f::OneVector.X, FVector3f::OneVector.Y, FVector3f::OneVector.Z);
	}
	else if( FDefaultValueHelper::GetParameters(CppForm, TEXT("FVector3f"), Parameters) )
	{
		FVector3f Vector;
		float Value;
		if (FDefaultValueHelper::ParseVector(Parameters, Vector))
		{
			OutForm = FString::Printf(TEXT("%f,%f,%f"),
				Vector.X, Vector.Y, Vector.Z);
		}
		else if (FDefaultValueHelper::ParseFloat(Parameters, Value))
		{
			OutForm = FString::Printf(TEXT("%f,%f,%f"),
				Value, Value, Value);
		}
	}

	return !OutForm.IsEmpty();
}

bool FVector3fType::GetCppForm(const FAngelscriptTypeUsage& Usage, FCppForm& OutCppForm) const
{
	OutCppForm.CppType = GetAngelscriptTypeName();
	return true;
}
