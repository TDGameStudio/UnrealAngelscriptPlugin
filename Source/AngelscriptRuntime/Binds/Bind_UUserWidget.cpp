#include "Bind_UUserWidget.h"

#include "AngelscriptBinds.h"
#include "Blueprint/UserWidget.h"
#include "Components/Widget.h"

/**
 * User-widget, paint-context, Slate color, and Slate brush binding surface.
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | AngelScript usage signature                                                                | Purpose / parameter notes                                                                                            |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FText UUserWidget.GetPaletteCategory() const;                                              | Returns the editor palette category assigned to this widget class.                                                   |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | void UUserWidget.SetPaletteCategory(const FText& InPaletteCategory);                       | Sets the editor palette category assigned to this widget class.                                                      |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | UWidget UUserWidget.GetRootWidget() const;                                                 | Returns the widget-tree root widget.                                                                                 |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | void UUserWidget.SetRootWidget(UWidget NewRootWidget);                                     | Replaces the widget-tree root widget.                                                                                |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | UWidget UUserWidget.ConstructWidget(const TSubclassOf<UWidget>& WidgetClass,               | Constructs a child widget typed to WidgetClass in this widget tree.                                                  |
 * |     FName WidgetName = NAME_None);                                                         | @param WidgetName Optional stable object name for the new widget.                                                    |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | bool UUserWidget.RemoveWidget(UWidget WidgetToRemove);                                     | Removes a widget from this widget tree.                                                                              |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | void UUserWidget.AddToViewport(int32 ZOrder = 0);                                          | Adds the user widget to the game viewport at the requested Z order.                                                  |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | void UUserWidget.GetAllWidgets(TArray<UWidget>& Widgets) const;                            | Collects every widget in this widget tree.                                                                           |
 * |                                                                                            | @param Widgets Receives the widget array.                                                                            |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | const FGeometry& FPaintContext.GetAllottedGeometry() const;                                | Returns the geometry allotted to the current paint pass.                                                             |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FLinearColor FPaintContext.GetStyleColor(const FName& Color) const;                        | Resolves a named color from the active core style set.                                                               |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | const FSlateBrush& FPaintContext.GetStyleBrush(const FName& Brush) const;                  | Resolves a named brush from the active core style set.                                                               |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FSlateFontInfo FPaintContext.GetStyleFont(int32 Size) const;                               | Returns the default core-style font at Size points.                                                                  |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | void FPaintContext.DrawBox(const FVector2D& Position, const FVector2D& Size,               | Draws a solid box in local paint coordinates.                                                                        |
 * |     const FLinearColor& Color);                                                            |                                                                                                                      |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | void FPaintContext.DrawBox(const FVector2D& Position, const FVector2D& Size,               | Draws a named style brush in local paint coordinates with an optional tint.                                          |
 * |     const FName& BrushName, const FLinearColor& TintColor = FLinearColor::White);          |                                                                                                                      |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | void FPaintContext.DrawBox(const FVector2D& Position, const FVector2D& Size,               | Draws a supplied brush in local paint coordinates with an optional tint.                                             |
 * |     const FSlateBrush& Brush, const FLinearColor& TintColor = FLinearColor::White);        |                                                                                                                      |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | void FPaintContext.DrawBox(const FGeometry& Geometry, const FSlateBrush& Brush,            | Draws a supplied brush using the provided paint geometry.                                                            |
 * |     const FLinearColor& TintColor = FLinearColor::White);                                  |                                                                                                                      |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | void FPaintContext.DrawBox(const FVector2D& Position, const FVector2D& Size,               | Draws a brush asset in local paint coordinates when Brush is valid.                                                  |
 * |     USlateBrushAsset Brush, const FLinearColor& TintColor = FLinearColor::White);          |                                                                                                                      |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | void FPaintContext.DrawRotatedBox(const FVector2D& Position, const FVector2D& Size,        | Draws a rotated brush box in local paint coordinates.                                                                |
 * |     float32 Angle, const FSlateBrush& Brush,                                               | @param Angle Slate draw-element rotation passed through unchanged.                                                   |
 * |     const FLinearColor& TintColor = FLinearColor::White);                                  |                                                                                                                      |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | void FPaintContext.DrawLine(const FVector2D& PositionA, const FVector2D& PositionB,        | Draws one local-space line segment.                                                                                  |
 * |     const FLinearColor& Color, float32 Thickness = 1.f, bool bAntiAlias = true);           | @param Thickness Line width in Slate units. @param bAntiAlias Enables line antialiasing.                             |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | void FPaintContext.DrawLines(const TArray<FVector2D>& Points, const FLinearColor& Color,   | Draws a connected local-space line strip.                                                                            |
 * |     float32 Thickness = 1.f, bool bAntiAlias = true);                                      | @param Points Ordered line-strip vertices. @param Thickness Line width in Slate units.                               |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | void FPaintContext.DrawText(const FString& Text, const FVector2D& Position,                | Draws text with the default font at a local paint position.                                                          |
 * |     const FLinearColor& Color);                                                            |                                                                                                                      |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | void FPaintContext.DrawText(const FSlateFontInfo& Font, const FString& Text,               | Draws text with the supplied font at a local paint position.                                                         |
 * |     const FVector2D& Position, const FLinearColor& Color);                                 |                                                                                                                      |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | UUserWidget WidgetBlueprint::CreateWidget(                                                 | Creates a user widget typed to WidgetClass for OwningPlayer in the current world.                                    |
 * |     const TSubclassOf<UUserWidget>& WidgetClass, APlayerController OwningPlayer);          |                                                                                                                      |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FSlateColor Color(const FLinearColor& InColor);                                            | Implicitly constructs a fixed Slate color from a linear color.                                                       |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FSlateColor Color(const FColor& InColor);                                                  | Implicitly constructs a fixed Slate color from an 8-bit color.                                                       |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FSlateColor Color(EStyleColor InColorTableId);                                             | Implicitly constructs a color linked to a style color-table entry.                                                   |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FSlateBrush Brush(const FName& BrushStyleName);                                            | Implicitly constructs a copy of a named core-style brush.                                                            |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FSlateBrush Brush(const FLinearColor& Color);                                              | Implicitly constructs a solid-color brush.                                                                           |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FSlateBrush Brush(UTexture2D Texture, const FVector2D& ImageSize,                          | Constructs an image brush from a texture.                                                                            |
 * |     const FLinearColor& Tint = FLinearColor::White);                                       | @param ImageSize Desired Slate image size.                                                                           |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FSlateBrush Brush(UMaterialInterface Material, const FVector2D& ImageSize,                 | Constructs an image brush from a material.                                                                           |
 * |     const FLinearColor& Tint = FLinearColor::White);                                       | @param ImageSize Desired Slate image size.                                                                           |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 */

namespace
{
	void BindUserWidget(FAngelscriptBinds& Binds)
	{
		auto UserWidget = Binds.ExistingClassForTarget("UUserWidget");
		UserWidget.Method("FText GetPaletteCategory() const", &FAngelscriptUUserWidgetBinds::GetPaletteCategory);
		UserWidget.Method("void SetPaletteCategory(const FText& InPaletteCategory)", &FAngelscriptUUserWidgetBinds::SetPaletteCategory);
		UserWidget.Method("UWidget GetRootWidget() const", &FAngelscriptUUserWidgetBinds::GetRootWidget);
		UserWidget.Method("void SetRootWidget(UWidget NewRootWidget)", &FAngelscriptUUserWidgetBinds::SetRootWidget);
		UserWidget.Method(
			"UWidget ConstructWidget(const TSubclassOf<UWidget>& WidgetClass, FName WidgetName = NAME_None)",
			&FAngelscriptUUserWidgetBinds::ConstructWidget)
			.DeterminesOutputType(0);
		UserWidget.Method("bool RemoveWidget(UWidget WidgetToRemove)", &FAngelscriptUUserWidgetBinds::RemoveWidget);
		UserWidget.Method("void AddToViewport(int32 ZOrder = 0)", METHOD_TRIVIAL(UUserWidget, AddToViewport));
		UserWidget.Method("void GetAllWidgets(TArray<UWidget>& Widgets) const", &FAngelscriptUUserWidgetBinds::GetAllWidgets);

		auto PaintContext = Binds.ExistingClassForTarget("FPaintContext");
		PaintContext.Method("const FGeometry& GetAllottedGeometry() const", &FAngelscriptUUserWidgetBinds::GetAllottedGeometry);
		PaintContext.Method("FLinearColor GetStyleColor(const FName& Color) const", &FAngelscriptUUserWidgetBinds::GetStyleColor);
		PaintContext.Method("const FSlateBrush& GetStyleBrush(const FName& Brush) const", &FAngelscriptUUserWidgetBinds::GetStyleBrush);
		PaintContext.Method("FSlateFontInfo GetStyleFont(int32 Size) const", &FAngelscriptUUserWidgetBinds::GetStyleFont);
		PaintContext.Method(
			"void DrawBox(const FVector2D& Position, const FVector2D& Size, const FLinearColor& Color)",
			&FAngelscriptUUserWidgetBinds::DrawBoxColor);
		PaintContext.Method(
			"void DrawBox(const FVector2D& Position, const FVector2D& Size, const FName& BrushName, const FLinearColor& TintColor = FLinearColor::White)",
			&FAngelscriptUUserWidgetBinds::DrawBoxStyle);
		PaintContext.Method(
			"void DrawBox(const FVector2D& Position, const FVector2D& Size, const FSlateBrush& Brush, const FLinearColor& TintColor = FLinearColor::White)",
			&FAngelscriptUUserWidgetBinds::DrawBoxBrush);
		PaintContext.Method(
			"void DrawBox(const FGeometry& Geometry, const FSlateBrush& Brush, const FLinearColor& TintColor = FLinearColor::White)",
			&FAngelscriptUUserWidgetBinds::DrawBoxGeometry);
		PaintContext.Method(
			"void DrawBox(const FVector2D& Position, const FVector2D& Size, USlateBrushAsset Brush, const FLinearColor& TintColor = FLinearColor::White)",
			&FAngelscriptUUserWidgetBinds::DrawBoxAsset);
		PaintContext.Method(
			"void DrawRotatedBox(const FVector2D& Position, const FVector2D& Size, float32 Angle, const FSlateBrush& Brush, const FLinearColor& TintColor = FLinearColor::White)",
			&FAngelscriptUUserWidgetBinds::DrawRotatedBox);
		PaintContext.Method(
			"void DrawLine(const FVector2D& PositionA, const FVector2D& PositionB, const FLinearColor& Color, float32 Thickness = 1.f, bool bAntiAlias = true)",
			&FAngelscriptUUserWidgetBinds::DrawLine);
		PaintContext.Method(
			"void DrawLines(const TArray<FVector2D>& Points, const FLinearColor& Color, float32 Thickness = 1.f, bool bAntiAlias = true)",
			&FAngelscriptUUserWidgetBinds::DrawLines);
		PaintContext.Method(
			"void DrawText(const FString& Text, const FVector2D& Position, const FLinearColor& Color)",
			&FAngelscriptUUserWidgetBinds::DrawTextDefaultFont);
		PaintContext.Method(
			"void DrawText(const FSlateFontInfo& Font, const FString& Text, const FVector2D& Position, const FLinearColor& Color)",
			&FAngelscriptUUserWidgetBinds::DrawTextFont);

		FAngelscriptBinds::FNamespace WidgetBlueprintNamespace(Binds.GetTargetEngine(), "WidgetBlueprint");
		Binds.BindGlobalFunctionForTarget(
			"UUserWidget CreateWidget(const TSubclassOf<UUserWidget>& WidgetClass, APlayerController OwningPlayer)",
			&FAngelscriptUUserWidgetBinds::CreateWidget)
			.WorldContext();

		auto SlateColor = Binds.ExistingClassForTarget("FSlateColor");
		SlateColor.ImplicitConstructor(
			"void f(const FLinearColor& InColor)",
			&FAngelscriptUUserWidgetBinds::ConstructSlateColorLinear)
			.NoDiscard();
		SlateColor.ImplicitConstructor(
			"void f(const FColor& InColor)",
			&FAngelscriptUUserWidgetBinds::ConstructSlateColorColor)
			.NoDiscard();
		SlateColor.ImplicitConstructor(
			"void f(EStyleColor InColorTableId)",
			&FAngelscriptUUserWidgetBinds::ConstructSlateColorStyle)
			.NoDiscard();

#if WITH_EDITOR
		// GetIsVisible() conflicts with IsVisible() when both are interpreted as property accessors.
		if (UFunction* GetIsVisible = FindObject<UFunction>(nullptr, TEXT("/Script/UMG.UserWidget:GetIsVisible")))
		{
			GetIsVisible->SetMetaData(TEXT("NotInAngelscript"), TEXT("true"));
		}
#endif

		auto SlateBrush = Binds.ExistingClassForTarget("FSlateBrush");
		SlateBrush.ImplicitConstructor(
			"void f(const FName& BrushStyleName)",
			&FAngelscriptUUserWidgetBinds::ConstructSlateBrushStyle)
			.NoDiscard();
		SlateBrush.ImplicitConstructor(
			"void f(const FLinearColor& Color)",
			&FAngelscriptUUserWidgetBinds::ConstructSlateBrushColor)
			.NoDiscard();
		SlateBrush.Constructor(
			"void f(UTexture2D Texture, const FVector2D& ImageSize, const FLinearColor& Tint = FLinearColor::White)",
			&FAngelscriptUUserWidgetBinds::ConstructSlateBrushTexture)
			.NoDiscard();
		SlateBrush.Constructor(
			"void f(UMaterialInterface Material, const FVector2D& ImageSize, const FLinearColor& Tint = FLinearColor::White)",
			&FAngelscriptUUserWidgetBinds::ConstructSlateBrushMaterial)
			.NoDiscard();
	}
}

AS_FORCE_LINK const FAngelscriptBind Bind_UUserWidget(
	TEXT("UUserWidget.Manual"),
	EAngelscriptBindPhase::ManualBindings,
	&BindUserWidget);
