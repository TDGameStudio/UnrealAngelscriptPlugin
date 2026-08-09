#include "Testing/AngelscriptTest.h"

#include "AngelscriptBinds.h"
#include "Testing/AngelscriptScriptTestRunner.h"
#include "Testing/AngelscriptTestSuite.h"
#include "Testing/AngelscriptTest_Functions.h"
#include "Testing/LatentAutomationCommand.h"

#include "Components/ActorComponent.h"
#include "GameFramework/Actor.h"

namespace
{
	FAngelscriptScriptTestExecutionContext* RequireActiveContext(
		const TCHAR* Operation)
	{
		if (FAngelscriptScriptTestExecutionContext* Context =
			FAngelscriptScriptTestRunner::GetActiveContext())
		{
			return Context;
		}

		FAngelscriptScriptTestRunner::ReportActiveContextMisuse(
			FString::Printf(
				TEXT("FAngelscriptTest::%s requires an active method leaf."),
				Operation));
		return nullptr;
	}

	template <bool bTeardown>
	FAngelscriptTestCommandBuilder EnqueueAction(
		FName Action,
		const FString& Description,
		const TCHAR* Operation)
	{
		if (FAngelscriptScriptTestExecutionContext* Context =
			RequireActiveContext(Operation))
		{
			Context->EnqueueAction(Action, Description, bTeardown);
		}
		return {};
	}
}

FAngelscriptTestCommandBuilder FAngelscriptTest::Commands()
{
	RequireActiveContext(TEXT("Commands"));
	return {};
}

void FAngelscriptTest::CreateTestWorld(
	bool bInitializeGameSubsystems)
{
	if (FAngelscriptScriptTestExecutionContext* Context =
		RequireActiveContext(TEXT("CreateTestWorld")))
	{
		Context->CreateTestWorld(bInitializeGameSubsystems);
	}
}

void FAngelscriptTest::DestroyTestWorld()
{
	if (FAngelscriptScriptTestExecutionContext* Context =
		RequireActiveContext(TEXT("DestroyTestWorld")))
	{
		Context->DestroyTestWorld();
	}
}

UWorld* FAngelscriptTest::GetTestWorld()
{
	if (FAngelscriptScriptTestExecutionContext* Context =
		RequireActiveContext(TEXT("GetTestWorld")))
	{
		return Context->GetWorld();
	}
	return nullptr;
}

UObject* FAngelscriptTest::SpawnObject(
	UClass* ObjectClass,
	UObject* Outer)
{
	if (FAngelscriptScriptTestExecutionContext* Context =
		RequireActiveContext(TEXT("SpawnObject")))
	{
		return Context->SpawnObject(ObjectClass, Outer);
	}
	return nullptr;
}

AActor* FAngelscriptTest::SpawnActor(
	const TSubclassOf<AActor>& ActorClass,
	const FVector& Location,
	const FRotator& Rotation)
{
	if (FAngelscriptScriptTestExecutionContext* Context =
		RequireActiveContext(TEXT("SpawnActor")))
	{
		return Context->SpawnActor(
			ActorClass.Get(),
			Location,
			Rotation);
	}
	return nullptr;
}

UActorComponent* FAngelscriptTest::SpawnComponent(
	const TSubclassOf<UActorComponent>& ComponentClass,
	AActor* Owner,
	bool bRegister)
{
	if (FAngelscriptScriptTestExecutionContext* Context =
		RequireActiveContext(TEXT("SpawnComponent")))
	{
		return Context->SpawnComponent(
			ComponentClass.Get(),
			Owner,
			bRegister);
	}
	return nullptr;
}

void FAngelscriptTest::BeginPlay(AActor* Actor)
{
	if (FAngelscriptScriptTestExecutionContext* Context =
		RequireActiveContext(TEXT("BeginPlay")))
	{
		Context->BeginPlay(Actor);
	}
}

void FAngelscriptTest::BeginPlayAll()
{
	if (FAngelscriptScriptTestExecutionContext* Context =
		RequireActiveContext(TEXT("BeginPlayAll")))
	{
		Context->BeginPlayAll();
	}
}

void FAngelscriptTest::TickWorld(
	float DeltaSeconds,
	int32 NumTicks)
{
	if (FAngelscriptScriptTestExecutionContext* Context =
		RequireActiveContext(TEXT("TickWorld")))
	{
		Context->TickWorld(DeltaSeconds, NumTicks);
	}
}

void FAngelscriptTest::TickActor(
	AActor* Actor,
	float DeltaSeconds,
	int32 NumTicks)
{
	if (FAngelscriptScriptTestExecutionContext* Context =
		RequireActiveContext(TEXT("TickActor")))
	{
		Context->TickActor(Actor, DeltaSeconds, NumTicks);
	}
}

void FAngelscriptTest::TickComponent(
	UActorComponent* Component,
	float DeltaSeconds,
	int32 NumTicks)
{
	if (FAngelscriptScriptTestExecutionContext* Context =
		RequireActiveContext(TEXT("TickComponent")))
	{
		Context->TickComponent(
			Component,
			DeltaSeconds,
			NumTicks);
	}
}

void FAngelscriptTest::AdvanceTime(
	float DeltaSeconds,
	int32 NumTicks)
{
	if (FAngelscriptScriptTestExecutionContext* Context =
		RequireActiveContext(TEXT("AdvanceTime")))
	{
		Context->AdvanceTime(DeltaSeconds, NumTicks);
	}
}

void FAngelscriptTest::DestroyActor(
	AActor* Actor,
	bool bDrain)
{
	if (FAngelscriptScriptTestExecutionContext* Context =
		RequireActiveContext(TEXT("DestroyActor")))
	{
		Context->DestroyActor(Actor, bDrain);
	}
}

FAngelscriptTestCommandBuilder FAngelscriptTestCommandBuilder::Do(
	FName Action,
	const FString& Description) const
{
	return EnqueueAction<false>(
		Action,
		Description,
		TEXT("Commands().Do"));
}

FAngelscriptTestCommandBuilder FAngelscriptTestCommandBuilder::Then(
	FName Action,
	const FString& Description) const
{
	return EnqueueAction<false>(
		Action,
		Description,
		TEXT("Commands().Then"));
}

FAngelscriptTestCommandBuilder FAngelscriptTestCommandBuilder::StartWhen(
	FName Condition,
	float TimeoutSeconds,
	const FString& Description) const
{
	if (FAngelscriptScriptTestExecutionContext* Context =
		RequireActiveContext(TEXT("Commands().StartWhen")))
	{
		Context->EnqueueCondition(
			Condition,
			TimeoutSeconds,
			Description);
	}
	return {};
}

FAngelscriptTestCommandBuilder FAngelscriptTestCommandBuilder::Until(
	FName Condition,
	float TimeoutSeconds,
	const FString& Description) const
{
	if (FAngelscriptScriptTestExecutionContext* Context =
		RequireActiveContext(TEXT("Commands().Until")))
	{
		Context->EnqueueCondition(
			Condition,
			TimeoutSeconds,
			Description);
	}
	return {};
}

FAngelscriptTestCommandBuilder FAngelscriptTestCommandBuilder::WaitDelay(
	float Seconds,
	const FString& Description) const
{
	if (FAngelscriptScriptTestExecutionContext* Context =
		RequireActiveContext(TEXT("Commands().WaitDelay")))
	{
		Context->EnqueueDelay(Seconds, Description);
	}
	return {};
}

FAngelscriptTestCommandBuilder FAngelscriptTestCommandBuilder::OnTearDown(
	FName Action,
	const FString& Description) const
{
	return EnqueueAction<true>(
		Action,
		Description,
		TEXT("Commands().OnTearDown"));
}

FAngelscriptTestCommandBuilder FAngelscriptTestCommandBuilder::OnCleanup(
	FName Action,
	const FString& Description) const
{
	return EnqueueAction<true>(
		Action,
		Description,
		TEXT("Commands().OnCleanup"));
}

FAngelscriptTestCommandBuilder
FAngelscriptTestCommandBuilder::AddLatentCommand(
	ULatentAutomationCommand* Command,
	float TimeoutSeconds) const
{
	if (FAngelscriptScriptTestExecutionContext* Context =
		RequireActiveContext(TEXT("Commands().AddLatentCommand")))
	{
		Context->EnqueueAdvanced(Command, TimeoutSeconds);
	}
	return {};
}

namespace
{
	void BindAngelscriptTest(FAngelscriptBinds& Binds)
	{
		{
			FAngelscriptBinds::FNamespace Namespace(
				Binds.GetTargetEngine(),
				"FAngelscriptTest");
			Binds.BindGlobalFunctionForTarget(
				"FAngelscriptTestCommandBuilder Commands()",
				&FAngelscriptTest::Commands);
			Binds.BindGlobalFunctionForTarget(
				"void CreateTestWorld(bool bInitializeGameSubsystems = true)",
				&FAngelscriptTest::CreateTestWorld);
			Binds.BindGlobalFunctionForTarget(
				"void DestroyTestWorld()",
				&FAngelscriptTest::DestroyTestWorld);
			Binds.BindGlobalFunctionForTarget(
				"UWorld GetTestWorld()",
				&FAngelscriptTest::GetTestWorld);
			Binds.BindGlobalFunctionForTarget(
				"UObject SpawnObject(UClass ObjectClass, UObject Outer = nullptr)",
				&FAngelscriptTest::SpawnObject);
			Binds.BindGlobalFunctionForTarget(
				"AActor SpawnActor(TSubclassOf<AActor> ActorClass, FVector Location = FVector::ZeroVector, FRotator Rotation = FRotator::ZeroRotator)",
				&FAngelscriptTest::SpawnActor);
			Binds.BindGlobalFunctionForTarget(
				"UActorComponent SpawnComponent(TSubclassOf<UActorComponent> ComponentClass, AActor Owner, bool bRegister = true)",
				&FAngelscriptTest::SpawnComponent);
			Binds.BindGlobalFunctionForTarget(
				"void BeginPlay(AActor Actor)",
				&FAngelscriptTest::BeginPlay);
			Binds.BindGlobalFunctionForTarget(
				"void BeginPlayAll()",
				&FAngelscriptTest::BeginPlayAll);
			Binds.BindGlobalFunctionForTarget(
				"void TickWorld(float32 DeltaSeconds, int NumTicks = 1)",
				&FAngelscriptTest::TickWorld);
			Binds.BindGlobalFunctionForTarget(
				"void TickActor(AActor Actor, float32 DeltaSeconds, int NumTicks = 1)",
				&FAngelscriptTest::TickActor);
			Binds.BindGlobalFunctionForTarget(
				"void TickComponent(UActorComponent Component, float32 DeltaSeconds, int NumTicks = 1)",
				&FAngelscriptTest::TickComponent);
			Binds.BindGlobalFunctionForTarget(
				"void AdvanceTime(float32 DeltaSeconds, int NumTicks = 1)",
				&FAngelscriptTest::AdvanceTime);
			Binds.BindGlobalFunctionForTarget(
				"void DestroyActor(AActor Actor, bool bDrain = true)",
				&FAngelscriptTest::DestroyActor);
		}

		FAngelscriptBinds Builder =
			Binds.ExistingClassForTarget(
				"FAngelscriptTestCommandBuilder");
		Builder.Method(
			"FAngelscriptTestCommandBuilder Do(FName Action, const FString& Description = \"\") const allow_discard",
			&FAngelscriptTestCommandBuilder::Do);
		Builder.Method(
			"FAngelscriptTestCommandBuilder Then(FName Action, const FString& Description = \"\") const allow_discard",
			&FAngelscriptTestCommandBuilder::Then);
		Builder.Method(
			"FAngelscriptTestCommandBuilder StartWhen(FName Condition, float32 TimeoutSeconds = 5.0, const FString& Description = \"\") const allow_discard",
			&FAngelscriptTestCommandBuilder::StartWhen);
		Builder.Method(
			"FAngelscriptTestCommandBuilder Until(FName Condition, float32 TimeoutSeconds = 5.0, const FString& Description = \"\") const allow_discard",
			&FAngelscriptTestCommandBuilder::Until);
		Builder.Method(
			"FAngelscriptTestCommandBuilder WaitDelay(float32 Seconds, const FString& Description = \"\") const allow_discard",
			&FAngelscriptTestCommandBuilder::WaitDelay);
		Builder.Method(
			"FAngelscriptTestCommandBuilder OnTearDown(FName Action, const FString& Description = \"\") const allow_discard",
			&FAngelscriptTestCommandBuilder::OnTearDown);
		Builder.Method(
			"FAngelscriptTestCommandBuilder OnCleanup(FName Action, const FString& Description = \"\") const allow_discard",
			&FAngelscriptTestCommandBuilder::OnCleanup);
		Builder.Method(
			"FAngelscriptTestCommandBuilder AddLatentCommand(ULatentAutomationCommand Command, float32 TimeoutSeconds = 5.0) const allow_discard",
			&FAngelscriptTestCommandBuilder::AddLatentCommand);

		FAngelscriptBinds LatentCommand =
			Binds.ExistingClassForTarget(
				"ULatentAutomationCommand");
		LatentCommand.Method(
			"UAngelscriptTestSuite GetCurrentSuite() const",
			&FAngelscriptTestBinds::GetCurrentSuite);
	}
}

AS_FORCE_LINK const FAngelscriptBind Bind_AngelscriptTest(
	TEXT("AngelscriptTest.ExplicitBindings"),
	EAngelscriptBindPhase::ExplicitBindings,
	&BindAngelscriptTest);
