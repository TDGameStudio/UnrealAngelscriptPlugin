// ============================================================================
// AngelscriptWorldCollisionAsyncSweepBindingsTests.cpp
//
// World collision async sweep binding contract smoke. Detailed hit payload,
// query-data, and shape/profile matrices belong in Coverage (`13-physics-collision`).
// ============================================================================

#include "CQTest.h"
#include "AngelscriptFunctionalTestUtils.h"
#include "AngelscriptTestMacros.h"

#include "Components/ActorTestSpawner.h"
#include "Components/BoxComponent.h"
#include "GameFramework/Actor.h"
#include "Misc/ScopeExit.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FAngelscriptWorldCollisionAsyncSweepBindingsTest,
	"Angelscript.TestModule.Bindings.WorldCollisionAsyncSweep",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	inline static const FName AsyncSweepModuleName = FName(TEXT("ASWorldCollisionAsyncSweepEntrypointSmoke"));
	inline static const FString AsyncSweepFilename = FString(TEXT("WorldCollisionAsyncSweepEntrypointSmoke.as"));
	inline static const FName AsyncSweepClassName = FName(TEXT("ATestWorldCollisionAsyncSweepEntrypointSmoke"));
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
			*FString::Printf(TEXT("World collision async sweep method '%s' should exist"), *FunctionName.ToString())))
		{
			return false;
		}

		return LocalAssert.IsTrue(
			ExecuteGeneratedIntEventOnGameThread(Object, Function, OutResult),
			*FString::Printf(TEXT("World collision async sweep method '%s' should execute"), *FunctionName.ToString()));
	}

	static bool WaitForAsyncSweepCallbacks(
		FAutomationTestBase& Test,
		FAngelscriptEngine& Engine,
		UWorld& World,
		AActor& ScriptActor)
	{
		FNoDiscardAsserter LocalAssert(Test);
		FIntProperty* ChannelCallbackCountProperty = FindFProperty<FIntProperty>(ScriptActor.GetClass(), TEXT("ChannelCallbackCount"));
		FIntProperty* ObjectCallbackCountProperty = FindFProperty<FIntProperty>(ScriptActor.GetClass(), TEXT("ObjectCallbackCount"));
		FIntProperty* ProfileCallbackCountProperty = FindFProperty<FIntProperty>(ScriptActor.GetClass(), TEXT("ProfileCallbackCount"));
		bool bHasCallbackProperties = true;
		bHasCallbackProperties &= LocalAssert.IsNotNull(ChannelCallbackCountProperty, TEXT("Async sweep actor should expose ChannelCallbackCount"));
		bHasCallbackProperties &= LocalAssert.IsNotNull(ObjectCallbackCountProperty, TEXT("Async sweep actor should expose ObjectCallbackCount"));
		bHasCallbackProperties &= LocalAssert.IsNotNull(ProfileCallbackCountProperty, TEXT("Async sweep actor should expose ProfileCallbackCount"));
		if (!bHasCallbackProperties)
		{
			return false;
		}

		for (int32 TickIndex = 0; TickIndex < MaxTickCount; ++TickIndex)
		{
			const int32 ChannelCallbackCount = ChannelCallbackCountProperty->GetPropertyValue_InContainer(&ScriptActor);
			const int32 ObjectCallbackCount = ObjectCallbackCountProperty->GetPropertyValue_InContainer(&ScriptActor);
			const int32 ProfileCallbackCount = ProfileCallbackCountProperty->GetPropertyValue_InContainer(&ScriptActor);
			if (ChannelCallbackCount >= 1 && ObjectCallbackCount >= 1 && ProfileCallbackCount >= 1)
			{
				return true;
			}

			AngelscriptFunctionalTestUtils::TickWorld(Engine, World, TickDeltaTime, 1);
		}

		Test.AddError(FString::Printf(
			TEXT("Async sweep contract callbacks did not arrive within %d ticks."),
			MaxTickCount));
		return false;
	}

public:
	TEST_METHOD(AsyncSweepRegistrationSmoke)
	{
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE_FULL();
		FAngelscriptEngineScope Scope(Engine);
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*AsyncSweepModuleName.ToString());
		};

		UClass* ScriptClass = AngelscriptFunctionalTestUtils::CompileScriptModule(
			*TestRunner,
			Engine,
			AsyncSweepModuleName,
			AsyncSweepFilename,
			ASTEST_AS(R"AS(
				UCLASS()
				class ATestWorldCollisionAsyncSweepEntrypointSmoke : AActor
				{
					UPROPERTY()
					int ChannelCallbackCount = 0;

					UPROPERTY()
					int ObjectCallbackCount = 0;

					UPROPERTY()
					int ProfileCallbackCount = 0;

					UPROPERTY()
					int ChannelHandleValidInitially = 0;

					UPROPERTY()
					int ObjectHandleValidInitially = 0;

					UPROPERTY()
					int ProfileHandleValidInitially = 0;

					FTraceHandle ChannelHandle;
					FTraceHandle ObjectHandle;
					FTraceHandle ProfileHandle;

					UFUNCTION()
					int StartAsyncSweeps()
					{
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
						ChannelHandleValidInitially = System::IsTraceHandleValid(ChannelHandle, false) ? 1 : 0;

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
						ObjectHandleValidInitially = System::IsTraceHandleValid(ObjectHandle, false) ? 1 : 0;

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
						ProfileHandleValidInitially = System::IsTraceHandleValid(ProfileHandle, false) ? 1 : 0;

						return ChannelHandleValidInitially == 1
							&& ObjectHandleValidInitially == 1
							&& ProfileHandleValidInitially == 1 ? 1 : 0;
					}

					UFUNCTION()
					void HandleChannelSweep(uint64 TraceHandleValue, const TArray<FHitResult>& OutHits, uint32 UserData)
					{
						ChannelCallbackCount += 1;
					}

					UFUNCTION()
					void HandleObjectSweep(uint64 TraceHandleValue, const TArray<FHitResult>& OutHits, uint32 UserData)
					{
						ObjectCallbackCount += 1;
					}

					UFUNCTION()
					void HandleProfileSweep(uint64 TraceHandleValue, const TArray<FHitResult>& OutHits, uint32 UserData)
					{
						ProfileCallbackCount += 1;
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

		AActor& TargetActor = Spawner.SpawnActor<AActor>();
		UBoxComponent* TargetBox = AddCollisionBox(
			TargetActor,
			FName(TEXT("AsyncSweepContractTarget")),
			TargetExtent,
			TargetLocation);
		ASSERT_THAT(IsNotNull(TargetBox, TEXT("Async sweep blocking box should be created")));

		AActor* ScriptActor = AngelscriptFunctionalTestUtils::SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(ScriptActor, TEXT("Async sweep script actor should spawn")));
		if (ScriptActor == nullptr)
		{
			return;
		}

		AngelscriptFunctionalTestUtils::BeginPlayActor(Engine, *ScriptActor);

		UWorld* World = TargetActor.GetWorld();
		ASSERT_THAT(IsNotNull(World, TEXT("Async sweep test should access the spawned world")));
		if (World == nullptr)
		{
			return;
		}

		int32 StartResult = 0;
		if (!ExecuteGeneratedIntMethod(*TestRunner, ScriptActor, ScriptClass, TEXT("StartAsyncSweeps"), StartResult))
		{
			return;
		}
		ASSERT_THAT(AreEqual(1, StartResult, TEXT("Async sweep bindings should register and return initially valid handles")));

		if (!WaitForAsyncSweepCallbacks(*TestRunner, Engine, *World, *ScriptActor))
		{
			return;
		}

		int32 ChannelCallbackCount = 0;
		int32 ObjectCallbackCount = 0;
		int32 ProfileCallbackCount = 0;
		int32 ChannelHandleValidInitially = 0;
		int32 ObjectHandleValidInitially = 0;
		int32 ProfileHandleValidInitially = 0;
		if (!AngelscriptFunctionalTestUtils::ReadIntPropertyChecked(*TestRunner, ScriptActor, TEXT("ChannelCallbackCount"), ChannelCallbackCount)
			|| !AngelscriptFunctionalTestUtils::ReadIntPropertyChecked(*TestRunner, ScriptActor, TEXT("ObjectCallbackCount"), ObjectCallbackCount)
			|| !AngelscriptFunctionalTestUtils::ReadIntPropertyChecked(*TestRunner, ScriptActor, TEXT("ProfileCallbackCount"), ProfileCallbackCount)
			|| !AngelscriptFunctionalTestUtils::ReadIntPropertyChecked(*TestRunner, ScriptActor, TEXT("ChannelHandleValidInitially"), ChannelHandleValidInitially)
			|| !AngelscriptFunctionalTestUtils::ReadIntPropertyChecked(*TestRunner, ScriptActor, TEXT("ObjectHandleValidInitially"), ObjectHandleValidInitially)
			|| !AngelscriptFunctionalTestUtils::ReadIntPropertyChecked(*TestRunner, ScriptActor, TEXT("ProfileHandleValidInitially"), ProfileHandleValidInitially))
		{
			return;
		}

		ASSERT_THAT(IsTrue(ChannelCallbackCount >= 1, TEXT("AsyncSweepByChannel binding should reach its script delegate")));
		ASSERT_THAT(IsTrue(ObjectCallbackCount >= 1, TEXT("AsyncSweepByObjectType binding should reach its script delegate")));
		ASSERT_THAT(IsTrue(ProfileCallbackCount >= 1, TEXT("AsyncSweepByProfile binding should reach its script delegate")));
		ASSERT_THAT(AreEqual(1, ChannelHandleValidInitially, TEXT("AsyncSweepByChannel should expose a valid handle")));
		ASSERT_THAT(AreEqual(1, ObjectHandleValidInitially, TEXT("AsyncSweepByObjectType should expose a valid handle")));
		ASSERT_THAT(AreEqual(1, ProfileHandleValidInitially, TEXT("AsyncSweepByProfile should expose a valid handle")));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
