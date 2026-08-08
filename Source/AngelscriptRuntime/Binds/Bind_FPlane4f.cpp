#include "AngelscriptBinds.h"

#include "Bind_FPlane4f_Functions.h"

namespace
{
	void BindFPlane4f(FAngelscriptBinds& Binds)
	{
		auto FPlane4f_ = Binds.ExistingClassForTarget("FPlane4f");
		FPlane4f_.Constructor(
			"void f(const FVector3f& InLocation, const FVector3f& InNormal)",
			&FAngelscriptFPlane4fBinds::ConstructFromLocationAndNormal)
			.NoDiscard();
		FPlane4f_.Constructor(
			"void f(const FVector3f& PointA, const FVector3f& PointB, const FVector3f& PointC)",
			&FAngelscriptFPlane4fBinds::ConstructFromPoints)
			.NoDiscard();
		FPlane4f_.Constructor(
			"void f(const FPlane& Plane)",
			&FAngelscriptFPlane4fBinds::ConstructFromPlane)
			.NoDiscard();
		FPlane4f_.Method("float32 PlaneDot(const FVector3f& Location) const", METHODPR_TRIVIAL(float, FPlane4f, PlaneDot, (const FVector3f&)));
		FPlane4f_.Method("FVector3f GetOrigin() const", METHOD_TRIVIAL(FPlane4f, GetOrigin));
		FPlane4f_.Method("const FVector3f& GetNormal() const", METHOD_TRIVIAL(FPlane4f, GetNormal));
	}
}

AS_FORCE_LINK const FAngelscriptBind Bind_FPlane4f(
	TEXT("FPlane4f"),
	EAngelscriptBindPhase::ManualBindings,
	&BindFPlane4f);
