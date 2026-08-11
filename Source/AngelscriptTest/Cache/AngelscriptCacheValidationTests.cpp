#include "Cache/AngelscriptCacheTypes.h"

#include "CQTest.h"

#include <type_traits>

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FAngelscriptCacheValidationTests,
	"Angelscript.TestModule.Cache.Validation.Result",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	TEST_METHOD(AppendedErrorsAndDiagnosticStagesAreNumericallyFrozen)
	{
		const EAngelscriptCacheValidationStage Stages[] = {
			EAngelscriptCacheValidationStage::None,
			EAngelscriptCacheValidationStage::EnvelopeDecode,
			EAngelscriptCacheValidationStage::PayloadDecode,
			EAngelscriptCacheValidationStage::LocalSemantic,
			EAngelscriptCacheValidationStage::OpaqueCodec,
			EAngelscriptCacheValidationStage::ModuleGraph,
			EAngelscriptCacheValidationStage::CurrentResolver,
		};
		for (int32 Index = 0; Index < UE_ARRAY_COUNT(Stages); ++Index)
		{
			ASSERT_THAT(AreEqual(Index, static_cast<int32>(Stages[Index])));
		}

		const EAngelscriptCacheValidationError AppendedErrors[] = {
			EAngelscriptCacheValidationError::UnsupportedCodecVersion,
			EAngelscriptCacheValidationError::OpaquePayloadMalformed,
			EAngelscriptCacheValidationError::OpaquePayloadHashMismatch,
			EAngelscriptCacheValidationError::RelocationDependencyMismatch,
			EAngelscriptCacheValidationError::WrongRecordKind,
			EAngelscriptCacheValidationError::MissingRecord,
			EAngelscriptCacheValidationError::MissingCoverage,
			EAngelscriptCacheValidationError::UnexpectedRecord,
			EAngelscriptCacheValidationError::UndeclaredEntity,
			EAngelscriptCacheValidationError::DuplicateDebugOwner,
			EAngelscriptCacheValidationError::DebugLinkMismatch,
			EAngelscriptCacheValidationError::EnumAuthorityMismatch,
			EAngelscriptCacheValidationError::InitializerOwnershipMismatch,
			EAngelscriptCacheValidationError::GlobalCoverageMismatch,
			EAngelscriptCacheValidationError::ProfileGraphMismatch,
			EAngelscriptCacheValidationError::SourceGraphMismatch,
			EAngelscriptCacheValidationError::GraphAbiMismatch,
			EAngelscriptCacheValidationError::InvocationKindMismatch,
			EAngelscriptCacheValidationError::DebugSourceMismatch,
			EAngelscriptCacheValidationError::CurrentContentMismatch,
			EAngelscriptCacheValidationError::CurrentSymbolMissing,
		};
		for (int32 Index = 0; Index < UE_ARRAY_COUNT(AppendedErrors); ++Index)
		{
			ASSERT_THAT(AreEqual(Index + 44, static_cast<int32>(AppendedErrors[Index])));
		}
	}

	TEST_METHOD(AppendedErrorsHaveExhaustiveFrozenClassifications)
	{
		const EAngelscriptCacheValidationError CodecErrors[] = {
			EAngelscriptCacheValidationError::UnsupportedCodecVersion,
			EAngelscriptCacheValidationError::OpaquePayloadMalformed,
			EAngelscriptCacheValidationError::OpaquePayloadHashMismatch,
		};
		for (const EAngelscriptCacheValidationError Error : CodecErrors)
		{
			ASSERT_THAT(AreEqual(EAngelscriptCacheValidationClass::CodecOrIntegrity,
				FAngelscriptCacheValidationResult::Classify(Error)));
			ASSERT_THAT(AreEqual(EAngelscriptCacheValidationClass::CodecOrIntegrity,
				FAngelscriptCacheValidationResult(Error).Class));
		}

		const EAngelscriptCacheValidationError GraphErrors[] = {
			EAngelscriptCacheValidationError::RelocationDependencyMismatch,
			EAngelscriptCacheValidationError::WrongRecordKind,
			EAngelscriptCacheValidationError::MissingRecord,
			EAngelscriptCacheValidationError::MissingCoverage,
			EAngelscriptCacheValidationError::UnexpectedRecord,
			EAngelscriptCacheValidationError::UndeclaredEntity,
			EAngelscriptCacheValidationError::DuplicateDebugOwner,
			EAngelscriptCacheValidationError::DebugLinkMismatch,
			EAngelscriptCacheValidationError::EnumAuthorityMismatch,
			EAngelscriptCacheValidationError::InitializerOwnershipMismatch,
			EAngelscriptCacheValidationError::GlobalCoverageMismatch,
			EAngelscriptCacheValidationError::ProfileGraphMismatch,
			EAngelscriptCacheValidationError::SourceGraphMismatch,
			EAngelscriptCacheValidationError::GraphAbiMismatch,
			EAngelscriptCacheValidationError::InvocationKindMismatch,
			EAngelscriptCacheValidationError::DebugSourceMismatch,
		};
		for (const EAngelscriptCacheValidationError Error : GraphErrors)
		{
			ASSERT_THAT(AreEqual(EAngelscriptCacheValidationClass::GraphOrOwnership,
				FAngelscriptCacheValidationResult::Classify(Error)));
			ASSERT_THAT(AreEqual(EAngelscriptCacheValidationClass::GraphOrOwnership,
				FAngelscriptCacheValidationResult(Error).Class));
		}

		const EAngelscriptCacheValidationError IneligibleErrors[] = {
			EAngelscriptCacheValidationError::CurrentContentMismatch,
			EAngelscriptCacheValidationError::CurrentSymbolMissing,
		};
		for (const EAngelscriptCacheValidationError Error : IneligibleErrors)
		{
			ASSERT_THAT(AreEqual(EAngelscriptCacheValidationClass::Ineligible,
				FAngelscriptCacheValidationResult::Classify(Error)));
			ASSERT_THAT(AreEqual(EAngelscriptCacheValidationClass::Ineligible,
				FAngelscriptCacheValidationResult(Error).Class));
		}
	}

	TEST_METHOD(LegacyConstructorAndNamedStageFactoryCannotSwapStageAndOffset)
	{
		static_assert(std::is_constructible_v<FAngelscriptCacheValidationResult,
			EAngelscriptCacheValidationError, EAngelscriptCacheRecordKind, uint64>);
		static_assert(!std::is_constructible_v<FAngelscriptCacheValidationResult,
			EAngelscriptCacheValidationError, EAngelscriptCacheRecordKind,
			EAngelscriptCacheValidationStage>);
		static_assert(!std::is_constructible_v<FAngelscriptCacheValidationResult,
			EAngelscriptCacheValidationError, EAngelscriptCacheRecordKind,
			EAngelscriptCacheValidationStage, uint64>);

		const FAngelscriptCacheValidationResult Legacy(
			EAngelscriptCacheValidationError::UnsupportedPayloadSchema,
			EAngelscriptCacheRecordKind::TypeSchema,
			UINT64_C(37));
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::UnsupportedPayloadSchema,
			Legacy.Error));
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationClass::Malformed, Legacy.Class));
		ASSERT_THAT(AreEqual(EAngelscriptCacheRecordKind::TypeSchema, Legacy.RecordKind));
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationStage::None, Legacy.Stage));
		ASSERT_THAT(AreEqual(UINT64_C(37), Legacy.ByteOffset));

		const FAngelscriptCacheValidationResult Staged =
			FAngelscriptCacheValidationResult::AtStage(
				EAngelscriptCacheValidationError::GraphAbiMismatch,
				EAngelscriptCacheRecordKind::ModuleSnapshot,
				EAngelscriptCacheValidationStage::ModuleGraph,
				UINT64_C(91));
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::GraphAbiMismatch,
			Staged.Error));
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationClass::GraphOrOwnership,
			Staged.Class));
		ASSERT_THAT(AreEqual(EAngelscriptCacheRecordKind::ModuleSnapshot,
			Staged.RecordKind));
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationStage::ModuleGraph,
			Staged.Stage));
		ASSERT_THAT(AreEqual(UINT64_C(91), Staged.ByteOffset));
	}
};

#endif
