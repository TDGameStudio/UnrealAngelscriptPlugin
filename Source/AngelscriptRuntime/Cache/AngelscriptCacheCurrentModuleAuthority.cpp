#include "Cache/AngelscriptCacheCurrentModuleAuthority.h"

#include "Cache/AngelscriptCacheEnvironment.h"
#include "Cache/AngelscriptCacheSemanticRecords.h"
#include "Cache/AngelscriptCacheStableSymbolIdentity.h"
#include "Core/AngelscriptEngine.h"

#include "as_datatype.h"
#include "as_module.h"
#include "as_objecttype.h"
#include "as_property.h"
#include "as_scriptengine.h"
#include "as_scriptfunction.h"

namespace AngelscriptCacheCurrentModuleAuthority_Private
{
	static FAngelscriptCacheCleanCaptureResult Failure(
		const EAngelscriptCacheCleanCaptureError Error,
		FString Detail)
	{
		FAngelscriptCacheCleanCaptureResult Result;
		Result.Error = Error;
		Result.Detail = MoveTemp(Detail);
		return Result;
	}

	static FAngelscriptCacheCleanCaptureResult ValidationFailure(
		const EAngelscriptCacheCleanCaptureError Error,
		const TCHAR* Operation,
		const FAngelscriptCacheValidationResult& Validation)
	{
		return Failure(Error, FString::Printf(
			TEXT("%s failed: Error=%u Class=%u Kind=%u Stage=%u Offset=%llu"),
			Operation,
			static_cast<uint32>(Validation.Error),
			static_cast<uint32>(Validation.Class),
			static_cast<uint32>(Validation.RecordKind),
			static_cast<uint32>(Validation.Stage),
			Validation.ByteOffset));
	}

	static FAngelscriptCachedDataType MakeInt32Type()
	{
		FAngelscriptCachedDataType Type;
		Type.Kind = EAngelscriptCachedDataTypeKind::Primitive;
		Type.Primitive = EAngelscriptCachedPrimitiveType::Int32;
		return Type;
	}

	static bool TryMapPrimitivePropertyDataType(
		const asCDataType& ScriptType,
		FAngelscriptCachedDataType& OutType,
		FString& OutCanonicalType)
	{
		OutType = {};
		OutCanonicalType.Reset();
		if (!ScriptType.IsPrimitive()
			|| ScriptType.IsReference()
			|| ScriptType.IsObjectHandle()
			|| ScriptType.IsAuto()
			|| ScriptType.GetTokenType() != ttInt)
		{
			return false;
		}
		OutType = MakeInt32Type();
		OutCanonicalType = TEXT("int");
		return true;
	}

	static FAngelscriptCacheV1StorageLayout GetPropertyStorageLayout(
		const asCDataType& ScriptDataType)
	{
		if (ScriptDataType.IsObjectHandle())
		{
			return FAngelscriptCacheTypeSchemaArchive::
				GetV1BuildLayoutConstants().GetObjectHandleStorageLayout();
		}
		return {
			static_cast<uint32>(ScriptDataType.GetSizeInMemoryBytes()),
			static_cast<uint32>(ScriptDataType.GetAlignment()),
		};
	}

	static bool TryMapEnvironmentPropertyDataType(
		const asCDataType& ScriptDataType,
		FAngelscriptCachedDataType& OutType,
		FString& OutCanonicalType,
		FAngelscriptCacheSemanticDependency& OutLayoutDependency)
	{
		OutType = {};
		OutCanonicalType.Reset();
		OutLayoutDependency = {};
		asCTypeInfo* TypeInfo = ScriptDataType.GetTypeInfo();
		FAngelscriptCacheStableReference EnvironmentReference;
		if (TypeInfo == nullptr || TypeInfo->module != nullptr
			|| ScriptDataType.IsReference() || ScriptDataType.IsAuto()
			|| !FAngelscriptCacheEnvironmentIdentity::TryBuildTypeReference(
				*TypeInfo, EnvironmentReference))
		{
			return false;
		}

		OutType.Kind = EAngelscriptCachedDataTypeKind::EnvironmentType;
		OutType.TypeReference = EnvironmentReference;
		if (ScriptDataType.IsObjectConst())
		{
			OutType.QualifierFlags |= static_cast<uint32>(
				EAngelscriptCachedTypeQualifierFlags::ObjectConst);
		}
		if (ScriptDataType.IsObjectHandle())
		{
			OutType.QualifierFlags |= static_cast<uint32>(
				EAngelscriptCachedTypeQualifierFlags::ObjectHandle);
		}
		if (ScriptDataType.IsHandleToConst())
		{
			OutType.QualifierFlags |= static_cast<uint32>(
				EAngelscriptCachedTypeQualifierFlags::ConstHandle);
		}
		if (ScriptDataType.HasIfHandleThenConst())
		{
			OutType.QualifierFlags |= static_cast<uint32>(
				EAngelscriptCachedTypeQualifierFlags::IfHandleThenConst);
		}

		const asCString Formatted = ScriptDataType.Format(nullptr, true, false);
		OutCanonicalType = UTF8_TO_TCHAR(Formatted.AddressOf());
		if (OutCanonicalType.IsEmpty())
		{
			return false;
		}
		OutLayoutDependency.Kind = ScriptDataType.IsObjectHandle()
			? EAngelscriptCacheSemanticDependencyKind::EnvironmentAbi
			: EAngelscriptCacheSemanticDependencyKind::ValueLayout;
		OutLayoutDependency.Target = EnvironmentReference;
		if (!ScriptDataType.IsObjectHandle())
		{
			OutLayoutDependency.ExpectedContentOrValue =
				EnvironmentReference.ExpectedAbi;
		}
		return true;
	}

	static FAngelscriptHash256 HashStrings(
		const FStringView Domain,
		const TConstArrayView<FString> Values)
	{
		FAngelscriptArtifactCanonicalWriter Writer(Domain);
		Writer.WriteUInt32(static_cast<uint32>(Values.Num()));
		for (const FString& Value : Values)
		{
			Writer.WriteString(Value);
		}
		return Writer.FinalizeHash();
	}

	static FAngelscriptHash256 HashOneString(
		const FStringView Domain,
		const FStringView Value)
	{
		FAngelscriptArtifactCanonicalWriter Writer(Domain);
		Writer.WriteString(Value);
		return Writer.FinalizeHash();
	}

	static FAngelscriptCacheStableReference BuildCodeRootReference(
		const UClass& CodeRoot)
	{
		TArray<FString> AbiInputs;
		AbiInputs.Add(CodeRoot.GetPathName());
		AbiInputs.Add(CodeRoot.GetSuperClass() != nullptr
			? CodeRoot.GetSuperClass()->GetPathName() : FString());
		return {
			EAngelscriptCacheReferenceKind::EnvironmentSymbol,
			HashOneString(
				TEXT("cache-v2-environment-code-root-key-v1"),
				CodeRoot.GetPathName()),
			HashStrings(
				TEXT("cache-v2-environment-code-root-declaration-abi-v1"),
				AbiInputs),
		};
	}

	static void AppendMetadata(
		const TMap<FName, FString>& Source,
		TArray<FAngelscriptCachedMetadataEntry>& OutMetadata)
	{
		for (const TPair<FName, FString>& Pair : Source)
		{
			OutMetadata.Add({Pair.Key.ToString(), Pair.Value});
		}
		OutMetadata.Sort([](
			const FAngelscriptCachedMetadataEntry& Left,
			const FAngelscriptCachedMetadataEntry& Right)
		{
			if (Left.CanonicalKey != Right.CanonicalKey)
			{
				return Left.CanonicalKey < Right.CanonicalKey;
			}
			return Left.CanonicalValue < Right.CanonicalValue;
		});
	}

	static TOptional<EAngelscriptArtifactEntityKind> MapFunctionEntityKind(
		const asEBuildArtifactInvocationKind Kind)
	{
		switch (Kind)
		{
		case asBUILD_ARTIFACT_INVOCATION_GLOBAL_FUNCTION:
			return EAngelscriptArtifactEntityKind::GlobalFunction;
		case asBUILD_ARTIFACT_INVOCATION_METHOD:
			return EAngelscriptArtifactEntityKind::Method;
		case asBUILD_ARTIFACT_INVOCATION_CONSTRUCTOR:
			return EAngelscriptArtifactEntityKind::Constructor;
		case asBUILD_ARTIFACT_INVOCATION_DESTRUCTOR:
			return EAngelscriptArtifactEntityKind::Destructor;
		case asBUILD_ARTIFACT_INVOCATION_FACTORY:
			return EAngelscriptArtifactEntityKind::Factory;
		case asBUILD_ARTIFACT_INVOCATION_GENERATED_DEFAULT_CONSTRUCTOR:
			return EAngelscriptArtifactEntityKind::GeneratedDefaultConstructor;
		case asBUILD_ARTIFACT_INVOCATION_GENERATED_DEFAULT_DESTRUCTOR:
			return EAngelscriptArtifactEntityKind::GeneratedDefaultDestructor;
		case asBUILD_ARTIFACT_INVOCATION_INIT_DEFAULTS:
			return EAngelscriptArtifactEntityKind::InitDefaults;
		default:
			return {};
		}
	}

	static uint32 BuildFunctionDeclarationTraitFlags(
		const asCScriptFunction& Function)
	{
		uint32 Flags = 0;
		auto AddTrait = [&Flags, &Function](
			const asEFuncTrait Source,
			const EAngelscriptCachedDeclarationTraitFlags Target)
		{
			if (Function.traits.GetTrait(Source))
			{
				Flags |= static_cast<uint32>(Target);
			}
		};
		AddTrait(asTRAIT_CONST,
			EAngelscriptCachedDeclarationTraitFlags::Const);
		AddTrait(asTRAIT_PRIVATE,
			EAngelscriptCachedDeclarationTraitFlags::Private);
		AddTrait(asTRAIT_PROTECTED,
			EAngelscriptCachedDeclarationTraitFlags::Protected);
		AddTrait(asTRAIT_FINAL,
			EAngelscriptCachedDeclarationTraitFlags::Final);
		AddTrait(asTRAIT_OVERRIDE,
			EAngelscriptCachedDeclarationTraitFlags::Override);
		AddTrait(asTRAIT_GENERATED_FUNCTION,
			EAngelscriptCachedDeclarationTraitFlags::Generated);
		AddTrait(asTRAIT_SHARED,
			EAngelscriptCachedDeclarationTraitFlags::Shared);
		AddTrait(asTRAIT_EXTERNAL,
			EAngelscriptCachedDeclarationTraitFlags::External);
		AddTrait(asTRAIT_PROPERTY,
			EAngelscriptCachedDeclarationTraitFlags::Property);
		AddTrait(asTRAIT_IMPLICITCONSTRUCTOR,
			EAngelscriptCachedDeclarationTraitFlags::ImplicitConstructor);
		AddTrait(asTRAIT_MIXIN,
			EAngelscriptCachedDeclarationTraitFlags::Mixin);
		AddTrait(asTRAIT_LOCAL,
			EAngelscriptCachedDeclarationTraitFlags::Local);
		AddTrait(asTRAIT_NODISCARD,
			EAngelscriptCachedDeclarationTraitFlags::NoDiscard);
		AddTrait(asTRAIT_DEPRECATED,
			EAngelscriptCachedDeclarationTraitFlags::Deprecated);
		AddTrait(asTRAIT_GENERIC_TEMPLATE_FUNCTION,
			EAngelscriptCachedDeclarationTraitFlags::GenericTemplateFunction);
		AddTrait(asTRAIT_USES_WORLDCONTEXT,
			EAngelscriptCachedDeclarationTraitFlags::UsesWorldContext);
		AddTrait(asTRAIT_ACCEPT_TEMPORARY_OBJECT,
			EAngelscriptCachedDeclarationTraitFlags::AcceptTemporaryObject);
		AddTrait(asTRAIT_NOT_CALLABLE,
			EAngelscriptCachedDeclarationTraitFlags::NotCallable);
		AddTrait(asTRAIT_FORCE_CONST_ARGUMENT_EXPRESSIONS,
			EAngelscriptCachedDeclarationTraitFlags::ForceConstArgumentExpressions);
		AddTrait(asTRAIT_EXTERNAL_IMPLICIT_THIS,
			EAngelscriptCachedDeclarationTraitFlags::ExternalImplicitThis);
		AddTrait(asTRAIT_ALLOWDISCARD,
			EAngelscriptCachedDeclarationTraitFlags::AllowDiscard);
		AddTrait(asTRAIT_EDITOR_ONLY,
			EAngelscriptCachedDeclarationTraitFlags::EditorOnly);
		AddTrait(asTRAIT_EXPLICIT,
			EAngelscriptCachedDeclarationTraitFlags::Explicit);
		AddTrait(asTRAIT_UNSAFE_DURING_CONSTRUCTION,
			EAngelscriptCachedDeclarationTraitFlags::UnsafeDuringConstruction);
		AddTrait(asTRAIT_DEFAULTS_ONLY,
			EAngelscriptCachedDeclarationTraitFlags::DefaultsOnly);
		AddTrait(asTRAIT_CONSTRUCTOR,
			EAngelscriptCachedDeclarationTraitFlags::Constructor);
		AddTrait(asTRAIT_DESTRUCTOR,
			EAngelscriptCachedDeclarationTraitFlags::Destructor);
		return Flags;
	}

	static uint32 BuildClassReflectionFlags(
		const FAngelscriptClassDesc& Class)
	{
		uint32 Flags = 0;
		const auto Add = [&Flags](
			const bool bSet,
			const EAngelscriptCachedClassReflectionFlags Flag)
		{
			if (bSet)
			{
				Flags |= static_cast<uint32>(Flag);
			}
		};
		Add(Class.bSuperIsCodeClass,
			EAngelscriptCachedClassReflectionFlags::SuperIsCodeClass);
		Add(Class.bIsStaticsClass,
			EAngelscriptCachedClassReflectionFlags::StaticsClass);
		Add(Class.bAbstract,
			EAngelscriptCachedClassReflectionFlags::Abstract);
		Add(Class.bTransient,
			EAngelscriptCachedClassReflectionFlags::Transient);
		Add(Class.bHideDropdown,
			EAngelscriptCachedClassReflectionFlags::HideDropdown);
		Add(Class.bDefaultToInstanced,
			EAngelscriptCachedClassReflectionFlags::DefaultToInstanced);
		Add(Class.bEditInlineNew,
			EAngelscriptCachedClassReflectionFlags::EditInlineNew);
		Add(Class.bIsDeprecatedClass,
			EAngelscriptCachedClassReflectionFlags::Deprecated);
		Add(Class.bPlaceable,
			EAngelscriptCachedClassReflectionFlags::Placeable);
		Add(Class.bIsStruct,
			EAngelscriptCachedClassReflectionFlags::IsStruct);
		return Flags;
	}

	static uint32 BuildPropertySemanticFlags(
		const FAngelscriptPropertyDesc* Property)
	{
		if (Property == nullptr)
		{
			return 0;
		}
		uint32 Flags = 0;
		const auto Add = [&Flags](
			const bool bSet,
			const EAngelscriptCachedPropertySemanticFlags Flag)
		{
			if (bSet)
			{
				Flags |= static_cast<uint32>(Flag);
			}
		};
		// Descriptor presence is the cold-start reflected-property authority. The
		// mutable bHasUnrealProperty bit describes the currently installed UClass
		// and may be cleared during soft reload before this next class is installed.
		Add(true, EAngelscriptCachedPropertySemanticFlags::HasUnrealProperty);
		Add(Property->bBlueprintReadable,
			EAngelscriptCachedPropertySemanticFlags::BlueprintReadable);
		Add(Property->bBlueprintWritable,
			EAngelscriptCachedPropertySemanticFlags::BlueprintWritable);
		Add(Property->bEditableOnDefaults,
			EAngelscriptCachedPropertySemanticFlags::EditableOnDefaults);
		Add(Property->bEditableOnInstance,
			EAngelscriptCachedPropertySemanticFlags::EditableOnInstance);
		Add(Property->bEditConst,
			EAngelscriptCachedPropertySemanticFlags::EditConst);
		Add(Property->bInstancedReference,
			EAngelscriptCachedPropertySemanticFlags::InstancedReference);
		Add(Property->bPersistentInstance,
			EAngelscriptCachedPropertySemanticFlags::PersistentInstance);
		Add(Property->bAdvancedDisplay,
			EAngelscriptCachedPropertySemanticFlags::AdvancedDisplay);
		Add(Property->bTransient,
			EAngelscriptCachedPropertySemanticFlags::Transient);
		Add(Property->bReplicated,
			EAngelscriptCachedPropertySemanticFlags::Replicated);
		Add(Property->bSkipReplication,
			EAngelscriptCachedPropertySemanticFlags::SkipReplication);
		Add(Property->bSkipSerialization,
			EAngelscriptCachedPropertySemanticFlags::SkipSerialization);
		Add(Property->bSaveGame,
			EAngelscriptCachedPropertySemanticFlags::SaveGame);
		Add(Property->bRepNotify,
			EAngelscriptCachedPropertySemanticFlags::RepNotify);
		Add(Property->bConfig,
			EAngelscriptCachedPropertySemanticFlags::Config);
		Add(Property->bInterp,
			EAngelscriptCachedPropertySemanticFlags::Interp);
		Add(Property->bAssetRegistrySearchable,
			EAngelscriptCachedPropertySemanticFlags::AssetRegistrySearchable);
		Add(Property->bNoClear,
			EAngelscriptCachedPropertySemanticFlags::NoClear);
		return Flags;
	}

	static uint32 BuildPropertyReflectionFlags(
		const FAngelscriptPropertyDesc* Property)
	{
		if (Property == nullptr)
		{
			return 0;
		}
		uint32 Flags = 0;
		if (Property->bBlueprintReadable)
		{
			Flags |= static_cast<uint32>(
				EAngelscriptCachedReflectionFlags::BlueprintReadable);
		}
		if (Property->bBlueprintWritable)
		{
			Flags |= static_cast<uint32>(
				EAngelscriptCachedReflectionFlags::BlueprintWritable);
		}
		return Flags;
	}

	static uint32 BuildFunctionReflectionFlags(
		const FAngelscriptFunctionDesc* Function)
	{
		if (Function == nullptr)
		{
			return 0;
		}
		uint32 Flags = 0;
		auto Add = [&Flags](
			const bool bSet,
			const EAngelscriptCachedReflectionFlags Flag)
		{
			if (bSet)
			{
				Flags |= static_cast<uint32>(Flag);
			}
		};
		Add(Function->bBlueprintCallable,
			EAngelscriptCachedReflectionFlags::BlueprintCallable);
		Add(Function->bBlueprintOverride,
			EAngelscriptCachedReflectionFlags::BlueprintOverride);
		Add(Function->bBlueprintEvent,
			EAngelscriptCachedReflectionFlags::BlueprintEvent);
		Add(Function->bBlueprintPure,
			EAngelscriptCachedReflectionFlags::BlueprintPure);
		Add(Function->bNetMulticast,
			EAngelscriptCachedReflectionFlags::NetMulticast);
		Add(Function->bNetClient,
			EAngelscriptCachedReflectionFlags::NetClient);
		Add(Function->bNetServer,
			EAngelscriptCachedReflectionFlags::NetServer);
		Add(Function->bNetValidate,
			EAngelscriptCachedReflectionFlags::NetValidate);
		Add(Function->bUnreliable,
			EAngelscriptCachedReflectionFlags::Unreliable);
		Add(Function->bBlueprintAuthorityOnly,
			EAngelscriptCachedReflectionFlags::BlueprintAuthorityOnly);
		Add(Function->bExec,
			EAngelscriptCachedReflectionFlags::Exec);
		Add(Function->bCanOverrideEvent,
			EAngelscriptCachedReflectionFlags::CanOverrideEvent);
		return Flags;
	}

	static uint32 BuildFunctionDescriptorTraitFlags(
		const FAngelscriptFunctionDesc* Function)
	{
		if (Function == nullptr)
		{
			return 0;
		}
		uint32 Flags = 0;
		auto Add = [&Flags](
			const bool bSet,
			const EAngelscriptCachedDeclarationTraitFlags Flag)
		{
			if (bSet)
			{
				Flags |= static_cast<uint32>(Flag);
			}
		};
		Add(Function->bIsStatic,
			EAngelscriptCachedDeclarationTraitFlags::Static);
		Add(Function->bIsConstMethod,
			EAngelscriptCachedDeclarationTraitFlags::Const);
		Add(Function->bIsPrivate,
			EAngelscriptCachedDeclarationTraitFlags::Private);
		Add(Function->bIsProtected,
			EAngelscriptCachedDeclarationTraitFlags::Protected);
		Add(Function->bThreadSafe,
			EAngelscriptCachedDeclarationTraitFlags::ThreadSafe);
		return Flags;
	}

	static bool TryMapRootFunctionDataType(
		const asCDataType& ScriptDataType,
		const asCObjectType& RootType,
		const FAngelscriptCachedDeclaration& TypeDeclaration,
		FAngelscriptCachedDataType& OutType)
	{
		OutType = {};
		if (ScriptDataType.IsPrimitive())
		{
			OutType.Kind = EAngelscriptCachedDataTypeKind::Primitive;
			switch (ScriptDataType.GetTokenType())
			{
			case ttVoid:
				OutType.Primitive = EAngelscriptCachedPrimitiveType::Void;
				break;
			case ttBool:
				OutType.Primitive = EAngelscriptCachedPrimitiveType::Bool;
				break;
			case ttInt8:
				OutType.Primitive = EAngelscriptCachedPrimitiveType::Int8;
				break;
			case ttInt16:
				OutType.Primitive = EAngelscriptCachedPrimitiveType::Int16;
				break;
			case ttInt:
				OutType.Primitive = EAngelscriptCachedPrimitiveType::Int32;
				break;
			case ttInt64:
				OutType.Primitive = EAngelscriptCachedPrimitiveType::Int64;
				break;
			case ttUInt8:
				OutType.Primitive = EAngelscriptCachedPrimitiveType::UInt8;
				break;
			case ttUInt16:
				OutType.Primitive = EAngelscriptCachedPrimitiveType::UInt16;
				break;
			case ttUInt:
				OutType.Primitive = EAngelscriptCachedPrimitiveType::UInt32;
				break;
			case ttUInt64:
				OutType.Primitive = EAngelscriptCachedPrimitiveType::UInt64;
				break;
			case ttFloat:
			case ttFloat32:
				OutType.Primitive = EAngelscriptCachedPrimitiveType::Float32;
				break;
			case ttDouble:
			case ttFloat64:
				OutType.Primitive = EAngelscriptCachedPrimitiveType::Float64;
				break;
			default:
				return false;
			}
			if (ScriptDataType.IsReference())
			{
				OutType.QualifierFlags |= static_cast<uint32>(
					EAngelscriptCachedTypeQualifierFlags::Reference);
			}
			if (ScriptDataType.IsObjectConst())
			{
				OutType.QualifierFlags |= static_cast<uint32>(
					EAngelscriptCachedTypeQualifierFlags::ObjectConst);
			}
			return !ScriptDataType.IsObjectHandle()
				&& !ScriptDataType.IsAuto();
		}

		if (ScriptDataType.IsAuto()
			|| ScriptDataType.GetTypeInfo() != &RootType)
		{
			return false;
		}
		OutType.Kind = EAngelscriptCachedDataTypeKind::ScriptType;
		OutType.TypeReference = FAngelscriptCacheStableReference{
			EAngelscriptCacheReferenceKind::ScriptType,
			TypeDeclaration.StableKey,
			TypeDeclaration.SignatureHash,
		};
		if (ScriptDataType.IsReference())
		{
			OutType.QualifierFlags |= static_cast<uint32>(
				EAngelscriptCachedTypeQualifierFlags::Reference);
		}
		if (ScriptDataType.IsObjectConst())
		{
			OutType.QualifierFlags |= static_cast<uint32>(
				EAngelscriptCachedTypeQualifierFlags::ObjectConst);
		}
		if (ScriptDataType.IsObjectHandle())
		{
			OutType.QualifierFlags |= static_cast<uint32>(
				EAngelscriptCachedTypeQualifierFlags::ObjectHandle);
		}
		if (ScriptDataType.IsHandleToConst())
		{
			OutType.QualifierFlags |= static_cast<uint32>(
				EAngelscriptCachedTypeQualifierFlags::ConstHandle);
		}
		if (ScriptDataType.HasIfHandleThenConst())
		{
			OutType.QualifierFlags |= static_cast<uint32>(
				EAngelscriptCachedTypeQualifierFlags::IfHandleThenConst);
		}
		return true;
	}

	struct FClassGraphTypeAuthority final
	{
		const asCObjectType* ScriptType = nullptr;
		const FAngelscriptCachedDeclaration* TypeDeclaration = nullptr;
	};

	static bool TryMapClassGraphFunctionDataType(
		const asCDataType& ScriptDataType,
		const TConstArrayView<FClassGraphTypeAuthority> Authorities,
		FAngelscriptCachedDataType& OutType)
	{
		if (Authorities.IsEmpty())
		{
			return false;
		}
		if (ScriptDataType.IsPrimitive())
		{
			return TryMapRootFunctionDataType(
				ScriptDataType,
				*Authorities[0].ScriptType,
				*Authorities[0].TypeDeclaration,
				OutType);
		}
		for (const FClassGraphTypeAuthority& Authority : Authorities)
		{
			if (Authority.ScriptType != nullptr
				&& Authority.TypeDeclaration != nullptr
				&& ScriptDataType.GetTypeInfo() == Authority.ScriptType)
			{
				return TryMapRootFunctionDataType(
					ScriptDataType,
					*Authority.ScriptType,
					*Authority.TypeDeclaration,
					OutType);
			}
		}
		return false;
	}

	static bool TryMapClassGraphPropertyDataType(
		const asCDataType& ScriptDataType,
		const TConstArrayView<FClassGraphTypeAuthority> Authorities,
		FAngelscriptCachedDataType& OutType,
		FString& OutCanonicalType,
		FAngelscriptCacheSemanticDependency& OutDependency)
	{
		OutType = {};
		OutCanonicalType.Reset();
		OutDependency = {};
		if (ScriptDataType.IsReference() || ScriptDataType.IsAuto()
			|| !ScriptDataType.IsObjectHandle())
		{
			return false;
		}
		for (const FClassGraphTypeAuthority& Authority : Authorities)
		{
			if (Authority.ScriptType == nullptr
				|| Authority.TypeDeclaration == nullptr
				|| ScriptDataType.GetTypeInfo() != Authority.ScriptType
				|| !TryMapRootFunctionDataType(
					ScriptDataType,
					*Authority.ScriptType,
					*Authority.TypeDeclaration,
					OutType))
			{
				continue;
			}
			const asCString Formatted = ScriptDataType.Format(
				nullptr, true, false);
			OutCanonicalType = UTF8_TO_TCHAR(Formatted.AddressOf());
			if (OutCanonicalType.IsEmpty() || !OutType.TypeReference.IsSet())
			{
				return false;
			}
			OutDependency.Kind =
				EAngelscriptCacheSemanticDependencyKind::Declaration;
			OutDependency.Target = OutType.TypeReference.GetValue();
			return true;
		}
		return false;
	}

	static FAngelscriptCacheCleanCaptureResult BuildClassGraphMethodAuthority(
		const TSharedRef<FAngelscriptModuleDesc>& Module,
		const FAngelscriptCacheCleanCaptureOptions& Options,
		const FAngelscriptStableModuleKey& ModuleKey,
		FAngelscriptCacheCurrentModuleAuthority& OutAuthority)
	{
		asCModule* ScriptModule = Module->ScriptModule;
		if (Module->Classes.Num() < 2 || !Module->Enums.IsEmpty()
			|| !Module->Delegates.IsEmpty() || !Module->ImportedModules.IsEmpty()
			|| !Module->PostInitFunctions.IsEmpty()
			|| ScriptModule->GetObjectTypeCount()
				!= static_cast<asUINT>(Module->Classes.Num())
			|| ScriptModule->GetEnumCount() != 0
			|| ScriptModule->GetTypedefCount() != 0
			|| ScriptModule->GetImportedFunctionCount() != 0)
		{
			return Failure(EAngelscriptCacheCleanCaptureError::NotCacheable,
				TEXT("Current class-graph authority requires two or more classes and no enum, delegate, import, typedef or post-init declaration"));
		}

		struct FClassPlan final
		{
			const FAngelscriptClassDesc* ClassDesc = nullptr;
			asCObjectType* ScriptType = nullptr;
			int32 BaseClassIndex = INDEX_NONE;
			FString ClassNamespace;
			FString ClassName;
			FAngelscriptCachedDeclaration TypeDeclaration;
			FAngelscriptCachedTypeSchema TypeSchema;
			TArray<FAngelscriptCachedDeclaration> PropertyDeclarations;
			asCGlobalProperty* GeneratedStaticClassGlobal = nullptr;
			asCScriptFunction* GeneratedStaticClassFunction = nullptr;
		};

		auto FindStagingType = [ScriptModule](
			const FAngelscriptClassDesc& ClassDesc) -> asCObjectType*
		{
			asCObjectType* Match = nullptr;
			for (asUINT Index = 0;
				Index < ScriptModule->GetObjectTypeCount(); ++Index)
			{
				asCObjectType* Candidate = static_cast<asCObjectType*>(
					ScriptModule->GetObjectTypeByIndex(Index));
				if (Candidate == nullptr
					|| !ClassDesc.ClassName.Equals(
						UTF8_TO_TCHAR(Candidate->GetName()),
						ESearchCase::CaseSensitive))
				{
					continue;
				}
				const FString CandidateNamespace =
					UTF8_TO_TCHAR(Candidate->GetNamespace());
				if (ClassDesc.Namespace.IsSet()
					&& !ClassDesc.Namespace.GetValue().Equals(
						CandidateNamespace, ESearchCase::CaseSensitive))
				{
					continue;
				}
				if (Match != nullptr)
				{
					return nullptr;
				}
				Match = Candidate;
			}
			return Match;
		};

		struct FPendingClass final
		{
			const FAngelscriptClassDesc* ClassDesc = nullptr;
			asCObjectType* ScriptType = nullptr;
		};
		TArray<FPendingClass> Pending;
		Pending.Reserve(Module->Classes.Num());
		for (const TSharedRef<FAngelscriptClassDesc>& Class : Module->Classes)
		{
			asCObjectType* ScriptType = FindStagingType(Class.Get());
			if (ScriptType == nullptr
				|| Pending.ContainsByPredicate(
					[ScriptType](const FPendingClass& Existing)
					{
						return Existing.ScriptType == ScriptType;
					}))
			{
				return Failure(EAngelscriptCacheCleanCaptureError::NotCacheable,
					TEXT("A class descriptor has no unique staging VM type"));
			}
			Pending.Add({&Class.Get(), ScriptType});
		}

		TArray<FClassPlan> Classes;
		Classes.Reserve(Pending.Num());
		while (!Pending.IsEmpty())
		{
			int32 SelectedPendingIndex = INDEX_NONE;
			int32 SelectedBaseIndex = INDEX_NONE;
			FString SelectedNamespace;
			FString SelectedName;
			for (int32 PendingIndex = 0;
				PendingIndex < Pending.Num(); ++PendingIndex)
			{
				const FPendingClass& Candidate = Pending[PendingIndex];
				int32 BaseIndex = INDEX_NONE;
				if (Candidate.ScriptType->derivedFrom != nullptr)
				{
					BaseIndex = Classes.IndexOfByPredicate(
						[&Candidate](const FClassPlan& Existing)
						{
							return Existing.ScriptType
								== Candidate.ScriptType->derivedFrom;
						});
					if (BaseIndex == INDEX_NONE)
					{
						continue;
					}
				}
				const FString CandidateNamespace =
					Candidate.ClassDesc->Namespace.IsSet()
						? Candidate.ClassDesc->Namespace.GetValue()
						: UTF8_TO_TCHAR(Candidate.ScriptType->GetNamespace());
				const FString CandidateName =
					UTF8_TO_TCHAR(Candidate.ScriptType->GetName());
				const int32 NamespaceComparison =
					SelectedPendingIndex == INDEX_NONE ? -1
					: CandidateNamespace.Compare(
						SelectedNamespace, ESearchCase::CaseSensitive);
				const int32 NameComparison =
					SelectedPendingIndex == INDEX_NONE
						|| NamespaceComparison != 0 ? 0
					: CandidateName.Compare(
						SelectedName, ESearchCase::CaseSensitive);
				if (SelectedPendingIndex == INDEX_NONE
					|| NamespaceComparison < 0
					|| (NamespaceComparison == 0 && NameComparison < 0))
				{
					SelectedPendingIndex = PendingIndex;
					SelectedBaseIndex = BaseIndex;
					SelectedNamespace = CandidateNamespace;
					SelectedName = CandidateName;
				}
				else if (NamespaceComparison == 0 && NameComparison == 0)
				{
					return Failure(EAngelscriptCacheCleanCaptureError::NotCacheable,
						TEXT("The current class graph contains duplicate canonical type authority"));
				}
			}
			if (SelectedPendingIndex == INDEX_NONE)
			{
				return Failure(EAngelscriptCacheCleanCaptureError::NotCacheable,
					TEXT("The current same-module class graph is cyclic or references an unknown script base"));
			}

			const FPendingClass Selected = Pending[SelectedPendingIndex];
			if (Selected.ClassDesc->bIsStruct
				|| Selected.ClassDesc->bIsStaticsClass
				|| Selected.ClassDesc->CodeSuperClass == nullptr
				|| Selected.ScriptType->shadowType == nullptr
				|| !Selected.ClassDesc->ImplementedInterfaces.IsEmpty()
				|| !Selected.ClassDesc->ComposeOntoClass.IsEmpty()
				|| Selected.ScriptType->size <= 0
				|| Selected.ScriptType->alignment <= 0
				|| Selected.ScriptType->basePropertyOffset < 0
				|| Selected.ClassDesc->StaticClassGlobalVariableName.IsEmpty()
				|| (SelectedBaseIndex == INDEX_NONE
					? !Selected.ClassDesc->bSuperIsCodeClass
					: Selected.ClassDesc->bSuperIsCodeClass))
			{
				return Failure(EAngelscriptCacheCleanCaptureError::NotCacheable,
					TEXT("A current class is outside the same-module reflected UClass graph authority shape"));
			}
			FClassPlan& Plan = Classes.AddDefaulted_GetRef();
			Plan.ClassDesc = Selected.ClassDesc;
			Plan.ScriptType = Selected.ScriptType;
			Plan.BaseClassIndex = SelectedBaseIndex;
			Plan.ClassNamespace = MoveTemp(SelectedNamespace);
			Plan.ClassName = MoveTemp(SelectedName);
			if (!Plan.ClassName.Equals(
				Plan.ClassDesc->ClassName, ESearchCase::CaseSensitive))
			{
				return Failure(EAngelscriptCacheCleanCaptureError::NotCacheable,
					TEXT("A current class descriptor and staging VM type name disagree"));
			}
			Pending.RemoveAt(SelectedPendingIndex);
		}

		uint32 NextDeclarationSlot = 0;
		for (FClassPlan& Class : Classes)
		{
			FAngelscriptCachedDeclaration& Declaration = Class.TypeDeclaration;
			Declaration.DeclarationKind =
				EAngelscriptCacheDeclarationKind::Type;
			Declaration.EntityKind = EAngelscriptArtifactEntityKind::Class;
			Declaration.SchemaCoverage =
				EAngelscriptCacheSchemaCoverage::Required;
			Declaration.BodyCoverage =
				EAngelscriptCacheBodyCoverage::Forbidden;
			Declaration.OwnerKind = EAngelscriptFunctionOwnerKind::Module;
			Declaration.OwnerKey = ModuleKey.Hash;
			Declaration.ModuleKey = ModuleKey;
			Declaration.CanonicalNamespace = Class.ClassNamespace;
			Declaration.CanonicalName = Class.ClassName;
			Declaration.CanonicalDeclaration = FString::Printf(
				TEXT("class %s"), *Class.ClassName);
			if (Class.ClassDesc->bAbstract)
			{
				Declaration.TraitFlags |= static_cast<uint32>(
					EAngelscriptCachedDeclarationTraitFlags::Abstract);
			}
			AppendMetadata(Class.ClassDesc->Meta, Declaration.Metadata);
			Declaration.Slots.Add({
				EAngelscriptCacheDeclarationSlotKind::Declaration,
				NextDeclarationSlot++});
			FAngelscriptTypeIdentityDescriptor TypeIdentity;
			TypeIdentity.ModuleKey = ModuleKey;
			TypeIdentity.Namespace = Class.ClassNamespace;
			TypeIdentity.Kind = Declaration.EntityKind;
			TypeIdentity.CanonicalDeclaration =
				Declaration.CanonicalDeclaration;
			Declaration.StableKey =
				FAngelscriptArtifactIdentityBuilder::BuildTypeKey(
					TypeIdentity).Hash;
			const FAngelscriptCacheValidationResult IdentityResult =
				FAngelscriptCacheSemanticArchive::ComputeDeclarationHashes(
					Declaration,
					Declaration.SignatureHash,
					Declaration.TraitsHash);
			if (!IdentityResult.IsSuccess())
			{
				return ValidationFailure(
					EAngelscriptCacheCleanCaptureError::RecordEncodingFailed,
					TEXT("Current class-graph type declaration hashes"),
					IdentityResult);
			}
		}

		TArray<FClassGraphTypeAuthority> TypeAuthorities;
		TypeAuthorities.Reserve(Classes.Num());
		for (const FClassPlan& Class : Classes)
		{
			TypeAuthorities.Add({Class.ScriptType, &Class.TypeDeclaration});
		}

		FAngelscriptCacheValidationResult IdentityResult;
		for (FClassPlan& Class : Classes)
		{
			FAngelscriptCachedTypeSchema& Schema = Class.TypeSchema;
			Schema.PayloadSchemaVersion =
				FAngelscriptCacheTypeSchemaArchive::TypeSchemaPayloadSchemaVersion;
			Schema.ModuleKey = ModuleKey;
			Schema.TypeKey = FAngelscriptStableTypeKey{
				Class.TypeDeclaration.StableKey};
			Schema.TypeKind = EAngelscriptCachedTypeKind::Class;
			Schema.CanonicalNamespace = Class.ClassNamespace;
			Schema.CanonicalName = Class.ClassName;
			Schema.CanonicalDeclaration =
				Class.TypeDeclaration.CanonicalDeclaration;
			Schema.TypeSemanticFlags = static_cast<uint32>(
				EAngelscriptCachedTypeSemanticFlags::ReferenceType);
			if (Class.ClassDesc->bAbstract)
			{
				Schema.TypeSemanticFlags |= static_cast<uint32>(
					EAngelscriptCachedTypeSemanticFlags::Abstract);
			}
			Schema.Metadata = Class.TypeDeclaration.Metadata;
			Schema.Layout.SemanticSize =
				static_cast<uint64>(Class.ScriptType->size);
			Schema.Layout.SemanticAlignment =
				static_cast<uint32>(Class.ScriptType->alignment);
			Schema.Layout.BasePropertyBoundary =
				Class.BaseClassIndex != INDEX_NONE
					? static_cast<uint32>(
						Classes[Class.BaseClassIndex].ScriptType->size)
					: static_cast<uint32>(
						Class.ScriptType->basePropertyOffset);
			Schema.Reflection.ReflectionKind =
				EAngelscriptCachedReflectionKind::UClass;
			Schema.Reflection.ClassReflectionFlags =
				BuildClassReflectionFlags(*Class.ClassDesc);
			if (!Class.ClassDesc->ConfigName.IsEmpty())
			{
				Schema.Reflection.ConfigName = Class.ClassDesc->ConfigName;
			}
			Schema.Reflection.StaticClassGlobalName =
				Class.ClassDesc->StaticClassGlobalVariableName;

			if (Class.BaseClassIndex != INDEX_NONE)
			{
				const FClassPlan& Base = Classes[Class.BaseClassIndex];
				const FAngelscriptCacheStableReference BaseReference{
					EAngelscriptCacheReferenceKind::ScriptType,
					Base.TypeDeclaration.StableKey,
					Base.TypeDeclaration.SignatureHash};
				Schema.Relations.Add({
					EAngelscriptCachedTypeRelationKind::Base,
					{}, BaseReference});
				FAngelscriptCachedTypeLayoutInput BaseInput;
				BaseInput.InputKind =
					EAngelscriptCachedTypeLayoutInputKind::BaseType;
				BaseInput.Target = BaseReference;
				BaseInput.BoundaryContribution =
					static_cast<uint32>(Base.ScriptType->size);
				BaseInput.AlignmentContribution =
					static_cast<uint32>(Base.ScriptType->alignment);
				IdentityResult = FAngelscriptCacheTypeSchemaArchive::
					ComputeLayoutInputHash(
						BaseInput, BaseInput.LayoutInputHash);
				if (!IdentityResult.IsSuccess())
				{
					return ValidationFailure(
						EAngelscriptCacheCleanCaptureError::RecordEncodingFailed,
						TEXT("Current class-graph base layout input"),
						IdentityResult);
				}
				Schema.LayoutInputs.Add(BaseInput);
				Schema.Dependencies.Add({
					EAngelscriptCacheSemanticDependencyKind::Inheritance,
					BaseReference, {}});
			}

			const FAngelscriptCacheStableReference CodeRoot =
				BuildCodeRootReference(*Class.ClassDesc->CodeSuperClass);
			Schema.Relations.Add({
				EAngelscriptCachedTypeRelationKind::ShadowSuper, {}, CodeRoot});
			Schema.Relations.Add({
				EAngelscriptCachedTypeRelationKind::CodeSuper, {}, CodeRoot});
			FAngelscriptCachedTypeLayoutInput CodeInput;
			CodeInput.InputKind =
				EAngelscriptCachedTypeLayoutInputKind::CodeRoot;
			CodeInput.Target = CodeRoot;
			if (Class.BaseClassIndex == INDEX_NONE)
			{
				CodeInput.BoundaryContribution = static_cast<uint32>(
					Class.ClassDesc->CodeSuperClass->GetPropertiesSize());
			}
			CodeInput.AlignmentContribution = static_cast<uint32>(
				static_cast<const asCObjectType*>(
					Class.ScriptType->shadowType)->alignment);
			IdentityResult = FAngelscriptCacheTypeSchemaArchive::
				ComputeLayoutInputHash(CodeInput, CodeInput.LayoutInputHash);
			if (!IdentityResult.IsSuccess())
			{
				return ValidationFailure(
					EAngelscriptCacheCleanCaptureError::RecordEncodingFailed,
					TEXT("Current class-graph code-root layout input"),
					IdentityResult);
			}
			Schema.LayoutInputs.Add(CodeInput);
			Schema.Dependencies.Add({
				EAngelscriptCacheSemanticDependencyKind::EnvironmentAbi,
				CodeRoot, {}});

			TSet<const FAngelscriptPropertyDesc*> MatchedProperties;
			for (asUINT PropertyIndex = 0;
				PropertyIndex < Class.ScriptType->localProperties.GetLength();
				++PropertyIndex)
			{
				const asCObjectProperty* ScriptProperty =
					Class.ScriptType->localProperties[PropertyIndex];
				if (ScriptProperty == nullptr || ScriptProperty->isInherited
					|| ScriptProperty->byteOffset < 0)
				{
					return Failure(EAngelscriptCacheCleanCaptureError::NotCacheable,
						TEXT("A current class-graph type contains an unsupported local VM property"));
				}
				FAngelscriptCachedDataType CachedType;
				FString CanonicalType;
				FAngelscriptCacheSemanticDependency PropertyDependency;
				bool bHasPropertyDependency = false;
				if (!TryMapPrimitivePropertyDataType(
					ScriptProperty->type, CachedType, CanonicalType))
				{
					bHasPropertyDependency =
						TryMapClassGraphPropertyDataType(
							ScriptProperty->type, TypeAuthorities,
							CachedType, CanonicalType, PropertyDependency);
					if (!bHasPropertyDependency)
					{
						bHasPropertyDependency =
							TryMapEnvironmentPropertyDataType(
								ScriptProperty->type, CachedType,
								CanonicalType, PropertyDependency);
					}
					if (!bHasPropertyDependency)
					{
						return Failure(
							EAngelscriptCacheCleanCaptureError::NotCacheable,
							TEXT("A current class-graph property is outside the stable primitive/script-handle/environment table"));
					}
				}

				const FString PropertyName =
					UTF8_TO_TCHAR(ScriptProperty->name.AddressOf());
				const FAngelscriptPropertyDesc* ReflectedProperty = nullptr;
				for (const TSharedRef<FAngelscriptPropertyDesc>& Candidate
					: Class.ClassDesc->Properties)
				{
					if (Candidate->ScriptPropertyIndex
							== static_cast<int32>(PropertyIndex)
						|| Candidate->PropertyName.Equals(
							PropertyName, ESearchCase::CaseSensitive))
					{
						if (ReflectedProperty != nullptr)
						{
							return Failure(
								EAngelscriptCacheCleanCaptureError::NotCacheable,
								TEXT("More than one reflected descriptor maps to a current class-graph property"));
						}
						ReflectedProperty = &Candidate.Get();
						MatchedProperties.Add(ReflectedProperty);
					}
				}

				FAngelscriptCachedDeclaration PropertyDeclaration;
				PropertyDeclaration.DeclarationKind =
					EAngelscriptCacheDeclarationKind::Property;
				PropertyDeclaration.EntityKind =
					EAngelscriptArtifactEntityKind::Property;
				PropertyDeclaration.SchemaCoverage =
					EAngelscriptCacheSchemaCoverage::Forbidden;
				PropertyDeclaration.BodyCoverage =
					EAngelscriptCacheBodyCoverage::Forbidden;
				PropertyDeclaration.OwnerKind =
					EAngelscriptFunctionOwnerKind::Type;
				PropertyDeclaration.OwnerKey =
					Class.TypeDeclaration.StableKey;
				PropertyDeclaration.ModuleKey = ModuleKey;
				PropertyDeclaration.CanonicalNamespace = Class.ClassNamespace;
				PropertyDeclaration.CanonicalName = PropertyName;
				PropertyDeclaration.CanonicalTypeSpelling = CanonicalType;
				PropertyDeclaration.CanonicalDeclaration = FString::Printf(
					TEXT("%s %s"), *CanonicalType, *PropertyName);
				PropertyDeclaration.DeclaredType = CachedType;
				if (ScriptProperty->isPrivate)
				{
					PropertyDeclaration.TraitFlags |= static_cast<uint32>(
						EAngelscriptCachedDeclarationTraitFlags::Private);
				}
				if (ScriptProperty->isProtected)
				{
					PropertyDeclaration.TraitFlags |= static_cast<uint32>(
						EAngelscriptCachedDeclarationTraitFlags::Protected);
				}
				PropertyDeclaration.ReflectionFlags =
					BuildPropertyReflectionFlags(ReflectedProperty);
				if (ReflectedProperty != nullptr)
				{
					AppendMetadata(
						ReflectedProperty->Meta,
						PropertyDeclaration.Metadata);
				}
				PropertyDeclaration.Slots.Add({
					EAngelscriptCacheDeclarationSlotKind::Declaration,
					NextDeclarationSlot++});
				FAngelscriptPropertyIdentityDescriptor PropertyIdentity;
				PropertyIdentity.OwnerTypeKey = Schema.TypeKey;
				PropertyIdentity.Kind = PropertyDeclaration.EntityKind;
				PropertyIdentity.Name = PropertyName;
				PropertyIdentity.CanonicalType = CanonicalType;
				PropertyDeclaration.StableKey =
					FAngelscriptArtifactIdentityBuilder::BuildPropertyKey(
						PropertyIdentity).Hash;
				IdentityResult = FAngelscriptCacheSemanticArchive::
					ComputeDeclarationHashes(
						PropertyDeclaration,
						PropertyDeclaration.SignatureHash,
						PropertyDeclaration.TraitsHash);
				if (!IdentityResult.IsSuccess())
				{
					return ValidationFailure(
						EAngelscriptCacheCleanCaptureError::RecordEncodingFailed,
						TEXT("Current class-graph property declaration hashes"),
						IdentityResult);
				}

				FAngelscriptCachedPropertySchema PropertySchema;
				PropertySchema.LayoutOrdinal = PropertyIndex;
				PropertySchema.SemanticByteOffset =
					static_cast<uint32>(ScriptProperty->byteOffset);
				PropertySchema.PropertyKey = FAngelscriptStablePropertyKey{
					PropertyDeclaration.StableKey};
				PropertySchema.CanonicalName = PropertyName;
				PropertySchema.Type = CachedType;
				PropertySchema.StorageKind = ScriptProperty->type.IsObjectHandle()
					? EAngelscriptCachedPropertyStorageKind::ObjectHandle
					: EAngelscriptCachedPropertyStorageKind::InlineValue;
				const FAngelscriptCacheV1StorageLayout PropertyStorage =
					GetPropertyStorageLayout(ScriptProperty->type);
				PropertySchema.SemanticStorageSize =
					PropertyStorage.SemanticStorageSize;
				PropertySchema.SemanticStorageAlignment =
					PropertyStorage.SemanticStorageAlignment;
				PropertySchema.Access = ScriptProperty->isPrivate
					? EAngelscriptCachedMemberAccess::Private
					: ScriptProperty->isProtected
						? EAngelscriptCachedMemberAccess::Protected
						: EAngelscriptCachedMemberAccess::Public;
				PropertySchema.PropertySemanticFlags =
					BuildPropertySemanticFlags(ReflectedProperty);
				PropertySchema.ReplicationCondition = ReflectedProperty != nullptr
					? static_cast<EAngelscriptCachedReplicationCondition>(
						ReflectedProperty->ReplicationCondition.GetValue())
					: EAngelscriptCachedReplicationCondition::None;
				PropertySchema.Metadata = PropertyDeclaration.Metadata;
				IdentityResult = FAngelscriptCacheTypeSchemaArchive::
					ComputeStorageLayoutHash(
						PropertySchema.Type, PropertySchema.StorageKind,
						PropertySchema.SemanticStorageSize,
						PropertySchema.SemanticStorageAlignment,
						PropertySchema.StorageLayoutHash);
				if (IdentityResult.IsSuccess())
				{
					IdentityResult = FAngelscriptCacheTypeSchemaArchive::
						ComputePropertyLayoutFingerprint(
							Schema.TypeKey, PropertySchema,
							PropertySchema.PropertyLayoutFingerprint);
				}
				if (!IdentityResult.IsSuccess())
				{
					return ValidationFailure(
						EAngelscriptCacheCleanCaptureError::RecordEncodingFailed,
						TEXT("Current class-graph property layout hashes"),
						IdentityResult);
				}
				Class.PropertyDeclarations.Add(MoveTemp(PropertyDeclaration));
				Schema.OrderedProperties.Add(MoveTemp(PropertySchema));
				if (bHasPropertyDependency)
				{
					Schema.Dependencies.AddUnique(
						MoveTemp(PropertyDependency));
				}
			}
			if (MatchedProperties.Num() != Class.ClassDesc->Properties.Num())
			{
				return Failure(EAngelscriptCacheCleanCaptureError::NotCacheable,
					TEXT("A reflected current class-graph property has no matching local VM property"));
			}
		}

		OutAuthority.ModuleState.PayloadSchemaVersion =
			FAngelscriptCacheRemainingRecordArchive::ModuleStatePayloadSchemaVersion;
		OutAuthority.ModuleState.ModuleKey = ModuleKey;
		OutAuthority.ModuleState.Profile = Options.Profile;
		IdentityResult = FAngelscriptCacheRemainingRecordArchive::
			ComputeModuleStateInputHash(
				OutAuthority.ModuleState,
				OutAuthority.ModuleState.StateInputHash);
		if (!IdentityResult.IsSuccess())
		{
			return ValidationFailure(
				EAngelscriptCacheCleanCaptureError::RecordEncodingFailed,
				TEXT("Current class-graph module state input hash"),
				IdentityResult);
		}

		for (asUINT GlobalIndex = 0;
			GlobalIndex < ScriptModule->scriptGlobalsList.GetLength();
			++GlobalIndex)
		{
			asCGlobalProperty* Global =
				ScriptModule->scriptGlobalsList[GlobalIndex];
			if (Global == nullptr)
			{
				return Failure(EAngelscriptCacheCleanCaptureError::NotCacheable,
					TEXT("The current class-graph VM global table contains a null entry"));
			}
			const FString GlobalName =
				UTF8_TO_TCHAR(Global->name.AddressOf());
			FClassPlan* Owner = Classes.FindByPredicate(
				[&GlobalName](const FClassPlan& Candidate)
				{
					return Candidate.ClassDesc->StaticClassGlobalVariableName.
						Equals(GlobalName, ESearchCase::CaseSensitive);
				});
			if (Owner == nullptr || Owner->GeneratedStaticClassGlobal != nullptr)
			{
				return Failure(EAngelscriptCacheCleanCaptureError::NotCacheable,
					TEXT("Current class-graph authority admits exactly one generated StaticClass global per type"));
			}
			Owner->GeneratedStaticClassGlobal = Global;
		}
		for (const FClassPlan& Class : Classes)
		{
			if (Class.GeneratedStaticClassGlobal == nullptr)
			{
				return Failure(EAngelscriptCacheCleanCaptureError::NotCacheable,
					TEXT("A current class-graph type is missing its generated StaticClass global"));
			}
		}

		struct FFunctionPlan final
		{
			FAngelscriptCacheCurrentFunctionAuthority Authority;
			int32 OwnerClassIndex = INDEX_NONE;
			const FAngelscriptFunctionDesc* ReflectedFunction = nullptr;
		};
		TArray<FFunctionPlan> Functions;
		TArray<TSet<const FAngelscriptFunctionDesc*>> MatchedFunctionDescs;
		MatchedFunctionDescs.SetNum(Classes.Num());
		for (asUINT FunctionIndex = 0;
			FunctionIndex < ScriptModule->scriptFunctions.GetLength();
			++FunctionIndex)
		{
			asCScriptFunction* Function =
				ScriptModule->scriptFunctions[FunctionIndex];
			if (Function == nullptr || Function->module != ScriptModule)
			{
				return Failure(EAngelscriptCacheCleanCaptureError::NotCacheable,
					TEXT("The current class-graph VM function table contains a null or foreign entry"));
			}
			const FString FunctionName = UTF8_TO_TCHAR(Function->GetName());
			const FString CanonicalDeclaration = UTF8_TO_TCHAR(
				Function->GetDeclaration(false, false, false));
			if (FunctionName == TEXT("StaticClass")
				&& CanonicalDeclaration == TEXT("UClass StaticClass()")
				&& Function->GetObjectType() == nullptr
				&& Function->traits.GetTrait(asTRAIT_GENERATED_FUNCTION))
			{
				const FString FunctionNamespace =
					UTF8_TO_TCHAR(Function->GetNamespace());
				FClassPlan* Owner = Classes.FindByPredicate(
					[&FunctionNamespace](const FClassPlan& Candidate)
					{
						const FString ExpectedNamespace =
							Candidate.ClassNamespace.IsEmpty()
								? Candidate.ClassName
								: Candidate.ClassNamespace + TEXT("::")
									+ Candidate.ClassName;
						return ExpectedNamespace.Equals(
							FunctionNamespace, ESearchCase::CaseSensitive);
					});
				if (Owner == nullptr
					|| Owner->GeneratedStaticClassFunction != nullptr)
				{
					return Failure(EAngelscriptCacheCleanCaptureError::NotCacheable,
						TEXT("A generated current StaticClass helper has no unique class owner"));
				}
				Owner->GeneratedStaticClassFunction = Function;
				continue;
			}

			asCObjectType* OwnerType = Function->artifactOwnerType != nullptr
				? static_cast<asCObjectType*>(Function->artifactOwnerType)
				: Function->objectType;
			const int32 OwnerClassIndex = Classes.IndexOfByPredicate(
				[OwnerType](const FClassPlan& Candidate)
				{
					return Candidate.ScriptType == OwnerType;
				});
			const TOptional<EAngelscriptArtifactEntityKind> EntityKind =
				MapFunctionEntityKind(Function->artifactInvocationKind);
			if (OwnerClassIndex == INDEX_NONE
				|| Function->GetFuncType() != asFUNC_SCRIPT
				|| Function->scriptData == nullptr
				|| !EntityKind.IsSet()
				|| Function->scriptData->artifactCanonicalSource.GetLength() == 0)
			{
				return Failure(EAngelscriptCacheCleanCaptureError::NotCacheable,
					FString::Printf(TEXT("Current class-graph function %s has no complete stable owner/invocation/source authority"),
						*CanonicalDeclaration));
			}

			FAngelscriptStableFunctionKey StableFunctionKey;
			FString StableKeyFailure;
			if (!FAngelscriptCacheStableSymbolIdentity::TryBuildFunctionKey(
				ModuleKey, *Function, StableFunctionKey, &StableKeyFailure))
			{
				return Failure(EAngelscriptCacheCleanCaptureError::NotCacheable,
					FString::Printf(TEXT("Current class-graph function %s has no StableFunctionKey: %s"),
						*CanonicalDeclaration, *StableKeyFailure));
			}

			FFunctionPlan& Plan = Functions.AddDefaulted_GetRef();
			Plan.Authority.Function = Function;
			Plan.OwnerClassIndex = OwnerClassIndex;
			for (const TSharedRef<FAngelscriptFunctionDesc>& Candidate
				: Classes[OwnerClassIndex].ClassDesc->Methods)
			{
				if (Function->GetObjectType()
						!= Classes[OwnerClassIndex].ScriptType
					|| !Candidate->ScriptFunctionName.Equals(
						FunctionName, ESearchCase::CaseSensitive))
				{
					continue;
				}
				if (Plan.ReflectedFunction != nullptr)
				{
					return Failure(EAngelscriptCacheCleanCaptureError::NotCacheable,
						TEXT("More than one reflected descriptor maps to a current class-graph method"));
				}
				Plan.ReflectedFunction = &Candidate.Get();
				MatchedFunctionDescs[OwnerClassIndex].Add(
					Plan.ReflectedFunction);
			}

			FAngelscriptCachedDeclaration& Declaration =
				Plan.Authority.Declaration;
			Declaration.DeclarationKind =
				EAngelscriptCacheDeclarationKind::Function;
			Declaration.EntityKind = EntityKind.GetValue();
			Declaration.SchemaCoverage =
				EAngelscriptCacheSchemaCoverage::Forbidden;
			Declaration.BodyCoverage =
				EAngelscriptCacheBodyCoverage::Required;
			Declaration.OwnerKind = EAngelscriptFunctionOwnerKind::Type;
			Declaration.OwnerKey =
				Classes[OwnerClassIndex].TypeDeclaration.StableKey;
			Declaration.ModuleKey = ModuleKey;
			Declaration.CanonicalNamespace =
				UTF8_TO_TCHAR(Function->GetNamespace());
			Declaration.CanonicalName = FunctionName;
			Declaration.CanonicalDeclaration = CanonicalDeclaration;
			Declaration.StableKey = StableFunctionKey.Hash;
			Declaration.TraitFlags =
				BuildFunctionDeclarationTraitFlags(*Function)
				| BuildFunctionDescriptorTraitFlags(Plan.ReflectedFunction);
			Declaration.ReflectionFlags =
				BuildFunctionReflectionFlags(Plan.ReflectedFunction);
			if (Plan.ReflectedFunction != nullptr)
			{
				AppendMetadata(
					Plan.ReflectedFunction->Meta, Declaration.Metadata);
			}
			Declaration.Slots.Add({
				EAngelscriptCacheDeclarationSlotKind::Function, 0});
			FAngelscriptCachedDataType ReturnType;
			if (!TryMapClassGraphFunctionDataType(
				Function->returnType, TypeAuthorities, ReturnType))
			{
				return Failure(EAngelscriptCacheCleanCaptureError::NotCacheable,
					FString::Printf(TEXT("Current function %s return type is outside the class-graph stable type table"),
						*CanonicalDeclaration));
			}
			Declaration.DeclaredType = MoveTemp(ReturnType);
			for (asUINT ParameterIndex = 0;
				ParameterIndex < Function->parameterTypes.GetLength();
				++ParameterIndex)
			{
				FAngelscriptCachedParameter& Parameter =
					Declaration.OrderedParameters.AddDefaulted_GetRef();
				Parameter.Ordinal = ParameterIndex;
				Parameter.CanonicalName =
					ParameterIndex < Function->parameterNames.GetLength()
						&& Function->parameterNames[ParameterIndex].GetLength() != 0
						? UTF8_TO_TCHAR(
							Function->parameterNames[ParameterIndex].AddressOf())
						: FString::Printf(TEXT("arg%u"), ParameterIndex);
				if (!TryMapClassGraphFunctionDataType(
					Function->parameterTypes[ParameterIndex],
					TypeAuthorities, Parameter.Type))
				{
					return Failure(EAngelscriptCacheCleanCaptureError::NotCacheable,
						FString::Printf(TEXT("Current function %s parameter %u is outside the class-graph stable type table"),
							*CanonicalDeclaration, ParameterIndex));
				}
				const asETypeModifiers Passing =
					ParameterIndex < Function->inOutFlags.GetLength()
						? static_cast<asETypeModifiers>(
							Function->inOutFlags[ParameterIndex] & asTM_INOUTREF)
						: asTM_NONE;
				switch (Passing)
				{
				case asTM_NONE:
					Parameter.Passing =
						EAngelscriptCachedParameterPassing::Value;
					break;
				case asTM_INREF:
					Parameter.Passing =
						EAngelscriptCachedParameterPassing::InReference;
					break;
				case asTM_OUTREF:
					Parameter.Passing =
						EAngelscriptCachedParameterPassing::OutReference;
					break;
				case asTM_INOUTREF:
					Parameter.Passing =
						EAngelscriptCachedParameterPassing::InOutReference;
					break;
				default:
					return Failure(EAngelscriptCacheCleanCaptureError::NotCacheable,
						TEXT("A current class-graph parameter has an unknown passing mode"));
				}
				if (ParameterIndex < Function->defaultArgs.GetLength()
					&& Function->defaultArgs[ParameterIndex] != nullptr)
				{
					Parameter.CanonicalDefaultExpression = UTF8_TO_TCHAR(
						Function->defaultArgs[ParameterIndex]->AddressOf());
				}
				if (Plan.ReflectedFunction != nullptr)
				{
					if (ParameterIndex >= static_cast<asUINT>(
						Plan.ReflectedFunction->Arguments.Num()))
					{
						return Failure(
							EAngelscriptCacheCleanCaptureError::NotCacheable,
							TEXT("A reflected current class-graph method has fewer parameters than its VM function"));
					}
					const FAngelscriptArgumentDesc& Argument =
						Plan.ReflectedFunction->Arguments[ParameterIndex];
					if (Argument.bBlueprintByValue)
					{
						Parameter.TraitFlags |= static_cast<uint32>(
							EAngelscriptCachedParameterTraitFlags::BlueprintByValue);
					}
					if (Argument.bBlueprintOutRef)
					{
						Parameter.TraitFlags |= static_cast<uint32>(
							EAngelscriptCachedParameterTraitFlags::BlueprintOutRef);
					}
					if (Argument.bBlueprintInRef)
					{
						Parameter.TraitFlags |= static_cast<uint32>(
							EAngelscriptCachedParameterTraitFlags::BlueprintInRef);
					}
				}
			}
			if (Plan.ReflectedFunction != nullptr
				&& Plan.ReflectedFunction->Arguments.Num()
					!= static_cast<int32>(
						Function->parameterTypes.GetLength()))
			{
				return Failure(EAngelscriptCacheCleanCaptureError::NotCacheable,
					TEXT("A reflected current class-graph method has more parameters than its VM function"));
			}
			IdentityResult = FAngelscriptCacheSemanticArchive::
				ComputeDeclarationHashes(
					Declaration,
					Declaration.SignatureHash,
					Declaration.TraitsHash);
			if (!IdentityResult.IsSuccess())
			{
				return ValidationFailure(
					EAngelscriptCacheCleanCaptureError::RecordEncodingFailed,
					TEXT("Current class-graph function declaration hashes"),
					IdentityResult);
			}
		}

		for (int32 ClassIndex = 0; ClassIndex < Classes.Num(); ++ClassIndex)
		{
			if (Classes[ClassIndex].GeneratedStaticClassFunction == nullptr
				|| MatchedFunctionDescs[ClassIndex].Num()
					!= Classes[ClassIndex].ClassDesc->Methods.Num())
			{
				return Failure(EAngelscriptCacheCleanCaptureError::NotCacheable,
					TEXT("A current class-graph StaticClass helper or reflected descriptor could not be classified exactly"));
			}
		}
		Functions.StableSort([](
			const FFunctionPlan& Left,
			const FFunctionPlan& Right)
		{
			return Left.OwnerClassIndex < Right.OwnerClassIndex;
		});
		for (int32 FunctionSlot = 0;
			FunctionSlot < Functions.Num(); ++FunctionSlot)
		{
			Functions[FunctionSlot].Authority.Declaration.Slots[0].Ordinal =
				static_cast<uint32>(FunctionSlot);
		}

		auto FindFunction = [&Functions](
			const asCScriptFunction* Function) -> FFunctionPlan*
		{
			return Functions.FindByPredicate(
				[Function](const FFunctionPlan& Candidate)
				{
					return Candidate.Authority.Function == Function;
				});
		};
		auto MakeFunctionReference = [](const FFunctionPlan& Plan)
		{
			return FAngelscriptCacheStableReference{
				EAngelscriptCacheReferenceKind::ScriptFunction,
				Plan.Authority.Declaration.StableKey,
				Plan.Authority.Declaration.SignatureHash};
		};

		for (int32 ClassIndex = 0; ClassIndex < Classes.Num(); ++ClassIndex)
		{
			FClassPlan& Class = Classes[ClassIndex];
			FAngelscriptCachedTypeSchema& Schema = Class.TypeSchema;
			auto AddDeclarationDependency = [&Schema](
				const FAngelscriptCacheStableReference& Target)
			{
				FAngelscriptCacheSemanticDependency Dependency;
				Dependency.Kind =
					EAngelscriptCacheSemanticDependencyKind::Declaration;
				Dependency.Target = Target;
				Schema.Dependencies.AddUnique(MoveTemp(Dependency));
			};

			for (asUINT MethodIndex = 0;
				MethodIndex < Class.ScriptType->methods.GetLength();
				++MethodIndex)
			{
				asCScriptFunction* Function =
					ScriptModule->engine->GetScriptFunction(
						Class.ScriptType->methods[MethodIndex]);
				FFunctionPlan* FunctionPlan = FindFunction(Function);
				if (FunctionPlan == nullptr)
				{
					return Failure(EAngelscriptCacheCleanCaptureError::NotCacheable,
						TEXT("A current class-graph method has no stable local-module function"));
				}
				FAngelscriptCachedMethodEntry& Method =
					Schema.OrderedMethods.AddDefaulted_GetRef();
				Method.EntryKind = FunctionPlan->OwnerClassIndex == ClassIndex
					? EAngelscriptCachedMethodSlotKind::LocalMethod
					: EAngelscriptCachedMethodSlotKind::Inherited;
				Method.MethodOrdinal = MethodIndex;
				Method.FunctionKey = FAngelscriptStableFunctionKey{
					FunctionPlan->Authority.Declaration.StableKey};
				Method.DeclaringOwner = Classes[
					FunctionPlan->OwnerClassIndex].TypeSchema.TypeKey;
				Method.ExpectedDeclarationAbi =
					FunctionPlan->Authority.Declaration.SignatureHash;
				AddDeclarationDependency(
					MakeFunctionReference(*FunctionPlan));
			}

			for (asUINT VftIndex = 0;
				VftIndex < Class.ScriptType->virtualFunctionTable.GetLength();
				++VftIndex)
			{
				asCScriptFunction* Function =
					Class.ScriptType->virtualFunctionTable[VftIndex];
				FFunctionPlan* FunctionPlan = FindFunction(Function);
				if (FunctionPlan == nullptr
					|| Function->vfTableIdx != static_cast<int>(VftIndex))
				{
					return Failure(EAngelscriptCacheCleanCaptureError::NotCacheable,
						TEXT("A current class-graph VFT slot has no stable or correctly indexed function"));
				}
				FAngelscriptCachedVirtualFunctionSlot& Slot =
					Schema.VirtualFunctionTable.AddDefaulted_GetRef();
				Slot.VftOrdinal = VftIndex;
				Slot.FunctionKey = FAngelscriptStableFunctionKey{
					FunctionPlan->Authority.Declaration.StableKey};
				Slot.ImplementingOwner = Classes[
					FunctionPlan->OwnerClassIndex].TypeSchema.TypeKey;
				Slot.ExpectedDeclarationAbi =
					FunctionPlan->Authority.Declaration.SignatureHash;
				if (FunctionPlan->OwnerClassIndex != ClassIndex)
				{
					Slot.SlotKind =
						EAngelscriptCachedMethodSlotKind::Inherited;
					Slot.DeclaringOwner = Slot.ImplementingOwner;
				}
				else if (Class.BaseClassIndex != INDEX_NONE
					&& VftIndex < Classes[Class.BaseClassIndex].ScriptType->
						virtualFunctionTable.GetLength())
				{
					const FAngelscriptCachedTypeSchema& BaseSchema =
						Classes[Class.BaseClassIndex].TypeSchema;
					if (!BaseSchema.VirtualFunctionTable.IsValidIndex(
						static_cast<int32>(VftIndex))
						|| FindFunction(
							Classes[Class.BaseClassIndex].ScriptType->
								virtualFunctionTable[VftIndex]) == nullptr)
					{
						return Failure(
							EAngelscriptCacheCleanCaptureError::NotCacheable,
							TEXT("A current class-graph override has no stable base VFT authority"));
					}
					Slot.SlotKind =
						EAngelscriptCachedMethodSlotKind::VirtualOverride;
					Slot.DeclaringOwner =
						BaseSchema.VirtualFunctionTable[VftIndex].DeclaringOwner;
				}
				else
				{
					Slot.SlotKind =
						EAngelscriptCachedMethodSlotKind::VirtualDeclaration;
					Slot.DeclaringOwner = Schema.TypeKey;
				}
				AddDeclarationDependency(
					MakeFunctionReference(*FunctionPlan));
			}

			for (int32 ReflectionIndex = 0;
				ReflectionIndex < Class.ClassDesc->Methods.Num();
				++ReflectionIndex)
			{
				const FAngelscriptFunctionDesc& Reflected =
					Class.ClassDesc->Methods[ReflectionIndex].Get();
				FFunctionPlan* FunctionPlan = Functions.FindByPredicate(
					[ClassIndex, &Reflected](const FFunctionPlan& Candidate)
					{
						return Candidate.OwnerClassIndex == ClassIndex
							&& Candidate.ReflectedFunction == &Reflected;
					});
				if (FunctionPlan == nullptr)
				{
					return Failure(EAngelscriptCacheCleanCaptureError::NotCacheable,
						TEXT("A reflected current class-graph UFunction is not a local cacheable method"));
				}
				FAngelscriptCachedReflectedFunctionMember& Member =
					Schema.Reflection.OrderedUFunctionMembers.
						AddDefaulted_GetRef();
				Member.ReflectionOrdinal =
					static_cast<uint32>(ReflectionIndex);
				Member.CanonicalFunctionName = Reflected.FunctionName;
				Member.CanonicalOriginalFunctionName =
					Reflected.OriginalFunctionName;
				Member.CanonicalScriptFunctionName =
					Reflected.ScriptFunctionName;
				Member.Target = MakeFunctionReference(*FunctionPlan);
				AddDeclarationDependency(Member.Target);
			}

			auto AddBehavior = [&Schema, &FindFunction,
				&MakeFunctionReference, &AddDeclarationDependency,
				ScriptModule, &Classes](
					const EAngelscriptCachedBehaviorKind Kind,
					const uint32 Ordinal,
					const int FunctionId) -> bool
			{
				if (FunctionId == 0)
				{
					return true;
				}
				asCScriptFunction* Function =
					ScriptModule->engine->GetScriptFunction(FunctionId);
				if (Function == nullptr)
				{
					return false;
				}
				FAngelscriptCachedBehaviorSlot& Slot =
					Schema.OrderedBehaviorSlots.AddDefaulted_GetRef();
				Slot.BehaviorKind = Kind;
				Slot.SlotOrdinal = Ordinal;
				if (FFunctionPlan* FunctionPlan = FindFunction(Function))
				{
					Slot.Target = MakeFunctionReference(*FunctionPlan);
					Slot.DeclaringOwner = Classes[
						FunctionPlan->OwnerClassIndex].TypeSchema.TypeKey;
					AddDeclarationDependency(Slot.Target);
					return true;
				}
				if (!FAngelscriptCacheEnvironmentIdentity::TryBuildFunctionReference(
					*Function, Slot.Target))
				{
					Schema.OrderedBehaviorSlots.Pop();
					return false;
				}
				FAngelscriptCacheSemanticDependency Dependency;
				Dependency.Kind =
					EAngelscriptCacheSemanticDependencyKind::EnvironmentAbi;
				Dependency.Target = Slot.Target;
				Schema.Dependencies.AddUnique(MoveTemp(Dependency));
				return true;
			};
			for (asUINT Index = 0;
				Index < Class.ScriptType->beh.constructors.GetLength(); ++Index)
			{
				if (!AddBehavior(EAngelscriptCachedBehaviorKind::Construct,
					Index, Class.ScriptType->beh.constructors[Index]))
				{
					return Failure(EAngelscriptCacheCleanCaptureError::NotCacheable,
						TEXT("A current class-graph constructor has no stable function reference"));
				}
			}
			if (!AddBehavior(EAngelscriptCachedBehaviorKind::Destruct,
				0, Class.ScriptType->beh.destruct))
			{
				return Failure(EAngelscriptCacheCleanCaptureError::NotCacheable,
					TEXT("A current class-graph destructor has no stable function reference"));
			}
			for (asUINT Index = 0;
				Index < Class.ScriptType->beh.factories.GetLength(); ++Index)
			{
				if (!AddBehavior(EAngelscriptCachedBehaviorKind::Factory,
					Index, Class.ScriptType->beh.factories[Index]))
				{
					return Failure(EAngelscriptCacheCleanCaptureError::NotCacheable,
						TEXT("A current class-graph factory has no stable function reference"));
				}
			}
			const struct
			{
				EAngelscriptCachedBehaviorKind Kind;
				int FunctionId;
			} SingletonBehaviors[] = {
				{EAngelscriptCachedBehaviorKind::ListFactory,
					Class.ScriptType->beh.listFactory},
				{EAngelscriptCachedBehaviorKind::AddRef,
					Class.ScriptType->beh.addref},
				{EAngelscriptCachedBehaviorKind::Release,
					Class.ScriptType->beh.release},
				{EAngelscriptCachedBehaviorKind::GetWeakRefFlag,
					Class.ScriptType->beh.getWeakRefFlag},
				{EAngelscriptCachedBehaviorKind::TemplateCallback,
					Class.ScriptType->beh.templateCallback},
				{EAngelscriptCachedBehaviorKind::GetRefCount,
					Class.ScriptType->beh.gcGetRefCount},
				{EAngelscriptCachedBehaviorKind::SetGcFlag,
					Class.ScriptType->beh.gcSetFlag},
				{EAngelscriptCachedBehaviorKind::GetGcFlag,
					Class.ScriptType->beh.gcGetFlag},
				{EAngelscriptCachedBehaviorKind::EnumRefs,
					Class.ScriptType->beh.gcEnumReferences},
				{EAngelscriptCachedBehaviorKind::ReleaseRefs,
					Class.ScriptType->beh.gcReleaseAllReferences},
				{EAngelscriptCachedBehaviorKind::Copy,
					Class.ScriptType->beh.copy},
				{EAngelscriptCachedBehaviorKind::CopyConstruct,
					Class.ScriptType->beh.copyconstruct},
				{EAngelscriptCachedBehaviorKind::CopyFactory,
					Class.ScriptType->beh.copyfactory},
			};
			for (const auto& Behavior : SingletonBehaviors)
			{
				if (!AddBehavior(Behavior.Kind, 0, Behavior.FunctionId))
				{
					return Failure(EAngelscriptCacheCleanCaptureError::NotCacheable,
						TEXT("A current class-graph singleton behavior has no stable function reference"));
				}
			}
			if (Class.ScriptType->beh.construct != 0)
			{
				Schema.TypeSemanticFlags |= static_cast<uint32>(
					EAngelscriptCachedTypeSemanticFlags::HasDefaultConstructor);
			}
			if (Class.ScriptType->beh.destruct != 0)
			{
				Schema.TypeSemanticFlags |= static_cast<uint32>(
					EAngelscriptCachedTypeSemanticFlags::HasDestructor);
			}
			Schema.Dependencies.Sort([](
				const FAngelscriptCacheSemanticDependency& Left,
				const FAngelscriptCacheSemanticDependency& Right)
			{
				return FAngelscriptCacheTypeSchemaArchive::CompareDependencies(
					Left, Right) < 0;
			});
			IdentityResult = FAngelscriptCacheTypeSchemaArchive::
				ComputeTypeLayoutHash(
					Schema, Schema.Layout.TypeLayoutHash);
			if (!IdentityResult.IsSuccess())
			{
				return ValidationFailure(
					EAngelscriptCacheCleanCaptureError::RecordEncodingFailed,
					TEXT("Current class-graph type layout hash"),
					IdentityResult);
			}
		}

		OutAuthority.ModuleKey = ModuleKey;
		OutAuthority.ModuleInterface.PayloadSchemaVersion =
			FAngelscriptCacheSemanticArchive::ModuleInterfacePayloadSchemaVersion;
		OutAuthority.ModuleInterface.ModuleKey = ModuleKey;
		OutAuthority.ModuleInterface.CanonicalModuleName = Module->ModuleName;
		for (const FClassPlan& Class : Classes)
		{
			if (!Class.ClassNamespace.IsEmpty())
			{
				OutAuthority.ModuleInterface.CanonicalNamespaces.AddUnique(
					Class.ClassNamespace);
			}
			OutAuthority.ModuleInterface.Declarations.Add(
				Class.TypeDeclaration);
			OutAuthority.ModuleInterface.Declarations.Append(
				Class.PropertyDeclarations);
			OutAuthority.TypeSchemas.Add(Class.TypeSchema);
		}
		for (FFunctionPlan& Function : Functions)
		{
			if (!Function.Authority.Declaration.CanonicalNamespace.IsEmpty())
			{
				OutAuthority.ModuleInterface.CanonicalNamespaces.AddUnique(
					Function.Authority.Declaration.CanonicalNamespace);
			}
			OutAuthority.ModuleInterface.Declarations.Add(
				Function.Authority.Declaration);
			OutAuthority.Functions.Add(MoveTemp(Function.Authority));
		}
		IdentityResult = FAngelscriptCacheSemanticArchive::
			ComputeModuleInterfaceAbi(
				OutAuthority.ModuleInterface,
				OutAuthority.ModuleInterface.InterfaceAbi);
		if (!IdentityResult.IsSuccess())
		{
			return ValidationFailure(
				EAngelscriptCacheCleanCaptureError::RecordEncodingFailed,
				TEXT("Current class-graph module interface ABI"),
				IdentityResult);
		}

		FAngelscriptCacheCleanCaptureResult Result;
		const int32 PropertyCount =
			static_cast<int32>(NextDeclarationSlot) - Classes.Num();
		Result.Detail = FString::Printf(
			TEXT("Prepared current semantic authority for %d base-before-derived classes, %d properties and %d functions in module %s"),
			Classes.Num(), PropertyCount,
			OutAuthority.Functions.Num(), *Module->ModuleName);
		return Result;
	}

	static FAngelscriptCacheCleanCaptureResult BuildRootClassMethodAuthority(
		const TSharedRef<FAngelscriptModuleDesc>& Module,
		const FAngelscriptCacheCleanCaptureOptions& Options,
		const FAngelscriptStableModuleKey& ModuleKey,
		FAngelscriptCacheCurrentModuleAuthority& OutAuthority)
	{
		asCModule* ScriptModule = Module->ScriptModule;
		if (Module->Classes.Num() != 1 || !Module->Enums.IsEmpty()
			|| !Module->Delegates.IsEmpty() || !Module->ImportedModules.IsEmpty()
			|| !Module->PostInitFunctions.IsEmpty()
			|| ScriptModule->GetObjectTypeCount() != 1
			|| ScriptModule->GetEnumCount() != 0
			|| ScriptModule->GetTypedefCount() != 0
			|| ScriptModule->GetImportedFunctionCount() != 0)
		{
			return Failure(EAngelscriptCacheCleanCaptureError::NotCacheable,
				TEXT("Current root-class authority requires one class and no enum, delegate, import, typedef or post-init declaration"));
		}

		const TSharedRef<FAngelscriptClassDesc>& ClassDesc = Module->Classes[0];
		asCObjectType* ScriptType = static_cast<asCObjectType*>(
			ScriptModule->GetObjectTypeByIndex(0));
		if (ScriptType == nullptr || ClassDesc->bIsStruct
			|| ClassDesc->bIsStaticsClass || !ClassDesc->bSuperIsCodeClass
			|| ClassDesc->CodeSuperClass == nullptr
			|| ScriptType->shadowType == nullptr
			|| ScriptType->derivedFrom != nullptr
			|| !ClassDesc->ImplementedInterfaces.IsEmpty()
			|| !ClassDesc->ComposeOntoClass.IsEmpty()
			|| ScriptType->size <= 0 || ScriptType->alignment <= 0
			|| ScriptType->basePropertyOffset < 0
			|| ClassDesc->StaticClassGlobalVariableName.IsEmpty())
		{
			return Failure(EAngelscriptCacheCleanCaptureError::NotCacheable,
				TEXT("The class is outside the current root-UClass authority shape"));
		}
		const FString ClassNamespace = ClassDesc->Namespace.IsSet()
			? ClassDesc->Namespace.GetValue()
			: UTF8_TO_TCHAR(ScriptType->GetNamespace());
		const FString ClassName = UTF8_TO_TCHAR(ScriptType->GetName());
		if (!ClassName.Equals(
			ClassDesc->ClassName, ESearchCase::CaseSensitive))
		{
			return Failure(EAngelscriptCacheCleanCaptureError::NotCacheable,
				TEXT("The class descriptor does not own the staging VM type"));
		}

		FAngelscriptCachedDeclaration TypeDeclaration;
		TypeDeclaration.DeclarationKind =
			EAngelscriptCacheDeclarationKind::Type;
		TypeDeclaration.EntityKind = EAngelscriptArtifactEntityKind::Class;
		TypeDeclaration.SchemaCoverage =
			EAngelscriptCacheSchemaCoverage::Required;
		TypeDeclaration.BodyCoverage =
			EAngelscriptCacheBodyCoverage::Forbidden;
		TypeDeclaration.OwnerKind = EAngelscriptFunctionOwnerKind::Module;
		TypeDeclaration.OwnerKey = ModuleKey.Hash;
		TypeDeclaration.ModuleKey = ModuleKey;
		TypeDeclaration.CanonicalNamespace = ClassNamespace;
		TypeDeclaration.CanonicalName = ClassName;
		TypeDeclaration.CanonicalDeclaration =
			FString::Printf(TEXT("class %s"), *ClassName);
		if (ClassDesc->bAbstract)
		{
			TypeDeclaration.TraitFlags |= static_cast<uint32>(
				EAngelscriptCachedDeclarationTraitFlags::Abstract);
		}
		AppendMetadata(ClassDesc->Meta, TypeDeclaration.Metadata);
		TypeDeclaration.Slots.Add({
			EAngelscriptCacheDeclarationSlotKind::Declaration, 0});
		FAngelscriptTypeIdentityDescriptor TypeIdentity;
		TypeIdentity.ModuleKey = ModuleKey;
		TypeIdentity.Namespace = ClassNamespace;
		TypeIdentity.Kind = TypeDeclaration.EntityKind;
		TypeIdentity.CanonicalDeclaration =
			TypeDeclaration.CanonicalDeclaration;
		TypeDeclaration.StableKey =
			FAngelscriptArtifactIdentityBuilder::BuildTypeKey(
				TypeIdentity).Hash;
		FAngelscriptCacheValidationResult IdentityResult =
			FAngelscriptCacheSemanticArchive::ComputeDeclarationHashes(
				TypeDeclaration,
				TypeDeclaration.SignatureHash,
				TypeDeclaration.TraitsHash);
		if (!IdentityResult.IsSuccess())
		{
			return ValidationFailure(
				EAngelscriptCacheCleanCaptureError::RecordEncodingFailed,
				TEXT("Class declaration hashes"), IdentityResult);
		}

		FAngelscriptCachedTypeSchema TypeSchema;
		TypeSchema.PayloadSchemaVersion =
			FAngelscriptCacheTypeSchemaArchive::TypeSchemaPayloadSchemaVersion;
		TypeSchema.ModuleKey = ModuleKey;
		TypeSchema.TypeKey =
			FAngelscriptStableTypeKey{TypeDeclaration.StableKey};
		TypeSchema.TypeKind = EAngelscriptCachedTypeKind::Class;
		TypeSchema.CanonicalNamespace = ClassNamespace;
		TypeSchema.CanonicalName = ClassName;
		TypeSchema.CanonicalDeclaration =
			TypeDeclaration.CanonicalDeclaration;
		TypeSchema.TypeSemanticFlags = static_cast<uint32>(
			EAngelscriptCachedTypeSemanticFlags::ReferenceType);
		if (ClassDesc->bAbstract)
		{
			TypeSchema.TypeSemanticFlags |= static_cast<uint32>(
				EAngelscriptCachedTypeSemanticFlags::Abstract);
		}
		TypeSchema.Metadata = TypeDeclaration.Metadata;
		TypeSchema.Layout.SemanticSize =
			static_cast<uint64>(ScriptType->size);
		TypeSchema.Layout.SemanticAlignment =
			static_cast<uint32>(ScriptType->alignment);
		TypeSchema.Layout.BasePropertyBoundary =
			static_cast<uint32>(ScriptType->basePropertyOffset);
		TypeSchema.Reflection.ReflectionKind =
			EAngelscriptCachedReflectionKind::UClass;
		TypeSchema.Reflection.ClassReflectionFlags =
			BuildClassReflectionFlags(*ClassDesc);
		if (!ClassDesc->ConfigName.IsEmpty())
		{
			TypeSchema.Reflection.ConfigName = ClassDesc->ConfigName;
		}
		TypeSchema.Reflection.StaticClassGlobalName =
			ClassDesc->StaticClassGlobalVariableName;

		const FAngelscriptCacheStableReference CodeRoot =
			BuildCodeRootReference(*ClassDesc->CodeSuperClass);
		TypeSchema.Relations.Add({
			EAngelscriptCachedTypeRelationKind::ShadowSuper, {}, CodeRoot});
		TypeSchema.Relations.Add({
			EAngelscriptCachedTypeRelationKind::CodeSuper, {}, CodeRoot});
		FAngelscriptCachedTypeLayoutInput LayoutInput;
		LayoutInput.InputKind =
			EAngelscriptCachedTypeLayoutInputKind::CodeRoot;
		LayoutInput.Target = CodeRoot;
		LayoutInput.BoundaryContribution =
			static_cast<uint32>(ScriptType->basePropertyOffset);
		LayoutInput.AlignmentContribution = static_cast<uint32>(
			static_cast<const asCObjectType*>(
				ScriptType->shadowType)->alignment);
		IdentityResult = FAngelscriptCacheTypeSchemaArchive::
			ComputeLayoutInputHash(
				LayoutInput, LayoutInput.LayoutInputHash);
		if (!IdentityResult.IsSuccess())
		{
			return ValidationFailure(
				EAngelscriptCacheCleanCaptureError::RecordEncodingFailed,
				TEXT("Class code-root layout input"), IdentityResult);
		}
		TypeSchema.LayoutInputs.Add(LayoutInput);
		FAngelscriptCacheSemanticDependency CodeRootDependency;
		CodeRootDependency.Kind =
			EAngelscriptCacheSemanticDependencyKind::EnvironmentAbi;
		CodeRootDependency.Target = CodeRoot;
		TypeSchema.Dependencies.Add(CodeRootDependency);

		TArray<FAngelscriptCachedDeclaration> PropertyDeclarations;
		TSet<const FAngelscriptPropertyDesc*> MatchedPropertyDescs;
		for (asUINT PropertyIndex = 0;
			PropertyIndex < ScriptType->localProperties.GetLength();
			++PropertyIndex)
		{
			const asCObjectProperty* ScriptProperty =
				ScriptType->localProperties[PropertyIndex];
			if (ScriptProperty == nullptr || ScriptProperty->isInherited
				|| ScriptProperty->byteOffset < 0)
			{
				return Failure(EAngelscriptCacheCleanCaptureError::NotCacheable,
					TEXT("The current root class contains an unsupported local VM property"));
			}

			FAngelscriptCachedDataType CachedType;
			FString CanonicalType;
			FAngelscriptCacheSemanticDependency PropertyTypeDependency;
			bool bHasPropertyTypeDependency = false;
			if (!TryMapPrimitivePropertyDataType(
				ScriptProperty->type, CachedType, CanonicalType))
			{
				bHasPropertyTypeDependency =
					TryMapEnvironmentPropertyDataType(
						ScriptProperty->type,
						CachedType,
						CanonicalType,
						PropertyTypeDependency);
				if (!bHasPropertyTypeDependency)
				{
					return Failure(
						EAngelscriptCacheCleanCaptureError::NotCacheable,
						TEXT("Current root-class authority supports primitive int or stable environment-value properties"));
				}
			}

			const FString PropertyName =
				UTF8_TO_TCHAR(ScriptProperty->name.AddressOf());
			const FAngelscriptPropertyDesc* ReflectedProperty = nullptr;
			for (const TSharedRef<FAngelscriptPropertyDesc>& Candidate
				: ClassDesc->Properties)
			{
				if (Candidate->ScriptPropertyIndex
						== static_cast<int32>(PropertyIndex)
					|| Candidate->PropertyName.Equals(
						PropertyName, ESearchCase::CaseSensitive))
				{
					if (ReflectedProperty != nullptr)
					{
						return Failure(
							EAngelscriptCacheCleanCaptureError::NotCacheable,
							TEXT("More than one reflected descriptor maps to one current VM property"));
					}
					ReflectedProperty = &Candidate.Get();
				}
			}
			if (ReflectedProperty != nullptr)
			{
				MatchedPropertyDescs.Add(ReflectedProperty);
			}

			FAngelscriptCachedDeclaration PropertyDeclaration;
			PropertyDeclaration.DeclarationKind =
				EAngelscriptCacheDeclarationKind::Property;
			PropertyDeclaration.EntityKind =
				EAngelscriptArtifactEntityKind::Property;
			PropertyDeclaration.SchemaCoverage =
				EAngelscriptCacheSchemaCoverage::Forbidden;
			PropertyDeclaration.BodyCoverage =
				EAngelscriptCacheBodyCoverage::Forbidden;
			PropertyDeclaration.OwnerKind =
				EAngelscriptFunctionOwnerKind::Type;
			PropertyDeclaration.OwnerKey = TypeDeclaration.StableKey;
			PropertyDeclaration.ModuleKey = ModuleKey;
			PropertyDeclaration.CanonicalNamespace = ClassNamespace;
			PropertyDeclaration.CanonicalName = PropertyName;
			PropertyDeclaration.CanonicalTypeSpelling = CanonicalType;
			PropertyDeclaration.CanonicalDeclaration = FString::Printf(
				TEXT("%s %s"), *CanonicalType, *PropertyName);
			PropertyDeclaration.DeclaredType = CachedType;
			if (ScriptProperty->isPrivate)
			{
				PropertyDeclaration.TraitFlags |= static_cast<uint32>(
					EAngelscriptCachedDeclarationTraitFlags::Private);
			}
			if (ScriptProperty->isProtected)
			{
				PropertyDeclaration.TraitFlags |= static_cast<uint32>(
					EAngelscriptCachedDeclarationTraitFlags::Protected);
			}
			PropertyDeclaration.ReflectionFlags =
				BuildPropertyReflectionFlags(ReflectedProperty);
			if (ReflectedProperty != nullptr)
			{
				AppendMetadata(
					ReflectedProperty->Meta, PropertyDeclaration.Metadata);
			}
			PropertyDeclaration.Slots.Add({
				EAngelscriptCacheDeclarationSlotKind::Declaration,
				PropertyIndex + 1});
			FAngelscriptPropertyIdentityDescriptor PropertyIdentity;
			PropertyIdentity.OwnerTypeKey = TypeSchema.TypeKey;
			PropertyIdentity.Kind = PropertyDeclaration.EntityKind;
			PropertyIdentity.Name = PropertyName;
			PropertyIdentity.CanonicalType = CanonicalType;
			PropertyDeclaration.StableKey =
				FAngelscriptArtifactIdentityBuilder::BuildPropertyKey(
					PropertyIdentity).Hash;
			IdentityResult = FAngelscriptCacheSemanticArchive::
				ComputeDeclarationHashes(
					PropertyDeclaration,
					PropertyDeclaration.SignatureHash,
					PropertyDeclaration.TraitsHash);
			if (!IdentityResult.IsSuccess())
			{
				return ValidationFailure(
					EAngelscriptCacheCleanCaptureError::RecordEncodingFailed,
					TEXT("Current property declaration hashes"),
					IdentityResult);
			}

			FAngelscriptCachedPropertySchema PropertySchema;
			PropertySchema.LayoutOrdinal = PropertyIndex;
			PropertySchema.SemanticByteOffset =
				static_cast<uint32>(ScriptProperty->byteOffset);
			PropertySchema.PropertyKey = FAngelscriptStablePropertyKey{
				PropertyDeclaration.StableKey};
			PropertySchema.CanonicalName = PropertyName;
			PropertySchema.Type = CachedType;
			PropertySchema.StorageKind = ScriptProperty->type.IsObjectHandle()
				? EAngelscriptCachedPropertyStorageKind::ObjectHandle
				: EAngelscriptCachedPropertyStorageKind::InlineValue;
			const FAngelscriptCacheV1StorageLayout PropertyStorage =
				GetPropertyStorageLayout(ScriptProperty->type);
			PropertySchema.SemanticStorageSize =
				PropertyStorage.SemanticStorageSize;
			PropertySchema.SemanticStorageAlignment =
				PropertyStorage.SemanticStorageAlignment;
			PropertySchema.Access = ScriptProperty->isPrivate
				? EAngelscriptCachedMemberAccess::Private
				: ScriptProperty->isProtected
					? EAngelscriptCachedMemberAccess::Protected
					: EAngelscriptCachedMemberAccess::Public;
			PropertySchema.PropertySemanticFlags =
				BuildPropertySemanticFlags(ReflectedProperty);
			PropertySchema.ReplicationCondition = ReflectedProperty != nullptr
				? static_cast<EAngelscriptCachedReplicationCondition>(
					ReflectedProperty->ReplicationCondition.GetValue())
				: EAngelscriptCachedReplicationCondition::None;
			PropertySchema.Metadata = PropertyDeclaration.Metadata;
			IdentityResult = FAngelscriptCacheTypeSchemaArchive::
				ComputeStorageLayoutHash(
					PropertySchema.Type,
					PropertySchema.StorageKind,
					PropertySchema.SemanticStorageSize,
					PropertySchema.SemanticStorageAlignment,
					PropertySchema.StorageLayoutHash);
			if (IdentityResult.IsSuccess())
			{
				IdentityResult = FAngelscriptCacheTypeSchemaArchive::
					ComputePropertyLayoutFingerprint(
						TypeSchema.TypeKey,
						PropertySchema,
						PropertySchema.PropertyLayoutFingerprint);
			}
			if (!IdentityResult.IsSuccess())
			{
				return ValidationFailure(
					EAngelscriptCacheCleanCaptureError::RecordEncodingFailed,
					TEXT("Current property layout hashes"),
					IdentityResult);
			}
			PropertyDeclarations.Add(MoveTemp(PropertyDeclaration));
			TypeSchema.OrderedProperties.Add(MoveTemp(PropertySchema));
			if (bHasPropertyTypeDependency)
			{
				TypeSchema.Dependencies.AddUnique(
					MoveTemp(PropertyTypeDependency));
			}
		}
		if (MatchedPropertyDescs.Num() != ClassDesc->Properties.Num())
		{
			return Failure(EAngelscriptCacheCleanCaptureError::NotCacheable,
				TEXT("A reflected property descriptor has no matching current local VM property"));
		}

		asCGlobalProperty* GeneratedStaticClassGlobal = nullptr;
		for (asUINT GlobalIndex = 0;
			GlobalIndex < ScriptModule->scriptGlobalsList.GetLength(); ++GlobalIndex)
		{
			asCGlobalProperty* Candidate =
				ScriptModule->scriptGlobalsList[GlobalIndex];
			if (Candidate != nullptr
				&& FString(UTF8_TO_TCHAR(Candidate->name.AddressOf())).Equals(
					ClassDesc->StaticClassGlobalVariableName,
					ESearchCase::CaseSensitive)
				&& GeneratedStaticClassGlobal == nullptr)
			{
				GeneratedStaticClassGlobal = Candidate;
				continue;
			}
			return Failure(EAngelscriptCacheCleanCaptureError::NotCacheable,
				TEXT("The first current root-class authority slice permits only its generated StaticClass global"));
		}
		if (GeneratedStaticClassGlobal == nullptr)
		{
			return Failure(EAngelscriptCacheCleanCaptureError::NotCacheable,
				TEXT("The generated root-class StaticClass global is absent"));
		}

		OutAuthority.ModuleState.PayloadSchemaVersion =
			FAngelscriptCacheRemainingRecordArchive::
				ModuleStatePayloadSchemaVersion;
		OutAuthority.ModuleState.ModuleKey = ModuleKey;
		OutAuthority.ModuleState.Profile = Options.Profile;
		IdentityResult = FAngelscriptCacheRemainingRecordArchive::
			ComputeModuleStateInputHash(
				OutAuthority.ModuleState,
				OutAuthority.ModuleState.StateInputHash);
		if (!IdentityResult.IsSuccess())
		{
			return ValidationFailure(
				EAngelscriptCacheCleanCaptureError::RecordEncodingFailed,
				TEXT("Module state input hash"), IdentityResult);
		}

		struct FFunctionPlan final
		{
			FAngelscriptCacheCurrentFunctionAuthority Authority;
			const FAngelscriptFunctionDesc* ReflectedFunction = nullptr;
		};
		TArray<FFunctionPlan> Functions;
		TSet<const FAngelscriptFunctionDesc*> MatchedFunctionDescs;
		asCScriptFunction* GeneratedStaticClassFunction = nullptr;
		uint32 GeneratedStaticClassFunctionCount = 0;
		for (asUINT FunctionIndex = 0;
			FunctionIndex < ScriptModule->scriptFunctions.GetLength();
			++FunctionIndex)
		{
			asCScriptFunction* Function =
				ScriptModule->scriptFunctions[FunctionIndex];
			if (Function == nullptr || Function->module != ScriptModule)
			{
				return Failure(EAngelscriptCacheCleanCaptureError::NotCacheable,
					TEXT("The staging VM module function table contains a null or foreign entry"));
			}
			const FString FunctionName = UTF8_TO_TCHAR(Function->GetName());
			const FString CanonicalDeclaration = UTF8_TO_TCHAR(
				Function->GetDeclaration(false, false, false));
			if (FunctionName == TEXT("StaticClass")
				&& CanonicalDeclaration == TEXT("UClass StaticClass()")
				&& Function->GetObjectType() == nullptr
				&& Function->traits.GetTrait(asTRAIT_GENERATED_FUNCTION))
			{
				GeneratedStaticClassFunction = Function;
				++GeneratedStaticClassFunctionCount;
				continue;
			}

			const TOptional<EAngelscriptArtifactEntityKind> EntityKind =
				MapFunctionEntityKind(Function->artifactInvocationKind);
			if (Function->GetFuncType() != asFUNC_SCRIPT
				|| Function->scriptData == nullptr
				|| !EntityKind.IsSet()
				|| Function->scriptData->artifactCanonicalSource.GetLength() == 0)
			{
				return Failure(EAngelscriptCacheCleanCaptureError::NotCacheable,
					FString::Printf(
						TEXT("Staging function %s has no stable invocation/source authority"),
						*CanonicalDeclaration));
			}

			FAngelscriptStableFunctionKey StableFunctionKey;
			FString StableKeyFailure;
			if (!FAngelscriptCacheStableSymbolIdentity::TryBuildFunctionKey(
				ModuleKey,
				*Function,
				StableFunctionKey,
				&StableKeyFailure))
			{
				return Failure(EAngelscriptCacheCleanCaptureError::NotCacheable,
					FString::Printf(
						TEXT("Staging function %s has no StableFunctionKey: %s"),
						*CanonicalDeclaration,
						*StableKeyFailure));
			}

			FFunctionPlan& Plan = Functions.AddDefaulted_GetRef();
			Plan.Authority.Function = Function;
			for (const TSharedRef<FAngelscriptFunctionDesc>& Candidate
				: ClassDesc->Methods)
			{
				if (Function->GetObjectType() != ScriptType
					|| !Candidate->ScriptFunctionName.Equals(
						FunctionName, ESearchCase::CaseSensitive))
				{
					continue;
				}
				if (Plan.ReflectedFunction != nullptr)
				{
					return Failure(
						EAngelscriptCacheCleanCaptureError::NotCacheable,
						TEXT("More than one reflected descriptor maps to one staging method"));
				}
				Plan.ReflectedFunction = &Candidate.Get();
				MatchedFunctionDescs.Add(Plan.ReflectedFunction);
			}

			FAngelscriptCachedDeclaration& Declaration =
				Plan.Authority.Declaration;
			Declaration.DeclarationKind =
				EAngelscriptCacheDeclarationKind::Function;
			Declaration.EntityKind = EntityKind.GetValue();
			Declaration.SchemaCoverage =
				EAngelscriptCacheSchemaCoverage::Forbidden;
			Declaration.BodyCoverage =
				EAngelscriptCacheBodyCoverage::Required;
			Declaration.OwnerKind = Function->artifactInvocationKind
				== asBUILD_ARTIFACT_INVOCATION_GLOBAL_FUNCTION
					? EAngelscriptFunctionOwnerKind::Module
					: EAngelscriptFunctionOwnerKind::Type;
			Declaration.OwnerKey = Declaration.OwnerKind
				== EAngelscriptFunctionOwnerKind::Module
					? ModuleKey.Hash : TypeDeclaration.StableKey;
			Declaration.ModuleKey = ModuleKey;
			Declaration.CanonicalNamespace =
				UTF8_TO_TCHAR(Function->GetNamespace());
			Declaration.CanonicalName = FunctionName;
			Declaration.CanonicalDeclaration = CanonicalDeclaration;
			Declaration.StableKey = StableFunctionKey.Hash;
			Declaration.TraitFlags =
				BuildFunctionDeclarationTraitFlags(*Function)
				| BuildFunctionDescriptorTraitFlags(
					Plan.ReflectedFunction);
			Declaration.ReflectionFlags =
				BuildFunctionReflectionFlags(Plan.ReflectedFunction);
			if (Plan.ReflectedFunction != nullptr)
			{
				AppendMetadata(
					Plan.ReflectedFunction->Meta,
					Declaration.Metadata);
			}
			Declaration.Slots.Add({
				EAngelscriptCacheDeclarationSlotKind::Function,
				static_cast<uint32>(Functions.Num() - 1)});
			FAngelscriptCachedDataType ReturnType;
			if (!TryMapRootFunctionDataType(
				Function->returnType,
				*ScriptType,
				TypeDeclaration,
				ReturnType))
			{
				return Failure(EAngelscriptCacheCleanCaptureError::NotCacheable,
					FString::Printf(
						TEXT("Function %s has an unsupported return type"),
						*CanonicalDeclaration));
			}
			Declaration.DeclaredType = MoveTemp(ReturnType);

			for (asUINT ParameterIndex = 0;
				ParameterIndex < Function->parameterTypes.GetLength();
				++ParameterIndex)
			{
				FAngelscriptCachedParameter& Parameter =
					Declaration.OrderedParameters.AddDefaulted_GetRef();
				Parameter.Ordinal = ParameterIndex;
				Parameter.CanonicalName = ParameterIndex
					< Function->parameterNames.GetLength()
					&& Function->parameterNames[ParameterIndex].GetLength() != 0
						? UTF8_TO_TCHAR(
							Function->parameterNames[ParameterIndex].AddressOf())
						: FString::Printf(TEXT("arg%u"), ParameterIndex);
				if (!TryMapRootFunctionDataType(
					Function->parameterTypes[ParameterIndex],
					*ScriptType,
					TypeDeclaration,
					Parameter.Type))
				{
					return Failure(
						EAngelscriptCacheCleanCaptureError::NotCacheable,
						FString::Printf(
							TEXT("Function %s parameter %u has an unsupported type"),
							*CanonicalDeclaration,
							ParameterIndex));
				}
				const asETypeModifiers Passing = ParameterIndex
					< Function->inOutFlags.GetLength()
						? static_cast<asETypeModifiers>(
							Function->inOutFlags[ParameterIndex]
								& asTM_INOUTREF)
						: asTM_NONE;
				switch (Passing)
				{
				case asTM_NONE:
					Parameter.Passing =
						EAngelscriptCachedParameterPassing::Value;
					break;
				case asTM_INREF:
					Parameter.Passing =
						EAngelscriptCachedParameterPassing::InReference;
					break;
				case asTM_OUTREF:
					Parameter.Passing =
						EAngelscriptCachedParameterPassing::OutReference;
					break;
				case asTM_INOUTREF:
					Parameter.Passing =
						EAngelscriptCachedParameterPassing::InOutReference;
					break;
				default:
					return Failure(
						EAngelscriptCacheCleanCaptureError::NotCacheable,
						TEXT("A staging function parameter has an unknown passing mode"));
				}
				if (ParameterIndex < Function->defaultArgs.GetLength()
					&& Function->defaultArgs[ParameterIndex] != nullptr)
				{
					Parameter.CanonicalDefaultExpression = UTF8_TO_TCHAR(
						Function->defaultArgs[ParameterIndex]->AddressOf());
				}
				if (Plan.ReflectedFunction != nullptr)
				{
					if (ParameterIndex >= static_cast<asUINT>(
						Plan.ReflectedFunction->Arguments.Num()))
					{
						return Failure(
							EAngelscriptCacheCleanCaptureError::NotCacheable,
							TEXT("A reflected descriptor has fewer parameters than its staging method"));
					}
					const FAngelscriptArgumentDesc& Argument =
						Plan.ReflectedFunction->Arguments[
							static_cast<int32>(ParameterIndex)];
					if (Argument.bBlueprintByValue)
					{
						Parameter.TraitFlags |= static_cast<uint32>(
							EAngelscriptCachedParameterTraitFlags::BlueprintByValue);
					}
					if (Argument.bBlueprintOutRef)
					{
						Parameter.TraitFlags |= static_cast<uint32>(
							EAngelscriptCachedParameterTraitFlags::BlueprintOutRef);
					}
					if (Argument.bBlueprintInRef)
					{
						Parameter.TraitFlags |= static_cast<uint32>(
							EAngelscriptCachedParameterTraitFlags::BlueprintInRef);
					}
				}
			}
			if (Plan.ReflectedFunction != nullptr
				&& Plan.ReflectedFunction->Arguments.Num()
					!= static_cast<int32>(
						Function->parameterTypes.GetLength()))
			{
				return Failure(EAngelscriptCacheCleanCaptureError::NotCacheable,
					TEXT("A reflected descriptor has more parameters than its staging method"));
			}
			IdentityResult = FAngelscriptCacheSemanticArchive::
				ComputeDeclarationHashes(
					Declaration,
					Declaration.SignatureHash,
					Declaration.TraitsHash);
			if (!IdentityResult.IsSuccess())
			{
				return ValidationFailure(
					EAngelscriptCacheCleanCaptureError::RecordEncodingFailed,
					TEXT("Function declaration hashes"), IdentityResult);
			}
		}
		if (GeneratedStaticClassFunctionCount != 1
			|| GeneratedStaticClassFunction == nullptr
			|| Functions.IsEmpty())
		{
			return Failure(EAngelscriptCacheCleanCaptureError::NotCacheable,
				TEXT("The generated StaticClass helper or staging method table is incomplete"));
		}
		if (MatchedFunctionDescs.Num() != ClassDesc->Methods.Num())
		{
			return Failure(EAngelscriptCacheCleanCaptureError::NotCacheable,
				TEXT("A reflected function descriptor has no uniquely named staging method"));
		}

		auto FindPlan = [&Functions](const asCScriptFunction* Function)
			-> FFunctionPlan*
		{
			for (FFunctionPlan& Candidate : Functions)
			{
				if (Candidate.Authority.Function == Function)
				{
					return &Candidate;
				}
			}
			return nullptr;
		};
		auto MakeLocalFunctionReference = [](
			const FFunctionPlan& Plan)
		{
			return FAngelscriptCacheStableReference{
				EAngelscriptCacheReferenceKind::ScriptFunction,
				Plan.Authority.Declaration.StableKey,
				Plan.Authority.Declaration.SignatureHash,
			};
		};
		auto AddSchemaDependency = [&TypeSchema](
			const EAngelscriptCacheSemanticDependencyKind Kind,
			const FAngelscriptCacheStableReference& Target)
		{
			FAngelscriptCacheSemanticDependency Dependency;
			Dependency.Kind = Kind;
			Dependency.Target = Target;
			TypeSchema.Dependencies.AddUnique(MoveTemp(Dependency));
		};

		for (asUINT MethodIndex = 0;
			MethodIndex < ScriptType->methods.GetLength(); ++MethodIndex)
		{
			asCScriptFunction* Function = ScriptModule->engine->GetScriptFunction(
				ScriptType->methods[MethodIndex]);
			FFunctionPlan* Plan = FindPlan(Function);
			if (Plan == nullptr || Function->objectType != ScriptType)
			{
				return Failure(EAngelscriptCacheCleanCaptureError::NotCacheable,
					TEXT("The root-class method table contains a non-local or unsupported function"));
			}
			FAngelscriptCachedMethodEntry& Method =
				TypeSchema.OrderedMethods.AddDefaulted_GetRef();
			Method.EntryKind = EAngelscriptCachedMethodSlotKind::LocalMethod;
			Method.MethodOrdinal = MethodIndex;
			Method.FunctionKey = FAngelscriptStableFunctionKey{
				Plan->Authority.Declaration.StableKey};
			Method.DeclaringOwner = TypeSchema.TypeKey;
			Method.ExpectedDeclarationAbi =
				Plan->Authority.Declaration.SignatureHash;
			AddSchemaDependency(
				EAngelscriptCacheSemanticDependencyKind::Declaration,
				MakeLocalFunctionReference(*Plan));
		}

		for (asUINT VftIndex = 0;
			VftIndex < ScriptType->virtualFunctionTable.GetLength(); ++VftIndex)
		{
			asCScriptFunction* Function =
				ScriptType->virtualFunctionTable[VftIndex];
			FFunctionPlan* Plan = FindPlan(Function);
			if (Plan == nullptr || Function->objectType != ScriptType
				|| Function->vfTableIdx != static_cast<int>(VftIndex))
			{
				return Failure(EAngelscriptCacheCleanCaptureError::NotCacheable,
					TEXT("The root-class VFT contains a non-local, unsupported or misindexed function"));
			}
			FAngelscriptCachedVirtualFunctionSlot& Slot =
				TypeSchema.VirtualFunctionTable.AddDefaulted_GetRef();
			Slot.SlotKind =
				EAngelscriptCachedMethodSlotKind::VirtualDeclaration;
			Slot.VftOrdinal = VftIndex;
			Slot.FunctionKey = FAngelscriptStableFunctionKey{
				Plan->Authority.Declaration.StableKey};
			Slot.DeclaringOwner = TypeSchema.TypeKey;
			Slot.ImplementingOwner = TypeSchema.TypeKey;
			Slot.ExpectedDeclarationAbi =
				Plan->Authority.Declaration.SignatureHash;
			AddSchemaDependency(
				EAngelscriptCacheSemanticDependencyKind::Declaration,
				MakeLocalFunctionReference(*Plan));
		}

		for (int32 ReflectionIndex = 0;
			ReflectionIndex < ClassDesc->Methods.Num(); ++ReflectionIndex)
		{
			const FAngelscriptFunctionDesc& Reflected =
				ClassDesc->Methods[ReflectionIndex].Get();
			FFunctionPlan* Plan = Functions.FindByPredicate(
				[&Reflected](const FFunctionPlan& Candidate)
				{
					return Candidate.ReflectedFunction == &Reflected;
				});
			if (Plan == nullptr
				|| Plan->Authority.Declaration.EntityKind
					!= EAngelscriptArtifactEntityKind::Method)
			{
				return Failure(EAngelscriptCacheCleanCaptureError::NotCacheable,
					TEXT("A reflected UFunction is not a local cacheable staging method"));
			}
			FAngelscriptCachedReflectedFunctionMember& Member =
				TypeSchema.Reflection.OrderedUFunctionMembers.
					AddDefaulted_GetRef();
			Member.ReflectionOrdinal = static_cast<uint32>(ReflectionIndex);
			Member.CanonicalFunctionName = Reflected.FunctionName;
			Member.CanonicalOriginalFunctionName =
				Reflected.OriginalFunctionName;
			Member.CanonicalScriptFunctionName =
				Reflected.ScriptFunctionName;
			Member.Target = MakeLocalFunctionReference(*Plan);
			AddSchemaDependency(
				EAngelscriptCacheSemanticDependencyKind::Declaration,
				Member.Target);
		}

		auto AddBehavior = [&TypeSchema, &FindPlan,
			&MakeLocalFunctionReference, &AddSchemaDependency, ScriptModule](
			const EAngelscriptCachedBehaviorKind Kind,
			const uint32 Ordinal,
			const int FunctionId) -> bool
		{
			if (FunctionId == 0)
			{
				return true;
			}
			asCScriptFunction* Function =
				ScriptModule->engine->GetScriptFunction(FunctionId);
			if (Function == nullptr)
			{
				return false;
			}
			FAngelscriptCachedBehaviorSlot& Slot =
				TypeSchema.OrderedBehaviorSlots.AddDefaulted_GetRef();
			Slot.BehaviorKind = Kind;
			Slot.SlotOrdinal = Ordinal;
			if (FFunctionPlan* Plan = FindPlan(Function))
			{
				Slot.Target = MakeLocalFunctionReference(*Plan);
				Slot.DeclaringOwner = TypeSchema.TypeKey;
				AddSchemaDependency(
					EAngelscriptCacheSemanticDependencyKind::Declaration,
					Slot.Target);
				return true;
			}
			if (!FAngelscriptCacheEnvironmentIdentity::TryBuildFunctionReference(
				*Function, Slot.Target))
			{
				TypeSchema.OrderedBehaviorSlots.Pop();
				return false;
			}
			AddSchemaDependency(
				EAngelscriptCacheSemanticDependencyKind::EnvironmentAbi,
				Slot.Target);
			return true;
		};

		for (asUINT Index = 0;
			Index < ScriptType->beh.constructors.GetLength(); ++Index)
		{
			if (!AddBehavior(
				EAngelscriptCachedBehaviorKind::Construct,
				Index,
				ScriptType->beh.constructors[Index]))
			{
				return Failure(EAngelscriptCacheCleanCaptureError::NotCacheable,
					TEXT("A root-class constructor has no stable function reference"));
			}
		}
		if (!AddBehavior(
			EAngelscriptCachedBehaviorKind::Destruct,
			0,
			ScriptType->beh.destruct))
		{
			return Failure(EAngelscriptCacheCleanCaptureError::NotCacheable,
				TEXT("The root-class destructor has no stable function reference"));
		}
		for (asUINT Index = 0;
			Index < ScriptType->beh.factories.GetLength(); ++Index)
		{
			if (!AddBehavior(
				EAngelscriptCachedBehaviorKind::Factory,
				Index,
				ScriptType->beh.factories[Index]))
			{
				return Failure(EAngelscriptCacheCleanCaptureError::NotCacheable,
					TEXT("A root-class factory has no stable function reference"));
			}
		}
		const struct
		{
			EAngelscriptCachedBehaviorKind Kind;
			int FunctionId;
		} SingletonBehaviors[] = {
			{EAngelscriptCachedBehaviorKind::ListFactory,
				ScriptType->beh.listFactory},
			{EAngelscriptCachedBehaviorKind::AddRef, ScriptType->beh.addref},
			{EAngelscriptCachedBehaviorKind::Release, ScriptType->beh.release},
			{EAngelscriptCachedBehaviorKind::GetWeakRefFlag,
				ScriptType->beh.getWeakRefFlag},
			{EAngelscriptCachedBehaviorKind::TemplateCallback,
				ScriptType->beh.templateCallback},
			{EAngelscriptCachedBehaviorKind::GetRefCount,
				ScriptType->beh.gcGetRefCount},
			{EAngelscriptCachedBehaviorKind::SetGcFlag,
				ScriptType->beh.gcSetFlag},
			{EAngelscriptCachedBehaviorKind::GetGcFlag,
				ScriptType->beh.gcGetFlag},
			{EAngelscriptCachedBehaviorKind::EnumRefs,
				ScriptType->beh.gcEnumReferences},
			{EAngelscriptCachedBehaviorKind::ReleaseRefs,
				ScriptType->beh.gcReleaseAllReferences},
			{EAngelscriptCachedBehaviorKind::Copy, ScriptType->beh.copy},
			{EAngelscriptCachedBehaviorKind::CopyConstruct,
				ScriptType->beh.copyconstruct},
			{EAngelscriptCachedBehaviorKind::CopyFactory,
				ScriptType->beh.copyfactory},
		};
		for (const auto& Behavior : SingletonBehaviors)
		{
			if (!AddBehavior(Behavior.Kind, 0, Behavior.FunctionId))
			{
				return Failure(EAngelscriptCacheCleanCaptureError::NotCacheable,
					TEXT("A root-class singleton behavior has no stable function reference"));
			}
		}
		if (ScriptType->beh.construct != 0)
		{
			TypeSchema.TypeSemanticFlags |= static_cast<uint32>(
				EAngelscriptCachedTypeSemanticFlags::HasDefaultConstructor);
		}
		if (ScriptType->beh.destruct != 0)
		{
			TypeSchema.TypeSemanticFlags |= static_cast<uint32>(
				EAngelscriptCachedTypeSemanticFlags::HasDestructor);
		}
		TypeSchema.Dependencies.Sort([](
			const FAngelscriptCacheSemanticDependency& Left,
			const FAngelscriptCacheSemanticDependency& Right)
		{
			return FAngelscriptCacheTypeSchemaArchive::CompareDependencies(
				Left, Right) < 0;
		});
		IdentityResult = FAngelscriptCacheTypeSchemaArchive::
			ComputeTypeLayoutHash(
				TypeSchema, TypeSchema.Layout.TypeLayoutHash);
		if (!IdentityResult.IsSuccess())
		{
			return ValidationFailure(
				EAngelscriptCacheCleanCaptureError::RecordEncodingFailed,
				TEXT("Class type layout hash"), IdentityResult);
		}

		OutAuthority.ModuleKey = ModuleKey;
		OutAuthority.ModuleInterface.PayloadSchemaVersion =
			FAngelscriptCacheSemanticArchive::
				ModuleInterfacePayloadSchemaVersion;
		OutAuthority.ModuleInterface.ModuleKey = ModuleKey;
		OutAuthority.ModuleInterface.CanonicalModuleName =
			Module->ModuleName;
		if (!ClassNamespace.IsEmpty())
		{
			OutAuthority.ModuleInterface.CanonicalNamespaces.AddUnique(
				ClassNamespace);
		}
		OutAuthority.ModuleInterface.Declarations.Add(TypeDeclaration);
		OutAuthority.ModuleInterface.Declarations.Append(PropertyDeclarations);
		for (FFunctionPlan& Plan : Functions)
		{
			if (!Plan.Authority.Declaration.CanonicalNamespace.IsEmpty())
			{
				OutAuthority.ModuleInterface.CanonicalNamespaces.AddUnique(
					Plan.Authority.Declaration.CanonicalNamespace);
			}
			OutAuthority.ModuleInterface.Declarations.Add(
				Plan.Authority.Declaration);
			OutAuthority.Functions.Add(MoveTemp(Plan.Authority));
		}
		IdentityResult = FAngelscriptCacheSemanticArchive::
			ComputeModuleInterfaceAbi(
				OutAuthority.ModuleInterface,
				OutAuthority.ModuleInterface.InterfaceAbi);
		if (!IdentityResult.IsSuccess())
		{
			return ValidationFailure(
				EAngelscriptCacheCleanCaptureError::RecordEncodingFailed,
				TEXT("Module interface ABI"), IdentityResult);
		}
		OutAuthority.TypeSchemas.Add(MoveTemp(TypeSchema));

		FAngelscriptCacheCleanCaptureResult Result;
		Result.Detail = FString::Printf(
			TEXT("Prepared current root-class semantic authority for module %s with one type, %d properties and %d functions"),
			*Module->ModuleName,
			PropertyDeclarations.Num(),
			OutAuthority.Functions.Num());
		return Result;
	}
}

FAngelscriptCacheCleanCaptureResult
BuildAngelscriptCacheCurrentModuleAuthority(
	const TSharedRef<FAngelscriptModuleDesc>& Module,
	const FAngelscriptCacheCleanCaptureOptions& Options,
	const FAngelscriptStableModuleKey& ModuleKey,
	FAngelscriptCacheCurrentModuleAuthority& OutAuthority)
{
	using namespace AngelscriptCacheCurrentModuleAuthority_Private;
	OutAuthority.Reset();
	if (ModuleKey.Hash.IsZero() || Options.Profile.Hash.IsZero()
		|| Module->bCompileError || Module->ScriptModule == nullptr
		|| Module->ModuleName.IsEmpty())
	{
		return Failure(EAngelscriptCacheCleanCaptureError::InvalidInput,
			TEXT("Current module authority requires a successful staged module, Profile and ModuleKey"));
	}
	if (Module->Classes.Num() >= 2)
	{
		return BuildClassGraphMethodAuthority(
			Module, Options, ModuleKey, OutAuthority);
	}
	if (Module->Classes.Num() == 1)
	{
		return BuildRootClassMethodAuthority(
			Module, Options, ModuleKey, OutAuthority);
	}
	if (Module->Enums.Num() > 1 || !Module->Classes.IsEmpty()
		|| !Module->Delegates.IsEmpty() || !Module->ImportedModules.IsEmpty()
		|| !Module->PostInitFunctions.IsEmpty())
	{
		return Failure(EAngelscriptCacheCleanCaptureError::NotCacheable,
			TEXT("Current module authority currently admits zero-or-one enum with global functions and no class, delegate, import or post-init declarations"));
	}

	asCModule* ScriptModule = Module->ScriptModule;
	if (ScriptModule->GetObjectTypeCount() != 0
		|| ScriptModule->GetTypedefCount() != 0
		|| ScriptModule->GetGlobalVarCount() != 0
		|| ScriptModule->GetImportedFunctionCount() != 0
		|| ScriptModule->GetEnumCount()
			!= static_cast<asUINT>(Module->Enums.Num())
		|| ScriptModule->GetFunctionCount() == 0)
	{
		return Failure(EAngelscriptCacheCleanCaptureError::NotCacheable,
			TEXT("Current module authority requires zero-or-one enum and one or more global functions with no globals/imports/object/typedef declarations"));
	}

	OutAuthority.ModuleKey = ModuleKey;
	OutAuthority.ModuleInterface.PayloadSchemaVersion =
		FAngelscriptCacheSemanticArchive::ModuleInterfacePayloadSchemaVersion;
	OutAuthority.ModuleInterface.ModuleKey = ModuleKey;
	OutAuthority.ModuleInterface.CanonicalModuleName = Module->ModuleName;

	FAngelscriptCacheValidationResult IdentityResult;
	if (Module->Enums.Num() == 1)
	{
		asITypeInfo* EnumType = ScriptModule->GetEnumByIndex(0);
		const TSharedRef<FAngelscriptEnumDesc>& EnumDesc = Module->Enums[0];
		if (EnumType == nullptr || EnumType->GetEnumValueCount() == 0)
		{
			return Failure(EAngelscriptCacheCleanCaptureError::NotCacheable,
				TEXT("The current compiled AS enum authority is absent or empty"));
		}

		const FString EnumNamespace = UTF8_TO_TCHAR(EnumType->GetNamespace());
		const FString EnumName = UTF8_TO_TCHAR(EnumType->GetName());
		if (!EnumDesc->EnumName.Equals(
			EnumName, ESearchCase::CaseSensitive))
		{
			return Failure(EAngelscriptCacheCleanCaptureError::NotCacheable,
				TEXT("The enum descriptor name does not own the current compiled AS enum"));
		}
		const FString EnumDeclaration = FString::Printf(
			TEXT("enum %s"), *EnumName);
		FAngelscriptCachedDeclaration Declaration;
		Declaration.DeclarationKind = EAngelscriptCacheDeclarationKind::Type;
		Declaration.EntityKind = EAngelscriptArtifactEntityKind::Enum;
		Declaration.SchemaCoverage = EAngelscriptCacheSchemaCoverage::Required;
		Declaration.BodyCoverage = EAngelscriptCacheBodyCoverage::Forbidden;
		Declaration.OwnerKind = EAngelscriptFunctionOwnerKind::Module;
		Declaration.OwnerKey = ModuleKey.Hash;
		Declaration.ModuleKey = ModuleKey;
		Declaration.CanonicalNamespace = EnumNamespace;
		Declaration.CanonicalName = EnumName;
		Declaration.CanonicalDeclaration = EnumDeclaration;
		Declaration.Slots.Add({
			EAngelscriptCacheDeclarationSlotKind::Declaration, 0});
		FAngelscriptTypeIdentityDescriptor TypeIdentity;
		TypeIdentity.ModuleKey = ModuleKey;
		TypeIdentity.Namespace = EnumNamespace;
		TypeIdentity.Kind = EAngelscriptArtifactEntityKind::Enum;
		TypeIdentity.CanonicalDeclaration = EnumDeclaration;
		Declaration.StableKey =
			FAngelscriptArtifactIdentityBuilder::BuildTypeKey(TypeIdentity).Hash;
		IdentityResult =
			FAngelscriptCacheSemanticArchive::ComputeDeclarationHashes(
				Declaration,
				Declaration.SignatureHash,
				Declaration.TraitsHash);
		if (!IdentityResult.IsSuccess())
		{
			return ValidationFailure(
				EAngelscriptCacheCleanCaptureError::RecordEncodingFailed,
				TEXT("Enum declaration hashes"), IdentityResult);
		}
		if (!EnumNamespace.IsEmpty())
		{
			OutAuthority.ModuleInterface.CanonicalNamespaces.Add(EnumNamespace);
		}
		OutAuthority.ModuleInterface.Declarations.Add(Declaration);

		FAngelscriptCachedTypeSchema& Schema =
			OutAuthority.TypeSchemas.AddDefaulted_GetRef();
		Schema.PayloadSchemaVersion =
			FAngelscriptCacheTypeSchemaArchive::TypeSchemaPayloadSchemaVersion;
		Schema.ModuleKey = ModuleKey;
		Schema.TypeKey = FAngelscriptStableTypeKey{Declaration.StableKey};
		Schema.TypeKind = EAngelscriptCachedTypeKind::Enum;
		Schema.CanonicalNamespace = EnumNamespace;
		Schema.CanonicalName = EnumName;
		Schema.CanonicalDeclaration = EnumDeclaration;
		Schema.TypeSemanticFlags = static_cast<uint32>(
			EAngelscriptCachedTypeSemanticFlags::Final)
			| static_cast<uint32>(
				EAngelscriptCachedTypeSemanticFlags::ValueType);
		Schema.Layout.SemanticSize = 1;
		Schema.Layout.SemanticAlignment = 1;
		// ScriptType and Enum are intentionally assigned later by ClassGenerator.
		// The descriptor itself is the pre-compile reflection declaration authority.
		Schema.Reflection.ReflectionKind =
			EAngelscriptCachedReflectionKind::UEnum;
		FAngelscriptCachedEnumTypePayload EnumPayload;
		for (asUINT Index = 0; Index < EnumType->GetEnumValueCount(); ++Index)
		{
			int Value = 0;
			const char* Name = EnumType->GetEnumValueByIndex(Index, &Value);
			if (Name == nullptr)
			{
				return Failure(EAngelscriptCacheCleanCaptureError::NotCacheable,
					TEXT("The current compiled enum contains an unnamed enumerator"));
			}
			EnumPayload.OrderedEnumerators.Add({
				Index, UTF8_TO_TCHAR(Name), Value, {}});
		}
		Schema.KindPayload.Enum = MoveTemp(EnumPayload);
		IdentityResult =
			FAngelscriptCacheTypeSchemaArchive::ComputeEnumAuthorityHash(
				Schema.TypeKey, Schema.KindPayload.Enum.GetValue(),
				Schema.KindPayload.Enum->EnumAuthorityHash);
		if (!IdentityResult.IsSuccess())
		{
			return ValidationFailure(
				EAngelscriptCacheCleanCaptureError::RecordEncodingFailed,
				TEXT("Enum authority hash"), IdentityResult);
		}
		IdentityResult =
			FAngelscriptCacheTypeSchemaArchive::ComputeTypeLayoutHash(
				Schema, Schema.Layout.TypeLayoutHash);
		if (!IdentityResult.IsSuccess())
		{
			return ValidationFailure(
				EAngelscriptCacheCleanCaptureError::RecordEncodingFailed,
				TEXT("Enum layout hash"), IdentityResult);
		}
	}

	OutAuthority.Functions.Reserve(
		static_cast<int32>(ScriptModule->GetFunctionCount()));
	for (asUINT FunctionIndex = 0;
		FunctionIndex < ScriptModule->GetFunctionCount(); ++FunctionIndex)
	{
		asIScriptFunction* PublicFunction =
			ScriptModule->GetFunctionByIndex(FunctionIndex);
		if (PublicFunction == nullptr
			|| PublicFunction->GetFuncType() != asFUNC_SCRIPT
			|| PublicFunction->GetObjectType() != nullptr
			|| PublicFunction->GetParamCount() != 0
			|| PublicFunction->GetReturnTypeId() != asTYPEID_INT32)
		{
			return Failure(EAngelscriptCacheCleanCaptureError::NotCacheable,
				TEXT("Every current authority function must be a global int function with no parameters"));
		}

		FAngelscriptCacheCurrentFunctionAuthority& FunctionAuthority =
			OutAuthority.Functions.AddDefaulted_GetRef();
		FunctionAuthority.Function =
			static_cast<asCScriptFunction*>(PublicFunction);
		FAngelscriptCachedDeclaration& Declaration =
			FunctionAuthority.Declaration;
		Declaration.DeclarationKind = EAngelscriptCacheDeclarationKind::Function;
		Declaration.EntityKind = EAngelscriptArtifactEntityKind::GlobalFunction;
		Declaration.SchemaCoverage = EAngelscriptCacheSchemaCoverage::Forbidden;
		Declaration.BodyCoverage = EAngelscriptCacheBodyCoverage::Required;
		Declaration.OwnerKind = EAngelscriptFunctionOwnerKind::Module;
		Declaration.OwnerKey = ModuleKey.Hash;
		Declaration.ModuleKey = ModuleKey;
		Declaration.CanonicalNamespace =
			UTF8_TO_TCHAR(PublicFunction->GetNamespace());
		Declaration.CanonicalName = UTF8_TO_TCHAR(PublicFunction->GetName());
		Declaration.CanonicalDeclaration =
			UTF8_TO_TCHAR(PublicFunction->GetDeclaration(false, false, false));
		Declaration.DeclaredType = MakeInt32Type();
		Declaration.Slots.Add({
			EAngelscriptCacheDeclarationSlotKind::Function, FunctionIndex});
		FAngelscriptFunctionIdentityDescriptor FunctionIdentity;
		FunctionIdentity.OwnerKind = Declaration.OwnerKind;
		FunctionIdentity.OwnerKey = Declaration.OwnerKey;
		FunctionIdentity.Namespace = Declaration.CanonicalNamespace;
		FunctionIdentity.Kind = Declaration.EntityKind;
		FunctionIdentity.CanonicalDeclaration = Declaration.CanonicalDeclaration;
		Declaration.StableKey =
			FAngelscriptArtifactIdentityBuilder::BuildFunctionKey(
				FunctionIdentity).Hash;
		IdentityResult =
			FAngelscriptCacheSemanticArchive::ComputeDeclarationHashes(
				Declaration,
				Declaration.SignatureHash,
				Declaration.TraitsHash);
		if (!IdentityResult.IsSuccess())
		{
			return ValidationFailure(
				EAngelscriptCacheCleanCaptureError::RecordEncodingFailed,
				TEXT("Function declaration hashes"), IdentityResult);
		}
		if (!Declaration.CanonicalNamespace.IsEmpty())
		{
			OutAuthority.ModuleInterface.CanonicalNamespaces.AddUnique(
				Declaration.CanonicalNamespace);
		}
		OutAuthority.ModuleInterface.Declarations.Add(Declaration);
	}

	IdentityResult =
		FAngelscriptCacheSemanticArchive::ComputeModuleInterfaceAbi(
			OutAuthority.ModuleInterface,
			OutAuthority.ModuleInterface.InterfaceAbi);
	if (!IdentityResult.IsSuccess())
	{
		return ValidationFailure(
			EAngelscriptCacheCleanCaptureError::RecordEncodingFailed,
			TEXT("Module interface ABI"), IdentityResult);
	}

	OutAuthority.ModuleState.PayloadSchemaVersion =
		FAngelscriptCacheRemainingRecordArchive::ModuleStatePayloadSchemaVersion;
	OutAuthority.ModuleState.ModuleKey = ModuleKey;
	OutAuthority.ModuleState.Profile = Options.Profile;
	IdentityResult = FAngelscriptCacheRemainingRecordArchive::
		ComputeModuleStateInputHash(
			OutAuthority.ModuleState,
			OutAuthority.ModuleState.StateInputHash);
	if (!IdentityResult.IsSuccess())
	{
		return ValidationFailure(
			EAngelscriptCacheCleanCaptureError::RecordEncodingFailed,
			TEXT("Module state input hash"), IdentityResult);
	}

	FAngelscriptCacheCleanCaptureResult Result;
	Result.Detail = FString::Printf(
		TEXT("Prepared current semantic authority for module %s with %d type(s) and %d function(s)"),
		*Module->ModuleName,
		OutAuthority.TypeSchemas.Num(),
		OutAuthority.Functions.Num());
	return Result;
}
