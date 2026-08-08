#include "Bind_FunctionLibraryMixins.h"

#include "AngelscriptBinds.h"
#include "Core/FunctionCallers.h"

#include "FunctionLibraries/AngelscriptComponentLibrary.h"
#include "FunctionLibraries/AngelscriptLevelStreamingLibrary.h"
#include "FunctionLibraries/RuntimeFloatCurveMixinLibrary.h"

/**
 * Post-reflection component, streaming, runtime-curve, and curve-asset mixin surfaces.
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | AngelScript usage signature                                                                          | Purpose / parameter notes                                                                                        |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | bool bVisible = LevelStreaming.GetShouldBeVisibleInEditor() const;                                   | Returns the editor visibility request for a streaming level; available only in editor builds.                    |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | SceneComponent.SetRelativeRotation(FRotator NewRotation);                                            | Sets component rotation relative to its parent without sweep.                                                    |
 * |                                                                                                      | @param NewRotation Relative rotation in degrees.                                                                 |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | bool bAttached = SceneComponent.IsAttachedTo(const USceneComponent CheckComponent) const;            | Reports whether a component appears in this component's attachment ancestry.                                     |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | RuntimeCurveLinearColor.AddDefaultKey(float32 InTime, FLinearColor InColor);                         | Adds a key to the runtime linear-color curve's default rich curves.                                              |
 * |                                                                                                      | @param InTime Curve input time.                                                                                  |
 * |                                                                                                      | @param InColor Color value stored at the key.                                                                    |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | RuntimeFloatCurve.AddDefaultKey(float32 InTime, float32 InValue);                                    | Adds a key to the runtime float curve's default rich curve.                                                      |
 * |                                                                                                      | @param InTime Curve input time.                                                                                  |
 * |                                                                                                      | @param InValue Float value stored at the key.                                                                    |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | int Count = RuntimeFloatCurve.GetNumKeys() const;                                                    | Returns the number of keys in the runtime float curve.                                                           |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | RuntimeFloatCurve.GetTimeRange(float32&out MinTime, float32&out MaxTime) const;                      | Returns the minimum and maximum key times.                                                                       |
 * |                                                                                                      | @param MinTime Receives the earliest key time.                                                                   |
 * |                                                                                                      | @param MaxTime Receives the latest key time.                                                                     |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FCurveKeyHandle Handle = CurveFloat.AddAutoCurveKey(float32 InTime, float32 InValue);                | Adds an automatically-tangent key and returns its stable handle.                                                 |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | CurveFloat.SetKeyInterpMode(FCurveKeyHandle KeyHandle, ERichCurveInterpMode NewInterpMode,           | Changes interpolation for a curve key.                                                                           |
 * |     bool bAutoSetTangents);                                                                          | @param KeyHandle Key to edit.                                                                                    |
 * |                                                                                                      | @param NewInterpMode New rich-curve interpolation mode.                                                          |
 * |                                                                                                      | @param bAutoSetTangents Recomputes automatic tangents when true.                                                 |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | URuntimeCurveLinearColorMixinLibrary::AddDefaultKey(                                                 | Static form of the runtime linear-color key helper.                                                              |
 * |     FRuntimeCurveLinearColor& Target, float32 InTime, FLinearColor InColor);                         |                                                                                                                  |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | URuntimeFloatCurveMixinLibrary::GetTimeRange(const FRuntimeFloatCurve& Target, float32&out MinTime,  | Static form of the runtime float-curve time-range query.                                                         |
 * |     float32&out MaxTime);                                                                            |                                                                                                                  |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 */

namespace
{
	void BindFunctionLibraryMixins(FAngelscriptBinds& Binds)
	{
		Binds.RegisterFunctionBindingForTarget(
			URuntimeFloatCurveMixinLibrary::StaticClass(),
			"GetTimeRange",
			{ERASE_FUNCTION_PTR(URuntimeFloatCurveMixinLibrary::GetTimeRange, (const FRuntimeFloatCurve&, float&, float&), ERASE_ARGUMENT_PACK(void))});

		auto LevelStreaming_ = Binds.ExistingClassForTarget("ULevelStreaming");
#if WITH_EDITOR
		// ReflectionBindings auto-registers this ScriptMixin UFUNCTION first. Keep
		// the explicit fallback only when reflection did not produce the method.
		asITypeInfo* LevelStreamingType = LevelStreaming_.GetTypeInfo();
		if (LevelStreamingType == nullptr || LevelStreamingType->GetMethodByDecl("bool GetShouldBeVisibleInEditor() const") == nullptr)
		{
			LevelStreaming_.Method(
				"bool GetShouldBeVisibleInEditor() const",
				&FAngelscriptFunctionLibraryMixinsBinds::GetShouldBeVisibleInEditor);
		}
#endif

		auto SceneComponent_ = Binds.ExistingClassForTarget("USceneComponent");
		asITypeInfo* SceneComponentType = SceneComponent_.GetTypeInfo();
		if (SceneComponentType == nullptr || SceneComponentType->GetMethodByDecl("void SetRelativeRotation(FRotator NewRotation)") == nullptr)
		{
			SceneComponent_.Method(
				"void SetRelativeRotation(FRotator NewRotation)",
				&FAngelscriptFunctionLibraryMixinsBinds::SetRelativeRotation);
		}
		if (SceneComponentType == nullptr || SceneComponentType->GetMethodByDecl("bool IsAttachedTo(const USceneComponent CheckComponent) const") == nullptr)
		{
			SceneComponent_.Method(
				"bool IsAttachedTo(const USceneComponent CheckComponent) const",
				&FAngelscriptFunctionLibraryMixinsBinds::IsAttachedToComponent);
		}

		auto RuntimeCurveLinearColor_ = Binds.ExistingClassForTarget("FRuntimeCurveLinearColor");
		if (!RuntimeCurveLinearColor_.HasMethod(TEXT("AddDefaultKey")))
		{
			RuntimeCurveLinearColor_.Method(
				"void AddDefaultKey(float32 InTime, FLinearColor InColor)",
				&FAngelscriptFunctionLibraryMixinsBinds::AddRuntimeCurveLinearColorKey);
		}

		auto RuntimeFloatCurve_ = Binds.ExistingClassForTarget("FRuntimeFloatCurve");
		asITypeInfo* RuntimeFloatCurveType = RuntimeFloatCurve_.GetTypeInfo();
		if (!RuntimeFloatCurve_.HasMethod(TEXT("AddDefaultKey")))
		{
			RuntimeFloatCurve_.Method(
				"void AddDefaultKey(float32 InTime, float32 InValue)",
				&FAngelscriptFunctionLibraryMixinsBinds::AddRuntimeFloatCurveKey);
		}
		if (RuntimeFloatCurveType == nullptr || RuntimeFloatCurveType->GetMethodByDecl("int GetNumKeys() const") == nullptr)
		{
			RuntimeFloatCurve_.Method(
				"int GetNumKeys() const",
				&FAngelscriptFunctionLibraryMixinsBinds::GetRuntimeFloatCurveNumKeys);
		}
		if (RuntimeFloatCurveType == nullptr || RuntimeFloatCurveType->GetMethodByDecl("void GetTimeRange(float32&out MinTime, float32&out MaxTime) const") == nullptr)
		{
			RuntimeFloatCurve_.Method(
				"void GetTimeRange(float32&out MinTime, float32&out MaxTime) const",
				&FAngelscriptFunctionLibraryMixinsBinds::GetRuntimeFloatCurveTimeRange);
		}

		// Reflection-generated ScriptMixin declarations can differ textually from
		// these hand-written forms, so the UCurveFloat guards intentionally remain
		// name-based to avoid asALREADY_REGISTERED.
		auto CurveFloat_ = Binds.ExistingClassForTarget("UCurveFloat");
		if (!CurveFloat_.HasMethod(TEXT("AddAutoCurveKey")))
		{
			CurveFloat_.Method(
				"FCurveKeyHandle AddAutoCurveKey(float32 InTime, float32 InValue)",
				&FAngelscriptFunctionLibraryMixinsBinds::AddAutoCurveKey);
		}
		if (!CurveFloat_.HasMethod(TEXT("SetKeyInterpMode")))
		{
			CurveFloat_.Method(
				"void SetKeyInterpMode(FCurveKeyHandle KeyHandle, ERichCurveInterpMode NewInterpMode, bool bAutoSetTangents)",
				&FAngelscriptFunctionLibraryMixinsBinds::SetKeyInterpMode);
		}

		{
			FAngelscriptBinds::FNamespace Namespace(Binds.GetTargetEngine(), "URuntimeCurveLinearColorMixinLibrary");
			Binds.BindGlobalFunctionForTarget(
				"void AddDefaultKey(FRuntimeCurveLinearColor& Target, float32 InTime, FLinearColor InColor)",
				&FAngelscriptFunctionLibraryMixinsBinds::AddRuntimeCurveLinearColorKeyGlobal);
		}

		{
			FAngelscriptBinds::FNamespace Namespace(Binds.GetTargetEngine(), "URuntimeFloatCurveMixinLibrary");
			Binds.BindGlobalFunctionForTarget(
				"void GetTimeRange(const FRuntimeFloatCurve& Target, float32&out MinTime, float32&out MaxTime)",
				&FAngelscriptFunctionLibraryMixinsBinds::GetRuntimeFloatCurveTimeRangeGlobal);
		}
	}
}

AS_FORCE_LINK const FAngelscriptBind Bind_FunctionLibraryMixins(
	TEXT("FunctionLibraryMixins.PostReflection"),
	EAngelscriptBindPhase::PostReflectionBindings,
	&BindFunctionLibraryMixins);
