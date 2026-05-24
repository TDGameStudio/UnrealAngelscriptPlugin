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
//   into six TEST_METHODs, each with its own FCoverageModuleScope. The custom
//   address-based invocation helpers are retained for the bool+out-param
//   calling convention. World/collision setup is shared via BEFORE_EACH.
// ============================================================================

#include "CQTest.h"
#include "Shared/AngelscriptTestMacros.h"
#include "Shared/AngelscriptTestModuleScope.h"
#include "Shared/AngelscriptBindingsAssertions.h"

#include "Components/ActorTestSpawner.h"
#include "Components/BoxComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Misc/ScopeExit.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptTestSupport;
using namespace AngelscriptTestBindings;
using namespace AngelscriptReflectiveAccess;

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

	bool SetArgAddressChecked(
		FAutomationTestBase& Test,
		asIScriptContext& Context,
		asUINT ArgumentIndex,
		void* Address,
		const TCHAR* ContextLabel)
	{
		return Test.TestEqual(
			*FString::Printf(TEXT("%s should bind address argument %u"), ContextLabel, static_cast<uint32>(ArgumentIndex)),
			Context.SetArgAddress(ArgumentIndex, Address),
			static_cast<int32>(asSUCCESS));
	}

	// TODO(refactor-as-test-shared-layout-and-naming): migrate ExecuteBoolFunction / ExecuteAddressBoolFunction<T> to Shared/AngelscriptTestExecute.h once the Execute* naming family lands in Phase 3.
	bool ExecuteBoolFunction(
		FAutomationTestBase& Test,
		FAngelscriptEngine& Engine,
		asIScriptModule& Module,
		const FString& FunctionDecl,
		TFunctionRef<bool(asIScriptContext&)> BindArguments,
		const TCHAR* ContextLabel,
		bool& OutResult)
	{
		asIScriptFunction* Function = GetFunctionByDecl(Test, Module, FunctionDecl);
		if (Function == nullptr)
		{
			return false;
		}

		FAngelscriptEngineScope EngineScope(Engine);
		asIScriptContext* Context = Engine.CreateContext();
		if (!Test.TestNotNull(*FString::Printf(TEXT("%s should create an execution context"), ContextLabel), Context))
		{
			return false;
		}

		ON_SCOPE_EXIT
		{
			Context->Release();
		};

		const int PrepareResult = Context->Prepare(Function);
		if (!Test.TestEqual(*FString::Printf(TEXT("%s should prepare successfully"), ContextLabel), PrepareResult, static_cast<int32>(asSUCCESS)))
		{
			return false;
		}

		if (!BindArguments(*Context))
		{
			return false;
		}

		const int ExecuteResult = Context->Execute();
		if (!Test.TestEqual(*FString::Printf(TEXT("%s should execute successfully"), ContextLabel), ExecuteResult, static_cast<int32>(asEXECUTION_FINISHED)))
		{
			return false;
		}

		OutResult = Context->GetReturnByte() != 0;
		return true;
	}

	template <typename TValue>
	bool ExecuteAddressBoolFunction(
		FAutomationTestBase& Test,
		FAngelscriptEngine& Engine,
		asIScriptModule& Module,
		const TCHAR* FunctionDecl,
		const TCHAR* ContextLabel,
		TValue& OutValue,
		bool& OutResult)
	{
		return ExecuteBoolFunction(
			Test,
			Engine,
			Module,
			FunctionDecl,
			[&](asIScriptContext& Context)
			{
				return SetArgAddressChecked(Test, Context, 0, &OutValue, ContextLabel);
			},
			ContextLabel,
			OutResult);
	}

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
		bool bPassed = true;
		bPassed &= Test.TestEqual(*FString::Printf(TEXT("%s should preserve the bool return value"), Label), bScriptReturnValue, bNativeReturnValue);
		bPassed &= Test.TestEqual(*FString::Printf(TEXT("%s should preserve the hit actor"), Label), ScriptHit.GetActor(), NativeHit.GetActor());
		bPassed &= Test.TestEqual(*FString::Printf(TEXT("%s should preserve the hit component"), Label), ScriptHit.GetComponent(), NativeHit.GetComponent());
		bPassed &= Test.TestEqual(*FString::Printf(TEXT("%s should preserve the blocking-hit flag"), Label), ScriptHit.bBlockingHit, NativeHit.bBlockingHit);
		bPassed &= Test.TestTrue(*FString::Printf(TEXT("%s should preserve the hit location"), Label), ScriptHit.Location.Equals(NativeHit.Location, 0.05f));
		bPassed &= Test.TestTrue(*FString::Printf(TEXT("%s should preserve the impact point"), Label), ScriptHit.ImpactPoint.Equals(NativeHit.ImpactPoint, 0.05f));
		return bPassed;
	}

	template <typename TResult>
	bool ExpectArrayParity(FAutomationTestBase& Test, const TCHAR* Label, bool bScriptReturnValue, bool bNativeReturnValue, const TArray<TResult>& ScriptResults, const TArray<TResult>& NativeResults)
	{
		bool bPassed = true;
		bPassed &= Test.TestEqual(*FString::Printf(TEXT("%s should preserve the bool return value"), Label), bScriptReturnValue, bNativeReturnValue);
		bPassed &= Test.TestEqual(*FString::Printf(TEXT("%s should preserve the result count"), Label), ScriptResults.Num(), NativeResults.Num());
		if (ScriptResults.Num() > 0 && NativeResults.Num() > 0)
		{
			bPassed &= Test.TestEqual(*FString::Printf(TEXT("%s should preserve the first result actor"), Label), ScriptResults[0].GetActor(), NativeResults[0].GetActor());
			bPassed &= Test.TestEqual(*FString::Printf(TEXT("%s should preserve the first result component"), Label), ScriptResults[0].GetComponent(), NativeResults[0].GetComponent());
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
		if (!ExecuteAddressBoolFunction(*TestRunner, Engine, M, TEXT("bool RunLineTraceSingleByChannelHit(FHitResult& OutHit)"), TEXT("RunLineTraceSingleByChannelHit"), ScriptLineHit, bScriptLineHit))
		{
			return;
		}
		ExpectHitResultParity(*TestRunner, TEXT("LineTraceSingleByChannel hit"), bScriptLineHit, bNativeLineHit, ScriptLineHit, NativeLineHit);
		TestRunner->TestEqual(TEXT("LineTraceSingleByChannel hit should identify the blocker actor"), ScriptLineHit.GetActor(), static_cast<AActor*>(&BlockingActor));
		TestRunner->TestEqual(TEXT("LineTraceSingleByChannel hit should identify the blocker component"), ScriptLineHit.GetComponent(), static_cast<UPrimitiveComponent*>(BlockingBox));
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
		if (!ExecuteAddressBoolFunction(*TestRunner, Engine, M, TEXT("bool RunLineTraceMultiByChannelHit(TArray<FHitResult>& OutHits)"), TEXT("RunLineTraceMultiByChannelHit"), ScriptLineHits, bScriptLineMultiHit))
		{
			return;
		}
		ExpectArrayParity(*TestRunner, TEXT("LineTraceMultiByChannel hit"), bScriptLineMultiHit, bNativeLineMultiHit, ScriptLineHits, NativeLineHits);
		TestRunner->TestTrue(TEXT("LineTraceMultiByChannel hit should produce at least one hit"), ScriptLineHits.Num() >= 1);
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
		if (!ExecuteAddressBoolFunction(*TestRunner, Engine, M, TEXT("bool RunLineTraceMultiByChannelMiss(TArray<FHitResult>& OutHits)"), TEXT("RunLineTraceMultiByChannelMiss"), ScriptLineMissHits, bScriptLineMultiMiss))
		{
			return;
		}
		ExpectArrayParity(*TestRunner, TEXT("LineTraceMultiByChannel miss"), bScriptLineMultiMiss, bNativeLineMultiMiss, ScriptLineMissHits, NativeLineMissHits);
		TestRunner->TestEqual(TEXT("LineTraceMultiByChannel miss should clear stale output hits"), ScriptLineMissHits.Num(), 0);
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
		if (!ExecuteAddressBoolFunction(*TestRunner, Engine, M, TEXT("bool RunSweepSingleByObjectTypeHit(FHitResult& OutHit)"), TEXT("RunSweepSingleByObjectTypeHit"), ScriptObjectSweepHit, bScriptObjectSweepHit))
		{
			return;
		}
		ExpectHitResultParity(*TestRunner, TEXT("SweepSingleByObjectType hit"), bScriptObjectSweepHit, bNativeObjectSweepHit, ScriptObjectSweepHit, NativeObjectSweepHit);
		TestRunner->TestEqual(TEXT("SweepSingleByObjectType hit should identify the blocker actor"), ScriptObjectSweepHit.GetActor(), static_cast<AActor*>(&BlockingActor));
		TestRunner->TestEqual(TEXT("SweepSingleByObjectType hit should identify the blocker component"), ScriptObjectSweepHit.GetComponent(), static_cast<UPrimitiveComponent*>(BlockingBox));
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
		if (!ExecuteAddressBoolFunction(*TestRunner, Engine, M, TEXT("bool RunOverlapMultiByProfileHit(TArray<FOverlapResult>& OutOverlaps)"), TEXT("RunOverlapMultiByProfileHit"), ScriptProfileOverlaps, bScriptProfileOverlapHit))
		{
			return;
		}
		ExpectArrayParity(*TestRunner, TEXT("OverlapMultiByProfile hit"), bScriptProfileOverlapHit, bNativeProfileOverlapHit, ScriptProfileOverlaps, NativeProfileOverlaps);
		TestRunner->TestTrue(TEXT("OverlapMultiByProfile hit should include the overlap target component"), OverlapsContainComponent(ScriptProfileOverlaps, OverlapBox));
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
		if (!ExecuteAddressBoolFunction(*TestRunner, Engine, M, TEXT("bool RunOverlapMultiByProfileMiss(TArray<FOverlapResult>& OutOverlaps)"), TEXT("RunOverlapMultiByProfileMiss"), ScriptProfileMissOverlaps, bScriptProfileOverlapMiss))
		{
			return;
		}
		ExpectArrayParity(*TestRunner, TEXT("OverlapMultiByProfile miss"), bScriptProfileOverlapMiss, bNativeProfileOverlapMiss, ScriptProfileMissOverlaps, NativeProfileMissOverlaps);
	}
};

#endif
