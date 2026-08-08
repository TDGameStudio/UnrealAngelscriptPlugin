#include "Bind_FMemoryReader_Functions.h"

#include "Serialization/MemoryReader.h"

#include "AngelscriptEngine.h"

FMemoryReader* FAngelscriptFMemoryReaderBinds::Construct(
	FMemoryReader* Address,
	const TArray<uint8>& Data,
	const bool bForceByteSwapping)
{
	FMemoryReader* Reader = new (Address) FMemoryReader(Data);
	Reader->SetByteSwapping(bForceByteSwapping);
	return Reader;
}

int32 FAngelscriptFMemoryReaderBinds::TotalSize(FMemoryReader* Reader)
{
	return static_cast<int32>(Reader->TotalSize());
}

int32 FAngelscriptFMemoryReaderBinds::Tell(FMemoryReader* Reader)
{
	return static_cast<int32>(Reader->Tell());
}

void FAngelscriptFMemoryReaderBinds::Seek(FMemoryReader* Reader, const int32 Position)
{
	if (Position > Reader->TotalSize())
	{
		FAngelscriptEngine::Throw("Skipping past array bounds");
		return;
	}
	Reader->Seek(Position);
}

void FAngelscriptFMemoryReaderBinds::Skip(FMemoryReader* Reader, const int32 Count)
{
	if (Reader->Tell() + Count > Reader->TotalSize())
	{
		FAngelscriptEngine::Throw("Skipping past array bounds");
		return;
	}
	Reader->Seek(Reader->Tell() + Count);
}

int8 FAngelscriptFMemoryReaderBinds::ReadInt8(FMemoryReader* Reader)
{
	int8 Result;
	*Reader << Result;
	return Result;
}

uint8 FAngelscriptFMemoryReaderBinds::ReadUInt8(FMemoryReader* Reader)
{
	uint8 Result;
	*Reader << Result;
	return Result;
}

int16 FAngelscriptFMemoryReaderBinds::ReadInt16(FMemoryReader* Reader)
{
	int16 Result;
	*Reader << Result;
	return Result;
}

uint16 FAngelscriptFMemoryReaderBinds::ReadUInt16(FMemoryReader* Reader)
{
	uint16 Result;
	*Reader << Result;
	return Result;
}

int32 FAngelscriptFMemoryReaderBinds::ReadInt32(FMemoryReader* Reader)
{
	int32 Result;
	*Reader << Result;
	return Result;
}

uint32 FAngelscriptFMemoryReaderBinds::ReadUInt32(FMemoryReader* Reader)
{
	uint32 Result;
	*Reader << Result;
	return Result;
}

int64 FAngelscriptFMemoryReaderBinds::ReadInt64(FMemoryReader* Reader)
{
	int64 Result;
	*Reader << Result;
	return Result;
}

uint64 FAngelscriptFMemoryReaderBinds::ReadUInt64(FMemoryReader* Reader)
{
	uint64 Result;
	*Reader << Result;
	return Result;
}

float FAngelscriptFMemoryReaderBinds::ReadFloat(FMemoryReader* Reader)
{
	float Result;
	*Reader << Result;
	return Result;
}

double FAngelscriptFMemoryReaderBinds::ReadDouble(FMemoryReader* Reader)
{
	double Result;
	*Reader << Result;
	return Result;
}

TArray<uint8> FAngelscriptFMemoryReaderBinds::ReadBytes(FMemoryReader* Reader, const int32 Count)
{
	TArray<uint8> Result;
	Result.SetNumUninitialized(Count);
	Reader->Serialize(Result.GetData(), Count);
	return Result;
}

FString FAngelscriptFMemoryReaderBinds::ReadAnsiString(FMemoryReader* Reader, const int32 Count)
{
	TArray<ANSICHAR> Buffer;
	Buffer.SetNumUninitialized(Count);
	Reader->Serialize(Buffer.GetData(), Count);
	return FString(Count, StringCast<TCHAR>(Buffer.GetData(), Count).Get());
}
