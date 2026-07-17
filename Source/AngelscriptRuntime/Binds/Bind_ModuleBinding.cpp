#if WITH_ANGELSCRIPT_MODULE_BINDINGS

#include "Core/AngelscriptBinds.h"
#include "Core/AngelscriptEngine.h"
#include "Core/FunctionCallers.h"
#include "Bindings/AngelscriptModuleBindingProtocol.h"

#include "Async/Async.h"
#include "Containers/Array.h"
#include "Features/IModularFeatures.h"
#include "Misc/CoreDelegates.h"
#include "UObject/FindObjectFlags.h"
#include "UObject/UObjectGlobals.h"

void GAngelscriptModuleBindingGenericHook(asIScriptGeneric* Generic);
#if WITH_DEV_AUTOMATION_TESTS
ANGELSCRIPTRUNTIME_API void GAngelscriptModuleBindingEnsureRegisteredForTesting();
ANGELSCRIPTRUNTIME_API int32 GAngelscriptModuleBindingBindGlobalFunctionForTesting(const ANSICHAR* Signature, FAngelscriptModuleBinding* Binding);
#endif

namespace
{
	FDelegateHandle GModuleBindingFeatureRegisteredHandle;
	FDelegateHandle GModuleBindingPreExitHandle;
	bool bModuleBindingShuttingDown = false;

	UClass* ResolveModuleBindingClassByName(const TCHAR* ModuleName, const TCHAR* ClassName)
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

	UClass* ResolveModuleBindingClass(const FAngelscriptModuleBinding& Binding, const FAngelscriptModuleBindingFeatureView& Reader)
	{
		if (UClass* Class = ResolveModuleBindingClassByName(Reader.ModuleName, Binding.ClassName))
		{
			return Class;
		}

		if (Binding.ClassName != nullptr && (Binding.ClassName[0] == TEXT('U') || Binding.ClassName[0] == TEXT('A')) && Binding.ClassName[1] != TEXT('\0'))
		{
			return ResolveModuleBindingClassByName(Reader.ModuleName, Binding.ClassName + 1);
		}

		return nullptr;
	}

	FGenericFuncPtr MakeModuleBindingGenericFuncPtr()
	{
		asSFuncPtr ASFuncPtr = asFUNCTION(GAngelscriptModuleBindingGenericHook);
		FGenericFuncPtr FuncPtr;
		static_assert(sizeof(asSFuncPtr) == sizeof(FGenericFuncPtr), "FGenericFuncPtr must stay layout-compatible with asSFuncPtr.");
		FMemory::Memcpy(&FuncPtr, &ASFuncPtr, sizeof(FGenericFuncPtr));
		return FuncPtr;
	}

	bool IsValidModuleBindingFeatureView(const FAngelscriptModuleBindingFeatureView* Reader)
	{
		if (Reader == nullptr)
		{
			return false;
		}
		if (Reader->LayoutVersion != FAngelscriptModuleBindingProtocol::LayoutVersionExpected)
		{
			UE_LOG(Angelscript, Warning, TEXT("Module binding feature skipped because layout version 0x%08x does not match 0x%08x."), Reader->LayoutVersion, FAngelscriptModuleBindingProtocol::LayoutVersionExpected);
			return false;
		}
		if (Reader->Count < 0 || (Reader->Count > 0 && Reader->Table == nullptr) || Reader->ModuleName == nullptr)
		{
			UE_LOG(Angelscript, Warning, TEXT("Module binding binding feature skipped because its payload is malformed."));
			return false;
		}
		return true;
	}

	void InjectModuleBindingFeature(IModularFeature* Feature)
	{
		const FAngelscriptModuleBindingFeatureView* Reader = reinterpret_cast<const FAngelscriptModuleBindingFeatureView*>(Feature);
		if (!IsValidModuleBindingFeatureView(Reader))
		{
			return;
		}

		for (int32 EntryIndex = 0; EntryIndex < Reader->Count; ++EntryIndex)
		{
			const FAngelscriptModuleBinding& Binding = Reader->Table[EntryIndex];
			if (Binding.ClassName == nullptr || Binding.FunctionName == nullptr || Binding.Thunk == nullptr)
			{
				UE_LOG(Angelscript, Warning, TEXT("Module binding entry skipped because class, function, or thunk is null."));
				continue;
			}

			UClass* Class = ResolveModuleBindingClass(Binding, *Reader);
			if (Class == nullptr)
			{
				UE_LOG(Angelscript, Warning, TEXT("Module binding entry skipped because class '%s.%s' could not be resolved."), Reader->ModuleName, Binding.ClassName != nullptr ? Binding.ClassName : TEXT("<null>"));
				continue;
			}

			FAngelscriptFunctionBinding FunctionBinding;
			FunctionBinding.FunctionPointer = MakeModuleBindingGenericFuncPtr();
			FunctionBinding.FunctionCaller = ASAutoCaller::FunctionCaller::Make();
			FunctionBinding.UserData = const_cast<FAngelscriptModuleBinding*>(&Binding);
			FunctionBinding.bUsesGenericCall = true;
			FAngelscriptBinds::RegisterFunctionBinding(Class, Binding.FunctionName, FunctionBinding);
		}
	}

	void InjectModuleBindingFeatureOnGameThread(IModularFeature* Feature)
	{
		if (Feature == nullptr || bModuleBindingShuttingDown)
		{
			return;
		}

		if (IsInGameThread())
		{
			InjectModuleBindingFeature(Feature);
			return;
		}

		AsyncTask(ENamedThreads::GameThread, [Feature]()
		{
			if (!bModuleBindingShuttingDown)
			{
				InjectModuleBindingFeature(Feature);
			}
		});
	}

	void OnModuleBindingFeatureRegistered(const FName& Type, IModularFeature* Feature)
	{
		if (Type == FAngelscriptModuleBindingProtocol::FeatureName())
		{
			InjectModuleBindingFeatureOnGameThread(Feature);
		}
	}

	void UnsubscribeModuleBindingFeature()
	{
		bModuleBindingShuttingDown = true;
		if (GModuleBindingFeatureRegisteredHandle.IsValid())
		{
			IModularFeatures::Get().OnModularFeatureRegistered().Remove(GModuleBindingFeatureRegisteredHandle);
			GModuleBindingFeatureRegisteredHandle.Reset();
		}
	}

	void EnsureModuleBindingFeatureSubscription()
	{
		if (!GModuleBindingFeatureRegisteredHandle.IsValid())
		{
			GModuleBindingFeatureRegisteredHandle = IModularFeatures::Get().OnModularFeatureRegistered().AddStatic(&OnModuleBindingFeatureRegistered);
		}
		if (!GModuleBindingPreExitHandle.IsValid())
		{
			GModuleBindingPreExitHandle = FCoreDelegates::OnPreExit.AddStatic(&UnsubscribeModuleBindingFeature);
		}
	}

	void RegisterExistingModuleBindingFeatures()
	{
		EnsureModuleBindingFeatureSubscription();

		TArray<IModularFeature*> Features = IModularFeatures::Get().GetModularFeatureImplementations<IModularFeature>(
			FAngelscriptModuleBindingProtocol::FeatureName());
		for (IModularFeature* Feature : Features)
		{
			InjectModuleBindingFeatureOnGameThread(Feature);
		}
	}

	AS_FORCE_LINK const FAngelscriptBinds::FBind Bind_AS_ModuleBinding(
		FName(TEXT("Bind_ModuleBinding")),
		(int32)FAngelscriptBinds::EOrder::Late + 60,
		[]()
		{
			RegisterExistingModuleBindingFeatures();
		});
}

#if WITH_DEV_AUTOMATION_TESTS
void GAngelscriptModuleBindingEnsureRegisteredForTesting()
{
	EnsureModuleBindingFeatureSubscription();
}

int32 GAngelscriptModuleBindingBindGlobalFunctionForTesting(const ANSICHAR* Signature, FAngelscriptModuleBinding* Binding)
{
	return FAngelscriptBinds::BindGlobalFunctionDirect(
		Signature,
		asFUNCTION(GAngelscriptModuleBindingGenericHook),
		asCALL_GENERIC,
		ASAutoCaller::FunctionCaller::Make(),
		Binding);
}
#endif

void GAngelscriptModuleBindingGenericHook(asIScriptGeneric* Generic)
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

	const FAngelscriptModuleBinding* Binding = static_cast<const FAngelscriptModuleBinding*>(Function->GetUserData());
	if (Binding == nullptr || Binding->Thunk == nullptr)
	{
		return;
	}

	TArray<void*, TInlineAllocator<8>> Args;
	Args.Reserve(Binding->ArgCount);
	for (uint16 ArgIndex = 0; ArgIndex < Binding->ArgCount; ++ArgIndex)
	{
		Args.Add(Generic->GetAddressOfArg(ArgIndex));
	}

	UObject* Self = (Binding->Flags & FAngelscriptModuleBindingProtocol::FlagStatic) != 0
		? nullptr
		: static_cast<UObject*>(Generic->GetObject());
	FAngelscriptModuleBindingCallFrame Frame = {
		Args.GetData(),
		Binding->ArgCount,
		0,
		Binding->RetSize > 0 ? Generic->GetAddressOfReturnLocation() : nullptr,
		Self,
		nullptr,
		Binding->Flags,
		0
	};
	if ((Binding->Flags & FAngelscriptModuleBindingProtocol::FlagStatic) != 0)
	{
		Frame.ScriptSelf = nullptr;
	}

	Binding->Thunk(Self, &Frame);
}

#endif
