#include "HotReload/AngelscriptScriptTestAutomationRefresh.h"

#include "IAutomationControllerManager.h"
#include "IAutomationControllerModule.h"
#include "Modules/ModuleManager.h"
#include "Testing/AngelscriptScriptTestRegistry.h"

FAngelscriptScriptTestAutomationRefresh::
	FAngelscriptScriptTestAutomationRefresh()
	: Callbacks(MakeDefaultCallbacks())
{
}

FAngelscriptScriptTestAutomationRefresh::
	FAngelscriptScriptTestAutomationRefresh(
		FCallbacks InCallbacks)
	: Callbacks(MoveTemp(InCallbacks))
{
}

FAngelscriptScriptTestAutomationRefresh::
	~FAngelscriptScriptTestAutomationRefresh()
{
	Shutdown();
}

FAngelscriptScriptTestAutomationRefresh::FCallbacks
FAngelscriptScriptTestAutomationRefresh::MakeDefaultCallbacks()
{
	FCallbacks Result;
	Result.IsControllerLoaded = []()
	{
		return FModuleManager::Get().IsModuleLoaded(
			TEXT("AutomationController"));
	};
	Result.IsControllerRunning = []()
	{
		IAutomationControllerModule* Module =
			FModuleManager::GetModulePtr<IAutomationControllerModule>(
				TEXT("AutomationController"));
		return Module != nullptr
			&& Module->GetAutomationController()->GetTestState()
				== EAutomationControllerModuleState::Running;
	};
	Result.RequestTests = []()
	{
		IAutomationControllerModule* Module =
			FModuleManager::GetModulePtr<IAutomationControllerModule>(
				TEXT("AutomationController"));
		if (Module != nullptr)
		{
			Module->GetAutomationController()->RequestTests();
		}
	};
	return Result;
}

void FAngelscriptScriptTestAutomationRefresh::Startup()
{
	if (bStarted)
	{
		return;
	}
	bStarted = true;
	RegistryChangedHandle =
		FAngelscriptScriptTestRegistry::Get().OnChanged().AddRaw(
			this,
			&FAngelscriptScriptTestAutomationRefresh::
				NotifyRegistryChanged);
	TickHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateRaw(
			this,
			&FAngelscriptScriptTestAutomationRefresh::Tick));
}

void FAngelscriptScriptTestAutomationRefresh::Shutdown()
{
	if (!bStarted)
	{
		return;
	}
	bStarted = false;
	if (RegistryChangedHandle.IsValid())
	{
		FAngelscriptScriptTestRegistry::Get().OnChanged().Remove(
			RegistryChangedHandle);
		RegistryChangedHandle.Reset();
	}
	if (TickHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);
		TickHandle.Reset();
	}
	bRefreshPending = false;
	NewestGeneration = 0;
}

void FAngelscriptScriptTestAutomationRefresh::NotifyRegistryChanged(
	uint64 Generation)
{
	NewestGeneration = FMath::Max(NewestGeneration, Generation);
	bRefreshPending = true;
}

bool FAngelscriptScriptTestAutomationRefresh::Tick(float)
{
	if (!bRefreshPending)
	{
		return true;
	}

	if (!Callbacks.IsControllerLoaded
		|| !Callbacks.IsControllerLoaded())
	{
		// An unopened Session Frontend will obtain the current list when it
		// initializes. Do not create a heavyweight controller just for this.
		bRefreshPending = false;
		NewestGeneration = 0;
		return true;
	}

	if (Callbacks.IsControllerRunning
		&& Callbacks.IsControllerRunning())
	{
		return true;
	}

	if (Callbacks.RequestTests)
	{
		Callbacks.RequestTests();
	}
	bRefreshPending = false;
	NewestGeneration = 0;
	return true;
}
