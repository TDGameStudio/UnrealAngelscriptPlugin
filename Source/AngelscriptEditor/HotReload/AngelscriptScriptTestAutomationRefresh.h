#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"

/**
 * Mirrors reflected AngelScript registry changes into an Automation
 * Controller which is already open.
 *
 * The Runtime registry deliberately knows nothing about
 * AutomationController. This Editor-owned adapter also refuses to load the
 * controller merely because a script changed. Multiple generations are
 * coalesced, and a refresh requested during a run waits until the controller
 * is idle.
 */
class ANGELSCRIPTEDITOR_API FAngelscriptScriptTestAutomationRefresh
{
public:
	struct FCallbacks
	{
		TFunction<bool()> IsControllerLoaded;
		TFunction<bool()> IsControllerRunning;
		TFunction<void()> RequestTests;
	};

	FAngelscriptScriptTestAutomationRefresh();
	explicit FAngelscriptScriptTestAutomationRefresh(
		FCallbacks InCallbacks);
	~FAngelscriptScriptTestAutomationRefresh();

	void Startup();
	void Shutdown();

	/** Testable registry notification boundary. */
	void NotifyRegistryChanged(uint64 Generation);
	bool Tick(float DeltaSeconds);

	bool HasPendingRefresh() const
	{
		return bRefreshPending;
	}

private:
	static FCallbacks MakeDefaultCallbacks();

	FCallbacks Callbacks;
	FDelegateHandle RegistryChangedHandle;
	FTSTicker::FDelegateHandle TickHandle;
	bool bStarted = false;
	bool bRefreshPending = false;
	uint64 NewestGeneration = 0;
};
