#if WITH_ANGELSCRIPT_MODULE_LOCAL_BINDINGS

#include "Core/AngelscriptBinds.h"
#include "Core/AngelscriptEngine.h"
#include "Core/FunctionCallers.h"
#include "UHT/AngelscriptCrossModuleFunctionBindings.h"

#include "Async/Async.h"
#include "Containers/Array.h"
#include "Features/IModularFeatures.h"
#include "Misc/CoreDelegates.h"
#include "UObject/FindObjectFlags.h"
#include "UObject/UObjectGlobals.h"

void GAngelscriptCrossModuleGenericHook(asIScriptGeneric* Generic);
#if WITH_DEV_AUTOMATION_TESTS
ANGELSCRIPTRUNTIME_API void GAngelscriptCrossModuleEnsureRegisteredForTesting();
ANGELSCRIPTRUNTIME_API int32 GAngelscriptCrossModuleBindGlobalFunctionForTesting(const ANSICHAR* Signature, FAngelscriptCrossModuleBinding* Binding);
#endif

namespace
{
	FDelegateHandle GCrossoverModuleFeatureRegisteredHandle;
	FDelegateHandle GCrossoverModulePreExitHandle;
	bool GCrossoverModuleShuttingDown = false;

	UClass* ResolveCrossModuleClassByName(const TCHAR* ModuleName, const TCHAR* ClassName)
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

	UClass* ResolveCrossModuleClass(const FAngelscriptCrossModuleBinding& Binding, const FAngelscriptCrossModuleBindingFeatureReader& Reader)
	{
		if (UClass* Class = ResolveCrossModuleClassByName(Reader.ModuleName, Binding.ClassName))
		{
			return Class;
		}

		if (Binding.ClassName != nullptr && (Binding.ClassName[0] == TEXT('U') || Binding.ClassName[0] == TEXT('A')) && Binding.ClassName[1] != TEXT('\0'))
		{
			return ResolveCrossModuleClassByName(Reader.ModuleName, Binding.ClassName + 1);
		}

		return nullptr;
	}

	FGenericFuncPtr MakeCrossModuleGenericFuncPtr()
	{
		asSFuncPtr ASFuncPtr = asFUNCTION(GAngelscriptCrossModuleGenericHook);
		FGenericFuncPtr FuncPtr;
		static_assert(sizeof(asSFuncPtr) == sizeof(FGenericFuncPtr), "FGenericFuncPtr must stay layout-compatible with asSFuncPtr.");
		FMemory::Memcpy(&FuncPtr, &ASFuncPtr, sizeof(FGenericFuncPtr));
		return FuncPtr;
	}

	bool IsValidFeatureReader(const FAngelscriptCrossModuleBindingFeatureReader* Reader)
	{
		if (Reader == nullptr)
		{
			return false;
		}
		if (Reader->LayoutVersion != FAngelscriptCrossModuleFunctionBindings::LayoutVersionExpected)
		{
			UE_LOG(Angelscript, Warning, TEXT("Cross-module binding feature skipped because layout version 0x%08x does not match 0x%08x."), Reader->LayoutVersion, FAngelscriptCrossModuleFunctionBindings::LayoutVersionExpected);
			return false;
		}
		if (Reader->Count < 0 || (Reader->Count > 0 && Reader->Table == nullptr) || Reader->ModuleName == nullptr)
		{
			UE_LOG(Angelscript, Warning, TEXT("Cross-module binding feature skipped because its payload is malformed."));
			return false;
		}
		return true;
	}

	void InjectCrossModuleFeature(IModularFeature* Feature)
	{
		const FAngelscriptCrossModuleBindingFeatureReader* Reader = reinterpret_cast<const FAngelscriptCrossModuleBindingFeatureReader*>(Feature);
		if (!IsValidFeatureReader(Reader))
		{
			return;
		}

		for (int32 EntryIndex = 0; EntryIndex < Reader->Count; ++EntryIndex)
		{
			const FAngelscriptCrossModuleBinding& Binding = Reader->Table[EntryIndex];
			if (Binding.ClassName == nullptr || Binding.FunctionName == nullptr || Binding.Thunk == nullptr)
			{
				UE_LOG(Angelscript, Warning, TEXT("Cross-module binding entry skipped because class, function, or thunk is null."));
				continue;
			}

			UClass* Class = ResolveCrossModuleClass(Binding, *Reader);
			if (Class == nullptr)
			{
				UE_LOG(Angelscript, Warning, TEXT("Cross-module binding entry skipped because class '%s.%s' could not be resolved."), Reader->ModuleName, Binding.ClassName != nullptr ? Binding.ClassName : TEXT("<null>"));
				continue;
			}

			FAngelscriptFunctionBinding FunctionBinding;
			FunctionBinding.FunctionPointer = MakeCrossModuleGenericFuncPtr();
			FunctionBinding.FunctionCaller = ASAutoCaller::FunctionCaller::Make();
			FunctionBinding.UserData = const_cast<FAngelscriptCrossModuleBinding*>(&Binding);
			FunctionBinding.bUsesGenericCall = true;
			FAngelscriptBinds::RegisterFunctionBinding(Class, Binding.FunctionName, FunctionBinding);
		}
	}

	void InjectCrossModuleFeatureOnGameThread(IModularFeature* Feature)
	{
		if (Feature == nullptr || GCrossoverModuleShuttingDown)
		{
			return;
		}

		if (IsInGameThread())
		{
			InjectCrossModuleFeature(Feature);
			return;
		}

		AsyncTask(ENamedThreads::GameThread, [Feature]()
		{
			if (!GCrossoverModuleShuttingDown)
			{
				InjectCrossModuleFeature(Feature);
			}
		});
	}

	void OnCrossModuleFeatureRegistered(const FName& Type, IModularFeature* Feature)
	{
		if (Type == FAngelscriptCrossModuleFunctionBindings::FeatureName())
		{
			InjectCrossModuleFeatureOnGameThread(Feature);
		}
	}

	void UnsubscribeCrossModuleBindings()
	{
		GCrossoverModuleShuttingDown = true;
		if (GCrossoverModuleFeatureRegisteredHandle.IsValid())
		{
			IModularFeatures::Get().OnModularFeatureRegistered().Remove(GCrossoverModuleFeatureRegisteredHandle);
			GCrossoverModuleFeatureRegisteredHandle.Reset();
		}
	}

	void EnsureCrossModuleFeatureSubscription()
	{
		if (!GCrossoverModuleFeatureRegisteredHandle.IsValid())
		{
			GCrossoverModuleFeatureRegisteredHandle = IModularFeatures::Get().OnModularFeatureRegistered().AddStatic(&OnCrossModuleFeatureRegistered);
		}
		if (!GCrossoverModulePreExitHandle.IsValid())
		{
			GCrossoverModulePreExitHandle = FCoreDelegates::OnPreExit.AddStatic(&UnsubscribeCrossModuleBindings);
		}
	}

	void RegisterExistingCrossModuleFeatures()
	{
		EnsureCrossModuleFeatureSubscription();

		TArray<IModularFeature*> Features = IModularFeatures::Get().GetModularFeatureImplementations<IModularFeature>(
			FAngelscriptCrossModuleFunctionBindings::FeatureName());
		for (IModularFeature* Feature : Features)
		{
			InjectCrossModuleFeatureOnGameThread(Feature);
		}
	}

	AS_FORCE_LINK const FAngelscriptBinds::FBind Bind_AS_CrossModuleDirect(
		FName(TEXT("Bind_CrossModuleDirect")),
		(int32)FAngelscriptBinds::EOrder::Late + 60,
		[]()
		{
			RegisterExistingCrossModuleFeatures();
		});
}

#if WITH_DEV_AUTOMATION_TESTS
void GAngelscriptCrossModuleEnsureRegisteredForTesting()
{
	EnsureCrossModuleFeatureSubscription();
}

int32 GAngelscriptCrossModuleBindGlobalFunctionForTesting(const ANSICHAR* Signature, FAngelscriptCrossModuleBinding* Binding)
{
	return FAngelscriptBinds::BindGlobalFunctionDirect(
		Signature,
		asFUNCTION(GAngelscriptCrossModuleGenericHook),
		asCALL_GENERIC,
		ASAutoCaller::FunctionCaller::Make(),
		Binding);
}
#endif

void GAngelscriptCrossModuleGenericHook(asIScriptGeneric* Generic)
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

	const FAngelscriptCrossModuleBinding* Binding = static_cast<const FAngelscriptCrossModuleBinding*>(Function->GetUserData());
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

	UObject* Self = (Binding->Flags & FAngelscriptCrossModuleFunctionBindings::FlagStatic) != 0
		? nullptr
		: static_cast<UObject*>(Generic->GetObject());
	FAngelscriptCrossModuleCallFrame Frame = {
		Args.GetData(),
		Binding->ArgCount,
		0,
		Binding->RetSize > 0 ? Generic->GetAddressOfReturnLocation() : nullptr,
		Self,
		nullptr,
		Binding->Flags,
		0
	};
	if ((Binding->Flags & FAngelscriptCrossModuleFunctionBindings::FlagStatic) != 0)
	{
		Frame.ScriptSelf = nullptr;
	}

	Binding->Thunk(Self, &Frame);
}

#endif
