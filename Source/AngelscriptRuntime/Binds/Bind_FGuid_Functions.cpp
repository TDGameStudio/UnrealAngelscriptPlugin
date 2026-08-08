#include "Bind_FGuid.h"

void FAngelscriptFGuidBinds::ConstructParts(FGuid* Address, uint32 A, uint32 B, uint32 C, uint32 D)
{
	new (Address) FGuid(A, B, C, D);
}

void FAngelscriptFGuidBinds::ConstructString(FGuid* Address, const FString& GuidString)
{
	new (Address) FGuid(GuidString);
}

bool FAngelscriptFGuidBinds::Equals(const FGuid& Guid, const FGuid& Other)
{
	return Guid == Other;
}

int FAngelscriptFGuidBinds::Compare(const FGuid& Guid, const FGuid& Other)
{
	if (Guid < Other)
	{
		return -1;
	}
	if (Other < Guid)
	{
		return 1;
	}
	return 0;
}

FString FAngelscriptFGuidBinds::ToString(const FGuid& Guid)
{
	return Guid.ToString();
}

uint32 FAngelscriptFGuidBinds::Hash(const FGuid& Guid)
{
	return GetTypeHash(Guid);
}
