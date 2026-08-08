#include "EditorMenuExtensions/ScriptEditorPrompts_Functions.h"

#include "AngelscriptBinds.h"
#include "AngelscriptEngine.h"
#include "EditorMenuExtensions/ScriptEditorPrompts.h"

bool FAngelscriptScriptEditorPromptsBinds::ShowPromptForStruct(
	void* StructAddr,
	int32 TypeId)
{
	const FAngelscriptTypeUsage Usage =
		FAngelscriptTypeUsage::FromTypeId(TypeId);
	const UStruct* StructDef = Usage.GetUnrealStruct();
	if (StructDef == nullptr)
	{
		FAngelscriptEngine::Throw(
			"ShowPromptForStruct: not a valid USTRUCT");
		return false;
	}

	TSharedRef<FStructOnScope> Struct = MakeShared<FStructOnScope>(
		StructDef,
		static_cast<uint8*>(StructAddr));
	return FScriptEditorPrompts::ShowPromptForStruct(
		Struct,
		FScriptEditorPromptOptions());
}

bool FAngelscriptScriptEditorPromptsBinds::ShowPromptForStructWithOptions(
	void* StructAddr,
	int32 TypeId,
	const FScriptEditorPromptOptions& Options)
{
	const FAngelscriptTypeUsage Usage =
		FAngelscriptTypeUsage::FromTypeId(TypeId);
	const UStruct* StructDef = Usage.GetUnrealStruct();
	if (StructDef == nullptr)
	{
		FAngelscriptEngine::Throw(
			"ShowPromptForStruct: not a valid USTRUCT");
		return false;
	}

	TSharedRef<FStructOnScope> Struct = MakeShared<FStructOnScope>(
		StructDef,
		static_cast<uint8*>(StructAddr));
	return FScriptEditorPrompts::ShowPromptForStruct(Struct, Options);
}

bool FAngelscriptScriptEditorPromptsBinds::ShowPromptToCallFunction(
	UObject* Object,
	FName FunctionName)
{
	return FScriptEditorPrompts::ShowPromptToCallFunction(
		Object,
		FunctionName,
		FScriptEditorPromptOptions(),
		TArray<UObject*>());
}

bool FAngelscriptScriptEditorPromptsBinds::ShowPromptToCallFunctionWithOptions(
	UObject* Object,
	FName FunctionName,
	const FScriptEditorPromptOptions& Options)
{
	return FScriptEditorPrompts::ShowPromptToCallFunction(
		Object,
		FunctionName,
		Options,
		TArray<UObject*>());
}

bool FAngelscriptScriptEditorPromptsBinds::ShowPromptToCallFunctionWithObjects(
	UObject* Object,
	FName FunctionName,
	const FScriptEditorPromptOptions& Options,
	TArray<UObject*> FirstParameterObjects)
{
	return FScriptEditorPrompts::ShowPromptToCallFunction(
		Object,
		FunctionName,
		Options,
		FirstParameterObjects);
}

bool FAngelscriptScriptEditorPromptsBinds::ShowPromptToCallFunctionOnObjects(
	TArray<UObject*> Objects,
	FName FunctionName,
	const FScriptEditorPromptOptions& Options)
{
	UFunction* FoundFunction = nullptr;
	for (UObject* Object : Objects)
	{
		if (Object == nullptr)
		{
			continue;
		}
		FoundFunction = Object->GetClass()->FindFunctionByName(FunctionName);
		if (FoundFunction != nullptr)
		{
			break;
		}
	}
	if (FoundFunction == nullptr)
	{
		return false;
	}
	return FScriptEditorPrompts::ShowPromptToCallFunctionOnObjects(
		FoundFunction,
		Objects,
		Options);
}
