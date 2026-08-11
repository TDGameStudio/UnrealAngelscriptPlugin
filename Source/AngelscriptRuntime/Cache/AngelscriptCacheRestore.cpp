#include "Cache/AngelscriptCacheRestore.h"

#include "AngelscriptEngine.h"
#include "Cache/AngelscriptCacheEnvironment.h"
#include "Cache/AngelscriptFunctionArtifactCodec.h"
#include "Cache/AngelscriptCacheService.h"
#include "Cache/AngelscriptCacheStableSymbolIdentity.h"
#include "Cache/Private/AngelscriptCacheRuntimeState.h"
#include "ClassGenerator/AngelscriptClassGenerator.h"
#include "Core/AngelscriptType.h"
#include "Hash/Blake3.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_module.h"
#include "source/as_objecttype.h"
#include "source/as_property.h"
#include "source/as_scriptengine.h"
#include "source/as_scriptfunction.h"
#include "source/as_typeinfo.h"
#include "EndAngelscriptHeaders.h"

namespace AngelscriptCacheRestore_Private
{
	struct FFunctionRestoreInput final
	{
		const FAngelscriptCacheValidatedFunctionOrdinal* Ordinal = nullptr;
		const FAngelscriptCachedDeclaration* Declaration = nullptr;
		const FAngelscriptCachedFunctionBody* Body = nullptr;
		const FAngelscriptCachedDebugSidecar* Debug = nullptr;
		asCScriptFunction* LiveFunction = nullptr;
	};

	struct FClassRestoreType final
	{
		const FAngelscriptCachedTypeSchema* Schema = nullptr;
		const FAngelscriptCachedTypeSchema* BaseSchema = nullptr;
		asCObjectType* LiveType = nullptr;
		UClass* CodeRoot = nullptr;
	};

	static const FClassRestoreType* FindClassRestoreType(
		const TConstArrayView<FClassRestoreType> Types,
		const FAngelscriptHash256& TypeKey)
	{
		const FClassRestoreType* Match = nullptr;
		for (const FClassRestoreType& Type : Types)
		{
			if (Type.Schema == nullptr || Type.Schema->TypeKey.Hash != TypeKey)
			{
				continue;
			}
			if (Match != nullptr)
			{
				return nullptr;
			}
			Match = &Type;
		}
		return Match;
	}

	static FClassRestoreType* FindClassRestoreType(
		const TArrayView<FClassRestoreType> Types,
		const FAngelscriptHash256& TypeKey)
	{
		FClassRestoreType* Match = nullptr;
		for (FClassRestoreType& Type : Types)
		{
			if (Type.Schema == nullptr || Type.Schema->TypeKey.Hash != TypeKey)
			{
				continue;
			}
			if (Match != nullptr)
			{
				return nullptr;
			}
			Match = &Type;
		}
		return Match;
	}

	static FAngelscriptCacheRestoreResult Failure(
		const EAngelscriptCacheRestoreError Error,
		const EAngelscriptCacheRestoreStage Stage,
		FString&& Detail,
		const TOptional<FAngelscriptCacheValidationResult>& Validation = {})
	{
		FAngelscriptCacheRestoreResult Result;
		Result.Error = Error;
		Result.Stage = Stage;
		Result.Detail = MoveTemp(Detail);
		Result.Validation = Validation;
		return Result;
	}

	static const FAngelscriptDecodedCacheRecord* FindRecord(
		const FAngelscriptValidatedGeneration& Generation,
		const FAngelscriptCacheRecordId& RecordId)
	{
		for (const FAngelscriptDecodedCacheRecordHandle& Record
			: Generation.ReachableRecords)
		{
			if (Record->GetRecordId() == RecordId)
			{
				return &Record.Get();
			}
		}
		return nullptr;
	}

	static const FAngelscriptDecodedCacheRecord* FindGraphRecord(
		const FAngelscriptValidatedModuleGraph& Graph,
		const uint32 RecordOrdinal)
	{
		const TConstArrayView<FAngelscriptDecodedCacheRecordHandle> Records =
			Graph.GetReachableRecords();
		return RecordOrdinal < static_cast<uint32>(Records.Num())
			? &Records[RecordOrdinal].Get()
			: nullptr;
	}

	class FSelfContainedCurrentSymbols final
		: public IAngelscriptCacheCurrentSymbolResolver
	{
	public:
		virtual TOptional<FAngelscriptCacheCurrentSymbol> Resolve(
			EAngelscriptCacheReferenceKind,
			const FAngelscriptHash256&) const override
		{
			return {};
		}
	};

	class FSelfContainedCurrentLayouts final
		: public IAngelscriptCacheCurrentLayoutResolver
	{
	public:
		virtual TOptional<FAngelscriptCacheResolvedDataTypeLayout>
		ResolveDataTypeLayout(
			const FAngelscriptCachedDataType&,
			const IAngelscriptCacheProspectiveTypeLayoutView&) const override
		{
			return {};
		}

		virtual TOptional<FAngelscriptCacheResolvedTypeLayoutInput>
		ResolveTypeLayoutInput(
			EAngelscriptCachedTypeLayoutInputKind,
			EAngelscriptCacheReferenceKind,
			const FAngelscriptHash256&) const override
		{
			return {};
		}
	};

	static FString BuildLogicalVirtualPath(
		const FAngelscriptCachedSourceIndex& SourceIndex,
		const FAngelscriptCachedSourceFile& File)
	{
		for (const FAngelscriptCachedSourceMount& Mount : SourceIndex.Mounts)
		{
			if (Mount.MountKey.Hash == File.MountKey.Hash)
			{
				FString Result = Mount.LogicalMount;
				if (!Result.StartsWith(TEXT("/")))
				{
					Result = TEXT("/") + Result;
				}
				if (!Result.EndsWith(TEXT("/")))
				{
					Result += TEXT("/");
				}
				Result += File.RelativeLogicalPath;
				return Result;
			}
		}
		return {};
	}

	static FAngelscriptHash256 HashRawSourceBytes(
		const TConstArrayView<uint8> Bytes)
	{
		return FAngelscriptHash256{FBlake3::HashBuffer(
			Bytes.GetData(), static_cast<uint64>(Bytes.Num()))};
	}

	static bool ValidateCurrentSourceProjection(
		const FAngelscriptCachedSourceIndex& SourceIndex,
		const FAngelscriptStableModuleKey& ModuleKey,
		const TConstArrayView<FAngelscriptCacheCurrentSourceProjection>
			CurrentSources,
		TArray<const FAngelscriptCacheCurrentSourceProjection*>&
			OutModuleSources,
		FString& OutFailure)
	{
		OutModuleSources.Reset();
		OutFailure.Reset();
		if (CurrentSources.IsEmpty())
		{
			return true;
		}

		int32 PersistedModuleSourceCount = 0;
		for (const FAngelscriptCachedSourceFile& File : SourceIndex.Files)
		{
			if (File.ModuleKey != ModuleKey)
			{
				continue;
			}
			++PersistedModuleSourceCount;
			const FAngelscriptCacheCurrentSourceProjection* Match = nullptr;
			for (const FAngelscriptCacheCurrentSourceProjection& Current
				: CurrentSources)
			{
				if (Current.SourceFileKey.Hash == File.SourceFileKey.Hash)
				{
					if (Match != nullptr)
					{
						OutFailure = TEXT(
							"The current source projection contains a duplicate SourceFileKey");
						return false;
					}
					Match = &Current;
				}
			}
			const FString ExpectedVirtualPath =
				BuildLogicalVirtualPath(SourceIndex, File);
			if (Match == nullptr || Match->ModuleKey != ModuleKey
				|| !Match->VirtualPath.Equals(
					ExpectedVirtualPath, ESearchCase::CaseSensitive)
				|| !Match->RelativeFilename.Equals(
					File.RelativeLogicalPath, ESearchCase::CaseSensitive)
				|| Match->AbsoluteFilename.IsEmpty()
				|| HashRawSourceBytes(Match->RawSourceBytes)
					!= File.RawContentHash)
			{
				OutFailure = FString::Printf(
					TEXT("The current source projection does not match persisted source %s"),
					*File.SourceFileKey.Hash.ToHexString());
				return false;
			}
			OutModuleSources.Add(Match);
		}

		int32 CurrentModuleSourceCount = 0;
		for (const FAngelscriptCacheCurrentSourceProjection& Current
			: CurrentSources)
		{
			CurrentModuleSourceCount += Current.ModuleKey == ModuleKey ? 1 : 0;
		}
		if (PersistedModuleSourceCount == 0
			|| CurrentModuleSourceCount != PersistedModuleSourceCount)
		{
			OutFailure = TEXT(
				"The current and persisted module source sets are not one-to-one");
			OutModuleSources.Reset();
			return false;
		}
		return true;
	}

	static bool IsSupportedPrimitiveGlobalDeclaration(
		const FAngelscriptCachedDeclaration& Declaration)
	{
		return Declaration.DeclarationKind
				== EAngelscriptCacheDeclarationKind::Function
			&& Declaration.EntityKind
				== EAngelscriptArtifactEntityKind::GlobalFunction
			&& Declaration.OwnerKind == EAngelscriptFunctionOwnerKind::Module
			&& Declaration.DeclaredType.IsSet()
			&& Declaration.DeclaredType->Kind
				== EAngelscriptCachedDataTypeKind::Primitive
			&& Declaration.OrderedParameters.IsEmpty();
	}

	static bool DoesLiveFunctionMatchDeclaration(
		const asCScriptFunction& Function,
		const FAngelscriptCachedDeclaration& Declaration)
	{
		return Declaration.CanonicalName.Equals(
			UTF8_TO_TCHAR(Function.GetName()), ESearchCase::CaseSensitive)
			&& Declaration.CanonicalNamespace.Equals(
				UTF8_TO_TCHAR(Function.GetNamespace()),
				ESearchCase::CaseSensitive)
			&& Declaration.CanonicalDeclaration.Equals(
				UTF8_TO_TCHAR(Function.GetDeclaration(false, false, false)),
				ESearchCase::CaseSensitive)
			&& Function.GetParamCount() == 0
			&& Function.GetObjectType() == nullptr;
	}

	static bool HasFlag(const uint32 Value,
		const EAngelscriptCachedTypeQualifierFlags Flag)
	{
		return (Value & static_cast<uint32>(Flag)) != 0;
	}

	static bool HasFlag(const uint32 Value,
		const EAngelscriptCachedDeclarationTraitFlags Flag)
	{
		return (Value & static_cast<uint32>(Flag)) != 0;
	}

	static bool HasFlag(const uint32 Value,
		const EAngelscriptCachedTypeSemanticFlags Flag)
	{
		return (Value & static_cast<uint32>(Flag)) != 0;
	}

	static bool HasFlag(const uint32 Value,
		const EAngelscriptCachedClassReflectionFlags Flag)
	{
		return (Value & static_cast<uint32>(Flag)) != 0;
	}

	static bool HasFlag(const uint32 Value,
		const EAngelscriptCachedPropertySemanticFlags Flag)
	{
		return (Value & static_cast<uint32>(Flag)) != 0;
	}

	static bool HasFlag(const uint32 Value,
		const EAngelscriptCachedReflectionFlags Flag)
	{
		return (Value & static_cast<uint32>(Flag)) != 0;
	}

	static bool HasFlag(const uint32 Value,
		const EAngelscriptCachedParameterTraitFlags Flag)
	{
		return (Value & static_cast<uint32>(Flag)) != 0;
	}

	static bool TryMapPrimitiveToken(
		const EAngelscriptCachedPrimitiveType Primitive,
		eTokenType& OutToken)
	{
		switch (Primitive)
		{
		case EAngelscriptCachedPrimitiveType::Void: OutToken = ttVoid; return true;
		case EAngelscriptCachedPrimitiveType::Bool: OutToken = ttBool; return true;
		case EAngelscriptCachedPrimitiveType::Int8: OutToken = ttInt8; return true;
		case EAngelscriptCachedPrimitiveType::Int16: OutToken = ttInt16; return true;
		case EAngelscriptCachedPrimitiveType::Int32: OutToken = ttInt; return true;
		case EAngelscriptCachedPrimitiveType::Int64: OutToken = ttInt64; return true;
		case EAngelscriptCachedPrimitiveType::UInt8: OutToken = ttUInt8; return true;
		case EAngelscriptCachedPrimitiveType::UInt16: OutToken = ttUInt16; return true;
		case EAngelscriptCachedPrimitiveType::UInt32: OutToken = ttUInt; return true;
		case EAngelscriptCachedPrimitiveType::UInt64: OutToken = ttUInt64; return true;
		case EAngelscriptCachedPrimitiveType::Float32: OutToken = ttFloat32; return true;
		case EAngelscriptCachedPrimitiveType::Float64: OutToken = ttFloat64; return true;
		default: return false;
		}
	}

	static bool TryMaterializeDataType(
		const FAngelscriptCachedDataType& Cached,
		const TConstArrayView<FClassRestoreType> ClassTypes,
		const FAngelscriptCacheEngineEnvironmentResolver& Environment,
		asCDataType& OutType)
	{
		if (!Cached.OrderedSubTypes.IsEmpty()
			|| HasFlag(Cached.QualifierFlags,
				EAngelscriptCachedTypeQualifierFlags::Auto))
		{
			return false;
		}

		if (Cached.Kind == EAngelscriptCachedDataTypeKind::Primitive)
		{
			eTokenType Token = ttUnrecognizedToken;
			if (!TryMapPrimitiveToken(Cached.Primitive, Token))
			{
				return false;
			}
			OutType = asCDataType::CreatePrimitive(Token, false);
		}
		else if (Cached.Kind == EAngelscriptCachedDataTypeKind::ScriptType)
		{
			if (!Cached.TypeReference.IsSet()
				|| Cached.TypeReference->Kind
					!= EAngelscriptCacheReferenceKind::ScriptType)
			{
				return false;
			}
			const FClassRestoreType* Type = FindClassRestoreType(
				ClassTypes, Cached.TypeReference->StableKey);
			if (Type == nullptr || Type->LiveType == nullptr)
			{
				return false;
			}
			OutType = asCDataType::CreateType(Type->LiveType, false);
		}
		else if (Cached.Kind == EAngelscriptCachedDataTypeKind::EnvironmentType)
		{
			asCTypeInfo* EnvironmentType = Cached.TypeReference.IsSet()
				? Environment.ResolveTypeReference(Cached.TypeReference.GetValue())
				: nullptr;
			if (EnvironmentType == nullptr)
			{
				return false;
			}
			OutType = asCDataType::CreateType(EnvironmentType, false);
		}
		else
		{
			return false;
		}

		if (HasFlag(Cached.QualifierFlags,
			EAngelscriptCachedTypeQualifierFlags::ObjectHandle)
			&& OutType.MakeHandle(true, true) < 0)
		{
			return false;
		}
		if (HasFlag(Cached.QualifierFlags,
			EAngelscriptCachedTypeQualifierFlags::ConstHandle)
			&& OutType.MakeHandleToConst(true) < 0)
		{
			return false;
		}
		if (HasFlag(Cached.QualifierFlags,
			EAngelscriptCachedTypeQualifierFlags::ObjectConst)
			&& OutType.MakeReadOnly(true) < 0)
		{
			return false;
		}
		if (HasFlag(Cached.QualifierFlags,
			EAngelscriptCachedTypeQualifierFlags::Reference)
			&& OutType.MakeReference(true) < 0)
		{
			return false;
		}
		OutType.SetIfHandleThenConst(HasFlag(
			Cached.QualifierFlags,
			EAngelscriptCachedTypeQualifierFlags::IfHandleThenConst));
		return OutType.IsValid();
	}

	static asEBuildArtifactInvocationKind MapInvocationKind(
		const EAngelscriptCachedFunctionInvocationKind Kind)
	{
		switch (Kind)
		{
		case EAngelscriptCachedFunctionInvocationKind::GlobalFunction:
			return asBUILD_ARTIFACT_INVOCATION_GLOBAL_FUNCTION;
		case EAngelscriptCachedFunctionInvocationKind::Method:
			return asBUILD_ARTIFACT_INVOCATION_METHOD;
		case EAngelscriptCachedFunctionInvocationKind::Constructor:
			return asBUILD_ARTIFACT_INVOCATION_CONSTRUCTOR;
		case EAngelscriptCachedFunctionInvocationKind::Destructor:
			return asBUILD_ARTIFACT_INVOCATION_DESTRUCTOR;
		case EAngelscriptCachedFunctionInvocationKind::Factory:
			return asBUILD_ARTIFACT_INVOCATION_FACTORY;
		case EAngelscriptCachedFunctionInvocationKind::GeneratedDefaultConstructor:
			return asBUILD_ARTIFACT_INVOCATION_GENERATED_DEFAULT_CONSTRUCTOR;
		case EAngelscriptCachedFunctionInvocationKind::GeneratedDefaultDestructor:
			return asBUILD_ARTIFACT_INVOCATION_GENERATED_DEFAULT_DESTRUCTOR;
		case EAngelscriptCachedFunctionInvocationKind::InitDefaults:
			return asBUILD_ARTIFACT_INVOCATION_INIT_DEFAULTS;
		default:
			return asBUILD_ARTIFACT_INVOCATION_INVALID;
		}
	}

	static asSFunctionTraits BuildFunctionTraits(
		const FAngelscriptCachedDeclaration& Declaration)
	{
		asSFunctionTraits Traits;
		Traits.SetTrait(asTRAIT_CONST, HasFlag(Declaration.TraitFlags,
			EAngelscriptCachedDeclarationTraitFlags::Const));
		Traits.SetTrait(asTRAIT_PRIVATE, HasFlag(Declaration.TraitFlags,
			EAngelscriptCachedDeclarationTraitFlags::Private));
		Traits.SetTrait(asTRAIT_PROTECTED, HasFlag(Declaration.TraitFlags,
			EAngelscriptCachedDeclarationTraitFlags::Protected));
		Traits.SetTrait(asTRAIT_FINAL, HasFlag(Declaration.TraitFlags,
			EAngelscriptCachedDeclarationTraitFlags::Final));
		Traits.SetTrait(asTRAIT_OVERRIDE, HasFlag(Declaration.TraitFlags,
			EAngelscriptCachedDeclarationTraitFlags::Override));
		Traits.SetTrait(asTRAIT_GENERATED_FUNCTION,
			HasFlag(Declaration.TraitFlags,
				EAngelscriptCachedDeclarationTraitFlags::Generated));
		Traits.SetTrait(asTRAIT_SHARED, HasFlag(Declaration.TraitFlags,
			EAngelscriptCachedDeclarationTraitFlags::Shared));
		Traits.SetTrait(asTRAIT_EXTERNAL, HasFlag(Declaration.TraitFlags,
			EAngelscriptCachedDeclarationTraitFlags::External));
		Traits.SetTrait(asTRAIT_PROPERTY, HasFlag(Declaration.TraitFlags,
			EAngelscriptCachedDeclarationTraitFlags::Property));
		Traits.SetTrait(asTRAIT_IMPLICITCONSTRUCTOR,
			HasFlag(Declaration.TraitFlags,
				EAngelscriptCachedDeclarationTraitFlags::ImplicitConstructor));
		Traits.SetTrait(asTRAIT_MIXIN, HasFlag(Declaration.TraitFlags,
			EAngelscriptCachedDeclarationTraitFlags::Mixin));
		Traits.SetTrait(asTRAIT_LOCAL, HasFlag(Declaration.TraitFlags,
			EAngelscriptCachedDeclarationTraitFlags::Local));
		Traits.SetTrait(asTRAIT_NODISCARD, HasFlag(Declaration.TraitFlags,
			EAngelscriptCachedDeclarationTraitFlags::NoDiscard));
		Traits.SetTrait(asTRAIT_DEPRECATED, HasFlag(Declaration.TraitFlags,
			EAngelscriptCachedDeclarationTraitFlags::Deprecated));
		Traits.SetTrait(asTRAIT_GENERIC_TEMPLATE_FUNCTION,
			HasFlag(Declaration.TraitFlags,
				EAngelscriptCachedDeclarationTraitFlags::GenericTemplateFunction));
		Traits.SetTrait(asTRAIT_USES_WORLDCONTEXT,
			HasFlag(Declaration.TraitFlags,
				EAngelscriptCachedDeclarationTraitFlags::UsesWorldContext));
		Traits.SetTrait(asTRAIT_ACCEPT_TEMPORARY_OBJECT,
			HasFlag(Declaration.TraitFlags,
				EAngelscriptCachedDeclarationTraitFlags::AcceptTemporaryObject));
		Traits.SetTrait(asTRAIT_NOT_CALLABLE, HasFlag(Declaration.TraitFlags,
			EAngelscriptCachedDeclarationTraitFlags::NotCallable));
		Traits.SetTrait(asTRAIT_FORCE_CONST_ARGUMENT_EXPRESSIONS,
			HasFlag(Declaration.TraitFlags,
				EAngelscriptCachedDeclarationTraitFlags::ForceConstArgumentExpressions));
		Traits.SetTrait(asTRAIT_EXTERNAL_IMPLICIT_THIS,
			HasFlag(Declaration.TraitFlags,
				EAngelscriptCachedDeclarationTraitFlags::ExternalImplicitThis));
		Traits.SetTrait(asTRAIT_ALLOWDISCARD, HasFlag(Declaration.TraitFlags,
			EAngelscriptCachedDeclarationTraitFlags::AllowDiscard));
		Traits.SetTrait(asTRAIT_EDITOR_ONLY, HasFlag(Declaration.TraitFlags,
			EAngelscriptCachedDeclarationTraitFlags::EditorOnly));
		Traits.SetTrait(asTRAIT_EXPLICIT, HasFlag(Declaration.TraitFlags,
			EAngelscriptCachedDeclarationTraitFlags::Explicit));
		Traits.SetTrait(asTRAIT_UNSAFE_DURING_CONSTRUCTION,
			HasFlag(Declaration.TraitFlags,
				EAngelscriptCachedDeclarationTraitFlags::UnsafeDuringConstruction));
		Traits.SetTrait(asTRAIT_DEFAULTS_ONLY,
			HasFlag(Declaration.TraitFlags,
				EAngelscriptCachedDeclarationTraitFlags::DefaultsOnly));
		Traits.SetTrait(asTRAIT_CONSTRUCTOR,
			HasFlag(Declaration.TraitFlags,
				EAngelscriptCachedDeclarationTraitFlags::Constructor));
		Traits.SetTrait(asTRAIT_DESTRUCTOR,
			HasFlag(Declaration.TraitFlags,
				EAngelscriptCachedDeclarationTraitFlags::Destructor));
		return Traits;
	}

	static const FFunctionRestoreInput* FindFunctionInput(
		const TConstArrayView<FFunctionRestoreInput> Inputs,
		const FAngelscriptHash256& StableKey)
	{
		const FFunctionRestoreInput* Match = nullptr;
		for (const FFunctionRestoreInput& Input : Inputs)
		{
			if (Input.Body == nullptr
				|| Input.Body->Identity.FunctionKey.Hash != StableKey)
			{
				continue;
			}
			if (Match != nullptr)
			{
				return nullptr;
			}
			Match = &Input;
		}
		return Match;
	}

	static const FAngelscriptCachedDeclaration* FindDeclaration(
		const FAngelscriptCachedModuleInterface& ModuleInterface,
		const FAngelscriptHash256& StableKey)
	{
		const FAngelscriptCachedDeclaration* Match = nullptr;
		for (const FAngelscriptCachedDeclaration& Declaration
			: ModuleInterface.Declarations)
		{
			if (Declaration.StableKey != StableKey)
			{
				continue;
			}
			if (Match != nullptr)
			{
				return nullptr;
			}
			Match = &Declaration;
		}
		return Match;
	}

	static bool ResolveCodeRoot(
		asCScriptEngine& ScriptEngine,
		const FAngelscriptCachedTypeSchema& TypeSchema,
		const FAngelscriptCacheEngineEnvironmentResolver& Environment,
		UClass*& OutCodeRoot,
		asCObjectType*& OutShadowType,
		FString& OutFailure)
	{
		OutCodeRoot = nullptr;
		OutShadowType = nullptr;
		const FAngelscriptCacheStableReference* CodeSuper = nullptr;
		const FAngelscriptCacheStableReference* ShadowSuper = nullptr;
		for (const FAngelscriptCachedTypeRelation& Relation
			: TypeSchema.Relations)
		{
			if (Relation.RelationKind
				== EAngelscriptCachedTypeRelationKind::CodeSuper)
			{
				if (CodeSuper != nullptr)
				{
					OutFailure = TEXT("The cached class has duplicate code-root relations");
					return false;
				}
				CodeSuper = &Relation.Target;
			}
			else if (Relation.RelationKind
				== EAngelscriptCachedTypeRelationKind::ShadowSuper)
			{
				if (ShadowSuper != nullptr)
				{
					OutFailure = TEXT("The cached class has duplicate shadow-root relations");
					return false;
				}
				ShadowSuper = &Relation.Target;
			}
			else if (Relation.RelationKind
				== EAngelscriptCachedTypeRelationKind::Base)
			{
				// The local script base is resolved from the validated module type
				// graph. It does not replace the native code/shadow root.
				continue;
			}
			else
			{
				OutFailure = FString::Printf(
					TEXT("Class %s contains unsupported relation kind %u"),
					*TypeSchema.CanonicalName,
					static_cast<uint32>(Relation.RelationKind));
				return false;
			}
		}
		if (CodeSuper == nullptr || ShadowSuper == nullptr
			|| CodeSuper->StableKey != ShadowSuper->StableKey
			|| CodeSuper->ExpectedAbi != ShadowSuper->ExpectedAbi)
		{
			OutFailure = TEXT("The cached class has no single matching code/shadow root");
			return false;
		}

		OutCodeRoot = Environment.ResolveCodeRootReference(*CodeSuper);
		if (OutCodeRoot == nullptr)
		{
			OutFailure = TEXT("The cached native code root is absent or ambiguous in the current process");
			return false;
		}
		const FString BoundName = FAngelscriptType::GetBoundClassName(OutCodeRoot);
		OutShadowType = CastToObjectType(
			ScriptEngine.allRegisteredTypesByName.FindFirst_CaseInsensitive(
				TCHAR_TO_ANSI(*BoundName)));
		if (OutShadowType == nullptr || OutShadowType->module != nullptr)
		{
			OutFailure = TEXT("The current Engine has no compatible registered shadow type for the code root");
			return false;
		}
		return true;
	}

	static bool MaterializeClassType(
		asCScriptEngine& ScriptEngine,
		asCModule& Module,
		const FAngelscriptCachedTypeSchema& TypeSchema,
		const FAngelscriptCacheEngineEnvironmentResolver& Environment,
		asCObjectType* BaseLiveType,
		const TArrayView<FClassRestoreType> ClassTypes,
		asCObjectType*& OutType,
		UClass*& OutCodeRoot,
		FString& OutFailure)
	{
		OutType = nullptr;
		asCObjectType* ShadowType = nullptr;
		if (TypeSchema.TypeKind != EAngelscriptCachedTypeKind::Class
			|| TypeSchema.Reflection.ReflectionKind
				!= EAngelscriptCachedReflectionKind::UClass
			|| TypeSchema.Layout.SemanticSize > MAX_int32
			|| TypeSchema.Layout.SemanticAlignment == 0
			|| !FMath::IsPowerOfTwo(TypeSchema.Layout.SemanticAlignment)
			|| !ResolveCodeRoot(ScriptEngine, TypeSchema, Environment,
				OutCodeRoot, ShadowType, OutFailure))
		{
			if (OutFailure.IsEmpty())
			{
				OutFailure = TEXT("The cached type is outside the root-UClass materialization shape");
			}
			return false;
		}
		const uint32 ExpectedBaseBoundary = BaseLiveType != nullptr
			? static_cast<uint32>(BaseLiveType->size)
			: static_cast<uint32>(OutCodeRoot->GetPropertiesSize());
		if (TypeSchema.Layout.BasePropertyBoundary != ExpectedBaseBoundary)
		{
			OutFailure = FString::Printf(
				TEXT("Class %s base boundary mismatch: cached=%u current=%u"),
				*TypeSchema.CanonicalName,
				TypeSchema.Layout.BasePropertyBoundary,
				ExpectedBaseBoundary);
			return false;
		}

		asCObjectType* LiveType = asNEW(asCObjectType)(&ScriptEngine);
		if (LiveType == nullptr)
		{
			OutFailure = TEXT("The target VM could not allocate a cached class type");
			return false;
		}
		LiveType->name = TCHAR_TO_UTF8(*TypeSchema.CanonicalName);
		LiveType->nameSpace = ScriptEngine.AddNameSpace(
			TCHAR_TO_UTF8(*TypeSchema.CanonicalNamespace));
		LiveType->module = &Module;
		LiveType->flags = asOBJ_REF | asOBJ_SCRIPT_OBJECT | asOBJ_NOCOUNT
			| asOBJ_IMPLICIT_HANDLE;
		if (HasFlag(TypeSchema.TypeSemanticFlags,
			EAngelscriptCachedTypeSemanticFlags::Abstract))
		{
			LiveType->flags |= asOBJ_ABSTRACT;
		}
		if (HasFlag(TypeSchema.TypeSemanticFlags,
			EAngelscriptCachedTypeSemanticFlags::Final))
		{
			LiveType->flags |= asOBJ_NOINHERIT;
		}
		if (HasFlag(TypeSchema.TypeSemanticFlags,
			EAngelscriptCachedTypeSemanticFlags::Shared))
		{
			LiveType->flags |= asOBJ_SHARED;
		}
		LiveType->size = static_cast<int>(TypeSchema.Layout.SemanticSize);
		LiveType->alignment = static_cast<int>(
			TypeSchema.Layout.SemanticAlignment);
		LiveType->basePropertyOffset = static_cast<int>(
			TypeSchema.Layout.BasePropertyBoundary);
		LiveType->shadowType = ShadowType;
		LiveType->derivedFrom = BaseLiveType;
		if (BaseLiveType != nullptr)
		{
			BaseLiveType->AddRefInternal();
		}
		Module.AddClassType(LiveType);
		ScriptEngine.allScriptDeclaredTypes.Add(LiveType);
		ScriptEngine.GetTypeIdFromDataType(
			asCDataType::CreateType(LiveType, false));
		FClassRestoreType* CurrentType = FindClassRestoreType(
			ClassTypes, TypeSchema.TypeKey.Hash);
		if (CurrentType == nullptr)
		{
			OutFailure = FString::Printf(
				TEXT("Class %s has no unique restore-graph authority"),
				*TypeSchema.CanonicalName);
			return false;
		}
		CurrentType->LiveType = LiveType;
		CurrentType->CodeRoot = OutCodeRoot;

		OutType = LiveType;
		return true;
	}

	static bool MaterializeClassProperties(
		const FAngelscriptCachedTypeSchema& TypeSchema,
		asCObjectType& LiveType,
		const TConstArrayView<FClassRestoreType> ClassTypes,
		const FAngelscriptCacheEngineEnvironmentResolver& Environment,
		FString& OutFailure)
	{
		for (int32 Index = 0; Index < TypeSchema.OrderedProperties.Num(); ++Index)
		{
			const FAngelscriptCachedPropertySchema& Property =
				TypeSchema.OrderedProperties[Index];
			asCDataType DataType;
			if (Property.LayoutOrdinal != static_cast<uint32>(Index)
				|| Property.SemanticByteOffset > MAX_int32
				|| !TryMaterializeDataType(
					Property.Type, ClassTypes, Environment, DataType))
			{
				OutFailure = FString::Printf(
					TEXT("Cached property %s has no materializable ordered type/layout"),
					*Property.CanonicalName);
				return false;
			}
			const bool bPrivate = Property.Access
				== EAngelscriptCachedMemberAccess::Private;
			const bool bProtected = Property.Access
				== EAngelscriptCachedMemberAccess::Protected;
			asCObjectProperty* LiveProperty = LiveType.AddPropertyToClass(
				TCHAR_TO_UTF8(*Property.CanonicalName), DataType,
				bPrivate, bProtected);
			if (LiveProperty == nullptr)
			{
				OutFailure = FString::Printf(
					TEXT("The target VM could not allocate cached property %s"),
					*Property.CanonicalName);
				return false;
			}
			LiveProperty->byteOffset = static_cast<int>(
				Property.SemanticByteOffset);
		}
		return true;
	}

	static bool CreateClassFunctionSkeletons(
		asCScriptEngine& ScriptEngine,
		asCModule& Module,
		const FAngelscriptCachedTypeSchema& TypeSchema,
		const FAngelscriptCacheEngineEnvironmentResolver& Environment,
		asCObjectType& LiveType,
		const TConstArrayView<FClassRestoreType> ClassTypes,
		TArray<FFunctionRestoreInput>& Inputs,
		FString& OutFailure)
	{
		for (FFunctionRestoreInput& Input : Inputs)
		{
			if (Input.Declaration != nullptr
				&& Input.Declaration->OwnerKind
					== EAngelscriptFunctionOwnerKind::Type
				&& Input.Declaration->OwnerKey != TypeSchema.TypeKey.Hash)
			{
				continue;
			}
			if (Input.Declaration == nullptr || Input.Body == nullptr
				|| Input.Declaration->DeclarationKind
					!= EAngelscriptCacheDeclarationKind::Function
				|| Input.Declaration->OwnerKind
					!= EAngelscriptFunctionOwnerKind::Type
				|| Input.Declaration->OwnerKey != TypeSchema.TypeKey.Hash
				|| !Input.Declaration->DeclaredType.IsSet())
			{
				OutFailure = TEXT("A cached class function has no exact type owner or return declaration");
				return false;
			}
			const asEBuildArtifactInvocationKind VmInvocation =
				MapInvocationKind(Input.Body->InvocationKind);
			if (VmInvocation == asBUILD_ARTIFACT_INVOCATION_INVALID
				|| Input.Body->InvocationKind
					== EAngelscriptCachedFunctionInvocationKind::GlobalFunction)
			{
				OutFailure = TEXT("A cached root class contains an unsupported invocation kind");
				return false;
			}

			asCDataType ReturnType;
			if (!TryMaterializeDataType(
				Input.Declaration->DeclaredType.GetValue(), ClassTypes,
				Environment, ReturnType))
			{
				OutFailure = FString::Printf(
					TEXT("Function %s has no materializable return type"),
					*Input.Declaration->CanonicalDeclaration);
				return false;
			}

			const int32 ParameterCount =
				Input.Declaration->OrderedParameters.Num();
			asCArray<asCDataType> Parameters;
			asCArray<asCString> ParameterNames;
			asCArray<asETypeModifiers> InOutFlags;
			asCArray<asCString*> DefaultArgs;
			Parameters.SetLength(ParameterCount);
			ParameterNames.SetLength(ParameterCount);
			InOutFlags.SetLength(ParameterCount);
			DefaultArgs.SetLength(ParameterCount);
			for (int32 ParameterIndex = 0;
				ParameterIndex < ParameterCount; ++ParameterIndex)
			{
				const FAngelscriptCachedParameter& Parameter =
					Input.Declaration->OrderedParameters[ParameterIndex];
				DefaultArgs[ParameterIndex] = nullptr;
				if (Parameter.Ordinal != static_cast<uint32>(ParameterIndex)
					|| !TryMaterializeDataType(Parameter.Type, ClassTypes,
						Environment, Parameters[ParameterIndex]))
				{
					OutFailure = FString::Printf(
						TEXT("Function %s parameter %d is not materializable"),
						*Input.Declaration->CanonicalDeclaration, ParameterIndex);
					return false;
				}
				ParameterNames[ParameterIndex] =
					TCHAR_TO_UTF8(*Parameter.CanonicalName);
				switch (Parameter.Passing)
				{
				case EAngelscriptCachedParameterPassing::Value:
					InOutFlags[ParameterIndex] = asTM_NONE;
					break;
				case EAngelscriptCachedParameterPassing::InReference:
					InOutFlags[ParameterIndex] = asTM_INREF;
					break;
				case EAngelscriptCachedParameterPassing::OutReference:
					InOutFlags[ParameterIndex] = asTM_OUTREF;
					break;
				case EAngelscriptCachedParameterPassing::InOutReference:
					InOutFlags[ParameterIndex] = asTM_INOUTREF;
					break;
				default:
					OutFailure = TEXT("A cached function parameter has an invalid passing mode");
					return false;
				}
				if (Parameter.CanonicalDefaultExpression.IsSet())
				{
					DefaultArgs[ParameterIndex] = asNEW(asCString)(
						TCHAR_TO_UTF8(
							*Parameter.CanonicalDefaultExpression.GetValue()));
				}
			}

			asCObjectType* ObjectType =
				Input.Body->InvocationKind
					== EAngelscriptCachedFunctionInvocationKind::Factory
						? nullptr : &LiveType;
			const int FunctionId = ScriptEngine.GetNextScriptFunctionId();
			const int AddResult = Module.AddScriptFunction(
				0, 0, FunctionId,
				TCHAR_TO_UTF8(*Input.Declaration->CanonicalName),
				ReturnType, Parameters, ParameterNames, InOutFlags, DefaultArgs,
				false, ObjectType, false,
				BuildFunctionTraits(*Input.Declaration),
				ScriptEngine.AddNameSpace(
					TCHAR_TO_UTF8(*Input.Declaration->CanonicalNamespace)));
			if (AddResult < 0)
			{
				OutFailure = FString::Printf(
					TEXT("The target VM rejected function skeleton %s"),
					*Input.Declaration->CanonicalDeclaration);
				return false;
			}
			Input.LiveFunction = ScriptEngine.GetScriptFunction(FunctionId);
			if (Input.LiveFunction == nullptr)
			{
				OutFailure = TEXT("The target VM lost a newly registered function skeleton");
				return false;
			}
			Input.LiveFunction->artifactInvocationKind = VmInvocation;
			// The semantic owner is independent from objectType: factories are
			// global-shaped, while every cached type-owned invocation still needs
			// the owner to reproduce its stable FunctionKey during relocation.
			Input.LiveFunction->artifactOwnerType = &LiveType;
			Input.LiveFunction->CalculateParameterOffsets();
			if (!Input.Declaration->CanonicalDeclaration.Equals(
				UTF8_TO_TCHAR(Input.LiveFunction->GetDeclaration(
					false, false, false)), ESearchCase::CaseSensitive))
			{
				OutFailure = FString::Printf(
					TEXT("Function skeleton declaration mismatch: expected %s, current %s"),
					*Input.Declaration->CanonicalDeclaration,
					UTF8_TO_TCHAR(Input.LiveFunction->GetDeclaration(
						false, false, false)));
				return false;
			}
		}
		return true;
	}

	static asCScriptFunction* ResolveBehaviorFunction(
		const FAngelscriptCacheStableReference& Reference,
		const TConstArrayView<FFunctionRestoreInput> Inputs,
		const FAngelscriptCacheEngineEnvironmentResolver& Environment)
	{
		if (Reference.Kind == EAngelscriptCacheReferenceKind::ScriptFunction)
		{
			const FFunctionRestoreInput* Input = FindFunctionInput(
				Inputs, Reference.StableKey);
			return Input != nullptr
				&& Input->Declaration != nullptr
				&& Input->Declaration->SignatureHash == Reference.ExpectedAbi
					? Input->LiveFunction : nullptr;
		}
		if (Reference.Kind == EAngelscriptCacheReferenceKind::EnvironmentSymbol)
		{
			return Environment.ResolveFunctionReference(Reference);
		}
		return nullptr;
	}

	static bool WireClassMethodsAndBehaviors(
		asCObjectType& LiveType,
		const FAngelscriptCachedTypeSchema& TypeSchema,
		const TConstArrayView<FClassRestoreType> ClassTypes,
		const TArray<FFunctionRestoreInput>& Inputs,
		const FAngelscriptCacheEngineEnvironmentResolver& Environment,
		FString& OutFailure)
	{
		LiveType.methods.SetLength(TypeSchema.OrderedMethods.Num());
		for (int32 Index = 0; Index < TypeSchema.OrderedMethods.Num(); ++Index)
		{
			const FAngelscriptCachedMethodEntry& Method =
				TypeSchema.OrderedMethods[Index];
			const FFunctionRestoreInput* Input = FindFunctionInput(
				Inputs, Method.FunctionKey.Hash);
			const FClassRestoreType* DeclaringType = FindClassRestoreType(
				ClassTypes, Method.DeclaringOwner.Hash);
			const bool bValidSlotKind =
				(Method.EntryKind
					== EAngelscriptCachedMethodSlotKind::LocalMethod
					&& Method.DeclaringOwner == TypeSchema.TypeKey)
				|| (Method.EntryKind
					== EAngelscriptCachedMethodSlotKind::Inherited
					&& Method.DeclaringOwner != TypeSchema.TypeKey);
			if (Method.MethodOrdinal != static_cast<uint32>(Index)
				|| !bValidSlotKind || DeclaringType == nullptr
				|| DeclaringType->LiveType == nullptr
				|| Input == nullptr || Input->Declaration == nullptr
				|| Input->LiveFunction == nullptr
				|| Input->LiveFunction->objectType != DeclaringType->LiveType
				|| Input->Declaration->OwnerKey != Method.DeclaringOwner.Hash
				|| Input->Declaration->SignatureHash
					!= Method.ExpectedDeclarationAbi)
			{
				OutFailure = FString::Printf(
					TEXT("Class %s method slot %d is not reconstructible (kind=%u owner=%s function=%s)"),
					*TypeSchema.CanonicalName, Index,
					static_cast<uint32>(Method.EntryKind),
					*Method.DeclaringOwner.Hash.ToHexString(),
					Input != nullptr && Input->Declaration != nullptr
						? *Input->Declaration->CanonicalDeclaration
						: TEXT("<missing>"));
				return false;
			}
			LiveType.methods[Index] = Input->LiveFunction->id;
			Input->LiveFunction->AddRefInternal();
			LiveType.methodTable.Add(Input->LiveFunction);
		}

		LiveType.virtualFunctionTable.SetLength(
			TypeSchema.VirtualFunctionTable.Num());
		for (int32 Index = 0;
			Index < TypeSchema.VirtualFunctionTable.Num(); ++Index)
		{
			const FAngelscriptCachedVirtualFunctionSlot& Slot =
				TypeSchema.VirtualFunctionTable[Index];
			const FFunctionRestoreInput* Input = FindFunctionInput(
				Inputs, Slot.FunctionKey.Hash);
			const FClassRestoreType* ImplementingType = FindClassRestoreType(
				ClassTypes, Slot.ImplementingOwner.Hash);
			const bool bValidSlotKind =
				(Slot.SlotKind
					== EAngelscriptCachedMethodSlotKind::Inherited
					&& Slot.DeclaringOwner == Slot.ImplementingOwner
					&& Slot.ImplementingOwner != TypeSchema.TypeKey)
				|| (Slot.SlotKind
					== EAngelscriptCachedMethodSlotKind::VirtualDeclaration
					&& Slot.DeclaringOwner == TypeSchema.TypeKey
					&& Slot.ImplementingOwner == TypeSchema.TypeKey)
				|| (Slot.SlotKind
					== EAngelscriptCachedMethodSlotKind::VirtualOverride
					&& Slot.DeclaringOwner != TypeSchema.TypeKey
					&& Slot.ImplementingOwner == TypeSchema.TypeKey);
			if (Slot.VftOrdinal != static_cast<uint32>(Index)
				|| !bValidSlotKind || ImplementingType == nullptr
				|| ImplementingType->LiveType == nullptr
				|| Input == nullptr || Input->Declaration == nullptr
				|| Input->LiveFunction == nullptr
				|| Input->LiveFunction->objectType != ImplementingType->LiveType
				|| Input->Declaration->OwnerKey != Slot.ImplementingOwner.Hash
				|| Input->Declaration->SignatureHash
					!= Slot.ExpectedDeclarationAbi)
			{
				OutFailure = FString::Printf(
					TEXT("Class %s VFT slot %d is not reconstructible (kind=%u declaring=%s implementing=%s function=%s)"),
					*TypeSchema.CanonicalName, Index,
					static_cast<uint32>(Slot.SlotKind),
					*Slot.DeclaringOwner.Hash.ToHexString(),
					*Slot.ImplementingOwner.Hash.ToHexString(),
					Input != nullptr && Input->Declaration != nullptr
						? *Input->Declaration->CanonicalDeclaration
						: TEXT("<missing>"));
				return false;
			}
			LiveType.virtualFunctionTable[Index] = Input->LiveFunction;
			Input->LiveFunction->vfTableIdx = Index;
			Input->LiveFunction->AddRefInternal();
		}

		uint32 ConstructorCount = 0;
		uint32 FactoryCount = 0;
		for (const FAngelscriptCachedBehaviorSlot& Slot
			: TypeSchema.OrderedBehaviorSlots)
		{
			if (Slot.BehaviorKind == EAngelscriptCachedBehaviorKind::Construct)
			{
				ConstructorCount = FMath::Max(ConstructorCount, Slot.SlotOrdinal + 1);
			}
			else if (Slot.BehaviorKind
				== EAngelscriptCachedBehaviorKind::Factory)
			{
				FactoryCount = FMath::Max(FactoryCount, Slot.SlotOrdinal + 1);
			}
		}
		LiveType.beh.constructors.SetLength(ConstructorCount);
		LiveType.beh.factories.SetLength(FactoryCount);
		for (uint32 Index = 0; Index < ConstructorCount; ++Index)
		{
			LiveType.beh.constructors[Index] = 0;
		}
		for (uint32 Index = 0; Index < FactoryCount; ++Index)
		{
			LiveType.beh.factories[Index] = 0;
		}

		auto SetSingleton = [&LiveType](
			const EAngelscriptCachedBehaviorKind Kind,
			const int FunctionId) -> bool
		{
			switch (Kind)
			{
			case EAngelscriptCachedBehaviorKind::ListFactory:
				LiveType.beh.listFactory = FunctionId; return true;
			case EAngelscriptCachedBehaviorKind::Destruct:
				LiveType.beh.destruct = FunctionId; return true;
			case EAngelscriptCachedBehaviorKind::AddRef:
				LiveType.beh.addref = FunctionId; return true;
			case EAngelscriptCachedBehaviorKind::Release:
				LiveType.beh.release = FunctionId; return true;
			case EAngelscriptCachedBehaviorKind::GetWeakRefFlag:
				LiveType.beh.getWeakRefFlag = FunctionId; return true;
			case EAngelscriptCachedBehaviorKind::TemplateCallback:
				LiveType.beh.templateCallback = FunctionId; return true;
			case EAngelscriptCachedBehaviorKind::GetRefCount:
				LiveType.beh.gcGetRefCount = FunctionId; return true;
			case EAngelscriptCachedBehaviorKind::SetGcFlag:
				LiveType.beh.gcSetFlag = FunctionId; return true;
			case EAngelscriptCachedBehaviorKind::GetGcFlag:
				LiveType.beh.gcGetFlag = FunctionId; return true;
			case EAngelscriptCachedBehaviorKind::EnumRefs:
				LiveType.beh.gcEnumReferences = FunctionId; return true;
			case EAngelscriptCachedBehaviorKind::ReleaseRefs:
				LiveType.beh.gcReleaseAllReferences = FunctionId; return true;
			case EAngelscriptCachedBehaviorKind::Copy:
				LiveType.beh.copy = FunctionId; return true;
			case EAngelscriptCachedBehaviorKind::CopyConstruct:
				LiveType.beh.copyconstruct = FunctionId; return true;
			case EAngelscriptCachedBehaviorKind::CopyFactory:
				LiveType.beh.copyfactory = FunctionId; return true;
			default:
				return false;
			}
		};

		for (const FAngelscriptCachedBehaviorSlot& Slot
			: TypeSchema.OrderedBehaviorSlots)
		{
			asCScriptFunction* Function = ResolveBehaviorFunction(
				Slot.Target, Inputs, Environment);
			if (Function == nullptr)
			{
				OutFailure = FString::Printf(
					TEXT("Cached behavior kind %u has no exact current function"),
					static_cast<uint32>(Slot.BehaviorKind));
				return false;
			}
			if (Slot.BehaviorKind == EAngelscriptCachedBehaviorKind::Construct)
			{
				if (Slot.SlotOrdinal >= ConstructorCount
					|| LiveType.beh.constructors[Slot.SlotOrdinal] != 0
					|| Function->objectType != &LiveType)
				{
					OutFailure = TEXT("A cached constructor behavior has an invalid slot or owner");
					return false;
				}
				LiveType.beh.constructors[Slot.SlotOrdinal] = Function->id;
				Function->AddRefInternal();
				Function->isInUse = true;
				continue;
			}
			if (Slot.BehaviorKind == EAngelscriptCachedBehaviorKind::Factory)
			{
				if (Slot.SlotOrdinal >= FactoryCount
					|| LiveType.beh.factories[Slot.SlotOrdinal] != 0
					|| Function->objectType != nullptr)
				{
					OutFailure = TEXT("A cached factory behavior has an invalid slot or owner");
					return false;
				}
				LiveType.beh.factories[Slot.SlotOrdinal] = Function->id;
				Function->AddRefInternal();
				Function->isInUse = true;
				continue;
			}
			if (Slot.SlotOrdinal != 0
				|| !SetSingleton(Slot.BehaviorKind, Function->id))
			{
				OutFailure = TEXT("A cached singleton behavior has an invalid kind or ordinal");
				return false;
			}
			if (Slot.BehaviorKind
				!= EAngelscriptCachedBehaviorKind::CopyConstruct
				&& Slot.BehaviorKind
					!= EAngelscriptCachedBehaviorKind::CopyFactory)
			{
				Function->AddRefInternal();
			}
			Function->isInUse = true;
		}

		if (ConstructorCount != 0)
		{
			for (uint32 Index = 0; Index < ConstructorCount; ++Index)
			{
				if (LiveType.beh.constructors[Index] == 0)
				{
					OutFailure = TEXT("The cached constructor behavior table has a gap");
					return false;
				}
			}
			LiveType.beh.construct = LiveType.beh.constructors[0];
		}
		if (FactoryCount != 0)
		{
			for (uint32 Index = 0; Index < FactoryCount; ++Index)
			{
				if (LiveType.beh.factories[Index] == 0)
				{
					OutFailure = TEXT("The cached factory behavior table has a gap");
					return false;
				}
			}
			LiveType.beh.factory = LiveType.beh.factories[0];
		}
		return true;
	}

	static void RestoreMetadata(
		const TConstArrayView<FAngelscriptCachedMetadataEntry> Cached,
		TMap<FName, FString>& OutMetadata)
	{
		OutMetadata.Reset();
		for (const FAngelscriptCachedMetadataEntry& Entry : Cached)
		{
			OutMetadata.Add(FName(*Entry.CanonicalKey), Entry.CanonicalValue);
		}
	}

	static bool AllocateStaticClassGlobal(
		asCScriptEngine& ScriptEngine,
		asCModule& Module,
		const FAngelscriptCachedTypeSchema& TypeSchema,
		FString& OutFailure)
	{
		if (!TypeSchema.Reflection.StaticClassGlobalName.IsSet()
			|| TypeSchema.Reflection.StaticClassGlobalName->IsEmpty())
		{
			OutFailure = TEXT("The cached reflected class has no generated StaticClass global name");
			return false;
		}
		asCTypeInfo* StaticClassType = static_cast<asCTypeInfo*>(
			ScriptEngine.GetTypeInfoByDecl("TSubclassOf<UObject>"));
		if (StaticClassType == nullptr)
		{
			OutFailure = TEXT("The current Engine does not expose TSubclassOf<UObject>");
			return false;
		}
		asCDataType DataType = asCDataType::CreateType(StaticClassType, true);
		asCGlobalProperty* Property = Module.AllocateGlobalProperty(
			TCHAR_TO_UTF8(
				*TypeSchema.Reflection.StaticClassGlobalName.GetValue()),
			DataType,
			ScriptEngine.AddNameSpace(
				TCHAR_TO_UTF8(*TypeSchema.CanonicalNamespace)));
		if (Property == nullptr)
		{
			OutFailure = TEXT("The target VM could not allocate the generated StaticClass global");
			return false;
		}
		Property->isDefaultInit = true;
		return true;
	}

	static bool BuildClassDescriptor(
		const FAngelscriptCachedModuleInterface& ModuleInterface,
		const FAngelscriptCachedTypeSchema& TypeSchema,
		const FAngelscriptCachedTypeSchema* BaseSchema,
		asCObjectType& LiveType,
		UClass& CodeRoot,
		const TArray<FFunctionRestoreInput>& Inputs,
		TSharedRef<FAngelscriptClassDesc>& OutClass,
		FString& OutFailure)
	{
		TSharedRef<FAngelscriptClassDesc> Class =
			MakeShared<FAngelscriptClassDesc>();
		Class->ClassName = TypeSchema.CanonicalName;
		Class->SuperClass = BaseSchema != nullptr
			? BaseSchema->CanonicalName
			: FAngelscriptType::GetBoundClassName(&CodeRoot);
		Class->CodeSuperClass = &CodeRoot;
		Class->ScriptType = &LiveType;
		if (!TypeSchema.CanonicalNamespace.IsEmpty())
		{
			Class->Namespace = TypeSchema.CanonicalNamespace;
		}
		Class->bSuperIsCodeClass = HasFlag(
			TypeSchema.Reflection.ClassReflectionFlags,
			EAngelscriptCachedClassReflectionFlags::SuperIsCodeClass);
		Class->bIsStaticsClass = HasFlag(
			TypeSchema.Reflection.ClassReflectionFlags,
			EAngelscriptCachedClassReflectionFlags::StaticsClass);
		Class->bAbstract = HasFlag(
			TypeSchema.Reflection.ClassReflectionFlags,
			EAngelscriptCachedClassReflectionFlags::Abstract);
		Class->bTransient = HasFlag(
			TypeSchema.Reflection.ClassReflectionFlags,
			EAngelscriptCachedClassReflectionFlags::Transient);
		Class->bHideDropdown = HasFlag(
			TypeSchema.Reflection.ClassReflectionFlags,
			EAngelscriptCachedClassReflectionFlags::HideDropdown);
		Class->bDefaultToInstanced = HasFlag(
			TypeSchema.Reflection.ClassReflectionFlags,
			EAngelscriptCachedClassReflectionFlags::DefaultToInstanced);
		Class->bEditInlineNew = HasFlag(
			TypeSchema.Reflection.ClassReflectionFlags,
			EAngelscriptCachedClassReflectionFlags::EditInlineNew);
		Class->bIsDeprecatedClass = HasFlag(
			TypeSchema.Reflection.ClassReflectionFlags,
			EAngelscriptCachedClassReflectionFlags::Deprecated);
		Class->bPlaceable = HasFlag(
			TypeSchema.Reflection.ClassReflectionFlags,
			EAngelscriptCachedClassReflectionFlags::Placeable);
		Class->bIsStruct = HasFlag(
			TypeSchema.Reflection.ClassReflectionFlags,
			EAngelscriptCachedClassReflectionFlags::IsStruct);
		Class->ConfigName = TypeSchema.Reflection.ConfigName.Get(FString());
		Class->StaticClassGlobalVariableName =
			TypeSchema.Reflection.StaticClassGlobalName.Get(FString());
		RestoreMetadata(TypeSchema.Metadata, Class->Meta);

		for (int32 Index = 0; Index < TypeSchema.OrderedProperties.Num(); ++Index)
		{
			const FAngelscriptCachedPropertySchema& CachedProperty =
				TypeSchema.OrderedProperties[Index];
			if (!HasFlag(CachedProperty.PropertySemanticFlags,
				EAngelscriptCachedPropertySemanticFlags::HasUnrealProperty)
				|| Index < 0
				|| Index >= static_cast<int32>(
					LiveType.localProperties.GetLength()))
			{
				continue;
			}
			const FAngelscriptCachedDeclaration* Declaration = FindDeclaration(
				ModuleInterface, CachedProperty.PropertyKey.Hash);
			asCObjectProperty* LiveProperty = LiveType.localProperties[Index];
			if (Declaration == nullptr || LiveProperty == nullptr
				|| Declaration->DeclarationKind
					!= EAngelscriptCacheDeclarationKind::Property)
			{
				OutFailure = FString::Printf(
					TEXT("Reflected cached property %s has no declaration"),
					*CachedProperty.CanonicalName);
				return false;
			}
			TSharedRef<FAngelscriptPropertyDesc> Property =
				MakeShared<FAngelscriptPropertyDesc>();
			Property->PropertyName = CachedProperty.CanonicalName;
			Property->LiteralType = Declaration->CanonicalTypeSpelling.Get(
				UTF8_TO_TCHAR(LiveProperty->type.Format(
					nullptr, true, false).AddressOf()));
			Property->PropertyType =
				FAngelscriptTypeUsage::FromDataType(LiveProperty->type);
			Property->ScriptPropertyIndex = Index;
			Property->ScriptPropertyOffset = CachedProperty.SemanticByteOffset;
			Property->bHasUnrealProperty = true;
			Property->bBlueprintReadable = HasFlag(
				CachedProperty.PropertySemanticFlags,
				EAngelscriptCachedPropertySemanticFlags::BlueprintReadable);
			Property->bBlueprintWritable = HasFlag(
				CachedProperty.PropertySemanticFlags,
				EAngelscriptCachedPropertySemanticFlags::BlueprintWritable);
			Property->bEditableOnDefaults = HasFlag(
				CachedProperty.PropertySemanticFlags,
				EAngelscriptCachedPropertySemanticFlags::EditableOnDefaults);
			Property->bEditableOnInstance = HasFlag(
				CachedProperty.PropertySemanticFlags,
				EAngelscriptCachedPropertySemanticFlags::EditableOnInstance);
			Property->bEditConst = HasFlag(CachedProperty.PropertySemanticFlags,
				EAngelscriptCachedPropertySemanticFlags::EditConst);
			Property->bInstancedReference = HasFlag(
				CachedProperty.PropertySemanticFlags,
				EAngelscriptCachedPropertySemanticFlags::InstancedReference);
			Property->bPersistentInstance = HasFlag(
				CachedProperty.PropertySemanticFlags,
				EAngelscriptCachedPropertySemanticFlags::PersistentInstance);
			Property->bAdvancedDisplay = HasFlag(
				CachedProperty.PropertySemanticFlags,
				EAngelscriptCachedPropertySemanticFlags::AdvancedDisplay);
			Property->bTransient = HasFlag(CachedProperty.PropertySemanticFlags,
				EAngelscriptCachedPropertySemanticFlags::Transient);
			Property->bReplicated = HasFlag(CachedProperty.PropertySemanticFlags,
				EAngelscriptCachedPropertySemanticFlags::Replicated);
			Property->bSkipReplication = HasFlag(
				CachedProperty.PropertySemanticFlags,
				EAngelscriptCachedPropertySemanticFlags::SkipReplication);
			Property->bSkipSerialization = HasFlag(
				CachedProperty.PropertySemanticFlags,
				EAngelscriptCachedPropertySemanticFlags::SkipSerialization);
			Property->bSaveGame = HasFlag(CachedProperty.PropertySemanticFlags,
				EAngelscriptCachedPropertySemanticFlags::SaveGame);
			Property->bRepNotify = HasFlag(CachedProperty.PropertySemanticFlags,
				EAngelscriptCachedPropertySemanticFlags::RepNotify);
			Property->bConfig = HasFlag(CachedProperty.PropertySemanticFlags,
				EAngelscriptCachedPropertySemanticFlags::Config);
			Property->bInterp = HasFlag(CachedProperty.PropertySemanticFlags,
				EAngelscriptCachedPropertySemanticFlags::Interp);
			Property->bAssetRegistrySearchable = HasFlag(
				CachedProperty.PropertySemanticFlags,
				EAngelscriptCachedPropertySemanticFlags::AssetRegistrySearchable);
			Property->bNoClear = HasFlag(CachedProperty.PropertySemanticFlags,
				EAngelscriptCachedPropertySemanticFlags::NoClear);
			Property->bIsPrivate = CachedProperty.Access
				== EAngelscriptCachedMemberAccess::Private;
			Property->bIsProtected = CachedProperty.Access
				== EAngelscriptCachedMemberAccess::Protected;
			Property->ReplicationCondition = static_cast<ELifetimeCondition>(
				CachedProperty.ReplicationCondition);
			RestoreMetadata(CachedProperty.Metadata, Property->Meta);
			Class->Properties.Add(Property);
		}

		for (int32 Index = 0;
			Index < TypeSchema.Reflection.OrderedUFunctionMembers.Num(); ++Index)
		{
			const FAngelscriptCachedReflectedFunctionMember& Member =
				TypeSchema.Reflection.OrderedUFunctionMembers[Index];
			const FFunctionRestoreInput* Input = FindFunctionInput(
				Inputs, Member.Target.StableKey);
			if (Member.ReflectionOrdinal != static_cast<uint32>(Index)
				|| Input == nullptr || Input->Declaration == nullptr
				|| Input->LiveFunction == nullptr
				|| Input->Declaration->SignatureHash != Member.Target.ExpectedAbi)
			{
				OutFailure = TEXT("A reflected cached method has no exact live declaration");
				return false;
			}
			const FAngelscriptCachedDeclaration& Declaration = *Input->Declaration;
			if (Declaration.CanonicalName != Member.CanonicalScriptFunctionName)
			{
				OutFailure = TEXT("A reflected cached method script name does not match its live declaration");
				return false;
			}
			TSharedRef<FAngelscriptFunctionDesc> Function =
				MakeShared<FAngelscriptFunctionDesc>();
			Function->FunctionName = Member.CanonicalFunctionName;
			Function->OriginalFunctionName =
				Member.CanonicalOriginalFunctionName;
			Function->ScriptFunctionName = Member.CanonicalScriptFunctionName;
			Function->ScriptFunction = Input->LiveFunction;
			Function->ReturnType = FAngelscriptTypeUsage::FromDataType(
				Input->LiveFunction->returnType);
			Function->bBlueprintCallable = HasFlag(Declaration.ReflectionFlags,
				EAngelscriptCachedReflectionFlags::BlueprintCallable);
			Function->bBlueprintOverride = HasFlag(Declaration.ReflectionFlags,
				EAngelscriptCachedReflectionFlags::BlueprintOverride);
			Function->bBlueprintEvent = HasFlag(Declaration.ReflectionFlags,
				EAngelscriptCachedReflectionFlags::BlueprintEvent);
			Function->bBlueprintPure = HasFlag(Declaration.ReflectionFlags,
				EAngelscriptCachedReflectionFlags::BlueprintPure);
			Function->bNetMulticast = HasFlag(Declaration.ReflectionFlags,
				EAngelscriptCachedReflectionFlags::NetMulticast);
			Function->bNetClient = HasFlag(Declaration.ReflectionFlags,
				EAngelscriptCachedReflectionFlags::NetClient);
			Function->bNetServer = HasFlag(Declaration.ReflectionFlags,
				EAngelscriptCachedReflectionFlags::NetServer);
			Function->bNetValidate = HasFlag(Declaration.ReflectionFlags,
				EAngelscriptCachedReflectionFlags::NetValidate);
			Function->bUnreliable = HasFlag(Declaration.ReflectionFlags,
				EAngelscriptCachedReflectionFlags::Unreliable);
			Function->bBlueprintAuthorityOnly = HasFlag(
				Declaration.ReflectionFlags,
				EAngelscriptCachedReflectionFlags::BlueprintAuthorityOnly);
			Function->bExec = HasFlag(Declaration.ReflectionFlags,
				EAngelscriptCachedReflectionFlags::Exec);
			Function->bCanOverrideEvent = HasFlag(Declaration.ReflectionFlags,
				EAngelscriptCachedReflectionFlags::CanOverrideEvent);
			Function->bIsStatic = HasFlag(Declaration.TraitFlags,
				EAngelscriptCachedDeclarationTraitFlags::Static);
			Function->bIsConstMethod = HasFlag(Declaration.TraitFlags,
				EAngelscriptCachedDeclarationTraitFlags::Const);
			Function->bThreadSafe = HasFlag(Declaration.TraitFlags,
				EAngelscriptCachedDeclarationTraitFlags::ThreadSafe);
			Function->bIsPrivate = HasFlag(Declaration.TraitFlags,
				EAngelscriptCachedDeclarationTraitFlags::Private);
			Function->bIsProtected = HasFlag(Declaration.TraitFlags,
				EAngelscriptCachedDeclarationTraitFlags::Protected);
			RestoreMetadata(Declaration.Metadata, Function->Meta);
			// Arguments are ClassGenerator analysis output, not preprocessor
			// descriptor input. The restored VM skeleton above already owns the
			// canonical parameter types, names, passing modes and defaults. Leave
			// this array empty so Analyze() materializes the reflected argument
			// descriptors exactly once, matching clean compile and dependency
			// recompile paths.
			Class->Methods.Add(Function);
		}

		OutClass = Class;
		return true;
	}

	static void DiscardStagingModule(
		asCScriptEngine& ScriptEngine,
		asCModule*& StagingModule)
	{
		if (StagingModule == nullptr)
		{
			return;
		}
		const asCString StagingName = StagingModule->name;
		StagingModule->InternalReset();
		ScriptEngine.DiscardModule(StagingName.AddressOf());
		ScriptEngine.DeleteDiscardedModules();
		StagingModule = nullptr;
	}

	struct FPreparedModule final
	{
		FAngelscriptStableModuleKey ModuleKey;
		TSharedPtr<FAngelscriptModuleDesc> ModuleDesc;
		asCModule* StagingModule = nullptr;
		TArray<FAngelscriptCacheLiveFunctionRoute> CandidateRoutes;
		uint32 RestoredTypeCount = 0;
		uint32 RestoredFunctionCount = 0;
	};

	static void DiscardPreparedModule(
		asCScriptEngine& ScriptEngine,
		FPreparedModule& Prepared)
	{
		DiscardStagingModule(ScriptEngine, Prepared.StagingModule);
		if (Prepared.ModuleDesc.IsValid())
		{
			Prepared.ModuleDesc->ScriptModule = nullptr;
		}
		Prepared.CandidateRoutes.Reset();
	}

	static void DiscardPreparedModules(
		asCScriptEngine& ScriptEngine,
		TArray<FPreparedModule>& PreparedModules)
	{
		for (FPreparedModule& Prepared : PreparedModules)
		{
			DiscardPreparedModule(ScriptEngine, Prepared);
		}
		PreparedModules.Reset();
	}
}

class FAngelscriptCacheModuleRestorer
{
public:
	static FAngelscriptCacheRestoreResult Prepare(
		FAngelscriptEngine& TargetEngine,
		const FAngelscriptValidatedGeneration& Generation,
		const FAngelscriptStableModuleKey& ModuleKey,
		const TConstArrayView<FAngelscriptCacheCurrentSourceProjection>
			CurrentSources,
		const FAngelscriptCacheReadLimits& Limits,
		IAngelscriptCacheRestoreFaultInjector* FaultInjector,
		const uint32 ModuleOrdinal,
		AngelscriptCacheRestore_Private::FPreparedModule& OutPrepared)
	{
		using namespace AngelscriptCacheRestore_Private;
		OutPrepared = {};

		if (TargetEngine.Engine == nullptr || ModuleKey.Hash.IsZero()
			|| Generation.Manifest.Profile.Hash.IsZero()
			|| Generation.Manifest.SourceSnapshot.IsZero())
		{
			return Failure(EAngelscriptCacheRestoreError::InvalidInput,
				EAngelscriptCacheRestoreStage::SelectModule,
				TEXT("The target Engine, module key, profile and source snapshot must be valid"));
		}

		const FAngelscriptCacheModuleSnapshotLink* SelectedLink = nullptr;
		for (const FAngelscriptCacheModuleSnapshotLink& Link
			: Generation.Manifest.ModuleSnapshots)
		{
			if (Link.ModuleKey == ModuleKey)
			{
				if (SelectedLink != nullptr)
				{
					return Failure(EAngelscriptCacheRestoreError::InvalidInput,
						EAngelscriptCacheRestoreStage::SelectModule,
						TEXT("The validated generation contains a duplicate module root"));
				}
				SelectedLink = &Link;
			}
		}
		if (SelectedLink == nullptr)
		{
			return Failure(EAngelscriptCacheRestoreError::MissingModuleSnapshot,
				EAngelscriptCacheRestoreStage::SelectModule,
				TEXT("The selected module key is absent from this generation"));
		}

		const FAngelscriptDecodedCacheRecord* SourceIndexRecord = FindRecord(
			Generation, Generation.Manifest.SourceIndexRecordId);
		const FAngelscriptCachedSourceIndex* SourceIndex = SourceIndexRecord != nullptr
			? SourceIndexRecord->TryGetSourceIndex() : nullptr;
		if (SourceIndex == nullptr)
		{
			return Failure(EAngelscriptCacheRestoreError::InvalidInput,
				EAngelscriptCacheRestoreStage::ValidateGraph,
				TEXT("The validated generation has no usable SourceIndex authority"));
		}

		TArray<const FAngelscriptCacheCurrentSourceProjection*>
			CurrentModuleSources;
		FString CurrentSourceFailure;
		if (!ValidateCurrentSourceProjection(
			*SourceIndex,
			ModuleKey,
			CurrentSources,
			CurrentModuleSources,
			CurrentSourceFailure))
		{
			return Failure(
				EAngelscriptCacheRestoreError::CurrentSourceProjectionMismatch,
				EAngelscriptCacheRestoreStage::ReconstructDescriptor,
				MoveTemp(CurrentSourceFailure));
		}

		asCScriptEngine& ScriptEngine = *TargetEngine.Engine;
		asCModule ValidationModule("__CacheV2Validation", &ScriptEngine);
		FAngelscriptCacheEngineEnvironmentResolver CurrentSymbols(ScriptEngine);
		FAngelscriptCacheEngineLayoutResolver CurrentLayouts(ScriptEngine);
		FAngelscriptFunctionArtifactCodec ValidationOpaque(
			ValidationModule, ScriptEngine);
		FAngelscriptCacheModuleGraphValidationContext GraphContext;
		GraphContext.SelectedProfile = Generation.Manifest.Profile;
		GraphContext.SelectedSourceSnapshot =
			Generation.Manifest.SourceSnapshot;
		GraphContext.SourceIndex = SourceIndexRecord;
		GraphContext.CurrentSymbols = &CurrentSymbols;
		GraphContext.CurrentLayouts = &CurrentLayouts;
		GraphContext.OpaquePayloads = &ValidationOpaque;

		FAngelscriptCacheReadBudget RestoreBudget;
		FAngelscriptValidatedModuleGraph Graph;
		const FAngelscriptCacheValidationResult GraphResult =
			ValidateModuleSnapshotGraph(
				SelectedLink->RecordId,
				Generation.ReachableRecords,
				GraphContext,
				Limits,
				RestoreBudget,
				Graph);
		if (!GraphResult.IsSuccess())
		{
			FString Detail = FString::Printf(
				TEXT("Consumer graph validation failed: Error=%u Kind=%u Stage=%u Offset=%llu"),
				static_cast<uint32>(GraphResult.Error),
				static_cast<uint32>(GraphResult.RecordKind),
				static_cast<uint32>(GraphResult.Stage),
				GraphResult.ByteOffset);
			if (!ValidationOpaque.GetLastExecutionFailureDetail().IsEmpty())
			{
				Detail += TEXT("; ");
				Detail += ValidationOpaque.GetLastExecutionFailureDetail();
			}
			return Failure(EAngelscriptCacheRestoreError::GraphValidationFailed,
				EAngelscriptCacheRestoreStage::ValidateGraph,
				MoveTemp(Detail), GraphResult);
		}

		const FAngelscriptCachedModuleInterface* ModuleInterface = nullptr;
		const FAngelscriptCachedModuleState* ModuleState = nullptr;
		for (const FAngelscriptDecodedCacheRecordHandle& Record
			: Graph.GetReachableRecords())
		{
			if (const FAngelscriptCachedModuleInterface* InterfaceCandidate =
				Record->TryGetModuleInterface())
			{
				ModuleInterface = InterfaceCandidate;
			}
			else if (const FAngelscriptCachedModuleState* StateCandidate =
				Record->TryGetModuleState())
			{
				ModuleState = StateCandidate;
			}
		}
		if (ModuleInterface == nullptr || ModuleState == nullptr
			|| ModuleInterface->ModuleKey != ModuleKey
			|| ModuleState->ModuleKey != ModuleKey)
		{
			return Failure(EAngelscriptCacheRestoreError::GraphValidationFailed,
				EAngelscriptCacheRestoreStage::ValidateGraph,
				TEXT("The validated graph omitted its module interface or state"));
		}

		// Mutable module state remains outside this vertical. Validated class
		// graphs are restored atomically; an enum remains a single optional type.
		if (Graph.GetFunctionOrdinals().IsEmpty()
			|| !Graph.GetGlobalOrdinals().IsEmpty()
			|| !Graph.GetInitializerOrdinals().IsEmpty()
			|| !ModuleInterface->Imports.IsEmpty()
			|| !ModuleInterface->Dependencies.IsEmpty()
			|| !ModuleState->OrderedGlobals.IsEmpty()
			|| !ModuleState->HardValues.IsEmpty()
			|| !ModuleState->Initializers.IsEmpty()
			|| !ModuleState->OrderedInitializationActions.IsEmpty()
			|| !ModuleState->OrderedPostInitFunctions.IsEmpty()
			|| !ModuleState->Dependencies.IsEmpty())
		{
			return Failure(EAngelscriptCacheRestoreError::UnsupportedRecordShape,
				EAngelscriptCacheRestoreStage::ReconstructDescriptor,
				TEXT("The live restore vertical admits an immutable enum/class graph and one or more complete functions"));
		}

		const FAngelscriptCachedTypeSchema* EnumTypeSchema = nullptr;
		TArray<const FAngelscriptCachedTypeSchema*> PendingClassSchemas;
		PendingClassSchemas.Reserve(Graph.GetTypeOrdinals().Num());
		for (const FAngelscriptCacheValidatedTypeOrdinal& TypeOrdinal
			: Graph.GetTypeOrdinals())
		{
			const FAngelscriptDecodedCacheRecord* TypeRecord = FindGraphRecord(
				Graph, TypeOrdinal.TypeSchemaRecordOrdinal);
			const FAngelscriptCachedTypeSchema* TypeSchema = TypeRecord != nullptr
				? TypeRecord->TryGetTypeSchema() : nullptr;
			if (TypeSchema == nullptr)
			{
				return Failure(
					EAngelscriptCacheRestoreError::UnsupportedRecordShape,
					EAngelscriptCacheRestoreStage::ReconstructDescriptor,
					TEXT("A validated type ordinal has no TypeSchema record"));
			}
			if (TypeSchema->TypeKind == EAngelscriptCachedTypeKind::Enum
				&& TypeSchema->KindPayload.Enum.IsSet())
			{
				if (EnumTypeSchema != nullptr
					|| Graph.GetTypeOrdinals().Num() != 1)
				{
					return Failure(
						EAngelscriptCacheRestoreError::UnsupportedRecordShape,
						EAngelscriptCacheRestoreStage::ReconstructDescriptor,
						TEXT("Enum restore admits exactly one enum and no class graph"));
				}
				EnumTypeSchema = TypeSchema;
			}
			else if (TypeSchema->TypeKind == EAngelscriptCachedTypeKind::Class)
			{
				PendingClassSchemas.Add(TypeSchema);
			}
			else
			{
				return Failure(
					EAngelscriptCacheRestoreError::UnsupportedRecordShape,
					EAngelscriptCacheRestoreStage::ReconstructDescriptor,
					FString::Printf(
						TEXT("Type %s is not a reconstructible enum or reflected class"),
						*TypeSchema->CanonicalName));
			}
		}

		TArray<FClassRestoreType> ClassTypes;
		ClassTypes.Reserve(PendingClassSchemas.Num());
		while (!PendingClassSchemas.IsEmpty())
		{
			bool bMadeProgress = false;
			for (int32 PendingIndex = PendingClassSchemas.Num() - 1;
				PendingIndex >= 0; --PendingIndex)
			{
				const FAngelscriptCachedTypeSchema* Schema =
					PendingClassSchemas[PendingIndex];
				const FAngelscriptCacheStableReference* BaseReference = nullptr;
				for (const FAngelscriptCachedTypeRelation& Relation : Schema->Relations)
				{
					if (Relation.RelationKind
						== EAngelscriptCachedTypeRelationKind::Base)
					{
						if (BaseReference != nullptr)
						{
							return Failure(
								EAngelscriptCacheRestoreError::UnsupportedRecordShape,
								EAngelscriptCacheRestoreStage::ReconstructDescriptor,
								FString::Printf(
									TEXT("Class %s has duplicate script-base relations"),
									*Schema->CanonicalName));
						}
						BaseReference = &Relation.Target;
					}
				}
				const FClassRestoreType* BaseType = BaseReference != nullptr
					? FindClassRestoreType(
						TConstArrayView<FClassRestoreType>(ClassTypes),
						BaseReference->StableKey)
					: nullptr;
				if (BaseReference != nullptr && BaseType == nullptr)
				{
					continue;
				}
				ClassTypes.Add({Schema,
					BaseType != nullptr ? BaseType->Schema : nullptr,
					nullptr, nullptr});
				PendingClassSchemas.RemoveAt(PendingIndex);
				bMadeProgress = true;
			}
			if (!bMadeProgress)
			{
				const FAngelscriptCachedTypeSchema* Blocked = PendingClassSchemas[0];
				return Failure(
					EAngelscriptCacheRestoreError::UnsupportedRecordShape,
					EAngelscriptCacheRestoreStage::ReconstructDescriptor,
					FString::Printf(
						TEXT("Class graph cannot resolve a local base for %s (%s)"),
						*Blocked->CanonicalName,
						*Blocked->TypeKey.Hash.ToHexString()));
			}
		}

		TArray<FFunctionRestoreInput> FunctionInputs;
		FunctionInputs.Reserve(Graph.GetFunctionOrdinals().Num());
		for (const FAngelscriptCacheValidatedFunctionOrdinal& FunctionOrdinal
			: Graph.GetFunctionOrdinals())
		{
			if (!ModuleInterface->Declarations.IsValidIndex(
					static_cast<int32>(FunctionOrdinal.DeclarationOrdinal))
				|| !FunctionOrdinal.BodyRecordOrdinal.IsSet()
				|| !FunctionOrdinal.DebugRecordOrdinal.IsSet())
			{
				return Failure(
					EAngelscriptCacheRestoreError::UnsupportedRecordShape,
					EAngelscriptCacheRestoreStage::ReconstructDescriptor,
					TEXT("A validated global function is missing reconstructible records"));
			}

			const FAngelscriptCachedDeclaration& FunctionDeclaration =
				ModuleInterface->Declarations[
					FunctionOrdinal.DeclarationOrdinal];
			const FAngelscriptDecodedCacheRecord* BodyRecord = FindGraphRecord(
				Graph, FunctionOrdinal.BodyRecordOrdinal.GetValue());
			const FAngelscriptDecodedCacheRecord* DebugRecord = FindGraphRecord(
				Graph, FunctionOrdinal.DebugRecordOrdinal.GetValue());
			const FAngelscriptCachedFunctionBody* FunctionBody =
				BodyRecord != nullptr ? BodyRecord->TryGetFunctionBody() : nullptr;
			const FAngelscriptCachedDebugSidecar* DebugSidecar =
				DebugRecord != nullptr
					? DebugRecord->TryGetDebugSidecar() : nullptr;
			const FClassRestoreType* FunctionOwner =
				FunctionDeclaration.OwnerKind
					== EAngelscriptFunctionOwnerKind::Type
				? FindClassRestoreType(
					TConstArrayView<FClassRestoreType>(ClassTypes),
					FunctionDeclaration.OwnerKey)
				: nullptr;
			const bool bClassFunction = !ClassTypes.IsEmpty();
			const bool bSupportedDeclaration = bClassFunction
				? FunctionDeclaration.DeclarationKind
						== EAngelscriptCacheDeclarationKind::Function
					&& FunctionDeclaration.OwnerKind
						== EAngelscriptFunctionOwnerKind::Type
					&& FunctionOwner != nullptr
					&& FunctionBody != nullptr
					&& FunctionBody->InvocationKind
						!= EAngelscriptCachedFunctionInvocationKind::GlobalFunction
					&& FunctionBody->InvocationKind
						!= EAngelscriptCachedFunctionInvocationKind::Invalid
				: IsSupportedPrimitiveGlobalDeclaration(FunctionDeclaration)
					&& FunctionBody != nullptr
					&& FunctionBody->InvocationKind
						== EAngelscriptCachedFunctionInvocationKind::GlobalFunction;
			if (!bSupportedDeclaration || DebugSidecar == nullptr
				|| FunctionBody->VmExecutionCodecVersion
					!= FAngelscriptFunctionArtifactCodec::ExecutionCodecVersion
				|| DebugSidecar->VmDebugCodecVersion
					!= FAngelscriptFunctionArtifactCodec::DebugCodecVersion)
			{
				return Failure(
					EAngelscriptCacheRestoreError::UnsupportedRecordShape,
					EAngelscriptCacheRestoreStage::RestoreFunctions,
					FString::Printf(
						TEXT("A FunctionBody is not supported by the selected type materializer: Declaration=%s Owner=%u Invocation=%u Dependencies=%d ExecutionCodec=%u DebugCodec=%u"),
						*FunctionDeclaration.CanonicalDeclaration,
						static_cast<uint32>(FunctionDeclaration.OwnerKind),
						FunctionBody != nullptr
							? static_cast<uint32>(FunctionBody->InvocationKind)
							: 0u,
						FunctionBody != nullptr
							? FunctionBody->ActualDependencies.Num() : -1,
						FunctionBody != nullptr
							? FunctionBody->VmExecutionCodecVersion : 0u,
						DebugSidecar != nullptr
							? DebugSidecar->VmDebugCodecVersion : 0u));
			}
			FAngelscriptCacheLiveFunctionRoute ExistingRoute;
			if (TargetEngine.ResolveCacheFunctionRoute(
				FunctionBody->Identity.FunctionKey, ExistingRoute))
			{
				return Failure(
					EAngelscriptCacheRestoreError::ActivationFailed,
					EAngelscriptCacheRestoreStage::ActivateModule,
					TEXT("A stable function route is already active in the target Engine"));
			}
			for (const FFunctionRestoreInput& Existing : FunctionInputs)
			{
				if (Existing.Body->Identity.FunctionKey
					== FunctionBody->Identity.FunctionKey)
				{
					return Failure(
						EAngelscriptCacheRestoreError::ActivationFailed,
						EAngelscriptCacheRestoreStage::ReconstructDescriptor,
						TEXT("The module contains duplicate stable function routes"));
				}
			}
			FunctionInputs.Add({
				&FunctionOrdinal, &FunctionDeclaration,
				FunctionBody, DebugSidecar, nullptr});
		}

		if (TargetEngine.GetModuleByModuleName(
			ModuleInterface->CanonicalModuleName).IsValid())
		{
			return Failure(EAngelscriptCacheRestoreError::ActivationFailed,
				EAngelscriptCacheRestoreStage::ActivateModule,
				TEXT("Fresh-engine restore refuses to replace an already active module"));
		}

		TSharedRef<FAngelscriptModuleDesc> ModuleDesc =
			MakeShared<FAngelscriptModuleDesc>();
		ModuleDesc->ModuleName = ModuleInterface->CanonicalModuleName;
		ModuleDesc->bCompileError = false;
		ModuleDesc->bLoadedPrecompiledCode = false;
		ModuleDesc->bLoadedIncrementalCache = true;
		for (const FAngelscriptCachedSourceFile& File : SourceIndex->Files)
		{
			if (File.ModuleKey != ModuleKey)
			{
				continue;
			}
			FAngelscriptModuleDesc::FCodeSection& Section =
				ModuleDesc->Code.AddDefaulted_GetRef();
			Section.VirtualPath = BuildLogicalVirtualPath(*SourceIndex, File);
			Section.RelativeFilename = File.RelativeLogicalPath;
			for (const FAngelscriptCacheCurrentSourceProjection* Current
				: CurrentModuleSources)
			{
				if (Current != nullptr
					&& Current->SourceFileKey.Hash == File.SourceFileKey.Hash)
				{
					Section.VirtualPath = Current->VirtualPath;
					Section.RelativeFilename = Current->RelativeFilename;
					Section.AbsoluteFilename = Current->AbsoluteFilename;
					break;
				}
			}
			Section.CodeHash = 0;
		}

		const FString StagingName = FString::Printf(
			TEXT("%s_CACHEV2_NEW_%d"),
			*TargetEngine.MakeModuleName(ModuleDesc->ModuleName),
			TargetEngine.TempNameIndex++);
		asCModule* StagingModule = static_cast<asCModule*>(
			ScriptEngine.GetModule(TCHAR_TO_UTF8(*StagingName), asGM_ALWAYS_CREATE));
		if (StagingModule == nullptr)
		{
			return Failure(EAngelscriptCacheRestoreError::ActivationFailed,
				EAngelscriptCacheRestoreStage::MaterializeTypes,
				TEXT("The target VM could not create a staging module"));
		}
		StagingModule->baseModuleName = TCHAR_TO_UTF8(*ModuleDesc->ModuleName);

		if (EnumTypeSchema != nullptr)
		{
			asCEnumType* LiveEnum = asNEW(asCEnumType)(&ScriptEngine);
			if (LiveEnum == nullptr)
			{
				DiscardStagingModule(ScriptEngine, StagingModule);
				return Failure(EAngelscriptCacheRestoreError::ActivationFailed,
					EAngelscriptCacheRestoreStage::MaterializeTypes,
					TEXT("The target VM could not allocate the cached enum type"));
			}
			LiveEnum->name = TCHAR_TO_UTF8(*EnumTypeSchema->CanonicalName);
			LiveEnum->nameSpace = ScriptEngine.AddNameSpace(
				TCHAR_TO_UTF8(*EnumTypeSchema->CanonicalNamespace));
			LiveEnum->flags = asOBJ_ENUM;
			LiveEnum->size = static_cast<int>(
				EnumTypeSchema->Layout.SemanticSize);
			LiveEnum->alignment = static_cast<int>(
				EnumTypeSchema->Layout.SemanticAlignment);
			LiveEnum->module = StagingModule;
			const FAngelscriptCachedEnumTypePayload& EnumPayload =
				EnumTypeSchema->KindPayload.Enum.GetValue();
			LiveEnum->enumValues.SetLength(
				EnumPayload.OrderedEnumerators.Num());
			for (int32 Index = 0;
				Index < EnumPayload.OrderedEnumerators.Num(); ++Index)
			{
				const FAngelscriptCachedEnumEnumerator& Enumerator =
					EnumPayload.OrderedEnumerators[Index];
				LiveEnum->enumValues[Index] = asNEW(asSEnumValue)();
				LiveEnum->enumValues[Index]->name =
					TCHAR_TO_UTF8(*Enumerator.CanonicalName);
				LiveEnum->enumValues[Index]->value = Enumerator.Value;
			}
			StagingModule->AddEnumType(LiveEnum);
			ScriptEngine.allScriptDeclaredTypes.Add(LiveEnum);

			TSharedRef<FAngelscriptEnumDesc> EnumDesc =
				MakeShared<FAngelscriptEnumDesc>();
			EnumDesc->EnumName = EnumTypeSchema->CanonicalName;
			EnumDesc->ScriptType = LiveEnum;
			for (const FAngelscriptCachedEnumEnumerator& Enumerator
				: EnumPayload.OrderedEnumerators)
			{
				EnumDesc->ValueNames.Add(FName(Enumerator.CanonicalName));
				EnumDesc->EnumValues.Add(Enumerator.Value);
			}
			ModuleDesc->Enums.Add(EnumDesc);
		}
		else if (!ClassTypes.IsEmpty())
		{
			FString MaterializationFailure;
			for (FClassRestoreType& Type : ClassTypes)
			{
				const FClassRestoreType* BaseType = Type.BaseSchema != nullptr
					? FindClassRestoreType(
						TConstArrayView<FClassRestoreType>(ClassTypes),
						Type.BaseSchema->TypeKey.Hash)
					: nullptr;
				asCObjectType* LiveType = nullptr;
				UClass* CodeRoot = nullptr;
				if (Type.Schema == nullptr
					|| !MaterializeClassType(
						ScriptEngine, *StagingModule, *Type.Schema,
						CurrentSymbols,
						BaseType != nullptr ? BaseType->LiveType : nullptr,
						TArrayView<FClassRestoreType>(ClassTypes),
						LiveType, CodeRoot, MaterializationFailure)
					|| LiveType == nullptr || CodeRoot == nullptr)
				{
					DiscardStagingModule(ScriptEngine, StagingModule);
					return Failure(EAngelscriptCacheRestoreError::ActivationFailed,
						EAngelscriptCacheRestoreStage::MaterializeTypes,
						MaterializationFailure.IsEmpty()
							? FString(TEXT("A cached class could not be materialized"))
							: MoveTemp(MaterializationFailure));
				}
			}
			for (FClassRestoreType& Type : ClassTypes)
			{
				if (Type.Schema == nullptr || Type.LiveType == nullptr
					|| !MaterializeClassProperties(
						*Type.Schema, *Type.LiveType,
						TConstArrayView<FClassRestoreType>(ClassTypes),
						CurrentSymbols, MaterializationFailure))
				{
					DiscardStagingModule(ScriptEngine, StagingModule);
					return Failure(EAngelscriptCacheRestoreError::ActivationFailed,
						EAngelscriptCacheRestoreStage::MaterializeTypes,
						MaterializationFailure.IsEmpty()
							? FString(TEXT("Cached class properties could not be materialized"))
							: MoveTemp(MaterializationFailure));
				}
			}
			for (FClassRestoreType& Type : ClassTypes)
			{
				if (Type.Schema == nullptr || Type.LiveType == nullptr
					|| !CreateClassFunctionSkeletons(
						ScriptEngine, *StagingModule, *Type.Schema,
						CurrentSymbols, *Type.LiveType,
						TConstArrayView<FClassRestoreType>(ClassTypes),
						FunctionInputs, MaterializationFailure))
				{
					DiscardStagingModule(ScriptEngine, StagingModule);
					return Failure(EAngelscriptCacheRestoreError::ActivationFailed,
						EAngelscriptCacheRestoreStage::MaterializeTypes,
						MoveTemp(MaterializationFailure));
				}
			}
			for (FClassRestoreType& Type : ClassTypes)
			{
				TSharedRef<FAngelscriptClassDesc> ClassDesc =
					MakeShared<FAngelscriptClassDesc>();
				if (Type.Schema == nullptr || Type.LiveType == nullptr
					|| Type.CodeRoot == nullptr
					|| !WireClassMethodsAndBehaviors(
						*Type.LiveType, *Type.Schema,
						TConstArrayView<FClassRestoreType>(ClassTypes),
						FunctionInputs, CurrentSymbols,
						MaterializationFailure)
					|| !AllocateStaticClassGlobal(
						ScriptEngine, *StagingModule, *Type.Schema,
						MaterializationFailure)
					|| !BuildClassDescriptor(
						*ModuleInterface, *Type.Schema, Type.BaseSchema,
						*Type.LiveType, *Type.CodeRoot, FunctionInputs,
						ClassDesc, MaterializationFailure))
				{
					DiscardStagingModule(ScriptEngine, StagingModule);
					return Failure(EAngelscriptCacheRestoreError::ActivationFailed,
						EAngelscriptCacheRestoreStage::ReconstructDescriptor,
						MaterializationFailure.IsEmpty()
							? FString::Printf(
								TEXT("Class graph descriptor reconstruction failed for %s"),
								Type.Schema != nullptr
									? *Type.Schema->CanonicalName : TEXT("<missing>"))
							: MoveTemp(MaterializationFailure));
				}
				ModuleDesc->Classes.Add(ClassDesc);
			}
		}
		ModuleDesc->ScriptModule = StagingModule;

		FAngelscriptFunctionArtifactCodec RestoreCodec(
			*StagingModule, ScriptEngine);
		TArray<FAngelscriptCacheLiveFunctionRoute> CandidateRoutes;
		CandidateRoutes.Reserve(FunctionInputs.Num());
		for (FFunctionRestoreInput& Input : FunctionInputs)
		{
			asCScriptFunction* LiveFunction = nullptr;
			const FAngelscriptCacheValidationResult FunctionRestore =
				Input.LiveFunction != nullptr
				? RestoreCodec.RestoreFunctionIntoExisting(
					Input.Body->CanonicalExecutionPayload,
					Input.Debug->CanonicalDebugPayload,
					Input.Body->ModuleKey,
					Input.Body->ActualDependencies,
					Input.Body->Identity.Content.Execution,
					Limits,
					RestoreBudget,
					*Input.LiveFunction)
				: RestoreCodec.RestoreGlobalFunction(
					Input.Body->CanonicalExecutionPayload,
					Input.Debug->CanonicalDebugPayload,
					Input.Body->ModuleKey,
					Input.Body->ActualDependencies,
					Input.Body->Identity.Content.Execution,
					Limits,
					RestoreBudget,
					LiveFunction);
			if (Input.LiveFunction != nullptr)
			{
				LiveFunction = Input.LiveFunction;
			}
			if (!FunctionRestore.IsSuccess() || LiveFunction == nullptr)
			{
				FString Detail = FString::Printf(
					TEXT("The private VM adapter rejected function %s: Error=%u Offset=%llu"),
					*Input.Declaration->CanonicalDeclaration,
					static_cast<uint32>(FunctionRestore.Error),
					FunctionRestore.ByteOffset);
				if (!RestoreCodec.GetLastExecutionFailureDetail().IsEmpty())
				{
					Detail += TEXT("; ");
					Detail += RestoreCodec.GetLastExecutionFailureDetail();
				}
				DiscardStagingModule(ScriptEngine, StagingModule);
				ModuleDesc->ScriptModule = nullptr;
				return Failure(EAngelscriptCacheRestoreError::VmRestoreFailed,
					EAngelscriptCacheRestoreStage::RestoreFunctions,
					MoveTemp(Detail), FunctionRestore);
			}
			const bool bDeclarationMatches = Input.LiveFunction != nullptr
				? Input.Declaration->CanonicalName.Equals(
						UTF8_TO_TCHAR(LiveFunction->GetName()),
						ESearchCase::CaseSensitive)
					&& Input.Declaration->CanonicalNamespace.Equals(
						UTF8_TO_TCHAR(LiveFunction->GetNamespace()),
						ESearchCase::CaseSensitive)
					&& Input.Declaration->CanonicalDeclaration.Equals(
						UTF8_TO_TCHAR(LiveFunction->GetDeclaration(
							false, false, false)),
						ESearchCase::CaseSensitive)
				: DoesLiveFunctionMatchDeclaration(
					*LiveFunction, *Input.Declaration);
			if (!bDeclarationMatches)
			{
				DiscardStagingModule(ScriptEngine, StagingModule);
				ModuleDesc->ScriptModule = nullptr;
				return Failure(EAngelscriptCacheRestoreError::VmRestoreFailed,
					EAngelscriptCacheRestoreStage::RestoreFunctions,
					TEXT("An opaque function signature does not match its stable declaration"));
			}
			if (Input.LiveFunction == nullptr)
			{
				ScriptEngine.allScriptGlobalFunctions.Add(LiveFunction);
			}

			FAngelscriptCacheLiveFunctionRoute& CandidateRoute =
				CandidateRoutes.AddDefaulted_GetRef();
			CandidateRoute.ModuleKey = ModuleKey;
			CandidateRoute.Identity = Input.Body->Identity;
			CandidateRoute.CanonicalDeclaration =
				Input.Declaration->CanonicalDeclaration;
			CandidateRoute.Function = LiveFunction;
			CandidateRoute.NumericFunctionId = LiveFunction->GetId();
			CandidateRoute.SelectedExecutionRoute =
				LiveFunction->jitFunction != nullptr
					? EAngelscriptCacheFunctionExecutionRoute::Native
					: EAngelscriptCacheFunctionExecutionRoute::Vm;
			CandidateRoute.bHasVerifiedArtifactIdentity = true;
			if (CandidateRoute.NumericFunctionId < 0
				|| Input.Ordinal->FunctionKey
					!= CandidateRoute.Identity.FunctionKey)
			{
				DiscardStagingModule(ScriptEngine, StagingModule);
				ModuleDesc->ScriptModule = nullptr;
				return Failure(EAngelscriptCacheRestoreError::VmRestoreFailed,
					EAngelscriptCacheRestoreStage::RestoreFunctions,
					TEXT("A restored live function has no valid current-engine route"));
			}
		}

		ScriptEngine.PrepareEngine();
		StagingModule->JITCompile();
		if (ScriptEngine.ep.initGlobalVarsAfterBuild
			&& StagingModule->ResetGlobalVars(nullptr) < 0)
		{
			DiscardStagingModule(ScriptEngine, StagingModule);
			ModuleDesc->ScriptModule = nullptr;
			return Failure(EAngelscriptCacheRestoreError::ActivationFailed,
				EAngelscriptCacheRestoreStage::FinalizeModule,
				TEXT("The restored module could not initialize its global state"));
		}

		if (FaultInjector != nullptr
			&& FaultInjector->ShouldStopAt(
				EAngelscriptCacheRestoreFaultPoint::AfterModulePrepared,
				ModuleOrdinal,
				ModuleKey))
		{
			DiscardStagingModule(ScriptEngine, StagingModule);
			ModuleDesc->ScriptModule = nullptr;
			return Failure(EAngelscriptCacheRestoreError::ActivationFailed,
				EAngelscriptCacheRestoreStage::FinalizeModule,
				TEXT("Injected restore failure after module preparation"));
		}

		for (const FAngelscriptCacheLiveFunctionRoute& CandidateRoute
			: CandidateRoutes)
		{
			FAngelscriptCacheLiveFunctionRoute ConflictingRoute;
			if (TargetEngine.ResolveCacheFunctionRoute(
				CandidateRoute.Identity.FunctionKey, ConflictingRoute))
			{
				DiscardStagingModule(ScriptEngine, StagingModule);
				ModuleDesc->ScriptModule = nullptr;
				return Failure(EAngelscriptCacheRestoreError::ActivationFailed,
					EAngelscriptCacheRestoreStage::ActivateModule,
					TEXT("A stable function route already exists in the target Engine"));
			}
		}

		OutPrepared.ModuleKey = ModuleKey;
		OutPrepared.ModuleDesc = ModuleDesc;
		OutPrepared.StagingModule = StagingModule;
		OutPrepared.CandidateRoutes = MoveTemp(CandidateRoutes);
		OutPrepared.RestoredTypeCount = static_cast<uint32>(ClassTypes.Num())
			+ (EnumTypeSchema != nullptr ? 1u : 0u);
		OutPrepared.RestoredFunctionCount =
			static_cast<uint32>(FunctionInputs.Num());
		FAngelscriptCacheRestoreResult Result;
		Result.Stage = EAngelscriptCacheRestoreStage::FinalizeModule;
		Result.RestoredTypeCount = OutPrepared.RestoredTypeCount;
		Result.RestoredFunctionCount = OutPrepared.RestoredFunctionCount;
		Result.Detail = FString::Printf(
			TEXT("Prepared module %s with %u restored types and %u current-engine function routes"),
			*ModuleDesc->ModuleName,
			Result.RestoredTypeCount,
			Result.RestoredFunctionCount);
		return Result;
	}

	static FAngelscriptCacheRestoreResult RestoreBatch(
		FAngelscriptEngine& TargetEngine,
		const FAngelscriptValidatedGeneration& Generation,
		const TConstArrayView<FAngelscriptStableModuleKey> ModuleKeys,
		const TConstArrayView<FAngelscriptCacheCurrentSourceProjection>
			CurrentSources,
		const FAngelscriptCacheReadLimits& Limits,
		IAngelscriptCacheRestoreFaultInjector* FaultInjector)
	{
		using namespace AngelscriptCacheRestore_Private;
		if (TargetEngine.Engine == nullptr || ModuleKeys.IsEmpty())
		{
			return Failure(EAngelscriptCacheRestoreError::InvalidInput,
				EAngelscriptCacheRestoreStage::SelectModule,
				TEXT("Batch restore requires a target Engine and at least one module key"));
		}

		for (int32 Left = 0; Left < ModuleKeys.Num(); ++Left)
		{
			if (ModuleKeys[Left].Hash.IsZero())
			{
				return Failure(EAngelscriptCacheRestoreError::InvalidInput,
					EAngelscriptCacheRestoreStage::SelectModule,
					TEXT("Batch restore contains an invalid module key"));
			}
			for (int32 Right = Left + 1; Right < ModuleKeys.Num(); ++Right)
			{
				if (ModuleKeys[Left] == ModuleKeys[Right])
				{
					return Failure(EAngelscriptCacheRestoreError::InvalidInput,
						EAngelscriptCacheRestoreStage::SelectModule,
						TEXT("Batch restore contains a duplicate module key"));
				}
			}
		}

		asCScriptEngine& ScriptEngine = *TargetEngine.Engine;
		TArray<FPreparedModule> PreparedModules;
		PreparedModules.Reserve(ModuleKeys.Num());
		for (int32 ModuleIndex = 0; ModuleIndex < ModuleKeys.Num(); ++ModuleIndex)
		{
			FPreparedModule Prepared;
			const FAngelscriptCacheRestoreResult PrepareResult = Prepare(
				TargetEngine,
				Generation,
				ModuleKeys[ModuleIndex],
				CurrentSources,
				Limits,
				FaultInjector,
				static_cast<uint32>(ModuleIndex),
				Prepared);
			if (!PrepareResult.IsSuccess())
			{
				DiscardPreparedModules(ScriptEngine, PreparedModules);
				return PrepareResult;
			}
			for (const FPreparedModule& Existing : PreparedModules)
			{
				for (const FAngelscriptCacheLiveFunctionRoute& ExistingRoute
					: Existing.CandidateRoutes)
				{
					for (const FAngelscriptCacheLiveFunctionRoute& PreparedRoute
						: Prepared.CandidateRoutes)
					{
						if (ExistingRoute.Identity.FunctionKey
							!= PreparedRoute.Identity.FunctionKey)
						{
							continue;
						}
						DiscardPreparedModule(ScriptEngine, Prepared);
						DiscardPreparedModules(
							ScriptEngine, PreparedModules);
						return Failure(
							EAngelscriptCacheRestoreError::ActivationFailed,
							EAngelscriptCacheRestoreStage::FinalizeModule,
							TEXT("Prepared modules contain a duplicate stable function route"));
					}
				}
			}
			PreparedModules.Add(MoveTemp(Prepared));
		}

		TArray<TSharedRef<FAngelscriptModuleDesc>> RestoredModules;
		TArray<FAngelscriptCacheLiveFunctionRoute> CandidateRoutes;
		RestoredModules.Reserve(PreparedModules.Num());
		int32 CandidateRouteCount = 0;
		for (const FPreparedModule& Prepared : PreparedModules)
		{
			CandidateRouteCount += Prepared.CandidateRoutes.Num();
		}
		CandidateRoutes.Reserve(CandidateRouteCount);
		for (FPreparedModule& Prepared : PreparedModules)
		{
			if (!Prepared.ModuleDesc.IsValid()
				|| Prepared.StagingModule == nullptr)
			{
				DiscardPreparedModules(ScriptEngine, PreparedModules);
				return Failure(EAngelscriptCacheRestoreError::ActivationFailed,
					EAngelscriptCacheRestoreStage::FinalizeModule,
					TEXT("A prepared restore module lost its staging state"));
			}
			RestoredModules.Add(Prepared.ModuleDesc.ToSharedRef());
			CandidateRoutes.Append(Prepared.CandidateRoutes);
		}

		FAngelscriptClassGenerator ClassGenerator;
		TargetEngine.GetPreGenerateClasses().Broadcast(RestoredModules);
		for (const TSharedRef<FAngelscriptModuleDesc>& Module : RestoredModules)
		{
			ClassGenerator.AddModule(Module);
		}
		const FAngelscriptClassGenerator::EReloadRequirement Reload =
			ClassGenerator.Setup();
		if (Reload == FAngelscriptClassGenerator::EReloadRequirement::Error)
		{
			DiscardPreparedModules(ScriptEngine, PreparedModules);
			return Failure(EAngelscriptCacheRestoreError::ActivationFailed,
				EAngelscriptCacheRestoreStage::ActivateModule,
				TEXT("ClassGenerator rejected the reconstructed module batch"));
		}

		// Recheck the complete target immediately before the sole commit. Exact
		// startup owns the per-Engine mutation gate, but the public one-module
		// wrapper also remains safe when called directly by focused consumers.
		for (const FPreparedModule& Prepared : PreparedModules)
		{
			if (TargetEngine.GetModuleByModuleName(
				Prepared.ModuleDesc->ModuleName).IsValid())
			{
				DiscardPreparedModules(ScriptEngine, PreparedModules);
				return Failure(EAngelscriptCacheRestoreError::ActivationFailed,
					EAngelscriptCacheRestoreStage::ActivateModule,
					TEXT("The target Engine changed before atomic restore commit"));
			}
			for (const FAngelscriptCacheLiveFunctionRoute& CandidateRoute
				: Prepared.CandidateRoutes)
			{
				FAngelscriptCacheLiveFunctionRoute ConflictingRoute;
				if (TargetEngine.ResolveCacheFunctionRoute(
					CandidateRoute.Identity.FunctionKey, ConflictingRoute))
				{
					DiscardPreparedModules(
						ScriptEngine, PreparedModules);
					return Failure(
						EAngelscriptCacheRestoreError::ActivationFailed,
						EAngelscriptCacheRestoreStage::ActivateModule,
						TEXT("The target Engine changed before atomic restore commit"));
				}
			}
		}

		TArray<TSharedRef<FAngelscriptModuleDesc>> DiscardedModules;
		TargetEngine.SwapInModules(RestoredModules, DiscardedModules);
		if (!DiscardedModules.IsEmpty())
		{
			return Failure(EAngelscriptCacheRestoreError::ActivationFailed,
				EAngelscriptCacheRestoreStage::ActivateModule,
				TEXT("Fresh batch restore unexpectedly displaced an active module"));
		}

		switch (Reload)
		{
		case FAngelscriptClassGenerator::EReloadRequirement::SoftReload:
			ClassGenerator.PerformSoftReload();
			break;
		case FAngelscriptClassGenerator::EReloadRequirement::FullReloadSuggested:
		case FAngelscriptClassGenerator::EReloadRequirement::FullReloadRequired:
			ClassGenerator.PerformFullReload();
			break;
		case FAngelscriptClassGenerator::EReloadRequirement::Error:
			checkNoEntry();
			break;
		}

		FAngelscriptCacheRestoreResult Result;
		Result.Stage = EAngelscriptCacheRestoreStage::ActivateModule;
		Result.RestoredModuleCount = static_cast<uint32>(PreparedModules.Num());
		for (FPreparedModule& Prepared : PreparedModules)
		{
			TargetEngine.ModulesByScriptModule.Add(
				Prepared.StagingModule, Prepared.ModuleDesc);
			Result.RestoredTypeCount += Prepared.RestoredTypeCount;
			Result.RestoredFunctionCount += Prepared.RestoredFunctionCount;
		}
		TargetEngine.RebuildFunctionRouteSnapshot(CandidateRoutes);
		if (!TargetEngine.ShouldUseAutomaticImportMethod())
		{
			TargetEngine.ResolveAllDeclaredImports();
		}
		Result.Detail = FString::Printf(
			TEXT("Atomically restored %u modules, %u types and %u current-engine function routes"),
			Result.RestoredModuleCount,
			Result.RestoredTypeCount,
			Result.RestoredFunctionCount);
		return Result;
	}
};

FAngelscriptCacheRestoreResult RestoreAngelscriptCacheModules(
	FAngelscriptEngine& TargetEngine,
	const FAngelscriptValidatedGeneration& Generation,
	const TConstArrayView<FAngelscriptStableModuleKey> ModuleKeys,
	const FAngelscriptCacheReadLimits& Limits,
	IAngelscriptCacheRestoreFaultInjector* FaultInjector)
{
	return FAngelscriptCacheModuleRestorer::RestoreBatch(
		TargetEngine, Generation, ModuleKeys, {}, Limits, FaultInjector);
}

FAngelscriptCacheRestoreResult RestoreAngelscriptCacheModules(
	FAngelscriptEngine& TargetEngine,
	const FAngelscriptValidatedGeneration& Generation,
	const TConstArrayView<FAngelscriptStableModuleKey> ModuleKeys,
	const TConstArrayView<FAngelscriptCacheCurrentSourceProjection> CurrentSources,
	const FAngelscriptCacheReadLimits& Limits,
	IAngelscriptCacheRestoreFaultInjector* FaultInjector)
{
	return FAngelscriptCacheModuleRestorer::RestoreBatch(
		TargetEngine, Generation, ModuleKeys, CurrentSources, Limits,
		FaultInjector);
}

FAngelscriptCacheRestoreResult RestoreAngelscriptCacheModule(
	FAngelscriptEngine& TargetEngine,
	const FAngelscriptValidatedGeneration& Generation,
	const FAngelscriptStableModuleKey& ModuleKey,
	const FAngelscriptCacheReadLimits& Limits,
	IAngelscriptCacheRestoreFaultInjector* FaultInjector)
{
	return RestoreAngelscriptCacheModules(
		TargetEngine,
		Generation,
		TConstArrayView<FAngelscriptStableModuleKey>(&ModuleKey, 1),
		Limits,
		FaultInjector);
}

FAngelscriptCacheRestoreResult RestoreAngelscriptCacheModule(
	FAngelscriptEngine& TargetEngine,
	const FAngelscriptValidatedGeneration& Generation,
	const FAngelscriptStableModuleKey& ModuleKey,
	const TConstArrayView<FAngelscriptCacheCurrentSourceProjection> CurrentSources,
	const FAngelscriptCacheReadLimits& Limits,
	IAngelscriptCacheRestoreFaultInjector* FaultInjector)
{
	return RestoreAngelscriptCacheModules(
		TargetEngine,
		Generation,
		TConstArrayView<FAngelscriptStableModuleKey>(&ModuleKey, 1),
		CurrentSources,
		Limits,
		FaultInjector);
}

bool FAngelscriptEngine::ResolveCacheFunctionRoute(
	const FAngelscriptStableFunctionKey& FunctionKey,
	FAngelscriptCacheLiveFunctionRoute& OutRoute) const
{
	OutRoute = {};
	if (!CacheRuntimeState.IsValid()
		|| !CacheRuntimeState->FunctionRouteSnapshot.IsValid())
	{
		return false;
	}
	const TArray<FAngelscriptCacheLiveFunctionRoute>& Routes =
		CacheRuntimeState->FunctionRouteSnapshot->FunctionRoutes;
	int32 Lower = 0;
	int32 Upper = Routes.Num();
	while (Lower < Upper)
	{
		const int32 Middle = Lower + (Upper - Lower) / 2;
		if (Routes[Middle].Identity.FunctionKey.Hash < FunctionKey.Hash)
		{
			Lower = Middle + 1;
		}
		else
		{
			Upper = Middle;
		}
	}
	if (!Routes.IsValidIndex(Lower))
	{
		return false;
	}
	const FAngelscriptCacheLiveFunctionRoute& Route = Routes[Lower];
	if (Route.Identity.FunctionKey != FunctionKey
		|| Route.Function == nullptr
		|| Route.Function->GetEngine() != Engine
		|| Route.NumericFunctionId != Route.Function->GetId()
		|| Engine->GetFunctionById(Route.NumericFunctionId) != Route.Function)
	{
		return false;
	}
	OutRoute = Route;
	return true;
}

TSharedPtr<const FAngelscriptCacheFunctionRouteSnapshot,
	ESPMode::ThreadSafe> FAngelscriptEngine::GetFunctionRouteSnapshot() const
{
	return CacheRuntimeState.IsValid()
		? CacheRuntimeState->FunctionRouteSnapshot
		: TSharedPtr<const FAngelscriptCacheFunctionRouteSnapshot,
			ESPMode::ThreadSafe>();
}

bool FAngelscriptEngine::RefreshFunctionRouteSnapshotAfterStaticJITChange(
	const bool bValidatedProviderStateWasAppliedAtSafePoint)
{
	// Provider enumeration, entry identity/ABI matching and Live Coding state
	// remain owned by the sibling StaticJIT system. A rejected upstream result
	// cannot publish even an identical replacement snapshot.
	if (!bValidatedProviderStateWasAppliedAtSafePoint
		|| !CacheService.IsValid())
	{
		return false;
	}

	FAngelscriptCacheMutationGuard RouteRefresh = CacheService->EnterMutation(
		EAngelscriptCacheMutationKind::RouteRefresh);
	if (!RouteRefresh.IsEntered())
	{
		return false;
	}

	// RebuildFunctionRouteSnapshot reads only current Engine-owned function
	// state and preserves graph-validated identities from the prior immutable
	// publication. It never consults or changes Current/Previous/Pending roots.
	RebuildFunctionRouteSnapshot();
	return true;
}

void FAngelscriptEngine::RebuildFunctionRouteSnapshot(
	const TConstArrayView<FAngelscriptCacheLiveFunctionRoute>
		VerifiedArtifactRoutes,
	const TConstArrayView<FAngelscriptFunctionArtifactIdentity>
		ValidatedArtifactIdentities,
	const TConstArrayView<asIScriptModule*> RebuiltModules,
	const TConstArrayView<asIScriptModule*> ArtifactInvalidatedModules)
{
	if (!CacheRuntimeState.IsValid())
	{
		CacheRuntimeState = MakeUnique<FAngelscriptCacheRuntimeState>();
	}

	TArray<FAngelscriptCacheLiveFunctionRoute> CandidateRoutes;
	const TSharedPtr<const FAngelscriptCacheFunctionRouteSnapshot,
		ESPMode::ThreadSafe> PreviousSnapshot =
		CacheRuntimeState->FunctionRouteSnapshot;
	const auto ContainsModule = [](
		const TConstArrayView<asIScriptModule*> Modules,
		const asIScriptModule* Candidate)
	{
		for (const asIScriptModule* Module : Modules)
		{
			if (Module == Candidate)
			{
				return true;
			}
		}
		return false;
	};
	TArray<TSharedRef<FAngelscriptModuleDesc>> Modules = GetActiveModules();
	Modules.Sort([](
		const TSharedRef<FAngelscriptModuleDesc>& Left,
		const TSharedRef<FAngelscriptModuleDesc>& Right)
	{
		return FAngelscriptArtifactCanonicalWriter::CompareCanonicalUtf8Strings(
			Left->ModuleName, Right->ModuleName) < 0;
	});

	for (const TSharedRef<FAngelscriptModuleDesc>& Module : Modules)
	{
		asCModule* ScriptModule = Module->ScriptModule;
		if (ScriptModule == nullptr)
		{
			continue;
		}

		FAngelscriptStableModuleKey ModuleKey;
		for (const FAngelscriptCacheLiveFunctionRoute& Verified
			: VerifiedArtifactRoutes)
		{
			if (Verified.Function != nullptr
				&& Verified.Function->GetModule() == ScriptModule)
			{
				ModuleKey = Verified.ModuleKey;
				break;
			}
		}
		if (ModuleKey.Hash.IsZero())
		{
			FString ModuleKeyFailure;
			if (!FAngelscriptCacheStableSymbolIdentity::TryBuildModuleKey(
				*Module, ModuleKey, &ModuleKeyFailure))
			{
				UE_LOG(Angelscript, Verbose,
					TEXT("[CacheV2][StableRoute] Module=%s skipped: %s"),
					*Module->ModuleName, *ModuleKeyFailure);
				continue;
			}
		}
		const bool bModuleWasRebuilt = ContainsModule(
			RebuiltModules, ScriptModule);
		const bool bModuleArtifactWasInvalidated = ContainsModule(
			ArtifactInvalidatedModules, ScriptModule);

		for (asUINT FunctionIndex = 0;
			FunctionIndex < ScriptModule->scriptFunctions.GetLength();
			++FunctionIndex)
		{
			asCScriptFunction* Function =
				ScriptModule->scriptFunctions[FunctionIndex];
			if (Function == nullptr || Function->module != ScriptModule)
			{
				continue;
			}

			// A graph-validated restored route is already the identity authority for
			// this exact current function. Reuse it without formatting live AS type
			// metadata, which may not be enumerable during reconstruction.
			const FAngelscriptCacheLiveFunctionRoute* VerifiedByFunction =
				VerifiedArtifactRoutes.FindByPredicate(
					[Function](
						const FAngelscriptCacheLiveFunctionRoute& Entry)
					{
						return Entry.Function == Function
							&& !Entry.Identity.FunctionKey.Hash.IsZero();
					});
			if (VerifiedByFunction != nullptr)
			{
				FAngelscriptCacheLiveFunctionRoute Route = *VerifiedByFunction;
				Route.ModuleKey = ModuleKey;
				Route.Function = Function;
				Route.NumericFunctionId = Function->GetId();
				Route.SelectedExecutionRoute = Function->jitFunction != nullptr
					? EAngelscriptCacheFunctionExecutionRoute::Native
					: EAngelscriptCacheFunctionExecutionRoute::Vm;
				Route.bHasVerifiedArtifactIdentity = true;
				CandidateRoutes.Add(MoveTemp(Route));
				continue;
			}

			// Modules not compiled in this transaction may temporarily retain
			// functions whose signature types were retargeted by a dependency full
			// reload. Their previous stable declaration/key is owned value data and
			// remains safe; calling GetDeclaration() here can traverse an already
			// released old asCTypeInfo. A rebuilt module must derive its current key
			// from the new compiler output instead of taking this reuse path.
			const FAngelscriptCacheLiveFunctionRoute* PreviousByFunction =
				PreviousSnapshot.IsValid()
				? PreviousSnapshot->FunctionRoutes.FindByPredicate(
					[Function](
						const FAngelscriptCacheLiveFunctionRoute& Entry)
					{
						return Entry.Function == Function;
					})
				: nullptr;
			if (!bModuleWasRebuilt && PreviousByFunction != nullptr)
			{
				FAngelscriptCacheLiveFunctionRoute Route = *PreviousByFunction;
				Route.ModuleKey = ModuleKey;
				Route.Function = Function;
				Route.NumericFunctionId = Function->GetId();
				Route.SelectedExecutionRoute = Function->jitFunction != nullptr
					? EAngelscriptCacheFunctionExecutionRoute::Native
					: EAngelscriptCacheFunctionExecutionRoute::Vm;
				if (bModuleArtifactWasInvalidated)
				{
					const FAngelscriptStableFunctionKey StableKey =
						Route.Identity.FunctionKey;
					Route.Identity = {};
					Route.Identity.FunctionKey = StableKey;
					Route.bHasVerifiedArtifactIdentity = false;
					Route.SelectedExecutionRoute =
						EAngelscriptCacheFunctionExecutionRoute::Vm;
				}
				CandidateRoutes.Add(MoveTemp(Route));
				continue;
			}
			if (bModuleArtifactWasInvalidated)
			{
				UE_LOG(Angelscript, Verbose,
					TEXT("[CacheV2][StableRoute] Function in dependency-invalidated module %s skipped until authoritative recompile"),
					*Module->ModuleName);
				continue;
			}
			FAngelscriptStableFunctionKey FunctionKey;
			if (!FAngelscriptCacheStableSymbolIdentity::TryBuildFunctionKey(
				ModuleKey, *Function, FunctionKey))
			{
				continue;
			}

			FAngelscriptCacheLiveFunctionRoute Route;
			Route.ModuleKey = ModuleKey;
			Route.Identity.FunctionKey = FunctionKey;
			Route.CanonicalDeclaration = UTF8_TO_TCHAR(
				Function->GetDeclaration(false, false, false));
			Route.Function = Function;
			Route.NumericFunctionId = Function->GetId();
			Route.SelectedExecutionRoute = Function->jitFunction != nullptr
				? EAngelscriptCacheFunctionExecutionRoute::Native
				: EAngelscriptCacheFunctionExecutionRoute::Vm;

			const FAngelscriptCacheLiveFunctionRoute* Verified =
				VerifiedArtifactRoutes.FindByPredicate(
					[&Route](const FAngelscriptCacheLiveFunctionRoute& Entry)
					{
						return Entry.Function == Route.Function
							&& Entry.Identity.FunctionKey
								== Route.Identity.FunctionKey;
					});
			if (Verified != nullptr
				&& Verified->bHasVerifiedArtifactIdentity)
			{
				Route.Identity = Verified->Identity;
				Route.bHasVerifiedArtifactIdentity = true;
			}
			else if (CacheRuntimeState->FunctionRouteSnapshot.IsValid())
			{
				const FAngelscriptCacheLiveFunctionRoute* Previous =
					CacheRuntimeState->FunctionRouteSnapshot->FunctionRoutes.
						FindByPredicate(
							[&Route](
								const FAngelscriptCacheLiveFunctionRoute& Entry)
							{
								return Entry.Function == Route.Function
									&& Entry.Identity.FunctionKey
										== Route.Identity.FunctionKey;
							});
				if (Previous != nullptr
					&& Previous->bHasVerifiedArtifactIdentity)
				{
					Route.Identity = Previous->Identity;
					Route.bHasVerifiedArtifactIdentity = true;
				}
			}
			CandidateRoutes.Add(MoveTemp(Route));
		}
	}

	// A graph-validated restore may materialize a function whose maintained-fork
	// declaration metadata is not yet enumerable through the normal compiler
	// identity path. The restore DTO is already the authoritative stable
	// identity; merge it only after proving that its transient route belongs to
	// this Engine and to one of the newly active modules.
	for (const FAngelscriptCacheLiveFunctionRoute& Verified
		: VerifiedArtifactRoutes)
	{
		if (Verified.Function == nullptr
			|| Verified.Function->GetEngine() != Engine
			|| Verified.Function->GetModule() == nullptr
			|| Verified.NumericFunctionId != Verified.Function->GetId()
			|| !ModulesByScriptModule.Contains(Verified.Function->GetModule())
			|| Verified.Identity.FunctionKey.Hash.IsZero())
		{
			continue;
		}
		CandidateRoutes.RemoveAll(
			[&Verified](const FAngelscriptCacheLiveFunctionRoute& Candidate)
			{
				return Candidate.Function == Verified.Function;
		});
		FAngelscriptCacheLiveFunctionRoute Route = Verified;
		Route.NumericFunctionId = Route.Function->GetId();
		asCScriptFunction* LiveFunction =
			static_cast<asCScriptFunction*>(Route.Function);
		Route.SelectedExecutionRoute = LiveFunction->jitFunction != nullptr
			? EAngelscriptCacheFunctionExecutionRoute::Native
			: EAngelscriptCacheFunctionExecutionRoute::Vm;
		Route.bHasVerifiedArtifactIdentity = true;
		if (Route.CanonicalDeclaration.IsEmpty())
		{
			Route.CanonicalDeclaration = UTF8_TO_TCHAR(
				Route.Function->GetDeclaration(false, false, false));
		}
		CandidateRoutes.Add(MoveTemp(Route));
	}

	// Normal compile capture derives these tuples from the sole validated
	// ModuleSnapshot graph. Join them to the already-proven current-Engine route
	// by the full StableFunctionKey; never recompute content/profile from live
	// pointers and never publish an identity for a missing current function.
	for (const FAngelscriptFunctionArtifactIdentity& ValidatedIdentity
		: ValidatedArtifactIdentities)
	{
		if (ValidatedIdentity.FunctionKey.Hash.IsZero()
			|| ValidatedIdentity.Content.Execution.IsZero()
			|| ValidatedIdentity.Content.Debug.IsZero()
			|| ValidatedIdentity.Profile.Hash.IsZero())
		{
			continue;
		}
		FAngelscriptCacheLiveFunctionRoute* Route =
			CandidateRoutes.FindByPredicate(
				[&ValidatedIdentity](
					FAngelscriptCacheLiveFunctionRoute& Candidate)
				{
					return Candidate.Identity.FunctionKey
						== ValidatedIdentity.FunctionKey;
				});
		if (Route != nullptr)
		{
			Route->Identity = ValidatedIdentity;
			Route->bHasVerifiedArtifactIdentity = true;
		}
	}

	CandidateRoutes.Sort([](
		const FAngelscriptCacheLiveFunctionRoute& Left,
		const FAngelscriptCacheLiveFunctionRoute& Right)
	{
		return Left.Identity.FunctionKey.Hash
			< Right.Identity.FunctionKey.Hash;
	});

	TSharedRef<FAngelscriptCacheFunctionRouteSnapshot,
		ESPMode::ThreadSafe> Published =
		MakeShared<FAngelscriptCacheFunctionRouteSnapshot,
			ESPMode::ThreadSafe>();
	Published->PublicationOrdinal =
		CacheRuntimeState->NextFunctionRoutePublicationOrdinal++;
	for (int32 Index = 0; Index < CandidateRoutes.Num();)
	{
		int32 DuplicateEnd = Index + 1;
		while (DuplicateEnd < CandidateRoutes.Num()
			&& CandidateRoutes[DuplicateEnd].Identity.FunctionKey
				== CandidateRoutes[Index].Identity.FunctionKey)
		{
			++DuplicateEnd;
		}
		if (DuplicateEnd == Index + 1)
		{
			FAngelscriptCacheLiveFunctionRoute& Route =
				Published->FunctionRoutes.Add_GetRef(
					MoveTemp(CandidateRoutes[Index]));
			if (Route.SelectedExecutionRoute
				== EAngelscriptCacheFunctionExecutionRoute::Native)
			{
				++Published->NativeRouteCount;
			}
			else
			{
				++Published->VmRouteCount;
			}
		}
		else
		{
			UE_LOG(Angelscript, Error,
				TEXT("[CacheV2][StableRoute] Rejected %d duplicate live routes for StableFunctionKey=%s"),
				DuplicateEnd - Index,
				*CandidateRoutes[Index].Identity.FunctionKey.Hash.ToHexString());
		}
		Index = DuplicateEnd;
	}

	CacheRuntimeState->FunctionRouteSnapshot = Published;
	if (CacheService.IsValid())
	{
		for (const FAngelscriptCacheLiveFunctionRoute& Route
			: Published->FunctionRoutes)
		{
			FAngelscriptCacheDecisionEvent Event;
			Event.TransactionOrdinal = Published->PublicationOrdinal;
			Event.Stage = EAngelscriptCacheDecisionStage::StableRoute;
			Event.Outcome = EAngelscriptCacheDecisionOutcome::Published;
			Event.ReasonDomain =
				EAngelscriptCacheDecisionReasonDomain::StableRoute;
			Event.ReasonCode =
				static_cast<uint32>(Route.SelectedExecutionRoute);
			Event.ModuleKeys.Add(Route.ModuleKey);
			Event.FunctionKey = Route.Identity.FunctionKey;
			if (Route.bHasVerifiedArtifactIdentity)
			{
				Event.CurrentCoordinate = Route.Identity.Content.Execution;
				Event.Profile = Route.Identity.Profile;
			}
			Event.PrimaryCount = 1;
			Event.SecondaryCount = Route.bHasVerifiedArtifactIdentity ? 1 : 0;
			CacheService->RecordDecisionEvent(MoveTemp(Event));
		}
	}
	UE_LOG(Angelscript, Verbose,
		TEXT("[CacheV2][StableRoute] Published Engine=%p Revision=%llu Routes=%d VM=%u Native=%u"),
		this,
		Published->PublicationOrdinal,
		Published->FunctionRoutes.Num(),
		Published->VmRouteCount,
		Published->NativeRouteCount);
}
