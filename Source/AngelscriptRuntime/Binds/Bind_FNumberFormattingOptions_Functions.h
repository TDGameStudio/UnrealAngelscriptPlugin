#pragma once

#include "Internationalization/Text.h"

struct FAngelscriptFNumberFormattingOptionsBinds
{
	static void Construct(FNumberFormattingOptions* Address);
	static uint32 Hash(const FNumberFormattingOptions& Options);
};
