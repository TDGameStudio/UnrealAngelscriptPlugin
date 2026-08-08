#include "AngelscriptBinds.h"

#include "Bind_LandscapeProxy_Functions.h"

namespace
{
	void BindALandscapeProxy(FAngelscriptBinds& Binds)
	{
		auto ALandscapeProxy_ = Binds.ExistingClassForTarget("ALandscapeProxy");
		ALandscapeProxy_.Method(
			"bool GetHeightAtLocation(FVector Location, float32& OutHeight) const",
			&FAngelscriptLandscapeProxyBinds::GetHeightAtLocation);
	}
}

AS_FORCE_LINK const FAngelscriptBind Bind_ALandscapeProxy(
	TEXT("ALandscapeProxy.GetHeightAtLocation"),
	EAngelscriptBindPhase::ManualBindings,
	&BindALandscapeProxy);
