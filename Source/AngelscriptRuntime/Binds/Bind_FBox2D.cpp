#include "AngelscriptBinds.h"

#include "Math/Box2D.h"

struct FAngelscriptFBox2DBinds
{
	static void Construct(FBox2D* Address, const FVector2D& Min, const FVector2D& Max)
	{
		new (Address) FBox2D(Min, Max);
	}
};

/**
 * FBox2D binding surface.
 * +---------------------------------------------------------------------------------------------+------------------------------------------------------------------+
 * | AngelScript usage signature                                                                 | Purpose / parameter notes                                        |
 * +---------------------------------------------------------------------------------------------+------------------------------------------------------------------+
 * | FBox2D Box(const FVector2D& InMin, const FVector2D& InMax);                                 | Constructs a valid 2D box from minimum and maximum corners.     |
 * +---------------------------------------------------------------------------------------------+------------------------------------------------------------------+
 * | FBox2D FBox2D.ExpandBy(float64 Amount) const;                                               | Expands all sides by Amount.                                     |
 * +---------------------------------------------------------------------------------------------+------------------------------------------------------------------+
 * | bool FBox2D.IsInside(const FVector2D& Point) const;                                         | Tests whether Point is inside the box.                           |
 * +---------------------------------------------------------------------------------------------+------------------------------------------------------------------+
 */
AS_FORCE_LINK const FAngelscriptBind Bind_FBox2D(
	TEXT("FBox2D"),
	EAngelscriptBindPhase::ExplicitBindings,
	[](FAngelscriptBinds& Binds)
	{
		auto Box = Binds.ExistingClassForTarget("FBox2D");
		Box.Constructor("void f(const FVector2D& InMin, const FVector2D& InMax)", &FAngelscriptFBox2DBinds::Construct, "FBox2D", true);
		Box.Method("bool opEquals(const FBox2D& Other) const", METHODPR_TRIVIAL(bool, FBox2D, operator==, (const FBox2D&) const));
		Box.Method("float64 GetArea() const", METHOD_TRIVIAL(FBox2D, GetArea));
		Box.Method("FVector2D GetCenter() const", METHOD_TRIVIAL(FBox2D, GetCenter));
		Box.Method("FVector2D GetExtent() const", METHOD_TRIVIAL(FBox2D, GetExtent));
		Box.Method("FBox2D ExpandBy(float64 Amount) const", METHODPR_TRIVIAL(FBox2D, FBox2D, ExpandBy, (double) const));
		Box.Method("bool Intersect(const FBox2D& Other) const", METHOD_TRIVIAL(FBox2D, Intersect));
		Box.Method("bool IsInside(const FVector2D& Point) const", METHODPR_TRIVIAL(bool, FBox2D, IsInside, (const FVector2D&) const));
	});
