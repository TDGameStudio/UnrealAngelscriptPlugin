#include "CoreMinimal.h"
#include "angelscript.h"
#include "ClassGenerator/ASClass.h"
#include "as_string_util.h"

#include <cstdlib>
#include <memory>
#include <mutex>
#include <unordered_map>

namespace
{
	struct FRawScriptObjectRegistration
	{
		asITypeInfo* ScriptType = nullptr;
		asIScriptEngine* ScriptEngine = nullptr;
		int32 ReferenceCount = 0;
		bool bDestructorCalled = false;
		bool bDestructionInProgress = false;
	};

	std::mutex GRawScriptObjectMutex;
	using FRawScriptObjectRegistrationMap = std::unordered_map<
		const void*,
		FRawScriptObjectRegistration,
		std::hash<const void*>,
		std::equal_to<const void*>,
		TStandaloneFMemoryAllocator<std::pair<
			const void* const,
			FRawScriptObjectRegistration>>>;
	std::unique_ptr<FRawScriptObjectRegistrationMap> GRawScriptObjectRegistrations;

	void ReleaseEmptyRawScriptObjectRegistry()
	{
		if (GRawScriptObjectRegistrations != nullptr
			&& GRawScriptObjectRegistrations->empty())
		{
			GRawScriptObjectRegistrations.reset();
		}
	}

	struct FRawScriptObjectHookInstaller
	{
		FRawScriptObjectHookInstaller()
		{
			asSetAllocScriptObjectFunction(
				&UASClass::AllocScriptObject,
				&UASClass::FinishConstructObject);
		}
	};

	FRawScriptObjectHookInstaller GRawScriptObjectHookInstaller;
}

void* UASClass::AllocScriptObject(
	asITypeInfo* ScriptType,
	const std::size_t Size)
{
	if (ScriptType == nullptr)
	{
		return nullptr;
	}

	void* Memory = FMemory::Malloc(Size, ScriptType->alignment);
	if (Memory == nullptr)
	{
		return nullptr;
	}
	FMemory::Memset(Memory, 0, Size);

	if ((ScriptType->GetFlags() & asOBJ_VALUE) == 0)
	{
		RegisterRawScriptObject(Memory, ScriptType);
	}
	return Memory;
}

void UASClass::FinishConstructObject(asIScriptObject*, asITypeInfo*)
{
}

void UASClass::RegisterRawScriptObject(
	void* ScriptObject,
	asITypeInfo* ScriptType)
{
	if (ScriptObject == nullptr || ScriptType == nullptr)
	{
		return;
	}

	std::scoped_lock Lock(GRawScriptObjectMutex);
	if (GRawScriptObjectRegistrations == nullptr)
	{
		GRawScriptObjectRegistrations =
			std::make_unique<FRawScriptObjectRegistrationMap>();
	}
	(*GRawScriptObjectRegistrations)[ScriptObject] = {
		ScriptType,
		ScriptType->GetEngine(),
		1,
		false,
		false,
	};
}

bool UASClass::AddRawScriptObjectReference(
	const void* ScriptObject,
	asITypeInfo* ExpectedScriptType)
{
	std::scoped_lock Lock(GRawScriptObjectMutex);
	if (GRawScriptObjectRegistrations == nullptr)
	{
		return false;
	}
	const auto Iterator = GRawScriptObjectRegistrations->find(ScriptObject);
	if (Iterator == GRawScriptObjectRegistrations->end()
		|| Iterator->second.ScriptType != ExpectedScriptType
		|| Iterator->second.ReferenceCount <= 0
		|| Iterator->second.ReferenceCount == MAX_int32)
	{
		return false;
	}

	++Iterator->second.ReferenceCount;
	return true;
}

bool UASClass::BeginReleaseRawScriptObjectReference(
	const void* ScriptObject,
	asITypeInfo* ExpectedScriptType,
	bool& bOutRunDestructor,
	bool& bOutFreeWithoutDestructor)
{
	bOutRunDestructor = false;
	bOutFreeWithoutDestructor = false;

	std::scoped_lock Lock(GRawScriptObjectMutex);
	if (GRawScriptObjectRegistrations == nullptr)
	{
		return false;
	}
	const auto Iterator = GRawScriptObjectRegistrations->find(ScriptObject);
	if (Iterator == GRawScriptObjectRegistrations->end()
		|| Iterator->second.ScriptType != ExpectedScriptType
		|| Iterator->second.ReferenceCount <= 0)
	{
		return false;
	}

	FRawScriptObjectRegistration& Registration = Iterator->second;
	if (Registration.ReferenceCount > 1)
	{
		--Registration.ReferenceCount;
		return true;
	}
	if (Registration.bDestructionInProgress)
	{
		return true;
	}
	if (!Registration.bDestructorCalled)
	{
		Registration.bDestructorCalled = true;
		Registration.bDestructionInProgress = true;
		bOutRunDestructor = true;
		return true;
	}

	--Registration.ReferenceCount;
	bOutFreeWithoutDestructor = Registration.ReferenceCount == 0;
	return true;
}

bool UASClass::FinishReleaseRawScriptObjectReference(
	const void* ScriptObject,
	asITypeInfo* ExpectedScriptType)
{
	std::scoped_lock Lock(GRawScriptObjectMutex);
	if (GRawScriptObjectRegistrations == nullptr)
	{
		return false;
	}
	const auto Iterator = GRawScriptObjectRegistrations->find(ScriptObject);
	if (Iterator == GRawScriptObjectRegistrations->end()
		|| Iterator->second.ScriptType != ExpectedScriptType
		|| !Iterator->second.bDestructionInProgress
		|| Iterator->second.ReferenceCount <= 0)
	{
		return false;
	}

	Iterator->second.bDestructionInProgress = false;
	--Iterator->second.ReferenceCount;
	return Iterator->second.ReferenceCount == 0;
}

void UASClass::UnregisterRawScriptObject(const void* ScriptObject)
{
	std::scoped_lock Lock(GRawScriptObjectMutex);
	if (GRawScriptObjectRegistrations != nullptr)
	{
		GRawScriptObjectRegistrations->erase(ScriptObject);
		ReleaseEmptyRawScriptObjectRegistry();
	}
}

void UASClass::UnregisterRawScriptObjectsForEngine(
	const asIScriptEngine* ScriptEngine)
{
	std::scoped_lock Lock(GRawScriptObjectMutex);
	if (GRawScriptObjectRegistrations == nullptr)
	{
		return;
	}
	for (auto Iterator = GRawScriptObjectRegistrations->begin();
		Iterator != GRawScriptObjectRegistrations->end();)
	{
		if (Iterator->second.ScriptEngine == ScriptEngine)
		{
			Iterator = GRawScriptObjectRegistrations->erase(Iterator);
		}
		else
		{
			++Iterator;
		}
	}
	ReleaseEmptyRawScriptObjectRegistry();
}

asITypeInfo* UASClass::GetRawScriptObjectType(
	const void* ScriptObject)
{
	std::scoped_lock Lock(GRawScriptObjectMutex);
	if (GRawScriptObjectRegistrations == nullptr)
	{
		return nullptr;
	}
	const auto Iterator = GRawScriptObjectRegistrations->find(ScriptObject);
	return Iterator == GRawScriptObjectRegistrations->end()
		? nullptr
		: Iterator->second.ScriptType;
}

asITypeInfo* asIScriptObject::GetObjectType() const
{
	return UASClass::GetRawScriptObjectType(this);
}

double asStringScanDouble(const char* String)
{
	return std::strtod(String, nullptr);
}

float asStringScanFloat(const char* String)
{
	return std::strtof(String, nullptr);
}
