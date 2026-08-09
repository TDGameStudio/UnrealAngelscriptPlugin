#include "Bind_AVolume.h"

#include "AngelscriptBinds.h"

#include "GameFramework/Volume.h"

/**
 * AVolume binding surface.
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | AngelScript usage signature                                                                          | Purpose / parameter notes                                                                                        |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FBoxSphereBounds Volume.GetBounds() const;                                                           | Returns the volume bounds.                                                                                       |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | bool Volume.EncompassesPoint(const FVector& Point, float32 SphereRadius = 0.f) const;                | Reports whether the volume encompasses the point and optional sphere radius.                                     |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | bool Volume.EncompassesPoint(const FVector& Point,                                                   | Reports containment and writes the distance from the volume to the point.                                        |
 * |     float32 SphereRadius,                                                                            | @param OutDistanceToPoint Receives the distance from the volume boundary when the point is outside.              |
 * |     float32& OutDistanceToPoint) const;                                                              |                                                                                                                  |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | void Volume.SetBrushColor(FLinearColor InBrushColor);                                                | Sets the volume brush color.                                                                                     |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 */

AS_FORCE_LINK const FAngelscriptBind Bind_AVolume(
	TEXT("AVolume"),
	EAngelscriptBindPhase::ExplicitBindings,
	[](FAngelscriptBinds& Binds)
	{
		auto AVolume_ = Binds.ExistingClassForTarget("AVolume");
		AVolume_.Method("FBoxSphereBounds GetBounds() const", METHOD_TRIVIAL(AVolume, GetBounds));
		AVolume_.Method(
			"bool EncompassesPoint(const FVector& Point, float32 SphereRadius = 0.f) const",
			&FAngelscriptAVolumeBinds::EncompassesPoint);
		AVolume_.Method(
			"bool EncompassesPoint(const FVector& Point, float32 SphereRadius, float32& OutDistanceToPoint) const",
			&FAngelscriptAVolumeBinds::EncompassesPointWithDistance);
		AVolume_.Method("void SetBrushColor(FLinearColor InBrushColor)", &FAngelscriptAVolumeBinds::SetBrushColor);
	});
