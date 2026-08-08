#include "AngelscriptBinds.h"
#include "UObject/Package.h"

namespace
{
	void BindUPackage(FAngelscriptBinds& Binds)
	{
		auto UPackage_ = Binds.ExistingClassForTarget("UPackage");

		UPackage_.Method("bool IsDirty() const", METHOD_TRIVIAL(UPackage, IsDirty));
	}
}

AS_FORCE_LINK const FAngelscriptBind Bind_UPackage(
	TEXT("UPackage"),
	EAngelscriptBindPhase::ManualBindings,
	&BindUPackage);
