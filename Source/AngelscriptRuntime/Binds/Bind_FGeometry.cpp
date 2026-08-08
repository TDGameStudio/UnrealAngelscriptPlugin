#include "AngelscriptBinds.h"

#include "Bind_FGeometry_Functions.h"

namespace
{
	void BindFGeometry(FAngelscriptBinds& Binds)
	{
		auto FGeometry_ = Binds.ExistingClassForTarget("FGeometry");
		FGeometry_.Method("FVector2D GetLocalSize() const", &FAngelscriptFGeometryBinds::GetLocalSize);
		FGeometry_.Method("FVector2D GetAbsoluteSize() const", &FAngelscriptFGeometryBinds::GetAbsoluteSize);
		FGeometry_.Method("FVector2D AbsoluteToLocal(const FVector2D& Position) const", &FAngelscriptFGeometryBinds::AbsoluteToLocal);
		FGeometry_.Method("FVector2D LocalToAbsolute(const FVector2D& Position) const", &FAngelscriptFGeometryBinds::LocalToAbsolute);
		FGeometry_.Method("FGeometry MakeChild(const FVector2D& Position, const FVector2D& Size) const", &FAngelscriptFGeometryBinds::MakeChild);
	}
}

AS_FORCE_LINK const FAngelscriptBind Bind_FGeometry(
	TEXT("FGeometry"),
	EAngelscriptBindPhase::ManualBindings,
	&BindFGeometry);
