#include "Support/AngelscriptStandaloneStreamingHash.h"

#include <algorithm>
#include <bit>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace AngelscriptStandalone
{
	namespace
	{
		constexpr std::array<std::uint32_t, 64> RoundConstants = {
			0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u,
			0x3956c25bu, 0x59f111f1u, 0x923f82a4u, 0xab1c5ed5u,
			0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u,
			0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u, 0xc19bf174u,
			0xe49b69c1u, 0xefbe4786u, 0x0fc19dc6u, 0x240ca1ccu,
			0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau,
			0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u,
			0xc6e00bf3u, 0xd5a79147u, 0x06ca6351u, 0x14292967u,
			0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu, 0x53380d13u,
			0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u,
			0xa2bfe8a1u, 0xa81a664bu, 0xc24b8b70u, 0xc76c51a3u,
			0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u,
			0x19a4c116u, 0x1e376c08u, 0x2748774cu, 0x34b0bcb5u,
			0x391c0cb3u, 0x4ed8aa4au, 0x5b9cca4fu, 0x682e6ff3u,
			0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u,
			0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u,
		};

		std::uint32_t ReadBigEndian(const std::uint8_t* Data)
		{
			return (static_cast<std::uint32_t>(Data[0]) << 24)
				| (static_cast<std::uint32_t>(Data[1]) << 16)
				| (static_cast<std::uint32_t>(Data[2]) << 8)
				| static_cast<std::uint32_t>(Data[3]);
		}
	}

	FStreamingSha256::FStreamingSha256()
		: State_{
			0x6a09e667u, 0xbb67ae85u, 0x3c6ef372u, 0xa54ff53au,
			0x510e527fu, 0x9b05688cu, 0x1f83d9abu, 0x5be0cd19u}
	{
	}

	void FStreamingSha256::Transform(const std::uint8_t* Block)
	{
		std::array<std::uint32_t, 64> Words = {};
		for (std::size_t Index = 0; Index < 16; ++Index)
		{
			Words[Index] = ReadBigEndian(Block + Index * 4u);
		}
		for (std::size_t Index = 16; Index < Words.size(); ++Index)
		{
			const std::uint32_t S0 = std::rotr(Words[Index - 15], 7)
				^ std::rotr(Words[Index - 15], 18)
				^ (Words[Index - 15] >> 3);
			const std::uint32_t S1 = std::rotr(Words[Index - 2], 17)
				^ std::rotr(Words[Index - 2], 19)
				^ (Words[Index - 2] >> 10);
			Words[Index] = Words[Index - 16] + S0 + Words[Index - 7] + S1;
		}

		std::uint32_t A = State_[0];
		std::uint32_t B = State_[1];
		std::uint32_t C = State_[2];
		std::uint32_t D = State_[3];
		std::uint32_t E = State_[4];
		std::uint32_t F = State_[5];
		std::uint32_t G = State_[6];
		std::uint32_t H = State_[7];
		for (std::size_t Index = 0; Index < Words.size(); ++Index)
		{
			const std::uint32_t Sum1 =
				std::rotr(E, 6) ^ std::rotr(E, 11) ^ std::rotr(E, 25);
			const std::uint32_t Choice = (E & F) ^ (~E & G);
			const std::uint32_t Temporary1 =
				H + Sum1 + Choice + RoundConstants[Index] + Words[Index];
			const std::uint32_t Sum0 =
				std::rotr(A, 2) ^ std::rotr(A, 13) ^ std::rotr(A, 22);
			const std::uint32_t Majority = (A & B) ^ (A & C) ^ (B & C);
			const std::uint32_t Temporary2 = Sum0 + Majority;
			H = G;
			G = F;
			F = E;
			E = D + Temporary1;
			D = C;
			C = B;
			B = A;
			A = Temporary1 + Temporary2;
		}

		State_[0] += A;
		State_[1] += B;
		State_[2] += C;
		State_[3] += D;
		State_[4] += E;
		State_[5] += F;
		State_[6] += G;
		State_[7] += H;
	}

	void FStreamingSha256::Update(std::span<const std::uint8_t> Bytes)
	{
		if (bFinished_)
		{
			throw std::logic_error("cannot update a finalized SHA-256");
		}
		TotalBytes_ += static_cast<std::uint64_t>(Bytes.size());
		while (!Bytes.empty())
		{
			const std::size_t CopyCount = std::min(
				Buffer_.size() - BufferedBytes_,
				Bytes.size());
			std::copy_n(
				Bytes.begin(),
				CopyCount,
				Buffer_.begin() + static_cast<std::ptrdiff_t>(BufferedBytes_));
			BufferedBytes_ += CopyCount;
			Bytes = Bytes.subspan(CopyCount);
			if (BufferedBytes_ == Buffer_.size())
			{
				Transform(Buffer_.data());
				BufferedBytes_ = 0;
			}
		}
	}

	void FStreamingSha256::Update(const std::string_view Text)
	{
		Update(std::span<const std::uint8_t>(
			reinterpret_cast<const std::uint8_t*>(Text.data()),
			Text.size()));
	}

	std::string FStreamingSha256::Finish()
	{
		if (bFinished_)
		{
			throw std::logic_error("SHA-256 already finalized");
		}
		const std::uint64_t BitLength = TotalBytes_ * 8u;
		Buffer_[BufferedBytes_++] = 0x80u;
		if (BufferedBytes_ > 56u)
		{
			std::fill(
				Buffer_.begin() + static_cast<std::ptrdiff_t>(BufferedBytes_),
				Buffer_.end(),
				std::uint8_t{0});
			Transform(Buffer_.data());
			BufferedBytes_ = 0;
		}
		std::fill(
			Buffer_.begin() + static_cast<std::ptrdiff_t>(BufferedBytes_),
			Buffer_.begin() + 56,
			std::uint8_t{0});
		for (std::size_t Index = 0; Index < 8; ++Index)
		{
			Buffer_[56 + Index] = static_cast<std::uint8_t>(
				(BitLength >> (56u - Index * 8u)) & 0xffu);
		}
		Transform(Buffer_.data());
		bFinished_ = true;

		std::ostringstream Result;
		Result << std::hex << std::setfill('0');
		for (const std::uint32_t Word : State_)
		{
			Result << std::setw(8) << Word;
		}
		return Result.str();
	}
}
