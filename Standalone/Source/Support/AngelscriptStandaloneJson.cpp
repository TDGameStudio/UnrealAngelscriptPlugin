#include "Support/AngelscriptStandaloneJson.h"

#include <cstdio>

namespace AngelscriptStandalone
{
	std::string EscapeJsonString(std::string_view Value)
	{
		std::string Result;
		Result.reserve(Value.size() + 2);
		Result.push_back('"');
		for (const unsigned char Character : Value)
		{
			switch (Character)
			{
			case '"':
				Result += "\\\"";
				break;
			case '\\':
				Result += "\\\\";
				break;
			case '\b':
				Result += "\\b";
				break;
			case '\f':
				Result += "\\f";
				break;
			case '\n':
				Result += "\\n";
				break;
			case '\r':
				Result += "\\r";
				break;
			case '\t':
				Result += "\\t";
				break;
			default:
				if (Character < 0x20)
				{
					char Buffer[7] = {};
					std::snprintf(Buffer, sizeof(Buffer), "\\u%04x", Character);
					Result += Buffer;
				}
				else
				{
					Result.push_back(static_cast<char>(Character));
				}
				break;
			}
		}
		Result.push_back('"');
		return Result;
	}
}
