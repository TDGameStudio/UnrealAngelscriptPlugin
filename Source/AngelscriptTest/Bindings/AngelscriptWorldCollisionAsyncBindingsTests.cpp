// ============================================================================
// AngelscriptWorldCollisionAsyncBindingsTests.cpp
//
// World collision async trace/overlap binding contract smoke. Callback payload
// matrices and query-data behavior belong in Coverage (`13-physics-collision`).
// ============================================================================

#include "CQTest.h"
#include "AngelscriptFunctionalTestUtils.h"
#include "AngelscriptTestMacros.h"

#include "Components/ActorTestSpawner.h"
#include "Components/BoxComponent.h"
#include "GameFramework/Actor.h"
#include "Misc/ScopeExit.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FAngelscriptWorldCollisionAsyncBindingsTest,
	"Angelscript.TestModule.Bindings.WorldCollisionAsync",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	inline static const FName AsyncModuleName = FName(TEXT("ASWorldCollisionAsyncEntrypointSmoke"));
	inline static const FString AsyncFilename = FString(TEXT("WorldCollisionAsyncEntrypointSmoke.as"));
	inline static const FName AsyncClassName = FName(TEXT("ATestWorldCollisionAsyncEntrypointSmoke"));
	inline static const FVector TargetLocation = FVector(0.0f, 0.0f, 0.0f);
	inline static const FVector TargetExtent = FVector(50.0f, 50.0f, 50.0f);
	static constexpr float TickDeltaTime = 1.0f / 60.0f;
	static constexpr int32 MaxTickCount = 90;

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

		for (int32 TickIndex = 0; TickIndex < MaxTickCount; ++TickIndex)
		{
			const int32 LineCallbackCount = LineCallbackCountProperty->GetPropertyValue_InContainer(&ScriptActor);
			const int32 OverlapCallbackCount = OverlapCallbackCountProperty->GetPropertyValue_InContainer(&ScriptActor);
			if (LineCallbackCount >= 1 && OverlapCallbackCount >= 1)
			{
				return true;
			}

			AngelscriptFunctionalTestUtils::TickWorld(Engine, World, TickDeltaTime, 1);
		}

		Test.AddError(FString::Printf(
			TEXT("Async world-collision contract callbacks did not arrive within %d ticks."),
			MaxTickCount));
		return false;
	}

public:
	TEST_METHOD(AsyncTraceRegistrationSmoke)
	{
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE_FULL();
		FAngelscriptEngineScope Scope(Engine);
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*AsyncModuleName.ToString());
		};

		UClass* ScriptClass = AngelscriptFunctionalTestUtils::CompileScriptModule(
			*TestRunner,
			Engine,
			AsyncModuleName,
			AsyncFilename,
			ASTEST_AS(R"AS(
				UCLASS()
				class ATestWorldCollisionAsyncEntrypointSmoke : AActor
				{
					UPROPERTY()
					int LineCallbackCount = 0;

					UPROPERTY()
					int OverlapCallbackCount = 0;

					UPROPERTY()
					int LineHandleValidInitially = 0;

					UPROPERTY()
					int OverlapHandleValidInitially = 0;

					FTraceHandle LineHandle;
					FTraceHandle OverlapHandle;

					UFUNCTION()
					int VerifyTraceValueSurface()
					{
						FTraceHandle DefaultHandle;
						FTraceHandle ExplicitHandle(42);
						FTraceHandle EqualHandle(42);
						bool bHandlesInitiallyEqual = ExplicitHandle == EqualHandle;
						uint64 InitialHandleValue = ExplicitHandle._Handle;
						ExplicitHandle._FrameNumber = 7;
						ExplicitHandle._Index = 9;

						FTraceDatum TraceDatum;
						TraceDatum.Start = FVector(1.0f, 2.0f, 3.0f);
						TraceDatum.End = FVector(4.0f, 5.0f, 6.0f);
						TraceDatum.Rot = FQuat::Identity;
						TraceDatum.TraceType = EAsyncTraceType::Multi;
						TraceDatum.TraceChannel = ECollisionChannel::ECC_Visibility;
						TraceDatum.UserData = 77;

						FOverlapDatum OverlapDatum;
						OverlapDatum.Pos = FVector(7.0f, 8.0f, 9.0f);
						OverlapDatum.Rot = FQuat::Identity;
						OverlapDatum.TraceChannel = ECollisionChannel::ECC_WorldDynamic;
						OverlapDatum.UserData = 88;

						return !DefaultHandle.IsValid()
							&& DefaultHandle._Handle == 0
							&& ExplicitHandle.IsValid()
							&& InitialHandleValue == 42
							&& bHandlesInitiallyEqual
							&& !(ExplicitHandle == EqualHandle)
							&& ExplicitHandle._FrameNumber == 7
							&& ExplicitHandle._Index == 9
							&& TraceDatum.Start == FVector(1.0f, 2.0f, 3.0f)
							&& TraceDatum.End == FVector(4.0f, 5.0f, 6.0f)
							&& TraceDatum.Rot == FQuat::Identity
							&& TraceDatum.OutHits.Num() == 0
							&& TraceDatum.TraceType == EAsyncTraceType::Multi
							&& TraceDatum.TraceChannel == ECollisionChannel::ECC_Visibility
							&& TraceDatum.UserData == 77
							&& OverlapDatum.Pos == FVector(7.0f, 8.0f, 9.0f)
							&& OverlapDatum.Rot == FQuat::Identity
							&& OverlapDatum.OutOverlaps.Num() == 0
							&& OverlapDatum.TraceChannel == ECollisionChannel::ECC_WorldDynamic
							&& OverlapDatum.UserData == 88
							&& EAsyncTraceType::Test != EAsyncTraceType::Single
							&& EAsyncTraceType::Single != EAsyncTraceType::Multi ? 1 : 0;
					}

					UFUNCTION()
					int VerifyInvalidQueryHelpers()
					{
						FTraceHandle InvalidHandle;
						FTraceDatum TraceDatum;
						FOverlapDatum OverlapDatum;

						return !System::QueryTraceData(InvalidHandle, TraceDatum)
							&& !System::QueryOverlapData(InvalidHandle, OverlapDatum)
							&& !System::IsTraceHandleValid(InvalidHandle, false)
							&& !System::IsTraceHandleValid(InvalidHandle, true) ? 1 : 0;
					}

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
						OverlapHandleValidInitially = System::IsTraceHandleValid(OverlapHandle, true) ? 1 : 0;

						return LineHandleValidInitially == 1
							&& OverlapHandleValidInitially == 1 ? 1 : 0;
					}

					UFUNCTION()
					void HandleLineTrace(uint64 TraceHandleValue, const TArray<FHitResult>& OutHits, uint32 UserData)
					{
						LineCallbackCount += 1;
					}

					UFUNCTION()
					void HandleOverlapTrace(uint64 TraceHandleValue, const TArray<FOverlapResult>& OutOverlaps, uint32 UserData)
					{
						OverlapCallbackCount += 1;
					}
				}
				)AS"),
			AsyncClassName);
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();

		AActor& TargetActor = Spawner.SpawnActor<AActor>();
		UBoxComponent* TargetBox = AddCollisionBox(
			TargetActor,
			FName(TEXT("AsyncCollisionContractTarget")),
			TargetExtent,
			TargetLocation);
		ASSERT_THAT(IsNotNull(TargetBox, TEXT("World collision async target box should be created")));

		AActor* ScriptActor = AngelscriptFunctionalTestUtils::SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(ScriptActor, TEXT("World collision async script actor should spawn")));
		if (ScriptActor == nullptr)
		{
			return;
		}

		AngelscriptFunctionalTestUtils::BeginPlayActor(Engine, *ScriptActor);

		UWorld* World = TargetActor.GetWorld();
		ASSERT_THAT(IsNotNull(World, TEXT("World collision async test should access the spawned test world")));
		if (World == nullptr)
		{
			return;
		}

		int32 StartResult = 0;
		int32 TraceValueSurfaceResult = 0;
		int32 InvalidQueryHelpersResult = 0;
		if (!ExecuteGeneratedIntMethod(*TestRunner, ScriptActor, ScriptClass, TEXT("VerifyTraceValueSurface"), TraceValueSurfaceResult)
			|| !ExecuteGeneratedIntMethod(*TestRunner, ScriptActor, ScriptClass, TEXT("VerifyInvalidQueryHelpers"), InvalidQueryHelpersResult))
		{
			return;
		}
		ASSERT_THAT(AreEqual(1, TraceValueSurfaceResult, TEXT("Trace handle and datum value bindings should preserve constructor, property, enum, and equality behavior")));
		ASSERT_THAT(AreEqual(1, InvalidQueryHelpersResult, TEXT("Invalid trace handles should safely dispatch through query and validity wrappers")));

		if (!ExecuteGeneratedIntMethod(*TestRunner, ScriptActor, ScriptClass, TEXT("StartAsyncQueries"), StartResult))
		{
			return;
		}
		ASSERT_THAT(AreEqual(1, StartResult, TEXT("Async trace and overlap bindings should register and return initially valid handles")));

		if (!WaitForAsyncCallbacks(*TestRunner, Engine, *World, *ScriptActor))
		{
			return;
		}

		int32 LineCallbackCount = 0;
		int32 OverlapCallbackCount = 0;
		int32 LineHandleValidInitially = 0;
		int32 OverlapHandleValidInitially = 0;
		if (!AngelscriptFunctionalTestUtils::ReadIntPropertyChecked(*TestRunner, ScriptActor, TEXT("LineCallbackCount"), LineCallbackCount)
			|| !AngelscriptFunctionalTestUtils::ReadIntPropertyChecked(*TestRunner, ScriptActor, TEXT("OverlapCallbackCount"), OverlapCallbackCount)
			|| !AngelscriptFunctionalTestUtils::ReadIntPropertyChecked(*TestRunner, ScriptActor, TEXT("LineHandleValidInitially"), LineHandleValidInitially)
			|| !AngelscriptFunctionalTestUtils::ReadIntPropertyChecked(*TestRunner, ScriptActor, TEXT("OverlapHandleValidInitially"), OverlapHandleValidInitially))
		{
			return;
		}

		ASSERT_THAT(IsTrue(LineCallbackCount >= 1, TEXT("Async line trace binding should reach its script delegate")));
		ASSERT_THAT(IsTrue(OverlapCallbackCount >= 1, TEXT("Async overlap binding should reach its script delegate")));
		ASSERT_THAT(AreEqual(1, LineHandleValidInitially, TEXT("Async line trace binding should expose a valid handle")));
		ASSERT_THAT(AreEqual(1, OverlapHandleValidInitially, TEXT("Async overlap binding should expose a valid handle")));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
