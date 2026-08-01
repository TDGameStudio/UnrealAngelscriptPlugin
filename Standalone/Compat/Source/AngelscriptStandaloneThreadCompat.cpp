#include "CoreMinimal.h"
#include "as_thread.h"

#include <new>
#include <mutex>
#include <shared_mutex>

BEGIN_AS_NAMESPACE

namespace
{
	class FStandaloneThreadLocalData final : public asCThreadLocalData
	{
	public:
		FStandaloneThreadLocalData() = default;
		~FStandaloneThreadLocalData() = default;
	};

	thread_local FStandaloneThreadLocalData* GThreadLocalData = nullptr;

	class FStandaloneThreadManager final : public asIThreadManager
	{
	public:
		~FStandaloneThreadManager() override = default;
	};

	std::mutex GThreadManagerMutex;
	asIThreadManager* GThreadManager = nullptr;
	FStandaloneThreadManager* GOwnedThreadManager = nullptr;
	int32 GThreadManagerReferenceCount = 0;

#ifndef AS_NO_THREADS
	std::shared_mutex GApplicationLock;
#endif
}

asCThreadLocalData* asCThreadManager::GetLocalData()
{
	if (GThreadLocalData == nullptr)
	{
		void* Storage = FMemory::Malloc(
			sizeof(FStandaloneThreadLocalData),
			alignof(FStandaloneThreadLocalData));
		if (Storage == nullptr)
		{
			return nullptr;
		}
		GThreadLocalData = new(Storage) FStandaloneThreadLocalData();
	}

	return GThreadLocalData;
}

asCThreadLocalData::asCThreadLocalData()
{
	activeFunction = nullptr;
}

asCThreadLocalData::~asCThreadLocalData() = default;

extern "C"
{
	int asThreadCleanup()
	{
		if (GThreadLocalData == nullptr)
		{
			return asSUCCESS;
		}
		if (GThreadLocalData->activeContext != nullptr)
		{
			return asCONTEXT_ACTIVE;
		}

		GThreadLocalData->~FStandaloneThreadLocalData();
		FMemory::Free(GThreadLocalData);
		GThreadLocalData = nullptr;
		return asSUCCESS;
	}

	asIThreadManager* asGetThreadManager()
	{
		std::scoped_lock Lock(GThreadManagerMutex);
		return GThreadManager;
	}

	int asPrepareMultithread(asIThreadManager* ExternalManager)
	{
		std::scoped_lock Lock(GThreadManagerMutex);
		if (ExternalManager != nullptr && GThreadManager != nullptr)
		{
			return asINVALID_ARG;
		}

		if (GThreadManager == nullptr)
		{
			if (ExternalManager != nullptr)
			{
				GThreadManager = ExternalManager;
			}
			else
			{
				void* Storage = FMemory::Malloc(
					sizeof(FStandaloneThreadManager),
					alignof(FStandaloneThreadManager));
				if (Storage == nullptr)
				{
					return asOUT_OF_MEMORY;
				}
				GOwnedThreadManager =
					new(Storage) FStandaloneThreadManager();
				GThreadManager = GOwnedThreadManager;
			}
			GThreadManagerReferenceCount = 1;
		}
		else
		{
			++GThreadManagerReferenceCount;
		}
		return asSUCCESS;
	}

	void asUnprepareMultithread()
	{
		FStandaloneThreadManager* OwnedManagerToDelete = nullptr;
		{
			std::scoped_lock Lock(GThreadManagerMutex);
			if (GThreadManager == nullptr)
			{
				return;
			}
			--GThreadManagerReferenceCount;
			if (GThreadManagerReferenceCount != 0)
			{
				return;
			}
			OwnedManagerToDelete = GOwnedThreadManager;
			GOwnedThreadManager = nullptr;
			GThreadManager = nullptr;
		}

		asThreadCleanup();
		if (OwnedManagerToDelete != nullptr)
		{
			OwnedManagerToDelete->~FStandaloneThreadManager();
			FMemory::Free(OwnedManagerToDelete);
		}
	}

	void asAcquireExclusiveLock()
	{
#ifndef AS_NO_THREADS
		GApplicationLock.lock();
#endif
	}

	void asReleaseExclusiveLock()
	{
#ifndef AS_NO_THREADS
		GApplicationLock.unlock();
#endif
	}

	void asAcquireSharedLock()
	{
#ifndef AS_NO_THREADS
		GApplicationLock.lock_shared();
#endif
	}

	void asReleaseSharedLock()
	{
#ifndef AS_NO_THREADS
		GApplicationLock.unlock_shared();
#endif
	}
}

END_AS_NAMESPACE
