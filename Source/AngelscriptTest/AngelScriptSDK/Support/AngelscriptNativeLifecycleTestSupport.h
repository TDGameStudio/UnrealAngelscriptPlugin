#pragma once

#include "CoreMinimal.h"

#include <initializer_list>

namespace AngelscriptNativeTestSupport
{
	enum class ENativeLifecycleEvent : uint8
	{
		DefaultConstruct,
		ValueConstruct,
		CopyConstruct,
		Assign,
		Destruct,
		AddRef,
		Release,
		WeakFlagRead,
		GarbageCollectorNotify,
		IteratorBegin,
		IteratorNext,
		IteratorValue,
	};

	struct FNativeLifecycleEntry
	{
		ENativeLifecycleEvent Event = ENativeLifecycleEvent::DefaultConstruct;
		int32 ObjectId = INDEX_NONE;
		int32 RelatedObjectId = INDEX_NONE;
		int32 Value = 0;
	};

	class FNativeLifecycleRecorder
	{
	public:
		void Reset()
		{
			Entries.Reset();
			NextObjectId = 1;
			LiveObjectCount = 0;
		}

		int32 AllocateObjectId()
		{
			++LiveObjectCount;
			return NextObjectId++;
		}

		void Record(
			const ENativeLifecycleEvent Event,
			const int32 ObjectId,
			const int32 RelatedObjectId = INDEX_NONE,
			const int32 Value = 0)
		{
			Entries.Add({ Event, ObjectId, RelatedObjectId, Value });
			if (Event == ENativeLifecycleEvent::Destruct)
			{
				LiveObjectCount = FMath::Max(0, LiveObjectCount - 1);
			}
		}

		const TArray<FNativeLifecycleEntry>& GetEntries() const
		{
			return Entries;
		}

		int32 Num(const ENativeLifecycleEvent Event) const
		{
			int32 Count = 0;
			for (const FNativeLifecycleEntry& Entry : Entries)
			{
				Count += Entry.Event == Event ? 1 : 0;
			}
			return Count;
		}

		int32 GetLiveObjectCount() const
		{
			return LiveObjectCount;
		}

		bool HasExactEventOrder(std::initializer_list<ENativeLifecycleEvent> Expected) const
		{
			if (Entries.Num() != static_cast<int32>(Expected.size()))
			{
				return false;
			}

			int32 Index = 0;
			for (const ENativeLifecycleEvent ExpectedEvent : Expected)
			{
				if (Entries[Index++].Event != ExpectedEvent)
				{
					return false;
				}
			}
			return true;
		}

	private:
		TArray<FNativeLifecycleEntry> Entries;
		int32 NextObjectId = 1;
		int32 LiveObjectCount = 0;
	};

	inline const TCHAR* ToNativeLifecycleEventString(
		const ENativeLifecycleEvent Event)
	{
		switch (Event)
		{
		case ENativeLifecycleEvent::DefaultConstruct:
			return TEXT("DefaultConstruct");
		case ENativeLifecycleEvent::ValueConstruct:
			return TEXT("ValueConstruct");
		case ENativeLifecycleEvent::CopyConstruct:
			return TEXT("CopyConstruct");
		case ENativeLifecycleEvent::Assign:
			return TEXT("Assign");
		case ENativeLifecycleEvent::Destruct:
			return TEXT("Destruct");
		case ENativeLifecycleEvent::AddRef:
			return TEXT("AddRef");
		case ENativeLifecycleEvent::Release:
			return TEXT("Release");
		case ENativeLifecycleEvent::WeakFlagRead:
			return TEXT("WeakFlagRead");
		case ENativeLifecycleEvent::GarbageCollectorNotify:
			return TEXT("GarbageCollectorNotify");
		case ENativeLifecycleEvent::IteratorBegin:
			return TEXT("IteratorBegin");
		case ENativeLifecycleEvent::IteratorNext:
			return TEXT("IteratorNext");
		case ENativeLifecycleEvent::IteratorValue:
			return TEXT("IteratorValue");
		default:
			return TEXT("Unknown");
		}
	}

	inline FString CollectNativeLifecycleEntries(
		const FNativeLifecycleRecorder& Recorder)
	{
		FString Result;
		for (const FNativeLifecycleEntry& Entry : Recorder.GetEntries())
		{
			if (!Result.IsEmpty())
			{
				Result += TEXT(", ");
			}

			Result += FString::Printf(
				TEXT("%s(id=%d, related=%d, value=%d)"),
				ToNativeLifecycleEventString(Entry.Event),
				Entry.ObjectId,
				Entry.RelatedObjectId,
				Entry.Value);
		}

		return Result.IsEmpty() ? TEXT("<no lifecycle entries>") : Result;
	}

	class FNativeLifecycleFaultController
	{
	public:
		void Reset()
		{
			bThrowOnNextCopy = false;
			TriggeredCopyCount = 0;
		}

		void ArmNextCopy()
		{
			bThrowOnNextCopy = true;
		}

		bool ConsumeCopyFault()
		{
			if (!bThrowOnNextCopy)
			{
				return false;
			}

			bThrowOnNextCopy = false;
			++TriggeredCopyCount;
			return true;
		}

		bool IsArmed() const
		{
			return bThrowOnNextCopy;
		}

		int32 GetTriggeredCopyCount() const
		{
			return TriggeredCopyCount;
		}

	private:
		bool bThrowOnNextCopy = false;
		int32 TriggeredCopyCount = 0;
	};

	struct FNativeTrackedValue
	{
		FNativeTrackedValue() = default;

		explicit FNativeTrackedValue(FNativeLifecycleRecorder* InRecorder, const int32 InValue = 0)
			: Recorder(InRecorder)
			, ObjectId(Recorder != nullptr ? Recorder->AllocateObjectId() : INDEX_NONE)
			, Value(InValue)
		{
			if (Recorder != nullptr)
			{
				Recorder->Record(InValue == 0 ? ENativeLifecycleEvent::DefaultConstruct : ENativeLifecycleEvent::ValueConstruct, ObjectId, INDEX_NONE, Value);
			}
		}

		FNativeTrackedValue(const FNativeTrackedValue& Other)
			: Recorder(Other.Recorder)
			, ObjectId(Recorder != nullptr ? Recorder->AllocateObjectId() : INDEX_NONE)
			, Value(Other.Value)
		{
			if (Recorder != nullptr)
			{
				Recorder->Record(ENativeLifecycleEvent::CopyConstruct, ObjectId, Other.ObjectId, Value);
			}
		}

		FNativeTrackedValue& operator=(const FNativeTrackedValue& Other)
		{
			if (this != &Other)
			{
				Recorder = Other.Recorder;
				Value = Other.Value;
			}
			if (Recorder != nullptr)
			{
				Recorder->Record(ENativeLifecycleEvent::Assign, ObjectId, Other.ObjectId, Value);
			}
			return *this;
		}

		~FNativeTrackedValue()
		{
			if (Recorder != nullptr)
			{
				Recorder->Record(ENativeLifecycleEvent::Destruct, ObjectId, INDEX_NONE, Value);
			}
		}

		FNativeLifecycleRecorder* Recorder = nullptr;
		int32 ObjectId = INDEX_NONE;
		int32 Value = 0;
	};
}
