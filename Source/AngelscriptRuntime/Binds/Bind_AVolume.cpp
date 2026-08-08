#include "AngelscriptBinds.h"

#include "GameFramework/Volume.h"

#include "Bind_AVolume_Functions.h"

namespace
{
	void BindAVolume(FAngelscriptBinds& Binds)
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
	}
}

AS_FORCE_LINK const FAngelscriptBind Bind_AVolume(
	TEXT("AVolume"),
	EAngelscriptBindPhase::ManualBindings,
	&BindAVolume);
