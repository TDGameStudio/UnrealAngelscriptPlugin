#include "ClassGenerator/AngelscriptClassReloadPlanner.h"

FAngelscriptClassReloadPlanner::FNodeHandle FAngelscriptClassReloadPlanner::AddNode(
	FName DebugName,
	EReloadRequirement InitialRequirement)
{
	FNode Node;
	Node.DebugName = DebugName;
	Node.Requirement = InitialRequirement;
	return FNodeHandle(Nodes.Add(MoveTemp(Node)));
}

void FAngelscriptClassReloadPlanner::SetRequirement(FNodeHandle Node, EReloadRequirement Requirement)
{
	check(IsValidNode(Node));

	Nodes[Node.Index].Requirement = Requirement;
}

void FAngelscriptClassReloadPlanner::AddDependency(FNodeHandle Dependee, FNodeHandle Dependency)
{
	check(IsValidNode(Dependee));
	check(IsValidNode(Dependency));

	Nodes[Dependee.Index].Dependencies.AddUnique(Dependency.Index);
}

void FAngelscriptClassReloadPlanner::PropagateAll()
{
	bool bChanged = false;
	do
	{
		bChanged = false;

		for (FNode& Node : Nodes)
		{
			for (const int32 DependencyIndex : Node.Dependencies)
			{
				check(Nodes.IsValidIndex(DependencyIndex));

				const EReloadRequirement DependencyRequirement = Nodes[DependencyIndex].Requirement;
				if (DependencyRequirement > Node.Requirement)
				{
					Node.Requirement = DependencyRequirement;
					bChanged = true;
				}
			}
		}
	}
	while (bChanged);
}

FAngelscriptClassReloadPlanner::EReloadRequirement FAngelscriptClassReloadPlanner::GetRequirement(FNodeHandle Node) const
{
	check(IsValidNode(Node));

	return Nodes[Node.Index].Requirement;
}

void FAngelscriptClassReloadPlanner::Reset()
{
	Nodes.Reset();
}

bool FAngelscriptClassReloadPlanner::IsValidNode(FNodeHandle Node) const
{
	return Nodes.IsValidIndex(Node.Index);
}
