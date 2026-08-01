#pragma once

#include "angelscript.h"

#include <cstdint>
#include <vector>

namespace AngelscriptStandalone
{
	class FMemoryByteCodeStream final : public asIBinaryStream
	{
	public:
		int Write(const void* Data, asUINT Size) override
		{
			const auto* Bytes = static_cast<const std::uint8_t*>(Data);
			Buffer.insert(Buffer.end(), Bytes, Bytes + Size);
			return 0;
		}

		int Read(void*, asUINT) override
		{
			return -1;
		}

		std::vector<std::uint8_t> Buffer;
	};
}
