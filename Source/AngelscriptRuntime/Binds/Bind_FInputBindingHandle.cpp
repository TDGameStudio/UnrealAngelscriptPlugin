#include "EnhancedInputComponent.h"

#include "AngelscriptBinds.h"

AS_FORCE_LINK const FAngelscriptBinds::FBind Bind_FInputBindingHandle(FAngelscriptBinds::EOrder::Early, []
{
	const FBindFlags Flags;
	auto FInputBindingHandle_ = FAngelscriptBinds::ValueClass<FInputBindingHandle>("FInputBindingHandle", Flags);
	FInputBindingHandle_.Method("bool opEquals(const FInputBindingHandle& Other) const", METHODPR_TRIVIAL(bool, FInputBindingHandle, operator==, (const FInputBindingHandle&) const));
});

AS_FORCE_LINK const FAngelscriptBinds::FBind Bind_FInputBindingHandle_Late(FAngelscriptBinds::EOrder::Late, []
{
	auto FInputBindingHandle_ = FAngelscriptBinds::ExistingClass("FInputBindingHandle");
	FInputBindingHandle_.Method("uint32 GetHandle() const", METHOD_TRIVIAL(FInputBindingHandle, GetHandle));
});

AS_FORCE_LINK const FAngelscriptBinds::FBind Bind_FEnhancedInputActionEventBinding(FAngelscriptBinds::EOrder::Early, []
{
	const FBindFlags Flags;
	auto FEnhancedInputActionEventBinding_ = FAngelscriptBinds::ValueClass<FEnhancedInputActionEventBinding>("FEnhancedInputActionEventBinding", Flags);
	FEnhancedInputActionEventBinding_.Method("bool opEquals(const FEnhancedInputActionEventBinding& Other) const", METHODPR_TRIVIAL(bool, FEnhancedInputActionEventBinding, operator==, (const FEnhancedInputActionEventBinding&) const));
});

AS_FORCE_LINK const FAngelscriptBinds::FBind Bind_FEnhancedInputActionEventBinding_Late(FAngelscriptBinds::EOrder::Late, []
{
	auto FEnhancedInputActionEventBinding_ = FAngelscriptBinds::ExistingClass("FEnhancedInputActionEventBinding");
	FEnhancedInputActionEventBinding_.Method("uint32 GetHandle() const", [](const FEnhancedInputActionEventBinding& Binding)
	{
		return Binding.GetHandle();
	});
	FEnhancedInputActionEventBinding_.Method("const UInputAction GetAction() const", METHOD_TRIVIAL(FEnhancedInputActionEventBinding, GetAction));
	FEnhancedInputActionEventBinding_.Method("ETriggerEvent GetTriggerEvent() const", METHOD_TRIVIAL(FEnhancedInputActionEventBinding, GetTriggerEvent));
	FEnhancedInputActionEventBinding_.Method("void Execute(const FInputActionInstance& ActionData) const", METHODPR_TRIVIAL(void, FEnhancedInputActionEventBinding, Execute, (const FInputActionInstance&) const));
	FEnhancedInputActionEventBinding_.Method("void SetShouldFireWithEditorScriptGuard(const bool bNewValue)", METHODPR_TRIVIAL(void, FEnhancedInputActionEventBinding, SetShouldFireWithEditorScriptGuard, (const bool)));
	FEnhancedInputActionEventBinding_.Method("bool IsBoundToObject(const UObject Object) const", METHODPR_TRIVIAL(bool, FEnhancedInputActionEventBinding, IsBoundToObject, (const UObject*) const));
});

AS_FORCE_LINK const FAngelscriptBinds::FBind Bind_FEnhancedInputActionValueBinding(FAngelscriptBinds::EOrder::Early, []
{
	const FBindFlags Flags;
	auto FEnhancedInputActionValueBinding_ = FAngelscriptBinds::ValueClass<FEnhancedInputActionValueBinding>("FEnhancedInputActionValueBinding", Flags);
	FEnhancedInputActionValueBinding_.Constructor("void f()", [](FEnhancedInputActionValueBinding* Address)
	{
		new(Address) FEnhancedInputActionValueBinding();
	});
	SCRIPT_TRIVIAL_NATIVE_CONSTRUCTOR(FEnhancedInputActionValueBinding_, "FEnhancedInputActionValueBinding");
	FEnhancedInputActionValueBinding_.Method("bool opEquals(const FEnhancedInputActionValueBinding& Other) const", METHODPR_TRIVIAL(bool, FEnhancedInputActionValueBinding, operator==, (const FEnhancedInputActionValueBinding&) const));
});

AS_FORCE_LINK const FAngelscriptBinds::FBind Bind_FEnhancedInputActionValueBinding_Late(FAngelscriptBinds::EOrder::Late, []
{
	auto FEnhancedInputActionValueBinding_ = FAngelscriptBinds::ExistingClass("FEnhancedInputActionValueBinding");
	FEnhancedInputActionValueBinding_.Constructor("void f(const UInputAction InAction)", [](FEnhancedInputActionEventBinding* Address, const UInputAction* InAction)
	{
		new(Address) FEnhancedInputActionValueBinding(InAction);
	});
	SCRIPT_TRIVIAL_NATIVE_CONSTRUCTOR(FEnhancedInputActionValueBinding_, "FEnhancedInputActionValueBinding");
	FEnhancedInputActionValueBinding_.Method("uint32 GetHandle() const", METHOD_TRIVIAL(FEnhancedInputActionValueBinding, GetHandle));
	FEnhancedInputActionValueBinding_.Method("const UInputAction GetAction() const", METHOD_TRIVIAL(FEnhancedInputActionValueBinding, GetAction));
	FEnhancedInputActionValueBinding_.Method("FInputActionValue GetValue() const", METHOD_TRIVIAL(FEnhancedInputActionValueBinding, GetValue));
});

AS_FORCE_LINK const FAngelscriptBinds::FBind Bind_FInputDebugKeyBinding(FAngelscriptBinds::EOrder::Early, []
{
	const FBindFlags Flags;
	auto FInputDebugKeyBinding_ = FAngelscriptBinds::ValueClass<FInputDebugKeyBinding>("FInputDebugKeyBinding", Flags);
	FInputDebugKeyBinding_.Method("bool opEquals(const FInputDebugKeyBinding& Other) const", METHODPR_TRIVIAL(bool, FInputDebugKeyBinding, operator==, (const FInputDebugKeyBinding&) const));
});

AS_FORCE_LINK const FAngelscriptBinds::FBind Bind_FInputDebugKeyBinding_Late(FAngelscriptBinds::EOrder::Late, []
{
	auto FInputDebugKeyBinding_ = FAngelscriptBinds::ExistingClass("FInputDebugKeyBinding");
	FInputDebugKeyBinding_.Method("uint32 GetHandle() const", METHOD_TRIVIAL(FInputDebugKeyBinding, GetHandle));
	FInputDebugKeyBinding_.Method("void Execute(const FInputActionValue& ActionValue) const", METHODPR_TRIVIAL(void, FEnhancedInputActionEventBinding, Execute, (const FInputActionValue&) const));
});
