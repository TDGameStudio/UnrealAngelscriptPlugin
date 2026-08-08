#include "Bind_FLinearColor_Functions.h"

void FAngelscriptFLinearColorBinds::ConstructDefault(FLinearColor* Address)
{
	new (Address) FLinearColor(0.f, 0.f, 0.f, 1.f);
}

void FAngelscriptFLinearColorBinds::ConstructRGBA(
	FLinearColor* Address,
	const float R,
	const float G,
	const float B,
	const float A)
{
	new (Address) FLinearColor(R, G, B, A);
}

void FAngelscriptFLinearColorBinds::ConstructCopy(FLinearColor* Address, const FLinearColor& Other)
{
	new (Address) FLinearColor(Other);
}

FLinearColor* FAngelscriptFLinearColorBinds::Assign(FLinearColor* Color, const FLinearColor& Other)
{
	*Color = Other;
	return Color;
}

FLinearColor FAngelscriptFLinearColorBinds::MakeFromHex(const uint32 HexColor, const bool bSRGB)
{
	if (bSRGB)
	{
		return FLinearColor(FColor(HexColor));
	}

	return FColor(HexColor).ReinterpretAsLinear();
}

void FAngelscriptFLinearColorBinds::ConstructFromVector(
	FLinearColor* Address,
	const FVector& Other,
	const float A)
{
	new (Address) FLinearColor(Other.X, Other.Y, Other.Z, A);
}

void FAngelscriptFLinearColorBinds::ConstructFromColor(FLinearColor* Address, const FColor& Other)
{
	new (Address) FLinearColor(Other);
}

void FAngelscriptFLinearColorBinds::AppendToString(void* Address, FString& OutString)
{
	OutString += static_cast<FLinearColor*>(Address)->ToString();
}
