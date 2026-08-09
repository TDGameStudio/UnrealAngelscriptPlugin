#include "Bind_Stats.h"

#include "AngelscriptBinds.h"

/**
 * Stats binding surface.
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | AngelScript usage signature                                                                | Purpose / parameter notes                                                                                            |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | struct FStatID;                                                                            | Declares the scoped-stat identifier value type.                                                                      |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | struct FScopeCycleCounter;                                                                 | Declares the scoped cycle-counter value type.                                                                        |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FStatID Stat(const FName& Name);                                                           | Constructs a stat identifier; its native lifetime is managed automatically.                                          |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FScopeCycleCounter Scope(const FStatID& Stat);                                             | Begins a scoped cycle counter for a stat identifier.                                                                 |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FScopeCycleCounter Scope(const UObject Object);                                            | Begins a scoped cycle counter attributed to an object.                                                               |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 */

AS_FORCE_LINK const FAngelscriptBind Bind_Stats_Types(
	TEXT("Stats.Types"),
	EAngelscriptBindPhase::TypeDeclarations,
	[](FAngelscriptBinds& Binds)
	{
		FBindFlags StatFlags;
		Binds.ValueClassForTarget<FScriptStatID>("FStatID", StatFlags);

		FBindFlags CounterFlags;
		Binds.ValueClassForTarget<FScriptScopeCycleCounter>("FScopeCycleCounter", CounterFlags);
	});

AS_FORCE_LINK const FAngelscriptBind Bind_Stats(
	TEXT("Stats.Functions"),
	EAngelscriptBindPhase::ExplicitBindings,
	[](FAngelscriptBinds& Binds)
	{
		auto FStatID_ = Binds.ExistingClassForTarget("FStatID");
		FStatID_.Constructor("void f(const FName& Name)", &FAngelscriptStatsBinds::ConstructStatID).NoDiscard();
		FStatID_.Destructor("void f()", &FAngelscriptStatsBinds::DestructStatID);

		auto FScopeCycleCounter_ = Binds.ExistingClassForTarget("FScopeCycleCounter");
		FScopeCycleCounter_.Constructor(
			"void f(const FStatID& Stat)",
			&FAngelscriptStatsBinds::ConstructScopeFromStat)
			.NoDiscard();
		FScopeCycleCounter_.Constructor(
			"void f(const UObject Object)",
			&FAngelscriptStatsBinds::ConstructScopeFromObject)
			.NoDiscard();
		FScopeCycleCounter_.Destructor("void f()", &FAngelscriptStatsBinds::DestructScope);
	});
