#include "FunctionLibraries/AngelscriptActorLibrary.h"

void UAngelscriptActorLibrary::SetActorRelativeRotationQuat(AActor* Actor, const FQuat& NewRelativeRotation)
{
	Actor->SetActorRelativeRotation(NewRelativeRotation);
}

void UAngelscriptActorLibrary::SetActorRotationQuat(AActor* Actor, const FQuat& NewRotation)
{
	Actor->SetActorRotation(NewRotation);
}

void UAngelscriptActorLibrary::SetActorLocationAndRotationQuat(
	AActor* Actor,
	const FVector& NewLocation,
	const FQuat& NewRotation,
	bool bTeleport)
{
	Actor->SetActorLocationAndRotation(NewLocation, NewRotation, false, nullptr, TeleportFlagToEnum(bTeleport));
}

void UAngelscriptActorLibrary::SetActorQuat(AActor* Actor, const FQuat& NewRotation)
{
	Actor->SetActorRotation(NewRotation);
}

void UAngelscriptActorLibrary::AddActorLocalRotationQuat(AActor* Actor, const FQuat& DeltaRotation)
{
	Actor->AddActorLocalRotation(DeltaRotation);
}

void UAngelscriptActorLibrary::AddActorWorldRotationQuat(AActor* Actor, const FQuat& DeltaRotation)
{
	Actor->AddActorWorldRotation(DeltaRotation);
}

bool UAngelscriptActorLibrary::SetActorLocationAdvanced(
	AActor* Actor,
	const FVector& NewLocation,
	bool bSweep,
	FHitResult& SweepHitResult,
	bool bTeleport)
{
	return Actor->K2_SetActorLocation(NewLocation, bSweep, SweepHitResult, bTeleport);
}

void UAngelscriptActorLibrary::SetbRunConstructionScriptOnDrag(AActor* Actor, bool Value)
{
#if WITH_EDITOR
	Actor->bRunConstructionScriptOnDrag = Value;
#endif
}

#if WITH_EDITOR
void UAngelscriptActorLibrary::RerunConstructionScripts(AActor* Actor)
{
	Actor->RerunConstructionScripts();
}
#endif

TArray<AActor*> UAngelscriptActorLibrary::GetAttachedActors(
	const AActor* Actor,
	bool bRecursivelyIncludeAttachedActors)
{
	TArray<AActor*> OutActors;
	Actor->GetAttachedActors(OutActors, false, bRecursivelyIncludeAttachedActors);
	return OutActors;
}

TArray<AActor*> UAngelscriptActorLibrary::GetAttachedActorsOfClass(
	const AActor* Actor,
	const TSubclassOf<AActor> ActorClass,
	bool bRecursivelyIncludeAttachedActors)
{
	TArray<AActor*> OutActors;
	if (Actor == nullptr || ActorClass.Get() == nullptr)
		return OutActors;

	Actor->GetAttachedActors(OutActors, false, bRecursivelyIncludeAttachedActors);

	for (int32 Index = OutActors.Num() - 1; Index >= 0; --Index)
	{
		if (OutActors[Index] == nullptr || !OutActors[Index]->IsA(ActorClass))
			OutActors.RemoveAt(Index, EAllowShrinking::No);
	}

	return OutActors;
}
