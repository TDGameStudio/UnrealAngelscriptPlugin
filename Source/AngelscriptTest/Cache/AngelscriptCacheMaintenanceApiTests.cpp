#include "Cache/AngelscriptCacheDiagnostics.h"

#include "AngelscriptEngine.h"
#include "CQTest.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "Misc/StringOutputDevice.h"
#include "Shared/AngelscriptTestEngineAcquisition.h"

#include "angelscript.h"

#if WITH_ANGELSCRIPT_UNITTESTS

namespace AngelscriptCacheMaintenanceApiTests_Private
{
	class FScopedProjectRoot final
	{
	public:
		FScopedProjectRoot()
		{
			Root = FPaths::ConvertRelativePathToFull(FPaths::Combine(
				FPaths::ProjectSavedDir(),
				TEXT("Automation/AngelscriptCacheMaintenanceApi"),
				FGuid::NewGuid().ToString(EGuidFormats::Digits)));
			FPaths::NormalizeDirectoryName(Root);
			check(Root.Contains(
				TEXT("/Saved/Automation/AngelscriptCacheMaintenanceApi/")));
			ScriptRoot = Root / TEXT("Script");
			CacheRoot = Root / TEXT("CacheV2");
			check(IFileManager::Get().MakeDirectory(*ScriptRoot, true));
		}

		~FScopedProjectRoot()
		{
			if (Root.Contains(
				TEXT("/Saved/Automation/AngelscriptCacheMaintenanceApi/")))
			{
				IFileManager::Get().DeleteDirectory(*Root, false, true);
			}
		}

		bool WriteSource(const FString& Source) const
		{
			return FFileHelper::SaveStringToFile(
				Source, *(ScriptRoot / TEXT("MaintenanceApi.as")));
		}

		FString Root;
		FString ScriptRoot;
		FString CacheRoot;
	};

	static TUniquePtr<FAngelscriptEngine> CreateEngine(
		const FScopedProjectRoot& Project)
	{
		FAngelscriptEngineConfig Config;
		Config.bIsEditor = true;
		Config.bDevelopmentMode = true;
		Config.bSkipThreadedInitialize = true;
		Config.CacheV2RootOverride = Project.CacheRoot;

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
		return CreateScriptScanFreeFullEngineForTesting(Config, Dependencies);
	}

	static bool ExecuteForceCleanValue(
		FAngelscriptEngine& Engine,
		const FString& Declaration,
		int32& OutValue)
	{
		const TSharedPtr<FAngelscriptModuleDesc> Module =
			Engine.GetModuleByModuleName(TEXT("MaintenanceApi"));
		if (!Module.IsValid() || Module->ScriptModule == nullptr)
		{
			return false;
		}
		asIScriptFunction* Function = Module->ScriptModule->GetFunctionByDecl(
			TCHAR_TO_UTF8(*Declaration));
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
			OutValue = static_cast<int32>(Context->GetReturnDWord());
		}
		Context->Release();
		return bSuccess;
	}
}

TEST_CLASS_WITH_FLAGS(FAngelscriptCacheMaintenanceApiTests,
	"Angelscript.TestModule.Cache.MaintenanceApi",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	TEST_METHOD(FlushedStoreSupportsShallowDeepVerifyAndCompaction)
	{
		using namespace AngelscriptCacheMaintenanceApiTests_Private;
		FScopedProjectRoot Project;
		const FString Unique = FGuid::NewGuid().ToString(EGuidFormats::Digits);
		const FString Source = FString::Printf(TEXT(R"AS(
enum ECacheMaintenanceApi%sState
{
	Ready = 1,
}

int ReadCacheMaintenanceApi%sValue()
{
	return 501;
}
)AS"), *Unique, *Unique);
		ASSERT_THAT(IsTrue(Project.WriteSource(Source)));

		TUniquePtr<FAngelscriptEngine> Engine = CreateEngine(Project);
		ASSERT_THAT(IsNotNull(Engine.Get()));
		FAngelscriptEngineScope Scope(*Engine);
		Engine->InitialCompile();
		ASSERT_THAT(IsTrue(
			FlushAngelscriptCacheToStore(Engine.Get()).IsSuccess()));

		const FAngelscriptCacheVerifyApiResult Shallow =
			VerifyAngelscriptCacheStore(
				Engine.Get(), EAngelscriptCacheDiagnosticGeneration::Current, false);
		ASSERT_THAT(IsTrue(Shallow.IsSuccess()));
		ASSERT_THAT(IsTrue(Shallow.GenerationId.IsSet()));
		ASSERT_THAT(IsFalse(Shallow.bDeep));

		const FAngelscriptCacheVerifyApiResult Deep =
			VerifyAngelscriptCacheStore(
				Engine.Get(), EAngelscriptCacheDiagnosticGeneration::Current, true);
		ASSERT_THAT(IsTrue(Deep.IsSuccess()));
		ASSERT_THAT(IsTrue(Deep.GenerationId == Shallow.GenerationId));
		ASSERT_THAT(IsTrue(Deep.bDeep));
		ASSERT_THAT(IsTrue(Deep.ManifestRecordCount > 0));
		ASSERT_THAT(IsTrue(Deep.ReachableRecordCount > 0));

		const FAngelscriptCacheCompactApiResult Compact =
			CompactAngelscriptCacheStoreForEngine(Engine.Get());
		ASSERT_THAT(IsTrue(Compact.IsSuccess()));
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheStoreCommitState::CompactionCommitted,
			Compact.Store.CommitState));

		FStringOutputDevice ConsoleOutput;
		ASSERT_THAT(IsTrue(IConsoleManager::Get().ProcessUserConsoleInput(
			TEXT("as.Cache.Verify Generation=Current Deep=1"),
			ConsoleOutput, nullptr)));
		ASSERT_THAT(IsTrue(ConsoleOutput.Contains(
			TEXT("as.Cache.Verify succeeded"))));
	}

	TEST_METHOD(MissingEngineIsTypedAndMaintenanceCommandsAreRegistered)
	{
		const FAngelscriptCacheVerifyApiResult Verify =
			VerifyAngelscriptCacheStore(
				nullptr, EAngelscriptCacheDiagnosticGeneration::Current, true);
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheDiagnosticApiError::EngineUnavailable,
			Verify.Error));
		ASSERT_THAT(IsFalse(Verify.IsSuccess()));

		const FAngelscriptCacheCompactApiResult Compact =
			CompactAngelscriptCacheStoreForEngine(nullptr);
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheDiagnosticApiError::EngineUnavailable,
			Compact.Error));
		ASSERT_THAT(IsFalse(Compact.IsSuccess()));
		const FAngelscriptCacheForceCleanApiResult ForceClean =
			ForceCleanAngelscriptCache(nullptr, FString());
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheDiagnosticApiError::EngineUnavailable,
			ForceClean.Error));
		ASSERT_THAT(IsFalse(ForceClean.IsSuccess()));

		ASSERT_THAT(IsNotNull(IConsoleManager::Get().FindConsoleObject(
			TEXT("as.Cache.Verify"))));
		ASSERT_THAT(IsNotNull(IConsoleManager::Get().FindConsoleObject(
			TEXT("as.Cache.Compact"))));
		ASSERT_THAT(IsNotNull(IConsoleManager::Get().FindConsoleObject(
			TEXT("as.Cache.ForceClean"))));
	}

	TEST_METHOD(ForceCleanUsesAuthoritativeCompileTransaction)
	{
		using namespace AngelscriptCacheMaintenanceApiTests_Private;
		FScopedProjectRoot Project;
		const FString Unique = FGuid::NewGuid().ToString(EGuidFormats::Digits);
		const FString Source = FString::Printf(TEXT(R"AS(
class UCacheForceClean%sObject : UObject
{
	UPROPERTY()
	int Value;
}

int ReadCacheForceClean%sValue()
{
	return 502;
}
)AS"), *Unique, *Unique);
		ASSERT_THAT(IsTrue(Project.WriteSource(Source)));

		TUniquePtr<FAngelscriptEngine> Engine = CreateEngine(Project);
		ASSERT_THAT(IsNotNull(Engine.Get()));
		FAngelscriptEngineScope Scope(*Engine);
		Engine->InitialCompile();
		const FAngelscriptCacheLifecyclePublications Before =
			Engine->GetCacheService()->GetLifecyclePublications();
		ASSERT_THAT(IsTrue(Before.Current.IsValid()));

		const FAngelscriptCacheForceCleanApiResult Result =
			ForceCleanAngelscriptCache(Engine.Get(), TEXT("MaintenanceApi"));
		ASSERT_THAT(IsTrue(Result.IsSuccess()));
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheForceCleanOutcome::Applied,
			Result.Outcome));
		ASSERT_THAT(AreEqual(1, Result.SelectedModuleNames.Num()));
		ASSERT_THAT(AreEqual(FString(TEXT("MaintenanceApi")),
			Result.SelectedModuleNames[0]));
		const FAngelscriptCacheLifecyclePublications After =
			Engine->GetCacheService()->GetLifecyclePublications();
		ASSERT_THAT(IsTrue(After.Current.IsValid()));
		ASSERT_THAT(IsTrue(After.Current->TransactionOrdinal
			> Before.Current->TransactionOrdinal));

		FStringOutputDevice ConsoleOutput;
		ASSERT_THAT(IsTrue(IConsoleManager::Get().ProcessUserConsoleInput(
			TEXT("as.Cache.ForceClean Module=MaintenanceApi"),
			ConsoleOutput, nullptr)));
		ASSERT_THAT(IsTrue(ConsoleOutput.Contains(
			TEXT("as.Cache.ForceClean succeeded"))));
	}

	TEST_METHOD(ForceCleanCompileFailureKeepsLastGoodActive)
	{
		using namespace AngelscriptCacheMaintenanceApiTests_Private;
		FScopedProjectRoot Project;
		const FString Unique = FGuid::NewGuid().ToString(EGuidFormats::Digits);
		const FString Declaration = FString::Printf(
			TEXT("int ReadCacheForceClean%sValue()"), *Unique);
		const FString InitialSource = FString::Printf(TEXT(R"AS(
class UCacheForceClean%sObject : UObject
{
	UPROPERTY()
	int Value;
}

int ReadCacheForceClean%sValue()
{
	return 503;
}
)AS"), *Unique, *Unique);
		ASSERT_THAT(IsTrue(Project.WriteSource(InitialSource)));

		TUniquePtr<FAngelscriptEngine> Engine = CreateEngine(Project);
		ASSERT_THAT(IsNotNull(Engine.Get()));
		FAngelscriptEngineScope Scope(*Engine);
		Engine->InitialCompile();
		const FAngelscriptCacheLifecyclePublications Before =
			Engine->GetCacheService()->GetLifecyclePublications();
		ASSERT_THAT(IsTrue(Before.Current.IsValid()));

		int32 Value = 0;
		ASSERT_THAT(IsTrue(ExecuteForceCleanValue(
			*Engine, Declaration, Value)));
		ASSERT_THAT(AreEqual(503, Value));

		const FString InvalidSource = FString::Printf(TEXT(R"AS(
class UCacheForceClean%sObject : UObject
{
	UPROPERTY()
	int Value;
}

int ReadCacheForceClean%sValue(
)AS"), *Unique, *Unique);
		ASSERT_THAT(IsTrue(Project.WriteSource(InvalidSource)));
		TestRunner->AddExpectedErrorPlain(
			TEXT("MaintenanceApi.as:"),
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

		const FAngelscriptCacheForceCleanApiResult Result =
			ForceCleanAngelscriptCache(Engine.Get(), TEXT("MaintenanceApi"));
		ASSERT_THAT(IsFalse(Result.IsSuccess()));
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheDiagnosticApiError::ForceCleanFailed,
			Result.Error));
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheForceCleanOutcome::CompileFailed,
			Result.Outcome));
		const FAngelscriptCacheLifecyclePublications After =
			Engine->GetCacheService()->GetLifecyclePublications();
		ASSERT_THAT(IsTrue(After.Current == Before.Current));
		ASSERT_THAT(IsFalse(After.PendingColdStart.IsValid()));

		Value = 0;
		ASSERT_THAT(IsTrue(ExecuteForceCleanValue(
			*Engine, Declaration, Value)));
		ASSERT_THAT(AreEqual(503, Value));
	}
};

#endif
