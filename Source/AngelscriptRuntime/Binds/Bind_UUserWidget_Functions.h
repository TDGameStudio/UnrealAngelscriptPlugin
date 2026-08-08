#pragma once

#include "CoreMinimal.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Styling/SlateBrush.h"
#include "Styling/SlateColor.h"

class APlayerController;
class UMaterialInterface;
class USlateBrushAsset;
class UTexture2D;
class UUserWidget;
class UWidget;
struct FSlateFontInfo;

struct FAngelscriptUUserWidgetBinds
{
	static FText GetPaletteCategory(const UUserWidget* Widget);
	static void SetPaletteCategory(UUserWidget* Widget, const FText& InPaletteCategory);
	static UWidget* GetRootWidget(const UUserWidget* Widget);
	static void SetRootWidget(UUserWidget* Widget, UWidget* NewRootWidget);
	static UWidget* ConstructWidget(UUserWidget* Widget, const TSubclassOf<UWidget>& WidgetClass, FName WidgetName);
	static bool RemoveWidget(UUserWidget* Widget, UWidget* WidgetToRemove);
	static void GetAllWidgets(const UUserWidget* Widget, TArray<UWidget*>& Widgets);

	static const FGeometry& GetAllottedGeometry(FPaintContext& PaintContext);
	static FLinearColor GetStyleColor(FPaintContext& PaintContext, const FName& ColorName);
	static const FSlateBrush& GetStyleBrush(FPaintContext& PaintContext, const FName& BrushName);
	static FSlateFontInfo GetStyleFont(FPaintContext& PaintContext, int32 Size);
	static void DrawBoxColor(FPaintContext& PaintContext, const FVector2D& Position, const FVector2D& Size, const FLinearColor& Color);
	static void DrawBoxStyle(FPaintContext& PaintContext, const FVector2D& Position, const FVector2D& Size, const FName& BrushName, const FLinearColor& TintColor);
	static void DrawBoxBrush(FPaintContext& PaintContext, const FVector2D& Position, const FVector2D& Size, const FSlateBrush& Brush, const FLinearColor& TintColor);
	static void DrawBoxGeometry(FPaintContext& PaintContext, const FGeometry& Geometry, const FSlateBrush& Brush, const FLinearColor& TintColor);
	static void DrawBoxAsset(FPaintContext& PaintContext, const FVector2D& Position, const FVector2D& Size, USlateBrushAsset* Brush, const FLinearColor& TintColor);
	static void DrawRotatedBox(FPaintContext& PaintContext, const FVector2D& Position, const FVector2D& Size, float Angle, const FSlateBrush& Brush, const FLinearColor& TintColor);
	static void DrawLine(FPaintContext& PaintContext, const FVector2D& PositionA, const FVector2D& PositionB, const FLinearColor& Color, float Thickness, bool bAntiAlias);
	static void DrawLines(FPaintContext& PaintContext, const TArray<FVector2D>& Points, const FLinearColor& Color, float Thickness, bool bAntiAlias);
	static void DrawTextDefaultFont(FPaintContext& PaintContext, const FString& InString, const FVector2D& Position, const FLinearColor& Color);
	static void DrawTextFont(FPaintContext& PaintContext, const FSlateFontInfo& FontInfo, const FString& InString, const FVector2D& Position, const FLinearColor& Color);

	static UUserWidget* CreateWidget(const TSubclassOf<UUserWidget>& WidgetClass, APlayerController* OwningPlayer);
	static void ConstructSlateColorLinear(FSlateColor* SlateColor, const FLinearColor& Color);
	static void ConstructSlateColorColor(FSlateColor* SlateColor, const FColor& Color);
	static void ConstructSlateColorStyle(FSlateColor* SlateColor, EStyleColor Color);
	static void ConstructSlateBrushStyle(FSlateBrush* Brush, const FName& BrushStyleName);
	static void ConstructSlateBrushColor(FSlateBrush* Brush, const FLinearColor& Color);
	static void ConstructSlateBrushTexture(FSlateBrush* Brush, UTexture2D* Texture, const FVector2D& ImageSize, const FLinearColor& Tint);
	static void ConstructSlateBrushMaterial(FSlateBrush* Brush, UMaterialInterface* Material, const FVector2D& ImageSize, const FLinearColor& Tint);
};
