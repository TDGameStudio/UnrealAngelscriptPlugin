#include "Cache/AngelscriptCacheCleanCapture.h"
#include "Cache/AngelscriptCacheRestore.h"

#include "AngelscriptTestMacros.h"
#include "CQTest.h"
#include "Shared/AngelscriptReflectiveAccess.h"
#include "Shared/AngelscriptTestFixture.h"
#include "UObject/UObjectGlobals.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FAngelscriptCacheRootClassRestoreTests,
	"Angelscript.TestModule.Cache.RootClassRestore",
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
			TEXT("CacheV2RootClassRestore"),
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
	TEST_METHOD(ReflectedRootClassRoundTripsIntoFreshEngine)
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
				UCLASS()
				class UCacheV2RestoredRootObject : UObject
				{
					UPROPERTY()
					int Value = 37;

					int ScriptOnlyAdd(int Delta)
					{
						return Value + Delta;
					}

					UFUNCTION()
					int ReflectedValue()
					{
						return ScriptOnlyAdd(5);
					}
				}
			)AS");
			asIScriptModule* ScriptModule = Producer.BuildModule(
				"ASCacheV2RootClassRestore", ScriptSource);
			ASSERT_THAT(IsNotNull(ScriptModule,
				TEXT("Root-class producer module should compile")));
			TSharedPtr<FAngelscriptModuleDesc> Module =
				Producer.GetEngine().GetModule(ScriptModule);
			ASSERT_THAT(IsTrue(Module.IsValid(),
				TEXT("Producer should retain the root-class descriptor")));
			ASSERT_THAT(AreEqual(1, Module->Classes.Num()));
			ASSERT_THAT(AreEqual(1, Module->Classes[0]->Properties.Num()));
			ASSERT_THAT(AreEqual(1, Module->Classes[0]->Methods.Num()));

			const FAngelscriptCacheCleanCaptureResult Capture =
				CaptureAngelscriptCleanCompiledModule(
					Module.ToSharedRef(), Options, Artifacts);
			TestRunner->AddInfo(FString::Printf(
				TEXT("IC-434 root-class capture: Error=%u Records=%d GraphRecords=%u Detail=%s"),
				static_cast<uint32>(Capture.Error),
				Artifacts.Records.Num(),
				Capture.ValidatedGraphRecordCount,
				*Capture.Detail));
			ASSERT_THAT(IsTrue(Capture.IsSuccess(),
				TEXT("A complete reflected root class should be cacheable")));
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
			TEXT("Root-class artifacts should form a generation")));

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
			TEXT("Root-class generation should validate")));
		ASSERT_THAT(IsTrue(Validated.IsSet(),
			TEXT("Validation should promote the root-class generation")));

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
			TEXT("IC-434 root-class restore: Error=%u Stage=%u Types=%u Functions=%u Detail=%s"),
			static_cast<uint32>(Restore.Error),
			static_cast<uint32>(Restore.Stage),
			Restore.RestoredTypeCount,
			Restore.RestoredFunctionCount,
			*Restore.Detail));
		ASSERT_THAT(IsTrue(Restore.IsSuccess(),
			TEXT("A fresh Engine should restore the complete root class")));
		ASSERT_THAT(AreEqual(uint32(1), Restore.RestoredTypeCount));
		ASSERT_THAT(AreEqual(
			static_cast<uint32>(
				Artifacts.ValidatedFunctionArtifactIdentities.Num()),
			Restore.RestoredFunctionCount));

		TSharedPtr<FAngelscriptModuleDesc> RestoredModule =
			Consumer.GetEngine().GetModuleByModuleName(
				TEXT("ASCacheV2RootClassRestore"));
		ASSERT_THAT(IsTrue(RestoredModule.IsValid()));
		ASSERT_THAT(IsNotNull(RestoredModule->ScriptModule));
		ASSERT_THAT(AreEqual(1, RestoredModule->Classes.Num()));
		const TSharedRef<FAngelscriptClassDesc>& RestoredClass =
			RestoredModule->Classes[0];
		ASSERT_THAT(AreEqual(
			FString(TEXT("UCacheV2RestoredRootObject")),
			RestoredClass->ClassName));
		ASSERT_THAT(IsNotNull(RestoredClass->ScriptType));
		ASSERT_THAT(IsNotNull(RestoredClass->Class));
		ASSERT_THAT(AreEqual(1, RestoredClass->Properties.Num()));
		ASSERT_THAT(AreEqual(1, RestoredClass->Methods.Num()));

		UFunction* ReflectedValue = RestoredClass->Class->FindFunctionByName(
			TEXT("ReflectedValue"));
		ASSERT_THAT(IsNotNull(ReflectedValue,
			TEXT("ClassGenerator should recreate the cached UFUNCTION")));
		UObject* Instance = NewObject<UObject>(
			GetTransientPackage(), RestoredClass->Class);
		ASSERT_THAT(IsNotNull(Instance,
			TEXT("The restored UClass should create an instance")));
		FFunctionInvoker Invoker(
			*TestRunner, Instance, FName(TEXT("ReflectedValue")));
		ASSERT_THAT(IsTrue(Invoker.IsValid(),
			TEXT("The restored UFUNCTION should be invokable")));
		ASSERT_THAT(AreEqual(42,
			Invoker.CallAndReturn<int32>(INDEX_NONE),
			TEXT("Restored property defaults and method dependencies should execute")));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
