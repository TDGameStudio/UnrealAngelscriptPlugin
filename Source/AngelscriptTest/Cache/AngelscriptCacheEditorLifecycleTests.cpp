#include "Cache/AngelscriptCacheService.h"

#include "CQTest.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "Shared/AngelscriptTestEngineAcquisition.h"

#if WITH_ANGELSCRIPT_UNITTESTS

namespace AngelscriptCacheEditorLifecycleTests_Private
{
	class FScopedProjectRoot final
	{
	public:
		FScopedProjectRoot()
		{
			Root = FPaths::ConvertRelativePathToFull(FPaths::Combine(
				FPaths::ProjectSavedDir(),
				TEXT("Automation/AngelscriptCacheEditorLifecycle"),
				FGuid::NewGuid().ToString(EGuidFormats::Digits)));
			FPaths::NormalizeDirectoryName(Root);
			check(Root.Contains(
				TEXT("/Saved/Automation/AngelscriptCacheEditorLifecycle/")));
			ScriptRoot = Root / TEXT("Script");
			check(IFileManager::Get().MakeDirectory(*ScriptRoot, true));
		}

		~FScopedProjectRoot()
		{
			if (Root.Contains(
				TEXT("/Saved/Automation/AngelscriptCacheEditorLifecycle/")))
			{
				IFileManager::Get().DeleteDirectory(*Root, false, true);
			}
		}

		bool WriteSource(const FString& RelativePath, const FString& Source) const
		{
			return FFileHelper::SaveStringToFile(
				Source, *(ScriptRoot / RelativePath));
		}

		FString Root;
		FString ScriptRoot;
	};

	static TUniquePtr<FAngelscriptEngine> CreateScanFreeEditorEngine(
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

	static void QueueModifiedSource(
		FAngelscriptEngine& Engine,
		const FScopedProjectRoot& Project,
		const FString& RelativePath)
	{
		Engine.FileChangesDetectedForReload.AddUnique({
			Project.ScriptRoot / RelativePath,
			RelativePath,
			FString(TEXT("/Angelscript/Game/")) + RelativePath,
		});
	}
}

TEST_CLASS_WITH_FLAGS(FAngelscriptCacheEditorLifecycleTests,
	"Angelscript.TestModule.Cache.EditorLifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(InitialCompileAutomaticallyPublishesCurrentGeneration)
	{
		using namespace AngelscriptCacheEditorLifecycleTests_Private;
		FScopedProjectRoot Project;
		const FString Unique = FGuid::NewGuid().ToString(EGuidFormats::Digits);
		const FString Source = FString::Printf(TEXT(R"AS(
enum ECacheInitial%sState
{
	Ready = 1,
}

int ReadCacheInitial%sValue()
{
	return 71;
}
)AS"), *Unique, *Unique);
		ASSERT_THAT(IsTrue(Project.WriteSource(TEXT("Initial.as"), Source)));

		TUniquePtr<FAngelscriptEngine> Engine =
			CreateScanFreeEditorEngine(Project.Root);
		ASSERT_THAT(IsNotNull(Engine.Get()));
		FAngelscriptEngineScope EngineScope(*Engine);
		Engine->InitialCompile();

		FAngelscriptCacheService* Service = Engine->GetCacheService();
		ASSERT_THAT(IsNotNull(Service));
		const FAngelscriptCacheLifecyclePublications Publications =
			Service->GetLifecyclePublications();
		TestRunner->AddInfo(FString::Printf(
			TEXT("V6.2 production initial publication: Current=%d Pending=%d Latest=%d Tx=%llu Modules=%d"),
			Publications.Current.IsValid() ? 1 : 0,
			Publications.PendingColdStart.IsValid() ? 1 : 0,
			Publications.LatestSuccessful.IsValid() ? 1 : 0,
			Publications.Current.IsValid()
				? Publications.Current->TransactionOrdinal : 0,
			Publications.Current.IsValid()
				? Publications.Current->Modules.Num() : 0));
		ASSERT_THAT(IsTrue(Publications.Current.IsValid()));
		ASSERT_THAT(IsFalse(Publications.PendingColdStart.IsValid()));
		ASSERT_THAT(IsTrue(
			Publications.LatestSuccessful == Publications.Current));
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheSuccessfulCompileKind::Initial,
			Publications.Current->Kind));
		ASSERT_THAT(AreEqual(
			EAngelscriptCachePublicationDisposition::Current,
			Publications.Current->Disposition));
		ASSERT_THAT(AreEqual(1, Publications.Current->Modules.Num()));
	}

	TEST_METHOD(FailedHotReloadPreservesLastGoodCurrentAndActiveModule)
	{
		using namespace AngelscriptCacheEditorLifecycleTests_Private;
		FScopedProjectRoot Project;
		const FString Unique = FGuid::NewGuid().ToString(EGuidFormats::Digits);
		const FString ValidSource = FString::Printf(TEXT(R"AS(
enum ECacheFailure%sState
{
	Ready = 1,
}

int ReadCacheFailure%sValue()
{
	return 81;
}
)AS"), *Unique, *Unique);
		ASSERT_THAT(IsTrue(Project.WriteSource(TEXT("Failure.as"), ValidSource)));

		TUniquePtr<FAngelscriptEngine> Engine =
			CreateScanFreeEditorEngine(Project.Root);
		ASSERT_THAT(IsNotNull(Engine.Get()));
		FAngelscriptEngineScope EngineScope(*Engine);
		Engine->InitialCompile();
		FAngelscriptCacheService* Service = Engine->GetCacheService();
		ASSERT_THAT(IsNotNull(Service));
		const FAngelscriptCacheLifecyclePublications Before =
			Service->GetLifecyclePublications();
		ASSERT_THAT(IsTrue(Before.Current.IsValid()));
		const TSharedPtr<FAngelscriptModuleDesc> ActiveBefore =
			Engine->GetModuleByModuleName(TEXT("Failure"));
		ASSERT_THAT(IsTrue(ActiveBefore.IsValid()));
		asIScriptModule* const ScriptModuleBefore = ActiveBefore->ScriptModule;

		const FString InvalidSource = FString::Printf(TEXT(R"AS(
enum ECacheFailure%sState
{
	Ready = 1,
}

int ReadCacheFailure%sValue(
)AS"), *Unique, *Unique);
		ASSERT_THAT(IsTrue(Project.WriteSource(TEXT("Failure.as"), InvalidSource)));
		TestRunner->AddExpectedError(
			TEXT("Expected data type"),
			EAutomationExpectedErrorFlags::Contains, 1);
		TestRunner->AddExpectedError(
			TEXT("Instead found '<end of file>'"),
			EAutomationExpectedErrorFlags::Contains, 1);
		TestRunner->AddExpectedError(
			TEXT("Hot reload failed due to script compile errors"),
			EAutomationExpectedErrorFlags::Contains, 1);
		TestRunner->AddExpectedError(
			TEXT("Failure.as:"),
			EAutomationExpectedErrorFlags::Contains, 1);
		QueueModifiedSource(*Engine, Project, TEXT("Failure.as"));
		Engine->CheckForHotReload(ECompileType::FullReload);

		const FAngelscriptCacheLifecyclePublications After =
			Service->GetLifecyclePublications();
		const TSharedPtr<FAngelscriptModuleDesc> ActiveAfter =
			Engine->GetModuleByModuleName(TEXT("Failure"));
		TestRunner->AddInfo(FString::Printf(
			TEXT("V6.2 failed reload preservation: BeforeTx=%llu AfterTx=%llu SameCurrent=%d Pending=%d SameScriptModule=%d"),
			Before.Current->TransactionOrdinal,
			After.Current.IsValid()
				? After.Current->TransactionOrdinal : 0,
			After.Current == Before.Current ? 1 : 0,
			After.PendingColdStart.IsValid() ? 1 : 0,
			ActiveAfter.IsValid()
				&& ActiveAfter->ScriptModule == ScriptModuleBefore ? 1 : 0));
		ASSERT_THAT(IsTrue(After.Current == Before.Current));
		ASSERT_THAT(IsTrue(
			After.LatestSuccessful == Before.LatestSuccessful));
		ASSERT_THAT(IsFalse(After.PendingColdStart.IsValid()));
		ASSERT_THAT(IsTrue(ActiveAfter.IsValid()));
		ASSERT_THAT(IsTrue(ActiveAfter->ScriptModule == ScriptModuleBefore));
	}

	TEST_METHOD(PieStructuralChangePublishesPendingThenFullReloadPromotes)
	{
		using namespace AngelscriptCacheEditorLifecycleTests_Private;
		FScopedProjectRoot Project;
		const FString Unique = FGuid::NewGuid().ToString(EGuidFormats::Digits);
		const FString ClassName = FString::Printf(
			TEXT("UCacheLifecycle%sTarget"), *Unique);
		const FString InitialSource = FString::Printf(TEXT(R"AS(
UCLASS()
class %s : UObject
{
	UPROPERTY()
	int Value;
}
)AS"), *ClassName);
		const FString StructuralSource = FString::Printf(TEXT(R"AS(
UCLASS()
class %s : UObject
{
	UPROPERTY()
	int Value;

	UPROPERTY()
	int ExtraValue;
}
)AS"), *ClassName);
		ASSERT_THAT(IsTrue(Project.WriteSource(
			TEXT("Structural.as"), InitialSource)));

		TUniquePtr<FAngelscriptEngine> Engine =
			CreateScanFreeEditorEngine(Project.Root);
		ASSERT_THAT(IsNotNull(Engine.Get()));
		FAngelscriptEngineScope EngineScope(*Engine);
		Engine->InitialCompile();
		FAngelscriptCacheService* Service = Engine->GetCacheService();
		ASSERT_THAT(IsNotNull(Service));
		const FAngelscriptCacheLifecyclePublications Initial =
			Service->GetLifecyclePublications();
		ASSERT_THAT(IsTrue(Initial.Current.IsValid()));
		ASSERT_THAT(IsFalse(Initial.PendingColdStart.IsValid()));

		ASSERT_THAT(IsTrue(Project.WriteSource(
			TEXT("Structural.as"), StructuralSource)));
		QueueModifiedSource(*Engine, Project, TEXT("Structural.as"));
		Engine->CheckForHotReload(ECompileType::SoftReloadOnly);

		const FAngelscriptCacheLifecyclePublications DuringPie =
			Service->GetLifecyclePublications();
		TestRunner->AddInfo(FString::Printf(
			TEXT("V6.2 PIE structural deferral: CurrentTx=%llu PendingTx=%llu LatestTx=%llu CurrentModules=%d PendingModules=%d"),
			DuringPie.Current.IsValid()
				? DuringPie.Current->TransactionOrdinal : 0,
			DuringPie.PendingColdStart.IsValid()
				? DuringPie.PendingColdStart->TransactionOrdinal : 0,
			DuringPie.LatestSuccessful.IsValid()
				? DuringPie.LatestSuccessful->TransactionOrdinal : 0,
			DuringPie.Current.IsValid()
				? DuringPie.Current->Modules.Num() : 0,
			DuringPie.PendingColdStart.IsValid()
				? DuringPie.PendingColdStart->Modules.Num() : 0));
		ASSERT_THAT(IsTrue(DuringPie.Current == Initial.Current));
		ASSERT_THAT(IsTrue(DuringPie.PendingColdStart.IsValid()));
		ASSERT_THAT(IsTrue(
			DuringPie.LatestSuccessful == DuringPie.PendingColdStart));
		ASSERT_THAT(AreEqual(
			EAngelscriptCachePublicationDisposition::PendingColdStart,
			DuringPie.PendingColdStart->Disposition));
		ASSERT_THAT(IsFalse(
			DuringPie.PendingColdStart->SourceSnapshot
				== Initial.Current->SourceSnapshot));

		Engine->CheckForHotReload(ECompileType::FullReload);
		const FAngelscriptCacheLifecyclePublications Promoted =
			Service->GetLifecyclePublications();
		TestRunner->AddInfo(FString::Printf(
			TEXT("V6.2 structural promotion: CurrentTx=%llu Pending=%d LatestTx=%llu"),
			Promoted.Current.IsValid()
				? Promoted.Current->TransactionOrdinal : 0,
			Promoted.PendingColdStart.IsValid() ? 1 : 0,
			Promoted.LatestSuccessful.IsValid()
				? Promoted.LatestSuccessful->TransactionOrdinal : 0));
		ASSERT_THAT(IsTrue(Promoted.Current.IsValid()));
		ASSERT_THAT(IsFalse(Promoted.PendingColdStart.IsValid()));
		ASSERT_THAT(IsTrue(Promoted.LatestSuccessful == Promoted.Current));
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheSuccessfulCompileKind::FullReload,
			Promoted.Current->Kind));
		ASSERT_THAT(IsTrue(
			Promoted.Current->SourceSnapshot
				== DuringPie.PendingColdStart->SourceSnapshot));
		ASSERT_THAT(IsTrue(
			Promoted.Current->TransactionOrdinal
				> DuringPie.PendingColdStart->TransactionOrdinal));
	}
};

#endif
