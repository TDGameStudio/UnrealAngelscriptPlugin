#include "Cache/AngelscriptRuntimeReload.h"
#include "Cache/AngelscriptCacheService.h"

#include "AngelscriptEngine.h"
#include "CQTest.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "Shared/AngelscriptTestEngineAcquisition.h"

#include "angelscript.h"

#if WITH_ANGELSCRIPT_UNITTESTS

namespace AngelscriptCachePackagedRuntimeReloadTests_Private
{
	class FScopedProjectRoot final
	{
	public:
		FScopedProjectRoot()
		{
			Root = FPaths::ConvertRelativePathToFull(FPaths::Combine(
				FPaths::ProjectSavedDir(),
				TEXT("Automation/AngelscriptCachePackagedRuntimeReload"),
				FGuid::NewGuid().ToString(EGuidFormats::Digits)));
			FPaths::NormalizeDirectoryName(Root);
			check(Root.Contains(TEXT(
				"/Saved/Automation/AngelscriptCachePackagedRuntimeReload/")));
			ScriptRoot = Root / TEXT("Script");
			check(IFileManager::Get().MakeDirectory(*ScriptRoot, true));
			SourceFilename = ScriptRoot / TEXT("PackagedReload.as");
		}

		~FScopedProjectRoot()
		{
			if (Root.Contains(TEXT(
				"/Saved/Automation/AngelscriptCachePackagedRuntimeReload/")))
			{
				IFileManager::Get().DeleteDirectory(*Root, false, true);
			}
		}

		bool WriteSource(
			const FString& Unique,
			const int32 Answer,
			const bool bAddStructuralProperty = false) const
		{
			const FString ExtraProperty = bAddStructuralProperty
				? TEXT("\n\tUPROPERTY()\n\tint AddedValue;\n")
				: FString();
			const FString Source = FString::Printf(TEXT(R"AS(
class UCachePackagedReload%sObject : UObject
{
	UPROPERTY()
	int Value;
%s}

int Answer()
{
	return %d;
}
)AS"), *Unique, *ExtraProperty, Answer);
			return FFileHelper::SaveStringToFile(
				Source,
				*SourceFilename,
				FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		}

		bool DeleteSource() const
		{
			return IFileManager::Get().Delete(*SourceFilename, false, true);
		}

		bool WriteInvalidSource(const FString& Unique) const
		{
			const FString Source = FString::Printf(TEXT(R"AS(
class UCachePackagedReload%sObject : UObject
{
	UPROPERTY()
	int Value;
}

int Answer(
)AS"), *Unique);
			return FFileHelper::SaveStringToFile(
				Source,
				*SourceFilename,
				FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		}

		FString Root;
		FString ScriptRoot;
		FString SourceFilename;
	};

	static TUniquePtr<FAngelscriptEngine> CreateEngine(
		const FScopedProjectRoot& Project,
		const EAngelscriptPackagedRuntimeReloadMode Mode,
		const bool bOverrideMode)
	{
		FAngelscriptEngineConfig Config;
		Config.bIsEditor = false;
		Config.bDevelopmentMode = false;
		Config.bSkipThreadedInitialize = true;
		Config.bOverridePackagedRuntimeReloadMode = bOverrideMode;
		Config.PackagedRuntimeReloadMode = Mode;
		Config.PackagedRuntimeReloadScanIntervalSeconds = 0.1f;

		FAngelscriptEngineDependencies Dependencies =
			FAngelscriptEngineDependencies::CreateDefault();
		Dependencies.GetProjectDir = [&Project]()
		{
			return Project.Root;
		};
		Dependencies.GetEnabledPluginScriptRoots = []()
		{
			return TArray<FString>();
		};
		Dependencies.GetEnabledPluginScriptRootDescriptors = []()
		{
			return TArray<FAngelscriptPluginScriptRoot>();
		};
		return FAngelscriptEngine::Create(Config, Dependencies);
	}

	static bool ExecuteAnswer(
		FAngelscriptEngine& Engine,
		int32& OutAnswer)
	{
		const TSharedPtr<FAngelscriptModuleDesc> Module =
			Engine.GetModuleByModuleName(TEXT("PackagedReload"));
		if (!Module.IsValid() || Module->ScriptModule == nullptr)
		{
			return false;
		}
		asIScriptFunction* Function =
			Module->ScriptModule->GetFunctionByDecl("int Answer()");
		if (Function == nullptr)
		{
			return false;
		}
		asIScriptContext* Context = Engine.GetScriptEngine()->CreateContext();
		if (Context == nullptr)
		{
			return false;
		}
		const bool bSuccess = Context->Prepare(Function) >= 0
			&& Context->Execute() == asEXECUTION_FINISHED;
		if (bSuccess)
		{
			OutAnswer = static_cast<int32>(Context->GetReturnDWord());
		}
		Context->Release();
		return bSuccess;
	}
}

TEST_CLASS_WITH_FLAGS(FAngelscriptCachePackagedRuntimeReloadTests,
	"Angelscript.TestModule.Cache.PackagedRuntimeReload",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	TEST_METHOD(DefaultPackagedPolicyRejectsManualRequest)
	{
		using namespace AngelscriptCachePackagedRuntimeReloadTests_Private;
		FScopedProjectRoot Project;
		const FString Unique = FGuid::NewGuid().ToString(EGuidFormats::Digits);
		ASSERT_THAT(IsTrue(Project.WriteSource(Unique, 10)));
		TUniquePtr<FAngelscriptEngine> Engine = CreateEngine(
			Project, EAngelscriptPackagedRuntimeReloadMode::Disabled, false);
		ASSERT_THAT(IsNotNull(Engine.Get()));
		FAngelscriptEngineScope Scope(*Engine);

		ASSERT_THAT(AreEqual(
			EAngelscriptRuntimeReloadRequestStatus::Disabled,
			Engine->RequestPackagedRuntimeReload()));
		FAngelscriptRuntimeReloadResult Result;
		ASSERT_THAT(IsFalse(
			Engine->ConsumePackagedRuntimeReloadResult(Result)));
	}

	TEST_METHOD(ManualBodyEditRunsAtTickSafePointAndAppliesCodeOnly)
	{
		using namespace AngelscriptCachePackagedRuntimeReloadTests_Private;
		FScopedProjectRoot Project;
		const FString Unique = FGuid::NewGuid().ToString(EGuidFormats::Digits);
		ASSERT_THAT(IsTrue(Project.WriteSource(Unique, 20)));
		TUniquePtr<FAngelscriptEngine> Engine = CreateEngine(
			Project, EAngelscriptPackagedRuntimeReloadMode::Manual, true);
		ASSERT_THAT(IsNotNull(Engine.Get()));
		FAngelscriptEngineScope Scope(*Engine);
		int32 Answer = 0;
		ASSERT_THAT(IsTrue(ExecuteAnswer(*Engine, Answer)));
		ASSERT_THAT(AreEqual(20, Answer));
		Engine->GetCacheService()->ConfigureDecisionTrace(true, 8);

		ASSERT_THAT(IsTrue(Project.WriteSource(Unique, 21)));
		ASSERT_THAT(AreEqual(
			EAngelscriptRuntimeReloadRequestStatus::Queued,
			Engine->RequestPackagedRuntimeReload()));
		ASSERT_THAT(AreEqual(
			EAngelscriptRuntimeReloadRequestStatus::Busy,
			Engine->RequestPackagedRuntimeReload()));
		FAngelscriptRuntimeReloadResult Result;
		ASSERT_THAT(IsFalse(
			Engine->ConsumePackagedRuntimeReloadResult(Result)));
		Engine->Tick(0.0f);
		ASSERT_THAT(IsTrue(
			Engine->ConsumePackagedRuntimeReloadResult(Result)));
		ASSERT_THAT(AreEqual(
			EAngelscriptRuntimeReloadOutcome::AppliedCodeOnly,
			Result.Outcome));
		ASSERT_THAT(IsTrue(Result.ChangedModuleNames.Contains(
			TEXT("PackagedReload"))));
		ASSERT_THAT(AreEqual(1, Result.ChangedModuleKeys.Num()));
		ASSERT_THAT(AreEqual(64, Result.ChangedModuleKeys[0].Len()));
		const FAngelscriptCacheDecisionTraceSnapshot Trace =
			Engine->GetCacheService()->CaptureDecisionTrace();
		ASSERT_THAT(IsTrue(Trace.Events.ContainsByPredicate(
			[](const FAngelscriptCacheDecisionEvent& Event)
			{
				return Event.Stage ==
					EAngelscriptCacheDecisionStage::RuntimeReload
					&& Event.Outcome ==
						EAngelscriptCacheDecisionOutcome::Completed
					&& Event.ModuleKeys.Num() == 1;
			})));
		ASSERT_THAT(IsTrue(ExecuteAnswer(*Engine, Answer)));
		ASSERT_THAT(AreEqual(21, Answer));
	}

	TEST_METHOD(ManualNoChangesCompletesAtTickSafePoint)
	{
		using namespace AngelscriptCachePackagedRuntimeReloadTests_Private;
		FScopedProjectRoot Project;
		const FString Unique = FGuid::NewGuid().ToString(EGuidFormats::Digits);
		ASSERT_THAT(IsTrue(Project.WriteSource(Unique, 25)));
		TUniquePtr<FAngelscriptEngine> Engine = CreateEngine(
			Project, EAngelscriptPackagedRuntimeReloadMode::Manual, true);
		ASSERT_THAT(IsNotNull(Engine.Get()));
		FAngelscriptEngineScope Scope(*Engine);

		ASSERT_THAT(AreEqual(
			EAngelscriptRuntimeReloadRequestStatus::Queued,
			Engine->RequestPackagedRuntimeReload()));
		FAngelscriptRuntimeReloadResult Result;
		ASSERT_THAT(IsFalse(
			Engine->ConsumePackagedRuntimeReloadResult(Result)));
		Engine->Tick(0.0f);
		ASSERT_THAT(IsTrue(
			Engine->ConsumePackagedRuntimeReloadResult(Result)));
		ASSERT_THAT(AreEqual(
			EAngelscriptRuntimeReloadOutcome::NoChanges,
			Result.Outcome));
	}

	TEST_METHOD(AutomaticBodyEditUsesRuntimeContentHashing)
	{
		using namespace AngelscriptCachePackagedRuntimeReloadTests_Private;
		FScopedProjectRoot Project;
		const FString Unique = FGuid::NewGuid().ToString(EGuidFormats::Digits);
		ASSERT_THAT(IsTrue(Project.WriteSource(Unique, 26)));
		TUniquePtr<FAngelscriptEngine> Engine = CreateEngine(
			Project, EAngelscriptPackagedRuntimeReloadMode::Automatic, true);
		ASSERT_THAT(IsNotNull(Engine.Get()));
		FAngelscriptEngineScope Scope(*Engine);

		ASSERT_THAT(IsTrue(Project.WriteSource(Unique, 27)));
		FPlatformProcess::Sleep(0.12f);
		Engine->Tick(0.0f);
		FAngelscriptRuntimeReloadResult Result;
		ASSERT_THAT(IsTrue(
			Engine->ConsumePackagedRuntimeReloadResult(Result)));
		ASSERT_THAT(AreEqual(
			EAngelscriptRuntimeReloadOutcome::AppliedCodeOnly,
			Result.Outcome));
		int32 Answer = 0;
		ASSERT_THAT(IsTrue(ExecuteAnswer(*Engine, Answer)));
		ASSERT_THAT(AreEqual(27, Answer));
	}

	TEST_METHOD(ManualStructuralEditRequiresRestartAndKeepsLastGoodActive)
	{
		using namespace AngelscriptCachePackagedRuntimeReloadTests_Private;
		FScopedProjectRoot Project;
		const FString Unique = FGuid::NewGuid().ToString(EGuidFormats::Digits);
		ASSERT_THAT(IsTrue(Project.WriteSource(Unique, 30)));
		TUniquePtr<FAngelscriptEngine> Engine = CreateEngine(
			Project, EAngelscriptPackagedRuntimeReloadMode::Manual, true);
		ASSERT_THAT(IsNotNull(Engine.Get()));
		FAngelscriptEngineScope Scope(*Engine);

		ASSERT_THAT(IsTrue(Project.WriteSource(Unique, 31, true)));
		ASSERT_THAT(AreEqual(
			EAngelscriptRuntimeReloadRequestStatus::Queued,
			Engine->RequestPackagedRuntimeReload()));
		Engine->Tick(0.0f);
		FAngelscriptRuntimeReloadResult Result;
		ASSERT_THAT(IsTrue(
			Engine->ConsumePackagedRuntimeReloadResult(Result)));
		ASSERT_THAT(AreEqual(
			EAngelscriptRuntimeReloadOutcome::RequiresRestart,
			Result.Outcome));
		int32 Answer = 0;
		ASSERT_THAT(IsTrue(ExecuteAnswer(*Engine, Answer)));
		ASSERT_THAT(AreEqual(30, Answer));
		ASSERT_THAT(IsTrue(
			Engine->GetCacheService()->GetLifecyclePublications().
				PendingColdStart.IsValid()));
	}

	TEST_METHOD(ManualDeletionRequiresRestartAndKeepsLastGoodActive)
	{
		using namespace AngelscriptCachePackagedRuntimeReloadTests_Private;
		FScopedProjectRoot Project;
		const FString Unique = FGuid::NewGuid().ToString(EGuidFormats::Digits);
		ASSERT_THAT(IsTrue(Project.WriteSource(Unique, 32)));
		TUniquePtr<FAngelscriptEngine> Engine = CreateEngine(
			Project, EAngelscriptPackagedRuntimeReloadMode::Manual, true);
		ASSERT_THAT(IsNotNull(Engine.Get()));
		FAngelscriptEngineScope Scope(*Engine);

		ASSERT_THAT(IsTrue(Project.DeleteSource()));
		ASSERT_THAT(AreEqual(
			EAngelscriptRuntimeReloadRequestStatus::Queued,
			Engine->RequestPackagedRuntimeReload()));
		Engine->Tick(0.0f);
		FAngelscriptRuntimeReloadResult Result;
		ASSERT_THAT(IsTrue(
			Engine->ConsumePackagedRuntimeReloadResult(Result)));
		ASSERT_THAT(AreEqual(
			EAngelscriptRuntimeReloadOutcome::RequiresRestart,
			Result.Outcome));
		int32 Answer = 0;
		ASSERT_THAT(IsTrue(ExecuteAnswer(*Engine, Answer)));
		ASSERT_THAT(AreEqual(32, Answer));
	}

	TEST_METHOD(ManualCompileFailureKeepsLastGoodAndRecordsRejectedTrace)
	{
		using namespace AngelscriptCachePackagedRuntimeReloadTests_Private;
		FScopedProjectRoot Project;
		const FString Unique = FGuid::NewGuid().ToString(EGuidFormats::Digits);
		ASSERT_THAT(IsTrue(Project.WriteSource(Unique, 40)));
		TUniquePtr<FAngelscriptEngine> Engine = CreateEngine(
			Project, EAngelscriptPackagedRuntimeReloadMode::Manual, true);
		ASSERT_THAT(IsNotNull(Engine.Get()));
		FAngelscriptEngineScope Scope(*Engine);
		FAngelscriptCacheService* Service = Engine->GetCacheService();
		ASSERT_THAT(IsNotNull(Service));
		Service->ConfigureDecisionTrace(true, 8);
		const FAngelscriptCacheLifecyclePublications Before =
			Service->GetLifecyclePublications();
		ASSERT_THAT(IsTrue(Before.Current.IsValid()));

		ASSERT_THAT(IsTrue(Project.WriteInvalidSource(Unique)));
		TestRunner->AddExpectedErrorPlain(
			TEXT("PackagedReload.as:"),
			EAutomationExpectedErrorFlags::Contains, 1);
		TestRunner->AddExpectedErrorPlain(
			TEXT("Expected ')' or ','"),
			EAutomationExpectedErrorFlags::Contains, 1);
		TestRunner->AddExpectedErrorPlain(
			TEXT("Instead found ';'"),
			EAutomationExpectedErrorFlags::Contains, 1);
		TestRunner->AddExpectedErrorPlain(
			TEXT("Hot reload failed due to script compile errors"),
			EAutomationExpectedErrorFlags::Contains, 1);
		ASSERT_THAT(AreEqual(
			EAngelscriptRuntimeReloadRequestStatus::Queued,
			Engine->RequestPackagedRuntimeReload()));
		Engine->Tick(0.0f);

		FAngelscriptRuntimeReloadResult Result;
		ASSERT_THAT(IsTrue(
			Engine->ConsumePackagedRuntimeReloadResult(Result)));
		ASSERT_THAT(AreEqual(
			EAngelscriptRuntimeReloadOutcome::CompileFailed,
			Result.Outcome));
		ASSERT_THAT(IsTrue(Result.ChangedModuleNames.Contains(
			TEXT("PackagedReload"))));
		ASSERT_THAT(AreEqual(1, Result.ChangedModuleKeys.Num()));
		ASSERT_THAT(AreEqual(64, Result.ChangedModuleKeys[0].Len()));

		const FAngelscriptCacheLifecyclePublications After =
			Service->GetLifecyclePublications();
		ASSERT_THAT(IsTrue(After.Current == Before.Current));
		ASSERT_THAT(IsFalse(After.PendingColdStart.IsValid()));
		const FAngelscriptCacheDecisionTraceSnapshot Trace =
			Service->CaptureDecisionTrace();
		ASSERT_THAT(IsTrue(Trace.Events.ContainsByPredicate(
			[](const FAngelscriptCacheDecisionEvent& Event)
			{
				return Event.Stage ==
					EAngelscriptCacheDecisionStage::RuntimeReload
					&& Event.Outcome ==
						EAngelscriptCacheDecisionOutcome::Rejected
					&& Event.ModuleKeys.Num() == 1;
			})));
		int32 Answer = 0;
		ASSERT_THAT(IsTrue(ExecuteAnswer(*Engine, Answer)));
		ASSERT_THAT(AreEqual(40, Answer));
	}
};

#endif
