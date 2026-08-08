#include "Bind_FMargin_Functions.h"

void FAngelscriptFMarginBinds::ConstructUniform(FMargin* Address, float UniformMargin)
{
	new (Address) FMargin(UniformMargin);
}

void FAngelscriptFMarginBinds::ConstructHorizontalVertical(FMargin* Address, float Horizontal, float Vertical)
{
	new (Address) FMargin(Horizontal, Vertical);
}

void FAngelscriptFMarginBinds::ConstructFromVector2D(FMargin* Address, const FVector2D& Vector)
{
	new (Address) FMargin(Vector);
}

void FAngelscriptFMarginBinds::ConstructLTRB(FMargin* Address, float Left, float Top, float Right, float Bottom)
{
	new (Address) FMargin(Left, Top, Right, Bottom);
}

void FAngelscriptFMarginBinds::ConstructFromVector4(FMargin* Address, const FVector4& Vector)
{
	new (Address) FMargin(Vector);
}

float FAngelscriptFMarginBinds::GetTotalSpaceAlongHorizontal(const FMargin& Margin)
{
	return Margin.GetTotalSpaceAlong<Orient_Horizontal>();
}

float FAngelscriptFMarginBinds::GetTotalSpaceAlongVertical(const FMargin& Margin)
{
	return Margin.GetTotalSpaceAlong<Orient_Vertical>();
}
