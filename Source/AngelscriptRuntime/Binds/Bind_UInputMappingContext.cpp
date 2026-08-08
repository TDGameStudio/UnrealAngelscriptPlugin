#include "AngelscriptBinds.h"

#include "EnhancedActionKeyMapping.h"
#include "InputAction.h"
#include "InputMappingContext.h"

#include "Bind_UInputMappingContext_Functions.h"

namespace
{
	void BindUInputActionFunctions(FAngelscriptBinds& Binds)
	{
		auto Action_ = Binds.ExistingClassForTarget("UInputAction");
		Action_.Method("void SetValueType(EInputActionValueType InValueType)", &FAngelscriptUInputMappingContextBinds::SetValueType);
		Action_.Method("EInputActionValueType GetValueType() const", &FAngelscriptUInputMappingContextBinds::GetValueType);
		Action_.Method(
			"void SetAccumulationBehavior(EInputActionAccumulationBehavior InBehavior)",
			&FAngelscriptUInputMappingContextBinds::SetAccumulationBehavior);
		Action_.Method(
			"EInputActionAccumulationBehavior GetAccumulationBehavior() const",
			&FAngelscriptUInputMappingContextBinds::GetAccumulationBehavior);
	}

	void BindFEnhancedActionKeyMappingFunctions(FAngelscriptBinds& Binds)
	{
		auto Mapping_ = Binds.ExistingClassForTarget("FEnhancedActionKeyMapping");
		Mapping_.Constructor(
			"void f(const UInputAction InAction, FKey InKey)",
			&FAngelscriptUInputMappingContextBinds::ConstructMapping,
			"FEnhancedActionKeyMapping",
			true);
		Mapping_.Method(
			"bool opEquals(const FEnhancedActionKeyMapping& Other) const",
			METHOD_TRIVIAL(FEnhancedActionKeyMapping, operator==));
		Mapping_.Method("const UInputAction GetAction() const", &FAngelscriptUInputMappingContextBinds::GetAction);
		Mapping_.Method("void SetAction(const UInputAction InAction)", &FAngelscriptUInputMappingContextBinds::SetAction);
		Mapping_.Method("FKey GetKey() const", &FAngelscriptUInputMappingContextBinds::GetKey);
		Mapping_.Method("void SetKey(FKey InKey)", &FAngelscriptUInputMappingContextBinds::SetKey);
		Mapping_.Method("void AddModifier(UInputModifier Modifier)", &FAngelscriptUInputMappingContextBinds::AddModifier);
		Mapping_.Method("void ClearModifiers()", &FAngelscriptUInputMappingContextBinds::ClearModifiers);
		Mapping_.Method("int32 GetModifierCount() const", &FAngelscriptUInputMappingContextBinds::GetModifierCount);
		Mapping_.Method("void AddTrigger(UInputTrigger Trigger)", &FAngelscriptUInputMappingContextBinds::AddTrigger);
		Mapping_.Method("void ClearTriggers()", &FAngelscriptUInputMappingContextBinds::ClearTriggers);
		Mapping_.Method("int32 GetTriggerCount() const", &FAngelscriptUInputMappingContextBinds::GetTriggerCount);
	}

	void BindUInputMappingContextFunctions(FAngelscriptBinds& Binds)
	{
		auto Context_ = Binds.ExistingClassForTarget("UInputMappingContext");
		Context_.Method("FEnhancedActionKeyMapping& MapKey(const UInputAction Action, FKey ToKey)", &FAngelscriptUInputMappingContextBinds::MapKey);
		Context_.Method(
			"void UnmapKey(const UInputAction Action, FKey Key)",
			METHODPR_TRIVIAL(void, UInputMappingContext, UnmapKey, (const UInputAction*, FKey)));
		Context_.Method(
			"void UnmapAllKeysFromAction(const UInputAction Action)",
			METHODPR_TRIVIAL(void, UInputMappingContext, UnmapAllKeysFromAction, (const UInputAction*)));
		Context_.Method("void UnmapAll()", METHOD_TRIVIAL(UInputMappingContext, UnmapAll));
		Context_.Method(
			"bool HasMappingForInputAction(const UInputAction Action) const",
			METHODPR_TRIVIAL(bool, UInputMappingContext, HasMappingForInputAction, (const UInputAction*) const));
		Context_.Method("int32 GetMappingCount() const", &FAngelscriptUInputMappingContextBinds::GetMappingCount);
		Context_.Method("FEnhancedActionKeyMapping& GetMapping(int32 Index)", &FAngelscriptUInputMappingContextBinds::GetMapping);
	}
}

AS_FORCE_LINK const FAngelscriptBind Bind_UInputAction_Late(
	TEXT("UInputMappingContext.InputAction"),
	EAngelscriptBindPhase::ManualBindings,
	&BindUInputActionFunctions);

AS_FORCE_LINK const FAngelscriptBind Bind_FEnhancedActionKeyMapping_Late(
	TEXT("UInputMappingContext.EnhancedActionKeyMapping"),
	EAngelscriptBindPhase::ManualBindings,
	&BindFEnhancedActionKeyMappingFunctions);

AS_FORCE_LINK const FAngelscriptBind Bind_UInputMappingContext_Late(
	TEXT("UInputMappingContext.Functions"),
	EAngelscriptBindPhase::ManualBindings,
	&BindUInputMappingContextFunctions);
