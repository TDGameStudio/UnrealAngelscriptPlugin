#include "Bind_FLatentActionInfo_Functions.h"

void FAngelscriptFLatentActionInfoBinds::Construct(
	FLatentActionInfo* Address,
	int32 Linkage,
	int32 Uuid,
	FName FunctionName,
	UObject* CallbackTarget)
{
	new (Address) FLatentActionInfo();
	Address->Linkage = Linkage;
	Address->UUID = Uuid;
	Address->ExecutionFunction = FunctionName;
	Address->CallbackTarget = CallbackTarget;
}
