#include "BlueprintCallableReflectiveFallback.h"

#include "Core/AngelscriptBinds.h"
#include "Core/AngelscriptEngine.h"
#include "Core/FunctionCallers.h"
#include "Binds/Helper_FunctionSignature.h"

#include "HAL/IConsoleManager.h"
#include "Misc/Optional.h"
#include "UObject/UnrealType.h"
#include "UObject/Class.h"
#include "UObject/PropertyOptional.h"
#include "UObject/Script.h"
#include "UObject/Stack.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_generic.h"
#include "source/as_scriptfunction.h"
#include "EndAngelscriptHeaders.h"

namespace
{
	// ---- Opt 1: TLS cache for IsScriptDeclarationAlreadyBound's global-function scan ----
	// Key: namespace (UTF-8). Value: set of registered script-names and a set of full declarations.
	struct FGlobalDeclCacheEntry
	{
		TSet<FString> Names;
		TSet<FString> Declarations;
	};

	struct FBindCachesTLS
	{
		// Opt 1: namespace -> (names, decls)
		TMap<FString, FGlobalDeclCacheEntry> GlobalDecls;
		asUINT LastSyncedGlobalFunctionCount = 0;
		bool bGlobalDeclsActive = false;

		// Opt 3: per-class function-name cache (includes super chain for parity with FindFunctionByName).
		TMap<UClass*, TSet<FName>> ClassFuncNames;
		bool bClassFuncNamesActive = false;
	};

	thread_local FBindCachesTLS* GBindCachesTLS = nullptr;

	// Bring the TLS global-decl cache up to date with the AS engine's current global function list.
	// We only ever add — Phase 2 monotonically grows the global function set.
	void SyncGlobalDeclCacheFromEngine(asIScriptEngine* ScriptEngine)
	{
		if (GBindCachesTLS == nullptr || !GBindCachesTLS->bGlobalDeclsActive || ScriptEngine == nullptr)
		{
			return;
		}

		const asUINT Count = ScriptEngine->GetGlobalFunctionCount();
		if (Count == GBindCachesTLS->LastSyncedGlobalFunctionCount)
		{
			return;
		}

		for (asUINT Index = GBindCachesTLS->LastSyncedGlobalFunctionCount; Index < Count; ++Index)
		{
			asIScriptFunction* Func = ScriptEngine->GetGlobalFunctionByIndex(Index);
			if (Func == nullptr)
			{
				continue;
			}
			const char* NsRaw = Func->GetNamespace();
			const char* NameRaw = Func->GetName();
			const char* DeclRaw = Func->GetDeclaration(false, true, false, true);

			const FString NsStr = NsRaw != nullptr ? FString(UTF8_TO_TCHAR(NsRaw)) : FString();
			FGlobalDeclCacheEntry& Entry = GBindCachesTLS->GlobalDecls.FindOrAdd(NsStr);
			if (NameRaw != nullptr)
			{
				Entry.Names.Add(FString(UTF8_TO_TCHAR(NameRaw)));
			}
			if (DeclRaw != nullptr)
			{
				Entry.Declarations.Add(FString(UTF8_TO_TCHAR(DeclRaw)));
			}
		}

		GBindCachesTLS->LastSyncedGlobalFunctionCount = Count;
	}
}

namespace
{
	constexpr int32 BlueprintCallableReflectiveFallbackMaxArgs = 16;
#if WITH_EDITORONLY_DATA
	const FName NAME_BlueprintCallableReflectiveFallback_CustomThunk(TEXT("CustomThunk"));
#endif

	// =====================================================================
	// CVar: as.ReflectiveFallback.UseCache (default 1)
	//
	// Toggles between two reflective fallback dispatch strategies:
	//   1 (default) - cached path: precomputed FReflectiveParamCache +
	//                 FFrame + UFunction::Invoke (with FUNC_Net branch).
	//                 ~3-6x faster than ProcessEvent for typical signatures.
	//                 See InvokeReflectiveUFunctionFromGenericCallCached.
	//   0           - legacy path: per-call TFieldIterator walk + Init/Copy +
	//                 UObject::ProcessEvent + per-call DestroyValue. Matches
	//                 the pre-cache behaviour. Use to roll back instantly if
	//                 the cached path misbehaves on a specific UFunction or
	//                 to A/B compare correctness for a regression hunt.
	//                 See InvokeReflectiveUFunctionFromGenericCallProcessEvent.
	//
	// Read per-call via GetValueOnAnyThread() so toggling at runtime takes
	// effect immediately without engine restart or module reload.
	// =====================================================================
	TAutoConsoleVariable<bool> CVarReflectiveFallbackUseCache(
		TEXT("as.ReflectiveFallback.UseCache"),
		true,
		TEXT("Whether reflective fallback for BlueprintCallable UFunction calls uses ")
		TEXT("the precomputed FReflectiveParamCache + UFunction::Invoke fast path. ")
		TEXT("Set to 0 to revert to legacy per-call TFieldIterator + UObject::ProcessEvent ")
		TEXT("dispatch (slower; useful for A/B comparison and incident-response rollback). ")
		TEXT("Default: 1 (cached)."),
		ECVF_Default);

	// =====================================================================
	// FReflectiveParamCache
	//
	// Per-UFunction precomputed dispatch metadata. Built lazily on the first
	// reflective fallback call (see FBlueprintCallableReflectiveSignature::
	// GetOrBuildCache) and reused on every subsequent call. The intent mirrors
	// sluaunreal's `LuaFunctionAccelerator`: replace the per-call
	// TFieldIterator + virtual FProperty dispatch with a tight TArray loop
	// that knows offsets / sizes / copy strategy upfront.
	//
	// This is a runtime-only structure (allocated in TOptional inside the
	// signature); it is freed automatically when the signature is deleted.
	// =====================================================================
	struct FReflectiveParamCache
	{
		struct FParamEntry
		{
			FProperty* Property = nullptr;
			int32 UEOffset = 0;       // FProperty::GetOffset_ForInternal()
			int32 Size = 0;           // FProperty::GetSize()
			bool bIsSimpleCopy = false;
			bool bNeedInitialize = false;
			bool bNeedDestroy = false;
			// CPF_OutParm is set: the engine's FFrame::StepExplicitProperty
			// will look this property up in OutParms and crash if it is
			// missing. const-ref UFUNCTION args (CPF_OutParm | CPF_ConstParm)
			// fall under this category as well — they must be in the chain
			// even though we never write back to script.
			bool bRequiresOutParmRec = false;
			// Subset of bRequiresOutParmRec that we should also copy back to
			// the AS-side argument address after Invoke (true non-const out
			// parameters with no CPF_ReturnParm).
			bool bIsWritebackOut = false;
			bool bIsReferenceParam = false;
		};

		// In AS argument order, with the return parameter excluded.
		TArray<FParamEntry, TInlineAllocator<8>> Params;
		// Indices into Params with CPF_OutParm set (regardless of CPF_ConstParm).
		// Used to build the FOutParmRec linked list that engine exec stubs expect.
		TArray<int32, TInlineAllocator<4>> OutParamIndices;
		FParamEntry Return;
		int32 ParmsSize = 0;
		int32 ReturnValueOffset = MAX_uint16;
		bool bHasReturn = false;
		bool bIsNetFunction = false;
	};

	// Determine whether a property's bytes can be moved with a raw memcpy
	// (instead of going through CPP-level copy constructors via
	// FProperty::CopySingleValue). We use a deny-list because most engine
	// containers and string-like types depend on FProperty's virtual copy.
	bool IsPropertySimpleCopy(const FProperty* Property)
	{
		if (Property == nullptr)
		{
			return false;
		}

		if (Property->HasAnyPropertyFlags(CPF_IsPlainOldData))
		{
			return true;
		}

		// FObjectProperty / FInterfaceProperty / FObjectPtrProperty etc. all derive from
		// FObjectPropertyBase and store an 8-byte pointer that is bytewise copyable.
		if (CastField<FObjectPropertyBase>(Property) != nullptr)
		{
			return true;
		}

		// Containers, delegates, FString / FName / FText, FSoft* etc. require
		// per-element ref-counting / interning and cannot be memcpy'd safely.
		if (CastField<FStrProperty>(Property) != nullptr
			|| CastField<FNameProperty>(Property) != nullptr
			|| CastField<FTextProperty>(Property) != nullptr
			|| CastField<FArrayProperty>(Property) != nullptr
			|| CastField<FMapProperty>(Property) != nullptr
			|| CastField<FSetProperty>(Property) != nullptr
			|| CastField<FDelegateProperty>(Property) != nullptr
			|| CastField<FMulticastDelegateProperty>(Property) != nullptr
			|| CastField<FSoftObjectProperty>(Property) != nullptr
			|| CastField<FSoftClassProperty>(Property) != nullptr
			|| CastField<FFieldPathProperty>(Property) != nullptr
			|| CastField<FOptionalProperty>(Property) != nullptr)
		{
			return false;
		}

		// USTRUCT: only allow memcpy if the struct itself has no destructor and
		// no copy semantics that need running.
		if (const FStructProperty* StructProp = CastField<FStructProperty>(Property))
		{
			if (StructProp->Struct != nullptr)
			{
				const EStructFlags Flags = StructProp->Struct->StructFlags;
				const bool bHasDtor = (Flags & STRUCT_NoDestructor) == 0;
				const bool bHasCustomCopy = (Flags & STRUCT_CopyNative) != 0;
				if (bHasDtor || bHasCustomCopy)
				{
					return false;
				}
				return true;
			}
			return false;
		}

		// Conservatively fall back to virtual CopySingleValue for anything we don't recognise.
		return false;
	}

	struct FBlueprintCallableReflectiveSignature
	{
		FAngelscriptTypeUsage ReturnType;
		FAngelscriptTypeUsage Arguments[BlueprintCallableReflectiveFallbackMaxArgs];
		int32 ArgCount = 0;
		UFunction* UnrealFunction = nullptr;
		UObject* StaticObject = nullptr;
		bool bInjectMixinObject = false;
		bool bInitReturn = false;
		bool bZeroReturnPtr = false;

		// Lazily-constructed parameter cache. Populated on the first call to
		// GetOrBuildCache(); subsequent calls return the same cache by reference.
		// Storing inside TOptional keeps the cache colocated with the signature
		// so it dies automatically when the signature is deleted.
		TOptional<FReflectiveParamCache> CachedParams;

		const FReflectiveParamCache& GetOrBuildCache();
	};

	const FReflectiveParamCache& FBlueprintCallableReflectiveSignature::GetOrBuildCache()
	{
		if (CachedParams.IsSet())
		{
			return CachedParams.GetValue();
		}

		FReflectiveParamCache& Cache = CachedParams.Emplace();
		// Allocate space for parameters AND locals. For native UFUNCTIONs the two
		// are equal (no Blueprint locals); for Blueprint-defined functions reached
		// via reflective fallback the extra space is needed by ProcessInternal.
		Cache.ParmsSize = UnrealFunction != nullptr ? UnrealFunction->PropertiesSize : 0;
		Cache.ReturnValueOffset = UnrealFunction != nullptr ? UnrealFunction->ReturnValueOffset : MAX_uint16;
		Cache.bIsNetFunction = UnrealFunction != nullptr
			&& (UnrealFunction->FunctionFlags & FUNC_Net) != 0;

		if (UnrealFunction == nullptr)
		{
			return Cache;
		}

		Cache.Params.Reserve(BlueprintCallableReflectiveFallbackMaxArgs);

		for (TFieldIterator<FProperty> It(UnrealFunction); It && (It->PropertyFlags & CPF_Parm); ++It)
		{
			FProperty* Prop = *It;

			FReflectiveParamCache::FParamEntry Entry;
			Entry.Property = Prop;
			Entry.UEOffset = Prop->GetOffset_ForInternal();
			Entry.Size = Prop->GetSize();
			Entry.bNeedInitialize = !Prop->HasAnyPropertyFlags(CPF_ZeroConstructor);
			Entry.bNeedDestroy = !Prop->HasAnyPropertyFlags(CPF_NoDestructor)
				&& !Prop->HasAnyPropertyFlags(CPF_IsPlainOldData);
			Entry.bIsSimpleCopy = IsPropertySimpleCopy(Prop);
			Entry.bIsReferenceParam = Prop->HasAnyPropertyFlags(CPF_ReferenceParm)
				|| (Prop->HasAnyPropertyFlags(CPF_OutParm) && !Prop->HasAnyPropertyFlags(CPF_ReturnParm));
			// Engine FFrame contract: every CPF_OutParm parameter (including
			// const-ref) must appear in OutParms or StepExplicitProperty
			// dereferences a null record.
			Entry.bRequiresOutParmRec = Prop->HasAnyPropertyFlags(CPF_OutParm)
				&& !Prop->HasAnyPropertyFlags(CPF_ReturnParm);
			// Only true non-const out-parameters need to be written back to AS
			// after Invoke. Const-ref args are read-only on the engine side.
			Entry.bIsWritebackOut = Entry.bRequiresOutParmRec
				&& !Prop->HasAnyPropertyFlags(CPF_ConstParm);

			if (Prop->HasAnyPropertyFlags(CPF_ReturnParm))
			{
				Cache.Return = Entry;
				Cache.bHasReturn = true;
			}
			else
			{
				const int32 ParamIndex = Cache.Params.Add(Entry);
				if (Entry.bRequiresOutParmRec)
				{
					Cache.OutParamIndices.Add(ParamIndex);
				}
			}
		}

		return Cache;
	}

	int32 GetNonReturnParameterCount(const UFunction* Function)
	{
		int32 ParameterCount = 0;
		for (TFieldIterator<FProperty> It(Function); It && (It->PropertyFlags & CPF_Parm); ++It)
		{
			if (!It->HasAnyPropertyFlags(CPF_ReturnParm))
			{
				++ParameterCount;
			}
		}
		return ParameterCount;
	}

	void* ResolveScriptArgumentAddress(const FProperty* Property, void* ScriptArgumentAddress)
	{
		if (Property != nullptr
			&& (Property->HasAnyPropertyFlags(CPF_ReferenceParm)
				|| (Property->HasAnyPropertyFlags(CPF_OutParm) && !Property->HasAnyPropertyFlags(CPF_ReturnParm))))
		{
			return ScriptArgumentAddress != nullptr ? *(void**)ScriptArgumentAddress : nullptr;
		}

		return ScriptArgumentAddress;
	}

	// Cached counterpart to InvokeReflectiveUFunctionFromGenericCall:
	//   - reads everything from the prebuilt FReflectiveParamCache
	//   - skips ProcessEvent in favour of FFrame + UFunction::Invoke
	//   - mirrors sluaunreal's FOutParmRec linked-list construction so engines
	//     that expect FFrame::OutParms still see them
	//   - special-cases FUNC_Net via GetFunctionCallspace + CallRemoteFunction
	bool InvokeReflectiveUFunctionFromGenericCallCached(
		asCGeneric* Generic,
		UObject* TargetObject,
		UFunction* Function,
		const FReflectiveParamCache& Cache,
		bool bInjectMixinObject)
	{
		if (Generic == nullptr || TargetObject == nullptr || Function == nullptr)
		{
			return false;
		}

		uint8* ParameterBuffer = static_cast<uint8*>(FMemory_Alloca(Cache.ParmsSize));
		FMemory::Memzero(ParameterBuffer, Cache.ParmsSize);

		// Initialize parameters that need non-zero default construction.
		for (const FReflectiveParamCache::FParamEntry& Entry : Cache.Params)
		{
			if (Entry.bNeedInitialize)
			{
				Entry.Property->InitializeValue_InContainer(ParameterBuffer);
			}
		}
		if (Cache.bHasReturn && Cache.Return.bNeedInitialize)
		{
			Cache.Return.Property->InitializeValue_InContainer(ParameterBuffer);
		}

		// Track the script-side addresses of out parameters so we can write back after Invoke.
		void* OutScriptAddresses[BlueprintCallableReflectiveFallbackMaxArgs];
		FMemory::Memzero(OutScriptAddresses, sizeof(OutScriptAddresses));

		int32 ScriptArgIndex = 0;
		bool bInjectedMixinObject = false;

		const int32 ParamCount = Cache.Params.Num();
		for (int32 ParamIndex = 0; ParamIndex < ParamCount; ++ParamIndex)
		{
			const FReflectiveParamCache::FParamEntry& Entry = Cache.Params[ParamIndex];

			void* Destination = ParameterBuffer + Entry.UEOffset;

			if (bInjectMixinObject && !bInjectedMixinObject)
			{
				UObject* MixinObject = static_cast<UObject*>(Generic->GetObject());
				Entry.Property->CopySingleValue(Destination, &MixinObject);
				bInjectedMixinObject = true;
				continue;
			}

			if (ScriptArgIndex >= Generic->GetArgCount())
			{
				// Argument under-flow: clean up and bail out.
				for (const FReflectiveParamCache::FParamEntry& E : Cache.Params)
				{
					if (E.bNeedDestroy)
					{
						E.Property->DestroyValue_InContainer(ParameterBuffer);
					}
				}
				if (Cache.bHasReturn && Cache.Return.bNeedDestroy)
				{
					Cache.Return.Property->DestroyValue_InContainer(ParameterBuffer);
				}
				return false;
			}

			void* ScriptArgumentAddress = Generic->GetAddressOfArg(ScriptArgIndex);
			void* SourceAddress = ResolveScriptArgumentAddress(Entry.Property, ScriptArgumentAddress);

			if (Entry.bIsSimpleCopy && SourceAddress != nullptr)
			{
				FMemory::Memcpy(Destination, SourceAddress, Entry.Size);
			}
			else if (SourceAddress != nullptr)
			{
				Entry.Property->CopySingleValue(Destination, SourceAddress);
			}

			if (Entry.bIsWritebackOut)
			{
				OutScriptAddresses[ParamIndex] = SourceAddress;
			}

			++ScriptArgIndex;
		}

		// Build the FOutParmRec chain that the engine expects when out parameters are present.
		// Mirrors sluaunreal's LuaFunctionAccelerator::call layout.
		FFrame NewStack(TargetObject, Function, ParameterBuffer, /*PreviousFrame=*/ nullptr,
			Function->ChildProperties);

		FOutParmRec** LastOut = &NewStack.OutParms;
		for (int32 OutIdx : Cache.OutParamIndices)
		{
			const FReflectiveParamCache::FParamEntry& Entry = Cache.Params[OutIdx];
			FOutParmRec* OutRec = static_cast<FOutParmRec*>(FMemory_Alloca(sizeof(FOutParmRec)));
			OutRec->Property = Entry.Property;
			OutRec->PropAddr = ParameterBuffer + Entry.UEOffset;
			OutRec->NextOutParm = nullptr;
			if (*LastOut != nullptr)
			{
				(*LastOut)->NextOutParm = OutRec;
				LastOut = &(*LastOut)->NextOutParm;
			}
			else
			{
				*LastOut = OutRec;
			}
		}

		uint8* ReturnAddress = Cache.bHasReturn
			? (ParameterBuffer + Cache.ReturnValueOffset)
			: nullptr;

		if (Cache.bIsNetFunction)
		{
			// Replicate sluaunreal's FUNC_Net handling so RPCs remain functionally correct.
			const int32 FunctionCallspace = TargetObject->GetFunctionCallspace(Function, &NewStack);
			uint8* SavedCode = nullptr;
			if ((FunctionCallspace & FunctionCallspace::Remote) != 0)
			{
				SavedCode = NewStack.Code;
				TargetObject->CallRemoteFunction(Function, ParameterBuffer, NewStack.OutParms, &NewStack);
			}
			if ((FunctionCallspace & FunctionCallspace::Local) != 0)
			{
				if (SavedCode != nullptr)
				{
					NewStack.Code = SavedCode;
				}
				Function->Invoke(TargetObject, NewStack, ReturnAddress);
			}
		}
		else
		{
			Function->Invoke(TargetObject, NewStack, ReturnAddress);
		}

		// Out-parameter writeback (script-side addresses captured above).
		// Only non-const out parameters need their data copied back to AS.
		for (int32 OutIdx : Cache.OutParamIndices)
		{
			const FReflectiveParamCache::FParamEntry& Entry = Cache.Params[OutIdx];
			if (!Entry.bIsWritebackOut)
			{
				continue;
			}
			void* ScriptAddress = OutScriptAddresses[OutIdx];
			if (ScriptAddress == nullptr)
			{
				continue;
			}
			void* SourceAddress = ParameterBuffer + Entry.UEOffset;
			if (Entry.bIsSimpleCopy)
			{
				FMemory::Memcpy(ScriptAddress, SourceAddress, Entry.Size);
			}
			else
			{
				Entry.Property->CopySingleValue(ScriptAddress, SourceAddress);
			}
		}

		// Return-value writeback into the AS-controlled return location.
		if (Cache.bHasReturn)
		{
			void* ReturnDestination = Generic->GetAddressOfReturnLocation();
			if (ReturnDestination != nullptr)
			{
				if (Cache.Return.bIsSimpleCopy)
				{
					FMemory::Memcpy(ReturnDestination, ParameterBuffer + Cache.ReturnValueOffset, Cache.Return.Size);
				}
				else
				{
					Cache.Return.Property->InitializeValue(ReturnDestination);
					Cache.Return.Property->CopySingleValue(
						ReturnDestination,
						ParameterBuffer + Cache.ReturnValueOffset);
				}
			}
		}

		// Tear down anything the parameter buffer initialised.
		for (const FReflectiveParamCache::FParamEntry& Entry : Cache.Params)
		{
			if (Entry.bNeedDestroy)
			{
				Entry.Property->DestroyValue_InContainer(ParameterBuffer);
			}
		}
		if (Cache.bHasReturn && Cache.Return.bNeedDestroy)
		{
			Cache.Return.Property->DestroyValue_InContainer(ParameterBuffer);
		}

		return true;
	}

	// Legacy reflective fallback dispatch via UObject::ProcessEvent. Mirrors
	// the pre-cache behaviour exactly: every call walks Function->ChildProperties
	// with TFieldIterator, initialises the parameter buffer in place, copies
	// AS arguments into UFunction parameter slots, dispatches via ProcessEvent
	// (which handles the FFrame setup, FOutParmRec chain, FUNC_Net routing and
	// PreScriptCall hooks internally), then writes back out parameters and the
	// return value to the AS side. Selected when as.ReflectiveFallback.UseCache=0.
	bool InvokeReflectiveUFunctionFromGenericCallProcessEvent(
		asCGeneric* Generic,
		UObject* TargetObject,
		UFunction* Function,
		bool bInjectMixinObject)
	{
		if (Generic == nullptr || TargetObject == nullptr || Function == nullptr)
		{
			return false;
		}

		const int32 BufferSize = static_cast<int32>(Function->ParmsSize);
		uint8* ParameterBuffer = static_cast<uint8*>(FMemory_Alloca(FMath::Max(BufferSize, 1)));
		FMemory::Memzero(ParameterBuffer, FMath::Max(BufferSize, 1));

		// Initialize all parameter slots so non-trivial properties (FString,
		// USTRUCT, TArray etc.) start in a constructed state before CopySingleValue.
		for (TFieldIterator<FProperty> It(Function); It && (It->PropertyFlags & CPF_Parm); ++It)
		{
			It->InitializeValue_InContainer(ParameterBuffer);
		}

		// Capture script-side addresses for non-const out parameters so we can
		// write the post-dispatch values back into AS storage. Indexed by
		// parameter ordinal in the UFunction's child property list.
		void* OutScriptAddresses[BlueprintCallableReflectiveFallbackMaxArgs];
		FMemory::Memzero(OutScriptAddresses, sizeof(OutScriptAddresses));

		int32 ParamIndex = 0;
		int32 ScriptArgIndex = 0;
		bool bInjectedMixinObject = false;
		FProperty* ReturnProperty = nullptr;

		for (TFieldIterator<FProperty> It(Function); It && (It->PropertyFlags & CPF_Parm); ++It)
		{
			FProperty* Property = *It;

			if (Property->HasAnyPropertyFlags(CPF_ReturnParm))
			{
				ReturnProperty = Property;
				++ParamIndex;
				continue;
			}

			void* Destination = Property->ContainerPtrToValuePtr<void>(ParameterBuffer);

			if (bInjectMixinObject && !bInjectedMixinObject)
			{
				UObject* MixinObject = static_cast<UObject*>(Generic->GetObject());
				Property->CopySingleValue(Destination, &MixinObject);
				bInjectedMixinObject = true;
				++ParamIndex;
				continue;
			}

			if (ScriptArgIndex >= Generic->GetArgCount())
			{
				// Argument under-flow: clean up and bail out before ProcessEvent.
				for (TFieldIterator<FProperty> Cleanup(Function); Cleanup && (Cleanup->PropertyFlags & CPF_Parm); ++Cleanup)
				{
					Cleanup->DestroyValue_InContainer(ParameterBuffer);
				}
				return false;
			}

			void* ScriptArgumentAddress = Generic->GetAddressOfArg(ScriptArgIndex);
			void* SourceAddress = ResolveScriptArgumentAddress(Property, ScriptArgumentAddress);
			if (SourceAddress != nullptr)
			{
				Property->CopySingleValue(Destination, SourceAddress);
			}

			const bool bIsWritebackOut =
				Property->HasAnyPropertyFlags(CPF_OutParm)
				&& !Property->HasAnyPropertyFlags(CPF_ConstParm)
				&& !Property->HasAnyPropertyFlags(CPF_ReturnParm);
			if (bIsWritebackOut && ParamIndex < BlueprintCallableReflectiveFallbackMaxArgs)
			{
				OutScriptAddresses[ParamIndex] = SourceAddress;
			}

			++ScriptArgIndex;
			++ParamIndex;
		}

		// ProcessEvent handles FFrame, FOutParmRec chain, native vs Blueprint
		// dispatch, FUNC_Net RPC routing, and PreScriptCall hooks all in one.
		TargetObject->ProcessEvent(Function, ParameterBuffer);

		// Out-parameter writeback to script-side storage (non-const refs only).
		ParamIndex = 0;
		for (TFieldIterator<FProperty> It(Function); It && (It->PropertyFlags & CPF_Parm); ++It)
		{
			FProperty* Property = *It;
			if (Property->HasAnyPropertyFlags(CPF_ReturnParm))
			{
				++ParamIndex;
				continue;
			}
			if (ParamIndex < BlueprintCallableReflectiveFallbackMaxArgs)
			{
				void* ScriptAddress = OutScriptAddresses[ParamIndex];
				if (ScriptAddress != nullptr)
				{
					void* SourceAddress = Property->ContainerPtrToValuePtr<void>(ParameterBuffer);
					Property->CopySingleValue(ScriptAddress, SourceAddress);
				}
			}
			++ParamIndex;
		}

		// Return-value writeback to AS-controlled location.
		if (ReturnProperty != nullptr)
		{
			void* ReturnDestination = Generic->GetAddressOfReturnLocation();
			if (ReturnDestination != nullptr)
			{
				void* SourceAddress = ReturnProperty->ContainerPtrToValuePtr<void>(ParameterBuffer);
				ReturnProperty->InitializeValue(ReturnDestination);
				ReturnProperty->CopySingleValue(ReturnDestination, SourceAddress);
			}
		}

		// Tear down everything the buffer initialised.
		for (TFieldIterator<FProperty> It(Function); It && (It->PropertyFlags & CPF_Parm); ++It)
		{
			It->DestroyValue_InContainer(ParameterBuffer);
		}

		return true;
	}

	void CallBlueprintCallableReflectiveFallback(asIScriptGeneric* InGeneric)
	{
		auto* Generic = static_cast<asCGeneric*>(InGeneric);
		auto* Function = static_cast<asCScriptFunction*>(Generic->GetFunction());
		auto* Signature = static_cast<FBlueprintCallableReflectiveSignature*>(Function->GetUserData());

		if (Signature == nullptr || Signature->UnrealFunction == nullptr)
		{
			FAngelscriptEngine::Throw("Attempted reflective BlueprintCallable dispatch without a bound UFunction.");
			return;
		}

		UObject* TargetObject = Signature->StaticObject != nullptr ? Signature->StaticObject : static_cast<UObject*>(Generic->GetObject());
		if (TargetObject == nullptr)
		{
			FAngelscriptEngine::Throw("Attempted reflective BlueprintCallable dispatch without a target object.");
			return;
		}

		// Per-call CVar read so runtime toggles take effect immediately.
		// When disabled, skip cache build entirely - the legacy path is
		// fully self-contained and walks Function->ChildProperties on demand.
		if (CVarReflectiveFallbackUseCache.GetValueOnAnyThread())
		{
			const FReflectiveParamCache& Cache = Signature->GetOrBuildCache();
			InvokeReflectiveUFunctionFromGenericCallCached(
				static_cast<asCGeneric*>(InGeneric),
				TargetObject,
				Signature->UnrealFunction,
				Cache,
				Signature->bInjectMixinObject);
		}
		else
		{
			InvokeReflectiveUFunctionFromGenericCallProcessEvent(
				static_cast<asCGeneric*>(InGeneric),
				TargetObject,
				Signature->UnrealFunction,
				Signature->bInjectMixinObject);
		}
	}

	// Bridge for the public-API InvokeReflectiveUFunctionFromGenericCall: callers
	// outside the anonymous namespace cannot construct a FBlueprintCallableReflectiveSignature
	// directly, so this thin helper builds a transient signature, populates its
	// cache, and routes through the cached dispatch path.
	bool InvokeReflectiveUFunctionFromGenericCall_Bridge(
		asIScriptGeneric* InGeneric,
		UObject* TargetObject,
		UFunction* Function,
		bool bInjectMixinObject)
	{
		auto* Generic = static_cast<asCGeneric*>(InGeneric);
		if (Generic == nullptr || TargetObject == nullptr || Function == nullptr)
		{
			return false;
		}

		// Honour the same CVar as the primary entry point. Note: the bridge
		// is a one-shot path (transient signature, throwaway cache) used by
		// the public legacy API, so even the "cached" branch here pays the
		// cache-build cost on every call. The branch still preserves
		// dispatch-strategy parity (Invoke vs ProcessEvent) for A/B tests.
		if (!CVarReflectiveFallbackUseCache.GetValueOnAnyThread())
		{
			return InvokeReflectiveUFunctionFromGenericCallProcessEvent(
				Generic, TargetObject, Function, bInjectMixinObject);
		}

		FBlueprintCallableReflectiveSignature TransientSignature;
		TransientSignature.UnrealFunction = Function;
		TransientSignature.bInjectMixinObject = bInjectMixinObject;
		const FReflectiveParamCache& Cache = TransientSignature.GetOrBuildCache();

		return InvokeReflectiveUFunctionFromGenericCallCached(
			Generic, TargetObject, Function, Cache, bInjectMixinObject);
	}

	bool BindReflectiveFunction(
		TSharedRef<FAngelscriptType> InType,
		FAngelscriptFunctionSignature& Signature,
		FBlueprintCallableReflectiveSignature* ReflectiveSignature)
	{
		if (Signature.bStaticInScript)
		{
			ReflectiveSignature->StaticObject = InType->GetClass(FAngelscriptTypeUsage::DefaultUsage)->GetDefaultObject();
			if (ReflectiveSignature->StaticObject == nullptr)
			{
				return false;
			}

			if (Signature.bGlobalScope)
			{
				const int32 GlobalFunctionId = FAngelscriptBinds::BindGlobalFunctionDirect(
					Signature.Declaration,
					asFUNCTION(CallBlueprintCallableReflectiveFallback),
					asCALL_GENERIC,
					ASAutoCaller::FunctionCaller::Make(),
					ReflectiveSignature);
				Signature.ModifyScriptFunction(GlobalFunctionId);
			}

			FAngelscriptBinds::FNamespace Namespace(Signature.ClassName);
			const int32 NamespacedFunctionId = FAngelscriptBinds::BindGlobalFunctionDirect(
				Signature.Declaration,
				asFUNCTION(CallBlueprintCallableReflectiveFallback),
				asCALL_GENERIC,
				ASAutoCaller::FunctionCaller::Make(),
				ReflectiveSignature);
			Signature.ModifyScriptFunction(NamespacedFunctionId);
			return true;
		}

		if (Signature.bStaticInUnreal)
		{
			ReflectiveSignature->StaticObject = InType->GetClass(FAngelscriptTypeUsage::DefaultUsage)->GetDefaultObject();
			if (ReflectiveSignature->StaticObject == nullptr)
			{
				return false;
			}

			ReflectiveSignature->bInjectMixinObject = true;
			const int32 FunctionId = FAngelscriptBinds::BindMethodDirect(
				Signature.ClassName,
				Signature.Declaration,
				asFUNCTION(CallBlueprintCallableReflectiveFallback),
				asCALL_GENERIC,
				ASAutoCaller::FunctionCaller::Make(),
				ReflectiveSignature);
			Signature.ModifyScriptFunction(FunctionId);
			return true;
		}

		const int32 FunctionId = FAngelscriptBinds::BindMethodDirect(
			InType->GetAngelscriptTypeName(),
			Signature.Declaration,
			asFUNCTION(CallBlueprintCallableReflectiveFallback),
			asCALL_GENERIC,
			ASAutoCaller::FunctionCaller::Make(),
			ReflectiveSignature);
		Signature.ModifyScriptFunction(FunctionId);
		return true;
	}

	bool IsScriptDeclarationAlreadyBoundImpl(TSharedRef<FAngelscriptType> InType, const FAngelscriptFunctionSignature& Signature)
	{
		auto* ScriptEngine = FAngelscriptEngine::Get().GetScriptEngine();
		if (ScriptEngine == nullptr)
		{
			return false;
		}

		auto HasGlobalDeclaration = [&](const FString& Namespace) -> bool
		{
			// Opt 1 fast path: consult TLS cache populated during Phase 2.
			if (GBindCachesTLS != nullptr && GBindCachesTLS->bGlobalDeclsActive)
			{
				SyncGlobalDeclCacheFromEngine(ScriptEngine);
				if (const FGlobalDeclCacheEntry* Entry = GBindCachesTLS->GlobalDecls.Find(Namespace))
				{
					if (Entry->Names.Contains(Signature.ScriptName))
					{
						return true;
					}
					if (Entry->Declarations.Contains(Signature.Declaration))
					{
						return true;
					}
				}
				return false;
			}

			const FTCHARToUTF8 Utf8Declaration(*Signature.Declaration);
			const FTCHARToUTF8 Utf8ScriptName(*Signature.ScriptName);
			const FTCHARToUTF8 Utf8Namespace(*Namespace);
			const char* PreviousNamespace = ScriptEngine->GetDefaultNamespace();
			ScriptEngine->SetDefaultNamespace(Utf8Namespace.Get());
			asIScriptFunction* ExistingFunction = nullptr;
			for (asUINT FunctionIndex = 0, FunctionCount = ScriptEngine->GetGlobalFunctionCount(); FunctionIndex < FunctionCount; ++FunctionIndex)
			{
				asIScriptFunction* CandidateFunction = ScriptEngine->GetGlobalFunctionByIndex(FunctionIndex);
				if (CandidateFunction == nullptr)
				{
					continue;
				}

				const char* CandidateNamespace = CandidateFunction->GetNamespace();
				const bool bNamespaceMatches = Namespace.IsEmpty()
					? CandidateNamespace == nullptr || CandidateNamespace[0] == '\0'
					: FCStringAnsi::Strcmp(CandidateNamespace != nullptr ? CandidateNamespace : "", Utf8Namespace.Get()) == 0;
				if (!bNamespaceMatches)
				{
					continue;
				}

				if (FCStringAnsi::Strcmp(CandidateFunction->GetName(), Utf8ScriptName.Get()) == 0)
				{
					ExistingFunction = CandidateFunction;
					break;
				}

				if (FCStringAnsi::Strcmp(CandidateFunction->GetDeclaration(false, true, false, true), Utf8Declaration.Get()) == 0)
				{
					ExistingFunction = CandidateFunction;
					break;
				}
			}
			ScriptEngine->SetDefaultNamespace(PreviousNamespace != nullptr ? PreviousNamespace : "");
			return ExistingFunction != nullptr;
		};

		if (Signature.bStaticInScript)
		{
			if (HasGlobalDeclaration(Signature.ClassName))
			{
				return true;
			}

			if (HasGlobalDeclaration(FString()))
			{
				return true;
			}

			return false;
		}

		const FString& ScriptTypeName = Signature.bStaticInUnreal ? Signature.ClassName : InType->GetAngelscriptTypeName();
		const FTCHARToUTF8 Utf8TypeName(*ScriptTypeName);
		const FTCHARToUTF8 Utf8ScriptName(*Signature.ScriptName);
		const FTCHARToUTF8 Utf8Declaration(*Signature.Declaration);
		asITypeInfo* TypeInfo = ScriptEngine->GetTypeInfoByName(Utf8TypeName.Get());
		if (TypeInfo == nullptr)
		{
			return false;
		}

		if (TypeInfo->GetMethodByName(Utf8ScriptName.Get()) != nullptr)
		{
			return true;
		}

		return TypeInfo->GetMethodByDecl(Utf8Declaration.Get()) != nullptr;
	}
}

bool IsScriptDeclarationAlreadyBound(TSharedRef<FAngelscriptType> InType, const FAngelscriptFunctionSignature& Signature)
{
	return IsScriptDeclarationAlreadyBoundImpl(InType, Signature);
}

EReflectionFallbackResult EvaluateReflectionFallback(const UFunction* Function)
{
	if (Function == nullptr)
	{
		return EReflectionFallbackResult::NullFunction;
	}

	const UClass* OwningClass = Function->GetOuterUClass();
	if (OwningClass == nullptr)
	{
		return EReflectionFallbackResult::MissingOwningClass;
	}

	if (OwningClass->HasAnyClassFlags(CLASS_Interface))
	{
		return EReflectionFallbackResult::InterfaceClass;
	}

#if WITH_EDITORONLY_DATA
	// CustomThunk is UHT-only metadata, stripped from cooked builds. The generated
	// function table already excludes CustomThunk functions at cook time, so this
	// runtime gate is only needed in editor.
	if (Function->HasMetaData(NAME_BlueprintCallableReflectiveFallback_CustomThunk))
	{
		return EReflectionFallbackResult::CustomThunk;
	}
#endif

	if (GetNonReturnParameterCount(Function) > BlueprintCallableReflectiveFallbackMaxArgs)
	{
		return EReflectionFallbackResult::TooManyArguments;
	}

	return EReflectionFallbackResult::Success;
}

bool ShouldBindBlueprintCallableReflectionFallback(const UFunction* Function)
{
	return EvaluateReflectionFallback(Function) == EReflectionFallbackResult::Success;
}

bool InvokeReflectionFallbackFromGenericCall(
	asIScriptGeneric* InGeneric,
	UObject* TargetObject,
	UFunction* Function,
	bool bInjectMixinObject)
{
	// Backward-compatible entry point: external callers (notably automation
	// tests) hold an asIScriptGeneric* and a UFunction* without owning a
	// FBlueprintCallableReflectiveSignature. The bridge builds a transient
	// signature and routes through the same cached + Invoke-based fast path
	// as the bound trampoline. Even with the per-call cache build, skipping
	// ProcessEvent is a net win.
	return InvokeReflectiveUFunctionFromGenericCall_Bridge(
		InGeneric, TargetObject, Function, bInjectMixinObject);
}

bool BindBlueprintCallableReflectionFallback(
	TSharedRef<FAngelscriptType> InType,
	UFunction* Function,
	FAngelscriptFunctionSignature& Signature,
	FAngelscriptFunctionBinding& Binding)
{
	Binding.bReflectiveFallbackBound = false;

	if (!ShouldBindBlueprintCallableReflectionFallback(Function))
	{
		return false;
	}

	if (!Signature.bAllTypesValid || Signature.ArgumentTypes.Num() > BlueprintCallableReflectiveFallbackMaxArgs)
	{
		return false;
	}

	if (IsScriptDeclarationAlreadyBound(InType, Signature))
	{
		return false;
	}

	auto* ReflectiveSignature = new FBlueprintCallableReflectiveSignature();
	ReflectiveSignature->UnrealFunction = Function;
	ReflectiveSignature->ArgCount = Signature.ArgumentTypes.Num();
	ReflectiveSignature->ReturnType = Signature.ReturnType;

	for (int32 ArgumentIndex = 0; ArgumentIndex < ReflectiveSignature->ArgCount; ++ArgumentIndex)
	{
		ReflectiveSignature->Arguments[ArgumentIndex] = Signature.ArgumentTypes[ArgumentIndex];
	}

	if (ReflectiveSignature->ReturnType.IsValid())
	{
		ReflectiveSignature->bInitReturn = ReflectiveSignature->ReturnType.CanConstruct() && ReflectiveSignature->ReturnType.NeedConstruct();
		ReflectiveSignature->bZeroReturnPtr = !ReflectiveSignature->bInitReturn && ReflectiveSignature->ReturnType.Type->IsObjectPointer();
	}

	if (!BindReflectiveFunction(InType, Signature, ReflectiveSignature))
	{
		delete ReflectiveSignature;
		return false;
	}

	Binding.bReflectiveFallbackBound = true;
	Binding.Origin = EAngelscriptFunctionBindingOrigin::Reflective;
	return true;
}

// ---- Opt 1 / Opt 3: Scoped bind caches used during Phase 2 of Bind_Defaults ----
FScopedBindCaches::FScopedBindCaches()
{
	// Nested guards would silently clobber TLS state; forbid re-entry.
	checkf(GBindCachesTLS == nullptr, TEXT("FScopedBindCaches is not re-entrant"));

	static thread_local FBindCachesTLS TlsStorage;
	TlsStorage.GlobalDecls.Reset();
	TlsStorage.ClassFuncNames.Reset();
	TlsStorage.LastSyncedGlobalFunctionCount = 0;
	TlsStorage.bGlobalDeclsActive = true;
	TlsStorage.bClassFuncNamesActive = true;

	GBindCachesTLS = &TlsStorage;
}

FScopedBindCaches::~FScopedBindCaches()
{
	if (GBindCachesTLS == nullptr)
	{
		return;
	}

	GBindCachesTLS->bGlobalDeclsActive = false;
	GBindCachesTLS->bClassFuncNamesActive = false;
	GBindCachesTLS->GlobalDecls.Reset();
	GBindCachesTLS->ClassFuncNames.Reset();
	GBindCachesTLS = nullptr;
}

void AngelscriptBindCaches_NotifyGlobalFunctionRegistered(const char* /*Namespace*/, const char* /*Name*/, const char* /*Declaration*/)
{
	// Deprecated stub: cache is refreshed lazily from ScriptEngine->GetGlobalFunction* on query.
	// Retained for ABI/header compatibility.
}

bool AngelscriptBindCaches_TryHasFunctionName(UClass* OwningClass, FName FunctionName, bool& bOutExists)
{
	if (GBindCachesTLS == nullptr || !GBindCachesTLS->bClassFuncNamesActive || OwningClass == nullptr)
	{
		return false;
	}

	TSet<FName>* NameSet = GBindCachesTLS->ClassFuncNames.Find(OwningClass);
	if (NameSet == nullptr)
	{
		// Lazily populate: enumerate own + super chain to match FindFunctionByName semantics.
		TSet<FName>& NewSet = GBindCachesTLS->ClassFuncNames.Add(OwningClass);
		for (UClass* C = OwningClass; C != nullptr; C = C->GetSuperClass())
		{
			for (TFieldIterator<UFunction> It(C, EFieldIteratorFlags::ExcludeSuper); It; ++It)
			{
				NewSet.Add(It->GetFName());
			}
		}
		NameSet = &NewSet;
	}

	bOutExists = NameSet->Contains(FunctionName);
	return true;
}
