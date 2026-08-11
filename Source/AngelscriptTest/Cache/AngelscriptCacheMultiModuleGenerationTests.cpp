#include "Cache/AngelscriptCacheCleanCapture.h"
#include "Cache/AngelscriptCacheService.h"

#include "CQTest.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "Shared/AngelscriptTestEngineAcquisition.h"

#if WITH_ANGELSCRIPT_UNITTESTS

namespace AngelscriptCacheMultiModuleGenerationTests_Private
{
	class FScopedProjectRoot final
	{
	public:
		FScopedProjectRoot()
		{
			Root = FPaths::ConvertRelativePathToFull(FPaths::Combine(
				FPaths::ProjectSavedDir(),
				TEXT("Automation/AngelscriptCacheMultiModuleGeneration"),
				FGuid::NewGuid().ToString(EGuidFormats::Digits)));
			FPaths::NormalizeDirectoryName(Root);
			check(Root.Contains(
				TEXT("/Saved/Automation/AngelscriptCacheMultiModuleGeneration/")));
			ScriptRoot = Root / TEXT("Script");
			check(IFileManager::Get().MakeDirectory(*ScriptRoot, true));
		}

		~FScopedProjectRoot()
		{
			if (Root.Contains(
				TEXT("/Saved/Automation/AngelscriptCacheMultiModuleGeneration/")))
			{
				IFileManager::Get().DeleteDirectory(*Root, false, true);
			}
		}

		bool WriteSource(
			const FString& RelativePath,
			const FString& Source) const
		{
			return FFileHelper::SaveStringToFile(
				Source, *(ScriptRoot / RelativePath));
		}

		FString Root;
		FString ScriptRoot;
	};

	static TUniquePtr<FAngelscriptEngine> CreateEngine(
		const FString& ProjectRoot)
	{
		FAngelscriptEngineConfig Config;
		Config.bIsEditor = true;
		Config.bDevelopmentMode = true;
		Config.bSkipThreadedInitialize = true;

		FAngelscriptEngineDependencies Dependencies =
			FAngelscriptEngineDependencies::CreateDefault();
		Dependencies.GetProjectDir = [ProjectRoot]()
		{
			return ProjectRoot;
		};
		Dependencies.GetEnabledPluginScriptRoots = []()
		{
			return TArray<FString>();
		};
		Dependencies.GetEnabledPluginScriptRootDescriptors = []()
		{
			return TArray<FAngelscriptPluginScriptRoot>();
		};
		return CreateScriptScanFreeFullEngineForTesting(Config, Dependencies);
	}

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
}

TEST_CLASS_WITH_FLAGS(FAngelscriptCacheMultiModuleGenerationTests,
	"Angelscript.TestModule.Cache.MultiModuleGeneration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(ProductionPublicationBuildsOneDeduplicatedCompleteGeneration)
	{
		using namespace AngelscriptCacheMultiModuleGenerationTests_Private;
		FScopedProjectRoot Project;
		const FString Unique = FGuid::NewGuid().ToString(EGuidFormats::Digits);
		const FString First = FString::Printf(TEXT(R"AS(
enum ECacheMultiFirst%sState
{
	Ready = 1,
}

int ReadCacheMultiFirst%sValue()
{
	return 101;
}
)AS"), *Unique, *Unique);
		const FString Second = FString::Printf(TEXT(R"AS(
enum ECacheMultiSecond%sState
{
	Ready = 1,
}

int ReadCacheMultiSecond%sValue()
{
	return 102;
}
)AS"), *Unique, *Unique);
		ASSERT_THAT(IsTrue(Project.WriteSource(TEXT("First.as"), First)));
		ASSERT_THAT(IsTrue(Project.WriteSource(TEXT("Second.as"), Second)));

		TUniquePtr<FAngelscriptEngine> Engine = CreateEngine(Project.Root);
		ASSERT_THAT(IsNotNull(Engine.Get()));
		FAngelscriptEngineScope EngineScope(*Engine);
		FAngelscriptCacheService* Service = Engine->GetCacheService();
		ASSERT_THAT(IsNotNull(Service));
		Service->ConfigureDecisionTrace(true, 64);
		Engine->InitialCompile();
		const FAngelscriptCacheLifecyclePublications Publications =
			Service->GetLifecyclePublications();
		ASSERT_THAT(IsTrue(Publications.Current.IsValid()));
		ASSERT_THAT(AreEqual(2, Publications.Current->Modules.Num()));

		FAngelscriptCacheCleanCaptureOptions Options;
		Options.Compatibility = Publications.Current->Compatibility;
		Options.Context = Publications.Current->Context;
		Options.Profile = Publications.Current->Profile;
		FAngelscriptCachePackPolicy PackPolicy;
		PackPolicy.CompressionPolicy =
			EAngelscriptCachePackCompressionPolicy::Auto;
		FAngelscriptUnrealZlibCacheStorageCodec Codec;
		FAngelscriptCachePreparedColdGeneration Prepared;
		const FAngelscriptCacheCleanCaptureResult Preparation =
			PrepareAngelscriptCacheColdGeneration(
				Publications.Current->Modules,
				Options,
				PackPolicy,
				Codec,
				Prepared);
		TestRunner->AddInfo(FString::Printf(
			TEXT("V6.3 multi-module generation: Error=%u Modules=%d Records=%d Packs=%d Generation=%s Detail=%s"),
			static_cast<uint32>(Preparation.Error),
			Prepared.Manifest.ModuleSnapshots.Num(),
			Prepared.Manifest.Records.Num(), Prepared.Packs.Num(),
			*Prepared.EncodedManifest.ComputedGenerationId.ToHexString(),
			*Preparation.Detail));
		ASSERT_THAT(IsTrue(Preparation.IsSuccess()));
		ASSERT_THAT(AreEqual(2, Prepared.Manifest.ModuleSnapshots.Num()));
		ASSERT_THAT(AreEqual(13, Prepared.Manifest.Records.Num()));
		ASSERT_THAT(IsTrue(
			Prepared.Manifest.SourceSnapshot
				== Publications.Current->SourceSnapshot));

		int32 SourceIndexCount = 0;
		for (const FAngelscriptCacheRecordIndexEntry& Entry
			: Prepared.Manifest.Records)
		{
			SourceIndexCount += Entry.RecordId.Kind
				== EAngelscriptCacheRecordKind::SourceIndex;
		}
		ASSERT_THAT(AreEqual(1, SourceIndexCount));

		FPackSource Packs(Prepared.Packs);
		FAngelscriptCacheReadLimits Limits;
		FAngelscriptCacheReadBudget Budget;
		TOptional<FAngelscriptValidatedGeneration> Validated;
		const FAngelscriptCacheValidationResult Validation =
			ValidateAngelscriptCacheGeneration(
				Prepared.EncodedManifest.CompleteBytes,
				Prepared.EncodedManifest.ComputedGenerationId,
				Packs, Limits, Budget, Codec, Validated);
		ASSERT_THAT(IsTrue(Validation.IsSuccess()));
		ASSERT_THAT(IsTrue(Validated.IsSet()));
		ASSERT_THAT(AreEqual(2,
			Validated->Manifest.ModuleSnapshots.Num()));
		ASSERT_THAT(AreEqual(13,
			Validated->ReachableRecords.Num()));
	}

	TEST_METHOD(UnsupportedModuleDoesNotDiscardEligibleModulePublication)
	{
		using namespace AngelscriptCacheMultiModuleGenerationTests_Private;
		FScopedProjectRoot Project;
		const FString Unique = FGuid::NewGuid().ToString(EGuidFormats::Digits);
		const FString First = FString::Printf(TEXT(R"AS(
enum ECachePartialFirst%sState
{
	Ready = 1,
}

int ReadCachePartialFirst%sValue()
{
	return 201;
}
)AS"), *Unique, *Unique);
		const FString Unsupported = FString::Printf(TEXT(R"AS(
enum ECachePartialUnsupported%sFirst
{
	Ready = 1,
}

enum ECachePartialUnsupported%sSecond
{
	Ready = 2,
}

int ReadCachePartialUnsupported%sValue()
{
	return 202;
}
)AS"), *Unique, *Unique, *Unique);
		ASSERT_THAT(IsTrue(Project.WriteSource(
			TEXT("First.as"), First)));
		ASSERT_THAT(IsTrue(Project.WriteSource(
			TEXT("Unsupported.as"), Unsupported)));

		TUniquePtr<FAngelscriptEngine> Engine = CreateEngine(Project.Root);
		ASSERT_THAT(IsNotNull(Engine.Get()));
		FAngelscriptEngineScope EngineScope(*Engine);
		FAngelscriptCacheService* Service = Engine->GetCacheService();
		ASSERT_THAT(IsNotNull(Service));
		Service->ConfigureDecisionTrace(true, 64);
		Engine->InitialCompile();

		const TSharedPtr<FAngelscriptModuleDesc> FirstModule =
			Engine->GetModuleByModuleName(TEXT("First"));
		const TSharedPtr<FAngelscriptModuleDesc> UnsupportedModule =
			Engine->GetModuleByModuleName(TEXT("Unsupported"));
		ASSERT_THAT(IsTrue(FirstModule.IsValid()));
		ASSERT_THAT(IsTrue(UnsupportedModule.IsValid()));

		const FAngelscriptCacheLifecyclePublications Publications =
			Service->GetLifecyclePublications();
		TestRunner->AddInfo(FString::Printf(
			TEXT("Cache V2 partial capture: Current=%d Modules=%d FirstActive=%d UnsupportedActive=%d"),
			Publications.Current.IsValid() ? 1 : 0,
			Publications.Current.IsValid()
				? Publications.Current->Modules.Num() : 0,
			FirstModule.IsValid() ? 1 : 0,
			UnsupportedModule.IsValid() ? 1 : 0));
		ASSERT_THAT(IsTrue(Publications.Current.IsValid()));
		ASSERT_THAT(AreEqual(1, Publications.Current->Modules.Num()));
		ASSERT_THAT(AreEqual(
			FString(TEXT("First")),
			Publications.Current->Modules[0].CanonicalModuleName));

		const FAngelscriptCacheDecisionTraceSnapshot Trace =
			Service->CaptureDecisionTrace();
		const FAngelscriptCacheDecisionEvent* SkipEvent = Trace.Events.FindByPredicate(
			[](const FAngelscriptCacheDecisionEvent& Event)
			{
				return Event.Outcome
						== EAngelscriptCacheDecisionOutcome::NotCacheable
					&& Event.ReasonDomain
						== EAngelscriptCacheDecisionReasonDomain::CleanCapture
					&& Event.ReasonCode == static_cast<uint32>(
						EAngelscriptCacheCleanCaptureError::NotCacheable);
			});
		ASSERT_THAT(IsNotNull(SkipEvent));
		ASSERT_THAT(AreEqual(1, SkipEvent->ModuleKeys.Num()));
		ASSERT_THAT(IsTrue(SkipEvent->Profile.Hash
			== Publications.Current->Profile.Hash));
		ASSERT_THAT(IsTrue(SkipEvent->SourceSnapshot
			== Publications.Current->SourceSnapshot));
	}
};

#endif
