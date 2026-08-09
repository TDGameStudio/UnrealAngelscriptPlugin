#include "Bind_FMargin.h"

#include "AngelscriptBinds.h"

/**
 * FMargin manual binding surface.
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | AngelScript usage signature                                                                | Purpose / parameter notes                                                                                            |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FMargin Margin(float32 UniformMargin);                                                     | Constructs a margin with the same value on every side.                                                               |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FMargin Margin(float32 Horizontal, float32 Vertical);                                      | Constructs a margin from horizontal and vertical values.                                                             |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FMargin Margin(const FVector2D& InVector);                                                 | Constructs a margin from horizontal and vertical vector components.                                                  |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FMargin Margin(float32 InLeft, float32 InTop, float32 InRight, float32 InBottom);          | Constructs a margin from individual side values.                                                                     |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FMargin Margin(const FVector4& InVector);                                                  | Constructs a margin from left, top, right, and bottom vector components.                                             |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | Margin * Scale;                                                                            | Returns the margin scaled uniformly.                                                                                 |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | Margin * InScale;                                                                          | Returns the margin scaled component-wise.                                                                            |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | Margin + Other;                                                                            | Returns the component-wise sum of two margins.                                                                       |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | Margin - Other;                                                                            | Returns the component-wise difference of two margins.                                                                |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | Margin == Other;                                                                           | Compares two margins for exact equality.                                                                             |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FVector2D FMargin.GetTopLeft() const;                                                      | Returns the left and top components.                                                                                 |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FVector2D FMargin.GetDesiredSize() const;                                                  | Returns the total horizontal and vertical space.                                                                     |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | float32 FMargin.GetTotalSpaceAlongHorizontal() const;                                      | Returns the combined left and right space.                                                                           |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | float32 FMargin.GetTotalSpaceAlongVertical() const;                                        | Returns the combined top and bottom space.                                                                           |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 */

AS_FORCE_LINK const FAngelscriptBind Bind_FMargin(
	TEXT("FMargin"),
	EAngelscriptBindPhase::ExplicitBindings,
	[](FAngelscriptBinds& Binds)
	{
		auto FMargin_ = Binds.ExistingClassForTarget("FMargin");

		FMargin_.Constructor(
			"void f(float32 UniformMargin)",
			&FAngelscriptFMarginBinds::ConstructUniform,
			"FMargin",
			true)
			.NoDiscard();
		FMargin_.Constructor(
			"void f(float32 Horizontal, float32 Vertical)",
			&FAngelscriptFMarginBinds::ConstructHorizontalVertical,
			"FMargin",
			true)
			.NoDiscard();
		FMargin_.Constructor(
			"void f(const FVector2D& InVector)",
			&FAngelscriptFMarginBinds::ConstructFromVector2D,
			"FMargin",
			true)
			.NoDiscard();
		FMargin_.Constructor(
			"void f(float32 InLeft, float32 InTop, float32 InRight, float32 InBottom)",
			&FAngelscriptFMarginBinds::ConstructLTRB,
			"FMargin",
			true)
			.NoDiscard();
		FMargin_.Constructor(
			"void f(const FVector4& InVector)",
			&FAngelscriptFMarginBinds::ConstructFromVector4,
			"FMargin",
			true)
			.NoDiscard();

		FMargin_.Method("FMargin opMul(float32 Scale) const", METHODPR_TRIVIAL(FMargin, FMargin, operator*, (float) const));
		FMargin_.Method("FMargin opMul(const FMargin& InScale) const", METHODPR_TRIVIAL(FMargin, FMargin, operator*, (const FMargin&) const));
		FMargin_.Method("FMargin opAdd(const FMargin& Other) const", METHODPR_TRIVIAL(FMargin, FMargin, operator+, (const FMargin&) const));
		FMargin_.Method("FMargin opSub(const FMargin& Other) const", METHODPR_TRIVIAL(FMargin, FMargin, operator-, (const FMargin&) const));
		FMargin_.Method("bool opEquals(const FMargin& Other) const", METHODPR_TRIVIAL(bool, FMargin, operator==, (const FMargin&) const));
		FMargin_.Method("FVector2D GetTopLeft() const", METHOD_TRIVIAL(FMargin, GetTopLeft));
		FMargin_.Method("FVector2D GetDesiredSize() const", METHOD_TRIVIAL(FMargin, GetDesiredSize));
		FMargin_.Method("float32 GetTotalSpaceAlongHorizontal() const", &FAngelscriptFMarginBinds::GetTotalSpaceAlongHorizontal);
		FMargin_.Method("float32 GetTotalSpaceAlongVertical() const", &FAngelscriptFMarginBinds::GetTotalSpaceAlongVertical);
	});
