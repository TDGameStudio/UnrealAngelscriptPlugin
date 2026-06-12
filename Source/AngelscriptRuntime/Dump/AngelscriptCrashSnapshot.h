#pragma once

#include "CoreMinimal.h"

struct ANGELSCRIPTRUNTIME_API FAngelscriptCrashSnapshot
{
	struct FWriteResult
	{
		bool bSuccess = false;
		FString SnapshotPath;
		FString ErrorMessage;
	};

	static void Startup();
	static void Shutdown();
	static FWriteResult WriteSnapshotForTesting(const FString& OutputDir, const FString& Marker = FString());
	static void ConfigureForTesting(const FString& OutputDir, const FString& Marker);

private:
	static void HandleSystemError();
};
