#include "AngelscriptBinds.h"

#include "Bind_Stats_Functions.h"

namespace
{
	void BindStatsTypes(FAngelscriptBinds& Binds)
	{
		FBindFlags StatFlags;
		Binds.ValueClassForTarget<FScriptStatID>("FStatID", StatFlags);

		FBindFlags CounterFlags;
		Binds.ValueClassForTarget<FScriptScopeCycleCounter>("FScopeCycleCounter", CounterFlags);
	}

	void BindStatsFunctions(FAngelscriptBinds& Binds)
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
	}
}

AS_FORCE_LINK const FAngelscriptBind Bind_Stats_Types(
	TEXT("Stats.Types"),
	EAngelscriptBindPhase::TypeDeclarations,
	&BindStatsTypes);

AS_FORCE_LINK const FAngelscriptBind Bind_Stats(
	TEXT("Stats.Functions"),
	EAngelscriptBindPhase::ManualBindings,
	&BindStatsFunctions);
