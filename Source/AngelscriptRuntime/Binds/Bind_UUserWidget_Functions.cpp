#include "Bind_UUserWidget.h"

#include "AngelscriptEngine.h"

#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Brushes/SlateColorBrush.h"
#include "Brushes/SlateImageBrush.h"
#include "Engine/Texture2D.h"
#include "Fonts/SlateFontInfo.h"
#include "Materials/MaterialInterface.h"
#include "Rendering/DrawElements.h"
#include "Slate/SlateBrushAsset.h"
#include "Styling/CoreStyle.h"

namespace
{
	const FName NAME_Slate_WhiteBrush(TEXT("WhiteBrush"));

	FLinearColor GetPaintTint(const FPaintContext& PaintContext, const FLinearColor& TintColor)
	{
		return PaintContext.WidgetStyle.GetColorAndOpacityTint() * TintColor;
	}
}

FText FAngelscriptUUserWidgetBinds::GetPaletteCategory(const UUserWidget* Widget)
{
#if WITH_EDITORONLY_DATA
	return Widget->PaletteCategory;
#else
	return FText();
#endif
}

void FAngelscriptUUserWidgetBinds::SetPaletteCategory(UUserWidget* Widget, const FText& InPaletteCategory)
{
#if WITH_EDITORONLY_DATA
	Widget->PaletteCategory = InPaletteCategory;
#endif
}

UWidget* FAngelscriptUUserWidgetBinds::GetRootWidget(const UUserWidget* Widget)
{
	return Widget->GetRootWidget();
}

void FAngelscriptUUserWidgetBinds::SetRootWidget(UUserWidget* Widget, UWidget* NewRootWidget)
{
	if (Widget->WidgetTree)
	{
		Widget->WidgetTree->RootWidget = NewRootWidget;
	}
}

UWidget* FAngelscriptUUserWidgetBinds::ConstructWidget(
	UUserWidget* Widget,
	const TSubclassOf<UWidget>& WidgetClass,
	FName WidgetName)
{
	UClass* ResolvedWidgetClass = WidgetClass.Get();
	if (Widget->WidgetTree
		&& ensureMsgf(
			ResolvedWidgetClass && ResolvedWidgetClass->IsChildOf(UWidget::StaticClass()),
			TEXT("Widget Class must be a subclass of UWidget!")))
	{
		return Widget->WidgetTree->ConstructWidget<UWidget>(ResolvedWidgetClass, WidgetName);
	}

	return nullptr;
}

bool FAngelscriptUUserWidgetBinds::RemoveWidget(UUserWidget* Widget, UWidget* WidgetToRemove)
{
	return Widget->WidgetTree ? Widget->WidgetTree->RemoveWidget(WidgetToRemove) : false;
}

void FAngelscriptUUserWidgetBinds::GetAllWidgets(const UUserWidget* Widget, TArray<UWidget*>& Widgets)
{
	if (Widget->WidgetTree)
	{
		Widget->WidgetTree->GetAllWidgets(Widgets);
	}
}

const FGeometry& FAngelscriptUUserWidgetBinds::GetAllottedGeometry(FPaintContext& PaintContext)
{
	return PaintContext.AllottedGeometry;
}

FLinearColor FAngelscriptUUserWidgetBinds::GetStyleColor(FPaintContext& PaintContext, const FName& ColorName)
{
	return FCoreStyle::Get().GetColor(ColorName);
}

const FSlateBrush& FAngelscriptUUserWidgetBinds::GetStyleBrush(FPaintContext& PaintContext, const FName& BrushName)
{
	if (const FSlateBrush* Brush = FCoreStyle::Get().GetBrush(BrushName))
	{
		return *Brush;
	}

	static FSlateBrush NoBrush;
	return NoBrush;
}

FSlateFontInfo FAngelscriptUUserWidgetBinds::GetStyleFont(FPaintContext& PaintContext, int32 Size)
{
	return FSlateFontInfo(FCoreStyle::GetDefaultFont(), Size);
}

void FAngelscriptUUserWidgetBinds::DrawBoxColor(
	FPaintContext& PaintContext,
	const FVector2D& Position,
	const FVector2D& Size,
	const FLinearColor& Color)
{
	PaintContext.MaxLayer++;
	if (const FSlateBrush* WhiteBrush = FCoreStyle::Get().GetBrush(NAME_Slate_WhiteBrush))
	{
		FSlateDrawElement::MakeBox(
			PaintContext.OutDrawElements,
			PaintContext.MaxLayer,
			PaintContext.AllottedGeometry.ToPaintGeometry(FVector2f(Size), FSlateLayoutTransform(FVector2f(Position))),
			WhiteBrush,
			ESlateDrawEffect::None,
			GetPaintTint(PaintContext, Color));
	}
}

void FAngelscriptUUserWidgetBinds::DrawBoxStyle(
	FPaintContext& PaintContext,
	const FVector2D& Position,
	const FVector2D& Size,
	const FName& BrushName,
	const FLinearColor& TintColor)
{
	PaintContext.MaxLayer++;
	if (const FSlateBrush* SlateBrush = FCoreStyle::Get().GetBrush(BrushName))
	{
		FSlateDrawElement::MakeBox(
			PaintContext.OutDrawElements,
			PaintContext.MaxLayer,
			PaintContext.AllottedGeometry.ToPaintGeometry(FVector2f(Size), FSlateLayoutTransform(FVector2f(Position))),
			SlateBrush,
			ESlateDrawEffect::None,
			GetPaintTint(PaintContext, TintColor) * SlateBrush->TintColor.GetColor(PaintContext.WidgetStyle));
	}
}

void FAngelscriptUUserWidgetBinds::DrawBoxBrush(
	FPaintContext& PaintContext,
	const FVector2D& Position,
	const FVector2D& Size,
	const FSlateBrush& Brush,
	const FLinearColor& TintColor)
{
	PaintContext.MaxLayer++;
	FSlateDrawElement::MakeBox(
		PaintContext.OutDrawElements,
		PaintContext.MaxLayer,
		PaintContext.AllottedGeometry.ToPaintGeometry(FVector2f(Size), FSlateLayoutTransform(FVector2f(Position))),
		&Brush,
		ESlateDrawEffect::None,
		GetPaintTint(PaintContext, TintColor) * Brush.TintColor.GetColor(PaintContext.WidgetStyle));
}

void FAngelscriptUUserWidgetBinds::DrawBoxGeometry(
	FPaintContext& PaintContext,
	const FGeometry& Geometry,
	const FSlateBrush& Brush,
	const FLinearColor& TintColor)
{
	PaintContext.MaxLayer++;
	FSlateDrawElement::MakeBox(
		PaintContext.OutDrawElements,
		PaintContext.MaxLayer,
		Geometry.ToPaintGeometry(),
		&Brush,
		ESlateDrawEffect::None,
		GetPaintTint(PaintContext, TintColor) * Brush.TintColor.GetColor(PaintContext.WidgetStyle));
}

void FAngelscriptUUserWidgetBinds::DrawBoxAsset(
	FPaintContext& PaintContext,
	const FVector2D& Position,
	const FVector2D& Size,
	USlateBrushAsset* Brush,
	const FLinearColor& TintColor)
{
	PaintContext.MaxLayer++;
	if (Brush)
	{
		FSlateDrawElement::MakeBox(
			PaintContext.OutDrawElements,
			PaintContext.MaxLayer,
			PaintContext.AllottedGeometry.ToPaintGeometry(FVector2f(Size), FSlateLayoutTransform(FVector2f(Position))),
			&Brush->Brush,
			ESlateDrawEffect::None,
			GetPaintTint(PaintContext, TintColor) * PaintContext.WidgetStyle.GetForegroundColor());
	}
}

void FAngelscriptUUserWidgetBinds::DrawRotatedBox(
	FPaintContext& PaintContext,
	const FVector2D& Position,
	const FVector2D& Size,
	float Angle,
	const FSlateBrush& Brush,
	const FLinearColor& TintColor)
{
	PaintContext.MaxLayer++;
	FSlateDrawElement::MakeRotatedBox(
		PaintContext.OutDrawElements,
		PaintContext.MaxLayer,
		PaintContext.AllottedGeometry.ToPaintGeometry(FVector2f(Size), FSlateLayoutTransform(FVector2f(Position))),
		&Brush,
		ESlateDrawEffect::None,
		Angle,
		TOptional<FVector2f>(),
		FSlateDrawElement::RelativeToElement,
		GetPaintTint(PaintContext, TintColor) * Brush.TintColor.GetColor(PaintContext.WidgetStyle));
}

void FAngelscriptUUserWidgetBinds::DrawLine(
	FPaintContext& PaintContext,
	const FVector2D& PositionA,
	const FVector2D& PositionB,
	const FLinearColor& Color,
	float Thickness,
	bool bAntiAlias)
{
	PaintContext.MaxLayer++;
	TArray<FVector2f> Points{FVector2f(PositionA), FVector2f(PositionB)};
	FSlateDrawElement::MakeLines(
		PaintContext.OutDrawElements,
		PaintContext.MaxLayer,
		PaintContext.AllottedGeometry.ToPaintGeometry(),
		Points,
		ESlateDrawEffect::None,
		GetPaintTint(PaintContext, Color),
		bAntiAlias,
		Thickness);
}

void FAngelscriptUUserWidgetBinds::DrawLines(
	FPaintContext& PaintContext,
	const TArray<FVector2D>& Points,
	const FLinearColor& Color,
	float Thickness,
	bool bAntiAlias)
{
	PaintContext.MaxLayer++;
	TArray<FVector2f> FloatPoints;
	FloatPoints.Reserve(Points.Num());
	for (const FVector2D& Point : Points)
	{
		FloatPoints.Add(FVector2f(Point));
	}
	FSlateDrawElement::MakeLines(
		PaintContext.OutDrawElements,
		PaintContext.MaxLayer,
		PaintContext.AllottedGeometry.ToPaintGeometry(),
		FloatPoints,
		ESlateDrawEffect::None,
		GetPaintTint(PaintContext, Color),
		bAntiAlias,
		Thickness);
}

void FAngelscriptUUserWidgetBinds::DrawTextDefaultFont(
	FPaintContext& PaintContext,
	const FString& InString,
	const FVector2D& Position,
	const FLinearColor& Color)
{
	const FSlateFontInfo FontInfo = FCoreStyle::Get().GetWidgetStyle<FTextBlockStyle>("NormalText").Font;
	DrawTextFont(PaintContext, FontInfo, InString, Position, Color);
}

void FAngelscriptUUserWidgetBinds::DrawTextFont(
	FPaintContext& PaintContext,
	const FSlateFontInfo& FontInfo,
	const FString& InString,
	const FVector2D& Position,
	const FLinearColor& Color)
{
	PaintContext.MaxLayer++;
	FSlateDrawElement::MakeText(
		PaintContext.OutDrawElements,
		PaintContext.MaxLayer,
		PaintContext.AllottedGeometry.ToOffsetPaintGeometry(FVector2f(Position)),
		InString,
		FontInfo,
		ESlateDrawEffect::None,
		GetPaintTint(PaintContext, Color));
}

UUserWidget* FAngelscriptUUserWidgetBinds::CreateWidget(
	const TSubclassOf<UUserWidget>& WidgetClass,
	APlayerController* OwningPlayer)
{
	return UWidgetBlueprintLibrary::Create(
		FAngelscriptEngine::TryGetCurrentWorldContextObject(),
		WidgetClass,
		OwningPlayer);
}

void FAngelscriptUUserWidgetBinds::ConstructSlateColorLinear(FSlateColor* SlateColor, const FLinearColor& Color)
{
	new (SlateColor) FSlateColor(Color);
}

void FAngelscriptUUserWidgetBinds::ConstructSlateColorColor(FSlateColor* SlateColor, const FColor& Color)
{
	new (SlateColor) FSlateColor(Color);
}

void FAngelscriptUUserWidgetBinds::ConstructSlateColorStyle(FSlateColor* SlateColor, EStyleColor Color)
{
	new (SlateColor) FSlateColor(Color);
}

void FAngelscriptUUserWidgetBinds::ConstructSlateBrushStyle(FSlateBrush* Brush, const FName& BrushStyleName)
{
	if (const FSlateBrush* StyleBrush = FCoreStyle::Get().GetBrush(BrushStyleName))
	{
		new (Brush) FSlateBrush(*StyleBrush);
	}
	else
	{
		new (Brush) FSlateBrush();
	}
}

void FAngelscriptUUserWidgetBinds::ConstructSlateBrushColor(FSlateBrush* Brush, const FLinearColor& Color)
{
	new (Brush) FSlateColorBrush(Color);
}

void FAngelscriptUUserWidgetBinds::ConstructSlateBrushTexture(
	FSlateBrush* Brush,
	UTexture2D* Texture,
	const FVector2D& ImageSize,
	const FLinearColor& Tint)
{
	new (Brush) FSlateImageBrush(Texture, FVector2f(ImageSize), Tint);
}

void FAngelscriptUUserWidgetBinds::ConstructSlateBrushMaterial(
	FSlateBrush* Brush,
	UMaterialInterface* Material,
	const FVector2D& ImageSize,
	const FLinearColor& Tint)
{
	new (Brush) FSlateImageBrush(Material, FVector2f(ImageSize), Tint);
}
