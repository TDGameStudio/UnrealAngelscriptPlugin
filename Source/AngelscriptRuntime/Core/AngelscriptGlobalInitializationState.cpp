#include "Core/AngelscriptGlobalInitializationState.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_module.h"
#include "EndAngelscriptHeaders.h"

FString AngelscriptGlobalInitializationState::GetName()
{
	if (asCModule::InitializingGlobalProperty != nullptr)
	{
		return ANSI_TO_TCHAR(asCModule::InitializingGlobalProperty->name.AddressOf());
	}
	return FString();
}

FString AngelscriptGlobalInitializationState::GetNamespace()
{
	if (asCModule::InitializingGlobalProperty != nullptr
		&& asCModule::InitializingGlobalProperty->nameSpace != nullptr)
	{
		return ANSI_TO_TCHAR(asCModule::InitializingGlobalProperty->nameSpace->name.AddressOf());
	}
	return FString();
}

FString AngelscriptGlobalInitializationState::GetModuleName()
{
	if (asCModule::InitializingGlobalProperty != nullptr
		&& asCModule::InitializingGlobalProperty->module != nullptr)
	{
		return ANSI_TO_TCHAR(asCModule::InitializingGlobalProperty->module->GetName());
	}
	return FString();
}
