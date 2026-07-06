#pragma once

#include "CoreMinimal.h"
#include "LineCoverage.h"

struct FCoverageCounts
{
	FString ToString() const
	{
		if (NumExecutableLines == 0)
		{
			return "N/A";
		}
		else
		{
			float CoveragePercent = (float) NumLinesHit / NumExecutableLines * 100;
			return FString::Printf(TEXT("%.1f%% (%d/%d)"), CoveragePercent, NumLinesHit, NumExecutableLines);
		}
	}

	int NumLinesHit = 0;
	int NumExecutableLines = 0;
};

struct FCoverageNode
{
	~FCoverageNode()
	{
		for (const TPair<FString, FCoverageNode*>& Child : Children)
		{
			delete Child.Value;
		}
	}

	FCoverageCounts Counts;
	TMap<FString, FCoverageNode*> Children;
};

// Constructs a directory tree from a bunch of paths, e.g. A/B/C.as, A/B/D.as and
// E/F/G.as leads to the tree
// [root] -> A -> B -> C.as
//                  -> D.as
//        -> E -> F -> G.as
// The new leaf node gets the hit counts from Coverage.
ANGELSCRIPTRUNTIME_API void AddCoverageLeaf(FCoverageNode& Root, const FString& Path, const FLineCoverage& Coverage);

// Traverses the tree in postorder (leaves first) and computes hit counts for directories.
// This is done by adding up the hit counts from all children (and AddCoverageLeaf) already
// set up the hit counts for all leaves.
ANGELSCRIPTRUNTIME_API FCoverageCounts ComputeCoverage(FCoverageNode& Node);

struct FCoverageJsonExportOptions
{
	TArray<FString> ExcludePatterns;
};

// Writes the full machine-readable coverage package consumed by external tools.
bool WriteCoverageSummaryJson(
	TMap<FString, FLineCoverage>& FilesToCoverage,
	const FString& OutputDir,
	const FCoverageJsonExportOptions& Options);
