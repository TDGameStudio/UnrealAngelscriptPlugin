#include "Bind_FRandomStream.h"

void FAngelscriptFRandomStreamBinds::ConstructDefault(FRandomStream* Address)
{
	new (Address) FRandomStream();
}

void FAngelscriptFRandomStreamBinds::ConstructIntSeed(FRandomStream* Address, int32 Seed)
{
	new (Address) FRandomStream(Seed);
}

void FAngelscriptFRandomStreamBinds::ConstructUIntSeed(FRandomStream* Address, uint32 Seed)
{
	new (Address) FRandomStream(static_cast<int32>(Seed));
}

void FAngelscriptFRandomStreamBinds::AppendToString(void* Address, FString& String)
{
	String += static_cast<FRandomStream*>(Address)->ToString();
}
