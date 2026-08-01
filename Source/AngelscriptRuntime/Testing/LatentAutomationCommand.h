#pragma once

#include "Templates/SharedPointer.h"
#include "UObject/Object.h"

#include "LatentAutomationCommand.generated.h"

class FAngelscriptScriptTestExecutionContext;
class UAngelscriptTestSuite;

UCLASS(Blueprintable)
class ULatentAutomationCommand : public UObject
{
	GENERATED_BODY()

public:
	ULatentAutomationCommand();

	UFUNCTION(BlueprintNativeEvent)
	void Before();

	UFUNCTION(BlueprintNativeEvent)
	bool BeforeOnClient();

	UFUNCTION(BlueprintNativeEvent)
	bool Update();

	UFUNCTION(BlueprintNativeEvent)
	bool UpdateOnClient();

	UFUNCTION(BlueprintNativeEvent)
	void After();

	UFUNCTION(BlueprintNativeEvent)
	bool AfterOnClient();

	UFUNCTION(BlueprintImplementableEvent)
	FString Describe() const;

	UFUNCTION(BlueprintImplementableEvent)
	FString DescribeOnClient() const;

	UFUNCTION(BlueprintCallable, Category = "LatentAutomationCommand")
	bool HasAuthority() const;

	UWorld* GetWorld() const override;

	void SetWorld(UWorld* InWorld);

	void SetExecutionContext(
		TWeakPtr<FAngelscriptScriptTestExecutionContext> InContext);
	void ClearExecutionContext();
	TSharedPtr<FAngelscriptScriptTestExecutionContext>
		GetExecutionContext() const;
	UAngelscriptTestSuite* GetCurrentSuite() const;

	bool AllowsTimeout() const;
	bool RunsOnClient() const;

	bool IsSupportedForNetworking() const override;

	int32 GetFunctionCallspace(UFunction* Function, FFrame* Stack) override;
	bool CallRemoteFunction(UFunction* Function, void* Parms, struct FOutParmRec* OutParms, FFrame* Stack) override;

	void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	UPROPERTY()
	UWorld* World;

	UPROPERTY(Replicated, EditDefaultsOnly, Category=Settings)
	bool bAllowTimeout = false;

	UPROPERTY(Replicated, EditDefaultsOnly, Category=Settings)
	bool bAlsoRunOnClient = false;

	TWeakPtr<FAngelscriptScriptTestExecutionContext> ExecutionContext;
};
