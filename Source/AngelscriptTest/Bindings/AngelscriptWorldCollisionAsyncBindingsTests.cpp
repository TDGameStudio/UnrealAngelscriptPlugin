// ============================================================================
// AngelscriptWorldCollisionAsyncBindingsTests.cpp
//
// World collision async binding coverage �?CQTest refactor. Automation IDs:
//   Angelscript.TestModule.Bindings.WorldCollisionAsync.FAngelscriptWorldCollisionAsyncBindingsTest.*
//
// Sections:
//   AsyncTraceCallbacks �?async line trace and overlap with delegate callbacks
//
// CQTest adaptation notes:
//   Single legacy automation test converted to TEST_CLASS.
//   This test uses ASTEST_CREATE_ENGINE_FULL (world-based) and a spawned script
//   actor, so it does not use the standard FScopedAngelscriptModule pattern.
//   The original structure is preserved with property-read assertions.
// ============================================================================

#include "CQTest.h"
#include "AngelscriptFunctionalTestUtils.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestModuleScope.h"
#include "AngelscriptTestExecute.h"

#include "Components/ActorTestSpawner.h"
#include "Components/BoxComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Misc/ScopeExit.h"

#if WITH_DEV_AUTOMATION_TESTS


// ----------------------------------------------------------------------------
// Helper utilities (retained from original)
// ----------------------------------------------------------------------------


// ----------------------------------------------------------------------------
// Profile
// ----------------------------------------------------------------------------


// ----------------------------------------------------------------------------
// Test class
// ----------------------------------------------------------------------------

TEST_CLASS_WITH_FLAGS(FAngelscriptWorldCollisionAsyncBindingsTest,
	"Angelscript.TestModule.Bindings.WorldCollisionAsync",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
inline static const FName WorldCollisionAsyncModuleName = FName(TEXT("ASWorldCollisionAsyncTraceCallbacks"));
inline static const FString WorldCollisionAsyncFilename = FString(TEXT("WorldCollisionAsyncTraceCallbacks.as"));
inline static const FName WorldCollisionAsyncClassName = FName(TEXT("ATestWorldCollisionAsyncCallbacks"));
inline static const FVector AsyncCollisionTargetLocation = FVector(0.0f, 0.0f, 0.0f);
inline static const FVector AsyncLineTraceStart = FVector(-200.0f, 0.0f, 0.0f);
inline static const FVector AsyncLineTraceEnd = FVector(200.0f, 0.0f, 0.0f);
inline static const FVector AsyncTargetExtent = FVector(50.0f, 50.0f, 50.0f);
inline static const FVector AsyncQueryExtent = FVector(30.0f, 30.0f, 30.0f);
static constexpr float AsyncTickDeltaTime = 1.0f / 60.0f;
static constexpr int32 AsyncMaxTickCount = 90;

static UBoxComponent* AddCollisionBox(
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

static bool ExecuteGeneratedIntMethod(
	FAutomationTestBase& Test,
	UObject* Object,
	UClass* OwnerClass,
	FName FunctionName,
	int32& OutResult)
{
	FNoDiscardAsserter LocalAssert(Test);
	UFunction* Function = FindGeneratedFunction(OwnerClass, FunctionName);
	if (!LocalAssert.IsNotNull(
		Function,
		*FString::Printf(TEXT("World collision async method '%s' should exist"), *FunctionName.ToString())))
	{
		return false;
	}

	return LocalAssert.IsTrue(
		ExecuteGeneratedIntEventOnGameThread(Object, Function, OutResult),
		*FString::Printf(TEXT("World collision async method '%s' should execute"), *FunctionName.ToString()));
}

static bool ReadUInt64PropertyChecked(
	FAutomationTestBase& Test,
	UObject* Object,
	FName PropertyName,
	uint64& OutValue)
{
	FNoDiscardAsserter LocalAssert(Test);
	if (!LocalAssert.IsNotNull(Object, TEXT("World collision async object should be valid for uint64 property reads")))
	{
		return false;
	}

	FUInt64Property* Property = FindFProperty<FUInt64Property>(Object->GetClass(), PropertyName);
	if (!LocalAssert.IsNotNull(
		Property,
		*FString::Printf(TEXT("World collision async property '%s' should exist"), *PropertyName.ToString())))
	{
		return false;
	}

	OutValue = Property->GetPropertyValue_InContainer(Object);
	return true;
}

static bool WaitForAsyncCallbacks(
	FAutomationTestBase& Test,
	FAngelscriptEngine& Engine,
	UWorld& World,
	AActor& ScriptActor)
{
	FNoDiscardAsserter LocalAssert(Test);
	FIntProperty* LineCallbackCountProperty = FindFProperty<FIntProperty>(ScriptActor.GetClass(), TEXT("LineCallbackCount"));
	FIntProperty* OverlapCallbackCountProperty = FindFProperty<FIntProperty>(ScriptActor.GetClass(), TEXT("OverlapCallbackCount"));
	bool bHasCallbackProperties = true;
	bHasCallbackProperties &= LocalAssert.IsNotNull(LineCallbackCountProperty, TEXT("World collision async actor should expose LineCallbackCount"));
	bHasCallbackProperties &= LocalAssert.IsNotNull(OverlapCallbackCountProperty, TEXT("World collision async actor should expose OverlapCallbackCount"));
	if (!bHasCallbackProperties)
	{
		return false;
	}

	for (int32 TickIndex = 0; TickIndex < AsyncMaxTickCount; ++TickIndex)
	{
		const int32 LineCallbackCount = LineCallbackCountProperty->GetPropertyValue_InContainer(&ScriptActor);
		const int32 OverlapCallbackCount = OverlapCallbackCountProperty->GetPropertyValue_InContainer(&ScriptActor);
		if (LineCallbackCount >= 1 && OverlapCallbackCount >= 1)
		{
			return true;
		}

		AngelscriptFunctionalTestUtils::TickWorld(Engine, World, AsyncTickDeltaTime, 1);
	}

	const int32 FinalLineCallbackCount = LineCallbackCountProperty->GetPropertyValue_InContainer(&ScriptActor);
	const int32 FinalOverlapCallbackCount = OverlapCallbackCountProperty->GetPropertyValue_InContainer(&ScriptActor);
	Test.AddError(FString::Printf(
		TEXT("Async world-collision callbacks did not complete within %d ticks (line=%d overlap=%d)."),
		AsyncMaxTickCount,
		FinalLineCallbackCount,
		FinalOverlapCallbackCount));
	return false;
}

public:
	// ====================================================================
	// Section: AsyncTraceCallbacks
	// ====================================================================

	TEST_METHOD(AsyncTraceCallbacks)
	{
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
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*WorldCollisionAsyncModuleName.ToString());
		};

		UClass* ScriptClass = AngelscriptFunctionalTestUtils::CompileScriptModule(
			*TestRunner,
			Engine,
			WorldCollisionAsyncModuleName,
			WorldCollisionAsyncFilename,
			TEXT(R"AS(
UCLASS()
class ATestWorldCollisionAsyncCallbacks : AActor
{
	UPROPERTY()
	int LineCallbackCount = 0;
	UPROPERTY()
	int LineUserData = 0;
	UPROPERTY()
	int LineHitCount = 0;
	UPROPERTY()
	int LineQuerySucceeded = 0;
	UPROPERTY()
	int LineQueryHitCount = 0;
	UPROPERTY()
	int LineHandleValidInitially = 0;
	UPROPERTY()
	uint64 LineHandleRaw = 0;
	UPROPERTY()
	uint64 LastLineCallbackHandle = 0;

	UPROPERTY()
	int OverlapCallbackCount = 0;
	UPROPERTY()
	int OverlapUserData = 0;
	UPROPERTY()
	int OverlapHitCount = 0;
	UPROPERTY()
	int OverlapQuerySucceeded = 0;
	UPROPERTY()
	int OverlapQueryHitCount = 0;
	UPROPERTY()
	int OverlapHandleValidInitially = 0;
	UPROPERTY()
	uint64 OverlapHandleRaw = 0;
	UPROPERTY()
	uint64 LastOverlapCallbackHandle = 0;

	FTraceHandle LineHandle;
	FTraceHandle OverlapHandle;

	UFUNCTION()
	int StartAsyncQueries()
	{
		FScriptTraceDelegate LineDelegate;
		LineDelegate.BindUFunction(this, n"HandleLineTrace");
		LineHandle = System::AsyncLineTraceByChannel(
			EAsyncTraceType::Single,
			FVector(-200.0f, 0.0f, 0.0f),
			FVector(200.0f, 0.0f, 0.0f),
			ECollisionChannel::ECC_Visibility,
			FCollisionQueryParams::DefaultQueryParam,
			FCollisionResponseParams::DefaultResponseParam,
			LineDelegate,
			77);
		LineHandleRaw = LineHandle._Handle;
		LineHandleValidInitially = System::IsTraceHandleValid(LineHandle, false) ? 1 : 0;

		FCollisionShape Shape = FCollisionShape::MakeBox(FVector(30.0f, 30.0f, 30.0f));
		FScriptOverlapDelegate OverlapDelegate;
		OverlapDelegate.BindUFunction(this, n"HandleOverlapTrace");
		OverlapHandle = System::AsyncOverlapByChannel(
			FVector::ZeroVector,
			FQuat::Identity,
			ECollisionChannel::ECC_Visibility,
			Shape,
			FCollisionQueryParams::DefaultQueryParam,
			FCollisionResponseParams::DefaultResponseParam,
			OverlapDelegate,
			88);
		OverlapHandleRaw = OverlapHandle._Handle;
		OverlapHandleValidInitially = System::IsTraceHandleValid(OverlapHandle, true) ? 1 : 0;
		return 1;
	}

	UFUNCTION()
	void HandleLineTrace(uint64 TraceHandleValue, const TArray<FHitResult>& OutHits, uint32 UserData)
	{
		LineCallbackCount += 1;
		LastLineCallbackHandle = TraceHandleValue;
		LineUserData = int(UserData);
		LineHitCount = OutHits.Num();

		FTraceDatum Datum;
		LineQuerySucceeded = System::QueryTraceData(LineHandle, Datum) ? 1 : 0;
		LineQueryHitCount = Datum.OutHits.Num();
	}

	UFUNCTION()
	void HandleOverlapTrace(uint64 TraceHandleValue, const TArray<FOverlapResult>& OutOverlaps, uint32 UserData)
	{
		OverlapCallbackCount += 1;
		LastOverlapCallbackHandle = TraceHandleValue;
		OverlapUserData = int(UserData);
		OverlapHitCount = OutOverlaps.Num();

		FOverlapDatum Datum;
		OverlapQuerySucceeded = System::QueryOverlapData(OverlapHandle, Datum) ? 1 : 0;
		OverlapQueryHitCount = Datum.OutOverlaps.Num();
	}

	UFUNCTION()
	int GetLineHandleValidNow()
	{
		return System::IsTraceHandleValid(LineHandle, false) ? 1 : 0;
	}

	UFUNCTION()
	int GetOverlapHandleValidNow()
	{
		return System::IsTraceHandleValid(OverlapHandle, true) ? 1 : 0;
	}
}
)AS"),
			WorldCollisionAsyncClassName);
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();

		AActor& TargetActor = Spawner.SpawnActor<AActor>();
		UBoxComponent* TargetBox = AddCollisionBox(TargetActor, TEXT("AsyncCollisionTarget"), AsyncTargetExtent, AsyncCollisionTargetLocation);
		ASSERT_THAT(IsNotNull(TargetBox, TEXT("World collision async target box should be created")));

		AActor* ScriptActor = AngelscriptFunctionalTestUtils::SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(ScriptActor, TEXT("World collision async script actor should spawn")));

		AngelscriptFunctionalTestUtils::BeginPlayActor(Engine, *ScriptActor);

		UWorld* World = TargetActor.GetWorld();
		ASSERT_THAT(IsNotNull(World, TEXT("World collision async test should access the spawned test world")));

		int32 StartResult = 0;
		if (!ExecuteGeneratedIntMethod(*TestRunner, ScriptActor, ScriptClass, TEXT("StartAsyncQueries"), StartResult))
		{
			return;
		}
		ASSERT_THAT(AreEqual(1, StartResult, TEXT("Async world collision start method should acknowledge launch")));

		if (!WaitForAsyncCallbacks(*TestRunner, Engine, *World, *ScriptActor))
		{
			return;
		}

		int32 LineCallbackCount = 0;
		int32 LineUserData = 0;
		int32 LineHitCount = 0;
		int32 LineQuerySucceeded = 0;
		int32 LineQueryHitCount = 0;
		int32 LineHandleValidInitially = 0;
		int32 OverlapCallbackCount = 0;
		int32 OverlapUserData = 0;
		int32 OverlapHitCount = 0;
		int32 OverlapQuerySucceeded = 0;
		int32 OverlapQueryHitCount = 0;
		int32 OverlapHandleValidInitially = 0;
		uint64 LineHandleRaw = 0;
		uint64 LastLineCallbackHandle = 0;
		uint64 OverlapHandleRaw = 0;
		uint64 LastOverlapCallbackHandle = 0;
		if (!AngelscriptFunctionalTestUtils::ReadIntPropertyChecked(*TestRunner, ScriptActor, TEXT("LineCallbackCount"), LineCallbackCount)
			|| !AngelscriptFunctionalTestUtils::ReadIntPropertyChecked(*TestRunner, ScriptActor, TEXT("LineUserData"), LineUserData)
			|| !AngelscriptFunctionalTestUtils::ReadIntPropertyChecked(*TestRunner, ScriptActor, TEXT("LineHitCount"), LineHitCount)
			|| !AngelscriptFunctionalTestUtils::ReadIntPropertyChecked(*TestRunner, ScriptActor, TEXT("LineQuerySucceeded"), LineQuerySucceeded)
			|| !AngelscriptFunctionalTestUtils::ReadIntPropertyChecked(*TestRunner, ScriptActor, TEXT("LineQueryHitCount"), LineQueryHitCount)
			|| !AngelscriptFunctionalTestUtils::ReadIntPropertyChecked(*TestRunner, ScriptActor, TEXT("LineHandleValidInitially"), LineHandleValidInitially)
			|| !AngelscriptFunctionalTestUtils::ReadIntPropertyChecked(*TestRunner, ScriptActor, TEXT("OverlapCallbackCount"), OverlapCallbackCount)
			|| !AngelscriptFunctionalTestUtils::ReadIntPropertyChecked(*TestRunner, ScriptActor, TEXT("OverlapUserData"), OverlapUserData)
			|| !AngelscriptFunctionalTestUtils::ReadIntPropertyChecked(*TestRunner, ScriptActor, TEXT("OverlapHitCount"), OverlapHitCount)
			|| !AngelscriptFunctionalTestUtils::ReadIntPropertyChecked(*TestRunner, ScriptActor, TEXT("OverlapQuerySucceeded"), OverlapQuerySucceeded)
			|| !AngelscriptFunctionalTestUtils::ReadIntPropertyChecked(*TestRunner, ScriptActor, TEXT("OverlapQueryHitCount"), OverlapQueryHitCount)
			|| !AngelscriptFunctionalTestUtils::ReadIntPropertyChecked(*TestRunner, ScriptActor, TEXT("OverlapHandleValidInitially"), OverlapHandleValidInitially)
			|| !ReadUInt64PropertyChecked(*TestRunner, ScriptActor, TEXT("LineHandleRaw"), LineHandleRaw)
			|| !ReadUInt64PropertyChecked(*TestRunner, ScriptActor, TEXT("LastLineCallbackHandle"), LastLineCallbackHandle)
			|| !ReadUInt64PropertyChecked(*TestRunner, ScriptActor, TEXT("OverlapHandleRaw"), OverlapHandleRaw)
			|| !ReadUInt64PropertyChecked(*TestRunner, ScriptActor, TEXT("LastOverlapCallbackHandle"), LastOverlapCallbackHandle))
		{
			return;
		}

		ASSERT_THAT(AreEqual(1, LineCallbackCount, TEXT("Async line trace should invoke callback exactly once")));
		ASSERT_THAT(AreEqual(1, OverlapCallbackCount, TEXT("Async overlap should invoke callback exactly once")));
		ASSERT_THAT(AreEqual(77, LineUserData, TEXT("Async line trace should preserve UserData")));
		ASSERT_THAT(AreEqual(88, OverlapUserData, TEXT("Async overlap should preserve UserData")));
		ASSERT_THAT(IsTrue(LineHitCount > 0, TEXT("Async line trace should report at least one hit")));
		ASSERT_THAT(IsTrue(OverlapHitCount > 0, TEXT("Async overlap should report at least one overlap")));
		ASSERT_THAT(AreEqual(1, LineHandleValidInitially, TEXT("Async line trace handle should be initially valid")));
		ASSERT_THAT(AreEqual(1, OverlapHandleValidInitially, TEXT("Async overlap handle should be initially valid")));
		ASSERT_THAT(AreEqual(LineHandleRaw, LastLineCallbackHandle, TEXT("Line callback handle should match stored handle")));
		ASSERT_THAT(AreEqual(OverlapHandleRaw, LastOverlapCallbackHandle, TEXT("Overlap callback handle should match stored handle")));
		ASSERT_THAT(AreEqual(1, LineQuerySucceeded, TEXT("Line trace QueryTraceData should succeed")));
		ASSERT_THAT(AreEqual(1, OverlapQuerySucceeded, TEXT("Overlap QueryOverlapData should succeed")));
		ASSERT_THAT(IsTrue(LineQueryHitCount > 0, TEXT("Line trace queried hits should be non-empty")));
		ASSERT_THAT(IsTrue(OverlapQueryHitCount > 0, TEXT("Overlap queried hits should be non-empty")));
		ASSERT_THAT(AreEqual(LineHitCount, LineQueryHitCount, TEXT("Line trace query count should match callback payload")));
		ASSERT_THAT(AreEqual(OverlapHitCount, OverlapQueryHitCount, TEXT("Overlap query count should match callback payload")));

		FTraceHandle NativeLineHandle;
		NativeLineHandle._Handle = LineHandleRaw;
		FTraceHandle NativeOverlapHandle;
		NativeOverlapHandle._Handle = OverlapHandleRaw;

		int32 ScriptLineHandleValidNow = 0;
		int32 ScriptOverlapHandleValidNow = 0;
		if (!ExecuteGeneratedIntMethod(*TestRunner, ScriptActor, ScriptClass, TEXT("GetLineHandleValidNow"), ScriptLineHandleValidNow)
			|| !ExecuteGeneratedIntMethod(*TestRunner, ScriptActor, ScriptClass, TEXT("GetOverlapHandleValidNow"), ScriptOverlapHandleValidNow))
		{
			return;
		}

		const bool bNativeLineHandleValidNow = World->IsTraceHandleValid(NativeLineHandle, false);
		const bool bNativeOverlapHandleValidNow = World->IsTraceHandleValid(NativeOverlapHandle, true);
		ASSERT_THAT(AreEqual(bNativeLineHandleValidNow ? 1 : 0, ScriptLineHandleValidNow, TEXT("Script line handle validity should match native after completion")));
		ASSERT_THAT(AreEqual(bNativeOverlapHandleValidNow ? 1 : 0, ScriptOverlapHandleValidNow, TEXT("Script overlap handle validity should match native after completion")));

		}
	}
};

#endif
