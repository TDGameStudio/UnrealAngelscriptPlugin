#include "Cache/AngelscriptCacheCleanCapture.h"
#include "Cache/AngelscriptCacheRestore.h"

#include "AngelscriptTestMacros.h"
#include "CQTest.h"
#include "Shared/AngelscriptReflectiveAccess.h"
#include "Shared/AngelscriptTestFixture.h"
#include "UObject/UnrealType.h"
#include "UObject/UObjectGlobals.h"

#if WITH_ANGELSCRIPT_UNITTESTS

namespace AngelscriptCacheClassGraphPropertyRestoreTests_Private
{
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
			TEXT("CacheV2ClassGraphPropertyRestore"),
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

	static const TSharedRef<FAngelscriptClassDesc>* FindClass(
		const FAngelscriptModuleDesc& Module,
		const FStringView ClassName)
	{
		return Module.Classes.FindByPredicate(
			[ClassName](const TSharedRef<FAngelscriptClassDesc>& Candidate)
			{
				return Candidate->ClassName == ClassName;
			});
	}
}

TEST_CLASS_WITH_FLAGS(FAngelscriptCacheClassGraphPropertyRestoreTests,
	"Angelscript.TestModule.Cache.ClassGraphPropertyRestore",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(MutuallyReferencingClassPropertiesRestoreAfterAllTypeSkeletons)
	{
		using namespace AngelscriptCacheClassGraphPropertyRestoreTests_Private;
		const FAngelscriptCacheCleanCaptureOptions Options =
			MakeCaptureOptions();
		FAngelscriptCacheCleanModuleArtifacts Artifacts;
		{
			FAngelscriptTestFixture Producer(
				*TestRunner, ETestEngineMode::IsolatedFull);
			ASSERT_THAT(IsTrue(Producer.IsValid()));
			const FString Source = ASTEST_AS(R"AS(
				UCLASS()
				class UCacheV2GraphPropertyLeft : UObject
				{
					UPROPERTY()
					UCacheV2GraphPropertyRight Right;

					UFUNCTION()
					int ReadRightValue()
					{
						return Right.Value;
					}
				}

				UCLASS()
				class UCacheV2GraphPropertyRight : UObject
				{
					UPROPERTY()
					UCacheV2GraphPropertyLeft Left;

					UPROPERTY()
					int Value = 42;
				}
			)AS");
			asIScriptModule* ScriptModule = Producer.BuildModule(
				"ASCacheV2ClassGraphPropertyRestore", Source);
			ASSERT_THAT(IsNotNull(ScriptModule));
			TSharedPtr<FAngelscriptModuleDesc> Module =
				Producer.GetEngine().GetModule(ScriptModule);
			ASSERT_THAT(IsTrue(Module.IsValid()));
			ASSERT_THAT(AreEqual(2, Module->Classes.Num()));

			const FAngelscriptCacheCleanCaptureResult Capture =
				CaptureAngelscriptCleanCompiledModule(
					Module.ToSharedRef(), Options, Artifacts);
			TestRunner->AddInfo(FString::Printf(
				TEXT("Class-graph property capture: Error=%u Records=%d Graph=%u Detail=%s"),
				static_cast<uint32>(Capture.Error), Artifacts.Records.Num(),
				Capture.ValidatedGraphRecordCount, *Capture.Detail));
			ASSERT_THAT(IsTrue(Capture.IsSuccess()));
		}

		FAngelscriptCachePackPolicy PackPolicy;
		PackPolicy.CompressionPolicy =
			EAngelscriptCachePackCompressionPolicy::ForceNoneForTest;
		FAngelscriptUnrealZlibCacheStorageCodec Codec;
		FAngelscriptCachePreparedColdGeneration Prepared;
		ASSERT_THAT(IsTrue(PrepareAngelscriptCacheColdGeneration(
			Artifacts, Options, PackPolicy, Codec, Prepared).IsSuccess()));

		FPackSource Packs(Prepared.Packs);
		FAngelscriptCacheReadLimits Limits;
		FAngelscriptCacheReadBudget Budget;
		TOptional<FAngelscriptValidatedGeneration> Validated;
		ASSERT_THAT(IsTrue(ValidateAngelscriptCacheGeneration(
			Prepared.EncodedManifest.CompleteBytes,
			Prepared.EncodedManifest.ComputedGenerationId,
			Packs, Limits, Budget, Codec, Validated).IsSuccess()));
		ASSERT_THAT(IsTrue(Validated.IsSet()));

		FAngelscriptTestFixture Consumer(
			*TestRunner, ETestEngineMode::IsolatedFull);
		ASSERT_THAT(IsTrue(Consumer.IsValid()));
		const FAngelscriptCacheRestoreResult Restore =
			RestoreAngelscriptCacheModule(
				Consumer.GetEngine(), Validated.GetValue(),
				Artifacts.ModuleKey, Limits);
		TestRunner->AddInfo(FString::Printf(
			TEXT("Class-graph property restore: Error=%u Stage=%u Types=%u Functions=%u Detail=%s"),
			static_cast<uint32>(Restore.Error),
			static_cast<uint32>(Restore.Stage),
			Restore.RestoredTypeCount, Restore.RestoredFunctionCount,
			*Restore.Detail));
		ASSERT_THAT(IsTrue(Restore.IsSuccess()));
		ASSERT_THAT(AreEqual(uint32(2), Restore.RestoredTypeCount));

		TSharedPtr<FAngelscriptModuleDesc> RestoredModule =
			Consumer.GetEngine().GetModuleByModuleName(
				TEXT("ASCacheV2ClassGraphPropertyRestore"));
		ASSERT_THAT(IsTrue(RestoredModule.IsValid()));
		const TSharedRef<FAngelscriptClassDesc>* Left = FindClass(
			*RestoredModule, TEXT("UCacheV2GraphPropertyLeft"));
		const TSharedRef<FAngelscriptClassDesc>* Right = FindClass(
			*RestoredModule, TEXT("UCacheV2GraphPropertyRight"));
		ASSERT_THAT(IsNotNull(Left));
		ASSERT_THAT(IsNotNull(Right));
		ASSERT_THAT(IsNotNull((*Left)->Class));
		ASSERT_THAT(IsNotNull((*Right)->Class));

		FObjectProperty* RightProperty = CastField<FObjectProperty>(
			(*Left)->Class->FindPropertyByName(TEXT("Right")));
		FObjectProperty* LeftProperty = CastField<FObjectProperty>(
			(*Right)->Class->FindPropertyByName(TEXT("Left")));
		ASSERT_THAT(IsNotNull(RightProperty));
		ASSERT_THAT(IsNotNull(LeftProperty));
		ASSERT_THAT(AreEqual(
			static_cast<UClass*>((*Right)->Class),
			RightProperty->PropertyClass));
		ASSERT_THAT(AreEqual(
			static_cast<UClass*>((*Left)->Class),
			LeftProperty->PropertyClass));

		UObject* LeftObject = NewObject<UObject>(
			GetTransientPackage(), (*Left)->Class);
		UObject* RightObject = NewObject<UObject>(
			GetTransientPackage(), (*Right)->Class);
		ASSERT_THAT(IsNotNull(LeftObject));
		ASSERT_THAT(IsNotNull(RightObject));
		RightProperty->SetObjectPropertyValue_InContainer(
			LeftObject, RightObject);
		LeftProperty->SetObjectPropertyValue_InContainer(
			RightObject, LeftObject);

		FFunctionInvoker Invoker(
			*TestRunner, LeftObject, FName(TEXT("ReadRightValue")));
		ASSERT_THAT(IsTrue(Invoker.IsValid()));
		ASSERT_THAT(AreEqual(42,
			Invoker.CallAndReturn<int32>(INDEX_NONE)));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
