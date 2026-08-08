#pragma once

#include "CoreMinimal.h"

class AVolume;

struct FAngelscriptAVolumeBinds
{
	static bool EncompassesPoint(AVolume* Volume, const FVector& Point, float SphereRadius);
	static bool EncompassesPointWithDistance(AVolume* Volume, const FVector& Point, float SphereRadius, float& OutDistanceToPoint);
	static void SetBrushColor(AVolume* Volume, FLinearColor BrushColor);
};
