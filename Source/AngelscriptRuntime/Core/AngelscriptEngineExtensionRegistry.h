#pragma once

#include "CoreMinimal.h"

struct FAngelscriptEngine;

class IAngelscriptExtension
{
public:
	virtual ~IAngelscriptExtension() = default;

	virtual void OnEngineAttached(FAngelscriptEngine& Engine) = 0;
	virtual void OnEngineDetached(FAngelscriptEngine& Engine) = 0;
};

class ANGELSCRIPTRUNTIME_API FAngelscriptEngineExtensionRegistry
{
public:
	static FAngelscriptEngineExtensionRegistry& Get();

	FDelegateHandle RegisterExtension(TSharedRef<IAngelscriptExtension> Extension);
	void UnregisterExtension(FDelegateHandle Handle);

	// Explicit lifecycle attach: the target must already own an AngelScript engine.
	void AttachEngine(FAngelscriptEngine& Engine);
	void DetachEngine(FAngelscriptEngine& Engine);

	// Replays the current published engine to every registered extension.
	// Initialization-only context scopes are not active extension targets.
	void ReplayCurrentEngine();

	// Detaches every registered extension from the current engine. This is the
	// explicit shutdown counterpart to ReplayCurrentEngine and will no-op when
	// no engine is active.
	void DetachCurrentEngine();

	int32 NumExtensions() const;

private:
	struct FRegisteredExtension
	{
		TSharedRef<IAngelscriptExtension> Extension;
	};

	mutable FCriticalSection CriticalSection;
	TMap<FDelegateHandle, FRegisteredExtension> RegisteredExtensions;
};
