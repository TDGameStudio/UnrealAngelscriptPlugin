#include "Cache/AngelscriptCacheDiagnostics.h"

#include "AngelscriptEngine.h"
#include "Cache/AngelscriptCacheDecodedRecord.h"
#include "Cache/AngelscriptCacheSettings.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Serialization/JsonWriter.h"

namespace AngelscriptCacheDiagnostics_Private
{
	using FJsonWriter =
		TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>;

	static const TCHAR* MutationPhaseName(
		const EAngelscriptCacheMutationPhase Value)
	{
		switch (Value)
		{
		case EAngelscriptCacheMutationPhase::InitializingAnyThread:
			return TEXT("InitializingAnyThread");
		case EAngelscriptCacheMutationPhase::RuntimeGameThread:
			return TEXT("RuntimeGameThread");
		case EAngelscriptCacheMutationPhase::ShuttingDown:
			return TEXT("ShuttingDown");
		default:
			return TEXT("Unknown");
		}
	}

	static const TCHAR* CompileKindName(
		const EAngelscriptCacheSuccessfulCompileKind Value)
	{
		switch (Value)
		{
		case EAngelscriptCacheSuccessfulCompileKind::Initial:
			return TEXT("Initial");
		case EAngelscriptCacheSuccessfulCompileKind::SoftReload:
			return TEXT("SoftReload");
		case EAngelscriptCacheSuccessfulCompileKind::FullReload:
			return TEXT("FullReload");
		default:
			return TEXT("Invalid");
		}
	}

	static const TCHAR* DispositionName(
		const EAngelscriptCachePublicationDisposition Value)
	{
		switch (Value)
		{
		case EAngelscriptCachePublicationDisposition::Current:
			return TEXT("Current");
		case EAngelscriptCachePublicationDisposition::PendingColdStart:
			return TEXT("PendingColdStart");
		default:
			return TEXT("Invalid");
		}
	}

	static const TCHAR* RecordKindName(const EAngelscriptCacheRecordKind Value)
	{
		switch (Value)
		{
		case EAngelscriptCacheRecordKind::SourceIndex:
			return TEXT("SourceIndex");
		case EAngelscriptCacheRecordKind::ModuleInterface:
			return TEXT("ModuleInterface");
		case EAngelscriptCacheRecordKind::TypeSchema:
			return TEXT("TypeSchema");
		case EAngelscriptCacheRecordKind::ModuleState:
			return TEXT("ModuleState");
		case EAngelscriptCacheRecordKind::FunctionBody:
			return TEXT("FunctionBody");
		case EAngelscriptCacheRecordKind::DebugSidecar:
			return TEXT("DebugSidecar");
		case EAngelscriptCacheRecordKind::ModuleSnapshot:
			return TEXT("ModuleSnapshot");
		default:
			return TEXT("Unknown");
		}
	}

	static const TCHAR* ValidationClassName(
		const EAngelscriptCacheValidationClass Value)
	{
		static constexpr const TCHAR* Names[] = {
			TEXT("Success"), TEXT("Malformed"), TEXT("ArithmeticOrBudget"),
			TEXT("CodecOrIntegrity"), TEXT("CanonicalSemantic"),
			TEXT("GraphOrOwnership"), TEXT("Ineligible"),
		};
		const uint8 Raw = static_cast<uint8>(Value);
		return Raw < UE_ARRAY_COUNT(Names) ? Names[Raw] : TEXT("Unknown");
	}

	static const TCHAR* ValidationStageName(
		const EAngelscriptCacheValidationStage Value)
	{
		static constexpr const TCHAR* Names[] = {
			TEXT("None"), TEXT("EnvelopeDecode"), TEXT("PayloadDecode"),
			TEXT("LocalSemantic"), TEXT("OpaqueCodec"), TEXT("ModuleGraph"),
			TEXT("CurrentResolver"), TEXT("PackDecode"), TEXT("ManifestDecode"),
			TEXT("ManifestGraph"),
		};
		const uint8 Raw = static_cast<uint8>(Value);
		return Raw < UE_ARRAY_COUNT(Names) ? Names[Raw] : TEXT("Unknown");
	}

	static const TCHAR* ValidationErrorName(
		const EAngelscriptCacheValidationError Value)
	{
		static constexpr const TCHAR* Names[] = {
			TEXT("None"), TEXT("BadMagic"), TEXT("UnsupportedSchema"),
			TEXT("UnknownRecordKind"), TEXT("NonZeroReserved"),
			TEXT("Overflow"), TEXT("BudgetExceeded"), TEXT("OutOfBounds"),
			TEXT("ChecksumMismatch"), TEXT("TrailingData"),
			TEXT("InvalidArrayView"), TEXT("AliasedInputOutput"),
			TEXT("UnsupportedPayloadSchema"), TEXT("UnknownEnumValue"),
			TEXT("UnknownFlags"), TEXT("InvalidBoolean"),
			TEXT("InvalidOptionalTag"), TEXT("InvalidUtf8"),
			TEXT("EmbeddedNul"), TEXT("InvalidLogicalPath"),
			TEXT("ImpossibleCount"), TEXT("NestingDepthExceeded"),
			TEXT("RecordIdMismatch"), TEXT("NonCanonicalOrder"),
			TEXT("DuplicateKey"), TEXT("ConflictingKey"), TEXT("CaseCollision"),
			TEXT("ZeroStableKey"), TEXT("MissingExpectedAbi"),
			TEXT("ForbiddenExpectedAbi"), TEXT("InvalidPresence"),
			TEXT("InvalidQualifierCombination"), TEXT("OrdinalGap"),
			TEXT("DuplicateOrdinal"), TEXT("DerivedHashMismatch"),
			TEXT("MissingOwner"), TEXT("CrossModuleOwner"),
			TEXT("MissingGraphTarget"), TEXT("WrongReferenceKind"),
			TEXT("CompatibilityMismatch"), TEXT("ContextMismatch"),
			TEXT("ProfileMismatch"), TEXT("SourceSnapshotMismatch"),
			TEXT("CurrentAbiMismatch"), TEXT("UnsupportedCodecVersion"),
			TEXT("OpaquePayloadMalformed"), TEXT("OpaquePayloadHashMismatch"),
			TEXT("RelocationDependencyMismatch"), TEXT("WrongRecordKind"),
			TEXT("MissingRecord"), TEXT("MissingCoverage"),
			TEXT("UnexpectedRecord"), TEXT("UndeclaredEntity"),
			TEXT("DuplicateDebugOwner"), TEXT("DebugLinkMismatch"),
			TEXT("EnumAuthorityMismatch"), TEXT("InitializerOwnershipMismatch"),
			TEXT("GlobalCoverageMismatch"), TEXT("ProfileGraphMismatch"),
			TEXT("SourceGraphMismatch"), TEXT("GraphAbiMismatch"),
			TEXT("InvocationKindMismatch"), TEXT("DebugSourceMismatch"),
			TEXT("CurrentContentMismatch"), TEXT("CurrentSymbolMissing"),
			TEXT("UnsupportedStorageCodec"), TEXT("DecompressionFailed"),
			TEXT("DecompressedSizeMismatch"), TEXT("PackIdMismatch"),
			TEXT("GenerationIdMismatch"), TEXT("OverlappingRange"),
			TEXT("PackIndexMismatch"),
		};
		const uint8 Raw = static_cast<uint8>(Value);
		return Raw < UE_ARRAY_COUNT(Names) ? Names[Raw] : TEXT("Unknown");
	}

	static const TCHAR* ReferenceKindName(
		const EAngelscriptCacheReferenceKind Value)
	{
		static constexpr const TCHAR* Names[] = {
			TEXT("Invalid"), TEXT("ScriptModule"), TEXT("ScriptType"),
			TEXT("ScriptFunction"), TEXT("ScriptGlobal"),
			TEXT("ScriptProperty"), TEXT("ScriptImport"),
			TEXT("EnvironmentSymbol"), TEXT("CanonicalName"),
			TEXT("StringLiteral"),
		};
		const uint8 Raw = static_cast<uint8>(Value);
		return Raw < UE_ARRAY_COUNT(Names) ? Names[Raw] : TEXT("Unknown");
	}

	static const TCHAR* DependencyKindName(
		const EAngelscriptCacheSemanticDependencyKind Value)
	{
		static constexpr const TCHAR* Names[] = {
			TEXT("Invalid"), TEXT("Import"), TEXT("Declaration"),
			TEXT("Signature"), TEXT("Inheritance"), TEXT("ValueLayout"),
			TEXT("PropertyLayout"), TEXT("GlobalStorage"), TEXT("HardValue"),
			TEXT("Initializer"), TEXT("CompileOption"),
			TEXT("EnvironmentAbi"), TEXT("FunctionContent"),
		};
		const uint8 Raw = static_cast<uint8>(Value);
		return Raw < UE_ARRAY_COUNT(Names) ? Names[Raw] : TEXT("Unknown");
	}

	static const TCHAR* InvocationKindName(
		const EAngelscriptCachedFunctionInvocationKind Value)
	{
		static constexpr const TCHAR* Names[] = {
			TEXT("Invalid"), TEXT("GlobalFunction"), TEXT("Method"),
			TEXT("Constructor"), TEXT("Destructor"), TEXT("Factory"),
			TEXT("GeneratedDefaultConstructor"),
			TEXT("GeneratedDefaultDestructor"), TEXT("InitDefaults"),
			TEXT("PublicSingleFunction"), TEXT("Lambda"),
		};
		const uint8 Raw = static_cast<uint8>(Value);
		return Raw < UE_ARRAY_COUNT(Names) ? Names[Raw] : TEXT("Unknown");
	}

	static const TCHAR* TypeKindName(const EAngelscriptCachedTypeKind Value)
	{
		static constexpr const TCHAR* Names[] = {
			TEXT("Invalid"), TEXT("Class"), TEXT("Struct"),
			TEXT("Interface"), TEXT("Enum"), TEXT("Delegate"),
			TEXT("Typedef"), TEXT("Funcdef"),
		};
		const uint8 Raw = static_cast<uint8>(Value);
		return Raw < UE_ARRAY_COUNT(Names) ? Names[Raw] : TEXT("Unknown");
	}

	static const TCHAR* RouteName(
		const EAngelscriptCacheFunctionExecutionRoute Value)
	{
		switch (Value)
		{
		case EAngelscriptCacheFunctionExecutionRoute::Vm:
			return TEXT("Vm");
		case EAngelscriptCacheFunctionExecutionRoute::Native:
			return TEXT("Native");
		default:
			return TEXT("Unknown");
		}
	}

	static const TCHAR* DecisionStageName(
		const EAngelscriptCacheDecisionStage Value)
	{
		switch (Value)
		{
		case EAngelscriptCacheDecisionStage::StartupSelection:
			return TEXT("StartupSelection");
		case EAngelscriptCacheDecisionStage::StartupRestore:
			return TEXT("StartupRestore");
		case EAngelscriptCacheDecisionStage::FunctionLookup:
			return TEXT("FunctionLookup");
		case EAngelscriptCacheDecisionStage::DependencyPropagation:
			return TEXT("DependencyPropagation");
		case EAngelscriptCacheDecisionStage::SuccessfulPublication:
			return TEXT("SuccessfulPublication");
		case EAngelscriptCacheDecisionStage::LifecycleFlush:
			return TEXT("LifecycleFlush");
		case EAngelscriptCacheDecisionStage::StableRoute:
			return TEXT("StableRoute");
		case EAngelscriptCacheDecisionStage::RuntimeReload:
			return TEXT("RuntimeReload");
		default:
			return TEXT("Invalid");
		}
	}

	static const TCHAR* DecisionOutcomeName(
		const EAngelscriptCacheDecisionOutcome Value)
	{
		switch (Value)
		{
		case EAngelscriptCacheDecisionOutcome::Restored:
			return TEXT("Restored");
		case EAngelscriptCacheDecisionOutcome::Miss:
			return TEXT("Miss");
		case EAngelscriptCacheDecisionOutcome::Rejected:
			return TEXT("Rejected");
		case EAngelscriptCacheDecisionOutcome::NotCacheable:
			return TEXT("NotCacheable");
		case EAngelscriptCacheDecisionOutcome::Compiled:
			return TEXT("Compiled");
		case EAngelscriptCacheDecisionOutcome::Reused:
			return TEXT("Reused");
		case EAngelscriptCacheDecisionOutcome::Published:
			return TEXT("Published");
		case EAngelscriptCacheDecisionOutcome::Deferred:
			return TEXT("Deferred");
		case EAngelscriptCacheDecisionOutcome::RolledBack:
			return TEXT("RolledBack");
		case EAngelscriptCacheDecisionOutcome::Completed:
			return TEXT("Completed");
		default:
			return TEXT("Invalid");
		}
	}

	static const TCHAR* DecisionReasonDomainName(
		const EAngelscriptCacheDecisionReasonDomain Value)
	{
		switch (Value)
		{
		case EAngelscriptCacheDecisionReasonDomain::None:
			return TEXT("None");
		case EAngelscriptCacheDecisionReasonDomain::ExactStartup:
			return TEXT("ExactStartup");
		case EAngelscriptCacheDecisionReasonDomain::Validation:
			return TEXT("Validation");
		case EAngelscriptCacheDecisionReasonDomain::FunctionLookup:
			return TEXT("FunctionLookup");
		case EAngelscriptCacheDecisionReasonDomain::DependencyMiss:
			return TEXT("DependencyMiss");
		case EAngelscriptCacheDecisionReasonDomain::FreezePublication:
			return TEXT("FreezePublication");
		case EAngelscriptCacheDecisionReasonDomain::LifecycleFlush:
			return TEXT("LifecycleFlush");
		case EAngelscriptCacheDecisionReasonDomain::Store:
			return TEXT("Store");
		case EAngelscriptCacheDecisionReasonDomain::StableRoute:
			return TEXT("StableRoute");
		case EAngelscriptCacheDecisionReasonDomain::RuntimeReload:
			return TEXT("RuntimeReload");
		case EAngelscriptCacheDecisionReasonDomain::CleanCapture:
			return TEXT("CleanCapture");
		default:
			return TEXT("Unknown");
		}
	}

	static FString UInt64String(const uint64 Value)
	{
		return FString::Printf(TEXT("%llu"), Value);
	}

	static FAngelscriptCacheDiagnosticPublicationSummary BuildPublication(
		const TSharedPtr<const FAngelscriptCacheSuccessfulPublicationDto,
			ESPMode::ThreadSafe>& Publication)
	{
		FAngelscriptCacheDiagnosticPublicationSummary Summary;
		if (!Publication.IsValid())
		{
			return Summary;
		}

		Summary.bPresent = true;
		Summary.PublicationSchemaVersion = Publication->SchemaVersion;
		Summary.TransactionOrdinal = Publication->TransactionOrdinal;
		Summary.CompileKind = Publication->Kind;
		Summary.Disposition = Publication->Disposition;
		Summary.Compatibility = Publication->Compatibility;
		Summary.Context = Publication->Context;
		Summary.Profile = Publication->Profile;
		Summary.SourceSnapshot = Publication->SourceSnapshot;
		Summary.SourceIndexRecordId = Publication->SourceIndexRecordId;
		Summary.bRestoredFromStore = Publication->bRestoredFromStore;
		Summary.PersistedGenerationId = Publication->PersistedGenerationId;
		Summary.Modules.Reserve(Publication->Modules.Num());
		for (const FAngelscriptCacheCleanModuleArtifacts& Module
			: Publication->Modules)
		{
			FAngelscriptCacheDiagnosticModuleSummary& ModuleSummary =
				Summary.Modules.AddDefaulted_GetRef();
			ModuleSummary.ModuleKey = Module.ModuleKey;
			ModuleSummary.CanonicalModuleName = Module.CanonicalModuleName;
			ModuleSummary.ModuleSnapshotRecordId =
				Module.ModuleSnapshot.RecordId;
			ModuleSummary.TotalRecordCount = Module.Records.Num();
			ModuleSummary.RecordKinds.Reserve(7);
			for (uint8 RawKind = static_cast<uint8>(
					EAngelscriptCacheRecordKind::SourceIndex);
				RawKind <= static_cast<uint8>(
					EAngelscriptCacheRecordKind::ModuleSnapshot);
				++RawKind)
			{
				FAngelscriptCacheDiagnosticRecordKindSummary& KindSummary =
					ModuleSummary.RecordKinds.AddDefaulted_GetRef();
				KindSummary.Kind =
					static_cast<EAngelscriptCacheRecordKind>(RawKind);
			}

			for (const FAngelscriptPreparedRecord& Record : Module.Records)
			{
				const uint64 Bytes = static_cast<uint64>(
					Record.CanonicalPayload.Num());
				ModuleSummary.CanonicalPayloadBytes += Bytes;
				const uint8 RawKind = static_cast<uint8>(Record.RecordId.Kind);
				const uint8 FirstKind = static_cast<uint8>(
					EAngelscriptCacheRecordKind::SourceIndex);
				if (RawKind >= FirstKind
					&& RawKind <= static_cast<uint8>(
						EAngelscriptCacheRecordKind::ModuleSnapshot))
				{
					FAngelscriptCacheDiagnosticRecordKindSummary& KindSummary =
						ModuleSummary.RecordKinds[RawKind - FirstKind];
					++KindSummary.RecordCount;
					KindSummary.CanonicalPayloadBytes += Bytes;
				}

				FAngelscriptCacheReadLimits DiagnosticLimits;
				FAngelscriptCacheReadBudget DiagnosticBudget;
				TOptional<FAngelscriptDecodedCacheRecordHandle> Decoded;
				const FAngelscriptCacheValidationResult Decode =
					FAngelscriptDecodedCacheRecord::TryDecode(
						Record.RecordId,
						Record.CanonicalPayload,
						DiagnosticLimits,
						DiagnosticBudget,
						Decoded);
				if (!Decode.IsSuccess() || !Decoded.IsSet())
				{
					FAngelscriptCacheDiagnosticDecodeFailure& Failure =
						ModuleSummary.DecodeFailures.AddDefaulted_GetRef();
					Failure.RecordId = Record.RecordId;
					Failure.Validation = Decode;
					continue;
				}

				const FAngelscriptDecodedCacheRecord& DecodedRecord =
					Decoded.GetValue().Get();
				if (const FAngelscriptCachedModuleInterface* Interface =
						DecodedRecord.TryGetModuleInterface())
				{
					ModuleSummary.CanonicalModuleName =
						Interface->CanonicalModuleName;
					ModuleSummary.InterfaceAbi = Interface->InterfaceAbi;
					ModuleSummary.Declarations = Interface->Declarations;
					ModuleSummary.Imports = Interface->Imports;
					ModuleSummary.InterfaceDependencies =
						Interface->Dependencies;
				}
				else if (const FAngelscriptCachedTypeSchema* Type =
						DecodedRecord.TryGetTypeSchema())
				{
					FAngelscriptCacheDiagnosticTypeSummary& Row =
						ModuleSummary.Types.AddDefaulted_GetRef();
					Row.RecordId = Record.RecordId;
					Row.Schema = *Type;
				}
				else if (const FAngelscriptCachedModuleState* State =
						DecodedRecord.TryGetModuleState())
				{
					ModuleSummary.StateProfile = State->Profile;
					ModuleSummary.StateInputHash = State->StateInputHash;
					ModuleSummary.Globals = State->OrderedGlobals;
					ModuleSummary.StateDependencies = State->Dependencies;
					ModuleSummary.Initializers.Reserve(
						State->Initializers.Num());
					for (const FAngelscriptCachedInitializerUnit& Initializer
						: State->Initializers)
					{
						FAngelscriptCacheDiagnosticInitializerSummary& Row =
							ModuleSummary.Initializers.AddDefaulted_GetRef();
						Row.InitializerKind = Initializer.InitializerKind;
						Row.InitializerKey = Initializer.InitializerKey;
						Row.OwnerGlobal = Initializer.OwnerGlobal;
						Row.VmInitializerCodecVersion =
							Initializer.VmInitializerCodecVersion;
						Row.InitializerExecutionHash =
							Initializer.InitializerExecutionHash;
						Row.CanonicalExecutionPayloadBytes =
							static_cast<uint64>(
								Initializer.CanonicalExecutionPayload.Num());
					}
				}
				else if (const FAngelscriptCachedFunctionBody* Function =
						DecodedRecord.TryGetFunctionBody())
				{
					FAngelscriptCacheDiagnosticFunctionSummary& Row =
						ModuleSummary.Functions.AddDefaulted_GetRef();
					Row.RecordId = Record.RecordId;
					Row.Identity = Function->Identity;
					Row.ExpectedDeclarationAbi =
						Function->ExpectedDeclarationAbi;
					Row.FunctionSourceDigest = Function->FunctionSourceDigest;
					Row.FunctionInputDigest = Function->FunctionInputDigest;
					Row.InvocationKind = Function->InvocationKind;
					Row.VmExecutionCodecVersion =
						Function->VmExecutionCodecVersion;
					Row.CanonicalExecutionPayloadBytes =
						static_cast<uint64>(
							Function->CanonicalExecutionPayload.Num());
					Row.ActualDependencies = Function->ActualDependencies;
					Row.DebugSidecar = Function->DebugSidecar;
				}
				else if (const FAngelscriptCachedModuleSnapshot* ModuleSnapshot =
						DecodedRecord.TryGetModuleSnapshot())
				{
					ModuleSummary.ModuleInterfaceRecordId =
						ModuleSnapshot->ModuleInterface.RecordId;
					ModuleSummary.ModuleStateRecordId =
						ModuleSnapshot->ModuleState.RecordId;
				}
			}
			ModuleSummary.Declarations.Sort([](
				const FAngelscriptCachedDeclaration& Left,
				const FAngelscriptCachedDeclaration& Right)
			{
				return Left.StableKey < Right.StableKey;
			});
			ModuleSummary.Types.Sort([](
				const FAngelscriptCacheDiagnosticTypeSummary& Left,
				const FAngelscriptCacheDiagnosticTypeSummary& Right)
			{
				return Left.Schema.TypeKey.Hash < Right.Schema.TypeKey.Hash;
			});
			ModuleSummary.Functions.Sort([](
				const FAngelscriptCacheDiagnosticFunctionSummary& Left,
				const FAngelscriptCacheDiagnosticFunctionSummary& Right)
			{
				return Left.Identity.FunctionKey.Hash
					< Right.Identity.FunctionKey.Hash;
			});
			ModuleSummary.Globals.Sort([](
				const FAngelscriptCachedGlobalSchema& Left,
				const FAngelscriptCachedGlobalSchema& Right)
			{
				return Left.GlobalKey.Hash < Right.GlobalKey.Hash;
			});
			ModuleSummary.Initializers.Sort([](
				const FAngelscriptCacheDiagnosticInitializerSummary& Left,
				const FAngelscriptCacheDiagnosticInitializerSummary& Right)
			{
				return Left.InitializerKey.Hash < Right.InitializerKey.Hash;
			});
			ModuleSummary.DecodeFailures.Sort([](
				const FAngelscriptCacheDiagnosticDecodeFailure& Left,
				const FAngelscriptCacheDiagnosticDecodeFailure& Right)
			{
				return Left.RecordId < Right.RecordId;
			});
			Summary.TotalRecordCount += ModuleSummary.TotalRecordCount;
			Summary.CanonicalPayloadBytes +=
				ModuleSummary.CanonicalPayloadBytes;
		}
		Summary.Modules.Sort([](
			const FAngelscriptCacheDiagnosticModuleSummary& Left,
			const FAngelscriptCacheDiagnosticModuleSummary& Right)
		{
			return Left.ModuleKey.Hash < Right.ModuleKey.Hash;
		});
		return Summary;
	}

	static void WriteRecordId(
		FJsonWriter& Writer,
		const TCHAR* FieldName,
		const FAngelscriptCacheRecordId& RecordId)
	{
		Writer.WriteObjectStart(FieldName);
		Writer.WriteValue(TEXT("kind"), static_cast<uint32>(RecordId.Kind));
		Writer.WriteValue(TEXT("kindName"), RecordKindName(RecordId.Kind));
		Writer.WriteValue(TEXT("contentHash"),
			RecordId.ContentHash.ToHexString());
		Writer.WriteObjectEnd();
	}

	static void WriteValidation(
		FJsonWriter& Writer,
		const TCHAR* FieldName,
		const FAngelscriptCacheValidationResult& Validation)
	{
		Writer.WriteObjectStart(FieldName);
		Writer.WriteValue(TEXT("error"),
			static_cast<uint32>(Validation.Error));
		Writer.WriteValue(TEXT("errorName"),
			ValidationErrorName(Validation.Error));
		Writer.WriteValue(TEXT("class"),
			static_cast<uint32>(Validation.Class));
		Writer.WriteValue(TEXT("className"),
			ValidationClassName(Validation.Class));
		Writer.WriteValue(TEXT("recordKind"),
			static_cast<uint32>(Validation.RecordKind));
		Writer.WriteValue(TEXT("recordKindName"),
			RecordKindName(Validation.RecordKind));
		Writer.WriteValue(TEXT("stage"),
			static_cast<uint32>(Validation.Stage));
		Writer.WriteValue(TEXT("stageName"),
			ValidationStageName(Validation.Stage));
		Writer.WriteValue(TEXT("byteOffset"),
			UInt64String(Validation.ByteOffset));
		Writer.WriteObjectEnd();
	}

	static void WriteStableReference(
		FJsonWriter& Writer,
		const TCHAR* FieldName,
		const FAngelscriptCacheStableReference& Reference)
	{
		Writer.WriteObjectStart(FieldName);
		Writer.WriteValue(TEXT("kind"),
			static_cast<uint32>(Reference.Kind));
		Writer.WriteValue(TEXT("kindName"), ReferenceKindName(Reference.Kind));
		Writer.WriteValue(TEXT("stableKey"),
			Reference.StableKey.ToHexString());
		Writer.WriteValue(TEXT("expectedAbi"),
			Reference.ExpectedAbi.ToHexString());
		Writer.WriteObjectEnd();
	}

	static void WriteDependency(
		FJsonWriter& Writer,
		const FAngelscriptCacheSemanticDependency& Dependency)
	{
		Writer.WriteObjectStart();
		Writer.WriteValue(TEXT("kind"), static_cast<uint32>(Dependency.Kind));
		Writer.WriteValue(TEXT("kindName"),
			DependencyKindName(Dependency.Kind));
		WriteStableReference(Writer, TEXT("target"), Dependency.Target);
		if (Dependency.ExpectedContentOrValue.IsSet())
		{
			Writer.WriteValue(TEXT("expectedContentOrValue"),
				Dependency.ExpectedContentOrValue->ToHexString());
		}
		Writer.WriteObjectEnd();
	}

	static void WriteDependencies(
		FJsonWriter& Writer,
		const TCHAR* FieldName,
		const TArray<FAngelscriptCacheSemanticDependency>& Dependencies)
	{
		Writer.WriteArrayStart(FieldName);
		for (const FAngelscriptCacheSemanticDependency& Dependency : Dependencies)
		{
			WriteDependency(Writer, Dependency);
		}
		Writer.WriteArrayEnd();
	}

	static void WriteDataType(
		FJsonWriter& Writer,
		const TCHAR* FieldName,
		const FAngelscriptCachedDataType& Type)
	{
		if (FieldName == nullptr)
		{
			Writer.WriteObjectStart();
		}
		else
		{
			Writer.WriteObjectStart(FieldName);
		}
		Writer.WriteValue(TEXT("kind"), static_cast<uint32>(Type.Kind));
		Writer.WriteValue(TEXT("primitive"),
			static_cast<uint32>(Type.Primitive));
		Writer.WriteValue(TEXT("qualifierFlags"), Type.QualifierFlags);
		if (Type.TypeReference.IsSet())
		{
			WriteStableReference(
				Writer, TEXT("typeReference"), Type.TypeReference.GetValue());
		}
		Writer.WriteArrayStart(TEXT("orderedSubTypes"));
		for (const FAngelscriptCachedDataType& SubType : Type.OrderedSubTypes)
		{
			WriteDataType(Writer, nullptr, SubType);
		}
		Writer.WriteArrayEnd();
		Writer.WriteObjectEnd();
	}

	static void WriteMetadata(
		FJsonWriter& Writer,
		const TCHAR* FieldName,
		const TArray<FAngelscriptCachedMetadataEntry>& Metadata)
	{
		Writer.WriteArrayStart(FieldName);
		for (const FAngelscriptCachedMetadataEntry& Entry : Metadata)
		{
			Writer.WriteObjectStart();
			Writer.WriteValue(TEXT("key"), Entry.CanonicalKey);
			Writer.WriteValue(TEXT("value"), Entry.CanonicalValue);
			Writer.WriteObjectEnd();
		}
		Writer.WriteArrayEnd();
	}

	static void WriteDeclaration(
		FJsonWriter& Writer,
		const FAngelscriptCachedDeclaration& Declaration)
	{
		Writer.WriteObjectStart();
		Writer.WriteValue(TEXT("declarationKind"),
			static_cast<uint32>(Declaration.DeclarationKind));
		Writer.WriteValue(TEXT("entityKind"),
			static_cast<uint32>(Declaration.EntityKind));
		Writer.WriteValue(TEXT("schemaCoverage"),
			static_cast<uint32>(Declaration.SchemaCoverage));
		Writer.WriteValue(TEXT("bodyCoverage"),
			static_cast<uint32>(Declaration.BodyCoverage));
		Writer.WriteValue(TEXT("stableKey"),
			Declaration.StableKey.ToHexString());
		Writer.WriteValue(TEXT("ownerKind"),
			static_cast<uint32>(Declaration.OwnerKind));
		Writer.WriteValue(TEXT("ownerKey"),
			Declaration.OwnerKey.ToHexString());
		Writer.WriteValue(TEXT("moduleKey"),
			Declaration.ModuleKey.Hash.ToHexString());
		Writer.WriteValue(TEXT("canonicalNamespace"),
			Declaration.CanonicalNamespace);
		Writer.WriteValue(TEXT("canonicalName"), Declaration.CanonicalName);
		Writer.WriteValue(TEXT("canonicalDeclaration"),
			Declaration.CanonicalDeclaration);
		Writer.WriteValue(TEXT("traitFlags"), Declaration.TraitFlags);
		Writer.WriteValue(TEXT("reflectionFlags"),
			Declaration.ReflectionFlags);
		Writer.WriteValue(TEXT("signatureHash"),
			Declaration.SignatureHash.ToHexString());
		Writer.WriteValue(TEXT("traitsHash"),
			Declaration.TraitsHash.ToHexString());
		if (Declaration.CanonicalTypeSpelling.IsSet())
		{
			Writer.WriteValue(TEXT("canonicalTypeSpelling"),
				Declaration.CanonicalTypeSpelling.GetValue());
		}
		if (Declaration.DeclaredType.IsSet())
		{
			WriteDataType(
				Writer, TEXT("declaredType"), Declaration.DeclaredType.GetValue());
		}
		Writer.WriteArrayStart(TEXT("identityTraits"));
		for (const FString& Trait : Declaration.CanonicalIdentityTraits)
		{
			Writer.WriteValue(Trait);
		}
		Writer.WriteArrayEnd();
		Writer.WriteArrayStart(TEXT("parameters"));
		for (const FAngelscriptCachedParameter& Parameter :
			Declaration.OrderedParameters)
		{
			Writer.WriteObjectStart();
			Writer.WriteValue(TEXT("ordinal"), Parameter.Ordinal);
			Writer.WriteValue(TEXT("canonicalName"), Parameter.CanonicalName);
			Writer.WriteValue(TEXT("passing"),
				static_cast<uint32>(Parameter.Passing));
			Writer.WriteValue(TEXT("traitFlags"), Parameter.TraitFlags);
			if (Parameter.CanonicalDefaultExpression.IsSet())
			{
				Writer.WriteValue(TEXT("canonicalDefaultExpression"),
					Parameter.CanonicalDefaultExpression.GetValue());
			}
			WriteDataType(Writer, TEXT("type"), Parameter.Type);
			Writer.WriteObjectEnd();
		}
		Writer.WriteArrayEnd();
		WriteMetadata(Writer, TEXT("metadata"), Declaration.Metadata);
		Writer.WriteArrayStart(TEXT("slots"));
		for (const FAngelscriptCachedDeclarationSlot& Slot : Declaration.Slots)
		{
			Writer.WriteObjectStart();
			Writer.WriteValue(TEXT("kind"), static_cast<uint32>(Slot.SlotKind));
			Writer.WriteValue(TEXT("ordinal"), Slot.Ordinal);
			Writer.WriteObjectEnd();
		}
		Writer.WriteArrayEnd();
		Writer.WriteObjectEnd();
	}

	static void WriteType(
		FJsonWriter& Writer,
		const FAngelscriptCacheDiagnosticTypeSummary& Summary)
	{
		const FAngelscriptCachedTypeSchema& Type = Summary.Schema;
		Writer.WriteObjectStart();
		WriteRecordId(Writer, TEXT("recordId"), Summary.RecordId);
		Writer.WriteValue(TEXT("payloadSchemaVersion"),
			Type.PayloadSchemaVersion);
		Writer.WriteValue(TEXT("moduleKey"), Type.ModuleKey.Hash.ToHexString());
		Writer.WriteValue(TEXT("typeKey"), Type.TypeKey.Hash.ToHexString());
		Writer.WriteValue(TEXT("typeKind"), static_cast<uint32>(Type.TypeKind));
		Writer.WriteValue(TEXT("typeKindName"), TypeKindName(Type.TypeKind));
		Writer.WriteValue(TEXT("canonicalNamespace"), Type.CanonicalNamespace);
		Writer.WriteValue(TEXT("canonicalName"), Type.CanonicalName);
		Writer.WriteValue(TEXT("canonicalDeclaration"),
			Type.CanonicalDeclaration);
		Writer.WriteValue(TEXT("typeSemanticFlags"), Type.TypeSemanticFlags);
		Writer.WriteValue(TEXT("semanticSize"),
			UInt64String(Type.Layout.SemanticSize));
		Writer.WriteValue(TEXT("semanticAlignment"),
			Type.Layout.SemanticAlignment);
		Writer.WriteValue(TEXT("basePropertyBoundary"),
			Type.Layout.BasePropertyBoundary);
		Writer.WriteValue(TEXT("typeLayoutHash"),
			Type.Layout.TypeLayoutHash.ToHexString());
		WriteMetadata(Writer, TEXT("metadata"), Type.Metadata);
		Writer.WriteArrayStart(TEXT("relations"));
		for (const FAngelscriptCachedTypeRelation& Relation : Type.Relations)
		{
			Writer.WriteObjectStart();
			Writer.WriteValue(TEXT("kind"),
				static_cast<uint32>(Relation.RelationKind));
			if (Relation.SemanticOrdinal.IsSet())
			{
				Writer.WriteValue(TEXT("semanticOrdinal"),
					Relation.SemanticOrdinal.GetValue());
			}
			WriteStableReference(Writer, TEXT("target"), Relation.Target);
			Writer.WriteObjectEnd();
		}
		Writer.WriteArrayEnd();
		Writer.WriteArrayStart(TEXT("layoutInputs"));
		for (const FAngelscriptCachedTypeLayoutInput& Input : Type.LayoutInputs)
		{
			Writer.WriteObjectStart();
			Writer.WriteValue(TEXT("kind"),
				static_cast<uint32>(Input.InputKind));
			WriteStableReference(Writer, TEXT("target"), Input.Target);
			if (Input.BoundaryContribution.IsSet())
			{
				Writer.WriteValue(TEXT("boundaryContribution"),
					Input.BoundaryContribution.GetValue());
			}
			if (Input.AlignmentContribution.IsSet())
			{
				Writer.WriteValue(TEXT("alignmentContribution"),
					Input.AlignmentContribution.GetValue());
			}
			Writer.WriteValue(TEXT("layoutInputHash"),
				Input.LayoutInputHash.ToHexString());
			Writer.WriteObjectEnd();
		}
		Writer.WriteArrayEnd();
		Writer.WriteArrayStart(TEXT("properties"));
		for (const FAngelscriptCachedPropertySchema& Property :
			Type.OrderedProperties)
		{
			Writer.WriteObjectStart();
			Writer.WriteValue(TEXT("layoutOrdinal"), Property.LayoutOrdinal);
			Writer.WriteValue(TEXT("semanticByteOffset"),
				Property.SemanticByteOffset);
			Writer.WriteValue(TEXT("propertyKey"),
				Property.PropertyKey.Hash.ToHexString());
			Writer.WriteValue(TEXT("canonicalName"), Property.CanonicalName);
			Writer.WriteValue(TEXT("storageKind"),
				static_cast<uint32>(Property.StorageKind));
			Writer.WriteValue(TEXT("semanticStorageSize"),
				Property.SemanticStorageSize);
			Writer.WriteValue(TEXT("semanticStorageAlignment"),
				Property.SemanticStorageAlignment);
			Writer.WriteValue(TEXT("storageLayoutHash"),
				Property.StorageLayoutHash.ToHexString());
			Writer.WriteValue(TEXT("access"),
				static_cast<uint32>(Property.Access));
			Writer.WriteValue(TEXT("propertySemanticFlags"),
				Property.PropertySemanticFlags);
			Writer.WriteValue(TEXT("replicationCondition"),
				static_cast<uint32>(Property.ReplicationCondition));
			Writer.WriteValue(TEXT("propertyLayoutFingerprint"),
				Property.PropertyLayoutFingerprint.ToHexString());
			WriteDataType(Writer, TEXT("type"), Property.Type);
			WriteMetadata(Writer, TEXT("metadata"), Property.Metadata);
			Writer.WriteObjectEnd();
		}
		Writer.WriteArrayEnd();
		Writer.WriteArrayStart(TEXT("methods"));
		for (const FAngelscriptCachedMethodEntry& Method : Type.OrderedMethods)
		{
			Writer.WriteObjectStart();
			Writer.WriteValue(TEXT("entryKind"),
				static_cast<uint32>(Method.EntryKind));
			Writer.WriteValue(TEXT("methodOrdinal"), Method.MethodOrdinal);
			Writer.WriteValue(TEXT("functionKey"),
				Method.FunctionKey.Hash.ToHexString());
			Writer.WriteValue(TEXT("declaringOwner"),
				Method.DeclaringOwner.Hash.ToHexString());
			Writer.WriteValue(TEXT("expectedDeclarationAbi"),
				Method.ExpectedDeclarationAbi.ToHexString());
			Writer.WriteObjectEnd();
		}
		Writer.WriteArrayEnd();
		Writer.WriteArrayStart(TEXT("virtualFunctionTable"));
		for (const FAngelscriptCachedVirtualFunctionSlot& Slot :
			Type.VirtualFunctionTable)
		{
			Writer.WriteObjectStart();
			Writer.WriteValue(TEXT("slotKind"),
				static_cast<uint32>(Slot.SlotKind));
			Writer.WriteValue(TEXT("vftOrdinal"), Slot.VftOrdinal);
			Writer.WriteValue(TEXT("functionKey"),
				Slot.FunctionKey.Hash.ToHexString());
			Writer.WriteValue(TEXT("declaringOwner"),
				Slot.DeclaringOwner.Hash.ToHexString());
			Writer.WriteValue(TEXT("implementingOwner"),
				Slot.ImplementingOwner.Hash.ToHexString());
			Writer.WriteValue(TEXT("expectedDeclarationAbi"),
				Slot.ExpectedDeclarationAbi.ToHexString());
			Writer.WriteObjectEnd();
		}
		Writer.WriteArrayEnd();
		Writer.WriteArrayStart(TEXT("behaviorSlots"));
		for (const FAngelscriptCachedBehaviorSlot& Slot :
			Type.OrderedBehaviorSlots)
		{
			Writer.WriteObjectStart();
			Writer.WriteValue(TEXT("behaviorKind"),
				static_cast<uint32>(Slot.BehaviorKind));
			Writer.WriteValue(TEXT("slotOrdinal"), Slot.SlotOrdinal);
			WriteStableReference(Writer, TEXT("target"), Slot.Target);
			if (Slot.DeclaringOwner.IsSet())
			{
				Writer.WriteValue(TEXT("declaringOwner"),
					Slot.DeclaringOwner->Hash.ToHexString());
			}
			Writer.WriteObjectEnd();
		}
		Writer.WriteArrayEnd();
		Writer.WriteObjectStart(TEXT("kindPayload"));
		if (Type.KindPayload.Enum.IsSet())
		{
			Writer.WriteArrayStart(TEXT("enumerators"));
			for (const FAngelscriptCachedEnumEnumerator& Enumerator :
				Type.KindPayload.Enum->OrderedEnumerators)
			{
				Writer.WriteObjectStart();
				Writer.WriteValue(TEXT("declarationOrdinal"),
					Enumerator.DeclarationOrdinal);
				Writer.WriteValue(TEXT("canonicalName"),
					Enumerator.CanonicalName);
				Writer.WriteValue(TEXT("value"), Enumerator.Value);
				WriteMetadata(Writer, TEXT("metadata"), Enumerator.Metadata);
				Writer.WriteObjectEnd();
			}
			Writer.WriteArrayEnd();
			Writer.WriteValue(TEXT("enumAuthorityHash"),
				Type.KindPayload.Enum->EnumAuthorityHash.ToHexString());
		}
		if (Type.KindPayload.Callable.IsSet())
		{
			Writer.WriteValue(TEXT("signatureFunctionKey"),
				Type.KindPayload.Callable->SignatureFunctionKey.Hash.ToHexString());
			Writer.WriteValue(TEXT("expectedSignatureAbi"),
				Type.KindPayload.Callable->ExpectedSignatureAbi.ToHexString());
			Writer.WriteValue(TEXT("multicast"),
				Type.KindPayload.Callable->bMulticast);
		}
		if (Type.KindPayload.Typedef.IsSet())
		{
			WriteDataType(Writer, TEXT("aliasedType"),
				Type.KindPayload.Typedef->AliasedType);
		}
		Writer.WriteObjectEnd();
		Writer.WriteObjectStart(TEXT("reflection"));
		Writer.WriteValue(TEXT("kind"),
			static_cast<uint32>(Type.Reflection.ReflectionKind));
		Writer.WriteValue(TEXT("classFlags"),
			Type.Reflection.ClassReflectionFlags);
		if (Type.Reflection.ConfigName.IsSet())
		{
			Writer.WriteValue(TEXT("configName"),
				Type.Reflection.ConfigName.GetValue());
		}
		if (Type.Reflection.StaticClassGlobalName.IsSet())
		{
			Writer.WriteValue(TEXT("staticClassGlobalName"),
				Type.Reflection.StaticClassGlobalName.GetValue());
		}
		Writer.WriteArrayStart(TEXT("functions"));
		for (const FAngelscriptCachedReflectedFunctionMember& Member :
			Type.Reflection.OrderedUFunctionMembers)
		{
			Writer.WriteObjectStart();
			Writer.WriteValue(TEXT("reflectionOrdinal"),
				Member.ReflectionOrdinal);
			Writer.WriteValue(TEXT("functionName"),
				Member.CanonicalFunctionName);
			Writer.WriteValue(TEXT("originalFunctionName"),
				Member.CanonicalOriginalFunctionName);
			Writer.WriteValue(TEXT("scriptFunctionName"),
				Member.CanonicalScriptFunctionName);
			WriteStableReference(Writer, TEXT("target"), Member.Target);
			Writer.WriteObjectEnd();
		}
		Writer.WriteArrayEnd();
		Writer.WriteObjectEnd();
		WriteDependencies(Writer, TEXT("dependencies"), Type.Dependencies);
		Writer.WriteObjectEnd();
	}

	static void WriteFunction(
		FJsonWriter& Writer,
		const FAngelscriptCacheDiagnosticFunctionSummary& Function)
	{
		Writer.WriteObjectStart();
		WriteRecordId(Writer, TEXT("recordId"), Function.RecordId);
		Writer.WriteValue(TEXT("functionKey"),
			Function.Identity.FunctionKey.Hash.ToHexString());
		Writer.WriteValue(TEXT("executionHash"),
			Function.Identity.Content.Execution.ToHexString());
		Writer.WriteValue(TEXT("debugHash"),
			Function.Identity.Content.Debug.ToHexString());
		Writer.WriteValue(TEXT("profile"),
			Function.Identity.Profile.Hash.ToHexString());
		Writer.WriteValue(TEXT("expectedDeclarationAbi"),
			Function.ExpectedDeclarationAbi.ToHexString());
		Writer.WriteValue(TEXT("functionSourceDigest"),
			Function.FunctionSourceDigest.Hash.ToHexString());
		Writer.WriteValue(TEXT("functionInputDigest"),
			Function.FunctionInputDigest.Hash.ToHexString());
		Writer.WriteValue(TEXT("invocationKind"),
			static_cast<uint32>(Function.InvocationKind));
		Writer.WriteValue(TEXT("invocationKindName"),
			InvocationKindName(Function.InvocationKind));
		Writer.WriteValue(TEXT("vmExecutionCodecVersion"),
			Function.VmExecutionCodecVersion);
		Writer.WriteValue(TEXT("canonicalExecutionPayloadBytes"),
			UInt64String(Function.CanonicalExecutionPayloadBytes));
		WriteDependencies(
			Writer, TEXT("actualDependencies"), Function.ActualDependencies);
		if (Function.DebugSidecar.IsSet())
		{
			WriteRecordId(
				Writer, TEXT("debugSidecar"), Function.DebugSidecar.GetValue());
		}
		Writer.WriteObjectEnd();
	}

	static void WriteFunctionRoutes(
		FJsonWriter& Writer,
		const FAngelscriptCacheDiagnosticFunctionRouteSnapshot& Routes)
	{
		Writer.WriteObjectStart(TEXT("functionRoutes"));
		Writer.WriteValue(TEXT("present"), Routes.bPresent);
		if (Routes.bPresent)
		{
			Writer.WriteValue(TEXT("publicationOrdinal"),
				UInt64String(Routes.PublicationOrdinal));
			Writer.WriteValue(TEXT("vmRouteCount"), Routes.VmRouteCount);
			Writer.WriteValue(TEXT("nativeRouteCount"), Routes.NativeRouteCount);
			Writer.WriteArrayStart(TEXT("routes"));
			for (const FAngelscriptCacheDiagnosticFunctionRoute& Route :
				Routes.Routes)
			{
				Writer.WriteObjectStart();
				Writer.WriteValue(TEXT("moduleKey"),
					Route.ModuleKey.Hash.ToHexString());
				Writer.WriteValue(TEXT("functionKey"),
					Route.Identity.FunctionKey.Hash.ToHexString());
				Writer.WriteValue(TEXT("executionHash"),
					Route.Identity.Content.Execution.ToHexString());
				Writer.WriteValue(TEXT("debugHash"),
					Route.Identity.Content.Debug.ToHexString());
				Writer.WriteValue(TEXT("profile"),
					Route.Identity.Profile.Hash.ToHexString());
				Writer.WriteValue(TEXT("canonicalDeclaration"),
					Route.CanonicalDeclaration);
				Writer.WriteValue(TEXT("selectedRoute"),
					static_cast<uint32>(Route.SelectedExecutionRoute));
				Writer.WriteValue(TEXT("selectedRouteName"),
					RouteName(Route.SelectedExecutionRoute));
				Writer.WriteValue(TEXT("verifiedArtifactIdentity"),
					Route.bHasVerifiedArtifactIdentity);
				Writer.WriteObjectEnd();
			}
			Writer.WriteArrayEnd();
		}
		Writer.WriteObjectEnd();
	}

	static void WritePublication(
		FJsonWriter& Writer,
		const TCHAR* FieldName,
		const FAngelscriptCacheDiagnosticPublicationSummary& Publication)
	{
		Writer.WriteObjectStart(FieldName);
		Writer.WriteValue(TEXT("present"), Publication.bPresent);
		if (!Publication.bPresent)
		{
			Writer.WriteObjectEnd();
			return;
		}

		Writer.WriteValue(TEXT("publicationSchemaVersion"),
			Publication.PublicationSchemaVersion);
		Writer.WriteValue(TEXT("transactionOrdinal"),
			UInt64String(Publication.TransactionOrdinal));
		Writer.WriteValue(TEXT("compileKind"),
			static_cast<uint32>(Publication.CompileKind));
		Writer.WriteValue(TEXT("compileKindName"),
			CompileKindName(Publication.CompileKind));
		Writer.WriteValue(TEXT("disposition"),
			static_cast<uint32>(Publication.Disposition));
		Writer.WriteValue(TEXT("dispositionName"),
			DispositionName(Publication.Disposition));
		Writer.WriteValue(TEXT("compatibility"),
			Publication.Compatibility.Hash.ToHexString());
		Writer.WriteValue(TEXT("context"),
			Publication.Context.Hash.ToHexString());
		Writer.WriteValue(TEXT("profile"),
			Publication.Profile.Hash.ToHexString());
		Writer.WriteValue(TEXT("sourceSnapshot"),
			Publication.SourceSnapshot.ToHexString());
		Writer.WriteValue(TEXT("restoredFromStore"),
			Publication.bRestoredFromStore);
		if (Publication.bRestoredFromStore)
		{
			Writer.WriteValue(TEXT("persistedGenerationId"),
				Publication.PersistedGenerationId.ToHexString());
		}
		WriteRecordId(Writer, TEXT("sourceIndexRecord"),
			Publication.SourceIndexRecordId);
		Writer.WriteValue(TEXT("totalRecordCount"),
			Publication.TotalRecordCount);
		Writer.WriteValue(TEXT("canonicalPayloadBytes"),
			UInt64String(Publication.CanonicalPayloadBytes));
		Writer.WriteArrayStart(TEXT("modules"));
		for (const FAngelscriptCacheDiagnosticModuleSummary& Module
			: Publication.Modules)
		{
			Writer.WriteObjectStart();
			Writer.WriteValue(TEXT("moduleKey"),
				Module.ModuleKey.Hash.ToHexString());
			Writer.WriteValue(TEXT("canonicalModuleName"),
				Module.CanonicalModuleName);
			WriteRecordId(Writer, TEXT("moduleSnapshotRecord"),
				Module.ModuleSnapshotRecordId);
			if (Module.ModuleInterfaceRecordId.IsSet())
			{
				WriteRecordId(Writer, TEXT("moduleInterfaceRecord"),
					Module.ModuleInterfaceRecordId.GetValue());
			}
			if (Module.ModuleStateRecordId.IsSet())
			{
				WriteRecordId(Writer, TEXT("moduleStateRecord"),
					Module.ModuleStateRecordId.GetValue());
			}
			Writer.WriteValue(TEXT("interfaceAbi"),
				Module.InterfaceAbi.ToHexString());
			Writer.WriteArrayStart(TEXT("declarations"));
			for (const FAngelscriptCachedDeclaration& Declaration :
				Module.Declarations)
			{
				WriteDeclaration(Writer, Declaration);
			}
			Writer.WriteArrayEnd();
			Writer.WriteArrayStart(TEXT("imports"));
			for (const FAngelscriptCachedImportDeclaration& Import : Module.Imports)
			{
				Writer.WriteObjectStart();
				Writer.WriteValue(TEXT("importKey"),
					Import.ImportKey.Hash.ToHexString());
				Writer.WriteValue(TEXT("canonicalNamespace"),
					Import.CanonicalNamespace);
				Writer.WriteValue(TEXT("canonicalName"), Import.CanonicalName);
				Writer.WriteValue(TEXT("canonicalSignature"),
					Import.CanonicalSignature);
				Writer.WriteValue(TEXT("targetModuleKey"),
					Import.TargetModuleKey.Hash.ToHexString());
				WriteStableReference(
					Writer, TEXT("targetDeclaration"), Import.TargetDeclaration);
				Writer.WriteObjectEnd();
			}
			Writer.WriteArrayEnd();
			WriteDependencies(Writer, TEXT("interfaceDependencies"),
				Module.InterfaceDependencies);
			Writer.WriteArrayStart(TEXT("types"));
			for (const FAngelscriptCacheDiagnosticTypeSummary& Type : Module.Types)
			{
				WriteType(Writer, Type);
			}
			Writer.WriteArrayEnd();
			Writer.WriteObjectStart(TEXT("state"));
			Writer.WriteValue(TEXT("profile"),
				Module.StateProfile.Hash.ToHexString());
			Writer.WriteValue(TEXT("inputHash"),
				Module.StateInputHash.ToHexString());
			Writer.WriteArrayStart(TEXT("globals"));
			for (const FAngelscriptCachedGlobalSchema& Global : Module.Globals)
			{
				Writer.WriteObjectStart();
				Writer.WriteValue(TEXT("storageOrdinal"), Global.StorageOrdinal);
				Writer.WriteValue(TEXT("globalKey"),
					Global.GlobalKey.Hash.ToHexString());
				Writer.WriteValue(TEXT("canonicalNamespace"),
					Global.CanonicalNamespace);
				Writer.WriteValue(TEXT("canonicalName"), Global.CanonicalName);
				Writer.WriteValue(TEXT("traitFlags"), Global.GlobalTraitFlags);
				Writer.WriteValue(TEXT("initializationKind"),
					static_cast<uint32>(Global.InitializationKind));
				Writer.WriteValue(TEXT("cleanupPolicy"),
					static_cast<uint32>(Global.CleanupPolicy));
				Writer.WriteValue(TEXT("storageLayoutFingerprint"),
					Global.StorageLayoutFingerprint.ToHexString());
				WriteDataType(Writer, TEXT("type"), Global.Type);
				Writer.WriteObjectEnd();
			}
			Writer.WriteArrayEnd();
			Writer.WriteArrayStart(TEXT("initializers"));
			for (const FAngelscriptCacheDiagnosticInitializerSummary& Initializer :
				Module.Initializers)
			{
				Writer.WriteObjectStart();
				Writer.WriteValue(TEXT("kind"),
					static_cast<uint32>(Initializer.InitializerKind));
				Writer.WriteValue(TEXT("initializerKey"),
					Initializer.InitializerKey.Hash.ToHexString());
				if (Initializer.OwnerGlobal.IsSet())
				{
					Writer.WriteValue(TEXT("ownerGlobal"),
						Initializer.OwnerGlobal->Hash.ToHexString());
				}
				Writer.WriteValue(TEXT("vmInitializerCodecVersion"),
					Initializer.VmInitializerCodecVersion);
				Writer.WriteValue(TEXT("executionHash"),
					Initializer.InitializerExecutionHash.ToHexString());
				Writer.WriteValue(TEXT("canonicalExecutionPayloadBytes"),
					UInt64String(
						Initializer.CanonicalExecutionPayloadBytes));
				Writer.WriteObjectEnd();
			}
			Writer.WriteArrayEnd();
			WriteDependencies(Writer, TEXT("dependencies"),
				Module.StateDependencies);
			Writer.WriteObjectEnd();
			Writer.WriteArrayStart(TEXT("functions"));
			for (const FAngelscriptCacheDiagnosticFunctionSummary& Function :
				Module.Functions)
			{
				WriteFunction(Writer, Function);
			}
			Writer.WriteArrayEnd();
			Writer.WriteArrayStart(TEXT("decodeFailures"));
			for (const FAngelscriptCacheDiagnosticDecodeFailure& Failure :
				Module.DecodeFailures)
			{
				Writer.WriteObjectStart();
				WriteRecordId(Writer, TEXT("recordId"), Failure.RecordId);
				WriteValidation(Writer, TEXT("validation"), Failure.Validation);
				Writer.WriteObjectEnd();
			}
			Writer.WriteArrayEnd();
			Writer.WriteValue(TEXT("totalRecordCount"),
				Module.TotalRecordCount);
			Writer.WriteValue(TEXT("canonicalPayloadBytes"),
				UInt64String(Module.CanonicalPayloadBytes));
			Writer.WriteArrayStart(TEXT("recordKinds"));
			for (const FAngelscriptCacheDiagnosticRecordKindSummary& Kind
				: Module.RecordKinds)
			{
				Writer.WriteObjectStart();
				Writer.WriteValue(TEXT("kind"),
					static_cast<uint32>(Kind.Kind));
				Writer.WriteValue(TEXT("kindName"), RecordKindName(Kind.Kind));
				Writer.WriteValue(TEXT("count"), Kind.RecordCount);
				Writer.WriteValue(TEXT("canonicalPayloadBytes"),
					UInt64String(Kind.CanonicalPayloadBytes));
				Writer.WriteObjectEnd();
			}
			Writer.WriteArrayEnd();
			Writer.WriteObjectEnd();
		}
		Writer.WriteArrayEnd();
		Writer.WriteObjectEnd();
	}

	static void WriteDecisionEvent(
		FJsonWriter& Writer,
		const FAngelscriptCacheDecisionEvent& Event)
	{
		Writer.WriteObjectStart();
		Writer.WriteValue(TEXT("schemaVersion"), Event.SchemaVersion);
		Writer.WriteValue(TEXT("eventOrdinal"),
			UInt64String(Event.EventOrdinal));
		Writer.WriteValue(TEXT("transactionOrdinal"),
			UInt64String(Event.TransactionOrdinal));
		Writer.WriteValue(TEXT("stage"),
			static_cast<uint32>(Event.Stage));
		Writer.WriteValue(TEXT("stageName"),
			DecisionStageName(Event.Stage));
		Writer.WriteValue(TEXT("outcome"),
			static_cast<uint32>(Event.Outcome));
		Writer.WriteValue(TEXT("outcomeName"),
			DecisionOutcomeName(Event.Outcome));
		Writer.WriteValue(TEXT("reasonDomain"),
			static_cast<uint32>(Event.ReasonDomain));
		Writer.WriteValue(TEXT("reasonDomainName"),
			DecisionReasonDomainName(Event.ReasonDomain));
		Writer.WriteValue(TEXT("reasonCode"), Event.ReasonCode);
		Writer.WriteArrayStart(TEXT("moduleKeys"));
		for (const FAngelscriptStableModuleKey& ModuleKey : Event.ModuleKeys)
		{
			Writer.WriteValue(ModuleKey.Hash.ToHexString());
		}
		Writer.WriteArrayEnd();
		if (Event.FunctionKey.IsSet())
		{
			Writer.WriteValue(TEXT("functionKey"),
				Event.FunctionKey->Hash.ToHexString());
		}
		if (Event.RecordId.IsSet())
		{
			WriteRecordId(Writer, TEXT("recordId"),
				Event.RecordId.GetValue());
		}
		if (Event.ExpectedCoordinate.IsSet())
		{
			Writer.WriteValue(TEXT("expectedCoordinate"),
				Event.ExpectedCoordinate->ToHexString());
		}
		if (Event.CurrentCoordinate.IsSet())
		{
			Writer.WriteValue(TEXT("currentCoordinate"),
				Event.CurrentCoordinate->ToHexString());
		}
		Writer.WriteValue(TEXT("profile"), Event.Profile.Hash.ToHexString());
		Writer.WriteValue(TEXT("sourceSnapshot"),
			Event.SourceSnapshot.ToHexString());
		Writer.WriteValue(TEXT("primaryCount"), Event.PrimaryCount);
		Writer.WriteValue(TEXT("secondaryCount"), Event.SecondaryCount);
		Writer.WriteValue(TEXT("elapsedMicroseconds"),
			UInt64String(Event.ElapsedMicroseconds));
		if (Event.Validation.IsSet())
		{
			WriteValidation(
				Writer, TEXT("validation"), Event.Validation.GetValue());
		}
		if (!Event.Detail.IsEmpty())
		{
			Writer.WriteValue(TEXT("detail"), Event.Detail);
		}
		Writer.WriteObjectEnd();
	}

	static void WriteDecisionTrace(
		FJsonWriter& Writer,
		const FAngelscriptCacheDecisionTraceSnapshot& Trace)
	{
		Writer.WriteObjectStart(TEXT("decisionTrace"));
		Writer.WriteValue(TEXT("schemaVersion"), Trace.SchemaVersion);
		Writer.WriteValue(TEXT("enabled"), Trace.bEnabled);
		Writer.WriteValue(TEXT("capacity"), Trace.Capacity);
		Writer.WriteValue(TEXT("nextEventOrdinal"),
			UInt64String(Trace.NextEventOrdinal));
		Writer.WriteValue(TEXT("evictedEventCount"),
			UInt64String(Trace.EvictedEventCount));
		Writer.WriteArrayStart(TEXT("events"));
		for (const FAngelscriptCacheDecisionEvent& Event : Trace.Events)
		{
			WriteDecisionEvent(Writer, Event);
		}
		Writer.WriteArrayEnd();
		Writer.WriteObjectEnd();
	}

	static void WriteFunctionReuseSummary(
		FJsonWriter& Writer,
		const FAngelscriptCacheFunctionReuseSummary& Summary)
	{
		Writer.WriteObjectStart(TEXT("functionReuse"));
		Writer.WriteValue(TEXT("present"), Summary.bPresent);
		if (Summary.bPresent)
		{
			Writer.WriteValue(TEXT("schemaVersion"), Summary.SchemaVersion);
			Writer.WriteValue(TEXT("candidateGenerationId"),
				Summary.CandidateGenerationId.ToHexString());
			Writer.WriteValue(TEXT("candidateModuleCount"),
				Summary.CandidateModuleCount);
			Writer.WriteValue(TEXT("restoredFunctionCount"),
				Summary.RestoredFunctionCount);
			Writer.WriteValue(TEXT("compiledMissCount"),
				Summary.CompiledMissCount);
			Writer.WriteValue(TEXT("notCacheableCount"),
				Summary.NotCacheableCount);
			Writer.WriteValue(TEXT("rejectedCorruptCount"),
				Summary.RejectedCorruptCount);
		}
		Writer.WriteObjectEnd();
	}
}

FAngelscriptCacheDiagnosticFunctionRouteSnapshot
BuildAngelscriptCacheDiagnosticFunctionRoutes(
	const FAngelscriptCacheFunctionRouteSnapshot& Routes)
{
	FAngelscriptCacheDiagnosticFunctionRouteSnapshot Snapshot;
	Snapshot.bPresent = true;
	Snapshot.PublicationOrdinal = Routes.PublicationOrdinal;
	Snapshot.Routes.Reserve(Routes.FunctionRoutes.Num());
	for (const FAngelscriptCacheLiveFunctionRoute& LiveRoute :
		Routes.FunctionRoutes)
	{
		FAngelscriptCacheDiagnosticFunctionRoute& Route =
			Snapshot.Routes.AddDefaulted_GetRef();
		Route.ModuleKey = LiveRoute.ModuleKey;
		Route.Identity = LiveRoute.Identity;
		Route.CanonicalDeclaration = LiveRoute.CanonicalDeclaration;
		Route.SelectedExecutionRoute = LiveRoute.SelectedExecutionRoute;
		Route.bHasVerifiedArtifactIdentity =
			LiveRoute.bHasVerifiedArtifactIdentity;
		if (Route.SelectedExecutionRoute ==
			EAngelscriptCacheFunctionExecutionRoute::Native)
		{
			++Snapshot.NativeRouteCount;
		}
		else
		{
			++Snapshot.VmRouteCount;
		}
	}
	Snapshot.Routes.Sort([](
		const FAngelscriptCacheDiagnosticFunctionRoute& Left,
		const FAngelscriptCacheDiagnosticFunctionRoute& Right)
	{
		return Left.Identity.FunctionKey.Hash
			< Right.Identity.FunctionKey.Hash;
	});
	return Snapshot;
}

FAngelscriptCacheDiagnosticSnapshot BuildAngelscriptCacheDiagnosticSnapshot(
	const EAngelscriptCacheMutationPhase MutationPhase,
	const uint64 LastTransactionOrdinal,
	const FAngelscriptCacheLifecyclePublications& Publications,
	const FAngelscriptCacheDecisionTraceSnapshot& DecisionTrace)
{
	using namespace AngelscriptCacheDiagnostics_Private;
	FAngelscriptCacheDiagnosticSnapshot Snapshot;
	Snapshot.MutationPhase = MutationPhase;
	Snapshot.LastTransactionOrdinal = LastTransactionOrdinal;
	Snapshot.Current = BuildPublication(Publications.Current);
	Snapshot.PendingColdStart = BuildPublication(
		Publications.PendingColdStart);
	Snapshot.LatestSuccessful = BuildPublication(
		Publications.LatestSuccessful);
	Snapshot.DecisionTrace = DecisionTrace;
	return Snapshot;
}

bool SerializeAngelscriptCacheDiagnosticSnapshotJson(
	const FAngelscriptCacheDiagnosticSnapshot& Snapshot,
	FString& OutJson)
{
	using namespace AngelscriptCacheDiagnostics_Private;
	OutJson.Reset();
	TSharedRef<FJsonWriter> Writer =
		TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(
			&OutJson);
	Writer->WriteObjectStart();
	Writer->WriteValue(TEXT("schemaVersion"), Snapshot.SchemaVersion);
	Writer->WriteValue(TEXT("mutationPhase"),
		static_cast<uint32>(Snapshot.MutationPhase));
	Writer->WriteValue(TEXT("mutationPhaseName"),
		MutationPhaseName(Snapshot.MutationPhase));
	Writer->WriteValue(TEXT("lastTransactionOrdinal"),
		UInt64String(Snapshot.LastTransactionOrdinal));
	WritePublication(*Writer, TEXT("current"), Snapshot.Current);
	WritePublication(*Writer, TEXT("pendingColdStart"),
		Snapshot.PendingColdStart);
	WritePublication(*Writer, TEXT("latestSuccessful"),
		Snapshot.LatestSuccessful);
	WriteFunctionRoutes(*Writer, Snapshot.FunctionRoutes);
	WriteFunctionReuseSummary(*Writer, Snapshot.FunctionReuse);
	WriteDecisionTrace(*Writer, Snapshot.DecisionTrace);
	Writer->WriteObjectEnd();
	return Writer->Close();
}

FAngelscriptCacheExplainResult ExplainAngelscriptCacheDecisions(
	const FAngelscriptCacheDecisionTraceSnapshot& Trace,
	const FAngelscriptCacheExplainRequest& Request)
{
	FAngelscriptCacheExplainResult Result;
	Result.Request = Request;
	if (!Request.HasSelector())
	{
		Result.Error = EAngelscriptCacheDiagnosticApiError::InvalidRequest;
		Result.Detail = TEXT("Cache explain requires at least one typed selector.");
		return Result;
	}

	for (const FAngelscriptCacheDecisionEvent& Event : Trace.Events)
	{
		if (Request.EventOrdinal.IsSet()
			&& Event.EventOrdinal != Request.EventOrdinal.GetValue())
		{
			continue;
		}
		if (Request.TransactionOrdinal.IsSet()
			&& Event.TransactionOrdinal
				!= Request.TransactionOrdinal.GetValue())
		{
			continue;
		}
		if (Request.Stage.IsSet() && Event.Stage != Request.Stage.GetValue())
		{
			continue;
		}
		if (Request.ModuleKey.IsSet()
			&& !Event.ModuleKeys.Contains(Request.ModuleKey.GetValue()))
		{
			continue;
		}
		if (Request.FunctionKey.IsSet()
			&& (!Event.FunctionKey.IsSet()
				|| Event.FunctionKey.GetValue()
					!= Request.FunctionKey.GetValue()))
		{
			continue;
		}
		if (Request.RecordId.IsSet()
			&& (!Event.RecordId.IsSet()
				|| !(Event.RecordId.GetValue()
					== Request.RecordId.GetValue())))
		{
			continue;
		}
		Result.Events.Add(Event);
	}

	Result.Events.Sort([](
		const FAngelscriptCacheDecisionEvent& Left,
		const FAngelscriptCacheDecisionEvent& Right)
	{
		if (Left.EventOrdinal != Right.EventOrdinal)
		{
			return Left.EventOrdinal < Right.EventOrdinal;
		}
		if (Left.TransactionOrdinal != Right.TransactionOrdinal)
		{
			return Left.TransactionOrdinal < Right.TransactionOrdinal;
		}
		if (Left.Stage != Right.Stage)
		{
			return static_cast<uint8>(Left.Stage)
				< static_cast<uint8>(Right.Stage);
		}
		return static_cast<uint8>(Left.Outcome)
			< static_cast<uint8>(Right.Outcome);
	});
	if (Result.Events.IsEmpty())
	{
		Result.Error = EAngelscriptCacheDiagnosticApiError::NoMatch;
		Result.Detail = TEXT("No captured Cache V2 decision matched every selector.");
	}
	else
	{
		Result.Detail = FString::Printf(
			TEXT("Matched %d captured Cache V2 decision event(s)."),
			Result.Events.Num());
	}
	return Result;
}

FAngelscriptCacheExplainResult ExplainAngelscriptCacheDecisions(
	const FAngelscriptEngine* Engine,
	const FAngelscriptCacheExplainRequest& Request)
{
	if (Engine == nullptr)
	{
		FAngelscriptCacheExplainResult Result;
		Result.Request = Request;
		Result.Error = EAngelscriptCacheDiagnosticApiError::EngineUnavailable;
		Result.Detail = TEXT("AngelScript Engine is not available.");
		return Result;
	}
	const FAngelscriptCacheService* Service = Engine->GetCacheService();
	if (Service == nullptr)
	{
		FAngelscriptCacheExplainResult Result;
		Result.Request = Request;
		Result.Error = EAngelscriptCacheDiagnosticApiError::ServiceUnavailable;
		Result.Detail = TEXT("Cache V2 Service is not available on the Engine.");
		return Result;
	}
	return ExplainAngelscriptCacheDecisions(
		Service->CaptureDecisionTrace(), Request);
}

bool SerializeAngelscriptCacheExplainResultJson(
	const FAngelscriptCacheExplainResult& Result,
	FString& OutJson)
{
	using namespace AngelscriptCacheDiagnostics_Private;
	OutJson.Reset();
	TSharedRef<FJsonWriter> Writer =
		TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(
			&OutJson);
	Writer->WriteObjectStart();
	Writer->WriteValue(TEXT("schemaVersion"), Result.SchemaVersion);
	Writer->WriteValue(TEXT("error"), static_cast<uint32>(Result.Error));
	Writer->WriteValue(TEXT("detail"), Result.Detail);
	Writer->WriteValue(TEXT("matchedEventCount"), Result.Events.Num());
	Writer->WriteObjectStart(TEXT("request"));
	if (Result.Request.EventOrdinal.IsSet())
	{
		Writer->WriteValue(TEXT("eventOrdinal"),
			UInt64String(Result.Request.EventOrdinal.GetValue()));
	}
	if (Result.Request.TransactionOrdinal.IsSet())
	{
		Writer->WriteValue(TEXT("transactionOrdinal"),
			UInt64String(Result.Request.TransactionOrdinal.GetValue()));
	}
	if (Result.Request.Stage.IsSet())
	{
		Writer->WriteValue(TEXT("stage"),
			static_cast<uint32>(Result.Request.Stage.GetValue()));
		Writer->WriteValue(TEXT("stageName"),
			DecisionStageName(Result.Request.Stage.GetValue()));
	}
	if (Result.Request.ModuleKey.IsSet())
	{
		Writer->WriteValue(TEXT("moduleKey"),
			Result.Request.ModuleKey->Hash.ToHexString());
	}
	if (Result.Request.FunctionKey.IsSet())
	{
		Writer->WriteValue(TEXT("functionKey"),
			Result.Request.FunctionKey->Hash.ToHexString());
	}
	if (Result.Request.RecordId.IsSet())
	{
		WriteRecordId(*Writer, TEXT("recordId"),
			Result.Request.RecordId.GetValue());
	}
	Writer->WriteObjectEnd();
	Writer->WriteArrayStart(TEXT("events"));
	for (const FAngelscriptCacheDecisionEvent& Event : Result.Events)
	{
		WriteDecisionEvent(*Writer, Event);
	}
	Writer->WriteArrayEnd();
	Writer->WriteObjectEnd();
	return Writer->Close();
}

FAngelscriptCacheDiagnosticJsonResult CaptureAngelscriptCacheDiagnosticJson(
	const FAngelscriptEngine* Engine)
{
	FAngelscriptCacheDiagnosticJsonResult Result;
	if (Engine == nullptr)
	{
		Result.Error =
			EAngelscriptCacheDiagnosticApiError::EngineUnavailable;
		Result.Detail = TEXT("AngelScript Engine is not available.");
		return Result;
	}

	const FAngelscriptCacheService* Service = Engine->GetCacheService();
	if (Service == nullptr)
	{
		Result.Error =
			EAngelscriptCacheDiagnosticApiError::ServiceUnavailable;
		Result.Detail = TEXT("Cache V2 Service is not available on the Engine.");
		return Result;
	}

	FAngelscriptCacheDiagnosticSnapshot Snapshot =
		Service->CaptureDiagnosticSnapshot();
	const TSharedPtr<const FAngelscriptCacheFunctionRouteSnapshot,
		ESPMode::ThreadSafe> LiveRoutes = Engine->GetFunctionRouteSnapshot();
	if (LiveRoutes.IsValid())
	{
		Snapshot.FunctionRoutes =
			BuildAngelscriptCacheDiagnosticFunctionRoutes(*LiveRoutes);
	}
	if (!SerializeAngelscriptCacheDiagnosticSnapshotJson(
		Snapshot, Result.Json))
	{
		Result.Error =
			EAngelscriptCacheDiagnosticApiError::SerializationFailed;
		Result.Detail = TEXT("Cache V2 diagnostic JSON serialization failed.");
		Result.Json.Reset();
		return Result;
	}

	return Result;
}

FAngelscriptCacheDiagnosticJsonResult
CaptureCurrentAngelscriptCacheDiagnosticJson()
{
	FAngelscriptEngine* Engine = FAngelscriptEngine::TryGetCurrentEngine();
	if (Engine == nullptr && FAngelscriptEngine::IsInitialized())
	{
		Engine = &FAngelscriptEngine::Get();
	}
	return CaptureAngelscriptCacheDiagnosticJson(Engine);
}

FAngelscriptCacheReportWriteResult
WriteAngelscriptCacheDiagnosticJsonReport(
	const FAngelscriptEngine* Engine,
	const FString& RequestedPath)
{
	FAngelscriptCacheReportWriteResult Result;
	FString TrimmedPath = RequestedPath;
	TrimmedPath.TrimStartAndEndInline();
	if (TrimmedPath.Len() >= 2
		&& TrimmedPath.StartsWith(TEXT("\""))
		&& TrimmedPath.EndsWith(TEXT("\"")))
	{
		TrimmedPath = TrimmedPath.Mid(1, TrimmedPath.Len() - 2);
	}
	if (TrimmedPath.IsEmpty()
		|| FPaths::IsRelative(TrimmedPath)
		|| !FPaths::GetExtension(TrimmedPath, true).Equals(
			TEXT(".json"), ESearchCase::IgnoreCase))
	{
		Result.Error =
			EAngelscriptCacheDiagnosticApiError::ReportPathInvalid;
		Result.Detail =
			TEXT("Cache V2 process report requires an absolute .json path.");
		return Result;
	}

	Result.ResolvedPath = FPaths::ConvertRelativePathToFull(TrimmedPath);
	FPaths::NormalizeFilename(Result.ResolvedPath);
	const FAngelscriptCacheDiagnosticJsonResult Diagnostic =
		CaptureAngelscriptCacheDiagnosticJson(Engine);
	if (!Diagnostic.IsSuccess())
	{
		Result.Error = Diagnostic.Error;
		Result.Detail = Diagnostic.Detail;
		Result.ResolvedPath.Reset();
		return Result;
	}

	const FString Directory = FPaths::GetPath(Result.ResolvedPath);
	if (Directory.IsEmpty()
		|| !IFileManager::Get().MakeDirectory(*Directory, true)
		|| !FFileHelper::SaveStringToFile(
			Diagnostic.Json,
			*Result.ResolvedPath,
			FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		Result.Error =
			EAngelscriptCacheDiagnosticApiError::ReportWriteFailed;
		Result.Detail = FString::Printf(
			TEXT("Failed to write Cache V2 process report to %s."),
			*Result.ResolvedPath);
		Result.ResolvedPath.Reset();
		return Result;
	}

	Result.Detail = FString::Printf(
		TEXT("Wrote Cache V2 process report to %s."),
		*Result.ResolvedPath);
	return Result;
}

FAngelscriptCacheStoreResult
ResolveAngelscriptCacheRequestedBaseRootForEngine(
	const FAngelscriptEngine& Engine,
	FString& OutRequestedBaseRoot)
{
	FAngelscriptCacheRootSelectionInputs RootInputs;
	RootInputs.ProjectSavedDirectory = FPaths::ProjectSavedDir();
	RootInputs.LaunchWorkingDirectory =
		FPlatformProcess::GetCurrentWorkingDirectory();
	FString RootOverride = Engine.GetRuntimeConfig().CacheV2RootOverride;
	if (RootOverride.IsEmpty())
	{
		FParse::Value(
			FCommandLine::Get(), TEXT("-as-cache-root="), RootOverride);
	}
	if (!RootOverride.IsEmpty())
	{
		RootInputs.Override = MoveTemp(RootOverride);
	}
	return ResolveAngelscriptCacheRequestedBaseRoot(
		RootInputs, OutRequestedBaseRoot);
}

FAngelscriptCacheFlushApiResult FlushAngelscriptCacheToStore(
	const FAngelscriptEngine* Engine,
	const double TimeoutSeconds)
{
	FAngelscriptCacheFlushApiResult Result;
	if (Engine == nullptr)
	{
		Result.Error =
			EAngelscriptCacheDiagnosticApiError::EngineUnavailable;
		Result.Detail = TEXT("AngelScript Engine is not available.");
		return Result;
	}

	FAngelscriptCacheService* Service = Engine->GetCacheService();
	if (Service == nullptr)
	{
		Result.Error =
			EAngelscriptCacheDiagnosticApiError::ServiceUnavailable;
		Result.Detail = TEXT("Cache V2 Service is not available on the Engine.");
		return Result;
	}

	const UAngelscriptCacheSettings* Settings =
		GetDefault<UAngelscriptCacheSettings>();
	if (Settings == nullptr || !Settings->bEnableCacheV2)
	{
		Result.Error = EAngelscriptCacheDiagnosticApiError::CacheDisabled;
		Result.Detail = TEXT("Cache V2 is disabled by project settings.");
		return Result;
	}
	if (Engine->GetRuntimeConfig().bDisableCacheV2Persistence)
	{
		Result.Error =
			EAngelscriptCacheDiagnosticApiError::PersistenceDisabled;
		Result.Detail = TEXT("Cache V2 persistence is disabled for this Engine.");
		return Result;
	}

	double EffectiveTimeoutSeconds = TimeoutSeconds;
	if (EffectiveTimeoutSeconds == 0.0)
	{
		EffectiveTimeoutSeconds = Settings->ShutdownFlushTimeoutSeconds;
	}
	if (!FMath::IsFinite(EffectiveTimeoutSeconds)
		|| EffectiveTimeoutSeconds <= 0.0)
	{
		Result.Error = EAngelscriptCacheDiagnosticApiError::InvalidRequest;
		Result.Detail = TEXT("Cache V2 flush requires a positive finite timeout.");
		return Result;
	}

	FString RequestedBaseRoot;
	Result.RootSelection = ResolveAngelscriptCacheRequestedBaseRootForEngine(
		*Engine, RequestedBaseRoot);
	if (!Result.RootSelection.IsSuccess())
	{
		Result.Error =
			EAngelscriptCacheDiagnosticApiError::RootSelectionFailed;
		Result.Detail = FString::Printf(
			TEXT("Cache V2 root selection failed: Error=%u Stage=%u PathCategory=%u"),
			static_cast<uint32>(Result.RootSelection.Error),
			static_cast<uint32>(Result.RootSelection.Stage),
			static_cast<uint32>(Result.RootSelection.PathCategory));
		return Result;
	}

	Result.Flush = Service->FlushLifecyclePublicationsToStore(
		RequestedBaseRoot, EffectiveTimeoutSeconds);
	Result.Detail = Result.Flush.Detail;
	if (!Result.Flush.IsSuccess())
	{
		Result.Error = EAngelscriptCacheDiagnosticApiError::FlushFailed;
	}
	return Result;
}

FAngelscriptCacheFlushApiResult FlushCurrentAngelscriptCacheToStore(
	const double TimeoutSeconds)
{
	FAngelscriptEngine* Engine = FAngelscriptEngine::TryGetCurrentEngine();
	if (Engine == nullptr && FAngelscriptEngine::IsInitialized())
	{
		Engine = &FAngelscriptEngine::Get();
	}
	return FlushAngelscriptCacheToStore(Engine, TimeoutSeconds);
}

namespace AngelscriptCacheDiagnostics_Private
{
	static TSharedPtr<const FAngelscriptCacheSuccessfulPublicationDto,
		ESPMode::ThreadSafe> ResolveStoreCoordinatePublication(
		const FAngelscriptCacheService& Service)
	{
		const FAngelscriptCacheLifecyclePublications Publications =
			Service.GetLifecyclePublications();
		if (Publications.Current.IsValid())
		{
			return Publications.Current;
		}
		if (Publications.PendingColdStart.IsValid())
		{
			return Publications.PendingColdStart;
		}
		return Publications.LatestSuccessful;
	}

	static bool ResolveMaintenanceTimeout(
		const double RequestedTimeoutSeconds,
		double& OutTimeoutSeconds)
	{
		OutTimeoutSeconds = RequestedTimeoutSeconds;
		if (OutTimeoutSeconds == 0.0)
		{
			const UAngelscriptCacheSettings* Settings =
				GetDefault<UAngelscriptCacheSettings>();
			OutTimeoutSeconds = Settings == nullptr
				? 5.0
				: Settings->ShutdownFlushTimeoutSeconds;
		}
		return FMath::IsFinite(OutTimeoutSeconds)
			&& OutTimeoutSeconds > 0.0;
	}

	static EAngelscriptCachePointerKind ToPointerKind(
		const EAngelscriptCacheDiagnosticGeneration Generation)
	{
		switch (Generation)
		{
		case EAngelscriptCacheDiagnosticGeneration::Current:
			return EAngelscriptCachePointerKind::Current;
		case EAngelscriptCacheDiagnosticGeneration::Previous:
			return EAngelscriptCachePointerKind::Previous;
		case EAngelscriptCacheDiagnosticGeneration::PendingColdStart:
			return EAngelscriptCachePointerKind::PendingColdStart;
		default:
			return EAngelscriptCachePointerKind::Invalid;
		}
	}

	static TOptional<FAngelscriptCacheWriterToken> MakeMaintenanceWriterToken()
	{
		const FString Nonce = FGuid::NewGuid()
			.ToString(EGuidFormats::Digits)
			.ToLower();
		return FAngelscriptCacheWriterToken::TryParse(FString::Printf(
			TEXT("%u-%s"),
			FPlatformProcess::GetCurrentProcessId(),
			*Nonce));
	}

	static EAngelscriptCacheDiagnosticApiError ValidateMaintenanceEngine(
		const FAngelscriptEngine* Engine,
		FAngelscriptCacheService*& OutService,
		FString& OutDetail)
	{
		OutService = nullptr;
		if (Engine == nullptr)
		{
			OutDetail = TEXT("AngelScript Engine is not available.");
			return EAngelscriptCacheDiagnosticApiError::EngineUnavailable;
		}
		const UAngelscriptCacheSettings* Settings =
			GetDefault<UAngelscriptCacheSettings>();
		if (Settings == nullptr || !Settings->bEnableCacheV2)
		{
			OutDetail = TEXT("Cache V2 is disabled by project settings.");
			return EAngelscriptCacheDiagnosticApiError::CacheDisabled;
		}
		if (Engine->GetRuntimeConfig().bDisableCacheV2Persistence)
		{
			OutDetail = TEXT("Cache V2 persistence is disabled for this Engine.");
			return EAngelscriptCacheDiagnosticApiError::PersistenceDisabled;
		}
		OutService = Engine->GetCacheService();
		if (OutService == nullptr)
		{
			OutDetail = TEXT("Cache V2 Service is not available.");
			return EAngelscriptCacheDiagnosticApiError::ServiceUnavailable;
		}
		return EAngelscriptCacheDiagnosticApiError::None;
	}
}

FAngelscriptCacheVerifyApiResult VerifyAngelscriptCacheStore(
	const FAngelscriptEngine* Engine,
	const EAngelscriptCacheDiagnosticGeneration Generation,
	const bool bDeep,
	const double TimeoutSeconds)
{
	using namespace AngelscriptCacheDiagnostics_Private;
	FAngelscriptCacheVerifyApiResult Result;
	Result.Generation = Generation;
	Result.bDeep = bDeep;
	FAngelscriptCacheService* Service = nullptr;
	Result.Error = ValidateMaintenanceEngine(
		Engine, Service, Result.Detail);
	if (Result.Error != EAngelscriptCacheDiagnosticApiError::None)
	{
		return Result;
	}
	const EAngelscriptCachePointerKind PointerKind =
		ToPointerKind(Generation);
	if (PointerKind == EAngelscriptCachePointerKind::Invalid)
	{
		Result.Error = EAngelscriptCacheDiagnosticApiError::InvalidRequest;
		Result.Detail = TEXT("The requested Cache V2 generation slot is invalid.");
		return Result;
	}

	double EffectiveTimeoutSeconds = 0.0;
	if (!ResolveMaintenanceTimeout(TimeoutSeconds, EffectiveTimeoutSeconds))
	{
		Result.Error = EAngelscriptCacheDiagnosticApiError::InvalidRequest;
		Result.Detail = TEXT("Cache V2 verification requires a positive finite timeout.");
		return Result;
	}
	const TSharedPtr<const FAngelscriptCacheSuccessfulPublicationDto,
		ESPMode::ThreadSafe> Publication =
		ResolveStoreCoordinatePublication(*Service);
	if (!Publication.IsValid())
	{
		Result.Error = EAngelscriptCacheDiagnosticApiError::NoMatch;
		Result.Detail = TEXT("No immutable Cache V2 publication supplies Store namespace coordinates.");
		return Result;
	}

	FString RequestedBaseRoot;
	Result.RootSelection = ResolveAngelscriptCacheRequestedBaseRootForEngine(
		*Engine, RequestedBaseRoot);
	if (!Result.RootSelection.IsSuccess())
	{
		Result.Error = EAngelscriptCacheDiagnosticApiError::RootSelectionFailed;
		Result.Detail = TEXT("Cache V2 verification root selection failed.");
		return Result;
	}
	TUniquePtr<IAngelscriptCacheAtomicFileOps> FileOps =
		CreateAngelscriptCacheAtomicFileOps();
	TUniquePtr<IAngelscriptCacheNamespaceLockOps> LockOps =
		CreateAngelscriptCacheNamespaceLockOps();
	if (!FileOps.IsValid() || !LockOps.IsValid())
	{
		Result.Error = EAngelscriptCacheDiagnosticApiError::StoreUnavailable;
		Result.Detail = TEXT("Cache V2 verification platform Store dependencies are unavailable.");
		return Result;
	}

	FAngelscriptCacheStorePaths Paths;
	Result.Store = BuildAngelscriptCacheStorePaths(
		RequestedBaseRoot,
		Publication->Compatibility,
		Publication->Context,
		*FileOps,
		Paths);
	if (!Result.Store.IsSuccess())
	{
		Result.Error = EAngelscriptCacheDiagnosticApiError::VerifyFailed;
		Result.Detail = TEXT("Cache V2 verification could not build validated Store paths.");
		return Result;
	}
	const double DeadlineSeconds =
		FPlatformTime::Seconds() + EffectiveTimeoutSeconds;
	auto IsCancellationRequested = [DeadlineSeconds]()
	{
		return FPlatformTime::Seconds() >= DeadlineSeconds;
	};
	TUniquePtr<IAngelscriptCacheNamespaceLockHandle> NamespaceLock;
	Result.Store = AcquireAngelscriptCacheNamespaceLock(
		Paths,
		DeadlineSeconds,
		IsCancellationRequested,
		*LockOps,
		NamespaceLock);
	if (!Result.Store.IsSuccess())
	{
		Result.Error = EAngelscriptCacheDiagnosticApiError::VerifyFailed;
		Result.Detail = TEXT("Cache V2 verification could not acquire the namespace lock.");
		return Result;
	}
	Result.Store = ReadAngelscriptCachePointerSlot(
		Paths, PointerKind, *FileOps, Result.GenerationId);
	if (!Result.Store.IsSuccess())
	{
		Result.Error = EAngelscriptCacheDiagnosticApiError::VerifyFailed;
		Result.Detail = TEXT("Cache V2 verification could not parse the requested generation pointer.");
		return Result;
	}
	if (!Result.GenerationId.IsSet())
	{
		Result.Error = EAngelscriptCacheDiagnosticApiError::NoMatch;
		Result.Detail = TEXT("The requested Cache V2 generation slot is empty.");
		return Result;
	}

	const FAngelscriptCacheReadLimits Limits;
	if (!bDeep)
	{
		TArray<uint8> ManifestBytes;
		Result.Store = FileOps->ReopenReadAll(
			Paths.BuildManifestPath(Result.GenerationId.GetValue()),
			Limits.MaxManifestBytes,
			ManifestBytes);
		if (!Result.Store.IsSuccess())
		{
			Result.Error = EAngelscriptCacheDiagnosticApiError::VerifyFailed;
			Result.Detail = TEXT("Cache V2 shallow verification could not read the selected Manifest.");
			return Result;
		}
		const FAngelscriptHash256 ActualGenerationId{
			FBlake3::HashBuffer(ManifestBytes.GetData(), ManifestBytes.Num())};
		if (ActualGenerationId != Result.GenerationId.GetValue())
		{
			Result.Store = FAngelscriptCacheStoreResult::Failure(
				EAngelscriptCacheStoreError::ContentValidationFailed,
				EAngelscriptCacheStoreStage::CandidateValidation,
				EAngelscriptCacheStorePathCategory::Manifest);
			Result.Store.ContentValidation =
				FAngelscriptCacheValidationResult::AtStage(
					EAngelscriptCacheValidationError::GenerationIdMismatch,
					static_cast<EAngelscriptCacheRecordKind>(0),
					EAngelscriptCacheValidationStage::ManifestDecode);
			Result.Error = EAngelscriptCacheDiagnosticApiError::VerifyFailed;
			Result.Detail = TEXT("Cache V2 shallow verification found a Manifest generation hash mismatch.");
			return Result;
		}
		Result.StoredBytesRead = static_cast<uint64>(ManifestBytes.Num());
		Result.Detail = TEXT("Cache V2 shallow pointer and Manifest identity verification succeeded.");
		return Result;
	}

	FAngelscriptCacheReadBudget Budget;
	FAngelscriptUnrealZlibCacheStorageCodec Codec;
	TOptional<FAngelscriptValidatedGeneration> Validated;
	Result.Store = ReadAndValidateAngelscriptCacheGenerationUnderLock(
		Paths,
		Result.GenerationId.GetValue(),
		Limits,
		Budget,
		Codec,
		*NamespaceLock,
		*FileOps,
		Validated);
	Result.StoredBytesRead = Budget.GetStoredBytes();
	Result.DecompressedBytesRead = Budget.GetDecompressedBytes();
	Result.DecodedBytesRetained = Budget.GetDecodedBytes();
	if (!Result.Store.IsSuccess() || !Validated.IsSet())
	{
		Result.Error = EAngelscriptCacheDiagnosticApiError::VerifyFailed;
		Result.Detail = TEXT("Cache V2 deep Manifest/Pack/record graph verification failed.");
		return Result;
	}
	Result.ManifestModuleCount = Validated->Manifest.ModuleSnapshots.Num();
	Result.ManifestRecordCount = Validated->Manifest.Records.Num();
	Result.ReachableRecordCount = Validated->ReachableRecords.Num();
	TArray<FAngelscriptHash256> PackIds;
	for (const FAngelscriptCacheRecordIndexEntry& Entry :
		Validated->Manifest.Records)
	{
		PackIds.AddUnique(Entry.Location.PackId);
	}
	Result.ReferencedPackCount = PackIds.Num();
	Result.Detail = TEXT("Cache V2 deep Manifest/Pack/record graph verification succeeded.");
	return Result;
}

FAngelscriptCacheCompactApiResult CompactAngelscriptCacheStoreForEngine(
	const FAngelscriptEngine* Engine,
	const double TimeoutSeconds)
{
	using namespace AngelscriptCacheDiagnostics_Private;
	FAngelscriptCacheCompactApiResult Result;
	FAngelscriptCacheService* Service = nullptr;
	Result.Error = ValidateMaintenanceEngine(
		Engine, Service, Result.Detail);
	if (Result.Error != EAngelscriptCacheDiagnosticApiError::None)
	{
		return Result;
	}
	double EffectiveTimeoutSeconds = 0.0;
	if (!ResolveMaintenanceTimeout(TimeoutSeconds, EffectiveTimeoutSeconds))
	{
		Result.Error = EAngelscriptCacheDiagnosticApiError::InvalidRequest;
		Result.Detail = TEXT("Cache V2 compaction requires a positive finite timeout.");
		return Result;
	}
	const FAngelscriptCacheLifecyclePublications Publications =
		Service->GetLifecyclePublications();
	if (!Publications.Current.IsValid())
	{
		Result.Error = EAngelscriptCacheDiagnosticApiError::NoMatch;
		Result.Detail = TEXT("Cache V2 compaction requires an immutable Current publication authority.");
		return Result;
	}
	const TSharedPtr<const FAngelscriptCacheSuccessfulPublicationDto,
		ESPMode::ThreadSafe>& Publication = Publications.Current;

	FString RequestedBaseRoot;
	Result.RootSelection = ResolveAngelscriptCacheRequestedBaseRootForEngine(
		*Engine, RequestedBaseRoot);
	if (!Result.RootSelection.IsSuccess())
	{
		Result.Error = EAngelscriptCacheDiagnosticApiError::RootSelectionFailed;
		Result.Detail = TEXT("Cache V2 compaction root selection failed.");
		return Result;
	}
	TUniquePtr<IAngelscriptCacheAtomicFileOps> FileOps =
		CreateAngelscriptCacheAtomicFileOps();
	TUniquePtr<IAngelscriptCacheNamespaceLockOps> LockOps =
		CreateAngelscriptCacheNamespaceLockOps();
	const TOptional<FAngelscriptCacheWriterToken> WriterToken =
		MakeMaintenanceWriterToken();
	if (!FileOps.IsValid() || !LockOps.IsValid() || !WriterToken.IsSet())
	{
		Result.Error = EAngelscriptCacheDiagnosticApiError::StoreUnavailable;
		Result.Detail = TEXT("Cache V2 compaction platform Store dependencies are unavailable.");
		return Result;
	}
	FAngelscriptCacheStorePaths Paths;
	Result.Store = BuildAngelscriptCacheStorePaths(
		RequestedBaseRoot,
		Publication->Compatibility,
		Publication->Context,
		*FileOps,
		Paths);
	if (!Result.Store.IsSuccess())
	{
		Result.Error = EAngelscriptCacheDiagnosticApiError::CompactionFailed;
		Result.Detail = TEXT("Cache V2 compaction could not build validated Store paths.");
		return Result;
	}

	const double DeadlineSeconds =
		FPlatformTime::Seconds() + EffectiveTimeoutSeconds;
	auto IsCancellationRequested = [DeadlineSeconds]()
	{
		return FPlatformTime::Seconds() >= DeadlineSeconds;
	};
	FAngelscriptCacheCompactionAuthority Authority;
	Authority.Profile = Publication->Profile;
	Authority.SourceSnapshot = Publication->SourceSnapshot;
	FAngelscriptCachePackPolicy PackPolicy;
	FAngelscriptCacheReadLimits Limits;
	FAngelscriptCacheReadBudget Budget;
	FAngelscriptUnrealZlibCacheStorageCodec Codec;
	Result.Store = CompactAngelscriptCacheStore(
		Paths,
		Authority,
		WriterToken.GetValue(),
		PackPolicy,
		Limits,
		Budget,
		DeadlineSeconds,
		IsCancellationRequested,
		Codec,
		*LockOps,
		*FileOps);
	if (!Result.Store.IsSuccess())
	{
		Result.Error = EAngelscriptCacheDiagnosticApiError::CompactionFailed;
		Result.Detail = FString::Printf(
			TEXT("Cache V2 compaction failed: StoreError=%u Stage=%u Commit=%u."),
			static_cast<uint32>(Result.Store.Error),
			static_cast<uint32>(Result.Store.Stage),
			static_cast<uint32>(Result.Store.CommitState));
		return Result;
	}
	Result.Detail = TEXT("Cache V2 two-phase compaction and sweep succeeded.");
	return Result;
}

FAngelscriptCacheForceCleanApiResult ForceCleanAngelscriptCache(
	FAngelscriptEngine* Engine,
	FString ModuleSelector)
{
	using namespace AngelscriptCacheDiagnostics_Private;
	FAngelscriptCacheForceCleanApiResult Result;
	FAngelscriptCacheService* Service = nullptr;
	Result.Error = ValidateMaintenanceEngine(
		Engine, Service, Result.Detail);
	if (Result.Error != EAngelscriptCacheDiagnosticApiError::None)
	{
		return Result;
	}
	ModuleSelector.TrimStartAndEndInline();
	if (ModuleSelector.IsEmpty())
	{
		for (const TSharedRef<FAngelscriptModuleDesc>& Module :
			Engine->GetActiveModules())
		{
			Result.SelectedModuleNames.AddUnique(Module->ModuleName);
		}
	}
	else
	{
		FString CanonicalModuleName;
		if (ModuleSelector.Len() == 64)
		{
			bool bIsFullKey = true;
			for (const TCHAR Character : ModuleSelector)
			{
				if (!FChar::IsHexDigit(Character))
				{
					bIsFullKey = false;
					break;
				}
			}
			if (bIsFullKey)
			{
				const TSharedPtr<const FAngelscriptCacheSuccessfulPublicationDto,
					ESPMode::ThreadSafe> Publication =
					ResolveStoreCoordinatePublication(*Service);
				if (Publication.IsValid())
				{
					for (const FAngelscriptCacheCleanModuleArtifacts& Module :
						Publication->Modules)
					{
						if (Module.ModuleKey.Hash.ToHexString().Equals(
							ModuleSelector, ESearchCase::IgnoreCase))
						{
							CanonicalModuleName = Module.CanonicalModuleName;
							break;
						}
					}
				}
			}
		}
		if (CanonicalModuleName.IsEmpty())
		{
			const TSharedPtr<FAngelscriptModuleDesc> Module =
				Engine->GetModuleByModuleName(ModuleSelector);
			if (Module.IsValid())
			{
				CanonicalModuleName = Module->ModuleName;
			}
		}
		if (!CanonicalModuleName.IsEmpty())
		{
			Result.SelectedModuleNames.Add(CanonicalModuleName);
		}
	}
	Result.SelectedModuleNames.Sort();
	if (Result.SelectedModuleNames.IsEmpty())
	{
		Result.Error = EAngelscriptCacheDiagnosticApiError::NoMatch;
		Result.Detail = ModuleSelector.IsEmpty()
			? TEXT("No active AngelScript module is available for forced-clean compilation.")
			: TEXT("The forced-clean module selector matched no active module.");
		return Result;
	}

	ECompileResult CompileResult = ECompileResult::Error;
	if (!Engine->ForceCleanCacheModules(
		Result.SelectedModuleNames, CompileResult))
	{
		Result.Error = EAngelscriptCacheDiagnosticApiError::ForceCleanFailed;
		Result.Detail = TEXT("The authoritative forced-clean transaction could not start at this Engine safe point.");
		return Result;
	}
	switch (CompileResult)
	{
	case ECompileResult::FullyHandled:
	case ECompileResult::PartiallyHandled:
		Result.Outcome = EAngelscriptCacheForceCleanOutcome::Applied;
		Result.Detail = TEXT("The authoritative forced-clean compile transaction completed.");
		break;
	case ECompileResult::ErrorNeedFullReload:
		Result.Error = EAngelscriptCacheDiagnosticApiError::ForceCleanFailed;
		Result.Outcome = EAngelscriptCacheForceCleanOutcome::RequiresFullReload;
		Result.Detail = TEXT("Forced-clean compilation requires a full reload; normal last-good policy was retained.");
		break;
	case ECompileResult::Error:
	default:
		Result.Error = EAngelscriptCacheDiagnosticApiError::ForceCleanFailed;
		Result.Outcome = EAngelscriptCacheForceCleanOutcome::CompileFailed;
		Result.Detail = TEXT("Forced-clean compilation failed; normal last-good policy was retained.");
		break;
	}
	return Result;
}
