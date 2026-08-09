#include "Bind_UPrimitiveComponent.h"

#include "AngelscriptBinds.h"

/**
 * UPrimitiveComponent manual binding surface.
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | AngelScript usage signature                                                                | Purpose / parameter notes                                                                                            |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FVector UPrimitiveComponent.GetBoundingBoxExtents() const;                                 | Returns the full axis-aligned bounding-box size.                                                                     |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FVector UPrimitiveComponent.GetBoundsOrigin() const;                                       | Returns the world-space bounds origin.                                                                               |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FVector UPrimitiveComponent.GetBoundsExtent() const;                                       | Returns the world-space box extent.                                                                                  |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | float64 UPrimitiveComponent.GetBoundsRadius() const;                                       | Returns the world-space bounds sphere radius.                                                                        |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | bool UPrimitiveComponent.GetbSelectable() const;                                           | Returns whether the component is editor-selectable.                                                                  |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | void UPrimitiveComponent.SetbSelectable(bool bSelectable);                                 | Sets whether the component is editor-selectable.                                                                     |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | void UPrimitiveComponent.SetLightmapType(ELightmapType Type);                              | Sets the component lightmap interaction mode.                                                                        |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 */

AS_FORCE_LINK const FAngelscriptBind Bind_UPrimitiveComponent(
	TEXT("UPrimitiveComponent"),
	EAngelscriptBindPhase::ExplicitBindings,
	[](FAngelscriptBinds& Binds)
	{
		auto PrimitiveComponent_ = Binds.ExistingClassForTarget("UPrimitiveComponent");
		PrimitiveComponent_.Method("FVector GetBoundingBoxExtents() const", &FAngelscriptUPrimitiveComponentBinds::GetBoundingBoxExtents);
		PrimitiveComponent_.Method("FVector GetBoundsOrigin() const", &FAngelscriptUPrimitiveComponentBinds::GetBoundsOrigin);
		PrimitiveComponent_.Method("FVector GetBoundsExtent() const", &FAngelscriptUPrimitiveComponentBinds::GetBoundsExtent);
		PrimitiveComponent_.Method("float64 GetBoundsRadius() const", &FAngelscriptUPrimitiveComponentBinds::GetBoundsRadius);
		PrimitiveComponent_.Method("bool GetbSelectable() const", &FAngelscriptUPrimitiveComponentBinds::GetSelectable);
		PrimitiveComponent_.Method("void SetbSelectable(bool bSelectable)", &FAngelscriptUPrimitiveComponentBinds::SetSelectable);
		PrimitiveComponent_.Method("void SetLightmapType(ELightmapType Type)", &FAngelscriptUPrimitiveComponentBinds::SetLightmapType);
	});
