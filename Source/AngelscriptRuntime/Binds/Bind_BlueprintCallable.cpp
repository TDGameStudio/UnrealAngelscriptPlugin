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

	bool IsEquivalentScriptSignatureAlreadyBound(
		FAngelscriptBinds& Binds,
		TSharedRef<FAngelscriptType> InType,
		FAngelscriptFunctionSignature& Signature)
	{
		if (!Signature.bAllTypesValid || Signature.ScriptName.IsEmpty() || Signature.ScriptName == TEXT("-"))
		{
			return false;
		}

		asIScriptEngine* ScriptEngine = &Binds.GetTargetScriptEngine();
		auto ApplyExistingMetadata = [&Binds, &Signature](asIScriptFunction* ExistingFunction) -> bool
		{
			if (!DoesFunctionMatchSignatureShape(ExistingFunction, Signature))
			{
				return false;
			}

			FAngelscriptBoundFunction ExistingBinding(
				&Binds.GetTargetEngine(),
				ExistingFunction->GetId());
			Signature.ModifyScriptFunction(ExistingBinding);
			return true;
		};

		if (Signature.bStaticInScript)
		{
			auto HasMatchingGlobalFunction = [&](const FString& Namespace) -> bool
			{
				for (asUINT FunctionIndex = 0, FunctionCount = ScriptEngine->GetGlobalFunctionCount(); FunctionIndex < FunctionCount; ++FunctionIndex)
				{
					asIScriptFunction* ExistingFunction = ScriptEngine->GetGlobalFunctionByIndex(FunctionIndex);
					if (DoesGlobalFunctionMatchNamespace(ExistingFunction, Namespace)
						&& ApplyExistingMetadata(ExistingFunction))
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
			if (ApplyExistingMetadata(ExistingMethod))
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
	FAngelscriptBinds& Binds,
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
		auto* map = Binds.GetTargetBindState().ClassFunctionBindings.Find(OwningClass);

		if (map)
			FunctionBinding = map->Find(Name);
	}

	// Don't bind functions without a native pointer
	if (FunctionBinding == nullptr)
		return;

#if AS_USE_BIND_DB
	FAngelscriptFunctionSignature Signature;
	Signature.InitFromDB(Binds.GetTargetTypeDatabase(), InType, Function, DBBind, /* bInitTypes= */ false);

#elif !AS_USE_BIND_DB
	FAngelscriptFunctionSignature Signature(Binds.GetTargetTypeDatabase(), InType, Function, OverrideName);

	// Don't bind things that have types that are unknown to us
	if (!Signature.bAllTypesValid)
		return;
#endif

	auto* DirectNativePointer = &FunctionBinding->FunctionPointer;
	const bool bHasDirectNativePointer = DirectNativePointer != nullptr && DirectNativePointer->IsBound();
	if (!bHasDirectNativePointer)
	{
		FAngelscriptBoundFunction PrimaryBinding;
		if (!BindBlueprintCallableReflectionFallback(
			Binds,
			InType,
			Function,
			Signature,
			*FunctionBinding,
			PrimaryBinding))
		{
			return;
		}

#if AS_USE_BIND_DB
		PrimaryBinding.NativeUFunction(Function, FPackageName::ObjectPathToObjectName(DBBind.UnrealPath), false);
#else
		PrimaryBinding.NativeUFunction(Function, Function->GetName(), false);
#endif

#if !AS_USE_BIND_DB
		Signature.WriteToDB(DBBind);
#endif
		return;
	}

	FunctionBinding->bReflectiveFallbackBound = false;
	if (IsEquivalentScriptSignatureAlreadyBound(Binds, InType, Signature))
	{
		return;
	}

	// FGenericFuncPtr is a copy of asSFuncPtr, so do a direct memcpy
	asSFuncPtr ASFuncPtr;
	static_assert(sizeof(asSFuncPtr) == sizeof(FGenericFuncPtr), "FGenericFuncPtr must be the same struct as asSFuncPtr");
	FMemory::Memcpy(&ASFuncPtr, DirectNativePointer, sizeof(asSFuncPtr));

	// Actually bind into angelscript engine
	const asECallConvTypes GlobalBindingCallConv = FunctionBinding->bUsesGenericCall ? asCALL_GENERIC : asCALL_CDECL;
	const asECallConvTypes BindingCallConv = FunctionBinding->bUsesGenericCall ? asCALL_GENERIC : asCALL_THISCALL;
	const ASAutoCaller::FunctionCaller BindingCaller = FunctionBinding->bUsesGenericCall ? ASAutoCaller::FunctionCaller::Make() : FunctionBinding->FunctionCaller;
	void* BindingUserData = FunctionBinding->bUsesGenericCall ? FunctionBinding->UserData : nullptr;
	FAngelscriptBoundFunction PrimaryBinding;

	if (Signature.bStaticInScript)
	{
		// Some functions have a meta tag to put them in global scope
		if (Signature.bGlobalScope)
		{
			FAngelscriptBoundFunction GlobalBinding = Binds.BindGlobalFunctionDirectForTarget(
				Signature.Declaration,
				ASFuncPtr,
				GlobalBindingCallConv,
				BindingCaller,
				BindingUserData);
			Signature.ModifyScriptFunction(GlobalBinding);
		}

		// Static functions should be bound as a global function in a namespace
		FAngelscriptBinds::FNamespace Namespace(Binds.GetTargetEngine(), Signature.ClassName);
		PrimaryBinding = Binds.BindGlobalFunctionDirectForTarget(
			Signature.Declaration,
			ASFuncPtr,
			GlobalBindingCallConv,
			BindingCaller,
			BindingUserData);
		Signature.ModifyScriptFunction(PrimaryBinding);
	}
	else if (Signature.bStaticInUnreal)
	{
		// This is a static function converted through mixin to a script member function
		PrimaryBinding = Binds.BindMethodDirectForTarget(
			Signature.ClassName,
			Signature.Declaration,
			ASFuncPtr,
			FunctionBinding->bUsesGenericCall ? asCALL_GENERIC : asCALL_CDECL_OBJFIRST,
			BindingCaller,
			BindingUserData);
		Signature.ModifyScriptFunction(PrimaryBinding);
	}
	else
	{
		//auto caller = ASAutoCaller::FunctionCaller::Make();
		//caller.MethodPtr = DirectNativePointer;
		// Member methods should be bound as THISCALL
		PrimaryBinding = Binds.BindMethodDirectForTarget(
			InType->GetAngelscriptTypeName(),
			Signature.Declaration,
			ASFuncPtr,
			BindingCallConv,
			BindingCaller,
			BindingUserData);
		Signature.ModifyScriptFunction(PrimaryBinding);
	}

#if AS_USE_BIND_DB
	PrimaryBinding.NativeUFunction(Function, FPackageName::ObjectPathToObjectName(DBBind.UnrealPath), Signature.bTrivial);
#else
	PrimaryBinding.NativeUFunction(Function, Function->GetName(), Signature.bTrivial);
#endif

#if !AS_USE_BIND_DB
	Signature.WriteToDB(DBBind);
#endif
}

#if !AS_USE_BIND_DB && WITH_EDITOR
// Prepare-only entry point used by BlueprintType ReflectionBindings.
// Performs all read-only checks and builds Signature + CachedBinding into Prep.
// On success, sets Prep.Kind = Callable. On any rejection, leaves
// Prep.Kind = Skip so Phase 2B can short-circuit. Read-only with respect to
// the AS Engine and the binding registries (UE reflection reads only).
//
// The NameArray prewarm in Bind_BlueprintType.cpp Phase 2A guards against
// FindFunctionByName lazy mutation reaching this path concurrently.
void BindBlueprintCallable_Prepare(
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
		auto* Map = Binds.GetTargetBindState().ClassFunctionBindings.Find(OwningClass);
		if (Map)
			FunctionBinding = Map->Find(Name);
	}
	if (FunctionBinding == nullptr)
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

	Prep.CachedBinding = FunctionBinding;
	Prep.Kind = FUFunctionBindPrep::EKind::Callable;
}

// Commit-only entry point used by the BlueprintType ReflectionBindings
// prepare/commit split. The caller is responsible for filling Prep with:
//   - Function           (UFunction*, non-null, FUNC_Native, not skipped)
//   - Signature          (already InitFromFunction'd, bAllTypesValid == true)
//   - CachedBinding      (target BindState ClassFunctionBindings lookup result, non-null)
//   - Kind == Callable
// This function only performs the AS Engine register half (must run on GameThread).
void BindBlueprintCallable_FromPrep(
	FAngelscriptBinds& Binds,
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
		FAngelscriptBoundFunction PrimaryBinding;
		if (!BindBlueprintCallableReflectionFallback(
			Binds,
			InType,
			Function,
			Signature,
			*FunctionBinding,
			PrimaryBinding))
		{
			return;
		}

		PrimaryBinding.NativeUFunction(Function, Function->GetName(), false);

		Signature.WriteToDB(DBBind);
		return;
	}

	FunctionBinding->bReflectiveFallbackBound = false;
	if (IsEquivalentScriptSignatureAlreadyBound(Binds, InType, Signature))
	{
		return;
	}

	asSFuncPtr ASFuncPtr;
	static_assert(sizeof(asSFuncPtr) == sizeof(FGenericFuncPtr), "FGenericFuncPtr must be the same struct as asSFuncPtr");
	FMemory::Memcpy(&ASFuncPtr, DirectNativePointer, sizeof(asSFuncPtr));

	const asECallConvTypes GlobalBindingCallConv = FunctionBinding->bUsesGenericCall ? asCALL_GENERIC : asCALL_CDECL;
	const asECallConvTypes BindingCallConv = FunctionBinding->bUsesGenericCall ? asCALL_GENERIC : asCALL_THISCALL;
	const ASAutoCaller::FunctionCaller BindingCaller = FunctionBinding->bUsesGenericCall ? ASAutoCaller::FunctionCaller::Make() : FunctionBinding->FunctionCaller;
	void* BindingUserData = FunctionBinding->bUsesGenericCall ? FunctionBinding->UserData : nullptr;
	FAngelscriptBoundFunction PrimaryBinding;

	if (Signature.bStaticInScript)
	{
		if (Signature.bGlobalScope)
		{
			FAngelscriptBoundFunction GlobalBinding = Binds.BindGlobalFunctionDirectForTarget(
				Signature.Declaration,
				ASFuncPtr,
				GlobalBindingCallConv,
				BindingCaller,
				BindingUserData);
			Signature.ModifyScriptFunction(GlobalBinding);
		}

		FAngelscriptBinds::FNamespace Namespace(Binds.GetTargetEngine(), Signature.ClassName);
		PrimaryBinding = Binds.BindGlobalFunctionDirectForTarget(
			Signature.Declaration,
			ASFuncPtr,
			GlobalBindingCallConv,
			BindingCaller,
			BindingUserData);
		Signature.ModifyScriptFunction(PrimaryBinding);
	}
	else if (Signature.bStaticInUnreal)
	{
		PrimaryBinding = Binds.BindMethodDirectForTarget(
			Signature.ClassName,
			Signature.Declaration,
			ASFuncPtr,
			FunctionBinding->bUsesGenericCall ? asCALL_GENERIC : asCALL_CDECL_OBJFIRST,
			BindingCaller,
			BindingUserData);
		Signature.ModifyScriptFunction(PrimaryBinding);
	}
	else
	{
		PrimaryBinding = Binds.BindMethodDirectForTarget(
			InType->GetAngelscriptTypeName(),
			Signature.Declaration,
			ASFuncPtr,
			BindingCallConv,
			BindingCaller,
			BindingUserData);
		Signature.ModifyScriptFunction(PrimaryBinding);
	}

	PrimaryBinding.NativeUFunction(Function, Function->GetName(), Signature.bTrivial);

	Signature.WriteToDB(DBBind);
}

#endif // !AS_USE_BIND_DB && WITH_EDITOR
