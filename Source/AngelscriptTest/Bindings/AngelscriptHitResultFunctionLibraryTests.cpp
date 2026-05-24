// ============================================================================
// AngelscriptHitResultFunctionLibraryTests.cpp
//
// HitResult function library accessor binding coverage — CQTest refactor.
// Automation IDs:
//   Angelscript.TestModule.FunctionLibraries.HitResult.FAngelscriptHitResultFunctionLibraryTest.*
//
// Sections:
//   Accessors — populate / reset round-trip via FASGlobalFunctionInvoker
//
// CQTest adaptation notes:
//   Single IMPLEMENT_SIMPLE_AUTOMATION_TEST merged into TEST_CLASS.
//   Uses FASGlobalFunctionInvoker with AddArgRef/AddArgObject for parameterised
//   invocations. Original `this` assertions replaced with `*TestRunner`.
//   Keeps FActorTestSpawner + FScopedTestWorldContextScope for world context.
// ============================================================================

#include "CQTest.h"
#include "Shared/AngelscriptTestMacros.h"
#include "Shared/AngelscriptTestModuleScope.h"
#include "Shared/AngelscriptTestExecute.h"

#include "Components/ActorTestSpawner.h"
#include "Components/BoxComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

#if WITH_DEV_AUTOMATION_TESTS


// ----------------------------------------------------------------------------
// Profile
// ----------------------------------------------------------------------------


// ----------------------------------------------------------------------------
// Helpers
// ----------------------------------------------------------------------------

namespace
{
	UBoxComponent* AddHitResultTestComponent(AActor& Owner, const FName ComponentName)
	{
		UBoxComponent* BoxComponent = NewObject<UBoxComponent>(&Owner, ComponentName);
		check(BoxComponent != nullptr);

		Owner.AddInstanceComponent(BoxComponent);
		Owner.SetRootComponent(BoxComponent);
		BoxComponent->RegisterComponent();
		BoxComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		BoxComponent->SetBoxExtent(FVector(20.0f, 20.0f, 20.0f));
		BoxComponent->SetWorldLocation(FVector(25.0f, 0.0f, 0.0f));
		return BoxComponent;
	}
}

// ----------------------------------------------------------------------------
// Test class
// ----------------------------------------------------------------------------

TEST_CLASS_WITH_FLAGS(FAngelscriptHitResultFunctionLibraryTest,
	"Angelscript.TestModule.FunctionLibraries.HitResult",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	BEFORE_ALL()
	{
		ASTEST_CREATE_ENGINE();
	}

	AFTER_ALL() { FAngelscriptEngine& Engine = ASTEST_GET_ENGINE(); ASTEST_RESET_ENGINE(Engine); }

	// ====================================================================
	// Section: Accessors
	// ====================================================================

	TEST_METHOD(Accessors)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASHitResult_Accessors"), TEXT(R"(
int PopulateHitResult(FHitResult& OutHit, AActor ExpectedActor, UPrimitiveComponent ExpectedComponent)
{
	int MismatchMask = 0;

	if (OutHit.GetbBlockingHit())
		MismatchMask |= 1;
	if (OutHit.GetbStartPenetrating())
		MismatchMask |= 2;

	OutHit.SetActor(ExpectedActor);
	AActor RetrievedActor = OutHit.GetActor();
	if (!IsValid(RetrievedActor))
		MismatchMask |= 4;

	OutHit.SetComponent(ExpectedComponent);
	AActor RetrievedActorAfterComponent = OutHit.GetActor();
	if (!IsValid(RetrievedActorAfterComponent))
		MismatchMask |= 8;

	UPrimitiveComponent RetrievedComponent = OutHit.GetComponent();
	if (!IsValid(RetrievedComponent))
		MismatchMask |= 16;

	OutHit.SetBlockingHit(true);
	if (!OutHit.GetbBlockingHit())
		MismatchMask |= 32;

	OutHit.SetbBlockingHit(false);
	if (OutHit.GetbBlockingHit())
		MismatchMask |= 64;

	OutHit.SetbStartPenetrating(true);
	if (!OutHit.GetbStartPenetrating())
		MismatchMask |= 128;

	return MismatchMask;
}

int ResetHitResult(FHitResult& Hit)
{
	int MismatchMask = 0;

	Hit.Reset();
	if (Hit.GetbBlockingHit())
		MismatchMask |= 1;
	if (Hit.GetbStartPenetrating())
		MismatchMask |= 2;

	return MismatchMask;
}
)"));
		if (!Mod.IsValid()) return;
		auto& M = Mod.GetModule();

		// Create world and actor fixture
		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();

		AActor& TestActor = Spawner.SpawnActor<AActor>();
		UBoxComponent* TestComponent = AddHitResultTestComponent(TestActor, TEXT("HitResultTestComponent"));
		if (!TestRunner->TestNotNull(TEXT("HitResult accessor test should create a primitive component"), TestComponent))
		{
			return;
		}
		if (!TestRunner->TestEqual(TEXT("HitResult accessor test component should belong to the spawned actor"),
				TestComponent->GetOwner(), &TestActor))
		{
			return;
		}

		UWorld* TestWorld = TestActor.GetWorld();
		if (!TestRunner->TestNotNull(TEXT("HitResult accessor test should access the spawned world"), TestWorld))
		{
			return;
		}

		FScopedTestWorldContextScope WorldContextScope(&TestActor);

		FHitResult ScriptHit(FVector::ZeroVector, FVector::ForwardVector);
		TestRunner->TestNull(TEXT("HitResult accessor test should start with no actor handle"), ScriptHit.GetActor());
		TestRunner->TestNull(TEXT("HitResult accessor test should start with no component handle"), ScriptHit.GetComponent());
		TestRunner->TestFalse(TEXT("HitResult accessor test should start with bBlockingHit cleared"), ScriptHit.bBlockingHit);
		TestRunner->TestFalse(TEXT("HitResult accessor test should start with bStartPenetrating cleared"), ScriptHit.bStartPenetrating);

		// --- PopulateHitResult ---
		{
			FASGlobalFunctionInvoker PopulateInvoker(*TestRunner, Engine, M,
				TEXT("int PopulateHitResult(FHitResult&, AActor, UPrimitiveComponent)"));
			if (!PopulateInvoker.IsValid()) return;

			PopulateInvoker.AddArgRef(ScriptHit);
			PopulateInvoker.AddArgObject(&TestActor);
			PopulateInvoker.AddArgObject(TestComponent);

			const int32 PopulateResultMask = PopulateInvoker.CallAndReturn<int32>(INDEX_NONE);

			TestRunner->TestEqual(
				TEXT("FHitResult function library accessors should allow script-side helper calls without flag mismatches"),
				PopulateResultMask, 0);
			TestRunner->TestEqual(TEXT("HitResult accessor test should round-trip the actor handle back into native state"),
				ScriptHit.GetActor(), &TestActor);
			TestRunner->TestEqual(TEXT("HitResult accessor test should round-trip the component handle back into native state"),
				ScriptHit.GetComponent(), static_cast<UPrimitiveComponent*>(TestComponent));
			TestRunner->TestFalse(TEXT("HitResult accessor test should leave bBlockingHit cleared after SetbBlockingHit(false)"),
				ScriptHit.bBlockingHit);
			TestRunner->TestTrue(TEXT("HitResult accessor test should preserve the start penetrating flag set by script"),
				ScriptHit.bStartPenetrating);
		}

		// --- ResetHitResult ---
		{
			FASGlobalFunctionInvoker ResetInvoker(*TestRunner, Engine, M,
				TEXT("int ResetHitResult(FHitResult&)"));
			if (!ResetInvoker.IsValid()) return;

			ResetInvoker.AddArgRef(ScriptHit);

			const int32 ResetResultMask = ResetInvoker.CallAndReturn<int32>(INDEX_NONE);

			TestRunner->TestEqual(
				TEXT("FHitResult function library Reset helper should clear script-visible blocking and penetration flags"),
				ResetResultMask, 0);
			TestRunner->TestNull(TEXT("HitResult accessor test should clear the actor handle after Reset"), ScriptHit.GetActor());
			TestRunner->TestNull(TEXT("HitResult accessor test should clear the component handle after Reset"), ScriptHit.GetComponent());
			TestRunner->TestFalse(TEXT("HitResult accessor test should clear bBlockingHit after Reset"), ScriptHit.bBlockingHit);
			TestRunner->TestFalse(TEXT("HitResult accessor test should clear bStartPenetrating after Reset"), ScriptHit.bStartPenetrating);
		}
	}
};

#endif
