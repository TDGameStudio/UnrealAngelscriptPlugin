#include "Bind_FNumberFormattingOptions.h"

void FAngelscriptFNumberFormattingOptionsBinds::Construct(FNumberFormattingOptions* Address)
{
	new (Address) FNumberFormattingOptions();
}

uint32 FAngelscriptFNumberFormattingOptionsBinds::Hash(const FNumberFormattingOptions& Options)
{
	return GetTypeHash(Options);
}
