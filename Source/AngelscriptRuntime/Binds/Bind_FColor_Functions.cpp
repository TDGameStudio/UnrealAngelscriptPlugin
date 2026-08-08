#include "Bind_FColor.h"

void FAngelscriptFColorBinds::ConstructRGBA(FColor* Address, uint8 R, uint8 G, uint8 B, uint8 A)
{
	new (Address) FColor(R, G, B, A);
}

void FAngelscriptFColorBinds::ConstructPacked(FColor* Address, uint32 PackedColor)
{
	new (Address) FColor(PackedColor);
}

void FAngelscriptFColorBinds::AppendToString(void* Address, FString& OutString)
{
	OutString += static_cast<FColor*>(Address)->ToString();
}
