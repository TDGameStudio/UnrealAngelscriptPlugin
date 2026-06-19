// ============================================================================
// AngelscriptWorldCollisionAsyncSweepBindingsTests.cpp
//
// World collision async sweep callback coverage — CQTest refactor. Automation IDs:
//   Angelscript.TestModule.Bindings.WorldCollisionAsyncSweep.FAngelscriptWorldCollisionAsyncSweepBindingsTest.*
//
// Sections:
//   AsyncSweepCallbacks — spawns actor with callbacks, fires async sweeps, ticks world, verifies results
//
// CQTest adaptation notes:
//   This is an integration test requiring FULL engine, actor spawning, world ticking.
//   The original single monolithic test is preserved as one TEST_METHOD since the
//   async sweep workflow is inherently sequential. Verification is split into per-case
//   assertions using the profile naming conventions.
// ============================================================================

#include "CQTest.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestModuleScope.h"
#include "AngelscriptTestExecute.h"
#include "AngelscriptFunctionalTestUtils.h"

#include "Components/ActorTestSpawner.h"
#include "Components/BoxComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Misc/ScopeExit.h"
#include "UObject/UnrealType.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptFunctionalTestUtils;

// ----------------------------------------------------------------------------
// Profile
// ----------------------------------------------------------------------------


// ----------------------------------------------------------------------------
// Local helpers
// ----------------------------------------------------------------------------

namespace
{
	static const FName AsyncSweepModuleName(TEXT("ASWCAsyncSweepCallbacks"));
	static const FString AsyncSweepFilename(TEXT("WCAsyncSweepCallbacks.as"));
	static const FName AsyncSweepClassName(TEXT("ATestWorldCollisionAsyncSweepCallbacks"));
	static const FVector AsyncSweepStart(-200.0f, 0.0f, 0.0f);
	static const FVector AsyncSweepEnd(200.0f, 0.0f, 0.0f);
	static const FVector AsyncSweepTargetLocation(0.0f, 0.0f, 0.0f);
	static const FVector AsyncSweepTargetExtent(50.0f, 50.0f, 50.0f);
	static constexpr float AsyncTickDeltaTime = 1.0f / 60.0f;
	static constexpr int32 AsyncMaxTickCount = 90;

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

	bool ExecuteGeneratedIntMethod(
		FAutomationTestBase& Test,
		UObject* Object,
		UClass* OwnerClass,
		FName FunctionName,
		int32& OutResult)
	{
		FNoDiscardAsserter Assert(Test);
		UFunction* Function = FindGeneratedFunction(OwnerClass, FunctionName);
		if (!Assert.IsNotNull(
				Function,
				*FString::Printf(TEXT("World collision async sweep method '%s' should exist"), *FunctionName.ToString())))
		{
			return false;
		}

		return Assert.IsTrue(
			ExecuteGeneratedIntEventOnGameThread(Object, Function, OutResult),
			*FString::Printf(TEXT("World collision async sweep method '%s' should execute"), *FunctionName.ToString()));
	}

	template<typename ValueType>
	bool WriteObjectPropertyChecked(
		FAutomationTestBase& Test,
		UObject* Object,
		FName PropertyName,
		ValueType* Value)
	{
		FNoDiscardAsserter Assert(Test);
		if (!Assert.IsNotNull(Object, TEXT("World collision async sweep object should be valid for reflected writes")))
		{
			return false;
		}

		FObjectProperty* Property = FindFProperty<FObjectProperty>(Object->GetClass(), PropertyName);
		if (!Assert.IsNotNull(
				Property,
				*FString::Printf(TEXT("World collision async sweep property '%s' should exist"), *PropertyName.ToString())))
		{
			return false;
		}

		Property->SetObjectPropertyValue_InContainer(Object, Value);
		return true;
	}

	bool ReadUInt64PropertyChecked(
		FAutomationTestBase& Test,
		UObject* Object,
		FName PropertyName,
		uint64& OutValue)
	{
		FNoDiscardAsserter Assert(Test);
		if (!Assert.IsNotNull(Object, TEXT("World collision async sweep object should be valid for uint64 property reads")))
		{
			return false;
		}

		FUInt64Property* Property = FindFProperty<FUInt64Property>(Object->GetClass(), PropertyName);
		if (!Assert.IsNotNull(
				Property,
				*FString::Printf(TEXT("World collision async sweep property '%s' should exist"), *PropertyName.ToString())))
		{
			return false;
		}

		OutValue = Property->GetPropertyValue_InContainer(Object);
		return true;
	}

	bool ReadBoolPropertyChecked(
		FAutomationTestBase& Test,
		UObject* Object,
		FName PropertyName,
		bool& OutValue)
	{
		return ReadPropertyValue<FBoolProperty>(Test, Object, PropertyName, OutValue);
	}

	bool WaitForAsyncSweepCallbacks(
		FAutomationTestBase& Test,
		FAngelscriptEngine& Engine,
		UWorld& World,
		AActor& ScriptActor)
	{
		FNoDiscardAsserter Assert(Test);
		FIntProperty* ChannelCallbackCountProperty = FindFProperty<FIntProperty>(ScriptActor.GetClass(), TEXT("ChannelCallbackCount"));
		FIntProperty* ObjectCallbackCountProperty = FindFProperty<FIntProperty>(ScriptActor.GetClass(), TEXT("ObjectCallbackCount"));
		FIntProperty* ProfileCallbackCountProperty = FindFProperty<FIntProperty>(ScriptActor.GetClass(), TEXT("ProfileCallbackCount"));
		bool bHasCallbackProperties = true;
		bHasCallbackProperties &= Assert.IsNotNull(ChannelCallbackCountProperty, TEXT("Async sweep actor should expose ChannelCallbackCount"));
		bHasCallbackProperties &= Assert.IsNotNull(ObjectCallbackCountProperty, TEXT("Async sweep actor should expose ObjectCallbackCount"));
		bHasCallbackProperties &= Assert.IsNotNull(ProfileCallbackCountProperty, TEXT("Async sweep actor should expose ProfileCallbackCount"));
		if (!bHasCallbackProperties)
		{
			return false;
		}

		for (int32 TickIndex = 0; TickIndex < AsyncMaxTickCount; ++TickIndex)
		{
			const int32 ChannelCallbackCount = ChannelCallbackCountProperty->GetPropertyValue_InContainer(&ScriptActor);
			const int32 ObjectCallbackCount = ObjectCallbackCountProperty->GetPropertyValue_InContainer(&ScriptActor);
			const int32 ProfileCallbackCount = ProfileCallbackCountProperty->GetPropertyValue_InContainer(&ScriptActor);
			if (ChannelCallbackCount >= 1 && ObjectCallbackCount >= 1 && ProfileCallbackCount >= 1)
			{
				return true;
			}

			TickWorld(Engine, World, AsyncTickDeltaTime, 1);
		}

		Test.AddError(FString::Printf(
			TEXT("Async sweep callbacks did not complete within %d ticks."),
			AsyncMaxTickCount));
		return false;
	}
}

// ----------------------------------------------------------------------------
// Test class
// ----------------------------------------------------------------------------

TEST_CLASS_WITH_FLAGS(FAngelscriptWorldCollisionAsyncSweepBindingsTest,
	"Angelscript.TestModule.Bindings.WorldCollisionAsyncSweep",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	// ====================================================================
	// Section: AsyncSweepCallbacks
	// ====================================================================

	TEST_METHOD(AsyncSweepCallbacks)
	{
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE_FULL();
		FAngelscriptEngineScope Scope(Engine);
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*AsyncSweepModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			AsyncSweepModuleName,
			AsyncSweepFilename,
			TEXT(R"AS(
UCLASS()
class ATestWorldCollisionAsyncSweepCallbacks : AActor
{
	UPROPERTY()
	AActor ExpectedActor;

	UPROPERTY()
	UPrimitiveComponent ExpectedComponent;

	UPROPERTY()
	int ChannelCallbackCount = 0;
	UPROPERTY()
	int ChannelUserData = 0;
	UPROPERTY()
	int ChannelHitCount = 0;
	UPROPERTY()
	int ChannelQuerySucceeded = 0;
	UPROPERTY()
	int ChannelQueryHitCount = 0;
	UPROPERTY()
	int ChannelHandleValidInitially = 0;
	UPROPERTY()
	uint64 ChannelHandleRaw = 0;
	UPROPERTY()
	uint64 LastChannelCallbackHandle = 0;
	UPROPERTY()
	bool bChannelHitActorMatched = false;
	UPROPERTY()
	bool bChannelHitComponentMatched = false;

	UPROPERTY()
	int ObjectCallbackCount = 0;
	UPROPERTY()
	int ObjectUserData = 0;
	UPROPERTY()
	int ObjectHitCount = 0;
	UPROPERTY()
	int ObjectQuerySucceeded = 0;
	UPROPERTY()
	int ObjectQueryHitCount = 0;
	UPROPERTY()
	int ObjectHandleValidInitially = 0;
	UPROPERTY()
	uint64 ObjectHandleRaw = 0;
	UPROPERTY()
	uint64 LastObjectCallbackHandle = 0;
	UPROPERTY()
	bool bObjectHitActorMatched = false;
	UPROPERTY()
	bool bObjectHitComponentMatched = false;

	UPROPERTY()
	int ProfileCallbackCount = 0;
	UPROPERTY()
	int ProfileUserData = 0;
	UPROPERTY()
	int ProfileHitCount = 0;
	UPROPERTY()
	int ProfileQuerySucceeded = 0;
	UPROPERTY()
	int ProfileQueryHitCount = 0;
	UPROPERTY()
	int ProfileHandleValidInitially = 0;
	UPROPERTY()
	uint64 ProfileHandleRaw = 0;
	UPROPERTY()
	uint64 LastProfileCallbackHandle = 0;
	UPROPERTY()
	bool bProfileHitActorMatched = false;
	UPROPERTY()
	bool bProfileHitComponentMatched = false;

	FTraceHandle ChannelHandle;
	FTraceHandle ObjectHandle;
	FTraceHandle ProfileHandle;

	UFUNCTION()
	int StartAsyncSweeps()
	{
		if (ExpectedActor == nullptr || ExpectedComponent == nullptr)
			return 5;

		const FCollisionShape Shape = FCollisionShape::MakeBox(FVector(30.0f, 30.0f, 30.0f));

		FScriptTraceDelegate ChannelDelegate;
		ChannelDelegate.BindUFunction(this, n"HandleChannelSweep");
		ChannelHandle = System::AsyncSweepByChannel(
			EAsyncTraceType::Single,
			FVector(-200.0f, 0.0f, 0.0f),
			FVector(200.0f, 0.0f, 0.0f),
			FQuat::Identity,
			ECollisionChannel::ECC_Visibility,
			Shape,
			FCollisionQueryParams::DefaultQueryParam,
			FCollisionResponseParams::DefaultResponseParam,
			ChannelDelegate,
			101);
		ChannelHandleRaw = ChannelHandle._Handle;
		if (!System::IsTraceHandleValid(ChannelHandle, false))
			return 10;
		ChannelHandleValidInitially = 1;

		FCollisionObjectQueryParams ObjectQueryParams;
		ObjectQueryParams.AddObjectTypesToQuery(ECollisionChannel::ECC_WorldDynamic);
		FScriptTraceDelegate ObjectDelegate;
		ObjectDelegate.BindUFunction(this, n"HandleObjectSweep");
		ObjectHandle = System::AsyncSweepByObjectType(
			EAsyncTraceType::Single,
			FVector(-200.0f, 0.0f, 0.0f),
			FVector(200.0f, 0.0f, 0.0f),
			FQuat::Identity,
			ObjectQueryParams,
			Shape,
			FCollisionQueryParams::DefaultQueryParam,
			ObjectDelegate,
			202);
		ObjectHandleRaw = ObjectHandle._Handle;
		if (!System::IsTraceHandleValid(ObjectHandle, false))
			return 20;
		ObjectHandleValidInitially = 1;

		FScriptTraceDelegate ProfileDelegate;
		ProfileDelegate.BindUFunction(this, n"HandleProfileSweep");
		ProfileHandle = System::AsyncSweepByProfile(
			EAsyncTraceType::Single,
			FVector(-200.0f, 0.0f, 0.0f),
			FVector(200.0f, 0.0f, 0.0f),
			FQuat::Identity,
			CollisionProfile::BlockAllDynamic,
			Shape,
			FCollisionQueryParams::DefaultQueryParam,
			ProfileDelegate,
			303);
		ProfileHandleRaw = ProfileHandle._Handle;
		if (!System::IsTraceHandleValid(ProfileHandle, false))
			return 30;
		ProfileHandleValidInitially = 1;

		return 1;
	}

	UFUNCTION()
	void HandleChannelSweep(uint64 TraceHandleValue, const TArray<FHitResult>& OutHits, uint32 UserData)
	{
		ChannelCallbackCount += 1;
		LastChannelCallbackHandle = TraceHandleValue;
		ChannelUserData = int(UserData);
		ChannelHitCount = OutHits.Num();
		if (OutHits.Num() > 0)
		{
			bChannelHitActorMatched = OutHits[0].GetActor() == ExpectedActor;
			bChannelHitComponentMatched = OutHits[0].GetComponent() == ExpectedComponent;
		}

		FTraceDatum Datum;
		ChannelQuerySucceeded = System::QueryTraceData(ChannelHandle, Datum) ? 1 : 0;
		ChannelQueryHitCount = Datum.OutHits.Num();
	}

	UFUNCTION()
	void HandleObjectSweep(uint64 TraceHandleValue, const TArray<FHitResult>& OutHits, uint32 UserData)
	{
		ObjectCallbackCount += 1;
		LastObjectCallbackHandle = TraceHandleValue;
		ObjectUserData = int(UserData);
		ObjectHitCount = OutHits.Num();
		if (OutHits.Num() > 0)
		{
			bObjectHitActorMatched = OutHits[0].GetActor() == ExpectedActor;
			bObjectHitComponentMatched = OutHits[0].GetComponent() == ExpectedComponent;
		}

		FTraceDatum Datum;
		ObjectQuerySucceeded = System::QueryTraceData(ObjectHandle, Datum) ? 1 : 0;
		ObjectQueryHitCount = Datum.OutHits.Num();
	}

	UFUNCTION()
	void HandleProfileSweep(uint64 TraceHandleValue, const TArray<FHitResult>& OutHits, uint32 UserData)
	{
		ProfileCallbackCount += 1;
		LastProfileCallbackHandle = TraceHandleValue;
		ProfileUserData = int(UserData);
		ProfileHitCount = OutHits.Num();
		if (OutHits.Num() > 0)
		{
			bProfileHitActorMatched = OutHits[0].GetActor() == ExpectedActor;
			bProfileHitComponentMatched = OutHits[0].GetComponent() == ExpectedComponent;
		}

		FTraceDatum Datum;
		ProfileQuerySucceeded = System::QueryTraceData(ProfileHandle, Datum) ? 1 : 0;
		ProfileQueryHitCount = Datum.OutHits.Num();
	}
}
)AS"),
			AsyncSweepClassName);
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();

		AActor& BlockingActor = Spawner.SpawnActor<AActor>();
		UBoxComponent* BlockingBox = AddCollisionBox(
			BlockingActor,
			TEXT("AsyncSweepBlockingTarget"),
			AsyncSweepTargetExtent,
			AsyncSweepTargetLocation);
		ASSERT_THAT(IsNotNull(BlockingBox, TEXT("Async sweep blocking box should be created")));

		AActor* ScriptActor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(ScriptActor, TEXT("Async sweep script actor should spawn")));

		if (!WriteObjectPropertyChecked(*TestRunner, ScriptActor, TEXT("ExpectedActor"), &BlockingActor)
			|| !WriteObjectPropertyChecked(*TestRunner, ScriptActor, TEXT("ExpectedComponent"), BlockingBox))
		{
			return;
		}

		BeginPlayActor(Engine, *ScriptActor);

		UWorld* World = BlockingActor.GetWorld();
		ASSERT_THAT(IsNotNull(World, TEXT("Async sweep test should access the spawned world")));

		int32 StartResult = 0;
		if (!ExecuteGeneratedIntMethod(*TestRunner, ScriptActor, ScriptClass, TEXT("StartAsyncSweeps"), StartResult))
		{
			return;
		}
		ASSERT_THAT(AreEqual(1, StartResult, TEXT("Async sweep start method should acknowledge launch")));

		if (!WaitForAsyncSweepCallbacks(*TestRunner, Engine, *World, *ScriptActor))
		{
			return;
		}

		// Read all result properties
		int32 ChannelCallbackCount = 0, ChannelUserData = 0, ChannelHitCount = 0;
		int32 ChannelQuerySucceeded = 0, ChannelQueryHitCount = 0, ChannelHandleValidInitially = 0;
		int32 ObjectCallbackCount = 0, ObjectUserData = 0, ObjectHitCount = 0;
		int32 ObjectQuerySucceeded = 0, ObjectQueryHitCount = 0, ObjectHandleValidInitially = 0;
		int32 ProfileCallbackCount = 0, ProfileUserData = 0, ProfileHitCount = 0;
		int32 ProfileQuerySucceeded = 0, ProfileQueryHitCount = 0, ProfileHandleValidInitially = 0;
		uint64 ChannelHandleRaw = 0, LastChannelCallbackHandle = 0;
		uint64 ObjectHandleRaw = 0, LastObjectCallbackHandle = 0;
		uint64 ProfileHandleRaw = 0, LastProfileCallbackHandle = 0;
		bool bChannelHitActorMatched = false, bChannelHitComponentMatched = false;
		bool bObjectHitActorMatched = false, bObjectHitComponentMatched = false;
		bool bProfileHitActorMatched = false, bProfileHitComponentMatched = false;

		if (!ReadIntPropertyChecked(*TestRunner, ScriptActor, TEXT("ChannelCallbackCount"), ChannelCallbackCount)
			|| !ReadIntPropertyChecked(*TestRunner, ScriptActor, TEXT("ChannelUserData"), ChannelUserData)
			|| !ReadIntPropertyChecked(*TestRunner, ScriptActor, TEXT("ChannelHitCount"), ChannelHitCount)
			|| !ReadIntPropertyChecked(*TestRunner, ScriptActor, TEXT("ChannelQuerySucceeded"), ChannelQuerySucceeded)
			|| !ReadIntPropertyChecked(*TestRunner, ScriptActor, TEXT("ChannelQueryHitCount"), ChannelQueryHitCount)
			|| !ReadIntPropertyChecked(*TestRunner, ScriptActor, TEXT("ChannelHandleValidInitially"), ChannelHandleValidInitially)
			|| !ReadIntPropertyChecked(*TestRunner, ScriptActor, TEXT("ObjectCallbackCount"), ObjectCallbackCount)
			|| !ReadIntPropertyChecked(*TestRunner, ScriptActor, TEXT("ObjectUserData"), ObjectUserData)
			|| !ReadIntPropertyChecked(*TestRunner, ScriptActor, TEXT("ObjectHitCount"), ObjectHitCount)
			|| !ReadIntPropertyChecked(*TestRunner, ScriptActor, TEXT("ObjectQuerySucceeded"), ObjectQuerySucceeded)
			|| !ReadIntPropertyChecked(*TestRunner, ScriptActor, TEXT("ObjectQueryHitCount"), ObjectQueryHitCount)
			|| !ReadIntPropertyChecked(*TestRunner, ScriptActor, TEXT("ObjectHandleValidInitially"), ObjectHandleValidInitially)
			|| !ReadIntPropertyChecked(*TestRunner, ScriptActor, TEXT("ProfileCallbackCount"), ProfileCallbackCount)
			|| !ReadIntPropertyChecked(*TestRunner, ScriptActor, TEXT("ProfileUserData"), ProfileUserData)
			|| !ReadIntPropertyChecked(*TestRunner, ScriptActor, TEXT("ProfileHitCount"), ProfileHitCount)
			|| !ReadIntPropertyChecked(*TestRunner, ScriptActor, TEXT("ProfileQuerySucceeded"), ProfileQuerySucceeded)
			|| !ReadIntPropertyChecked(*TestRunner, ScriptActor, TEXT("ProfileQueryHitCount"), ProfileQueryHitCount)
			|| !ReadIntPropertyChecked(*TestRunner, ScriptActor, TEXT("ProfileHandleValidInitially"), ProfileHandleValidInitially)
			|| !ReadUInt64PropertyChecked(*TestRunner, ScriptActor, TEXT("ChannelHandleRaw"), ChannelHandleRaw)
			|| !ReadUInt64PropertyChecked(*TestRunner, ScriptActor, TEXT("LastChannelCallbackHandle"), LastChannelCallbackHandle)
			|| !ReadUInt64PropertyChecked(*TestRunner, ScriptActor, TEXT("ObjectHandleRaw"), ObjectHandleRaw)
			|| !ReadUInt64PropertyChecked(*TestRunner, ScriptActor, TEXT("LastObjectCallbackHandle"), LastObjectCallbackHandle)
			|| !ReadUInt64PropertyChecked(*TestRunner, ScriptActor, TEXT("ProfileHandleRaw"), ProfileHandleRaw)
			|| !ReadUInt64PropertyChecked(*TestRunner, ScriptActor, TEXT("LastProfileCallbackHandle"), LastProfileCallbackHandle)
			|| !ReadBoolPropertyChecked(*TestRunner, ScriptActor, TEXT("bChannelHitActorMatched"), bChannelHitActorMatched)
			|| !ReadBoolPropertyChecked(*TestRunner, ScriptActor, TEXT("bChannelHitComponentMatched"), bChannelHitComponentMatched)
			|| !ReadBoolPropertyChecked(*TestRunner, ScriptActor, TEXT("bObjectHitActorMatched"), bObjectHitActorMatched)
			|| !ReadBoolPropertyChecked(*TestRunner, ScriptActor, TEXT("bObjectHitComponentMatched"), bObjectHitComponentMatched)
			|| !ReadBoolPropertyChecked(*TestRunner, ScriptActor, TEXT("bProfileHitActorMatched"), bProfileHitActorMatched)
			|| !ReadBoolPropertyChecked(*TestRunner, ScriptActor, TEXT("bProfileHitComponentMatched"), bProfileHitComponentMatched))
		{
			return;
		}

		// Verify callback invocation counts
		ASSERT_THAT(AreEqual(1, ChannelCallbackCount, TEXT("AsyncSweepByChannel should invoke its callback exactly once")));
		ASSERT_THAT(AreEqual(1, ObjectCallbackCount, TEXT("AsyncSweepByObjectType should invoke its callback exactly once")));
		ASSERT_THAT(AreEqual(1, ProfileCallbackCount, TEXT("AsyncSweepByProfile should invoke its callback exactly once")));

		// Verify UserData preservation
		ASSERT_THAT(AreEqual(101, ChannelUserData, TEXT("AsyncSweepByChannel should preserve UserData through the delegate bridge")));
		ASSERT_THAT(AreEqual(202, ObjectUserData, TEXT("AsyncSweepByObjectType should preserve UserData through the delegate bridge")));
		ASSERT_THAT(AreEqual(303, ProfileUserData, TEXT("AsyncSweepByProfile should preserve UserData through the delegate bridge")));

		// Verify hit counts
		ASSERT_THAT(IsTrue(ChannelHitCount > 0, TEXT("AsyncSweepByChannel should report at least one hit")));
		ASSERT_THAT(IsTrue(ObjectHitCount > 0, TEXT("AsyncSweepByObjectType should report at least one hit")));
		ASSERT_THAT(IsTrue(ProfileHitCount > 0, TEXT("AsyncSweepByProfile should report at least one hit")));

		// Verify handle validity
		ASSERT_THAT(AreEqual(1, ChannelHandleValidInitially, TEXT("AsyncSweepByChannel should return an initially valid trace handle")));
		ASSERT_THAT(AreEqual(1, ObjectHandleValidInitially, TEXT("AsyncSweepByObjectType should return an initially valid trace handle")));
		ASSERT_THAT(AreEqual(1, ProfileHandleValidInitially, TEXT("AsyncSweepByProfile should return an initially valid trace handle")));

		// Verify handle matching
		ASSERT_THAT(AreEqual(ChannelHandleRaw, LastChannelCallbackHandle, TEXT("AsyncSweepByChannel callback should observe the same handle that StartAsyncSweeps stored")));
		ASSERT_THAT(AreEqual(ObjectHandleRaw, LastObjectCallbackHandle, TEXT("AsyncSweepByObjectType callback should observe the same handle that StartAsyncSweeps stored")));
		ASSERT_THAT(AreEqual(ProfileHandleRaw, LastProfileCallbackHandle, TEXT("AsyncSweepByProfile callback should observe the same handle that StartAsyncSweeps stored")));

		// Verify QueryTraceData
		ASSERT_THAT(AreEqual(1, ChannelQuerySucceeded, TEXT("AsyncSweepByChannel callback should report successful QueryTraceData")));
		ASSERT_THAT(AreEqual(1, ObjectQuerySucceeded, TEXT("AsyncSweepByObjectType callback should report successful QueryTraceData")));
		ASSERT_THAT(AreEqual(1, ProfileQuerySucceeded, TEXT("AsyncSweepByProfile callback should report successful QueryTraceData")));
		ASSERT_THAT(AreEqual(ChannelHitCount, ChannelQueryHitCount, TEXT("AsyncSweepByChannel query hit count should match callback payload")));
		ASSERT_THAT(AreEqual(ObjectHitCount, ObjectQueryHitCount, TEXT("AsyncSweepByObjectType query hit count should match callback payload")));
		ASSERT_THAT(AreEqual(ProfileHitCount, ProfileQueryHitCount, TEXT("AsyncSweepByProfile query hit count should match callback payload")));

		// Verify actor/component identification
		ASSERT_THAT(IsTrue(bChannelHitActorMatched, TEXT("AsyncSweepByChannel should identify the expected blocker actor")));
		ASSERT_THAT(IsTrue(bChannelHitComponentMatched, TEXT("AsyncSweepByChannel should identify the expected blocker component")));
		ASSERT_THAT(IsTrue(bObjectHitActorMatched, TEXT("AsyncSweepByObjectType should identify the expected blocker actor")));
		ASSERT_THAT(IsTrue(bObjectHitComponentMatched, TEXT("AsyncSweepByObjectType should identify the expected blocker component")));
		ASSERT_THAT(IsTrue(bProfileHitActorMatched, TEXT("AsyncSweepByProfile should identify the expected blocker actor")));
		ASSERT_THAT(IsTrue(bProfileHitComponentMatched, TEXT("AsyncSweepByProfile should identify the expected blocker component")));
	}
};

#endif
