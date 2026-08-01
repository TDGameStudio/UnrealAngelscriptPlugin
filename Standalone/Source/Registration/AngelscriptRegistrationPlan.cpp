#include "Registration/AngelscriptRegistrationPlan.h"

#include <algorithm>
#include <functional>
#include <map>
#include <set>

namespace AngelscriptStandalone
{
	namespace
	{
		bool IsEarlyType(const FOfflineSymbolRecord& Symbol)
		{
			return Symbol.Kind == "typedef"
				|| Symbol.Kind == "funcdef"
				|| Symbol.Kind == "delegate"
				|| (Symbol.Kind == "type"
					&& (Symbol.Type.Kind == "primitive"
						|| Symbol.Type.Kind == "enum"
						|| Symbol.Type.Kind == "typedef"
						|| Symbol.Type.Kind == "funcdef"
						|| Symbol.Type.Kind == "delegate"));
		}
	}

	FRegistrationPlan BuildRegistrationPlan(
		const FOfflineManifest& Manifest,
		const FScriptBaselinePlan& Baseline,
		const std::vector<AngelscriptStandalone::Frontend::FLanguageModule>&
			SourceClosure)
	{
		FRegistrationPlan Result;
		if (!Baseline.bSuccess)
		{
			Result.Error = "cannot build registration plan from an invalid baseline";
			return Result;
		}
		Result.Items.push_back({
			ERegistrationStage::EngineSettings,
			"manifest-engine-settings",
		});

		std::map<std::string, const FOfflineSymbolRecord*> SymbolsById;
		std::vector<const FOfflineSymbolRecord*> EarlyTypes;
		std::vector<const FOfflineSymbolRecord*> ObjectTypes;
		std::vector<const FOfflineSymbolRecord*> UnorderedMembers;
		std::vector<const FOfflineSymbolRecord*> AdapterMembers;
		std::vector<const FOfflineSymbolRecord*> Members;
		for (const FOfflineSymbolRecord* Symbol : Baseline.IncludedSymbols)
		{
			if (Symbol == nullptr
				|| !SymbolsById.emplace(Symbol->StableId, Symbol).second)
			{
				Result.Error =
					"registration input contains a duplicate stable symbol";
				return Result;
			}
			if (IsEarlyType(*Symbol))
			{
				EarlyTypes.push_back(Symbol);
			}
			else if (Symbol->Kind == "type")
			{
				ObjectTypes.push_back(Symbol);
			}
			else
			{
				UnorderedMembers.push_back(Symbol);
			}
		}
		for (const FOfflineSymbolRecord* Symbol : UnorderedMembers)
		{
			const std::string_view Owner = Symbol->GetOwnerStableId();
			const auto OwnerType = SymbolsById.find(std::string(Owner));
			if (!Owner.empty()
				&& OwnerType != SymbolsById.end()
				&& !OwnerType->second->Type.AdapterStableId.empty())
			{
				AdapterMembers.push_back(Symbol);
			}
			else
			{
				Members.push_back(Symbol);
			}
		}

		auto SortById = [](const auto* Left, const auto* Right)
		{
			return Left->StableId < Right->StableId;
		};
		std::sort(EarlyTypes.begin(), EarlyTypes.end(), SortById);
		std::sort(ObjectTypes.begin(), ObjectTypes.end(), SortById);
		std::sort(
			AdapterMembers.begin(),
			AdapterMembers.end(),
			[&SymbolsById](const auto* Left, const auto* Right)
			{
				const auto& LeftOwner =
					*SymbolsById.at(
						std::string(Left->GetOwnerStableId()));
				const auto& RightOwner =
					*SymbolsById.at(
						std::string(Right->GetOwnerStableId()));
				const bool bLeftIterator =
					LeftOwner.Type.Name.find("Iterator")
						!= std::string::npos;
				const bool bRightIterator =
					RightOwner.Type.Name.find("Iterator")
						!= std::string::npos;
				if (bLeftIterator != bRightIterator)
				{
					return bLeftIterator;
				}
				if (LeftOwner.Type.Name != RightOwner.Type.Name)
				{
					return LeftOwner.Type.Name
						< RightOwner.Type.Name;
				}
				return Left->StableId < Right->StableId;
			});
		std::sort(Members.begin(), Members.end(), SortById);

		for (const FOfflineSymbolRecord* Symbol : EarlyTypes)
		{
			Result.Items.push_back({
				ERegistrationStage::EnumTypedefFuncdef,
				Symbol->StableId,
				Symbol,
			});
		}
		for (const FOfflineSymbolRecord* Symbol : ObjectTypes)
		{
			Result.Items.push_back({
				ERegistrationStage::TypeSkeleton,
				Symbol->StableId,
				Symbol,
			});
		}

		enum class EVisitState
		{
			Unvisited,
			Visiting,
			Visited,
		};
		std::map<std::string, EVisitState> States;
		std::vector<const FOfflineSymbolRecord*> RelationshipOrder;
		bool bRelationshipError = false;
		std::function<void(const FOfflineSymbolRecord*)> VisitType =
			[&](const FOfflineSymbolRecord* Symbol)
			{
				if (bRelationshipError
					|| States[Symbol->StableId] == EVisitState::Visited)
				{
					return;
				}
				if (States[Symbol->StableId] == EVisitState::Visiting)
				{
					bRelationshipError = true;
					Result.Error =
						"type relationship cycle at stable symbol: "
						+ Symbol->StableId;
					return;
				}
				States[Symbol->StableId] = EVisitState::Visiting;
				std::vector<std::string> Dependencies =
					Symbol->Type.InterfaceStableIds;
				if (!Symbol->Type.BaseStableId.empty())
				{
					Dependencies.push_back(Symbol->Type.BaseStableId);
				}
				std::sort(Dependencies.begin(), Dependencies.end());
				for (const std::string& Dependency : Dependencies)
				{
					const auto Found = SymbolsById.find(Dependency);
					if (Found == SymbolsById.end()
						|| Found->second->Type.StableId.empty())
					{
						bRelationshipError = true;
						Result.Error =
							"type relationship references a missing type: "
							+ Dependency;
						return;
					}
					VisitType(Found->second);
				}
				States[Symbol->StableId] = EVisitState::Visited;
				RelationshipOrder.push_back(Symbol);
			};
		for (const FOfflineSymbolRecord* Symbol : ObjectTypes)
		{
			VisitType(Symbol);
		}
		if (bRelationshipError)
		{
			return Result;
		}
		for (const FOfflineSymbolRecord* Symbol : RelationshipOrder)
		{
			Result.Items.push_back({
				ERegistrationStage::TypeRelationships,
				Symbol->StableId,
				Symbol,
			});
		}

		auto AppendMember = [&Result, &SymbolsById](
			const FOfflineSymbolRecord* Symbol) -> bool
		{
			const std::string_view Owner = Symbol->GetOwnerStableId();
			if (!Owner.empty()
				&& (!SymbolsById.contains(std::string(Owner))
					|| SymbolsById.at(std::string(Owner))->Type.StableId.empty()))
			{
				Result.Error =
					"member references a missing owner type: "
					+ std::string(Owner);
				return false;
			}
			Result.Items.push_back({
				ERegistrationStage::MembersAndGlobals,
				Symbol->StableId,
				Symbol,
			});
			return true;
		};
		for (const FOfflineSymbolRecord* Symbol : AdapterMembers)
		{
			if (!AppendMember(Symbol))
			{
				return Result;
			}
		}
		for (const FOfflineSymbolRecord* Symbol : Members)
		{
			if (!AppendMember(Symbol))
			{
				return Result;
			}
		}
		for (const FOfflineSymbolRecord* Type : ObjectTypes)
		{
			for (const std::string& Member : Type->Type.MemberStableIds)
			{
				if (!SymbolsById.contains(Member))
				{
					Result.Error =
						"type references a missing member symbol: "
						+ Member;
					return Result;
				}
			}
		}

		for (const FOfflineAdapterDescriptor& Adapter : Manifest.Adapters)
		{
			Result.Items.push_back({
				ERegistrationStage::Adapters,
				Adapter.StableId,
				nullptr,
				&Adapter,
			});
		}
		for (const auto& Source : SourceClosure)
		{
			Result.Items.push_back({
				ERegistrationStage::Sources,
				Source.ModuleId,
				nullptr,
				nullptr,
				&Source,
			});
		}
		Result.bSuccess = true;
		return Result;
	}
}
