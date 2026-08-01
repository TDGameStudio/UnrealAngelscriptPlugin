#include "Testing/LatentAutomationCommand.h"

#include "Engine/ActorChannel.h"
#include "Engine/NetDriver.h"
#include "Net/UnrealNetwork.h"
#include "ClassGenerator/ASClass.h"
#include "Core/AngelscriptEngine.h"
#include "Misc/AutomationTest.h"
#include "Testing/AngelscriptScriptTestRunner.h"
#include "Testing/AngelscriptTestSuite.h"

ULatentAutomationCommand::ULatentAutomationCommand() :
	World(nullptr), bAllowTimeout(false), bAlsoRunOnClient(false)
{
}

void ULatentAutomationCommand::Before_Implementation()
{
	/* Default: do nothing */
}

bool ULatentAutomationCommand::BeforeOnClient_Implementation()
{
	return true;
}

bool ULatentAutomationCommand::Update_Implementation()
{
	return true;
}

bool ULatentAutomationCommand::UpdateOnClient_Implementation()
{
	return true;
}

void ULatentAutomationCommand::After_Implementation()
{
	/* Default: do nothing */
}

bool ULatentAutomationCommand::AfterOnClient_Implementation()
{
	return true;
}

bool ULatentAutomationCommand::HasAuthority() const
{
	return ExecutionContext.IsValid();
}

UWorld* ULatentAutomationCommand::GetWorld() const
{
	if (World != nullptr)
	{
		return World;
	}
	else
	{
		if (const UObject* MyOuter = GetOuter())
		{
			return MyOuter->GetWorld();
		}
		return nullptr;
	}
}

void ULatentAutomationCommand::SetWorld(UWorld* InWorld)
{
	World = InWorld;
}

void ULatentAutomationCommand::SetExecutionContext(
	TWeakPtr<FAngelscriptScriptTestExecutionContext> InContext)
{
	ExecutionContext = MoveTemp(InContext);
}

void ULatentAutomationCommand::ClearExecutionContext()
{
	ExecutionContext.Reset();
	World = nullptr;
}

TSharedPtr<FAngelscriptScriptTestExecutionContext>
ULatentAutomationCommand::GetExecutionContext() const
{
	return ExecutionContext.Pin();
}

UAngelscriptTestSuite* ULatentAutomationCommand::GetCurrentSuite() const
{
	const TSharedPtr<FAngelscriptScriptTestExecutionContext> Context =
		ExecutionContext.Pin();
	if (Context.IsValid()
		&& !Context->IsComplete()
		&& Context->GetSuite() != nullptr)
	{
		return Context->GetSuite();
	}

	if (FAutomationTestBase* CurrentTest =
		FAutomationTestFramework::Get().GetCurrentTest())
	{
		CurrentTest->AddError(TEXT(
			"ULatentAutomationCommand::GetCurrentSuite was called "
			"outside its active script-test leaf."));
	}
	FAngelscriptEngine::Throw(
		"ULatentAutomationCommand has no active script-test suite.");
	return nullptr;
}

bool ULatentAutomationCommand::AllowsTimeout() const
{
	return bAllowTimeout;
}

bool ULatentAutomationCommand::RunsOnClient() const
{
	return bAlsoRunOnClient;
}

bool ULatentAutomationCommand::IsSupportedForNetworking() const
{
	return true;
}

int32 ULatentAutomationCommand::GetFunctionCallspace(UFunction* Function, FFrame* Stack)
{
	check(GetOuter() != nullptr);
	return GetOuter()->GetFunctionCallspace(Function, Stack);
}

bool ULatentAutomationCommand::CallRemoteFunction(UFunction* Function, void* Parms, struct FOutParmRec* OutParms, FFrame* Stack)
{
	check(!HasAnyFlags(RF_ClassDefaultObject));
	AActor* Owner = GetTypedOuter<AActor>();
	UNetDriver* NetDriver = Owner->GetNetDriver();
	if (NetDriver)
	{
		NetDriver->ProcessRemoteFunction(Owner, Function, Parms, OutParms, Stack, this);
		return true;
	}
	return false;
}

void ULatentAutomationCommand::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ULatentAutomationCommand, bAllowTimeout);
	DOREPLIFETIME(ULatentAutomationCommand, bAlsoRunOnClient);

	// Replicate variables from angelscript
	UASClass* asClass = Cast<UASClass>(GetClass());
	//if (GetClass()->ScriptTypePtr != nullptr)
	if (asClass && asClass->ScriptTypePtr != nullptr)
	{
		//GetClass()->GetLifetimeScriptReplicationList(OutLifetimeProps);
		asClass->GetLifetimeScriptReplicationList(OutLifetimeProps);
	}
}
