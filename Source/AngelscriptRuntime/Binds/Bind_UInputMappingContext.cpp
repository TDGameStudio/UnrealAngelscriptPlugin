#include "Bind_UInputMappingContext.h"

#include "AngelscriptBinds.h"

#include "EnhancedActionKeyMapping.h"
#include "InputAction.h"
#include "InputMappingContext.h"

/**
 * Enhanced Input action, mapping, and mapping-context mutation surface.
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | AngelScript usage signature                                                                          | Purpose / parameter notes                                                                                        |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | void UInputAction.SetValueType(EInputActionValueType InValueType);                                   | Sets the action value type.                                                                                      |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | EInputActionValueType UInputAction.GetValueType() const;                                             | Returns the action value type.                                                                                   |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | void UInputAction.SetAccumulationBehavior(EInputActionAccumulationBehavior InBehavior);              | Sets how simultaneous input values accumulate.                                                                   |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | EInputActionAccumulationBehavior UInputAction.GetAccumulationBehavior() const;                       | Returns the accumulation behavior.                                                                               |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FEnhancedActionKeyMapping Mapping(const UInputAction InAction, FKey InKey);                          | Constructs an action/key mapping.                                                                                |
 * |                                                                                                      | @param InAction Action evaluated by the mapping.                                                                 |
 * |                                                                                                      | @param InKey Physical or virtual input key.                                                                      |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | bool bEqual = Left == Right;                                                                         | Compares enhanced action/key mappings.                                                                           |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | const UInputAction Mapping.GetAction() const;                                                        | Returns the mapped action.                                                                                       |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | void Mapping.SetAction(const UInputAction InAction);                                                 | Sets the mapped action.                                                                                          |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FKey Mapping.GetKey() const;                                                                         | Returns the mapped key.                                                                                          |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | void Mapping.SetKey(FKey InKey);                                                                     | Sets the mapped key.                                                                                             |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | void Mapping.AddModifier(UInputModifier Modifier);                                                   | Appends an input modifier.                                                                                       |
 * |                                                                                                      | @param Modifier Modifier evaluated in insertion order.                                                           |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | void Mapping.ClearModifiers();                                                                       | Removes all modifiers.                                                                                           |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | int32 Mapping.GetModifierCount() const;                                                              | Returns the modifier count.                                                                                      |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | void Mapping.AddTrigger(UInputTrigger Trigger);                                                      | Appends an input trigger.                                                                                        |
 * |                                                                                                      | @param Trigger Trigger evaluated in insertion order.                                                             |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | void Mapping.ClearTriggers();                                                                        | Removes all triggers.                                                                                            |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | int32 Mapping.GetTriggerCount() const;                                                               | Returns the trigger count.                                                                                       |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FEnhancedActionKeyMapping& UInputMappingContext.MapKey(const UInputAction Action, FKey ToKey);       | Adds a key mapping and returns it.                                                                               |
 * |                                                                                                      | @param Action Action to map.                                                                                     |
 * |                                                                                                      | @param ToKey Physical or virtual input key.                                                                      |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | void UInputMappingContext.UnmapKey(const UInputAction Action, FKey Key);                             | Removes one action/key mapping.                                                                                  |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | void UInputMappingContext.UnmapAllKeysFromAction(const UInputAction Action);                         | Removes every mapping for an action.                                                                             |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | void UInputMappingContext.UnmapAll();                                                                | Removes every mapping.                                                                                           |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | bool UInputMappingContext.HasMappingForInputAction(const UInputAction Action) const;                 | Reports whether the action has a mapping.                                                                        |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | int32 UInputMappingContext.GetMappingCount() const;                                                  | Returns the mapping count.                                                                                       |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FEnhancedActionKeyMapping& UInputMappingContext.GetMapping(int32 Index);                             | Returns a mapping by index.                                                                                      |
 * |                                                                                                      | @param Index Zero-based mapping index; invalid values follow the native bounds policy.                           |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 */

AS_FORCE_LINK const FAngelscriptBind Bind_UInputAction_Late(
	TEXT("UInputMappingContext.InputAction"),
	EAngelscriptBindPhase::ExplicitBindings,
	[](FAngelscriptBinds& Binds)
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
	});

AS_FORCE_LINK const FAngelscriptBind Bind_FEnhancedActionKeyMapping_Late(
	TEXT("UInputMappingContext.EnhancedActionKeyMapping"),
	EAngelscriptBindPhase::ExplicitBindings,
	[](FAngelscriptBinds& Binds)
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
	});

AS_FORCE_LINK const FAngelscriptBind Bind_UInputMappingContext_Late(
	TEXT("UInputMappingContext.Functions"),
	EAngelscriptBindPhase::ExplicitBindings,
	[](FAngelscriptBinds& Binds)
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
	});
