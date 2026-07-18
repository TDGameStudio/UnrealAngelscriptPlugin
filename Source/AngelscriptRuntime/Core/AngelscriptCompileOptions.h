#pragma once

#include "CoreMinimal.h"

#include "AngelscriptCompileOptions.generated.h"

UENUM(BlueprintType)
enum class EAngelscriptFunctionBindingMethod : uint8
{
	None UMETA(DisplayName = "None"),
	NativeRuntimeLinked UMETA(DisplayName = "Native Runtime Linked"),
	NativeModuleFunctionAddress UMETA(DisplayName = "Native Module Function Address"),
};

UCLASS(Config=AngelscriptCompileOptions, DefaultConfig, meta = (DisplayName = "Angelscript Compile Options"))
class ANGELSCRIPTRUNTIME_API UAngelscriptCompileOptions : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(Config, EditAnywhere, Category = "Tests", Meta = (ConfigRestartRequired = true))
	bool bCompileAngelscriptUnitTests = false;

	UPROPERTY(Config, EditAnywhere, Category = "Bindings", Meta = (ConfigRestartRequired = true))
	EAngelscriptFunctionBindingMethod FunctionBindingMethod = EAngelscriptFunctionBindingMethod::NativeRuntimeLinked;

	UPROPERTY(Config, EditAnywhere, Category = "Bindings", Meta = (ConfigRestartRequired = true))
	TArray<FName> NativeRuntimeLinkedModules;

	UPROPERTY(Config, EditAnywhere, Category = "Bindings", Meta = (ConfigRestartRequired = true))
	TArray<FName> NativeModuleFunctionAddressModules;
};
