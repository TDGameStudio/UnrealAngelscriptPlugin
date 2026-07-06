#pragma once

#include "CoreMinimal.h"
#include "ClassGenerator/AngelscriptClassGenerator.h"

struct ANGELSCRIPTRUNTIME_API FAngelscriptClassReloadPlanner
{
	using EReloadRequirement = FAngelscriptClassGenerator::EReloadRequirement;

	struct FNodeHandle
	{
		FNodeHandle() = default;
		explicit FNodeHandle(const int32 InIndex)
			: Index(InIndex)
		{
		}

		bool IsValid() const
		{
			return Index != INDEX_NONE;
		}

		int32 GetIndex() const
		{
			return Index;
		}

	private:
		int32 Index = INDEX_NONE;

		friend struct FAngelscriptClassReloadPlanner;
	};

	FNodeHandle AddNode(FName DebugName, EReloadRequirement InitialRequirement);
	void SetRequirement(FNodeHandle Node, EReloadRequirement Requirement);
	void AddDependency(FNodeHandle Dependee, FNodeHandle Dependency);
	void PropagateAll();
	EReloadRequirement GetRequirement(FNodeHandle Node) const;
	void Reset();

private:
	struct FNode
	{
		FName DebugName;
		EReloadRequirement Requirement = EReloadRequirement::SoftReload;
		TArray<int32> Dependencies;
	};

	bool IsValidNode(FNodeHandle Node) const;

	TArray<FNode> Nodes;
};
