// ============================================================================
// AngelscriptWorldCollisionFunctionLibraryTraceTests.cpp
//
// World collision trace binding coverage — CQTest refactor.
// Automation ID:
//   Angelscript.TestModule.FunctionLibraries.WorldCollisionTrace.*
//
// Sections:
//   LineTraceSingle        — LineTraceSingleByChannel hit parity
//   LineTraceMultiHit      — LineTraceMultiByChannel hit parity
//   LineTraceMultiMiss     — LineTraceMultiByChannel miss clears output
//   SweepSingleByObject    — SweepSingleByObjectType hit parity
//   OverlapMultiByProfile  — OverlapMultiByProfile hit containment
//   OverlapMultiMiss       — OverlapMultiByProfile miss parity
//
// CQTest adaptation notes:
//   Original single IMPLEMENT_SIMPLE_AUTOMATION_TEST monolithic function split
//   into six TEST_METHODs, each with its own FScopedAngelscriptModule.
//   Bool+address out-params use Bindings/AngelscriptWorldCollisionBindingsTestHelpers.h.
//   World/collision setup is shared via BEFORE_EACH.
// ============================================================================

#include "CQTest.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestModuleScope.h"
#include "AngelscriptTestExecute.h"
#include "Bindings/AngelscriptWorldCollisionBindingsTestHelpers.h"

#include "Components/ActorTestSpawner.h"
#include "Components/BoxComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Misc/ScopeExit.h"

#if WITH_DEV_AUTOMATION_TESTS


// ----------------------------------------------------------------------------
// Profile
// ----------------------------------------------------------------------------


// ----------------------------------------------------------------------------
// Shared helpers
// ----------------------------------------------------------------------------

namespace WorldCollisionTraceTestHelpers
{
	static const FVector BlockingTargetLocation(0.0f, 0.0f, 0.0f);
	static const FVector OverlapTargetLocation(0.0f, 150.0f, 0.0f);
	static const FVector LineTraceHitStart(-200.0f, 0.0f, 0.0f);
	static const FVector LineTraceHitEnd(200.0f, 0.0f, 0.0f);
	static const FVector LineTraceMissStart(-200.0f, -200.0f, 0.0f);
	static const FVector LineTraceMissEnd(200.0f, -200.0f, 0.0f);
	static const FVector TargetExtent(50.0f, 50.0f, 50.0f);
	static const FVector OverlapExtent(40.0f, 40.0f, 40.0f);
	static const FVector SweepExtent(30.0f, 30.0f, 30.0f);
	static const FVector OverlapShapeExtent(45.0f, 45.0f, 45.0f);
	static const FVector ProfileMissLocation(0.0f, -150.0f, 0.0f);
	static const FQuat IdentityRotation = FQuat::Identity;

	UBoxComponent* AddCollisionBox(AActor& Owner, FName ComponentName, const FVector& BoxExtent, const FVector& WorldLocation)
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

	bool ExpectHitResultParity(FAutomationTestBase& Test, const TCHAR* Label, bool bScriptReturnValue, bool bNativeReturnValue, const FHitResult& ScriptHit, const FHitResult& NativeHit)
	{
		FNoDiscardAsserter Assert(Test);
		bool bPassed = true;
		bPassed &= Assert.AreEqual(bNativeReturnValue, bScriptReturnValue, *FString::Printf(TEXT("%s should preserve the bool return value"), Label));
		bPassed &= Assert.AreEqual(NativeHit.GetActor(), ScriptHit.GetActor(), *FString::Printf(TEXT("%s should preserve the hit actor"), Label));
		bPassed &= Assert.AreEqual(NativeHit.GetComponent(), ScriptHit.GetComponent(), *FString::Printf(TEXT("%s should preserve the hit component"), Label));
		bPassed &= Assert.AreEqual(NativeHit.bBlockingHit, ScriptHit.bBlockingHit, *FString::Printf(TEXT("%s should preserve the blocking-hit flag"), Label));
		bPassed &= Assert.IsTrue(ScriptHit.Location.Equals(NativeHit.Location, 0.05f), *FString::Printf(TEXT("%s should preserve the hit location"), Label));
		bPassed &= Assert.IsTrue(ScriptHit.ImpactPoint.Equals(NativeHit.ImpactPoint, 0.05f), *FString::Printf(TEXT("%s should preserve the impact point"), Label));
		return bPassed;
	}

	template <typename TResult>
	bool ExpectArrayParity(FAutomationTestBase& Test, const TCHAR* Label, bool bScriptReturnValue, bool bNativeReturnValue, const TArray<TResult>& ScriptResults, const TArray<TResult>& NativeResults)
	{
		FNoDiscardAsserter Assert(Test);
		bool bPassed = true;
		bPassed &= Assert.AreEqual(bNativeReturnValue, bScriptReturnValue, *FString::Printf(TEXT("%s should preserve the bool return value"), Label));
		bPassed &= Assert.AreEqual(NativeResults.Num(), ScriptResults.Num(), *FString::Printf(TEXT("%s should preserve the result count"), Label));
		if (ScriptResults.Num() > 0 && NativeResults.Num() > 0)
		{
			bPassed &= Assert.AreEqual(NativeResults[0].GetActor(), ScriptResults[0].GetActor(), *FString::Printf(TEXT("%s should preserve the first result actor"), Label));
			bPassed &= Assert.AreEqual(NativeResults[0].GetComponent(), ScriptResults[0].GetComponent(), *FString::Printf(TEXT("%s should preserve the first result component"), Label));
		}
		return bPassed;
	}

	bool OverlapsContainComponent(const TArray<FOverlapResult>& Overlaps, const UPrimitiveComponent* Component)
	{
		return Overlaps.ContainsByPredicate([Component](const FOverlapResult& Overlap)
		{
			return Overlap.GetComponent() == Component;
		});
	}
}

using namespace WorldCollisionTraceTestHelpers;

// ----------------------------------------------------------------------------
// Test class
// ----------------------------------------------------------------------------

TEST_CLASS_WITH_FLAGS(FAngelscriptWorldCollisionTraceBindingsTest,
	"Angelscript.TestModule.FunctionLibraries.WorldCollisionTrace",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	BEFORE_ALL()
	{
		ASTEST_CREATE_ENGINE();
	}

	AFTER_ALL() { FAngelscriptEngine& Engine = ASTEST_GET_ENGINE(); ASTEST_RESET_ENGINE(Engine); }

	// ====================================================================
	// Section: LineTraceSingle
	// ====================================================================

	TEST_METHOD(LineTraceSingle)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASWorldCollisionTrace_LineTraceSingle"), TEXT(R"(
bool RunLineTraceSingleByChannelHit(FHitResult& OutHit)
{
	return System::LineTraceSingleByChannel(OutHit, FVector(-200.0f, 0.0f, 0.0f), FVector(200.0f, 0.0f, 0.0f), ECollisionChannel::ECC_Visibility);
}
)"));
		if (!Mod.IsValid()) return;
		auto& M = Mod.GetModule();

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor& BlockingActor = Spawner.SpawnActor<AActor>();
		UBoxComponent* BlockingBox = WorldCollisionTraceTestHelpers::AddCollisionBox(BlockingActor, TEXT("BlockingTarget"), TargetExtent, BlockingTargetLocation);
		ASSERT_THAT(IsNotNull(BlockingBox));

		UWorld* World = BlockingActor.GetWorld();
		ASSERT_THAT(IsNotNull(World));
		FScopedTestWorldContextScope WorldContextScope(&BlockingActor);

		FHitResult NativeLineHit;
		const bool bNativeLineHit = World->LineTraceSingleByChannel(NativeLineHit, LineTraceHitStart, LineTraceHitEnd, ECC_Visibility);
		FHitResult ScriptLineHit;
		bool bScriptLineHit = false;
		if (!WorldCollisionExecuteAddressBoolFunction(*TestRunner, Engine, M, TEXT("bool RunLineTraceSingleByChannelHit(FHitResult& OutHit)"), TEXT("RunLineTraceSingleByChannelHit"), ScriptLineHit, bScriptLineHit))
		{
			return;
		}
		ExpectHitResultParity(*TestRunner, TEXT("LineTraceSingleByChannel hit"), bScriptLineHit, bNativeLineHit, ScriptLineHit, NativeLineHit);
		ASSERT_THAT(AreEqual(static_cast<AActor*>(&BlockingActor), ScriptLineHit.GetActor(), TEXT("LineTraceSingleByChannel hit should identify the blocker actor")));
		ASSERT_THAT(AreEqual(static_cast<UPrimitiveComponent*>(BlockingBox), ScriptLineHit.GetComponent(), TEXT("LineTraceSingleByChannel hit should identify the blocker component")));
	}

	// ====================================================================
	// Section: LineTraceMultiHit
	// ====================================================================

	TEST_METHOD(LineTraceMultiHit)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASWorldCollisionTrace_LineTraceMultiHit"), TEXT(R"(
bool RunLineTraceMultiByChannelHit(TArray<FHitResult>& OutHits)
{
	return System::LineTraceMultiByChannel(OutHits, FVector(-200.0f, 0.0f, 0.0f), FVector(200.0f, 0.0f, 0.0f), ECollisionChannel::ECC_Visibility);
}
)"));
		if (!Mod.IsValid()) return;
		auto& M = Mod.GetModule();

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor& BlockingActor = Spawner.SpawnActor<AActor>();
		UBoxComponent* BlockingBox = WorldCollisionTraceTestHelpers::AddCollisionBox(BlockingActor, TEXT("BlockingTarget"), TargetExtent, BlockingTargetLocation);
		ASSERT_THAT(IsNotNull(BlockingBox));

		UWorld* World = BlockingActor.GetWorld();
		ASSERT_THAT(IsNotNull(World));
		FScopedTestWorldContextScope WorldContextScope(&BlockingActor);

		TArray<FHitResult> NativeLineHits;
		const bool bNativeLineMultiHit = World->LineTraceMultiByChannel(NativeLineHits, LineTraceHitStart, LineTraceHitEnd, ECC_Visibility);
		TArray<FHitResult> ScriptLineHits;
		bool bScriptLineMultiHit = false;
		if (!WorldCollisionExecuteAddressBoolFunction(*TestRunner, Engine, M, TEXT("bool RunLineTraceMultiByChannelHit(TArray<FHitResult>& OutHits)"), TEXT("RunLineTraceMultiByChannelHit"), ScriptLineHits, bScriptLineMultiHit))
		{
			return;
		}
		ExpectArrayParity(*TestRunner, TEXT("LineTraceMultiByChannel hit"), bScriptLineMultiHit, bNativeLineMultiHit, ScriptLineHits, NativeLineHits);
		ASSERT_THAT(IsTrue(ScriptLineHits.Num() >= 1, TEXT("LineTraceMultiByChannel hit should produce at least one hit")));
	}

	// ====================================================================
	// Section: LineTraceMultiMiss
	// ====================================================================

	TEST_METHOD(LineTraceMultiMiss)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASWorldCollisionTrace_LineTraceMultiMiss"), TEXT(R"(
bool RunLineTraceMultiByChannelMiss(TArray<FHitResult>& OutHits)
{
	return System::LineTraceMultiByChannel(OutHits, FVector(-200.0f, -200.0f, 0.0f), FVector(200.0f, -200.0f, 0.0f), ECollisionChannel::ECC_Visibility);
}
)"));
		if (!Mod.IsValid()) return;
		auto& M = Mod.GetModule();

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor& BlockingActor = Spawner.SpawnActor<AActor>();
		UBoxComponent* BlockingBox = WorldCollisionTraceTestHelpers::AddCollisionBox(BlockingActor, TEXT("BlockingTarget"), TargetExtent, BlockingTargetLocation);
		ASSERT_THAT(IsNotNull(BlockingBox));

		UWorld* World = BlockingActor.GetWorld();
		ASSERT_THAT(IsNotNull(World));
		FScopedTestWorldContextScope WorldContextScope(&BlockingActor);

		TArray<FHitResult> NativeLineMissHits;
		NativeLineMissHits.AddDefaulted();
		const bool bNativeLineMultiMiss = World->LineTraceMultiByChannel(NativeLineMissHits, LineTraceMissStart, LineTraceMissEnd, ECC_Visibility);
		TArray<FHitResult> ScriptLineMissHits;
		ScriptLineMissHits.AddDefaulted();
		bool bScriptLineMultiMiss = false;
		if (!WorldCollisionExecuteAddressBoolFunction(*TestRunner, Engine, M, TEXT("bool RunLineTraceMultiByChannelMiss(TArray<FHitResult>& OutHits)"), TEXT("RunLineTraceMultiByChannelMiss"), ScriptLineMissHits, bScriptLineMultiMiss))
		{
			return;
		}
		ExpectArrayParity(*TestRunner, TEXT("LineTraceMultiByChannel miss"), bScriptLineMultiMiss, bNativeLineMultiMiss, ScriptLineMissHits, NativeLineMissHits);
		ASSERT_THAT(AreEqual(0, ScriptLineMissHits.Num(), TEXT("LineTraceMultiByChannel miss should clear stale output hits")));
	}

	// ====================================================================
	// Section: SweepSingleByObject
	// ====================================================================

	TEST_METHOD(SweepSingleByObject)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASWorldCollisionTrace_SweepSingleByObject"), TEXT(R"(
bool RunSweepSingleByObjectTypeHit(FHitResult& OutHit)
{
	FCollisionObjectQueryParams ObjectQueryParams;
	ObjectQueryParams.AddObjectTypesToQuery(ECollisionChannel::ECC_WorldDynamic);
	const FCollisionShape Shape = FCollisionShape::MakeBox(FVector(30.0f, 30.0f, 30.0f));
	return System::SweepSingleByObjectType(OutHit, FVector(-200.0f, 0.0f, 0.0f), FVector(200.0f, 0.0f, 0.0f), FQuat::Identity, ObjectQueryParams, Shape);
}
)"));
		if (!Mod.IsValid()) return;
		auto& M = Mod.GetModule();

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor& BlockingActor = Spawner.SpawnActor<AActor>();
		UBoxComponent* BlockingBox = WorldCollisionTraceTestHelpers::AddCollisionBox(BlockingActor, TEXT("BlockingTarget"), TargetExtent, BlockingTargetLocation);
		ASSERT_THAT(IsNotNull(BlockingBox));

		UWorld* World = BlockingActor.GetWorld();
		ASSERT_THAT(IsNotNull(World));
		FScopedTestWorldContextScope WorldContextScope(&BlockingActor);

		const FCollisionShape SweepShape = FCollisionShape::MakeBox(SweepExtent);
		FCollisionObjectQueryParams ObjectQueryParams;
		ObjectQueryParams.AddObjectTypesToQuery(ECC_WorldDynamic);

		FHitResult NativeObjectSweepHit;
		const bool bNativeObjectSweepHit = World->SweepSingleByObjectType(NativeObjectSweepHit, LineTraceHitStart, LineTraceHitEnd, IdentityRotation, ObjectQueryParams, SweepShape);
		FHitResult ScriptObjectSweepHit;
		bool bScriptObjectSweepHit = false;
		if (!WorldCollisionExecuteAddressBoolFunction(*TestRunner, Engine, M, TEXT("bool RunSweepSingleByObjectTypeHit(FHitResult& OutHit)"), TEXT("RunSweepSingleByObjectTypeHit"), ScriptObjectSweepHit, bScriptObjectSweepHit))
		{
			return;
		}
		ExpectHitResultParity(*TestRunner, TEXT("SweepSingleByObjectType hit"), bScriptObjectSweepHit, bNativeObjectSweepHit, ScriptObjectSweepHit, NativeObjectSweepHit);
		ASSERT_THAT(AreEqual(static_cast<AActor*>(&BlockingActor), ScriptObjectSweepHit.GetActor(), TEXT("SweepSingleByObjectType hit should identify the blocker actor")));
		ASSERT_THAT(AreEqual(static_cast<UPrimitiveComponent*>(BlockingBox), ScriptObjectSweepHit.GetComponent(), TEXT("SweepSingleByObjectType hit should identify the blocker component")));
	}

	// ====================================================================
	// Section: OverlapMultiByProfile
	// ====================================================================

	TEST_METHOD(OverlapMultiByProfile)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASWorldCollisionTrace_OverlapMultiByProfile"), TEXT(R"(
bool RunOverlapMultiByProfileHit(TArray<FOverlapResult>& OutOverlaps)
{
	const FCollisionShape Shape = FCollisionShape::MakeBox(FVector(45.0f, 45.0f, 45.0f));
	return System::OverlapMultiByProfile(OutOverlaps, FVector(0.0f, 150.0f, 0.0f), FQuat::Identity, CollisionProfile::BlockAllDynamic, Shape);
}
)"));
		if (!Mod.IsValid()) return;
		auto& M = Mod.GetModule();

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor& BlockingActor = Spawner.SpawnActor<AActor>();
		WorldCollisionTraceTestHelpers::AddCollisionBox(BlockingActor, TEXT("BlockingTarget"), TargetExtent, BlockingTargetLocation);
		AActor& OverlapActor = Spawner.SpawnActor<AActor>();
		UBoxComponent* OverlapBox = WorldCollisionTraceTestHelpers::AddCollisionBox(OverlapActor, TEXT("OverlapTarget"), OverlapExtent, OverlapTargetLocation);
		ASSERT_THAT(IsNotNull(OverlapBox));

		UWorld* World = BlockingActor.GetWorld();
		ASSERT_THAT(IsNotNull(World));
		FScopedTestWorldContextScope WorldContextScope(&BlockingActor);

		const FCollisionShape ProfileOverlapShape = FCollisionShape::MakeBox(OverlapShapeExtent);
		TArray<FOverlapResult> NativeProfileOverlaps;
		const bool bNativeProfileOverlapHit = World->OverlapMultiByProfile(NativeProfileOverlaps, OverlapTargetLocation, IdentityRotation, UCollisionProfile::BlockAllDynamic_ProfileName, ProfileOverlapShape);
		TArray<FOverlapResult> ScriptProfileOverlaps;
		bool bScriptProfileOverlapHit = false;
		if (!WorldCollisionExecuteAddressBoolFunction(*TestRunner, Engine, M, TEXT("bool RunOverlapMultiByProfileHit(TArray<FOverlapResult>& OutOverlaps)"), TEXT("RunOverlapMultiByProfileHit"), ScriptProfileOverlaps, bScriptProfileOverlapHit))
		{
			return;
		}
		ExpectArrayParity(*TestRunner, TEXT("OverlapMultiByProfile hit"), bScriptProfileOverlapHit, bNativeProfileOverlapHit, ScriptProfileOverlaps, NativeProfileOverlaps);
		ASSERT_THAT(IsTrue(OverlapsContainComponent(ScriptProfileOverlaps, OverlapBox), TEXT("OverlapMultiByProfile hit should include the overlap target component")));
	}

	// ====================================================================
	// Section: OverlapMultiMiss
	// ====================================================================

	TEST_METHOD(OverlapMultiMiss)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASWorldCollisionTrace_OverlapMultiMiss"), TEXT(R"(
bool RunOverlapMultiByProfileMiss(TArray<FOverlapResult>& OutOverlaps)
{
	const FCollisionShape Shape = FCollisionShape::MakeBox(FVector(45.0f, 45.0f, 45.0f));
	return System::OverlapMultiByProfile(OutOverlaps, FVector(0.0f, -150.0f, 0.0f), FQuat::Identity, CollisionProfile::BlockAllDynamic, Shape);
}
)"));
		if (!Mod.IsValid()) return;
		auto& M = Mod.GetModule();

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor& BlockingActor = Spawner.SpawnActor<AActor>();
		WorldCollisionTraceTestHelpers::AddCollisionBox(BlockingActor, TEXT("BlockingTarget"), TargetExtent, BlockingTargetLocation);

		UWorld* World = BlockingActor.GetWorld();
		ASSERT_THAT(IsNotNull(World));
		FScopedTestWorldContextScope WorldContextScope(&BlockingActor);

		const FCollisionShape ProfileOverlapShape = FCollisionShape::MakeBox(OverlapShapeExtent);
		TArray<FOverlapResult> NativeProfileMissOverlaps;
		NativeProfileMissOverlaps.AddDefaulted();
		const bool bNativeProfileOverlapMiss = World->OverlapMultiByProfile(NativeProfileMissOverlaps, ProfileMissLocation, IdentityRotation, UCollisionProfile::BlockAllDynamic_ProfileName, ProfileOverlapShape);
		TArray<FOverlapResult> ScriptProfileMissOverlaps;
		ScriptProfileMissOverlaps.AddDefaulted();
		bool bScriptProfileOverlapMiss = false;
		if (!WorldCollisionExecuteAddressBoolFunction(*TestRunner, Engine, M, TEXT("bool RunOverlapMultiByProfileMiss(TArray<FOverlapResult>& OutOverlaps)"), TEXT("RunOverlapMultiByProfileMiss"), ScriptProfileMissOverlaps, bScriptProfileOverlapMiss))
		{
			return;
		}
		ExpectArrayParity(*TestRunner, TEXT("OverlapMultiByProfile miss"), bScriptProfileOverlapMiss, bNativeProfileOverlapMiss, ScriptProfileMissOverlaps, NativeProfileMissOverlaps);
	}
};

#endif
