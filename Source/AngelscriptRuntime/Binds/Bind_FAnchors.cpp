#include "AngelscriptBinds.h"

#include "Bind_FAnchors_Functions.h"

namespace
{
	void BindFAnchors(FAngelscriptBinds& Binds)
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
	}
}

AS_FORCE_LINK const FAngelscriptBind Bind_FAnchors(
	TEXT("FAnchors"),
	EAngelscriptBindPhase::ManualBindings,
	&BindFAnchors);
