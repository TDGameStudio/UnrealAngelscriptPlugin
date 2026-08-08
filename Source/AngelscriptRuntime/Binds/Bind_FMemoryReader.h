#pragma once

#include "CoreMinimal.h"

class FMemoryReader;

struct FAngelscriptFMemoryReaderBinds
{
	static FMemoryReader* Construct(FMemoryReader* Address, const TArray<uint8>& Data, bool bForceByteSwapping);
	static int32 TotalSize(FMemoryReader* Reader);
	static int32 Tell(FMemoryReader* Reader);
	static void Seek(FMemoryReader* Reader, int32 Position);
	static void Skip(FMemoryReader* Reader, int32 Count);
	static int8 ReadInt8(FMemoryReader* Reader);
	static uint8 ReadUInt8(FMemoryReader* Reader);
	static int16 ReadInt16(FMemoryReader* Reader);
	static uint16 ReadUInt16(FMemoryReader* Reader);
	static int32 ReadInt32(FMemoryReader* Reader);
	static uint32 ReadUInt32(FMemoryReader* Reader);
	static int64 ReadInt64(FMemoryReader* Reader);
	static uint64 ReadUInt64(FMemoryReader* Reader);
	static float ReadFloat(FMemoryReader* Reader);
	static double ReadDouble(FMemoryReader* Reader);
	static TArray<uint8> ReadBytes(FMemoryReader* Reader, int32 Count);
	static FString ReadAnsiString(FMemoryReader* Reader, int32 Count);
};
