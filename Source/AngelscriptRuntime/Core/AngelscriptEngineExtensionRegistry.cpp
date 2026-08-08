#include "Core/AngelscriptEngineExtensionRegistry.h"

#include "AngelscriptEngine.h"

FAngelscriptEngineExtensionRegistry& FAngelscriptEngineExtensionRegistry::Get()
{
	static FAngelscriptEngineExtensionRegistry Registry;
	return Registry;
}

FDelegateHandle FAngelscriptEngineExtensionRegistry::RegisterExtension(TSharedRef<IAngelscriptExtension> Extension)
{
	const FDelegateHandle Handle(FDelegateHandle::GenerateNewHandle);
	{
		FScopeLock Lock(&CriticalSection);
		RegisteredExtensions.Add(Handle, FRegisteredExtension{ Extension });
	}

	if (FAngelscriptEngine* CurrentEngine = FAngelscriptEngine::TryGetCurrentEngine();
		CurrentEngine != nullptr && CurrentEngine->IsReadyForPublication())
	{
		Extension->OnEngineAttached(*CurrentEngine);
	}

	return Handle;
}

void FAngelscriptEngineExtensionRegistry::UnregisterExtension(FDelegateHandle Handle)
{
	if (!Handle.IsValid())
	{
		return;
	}

	FScopeLock Lock(&CriticalSection);
	RegisteredExtensions.Remove(Handle);
}

void FAngelscriptEngineExtensionRegistry::AttachEngine(FAngelscriptEngine& Engine)
{
	// A context scope can exist before its AngelScript engine has been created.
	// Such a scope identifies initialization ownership, but it is not yet an
	// active engine that extensions can safely observe or bind against.
	if (!ensureMsgf(
		Engine.GetScriptEngine() != nullptr,
		TEXT("Extensions can only attach after the target AngelScript engine has been created.")))
	{
		return;
	}

	TArray<TSharedRef<IAngelscriptExtension>> Extensions;
	{
		FScopeLock Lock(&CriticalSection);
		Extensions.Reserve(RegisteredExtensions.Num());
		for (const TPair<FDelegateHandle, FRegisteredExtension>& Pair : RegisteredExtensions)
		{
			Extensions.Add(Pair.Value.Extension);
		}
	}

	for (const TSharedRef<IAngelscriptExtension>& Extension : Extensions)
	{
		Extension->OnEngineAttached(Engine);
	}
}

void FAngelscriptEngineExtensionRegistry::DetachEngine(FAngelscriptEngine& Engine)
{
	TArray<TSharedRef<IAngelscriptExtension>> Extensions;
	{
		FScopeLock Lock(&CriticalSection);
		Extensions.Reserve(RegisteredExtensions.Num());
		for (const TPair<FDelegateHandle, FRegisteredExtension>& Pair : RegisteredExtensions)
		{
			Extensions.Add(Pair.Value.Extension);
		}
	}

	for (const TSharedRef<IAngelscriptExtension>& Extension : Extensions)
	{
		Extension->OnEngineDetached(Engine);
	}
}

void FAngelscriptEngineExtensionRegistry::ReplayCurrentEngine()
{
	if (FAngelscriptEngine* CurrentEngine = FAngelscriptEngine::TryGetCurrentEngine();
		CurrentEngine != nullptr && CurrentEngine->IsReadyForPublication())
	{
		AttachEngine(*CurrentEngine);
	}
}

void FAngelscriptEngineExtensionRegistry::DetachCurrentEngine()
{
	if (FAngelscriptEngine* CurrentEngine = FAngelscriptEngine::TryGetCurrentEngine())
	{
		DetachEngine(*CurrentEngine);
	}
}

int32 FAngelscriptEngineExtensionRegistry::NumExtensions() const
{
	FScopeLock Lock(&CriticalSection);
	return RegisteredExtensions.Num();
}
