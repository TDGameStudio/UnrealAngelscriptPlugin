#include "Core/AngelscriptBinds.h"
#include "Core/AngelscriptEngine.h"
#include "Core/FunctionCallers.h"
#include "UHT/AngelscriptCrossModuleBindings.h"

#include "Async/Async.h"
#include "Containers/Array.h"
#include "Features/IModularFeatures.h"
#include "Misc/CoreDelegates.h"
#include "UObject/FindObjectFlags.h"
#include "UObject/UObjectGlobals.h"

void GAngelscriptCrossModuleGenericHook(asIScriptGeneric* Generic);
#if WITH_DEV_AUTOMATION_TESTS
ANGELSCRIPTRUNTIME_API void GAngelscriptCrossModuleEnsureRegisteredForTesting();
ANGELSCRIPTRUNTIME_API int32 GAngelscriptCrossModuleBindGlobalFunctionForTesting(const ANSICHAR* Signature, FAngelscriptCrossModuleEntry* Entry);
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

	UClass* ResolveCrossModuleClass(const FAngelscriptCrossModuleEntry& Entry, const FAngelscriptCrossModuleFeatureReader& Reader)
	{
		if (UClass* Class = ResolveCrossModuleClassByName(Reader.ModuleName, Entry.ClassName))
		{
			return Class;
		}

		if (Entry.ClassName != nullptr && (Entry.ClassName[0] == TEXT('U') || Entry.ClassName[0] == TEXT('A')) && Entry.ClassName[1] != TEXT('\0'))
		{
			return ResolveCrossModuleClassByName(Reader.ModuleName, Entry.ClassName + 1);
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

	bool IsValidFeatureReader(const FAngelscriptCrossModuleFeatureReader* Reader)
	{
		if (Reader == nullptr)
		{
			return false;
		}
		if (Reader->LayoutVersion != FAngelscriptCrossModuleBindings::LayoutVersionExpected)
		{
			UE_LOG(Angelscript, Warning, TEXT("Cross-module binding feature skipped because layout version 0x%08x does not match 0x%08x."), Reader->LayoutVersion, FAngelscriptCrossModuleBindings::LayoutVersionExpected);
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
		const FAngelscriptCrossModuleFeatureReader* Reader = reinterpret_cast<const FAngelscriptCrossModuleFeatureReader*>(Feature);
		if (!IsValidFeatureReader(Reader))
		{
			return;
		}

		for (int32 EntryIndex = 0; EntryIndex < Reader->Count; ++EntryIndex)
		{
			const FAngelscriptCrossModuleEntry& Entry = Reader->Table[EntryIndex];
			if (Entry.ClassName == nullptr || Entry.FunctionName == nullptr || Entry.Thunk == nullptr)
			{
				UE_LOG(Angelscript, Warning, TEXT("Cross-module binding entry skipped because class, function, or thunk is null."));
				continue;
			}

			UClass* Class = ResolveCrossModuleClass(Entry, *Reader);
			if (Class == nullptr)
			{
				UE_LOG(Angelscript, Warning, TEXT("Cross-module binding entry skipped because class '%s.%s' could not be resolved."), Reader->ModuleName, Entry.ClassName != nullptr ? Entry.ClassName : TEXT("<null>"));
				continue;
			}

			FFuncEntry FuncEntry;
			FuncEntry.FuncPtr = MakeCrossModuleGenericFuncPtr();
			FuncEntry.Caller = ASAutoCaller::FunctionCaller::Make();
			FuncEntry.UserData = const_cast<FAngelscriptCrossModuleEntry*>(&Entry);
			FuncEntry.bGenericCall = true;
			FAngelscriptBinds::AddFunctionEntry(Class, Entry.FunctionName, FuncEntry);
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
		if (Type == FAngelscriptCrossModuleBindings::FeatureName())
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
			FAngelscriptCrossModuleBindings::FeatureName());
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

int32 GAngelscriptCrossModuleBindGlobalFunctionForTesting(const ANSICHAR* Signature, FAngelscriptCrossModuleEntry* Entry)
{
	return FAngelscriptBinds::BindGlobalFunctionDirect(
		Signature,
		asFUNCTION(GAngelscriptCrossModuleGenericHook),
		asCALL_GENERIC,
		ASAutoCaller::FunctionCaller::Make(),
		Entry);
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

	const FAngelscriptCrossModuleEntry* Entry = static_cast<const FAngelscriptCrossModuleEntry*>(Function->GetUserData());
	if (Entry == nullptr || Entry->Thunk == nullptr)
	{
		return;
	}

	TArray<void*, TInlineAllocator<8>> Args;
	Args.Reserve(Entry->ArgCount);
	for (uint16 ArgIndex = 0; ArgIndex < Entry->ArgCount; ++ArgIndex)
	{
		Args.Add(Generic->GetAddressOfArg(ArgIndex));
	}

	UObject* Self = (Entry->Flags & FAngelscriptCrossModuleBindings::FlagStatic) != 0
		? nullptr
		: static_cast<UObject*>(Generic->GetObject());
	void* ReturnAddress = Entry->RetSize > 0 ? Generic->GetAddressOfReturnLocation() : nullptr;
	Entry->Thunk(Self, Args.GetData(), ReturnAddress);
}
