#if WITH_ANGELSCRIPT_NATIVE_MODULE_FUNCTION_ADDRESS

#include "Core/AngelscriptBinds.h"
#include "Core/AngelscriptEngine.h"
#include "Core/FunctionCallers.h"
#include "FunctionBinding/NativeModuleFunctionBindingBridge.h"

#include "Async/Async.h"
#include "Containers/Array.h"
#include "Features/IModularFeatures.h"
#include "HAL/CriticalSection.h"
#include "Misc/CoreDelegates.h"
#include "Misc/ScopeLock.h"
#include "Templates/Atomic.h"
#include "Templates/SharedPointer.h"
#include "UObject/FindObjectFlags.h"
#include "UObject/UObjectGlobals.h"

/**
 * Generated native-module transport contribution; this registrar declares no fixed script API.
 * +------------------------------------------------------------------------------------------+--------------------------------------------------------------------------------------------------------------------+
 * | AngelScript usage signature                                                              | Purpose / parameter notes                                                                                          |
 * +------------------------------------------------------------------------------------------+--------------------------------------------------------------------------------------------------------------------+
 * | <UHT-generated UFUNCTION signature>;                                                     | Replaces an eligible reflective binding with its versioned native-module function-address payload.                 |
 * |                                                                                          | The exact declaration is supplied by the target module and remains identical to its UFUNCTION surface.             |
 * +------------------------------------------------------------------------------------------+--------------------------------------------------------------------------------------------------------------------+
 */

void GAngelscriptNativeModuleFunctionBindingGenericThunk(asIScriptGeneric* Generic);
void GAngelscriptNativeModuleFunctionBindingUnregisterEngine(FAngelscriptEngine& Engine);
#if WITH_DEV_AUTOMATION_TESTS
ANGELSCRIPTRUNTIME_API void GAngelscriptNativeModuleFunctionBindingEnsureRegisteredForTesting();
ANGELSCRIPTRUNTIME_API int32 GAngelscriptNativeModuleFunctionBindingBindGlobalFunctionForTesting(
	FAngelscriptEngine& Engine,
	const ANSICHAR* Signature,
	FAngelscriptNativeModuleFunctionBinding* Binding);
#endif

namespace
{
	FDelegateHandle GNativeModuleFunctionBindingRegisteredHandle;
	FDelegateHandle GNativeModuleFunctionBindingUnregisteredHandle;
	FDelegateHandle GNativeModuleFunctionBindingObjectConstructedHandle;
	FDelegateHandle GNativeModuleFunctionBindingPreExitHandle;
	bool bNativeModuleFunctionBindingShuttingDown = false;

	struct FInjectedNativeModuleFunctionBinding
	{
		UClass* Class = nullptr;
		FString FunctionName;
		void* UserData = nullptr;
	};

	struct FNativeModuleFunctionBindingTargetState
	{
		bool bHasPendingBindings = false;
		TArray<int32> PendingBindingIndices;
		TArray<FInjectedNativeModuleFunctionBinding> InjectedBindings;
	};

	struct FNativeModuleFunctionBindingTargetEngine
	{
		explicit FNativeModuleFunctionBindingTargetEngine(FAngelscriptEngine& InEngine)
			: Engine(&InEngine)
		{
		}

		FCriticalSection Mutex;
		FAngelscriptEngine* Engine = nullptr;
	};

	struct FNativeModuleFunctionBindingRegistrationState : TSharedFromThis<FNativeModuleFunctionBindingRegistrationState>
	{
		explicit FNativeModuleFunctionBindingRegistrationState(IModularFeature* InFeature)
			: Feature(InFeature)
			, bRegistered(true)
		{
		}

		FCriticalSection Mutex;
		IModularFeature* Feature = nullptr;
		TAtomic<bool> bRegistered;
		bool bValidationFailureReported = false;
		TMap<FAngelscriptEngine*, FNativeModuleFunctionBindingTargetState> TargetStates;
	};

	FCriticalSection GNativeModuleFunctionBindingStatesMutex;
	TMap<IModularFeature*, TSharedPtr<FNativeModuleFunctionBindingRegistrationState>> GNativeModuleFunctionBindingStates;
	TMap<FAngelscriptEngine*, TSharedPtr<FNativeModuleFunctionBindingTargetEngine>> GNativeModuleFunctionBindingTargetEngines;

	UClass* ResolveNativeModuleFunctionClassByName(const TCHAR* ModuleName, const TCHAR* ClassName)
	{
		if (ClassName == nullptr)
		{
			return nullptr;
		}

		if (ModuleName != nullptr)
		{
			const FString FullPath = FString::Printf(TEXT("/Script/%s.%s"), ModuleName, ClassName);
			if (UClass* Class = Cast<UClass>(StaticFindObject(UClass::StaticClass(), nullptr, *FullPath, EFindObjectFlags::ExactClass)))
			{
				return Class;
			}
		}

		return Cast<UClass>(StaticFindFirstObject(UClass::StaticClass(), ClassName, EFindFirstObjectOptions::ExactClass | EFindFirstObjectOptions::NativeFirst));
	}

	UClass* ResolveNativeModuleFunctionClass(const FAngelscriptNativeModuleFunctionBinding& Binding, const FAngelscriptNativeModuleFunctionBindingView& Reader)
	{
		if (UClass* Class = ResolveNativeModuleFunctionClassByName(Reader.ModuleName, Binding.ClassName))
		{
			return Class;
		}

		if (Binding.ClassName != nullptr && (Binding.ClassName[0] == TEXT('U') || Binding.ClassName[0] == TEXT('A')) && Binding.ClassName[1] != TEXT('\0'))
		{
			return ResolveNativeModuleFunctionClassByName(Reader.ModuleName, Binding.ClassName + 1);
		}

		return nullptr;
	}

	FGenericFuncPtr MakeNativeModuleFunctionBindingGenericFuncPtr()
	{
		asSFuncPtr ASFuncPtr = asFUNCTION(GAngelscriptNativeModuleFunctionBindingGenericThunk);
		FGenericFuncPtr FuncPtr;
		static_assert(sizeof(asSFuncPtr) == sizeof(FGenericFuncPtr), "FGenericFuncPtr must stay layout-compatible with asSFuncPtr.");
		FMemory::Memcpy(&FuncPtr, &ASFuncPtr, sizeof(FGenericFuncPtr));
		return FuncPtr;
	}

	bool IsValidNativeModuleFunctionBindingView(
		const FAngelscriptNativeModuleFunctionBindingView* Reader,
		const bool bLogFailure)
	{
		if (Reader == nullptr)
		{
			return false;
		}
		if (Reader->LayoutVersion != FAngelscriptNativeModuleFunctionBindingBridge::LayoutVersionExpected)
		{
			if (bLogFailure)
			{
				UE_LOG(Angelscript, Warning, TEXT("Native module function binding feature skipped because layout version 0x%08x does not match 0x%08x."), Reader->LayoutVersion, FAngelscriptNativeModuleFunctionBindingBridge::LayoutVersionExpected);
			}
			return false;
		}
		if (Reader->Count < 0 || (Reader->Count > 0 && Reader->Table == nullptr) || Reader->ModuleName == nullptr)
		{
			if (bLogFailure)
			{
				UE_LOG(Angelscript, Warning, TEXT("Native module function binding feature skipped because its payload is malformed."));
			}
			return false;
		}
		return true;
	}

	void RemoveInjectedNativeModuleFunctionBindings(
		FAngelscriptBinds& Binds,
		FNativeModuleFunctionBindingTargetState& TargetState)
	{
		check(IsInGameThread());

		TMap<UClass*, TMap<FString, FAngelscriptFunctionBinding>>& ClassFunctionBindings =
			Binds.GetTargetBindState().ClassFunctionBindings;
		for (const FInjectedNativeModuleFunctionBinding& InjectedBinding : TargetState.InjectedBindings)
		{
			if (InjectedBinding.Class == nullptr)
			{
				continue;
			}

			TMap<FString, FAngelscriptFunctionBinding>* FunctionMap = ClassFunctionBindings.Find(InjectedBinding.Class);
			if (FunctionMap == nullptr)
			{
				continue;
			}

			FAngelscriptFunctionBinding* ExistingBinding = FunctionMap->Find(InjectedBinding.FunctionName);
			if (ExistingBinding != nullptr && ExistingBinding->UserData == InjectedBinding.UserData)
			{
				FunctionMap->Remove(InjectedBinding.FunctionName);
				if (FunctionMap->Num() == 0)
				{
					ClassFunctionBindings.Remove(InjectedBinding.Class);
				}
			}
		}
		TargetState.InjectedBindings.Empty();
	}

	TArray<TSharedPtr<FNativeModuleFunctionBindingTargetEngine>> SnapshotNativeModuleFunctionBindingTargetEngines()
	{
		TArray<TSharedPtr<FNativeModuleFunctionBindingTargetEngine>> Targets;
		FScopeLock StatesLock(&GNativeModuleFunctionBindingStatesMutex);
		GNativeModuleFunctionBindingTargetEngines.GenerateValueArray(Targets);
		return Targets;
	}

	void RemoveInjectedNativeModuleFunctionBindingState(
		const TSharedPtr<FNativeModuleFunctionBindingRegistrationState>& State)
	{
		check(IsInGameThread());
		if (!State.IsValid())
		{
			return;
		}

		const TArray<TSharedPtr<FNativeModuleFunctionBindingTargetEngine>> Targets =
			SnapshotNativeModuleFunctionBindingTargetEngines();
		for (const TSharedPtr<FNativeModuleFunctionBindingTargetEngine>& Target : Targets)
		{
			if (!Target.IsValid())
			{
				continue;
			}

			FScopeLock TargetLock(&Target->Mutex);
			FAngelscriptEngine* Engine = Target->Engine;
			if (Engine == nullptr)
			{
				continue;
			}

			FScopeLock StateLock(&State->Mutex);
			FNativeModuleFunctionBindingTargetState* TargetState = State->TargetStates.Find(Engine);
			if (TargetState == nullptr)
			{
				continue;
			}

			FAngelscriptBinds Binds(*Engine);
			RemoveInjectedNativeModuleFunctionBindings(Binds, *TargetState);
			State->TargetStates.Remove(Engine);
		}

		FScopeLock StateLock(&State->Mutex);
		State->TargetStates.Empty();
	}

	void InjectNativeModuleFunctionBindingStateIntoTarget(
		const TSharedPtr<FNativeModuleFunctionBindingRegistrationState>& State,
		const TSharedPtr<FNativeModuleFunctionBindingTargetEngine>& Target)
	{
		check(IsInGameThread());
		if (!State.IsValid() || !Target.IsValid())
		{
			return;
		}

		FScopeLock TargetLock(&Target->Mutex);
		FAngelscriptEngine* Engine = Target->Engine;
		if (Engine == nullptr)
		{
			return;
		}

		FScopeLock StateLock(&State->Mutex);
		if (!State->bRegistered.Load() || State->Feature == nullptr)
		{
			return;
		}

		const FAngelscriptNativeModuleFunctionBindingView* Reader = reinterpret_cast<const FAngelscriptNativeModuleFunctionBindingView*>(State->Feature);
		FNativeModuleFunctionBindingTargetState& TargetState = State->TargetStates.FindOrAdd(Engine);
		if (!IsValidNativeModuleFunctionBindingView(Reader, !State->bValidationFailureReported))
		{
			State->bValidationFailureReported = true;
			TargetState.PendingBindingIndices.Empty();
			TargetState.bHasPendingBindings = true;
			return;
		}

		if (!TargetState.bHasPendingBindings)
		{
			TargetState.PendingBindingIndices.Reserve(Reader->Count);
			for (int32 BindingIndex = 0; BindingIndex < Reader->Count; ++BindingIndex)
			{
				TargetState.PendingBindingIndices.Add(BindingIndex);
			}
			TargetState.bHasPendingBindings = true;
		}

		FAngelscriptBinds Binds(*Engine);
		TMap<UClass*, TMap<FString, FAngelscriptFunctionBinding>>& ClassFunctionBindings =
			Binds.GetTargetBindState().ClassFunctionBindings;
		TArray<int32> UnresolvedBindingIndices;
		for (int32 BindingIndex : TargetState.PendingBindingIndices)
		{
			if (BindingIndex < 0 || BindingIndex >= Reader->Count)
			{
				continue;
			}

			const FAngelscriptNativeModuleFunctionBinding& Binding = Reader->Table[BindingIndex];
			if (Binding.ClassName == nullptr || Binding.FunctionName == nullptr || Binding.Thunk == nullptr)
			{
				UE_LOG(Angelscript, Warning, TEXT("Native module function binding skipped because class, function, or thunk is null."));
				continue;
			}

			UClass* Class = ResolveNativeModuleFunctionClass(Binding, *Reader);
			if (Class == nullptr)
			{
				UnresolvedBindingIndices.Add(BindingIndex);
				continue;
			}

			TMap<FString, FAngelscriptFunctionBinding>* ExistingFunctionMap = ClassFunctionBindings.Find(Class);
			if (ExistingFunctionMap != nullptr && ExistingFunctionMap->Contains(Binding.FunctionName))
			{
				continue;
			}

			FAngelscriptFunctionBinding FunctionBinding;
			FunctionBinding.FunctionPointer = MakeNativeModuleFunctionBindingGenericFuncPtr();
			FunctionBinding.FunctionCaller = ASAutoCaller::FunctionCaller::Make();
			FunctionBinding.UserData = const_cast<FAngelscriptNativeModuleFunctionBinding*>(&Binding);
			FunctionBinding.bUsesGenericCall = true;
			FunctionBinding.Origin =
				EAngelscriptFunctionBindingOrigin::NativeModule;
			Binds.RegisterFunctionBindingForTarget(Class, Binding.FunctionName, FunctionBinding);
			TargetState.InjectedBindings.Add({ Class, Binding.FunctionName, FunctionBinding.UserData });
		}
		TargetState.PendingBindingIndices = MoveTemp(UnresolvedBindingIndices);
	}

	void InjectNativeModuleFunctionBindingState(const TSharedPtr<FNativeModuleFunctionBindingRegistrationState>& State)
	{
		if (!State.IsValid() || bNativeModuleFunctionBindingShuttingDown)
		{
			return;
		}

		if (!IsInGameThread())
		{
			AsyncTask(ENamedThreads::GameThread, [State]()
			{
				InjectNativeModuleFunctionBindingState(State);
			});
			return;
		}

		const TArray<TSharedPtr<FNativeModuleFunctionBindingTargetEngine>> Targets =
			SnapshotNativeModuleFunctionBindingTargetEngines();
		for (const TSharedPtr<FNativeModuleFunctionBindingTargetEngine>& Target : Targets)
		{
			InjectNativeModuleFunctionBindingStateIntoTarget(State, Target);
		}
	}

	void QueueNativeModuleFunctionBindingFeature(IModularFeature* Feature)
	{
		if (Feature == nullptr || bNativeModuleFunctionBindingShuttingDown)
		{
			return;
		}

		TSharedPtr<FNativeModuleFunctionBindingRegistrationState> State;
		{
			FScopeLock StatesLock(&GNativeModuleFunctionBindingStatesMutex);
			State = GNativeModuleFunctionBindingStates.FindRef(Feature);
			if (!State.IsValid())
			{
				State = MakeShared<FNativeModuleFunctionBindingRegistrationState>(Feature);
				GNativeModuleFunctionBindingStates.Add(Feature, State);
			}
		}
		InjectNativeModuleFunctionBindingState(State);
	}

	void OnNativeModuleFunctionBindingRegistered(const FName& Type, IModularFeature* Feature)
	{
		if (Type == FAngelscriptNativeModuleFunctionBindingBridge::FeatureName())
		{
			QueueNativeModuleFunctionBindingFeature(Feature);
		}
	}

	void OnNativeModuleFunctionBindingUnregistered(const FName& Type, IModularFeature* Feature)
	{
		if (Type != FAngelscriptNativeModuleFunctionBindingBridge::FeatureName())
		{
			return;
		}

		TSharedPtr<FNativeModuleFunctionBindingRegistrationState> State;
		{
			FScopeLock StatesLock(&GNativeModuleFunctionBindingStatesMutex);
			State = GNativeModuleFunctionBindingStates.FindRef(Feature);
			GNativeModuleFunctionBindingStates.Remove(Feature);
		}
		if (!State.IsValid())
		{
			return;
		}

		if (IsInGameThread())
		{
			{
				FScopeLock StateLock(&State->Mutex);
				State->bRegistered.Store(false);
				State->Feature = nullptr;
			}
			RemoveInjectedNativeModuleFunctionBindingState(State);
			return;
		}

		{
			FScopeLock StateLock(&State->Mutex);
			State->bRegistered.Store(false);
			State->Feature = nullptr;
		}
		AsyncTask(ENamedThreads::GameThread, [State]()
		{
			RemoveInjectedNativeModuleFunctionBindingState(State);
		});
	}

	void OnNativeModuleFunctionBindingObjectConstructed(UObject* Object)
	{
		if (Object == nullptr || !Object->IsA(UClass::StaticClass()) || bNativeModuleFunctionBindingShuttingDown)
		{
			return;
		}

		TArray<TSharedPtr<FNativeModuleFunctionBindingRegistrationState>> States;
		{
			FScopeLock StatesLock(&GNativeModuleFunctionBindingStatesMutex);
			GNativeModuleFunctionBindingStates.GenerateValueArray(States);
		}
		for (const TSharedPtr<FNativeModuleFunctionBindingRegistrationState>& State : States)
		{
			InjectNativeModuleFunctionBindingState(State);
		}
	}

	TSharedPtr<FNativeModuleFunctionBindingTargetEngine> RegisterNativeModuleFunctionBindingTarget(
		FAngelscriptBinds& Binds)
	{
		if (bNativeModuleFunctionBindingShuttingDown)
		{
			return nullptr;
		}

		FAngelscriptEngine& Engine = Binds.GetTargetEngine();
		FScopeLock StatesLock(&GNativeModuleFunctionBindingStatesMutex);
		TSharedPtr<FNativeModuleFunctionBindingTargetEngine>& Target =
			GNativeModuleFunctionBindingTargetEngines.FindOrAdd(&Engine);
		if (!Target.IsValid())
		{
			Target = MakeShared<FNativeModuleFunctionBindingTargetEngine>(Engine);
		}
		return Target;
	}

	void RequeueRegisteredNativeModuleFunctionBindings(
		const TSharedPtr<FNativeModuleFunctionBindingTargetEngine>& Target)
	{
		if (!Target.IsValid())
		{
			return;
		}

		TArray<TSharedPtr<FNativeModuleFunctionBindingRegistrationState>> States;
		{
			FScopeLock StatesLock(&GNativeModuleFunctionBindingStatesMutex);
			GNativeModuleFunctionBindingStates.GenerateValueArray(States);
		}

		FScopeLock TargetLock(&Target->Mutex);
		FAngelscriptEngine* Engine = Target->Engine;
		if (Engine == nullptr)
		{
			return;
		}
		for (const TSharedPtr<FNativeModuleFunctionBindingRegistrationState>& State : States)
		{
			if (!State.IsValid())
			{
				continue;
			}
			FScopeLock StateLock(&State->Mutex);
			if (State->bRegistered.Load())
			{
				if (FNativeModuleFunctionBindingTargetState* TargetState = State->TargetStates.Find(Engine))
				{
					TargetState->bHasPendingBindings = false;
					TargetState->PendingBindingIndices.Empty();
				}
			}
		}
	}

	void UnsubscribeNativeModuleFunctionBinding()
	{
		bNativeModuleFunctionBindingShuttingDown = true;
		if (GNativeModuleFunctionBindingRegisteredHandle.IsValid())
		{
			IModularFeatures::Get().OnModularFeatureRegistered().Remove(GNativeModuleFunctionBindingRegisteredHandle);
			GNativeModuleFunctionBindingRegisteredHandle.Reset();
		}
		if (GNativeModuleFunctionBindingUnregisteredHandle.IsValid())
		{
			IModularFeatures::Get().OnModularFeatureUnregistered().Remove(GNativeModuleFunctionBindingUnregisteredHandle);
			GNativeModuleFunctionBindingUnregisteredHandle.Reset();
		}
		if (GNativeModuleFunctionBindingObjectConstructedHandle.IsValid())
		{
			FCoreUObjectDelegates::OnObjectConstructed.Remove(GNativeModuleFunctionBindingObjectConstructedHandle);
			GNativeModuleFunctionBindingObjectConstructedHandle.Reset();
		}
	}

	void EnsureNativeModuleFunctionBindingSubscription()
	{
		if (!GNativeModuleFunctionBindingRegisteredHandle.IsValid())
		{
			GNativeModuleFunctionBindingRegisteredHandle = IModularFeatures::Get().OnModularFeatureRegistered().AddStatic(&OnNativeModuleFunctionBindingRegistered);
		}
		if (!GNativeModuleFunctionBindingUnregisteredHandle.IsValid())
		{
			GNativeModuleFunctionBindingUnregisteredHandle = IModularFeatures::Get().OnModularFeatureUnregistered().AddStatic(&OnNativeModuleFunctionBindingUnregistered);
		}
		if (!GNativeModuleFunctionBindingObjectConstructedHandle.IsValid())
		{
			GNativeModuleFunctionBindingObjectConstructedHandle = FCoreUObjectDelegates::OnObjectConstructed.AddStatic(&OnNativeModuleFunctionBindingObjectConstructed);
		}
		if (!GNativeModuleFunctionBindingPreExitHandle.IsValid())
		{
			GNativeModuleFunctionBindingPreExitHandle = FCoreDelegates::OnPreExit.AddStatic(&UnsubscribeNativeModuleFunctionBinding);
		}
	}

	void UnregisterNativeModuleFunctionBindingTarget(FAngelscriptEngine& Engine)
	{
		TSharedPtr<FNativeModuleFunctionBindingTargetEngine> Target;
		TArray<TSharedPtr<FNativeModuleFunctionBindingRegistrationState>> States;
		{
			FScopeLock StatesLock(&GNativeModuleFunctionBindingStatesMutex);
			Target = GNativeModuleFunctionBindingTargetEngines.FindRef(&Engine);
			GNativeModuleFunctionBindingTargetEngines.Remove(&Engine);
			GNativeModuleFunctionBindingStates.GenerateValueArray(States);
		}
		if (!Target.IsValid())
		{
			return;
		}

		FScopeLock TargetLock(&Target->Mutex);
		if (Target->Engine != &Engine)
		{
			Target->Engine = nullptr;
			return;
		}

		const bool bCanMutateTargetBindings = IsInGameThread() && Engine.GetBindState() != nullptr;
		TUniquePtr<FAngelscriptBinds> Binds;
		if (bCanMutateTargetBindings)
		{
			Binds = MakeUnique<FAngelscriptBinds>(Engine);
		}
		for (const TSharedPtr<FNativeModuleFunctionBindingRegistrationState>& State : States)
		{
			if (!State.IsValid())
			{
				continue;
			}

			FScopeLock StateLock(&State->Mutex);
			FNativeModuleFunctionBindingTargetState* TargetState = State->TargetStates.Find(&Engine);
			if (TargetState == nullptr)
			{
				continue;
			}

			if (Binds.IsValid())
			{
				RemoveInjectedNativeModuleFunctionBindings(*Binds, *TargetState);
			}
			State->TargetStates.Remove(&Engine);
		}
		Target->Engine = nullptr;
	}

	void RegisterExistingNativeModuleFunctionBindings(FAngelscriptBinds& Binds)
	{
		EnsureNativeModuleFunctionBindingSubscription();
		const TSharedPtr<FNativeModuleFunctionBindingTargetEngine> Target =
			RegisterNativeModuleFunctionBindingTarget(Binds);
		RequeueRegisteredNativeModuleFunctionBindings(Target);

		TArray<IModularFeature*> Features = IModularFeatures::Get().GetModularFeatureImplementations<IModularFeature>(
			FAngelscriptNativeModuleFunctionBindingBridge::FeatureName());
		for (IModularFeature* Feature : Features)
		{
			TSharedPtr<FNativeModuleFunctionBindingRegistrationState> State;
			{
				FScopeLock StatesLock(&GNativeModuleFunctionBindingStatesMutex);
				State = GNativeModuleFunctionBindingStates.FindRef(Feature);
				if (!State.IsValid())
				{
					State = MakeShared<FNativeModuleFunctionBindingRegistrationState>(Feature);
					GNativeModuleFunctionBindingStates.Add(Feature, State);
				}
			}

			if (IsInGameThread())
			{
				InjectNativeModuleFunctionBindingStateIntoTarget(State, Target);
				continue;
			}

			// GeneratedBindings must finish before ReflectionBindings starts. The
			// primary initialization path pumps GameThread tasks while this worker
			// waits, so marshal the UObject-facing lookup synchronously here.
			TAtomic<bool> bInjectionDone(false);
			AsyncTask(ENamedThreads::GameThread, [State, Target, &bInjectionDone]()
			{
				InjectNativeModuleFunctionBindingStateIntoTarget(State, Target);
				bInjectionDone.Store(true);
			});
			while (!bInjectionDone.Load())
			{
				FPlatformProcess::Sleep(0.001f);
			}
		}
	}

	void BindNativeModuleFunctionBindings(FAngelscriptBinds& Binds)
	{
		RegisterExistingNativeModuleFunctionBindings(Binds);
	}

	AS_FORCE_LINK const FAngelscriptBind Bind_AS_NativeModuleFunctionBinding(
		TEXT("NativeModuleFunctionBinding.GeneratedTransport"),
		EAngelscriptBindPhase::GeneratedBindings,
		&BindNativeModuleFunctionBindings);
}

void GAngelscriptNativeModuleFunctionBindingUnregisterEngine(FAngelscriptEngine& Engine)
{
	UnregisterNativeModuleFunctionBindingTarget(Engine);
}

#if WITH_DEV_AUTOMATION_TESTS
void GAngelscriptNativeModuleFunctionBindingEnsureRegisteredForTesting()
{
	EnsureNativeModuleFunctionBindingSubscription();
}

int32 GAngelscriptNativeModuleFunctionBindingBindGlobalFunctionForTesting(
	FAngelscriptEngine& Engine,
	const ANSICHAR* Signature,
	FAngelscriptNativeModuleFunctionBinding* Binding)
{
	FAngelscriptBinds Binds(Engine);
	return Binds.BindGlobalFunctionDirectForTarget(
		Signature,
		asFUNCTION(GAngelscriptNativeModuleFunctionBindingGenericThunk),
		asCALL_GENERIC,
		ASAutoCaller::FunctionCaller::Make(),
		Binding);
}
#endif

void GAngelscriptNativeModuleFunctionBindingGenericThunk(asIScriptGeneric* Generic)
{
	if (Generic == nullptr)
	{
		return;
	}

	asIScriptFunction* Function = Generic->GetFunction();
	if (Function == nullptr)
	{
		return;
	}

	const FAngelscriptNativeModuleFunctionBinding* Binding = static_cast<const FAngelscriptNativeModuleFunctionBinding*>(Function->GetUserData());
	if (Binding == nullptr || Binding->Thunk == nullptr)
	{
		return;
	}

	TArray<void*, TInlineAllocator<8>> Args;
	Args.Reserve(Binding->ArgCount);
	for (uint16 ArgumentIndex = 0; ArgumentIndex < Binding->ArgCount; ++ArgumentIndex)
	{
		Args.Add(Generic->GetAddressOfArg(ArgumentIndex));
	}

	UObject* Self = (Binding->Flags & FAngelscriptNativeModuleFunctionBindingBridge::FlagStatic) != 0
		? nullptr
		: static_cast<UObject*>(Generic->GetObject());
	FAngelscriptNativeModuleFunctionBindingCallFrame Frame = {
		Args.GetData(),
		Binding->ArgCount,
		0,
		Binding->RetSize > 0 ? Generic->GetAddressOfReturnLocation() : nullptr,
		Self,
		nullptr,
		Binding->Flags,
		0
	};
	Binding->Thunk(Self, &Frame);
}

#endif
