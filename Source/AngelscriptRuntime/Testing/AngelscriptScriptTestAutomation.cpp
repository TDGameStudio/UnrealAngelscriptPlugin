#include "Testing/AngelscriptScriptTestAutomation.h"

#include "ClassGenerator/ASClass.h"
#include "Core/AngelscriptEngine.h"
#include "Testing/AngelscriptScriptTestRunner.h"
#include "Testing/AngelscriptTestSuite.h"

#include "Algo/Sort.h"
#include "UObject/Package.h"

namespace
{
	const FString PublicRoot(TEXT("Angelscript.ScriptTests"));

	class FAngelscriptScriptTestLifecycleResult final
		: public FAutomationTestBase
	{
	public:
		explicit FAngelscriptScriptTestLifecycleResult(
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
			return TEXT("Angelscript.ScriptTests.Lifecycle");
		}

		uint32 GetRequiredDeviceNum() const override
		{
			return 1;
		}

	protected:
		void GetTests(
			TArray<FString>& OutBeautifiedNames,
			TArray<FString>& OutTestCommands) const override
		{
			OutBeautifiedNames.Add(GetBeautifiedTestName());
			OutTestCommands.Add(TEXT(""));
		}

		bool RunTest(const FString&) override
		{
			return true;
		}
	};

	uint64 ToMask(EAutomationTestFlags Flags)
	{
		return static_cast<uint64>(Flags);
	}

	FString MakeSection(
		const FAngelscriptScriptTestDescriptor& Descriptor)
	{
		return FString::Printf(
			TEXT("%s.%s.%s"),
			*PublicRoot,
			*Descriptor.Id.ModuleName,
			*Descriptor.Id.SuiteName);
	}

	bool ParseCommandFromCompleteName(
		const FString& CompleteName,
		FAngelscriptScriptTestId& OutId,
		uint64& OutGeneration)
	{
		int32 Separator = INDEX_NONE;
		if (!CompleteName.FindLastChar(TEXT(' '), Separator))
		{
			return false;
		}
		return FAngelscriptScriptTestId::TryParseCommandString(
			CompleteName.Mid(Separator + 1),
			OutId,
			OutGeneration);
	}

	TArray<FAutomationExecutionEntry> GetAutomationExecutionEntries(
		const FAutomationTestBase& Result)
	{
		FAutomationTestExecutionInfo Info;
		Result.GetExecutionInfo(Info);
		return Info.GetEntries();
	}

	int32 AppendAutomationExecutionEntries(
		const FAutomationTestBase& Source,
		FAutomationTestBase& Destination,
		int32 FirstEntry = 0)
	{
		const TArray<FAutomationExecutionEntry> Entries =
			GetAutomationExecutionEntries(Source);
		for (int32 EntryIndex =
				FMath::Clamp(FirstEntry, 0, Entries.Num());
			EntryIndex < Entries.Num();
			++EntryIndex)
		{
			Destination.AddEvent(Entries[EntryIndex].Event);
		}
		return Entries.Num() - FMath::Clamp(
			FirstEntry,
			0,
			Entries.Num());
	}

	int32 LogAutomationExecutionEntries(
		const FAutomationTestBase& Source,
		int32 FirstEntry)
	{
		const TArray<FAutomationExecutionEntry> Entries =
			GetAutomationExecutionEntries(Source);
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
}

class FAngelscriptScriptTestAutomation::FBridge final
	: public FAutomationTestBase
{
public:
	explicit FBridge(EAutomationTestFlags InFlags)
		: FAutomationTestBase(
			FString::Printf(
				TEXT("FAngelscriptScriptTests_%016llX"),
				static_cast<unsigned long long>(ToMask(InFlags))),
			true)
		, Flags(InFlags)
	{
	}

	EAutomationTestFlags GetTestFlags() const override
	{
		return Flags;
	}

	FString GetBeautifiedTestName() const override
	{
		return PublicRoot;
	}

	uint32 GetRequiredDeviceNum() const override
	{
		return 1;
	}

	FString GetTestSourceFileName(
		const FString& InTestName) const override
	{
		FAngelscriptScriptTestId Id;
		uint64 Generation = 0;
		if (!ParseCommandFromCompleteName(
			InTestName,
			Id,
			Generation))
		{
			return FString();
		}
		const TSharedPtr<const FAngelscriptScriptTestRegistrySnapshot> Snapshot =
			FAngelscriptScriptTestRegistry::Get().GetSnapshot();
		if (const FAngelscriptScriptTestDescriptor* Descriptor =
			Snapshot.IsValid() ? Snapshot->Find(Id) : nullptr)
		{
			return Descriptor->SourceFile;
		}
		return FString();
	}

	int32 GetTestSourceFileLine(
		const FString& InTestName) const override
	{
		FAngelscriptScriptTestId Id;
		uint64 Generation = 0;
		if (!ParseCommandFromCompleteName(
			InTestName,
			Id,
			Generation))
		{
			return 0;
		}
		const TSharedPtr<const FAngelscriptScriptTestRegistrySnapshot> Snapshot =
			FAngelscriptScriptTestRegistry::Get().GetSnapshot();
		if (const FAngelscriptScriptTestDescriptor* Descriptor =
			Snapshot.IsValid() ? Snapshot->Find(Id) : nullptr)
		{
			return Descriptor->SourceLine;
		}
		return 0;
	}

	void Enumerate(
		TArray<FString>& OutBeautifiedNames,
		TArray<FString>& OutCommands) const
	{
		GetTests(OutBeautifiedNames, OutCommands);
	}

#if WITH_DEV_AUTOMATION_TESTS
	bool ExecuteForTesting(
		const FString& Command,
		TArray<FAutomationExecutionEntry>& OutEntries)
	{
		ClearExecutionInfo();
		const bool bResult = RunTest(Command);
		FAutomationTestExecutionInfo Info;
		GetExecutionInfo(Info);
		OutEntries = Info.GetEntries();
		return bResult;
	}
#endif

protected:
	void GetTests(
		TArray<FString>& OutBeautifiedNames,
		TArray<FString>& OutTestCommands) const override
	{
		const TSharedPtr<const FAngelscriptScriptTestRegistrySnapshot> Snapshot =
			FAngelscriptScriptTestRegistry::Get().GetSnapshot();
		if (!Snapshot.IsValid())
		{
			return;
		}

		for (const FAngelscriptScriptTestDescriptor& Descriptor :
			Snapshot->Tests)
		{
			if (Descriptor.Flags != Flags)
			{
				continue;
			}
			const FString Prefix = PublicRoot + TEXT(".");
			OutBeautifiedNames.Add(
				Descriptor.DisplayName.StartsWith(Prefix)
					? Descriptor.DisplayName.Mid(Prefix.Len())
					: Descriptor.DisplayName);
			OutTestCommands.Add(
				Descriptor.Id.ToCommandString(
					Descriptor.Generation));
		}
	}

	bool RunTest(const FString& Parameters) override
	{
		FAngelscriptScriptTestId Id;
		uint64 RequestedGeneration = 0;
		if (!FAngelscriptScriptTestId::TryParseCommandString(
			Parameters,
			Id,
			RequestedGeneration))
		{
			AddError(TEXT("Malformed reflected script-test command."));
			return false;
		}

		const TSharedPtr<const FAngelscriptScriptTestRegistrySnapshot> Snapshot =
			FAngelscriptScriptTestRegistry::Get().GetSnapshot();
		const FAngelscriptScriptTestDescriptor* CurrentDescriptor =
			Snapshot.IsValid() ? Snapshot->Find(Id) : nullptr;
		if (CurrentDescriptor == nullptr)
		{
			AddError(TEXT(
				"The cached script-test leaf was removed or renamed. "
				"Refresh the Automation test list and rerun."));
			return false;
		}
		if (CurrentDescriptor->Flags != Flags)
		{
			AddError(TEXT(
				"The cached script-test leaf moved to a different flag bucket. "
				"Refresh the Automation test list and rerun."));
			return false;
		}

		// UE only broadcasts a section enter when the full test path crosses
		// a section boundary. A reload can replace this suite generation
		// between two leaves under the same cached section, so every leaf also
		// validates and lazily reopens its current-generation All-hook session.
		FAngelscriptScriptTestAutomation::Get().EnterSection(
			MakeSection(*CurrentDescriptor));
		if (FAngelscriptScriptTestAutomation::Get().IsSuiteSetupFailed(
			Id,
			CurrentDescriptor->Generation))
		{
			FAngelscriptScriptTestAutomation::Get()
				.AppendSuiteSetupDiagnostics(*this);
			return false;
		}

		return FAngelscriptScriptTestRunner::Run(Id, *this);
	}

private:
	EAutomationTestFlags Flags;
};

FAngelscriptScriptTestAutomation&
FAngelscriptScriptTestAutomation::Get()
{
	static FAngelscriptScriptTestAutomation Instance;
	return Instance;
}

void FAngelscriptScriptTestAutomation::Startup()
{
	bStarted = true;
	if (const TSharedPtr<const FAngelscriptScriptTestRegistrySnapshot> Snapshot =
		FAngelscriptScriptTestRegistry::Get().GetSnapshot())
	{
		EnsureBridges(*Snapshot);
	}
}

void FAngelscriptScriptTestAutomation::Shutdown()
{
	CloseActiveSection();
	FAutomationTestFramework& Framework =
		FAutomationTestFramework::Get();
	for (const TPair<FString, FSectionBinding>& Pair :
		SectionBindings)
	{
		Framework.GetOnEnteringTestSection(Pair.Key)
			.Remove(Pair.Value.EnterHandle);
		Framework.GetOnLeavingTestSection(Pair.Key)
			.Remove(Pair.Value.LeaveHandle);
	}
	SectionBindings.Reset();
	Bridges.Reset();
	bStarted = false;
}

void FAngelscriptScriptTestAutomation::EnsureBridges(
	const FAngelscriptScriptTestRegistrySnapshot& Snapshot)
{
	if (!bStarted)
	{
		bStarted = true;
	}
	for (const FAngelscriptScriptTestDescriptor& Descriptor :
		Snapshot.Tests)
	{
		const uint64 Mask = ToMask(Descriptor.Flags);
		if (!Bridges.Contains(Mask))
		{
			Bridges.Add(
				Mask,
				MakeShared<FBridge>(Descriptor.Flags));
		}
	}
	EnsureSectionBindings(Snapshot);
}

void FAngelscriptScriptTestAutomation::CancelModulesBeforeReload(
	const TSet<FString>& ModuleNames)
{
	if (ActiveSession.IsSet()
		&& ModuleNames.Contains(ActiveSession->Id.ModuleName))
	{
		// This method is called while the old script module is still valid.
		// Closing here guarantees old-generation AfterAll runs before class
		// replacement and releases the strong suite instance immediately.
		CloseActiveSection();
	}
}

void FAngelscriptScriptTestAutomation::CancelEngineBeforeShutdown(
	FAngelscriptEngine* Engine)
{
	check(IsInGameThread());
	if (Engine != nullptr
		&& ActiveSession.IsSet()
		&& ActiveSession->OwningEngine == Engine)
	{
		// Shutdown calls this while the engine and its script functions are
		// still valid, so the suite can run AfterAll safely.
		CloseActiveSection();
	}
}

int32 FAngelscriptScriptTestAutomation::GetBridgeCount() const
{
	return Bridges.Num();
}

bool FAngelscriptScriptTestAutomation::HasBridge(
	EAutomationTestFlags Flags) const
{
	return Bridges.Contains(ToMask(Flags));
}

void FAngelscriptScriptTestAutomation::GetLeavesForMask(
	EAutomationTestFlags Flags,
	TArray<FString>& OutBeautifiedNames,
	TArray<FString>& OutCommands) const
{
	if (const TSharedPtr<FBridge>* Bridge =
		Bridges.Find(ToMask(Flags)))
	{
		(*Bridge)->Enumerate(
			OutBeautifiedNames,
			OutCommands);
	}
}

#if WITH_DEV_AUTOMATION_TESTS
bool FAngelscriptScriptTestAutomation::ExecuteBridgeCommandForTesting(
	EAutomationTestFlags Flags,
	const FString& Command,
	TArray<FAutomationExecutionEntry>& OutEntries)
{
	if (const TSharedPtr<FBridge>* Bridge =
		Bridges.Find(ToMask(Flags)))
	{
		return (*Bridge)->ExecuteForTesting(
			Command,
			OutEntries);
	}
	OutEntries.Reset();
	return false;
}

void FAngelscriptScriptTestAutomation::EnterSectionForTesting(
	const FString& Section)
{
	EnterSection(Section);
}

void FAngelscriptScriptTestAutomation::LeaveSectionForTesting(
	const FString& Section)
{
	LeaveSection(Section);
}

bool FAngelscriptScriptTestAutomation::HasActiveSessionForTesting() const
{
	return ActiveSession.IsSet();
}

uint64
FAngelscriptScriptTestAutomation::GetActiveSessionGenerationForTesting() const
{
	return ActiveSession.IsSet()
		? ActiveSession->Generation
		: 0;
}
#endif

void FAngelscriptScriptTestAutomation::EnsureSectionBindings(
	const FAngelscriptScriptTestRegistrySnapshot& Snapshot)
{
	FAutomationTestFramework& Framework =
		FAutomationTestFramework::Get();
	for (const FAngelscriptScriptTestDescriptor& Descriptor :
		Snapshot.Tests)
	{
		const FString Section = MakeSection(Descriptor);
		if (SectionBindings.Contains(Section))
		{
			continue;
		}

		FSectionBinding Binding;
		Binding.Section = Section;
		Binding.EnterHandle =
			Framework.GetOnEnteringTestSection(Section).AddLambda(
				[this, Section](const FString&)
				{
					EnterSection(Section);
				});
		Binding.LeaveHandle =
			Framework.GetOnLeavingTestSection(Section).AddLambda(
				[this, Section](const FString&)
				{
					LeaveSection(Section);
				});
		SectionBindings.Add(Section, MoveTemp(Binding));
	}
}

void FAngelscriptScriptTestAutomation::EnterSection(
	const FString& Section)
{
	const TSharedPtr<const FAngelscriptScriptTestRegistrySnapshot> Snapshot =
		FAngelscriptScriptTestRegistry::Get().GetSnapshot();
	if (!Snapshot.IsValid())
	{
		return;
	}
	const FAngelscriptScriptTestDescriptor* Descriptor =
		Snapshot->Tests.FindByPredicate(
			[&Section](
				const FAngelscriptScriptTestDescriptor& Candidate)
			{
				return MakeSection(Candidate) == Section;
			});
	if (Descriptor == nullptr)
	{
		return;
	}

	if (ActiveSession.IsSet())
	{
		const FString ActiveSection = FString::Printf(
			TEXT("%s.%s.%s"),
			*PublicRoot,
			*ActiveSession->Id.ModuleName,
			*ActiveSession->Id.SuiteName);
		if (ActiveSection == Section
			&& ActiveSession->Generation == Snapshot->Generation)
		{
			return;
		}
		// UE section matching uses StartsWith and can miss Suite -> SuiteExtra
		// leave notifications. A different suite or registry generation closes
		// the old session proactively.
		CloseActiveSection();
	}

	FAngelscriptEngine* Engine =
		FAngelscriptEngine::TryGetCurrentEngine();
	const TSharedPtr<FAngelscriptModuleDesc> Module =
		Engine != nullptr
			? Engine->GetModule(Descriptor->Id.ModuleName)
			: nullptr;
	const TSharedPtr<FAngelscriptClassDesc> ClassDesc =
		Module.IsValid()
			? Module->GetClass(Descriptor->Id.SuiteName)
			: nullptr;
	if (!ClassDesc.IsValid() || ClassDesc->Class == nullptr)
	{
		return;
	}
	UClass* SuiteClass = ClassDesc->Class;
	if (UASClass* ScriptClass = Cast<UASClass>(SuiteClass))
	{
		SuiteClass = ScriptClass->GetMostUpToDateClass();
	}
	if (SuiteClass == nullptr)
	{
		return;
	}

	FSuiteSession Session;
	Session.Id = Descriptor->Id;
	Session.Id.MethodName.Reset();
	Session.Generation = Snapshot->Generation;
	Session.OwningEngine = Engine;
	Session.Instance.Reset(
		NewObject<UAngelscriptTestSuite>(
			GetTransientPackage(),
			SuiteClass,
			NAME_None,
			RF_Transient));
	Session.LifecycleResult =
		MakeUnique<FAngelscriptScriptTestLifecycleResult>(
			FString::Printf(
				TEXT("FAngelscriptScriptTestAutomationLifecycle_%llu_%s_%s_%p"),
				static_cast<unsigned long long>(Snapshot->Generation),
				*Descriptor->Id.ModuleName,
				*Descriptor->Id.SuiteName,
				this));
	if (Session.Instance != nullptr)
	{
		Session.bSetupFailed =
			!FAngelscriptScriptTestRunner::InvokeSuiteLifecycle(
				Session.Instance.Get(),
				TEXT("BeforeAll"),
				EAngelscriptScriptTestPhase::BeforeAll,
				*Session.LifecycleResult);
	}
	else
	{
		Session.bSetupFailed = true;
	}
	ActiveSession = MoveTemp(Session);
}

void FAngelscriptScriptTestAutomation::LeaveSection(
	const FString& Section)
{
	if (!ActiveSession.IsSet())
	{
		return;
	}
	const FString ActiveSection = FString::Printf(
		TEXT("%s.%s.%s"),
		*PublicRoot,
		*ActiveSession->Id.ModuleName,
		*ActiveSession->Id.SuiteName);
	if (ActiveSection == Section)
	{
		CloseActiveSection();
	}
}

void FAngelscriptScriptTestAutomation::CloseActiveSection()
{
	if (!ActiveSession.IsSet())
	{
		return;
	}
	if (ActiveSession->Instance != nullptr)
	{
		const int32 EntriesBefore =
			ActiveSession->LifecycleResult.IsValid()
				? GetAutomationExecutionEntries(
					*ActiveSession->LifecycleResult).Num()
				: 0;
		const bool bAfterAllPassed =
			ActiveSession->LifecycleResult.IsValid()
			&& FAngelscriptScriptTestRunner::InvokeSuiteLifecycle(
				ActiveSession->Instance.Get(),
				TEXT("AfterAll"),
				EAngelscriptScriptTestPhase::AfterAll,
				*ActiveSession->LifecycleResult);
		if (!bAfterAllPassed)
		{
			const int32 LoggedEntries =
				ActiveSession->LifecycleResult.IsValid()
					? LogAutomationExecutionEntries(
						*ActiveSession->LifecycleResult,
						EntriesBefore)
					: 0;
			if (LoggedEntries == 0)
			{
				UE_LOG(
					Angelscript,
					Error,
					TEXT("Reflected script test AfterAll failed for %s::%s."),
					*ActiveSession->Id.ModuleName,
					*ActiveSession->Id.SuiteName);
			}
		}
		ActiveSession->Instance.Reset();
	}
	ActiveSession->LifecycleResult.Reset();
	ActiveSession.Reset();
}

void FAngelscriptScriptTestAutomation::AppendSuiteSetupDiagnostics(
	FAutomationTestBase& Result) const
{
	const int32 AppendedEntries =
		ActiveSession.IsSet()
		&& ActiveSession->LifecycleResult.IsValid()
			? AppendAutomationExecutionEntries(
				*ActiveSession->LifecycleResult,
				Result)
			: 0;
	if (AppendedEntries == 0)
	{
		Result.AddError(TEXT(
			"The suite's BeforeAll hook failed for this worker and "
			"script generation; the leaf was not executed and AfterAll "
			"will still be attempted."));
	}
}

bool FAngelscriptScriptTestAutomation::IsSuiteSetupFailed(
	const FAngelscriptScriptTestId& Id,
	uint64 Generation) const
{
	return ActiveSession.IsSet()
		&& ActiveSession->Generation == Generation
		&& ActiveSession->Id.ModuleName == Id.ModuleName
		&& ActiveSession->Id.SuiteName == Id.SuiteName
		&& ActiveSession->bSetupFailed;
}
