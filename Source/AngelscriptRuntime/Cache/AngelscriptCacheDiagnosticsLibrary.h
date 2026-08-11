#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"

#include "AngelscriptCacheDiagnosticsLibrary.generated.h"

UCLASS()
class ANGELSCRIPTRUNTIME_API UAngelscriptCacheDiagnosticsLibrary final
	: public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// Returns the exact Engine-native schema consumed by console and optional
	// Python correlation. This is an observer and never mutates cache state.
	UFUNCTION(BlueprintCallable, Category = "AngelScript|Cache",
		meta = (DisplayName = "Get AngelScript Cache Status JSON"))
	static bool GetCacheStatusJson(
		FString& OutStatusJson,
		FString& OutError);
};
