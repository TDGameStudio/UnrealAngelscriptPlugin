#pragma once

#include "CoreMinimal.h"

#include "AngelscriptCompileOptions.generated.h"

UCLASS(Config=AngelscriptCompileOptions, DefaultConfig, meta = (DisplayName = "Angelscript Compile Options"))
class ANGELSCRIPTRUNTIME_API UAngelscriptCompileOptions : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(Config, EditAnywhere, Category = "Tests", Meta = (ConfigRestartRequired = true))
	bool bCompileAngelscriptUnitTests = false;
};
