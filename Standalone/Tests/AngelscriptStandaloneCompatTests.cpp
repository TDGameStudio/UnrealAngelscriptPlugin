#include "CoreMinimal.h"
#include "CoreTypes.h"
#include "AngelscriptEngine.h"
#include "ClassGenerator/ASClass.h"
#include "HAL/PlatformAtomics.h"
#include "angelscript.h"
#include "as_string_util.h"

#include <cstdint>
#include <iostream>
#include <type_traits>

namespace
{
	std::size_t GOutstandingCompatAllocations = 0;

	class FExternalThreadManager final : public asIThreadManager
	{
	public:
		~FExternalThreadManager() override = default;
	};

	bool Require(const bool bCondition, const char* Message)
	{
		if (!bCondition)
		{
			std::cerr << Message << '\n';
		}
		return bCondition;
	}

	void* TestAllocate(const std::size_t Size, const std::size_t Alignment)
	{
		void* Address = FMemory::DefaultMalloc(Size, Alignment);
		if (Address != nullptr)
		{
			++GOutstandingCompatAllocations;
		}
		return Address;
	}

	void TestFree(void* Address)
	{
		if (Address != nullptr)
		{
			--GOutstandingCompatAllocations;
		}
		FMemory::DefaultFree(Address);
	}
}

int main()
{
	static_assert(std::is_same_v<int32, std::int32_t>);
	static_assert(std::is_same_v<uint64, std::uint64_t>);
	static_assert(TIsReferenceType<int&>::Value);
	static_assert(!TIsReferenceType<int>::Value);
	static_assert(TIsPointer<int*>::Value);
	static_assert(std::is_same_v<TRemoveReference<int&>::Type, int>);
	static_assert(!std::is_polymorphic_v<UObject>);
	static_assert(!std::is_polymorphic_v<UClass>);
	static_assert(!std::is_base_of_v<UObject, UClass>);

	bool bPassed = true;

	TArray<int, TInlineAllocator<2>> Values;
	Values.Reserve(4);
	Values.Add(1);
	Values.Emplace(2);
	Values.Emplace_GetRef(3) = 4;
	bPassed &= Require(Values.Num() == 3, "TArray Num mismatch");
	bPassed &= Require(Values.Contains(4), "TArray Contains mismatch");
	bPassed &= Require(Values.Pop(EAllowShrinking::No) == 4, "TArray Pop mismatch");
	static_assert(std::is_same_v<
		decltype(Values.RemoveSingleSwap(1)),
		int32>);
	bPassed &= Require(Values.RemoveSingleSwap(1) == 1, "TArray RemoveSingleSwap failed");
	bPassed &= Require(Values.Num() == 1 && Values[0] == 2, "TArray removal result mismatch");

	TMap<int, int> Map;
	Map.Reserve(4);
	Map.Add(1, 10);
	Map.Add(2, 20);
	Map.Add(2, 21);
	bPassed &= Require(Map.Num() == 2, "TMap duplicate Add mismatch");
	bPassed &= Require(Map.FindRef(2) == 21, "TMap FindRef mismatch");
	bPassed &= Require(Map.FindChecked(1) == 10, "TMap FindChecked mismatch");
	Map.Remove(1);
	bPassed &= Require(!Map.Contains(1), "TMap Remove mismatch");

	TMultiMap<int, int> MultiMap;
	MultiMap.AddUnique(7, 1);
	MultiMap.AddUnique(7, 1);
	MultiMap.AddUnique(7, 2);
	int Sum = 0;
	for (auto Iterator = MultiMap.CreateConstKeyIterator(7); Iterator; ++Iterator)
	{
		Sum += Iterator.Value();
	}
	bPassed &= Require(Sum == 3, "TMultiMap key iteration mismatch");
	MultiMap.Remove(7, 1);
	bPassed &= Require(MultiMap.FindPair(7, 1) == nullptr, "TMultiMap Remove mismatch");

	bool bGuardedValue = false;
	{
		TGuardValue<bool> Guard(bGuardedValue, true);
		bPassed &= Require(bGuardedValue, "TGuardValue did not assign temporary value");
	}
	bPassed &= Require(!bGuardedValue, "TGuardValue did not restore original value");

	FMemory::SetAllocationFunctions(&TestAllocate, &TestFree);
	void* Aligned = FMemory::Malloc(37, 32);
	bPassed &= Require(Aligned != nullptr, "FMemory allocation failed");
	bPassed &= Require(
		reinterpret_cast<std::uintptr_t>(Aligned) % 32 == 0,
		"FMemory alignment mismatch");
	FMemory::Memset(Aligned, 0x5a, 37);
	unsigned char Copy[37] = {};
	FMemory::Memcpy(Copy, Aligned, sizeof(Copy));
	bPassed &= Require(Copy[0] == 0x5a && Copy[36] == 0x5a, "FMemory copy/memset mismatch");
	FMemory::Free(Aligned);
	bPassed &= Require(
		GOutstandingCompatAllocations == 0,
		"FMemory callback allocation accounting mismatch");

	{
		TArray<int> CountedArray;
		const std::size_t EmptyArrayAllocations =
			GOutstandingCompatAllocations;
		CountedArray.Reserve(32);
		bPassed &= Require(
			GOutstandingCompatAllocations > EmptyArrayAllocations,
			"TArray storage did not route through FMemory");
		const std::size_t ReservedAllocations =
			GOutstandingCompatAllocations;
		CountedArray.Reset();
		bPassed &= Require(
			GOutstandingCompatAllocations == ReservedAllocations,
			"TArray Reset unexpectedly released reserved storage");
		CountedArray.Empty();
		bPassed &= Require(
			GOutstandingCompatAllocations == EmptyArrayAllocations,
			"TArray Empty did not release reserved storage");
	}
	{
		TMap<int, int> CountedMap;
		CountedMap.Reserve(16);
		TSet<int> CountedSet;
		CountedSet.Add(7);
		bPassed &= Require(
			GOutstandingCompatAllocations >= 2,
			"TMap/TSet storage did not route through FMemory");
	}
	bPassed &= Require(
		GOutstandingCompatAllocations == 0,
		"Compat container storage did not release through FMemory");
	FMemory::ResetAllocationFunctions();

	bPassed &= Require(
		asSetGlobalMemoryFunctions(&TestAllocate, &TestFree) == asSUCCESS,
		"asSetGlobalMemoryFunctions failed");
	Aligned = asAllocMem(37);
	bPassed &= Require(Aligned != nullptr, "asAllocMem callback allocation failed");
	asFreeMem(Aligned);
	bPassed &= Require(
		asResetGlobalMemoryFunctions() == asSUCCESS,
		"asResetGlobalMemoryFunctions failed");

	FMemStackBase Arena;
	void* ArenaValue = Arena.Alloc(sizeof(std::uint64_t), alignof(std::uint64_t));
	bPassed &= Require(ArenaValue != nullptr, "FMemStackBase allocation failed");
	bPassed &= Require(
		reinterpret_cast<std::uintptr_t>(ArenaValue) % alignof(std::uint64_t) == 0,
		"FMemStackBase alignment mismatch");

	int AtomicValue = 0;
	bPassed &= Require(
		FPlatformAtomics::InterlockedIncrement(&AtomicValue) == 1,
		"FPlatformAtomics increment mismatch");
	bPassed &= Require(
		FPlatformAtomics::InterlockedDecrement(&AtomicValue) == 0,
		"FPlatformAtomics decrement mismatch");

	bPassed &= Require(FMath::Max(3, 5) == 5, "FMath Max mismatch");
	bPassed &= Require(FMath::Min3(3, 1, 2) == 1, "FMath Min3 mismatch");
	bPassed &= Require(FCStringAnsi::Strstr("AngelScript", "Script") != nullptr,
		"FCStringAnsi Strstr mismatch");
	bPassed &= Require(
		asStringScanDouble("12.5") == 12.5,
		"asStringScanDouble mismatch");
	bPassed &= Require(
		asStringScanFloat("3.25") == 3.25f,
		"asStringScanFloat mismatch");

	bPassed &= Require(
		!FAngelscriptEngine::IsSimulatingCookedForCurrentContext(),
		"standalone engine policy unexpectedly simulates cooked");
	bPassed &= Require(
		!FAngelscriptEngine::CanCastScriptObjectToUnrealInterface(nullptr, nullptr, nullptr),
		"standalone engine policy unexpectedly permits UE interface cast");

	asAcquireExclusiveLock();
	asReleaseExclusiveLock();
	asAcquireSharedLock();
	asReleaseSharedLock();
	bPassed &= Require(asThreadCleanup() == asSUCCESS, "first thread cleanup failed");
	bPassed &= Require(asThreadCleanup() == asSUCCESS, "repeated thread cleanup failed");
	bPassed &= Require(
		asGetThreadManager() == nullptr,
		"thread manager unexpectedly prepared by cleanup");
	bPassed &= Require(
		asPrepareMultithread() == asSUCCESS,
		"standalone thread manager prepare failed");
	asIThreadManager* PreparedManager = asGetThreadManager();
	bPassed &= Require(
		PreparedManager != nullptr,
		"prepared standalone thread manager is missing");
	bPassed &= Require(
		asPrepareMultithread() == asSUCCESS
			&& asGetThreadManager() == PreparedManager,
		"repeated standalone thread manager prepare mismatch");
	asUnprepareMultithread();
	bPassed &= Require(
		asGetThreadManager() == PreparedManager,
		"thread manager released before prepare references balanced");
	asUnprepareMultithread();
	bPassed &= Require(
		asGetThreadManager() == nullptr,
		"thread manager survived balanced unprepare");
	FExternalThreadManager ExternalManager;
	FExternalThreadManager OtherExternalManager;
	bPassed &= Require(
		asPrepareMultithread(&ExternalManager) == asSUCCESS
			&& asGetThreadManager() == &ExternalManager,
		"external thread manager prepare mismatch");
	bPassed &= Require(
		asPrepareMultithread(&OtherExternalManager) == asINVALID_ARG,
		"conflicting external thread manager was not rejected");
	asUnprepareMultithread();
	bPassed &= Require(
		asGetThreadManager() == nullptr,
		"external thread manager survived unprepare");

	asIScriptEngine* Engine = asCreateScriptEngine();
	bPassed &= Require(Engine != nullptr, "compat raw-object engine creation failed");
	if (Engine != nullptr)
	{
		bPassed &= Require(
			Engine->RegisterObjectType("FCompatRawObject", 0, asOBJ_REF | asOBJ_NOCOUNT) >= 0,
			"compat raw-object type registration failed");
		asITypeInfo* Type = Engine->GetTypeInfoByDecl("FCompatRawObject");
		bPassed &= Require(Type != nullptr, "compat raw-object type lookup failed");
		if (Type != nullptr)
		{
			void* RawObject = FMemory::Malloc(sizeof(void*), alignof(void*));
			UASClass::RegisterRawScriptObject(RawObject, Type);
			bPassed &= Require(
				UASClass::GetRawScriptObjectType(RawObject) == Type,
				"compat raw-object type registry mismatch");
			bPassed &= Require(
				reinterpret_cast<asIScriptObject*>(RawObject)->GetObjectType() == Type,
				"compat asIScriptObject type lookup mismatch");
			bPassed &= Require(
				UASClass::AddRawScriptObjectReference(RawObject, Type),
				"compat raw-object AddRef failed");

			bool bRunDestructor = false;
			bool bFreeWithoutDestructor = false;
			bPassed &= Require(
				UASClass::BeginReleaseRawScriptObjectReference(
					RawObject,
					Type,
					bRunDestructor,
					bFreeWithoutDestructor)
					&& !bRunDestructor
					&& !bFreeWithoutDestructor,
				"compat raw-object non-final release mismatch");
			bPassed &= Require(
				UASClass::BeginReleaseRawScriptObjectReference(
					RawObject,
					Type,
					bRunDestructor,
					bFreeWithoutDestructor)
					&& bRunDestructor,
				"compat raw-object final release mismatch");
			bPassed &= Require(
				UASClass::FinishReleaseRawScriptObjectReference(RawObject, Type),
				"compat raw-object release completion failed");
			UASClass::UnregisterRawScriptObject(RawObject);
			FMemory::Free(RawObject);
		}
		Engine->ShutDownAndRelease();
	}
	bPassed &= Require(asThreadCleanup() == asSUCCESS, "post-engine thread cleanup failed");

	return bPassed ? 0 : 1;
}
