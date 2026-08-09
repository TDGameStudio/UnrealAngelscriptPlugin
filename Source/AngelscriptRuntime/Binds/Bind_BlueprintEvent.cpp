#include "AngelscriptBinds.h"
#include "AngelscriptEngine.h"
#include "AngelscriptType.h"
#include "BlueprintCallableReflectiveFallback.h"
#include "BlueprintEventSignatureRegistry.h"
#include "ClassGenerator/ASClass.h"

#include "Containers/StringConv.h"
#include "UObject/UObjectIterator.h"
#include "UObject/UnrealType.h"

#include "StartAngelscriptHeaders.h"
//#include "as_generic.h"
//#include "as_scriptfunction.h"
#include "source/as_generic.h"
#include "source/as_scriptfunction.h"
#include "EndAngelscriptHeaders.h"

#include "Helper_FunctionSignature.h"
#include "Bind_BlueprintTypePrep.h"

/**
 * Blueprint-event helper, reflected event, and delegate-call binding surface.
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | AngelScript usage signature                                                                | Purpose / parameter notes                                                                                            |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | void __Evt_PushArgument__<TypeName>(const <Type>& Value);                                  | Expands once per complete registered type that can construct, copy, and destruct.                                    |
 * |                                                                                            | Pushes a copied argument into the pending internal reflected-event call.                                             |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | void __Evt_PushArgumentRef__<TypeName>(const <Type>& Value);                               | Expands for the same eligible types and pushes a writable reference argument.                                        |
 * |                                                                                            | Stable aliases are also ensured for int32, uint32, float32, and float64.                                             |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | void __Evt_PushArgument(const ?& Value);                                                   | Pushes a copied type-erased argument after runtime type validation.                                                  |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | void __Evt_PushArgumentRef(const ?& Value);                                                | Pushes a writable type-erased reference argument after runtime type validation.                                      |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | void __Evt_Execute(const UObject Object, const FName& Name);                               | Executes the pending argument list against a reflected event on Object.                                              |
 * |                                                                                            | @param Name Reflected event function name.                                                                           |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | void __Evt_ExecuteDelegate(const _FScriptDelegate& Delegate);                              | Executes the pending argument list against a single-cast script delegate.                                            |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | void __Evt_ExecuteDelegate(const _FMulticastScriptDelegate& Delegate);                     | Broadcasts the pending argument list through a multicast script delegate.                                            |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | ReturnType EventOwner.EventName(Arguments...);                                             | Expands for eligible instance Blueprint events with at most 16 supported arguments.                                  |
 * |                                                                                            | The declaration preserves reflected return, input, output, default, and callable metadata.                           |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | ReturnType EventOwner::EventName(Arguments...);                                            | Expands for eligible events exposed as static AngelScript namespace functions.                                       |
 * |                                                                                            | Invocation targets the reflected class default object.                                                               |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | ReturnType Receiver.EventName(Arguments...);                                               | Expands for eligible static Unreal functions exposed as AngelScript mixins.                                          |
 * |                                                                                            | Receiver supplies the mixin object type while invocation targets the class default object.                           |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | ReturnType MulticastDelegate.Broadcast(Arguments...);                                      | Expands from multicast delegate signatures whose argument types support call marshalling.                            |
 * |                                                                                            | Broadcast is a no-op when the delegate is unbound.                                                                   |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | ReturnType SparseDelegate.Broadcast(Arguments...);                                         | Expands from sparse delegate signatures whose argument types support call marshalling.                               |
 * |                                                                                            | The sparse owner and backing multicast delegate are resolved at invocation time.                                     |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | ReturnType Delegate.Execute(Arguments...);                                                 | Expands from single-cast delegate signatures.                                                                        |
 * |                                                                                            | Raises an AngelScript exception when the delegate is unbound.                                                        |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | ReturnType Delegate.ExecuteIfBound(Arguments...);                                          | Expands from single-cast delegate signatures.                                                                        |
 * |                                                                                            | Returns without invocation when the delegate is unbound.                                                             |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 */

#define AS_EVENT_MAX_ARGS 16
#define AS_EVENT_MAX_SIZE 1024

struct FScriptCall;
static FScriptCall* GCurrentCall = nullptr;
static FScriptCall* GStoredCall = nullptr;

static bool DoesCallArgumentMatchProperty(const FAngelscriptTypeUsage& ArgumentType, const FProperty* Property)
{
	if (!ArgumentType.IsValid() || Property == nullptr)
	{
		return false;
	}

	const FAngelscriptType::EPropertyMatchType MatchType = Property->HasAnyPropertyFlags(CPF_ReturnParm)
		? FAngelscriptType::EPropertyMatchType::OverrideReturnValue
		: FAngelscriptType::EPropertyMatchType::OverrideArgument;
	return ArgumentType.MatchesProperty(Property, MatchType);
}

static bool TryExtractMulticastFunctionNames(const FMulticastScriptDelegate& Delegate, TArray<FName>& OutFunctionNames)
{
	const FString DelegateString = Delegate.ToString<UObject>();
	if (DelegateString == TEXT("<Unbound>"))
	{
		return true;
	}

	FString TrimmedString = DelegateString;
	TrimmedString.RemoveFromStart(TEXT("["));
	TrimmedString.RemoveFromEnd(TEXT("]"));

	TArray<FString> Entries;
	TrimmedString.ParseIntoArray(Entries, TEXT(", "), true);
	for (const FString& Entry : Entries)
	{
		int32 LastDotIndex = INDEX_NONE;
		if (!Entry.FindLastChar(TEXT('.'), LastDotIndex) || LastDotIndex == INDEX_NONE || LastDotIndex + 1 >= Entry.Len())
		{
			return false;
		}

		OutFunctionNames.Add(FName(*Entry.Mid(LastDotIndex + 1)));
	}

	return true;
}

TMap<UClass*, TMap<FString, UFunction*>> GBlueprintEventsByScriptName;

// Diagnostic accessor used by the cross-cycle bounded-count regression
// tests in AngelscriptTest. We don't export the global itself across module
// boundaries; this single ANGELSCRIPTRUNTIME_API entry is enough for tests
// to assert the table doesn't accumulate across engine cycles.
ANGELSCRIPTRUNTIME_API int32 GetBlueprintEventsByScriptNameTotalCount()
{
	int32 Total = 0;
	for (const TPair<UClass*, TMap<FString, UFunction*>>& Outer : GBlueprintEventsByScriptName)
	{
		Total += Outer.Value.Num();
	}
	return Total;
}

void CleanupStaleBlueprintEventsByScriptName()
{
	for (auto ClassIt = GBlueprintEventsByScriptName.CreateIterator(); ClassIt; ++ClassIt)
	{
		UClass* Class = ClassIt.Key();
		if (Class == nullptr || !Class->IsValidLowLevelFast() || Class->HasAnyClassFlags(CLASS_NewerVersionExists) || Class->IsUnreachable() || Cast<UASClass>(Class) != nullptr)
		{
			ClassIt.RemoveCurrent();
			continue;
		}

		for (auto FunctionIt = ClassIt.Value().CreateIterator(); FunctionIt; ++FunctionIt)
		{
			UFunction* Function = FunctionIt.Value();
			if (Function == nullptr || !Function->IsValidLowLevelFast() || Function->IsUnreachable())
			{
				FunctionIt.RemoveCurrent();
			}
		}

		if (ClassIt.Value().IsEmpty())
		{
			ClassIt.RemoveCurrent();
		}
	}
}

UFunction* GetBlueprintEventByScriptName(UClass* Class, const FString& ScriptName)
{
	UClass* CheckClass = Class;
	while(CheckClass != nullptr)
	{
		auto* List = GBlueprintEventsByScriptName.Find(CheckClass);
		if (List != nullptr)
		{
			auto** Function = List->Find(ScriptName);
			if (Function != nullptr)
			{
				return *Function;
			}
		}

		for (TFieldIterator<UFunction> FunctionIt(CheckClass, EFieldIteratorFlags::ExcludeSuper); FunctionIt; ++FunctionIt)
		{
			UFunction* Function = *FunctionIt;
			if (Function != nullptr
				&& Function->HasAnyFunctionFlags(FUNC_BlueprintEvent)
				&& FAngelscriptFunctionSignature::GetScriptNameForFunction(Function) == ScriptName)
			{
				GBlueprintEventsByScriptName.FindOrAdd(CheckClass).Add(ScriptName, Function);
				return Function;
			}
		}

		CheckClass = CheckClass->GetSuperClass();
	}

	return nullptr;
}

#define SCRIPTCALL_INLINE FORCEINLINE_DEBUGGABLE
struct alignas(64) FScriptCall
{
	struct FArgumentInBuffer
	{
		FAngelscriptTypeUsage Type;
		SIZE_T Offset;
		void* Reference;
	};

	uint8 ArgumentBuffer[AS_EVENT_MAX_SIZE];
	FArgumentInBuffer ArgumentTypes[AS_EVENT_MAX_ARGS];
	int32 ArgumentIndex = 0;
	SIZE_T ArgumentOffset = 0;

	SCRIPTCALL_INLINE void AbortExecution(const FString& ErrorMessage)
	{
		ResetArguments();

		check(GCurrentCall == this);
		GCurrentCall = nullptr;

		if (GStoredCall == nullptr)
		{
			GStoredCall = this;
		}
		else
		{
			this->~FScriptCall();
			FMemory::Free(this);
		}

		const FTCHARToUTF8 ErrorMessageUtf8(*ErrorMessage);
		FAngelscriptEngine::Throw(ErrorMessageUtf8.Get());
	}

	SCRIPTCALL_INLINE bool ValidateAgainstFunction(const UFunction* Function, FString& OutErrorMessage) const
	{
		if (Function == nullptr)
		{
			OutErrorMessage = TEXT("Attempted to execute an event or delegate without a bound function.");
			return false;
		}

		int32 PropertyIndex = 0;
		for (TFieldIterator<FProperty> It(Function); It && (It->PropertyFlags & CPF_Parm); ++It)
		{
			if (PropertyIndex >= ArgumentIndex)
			{
				OutErrorMessage = FString::Printf(TEXT("Signature mismatch while executing '%s': too few arguments were pushed."), *Function->GetName());
				return false;
			}

			if (!DoesCallArgumentMatchProperty(ArgumentTypes[PropertyIndex].Type, *It))
			{
				OutErrorMessage = FString::Printf(TEXT("Signature mismatch while executing '%s' at parameter '%s'."), *Function->GetName(), *It->GetName());
				return false;
			}

			++PropertyIndex;
		}

		if (PropertyIndex != ArgumentIndex)
		{
			OutErrorMessage = FString::Printf(TEXT("Signature mismatch while executing '%s': too many arguments were pushed."), *Function->GetName());
			return false;
		}

		if (Function->ParmsSize != ArgumentOffset)
		{
			OutErrorMessage = FString::Printf(TEXT("Signature mismatch while executing '%s': argument buffer size %d does not match expected parameter size %d."), *Function->GetName(), static_cast<int32>(ArgumentOffset), Function->ParmsSize);
			return false;
		}

		return true;
	}

	SCRIPTCALL_INLINE void ResetNonArgumentVariables()
	{
		ArgumentIndex = 0;
		ArgumentOffset = 0;
	}

	SCRIPTCALL_INLINE void ResetArguments()
	{
		for (int32 i = 0; i < ArgumentIndex; ++i)
		{
			void* StoredPtr = &ArgumentBuffer[ArgumentTypes[i].Offset];
			ArgumentTypes[i].Type.DestructValue(StoredPtr);
		}

		ResetNonArgumentVariables();
	}

	SCRIPTCALL_INLINE void ResetArgumentsAndCopyBackReferences()
	{
		for (int32 i = 0; i < ArgumentIndex; ++i)
		{
			auto& ArgType = ArgumentTypes[i];
			void* StoredPtr = &ArgumentBuffer[ArgType.Offset];

			if (ArgType.Type.bIsReference && !ArgType.Type.bIsConst)
				ArgType.Type.CopyValue(StoredPtr, ArgType.Reference);

			ArgType.Type.DestructValue(StoredPtr);
		}

		ResetNonArgumentVariables();
	}

	template<bool TCheckErrors = true, bool TCopyInitialValue = true>
	SCRIPTCALL_INLINE void PushArgument(FAngelscriptTypeUsage& Type, void* ValueRef)
	{
		if ((TCheckErrors || DO_CHECK) && ArgumentIndex >= AS_EVENT_MAX_ARGS)
		{
			ResetArguments();
			FAngelscriptEngine::Throw("Too many arguments to event.");
			return;
		}

		int32 ArgumentAlign = Type.GetValueAlignment();
		int32 ArgumentSize = Type.GetValueSize();

		ArgumentOffset = Align(ArgumentOffset, ArgumentAlign);

		if ((TCheckErrors || DO_CHECK) && ArgumentOffset + ArgumentSize >= AS_EVENT_MAX_SIZE)
		{
			ResetArguments();
			FAngelscriptEngine::Throw("Arguments to event too large.");
			return;
		}

		auto& ArgType = ArgumentTypes[ArgumentIndex];
		ArgType.Type = Type;
		ArgType.Offset = ArgumentOffset;

		void* StoredPtr = &ArgumentBuffer[ArgumentOffset];
		Type.ConstructValue(StoredPtr);

		if (Type.bIsReference)
		{
			void* OrigValueRef = *(void**)ValueRef;
			if (TCopyInitialValue)
				Type.CopyValue(OrigValueRef, StoredPtr);
			ArgType.Reference = OrigValueRef;
		}
		else
		{
			if (TCopyInitialValue)
			{
				Type.CopyValue(ValueRef, StoredPtr);
			}
		}

		ArgumentOffset += ArgumentSize;
		ArgumentIndex += 1;
	}

	SCRIPTCALL_INLINE void ExecutePreamble()
	{
		check(GCurrentCall == this);
		check(IsInGameThread());
		GCurrentCall = nullptr;
	}

	SCRIPTCALL_INLINE void ExecuteCleanup()
	{
		ResetArgumentsAndCopyBackReferences();

		// We store one call struct for future use
		if (GStoredCall == nullptr)
		{
			GStoredCall = this;
		}
		else
		{
			this->~FScriptCall();
			FMemory::Free(this);
		}
	}

	SCRIPTCALL_INLINE void ExecuteEvent(UObject* Object, FName EventName)
	{
		UFunction* Function = Object->FindFunctionChecked(EventName);
		FString ValidationError;
		if (!ValidateAgainstFunction(Function, ValidationError))
		{
			AbortExecution(ValidationError);
			return;
		}

		ExecutePreamble();

		Object->ProcessEvent(Function, &ArgumentBuffer[0]);

		ExecuteCleanup();
	}

	SCRIPTCALL_INLINE void ExecuteDelegate(FScriptDelegate& Delegate)
	{
		UObject* BoundObject = Delegate.GetUObject();
		UFunction* BoundFunction = BoundObject != nullptr ? BoundObject->FindFunction(Delegate.GetFunctionName()) : nullptr;
		FString ValidationError;
		if (!ValidateAgainstFunction(BoundFunction, ValidationError))
		{
			AbortExecution(ValidationError);
			return;
		}

		ExecutePreamble();

		Delegate.ProcessDelegate<UObject>(&ArgumentBuffer[0]);

		ExecuteCleanup();
	}

	SCRIPTCALL_INLINE void ExecuteMulticastDelegate(FMulticastScriptDelegate& Delegate)
	{
		TArray<UObject*> BoundObjects = Delegate.GetAllObjects();
		TArray<FName> BoundFunctionNames;
		if (!TryExtractMulticastFunctionNames(Delegate, BoundFunctionNames) || BoundObjects.Num() != BoundFunctionNames.Num())
		{
			AbortExecution(TEXT("Signature mismatch while executing multicast delegate: failed to resolve bound functions."));
			return;
		}

		for (int32 DelegateIndex = 0; DelegateIndex < BoundObjects.Num(); ++DelegateIndex)
		{
			UObject* BoundObject = BoundObjects[DelegateIndex];
			UFunction* BoundFunction = BoundObject != nullptr ? BoundObject->FindFunction(BoundFunctionNames[DelegateIndex]) : nullptr;
			FString ValidationError;
			if (!ValidateAgainstFunction(BoundFunction, ValidationError))
			{
				AbortExecution(ValidationError);
				return;
			}
		}

		ExecutePreamble();

		Delegate.ProcessDelegate<UObject>(&ArgumentBuffer[0]);

		ExecuteCleanup();
	}
};

SCRIPTCALL_INLINE FScriptCall& CurrentCall()
{
	if (GCurrentCall == nullptr)
	{
		if (GStoredCall != nullptr)
		{
			GCurrentCall = GStoredCall;
			GStoredCall = nullptr;
		}
		else
		{
			GCurrentCall = (FScriptCall*)FMemory::Malloc(sizeof(FScriptCall), alignof(FScriptCall));
			new(GCurrentCall) FScriptCall();
		}
	}
	return *GCurrentCall;
}

FORCEINLINE FScriptCall& CurrentCall_NoCheck()
{
	return *GCurrentCall;
}

namespace
{
	struct FAngelscriptBlueprintEventHelperBinds
	{
		static void CallStaticWithSignature(asIScriptGeneric* InGeneric);
		static void CallEventWithSignature(asIScriptGeneric* InGeneric);
		static void CallMixinWithSignature(asIScriptGeneric* InGeneric);

		template<bool TIsMulticast, bool TErrorIfUnbound>
		static void CallDelegateEvent(asIScriptGeneric* InGeneric);

		static void CallSparseDelegate(asIScriptGeneric* InGeneric);

		static void PushArgumentSpecialized(void* ArgumentRef)
		{
			FAngelscriptTypeUsage Type;
			Type.Type = *FAngelscriptEngine::GetCurrentFunctionUserData<TSharedPtr<FAngelscriptType>>();

			CurrentCall().PushArgument(Type, ArgumentRef);
		}

		static void PushArgumentRefSpecialized(void* ArgumentRef)
		{
			FAngelscriptTypeUsage Type;
			Type.Type = *FAngelscriptEngine::GetCurrentFunctionUserData<TSharedPtr<FAngelscriptType>>();
			Type.bIsReference = true;
			Type.bIsConst = false;

			CurrentCall().PushArgument(Type, &ArgumentRef);
		}

		static void PushArgument(void* ArgumentRef, int ArgumentType)
		{
			FAngelscriptTypeUsage Type = FAngelscriptTypeUsage::FromTypeId(ArgumentType);

			if (!Type.CanConstruct() || !Type.CanCopy() || !Type.CanDestruct())
			{
				ensure(false);
				CurrentCall().ResetArguments();
				FAngelscriptEngine::Throw("Attempted to push invalid event argument type.");
				return;
			}

			CurrentCall().PushArgument(Type, ArgumentRef);
		}

		static void PushArgumentRef(void* ArgumentRef, int ArgumentType)
		{
			FAngelscriptTypeUsage Type = FAngelscriptTypeUsage::FromTypeId(ArgumentType);
			if (!Type.CanConstruct() || !Type.CanCopy() || !Type.CanDestruct())
			{
				ensure(false);
				CurrentCall().ResetArguments();
				FAngelscriptEngine::Throw("Attempted to push invalid event argument type.");
				return;
			}

			Type.bIsReference = true;
			Type.bIsConst = false;

			CurrentCall().PushArgument(Type, &ArgumentRef);
		}

		static void ExecuteEvent(UObject* Object, const FName& Name)
		{
			CurrentCall().ExecuteEvent(Object, Name);
		}

		static void ExecuteDelegate(FScriptDelegate& Delegate)
		{
			CurrentCall().ExecuteDelegate(Delegate);
		}

		static void ExecuteMulticastDelegate(FMulticastScriptDelegate& Delegate)
		{
			CurrentCall().ExecuteMulticastDelegate(Delegate);
		}
	};

	void BindPushArgument(FAngelscriptBinds& Binds, const FString& PushTypeName, TSharedPtr<FAngelscriptType> Type)
	{
		Binds.GetTargetEngine().BoundBlueprintEventArgumentSpecializations.Add(PushTypeName);

		// Create a 'push argument' function
		TSharedPtr<FAngelscriptType>* TypePtr = new TSharedPtr<FAngelscriptType>(Type);
		FString Decl = FString::Printf(TEXT("void __Evt_PushArgument__%s(const %s& Value)"),
			*PushTypeName,
			*Type->GetAngelscriptDeclaration(FAngelscriptTypeUsage::DefaultUsage, FAngelscriptType::FunctionArgument));

		Binds.BindGlobalFunctionForTarget(Decl, &FAngelscriptBlueprintEventHelperBinds::PushArgumentSpecialized, TypePtr)
			.NativePushArgument();

		// Create a 'push argument ref' function
		Decl = FString::Printf(TEXT("void __Evt_PushArgumentRef__%s(const %s& Value)"),
			*PushTypeName,
			*Type->GetAngelscriptDeclaration(FAngelscriptTypeUsage::DefaultUsage, FAngelscriptType::FunctionArgument));

		Binds.BindGlobalFunctionForTarget(Decl, &FAngelscriptBlueprintEventHelperBinds::PushArgumentRefSpecialized, TypePtr)
			.NativePushArgumentRef();
	}

	void BindAliasedPushArgument(FAngelscriptBinds& Binds, const FString& Alias, const FString& RealType)
	{
		if (Binds.GetTargetEngine().BoundBlueprintEventArgumentSpecializations.Contains(Alias))
			return;

		const TSharedRef<FAngelscriptType>* RegisteredType = Binds.GetTargetTypeDatabase().TypesByAngelscriptName.Find(RealType);
		check(RegisteredType != nullptr);
		BindPushArgument(Binds, Alias, RegisteredType->ToSharedPtr());
	}

}

AS_FORCE_LINK const FAngelscriptBind Bind_BlueprintEvents(
	TEXT("BlueprintEvents.HelperGlobals"),
	EAngelscriptBindPhase::ExplicitBindings,
	[](FAngelscriptBinds& Binds)
	{
		const TArray<TSharedRef<FAngelscriptType>>& Types = Binds.GetTargetTypeDatabase().RegisteredTypes;
		for (const TSharedRef<FAngelscriptType>& Type : Types)
		{
			if (!Type->DescribesCompleteType(FAngelscriptTypeUsage::DefaultUsage))
				continue;

			// We need specific operations to be able to use this as an event argument
			if (!Type->CanConstruct(FAngelscriptTypeUsage::DefaultUsage))
				continue;
			if (!Type->CanCopy(FAngelscriptTypeUsage::DefaultUsage))
				continue;
			if (!Type->CanDestruct(FAngelscriptTypeUsage::DefaultUsage))
				continue;

			FString PushTypeName = Type->GetAngelscriptTypeName();
			BindPushArgument(Binds, PushTypeName, Type.ToSharedPtr());
		}

		// Make sure aliased arguments are bound
		BindAliasedPushArgument(Binds, TEXT("int32"), TEXT("int32"));
		BindAliasedPushArgument(Binds, TEXT("uint32"), TEXT("uint32"));
		BindAliasedPushArgument(Binds, TEXT("float32"), TEXT("float32"));
		BindAliasedPushArgument(Binds, TEXT("float64"), TEXT("float64"));

		// Bind a generic 'push argument' function that does a runtime type lookup
		Binds.BindGlobalFunctionForTarget(
			"void __Evt_PushArgument(const ?& Value)",
			&FAngelscriptBlueprintEventHelperBinds::PushArgument)
			.NativePushArgument();

		// Bind a generic 'push argument ref' function that does a runtime type lookup
		Binds.BindGlobalFunctionForTarget(
			"void __Evt_PushArgumentRef(const ?& Value)",
			&FAngelscriptBlueprintEventHelperBinds::PushArgumentRef)
			.NativePushArgumentRef();

		// Bind the actual call execution
		Binds.BindGlobalFunctionForTarget(
			"void __Evt_Execute(const UObject Object, const FName& Name)",
			&FAngelscriptBlueprintEventHelperBinds::ExecuteEvent)
			.NativeEventFunctionExecute();

		// Generic call delegate
		Binds.BindGlobalFunctionForTarget(
			"void __Evt_ExecuteDelegate(const _FScriptDelegate& Delegate)",
			&FAngelscriptBlueprintEventHelperBinds::ExecuteDelegate)
			.NativeDelegateExecute();

		// Generic call multicast delegate
		Binds.BindGlobalFunctionForTarget(
			"void __Evt_ExecuteDelegate(const _FMulticastScriptDelegate& Delegate)",
			&FAngelscriptBlueprintEventHelperBinds::ExecuteMulticastDelegate)
			.NativeMulticastExecute();
	});

// Called from Bind_BlueprintCallable
struct FBlueprintEventSignature
{
	FAngelscriptTypeUsage ReturnType;
	FAngelscriptTypeUsage Arguments[AS_EVENT_MAX_ARGS];
	FAngelscriptTypeUsage MixinType;
	int32 ArgCount = 0;
	int32 OutReferences[AS_EVENT_MAX_ARGS];
	int32 OutCount = 0;
	FName FunctionName;
	UObject* StaticObject = nullptr;
	bool bInitReturn = false;
	bool bZeroReturnPtr = false;
	UFunction* UnrealFunction = nullptr;
};

// Centralized deleter referenced by FBlueprintEventSignatureRegistry::Reset().
// Defined here because the registry header only forward-declares the signature
// type to avoid pulling AS_EVENT_MAX_ARGS and FAngelscriptTypeUsage into a
// public header.
namespace BlueprintEventSignatureRegistryInternal
{
	void DropOwnedSignature(void* Signature)
	{
		delete static_cast<FBlueprintEventSignature*>(Signature);
	}
}

// Allocate a new signature and immediately transfer ownership to the explicit
// target engine's registry. The returned pointer remains stable for the
// lifetime of that engine (or until its registry is reset during Shutdown()).
//
// FAngelscriptEngine::Initialize* unconditionally allocates the registry, so
// by the time any BindBlueprintEvent_* helper runs the registry must exist.
// The target-aware registry accessor enforces that invariant so a future
// bootstrap reorder fails instead of silently leaking — every signature that
// escapes the registry survives every subsequent engine cycle (see
// ASBindFreeCompletenessVerification.md §4 for why this leak class is
// expensive to detect after the fact).
static FBlueprintEventSignature* NewOwnedBlueprintEventSignature(FAngelscriptBinds& Binds)
{
	auto* Sig = new FBlueprintEventSignature;
	Binds.GetTargetBlueprintEventSignatureRegistry().AddOwnership(Sig);
	return Sig;
}

void FAngelscriptBlueprintEventHelperBinds::CallStaticWithSignature(asIScriptGeneric* InGeneric)
{
	asCGeneric* Generic = static_cast<asCGeneric*>(InGeneric);
	auto* Function = (asCScriptFunction*)Generic->GetFunction();
	auto* Sig = (FBlueprintEventSignature*)Function->GetUserData();
	if (Sig == nullptr)
	{
		return;
	}

	InvokeReflectionFallbackFromGenericCall(Generic, Sig->StaticObject, Sig->UnrealFunction);
}

void FAngelscriptBlueprintEventHelperBinds::CallEventWithSignature(asIScriptGeneric* InGeneric)
{
	asCGeneric* Generic = static_cast<asCGeneric*>(InGeneric);
	auto* Function = Generic->GetFunction();
	auto* Sig = (FBlueprintEventSignature*)Function->GetUserData();
	if (Sig == nullptr)
	{
		return;
	}

	InvokeReflectionFallbackFromGenericCall(Generic, static_cast<UObject*>(Generic->GetObject()), Sig->UnrealFunction);
}

void FAngelscriptBlueprintEventHelperBinds::CallMixinWithSignature(asIScriptGeneric* InGeneric)
{
	asCGeneric* Generic = static_cast<asCGeneric*>(InGeneric);
	auto* Function = Generic->GetFunction();
	auto* Sig = (FBlueprintEventSignature*)Function->GetUserData();
	if (Sig == nullptr)
	{
		return;
	}

	InvokeReflectionFallbackFromGenericCall(Generic, Sig->StaticObject, Sig->UnrealFunction, true);
}

static const FName NAME_Event_DeprecatedFunction("DeprecatedFunction");
static const FName NAME_Event_NotInAngelscript("NotInAngelscript");
static const FName NAME_Event_BlueprintInternalUseOnly("BlueprintInternalUseOnly");
static const FName NAME_Event_ConstructionScript("UserConstructionScript");
static const FName NAME_Event_AllowAngelscriptOverride("AllowAngelscriptOverride");
static const FName NAME_Event_ScriptCallable("ScriptCallable");

static void CommitBlueprintEventBinding(
	FAngelscriptBinds& Binds,
	TSharedRef<FAngelscriptType> InType,
	UFunction* Function,
	FAngelscriptFunctionSignature& Signature,
	const FString& NativeFunctionName)
{
	auto* Sig = NewOwnedBlueprintEventSignature(Binds);
	Sig->FunctionName = Function->GetFName();
	Sig->UnrealFunction = Function;
	Sig->ArgCount = Signature.ArgumentTypes.Num();
	Sig->ReturnType = Signature.ReturnType;
	check(!Sig->ReturnType.bIsReference);
	Sig->ReturnType.bIsReference = true;
	for (int32 i = 0; i < Sig->ArgCount; ++i)
	{
		Sig->Arguments[i] = Signature.ArgumentTypes[i];
	}

	if (Sig->ReturnType.IsValid())
	{
		Sig->bInitReturn = Sig->ReturnType.CanConstruct() && Sig->ReturnType.NeedConstruct();
		Sig->bZeroReturnPtr = !Sig->bInitReturn && Sig->ReturnType.Type->IsObjectPointer();
	}

	FAngelscriptBoundFunction PrimaryBinding;
	if (Signature.bStaticInScript)
	{
		Sig->StaticObject = InType->GetClass(FAngelscriptTypeUsage::DefaultUsage)->GetDefaultObject();

		FAngelscriptBinds::FNamespace Namespace(Binds.GetTargetEngine(), Signature.ClassName);
		PrimaryBinding = Binds.BindGlobalFunctionDirectForTarget(
			Signature.Declaration,
			asFUNCTION(FAngelscriptBlueprintEventHelperBinds::CallStaticWithSignature),
			asCALL_GENERIC,
			ASAutoCaller::FunctionCaller::Make(),
			Sig);
	}
	else if (Signature.bStaticInUnreal)
	{
		Sig->StaticObject = InType->GetClass(FAngelscriptTypeUsage::DefaultUsage)->GetDefaultObject();

		Sig->MixinType = FAngelscriptTypeUsage();
		Sig->MixinType.Type = InType;

		PrimaryBinding = Binds.BindMethodDirectForTarget(
			Signature.ClassName,
			Signature.Declaration,
			asFUNCTION(FAngelscriptBlueprintEventHelperBinds::CallMixinWithSignature),
			asCALL_GENERIC,
			ASAutoCaller::FunctionCaller::Make(),
			Sig);
	}
	else
	{
		PrimaryBinding = Binds.BindMethodDirectForTarget(
			InType->GetAngelscriptTypeName(),
			Signature.Declaration,
			asFUNCTION(FAngelscriptBlueprintEventHelperBinds::CallEventWithSignature),
			asCALL_GENERIC,
			ASAutoCaller::FunctionCaller::Make(),
			Sig);
	}

	Signature.ModifyScriptFunction(PrimaryBinding);

#if WITH_EDITOR
	if (!Function->HasAnyFunctionFlags(FUNC_BlueprintCallable | FUNC_BlueprintPure)
		&& !Function->HasMetaData(NAME_Event_ScriptCallable))
	{
		PrimaryBinding.Callable(false);
	}
#endif

	GBlueprintEventsByScriptName.FindOrAdd(CastChecked<UClass>(Function->GetOuter())).Add(Signature.ScriptName, Function);

	PrimaryBinding.NativeUFunction(Function, NativeFunctionName, false);
}

void BindBlueprintEvent(
	FAngelscriptBinds& Binds,
	TSharedRef<FAngelscriptType> InType,
	UFunction* Function,
	FAngelscriptMethodBind& DBBind
#if !AS_USE_BIND_DB
	, const TCHAR* OverrideName
#endif
)
{

#if AS_USE_BIND_DB
	FAngelscriptFunctionSignature Signature;
	Signature.InitFromDB(
		Binds.GetTargetTypeDatabase(),
		InType,
		Function,
		DBBind,
		/* bInitTypes= */ true);

#elif !AS_USE_BIND_DB
	// Don't bind functions that are deprecated
	if (Function->HasMetaData(NAME_Event_DeprecatedFunction))
		return;

	// Specifically excluded functions are not bound
	if (Function->HasMetaData(NAME_Event_NotInAngelscript))
		return;

	// BlueprintInternalUseOnly functions are not bound, with the hardcoded exception of constructionscript
	if (Function->HasMetaData(NAME_Event_BlueprintInternalUseOnly) && Function->GetFName() != NAME_Event_ConstructionScript && !Function->HasMetaData(NAME_Event_AllowAngelscriptOverride))
		return;

	FAngelscriptFunctionSignature Signature(
		Binds.GetTargetTypeDatabase(),
		InType,
		Function,
		OverrideName);
#endif

	// Don't bind things that have types that are unknown to us
	if (!Signature.bAllTypesValid)
		return;
	if (Signature.ArgumentTypes.Num() > AS_EVENT_MAX_ARGS)
		return;

#if AS_USE_BIND_DB
	CommitBlueprintEventBinding(
		Binds,
		InType,
		Function,
		Signature,
		FPackageName::ObjectPathToObjectName(DBBind.UnrealPath));
#else
	CommitBlueprintEventBinding(Binds, InType, Function, Signature, Function->GetName());
#endif

#if !AS_USE_BIND_DB
	Signature.WriteToDB(DBBind);
#endif
}

#if !AS_USE_BIND_DB && WITH_EDITOR
// Prepare-only entry point used by BlueprintType ReflectionBindings.
// Performs all metadata gating + Signature build (read-only). On success, sets
// Prep.Kind = Event; on any rejection, leaves Prep.Kind = Skip.
// CachedBinding is unused for Event prepares.
void BindBlueprintEvent_Prepare(
	FAngelscriptBinds& Binds,
	TSharedRef<FAngelscriptType> InType,
	UFunction* Function,
	FUFunctionBindPrep& Prep)
{
	Prep.Function = Function;
	Prep.Kind = FUFunctionBindPrep::EKind::Skip;
	Prep.CachedBinding = nullptr;

	if (Function == nullptr)
	{
		return;
	}

	if (Function->HasMetaData(NAME_Event_DeprecatedFunction))
	{
		return;
	}
	if (Function->HasMetaData(NAME_Event_NotInAngelscript))
	{
		return;
	}
	if (Function->HasMetaData(NAME_Event_BlueprintInternalUseOnly)
		&& Function->GetFName() != NAME_Event_ConstructionScript
		&& !Function->HasMetaData(NAME_Event_AllowAngelscriptOverride))
	{
		return;
	}

	Prep.Signature = FAngelscriptFunctionSignature(
		Binds.GetTargetTypeDatabase(),
		InType,
		Function,
		/*OverrideName=*/nullptr);
	if (!Prep.Signature.bAllTypesValid)
	{
		return;
	}
	if (Prep.Signature.ArgumentTypes.Num() > AS_EVENT_MAX_ARGS)
	{
		return;
	}

	Prep.Kind = FUFunctionBindPrep::EKind::Event;
}

// Commit-only entry point used by the BlueprintType ReflectionBindings prepare/commit split.
// (see Plan_BindParallelization). The caller is responsible for filling Prep with:
//   - Function           (UFunction*, non-null, all event-eligibility metadata checks passed)
//   - Signature          (already InitFromFunction'd, bAllTypesValid == true,
//                         ArgumentTypes.Num() <= AS_EVENT_MAX_ARGS)
//   - Kind == Event
// CachedBinding is unused for Event commits. This function only performs the AS Engine
// register half (must run on GameThread).
void BindBlueprintEvent_FromPrep(
	FAngelscriptBinds& Binds,
	TSharedRef<FAngelscriptType> InType,
	FUFunctionBindPrep& Prep,
	FAngelscriptMethodBind& DBBind)
{
	UFunction* Function = Prep.Function;
	FAngelscriptFunctionSignature& Signature = Prep.Signature;

	if (Function == nullptr)
	{
		return;
	}

	CommitBlueprintEventBinding(Binds, InType, Function, Signature, Function->GetName());
	Signature.WriteToDB(DBBind);
}

#endif // !AS_USE_BIND_DB && WITH_EDITOR

template<bool TIsMulticast, bool TErrorIfUnbound>
void FAngelscriptBlueprintEventHelperBinds::CallDelegateEvent(asIScriptGeneric* InGeneric)
{
	asCGeneric* Generic = static_cast<asCGeneric*>(InGeneric);

	auto* Function = (asCScriptFunction*)Generic->GetFunction();
	auto* Sig = (FBlueprintEventSignature*)Function->GetUserData();
	void* Object = Generic->GetObject();
	if (!TIsMulticast)
	{
		FScriptDelegate& ScriptDelegate = *(FScriptDelegate*)Object;
		if (!ScriptDelegate.IsBound())
		{
			if (TErrorIfUnbound)
				FAngelscriptEngine::Throw("Executing unbound delegate.");
			return;
		}
	}
	else
	{
		FMulticastScriptDelegate& ScriptDelegate = *(FMulticastScriptDelegate*)Object;
		if (!ScriptDelegate.IsBound())
		{
			if (TErrorIfUnbound)
				FAngelscriptEngine::Throw("Executing unbound delegate.");
			return;
		}
	}

	FScriptCall& Call = CurrentCall();
	for (int32 Arg = 0; Arg < Sig->ArgCount; ++Arg)
		Call.PushArgument<false>(Sig->Arguments[Arg], Generic->GetAddressOfArg(Arg));
	if (Sig->ReturnType.IsValid())
	{
		void* ReturnPtr = Generic->GetAddressOfReturnLocation();
		if (Sig->bInitReturn)
			Sig->ReturnType.ConstructValue(ReturnPtr);
		else if (Sig->bZeroReturnPtr)
			*(void**)ReturnPtr = nullptr;
		Call.PushArgument<false,false>(Sig->ReturnType, &ReturnPtr);
	}

	if (!TIsMulticast)
	{
		FScriptDelegate& ScriptDelegate = *(FScriptDelegate*)Object;
		Call.ExecuteDelegate(ScriptDelegate);
	}
	else
	{
		FMulticastScriptDelegate& ScriptDelegate = *(FMulticastScriptDelegate*)Object;
		Call.ExecuteMulticastDelegate(ScriptDelegate);
	}
}

void FAngelscriptBlueprintEventHelperBinds::CallSparseDelegate(asIScriptGeneric* InGeneric)
{
	asCGeneric* Generic = static_cast<asCGeneric*>(InGeneric);

	auto* Function = Generic->GetFunction();
	auto* Sig = (FBlueprintEventSignature*)Function->GetUserData();
	void* Object = Generic->GetObject();

	FSparseDelegate& ScriptDelegate = *(FSparseDelegate*)Object;
	if (!ScriptDelegate.IsBound())
		return;

	FScriptCall& Call = CurrentCall();
	for (int32 Arg = 0; Arg < Sig->ArgCount; ++Arg)
		Call.PushArgument<false>(Sig->Arguments[Arg], Generic->GetAddressOfArg(Arg));
	if (Sig->ReturnType.IsValid())
	{
		void* ReturnPtr = Generic->GetAddressOfReturnLocation();
		if (Sig->bInitReturn)
			Sig->ReturnType.ConstructValue(ReturnPtr);
		else if (Sig->bZeroReturnPtr)
			*(void**)ReturnPtr = nullptr;
		Call.PushArgument<false,false>(Sig->ReturnType, &ReturnPtr);
	}

	USparseDelegateFunction* SparseDelegateFunc = CastChecked<USparseDelegateFunction>(Sig->UnrealFunction);
	UObject* OwningObject = FSparseDelegateStorage::ResolveSparseOwner(ScriptDelegate, SparseDelegateFunc->OwningClassName, SparseDelegateFunc->DelegateName);
	FMulticastScriptDelegate* MulticastDelegate = FSparseDelegateStorage::GetMulticastDelegate(OwningObject, SparseDelegateFunc->DelegateName);
	if (MulticastDelegate != nullptr)
	{
		Call.ExecuteMulticastDelegate(*MulticastDelegate);
	}
	else
	{
		// This probably shouldn't happen, but call an empty delegate to be safe
		FMulticastScriptDelegate EmptyDelegate;
		Call.ExecuteMulticastDelegate(EmptyDelegate);
	}
}

void BindDelegateEvent(FAngelscriptBinds& Binds, UFunction* Function, bool bIsMulticast, bool bIsSparse)
{
	FAngelscriptTypeUsage ReturnType;
	TArray<FAngelscriptTypeUsage> ArgumentTypes;
	TArray<FString> ArgumentNames;
	TArray<FString> ArgumentDefaults;

	bool bAllTypesValid = true;

	// Map all properties in the UFunction to FAngelscriptTypes
	for( TFieldIterator<FProperty> It(Function); It && (It->PropertyFlags & CPF_Parm); ++It )
	{
		FProperty* Property = *It;
		FAngelscriptTypeUsage Type = FAngelscriptTypeUsage::FromProperty(Binds.GetTargetTypeDatabase(), Property);

		if (!Type.IsValid() || !Type.CanCopy() || !Type.CanConstruct() || !Type.CanDestruct())
		{
			bAllTypesValid = false;
			break;
		}

		if( Property->PropertyFlags & CPF_ReturnParm )
		{
			ensure(!ReturnType.IsValid());
			ReturnType = Type;
		}
		else
		{
			ArgumentTypes.Add(Type);
			ArgumentNames.Add(Property->GetName());

			// This is a hack!
			//  We want to add the &in to the parameter if it is a const reference,
			//  that way we know that this requires an inref.
			if ((Property->PropertyFlags & CPF_OutParm) && (Property->PropertyFlags & CPF_ConstParm))
				ArgumentNames.Last() = TEXT("in ") + ArgumentNames.Last();
		}
	}

	// Don't bind things that have types that are unknown to us
	if (!bAllTypesValid)
		return;
	if (ArgumentTypes.Num() > AS_EVENT_MAX_ARGS)
		return;

	auto* Sig = NewOwnedBlueprintEventSignature(Binds);
	Sig->UnrealFunction = Function;
	Sig->FunctionName = Function->GetFName();
	Sig->ArgCount = ArgumentTypes.Num();
	Sig->ReturnType = ReturnType;
	check(!Sig->ReturnType.bIsReference);
	Sig->ReturnType.bIsReference = true;
	for (int32 i = 0; i < Sig->ArgCount; ++i)
		Sig->Arguments[i] = ArgumentTypes[i];

	if (Sig->ReturnType.IsValid())
	{
		Sig->bInitReturn = Sig->ReturnType.CanConstruct() && Sig->ReturnType.NeedConstruct();
		Sig->bZeroReturnPtr = !Sig->bInitReturn && Sig->ReturnType.Type->IsObjectPointer();
	}

	if (bIsSparse)
	{
		Binds.GenericMethod(
			FAngelscriptType::BuildFunctionDeclaration(ReturnType, TEXT("Broadcast"), ArgumentTypes, ArgumentNames, ArgumentDefaults, true),
			&FAngelscriptBlueprintEventHelperBinds::CallSparseDelegate, Sig);
	}
	else if (bIsMulticast)
	{
		Binds.GenericMethod(
			FAngelscriptType::BuildFunctionDeclaration(ReturnType, TEXT("Broadcast"), ArgumentTypes, ArgumentNames, ArgumentDefaults, true),
			&FAngelscriptBlueprintEventHelperBinds::CallDelegateEvent<true, false>, Sig);
	}
	else
	{
		Binds.GenericMethod(
			FAngelscriptType::BuildFunctionDeclaration(ReturnType, TEXT("Execute"), ArgumentTypes, ArgumentNames, ArgumentDefaults, true) + TEXT(" allow_discard"),
			&FAngelscriptBlueprintEventHelperBinds::CallDelegateEvent<false, true>, Sig);

		Binds.GenericMethod(
			FAngelscriptType::BuildFunctionDeclaration(ReturnType, TEXT("ExecuteIfBound"), ArgumentTypes, ArgumentNames, ArgumentDefaults, true) + TEXT(" allow_discard"),
			&FAngelscriptBlueprintEventHelperBinds::CallDelegateEvent<false, false>, Sig);
	}
}
