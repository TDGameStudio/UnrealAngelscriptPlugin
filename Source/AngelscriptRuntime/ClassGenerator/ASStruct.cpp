#include "ClassGenerator/ASStruct.h"

#include "Misc/SecureHash.h"

#include "AngelscriptEngine.h"
#include "AngelscriptInclude.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_config.h"
#include "source/as_scriptengine.h"
#include "source/as_scriptobject.h"
#include "source/as_objecttype.h"
#include "source/as_context.h"
#include "EndAngelscriptHeaders.h"

struct FASStructOps : UASStruct::ICppStructOps
{
	UASStruct* Struct;
	asCObjectType* ScriptType;

	asIScriptFunction* EqualsFunction;
	asIScriptFunction* ConstructFunction;
	asIScriptFunction* ToStrFunction;
	asIScriptFunction* HashFunction;

	struct FASFakeVTable : public UE::CoreUObject::Private::FStructOpsFakeVTable
	{
		void (*Construct)(void*);
#if !(UE_BUILD_TEST || UE_BUILD_SHIPPING)
		void (*ConstructForTests)(void*);
#endif
		void (*Destruct)(void*);
		bool (*Copy)(void*, const void*, int32);
		bool (*Identical)(const void*, const void*, uint32, bool&);
		uint32 (*GetStructTypeHash)(const void*);
	};

	FASFakeVTable FakeVTable;

	static thread_local FASStructOps* ConstructingOps;

	static TSet<const FASStructOps*>& GetRegisteredOps()
	{
		static TSet<const FASStructOps*> RegisteredOps;
		return RegisteredOps;
	}

	FASStructOps(UASStruct* InStruct, int32 InSize, int32 InAlignment)
		: UASStruct::ICppStructOps(InSize, InAlignment)
		, Struct(InStruct)
		, ScriptType((asCObjectType*)InStruct->ScriptType)
	{
		SetFromStruct(InStruct);
		FakeVPtr = &FakeVTable;

		FakeVTable.Flags =
			UE::CoreUObject::Private::EStructOpsFakeVTableFlags::Construct
#if !(UE_BUILD_TEST || UE_BUILD_SHIPPING)
			| UE::CoreUObject::Private::EStructOpsFakeVTableFlags::ConstructForTests
#endif
			| UE::CoreUObject::Private::EStructOpsFakeVTableFlags::Destruct
			| UE::CoreUObject::Private::EStructOpsFakeVTableFlags::Copy
			| UE::CoreUObject::Private::EStructOpsFakeVTableFlags::Identical
			| UE::CoreUObject::Private::EStructOpsFakeVTableFlags::GetStructTypeHash;

		FakeVTable.Construct = &FASStructOps::Construct;
#if !(UE_BUILD_TEST || UE_BUILD_SHIPPING)
		FakeVTable.ConstructForTests = &FASStructOps::Construct;
#endif
		FakeVTable.Destruct = &FASStructOps::Destruct;
		FakeVTable.Copy = &FASStructOps::Copy;
		FakeVTable.Identical = &FASStructOps::Identical;
		FakeVTable.GetStructTypeHash = &FASStructOps::GetStructTypeHash;

		FMemory::Memzero(FakeVTable.Capabilities);
		FakeVTable.Capabilities.HasDestructor = true;
		FakeVTable.Capabilities.HasCopy = true;
		FakeVTable.Capabilities.HasIdentical = (EqualsFunction != nullptr);
		FakeVTable.Capabilities.HasGetTypeHash = (HashFunction != nullptr);
		FakeVTable.Capabilities.ComputedPropertyFlags |= (HashFunction != nullptr) ? CPF_HasGetValueTypeHash : CPF_None;

		GetRegisteredOps().Add(this);
	}

	void SetFromStruct(UASStruct* InStruct)
	{
		check(InStruct == Struct);
		ScriptType = (asCObjectType*)InStruct->ScriptType;

		if (ScriptType != nullptr)
		{
			auto& Manager = FAngelscriptEngine::Get();
			if (ScriptType->beh.construct != 0)
				ConstructFunction = Manager.Engine->GetFunctionById(ScriptType->beh.construct);
			else
				ConstructFunction = nullptr;

			if (ScriptType->GetFirstMethod("opEquals") != nullptr)
			{
				FString StructName = ANSI_TO_TCHAR(ScriptType->GetName());
				FString EqualsDecl = FString::Printf(TEXT("bool opEquals(const %s& Other) const"), *StructName);
				EqualsFunction = ScriptType->GetMethodByDecl(TCHAR_TO_ANSI(*EqualsDecl));
			}
			else
			{
				EqualsFunction = nullptr;
			}

			if (ScriptType->GetFirstMethod("ToString") != nullptr)
			{
				ToStrFunction = ScriptType->GetMethodByDecl("FString ToString() const");
			}
			else
			{
				ToStrFunction = nullptr;
			}

			if (ScriptType->GetFirstMethod("Hash") != nullptr)
			{
				HashFunction = FAngelscriptType::FindScriptStructHashFunction(ScriptType);
			}
			else
			{
				HashFunction = nullptr;
			}
		}
		else
		{
			EqualsFunction = nullptr;
			ConstructFunction = nullptr;
			ToStrFunction = nullptr;
			HashFunction = nullptr;
		}
	}

	static FASStructOps* GetOpsFromValue(const void* Address)
	{
		if (Address == nullptr)
			return nullptr;

		const UASStruct::FScriptStructValueHeader* Header = UASStruct::GetValueHeader(Address);
		if (!Header->IsValid())
			return nullptr;

		FASStructOps* Ops = static_cast<FASStructOps*>(Header->CppStructOps);
		if (!GetRegisteredOps().Contains(Ops))
			return nullptr;
		if (Ops == nullptr || Ops->Struct == nullptr || Ops->ScriptType == nullptr)
			return nullptr;
		if (Ops->Struct->GetCppStructOps() != Ops)
			return nullptr;
		if (Header->ScriptType != Ops->ScriptType)
			return nullptr;

		return Ops;
	}

	static void CopyScriptPayload(void* Dest, const void* Src, asCObjectType* ScriptType)
	{
		const int32 PayloadOffset = ScriptType->basePropertyOffset;
		const int32 PayloadSize = ScriptType->size - PayloadOffset;
		if (PayloadSize > 0)
		{
			FMemory::Memcpy(
				static_cast<uint8*>(Dest) + PayloadOffset,
				static_cast<const uint8*>(Src) + PayloadOffset,
				PayloadSize);
		}
	}

	static void Construct(void* Dest)
	{
		FASStructOps* Ops = ConstructingOps;
		if (Ops == nullptr)
			Ops = GetOpsFromValue(Dest);
		check(Ops != nullptr);

		if (Ops->ScriptType == nullptr)
		{
			FMemory::Memzero(Dest, Ops->GetSize());
			return;
		}

		FMemory::Memzero(Dest, Ops->GetSize());
		UASStruct::FScriptStructValueHeader* Header = UASStruct::GetValueHeader(Dest);
		Header->Magic = UASStruct::FScriptStructValueHeader::MagicValue;
		Header->ScriptType = Ops->ScriptType;
		Header->CppStructOps = Ops;
		ScriptObject_Construct(Ops->ScriptType, (asCScriptObject*)Dest);

		if (Ops->ConstructFunction != nullptr)
		{
			FAngelscriptContext Context(Ops->ConstructFunction->GetEngine());
			if (!PrepareAngelscriptContextWithLog(Context, Ops->ConstructFunction, TEXT("FASStructOps::Construct")))
			{
				return;
			}
			Context->SetObject((asIScriptObject*)Dest);
			Context->Execute();
		}
	}

	static void Destruct(void* Dest)
	{
		FASStructOps* Ops = GetOpsFromValue(Dest);
		if (Ops == nullptr)
			return;

		if (Ops->ScriptType == nullptr)
		{
			FMemory::Memzero(Dest, Ops->GetSize());
			return;
		}

		// During process-exit UObject purge the owning AngelScript engine may already
		// have been released (FAngelscriptEngine::Shutdown -> ShutDownAndRelease), which
		// leaves asITypeInfo::engine dangling and crashes CallDestructor. The process is
		// terminating, so skipping the script destructor here is safe.
		if (FAngelscriptEngine::AreEnginesReleasedForExit())
		{
			FMemory::Memzero(Dest, Ops->GetSize());
			return;
		}

		auto* ScriptObject = (asCScriptObject*)(Dest);
		ScriptObject->CallDestructor(Ops->ScriptType);
	}

	static bool Copy(void* Dest, void const* Src, int32 ArrayDim)
	{
		// Script temporaries reserve the pre-class header space but do not always
		// contain a valid UE FScriptStructValueHeader. Prefer the initialized
		// destination header and only use the source as a fallback for raw copies.
		FASStructOps* Ops = GetOpsFromValue(Dest);
		if (Ops == nullptr)
			Ops = GetOpsFromValue(Src);
		if (Ops == nullptr)
			return false;

		if (Ops->ScriptType == nullptr)
			return true;

		for (int32 Index = 0; Index < ArrayDim; ++Index)
		{
			uint8* DestElement = static_cast<uint8*>(Dest) + Index * Ops->GetSize();
			const uint8* SrcElement = static_cast<const uint8*>(Src) + Index * Ops->GetSize();

			UASStruct::FScriptStructValueHeader* Header = UASStruct::GetValueHeader(DestElement);
			if (!Header->IsValid())
			{
				Header->Magic = UASStruct::FScriptStructValueHeader::MagicValue;
				Header->ScriptType = Ops->ScriptType;
				Header->CppStructOps = Ops;
				ScriptObject_Construct(Ops->ScriptType, (asCScriptObject*)DestElement);
			}

			auto* DestObject = reinterpret_cast<asCScriptObject*>(DestElement);
			auto* SourceObject = reinterpret_cast<asCScriptObject*>(const_cast<uint8*>(SrcElement));
			if ((Ops->ScriptType->flags & asOBJ_POD) != 0)
			{
				CopyScriptPayload(DestObject, SourceObject, Ops->ScriptType);
			}
			else
			{
				DestObject->CopyStruct(SourceObject, Ops->ScriptType);
			}
		}
		return true;
	}

	static bool Identical(const void* A, const void* B, uint32 PortFlags, bool& bOutResult)
	{
		FASStructOps* Ops = GetOpsFromValue(A);
		if (Ops == nullptr)
			return false;

		if (Ops->ScriptType == nullptr)
			return false;
		if (Ops->EqualsFunction == nullptr)
			return false;

		FAngelscriptContext Context(Ops->EqualsFunction->GetEngine());
		if (!PrepareAngelscriptContextWithLog(Context, Ops->EqualsFunction, TEXT("FASStructOps::Identical")))
		{
			bOutResult = false;
			return false;
		}
		Context->SetObject((asIScriptObject*)A);
		Context->SetArgAddress(0, (void*)B);
		Context->Execute();
		bOutResult = (Context->GetReturnByte() != 0);
		return true;
	}

	static uint32 GetStructTypeHash(const void* Src)
	{
		FASStructOps* Ops = GetOpsFromValue(Src);
		if (Ops == nullptr)
			return 0;

		if (Ops->HashFunction == nullptr)
			return 0;

		FAngelscriptContext Context(Ops->HashFunction->GetEngine());
		if (!PrepareAngelscriptContextWithLog(Context, Ops->HashFunction, TEXT("FASStructOps::GetStructTypeHash")))
		{
			return 0;
		}
		Context->SetObject(const_cast<void*>(Src));
		Context->Execute();
		return Context->GetReturnDWord();
	}
};

thread_local FASStructOps* FASStructOps::ConstructingOps = nullptr;

struct FASStructOpsScope
{
	FASStructOps* PreviousOps = nullptr;

	explicit FASStructOpsScope(const UASStruct& Struct)
	{
		PreviousOps = FASStructOps::ConstructingOps;
		FASStructOps::ConstructingOps = static_cast<FASStructOps*>(Struct.GetCppStructOps());
	}

	~FASStructOpsScope()
	{
		FASStructOps::ConstructingOps = PreviousOps;
	}
};

UASStruct::UASStruct(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

UASStruct::ICppStructOps* UASStruct::CreateCppStructOps()
{
	return new FASStructOps(this, GetPropertiesSize(), GetMinAlignment());
}

void UASStruct::InitializeStruct(void* Dest, int32 ArrayDim) const
{
	FASStructOpsScope OpsScope(*this);
	Super::InitializeStruct(Dest, ArrayDim);
}

void UASStruct::DestroyStruct(void* Dest, int32 ArrayDim) const
{
	FASStructOpsScope OpsScope(*this);
	Super::DestroyStruct(Dest, ArrayDim);
}

void UASStruct::PrepareCppStructOps()
{
	if (CppStructOps == nullptr)
		SetCppStructOps(CreateCppStructOps());
	Super::PrepareCppStructOps();
}

void UASStruct::UpdateScriptType()
{
	FASStructOps* Ops = ((FASStructOps*)GetCppStructOps());
	Ops->SetFromStruct(this);

	if (Ops->EqualsFunction != nullptr)
		StructFlags = EStructFlags(StructFlags | STRUCT_IdenticalNative);
	else
		StructFlags = EStructFlags(StructFlags & ~STRUCT_IdenticalNative);
}

asIScriptFunction* UASStruct::GetToStringFunction() const
{
	if (ICppStructOps* StructOps = GetCppStructOps())
	{
		return ((FASStructOps*)StructOps)->ToStrFunction;
	}

	return nullptr;
}

void UASStruct::SetGuid(FName FromName)
{
	FString HashString = TEXT("Script:");
	HashString += FromName.ToString();

	ensure(HashString.Len());

	const uint32 BufferLength = HashString.Len() * sizeof(HashString[0]);
	uint32 HashBuffer[5];
	FSHA1::HashBuffer(*HashString, BufferLength, reinterpret_cast<uint8*>(HashBuffer));
	Guid = FGuid(HashBuffer[1], HashBuffer[2], HashBuffer[3], HashBuffer[4]);
}
