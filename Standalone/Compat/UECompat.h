#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <initializer_list>
#include <limits>
#include <new>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#if defined(_MSC_VER)
#include <malloc.h>
#endif

using int8 = std::int8_t;
using int16 = std::int16_t;
using int32 = std::int32_t;
using int64 = std::int64_t;
using uint8 = std::uint8_t;
using uint16 = std::uint16_t;
using uint32 = std::uint32_t;
using uint64 = std::uint64_t;
using SIZE_T = std::size_t;
using TCHAR = char;
using ANSICHAR = char;
using nullptr_t = std::nullptr_t;

#ifndef FORCEINLINE
#define FORCEINLINE inline
#endif

#ifndef TEXT
#define TEXT(Value) Value
#endif

#ifndef ANSI_TO_TCHAR
#define ANSI_TO_TCHAR(Value) Value
#endif

#ifndef TCHAR_TO_ANSI
#define TCHAR_TO_ANSI(Value) Value
#endif

#ifndef TCHAR_TO_UTF8
#define TCHAR_TO_UTF8(Value) Value
#endif

#ifndef UTF8_TO_TCHAR
#define UTF8_TO_TCHAR(Value) Value
#endif

#ifndef INDEX_NONE
#define INDEX_NONE (-1)
#endif

#ifndef STDCALL
#if defined(_MSC_VER)
#define STDCALL __stdcall
#else
#define STDCALL
#endif
#endif

#ifndef check
#define check(Expression) do { assert(Expression); } while (false)
#endif

#ifndef checkSlow
#define checkSlow(Expression) do { assert(Expression); } while (false)
#endif

#ifndef ensure
#define ensure(Expression) (Expression)
#endif

#ifndef ensureMsgf
#define ensureMsgf(Expression, Format, ...) (Expression)
#endif

#ifndef UE_LOG
#define UE_LOG(Category, Verbosity, Format, ...) ((void)0)
#endif

#ifndef MoveTemp
#define MoveTemp(Value) std::move(Value)
#endif

#ifndef MAX_int8
#define MAX_int8 (std::numeric_limits<int8>::max())
#endif

#ifndef MIN_int8
#define MIN_int8 (std::numeric_limits<int8>::min())
#endif

#ifndef MAX_int32
#define MAX_int32 (std::numeric_limits<int32>::max())
#endif

inline bool GIsEditor = false;

template <bool Condition, class Result = void>
struct TEnableIf
{
};

template <class Result>
struct TEnableIf<true, Result>
{
	using Type = Result;
};

template <class T>
struct TIsReferenceType
{
	static constexpr bool Value = std::is_reference_v<T>;
};

template <class T>
struct TRemoveReference
{
	using Type = std::remove_reference_t<T>;
};

template <class T>
struct TIsPointer
{
	static constexpr bool Value = std::is_pointer_v<T>;
};

template <class T, T... Values>
using TIntegerSequence = std::integer_sequence<T, Values...>;

template <class T, T Count>
using TMakeIntegerSequence = std::make_integer_sequence<T, Count>;

template <class T>
constexpr T Align(const T Value, const std::size_t Alignment)
{
	return Alignment == 0
		? Value
		: static_cast<T>((static_cast<std::size_t>(Value) + Alignment - 1)
			& ~(Alignment - 1));
}

struct FMemory
{
	using FAllocate = void* (*)(std::size_t, std::size_t);
	using FFree = void (*)(void*);

	static void* DefaultMalloc(const std::size_t Size, std::size_t Alignment)
	{
		Alignment = std::max(Alignment, alignof(void*));
#if defined(_MSC_VER)
		return _aligned_malloc(Size, Alignment);
#else
		void* Result = nullptr;
		return posix_memalign(&Result, Alignment, Size) == 0 ? Result : nullptr;
#endif
	}

	static void DefaultFree(void* Address)
	{
#if defined(_MSC_VER)
		_aligned_free(Address);
#else
		std::free(Address);
#endif
	}

	static void SetAllocationFunctions(FAllocate Allocate, FFree Free)
	{
		Allocator = Allocate == nullptr ? &DefaultMalloc : Allocate;
		Deallocator = Free == nullptr ? &DefaultFree : Free;
	}

	static void ResetAllocationFunctions()
	{
		Allocator = &DefaultMalloc;
		Deallocator = &DefaultFree;
	}

	static void* Malloc(
		const std::size_t Size,
		const std::size_t Alignment = alignof(std::max_align_t))
	{
		return Allocator(Size, Alignment);
	}

	static void Free(void* Address)
	{
		if (Address != nullptr)
		{
			Deallocator(Address);
		}
	}

	static void* Memcpy(void* Destination, const void* Source, const std::size_t Size)
	{
		return std::memcpy(Destination, Source, Size);
	}

	static void* Memset(void* Destination, const uint8 Value, const std::size_t Size)
	{
		return std::memset(Destination, Value, Size);
	}

	static int32 Memcmp(const void* Left, const void* Right, const std::size_t Size)
	{
		return std::memcmp(Left, Right, Size);
	}

private:
	static inline FAllocate Allocator = &DefaultMalloc;
	static inline FFree Deallocator = &DefaultFree;
};

template <class T>
class TStandaloneFMemoryAllocator
{
public:
	using value_type = T;
	using is_always_equal = std::true_type;

	TStandaloneFMemoryAllocator() noexcept = default;

	template <class OtherType>
	TStandaloneFMemoryAllocator(
		const TStandaloneFMemoryAllocator<OtherType>&) noexcept
	{
	}

	[[nodiscard]] T* allocate(const std::size_t Count)
	{
		if (Count > std::numeric_limits<std::size_t>::max() / sizeof(T))
		{
			throw std::bad_array_new_length();
		}
		void* Address = FMemory::Malloc(Count * sizeof(T), alignof(T));
		if (Address == nullptr)
		{
			throw std::bad_alloc();
		}
		return static_cast<T*>(Address);
	}

	void deallocate(T* Address, std::size_t) noexcept
	{
		FMemory::Free(Address);
	}

	template <class OtherType>
	bool operator==(const TStandaloneFMemoryAllocator<OtherType>&) const noexcept
	{
		return true;
	}
};

enum class EAllowShrinking
{
	No,
	Yes,
};

template <int InlineCount>
struct TInlineAllocator
{
};

template <class T>
class TGuardValue
{
public:
	TGuardValue(T& InTarget, const T& InTemporaryValue)
		: Target(InTarget)
		, OriginalValue(InTarget)
	{
		Target = InTemporaryValue;
	}

	~TGuardValue()
	{
		Target = OriginalValue;
	}

	TGuardValue(const TGuardValue&) = delete;
	TGuardValue& operator=(const TGuardValue&) = delete;

private:
	T& Target;
	T OriginalValue;
};

template <class T>
TGuardValue(T&, const T&) -> TGuardValue<T>;

template <class T, class Allocator = void>
class TArray
{
public:
	TArray() = default;
	TArray(std::initializer_list<T> InitialValues)
		: Values(InitialValues)
	{
	}
	TArray(const TArray&) = default;
	TArray(TArray&&) noexcept = default;
	TArray& operator=(const TArray&) = default;
	TArray& operator=(TArray&&) noexcept = default;

	int32 Num() const
	{
		return static_cast<int32>(Values.size());
	}

	bool IsEmpty() const
	{
		return Values.empty();
	}

	void Reset()
	{
		Values.clear();
	}

	void Empty()
	{
		decltype(Values) EmptyValues;
		Values.swap(EmptyValues);
	}

	void Reserve(const int32 Count)
	{
		if (Count > 0)
		{
			Values.reserve(static_cast<std::size_t>(Count));
		}
	}

	int32 Add(const T& Value)
	{
		Values.push_back(Value);
		return Num() - 1;
	}

	int32 Add(T&& Value)
	{
		Values.push_back(std::move(Value));
		return Num() - 1;
	}

	int32 AddUninitialized(const int32 Count = 1)
	{
		static_assert(
			std::is_trivially_default_constructible_v<T>
				&& std::is_trivially_destructible_v<T>,
			"Standalone AddUninitialized is restricted to trivial element types");
		const int32 FirstIndex = Num();
		if (Count > 0)
		{
			Values.resize(Values.size() + static_cast<std::size_t>(Count));
		}
		return FirstIndex;
	}

	template <class... Args>
	int32 Emplace(Args&&... Arguments)
	{
		Values.emplace_back(std::forward<Args>(Arguments)...);
		return Num() - 1;
	}

	template <class... Args>
	T& Emplace_GetRef(Args&&... Arguments)
	{
		return Values.emplace_back(std::forward<Args>(Arguments)...);
	}

	bool Contains(const T& Value) const
	{
		return std::find(Values.begin(), Values.end(), Value) != Values.end();
	}

	int32 IndexOfByKey(const T& Value) const
	{
		const auto Iterator = std::find(Values.begin(), Values.end(), Value);
		return Iterator == Values.end()
			? INDEX_NONE
			: static_cast<int32>(Iterator - Values.begin());
	}

	int32 IndexOf(const T& Value) const
	{
		return IndexOfByKey(Value);
	}

	int32 RemoveSingleSwap(const T& Value)
	{
		const int32 Index = IndexOfByKey(Value);
		if (Index == INDEX_NONE)
		{
			return 0;
		}
		RemoveAtSwap(Index);
		return 1;
	}

	int32 Remove(const T& Value)
	{
		const std::size_t PreviousSize = Values.size();
		Values.erase(std::remove(Values.begin(), Values.end(), Value), Values.end());
		return static_cast<int32>(PreviousSize - Values.size());
	}

	void RemoveAtSwap(const int32 Index)
	{
		check(Index >= 0 && Index < Num());
		const std::size_t StorageIndex = static_cast<std::size_t>(Index);
		if (StorageIndex + 1 != Values.size())
		{
			Values[StorageIndex] = std::move(Values.back());
		}
		Values.pop_back();
	}

	T Pop(const EAllowShrinking = EAllowShrinking::Yes)
	{
		check(!Values.empty());
		T Result = std::move(Values.back());
		Values.pop_back();
		return Result;
	}

	T& Last()
	{
		return Values.back();
	}

	const T& Last() const
	{
		return Values.back();
	}

	T* GetData()
	{
		return Values.data();
	}

	const T* GetData() const
	{
		return Values.data();
	}

	T& operator[](const int32 Index)
	{
		return Values[static_cast<std::size_t>(Index)];
	}

	const T& operator[](const int32 Index) const
	{
		return Values[static_cast<std::size_t>(Index)];
	}

	auto begin() { return Values.begin(); }
	auto end() { return Values.end(); }
	auto begin() const { return Values.begin(); }
	auto end() const { return Values.end(); }

private:
	std::vector<T, TStandaloneFMemoryAllocator<T>> Values;
};

template <class KeyType, class ValueType>
struct TPair
{
	TPair() = default;
	TPair(const KeyType& InKey, const ValueType& InValue)
		: Key(InKey)
		, Value(InValue)
	{
	}

	template <class OtherPair>
	TPair(const OtherPair& Other)
		: Key(Other.Key)
		, Value(Other.Value)
	{
	}

	bool operator==(const TPair& Other) const
	{
		return Key == Other.Key && Value == Other.Value;
	}

	KeyType Key{};
	ValueType Value{};
};

template <class KeyType, class ValueType>
class TMap
{
public:
	using FEntry = TPair<KeyType, ValueType>;

	TMap() = default;
	TMap(const TMap&) = default;
	TMap(TMap&&) noexcept = default;
	TMap& operator=(const TMap&) = default;
	TMap& operator=(TMap&&) noexcept = default;

	int32 Num() const { return static_cast<int32>(Entries.size()); }
	bool IsEmpty() const { return Entries.empty(); }
	void Reset() { Entries.clear(); }
	void Empty()
	{
		decltype(Entries) EmptyEntries;
		Entries.swap(EmptyEntries);
	}

	void Reserve(const int32 Count)
	{
		if (Count > 0)
		{
			Entries.reserve(static_cast<std::size_t>(Count));
		}
	}

	ValueType& Add(const KeyType& Key, const ValueType& Value)
	{
		if (ValueType* Existing = Find(Key))
		{
			*Existing = Value;
			return *Existing;
		}
		Entries.emplace_back(Key, Value);
		return Entries.back().Value;
	}

	ValueType& Add(const KeyType& Key, ValueType&& Value)
	{
		if (ValueType* Existing = Find(Key))
		{
			*Existing = std::move(Value);
			return *Existing;
		}
		Entries.emplace_back(Key, std::move(Value));
		return Entries.back().Value;
	}

	ValueType* Find(const KeyType& Key)
	{
		for (FEntry& Entry : Entries)
		{
			if (Entry.Key == Key)
			{
				return &Entry.Value;
			}
		}
		return nullptr;
	}

	const ValueType* Find(const KeyType& Key) const
	{
		for (const FEntry& Entry : Entries)
		{
			if (Entry.Key == Key)
			{
				return &Entry.Value;
			}
		}
		return nullptr;
	}

	ValueType FindRef(const KeyType& Key) const
	{
		const ValueType* Existing = Find(Key);
		return Existing == nullptr ? ValueType{} : *Existing;
	}

	ValueType& FindChecked(const KeyType& Key)
	{
		ValueType* Existing = Find(Key);
		check(Existing != nullptr);
		return *Existing;
	}

	const ValueType& FindChecked(const KeyType& Key) const
	{
		const ValueType* Existing = Find(Key);
		check(Existing != nullptr);
		return *Existing;
	}

	bool Contains(const KeyType& Key) const
	{
		return Find(Key) != nullptr;
	}

	int32 Remove(const KeyType& Key)
	{
		const std::size_t PreviousSize = Entries.size();
		Entries.erase(
			std::remove_if(
				Entries.begin(), Entries.end(),
				[&](const FEntry& Entry) { return Entry.Key == Key; }),
			Entries.end());
		return static_cast<int32>(PreviousSize - Entries.size());
	}

	auto begin() { return Entries.begin(); }
	auto end() { return Entries.end(); }
	auto begin() const { return Entries.begin(); }
	auto end() const { return Entries.end(); }

private:
	std::vector<FEntry, TStandaloneFMemoryAllocator<FEntry>> Entries;
};

template <class KeyType, class ValueType>
class TMultiMap
{
public:
	using FEntry = TPair<KeyType, ValueType>;

	class FConstKeyIterator
	{
	public:
		FConstKeyIterator(const TMultiMap* InOwner, const KeyType& InKey)
			: Owner(InOwner)
			, Key(InKey)
		{
			MoveToMatch();
		}

		explicit operator bool() const
		{
			return Owner != nullptr && Index < Owner->Entries.size();
		}

		FConstKeyIterator& operator++()
		{
			++Index;
			MoveToMatch();
			return *this;
		}

		const ValueType& Value() const
		{
			return Owner->Entries[Index].Value;
		}

	private:
		void MoveToMatch()
		{
			while (Owner != nullptr
				&& Index < Owner->Entries.size()
				&& !(Owner->Entries[Index].Key == Key))
			{
				++Index;
			}
		}

		const TMultiMap* Owner = nullptr;
		KeyType Key;
		std::size_t Index = 0;
	};

	void Empty()
	{
		decltype(Entries) EmptyEntries;
		Entries.swap(EmptyEntries);
	}
	void Reset() { Entries.clear(); }
	void Reserve(const int32 Count)
	{
		if (Count > 0)
		{
			Entries.reserve(static_cast<std::size_t>(Count));
		}
	}

	void AddUnique(const KeyType& Key, const ValueType& Value)
	{
		if (FindPair(Key, Value) == nullptr)
		{
			Entries.emplace_back(Key, Value);
		}
	}

	int32 Remove(const KeyType& Key, const ValueType& Value)
	{
		const std::size_t PreviousSize = Entries.size();
		Entries.erase(
			std::remove_if(
				Entries.begin(), Entries.end(),
				[&](const FEntry& Entry)
				{
					return Entry.Key == Key && Entry.Value == Value;
				}),
			Entries.end());
		return static_cast<int32>(PreviousSize - Entries.size());
	}

	const FEntry* FindPair(const KeyType& Key, const ValueType& Value) const
	{
		const auto Iterator = std::find_if(
			Entries.begin(), Entries.end(),
			[&](const FEntry& Entry)
			{
				return Entry.Key == Key && Entry.Value == Value;
			});
		return Iterator == Entries.end() ? nullptr : &*Iterator;
	}

	FConstKeyIterator CreateConstKeyIterator(const KeyType& Key) const
	{
		return FConstKeyIterator(this, Key);
	}

	auto begin() const { return Entries.begin(); }
	auto end() const { return Entries.end(); }

private:
	std::vector<FEntry, TStandaloneFMemoryAllocator<FEntry>> Entries;
};

template <class T>
class TSet
{
public:
	bool Contains(const T& Value) const
	{
		return std::find(Values.begin(), Values.end(), Value) != Values.end();
	}

	void Add(const T& Value)
	{
		if (!Contains(Value))
		{
			Values.push_back(Value);
		}
	}

	int32 Num() const { return static_cast<int32>(Values.size()); }
	void Reset() { Values.clear(); }
	auto begin() const { return Values.begin(); }
	auto end() const { return Values.end(); }

private:
	std::vector<T, TStandaloneFMemoryAllocator<T>> Values;
};

template <class T>
uint32 GetTypeHash(const T& Value)
{
	return static_cast<uint32>(std::hash<T>{}(Value));
}

inline uint32 HashCombineFast(const uint32 A, const uint32 B)
{
	return A ^ (B + 0x9e3779b9u + (A << 6u) + (A >> 2u));
}

struct FPlatformAtomics
{
	static int32 InterlockedIncrement(int32* Value)
	{
		return ++std::atomic_ref<int32>(*Value);
	}

	static int32 InterlockedDecrement(int32* Value)
	{
		return --std::atomic_ref<int32>(*Value);
	}
};

struct FMath
{
	template <class T>
	static T Min(const T A, const T B) { return A < B ? A : B; }

	template <class T>
	static T Max(const T A, const T B) { return A > B ? A : B; }

	template <class T>
	static T Min3(const T A, const T B, const T C)
	{
		return Min(Min(A, B), C);
	}

	template <class A, class B>
	static auto Pow(const A Base, const B Exponent)
	{
		return std::pow(Base, Exponent);
	}
};

struct FCStringAnsi
{
	static const char* Strstr(const char* Text, const char* Search)
	{
		return Text == nullptr || Search == nullptr ? nullptr : std::strstr(Text, Search);
	}

	static double Atod(const char* Text)
	{
		return Text == nullptr ? 0.0 : std::strtod(Text, nullptr);
	}

	static float Atof(const char* Text)
	{
		return Text == nullptr ? 0.0f : std::strtof(Text, nullptr);
	}
};

struct FCrc
{
	static inline const std::array<std::array<uint32, 256>, 8> CRCTablesSB8 = []
	{
		std::array<std::array<uint32, 256>, 8> Tables{};
		for (uint32 Index = 0; Index < 256; ++Index)
		{
			uint32 Value = Index;
			for (int Bit = 0; Bit < 8; ++Bit)
			{
				Value = (Value >> 1u) ^ ((Value & 1u) ? 0xEDB88320u : 0u);
			}
			Tables[0][Index] = Value;
		}
		for (std::size_t Table = 1; Table < Tables.size(); ++Table)
		{
			for (uint32 Index = 0; Index < 256; ++Index)
			{
				const uint32 Previous = Tables[Table - 1][Index];
				Tables[Table][Index] =
					(Previous >> 8u) ^ Tables[0][Previous & 0xffu];
			}
		}
		return Tables;
	}();
};

class FMemStackBase
{
public:
	FMemStackBase() = default;
	FMemStackBase(const FMemStackBase&) = delete;
	FMemStackBase& operator=(const FMemStackBase&) = delete;

	~FMemStackBase()
	{
		for (void* Allocation : Allocations)
		{
			FMemory::Free(Allocation);
		}
	}

	void* Alloc(const std::size_t Size, const std::size_t Alignment)
	{
		void* Allocation = FMemory::Malloc(Size, Alignment);
		if (Allocation != nullptr)
		{
			Allocations.push_back(Allocation);
		}
		return Allocation;
	}

private:
	std::vector<void*, TStandaloneFMemoryAllocator<void*>> Allocations;
};
