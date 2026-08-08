#pragma once

#include "CoreMinimal.h"

struct FScriptEditorPromptOptions;

struct FAngelscriptScriptEditorPromptsBinds
{
	static bool ShowPromptForStruct(
		void* StructAddr,
		int32 TypeId);
	static bool ShowPromptForStructWithOptions(
		void* StructAddr,
		int32 TypeId,
		const FScriptEditorPromptOptions& Options);
	static bool ShowPromptToCallFunction(
		UObject* Object,
		FName FunctionName);
	static bool ShowPromptToCallFunctionWithOptions(
		UObject* Object,
		FName FunctionName,
		const FScriptEditorPromptOptions& Options);
	static bool ShowPromptToCallFunctionWithObjects(
		UObject* Object,
		FName FunctionName,
		const FScriptEditorPromptOptions& Options,
		TArray<UObject*> FirstParameterObjects);
	static bool ShowPromptToCallFunctionOnObjects(
		TArray<UObject*> Objects,
		FName FunctionName,
		const FScriptEditorPromptOptions& Options);
};
