#include "Cache/AngelscriptCacheDiagnostics.h"
#include "Cache/AngelscriptCacheRestore.h"
#include "Cache/AngelscriptCacheService.h"

#include "AngelscriptTestMacros.h"
#include "AngelscriptTestUtilities.h"
#include "ClassGenerator/ASClass.h"
#include "CQTest.h"
#include "Editor/AngelscriptPIETestUtils.h"
#include "Shared/AngelscriptTestEngineAcquisition.h"

#include "Engine/World.h"
#include "GameFramework/GameModeBase.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"

#if WITH_ANGELSCRIPT_UNITTESTS && WITH_EDITOR

namespace AngelscriptCachePIELifecycleTests_Private
{
	static constexpr double DefaultTimeoutSeconds = 15.0;

	class FScopedProjectRoot final
	{
	public:
		FScopedProjectRoot()
		{
			Root = FPaths::ConvertRelativePathToFull(FPaths::Combine(
				FPaths::ProjectSavedDir(),
				TEXT("Automation/AngelscriptCachePIE"),
				FGuid::NewGuid().ToString(EGuidFormats::Digits)));
			FPaths::NormalizeDirectoryName(Root);
			check(Root.Contains(TEXT("/Saved/Automation/AngelscriptCachePIE/")));
			ScriptRoot = Root / TEXT("Script");
			check(IFileManager::Get().MakeDirectory(*ScriptRoot, true));
		}

		~FScopedProjectRoot()
		{
			if (Root.Contains(TEXT("/Saved/Automation/AngelscriptCachePIE/")))
			{
				IFileManager::Get().DeleteDirectory(*Root, false, true);
			}
		}

		bool WriteSource(
			const FString& RelativePath,
			const FString& Source) const
		{
			return FFileHelper::SaveStringToFile(
				Source,
				*(ScriptRoot / RelativePath),
				FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		}

		FString Root;
		FString ScriptRoot;
	};

	struct FPIEFixture final
	{
		UClass* ScriptClass = nullptr;
		UASClass* InitialScriptASClass = nullptr;
		UWorld* EditorWorld = nullptr;
		FRequestPlaySessionParams RequestParams;
	};

	struct FPIETestState final
	{
		FScopedProjectRoot Project;
		TUniquePtr<FAngelscriptEngine> Engine;
		TUniquePtr<FAngelscriptEngineScope> EngineScope;
		FPIEFixture Fixture;
		TSharedPtr<const FAngelscriptCacheSuccessfulPublicationDto,
			ESPMode::ThreadSafe> InitialCurrent;
		TSharedPtr<const FAngelscriptCacheSuccessfulPublicationDto,
			ESPMode::ThreadSafe> BodyCurrent;
		TSharedPtr<const FAngelscriptCacheSuccessfulPublicationDto,
			ESPMode::ThreadSafe> Pending;
		FAngelscriptFunctionArtifactIdentity InitialFunctionIdentity;
		TStrongObjectPtr<UObject> PIEObject;
	};

	class FStartPIEFromRequestProvider final : public IAutomationLatentCommand
	{
	public:
		explicit FStartPIEFromRequestProvider(
			TFunction<FRequestPlaySessionParams()> InRequestProvider)
			: RequestProvider(MoveTemp(InRequestProvider))
		{
		}

		bool Update() override
		{
			if (!StartCommand.IsValid())
			{
				const FRequestPlaySessionParams Request = RequestProvider();
				if (FAutomationTestBase* CurrentTest =
					FAutomationTestFramework::Get().GetCurrentTest())
				{
					if (CurrentTest->HasAnyErrors())
					{
						return true;
					}
				}
				StartCommand = MakeUnique<FStartPIEForAutomationCommand>(Request);
			}
			return StartCommand->Update();
		}

	private:
		TFunction<FRequestPlaySessionParams()> RequestProvider;
		TUniquePtr<FStartPIEForAutomationCommand> StartCommand;
	};

	static TUniquePtr<FAngelscriptEngine> CreateEditorEngine(
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

	static bool PreparePIEFixture(
		FAutomationTestBase& Test,
		FPIETestState& State,
		const FName ClassName,
		const TCHAR* Context)
	{
		FNoDiscardAsserter LocalAssert(Test);
		State.Fixture.ScriptClass =
			FindGeneratedClass(State.Engine.Get(), ClassName);
		if (!LocalAssert.IsNotNull(State.Fixture.ScriptClass,
			*FString::Printf(TEXT("%s should compile an AS UObject"),
				Context))
			|| !LocalAssert.IsTrue(
				State.Fixture.ScriptClass->IsChildOf(UObject::StaticClass()),
				*FString::Printf(
					TEXT("%s class should derive from UObject"), Context)))
		{
			return false;
		}
		State.Fixture.InitialScriptASClass =
			Cast<UASClass>(State.Fixture.ScriptClass);
		if (!LocalAssert.IsNotNull(State.Fixture.InitialScriptASClass,
			*FString::Printf(TEXT("%s class should be a UASClass"), Context)))
		{
			return false;
		}

		State.Fixture.EditorWorld =
			AngelscriptPIETestUtils::CreateTransientEmptyMap(Test, Context);
		if (!LocalAssert.IsNotNull(State.Fixture.EditorWorld,
			*FString::Printf(TEXT("%s should create an editor map"), Context)))
		{
			return false;
		}
		return AngelscriptPIETestUtils::BuildStandalonePIERequest(
			Test, AGameModeBase::StaticClass(), State.Fixture.RequestParams);
	}

	static bool RebuildPIERequest(
		FAutomationTestBase& Test,
		FPIEFixture& Fixture,
		const TCHAR* Context)
	{
		FNoDiscardAsserter LocalAssert(Test);
		if (!LocalAssert.IsNotNull(Fixture.EditorWorld,
			*FString::Printf(TEXT("%s should retain its editor world"), Context))
			|| !LocalAssert.IsNotNull(Fixture.ScriptClass,
				*FString::Printf(TEXT("%s should retain its AS class"), Context)))
		{
			return false;
		}
		Fixture.RequestParams = FRequestPlaySessionParams();
		return AngelscriptPIETestUtils::BuildStandalonePIERequest(
			Test, AGameModeBase::StaticClass(), Fixture.RequestParams);
	}

	static UObject* GetOrCreatePIEScriptObject(
		FAutomationTestBase& Test,
		FPIETestState& State,
		const TCHAR* Context)
	{
		FNoDiscardAsserter LocalAssert(Test);
		UWorld* World = AngelscriptPIETestUtils::FindPIEWorld();
		if (!LocalAssert.IsNotNull(World,
			*FString::Printf(TEXT("%s should expose a PIE world"), Context))
			|| !LocalAssert.AreEqual(EWorldType::PIE, World->WorldType,
				*FString::Printf(TEXT("%s should be a PIE world"), Context))
			|| !LocalAssert.IsNotNull(State.Fixture.ScriptClass,
				*FString::Printf(TEXT("%s should retain its AS class"), Context)))
		{
			return nullptr;
		}
		if (!State.PIEObject.IsValid())
		{
			State.PIEObject.Reset(NewObject<UObject>(
				World, State.Fixture.ScriptClass));
		}
		UObject* Object = State.PIEObject.Get();
		return LocalAssert.IsNotNull(Object,
			*FString::Printf(TEXT("%s should create a live AS object in PIE"),
				Context)) ? Object : nullptr;
	}

	static bool InvokeGetValue(
		FAutomationTestBase& Test,
		FAngelscriptEngine& Engine,
		UObject* Object,
		const int32 ExpectedValue,
		const TCHAR* Context)
	{
		FNoDiscardAsserter LocalAssert(Test);
		if (!LocalAssert.IsNotNull(Object,
			*FString::Printf(TEXT("%s should have a live PIE object"), Context)))
		{
			return false;
		}

		UASClass* ScriptClass = UASClass::GetFirstASClass(Object);
		if (!LocalAssert.IsNotNull(ScriptClass,
			*FString::Printf(TEXT("%s should have an AS owner class"), Context)))
		{
			return false;
		}
		asITypeInfo* ScriptType = static_cast<asITypeInfo*>(
			ScriptClass->ScriptTypePtr);
		if (!LocalAssert.IsNotNull(ScriptType,
			*FString::Printf(TEXT("%s should expose its live AS type"), Context)))
		{
			return false;
		}
		asIScriptFunction* Function = ScriptType->GetMethodByDecl("int GetValue()");
		if (!LocalAssert.IsNotNull(Function,
			*FString::Printf(TEXT("%s should expose the non-reflected GetValue method"),
				Context)))
		{
			return false;
		}

		asIScriptContext* ScriptContext =
			Engine.GetScriptEngine()->CreateContext();
		if (!LocalAssert.IsNotNull(ScriptContext,
			*FString::Printf(TEXT("%s should create an AS context"), Context)))
		{
			return false;
		}
		const int PrepareResult = ScriptContext->Prepare(Function);
		const int SetObjectResult = PrepareResult >= 0
			? ScriptContext->SetObject(Object) : asERROR;
		const int ExecuteResult = PrepareResult >= 0 && SetObjectResult >= 0
			? ScriptContext->Execute() : asERROR;
		const int32 ActualValue = ExecuteResult == asEXECUTION_FINISHED
			? static_cast<int32>(ScriptContext->GetReturnDWord()) : 0;
		ScriptContext->Release();
		if (!LocalAssert.AreEqual(asSUCCESS, PrepareResult,
			*FString::Printf(TEXT("%s should prepare GetValue"), Context))
			|| !LocalAssert.AreEqual(asSUCCESS, SetObjectResult,
				*FString::Printf(TEXT("%s should bind the live PIE object"), Context))
			|| !LocalAssert.AreEqual(static_cast<int32>(asEXECUTION_FINISHED),
				ExecuteResult,
				*FString::Printf(TEXT("%s should execute GetValue"), Context)))
		{
			return false;
		}
		return LocalAssert.AreEqual(ExpectedValue, ActualValue,
			*FString::Printf(TEXT("%s should observe GetValue=%d"),
				Context, ExpectedValue));
	}

	static const FAngelscriptCacheLiveFunctionRoute* FindGetValueRoute(
		const FAngelscriptCacheFunctionRouteSnapshot& Snapshot,
		const FAngelscriptStableModuleKey& ModuleKey)
	{
		return Snapshot.FunctionRoutes.FindByPredicate(
			[&ModuleKey](const FAngelscriptCacheLiveFunctionRoute& Route)
			{
				return Route.ModuleKey == ModuleKey
					&& Route.CanonicalDeclaration.Contains(TEXT("GetValue("));
			});
	}

	static bool LogCacheStage(
		FAutomationTestBase& Test,
		FAngelscriptEngine& Engine,
		const TCHAR* Stage,
		const int32 ObservedValue,
		const bool bIncludeJson = false)
	{
		FAngelscriptCacheService* Service = Engine.GetCacheService();
		if (Service == nullptr)
		{
			Test.AddError(FString::Printf(
				TEXT("[CacheV2][PIE] Stage=%s has no Cache service"), Stage));
			return false;
		}
		const FAngelscriptCacheLifecyclePublications Publications =
			Service->GetLifecyclePublications();
		const TSharedPtr<const FAngelscriptCacheFunctionRouteSnapshot,
			ESPMode::ThreadSafe> Routes = Engine.GetFunctionRouteSnapshot();
		const FAngelscriptCacheDiagnosticJsonResult Diagnostic =
			CaptureAngelscriptCacheDiagnosticJson(&Engine);
		if (!Diagnostic.IsSuccess())
		{
			Test.AddError(FString::Printf(
				TEXT("[CacheV2][PIE] Stage=%s diagnostic failed: %s"),
				Stage, *Diagnostic.Detail));
			return false;
		}

		Test.AddInfo(FString::Printf(
			TEXT("[CacheV2][PIE] Stage=%s CurrentTx=%llu PendingTx=%llu LatestTx=%llu CurrentModules=%d PendingModules=%d RouteOrdinal=%llu Vm=%u Native=%u Observed=%d JsonBytes=%d"),
			Stage,
			Publications.Current.IsValid()
				? Publications.Current->TransactionOrdinal : 0,
			Publications.PendingColdStart.IsValid()
				? Publications.PendingColdStart->TransactionOrdinal : 0,
			Publications.LatestSuccessful.IsValid()
				? Publications.LatestSuccessful->TransactionOrdinal : 0,
			Publications.Current.IsValid()
				? Publications.Current->Modules.Num() : 0,
			Publications.PendingColdStart.IsValid()
				? Publications.PendingColdStart->Modules.Num() : 0,
			Routes.IsValid() ? Routes->PublicationOrdinal : 0,
			Routes.IsValid() ? Routes->VmRouteCount : 0,
			Routes.IsValid() ? Routes->NativeRouteCount : 0,
			ObservedValue, Diagnostic.Json.Len()));
		if (bIncludeJson)
		{
			Test.AddInfo(FString::Printf(
				TEXT("[CacheV2][PIE][SessionJson] Stage=%s Json=%s"),
				Stage, *Diagnostic.Json));
		}
		return true;
	}
}

TEST_CLASS_WITH_FLAGS(FAngelscriptCachePIELifecycleTests,
	"Angelscript.TestModule.Cache.PIE",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	inline static const FName ColdWarmModuleName = TEXT("ASCachePIEColdWarm");
	inline static const FString ColdWarmFilename = TEXT("ASCachePIEColdWarm.as");
	inline static const FName ColdWarmClassName =
		TEXT("UCachePIEColdWarmObject");
	inline static const FName MutationModuleName = TEXT("ASCachePIEMutation");
	inline static const FString MutationFilename = TEXT("ASCachePIEMutation.as");
	inline static const FName MutationClassName =
		TEXT("UCachePIEMutationObject");

	void RegisterCleanup(
		const TSharedRef<AngelscriptCachePIELifecycleTests_Private::
			FPIETestState>& State,
		const FName ModuleName)
	{
		// CQTest cleanup runs in reverse registration order. End PIE first, then
		// release the AS class/module and the scoped test Engine.
		TestCommandBuilder.CleanUpWith(TEXT("Release Cache V2 PIE test Engine"),
			[State, ModuleName]()
			{
				if (State->Engine.IsValid())
				{
					State->Engine->DiscardModule(*ModuleName.ToString());
				}
				State->EngineScope.Reset();
				State->Engine.Reset();
			});
		TestCommandBuilder.CleanUpWith(TEXT("End Cache V2 PIE session"), [State]()
		{
			State->PIEObject.Reset();
			AngelscriptPIETestUtils::EndPIE();
		});
	}

	void QueuePIEStart(TFunction<FRequestPlaySessionParams()> RequestProvider)
	{
		AddCommand(MakeShared<
			AngelscriptCachePIELifecycleTests_Private::
				FStartPIEFromRequestProvider>(MoveTemp(RequestProvider)));
	}

public:
	TEST_METHOD(ColdThenWarmPIESessionsKeepCurrentAndBehavior)
	{
		using namespace AngelscriptCachePIELifecycleTests_Private;
		ASSERT_THAT(IsFalse(AngelscriptPIETestUtils::IsPIEWorldAlive()));
		const TSharedRef<FPIETestState> State = MakeShared<FPIETestState>();
		RegisterCleanup(State, ColdWarmModuleName);

		const FString Source = ASTEST_AS(R"AS(
			UCLASS()
			class UCachePIEColdWarmObject : UObject
			{
				int GetValue()
				{
					return 701;
				}
			}
			)AS");
		ASSERT_THAT(IsTrue(State->Project.WriteSource(ColdWarmFilename, Source)));
		State->Engine = CreateEditorEngine(State->Project.Root);
		ASSERT_THAT(IsNotNull(State->Engine.Get()));
		State->EngineScope =
			MakeUnique<FAngelscriptEngineScope>(*State->Engine);
		State->Engine->InitialCompile();
		ASSERT_THAT(IsTrue(PreparePIEFixture(
			*TestRunner, State.Get(), ColdWarmClassName,
			TEXT("Cache V2 cold/warm PIE"))));

		const FAngelscriptCacheLifecyclePublications Initial =
			State->Engine->GetCacheService()->GetLifecyclePublications();
		ASSERT_THAT(IsTrue(Initial.Current.IsValid()));
		ASSERT_THAT(IsFalse(Initial.PendingColdStart.IsValid()));
		State->InitialCurrent = Initial.Current;
		ASSERT_THAT(IsTrue(LogCacheStage(
			*TestRunner, *State->Engine, TEXT("ColdCompiled"), 701, true)));
		if (TestRunner->HasAnyErrors())
		{
			return;
		}

		QueuePIEStart([State]() { return State->Fixture.RequestParams; });
		TestCommandBuilder
			.Until(TEXT("Wait for Cache V2 cold PIE world"), []()
			{
				return AngelscriptPIETestUtils::FindPIEWorld() != nullptr;
			}, FTimespan::FromSeconds(DefaultTimeoutSeconds))
			.Then(TEXT("Assert Cache V2 cold PIE behavior"), [this, State]()
			{
				UObject* Object = GetOrCreatePIEScriptObject(
					*TestRunner, *State, TEXT("Cache V2 cold PIE"));
				ASSERT_THAT(IsTrue(InvokeGetValue(
					*TestRunner, *State->Engine, Object,
					701,
					TEXT("Cache V2 cold PIE"))));
				const FAngelscriptCacheLifecyclePublications Publications =
					State->Engine->GetCacheService()->GetLifecyclePublications();
				ASSERT_THAT(IsTrue(Publications.Current == State->InitialCurrent));
				ASSERT_THAT(IsFalse(Publications.PendingColdStart.IsValid()));
				ASSERT_THAT(IsTrue(LogCacheStage(
					*TestRunner, *State->Engine, TEXT("ColdPIE"), 701)));
			})
			.Then(TEXT("End Cache V2 cold PIE"), [State]()
			{
				State->PIEObject.Reset();
				AngelscriptPIETestUtils::EndPIE();
			})
			.Until(TEXT("Wait for Cache V2 cold PIE shutdown"), []()
			{
				return !AngelscriptPIETestUtils::IsPIEWorldAlive();
			}, FTimespan::FromSeconds(DefaultTimeoutSeconds))
			.Then(TEXT("Prepare Cache V2 warm PIE request"), [this, State]()
			{
				ASSERT_THAT(IsTrue(RebuildPIERequest(
					*TestRunner, State->Fixture,
					TEXT("Cache V2 warm PIE request"))));
			});

		QueuePIEStart([State]() { return State->Fixture.RequestParams; });
		TestCommandBuilder
			.Until(TEXT("Wait for Cache V2 warm PIE world"), []()
			{
				return AngelscriptPIETestUtils::FindPIEWorld() != nullptr;
			}, FTimespan::FromSeconds(DefaultTimeoutSeconds))
			.Then(TEXT("Assert Cache V2 warm PIE behavior"), [this, State]()
			{
				UObject* Object = GetOrCreatePIEScriptObject(
					*TestRunner, *State, TEXT("Cache V2 warm PIE"));
				ASSERT_THAT(IsTrue(InvokeGetValue(
					*TestRunner, *State->Engine, Object,
					701,
					TEXT("Cache V2 warm PIE"))));
				const FAngelscriptCacheLifecyclePublications Publications =
					State->Engine->GetCacheService()->GetLifecyclePublications();
				ASSERT_THAT(IsTrue(Publications.Current == State->InitialCurrent));
				ASSERT_THAT(IsFalse(Publications.PendingColdStart.IsValid()));
				ASSERT_THAT(IsTrue(LogCacheStage(
					*TestRunner, *State->Engine, TEXT("WarmPIE"), 701, true)));
			})
			.Then(TEXT("End Cache V2 warm PIE"), [State]()
			{
				State->PIEObject.Reset();
				AngelscriptPIETestUtils::EndPIE();
			})
			.Until(TEXT("Wait for Cache V2 warm PIE shutdown"), []()
			{
				return !AngelscriptPIETestUtils::IsPIEWorldAlive();
			}, FTimespan::FromSeconds(DefaultTimeoutSeconds));
	}

	TEST_METHOD(BodyFailureStructuralDeferAndPromoteAcrossPIE)
	{
		using namespace AngelscriptCachePIELifecycleTests_Private;
		ASSERT_THAT(IsFalse(AngelscriptPIETestUtils::IsPIEWorldAlive()));
		const TSharedRef<FPIETestState> State = MakeShared<FPIETestState>();
		RegisterCleanup(State, MutationModuleName);

		const FString SourceV1 = ASTEST_AS(R"AS(
			UCLASS()
			class UCachePIEMutationObject : UObject
			{
				int GetValue()
				{
					return 710;
				}
			}
			)AS");
		const FString SourceV2Body = ASTEST_AS(R"AS(
			UCLASS()
			class UCachePIEMutationObject : UObject
			{
				int GetValue()
				{
					return 720;
				}
			}
			)AS");
		const FString SourceInvalid = ASTEST_AS(R"AS(
			UCLASS()
			class UCachePIEMutationObject : UObject
			{
				int GetValue(
			)AS");
		const FString SourceV3Structural = ASTEST_AS(R"AS(
			UCLASS()
			class UCachePIEMutationObject : UObject
			{
				UPROPERTY()
				int AddedValue;

				int GetValue()
				{
					return 730;
				}
			}
			)AS");

		ASSERT_THAT(IsTrue(State->Project.WriteSource(MutationFilename, SourceV1)));
		State->Engine = CreateEditorEngine(State->Project.Root);
		ASSERT_THAT(IsNotNull(State->Engine.Get()));
		State->EngineScope =
			MakeUnique<FAngelscriptEngineScope>(*State->Engine);
		State->Engine->InitialCompile();
		ASSERT_THAT(IsTrue(PreparePIEFixture(
			*TestRunner, State.Get(), MutationClassName,
			TEXT("Cache V2 mutation PIE"))));

		const FAngelscriptCacheLifecyclePublications Initial =
			State->Engine->GetCacheService()->GetLifecyclePublications();
		ASSERT_THAT(IsTrue(Initial.Current.IsValid()));
		ASSERT_THAT(AreEqual(1, Initial.Current->Modules.Num()));
		State->InitialCurrent = Initial.Current;
		const TSharedPtr<const FAngelscriptCacheFunctionRouteSnapshot,
			ESPMode::ThreadSafe> InitialRoutes =
			State->Engine->GetFunctionRouteSnapshot();
		ASSERT_THAT(IsTrue(InitialRoutes.IsValid()));
		const FAngelscriptCacheLiveFunctionRoute* InitialRoute =
			FindGetValueRoute(*InitialRoutes, Initial.Current->Modules[0].ModuleKey);
		ASSERT_THAT(IsNotNull(InitialRoute));
		ASSERT_THAT(IsTrue(InitialRoute->bHasVerifiedArtifactIdentity));
		State->InitialFunctionIdentity = InitialRoute->Identity;
		ASSERT_THAT(IsTrue(LogCacheStage(
			*TestRunner, *State->Engine, TEXT("MutationCompiled"), 710, true)));
		if (TestRunner->HasAnyErrors())
		{
			return;
		}

		QueuePIEStart([State]() { return State->Fixture.RequestParams; });
		TestCommandBuilder
			.Until(TEXT("Wait for Cache V2 mutation PIE world"), []()
			{
				return AngelscriptPIETestUtils::FindPIEWorld() != nullptr;
			}, FTimespan::FromSeconds(DefaultTimeoutSeconds))
			.Then(TEXT("Assert Cache V2 mutation baseline"), [this, State]()
			{
				UObject* Object = GetOrCreatePIEScriptObject(
					*TestRunner, *State, TEXT("Cache V2 mutation baseline"));
				ASSERT_THAT(IsTrue(InvokeGetValue(
					*TestRunner, *State->Engine, Object,
					710,
					TEXT("Cache V2 mutation baseline"))));
			})
			.Then(TEXT("Apply Cache V2 body edit during PIE"),
				[this, State, SourceV2Body]()
			{
				ASSERT_THAT(IsTrue(State->Project.WriteSource(
					MutationFilename, SourceV2Body)));
				QueueModifiedSource(
					*State->Engine, State->Project, MutationFilename);
				State->Engine->CheckForHotReload(ECompileType::SoftReloadOnly);
				const FAngelscriptCacheLifecyclePublications Publications =
					State->Engine->GetCacheService()->GetLifecyclePublications();
				ASSERT_THAT(IsTrue(Publications.Current.IsValid()));
				ASSERT_THAT(IsTrue(Publications.Current != State->InitialCurrent));
				ASSERT_THAT(IsFalse(Publications.PendingColdStart.IsValid()));
				State->BodyCurrent = Publications.Current;

				const TSharedPtr<const FAngelscriptCacheFunctionRouteSnapshot,
					ESPMode::ThreadSafe> Routes =
					State->Engine->GetFunctionRouteSnapshot();
				ASSERT_THAT(IsTrue(Routes.IsValid()));
				const FAngelscriptCacheLiveFunctionRoute* Route = FindGetValueRoute(
					*Routes, Publications.Current->Modules[0].ModuleKey);
				ASSERT_THAT(IsNotNull(Route));
				ASSERT_THAT(IsTrue(Route->Identity.FunctionKey
					== State->InitialFunctionIdentity.FunctionKey));
				ASSERT_THAT(IsFalse(Route->Identity.Content.Execution
					== State->InitialFunctionIdentity.Content.Execution));

				UObject* Object = GetOrCreatePIEScriptObject(
					*TestRunner, *State, TEXT("Cache V2 body edit"));
				ASSERT_THAT(IsTrue(InvokeGetValue(
					*TestRunner, *State->Engine, Object,
					720,
					TEXT("Cache V2 body edit"))));
				ASSERT_THAT(IsTrue(LogCacheStage(
					*TestRunner, *State->Engine, TEXT("BodyEdit"), 720, true)));
			})
			.Then(TEXT("Reject Cache V2 invalid edit during PIE"),
				[this, State, SourceInvalid]()
			{
				ASSERT_THAT(IsTrue(State->Project.WriteSource(
					MutationFilename, SourceInvalid)));
				TestRunner->AddExpectedErrorPlain(
					TEXT("ASCachePIEMutation.as:"),
					EAutomationExpectedErrorFlags::Contains, 1);
				TestRunner->AddExpectedErrorPlain(
					TEXT("Expected method or property"),
					EAutomationExpectedErrorFlags::Contains, 1);
				TestRunner->AddExpectedErrorPlain(
					TEXT("Instead found reserved keyword 'int'"),
					EAutomationExpectedErrorFlags::Contains, 1);
				TestRunner->AddExpectedErrorPlain(
					TEXT("Hot reload failed due to script compile errors"),
					EAutomationExpectedErrorFlags::Contains, 1);
				QueueModifiedSource(
					*State->Engine, State->Project, MutationFilename);
				State->Engine->CheckForHotReload(ECompileType::SoftReloadOnly);
				const FAngelscriptCacheLifecyclePublications Publications =
					State->Engine->GetCacheService()->GetLifecyclePublications();
				ASSERT_THAT(IsTrue(Publications.Current == State->BodyCurrent));
				ASSERT_THAT(IsFalse(Publications.PendingColdStart.IsValid()));
				UObject* Object = GetOrCreatePIEScriptObject(
					*TestRunner, *State, TEXT("Cache V2 invalid edit"));
				ASSERT_THAT(IsTrue(InvokeGetValue(
					*TestRunner, *State->Engine, Object,
					720,
					TEXT("Cache V2 invalid edit last-good"))));
				ASSERT_THAT(IsTrue(LogCacheStage(
					*TestRunner, *State->Engine,
					TEXT("CompileFailure"), 720, true)));
			})
			.Then(TEXT("Defer Cache V2 structural edit during PIE"),
				[this, State, SourceV3Structural]()
			{
				ASSERT_THAT(IsTrue(State->Project.WriteSource(
					MutationFilename, SourceV3Structural)));
				TestRunner->AddExpectedErrorPlain(
					TEXT("Performing a Soft Reload during PIE"),
					EAutomationExpectedErrorFlags::Contains, 0);
				QueueModifiedSource(
					*State->Engine, State->Project, MutationFilename);
				State->Engine->CheckForHotReload(ECompileType::SoftReloadOnly);
				const FAngelscriptCacheLifecyclePublications Publications =
					State->Engine->GetCacheService()->GetLifecyclePublications();
				ASSERT_THAT(IsTrue(Publications.Current == State->BodyCurrent));
				ASSERT_THAT(IsTrue(Publications.PendingColdStart.IsValid()));
				ASSERT_THAT(IsTrue(Publications.LatestSuccessful
					== Publications.PendingColdStart));
				State->Pending = Publications.PendingColdStart;
				UObject* Object = GetOrCreatePIEScriptObject(
					*TestRunner, *State, TEXT("Cache V2 structural defer"));
				ASSERT_THAT(IsNull(FindFProperty<FIntProperty>(
					Object->GetClass(), TEXT("AddedValue"))));
				ASSERT_THAT(IsTrue(InvokeGetValue(
					*TestRunner, *State->Engine, Object,
					730,
					TEXT("Cache V2 structural defer body"))));
				ASSERT_THAT(IsTrue(LogCacheStage(
					*TestRunner, *State->Engine,
					TEXT("StructuralPending"), 730, true)));
			})
			.Then(TEXT("End Cache V2 mutation PIE"), [State]()
			{
				State->PIEObject.Reset();
				AngelscriptPIETestUtils::EndPIE();
			})
			.Until(TEXT("Wait for Cache V2 mutation PIE shutdown"), []()
			{
				return !AngelscriptPIETestUtils::IsPIEWorldAlive();
			}, FTimespan::FromSeconds(DefaultTimeoutSeconds))
			.Then(TEXT("Promote Cache V2 structural edit after PIE"),
				[this, State]()
			{
				State->Engine->CheckForHotReload(ECompileType::FullReload);
				const FAngelscriptCacheLifecyclePublications Publications =
					State->Engine->GetCacheService()->GetLifecyclePublications();
				ASSERT_THAT(IsTrue(Publications.Current.IsValid()));
				ASSERT_THAT(IsFalse(Publications.PendingColdStart.IsValid()));
				ASSERT_THAT(IsTrue(Publications.Current->SourceSnapshot
					== State->Pending->SourceSnapshot));
				ASSERT_THAT(IsTrue(Publications.Current->TransactionOrdinal
					> State->Pending->TransactionOrdinal));

				UClass* PromotedClass =
					FindGeneratedClass(State->Engine.Get(), MutationClassName);
				ASSERT_THAT(IsNotNull(PromotedClass));
				ASSERT_THAT(IsTrue(
					PromotedClass != State->Fixture.ScriptClass));
				ASSERT_THAT(AreEqual(PromotedClass,
					State->Fixture.InitialScriptASClass->GetMostUpToDateClass()));
				State->Fixture.ScriptClass = PromotedClass;
				ASSERT_THAT(IsTrue(RebuildPIERequest(
					*TestRunner, State->Fixture,
					TEXT("Cache V2 promoted structure"))));
				ASSERT_THAT(IsTrue(LogCacheStage(
					*TestRunner, *State->Engine,
					TEXT("StructuralPromoted"), 730, true)));
			});

		QueuePIEStart([State]() { return State->Fixture.RequestParams; });
		TestCommandBuilder
			.Until(TEXT("Wait for Cache V2 promoted PIE world"), []()
			{
				return AngelscriptPIETestUtils::FindPIEWorld() != nullptr;
			}, FTimespan::FromSeconds(DefaultTimeoutSeconds))
			.Then(TEXT("Assert Cache V2 promoted PIE behavior"), [this, State]()
			{
				UObject* Object = GetOrCreatePIEScriptObject(
					*TestRunner, *State, TEXT("Cache V2 promoted PIE"));
				ASSERT_THAT(IsNotNull(FindFProperty<FIntProperty>(
					Object->GetClass(), TEXT("AddedValue"))));
				ASSERT_THAT(IsTrue(InvokeGetValue(
					*TestRunner, *State->Engine, Object,
					730,
					TEXT("Cache V2 promoted PIE"))));
				ASSERT_THAT(IsTrue(LogCacheStage(
					*TestRunner, *State->Engine,
					TEXT("PromotedWarmPIE"), 730, true)));
			})
			.Then(TEXT("End Cache V2 promoted PIE"), [State]()
			{
				State->PIEObject.Reset();
				AngelscriptPIETestUtils::EndPIE();
			})
			.Until(TEXT("Wait for Cache V2 promoted PIE shutdown"), []()
			{
				return !AngelscriptPIETestUtils::IsPIEWorldAlive();
			}, FTimespan::FromSeconds(DefaultTimeoutSeconds));
	}
};

#endif
