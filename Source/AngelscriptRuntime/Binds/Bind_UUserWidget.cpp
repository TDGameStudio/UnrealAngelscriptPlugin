#include "AngelscriptBinds.h"
#include "Bind_UUserWidget_Functions.h"

#include "Blueprint/UserWidget.h"
#include "Components/Widget.h"

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
