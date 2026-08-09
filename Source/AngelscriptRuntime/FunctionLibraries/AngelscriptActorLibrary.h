#pragma once
#include "GameFramework/Actor.h"
#include "AngelscriptActorLibrary.generated.h"

// Hybrid library after Plan_FunctionLibrariesCleanup.md Phase 2 (2026-04-28):
// only function shapes that UE-native AActor BlueprintCallable API does NOT cover survive here.
// All UFunctions are tagged BlueprintCallable so they enter the reflective binding path
// (Bind_BlueprintType.cpp:1428-1437); the historical bare UFUNCTION() forms were dead code
// (no manual Bind_*.cpp wiring + no BlueprintCallable/ScriptCallable flag => never bound).
// 22 redundant wrappers around UE-native FRotator/FVector/FTransform AActor APIs were removed;
// 8 fork-distinctive surfaces remain: 6 FQuat overloads
// + 2 editor-only construction-script utilities. Hazelight upstream parity holds via ScriptName
// aliases on the FQuat overloads.

UCLASS(meta = (ScriptMixin = "AActor"))
class ANGELSCRIPTRUNTIME_API UAngelscriptActorLibrary : public UObject
{
	GENERATED_BODY()

public:

	UFUNCTION(BlueprintCallable, Meta = (ScriptName = "SetActorRelativeRotation", NotAngelscriptProperty))
	static void SetActorRelativeRotationQuat(AActor* Actor, const FQuat& NewRelativeRotation);

	UFUNCTION(BlueprintCallable, Meta = (ScriptName = "SetActorRotation", NotAngelscriptProperty))
	static void SetActorRotationQuat(AActor* Actor, const FQuat& NewRotation);

	UFUNCTION(BlueprintCallable, Meta = (ScriptName = "SetActorLocationAndRotation"))
	static void SetActorLocationAndRotationQuat(AActor* Actor, const FVector& NewLocation, const FQuat& NewRotation, bool bTeleport = false);

	UFUNCTION(BlueprintCallable, Meta = (ScriptTrivial))
	static void SetActorQuat(AActor* Actor, const FQuat& NewRotation);

	UFUNCTION(BlueprintCallable, Meta = (ScriptName = "AddActorLocalRotation", NotAngelscriptProperty))
	static void AddActorLocalRotationQuat(AActor* Actor, const FQuat& DeltaRotation);

	UFUNCTION(BlueprintCallable, Meta = (ScriptName = "AddActorWorldRotation"))
	static void AddActorWorldRotationQuat(AActor* Actor, const FQuat& DeltaRotation);

	UFUNCTION(BlueprintCallable, Meta = (ScriptName = "SetActorLocation", NotAngelscriptProperty, NotInAngelscript = "true"))
	static bool SetActorLocationAdvanced(AActor* Actor, const FVector& NewLocation, bool bSweep, FHitResult& SweepHitResult, bool bTeleport = false);

	UFUNCTION(BlueprintCallable)
	static void SetbRunConstructionScriptOnDrag(AActor* Actor, bool Value);

#if WITH_EDITOR
	UFUNCTION(BlueprintCallable)
	static void RerunConstructionScripts(AActor* Actor);
#endif

	/** Find all Actors which are attached directly to a component in this actor */
	UFUNCTION(BlueprintCallable, Meta = (ScriptTrivial, NotAngelscriptProperty))
	static TArray<AActor*> GetAttachedActors(const AActor* Actor, bool bRecursivelyIncludeAttachedActors = false);

	/** Find all Actors of a particular class which are attached directly to a component in this actor */
	UFUNCTION(BlueprintCallable, Meta = (ScriptTrivial, DeterminesOutputType = "ActorClass", NotAngelscriptProperty))
	static TArray<AActor*> GetAttachedActorsOfClass(const AActor* Actor, const TSubclassOf<AActor> ActorClass, bool bRecursivelyIncludeAttachedActors = false);
};
