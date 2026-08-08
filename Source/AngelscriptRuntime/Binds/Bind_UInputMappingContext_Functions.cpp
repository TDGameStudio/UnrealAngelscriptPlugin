#include "Bind_UInputMappingContext_Functions.h"

#include "EnhancedActionKeyMapping.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "InputTriggers.h"

void FAngelscriptUInputMappingContextBinds::SetValueType(
	UInputAction* Action,
	const EInputActionValueType ValueType)
{
	if (Action != nullptr)
	{
		Action->ValueType = ValueType;
	}
}

EInputActionValueType FAngelscriptUInputMappingContextBinds::GetValueType(const UInputAction* Action)
{
	return Action != nullptr ? Action->ValueType : EInputActionValueType::Boolean;
}

void FAngelscriptUInputMappingContextBinds::SetAccumulationBehavior(
	UInputAction* Action,
	const EInputActionAccumulationBehavior Behavior)
{
	if (Action != nullptr)
	{
		Action->AccumulationBehavior = Behavior;
	}
}

EInputActionAccumulationBehavior FAngelscriptUInputMappingContextBinds::GetAccumulationBehavior(
	const UInputAction* Action)
{
	return Action != nullptr
		? Action->AccumulationBehavior
		: EInputActionAccumulationBehavior::TakeHighestAbsoluteValue;
}

void FAngelscriptUInputMappingContextBinds::ConstructMapping(
	FEnhancedActionKeyMapping* Address,
	const UInputAction* Action,
	const FKey Key)
{
	new (Address) FEnhancedActionKeyMapping(Action, Key);
}

const UInputAction* FAngelscriptUInputMappingContextBinds::GetAction(
	const FEnhancedActionKeyMapping* Mapping)
{
	return Mapping != nullptr ? Mapping->Action.Get() : nullptr;
}

void FAngelscriptUInputMappingContextBinds::SetAction(
	FEnhancedActionKeyMapping* Mapping,
	const UInputAction* Action)
{
	if (Mapping != nullptr)
	{
		Mapping->Action = Action;
	}
}

FKey FAngelscriptUInputMappingContextBinds::GetKey(const FEnhancedActionKeyMapping* Mapping)
{
	return Mapping != nullptr ? Mapping->Key : EKeys::Invalid;
}

void FAngelscriptUInputMappingContextBinds::SetKey(FEnhancedActionKeyMapping* Mapping, const FKey Key)
{
	if (Mapping != nullptr)
	{
		Mapping->Key = Key;
	}
}

void FAngelscriptUInputMappingContextBinds::AddModifier(
	FEnhancedActionKeyMapping* Mapping,
	UInputModifier* Modifier)
{
	if (Mapping != nullptr && Modifier != nullptr)
	{
		Mapping->Modifiers.Add(Modifier);
	}
}

void FAngelscriptUInputMappingContextBinds::ClearModifiers(FEnhancedActionKeyMapping* Mapping)
{
	if (Mapping != nullptr)
	{
		Mapping->Modifiers.Reset();
	}
}

int32 FAngelscriptUInputMappingContextBinds::GetModifierCount(
	const FEnhancedActionKeyMapping* Mapping)
{
	return Mapping != nullptr ? Mapping->Modifiers.Num() : 0;
}

void FAngelscriptUInputMappingContextBinds::AddTrigger(
	FEnhancedActionKeyMapping* Mapping,
	UInputTrigger* Trigger)
{
	if (Mapping != nullptr && Trigger != nullptr)
	{
		Mapping->Triggers.Add(Trigger);
	}
}

void FAngelscriptUInputMappingContextBinds::ClearTriggers(FEnhancedActionKeyMapping* Mapping)
{
	if (Mapping != nullptr)
	{
		Mapping->Triggers.Reset();
	}
}

int32 FAngelscriptUInputMappingContextBinds::GetTriggerCount(
	const FEnhancedActionKeyMapping* Mapping)
{
	return Mapping != nullptr ? Mapping->Triggers.Num() : 0;
}

FEnhancedActionKeyMapping& FAngelscriptUInputMappingContextBinds::MapKey(
	UInputMappingContext* Context,
	const UInputAction* Action,
	const FKey Key)
{
	return Context->MapKey(Action, Key);
}

int32 FAngelscriptUInputMappingContextBinds::GetMappingCount(const UInputMappingContext* Context)
{
	return Context != nullptr ? Context->GetMappings().Num() : 0;
}

FEnhancedActionKeyMapping& FAngelscriptUInputMappingContextBinds::GetMapping(
	UInputMappingContext* Context,
	const int32 Index)
{
	return Context->GetMapping(Index);
}
