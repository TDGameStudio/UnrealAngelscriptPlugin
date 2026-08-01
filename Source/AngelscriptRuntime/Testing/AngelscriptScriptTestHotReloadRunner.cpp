#include "Testing/AngelscriptScriptTestHotReloadRunner.h"

#include "ClassGenerator/ASClass.h"
#include "Core/AngelscriptEngine.h"
#include "HAL/PlatformProcess.h"
#include "Misc/AutomationTest.h"
#include "Testing/AngelscriptScriptTestRunner.h"
#include "Testing/AngelscriptTestSettings.h"
#include "UObject/GarbageCollection.h"
#include "UObject/Package.h"

namespace
{
	class FAngelscriptScriptTestDirectResult final
		: public FAutomationTestBase
	{
	public:
		explicit FAngelscriptScriptTestDirectResult(
			const FString& UniqueName)
			: FAutomationTestBase(UniqueName, false)
		{
		}

		EAutomationTestFlags GetTestFlags() const override
		{
			return EAutomationTestFlags::EditorContext
				| EAutomationTestFlags::EngineFilter;
		}

		FString GetBeautifiedTestName() const override
		{
			return TEXT("Angelscript.ScriptTests.Direct");
		}

		void GetTests(
			TArray<FString>& OutBeautifiedNames,
			TArray<FString>& OutTestCommands) const override
		{
			OutBeautifiedNames.Add(GetBeautifiedTestName());
			OutTestCommands.Add(TEXT(""));
		}

		uint32 GetRequiredDeviceNum() const override
		{
			return 1;
		}

		int32 GetErrorCount() const
		{
			FAutomationTestExecutionInfo Info;
			GetExecutionInfo(Info);
			return Info.GetErrorTotal();
		}

	protected:
		bool RunTest(const FString&) override
		{
			return true;
		}
	};

	TUniquePtr<FAutomationTestBase> MakeResult(
		const TCHAR* Purpose,
		uint64 Generation,
		int32 Serial)
	{
		return MakeUnique<FAngelscriptScriptTestDirectResult>(
			FString::Printf(
				TEXT("FAngelscriptScriptTest%s_%llu_%d_%p"),
				Purpose,
				static_cast<unsigned long long>(Generation),
				Serial,
				&FAutomationTestFramework::Get()));
	}

	int32 CountErrors(const FAutomationTestBase* Result)
	{
		const FAngelscriptScriptTestDirectResult* Direct =
			static_cast<const FAngelscriptScriptTestDirectResult*>(
				Result);
		return Direct != nullptr ? Direct->GetErrorCount() : 0;
	}

	TArray<FAutomationExecutionEntry> GetHotReloadExecutionEntries(
		const FAutomationTestBase& Result)
	{
		FAutomationTestExecutionInfo Info;
		Result.GetExecutionInfo(Info);
		return Info.GetEntries();
	}

	int32 AppendHotReloadExecutionEntries(
		const FAutomationTestBase& Source,
		FAutomationTestBase& Destination,
		int32 FirstEntry = 0)
	{
		const TArray<FAutomationExecutionEntry> Entries =
			GetHotReloadExecutionEntries(Source);
		const int32 ClampedFirst =
			FMath::Clamp(FirstEntry, 0, Entries.Num());
		for (int32 EntryIndex = ClampedFirst;
			EntryIndex < Entries.Num();
			++EntryIndex)
		{
			Destination.AddEvent(Entries[EntryIndex].Event);
		}
		return Entries.Num() - ClampedFirst;
	}

	int32 LogHotReloadExecutionEntries(
		const FAutomationTestBase& Source,
		int32 FirstEntry)
	{
		const TArray<FAutomationExecutionEntry> Entries =
			GetHotReloadExecutionEntries(Source);
		const int32 ClampedFirst =
			FMath::Clamp(FirstEntry, 0, Entries.Num());
		for (int32 EntryIndex = ClampedFirst;
			EntryIndex < Entries.Num();
			++EntryIndex)
		{
			const FAutomationEvent& Event =
				Entries[EntryIndex].Event;
			switch (Event.Type)
			{
			case EAutomationEventType::Error:
				UE_LOG(Angelscript, Error, TEXT("%s"), *Event.Message);
				break;
			case EAutomationEventType::Warning:
				UE_LOG(Angelscript, Warning, TEXT("%s"), *Event.Message);
				break;
			default:
				UE_LOG(Angelscript, Display, TEXT("%s"), *Event.Message);
				break;
			}
		}
		return Entries.Num() - ClampedFirst;
	}

	bool IsEligibleForContext(
		EAutomationTestFlags Flags,
		EAutomationTestFlags Context)
	{
		return EnumHasAnyFlags(Flags, Context)
			&& !EnumHasAnyFlags(Flags, EAutomationTestFlags::Disabled);
	}
}

struct FAngelscriptScriptTestHotReloadRunner::FSuiteSession
{
	FAngelscriptScriptTestId Id;
	uint64 Generation = 0;
	TStrongObjectPtr<UAngelscriptTestSuite> Instance;
	TUniquePtr<FAutomationTestBase> LifecycleResult;
	bool bSetupFailed = false;
};

FAngelscriptScriptTestHotReloadRunner::
	FAngelscriptScriptTestHotReloadRunner() = default;

FAngelscriptScriptTestHotReloadRunner::
	~FAngelscriptScriptTestHotReloadRunner()
{
	CancelOlderWork();
}

bool FAngelscriptScriptTestHotReloadRunner::EnsureSuiteSession(
	const FAngelscriptScriptTestId& Id,
	uint64 Generation,
	FAutomationTestBase& Result)
{
	if (SuiteSession.IsValid()
		&& SuiteSession->Generation == Generation
		&& SuiteSession->Id.ModuleName == Id.ModuleName
		&& SuiteSession->Id.SuiteName == Id.SuiteName)
	{
		return !SuiteSession->bSetupFailed;
	}
	CloseSuiteSession();

	FAngelscriptEngine* Engine = FAngelscriptEngine::TryGetCurrentEngine();
	const TSharedPtr<FAngelscriptModuleDesc> Module =
		Engine != nullptr ? Engine->GetModule(Id.ModuleName) : nullptr;
	const TSharedPtr<FAngelscriptClassDesc> ClassDesc =
		Module.IsValid() ? Module->GetClass(Id.SuiteName) : nullptr;
	if (!ClassDesc.IsValid() || ClassDesc->Class == nullptr)
	{
		Result.AddError(FString::Printf(
			TEXT("Cannot open reflected script test suite session '%s::%s'."),
			*Id.ModuleName,
			*Id.SuiteName));
		return false;
	}

	UClass* SuiteClass = ClassDesc->Class;
	if (UASClass* ScriptClass = Cast<UASClass>(SuiteClass))
	{
		SuiteClass = ScriptClass->GetMostUpToDateClass();
	}
	if (SuiteClass == nullptr
		|| !SuiteClass->IsChildOf(UAngelscriptTestSuite::StaticClass())
		|| SuiteClass->HasAnyClassFlags(
			CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists))
	{
		Result.AddError(FString::Printf(
			TEXT("Reflected script test suite '%s::%s' is not a current concrete class."),
			*Id.ModuleName,
			*Id.SuiteName));
		return false;
	}

	TUniquePtr<FSuiteSession> NewSession = MakeUnique<FSuiteSession>();
	NewSession->Id = Id;
	NewSession->Id.MethodName.Reset();
	NewSession->Generation = Generation;
	NewSession->Instance.Reset(
		NewObject<UAngelscriptTestSuite>(
			GetTransientPackage(),
			SuiteClass,
			NAME_None,
			RF_Transient));
	NewSession->LifecycleResult = MakeResult(
		TEXT("Lifecycle"),
		Generation,
		0);
	if (NewSession->Instance == nullptr)
	{
		Result.AddError(TEXT(
			"Failed to construct the reflected script test suite session."));
		bAllPassed = false;
		return false;
	}

	NewSession->bSetupFailed =
		!FAngelscriptScriptTestRunner::InvokeSuiteLifecycle(
			NewSession->Instance.Get(),
			TEXT("BeforeAll"),
			EAngelscriptScriptTestPhase::BeforeAll,
			*NewSession->LifecycleResult);
	if (NewSession->bSetupFailed)
	{
		const int32 AppendedEntries =
			AppendHotReloadExecutionEntries(
				*NewSession->LifecycleResult,
				Result);
		if (AppendedEntries == 0)
		{
			Result.AddError(FString::Printf(
				TEXT("BeforeAll failed for reflected script test suite "
					"'%s::%s'; the leaf was not executed."),
				*Id.ModuleName,
				*Id.SuiteName));
		}
		bAllPassed = false;
	}
	SuiteSession = MoveTemp(NewSession);
	return !SuiteSession->bSetupFailed;
}

void FAngelscriptScriptTestHotReloadRunner::CloseSuiteSession()
{
	if (!SuiteSession.IsValid())
	{
		return;
	}
	if (SuiteSession->Instance != nullptr)
	{
		const int32 EntriesBefore =
			SuiteSession->LifecycleResult.IsValid()
				? GetHotReloadExecutionEntries(
					*SuiteSession->LifecycleResult).Num()
				: 0;
		const bool bAfterAllPassed =
			SuiteSession->LifecycleResult.IsValid()
			&& FAngelscriptScriptTestRunner::InvokeSuiteLifecycle(
				SuiteSession->Instance.Get(),
				TEXT("AfterAll"),
				EAngelscriptScriptTestPhase::AfterAll,
				*SuiteSession->LifecycleResult);
#if WITH_DEV_AUTOMATION_TESTS
		if (SuiteSessionClosedObserverForTesting)
		{
			SuiteSessionClosedObserverForTesting(bAfterAllPassed);
		}
#endif
		if (!bAfterAllPassed)
		{
			const int32 LoggedEntries =
				SuiteSession->LifecycleResult.IsValid()
					? LogHotReloadExecutionEntries(
						*SuiteSession->LifecycleResult,
						EntriesBefore)
					: 0;
			if (LoggedEntries == 0)
			{
				UE_LOG(
					Angelscript,
					Error,
					TEXT("Reflected script test AfterAll failed for %s::%s."),
					*SuiteSession->Id.ModuleName,
					*SuiteSession->Id.SuiteName);
			}
			bAllPassed = false;
		}
		SuiteSession->Instance.Reset();
	}
	SuiteSession->LifecycleResult.Reset();
	SuiteSession.Reset();
}

void FAngelscriptScriptTestHotReloadRunner::CancelOlderWork()
{
	if (ActiveContext.IsValid() && !ActiveContext->IsComplete())
	{
		ActiveContext->Cancel(TEXT(
			"a newer successful script hot reload replaced automatic test work"));
	}
	ActiveContext.Reset();
	ActiveResult.Reset();
	PendingTests.Reset();
	CloseSuiteSession();
	bRunInProgress = false;
	bAllPassed = true;
}

void FAngelscriptScriptTestHotReloadRunner::
	CancelModulesBeforeReload(
		const TSet<FString>& ModuleNames)
{
	if (ActiveContext.IsValid()
		&& ModuleNames.Contains(
			ActiveContext->GetDescriptor().Id.ModuleName))
	{
		ActiveContext->Cancel(TEXT(
			"the owning AngelScript module is being hot reloaded"));
		ActiveContext.Reset();
		ActiveResult.Reset();
	}
	PendingTests.RemoveAll(
		[&ModuleNames](const FAngelscriptScriptTestId& Id)
		{
			return ModuleNames.Contains(Id.ModuleName);
		});
	if (SuiteSession.IsValid()
		&& ModuleNames.Contains(SuiteSession->Id.ModuleName))
	{
		CloseSuiteSession();
	}
	if (!ActiveContext.IsValid() && PendingTests.IsEmpty())
	{
		bRunInProgress = false;
		bAllPassed = true;
	}
}

void FAngelscriptScriptTestHotReloadRunner::PrepareTests(
	const TArray<TSharedRef<FAngelscriptModuleDesc>>&
		CompiledModules)
{
	CancelOlderWork();
	bAllPassed = true;
	if (!ShouldRunTestsOnHotReload())
	{
		return;
	}

	const TSharedPtr<const FAngelscriptScriptTestRegistrySnapshot> Snapshot =
		FAngelscriptScriptTestRegistry::Get().GetSnapshot();
	if (!Snapshot.IsValid())
	{
		return;
	}
	PreparedGeneration = Snapshot->Generation;

	TSet<FString> AffectedModules;
	for (const TSharedRef<FAngelscriptModuleDesc>& Module :
		CompiledModules)
	{
		AffectedModules.Add(Module->ModuleName);
	}

	const int32 ModuleLimit =
		GetDefault<UAngelscriptTestUserSettings>()
			->LimitNModulesToTestOnHotReload;
	TSet<FString> SelectedModules;
	for (const FAngelscriptScriptTestDescriptor& Descriptor :
		Snapshot->Tests)
	{
		if (!IsEligibleForContext(
			Descriptor.Flags,
			EAutomationTestFlags::EditorContext))
		{
			continue;
		}
		if (!AffectedModules.Contains(Descriptor.Id.ModuleName))
		{
			continue;
		}
		if (!SelectedModules.Contains(Descriptor.Id.ModuleName))
		{
			if (ModuleLimit > 0
				&& SelectedModules.Num() >= ModuleLimit)
			{
				continue;
			}
			SelectedModules.Add(Descriptor.Id.ModuleName);
		}
		PendingTests.Add(Descriptor.Id);
	}
	bRunInProgress = !PendingTests.IsEmpty();
}

void FAngelscriptScriptTestHotReloadRunner::CompleteActive()
{
	if (ActiveResult.IsValid())
	{
		const int32 ErrorCount = CountErrors(ActiveResult.Get());
		bAllPassed &= ErrorCount == 0;
		if (ErrorCount != 0)
		{
			const int32 LoggedEntries =
				LogHotReloadExecutionEntries(
					*ActiveResult,
					0);
			if (LoggedEntries == 0)
			{
				UE_LOG(
					Angelscript,
					Error,
					TEXT("Automatic reflected script test failed: %s"),
					ActiveContext.IsValid()
						? *ActiveContext->GetDescriptor().DisplayName
						: TEXT("<unknown reflected script test>"));
			}
		}
	}
	ActiveContext.Reset();
	ActiveResult.Reset();

	const int32 GarbageCollectionInterval =
		GetDefault<UAngelscriptTestSettings>()
			->GarbageCollectEveryNTests;
	if (GarbageCollectionInterval > 0
		&& ++CompletedSinceGarbageCollection
			>= GarbageCollectionInterval)
	{
		CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
		CompletedSinceGarbageCollection = 0;
	}
}

bool FAngelscriptScriptTestHotReloadRunner::CompletePreparedRun()
{
	if (!bRunInProgress)
	{
		return true;
	}

	CloseSuiteSession();
	bRunInProgress = false;
	const bool bPassed = bAllPassed;
	// A completed run is a consumable result, not persistent runner state.
	// Future idle ticks remain successful until PrepareTests starts new work.
	bAllPassed = true;
	return bPassed;
}

bool FAngelscriptScriptTestHotReloadRunner::RunTests(
	FAngelscriptEngine* Engine)
{
	if (Engine == nullptr || !ShouldRunTestsOnHotReload())
	{
		return true;
	}

	if (ActiveContext.IsValid())
	{
		if (!ActiveContext->IsComplete())
		{
			ActiveContext->Update();
		}
		if (ActiveContext->IsComplete())
		{
			CompleteActive();
		}
		return !ActiveContext.IsValid() && PendingTests.IsEmpty()
			? CompletePreparedRun()
			: true;
	}

	if (PendingTests.IsEmpty())
	{
		return CompletePreparedRun();
	}

	const FAngelscriptScriptTestId Id = PendingTests[0];
	PendingTests.RemoveAt(0, 1, EAllowShrinking::No);
	ActiveResult = MakeResult(
		TEXT("HotReload"),
		PreparedGeneration,
		PendingTests.Num());
	if (!EnsureSuiteSession(
		Id,
		PreparedGeneration,
		*ActiveResult))
	{
		bAllPassed = false;
		CompleteActive();
		return PendingTests.IsEmpty()
			? CompletePreparedRun()
			: true;
	}
	ActiveContext = FAngelscriptScriptTestRunner::Start(
		Id,
		*ActiveResult,
		false);
	if (!ActiveContext.IsValid() || ActiveContext->IsComplete())
	{
		CompleteActive();
	}
	return !ActiveContext.IsValid() && PendingTests.IsEmpty()
		? CompletePreparedRun()
		: true;
}

bool FAngelscriptScriptTestHotReloadRunner::
	ShouldRunTestsOnHotReload() const
{
	return GetDefault<UAngelscriptTestUserSettings>()
		->bRunUnitTestsOnHotReload;
}

bool FAngelscriptScriptTestHotReloadRunner::
	RunCurrentRegistrySynchronously(
		FAngelscriptEngine& Engine,
		FAngelscriptScriptTestRunSummary* OutSummary)
{
	(void)Engine;
	FAngelscriptScriptTestRunSummary Summary;
	const TSharedPtr<const FAngelscriptScriptTestRegistrySnapshot> Snapshot =
		FAngelscriptScriptTestRegistry::Get().GetSnapshot();
	if (!Snapshot.IsValid())
	{
		if (OutSummary != nullptr)
		{
			*OutSummary = Summary;
		}
		return false;
	}

	bAllPassed = true;
	bool bRegistryPassed = true;
	int32 Serial = 0;
	for (const FAngelscriptScriptTestDescriptor& Descriptor :
		Snapshot->Tests)
	{
		if (!IsEligibleForContext(
			Descriptor.Flags,
			EAutomationTestFlags::CommandletContext))
		{
			continue;
		}
		++Summary.Selected;
		TUniquePtr<FAutomationTestBase> Result = MakeResult(
			TEXT("Commandlet"),
			Snapshot->Generation,
			Serial++);
		if (!EnsureSuiteSession(
			Descriptor.Id,
			Snapshot->Generation,
			*Result))
		{
			bRegistryPassed = false;
			if (LogHotReloadExecutionEntries(*Result, 0) == 0)
			{
				UE_LOG(
					Angelscript,
					Error,
					TEXT("Failed to prepare reflected script test: %s"),
					*Descriptor.DisplayName);
			}
			continue;
		}
		const TSharedPtr<FAngelscriptScriptTestExecutionContext> Context =
			FAngelscriptScriptTestRunner::Start(
				Descriptor.Id,
				*Result,
				false);
		if (!Context.IsValid())
		{
			bRegistryPassed = false;
			if (LogHotReloadExecutionEntries(*Result, 0) == 0)
			{
				UE_LOG(
					Angelscript,
					Error,
					TEXT("Failed to start reflected script test: %s"),
					*Descriptor.DisplayName);
			}
			continue;
		}
		++Summary.Executed;

		while (!Context->IsComplete())
		{
			Context->Update();
			// WaitDelay is monotonic and non-blocking in Automation. A
			// commandlet has no Automation update loop, so yield briefly
			// between direct scheduler updates.
			FPlatformProcess::SleepNoStats(0.001f);
		}
		const bool bLeafPassed =
			CountErrors(Result.Get()) == 0
			&& !Context->HasFailed();
		bRegistryPassed &= bLeafPassed;
		if (bLeafPassed)
		{
			++Summary.Passed;
		}
		else
		{
			++Summary.Failed;
			if (LogHotReloadExecutionEntries(*Result, 0) == 0)
			{
				UE_LOG(
					Angelscript,
					Error,
					TEXT("Reflected script test failed without a diagnostic: %s"),
					*Descriptor.DisplayName);
			}
		}
	}
	CloseSuiteSession();
	if (!bAllPassed
		&& Summary.Failed == 0
		&& Summary.Passed > 0)
	{
		// A suite-level AfterAll failure is terminal even though it occurs
		// after the final leaf body completed. Keep the four summary counts
		// internally consistent by attributing that session failure to one
		// otherwise-passing executed leaf.
		--Summary.Passed;
		++Summary.Failed;
	}
	const bool bPassed =
		Summary.IsSuccessful()
		&& bRegistryPassed
		&& bAllPassed;
	if (OutSummary != nullptr)
	{
		*OutSummary = Summary;
	}
	return bPassed;
}

bool RunAngelscriptScriptTests(FAngelscriptEngine& Engine)
{
	FAngelscriptScriptTestHotReloadRunner DirectRunner;
	FAngelscriptScriptTestRunSummary Summary;
	const bool bPassed =
		DirectRunner.RunCurrentRegistrySynchronously(
			Engine,
			&Summary);
	UE_LOG(
		Angelscript,
		Display,
		TEXT("Reflected script-test commandlet summary: "
			"selected=%d executed=%d passed=%d failed=%d."),
		Summary.Selected,
		Summary.Executed,
		Summary.Passed,
		Summary.Failed);
	if (Summary.Selected == 0)
	{
		UE_LOG(
			Angelscript,
			Error,
			TEXT("No eligible reflected AngelScript tests were selected "
				"for CommandletContext."));
	}
	return bPassed;
}
