#pragma once
#include "CoreMinimal.h"
#include "Components/Widget.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "WidgetBlueprintStatics.generated.h"

/**
 * Expands the WidgetBlueprint:: namespace in script with static
 * functions that are handled by custom nodes in blueprint, and
 * aren't automatically bound as a result.
 */
UCLASS()
class UWidgetBlueprintStatics : public UObject
{
	GENERATED_BODY()

public:

	UFUNCTION(BlueprintCallable, Meta = (WorldContext = "WorldContextObject", DeterminesOutputType = "WidgetType"))
	static class UUserWidget* CreateWidget(UObject* WorldContextObject, TSubclassOf<class UUserWidget> WidgetType, APlayerController* OwningPlayer)
	{
		return UWidgetBlueprintLibrary::Create(WorldContextObject, WidgetType, OwningPlayer);
	}

};

UCLASS(Meta = (ScriptMixin = "UWidget"))
class UAngelscriptWidgetMixinLibrary : public UObject
{
	GENERATED_BODY()

public:

	UFUNCTION(BlueprintCallable, Meta = (WorldContext = "WorldContextObject"))
	static const FWidgetTransform& GetRenderTransform(UWidget* Widget)
	{
		return Widget->GetRenderTransform();
	}

};
