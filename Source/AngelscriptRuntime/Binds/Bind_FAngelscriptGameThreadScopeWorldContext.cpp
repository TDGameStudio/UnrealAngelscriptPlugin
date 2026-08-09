class UObject;
struct FAngelscriptGameThreadScopeWorldContext;

struct FAngelscriptFAngelscriptGameThreadScopeWorldContextBinds
{
	static void Construct(FAngelscriptGameThreadScopeWorldContext* Address, UObject* WorldContext);
	static void Destruct(FAngelscriptGameThreadScopeWorldContext& Scope);
};

#include "AngelscriptBinds.h"

#include "AngelscriptEngine.h"
/**
 * Scoped game-thread world context.
 * +------------------------------------------------------------------------------------------+--------------------------------------------------------------------------------------------------------------------+
 * | AngelScript usage signature                                                              | Purpose / parameter notes                                                                                          |
 * +------------------------------------------------------------------------------------------+--------------------------------------------------------------------------------------------------------------------+
 * | FAngelscriptGameThreadScopeWorldContext Scope(UObject WorldContext);                     | Pushes WorldContext for the scope lifetime and restores the preceding context on destruction.                      |
 * |                                                                                          | @param WorldContext Object used to resolve the active world.                                                       |
 * +------------------------------------------------------------------------------------------+--------------------------------------------------------------------------------------------------------------------+
 */

AS_FORCE_LINK const FAngelscriptBind Bind_FAngelscriptGameThreadScopeWorldContext(
	TEXT("FAngelscriptGameThreadScopeWorldContext"),
	EAngelscriptBindPhase::ExplicitBindings,
	[](FAngelscriptBinds& Binds)
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
	});

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
