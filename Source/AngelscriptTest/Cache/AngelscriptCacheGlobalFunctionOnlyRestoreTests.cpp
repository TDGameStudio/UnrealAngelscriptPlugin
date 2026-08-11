#include "Cache/AngelscriptCacheCleanCapture.h"
#include "Cache/AngelscriptCacheRestore.h"

#include "AngelscriptTestMacros.h"
#include "CQTest.h"
#include "Shared/AngelscriptTestFixture.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FAngelscriptCacheGlobalFunctionOnlyRestoreTests,
	"Angelscript.TestModule.Cache.GlobalFunctionOnlyRestore",
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
			TEXT("CacheV2GlobalFunctionOnlyRestore"),
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

public:
	TEST_METHOD(MultipleGlobalFunctionsRoundTripWithoutSyntheticType)
	{
		const FAngelscriptCacheCleanCaptureOptions Options =
			MakeCaptureOptions();
		FAngelscriptCacheCleanModuleArtifacts Artifacts;
		{
			FAngelscriptTestFixture Producer(
				*TestRunner, ETestEngineMode::IsolatedFull);
			ASSERT_THAT(IsTrue(Producer.IsValid(),
				TEXT("Producer Engine should initialize")));

			const FString ScriptSource = ASTEST_AS(R"AS(
				int FirstFixtureValue()
				{
					return 41;
				}

				int SecondFixtureValue()
				{
					return 42;
				}
			)AS");
			asIScriptModule* ScriptModule = Producer.BuildModule(
				"ASCacheV2GlobalFunctionOnlyRestore", ScriptSource);
			ASSERT_THAT(IsNotNull(ScriptModule,
				TEXT("Global-function-only producer module should compile")));
			TSharedPtr<FAngelscriptModuleDesc> Module =
				Producer.GetEngine().GetModule(ScriptModule);
			ASSERT_THAT(IsTrue(Module.IsValid(),
				TEXT("Producer should retain the compiled module descriptor")));

			const FAngelscriptCacheCleanCaptureResult Capture =
				CaptureAngelscriptCleanCompiledModule(
					Module.ToSharedRef(), Options, Artifacts);
			TestRunner->AddInfo(FString::Printf(
				TEXT("IC-434 global-only capture: Error=%u Records=%d GraphRecords=%u Detail=%s"),
				static_cast<uint32>(Capture.Error),
				Artifacts.Records.Num(),
				Capture.ValidatedGraphRecordCount,
				*Capture.Detail));
			ASSERT_THAT(IsTrue(Capture.IsSuccess(),
				TEXT("A complete global-function-only module should be cacheable")));
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
			TEXT("Global-function-only artifacts should form a generation")));

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
			TEXT("Global-function-only generation should validate")));
		ASSERT_THAT(IsTrue(Validated.IsSet(),
			TEXT("Validation should promote the generation")));

		FAngelscriptTestFixture Consumer(
			*TestRunner, ETestEngineMode::IsolatedFull);
		ASSERT_THAT(IsTrue(Consumer.IsValid(),
			TEXT("Consumer Engine should initialize")));
		const FAngelscriptCacheRestoreResult Restore =
			RestoreAngelscriptCacheModule(
				Consumer.GetEngine(),
				Validated.GetValue(),
				Artifacts.ModuleKey,
				Limits);
		TestRunner->AddInfo(FString::Printf(
			TEXT("IC-434 global-only restore: Error=%u Stage=%u Types=%u Functions=%u Detail=%s"),
			static_cast<uint32>(Restore.Error),
			static_cast<uint32>(Restore.Stage),
			Restore.RestoredTypeCount,
			Restore.RestoredFunctionCount,
			*Restore.Detail));
		ASSERT_THAT(IsTrue(Restore.IsSuccess(),
			TEXT("A fresh Engine should restore the complete global-only module")));
		ASSERT_THAT(AreEqual(uint32(0), Restore.RestoredTypeCount));
		ASSERT_THAT(AreEqual(uint32(2), Restore.RestoredFunctionCount));

		TSharedPtr<FAngelscriptModuleDesc> RestoredModule =
			Consumer.GetEngine().GetModuleByModuleName(
				TEXT("ASCacheV2GlobalFunctionOnlyRestore"));
		ASSERT_THAT(IsTrue(RestoredModule.IsValid()));
		ASSERT_THAT(IsNotNull(RestoredModule->ScriptModule));
		ASSERT_THAT(AreEqual(0, RestoredModule->Enums.Num()));
		ASSERT_THAT(AreEqual(0, RestoredModule->Classes.Num()));

		asIScriptFunction* First =
			RestoredModule->ScriptModule->GetFunctionByDecl(
				"int FirstFixtureValue()");
		asIScriptFunction* Second =
			RestoredModule->ScriptModule->GetFunctionByDecl(
				"int SecondFixtureValue()");
		ASSERT_THAT(IsNotNull(First));
		ASSERT_THAT(IsNotNull(Second));
		int32 FirstValue = 0;
		int32 SecondValue = 0;
		ASSERT_THAT(IsTrue(Consumer.ExecuteInt(*First, FirstValue)));
		ASSERT_THAT(IsTrue(Consumer.ExecuteInt(*Second, SecondValue)));
		ASSERT_THAT(AreEqual(41, FirstValue));
		ASSERT_THAT(AreEqual(42, SecondValue));

		ASSERT_THAT(AreEqual(
			2, Artifacts.ValidatedFunctionArtifactIdentities.Num()));
		FAngelscriptCacheLiveFunctionRoute FirstRoute;
		FAngelscriptCacheLiveFunctionRoute SecondRoute;
		ASSERT_THAT(IsTrue(Consumer.GetEngine().ResolveCacheFunctionRoute(
			Artifacts.ValidatedFunctionArtifactIdentities[0].FunctionKey,
			FirstRoute)));
		ASSERT_THAT(IsTrue(Consumer.GetEngine().ResolveCacheFunctionRoute(
			Artifacts.ValidatedFunctionArtifactIdentities[1].FunctionKey,
			SecondRoute)));
		ASSERT_THAT(IsFalse(
			FirstRoute.Identity.FunctionKey.Hash
				== SecondRoute.Identity.FunctionKey.Hash,
			TEXT("The two declarations must retain distinct stable routes")));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
