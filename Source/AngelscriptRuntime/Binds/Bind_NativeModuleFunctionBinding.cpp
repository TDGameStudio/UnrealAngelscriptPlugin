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

void GAngelscriptNativeModuleFunctionBindingGenericThunk(asIScriptGeneric* Generic);
#if WITH_DEV_AUTOMATION_TESTS
ANGELSCRIPTRUNTIME_API void GAngelscriptNativeModuleFunctionBindingEnsureRegisteredForTesting();
ANGELSCRIPTRUNTIME_API int32 GAngelscriptNativeModuleFunctionBindingBindGlobalFunctionForTesting(const ANSICHAR* Signature, FAngelscriptNativeModuleFunctionBinding* Binding);
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
		bool bHasPendingBindings = false;
		TArray<int32> PendingBindingIndices;
		TArray<FInjectedNativeModuleFunctionBinding> InjectedBindings;
	};

	FCriticalSection GNativeModuleFunctionBindingStatesMutex;
	TMap<IModularFeature*, TSharedPtr<FNativeModuleFunctionBindingRegistrationState>> GNativeModuleFunctionBindingStates;

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

	bool IsValidNativeModuleFunctionBindingView(const FAngelscriptNativeModuleFunctionBindingView* Reader)
	{
		if (Reader == nullptr)
		{
			return false;
		}
		if (Reader->LayoutVersion != FAngelscriptNativeModuleFunctionBindingBridge::LayoutVersionExpected)
		{
			UE_LOG(Angelscript, Warning, TEXT("Native module function binding feature skipped because layout version 0x%08x does not match 0x%08x."), Reader->LayoutVersion, FAngelscriptNativeModuleFunctionBindingBridge::LayoutVersionExpected);
			return false;
		}
		if (Reader->Count < 0 || (Reader->Count > 0 && Reader->Table == nullptr) || Reader->ModuleName == nullptr)
		{
			UE_LOG(Angelscript, Warning, TEXT("Native module function binding feature skipped because its payload is malformed."));
			return false;
		}
		return true;
	}

	void RemoveInjectedNativeModuleFunctionBindings(FNativeModuleFunctionBindingRegistrationState& State)
	{
		check(IsInGameThread());

		TMap<UClass*, TMap<FString, FAngelscriptFunctionBinding>>& ClassFunctionBindings = FAngelscriptBinds::GetClassFunctionBindings();
		for (const FInjectedNativeModuleFunctionBinding& InjectedBinding : State.InjectedBindings)
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
		State.InjectedBindings.Empty();
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

		FScopeLock StateLock(&State->Mutex);
		if (!State->bRegistered.Load() || State->Feature == nullptr)
		{
			return;
		}

		const FAngelscriptNativeModuleFunctionBindingView* Reader = reinterpret_cast<const FAngelscriptNativeModuleFunctionBindingView*>(State->Feature);
		if (!IsValidNativeModuleFunctionBindingView(Reader))
		{
			State->PendingBindingIndices.Empty();
			State->bHasPendingBindings = true;
			return;
		}

		if (!State->bHasPendingBindings)
		{
			State->PendingBindingIndices.Reserve(Reader->Count);
			for (int32 BindingIndex = 0; BindingIndex < Reader->Count; ++BindingIndex)
			{
				State->PendingBindingIndices.Add(BindingIndex);
			}
			State->bHasPendingBindings = true;
		}

		TArray<int32> UnresolvedBindingIndices;
		for (int32 BindingIndex : State->PendingBindingIndices)
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

			TMap<FString, FAngelscriptFunctionBinding>* ExistingFunctionMap = FAngelscriptBinds::GetClassFunctionBindings().Find(Class);
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
			FAngelscriptBinds::RegisterFunctionBinding(Class, Binding.FunctionName, FunctionBinding);
			State->InjectedBindings.Add({ Class, Binding.FunctionName, FunctionBinding.UserData });
		}
		State->PendingBindingIndices = MoveTemp(UnresolvedBindingIndices);
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
			FScopeLock StateLock(&State->Mutex);
			State->bRegistered.Store(false);
			RemoveInjectedNativeModuleFunctionBindings(*State);
			State->Feature = nullptr;
			State->PendingBindingIndices.Empty();
			return;
		}

		{
			FScopeLock StateLock(&State->Mutex);
			State->bRegistered.Store(false);
			State->Feature = nullptr;
		}
		AsyncTask(ENamedThreads::GameThread, [State]()
		{
			FScopeLock StateLock(&State->Mutex);
			RemoveInjectedNativeModuleFunctionBindings(*State);
			State->PendingBindingIndices.Empty();
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

	void RequeueRegisteredNativeModuleFunctionBindings()
	{
		TArray<TSharedPtr<FNativeModuleFunctionBindingRegistrationState>> States;
		{
			FScopeLock StatesLock(&GNativeModuleFunctionBindingStatesMutex);
			GNativeModuleFunctionBindingStates.GenerateValueArray(States);
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
				State->bHasPendingBindings = false;
				State->PendingBindingIndices.Empty();
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

	void RegisterExistingNativeModuleFunctionBindings()
	{
		EnsureNativeModuleFunctionBindingSubscription();
		RequeueRegisteredNativeModuleFunctionBindings();

		TArray<IModularFeature*> Features = IModularFeatures::Get().GetModularFeatureImplementations<IModularFeature>(
			FAngelscriptNativeModuleFunctionBindingBridge::FeatureName());
		for (IModularFeature* Feature : Features)
		{
			QueueNativeModuleFunctionBindingFeature(Feature);
		}
	}

	AS_FORCE_LINK const FAngelscriptBinds::FBind Bind_AS_NativeModuleFunctionBinding(
		FName(TEXT("Bind_NativeModuleFunctionBinding")),
		(int32)FAngelscriptBinds::EOrder::Late + 60,
		[]()
		{
			RegisterExistingNativeModuleFunctionBindings();
		});
}

#if WITH_DEV_AUTOMATION_TESTS
void GAngelscriptNativeModuleFunctionBindingEnsureRegisteredForTesting()
{
	EnsureNativeModuleFunctionBindingSubscription();
}

int32 GAngelscriptNativeModuleFunctionBindingBindGlobalFunctionForTesting(const ANSICHAR* Signature, FAngelscriptNativeModuleFunctionBinding* Binding)
{
	return FAngelscriptBinds::BindGlobalFunctionDirect(
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
