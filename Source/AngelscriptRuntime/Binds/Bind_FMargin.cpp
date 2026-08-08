#include "AngelscriptBinds.h"

#include "Bind_FMargin_Functions.h"

namespace
{
	void BindFMargin(FAngelscriptBinds& Binds)
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
	}
}

AS_FORCE_LINK const FAngelscriptBind Bind_FMargin(
	TEXT("FMargin"),
	EAngelscriptBindPhase::ManualBindings,
	&BindFMargin);
