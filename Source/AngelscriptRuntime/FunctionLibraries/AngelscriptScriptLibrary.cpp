#include "FunctionLibraries/AngelscriptScriptLibrary.h"

#include "Core/AngelscriptGlobalInitializationState.h"

FString UAngelscriptScriptLibrary::GetNameOfGlobalVariableBeingInitialized()
{
	return AngelscriptGlobalInitializationState::GetName();
}

FString UAngelscriptScriptLibrary::GetNamespaceOfGlobalVariableBeingInitialized()
{
	return AngelscriptGlobalInitializationState::GetNamespace();
}

FString UAngelscriptScriptLibrary::GetModuleNameOfGlobalVariableBeingInitialized()
{
	return AngelscriptGlobalInitializationState::GetModuleName();
}
