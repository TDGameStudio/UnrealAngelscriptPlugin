#include "AngelscriptBinds.h"
#include "Bind_FunctionLibraryMixins_Functions.h"
#include "Core/FunctionCallers.h"

#include "FunctionLibraries/AngelscriptComponentLibrary.h"
#include "FunctionLibraries/AngelscriptLevelStreamingLibrary.h"
#include "FunctionLibraries/RuntimeFloatCurveMixinLibrary.h"

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
