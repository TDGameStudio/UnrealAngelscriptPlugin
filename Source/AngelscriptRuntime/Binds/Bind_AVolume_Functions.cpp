#include "Bind_AVolume.h"

#include "GameFramework/Volume.h"

bool FAngelscriptAVolumeBinds::EncompassesPoint(AVolume* Volume, const FVector& Point, float SphereRadius)
{
	return Volume->EncompassesPoint(Point, SphereRadius);
}

bool FAngelscriptAVolumeBinds::EncompassesPointWithDistance(
	AVolume* Volume,
	const FVector& Point,
	float SphereRadius,
	float& OutDistanceToPoint)
{
	return Volume->EncompassesPoint(Point, SphereRadius, &OutDistanceToPoint);
}

void FAngelscriptAVolumeBinds::SetBrushColor(AVolume* Volume, FLinearColor BrushColor)
{
	Volume->BrushColor = BrushColor.ToFColor(true);
	Volume->bColored = true;
}
