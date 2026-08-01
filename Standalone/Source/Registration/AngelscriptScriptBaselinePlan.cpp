#include "Registration/AngelscriptScriptBaselinePlan.h"

#include "Support/AngelscriptStandaloneHash.h"
#include "Compiler/Frontend/AngelscriptStandaloneSource.h"

#include <algorithm>
#include <map>
#include <set>

namespace AngelscriptStandalone
{
	namespace
	{
		std::string MakeProjectScriptStableModuleId(
			const AngelscriptStandalone::Frontend::FLanguageModule& Module)
		{
			using namespace Frontend;
			const FPathResult LogicalPath =
				NormalizeLogicalPath(Module.LogicalPath);
			if (!LogicalPath.bSuccess
				|| ModuleNameFromLogicalPath(LogicalPath.Value)
					!= Module.ModuleName)
			{
				return {};
			}
			return Sha256(
				"module-id-v1\n" + Module.ModuleName
				+ "\n/Angelscript/Game/" + LogicalPath.Value);
		}
	}

	FScriptBaselinePlan BuildScriptBaselinePlan(
		const FOfflineBundleIndices& Indices,
		const std::vector<AngelscriptStandalone::Frontend::FLanguageModule>&
			SourceClosure)
	{
		FScriptBaselinePlan Result;
		std::map<std::string, std::string> ClosureNameById;
		std::map<std::string, std::string> ClosureIdByName;
		for (const auto& Module : SourceClosure)
		{
			const std::string StableModuleId =
				MakeProjectScriptStableModuleId(Module);
			if (Module.ModuleId.empty() || StableModuleId.empty())
			{
				Result.Error =
					"source closure contains a module without stable identity";
				return Result;
			}
			if (!ClosureNameById.emplace(
					StableModuleId,
					Module.ModuleName).second
				|| !ClosureIdByName.emplace(
					Module.ModuleName,
					StableModuleId).second)
			{
				Result.Error =
					"source closure contains an ambiguous module identity";
				return Result;
			}
		}

		std::map<std::string, std::string> BaselineNameById;
		for (const FOfflineSymbolRecord& Symbol : Indices.Symbols())
		{
			if (Symbol.Origin.Layer == "host-surface")
			{
				Result.IncludedSymbols.push_back(&Symbol);
				continue;
			}
			if (Symbol.Origin.Layer != "script-baseline")
			{
				Result.Error = "unsupported symbol origin layer: "
					+ Symbol.Origin.Layer;
				return Result;
			}
			if (Symbol.Origin.StableModuleId.empty()
				|| Symbol.Origin.Module.empty())
			{
				Result.Error =
					"script-baseline symbol is missing stable module identity: "
					+ Symbol.StableId;
				return Result;
			}
			const auto [Iterator, bInserted] = BaselineNameById.emplace(
				Symbol.Origin.StableModuleId,
				Symbol.Origin.Module);
			if (!bInserted && Iterator->second != Symbol.Origin.Module)
			{
				Result.Error =
					"script-baseline stable module ID has conflicting names";
				return Result;
			}
			const auto SameName =
				ClosureIdByName.find(Symbol.Origin.Module);
			if (SameName != ClosureIdByName.end()
				&& SameName->second != Symbol.Origin.StableModuleId)
			{
				Result.Error =
					"source and baseline module names have conflicting stable IDs: "
					+ Symbol.Origin.Module;
				return Result;
			}
			if (ClosureNameById.contains(Symbol.Origin.StableModuleId))
			{
				Result.SuppressedBaselineSymbols.push_back(&Symbol);
			}
			else
			{
				Result.IncludedSymbols.push_back(&Symbol);
			}
		}

		for (const auto& [ModuleId, ModuleName] : ClosureNameById)
		{
			(void)ModuleName;
			Result.ReplacedModuleIds.push_back(ModuleId);
		}
		std::sort(
			Result.ReplacedModuleIds.begin(),
			Result.ReplacedModuleIds.end());
		Result.bSuccess = true;
		return Result;
	}
}
