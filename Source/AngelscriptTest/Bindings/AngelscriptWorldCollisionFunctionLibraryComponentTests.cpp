// ============================================================================
// AngelscriptWorldCollisionFunctionLibraryComponentTests.cpp
//
// World collision function library component query coverage -- CQTest refactor.
// Automation IDs:
//   Angelscript.TestModule.FunctionLibraries.WorldCollisionComponent.FAngelscriptWorldCollisionFunctionLibraryComponentTest.*
//
// Sections:
//   ComponentQueries     — ComponentSweepMulti/ComponentOverlapMulti hit/miss parity
//   NullComponentQueries — null component guard returns false and clears output
//
// CQTest adaptation notes:
//   Two IMPLEMENT_SIMPLE_AUTOMATION_TEST merged into one TEST_CLASS.
//   Uses ASTEST_CREATE_ENGINE_FULL (world-based) with FActorTestSpawner.
//   Custom address-based invocation helpers retained.
// ============================================================================

#include "CQTest.h"
#include "Shared/AngelscriptTestMacros.h"
#include "Shared/AngelscriptTestUtilities.h"
#include "Shared/AngelscriptBindingsModuleBuilder.h"
#include "Shared/AngelscriptBindingsAssertions.h"

#include "Components/ActorTestSpawner.h"
#include "Components/BoxComponent.h"
#include "Engine/CollisionProfile.h"
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

namespace AngelscriptTest_Bindings_AngelscriptWorldCollisionFunctionLibraryComponentTests_Private
{
	static constexpr ANSICHAR ModuleName[] = "ASWorldCollisionFunctionLibraryComponentQueries";
	static constexpr ANSICHAR NullComponentModuleName[] = "ASWorldCollisionFunctionLibraryNullComponentQueries";
	static const FVector BlockingTargetLocation(0.0f, 0.0f, 0.0f);
	static const FVector OverlapTargetLocation(0.0f, 150.0f, 0.0f);
	static const FVector QueryComponentSpawnLocation(0.0f, 300.0f, 0.0f);
	static const FVector SweepHitStart(-200.0f, 0.0f, 0.0f);
	static const FVector SweepHitEnd(200.0f, 0.0f, 0.0f);
	static const FVector SweepMissStart(-200.0f, -200.0f, 0.0f);
	static const FVector SweepMissEnd(200.0f, -200.0f, 0.0f);
	static const FVector TargetExtent(50.0f, 50.0f, 50.0f);
	static const FVector OverlapExtent(40.0f, 40.0f, 40.0f);
	static const FVector QueryExtent(30.0f, 30.0f, 30.0f);
	static const FVector MissOverlapLocation(0.0f, -150.0f, 0.0f);
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
		const TCHAR* FunctionDecl,
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

	template <typename TResult>
	bool ExpectArrayParity(FAutomationTestBase& Test, const TCHAR* Label, bool bScriptReturnValue, bool bNativeReturnValue, const TArray<TResult>& ScriptResults, const TArray<TResult>& NativeResults)
	{
		bool bPassed = true;
		bPassed &= Test.TestEqual(*FString::Printf(TEXT("%s should preserve the bool return value"), Label), bScriptReturnValue, bNativeReturnValue);
		bPassed &= Test.TestEqual(*FString::Printf(TEXT("%s should preserve the result count"), Label), ScriptResults.Num(), NativeResults.Num());

		for (int32 ResultIndex = 0; ResultIndex < FMath::Min(ScriptResults.Num(), NativeResults.Num()); ++ResultIndex)
		{
			bPassed &= Test.TestEqual(*FString::Printf(TEXT("%s should preserve actor for result %d"), Label, ResultIndex), ScriptResults[ResultIndex].GetActor(), NativeResults[ResultIndex].GetActor());
			bPassed &= Test.TestEqual(*FString::Printf(TEXT("%s should preserve component for result %d"), Label, ResultIndex), ScriptResults[ResultIndex].GetComponent(), NativeResults[ResultIndex].GetComponent());
		}

		return bPassed;
	}

	bool HitResultsContainComponent(const TArray<FHitResult>& Hits, const UPrimitiveComponent* Component)
	{
		return Hits.ContainsByPredicate([Component](const FHitResult& Hit)
		{
			return Hit.GetComponent() == Component;
		});
	}

	bool OverlapsContainComponent(const TArray<FOverlapResult>& Overlaps, const UPrimitiveComponent* Component)
	{
		return Overlaps.ContainsByPredicate([Component](const FOverlapResult& Overlap)
		{
			return Overlap.GetComponent() == Component;
		});
	}
}


// ----------------------------------------------------------------------------
// Test class
// ----------------------------------------------------------------------------

TEST_CLASS_WITH_FLAGS(FAngelscriptWorldCollisionFunctionLibraryComponentTest,
	"Angelscript.TestModule.FunctionLibraries.WorldCollisionComponent",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	// ====================================================================
	// Section: ComponentQueries
	// ====================================================================

	TEST_METHOD(ComponentQueries)
	{
		using namespace AngelscriptTest_Bindings_AngelscriptWorldCollisionFunctionLibraryComponentTests_Private;
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
			ModuleName,
			TEXT(R"(
bool RunComponentSweepMultiHit(UPrimitiveComponent QueryComponent, TArray<FHitResult>& OutHits)
{
	FComponentQueryParams Params = FComponentQueryParams::DefaultComponentQueryParams;
	Params.AddIgnoredComponent(QueryComponent);
	return System::ComponentSweepMulti(OutHits, QueryComponent, FVector(-200.0f, 0.0f, 0.0f), FVector(200.0f, 0.0f, 0.0f), FQuat::Identity, Params);
}

bool RunComponentSweepMultiMiss(UPrimitiveComponent QueryComponent, TArray<FHitResult>& OutHits)
{
	FComponentQueryParams Params = FComponentQueryParams::DefaultComponentQueryParams;
	Params.AddIgnoredComponent(QueryComponent);
	return System::ComponentSweepMulti(OutHits, QueryComponent, FVector(-200.0f, -200.0f, 0.0f), FVector(200.0f, -200.0f, 0.0f), FQuat::Identity, Params);
}

bool RunComponentOverlapMultiHit(UPrimitiveComponent QueryComponent, TArray<FOverlapResult>& OutOverlaps)
{
	FComponentQueryParams Params = FComponentQueryParams::DefaultComponentQueryParams;
	Params.AddIgnoredComponent(QueryComponent);

	FCollisionObjectQueryParams ObjectQueryParams;
	ObjectQueryParams.AddObjectTypesToQuery(ECollisionChannel::ECC_WorldDynamic);
	return System::ComponentOverlapMulti(OutOverlaps, QueryComponent, FVector(0.0f, 150.0f, 0.0f), FQuat::Identity, Params, ObjectQueryParams);
}

bool RunComponentOverlapMultiMiss(UPrimitiveComponent QueryComponent, TArray<FOverlapResult>& OutOverlaps)
{
	FComponentQueryParams Params = FComponentQueryParams::DefaultComponentQueryParams;
	Params.AddIgnoredComponent(QueryComponent);

	FCollisionObjectQueryParams ObjectQueryParams;
	ObjectQueryParams.AddObjectTypesToQuery(ECollisionChannel::ECC_WorldDynamic);
	return System::ComponentOverlapMulti(OutOverlaps, QueryComponent, FVector(0.0f, -150.0f, 0.0f), FQuat::Identity, Params, ObjectQueryParams);
}
)"));
		if (Module == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();

		AActor& CollisionBlockingActor = Spawner.SpawnActor<AActor>();
		UBoxComponent* BlockingBox = AngelscriptTest_Bindings_AngelscriptWorldCollisionFunctionLibraryComponentTests_Private::AddCollisionBox(CollisionBlockingActor, FName(TEXT("BlockingTarget")), TargetExtent, BlockingTargetLocation);
		AActor& CollisionOverlapActor = Spawner.SpawnActor<AActor>();
		UBoxComponent* OverlapBox = AngelscriptTest_Bindings_AngelscriptWorldCollisionFunctionLibraryComponentTests_Private::AddCollisionBox(CollisionOverlapActor, FName(TEXT("OverlapTarget")), OverlapExtent, OverlapTargetLocation);
		AActor& CollisionQueryActor = Spawner.SpawnActor<AActor>();
		UBoxComponent* QueryBox = AngelscriptTest_Bindings_AngelscriptWorldCollisionFunctionLibraryComponentTests_Private::AddCollisionBox(CollisionQueryActor, FName(TEXT("QueryComponent")), QueryExtent, QueryComponentSpawnLocation);
		if (!TestRunner->TestNotNull(TEXT("World collision function library blocker should be created"), BlockingBox)
			|| !TestRunner->TestNotNull(TEXT("World collision function library overlap target should be created"), OverlapBox)
			|| !TestRunner->TestNotNull(TEXT("World collision function library query component should be created"), QueryBox))
		{
			return;
		}

		UWorld* World = CollisionBlockingActor.GetWorld();
		if (!TestRunner->TestNotNull(TEXT("World collision function library component test should access the spawned world"), World))
		{
			return;
		}

		FScopedTestWorldContextScope WorldContextScope(&CollisionBlockingActor);

		FCollisionObjectQueryParams ObjectQueryParams;
		ObjectQueryParams.AddObjectTypesToQuery(ECC_WorldDynamic);
		FComponentQueryParams ComponentQueryParams = FComponentQueryParams::DefaultComponentQueryParams;
		ComponentQueryParams.AddIgnoredComponent(QueryBox);

		// ComponentSweepMulti hit
		TArray<FHitResult> NativeComponentSweepHits;
		const bool bNativeComponentSweepHit = World->ComponentSweepMulti(NativeComponentSweepHits, QueryBox, SweepHitStart, SweepHitEnd, IdentityRotation, ComponentQueryParams);
		TArray<FHitResult> ScriptComponentSweepHits;
		bool bScriptComponentSweepHit = false;
		if (!ExecuteBoolFunction(
			*TestRunner,
			Engine,
			*Module,
			TEXT("bool RunComponentSweepMultiHit(UPrimitiveComponent QueryComponent, TArray<FHitResult>& OutHits)"),
			[this, QueryBox, &ScriptComponentSweepHits](asIScriptContext& Context)
			{
				return SetArgObjectChecked(*TestRunner, Context, 0, QueryBox, TEXT("RunComponentSweepMultiHit"))
					&& SetArgAddressChecked(*TestRunner, Context, 1, &ScriptComponentSweepHits, TEXT("RunComponentSweepMultiHit"));
			},
			TEXT("RunComponentSweepMultiHit"),
			bScriptComponentSweepHit))
		{
			return;
		}
		ExpectArrayParity(*TestRunner, TEXT("ComponentSweepMulti hit"), bScriptComponentSweepHit, bNativeComponentSweepHit, ScriptComponentSweepHits, NativeComponentSweepHits);
		TestRunner->TestTrue(TEXT("ComponentSweepMulti hit should produce at least one hit"), ScriptComponentSweepHits.Num() >= 1);
		TestRunner->TestTrue(TEXT("ComponentSweepMulti hit should include the blocker component"), HitResultsContainComponent(ScriptComponentSweepHits, BlockingBox));

		// ComponentSweepMulti miss
		TArray<FHitResult> NativeComponentSweepMisses;
		NativeComponentSweepMisses.AddDefaulted();
		const bool bNativeComponentSweepMiss = World->ComponentSweepMulti(NativeComponentSweepMisses, QueryBox, SweepMissStart, SweepMissEnd, IdentityRotation, ComponentQueryParams);
		TArray<FHitResult> ScriptComponentSweepMisses;
		ScriptComponentSweepMisses.AddDefaulted();
		bool bScriptComponentSweepMiss = false;
		if (!ExecuteBoolFunction(
			*TestRunner,
			Engine,
			*Module,
			TEXT("bool RunComponentSweepMultiMiss(UPrimitiveComponent QueryComponent, TArray<FHitResult>& OutHits)"),
			[this, QueryBox, &ScriptComponentSweepMisses](asIScriptContext& Context)
			{
				return SetArgObjectChecked(*TestRunner, Context, 0, QueryBox, TEXT("RunComponentSweepMultiMiss"))
					&& SetArgAddressChecked(*TestRunner, Context, 1, &ScriptComponentSweepMisses, TEXT("RunComponentSweepMultiMiss"));
			},
			TEXT("RunComponentSweepMultiMiss"),
			bScriptComponentSweepMiss))
		{
			return;
		}
		ExpectArrayParity(*TestRunner, TEXT("ComponentSweepMulti miss"), bScriptComponentSweepMiss, bNativeComponentSweepMiss, ScriptComponentSweepMisses, NativeComponentSweepMisses);
		TestRunner->TestEqual(TEXT("ComponentSweepMulti miss should clear stale hit results"), ScriptComponentSweepMisses.Num(), 0);

		// ComponentOverlapMulti hit
		TArray<FOverlapResult> NativeComponentOverlapHits;
		const bool bNativeComponentOverlapHit = World->ComponentOverlapMulti(NativeComponentOverlapHits, QueryBox, OverlapTargetLocation, IdentityRotation, ComponentQueryParams, ObjectQueryParams);
		TArray<FOverlapResult> ScriptComponentOverlapHits;
		bool bScriptComponentOverlapHit = false;
		if (!ExecuteBoolFunction(
			*TestRunner,
			Engine,
			*Module,
			TEXT("bool RunComponentOverlapMultiHit(UPrimitiveComponent QueryComponent, TArray<FOverlapResult>& OutOverlaps)"),
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
		ExpectArrayParity(*TestRunner, TEXT("ComponentOverlapMulti hit"), bScriptComponentOverlapHit, bNativeComponentOverlapHit, ScriptComponentOverlapHits, NativeComponentOverlapHits);
		TestRunner->TestTrue(TEXT("ComponentOverlapMulti hit should include the overlap target component"), OverlapsContainComponent(ScriptComponentOverlapHits, OverlapBox));

		// ComponentOverlapMulti miss
		TArray<FOverlapResult> NativeComponentOverlapMisses;
		NativeComponentOverlapMisses.AddDefaulted();
		const bool bNativeComponentOverlapMiss = World->ComponentOverlapMulti(NativeComponentOverlapMisses, QueryBox, MissOverlapLocation, IdentityRotation, ComponentQueryParams, ObjectQueryParams);
		TArray<FOverlapResult> ScriptComponentOverlapMisses;
		ScriptComponentOverlapMisses.AddDefaulted();
		bool bScriptComponentOverlapMiss = false;
		if (!ExecuteBoolFunction(
			*TestRunner,
			Engine,
			*Module,
			TEXT("bool RunComponentOverlapMultiMiss(UPrimitiveComponent QueryComponent, TArray<FOverlapResult>& OutOverlaps)"),
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
		ExpectArrayParity(*TestRunner, TEXT("ComponentOverlapMulti miss"), bScriptComponentOverlapMiss, bNativeComponentOverlapMiss, ScriptComponentOverlapMisses, NativeComponentOverlapMisses);
		TestRunner->TestEqual(TEXT("ComponentOverlapMulti miss should clear stale overlap results"), ScriptComponentOverlapMisses.Num(), 0);

		}
	}

	// ====================================================================
	// Section: NullComponentQueries
	// ====================================================================

	TEST_METHOD(NullComponentQueries)
	{
		using namespace AngelscriptTest_Bindings_AngelscriptWorldCollisionFunctionLibraryComponentTests_Private;
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
			NullComponentModuleName,
			TEXT(R"(
bool RunComponentSweepMultiBaseline(UPrimitiveComponent QueryComponent, TArray<FHitResult>& OutHits)
{
	FComponentQueryParams Params = FComponentQueryParams::DefaultComponentQueryParams;
	Params.AddIgnoredComponent(QueryComponent);
	return System::ComponentSweepMulti(OutHits, QueryComponent, FVector(-200.0f, 0.0f, 0.0f), FVector(200.0f, 0.0f, 0.0f), FQuat::Identity, Params);
}

bool RunComponentSweepMultiNull(UPrimitiveComponent QueryComponent, TArray<FHitResult>& OutHits)
{
	FComponentQueryParams Params = FComponentQueryParams::DefaultComponentQueryParams;
	return System::ComponentSweepMulti(OutHits, QueryComponent, FVector(-200.0f, 0.0f, 0.0f), FVector(200.0f, 0.0f, 0.0f), FQuat::Identity, Params);
}

bool RunComponentOverlapMultiBaseline(UPrimitiveComponent QueryComponent, TArray<FOverlapResult>& OutOverlaps)
{
	FComponentQueryParams Params = FComponentQueryParams::DefaultComponentQueryParams;
	Params.AddIgnoredComponent(QueryComponent);

	FCollisionObjectQueryParams ObjectQueryParams;
	ObjectQueryParams.AddObjectTypesToQuery(ECollisionChannel::ECC_WorldDynamic);
	return System::ComponentOverlapMulti(OutOverlaps, QueryComponent, FVector(0.0f, 150.0f, 0.0f), FQuat::Identity, Params, ObjectQueryParams);
}

bool RunComponentOverlapMultiNull(UPrimitiveComponent QueryComponent, TArray<FOverlapResult>& OutOverlaps)
{
	FComponentQueryParams Params = FComponentQueryParams::DefaultComponentQueryParams;

	FCollisionObjectQueryParams ObjectQueryParams;
	ObjectQueryParams.AddObjectTypesToQuery(ECollisionChannel::ECC_WorldDynamic);
	return System::ComponentOverlapMulti(OutOverlaps, QueryComponent, FVector(0.0f, 150.0f, 0.0f), FQuat::Identity, Params, ObjectQueryParams);
}
)"));
		if (Module == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();

		AActor& CollisionBlockingActor = Spawner.SpawnActor<AActor>();
		UBoxComponent* BlockingBox = AngelscriptTest_Bindings_AngelscriptWorldCollisionFunctionLibraryComponentTests_Private::AddCollisionBox(CollisionBlockingActor, FName(TEXT("NullGuardBlockingTarget")), TargetExtent, BlockingTargetLocation);
		AActor& CollisionOverlapActor = Spawner.SpawnActor<AActor>();
		UBoxComponent* OverlapBox = AngelscriptTest_Bindings_AngelscriptWorldCollisionFunctionLibraryComponentTests_Private::AddCollisionBox(CollisionOverlapActor, FName(TEXT("NullGuardOverlapTarget")), OverlapExtent, OverlapTargetLocation);
		AActor& CollisionQueryActor = Spawner.SpawnActor<AActor>();
		UBoxComponent* QueryBox = AngelscriptTest_Bindings_AngelscriptWorldCollisionFunctionLibraryComponentTests_Private::AddCollisionBox(CollisionQueryActor, FName(TEXT("NullGuardQueryComponent")), QueryExtent, QueryComponentSpawnLocation);
		if (!TestRunner->TestNotNull(TEXT("World collision null-component test should create the blocker component"), BlockingBox)
			|| !TestRunner->TestNotNull(TEXT("World collision null-component test should create the overlap target"), OverlapBox)
			|| !TestRunner->TestNotNull(TEXT("World collision null-component test should create the query component"), QueryBox))
		{
			return;
		}

		UWorld* World = CollisionBlockingActor.GetWorld();
		if (!TestRunner->TestNotNull(TEXT("World collision null-component test should access the spawned world"), World))
		{
			return;
		}

		FScopedTestWorldContextScope WorldContextScope(&CollisionBlockingActor);

		FComponentQueryParams BaselineComponentQueryParams = FComponentQueryParams::DefaultComponentQueryParams;
		BaselineComponentQueryParams.AddIgnoredComponent(QueryBox);
		FCollisionObjectQueryParams ObjectQueryParams;
		ObjectQueryParams.AddObjectTypesToQuery(ECC_WorldDynamic);

		// Baseline sweep
		TArray<FHitResult> NativeBaselineSweepHits;
		const bool bNativeBaselineSweep = World->ComponentSweepMulti(NativeBaselineSweepHits, QueryBox, SweepHitStart, SweepHitEnd, IdentityRotation, BaselineComponentQueryParams);
		TArray<FHitResult> ScriptBaselineSweepHits;
		bool bScriptBaselineSweep = false;
		if (!ExecuteBoolFunction(
			*TestRunner,
			Engine,
			*Module,
			TEXT("bool RunComponentSweepMultiBaseline(UPrimitiveComponent QueryComponent, TArray<FHitResult>& OutHits)"),
			[this, QueryBox, &ScriptBaselineSweepHits](asIScriptContext& Context)
			{
				return SetArgObjectChecked(*TestRunner, Context, 0, QueryBox, TEXT("RunComponentSweepMultiBaseline"))
					&& SetArgAddressChecked(*TestRunner, Context, 1, &ScriptBaselineSweepHits, TEXT("RunComponentSweepMultiBaseline"));
			},
			TEXT("RunComponentSweepMultiBaseline"),
			bScriptBaselineSweep))
		{
			return;
		}
		ExpectArrayParity(*TestRunner, TEXT("ComponentSweepMulti baseline"), bScriptBaselineSweep, bNativeBaselineSweep, ScriptBaselineSweepHits, NativeBaselineSweepHits);
		TestRunner->TestTrue(TEXT("ComponentSweepMulti baseline should still hit the blocker component"), HitResultsContainComponent(ScriptBaselineSweepHits, BlockingBox));

		// Null component sweep
		TArray<FHitResult> ScriptNullSweepHits = ScriptBaselineSweepHits;
		bool bScriptNullSweep = true;
		if (!ExecuteBoolFunction(
			*TestRunner,
			Engine,
			*Module,
			TEXT("bool RunComponentSweepMultiNull(UPrimitiveComponent QueryComponent, TArray<FHitResult>& OutHits)"),
			[this, &ScriptNullSweepHits](asIScriptContext& Context)
			{
				return SetArgObjectChecked(*TestRunner, Context, 0, nullptr, TEXT("RunComponentSweepMultiNull"))
					&& SetArgAddressChecked(*TestRunner, Context, 1, &ScriptNullSweepHits, TEXT("RunComponentSweepMultiNull"));
			},
			TEXT("RunComponentSweepMultiNull"),
			bScriptNullSweep))
		{
			return;
		}
		TestRunner->TestFalse(TEXT("ComponentSweepMulti should return false when the source component is null"), bScriptNullSweep);
		TestRunner->TestEqual(TEXT("ComponentSweepMulti should clear stale hit results when the source component is null"), ScriptNullSweepHits.Num(), 0);

		// Baseline overlap
		TArray<FOverlapResult> NativeBaselineOverlaps;
		const bool bNativeBaselineOverlap = World->ComponentOverlapMulti(NativeBaselineOverlaps, QueryBox, OverlapTargetLocation, IdentityRotation, BaselineComponentQueryParams, ObjectQueryParams);
		TArray<FOverlapResult> ScriptBaselineOverlaps;
		bool bScriptBaselineOverlap = false;
		if (!ExecuteBoolFunction(
			*TestRunner,
			Engine,
			*Module,
			TEXT("bool RunComponentOverlapMultiBaseline(UPrimitiveComponent QueryComponent, TArray<FOverlapResult>& OutOverlaps)"),
			[this, QueryBox, &ScriptBaselineOverlaps](asIScriptContext& Context)
			{
				return SetArgObjectChecked(*TestRunner, Context, 0, QueryBox, TEXT("RunComponentOverlapMultiBaseline"))
					&& SetArgAddressChecked(*TestRunner, Context, 1, &ScriptBaselineOverlaps, TEXT("RunComponentOverlapMultiBaseline"));
			},
			TEXT("RunComponentOverlapMultiBaseline"),
			bScriptBaselineOverlap))
		{
			return;
		}
		ExpectArrayParity(*TestRunner, TEXT("ComponentOverlapMulti baseline"), bScriptBaselineOverlap, bNativeBaselineOverlap, ScriptBaselineOverlaps, NativeBaselineOverlaps);
		TestRunner->TestTrue(TEXT("ComponentOverlapMulti baseline should still hit the overlap target component"), OverlapsContainComponent(ScriptBaselineOverlaps, OverlapBox));

		// Null component overlap
		TArray<FOverlapResult> ScriptNullOverlaps = ScriptBaselineOverlaps;
		bool bScriptNullOverlap = true;
		if (!ExecuteBoolFunction(
			*TestRunner,
			Engine,
			*Module,
			TEXT("bool RunComponentOverlapMultiNull(UPrimitiveComponent QueryComponent, TArray<FOverlapResult>& OutOverlaps)"),
			[this, &ScriptNullOverlaps](asIScriptContext& Context)
			{
				return SetArgObjectChecked(*TestRunner, Context, 0, nullptr, TEXT("RunComponentOverlapMultiNull"))
					&& SetArgAddressChecked(*TestRunner, Context, 1, &ScriptNullOverlaps, TEXT("RunComponentOverlapMultiNull"));
			},
			TEXT("RunComponentOverlapMultiNull"),
			bScriptNullOverlap))
		{
			return;
		}
		TestRunner->TestFalse(TEXT("ComponentOverlapMulti should return false when the source component is null"), bScriptNullOverlap);
		TestRunner->TestEqual(TEXT("ComponentOverlapMulti should clear stale overlap results when the source component is null"), ScriptNullOverlaps.Num(), 0);

		}
	}
};

#endif
