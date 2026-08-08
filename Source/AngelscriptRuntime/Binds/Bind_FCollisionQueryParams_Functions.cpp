#include "Bind_FCollisionQueryParams_Functions.h"

void FAngelscriptFCollisionQueryParamsBinds::ConstructCollisionQueryParams(FCollisionQueryParams* Address)
{
	new (Address) FCollisionQueryParams();
}

void FAngelscriptFCollisionQueryParamsBinds::ConstructCollisionQueryParamsCopy(FCollisionQueryParams* Address, const FCollisionQueryParams& Other)
{
	new (Address) FCollisionQueryParams(Other);
}

void FAngelscriptFCollisionQueryParamsBinds::ConstructCollisionQueryParamsFromTraceTag(FCollisionQueryParams* Address, FName InTraceTag, bool bInTraceComplex, const AActor* InIgnoreActor)
{
	new (Address) FCollisionQueryParams(InTraceTag, bInTraceComplex, InIgnoreActor);
}

TArray<uint32> FAngelscriptFCollisionQueryParamsBinds::GetCollisionQueryParamsIgnoredComponents(const FCollisionQueryParams* Address)
{
	const FCollisionQueryParams::IgnoreComponentsArrayType& IgnoredComponents = Address->GetIgnoredComponents();
	TArray<uint32> OutArray;
	OutArray.SetNumUninitialized(IgnoredComponents.Num());
	FMemory::Memcpy(OutArray.GetData(), IgnoredComponents.GetData(), IgnoredComponents.Num() * sizeof(IgnoredComponents[0]));
	return OutArray;
}

TArray<uint32> FAngelscriptFCollisionQueryParamsBinds::GetCollisionQueryParamsIgnoredActors(const FCollisionQueryParams* Address)
{
	const FCollisionQueryParams::IgnoreActorsArrayType& IgnoredActors = Address->GetIgnoredSourceObjects();
	TArray<uint32> OutArray;
	OutArray.SetNumUninitialized(IgnoredActors.Num());
	FMemory::Memcpy(OutArray.GetData(), IgnoredActors.GetData(), IgnoredActors.Num() * sizeof(IgnoredActors[0]));
	return OutArray;
}

void FAngelscriptFCollisionQueryParamsBinds::ConstructCollisionEnabledMask(FCollisionEnabledMask* Address)
{
	new (Address) FCollisionEnabledMask();
}

void FAngelscriptFCollisionQueryParamsBinds::ConstructCollisionEnabledMaskFromCollisionEnabled(FCollisionEnabledMask* Address, ECollisionEnabled::Type CollisionEnabled)
{
	new (Address) FCollisionEnabledMask(CollisionEnabled);
}

void FAngelscriptFCollisionQueryParamsBinds::ConstructComponentQueryParams(FComponentQueryParams* Address)
{
	new (Address) FComponentQueryParams();
}

void FAngelscriptFCollisionQueryParamsBinds::ConstructComponentQueryParamsCopy(FComponentQueryParams* Address, const FComponentQueryParams& Other)
{
	new (Address) FComponentQueryParams(Other);
}

void FAngelscriptFCollisionQueryParamsBinds::ConstructComponentQueryParamsFromTraceTag(FComponentQueryParams* Address, FName InTraceTag, const AActor* InIgnoreActor, FCollisionEnabledMask CollisionEnabledMask)
{
	new (Address) FComponentQueryParams(InTraceTag, FComponentQueryParams::GetUnknownStatId(), InIgnoreActor, CollisionEnabledMask);
}

TArray<uint32> FAngelscriptFCollisionQueryParamsBinds::GetComponentQueryParamsIgnoredComponents(const FComponentQueryParams* Address)
{
	const FComponentQueryParams::IgnoreComponentsArrayType& IgnoredComponents = Address->GetIgnoredComponents();
	TArray<uint32> OutArray;
	OutArray.SetNumUninitialized(IgnoredComponents.Num());
	FMemory::Memcpy(OutArray.GetData(), IgnoredComponents.GetData(), IgnoredComponents.Num() * sizeof(IgnoredComponents[0]));
	return OutArray;
}

TArray<uint32> FAngelscriptFCollisionQueryParamsBinds::GetComponentQueryParamsIgnoredActors(const FComponentQueryParams* Address)
{
	const FComponentQueryParams::IgnoreActorsArrayType& IgnoredActors = Address->GetIgnoredSourceObjects();
	TArray<uint32> OutArray;
	OutArray.SetNumUninitialized(IgnoredActors.Num());
	FMemory::Memcpy(OutArray.GetData(), IgnoredActors.GetData(), IgnoredActors.Num() * sizeof(IgnoredActors[0]));
	return OutArray;
}

void FAngelscriptFCollisionQueryParamsBinds::ConstructCollisionResponseParams(FCollisionResponseParams* Address)
{
	new (Address) FCollisionResponseParams();
}

void FAngelscriptFCollisionQueryParamsBinds::ConstructCollisionResponseParamsFromDefaultResponse(FCollisionResponseParams* Address, ECollisionResponse DefaultResponse)
{
	new (Address) FCollisionResponseParams(DefaultResponse);
}

void FAngelscriptFCollisionQueryParamsBinds::ConstructCollisionResponseParamsFromContainer(FCollisionResponseParams* Address, const FCollisionResponseContainer& ResponseContainer)
{
	new (Address) FCollisionResponseParams(ResponseContainer);
}

void FAngelscriptFCollisionQueryParamsBinds::ConstructCollisionObjectQueryParams(FCollisionObjectQueryParams* Address)
{
	new (Address) FCollisionObjectQueryParams();
}

void FAngelscriptFCollisionQueryParamsBinds::ConstructCollisionObjectQueryParamsFromChannel(FCollisionQueryParams* Address, ECollisionChannel QueryChannel)
{
	new (Address) FCollisionObjectQueryParams(QueryChannel);
}

void FAngelscriptFCollisionQueryParamsBinds::ConstructCollisionObjectQueryParamsFromInitType(FCollisionObjectQueryParams* Address, FCollisionObjectQueryParams::InitType QueryType)
{
	new (Address) FCollisionObjectQueryParams(QueryType);
}

void FAngelscriptFCollisionQueryParamsBinds::ConstructCollisionObjectQueryParamsFromObjectTypes(FCollisionObjectQueryParams* Address, int32 InObjectTypesToQuery)
{
	new (Address) FCollisionObjectQueryParams(InObjectTypesToQuery);
}

void FAngelscriptFCollisionQueryParamsBinds::ConstructCollisionResponseContainer(FCollisionResponseContainer* Address, ECollisionResponse DefaultResponse)
{
	new (Address) FCollisionResponseContainer(DefaultResponse);
}
