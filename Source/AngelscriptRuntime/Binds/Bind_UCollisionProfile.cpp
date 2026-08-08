#include "AngelscriptBinds.h"

#include "Bind_UCollisionProfile_Functions.h"

namespace
{
	void BindUCollisionProfile(FAngelscriptBinds& Binds)
	{
		FAngelscriptBinds::FNamespace Namespace(Binds.GetTargetEngine(), "UCollisionProfile");
		Binds.BindGlobalFunctionForTarget(
			"ECollisionChannel ConvertToCollisionChannel(bool TraceType, int32 Index)",
			&FAngelscriptUCollisionProfileBinds::ConvertToCollisionChannel);
		Binds.BindGlobalFunctionForTarget(
			"EObjectTypeQuery ConvertToObjectType(ECollisionChannel CollisionChannel)",
			&FAngelscriptUCollisionProfileBinds::ConvertToObjectType);
		Binds.BindGlobalFunctionForTarget(
			"ETraceTypeQuery ConvertToTraceType(ECollisionChannel CollisionChannel)",
			&FAngelscriptUCollisionProfileBinds::ConvertToTraceType);
	}
}

AS_FORCE_LINK const FAngelscriptBind Bind_UCollisionProfile(
	TEXT("UCollisionProfile"),
	EAngelscriptBindPhase::ManualBindings,
	&BindUCollisionProfile);
