#pragma once

#include "CoreMinimal.h"
#include "Dump/AngelscriptStateDiff.h"
#include "Dump/AngelscriptStateDump.h"
#include "Dump/AngelscriptStateSnapshot.h"
#include "Misc/AutomationTest.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"

struct FAngelscriptStateDumpDiffTestArtifacts
{
	FString OutputDir;
	FAngelscriptStateSnapshot Before;
	FAngelscriptStateSnapshot After;
	FAngelscriptStateDiff Diff;
};

struct FAngelscriptStateDumpDiffTestHelper
{
	static FString MakeUniqueOutputDir(const FString& Prefix)
	{
		return FPaths::Combine(
			FPaths::ProjectSavedDir(),
			TEXT("Automation"),
			TEXT("StateDumpDiff"),
			FString::Printf(TEXT("%s_%s"), *Prefix, *FGuid::NewGuid().ToString(EGuidFormats::Digits)));
	}

	static FAngelscriptStateSnapshot Capture(FAngelscriptEngine& Engine)
	{
		return FAngelscriptStateDump::CaptureSnapshot(Engine);
	}

	static bool DumpDiffArtifacts(
		FAutomationTestBase& Test,
		const FString& Prefix,
		const FAngelscriptStateSnapshot& Before,
		const FAngelscriptStateSnapshot& After,
		FAngelscriptStateDumpDiffTestArtifacts& OutArtifacts)
	{
		OutArtifacts.OutputDir = MakeUniqueOutputDir(Prefix);
		OutArtifacts.Before = Before;
		OutArtifacts.After = After;
		OutArtifacts.Diff = FAngelscriptStateDump::DiffSnapshots(OutArtifacts.Before, OutArtifacts.After);

		FAngelscriptStateDump::DumpSnapshot(OutArtifacts.Before, FPaths::Combine(OutArtifacts.OutputDir, TEXT("Before")));
		FAngelscriptStateDump::DumpSnapshot(OutArtifacts.After, FPaths::Combine(OutArtifacts.OutputDir, TEXT("After")));
		FAngelscriptStateDump::DumpSnapshotCategoryTables(OutArtifacts.Before, FPaths::Combine(OutArtifacts.OutputDir, TEXT("Before")));
		FAngelscriptStateDump::DumpSnapshotCategoryTables(OutArtifacts.After, FPaths::Combine(OutArtifacts.OutputDir, TEXT("After")));
		FAngelscriptStateDump::DumpDiff(OutArtifacts.Diff, OutArtifacts.OutputDir);

		Test.AddInfo(FString::Printf(
			TEXT("Angelscript state diff artifacts: %s | Added=%d Removed=%d Changed=%d"),
			*OutArtifacts.OutputDir,
			OutArtifacts.Diff.CountByChangeType(EAngelscriptStateDiffChangeType::Added),
			OutArtifacts.Diff.CountByChangeType(EAngelscriptStateDiffChangeType::Removed),
			OutArtifacts.Diff.CountByChangeType(EAngelscriptStateDiffChangeType::Changed)));

		return !OutArtifacts.OutputDir.IsEmpty();
	}

	static bool ContainsDiff(
		const FAngelscriptStateDiff& Diff,
		const FString& Category,
		const FString& Identity,
		const FString& Field,
		const EAngelscriptStateDiffChangeType ChangeType)
	{
		return Diff.Rows.ContainsByPredicate(
			[&Category, &Identity, &Field, ChangeType](const FAngelscriptStateDiffRow& Row)
			{
				return Row.Category == Category
					&& Row.Identity == Identity
					&& Row.Field == Field
					&& Row.ChangeType == ChangeType;
			});
	}
};
