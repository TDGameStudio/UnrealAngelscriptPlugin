#pragma once

#include "InputAction.h"
#include "InputCoreTypes.h"

class UInputAction;
class UInputMappingContext;
class UInputModifier;
class UInputTrigger;
struct FEnhancedActionKeyMapping;

struct FAngelscriptUInputMappingContextBinds
{
	static void SetValueType(UInputAction* Action, EInputActionValueType ValueType);
	static EInputActionValueType GetValueType(const UInputAction* Action);
	static void SetAccumulationBehavior(UInputAction* Action, EInputActionAccumulationBehavior Behavior);
	static EInputActionAccumulationBehavior GetAccumulationBehavior(const UInputAction* Action);

	static void ConstructMapping(FEnhancedActionKeyMapping* Address, const UInputAction* Action, FKey Key);
	static const UInputAction* GetAction(const FEnhancedActionKeyMapping* Mapping);
	static void SetAction(FEnhancedActionKeyMapping* Mapping, const UInputAction* Action);
	static FKey GetKey(const FEnhancedActionKeyMapping* Mapping);
	static void SetKey(FEnhancedActionKeyMapping* Mapping, FKey Key);
	static void AddModifier(FEnhancedActionKeyMapping* Mapping, UInputModifier* Modifier);
	static void ClearModifiers(FEnhancedActionKeyMapping* Mapping);
	static int32 GetModifierCount(const FEnhancedActionKeyMapping* Mapping);
	static void AddTrigger(FEnhancedActionKeyMapping* Mapping, UInputTrigger* Trigger);
	static void ClearTriggers(FEnhancedActionKeyMapping* Mapping);
	static int32 GetTriggerCount(const FEnhancedActionKeyMapping* Mapping);

	static FEnhancedActionKeyMapping& MapKey(UInputMappingContext* Context, const UInputAction* Action, FKey Key);
	static int32 GetMappingCount(const UInputMappingContext* Context);
	static FEnhancedActionKeyMapping& GetMapping(UInputMappingContext* Context, int32 Index);
};
