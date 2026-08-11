#include "CQTest.h"

#include "Cache/AngelscriptCacheCompilerBridge.h"

#if WITH_ANGELSCRIPT_UNITTESTS

namespace AngelscriptCacheFunctionInputTests_Private
{
	static FAngelscriptHash256 MakeHash(const uint8 Seed)
	{
		FBlake3Hash::ByteArray Bytes{};
		for (int32 Index = 0; Index < static_cast<int32>(sizeof(Bytes)); ++Index)
		{
			Bytes[Index] = static_cast<uint8>(Seed + Index);
		}
		return FAngelscriptHash256{FBlake3Hash(Bytes)};
	}

	class FCountingExternalResolver final
		: public IAngelscriptCacheCurrentSymbolResolver
	{
	public:
		TOptional<FAngelscriptCacheCurrentSymbol> Resolve(
			const EAngelscriptCacheReferenceKind ReferenceKind,
			const FAngelscriptHash256& StableKey) const override
		{
			++ResolveCount;
			if (ReferenceKind == EAngelscriptCacheReferenceKind::EnvironmentSymbol
				&& StableKey == ExpectedKey)
			{
				return FAngelscriptCacheCurrentSymbol{CurrentAbi, {}, {}};
			}
			return {};
		}

		FAngelscriptHash256 ExpectedKey;
		FAngelscriptHash256 CurrentAbi;
		mutable int32 ResolveCount = 0;
	};

	static FAngelscriptCacheSemanticDependency MakeDependency(
		const EAngelscriptCacheSemanticDependencyKind Kind,
		const EAngelscriptCacheReferenceKind ReferenceKind,
		const FAngelscriptHash256& StableKey,
		const FAngelscriptHash256& Abi,
		const TOptional<FAngelscriptHash256>& Content = {})
	{
		return {Kind, {ReferenceKind, StableKey, Abi}, Content};
	}
}

TEST_CLASS_WITH_FLAGS(FAngelscriptCacheFunctionInputTests,
	"Angelscript.TestModule.Cache.FunctionInput",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(CurrentDependenciesProduceDeterministicMatchAndSelectiveMiss)
	{
		using namespace AngelscriptCacheFunctionInputTests_Private;
		const FAngelscriptHash256 GlobalKey = MakeHash(0x10);
		const FAngelscriptHash256 GlobalAbi = MakeHash(0x20);
		const FAngelscriptHash256 GlobalValue = MakeHash(0x30);
		const FAngelscriptHash256 EnvironmentKey = MakeHash(0x40);
		const FAngelscriptHash256 EnvironmentAbi = MakeHash(0x50);

		FAngelscriptCachedModuleInterface Interface;
		Interface.ModuleKey.Hash = MakeHash(0x60);
		FAngelscriptCachedDeclaration& GlobalDeclaration =
			Interface.Declarations.AddDefaulted_GetRef();
		GlobalDeclaration.DeclarationKind =
			EAngelscriptCacheDeclarationKind::Global;
		GlobalDeclaration.StableKey = GlobalKey;
		GlobalDeclaration.SignatureHash = GlobalAbi;

		FAngelscriptCachedModuleState State;
		State.ModuleKey = Interface.ModuleKey;
		FAngelscriptCachedHardValue& HardValue =
			State.HardValues.AddDefaulted_GetRef();
		HardValue.Owner = {
			EAngelscriptCacheReferenceKind::ScriptGlobal,
			GlobalKey,
			GlobalAbi,
		};
		HardValue.HardValueHash = GlobalValue;

		FCountingExternalResolver External;
		External.ExpectedKey = EnvironmentKey;
		External.CurrentAbi = EnvironmentAbi;

		FAngelscriptCachedFunctionBody CachedBody;
		CachedBody.FunctionSourceDigest.Hash = MakeHash(0x70);
		const FAngelscriptCacheSemanticDependency GlobalDependency = MakeDependency(
			EAngelscriptCacheSemanticDependencyKind::HardValue,
			EAngelscriptCacheReferenceKind::ScriptGlobal,
			GlobalKey, GlobalAbi, GlobalValue);
		const FAngelscriptCacheSemanticDependency EnvironmentDependency = MakeDependency(
			EAngelscriptCacheSemanticDependencyKind::EnvironmentAbi,
			EAngelscriptCacheReferenceKind::EnvironmentSymbol,
			EnvironmentKey, EnvironmentAbi);
		TArray<FAngelscriptCacheSemanticDependency> Observed = {
			EnvironmentDependency,
			GlobalDependency,
			GlobalDependency,
		};
		FAngelscriptCacheValidationResult CanonicalResult =
			FAngelscriptCacheCompilerBridge::CanonicalizeActualDependencies(
				Observed, CachedBody.ActualDependencies);
		ASSERT_THAT(IsTrue(CanonicalResult.IsSuccess()));
		ASSERT_THAT(AreEqual(2, CachedBody.ActualDependencies.Num()));

		FAngelscriptCacheFunctionInputAuthorities Authorities;
		Authorities.ModuleInterface = &Interface;
		Authorities.ModuleState = &State;
		Authorities.ExternalSymbols = &External;
		const FAngelscriptCacheFunctionInputResolution First =
			FAngelscriptCacheCompilerBridge::ResolveCurrentFunctionInput(
				CachedBody, CachedBody.FunctionSourceDigest, Authorities);
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheFunctionInputStatus::ResolvedMismatch,
			First.Status));
		ASSERT_THAT(IsFalse(First.CurrentInputDigest.Hash.IsZero()));
		CachedBody.FunctionInputDigest = First.CurrentInputDigest;

		const FAngelscriptCacheFunctionInputResolution Match =
			FAngelscriptCacheCompilerBridge::ResolveCurrentFunctionInput(
				CachedBody, CachedBody.FunctionSourceDigest, Authorities);
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheFunctionInputStatus::ResolvedMatch,
			Match.Status));
		ASSERT_THAT(IsTrue(Match.CurrentInputDigest.Hash
			== CachedBody.FunctionInputDigest.Hash));

		HardValue.HardValueHash = MakeHash(0x31);
		const FAngelscriptCacheFunctionInputResolution HardValueMiss =
			FAngelscriptCacheCompilerBridge::ResolveCurrentFunctionInput(
				CachedBody, CachedBody.FunctionSourceDigest, Authorities);
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheFunctionInputStatus::ResolvedMismatch,
			HardValueMiss.Status));
		ASSERT_THAT(IsFalse(HardValueMiss.CurrentInputDigest.Hash
			== CachedBody.FunctionInputDigest.Hash));

		const int32 ResolveCountBeforeSourceChange = External.ResolveCount;
		FAngelscriptFunctionSourceDigest ChangedSource =
			CachedBody.FunctionSourceDigest;
		ChangedSource.Hash = MakeHash(0x71);
		const FAngelscriptCacheFunctionInputResolution SourceMiss =
			FAngelscriptCacheCompilerBridge::ResolveCurrentFunctionInput(
				CachedBody, ChangedSource, Authorities);
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheFunctionInputStatus::SourceChanged,
			SourceMiss.Status));
		ASSERT_THAT(IsTrue(SourceMiss.CurrentInputDigest.Hash.IsZero()));
		ASSERT_THAT(AreEqual(
			ResolveCountBeforeSourceChange, External.ResolveCount));
	}

	TEST_METHOD(InvocationKindAndOptionsContributeToFunctionSourceDigest)
	{
		asSBuildArtifactInvocation Invocation;
		Invocation.kind = asBUILD_ARTIFACT_INVOCATION_GENERATED_DEFAULT_DESTRUCTOR;
		Invocation.ineligibleReason = asBUILD_ARTIFACT_INELIGIBLE_NONE;
		Invocation.canonicalSource = "classFSourceDigest{intValue;}";
		Invocation.moduleName = "ASCacheV2FunctionInput";
		Invocation.ownerName = "FSourceDigest";
		Invocation.functionName = "~FSourceDigest";
		Invocation.declaration = "void FSourceDigest::~FSourceDigest()";
		Invocation.sourceSection = "FunctionInput.as";

		FAngelscriptFunctionSourceDigest First;
		FString Failure;
		ASSERT_THAT(IsTrue(
			FAngelscriptCacheCompilerBridge::TryBuildFunctionSourceDigest(
				Invocation,
				{TEXT("Debug=true"), TEXT("Optimize=true")},
				First,
				Failure)));
		ASSERT_THAT(IsTrue(Failure.IsEmpty()));

		FAngelscriptFunctionSourceDigest Reordered;
		ASSERT_THAT(IsTrue(
			FAngelscriptCacheCompilerBridge::TryBuildFunctionSourceDigest(
				Invocation,
				{TEXT("Optimize=true"), TEXT("Debug=true")},
				Reordered,
				Failure)));
		ASSERT_THAT(IsTrue(First.Hash == Reordered.Hash));

		Invocation.kind = asBUILD_ARTIFACT_INVOCATION_GENERATED_DEFAULT_CONSTRUCTOR;
		FAngelscriptFunctionSourceDigest DifferentKind;
		ASSERT_THAT(IsTrue(
			FAngelscriptCacheCompilerBridge::TryBuildFunctionSourceDigest(
				Invocation,
				{TEXT("Debug=true"), TEXT("Optimize=true")},
				DifferentKind,
				Failure)));
		ASSERT_THAT(IsFalse(First.Hash == DifferentKind.Hash));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
