#include "AngelscriptBinds.h"
#include "AngelscriptEngine.h"
#include "Engine/EngineTypes.h"

namespace
{
	void BindFBodyInstance(FAngelscriptBinds& Binds)
	{
		auto FBodyInstance_ = Binds.ExistingClassForTarget("FBodyInstance");

		FBodyInstance_.Method("UBodySetup GetBodySetup() const", METHOD_TRIVIAL(FBodyInstance, GetBodySetup));
		FBodyInstance_.Method("bool Weld(FBodyInstance& TheirBody, const FTransform& TheirTM)", METHOD_TRIVIAL(FBodyInstance, Weld));
		FBodyInstance_.Method("void UnWeld(FBodyInstance& TheirBI)", METHOD_TRIVIAL(FBodyInstance, UnWeld));
		FBodyInstance_.Method("void SetUseCCD(bool bInUseCCD)", METHOD_TRIVIAL(FBodyInstance, SetUseCCD));
	}
}

AS_FORCE_LINK const FAngelscriptBind Bind_FBodyInstance(
	TEXT("FBodyInstance"),
	EAngelscriptBindPhase::ManualBindings,
	&BindFBodyInstance);
