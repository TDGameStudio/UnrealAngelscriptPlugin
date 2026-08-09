#include "CoreMinimal.h"
#include "AngelscriptBinds.h"
#include "AngelscriptEngine.h"

#if WITH_EDITOR

/**
 * Reflected deprecation contribution; this registrar adds metadata and declares no new callable.
 * +------------------------------------------------------------------------------------------+--------------------------------------------------------------------------------------------------------------------+
 * | AngelScript usage signature                                                              | Purpose / parameter notes                                                                                          |
 * +------------------------------------------------------------------------------------------+--------------------------------------------------------------------------------------------------------------------+
 * | NiagaraComponent.SetNiagaraVariable*(...);                                               | Marks the legacy typed-name Niagara setters deprecated; use the corresponding SetVariable(FName, ...) APIs.        |
 * +------------------------------------------------------------------------------------------+--------------------------------------------------------------------------------------------------------------------+
 */

namespace DeprecationBind
{
	static const FName NAME_META_DeprecatedFunction("DeprecatedFunction");
	static const FName NAME_META_DeprecationMessage("DeprecationMessage");

	void DeprecateMethod(const TCHAR* MethodPath, const TCHAR* DeprecationMessage)
	{
		UFunction* Function = FindObject<UFunction>(nullptr, MethodPath);
		if (Function == nullptr)
		{
			UE_LOG(Angelscript, Warning, TEXT("Could not find method %s to add deprecation for"), MethodPath);
			return;
		}

		Function->SetMetaData(NAME_META_DeprecatedFunction, TEXT(""));
		Function->SetMetaData(NAME_META_DeprecationMessage, DeprecationMessage);
	}

}

AS_FORCE_LINK const FAngelscriptBind Bind_Deprecations(
	TEXT("Deprecations"),
	EAngelscriptBindPhase::TypeDeclarations,
	[](FAngelscriptBinds&)
	{
		DeprecationBind::DeprecateMethod(TEXT("/Script/Niagara.NiagaraComponent:SetNiagaraVariableLinearColor"), TEXT("Use the SetVariable variant that takes FName instead"));
		DeprecationBind::DeprecateMethod(TEXT("/Script/Niagara.NiagaraComponent:SetNiagaraVariableVec4"), TEXT("Use the SetVariable variant that takes FName instead"));
		DeprecationBind::DeprecateMethod(TEXT("/Script/Niagara.NiagaraComponent:SetNiagaraVariableQuat"), TEXT("Use the SetVariable variant that takes FName instead"));
		DeprecationBind::DeprecateMethod(TEXT("/Script/Niagara.NiagaraComponent:SetNiagaraVariableMatrix"), TEXT("Use the SetVariable variant that takes FName instead"));
		DeprecationBind::DeprecateMethod(TEXT("/Script/Niagara.NiagaraComponent:SetNiagaraVariableVec3"), TEXT("Use the SetVariable variant that takes FName instead"));
		DeprecationBind::DeprecateMethod(TEXT("/Script/Niagara.NiagaraComponent:SetNiagaraVariablePosition"), TEXT("Use the SetVariable variant that takes FName instead"));
		DeprecationBind::DeprecateMethod(TEXT("/Script/Niagara.NiagaraComponent:SetNiagaraVariableVec2"), TEXT("Use the SetVariable variant that takes FName instead"));
		DeprecationBind::DeprecateMethod(TEXT("/Script/Niagara.NiagaraComponent:SetNiagaraVariableFloat"), TEXT("Use the SetVariable variant that takes FName instead"));
		DeprecationBind::DeprecateMethod(TEXT("/Script/Niagara.NiagaraComponent:SetNiagaraVariableInt"), TEXT("Use the SetVariable variant that takes FName instead"));
		DeprecationBind::DeprecateMethod(TEXT("/Script/Niagara.NiagaraComponent:SetNiagaraVariableBool"), TEXT("Use the SetVariable variant that takes FName instead"));
		DeprecationBind::DeprecateMethod(TEXT("/Script/Niagara.NiagaraComponent:SetNiagaraVariableActor"), TEXT("Use the SetVariable variant that takes FName instead"));
		DeprecationBind::DeprecateMethod(TEXT("/Script/Niagara.NiagaraComponent:SetNiagaraVariableObject"), TEXT("Use the SetVariable variant that takes FName instead"));
	});

#endif
