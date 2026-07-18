#if WITH_ANGELSCRIPT_NATIVE_MODULE_FUNCTION_ADDRESS

#include "Core/AngelscriptBinds.h"
#include "Core/AngelscriptEngine.h"
#include "Core/FunctionCallers.h"
#include "FunctionBinding/NativeModuleFunctionBindingBridge.h"

#include "Async/Async.h"
#include "Containers/Array.h"
#include "Features/IModularFeatures.h"
#include "Misc/CoreDelegates.h"
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
	FDelegateHandle GNativeModuleFunctionBindingPreExitHandle;
	bool bNativeModuleFunctionBindingShuttingDown = false;

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

	void InjectNativeModuleFunctionBindingFeature(IModularFeature* Feature)
	{
		const FAngelscriptNativeModuleFunctionBindingView* Reader = reinterpret_cast<const FAngelscriptNativeModuleFunctionBindingView*>(Feature);
		if (!IsValidNativeModuleFunctionBindingView(Reader))
		{
			return;
		}

		for (int32 BindingIndex = 0; BindingIndex < Reader->Count; ++BindingIndex)
		{
			const FAngelscriptNativeModuleFunctionBinding& Binding = Reader->Table[BindingIndex];
			if (Binding.ClassName == nullptr || Binding.FunctionName == nullptr || Binding.Thunk == nullptr)
			{
				UE_LOG(Angelscript, Warning, TEXT("Native module function binding skipped because class, function, or thunk is null."));
				continue;
			}

			UClass* Class = ResolveNativeModuleFunctionClass(Binding, *Reader);
			if (Class == nullptr)
			{
				UE_LOG(Angelscript, Warning, TEXT("Native module function binding skipped because class '%s.%s' could not be resolved."), Reader->ModuleName, Binding.ClassName);
				continue;
			}

			FAngelscriptFunctionBinding FunctionBinding;
			FunctionBinding.FunctionPointer = MakeNativeModuleFunctionBindingGenericFuncPtr();
			FunctionBinding.FunctionCaller = ASAutoCaller::FunctionCaller::Make();
			FunctionBinding.UserData = const_cast<FAngelscriptNativeModuleFunctionBinding*>(&Binding);
			FunctionBinding.bUsesGenericCall = true;
			FAngelscriptBinds::RegisterFunctionBinding(Class, Binding.FunctionName, FunctionBinding);
		}
	}

	void InjectNativeModuleFunctionBindingFeatureOnGameThread(IModularFeature* Feature)
	{
		if (Feature == nullptr || bNativeModuleFunctionBindingShuttingDown)
		{
			return;
		}

		if (IsInGameThread())
		{
			InjectNativeModuleFunctionBindingFeature(Feature);
			return;
		}

		AsyncTask(ENamedThreads::GameThread, [Feature]()
		{
			if (!bNativeModuleFunctionBindingShuttingDown)
			{
				InjectNativeModuleFunctionBindingFeature(Feature);
			}
		});
	}

	void OnNativeModuleFunctionBindingRegistered(const FName& Type, IModularFeature* Feature)
	{
		if (Type == FAngelscriptNativeModuleFunctionBindingBridge::FeatureName())
		{
			InjectNativeModuleFunctionBindingFeatureOnGameThread(Feature);
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
	}

	void EnsureNativeModuleFunctionBindingSubscription()
	{
		if (!GNativeModuleFunctionBindingRegisteredHandle.IsValid())
		{
			GNativeModuleFunctionBindingRegisteredHandle = IModularFeatures::Get().OnModularFeatureRegistered().AddStatic(&OnNativeModuleFunctionBindingRegistered);
		}
		if (!GNativeModuleFunctionBindingPreExitHandle.IsValid())
		{
			GNativeModuleFunctionBindingPreExitHandle = FCoreDelegates::OnPreExit.AddStatic(&UnsubscribeNativeModuleFunctionBinding);
		}
	}

	void RegisterExistingNativeModuleFunctionBindings()
	{
		EnsureNativeModuleFunctionBindingSubscription();

		TArray<IModularFeature*> Features = IModularFeatures::Get().GetModularFeatureImplementations<IModularFeature>(
			FAngelscriptNativeModuleFunctionBindingBridge::FeatureName());
		for (IModularFeature* Feature : Features)
		{
			InjectNativeModuleFunctionBindingFeatureOnGameThread(Feature);
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
