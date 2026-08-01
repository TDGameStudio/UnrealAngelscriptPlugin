#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"

#include "AngelscriptTestSuite.generated.h"

/**
 * Native base for reflected AngelScript test suites.
 *
 * The reflected surface intentionally contains lifecycle events only.
 * Assertions and expected-log helpers are registered as native AngelScript
 * methods. Local-world and latent-command authoring live on the fieldless
 * FAngelscriptTest facade so the suite remains focused on fixture semantics.
 */
UCLASS(Abstract, Transient)
class ANGELSCRIPTRUNTIME_API UAngelscriptTestSuite : public UObject
{
	GENERATED_BODY()

public:
	UWorld* GetWorld() const override;

	UFUNCTION(BlueprintNativeEvent, Category = "Angelscript Test")
	void BeforeAll();
	virtual void BeforeAll_Implementation();

	UFUNCTION(BlueprintNativeEvent, Category = "Angelscript Test")
	void BeforeEach();
	virtual void BeforeEach_Implementation();

	UFUNCTION(BlueprintNativeEvent, Category = "Angelscript Test")
	void AfterEach();
	virtual void AfterEach_Implementation();

	UFUNCTION(BlueprintNativeEvent, Category = "Angelscript Test")
	void AfterAll();
	virtual void AfterAll_Implementation();
};
