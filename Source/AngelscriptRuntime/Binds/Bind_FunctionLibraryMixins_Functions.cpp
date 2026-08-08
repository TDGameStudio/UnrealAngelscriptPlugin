#include "Bind_FunctionLibraryMixins_Functions.h"

#include "Components/SceneComponent.h"
#include "Engine/LevelStreaming.h"
#include "FunctionLibraries/AngelscriptComponentLibrary.h"
#include "FunctionLibraries/AngelscriptLevelStreamingLibrary.h"
#include "FunctionLibraries/RuntimeCurveLinearColorMixinLibrary.h"
#include "FunctionLibraries/RuntimeFloatCurveMixinLibrary.h"

#if WITH_EDITOR
bool FAngelscriptFunctionLibraryMixinsBinds::GetShouldBeVisibleInEditor(const ULevelStreaming* LevelStreaming)
{
	return UAngelscriptLevelStreamingLibrary::GetShouldBeVisibleInEditor(LevelStreaming);
}
#endif

void FAngelscriptFunctionLibraryMixinsBinds::SetRelativeRotation(
	USceneComponent* Component,
	const FRotator& NewRotation)
{
	UAngelscriptComponentLibrary::SetRelativeRotation(Component, NewRotation);
}

bool FAngelscriptFunctionLibraryMixinsBinds::IsAttachedToComponent(
	const USceneComponent* Component,
	const USceneComponent* CheckComponent)
{
	return UAngelscriptComponentLibrary::IsAttachedTo(Component, CheckComponent);
}

void FAngelscriptFunctionLibraryMixinsBinds::AddRuntimeCurveLinearColorKey(
	FRuntimeCurveLinearColor* Target,
	const float InTime,
	const FLinearColor& InColor)
{
	Target->ColorCurves[0].AddKey(InTime, InColor.R);
	Target->ColorCurves[1].AddKey(InTime, InColor.G);
	Target->ColorCurves[2].AddKey(InTime, InColor.B);
	Target->ColorCurves[3].AddKey(InTime, InColor.A);
}

void FAngelscriptFunctionLibraryMixinsBinds::AddRuntimeFloatCurveKey(
	FRuntimeFloatCurve* Target,
	const float InTime,
	const float InValue)
{
	URuntimeFloatCurveMixinLibrary::AddDefaultKey(*Target, InTime, InValue);
}

int32 FAngelscriptFunctionLibraryMixinsBinds::GetRuntimeFloatCurveNumKeys(const FRuntimeFloatCurve* Target)
{
	return URuntimeFloatCurveMixinLibrary::GetNumKeys(*Target);
}

void FAngelscriptFunctionLibraryMixinsBinds::GetRuntimeFloatCurveTimeRange(
	const FRuntimeFloatCurve* Target,
	float& MinTime,
	float& MaxTime)
{
	URuntimeFloatCurveMixinLibrary::GetTimeRange(*Target, MinTime, MaxTime);
}

FCurveKeyHandle FAngelscriptFunctionLibraryMixinsBinds::AddAutoCurveKey(
	UCurveFloat* Curve,
	const float InTime,
	const float InValue)
{
	return URuntimeFloatCurveMixinLibrary::AddAutoCurveKey(Curve, InTime, InValue);
}

void FAngelscriptFunctionLibraryMixinsBinds::SetKeyInterpMode(
	UCurveFloat* Curve,
	const FCurveKeyHandle KeyHandle,
	const ERichCurveInterpMode NewInterpMode,
	const bool bAutoSetTangents)
{
	URuntimeFloatCurveMixinLibrary::SetKeyInterpMode(Curve, KeyHandle, NewInterpMode, bAutoSetTangents);
}

void FAngelscriptFunctionLibraryMixinsBinds::AddRuntimeCurveLinearColorKeyGlobal(
	FRuntimeCurveLinearColor& Target,
	const float InTime,
	const FLinearColor& InColor)
{
	URuntimeCurveLinearColorMixinLibrary::AddDefaultKey(Target, InTime, InColor);
}

void FAngelscriptFunctionLibraryMixinsBinds::GetRuntimeFloatCurveTimeRangeGlobal(
	const FRuntimeFloatCurve& Target,
	float& MinTime,
	float& MaxTime)
{
	URuntimeFloatCurveMixinLibrary::GetTimeRange(Target, MinTime, MaxTime);
}
