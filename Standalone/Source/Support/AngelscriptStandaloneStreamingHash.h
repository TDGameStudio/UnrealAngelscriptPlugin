#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>

namespace AngelscriptStandalone
{
	class FStreamingSha256
	{
	public:
		FStreamingSha256();

		void Update(std::span<const std::uint8_t> Bytes);
		void Update(std::string_view Text);
		std::string Finish();

	private:
		void Transform(const std::uint8_t* Block);

		std::array<std::uint32_t, 8> State_;
		std::array<std::uint8_t, 64> Buffer_ = {};
		std::size_t BufferedBytes_ = 0;
		std::uint64_t TotalBytes_ = 0;
		bool bFinished_ = false;
	};
}
