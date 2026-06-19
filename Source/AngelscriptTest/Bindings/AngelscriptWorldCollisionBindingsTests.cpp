// ============================================================================
// AngelscriptWorldCollisionBindingsTests.cpp
//
// World collision sync query binding coverage -- CQTest refactor. Automation IDs:
//   Angelscript.TestModule.Bindings.WorldCollision.FAngelscriptWorldCollisionBindingsTest.*
//
// Sections:
//   SyncQueries — LineTraceSingle/Multi, SweepSingle, OverlapAny,
//                 ComponentOverlapMulti (hit/miss parity)
//
// CQTest adaptation notes:
//   Single legacy automation test converted to TEST_CLASS.
//   Uses ASTEST_CREATE_ENGINE_FULL (world-based) with FActorTestSpawner.
//   Custom address-based invocation helpers retained for the
//   bool+out-param calling convention.
// ============================================================================

#include "CQTest.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestUtilities.h"
#include "AngelscriptTestModuleScope.h"
#include "AngelscriptTestExecute.h"
#include "Bindings/AngelscriptWorldCollisionBindingsTestHelpers.h"

#include "Components/ActorTestSpawner.h"
#include "Components/BoxComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Misc/ScopeExit.h"
#include "Templates/Function.h"

#if WITH_DEV_AUTOMATION_TESTS


// ----------------------------------------------------------------------------
// Profile
// ----------------------------------------------------------------------------


// ----------------------------------------------------------------------------
// Shared helpers (retained from original)
// ----------------------------------------------------------------------------

namespace AngelscriptTest_Bindings_AngelscriptWorldCollisionBindingsTests_Private
{
	static constexpr ANSICHAR WorldCollisionModuleName[] = "ASWorldCollisionSyncQueries";
	static const FVector CollisionTargetLocation(0.0f, 0.0f, 0.0f);
	static const FVector CollisionMissLocation(0.0f, 300.0f, 0.0f);
	static const FVector LineTraceStart(-200.0f, 0.0f, 0.0f);
	static const FVector LineTraceEnd(200.0f, 0.0f, 0.0f);
	static const FVector LineTraceMissStart(-200.0f, 300.0f, 0.0f);
	static const FVector LineTraceMissEnd(200.0f, 300.0f, 0.0f);
	static const FVector TargetExtent(50.0f, 50.0f, 50.0f);
	static const FVector QueryExtent(30.0f, 30.0f, 30.0f);
	static const FQuat IdentityRotation = FQuat::Identity;

	UBoxComponent* AddCollisionBox(
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

	bool ExpectHitResultParity(
		FAutomationTestBase& Test,
		const TCHAR* ContextLabel,
		const bool bScriptReturnValue,
		const bool bNativeReturnValue,
		const FHitResult& ScriptHit,
		const FHitResult& NativeHit)
	{
		FNoDiscardAsserter Assert(Test);
		bool bPassed = true;
		bPassed &= Assert.AreEqual(bNativeReturnValue, bScriptReturnValue, *FString::Printf(TEXT("%s should preserve the bool return value"), ContextLabel));
		bPassed &= Assert.AreEqual(NativeHit.GetActor(), ScriptHit.GetActor(), *FString::Printf(TEXT("%s should preserve the hit actor"), ContextLabel));
		bPassed &= Assert.AreEqual(NativeHit.GetComponent(), ScriptHit.GetComponent(), *FString::Printf(TEXT("%s should preserve the hit component"), ContextLabel));
		bPassed &= Assert.AreEqual(NativeHit.bBlockingHit, ScriptHit.bBlockingHit, *FString::Printf(TEXT("%s should preserve the blocking-hit flag"), ContextLabel));
		bPassed &= Assert.IsTrue(FMath::IsNearlyEqual(ScriptHit.Distance, NativeHit.Distance, 0.01f), *FString::Printf(TEXT("%s should preserve the hit distance"), ContextLabel));
		return bPassed;
	}

	bool ExpectHitArrayParity(
		FAutomationTestBase& Test,
		const TCHAR* ContextLabel,
		const bool bScriptReturnValue,
		const bool bNativeReturnValue,
		const TArray<FHitResult>& ScriptHits,
		const TArray<FHitResult>& NativeHits)
	{
		FNoDiscardAsserter Assert(Test);
		bool bPassed = true;
		bPassed &= Assert.AreEqual(bNativeReturnValue, bScriptReturnValue, *FString::Printf(TEXT("%s should preserve the bool return value"), ContextLabel));
		bPassed &= Assert.AreEqual(NativeHits.Num(), ScriptHits.Num(), *FString::Printf(TEXT("%s should preserve the hit count"), ContextLabel));

		for (int32 HitIndex = 0; HitIndex < FMath::Min(ScriptHits.Num(), NativeHits.Num()); ++HitIndex)
		{
			bPassed &= Assert.AreEqual(NativeHits[HitIndex].GetActor(), ScriptHits[HitIndex].GetActor(), *FString::Printf(TEXT("%s should preserve actor for hit %d"), ContextLabel, HitIndex));
			bPassed &= Assert.AreEqual(NativeHits[HitIndex].GetComponent(), ScriptHits[HitIndex].GetComponent(), *FString::Printf(TEXT("%s should preserve component for hit %d"), ContextLabel, HitIndex));
		}

		return bPassed;
	}

	bool ExpectOverlapArrayParity(
		FAutomationTestBase& Test,
		const TCHAR* ContextLabel,
		const bool bScriptReturnValue,
		const bool bNativeReturnValue,
		const TArray<FOverlapResult>& ScriptOverlaps,
		const TArray<FOverlapResult>& NativeOverlaps)
	{
		FNoDiscardAsserter Assert(Test);
		bool bPassed = true;
		bPassed &= Assert.AreEqual(bNativeReturnValue, bScriptReturnValue, *FString::Printf(TEXT("%s should preserve the bool return value"), ContextLabel));
		bPassed &= Assert.AreEqual(NativeOverlaps.Num(), ScriptOverlaps.Num(), *FString::Printf(TEXT("%s should preserve the overlap count"), ContextLabel));

		for (int32 OverlapIndex = 0; OverlapIndex < FMath::Min(ScriptOverlaps.Num(), NativeOverlaps.Num()); ++OverlapIndex)
		{
			bPassed &= Assert.AreEqual(NativeOverlaps[OverlapIndex].GetActor(), ScriptOverlaps[OverlapIndex].GetActor(), *FString::Printf(TEXT("%s should preserve actor for overlap %d"), ContextLabel, OverlapIndex));
			bPassed &= Assert.AreEqual(NativeOverlaps[OverlapIndex].GetComponent(), ScriptOverlaps[OverlapIndex].GetComponent(), *FString::Printf(TEXT("%s should preserve component for overlap %d"), ContextLabel, OverlapIndex));
		}

		return bPassed;
	}
}


// ----------------------------------------------------------------------------
// Test class
// ----------------------------------------------------------------------------

TEST_CLASS_WITH_FLAGS(FAngelscriptWorldCollisionBindingsTest,
	"Angelscript.TestModule.Bindings.WorldCollision",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	// ====================================================================
	// Section: SyncQueries
	// ====================================================================

	TEST_METHOD(SyncQueries)
	{
		using namespace AngelscriptTest_Bindings_AngelscriptWorldCollisionBindingsTests_Private;
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE_FULL();
		{
		FAngelscriptEngineScope _AutoEngineScope(Engine);
		ON_SCOPE_EXIT
		{
			const TArray<TSharedRef<FAngelscriptModuleDesc>> _ActiveModules = Engine.GetActiveModules();
			for (const TSharedRef<FAngelscriptModuleDesc>& _Module : _ActiveModules)
			{
				Engine.DiscardModule(*_Module->ModuleName);
			}
		};

		asIScriptModule* Module = BuildModule(
			*TestRunner,
			Engine,
			WorldCollisionModuleName,
			TEXT(R"(
bool RunLineTraceSingleHit(FHitResult& OutHit)
{
	return System::LineTraceSingleByChannel(OutHit, FVector(-200.0f, 0.0f, 0.0f), FVector(200.0f, 0.0f, 0.0f), ECollisionChannel::ECC_Visibility);
}

bool RunLineTraceSingleMiss(FHitResult& OutHit)
{
	return System::LineTraceSingleByChannel(OutHit, FVector(-200.0f, 300.0f, 0.0f), FVector(200.0f, 300.0f, 0.0f), ECollisionChannel::ECC_Visibility);
}

bool RunLineTraceMultiHit(TArray<FHitResult>& OutHits)
{
	return System::LineTraceMultiByChannel(OutHits, FVector(-200.0f, 0.0f, 0.0f), FVector(200.0f, 0.0f, 0.0f), ECollisionChannel::ECC_Visibility);
}

bool RunLineTraceMultiMiss(TArray<FHitResult>& OutHits)
{
	return System::LineTraceMultiByChannel(OutHits, FVector(-200.0f, 300.0f, 0.0f), FVector(200.0f, 300.0f, 0.0f), ECollisionChannel::ECC_Visibility);
}

bool RunSweepSingleHit(FHitResult& OutHit)
{
	FCollisionShape Shape = FCollisionShape::MakeBox(FVector(30.0f, 30.0f, 30.0f));
	return System::SweepSingleByChannel(OutHit, FVector(-200.0f, 0.0f, 0.0f), FVector(200.0f, 0.0f, 0.0f), FQuat::Identity, ECollisionChannel::ECC_Visibility, Shape);
}

bool RunSweepSingleMiss(FHitResult& OutHit)
{
	FCollisionShape Shape = FCollisionShape::MakeBox(FVector(30.0f, 30.0f, 30.0f));
	return System::SweepSingleByChannel(OutHit, FVector(-200.0f, 300.0f, 0.0f), FVector(200.0f, 300.0f, 0.0f), FQuat::Identity, ECollisionChannel::ECC_Visibility, Shape);
}

bool RunOverlapAnyHit()
{
	FCollisionShape Shape = FCollisionShape::MakeBox(FVector(30.0f, 30.0f, 30.0f));
	return System::OverlapAnyTestByChannel(FVector::ZeroVector, FQuat::Identity, ECollisionChannel::ECC_Visibility, Shape);
}

bool RunOverlapAnyMiss()
{
	FCollisionShape Shape = FCollisionShape::MakeBox(FVector(30.0f, 30.0f, 30.0f));
	return System::OverlapAnyTestByChannel(FVector(0.0f, 300.0f, 0.0f), FQuat::Identity, ECollisionChannel::ECC_Visibility, Shape);
}

bool RunComponentOverlapMultiHit(UPrimitiveComponent QueryComponent, TArray<FOverlapResult>& OutOverlaps)
{
	return System::ComponentOverlapMultiByChannel(OutOverlaps, QueryComponent, FVector::ZeroVector, FQuat::Identity, ECollisionChannel::ECC_Visibility);
}

bool RunComponentOverlapMultiMiss(UPrimitiveComponent QueryComponent, TArray<FOverlapResult>& OutOverlaps)
{
	return System::ComponentOverlapMultiByChannel(OutOverlaps, QueryComponent, FVector(0.0f, 300.0f, 0.0f), FQuat::Identity, ECollisionChannel::ECC_Visibility);
}
)"));
		if (Module == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();

		AActor& CollisionTargetActor = Spawner.SpawnActor<AActor>();
		UBoxComponent* TargetBox = AngelscriptTest_Bindings_AngelscriptWorldCollisionBindingsTests_Private::AddCollisionBox(CollisionTargetActor, FName(TEXT("CollisionTarget")), TargetExtent, CollisionTargetLocation);
		AActor& CollisionQueryActor = Spawner.SpawnActor<AActor>();
		UBoxComponent* QueryBox = AngelscriptTest_Bindings_AngelscriptWorldCollisionBindingsTests_Private::AddCollisionBox(CollisionQueryActor, FName(TEXT("CollisionQuery")), QueryExtent, CollisionMissLocation);
		if (!this->Assert.IsNotNull(TargetBox, TEXT("World collision target box should be created"))
			|| !this->Assert.IsNotNull(QueryBox, TEXT("World collision query box should be created")))
		{
			return;
		}

		UWorld* World = CollisionTargetActor.GetWorld();
		if (!this->Assert.IsNotNull(World, TEXT("World collision test should access the spawned test world")))
		{
			return;
		}

		FScopedTestWorldContextScope WorldContextScope(&CollisionTargetActor);
		const FCollisionShape SweepShape = FCollisionShape::MakeBox(QueryExtent);

		// LineTraceSingle hit
		FHitResult NativeLineHit;
		const bool bNativeLineHit = World->LineTraceSingleByChannel(NativeLineHit, LineTraceStart, LineTraceEnd, ECC_Visibility);
		FHitResult ScriptLineHit;
		bool bScriptLineHit = false;
		if (!WorldCollisionExecuteBoolFunction(*TestRunner, Engine, *Module, TEXT("bool RunLineTraceSingleHit(FHitResult& OutHit)"),
			[this, &ScriptLineHit](asIScriptContext& Context)
			{
				return WorldCollisionSetArgAddressChecked(*TestRunner, Context, 0, &ScriptLineHit, TEXT("RunLineTraceSingleHit"));
			},
			TEXT("RunLineTraceSingleHit"),
			bScriptLineHit))
		{
			return;
		}
		ExpectHitResultParity(*TestRunner, TEXT("LineTraceSingle hit"), bScriptLineHit, bNativeLineHit, ScriptLineHit, NativeLineHit);

		// LineTraceSingle miss
		FHitResult NativeLineMiss;
		const bool bNativeLineMiss = World->LineTraceSingleByChannel(NativeLineMiss, LineTraceMissStart, LineTraceMissEnd, ECC_Visibility);
		FHitResult ScriptLineMiss;
		bool bScriptLineMiss = false;
		if (!WorldCollisionExecuteBoolFunction(*TestRunner, Engine, *Module, TEXT("bool RunLineTraceSingleMiss(FHitResult& OutHit)"),
			[this, &ScriptLineMiss](asIScriptContext& Context)
			{
				return WorldCollisionSetArgAddressChecked(*TestRunner, Context, 0, &ScriptLineMiss, TEXT("RunLineTraceSingleMiss"));
			},
			TEXT("RunLineTraceSingleMiss"),
			bScriptLineMiss))
		{
			return;
		}
		ExpectHitResultParity(*TestRunner, TEXT("LineTraceSingle miss"), bScriptLineMiss, bNativeLineMiss, ScriptLineMiss, NativeLineMiss);

		// LineTraceMulti hit
		TArray<FHitResult> NativeLineHits;
		const bool bNativeLineMultiHit = World->LineTraceMultiByChannel(NativeLineHits, LineTraceStart, LineTraceEnd, ECC_Visibility);
		TArray<FHitResult> ScriptLineHits;
		bool bScriptLineMultiHit = false;
		if (!WorldCollisionExecuteBoolFunction(*TestRunner, Engine, *Module, TEXT("bool RunLineTraceMultiHit(TArray<FHitResult>& OutHits)"),
			[this, &ScriptLineHits](asIScriptContext& Context)
			{
				return WorldCollisionSetArgAddressChecked(*TestRunner, Context, 0, &ScriptLineHits, TEXT("RunLineTraceMultiHit"));
			},
			TEXT("RunLineTraceMultiHit"),
			bScriptLineMultiHit))
		{
			return;
		}
		ExpectHitArrayParity(*TestRunner, TEXT("LineTraceMulti hit"), bScriptLineMultiHit, bNativeLineMultiHit, ScriptLineHits, NativeLineHits);

		// LineTraceMulti miss
		TArray<FHitResult> NativeLineMissHits;
		const bool bNativeLineMultiMiss = World->LineTraceMultiByChannel(NativeLineMissHits, LineTraceMissStart, LineTraceMissEnd, ECC_Visibility);
		TArray<FHitResult> ScriptLineMissHits;
		bool bScriptLineMultiMiss = false;
		if (!WorldCollisionExecuteBoolFunction(*TestRunner, Engine, *Module, TEXT("bool RunLineTraceMultiMiss(TArray<FHitResult>& OutHits)"),
			[this, &ScriptLineMissHits](asIScriptContext& Context)
			{
				return WorldCollisionSetArgAddressChecked(*TestRunner, Context, 0, &ScriptLineMissHits, TEXT("RunLineTraceMultiMiss"));
			},
			TEXT("RunLineTraceMultiMiss"),
			bScriptLineMultiMiss))
		{
			return;
		}
		ExpectHitArrayParity(*TestRunner, TEXT("LineTraceMulti miss"), bScriptLineMultiMiss, bNativeLineMultiMiss, ScriptLineMissHits, NativeLineMissHits);

		// SweepSingle hit
		FHitResult NativeSweepHit;
		const bool bNativeSweepHit = World->SweepSingleByChannel(NativeSweepHit, LineTraceStart, LineTraceEnd, IdentityRotation, ECC_Visibility, SweepShape);
		FHitResult ScriptSweepHit;
		bool bScriptSweepHit = false;
		if (!WorldCollisionExecuteBoolFunction(*TestRunner, Engine, *Module, TEXT("bool RunSweepSingleHit(FHitResult& OutHit)"),
			[this, &ScriptSweepHit](asIScriptContext& Context)
			{
				return WorldCollisionSetArgAddressChecked(*TestRunner, Context, 0, &ScriptSweepHit, TEXT("RunSweepSingleHit"));
			},
			TEXT("RunSweepSingleHit"),
			bScriptSweepHit))
		{
			return;
		}
		ExpectHitResultParity(*TestRunner, TEXT("SweepSingle hit"), bScriptSweepHit, bNativeSweepHit, ScriptSweepHit, NativeSweepHit);

		// SweepSingle miss
		FHitResult NativeSweepMiss;
		const bool bNativeSweepMiss = World->SweepSingleByChannel(NativeSweepMiss, LineTraceMissStart, LineTraceMissEnd, IdentityRotation, ECC_Visibility, SweepShape);
		FHitResult ScriptSweepMiss;
		bool bScriptSweepMiss = false;
		if (!WorldCollisionExecuteBoolFunction(*TestRunner, Engine, *Module, TEXT("bool RunSweepSingleMiss(FHitResult& OutHit)"),
			[this, &ScriptSweepMiss](asIScriptContext& Context)
			{
				return WorldCollisionSetArgAddressChecked(*TestRunner, Context, 0, &ScriptSweepMiss, TEXT("RunSweepSingleMiss"));
			},
			TEXT("RunSweepSingleMiss"),
			bScriptSweepMiss))
		{
			return;
		}
		ExpectHitResultParity(*TestRunner, TEXT("SweepSingle miss"), bScriptSweepMiss, bNativeSweepMiss, ScriptSweepMiss, NativeSweepMiss);

		// OverlapAny hit
		const bool bNativeOverlapAnyHit = World->OverlapAnyTestByChannel(CollisionTargetLocation, IdentityRotation, ECC_Visibility, SweepShape);
		bool bScriptOverlapAnyHit = false;
		if (!WorldCollisionExecuteBoolFunction(*TestRunner, Engine, *Module, TEXT("bool RunOverlapAnyHit()"),
			[](asIScriptContext&) { return true; },
			TEXT("RunOverlapAnyHit"),
			bScriptOverlapAnyHit))
		{
			return;
		}
		if (!this->Assert.AreEqual(bNativeOverlapAnyHit, bScriptOverlapAnyHit, TEXT("OverlapAny hit should preserve the bool return value")))
		{
			return;
		}

		// OverlapAny miss
		const bool bNativeOverlapAnyMiss = World->OverlapAnyTestByChannel(CollisionMissLocation, IdentityRotation, ECC_Visibility, SweepShape);
		bool bScriptOverlapAnyMiss = false;
		if (!WorldCollisionExecuteBoolFunction(*TestRunner, Engine, *Module, TEXT("bool RunOverlapAnyMiss()"),
			[](asIScriptContext&) { return true; },
			TEXT("RunOverlapAnyMiss"),
			bScriptOverlapAnyMiss))
		{
			return;
		}
		if (!this->Assert.AreEqual(bNativeOverlapAnyMiss, bScriptOverlapAnyMiss, TEXT("OverlapAny miss should preserve the bool return value")))
		{
			return;
		}

		// ComponentOverlapMulti hit
		TArray<FOverlapResult> NativeComponentOverlapHits;
		const bool bNativeComponentOverlapHit = World->ComponentOverlapMultiByChannel(NativeComponentOverlapHits, QueryBox, CollisionTargetLocation, IdentityRotation, ECC_Visibility);
		TArray<FOverlapResult> ScriptComponentOverlapHits;
		bool bScriptComponentOverlapHit = false;
		if (!WorldCollisionExecuteBoolFunction(*TestRunner, Engine, *Module, TEXT("bool RunComponentOverlapMultiHit(UPrimitiveComponent QueryComponent, TArray<FOverlapResult>& OutOverlaps)"),
			[this, QueryBox, &ScriptComponentOverlapHits](asIScriptContext& Context)
			{
				return WorldCollisionSetArgObjectChecked(*TestRunner, Context, 0, QueryBox, TEXT("RunComponentOverlapMultiHit"))
					&& WorldCollisionSetArgAddressChecked(*TestRunner, Context, 1, &ScriptComponentOverlapHits, TEXT("RunComponentOverlapMultiHit"));
			},
			TEXT("RunComponentOverlapMultiHit"),
			bScriptComponentOverlapHit))
		{
			return;
		}
		ExpectOverlapArrayParity(
			*TestRunner,
			TEXT("ComponentOverlapMultiByChannel hit"),
			bScriptComponentOverlapHit,
			bNativeComponentOverlapHit,
			ScriptComponentOverlapHits,
			NativeComponentOverlapHits);

		// ComponentOverlapMulti miss
		TArray<FOverlapResult> NativeComponentOverlapMisses;
		const bool bNativeComponentOverlapMiss = World->ComponentOverlapMultiByChannel(NativeComponentOverlapMisses, QueryBox, CollisionMissLocation, IdentityRotation, ECC_Visibility);
		TArray<FOverlapResult> ScriptComponentOverlapMisses;
		bool bScriptComponentOverlapMiss = false;
		if (!WorldCollisionExecuteBoolFunction(*TestRunner, Engine, *Module, TEXT("bool RunComponentOverlapMultiMiss(UPrimitiveComponent QueryComponent, TArray<FOverlapResult>& OutOverlaps)"),
			[this, QueryBox, &ScriptComponentOverlapMisses](asIScriptContext& Context)
			{
				return WorldCollisionSetArgObjectChecked(*TestRunner, Context, 0, QueryBox, TEXT("RunComponentOverlapMultiMiss"))
					&& WorldCollisionSetArgAddressChecked(*TestRunner, Context, 1, &ScriptComponentOverlapMisses, TEXT("RunComponentOverlapMultiMiss"));
			},
			TEXT("RunComponentOverlapMultiMiss"),
			bScriptComponentOverlapMiss))
		{
			return;
		}
		ExpectOverlapArrayParity(
			*TestRunner,
			TEXT("ComponentOverlapMultiByChannel miss"),
			bScriptComponentOverlapMiss,
			bNativeComponentOverlapMiss,
			ScriptComponentOverlapMisses,
			NativeComponentOverlapMisses);

		}
	}
};

#endif
