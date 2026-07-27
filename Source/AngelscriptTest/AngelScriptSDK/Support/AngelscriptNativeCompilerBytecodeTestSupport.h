#pragma once

#include "StartAngelscriptHeaders.h"
#include "source/as_bytecode.h"
#include "EndAngelscriptHeaders.h"

namespace AngelscriptCompilerBytecodeTestSupport
{
	inline bool ContainsOpcode(asIScriptFunction* Function, const asEBCInstr WantedOpcode)
	{
		if (Function == nullptr)
		{
			return false;
		}

		asUINT BytecodeLength = 0;
		const asDWORD* Bytecode = Function->GetByteCode(&BytecodeLength);
		if (Bytecode == nullptr || BytecodeLength == 0)
		{
			return false;
		}

		asUINT DwordIndex = 0;
		while (DwordIndex < BytecodeLength)
		{
			const asEBCInstr Opcode = static_cast<asEBCInstr>(*reinterpret_cast<const asBYTE*>(&Bytecode[DwordIndex]));
			if (Opcode == WantedOpcode)
			{
				return true;
			}

			if (static_cast<int32>(Opcode) > static_cast<int32>(asBC_MAXBYTECODE))
			{
				break;
			}

			const int32 InstructionSize = asBCTypeSize[asBCInfo[Opcode].type];
			if (InstructionSize <= 0)
			{
				break;
			}

			DwordIndex += static_cast<asUINT>(InstructionSize);
		}

		return false;
	}

	inline bool ContainsAnyOpcode(asIScriptFunction* Function, const asEBCInstr* WantedOpcodes, const int32 WantedCount)
	{
		if (WantedOpcodes == nullptr || WantedCount <= 0)
		{
			return false;
		}

		for (int32 Index = 0; Index < WantedCount; ++Index)
		{
			if (ContainsOpcode(Function, WantedOpcodes[Index]))
			{
				return true;
			}
		}

		return false;
	}
}
