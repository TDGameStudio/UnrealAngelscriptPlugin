#include "Resources/AngelscriptResourceValidator.h"

#include "Resources/AngelscriptAssetPath.h"
#include "Support/AngelscriptStandaloneHash.h"

#include <map>
#include <set>

namespace AngelscriptStandalone
{
	namespace
	{
		const FOfflineSymbolRecord* FindTypeByUnrealPath(
			const std::string_view UnrealPath,
			const FOfflineBundleIndices& Symbols)
		{
			for (const FOfflineSymbolRecord& Symbol : Symbols.Symbols())
			{
				if (!Symbol.Type.StableId.empty()
					&& Symbol.Type.UETypePath == UnrealPath)
					return &Symbol;
			}
			return nullptr;
		}

		enum class EAssignability
		{
			Compatible,
			Incompatible,
			Unknown,
		};

		EAssignability CheckAssignability(
			const std::string_view ActualTypePath,
			const std::string_view RequestedStableId,
			const FOfflineBundleIndices& Symbols)
		{
			if (RequestedStableId.empty())
				return EAssignability::Compatible;
			const FOfflineSymbolRecord* Requested =
				Symbols.FindType(RequestedStableId);
			if (Requested == nullptr
				|| Requested->Type.UETypePath.empty()
				|| ActualTypePath.empty())
				return EAssignability::Unknown;
			if (ActualTypePath == Requested->Type.UETypePath)
				return EAssignability::Compatible;

			const FOfflineSymbolRecord* Current =
				FindTypeByUnrealPath(ActualTypePath, Symbols);
			std::set<std::string> Seen;
			while (Current != nullptr
				&& !Current->Type.BaseStableId.empty()
				&& Seen.emplace(Current->StableId).second)
			{
				if (Current->Type.BaseStableId == RequestedStableId)
					return EAssignability::Compatible;
				Current =
					Symbols.FindType(Current->Type.BaseStableId);
			}
			return Current == nullptr
				? EAssignability::Unknown
				: EAssignability::Incompatible;
		}
	}

	const char* ToString(const EResourceState State)
	{
		switch (State)
		{
		case EResourceState::Found:
			return "found";
		case EResourceState::Redirected:
			return "redirected";
		case EResourceState::Missing:
			return "missing";
		case EResourceState::Incompatible:
			return "incompatible";
		case EResourceState::Unknown:
		default:
			return "unknown";
		}
	}

	FResourceValidation ValidateResourceContext(
		const FResourceContext& Context,
		const FAssetIndex& Assets,
		const FOfflineBundleIndices& Symbols)
	{
		FResourceValidation Result;
		Result.Context = Context;
		const FAssetPathResult Normalized =
			NormalizeAssetPath(Context.ConstantPath);
		if (!Normalized.bSuccess)
		{
			Result.State = EResourceState::Incompatible;
			Result.Reason = Normalized.Error;
		}
		else
		{
			const FAssetLookup Lookup =
				Assets.Lookup(Normalized.Normalized);
			Result.NormalizedPath = Lookup.NormalizedPath;
			Result.FinalPath = Lookup.FinalPath;
			if (Lookup.Asset == nullptr)
			{
				Result.State = Lookup.bAuthoritativelyCovered
					? EResourceState::Missing
					: EResourceState::Unknown;
				Result.Reason = Lookup.bAuthoritativelyCovered
					? "authoritative asset scope proves the path absent"
					: "selected bundle asset scope cannot decide";
			}
			else
			{
				Result.ResolvedAssetStableId =
					Lookup.Asset->StableId;
				Result.ResolvedTypePath = Context.bClass
					? Lookup.Asset->BaseClassPath
					: Lookup.Asset->AssetClassPath;
				const EAssignability Assignability =
					CheckAssignability(
						Result.ResolvedTypePath,
						Context.RequestedStableTypeId,
						Symbols);
				if (Assignability == EAssignability::Unknown)
				{
					Result.State = EResourceState::Unknown;
					Result.Reason =
						"asset exists but type hierarchy evidence is incomplete";
				}
				else if (Assignability == EAssignability::Incompatible)
				{
					Result.State = EResourceState::Incompatible;
					Result.Reason =
						"asset type is not assignable to the requested type";
				}
				else
				{
					Result.State = Lookup.bRedirected
						? EResourceState::Redirected
						: EResourceState::Found;
					Result.Reason = Lookup.bRedirected
						? "asset path resolves through a bundle redirect"
						: "asset path and requested type are present";
				}
			}
		}
		Result.DiagnosticId = Sha256(
			"resource-diagnostic-v1\n"
			+ Result.Context.ContextId + "\n"
			+ ToString(Result.State) + "\n"
			+ Result.FinalPath);
		return Result;
	}
}
