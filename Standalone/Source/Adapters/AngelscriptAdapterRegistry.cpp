#include "Adapters/AngelscriptAdapterRegistry.h"

#include <algorithm>
#include <array>
#include <set>

namespace AngelscriptStandalone
{
	namespace
	{
		const std::array<FKnownAdapter, 9>& KnownAdapters()
		{
			static const std::array<FKnownAdapter, 9> Adapters = {{
				{"18da27074a277c400005c200db320c99c895ceb54bd61999b6d51e91c8ac5737", "TSoftClassPtr", "1", "af31caabe3c1f6d296f0fe10b3244758be6700e38f5129dc82d7845daef9ebf5", {"subtype.uclass"}},
				{"2112cc5c9d241e46a33397977a14681d95567b30430080553ab5cbb134b07b34", "TMap", "1", "cb276d23d17b0dcb00a7667393a6cf840bdce862f8f6b7acd933adfe0e0cb74c", {"key.construct", "key.destruct", "key.copy", "key.compare", "key.hash", "value.construct", "value.destruct", "value.copy"}},
				{"3638435a7b6be74a04c554dd8641fa53e17af375f306ed042ef39016ab40df40", "TSubclassOf", "1", "3c5caaf7a709262255caf205f3d83d7b21b5a91ad348f3b1270f35d2d24ac489", {"subtype.uclass"}},
				{"58f7ef2998328cd80becb9d681894afc91a6f80404976f88c15e6e4fc1aa0ad4", "TObjectPtr", "1", "971d88e1a4a548573991b59b4de150b63d12721c06522b0a69aa6a043077997c", {"subtype.uobject"}},
				{"73a698cb02e30941aa7d43d4f2cf0c235270cf6d3405df8bb61c891c8edf67e4", "TSoftObjectPtr", "1", "ca4f539ccb391470039e3b5605f39b195bda714d95cedbd85e41d1cfa5d0bb26", {"subtype.uobject"}},
				{"87a5d6f7f2d87baa482fced68c5131867bdd83b10c319ac4a6ebf245d16bab85", "TOptional", "1", "520e9b60bb917aa5a317ffa3ea82b8fc07875f6c32c4681412eae0ede4714cbf", {"value.construct", "value.destruct", "value.copy"}},
				{"8a3420687583f868141cd298e66ba350de2399125d7f7c939f58d5e31a8977d3", "TArray", "1", "bfb612167131d4d6ca05323d7802bdd03cb5d44f3d317157898951e38f29351f", {"element.construct", "element.destruct", "element.copy"}},
				{"a104061d0923a7d1d6bdafca58c8b7d5da512e3ad9d4dcdc1469a80b15b36f4a", "TWeakObjectPtr", "1", "9bdaa53436414d737bc5473b99f4fb029eb3224f4b77b602172dac96044f52d0", {"subtype.uobject"}},
				{"f310ae8b0006c25c0a56562ccf96a475a7eefe0ce0efa92797501e6ce508f4ef", "TSet", "1", "18a2b19d7fe9c98d7403a7283b593fe73f423298ce9f27ef6632af0b75aebff6", {"element.construct", "element.destruct", "element.copy", "element.compare", "element.hash"}},
			}};
			return Adapters;
		}

		template <typename Value>
		std::set<std::string> ToSet(const std::vector<Value>& Values)
		{
			std::set<std::string> Result;
			for (const auto& ValueItem : Values)
			{
				Result.emplace(ValueItem);
			}
			return Result;
		}
	}

	const FKnownAdapter* FindKnownAdapter(
		const std::string_view StableId)
	{
		const auto Found = std::find_if(
			KnownAdapters().begin(),
			KnownAdapters().end(),
			[StableId](const FKnownAdapter& Adapter)
			{
				return Adapter.StableId == StableId;
			});
		return Found == KnownAdapters().end() ? nullptr : &*Found;
	}

	FAdapterHandshakeResult ValidateAdapterHandshake(
		const FOfflineManifest& Manifest)
	{
		FAdapterHandshakeResult Result;
		std::set<std::string> SeenIds;
		for (const FOfflineAdapterDescriptor& Descriptor
			: Manifest.Adapters)
		{
			if (!SeenIds.emplace(Descriptor.StableId).second)
			{
				Result.Error = "duplicate adapter descriptor: "
					+ Descriptor.StableId;
				return Result;
			}
			const FKnownAdapter* Known =
				FindKnownAdapter(Descriptor.StableId);
			if (Known == nullptr)
			{
				Result.Error = "unknown standalone adapter ID: "
					+ Descriptor.StableId;
				return Result;
			}
			if (Descriptor.Name != Known->Name
				|| Descriptor.Version != Known->Version)
			{
				Result.Error = "adapter name/version mismatch for "
					+ Descriptor.StableId;
				return Result;
			}
			if (Descriptor.SurfaceHash != Known->SurfaceHash)
			{
				Result.Error = "adapter registration surface mismatch for "
					+ Descriptor.Name;
				return Result;
			}
			if (Descriptor.bDeclarativeOnly)
			{
				Result.Error =
					"non-declarative template adapter was marked "
					"declarative-only: " + Descriptor.Name;
				return Result;
			}
			if (ToSet(Descriptor.RequiredTraits)
				!= ToSet(Known->RequiredTraits))
			{
				Result.Error = "adapter trait contract mismatch for "
					+ Descriptor.Name;
				return Result;
			}
			for (const std::string& Property
				: Descriptor.RequiredEngineProperties)
			{
				if (!Manifest.EngineProperties.contains(Property))
				{
					Result.Error =
						"adapter requires an absent engine property: "
						+ Property;
					return Result;
				}
			}
			Result.ResolvedAdapters.push_back(Known);
		}
		Result.bSuccess = true;
		return Result;
	}
}
