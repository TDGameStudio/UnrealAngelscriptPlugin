#include "AngelscriptBinds.h"
#include "AngelscriptEngine.h"
#include "AngelscriptType.h"
#include "AngelscriptDocs.h"

#include "UObject/UObjectIterator.h"
#include "UObject/UnrealType.h"
#include "ClassGenerator/ASClass.h"

#include "BlueprintCallableReflectiveFallback.h"
#include "Helper_FunctionSignature.h"
#include "Bind_BlueprintTypePrep.h"

extern void RegisterBlueprintEventByScriptName(UClass* Class, const FString& ScriptName, UFunction* Function);

namespace
{
	bool IsScriptMethodReadOnly(const FString& Declaration)
	{
		return Declaration.Contains(TEXT(") const"));
	}

	bool DoesFunctionMatchSignatureShape(asIScriptFunction* ExistingFunction, const FAngelscriptFunctionSignature& Signature)
	{
		if (ExistingFunction == nullptr)
		{
			return false;
		}

		const FTCHARToUTF8 ScriptNameUtf8(*Signature.ScriptName);
		const char* ExistingName = ExistingFunction->GetName();
		if (FCStringAnsi::Strcmp(ExistingName != nullptr ? ExistingName : "", ScriptNameUtf8.Get()) != 0)
		{
			return false;
		}

		if (!Signature.bStaticInScript && ExistingFunction->IsReadOnly() != IsScriptMethodReadOnly(Signature.Declaration))
		{
			return false;
		}

		if (ExistingFunction->GetParamCount() != static_cast<asUINT>(Signature.ArgumentTypes.Num()))
		{
			return false;
		}

		for (int32 ArgumentIndex = 0, ArgumentCount = Signature.ArgumentTypes.Num(); ArgumentIndex < ArgumentCount; ++ArgumentIndex)
		{
			FAngelscriptTypeUsage ExistingArgument = FAngelscriptTypeUsage::FromParam(ExistingFunction, ArgumentIndex);
			if (!ExistingArgument.IsValid() || !ExistingArgument.EqualsUnqualified(Signature.ArgumentTypes[ArgumentIndex]))
			{
				return false;
			}
		}

		return true;
	}

	bool DoesGlobalFunctionMatchNamespace(asIScriptFunction* ExistingFunction, const FString& Namespace)
	{
		if (ExistingFunction == nullptr)
		{
			return false;
		}

		const char* ExistingNamespace = ExistingFunction->GetNamespace();
		if (Namespace.IsEmpty())
		{
			return ExistingNamespace == nullptr || ExistingNamespace[0] == '\0';
		}

		const FTCHARToUTF8 NamespaceUtf8(*Namespace);
		return FCStringAnsi::Strcmp(ExistingNamespace != nullptr ? ExistingNamespace : "", NamespaceUtf8.Get()) == 0;
	}

	bool IsEquivalentScriptSignatureAlreadyBound(TSharedRef<FAngelscriptType> InType, const FAngelscriptFunctionSignature& Signature)
	{
		if (!Signature.bAllTypesValid || Signature.ScriptName.IsEmpty() || Signature.ScriptName == TEXT("-"))
		{
			return false;
		}

		asIScriptEngine* ScriptEngine = FAngelscriptEngine::Get().GetScriptEngine();
		if (ScriptEngine == nullptr)
		{
			return false;
		}

		if (Signature.bStaticInScript)
		{
			auto HasMatchingGlobalFunction = [&](const FString& Namespace) -> bool
			{
				for (asUINT FunctionIndex = 0, FunctionCount = ScriptEngine->GetGlobalFunctionCount(); FunctionIndex < FunctionCount; ++FunctionIndex)
				{
					asIScriptFunction* ExistingFunction = ScriptEngine->GetGlobalFunctionByIndex(FunctionIndex);
					if (DoesGlobalFunctionMatchNamespace(ExistingFunction, Namespace)
						&& DoesFunctionMatchSignatureShape(ExistingFunction, Signature))
					{
						return true;
					}
				}

				return false;
			};

			return HasMatchingGlobalFunction(Signature.ClassName)
				|| (Signature.bGlobalScope && HasMatchingGlobalFunction(FString()));
		}

		const FString& ScriptTypeName = Signature.bStaticInUnreal ? Signature.ClassName : InType->GetAngelscriptTypeName();
		const FTCHARToUTF8 ScriptTypeNameUtf8(*ScriptTypeName);
		asITypeInfo* TypeInfo = ScriptEngine->GetTypeInfoByName(ScriptTypeNameUtf8.Get());
		if (TypeInfo == nullptr)
		{
			return false;
		}

		for (asUINT MethodIndex = 0, MethodCount = TypeInfo->GetMethodCount(); MethodIndex < MethodCount; ++MethodIndex)
		{
			asIScriptFunction* ExistingMethod = TypeInfo->GetMethodByIndex(MethodIndex);
			if (DoesFunctionMatchSignatureShape(ExistingMethod, Signature))
			{
				return true;
			}
		}

		return false;
	}
}

// Bind a native function to angelscript, provided all
// argument and return types are known as FAngelscriptTypes.
void BindBlueprintCallable(
	TSharedRef<FAngelscriptType> InType,
	UFunction* Function,
	FAngelscriptMethodBind& DBBind
#if !AS_USE_BIND_DB
	, const TCHAR* OverrideName
#endif
)
{
#if !AS_USE_BIND_DB
	// Don't bind functions that aren't native
	if (!Function->HasAnyFunctionFlags(FUNC_Native))
		return;
	if (FAngelscriptBinds::ShouldSkipBlueprintCallableFunction(Function))
		return;
#endif

	UClass* OwningClass = CastChecked<UClass>(Function->GetOuter());
	FAngelscriptFunctionBinding* FunctionBinding = nullptr;

	if (OwningClass != nullptr)
	{
		FString Name = Function->GetFName().ToString();
		auto* map = FAngelscriptBinds::GetClassFunctionBindings().Find(OwningClass);

		if (map)
			FunctionBinding = map->Find(Name);
	}

	// Don't bind functions without a native pointer
	if (FunctionBinding == nullptr)
		return;

#if AS_USE_BIND_DB
	FAngelscriptFunctionSignature Signature;
	Signature.InitFromDB(InType, Function, DBBind, /* bInitTypes= */ false);

#elif !AS_USE_BIND_DB
	FAngelscriptFunctionSignature Signature(InType, Function, OverrideName);

	// Don't bind things that have types that are unknown to us
	if (!Signature.bAllTypesValid)
		return;
#endif

	auto* DirectNativePointer = &FunctionBinding->FunctionPointer;
	const bool bHasDirectNativePointer = DirectNativePointer != nullptr && DirectNativePointer->IsBound();
	if (!bHasDirectNativePointer)
	{
		if (!BindBlueprintCallableReflectionFallback(InType, Function, Signature, *FunctionBinding))
			return;

#if AS_CAN_GENERATE_JIT
#if AS_USE_BIND_DB
		SCRIPT_NATIVE_UFUNCTION(Function, FPackageName::ObjectPathToObjectName(DBBind.UnrealPath), false);
#else
		SCRIPT_NATIVE_UFUNCTION(Function, Function->GetName(), false);
#endif
#endif

#if !AS_USE_BIND_DB
		Signature.WriteToDB(DBBind);
#endif
		return;
	}

	FunctionBinding->bReflectiveFallbackBound = false;
	if (IsEquivalentScriptSignatureAlreadyBound(InType, Signature))
	{
		return;
	}

	// FGenericFuncPtr is a copy of asSFuncPtr, so do a direct memcpy
	asSFuncPtr ASFuncPtr;
	static_assert(sizeof(asSFuncPtr) == sizeof(FGenericFuncPtr), "FGenericFuncPtr must be the same struct as asSFuncPtr");
	FMemory::Memcpy(&ASFuncPtr, DirectNativePointer, sizeof(asSFuncPtr));

	// Actually bind into angelscript engine
	const asECallConvTypes BindingCallConv = FunctionBinding->bUsesGenericCall ? asCALL_GENERIC : asCALL_THISCALL;
	const ASAutoCaller::FunctionCaller BindingCaller = FunctionBinding->bUsesGenericCall ? ASAutoCaller::FunctionCaller::Make() : FunctionBinding->FunctionCaller;
	void* BindingUserData = FunctionBinding->bUsesGenericCall ? FunctionBinding->UserData : nullptr;

	if (Signature.bStaticInScript)
	{
		// Some functions have a meta tag to put them in global scope
		if (Signature.bGlobalScope)
		{
			//int GlobalFunctionId = FAngelscriptBinds::BindGlobalFunction(Signature.Declaration, ASFuncPtr, FuncInMap->Value);			
			int GlobalFunctionId = FunctionBinding->bUsesGenericCall
				? FAngelscriptBinds::BindGlobalFunctionDirect(Signature.Declaration, ASFuncPtr, asCALL_GENERIC, BindingCaller, BindingUserData)
				: FAngelscriptBinds::BindGlobalFunction(Signature.Declaration, ASFuncPtr, BindingCaller);
			Signature.ModifyScriptFunction(GlobalFunctionId);
		}

		// Static functions should be bound as a global function in a namespace
		FAngelscriptBinds::FNamespace ns(Signature.ClassName);
		//int FunctionId = FAngelscriptBinds::BindGlobalFunction(Signature.Declaration, ASFuncPtr, FuncInMap->Value);
		int FunctionId = FunctionBinding->bUsesGenericCall
			? FAngelscriptBinds::BindGlobalFunctionDirect(Signature.Declaration, ASFuncPtr, asCALL_GENERIC, BindingCaller, BindingUserData)
			: FAngelscriptBinds::BindGlobalFunction(Signature.Declaration, ASFuncPtr, BindingCaller);
		Signature.ModifyScriptFunction(FunctionId);
	}
	else if (Signature.bStaticInUnreal)
	{
		// This is a static function converted through mixin to a script member function
		int FunctionId = FAngelscriptBinds::BindMethodDirect
		(
			Signature.ClassName,
			Signature.Declaration, ASFuncPtr,
			FunctionBinding->bUsesGenericCall ? asCALL_GENERIC : asCALL_CDECL_OBJFIRST, BindingCaller, BindingUserData
		);
		Signature.ModifyScriptFunction(FunctionId);
	}
	else
	{
		//auto caller = ASAutoCaller::FunctionCaller::Make();
		//caller.MethodPtr = DirectNativePointer;
		// Member methods should be bound as THISCALL		
		int FunctionId = FAngelscriptBinds::BindMethodDirect
		(
			InType->GetAngelscriptTypeName(),
			Signature.Declaration, ASFuncPtr, BindingCallConv, BindingCaller, BindingUserData
		);
		Signature.ModifyScriptFunction(FunctionId);
	}

#if AS_CAN_GENERATE_JIT
#if AS_USE_BIND_DB
	SCRIPT_NATIVE_UFUNCTION(Function, FPackageName::ObjectPathToObjectName(DBBind.UnrealPath), Signature.bTrivial);
#else
	SCRIPT_NATIVE_UFUNCTION(Function, Function->GetName(), Signature.bTrivial);
#endif
#endif

#if !AS_USE_BIND_DB
	Signature.WriteToDB(DBBind);
#endif
}

#if !AS_USE_BIND_DB && WITH_EDITOR
// Prepare-only entry point used by the Bind_Defaults Late+100 Phase 2A.
// Performs all read-only checks and builds Signature + CachedBinding into Prep.
// On success, sets Prep.Kind = Callable. On any rejection, leaves
// Prep.Kind = Skip so Phase 2B can short-circuit. Read-only with respect to
// the AS Engine and the binding registries (UE reflection reads only).
//
// The NameArray prewarm in Bind_BlueprintType.cpp Phase 2A guards against
// FindFunctionByName lazy mutation reaching this path concurrently.
void BindBlueprintCallable_Prepare(
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

	if (!Function->HasAnyFunctionFlags(FUNC_Native))
	{
		return;
	}
	if (FAngelscriptBinds::ShouldSkipBlueprintCallableFunction(Function))
	{
		return;
	}

	UClass* OwningClass = CastChecked<UClass>(Function->GetOuter());
	FAngelscriptFunctionBinding* FunctionBinding = nullptr;
	if (OwningClass != nullptr)
	{
		FString Name = Function->GetFName().ToString();
		auto* Map = FAngelscriptBinds::GetClassFunctionBindings().Find(OwningClass);
		if (Map)
			FunctionBinding = Map->Find(Name);
	}
	if (FunctionBinding == nullptr)
	{
		return;
	}

	Prep.Signature = FAngelscriptFunctionSignature(InType, Function, /*OverrideName=*/nullptr);
	if (!Prep.Signature.bAllTypesValid)
	{
		return;
	}

	Prep.CachedBinding = FunctionBinding;
	Prep.Kind = FUFunctionBindPrep::EKind::Callable;
}

// Commit-only entry point used by the Bind_Defaults Late+100 Phase 2A/2B split
// (see Plan_BindParallelization). The caller is responsible for filling Prep with:
//   - Function           (UFunction*, non-null, FUNC_Native, not skipped)
//   - Signature          (already InitFromFunction'd, bAllTypesValid == true)
//   - CachedBinding      (FAngelscriptBinds::GetClassFunctionBindings lookup result, non-null)
//   - Kind == Callable
// This function only performs the AS Engine register half (must run on GameThread).
void BindBlueprintCallable_FromPrep(
	TSharedRef<FAngelscriptType> InType,
	FUFunctionBindPrep& Prep,
	FAngelscriptMethodBind& DBBind)
{
	UFunction* Function = Prep.Function;
	FAngelscriptFunctionBinding* FunctionBinding = Prep.CachedBinding;
	FAngelscriptFunctionSignature& Signature = Prep.Signature;

	if (Function == nullptr || FunctionBinding == nullptr)
	{
		return;
	}

	auto* DirectNativePointer = &FunctionBinding->FunctionPointer;
	const bool bHasDirectNativePointer = DirectNativePointer != nullptr && DirectNativePointer->IsBound();
	if (!bHasDirectNativePointer)
	{
		if (!BindBlueprintCallableReflectionFallback(InType, Function, Signature, *FunctionBinding))
		{
			return;
		}

#if AS_CAN_GENERATE_JIT
		SCRIPT_NATIVE_UFUNCTION(Function, Function->GetName(), false);
#endif

		Signature.WriteToDB(DBBind);
		return;
	}

	FunctionBinding->bReflectiveFallbackBound = false;
	if (IsEquivalentScriptSignatureAlreadyBound(InType, Signature))
	{
		return;
	}

	asSFuncPtr ASFuncPtr;
	static_assert(sizeof(asSFuncPtr) == sizeof(FGenericFuncPtr), "FGenericFuncPtr must be the same struct as asSFuncPtr");
	FMemory::Memcpy(&ASFuncPtr, DirectNativePointer, sizeof(asSFuncPtr));

	const asECallConvTypes BindingCallConv = FunctionBinding->bUsesGenericCall ? asCALL_GENERIC : asCALL_THISCALL;
	const ASAutoCaller::FunctionCaller BindingCaller = FunctionBinding->bUsesGenericCall ? ASAutoCaller::FunctionCaller::Make() : FunctionBinding->FunctionCaller;
	void* BindingUserData = FunctionBinding->bUsesGenericCall ? FunctionBinding->UserData : nullptr;

	if (Signature.bStaticInScript)
	{
		if (Signature.bGlobalScope)
		{
			int GlobalFunctionId = FunctionBinding->bUsesGenericCall
				? FAngelscriptBinds::BindGlobalFunctionDirect(Signature.Declaration, ASFuncPtr, asCALL_GENERIC, BindingCaller, BindingUserData)
				: FAngelscriptBinds::BindGlobalFunction(Signature.Declaration, ASFuncPtr, BindingCaller);
			Signature.ModifyScriptFunction(GlobalFunctionId);
		}

		FAngelscriptBinds::FNamespace ns(Signature.ClassName);
		int FunctionId = FunctionBinding->bUsesGenericCall
			? FAngelscriptBinds::BindGlobalFunctionDirect(Signature.Declaration, ASFuncPtr, asCALL_GENERIC, BindingCaller, BindingUserData)
			: FAngelscriptBinds::BindGlobalFunction(Signature.Declaration, ASFuncPtr, BindingCaller);
		Signature.ModifyScriptFunction(FunctionId);
	}
	else if (Signature.bStaticInUnreal)
	{
		int FunctionId = FAngelscriptBinds::BindMethodDirect(
			Signature.ClassName,
			Signature.Declaration, ASFuncPtr,
			FunctionBinding->bUsesGenericCall ? asCALL_GENERIC : asCALL_CDECL_OBJFIRST, BindingCaller, BindingUserData);
		Signature.ModifyScriptFunction(FunctionId);
	}
	else
	{
		int FunctionId = FAngelscriptBinds::BindMethodDirect(
			InType->GetAngelscriptTypeName(),
			Signature.Declaration, ASFuncPtr,
			BindingCallConv, BindingCaller, BindingUserData);
		Signature.ModifyScriptFunction(FunctionId);
	}

#if AS_CAN_GENERATE_JIT
	SCRIPT_NATIVE_UFUNCTION(Function, Function->GetName(), Signature.bTrivial);
#endif

	Signature.WriteToDB(DBBind);
}
#endif // !AS_USE_BIND_DB && WITH_EDITOR
