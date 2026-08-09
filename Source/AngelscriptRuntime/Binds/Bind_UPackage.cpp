#include "AngelscriptBinds.h"
#include "UObject/Package.h"

/**
 * Unreal package state.
 * +------------------------------------------------------------------------------------------+--------------------------------------------------------------------------------------------------------------------+
 * | AngelScript usage signature                                                              | Purpose / parameter notes                                                                                          |
 * +------------------------------------------------------------------------------------------+--------------------------------------------------------------------------------------------------------------------+
 * | bool Package.IsDirty() const;                                                            | Reports whether the package has unsaved changes.                                                                   |
 * +------------------------------------------------------------------------------------------+--------------------------------------------------------------------------------------------------------------------+
 */

AS_FORCE_LINK const FAngelscriptBind Bind_UPackage(
	TEXT("UPackage"),
	EAngelscriptBindPhase::ManualBindings,
	[](FAngelscriptBinds& Binds)
	{
		auto UPackage_ = Binds.ExistingClassForTarget("UPackage");

		UPackage_.Method("bool IsDirty() const", METHOD_TRIVIAL(UPackage, IsDirty));
	});
