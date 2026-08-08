#pragma once

#include "CoreMinimal.h"

struct FAngelscriptDelegateWithPayload;

struct FAngelscriptDelegateWithPayloadBinds
{
	static void BindUFunction(
		FAngelscriptDelegateWithPayload& Delegate,
		UObject* Object,
		const FName& FunctionName);
	static void BindWithPayload(
		FAngelscriptDelegateWithPayload& Delegate,
		UObject* Object,
		const FName& FunctionName,
		void* Payload,
		int PayloadScriptTypeId);
};
