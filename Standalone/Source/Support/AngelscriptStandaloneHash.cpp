#include "Support/AngelscriptStandaloneHash.h"

#include "Support/AngelscriptStandaloneStreamingHash.h"

namespace AngelscriptStandalone
{
	std::string Sha256(std::span<const std::uint8_t> Bytes)
	{
		FStreamingSha256 Hash;
		Hash.Update(Bytes);
		return Hash.Finish();
	}

	std::string Sha256(std::string_view Text)
	{
		FStreamingSha256 Hash;
		Hash.Update(Text);
		return Hash.Finish();
	}
}
