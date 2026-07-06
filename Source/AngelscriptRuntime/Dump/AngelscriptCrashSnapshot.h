#pragma once

#include "CoreMinimal.h"
#include "Core/AngelscriptEngineExtensionRegistry.h"

struct ANGELSCRIPTRUNTIME_API FAngelscriptCrashSnapshot
{
	struct FWriteResult
	{
		bool bSuccess = false;
		FString SnapshotPath;
		FString ErrorMessage;
	};

	// Managed by FAngelscriptCrashSnapshotExtension. Direct calls should only
	// be used by extension lifecycle code.
	static void Startup();
	static void Shutdown();
	static FWriteResult WriteSnapshotForTesting(const FString& OutputDir, const FString& Marker = FString());
	static void ConfigureForTesting(const FString& OutputDir, const FString& Marker);

#if WITH_DEV_AUTOMATION_TESTS
	static bool IsHandlerRegisteredForTesting();
#endif

private:
	static void HandleSystemError();
};

class ANGELSCRIPTRUNTIME_API FAngelscriptCrashSnapshotExtension : public IAngelscriptExtension
{
public:
	virtual void OnEngineAttached(FAngelscriptEngine& Engine) override;
	virtual void OnEngineDetached(FAngelscriptEngine& Engine) override;

	static FDelegateHandle Startup();
	static void Shutdown(FDelegateHandle& Handle);

#if WITH_DEV_AUTOMATION_TESTS
	static int32 GetActiveEngineCountForTesting();
#endif
};
