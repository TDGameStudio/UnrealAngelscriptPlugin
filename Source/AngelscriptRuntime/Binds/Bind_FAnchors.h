#pragma once

#include "Widgets/Layout/Anchors.h"

struct FAngelscriptFAnchorsBinds
{
	static void ConstructUniform(FAnchors* Address, float UniformAnchors);
	static void ConstructPoint(FAnchors* Address, float Horizontal, float Vertical);
	static void ConstructRange(FAnchors* Address, float MinX, float MinY, float MaxX, float MaxY);
};
