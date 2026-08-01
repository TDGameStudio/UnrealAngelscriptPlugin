#include "Resources/AngelscriptAssetIndex.h"

#include "Resources/AngelscriptAssetPath.h"

#include <algorithm>
#include <set>

namespace AngelscriptStandalone
{
	std::shared_ptr<const FAssetIndex> FAssetIndex::Build(
		const std::vector<FOfflineAssetRecord>& Assets,
		FOfflineScope Scope,
		std::string& OutError)
	{
		auto Result =
			std::shared_ptr<FAssetIndex>(new FAssetIndex());
		Result->Assets_ = Assets;
		Result->Scope_ = std::move(Scope);
		std::sort(
			Result->Assets_.begin(),
			Result->Assets_.end(),
			[](const auto& Left, const auto& Right)
			{
				return Left.StableId < Right.StableId;
			});
		for (std::size_t Index = 0;
			Index < Result->Assets_.size();
			++Index)
		{
			const FOfflineAssetRecord& Asset =
				Result->Assets_[Index];
			for (const std::string* Path
				: {&Asset.ObjectPath, &Asset.GeneratedClassPath})
			{
				if (Path->empty())
					continue;
				const FAssetPathResult Normalized =
					NormalizeAssetPath(*Path);
				if (!Normalized.bSuccess)
				{
					OutError =
						"invalid asset path in offline bundle: "
						+ Normalized.Error;
					return {};
				}
				if (!Result->AssetByPath_.emplace(
						Normalized.Normalized,
						Index).second)
				{
					OutError =
						"duplicate normalized asset path: "
						+ Normalized.Normalized;
					return {};
				}
			}
			if (!Asset.RedirectSource.empty()
				|| !Asset.RedirectTarget.empty())
			{
				const FAssetPathResult Source =
					NormalizeAssetPath(Asset.RedirectSource);
				const FAssetPathResult Target =
					NormalizeAssetPath(Asset.RedirectTarget);
				if (!Source.bSuccess || !Target.bSuccess)
				{
					OutError = "invalid asset redirect path";
					return {};
				}
				if (!Result->Redirects_.emplace(
						Source.Normalized,
						Target.Normalized).second)
				{
					OutError =
						"duplicate asset redirect source: "
						+ Source.Normalized;
					return {};
				}
			}
		}

		for (const auto& [Source, Target] : Result->Redirects_)
		{
			(void)Target;
			std::set<std::string> Seen;
			std::string Current = Source;
			for (int Depth = 0; Depth <= 32; ++Depth)
			{
				if (!Seen.emplace(Current).second)
				{
					OutError =
						"asset redirect cycle at: " + Current;
					return {};
				}
				const auto Redirect =
					Result->Redirects_.find(Current);
				if (Redirect == Result->Redirects_.end())
				{
					if (!Result->AssetByPath_.contains(Current))
					{
						OutError =
							"asset redirect target is missing: "
							+ Current;
						return {};
					}
					break;
				}
				Current = Redirect->second;
				if (Depth == 32)
				{
					OutError =
						"asset redirect depth exceeds 32";
					return {};
				}
			}
		}
		return Result;
	}

	FAssetLookup FAssetIndex::Lookup(
		const std::string_view Path) const
	{
		FAssetLookup Result;
		Result.OriginalPath = std::string(Path);
		const FAssetPathResult Normalized =
			NormalizeAssetPath(Path);
		if (!Normalized.bSuccess)
		{
			return Result;
		}
		Result.NormalizedPath = Normalized.Normalized;
		Result.FinalPath = Result.NormalizedPath;
		std::set<std::string> Seen;
		for (;;)
		{
			const auto Redirect =
				Redirects_.find(Result.FinalPath);
			if (Redirect == Redirects_.end())
				break;
			if (!Seen.emplace(Result.FinalPath).second)
			{
				return Result;
			}
			Result.bRedirected = true;
			Result.FinalPath = Redirect->second;
		}
		const auto Found = AssetByPath_.find(Result.FinalPath);
		if (Found != AssetByPath_.end())
		{
			Result.Asset = &Assets_[Found->second];
		}
		Result.bAuthoritativelyCovered =
			IsAuthoritativelyCovered(Result.NormalizedPath);
		return Result;
	}

	bool FAssetIndex::IsAuthoritativelyCovered(
		const std::string_view Path) const
	{
		if (!Scope_.bComplete)
		{
			return false;
		}
		if (Scope_.Included.empty())
		{
			return true;
		}
		for (const std::string& Root : Scope_.Included)
		{
			if (Path == Root
				|| (Path.starts_with(Root)
					&& Path.size() > Root.size()
					&& (Path[Root.size()] == '/'
						|| Path[Root.size()] == '.')))
			{
				return true;
			}
		}
		return false;
	}

	const FOfflineScope& FAssetIndex::Scope() const
	{
		return Scope_;
	}
}
