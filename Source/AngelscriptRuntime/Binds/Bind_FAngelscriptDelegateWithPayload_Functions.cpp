#include "Bind_FAngelscriptDelegateWithPayload.h"

#include "AngelscriptDelegateWithPayload.h"

void FAngelscriptDelegateWithPayloadBinds::BindUFunction(
	FAngelscriptDelegateWithPayload& Delegate,
	UObject* Object,
	const FName& FunctionName)
{
	Delegate.BindUFunction(Object, FunctionName);
}

void FAngelscriptDelegateWithPayloadBinds::BindWithPayload(
	FAngelscriptDelegateWithPayload& Delegate,
	UObject* Object,
	const FName& FunctionName,
	void* Payload,
	int PayloadScriptTypeId)
{
	Delegate.BindUFunctionWithPayload(Object, FunctionName, Payload, PayloadScriptTypeId);
}
