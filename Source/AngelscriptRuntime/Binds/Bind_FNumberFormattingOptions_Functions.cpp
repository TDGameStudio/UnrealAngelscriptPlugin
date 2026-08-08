#include "Bind_FNumberFormattingOptions_Functions.h"

void FAngelscriptFNumberFormattingOptionsBinds::Construct(FNumberFormattingOptions* Address)
{
	new (Address) FNumberFormattingOptions();
}

uint32 FAngelscriptFNumberFormattingOptionsBinds::Hash(const FNumberFormattingOptions& Options)
{
	return GetTypeHash(Options);
}
