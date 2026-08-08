#include "Bind_FAngelscriptGameThreadScopeWorldContext_Functions.h"

#include "AngelscriptEngine.h"

void FAngelscriptFAngelscriptGameThreadScopeWorldContextBinds::Construct(
	FAngelscriptGameThreadScopeWorldContext* Address,
	UObject* WorldContext)
{
	new (Address) FAngelscriptGameThreadScopeWorldContext(WorldContext);
}

void FAngelscriptFAngelscriptGameThreadScopeWorldContextBinds::Destruct(
	FAngelscriptGameThreadScopeWorldContext& Scope)
{
	Scope.~FAngelscriptGameThreadScopeWorldContext();
}
