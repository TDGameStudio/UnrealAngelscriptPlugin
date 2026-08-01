//
// Script std::string
//
// This function registers the std::string type with AngelScript to be used as the default string type.
//
// The string type is registered as a value type, thus may have performance issues if a lot of 
// string operations are performed in the script. However, for relatively few operations, this should
// not cause any problem for most applications.
//

#ifndef SCRIPTSTDSTRING_H
#define SCRIPTSTDSTRING_H

#ifndef ANGELSCRIPT_H 
// Avoid having to inform include path if header is already include before
#include <angelscript.h>
#endif

#include <string>
#include <functional>
#include <limits>
#include <new>
#include <string_view>
#include <type_traits>

//---------------------------
// Compilation settings
//

// Sometimes it may be desired to use the same method names as used by C++ STL.
// This may for example reduce time when converting code from script to C++ or
// back.
//
//  0 = off
//  1 = on
#ifndef AS_USE_STLNAMES
#define AS_USE_STLNAMES 0
#endif

// Some prefer to use property accessors to get/set the length of the string
// This option registers the accessors instead of the method length()
#ifndef AS_USE_ACCESSORS
#define AS_USE_ACCESSORS 0
#endif

// This option disables the implicit operators with primitives
#ifndef AS_NO_IMPL_OPS_WITH_STRING_AND_PRIMITIVE
#define AS_NO_IMPL_OPS_WITH_STRING_AND_PRIMITIVE 0
#endif

BEGIN_AS_NAMESPACE

// The standalone profile routes script-owned string storage through the
// AngelScript allocation callbacks so it participates in the configured
// runtime memory limit. This allocator is intentionally local to the copied
// add-ons; it does not alter the maintained fork's public API.
template<typename T>
class TScriptStdAllocator
{
public:
	using value_type = T;
	using is_always_equal = std::true_type;

	TScriptStdAllocator() noexcept = default;

	template<typename U>
	TScriptStdAllocator(const TScriptStdAllocator<U>&) noexcept
	{
	}

	T* allocate(std::size_t Count)
	{
		if (Count > std::numeric_limits<std::size_t>::max() / sizeof(T))
		{
			throw std::bad_alloc();
		}
		void* Address = asAllocMem(Count * sizeof(T));
		if (Address == nullptr)
		{
			throw std::bad_alloc();
		}
		return static_cast<T*>(Address);
	}

	void deallocate(T* Address, std::size_t) noexcept
	{
		asFreeMem(Address);
	}
};

template<typename T, typename U>
bool operator==(const TScriptStdAllocator<T>&, const TScriptStdAllocator<U>&) noexcept
{
	return true;
}

template<typename T, typename U>
bool operator!=(const TScriptStdAllocator<T>&, const TScriptStdAllocator<U>&) noexcept
{
	return false;
}

using scriptstring_t = std::basic_string<
	char,
	std::char_traits<char>,
	TScriptStdAllocator<char>>;

struct FScriptStringHash
{
	std::size_t operator()(const scriptstring_t& Value) const noexcept
	{
		return std::hash<std::string_view>{}(
			std::string_view(Value.data(), Value.size()));
	}
};

void RegisterStdString(asIScriptEngine *engine);
void RegisterStdStringUtils(asIScriptEngine *engine);

END_AS_NAMESPACE

#endif
