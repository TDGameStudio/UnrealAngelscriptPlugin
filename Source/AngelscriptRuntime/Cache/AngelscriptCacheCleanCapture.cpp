#include "Cache/AngelscriptCacheCleanCapture.h"

#include "Cache/AngelscriptCacheArchive.h"
#include "Cache/AngelscriptCacheCompilerBridge.h"
#include "Cache/AngelscriptCacheCurrentModuleAuthority.h"
#include "Cache/AngelscriptCacheEnvironment.h"
#include "Cache/AngelscriptCacheRemainingRecordTypes.h"
#include "Cache/AngelscriptCacheSemanticRecords.h"
#include "Cache/AngelscriptCacheStableSymbolIdentity.h"
#include "Cache/AngelscriptCacheTypeSchema.h"
#include "Cache/AngelscriptFunctionArtifactCodec.h"
#include "Core/AngelscriptEngine.h"
#include "Core/AngelscriptSource.h"
#include "Misc/FileHelper.h"

#include "as_module.h"
#include "as_datatype.h"
#include "as_objecttype.h"
#include "as_property.h"
#include "as_restore.h"
#include "as_scriptengine.h"
#include "as_scriptfunction.h"

namespace AngelscriptCacheCleanCapture_Private
{
	class FFunctionArtifactStream final : public asIBinaryStream
	{
	public:
		virtual int Read(void*, asUINT) override
		{
			return asERROR;
		}

		virtual int Write(const void* Data, const asUINT Size) override
		{
			if ((Data == nullptr && Size != 0)
				|| Size > static_cast<asUINT>(MAX_int32 - Bytes.Num()))
			{
				return asOUT_OF_MEMORY;
			}
			if (Size != 0)
			{
				const int32 Offset = Bytes.AddUninitialized(static_cast<int32>(Size));
				FMemory::Memcpy(Bytes.GetData() + Offset, Data, Size);
			}
			return asSUCCESS;
		}

		TArray<uint8> Bytes;
	};

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

	static FAngelscriptCacheValidationResult CaptureActualDependencies(
		const asCScriptFunction& Function,
		const IAngelscriptCacheBuildDependencyResolver& CompilerResolver,
		const FAngelscriptStableModuleKey& ModuleKey,
		const FAngelscriptStableFunctionKey& FunctionKey,
		const IAngelscriptCacheRestoredFunctionDependencySource*
			RestoredDependencies,
		TArray<FAngelscriptCacheSemanticDependency>& OutDependencies,
		bool& bOutUsedGraphCarriedDependencies)
	{
		bOutUsedGraphCarriedDependencies = false;
		TArray<FAngelscriptCacheSemanticDependency> CarriedDependencies;
		if (RestoredDependencies != nullptr
			&& RestoredDependencies->TryCopyActualDependencies(
				ModuleKey, FunctionKey, CarriedDependencies))
		{
			bOutUsedGraphCarriedDependencies = true;
			return FAngelscriptCacheCompilerBridge::
				CanonicalizeActualDependencies(
					CarriedDependencies, OutDependencies);
		}
		return FAngelscriptCacheCompilerBridge::
			CaptureSuccessfulActualDependencies(
				Function, CompilerResolver, OutDependencies);
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

	static FAngelscriptHash256 HashRawBytes(const TConstArrayView<uint8> Bytes)
	{
		return FAngelscriptHash256{FBlake3::HashBuffer(
			Bytes.GetData(), static_cast<uint64>(Bytes.Num()))};
	}

	static EAngelscriptCachedSourceKind ToCachedSourceKind(
		const EAngelscriptSourceKind SourceKind)
	{
		switch (SourceKind)
		{
		case EAngelscriptSourceKind::Game:
			return EAngelscriptCachedSourceKind::Game;
		case EAngelscriptSourceKind::Plugin:
			return EAngelscriptCachedSourceKind::Plugin;
		case EAngelscriptSourceKind::Memory:
			return EAngelscriptCachedSourceKind::Memory;
		default:
			return EAngelscriptCachedSourceKind::Invalid;
		}
	}

	static bool TryBuildLogicalMount(
		const FAngelscriptVirtualPath& VirtualPath,
		FString& OutLogicalMount)
	{
		OutLogicalMount.Reset();
		switch (VirtualPath.GetSourceKind())
		{
		case EAngelscriptSourceKind::Game:
			OutLogicalMount = TEXT("Game");
			return true;
		case EAngelscriptSourceKind::Plugin:
			if (VirtualPath.GetMountName().IsEmpty())
			{
				return false;
			}
			OutLogicalMount = TEXT("Plugin/") + VirtualPath.GetMountName();
			return true;
		default:
			return false;
		}
	}

	static bool TryMakeRecord(
		const EAngelscriptCacheRecordKind Kind,
		TArray<uint8>&& Payload,
		FAngelscriptPreparedRecord& OutRecord,
		FAngelscriptCacheCleanCaptureResult& OutError)
	{
		FAngelscriptPreparedRecord Candidate;
		Candidate.CanonicalPayload = MoveTemp(Payload);
		const FAngelscriptCacheValidationResult RecordIdResult =
			FAngelscriptCacheRecordArchive::TryBuildRecordId(
				Kind, Candidate.CanonicalPayload, Candidate.RecordId);
		if (!RecordIdResult.IsSuccess())
		{
			OutError = ValidationFailure(
				EAngelscriptCacheCleanCaptureError::RecordEncodingFailed,
				TEXT("RecordId"), RecordIdResult);
			return false;
		}
		OutRecord = MoveTemp(Candidate);
		return true;
	}

	template <typename SerializeFunctionType, typename ValueType>
	static bool TrySerializeRecord(
		const EAngelscriptCacheRecordKind Kind,
		const ValueType& Value,
		SerializeFunctionType&& Serialize,
		FAngelscriptPreparedRecord& OutRecord,
		FAngelscriptCacheCleanCaptureResult& OutError)
	{
		TArray<uint8> Payload;
		const FAngelscriptCacheValidationResult SerializeResult =
			Serialize(Value, Payload);
		if (!SerializeResult.IsSuccess())
		{
			OutError = ValidationFailure(
				EAngelscriptCacheCleanCaptureError::RecordEncodingFailed,
				TEXT("Record serialization"), SerializeResult);
			return false;
		}
		return TryMakeRecord(Kind, MoveTemp(Payload), OutRecord, OutError);
	}

	static bool TrySerializeTypeSchemaRecord(
		const FAngelscriptCachedTypeSchema& Value,
		FAngelscriptPreparedRecord& OutRecord,
		FAngelscriptCacheCleanCaptureResult& OutError)
	{
		TArray<uint8> Payload;
		FAngelscriptTypeSchemaFieldCoordinate FailureCoordinate;
		const FAngelscriptCacheValidationResult SerializeResult =
			FAngelscriptCacheTypeSchemaArchive::SerializeTypeSchemaWithDiagnostics(
				Value, Payload, FailureCoordinate);
		if (!SerializeResult.IsSuccess())
		{
			OutError = ValidationFailure(
				EAngelscriptCacheCleanCaptureError::RecordEncodingFailed,
				TEXT("TypeSchema serialization"), SerializeResult);
			OutError.Detail += FString::Printf(
				TEXT(" Field=%u Primary=%u Secondary=%u Tertiary=%u"),
				static_cast<uint32>(FailureCoordinate.Field),
				FailureCoordinate.PrimaryIndex,
				FailureCoordinate.SecondaryIndex,
				FailureCoordinate.TertiaryIndex);
			if (FailureCoordinate.Field
					== EAngelscriptTypeSchemaCapturedField::OrderedProperty
				&& FailureCoordinate.PrimaryIndex
					< static_cast<uint32>(Value.OrderedProperties.Num()))
			{
				const FAngelscriptCachedPropertySchema& Property =
					Value.OrderedProperties[FailureCoordinate.PrimaryIndex];
				OutError.Detail += FString::Printf(
					TEXT(" Property=%s TypeKind=%u Qualifiers=0x%x Storage=%u Offset=%u Size=%u Alignment=%u Flags=0x%x"),
					*Property.CanonicalName,
					static_cast<uint32>(Property.Type.Kind),
					Property.Type.QualifierFlags,
					static_cast<uint32>(Property.StorageKind),
					Property.SemanticByteOffset,
					Property.SemanticStorageSize,
					Property.SemanticStorageAlignment,
					Property.PropertySemanticFlags);
			}
			return false;
		}
		return TryMakeRecord(EAngelscriptCacheRecordKind::TypeSchema,
			MoveTemp(Payload), OutRecord, OutError);
	}

	static FAngelscriptCachedDataType MakeInt32Type()
	{
		FAngelscriptCachedDataType Type;
		Type.Kind = EAngelscriptCachedDataTypeKind::Primitive;
		Type.Primitive = EAngelscriptCachedPrimitiveType::Int32;
		return Type;
	}

	static bool TryMapPrimitiveDataType(
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

	static TOptional<EAngelscriptCachedFunctionInvocationKind>
	MapCachedFunctionInvocationKind(const asEBuildArtifactInvocationKind Kind)
	{
		switch (Kind)
		{
		case asBUILD_ARTIFACT_INVOCATION_GLOBAL_FUNCTION:
			return EAngelscriptCachedFunctionInvocationKind::GlobalFunction;
		case asBUILD_ARTIFACT_INVOCATION_METHOD:
			return EAngelscriptCachedFunctionInvocationKind::Method;
		case asBUILD_ARTIFACT_INVOCATION_CONSTRUCTOR:
			return EAngelscriptCachedFunctionInvocationKind::Constructor;
		case asBUILD_ARTIFACT_INVOCATION_DESTRUCTOR:
			return EAngelscriptCachedFunctionInvocationKind::Destructor;
		case asBUILD_ARTIFACT_INVOCATION_FACTORY:
			return EAngelscriptCachedFunctionInvocationKind::Factory;
		case asBUILD_ARTIFACT_INVOCATION_GENERATED_DEFAULT_CONSTRUCTOR:
			return EAngelscriptCachedFunctionInvocationKind::GeneratedDefaultConstructor;
		case asBUILD_ARTIFACT_INVOCATION_GENERATED_DEFAULT_DESTRUCTOR:
			return EAngelscriptCachedFunctionInvocationKind::GeneratedDefaultDestructor;
		case asBUILD_ARTIFACT_INVOCATION_INIT_DEFAULTS:
			return EAngelscriptCachedFunctionInvocationKind::InitDefaults;
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

	struct FClassGraphTypeAuthority
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

	static FAngelscriptCacheV1StorageLayout GetPropertyStorageLayout(
		const asCDataType& ScriptDataType)
	{
		if (ScriptDataType.IsObjectHandle())
		{
			return FAngelscriptCacheTypeSchemaArchive::GetV1BuildLayoutConstants()
				.GetObjectHandleStorageLayout();
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
			// Environment type ABI already commits the registered type's size,
			// alignment, properties and behaviours, so it is also the exact layout
			// authority required by an inline environment-value property.
			OutLayoutDependency.ExpectedContentOrValue =
				EnvironmentReference.ExpectedAbi;
		}
		return true;
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

	static FAngelscriptHash256 BuildCodeRootStableKey(const UClass& CodeRoot)
	{
		return HashOneString(
			TEXT("cache-v2-environment-code-root-key-v1"),
			CodeRoot.GetPathName());
	}

	static FAngelscriptHash256 BuildCodeRootDeclarationAbi(const UClass& CodeRoot)
	{
		TArray<FString> Inputs;
		Inputs.Add(CodeRoot.GetPathName());
		Inputs.Add(CodeRoot.GetSuperClass() != nullptr
			? CodeRoot.GetSuperClass()->GetPathName() : FString());
		return HashStrings(
			TEXT("cache-v2-environment-code-root-declaration-abi-v1"), Inputs);
	}

	static FAngelscriptCacheStableReference BuildCodeRootReference(
		const UClass& CodeRoot)
	{
		return {
			EAngelscriptCacheReferenceKind::EnvironmentSymbol,
			BuildCodeRootStableKey(CodeRoot),
			BuildCodeRootDeclarationAbi(CodeRoot),
		};
	}

	static bool TryMapSectionName(
		const FAngelscriptModuleDesc& Module,
		const FStringView ScriptSectionName,
		FString& OutCanonicalSection)
	{
		for (const FAngelscriptModuleDesc::FCodeSection& Section : Module.Code)
		{
			if (ScriptSectionName.Equals(Section.AbsoluteFilename,
					ESearchCase::CaseSensitive)
				|| ScriptSectionName.Equals(Section.VirtualPath,
					ESearchCase::CaseSensitive)
				|| ScriptSectionName.Equals(Section.RelativeFilename,
					ESearchCase::CaseSensitive)
				|| (!Section.AbsoluteFilename.IsEmpty()
					&& ScriptSectionName.Equals(Section.AbsoluteFilename,
						ESearchCase::IgnoreCase)))
			{
				OutCanonicalSection = Section.VirtualPath;
				return !OutCanonicalSection.IsEmpty();
			}
		}
		return false;
	}

	static bool TryBuildDebugPayload(
		const FAngelscriptModuleDesc& Module,
		const asCScriptFunction& Function,
		const FAngelscriptCachedSourceFileKey& SourceFileKey,
		TArray<uint8>& OutPayload,
		TArray<FAngelscriptCachedDebugSourceReference>& OutSources,
		FString& OutFailure)
	{
		OutPayload.Reset();
		OutSources.Reset();
		OutFailure.Reset();
		if (Function.scriptData == nullptr)
		{
			OutFailure = TEXT("The script function has no ScriptFunctionData");
			return false;
		}
		const char* PrimarySection = Function.GetScriptSectionName();
		FString CanonicalPrimarySection;
		if (PrimarySection == nullptr
			|| !TryMapSectionName(Module, ANSI_TO_TCHAR(PrimarySection),
				CanonicalPrimarySection))
		{
			OutFailure = TEXT("The primary debug section cannot be mapped to a stable virtual path");
			return false;
		}

		FAngelscriptFunctionDebugArtifact Artifact;
		FAngelscriptCachedDebugSourceReference& Source =
			Artifact.Sources.AddDefaulted_GetRef();
		Source.SourceFileKey = SourceFileKey;
		Source.CanonicalLogicalSection = MoveTemp(CanonicalPrimarySection);
		const FAngelscriptCacheValidationResult SectionKeyResult =
			FAngelscriptCacheRemainingRecordArchive::TryBuildLogicalSectionKey(
				Source.SourceFileKey,
				Source.CanonicalLogicalSection,
				Source.LogicalSectionKey);
		if (!SectionKeyResult.IsSuccess())
		{
			OutFailure = TEXT("The primary debug section has no stable logical-section key");
			return false;
		}
		Artifact.DeclaredAt = static_cast<uint32>(Function.scriptData->declaredAt);

		for (asUINT Index = 0;
			Index < Function.scriptData->lineNumbers.GetLength(); ++Index)
		{
			Artifact.LineNumbers.Add(static_cast<uint32>(
				Function.scriptData->lineNumbers[Index]));
		}

		if ((Function.scriptData->sectionIdxs.GetLength() & 1u) != 0)
		{
			OutFailure = TEXT("The function has an odd section-transition array");
			return false;
		}
		for (asUINT Index = 0;
			Index < Function.scriptData->sectionIdxs.GetLength(); Index += 2)
		{
			const int SectionIndex = Function.scriptData->sectionIdxs[Index + 1];
			if (SectionIndex < 0
				|| static_cast<asUINT>(SectionIndex)
					>= Function.engine->scriptSectionNames.GetLength()
				|| Function.engine->scriptSectionNames[SectionIndex] == nullptr)
			{
				OutFailure = TEXT("A debug section-transition index is invalid");
				return false;
			}
			FString CanonicalTransitionSection;
			if (!TryMapSectionName(Module,
				ANSI_TO_TCHAR(Function.engine->scriptSectionNames[SectionIndex]->AddressOf()),
				CanonicalTransitionSection))
			{
				OutFailure = TEXT("A debug section transition cannot be mapped to a stable virtual path");
				return false;
			}
			if (!CanonicalTransitionSection.Equals(
				Source.CanonicalLogicalSection, ESearchCase::CaseSensitive))
			{
				OutFailure = TEXT("The first cold-capture vertical cannot encode a cross-source debug transition");
				return false;
			}
			Artifact.SectionTransitions.Add({
				static_cast<uint32>(Function.scriptData->sectionIdxs[Index]), 0});
		}

		for (asUINT Index = 0; Index < Function.parameterNames.GetLength(); ++Index)
		{
			Artifact.ParameterNames.Add(UTF8_TO_TCHAR(
				Function.parameterNames[Index].AddressOf()));
		}

		for (asUINT Index = 0;
			Index < Function.scriptData->temporaryVariables.GetLength(); ++Index)
		{
			Artifact.TemporaryVariables.Add({
				static_cast<uint32>(
					Function.scriptData->temporaryVariables[Index].Offset),
				static_cast<uint32>(
					Function.scriptData->temporaryVariables[Index].Token)});
		}

		for (asUINT Index = 0;
			Index < Function.scriptData->variables.GetLength(); ++Index)
		{
			const asSScriptVariable* Variable =
				Function.scriptData->variables[Index];
			if (Variable == nullptr)
			{
				OutFailure = TEXT("The function has a null explicit-local debug entry");
				return false;
			}
			Artifact.LocalVariables.Add({
				UTF8_TO_TCHAR(Variable->name.AddressOf()),
				Variable->declaredAtProgramPos});
		}

		const FAngelscriptCacheValidationResult EncodeResult =
			FAngelscriptFunctionArtifactCodec::EncodeDebugArtifact(
				Artifact, OutPayload);
		if (!EncodeResult.IsSuccess())
		{
			OutFailure = FString::Printf(
				TEXT("The debug artifact codec rejected the captured data: Error=%u Offset=%llu"),
				static_cast<uint32>(EncodeResult.Error), EncodeResult.ByteOffset);
			return false;
		}
		OutSources = MoveTemp(Artifact.Sources);
		return true;
	}

	static FAngelscriptCachePackLocation LocationFromPack(
		const FAngelscriptEncodedPack& Pack,
		const FAngelscriptCachePackIndexEntry& Entry)
	{
		FAngelscriptCachePackLocation Location;
		Location.PackId = Pack.PackId;
		Location.PackOffset = Entry.PackOffset;
		Location.StoredSize = Entry.StoredSize;
		Location.RawSize = Entry.RawSize;
		Location.Codec = Entry.Codec;
		Location.RawChecksum = Entry.RawChecksum;
		return Location;
	}

	class FCleanCaptureCurrentSymbols final
		: public IAngelscriptCacheCurrentSymbolResolver
	{
	public:
		explicit FCleanCaptureCurrentSymbols(
			const FAngelscriptModuleDesc& Module)
		{
			if (Module.ScriptModule != nullptr
				&& Module.ScriptModule->engine != nullptr)
			{
				EnvironmentSymbols =
					MakeUnique<FAngelscriptCacheEngineEnvironmentResolver>(
						*Module.ScriptModule->engine);
			}
			for (const TSharedRef<FAngelscriptClassDesc>& Class : Module.Classes)
			{
				if (Class->CodeSuperClass == nullptr)
				{
					continue;
				}
				Entries.Add({
					BuildCodeRootStableKey(*Class->CodeSuperClass),
					BuildCodeRootDeclarationAbi(*Class->CodeSuperClass),
				});
			}
		}

		virtual TOptional<FAngelscriptCacheCurrentSymbol> Resolve(
			const EAngelscriptCacheReferenceKind ReferenceKind,
			const FAngelscriptHash256& StableKey) const override
		{
			LastReferenceKind = ReferenceKind;
			LastStableKey = StableKey;
			bLastResolved = false;
			if (ReferenceKind != EAngelscriptCacheReferenceKind::EnvironmentSymbol)
			{
				return {};
			}
			const FEntry* Match = nullptr;
			for (const FEntry& Entry : Entries)
			{
				if (Entry.StableKey != StableKey)
				{
					continue;
				}
				if (Match != nullptr && Match->Abi != Entry.Abi)
				{
					return {};
				}
				Match = &Entry;
			}
			if (Match != nullptr)
			{
				FAngelscriptCacheCurrentSymbol Symbol;
				Symbol.CurrentAbi = Match->Abi;
				bLastResolved = true;
				return Symbol;
			}
			TOptional<FAngelscriptCacheCurrentSymbol> Resolved =
				EnvironmentSymbols.IsValid()
				? EnvironmentSymbols->Resolve(ReferenceKind, StableKey)
				: TOptional<FAngelscriptCacheCurrentSymbol>();
			bLastResolved = Resolved.IsSet();
			return Resolved;
		}

		FString DescribeLastResolution() const
		{
			return FString::Printf(
				TEXT("CurrentResolver last request: ReferenceKind=%u StableKey=%s Resolved=%d"),
				static_cast<uint32>(LastReferenceKind),
				*LastStableKey.ToHexString(),
				bLastResolved ? 1 : 0);
		}

	private:
		struct FEntry
		{
			FAngelscriptHash256 StableKey;
			FAngelscriptHash256 Abi;
		};
		TArray<FEntry> Entries;
		TUniquePtr<FAngelscriptCacheEngineEnvironmentResolver>
			EnvironmentSymbols;
		mutable EAngelscriptCacheReferenceKind LastReferenceKind =
			static_cast<EAngelscriptCacheReferenceKind>(0);
		mutable FAngelscriptHash256 LastStableKey;
		mutable bool bLastResolved = false;
	};

	// Maps the compiler's engine-local pointer observations to the stable
	// authorities built by this same clean-capture transaction. No pointer or
	// numeric FunctionId escapes this resolver into the persisted record graph.
	class FCleanCaptureBuildDependencyResolver final
		: public IAngelscriptCacheBuildDependencyResolver
	{
	public:
		void AddType(
			const asCTypeInfo* Type,
			const FAngelscriptCacheStableReference& Declaration,
			const FAngelscriptHash256& Layout)
		{
			Types.Add({Type, Declaration, Layout});
		}

		void AddProperty(
			const asCTypeInfo* OwnerType,
			const asCObjectProperty* Property,
			const FAngelscriptCacheStableReference& Declaration,
			const FAngelscriptHash256& Layout)
		{
			Properties.Add({OwnerType, Property, Declaration, Layout});
		}

		void AddGlobal(
			const asCGlobalProperty* Global,
			const FAngelscriptCacheStableReference& Declaration,
			const FAngelscriptHash256& StorageLayout,
			const TOptional<FAngelscriptHash256>& HardValue)
		{
			Globals.Add({Global, Declaration, StorageLayout, HardValue});
		}

		void AddFunction(
			const asCScriptFunction* Function,
			const FAngelscriptCacheStableReference& Declaration,
			const TOptional<FAngelscriptHash256>& Content = {})
		{
			Functions.Add({Function, Declaration, Content});
		}

		void AddDerivedStaticClassGlobal(
			const asCGlobalProperty* Global,
			const FAngelscriptCacheStableReference& OwningTypeDeclaration)
		{
			DerivedStaticClassGlobals.Add({Global, OwningTypeDeclaration});
		}

		void AddDerivedStaticClassFunction(
			const asCScriptFunction* Function,
			const FAngelscriptCacheStableReference& OwningTypeDeclaration)
		{
			DerivedStaticClassFunctions.Add({Function, OwningTypeDeclaration});
		}

		virtual bool Resolve(
			const asSBuildArtifactDependency& RawDependency,
			FAngelscriptCacheSemanticDependency& OutDependency) const override
		{
			OutDependency = {};
			if (RawDependency.descriptorVersion != 1)
			{
				return false;
			}

			switch (RawDependency.referenceKind)
			{
			case asBUILD_ARTIFACT_REFERENCE_TYPE:
			{
				if (RawDependency.type == nullptr
					|| RawDependency.function != nullptr
					|| RawDependency.globalProperty != nullptr
					|| RawDependency.propertyOwnerType != nullptr
					|| RawDependency.objectProperty != nullptr)
				{
					return false;
				}
				const FTypeEntry* Entry = Types.FindByPredicate(
					[&RawDependency](const FTypeEntry& Candidate)
					{
						return Candidate.Type == RawDependency.type;
					});
				if (Entry == nullptr)
				{
					FAngelscriptCacheStableReference EnvironmentType;
					if (RawDependency.type->module != nullptr
						|| !FAngelscriptCacheEnvironmentIdentity::
							TryBuildTypeReference(
								*RawDependency.type, EnvironmentType)
						|| (RawDependency.kind
								!= asBUILD_ARTIFACT_DEPENDENCY_DECLARATION
							&& RawDependency.kind
								!= asBUILD_ARTIFACT_DEPENDENCY_VALUE_LAYOUT))
					{
						return false;
					}
					OutDependency.Kind =
						EAngelscriptCacheSemanticDependencyKind::EnvironmentAbi;
					OutDependency.Target = EnvironmentType;
					return true;
				}
				if (RawDependency.kind
					== asBUILD_ARTIFACT_DEPENDENCY_DECLARATION)
				{
					OutDependency.Kind =
						EAngelscriptCacheSemanticDependencyKind::Declaration;
					OutDependency.Target = Entry->Declaration;
					return true;
				}
				if (RawDependency.kind
					== asBUILD_ARTIFACT_DEPENDENCY_VALUE_LAYOUT)
				{
					OutDependency.Kind =
						EAngelscriptCacheSemanticDependencyKind::ValueLayout;
					OutDependency.Target = Entry->Declaration;
					OutDependency.ExpectedContentOrValue = Entry->Layout;
					return true;
				}
				return false;
			}

			case asBUILD_ARTIFACT_REFERENCE_PROPERTY:
			{
				if (RawDependency.kind
						!= asBUILD_ARTIFACT_DEPENDENCY_PROPERTY_LAYOUT
					|| RawDependency.type != nullptr
					|| RawDependency.function != nullptr
					|| RawDependency.globalProperty != nullptr
					|| RawDependency.propertyOwnerType == nullptr
					|| RawDependency.objectProperty == nullptr)
				{
					return false;
				}
				const FPropertyEntry* Entry = Properties.FindByPredicate(
					[&RawDependency](const FPropertyEntry& Candidate)
					{
						return Candidate.OwnerType
							== RawDependency.propertyOwnerType
							&& Candidate.Property
								== RawDependency.objectProperty;
					});
				if (Entry == nullptr)
				{
					return false;
				}
				OutDependency.Kind =
					EAngelscriptCacheSemanticDependencyKind::PropertyLayout;
				OutDependency.Target = Entry->Declaration;
				OutDependency.ExpectedContentOrValue = Entry->Layout;
				return true;
			}

			case asBUILD_ARTIFACT_REFERENCE_GLOBAL:
			{
				if (RawDependency.type != nullptr
					|| RawDependency.function != nullptr
					|| RawDependency.globalProperty == nullptr
					|| RawDependency.propertyOwnerType != nullptr
					|| RawDependency.objectProperty != nullptr)
				{
					return false;
				}
				const FDerivedStaticClassGlobalEntry* DerivedEntry =
					DerivedStaticClassGlobals.FindByPredicate(
						[&RawDependency](
							const FDerivedStaticClassGlobalEntry& Candidate)
						{
							return Candidate.Global
								== RawDependency.globalProperty;
						});
				if (DerivedEntry != nullptr)
				{
					if (RawDependency.kind
						!= asBUILD_ARTIFACT_DEPENDENCY_GLOBAL_STORAGE)
					{
						return false;
					}
					OutDependency.Kind =
						EAngelscriptCacheSemanticDependencyKind::Declaration;
					OutDependency.Target =
						DerivedEntry->OwningTypeDeclaration;
					return true;
				}

				const FGlobalEntry* Entry = Globals.FindByPredicate(
					[&RawDependency](const FGlobalEntry& Candidate)
					{
						return Candidate.Global
							== RawDependency.globalProperty;
					});
				if (Entry == nullptr)
				{
					return false;
				}
				if (RawDependency.kind
					== asBUILD_ARTIFACT_DEPENDENCY_GLOBAL_STORAGE)
				{
					OutDependency.Kind =
						EAngelscriptCacheSemanticDependencyKind::GlobalStorage;
					OutDependency.Target = Entry->Declaration;
					OutDependency.ExpectedContentOrValue = Entry->StorageLayout;
					return true;
				}
				if (RawDependency.kind
						== asBUILD_ARTIFACT_DEPENDENCY_HARD_VALUE
					&& Entry->HardValue.IsSet())
				{
					OutDependency.Kind =
						EAngelscriptCacheSemanticDependencyKind::HardValue;
					OutDependency.Target = Entry->Declaration;
					OutDependency.ExpectedContentOrValue = Entry->HardValue;
					return true;
				}
				return false;
			}

			case asBUILD_ARTIFACT_REFERENCE_FUNCTION:
			{
				if (RawDependency.type != nullptr
					|| RawDependency.function == nullptr
					|| RawDependency.globalProperty != nullptr
					|| RawDependency.propertyOwnerType != nullptr
					|| RawDependency.objectProperty != nullptr)
				{
					return false;
				}
				const FDerivedStaticClassFunctionEntry* DerivedEntry =
					DerivedStaticClassFunctions.FindByPredicate(
						[&RawDependency](
							const FDerivedStaticClassFunctionEntry& Candidate)
						{
							return Candidate.Function
								== RawDependency.function;
						});
				if (DerivedEntry != nullptr)
				{
					if (RawDependency.kind
						!= asBUILD_ARTIFACT_DEPENDENCY_SIGNATURE)
					{
						return false;
					}
					OutDependency.Kind =
						EAngelscriptCacheSemanticDependencyKind::Declaration;
					OutDependency.Target =
						DerivedEntry->OwningTypeDeclaration;
					return true;
				}

				// A generated zero-argument factory is the VM implementation of
				// the owning type declaration. This derived identity must win even
				// when the factory is present in the complete local function table;
				// the opaque relocation codec applies the same rule.
				if (RawDependency.kind
						== asBUILD_ARTIFACT_DEPENDENCY_SIGNATURE
					&& RawDependency.function->artifactInvocationKind
						== asBUILD_ARTIFACT_INVOCATION_FACTORY
					&& RawDependency.function->GetParamCount() == 0
					&& RawDependency.function->artifactOwnerType != nullptr
					&& RawDependency.function->returnType.GetTypeInfo()
						== RawDependency.function->artifactOwnerType)
				{
					const FTypeEntry* DerivedOwner = Types.FindByPredicate(
						[&RawDependency](const FTypeEntry& Candidate)
						{
							return Candidate.Type
								== RawDependency.function->artifactOwnerType;
						});
					if (DerivedOwner != nullptr)
					{
						OutDependency.Kind =
							EAngelscriptCacheSemanticDependencyKind::Declaration;
						OutDependency.Target = DerivedOwner->Declaration;
						return true;
					}
				}
				const FFunctionEntry* Entry = Functions.FindByPredicate(
					[&RawDependency](const FFunctionEntry& Candidate)
					{
						return Candidate.Function == RawDependency.function;
					});
				if (Entry == nullptr)
				{
					FAngelscriptCacheStableReference EnvironmentFunction;
					if (RawDependency.function->module == nullptr
						&& (RawDependency.kind
								== asBUILD_ARTIFACT_DEPENDENCY_SIGNATURE
							|| RawDependency.kind
								== asBUILD_ARTIFACT_DEPENDENCY_FUNCTION_CONTENT)
						&& FAngelscriptCacheEnvironmentIdentity::
							TryBuildFunctionReference(
								*RawDependency.function, EnvironmentFunction))
					{
						OutDependency.Kind =
							EAngelscriptCacheSemanticDependencyKind::EnvironmentAbi;
						OutDependency.Target = EnvironmentFunction;
						return true;
					}
					return false;
				}
				if (RawDependency.kind
					== asBUILD_ARTIFACT_DEPENDENCY_SIGNATURE)
				{
					OutDependency.Kind =
						EAngelscriptCacheSemanticDependencyKind::Signature;
					OutDependency.Target = Entry->Declaration;
					return true;
				}
				if (RawDependency.kind
						== asBUILD_ARTIFACT_DEPENDENCY_FUNCTION_CONTENT
					&& Entry->Content.IsSet())
				{
					OutDependency.Kind =
						EAngelscriptCacheSemanticDependencyKind::FunctionContent;
					OutDependency.Target = Entry->Declaration;
					OutDependency.ExpectedContentOrValue = Entry->Content;
					return true;
				}
				return false;
			}

			default:
				return false;
			}
		}

	private:
		struct FTypeEntry final
		{
			const asCTypeInfo* Type = nullptr;
			FAngelscriptCacheStableReference Declaration;
			FAngelscriptHash256 Layout;
		};

		struct FPropertyEntry final
		{
			const asCTypeInfo* OwnerType = nullptr;
			const asCObjectProperty* Property = nullptr;
			FAngelscriptCacheStableReference Declaration;
			FAngelscriptHash256 Layout;
		};

		struct FGlobalEntry final
		{
			const asCGlobalProperty* Global = nullptr;
			FAngelscriptCacheStableReference Declaration;
			FAngelscriptHash256 StorageLayout;
			TOptional<FAngelscriptHash256> HardValue;
		};

		struct FFunctionEntry final
		{
			const asCScriptFunction* Function = nullptr;
			FAngelscriptCacheStableReference Declaration;
			TOptional<FAngelscriptHash256> Content;
		};

		struct FDerivedStaticClassGlobalEntry final
		{
			const asCGlobalProperty* Global = nullptr;
			FAngelscriptCacheStableReference OwningTypeDeclaration;
		};

		struct FDerivedStaticClassFunctionEntry final
		{
			const asCScriptFunction* Function = nullptr;
			FAngelscriptCacheStableReference OwningTypeDeclaration;
		};

		TArray<FTypeEntry> Types;
		TArray<FPropertyEntry> Properties;
		TArray<FGlobalEntry> Globals;
		TArray<FFunctionEntry> Functions;
		TArray<FDerivedStaticClassGlobalEntry> DerivedStaticClassGlobals;
		TArray<FDerivedStaticClassFunctionEntry> DerivedStaticClassFunctions;
	};

	class FCleanCaptureCurrentLayouts final
		: public IAngelscriptCacheCurrentLayoutResolver
	{
	public:
		explicit FCleanCaptureCurrentLayouts(
			const FAngelscriptModuleDesc& Module)
		{
			if (Module.ScriptModule != nullptr
				&& Module.ScriptModule->engine != nullptr)
			{
				Module.ScriptModule->engine->allRegisteredTypes.IterateAll(
					[this](asCTypeInfo* Type)
					{
						FAngelscriptCacheStableReference Reference;
						if (Type == nullptr || Type->module != nullptr
							|| !FAngelscriptCacheEnvironmentIdentity::
								TryBuildTypeReference(*Type, Reference))
						{
							return;
						}
						const asCDataType DataType =
							asCDataType::CreateType(Type, false);
						const int32 Size = DataType.GetSizeInMemoryBytes();
						const int32 Alignment = DataType.GetAlignment();
						if (Size <= 0 || Alignment <= 0
							|| !FMath::IsPowerOfTwo(Alignment))
						{
							return;
						}
						EnvironmentEntries.Add({
							Reference,
							static_cast<uint32>(Size),
							static_cast<uint32>(Alignment),
						});
					});
			}
			for (const TSharedRef<FAngelscriptClassDesc>& Class : Module.Classes)
			{
				const asCObjectType* ScriptType =
					static_cast<const asCObjectType*>(Class->ScriptType);
				if (Class->CodeSuperClass == nullptr || ScriptType == nullptr
					|| ScriptType->basePropertyOffset < 0
					|| ScriptType->alignment <= 0)
				{
					continue;
				}
				Entries.Add({
					BuildCodeRootStableKey(*Class->CodeSuperClass),
					static_cast<uint32>(ScriptType->basePropertyOffset),
					static_cast<uint32>(ScriptType->shadowType != nullptr
						? static_cast<const asCObjectType*>(ScriptType->shadowType)->alignment
						: ScriptType->alignment),
				});
			}
		}

		virtual TOptional<FAngelscriptCacheResolvedDataTypeLayout>
		ResolveDataTypeLayout(
			const FAngelscriptCachedDataType& DataType,
			const IAngelscriptCacheProspectiveTypeLayoutView&) const override
		{
			if (DataType.Kind != EAngelscriptCachedDataTypeKind::EnvironmentType
				|| !DataType.TypeReference.IsSet())
			{
				return {};
			}
			const FAngelscriptCacheStableReference& TypeReference =
				DataType.TypeReference.GetValue();
			if (TypeReference.Kind
					!= EAngelscriptCacheReferenceKind::EnvironmentSymbol
				|| TypeReference.StableKey.IsZero()
				|| TypeReference.ExpectedAbi.IsZero())
			{
				return {};
			}

			const FEnvironmentEntry* Match = nullptr;
			for (const FEnvironmentEntry& Entry : EnvironmentEntries)
			{
				if (Entry.Reference.StableKey
					!= TypeReference.StableKey)
				{
					continue;
				}
				if (Entry.Reference.ExpectedAbi
						!= TypeReference.ExpectedAbi
					|| (Match != nullptr
						&& (Match->SemanticStorageSize
								!= Entry.SemanticStorageSize
							|| Match->SemanticStorageAlignment
								!= Entry.SemanticStorageAlignment)))
				{
					return {};
				}
				Match = &Entry;
			}
			if (Match == nullptr)
			{
				return {};
			}

			FAngelscriptCacheResolvedDataTypeLayout Layout;
			const uint32 ObjectHandleFlag = static_cast<uint32>(
				EAngelscriptCachedTypeQualifierFlags::ObjectHandle);
			if ((DataType.QualifierFlags & ObjectHandleFlag) != 0)
			{
				const FAngelscriptCacheV1StorageLayout Handle =
					FAngelscriptCacheTypeSchemaArchive::
						GetV1BuildLayoutConstants().GetObjectHandleStorageLayout();
				Layout.StorageKind =
					EAngelscriptCachedPropertyStorageKind::ObjectHandle;
				Layout.SemanticStorageSize = Handle.SemanticStorageSize;
				Layout.SemanticStorageAlignment =
					Handle.SemanticStorageAlignment;
			}
			else
			{
				Layout.StorageKind =
					EAngelscriptCachedPropertyStorageKind::InlineValue;
				Layout.SemanticStorageSize = Match->SemanticStorageSize;
				Layout.SemanticStorageAlignment =
					Match->SemanticStorageAlignment;
			}
			return Layout;
		}

		virtual TOptional<FAngelscriptCacheResolvedTypeLayoutInput>
		ResolveTypeLayoutInput(
			const EAngelscriptCachedTypeLayoutInputKind InputKind,
			const EAngelscriptCacheReferenceKind ReferenceKind,
			const FAngelscriptHash256& StableKey) const override
		{
			if (InputKind != EAngelscriptCachedTypeLayoutInputKind::CodeRoot
				|| ReferenceKind
					!= EAngelscriptCacheReferenceKind::EnvironmentSymbol)
			{
				return {};
			}
			const FEntry* Match = nullptr;
			for (const FEntry& Entry : Entries)
			{
				if (Entry.StableKey != StableKey)
				{
					continue;
				}
				if (Match != nullptr
					&& (Match->Boundary != Entry.Boundary
						|| Match->Alignment != Entry.Alignment))
				{
					return {};
				}
				Match = &Entry;
			}
			if (Match != nullptr)
			{
				FAngelscriptCacheResolvedTypeLayoutInput Layout;
				Layout.BoundaryContribution = Match->Boundary;
				Layout.AlignmentContribution = Match->Alignment;
				return Layout;
			}
			return {};
		}

	private:
		struct FEntry
		{
			FAngelscriptHash256 StableKey;
			uint32 Boundary = 0;
			uint32 Alignment = 0;
		};
		struct FEnvironmentEntry
		{
			FAngelscriptCacheStableReference Reference;
			uint32 SemanticStorageSize = 0;
			uint32 SemanticStorageAlignment = 0;
		};
		TArray<FEntry> Entries;
		TArray<FEnvironmentEntry> EnvironmentEntries;
	};

	static bool TryOpenCapturedGraph(
		const FAngelscriptModuleDesc& Module,
		const FAngelscriptCacheCleanCaptureOptions& Options,
		const FAngelscriptCacheCleanModuleArtifacts& Artifacts,
		const FAngelscriptCacheReadLimits& Limits,
		FAngelscriptCacheReadBudget& Budget,
		FAngelscriptValidatedModuleGraph& OutGraph,
		uint32& OutReachableRecordCount,
		FAngelscriptCacheCleanCaptureResult& OutError)
	{
		OutGraph.Reset();
		OutReachableRecordCount = 0;
		asCModule* ScriptModule = static_cast<asCModule*>(Module.ScriptModule);
		if (ScriptModule == nullptr || ScriptModule->engine == nullptr)
		{
			OutError = Failure(
				EAngelscriptCacheCleanCaptureError::GraphValidationFailed,
				TEXT("The compiled module has no VM graph-validation context"));
			return false;
		}

		FAngelscriptDecodedCacheRecordBatch DecodeBatch(Budget, Limits);
		TArray<FAngelscriptDecodedCacheRecordHandle> DecodedRecords;
		DecodedRecords.Reserve(Artifacts.Records.Num());
		const FAngelscriptDecodedCacheRecord* SourceIndex = nullptr;
		const FAngelscriptCachedModuleInterface* DecodedInterface = nullptr;
		const FAngelscriptCachedModuleSnapshot* DecodedSnapshot = nullptr;
		for (const FAngelscriptPreparedRecord& Record : Artifacts.Records)
		{
			TOptional<FAngelscriptDecodedCacheRecordHandle> Decoded;
			const FAngelscriptCacheValidationResult DecodeResult =
				DecodeBatch.TryDecode(
					Record.RecordId, Record.CanonicalPayload, Decoded);
			if (!DecodeResult.IsSuccess() || !Decoded.IsSet())
			{
				OutError = ValidationFailure(
					EAngelscriptCacheCleanCaptureError::GraphValidationFailed,
					TEXT("Captured record decode"), DecodeResult);
				return false;
			}
			DecodedRecords.Add(Decoded.GetValue());
			if (Record.RecordId.Kind == EAngelscriptCacheRecordKind::SourceIndex)
			{
				SourceIndex = &Decoded.GetValue().Get();
			}
			else if (Record.RecordId.Kind
				== EAngelscriptCacheRecordKind::ModuleInterface)
			{
				DecodedInterface =
					Decoded.GetValue()->TryGetModuleInterface();
			}
			else if (Record.RecordId.Kind
				== EAngelscriptCacheRecordKind::ModuleSnapshot)
			{
				DecodedSnapshot =
					Decoded.GetValue()->TryGetModuleSnapshot();
			}
		}
		if (SourceIndex == nullptr || !DecodeBatch.PromoteToRetained())
		{
			OutError = Failure(
				EAngelscriptCacheCleanCaptureError::GraphValidationFailed,
				TEXT("The complete captured record batch could not be retained"));
			return false;
		}

		FCleanCaptureCurrentSymbols CurrentSymbols(Module);
		FCleanCaptureCurrentLayouts CurrentLayouts(Module);
		FAngelscriptFunctionArtifactCodec OpaquePayloads(
			*ScriptModule, *ScriptModule->engine);
		FAngelscriptCacheModuleGraphValidationContext Context;
		Context.SelectedProfile = Options.Profile;
		Context.SelectedSourceSnapshot = Artifacts.SourceSnapshot;
		Context.SourceIndex = SourceIndex;
		Context.CurrentSymbols = &CurrentSymbols;
		Context.CurrentLayouts = &CurrentLayouts;
		Context.OpaquePayloads = &OpaquePayloads;

		FAngelscriptValidatedModuleGraph Graph;
		const FAngelscriptCacheValidationResult GraphResult =
			ValidateModuleSnapshotGraph(
				Artifacts.ModuleSnapshot.RecordId,
				DecodedRecords,
				Context,
				Limits,
				Budget,
				Graph);
		if (!GraphResult.IsSuccess())
		{
			OutError = ValidationFailure(
				EAngelscriptCacheCleanCaptureError::GraphValidationFailed,
				TEXT("Captured module graph"), GraphResult);
			if (GraphResult.Stage
				== EAngelscriptCacheValidationStage::CurrentResolver)
			{
				OutError.Detail += TEXT("; ");
				OutError.Detail += CurrentSymbols.DescribeLastResolution();
			}
			if (!OpaquePayloads.GetLastExecutionFailureDetail().IsEmpty())
			{
				OutError.Detail += TEXT("; ");
				OutError.Detail += OpaquePayloads.GetLastExecutionFailureDetail();
			}
			if (GraphResult.Error
					== EAngelscriptCacheValidationError::MissingCoverage
				&& DecodedInterface != nullptr && DecodedSnapshot != nullptr)
			{
				const FAngelscriptCachedDeclaration* FirstFunction = nullptr;
				const FAngelscriptCachedDeclaration* FirstType = nullptr;
				uint32 FunctionDeclarationCount = 0;
				uint32 TypeDeclarationCount = 0;
				for (const FAngelscriptCachedDeclaration& Declaration
					: DecodedInterface->Declarations)
				{
					if (Declaration.DeclarationKind
						== EAngelscriptCacheDeclarationKind::Type)
					{
						++TypeDeclarationCount;
						if (FirstType == nullptr)
						{
							FirstType = &Declaration;
						}
					}
					if (Declaration.DeclarationKind
						== EAngelscriptCacheDeclarationKind::Function)
					{
						++FunctionDeclarationCount;
						if (FirstFunction == nullptr)
						{
							FirstFunction = &Declaration;
						}
					}
				}
				const FString DeclarationKey = FirstFunction != nullptr
					? FirstFunction->StableKey.ToHexString() : TEXT("none");
				const FString LinkKey = !DecodedSnapshot->FunctionBodies.IsEmpty()
					? DecodedSnapshot->FunctionBodies[0].FunctionKey.Hash.ToHexString()
					: TEXT("none");
				const FString TypeDeclarationKey = FirstType != nullptr
					? FirstType->StableKey.ToHexString() : TEXT("none");
				const FString TypeLinkKey = !DecodedSnapshot->TypeSchemas.IsEmpty()
					? DecodedSnapshot->TypeSchemas[0].TypeKey.Hash.ToHexString()
					: TEXT("none");
				OutError.Detail += FString::Printf(
					TEXT("; Coverage types=%u schemas=%d first-type=%s first-schema=%s functions=%u bodies=%d first-function=%s coverage=%u first-body=%s"),
					TypeDeclarationCount,
					DecodedSnapshot->TypeSchemas.Num(),
					*TypeDeclarationKey,
					*TypeLinkKey,
					FunctionDeclarationCount,
					DecodedSnapshot->FunctionBodies.Num(),
					*DeclarationKey,
					FirstFunction != nullptr
						? static_cast<uint32>(FirstFunction->BodyCoverage) : 0u,
					*LinkKey);
			}
			return false;
		}
		if (!(Graph.GetModuleKey() == Artifacts.ModuleKey))
		{
			OutError = Failure(
				EAngelscriptCacheCleanCaptureError::GraphValidationFailed,
				TEXT("The validated graph has the wrong module identity"));
			return false;
		}

		// SourceIndex is generation/context authority rather than a child owned by
		// one ModuleSnapshot, so the module graph deliberately does not publish it
		// in GetReachableRecords(). It was nevertheless decoded by the sole factory
		// and consumed by this exact graph for selected-source and debug-source
		// validation. Prove complete capture coverage without changing the graph's
		// long-standing record ordinals or synthesizing the expected count.
		uint32 ValidatedRecordCount = 0;
		uint32 ContextSourceIndexCount = 0;
		for (const FAngelscriptDecodedCacheRecordHandle& Record : DecodedRecords)
		{
			if (Record->GetRecordId().Kind
				== EAngelscriptCacheRecordKind::SourceIndex)
			{
				++ContextSourceIndexCount;
				++ValidatedRecordCount;
				continue;
			}
			if (!Graph.FindRecordOrdinal(Record->GetRecordId()).IsSet())
			{
				OutError = Failure(
					EAngelscriptCacheCleanCaptureError::GraphValidationFailed,
					FString::Printf(
						TEXT("Captured record kind %u is outside the validated module graph"),
						static_cast<uint32>(Record->GetRecordId().Kind)));
				return false;
			}
			++ValidatedRecordCount;
		}
		if (ContextSourceIndexCount != 1
			|| ValidatedRecordCount != static_cast<uint32>(DecodedRecords.Num()))
		{
			OutError = Failure(
				EAngelscriptCacheCleanCaptureError::GraphValidationFailed,
				FString::Printf(
					TEXT("Captured coverage is incomplete: ContextSourceIndexes=%u ModuleGraphRecords=%d Validated=%u Captured=%d"),
					ContextSourceIndexCount,
					Graph.GetReachableRecords().Num(),
					ValidatedRecordCount,
					DecodedRecords.Num()));
			return false;
		}
		OutReachableRecordCount = ValidatedRecordCount;
		OutGraph = MoveTemp(Graph);
		return true;
	}

	static bool TryValidateCapturedGraph(
		const FAngelscriptModuleDesc& Module,
		const FAngelscriptCacheCleanCaptureOptions& Options,
		const FAngelscriptCacheCleanModuleArtifacts& Artifacts,
		uint32& OutReachableRecordCount,
		TArray<FAngelscriptFunctionArtifactIdentity>&
			OutFunctionArtifactIdentities,
		FAngelscriptCacheCleanCaptureResult& OutError)
	{
		OutFunctionArtifactIdentities.Reset();
		FAngelscriptCacheReadLimits Limits;
		FAngelscriptCacheReadBudget Budget;
		FAngelscriptValidatedModuleGraph Graph;
		if (!TryOpenCapturedGraph(
			Module,
			Options,
			Artifacts,
			Limits,
			Budget,
			Graph,
			OutReachableRecordCount,
			OutError))
		{
			return false;
		}

		TArray<FAngelscriptFunctionArtifactIdentity> ValidatedIdentities;
		for (const FAngelscriptDecodedCacheRecordHandle& Record
			: Graph.GetReachableRecords())
		{
			const FAngelscriptCachedFunctionBody* FunctionBody =
				Record->TryGetFunctionBody();
			if (FunctionBody != nullptr)
			{
				ValidatedIdentities.Add(FunctionBody->Identity);
			}
		}
		ValidatedIdentities.Sort([](
			const FAngelscriptFunctionArtifactIdentity& Left,
			const FAngelscriptFunctionArtifactIdentity& Right)
		{
			return Left.FunctionKey.Hash < Right.FunctionKey.Hash;
		});
		for (int32 Index = 1; Index < ValidatedIdentities.Num(); ++Index)
		{
			if (ValidatedIdentities[Index - 1].FunctionKey.Hash
				== ValidatedIdentities[Index].FunctionKey.Hash)
			{
				OutError = Failure(
					EAngelscriptCacheCleanCaptureError::GraphValidationFailed,
					TEXT("The validated graph produced duplicate function identities"));
				return false;
			}
		}
		OutFunctionArtifactIdentities = MoveTemp(ValidatedIdentities);
		return true;
	}

	static uint32 BuildClassReflectionFlags(
		const FAngelscriptClassDesc& Class)
	{
		uint32 Flags = 0;
		const auto Add = [&Flags](const bool bSet,
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
		const auto Add = [&Flags](const bool bSet,
			const EAngelscriptCachedPropertySemanticFlags Flag)
		{
			if (bSet)
			{
				Flags |= static_cast<uint32>(Flag);
			}
		};
		// Every descriptor in FAngelscriptClassDesc::Properties is the declaration
		// authority for an explicit or GC-required FProperty.  The mutable
		// bHasUnrealProperty bit instead describes whether that property exists on
		// the currently installed UClass.  Soft reload deliberately clears it for
		// newly declared properties while PIE keeps the old UClass alive.  Cache
		// artifacts describe the next cold-start class shape, so descriptor
		// presence--not the temporarily installed state--is authoritative here.
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

	static FAngelscriptCacheCleanCaptureResult CaptureClassGraphPrimitiveVertical(
		const TSharedRef<FAngelscriptModuleDesc>& Module,
		const FAngelscriptCacheCleanCaptureOptions& Options,
		const FAngelscriptCachedSourceIndex& SourceIndex,
		const FAngelscriptCachedSourceFile& SourceFile,
		const FAngelscriptStableModuleKey& ModuleKey,
		const IAngelscriptCacheRestoredFunctionDependencySource*
			RestoredDependencies,
		FAngelscriptCacheCleanModuleArtifacts& OutArtifacts)
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
				TEXT("The class-graph capture vertical requires two or more classes and no enum, delegate, import, typedef or post-init declaration"));
		}

		struct FClassCapture
		{
			const FAngelscriptClassDesc* ClassDesc = nullptr;
			asCObjectType* ScriptType = nullptr;
			int32 BaseCaptureIndex = INDEX_NONE;
			FString ClassNamespace;
			FString ClassName;
			FAngelscriptCachedDeclaration TypeDeclaration;
			FAngelscriptCachedTypeSchema TypeSchema;
			TArray<FAngelscriptCachedDeclaration> PropertyDeclarations;
			asCGlobalProperty* GeneratedStaticClassGlobal = nullptr;
			asCScriptFunction* GeneratedStaticClassFunction = nullptr;
			FAngelscriptPreparedRecord TypeRecord;
		};

		TArray<const FAngelscriptClassDesc*> PendingClasses;
		PendingClasses.Reserve(Module->Classes.Num());
		for (const TSharedRef<FAngelscriptClassDesc>& Class : Module->Classes)
		{
			PendingClasses.Add(&Class.Get());
		}
		TArray<FClassCapture> Classes;
		Classes.Reserve(Module->Classes.Num());
		while (!PendingClasses.IsEmpty())
		{
			int32 SelectedPendingIndex = INDEX_NONE;
			int32 SelectedBaseCaptureIndex = INDEX_NONE;
			FString SelectedClassNamespace;
			FString SelectedClassName;
			for (int32 PendingIndex = 0;
				PendingIndex < PendingClasses.Num(); ++PendingIndex)
			{
				const FAngelscriptClassDesc* ClassDesc = PendingClasses[PendingIndex];
				asCObjectType* ScriptType = ClassDesc != nullptr
					? static_cast<asCObjectType*>(ClassDesc->ScriptType) : nullptr;
				if (ScriptType == nullptr)
				{
					return Failure(EAngelscriptCacheCleanCaptureError::NotCacheable,
						TEXT("A class descriptor has no compiled VM type"));
				}

				int32 BaseCaptureIndex = INDEX_NONE;
				if (ScriptType->derivedFrom != nullptr)
				{
					BaseCaptureIndex = Classes.IndexOfByPredicate(
						[ScriptType](const FClassCapture& Candidate)
						{
							return Candidate.ScriptType == ScriptType->derivedFrom;
						});
					if (BaseCaptureIndex == INDEX_NONE)
					{
						continue;
					}
				}

				const FString ClassNamespace = ClassDesc->Namespace.IsSet()
					? ClassDesc->Namespace.GetValue()
					: UTF8_TO_TCHAR(ScriptType->GetNamespace());
				const FString ClassName = UTF8_TO_TCHAR(ScriptType->GetName());
				const int32 NamespaceComparison = SelectedPendingIndex == INDEX_NONE
					? -1
					: ClassNamespace.Compare(
						SelectedClassNamespace, ESearchCase::CaseSensitive);
				const int32 NameComparison = SelectedPendingIndex == INDEX_NONE
					|| NamespaceComparison != 0
						? 0
						: ClassName.Compare(
							SelectedClassName, ESearchCase::CaseSensitive);
				if (SelectedPendingIndex == INDEX_NONE
					|| NamespaceComparison < 0
					|| (NamespaceComparison == 0 && NameComparison < 0))
				{
					SelectedPendingIndex = PendingIndex;
					SelectedBaseCaptureIndex = BaseCaptureIndex;
					SelectedClassNamespace = ClassNamespace;
					SelectedClassName = ClassName;
				}
				else if (NamespaceComparison == 0 && NameComparison == 0)
				{
					return Failure(EAngelscriptCacheCleanCaptureError::NotCacheable,
						TEXT("The class graph contains duplicate canonical class authority"));
				}
			}
			if (SelectedPendingIndex == INDEX_NONE)
			{
				return Failure(EAngelscriptCacheCleanCaptureError::NotCacheable,
					TEXT("The same-module class graph is cyclic or references an uncaptured script base"));
			}

			const FAngelscriptClassDesc* ClassDesc =
				PendingClasses[SelectedPendingIndex];
			asCObjectType* ScriptType =
				static_cast<asCObjectType*>(ClassDesc->ScriptType);
			if (ClassDesc->bIsStruct || ClassDesc->bIsStaticsClass
				|| ClassDesc->CodeSuperClass == nullptr
				|| ScriptType->shadowType == nullptr
				|| !ClassDesc->ImplementedInterfaces.IsEmpty()
				|| !ClassDesc->ComposeOntoClass.IsEmpty()
				|| ScriptType->size <= 0 || ScriptType->alignment <= 0
				|| ScriptType->basePropertyOffset < 0
				|| ClassDesc->StaticClassGlobalVariableName.IsEmpty()
				|| (SelectedBaseCaptureIndex == INDEX_NONE
					? !ClassDesc->bSuperIsCodeClass
					: ClassDesc->bSuperIsCodeClass))
			{
				return Failure(EAngelscriptCacheCleanCaptureError::NotCacheable,
					TEXT("A class is outside the same-module reflected UClass graph capture shape"));
			}

			FClassCapture& Capture = Classes.AddDefaulted_GetRef();
			Capture.ClassDesc = ClassDesc;
			Capture.ScriptType = ScriptType;
			Capture.BaseCaptureIndex = SelectedBaseCaptureIndex;
			Capture.ClassNamespace = MoveTemp(SelectedClassNamespace);
			Capture.ClassName = MoveTemp(SelectedClassName);
			if (!Capture.ClassName.Equals(
				ClassDesc->ClassName, ESearchCase::CaseSensitive))
			{
				return Failure(EAngelscriptCacheCleanCaptureError::NotCacheable,
					TEXT("A class descriptor and compiled VM type name disagree"));
			}
			PendingClasses.RemoveAt(SelectedPendingIndex);
		}

		uint32 NextDeclarationSlot = 0;
		for (FClassCapture& Class : Classes)
		{
			FAngelscriptCachedDeclaration& Declaration = Class.TypeDeclaration;
			Declaration.DeclarationKind = EAngelscriptCacheDeclarationKind::Type;
			Declaration.EntityKind = EAngelscriptArtifactEntityKind::Class;
			Declaration.SchemaCoverage = EAngelscriptCacheSchemaCoverage::Required;
			Declaration.BodyCoverage = EAngelscriptCacheBodyCoverage::Forbidden;
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
			FAngelscriptTypeIdentityDescriptor Identity;
			Identity.ModuleKey = ModuleKey;
			Identity.Namespace = Class.ClassNamespace;
			Identity.Kind = Declaration.EntityKind;
			Identity.CanonicalDeclaration = Declaration.CanonicalDeclaration;
			Declaration.StableKey =
				FAngelscriptArtifactIdentityBuilder::BuildTypeKey(Identity).Hash;
			const FAngelscriptCacheValidationResult HashResult =
				FAngelscriptCacheSemanticArchive::ComputeDeclarationHashes(
					Declaration, Declaration.SignatureHash, Declaration.TraitsHash);
			if (!HashResult.IsSuccess())
			{
				return ValidationFailure(
					EAngelscriptCacheCleanCaptureError::RecordEncodingFailed,
					TEXT("Class-graph type declaration hashes"), HashResult);
			}
		}

		TArray<FClassGraphTypeAuthority> TypeAuthorities;
		TypeAuthorities.Reserve(Classes.Num());
		for (const FClassCapture& Class : Classes)
		{
			TypeAuthorities.Add({Class.ScriptType, &Class.TypeDeclaration});
		}

		for (FClassCapture& Class : Classes)
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
			Schema.CanonicalDeclaration = Class.TypeDeclaration.CanonicalDeclaration;
			Schema.TypeSemanticFlags = static_cast<uint32>(
				EAngelscriptCachedTypeSemanticFlags::ReferenceType);
			if (Class.ClassDesc->bAbstract)
			{
				Schema.TypeSemanticFlags |= static_cast<uint32>(
					EAngelscriptCachedTypeSemanticFlags::Abstract);
			}
			Schema.Metadata = Class.TypeDeclaration.Metadata;
			Schema.Layout.SemanticSize = static_cast<uint64>(Class.ScriptType->size);
			Schema.Layout.SemanticAlignment =
				static_cast<uint32>(Class.ScriptType->alignment);
			Schema.Layout.BasePropertyBoundary = Class.BaseCaptureIndex != INDEX_NONE
				? static_cast<uint32>(
					Classes[Class.BaseCaptureIndex].ScriptType->size)
				: static_cast<uint32>(Class.ScriptType->basePropertyOffset);
			Schema.Reflection.ReflectionKind = EAngelscriptCachedReflectionKind::UClass;
			Schema.Reflection.ClassReflectionFlags =
				BuildClassReflectionFlags(*Class.ClassDesc);
			if (!Class.ClassDesc->ConfigName.IsEmpty())
			{
				Schema.Reflection.ConfigName = Class.ClassDesc->ConfigName;
			}
			Schema.Reflection.StaticClassGlobalName =
				Class.ClassDesc->StaticClassGlobalVariableName;

			if (Class.BaseCaptureIndex != INDEX_NONE)
			{
				const FClassCapture& Base = Classes[Class.BaseCaptureIndex];
				const FAngelscriptCacheStableReference BaseReference{
					EAngelscriptCacheReferenceKind::ScriptType,
					Base.TypeDeclaration.StableKey,
					Base.TypeDeclaration.SignatureHash};
				Schema.Relations.Add({
					EAngelscriptCachedTypeRelationKind::Base, {}, BaseReference});
				FAngelscriptCachedTypeLayoutInput BaseInput;
				BaseInput.InputKind = EAngelscriptCachedTypeLayoutInputKind::BaseType;
				BaseInput.Target = BaseReference;
				BaseInput.BoundaryContribution =
					static_cast<uint32>(Base.ScriptType->size);
				BaseInput.AlignmentContribution =
					static_cast<uint32>(Base.ScriptType->alignment);
				FAngelscriptCacheValidationResult HashResult =
					FAngelscriptCacheTypeSchemaArchive::ComputeLayoutInputHash(
						BaseInput, BaseInput.LayoutInputHash);
				if (!HashResult.IsSuccess())
				{
					return ValidationFailure(
						EAngelscriptCacheCleanCaptureError::RecordEncodingFailed,
						TEXT("Class-graph base layout input"), HashResult);
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
			CodeInput.InputKind = EAngelscriptCachedTypeLayoutInputKind::CodeRoot;
			CodeInput.Target = CodeRoot;
			if (Class.BaseCaptureIndex == INDEX_NONE)
			{
				CodeInput.BoundaryContribution = static_cast<uint32>(
					Class.ClassDesc->CodeSuperClass->GetPropertiesSize());
			}
			CodeInput.AlignmentContribution = static_cast<uint32>(
				static_cast<const asCObjectType*>(Class.ScriptType->shadowType)->alignment);
			FAngelscriptCacheValidationResult HashResult =
				FAngelscriptCacheTypeSchemaArchive::ComputeLayoutInputHash(
					CodeInput, CodeInput.LayoutInputHash);
			if (!HashResult.IsSuccess())
			{
				return ValidationFailure(
					EAngelscriptCacheCleanCaptureError::RecordEncodingFailed,
					TEXT("Class-graph code-root layout input"), HashResult);
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
						TEXT("A class-graph type contains an unsupported local VM property"));
				}
				FAngelscriptCachedDataType CachedType;
				FString CanonicalType;
				FAngelscriptCacheSemanticDependency PropertyDependency;
				bool bHasPropertyDependency = false;
				if (!TryMapPrimitiveDataType(
					ScriptProperty->type, CachedType, CanonicalType))
				{
					bHasPropertyDependency = TryMapClassGraphPropertyDataType(
						ScriptProperty->type, TypeAuthorities,
						CachedType, CanonicalType, PropertyDependency);
					if (!bHasPropertyDependency)
					{
						bHasPropertyDependency = TryMapEnvironmentPropertyDataType(
							ScriptProperty->type, CachedType, CanonicalType,
							PropertyDependency);
					}
					if (!bHasPropertyDependency)
					{
						return Failure(EAngelscriptCacheCleanCaptureError::NotCacheable,
							TEXT("A class-graph property is outside the stable primitive/script-handle/environment value table"));
					}
				}
				const FString PropertyName =
					UTF8_TO_TCHAR(ScriptProperty->name.AddressOf());
				const FAngelscriptPropertyDesc* ReflectedProperty = nullptr;
				for (const TSharedRef<FAngelscriptPropertyDesc>& Candidate
					: Class.ClassDesc->Properties)
				{
					if (Candidate->ScriptPropertyIndex == static_cast<int32>(PropertyIndex)
						|| Candidate->PropertyName.Equals(
							PropertyName, ESearchCase::CaseSensitive))
					{
						if (ReflectedProperty != nullptr)
						{
							return Failure(EAngelscriptCacheCleanCaptureError::NotCacheable,
								TEXT("More than one reflected property maps to one class-graph VM property"));
						}
						ReflectedProperty = &Candidate.Get();
						MatchedProperties.Add(ReflectedProperty);
					}
				}

				FAngelscriptCachedDeclaration PropertyDeclaration;
				PropertyDeclaration.DeclarationKind =
					EAngelscriptCacheDeclarationKind::Property;
				PropertyDeclaration.EntityKind = EAngelscriptArtifactEntityKind::Property;
				PropertyDeclaration.SchemaCoverage = EAngelscriptCacheSchemaCoverage::Forbidden;
				PropertyDeclaration.BodyCoverage = EAngelscriptCacheBodyCoverage::Forbidden;
				PropertyDeclaration.OwnerKind = EAngelscriptFunctionOwnerKind::Type;
				PropertyDeclaration.OwnerKey = Class.TypeDeclaration.StableKey;
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
					AppendMetadata(ReflectedProperty->Meta, PropertyDeclaration.Metadata);
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
				HashResult = FAngelscriptCacheSemanticArchive::ComputeDeclarationHashes(
					PropertyDeclaration, PropertyDeclaration.SignatureHash,
					PropertyDeclaration.TraitsHash);
				if (!HashResult.IsSuccess())
				{
					return ValidationFailure(
						EAngelscriptCacheCleanCaptureError::RecordEncodingFailed,
						TEXT("Class-graph property declaration hashes"), HashResult);
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
				HashResult = FAngelscriptCacheTypeSchemaArchive::ComputeStorageLayoutHash(
					PropertySchema.Type, PropertySchema.StorageKind,
					PropertySchema.SemanticStorageSize,
					PropertySchema.SemanticStorageAlignment,
					PropertySchema.StorageLayoutHash);
				if (HashResult.IsSuccess())
				{
					HashResult = FAngelscriptCacheTypeSchemaArchive::
						ComputePropertyLayoutFingerprint(
							Schema.TypeKey, PropertySchema,
							PropertySchema.PropertyLayoutFingerprint);
				}
				if (!HashResult.IsSuccess())
				{
					return ValidationFailure(
						EAngelscriptCacheCleanCaptureError::RecordEncodingFailed,
						TEXT("Class-graph property layout hashes"), HashResult);
				}
				Class.PropertyDeclarations.Add(MoveTemp(PropertyDeclaration));
				Schema.OrderedProperties.Add(MoveTemp(PropertySchema));
				if (bHasPropertyDependency)
				{
					Schema.Dependencies.AddUnique(MoveTemp(PropertyDependency));
				}
			}
			if (MatchedProperties.Num() != Class.ClassDesc->Properties.Num())
			{
				return Failure(EAngelscriptCacheCleanCaptureError::NotCacheable,
					TEXT("A reflected class-graph property has no matching local VM property"));
			}
		}

		FAngelscriptCachedModuleState ModuleState;
		ModuleState.PayloadSchemaVersion =
			FAngelscriptCacheRemainingRecordArchive::ModuleStatePayloadSchemaVersion;
		ModuleState.ModuleKey = ModuleKey;
		ModuleState.Profile = Options.Profile;
		FAngelscriptCacheValidationResult IdentityResult =
			FAngelscriptCacheRemainingRecordArchive::ComputeModuleStateInputHash(
				ModuleState, ModuleState.StateInputHash);
		if (!IdentityResult.IsSuccess())
		{
			return ValidationFailure(
				EAngelscriptCacheCleanCaptureError::RecordEncodingFailed,
				TEXT("Class-graph module state input hash"), IdentityResult);
		}

		for (asUINT GlobalIndex = 0;
			GlobalIndex < ScriptModule->scriptGlobalsList.GetLength(); ++GlobalIndex)
		{
			asCGlobalProperty* Global = ScriptModule->scriptGlobalsList[GlobalIndex];
			if (Global == nullptr)
			{
				return Failure(EAngelscriptCacheCleanCaptureError::NotCacheable,
					TEXT("The class-graph VM global table contains a null entry"));
			}
			const FString GlobalName = UTF8_TO_TCHAR(Global->name.AddressOf());
			FClassCapture* Owner = Classes.FindByPredicate(
				[&GlobalName](const FClassCapture& Candidate)
				{
					return Candidate.ClassDesc->StaticClassGlobalVariableName.Equals(
						GlobalName, ESearchCase::CaseSensitive);
				});
			if (Owner == nullptr || Owner->GeneratedStaticClassGlobal != nullptr)
			{
				return Failure(EAngelscriptCacheCleanCaptureError::NotCacheable,
					TEXT("The class-graph vertical admits only one generated StaticClass global per type"));
			}
			Owner->GeneratedStaticClassGlobal = Global;
		}
		for (const FClassCapture& Class : Classes)
		{
			if (Class.GeneratedStaticClassGlobal == nullptr)
			{
				return Failure(EAngelscriptCacheCleanCaptureError::NotCacheable,
					TEXT("A class-graph type is missing its generated StaticClass global"));
			}
		}

		struct FClassFunctionCapture
		{
			asCScriptFunction* Function = nullptr;
			int32 OwnerClassIndex = INDEX_NONE;
			const FAngelscriptFunctionDesc* ReflectedFunction = nullptr;
			FAngelscriptCachedDeclaration Declaration;
			EAngelscriptCachedFunctionInvocationKind InvocationKind =
				EAngelscriptCachedFunctionInvocationKind::Invalid;
			FAngelscriptCachedFunctionBody Body;
			FAngelscriptCachedDebugSidecar Debug;
			TArray<uint8> RawVmPayload;
			FAngelscriptPreparedRecord BodyRecord;
			FAngelscriptPreparedRecord DebugRecord;
		};
		TArray<FClassFunctionCapture> FunctionCaptures;
		TArray<TSet<const FAngelscriptFunctionDesc*>> MatchedFunctionDescs;
		MatchedFunctionDescs.SetNum(Classes.Num());
		for (asUINT FunctionIndex = 0;
			FunctionIndex < ScriptModule->scriptFunctions.GetLength(); ++FunctionIndex)
		{
			asCScriptFunction* Function = ScriptModule->scriptFunctions[FunctionIndex];
			if (Function == nullptr || Function->module != ScriptModule)
			{
				return Failure(EAngelscriptCacheCleanCaptureError::NotCacheable,
					TEXT("The class-graph VM function table contains a null or foreign entry"));
			}
			const FString FunctionName = UTF8_TO_TCHAR(Function->GetName());
			const FString CanonicalDeclaration = UTF8_TO_TCHAR(
				Function->GetDeclaration(false, false, false));
			if (FunctionName == TEXT("StaticClass")
				&& CanonicalDeclaration == TEXT("UClass StaticClass()")
				&& Function->GetObjectType() == nullptr
				&& Function->traits.GetTrait(asTRAIT_GENERATED_FUNCTION))
			{
				const FString FunctionNamespace = UTF8_TO_TCHAR(Function->GetNamespace());
				FClassCapture* Owner = Classes.FindByPredicate(
					[&FunctionNamespace](const FClassCapture& Candidate)
					{
						const FString ExpectedNamespace =
							Candidate.ClassNamespace.IsEmpty()
								? Candidate.ClassName
								: Candidate.ClassNamespace + TEXT("::")
									+ Candidate.ClassName;
						return ExpectedNamespace.Equals(
							FunctionNamespace, ESearchCase::CaseSensitive);
					});
				if (Owner == nullptr || Owner->GeneratedStaticClassFunction != nullptr)
				{
					return Failure(EAngelscriptCacheCleanCaptureError::NotCacheable,
						FString::Printf(TEXT("Generated StaticClass function namespace %s has no unique class owner"), *FunctionNamespace));
				}
				Owner->GeneratedStaticClassFunction = Function;
				continue;
			}

			asCObjectType* OwnerType = Function->artifactOwnerType != nullptr
				? static_cast<asCObjectType*>(Function->artifactOwnerType)
				: Function->objectType;
			const int32 OwnerClassIndex = Classes.IndexOfByPredicate(
				[OwnerType](const FClassCapture& Candidate)
				{
					return Candidate.ScriptType == OwnerType;
				});
			const TOptional<EAngelscriptArtifactEntityKind> EntityKind =
				MapFunctionEntityKind(Function->artifactInvocationKind);
			const TOptional<EAngelscriptCachedFunctionInvocationKind> InvocationKind =
				MapCachedFunctionInvocationKind(Function->artifactInvocationKind);
			if (OwnerClassIndex == INDEX_NONE
				|| Function->GetFuncType() != asFUNC_SCRIPT
				|| Function->scriptData == nullptr
				|| !EntityKind.IsSet() || !InvocationKind.IsSet()
				|| Function->scriptData->artifactCanonicalSource.GetLength() == 0)
			{
				return Failure(EAngelscriptCacheCleanCaptureError::NotCacheable,
					FString::Printf(TEXT("Class-graph function %s has no complete stable type owner/invocation/source authority"), *CanonicalDeclaration));
			}

			FAngelscriptStableFunctionKey StableFunctionKey;
			FString StableKeyFailure;
			if (!FAngelscriptCacheStableSymbolIdentity::TryBuildFunctionKey(
				ModuleKey, *Function, StableFunctionKey, &StableKeyFailure))
			{
				return Failure(EAngelscriptCacheCleanCaptureError::NotCacheable,
					FString::Printf(TEXT("Class-graph function %s has no stable FunctionKey: %s"),
						*CanonicalDeclaration, *StableKeyFailure));
			}

			FClassFunctionCapture& Capture =
				FunctionCaptures.AddDefaulted_GetRef();
			Capture.Function = Function;
			Capture.OwnerClassIndex = OwnerClassIndex;
			Capture.InvocationKind = InvocationKind.GetValue();
			for (const TSharedRef<FAngelscriptFunctionDesc>& Candidate
				: Classes[OwnerClassIndex].ClassDesc->Methods)
			{
				if (Candidate->ScriptFunction == Function)
				{
					if (Capture.ReflectedFunction != nullptr)
					{
						return Failure(EAngelscriptCacheCleanCaptureError::NotCacheable,
							TEXT("More than one reflected method maps to one class-graph VM function"));
					}
					Capture.ReflectedFunction = &Candidate.Get();
					MatchedFunctionDescs[OwnerClassIndex].Add(Capture.ReflectedFunction);
				}
			}

			FAngelscriptCachedDeclaration& Declaration = Capture.Declaration;
			Declaration.DeclarationKind = EAngelscriptCacheDeclarationKind::Function;
			Declaration.EntityKind = EntityKind.GetValue();
			Declaration.SchemaCoverage = EAngelscriptCacheSchemaCoverage::Forbidden;
			Declaration.BodyCoverage = EAngelscriptCacheBodyCoverage::Required;
			Declaration.OwnerKind = EAngelscriptFunctionOwnerKind::Type;
			Declaration.OwnerKey = Classes[OwnerClassIndex].TypeDeclaration.StableKey;
			Declaration.ModuleKey = ModuleKey;
			Declaration.CanonicalNamespace = UTF8_TO_TCHAR(Function->GetNamespace());
			Declaration.CanonicalName = FunctionName;
			Declaration.CanonicalDeclaration = CanonicalDeclaration;
			Declaration.StableKey = StableFunctionKey.Hash;
			Declaration.TraitFlags = BuildFunctionDeclarationTraitFlags(*Function)
				| BuildFunctionDescriptorTraitFlags(Capture.ReflectedFunction);
			Declaration.ReflectionFlags =
				BuildFunctionReflectionFlags(Capture.ReflectedFunction);
			if (Capture.ReflectedFunction != nullptr)
			{
				AppendMetadata(Capture.ReflectedFunction->Meta, Declaration.Metadata);
			}
			Declaration.Slots.Add({
				EAngelscriptCacheDeclarationSlotKind::Function,
				0});
			FAngelscriptCachedDataType ReturnType;
			if (!TryMapClassGraphFunctionDataType(
				Function->returnType, TypeAuthorities, ReturnType))
			{
				return Failure(EAngelscriptCacheCleanCaptureError::NotCacheable,
					FString::Printf(TEXT("Function %s return type is outside the class-graph stable type table"), *CanonicalDeclaration));
			}
			Declaration.DeclaredType = MoveTemp(ReturnType);
			for (asUINT ParameterIndex = 0;
				ParameterIndex < Function->parameterTypes.GetLength(); ++ParameterIndex)
			{
				FAngelscriptCachedParameter& Parameter =
					Declaration.OrderedParameters.AddDefaulted_GetRef();
				Parameter.Ordinal = ParameterIndex;
				Parameter.CanonicalName =
					ParameterIndex < Function->parameterNames.GetLength()
						&& Function->parameterNames[ParameterIndex].GetLength() != 0
						? UTF8_TO_TCHAR(Function->parameterNames[ParameterIndex].AddressOf())
						: FString::Printf(TEXT("arg%u"), ParameterIndex);
				if (!TryMapClassGraphFunctionDataType(
					Function->parameterTypes[ParameterIndex],
					TypeAuthorities, Parameter.Type))
				{
					return Failure(EAngelscriptCacheCleanCaptureError::NotCacheable,
						FString::Printf(TEXT("Function %s parameter %u is outside the class-graph stable type table"),
							*CanonicalDeclaration, ParameterIndex));
				}
				const asETypeModifiers Passing =
					ParameterIndex < Function->inOutFlags.GetLength()
						? static_cast<asETypeModifiers>(
							Function->inOutFlags[ParameterIndex] & asTM_INOUTREF)
						: asTM_NONE;
				switch (Passing)
				{
				case asTM_NONE: Parameter.Passing = EAngelscriptCachedParameterPassing::Value; break;
				case asTM_INREF: Parameter.Passing = EAngelscriptCachedParameterPassing::InReference; break;
				case asTM_OUTREF: Parameter.Passing = EAngelscriptCachedParameterPassing::OutReference; break;
				case asTM_INOUTREF: Parameter.Passing = EAngelscriptCachedParameterPassing::InOutReference; break;
				default:
					return Failure(EAngelscriptCacheCleanCaptureError::NotCacheable,
						TEXT("A class-graph function parameter has an unknown passing mode"));
				}
				if (ParameterIndex < Function->defaultArgs.GetLength()
					&& Function->defaultArgs[ParameterIndex] != nullptr)
				{
					Parameter.CanonicalDefaultExpression = UTF8_TO_TCHAR(
						Function->defaultArgs[ParameterIndex]->AddressOf());
				}
				if (Capture.ReflectedFunction != nullptr)
				{
					if (ParameterIndex >= static_cast<asUINT>(
						Capture.ReflectedFunction->Arguments.Num()))
					{
						return Failure(EAngelscriptCacheCleanCaptureError::NotCacheable,
							TEXT("A reflected class-graph function has fewer parameters than its VM function"));
					}
					const FAngelscriptArgumentDesc& Argument =
						Capture.ReflectedFunction->Arguments[ParameterIndex];
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
			if (Capture.ReflectedFunction != nullptr
				&& Capture.ReflectedFunction->Arguments.Num()
					!= static_cast<int32>(Function->parameterTypes.GetLength()))
			{
				return Failure(EAngelscriptCacheCleanCaptureError::NotCacheable,
					TEXT("A reflected class-graph function has more parameters than its VM function"));
			}
			IdentityResult = FAngelscriptCacheSemanticArchive::ComputeDeclarationHashes(
				Declaration, Declaration.SignatureHash, Declaration.TraitsHash);
			if (!IdentityResult.IsSuccess())
			{
				return ValidationFailure(
					EAngelscriptCacheCleanCaptureError::RecordEncodingFailed,
					TEXT("Class-graph function declaration hashes"), IdentityResult);
			}
		}

		for (int32 ClassIndex = 0; ClassIndex < Classes.Num(); ++ClassIndex)
		{
			FClassCapture& Class = Classes[ClassIndex];
			if (Class.GeneratedStaticClassFunction == nullptr
				|| MatchedFunctionDescs[ClassIndex].Num()
					!= Class.ClassDesc->Methods.Num())
			{
				return Failure(EAngelscriptCacheCleanCaptureError::NotCacheable,
					TEXT("A class-graph StaticClass helper or reflected function descriptor could not be classified exactly"));
			}
		}
		FunctionCaptures.StableSort([](
			const FClassFunctionCapture& Left,
			const FClassFunctionCapture& Right)
		{
			return Left.OwnerClassIndex < Right.OwnerClassIndex;
		});
		for (int32 FunctionSlot = 0;
			FunctionSlot < FunctionCaptures.Num(); ++FunctionSlot)
		{
			FunctionCaptures[FunctionSlot].Declaration.Slots[0].Ordinal =
				static_cast<uint32>(FunctionSlot);
		}

		auto FindFunctionCapture = [&FunctionCaptures](const asCScriptFunction* Function)
			-> FClassFunctionCapture*
		{
			return FunctionCaptures.FindByPredicate(
				[Function](const FClassFunctionCapture& Candidate)
				{
					return Candidate.Function == Function;
				});
		};
		auto MakeFunctionReference = [](
			const FClassFunctionCapture& Capture)
		{
			return FAngelscriptCacheStableReference{
				EAngelscriptCacheReferenceKind::ScriptFunction,
				Capture.Declaration.StableKey,
				Capture.Declaration.SignatureHash};
		};

		for (int32 ClassIndex = 0; ClassIndex < Classes.Num(); ++ClassIndex)
		{
			FClassCapture& Class = Classes[ClassIndex];
			FAngelscriptCachedTypeSchema& Schema = Class.TypeSchema;
			auto AddDeclarationDependency = [&Schema](
				const FAngelscriptCacheStableReference& Target)
			{
				FAngelscriptCacheSemanticDependency Dependency;
				Dependency.Kind = EAngelscriptCacheSemanticDependencyKind::Declaration;
				Dependency.Target = Target;
				Schema.Dependencies.AddUnique(MoveTemp(Dependency));
			};

			for (asUINT MethodIndex = 0;
				MethodIndex < Class.ScriptType->methods.GetLength(); ++MethodIndex)
			{
				asCScriptFunction* Function = ScriptModule->engine->GetScriptFunction(
					Class.ScriptType->methods[MethodIndex]);
				FClassFunctionCapture* FunctionCapture = FindFunctionCapture(Function);
				if (FunctionCapture == nullptr)
				{
					return Failure(EAngelscriptCacheCleanCaptureError::NotCacheable,
						TEXT("A class-graph method has no stable local-module function"));
				}
				FAngelscriptCachedMethodEntry& Method =
					Schema.OrderedMethods.AddDefaulted_GetRef();
				Method.EntryKind = FunctionCapture->OwnerClassIndex == ClassIndex
					? EAngelscriptCachedMethodSlotKind::LocalMethod
					: EAngelscriptCachedMethodSlotKind::Inherited;
				Method.MethodOrdinal = MethodIndex;
				Method.FunctionKey = FAngelscriptStableFunctionKey{
					FunctionCapture->Declaration.StableKey};
				Method.DeclaringOwner = Classes[
					FunctionCapture->OwnerClassIndex].TypeSchema.TypeKey;
				Method.ExpectedDeclarationAbi = FunctionCapture->Declaration.SignatureHash;
				AddDeclarationDependency(MakeFunctionReference(*FunctionCapture));
			}

			for (asUINT VftIndex = 0;
				VftIndex < Class.ScriptType->virtualFunctionTable.GetLength(); ++VftIndex)
			{
				asCScriptFunction* Function =
					Class.ScriptType->virtualFunctionTable[VftIndex];
				FClassFunctionCapture* FunctionCapture = FindFunctionCapture(Function);
				if (FunctionCapture == nullptr
					|| Function->vfTableIdx != static_cast<int>(VftIndex))
				{
					return Failure(EAngelscriptCacheCleanCaptureError::NotCacheable,
						TEXT("A class-graph VFT slot has no stable or correctly indexed function"));
				}
				FAngelscriptCachedVirtualFunctionSlot& Slot =
					Schema.VirtualFunctionTable.AddDefaulted_GetRef();
				Slot.VftOrdinal = VftIndex;
				Slot.FunctionKey = FAngelscriptStableFunctionKey{
					FunctionCapture->Declaration.StableKey};
				Slot.ImplementingOwner = Classes[
					FunctionCapture->OwnerClassIndex].TypeSchema.TypeKey;
				Slot.ExpectedDeclarationAbi = FunctionCapture->Declaration.SignatureHash;
				if (FunctionCapture->OwnerClassIndex != ClassIndex)
				{
					Slot.SlotKind = EAngelscriptCachedMethodSlotKind::Inherited;
					Slot.DeclaringOwner = Slot.ImplementingOwner;
				}
				else if (Class.BaseCaptureIndex != INDEX_NONE
					&& VftIndex < Classes[Class.BaseCaptureIndex].ScriptType->
						virtualFunctionTable.GetLength())
				{
					const FAngelscriptCachedTypeSchema& BaseSchema =
						Classes[Class.BaseCaptureIndex].TypeSchema;
					if (!BaseSchema.VirtualFunctionTable.IsValidIndex(
							static_cast<int32>(VftIndex)))
					{
						return Failure(EAngelscriptCacheCleanCaptureError::NotCacheable,
							TEXT("A class-graph override has no cached base VFT slot"));
					}
					FClassFunctionCapture* BaseFunction = FindFunctionCapture(
						Classes[Class.BaseCaptureIndex].ScriptType->
							virtualFunctionTable[VftIndex]);
					if (BaseFunction == nullptr)
					{
						return Failure(EAngelscriptCacheCleanCaptureError::NotCacheable,
							TEXT("A class-graph override has no stable base declaration"));
					}
					Slot.SlotKind = EAngelscriptCachedMethodSlotKind::VirtualOverride;
					// The immediate base function identifies the implementation being
					// replaced, not necessarily the class that introduced this virtual
					// family. Carry the declaration owner through every override level.
					Slot.DeclaringOwner =
						BaseSchema.VirtualFunctionTable[VftIndex].DeclaringOwner;
				}
				else
				{
					Slot.SlotKind = EAngelscriptCachedMethodSlotKind::VirtualDeclaration;
					Slot.DeclaringOwner = Schema.TypeKey;
				}
				AddDeclarationDependency(MakeFunctionReference(*FunctionCapture));
			}

			for (int32 ReflectionIndex = 0;
				ReflectionIndex < Class.ClassDesc->Methods.Num(); ++ReflectionIndex)
			{
				const FAngelscriptFunctionDesc& Reflected =
					Class.ClassDesc->Methods[ReflectionIndex].Get();
				FClassFunctionCapture* FunctionCapture = FindFunctionCapture(
					static_cast<asCScriptFunction*>(Reflected.ScriptFunction));
				if (FunctionCapture == nullptr
					|| FunctionCapture->OwnerClassIndex != ClassIndex
					|| FunctionCapture->ReflectedFunction != &Reflected)
				{
					return Failure(EAngelscriptCacheCleanCaptureError::NotCacheable,
						TEXT("A reflected class-graph UFunction is not a local cacheable method"));
				}
				FAngelscriptCachedReflectedFunctionMember& Member =
					Schema.Reflection.OrderedUFunctionMembers.AddDefaulted_GetRef();
				Member.ReflectionOrdinal = static_cast<uint32>(ReflectionIndex);
				Member.CanonicalFunctionName = Reflected.FunctionName;
				Member.CanonicalOriginalFunctionName =
					Reflected.OriginalFunctionName;
				Member.CanonicalScriptFunctionName =
					Reflected.ScriptFunctionName;
				Member.Target = MakeFunctionReference(*FunctionCapture);
				AddDeclarationDependency(Member.Target);
			}

			auto AddBehavior = [&Schema, &FindFunctionCapture,
				&MakeFunctionReference, &AddDeclarationDependency, ScriptModule,
				&Classes](const EAngelscriptCachedBehaviorKind Kind,
					const uint32 Ordinal, const int FunctionId) -> bool
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
				if (FClassFunctionCapture* FunctionCapture =
					FindFunctionCapture(Function))
				{
					Slot.Target = MakeFunctionReference(*FunctionCapture);
					Slot.DeclaringOwner = Classes[
						FunctionCapture->OwnerClassIndex].TypeSchema.TypeKey;
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
				Dependency.Kind = EAngelscriptCacheSemanticDependencyKind::EnvironmentAbi;
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
						TEXT("A class-graph constructor has no stable function reference"));
				}
			}
			if (!AddBehavior(EAngelscriptCachedBehaviorKind::Destruct,
				0, Class.ScriptType->beh.destruct))
			{
				return Failure(EAngelscriptCacheCleanCaptureError::NotCacheable,
					TEXT("A class-graph destructor has no stable function reference"));
			}
			for (asUINT Index = 0;
				Index < Class.ScriptType->beh.factories.GetLength(); ++Index)
			{
				if (!AddBehavior(EAngelscriptCachedBehaviorKind::Factory,
					Index, Class.ScriptType->beh.factories[Index]))
				{
					return Failure(EAngelscriptCacheCleanCaptureError::NotCacheable,
						TEXT("A class-graph factory has no stable function reference"));
				}
			}
			const struct
			{
				EAngelscriptCachedBehaviorKind Kind;
				int FunctionId;
			} SingletonBehaviors[] = {
				{EAngelscriptCachedBehaviorKind::ListFactory, Class.ScriptType->beh.listFactory},
				{EAngelscriptCachedBehaviorKind::AddRef, Class.ScriptType->beh.addref},
				{EAngelscriptCachedBehaviorKind::Release, Class.ScriptType->beh.release},
				{EAngelscriptCachedBehaviorKind::GetWeakRefFlag, Class.ScriptType->beh.getWeakRefFlag},
				{EAngelscriptCachedBehaviorKind::TemplateCallback, Class.ScriptType->beh.templateCallback},
				{EAngelscriptCachedBehaviorKind::GetRefCount, Class.ScriptType->beh.gcGetRefCount},
				{EAngelscriptCachedBehaviorKind::SetGcFlag, Class.ScriptType->beh.gcSetFlag},
				{EAngelscriptCachedBehaviorKind::GetGcFlag, Class.ScriptType->beh.gcGetFlag},
				{EAngelscriptCachedBehaviorKind::EnumRefs, Class.ScriptType->beh.gcEnumReferences},
				{EAngelscriptCachedBehaviorKind::ReleaseRefs, Class.ScriptType->beh.gcReleaseAllReferences},
				{EAngelscriptCachedBehaviorKind::Copy, Class.ScriptType->beh.copy},
				{EAngelscriptCachedBehaviorKind::CopyConstruct, Class.ScriptType->beh.copyconstruct},
				{EAngelscriptCachedBehaviorKind::CopyFactory, Class.ScriptType->beh.copyfactory},
			};
			for (const auto& Behavior : SingletonBehaviors)
			{
				if (!AddBehavior(Behavior.Kind, 0, Behavior.FunctionId))
				{
					return Failure(EAngelscriptCacheCleanCaptureError::NotCacheable,
						TEXT("A class-graph singleton behavior has no stable function reference"));
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
			IdentityResult = FAngelscriptCacheTypeSchemaArchive::ComputeTypeLayoutHash(
				Schema, Schema.Layout.TypeLayoutHash);
			if (!IdentityResult.IsSuccess())
			{
				return ValidationFailure(
					EAngelscriptCacheCleanCaptureError::RecordEncodingFailed,
					TEXT("Class-graph type layout hash"), IdentityResult);
			}
		}

		FAngelscriptCachedModuleInterface ModuleInterface;
		ModuleInterface.PayloadSchemaVersion =
			FAngelscriptCacheSemanticArchive::ModuleInterfacePayloadSchemaVersion;
		ModuleInterface.ModuleKey = ModuleKey;
		ModuleInterface.CanonicalModuleName = Module->ModuleName;
		for (const FClassCapture& Class : Classes)
		{
			if (!Class.ClassNamespace.IsEmpty())
			{
				ModuleInterface.CanonicalNamespaces.AddUnique(Class.ClassNamespace);
			}
			ModuleInterface.Declarations.Add(Class.TypeDeclaration);
			ModuleInterface.Declarations.Append(Class.PropertyDeclarations);
		}
		for (const FClassFunctionCapture& Capture : FunctionCaptures)
		{
			if (!Capture.Declaration.CanonicalNamespace.IsEmpty())
			{
				ModuleInterface.CanonicalNamespaces.AddUnique(
					Capture.Declaration.CanonicalNamespace);
			}
			ModuleInterface.Declarations.Add(Capture.Declaration);
		}
		IdentityResult = FAngelscriptCacheSemanticArchive::ComputeModuleInterfaceAbi(
			ModuleInterface, ModuleInterface.InterfaceAbi);
		if (!IdentityResult.IsSuccess())
		{
			return ValidationFailure(
				EAngelscriptCacheCleanCaptureError::RecordEncodingFailed,
				TEXT("Class-graph module interface ABI"), IdentityResult);
		}

		FAngelscriptPreparedRecord SourceRecord;
		FAngelscriptPreparedRecord InterfaceRecord;
		FAngelscriptPreparedRecord StateRecord;
		FAngelscriptPreparedRecord SnapshotRecord;
		FAngelscriptCacheCleanCaptureResult EncodingError;
		if (!TrySerializeRecord(EAngelscriptCacheRecordKind::SourceIndex,
			SourceIndex, FAngelscriptCacheSemanticArchive::SerializeSourceIndex,
			SourceRecord, EncodingError)
			|| !TrySerializeRecord(EAngelscriptCacheRecordKind::ModuleInterface,
				ModuleInterface,
				FAngelscriptCacheSemanticArchive::SerializeModuleInterface,
				InterfaceRecord, EncodingError)
			|| !TrySerializeRecord(EAngelscriptCacheRecordKind::ModuleState,
				ModuleState,
				FAngelscriptCacheRemainingRecordArchive::SerializeModuleState,
				StateRecord, EncodingError))
		{
			return EncodingError;
		}
		for (FClassCapture& Class : Classes)
		{
			if (!TrySerializeTypeSchemaRecord(
				Class.TypeSchema, Class.TypeRecord, EncodingError))
			{
				FString PropertyLayouts;
				for (const FAngelscriptCachedPropertySchema& Property
					: Class.TypeSchema.OrderedProperties)
				{
					PropertyLayouts += FString::Printf(
						TEXT(" [%s offset=%u size=%u alignment=%u storage=%u]"),
						*Property.CanonicalName,
						Property.SemanticByteOffset,
						Property.SemanticStorageSize,
						Property.SemanticStorageAlignment,
						static_cast<uint8>(Property.StorageKind));
				}
				EncodingError.Detail = FString::Printf(
					TEXT("%s; Type=%s size=%llu alignment=%u base-boundary=%u properties=%s"),
					*EncodingError.Detail,
					*Class.TypeSchema.CanonicalName,
					Class.TypeSchema.Layout.SemanticSize,
					Class.TypeSchema.Layout.SemanticAlignment,
					Class.TypeSchema.Layout.BasePropertyBoundary,
					*PropertyLayouts);
				return EncodingError;
			}
		}

		FCleanCaptureBuildDependencyResolver BuildDependencyResolver;
		for (FClassCapture& Class : Classes)
		{
			const FAngelscriptCacheStableReference TypeReference{
				EAngelscriptCacheReferenceKind::ScriptType,
				Class.TypeDeclaration.StableKey,
				Class.TypeDeclaration.SignatureHash};
			BuildDependencyResolver.AddType(
				Class.ScriptType, TypeReference,
				Class.TypeSchema.Layout.TypeLayoutHash);
			BuildDependencyResolver.AddDerivedStaticClassGlobal(
				Class.GeneratedStaticClassGlobal, TypeReference);
			BuildDependencyResolver.AddDerivedStaticClassFunction(
				Class.GeneratedStaticClassFunction, TypeReference);
			for (int32 PropertyIndex = 0;
				PropertyIndex < Class.PropertyDeclarations.Num(); ++PropertyIndex)
			{
				BuildDependencyResolver.AddProperty(
					Class.ScriptType,
					Class.ScriptType->localProperties[PropertyIndex],
					{EAngelscriptCacheReferenceKind::ScriptProperty,
						Class.PropertyDeclarations[PropertyIndex].StableKey,
						Class.PropertyDeclarations[PropertyIndex].SignatureHash},
					Class.TypeSchema.OrderedProperties[PropertyIndex].
						PropertyLayoutFingerprint);
			}
		}

		for (FClassFunctionCapture& Capture : FunctionCaptures)
		{
			FFunctionArtifactStream ExecutionStream;
			asCWriter FunctionWriter(
				ScriptModule, &ExecutionStream, ScriptModule->engine, true);
			const int WriteResult = FunctionWriter.WriteFunctionArtifact(Capture.Function);
			if (WriteResult == asNOT_SUPPORTED)
			{
				return Failure(EAngelscriptCacheCleanCaptureError::NotCacheable,
					FString::Printf(TEXT("Function %s contains unsupported symbolic reference tables"),
						*Capture.Declaration.CanonicalDeclaration));
			}
			if (WriteResult < 0 || ExecutionStream.Bytes.IsEmpty())
			{
				return Failure(EAngelscriptCacheCleanCaptureError::FunctionArtifactFailed,
					FString::Printf(TEXT("The maintained-fork artifact writer failed for %s with %d"),
						*Capture.Declaration.CanonicalDeclaration, WriteResult));
			}

			TArray<uint8> DebugPayload;
			TArray<FAngelscriptCachedDebugSourceReference> DebugSources;
			FString DebugFailure;
			if (!TryBuildDebugPayload(*Module, *Capture.Function,
				SourceFile.SourceFileKey, DebugPayload, DebugSources, DebugFailure))
			{
				return Failure(EAngelscriptCacheCleanCaptureError::NotCacheable,
					MoveTemp(DebugFailure));
			}
			Capture.Debug.PayloadSchemaVersion =
				FAngelscriptCacheRemainingRecordArchive::DebugSidecarPayloadSchemaVersion;
			Capture.Debug.FunctionKey = FAngelscriptStableFunctionKey{
				Capture.Declaration.StableKey};
			Capture.Debug.Profile = Options.Profile;
			Capture.Debug.VmDebugCodecVersion =
				FAngelscriptFunctionArtifactCodec::DebugCodecVersion;
			Capture.Debug.Sources = MoveTemp(DebugSources);
			Capture.Debug.CanonicalDebugPayload = MoveTemp(DebugPayload);
			Capture.Debug.DebugHash =
				FAngelscriptArtifactIdentityBuilder::BuildFunctionContentHash(
					{}, Capture.Debug.CanonicalDebugPayload).Debug;
			if (!TrySerializeRecord(EAngelscriptCacheRecordKind::DebugSidecar,
				Capture.Debug,
				FAngelscriptCacheRemainingRecordArchive::SerializeDebugSidecar,
				Capture.DebugRecord, EncodingError))
			{
				return EncodingError;
			}

			FAngelscriptFunctionSourceDescriptor FunctionSource;
			FunctionSource.Kind = Capture.Declaration.EntityKind;
			FunctionSource.CanonicalSource = UTF8_TO_TCHAR(
				Capture.Function->scriptData->artifactCanonicalSource.AddressOf());
			FunctionSource.CanonicalOptions = Options.CanonicalCompileOptions;
			Capture.Body.PayloadSchemaVersion =
				FAngelscriptCacheRemainingRecordArchive::FunctionBodyPayloadSchemaVersion;
			Capture.Body.ModuleKey = ModuleKey;
			Capture.Body.Identity.FunctionKey = FAngelscriptStableFunctionKey{
				Capture.Declaration.StableKey};
			Capture.Body.Identity.Profile = Options.Profile;
			Capture.Body.ExpectedDeclarationAbi = Capture.Declaration.SignatureHash;
			Capture.Body.FunctionSourceDigest =
				FAngelscriptArtifactIdentityBuilder::BuildFunctionSourceDigest(
					FunctionSource);
			Capture.Body.InvocationKind = Capture.InvocationKind;
			Capture.Body.VmExecutionCodecVersion =
				FAngelscriptFunctionArtifactCodec::ExecutionCodecVersion;
			Capture.RawVmPayload = MoveTemp(ExecutionStream.Bytes);
			Capture.Body.DebugSidecar = Capture.DebugRecord.RecordId;
			Capture.Body.Identity.Content =
				FAngelscriptArtifactIdentityBuilder::BuildFunctionContentHash(
					Capture.RawVmPayload, Capture.Debug.CanonicalDebugPayload);
		}
		for (const FClassFunctionCapture& Capture : FunctionCaptures)
		{
			BuildDependencyResolver.AddFunction(
				Capture.Function,
				{EAngelscriptCacheReferenceKind::ScriptFunction,
					Capture.Declaration.StableKey,
					Capture.Declaration.SignatureHash},
				Capture.Body.Identity.Content.Execution);
		}

		TArray<FAngelscriptCachedTypeSchema> TypeAuthoritySchemas;
		TypeAuthoritySchemas.Reserve(Classes.Num());
		for (const FClassCapture& Class : Classes)
		{
			TypeAuthoritySchemas.Add(Class.TypeSchema);
		}
		TArray<FAngelscriptCachedFunctionBody> FunctionAuthorityBodies;
		FunctionAuthorityBodies.Reserve(FunctionCaptures.Num());
		for (const FClassFunctionCapture& Capture : FunctionCaptures)
		{
			FunctionAuthorityBodies.Add(Capture.Body);
		}
		FAngelscriptCacheFunctionInputAuthorities FunctionAuthorities;
		FunctionAuthorities.ModuleInterface = &ModuleInterface;
		FunctionAuthorities.TypeSchemas = TypeAuthoritySchemas;
		FunctionAuthorities.ModuleState = &ModuleState;
		FunctionAuthorities.FunctionBodies = FunctionAuthorityBodies;
		FAngelscriptCacheEngineEnvironmentResolver EnvironmentSymbols(
			*ScriptModule->engine);
		FunctionAuthorities.ExternalSymbols = &EnvironmentSymbols;
		uint32 GraphCarriedDependencyFunctionCount = 0;
		for (FClassFunctionCapture& Capture : FunctionCaptures)
		{
			bool bUsedGraphCarriedDependencies = false;
			IdentityResult = CaptureActualDependencies(
				*Capture.Function, BuildDependencyResolver, ModuleKey,
				Capture.Body.Identity.FunctionKey, RestoredDependencies,
				Capture.Body.ActualDependencies,
				bUsedGraphCarriedDependencies);
			GraphCarriedDependencyFunctionCount +=
				bUsedGraphCarriedDependencies ? 1u : 0u;
			if (!IdentityResult.IsSuccess())
			{
				return ValidationFailure(EAngelscriptCacheCleanCaptureError::NotCacheable,
					*FString::Printf(TEXT("Function %s class-graph dependency capture"),
						*Capture.Declaration.CanonicalDeclaration), IdentityResult);
			}
			const FAngelscriptCacheFunctionInputResolution InputResolution =
				FAngelscriptCacheCompilerBridge::ResolveCurrentFunctionInput(
					Capture.Body, Capture.Body.FunctionSourceDigest,
					FunctionAuthorities);
			if ((InputResolution.Status
					!= EAngelscriptCacheFunctionInputStatus::ResolvedMatch
					&& InputResolution.Status
						!= EAngelscriptCacheFunctionInputStatus::ResolvedMismatch)
				|| InputResolution.CurrentInputDigest.Hash.IsZero())
			{
				return Failure(EAngelscriptCacheCleanCaptureError::NotCacheable,
					FString::Printf(TEXT("Function %s class-graph inputs could not be resolved: Status=%u Missing=%d"),
						*Capture.Declaration.CanonicalDeclaration,
						static_cast<uint32>(InputResolution.Status),
						InputResolution.MissingDependencyOrdinal.IsSet()
							? static_cast<int32>(InputResolution.MissingDependencyOrdinal.GetValue())
							: -1));
			}
			Capture.Body.FunctionInputDigest = InputResolution.CurrentInputDigest;
			FAngelscriptFunctionArtifactCodec ExecutionCodec(
				*ScriptModule, *ScriptModule->engine);
			FAngelscriptHash256 EncodedExecutionHash;
			const FAngelscriptCacheValidationResult ExecutionResult =
				ExecutionCodec.EncodeExecutionArtifact(
					Capture.RawVmPayload, ModuleKey,
					Capture.Body.ActualDependencies,
					Capture.Body.CanonicalExecutionPayload,
					EncodedExecutionHash);
			if (!ExecutionResult.IsSuccess()
				|| EncodedExecutionHash != Capture.Body.Identity.Content.Execution)
			{
				FString ExecutionDetail = FString::Printf(
					TEXT("Function %s class-graph execution envelope"),
					*Capture.Declaration.CanonicalDeclaration);
				if (!ExecutionCodec.GetLastExecutionFailureDetail().IsEmpty())
				{
					ExecutionDetail += TEXT(": ");
					ExecutionDetail +=
						ExecutionCodec.GetLastExecutionFailureDetail();
				}
				return ValidationFailure(
					EAngelscriptCacheCleanCaptureError::FunctionArtifactFailed,
					*ExecutionDetail,
					ExecutionResult.IsSuccess()
						? FAngelscriptCacheValidationResult::AtStage(
							EAngelscriptCacheValidationError::DerivedHashMismatch,
							EAngelscriptCacheRecordKind::FunctionBody,
							EAngelscriptCacheValidationStage::OpaqueCodec)
						: ExecutionResult);
			}
			if (!TrySerializeRecord(EAngelscriptCacheRecordKind::FunctionBody,
				Capture.Body,
				FAngelscriptCacheRemainingRecordArchive::SerializeFunctionBody,
				Capture.BodyRecord, EncodingError))
			{
				return EncodingError;
			}
		}

		FAngelscriptCachedModuleSnapshot Snapshot;
		Snapshot.PayloadSchemaVersion =
			FAngelscriptCacheRemainingRecordArchive::ModuleSnapshotPayloadSchemaVersion;
		Snapshot.ModuleKey = ModuleKey;
		Snapshot.ModuleInterface = {ModuleKey, InterfaceRecord.RecordId};
		Snapshot.ModuleState = {ModuleKey, StateRecord.RecordId};
		for (const FClassCapture& Class : Classes)
		{
			Snapshot.TypeSchemas.Add({
				Class.TypeSchema.TypeKey, Class.TypeRecord.RecordId});
		}
		Snapshot.TypeSchemas.Sort([](
			const FAngelscriptCachedTypeSchemaLink& Left,
			const FAngelscriptCachedTypeSchemaLink& Right)
		{
			return Left.TypeKey.Hash < Right.TypeKey.Hash;
		});
		for (const FClassFunctionCapture& Capture : FunctionCaptures)
		{
			Snapshot.FunctionBodies.Add({
				Capture.Body.Identity.FunctionKey,
				Capture.BodyRecord.RecordId});
		}
		Snapshot.FunctionBodies.Sort([](
			const FAngelscriptCachedFunctionBodyLink& Left,
			const FAngelscriptCachedFunctionBodyLink& Right)
		{
			return Left.FunctionKey.Hash < Right.FunctionKey.Hash;
		});
		if (!TrySerializeRecord(EAngelscriptCacheRecordKind::ModuleSnapshot,
			Snapshot,
			FAngelscriptCacheRemainingRecordArchive::SerializeModuleSnapshot,
			SnapshotRecord, EncodingError))
		{
			return EncodingError;
		}

		FAngelscriptCacheCleanModuleArtifacts CandidateArtifacts;
		CandidateArtifacts.ModuleKey = ModuleKey;
		CandidateArtifacts.CanonicalModuleName = Module->ModuleName;
		CandidateArtifacts.SourceSnapshot = SourceIndex.SourceSnapshot;
		CandidateArtifacts.SourceIndexRecordId = SourceRecord.RecordId;
		CandidateArtifacts.ModuleSnapshot = {ModuleKey, SnapshotRecord.RecordId};
		CandidateArtifacts.Records.Reserve(
			4 + Classes.Num() + FunctionCaptures.Num() * 2);
		CandidateArtifacts.Records.Add(MoveTemp(SourceRecord));
		CandidateArtifacts.Records.Add(MoveTemp(InterfaceRecord));
		for (FClassCapture& Class : Classes)
		{
			CandidateArtifacts.Records.Add(MoveTemp(Class.TypeRecord));
		}
		CandidateArtifacts.Records.Add(MoveTemp(StateRecord));
		for (FClassFunctionCapture& Capture : FunctionCaptures)
		{
			CandidateArtifacts.Records.Add(MoveTemp(Capture.BodyRecord));
			CandidateArtifacts.Records.Add(MoveTemp(Capture.DebugRecord));
		}
		CandidateArtifacts.Records.Add(MoveTemp(SnapshotRecord));
		FAngelscriptCacheCleanCaptureResult Result =
			ValidateAndPromoteAngelscriptCleanCompiledModuleArtifacts(
				Module, Options, MoveTemp(CandidateArtifacts), OutArtifacts);
		if (Result.IsSuccess())
		{
			Result.GraphCarriedDependencyFunctionCount =
				GraphCarriedDependencyFunctionCount;
			Result.Detail = FString::Printf(
				TEXT("Captured %d base-before-derived classes and %d stable functions (%u graph-carried dependency sets) as %u graph-validated Cache V2 records"),
				Classes.Num(), FunctionCaptures.Num(),
				GraphCarriedDependencyFunctionCount,
				Result.ValidatedGraphRecordCount);
		}
		return Result;
	}

	static FAngelscriptCacheCleanCaptureResult CaptureRootClassPrimitiveVertical(
		const TSharedRef<FAngelscriptModuleDesc>& Module,
		const FAngelscriptCacheCleanCaptureOptions& Options,
		const FAngelscriptCachedSourceIndex& SourceIndex,
		const FAngelscriptCachedSourceFile& SourceFile,
		const FAngelscriptStableModuleKey& ModuleKey,
		const IAngelscriptCacheRestoredFunctionDependencySource*
			RestoredDependencies,
		FAngelscriptCacheCleanModuleArtifacts& OutArtifacts)
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
				TEXT("The root-class capture vertical requires one class and no enum, delegate, import, typedef or post-init declaration"));
		}

		const TSharedRef<FAngelscriptClassDesc>& ClassDesc = Module->Classes[0];
		asCObjectType* ScriptType = static_cast<asCObjectType*>(ClassDesc->ScriptType);
		if (ScriptType == nullptr
			|| ScriptModule->GetObjectTypeByIndex(0) != ScriptType
			|| ClassDesc->bIsStruct || ClassDesc->bIsStaticsClass
			|| !ClassDesc->bSuperIsCodeClass || ClassDesc->CodeSuperClass == nullptr
			|| ScriptType->shadowType == nullptr || ScriptType->derivedFrom != nullptr
			|| !ClassDesc->ImplementedInterfaces.IsEmpty()
			|| !ClassDesc->ComposeOntoClass.IsEmpty()
			|| ScriptType->size <= 0 || ScriptType->alignment <= 0
			|| ScriptType->basePropertyOffset < 0
			|| ClassDesc->StaticClassGlobalVariableName.IsEmpty())
		{
			return Failure(EAngelscriptCacheCleanCaptureError::NotCacheable,
				TEXT("The class is outside the first root-UClass primitive capture shape"));
		}

		const FString ClassNamespace = ClassDesc->Namespace.IsSet()
			? ClassDesc->Namespace.GetValue()
			: UTF8_TO_TCHAR(ScriptType->GetNamespace());
		const FString ClassName = UTF8_TO_TCHAR(ScriptType->GetName());
		if (!ClassName.Equals(ClassDesc->ClassName, ESearchCase::CaseSensitive))
		{
			return Failure(EAngelscriptCacheCleanCaptureError::NotCacheable,
				TEXT("The class descriptor and compiled VM type name disagree"));
		}

		FAngelscriptCachedDeclaration TypeDeclaration;
		TypeDeclaration.DeclarationKind = EAngelscriptCacheDeclarationKind::Type;
		TypeDeclaration.EntityKind = EAngelscriptArtifactEntityKind::Class;
		TypeDeclaration.SchemaCoverage = EAngelscriptCacheSchemaCoverage::Required;
		TypeDeclaration.BodyCoverage = EAngelscriptCacheBodyCoverage::Forbidden;
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
		TypeIdentity.CanonicalDeclaration = TypeDeclaration.CanonicalDeclaration;
		TypeDeclaration.StableKey =
			FAngelscriptArtifactIdentityBuilder::BuildTypeKey(TypeIdentity).Hash;
		FAngelscriptCacheValidationResult IdentityResult =
			FAngelscriptCacheSemanticArchive::ComputeDeclarationHashes(
				TypeDeclaration, TypeDeclaration.SignatureHash,
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
		TypeSchema.TypeKey = FAngelscriptStableTypeKey{TypeDeclaration.StableKey};
		TypeSchema.TypeKind = EAngelscriptCachedTypeKind::Class;
		TypeSchema.CanonicalNamespace = TypeDeclaration.CanonicalNamespace;
		TypeSchema.CanonicalName = TypeDeclaration.CanonicalName;
		TypeSchema.CanonicalDeclaration = TypeDeclaration.CanonicalDeclaration;
		TypeSchema.TypeSemanticFlags = static_cast<uint32>(
			EAngelscriptCachedTypeSemanticFlags::ReferenceType);
		if (ClassDesc->bAbstract)
		{
			TypeSchema.TypeSemanticFlags |= static_cast<uint32>(
				EAngelscriptCachedTypeSemanticFlags::Abstract);
		}
		TypeSchema.Metadata = TypeDeclaration.Metadata;
		TypeSchema.Layout.SemanticSize = static_cast<uint64>(ScriptType->size);
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
		LayoutInput.InputKind = EAngelscriptCachedTypeLayoutInputKind::CodeRoot;
		LayoutInput.Target = CodeRoot;
		LayoutInput.BoundaryContribution =
			static_cast<uint32>(ScriptType->basePropertyOffset);
		LayoutInput.AlignmentContribution = static_cast<uint32>(
			static_cast<const asCObjectType*>(ScriptType->shadowType)->alignment);
		IdentityResult = FAngelscriptCacheTypeSchemaArchive::ComputeLayoutInputHash(
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
			PropertyIndex < ScriptType->localProperties.GetLength(); ++PropertyIndex)
		{
			const asCObjectProperty* ScriptProperty =
				ScriptType->localProperties[PropertyIndex];
			if (ScriptProperty == nullptr || ScriptProperty->isInherited
				|| ScriptProperty->byteOffset < 0)
			{
				return Failure(EAngelscriptCacheCleanCaptureError::NotCacheable,
					TEXT("The class contains an unsupported local VM property"));
			}
			FAngelscriptCachedDataType CachedType;
			FString CanonicalType;
			FAngelscriptCacheSemanticDependency PropertyTypeDependency;
			bool bHasPropertyTypeDependency = false;
			if (!TryMapPrimitiveDataType(
				ScriptProperty->type, CachedType, CanonicalType))
			{
				bHasPropertyTypeDependency = TryMapEnvironmentPropertyDataType(
					ScriptProperty->type,
					CachedType,
					CanonicalType,
					PropertyTypeDependency);
				if (!bHasPropertyTypeDependency)
				{
					return Failure(EAngelscriptCacheCleanCaptureError::NotCacheable,
						TEXT("The root-class capture vertical supports primitive int or stable environment-value properties"));
				}
			}

			const FString PropertyName =
				UTF8_TO_TCHAR(ScriptProperty->name.AddressOf());
			const FAngelscriptPropertyDesc* ReflectedProperty = nullptr;
			for (const TSharedRef<FAngelscriptPropertyDesc>& Candidate
				: ClassDesc->Properties)
			{
				if ((Candidate->ScriptPropertyIndex
						== static_cast<int32>(PropertyIndex)
					|| Candidate->PropertyName.Equals(
						PropertyName, ESearchCase::CaseSensitive)))
				{
					if (ReflectedProperty != nullptr)
					{
						return Failure(
							EAngelscriptCacheCleanCaptureError::NotCacheable,
							TEXT("More than one reflection descriptor maps to one VM property"));
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
			PropertyDeclaration.EntityKind = EAngelscriptArtifactEntityKind::Property;
			PropertyDeclaration.SchemaCoverage =
				EAngelscriptCacheSchemaCoverage::Forbidden;
			PropertyDeclaration.BodyCoverage =
				EAngelscriptCacheBodyCoverage::Forbidden;
			PropertyDeclaration.OwnerKind = EAngelscriptFunctionOwnerKind::Type;
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
				AppendMetadata(ReflectedProperty->Meta,
					PropertyDeclaration.Metadata);
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
			IdentityResult =
				FAngelscriptCacheSemanticArchive::ComputeDeclarationHashes(
					PropertyDeclaration, PropertyDeclaration.SignatureHash,
					PropertyDeclaration.TraitsHash);
			if (!IdentityResult.IsSuccess())
			{
				return ValidationFailure(
					EAngelscriptCacheCleanCaptureError::RecordEncodingFailed,
					TEXT("Property declaration hashes"), IdentityResult);
			}

			FAngelscriptCachedPropertySchema PropertySchema;
			PropertySchema.LayoutOrdinal = PropertyIndex;
			PropertySchema.SemanticByteOffset =
				static_cast<uint32>(ScriptProperty->byteOffset);
			PropertySchema.PropertyKey = FAngelscriptStablePropertyKey{
				PropertyDeclaration.StableKey};
			PropertySchema.CanonicalName = PropertyName;
			PropertySchema.Type = CachedType;
			PropertySchema.StorageKind =
				ScriptProperty->type.IsObjectHandle()
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
						TypeSchema.TypeKey, PropertySchema,
						PropertySchema.PropertyLayoutFingerprint);
			}
			if (!IdentityResult.IsSuccess())
			{
				return ValidationFailure(
					EAngelscriptCacheCleanCaptureError::RecordEncodingFailed,
					TEXT("Property layout hashes"), IdentityResult);
			}
			PropertyDeclarations.Add(MoveTemp(PropertyDeclaration));
			TypeSchema.OrderedProperties.Add(MoveTemp(PropertySchema));
			if (bHasPropertyTypeDependency)
			{
				TypeSchema.Dependencies.AddUnique(MoveTemp(PropertyTypeDependency));
			}
		}
		if (MatchedPropertyDescs.Num() != ClassDesc->Properties.Num())
		{
			return Failure(EAngelscriptCacheCleanCaptureError::NotCacheable,
				TEXT("A reflected property descriptor has no matching local VM property"));
		}
		FAngelscriptCachedDeclaration GlobalDeclaration;
		FAngelscriptCachedModuleState ModuleState;
		FAngelscriptCachedGlobalSchema* Global = nullptr;
		FAngelscriptCachedHardValue* HardValue = nullptr;
		ModuleState.PayloadSchemaVersion =
			FAngelscriptCacheRemainingRecordArchive::ModuleStatePayloadSchemaVersion;
		ModuleState.ModuleKey = ModuleKey;
		ModuleState.Profile = Options.Profile;
		asCGlobalProperty* UserGlobal = nullptr;
		asCGlobalProperty* GeneratedStaticClassGlobal = nullptr;
		uint32 GeneratedGlobalCount = 0;
		for (asUINT GlobalIndex = 0;
			GlobalIndex < ScriptModule->scriptGlobalsList.GetLength(); ++GlobalIndex)
		{
			asCGlobalProperty* Candidate = ScriptModule->scriptGlobalsList[GlobalIndex];
			if (Candidate == nullptr)
			{
				return Failure(EAngelscriptCacheCleanCaptureError::NotCacheable,
					TEXT("The VM global table contains a null entry"));
			}
			const FString CandidateName =
				UTF8_TO_TCHAR(Candidate->name.AddressOf());
			if (CandidateName.Equals(ClassDesc->StaticClassGlobalVariableName,
				ESearchCase::CaseSensitive))
			{
				GeneratedStaticClassGlobal = Candidate;
				++GeneratedGlobalCount;
				continue;
			}
			if (UserGlobal != nullptr)
			{
				return Failure(EAngelscriptCacheCleanCaptureError::NotCacheable,
					TEXT("The root-class primitive vertical supports one user global"));
			}
			UserGlobal = Candidate;
		}
		if (GeneratedGlobalCount != 1
			|| GeneratedStaticClassGlobal == nullptr
			|| (UserGlobal != nullptr && !UserGlobal->isPureConstant))
		{
			return Failure(EAngelscriptCacheCleanCaptureError::NotCacheable,
				FString::Printf(
					TEXT("The class static helper or pure-constant user global could not be classified exactly: Generated=%u User=%s PureConstant=%d DefaultInit=%d HasInitFunction=%d"),
					GeneratedGlobalCount,
					UserGlobal != nullptr
						? UTF8_TO_TCHAR(UserGlobal->name.AddressOf()) : TEXT("<none>"),
					UserGlobal != nullptr && UserGlobal->isPureConstant ? 1 : 0,
					UserGlobal != nullptr && UserGlobal->isDefaultInit ? 1 : 0,
					UserGlobal != nullptr && UserGlobal->GetInitFunc() != nullptr ? 1 : 0));
		}
		if (UserGlobal != nullptr)
		{
			FAngelscriptCachedDataType GlobalType;
			FString GlobalTypeSpelling;
			if (!TryMapPrimitiveDataType(
				UserGlobal->type, GlobalType, GlobalTypeSpelling))
			{
				return Failure(EAngelscriptCacheCleanCaptureError::NotCacheable,
					TEXT("The root-class primitive vertical supports only a const int global"));
			}
		GlobalDeclaration.DeclarationKind = EAngelscriptCacheDeclarationKind::Global;
		GlobalDeclaration.EntityKind = EAngelscriptArtifactEntityKind::GlobalVariable;
		GlobalDeclaration.SchemaCoverage = EAngelscriptCacheSchemaCoverage::Forbidden;
		GlobalDeclaration.BodyCoverage = EAngelscriptCacheBodyCoverage::Forbidden;
		GlobalDeclaration.OwnerKind = EAngelscriptFunctionOwnerKind::Module;
		GlobalDeclaration.OwnerKey = ModuleKey.Hash;
		GlobalDeclaration.ModuleKey = ModuleKey;
		GlobalDeclaration.CanonicalNamespace = UserGlobal->nameSpace != nullptr
			? UTF8_TO_TCHAR(UserGlobal->nameSpace->name.AddressOf()) : FString();
		GlobalDeclaration.CanonicalName =
			UTF8_TO_TCHAR(UserGlobal->name.AddressOf());
		GlobalDeclaration.CanonicalTypeSpelling = GlobalTypeSpelling;
		GlobalDeclaration.CanonicalDeclaration = FString::Printf(
			TEXT("%s %s"), *GlobalTypeSpelling,
			*GlobalDeclaration.CanonicalName);
		GlobalDeclaration.DeclaredType = GlobalType;
		GlobalDeclaration.TraitFlags = static_cast<uint32>(
			EAngelscriptCachedDeclarationTraitFlags::Const);
		GlobalDeclaration.Slots.Add({
			EAngelscriptCacheDeclarationSlotKind::Declaration,
			static_cast<uint32>(PropertyDeclarations.Num()) + 1});
		FAngelscriptGlobalIdentityDescriptor GlobalIdentity;
		GlobalIdentity.ModuleKey = ModuleKey;
		GlobalIdentity.Namespace = GlobalDeclaration.CanonicalNamespace;
		GlobalIdentity.Kind = GlobalDeclaration.EntityKind;
		GlobalIdentity.Name = GlobalDeclaration.CanonicalName;
		GlobalIdentity.CanonicalType = GlobalTypeSpelling;
		GlobalDeclaration.StableKey =
			FAngelscriptArtifactIdentityBuilder::BuildGlobalKey(GlobalIdentity).Hash;
		IdentityResult = FAngelscriptCacheSemanticArchive::ComputeDeclarationHashes(
			GlobalDeclaration, GlobalDeclaration.SignatureHash,
			GlobalDeclaration.TraitsHash);
		if (!IdentityResult.IsSuccess())
		{
			return ValidationFailure(
				EAngelscriptCacheCleanCaptureError::RecordEncodingFailed,
				TEXT("Global declaration hashes"), IdentityResult);
		}

			Global = &ModuleState.OrderedGlobals.AddDefaulted_GetRef();
			Global->StorageOrdinal = 0;
			Global->GlobalKey = FAngelscriptStableGlobalKey{GlobalDeclaration.StableKey};
			Global->CanonicalNamespace = GlobalDeclaration.CanonicalNamespace;
			Global->CanonicalName = GlobalDeclaration.CanonicalName;
			Global->Type = GlobalType;
			Global->GlobalTraitFlags = GlobalDeclaration.TraitFlags;
			Global->InitializationKind =
			EAngelscriptCachedGlobalInitializationKind::PureConstant;
			Global->CleanupPolicy = EAngelscriptCachedGlobalCleanupPolicy::None;
		IdentityResult = FAngelscriptCacheRemainingRecordArchive::
			ComputeGlobalStorageLayoutFingerprint(
				ModuleKey, *Global, Global->StorageLayoutFingerprint);
		if (!IdentityResult.IsSuccess())
		{
			return ValidationFailure(
				EAngelscriptCacheCleanCaptureError::RecordEncodingFailed,
				TEXT("Global storage layout"), IdentityResult);
		}

			HardValue = &ModuleState.HardValues.AddDefaulted_GetRef();
			HardValue->HardValueKind = EAngelscriptCachedHardValueKind::GlobalConstant;
			HardValue->Owner = {
			EAngelscriptCacheReferenceKind::ScriptGlobal,
			GlobalDeclaration.StableKey,
			GlobalDeclaration.SignatureHash,
		};
			HardValue->Type = GlobalType;
			HardValue->CanonicalValue.Emplace();
			HardValue->CanonicalValue->ValueKind =
			EAngelscriptCachedCanonicalValueKind::SignedInteger;
		const int32 ConstantValue = static_cast<int32>(UserGlobal->storage);
			HardValue->CanonicalValue->FixedWidthValueBytes = {
			static_cast<uint8>(ConstantValue & 0xff),
			static_cast<uint8>((ConstantValue >> 8) & 0xff),
			static_cast<uint8>((ConstantValue >> 16) & 0xff),
			static_cast<uint8>((ConstantValue >> 24) & 0xff),
		};
		IdentityResult = FAngelscriptCacheRemainingRecordArchive::
			ComputeGlobalConstantHardValueHash(
				*HardValue, HardValue->HardValueHash);
		if (!IdentityResult.IsSuccess())
		{
			return ValidationFailure(
				EAngelscriptCacheCleanCaptureError::RecordEncodingFailed,
				TEXT("Global hard value"), IdentityResult);
		}
		}
		IdentityResult = FAngelscriptCacheRemainingRecordArchive::
			ComputeModuleStateInputHash(ModuleState, ModuleState.StateInputHash);
		if (!IdentityResult.IsSuccess())
		{
			return ValidationFailure(
				EAngelscriptCacheCleanCaptureError::RecordEncodingFailed,
				TEXT("Module state input hash"), IdentityResult);
		}

		struct FRootFunctionCapture
		{
			asCScriptFunction* Function = nullptr;
			const FAngelscriptFunctionDesc* ReflectedFunction = nullptr;
			FAngelscriptCachedDeclaration Declaration;
			EAngelscriptCachedFunctionInvocationKind InvocationKind =
				EAngelscriptCachedFunctionInvocationKind::Invalid;
			FAngelscriptCachedFunctionBody Body;
			FAngelscriptCachedDebugSidecar Debug;
			TArray<uint8> RawVmPayload;
			FAngelscriptPreparedRecord BodyRecord;
			FAngelscriptPreparedRecord DebugRecord;
		};
		TArray<FRootFunctionCapture> FunctionCaptures;
		TSet<const FAngelscriptFunctionDesc*> MatchedFunctionDescs;
		asCScriptFunction* GeneratedStaticClassFunction = nullptr;
		uint32 GeneratedStaticClassFunctionCount = 0;
		for (asUINT FunctionIndex = 0;
			FunctionIndex < ScriptModule->scriptFunctions.GetLength(); ++FunctionIndex)
		{
			asCScriptFunction* Function =
				ScriptModule->scriptFunctions[FunctionIndex];
			if (Function == nullptr || Function->module != ScriptModule)
			{
				return Failure(EAngelscriptCacheCleanCaptureError::NotCacheable,
					TEXT("The VM module function table contains a null or foreign entry"));
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
			const TOptional<EAngelscriptCachedFunctionInvocationKind>
				InvocationKind = MapCachedFunctionInvocationKind(
					Function->artifactInvocationKind);
			if (Function->GetFuncType() != asFUNC_SCRIPT
				|| Function->scriptData == nullptr
				|| !EntityKind.IsSet() || !InvocationKind.IsSet()
				|| Function->scriptData->artifactCanonicalSource.GetLength() == 0)
			{
				return Failure(EAngelscriptCacheCleanCaptureError::NotCacheable,
					FString::Printf(
						TEXT("Module function %s has no complete stable invocation/source authority"),
						*CanonicalDeclaration));
			}

			FAngelscriptStableFunctionKey StableFunctionKey;
			FString StableKeyFailure;
			if (!FAngelscriptCacheStableSymbolIdentity::TryBuildFunctionKey(
				ModuleKey, *Function, StableFunctionKey, &StableKeyFailure))
			{
				return Failure(EAngelscriptCacheCleanCaptureError::NotCacheable,
					FString::Printf(
						TEXT("Module function %s has no stable FunctionKey: %s"),
						*CanonicalDeclaration, *StableKeyFailure));
			}

			FRootFunctionCapture& Capture =
				FunctionCaptures.AddDefaulted_GetRef();
			Capture.Function = Function;
			Capture.InvocationKind = InvocationKind.GetValue();
			for (const TSharedRef<FAngelscriptFunctionDesc>& Candidate
				: ClassDesc->Methods)
			{
				if (Candidate->ScriptFunction != Function)
				{
					continue;
				}
				if (Capture.ReflectedFunction != nullptr)
				{
					return Failure(
						EAngelscriptCacheCleanCaptureError::NotCacheable,
						TEXT("More than one reflected function descriptor maps to one VM function"));
				}
				Capture.ReflectedFunction = &Candidate.Get();
				MatchedFunctionDescs.Add(Capture.ReflectedFunction);
			}
			FAngelscriptCachedDeclaration& Declaration = Capture.Declaration;
			Declaration.DeclarationKind =
				EAngelscriptCacheDeclarationKind::Function;
			Declaration.EntityKind = EntityKind.GetValue();
			Declaration.SchemaCoverage =
				EAngelscriptCacheSchemaCoverage::Forbidden;
			Declaration.BodyCoverage = EAngelscriptCacheBodyCoverage::Required;
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
				| BuildFunctionDescriptorTraitFlags(Capture.ReflectedFunction);
			Declaration.ReflectionFlags =
				BuildFunctionReflectionFlags(Capture.ReflectedFunction);
			if (Capture.ReflectedFunction != nullptr)
			{
				AppendMetadata(
					Capture.ReflectedFunction->Meta, Declaration.Metadata);
			}
			Declaration.Slots.Add({
				EAngelscriptCacheDeclarationSlotKind::Function,
				static_cast<uint32>(FunctionCaptures.Num() - 1)});
			FAngelscriptCachedDataType ReturnType;
			if (!TryMapRootFunctionDataType(
				Function->returnType, *ScriptType, TypeDeclaration, ReturnType))
			{
				return Failure(EAngelscriptCacheCleanCaptureError::NotCacheable,
					FString::Printf(
						TEXT("Function %s has a return type outside the root-class stable type table"),
						*CanonicalDeclaration));
			}
			Declaration.DeclaredType = MoveTemp(ReturnType);

			for (asUINT ParameterIndex = 0;
				ParameterIndex < Function->parameterTypes.GetLength(); ++ParameterIndex)
			{
				FAngelscriptCachedParameter& Parameter =
					Declaration.OrderedParameters.AddDefaulted_GetRef();
				Parameter.Ordinal = ParameterIndex;
				if (ParameterIndex < Function->parameterNames.GetLength()
					&& Function->parameterNames[ParameterIndex].GetLength() != 0)
				{
					Parameter.CanonicalName = UTF8_TO_TCHAR(
						Function->parameterNames[ParameterIndex].AddressOf());
				}
				else
				{
					Parameter.CanonicalName = FString::Printf(
						TEXT("arg%u"), ParameterIndex);
				}
				if (!TryMapRootFunctionDataType(
					Function->parameterTypes[ParameterIndex],
					*ScriptType,
					TypeDeclaration,
					Parameter.Type))
				{
					return Failure(EAngelscriptCacheCleanCaptureError::NotCacheable,
						FString::Printf(
							TEXT("Function %s parameter %u is outside the root-class stable type table"),
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
					Parameter.Passing = EAngelscriptCachedParameterPassing::Value;
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
						TEXT("A function parameter has an unknown passing mode"));
				}
				if (ParameterIndex < Function->defaultArgs.GetLength()
					&& Function->defaultArgs[ParameterIndex] != nullptr)
				{
					Parameter.CanonicalDefaultExpression = UTF8_TO_TCHAR(
						Function->defaultArgs[ParameterIndex]->AddressOf());
				}
				if (Capture.ReflectedFunction != nullptr)
				{
					if (ParameterIndex
						>= static_cast<asUINT>(
							Capture.ReflectedFunction->Arguments.Num()))
					{
						return Failure(
							EAngelscriptCacheCleanCaptureError::NotCacheable,
							TEXT("A reflected function descriptor has fewer parameters than its VM function"));
					}
					const FAngelscriptArgumentDesc& Argument =
						Capture.ReflectedFunction->Arguments[
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
			if (Capture.ReflectedFunction != nullptr
				&& Capture.ReflectedFunction->Arguments.Num()
					!= static_cast<int32>(Function->parameterTypes.GetLength()))
			{
				return Failure(
					EAngelscriptCacheCleanCaptureError::NotCacheable,
					TEXT("A reflected function descriptor has more parameters than its VM function"));
			}

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
		}
		if (GeneratedStaticClassFunctionCount != 1
			|| GeneratedStaticClassFunction == nullptr
			|| FunctionCaptures.IsEmpty())
		{
			return Failure(EAngelscriptCacheCleanCaptureError::NotCacheable,
				TEXT("The generated StaticClass helper or stable module function table could not be classified exactly"));
		}
		if (MatchedFunctionDescs.Num() != ClassDesc->Methods.Num())
		{
			return Failure(EAngelscriptCacheCleanCaptureError::NotCacheable,
				TEXT("A reflected function descriptor has no exact VM function pointer"));
		}

		auto FindCapture = [&FunctionCaptures](const asCScriptFunction* Function)
			-> FRootFunctionCapture*
		{
			for (FRootFunctionCapture& Candidate : FunctionCaptures)
			{
				if (Candidate.Function == Function)
				{
					return &Candidate;
				}
			}
			return nullptr;
		};
		auto MakeLocalFunctionReference = [](
			const FRootFunctionCapture& Capture)
		{
			return FAngelscriptCacheStableReference{
				EAngelscriptCacheReferenceKind::ScriptFunction,
				Capture.Declaration.StableKey,
				Capture.Declaration.SignatureHash,
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
			const int FunctionId = ScriptType->methods[MethodIndex];
			asCScriptFunction* Function = ScriptModule->engine->GetScriptFunction(
				FunctionId);
			FRootFunctionCapture* Capture = FindCapture(Function);
			if (Capture == nullptr || Function->objectType != ScriptType)
			{
				return Failure(EAngelscriptCacheCleanCaptureError::NotCacheable,
					TEXT("The root class method table contains a non-local or uncacheable function"));
			}
			FAngelscriptCachedMethodEntry& Method =
				TypeSchema.OrderedMethods.AddDefaulted_GetRef();
			Method.EntryKind = EAngelscriptCachedMethodSlotKind::LocalMethod;
			Method.MethodOrdinal = MethodIndex;
			Method.FunctionKey = FAngelscriptStableFunctionKey{
				Capture->Declaration.StableKey};
			Method.DeclaringOwner = TypeSchema.TypeKey;
			Method.ExpectedDeclarationAbi = Capture->Declaration.SignatureHash;
			AddSchemaDependency(
				EAngelscriptCacheSemanticDependencyKind::Declaration,
				MakeLocalFunctionReference(*Capture));
		}

		for (asUINT VftIndex = 0;
			VftIndex < ScriptType->virtualFunctionTable.GetLength(); ++VftIndex)
		{
			asCScriptFunction* Function = ScriptType->virtualFunctionTable[VftIndex];
			FRootFunctionCapture* Capture = FindCapture(Function);
			if (Capture == nullptr || Function->objectType != ScriptType
				|| Function->vfTableIdx != static_cast<int>(VftIndex))
			{
				return Failure(EAngelscriptCacheCleanCaptureError::NotCacheable,
					TEXT("The root class VFT contains a non-local, uncacheable or misindexed function"));
			}
			FAngelscriptCachedVirtualFunctionSlot& Slot =
				TypeSchema.VirtualFunctionTable.AddDefaulted_GetRef();
			Slot.SlotKind =
				EAngelscriptCachedMethodSlotKind::VirtualDeclaration;
			Slot.VftOrdinal = VftIndex;
			Slot.FunctionKey = FAngelscriptStableFunctionKey{
				Capture->Declaration.StableKey};
			Slot.DeclaringOwner = TypeSchema.TypeKey;
			Slot.ImplementingOwner = TypeSchema.TypeKey;
			Slot.ExpectedDeclarationAbi = Capture->Declaration.SignatureHash;
			AddSchemaDependency(
				EAngelscriptCacheSemanticDependencyKind::Declaration,
				MakeLocalFunctionReference(*Capture));
		}

		for (int32 ReflectionIndex = 0;
			ReflectionIndex < ClassDesc->Methods.Num(); ++ReflectionIndex)
		{
			const FAngelscriptFunctionDesc& Reflected =
				ClassDesc->Methods[ReflectionIndex].Get();
			FRootFunctionCapture* Capture = FindCapture(
				static_cast<asCScriptFunction*>(Reflected.ScriptFunction));
			if (Capture == nullptr || Capture->ReflectedFunction != &Reflected
				|| Capture->Declaration.EntityKind
					!= EAngelscriptArtifactEntityKind::Method)
			{
				return Failure(EAngelscriptCacheCleanCaptureError::NotCacheable,
					TEXT("The reflected UFunction member is not a local cacheable method"));
			}
			FAngelscriptCachedReflectedFunctionMember& Member =
				TypeSchema.Reflection.OrderedUFunctionMembers.
					AddDefaulted_GetRef();
			Member.ReflectionOrdinal = static_cast<uint32>(ReflectionIndex);
			Member.CanonicalFunctionName = Reflected.FunctionName;
			Member.CanonicalOriginalFunctionName = Reflected.OriginalFunctionName;
			Member.CanonicalScriptFunctionName = Reflected.ScriptFunctionName;
			Member.Target = MakeLocalFunctionReference(*Capture);
			AddSchemaDependency(
				EAngelscriptCacheSemanticDependencyKind::Declaration,
				Member.Target);
		}

		auto AddBehavior = [&TypeSchema, &FindCapture,
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
			if (FRootFunctionCapture* Capture = FindCapture(Function))
			{
				Slot.Target = MakeLocalFunctionReference(*Capture);
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
			if (!AddBehavior(EAngelscriptCachedBehaviorKind::Construct,
				Index, ScriptType->beh.constructors[Index]))
			{
				return Failure(EAngelscriptCacheCleanCaptureError::NotCacheable,
					TEXT("A constructor behavior has no stable function reference"));
			}
		}
		if (!AddBehavior(EAngelscriptCachedBehaviorKind::Destruct, 0,
				ScriptType->beh.destruct))
		{
			return Failure(EAngelscriptCacheCleanCaptureError::NotCacheable,
				TEXT("A destructor behavior has no stable function reference"));
		}
		for (asUINT Index = 0;
			Index < ScriptType->beh.factories.GetLength(); ++Index)
		{
			if (!AddBehavior(EAngelscriptCachedBehaviorKind::Factory,
				Index, ScriptType->beh.factories[Index]))
			{
				return Failure(EAngelscriptCacheCleanCaptureError::NotCacheable,
					TEXT("A factory behavior has no stable function reference"));
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
					TEXT("A singleton behavior has no stable function reference"));
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
		IdentityResult = FAngelscriptCacheTypeSchemaArchive::ComputeTypeLayoutHash(
			TypeSchema, TypeSchema.Layout.TypeLayoutHash);
		if (!IdentityResult.IsSuccess())
		{
			return ValidationFailure(
				EAngelscriptCacheCleanCaptureError::RecordEncodingFailed,
				TEXT("Class type layout hash"), IdentityResult);
		}

		FAngelscriptCachedModuleInterface ModuleInterface;
		ModuleInterface.PayloadSchemaVersion =
			FAngelscriptCacheSemanticArchive::ModuleInterfacePayloadSchemaVersion;
		ModuleInterface.ModuleKey = ModuleKey;
		ModuleInterface.CanonicalModuleName = Module->ModuleName;
		if (!ClassNamespace.IsEmpty())
		{
			ModuleInterface.CanonicalNamespaces.AddUnique(ClassNamespace);
		}
		if (UserGlobal != nullptr
			&& !GlobalDeclaration.CanonicalNamespace.IsEmpty())
		{
			ModuleInterface.CanonicalNamespaces.AddUnique(
				GlobalDeclaration.CanonicalNamespace);
		}
		ModuleInterface.Declarations.Add(TypeDeclaration);
		ModuleInterface.Declarations.Append(PropertyDeclarations);
		if (UserGlobal != nullptr)
		{
			ModuleInterface.Declarations.Add(GlobalDeclaration);
		}
		for (const FRootFunctionCapture& Capture : FunctionCaptures)
		{
			if (!Capture.Declaration.CanonicalNamespace.IsEmpty())
			{
				ModuleInterface.CanonicalNamespaces.AddUnique(
					Capture.Declaration.CanonicalNamespace);
			}
			ModuleInterface.Declarations.Add(Capture.Declaration);
		}
		IdentityResult = FAngelscriptCacheSemanticArchive::ComputeModuleInterfaceAbi(
			ModuleInterface, ModuleInterface.InterfaceAbi);
		if (!IdentityResult.IsSuccess())
		{
			return ValidationFailure(
				EAngelscriptCacheCleanCaptureError::RecordEncodingFailed,
				TEXT("Module interface ABI"), IdentityResult);
		}

		FAngelscriptPreparedRecord SourceRecord;
		FAngelscriptPreparedRecord InterfaceRecord;
		FAngelscriptPreparedRecord TypeRecord;
		FAngelscriptPreparedRecord StateRecord;
		FAngelscriptPreparedRecord SnapshotRecord;
		FAngelscriptCacheCleanCaptureResult EncodingError;
		if (!TrySerializeRecord(EAngelscriptCacheRecordKind::SourceIndex,
			SourceIndex, FAngelscriptCacheSemanticArchive::SerializeSourceIndex,
			SourceRecord, EncodingError)
			|| !TrySerializeRecord(EAngelscriptCacheRecordKind::ModuleInterface,
				ModuleInterface,
				FAngelscriptCacheSemanticArchive::SerializeModuleInterface,
				InterfaceRecord, EncodingError)
			|| !TrySerializeTypeSchemaRecord(
				TypeSchema, TypeRecord, EncodingError)
			|| !TrySerializeRecord(EAngelscriptCacheRecordKind::ModuleState,
				ModuleState,
				FAngelscriptCacheRemainingRecordArchive::SerializeModuleState,
				StateRecord, EncodingError))
		{
			return EncodingError;
		}

		FCleanCaptureBuildDependencyResolver BuildDependencyResolver;
		const FAngelscriptCacheStableReference TypeDeclarationReference{
			EAngelscriptCacheReferenceKind::ScriptType,
			TypeDeclaration.StableKey,
			TypeDeclaration.SignatureHash,
		};
		BuildDependencyResolver.AddType(
			ScriptType,
			TypeDeclarationReference,
			TypeSchema.Layout.TypeLayoutHash);
		BuildDependencyResolver.AddDerivedStaticClassGlobal(
			GeneratedStaticClassGlobal, TypeDeclarationReference);
		BuildDependencyResolver.AddDerivedStaticClassFunction(
			GeneratedStaticClassFunction, TypeDeclarationReference);
		for (int32 PropertyIndex = 0;
			PropertyIndex < PropertyDeclarations.Num(); ++PropertyIndex)
		{
			BuildDependencyResolver.AddProperty(
				ScriptType,
				ScriptType->localProperties[static_cast<asUINT>(PropertyIndex)],
				{
					EAngelscriptCacheReferenceKind::ScriptProperty,
					PropertyDeclarations[PropertyIndex].StableKey,
					PropertyDeclarations[PropertyIndex].SignatureHash,
				},
				TypeSchema.OrderedProperties[PropertyIndex].
					PropertyLayoutFingerprint);
		}
		if (UserGlobal != nullptr)
		{
			check(Global != nullptr && HardValue != nullptr);
			BuildDependencyResolver.AddGlobal(
				UserGlobal,
				{
					EAngelscriptCacheReferenceKind::ScriptGlobal,
					GlobalDeclaration.StableKey,
					GlobalDeclaration.SignatureHash,
				},
				Global->StorageLayoutFingerprint,
				HardValue->HardValueHash);
		}

		for (FRootFunctionCapture& Capture : FunctionCaptures)
		{
			FFunctionArtifactStream ExecutionStream;
			asCWriter FunctionWriter(
				ScriptModule, &ExecutionStream, ScriptModule->engine, true);
			const int FunctionArtifactResult =
				FunctionWriter.WriteFunctionArtifact(Capture.Function);
			if (FunctionArtifactResult == asNOT_SUPPORTED)
			{
				return Failure(EAngelscriptCacheCleanCaptureError::NotCacheable,
					FString::Printf(
						TEXT("Function %s contains unsupported symbolic reference tables"),
						*Capture.Declaration.CanonicalDeclaration));
			}
			if (FunctionArtifactResult < 0 || ExecutionStream.Bytes.IsEmpty())
			{
				return Failure(
					EAngelscriptCacheCleanCaptureError::FunctionArtifactFailed,
					FString::Printf(
						TEXT("The maintained-fork artifact writer failed for %s with %d"),
						*Capture.Declaration.CanonicalDeclaration,
						FunctionArtifactResult));
			}

			TArray<uint8> DebugPayload;
			TArray<FAngelscriptCachedDebugSourceReference> DebugSources;
			FString DebugFailure;
			if (!TryBuildDebugPayload(
				*Module,
				*Capture.Function,
				SourceFile.SourceFileKey,
				DebugPayload,
				DebugSources,
				DebugFailure))
			{
				return Failure(EAngelscriptCacheCleanCaptureError::NotCacheable,
					MoveTemp(DebugFailure));
			}

			Capture.Debug.PayloadSchemaVersion =
				FAngelscriptCacheRemainingRecordArchive::
					DebugSidecarPayloadSchemaVersion;
			Capture.Debug.FunctionKey = FAngelscriptStableFunctionKey{
				Capture.Declaration.StableKey};
			Capture.Debug.Profile = Options.Profile;
			Capture.Debug.VmDebugCodecVersion =
				FAngelscriptFunctionArtifactCodec::DebugCodecVersion;
			Capture.Debug.Sources = MoveTemp(DebugSources);
			Capture.Debug.CanonicalDebugPayload = MoveTemp(DebugPayload);
			Capture.Debug.DebugHash =
				FAngelscriptArtifactIdentityBuilder::BuildFunctionContentHash(
					{}, Capture.Debug.CanonicalDebugPayload).Debug;
			if (!TrySerializeRecord(
				EAngelscriptCacheRecordKind::DebugSidecar,
				Capture.Debug,
				FAngelscriptCacheRemainingRecordArchive::SerializeDebugSidecar,
				Capture.DebugRecord,
				EncodingError))
			{
				return EncodingError;
			}

			FAngelscriptFunctionSourceDescriptor FunctionSource;
			FunctionSource.Kind = Capture.Declaration.EntityKind;
			FunctionSource.CanonicalSource = UTF8_TO_TCHAR(
				Capture.Function->scriptData->artifactCanonicalSource.AddressOf());
			FunctionSource.CanonicalOptions = Options.CanonicalCompileOptions;
			Capture.Body.PayloadSchemaVersion =
				FAngelscriptCacheRemainingRecordArchive::
					FunctionBodyPayloadSchemaVersion;
			Capture.Body.ModuleKey = ModuleKey;
			Capture.Body.Identity.FunctionKey = FAngelscriptStableFunctionKey{
				Capture.Declaration.StableKey};
			Capture.Body.Identity.Profile = Options.Profile;
			Capture.Body.ExpectedDeclarationAbi =
				Capture.Declaration.SignatureHash;
			Capture.Body.FunctionSourceDigest =
				FAngelscriptArtifactIdentityBuilder::BuildFunctionSourceDigest(
					FunctionSource);
			Capture.Body.InvocationKind = Capture.InvocationKind;
			Capture.Body.VmExecutionCodecVersion =
				FAngelscriptFunctionArtifactCodec::ExecutionCodecVersion;
			Capture.RawVmPayload = MoveTemp(ExecutionStream.Bytes);
			Capture.Body.DebugSidecar = Capture.DebugRecord.RecordId;
			Capture.Body.Identity.Content =
				FAngelscriptArtifactIdentityBuilder::BuildFunctionContentHash(
					Capture.RawVmPayload,
					Capture.Debug.CanonicalDebugPayload);
		}

		for (const FRootFunctionCapture& Capture : FunctionCaptures)
		{
			BuildDependencyResolver.AddFunction(
				Capture.Function,
				{
					EAngelscriptCacheReferenceKind::ScriptFunction,
					Capture.Declaration.StableKey,
					Capture.Declaration.SignatureHash,
				},
				Capture.Body.Identity.Content.Execution);
		}

		TArray<FAngelscriptCachedFunctionBody> FunctionAuthorityBodies;
		FunctionAuthorityBodies.Reserve(FunctionCaptures.Num());
		for (const FRootFunctionCapture& Capture : FunctionCaptures)
		{
			FunctionAuthorityBodies.Add(Capture.Body);
		}

		FAngelscriptCacheFunctionInputAuthorities FunctionAuthorities;
		FunctionAuthorities.ModuleInterface = &ModuleInterface;
		FunctionAuthorities.TypeSchemas =
			TConstArrayView<FAngelscriptCachedTypeSchema>(&TypeSchema, 1);
		FunctionAuthorities.ModuleState = &ModuleState;
		FunctionAuthorities.FunctionBodies = FunctionAuthorityBodies;
		FAngelscriptCacheEngineEnvironmentResolver EnvironmentSymbols(
			*ScriptModule->engine);
		FunctionAuthorities.ExternalSymbols = &EnvironmentSymbols;
		uint32 GraphCarriedDependencyFunctionCount = 0;
		for (FRootFunctionCapture& Capture : FunctionCaptures)
		{
			bool bUsedGraphCarriedDependencies = false;
			IdentityResult = CaptureActualDependencies(
				*Capture.Function, BuildDependencyResolver, ModuleKey,
				Capture.Body.Identity.FunctionKey, RestoredDependencies,
				Capture.Body.ActualDependencies,
				bUsedGraphCarriedDependencies);
			GraphCarriedDependencyFunctionCount +=
				bUsedGraphCarriedDependencies ? 1u : 0u;
			if (!IdentityResult.IsSuccess())
			{
				return ValidationFailure(
					EAngelscriptCacheCleanCaptureError::NotCacheable,
					*FString::Printf(
						TEXT("Function %s actual dependency capture"),
						*Capture.Declaration.CanonicalDeclaration),
					IdentityResult);
			}
			const FAngelscriptCacheFunctionInputResolution InputResolution =
				FAngelscriptCacheCompilerBridge::ResolveCurrentFunctionInput(
					Capture.Body,
					Capture.Body.FunctionSourceDigest,
					FunctionAuthorities);
			if ((InputResolution.Status
					!= EAngelscriptCacheFunctionInputStatus::ResolvedMatch
					&& InputResolution.Status
						!= EAngelscriptCacheFunctionInputStatus::ResolvedMismatch)
				|| InputResolution.CurrentInputDigest.Hash.IsZero())
			{
				return Failure(
					EAngelscriptCacheCleanCaptureError::NotCacheable,
					FString::Printf(
						TEXT("Function %s input dependencies could not be resolved after the complete current function table: Status=%u Missing=%d"),
						*Capture.Declaration.CanonicalDeclaration,
						static_cast<uint32>(InputResolution.Status),
						InputResolution.MissingDependencyOrdinal.IsSet()
							? static_cast<int32>(InputResolution.
								MissingDependencyOrdinal.GetValue()) : -1));
			}
			Capture.Body.FunctionInputDigest = InputResolution.CurrentInputDigest;
			FAngelscriptFunctionArtifactCodec ExecutionCodec(
				*ScriptModule, *ScriptModule->engine);
			FAngelscriptHash256 EncodedExecutionHash;
			const FAngelscriptCacheValidationResult ExecutionResult =
				ExecutionCodec.EncodeExecutionArtifact(
					Capture.RawVmPayload,
					ModuleKey,
					Capture.Body.ActualDependencies,
					Capture.Body.CanonicalExecutionPayload,
					EncodedExecutionHash);
			if (!ExecutionResult.IsSuccess()
				|| !(EncodedExecutionHash
					== Capture.Body.Identity.Content.Execution))
			{
				FString ExecutionDetail = FString::Printf(
					TEXT("Function %s stable execution envelope (%s dependencies)"),
					*Capture.Declaration.CanonicalDeclaration,
					bUsedGraphCarriedDependencies
						? TEXT("graph-carried") : TEXT("compiler-observed"));
				if (!ExecutionCodec.GetLastExecutionFailureDetail().IsEmpty())
				{
					ExecutionDetail += TEXT(": ");
					ExecutionDetail +=
						ExecutionCodec.GetLastExecutionFailureDetail();
				}
				return ValidationFailure(
					EAngelscriptCacheCleanCaptureError::FunctionArtifactFailed,
					*ExecutionDetail,
					ExecutionResult.IsSuccess()
						? FAngelscriptCacheValidationResult::AtStage(
							EAngelscriptCacheValidationError::DerivedHashMismatch,
							EAngelscriptCacheRecordKind::FunctionBody,
							EAngelscriptCacheValidationStage::OpaqueCodec)
						: ExecutionResult);
			}
			if (!TrySerializeRecord(
				EAngelscriptCacheRecordKind::FunctionBody,
				Capture.Body,
				FAngelscriptCacheRemainingRecordArchive::SerializeFunctionBody,
				Capture.BodyRecord,
				EncodingError))
			{
				return EncodingError;
			}
		}

		FAngelscriptCachedModuleSnapshot Snapshot;
		Snapshot.PayloadSchemaVersion =
			FAngelscriptCacheRemainingRecordArchive::ModuleSnapshotPayloadSchemaVersion;
		Snapshot.ModuleKey = ModuleKey;
		Snapshot.ModuleInterface = {ModuleKey, InterfaceRecord.RecordId};
		Snapshot.TypeSchemas.Add({TypeSchema.TypeKey, TypeRecord.RecordId});
		Snapshot.ModuleState = {ModuleKey, StateRecord.RecordId};
		for (const FRootFunctionCapture& Capture : FunctionCaptures)
		{
			Snapshot.FunctionBodies.Add({
				Capture.Body.Identity.FunctionKey,
				Capture.BodyRecord.RecordId});
		}
		Snapshot.FunctionBodies.Sort([](
			const FAngelscriptCachedFunctionBodyLink& Left,
			const FAngelscriptCachedFunctionBodyLink& Right)
		{
			return Left.FunctionKey.Hash < Right.FunctionKey.Hash;
		});
		if (!TrySerializeRecord(EAngelscriptCacheRecordKind::ModuleSnapshot,
			Snapshot,
			FAngelscriptCacheRemainingRecordArchive::SerializeModuleSnapshot,
			SnapshotRecord, EncodingError))
		{
			return EncodingError;
		}

		FAngelscriptCacheCleanModuleArtifacts CandidateArtifacts;
		CandidateArtifacts.ModuleKey = ModuleKey;
		CandidateArtifacts.CanonicalModuleName = Module->ModuleName;
		CandidateArtifacts.SourceSnapshot = SourceIndex.SourceSnapshot;
		CandidateArtifacts.SourceIndexRecordId = SourceRecord.RecordId;
		CandidateArtifacts.ModuleSnapshot = {ModuleKey, SnapshotRecord.RecordId};
		CandidateArtifacts.Records.Reserve(5 + FunctionCaptures.Num() * 2);
		CandidateArtifacts.Records.Add(MoveTemp(SourceRecord));
		CandidateArtifacts.Records.Add(MoveTemp(InterfaceRecord));
		CandidateArtifacts.Records.Add(MoveTemp(TypeRecord));
		CandidateArtifacts.Records.Add(MoveTemp(StateRecord));
		for (FRootFunctionCapture& Capture : FunctionCaptures)
		{
			CandidateArtifacts.Records.Add(MoveTemp(Capture.BodyRecord));
			CandidateArtifacts.Records.Add(MoveTemp(Capture.DebugRecord));
		}
		CandidateArtifacts.Records.Add(MoveTemp(SnapshotRecord));
		FAngelscriptCacheCleanCaptureResult Result =
			ValidateAndPromoteAngelscriptCleanCompiledModuleArtifacts(
				Module, Options, MoveTemp(CandidateArtifacts), OutArtifacts);
		if (Result.IsSuccess())
		{
			Result.GraphCarriedDependencyFunctionCount =
				GraphCarriedDependencyFunctionCount;
			Result.Detail = FString::Printf(
				TEXT("Captured root class %s with %d VM properties, %d pure constant user globals and %d stable functions (%u graph-carried dependency sets) as %u graph-validated Cache V2 records"),
				*ClassName,
				TypeSchema.OrderedProperties.Num(),
				UserGlobal != nullptr ? 1 : 0,
				FunctionCaptures.Num(),
				GraphCarriedDependencyFunctionCount,
				Result.ValidatedGraphRecordCount);
		}
		return Result;
	}
}

FAngelscriptCacheCleanCaptureResult
ValidateAndPromoteAngelscriptCleanCompiledModuleArtifacts(
	const TSharedRef<FAngelscriptModuleDesc>& Module,
	const FAngelscriptCacheCleanCaptureOptions& Options,
	FAngelscriptCacheCleanModuleArtifacts CandidateArtifacts,
	FAngelscriptCacheCleanModuleArtifacts& OutArtifacts)
{
	using namespace AngelscriptCacheCleanCapture_Private;

	OutArtifacts.Reset();
	uint32 ValidatedGraphRecordCount = 0;
	TArray<FAngelscriptFunctionArtifactIdentity>
		ValidatedFunctionArtifactIdentities;
	FAngelscriptCacheCleanCaptureResult Result;
	if (!TryValidateCapturedGraph(
		*Module,
		Options,
		CandidateArtifacts,
		ValidatedGraphRecordCount,
		ValidatedFunctionArtifactIdentities,
		Result))
	{
		return Result;
	}

	OutArtifacts = MoveTemp(CandidateArtifacts);
	OutArtifacts.ValidatedFunctionArtifactIdentities =
		MoveTemp(ValidatedFunctionArtifactIdentities);
	Result.ValidatedGraphRecordCount = ValidatedGraphRecordCount;
	Result.Detail = FString::Printf(
		TEXT("Validated and promoted module %s as %u pointer-free Cache V2 records"),
		*Module->ModuleName, Result.ValidatedGraphRecordCount);
	return Result;
}

FAngelscriptCacheCleanCaptureResult
OpenAngelscriptValidatedModuleGraphFromCleanArtifacts(
	const TSharedRef<FAngelscriptModuleDesc>& Module,
	const FAngelscriptCacheCleanCaptureOptions& Options,
	const FAngelscriptCacheCleanModuleArtifacts& Artifacts,
	const FAngelscriptCacheReadLimits& Limits,
	FAngelscriptCacheReadBudget& Budget,
	FAngelscriptValidatedModuleGraph& OutGraph)
{
	using namespace AngelscriptCacheCleanCapture_Private;
	FAngelscriptCacheCleanCaptureResult Result;
	if (!TryOpenCapturedGraph(
		*Module,
		Options,
		Artifacts,
		Limits,
		Budget,
		OutGraph,
		Result.ValidatedGraphRecordCount,
		Result))
	{
		return Result;
	}
	Result.Detail = FString::Printf(
		TEXT("Opened module %s as %u graph-validated Cache V2 records"),
		*Module->ModuleName,
		Result.ValidatedGraphRecordCount);
	return Result;
}

static FAngelscriptCacheCleanCaptureResult
CaptureAngelscriptCleanCompiledModuleImpl(
	const TSharedRef<FAngelscriptModuleDesc>& Module,
	const FAngelscriptCacheCleanCaptureOptions& Options,
	const FAngelscriptCachedSourceIndex* AuthoritativeSourceIndex,
	const IAngelscriptCacheRestoredFunctionDependencySource*
		RestoredDependencies,
	FAngelscriptCacheCleanModuleArtifacts& OutArtifacts)
{
	using namespace AngelscriptCacheCleanCapture_Private;

	OutArtifacts.Reset();
	if (Options.Compatibility.Hash.IsZero() || Options.Context.Hash.IsZero()
		|| Options.Profile.Hash.IsZero())
	{
		return Failure(EAngelscriptCacheCleanCaptureError::InvalidInput,
			TEXT("Compatibility, Context and Profile must be nonzero"));
	}
	if (Module->bCompileError || Module->ScriptModule == nullptr
		|| Module->ModuleName.IsEmpty())
	{
		return Failure(EAngelscriptCacheCleanCaptureError::InvalidInput,
			TEXT("The module is not a successful normal compiled module"));
	}
	const bool bGlobalFunctionVertical = Module->Enums.Num() <= 1
		&& Module->Classes.IsEmpty();
	const bool bRootClassPrimitiveVertical = Module->Enums.IsEmpty()
		&& Module->Classes.Num() == 1;
	const bool bClassGraphPrimitiveVertical = Module->Enums.IsEmpty()
		&& Module->Classes.Num() > 1;
	if (Module->Code.Num() != 1
		|| (!bGlobalFunctionVertical && !bRootClassPrimitiveVertical
			&& !bClassGraphPrimitiveVertical)
		|| !Module->Delegates.IsEmpty()
		|| !Module->ImportedModules.IsEmpty() || !Module->PostInitFunctions.IsEmpty())
	{
		return Failure(EAngelscriptCacheCleanCaptureError::NotCacheable,
			TEXT("Clean capture currently requires exactly one source and either zero-or-one enum with global functions or a reflected class graph, with no delegate, import or post-init declarations"));
	}

	const FAngelscriptModuleDesc::FCodeSection& Code = Module->Code[0];
	FAngelscriptVirtualPath VirtualPath;
	FString VirtualPathError;
	if (!FAngelscriptVirtualPath::TryParse(
		Code.VirtualPath, VirtualPath, &VirtualPathError))
	{
		return Failure(EAngelscriptCacheCleanCaptureError::NotCacheable,
			FString::Printf(TEXT("Invalid source virtual path: %s"),
				*VirtualPathError));
	}
	if (VirtualPath.GetSourceKind() == EAngelscriptSourceKind::Memory
		|| Code.AbsoluteFilename.IsEmpty())
	{
		return Failure(EAngelscriptCacheCleanCaptureError::NotCacheable,
			TEXT("The first clean-capture vertical requires a built-in disk source with exact raw bytes"));
	}
	FString LogicalMount;
	if (!TryBuildLogicalMount(VirtualPath, LogicalMount))
	{
		return Failure(EAngelscriptCacheCleanCaptureError::NotCacheable,
			TEXT("The source has no unambiguous logical disk mount"));
	}

	TArray<uint8> RawSourceBytes;
	if (!FFileHelper::LoadFileToArray(RawSourceBytes, *Code.AbsoluteFilename))
	{
		return Failure(EAngelscriptCacheCleanCaptureError::SourceReadFailed,
			FString::Printf(TEXT("Failed to read exact raw source bytes for %s"),
				*Code.VirtualPath));
	}
	const FAngelscriptHash256 RawContentHash = HashRawBytes(RawSourceBytes);

	FAngelscriptCachedSourceIndex SourceIndex;
	FAngelscriptCachedSourceFile SourceFile;
	TOptional<FAngelscriptStableModuleKey> ModuleKey;
	FAngelscriptCacheValidationResult IdentityResult;
	if (AuthoritativeSourceIndex != nullptr)
	{
		SourceIndex = *AuthoritativeSourceIndex;
		const FAngelscriptHash256 DeclaredSnapshot = SourceIndex.SourceSnapshot;
		IdentityResult =
			FAngelscriptCacheSemanticArchive::CanonicalizeSourceIndex(
				SourceIndex);
		if (!IdentityResult.IsSuccess()
			|| DeclaredSnapshot != SourceIndex.SourceSnapshot)
		{
			return ValidationFailure(
				EAngelscriptCacheCleanCaptureError::RecordEncodingFailed,
				TEXT("Authoritative SourceIndex"),
				IdentityResult.IsSuccess()
					? FAngelscriptCacheValidationResult(
						EAngelscriptCacheValidationError::SourceSnapshotMismatch,
						EAngelscriptCacheRecordKind::SourceIndex)
					: IdentityResult);
		}

		const FAngelscriptCachedSourceFile* MatchingFile = nullptr;
		const FAngelscriptCachedSourceMount* MatchingMount = nullptr;
		for (const FAngelscriptCachedSourceFile& Candidate : SourceIndex.Files)
		{
			if (Candidate.SourceKind
					== ToCachedSourceKind(VirtualPath.GetSourceKind())
				&& Candidate.RelativeLogicalPath.Equals(
					VirtualPath.GetRelativePath(), ESearchCase::CaseSensitive)
				&& Candidate.RawContentHash == RawContentHash)
			{
				if (MatchingFile != nullptr)
				{
					return Failure(
						EAngelscriptCacheCleanCaptureError::NotCacheable,
						TEXT("The authoritative SourceIndex maps the compiled source more than once"));
				}
				MatchingFile = &Candidate;
			}
		}
		if (MatchingFile != nullptr)
		{
			for (const FAngelscriptCachedSourceMount& Candidate
				: SourceIndex.Mounts)
			{
				if (Candidate.MountKey.Hash == MatchingFile->MountKey.Hash)
				{
					MatchingMount = &Candidate;
					break;
				}
			}
		}
		FString ExpectedVirtualPath = MatchingMount != nullptr
			? MatchingMount->LogicalMount : FString();
		if (!ExpectedVirtualPath.EndsWith(TEXT("/")))
		{
			ExpectedVirtualPath += TEXT("/");
		}
		ExpectedVirtualPath += VirtualPath.GetRelativePath();
		if (MatchingFile == nullptr || MatchingMount == nullptr
			|| !ExpectedVirtualPath.Equals(
				Code.VirtualPath, ESearchCase::CaseSensitive))
		{
			return Failure(
				EAngelscriptCacheCleanCaptureError::NotCacheable,
				TEXT("The compiled module source is absent from the authoritative SourceIndex"));
		}

		const TOptional<FAngelscriptStableModuleKey> DerivedModuleKey =
			FAngelscriptArtifactIdentityBuilder::TryBuildModuleKey(
				MatchingMount->LogicalMount,
				MatchingFile->RelativeLogicalPath,
				Module->ModuleName);
		if (!DerivedModuleKey.IsSet()
			|| DerivedModuleKey.GetValue() != MatchingFile->ModuleKey)
		{
			return Failure(
				EAngelscriptCacheCleanCaptureError::NotCacheable,
				TEXT("The authoritative SourceIndex ModuleKey does not own the compiled module"));
		}
		SourceFile = *MatchingFile;
		ModuleKey = MatchingFile->ModuleKey;
	}
	else
	{
		ModuleKey = FAngelscriptArtifactIdentityBuilder::TryBuildModuleKey(
			LogicalMount, VirtualPath.GetRelativePath(), Module->ModuleName);
		if (!ModuleKey.IsSet())
		{
			return Failure(EAngelscriptCacheCleanCaptureError::NotCacheable,
				TEXT("The module identity cannot be normalized"));
		}

		SourceIndex.PayloadSchemaVersion =
			FAngelscriptCacheSemanticArchive::SourceIndexPayloadSchemaVersion;
		SourceIndex.DiscoveryPolicy.PolicyVersion = 1;
		for (const FString& CompileOption : Options.CanonicalCompileOptions)
		{
			FAngelscriptCachedCanonicalOption& Option =
				SourceIndex.DiscoveryPolicy.Options.AddDefaulted_GetRef();
			Option.CanonicalKey = CompileOption;
			Option.ValueFingerprint = HashOneString(
				TEXT("cache-v2-clean-compile-option"), CompileOption);
		}

		FAngelscriptCachedSourceProvider Provider;
		Provider.ProviderKind =
			EAngelscriptCachedSourceProviderKind::BuiltInDisk;
		Provider.CanonicalImplementationIdentity =
			TEXT("Angelscript.BuiltInDiskSource.V1");
		Provider.IdentityFingerprint = HashOneString(
			TEXT("cache-v2-source-provider-identity"),
			Provider.CanonicalImplementationIdentity);
		Provider.VersionFingerprint = HashOneString(
			TEXT("cache-v2-source-provider-version"), TEXT("1"));
		TArray<FString> ProviderConfigurationInputs{
			LogicalMount, VirtualPath.GetRelativePath()};
		ProviderConfigurationInputs.Append(Options.CanonicalCompileOptions);
		Provider.ConfigurationFingerprint = HashStrings(
			TEXT("cache-v2-source-provider-configuration"),
			ProviderConfigurationInputs);
		Provider.ContentFingerprint = RawContentHash;
		Provider.CapabilityFlags = static_cast<uint32>(
			EAngelscriptCachedFingerprintCapabilityFlags::KnownMask);
		const FAngelscriptSourceProviderIdentityInput ProviderIdentity{
			Provider.ProviderKind, Provider.CanonicalImplementationIdentity,
			Provider.IdentityFingerprint};
		IdentityResult =
			FAngelscriptCacheSemanticArchive::TryBuildSourceProviderKey(
				ProviderIdentity, Provider.ProviderKey);
		if (!IdentityResult.IsSuccess())
		{
			return ValidationFailure(
				EAngelscriptCacheCleanCaptureError::RecordEncodingFailed,
				TEXT("Source provider key"), IdentityResult);
		}
		SourceIndex.Providers.Add(Provider);

		FAngelscriptCachedSourceMount Mount;
		Mount.SourceKind = ToCachedSourceKind(VirtualPath.GetSourceKind());
		Mount.LogicalMount = LogicalMount;
		Mount.ProviderKey = Provider.ProviderKey;
		Mount.RootConfigurationFingerprint = HashStrings(
			TEXT("cache-v2-source-mount-configuration"),
			ProviderConfigurationInputs);
		const FAngelscriptSourceMountIdentityInput MountIdentity{
			Mount.SourceKind, Mount.LogicalMount, Mount.ProviderKey};
		IdentityResult =
			FAngelscriptCacheSemanticArchive::TryBuildSourceMountKey(
				MountIdentity, Mount.MountKey);
		if (!IdentityResult.IsSuccess())
		{
			return ValidationFailure(
				EAngelscriptCacheCleanCaptureError::RecordEncodingFailed,
				TEXT("Source mount key"), IdentityResult);
		}
		SourceIndex.Mounts.Add(Mount);

		SourceFile.SourceKind = Mount.SourceKind;
		SourceFile.MountKey = Mount.MountKey;
		SourceFile.ProviderKey = Provider.ProviderKey;
		SourceFile.RelativeLogicalPath = VirtualPath.GetRelativePath();
		SourceFile.RawContentHash = RawContentHash;
		SourceFile.ModuleKey = ModuleKey.GetValue();
		const FAngelscriptSourceFileIdentityInput SourceFileIdentity{
			SourceFile.SourceKind, SourceFile.MountKey, SourceFile.ProviderKey,
			SourceFile.RelativeLogicalPath, SourceFile.GeneratedSourceKey};
		IdentityResult =
			FAngelscriptCacheSemanticArchive::TryBuildSourceFileKey(
				SourceFileIdentity, SourceFile.SourceFileKey);
		if (!IdentityResult.IsSuccess())
		{
			return ValidationFailure(
				EAngelscriptCacheCleanCaptureError::RecordEncodingFailed,
				TEXT("Source file key"), IdentityResult);
		}
		SourceIndex.Files.Add(SourceFile);
		IdentityResult = FAngelscriptCacheSemanticArchive::ComputeSourceSnapshot(
			SourceIndex, SourceIndex.SourceSnapshot);
		if (!IdentityResult.IsSuccess())
		{
			return ValidationFailure(
				EAngelscriptCacheCleanCaptureError::RecordEncodingFailed,
				TEXT("Source snapshot"), IdentityResult);
		}
	}

	if (bRootClassPrimitiveVertical)
	{
		return CaptureRootClassPrimitiveVertical(
			Module, Options, SourceIndex, SourceFile,
			ModuleKey.GetValue(), RestoredDependencies, OutArtifacts);
	}
	if (bClassGraphPrimitiveVertical)
	{
		return CaptureClassGraphPrimitiveVertical(
			Module, Options, SourceIndex, SourceFile,
			ModuleKey.GetValue(), RestoredDependencies, OutArtifacts);
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
			TEXT("The admitted clean-capture AS module shape is zero or one enum and one or more global functions with no globals/imports/object/typedef declarations"));
	}

	struct FFunctionCapture
	{
		asCScriptFunction* Function = nullptr;
		FAngelscriptCachedDeclaration Declaration;
		FAngelscriptCachedFunctionBody Body;
		FAngelscriptCachedDebugSidecar Debug;
		FAngelscriptPreparedRecord BodyRecord;
		FAngelscriptPreparedRecord DebugRecord;
	};
	FAngelscriptCacheCurrentModuleAuthority CurrentAuthority;
	const FAngelscriptCacheCleanCaptureResult AuthorityResult =
		BuildAngelscriptCacheCurrentModuleAuthority(
			Module,
			Options,
			ModuleKey.GetValue(),
			CurrentAuthority);
	if (!AuthorityResult.IsSuccess())
	{
		return AuthorityResult;
	}
	if (CurrentAuthority.TypeSchemas.Num() > 1)
	{
		return Failure(EAngelscriptCacheCleanCaptureError::NotCacheable,
			TEXT("The global-function authority producer returned more than one TypeSchema"));
	}

	FAngelscriptCachedModuleInterface ModuleInterface =
		MoveTemp(CurrentAuthority.ModuleInterface);
	TOptional<FAngelscriptCachedTypeSchema> TypeSchema;
	if (CurrentAuthority.TypeSchemas.Num() == 1)
	{
		TypeSchema = MoveTemp(CurrentAuthority.TypeSchemas[0]);
	}
	FAngelscriptCachedModuleState ModuleState =
		MoveTemp(CurrentAuthority.ModuleState);
	TArray<FFunctionCapture> FunctionCaptures;
	FunctionCaptures.Reserve(CurrentAuthority.Functions.Num());
	for (FAngelscriptCacheCurrentFunctionAuthority& FunctionAuthority
		: CurrentAuthority.Functions)
	{
		asCScriptFunction* Function = FunctionAuthority.Function;
		if (Function->scriptData == nullptr
			|| Function->scriptData->artifactCanonicalSource.GetLength() == 0)
		{
			return Failure(EAngelscriptCacheCleanCaptureError::NotCacheable,
				TEXT("The maintained fork did not retain every function canonical token slice"));
		}

		FFunctionCapture& Capture = FunctionCaptures.AddDefaulted_GetRef();
		Capture.Function = Function;
		Capture.Declaration = MoveTemp(FunctionAuthority.Declaration);
	}

	FCleanCaptureBuildDependencyResolver BuildDependencyResolver;
	for (const FFunctionCapture& Capture : FunctionCaptures)
	{
		BuildDependencyResolver.AddFunction(
			Capture.Function,
			{
				EAngelscriptCacheReferenceKind::ScriptFunction,
				Capture.Declaration.StableKey,
				Capture.Declaration.SignatureHash,
			});
	}

	FAngelscriptPreparedRecord SourceRecord;
	FAngelscriptPreparedRecord InterfaceRecord;
	TOptional<FAngelscriptPreparedRecord> TypeRecord;
	FAngelscriptPreparedRecord StateRecord;
	FAngelscriptPreparedRecord SnapshotRecord;
	FAngelscriptCacheCleanCaptureResult EncodingError;
	if (!TrySerializeRecord(EAngelscriptCacheRecordKind::SourceIndex,
		SourceIndex, FAngelscriptCacheSemanticArchive::SerializeSourceIndex,
		SourceRecord, EncodingError)
		|| !TrySerializeRecord(EAngelscriptCacheRecordKind::ModuleInterface,
			ModuleInterface,
			FAngelscriptCacheSemanticArchive::SerializeModuleInterface,
			InterfaceRecord, EncodingError)
		|| !TrySerializeRecord(EAngelscriptCacheRecordKind::ModuleState,
			ModuleState,
			FAngelscriptCacheRemainingRecordArchive::SerializeModuleState,
			StateRecord, EncodingError))
	{
		return EncodingError;
	}
	if (TypeSchema.IsSet())
	{
		TypeRecord.Emplace();
		if (!TrySerializeTypeSchemaRecord(
			TypeSchema.GetValue(), TypeRecord.GetValue(), EncodingError))
		{
			return EncodingError;
		}
	}

	uint32 GraphCarriedDependencyFunctionCount = 0;
	for (FFunctionCapture& Capture : FunctionCaptures)
	{
		FFunctionArtifactStream ExecutionStream;
		asCWriter FunctionWriter(
			ScriptModule, &ExecutionStream, ScriptModule->engine, true);
		const int FunctionArtifactResult =
			FunctionWriter.WriteFunctionArtifact(Capture.Function);
		if (FunctionArtifactResult == asNOT_SUPPORTED)
		{
			return Failure(EAngelscriptCacheCleanCaptureError::NotCacheable,
				FString::Printf(
					TEXT("Function %s contains symbolic reference tables not supported by the admitted cold-capture vertical"),
					*Capture.Declaration.CanonicalDeclaration));
		}
		if (FunctionArtifactResult < 0 || ExecutionStream.Bytes.IsEmpty())
		{
			return Failure(
				EAngelscriptCacheCleanCaptureError::FunctionArtifactFailed,
				FString::Printf(
					TEXT("The maintained-fork artifact writer failed for %s with %d"),
					*Capture.Declaration.CanonicalDeclaration,
					FunctionArtifactResult));
		}

		TArray<uint8> DebugPayload;
		TArray<FAngelscriptCachedDebugSourceReference> DebugSources;
		FString DebugFailure;
		if (!TryBuildDebugPayload(
			*Module,
			*Capture.Function,
			SourceFile.SourceFileKey,
			DebugPayload,
			DebugSources,
			DebugFailure))
		{
			return Failure(EAngelscriptCacheCleanCaptureError::NotCacheable,
				MoveTemp(DebugFailure));
		}

		Capture.Debug.PayloadSchemaVersion =
			FAngelscriptCacheRemainingRecordArchive::DebugSidecarPayloadSchemaVersion;
		Capture.Debug.FunctionKey = FAngelscriptStableFunctionKey{
			Capture.Declaration.StableKey};
		Capture.Debug.Profile = Options.Profile;
		Capture.Debug.VmDebugCodecVersion =
			FAngelscriptFunctionArtifactCodec::DebugCodecVersion;
		Capture.Debug.Sources = MoveTemp(DebugSources);
		Capture.Debug.CanonicalDebugPayload = MoveTemp(DebugPayload);
		Capture.Debug.DebugHash =
			FAngelscriptArtifactIdentityBuilder::BuildFunctionContentHash(
				{}, Capture.Debug.CanonicalDebugPayload).Debug;
		if (!TrySerializeRecord(
			EAngelscriptCacheRecordKind::DebugSidecar,
			Capture.Debug,
			FAngelscriptCacheRemainingRecordArchive::SerializeDebugSidecar,
			Capture.DebugRecord,
			EncodingError))
		{
			return EncodingError;
		}

		FAngelscriptFunctionSourceDescriptor FunctionSource;
		FunctionSource.Kind = EAngelscriptArtifactEntityKind::GlobalFunction;
		FunctionSource.CanonicalSource = UTF8_TO_TCHAR(
			Capture.Function->scriptData->artifactCanonicalSource.AddressOf());
		FunctionSource.CanonicalOptions = Options.CanonicalCompileOptions;
		Capture.Body.PayloadSchemaVersion =
			FAngelscriptCacheRemainingRecordArchive::FunctionBodyPayloadSchemaVersion;
		Capture.Body.ModuleKey = ModuleKey.GetValue();
		Capture.Body.Identity.FunctionKey = FAngelscriptStableFunctionKey{
			Capture.Declaration.StableKey};
		Capture.Body.Identity.Profile = Options.Profile;
		Capture.Body.ExpectedDeclarationAbi =
			Capture.Declaration.SignatureHash;
		Capture.Body.FunctionSourceDigest =
			FAngelscriptArtifactIdentityBuilder::BuildFunctionSourceDigest(
				FunctionSource);
		bool bUsedGraphCarriedDependencies = false;
		IdentityResult = CaptureActualDependencies(
			*Capture.Function, BuildDependencyResolver,
			ModuleKey.GetValue(), Capture.Body.Identity.FunctionKey,
			RestoredDependencies, Capture.Body.ActualDependencies,
			bUsedGraphCarriedDependencies);
		GraphCarriedDependencyFunctionCount +=
			bUsedGraphCarriedDependencies ? 1u : 0u;
		if (!IdentityResult.IsSuccess())
		{
			return ValidationFailure(
				EAngelscriptCacheCleanCaptureError::NotCacheable,
				TEXT("Function actual dependency capture"), IdentityResult);
		}
		FAngelscriptCacheFunctionInputAuthorities FunctionAuthorities;
		FunctionAuthorities.ModuleInterface = &ModuleInterface;
		if (TypeSchema.IsSet())
		{
			FunctionAuthorities.TypeSchemas =
				TConstArrayView<FAngelscriptCachedTypeSchema>(
					&TypeSchema.GetValue(), 1);
		}
		FunctionAuthorities.ModuleState = &ModuleState;
		FAngelscriptCacheEngineEnvironmentResolver EnvironmentSymbols(
			*ScriptModule->engine);
		FunctionAuthorities.ExternalSymbols = &EnvironmentSymbols;
		const FAngelscriptCacheFunctionInputResolution InputResolution =
			FAngelscriptCacheCompilerBridge::ResolveCurrentFunctionInput(
				Capture.Body,
				Capture.Body.FunctionSourceDigest,
				FunctionAuthorities);
		if ((InputResolution.Status
				!= EAngelscriptCacheFunctionInputStatus::ResolvedMatch
				&& InputResolution.Status
					!= EAngelscriptCacheFunctionInputStatus::ResolvedMismatch)
			|| InputResolution.CurrentInputDigest.Hash.IsZero())
		{
			return Failure(
				EAngelscriptCacheCleanCaptureError::NotCacheable,
				FString::Printf(
					TEXT("Function input dependencies could not be resolved after authoritative declaration/type state: Status=%u Missing=%d"),
					static_cast<uint32>(InputResolution.Status),
					InputResolution.MissingDependencyOrdinal.IsSet()
						? static_cast<int32>(InputResolution.
							MissingDependencyOrdinal.GetValue()) : -1));
		}
		Capture.Body.FunctionInputDigest =
			InputResolution.CurrentInputDigest;
		Capture.Body.InvocationKind =
			EAngelscriptCachedFunctionInvocationKind::GlobalFunction;
		Capture.Body.VmExecutionCodecVersion =
			FAngelscriptFunctionArtifactCodec::ExecutionCodecVersion;
		FAngelscriptFunctionArtifactCodec ExecutionCodec(
			*ScriptModule, *ScriptModule->engine);
		FAngelscriptHash256 EncodedExecutionHash;
		const FAngelscriptCacheValidationResult ExecutionResult =
			ExecutionCodec.EncodeExecutionArtifact(
				ExecutionStream.Bytes,
				ModuleKey.GetValue(),
				Capture.Body.ActualDependencies,
				Capture.Body.CanonicalExecutionPayload,
				EncodedExecutionHash);
		if (!ExecutionResult.IsSuccess())
		{
			return ValidationFailure(
				EAngelscriptCacheCleanCaptureError::FunctionArtifactFailed,
				TEXT("Function stable execution envelope"),
				ExecutionResult);
		}
		Capture.Body.DebugSidecar = Capture.DebugRecord.RecordId;
		Capture.Body.Identity.Content.Execution = EncodedExecutionHash;
		Capture.Body.Identity.Content.Debug = Capture.Debug.DebugHash;
		if (!TrySerializeRecord(
			EAngelscriptCacheRecordKind::FunctionBody,
			Capture.Body,
			FAngelscriptCacheRemainingRecordArchive::SerializeFunctionBody,
			Capture.BodyRecord,
			EncodingError))
		{
			return EncodingError;
		}
	}

	FAngelscriptCachedModuleSnapshot Snapshot;
	Snapshot.PayloadSchemaVersion =
		FAngelscriptCacheRemainingRecordArchive::ModuleSnapshotPayloadSchemaVersion;
	Snapshot.ModuleKey = ModuleKey.GetValue();
	Snapshot.ModuleInterface = {ModuleKey.GetValue(), InterfaceRecord.RecordId};
	if (TypeSchema.IsSet() && TypeRecord.IsSet())
	{
		Snapshot.TypeSchemas.Add({
			TypeSchema->TypeKey, TypeRecord->RecordId});
	}
	Snapshot.ModuleState = {ModuleKey.GetValue(), StateRecord.RecordId};
	for (const FFunctionCapture& Capture : FunctionCaptures)
	{
		Snapshot.FunctionBodies.Add({
			Capture.Body.Identity.FunctionKey,
			Capture.BodyRecord.RecordId});
	}
	Snapshot.FunctionBodies.Sort([](
		const FAngelscriptCachedFunctionBodyLink& Left,
		const FAngelscriptCachedFunctionBodyLink& Right)
	{
		return Left.FunctionKey.Hash < Right.FunctionKey.Hash;
	});
	if (!TrySerializeRecord(EAngelscriptCacheRecordKind::ModuleSnapshot,
		Snapshot,
		FAngelscriptCacheRemainingRecordArchive::SerializeModuleSnapshot,
		SnapshotRecord, EncodingError))
	{
		return EncodingError;
	}

	FAngelscriptCacheCleanModuleArtifacts CandidateArtifacts;
	CandidateArtifacts.ModuleKey = ModuleKey.GetValue();
	CandidateArtifacts.CanonicalModuleName = Module->ModuleName;
	CandidateArtifacts.SourceSnapshot = SourceIndex.SourceSnapshot;
	CandidateArtifacts.SourceIndexRecordId = SourceRecord.RecordId;
	CandidateArtifacts.ModuleSnapshot = {
		ModuleKey.GetValue(), SnapshotRecord.RecordId};
	CandidateArtifacts.Records.Reserve(
		4 + (TypeRecord.IsSet() ? 1 : 0) + FunctionCaptures.Num() * 2);
	CandidateArtifacts.Records.Add(MoveTemp(SourceRecord));
	CandidateArtifacts.Records.Add(MoveTemp(InterfaceRecord));
	if (TypeRecord.IsSet())
	{
		CandidateArtifacts.Records.Add(MoveTemp(TypeRecord.GetValue()));
	}
	CandidateArtifacts.Records.Add(MoveTemp(StateRecord));
	for (FFunctionCapture& Capture : FunctionCaptures)
	{
		CandidateArtifacts.Records.Add(MoveTemp(Capture.BodyRecord));
	}
	for (FFunctionCapture& Capture : FunctionCaptures)
	{
		CandidateArtifacts.Records.Add(MoveTemp(Capture.DebugRecord));
	}
	CandidateArtifacts.Records.Add(MoveTemp(SnapshotRecord));

	FAngelscriptCacheCleanCaptureResult Result =
		ValidateAndPromoteAngelscriptCleanCompiledModuleArtifacts(
			Module, Options, MoveTemp(CandidateArtifacts), OutArtifacts);
	if (!Result.IsSuccess())
	{
		return Result;
	}
	Result.Detail = FString::Printf(
		TEXT("Captured and graph-validated module %s with %d functions (%u graph-carried dependency sets) as %u pointer-free Cache V2 records"),
		*Module->ModuleName,
		FunctionCaptures.Num(),
		GraphCarriedDependencyFunctionCount,
		Result.ValidatedGraphRecordCount);
	Result.GraphCarriedDependencyFunctionCount =
		GraphCarriedDependencyFunctionCount;
	return Result;
}

FAngelscriptCacheCleanCaptureResult CaptureAngelscriptCleanCompiledModule(
	const TSharedRef<FAngelscriptModuleDesc>& Module,
	const FAngelscriptCacheCleanCaptureOptions& Options,
	FAngelscriptCacheCleanModuleArtifacts& OutArtifacts)
{
	return CaptureAngelscriptCleanCompiledModuleImpl(
		Module, Options, nullptr, nullptr, OutArtifacts);
}

FAngelscriptCacheCleanCaptureResult CaptureAngelscriptCleanCompiledModule(
	const TSharedRef<FAngelscriptModuleDesc>& Module,
	const FAngelscriptCacheCleanCaptureOptions& Options,
	const FAngelscriptCachedSourceIndex& AuthoritativeSourceIndex,
	FAngelscriptCacheCleanModuleArtifacts& OutArtifacts)
{
	return CaptureAngelscriptCleanCompiledModuleImpl(
		Module, Options, &AuthoritativeSourceIndex, nullptr, OutArtifacts);
}

FAngelscriptCacheCleanCaptureResult CaptureAngelscriptCleanCompiledModule(
	const TSharedRef<FAngelscriptModuleDesc>& Module,
	const FAngelscriptCacheCleanCaptureOptions& Options,
	const FAngelscriptCachedSourceIndex& AuthoritativeSourceIndex,
	const IAngelscriptCacheRestoredFunctionDependencySource*
		RestoredDependencies,
	FAngelscriptCacheCleanModuleArtifacts& OutArtifacts)
{
	return CaptureAngelscriptCleanCompiledModuleImpl(
		Module, Options, &AuthoritativeSourceIndex, RestoredDependencies,
		OutArtifacts);
}

FAngelscriptCacheCleanCaptureResult PrepareAngelscriptCacheColdGeneration(
	const FAngelscriptCacheCleanModuleArtifacts& Artifacts,
	const FAngelscriptCacheCleanCaptureOptions& Options,
	const FAngelscriptCachePackPolicy& PackPolicy,
	IAngelscriptCacheStorageCodec& Codec,
	FAngelscriptCachePreparedColdGeneration& OutGeneration)
{
	return PrepareAngelscriptCacheColdGeneration(
		TConstArrayView<FAngelscriptCacheCleanModuleArtifacts>(&Artifacts, 1),
		Options, PackPolicy, Codec, OutGeneration);
}

FAngelscriptCacheCleanCaptureResult PrepareAngelscriptCacheColdGeneration(
	const TConstArrayView<FAngelscriptCacheCleanModuleArtifacts> Modules,
	const FAngelscriptCacheCleanCaptureOptions& Options,
	const FAngelscriptCachePackPolicy& PackPolicy,
	IAngelscriptCacheStorageCodec& Codec,
	FAngelscriptCachePreparedColdGeneration& OutGeneration)
{
	using namespace AngelscriptCacheCleanCapture_Private;

	OutGeneration.Reset();
	if (Modules.IsEmpty()
		|| Options.Compatibility.Hash.IsZero()
		|| Options.Context.Hash.IsZero()
		|| Options.Profile.Hash.IsZero())
	{
		return Failure(EAngelscriptCacheCleanCaptureError::InvalidInput,
			TEXT("Cold-generation preparation received an empty module set or incomplete environment identity"));
	}

	TArray<const FAngelscriptCacheCleanModuleArtifacts*> SortedModules;
	SortedModules.Reserve(Modules.Num());
	TArray<FAngelscriptPreparedRecord> MergedRecords;
	int32 SubmittedRecordCount = 0;
	const FAngelscriptHash256 SharedSourceSnapshot = Modules[0].SourceSnapshot;
	const FAngelscriptCacheRecordId SharedSourceIndexRecordId =
		Modules[0].SourceIndexRecordId;
	for (const FAngelscriptCacheCleanModuleArtifacts& Artifacts : Modules)
	{
		if (Artifacts.ModuleKey.Hash.IsZero()
			|| Artifacts.SourceSnapshot.IsZero()
			|| Artifacts.SourceIndexRecordId.Kind
				!= EAngelscriptCacheRecordKind::SourceIndex
			|| Artifacts.SourceIndexRecordId.ContentHash.IsZero()
			|| Artifacts.ModuleSnapshot.ModuleKey != Artifacts.ModuleKey
			|| Artifacts.ModuleSnapshot.RecordId.Kind
				!= EAngelscriptCacheRecordKind::ModuleSnapshot
			|| Artifacts.ModuleSnapshot.RecordId.ContentHash.IsZero()
			|| Artifacts.Records.IsEmpty())
		{
			return Failure(EAngelscriptCacheCleanCaptureError::InvalidInput,
				TEXT("Cold-generation preparation received incomplete module artifacts"));
		}
		if (!(Artifacts.SourceSnapshot == SharedSourceSnapshot)
			|| !(Artifacts.SourceIndexRecordId == SharedSourceIndexRecordId))
		{
			return Failure(EAngelscriptCacheCleanCaptureError::InvalidInput,
				TEXT("Cold-generation modules do not share one authoritative source snapshot and SourceIndex"));
		}
		SortedModules.Add(&Artifacts);
		SubmittedRecordCount += Artifacts.Records.Num();
		MergedRecords.Append(Artifacts.Records);
	}
	SortedModules.Sort([](
		const FAngelscriptCacheCleanModuleArtifacts& A,
		const FAngelscriptCacheCleanModuleArtifacts& B)
	{
		return A.ModuleKey.Hash < B.ModuleKey.Hash;
	});
	for (int32 Index = 1; Index < SortedModules.Num(); ++Index)
	{
		if (SortedModules[Index - 1]->ModuleKey
			== SortedModules[Index]->ModuleKey)
		{
			return Failure(EAngelscriptCacheCleanCaptureError::InvalidInput,
				TEXT("Cold-generation preparation received a duplicate ModuleKey"));
		}
	}

	MergedRecords.Sort([](
		const FAngelscriptPreparedRecord& A,
		const FAngelscriptPreparedRecord& B)
	{
		return A.RecordId < B.RecordId;
	});
	TArray<FAngelscriptPreparedRecord> UniqueRecords;
	UniqueRecords.Reserve(MergedRecords.Num());
	for (const FAngelscriptPreparedRecord& Record : MergedRecords)
	{
		if (!UniqueRecords.IsEmpty()
			&& UniqueRecords.Last().RecordId == Record.RecordId)
		{
			if (UniqueRecords.Last().CanonicalPayload != Record.CanonicalPayload)
			{
				return Failure(EAngelscriptCacheCleanCaptureError::InvalidInput,
					FString::Printf(
						TEXT("Cold-generation RecordId collision has conflicting payloads: Kind=%u Hash=%s"),
						static_cast<uint32>(Record.RecordId.Kind),
						*Record.RecordId.ContentHash.ToHexString()));
			}
			continue;
		}
		UniqueRecords.Add(Record);
	}

	FAngelscriptCachePreparedColdGeneration Candidate;
	const FAngelscriptCacheValidationResult PackResult =
		BuildAngelscriptCachePacks(
			UniqueRecords, PackPolicy, Codec, Candidate.Packs);
	if (!PackResult.IsSuccess())
	{
		return ValidationFailure(
			EAngelscriptCacheCleanCaptureError::PackBuildFailed,
			TEXT("Pack build"), PackResult);
	}

	FAngelscriptCacheGenerationManifest Manifest;
	Manifest.ManifestSchemaVersion =
		FAngelscriptCacheManifestPackArchive::ManifestSchemaVersion;
	Manifest.Compatibility = Options.Compatibility;
	Manifest.Context = Options.Context;
	Manifest.Profile = Options.Profile;
	Manifest.SourceSnapshot = SharedSourceSnapshot;
	Manifest.SourceIndexRecordId = SharedSourceIndexRecordId;
	for (const FAngelscriptCacheCleanModuleArtifacts* Artifacts : SortedModules)
	{
		Manifest.ModuleSnapshots.Add(Artifacts->ModuleSnapshot);
	}
	for (const FAngelscriptEncodedPack& Pack : Candidate.Packs)
	{
		for (const FAngelscriptCachePackIndexEntry& Entry : Pack.Index)
		{
			Manifest.Records.Add({Entry.RecordId, LocationFromPack(Pack, Entry)});
		}
	}
	Manifest.Records.Sort([](
		const FAngelscriptCacheRecordIndexEntry& A,
		const FAngelscriptCacheRecordIndexEntry& B)
	{
		return A.RecordId < B.RecordId;
	});

	FAngelscriptEncodedCacheGenerationManifest Encoded;
	const FAngelscriptCacheValidationResult ManifestResult =
		EncodeAngelscriptCacheGenerationManifest(Manifest, Encoded);
	if (!ManifestResult.IsSuccess())
	{
		return ValidationFailure(
			EAngelscriptCacheCleanCaptureError::ManifestBuildFailed,
			TEXT("Manifest encode"), ManifestResult);
	}

	Candidate.Manifest = MoveTemp(Manifest);
	Candidate.EncodedManifest = MoveTemp(Encoded);
	OutGeneration = MoveTemp(Candidate);
	FAngelscriptCacheCleanCaptureResult Result;
	Result.Detail = FString::Printf(
		TEXT("Prepared Generation %s with %d module(s), %d Pack(s), %d unique record(s), and %d deduplicated record(s)"),
		*OutGeneration.EncodedManifest.ComputedGenerationId.ToHexString(),
		OutGeneration.Manifest.ModuleSnapshots.Num(), OutGeneration.Packs.Num(),
		OutGeneration.Manifest.Records.Num(),
		SubmittedRecordCount - UniqueRecords.Num());
	return Result;
}
