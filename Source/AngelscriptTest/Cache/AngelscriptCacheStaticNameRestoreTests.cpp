#include "Cache/AngelscriptCacheCleanCapture.h"
#include "Cache/AngelscriptCacheRestore.h"

#include "AngelscriptTestMacros.h"
#include "CQTest.h"
#include "Shared/AngelscriptTestFixture.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FAngelscriptCacheStaticNameRestoreTests,
	"Angelscript.TestModule.Cache.StaticNameRestore",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	class FPackSource final : public IAngelscriptCachePackSource
	{
	public:
		explicit FPackSource(
			const TConstArrayView<FAngelscriptEncodedPack> InPacks)
			: Packs(InPacks)
		{
		}

		virtual bool TryGetCompletePack(
			const FAngelscriptHash256& PackId,
			TConstArrayView<uint8>& OutBytes) override
		{
			for (const FAngelscriptEncodedPack& Pack : Packs)
			{
				if (Pack.PackId == PackId)
				{
					OutBytes = Pack.Bytes;
					return true;
				}
			}

			OutBytes = {};
			return false;
		}

	private:
		TConstArrayView<FAngelscriptEncodedPack> Packs;
	};

	static FAngelscriptCacheCleanCaptureOptions MakeCaptureOptions()
	{
		FAngelscriptCacheCleanCaptureOptions Options;
		FAngelscriptCompatibilityDescriptor Compatibility;
		Compatibility.CanonicalInputs = {
			TEXT("CacheV2StaticNameRestore"),
			TEXT("VmExecutionCodec=5"),
		};
		Options.Compatibility =
			FAngelscriptArtifactIdentityBuilder::BuildCompatibilityKey(
				Compatibility);

		FAngelscriptContextDescriptor Context;
		Context.CanonicalInputs = {
			TEXT("SourceMount=Game"),
			TEXT("DebugSidecar=Enabled"),
		};
		Options.Context =
			FAngelscriptArtifactIdentityBuilder::BuildContextKey(Context);
		Options.Profile =
			FAngelscriptArtifactIdentityBuilder::BuildArtifactProfileKey(
				Options.Compatibility, Options.Context);
		Options.CanonicalCompileOptions = {
			TEXT("AutomaticImports=false"),
		};
		return Options;
	}

	static void AppendConsumerConflictAt(
		FAutomationTestBase& Test,
		const int32 ProducerIndex,
		const FName ExpectedName,
		const TCHAR* Coordinate)
	{
		while (FAngelscriptEngine::GetStaticNameCount() <= ProducerIndex)
		{
			const int32 NextIndex = FAngelscriptEngine::GetStaticNameCount();
			const FName ConflictName(*FString::Printf(
				TEXT("CacheV2ConsumerConflict_%s_%d"),
				Coordinate,
				NextIndex));
			const int32 AddedIndex =
				FAngelscriptEngine::GetOrAddStaticName(ConflictName);
			Test.TestEqual(
				FString::Printf(TEXT("Consumer conflict index %s"), Coordinate),
				AddedIndex,
				NextIndex);
		}

		FName ConsumerName;
		Test.TestTrue(
			FString::Printf(TEXT("Consumer should own producer index %s"), Coordinate),
			FAngelscriptEngine::TryGetStaticName(ProducerIndex, ConsumerName));
		Test.TestNotEqual(
			FString::Printf(TEXT("Consumer index %s must deliberately disagree"), Coordinate),
			ConsumerName,
			ExpectedName);
		Test.AddInfo(FString::Printf(
			TEXT("IC-454 consumer index conflict: Coordinate=%s Index=%d Expected=%s Actual=%s"),
			Coordinate,
			ProducerIndex,
			*ExpectedName.ToString(),
			*ConsumerName.ToString()));
	}

public:
	TEST_METHOD(DuplicateAndDistinctLiteralsIgnoreProducerIndicesInFreshEngine)
	{
		static const FName AlphaName(TEXT("CacheV2StaticNameAlpha"));
		static const FName BetaName(TEXT("CacheV2StaticNameBeta"));
		const FAngelscriptCacheCleanCaptureOptions Options =
			MakeCaptureOptions();
		FAngelscriptCacheCleanModuleArtifacts Artifacts;
		int32 ProducerAlphaIndex = INDEX_NONE;
		int32 ProducerBetaIndex = INDEX_NONE;
		{
			FAngelscriptTestFixture Producer(
				*TestRunner, ETestEngineMode::IsolatedFull);
			ASSERT_THAT(IsTrue(Producer.IsValid(),
				TEXT("Producer Engine should initialize")));

			// Keep the literal indices away from the baseline boundary so the
			// consumer can install explicit, different values at both positions.
			FAngelscriptEngine::GetOrAddStaticName(
				TEXT("CacheV2ProducerOnlySentinel"));
			const FString ScriptSource = ASTEST_AS(R"AS(
				int StaticNameLiteralResult()
				{
					FName First = n"CacheV2StaticNameAlpha";
					FName Duplicate = n"CacheV2StaticNameAlpha";
					FName Other = n"CacheV2StaticNameBeta";
					return First == Duplicate && First != Other ? 454 : 0;
				}
			)AS");
			asIScriptModule* ScriptModule = Producer.BuildModule(
				"ASCacheV2StaticNameRestore", ScriptSource);
			ASSERT_THAT(IsNotNull(ScriptModule,
				TEXT("Static-name producer module should compile")));

			ProducerAlphaIndex =
				FAngelscriptEngine::GetOrAddStaticName(AlphaName);
			ProducerBetaIndex =
				FAngelscriptEngine::GetOrAddStaticName(BetaName);
			ASSERT_THAT(IsTrue(ProducerAlphaIndex >= 0));
			ASSERT_THAT(IsTrue(ProducerBetaIndex >= 0));
			ASSERT_THAT(IsTrue(ProducerAlphaIndex != ProducerBetaIndex));
			TestRunner->AddInfo(FString::Printf(
				TEXT("IC-454 producer StaticNames: AlphaIndex=%d BetaIndex=%d Count=%d"),
				ProducerAlphaIndex,
				ProducerBetaIndex,
				FAngelscriptEngine::GetStaticNameCount()));

			TSharedPtr<FAngelscriptModuleDesc> Module =
				Producer.GetEngine().GetModule(ScriptModule);
			ASSERT_THAT(IsTrue(Module.IsValid(),
				TEXT("Producer should retain the static-name module descriptor")));
			const FAngelscriptCacheCleanCaptureResult Capture =
				CaptureAngelscriptCleanCompiledModule(
					Module.ToSharedRef(), Options, Artifacts);
			TestRunner->AddInfo(FString::Printf(
				TEXT("IC-454 static-name capture: Error=%u Records=%d GraphRecords=%u Detail=%s"),
				static_cast<uint32>(Capture.Error),
				Artifacts.Records.Num(),
				Capture.ValidatedGraphRecordCount,
				*Capture.Detail));
			ASSERT_THAT(IsTrue(Capture.IsSuccess(),
				TEXT("A static-name function should be cacheable")));
		}

		FAngelscriptCachePackPolicy PackPolicy;
		PackPolicy.CompressionPolicy =
			EAngelscriptCachePackCompressionPolicy::ForceNoneForTest;
		FAngelscriptUnrealZlibCacheStorageCodec Codec;
		FAngelscriptCachePreparedColdGeneration Prepared;
		const FAngelscriptCacheCleanCaptureResult Preparation =
			PrepareAngelscriptCacheColdGeneration(
				Artifacts, Options, PackPolicy, Codec, Prepared);
		ASSERT_THAT(IsTrue(Preparation.IsSuccess(),
			TEXT("Static-name artifacts should form a cold generation")));

		FPackSource Packs(Prepared.Packs);
		FAngelscriptCacheReadLimits Limits;
		FAngelscriptCacheReadBudget Budget;
		TOptional<FAngelscriptValidatedGeneration> Validated;
		const FAngelscriptCacheValidationResult Validation =
			ValidateAngelscriptCacheGeneration(
				Prepared.EncodedManifest.CompleteBytes,
				Prepared.EncodedManifest.ComputedGenerationId,
				Packs,
				Limits,
				Budget,
				Codec,
				Validated);
		ASSERT_THAT(IsTrue(Validation.IsSuccess(),
			TEXT("Static-name generation should validate")));
		ASSERT_THAT(IsTrue(Validated.IsSet(),
			TEXT("Validation should promote the static-name generation")));

		FAngelscriptTestFixture Consumer(
			*TestRunner, ETestEngineMode::IsolatedFull);
		ASSERT_THAT(IsTrue(Consumer.IsValid(),
			TEXT("Consumer Engine should initialize")));
		AppendConsumerConflictAt(
			*TestRunner, ProducerAlphaIndex, AlphaName, TEXT("Alpha"));
		AppendConsumerConflictAt(
			*TestRunner, ProducerBetaIndex, BetaName, TEXT("Beta"));
		const int32 ConsumerNameCountBeforeRestore =
			FAngelscriptEngine::GetStaticNameCount();
		TestRunner->AddInfo(FString::Printf(
			TEXT("IC-454 consumer conflicts: AlphaIndex=%d BetaIndex=%d Count=%d"),
			ProducerAlphaIndex,
			ProducerBetaIndex,
			ConsumerNameCountBeforeRestore));

		const FAngelscriptCacheRestoreResult Restore =
			RestoreAngelscriptCacheModule(
				Consumer.GetEngine(),
				Validated.GetValue(),
				Artifacts.ModuleKey,
				Limits);
		TestRunner->AddInfo(FString::Printf(
			TEXT("IC-454 static-name restore: Error=%u Stage=%u Types=%u Functions=%u Detail=%s"),
			static_cast<uint32>(Restore.Error),
			static_cast<uint32>(Restore.Stage),
			Restore.RestoredTypeCount,
			Restore.RestoredFunctionCount,
			*Restore.Detail));
		ASSERT_THAT(IsTrue(Restore.IsSuccess(),
			TEXT("A fresh Engine should restore the static-name module")));
		ASSERT_THAT(AreEqual(uint32(0), Restore.RestoredTypeCount));
		ASSERT_THAT(AreEqual(uint32(1), Restore.RestoredFunctionCount));

		TSharedPtr<FAngelscriptModuleDesc> RestoredModule =
			Consumer.GetEngine().GetModuleByModuleName(
				TEXT("ASCacheV2StaticNameRestore"));
		ASSERT_THAT(IsTrue(RestoredModule.IsValid()));
		ASSERT_THAT(IsNotNull(RestoredModule->ScriptModule));
		asIScriptFunction* RestoredFunction =
			RestoredModule->ScriptModule->GetFunctionByDecl(
				"int StaticNameLiteralResult()");
		ASSERT_THAT(IsNotNull(RestoredFunction));

		int32 Result = 0;
		ASSERT_THAT(IsTrue(Consumer.ExecuteInt(*RestoredFunction, Result),
			TEXT("The restored static-name function should execute")));
		ASSERT_THAT(AreEqual(454, Result,
			TEXT("Canonical names must override both stale producer indices")));
		ASSERT_THAT(AreEqual(
			ConsumerNameCountBeforeRestore,
			FAngelscriptEngine::GetStaticNameCount(),
			TEXT("Canonical fallback must not copy or mutate the consumer StaticNames table")));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
