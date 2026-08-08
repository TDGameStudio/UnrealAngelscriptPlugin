#pragma once

#include "CoreMinimal.h"
#include "Layout/Margin.h"

struct FAngelscriptFMarginBinds
{
	static void ConstructUniform(FMargin* Address, float UniformMargin);
	static void ConstructHorizontalVertical(FMargin* Address, float Horizontal, float Vertical);
	static void ConstructFromVector2D(FMargin* Address, const FVector2D& Vector);
	static void ConstructLTRB(FMargin* Address, float Left, float Top, float Right, float Bottom);
	static void ConstructFromVector4(FMargin* Address, const FVector4& Vector);
	static float GetTotalSpaceAlongHorizontal(const FMargin& Margin);
	static float GetTotalSpaceAlongVertical(const FMargin& Margin);
};
