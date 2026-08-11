#include "Cache/AngelscriptCacheTypeSchema.h"

#include "Cache/Private/AngelscriptCacheCanonicalCodec.h"
#include "Cache/Private/AngelscriptCacheTypeSchemaCodec.h"
#include "Internationalization/TextChar.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_config.h"
#include "EndAngelscriptHeaders.h"

namespace AngelscriptCacheTypeSchemaProducer_Private
{
	using AngelscriptCacheCanonicalCodec_Private::FWriter;

	static FAngelscriptCacheValidationResult Failure(
		const EAngelscriptCacheValidationError Error)
	{
		return FAngelscriptCacheValidationResult(
			Error, EAngelscriptCacheRecordKind::TypeSchema, 0);
	}

	static FAngelscriptCacheValidationResult FailureAt(
		const EAngelscriptCacheValidationError Error,
		FAngelscriptTypeSchemaFieldCoordinate* OutFailureCoordinate,
		const EAngelscriptTypeSchemaCapturedField Field,
		const uint32 Primary = MAX_uint32,
		const uint32 Secondary = MAX_uint32,
		const uint32 Tertiary = MAX_uint32)
	{
		if (OutFailureCoordinate != nullptr)
		{
			*OutFailureCoordinate = {Field, Primary, Secondary, Tertiary};
		}
		return Failure(Error);
	}

	static int32 CompareHash(
		const FAngelscriptHash256& A,
		const FAngelscriptHash256& B)
	{
		return A == B ? 0 : (A < B ? -1 : 1);
	}

	static int32 CompareString(const FString& A, const FString& B)
	{
		return FAngelscriptArtifactCanonicalWriter::CompareCanonicalUtf8Strings(A, B);
	}

	static FAngelscriptCacheValidationResult ValidateString(
		const FString& Value,
		const bool bRequired)
	{
		if (bRequired && Value.IsEmpty())
		{
			return Failure(EAngelscriptCacheValidationError::InvalidPresence);
		}
		for (int32 Index = 0; Index < Value.Len(); ++Index)
		{
			const uint32 CodeUnit = static_cast<uint32>(Value[Index]);
			if (CodeUnit == 0)
			{
				return Failure(EAngelscriptCacheValidationError::EmbeddedNul);
			}
			if constexpr (sizeof(TCHAR) == 2)
			{
				if (CodeUnit >= 0xd800 && CodeUnit <= 0xdbff)
				{
					if (Index + 1 >= Value.Len())
					{
						return Failure(EAngelscriptCacheValidationError::InvalidUtf8);
					}
					const uint32 Low = static_cast<uint32>(Value[++Index]);
					if (Low < 0xdc00 || Low > 0xdfff)
					{
						return Failure(EAngelscriptCacheValidationError::InvalidUtf8);
					}
				}
				else if (CodeUnit >= 0xdc00 && CodeUnit <= 0xdfff)
				{
					return Failure(EAngelscriptCacheValidationError::InvalidUtf8);
				}
			}
			else if (CodeUnit > 0x10ffff
				|| (CodeUnit >= 0xd800 && CodeUnit <= 0xdfff))
			{
				return Failure(EAngelscriptCacheValidationError::InvalidUtf8);
			}
		}
		return {};
	}

	static FAngelscriptCacheValidationResult ValidateStableReference(
		const FAngelscriptCacheStableReference& Value)
	{
		const uint8 Kind = static_cast<uint8>(Value.Kind);
		if (Kind < 1 || Kind > 9)
		{
			return Failure(EAngelscriptCacheValidationError::UnknownEnumValue);
		}
		if (Value.StableKey.IsZero())
		{
			return Failure(EAngelscriptCacheValidationError::ZeroStableKey);
		}
		const bool bRequiresAbi = Kind <= static_cast<uint8>(
			EAngelscriptCacheReferenceKind::EnvironmentSymbol);
		if (bRequiresAbi && Value.ExpectedAbi.IsZero())
		{
			return Failure(EAngelscriptCacheValidationError::MissingExpectedAbi);
		}
		if (!bRequiresAbi && !Value.ExpectedAbi.IsZero())
		{
			return Failure(EAngelscriptCacheValidationError::ForbiddenExpectedAbi);
		}
		return {};
	}

	static FAngelscriptCacheValidationResult ValidateDataType(
		const FAngelscriptCachedDataType& Value,
		const uint64 Depth = 1)
	{
		if (Depth > FAngelscriptCacheReadLimits::DefaultMaxNestingDepth)
		{
			return Failure(EAngelscriptCacheValidationError::NestingDepthExceeded);
		}
		const uint8 Kind = static_cast<uint8>(Value.Kind);
		if (Kind < 1 || Kind > 4)
		{
			return Failure(EAngelscriptCacheValidationError::UnknownEnumValue);
		}
		const uint32 KnownFlags = static_cast<uint32>(
			EAngelscriptCachedTypeQualifierFlags::KnownMask);
		if ((Value.QualifierFlags & ~KnownFlags) != 0)
		{
			return Failure(EAngelscriptCacheValidationError::UnknownFlags);
		}
		const uint32 Auto = static_cast<uint32>(EAngelscriptCachedTypeQualifierFlags::Auto);
		const uint32 Handle = static_cast<uint32>(
			EAngelscriptCachedTypeQualifierFlags::ObjectHandle);
		const uint32 ConstHandle = static_cast<uint32>(
			EAngelscriptCachedTypeQualifierFlags::ConstHandle);
		if ((Value.Kind == EAngelscriptCachedDataTypeKind::Auto)
			!= ((Value.QualifierFlags & Auto) != 0)
			|| ((Value.QualifierFlags & ConstHandle) != 0
				&& (Value.QualifierFlags & Handle) == 0))
		{
			return Failure(EAngelscriptCacheValidationError::InvalidQualifierCombination);
		}
		switch (Value.Kind)
		{
		case EAngelscriptCachedDataTypeKind::Primitive:
			if (Value.Primitive == EAngelscriptCachedPrimitiveType::Invalid
				|| Value.TypeReference.IsSet())
			{
				return Failure(EAngelscriptCacheValidationError::InvalidPresence);
			}
			if (static_cast<uint8>(Value.Primitive) > 12)
			{
				return Failure(EAngelscriptCacheValidationError::UnknownEnumValue);
			}
			break;
		case EAngelscriptCachedDataTypeKind::ScriptType:
		case EAngelscriptCachedDataTypeKind::EnvironmentType:
			if (Value.Primitive != EAngelscriptCachedPrimitiveType::Invalid
				|| !Value.TypeReference.IsSet())
			{
				return Failure(EAngelscriptCacheValidationError::InvalidPresence);
			}
			if ((Value.Kind == EAngelscriptCachedDataTypeKind::ScriptType
					&& Value.TypeReference->Kind
						!= EAngelscriptCacheReferenceKind::ScriptType)
				|| (Value.Kind == EAngelscriptCachedDataTypeKind::EnvironmentType
					&& Value.TypeReference->Kind
						!= EAngelscriptCacheReferenceKind::EnvironmentSymbol))
			{
				return Failure(EAngelscriptCacheValidationError::WrongReferenceKind);
			}
			if (const FAngelscriptCacheValidationResult Result =
				ValidateStableReference(Value.TypeReference.GetValue()); !Result.IsSuccess())
			{
				return Result;
			}
			break;
		case EAngelscriptCachedDataTypeKind::Auto:
			if (Value.Primitive != EAngelscriptCachedPrimitiveType::Invalid
				|| Value.TypeReference.IsSet() || Value.QualifierFlags != Auto)
			{
				return Failure(
					EAngelscriptCacheValidationError::InvalidQualifierCombination);
			}
			break;
		default:
			break;
		}
		for (const FAngelscriptCachedDataType& SubType : Value.OrderedSubTypes)
		{
			if (const FAngelscriptCacheValidationResult Result =
				ValidateDataType(SubType, Depth + 1); !Result.IsSuccess())
			{
				return Result;
			}
		}
		return {};
	}

	static void WriteStableReference(
		FWriter& Writer,
		const FAngelscriptCacheStableReference& Value)
	{
		Writer.WriteUInt8(static_cast<uint8>(Value.Kind));
		Writer.WriteHash(Value.StableKey);
		Writer.WriteHash(Value.ExpectedAbi);
	}

	static void WriteOptionalUInt32(FWriter& Writer, const TOptional<uint32>& Value)
	{
		Writer.WriteUInt8(Value.IsSet() ? 1 : 0);
		if (Value.IsSet())
		{
			Writer.WriteUInt32(Value.GetValue());
		}
	}

	static void WriteOptionalTypeKey(
		FWriter& Writer,
		const TOptional<FAngelscriptStableTypeKey>& Value)
	{
		Writer.WriteUInt8(Value.IsSet() ? 1 : 0);
		if (Value.IsSet())
		{
			Writer.WriteHash(Value->Hash);
		}
	}

	static void WriteDataType(FWriter& Writer, const FAngelscriptCachedDataType& Value)
	{
		Writer.WriteUInt8(static_cast<uint8>(Value.Kind));
		Writer.WriteUInt8(static_cast<uint8>(Value.Primitive));
		Writer.WriteUInt8(Value.TypeReference.IsSet() ? 1 : 0);
		if (Value.TypeReference.IsSet())
		{
			WriteStableReference(Writer, Value.TypeReference.GetValue());
		}
		Writer.WriteUInt32(Value.QualifierFlags);
		Writer.WriteUInt32(static_cast<uint32>(Value.OrderedSubTypes.Num()));
		for (const FAngelscriptCachedDataType& SubType : Value.OrderedSubTypes)
		{
			WriteDataType(Writer, SubType);
		}
	}

	static void WriteMetadata(FWriter& Writer, const FAngelscriptCachedMetadataEntry& Value)
	{
		Writer.WriteString(Value.CanonicalKey);
		Writer.WriteString(Value.CanonicalValue);
	}

	static void WriteMetadataArray(
		FWriter& Writer,
		const TArray<FAngelscriptCachedMetadataEntry>& Values)
	{
		Writer.WriteUInt32(static_cast<uint32>(Values.Num()));
		for (const FAngelscriptCachedMetadataEntry& Value : Values)
		{
			WriteMetadata(Writer, Value);
		}
	}

	static void WriteDependency(
		FWriter& Writer,
		const FAngelscriptCacheSemanticDependency& Value)
	{
		Writer.WriteUInt8(static_cast<uint8>(Value.Kind));
		WriteStableReference(Writer, Value.Target);
		Writer.WriteOptionalHash(Value.ExpectedContentOrValue);
	}

	template <typename KeyType>
	static void WriteHashKey(FAngelscriptArtifactCanonicalWriter& Writer, const KeyType& Key)
	{
		Writer.WriteHash(Key.Hash);
	}

	static void WriteHashReference(
		FAngelscriptArtifactCanonicalWriter& Writer,
		const FAngelscriptCacheStableReference& Value)
	{
		Writer.WriteUInt8(static_cast<uint8>(Value.Kind));
		Writer.WriteHash(Value.StableKey);
		Writer.WriteHash(Value.ExpectedAbi);
	}

	static void WriteHashOptionalUInt32(
		FAngelscriptArtifactCanonicalWriter& Writer,
		const TOptional<uint32>& Value)
	{
		Writer.WriteBool(Value.IsSet());
		if (Value.IsSet())
		{
			Writer.WriteUInt32(Value.GetValue());
		}
	}

	static void WriteHashOptionalString(
		FAngelscriptArtifactCanonicalWriter& Writer,
		const TOptional<FString>& Value)
	{
		Writer.WriteBool(Value.IsSet());
		if (Value.IsSet())
		{
			Writer.WriteString(Value.GetValue());
		}
	}

	static void WriteHashDataType(
		FAngelscriptArtifactCanonicalWriter& Writer,
		const FAngelscriptCachedDataType& Value)
	{
		Writer.WriteUInt8(static_cast<uint8>(Value.Kind));
		Writer.WriteUInt8(static_cast<uint8>(Value.Primitive));
		Writer.WriteBool(Value.TypeReference.IsSet());
		if (Value.TypeReference.IsSet())
		{
			WriteHashReference(Writer, Value.TypeReference.GetValue());
		}
		Writer.WriteUInt32(Value.QualifierFlags);
		Writer.WriteUInt32(static_cast<uint32>(Value.OrderedSubTypes.Num()));
		for (const FAngelscriptCachedDataType& SubType : Value.OrderedSubTypes)
		{
			WriteHashDataType(Writer, SubType);
		}
	}

	static void WriteHashMetadata(
		FAngelscriptArtifactCanonicalWriter& Writer,
		const TArray<FAngelscriptCachedMetadataEntry>& Values)
	{
		TArray<FAngelscriptCachedMetadataEntry> Canonical = Values;
		Canonical.Sort([](
			const FAngelscriptCachedMetadataEntry& A,
			const FAngelscriptCachedMetadataEntry& B)
		{
			return FAngelscriptCacheTypeSchemaArchive::CompareMetadata(A, B) < 0;
		});
		Writer.WriteUInt32(static_cast<uint32>(Canonical.Num()));
		for (const FAngelscriptCachedMetadataEntry& Value : Canonical)
		{
			Writer.WriteString(Value.CanonicalKey);
			Writer.WriteString(Value.CanonicalValue);
		}
	}

	static FAngelscriptCacheValidationResult ValidateMetadataArray(
		const TArray<FAngelscriptCachedMetadataEntry>& Values,
		int32* OutFailureIndex = nullptr)
	{
		if (OutFailureIndex != nullptr)
		{
			*OutFailureIndex = INDEX_NONE;
		}
		for (int32 Index = 0; Index < Values.Num(); ++Index)
		{
			if (const FAngelscriptCacheValidationResult KeyResult =
				ValidateString(Values[Index].CanonicalKey, true); !KeyResult.IsSuccess())
			{
				if (OutFailureIndex != nullptr) *OutFailureIndex = Index;
				return KeyResult;
			}
			if (const FAngelscriptCacheValidationResult ValueResult =
				ValidateString(Values[Index].CanonicalValue, false); !ValueResult.IsSuccess())
			{
				if (OutFailureIndex != nullptr) *OutFailureIndex = Index;
				return ValueResult;
			}
			if (Index > 0)
			{
				if (CompareString(Values[Index - 1].CanonicalKey,
					Values[Index].CanonicalKey) == 0)
				{
					if (OutFailureIndex != nullptr) *OutFailureIndex = Index;
					return Failure(Values[Index - 1].CanonicalValue
						== Values[Index].CanonicalValue
						? EAngelscriptCacheValidationError::DuplicateKey
						: EAngelscriptCacheValidationError::ConflictingKey);
				}
				const int32 Compare = FAngelscriptCacheTypeSchemaArchive::CompareMetadata(
					Values[Index - 1], Values[Index]);
				if (Compare > 0)
				{
					if (OutFailureIndex != nullptr) *OutFailureIndex = Index;
					return Failure(EAngelscriptCacheValidationError::NonCanonicalOrder);
				}
			}
		}
		return {};
	}

	static bool DependencyRequiresContent(
		const EAngelscriptCacheSemanticDependencyKind Kind)
	{
		return Kind == EAngelscriptCacheSemanticDependencyKind::ValueLayout
			|| Kind == EAngelscriptCacheSemanticDependencyKind::PropertyLayout
			|| Kind == EAngelscriptCacheSemanticDependencyKind::GlobalStorage
			|| Kind == EAngelscriptCacheSemanticDependencyKind::HardValue
			|| Kind == EAngelscriptCacheSemanticDependencyKind::Initializer
			|| Kind == EAngelscriptCacheSemanticDependencyKind::CompileOption
			|| Kind == EAngelscriptCacheSemanticDependencyKind::FunctionContent;
	}

	static FAngelscriptCacheValidationResult ValidateDependency(
		const FAngelscriptCacheSemanticDependency& Value)
	{
		if (static_cast<uint8>(Value.Kind) < 1
			|| static_cast<uint8>(Value.Kind) > 12)
		{
			return Failure(EAngelscriptCacheValidationError::UnknownEnumValue);
		}
		if (const FAngelscriptCacheValidationResult Result =
			ValidateStableReference(Value.Target); !Result.IsSuccess())
		{
			return Result;
		}
		if (DependencyRequiresContent(Value.Kind)
			!= Value.ExpectedContentOrValue.IsSet())
		{
			return Failure(EAngelscriptCacheValidationError::InvalidPresence);
		}
		if (Value.ExpectedContentOrValue.IsSet()
			&& Value.ExpectedContentOrValue->IsZero())
		{
			return Failure(EAngelscriptCacheValidationError::ZeroStableKey);
		}
		if (Value.Kind == EAngelscriptCacheSemanticDependencyKind::FunctionContent
			&& Value.Target.Kind
				!= EAngelscriptCacheReferenceKind::ScriptFunction)
		{
			return Failure(EAngelscriptCacheValidationError::WrongReferenceKind);
		}
		return {};
	}

	static bool IsTypeSchemaDependencyKind(
		const EAngelscriptCacheSemanticDependencyKind Kind)
	{
		return Kind == EAngelscriptCacheSemanticDependencyKind::Declaration
			|| Kind == EAngelscriptCacheSemanticDependencyKind::Signature
			|| Kind == EAngelscriptCacheSemanticDependencyKind::Inheritance
			|| Kind == EAngelscriptCacheSemanticDependencyKind::ValueLayout
			|| Kind == EAngelscriptCacheSemanticDependencyKind::EnvironmentAbi;
	}

	static bool DependencyMatches(
		const FAngelscriptCacheSemanticDependency& Dependency,
		const EAngelscriptCacheSemanticDependencyKind Kind,
		const FAngelscriptCacheStableReference& Target)
	{
		return Dependency.Kind == Kind
			&& Dependency.Target == Target;
	}

	static bool DependencyHasSameCoordinate(
		const FAngelscriptCacheSemanticDependency& Dependency,
		const EAngelscriptCacheSemanticDependencyKind Kind,
		const FAngelscriptCacheStableReference& Target)
	{
		return Dependency.Kind == Kind
			&& Dependency.Target.Kind == Target.Kind
			&& Dependency.Target.StableKey == Target.StableKey;
	}

	static FAngelscriptCacheStableReference MakeScriptFunctionReference(
		const FAngelscriptStableFunctionKey& FunctionKey,
		const FAngelscriptHash256& ExpectedDeclarationAbi)
	{
		return {
			EAngelscriptCacheReferenceKind::ScriptFunction,
			FunctionKey.Hash,
			ExpectedDeclarationAbi,
		};
	}

	static bool DataTypeDerivesDependency(
		const FAngelscriptCachedDataType& DataType,
		const FAngelscriptCacheSemanticDependency& Dependency)
	{
		if ((DataType.Kind == EAngelscriptCachedDataTypeKind::ScriptType
				|| DataType.Kind == EAngelscriptCachedDataTypeKind::EnvironmentType)
			&& DataType.TypeReference.IsSet()
			&& DependencyMatches(Dependency,
				EAngelscriptCacheSemanticDependencyKind::ValueLayout,
				DataType.TypeReference.GetValue()))
		{
			return true;
		}
		for (const FAngelscriptCachedDataType& SubType : DataType.OrderedSubTypes)
		{
			if (DataTypeDerivesDependency(SubType, Dependency))
			{
				return true;
			}
		}
		return false;
	}

	static EAngelscriptCacheSemanticDependencyKind GetObjectHandleTargetDependencyKind(
		const FAngelscriptCachedDataType& DataType)
	{
		return DataType.Kind == EAngelscriptCachedDataTypeKind::ScriptType
			? EAngelscriptCacheSemanticDependencyKind::Declaration
			: EAngelscriptCacheSemanticDependencyKind::EnvironmentAbi;
	}

	static bool PropertyDerivesDependency(
		const FAngelscriptCachedPropertySchema& Property,
		const FAngelscriptCacheSemanticDependency& Dependency)
	{
		if (Property.StorageKind == EAngelscriptCachedPropertyStorageKind::ObjectHandle
			&& Property.Type.TypeReference.IsSet())
		{
			return DependencyMatches(Dependency,
				GetObjectHandleTargetDependencyKind(Property.Type),
				Property.Type.TypeReference.GetValue());
		}
		return DataTypeDerivesDependency(Property.Type, Dependency);
	}

	static bool TypeSchemaDerivesDependency(
		const FAngelscriptCachedTypeSchema& Value,
		const FAngelscriptCacheSemanticDependency& Dependency)
	{
		for (const FAngelscriptCachedTypeRelation& Relation : Value.Relations)
		{
			const EAngelscriptCacheSemanticDependencyKind Kind =
				Relation.Target.Kind == EAngelscriptCacheReferenceKind::ScriptType
					? EAngelscriptCacheSemanticDependencyKind::Inheritance
					: EAngelscriptCacheSemanticDependencyKind::EnvironmentAbi;
			if (DependencyMatches(Dependency, Kind, Relation.Target))
			{
				return true;
			}
		}

		for (const FAngelscriptCachedTypeLayoutInput& Input : Value.LayoutInputs)
		{
			const EAngelscriptCacheSemanticDependencyKind Kind =
				Input.InputKind == EAngelscriptCachedTypeLayoutInputKind::BaseType
					? EAngelscriptCacheSemanticDependencyKind::Inheritance
					: EAngelscriptCacheSemanticDependencyKind::EnvironmentAbi;
			if (DependencyMatches(Dependency, Kind, Input.Target))
			{
				return true;
			}
		}

		for (const FAngelscriptCachedPropertySchema& Property : Value.OrderedProperties)
		{
			if (PropertyDerivesDependency(Property, Dependency))
			{
				return true;
			}
		}

		for (const FAngelscriptCachedMethodEntry& Method : Value.OrderedMethods)
		{
			if (DependencyMatches(Dependency,
				EAngelscriptCacheSemanticDependencyKind::Declaration,
				MakeScriptFunctionReference(
					Method.FunctionKey, Method.ExpectedDeclarationAbi)))
			{
				return true;
			}
		}

		for (const FAngelscriptCachedVirtualFunctionSlot& Slot :
			Value.VirtualFunctionTable)
		{
			if (DependencyMatches(Dependency,
				EAngelscriptCacheSemanticDependencyKind::Declaration,
				MakeScriptFunctionReference(
					Slot.FunctionKey, Slot.ExpectedDeclarationAbi)))
			{
				return true;
			}
		}

		for (const FAngelscriptCachedBehaviorSlot& Slot : Value.OrderedBehaviorSlots)
		{
			const EAngelscriptCacheSemanticDependencyKind Kind =
				Slot.Target.Kind == EAngelscriptCacheReferenceKind::ScriptFunction
					? EAngelscriptCacheSemanticDependencyKind::Declaration
					: EAngelscriptCacheSemanticDependencyKind::EnvironmentAbi;
			if (DependencyMatches(Dependency, Kind, Slot.Target))
			{
				return true;
			}
		}

		if (Value.KindPayload.Callable.IsSet())
		{
			const FAngelscriptCachedCallableTypePayload& Callable =
				Value.KindPayload.Callable.GetValue();
			if (DependencyMatches(Dependency,
				EAngelscriptCacheSemanticDependencyKind::Signature,
				MakeScriptFunctionReference(Callable.SignatureFunctionKey,
					Callable.ExpectedSignatureAbi)))
			{
				return true;
			}
		}

		for (const FAngelscriptCachedReflectedFunctionMember& Member :
			Value.Reflection.OrderedUFunctionMembers)
		{
			if (DependencyMatches(Dependency,
				EAngelscriptCacheSemanticDependencyKind::Declaration,
				Member.Target))
			{
				return true;
			}
		}
		return false;
	}

	static FAngelscriptCacheValidationResult RequireDependency(
		const FAngelscriptCachedTypeSchema& Value,
		const EAngelscriptCacheSemanticDependencyKind Kind,
		const FAngelscriptCacheStableReference& Target,
		FAngelscriptTypeSchemaFieldCoordinate* OutFailureCoordinate)
	{
		for (int32 Index = 0; Index < Value.Dependencies.Num(); ++Index)
		{
			const FAngelscriptCacheSemanticDependency& Dependency =
				Value.Dependencies[Index];
			if (DependencyMatches(Dependency, Kind, Target))
			{
				return {};
			}
			if (DependencyHasSameCoordinate(Dependency, Kind, Target))
			{
				return FailureAt(EAngelscriptCacheValidationError::ConflictingKey,
					OutFailureCoordinate,
					EAngelscriptTypeSchemaCapturedField::Dependency, Index);
			}
		}
		return Failure(EAngelscriptCacheValidationError::MissingCoverage);
	}

	static FAngelscriptCacheValidationResult RequireDataTypeDependencies(
		const FAngelscriptCachedTypeSchema& Value,
		const FAngelscriptCachedDataType& DataType,
		FAngelscriptTypeSchemaFieldCoordinate* OutFailureCoordinate)
	{
		if ((DataType.Kind == EAngelscriptCachedDataTypeKind::ScriptType
				|| DataType.Kind == EAngelscriptCachedDataTypeKind::EnvironmentType)
			&& DataType.TypeReference.IsSet())
		{
			if (const FAngelscriptCacheValidationResult Result = RequireDependency(
				Value, EAngelscriptCacheSemanticDependencyKind::ValueLayout,
				DataType.TypeReference.GetValue(), OutFailureCoordinate);
				!Result.IsSuccess())
			{
				return Result;
			}
		}
		for (const FAngelscriptCachedDataType& SubType : DataType.OrderedSubTypes)
		{
			if (const FAngelscriptCacheValidationResult Result =
				RequireDataTypeDependencies(Value, SubType, OutFailureCoordinate);
				!Result.IsSuccess())
			{
				return Result;
			}
		}
		return {};
	}

	static FAngelscriptCacheValidationResult RequirePropertyDependencies(
		const FAngelscriptCachedTypeSchema& Value,
		const FAngelscriptCachedPropertySchema& Property,
		FAngelscriptTypeSchemaFieldCoordinate* OutFailureCoordinate)
	{
		if (Property.StorageKind == EAngelscriptCachedPropertyStorageKind::ObjectHandle
			&& Property.Type.TypeReference.IsSet())
		{
			return RequireDependency(Value,
				GetObjectHandleTargetDependencyKind(Property.Type),
				Property.Type.TypeReference.GetValue(), OutFailureCoordinate);
		}
		return RequireDataTypeDependencies(Value, Property.Type, OutFailureCoordinate);
	}

	static FAngelscriptCacheValidationResult ValidateDependencyCrossFieldClosure(
		const FAngelscriptCachedTypeSchema& Value,
		FAngelscriptTypeSchemaFieldCoordinate* OutFailureCoordinate)
	{
		for (const FAngelscriptCachedTypeRelation& Relation : Value.Relations)
		{
			const EAngelscriptCacheSemanticDependencyKind Kind =
				Relation.Target.Kind == EAngelscriptCacheReferenceKind::ScriptType
					? EAngelscriptCacheSemanticDependencyKind::Inheritance
					: EAngelscriptCacheSemanticDependencyKind::EnvironmentAbi;
			if (const FAngelscriptCacheValidationResult Result = RequireDependency(
				Value, Kind, Relation.Target, OutFailureCoordinate); !Result.IsSuccess())
			{
				return Result;
			}
		}

		for (const FAngelscriptCachedTypeLayoutInput& Input : Value.LayoutInputs)
		{
			const EAngelscriptCacheSemanticDependencyKind Kind =
				Input.InputKind == EAngelscriptCachedTypeLayoutInputKind::BaseType
					? EAngelscriptCacheSemanticDependencyKind::Inheritance
					: EAngelscriptCacheSemanticDependencyKind::EnvironmentAbi;
			if (const FAngelscriptCacheValidationResult Result = RequireDependency(
				Value, Kind, Input.Target, OutFailureCoordinate); !Result.IsSuccess())
			{
				return Result;
			}
		}

		for (const FAngelscriptCachedPropertySchema& Property : Value.OrderedProperties)
		{
			if (const FAngelscriptCacheValidationResult Result =
				RequirePropertyDependencies(Value, Property, OutFailureCoordinate);
				!Result.IsSuccess())
			{
				return Result;
			}
		}

		for (const FAngelscriptCachedMethodEntry& Method : Value.OrderedMethods)
		{
			if (const FAngelscriptCacheValidationResult Result = RequireDependency(
				Value, EAngelscriptCacheSemanticDependencyKind::Declaration,
				MakeScriptFunctionReference(
					Method.FunctionKey, Method.ExpectedDeclarationAbi),
				OutFailureCoordinate); !Result.IsSuccess())
			{
				return Result;
			}
		}

		for (const FAngelscriptCachedVirtualFunctionSlot& Slot :
			Value.VirtualFunctionTable)
		{
			if (const FAngelscriptCacheValidationResult Result = RequireDependency(
				Value, EAngelscriptCacheSemanticDependencyKind::Declaration,
				MakeScriptFunctionReference(
					Slot.FunctionKey, Slot.ExpectedDeclarationAbi),
				OutFailureCoordinate); !Result.IsSuccess())
			{
				return Result;
			}
		}

		for (const FAngelscriptCachedBehaviorSlot& Slot : Value.OrderedBehaviorSlots)
		{
			const EAngelscriptCacheSemanticDependencyKind Kind =
				Slot.Target.Kind == EAngelscriptCacheReferenceKind::ScriptFunction
					? EAngelscriptCacheSemanticDependencyKind::Declaration
					: EAngelscriptCacheSemanticDependencyKind::EnvironmentAbi;
			if (const FAngelscriptCacheValidationResult Result = RequireDependency(
				Value, Kind, Slot.Target, OutFailureCoordinate); !Result.IsSuccess())
			{
				return Result;
			}
		}

		if (Value.KindPayload.Callable.IsSet())
		{
			const FAngelscriptCachedCallableTypePayload& Callable =
				Value.KindPayload.Callable.GetValue();
			if (const FAngelscriptCacheValidationResult Result = RequireDependency(
				Value, EAngelscriptCacheSemanticDependencyKind::Signature,
				MakeScriptFunctionReference(Callable.SignatureFunctionKey,
					Callable.ExpectedSignatureAbi),
				OutFailureCoordinate); !Result.IsSuccess())
			{
				return Result;
			}
		}

		for (const FAngelscriptCachedReflectedFunctionMember& Member :
			Value.Reflection.OrderedUFunctionMembers)
		{
			if (const FAngelscriptCacheValidationResult Result = RequireDependency(
				Value, EAngelscriptCacheSemanticDependencyKind::Declaration,
				Member.Target, OutFailureCoordinate); !Result.IsSuccess())
			{
				return Result;
			}
		}

		for (int32 Index = 0; Index < Value.Dependencies.Num(); ++Index)
		{
			const FAngelscriptCacheSemanticDependency& Dependency =
				Value.Dependencies[Index];
			if (!IsTypeSchemaDependencyKind(Dependency.Kind)
				|| !TypeSchemaDerivesDependency(Value, Dependency))
			{
				return FailureAt(EAngelscriptCacheValidationError::UnexpectedRecord,
					OutFailureCoordinate,
					EAngelscriptTypeSchemaCapturedField::Dependency, Index);
			}
		}
		return {};
	}

	static void CanonicalizeMetadataArray(
		TArray<FAngelscriptCachedMetadataEntry>& Values)
	{
		Values.Sort([](
			const FAngelscriptCachedMetadataEntry& A,
			const FAngelscriptCachedMetadataEntry& B)
		{
			return FAngelscriptCacheTypeSchemaArchive::CompareMetadata(A, B) < 0;
		});
	}

	static void CanonicalizeSetFields(FAngelscriptCachedTypeSchema& Value)
	{
		CanonicalizeMetadataArray(Value.Metadata);
		Value.Relations.StableSort([](
			const FAngelscriptCachedTypeRelation& A,
			const FAngelscriptCachedTypeRelation& B)
		{
			return static_cast<uint8>(A.RelationKind)
				< static_cast<uint8>(B.RelationKind);
		});
		Value.LayoutInputs.StableSort([](
			const FAngelscriptCachedTypeLayoutInput& A,
			const FAngelscriptCachedTypeLayoutInput& B)
		{
			return static_cast<uint8>(A.InputKind)
				< static_cast<uint8>(B.InputKind);
		});
		for (FAngelscriptCachedPropertySchema& Property : Value.OrderedProperties)
		{
			CanonicalizeMetadataArray(Property.Metadata);
		}
		if (Value.KindPayload.Enum.IsSet())
		{
			for (FAngelscriptCachedEnumEnumerator& Enumerator :
				Value.KindPayload.Enum->OrderedEnumerators)
			{
				CanonicalizeMetadataArray(Enumerator.Metadata);
			}
		}
		Value.Dependencies.Sort([](
			const FAngelscriptCacheSemanticDependency& A,
			const FAngelscriptCacheSemanticDependency& B)
		{
			return FAngelscriptCacheTypeSchemaArchive::CompareDependencies(A, B) < 0;
		});
	}

	static bool HasTypeSemanticFlag(
		const FAngelscriptCachedTypeSchema& Value,
		const EAngelscriptCachedTypeSemanticFlags Flag)
	{
		return (Value.TypeSemanticFlags & static_cast<uint32>(Flag)) != 0;
	}

	static FAngelscriptCacheValidationResult ValidateTypeSemanticFlagsFieldLocal(
		const FAngelscriptCachedTypeSchema& Value,
		FAngelscriptTypeSchemaFieldCoordinate* OutFailureCoordinate)
	{
		using EFlags = EAngelscriptCachedTypeSemanticFlags;
		const auto Flag = [](const EFlags ValueFlag)
		{
			return static_cast<uint32>(ValueFlag);
		};
		const uint32 KnownMask = Flag(EFlags::KnownMask);
		if ((Value.TypeSemanticFlags & ~KnownMask) != 0)
		{
			return FailureAt(EAngelscriptCacheValidationError::UnknownFlags,
				OutFailureCoordinate,
				EAngelscriptTypeSchemaCapturedField::TypeSemanticFlags);
		}

		const uint32 Abstract = Flag(EFlags::Abstract);
		const uint32 Final = Flag(EFlags::Final);
		const uint32 Shared = Flag(EFlags::Shared);
		const uint32 Generated = Flag(EFlags::Generated);
		const uint32 HasDefaultConstructor = Flag(EFlags::HasDefaultConstructor);
		const uint32 HasDestructor = Flag(EFlags::HasDestructor);
		const uint32 ValueType = Flag(EFlags::ValueType);
		const uint32 ReferenceType = Flag(EFlags::ReferenceType);

		uint32 Required = 0;
		uint32 Allowed = 0;
		switch (Value.TypeKind)
		{
		case EAngelscriptCachedTypeKind::Class:
			Required = ReferenceType;
			Allowed = ReferenceType | Abstract | Final | Shared | Generated
				| HasDefaultConstructor | HasDestructor;
			break;
		case EAngelscriptCachedTypeKind::Struct:
			Required = Final | ValueType;
			Allowed = Required | Shared | Generated
				| HasDefaultConstructor | HasDestructor;
			break;
		case EAngelscriptCachedTypeKind::Interface:
			Required = Abstract | ReferenceType;
			Allowed = Required | Shared;
			break;
		case EAngelscriptCachedTypeKind::Enum:
			Required = Final | ValueType;
			Allowed = Required | Shared;
			break;
		case EAngelscriptCachedTypeKind::Delegate:
			Required = Final | Generated | ValueType;
			Allowed = Required | HasDefaultConstructor | HasDestructor;
			break;
		case EAngelscriptCachedTypeKind::Typedef:
			break;
		case EAngelscriptCachedTypeKind::Funcdef:
			Required = ReferenceType;
			Allowed = Required | Shared;
			break;
		default:
			return FailureAt(EAngelscriptCacheValidationError::UnknownEnumValue,
				OutFailureCoordinate,
				EAngelscriptTypeSchemaCapturedField::TypeKind);
		}

		const bool bMissingRequired =
			(Value.TypeSemanticFlags & Required) != Required;
		const bool bHasForbidden = (Value.TypeSemanticFlags & ~Allowed) != 0;
		const bool bAbstractFinal =
			(Value.TypeSemanticFlags & (Abstract | Final)) == (Abstract | Final);
		const bool bValueReference =
			(Value.TypeSemanticFlags & (ValueType | ReferenceType))
				== (ValueType | ReferenceType);
		if (bMissingRequired || bHasForbidden || bAbstractFinal || bValueReference)
		{
			return FailureAt(
				EAngelscriptCacheValidationError::InvalidQualifierCombination,
				OutFailureCoordinate,
				EAngelscriptTypeSchemaCapturedField::TypeSemanticFlags);
		}
		return {};
	}

	static bool IsRelationKindPotentiallyAllowedForType(
		const EAngelscriptCachedTypeKind TypeKind,
		const EAngelscriptCachedTypeRelationKind RelationKind)
	{
		if (TypeKind == EAngelscriptCachedTypeKind::Class)
		{
			return RelationKind == EAngelscriptCachedTypeRelationKind::Base
				|| RelationKind == EAngelscriptCachedTypeRelationKind::ShadowSuper
				|| RelationKind == EAngelscriptCachedTypeRelationKind::CodeSuper
				|| RelationKind
					== EAngelscriptCachedTypeRelationKind::ImplementedInterface;
		}
		return TypeKind == EAngelscriptCachedTypeKind::Interface
			&& RelationKind == EAngelscriptCachedTypeRelationKind::ImplementedInterface;
	}

	static FAngelscriptCacheValidationResult ValidateRelationsFieldLocal(
		const FAngelscriptCachedTypeSchema& Value,
		FAngelscriptTypeSchemaFieldCoordinate* OutFailureCoordinate)
	{
		uint8 PreviousKind = 0;
		int32 GroupStart = 0;
		for (int32 Index = 0; Index < Value.Relations.Num(); ++Index)
		{
			const FAngelscriptCachedTypeRelation& Relation = Value.Relations[Index];
			const uint8 RawKind = static_cast<uint8>(Relation.RelationKind);
			if (RawKind < 1 || RawKind > 5)
			{
				return FailureAt(EAngelscriptCacheValidationError::UnknownEnumValue,
					OutFailureCoordinate,
					EAngelscriptTypeSchemaCapturedField::Relation, Index);
			}
			if (const FAngelscriptCacheValidationResult Result =
				ValidateStableReference(Relation.Target); !Result.IsSuccess())
			{
				return FailureAt(Result.Error, OutFailureCoordinate,
					EAngelscriptTypeSchemaCapturedField::Relation, Index);
			}
			if (Relation.Target.Kind != EAngelscriptCacheReferenceKind::ScriptType
				&& Relation.Target.Kind
					!= EAngelscriptCacheReferenceKind::EnvironmentSymbol)
			{
				return FailureAt(EAngelscriptCacheValidationError::WrongReferenceKind,
					OutFailureCoordinate,
					EAngelscriptTypeSchemaCapturedField::Relation, Index);
			}
			if (Relation.Target.Kind == EAngelscriptCacheReferenceKind::ScriptType
				&& Relation.Target.StableKey == Value.TypeKey.Hash)
			{
				return FailureAt(EAngelscriptCacheValidationError::ConflictingKey,
					OutFailureCoordinate,
					EAngelscriptTypeSchemaCapturedField::Relation, Index);
			}
			if (RawKind < PreviousKind)
			{
				return FailureAt(EAngelscriptCacheValidationError::NonCanonicalOrder,
					OutFailureCoordinate,
					EAngelscriptTypeSchemaCapturedField::Relation, Index - 1);
			}
			if (RawKind != PreviousKind)
			{
				GroupStart = Index;
			}

			const bool bImplementedInterface = Relation.RelationKind
				== EAngelscriptCachedTypeRelationKind::ImplementedInterface;
			if (Relation.SemanticOrdinal.IsSet() != bImplementedInterface)
			{
				return FailureAt(EAngelscriptCacheValidationError::InvalidPresence,
					OutFailureCoordinate,
					EAngelscriptTypeSchemaCapturedField::Relation, Index);
			}

			if (Index > GroupStart)
			{
				for (int32 EarlierIndex = GroupStart;
					EarlierIndex < Index; ++EarlierIndex)
				{
					const FAngelscriptCachedTypeRelation& Earlier =
						Value.Relations[EarlierIndex];
					if (Earlier.Target == Relation.Target)
					{
						return FailureAt(
							EAngelscriptCacheValidationError::DuplicateKey,
							OutFailureCoordinate,
							EAngelscriptTypeSchemaCapturedField::Relation, Index);
					}
				}
			}

			if (bImplementedInterface)
			{
				const uint32 ExpectedOrdinal = static_cast<uint32>(Index - GroupStart);
				const uint32 ActualOrdinal = Relation.SemanticOrdinal.GetValue();
				if (ActualOrdinal != ExpectedOrdinal)
				{
					if (ActualOrdinal < ExpectedOrdinal)
					{
						return FailureAt(
							EAngelscriptCacheValidationError::DuplicateOrdinal,
							OutFailureCoordinate,
							EAngelscriptTypeSchemaCapturedField::Relation, Index);
					}
					bool bExpectedOrdinalAppearsLater = false;
					for (int32 LaterIndex = Index + 1;
						LaterIndex < Value.Relations.Num(); ++LaterIndex)
					{
						const FAngelscriptCachedTypeRelation& Later =
							Value.Relations[LaterIndex];
						if (Later.RelationKind != Relation.RelationKind)
						{
							break;
						}
						bExpectedOrdinalAppearsLater |= Later.SemanticOrdinal.IsSet()
							&& Later.SemanticOrdinal.GetValue() == ExpectedOrdinal;
					}
					return FailureAt(bExpectedOrdinalAppearsLater
							? EAngelscriptCacheValidationError::NonCanonicalOrder
							: EAngelscriptCacheValidationError::OrdinalGap,
						OutFailureCoordinate,
						EAngelscriptTypeSchemaCapturedField::Relation, Index);
				}
			}

			if (!IsRelationKindPotentiallyAllowedForType(
				Value.TypeKind, Relation.RelationKind))
			{
				return FailureAt(EAngelscriptCacheValidationError::InvalidPresence,
					OutFailureCoordinate,
					EAngelscriptTypeSchemaCapturedField::Relation, Index);
			}
			PreviousKind = RawKind;
		}
		return {};
	}

	enum class ERelationForm : uint8
	{
		NoRelations,
		ClassNone,
		OrdinaryUClass,
		StaticsUClass,
		InterfaceNone,
	};

	static ERelationForm GetRelationForm(const FAngelscriptCachedTypeSchema& Value)
	{
		if (Value.TypeKind == EAngelscriptCachedTypeKind::Class)
		{
			if (Value.Reflection.ReflectionKind == EAngelscriptCachedReflectionKind::None)
			{
				return ERelationForm::ClassNone;
			}
			if (Value.Reflection.ReflectionKind == EAngelscriptCachedReflectionKind::UClass)
			{
				const bool bStaticsClass = (Value.Reflection.ClassReflectionFlags
					& static_cast<uint32>(
						EAngelscriptCachedClassReflectionFlags::StaticsClass)) != 0;
				return bStaticsClass
					? ERelationForm::StaticsUClass
					: ERelationForm::OrdinaryUClass;
			}
		}
		if (Value.TypeKind == EAngelscriptCachedTypeKind::Interface
			&& Value.Reflection.ReflectionKind == EAngelscriptCachedReflectionKind::None)
		{
			return ERelationForm::InterfaceNone;
		}
		return ERelationForm::NoRelations;
	}

	struct FRelationFormRule
	{
		bool bAllowed = false;
		bool bExactlyOne = false;
		bool bMany = false;
		EAngelscriptCacheReferenceKind TargetKind =
			EAngelscriptCacheReferenceKind::Invalid;
	};

	static FRelationFormRule GetRelationFormRule(
		const ERelationForm Form,
		const EAngelscriptCachedTypeRelationKind Kind)
	{
		FRelationFormRule Rule;
		if (Form == ERelationForm::ClassNone)
		{
			Rule.bAllowed = Kind == EAngelscriptCachedTypeRelationKind::Base
				|| Kind == EAngelscriptCachedTypeRelationKind::ImplementedInterface;
			Rule.bMany = Kind
				== EAngelscriptCachedTypeRelationKind::ImplementedInterface;
			Rule.TargetKind = EAngelscriptCacheReferenceKind::ScriptType;
		}
		else if (Form == ERelationForm::OrdinaryUClass)
		{
			Rule.bAllowed = Kind != EAngelscriptCachedTypeRelationKind::Compose;
			Rule.bExactlyOne = Kind == EAngelscriptCachedTypeRelationKind::ShadowSuper
				|| Kind == EAngelscriptCachedTypeRelationKind::CodeSuper;
			Rule.bMany = Kind
				== EAngelscriptCachedTypeRelationKind::ImplementedInterface;
			Rule.TargetKind = Kind == EAngelscriptCachedTypeRelationKind::Base
				? EAngelscriptCacheReferenceKind::ScriptType
				: EAngelscriptCacheReferenceKind::EnvironmentSymbol;
		}
		else if (Form == ERelationForm::StaticsUClass)
		{
			Rule.bAllowed = Kind == EAngelscriptCachedTypeRelationKind::CodeSuper;
			Rule.bExactlyOne = Rule.bAllowed;
			Rule.TargetKind = EAngelscriptCacheReferenceKind::EnvironmentSymbol;
		}
		else if (Form == ERelationForm::InterfaceNone)
		{
			Rule.bAllowed = Kind
				== EAngelscriptCachedTypeRelationKind::ImplementedInterface;
			Rule.bMany = Rule.bAllowed;
			Rule.TargetKind = EAngelscriptCacheReferenceKind::ScriptType;
		}
		return Rule;
	}

	static FAngelscriptCacheValidationResult ValidateRelationsCrossFieldClosure(
		const FAngelscriptCachedTypeSchema& Value,
		FAngelscriptTypeSchemaFieldCoordinate* OutFailureCoordinate)
	{
		const ERelationForm Form = GetRelationForm(Value);
		for (int32 Index = 0; Index < Value.Relations.Num(); ++Index)
		{
			const FAngelscriptCachedTypeRelation& Relation = Value.Relations[Index];
			const FRelationFormRule Rule = GetRelationFormRule(Form, Relation.RelationKind);
			if (!Rule.bAllowed)
			{
				return FailureAt(EAngelscriptCacheValidationError::InvalidPresence,
					OutFailureCoordinate,
					EAngelscriptTypeSchemaCapturedField::Relation, Index);
			}
			if (Relation.Target.Kind != Rule.TargetKind)
			{
				return FailureAt(EAngelscriptCacheValidationError::WrongReferenceKind,
					OutFailureCoordinate,
					EAngelscriptTypeSchemaCapturedField::Relation, Index);
			}
		}

		for (uint8 RawKind = 1; RawKind <= 5; ++RawKind)
		{
			const EAngelscriptCachedTypeRelationKind Kind =
				static_cast<EAngelscriptCachedTypeRelationKind>(RawKind);
			const FRelationFormRule Rule = GetRelationFormRule(Form, Kind);
			int32 FirstIndex = INDEX_NONE;
			uint32 Count = 0;
			for (int32 Index = 0; Index < Value.Relations.Num(); ++Index)
			{
				if (Value.Relations[Index].RelationKind == Kind)
				{
					FirstIndex = FirstIndex == INDEX_NONE ? Index : FirstIndex;
					++Count;
				}
			}
			if (Rule.bExactlyOne && Count == 0)
			{
				return FailureAt(EAngelscriptCacheValidationError::InvalidPresence,
					OutFailureCoordinate,
					EAngelscriptTypeSchemaCapturedField::Reflection);
			}
			if (Rule.bAllowed && !Rule.bMany && Count > 1)
			{
				return FailureAt(EAngelscriptCacheValidationError::ConflictingKey,
					OutFailureCoordinate,
					EAngelscriptTypeSchemaCapturedField::Relation,
					static_cast<uint32>(FirstIndex + 1));
			}
		}
		return {};
	}

	static bool AreOptionalUInt32Equal(
		const TOptional<uint32>& A,
		const TOptional<uint32>& B)
	{
		return A.IsSet() == B.IsSet()
			&& (!A.IsSet() || A.GetValue() == B.GetValue());
	}

	static bool AreLayoutInputsEqual(
		const FAngelscriptCachedTypeLayoutInput& A,
		const FAngelscriptCachedTypeLayoutInput& B)
	{
		return A.InputKind == B.InputKind
			&& A.Target == B.Target
			&& AreOptionalUInt32Equal(
				A.BoundaryContribution, B.BoundaryContribution)
			&& AreOptionalUInt32Equal(
				A.AlignmentContribution, B.AlignmentContribution)
			&& A.LayoutInputHash == B.LayoutInputHash;
	}

	static bool HasBaseRelation(const FAngelscriptCachedTypeSchema& Value)
	{
		for (const FAngelscriptCachedTypeRelation& Relation : Value.Relations)
		{
			if (Relation.RelationKind == EAngelscriptCachedTypeRelationKind::Base)
			{
				return true;
			}
		}
		return false;
	}

	static bool IsLayoutInputKindPotentiallyAllowedForType(
		const EAngelscriptCachedTypeKind TypeKind,
		const EAngelscriptCachedTypeLayoutInputKind InputKind)
	{
		if (TypeKind == EAngelscriptCachedTypeKind::Class)
		{
			return InputKind == EAngelscriptCachedTypeLayoutInputKind::BaseType
				|| InputKind == EAngelscriptCachedTypeLayoutInputKind::CodeRoot;
		}
		return TypeKind == EAngelscriptCachedTypeKind::Struct
			&& InputKind == EAngelscriptCachedTypeLayoutInputKind::StructHeader;
	}

	static EAngelscriptCacheReferenceKind GetLayoutInputTargetKind(
		const EAngelscriptCachedTypeLayoutInputKind InputKind)
	{
		return InputKind == EAngelscriptCachedTypeLayoutInputKind::BaseType
			? EAngelscriptCacheReferenceKind::ScriptType
			: EAngelscriptCacheReferenceKind::EnvironmentSymbol;
	}

	static FAngelscriptCacheValidationResult ValidateLayoutInputsFieldLocal(
		const FAngelscriptCachedTypeSchema& Value,
		FAngelscriptTypeSchemaFieldCoordinate* OutFailureCoordinate)
	{
		const bool bHasBase = HasBaseRelation(Value);
		uint8 PreviousKind = 0;
		for (int32 Index = 0; Index < Value.LayoutInputs.Num(); ++Index)
		{
			const FAngelscriptCachedTypeLayoutInput& Input = Value.LayoutInputs[Index];
			const uint8 RawKind = static_cast<uint8>(Input.InputKind);
			if (RawKind < 1 || RawKind > 3)
			{
				return FailureAt(EAngelscriptCacheValidationError::UnknownEnumValue,
					OutFailureCoordinate,
					EAngelscriptTypeSchemaCapturedField::LayoutInput, Index);
			}
			if (const FAngelscriptCacheValidationResult Result =
				ValidateStableReference(Input.Target); !Result.IsSuccess())
			{
				return FailureAt(Result.Error, OutFailureCoordinate,
					EAngelscriptTypeSchemaCapturedField::LayoutInput, Index);
			}
			if (Input.Target.Kind != GetLayoutInputTargetKind(Input.InputKind))
			{
				return FailureAt(EAngelscriptCacheValidationError::WrongReferenceKind,
					OutFailureCoordinate,
					EAngelscriptTypeSchemaCapturedField::LayoutInput, Index);
			}

			bool bRequiresBoundary = false;
			bool bRequiresAlignment = false;
			switch (Input.InputKind)
			{
			case EAngelscriptCachedTypeLayoutInputKind::BaseType:
				bRequiresBoundary = true;
				bRequiresAlignment = true;
				break;
			case EAngelscriptCachedTypeLayoutInputKind::CodeRoot:
				bRequiresBoundary = !bHasBase;
				bRequiresAlignment = true;
				break;
			case EAngelscriptCachedTypeLayoutInputKind::StructHeader:
				bRequiresBoundary = true;
				break;
			default:
				checkNoEntry();
			}
			if (Input.BoundaryContribution.IsSet() != bRequiresBoundary
				|| Input.AlignmentContribution.IsSet() != bRequiresAlignment)
			{
				return FailureAt(EAngelscriptCacheValidationError::InvalidPresence,
					OutFailureCoordinate,
					EAngelscriptTypeSchemaCapturedField::LayoutInput, Index);
			}
			if (Input.BoundaryContribution.IsSet()
				&& Input.BoundaryContribution.GetValue()
					> static_cast<uint32>(MAX_int32))
			{
				return FailureAt(EAngelscriptCacheValidationError::Overflow,
					OutFailureCoordinate,
					EAngelscriptTypeSchemaCapturedField::LayoutInput, Index);
			}
			if (Input.AlignmentContribution.IsSet())
			{
				const uint32 Alignment = Input.AlignmentContribution.GetValue();
				if (Alignment > static_cast<uint32>(MAX_int32))
				{
					return FailureAt(EAngelscriptCacheValidationError::Overflow,
						OutFailureCoordinate,
						EAngelscriptTypeSchemaCapturedField::LayoutInput, Index);
				}
				if (Alignment == 0 || (Alignment & (Alignment - 1)) != 0)
				{
					return FailureAt(
						EAngelscriptCacheValidationError::InvalidQualifierCombination,
						OutFailureCoordinate,
						EAngelscriptTypeSchemaCapturedField::LayoutInput, Index);
				}
			}

			FAngelscriptHash256 ExpectedHash;
			if (const FAngelscriptCacheValidationResult Result =
				FAngelscriptCacheTypeSchemaArchive::ComputeLayoutInputHash(
					Input, ExpectedHash); !Result.IsSuccess())
			{
				return FailureAt(Result.Error, OutFailureCoordinate,
					EAngelscriptTypeSchemaCapturedField::LayoutInput, Index);
			}
			if (!(ExpectedHash == Input.LayoutInputHash))
			{
				return FailureAt(EAngelscriptCacheValidationError::DerivedHashMismatch,
					OutFailureCoordinate,
					EAngelscriptTypeSchemaCapturedField::LayoutInput, Index);
			}

			if (RawKind < PreviousKind)
			{
				return FailureAt(EAngelscriptCacheValidationError::NonCanonicalOrder,
					OutFailureCoordinate,
					EAngelscriptTypeSchemaCapturedField::LayoutInput, Index - 1);
			}
			if (RawKind == PreviousKind)
			{
				return FailureAt(AreLayoutInputsEqual(Value.LayoutInputs[Index - 1], Input)
						? EAngelscriptCacheValidationError::DuplicateKey
						: EAngelscriptCacheValidationError::ConflictingKey,
					OutFailureCoordinate,
					EAngelscriptTypeSchemaCapturedField::LayoutInput, Index);
			}
			if (!IsLayoutInputKindPotentiallyAllowedForType(
				Value.TypeKind, Input.InputKind))
			{
				return FailureAt(EAngelscriptCacheValidationError::InvalidPresence,
					OutFailureCoordinate,
					EAngelscriptTypeSchemaCapturedField::LayoutInput, Index);
			}
			PreviousKind = RawKind;
		}
		return {};
	}

	enum class ELayoutInputForm : uint8
	{
		NoInputs,
		ClassNoneWithBase,
		OrdinaryUClassRoot,
		OrdinaryUClassDerived,
		ReflectedUStruct,
	};

	static ELayoutInputForm GetLayoutInputForm(
		const FAngelscriptCachedTypeSchema& Value)
	{
		const bool bHasBase = HasBaseRelation(Value);
		if (Value.TypeKind == EAngelscriptCachedTypeKind::Class)
		{
			if (Value.Reflection.ReflectionKind == EAngelscriptCachedReflectionKind::None)
			{
				return bHasBase
					? ELayoutInputForm::ClassNoneWithBase
					: ELayoutInputForm::NoInputs;
			}
			if (Value.Reflection.ReflectionKind == EAngelscriptCachedReflectionKind::UClass)
			{
				const bool bStaticsClass = (Value.Reflection.ClassReflectionFlags
					& static_cast<uint32>(
						EAngelscriptCachedClassReflectionFlags::StaticsClass)) != 0;
				if (!bStaticsClass)
				{
					return bHasBase
						? ELayoutInputForm::OrdinaryUClassDerived
						: ELayoutInputForm::OrdinaryUClassRoot;
				}
			}
		}
		if (Value.TypeKind == EAngelscriptCachedTypeKind::Struct
			&& Value.Reflection.ReflectionKind
				== EAngelscriptCachedReflectionKind::UStruct)
		{
			return ELayoutInputForm::ReflectedUStruct;
		}
		return ELayoutInputForm::NoInputs;
	}

	static uint8 GetLayoutInputRoleMask(const ELayoutInputForm Form)
	{
		const uint8 Base = 1u << (static_cast<uint8>(
			EAngelscriptCachedTypeLayoutInputKind::BaseType) - 1u);
		const uint8 Code = 1u << (static_cast<uint8>(
			EAngelscriptCachedTypeLayoutInputKind::CodeRoot) - 1u);
		const uint8 Header = 1u << (static_cast<uint8>(
			EAngelscriptCachedTypeLayoutInputKind::StructHeader) - 1u);
		switch (Form)
		{
		case ELayoutInputForm::ClassNoneWithBase:
			return Base;
		case ELayoutInputForm::OrdinaryUClassRoot:
			return Code;
		case ELayoutInputForm::OrdinaryUClassDerived:
			return Base | Code;
		case ELayoutInputForm::ReflectedUStruct:
			return Header;
		default:
			return 0;
		}
	}

	static int32 FindLayoutInputIndex(
		const FAngelscriptCachedTypeSchema& Value,
		const EAngelscriptCachedTypeLayoutInputKind Kind)
	{
		for (int32 Index = 0; Index < Value.LayoutInputs.Num(); ++Index)
		{
			if (Value.LayoutInputs[Index].InputKind == Kind)
			{
				return Index;
			}
		}
		return INDEX_NONE;
	}

	static int32 FindRelationIndex(
		const FAngelscriptCachedTypeSchema& Value,
		const EAngelscriptCachedTypeRelationKind Kind)
	{
		for (int32 Index = 0; Index < Value.Relations.Num(); ++Index)
		{
			if (Value.Relations[Index].RelationKind == Kind)
			{
				return Index;
			}
		}
		return INDEX_NONE;
	}

	static FAngelscriptCacheValidationResult ValidateLayoutInputsCrossFieldClosure(
		const FAngelscriptCachedTypeSchema& Value,
		FAngelscriptTypeSchemaFieldCoordinate* OutFailureCoordinate)
	{
		const uint8 RequiredMask = GetLayoutInputRoleMask(GetLayoutInputForm(Value));
		uint8 PresentMask = 0;
		for (int32 Index = 0; Index < Value.LayoutInputs.Num(); ++Index)
		{
			const uint8 RoleBit = 1u << (static_cast<uint8>(
				Value.LayoutInputs[Index].InputKind) - 1u);
			if ((RequiredMask & RoleBit) == 0)
			{
				return FailureAt(EAngelscriptCacheValidationError::InvalidPresence,
					OutFailureCoordinate,
					EAngelscriptTypeSchemaCapturedField::LayoutInput, Index);
			}
			PresentMask |= RoleBit;
		}

		const uint8 MissingMask = RequiredMask & ~PresentMask;
		if (MissingMask == 0)
		{
			return {};
		}
		const uint8 BaseBit = 1u << (static_cast<uint8>(
			EAngelscriptCachedTypeLayoutInputKind::BaseType) - 1u);
		if ((MissingMask & BaseBit) != 0)
		{
			const int32 BaseRelationIndex = FindRelationIndex(
				Value, EAngelscriptCachedTypeRelationKind::Base);
			return FailureAt(EAngelscriptCacheValidationError::InvalidPresence,
				OutFailureCoordinate,
				EAngelscriptTypeSchemaCapturedField::Relation,
				BaseRelationIndex == INDEX_NONE
					? MAX_uint32 : static_cast<uint32>(BaseRelationIndex));
		}
		return FailureAt(EAngelscriptCacheValidationError::InvalidPresence,
			OutFailureCoordinate,
			EAngelscriptTypeSchemaCapturedField::Reflection);
	}

	static FAngelscriptCacheValidationResult ValidateRelationLayoutInputPairing(
		const FAngelscriptCachedTypeSchema& Value,
		FAngelscriptTypeSchemaFieldCoordinate* OutFailureCoordinate)
	{
		const int32 BaseInputIndex = FindLayoutInputIndex(
			Value, EAngelscriptCachedTypeLayoutInputKind::BaseType);
		if (BaseInputIndex != INDEX_NONE)
		{
			const int32 BaseRelationIndex = FindRelationIndex(
				Value, EAngelscriptCachedTypeRelationKind::Base);
			if (BaseRelationIndex == INDEX_NONE
				|| !(Value.LayoutInputs[BaseInputIndex].Target
					== Value.Relations[BaseRelationIndex].Target))
			{
				return FailureAt(
					EAngelscriptCacheValidationError::InvalidQualifierCombination,
					OutFailureCoordinate,
					EAngelscriptTypeSchemaCapturedField::LayoutInput,
					static_cast<uint32>(BaseInputIndex));
			}
		}

		const int32 CodeInputIndex = FindLayoutInputIndex(
			Value, EAngelscriptCachedTypeLayoutInputKind::CodeRoot);
		if (CodeInputIndex != INDEX_NONE)
		{
			const int32 ShadowRelationIndex = FindRelationIndex(
				Value, EAngelscriptCachedTypeRelationKind::ShadowSuper);
			const int32 CodeRelationIndex = FindRelationIndex(
				Value, EAngelscriptCachedTypeRelationKind::CodeSuper);
			const FAngelscriptCacheStableReference& InputTarget =
				Value.LayoutInputs[CodeInputIndex].Target;
			if (ShadowRelationIndex == INDEX_NONE || CodeRelationIndex == INDEX_NONE
				|| !(InputTarget == Value.Relations[ShadowRelationIndex].Target)
				|| !(InputTarget == Value.Relations[CodeRelationIndex].Target))
			{
				return FailureAt(
					EAngelscriptCacheValidationError::InvalidQualifierCombination,
					OutFailureCoordinate,
					EAngelscriptTypeSchemaCapturedField::LayoutInput,
					static_cast<uint32>(CodeInputIndex));
			}
		}
		return {};
	}

	static bool IsStaticsClassForm(const FAngelscriptCachedTypeSchema& Value);

	static bool AreTypeKeysEqual(
		const FAngelscriptStableTypeKey& A,
		const FAngelscriptStableTypeKey& B)
	{
		return A.Hash == B.Hash;
	}

	static bool AreMethodsAllowed(const FAngelscriptCachedTypeSchema& Value)
	{
		return Value.TypeKind == EAngelscriptCachedTypeKind::Class
			|| Value.TypeKind == EAngelscriptCachedTypeKind::Struct
			|| Value.TypeKind == EAngelscriptCachedTypeKind::Interface
			|| Value.TypeKind == EAngelscriptCachedTypeKind::Delegate;
	}

	static bool AreInheritedMethodsAllowed(
		const FAngelscriptCachedTypeSchema& Value)
	{
		return Value.TypeKind == EAngelscriptCachedTypeKind::Class
			|| Value.TypeKind == EAngelscriptCachedTypeKind::Interface;
	}

	static bool IsVirtualFunctionTableAllowed(
		const FAngelscriptCachedTypeSchema& Value)
	{
		return Value.TypeKind == EAngelscriptCachedTypeKind::Class
			|| Value.TypeKind == EAngelscriptCachedTypeKind::Interface;
	}

	static FAngelscriptCacheValidationResult ValidateMethodEntriesFieldLocal(
		const FAngelscriptCachedTypeSchema& Value,
		FAngelscriptTypeSchemaFieldCoordinate* OutFailureCoordinate)
	{
		for (int32 Index = 0; Index < Value.OrderedMethods.Num(); ++Index)
		{
			const FAngelscriptCachedMethodEntry& Method = Value.OrderedMethods[Index];
			const uint8 RawKind = static_cast<uint8>(Method.EntryKind);
			if (RawKind < 1 || RawKind > 4)
			{
				return FailureAt(EAngelscriptCacheValidationError::UnknownEnumValue,
					OutFailureCoordinate,
					EAngelscriptTypeSchemaCapturedField::OrderedMethod, Index);
			}
		}

		for (int32 Index = 0; Index < Value.OrderedMethods.Num(); ++Index)
		{
			const FAngelscriptCachedMethodEntry& Method = Value.OrderedMethods[Index];
			if (Method.FunctionKey.Hash.IsZero())
			{
				return FailureAt(EAngelscriptCacheValidationError::ZeroStableKey,
					OutFailureCoordinate,
					EAngelscriptTypeSchemaCapturedField::MethodFunction, Index);
			}
			if (Method.DeclaringOwner.Hash.IsZero())
			{
				return FailureAt(EAngelscriptCacheValidationError::ZeroStableKey,
					OutFailureCoordinate,
					EAngelscriptTypeSchemaCapturedField::MethodDeclaringOwner, Index);
			}
			if (Method.ExpectedDeclarationAbi.IsZero())
			{
				return FailureAt(EAngelscriptCacheValidationError::MissingExpectedAbi,
					OutFailureCoordinate,
					EAngelscriptTypeSchemaCapturedField::OrderedMethod, Index);
			}
		}

		for (int32 Index = 0; Index < Value.OrderedMethods.Num(); ++Index)
		{
			const uint32 Ordinal = Value.OrderedMethods[Index].MethodOrdinal;
			if (Ordinal == static_cast<uint32>(Index))
			{
				continue;
			}
			if (Index > 0
				&& Ordinal == Value.OrderedMethods[Index - 1].MethodOrdinal)
			{
				return FailureAt(EAngelscriptCacheValidationError::DuplicateOrdinal,
					OutFailureCoordinate,
					EAngelscriptTypeSchemaCapturedField::OrderedMethod, Index);
			}
			bool bExpectedOrdinalAppearsLater = false;
			for (int32 LaterIndex = Index + 1;
				LaterIndex < Value.OrderedMethods.Num(); ++LaterIndex)
			{
				bExpectedOrdinalAppearsLater |=
					Value.OrderedMethods[LaterIndex].MethodOrdinal
						== static_cast<uint32>(Index);
			}
			return FailureAt(Ordinal < static_cast<uint32>(Index)
					|| bExpectedOrdinalAppearsLater
					? EAngelscriptCacheValidationError::NonCanonicalOrder
					: EAngelscriptCacheValidationError::OrdinalGap,
				OutFailureCoordinate,
				EAngelscriptTypeSchemaCapturedField::OrderedMethod, Index);
		}

		// StaticsClass is a later Reflection discriminator.  The field-local pass
		// must validate an otherwise well-formed Class method row without looking
		// ahead; ReflectionFormClosure owns the form-selected empty-array rule.
		const bool bMethodsAllowed = AreMethodsAllowed(Value);
		for (int32 Index = 0; Index < Value.OrderedMethods.Num(); ++Index)
		{
			const FAngelscriptCachedMethodEntry& Method = Value.OrderedMethods[Index];
			if (!bMethodsAllowed)
			{
				return FailureAt(EAngelscriptCacheValidationError::InvalidPresence,
					OutFailureCoordinate,
					EAngelscriptTypeSchemaCapturedField::OrderedMethod, Index);
			}
			const bool bLocal =
				Method.EntryKind == EAngelscriptCachedMethodSlotKind::LocalMethod;
			const bool bInherited =
				Method.EntryKind == EAngelscriptCachedMethodSlotKind::Inherited;
			if ((!bLocal && !bInherited)
				|| (bInherited && !AreInheritedMethodsAllowed(Value))
				|| (bLocal && !AreTypeKeysEqual(Method.DeclaringOwner, Value.TypeKey))
				|| (bInherited && AreTypeKeysEqual(Method.DeclaringOwner, Value.TypeKey)))
			{
				return FailureAt(
					EAngelscriptCacheValidationError::InvalidQualifierCombination,
					OutFailureCoordinate,
					EAngelscriptTypeSchemaCapturedField::OrderedMethod, Index);
			}
		}

		for (int32 Index = 0; Index < Value.OrderedMethods.Num(); ++Index)
		{
			for (int32 PreviousIndex = 0; PreviousIndex < Index; ++PreviousIndex)
			{
				if (Value.OrderedMethods[PreviousIndex].FunctionKey.Hash
					== Value.OrderedMethods[Index].FunctionKey.Hash)
				{
					return FailureAt(EAngelscriptCacheValidationError::DuplicateKey,
						OutFailureCoordinate,
						EAngelscriptTypeSchemaCapturedField::OrderedMethod, Index);
				}
			}
		}
		return {};
	}

	static FAngelscriptCacheValidationResult ValidateVirtualFunctionTableFieldLocal(
		const FAngelscriptCachedTypeSchema& Value,
		FAngelscriptTypeSchemaFieldCoordinate* OutFailureCoordinate)
	{
		for (int32 Index = 0; Index < Value.VirtualFunctionTable.Num(); ++Index)
		{
			const FAngelscriptCachedVirtualFunctionSlot& Slot =
				Value.VirtualFunctionTable[Index];
			const uint8 RawKind = static_cast<uint8>(Slot.SlotKind);
			if (RawKind < 1 || RawKind > 4)
			{
				return FailureAt(EAngelscriptCacheValidationError::UnknownEnumValue,
					OutFailureCoordinate,
					EAngelscriptTypeSchemaCapturedField::VirtualFunctionSlot, Index);
			}
		}

		for (int32 Index = 0; Index < Value.VirtualFunctionTable.Num(); ++Index)
		{
			const FAngelscriptCachedVirtualFunctionSlot& Slot =
				Value.VirtualFunctionTable[Index];
			if (Slot.FunctionKey.Hash.IsZero())
			{
				return FailureAt(EAngelscriptCacheValidationError::ZeroStableKey,
					OutFailureCoordinate,
					EAngelscriptTypeSchemaCapturedField::VirtualFunction, Index);
			}
			if (Slot.DeclaringOwner.Hash.IsZero())
			{
				return FailureAt(EAngelscriptCacheValidationError::ZeroStableKey,
					OutFailureCoordinate,
					EAngelscriptTypeSchemaCapturedField::VirtualDeclaringOwner, Index);
			}
			if (Slot.ImplementingOwner.Hash.IsZero())
			{
				return FailureAt(EAngelscriptCacheValidationError::ZeroStableKey,
					OutFailureCoordinate,
					EAngelscriptTypeSchemaCapturedField::VirtualImplementingOwner, Index);
			}
			if (Slot.ExpectedDeclarationAbi.IsZero())
			{
				return FailureAt(EAngelscriptCacheValidationError::MissingExpectedAbi,
					OutFailureCoordinate,
					EAngelscriptTypeSchemaCapturedField::VirtualFunctionSlot, Index);
			}
		}

		for (int32 Index = 0; Index < Value.VirtualFunctionTable.Num(); ++Index)
		{
			const uint32 Ordinal = Value.VirtualFunctionTable[Index].VftOrdinal;
			if (Ordinal == static_cast<uint32>(Index))
			{
				continue;
			}
			if (Index > 0
				&& Ordinal == Value.VirtualFunctionTable[Index - 1].VftOrdinal)
			{
				return FailureAt(EAngelscriptCacheValidationError::DuplicateOrdinal,
					OutFailureCoordinate,
					EAngelscriptTypeSchemaCapturedField::VirtualFunctionSlot, Index);
			}
			bool bExpectedOrdinalAppearsLater = false;
			for (int32 LaterIndex = Index + 1;
				LaterIndex < Value.VirtualFunctionTable.Num(); ++LaterIndex)
			{
				bExpectedOrdinalAppearsLater |=
					Value.VirtualFunctionTable[LaterIndex].VftOrdinal
						== static_cast<uint32>(Index);
			}
			return FailureAt(Ordinal < static_cast<uint32>(Index)
					|| bExpectedOrdinalAppearsLater
					? EAngelscriptCacheValidationError::NonCanonicalOrder
					: EAngelscriptCacheValidationError::OrdinalGap,
				OutFailureCoordinate,
				EAngelscriptTypeSchemaCapturedField::VirtualFunctionSlot, Index);
		}

		// As with OrderedMethods, statics-specific absence is a later reflection
		// closure rule rather than a field-local VFT rule.
		const bool bVftAllowed = IsVirtualFunctionTableAllowed(Value);
		for (int32 Index = 0; Index < Value.VirtualFunctionTable.Num(); ++Index)
		{
			const FAngelscriptCachedVirtualFunctionSlot& Slot =
				Value.VirtualFunctionTable[Index];
			if (!bVftAllowed)
			{
				return FailureAt(EAngelscriptCacheValidationError::InvalidPresence,
					OutFailureCoordinate,
					EAngelscriptTypeSchemaCapturedField::VirtualFunctionSlot, Index);
			}

			const bool bDeclaringSelf = AreTypeKeysEqual(
				Slot.DeclaringOwner, Value.TypeKey);
			const bool bImplementingSelf = AreTypeKeysEqual(
				Slot.ImplementingOwner, Value.TypeKey);
			const bool bValidRole =
				(Slot.SlotKind
						== EAngelscriptCachedMethodSlotKind::VirtualDeclaration
					&& bDeclaringSelf && bImplementingSelf)
				|| (Slot.SlotKind
						== EAngelscriptCachedMethodSlotKind::VirtualOverride
					&& !bDeclaringSelf && bImplementingSelf)
				|| (Slot.SlotKind == EAngelscriptCachedMethodSlotKind::Inherited
					&& !bDeclaringSelf && !bImplementingSelf);
			if (!bValidRole)
			{
				return FailureAt(
					EAngelscriptCacheValidationError::InvalidQualifierCombination,
					OutFailureCoordinate,
					EAngelscriptTypeSchemaCapturedField::VirtualFunctionSlot, Index);
			}
		}

		for (int32 Index = 0; Index < Value.VirtualFunctionTable.Num(); ++Index)
		{
			for (int32 PreviousIndex = 0; PreviousIndex < Index; ++PreviousIndex)
			{
				if (Value.VirtualFunctionTable[PreviousIndex].FunctionKey.Hash
					== Value.VirtualFunctionTable[Index].FunctionKey.Hash)
				{
					return FailureAt(EAngelscriptCacheValidationError::DuplicateKey,
						OutFailureCoordinate,
						EAngelscriptTypeSchemaCapturedField::VirtualFunctionSlot, Index);
				}
			}
		}
		return {};
	}

	static bool IsStaticsClassForm(const FAngelscriptCachedTypeSchema& Value)
	{
		return Value.TypeKind == EAngelscriptCachedTypeKind::Class
			&& Value.Reflection.ReflectionKind == EAngelscriptCachedReflectionKind::UClass
			&& (Value.Reflection.ClassReflectionFlags & static_cast<uint32>(
				EAngelscriptCachedClassReflectionFlags::StaticsClass)) != 0;
	}

	static bool IsPowerOfTwo(const uint32 Value)
	{
		return Value != 0 && (Value & (Value - 1u)) == 0;
	}

	static bool IsPropertyQualifierAndStorageCombinationValid(
		const FAngelscriptCachedPropertySchema& Property)
	{
		const uint32 Qualifiers = Property.Type.QualifierFlags;
		switch (Property.StorageKind)
		{
		case EAngelscriptCachedPropertyStorageKind::InlineValue:
			switch (Property.Type.Kind)
			{
			case EAngelscriptCachedDataTypeKind::Primitive:
				return Qualifiers == 0
					&& Property.Type.Primitive
						!= EAngelscriptCachedPrimitiveType::Void;
			case EAngelscriptCachedDataTypeKind::ScriptType:
			case EAngelscriptCachedDataTypeKind::EnvironmentType:
				return Qualifiers == 0
					|| Qualifiers == static_cast<uint32>(
						EAngelscriptCachedTypeQualifierFlags::ObjectConst);
			default:
				return false;
			}
		case EAngelscriptCachedPropertyStorageKind::ObjectHandle:
			if (Property.Type.Kind != EAngelscriptCachedDataTypeKind::ScriptType
				&& Property.Type.Kind
					!= EAngelscriptCachedDataTypeKind::EnvironmentType)
			{
				return false;
			}
			{
				const uint32 Required = static_cast<uint32>(
					EAngelscriptCachedTypeQualifierFlags::ObjectHandle);
				const uint32 Optional = static_cast<uint32>(
					EAngelscriptCachedTypeQualifierFlags::ObjectConst)
					| static_cast<uint32>(
						EAngelscriptCachedTypeQualifierFlags::ConstHandle)
					| static_cast<uint32>(
						EAngelscriptCachedTypeQualifierFlags::IfHandleThenConst);
				return (Qualifiers & Required) != 0
					&& (Qualifiers & ~(Required | Optional)) == 0;
			}
		default:
			return false;
		}
	}

	static FAngelscriptCacheValidationResult ValidatePropertyFlagsFieldLocal(
		const FAngelscriptCachedPropertySchema& Property)
	{
		const uint32 KnownFlags = static_cast<uint32>(
			EAngelscriptCachedPropertySemanticFlags::KnownMask);
		if ((Property.PropertySemanticFlags & ~KnownFlags) != 0)
		{
			return Failure(EAngelscriptCacheValidationError::UnknownFlags);
		}
		const uint8 RawCondition = static_cast<uint8>(Property.ReplicationCondition);
		if (RawCondition > static_cast<uint8>(
			EAngelscriptCachedReplicationCondition::NetGroup))
		{
			return Failure(EAngelscriptCacheValidationError::UnknownEnumValue);
		}

		const uint32 HasUnrealProperty = static_cast<uint32>(
			EAngelscriptCachedPropertySemanticFlags::HasUnrealProperty);
		const uint32 BlueprintReadable = static_cast<uint32>(
			EAngelscriptCachedPropertySemanticFlags::BlueprintReadable);
		const uint32 BlueprintWritable = static_cast<uint32>(
			EAngelscriptCachedPropertySemanticFlags::BlueprintWritable);
		const uint32 Replicated = static_cast<uint32>(
			EAngelscriptCachedPropertySemanticFlags::Replicated);
		const uint32 SkipReplication = static_cast<uint32>(
			EAngelscriptCachedPropertySemanticFlags::SkipReplication);
		const uint32 RepNotify = static_cast<uint32>(
			EAngelscriptCachedPropertySemanticFlags::RepNotify);
		const uint32 Flags = Property.PropertySemanticFlags;
		const bool bReplicated = (Flags & Replicated) != 0;
		const bool bRepNotify = (Flags & RepNotify) != 0;
		const bool bSkipReplication = (Flags & SkipReplication) != 0;
		const bool bHasReplicationCondition =
			Property.ReplicationCondition != EAngelscriptCachedReplicationCondition::None;

		if ((Flags != 0 && (Flags & HasUnrealProperty) == 0)
			|| ((Flags & BlueprintWritable) != 0
				&& (Flags & BlueprintReadable) == 0)
			|| (bRepNotify && !bReplicated)
			|| (bHasReplicationCondition && !bReplicated)
			|| (bSkipReplication
				&& (bReplicated || bRepNotify || bHasReplicationCondition))
			|| Property.ReplicationCondition
				== EAngelscriptCachedReplicationCondition::NetGroup)
		{
			return Failure(
				EAngelscriptCacheValidationError::InvalidQualifierCombination);
		}

		if (bRepNotify)
		{
			bool bHasReplicatedUsing = false;
			for (const FAngelscriptCachedMetadataEntry& Entry : Property.Metadata)
			{
				if (CompareString(Entry.CanonicalKey, TEXT("ReplicatedUsing")) == 0)
				{
					bHasReplicatedUsing = !Entry.CanonicalValue.IsEmpty();
					break;
				}
			}
			if (!bHasReplicatedUsing)
			{
				return Failure(
					EAngelscriptCacheValidationError::InvalidQualifierCombination);
			}
		}
		return {};
	}

	static FAngelscriptCacheValidationResult ValidatePropertyEntriesFieldLocal(
		const FAngelscriptCachedTypeSchema& Value,
		FAngelscriptTypeSchemaFieldCoordinate* OutFailureCoordinate)
	{
		for (int32 Index = 0; Index < Value.OrderedProperties.Num(); ++Index)
		{
			const FAngelscriptCachedPropertySchema& Property =
				Value.OrderedProperties[Index];
			const uint32 ExpectedOrdinal = static_cast<uint32>(Index);
			if (Property.LayoutOrdinal != ExpectedOrdinal)
			{
				if (Index > 0 && Property.LayoutOrdinal
					== Value.OrderedProperties[Index - 1].LayoutOrdinal)
				{
					return FailureAt(EAngelscriptCacheValidationError::DuplicateOrdinal,
						OutFailureCoordinate,
						EAngelscriptTypeSchemaCapturedField::OrderedProperty, Index);
				}
				bool bExpectedOrdinalAppearsLater = false;
				for (int32 LaterIndex = Index + 1;
					LaterIndex < Value.OrderedProperties.Num(); ++LaterIndex)
				{
					bExpectedOrdinalAppearsLater |=
						Value.OrderedProperties[LaterIndex].LayoutOrdinal
							== ExpectedOrdinal;
				}
				return FailureAt(
					bExpectedOrdinalAppearsLater
						? EAngelscriptCacheValidationError::NonCanonicalOrder
						: EAngelscriptCacheValidationError::OrdinalGap,
					OutFailureCoordinate,
					EAngelscriptTypeSchemaCapturedField::OrderedProperty, Index);
			}
			if (Property.SemanticByteOffset > static_cast<uint32>(MAX_int32))
			{
				return FailureAt(EAngelscriptCacheValidationError::Overflow,
					OutFailureCoordinate,
					EAngelscriptTypeSchemaCapturedField::OrderedProperty, Index);
			}
			if (Property.PropertyKey.Hash.IsZero())
			{
				return FailureAt(EAngelscriptCacheValidationError::ZeroStableKey,
					OutFailureCoordinate,
					EAngelscriptTypeSchemaCapturedField::OrderedProperty, Index);
			}
			if (const FAngelscriptCacheValidationResult Result =
				ValidateString(Property.CanonicalName, true); !Result.IsSuccess())
			{
				return FailureAt(Result.Error, OutFailureCoordinate,
					EAngelscriptTypeSchemaCapturedField::OrderedProperty, Index);
			}
			if (const FAngelscriptCacheValidationResult Result =
				ValidateDataType(Property.Type); !Result.IsSuccess())
			{
				return FailureAt(Result.Error, OutFailureCoordinate,
					EAngelscriptTypeSchemaCapturedField::OrderedProperty, Index);
			}
			const uint8 RawStorageKind = static_cast<uint8>(Property.StorageKind);
			if (RawStorageKind < 1 || RawStorageKind > 2)
			{
				return FailureAt(EAngelscriptCacheValidationError::UnknownEnumValue,
					OutFailureCoordinate,
					EAngelscriptTypeSchemaCapturedField::OrderedProperty, Index);
			}
			if (!IsPropertyQualifierAndStorageCombinationValid(Property))
			{
				return FailureAt(
					EAngelscriptCacheValidationError::InvalidQualifierCombination,
					OutFailureCoordinate,
					EAngelscriptTypeSchemaCapturedField::OrderedProperty, Index);
			}
			if (Property.SemanticStorageSize > static_cast<uint32>(MAX_int32)
				|| Property.SemanticStorageAlignment > static_cast<uint32>(MAX_int32))
			{
				return FailureAt(EAngelscriptCacheValidationError::Overflow,
					OutFailureCoordinate,
					EAngelscriptTypeSchemaCapturedField::OrderedProperty, Index);
			}
			if (Property.SemanticStorageSize == 0
				|| !IsPowerOfTwo(Property.SemanticStorageAlignment))
			{
				return FailureAt(
					EAngelscriptCacheValidationError::InvalidQualifierCombination,
					OutFailureCoordinate,
					EAngelscriptTypeSchemaCapturedField::OrderedProperty, Index);
			}
			if (uint64(Property.SemanticByteOffset)
					+ uint64(Property.SemanticStorageSize)
				> static_cast<uint64>(MAX_int32))
			{
				return FailureAt(EAngelscriptCacheValidationError::Overflow,
					OutFailureCoordinate,
					EAngelscriptTypeSchemaCapturedField::OrderedProperty, Index);
			}

			FAngelscriptHash256 ExpectedStorage;
			if (const FAngelscriptCacheValidationResult Result =
				FAngelscriptCacheTypeSchemaArchive::ComputeStorageLayoutHash(
					Property.Type, Property.StorageKind,
					Property.SemanticStorageSize,
					Property.SemanticStorageAlignment, ExpectedStorage);
				!Result.IsSuccess())
			{
				return FailureAt(Result.Error, OutFailureCoordinate,
					EAngelscriptTypeSchemaCapturedField::OrderedProperty, Index);
			}
			if (!(ExpectedStorage == Property.StorageLayoutHash))
			{
				return FailureAt(EAngelscriptCacheValidationError::DerivedHashMismatch,
					OutFailureCoordinate,
					EAngelscriptTypeSchemaCapturedField::OrderedProperty, Index);
			}

			const uint8 RawAccess = static_cast<uint8>(Property.Access);
			if (RawAccess < 1 || RawAccess > 3)
			{
				return FailureAt(EAngelscriptCacheValidationError::UnknownEnumValue,
					OutFailureCoordinate,
					EAngelscriptTypeSchemaCapturedField::OrderedProperty, Index);
			}
			if (const FAngelscriptCacheValidationResult Result =
				ValidatePropertyFlagsFieldLocal(Property); !Result.IsSuccess())
			{
				return FailureAt(Result.Error, OutFailureCoordinate,
					EAngelscriptTypeSchemaCapturedField::OrderedProperty, Index);
			}
			if (const FAngelscriptCacheValidationResult Result =
				ValidateMetadataArray(Property.Metadata); !Result.IsSuccess())
			{
				return FailureAt(Result.Error, OutFailureCoordinate,
					EAngelscriptTypeSchemaCapturedField::OrderedProperty, Index);
			}

			FAngelscriptHash256 ExpectedProperty;
			if (const FAngelscriptCacheValidationResult Result =
				FAngelscriptCacheTypeSchemaArchive::ComputePropertyLayoutFingerprint(
					Value.TypeKey, Property, ExpectedProperty); !Result.IsSuccess())
			{
				return FailureAt(Result.Error, OutFailureCoordinate,
					EAngelscriptTypeSchemaCapturedField::OrderedProperty, Index);
			}
			if (!(ExpectedProperty == Property.PropertyLayoutFingerprint))
			{
				return FailureAt(EAngelscriptCacheValidationError::DerivedHashMismatch,
					OutFailureCoordinate,
					EAngelscriptTypeSchemaCapturedField::OrderedProperty, Index);
			}
		}
		return {};
	}

	static FAngelscriptCacheValidationResult ValidatePropertyOwnerAndFlagsClosure(
		const FAngelscriptCachedTypeSchema& Value,
		FAngelscriptTypeSchemaFieldCoordinate* OutFailureCoordinate)
	{
		bool bPropertiesAllowed = false;
		switch (Value.TypeKind)
		{
		case EAngelscriptCachedTypeKind::Class:
			bPropertiesAllowed =
				Value.Reflection.ReflectionKind == EAngelscriptCachedReflectionKind::None
				|| (Value.Reflection.ReflectionKind
						== EAngelscriptCachedReflectionKind::UClass
					&& !IsStaticsClassForm(Value));
			break;
		case EAngelscriptCachedTypeKind::Struct:
			bPropertiesAllowed =
				Value.Reflection.ReflectionKind == EAngelscriptCachedReflectionKind::None
				|| Value.Reflection.ReflectionKind
					== EAngelscriptCachedReflectionKind::UStruct;
			break;
		case EAngelscriptCachedTypeKind::Delegate:
			bPropertiesAllowed = Value.Reflection.ReflectionKind
				== EAngelscriptCachedReflectionKind::UDelegate;
			break;
		default:
			break;
		}
		if (!bPropertiesAllowed && !Value.OrderedProperties.IsEmpty())
		{
			return FailureAt(EAngelscriptCacheValidationError::InvalidPresence,
				OutFailureCoordinate,
				EAngelscriptTypeSchemaCapturedField::OrderedProperty, 0);
		}

		const uint32 SkipReplication = static_cast<uint32>(
			EAngelscriptCachedPropertySemanticFlags::SkipReplication);
		const uint32 StructForbidden = static_cast<uint32>(
			EAngelscriptCachedPropertySemanticFlags::Replicated)
			| static_cast<uint32>(EAngelscriptCachedPropertySemanticFlags::RepNotify)
			| static_cast<uint32>(EAngelscriptCachedPropertySemanticFlags::Config);
		for (int32 Index = 0; Index < Value.OrderedProperties.Num(); ++Index)
		{
			const uint32 Flags = Value.OrderedProperties[Index].PropertySemanticFlags;
			bool bOwnerAllowsFlags = Flags == 0;
			if (Value.TypeKind == EAngelscriptCachedTypeKind::Class
				&& Value.Reflection.ReflectionKind
					== EAngelscriptCachedReflectionKind::UClass
				&& !IsStaticsClassForm(Value))
			{
				bOwnerAllowsFlags = (Flags & SkipReplication) == 0;
			}
			else if (Value.TypeKind == EAngelscriptCachedTypeKind::Struct
				&& Value.Reflection.ReflectionKind
					== EAngelscriptCachedReflectionKind::UStruct)
			{
				bOwnerAllowsFlags = (Flags & StructForbidden) == 0;
			}
			if (!bOwnerAllowsFlags)
			{
				return FailureAt(EAngelscriptCacheValidationError::InvalidPresence,
					OutFailureCoordinate,
					EAngelscriptTypeSchemaCapturedField::OrderedProperty, Index);
			}
		}
		return {};
	}

	static FAngelscriptCacheValidationResult ValidatePropertyLayoutReplay(
		const FAngelscriptCachedTypeSchema& Value,
		FAngelscriptTypeSchemaFieldCoordinate* OutFailureCoordinate)
	{
		const bool bNormalClass = Value.TypeKind == EAngelscriptCachedTypeKind::Class
			&& (Value.Reflection.ReflectionKind
					== EAngelscriptCachedReflectionKind::None
				|| (Value.Reflection.ReflectionKind
						== EAngelscriptCachedReflectionKind::UClass
					&& !IsStaticsClassForm(Value)));
		const bool bNormalStruct = Value.TypeKind == EAngelscriptCachedTypeKind::Struct
			&& (Value.Reflection.ReflectionKind
					== EAngelscriptCachedReflectionKind::None
				|| Value.Reflection.ReflectionKind
					== EAngelscriptCachedReflectionKind::UStruct);
		const bool bNormalDelegate = Value.TypeKind
				== EAngelscriptCachedTypeKind::Delegate
			&& Value.Reflection.ReflectionKind
				== EAngelscriptCachedReflectionKind::UDelegate;
		if (!bNormalClass && !bNormalStruct && !bNormalDelegate)
		{
			return {};
		}

		if (Value.Layout.SemanticSize > static_cast<uint64>(MAX_int32)
			|| Value.Layout.SemanticAlignment > static_cast<uint32>(MAX_int32)
			|| Value.Layout.BasePropertyBoundary > static_cast<uint32>(MAX_int32))
		{
			return FailureAt(EAngelscriptCacheValidationError::Overflow,
				OutFailureCoordinate,
				EAngelscriptTypeSchemaCapturedField::LayoutExpectation);
		}

		uint32 ExpectedBoundary = 0;
		uint32 ExpectedAlignment =
			FAngelscriptCacheTypeSchemaArchive::GetV1BuildLayoutConstants()
				.ObjectInitialAlignment;
		const EAngelscriptCachedTypeLayoutInputKind InputKinds[] = {
			EAngelscriptCachedTypeLayoutInputKind::BaseType,
			EAngelscriptCachedTypeLayoutInputKind::CodeRoot,
			EAngelscriptCachedTypeLayoutInputKind::StructHeader,
		};
		for (const EAngelscriptCachedTypeLayoutInputKind InputKind : InputKinds)
		{
			const int32 Index = FindLayoutInputIndex(Value, InputKind);
			if (Index == INDEX_NONE)
			{
				continue;
			}
			const FAngelscriptCachedTypeLayoutInput& Input = Value.LayoutInputs[Index];
			if (ExpectedBoundary == 0 && Input.BoundaryContribution.IsSet())
			{
				ExpectedBoundary = Input.BoundaryContribution.GetValue();
			}
			if (Input.AlignmentContribution.IsSet())
			{
				ExpectedAlignment = FMath::Max(
					ExpectedAlignment, Input.AlignmentContribution.GetValue());
			}
		}
		for (const FAngelscriptCachedPropertySchema& Property : Value.OrderedProperties)
		{
			ExpectedAlignment = FMath::Max(
				ExpectedAlignment, Property.SemanticStorageAlignment);
		}
		if (!IsPowerOfTwo(ExpectedAlignment)
			|| Value.Layout.SemanticAlignment != ExpectedAlignment
			|| Value.Layout.BasePropertyBoundary != ExpectedBoundary
			|| Value.Layout.BasePropertyBoundary > Value.Layout.SemanticSize)
		{
			return FailureAt(
				EAngelscriptCacheValidationError::InvalidQualifierCombination,
				OutFailureCoordinate,
				EAngelscriptTypeSchemaCapturedField::LayoutExpectation);
		}

		uint64 Cursor = ExpectedBoundary;
		for (int32 Index = 0; Index < Value.OrderedProperties.Num(); ++Index)
		{
			const FAngelscriptCachedPropertySchema& Property =
				Value.OrderedProperties[Index];
			const uint64 Alignment = Property.SemanticStorageAlignment;
			const uint64 AlignedCursor = (Cursor + Alignment - 1u)
				& ~(Alignment - 1u);
			if (AlignedCursor > static_cast<uint64>(MAX_int32))
			{
				return FailureAt(EAngelscriptCacheValidationError::Overflow,
					OutFailureCoordinate,
					EAngelscriptTypeSchemaCapturedField::OrderedProperty, Index);
			}
			if (Property.SemanticByteOffset != AlignedCursor)
			{
				return FailureAt(
					EAngelscriptCacheValidationError::InvalidQualifierCombination,
					OutFailureCoordinate,
					EAngelscriptTypeSchemaCapturedField::OrderedProperty, Index);
			}
			const uint64 PropertyEnd = AlignedCursor + Property.SemanticStorageSize;
			if (PropertyEnd > static_cast<uint64>(MAX_int32))
			{
				return FailureAt(EAngelscriptCacheValidationError::Overflow,
					OutFailureCoordinate,
					EAngelscriptTypeSchemaCapturedField::OrderedProperty, Index);
			}
			Cursor = PropertyEnd;
		}

		const uint64 TerminalSize = (Cursor + ExpectedAlignment - 1u)
			& ~(uint64(ExpectedAlignment) - 1u);
		if (TerminalSize > static_cast<uint64>(MAX_int32))
		{
			return FailureAt(EAngelscriptCacheValidationError::Overflow,
				OutFailureCoordinate,
				EAngelscriptTypeSchemaCapturedField::LayoutExpectation);
		}
		if (Value.Layout.SemanticSize != TerminalSize)
		{
			return FailureAt(
				EAngelscriptCacheValidationError::InvalidQualifierCombination,
				OutFailureCoordinate,
				EAngelscriptTypeSchemaCapturedField::LayoutExpectation);
		}
		return {};
	}

	static FAngelscriptCacheValidationResult ValidateReflectionFieldLocal(
		const FAngelscriptCachedTypeSchema& Value,
		FAngelscriptTypeSchemaFieldCoordinate* OutFailureCoordinate)
	{
		const uint8 RawReflectionKind = static_cast<uint8>(
			Value.Reflection.ReflectionKind);
		if (RawReflectionKind < 1 || RawReflectionKind > 5)
		{
			return FailureAt(EAngelscriptCacheValidationError::UnknownEnumValue,
				OutFailureCoordinate,
				EAngelscriptTypeSchemaCapturedField::ReflectionKind);
		}
		const uint32 KnownFlags = static_cast<uint32>(
			EAngelscriptCachedClassReflectionFlags::KnownMask);
		if ((Value.Reflection.ClassReflectionFlags & ~KnownFlags) != 0)
		{
			return FailureAt(EAngelscriptCacheValidationError::UnknownFlags,
				OutFailureCoordinate,
				EAngelscriptTypeSchemaCapturedField::ClassReflectionFlags);
		}
		if (Value.Reflection.ConfigName.IsSet())
		{
			if (const FAngelscriptCacheValidationResult Result =
				ValidateString(Value.Reflection.ConfigName.GetValue(), true);
				!Result.IsSuccess())
			{
				return FailureAt(Result.Error, OutFailureCoordinate,
					EAngelscriptTypeSchemaCapturedField::Reflection);
			}
		}
		if (Value.Reflection.StaticClassGlobalName.IsSet())
		{
			if (const FAngelscriptCacheValidationResult Result = ValidateString(
				Value.Reflection.StaticClassGlobalName.GetValue(), true);
				!Result.IsSuccess())
			{
				return FailureAt(Result.Error, OutFailureCoordinate,
					EAngelscriptTypeSchemaCapturedField::Reflection);
			}
		}

		for (int32 Index = 0;
			Index < Value.Reflection.OrderedUFunctionMembers.Num(); ++Index)
		{
			const FAngelscriptCachedReflectedFunctionMember& Member =
				Value.Reflection.OrderedUFunctionMembers[Index];
			if (const FAngelscriptCacheValidationResult Result =
				ValidateString(Member.CanonicalFunctionName, true);
				!Result.IsSuccess())
			{
				return FailureAt(Result.Error, OutFailureCoordinate,
					EAngelscriptTypeSchemaCapturedField::ReflectedFunctionName,
					Index);
			}
			if (const FAngelscriptCacheValidationResult Result =
				ValidateString(Member.CanonicalOriginalFunctionName, false);
				!Result.IsSuccess())
			{
				return FailureAt(Result.Error, OutFailureCoordinate,
					EAngelscriptTypeSchemaCapturedField::ReflectedOriginalFunctionName,
					Index);
			}
			if (const FAngelscriptCacheValidationResult Result =
				ValidateString(Member.CanonicalScriptFunctionName, true);
				!Result.IsSuccess())
			{
				return FailureAt(Result.Error, OutFailureCoordinate,
					EAngelscriptTypeSchemaCapturedField::ReflectedScriptFunctionName,
					Index);
			}
			const uint32 ExpectedOrdinal = static_cast<uint32>(Index);
			if (Member.ReflectionOrdinal != ExpectedOrdinal)
			{
				if (Index > 0 && Member.ReflectionOrdinal
					== Value.Reflection.OrderedUFunctionMembers[Index - 1]
						.ReflectionOrdinal)
				{
					return FailureAt(EAngelscriptCacheValidationError::DuplicateOrdinal,
						OutFailureCoordinate,
						EAngelscriptTypeSchemaCapturedField::ReflectedFunctionMember,
						Index);
				}
				bool bExpectedOrdinalAppearsLater = false;
				for (int32 LaterIndex = Index + 1;
					LaterIndex < Value.Reflection.OrderedUFunctionMembers.Num();
					++LaterIndex)
				{
					bExpectedOrdinalAppearsLater |=
						Value.Reflection.OrderedUFunctionMembers[LaterIndex]
							.ReflectionOrdinal == ExpectedOrdinal;
				}
				return FailureAt(
					bExpectedOrdinalAppearsLater
						? EAngelscriptCacheValidationError::NonCanonicalOrder
						: EAngelscriptCacheValidationError::OrdinalGap,
					OutFailureCoordinate,
					EAngelscriptTypeSchemaCapturedField::ReflectedFunctionMember,
					Index);
			}
			if (const FAngelscriptCacheValidationResult Result =
				ValidateStableReference(Member.Target); !Result.IsSuccess())
			{
				return FailureAt(Result.Error, OutFailureCoordinate,
					EAngelscriptTypeSchemaCapturedField::ReflectedFunctionTarget,
					Index);
			}
			if (Member.Target.Kind != EAngelscriptCacheReferenceKind::ScriptFunction)
			{
				return FailureAt(EAngelscriptCacheValidationError::WrongReferenceKind,
					OutFailureCoordinate,
					EAngelscriptTypeSchemaCapturedField::ReflectedFunctionTarget,
					Index);
			}
			for (int32 PreviousIndex = 0; PreviousIndex < Index; ++PreviousIndex)
			{
				if (Value.Reflection.OrderedUFunctionMembers[PreviousIndex]
					.Target.StableKey == Member.Target.StableKey)
				{
					return FailureAt(EAngelscriptCacheValidationError::DuplicateKey,
						OutFailureCoordinate,
						EAngelscriptTypeSchemaCapturedField::ReflectedFunctionMember,
						Index);
				}
			}
		}
		return {};
	}

	static FAngelscriptCacheValidationResult ValidateSpecialLayout(
		const FAngelscriptCachedTypeSchema& Value,
		const uint64 ExpectedSize,
		const uint32 ExpectedAlignment,
		FAngelscriptTypeSchemaFieldCoordinate* OutFailureCoordinate)
	{
		if (Value.Layout.SemanticSize != ExpectedSize
			|| Value.Layout.SemanticAlignment != ExpectedAlignment
			|| Value.Layout.BasePropertyBoundary != 0)
		{
			return FailureAt(
				EAngelscriptCacheValidationError::InvalidQualifierCombination,
				OutFailureCoordinate,
				EAngelscriptTypeSchemaCapturedField::LayoutExpectation);
		}
		return {};
	}

	static FAngelscriptCacheValidationResult ValidateReflectionFormClosure(
		const FAngelscriptCachedTypeSchema& Value,
		FAngelscriptTypeSchemaFieldCoordinate* OutFailureCoordinate)
	{
		const EAngelscriptCachedReflectionKind ReflectionKind =
			Value.Reflection.ReflectionKind;
		const bool bClassNone = Value.TypeKind == EAngelscriptCachedTypeKind::Class
			&& ReflectionKind == EAngelscriptCachedReflectionKind::None;
		const bool bUClass = Value.TypeKind == EAngelscriptCachedTypeKind::Class
			&& ReflectionKind == EAngelscriptCachedReflectionKind::UClass;
		const bool bStructNone = Value.TypeKind == EAngelscriptCachedTypeKind::Struct
			&& ReflectionKind == EAngelscriptCachedReflectionKind::None;
		const bool bUStruct = Value.TypeKind == EAngelscriptCachedTypeKind::Struct
			&& ReflectionKind == EAngelscriptCachedReflectionKind::UStruct;
		const bool bInterfaceNone =
			Value.TypeKind == EAngelscriptCachedTypeKind::Interface
			&& ReflectionKind == EAngelscriptCachedReflectionKind::None;
		const bool bEnum = Value.TypeKind == EAngelscriptCachedTypeKind::Enum
			&& (ReflectionKind == EAngelscriptCachedReflectionKind::None
				|| ReflectionKind == EAngelscriptCachedReflectionKind::UEnum);
		const bool bDelegate = Value.TypeKind == EAngelscriptCachedTypeKind::Delegate
			&& ReflectionKind == EAngelscriptCachedReflectionKind::UDelegate;
		const bool bTypedef = Value.TypeKind == EAngelscriptCachedTypeKind::Typedef
			&& ReflectionKind == EAngelscriptCachedReflectionKind::None;
		const bool bFuncdef = Value.TypeKind == EAngelscriptCachedTypeKind::Funcdef
			&& ReflectionKind == EAngelscriptCachedReflectionKind::None;
		if (!bClassNone && !bUClass && !bStructNone && !bUStruct
			&& !bInterfaceNone && !bEnum && !bDelegate && !bTypedef && !bFuncdef)
		{
			return FailureAt(EAngelscriptCacheValidationError::InvalidPresence,
				OutFailureCoordinate,
				EAngelscriptTypeSchemaCapturedField::Reflection);
		}

		const uint32 ReflectionFlags = Value.Reflection.ClassReflectionFlags;
		const uint32 SuperIsCode = static_cast<uint32>(
			EAngelscriptCachedClassReflectionFlags::SuperIsCodeClass);
		const uint32 Statics = static_cast<uint32>(
			EAngelscriptCachedClassReflectionFlags::StaticsClass);
		const uint32 Abstract = static_cast<uint32>(
			EAngelscriptCachedClassReflectionFlags::Abstract);
		const uint32 Placeable = static_cast<uint32>(
			EAngelscriptCachedClassReflectionFlags::Placeable);
		const uint32 IsStruct = static_cast<uint32>(
			EAngelscriptCachedClassReflectionFlags::IsStruct);
		const bool bHasStaticsFlag = bUClass && (ReflectionFlags & Statics) != 0;
		const bool bHasOrdinaryUClassWitness = bUClass
			&& (Value.Reflection.StaticClassGlobalName.IsSet()
				|| HasBaseRelation(Value)
				|| FindRelationIndex(Value,
					EAngelscriptCachedTypeRelationKind::ShadowSuper) != INDEX_NONE
				|| FindLayoutInputIndex(Value,
					EAngelscriptCachedTypeLayoutInputKind::CodeRoot) != INDEX_NONE);
		const uint32 Generated = static_cast<uint32>(
			EAngelscriptCachedTypeSemanticFlags::Generated);
		const uint32 Reference = static_cast<uint32>(
			EAngelscriptCachedTypeSemanticFlags::ReferenceType);
		const bool bHasStaticsUClassWitness = bUClass
			&& !bHasOrdinaryUClassWitness
			&& (Value.TypeSemanticFlags & (Generated | Reference))
				== (Generated | Reference);

		// StaticsClass is a bidirectional discriminator.  Validate contradictory
		// ordinary/statics evidence at Reflection before selecting a form, so an
		// illegal known mask cannot redirect validation into the other form and
		// steal an earlier-field diagnostic.
		if (bHasStaticsFlag && bHasOrdinaryUClassWitness)
		{
			const bool bTypeAbstract = HasTypeSemanticFlag(
				Value, EAngelscriptCachedTypeSemanticFlags::Abstract);
			const bool bParityMismatch =
				((ReflectionFlags & Abstract) != 0) != bTypeAbstract
				|| ((ReflectionFlags & SuperIsCode) != 0) == HasBaseRelation(Value);
			return FailureAt(bParityMismatch
					? EAngelscriptCacheValidationError::InvalidQualifierCombination
					: EAngelscriptCacheValidationError::InvalidPresence,
				OutFailureCoordinate,
				EAngelscriptTypeSchemaCapturedField::Reflection);
		}
		if (!bHasStaticsFlag && bHasStaticsUClassWitness)
		{
			return FailureAt(EAngelscriptCacheValidationError::InvalidPresence,
				OutFailureCoordinate,
				EAngelscriptTypeSchemaCapturedField::Reflection);
		}
		const bool bStatics = bHasStaticsFlag;

		if (bUClass)
		{
			if (bStatics)
			{
				if ((ReflectionFlags & (SuperIsCode | Statics))
						!= (SuperIsCode | Statics)
					|| (ReflectionFlags & ~(SuperIsCode | Statics | Placeable)) != 0)
				{
					return FailureAt(EAngelscriptCacheValidationError::InvalidPresence,
						OutFailureCoordinate,
						EAngelscriptTypeSchemaCapturedField::Reflection);
				}
				const uint32 Final = static_cast<uint32>(
					EAngelscriptCachedTypeSemanticFlags::Final);
				if ((Value.TypeSemanticFlags & (Generated | Reference))
						!= (Generated | Reference)
					|| (Value.TypeSemanticFlags & ~(Generated | Reference | Final)) != 0)
				{
					return FailureAt(
						EAngelscriptCacheValidationError::InvalidQualifierCombination,
						OutFailureCoordinate,
						EAngelscriptTypeSchemaCapturedField::TypeSemanticFlags);
				}
			}
			else
			{
				const uint32 OrdinaryAllowed = SuperIsCode | Abstract
					| static_cast<uint32>(EAngelscriptCachedClassReflectionFlags::Transient)
					| static_cast<uint32>(EAngelscriptCachedClassReflectionFlags::HideDropdown)
					| static_cast<uint32>(EAngelscriptCachedClassReflectionFlags::DefaultToInstanced)
					| static_cast<uint32>(EAngelscriptCachedClassReflectionFlags::EditInlineNew)
					| static_cast<uint32>(EAngelscriptCachedClassReflectionFlags::Deprecated)
					| Placeable;
				const bool bTypeAbstract = HasTypeSemanticFlag(
					Value, EAngelscriptCachedTypeSemanticFlags::Abstract);
				const bool bHasBase = HasBaseRelation(Value);
				if (((ReflectionFlags & Abstract) != 0) != bTypeAbstract
					|| ((ReflectionFlags & SuperIsCode) != 0) == bHasBase)
				{
					return FailureAt(
						EAngelscriptCacheValidationError::InvalidQualifierCombination,
						OutFailureCoordinate,
						EAngelscriptTypeSchemaCapturedField::Reflection);
				}
				if ((ReflectionFlags & ~OrdinaryAllowed) != 0)
				{
					return FailureAt(EAngelscriptCacheValidationError::InvalidPresence,
						OutFailureCoordinate,
						EAngelscriptTypeSchemaCapturedField::Reflection);
				}
			}
		}
		else if (bUStruct)
		{
			if (ReflectionFlags != IsStruct)
			{
				return FailureAt(EAngelscriptCacheValidationError::InvalidPresence,
					OutFailureCoordinate,
					EAngelscriptTypeSchemaCapturedField::Reflection);
			}
		}
		else if (ReflectionFlags != 0)
		{
			return FailureAt(EAngelscriptCacheValidationError::InvalidPresence,
				OutFailureCoordinate,
				EAngelscriptTypeSchemaCapturedField::Reflection);
		}

		if (bUClass && !bStatics)
		{
			if (!Value.Reflection.StaticClassGlobalName.IsSet())
			{
				return FailureAt(EAngelscriptCacheValidationError::InvalidPresence,
					OutFailureCoordinate,
					EAngelscriptTypeSchemaCapturedField::Reflection);
			}
		}
		else if (Value.Reflection.ConfigName.IsSet()
			|| Value.Reflection.StaticClassGlobalName.IsSet())
		{
			return FailureAt(EAngelscriptCacheValidationError::InvalidPresence,
				OutFailureCoordinate,
				EAngelscriptTypeSchemaCapturedField::Reflection);
		}

		if (!bUClass && !Value.Reflection.OrderedUFunctionMembers.IsEmpty())
		{
			return FailureAt(EAngelscriptCacheValidationError::InvalidPresence,
				OutFailureCoordinate,
				EAngelscriptTypeSchemaCapturedField::ReflectedFunctionMember, 0);
		}
		if (bStatics)
		{
			if (Value.Reflection.OrderedUFunctionMembers.IsEmpty())
			{
				return FailureAt(EAngelscriptCacheValidationError::InvalidPresence,
					OutFailureCoordinate,
					EAngelscriptTypeSchemaCapturedField::Reflection);
			}
			if (!Value.OrderedProperties.IsEmpty())
			{
				return FailureAt(EAngelscriptCacheValidationError::InvalidPresence,
					OutFailureCoordinate,
					EAngelscriptTypeSchemaCapturedField::OrderedProperty, 0);
			}
			if (!Value.OrderedMethods.IsEmpty())
			{
				return FailureAt(EAngelscriptCacheValidationError::InvalidPresence,
					OutFailureCoordinate,
					EAngelscriptTypeSchemaCapturedField::OrderedMethod, 0);
			}
			if (!Value.VirtualFunctionTable.IsEmpty())
			{
				return FailureAt(EAngelscriptCacheValidationError::InvalidPresence,
					OutFailureCoordinate,
					EAngelscriptTypeSchemaCapturedField::VirtualFunctionSlot, 0);
			}
			if (!Value.OrderedBehaviorSlots.IsEmpty())
			{
				return FailureAt(EAngelscriptCacheValidationError::InvalidPresence,
					OutFailureCoordinate,
					EAngelscriptTypeSchemaCapturedField::BehaviorSlot, 0);
			}
			if (const FAngelscriptCacheValidationResult Result = ValidateSpecialLayout(
				Value, 0, 1, OutFailureCoordinate); !Result.IsSuccess())
			{
				return Result;
			}
		}
		if (bInterfaceNone)
		{
			return ValidateSpecialLayout(Value, 0,
				FAngelscriptCacheTypeSchemaArchive::GetV1BuildLayoutConstants()
					.ObjectInitialAlignment,
				OutFailureCoordinate);
		}
		if (bEnum)
		{
			return ValidateSpecialLayout(Value, 1, 1, OutFailureCoordinate);
		}
		return {};
	}

	static bool IsBehaviorKindAllowedForType(
		const EAngelscriptCachedTypeKind TypeKind,
		const EAngelscriptCachedBehaviorKind BehaviorKind)
	{
		switch (TypeKind)
		{
		case EAngelscriptCachedTypeKind::Class:
			switch (BehaviorKind)
			{
			case EAngelscriptCachedBehaviorKind::Construct:
			case EAngelscriptCachedBehaviorKind::Destruct:
			case EAngelscriptCachedBehaviorKind::Factory:
			case EAngelscriptCachedBehaviorKind::ListFactory:
			case EAngelscriptCachedBehaviorKind::AddRef:
			case EAngelscriptCachedBehaviorKind::Release:
			case EAngelscriptCachedBehaviorKind::GetWeakRefFlag:
			case EAngelscriptCachedBehaviorKind::GetRefCount:
			case EAngelscriptCachedBehaviorKind::SetGcFlag:
			case EAngelscriptCachedBehaviorKind::GetGcFlag:
			case EAngelscriptCachedBehaviorKind::EnumRefs:
			case EAngelscriptCachedBehaviorKind::ReleaseRefs:
			case EAngelscriptCachedBehaviorKind::Copy:
			case EAngelscriptCachedBehaviorKind::CopyFactory:
				return true;
			default:
				return false;
			}
		case EAngelscriptCachedTypeKind::Struct:
		case EAngelscriptCachedTypeKind::Delegate:
			return BehaviorKind == EAngelscriptCachedBehaviorKind::Construct
				|| BehaviorKind == EAngelscriptCachedBehaviorKind::ListConstruct
				|| BehaviorKind == EAngelscriptCachedBehaviorKind::Destruct
				|| BehaviorKind == EAngelscriptCachedBehaviorKind::Copy
				|| BehaviorKind == EAngelscriptCachedBehaviorKind::CopyConstruct;
		default:
			return false;
		}
	}

	static bool IsBehaviorTargetKindAllowed(
		const EAngelscriptCachedBehaviorKind BehaviorKind,
		const EAngelscriptCacheReferenceKind TargetKind)
	{
		if (TargetKind != EAngelscriptCacheReferenceKind::ScriptFunction
			&& TargetKind != EAngelscriptCacheReferenceKind::EnvironmentSymbol)
		{
			return false;
		}
		if (BehaviorKind == EAngelscriptCachedBehaviorKind::TemplateCallback)
		{
			return false;
		}
		if (TargetKind == EAngelscriptCacheReferenceKind::EnvironmentSymbol)
		{
			return BehaviorKind != EAngelscriptCachedBehaviorKind::Construct
				&& BehaviorKind != EAngelscriptCachedBehaviorKind::ListConstruct
				&& BehaviorKind != EAngelscriptCachedBehaviorKind::Factory
				&& BehaviorKind != EAngelscriptCachedBehaviorKind::ListFactory;
		}
		return true;
	}

	static bool IsSingletonBehaviorKind(
		const EAngelscriptCachedBehaviorKind BehaviorKind)
	{
		return BehaviorKind != EAngelscriptCachedBehaviorKind::Construct
			&& BehaviorKind != EAngelscriptCachedBehaviorKind::Factory;
	}

	static bool HasExactScriptBehaviorPeer(
		const FAngelscriptCachedTypeSchema& Value,
		const FAngelscriptCachedBehaviorSlot& Alias,
		const EAngelscriptCachedBehaviorKind PeerKind)
	{
		for (const FAngelscriptCachedBehaviorSlot& Candidate : Value.OrderedBehaviorSlots)
		{
			if (Candidate.BehaviorKind == PeerKind
				&& Candidate.Target == Alias.Target
				&& Candidate.DeclaringOwner.IsSet()
				&& Alias.DeclaringOwner.IsSet()
				&& Candidate.DeclaringOwner->Hash == Alias.DeclaringOwner->Hash)
			{
				return true;
			}
		}
		return false;
	}

	static FAngelscriptCacheValidationResult ValidateBehaviorFieldLocal(
		const FAngelscriptCachedTypeSchema& Value,
		FAngelscriptTypeSchemaFieldCoordinate* OutFailureCoordinate)
	{
		uint8 PreviousKind = 0;
		uint32 PreviousOrdinal = 0;
		for (int32 Index = 0; Index < Value.OrderedBehaviorSlots.Num(); ++Index)
		{
			const FAngelscriptCachedBehaviorSlot& Slot = Value.OrderedBehaviorSlots[Index];
			const uint8 RawKind = static_cast<uint8>(Slot.BehaviorKind);
			if (RawKind < 1 || RawKind > 17)
			{
				return FailureAt(EAngelscriptCacheValidationError::UnknownEnumValue,
					OutFailureCoordinate,
					EAngelscriptTypeSchemaCapturedField::BehaviorSlot, Index);
			}
			if (const FAngelscriptCacheValidationResult Result =
				ValidateStableReference(Slot.Target); !Result.IsSuccess())
			{
				return FailureAt(Result.Error, OutFailureCoordinate,
					EAngelscriptTypeSchemaCapturedField::BehaviorTarget, Index);
			}
			if (Slot.Target.Kind == EAngelscriptCacheReferenceKind::ScriptFunction
				&& Slot.DeclaringOwner.IsSet()
				&& Slot.DeclaringOwner->Hash.IsZero())
			{
				return FailureAt(EAngelscriptCacheValidationError::ZeroStableKey,
					OutFailureCoordinate,
					EAngelscriptTypeSchemaCapturedField::BehaviorDeclaringOwner, Index);
			}

			if (RawKind < PreviousKind)
			{
				return FailureAt(EAngelscriptCacheValidationError::NonCanonicalOrder,
					OutFailureCoordinate,
					EAngelscriptTypeSchemaCapturedField::BehaviorSlot, Index - 1);
			}
			if (RawKind != PreviousKind)
			{
				if (Slot.SlotOrdinal != 0)
				{
					bool bZeroOrdinalAppearsLater = false;
					for (int32 LaterIndex = Index + 1;
						LaterIndex < Value.OrderedBehaviorSlots.Num(); ++LaterIndex)
					{
						const FAngelscriptCachedBehaviorSlot& Later =
							Value.OrderedBehaviorSlots[LaterIndex];
						if (static_cast<uint8>(Later.BehaviorKind) != RawKind)
						{
							break;
						}
						bZeroOrdinalAppearsLater |= Later.SlotOrdinal == 0;
					}
					return FailureAt(bZeroOrdinalAppearsLater
							? EAngelscriptCacheValidationError::NonCanonicalOrder
							: EAngelscriptCacheValidationError::OrdinalGap,
						OutFailureCoordinate,
						EAngelscriptTypeSchemaCapturedField::BehaviorSlot, Index);
				}
			}
			else
			{
				if (Slot.SlotOrdinal == PreviousOrdinal)
				{
					return FailureAt(EAngelscriptCacheValidationError::DuplicateOrdinal,
						OutFailureCoordinate,
						EAngelscriptTypeSchemaCapturedField::BehaviorSlot, Index);
				}
				if (Slot.SlotOrdinal < PreviousOrdinal)
				{
					return FailureAt(EAngelscriptCacheValidationError::NonCanonicalOrder,
						OutFailureCoordinate,
						EAngelscriptTypeSchemaCapturedField::BehaviorSlot, Index);
				}
				if (Slot.SlotOrdinal != PreviousOrdinal + 1)
				{
					return FailureAt(EAngelscriptCacheValidationError::OrdinalGap,
						OutFailureCoordinate,
						EAngelscriptTypeSchemaCapturedField::BehaviorSlot, Index);
				}
			}
			PreviousKind = RawKind;
			PreviousOrdinal = Slot.SlotOrdinal;

		}

		for (int32 Index = 0; Index < Value.OrderedBehaviorSlots.Num(); ++Index)
		{
			const FAngelscriptCachedBehaviorSlot& Slot = Value.OrderedBehaviorSlots[Index];
			if (Slot.Target.Kind == EAngelscriptCacheReferenceKind::EnvironmentSymbol
				&& Slot.DeclaringOwner.IsSet())
			{
				return FailureAt(EAngelscriptCacheValidationError::InvalidPresence,
					OutFailureCoordinate,
					EAngelscriptTypeSchemaCapturedField::BehaviorDeclaringOwner, Index);
			}
			if (!IsBehaviorTargetKindAllowed(Slot.BehaviorKind, Slot.Target.Kind))
			{
				const bool bKnownBehaviorTarget =
					Slot.Target.Kind == EAngelscriptCacheReferenceKind::ScriptFunction
					|| Slot.Target.Kind == EAngelscriptCacheReferenceKind::EnvironmentSymbol;
				return FailureAt(bKnownBehaviorTarget
						? EAngelscriptCacheValidationError::InvalidPresence
						: EAngelscriptCacheValidationError::WrongReferenceKind,
					OutFailureCoordinate,
					bKnownBehaviorTarget
						? EAngelscriptTypeSchemaCapturedField::BehaviorSlot
						: EAngelscriptTypeSchemaCapturedField::BehaviorTarget,
					Index);
			}
			if (Slot.Target.Kind == EAngelscriptCacheReferenceKind::ScriptFunction)
			{
				if (!Slot.DeclaringOwner.IsSet())
				{
					return FailureAt(EAngelscriptCacheValidationError::InvalidPresence,
						OutFailureCoordinate,
						EAngelscriptTypeSchemaCapturedField::BehaviorSlot, Index);
				}
			}

			if (!IsBehaviorKindAllowedForType(Value.TypeKind, Slot.BehaviorKind))
			{
				return FailureAt(EAngelscriptCacheValidationError::InvalidPresence,
					OutFailureCoordinate,
					EAngelscriptTypeSchemaCapturedField::BehaviorSlot, Index);
			}
		}
		return {};
	}

	static FAngelscriptCacheValidationResult ValidateBehaviorCrossFieldClosure(
		const FAngelscriptCachedTypeSchema& Value,
		FAngelscriptTypeSchemaFieldCoordinate* OutFailureCoordinate)
	{
		if (IsStaticsClassForm(Value) && !Value.OrderedBehaviorSlots.IsEmpty())
		{
			return FailureAt(EAngelscriptCacheValidationError::InvalidPresence,
				OutFailureCoordinate,
				EAngelscriptTypeSchemaCapturedField::BehaviorSlot, 0);
		}

		uint32 ConstructCount = 0;
		uint32 FactoryCount = 0;
		uint32 DestructCount = 0;
		for (int32 Index = 0; Index < Value.OrderedBehaviorSlots.Num(); ++Index)
		{
			const FAngelscriptCachedBehaviorSlot& Slot = Value.OrderedBehaviorSlots[Index];
			if (IsSingletonBehaviorKind(Slot.BehaviorKind) && Slot.SlotOrdinal != 0)
			{
				return FailureAt(EAngelscriptCacheValidationError::InvalidPresence,
					OutFailureCoordinate,
					EAngelscriptTypeSchemaCapturedField::BehaviorSlot, Index);
			}

			ConstructCount +=
				Slot.BehaviorKind == EAngelscriptCachedBehaviorKind::Construct;
			FactoryCount +=
				Slot.BehaviorKind == EAngelscriptCachedBehaviorKind::Factory;
			DestructCount +=
				Slot.BehaviorKind == EAngelscriptCachedBehaviorKind::Destruct;
		}

		if (Value.TypeKind == EAngelscriptCachedTypeKind::Class
			&& ConstructCount != FactoryCount)
		{
			const EAngelscriptCachedBehaviorKind UnmatchedKind =
				ConstructCount > FactoryCount
				? EAngelscriptCachedBehaviorKind::Construct
				: EAngelscriptCachedBehaviorKind::Factory;
			const uint32 UnmatchedOrdinal = FMath::Min(ConstructCount, FactoryCount);
			for (int32 Index = 0; Index < Value.OrderedBehaviorSlots.Num(); ++Index)
			{
				const FAngelscriptCachedBehaviorSlot& Slot =
					Value.OrderedBehaviorSlots[Index];
				if (Slot.BehaviorKind == UnmatchedKind
					&& Slot.SlotOrdinal == UnmatchedOrdinal)
				{
					return FailureAt(EAngelscriptCacheValidationError::InvalidPresence,
						OutFailureCoordinate,
						EAngelscriptTypeSchemaCapturedField::BehaviorSlot, Index);
				}
			}
			return FailureAt(EAngelscriptCacheValidationError::InvalidPresence,
				OutFailureCoordinate,
				EAngelscriptTypeSchemaCapturedField::TypeSemanticFlags);
		}

		for (int32 Index = 0; Index < Value.OrderedBehaviorSlots.Num(); ++Index)
		{
			const FAngelscriptCachedBehaviorSlot& Slot = Value.OrderedBehaviorSlots[Index];
			if (Slot.Target.Kind != EAngelscriptCacheReferenceKind::ScriptFunction)
			{
				continue;
			}
			if (Slot.BehaviorKind == EAngelscriptCachedBehaviorKind::CopyConstruct
				&& !HasExactScriptBehaviorPeer(
					Value, Slot, EAngelscriptCachedBehaviorKind::Construct))
			{
				return FailureAt(
					EAngelscriptCacheValidationError::InvalidQualifierCombination,
					OutFailureCoordinate,
					EAngelscriptTypeSchemaCapturedField::BehaviorSlot, Index);
			}
			if (Slot.BehaviorKind == EAngelscriptCachedBehaviorKind::CopyFactory
				&& !HasExactScriptBehaviorPeer(
					Value, Slot, EAngelscriptCachedBehaviorKind::Factory))
			{
				return FailureAt(
					EAngelscriptCacheValidationError::InvalidQualifierCombination,
					OutFailureCoordinate,
					EAngelscriptTypeSchemaCapturedField::BehaviorSlot, Index);
			}
		}

		const bool bHasDefaultConstructor = HasTypeSemanticFlag(
			Value, EAngelscriptCachedTypeSemanticFlags::HasDefaultConstructor);
		if (bHasDefaultConstructor
			&& (ConstructCount == 0
				|| (Value.TypeKind == EAngelscriptCachedTypeKind::Class
					&& FactoryCount == 0)))
		{
			return FailureAt(
				EAngelscriptCacheValidationError::InvalidQualifierCombination,
				OutFailureCoordinate,
				EAngelscriptTypeSchemaCapturedField::TypeSemanticFlags);
		}
		const bool bHasDestructor = HasTypeSemanticFlag(
			Value, EAngelscriptCachedTypeSemanticFlags::HasDestructor);
		if (bHasDestructor && DestructCount != 1)
		{
			return FailureAt(
				EAngelscriptCacheValidationError::InvalidQualifierCombination,
				OutFailureCoordinate,
				EAngelscriptTypeSchemaCapturedField::TypeSemanticFlags);
		}
		if (!bHasDestructor && DestructCount == 1)
		{
			for (int32 Index = 0; Index < Value.OrderedBehaviorSlots.Num(); ++Index)
			{
				if (Value.OrderedBehaviorSlots[Index].BehaviorKind
					== EAngelscriptCachedBehaviorKind::Destruct)
				{
					return FailureAt(
						EAngelscriptCacheValidationError::InvalidQualifierCombination,
						OutFailureCoordinate,
						EAngelscriptTypeSchemaCapturedField::BehaviorSlot, Index);
				}
			}
		}
		return {};
	}

	static FAngelscriptCacheValidationResult ValidateDescriptorFormClosure(
		const FAngelscriptCachedTypeSchema& Value,
		FAngelscriptTypeSchemaFieldCoordinate* OutFailureCoordinate)
	{
		if (Value.TypeKind != EAngelscriptCachedTypeKind::Typedef
			&& Value.TypeKind != EAngelscriptCachedTypeKind::Funcdef)
		{
			return {};
		}
		if (!Value.OrderedProperties.IsEmpty())
		{
			return FailureAt(EAngelscriptCacheValidationError::InvalidPresence,
				OutFailureCoordinate,
				EAngelscriptTypeSchemaCapturedField::OrderedProperty, 0);
		}

		uint64 ExpectedSize = 0;
		const uint32 ExpectedAlignment =
			FAngelscriptCacheTypeSchemaArchive::GetV1BuildLayoutConstants()
				.TypeInfoInitialAlignment;
		if (Value.TypeKind == EAngelscriptCachedTypeKind::Typedef)
		{
			FAngelscriptCacheV1StorageLayout PrimitiveLayout;
			if (!Value.KindPayload.Typedef.IsSet()
				|| !FAngelscriptCacheTypeSchemaArchive::GetV1BuildLayoutConstants()
					.TryGetPrimitiveStorageLayout(
						Value.KindPayload.Typedef->AliasedType.Primitive,
						PrimitiveLayout))
			{
				return FailureAt(
					EAngelscriptCacheValidationError::InvalidQualifierCombination,
					OutFailureCoordinate,
					EAngelscriptTypeSchemaCapturedField::LayoutExpectation);
			}
			ExpectedSize = PrimitiveLayout.SemanticStorageSize;
		}

		if (Value.Layout.SemanticSize != ExpectedSize
			|| Value.Layout.SemanticAlignment != ExpectedAlignment
			|| Value.Layout.BasePropertyBoundary != 0)
		{
			return FailureAt(
				EAngelscriptCacheValidationError::InvalidQualifierCombination,
				OutFailureCoordinate,
				EAngelscriptTypeSchemaCapturedField::LayoutExpectation);
		}
		return {};
	}

	static FAngelscriptCacheValidationResult ValidateProducerShape(
		const FAngelscriptCachedTypeSchema& Value,
		FAngelscriptTypeSchemaFieldCoordinate* OutFailureCoordinate = nullptr)
	{
		if (OutFailureCoordinate != nullptr)
		{
			*OutFailureCoordinate = {};
		}
		if (Value.PayloadSchemaVersion
			!= FAngelscriptCacheTypeSchemaArchive::TypeSchemaPayloadSchemaVersion)
		{
			return Failure(EAngelscriptCacheValidationError::UnsupportedPayloadSchema);
		}
		if (Value.ModuleKey.Hash.IsZero() || Value.TypeKey.Hash.IsZero())
		{
			return Failure(EAngelscriptCacheValidationError::ZeroStableKey);
		}
		if (static_cast<uint8>(Value.TypeKind) < 1
			|| static_cast<uint8>(Value.TypeKind) > 7)
		{
			return Failure(EAngelscriptCacheValidationError::UnknownEnumValue);
		}
		if (const FAngelscriptCacheValidationResult Result =
			ValidateString(Value.CanonicalNamespace, false); !Result.IsSuccess())
		{
			return Result;
		}
		if (const FAngelscriptCacheValidationResult Result =
			ValidateString(Value.CanonicalName, true); !Result.IsSuccess())
		{
			return Result;
		}
		if (const FAngelscriptCacheValidationResult Result =
			ValidateString(Value.CanonicalDeclaration, true); !Result.IsSuccess())
		{
			return Result;
		}
		if (const FAngelscriptCacheValidationResult Result =
			ValidateTypeSemanticFlagsFieldLocal(Value, OutFailureCoordinate);
			!Result.IsSuccess())
		{
			return Result;
		}
		int32 MetadataFailureIndex = INDEX_NONE;
		if (const FAngelscriptCacheValidationResult Result =
			ValidateMetadataArray(Value.Metadata, &MetadataFailureIndex); !Result.IsSuccess())
		{
			return FailureAt(Result.Error, OutFailureCoordinate,
				EAngelscriptTypeSchemaCapturedField::MetadataEntry,
				MetadataFailureIndex);
		}
		if (const FAngelscriptCacheValidationResult Result =
			ValidateRelationsFieldLocal(Value, OutFailureCoordinate);
			!Result.IsSuccess())
		{
			return Result;
		}
		if (const FAngelscriptCacheValidationResult Result =
			ValidateLayoutInputsFieldLocal(Value, OutFailureCoordinate);
			!Result.IsSuccess())
		{
			return Result;
		}
		if (const FAngelscriptCacheValidationResult Result =
			ValidatePropertyEntriesFieldLocal(Value, OutFailureCoordinate);
			!Result.IsSuccess())
		{
			return Result;
		}
		if (const FAngelscriptCacheValidationResult Result =
			ValidateMethodEntriesFieldLocal(Value, OutFailureCoordinate);
			!Result.IsSuccess())
		{
			return Result;
		}
		if (const FAngelscriptCacheValidationResult Result =
			ValidateVirtualFunctionTableFieldLocal(Value, OutFailureCoordinate);
			!Result.IsSuccess())
		{
			return Result;
		}

		const bool bEnum = Value.TypeKind == EAngelscriptCachedTypeKind::Enum;
		const bool bCallable = Value.TypeKind == EAngelscriptCachedTypeKind::Delegate
			|| Value.TypeKind == EAngelscriptCachedTypeKind::Funcdef;
		const bool bTypedef = Value.TypeKind == EAngelscriptCachedTypeKind::Typedef;
		if (Value.KindPayload.Enum.IsSet() != bEnum
			|| Value.KindPayload.Callable.IsSet() != bCallable
			|| Value.KindPayload.Typedef.IsSet() != bTypedef)
		{
			return Failure(EAngelscriptCacheValidationError::InvalidPresence);
		}
		if (bEnum)
		{
			const TArray<FAngelscriptCachedEnumEnumerator>& Enumerators =
				Value.KindPayload.Enum->OrderedEnumerators;
			for (int32 Index = 0; Index < Enumerators.Num(); ++Index)
			{
				const FAngelscriptCachedEnumEnumerator& Enumerator = Enumerators[Index];
				const uint32 ExpectedOrdinal = static_cast<uint32>(Index);
				if (Enumerator.DeclarationOrdinal != ExpectedOrdinal)
				{
					if (Index > 0
						&& Enumerator.DeclarationOrdinal
							== Enumerators[Index - 1].DeclarationOrdinal)
					{
						return FailureAt(
							EAngelscriptCacheValidationError::DuplicateOrdinal,
							OutFailureCoordinate,
							EAngelscriptTypeSchemaCapturedField::KindPayload);
					}
					bool bExpectedOrdinalAppearsLater = false;
					for (int32 LaterIndex = Index + 1;
						LaterIndex < Enumerators.Num(); ++LaterIndex)
					{
						bExpectedOrdinalAppearsLater |=
							Enumerators[LaterIndex].DeclarationOrdinal == ExpectedOrdinal;
					}
					return FailureAt(
						bExpectedOrdinalAppearsLater
							? EAngelscriptCacheValidationError::NonCanonicalOrder
							: EAngelscriptCacheValidationError::OrdinalGap,
						OutFailureCoordinate,
						EAngelscriptTypeSchemaCapturedField::KindPayload);
				}
				if (const FAngelscriptCacheValidationResult Result =
					ValidateString(Enumerator.CanonicalName, true); !Result.IsSuccess())
				{
					return FailureAt(Result.Error, OutFailureCoordinate,
						EAngelscriptTypeSchemaCapturedField::KindPayload);
				}
				if (const FAngelscriptCacheValidationResult Result =
					ValidateMetadataArray(Enumerator.Metadata); !Result.IsSuccess())
				{
					return FailureAt(Result.Error, OutFailureCoordinate,
						EAngelscriptTypeSchemaCapturedField::KindPayload);
				}
				for (int32 PreviousIndex = 0; PreviousIndex < Index; ++PreviousIndex)
				{
					if (CompareString(
						Enumerators[PreviousIndex].CanonicalName,
						Enumerator.CanonicalName) == 0)
					{
						return FailureAt(EAngelscriptCacheValidationError::DuplicateKey,
							OutFailureCoordinate,
							EAngelscriptTypeSchemaCapturedField::KindPayload);
					}
				}
			}
		}
		if (bCallable)
		{
			const FAngelscriptCachedCallableTypePayload& Callable =
				Value.KindPayload.Callable.GetValue();
			if (Callable.SignatureFunctionKey.Hash.IsZero())
			{
				return Failure(EAngelscriptCacheValidationError::ZeroStableKey);
			}
			if (Callable.ExpectedSignatureAbi.IsZero())
			{
				return Failure(EAngelscriptCacheValidationError::MissingExpectedAbi);
			}
			if (Value.TypeKind == EAngelscriptCachedTypeKind::Funcdef
				&& Callable.bMulticast)
			{
				return Failure(
					EAngelscriptCacheValidationError::InvalidQualifierCombination);
			}
		}
		if (bTypedef)
		{
			const FAngelscriptCachedDataType& Aliased =
				Value.KindPayload.Typedef->AliasedType;
			if (Aliased.Kind != EAngelscriptCachedDataTypeKind::Primitive
				|| Aliased.Primitive == EAngelscriptCachedPrimitiveType::Void
				|| Aliased.QualifierFlags != 0
				|| !Aliased.OrderedSubTypes.IsEmpty())
			{
				return Failure(
					EAngelscriptCacheValidationError::InvalidQualifierCombination);
			}
			if (const FAngelscriptCacheValidationResult Result = ValidateDataType(Aliased);
				!Result.IsSuccess())
			{
				return Result;
			}
		}
		if (const FAngelscriptCacheValidationResult Result = ValidateBehaviorFieldLocal(
			Value, OutFailureCoordinate);
			!Result.IsSuccess())
		{
			return Result;
		}
		if (const FAngelscriptCacheValidationResult Result =
			ValidateReflectionFieldLocal(Value, OutFailureCoordinate);
			!Result.IsSuccess())
		{
			return Result;
		}
		for (int32 Index = 0; Index < Value.Dependencies.Num(); ++Index)
		{
			if (const FAngelscriptCacheValidationResult Result =
				ValidateDependency(Value.Dependencies[Index]); !Result.IsSuccess())
			{
				return FailureAt(Result.Error, OutFailureCoordinate,
					EAngelscriptTypeSchemaCapturedField::Dependency, Index);
			}
			if (Index > 0)
			{
				const FAngelscriptCacheSemanticDependency& Previous =
					Value.Dependencies[Index - 1];
				const FAngelscriptCacheSemanticDependency& Current =
					Value.Dependencies[Index];
				if (Previous.Kind == Current.Kind
					&& Previous.Target.Kind == Current.Target.Kind
					&& Previous.Target.StableKey == Current.Target.StableKey)
				{
					return FailureAt(Previous == Current
							? EAngelscriptCacheValidationError::DuplicateKey
							: EAngelscriptCacheValidationError::ConflictingKey,
						OutFailureCoordinate,
						EAngelscriptTypeSchemaCapturedField::Dependency, Index);
				}
				const int32 Compare = FAngelscriptCacheTypeSchemaArchive::CompareDependencies(
					Previous, Current);
				if (Compare > 0)
				{
					return FailureAt(
						EAngelscriptCacheValidationError::NonCanonicalOrder,
						OutFailureCoordinate,
						EAngelscriptTypeSchemaCapturedField::Dependency, Index);
				}
			}
		}
		if (const FAngelscriptCacheValidationResult Result =
			ValidateReflectionFormClosure(Value, OutFailureCoordinate);
			!Result.IsSuccess())
		{
			return Result;
		}
		if (const FAngelscriptCacheValidationResult Result =
			ValidateDescriptorFormClosure(Value, OutFailureCoordinate);
			!Result.IsSuccess())
		{
			return Result;
		}
		if (const FAngelscriptCacheValidationResult Result =
			ValidateRelationsCrossFieldClosure(Value, OutFailureCoordinate);
			!Result.IsSuccess())
		{
			return Result;
		}
		if (const FAngelscriptCacheValidationResult Result =
			ValidateLayoutInputsCrossFieldClosure(Value, OutFailureCoordinate);
			!Result.IsSuccess())
		{
			return Result;
		}
		if (const FAngelscriptCacheValidationResult Result =
			ValidateBehaviorCrossFieldClosure(Value, OutFailureCoordinate);
			!Result.IsSuccess())
		{
			return Result;
		}
		if (const FAngelscriptCacheValidationResult Result =
			ValidateRelationLayoutInputPairing(Value, OutFailureCoordinate);
			!Result.IsSuccess())
		{
			return Result;
		}
		if (const FAngelscriptCacheValidationResult Result =
			ValidateDependencyCrossFieldClosure(Value, OutFailureCoordinate);
			!Result.IsSuccess())
		{
			return Result;
		}
		if (const FAngelscriptCacheValidationResult Result =
			ValidatePropertyOwnerAndFlagsClosure(Value, OutFailureCoordinate);
			!Result.IsSuccess())
		{
			return Result;
		}
		if (const FAngelscriptCacheValidationResult Result =
			ValidatePropertyLayoutReplay(Value, OutFailureCoordinate);
			!Result.IsSuccess())
		{
			return Result;
		}
		if (Value.KindPayload.Enum.IsSet())
		{
			FAngelscriptHash256 ExpectedEnum;
			if (const FAngelscriptCacheValidationResult Result =
				FAngelscriptCacheTypeSchemaArchive::ComputeEnumAuthorityHash(
					Value.TypeKey, Value.KindPayload.Enum.GetValue(), ExpectedEnum);
				!Result.IsSuccess())
			{
				return Result;
			}
			if (!(ExpectedEnum == Value.KindPayload.Enum->EnumAuthorityHash))
			{
				return FailureAt(
					EAngelscriptCacheValidationError::DerivedHashMismatch,
					OutFailureCoordinate,
					EAngelscriptTypeSchemaCapturedField::KindPayload);
			}
		}
		FAngelscriptHash256 ExpectedLayout;
		if (const FAngelscriptCacheValidationResult Result =
			FAngelscriptCacheTypeSchemaArchive::ComputeTypeLayoutHash(
				Value, ExpectedLayout); !Result.IsSuccess())
		{
			return Result;
		}
		if (!(ExpectedLayout == Value.Layout.TypeLayoutHash))
		{
			return Failure(EAngelscriptCacheValidationError::DerivedHashMismatch);
		}
		return {};
	}

#if WITH_ANGELSCRIPT_UNITTESTS
	using FTrace = FAngelscriptCacheTypeSchemaTestWireTrace;
	using ETraceField = EAngelscriptCacheTypeSchemaTestField;

	static void AddTrace(
		FTrace* Trace,
		const ETraceField Field,
		const uint64 Offset,
		const uint64 Size,
		const int32 Primary = INDEX_NONE,
		const int32 Secondary = INDEX_NONE,
		const int32 Tertiary = INDEX_NONE)
	{
		if (Trace != nullptr)
		{
			Trace->Spans.Add({Field, Primary, Secondary, Tertiary, Offset, Size});
		}
	}

	class FPhysicalTraceScanner final
	{
	public:
		FPhysicalTraceScanner(
			const TConstArrayView<uint8> InPayload,
			FTrace& InTrace)
			: Payload(InPayload)
			, Trace(InTrace)
		{
			Trace.Spans.Reset();
		}

		bool Scan()
		{
			U32(ETraceField::PayloadSchemaVersion);
			Hash(ETraceField::ModuleKey);
			Hash(ETraceField::TypeKey);
			const uint8 TypeKind = U8(ETraceField::TypeKind);
			String(ETraceField::CanonicalNamespace);
			String(ETraceField::CanonicalNameBytes);
			String(ETraceField::CanonicalDeclaration);
			U32(ETraceField::TypeSemanticFlags);
			Metadata();
			Relations();
			LayoutInputs();
			Mark(ETraceField::Layout);
			U64(ETraceField::LayoutSemanticSize);
			U32(ETraceField::LayoutSemanticAlignment);
			U32(ETraceField::LayoutBasePropertyBoundary);
			Hash(ETraceField::TypeLayoutHash);
			Properties();
			Methods();
			VirtualFunctions();
			Behaviors();
			KindPayload(TypeKind);
			Reflection();
			Dependencies();
			return bValid && Offset == static_cast<uint64>(Payload.Num());
		}

	private:
		bool Take(const uint64 Size, uint64& OutStart)
		{
			OutStart = Offset;
			if (Size > static_cast<uint64>(Payload.Num())
				|| Offset > static_cast<uint64>(Payload.Num()) - Size)
			{
				bValid = false;
				return false;
			}
			Offset += Size;
			return true;
		}

		void Span(
			const ETraceField Field,
			const uint64 Start,
			const uint64 Size,
			const int32 Primary = INDEX_NONE,
			const int32 Secondary = INDEX_NONE,
			const int32 Tertiary = INDEX_NONE)
		{
			Trace.Spans.Add({Field, Primary, Secondary, Tertiary, Start, Size});
		}

		void Mark(
			const ETraceField Field,
			const int32 Primary = INDEX_NONE,
			const int32 Secondary = INDEX_NONE,
			const int32 Tertiary = INDEX_NONE)
		{
			Span(Field, Offset, 0, Primary, Secondary, Tertiary);
		}

		uint8 RawU8()
		{
			uint64 Start = 0;
			return Take(1, Start) ? Payload[static_cast<int32>(Start)] : 0;
		}

		uint32 RawU32()
		{
			uint64 Start = 0;
			uint32 Value = 0;
			if (Take(4, Start))
			{
				Value = static_cast<uint32>(Payload[Start])
					| (static_cast<uint32>(Payload[Start + 1]) << 8)
					| (static_cast<uint32>(Payload[Start + 2]) << 16)
					| (static_cast<uint32>(Payload[Start + 3]) << 24);
			}
			return Value;
		}

		uint8 U8(
			const ETraceField Field,
			const int32 Primary = INDEX_NONE,
			const int32 Secondary = INDEX_NONE,
			const int32 Tertiary = INDEX_NONE)
		{
			const uint64 Start = Offset;
			const uint8 Value = RawU8();
			Span(Field, Start, 1, Primary, Secondary, Tertiary);
			return Value;
		}

		uint32 U32(
			const ETraceField Field,
			const int32 Primary = INDEX_NONE,
			const int32 Secondary = INDEX_NONE,
			const int32 Tertiary = INDEX_NONE)
		{
			const uint64 Start = Offset;
			const uint32 Value = RawU32();
			Span(Field, Start, 4, Primary, Secondary, Tertiary);
			return Value;
		}

		void U64(const ETraceField Field, const int32 Primary = INDEX_NONE)
		{
			uint64 Start = 0;
			Take(8, Start);
			Span(Field, Start, 8, Primary);
		}

		void Hash(
			const ETraceField Field,
			const int32 Primary = INDEX_NONE,
			const int32 Secondary = INDEX_NONE,
			const int32 Tertiary = INDEX_NONE)
		{
			uint64 Start = 0;
			Take(sizeof(FBlake3Hash::ByteArray), Start);
			Span(Field, Start, sizeof(FBlake3Hash::ByteArray),
				Primary, Secondary, Tertiary);
		}

		void String(
			const ETraceField Field,
			const int32 Primary = INDEX_NONE,
			const int32 Secondary = INDEX_NONE,
			const int32 Tertiary = INDEX_NONE)
		{
			const uint32 ByteCount = RawU32();
			uint64 Start = 0;
			Take(ByteCount, Start);
			Span(Field, Start, ByteCount, Primary, Secondary, Tertiary);
		}

		void StableReference(
			const ETraceField Field,
			const int32 Primary = INDEX_NONE,
			const int32 Secondary = INDEX_NONE,
			const int32 Tertiary = INDEX_NONE)
		{
			uint64 Start = 0;
			constexpr uint64 Size = 1 + 2 * sizeof(FBlake3Hash::ByteArray);
			Take(Size, Start);
			Span(Field, Start, Size, Primary, Secondary, Tertiary);
		}

		void Metadata()
		{
			const uint32 Count = U32(ETraceField::Metadata);
			for (uint32 Index = 0; Index < Count && bValid; ++Index)
			{
				Mark(ETraceField::MetadataEntry, static_cast<int32>(Index));
				String(ETraceField::MetadataEntry,
					static_cast<int32>(Index), INDEX_NONE, 0);
				String(ETraceField::MetadataEntry,
					static_cast<int32>(Index), INDEX_NONE, 1);
			}
		}

		void NestedMetadata(
			const int32 OwnerIndex,
			const ETraceField ContainerField,
			const ETraceField EntryField)
		{
			const uint32 Count = U32(ContainerField, OwnerIndex);
			for (uint32 Index = 0; Index < Count && bValid; ++Index)
			{
				Mark(EntryField, OwnerIndex, static_cast<int32>(Index));
				String(EntryField, OwnerIndex, static_cast<int32>(Index), 0);
				String(EntryField, OwnerIndex, static_cast<int32>(Index), 1);
			}
		}

		void Relations()
		{
			const uint32 Count = U32(ETraceField::Relations);
			for (uint32 Index = 0; Index < Count && bValid; ++Index)
			{
				const int32 I = static_cast<int32>(Index);
				Mark(ETraceField::Relations, I);
				U8(ETraceField::RelationKind, I);
				if (U8(ETraceField::RelationSemanticOrdinalOptionalTag, I) == 1)
				{
					uint64 Ignored = 0; Take(4, Ignored);
				}
				StableReference(ETraceField::Relations, I, 0);
			}
		}

		void LayoutInputs()
		{
			const uint32 Count = U32(ETraceField::LayoutInputs);
			for (uint32 Index = 0; Index < Count && bValid; ++Index)
			{
				const int32 I = static_cast<int32>(Index);
				Mark(ETraceField::LayoutInput, I);
				U8(ETraceField::LayoutInputKind, I);
				StableReference(ETraceField::LayoutInput, I, 0);
				if (U8(ETraceField::LayoutInputBoundaryOptionalTag, I) == 1)
				{
					uint64 Ignored = 0; Take(4, Ignored);
				}
				if (U8(ETraceField::LayoutInputAlignmentOptionalTag, I) == 1)
				{
					uint64 Ignored = 0; Take(4, Ignored);
				}
				Hash(ETraceField::LayoutInput, I, 1);
			}
		}

		void DataType(const int32 PropertyIndex, int32& PreOrder)
		{
			const int32 Node = PreOrder++;
			Mark(ETraceField::PropertyType, PropertyIndex, Node);
			U8(ETraceField::DataTypeKind, PropertyIndex, Node);
			U8(ETraceField::DataTypePrimitive, PropertyIndex, Node);
			if (U8(ETraceField::DataTypeTypeReferenceOptionalTag,
				PropertyIndex, Node) == 1)
			{
				StableReference(ETraceField::PropertyType, PropertyIndex, Node, 0);
			}
			U32(ETraceField::DataTypeQualifierFlags, PropertyIndex, Node);
			const uint32 Count = U32(
				ETraceField::DataTypeOrderedSubTypes, PropertyIndex, Node);
			for (uint32 Index = 0; Index < Count && bValid; ++Index)
			{
				DataType(PropertyIndex, PreOrder);
			}
		}

		void Properties()
		{
			const uint32 Count = U32(ETraceField::OrderedProperties);
			for (uint32 Index = 0; Index < Count && bValid; ++Index)
			{
				const int32 I = static_cast<int32>(Index);
				Mark(ETraceField::OrderedProperty, I);
				U32(ETraceField::PropertyLayoutOrdinal, I);
				U32(ETraceField::PropertySemanticByteOffset, I);
				Hash(ETraceField::PropertyKey, I);
				String(ETraceField::PropertyCanonicalName, I);
				int32 PreOrder = 0; DataType(I, PreOrder);
				U8(ETraceField::PropertyStorageKind, I);
				U32(ETraceField::PropertySemanticStorageSize, I);
				U32(ETraceField::PropertySemanticStorageAlignment, I);
				Hash(ETraceField::PropertyStorageLayoutHash, I);
				U8(ETraceField::PropertyMemberAccess, I);
				U32(ETraceField::PropertySemanticFlags, I);
				U8(ETraceField::PropertyReplicationCondition, I);
				NestedMetadata(I, ETraceField::PropertyMetadata,
					ETraceField::PropertyMetadataEntry);
				Hash(ETraceField::PropertyLayoutFingerprint, I);
			}
		}

		void Methods()
		{
			const uint32 Count = U32(ETraceField::OrderedMethods);
			for (uint32 Index = 0; Index < Count && bValid; ++Index)
			{
				const int32 I = static_cast<int32>(Index);
				Mark(ETraceField::OrderedMethod, I);
				U8(ETraceField::MethodSlotKind, I);
				U32(ETraceField::MethodOrdinal, I);
				Hash(ETraceField::MethodFunctionKey, I);
				Hash(ETraceField::MethodDeclaringOwner, I);
				Hash(ETraceField::MethodExpectedDeclarationAbi, I);
			}
		}

		void VirtualFunctions()
		{
			const uint32 Count = U32(ETraceField::VirtualFunctionTable);
			for (uint32 Index = 0; Index < Count && bValid; ++Index)
			{
				const int32 I = static_cast<int32>(Index);
				Mark(ETraceField::VirtualFunctionTable, I);
				U8(ETraceField::MethodSlotKind, I, 1);
				U32(ETraceField::MethodOrdinal, I, 1);
				Hash(ETraceField::MethodFunctionKey, I, 1);
				Hash(ETraceField::MethodDeclaringOwner, I, 1);
				Hash(ETraceField::MethodDeclaringOwner, I, 2);
				Hash(ETraceField::MethodExpectedDeclarationAbi, I, 1);
			}
		}

		void Behaviors()
		{
			const uint32 Count = U32(ETraceField::BehaviorSlots);
			for (uint32 Index = 0; Index < Count && bValid; ++Index)
			{
				const int32 I = static_cast<int32>(Index);
				Mark(ETraceField::BehaviorSlot, I);
				U8(ETraceField::BehaviorKind, I);
				U32(ETraceField::BehaviorOrdinal, I);
				StableReference(ETraceField::BehaviorTarget, I);
				if (U8(ETraceField::BehaviorDeclaringOwnerOptionalTag, I) == 1)
				{
					Hash(ETraceField::BehaviorDeclaringOwnerOptionalTag, I, 1);
				}
			}
		}

		void KindPayload(const uint8 TypeKind)
		{
			Mark(ETraceField::KindPayload);
			switch (TypeKind)
			{
			case 1: case 2: case 3: break;
			case 4:
				{
					const uint32 Count = U32(ETraceField::KindPayload, 0);
					for (uint32 Index = 0; Index < Count && bValid; ++Index)
					{
						const int32 I = static_cast<int32>(Index);
						Mark(ETraceField::EnumEnumerator, I);
						U32(ETraceField::EnumDeclarationOrdinal, I);
						String(ETraceField::EnumCanonicalName, I);
						U32(ETraceField::EnumSignedValue, I);
						NestedMetadata(I, ETraceField::EnumEnumeratorMetadata,
							ETraceField::EnumEnumeratorMetadataEntry);
					}
					Hash(ETraceField::EnumAuthorityHash);
				}
				break;
			case 5: case 7:
				Hash(ETraceField::CallableSignatureFunctionKey);
				Hash(ETraceField::CallableExpectedSignatureAbi);
				U8(ETraceField::CallableMulticastBoolean);
				break;
			case 6:
				{
					int32 PreOrder = 0; DataType(INDEX_NONE, PreOrder);
				}
				break;
			default:
				// Physical test serialization must be able to snapshot a DTO
				// whose discriminator is intentionally malformed. The normal
				// producer rejects it before any canonical bytes are published;
				// the trace scanner treats the unknown arm as physically empty.
				break;
			}
		}

		void OptionalString(const ETraceField Field)
		{
			if (U8(Field) == 1)
			{
				String(Field, INDEX_NONE, INDEX_NONE, 1);
			}
		}

		void Reflection()
		{
			Mark(ETraceField::Reflection);
			U8(ETraceField::ReflectionKind);
			U32(ETraceField::ClassReflectionFlags);
			OptionalString(ETraceField::ReflectionConfigNameOptionalTag);
			OptionalString(ETraceField::ReflectionStaticClassGlobalNameOptionalTag);
			const uint32 Count = U32(ETraceField::ReflectedFunctionMembers);
			for (uint32 Index = 0; Index < Count && bValid; ++Index)
			{
				const int32 I = static_cast<int32>(Index);
				Mark(ETraceField::ReflectedFunctionMembers, I);
				uint64 Ignored = 0; Take(4, Ignored);
				String(ETraceField::ReflectedFunctionNameBytes, I);
				String(ETraceField::ReflectedOriginalFunctionNameBytes, I);
				String(ETraceField::ReflectedScriptFunctionNameBytes, I);
				StableReference(ETraceField::ReflectedFunctionMembers, I, 0);
			}
		}

		void Dependencies()
		{
			const uint32 Count = U32(ETraceField::Dependencies);
			for (uint32 Index = 0; Index < Count && bValid; ++Index)
			{
				const int32 I = static_cast<int32>(Index);
				Mark(ETraceField::Dependency, I);
				U8(ETraceField::Dependency, I, 1);
				StableReference(ETraceField::Dependency, I, 2);
				if (U8(ETraceField::DependencyExpectedContentOrValueOptionalTag, I) == 1)
				{
					uint64 Ignored = 0;
					Take(sizeof(FBlake3Hash::ByteArray), Ignored);
				}
			}
		}

		TConstArrayView<uint8> Payload;
		FTrace& Trace;
		uint64 Offset = 0;
		bool bValid = true;
	};
#endif

}

namespace AngelscriptCacheTypeSchema_Private
{
	using AngelscriptCacheCanonicalCodec_Private::BeginRead;
	using AngelscriptCacheCanonicalCodec_Private::FReader;
	using AngelscriptCacheCanonicalCodec_Private::ReadEnum;

	static constexpr uint32 UnusedCapturedIndex = MAX_uint32;

	template <typename OffsetStorageType>
	class TTypeSchemaDecoder final
	{
		using FOffsetEntry = typename OffsetStorageType::FEntry;

	public:
		TTypeSchemaDecoder(
			const TConstArrayView<uint8> InPayload,
			const FAngelscriptCacheReadLimits& InLimits,
			FAngelscriptCacheReadBudget& InBudget,
			const FDecodedChargeSink& InChargeSink,
			FAngelscriptCachedTypeSchema& InValue,
			OffsetStorageType& InOffsets)
			: Payload(InPayload)
			, Reader(InPayload, InLimits, InBudget,
				EAngelscriptCacheRecordKind::TypeSchema, InChargeSink)
			, Value(InValue)
			, Offsets(InOffsets)
		{
			for (uint64& Offset : CheckpointOffsets)
			{
				Offset = 0;
			}
		}

		FAngelscriptCacheValidationResult DecodePhysical()
		{
			Value = {};
			Offsets = {};

			// PayloadSchemaVersion, ModuleKey, TypeKey, TypeKind, three canonical
			// strings, TypeSemanticFlags, Metadata and LayoutExpectation are all
			// captured here. Reserve the full ten-entry authority before any Add so
			// the last coordinate cannot trigger an unbudgeted TArray growth.
			if (!ReserveOffsets(0, 10, Offsets.FlatHeaderOffsets))
			{
				return Reader.GetResult();
			}

			Capture(Offsets.FlatHeaderOffsets,
				EAngelscriptTypeSchemaCapturedField::PayloadSchemaVersion, Reader.GetOffset());
			if (!Reader.ReadUInt32(Value.PayloadSchemaVersion))
			{
				return Reader.GetResult();
			}
			if (Value.PayloadSchemaVersion
				!= FAngelscriptCacheTypeSchemaArchive::TypeSchemaPayloadSchemaVersion)
			{
				Reader.Fail(EAngelscriptCacheValidationError::UnsupportedPayloadSchema, 0);
				return Reader.GetResult();
			}

			Capture(Offsets.FlatHeaderOffsets,
				EAngelscriptTypeSchemaCapturedField::ModuleKey, Reader.GetOffset());
			if (!Reader.ReadHash(Value.ModuleKey.Hash))
			{
				return Reader.GetResult();
			}
			Capture(Offsets.FlatHeaderOffsets,
				EAngelscriptTypeSchemaCapturedField::TypeKey, Reader.GetOffset());
			if (!Reader.ReadHash(Value.TypeKey.Hash))
			{
				return Reader.GetResult();
			}
			Capture(Offsets.FlatHeaderOffsets,
				EAngelscriptTypeSchemaCapturedField::TypeKind, Reader.GetOffset());
			if (!ReadEnum(Reader, Value.TypeKind, 1, 7))
			{
				return Reader.GetResult();
			}

			if (!ReadCapturedString(EAngelscriptTypeSchemaCapturedField::CanonicalNamespace,
				Value.CanonicalNamespace)
				|| !ReadCapturedString(EAngelscriptTypeSchemaCapturedField::CanonicalName,
					Value.CanonicalName)
				|| !ReadCapturedString(EAngelscriptTypeSchemaCapturedField::CanonicalDeclaration,
					Value.CanonicalDeclaration))
			{
				return Reader.GetResult();
			}
			Capture(Offsets.FlatHeaderOffsets,
				EAngelscriptTypeSchemaCapturedField::TypeSemanticFlags, Reader.GetOffset());
			if (!Reader.ReadUInt32(Value.TypeSemanticFlags))
			{
				return Reader.GetResult();
			}

			if (!ReadTopLevelMetadata()
				|| !ReadRelations()
				|| !ReadLayoutInputs())
			{
				return Reader.GetResult();
			}

			Capture(Offsets.FlatHeaderOffsets,
				EAngelscriptTypeSchemaCapturedField::LayoutExpectation, Reader.GetOffset());
			if (!Reader.ReadUInt64(Value.Layout.SemanticSize)
				|| !Reader.ReadUInt32(Value.Layout.SemanticAlignment)
				|| !Reader.ReadUInt32(Value.Layout.BasePropertyBoundary)
				|| !Reader.ReadHash(Value.Layout.TypeLayoutHash))
			{
				return Reader.GetResult();
			}

			if (!ReadProperties()
				|| !ReadMethods()
				|| !ReadVirtualFunctionTable()
				|| !ReadBehaviors()
				|| !ReadKindPayload()
				|| !ReadReflection()
				|| !ReadDependencies())
			{
				return Reader.GetResult();
			}

			if (!Reader.IsAtEnd())
			{
				Reader.Fail(EAngelscriptCacheValidationError::TrailingData, Reader.GetOffset());
				return Reader.GetResult();
			}
			return {};
		}

		uint64 GetCheckpointOffset(const uint32 Ordinal) const
		{
			check(Ordinal < UE_ARRAY_COUNT(CheckpointOffsets));
			return CheckpointOffsets[Ordinal];
		}

		FAngelscriptCacheValidationResult ValidateCurrentLocalShape() const
		{
			FAngelscriptTypeSchemaFieldCoordinate FailureCoordinate;
			const FAngelscriptCacheValidationResult Result =
				AngelscriptCacheTypeSchemaProducer_Private::ValidateProducerShape(
					Value, &FailureCoordinate);
			if (Result.IsSuccess())
			{
				return {};
			}
			const TOptional<uint64> ExactOffset = FailureCoordinate.Field
				!= EAngelscriptTypeSchemaCapturedField::Invalid
				? FindOffset(FailureCoordinate.Field,
					FailureCoordinate.PrimaryIndex,
					FailureCoordinate.SecondaryIndex)
				: TOptional<uint64>{};
			return FAngelscriptCacheValidationResult::AtStage(
				Result.Error,
				EAngelscriptCacheRecordKind::TypeSchema,
				EAngelscriptCacheValidationStage::LocalSemantic,
				ExactOffset.Get(GuessSemanticFailureOffset(Result.Error)));
		}

	private:
		static bool TryMultiplyCount(
			const uint64 Count,
			const uint64 Factor,
			uint64& OutCount)
		{
			if (Factor != 0 && Count > MAX_uint64 / Factor)
			{
				return false;
			}
			OutCount = Count * Factor;
			return true;
		}

		template <typename ArrayType>
		bool ReserveOffsets(
			const uint64 FieldOffset,
			const uint64 Count,
			ArrayType& Entries)
		{
			return Reader.ReserveDecodedArrayAtOffset(FieldOffset, Count, Entries);
		}

		static FAngelscriptTypeSchemaFieldCoordinate Coordinate(
			const EAngelscriptTypeSchemaCapturedField Field,
			const uint32 Primary = UnusedCapturedIndex,
			const uint32 Secondary = UnusedCapturedIndex,
			const uint32 Tertiary = UnusedCapturedIndex)
		{
			return {Field, Primary, Secondary, Tertiary};
		}

		template <typename ArrayType>
		static void Capture(
			ArrayType& Entries,
			const EAngelscriptTypeSchemaCapturedField Field,
			const uint64 Offset,
			const uint32 Primary = UnusedCapturedIndex,
			const uint32 Secondary = UnusedCapturedIndex,
			const uint32 Tertiary = UnusedCapturedIndex)
		{
			Entries.Add(FOffsetEntry{Coordinate(
				Field, Primary, Secondary, Tertiary), Offset});
		}

		bool ReadCapturedString(
			const EAngelscriptTypeSchemaCapturedField Field,
			FString& OutValue)
		{
			Capture(Offsets.FlatHeaderOffsets, Field, Reader.GetOffset());
			return Reader.ReadString(OutValue);
		}

		bool ReadOptionalUInt32(TOptional<uint32>& OutValue)
		{
			OutValue.Reset();
			const uint64 TagOffset = Reader.GetOffset();
			uint8 Tag = 0;
			if (!Reader.ReadUInt8(Tag))
			{
				return false;
			}
			if (Tag > 1)
			{
				Reader.Fail(EAngelscriptCacheValidationError::InvalidOptionalTag, TagOffset);
				return false;
			}
			if (Tag == 1)
			{
				uint32 Value32 = 0;
				if (!Reader.ReadUInt32(Value32))
				{
					return false;
				}
				OutValue = Value32;
			}
			return true;
		}

		bool ReadStableReference(FAngelscriptCacheStableReference& OutValue)
		{
			const uint64 ReferenceOffset = Reader.GetOffset();
			if (!Reader.ConsumeReference())
			{
				return false;
			}
			if (!ReadEnum(Reader, OutValue.Kind, 1, 9)
				|| !Reader.ReadHash(OutValue.StableKey)
				|| !Reader.ReadHash(OutValue.ExpectedAbi))
			{
				return false;
			}
			(void)ReferenceOffset;
			return true;
		}

		bool ReadMetadataArray(TArray<FAngelscriptCachedMetadataEntry>& OutValues)
		{
			uint32 Count = 0;
			if (!Reader.ReadArrayCountAndReserve(8, OutValues, Count))
			{
				return false;
			}
			for (uint32 Index = 0; Index < Count; ++Index)
			{
				FAngelscriptCachedMetadataEntry Entry;
				if (!Reader.ReadString(Entry.CanonicalKey)
					|| !Reader.ReadString(Entry.CanonicalValue))
				{
					return false;
				}
				OutValues.Add(MoveTemp(Entry));
			}
			return true;
		}

		bool ReadTopLevelMetadata()
		{
			const uint64 CountOffset = Reader.GetOffset();
			CheckpointOffsets[0] = CountOffset;
			Capture(Offsets.FlatHeaderOffsets,
				EAngelscriptTypeSchemaCapturedField::Metadata, CountOffset);
			uint32 Count = 0;
			if (!Reader.ReadArrayCountAndReserve(8, Value.Metadata, Count))
			{
				return false;
			}
			for (uint32 Index = 0; Index < Count; ++Index)
			{
				FAngelscriptCachedMetadataEntry Entry;
				if (!Reader.ReadString(Entry.CanonicalKey)
					|| !Reader.ReadString(Entry.CanonicalValue))
				{
					return false;
				}
				Value.Metadata.Add(MoveTemp(Entry));
			}
			if (!ReserveOffsets(CountOffset, Count, Offsets.ParallelMetadataOffsets))
			{
				return false;
			}
			uint64 ScanOffset = CountOffset + 4;
			for (uint32 Index = 0; Index < Count; ++Index)
			{
				Capture(Offsets.ParallelMetadataOffsets,
					EAngelscriptTypeSchemaCapturedField::MetadataEntry,
					ScanOffset, Index);
				if (!SkipRawString(ScanOffset) || !SkipRawString(ScanOffset))
				{
					Reader.Fail(EAngelscriptCacheValidationError::OutOfBounds, ScanOffset);
					return false;
				}
			}
			return true;
		}

		bool ReadRelations()
		{
			const uint64 CountOffset = Reader.GetOffset();
			CheckpointOffsets[1] = CountOffset;
			uint32 Count = 0;
			if (!Reader.ReadArrayCountAndReserve(67, Value.Relations, Count))
			{
				return false;
			}
			uint64 OffsetCount = 0;
			if (!TryMultiplyCount(Count, 2, OffsetCount)
				|| !ReserveOffsets(CountOffset, OffsetCount,
					Offsets.ParallelRelationOffsets))
			{
				if (!Reader.HasFailed())
				{
					Reader.Fail(EAngelscriptCacheValidationError::Overflow, CountOffset);
				}
				return false;
			}
			for (uint32 Index = 0; Index < Count; ++Index)
			{
				FAngelscriptCachedTypeRelation Relation;
				Capture(Offsets.ParallelRelationOffsets,
					EAngelscriptTypeSchemaCapturedField::Relation,
					Reader.GetOffset(), Index);
				if (!ReadEnum(Reader, Relation.RelationKind, 1, 5)
					|| !ReadOptionalUInt32(Relation.SemanticOrdinal))
				{
					return false;
				}
				Capture(Offsets.ParallelRelationOffsets,
					EAngelscriptTypeSchemaCapturedField::RelationTarget,
					Reader.GetOffset(), Index);
				if (!ReadStableReference(Relation.Target))
				{
					return false;
				}
				Value.Relations.Add(MoveTemp(Relation));
			}
			return true;
		}

		bool ReadLayoutInputs()
		{
			const uint64 CountOffset = Reader.GetOffset();
			CheckpointOffsets[2] = CountOffset;
			uint32 Count = 0;
			if (!Reader.ReadArrayCountAndReserve(100, Value.LayoutInputs, Count))
			{
				return false;
			}
			uint64 OffsetCount = 0;
			if (!TryMultiplyCount(Count, 2, OffsetCount)
				|| !ReserveOffsets(CountOffset, OffsetCount,
					Offsets.ParallelLayoutInputOffsets))
			{
				if (!Reader.HasFailed())
				{
					Reader.Fail(EAngelscriptCacheValidationError::Overflow, CountOffset);
				}
				return false;
			}
			for (uint32 Index = 0; Index < Count; ++Index)
			{
				FAngelscriptCachedTypeLayoutInput Input;
				Capture(Offsets.ParallelLayoutInputOffsets,
					EAngelscriptTypeSchemaCapturedField::LayoutInput,
					Reader.GetOffset(), Index);
				if (!ReadEnum(Reader, Input.InputKind, 1, 3))
				{
					return false;
				}
				Capture(Offsets.ParallelLayoutInputOffsets,
					EAngelscriptTypeSchemaCapturedField::LayoutInputTarget,
					Reader.GetOffset(), Index);
				if (!ReadStableReference(Input.Target)
					|| !ReadOptionalUInt32(Input.BoundaryContribution)
					|| !ReadOptionalUInt32(Input.AlignmentContribution)
					|| !Reader.ReadHash(Input.LayoutInputHash))
				{
					return false;
				}
				Value.LayoutInputs.Add(MoveTemp(Input));
			}
			return true;
		}

		bool ReadDataType(
			FAngelscriptCachedDataType& OutType,
			const uint64 Depth)
		{
			if (Depth > Reader.GetLimits().MaxNestingDepth)
			{
				Reader.Fail(EAngelscriptCacheValidationError::NestingDepthExceeded);
				return false;
			}
			if (!ReadEnum(Reader, OutType.Kind, 1, 4)
				|| !ReadEnum(Reader, OutType.Primitive, 0, 12))
			{
				return false;
			}
			const uint64 TagOffset = Reader.GetOffset();
			uint8 HasReference = 0;
			if (!Reader.ReadUInt8(HasReference))
			{
				return false;
			}
			if (HasReference > 1)
			{
				Reader.Fail(EAngelscriptCacheValidationError::InvalidOptionalTag, TagOffset);
				return false;
			}
			if (HasReference == 1)
			{
				FAngelscriptCacheStableReference Reference;
				if (!ReadStableReference(Reference))
				{
					return false;
				}
				OutType.TypeReference = MoveTemp(Reference);
			}
			if (!Reader.ReadUInt32(OutType.QualifierFlags))
			{
				return false;
			}
			uint32 Count = 0;
			if (!Reader.ReadArrayCountAndReserve(11, OutType.OrderedSubTypes, Count))
			{
				return false;
			}
			for (uint32 Index = 0; Index < Count; ++Index)
			{
				FAngelscriptCachedDataType Child;
				if (!ReadDataType(Child, Depth + 1))
				{
					return false;
				}
				OutType.OrderedSubTypes.Add(MoveTemp(Child));
			}
			return true;
		}

		bool ReadProperties()
		{
			const uint64 CountOffset = Reader.GetOffset();
			uint32 Count = 0;
			if (!Reader.ReadArrayCountAndReserve(1, Value.OrderedProperties, Count))
			{
				return false;
			}
			uint64 ParallelCount = 0;
			if (!TryMultiplyCount(Count, 2, ParallelCount)
				|| !ReserveOffsets(CountOffset, ParallelCount,
					Offsets.ParallelPropertyOffsets))
			{
				if (!Reader.HasFailed())
				{
					Reader.Fail(EAngelscriptCacheValidationError::Overflow, CountOffset);
				}
				return false;
			}
			for (uint32 Index = 0; Index < Count; ++Index)
			{
				FAngelscriptCachedPropertySchema Property;
				Capture(Offsets.ParallelPropertyOffsets,
					EAngelscriptTypeSchemaCapturedField::OrderedProperty,
					Reader.GetOffset(), Index);
				if (Index == 0)
				{
					CheckpointOffsets[7] = Reader.GetOffset();
				}
				if (!Reader.ReadUInt32(Property.LayoutOrdinal)
					|| !Reader.ReadUInt32(Property.SemanticByteOffset))
				{
					return false;
				}
				Capture(Offsets.ParallelPropertyOffsets,
					EAngelscriptTypeSchemaCapturedField::PropertyKey,
					Reader.GetOffset(), Index);
				if (!Reader.ReadHash(Property.PropertyKey.Hash)
					|| !Reader.ReadString(Property.CanonicalName)
					|| !ReadDataType(Property.Type, 1)
					|| !ReadEnum(Reader, Property.StorageKind, 1, 2)
					|| !Reader.ReadUInt32(Property.SemanticStorageSize)
					|| !Reader.ReadUInt32(Property.SemanticStorageAlignment)
					|| !Reader.ReadHash(Property.StorageLayoutHash)
					|| !ReadEnum(Reader, Property.Access, 1, 3)
					|| !Reader.ReadUInt32(Property.PropertySemanticFlags)
					|| !ReadEnum(Reader, Property.ReplicationCondition, 0, 16))
				{
					return false;
				}
				if (Index == 0)
				{
					CheckpointOffsets[5] = Reader.GetOffset();
				}
				if (!ReadMetadataArray(Property.Metadata)
					|| !Reader.ReadHash(Property.PropertyLayoutFingerprint))
				{
					return false;
				}
				Value.OrderedProperties.Add(MoveTemp(Property));
			}

			uint64 NestedCount = 0;
			for (const FAngelscriptCachedPropertySchema& Property : Value.OrderedProperties)
			{
				NestedCount += CountDataTypeNodes(Property.Type);
				NestedCount += static_cast<uint64>(Property.Metadata.Num());
			}
			if (!ReserveOffsets(CountOffset, NestedCount,
				Offsets.ParallelNestedPropertyOffsets)
				|| !ScanPropertyNestedOffsets(CountOffset, Count))
			{
				return false;
			}
			return true;
		}

		bool ReadMethods()
		{
			const uint64 CountOffset = Reader.GetOffset();
			CheckpointOffsets[8] = CountOffset;
			uint32 Count = 0;
			if (!Reader.ReadArrayCountAndReserve(101, Value.OrderedMethods, Count))
			{
				return false;
			}
			uint64 OffsetCount = 0;
			if (!TryMultiplyCount(Count, 3, OffsetCount)
				|| !ReserveOffsets(CountOffset, OffsetCount,
					Offsets.ParallelMethodOffsets))
			{
				if (!Reader.HasFailed())
				{
					Reader.Fail(EAngelscriptCacheValidationError::Overflow, CountOffset);
				}
				return false;
			}
			for (uint32 Index = 0; Index < Count; ++Index)
			{
				FAngelscriptCachedMethodEntry Method;
				Capture(Offsets.ParallelMethodOffsets,
					EAngelscriptTypeSchemaCapturedField::OrderedMethod,
					Reader.GetOffset(), Index);
				if (!ReadEnum(Reader, Method.EntryKind, 1, 4)
					|| !Reader.ReadUInt32(Method.MethodOrdinal))
				{
					return false;
				}
				Capture(Offsets.ParallelMethodOffsets,
					EAngelscriptTypeSchemaCapturedField::MethodFunction,
					Reader.GetOffset(), Index);
				if (!Reader.ReadHash(Method.FunctionKey.Hash))
				{
					return false;
				}
				Capture(Offsets.ParallelMethodOffsets,
					EAngelscriptTypeSchemaCapturedField::MethodDeclaringOwner,
					Reader.GetOffset(), Index);
				if (!Reader.ReadHash(Method.DeclaringOwner.Hash)
					|| !Reader.ReadHash(Method.ExpectedDeclarationAbi))
				{
					return false;
				}
				Value.OrderedMethods.Add(MoveTemp(Method));
			}
			return true;
		}

		bool ReadVirtualFunctionTable()
		{
			const uint64 CountOffset = Reader.GetOffset();
			CheckpointOffsets[9] = CountOffset;
			uint32 Count = 0;
			if (!Reader.ReadArrayCountAndReserve(133, Value.VirtualFunctionTable, Count))
			{
				return false;
			}
			uint64 OffsetCount = 0;
			if (!TryMultiplyCount(Count, 4, OffsetCount)
				|| !ReserveOffsets(CountOffset, OffsetCount,
					Offsets.ParallelVftOffsets))
			{
				if (!Reader.HasFailed())
				{
					Reader.Fail(EAngelscriptCacheValidationError::Overflow, CountOffset);
				}
				return false;
			}
			for (uint32 Index = 0; Index < Count; ++Index)
			{
				FAngelscriptCachedVirtualFunctionSlot Slot;
				Capture(Offsets.ParallelVftOffsets,
					EAngelscriptTypeSchemaCapturedField::VirtualFunctionSlot,
					Reader.GetOffset(), Index);
				if (!ReadEnum(Reader, Slot.SlotKind, 1, 4)
					|| !Reader.ReadUInt32(Slot.VftOrdinal))
				{
					return false;
				}
				Capture(Offsets.ParallelVftOffsets,
					EAngelscriptTypeSchemaCapturedField::VirtualFunction,
					Reader.GetOffset(), Index);
				if (!Reader.ReadHash(Slot.FunctionKey.Hash))
				{
					return false;
				}
				Capture(Offsets.ParallelVftOffsets,
					EAngelscriptTypeSchemaCapturedField::VirtualDeclaringOwner,
					Reader.GetOffset(), Index);
				if (!Reader.ReadHash(Slot.DeclaringOwner.Hash))
				{
					return false;
				}
				Capture(Offsets.ParallelVftOffsets,
					EAngelscriptTypeSchemaCapturedField::VirtualImplementingOwner,
					Reader.GetOffset(), Index);
				if (!Reader.ReadHash(Slot.ImplementingOwner.Hash)
					|| !Reader.ReadHash(Slot.ExpectedDeclarationAbi))
				{
					return false;
				}
				Value.VirtualFunctionTable.Add(MoveTemp(Slot));
			}
			return true;
		}

		bool ReadBehaviors()
		{
			const uint64 CountOffset = Reader.GetOffset();
			CheckpointOffsets[10] = CountOffset;
			uint32 Count = 0;
			if (!Reader.ReadArrayCountAndReserve(71, Value.OrderedBehaviorSlots, Count))
			{
				return false;
			}
			const uint64 OffsetReserve = static_cast<uint64>(Count) * 3;
			if (!ReserveOffsets(CountOffset, OffsetReserve,
				Offsets.ParallelBehaviorOffsets))
			{
				return false;
			}
			for (uint32 Index = 0; Index < Count; ++Index)
			{
				FAngelscriptCachedBehaviorSlot Slot;
				Capture(Offsets.ParallelBehaviorOffsets,
					EAngelscriptTypeSchemaCapturedField::BehaviorSlot,
					Reader.GetOffset(), Index);
				if (!ReadEnum(Reader, Slot.BehaviorKind, 1, 17)
					|| !Reader.ReadUInt32(Slot.SlotOrdinal))
				{
					return false;
				}
				Capture(Offsets.ParallelBehaviorOffsets,
					EAngelscriptTypeSchemaCapturedField::BehaviorTarget,
					Reader.GetOffset(), Index);
				if (!ReadStableReference(Slot.Target))
				{
					return false;
				}
				const uint64 TagOffset = Reader.GetOffset();
				uint8 HasOwner = 0;
				if (!Reader.ReadUInt8(HasOwner))
				{
					return false;
				}
				if (HasOwner > 1)
				{
					Reader.Fail(EAngelscriptCacheValidationError::InvalidOptionalTag, TagOffset);
					return false;
				}
				if (HasOwner == 1)
				{
					FAngelscriptStableTypeKey Owner;
					Capture(Offsets.ParallelBehaviorOffsets,
						EAngelscriptTypeSchemaCapturedField::BehaviorDeclaringOwner,
						Reader.GetOffset(), Index);
					if (!Reader.ReadHash(Owner.Hash))
					{
						return false;
					}
					Slot.DeclaringOwner = Owner;
				}
				Value.OrderedBehaviorSlots.Add(MoveTemp(Slot));
			}
			return true;
		}

		bool ReadKindPayload()
		{
			const uint64 KindOffset = Reader.GetOffset();
			switch (Value.TypeKind)
			{
			case EAngelscriptCachedTypeKind::Enum:
			{
				FAngelscriptCachedEnumTypePayload EnumPayload;
				uint32 Count = 0;
				if (!Reader.ReadArrayCountAndReserve(48,
					EnumPayload.OrderedEnumerators, Count))
				{
					return false;
				}
				for (uint32 Index = 0; Index < Count; ++Index)
				{
					FAngelscriptCachedEnumEnumerator Enumerator;
					uint32 RawValue = 0;
					if (!Reader.ReadUInt32(Enumerator.DeclarationOrdinal)
						|| !Reader.ReadString(Enumerator.CanonicalName)
						|| !Reader.ReadUInt32(RawValue))
					{
						return false;
					}
					Enumerator.Value = static_cast<int32>(RawValue);
					if (!ReadMetadataArray(Enumerator.Metadata))
					{
						return false;
					}
					EnumPayload.OrderedEnumerators.Add(MoveTemp(Enumerator));
				}
				if (!Reader.ReadHash(EnumPayload.EnumAuthorityHash))
				{
					return false;
				}
				Value.KindPayload.Enum = MoveTemp(EnumPayload);
				break;
			}
			case EAngelscriptCachedTypeKind::Delegate:
			case EAngelscriptCachedTypeKind::Funcdef:
			{
				FAngelscriptCachedCallableTypePayload Callable;
				uint8 Multicast = 0;
				if (!Reader.ReadHash(Callable.SignatureFunctionKey.Hash)
					|| !Reader.ReadHash(Callable.ExpectedSignatureAbi))
				{
					return false;
				}
				const uint64 BooleanOffset = Reader.GetOffset();
				if (!Reader.ReadUInt8(Multicast))
				{
					return false;
				}
				if (Multicast > 1)
				{
					Reader.Fail(EAngelscriptCacheValidationError::InvalidBoolean, BooleanOffset);
					return false;
				}
				Callable.bMulticast = Multicast != 0;
				Value.KindPayload.Callable = MoveTemp(Callable);
				break;
			}
			case EAngelscriptCachedTypeKind::Typedef:
			{
				FAngelscriptCachedTypedefTypePayload Typedef;
				if (!ReadDataType(Typedef.AliasedType, 1))
				{
					return false;
				}
				Value.KindPayload.Typedef = MoveTemp(Typedef);
				break;
			}
			default:
				break;
			}

			uint64 SelectedCount = 1;
			if (Value.KindPayload.Enum.IsSet())
			{
				SelectedCount += Value.KindPayload.Enum->OrderedEnumerators.Num();
				for (const FAngelscriptCachedEnumEnumerator& Enumerator :
					Value.KindPayload.Enum->OrderedEnumerators)
				{
					SelectedCount += Enumerator.Metadata.Num();
				}
			}
			else if (Value.KindPayload.Callable.IsSet())
			{
				++SelectedCount;
			}
			if (!ReserveOffsets(KindOffset, SelectedCount, Offsets.FlatSelectedArmOffsets)
				|| !ScanSelectedArmOffsets(KindOffset))
			{
				return false;
			}
			CheckpointOffsets[4] = KindOffset;
			return true;
		}

		bool ReadReflection()
		{
			const uint64 ReflectionOffset = Reader.GetOffset();
			CheckpointOffsets[11] = ReflectionOffset;
			Offsets.ReflectionOffset = FOffsetEntry{
				Coordinate(EAngelscriptTypeSchemaCapturedField::Reflection), ReflectionOffset};
			Offsets.ReflectionKindOffset = FOffsetEntry{
				Coordinate(EAngelscriptTypeSchemaCapturedField::ReflectionKind),
				Reader.GetOffset()};
			if (!ReadEnum(Reader, Value.Reflection.ReflectionKind, 1, 5))
			{
				return false;
			}
			Offsets.ClassReflectionFlagsOffset = FOffsetEntry{
				Coordinate(EAngelscriptTypeSchemaCapturedField::ClassReflectionFlags),
				Reader.GetOffset()};
			if (!Reader.ReadUInt32(Value.Reflection.ClassReflectionFlags)
				|| !Reader.ReadOptionalString(Value.Reflection.ConfigName)
				|| !Reader.ReadOptionalString(Value.Reflection.StaticClassGlobalName))
			{
				return false;
			}
			const uint64 CountOffset = Reader.GetOffset();
			uint32 Count = 0;
			if (!Reader.ReadArrayCountAndReserve(69,
				Value.Reflection.OrderedUFunctionMembers, Count))
			{
				return false;
			}
			uint64 OffsetCount = 0;
			if (!TryMultiplyCount(Count, 5, OffsetCount))
			{
				Reader.Fail(EAngelscriptCacheValidationError::Overflow, CountOffset);
				return false;
			}
			for (uint32 Index = 0; Index < Count; ++Index)
			{
				FAngelscriptCachedReflectedFunctionMember Member;
				if (!Reader.ReadUInt32(Member.ReflectionOrdinal)
					|| !Reader.ReadString(Member.CanonicalFunctionName)
					|| !Reader.ReadString(Member.CanonicalOriginalFunctionName)
					|| !Reader.ReadString(Member.CanonicalScriptFunctionName)
					|| !ReadStableReference(Member.Target))
				{
					return false;
				}
				Value.Reflection.OrderedUFunctionMembers.Add(MoveTemp(Member));
			}
			if (!ReserveOffsets(ReflectionOffset, OffsetCount,
				Offsets.ParallelReflectionOffsets)
				|| !ScanReflectionMemberOffsets(ReflectionOffset, Count))
			{
				return false;
			}
			return true;
		}

		bool ReadDependencies()
		{
			const uint64 CountOffset = Reader.GetOffset();
			CheckpointOffsets[3] = CountOffset;
			CheckpointOffsets[6] = CountOffset;
			uint32 Count = 0;
			if (!Reader.ReadArrayCountAndReserve(67, Value.Dependencies, Count))
			{
				return false;
			}
			uint64 OffsetCount = 0;
			if (!TryMultiplyCount(Count, 2, OffsetCount)
				|| !ReserveOffsets(CountOffset, OffsetCount,
					Offsets.ParallelDependencyOffsets))
			{
				if (!Reader.HasFailed())
				{
					Reader.Fail(EAngelscriptCacheValidationError::Overflow, CountOffset);
				}
				return false;
			}
			for (uint32 Index = 0; Index < Count; ++Index)
			{
				FAngelscriptCacheSemanticDependency Dependency;
				Capture(Offsets.ParallelDependencyOffsets,
					EAngelscriptTypeSchemaCapturedField::Dependency,
					Reader.GetOffset(), Index);
				if (!ReadEnum(Reader, Dependency.Kind, 1, 12))
				{
					return false;
				}
				Capture(Offsets.ParallelDependencyOffsets,
					EAngelscriptTypeSchemaCapturedField::DependencyTarget,
					Reader.GetOffset(), Index);
				if (!ReadStableReference(Dependency.Target)
					|| !Reader.ReadOptionalHash(Dependency.ExpectedContentOrValue))
				{
					return false;
				}
				Value.Dependencies.Add(MoveTemp(Dependency));
			}
			return true;
		}

		uint64 CountDataTypeNodes(const FAngelscriptCachedDataType& Type) const
		{
			uint64 Count = 1;
			for (const FAngelscriptCachedDataType& Child : Type.OrderedSubTypes)
			{
				Count += CountDataTypeNodes(Child);
			}
			return Count;
		}

		bool CanScan(const uint64 Offset, const uint64 Size) const
		{
			return Offset <= static_cast<uint64>(Payload.Num())
				&& Size <= static_cast<uint64>(Payload.Num()) - Offset;
		}

		bool ScanUInt8(uint64& Offset, uint8& OutValue) const
		{
			if (!CanScan(Offset, 1))
			{
				return false;
			}
			OutValue = Payload[static_cast<int32>(Offset++)];
			return true;
		}

		bool ScanUInt32(uint64& Offset, uint32& OutValue) const
		{
			if (!CanScan(Offset, 4))
			{
				return false;
			}
			OutValue = static_cast<uint32>(Payload[Offset])
				| (static_cast<uint32>(Payload[Offset + 1]) << 8)
				| (static_cast<uint32>(Payload[Offset + 2]) << 16)
				| (static_cast<uint32>(Payload[Offset + 3]) << 24);
			Offset += 4;
			return true;
		}

		bool ScanSkip(uint64& Offset, const uint64 Size) const
		{
			if (!CanScan(Offset, Size))
			{
				return false;
			}
			Offset += Size;
			return true;
		}

		bool SkipRawString(uint64& Offset) const
		{
			uint32 Length = 0;
			return ScanUInt32(Offset, Length) && ScanSkip(Offset, Length);
		}

		bool ScanSkipOptionalUInt32(uint64& Offset) const
		{
			uint8 Tag = 0;
			return ScanUInt8(Offset, Tag) && (Tag == 0 || (Tag == 1 && ScanSkip(Offset, 4)));
		}

		bool ScanSkipStableReference(uint64& Offset) const
		{
			return ScanSkip(Offset, 1 + 2 * sizeof(FBlake3Hash::ByteArray));
		}

		bool ScanSkipMetadata(uint64& Offset) const
		{
			uint32 Count = 0;
			if (!ScanUInt32(Offset, Count))
			{
				return false;
			}
			for (uint32 Index = 0; Index < Count; ++Index)
			{
				if (!SkipRawString(Offset) || !SkipRawString(Offset))
				{
					return false;
				}
			}
			return true;
		}

		bool ScanPropertyDataType(
			uint64& Offset,
			const uint32 PropertyIndex,
			uint32& PreOrder)
		{
			Capture(Offsets.ParallelNestedPropertyOffsets,
				EAngelscriptTypeSchemaCapturedField::PropertyType,
				Offset, PropertyIndex, PreOrder++);
			uint8 HasReference = 0;
			if (!ScanSkip(Offset, 2)
				|| !ScanUInt8(Offset, HasReference)
				|| (HasReference == 1 && !ScanSkipStableReference(Offset))
				|| !ScanSkip(Offset, 4))
			{
				return false;
			}
			uint32 ChildCount = 0;
			if (!ScanUInt32(Offset, ChildCount))
			{
				return false;
			}
			for (uint32 ChildIndex = 0; ChildIndex < ChildCount; ++ChildIndex)
			{
				if (!ScanPropertyDataType(Offset, PropertyIndex, PreOrder))
				{
					return false;
				}
			}
			return true;
		}

		bool ScanPropertyNestedOffsets(const uint64 CountOffset, const uint32 Count)
		{
			uint64 Offset = CountOffset + 4;
			for (uint32 PropertyIndex = 0; PropertyIndex < Count; ++PropertyIndex)
			{
				if (!ScanSkip(Offset, 8 + sizeof(FBlake3Hash::ByteArray))
					|| !SkipRawString(Offset))
				{
					return false;
				}
				uint32 PreOrder = 0;
				if (!ScanPropertyDataType(Offset, PropertyIndex, PreOrder)
					|| !ScanSkip(Offset,
						1 + 4 + 4 + sizeof(FBlake3Hash::ByteArray) + 1 + 4 + 1))
				{
					return false;
				}
				uint32 MetadataCount = 0;
				if (!ScanUInt32(Offset, MetadataCount))
				{
					return false;
				}
				for (uint32 MetadataIndex = 0;
					MetadataIndex < MetadataCount; ++MetadataIndex)
				{
					Capture(Offsets.ParallelNestedPropertyOffsets,
						EAngelscriptTypeSchemaCapturedField::PropertyMetadata,
						Offset, PropertyIndex, MetadataIndex);
					if (!SkipRawString(Offset) || !SkipRawString(Offset))
					{
						return false;
					}
				}
				if (!ScanSkip(Offset, sizeof(FBlake3Hash::ByteArray)))
				{
					return false;
				}
			}
			return true;
		}

		bool ScanSelectedDataType(uint64& Offset) const
		{
			uint8 HasReference = 0;
			if (!ScanSkip(Offset, 2)
				|| !ScanUInt8(Offset, HasReference)
				|| (HasReference == 1 && !ScanSkipStableReference(Offset))
				|| !ScanSkip(Offset, 4))
			{
				return false;
			}
			uint32 Count = 0;
			if (!ScanUInt32(Offset, Count))
			{
				return false;
			}
			for (uint32 Index = 0; Index < Count; ++Index)
			{
				if (!ScanSelectedDataType(Offset))
				{
					return false;
				}
			}
			return true;
		}

		bool ScanSelectedArmOffsets(const uint64 KindOffset)
		{
			Capture(Offsets.FlatSelectedArmOffsets,
				EAngelscriptTypeSchemaCapturedField::KindPayload, KindOffset);
			uint64 Offset = KindOffset;
			if (Value.TypeKind == EAngelscriptCachedTypeKind::Enum)
			{
				uint32 Count = 0;
				if (!ScanUInt32(Offset, Count))
				{
					return false;
				}
				for (uint32 EnumIndex = 0; EnumIndex < Count; ++EnumIndex)
				{
					Capture(Offsets.FlatSelectedArmOffsets,
						EAngelscriptTypeSchemaCapturedField::EnumEnumerator,
						Offset, EnumIndex);
					if (!ScanSkip(Offset, 4) || !SkipRawString(Offset)
						|| !ScanSkip(Offset, 4))
					{
						return false;
					}
					uint32 MetadataCount = 0;
					if (!ScanUInt32(Offset, MetadataCount))
					{
						return false;
					}
					for (uint32 MetadataIndex = 0;
						MetadataIndex < MetadataCount; ++MetadataIndex)
					{
						Capture(Offsets.FlatSelectedArmOffsets,
							EAngelscriptTypeSchemaCapturedField::EnumEnumeratorMetadata,
							Offset, EnumIndex, MetadataIndex);
						if (!SkipRawString(Offset) || !SkipRawString(Offset))
						{
							return false;
						}
					}
				}
				return ScanSkip(Offset, sizeof(FBlake3Hash::ByteArray));
			}
			if (Value.TypeKind == EAngelscriptCachedTypeKind::Delegate
				|| Value.TypeKind == EAngelscriptCachedTypeKind::Funcdef)
			{
				Capture(Offsets.FlatSelectedArmOffsets,
					EAngelscriptTypeSchemaCapturedField::CallableSignature, Offset);
				return ScanSkip(Offset, 2 * sizeof(FBlake3Hash::ByteArray) + 1);
			}
			if (Value.TypeKind == EAngelscriptCachedTypeKind::Typedef)
			{
				return ScanSelectedDataType(Offset);
			}
			return true;
		}

		bool ScanReflectionMemberOffsets(
			const uint64 ReflectionOffset,
			const uint32 Count)
		{
			uint64 Offset = ReflectionOffset;
			if (!ScanSkip(Offset, 1 + 4))
			{
				return false;
			}
			for (int32 OptionalIndex = 0; OptionalIndex < 2; ++OptionalIndex)
			{
				uint8 Tag = 0;
				if (!ScanUInt8(Offset, Tag)
					|| (Tag == 1 && !SkipRawString(Offset)))
				{
					return false;
				}
			}
			uint32 WireCount = 0;
			if (!ScanUInt32(Offset, WireCount) || WireCount != Count)
			{
				return false;
			}
			for (uint32 Index = 0; Index < Count; ++Index)
			{
				Capture(Offsets.ParallelReflectionOffsets,
					EAngelscriptTypeSchemaCapturedField::ReflectedFunctionMember,
					Offset, Index);
				if (!ScanSkip(Offset, 4))
				{
					return false;
				}
				Capture(Offsets.ParallelReflectionOffsets,
					EAngelscriptTypeSchemaCapturedField::ReflectedFunctionName,
					Offset, Index);
				if (!SkipRawString(Offset))
				{
					return false;
				}
				Capture(Offsets.ParallelReflectionOffsets,
					EAngelscriptTypeSchemaCapturedField::ReflectedOriginalFunctionName,
					Offset, Index);
				if (!SkipRawString(Offset))
				{
					return false;
				}
				Capture(Offsets.ParallelReflectionOffsets,
					EAngelscriptTypeSchemaCapturedField::ReflectedScriptFunctionName,
					Offset, Index);
				if (!SkipRawString(Offset))
				{
					return false;
				}
				Capture(Offsets.ParallelReflectionOffsets,
					EAngelscriptTypeSchemaCapturedField::ReflectedFunctionTarget,
					Offset, Index);
				if (!ScanSkipStableReference(Offset))
				{
					return false;
				}
			}
			return true;
		}

		TOptional<uint64> FindOffset(
			const EAngelscriptTypeSchemaCapturedField Field,
			const uint32 Primary = UnusedCapturedIndex,
			const uint32 Secondary = UnusedCapturedIndex) const
		{
			const FAngelscriptTypeSchemaFieldCoordinate Wanted =
				Coordinate(Field, Primary, Secondary);
			const auto Search = [&Wanted](const auto& Entries) -> TOptional<uint64>
			{
				for (const auto& Entry : Entries)
				{
					if (Entry.Coordinate.Field == Wanted.Field
						&& Entry.Coordinate.PrimaryIndex == Wanted.PrimaryIndex
						&& Entry.Coordinate.SecondaryIndex == Wanted.SecondaryIndex)
					{
						return Entry.Offset;
					}
				}
				return {};
			};
			const TOptional<uint64> Header = Search(Offsets.FlatHeaderOffsets);
			if (Header.IsSet()) return Header;
			const TOptional<uint64> Metadata = Search(Offsets.ParallelMetadataOffsets);
			if (Metadata.IsSet()) return Metadata;
			const TOptional<uint64> Relation = Search(Offsets.ParallelRelationOffsets);
			if (Relation.IsSet()) return Relation;
			const TOptional<uint64> LayoutInput = Search(Offsets.ParallelLayoutInputOffsets);
			if (LayoutInput.IsSet()) return LayoutInput;
			const TOptional<uint64> Property = Search(Offsets.ParallelPropertyOffsets);
			if (Property.IsSet()) return Property;
			const TOptional<uint64> NestedProperty =
				Search(Offsets.ParallelNestedPropertyOffsets);
			if (NestedProperty.IsSet()) return NestedProperty;
			const TOptional<uint64> Method = Search(Offsets.ParallelMethodOffsets);
			if (Method.IsSet()) return Method;
			const TOptional<uint64> VirtualFunction = Search(Offsets.ParallelVftOffsets);
			if (VirtualFunction.IsSet()) return VirtualFunction;
			const TOptional<uint64> Behavior = Search(Offsets.ParallelBehaviorOffsets);
			if (Behavior.IsSet()) return Behavior;
			const TOptional<uint64> Selected = Search(Offsets.FlatSelectedArmOffsets);
			if (Selected.IsSet()) return Selected;
			if (Offsets.ReflectionOffset.IsSet()
				&& Offsets.ReflectionOffset->Coordinate.Field == Wanted.Field
				&& Offsets.ReflectionOffset->Coordinate.PrimaryIndex == Wanted.PrimaryIndex
				&& Offsets.ReflectionOffset->Coordinate.SecondaryIndex
					== Wanted.SecondaryIndex)
			{
				return Offsets.ReflectionOffset->Offset;
			}
			if (Offsets.ReflectionKindOffset.IsSet()
				&& Offsets.ReflectionKindOffset->Coordinate.Field == Wanted.Field
				&& Offsets.ReflectionKindOffset->Coordinate.PrimaryIndex
					== Wanted.PrimaryIndex
				&& Offsets.ReflectionKindOffset->Coordinate.SecondaryIndex
					== Wanted.SecondaryIndex)
			{
				return Offsets.ReflectionKindOffset->Offset;
			}
			if (Offsets.ClassReflectionFlagsOffset.IsSet()
				&& Offsets.ClassReflectionFlagsOffset->Coordinate.Field == Wanted.Field
				&& Offsets.ClassReflectionFlagsOffset->Coordinate.PrimaryIndex
					== Wanted.PrimaryIndex
				&& Offsets.ClassReflectionFlagsOffset->Coordinate.SecondaryIndex
					== Wanted.SecondaryIndex)
			{
				return Offsets.ClassReflectionFlagsOffset->Offset;
			}
			const TOptional<uint64> Reflection = Search(Offsets.ParallelReflectionOffsets);
			if (Reflection.IsSet()) return Reflection;
			const TOptional<uint64> Dependency = Search(Offsets.ParallelDependencyOffsets);
			if (Dependency.IsSet()) return Dependency;
			return {};
		}

		uint64 GuessSemanticFailureOffset(
			const EAngelscriptCacheValidationError Error) const
		{
			if (Error == EAngelscriptCacheValidationError::MissingCoverage)
			{
				return CheckpointOffsets[3];
			}
			if (Error == EAngelscriptCacheValidationError::UnsupportedPayloadSchema)
			{
				return FindOffset(EAngelscriptTypeSchemaCapturedField::PayloadSchemaVersion).Get(0);
			}
			if (Error == EAngelscriptCacheValidationError::UnknownFlags)
			{
				return FindOffset(EAngelscriptTypeSchemaCapturedField::TypeSemanticFlags).Get(0);
			}
			if (Error == EAngelscriptCacheValidationError::DerivedHashMismatch)
			{
				return FindOffset(EAngelscriptTypeSchemaCapturedField::LayoutExpectation).Get(0);
			}
			if (Error == EAngelscriptCacheValidationError::ZeroStableKey)
			{
				if (Value.ModuleKey.Hash.IsZero())
				{
					return FindOffset(EAngelscriptTypeSchemaCapturedField::ModuleKey).Get(0);
				}
				if (Value.TypeKey.Hash.IsZero())
				{
					return FindOffset(EAngelscriptTypeSchemaCapturedField::TypeKey).Get(0);
				}
			}
			return FindOffset(EAngelscriptTypeSchemaCapturedField::KindPayload).Get(0);
		}

		TConstArrayView<uint8> Payload;
		FReader Reader;
		FAngelscriptCachedTypeSchema& Value;
		OffsetStorageType& Offsets;
		uint64 CheckpointOffsets[12];
	};

	FAngelscriptCacheValidationResult FDecodedRecordCodecBridge::TryDecodeTypeSchema(
		const TConstArrayView<uint8> Payload,
		const FAngelscriptCacheReadLimits& Limits,
		FAngelscriptCacheReadBudget& Budget,
		const FDecodedChargeSink& ChargeSink,
#if WITH_ANGELSCRIPT_UNITTESTS
		FAngelscriptCacheTypeSchemaAllocationProbeForTests* Probe,
#endif
		FAngelscriptCachedTypeSchema& OutValue,
		FAngelscriptDecodedCacheRecord::FTypeSchemaCapturedOffsetStorage& OutOffsets)
	{
		if (const FAngelscriptCacheValidationResult BeginResult = BeginRead(
			Payload, Limits, Budget, EAngelscriptCacheRecordKind::TypeSchema);
			!BeginResult.IsSuccess())
		{
			return BeginResult;
		}
		TTypeSchemaDecoder Decoder(
			Payload, Limits, Budget, ChargeSink, OutValue, OutOffsets);
		if (const FAngelscriptCacheValidationResult Physical = Decoder.DecodePhysical();
			!Physical.IsSuccess())
		{
			return Physical;
		}

#if WITH_ANGELSCRIPT_UNITTESTS
		if (Probe != nullptr)
		{
			for (uint32 Checkpoint = 0; Checkpoint < 12; ++Checkpoint)
			{
				if (Probe->RecordValidationCheckpointForTests(Checkpoint, Budget))
				{
					return FAngelscriptCacheValidationResult::AtStage(
						EAngelscriptCacheValidationError::Overflow,
						EAngelscriptCacheRecordKind::TypeSchema,
						EAngelscriptCacheValidationStage::LocalSemantic,
						Decoder.GetCheckpointOffset(Checkpoint));
				}
			}

			uint64 InjectedOffset = 0;
			if (Probe->ConsumeDeferredAcceptedEventFailureForTests(
				EAngelscriptCacheTypeSchemaInjectedFailureForTests::LocalAfterTarget,
				InjectedOffset))
			{
				return FAngelscriptCacheValidationResult::AtStage(
					EAngelscriptCacheValidationError::Overflow,
					EAngelscriptCacheRecordKind::TypeSchema,
					EAngelscriptCacheValidationStage::LocalSemantic,
					InjectedOffset);
			}
			if (Probe->ConsumeDeferredAcceptedEventFailureForTests(
				EAngelscriptCacheTypeSchemaInjectedFailureForTests::HashAfterTarget,
				InjectedOffset))
			{
				return FAngelscriptCacheValidationResult::AtStage(
					EAngelscriptCacheValidationError::Overflow,
					EAngelscriptCacheRecordKind::TypeSchema,
					EAngelscriptCacheValidationStage::LocalSemantic,
					InjectedOffset);
			}
		}
#endif

		return Decoder.ValidateCurrentLocalShape();
	}
}

namespace AngelscriptCacheTypeSchemaProducer_Private
{

	static FAngelscriptCacheValidationResult WriteTypeSchemaPayload(
		const FAngelscriptCachedTypeSchema& Value,
		TArray<uint8>& OutPayload
#if WITH_ANGELSCRIPT_UNITTESTS
		, FTrace* Trace
#endif
		)
	{
		OutPayload.Reset();
#if WITH_ANGELSCRIPT_UNITTESTS
		if (Trace != nullptr)
		{
			Trace->Spans.Reset();
		}
#define UEAS_TRACE_WRITE(FieldName, Primary, Secondary, Tertiary, Expression) \
		do { const uint64 UEAS_Start = static_cast<uint64>(Writer.Bytes.Num()); \
			Expression; AddTrace(Trace, ETraceField::FieldName, UEAS_Start, \
				static_cast<uint64>(Writer.Bytes.Num()) - UEAS_Start, \
				Primary, Secondary, Tertiary); } while (false)
#define UEAS_TRACE_SPAN(FieldName, Offset, Size, Primary, Secondary, Tertiary) \
		AddTrace(Trace, ETraceField::FieldName, Offset, Size, Primary, Secondary, Tertiary)
#else
#define UEAS_TRACE_WRITE(FieldName, Primary, Secondary, Tertiary, Expression) \
		do { Expression; } while (false)
#define UEAS_TRACE_SPAN(FieldName, Offset, Size, Primary, Secondary, Tertiary) \
		do { } while (false)
#endif

		FWriter Writer;
		UEAS_TRACE_WRITE(PayloadSchemaVersion, INDEX_NONE, INDEX_NONE, INDEX_NONE,
			Writer.WriteUInt32(Value.PayloadSchemaVersion));
		UEAS_TRACE_WRITE(ModuleKey, INDEX_NONE, INDEX_NONE, INDEX_NONE,
			Writer.WriteHash(Value.ModuleKey.Hash));
		UEAS_TRACE_WRITE(TypeKey, INDEX_NONE, INDEX_NONE, INDEX_NONE,
			Writer.WriteHash(Value.TypeKey.Hash));
		UEAS_TRACE_WRITE(TypeKind, INDEX_NONE, INDEX_NONE, INDEX_NONE,
			Writer.WriteUInt8(static_cast<uint8>(Value.TypeKind)));
		UEAS_TRACE_WRITE(CanonicalNamespace, INDEX_NONE, INDEX_NONE, INDEX_NONE,
			Writer.WriteString(Value.CanonicalNamespace));
		UEAS_TRACE_WRITE(CanonicalNameBytes, INDEX_NONE, INDEX_NONE, INDEX_NONE,
			Writer.WriteString(Value.CanonicalName));
		UEAS_TRACE_WRITE(CanonicalDeclaration, INDEX_NONE, INDEX_NONE, INDEX_NONE,
			Writer.WriteString(Value.CanonicalDeclaration));
		UEAS_TRACE_WRITE(TypeSemanticFlags, INDEX_NONE, INDEX_NONE, INDEX_NONE,
			Writer.WriteUInt32(Value.TypeSemanticFlags));

		UEAS_TRACE_WRITE(Metadata, INDEX_NONE, INDEX_NONE, INDEX_NONE,
			Writer.WriteUInt32(static_cast<uint32>(Value.Metadata.Num())));
		for (int32 Index = 0; Index < Value.Metadata.Num(); ++Index)
		{
			UEAS_TRACE_WRITE(MetadataEntry, Index, INDEX_NONE, INDEX_NONE,
				WriteMetadata(Writer, Value.Metadata[Index]));
		}

		UEAS_TRACE_WRITE(Relations, INDEX_NONE, INDEX_NONE, INDEX_NONE,
			Writer.WriteUInt32(static_cast<uint32>(Value.Relations.Num())));
		for (int32 Index = 0; Index < Value.Relations.Num(); ++Index)
		{
			const FAngelscriptCachedTypeRelation& Relation = Value.Relations[Index];
			UEAS_TRACE_WRITE(RelationKind, Index, INDEX_NONE, INDEX_NONE,
				Writer.WriteUInt8(static_cast<uint8>(Relation.RelationKind)));
			UEAS_TRACE_WRITE(RelationSemanticOrdinalOptionalTag, Index, INDEX_NONE,
				INDEX_NONE, WriteOptionalUInt32(Writer, Relation.SemanticOrdinal));
			WriteStableReference(Writer, Relation.Target);
		}

		UEAS_TRACE_WRITE(LayoutInputs, INDEX_NONE, INDEX_NONE, INDEX_NONE,
			Writer.WriteUInt32(static_cast<uint32>(Value.LayoutInputs.Num())));
		for (int32 Index = 0; Index < Value.LayoutInputs.Num(); ++Index)
		{
			const FAngelscriptCachedTypeLayoutInput& Input = Value.LayoutInputs[Index];
			const uint64 RowStart = static_cast<uint64>(Writer.Bytes.Num());
			UEAS_TRACE_WRITE(LayoutInputKind, Index, INDEX_NONE, INDEX_NONE,
				Writer.WriteUInt8(static_cast<uint8>(Input.InputKind)));
			WriteStableReference(Writer, Input.Target);
			UEAS_TRACE_WRITE(LayoutInputBoundaryOptionalTag, Index, INDEX_NONE,
				INDEX_NONE, WriteOptionalUInt32(Writer, Input.BoundaryContribution));
			UEAS_TRACE_WRITE(LayoutInputAlignmentOptionalTag, Index, INDEX_NONE,
				INDEX_NONE, WriteOptionalUInt32(Writer, Input.AlignmentContribution));
			Writer.WriteHash(Input.LayoutInputHash);
			UEAS_TRACE_SPAN(LayoutInput, RowStart,
				static_cast<uint64>(Writer.Bytes.Num()) - RowStart,
				Index, INDEX_NONE, INDEX_NONE);
		}

		const uint64 LayoutStart = static_cast<uint64>(Writer.Bytes.Num());
		UEAS_TRACE_WRITE(LayoutSemanticSize, INDEX_NONE, INDEX_NONE, INDEX_NONE,
			Writer.WriteUInt64(Value.Layout.SemanticSize));
		UEAS_TRACE_WRITE(LayoutSemanticAlignment, INDEX_NONE, INDEX_NONE, INDEX_NONE,
			Writer.WriteUInt32(Value.Layout.SemanticAlignment));
		UEAS_TRACE_WRITE(LayoutBasePropertyBoundary, INDEX_NONE, INDEX_NONE, INDEX_NONE,
			Writer.WriteUInt32(Value.Layout.BasePropertyBoundary));
		UEAS_TRACE_WRITE(TypeLayoutHash, INDEX_NONE, INDEX_NONE, INDEX_NONE,
			Writer.WriteHash(Value.Layout.TypeLayoutHash));
		UEAS_TRACE_SPAN(Layout, LayoutStart,
			static_cast<uint64>(Writer.Bytes.Num()) - LayoutStart,
			INDEX_NONE, INDEX_NONE, INDEX_NONE);

		UEAS_TRACE_WRITE(OrderedProperties, INDEX_NONE, INDEX_NONE, INDEX_NONE,
			Writer.WriteUInt32(static_cast<uint32>(Value.OrderedProperties.Num())));
		for (int32 Index = 0; Index < Value.OrderedProperties.Num(); ++Index)
		{
			const FAngelscriptCachedPropertySchema& Property = Value.OrderedProperties[Index];
			const uint64 RowStart = static_cast<uint64>(Writer.Bytes.Num());
			Writer.WriteUInt32(Property.LayoutOrdinal);
			Writer.WriteUInt32(Property.SemanticByteOffset);
			Writer.WriteHash(Property.PropertyKey.Hash);
			Writer.WriteString(Property.CanonicalName);
			WriteDataType(Writer, Property.Type);
			UEAS_TRACE_WRITE(PropertyStorageKind, Index, INDEX_NONE, INDEX_NONE,
				Writer.WriteUInt8(static_cast<uint8>(Property.StorageKind)));
			Writer.WriteUInt32(Property.SemanticStorageSize);
			Writer.WriteUInt32(Property.SemanticStorageAlignment);
			Writer.WriteHash(Property.StorageLayoutHash);
			UEAS_TRACE_WRITE(PropertyMemberAccess, Index, INDEX_NONE, INDEX_NONE,
				Writer.WriteUInt8(static_cast<uint8>(Property.Access)));
			Writer.WriteUInt32(Property.PropertySemanticFlags);
			UEAS_TRACE_WRITE(PropertyReplicationCondition, Index, INDEX_NONE, INDEX_NONE,
				Writer.WriteUInt8(static_cast<uint8>(Property.ReplicationCondition)));
			WriteMetadataArray(Writer, Property.Metadata);
			Writer.WriteHash(Property.PropertyLayoutFingerprint);
			UEAS_TRACE_SPAN(OrderedProperty, RowStart,
				static_cast<uint64>(Writer.Bytes.Num()) - RowStart,
				Index, INDEX_NONE, INDEX_NONE);
		}

		UEAS_TRACE_WRITE(OrderedMethods, INDEX_NONE, INDEX_NONE, INDEX_NONE,
			Writer.WriteUInt32(static_cast<uint32>(Value.OrderedMethods.Num())));
		for (int32 Index = 0; Index < Value.OrderedMethods.Num(); ++Index)
		{
			const FAngelscriptCachedMethodEntry& Method = Value.OrderedMethods[Index];
			const uint64 RowStart = static_cast<uint64>(Writer.Bytes.Num());
			UEAS_TRACE_WRITE(MethodSlotKind, Index, INDEX_NONE, INDEX_NONE,
				Writer.WriteUInt8(static_cast<uint8>(Method.EntryKind)));
			Writer.WriteUInt32(Method.MethodOrdinal);
			Writer.WriteHash(Method.FunctionKey.Hash);
			Writer.WriteHash(Method.DeclaringOwner.Hash);
			Writer.WriteHash(Method.ExpectedDeclarationAbi);
			UEAS_TRACE_SPAN(OrderedMethod, RowStart,
				static_cast<uint64>(Writer.Bytes.Num()) - RowStart,
				Index, INDEX_NONE, INDEX_NONE);
		}

		UEAS_TRACE_WRITE(VirtualFunctionTable, INDEX_NONE, INDEX_NONE, INDEX_NONE,
			Writer.WriteUInt32(static_cast<uint32>(Value.VirtualFunctionTable.Num())));
		for (const FAngelscriptCachedVirtualFunctionSlot& Slot : Value.VirtualFunctionTable)
		{
			Writer.WriteUInt8(static_cast<uint8>(Slot.SlotKind));
			Writer.WriteUInt32(Slot.VftOrdinal);
			Writer.WriteHash(Slot.FunctionKey.Hash);
			Writer.WriteHash(Slot.DeclaringOwner.Hash);
			Writer.WriteHash(Slot.ImplementingOwner.Hash);
			Writer.WriteHash(Slot.ExpectedDeclarationAbi);
		}

		UEAS_TRACE_WRITE(BehaviorSlots, INDEX_NONE, INDEX_NONE, INDEX_NONE,
			Writer.WriteUInt32(static_cast<uint32>(Value.OrderedBehaviorSlots.Num())));
		for (int32 Index = 0; Index < Value.OrderedBehaviorSlots.Num(); ++Index)
		{
			const FAngelscriptCachedBehaviorSlot& Slot = Value.OrderedBehaviorSlots[Index];
			const uint64 RowStart = static_cast<uint64>(Writer.Bytes.Num());
			UEAS_TRACE_WRITE(BehaviorKind, Index, INDEX_NONE, INDEX_NONE,
				Writer.WriteUInt8(static_cast<uint8>(Slot.BehaviorKind)));
			Writer.WriteUInt32(Slot.SlotOrdinal);
			WriteStableReference(Writer, Slot.Target);
			UEAS_TRACE_WRITE(BehaviorDeclaringOwnerOptionalTag, Index, INDEX_NONE,
				INDEX_NONE, WriteOptionalTypeKey(Writer, Slot.DeclaringOwner));
			UEAS_TRACE_SPAN(BehaviorSlot, RowStart,
				static_cast<uint64>(Writer.Bytes.Num()) - RowStart,
				Index, INDEX_NONE, INDEX_NONE);
		}

		const uint64 KindPayloadStart = static_cast<uint64>(Writer.Bytes.Num());
		switch (Value.TypeKind)
		{
		case EAngelscriptCachedTypeKind::Enum:
		{
			const FAngelscriptCachedEnumTypePayload* EnumPayload =
				Value.KindPayload.Enum.IsSet() ? &Value.KindPayload.Enum.GetValue() : nullptr;
			Writer.WriteUInt32(static_cast<uint32>(
				EnumPayload != nullptr ? EnumPayload->OrderedEnumerators.Num() : 0));
			for (int32 Index = 0;
				EnumPayload != nullptr && Index < EnumPayload->OrderedEnumerators.Num(); ++Index)
			{
				const FAngelscriptCachedEnumEnumerator& Enumerator =
					EnumPayload->OrderedEnumerators[Index];
				Writer.WriteUInt32(Enumerator.DeclarationOrdinal);
				Writer.WriteString(Enumerator.CanonicalName);
				UEAS_TRACE_WRITE(EnumSignedValue, Index, INDEX_NONE, INDEX_NONE,
					Writer.WriteUInt32(static_cast<uint32>(Enumerator.Value)));
				WriteMetadataArray(Writer, Enumerator.Metadata);
			}
			Writer.WriteHash(EnumPayload != nullptr
				? EnumPayload->EnumAuthorityHash
				: FAngelscriptHash256{});
			break;
		}
		case EAngelscriptCachedTypeKind::Delegate:
		case EAngelscriptCachedTypeKind::Funcdef:
		{
			const FAngelscriptCachedCallableTypePayload* CallablePayload =
				Value.KindPayload.Callable.IsSet() ? &Value.KindPayload.Callable.GetValue() : nullptr;
			Writer.WriteHash(CallablePayload != nullptr
				? CallablePayload->SignatureFunctionKey.Hash
				: FAngelscriptHash256{});
			Writer.WriteHash(CallablePayload != nullptr
				? CallablePayload->ExpectedSignatureAbi
				: FAngelscriptHash256{});
			UEAS_TRACE_WRITE(CallableMulticastBoolean, INDEX_NONE, INDEX_NONE, INDEX_NONE,
				Writer.WriteUInt8(CallablePayload != nullptr && CallablePayload->bMulticast ? 1 : 0));
			break;
		}
		case EAngelscriptCachedTypeKind::Typedef:
			WriteDataType(Writer, Value.KindPayload.Typedef.IsSet()
				? Value.KindPayload.Typedef->AliasedType
				: FAngelscriptCachedDataType{});
			break;
		default:
			break;
		}
		UEAS_TRACE_SPAN(KindPayload, KindPayloadStart,
			static_cast<uint64>(Writer.Bytes.Num()) - KindPayloadStart,
			INDEX_NONE, INDEX_NONE, INDEX_NONE);

		const uint64 ReflectionStart = static_cast<uint64>(Writer.Bytes.Num());
		UEAS_TRACE_WRITE(ReflectionKind, INDEX_NONE, INDEX_NONE, INDEX_NONE,
			Writer.WriteUInt8(static_cast<uint8>(Value.Reflection.ReflectionKind)));
		Writer.WriteUInt32(Value.Reflection.ClassReflectionFlags);
		Writer.WriteOptionalString(Value.Reflection.ConfigName);
		Writer.WriteOptionalString(Value.Reflection.StaticClassGlobalName);
		Writer.WriteUInt32(static_cast<uint32>(
			Value.Reflection.OrderedUFunctionMembers.Num()));
		for (int32 Index = 0;
			Index < Value.Reflection.OrderedUFunctionMembers.Num(); ++Index)
		{
			const FAngelscriptCachedReflectedFunctionMember& Member =
				Value.Reflection.OrderedUFunctionMembers[Index];
			Writer.WriteUInt32(Member.ReflectionOrdinal);
			UEAS_TRACE_WRITE(ReflectedFunctionNameBytes, Index, INDEX_NONE,
				INDEX_NONE, Writer.WriteString(Member.CanonicalFunctionName));
			UEAS_TRACE_WRITE(ReflectedOriginalFunctionNameBytes, Index, INDEX_NONE,
				INDEX_NONE,
				Writer.WriteString(Member.CanonicalOriginalFunctionName));
			UEAS_TRACE_WRITE(ReflectedScriptFunctionNameBytes, Index, INDEX_NONE,
				INDEX_NONE,
				Writer.WriteString(Member.CanonicalScriptFunctionName));
			WriteStableReference(Writer, Member.Target);
		}
		UEAS_TRACE_SPAN(Reflection, ReflectionStart,
			static_cast<uint64>(Writer.Bytes.Num()) - ReflectionStart,
			INDEX_NONE, INDEX_NONE, INDEX_NONE);

		UEAS_TRACE_WRITE(Dependencies, INDEX_NONE, INDEX_NONE, INDEX_NONE,
			Writer.WriteUInt32(static_cast<uint32>(Value.Dependencies.Num())));
		for (int32 Index = 0; Index < Value.Dependencies.Num(); ++Index)
		{
			const uint64 RowStart = static_cast<uint64>(Writer.Bytes.Num());
			WriteDependency(Writer, Value.Dependencies[Index]);
			UEAS_TRACE_SPAN(Dependency, RowStart,
				static_cast<uint64>(Writer.Bytes.Num()) - RowStart,
				Index, INDEX_NONE, INDEX_NONE);
		}

		OutPayload = MoveTemp(Writer.Bytes);
#undef UEAS_TRACE_WRITE
#undef UEAS_TRACE_SPAN
		return {};
	}
}

using namespace AngelscriptCacheTypeSchemaProducer_Private;

bool FAngelscriptCacheV1BuildLayoutConstants::TryGetPrimitiveStorageLayout(
	const EAngelscriptCachedPrimitiveType Primitive,
	FAngelscriptCacheV1StorageLayout& OutLayout) const
{
	OutLayout = {};
	switch (Primitive)
	{
	case EAngelscriptCachedPrimitiveType::Bool:
		OutLayout = {AsSizeOfBool, 1}; return true;
	case EAngelscriptCachedPrimitiveType::Int8:
	case EAngelscriptCachedPrimitiveType::UInt8:
		OutLayout = {1, 1}; return true;
	case EAngelscriptCachedPrimitiveType::Int16:
	case EAngelscriptCachedPrimitiveType::UInt16:
		OutLayout = {2, 2}; return true;
	case EAngelscriptCachedPrimitiveType::Int32:
	case EAngelscriptCachedPrimitiveType::UInt32:
	case EAngelscriptCachedPrimitiveType::Float32:
		OutLayout = {4, 4}; return true;
	case EAngelscriptCachedPrimitiveType::Int64:
	case EAngelscriptCachedPrimitiveType::UInt64:
		OutLayout = {8, Int64Alignment}; return true;
	case EAngelscriptCachedPrimitiveType::Float64:
		OutLayout = {8, DoubleAlignment}; return true;
	default:
		return false;
	}
}

FAngelscriptCacheV1StorageLayout
FAngelscriptCacheV1BuildLayoutConstants::GetObjectHandleStorageLayout() const
{
	return {PointerByteWidth, ObjectHandleAlignment};
}

int32 FAngelscriptCacheTypeSchemaArchive::CompareDependencies(
	const FAngelscriptCacheSemanticDependency& A,
	const FAngelscriptCacheSemanticDependency& B)
{
	if (A.Kind != B.Kind)
	{
		return static_cast<uint8>(A.Kind) < static_cast<uint8>(B.Kind) ? -1 : 1;
	}
	if (A.Target.Kind != B.Target.Kind)
	{
		return static_cast<uint8>(A.Target.Kind) < static_cast<uint8>(B.Target.Kind) ? -1 : 1;
	}
	if (const int32 Result = CompareHash(A.Target.StableKey, B.Target.StableKey); Result != 0)
	{
		return Result;
	}
	if (const int32 Result = CompareHash(A.Target.ExpectedAbi, B.Target.ExpectedAbi); Result != 0)
	{
		return Result;
	}
	if (A.ExpectedContentOrValue.IsSet() != B.ExpectedContentOrValue.IsSet())
	{
		return A.ExpectedContentOrValue.IsSet() ? 1 : -1;
	}
	return A.ExpectedContentOrValue.IsSet()
		? CompareHash(A.ExpectedContentOrValue.GetValue(),
			B.ExpectedContentOrValue.GetValue())
		: 0;
}

int32 FAngelscriptCacheTypeSchemaArchive::CompareMetadata(
	const FAngelscriptCachedMetadataEntry& A,
	const FAngelscriptCachedMetadataEntry& B)
{
	if (const int32 Result = CompareString(A.CanonicalKey, B.CanonicalKey); Result != 0)
	{
		return Result;
	}
	return CompareString(A.CanonicalValue, B.CanonicalValue);
}

const FAngelscriptCacheV1BuildLayoutConstants&
FAngelscriptCacheTypeSchemaArchive::GetV1BuildLayoutConstants()
{
	static const FAngelscriptCacheV1BuildLayoutConstants Constants = {
		static_cast<uint32>(AS_SIZEOF_BOOL),
		static_cast<uint32>(4 * AS_PTR_SIZE),
		static_cast<uint32>(alignof(asINT64)),
		static_cast<uint32>(alignof(double)),
		8,
		8,
		4,
	};
	return Constants;
}

FAngelscriptCacheValidationResult
FAngelscriptCacheTypeSchemaArchive::ComputeLayoutInputHash(
	const FAngelscriptCachedTypeLayoutInput& Value,
	FAngelscriptHash256& OutHash)
{
	OutHash = {};
	if (static_cast<uint8>(Value.InputKind) < 1
		|| static_cast<uint8>(Value.InputKind) > 3)
	{
		return Failure(EAngelscriptCacheValidationError::UnknownEnumValue);
	}
	if (const FAngelscriptCacheValidationResult Result = ValidateStableReference(Value.Target);
		!Result.IsSuccess())
	{
		return Result;
	}
	FAngelscriptArtifactCanonicalWriter Writer(TEXT("cache-type-layout-input-v1"));
	Writer.WriteUInt8(static_cast<uint8>(Value.InputKind));
	WriteHashReference(Writer, Value.Target);
	WriteHashOptionalUInt32(Writer, Value.BoundaryContribution);
	WriteHashOptionalUInt32(Writer, Value.AlignmentContribution);
	OutHash = Writer.FinalizeHash();
	return {};
}

FAngelscriptCacheValidationResult
FAngelscriptCacheTypeSchemaArchive::ComputeStorageLayoutHash(
	const FAngelscriptCachedDataType& DataType,
	const EAngelscriptCachedPropertyStorageKind StorageKind,
	const uint32 SemanticStorageSize,
	const uint32 SemanticStorageAlignment,
	FAngelscriptHash256& OutHash)
{
	OutHash = {};
	if (const FAngelscriptCacheValidationResult Result = ValidateDataType(DataType);
		!Result.IsSuccess())
	{
		return Result;
	}
	if (static_cast<uint8>(StorageKind) < 1 || static_cast<uint8>(StorageKind) > 2)
	{
		return Failure(EAngelscriptCacheValidationError::UnknownEnumValue);
	}
	FAngelscriptArtifactCanonicalWriter Writer(
		TEXT("cache-data-type-storage-layout-v1"));
	WriteHashDataType(Writer, DataType);
	Writer.WriteUInt8(static_cast<uint8>(StorageKind));
	Writer.WriteUInt32(SemanticStorageSize);
	Writer.WriteUInt32(SemanticStorageAlignment);
	OutHash = Writer.FinalizeHash();
	return {};
}

FAngelscriptCacheValidationResult
FAngelscriptCacheTypeSchemaArchive::ComputePropertyLayoutFingerprint(
	const FAngelscriptStableTypeKey& OwnerTypeKey,
	const FAngelscriptCachedPropertySchema& Property,
	FAngelscriptHash256& OutHash)
{
	OutHash = {};
	if (OwnerTypeKey.Hash.IsZero() || Property.PropertyKey.Hash.IsZero())
	{
		return Failure(EAngelscriptCacheValidationError::ZeroStableKey);
	}
	if (const FAngelscriptCacheValidationResult Result =
		ValidateString(Property.CanonicalName, true); !Result.IsSuccess())
	{
		return Result;
	}
	if (const FAngelscriptCacheValidationResult Result = ValidateDataType(Property.Type);
		!Result.IsSuccess())
	{
		return Result;
	}
	TArray<FAngelscriptCachedMetadataEntry> CanonicalMetadata = Property.Metadata;
	CanonicalizeMetadataArray(CanonicalMetadata);
	if (const FAngelscriptCacheValidationResult Result =
		ValidateMetadataArray(CanonicalMetadata); !Result.IsSuccess())
	{
		return Result;
	}
	FAngelscriptArtifactCanonicalWriter Writer(TEXT("cache-property-layout-v1"));
	WriteHashKey(Writer, OwnerTypeKey);
	WriteHashKey(Writer, Property.PropertyKey);
	Writer.WriteString(Property.CanonicalName);
	WriteHashDataType(Writer, Property.Type);
	Writer.WriteUInt8(static_cast<uint8>(Property.StorageKind));
	Writer.WriteUInt32(Property.SemanticStorageSize);
	Writer.WriteUInt32(Property.SemanticStorageAlignment);
	Writer.WriteHash(Property.StorageLayoutHash);
	Writer.WriteUInt8(static_cast<uint8>(Property.Access));
	Writer.WriteUInt32(Property.LayoutOrdinal);
	Writer.WriteUInt32(Property.SemanticByteOffset);
	Writer.WriteUInt32(Property.PropertySemanticFlags);
	Writer.WriteUInt8(static_cast<uint8>(Property.ReplicationCondition));
	WriteHashMetadata(Writer, Property.Metadata);
	OutHash = Writer.FinalizeHash();
	return {};
}

FAngelscriptCacheValidationResult
FAngelscriptCacheTypeSchemaArchive::ComputeEnumAuthorityHash(
	const FAngelscriptStableTypeKey& OwnerTypeKey,
	const FAngelscriptCachedEnumTypePayload& EnumPayload,
	FAngelscriptHash256& OutHash)
{
	OutHash = {};
	if (OwnerTypeKey.Hash.IsZero())
	{
		return Failure(EAngelscriptCacheValidationError::ZeroStableKey);
	}
	FAngelscriptArtifactCanonicalWriter Writer(TEXT("cache-enum-authority-v1"));
	WriteHashKey(Writer, OwnerTypeKey);
	Writer.WriteUInt32(static_cast<uint32>(EnumPayload.OrderedEnumerators.Num()));
	for (const FAngelscriptCachedEnumEnumerator& Enumerator :
		EnumPayload.OrderedEnumerators)
	{
		if (const FAngelscriptCacheValidationResult Result =
			ValidateString(Enumerator.CanonicalName, true); !Result.IsSuccess())
		{
			return Result;
		}
		TArray<FAngelscriptCachedMetadataEntry> CanonicalMetadata = Enumerator.Metadata;
		CanonicalizeMetadataArray(CanonicalMetadata);
		if (const FAngelscriptCacheValidationResult Result =
			ValidateMetadataArray(CanonicalMetadata); !Result.IsSuccess())
		{
			return Result;
		}
		Writer.WriteUInt32(Enumerator.DeclarationOrdinal);
		Writer.WriteString(Enumerator.CanonicalName);
		Writer.WriteUInt32(static_cast<uint32>(Enumerator.Value));
		WriteHashMetadata(Writer, Enumerator.Metadata);
	}
	OutHash = Writer.FinalizeHash();
	return {};
}

FAngelscriptCacheValidationResult
FAngelscriptCacheTypeSchemaArchive::ComputeTypeLayoutHash(
	const FAngelscriptCachedTypeSchema& Value,
	FAngelscriptHash256& OutHash)
{
	OutHash = {};
	if (Value.TypeKey.Hash.IsZero()
		|| static_cast<uint8>(Value.TypeKind) < 1
		|| static_cast<uint8>(Value.TypeKind) > 7)
	{
		return Failure(Value.TypeKey.Hash.IsZero()
			? EAngelscriptCacheValidationError::ZeroStableKey
			: EAngelscriptCacheValidationError::UnknownEnumValue);
	}
	FAngelscriptArtifactCanonicalWriter Writer(TEXT("cache-type-layout-v1"));
	WriteHashKey(Writer, Value.TypeKey);
	Writer.WriteUInt8(static_cast<uint8>(Value.TypeKind));
	TArray<FAngelscriptCachedTypeRelation> CanonicalRelations = Value.Relations;
	CanonicalRelations.StableSort([](
		const FAngelscriptCachedTypeRelation& A,
		const FAngelscriptCachedTypeRelation& B)
	{
		return static_cast<uint8>(A.RelationKind)
			< static_cast<uint8>(B.RelationKind);
	});
	Writer.WriteUInt32(static_cast<uint32>(CanonicalRelations.Num()));
	for (const FAngelscriptCachedTypeRelation& Relation : CanonicalRelations)
	{
		Writer.WriteUInt8(static_cast<uint8>(Relation.RelationKind));
		WriteHashOptionalUInt32(Writer, Relation.SemanticOrdinal);
		WriteHashReference(Writer, Relation.Target);
	}
	TArray<FAngelscriptCachedTypeLayoutInput> CanonicalLayoutInputs = Value.LayoutInputs;
	CanonicalLayoutInputs.StableSort([](
		const FAngelscriptCachedTypeLayoutInput& A,
		const FAngelscriptCachedTypeLayoutInput& B)
	{
		return static_cast<uint8>(A.InputKind)
			< static_cast<uint8>(B.InputKind);
	});
	Writer.WriteUInt32(static_cast<uint32>(CanonicalLayoutInputs.Num()));
	for (const FAngelscriptCachedTypeLayoutInput& Input : CanonicalLayoutInputs)
	{
		Writer.WriteUInt8(static_cast<uint8>(Input.InputKind));
		WriteHashReference(Writer, Input.Target);
		WriteHashOptionalUInt32(Writer, Input.BoundaryContribution);
		WriteHashOptionalUInt32(Writer, Input.AlignmentContribution);
		Writer.WriteHash(Input.LayoutInputHash);
	}
	Writer.WriteUInt64(Value.Layout.SemanticSize);
	Writer.WriteUInt32(Value.Layout.SemanticAlignment);
	Writer.WriteUInt32(Value.Layout.BasePropertyBoundary);
	Writer.WriteUInt32(static_cast<uint32>(Value.OrderedProperties.Num()));
	for (const FAngelscriptCachedPropertySchema& Property : Value.OrderedProperties)
	{
		Writer.WriteHash(Property.PropertyLayoutFingerprint);
	}
	Writer.WriteUInt32(static_cast<uint32>(Value.OrderedMethods.Num()));
	for (const FAngelscriptCachedMethodEntry& Method : Value.OrderedMethods)
	{
		Writer.WriteUInt8(static_cast<uint8>(Method.EntryKind));
		Writer.WriteUInt32(Method.MethodOrdinal);
		WriteHashKey(Writer, Method.FunctionKey);
		WriteHashKey(Writer, Method.DeclaringOwner);
		Writer.WriteHash(Method.ExpectedDeclarationAbi);
	}
	Writer.WriteUInt32(static_cast<uint32>(Value.VirtualFunctionTable.Num()));
	for (const FAngelscriptCachedVirtualFunctionSlot& Slot : Value.VirtualFunctionTable)
	{
		Writer.WriteUInt8(static_cast<uint8>(Slot.SlotKind));
		Writer.WriteUInt32(Slot.VftOrdinal);
		WriteHashKey(Writer, Slot.FunctionKey);
		WriteHashKey(Writer, Slot.DeclaringOwner);
		WriteHashKey(Writer, Slot.ImplementingOwner);
		Writer.WriteHash(Slot.ExpectedDeclarationAbi);
	}
	Writer.WriteUInt32(static_cast<uint32>(Value.OrderedBehaviorSlots.Num()));
	for (const FAngelscriptCachedBehaviorSlot& Slot : Value.OrderedBehaviorSlots)
	{
		Writer.WriteUInt8(static_cast<uint8>(Slot.BehaviorKind));
		Writer.WriteUInt32(Slot.SlotOrdinal);
		WriteHashReference(Writer, Slot.Target);
		Writer.WriteBool(Slot.DeclaringOwner.IsSet());
		if (Slot.DeclaringOwner.IsSet())
		{
			WriteHashKey(Writer, Slot.DeclaringOwner.GetValue());
		}
	}
	Writer.WriteUInt32(Value.TypeSemanticFlags);
	Writer.WriteUInt8(static_cast<uint8>(Value.Reflection.ReflectionKind));
	Writer.WriteUInt32(Value.Reflection.ClassReflectionFlags);
	WriteHashOptionalString(Writer, Value.Reflection.ConfigName);
	WriteHashOptionalString(Writer, Value.Reflection.StaticClassGlobalName);
	Writer.WriteUInt32(static_cast<uint32>(
		Value.Reflection.OrderedUFunctionMembers.Num()));
	for (const FAngelscriptCachedReflectedFunctionMember& Member :
		Value.Reflection.OrderedUFunctionMembers)
	{
		Writer.WriteUInt32(Member.ReflectionOrdinal);
		Writer.WriteString(Member.CanonicalFunctionName);
		Writer.WriteString(Member.CanonicalOriginalFunctionName);
		Writer.WriteString(Member.CanonicalScriptFunctionName);
		WriteHashReference(Writer, Member.Target);
	}
	OutHash = Writer.FinalizeHash();
	return {};
}

FAngelscriptCacheValidationResult
FAngelscriptCacheTypeSchemaArchive::SerializeTypeSchema(
	const FAngelscriptCachedTypeSchema& Value,
	TArray<uint8>& OutPayload)
{
	FAngelscriptTypeSchemaFieldCoordinate IgnoredFailureCoordinate;
	return SerializeTypeSchemaWithDiagnostics(
		Value, OutPayload, IgnoredFailureCoordinate);
}

FAngelscriptCacheValidationResult
FAngelscriptCacheTypeSchemaArchive::SerializeTypeSchemaWithDiagnostics(
	const FAngelscriptCachedTypeSchema& Value,
	TArray<uint8>& OutPayload,
	FAngelscriptTypeSchemaFieldCoordinate& OutFailureCoordinate)
{
	OutPayload.Reset();
	OutFailureCoordinate = {};
	FAngelscriptCachedTypeSchema Canonical = Value;
	CanonicalizeSetFields(Canonical);
	if (const FAngelscriptCacheValidationResult Result = ValidateProducerShape(
		Canonical, &OutFailureCoordinate);
		!Result.IsSuccess())
	{
		return Result;
	}
	return WriteTypeSchemaPayload(Canonical, OutPayload
#if WITH_ANGELSCRIPT_UNITTESTS
		, nullptr
#endif
		);
}

#if WITH_ANGELSCRIPT_UNITTESTS
FAngelscriptCacheValidationResult
FAngelscriptCacheTypeSchemaArchive::SerializeTypeSchemaPhysicalForTests(
	const FAngelscriptCachedTypeSchema& Value,
	TArray<uint8>& OutPayload,
	FAngelscriptCacheTypeSchemaTestWireTrace& OutTrace)
{
	const FAngelscriptCacheValidationResult Result =
		WriteTypeSchemaPayload(Value, OutPayload, nullptr);
	if (!Result.IsSuccess())
	{
		OutTrace.Spans.Reset();
		return Result;
	}
	if (!FPhysicalTraceScanner(OutPayload, OutTrace).Scan())
	{
		OutPayload.Reset();
		OutTrace.Spans.Reset();
		return Failure(EAngelscriptCacheValidationError::OutOfBounds);
	}
	return {};
}

TOptional<FAngelscriptCacheTypeSchemaTestWireSpan>
FAngelscriptCacheTypeSchemaTestWireTrace::FindUnique(
	const EAngelscriptCacheTypeSchemaTestField Field,
	const int32 PrimaryIndex,
	const int32 SecondaryIndex,
	const int32 TertiaryIndex) const
{
	TOptional<FAngelscriptCacheTypeSchemaTestWireSpan> Found;
	for (const FAngelscriptCacheTypeSchemaTestWireSpan& Span : Spans)
	{
		if (Span.Field != Field || Span.PrimaryIndex != PrimaryIndex
			|| Span.SecondaryIndex != SecondaryIndex
			|| Span.TertiaryIndex != TertiaryIndex)
		{
			continue;
		}
		if (Found.IsSet())
		{
			return {};
		}
		Found = Span;
	}
	return Found;
}
#endif
