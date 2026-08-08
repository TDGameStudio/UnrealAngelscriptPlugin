#include "AngelscriptBinds.h"

#include "AngelscriptEngine.h"
#include "Bind_FAngelscriptGameThreadScopeWorldContext_Functions.h"

namespace
{
	void BindFAngelscriptGameThreadScopeWorldContext(FAngelscriptBinds& Binds)
	{
		auto Scope_ = Binds.ValueClassForTarget<FAngelscriptGameThreadScopeWorldContext>(
			"FAngelscriptGameThreadScopeWorldContext",
			FBindFlags());

		Scope_.Constructor(
			"void f(const UObject WorldContext)",
			&FAngelscriptFAngelscriptGameThreadScopeWorldContextBinds::Construct)
			.NoDiscard();
		Scope_.Destructor(
			"void f()",
			&FAngelscriptFAngelscriptGameThreadScopeWorldContextBinds::Destruct);
	}
}

AS_FORCE_LINK const FAngelscriptBind Bind_FAngelscriptGameThreadScopeWorldContext(
	TEXT("FAngelscriptGameThreadScopeWorldContext"),
	EAngelscriptBindPhase::ManualBindings,
	&BindFAngelscriptGameThreadScopeWorldContext);
