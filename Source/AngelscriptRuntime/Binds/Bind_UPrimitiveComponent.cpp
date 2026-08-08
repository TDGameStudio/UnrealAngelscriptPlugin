#include "AngelscriptBinds.h"

#include "Bind_UPrimitiveComponent_Functions.h"

namespace
{
	void BindUPrimitiveComponent(FAngelscriptBinds& Binds)
	{
		auto PrimitiveComponent_ = Binds.ExistingClassForTarget("UPrimitiveComponent");
		PrimitiveComponent_.Method("FVector GetBoundingBoxExtents() const", &FAngelscriptUPrimitiveComponentBinds::GetBoundingBoxExtents);
		PrimitiveComponent_.Method("FVector GetBoundsOrigin() const", &FAngelscriptUPrimitiveComponentBinds::GetBoundsOrigin);
		PrimitiveComponent_.Method("FVector GetBoundsExtent() const", &FAngelscriptUPrimitiveComponentBinds::GetBoundsExtent);
		PrimitiveComponent_.Method("float64 GetBoundsRadius() const", &FAngelscriptUPrimitiveComponentBinds::GetBoundsRadius);
		PrimitiveComponent_.Method("bool GetbSelectable() const", &FAngelscriptUPrimitiveComponentBinds::GetSelectable);
		PrimitiveComponent_.Method("void SetbSelectable(bool bSelectable)", &FAngelscriptUPrimitiveComponentBinds::SetSelectable);
		PrimitiveComponent_.Method("void SetLightmapType(ELightmapType Type)", &FAngelscriptUPrimitiveComponentBinds::SetLightmapType);
	}
}

AS_FORCE_LINK const FAngelscriptBind Bind_UPrimitiveComponent(
	TEXT("UPrimitiveComponent"),
	EAngelscriptBindPhase::ManualBindings,
	&BindUPrimitiveComponent);
