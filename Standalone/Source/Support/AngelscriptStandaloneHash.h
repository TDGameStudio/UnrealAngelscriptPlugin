#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace AngelscriptStandalone
{
	std::string Sha256(std::span<const std::uint8_t> Bytes);
	std::string Sha256(std::string_view Text);

	inline std::string Sha256(const std::vector<std::uint8_t>& Bytes)
	{
		return Sha256(std::span<const std::uint8_t>(Bytes.data(), Bytes.size()));
	}
}
