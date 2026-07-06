#pragma once

#include "CoreMinimal.h"

// Keeps track of which lines are executable and which ones we hit.
struct FLineCoverage
{
	// The total number of executable lines in this file.
	int NumExecutableLines() const
	{
		return HitCounts.Num();
	}

	// Returns the number of executable lines that were hit.
	int NumLinesHit() const
	{
		int LinesHit = 0;
		for (const TPair<int, int>& Hit : HitCounts)
		{
			if (Hit.Value > 0)
			{
				LinesHit++;
			}
		}
		return LinesHit;
	}

	// Use this to cut out mapped lines after a particular line. This is
	// used to ignore i.e. generated Angelscript code which tends to be
	// put on a line outside the lines in the file.
	void PruneGeneratedCode(int Cutoff)
	{
		for (auto It = HitCounts.CreateIterator(); It; ++It)
		{
			if (It->Key > Cutoff)
			{
				It.RemoveCurrent();
			}
		}
	}

	// Map line number -> hit counts.
	TMap<int, int> HitCounts;

	// Absolute filename of the .as source file.
	FString AbsoluteFilename;
};
