// ============================================================================
// AngelscriptCollisionValueBindingsTests.cpp
//
// Collision value-type binding coverage — CQTest refactor. Automation IDs:
//   Angelscript.TestModule.Bindings.CollisionValue.FAngelscriptCollisionValueBindingsTest.*
//
// Sections:
//   CollisionShape        — mutators, factories, min-extent queries
//   CollisionResultAccessors — FHitResult / FOverlapResult field round-trip
//
// CQTest adaptation notes:
//   Two IMPLEMENT_SIMPLE_AUTOMATION_TEST merged into one TEST_CLASS.
//   CollisionShape: bitmask `int Run()` split into per-aspect functions.
//   CollisionResultAccessors: uses FASGlobalFunctionInvoker with AddArgRef/AddArgObject
//   for parameterised invocations, plus native-side assertions.
// ============================================================================

#include "CQTest.h"
#include "Shared/AngelscriptTestMacros.h"
#include "Shared/AngelscriptTestModuleScope.h"
#include "Shared/AngelscriptTestExecute.h"

#include "Components/BoxComponent.h"
#include "Engine/OverlapResult.h"
#include "GameFramework/Actor.h"

#if WITH_DEV_AUTOMATION_TESTS


// ----------------------------------------------------------------------------
// Profile
// ----------------------------------------------------------------------------


// ----------------------------------------------------------------------------
// Test class
// ----------------------------------------------------------------------------

TEST_CLASS_WITH_FLAGS(FAngelscriptCollisionValueBindingsTest,
	"Angelscript.TestModule.Bindings.CollisionValue",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	BEFORE_ALL()
	{
		ASTEST_CREATE_ENGINE();
	}

	AFTER_ALL() { FAngelscriptEngine& Engine = ASTEST_GET_ENGINE(); ASTEST_RESET_ENGINE(Engine); }

	// ====================================================================
	// Section: CollisionShape
	// ====================================================================

	TEST_METHOD(CollisionShape)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASCollisionValue_CollisionShape"), TEXT(R"(
bool MatchesVector(const FVector InValue, const float X, const float Y, const float Z)
{
	return InValue.X == X && InValue.Y == Y && InValue.Z == Z;
}

int Shape_DefaultIsLine()
{
	FCollisionShape Shape;
	return (Shape.IsLine() && Shape.IsNearlyZero() && MatchesVector(Shape.GetExtent(), 0.0f, 0.0f, 0.0f)) ? 1 : 0;
}
int Shape_SetBox()
{
	FCollisionShape Shape;
	Shape.SetBox(FVector(10.0f, 20.0f, 30.0f));
	return (Shape.IsBox() && MatchesVector(Shape.GetBox(), 10.0f, 20.0f, 30.0f) && MatchesVector(Shape.GetExtent(), 10.0f, 20.0f, 30.0f)) ? 1 : 0;
}
int Shape_SetSphere()
{
	FCollisionShape Shape;
	Shape.SetSphere(15.0f);
	return (Shape.IsSphere() && Shape.GetSphereRadius() == 15.0f) ? 1 : 0;
}
int Shape_SetCapsule()
{
	FCollisionShape Shape;
	Shape.SetCapsule(7.0f, 12.0f);
	return (Shape.IsCapsule() && Shape.GetCapsuleRadius() == 7.0f && Shape.GetCapsuleHalfHeight() == 12.0f) ? 1 : 0;
}
int Shape_MakeBox()
{
	FCollisionShape BoxFactory = FCollisionShape::MakeBox(FVector(10.0f, 20.0f, 30.0f));
	return (BoxFactory.IsBox() && MatchesVector(BoxFactory.GetBox(), 10.0f, 20.0f, 30.0f)) ? 1 : 0;
}
int Shape_MakeSphere()
{
	FCollisionShape SphereFactory = FCollisionShape::MakeSphere(15.0f);
	return (SphereFactory.IsSphere() && SphereFactory.GetSphereRadius() == 15.0f) ? 1 : 0;
}
int Shape_MakeCapsule()
{
	FCollisionShape CapsuleFactory = FCollisionShape::MakeCapsule(7.0f, 12.0f);
	return (CapsuleFactory.IsCapsule() && CapsuleFactory.GetCapsuleRadius() == 7.0f && CapsuleFactory.GetCapsuleHalfHeight() == 12.0f) ? 1 : 0;
}
int Shape_MinExtents()
{
	return (FCollisionShape::MinBoxExtent() > 0.0f
		&& FCollisionShape::MinSphereRadius() > 0.0f
		&& FCollisionShape::MinCapsuleRadius() > 0.0f
		&& FCollisionShape::MinCapsuleAxisHalfHeight() > 0.0f) ? 1 : 0;
}
)"));
		if (!Mod.IsValid()) return;
		auto& M = Mod.GetModule();

		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int Shape_DefaultIsLine()"), TEXT("default FCollisionShape should be line with zero extent"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int Shape_SetBox()"), TEXT("SetBox should configure box shape with correct extents"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int Shape_SetSphere()"), TEXT("SetSphere should configure sphere shape with correct radius"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int Shape_SetCapsule()"), TEXT("SetCapsule should configure capsule with correct radius and half-height"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int Shape_MakeBox()"), TEXT("MakeBox factory should produce a valid box shape"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int Shape_MakeSphere()"), TEXT("MakeSphere factory should produce a valid sphere shape"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int Shape_MakeCapsule()"), TEXT("MakeCapsule factory should produce a valid capsule shape"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int Shape_MinExtents()"), TEXT("min-extent queries should all return positive values"), 1);
	}

	// ====================================================================
	// Section: CollisionResultAccessors
	// ====================================================================

	TEST_METHOD(CollisionResultAccessors)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASCollisionValue_CollisionResultAccessors"), TEXT(R"(
bool MatchesVector(const FVector InValue, const float X, const float Y, const float Z)
{
	return InValue.X == X && InValue.Y == Y && InValue.Z == Z;
}

int PopulateCollisionResults(FHitResult& OutHit, FOverlapResult& OutOverlap, AActor ExpectedActor, UPrimitiveComponent ExpectedComponent)
{
	int MismatchMask = 0;

	OutHit = FHitResult(FVector(-1.0f, 0.0f, 0.0f), FVector(1.0f, 0.0f, 0.0f));
	OutHit.FaceIndex = 1;
	OutHit.ElementIndex = 2;
	OutHit.Item = 3;
	OutHit.MyItem = 4;
	OutHit.TraceStart = FVector(-2.0f, 1.0f, 0.0f);
	OutHit.TraceEnd = FVector(3.0f, 4.0f, 5.0f);
	OutHit.ImpactPoint = FVector(6.0f, 7.0f, 8.0f);
	OutHit.ImpactNormal = FVector(0.0f, 0.0f, 1.0f);
	OutHit.BoneName = n"Bone";
	OutHit.MyBoneName = n"MyBone";

	if (OutHit.FaceIndex != 1)
		MismatchMask |= 1;
	if (OutHit.ElementIndex != 2)
		MismatchMask |= 2;
	if (OutHit.Item != 3)
		MismatchMask |= 4;
	if (OutHit.MyItem != 4)
		MismatchMask |= 8;
	if (!MatchesVector(OutHit.TraceStart, -2.0f, 1.0f, 0.0f))
		MismatchMask |= 16;
	if (!MatchesVector(OutHit.TraceEnd, 3.0f, 4.0f, 5.0f))
		MismatchMask |= 32;
	if (!MatchesVector(OutHit.ImpactPoint, 6.0f, 7.0f, 8.0f))
		MismatchMask |= 64;
	if (!MatchesVector(OutHit.ImpactNormal, 0.0f, 0.0f, 1.0f))
		MismatchMask |= 128;
	if (OutHit.BoneName != n"Bone")
		MismatchMask |= 256;
	if (OutHit.MyBoneName != n"MyBone")
		MismatchMask |= 512;

	OutOverlap.ItemIndex = 9;
	OutOverlap.SetActor(ExpectedActor);
	OutOverlap.SetComponent(ExpectedComponent);
	OutOverlap.SetBlockingHit(true);

	if (OutOverlap.GetActor() != ExpectedActor)
		MismatchMask |= 1024;
	if (OutOverlap.GetComponent() != ExpectedComponent)
		MismatchMask |= 2048;
	if (!OutOverlap.GetbBlockingHit())
		MismatchMask |= 4096;
	if (OutOverlap.ItemIndex != 9)
		MismatchMask |= 8192;

	OutOverlap.SetBlockingHit(false);
	if (OutOverlap.GetbBlockingHit())
		MismatchMask |= 16384;

	return MismatchMask;
}
)"));
		if (!Mod.IsValid()) return;
		auto& M = Mod.GetModule();

		AActor* TestActor = NewObject<AActor>(GetTransientPackage(), NAME_None, RF_Transient);
		UBoxComponent* TestComponent = NewObject<UBoxComponent>(TestActor, NAME_None, RF_Transient);
		if (!TestRunner->TestNotNull(TEXT("CollisionResultAccessors should create a transient actor"), TestActor) ||
			!TestRunner->TestNotNull(TEXT("CollisionResultAccessors should create a transient primitive component"), TestComponent))
		{
			return;
		}

		FHitResult ScriptHit(FVector::ZeroVector, FVector::ZeroVector);
		FOverlapResult ScriptOverlap;

		// Use FASGlobalFunctionInvoker with AddArgRef/AddArgObject for parameterised call
		FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, M,
			TEXT("int PopulateCollisionResults(FHitResult&, FOverlapResult&, AActor, UPrimitiveComponent)"));
		if (!Invoker.IsValid()) return;

		Invoker.AddArgRef(ScriptHit);
		Invoker.AddArgRef(ScriptOverlap);
		Invoker.AddArgObject(TestActor);
		Invoker.AddArgObject(TestComponent);

		const int32 ResultMask = Invoker.CallAndReturn<int32>(INDEX_NONE);

		TestRunner->TestEqual(TEXT("CollisionResultAccessors should preserve script-side round-trip checks"), ResultMask, 0);
		TestRunner->TestEqual(TEXT("CollisionResultAccessors should write FaceIndex into native FHitResult state"), ScriptHit.FaceIndex, 1);
		TestRunner->TestEqual(TEXT("CollisionResultAccessors should write ElementIndex into native FHitResult state"), ScriptHit.ElementIndex, static_cast<uint8>(2));
		TestRunner->TestEqual(TEXT("CollisionResultAccessors should write Item into native FHitResult state"), ScriptHit.Item, 3);
		TestRunner->TestEqual(TEXT("CollisionResultAccessors should write MyItem into native FHitResult state"), ScriptHit.MyItem, 4);
		TestRunner->TestTrue(TEXT("CollisionResultAccessors should write TraceStart into native FHitResult state"), ScriptHit.TraceStart.Equals(FVector(-2.0f, 1.0f, 0.0f)));
		TestRunner->TestTrue(TEXT("CollisionResultAccessors should write TraceEnd into native FHitResult state"), ScriptHit.TraceEnd.Equals(FVector(3.0f, 4.0f, 5.0f)));
		TestRunner->TestTrue(TEXT("CollisionResultAccessors should write ImpactPoint into native FHitResult state"), ScriptHit.ImpactPoint.Equals(FVector(6.0f, 7.0f, 8.0f)));
		TestRunner->TestTrue(TEXT("CollisionResultAccessors should write ImpactNormal into native FHitResult state"), ScriptHit.ImpactNormal.Equals(FVector(0.0f, 0.0f, 1.0f)));
		TestRunner->TestEqual(TEXT("CollisionResultAccessors should write BoneName into native FHitResult state"), ScriptHit.BoneName, FName(TEXT("Bone")));
		TestRunner->TestEqual(TEXT("CollisionResultAccessors should write MyBoneName into native FHitResult state"), ScriptHit.MyBoneName, FName(TEXT("MyBone")));
		TestRunner->TestEqual(TEXT("CollisionResultAccessors should write ItemIndex into native FOverlapResult state"), ScriptOverlap.ItemIndex, 9);
		TestRunner->TestEqual(TEXT("CollisionResultAccessors should round-trip actor handle through native FOverlapResult state"), ScriptOverlap.GetActor(), TestActor);
		TestRunner->TestEqual(TEXT("CollisionResultAccessors should round-trip component handle through native FOverlapResult state"), ScriptOverlap.GetComponent(), static_cast<UPrimitiveComponent*>(TestComponent));
		TestRunner->TestFalse(TEXT("CollisionResultAccessors should leave bBlockingHit cleared after final SetBlockingHit(false)"), ScriptOverlap.bBlockingHit);
	}
};

#endif
