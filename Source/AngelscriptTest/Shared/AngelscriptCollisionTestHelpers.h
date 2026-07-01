#pragma once

#include "CoreMinimal.h"
#include "Components/BoxComponent.h"
#include "Engine/CollisionProfile.h"
#include "GameFramework/Actor.h"

/**
 * AngelscriptCollisionTestHelpers — shared box-collision setup helpers used
 * by the world-collision Bindings tests. Two variants exist because the
 * legacy fixtures were split between explicit channel/response wiring and
 * profile-based wiring; both behaviours need to be preserved.
 *
 * These helpers were extracted from individual test .cpp files to remove the
 * ODR-violating `AddCollisionBox` collisions that broke unity builds.
 */

#if WITH_ANGELSCRIPT_UNITTESTS

/**
 * Spawn a UBoxComponent on `Owner` configured for query-only collision
 * with `WorldDynamic` object type and `Block` response on every channel.
 * Mirrors the original `Bindings/AngelscriptWorldCollisionAsyncBindingsTests`
 * helper. Component is registered and assigned as root.
 */
inline UBoxComponent* AddQueryOnlyCollisionBox(
	AActor& Owner,
	const FName ComponentName,
	const FVector& BoxExtent,
	const FVector& WorldLocation)
{
	UBoxComponent* BoxComponent = NewObject<UBoxComponent>(&Owner, ComponentName);
	check(BoxComponent != nullptr);

	Owner.AddInstanceComponent(BoxComponent);
	Owner.SetRootComponent(BoxComponent);
	BoxComponent->RegisterComponent();
	BoxComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	BoxComponent->SetCollisionObjectType(ECC_WorldDynamic);
	BoxComponent->SetCollisionResponseToAllChannels(ECR_Block);
	BoxComponent->SetGenerateOverlapEvents(true);
	BoxComponent->SetBoxExtent(BoxExtent);
	BoxComponent->SetWorldLocation(WorldLocation);
	return BoxComponent;
}

/**
 * Spawn a UBoxComponent on `Owner` configured via the
 * `BlockAllDynamic_ProfileName` collision profile. Mirrors the original
 * `Bindings/AngelscriptWorldCollisionFunctionLibrary*Tests` helper.
 */
inline UBoxComponent* AddBlockAllDynamicCollisionBox(
	AActor& Owner,
	FName ComponentName,
	const FVector& BoxExtent,
	const FVector& WorldLocation)
{
	UBoxComponent* BoxComponent = NewObject<UBoxComponent>(&Owner, ComponentName);
	check(BoxComponent != nullptr);
	Owner.AddInstanceComponent(BoxComponent);
	Owner.SetRootComponent(BoxComponent);
	BoxComponent->RegisterComponent();
	BoxComponent->SetCollisionProfileName(UCollisionProfile::BlockAllDynamic_ProfileName);
	BoxComponent->SetGenerateOverlapEvents(true);
	BoxComponent->SetBoxExtent(BoxExtent);
	BoxComponent->SetWorldLocation(WorldLocation);
	return BoxComponent;
}


#endif // WITH_ANGELSCRIPT_UNITTESTS
