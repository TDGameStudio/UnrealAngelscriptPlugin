#include "Cache/AngelscriptCacheStore.h"

#include "HAL/CriticalSection.h"
#include "HAL/PlatformTime.h"

#if PLATFORM_WINDOWS || PLATFORM_LINUX || PLATFORM_MAC

namespace AngelscriptCacheStoreLock_Private
{
	class FPlatformNamespaceLockHandle final
		: public IAngelscriptCacheNamespaceLockHandle
	{
	public:
		explicit FPlatformNamespaceLockHandle(
			TUniquePtr<FSystemWideCriticalSection>&& InLock)
			: Lock(MoveTemp(InLock))
		{
		}

	private:
		TUniquePtr<FSystemWideCriticalSection> Lock;
	};

	class FPlatformNamespaceLockOps final : public IAngelscriptCacheNamespaceLockOps
	{
	public:
		virtual double MonotonicSeconds() const override
		{
			return FPlatformTime::Seconds();
		}

		virtual TUniquePtr<IAngelscriptCacheNamespaceLockHandle> TryAcquire(
			const FString& LockName,
			const FTimespan WaitSlice) override
		{
			TUniquePtr<FSystemWideCriticalSection> Lock =
				MakeUnique<FSystemWideCriticalSection>(LockName, WaitSlice);
			if (!Lock->IsValid())
			{
				return nullptr;
			}
			return MakeUnique<FPlatformNamespaceLockHandle>(MoveTemp(Lock));
		}
	};
}

#endif

TUniquePtr<IAngelscriptCacheNamespaceLockOps>
CreateAngelscriptCacheNamespaceLockOps()
{
#if PLATFORM_WINDOWS || PLATFORM_LINUX || PLATFORM_MAC
	return MakeUnique<AngelscriptCacheStoreLock_Private::FPlatformNamespaceLockOps>();
#else
	return nullptr;
#endif
}
