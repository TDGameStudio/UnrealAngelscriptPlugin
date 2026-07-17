#include "AngelscriptBinds.h"
#include "Curves/CurveLinearColor.h"
#include "Engine/LevelStreaming.h"

#include "FunctionLibraries/AngelscriptComponentLibrary.h"
#include "FunctionLibraries/AngelscriptLevelStreamingLibrary.h"
#include "FunctionLibraries/RuntimeCurveLinearColorMixinLibrary.h"
#include "FunctionLibraries/RuntimeFloatCurveMixinLibrary.h"

AS_FORCE_LINK const FAngelscriptBinds::FBind Bind_FunctionLibraryMixins((int32)FAngelscriptBinds::EOrder::Late + 110, []
{
	FAngelscriptBinds::RegisterFunctionBinding(
		URuntimeFloatCurveMixinLibrary::StaticClass(),
		"GetTimeRange",
		{ ERASE_FUNCTION_PTR(URuntimeFloatCurveMixinLibrary::GetTimeRange, (const FRuntimeFloatCurve&, float&, float&), ERASE_ARGUMENT_PACK(void)) });

	auto LevelStreaming_ = FAngelscriptBinds::ExistingClass("ULevelStreaming");
#if WITH_EDITOR
	// Phase 2 of Bind_Defaults (EOrder::Late+100) auto-registers this member via the
	// UAngelscriptLevelStreamingLibrary ScriptMixin-annotated UFUNCTION. We run at
	// EOrder::Late+110, so guard against re-registering the same signature — otherwise
	// AngelScript returns asALREADY_REGISTERED and the asIScriptEngine is left in a
	// broken state (breaks MultiEngine/DependencyInjection tests that rebind on clone).
	asITypeInfo* LevelStreamingType = LevelStreaming_.GetTypeInfo();
	if (LevelStreamingType == nullptr || LevelStreamingType->GetMethodByDecl("bool GetShouldBeVisibleInEditor() const") == nullptr)
	{
		LevelStreaming_.Method("bool GetShouldBeVisibleInEditor() const", [](const ULevelStreaming* LevelStreaming) -> bool
		{
			return UAngelscriptLevelStreamingLibrary::GetShouldBeVisibleInEditor(LevelStreaming);
		});
	}
#endif

	auto SceneComponent_ = FAngelscriptBinds::ExistingClass("USceneComponent");
	asITypeInfo* SceneComponentType = SceneComponent_.GetTypeInfo();
	if (SceneComponentType == nullptr || SceneComponentType->GetMethodByDecl("void SetRelativeRotation(FRotator NewRotation)") == nullptr)
	{
		SceneComponent_.Method(
			"void SetRelativeRotation(FRotator NewRotation)",
			[](USceneComponent* Component, const FRotator& NewRotation)
			{
				UAngelscriptComponentLibrary::SetRelativeRotation(Component, NewRotation);
			});
	}

	auto RuntimeCurveLinearColor_ = FAngelscriptBinds::ExistingClass("FRuntimeCurveLinearColor");
	if (!RuntimeCurveLinearColor_.HasMethod(TEXT("AddDefaultKey")))
	{
		RuntimeCurveLinearColor_.Method(
			"void AddDefaultKey(float32 InTime, FLinearColor InColor)",
			[](FRuntimeCurveLinearColor* Target, float InTime, const FLinearColor& InColor)
			{
				Target->ColorCurves[0].AddKey(InTime, InColor.R);
				Target->ColorCurves[1].AddKey(InTime, InColor.G);
				Target->ColorCurves[2].AddKey(InTime, InColor.B);
				Target->ColorCurves[3].AddKey(InTime, InColor.A);
			});
	}

	auto RuntimeFloatCurve_ = FAngelscriptBinds::ExistingClass("FRuntimeFloatCurve");
	asITypeInfo* RuntimeFloatCurveType = RuntimeFloatCurve_.GetTypeInfo();
	if (!RuntimeFloatCurve_.HasMethod(TEXT("AddDefaultKey")))
	{
		RuntimeFloatCurve_.Method(
			"void AddDefaultKey(float32 InTime, float32 InValue)",
			[](FRuntimeFloatCurve* Target, float InTime, float InValue)
			{
				URuntimeFloatCurveMixinLibrary::AddDefaultKey(*Target, InTime, InValue);
			});
	}
	if (RuntimeFloatCurveType == nullptr || RuntimeFloatCurveType->GetMethodByDecl("int GetNumKeys() const") == nullptr)
	{
		RuntimeFloatCurve_.Method(
			"int GetNumKeys() const",
			[](const FRuntimeFloatCurve* Target) -> int32
			{
				return URuntimeFloatCurveMixinLibrary::GetNumKeys(*Target);
			});
	}
	if (RuntimeFloatCurveType == nullptr || RuntimeFloatCurveType->GetMethodByDecl("void GetTimeRange(float32&out MinTime, float32&out MaxTime) const") == nullptr)
	{
		RuntimeFloatCurve_.Method(
			"void GetTimeRange(float32&out MinTime, float32&out MaxTime) const",
			[](const FRuntimeFloatCurve* Target, float& MinTime, float& MaxTime)
			{
				URuntimeFloatCurveMixinLibrary::GetTimeRange(*Target, MinTime, MaxTime);
			});
	}

	// UCurveFloat: guard with HasMethod (name-based) instead of GetMethodByDecl.
	// Bind_Defaults (EOrder::Late+100) auto-registers these via ScriptMixin on
	// URuntimeFloatCurveMixinLibrary. The auto-generated declaration string may
	// differ from the hand-written one, causing GetMethodByDecl to miss the
	// existing method and trigger asALREADY_REGISTERED (-13).
	auto CurveFloat_ = FAngelscriptBinds::ExistingClass("UCurveFloat");
	if (!CurveFloat_.HasMethod(TEXT("AddAutoCurveKey")))
	{
		CurveFloat_.Method(
			"FCurveKeyHandle AddAutoCurveKey(float32 InTime, float32 InValue)",
			[](UCurveFloat* Curve, float InTime, float InValue) -> FCurveKeyHandle
			{
				return URuntimeFloatCurveMixinLibrary::AddAutoCurveKey(Curve, InTime, InValue);
			});
	}
	if (!CurveFloat_.HasMethod(TEXT("SetKeyInterpMode")))
	{
		CurveFloat_.Method(
			"void SetKeyInterpMode(FCurveKeyHandle KeyHandle, ERichCurveInterpMode NewInterpMode, bool bAutoSetTangents)",
			[](UCurveFloat* Curve, FCurveKeyHandle KeyHandle, ERichCurveInterpMode NewInterpMode, bool bAutoSetTangents)
			{
				URuntimeFloatCurveMixinLibrary::SetKeyInterpMode(Curve, KeyHandle, NewInterpMode, bAutoSetTangents);
			});
	}

	FAngelscriptBinds::FNamespace RuntimeCurveLinearColorHelperNs("URuntimeCurveLinearColorMixinLibrary");
	FAngelscriptBinds::BindGlobalFunction(
		"void AddDefaultKey(FRuntimeCurveLinearColor& Target, float32 InTime, FLinearColor InColor)",
		[](FRuntimeCurveLinearColor& Target, float InTime, const FLinearColor& InColor)
		{
			URuntimeCurveLinearColorMixinLibrary::AddDefaultKey(Target, InTime, InColor);
		});

	FAngelscriptBinds::FNamespace RuntimeFloatCurveHelperNs("URuntimeFloatCurveMixinLibrary");
	FAngelscriptBinds::BindGlobalFunction(
		"void GetTimeRange(const FRuntimeFloatCurve& Target, float32&out MinTime, float32&out MaxTime)",
		[](const FRuntimeFloatCurve& Target, float& MinTime, float& MaxTime)
		{
			URuntimeFloatCurveMixinLibrary::GetTimeRange(Target, MinTime, MaxTime);
		});
});
