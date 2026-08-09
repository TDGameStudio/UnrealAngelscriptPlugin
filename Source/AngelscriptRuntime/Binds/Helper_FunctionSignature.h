#pragma once

#include "CoreMinimal.h"
#include "UObject/UnrealType.h"

#include "AngelscriptEngine.h"
#include "AngelscriptType.h"
#include "AngelscriptDocs.h"
#include "AngelscriptBindDatabase.h"
#include "Binds/BlueprintCallableReflectiveFallback.h"

#include "StartAngelscriptHeaders.h"
//#include "as_scriptfunction.h"
#include "source/as_scriptfunction.h"
#include "EndAngelscriptHeaders.h"

static const FName NAME_Signature_ScriptName("ScriptName");
static const FName NAME_Signature_WorldContext("WorldContext");
static const FName NAME_Signature_DeterminesOutputType("DeterminesOutputType");
static const FName NAME_Signature_ScriptGlobalScope("ScriptGlobalScope");
static const FName NAME_Signature_ToolTip("ToolTip");
static const FName NAME_Signature_Category("Category");
static const FName NAME_Signature_ScriptMixin("ScriptMixin");
static const FName NAME_Signature_ScriptTrivial("ScriptTrivial");
static const FName NAME_Signature_NotAngelscriptProperty("NotAngelscriptProperty");
static const FName NAME_AS_Tooltip("ScriptTooltip");
static const FName NAME_AS_BlueprintProtected("BlueprintProtected");
static const FName NAME_Function_DeprecatedFunction("DeprecatedFunction");
static const FName NAME_Function_DeprecationMessage("DeprecationMessage");
static const FName NAME_OptionalWorldContext("OptionalWorldContext");
static const FName NAME_CallableWithoutWorldContext("CallableWithoutWorldContext");
static const FName NAME_Signature_ScriptMethod("ScriptMethod");
static const FName NAME_ScriptNoDiscard("ScriptNoDiscard");
static const FName NAME_ScriptAllowDiscard("ScriptAllowDiscard");
static const FName NAME_ScriptAllowTemporaryThis("ScriptAllowTemporaryThis");
static const FName NAME_UnsafeDuringActorConstruction("UnsafeDuringActorConstruction");

struct FAngelscriptFunctionSignature
{
	TArray<FAngelscriptTypeUsage> ArgumentTypes;
	FAngelscriptTypeUsage ReturnType;

	TArray<FString> ArgumentNames;
	TArray<FString> ArgumentDefaults;

	bool bAllTypesValid = true;
	int8 WorldContextArgument = -1;
	int8 DeterminesOutputTypeArgument = -1;

	FString Declaration;
	FString ClassName;

	bool bStaticInScript = false;
	bool bStaticInUnreal = false;
	bool bHasMixinIntent = false;
	bool bMixinReceiverMatched = false;
	FString MixinTargets;
	FString MixinFirstParameterType;
	FString ValidationError;

	bool bGlobalScope = false;
	bool bNotAngelscriptProperty = false;
	bool bTrivial = false;
	bool bBlueprintProtected = false;
	FString ScriptName;

#if WITH_EDITOR
	bool bDeprecated = false;
	FString DeprecationMessage;
#endif

	UFunction* Function = nullptr;

	FAngelscriptFunctionSignature()
	{
	}

	// Available in all build configurations. The dynamic class generator resolves
	// Blueprint-event script names at runtime (including in packaged / non-editor
	// builds via GetBlueprintEventByScriptName), so these helpers must not be
	// editor-only. Only the metadata lookup below is editor-only and is guarded.
	static FString GetPrimaryScriptName(const FString& InScriptName)
	{
		FString PrimaryName;
		if (InScriptName.Split(TEXT(";"), &PrimaryName, nullptr))
		{
			return PrimaryName;
		}

		return InScriptName;
	}

	static FString GetScriptNameForFunction(UFunction* InFunction)
	{
		// Determine the actual name of the function to bind
		FString OutScriptName = InFunction->GetName();

#if WITH_EDITORONLY_DATA
		if (InFunction->HasMetaData(NAME_Signature_ScriptName))
		{
			OutScriptName = GetPrimaryScriptName(InFunction->GetMetaData(NAME_Signature_ScriptName));
		}
		else
#endif
		{
			bool bChangedName = false;
			bChangedName |= OutScriptName.RemoveFromStart(TEXT("K2_"));
			bChangedName |= OutScriptName.RemoveFromStart(TEXT("BP_"));
			bChangedName |= OutScriptName.RemoveFromStart(TEXT("AS_"));

			if (InFunction->HasAnyFunctionFlags(FUNC_BlueprintEvent))
			{
				bChangedName |= OutScriptName.RemoveFromStart(TEXT("Received_"));
				bChangedName |= OutScriptName.RemoveFromStart(TEXT("Receive"));
			}

			if (bChangedName)
			{
				// If another function already exists with this name, don't bind it without the prefix.
				// Opt 3: when Phase 2 TLS cache is active, consult it first (O(1)) instead of FindFunctionByName.
				UClass* OwningClass = CastChecked<UClass>(InFunction->GetOuter());
				bool bConflict = false;

				bool bCacheExists = false;
				if (AngelscriptBindCaches_TryHasFunctionName(OwningClass, FName(*OutScriptName), bCacheExists))
				{
					if (bCacheExists)
					{
						// Cache only records existence; we still need the function pointer to
						// determine whether the conflict is a Blueprint-exposed function.
						if (UFunction* ExistingFunction = OwningClass->FindFunctionByName(*OutScriptName))
						{
							if (ExistingFunction != InFunction && ExistingFunction->HasAnyFunctionFlags(FUNC_BlueprintCallable | FUNC_BlueprintPure | FUNC_BlueprintEvent))
							{
								bConflict = true;
							}
						}
					}
				}
				else if (UFunction* ExistingFunction = OwningClass->FindFunctionByName(*OutScriptName))
				{
					if (ExistingFunction != InFunction && ExistingFunction->HasAnyFunctionFlags(FUNC_BlueprintCallable | FUNC_BlueprintPure | FUNC_BlueprintEvent))
					{
						bConflict = true;
					}
				}

				if (bConflict)
				{
					OutScriptName = InFunction->GetName();
				}
			}
		}
		return OutScriptName;
	}

#if WITH_EDITOR
	FAngelscriptFunctionSignature(
		FAngelscriptTypeDatabase& TypeDatabase,
		TSharedRef<FAngelscriptType> InType,
		UFunction* InFunction,
		const TCHAR* OverrideName = nullptr)
	{
		InitFromFunction(TypeDatabase, InType, InFunction, OverrideName);
	}

	FAngelscriptFunctionSignature(TSharedRef<FAngelscriptType> InType, UFunction* InFunction, const TCHAR* OverrideName = nullptr)
	{
		InitFromFunction(InType, InFunction, OverrideName);
	}

	void InitFromFunction(
		FAngelscriptTypeDatabase& TypeDatabase,
		TSharedRef<FAngelscriptType> InType,
		UFunction* InFunction,
		const TCHAR* OverrideName = nullptr)
	{
		(void)TypeDatabase;
		InitFromFunction(InType, InFunction, OverrideName);
	}

	static FString GetScriptNamespaceForClass(TSharedRef<FAngelscriptType> InType, UFunction* InFunction)
	{
		if (InFunction != nullptr)
		{
			if (UClass* OuterClass = InFunction->GetOuterUClass())
			{
				if (OuterClass->HasMetaData(NAME_Signature_ScriptName))
				{
					return GetPrimaryScriptName(OuterClass->GetMetaData(NAME_Signature_ScriptName));
				}
			}
		}

		return InType->GetAngelscriptTypeName();
	}

	void InitFromFunction(TSharedRef<FAngelscriptType> InType, UFunction* InFunction, const TCHAR* OverrideName = nullptr)
	{
		Function = InFunction;

		// Opt 2 (revised): UField metadata is stored package-side, not on the field, so
		// there is no O(1) map pointer to batch from. Instead, collapse each
		// HasMetaData(K) + GetMetaData(K) pair into a single FindMetaData(K) call —
		// this halves the FMetaData::FindValue lookups without changing semantics.
		UClass* OuterClassForMeta = Function->GetOuterUClass();

		static const FString EmptyString;
		auto HasFuncMeta = [&](const FName& K) -> bool
		{
			return Function->FindMetaData(K) != nullptr;
		};
		auto GetFuncMetaRef = [&](const FName& K) -> const FString&
		{
			const FString* V = Function->FindMetaData(K);
			return V != nullptr ? *V : EmptyString;
		};
		auto GetClassMetaRef = [&](const FName& K) -> const FString&
		{
			if (OuterClassForMeta == nullptr)
			{
				return EmptyString;
			}
			const FString* V = OuterClassForMeta->FindMetaData(K);
			return V != nullptr ? *V : EmptyString;
		};

		// Map all properties in the UFunction to FAngelscriptTypes
		for( TFieldIterator<FProperty> It(Function); It && (It->PropertyFlags & CPF_Parm); ++It )
		{
			FProperty* Property = *It;
			FAngelscriptTypeUsage Type = FAngelscriptTypeUsage::FromProperty(Property);

			if (!Type.IsValid())
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

				// Opt 2: one FindMetaData call instead of HasMetaData + GetMetaData.
				FString DefaultMeta = TEXT("CPP_Default_");
				DefaultMeta += Property->GetName();

				FName MetaName = *DefaultMeta;
				if (const FString* MetaValue = Function->FindMetaData(MetaName))
				{
					FString MetaStr = *MetaValue;
					if (MetaStr == TEXT("None"))
						MetaStr = TEXT("");
					ArgumentDefaults.Add(MetaStr);
				}
				else
				{
					ArgumentDefaults.Add(TEXT("-"));
				}
			}
		}

		// If the function has a world context pin, we should default it
		const FString& WorldContextParam = GetFuncMetaRef(NAME_Signature_WorldContext);
		if (WorldContextParam.Len() != 0)
		{
			for (int32 ArgIndex = 0, ArgCount = ArgumentTypes.Num(); ArgIndex < ArgCount; ++ArgIndex)
			{
				if (ArgumentNames[ArgIndex] == WorldContextParam)
				{
					ArgumentDefaults[ArgIndex] = TEXT("__WorldContext()");
					WorldContextArgument = ArgIndex;
					break;
				}
			}
		}

		// Check if we're using the DeterminesOutputType functionality to change the return type dynamically
		const FString& DeterminesOutputTypeParam = GetFuncMetaRef(NAME_Signature_DeterminesOutputType);
		if (DeterminesOutputTypeParam.Len() != 0)
		{
			for (int32 ArgIndex = 0, ArgCount = ArgumentTypes.Num(); ArgIndex < ArgCount; ++ArgIndex)
			{
				if (ArgumentNames[ArgIndex] == DeterminesOutputTypeParam)
				{
					DeterminesOutputTypeArgument = ArgIndex;
					break;
				}
			}
		}

		if (OverrideName)
			ScriptName = OverrideName;
		else
			ScriptName = GetScriptNameForFunction(Function);

		// Functions with - as their script name are excluded from being bound
		if (ScriptName == TEXT("-"))
			return;

		bNotAngelscriptProperty = HasFuncMeta(NAME_Signature_NotAngelscriptProperty);
		bTrivial = HasFuncMeta(NAME_Signature_ScriptTrivial);
		// GetBoolMetaData has non-trivial semantics (treats "true" specifically); keep the direct call.
		bBlueprintProtected = Function->GetBoolMetaData(NAME_AS_BlueprintProtected);

		{
			// Opt 2: single FindMetaData call for deprecated probe.
			const FString* DeprecatedMeta = Function->FindMetaData(NAME_Function_DeprecatedFunction);
			bDeprecated = DeprecatedMeta != nullptr;
			if (bDeprecated)
				DeprecationMessage = GetFuncMetaRef(NAME_Function_DeprecationMessage);
		}

		// Figure out the namespace for static functions
		bool bForceConst = false;
		bStaticInUnreal = Function->HasAnyFunctionFlags(FUNC_Static);
		if (bStaticInUnreal)
		{
			FString Namespace = GetScriptNamespaceForClass(InType, Function);
			bGlobalScope = HasFuncMeta(NAME_Signature_ScriptGlobalScope);

			// If our class is marked as a script mixin, bind matching receivers as
			// members. A class-level ScriptName is an explicit namespace for static
			// factory helpers that share the class with receiver-style functions.
			bool bFoundMixin = false;
			bool bMixinReceiverValidationFailed = false;
			const FString& MixinClasses = GetClassMetaRef(NAME_Signature_ScriptMixin);
			const bool bHasExplicitStaticNamespace = !GetClassMetaRef(NAME_Signature_ScriptName).IsEmpty();

			// UE 5.7+: function-level ScriptMethod metadata is no longer propagated to class-level
			// ScriptMixin by UHT. When the class has no ScriptMixin but the function itself carries
			// ScriptMethod, treat the first parameter's type as the mixin target.
			const bool bFunctionLevelScriptMethod = MixinClasses.IsEmpty()
				&& HasFuncMeta(NAME_Signature_ScriptMethod);
			bHasMixinIntent = !MixinClasses.IsEmpty() || bFunctionLevelScriptMethod;
			MixinTargets = !MixinClasses.IsEmpty() ? MixinClasses : TEXT("<ScriptMethod:first-parameter>");
			MixinFirstParameterType = TEXT("<missing>");

			if (bHasMixinIntent)
			{
				TArray<FString> MixinList;
				MixinClasses.ParseIntoArray(MixinList, TEXT(" "));

				const bool bHasReceiverParameter = ArgumentTypes.Num() > 0
					&& ArgumentTypes[0].Type.IsValid();
				if (bHasReceiverParameter)
				{
					MixinFirstParameterType = ArgumentTypes[0].Type->GetAngelscriptTypeName(ArgumentTypes[0]);
					if (bFunctionLevelScriptMethod && MixinList.Num() == 0)
					{
						MixinList.Add(MixinFirstParameterType);
						MixinTargets = MixinFirstParameterType;
					}
				}

				FString UnresolvedObjectMixinType;
				if (bHasReceiverParameter
					&& ArgumentTypes[0].Type->IsUnresolvedObjectPointer()
					&& ArgumentTypes[0].SubTypes.Num() > 0
					&& ArgumentTypes[0].SubTypes[0].Type.IsValid())
				{
					UnresolvedObjectMixinType = ArgumentTypes[0].SubTypes[0].Type->GetAngelscriptTypeName(ArgumentTypes[0].SubTypes[0]);
				}

				// UE ScriptMethod metadata commonly describes value-type receivers by
				// value (for example FVector in Kismet libraries). Exact AS type identity,
				// not the C++ reference qualifier, determines receiver compatibility.
				const bool bHasReceiverShape = bHasReceiverParameter;
				for (const FString& Mixin : MixinList)
				{
					if (bHasReceiverShape
						&& FAngelscriptType::GetByAngelscriptTypeName(Mixin).IsValid()
						&& (MixinFirstParameterType == Mixin || UnresolvedObjectMixinType == Mixin))
					{
						if (ArgumentTypes[0].bIsConst)
							bForceConst = true;

						ArgumentTypes.RemoveAt(0);
						ArgumentNames.RemoveAt(0);
						ArgumentDefaults.RemoveAt(0);
						ClassName = Mixin;

						bStaticInScript = false;
						bFoundMixin = true;
						bMixinReceiverMatched = true;

						if (WorldContextArgument >= 0)
							WorldContextArgument -= 1;
						if (DeterminesOutputTypeArgument >= 0)
							DeterminesOutputTypeArgument -= 1;
						break;
					}
				}

				// A class ScriptName explicitly supplies the namespace for static factory
				// helpers. Every other mixin declaration requires a compatible receiver;
				// silently turning it into a library namespace would change the API shape.
				bMixinReceiverValidationFailed = !bFoundMixin
					&& (bFunctionLevelScriptMethod || !bHasExplicitStaticNamespace);
			}

			if (bMixinReceiverValidationFailed)
			{
				ClassName.Empty();
				bStaticInScript = false;
			}
			else if (!bFoundMixin)
			{
				ClassName = Namespace;
				bStaticInScript = true;
			}
		}
		else
		{
			ClassName = InType->GetAngelscriptTypeName();
		}

		// Build the declaration for the function
		Declaration = FAngelscriptType::BuildFunctionDeclaration(ReturnType, ScriptName, ArgumentTypes, ArgumentNames, ArgumentDefaults,
			(Function->HasAnyFunctionFlags(FUNC_Const) && !bStaticInScript) || bForceConst);

		if (bStaticInUnreal && bHasMixinIntent && !bMixinReceiverMatched
			&& ClassName.IsEmpty())
		{
			ValidationError = FString::Printf(
				TEXT("Invalid ScriptMixin receiver for %s::%s: targets='%s', first parameter='%s', declaration='%s'"),
				OuterClassForMeta != nullptr ? *OuterClassForMeta->GetName() : TEXT("<unknown-class>"),
				*Function->GetName(),
				*MixinTargets,
				*MixinFirstParameterType,
				*Declaration);
			bAllTypesValid = false;
		}

		// Add no-discard modifier if we want to
		if (ReturnType.IsValid())
		{
			if (HasFuncMeta(NAME_ScriptNoDiscard))
				Declaration += TEXT(" no_discard");
			else if (HasFuncMeta(NAME_ScriptAllowDiscard))
				Declaration += TEXT(" allow_discard");
		}

		if (HasFuncMeta(NAME_ScriptAllowTemporaryThis))
			Declaration += TEXT(" accept_temporary_this");
	}
#endif

	void InitFromDB(TSharedRef<FAngelscriptType> InType, UFunction* InFunction, const FAngelscriptMethodBind& DBBind, bool bInitTypes)
	{
		Function = InFunction;
		Declaration = DBBind.Declaration;
		WorldContextArgument = DBBind.WorldContextArgument;
		DeterminesOutputTypeArgument = DBBind.DeterminesOutputTypeArgument;
		bStaticInUnreal = DBBind.bStaticInUnreal;
		bStaticInScript = DBBind.bStaticInScript;
		bGlobalScope = DBBind.bGlobalScope;
		bNotAngelscriptProperty = DBBind.bNotAngelscriptProperty;
		bTrivial = DBBind.bTrivial;
		ClassName = DBBind.ClassName.Len() != 0 ? DBBind.ClassName : InType->GetAngelscriptTypeName();
		bAllTypesValid = bInitTypes;
		ScriptName = DBBind.ScriptName.Len() != 0 ? DBBind.ScriptName : InFunction->GetName();

		// Map all properties in the UFunction to FAngelscriptTypes
		if (bInitTypes)
		{
			for (TFieldIterator<FProperty> It(Function); It && (It->PropertyFlags & CPF_Parm); ++It)
			{
				FProperty* Property = *It;
				FAngelscriptTypeUsage Type = FAngelscriptTypeUsage::FromProperty(Property);

				if (!Type.IsValid())
				{
					bAllTypesValid = false;
					break;
				}

				if (Property->PropertyFlags & CPF_ReturnParm)
				{
					ensure(!ReturnType.IsValid());
					ReturnType = Type;
				}
				else
				{
					ArgumentTypes.Add(Type);
					ArgumentNames.Add(Property->GetName());
				}
			}
		}
	}

	void InitFromDB(
		FAngelscriptTypeDatabase& TypeDatabase,
		TSharedRef<FAngelscriptType> InType,
		UFunction* InFunction,
		const FAngelscriptMethodBind& DBBind,
		bool bInitTypes)
	{
		(void)TypeDatabase;
		InitFromDB(InType, InFunction, DBBind, bInitTypes);
	}

	void WriteToDB(FAngelscriptMethodBind& DBBind)
	{
		DBBind.Declaration = Declaration;
		DBBind.UnrealPath = Function->GetName();
		if (bStaticInUnreal)
			DBBind.ClassName = ClassName;
		DBBind.WorldContextArgument = WorldContextArgument;
		DBBind.DeterminesOutputTypeArgument = DeterminesOutputTypeArgument;
		DBBind.bStaticInUnreal = bStaticInUnreal;
		DBBind.bStaticInScript = bStaticInScript;
		DBBind.bGlobalScope = bGlobalScope;
		DBBind.bNotAngelscriptProperty = bNotAngelscriptProperty;
		DBBind.bTrivial = bTrivial;
		if (Function->HasAnyFunctionFlags(FUNC_BlueprintEvent))
			DBBind.ScriptName = ScriptName;
	}

#if WITH_EDITOR
	bool IsFunctionEditorOnly() const
	{
		if (Function->HasAnyFunctionFlags(FUNC_EditorOnly))
			return true;

		if (Function->HasAnyFunctionFlags(FUNC_Static))
		{
			extern ANGELSCRIPTRUNTIME_API bool IsEditorOnlyClass(UClass* Class);
			UClass* Class = Function->GetOuterUClass();
			if (Class != nullptr && IsEditorOnlyClass(Class))
				return true;
		}

		return false;
	}
#endif

	void ModifyScriptFunction(int FunctionId)
	{
#if !WITH_EDITOR
		if (WorldContextArgument != -1 || bNotAngelscriptProperty || bBlueprintProtected || DeterminesOutputTypeArgument != -1)
#endif
		{
			auto* ScriptFunction = (asCScriptFunction*)FAngelscriptEngine::Get().GetScriptEngine()->GetFunctionById(FunctionId);
			if (ScriptFunction != nullptr)
			{
				if (WorldContextArgument != -1)
				{
					ScriptFunction->hiddenArgumentIndex = WorldContextArgument;
					ScriptFunction->hiddenArgumentDefault = "__WorldContext()";
#if WITH_EDITOR
					if (!Function->HasMetaData(NAME_OptionalWorldContext) && !Function->HasMetaData(NAME_CallableWithoutWorldContext))
						ScriptFunction->traits.SetTrait(asTRAIT_USES_WORLDCONTEXT, true);
#endif
				}
				
				if (DeterminesOutputTypeArgument != -1)
				{
					ScriptFunction->determinesOutputTypeArgumentIndex = DeterminesOutputTypeArgument;
				}

				if (bNotAngelscriptProperty)
				{
					ScriptFunction->SetProperty(false);
				}

				if (bBlueprintProtected)
				{
					ScriptFunction->SetProtected(true);
				}

#if WITH_EDITOR
				if (bDeprecated)
				{
					ScriptFunction->traits.SetTrait(asTRAIT_DEPRECATED, true);
					ScriptFunction->deprecationMessage = TCHAR_TO_UTF8(*DeprecationMessage);
				}

				if (IsFunctionEditorOnly())
				{
					ScriptFunction->traits.SetTrait(asTRAIT_EDITOR_ONLY, true);
				}

				const FString* UnsafeDuringConstructionMeta = Function->FindMetaData(NAME_UnsafeDuringActorConstruction);
				if (UnsafeDuringConstructionMeta != nullptr && *UnsafeDuringConstructionMeta != TEXT("false"))
				{
					ScriptFunction->traits.SetTrait(asTRAIT_UNSAFE_DURING_CONSTRUCTION, true);
				}
#endif
			}
		}

#if WITH_EDITOR
		FAngelscriptDocs::AddUnrealDocumentation(
			FunctionId,
			Function->GetMetaData(NAME_Signature_ToolTip),
			Function->GetMetaData(NAME_Signature_Category),
			Function);

		FString ScriptTooltip;
		/*if (ArgumentTypes.Num() > 0)
		{
			for (int32 i = 0, Count = ArgumentTypes.Num(); i < Count; ++i)
			{
				ScriptTooltip += FString::Printf(TEXT("%s %s;\n"),
					*ArgumentTypes[i].GetAngelscriptDeclaration(),
					*ArgumentNames[i]);
			}
			ScriptTooltip += TEXT("\n");
		}*/

		if (!bStaticInScript)
			ScriptTooltip += FString::Printf(TEXT("%s Target;\n"), *ClassName);
		if (ReturnType.IsValid())
		{
			ScriptTooltip += ReturnType.GetAngelscriptDeclaration();
			ScriptTooltip += TEXT(" ReturnValue = ");
		}

		if (bStaticInScript)
		{
			ScriptTooltip += ClassName;
			ScriptTooltip += TEXT("::");
		}
		else
		{
			ScriptTooltip += TEXT("Target.");
		}

		ScriptTooltip += ScriptName;
		ScriptTooltip += TEXT("(");
		for (int32 i = 0, Count = ArgumentTypes.Num(); i < Count; ++i)
		{
			if (i != 0)
				ScriptTooltip += TEXT(", ");
			ScriptTooltip += ArgumentNames[i];
		}
		ScriptTooltip += TEXT(");");

		Function->SetMetaData(NAME_AS_Tooltip, *ScriptTooltip);
#endif
	}
};
