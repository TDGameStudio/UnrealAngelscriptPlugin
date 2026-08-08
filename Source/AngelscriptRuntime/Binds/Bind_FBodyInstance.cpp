#include "AngelscriptBinds.h"
#include "AngelscriptEngine.h"
#include "Engine/EngineTypes.h"

/**
 * FBodyInstance manual binding surface.
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | AngelScript usage signature                                                                | Purpose / parameter notes                                                                                            |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | UBodySetup FBodyInstance.GetBodySetup() const;                                             | Returns the body setup asset that defines collision geometry and physical defaults.                                  |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | bool FBodyInstance.Weld(FBodyInstance& TheirBody, const FTransform& TheirTM);              | Welds TheirBody into this physics body and reports success.                                                          |
 * |                                                                                            | @param TheirTM TheirBody transform in the welding frame used by the engine.                                          |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | void FBodyInstance.UnWeld(FBodyInstance& TheirBI);                                         | Separates a previously welded body from this physics body.                                                           |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | void FBodyInstance.SetUseCCD(bool bInUseCCD);                                              | Enables or disables continuous collision detection for this body.                                                    |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 */

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
