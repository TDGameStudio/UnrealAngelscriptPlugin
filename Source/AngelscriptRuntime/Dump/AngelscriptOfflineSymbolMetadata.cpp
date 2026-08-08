#include "AngelscriptOfflineSymbolMetadata.h"

#include "AngelscriptInclude.h"
#include "Binds/Helper_FunctionSignature.h"
#include "ClassGenerator/ASClass.h"
#include "Core/AngelscriptBindDatabase.h"
#include "Core/AngelscriptBinds.h"
#include "Core/AngelscriptDocs.h"
#include "Core/AngelscriptEngine.h"
#include "Core/AngelscriptType.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "UObject/Class.h"
#include "UObject/UnrealType.h"

namespace AngelscriptOfflineContract
{
	namespace
	{
		struct FUnrealOrigin
		{
			FString Module;
			FString Plugin;
			bool bProjectModule = false;
		};

		void AddUniqueFlag(TArray<FString>& Flags, const TCHAR* Flag)
		{
			Flags.AddUnique(Flag);
			Flags.Sort();
		}

		FString GetScriptModuleName(const UObject& Object)
		{
			const UPackage* Package = Object.GetOutermost();
			if (Package == nullptr)
			{
				return FString();
			}

			const FString PackageName = Package->GetName();
			constexpr FStringView ScriptPrefix(TEXT("/Script/"));
			return PackageName.StartsWith(ScriptPrefix)
				? PackageName.RightChop(ScriptPrefix.Len())
				: FString();
		}

		const TMap<FName, FString>& GetEnabledPluginModules()
		{
			static const TMap<FName, FString> Modules = []
			{
				TMap<FName, FString> Result;
				for (const TSharedRef<IPlugin>& Plugin :
					IPluginManager::Get().GetEnabledPlugins())
				{
					for (const FModuleDescriptor& Module :
						Plugin->GetDescriptor().Modules)
					{
						Result.Add(Module.Name, Plugin->GetName());
					}
				}
				return Result;
			}();
			return Modules;
		}

		FUnrealOrigin ResolveUnrealOrigin(const UObject& Object)
		{
			FUnrealOrigin Result;
			Result.Module = GetScriptModuleName(Object);
			if (Result.Module.IsEmpty())
			{
				const FString PackageName =
					Object.GetOutermost() != nullptr
						? Object.GetOutermost()->GetName()
						: FString();
				Result.bProjectModule =
					PackageName.StartsWith(TEXT("/Game/"))
					|| PackageName == TEXT("/Game");
				return Result;
			}

			if (const FString* Plugin =
				GetEnabledPluginModules().Find(FName(*Result.Module)))
			{
				Result.Plugin = *Plugin;
				return Result;
			}

			FString ModuleFilename;
			FModuleManager::Get().ModuleExists(
				*Result.Module,
				&ModuleFilename);
			Result.bProjectModule =
				!ModuleFilename.IsEmpty()
				&& FPaths::IsUnderDirectory(
					FPaths::ConvertRelativePathToFull(ModuleFilename),
					FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()));
			return Result;
		}

		bool IsBlueprintGenerated(const UObject& Object)
		{
#if WITH_EDITORONLY_DATA
			const UClass* Class = Cast<UClass>(&Object);
			return Class != nullptr && Class->ClassGeneratedBy != nullptr;
#else
			return false;
#endif
		}

		void ApplyScopeOrigin(
			FOriginRecord& Origin,
			const UObject& Object,
			const FUnrealOrigin& UnrealOrigin)
		{
			Origin.Module = UnrealOrigin.Module;
			Origin.Plugin = UnrealOrigin.Plugin;
			if (Origin.Kind != EOriginKind::Unknown)
			{
				return;
			}
			if (IsBlueprintGenerated(Object))
			{
				Origin.Kind = EOriginKind::Blueprint;
			}
			else if (!UnrealOrigin.Plugin.IsEmpty()
				&& UnrealOrigin.Plugin != TEXT("Angelscript"))
			{
				Origin.Kind = EOriginKind::OptionalPlugin;
			}
			else if (UnrealOrigin.bProjectModule)
			{
				Origin.Kind = EOriginKind::Project;
			}
		}

		EAvailability GetAvailability(const UStruct& Struct)
		{
			if (const UClass* Class = Cast<UClass>(&Struct))
			{
				if (Class->HasAnyClassFlags(CLASS_Deprecated))
				{
					return EAvailability::Deprecated;
				}
				if (Class->HasAnyClassFlags(CLASS_NewerVersionExists))
				{
					return EAvailability::Unavailable;
				}
			}
#if WITH_EDITORONLY_DATA
			if (Struct.HasMetaData(TEXT("BlueprintInternalUseOnly")))
			{
				return EAvailability::Internal;
			}
#endif
			return EAvailability::Available;
		}

		EAvailability GetAvailability(const UFunction& Function)
		{
#if WITH_EDITORONLY_DATA
			if (Function.HasMetaData(TEXT("DeprecatedFunction")))
			{
				return EAvailability::Deprecated;
			}
#endif
			if (Function.HasAnyFunctionFlags(FUNC_EditorOnly))
			{
				return EAvailability::EditorOnly;
			}
#if WITH_EDITORONLY_DATA
			if (Function.HasMetaData(TEXT("BlueprintInternalUseOnly")))
			{
				return EAvailability::Internal;
			}
#endif
			return EAvailability::Available;
		}

		EAvailability GetAvailability(const FProperty& Property)
		{
			if (Property.HasAnyPropertyFlags(CPF_Deprecated))
			{
				return EAvailability::Deprecated;
			}
			if (Property.HasAnyPropertyFlags(CPF_EditorOnly))
			{
				return EAvailability::EditorOnly;
			}
			return EAvailability::Available;
		}

		UObject* ResolveUnrealType(asITypeInfo& TypeInfo)
		{
			const FAngelscriptTypeUsage Usage =
				FAngelscriptTypeUsage::FromTypeId(TypeInfo.GetTypeId());
			if (UClass* Class = Usage.GetClass())
			{
				return Class;
			}
			if (UStruct* Struct = Usage.GetUnrealStruct())
			{
				return Struct;
			}

			const FString TypeName = UTF8_TO_TCHAR(TypeInfo.GetName());
			FAngelscriptEngine* CurrentEngine = FAngelscriptEngine::TryGetCurrentEngine();
			checkf(CurrentEngine != nullptr, TEXT("Offline symbol metadata requires a current engine."));
			const FAngelscriptBindDatabase* CurrentBindDatabase = CurrentEngine->GetBindDatabase();
			checkf(CurrentBindDatabase != nullptr, TEXT("Offline symbol metadata requires an engine-owned bind database."));
			const FAngelscriptBindDatabase& BindDatabase = *CurrentBindDatabase;
			for (UEnum* Enum : BindDatabase.BoundEnums)
			{
				if (Enum == nullptr)
				{
					continue;
				}
				const TSharedPtr<FAngelscriptType> BoundType =
					FAngelscriptType::GetByData(Enum);
				if (BoundType.IsValid()
					&& BoundType->GetAngelscriptTypeName() == TypeName)
				{
					return Enum;
				}
			}
			for (UDelegateFunction* Delegate :
				BindDatabase.BoundDelegateFunctions)
			{
				if (Delegate == nullptr)
				{
					continue;
				}
				const TSharedPtr<FAngelscriptType> BoundType =
					FAngelscriptType::GetByData(Delegate);
				if (BoundType.IsValid()
					&& BoundType->GetAngelscriptTypeName() == TypeName)
				{
					return Delegate;
				}
			}
			return nullptr;
		}

		EOriginKind ToContractOrigin(
			const EAngelscriptFunctionBindingOrigin Origin)
		{
			switch (Origin)
			{
			case EAngelscriptFunctionBindingOrigin::Manual:
				return EOriginKind::Manual;
			case EAngelscriptFunctionBindingOrigin::Generated:
				return EOriginKind::Generated;
			case EAngelscriptFunctionBindingOrigin::NativeModule:
				return EOriginKind::NativeModule;
			case EAngelscriptFunctionBindingOrigin::Reflective:
				return EOriginKind::Reflective;
			default:
				return EOriginKind::Unknown;
			}
		}

		UFunction* ResolveUnrealFunction(
			const FCallableRecord& Callable,
			asIScriptFunction& Function,
			const TMap<FString, UObject*>& UnrealTypesByStableId)
		{
			if (UFunction* Documented =
				FAngelscriptDocs::LookupAngelscriptFunction(Function.GetId()))
			{
				return Documented;
			}

			UClass* OwnerClass = nullptr;
			if (UObject* const* Owner =
				UnrealTypesByStableId.Find(Callable.OwnerStableId))
			{
				OwnerClass = Cast<UClass>(*Owner);
			}
			if (OwnerClass == nullptr && !Callable.Namespace.IsEmpty())
			{
				for (const TPair<FString, UObject*>& Pair :
					UnrealTypesByStableId)
				{
					UClass* Candidate = Cast<UClass>(Pair.Value);
					if (Candidate != nullptr
						&& FAngelscriptType::GetBoundClassName(Candidate)
							== Callable.Namespace)
					{
						OwnerClass = Candidate;
						break;
					}
				}
			}
			if (OwnerClass == nullptr)
			{
				return nullptr;
			}

			if (UFunction* ExactName =
				OwnerClass->FindFunctionByName(FName(*Callable.Name)))
			{
				return ExactName;
			}

			UFunction* ScriptNameMatch = nullptr;
			for (TFieldIterator<UFunction> FunctionIt(
				OwnerClass,
				EFieldIteratorFlags::IncludeSuper);
				FunctionIt;
				++FunctionIt)
			{
				UFunction* Candidate = *FunctionIt;
				if (Candidate == nullptr
					|| FAngelscriptFunctionSignature::
						GetScriptNameForFunction(Candidate)
						!= Callable.Name)
				{
					continue;
				}

				// Without an exact documentation or bind-database identity, an
				// overloaded script alias is ambiguous. Preserve Unknown instead
				// of attaching the wrong reflected function.
				if (ScriptNameMatch != nullptr
					&& ScriptNameMatch != Candidate)
				{
					return nullptr;
				}
				ScriptNameMatch = Candidate;
			}
			return ScriptNameMatch;
		}

		EOriginKind ResolveFunctionOrigin(
			const asIScriptFunction& Function,
			const UFunction* UnrealFunction)
		{
			const FAngelscriptRegisteredFunctionProvenance*
				RegisteredProvenance =
					FAngelscriptBinds::FindFunctionProvenance(Function);
			const FAngelscriptFunctionBinding* RuntimeBinding = nullptr;

			if (UnrealFunction != nullptr)
			{
				if (UClass* Class = UnrealFunction->GetOuterUClass())
				{
					if (const TMap<FString, FAngelscriptFunctionBinding>* Bindings =
						FAngelscriptBinds::GetClassFunctionBindings().Find(Class))
					{
						RuntimeBinding =
							Bindings->Find(UnrealFunction->GetName());
						if (RuntimeBinding != nullptr
							&& RuntimeBinding->bReflectiveFallbackBound)
						{
							return EOriginKind::Reflective;
						}
					}
				}
			}

			if (RegisteredProvenance != nullptr)
			{
				const EOriginKind RegisteredOrigin =
					ToContractOrigin(
						RegisteredProvenance->Origin);
				if (RegisteredOrigin != EOriginKind::Unknown)
				{
					return RegisteredOrigin;
				}
			}
			if (RuntimeBinding != nullptr)
			{
				return ToContractOrigin(RuntimeBinding->Origin);
			}
			return EOriginKind::Unknown;
		}

		FString NormalizeResourceKind(FString Value)
		{
			Value.TrimStartAndEndInline();
			Value.ToLowerInline();
			Value.ReplaceInline(TEXT("_"), TEXT("-"));
			if (Value == TEXT("softobject"))
			{
				return TEXT("soft-object");
			}
			if (Value == TEXT("softclass"))
			{
				return TEXT("soft-class");
			}
			if (Value == TEXT("loadobject"))
			{
				return TEXT("load-object");
			}
			if (Value == TEXT("loadclass"))
			{
				return TEXT("load-class");
			}
			return Value == TEXT("soft-object")
					|| Value == TEXT("soft-class")
					|| Value == TEXT("load-object")
					|| Value == TEXT("load-class")
				? Value
				: FString();
		}

		FString GetMarkedResourceKind(
			const UFunction& Function,
			const FProperty& Parameter)
		{
			FString Mark;
#if WITH_EDITORONLY_DATA
			Mark = Parameter.GetMetaData(TEXT("AngelscriptResourceContext"));
			if (Mark.IsEmpty())
			{
				Mark = Function.GetMetaData(
					FName(*FString::Printf(
						TEXT("AngelscriptResourceContext_%s"),
						*Parameter.GetName())));
			}
#endif
			Mark = NormalizeResourceKind(MoveTemp(Mark));
			if (!Mark.IsEmpty())
			{
				return Mark;
			}

			if (CastField<FSoftClassProperty>(&Parameter) != nullptr)
			{
				return TEXT("soft-class");
			}
			if (CastField<FSoftObjectProperty>(&Parameter) != nullptr)
			{
				return TEXT("soft-object");
			}
			if (const FStructProperty* StructProperty =
				CastField<FStructProperty>(&Parameter))
			{
				if (StructProperty->Struct != nullptr)
				{
					if (StructProperty->Struct->GetName()
						== TEXT("SoftObjectPath"))
					{
						return TEXT("soft-object");
					}
					if (StructProperty->Struct->GetName()
						== TEXT("SoftClassPath"))
					{
						return TEXT("soft-class");
					}
				}
			}
			return FString();
		}

		const UStruct* GetRequestedResourceType(
			const FProperty& Parameter)
		{
			if (const FSoftClassProperty* SoftClass =
				CastField<FSoftClassProperty>(&Parameter))
			{
				return SoftClass->MetaClass;
			}
			if (const FSoftObjectProperty* SoftObject =
				CastField<FSoftObjectProperty>(&Parameter))
			{
				return SoftObject->PropertyClass;
			}
			if (const FClassProperty* Class =
				CastField<FClassProperty>(&Parameter))
			{
				return Class->MetaClass;
			}
			if (const FObjectPropertyBase* Object =
				CastField<FObjectPropertyBase>(&Parameter))
			{
				return Object->PropertyClass;
			}
			return nullptr;
		}

		void SupplementResourceParameters(
			FCallableRecord& Callable,
			const UFunction& Function,
			const TMap<const UStruct*, FString>&
				StableIdsByUnrealStruct)
		{
			for (FParameterRecord& Parameter : Callable.Parameters)
			{
				const FProperty* UnrealParameter =
					FindFProperty<FProperty>(
						&Function,
						FName(*Parameter.Name));
				if (UnrealParameter == nullptr
					|| !UnrealParameter->HasAnyPropertyFlags(CPF_Parm)
					|| UnrealParameter->HasAnyPropertyFlags(CPF_ReturnParm))
				{
					continue;
				}

				const FString ResourceKind =
					GetMarkedResourceKind(Function, *UnrealParameter);
				if (ResourceKind.IsEmpty())
				{
					continue;
				}
				Parameter.ResourceKind = ResourceKind;
				if (const UStruct* RequestedType =
					GetRequestedResourceType(*UnrealParameter))
				{
					if (const FString* StableId =
						StableIdsByUnrealStruct.Find(RequestedType))
					{
						Parameter.ResourceTypeStableId = *StableId;
					}
				}
			}
		}
	}

	void SupplementWithCurrentUnrealMetadata(
		asIScriptEngine& ScriptEngine,
		const FObservedHostMetadata& ObservedMetadata,
		TArray<FSymbolRecord>& Symbols)
	{
		FAngelscriptEngine* CurrentEngine =
			FAngelscriptEngine::TryGetCurrentEngine();
		if (CurrentEngine == nullptr
			|| CurrentEngine->GetScriptEngine() != &ScriptEngine)
		{
			return;
		}

		TMap<FString, UObject*> UnrealTypesByStableId;
		TMap<const UStruct*, FString> StableIdsByUnrealStruct;
		for (const TPair<FString, asITypeInfo*>& Pair :
			ObservedMetadata.TypesByStableId)
		{
			if (Pair.Value == nullptr)
			{
				continue;
			}
			if (UObject* UnrealType = ResolveUnrealType(*Pair.Value))
			{
				UnrealTypesByStableId.Add(Pair.Key, UnrealType);
				if (const UStruct* Struct = Cast<UStruct>(UnrealType))
				{
					StableIdsByUnrealStruct.Add(Struct, Pair.Key);
				}
			}
		}

		for (FSymbolRecord& Symbol : Symbols)
		{
			if (Symbol.Kind != ESymbolKind::Type
				&& Symbol.Kind != ESymbolKind::Typedef
				&& Symbol.Kind != ESymbolKind::Funcdef
				&& Symbol.Kind != ESymbolKind::Delegate)
			{
				continue;
			}
			UObject* const* Found =
				UnrealTypesByStableId.Find(Symbol.StableId);
			if (Found == nullptr || *Found == nullptr)
			{
				continue;
			}

			UObject& UnrealType = **Found;
			Symbol.Type.UETypePath = UnrealType.GetPathName();
			const FUnrealOrigin UnrealOrigin =
				ResolveUnrealOrigin(UnrealType);
			ApplyScopeOrigin(Symbol.Type.Origin, UnrealType, UnrealOrigin);
			Symbol.Origin = Symbol.Type.Origin;

			if (UStruct* Struct = Cast<UStruct>(&UnrealType))
			{
				Symbol.Type.Availability = GetAvailability(*Struct);
				if (const UStruct* Super = Struct->GetSuperStruct())
				{
					if (const FString* SuperStableId =
						StableIdsByUnrealStruct.Find(Super))
					{
						Symbol.Type.BaseStableId = *SuperStableId;
					}
				}
				if (const UClass* Class = Cast<UClass>(Struct))
				{
					if (Class->HasAnyClassFlags(CLASS_Abstract))
					{
						AddUniqueFlag(Symbol.Type.Flags, TEXT("ue-abstract"));
						Symbol.Type.Traits.bConstructible = false;
					}
					if (Class->HasAnyClassFlags(CLASS_Interface))
					{
						AddUniqueFlag(Symbol.Type.Flags, TEXT("ue-interface"));
					}
					if (Class->HasAnyClassFlags(CLASS_Deprecated))
					{
						AddUniqueFlag(Symbol.Type.Flags, TEXT("ue-deprecated"));
					}
#if WITH_EDITORONLY_DATA
					if (Class->ClassGeneratedBy != nullptr)
					{
						AddUniqueFlag(
							Symbol.Type.Flags,
							TEXT("ue-blueprint-generated"));
					}
#endif
					for (const FImplementedInterface& Interface :
						Class->Interfaces)
					{
						if (const FString* InterfaceStableId =
							StableIdsByUnrealStruct.Find(
								Interface.Class))
						{
							Symbol.Type.InterfaceStableIds.AddUnique(
								*InterfaceStableId);
						}
					}
					Symbol.Type.InterfaceStableIds.Sort();
				}
			}
		}

		for (FSymbolRecord& Symbol : Symbols)
		{
			if (Symbol.Kind == ESymbolKind::Callable)
			{
				asIScriptFunction* const* FoundFunction =
					ObservedMetadata.FunctionsByStableId.Find(
						Symbol.StableId);
				if (FoundFunction == nullptr || *FoundFunction == nullptr)
				{
					continue;
				}
				asIScriptFunction& Function = **FoundFunction;
				UFunction* UnrealFunction = ResolveUnrealFunction(
					Symbol.Callable,
					Function,
					UnrealTypesByStableId);
				if (UnrealFunction == nullptr)
				{
					continue;
				}

				Symbol.Callable.UEFunctionPath =
					UnrealFunction->GetPathName();
				Symbol.Callable.Availability =
					GetAvailability(*UnrealFunction);
				SupplementResourceParameters(
					Symbol.Callable,
					*UnrealFunction,
					StableIdsByUnrealStruct);
				Symbol.Callable.Origin.Kind =
					ResolveFunctionOrigin(Function, UnrealFunction);
				const FUnrealOrigin UnrealOrigin =
					ResolveUnrealOrigin(*UnrealFunction);
				ApplyScopeOrigin(
					Symbol.Callable.Origin,
					*UnrealFunction,
					UnrealOrigin);
				Symbol.Origin = Symbol.Callable.Origin;
			}
			else if (Symbol.Kind == ESymbolKind::Property
				|| Symbol.Kind == ESymbolKind::Global)
			{
				UObject* const* Owner =
					UnrealTypesByStableId.Find(
						Symbol.Property.OwnerStableId);
				UStruct* OwnerStruct =
					Owner != nullptr ? Cast<UStruct>(*Owner) : nullptr;
				FProperty* UnrealProperty =
					OwnerStruct != nullptr
						? FindFProperty<FProperty>(
							OwnerStruct,
							FName(*Symbol.Property.Name))
						: nullptr;
				if (UnrealProperty == nullptr)
				{
					continue;
				}
				Symbol.Property.UEPropertyPath =
					UnrealProperty->GetPathName();
				Symbol.Property.Availability =
					GetAvailability(*UnrealProperty);
				const FUnrealOrigin UnrealOrigin =
					ResolveUnrealOrigin(*OwnerStruct);
				ApplyScopeOrigin(
					Symbol.Property.Origin,
					*OwnerStruct,
					UnrealOrigin);
				Symbol.Origin = Symbol.Property.Origin;
			}
		}
	}
}
