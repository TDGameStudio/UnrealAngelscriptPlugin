#include "Cache/AngelscriptCacheSemanticRecords.h"

#include "Async/Async.h"
#include "CQTest.h"

#if WITH_ANGELSCRIPT_UNITTESTS

namespace AngelscriptCachePrimitiveTests_Private
{
	static FAngelscriptHash256 MakeHash(const uint8 Fill)
	{
		FBlake3Hash::ByteArray Bytes{};
		FMemory::Memset(Bytes, Fill, sizeof(Bytes));
		return FAngelscriptHash256{FBlake3Hash(Bytes)};
	}

	static FString Hex(const TConstArrayView<uint8> Bytes)
	{
		return BytesToHexLower(Bytes.GetData(), Bytes.Num());
	}

	static FAngelscriptCachedDataType MakeInt32Type()
	{
		FAngelscriptCachedDataType Type;
		Type.Kind = EAngelscriptCachedDataTypeKind::Primitive;
		Type.Primitive = EAngelscriptCachedPrimitiveType::Int32;
		return Type;
	}

	static FAngelscriptCacheStableReference MakeFunctionReference()
	{
		FAngelscriptCacheStableReference Reference;
		Reference.Kind = EAngelscriptCacheReferenceKind::ScriptFunction;
		Reference.StableKey = MakeHash(0x11);
		Reference.ExpectedAbi = MakeHash(0x22);
		return Reference;
	}

	template <typename ElementType>
	static int32 CalculateArrayReserveCapacityForTests(const int32 RequestedCapacity)
	{
		if (RequestedCapacity <= 0)
		{
			check(RequestedCapacity == 0);
			return 0;
		}

		using FArrayType = TArray<ElementType>;
		typename FArrayType::ElementAllocatorType Allocator;
		if constexpr (TAllocatorTraits<typename FArrayType::AllocatorType>::SupportsElementAlignment)
		{
			return Allocator.CalculateSlackReserve(
				RequestedCapacity, sizeof(ElementType), alignof(ElementType));
		}
		else
		{
			return Allocator.CalculateSlackReserve(
				RequestedCapacity, sizeof(ElementType));
		}
	}

	template <typename ElementType>
	static uint64 CalculateArrayReserveBytesForTests(const int32 RequestedCapacity)
	{
		const int32 ReservedCapacity =
			CalculateArrayReserveCapacityForTests<ElementType>(RequestedCapacity);
		check(ReservedCapacity >= RequestedCapacity);
		return static_cast<uint64>(ReservedCapacity) * sizeof(ElementType);
	}

	template <typename ElementType>
	static int32 FindAllocatorSlackCountForTests()
	{
		for (int32 Candidate = 2; Candidate <= 8192; ++Candidate)
		{
			if (CalculateArrayReserveCapacityForTests<ElementType>(Candidate) > Candidate)
			{
				return Candidate;
			}
		}
		return INDEX_NONE;
	}

}

TEST_CLASS_WITH_FLAGS(FAngelscriptCacheArchivePrimitiveTests,
	"Angelscript.TestModule.Cache.Archive.Primitives",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	TEST_METHOD(WireEnumsFlagsAndValidationClassesAreFrozen)
	{
		ASSERT_THAT(AreEqual(1, static_cast<int32>(EAngelscriptCacheReferenceKind::ScriptModule)));
		ASSERT_THAT(AreEqual(2, static_cast<int32>(EAngelscriptCacheReferenceKind::ScriptType)));
		ASSERT_THAT(AreEqual(3, static_cast<int32>(EAngelscriptCacheReferenceKind::ScriptFunction)));
		ASSERT_THAT(AreEqual(4, static_cast<int32>(EAngelscriptCacheReferenceKind::ScriptGlobal)));
		ASSERT_THAT(AreEqual(5, static_cast<int32>(EAngelscriptCacheReferenceKind::ScriptProperty)));
		ASSERT_THAT(AreEqual(6, static_cast<int32>(EAngelscriptCacheReferenceKind::ScriptImport)));
		ASSERT_THAT(AreEqual(7, static_cast<int32>(EAngelscriptCacheReferenceKind::EnvironmentSymbol)));
		ASSERT_THAT(AreEqual(8, static_cast<int32>(EAngelscriptCacheReferenceKind::CanonicalName)));
		ASSERT_THAT(AreEqual(9, static_cast<int32>(EAngelscriptCacheReferenceKind::StringLiteral)));
		ASSERT_THAT(AreEqual(1, static_cast<int32>(EAngelscriptCachedDataTypeKind::Primitive)));
		ASSERT_THAT(AreEqual(4, static_cast<int32>(EAngelscriptCachedDataTypeKind::Auto)));
		ASSERT_THAT(AreEqual(1, static_cast<int32>(EAngelscriptCachedPrimitiveType::Void)));
		ASSERT_THAT(AreEqual(12, static_cast<int32>(EAngelscriptCachedPrimitiveType::Float64)));
		ASSERT_THAT(AreEqual(1, static_cast<int32>(EAngelscriptCacheDeclarationKind::Type)));
		ASSERT_THAT(AreEqual(4, static_cast<int32>(EAngelscriptCacheDeclarationKind::Property)));
		ASSERT_THAT(AreEqual(1, static_cast<int32>(EAngelscriptCacheSchemaCoverage::Forbidden)));
		ASSERT_THAT(AreEqual(2, static_cast<int32>(EAngelscriptCacheSchemaCoverage::Required)));
		ASSERT_THAT(AreEqual(1, static_cast<int32>(EAngelscriptCacheBodyCoverage::Forbidden)));
		ASSERT_THAT(AreEqual(2, static_cast<int32>(EAngelscriptCacheBodyCoverage::Required)));
		ASSERT_THAT(AreEqual(4, static_cast<int32>(EAngelscriptCacheDeclarationSlotKind::Import)));
		ASSERT_THAT(AreEqual(4, static_cast<int32>(EAngelscriptCachedParameterPassing::InOutReference)));
		const EAngelscriptCacheSemanticDependencyKind DependencyKinds[] = {
			EAngelscriptCacheSemanticDependencyKind::Import,
			EAngelscriptCacheSemanticDependencyKind::Declaration,
			EAngelscriptCacheSemanticDependencyKind::Signature,
			EAngelscriptCacheSemanticDependencyKind::Inheritance,
			EAngelscriptCacheSemanticDependencyKind::ValueLayout,
			EAngelscriptCacheSemanticDependencyKind::PropertyLayout,
			EAngelscriptCacheSemanticDependencyKind::GlobalStorage,
			EAngelscriptCacheSemanticDependencyKind::HardValue,
			EAngelscriptCacheSemanticDependencyKind::Initializer,
			EAngelscriptCacheSemanticDependencyKind::CompileOption,
			EAngelscriptCacheSemanticDependencyKind::EnvironmentAbi,
			EAngelscriptCacheSemanticDependencyKind::FunctionContent};
		for (int32 Index = 0; Index < UE_ARRAY_COUNT(DependencyKinds); ++Index)
		{
			ASSERT_THAT(AreEqual(Index + 1, static_cast<int32>(DependencyKinds[Index])));
		}
		ASSERT_THAT(AreEqual(4, static_cast<int32>(EAngelscriptCachePreprocessorInputKind::GeneratedSource)));
		ASSERT_THAT(AreEqual(3, static_cast<int32>(EAngelscriptCachedSourceKind::Memory)));
		ASSERT_THAT(AreEqual(4, static_cast<int32>(EAngelscriptCachedSourceProviderKind::External)));
		ASSERT_THAT(AreEqual(3, static_cast<int32>(EAngelscriptCachedPreprocessHookPhase::External)));
		ASSERT_THAT(AreEqual(2, static_cast<int32>(EAngelscriptCachedSourceEdgeKind::GeneratedSource)));
		ASSERT_THAT(AreEqual(5, static_cast<int32>(EAngelscriptCachedFastPathScopeKind::Module)));
		ASSERT_THAT(AreEqual(5, static_cast<int32>(EAngelscriptCachedFastPathIneligibleReason::UnknownHookBehavior)));
		ASSERT_THAT(AreEqual(5, static_cast<int32>(EAngelscriptCachePreprocessorInputTargetKind::GeneratedSource)));
		ASSERT_THAT(AreEqual(6, static_cast<int32>(EAngelscriptArtifactEntityKind::Typedef)));
		ASSERT_THAT(AreEqual(7, static_cast<int32>(EAngelscriptArtifactEntityKind::Funcdef)));

		const int32 DataTypeKinds[] = {
			static_cast<int32>(EAngelscriptCachedDataTypeKind::Primitive),
			static_cast<int32>(EAngelscriptCachedDataTypeKind::ScriptType),
			static_cast<int32>(EAngelscriptCachedDataTypeKind::EnvironmentType),
			static_cast<int32>(EAngelscriptCachedDataTypeKind::Auto)};
		for (int32 Index = 0; Index < UE_ARRAY_COUNT(DataTypeKinds); ++Index)
		{
			ASSERT_THAT(AreEqual(Index + 1, DataTypeKinds[Index]));
		}
		const int32 PrimitiveTypes[] = {
			static_cast<int32>(EAngelscriptCachedPrimitiveType::Void),
			static_cast<int32>(EAngelscriptCachedPrimitiveType::Bool),
			static_cast<int32>(EAngelscriptCachedPrimitiveType::Int8),
			static_cast<int32>(EAngelscriptCachedPrimitiveType::Int16),
			static_cast<int32>(EAngelscriptCachedPrimitiveType::Int32),
			static_cast<int32>(EAngelscriptCachedPrimitiveType::Int64),
			static_cast<int32>(EAngelscriptCachedPrimitiveType::UInt8),
			static_cast<int32>(EAngelscriptCachedPrimitiveType::UInt16),
			static_cast<int32>(EAngelscriptCachedPrimitiveType::UInt32),
			static_cast<int32>(EAngelscriptCachedPrimitiveType::UInt64),
			static_cast<int32>(EAngelscriptCachedPrimitiveType::Float32),
			static_cast<int32>(EAngelscriptCachedPrimitiveType::Float64)};
		for (int32 Index = 0; Index < UE_ARRAY_COUNT(PrimitiveTypes); ++Index)
		{
			ASSERT_THAT(AreEqual(Index + 1, PrimitiveTypes[Index]));
		}
		const int32 DeclarationKinds[] = {
			static_cast<int32>(EAngelscriptCacheDeclarationKind::Type),
			static_cast<int32>(EAngelscriptCacheDeclarationKind::Function),
			static_cast<int32>(EAngelscriptCacheDeclarationKind::Global),
			static_cast<int32>(EAngelscriptCacheDeclarationKind::Property)};
		const int32 SlotKinds[] = {
			static_cast<int32>(EAngelscriptCacheDeclarationSlotKind::Declaration),
			static_cast<int32>(EAngelscriptCacheDeclarationSlotKind::Function),
			static_cast<int32>(EAngelscriptCacheDeclarationSlotKind::VirtualFunction),
			static_cast<int32>(EAngelscriptCacheDeclarationSlotKind::Import)};
		const int32 PassingKinds[] = {
			static_cast<int32>(EAngelscriptCachedParameterPassing::Value),
			static_cast<int32>(EAngelscriptCachedParameterPassing::InReference),
			static_cast<int32>(EAngelscriptCachedParameterPassing::OutReference),
			static_cast<int32>(EAngelscriptCachedParameterPassing::InOutReference)};
		const int32 InputKinds[] = {
			static_cast<int32>(EAngelscriptCachePreprocessorInputKind::IncludeFile),
			static_cast<int32>(EAngelscriptCachePreprocessorInputKind::Define),
			static_cast<int32>(EAngelscriptCachePreprocessorInputKind::ConditionalSymbol),
			static_cast<int32>(EAngelscriptCachePreprocessorInputKind::GeneratedSource)};
		for (const int32* Values : {DeclarationKinds, SlotKinds, PassingKinds, InputKinds})
		{
			for (int32 Index = 0; Index < 4; ++Index) { ASSERT_THAT(AreEqual(Index + 1, Values[Index])); }
		}
		const int32 SourceKinds[] = {
			static_cast<int32>(EAngelscriptCachedSourceKind::Game),
			static_cast<int32>(EAngelscriptCachedSourceKind::Plugin),
			static_cast<int32>(EAngelscriptCachedSourceKind::Memory)};
		const int32 HookPhases[] = {
			static_cast<int32>(EAngelscriptCachedPreprocessHookPhase::ProcessChunks),
			static_cast<int32>(EAngelscriptCachedPreprocessHookPhase::PostProcessCode),
			static_cast<int32>(EAngelscriptCachedPreprocessHookPhase::External)};
		for (const int32* Values : {SourceKinds, HookPhases})
		{
			for (int32 Index = 0; Index < 3; ++Index) { ASSERT_THAT(AreEqual(Index + 1, Values[Index])); }
		}
		const int32 ProviderKinds[] = {
			static_cast<int32>(EAngelscriptCachedSourceProviderKind::BuiltInDisk),
			static_cast<int32>(EAngelscriptCachedSourceProviderKind::Memory),
			static_cast<int32>(EAngelscriptCachedSourceProviderKind::Generated),
			static_cast<int32>(EAngelscriptCachedSourceProviderKind::External)};
		for (int32 Index = 0; Index < 4; ++Index) { ASSERT_THAT(AreEqual(Index + 1, ProviderKinds[Index])); }
		const int32 ScopeKinds[] = {
			static_cast<int32>(EAngelscriptCachedFastPathScopeKind::Mount),
			static_cast<int32>(EAngelscriptCachedFastPathScopeKind::Provider),
			static_cast<int32>(EAngelscriptCachedFastPathScopeKind::Hook),
			static_cast<int32>(EAngelscriptCachedFastPathScopeKind::SourceFile),
			static_cast<int32>(EAngelscriptCachedFastPathScopeKind::Module)};
		const int32 IneligibleReasons[] = {
			static_cast<int32>(EAngelscriptCachedFastPathIneligibleReason::MissingStableIdentity),
			static_cast<int32>(EAngelscriptCachedFastPathIneligibleReason::MissingVersionFingerprint),
			static_cast<int32>(EAngelscriptCachedFastPathIneligibleReason::MissingConfigurationFingerprint),
			static_cast<int32>(EAngelscriptCachedFastPathIneligibleReason::UnstableGeneratedSource),
			static_cast<int32>(EAngelscriptCachedFastPathIneligibleReason::UnknownHookBehavior)};
		for (const int32* Values : {ScopeKinds, IneligibleReasons})
		{
			for (int32 Index = 0; Index < 5; ++Index) { ASSERT_THAT(AreEqual(Index + 1, Values[Index])); }
		}
		ASSERT_THAT(AreEqual(0, static_cast<int32>(EAngelscriptCachePreprocessorInputTargetKind::None)));
		ASSERT_THAT(AreEqual(1, static_cast<int32>(EAngelscriptCachePreprocessorInputTargetKind::SourceFile)));
		ASSERT_THAT(AreEqual(2, static_cast<int32>(EAngelscriptCachePreprocessorInputTargetKind::Provider)));
		ASSERT_THAT(AreEqual(3, static_cast<int32>(EAngelscriptCachePreprocessorInputTargetKind::Hook)));
		ASSERT_THAT(AreEqual(4, static_cast<int32>(EAngelscriptCachePreprocessorInputTargetKind::Module)));
		ASSERT_THAT(AreEqual(5, static_cast<int32>(EAngelscriptCachePreprocessorInputTargetKind::GeneratedSource)));

		ASSERT_THAT(AreEqual(0x01u, static_cast<uint32>(EAngelscriptCachedTypeQualifierFlags::Reference)));
		ASSERT_THAT(AreEqual(0x02u, static_cast<uint32>(EAngelscriptCachedTypeQualifierFlags::ObjectConst)));
		ASSERT_THAT(AreEqual(0x04u, static_cast<uint32>(EAngelscriptCachedTypeQualifierFlags::ObjectHandle)));
		ASSERT_THAT(AreEqual(0x08u, static_cast<uint32>(EAngelscriptCachedTypeQualifierFlags::ConstHandle)));
		ASSERT_THAT(AreEqual(0x10u, static_cast<uint32>(EAngelscriptCachedTypeQualifierFlags::Auto)));
		ASSERT_THAT(AreEqual(0x20u, static_cast<uint32>(EAngelscriptCachedTypeQualifierFlags::IfHandleThenConst)));
		ASSERT_THAT(AreEqual(0x3fu, static_cast<uint32>(EAngelscriptCachedTypeQualifierFlags::KnownMask)));
		ASSERT_THAT(AreEqual(0x1u, static_cast<uint32>(EAngelscriptCachedSourceDiscoveryFilterFlags::SkipDevelopment)));
		ASSERT_THAT(AreEqual(0x2u, static_cast<uint32>(EAngelscriptCachedSourceDiscoveryFilterFlags::SkipEditor)));
		ASSERT_THAT(AreEqual(0x3u, static_cast<uint32>(EAngelscriptCachedSourceDiscoveryFilterFlags::KnownMask)));
		ASSERT_THAT(AreEqual(0x1u, static_cast<uint32>(EAngelscriptCachedFingerprintCapabilityFlags::StableIdentity)));
		ASSERT_THAT(AreEqual(0x2u, static_cast<uint32>(EAngelscriptCachedFingerprintCapabilityFlags::VersionFingerprint)));
		ASSERT_THAT(AreEqual(0x4u, static_cast<uint32>(EAngelscriptCachedFingerprintCapabilityFlags::ConfigurationFingerprint)));
		ASSERT_THAT(AreEqual(0x8u, static_cast<uint32>(EAngelscriptCachedFingerprintCapabilityFlags::ContentFingerprint)));
		ASSERT_THAT(AreEqual(0xfu, static_cast<uint32>(EAngelscriptCachedFingerprintCapabilityFlags::KnownMask)));
		const uint32 DeclarationTraitFlags[] = {
			static_cast<uint32>(EAngelscriptCachedDeclarationTraitFlags::Static),
			static_cast<uint32>(EAngelscriptCachedDeclarationTraitFlags::Const),
			static_cast<uint32>(EAngelscriptCachedDeclarationTraitFlags::Private),
			static_cast<uint32>(EAngelscriptCachedDeclarationTraitFlags::Protected),
			static_cast<uint32>(EAngelscriptCachedDeclarationTraitFlags::ThreadSafe),
			static_cast<uint32>(EAngelscriptCachedDeclarationTraitFlags::Abstract),
			static_cast<uint32>(EAngelscriptCachedDeclarationTraitFlags::Final),
			static_cast<uint32>(EAngelscriptCachedDeclarationTraitFlags::Override),
			static_cast<uint32>(EAngelscriptCachedDeclarationTraitFlags::Generated),
			static_cast<uint32>(EAngelscriptCachedDeclarationTraitFlags::Shared),
			static_cast<uint32>(EAngelscriptCachedDeclarationTraitFlags::External),
			static_cast<uint32>(EAngelscriptCachedDeclarationTraitFlags::Property),
			static_cast<uint32>(EAngelscriptCachedDeclarationTraitFlags::ImplicitConstructor),
			static_cast<uint32>(EAngelscriptCachedDeclarationTraitFlags::Mixin),
			static_cast<uint32>(EAngelscriptCachedDeclarationTraitFlags::Local),
			static_cast<uint32>(EAngelscriptCachedDeclarationTraitFlags::NoDiscard),
			static_cast<uint32>(EAngelscriptCachedDeclarationTraitFlags::Deprecated),
			static_cast<uint32>(EAngelscriptCachedDeclarationTraitFlags::GenericTemplateFunction),
			static_cast<uint32>(EAngelscriptCachedDeclarationTraitFlags::UsesWorldContext),
			static_cast<uint32>(EAngelscriptCachedDeclarationTraitFlags::AcceptTemporaryObject),
			static_cast<uint32>(EAngelscriptCachedDeclarationTraitFlags::NotCallable),
			static_cast<uint32>(EAngelscriptCachedDeclarationTraitFlags::ForceConstArgumentExpressions),
			static_cast<uint32>(EAngelscriptCachedDeclarationTraitFlags::ExternalImplicitThis),
			static_cast<uint32>(EAngelscriptCachedDeclarationTraitFlags::AllowDiscard),
			static_cast<uint32>(EAngelscriptCachedDeclarationTraitFlags::EditorOnly),
			static_cast<uint32>(EAngelscriptCachedDeclarationTraitFlags::Explicit),
			static_cast<uint32>(EAngelscriptCachedDeclarationTraitFlags::UnsafeDuringConstruction),
			static_cast<uint32>(EAngelscriptCachedDeclarationTraitFlags::DefaultsOnly),
			static_cast<uint32>(EAngelscriptCachedDeclarationTraitFlags::Constructor),
			static_cast<uint32>(EAngelscriptCachedDeclarationTraitFlags::Destructor)};
		for (int32 Index = 0; Index < UE_ARRAY_COUNT(DeclarationTraitFlags); ++Index)
		{
			ASSERT_THAT(AreEqual(1u << Index, DeclarationTraitFlags[Index]));
		}
		ASSERT_THAT(AreEqual(0x3fffffffu, static_cast<uint32>(EAngelscriptCachedDeclarationTraitFlags::KnownMask)));
		const uint32 ReflectionFlags[] = {
			static_cast<uint32>(EAngelscriptCachedReflectionFlags::BlueprintCallable),
			static_cast<uint32>(EAngelscriptCachedReflectionFlags::BlueprintOverride),
			static_cast<uint32>(EAngelscriptCachedReflectionFlags::BlueprintEvent),
			static_cast<uint32>(EAngelscriptCachedReflectionFlags::BlueprintPure),
			static_cast<uint32>(EAngelscriptCachedReflectionFlags::NetMulticast),
			static_cast<uint32>(EAngelscriptCachedReflectionFlags::NetClient),
			static_cast<uint32>(EAngelscriptCachedReflectionFlags::NetServer),
			static_cast<uint32>(EAngelscriptCachedReflectionFlags::NetValidate),
			static_cast<uint32>(EAngelscriptCachedReflectionFlags::Unreliable),
			static_cast<uint32>(EAngelscriptCachedReflectionFlags::BlueprintAuthorityOnly),
			static_cast<uint32>(EAngelscriptCachedReflectionFlags::Exec),
			static_cast<uint32>(EAngelscriptCachedReflectionFlags::CanOverrideEvent),
			static_cast<uint32>(EAngelscriptCachedReflectionFlags::BlueprintReadable),
			static_cast<uint32>(EAngelscriptCachedReflectionFlags::BlueprintWritable)};
		for (int32 Index = 0; Index < UE_ARRAY_COUNT(ReflectionFlags); ++Index)
		{
			ASSERT_THAT(AreEqual(1u << Index, ReflectionFlags[Index]));
		}
		ASSERT_THAT(AreEqual(0x3fffu, static_cast<uint32>(EAngelscriptCachedReflectionFlags::KnownMask)));
		ASSERT_THAT(AreEqual(0x1u, static_cast<uint32>(EAngelscriptCachedParameterTraitFlags::BlueprintByValue)));
		ASSERT_THAT(AreEqual(0x2u, static_cast<uint32>(EAngelscriptCachedParameterTraitFlags::BlueprintOutRef)));
		ASSERT_THAT(AreEqual(0x4u, static_cast<uint32>(EAngelscriptCachedParameterTraitFlags::BlueprintInRef)));
		ASSERT_THAT(AreEqual(0x7u, static_cast<uint32>(EAngelscriptCachedParameterTraitFlags::KnownMask)));

		const EAngelscriptCacheValidationClass ValidationClasses[] = {
			EAngelscriptCacheValidationClass::Success,
			EAngelscriptCacheValidationClass::Malformed,
			EAngelscriptCacheValidationClass::ArithmeticOrBudget,
			EAngelscriptCacheValidationClass::CodecOrIntegrity,
			EAngelscriptCacheValidationClass::CanonicalSemantic,
			EAngelscriptCacheValidationClass::GraphOrOwnership,
			EAngelscriptCacheValidationClass::Ineligible};
		for (int32 Index = 0; Index < UE_ARRAY_COUNT(ValidationClasses); ++Index)
		{
			ASSERT_THAT(AreEqual(Index, static_cast<int32>(ValidationClasses[Index])));
		}

		struct FValidationExpectation
		{
			EAngelscriptCacheValidationError Error;
			EAngelscriptCacheValidationClass ExpectedClass;
		};
		const FValidationExpectation ValidationExpectations[] = {
			{EAngelscriptCacheValidationError::None, EAngelscriptCacheValidationClass::Success},
			{EAngelscriptCacheValidationError::BadMagic, EAngelscriptCacheValidationClass::Malformed},
			{EAngelscriptCacheValidationError::UnsupportedSchema, EAngelscriptCacheValidationClass::Malformed},
			{EAngelscriptCacheValidationError::UnknownRecordKind, EAngelscriptCacheValidationClass::Malformed},
			{EAngelscriptCacheValidationError::NonZeroReserved, EAngelscriptCacheValidationClass::Malformed},
			{EAngelscriptCacheValidationError::Overflow, EAngelscriptCacheValidationClass::ArithmeticOrBudget},
			{EAngelscriptCacheValidationError::BudgetExceeded, EAngelscriptCacheValidationClass::ArithmeticOrBudget},
			{EAngelscriptCacheValidationError::OutOfBounds, EAngelscriptCacheValidationClass::ArithmeticOrBudget},
			{EAngelscriptCacheValidationError::ChecksumMismatch, EAngelscriptCacheValidationClass::CodecOrIntegrity},
			{EAngelscriptCacheValidationError::TrailingData, EAngelscriptCacheValidationClass::Malformed},
			{EAngelscriptCacheValidationError::InvalidArrayView, EAngelscriptCacheValidationClass::Malformed},
			{EAngelscriptCacheValidationError::AliasedInputOutput, EAngelscriptCacheValidationClass::Malformed},
			{EAngelscriptCacheValidationError::UnsupportedPayloadSchema, EAngelscriptCacheValidationClass::Malformed},
			{EAngelscriptCacheValidationError::UnknownEnumValue, EAngelscriptCacheValidationClass::Malformed},
			{EAngelscriptCacheValidationError::UnknownFlags, EAngelscriptCacheValidationClass::Malformed},
			{EAngelscriptCacheValidationError::InvalidBoolean, EAngelscriptCacheValidationClass::Malformed},
			{EAngelscriptCacheValidationError::InvalidOptionalTag, EAngelscriptCacheValidationClass::Malformed},
			{EAngelscriptCacheValidationError::InvalidUtf8, EAngelscriptCacheValidationClass::Malformed},
			{EAngelscriptCacheValidationError::EmbeddedNul, EAngelscriptCacheValidationClass::Malformed},
			{EAngelscriptCacheValidationError::InvalidLogicalPath, EAngelscriptCacheValidationClass::Malformed},
			{EAngelscriptCacheValidationError::ImpossibleCount, EAngelscriptCacheValidationClass::ArithmeticOrBudget},
			{EAngelscriptCacheValidationError::NestingDepthExceeded, EAngelscriptCacheValidationClass::ArithmeticOrBudget},
			{EAngelscriptCacheValidationError::RecordIdMismatch, EAngelscriptCacheValidationClass::CodecOrIntegrity},
			{EAngelscriptCacheValidationError::NonCanonicalOrder, EAngelscriptCacheValidationClass::CanonicalSemantic},
			{EAngelscriptCacheValidationError::DuplicateKey, EAngelscriptCacheValidationClass::CanonicalSemantic},
			{EAngelscriptCacheValidationError::ConflictingKey, EAngelscriptCacheValidationClass::CanonicalSemantic},
			{EAngelscriptCacheValidationError::CaseCollision, EAngelscriptCacheValidationClass::CanonicalSemantic},
			{EAngelscriptCacheValidationError::ZeroStableKey, EAngelscriptCacheValidationClass::CanonicalSemantic},
			{EAngelscriptCacheValidationError::MissingExpectedAbi, EAngelscriptCacheValidationClass::CanonicalSemantic},
			{EAngelscriptCacheValidationError::ForbiddenExpectedAbi, EAngelscriptCacheValidationClass::CanonicalSemantic},
			{EAngelscriptCacheValidationError::InvalidPresence, EAngelscriptCacheValidationClass::CanonicalSemantic},
			{EAngelscriptCacheValidationError::InvalidQualifierCombination, EAngelscriptCacheValidationClass::CanonicalSemantic},
			{EAngelscriptCacheValidationError::OrdinalGap, EAngelscriptCacheValidationClass::CanonicalSemantic},
			{EAngelscriptCacheValidationError::DuplicateOrdinal, EAngelscriptCacheValidationClass::CanonicalSemantic},
			{EAngelscriptCacheValidationError::DerivedHashMismatch, EAngelscriptCacheValidationClass::CanonicalSemantic},
			{EAngelscriptCacheValidationError::MissingOwner, EAngelscriptCacheValidationClass::GraphOrOwnership},
			{EAngelscriptCacheValidationError::CrossModuleOwner, EAngelscriptCacheValidationClass::GraphOrOwnership},
			{EAngelscriptCacheValidationError::MissingGraphTarget, EAngelscriptCacheValidationClass::GraphOrOwnership},
			{EAngelscriptCacheValidationError::WrongReferenceKind, EAngelscriptCacheValidationClass::GraphOrOwnership},
			{EAngelscriptCacheValidationError::CompatibilityMismatch, EAngelscriptCacheValidationClass::Ineligible},
			{EAngelscriptCacheValidationError::ContextMismatch, EAngelscriptCacheValidationClass::Ineligible},
			{EAngelscriptCacheValidationError::ProfileMismatch, EAngelscriptCacheValidationClass::Ineligible},
			{EAngelscriptCacheValidationError::SourceSnapshotMismatch, EAngelscriptCacheValidationClass::Ineligible},
			{EAngelscriptCacheValidationError::CurrentAbiMismatch, EAngelscriptCacheValidationClass::Ineligible}};
		for (int32 Index = 0; Index < UE_ARRAY_COUNT(ValidationExpectations); ++Index)
		{
			const FValidationExpectation& Expectation = ValidationExpectations[Index];
			ASSERT_THAT(AreEqual(Index, static_cast<int32>(Expectation.Error),
				TEXT("Every named validation error must retain its frozen wire value")));
			ASSERT_THAT(AreEqual(static_cast<int32>(Expectation.ExpectedClass),
				static_cast<int32>(FAngelscriptCacheValidationResult::Classify(Expectation.Error)),
				TEXT("Every named validation error must retain its frozen classification")));
			ASSERT_THAT(AreEqual(static_cast<int32>(Expectation.ExpectedClass),
				static_cast<int32>(FAngelscriptCacheValidationResult(Expectation.Error).Class),
				TEXT("Validation result construction must use the frozen classifier")));
		}
	}

	TEST_METHOD(CommonPrimitiveBytesHaveIndependentGoldens)
	{
		using namespace AngelscriptCachePrimitiveTests_Private;
		TArray<uint8> Bytes;

		FAngelscriptCacheValidationResult Result =
			FAngelscriptCacheSemanticArchive::SerializeCanonicalString(TEXT("AS"), Bytes);
		ASSERT_THAT(IsTrue(Result.IsSuccess()));
		ASSERT_THAT(AreEqual(FString(TEXT("020000004153")), Hex(Bytes)));

		Result = FAngelscriptCacheSemanticArchive::SerializeCanonicalDataType(MakeInt32Type(), Bytes);
		ASSERT_THAT(IsTrue(Result.IsSuccess()));
		ASSERT_THAT(AreEqual(FString(TEXT("0105000000000000000000")), Hex(Bytes)));

		Result = FAngelscriptCacheSemanticArchive::SerializeStableReference(MakeFunctionReference(), Bytes);
		ASSERT_THAT(IsTrue(Result.IsSuccess()));
		ASSERT_THAT(AreEqual(
			FString(TEXT("031111111111111111111111111111111111111111111111111111111111111111"
				"2222222222222222222222222222222222222222222222222222222222222222")),
			Hex(Bytes)));

		FAngelscriptCacheSemanticDependency Dependency;
		Dependency.Kind = EAngelscriptCacheSemanticDependencyKind::HardValue;
		Dependency.Target = MakeFunctionReference();
		Dependency.ExpectedContentOrValue = MakeHash(0x33);
		Result = FAngelscriptCacheSemanticArchive::SerializeSemanticDependency(Dependency, Bytes);
		ASSERT_THAT(IsTrue(Result.IsSuccess()));
		ASSERT_THAT(AreEqual(
			FString(TEXT("08031111111111111111111111111111111111111111111111111111111111111111"
				"222222222222222222222222222222222222222222222222222222222222222201"
				"3333333333333333333333333333333333333333333333333333333333333333")),
			Hex(Bytes)));

		FAngelscriptCachedMetadataEntry Metadata{TEXT("A"), TEXT("B")};
		Result = FAngelscriptCacheSemanticArchive::SerializeMetadataEntry(Metadata, Bytes);
		ASSERT_THAT(IsTrue(Result.IsSuccess()));
		ASSERT_THAT(AreEqual(FString(TEXT("01000000410100000042")), Hex(Bytes)));

		FAngelscriptCachedParameter Parameter;
		Parameter.Ordinal = 0;
		Parameter.CanonicalName = TEXT("X");
		Parameter.Type = MakeInt32Type();
		Parameter.Passing = EAngelscriptCachedParameterPassing::Value;
		Parameter.CanonicalDefaultExpression = FString(TEXT("7"));
		Parameter.TraitFlags = static_cast<uint32>(EAngelscriptCachedParameterTraitFlags::BlueprintByValue);
		Result = FAngelscriptCacheSemanticArchive::SerializeParameter(Parameter, Bytes);
		ASSERT_THAT(IsTrue(Result.IsSuccess()));
		ASSERT_THAT(AreEqual(
			FString(TEXT("00000000010000005801050000000000000000000101010000003701000000")),
			Hex(Bytes)));

		const FAngelscriptCachedDeclarationSlot Slot{
			EAngelscriptCacheDeclarationSlotKind::Function, 0};
		Result = FAngelscriptCacheSemanticArchive::SerializeDeclarationSlot(Slot, Bytes);
		ASSERT_THAT(IsTrue(Result.IsSuccess()));
		ASSERT_THAT(AreEqual(FString(TEXT("0200000000")), Hex(Bytes)));
	}

	TEST_METHOD(CommonValuesRoundTripAndReserializeByteExactly)
	{
		using namespace AngelscriptCachePrimitiveTests_Private;
		FAngelscriptCachedDataType Original;
		Original.Kind = EAngelscriptCachedDataTypeKind::ScriptType;
		Original.TypeReference = FAngelscriptCacheStableReference{
			EAngelscriptCacheReferenceKind::ScriptType, MakeHash(0x41), MakeHash(0x42)};
		Original.QualifierFlags = static_cast<uint32>(EAngelscriptCachedTypeQualifierFlags::ObjectHandle)
			| static_cast<uint32>(EAngelscriptCachedTypeQualifierFlags::ConstHandle);
		Original.OrderedSubTypes.Add(MakeInt32Type());

		TArray<uint8> Bytes;
		ASSERT_THAT(IsTrue(FAngelscriptCacheSemanticArchive::SerializeCanonicalDataType(Original, Bytes).IsSuccess()));
		const FString OriginalHex = Hex(Bytes);

		FAngelscriptCacheReadLimits Limits;
		FAngelscriptCacheReadBudget Budget;
		FAngelscriptCachedDataType Decoded;
		const FAngelscriptCacheValidationResult ReadResult =
			FAngelscriptCacheSemanticArchive::DeserializeCanonicalDataType(Bytes, Limits, Budget, Decoded);
		ASSERT_THAT(IsTrue(ReadResult.IsSuccess()));

		TArray<uint8> Reserialized;
		ASSERT_THAT(IsTrue(
			FAngelscriptCacheSemanticArchive::SerializeCanonicalDataType(Decoded, Reserialized).IsSuccess()));
		ASSERT_THAT(AreEqual(OriginalHex, Hex(Reserialized)));
	}

	TEST_METHOD(ReferenceDependencyAndQualifierRulesFailClosed)
	{
		using namespace AngelscriptCachePrimitiveTests_Private;
		TArray<uint8> Bytes = {0xde, 0xad};

		FAngelscriptCacheStableReference Reference = MakeFunctionReference();
		Reference.StableKey = {};
		FAngelscriptCacheValidationResult Result =
			FAngelscriptCacheSemanticArchive::SerializeStableReference(Reference, Bytes);
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::ZeroStableKey, Result.Error));
		ASSERT_THAT(IsTrue(Bytes.IsEmpty()));

		Reference = MakeFunctionReference();
		Reference.ExpectedAbi = {};
		Result = FAngelscriptCacheSemanticArchive::SerializeStableReference(Reference, Bytes);
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::MissingExpectedAbi, Result.Error));

		Reference.Kind = EAngelscriptCacheReferenceKind::CanonicalName;
		Reference.ExpectedAbi = MakeHash(0x22);
		Result = FAngelscriptCacheSemanticArchive::SerializeStableReference(Reference, Bytes);
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::ForbiddenExpectedAbi, Result.Error));

		FAngelscriptCacheSemanticDependency Dependency;
		Dependency.Kind = EAngelscriptCacheSemanticDependencyKind::HardValue;
		Dependency.Target = MakeFunctionReference();
		Result = FAngelscriptCacheSemanticArchive::SerializeSemanticDependency(Dependency, Bytes);
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::InvalidPresence, Result.Error));

		Dependency.Kind = EAngelscriptCacheSemanticDependencyKind::Declaration;
		Dependency.ExpectedContentOrValue = MakeHash(0x33);
		Result = FAngelscriptCacheSemanticArchive::SerializeSemanticDependency(Dependency, Bytes);
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::InvalidPresence, Result.Error));

		for (uint8 KindValue = 1; KindValue <= 12; ++KindValue)
		{
			Dependency = {};
			Dependency.Kind = static_cast<EAngelscriptCacheSemanticDependencyKind>(KindValue);
			Dependency.Target = MakeFunctionReference();
			const bool bRequiresContent = KindValue == 5 || KindValue == 6
				|| KindValue == 7 || KindValue == 8 || KindValue == 9
				|| KindValue == 10 || KindValue == 12;
			if (bRequiresContent)
			{
				Dependency.ExpectedContentOrValue = MakeHash(static_cast<uint8>(0x40 + KindValue));
			}
			Result = FAngelscriptCacheSemanticArchive::SerializeSemanticDependency(Dependency, Bytes);
			ASSERT_THAT(IsTrue(Result.IsSuccess(),
				TEXT("Every dependency kind must accept exactly its frozen content-presence matrix")));
			Dependency.ExpectedContentOrValue = bRequiresContent
				? TOptional<FAngelscriptHash256>{}
				: TOptional<FAngelscriptHash256>{MakeHash(static_cast<uint8>(0x60 + KindValue))};
			Result = FAngelscriptCacheSemanticArchive::SerializeSemanticDependency(Dependency, Bytes);
			ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::InvalidPresence, Result.Error));
		}

		Dependency = {};
		Dependency.Kind = EAngelscriptCacheSemanticDependencyKind::FunctionContent;
		Dependency.Target = MakeFunctionReference();
		Dependency.Target.Kind = EAngelscriptCacheReferenceKind::ScriptType;
		Dependency.ExpectedContentOrValue = MakeHash(0x7f);
		Result = FAngelscriptCacheSemanticArchive::SerializeSemanticDependency(
			Dependency, Bytes);
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheValidationError::WrongReferenceKind, Result.Error));

		FAngelscriptCachedDataType Primitive = MakeInt32Type();
		Primitive.QualifierFlags = static_cast<uint32>(EAngelscriptCachedTypeQualifierFlags::ObjectHandle);
		Result = FAngelscriptCacheSemanticArchive::SerializeCanonicalDataType(Primitive, Bytes);
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::InvalidQualifierCombination, Result.Error));

		FAngelscriptCachedDataType Auto;
		Auto.Kind = EAngelscriptCachedDataTypeKind::Auto;
		Result = FAngelscriptCacheSemanticArchive::SerializeCanonicalDataType(Auto, Bytes);
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::InvalidQualifierCombination, Result.Error));

		Primitive = MakeInt32Type();
		Primitive.QualifierFlags = 0x80000000u;
		Result = FAngelscriptCacheSemanticArchive::SerializeCanonicalDataType(Primitive, Bytes);
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::UnknownFlags, Result.Error));
	}

	TEST_METHOD(ReaderRejectsMalformedStringsUnknownTagsTrailingDataAndBudgets)
	{
		using namespace AngelscriptCachePrimitiveTests_Private;
		FAngelscriptCacheReadLimits Limits;
		FAngelscriptCacheReadBudget InvalidUtf8Budget;
		FString StringOutput = TEXT("sentinel");

		const TArray<uint8> InvalidUtf8 = {2, 0, 0, 0, 0xc0, 0x80};
		FAngelscriptCacheValidationResult Result =
			FAngelscriptCacheSemanticArchive::DeserializeCanonicalString(
				InvalidUtf8, Limits, InvalidUtf8Budget, StringOutput);
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::InvalidUtf8, Result.Error));
		ASSERT_THAT(IsTrue(StringOutput.IsEmpty()));

		const TConstArrayView<uint8> PositiveNullBytes(static_cast<const uint8*>(nullptr), 1);
		FAngelscriptCacheReadBudget PositiveNullBytesBudget;
		Result = FAngelscriptCacheSemanticArchive::DeserializeCanonicalString(
			PositiveNullBytes, Limits, PositiveNullBytesBudget, StringOutput);
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::InvalidArrayView, Result.Error));
		ASSERT_THAT(IsTrue(StringOutput.IsEmpty()));

		TArray<uint8> StringBytes = {0xde};
		const FStringView PositiveNullString(static_cast<const TCHAR*>(nullptr), 1);
		Result = FAngelscriptCacheSemanticArchive::SerializeCanonicalString(PositiveNullString, StringBytes);
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::InvalidArrayView, Result.Error));
		ASSERT_THAT(IsTrue(StringBytes.IsEmpty()));

		FAngelscriptCacheReadBudget EmbeddedNulBudget;
		StringOutput = TEXT("sentinel");
		const TArray<uint8> EmbeddedNul = {1, 0, 0, 0, 0};
		Result = FAngelscriptCacheSemanticArchive::DeserializeCanonicalString(
			EmbeddedNul, Limits, EmbeddedNulBudget, StringOutput);
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::EmbeddedNul, Result.Error));
		ASSERT_THAT(IsTrue(StringOutput.IsEmpty()));

		TArray<uint8> TrailingStringBytes;
		ASSERT_THAT(IsTrue(FAngelscriptCacheSemanticArchive::SerializeCanonicalString(
			TEXT("AS"), TrailingStringBytes).IsSuccess()));
		const uint64 FirstTrailingStringOffset = static_cast<uint64>(TrailingStringBytes.Num());
		TrailingStringBytes.Add(0x6f);
		FAngelscriptCacheReadBudget TrailingStringBudget;
		Result = FAngelscriptCacheSemanticArchive::DeserializeCanonicalString(
			TrailingStringBytes, Limits, TrailingStringBudget, StringOutput);
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::TrailingData, Result.Error));
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationClass::Malformed, Result.Class));
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationStage::PayloadDecode, Result.Stage));
		ASSERT_THAT(AreEqual(static_cast<EAngelscriptCacheRecordKind>(0), Result.RecordKind));
		ASSERT_THAT(AreEqual(FirstTrailingStringOffset, Result.ByteOffset));
		ASSERT_THAT(IsTrue(StringOutput.IsEmpty()));
		ASSERT_THAT(AreEqual(UINT64_C(0), StringOutput.GetCharArray().GetAllocatedSize()));

		TArray<uint8> TypeBytes;
		ASSERT_THAT(IsTrue(
			FAngelscriptCacheSemanticArchive::SerializeCanonicalDataType(MakeInt32Type(), TypeBytes).IsSuccess()));
		TypeBytes[0] = 0xff;
		FAngelscriptCachedDataType TypeOutput = MakeInt32Type();
		FAngelscriptCacheReadBudget UnknownEnumBudget;
		Result = FAngelscriptCacheSemanticArchive::DeserializeCanonicalDataType(
			TypeBytes, Limits, UnknownEnumBudget, TypeOutput);
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::UnknownEnumValue, Result.Error));
		ASSERT_THAT(AreEqual(0, static_cast<int32>(TypeOutput.Kind)));

		TypeBytes[0] = static_cast<uint8>(EAngelscriptCachedDataTypeKind::Primitive);
		TypeBytes.Add(0x77);
		FAngelscriptCacheReadBudget TrailingDataBudget;
		Result = FAngelscriptCacheSemanticArchive::DeserializeCanonicalDataType(
			TypeBytes, Limits, TrailingDataBudget, TypeOutput);
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::TrailingData, Result.Error));
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationClass::Malformed, Result.Class));
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationStage::PayloadDecode, Result.Stage));
		ASSERT_THAT(AreEqual(static_cast<EAngelscriptCacheRecordKind>(0), Result.RecordKind));
		ASSERT_THAT(AreEqual(UINT64_C(11), Result.ByteOffset));

		Limits.MaxCanonicalRecordPayloadBytes = 4;
		FAngelscriptCacheReadBudget PayloadBudget;
		Result = FAngelscriptCacheSemanticArchive::DeserializeCanonicalDataType(
			TypeBytes, Limits, PayloadBudget, TypeOutput);
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::BudgetExceeded, Result.Error));

		Limits = {};
		Limits.MaxResidentDecodedBytes = 2;
		FAngelscriptCacheReadBudget ResidentBudget;
		StringOutput = TEXT("sentinel");
		const TArray<uint8> TwoCharacterString = {2, 0, 0, 0, 'A', 'S'};
		Result = FAngelscriptCacheSemanticArchive::DeserializeCanonicalString(
			TwoCharacterString, Limits, ResidentBudget, StringOutput);
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::BudgetExceeded, Result.Error));
		ASSERT_THAT(IsTrue(StringOutput.IsEmpty()));

		TArray<uint8> NestedBytes;
		FAngelscriptCachedDataType Nested = MakeInt32Type();
		Nested.OrderedSubTypes.Add(MakeInt32Type());
		ASSERT_THAT(IsTrue(
			FAngelscriptCacheSemanticArchive::SerializeCanonicalDataType(Nested, NestedBytes).IsSuccess()));
		Limits = {};
		Limits.MaxNestingDepth = 1;
		FAngelscriptCacheReadBudget NestingDepthBudget;
		Result = FAngelscriptCacheSemanticArchive::DeserializeCanonicalDataType(
			NestedBytes, Limits, NestingDepthBudget, TypeOutput);
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::NestingDepthExceeded, Result.Error));

		Limits = {};
		Limits.MaxArrayElements = 0;
		FAngelscriptCacheReadBudget ArrayElementsBudget;
		Result = FAngelscriptCacheSemanticArchive::DeserializeCanonicalDataType(
			NestedBytes, Limits, ArrayElementsBudget, TypeOutput);
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::BudgetExceeded, Result.Error));

		Limits = {};
		FAngelscriptCacheReadBudget SingleNestedReadMeasureBudget;
		ASSERT_THAT(IsTrue(FAngelscriptCacheSemanticArchive::DeserializeCanonicalDataType(
			NestedBytes, Limits, SingleNestedReadMeasureBudget, TypeOutput).IsSuccess()));
		const uint64 SingleNestedReadDecodedBytes = SingleNestedReadMeasureBudget.GetDecodedBytes();
		ASSERT_THAT(IsTrue(SingleNestedReadDecodedBytes > 0));
		Limits.MaxTotalDecodedBytes = SingleNestedReadDecodedBytes;
		FAngelscriptCacheReadBudget CumulativeBudget;
		ASSERT_THAT(IsTrue(FAngelscriptCacheSemanticArchive::DeserializeCanonicalDataType(
			NestedBytes, Limits, CumulativeBudget, TypeOutput).IsSuccess()));
		Result = FAngelscriptCacheSemanticArchive::DeserializeCanonicalDataType(
			NestedBytes, Limits, CumulativeBudget, TypeOutput);
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::BudgetExceeded, Result.Error));
		ASSERT_THAT(AreEqual(0, static_cast<int32>(TypeOutput.Kind)));

		TArray<uint8> ImpossibleCountBytes;
		ASSERT_THAT(IsTrue(FAngelscriptCacheSemanticArchive::SerializeCanonicalDataType(
			MakeInt32Type(), ImpossibleCountBytes).IsSuccess()));
		check(ImpossibleCountBytes.Num() == 11);
		for (int32 Offset = 7; Offset <= 10; ++Offset)
		{
			ImpossibleCountBytes[Offset] = 0xff;
		}
		Limits = {};
		Limits.MaxArrayElements = UINT64_MAX;
		FAngelscriptCacheReadBudget ImpossibleCountBudget;
		Result = FAngelscriptCacheSemanticArchive::DeserializeCanonicalDataType(
			ImpossibleCountBytes, Limits, ImpossibleCountBudget, TypeOutput);
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::ImpossibleCount, Result.Error));
	}

	TEST_METHOD(CanonicalStringInputOutputAllocationOverlapFailsBeforeMutation)
	{
		FAngelscriptCacheReadLimits Limits;

		TArray<uint8> SerializeOutput;
		SerializeOutput.SetNumUninitialized(4);
		TCHAR* AliasedCharacters = reinterpret_cast<TCHAR*>(SerializeOutput.GetData());
		AliasedCharacters[0] = TEXT('A');
		AliasedCharacters[1] = TEXT('S');
		const FStringView AliasedStringInput(AliasedCharacters, 2);
		FAngelscriptCacheValidationResult Result =
			FAngelscriptCacheSemanticArchive::SerializeCanonicalString(
				AliasedStringInput, SerializeOutput);
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::AliasedInputOutput,
			Result.Error));
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationClass::Malformed, Result.Class));
		ASSERT_THAT(IsTrue(SerializeOutput.IsEmpty()));

		FString DeserializeOutput;
		DeserializeOutput.GetCharArray().SetNumUninitialized(4);
		uint8* AliasedPayload = reinterpret_cast<uint8*>(
			DeserializeOutput.GetCharArray().GetData());
		const uint8 CanonicalAs[] = {2, 0, 0, 0, 'A', 'S'};
		FMemory::Memcpy(AliasedPayload, CanonicalAs, UE_ARRAY_COUNT(CanonicalAs));
		AliasedPayload[6] = 0;
		AliasedPayload[7] = 0;
		const TConstArrayView<uint8> AliasedByteInput(
			AliasedPayload, UE_ARRAY_COUNT(CanonicalAs));
		FAngelscriptCacheReadBudget Budget;
		Result = FAngelscriptCacheSemanticArchive::DeserializeCanonicalString(
			AliasedByteInput, Limits, Budget, DeserializeOutput);
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::AliasedInputOutput,
			Result.Error));
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationClass::Malformed, Result.Class));
		ASSERT_THAT(IsTrue(DeserializeOutput.IsEmpty()));
		ASSERT_THAT(AreEqual(UINT64_C(0), Budget.GetStoredBytes()));
		ASSERT_THAT(AreEqual(UINT64_C(0), Budget.GetDecompressedBytes()));
		ASSERT_THAT(AreEqual(UINT64_C(0), Budget.GetDecodedBytes()));
		ASSERT_THAT(AreEqual(UINT64_C(0), Budget.GetResidentDecodedBytes()));
		ASSERT_THAT(AreEqual(UINT64_C(0), Budget.GetTemporaryResidentDecodedBytes()));
		ASSERT_THAT(AreEqual(UINT64_C(0), Budget.GetPeakLiveResidentDecodedBytes()));
	}

	TEST_METHOD(CanonicalStringAliasingIncludesUnusedOutputCapacity)
	{
		FAngelscriptCacheReadLimits Limits;

		TArray<uint8> SerializeOutput;
		SerializeOutput.Reserve(64);
		SerializeOutput.SetNumUninitialized(1);
		check(SerializeOutput.Max() >= 16);
		TCHAR* CapacityCharacters = reinterpret_cast<TCHAR*>(
			SerializeOutput.GetData() + 8);
		CapacityCharacters[0] = TEXT('A');
		CapacityCharacters[1] = TEXT('S');
		FAngelscriptCacheValidationResult Result =
			FAngelscriptCacheSemanticArchive::SerializeCanonicalString(
				FStringView(CapacityCharacters, 2), SerializeOutput);
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::AliasedInputOutput,
			Result.Error));
		ASSERT_THAT(IsTrue(SerializeOutput.IsEmpty()));

		FString DeserializeOutput;
		DeserializeOutput.Reserve(64);
		check(DeserializeOutput.GetCharArray().Max()
			* static_cast<int32>(sizeof(TCHAR)) >= 16);
		uint8* CapacityBytes = reinterpret_cast<uint8*>(
			DeserializeOutput.GetCharArray().GetData()) + 8;
		const uint8 CanonicalAs[] = {2, 0, 0, 0, 'A', 'S'};
		FMemory::Memcpy(CapacityBytes, CanonicalAs, UE_ARRAY_COUNT(CanonicalAs));
		FAngelscriptCacheReadBudget Budget;
		Result = FAngelscriptCacheSemanticArchive::DeserializeCanonicalString(
			TConstArrayView<uint8>(CapacityBytes, UE_ARRAY_COUNT(CanonicalAs)),
			Limits, Budget, DeserializeOutput);
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::AliasedInputOutput,
			Result.Error));
		ASSERT_THAT(IsTrue(DeserializeOutput.IsEmpty()));
		ASSERT_THAT(AreEqual(UINT64_C(0), Budget.GetDecodedBytes()));
		ASSERT_THAT(AreEqual(UINT64_C(0), Budget.GetResidentDecodedBytes()));
		ASSERT_THAT(AreEqual(UINT64_C(0), Budget.GetTemporaryResidentDecodedBytes()));
		ASSERT_THAT(AreEqual(UINT64_C(0), Budget.GetPeakLiveResidentDecodedBytes()));
	}

	TEST_METHOD(CanonicalDataTypeRejectsInputInsideTopLevelOrNestedOutputAllocation)
	{
		TArray<uint8> CanonicalTypeBytes;
		ASSERT_THAT(IsTrue(FAngelscriptCacheSemanticArchive::SerializeCanonicalDataType(
			AngelscriptCachePrimitiveTests_Private::MakeInt32Type(),
			CanonicalTypeBytes).IsSuccess()));
		FAngelscriptCacheReadLimits Limits;

		const auto RunAliasedDecode = [&](const bool bNested)
		{
			FAngelscriptCachedDataType Output;
			TArray<FAngelscriptCachedDataType>* AliasedAllocation =
				&Output.OrderedSubTypes;
			if (bNested)
			{
				Output.OrderedSubTypes.AddDefaulted();
				AliasedAllocation = &Output.OrderedSubTypes[0].OrderedSubTypes;
			}

			const int32 RequiredElements = FMath::DivideAndRoundUp(
				CanonicalTypeBytes.Num(),
				static_cast<int32>(sizeof(FAngelscriptCachedDataType))) + 1;
			AliasedAllocation->Reserve(RequiredElements);
			check(AliasedAllocation->GetAllocatedSize() >= CanonicalTypeBytes.Num());
			uint8* CapacityBytes = reinterpret_cast<uint8*>(
				AliasedAllocation->GetData());
			FMemory::Memcpy(
				CapacityBytes, CanonicalTypeBytes.GetData(), CanonicalTypeBytes.Num());

			FAngelscriptCacheReadBudget Budget;
			const FAngelscriptCacheValidationResult Result =
				FAngelscriptCacheSemanticArchive::DeserializeCanonicalDataType(
					TConstArrayView<uint8>(CapacityBytes, CanonicalTypeBytes.Num()),
					Limits, Budget, Output);
			ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::AliasedInputOutput,
				Result.Error));
			ASSERT_THAT(IsTrue(Output.Kind == EAngelscriptCachedDataTypeKind::Invalid));
			ASSERT_THAT(AreEqual(0, Output.OrderedSubTypes.Num()));
			ASSERT_THAT(AreEqual(UINT64_C(0), Budget.GetDecodedBytes()));
			ASSERT_THAT(AreEqual(UINT64_C(0), Budget.GetResidentDecodedBytes()));
			ASSERT_THAT(AreEqual(UINT64_C(0), Budget.GetTemporaryResidentDecodedBytes()));
			ASSERT_THAT(AreEqual(UINT64_C(0), Budget.GetPeakLiveResidentDecodedBytes()));
		};

		RunAliasedDecode(false);
		RunAliasedDecode(true);
	}

	TEST_METHOD(CanonicalDataTypeOutputAliasPreflightHonorsDepthAndNodeLimits)
	{
		TArray<uint8> CanonicalTypeBytes;
		ASSERT_THAT(IsTrue(FAngelscriptCacheSemanticArchive::SerializeCanonicalDataType(
			AngelscriptCachePrimitiveTests_Private::MakeInt32Type(),
			CanonicalTypeBytes).IsSuccess()));

		FAngelscriptCachedDataType DeepOutput;
		FAngelscriptCachedDataType* Cursor = &DeepOutput;
		for (int32 Depth = 0; Depth < 8; ++Depth)
		{
			Cursor->OrderedSubTypes.AddDefaulted();
			Cursor = &Cursor->OrderedSubTypes[0];
		}
		FAngelscriptCacheReadLimits DepthLimits;
		DepthLimits.MaxNestingDepth = 3;
		FAngelscriptCacheReadBudget DepthBudget;
		FAngelscriptCacheValidationResult Result =
			FAngelscriptCacheSemanticArchive::DeserializeCanonicalDataType(
				CanonicalTypeBytes, DepthLimits, DepthBudget, DeepOutput);
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::NestingDepthExceeded,
			Result.Error));
		ASSERT_THAT(IsTrue(DeepOutput.Kind == EAngelscriptCachedDataTypeKind::Invalid));
		ASSERT_THAT(AreEqual(0, DeepOutput.OrderedSubTypes.Num()));
		ASSERT_THAT(AreEqual(UINT64_C(0), DepthBudget.GetDecodedBytes()));
		ASSERT_THAT(AreEqual(UINT64_C(0), DepthBudget.GetResidentDecodedBytes()));
		ASSERT_THAT(AreEqual(UINT64_C(0),
			DepthBudget.GetTemporaryResidentDecodedBytes()));
		ASSERT_THAT(AreEqual(UINT64_C(0),
			DepthBudget.GetPeakLiveResidentDecodedBytes()));

		FAngelscriptCachedDataType WideOutput;
		WideOutput.OrderedSubTypes.SetNum(5);
		FAngelscriptCacheReadLimits NodeLimits;
		NodeLimits.MaxArrayElements = 4;
		FAngelscriptCacheReadBudget NodeBudget;
		Result = FAngelscriptCacheSemanticArchive::DeserializeCanonicalDataType(
			CanonicalTypeBytes, NodeLimits, NodeBudget, WideOutput);
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::BudgetExceeded,
			Result.Error));
		ASSERT_THAT(IsTrue(WideOutput.Kind == EAngelscriptCachedDataTypeKind::Invalid));
		ASSERT_THAT(AreEqual(0, WideOutput.OrderedSubTypes.Num()));
		ASSERT_THAT(AreEqual(UINT64_C(0), NodeBudget.GetDecodedBytes()));
		ASSERT_THAT(AreEqual(UINT64_C(0), NodeBudget.GetResidentDecodedBytes()));
		ASSERT_THAT(AreEqual(UINT64_C(0),
			NodeBudget.GetTemporaryResidentDecodedBytes()));
		ASSERT_THAT(AreEqual(UINT64_C(0),
			NodeBudget.GetPeakLiveResidentDecodedBytes()));
	}

	TEST_METHOD(LatePrimitiveDecodeFailureReleasesCandidateLiveOwnershipWithoutRefundingTotal)
	{
		using namespace AngelscriptCachePrimitiveTests_Private;
		constexpr uint64 SeedRetained = 7;
		constexpr uint64 SeedTemporary = 5;
		FAngelscriptCacheReadLimits Limits;

		TArray<uint8> StringPayload;
		ASSERT_THAT(IsTrue(FAngelscriptCacheSemanticArchive::SerializeCanonicalString(
			TEXT("allocator-owned-string"), StringPayload).IsSuccess()));
		const uint64 StringTrailingOffset = static_cast<uint64>(StringPayload.Num());
		StringPayload.Add(0x5a);
		FAngelscriptCacheReadBudget StringBudget;
		ASSERT_THAT(IsTrue(StringBudget.TryConsumeRetainedDecoded(
			SeedRetained, Limits)));
		FAngelscriptCacheTemporaryResidentReservation StringSeedReservation;
		ASSERT_THAT(IsTrue(StringBudget.TryReserveTemporaryDecoded(
			SeedTemporary, Limits, StringSeedReservation)));
		FString StringOutput = TEXT("sentinel");
		FAngelscriptCacheValidationResult Result =
			FAngelscriptCacheSemanticArchive::DeserializeCanonicalString(
				StringPayload, Limits, StringBudget, StringOutput);
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::TrailingData,
			Result.Error));
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationStage::PayloadDecode,
			Result.Stage));
		ASSERT_THAT(AreEqual(StringTrailingOffset, Result.ByteOffset));
		ASSERT_THAT(IsTrue(StringOutput.IsEmpty()));
		ASSERT_THAT(IsTrue(StringBudget.GetDecodedBytes()
			> SeedRetained + SeedTemporary,
			TEXT("accepted candidate allocations remain in monotonic Total")));
		ASSERT_THAT(AreEqual(SeedRetained,
			StringBudget.GetResidentDecodedBytes(),
			TEXT("destroyed candidate bytes cannot remain resident")));
		ASSERT_THAT(AreEqual(SeedTemporary,
			StringBudget.GetTemporaryResidentDecodedBytes(),
			TEXT("failure preserves caller-owned temporary seed only")));
		ASSERT_THAT(IsTrue(StringBudget.GetPeakLiveResidentDecodedBytes()
			> SeedRetained + SeedTemporary));
		StringSeedReservation.Reset();

		FAngelscriptCachedDataType NestedType = MakeInt32Type();
		NestedType.OrderedSubTypes.Add(MakeInt32Type());
		TArray<uint8> TypePayload;
		ASSERT_THAT(IsTrue(FAngelscriptCacheSemanticArchive::SerializeCanonicalDataType(
			NestedType, TypePayload).IsSuccess()));
		const uint64 TypeTrailingOffset = static_cast<uint64>(TypePayload.Num());
		TypePayload.Add(0x6b);
		FAngelscriptCacheReadBudget TypeBudget;
		ASSERT_THAT(IsTrue(TypeBudget.TryConsumeRetainedDecoded(
			SeedRetained, Limits)));
		FAngelscriptCacheTemporaryResidentReservation TypeSeedReservation;
		ASSERT_THAT(IsTrue(TypeBudget.TryReserveTemporaryDecoded(
			SeedTemporary, Limits, TypeSeedReservation)));
		FAngelscriptCachedDataType TypeOutput = MakeInt32Type();
		Result = FAngelscriptCacheSemanticArchive::DeserializeCanonicalDataType(
			TypePayload, Limits, TypeBudget, TypeOutput);
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::TrailingData,
			Result.Error));
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationStage::PayloadDecode,
			Result.Stage));
		ASSERT_THAT(AreEqual(TypeTrailingOffset, Result.ByteOffset));
		ASSERT_THAT(AreEqual(0, static_cast<int32>(TypeOutput.Kind)));
		ASSERT_THAT(IsTrue(TypeOutput.OrderedSubTypes.IsEmpty()));
		ASSERT_THAT(IsTrue(TypeBudget.GetDecodedBytes()
			> SeedRetained + SeedTemporary));
		ASSERT_THAT(AreEqual(SeedRetained,
			TypeBudget.GetResidentDecodedBytes()));
		ASSERT_THAT(AreEqual(SeedTemporary,
			TypeBudget.GetTemporaryResidentDecodedBytes()));
		ASSERT_THAT(IsTrue(TypeBudget.GetPeakLiveResidentDecodedBytes()
			> SeedRetained + SeedTemporary));
		TypeSeedReservation.Reset();
	}

	TEST_METHOD(ReaderChargesActualAllocatorCapacityAtomicallyBeforeAllocation)
	{
		using namespace AngelscriptCachePrimitiveTests_Private;
		using namespace AngelscriptCacheCanonicalCodecTestHooks;
		constexpr int32 AllocationEventCapacity = 16;
		FAllocationEvent AllocationEventStorage[AllocationEventCapacity];
		int32 AllocationEventCount = 0;
		bool bAllocationEventOverflowed = false;
		auto CapturedAllocationEvents = [&]()
		{
			return MakeArrayView(AllocationEventStorage, AllocationEventCount);
		};

		const FString Original = TEXT("AS");
		TArray<uint8> StringBytes;
		ASSERT_THAT(IsTrue(FAngelscriptCacheSemanticArchive::SerializeCanonicalString(
			Original, StringBytes).IsSuccess()));
		const int32 StringReservedCapacity =
			CalculateArrayReserveCapacityForTests<TCHAR>(Original.Len() + 1);
		const uint64 StringCapacityBytes =
			static_cast<uint64>(StringReservedCapacity) * sizeof(TCHAR);
		ASSERT_THAT(IsTrue(StringCapacityBytes > 0));

		FAngelscriptCacheReadLimits ExactStringLimits;
		ExactStringLimits.MaxTotalDecodedBytes = StringCapacityBytes;
		ExactStringLimits.MaxResidentDecodedBytes = StringCapacityBytes;
		FAngelscriptCacheReadBudget ExactStringBudget;
		FString DecodedString;
		FAngelscriptCacheValidationResult Result;
		Result = FAngelscriptCacheSemanticArchive::
			DeserializeCanonicalStringWithAllocationCaptureForTests(
				StringBytes,
				ExactStringLimits,
				ExactStringBudget,
				FAllocationEventCaptureView(
					MakeArrayView(AllocationEventStorage),
					AllocationEventCount,
					bAllocationEventOverflowed),
				DecodedString);
		ASSERT_THAT(IsFalse(bAllocationEventOverflowed));
		ASSERT_THAT(IsTrue(Result.IsSuccess()));
		ASSERT_THAT(AreEqual(Original, DecodedString));
		ASSERT_THAT(AreEqual(StringCapacityBytes, ExactStringBudget.GetDecodedBytes()));
		ASSERT_THAT(AreEqual(StringCapacityBytes, ExactStringBudget.GetResidentDecodedBytes()));
		ASSERT_THAT(AreEqual(StringCapacityBytes, ExactStringBudget.GetPeakLiveResidentDecodedBytes()));
		ASSERT_THAT(AreEqual(UINT64_C(0), ExactStringBudget.GetTemporaryResidentDecodedBytes()));
		ASSERT_THAT(AreEqual(StringCapacityBytes, DecodedString.GetCharArray().GetAllocatedSize()));
		auto Events = CapturedAllocationEvents();
		ASSERT_THAT(AreEqual(4, Events.Num()));
		const EAllocationEventPhase ExactStringPhases[] = {
			EAllocationEventPhase::BudgetAttempt,
			EAllocationEventPhase::BudgetAccepted,
			EAllocationEventPhase::AllocationAttempt,
			EAllocationEventPhase::AllocationSucceeded};
		const int32 ExactStringEventCount =
			FMath::Min(Events.Num(), static_cast<int32>(UE_ARRAY_COUNT(ExactStringPhases)));
		for (int32 EventIndex = 0; EventIndex < ExactStringEventCount; ++EventIndex)
		{
			ASSERT_THAT(AreEqual(EAllocationSite::StringCharacters, Events[EventIndex].Site));
			ASSERT_THAT(AreEqual(ExactStringPhases[EventIndex], Events[EventIndex].Phase));
			ASSERT_THAT(AreEqual(UINT64_C(0), Events[EventIndex].FieldOffset));
			ASSERT_THAT(AreEqual(Original.Len() + 1, Events[EventIndex].RequestedCapacity));
			ASSERT_THAT(AreEqual(StringReservedCapacity, Events[EventIndex].ReservedCapacity));
			ASSERT_THAT(AreEqual(StringCapacityBytes, Events[EventIndex].ReservedBytes));
			ASSERT_THAT(AreEqual(
				ExactStringPhases[EventIndex] == EAllocationEventPhase::AllocationSucceeded
					? StringCapacityBytes
					: UINT64_C(0),
				Events[EventIndex].ActualAllocatedBytes));
		}
		const int32 CapturedEventCountBeforeUnscopedDecode = AllocationEventCount;
		FAngelscriptCacheReadBudget UnscopedStringBudget;
		FString UnscopedDecodedString;
		Result = FAngelscriptCacheSemanticArchive::DeserializeCanonicalString(
			StringBytes, ExactStringLimits, UnscopedStringBudget, UnscopedDecodedString);
		ASSERT_THAT(IsTrue(Result.IsSuccess()));
		ASSERT_THAT(AreEqual(Original, UnscopedDecodedString));
		ASSERT_THAT(AreEqual(CapturedEventCountBeforeUnscopedDecode,
			AllocationEventCount,
			TEXT("allocation observation is inert outside an explicit capture scope")));

		FAllocationEvent IndependentEventStorage[4];
		int32 IndependentEventCount = -1;
		bool bIndependentEventOverflowed = true;
		const int32 PrimaryEventCountBeforeIndependentDecode = AllocationEventCount;
		FAngelscriptCacheReadBudget IndependentStringBudget;
		FString IndependentDecodedString;
		Result = FAngelscriptCacheSemanticArchive::
			DeserializeCanonicalStringWithAllocationCaptureForTests(
				StringBytes,
				ExactStringLimits,
				IndependentStringBudget,
				FAllocationEventCaptureView(
					MakeArrayView(IndependentEventStorage),
					IndependentEventCount,
					bIndependentEventOverflowed),
				IndependentDecodedString);
		ASSERT_THAT(IsTrue(Result.IsSuccess()));
		ASSERT_THAT(AreEqual(Original, IndependentDecodedString));
		ASSERT_THAT(IsFalse(bIndependentEventOverflowed));
		ASSERT_THAT(AreEqual(4, IndependentEventCount));
		ASSERT_THAT(AreEqual(PrimaryEventCountBeforeIndependentDecode,
			AllocationEventCount,
			TEXT("explicit per-call capture cannot route into an earlier caller view")));

		FAllocationEvent OverflowEventStorage[1];
		int32 OverflowEventCount = -1;
		bool bOverflowEventOverflowed = false;
		FAngelscriptCacheReadBudget OverflowStringBudget;
		FString OverflowDecodedString;
		Result = FAngelscriptCacheSemanticArchive::
			DeserializeCanonicalStringWithAllocationCaptureForTests(
				StringBytes,
				ExactStringLimits,
				OverflowStringBudget,
				FAllocationEventCaptureView(
					MakeArrayView(OverflowEventStorage),
					OverflowEventCount,
					bOverflowEventOverflowed),
				OverflowDecodedString);
		ASSERT_THAT(IsTrue(Result.IsSuccess()));
		ASSERT_THAT(AreEqual(Original, OverflowDecodedString));
		ASSERT_THAT(AreEqual(1, OverflowEventCount));
		ASSERT_THAT(IsTrue(bOverflowEventOverflowed,
			TEXT("a full caller-owned event view must report overflow without growing")));
		ASSERT_THAT(AreEqual(EAllocationEventPhase::BudgetAttempt,
			OverflowEventStorage[0].Phase));

		int32 ZeroCapacityEventCount = -1;
		bool bZeroCapacityEventOverflowed = false;
		FAngelscriptCacheReadBudget ZeroCapacityStringBudget;
		FString ZeroCapacityDecodedString;
		Result = FAngelscriptCacheSemanticArchive::
			DeserializeCanonicalStringWithAllocationCaptureForTests(
				StringBytes,
				ExactStringLimits,
				ZeroCapacityStringBudget,
				FAllocationEventCaptureView(
					TArrayView<FAllocationEvent>(),
					ZeroCapacityEventCount,
					bZeroCapacityEventOverflowed),
				ZeroCapacityDecodedString);
		ASSERT_THAT(IsTrue(Result.IsSuccess()));
		ASSERT_THAT(AreEqual(Original, ZeroCapacityDecodedString));
		ASSERT_THAT(AreEqual(0, ZeroCapacityEventCount));
		ASSERT_THAT(IsTrue(bZeroCapacityEventOverflowed,
			TEXT("a zero-capacity event view must report overflow without writing")));

		int32 PositiveNullEventCount = -1;
		bool bPositiveNullEventOverflowed = false;
		FAngelscriptCacheReadBudget PositiveNullStringBudget;
		FString PositiveNullDecodedString;
		const TArrayView<FAllocationEvent> PositiveNullEventStorage(
			static_cast<FAllocationEvent*>(nullptr), 1);
		Result = FAngelscriptCacheSemanticArchive::
			DeserializeCanonicalStringWithAllocationCaptureForTests(
				StringBytes,
				ExactStringLimits,
				PositiveNullStringBudget,
				FAllocationEventCaptureView(
					PositiveNullEventStorage,
					PositiveNullEventCount,
					bPositiveNullEventOverflowed),
				PositiveNullDecodedString);
		ASSERT_THAT(AreEqual(0, PositiveNullEventCount,
			TEXT("an invalid positive-null capture must not observe decode events")));
		ASSERT_THAT(IsTrue(Result.IsSuccess()));
		ASSERT_THAT(AreEqual(Original, PositiveNullDecodedString));
		ASSERT_THAT(AreEqual(0, PositiveNullEventCount));
		ASSERT_THAT(IsTrue(bPositiveNullEventOverflowed));

		FAngelscriptCacheReadLimits TotalShortStringLimits = ExactStringLimits;
		TotalShortStringLimits.MaxTotalDecodedBytes = StringCapacityBytes - 1;
		FAngelscriptCacheReadBudget TotalShortStringBudget;
		ASSERT_THAT(IsTrue(TotalShortStringBudget.TryConsumeStored(3, TotalShortStringLimits)));
		ASSERT_THAT(IsTrue(TotalShortStringBudget.TryConsumeDecompressed(5, TotalShortStringLimits)));
		ASSERT_THAT(IsTrue(TotalShortStringBudget.TryConsumeReferencesAndRelocations(
			7, TotalShortStringLimits)));
		FString TotalShortOutput = FString::ChrN(64, TEXT('T'));
		ASSERT_THAT(IsTrue(TotalShortOutput.GetCharArray().GetAllocatedSize() > 0));
		Result = FAngelscriptCacheSemanticArchive::
			DeserializeCanonicalStringWithAllocationCaptureForTests(
				StringBytes,
				TotalShortStringLimits,
				TotalShortStringBudget,
				FAllocationEventCaptureView(
					MakeArrayView(AllocationEventStorage),
					AllocationEventCount,
					bAllocationEventOverflowed),
				TotalShortOutput);
		ASSERT_THAT(IsFalse(bAllocationEventOverflowed));
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::BudgetExceeded, Result.Error));
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationStage::PayloadDecode, Result.Stage));
		ASSERT_THAT(AreEqual(UINT64_C(0), Result.ByteOffset));
		ASSERT_THAT(IsTrue(TotalShortOutput.IsEmpty()));
		ASSERT_THAT(AreEqual(UINT64_C(0), TotalShortOutput.GetCharArray().GetAllocatedSize()));
		ASSERT_THAT(AreEqual(UINT64_C(3), TotalShortStringBudget.GetStoredBytes()));
		ASSERT_THAT(AreEqual(UINT64_C(5), TotalShortStringBudget.GetDecompressedBytes()));
		ASSERT_THAT(AreEqual(UINT64_C(7), TotalShortStringBudget.GetReferencesAndRelocations()));
		ASSERT_THAT(AreEqual(UINT64_C(0), TotalShortStringBudget.GetDecodedBytes()));
		ASSERT_THAT(AreEqual(UINT64_C(0), TotalShortStringBudget.GetResidentDecodedBytes()));
		ASSERT_THAT(AreEqual(UINT64_C(0), TotalShortStringBudget.GetPeakLiveResidentDecodedBytes()));
		Events = CapturedAllocationEvents();
		ASSERT_THAT(AreEqual(1, Events.Num()));
		ASSERT_THAT(AreEqual(EAllocationSite::StringCharacters, Events[0].Site));
		ASSERT_THAT(AreEqual(EAllocationEventPhase::BudgetAttempt, Events[0].Phase));
		ASSERT_THAT(AreEqual(UINT64_C(0), Events[0].FieldOffset));
		ASSERT_THAT(AreEqual(Original.Len() + 1, Events[0].RequestedCapacity));
		ASSERT_THAT(AreEqual(StringReservedCapacity, Events[0].ReservedCapacity));
		ASSERT_THAT(AreEqual(StringCapacityBytes, Events[0].ReservedBytes));

		FAngelscriptCacheReadLimits ResidentShortStringLimits = ExactStringLimits;
		ResidentShortStringLimits.MaxResidentDecodedBytes = StringCapacityBytes - 1;
		FAngelscriptCacheReadBudget ResidentShortStringBudget;
		FString ResidentShortOutput = FString::ChrN(64, TEXT('R'));
		ASSERT_THAT(IsTrue(ResidentShortOutput.GetCharArray().GetAllocatedSize() > 0));
		Result = FAngelscriptCacheSemanticArchive::
			DeserializeCanonicalStringWithAllocationCaptureForTests(
				StringBytes,
				ResidentShortStringLimits,
				ResidentShortStringBudget,
				FAllocationEventCaptureView(
					MakeArrayView(AllocationEventStorage),
					AllocationEventCount,
					bAllocationEventOverflowed),
				ResidentShortOutput);
		ASSERT_THAT(IsFalse(bAllocationEventOverflowed));
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::BudgetExceeded, Result.Error));
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationStage::PayloadDecode, Result.Stage));
		ASSERT_THAT(AreEqual(UINT64_C(0), Result.ByteOffset));
		ASSERT_THAT(IsTrue(ResidentShortOutput.IsEmpty()));
		ASSERT_THAT(AreEqual(UINT64_C(0), ResidentShortOutput.GetCharArray().GetAllocatedSize()));
		ASSERT_THAT(AreEqual(UINT64_C(0), ResidentShortStringBudget.GetDecodedBytes()));
		ASSERT_THAT(AreEqual(UINT64_C(0), ResidentShortStringBudget.GetResidentDecodedBytes()));
		Events = CapturedAllocationEvents();
		ASSERT_THAT(AreEqual(1, Events.Num()));
		ASSERT_THAT(AreEqual(EAllocationSite::StringCharacters, Events[0].Site));
		ASSERT_THAT(AreEqual(EAllocationEventPhase::BudgetAttempt, Events[0].Phase));
		ASSERT_THAT(AreEqual(UINT64_C(0), Events[0].FieldOffset));
		ASSERT_THAT(AreEqual(Original.Len() + 1, Events[0].RequestedCapacity));
		ASSERT_THAT(AreEqual(StringReservedCapacity, Events[0].ReservedCapacity));
		ASSERT_THAT(AreEqual(StringCapacityBytes, Events[0].ReservedBytes));

		const TArray<uint8> EmptyStringBytes = {0, 0, 0, 0};
		FAngelscriptCacheReadLimits EmptyStringLimits;
		EmptyStringLimits.MaxTotalDecodedBytes = 0;
		EmptyStringLimits.MaxResidentDecodedBytes = 0;
		FAngelscriptCacheReadBudget EmptyStringBudget;
		FString EmptyStringOutput;
		Result = FAngelscriptCacheSemanticArchive::
			DeserializeCanonicalStringWithAllocationCaptureForTests(
				EmptyStringBytes,
				EmptyStringLimits,
				EmptyStringBudget,
				FAllocationEventCaptureView(
					MakeArrayView(AllocationEventStorage),
					AllocationEventCount,
					bAllocationEventOverflowed),
				EmptyStringOutput);
		ASSERT_THAT(IsFalse(bAllocationEventOverflowed));
		ASSERT_THAT(IsTrue(Result.IsSuccess()));
		ASSERT_THAT(IsTrue(EmptyStringOutput.IsEmpty()));
		ASSERT_THAT(AreEqual(UINT64_C(0), EmptyStringOutput.GetCharArray().GetAllocatedSize()));
		ASSERT_THAT(AreEqual(UINT64_C(0), EmptyStringBudget.GetDecodedBytes()));
		ASSERT_THAT(AreEqual(UINT64_C(0), EmptyStringBudget.GetResidentDecodedBytes()));
		ASSERT_THAT(AreEqual(0, AllocationEventCount));

		const int32 NestedSlackCount =
			FindAllocatorSlackCountForTests<FAngelscriptCachedDataType>();
		// A large element type may consume every allocator bin without leaving room
		// for another complete element. That is a valid exact-reserve strategy; the
		// TCHAR fixture above independently covers a real slack-capacity charge.
		const int32 NestedElementCount = NestedSlackCount != INDEX_NONE
			? NestedSlackCount
			: 3;
		const int32 NestedReservedCapacity =
			CalculateArrayReserveCapacityForTests<FAngelscriptCachedDataType>(
				NestedElementCount);
		ASSERT_THAT(IsTrue(NestedReservedCapacity >= NestedElementCount));

		FAngelscriptCachedDataType Nested = MakeInt32Type();
		FAngelscriptCachedDataType NestedChild = MakeInt32Type();
		NestedChild.OrderedSubTypes.Reserve(NestedElementCount);
		for (int32 Index = 0; Index < NestedElementCount; ++Index)
		{
			NestedChild.OrderedSubTypes.Add(MakeInt32Type());
		}
		Nested.OrderedSubTypes.Add(MoveTemp(NestedChild));
		TArray<uint8> NestedBytes;
		ASSERT_THAT(IsTrue(FAngelscriptCacheSemanticArchive::SerializeCanonicalDataType(
			Nested, NestedBytes).IsSuccess()));
		const int32 RootReservedCapacity =
			CalculateArrayReserveCapacityForTests<FAngelscriptCachedDataType>(1);
		const uint64 RootSubTypeCapacityBytes =
			static_cast<uint64>(RootReservedCapacity) * sizeof(FAngelscriptCachedDataType);
		const uint64 ChildSubTypeCapacityBytes =
			CalculateArrayReserveBytesForTests<FAngelscriptCachedDataType>(NestedElementCount);
		const uint64 RecursiveSubTypeCapacityBytes =
			RootSubTypeCapacityBytes + ChildSubTypeCapacityBytes;

		FAngelscriptCacheReadLimits ExactArrayLimits;
		ExactArrayLimits.MaxTotalDecodedBytes = RecursiveSubTypeCapacityBytes;
		ExactArrayLimits.MaxResidentDecodedBytes = RecursiveSubTypeCapacityBytes;
		FAngelscriptCacheReadBudget ExactArrayBudget;
		FAngelscriptCachedDataType DecodedType;
		Result = FAngelscriptCacheSemanticArchive::
			DeserializeCanonicalDataTypeWithAllocationCaptureForTests(
				NestedBytes,
				ExactArrayLimits,
				ExactArrayBudget,
				FAllocationEventCaptureView(
					MakeArrayView(AllocationEventStorage),
					AllocationEventCount,
					bAllocationEventOverflowed),
				DecodedType);
		ASSERT_THAT(IsFalse(bAllocationEventOverflowed));
		ASSERT_THAT(IsTrue(Result.IsSuccess()));
		ASSERT_THAT(AreEqual(1, DecodedType.OrderedSubTypes.Num()));
		ASSERT_THAT(AreEqual(NestedElementCount,
			DecodedType.OrderedSubTypes[0].OrderedSubTypes.Num()));
		ASSERT_THAT(AreEqual(RecursiveSubTypeCapacityBytes, ExactArrayBudget.GetDecodedBytes()));
		ASSERT_THAT(AreEqual(RecursiveSubTypeCapacityBytes,
			ExactArrayBudget.GetResidentDecodedBytes()));
		ASSERT_THAT(AreEqual(RootSubTypeCapacityBytes,
			DecodedType.OrderedSubTypes.GetAllocatedSize()));
		ASSERT_THAT(AreEqual(ChildSubTypeCapacityBytes,
			DecodedType.OrderedSubTypes[0].OrderedSubTypes.GetAllocatedSize()));
		Events = CapturedAllocationEvents();
		ASSERT_THAT(AreEqual(8, Events.Num()));
		const EAllocationEventPhase ExactArrayPhases[] = {
			EAllocationEventPhase::BudgetAttempt,
			EAllocationEventPhase::BudgetAccepted,
			EAllocationEventPhase::AllocationAttempt,
			EAllocationEventPhase::AllocationSucceeded,
			EAllocationEventPhase::BudgetAttempt,
			EAllocationEventPhase::BudgetAccepted,
			EAllocationEventPhase::AllocationAttempt,
			EAllocationEventPhase::AllocationSucceeded};
		const int32 ExactArrayEventCount =
			FMath::Min(Events.Num(), static_cast<int32>(UE_ARRAY_COUNT(ExactArrayPhases)));
		for (int32 EventIndex = 0; EventIndex < ExactArrayEventCount; ++EventIndex)
		{
			const bool bRootEvent = EventIndex < 4;
			ASSERT_THAT(AreEqual(EAllocationSite::TypedArrayElements, Events[EventIndex].Site));
			ASSERT_THAT(AreEqual(ExactArrayPhases[EventIndex], Events[EventIndex].Phase));
			ASSERT_THAT(AreEqual(bRootEvent ? UINT64_C(7) : UINT64_C(18),
				Events[EventIndex].FieldOffset));
			ASSERT_THAT(AreEqual(bRootEvent ? 1 : NestedElementCount,
				Events[EventIndex].RequestedCapacity));
			ASSERT_THAT(AreEqual(bRootEvent ? RootReservedCapacity : NestedReservedCapacity,
				Events[EventIndex].ReservedCapacity));
			ASSERT_THAT(AreEqual(bRootEvent ? RootSubTypeCapacityBytes : ChildSubTypeCapacityBytes,
				Events[EventIndex].ReservedBytes));
			ASSERT_THAT(AreEqual(
				ExactArrayPhases[EventIndex] == EAllocationEventPhase::AllocationSucceeded
					? (bRootEvent ? RootSubTypeCapacityBytes : ChildSubTypeCapacityBytes)
					: UINT64_C(0),
				Events[EventIndex].ActualAllocatedBytes));
		}

		FAngelscriptCacheReadLimits TotalShortArrayLimits = ExactArrayLimits;
		TotalShortArrayLimits.MaxTotalDecodedBytes = RecursiveSubTypeCapacityBytes - 1;
		FAngelscriptCacheReadBudget TotalShortArrayBudget;
		FAngelscriptCachedDataType TotalShortTypeOutput;
		TotalShortTypeOutput.OrderedSubTypes.Reserve(8);
		ASSERT_THAT(IsTrue(TotalShortTypeOutput.OrderedSubTypes.GetAllocatedSize() > 0));
		Result = FAngelscriptCacheSemanticArchive::
			DeserializeCanonicalDataTypeWithAllocationCaptureForTests(
				NestedBytes,
				TotalShortArrayLimits,
				TotalShortArrayBudget,
				FAllocationEventCaptureView(
					MakeArrayView(AllocationEventStorage),
					AllocationEventCount,
					bAllocationEventOverflowed),
				TotalShortTypeOutput);
		ASSERT_THAT(IsFalse(bAllocationEventOverflowed));
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::BudgetExceeded, Result.Error));
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationStage::PayloadDecode, Result.Stage));
		ASSERT_THAT(AreEqual(UINT64_C(18), Result.ByteOffset));
		ASSERT_THAT(IsTrue(TotalShortTypeOutput.OrderedSubTypes.IsEmpty()));
		ASSERT_THAT(AreEqual(UINT64_C(0), TotalShortTypeOutput.OrderedSubTypes.GetAllocatedSize()));
		ASSERT_THAT(AreEqual(RootSubTypeCapacityBytes, TotalShortArrayBudget.GetDecodedBytes()));
		ASSERT_THAT(AreEqual(UINT64_C(0),
			TotalShortArrayBudget.GetResidentDecodedBytes()));
		ASSERT_THAT(AreEqual(UINT64_C(0),
			TotalShortArrayBudget.GetTemporaryResidentDecodedBytes()));
		ASSERT_THAT(AreEqual(RootSubTypeCapacityBytes,
			TotalShortArrayBudget.GetPeakLiveResidentDecodedBytes()));
		Events = CapturedAllocationEvents();
		ASSERT_THAT(AreEqual(5, Events.Num()));
		ASSERT_THAT(AreEqual(EAllocationEventPhase::AllocationAttempt, Events[2].Phase));
		ASSERT_THAT(AreEqual(EAllocationEventPhase::AllocationSucceeded, Events[3].Phase));
		ASSERT_THAT(AreEqual(UINT64_C(7), Events[3].FieldOffset));
		ASSERT_THAT(AreEqual(RootSubTypeCapacityBytes, Events[3].ActualAllocatedBytes));
		ASSERT_THAT(AreEqual(EAllocationEventPhase::BudgetAttempt, Events[4].Phase));
		ASSERT_THAT(AreEqual(UINT64_C(18), Events[4].FieldOffset));
		ASSERT_THAT(AreEqual(NestedElementCount, Events[4].RequestedCapacity));
		ASSERT_THAT(AreEqual(NestedReservedCapacity, Events[4].ReservedCapacity));
		ASSERT_THAT(AreEqual(ChildSubTypeCapacityBytes, Events[4].ReservedBytes));

		FAngelscriptCacheReadLimits ResidentShortArrayLimits = ExactArrayLimits;
		ResidentShortArrayLimits.MaxResidentDecodedBytes = RecursiveSubTypeCapacityBytes - 1;
		FAngelscriptCacheReadBudget ResidentShortArrayBudget;
		FAngelscriptCachedDataType ResidentShortTypeOutput;
		ResidentShortTypeOutput.OrderedSubTypes.Reserve(8);
		ASSERT_THAT(IsTrue(ResidentShortTypeOutput.OrderedSubTypes.GetAllocatedSize() > 0));
		Result = FAngelscriptCacheSemanticArchive::
			DeserializeCanonicalDataTypeWithAllocationCaptureForTests(
				NestedBytes,
				ResidentShortArrayLimits,
				ResidentShortArrayBudget,
				FAllocationEventCaptureView(
					MakeArrayView(AllocationEventStorage),
					AllocationEventCount,
					bAllocationEventOverflowed),
				ResidentShortTypeOutput);
		ASSERT_THAT(IsFalse(bAllocationEventOverflowed));
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::BudgetExceeded, Result.Error));
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationStage::PayloadDecode, Result.Stage));
		ASSERT_THAT(AreEqual(UINT64_C(18), Result.ByteOffset));
		ASSERT_THAT(IsTrue(ResidentShortTypeOutput.OrderedSubTypes.IsEmpty()));
		ASSERT_THAT(AreEqual(UINT64_C(0), ResidentShortTypeOutput.OrderedSubTypes.GetAllocatedSize()));
		ASSERT_THAT(AreEqual(RootSubTypeCapacityBytes, ResidentShortArrayBudget.GetDecodedBytes()));
		ASSERT_THAT(AreEqual(UINT64_C(0),
			ResidentShortArrayBudget.GetResidentDecodedBytes()));
		ASSERT_THAT(AreEqual(UINT64_C(0),
			ResidentShortArrayBudget.GetTemporaryResidentDecodedBytes()));
		ASSERT_THAT(AreEqual(RootSubTypeCapacityBytes,
			ResidentShortArrayBudget.GetPeakLiveResidentDecodedBytes()));
		Events = CapturedAllocationEvents();
		ASSERT_THAT(AreEqual(5, Events.Num()));
		ASSERT_THAT(AreEqual(EAllocationEventPhase::AllocationAttempt, Events[2].Phase));
		ASSERT_THAT(AreEqual(EAllocationEventPhase::AllocationSucceeded, Events[3].Phase));
		ASSERT_THAT(AreEqual(UINT64_C(7), Events[3].FieldOffset));
		ASSERT_THAT(AreEqual(RootSubTypeCapacityBytes, Events[3].ActualAllocatedBytes));
		ASSERT_THAT(AreEqual(EAllocationEventPhase::BudgetAttempt, Events[4].Phase));
		ASSERT_THAT(AreEqual(UINT64_C(18), Events[4].FieldOffset));
		ASSERT_THAT(AreEqual(NestedElementCount, Events[4].RequestedCapacity));
		ASSERT_THAT(AreEqual(NestedReservedCapacity, Events[4].ReservedCapacity));
		ASSERT_THAT(AreEqual(ChildSubTypeCapacityBytes, Events[4].ReservedBytes));
	}

	TEST_METHOD(CanonicalAllocationCaptureIsBehaviorallyInertAtEveryCapacity)
	{
		using namespace AngelscriptCachePrimitiveTests_Private;
		using namespace AngelscriptCacheCanonicalCodecTestHooks;

		struct FBudgetSnapshot
		{
			uint64 Stored = 0;
			uint64 Decompressed = 0;
			uint64 Decoded = 0;
			uint64 Resident = 0;
			uint64 Temporary = 0;
			uint64 PeakLive = 0;
			uint64 References = 0;
		};
		auto CaptureBudget = [](const FAngelscriptCacheReadBudget& Budget)
		{
			return FBudgetSnapshot{
				Budget.GetStoredBytes(),
				Budget.GetDecompressedBytes(),
				Budget.GetDecodedBytes(),
				Budget.GetResidentDecodedBytes(),
				Budget.GetTemporaryResidentDecodedBytes(),
				Budget.GetPeakLiveResidentDecodedBytes(),
				Budget.GetReferencesAndRelocations()};
		};

		struct FStringDecodeRun
		{
			FAngelscriptCacheValidationResult Result;
			FBudgetSnapshot Budget;
			FString Output;
			int32 EventCount = -1;
			bool bOverflowed = false;
		};
		struct FDataTypeDecodeRun
		{
			FAngelscriptCacheValidationResult Result;
			FBudgetSnapshot Budget;
			FAngelscriptCachedDataType Output;
			int32 EventCount = -1;
			bool bOverflowed = false;
		};

		auto SeedBudget = [](FAngelscriptCacheReadBudget& Budget,
			const FAngelscriptCacheReadLimits& Limits)
		{
			return Budget.TryConsumeStored(3, Limits)
				&& Budget.TryConsumeDecompressed(5, Limits)
				&& Budget.TryConsumeReferencesAndRelocations(7, Limits);
		};
		auto AssertEquivalentResult = [&](const FAngelscriptCacheValidationResult& Expected,
			const FAngelscriptCacheValidationResult& Actual)
		{
			ASSERT_THAT(AreEqual(Expected.Error, Actual.Error));
			ASSERT_THAT(AreEqual(Expected.Class, Actual.Class));
			ASSERT_THAT(AreEqual(Expected.Stage, Actual.Stage));
			ASSERT_THAT(AreEqual(Expected.RecordKind, Actual.RecordKind));
			ASSERT_THAT(AreEqual(Expected.ByteOffset, Actual.ByteOffset));
		};
		auto AssertEquivalentBudget = [&](const FBudgetSnapshot& Expected,
			const FBudgetSnapshot& Actual)
		{
			ASSERT_THAT(AreEqual(Expected.Stored, Actual.Stored));
			ASSERT_THAT(AreEqual(Expected.Decompressed, Actual.Decompressed));
			ASSERT_THAT(AreEqual(Expected.Decoded, Actual.Decoded));
			ASSERT_THAT(AreEqual(Expected.Resident, Actual.Resident));
			ASSERT_THAT(AreEqual(Expected.Temporary, Actual.Temporary));
			ASSERT_THAT(AreEqual(Expected.PeakLive, Actual.PeakLive));
			ASSERT_THAT(AreEqual(Expected.References, Actual.References));
		};

		const FAngelscriptCacheReadLimits Limits;
		const FString OriginalString = TEXT("observer-parity");
		TArray<uint8> StringBytes;
		ASSERT_THAT(IsTrue(FAngelscriptCacheSemanticArchive::SerializeCanonicalString(
			OriginalString, StringBytes).IsSuccess()));
		auto RunString = [&](const int32 CaptureCapacity)
		{
			FStringDecodeRun Run;
			FAngelscriptCacheReadBudget Budget;
			if (!SeedBudget(Budget, Limits))
			{
				TestRunner->AddError(TEXT("String parity fixture budget seed failed"));
			}
			Run.Output = TEXT("previous-output");
			if (CaptureCapacity < 0)
			{
				Run.Result = FAngelscriptCacheSemanticArchive::DeserializeCanonicalString(
					StringBytes, Limits, Budget, Run.Output);
			}
			else
			{
				FAllocationEvent Events[4];
				Run.EventCount = -1;
				Run.bOverflowed = false;
				Run.Result = FAngelscriptCacheSemanticArchive::
					DeserializeCanonicalStringWithAllocationCaptureForTests(
						StringBytes,
						Limits,
						Budget,
						FAllocationEventCaptureView(
							TArrayView<FAllocationEvent>(Events, CaptureCapacity),
							Run.EventCount,
							Run.bOverflowed),
						Run.Output);
			}
			Run.Budget = CaptureBudget(Budget);
			return Run;
		};

		const FStringDecodeRun StringBaseline = RunString(-1);
		ASSERT_THAT(IsTrue(StringBaseline.Result.IsSuccess()));
		ASSERT_THAT(AreEqual(OriginalString, StringBaseline.Output));
		for (const int32 CaptureCapacity : {0, 3, 4})
		{
			const FStringDecodeRun Captured = RunString(CaptureCapacity);
			AssertEquivalentResult(StringBaseline.Result, Captured.Result);
			AssertEquivalentBudget(StringBaseline.Budget, Captured.Budget);
			ASSERT_THAT(AreEqual(StringBaseline.Output, Captured.Output));
			ASSERT_THAT(AreEqual(CaptureCapacity, Captured.EventCount));
			ASSERT_THAT(AreEqual(CaptureCapacity < 4, Captured.bOverflowed));
		}

		FAngelscriptCachedDataType OriginalType = MakeInt32Type();
		FAngelscriptCachedDataType Child = MakeInt32Type();
		Child.OrderedSubTypes.Add(MakeInt32Type());
		OriginalType.OrderedSubTypes.Add(MoveTemp(Child));
		TArray<uint8> TypeBytes;
		ASSERT_THAT(IsTrue(FAngelscriptCacheSemanticArchive::SerializeCanonicalDataType(
			OriginalType, TypeBytes).IsSuccess()));
		auto RunDataType = [&](const int32 CaptureCapacity)
		{
			FDataTypeDecodeRun Run;
			FAngelscriptCacheReadBudget Budget;
			if (!SeedBudget(Budget, Limits))
			{
				TestRunner->AddError(TEXT("DataType parity fixture budget seed failed"));
			}
			Run.Output.OrderedSubTypes.Reserve(3);
			if (CaptureCapacity < 0)
			{
				Run.Result = FAngelscriptCacheSemanticArchive::DeserializeCanonicalDataType(
					TypeBytes, Limits, Budget, Run.Output);
			}
			else
			{
				FAllocationEvent Events[8];
				Run.EventCount = -1;
				Run.bOverflowed = false;
				Run.Result = FAngelscriptCacheSemanticArchive::
					DeserializeCanonicalDataTypeWithAllocationCaptureForTests(
						TypeBytes,
						Limits,
						Budget,
						FAllocationEventCaptureView(
							TArrayView<FAllocationEvent>(Events, CaptureCapacity),
							Run.EventCount,
							Run.bOverflowed),
						Run.Output);
			}
			Run.Budget = CaptureBudget(Budget);
			return Run;
		};

		const FDataTypeDecodeRun TypeBaseline = RunDataType(-1);
		ASSERT_THAT(IsTrue(TypeBaseline.Result.IsSuccess()));
		TArray<uint8> TypeBaselineBytes;
		ASSERT_THAT(IsTrue(FAngelscriptCacheSemanticArchive::SerializeCanonicalDataType(
			TypeBaseline.Output, TypeBaselineBytes).IsSuccess()));
		ASSERT_THAT(IsTrue(TypeBaselineBytes == TypeBytes));
		for (const int32 CaptureCapacity : {0, 7, 8})
		{
			const FDataTypeDecodeRun Captured = RunDataType(CaptureCapacity);
			AssertEquivalentResult(TypeBaseline.Result, Captured.Result);
			AssertEquivalentBudget(TypeBaseline.Budget, Captured.Budget);
			TArray<uint8> CapturedBytes;
			ASSERT_THAT(IsTrue(FAngelscriptCacheSemanticArchive::SerializeCanonicalDataType(
				Captured.Output, CapturedBytes).IsSuccess()));
			ASSERT_THAT(IsTrue(CapturedBytes == TypeBaselineBytes));
			ASSERT_THAT(AreEqual(CaptureCapacity, Captured.EventCount));
			ASSERT_THAT(AreEqual(CaptureCapacity < 8, Captured.bOverflowed));
		}
	}

	TEST_METHOD(ConcurrentCanonicalDecodesKeepCaptureAndBudgetPerCaller)
	{
		using namespace AngelscriptCachePrimitiveTests_Private;
		using namespace AngelscriptCacheCanonicalCodecTestHooks;

		const FString OriginalString = FString::ChrN(37, TEXT('S'));
		TArray<uint8> StringBytes;
		ASSERT_THAT(IsTrue(FAngelscriptCacheSemanticArchive::SerializeCanonicalString(
			OriginalString, StringBytes).IsSuccess()));

		FAngelscriptCachedDataType OriginalType = MakeInt32Type();
		OriginalType.OrderedSubTypes.Add(MakeInt32Type());
		TArray<uint8> TypeBytes;
		ASSERT_THAT(IsTrue(FAngelscriptCacheSemanticArchive::SerializeCanonicalDataType(
			OriginalType, TypeBytes).IsSuccess()));

		struct FDecodeSummary
		{
			EAngelscriptCacheValidationError Error =
				EAngelscriptCacheValidationError::InvalidPresence;
			bool bValueMatches = false;
			uint64 DecodedBytes = 0;
			uint64 ResidentDecodedBytes = 0;
			uint64 TemporaryResidentDecodedBytes = 0;
			int32 EventCount = 0;
			bool bOverflowed = false;
			FAllocationEvent Events[16];
		};

		TFuture<FDecodeSummary> StringFuture = Async(
			EAsyncExecution::ThreadPool,
			[StringBytes, OriginalString]()
			{
				FDecodeSummary Summary;
				FAngelscriptCacheReadLimits Limits;
				FAngelscriptCacheReadBudget Budget;
				FString Output;
				const FAngelscriptCacheValidationResult Result =
					FAngelscriptCacheSemanticArchive::
						DeserializeCanonicalStringWithAllocationCaptureForTests(
							StringBytes, Limits, Budget,
							FAllocationEventCaptureView(
								MakeArrayView(Summary.Events),
								Summary.EventCount,
								Summary.bOverflowed),
							Output);
				Summary.Error = Result.Error;
				Summary.bValueMatches = Output == OriginalString;
				Summary.DecodedBytes = Budget.GetDecodedBytes();
				Summary.ResidentDecodedBytes = Budget.GetResidentDecodedBytes();
				Summary.TemporaryResidentDecodedBytes =
					Budget.GetTemporaryResidentDecodedBytes();
				return Summary;
			});

		TFuture<FDecodeSummary> TypeFuture = Async(
			EAsyncExecution::ThreadPool,
			[TypeBytes]()
			{
				FDecodeSummary Summary;
				FAngelscriptCacheReadLimits Limits;
				FAngelscriptCacheReadBudget Budget;
				FAngelscriptCachedDataType Output;
				const FAngelscriptCacheValidationResult Result =
					FAngelscriptCacheSemanticArchive::
						DeserializeCanonicalDataTypeWithAllocationCaptureForTests(
							TypeBytes, Limits, Budget,
							FAllocationEventCaptureView(
								MakeArrayView(Summary.Events),
								Summary.EventCount,
								Summary.bOverflowed),
							Output);
				Summary.Error = Result.Error;
				Summary.bValueMatches = Output.Kind
						== EAngelscriptCachedDataTypeKind::Primitive
					&& Output.OrderedSubTypes.Num() == 1
					&& Output.OrderedSubTypes[0].Primitive
						== EAngelscriptCachedPrimitiveType::Int32;
				Summary.DecodedBytes = Budget.GetDecodedBytes();
				Summary.ResidentDecodedBytes = Budget.GetResidentDecodedBytes();
				Summary.TemporaryResidentDecodedBytes =
					Budget.GetTemporaryResidentDecodedBytes();
				return Summary;
			});

		const FDecodeSummary StringSummary = StringFuture.Get();
		const FDecodeSummary TypeSummary = TypeFuture.Get();
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::None,
			StringSummary.Error));
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::None,
			TypeSummary.Error));
		ASSERT_THAT(IsTrue(StringSummary.bValueMatches));
		ASSERT_THAT(IsTrue(TypeSummary.bValueMatches));
		ASSERT_THAT(IsFalse(StringSummary.bOverflowed));
		ASSERT_THAT(IsFalse(TypeSummary.bOverflowed));
		ASSERT_THAT(AreEqual(4, StringSummary.EventCount));
		ASSERT_THAT(AreEqual(4, TypeSummary.EventCount));
		for (int32 EventIndex = 0; EventIndex < StringSummary.EventCount; ++EventIndex)
		{
			ASSERT_THAT(AreEqual(EAllocationSite::StringCharacters,
				StringSummary.Events[EventIndex].Site));
		}
		for (int32 EventIndex = 0; EventIndex < TypeSummary.EventCount; ++EventIndex)
		{
			ASSERT_THAT(AreEqual(EAllocationSite::TypedArrayElements,
				TypeSummary.Events[EventIndex].Site));
		}
		ASSERT_THAT(IsTrue(StringSummary.DecodedBytes > 0));
		ASSERT_THAT(IsTrue(TypeSummary.DecodedBytes > 0));
		ASSERT_THAT(AreEqual(StringSummary.DecodedBytes,
			StringSummary.ResidentDecodedBytes));
		ASSERT_THAT(AreEqual(TypeSummary.DecodedBytes,
			TypeSummary.ResidentDecodedBytes));
		ASSERT_THAT(AreEqual(UINT64_C(0),
			StringSummary.TemporaryResidentDecodedBytes));
		ASSERT_THAT(AreEqual(UINT64_C(0),
			TypeSummary.TemporaryResidentDecodedBytes));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
