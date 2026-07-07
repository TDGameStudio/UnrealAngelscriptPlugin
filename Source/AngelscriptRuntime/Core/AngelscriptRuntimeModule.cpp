#include "AngelscriptRuntimeModule.h"
#include "AngelscriptEngine.h"
#include "AngelscriptEngineSubsystem.h"
#include "AngelscriptSubsystem.h"
#include "Dump/AngelscriptCrashSnapshot.h"
#if WITH_AS_COVERAGE
#include "Extension/CodeCoverage/AngelscriptCodeCoverage.h"
#endif

IMPLEMENT_MODULE(FAngelscriptRuntimeModule, AngelscriptRuntime);

bool FAngelscriptRuntimeModule::bInitializeAngelscriptCalled = false;
TUniquePtr<FAngelscriptEngine> FAngelscriptRuntimeModule::OwnedPrimaryEngine;
#if WITH_DEV_AUTOMATION_TESTS
TFunction<FAngelscriptEngine*()> FAngelscriptRuntimeModule::InitializeOverrideForTesting;
FAngelscriptEngine* FAngelscriptRuntimeModule::InitializedOverrideEngineForTesting = nullptr;
#endif

void FAngelscriptRuntimeModule::StartupModule()
{
	UE_LOG(Angelscript, Verbose, TEXT("[RuntimeStartup] StartupModule."));
	CrashSnapshotExtensionHandle = FAngelscriptCrashSnapshotExtension::Startup();
#if WITH_AS_COVERAGE
	CodeCoverageExtensionHandle = FAngelscriptCodeCoverageExtension::Startup();
#endif
}

void FAngelscriptRuntimeModule::ShutdownModule()
{
	UE_LOG(Angelscript, Verbose, TEXT("[RuntimeStartup] ShutdownModule ownedEngine=%s"),
		OwnedPrimaryEngine.IsValid() ? TEXT("true") : TEXT("false"));
#if WITH_AS_COVERAGE
	FAngelscriptCodeCoverageExtension::Shutdown(CodeCoverageExtensionHandle);
#endif
	FAngelscriptCrashSnapshotExtension::Shutdown(CrashSnapshotExtensionHandle);

	if (OwnedPrimaryEngine.IsValid())
	{
		FAngelscriptEngineContextStack::Pop(OwnedPrimaryEngine.Get());
		OwnedPrimaryEngine.Reset();
	}
}

void FAngelscriptRuntimeModule::InitializeAngelscript()
{
	if (bInitializeAngelscriptCalled)
	{
		UE_LOG(Angelscript, Verbose, TEXT("[RuntimeStartup] InitializeAngelscript skipped because initialization already ran."));
		return;
	}

	bInitializeAngelscriptCalled = true;
	UE_LOG(Angelscript, Display, TEXT("[RuntimeStartup] InitializeAngelscript begin."));
	#if WITH_DEV_AUTOMATION_TESTS
	if (InitializeOverrideForTesting)
	{
		InitializedOverrideEngineForTesting = nullptr;
		if (FAngelscriptEngine* OverrideEngine = InitializeOverrideForTesting())
		{
			FAngelscriptEngineContextStack::Push(OverrideEngine);
			InitializedOverrideEngineForTesting = OverrideEngine;
			UE_LOG(Angelscript, Verbose, TEXT("[RuntimeStartup] InitializeAngelscript used automation override engine=%p."), OverrideEngine);
		}
		return;
	}
	#endif

	FModuleManager::Get().LoadModuleChecked(TEXT("AngelscriptRuntime"));
	if (UAngelscriptEngineSubsystem* EngineSubsystem = UAngelscriptEngineSubsystem::Get())
	{
		UE_LOG(Angelscript, Verbose, TEXT("[RuntimeStartup] Routing InitializeAngelscript to EngineSubsystem=%p."), EngineSubsystem);
		EngineSubsystem->EnsurePrimaryEngineInitialized();
		return;
	}

	if (FAngelscriptEngine* CurrentEngine = FAngelscriptEngine::TryGetCurrentEngine())
	{
		// Adopt an already-initialized ambient engine instead of re-running full initialization.
		if (CurrentEngine->GetScriptEngine() == nullptr)
		{
			UE_LOG(Angelscript, Display, TEXT("[RuntimeStartup] Initializing existing ambient engine=%p."), CurrentEngine);
			CurrentEngine->Initialize();
		}
		else
		{
			UE_LOG(Angelscript, Verbose, TEXT("[RuntimeStartup] Adopted initialized ambient engine=%p."), CurrentEngine);
		}
	}
	else
	{
		OwnedPrimaryEngine = MakeUnique<FAngelscriptEngine>();
		FAngelscriptEngineContextStack::Push(OwnedPrimaryEngine.Get());
		UE_LOG(Angelscript, Display, TEXT("[RuntimeStartup] Created owned primary engine=%p."), OwnedPrimaryEngine.Get());
		OwnedPrimaryEngine->Initialize();
	}
}

#if WITH_DEV_AUTOMATION_TESTS
void FAngelscriptRuntimeModule::SetInitializeOverrideForTesting(TFunction<FAngelscriptEngine*()> InOverride)
{
	InitializeOverrideForTesting = MoveTemp(InOverride);
}

void FAngelscriptRuntimeModule::ResetInitializeStateForTesting()
{
	if (InitializedOverrideEngineForTesting != nullptr)
	{
		FAngelscriptEngineContextStack::Pop(InitializedOverrideEngineForTesting);
		InitializedOverrideEngineForTesting = nullptr;
	}

	if (OwnedPrimaryEngine.IsValid())
	{
		FAngelscriptEngineContextStack::Pop(OwnedPrimaryEngine.Get());
		OwnedPrimaryEngine.Reset();
	}
	bInitializeAngelscriptCalled = false;
	InitializeOverrideForTesting = nullptr;
}
#endif
