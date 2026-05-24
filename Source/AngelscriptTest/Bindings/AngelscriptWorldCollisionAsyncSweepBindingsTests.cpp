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
#include "Shared/AngelscriptTestMacros.h"
#include "Shared/AngelscriptBindingsModuleBuilder.h"
#include "Shared/AngelscriptBindingsAssertions.h"
#include "Shared/AngelscriptFunctionalTestUtils.h"

#include "Components/ActorTestSpawner.h"
#include "Components/BoxComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Misc/ScopeExit.h"
#include "UObject/UnrealType.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptTestSupport;
using namespace AngelscriptTestBindings;
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
		UFunction* Function = FindGeneratedFunction(OwnerClass, FunctionName);
		if (!Test.TestNotNull(
				*FString::Printf(TEXT("World collision async sweep method '%s' should exist"), *FunctionName.ToString()),
				Function))
		{
			return false;
		}

		return Test.TestTrue(
			*FString::Printf(TEXT("World collision async sweep method '%s' should execute"), *FunctionName.ToString()),
			ExecuteGeneratedIntEventOnGameThread(Object, Function, OutResult));
	}

	template<typename ValueType>
	bool WriteObjectPropertyChecked(
		FAutomationTestBase& Test,
		UObject* Object,
		FName PropertyName,
		ValueType* Value)
	{
		if (!Test.TestNotNull(TEXT("World collision async sweep object should be valid for reflected writes"), Object))
		{
			return false;
		}

		FObjectProperty* Property = FindFProperty<FObjectProperty>(Object->GetClass(), PropertyName);
		if (!Test.TestNotNull(
				*FString::Printf(TEXT("World collision async sweep property '%s' should exist"), *PropertyName.ToString()),
				Property))
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
		if (!Test.TestNotNull(TEXT("World collision async sweep object should be valid for uint64 property reads"), Object))
		{
			return false;
		}

		FUInt64Property* Property = FindFProperty<FUInt64Property>(Object->GetClass(), PropertyName);
		if (!Test.TestNotNull(
				*FString::Printf(TEXT("World collision async sweep property '%s' should exist"), *PropertyName.ToString()),
				Property))
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
		FIntProperty* ChannelCallbackCountProperty = FindFProperty<FIntProperty>(ScriptActor.GetClass(), TEXT("ChannelCallbackCount"));
		FIntProperty* ObjectCallbackCountProperty = FindFProperty<FIntProperty>(ScriptActor.GetClass(), TEXT("ObjectCallbackCount"));
		FIntProperty* ProfileCallbackCountProperty = FindFProperty<FIntProperty>(ScriptActor.GetClass(), TEXT("ProfileCallbackCount"));
		if (!Test.TestNotNull(TEXT("Async sweep actor should expose ChannelCallbackCount"), ChannelCallbackCountProperty)
			|| !Test.TestNotNull(TEXT("Async sweep actor should expose ObjectCallbackCount"), ObjectCallbackCountProperty)
			|| !Test.TestNotNull(TEXT("Async sweep actor should expose ProfileCallbackCount"), ProfileCallbackCountProperty))
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
		if (!TestRunner->TestNotNull(TEXT("Async sweep blocking box should be created"), BlockingBox))
		{
			return;
		}

		AActor* ScriptActor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		if (!TestRunner->TestNotNull(TEXT("Async sweep script actor should spawn"), ScriptActor))
		{
			return;
		}

		if (!WriteObjectPropertyChecked(*TestRunner, ScriptActor, TEXT("ExpectedActor"), &BlockingActor)
			|| !WriteObjectPropertyChecked(*TestRunner, ScriptActor, TEXT("ExpectedComponent"), BlockingBox))
		{
			return;
		}

		BeginPlayActor(Engine, *ScriptActor);

		UWorld* World = BlockingActor.GetWorld();
		if (!TestRunner->TestNotNull(TEXT("Async sweep test should access the spawned world"), World))
		{
			return;
		}

		int32 StartResult = 0;
		if (!ExecuteGeneratedIntMethod(*TestRunner, ScriptActor, ScriptClass, TEXT("StartAsyncSweeps"), StartResult))
		{
			return;
		}
		TestRunner->TestEqual(TEXT("Async sweep start method should acknowledge launch"), StartResult, 1);

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
		TestRunner->TestEqual(TEXT("AsyncSweepByChannel should invoke its callback exactly once"), ChannelCallbackCount, 1);
		TestRunner->TestEqual(TEXT("AsyncSweepByObjectType should invoke its callback exactly once"), ObjectCallbackCount, 1);
		TestRunner->TestEqual(TEXT("AsyncSweepByProfile should invoke its callback exactly once"), ProfileCallbackCount, 1);

		// Verify UserData preservation
		TestRunner->TestEqual(TEXT("AsyncSweepByChannel should preserve UserData through the delegate bridge"), ChannelUserData, 101);
		TestRunner->TestEqual(TEXT("AsyncSweepByObjectType should preserve UserData through the delegate bridge"), ObjectUserData, 202);
		TestRunner->TestEqual(TEXT("AsyncSweepByProfile should preserve UserData through the delegate bridge"), ProfileUserData, 303);

		// Verify hit counts
		TestRunner->TestTrue(TEXT("AsyncSweepByChannel should report at least one hit"), ChannelHitCount > 0);
		TestRunner->TestTrue(TEXT("AsyncSweepByObjectType should report at least one hit"), ObjectHitCount > 0);
		TestRunner->TestTrue(TEXT("AsyncSweepByProfile should report at least one hit"), ProfileHitCount > 0);

		// Verify handle validity
		TestRunner->TestEqual(TEXT("AsyncSweepByChannel should return an initially valid trace handle"), ChannelHandleValidInitially, 1);
		TestRunner->TestEqual(TEXT("AsyncSweepByObjectType should return an initially valid trace handle"), ObjectHandleValidInitially, 1);
		TestRunner->TestEqual(TEXT("AsyncSweepByProfile should return an initially valid trace handle"), ProfileHandleValidInitially, 1);

		// Verify handle matching
		TestRunner->TestEqual(TEXT("AsyncSweepByChannel callback should observe the same handle that StartAsyncSweeps stored"), LastChannelCallbackHandle, ChannelHandleRaw);
		TestRunner->TestEqual(TEXT("AsyncSweepByObjectType callback should observe the same handle that StartAsyncSweeps stored"), LastObjectCallbackHandle, ObjectHandleRaw);
		TestRunner->TestEqual(TEXT("AsyncSweepByProfile callback should observe the same handle that StartAsyncSweeps stored"), LastProfileCallbackHandle, ProfileHandleRaw);

		// Verify QueryTraceData
		TestRunner->TestEqual(TEXT("AsyncSweepByChannel callback should report successful QueryTraceData"), ChannelQuerySucceeded, 1);
		TestRunner->TestEqual(TEXT("AsyncSweepByObjectType callback should report successful QueryTraceData"), ObjectQuerySucceeded, 1);
		TestRunner->TestEqual(TEXT("AsyncSweepByProfile callback should report successful QueryTraceData"), ProfileQuerySucceeded, 1);
		TestRunner->TestEqual(TEXT("AsyncSweepByChannel query hit count should match callback payload"), ChannelQueryHitCount, ChannelHitCount);
		TestRunner->TestEqual(TEXT("AsyncSweepByObjectType query hit count should match callback payload"), ObjectQueryHitCount, ObjectHitCount);
		TestRunner->TestEqual(TEXT("AsyncSweepByProfile query hit count should match callback payload"), ProfileQueryHitCount, ProfileHitCount);

		// Verify actor/component identification
		TestRunner->TestTrue(TEXT("AsyncSweepByChannel should identify the expected blocker actor"), bChannelHitActorMatched);
		TestRunner->TestTrue(TEXT("AsyncSweepByChannel should identify the expected blocker component"), bChannelHitComponentMatched);
		TestRunner->TestTrue(TEXT("AsyncSweepByObjectType should identify the expected blocker actor"), bObjectHitActorMatched);
		TestRunner->TestTrue(TEXT("AsyncSweepByObjectType should identify the expected blocker component"), bObjectHitComponentMatched);
		TestRunner->TestTrue(TEXT("AsyncSweepByProfile should identify the expected blocker actor"), bProfileHitActorMatched);
		TestRunner->TestTrue(TEXT("AsyncSweepByProfile should identify the expected blocker component"), bProfileHitComponentMatched);
	}
};

#endif
