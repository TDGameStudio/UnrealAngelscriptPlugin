#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "AngelscriptPropertyAccessorRemovalTestTypes.generated.h"

UCLASS(BlueprintType, Blueprintable)
class ANGELSCRIPTRUNTIME_API AAngelscriptPropertyAccessorCarrier : public AActor
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadWrite)
	int32 Field = 17;

	UPROPERTY(BlueprintReadWrite)
	bool bEnabled = true;

	UPROPERTY(BlueprintReadWrite, BlueprintGetter=FetchScore)
	int32 Score = 7;

	UFUNCTION(BlueprintCallable, BlueprintPure)
	int32 FetchScore() const;

	UFUNCTION()
	void SetField(int32 InField);
};
