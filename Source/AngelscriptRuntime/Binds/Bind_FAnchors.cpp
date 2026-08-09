#include "Bind_FAnchors.h"

#include "AngelscriptBinds.h"

/**
 * FAnchors construction, comparison, and stretch queries.
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | AngelScript usage signature                                                                          | Purpose / parameter notes                                                                                        |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FAnchors Anchors(float32 UnifromAnchors);                                                            | Constructs uniform minimum and maximum anchors.                                                                  |
 * |                                                                                                      | @param UnifromAnchors Shared normalized anchor coordinate on both axes.                                          |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FAnchors Anchors(float32 Horizontal, float32 Vertical);                                              | Constructs point anchors from horizontal and vertical coordinates.                                               |
 * |                                                                                                      | @param Horizontal Normalized X anchor coordinate.                                                                |
 * |                                                                                                      | @param Vertical Normalized Y anchor coordinate.                                                                  |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FAnchors Anchors(float32 MinX, float32 MinY, float32 MaxX, float32 MaxY);                            | Constructs ranged anchors from minimum and maximum coordinates.                                                  |
 * |                                                                                                      | @param MinX Minimum normalized X coordinate.                                                                     |
 * |                                                                                                      | @param MinY Minimum normalized Y coordinate.                                                                     |
 * |                                                                                                      | @param MaxX Maximum normalized X coordinate.                                                                     |
 * |                                                                                                      | @param MaxY Maximum normalized Y coordinate.                                                                     |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | bool bEqual = Left == Right;                                                                         | Compares every anchor coordinate.                                                                                |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | bool Anchors.IsStretchedVertical() const;                                                            | Reports whether the vertical anchors span a range.                                                               |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | bool Anchors.IsStretchedHorizontal() const;                                                          | Reports whether the horizontal anchors span a range.                                                             |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 */

AS_FORCE_LINK const FAngelscriptBind Bind_FAnchors(
	TEXT("FAnchors"),
	EAngelscriptBindPhase::ExplicitBindings,
	[](FAngelscriptBinds& Binds)
	{
		auto FAnchors_ = Binds.ExistingClassForTarget("FAnchors");
		FAnchors_.Constructor(
			"void f(float32 UnifromAnchors)",
			&FAngelscriptFAnchorsBinds::ConstructUniform,
			"FAnchors",
			true)
			.NoDiscard();
		FAnchors_.Constructor(
			"void f(float32 Horizontal, float32 Vertical)",
			&FAngelscriptFAnchorsBinds::ConstructPoint,
			"FAnchors",
			true)
			.NoDiscard();
		FAnchors_.Constructor(
			"void f(float32 MinX, float32 MinY, float32 MaxX, float32 MaxY)",
			&FAngelscriptFAnchorsBinds::ConstructRange,
			"FAnchors",
			true)
			.NoDiscard();
		FAnchors_.Method("bool opEquals(const FAnchors& Other) const", METHODPR_TRIVIAL(bool, FAnchors, operator==, (const FAnchors&) const));
		FAnchors_.Method("bool IsStretchedVertical() const", METHOD_TRIVIAL(FAnchors, IsStretchedVertical));
		FAnchors_.Method("bool IsStretchedHorizontal() const", METHOD_TRIVIAL(FAnchors, IsStretchedHorizontal));
	});
