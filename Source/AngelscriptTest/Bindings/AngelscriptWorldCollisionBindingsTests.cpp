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
//   Single IMPLEMENT_SIMPLE_AUTOMATION_TEST converted to TEST_CLASS.
//   Uses ASTEST_CREATE_ENGINE_FULL (world-based) with FActorTestSpawner.
//   Custom address-based invocation helpers retained for the
//   bool+out-param calling convention.
// ============================================================================

#include "CQTest.h"
#include "Shared/AngelscriptTestMacros.h"
#include "Shared/AngelscriptTestUtilities.h"
#include "Shared/AngelscriptBindingsModuleBuilder.h"
#include "Shared/AngelscriptBindingsAssertions.h"

#include "Components/ActorTestSpawner.h"
#include "Components/BoxComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Misc/ScopeExit.h"
#include "Templates/Function.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptTestSupport;
using namespace AngelscriptTestBindings;
using namespace AngelscriptReflectiveAccess;

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

	bool SetArgObjectChecked(
		FAutomationTestBase& Test,
		asIScriptContext& Context,
		asUINT ArgumentIndex,
		void* Object,
		const TCHAR* ContextLabel)
	{
		return Test.TestEqual(
			*FString::Printf(TEXT("%s should bind object argument %u"), ContextLabel, static_cast<uint32>(ArgumentIndex)),
			Context.SetArgObject(ArgumentIndex, Object),
			static_cast<int32>(asSUCCESS));
	}

	// TODO(refactor-as-test-shared-layout-and-naming): migrate ExecuteBoolFunction to Shared/AngelscriptTestExecute.h once the Execute* naming family lands in Phase 3.
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
		if (!Test.TestEqual(
			*FString::Printf(TEXT("%s should prepare successfully"), ContextLabel),
			PrepareResult,
			static_cast<int32>(asSUCCESS)))
		{
			return false;
		}

		if (!BindArguments(*Context))
		{
			return false;
		}

		const int ExecuteResult = Context->Execute();
		if (!Test.TestEqual(
			*FString::Printf(TEXT("%s should execute successfully"), ContextLabel),
			ExecuteResult,
			static_cast<int32>(asEXECUTION_FINISHED)))
		{
			return false;
		}

		OutResult = Context->GetReturnByte() != 0;
		return true;
	}

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
		bool bPassed = true;
		bPassed &= Test.TestEqual(
			*FString::Printf(TEXT("%s should preserve the bool return value"), ContextLabel),
			bScriptReturnValue,
			bNativeReturnValue);
		bPassed &= Test.TestEqual(
			*FString::Printf(TEXT("%s should preserve the hit actor"), ContextLabel),
			ScriptHit.GetActor(),
			NativeHit.GetActor());
		bPassed &= Test.TestEqual(
			*FString::Printf(TEXT("%s should preserve the hit component"), ContextLabel),
			ScriptHit.GetComponent(),
			NativeHit.GetComponent());
		bPassed &= Test.TestEqual(
			*FString::Printf(TEXT("%s should preserve the blocking-hit flag"), ContextLabel),
			ScriptHit.bBlockingHit,
			NativeHit.bBlockingHit);
		bPassed &= Test.TestTrue(
			*FString::Printf(TEXT("%s should preserve the hit distance"), ContextLabel),
			FMath::IsNearlyEqual(ScriptHit.Distance, NativeHit.Distance, 0.01f));
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
		bool bPassed = true;
		bPassed &= Test.TestEqual(
			*FString::Printf(TEXT("%s should preserve the bool return value"), ContextLabel),
			bScriptReturnValue,
			bNativeReturnValue);
		bPassed &= Test.TestEqual(
			*FString::Printf(TEXT("%s should preserve the hit count"), ContextLabel),
			ScriptHits.Num(),
			NativeHits.Num());

		for (int32 HitIndex = 0; HitIndex < FMath::Min(ScriptHits.Num(), NativeHits.Num()); ++HitIndex)
		{
			bPassed &= Test.TestEqual(
				*FString::Printf(TEXT("%s should preserve actor for hit %d"), ContextLabel, HitIndex),
				ScriptHits[HitIndex].GetActor(),
				NativeHits[HitIndex].GetActor());
			bPassed &= Test.TestEqual(
				*FString::Printf(TEXT("%s should preserve component for hit %d"), ContextLabel, HitIndex),
				ScriptHits[HitIndex].GetComponent(),
				NativeHits[HitIndex].GetComponent());
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
		bool bPassed = true;
		bPassed &= Test.TestEqual(
			*FString::Printf(TEXT("%s should preserve the bool return value"), ContextLabel),
			bScriptReturnValue,
			bNativeReturnValue);
		bPassed &= Test.TestEqual(
			*FString::Printf(TEXT("%s should preserve the overlap count"), ContextLabel),
			ScriptOverlaps.Num(),
			NativeOverlaps.Num());

		for (int32 OverlapIndex = 0; OverlapIndex < FMath::Min(ScriptOverlaps.Num(), NativeOverlaps.Num()); ++OverlapIndex)
		{
			bPassed &= Test.TestEqual(
				*FString::Printf(TEXT("%s should preserve actor for overlap %d"), ContextLabel, OverlapIndex),
				ScriptOverlaps[OverlapIndex].GetActor(),
				NativeOverlaps[OverlapIndex].GetActor());
			bPassed &= Test.TestEqual(
				*FString::Printf(TEXT("%s should preserve component for overlap %d"), ContextLabel, OverlapIndex),
				ScriptOverlaps[OverlapIndex].GetComponent(),
				NativeOverlaps[OverlapIndex].GetComponent());
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
		if (!TestRunner->TestNotNull(TEXT("World collision target box should be created"), TargetBox)
			|| !TestRunner->TestNotNull(TEXT("World collision query box should be created"), QueryBox))
		{
			return;
		}

		UWorld* World = CollisionTargetActor.GetWorld();
		if (!TestRunner->TestNotNull(TEXT("World collision test should access the spawned test world"), World))
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
		if (!ExecuteBoolFunction(*TestRunner, Engine, *Module, TEXT("bool RunLineTraceSingleHit(FHitResult& OutHit)"),
			[this, &ScriptLineHit](asIScriptContext& Context)
			{
				return SetArgAddressChecked(*TestRunner, Context, 0, &ScriptLineHit, TEXT("RunLineTraceSingleHit"));
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
		if (!ExecuteBoolFunction(*TestRunner, Engine, *Module, TEXT("bool RunLineTraceSingleMiss(FHitResult& OutHit)"),
			[this, &ScriptLineMiss](asIScriptContext& Context)
			{
				return SetArgAddressChecked(*TestRunner, Context, 0, &ScriptLineMiss, TEXT("RunLineTraceSingleMiss"));
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
		if (!ExecuteBoolFunction(*TestRunner, Engine, *Module, TEXT("bool RunLineTraceMultiHit(TArray<FHitResult>& OutHits)"),
			[this, &ScriptLineHits](asIScriptContext& Context)
			{
				return SetArgAddressChecked(*TestRunner, Context, 0, &ScriptLineHits, TEXT("RunLineTraceMultiHit"));
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
		if (!ExecuteBoolFunction(*TestRunner, Engine, *Module, TEXT("bool RunLineTraceMultiMiss(TArray<FHitResult>& OutHits)"),
			[this, &ScriptLineMissHits](asIScriptContext& Context)
			{
				return SetArgAddressChecked(*TestRunner, Context, 0, &ScriptLineMissHits, TEXT("RunLineTraceMultiMiss"));
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
		if (!ExecuteBoolFunction(*TestRunner, Engine, *Module, TEXT("bool RunSweepSingleHit(FHitResult& OutHit)"),
			[this, &ScriptSweepHit](asIScriptContext& Context)
			{
				return SetArgAddressChecked(*TestRunner, Context, 0, &ScriptSweepHit, TEXT("RunSweepSingleHit"));
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
		if (!ExecuteBoolFunction(*TestRunner, Engine, *Module, TEXT("bool RunSweepSingleMiss(FHitResult& OutHit)"),
			[this, &ScriptSweepMiss](asIScriptContext& Context)
			{
				return SetArgAddressChecked(*TestRunner, Context, 0, &ScriptSweepMiss, TEXT("RunSweepSingleMiss"));
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
		if (!ExecuteBoolFunction(*TestRunner, Engine, *Module, TEXT("bool RunOverlapAnyHit()"),
			[](asIScriptContext&) { return true; },
			TEXT("RunOverlapAnyHit"),
			bScriptOverlapAnyHit))
		{
			return;
		}
		TestRunner->TestEqual(TEXT("OverlapAny hit should preserve the bool return value"), bScriptOverlapAnyHit, bNativeOverlapAnyHit);

		// OverlapAny miss
		const bool bNativeOverlapAnyMiss = World->OverlapAnyTestByChannel(CollisionMissLocation, IdentityRotation, ECC_Visibility, SweepShape);
		bool bScriptOverlapAnyMiss = false;
		if (!ExecuteBoolFunction(*TestRunner, Engine, *Module, TEXT("bool RunOverlapAnyMiss()"),
			[](asIScriptContext&) { return true; },
			TEXT("RunOverlapAnyMiss"),
			bScriptOverlapAnyMiss))
		{
			return;
		}
		TestRunner->TestEqual(TEXT("OverlapAny miss should preserve the bool return value"), bScriptOverlapAnyMiss, bNativeOverlapAnyMiss);

		// ComponentOverlapMulti hit
		TArray<FOverlapResult> NativeComponentOverlapHits;
		const bool bNativeComponentOverlapHit = World->ComponentOverlapMultiByChannel(NativeComponentOverlapHits, QueryBox, CollisionTargetLocation, IdentityRotation, ECC_Visibility);
		TArray<FOverlapResult> ScriptComponentOverlapHits;
		bool bScriptComponentOverlapHit = false;
		if (!ExecuteBoolFunction(*TestRunner, Engine, *Module, TEXT("bool RunComponentOverlapMultiHit(UPrimitiveComponent QueryComponent, TArray<FOverlapResult>& OutOverlaps)"),
			[this, QueryBox, &ScriptComponentOverlapHits](asIScriptContext& Context)
			{
				return SetArgObjectChecked(*TestRunner, Context, 0, QueryBox, TEXT("RunComponentOverlapMultiHit"))
					&& SetArgAddressChecked(*TestRunner, Context, 1, &ScriptComponentOverlapHits, TEXT("RunComponentOverlapMultiHit"));
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
		if (!ExecuteBoolFunction(*TestRunner, Engine, *Module, TEXT("bool RunComponentOverlapMultiMiss(UPrimitiveComponent QueryComponent, TArray<FOverlapResult>& OutOverlaps)"),
			[this, QueryBox, &ScriptComponentOverlapMisses](asIScriptContext& Context)
			{
				return SetArgObjectChecked(*TestRunner, Context, 0, QueryBox, TEXT("RunComponentOverlapMultiMiss"))
					&& SetArgAddressChecked(*TestRunner, Context, 1, &ScriptComponentOverlapMisses, TEXT("RunComponentOverlapMultiMiss"));
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
