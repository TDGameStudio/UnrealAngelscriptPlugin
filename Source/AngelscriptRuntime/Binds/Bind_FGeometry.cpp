#include "Bind_FGeometry.h"

#include "AngelscriptBinds.h"

/**
 * FGeometry manual binding surface.
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | AngelScript usage signature                                                                | Purpose / parameter notes                                                                                            |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FVector2D FGeometry.GetLocalSize() const;                                                  | Returns the geometry size in local Slate units.                                                                      |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FVector2D FGeometry.GetAbsoluteSize() const;                                               | Returns the geometry size after accumulated layout transforms.                                                       |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FVector2D FGeometry.AbsoluteToLocal(const FVector2D& Position) const;                      | Transforms an absolute Slate position into local geometry coordinates.                                               |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FVector2D FGeometry.LocalToAbsolute(const FVector2D& Position) const;                      | Transforms a local geometry position into absolute Slate coordinates.                                                |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FGeometry FGeometry.MakeChild(const FVector2D& Position,                                   | Creates child geometry at local Position with Size in Slate units.                                                   |
 * |     const FVector2D& Size) const;                                                          |                                                                                                                      |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 */

AS_FORCE_LINK const FAngelscriptBind Bind_FGeometry(
	TEXT("FGeometry"),
	EAngelscriptBindPhase::ManualBindings,
	[](FAngelscriptBinds& Binds)
	{
		auto FGeometry_ = Binds.ExistingClassForTarget("FGeometry");
		FGeometry_.Method("FVector2D GetLocalSize() const", &FAngelscriptFGeometryBinds::GetLocalSize);
		FGeometry_.Method("FVector2D GetAbsoluteSize() const", &FAngelscriptFGeometryBinds::GetAbsoluteSize);
		FGeometry_.Method("FVector2D AbsoluteToLocal(const FVector2D& Position) const", &FAngelscriptFGeometryBinds::AbsoluteToLocal);
		FGeometry_.Method("FVector2D LocalToAbsolute(const FVector2D& Position) const", &FAngelscriptFGeometryBinds::LocalToAbsolute);
		FGeometry_.Method("FGeometry MakeChild(const FVector2D& Position, const FVector2D& Size) const", &FAngelscriptFGeometryBinds::MakeChild);
	});
