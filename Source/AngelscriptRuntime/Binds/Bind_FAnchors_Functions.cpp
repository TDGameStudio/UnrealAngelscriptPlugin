#include "Bind_FAnchors.h"

void FAngelscriptFAnchorsBinds::ConstructUniform(FAnchors* Address, float UniformAnchors)
{
	new (Address) FAnchors(UniformAnchors);
}

void FAngelscriptFAnchorsBinds::ConstructPoint(FAnchors* Address, float Horizontal, float Vertical)
{
	new (Address) FAnchors(Horizontal, Vertical);
}

void FAngelscriptFAnchorsBinds::ConstructRange(FAnchors* Address, float MinX, float MinY, float MaxX, float MaxY)
{
	new (Address) FAnchors(MinX, MinY, MaxX, MaxY);
}
