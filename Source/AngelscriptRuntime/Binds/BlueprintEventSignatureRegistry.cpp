// Copyright Hazelight + AngelscriptProject contributors. Apache-2.0.

#include "BlueprintEventSignatureRegistry.h"


FBlueprintEventSignatureRegistry::~FBlueprintEventSignatureRegistry()
{
	Reset();
}

void FBlueprintEventSignatureRegistry::AddOwnership(void* Signature)
{
	if (Signature == nullptr)
	{
		return;
	}

	FScopeLock Lock(&CriticalSection);
	Owned.Add(Signature);
}

void FBlueprintEventSignatureRegistry::Reset()
{
	TArray<void*> Pending;
	{
		FScopeLock Lock(&CriticalSection);
		Pending = MoveTemp(Owned);
		Owned.Reset();
	}

	for (void* Ptr : Pending)
	{
		BlueprintEventSignatureRegistryInternal::DropOwnedSignature(Ptr);
	}
}

int32 FBlueprintEventSignatureRegistry::Num() const
{
	FScopeLock Lock(&CriticalSection);
	return Owned.Num();
}
