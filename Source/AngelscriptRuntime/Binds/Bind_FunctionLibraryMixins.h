#pragma once

#include "CoreMinimal.h"
#include "Curves/CurveLinearColor.h"
#include "FunctionLibraries/RuntimeFloatCurveMixinLibrary.h"

class ULevelStreaming;
class USceneComponent;

struct FAngelscriptFunctionLibraryMixinsBinds
{
#if WITH_EDITOR
	static bool GetShouldBeVisibleInEditor(const ULevelStreaming* LevelStreaming);
#endif
	static void SetRelativeRotation(USceneComponent* Component, const FRotator& NewRotation);
	static bool IsAttachedToComponent(
		const USceneComponent* Component,
		const USceneComponent* CheckComponent);

	static void AddRuntimeCurveLinearColorKey(
		FRuntimeCurveLinearColor* Target,
		float InTime,
		const FLinearColor& InColor);
	static void AddRuntimeFloatCurveKey(FRuntimeFloatCurve* Target, float InTime, float InValue);
	static int32 GetRuntimeFloatCurveNumKeys(const FRuntimeFloatCurve* Target);
	static void GetRuntimeFloatCurveTimeRange(
		const FRuntimeFloatCurve* Target,
		float& MinTime,
		float& MaxTime);

	static FCurveKeyHandle AddAutoCurveKey(UCurveFloat* Curve, float InTime, float InValue);
	static void SetKeyInterpMode(
		UCurveFloat* Curve,
		FCurveKeyHandle KeyHandle,
		ERichCurveInterpMode NewInterpMode,
		bool bAutoSetTangents);

	static void AddRuntimeCurveLinearColorKeyGlobal(
		FRuntimeCurveLinearColor& Target,
		float InTime,
		const FLinearColor& InColor);
	static void GetRuntimeFloatCurveTimeRangeGlobal(
		const FRuntimeFloatCurve& Target,
		float& MinTime,
		float& MaxTime);
};
