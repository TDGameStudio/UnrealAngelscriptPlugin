#pragma once

#include "CoreMinimal.h"
#include "AngelscriptFunctionLibraryContractTestTypes.generated.h"

UCLASS(meta = (NotInAngelscript, ScriptMixin = "FVector"))
class UAngelscriptFunctionLibraryMissingReceiverFixture : public UObject
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable)
	static double MissingReceiver()
	{
		return 0.0;
	}
};

UCLASS(meta = (NotInAngelscript, ScriptMixin = "FDoesNotExist"))
class UAngelscriptFunctionLibraryUnresolvedReceiverFixture : public UObject
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable)
	static double UnresolvedReceiver(const FVector& Value)
	{
		return Value.X;
	}
};

UCLASS(meta = (NotInAngelscript, ScriptMixin = "FRotator"))
class UAngelscriptFunctionLibraryIncompatibleReceiverFixture : public UObject
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable)
	static double IncompatibleReceiver(const FVector& Value)
	{
		return Value.X;
	}
};

UCLASS(meta = (NotInAngelscript, ScriptMixin = "FRotator FVector"))
class UAngelscriptFunctionLibraryMultiTargetFixture : public UObject
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable)
	static double MatchingReceiver(const FVector& Value)
	{
		return Value.X;
	}
};
