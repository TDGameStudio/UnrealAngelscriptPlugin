#pragma once

#include "Misc/AutomationTest.h"
#include "Testing/AngelscriptScriptTestRegistry.h"

/**
 * An isolated Automation result sink used when a framework regression test
 * deliberately exercises a failing script leaf. Its unique registration name
 * keeps the intentional inner failure out of the surrounding CQTest result.
 */
class FAngelscriptScriptTestProbe final : public FAutomationTestBase
{
public:
	explicit FAngelscriptScriptTestProbe(const FString& Name)
		: FAutomationTestBase(Name, false)
	{
	}

	EAutomationTestFlags GetTestFlags() const override
	{
		return EAutomationTestFlags::EditorContext
			| EAutomationTestFlags::EngineFilter;
	}

	FString GetBeautifiedTestName() const override
	{
		return TEXT("Angelscript.ScriptTests.Probe");
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

	TArray<FAutomationExecutionEntry> GetExecutionEntries() const
	{
		FAutomationTestExecutionInfo Info;
		GetExecutionInfo(Info);
		return Info.GetEntries();
	}

protected:
	bool RunTest(const FString&) override
	{
		return true;
	}
};

inline const FAngelscriptScriptTestDescriptor*
FindOnlyAngelscriptScriptTestDescriptor(
	const FAngelscriptScriptTestRegistryBuildResult& Result)
{
	return Result.Snapshot.IsValid()
		&& Result.Snapshot->Tests.Num() == 1
		? &Result.Snapshot->Tests[0]
		: nullptr;
}
