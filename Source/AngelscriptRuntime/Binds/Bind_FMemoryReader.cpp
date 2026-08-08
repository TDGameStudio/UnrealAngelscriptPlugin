#include "AngelscriptBinds.h"

#include "Serialization/MemoryReader.h"

#include "Bind_FMemoryReader_Functions.h"

namespace
{
	void BindFMemoryReaderType(FAngelscriptBinds& Binds)
	{
		Binds.ValueClassForTarget<FMemoryReader>("FMemoryReader", FBindFlags());
	}

	void BindFMemoryReaderFunctions(FAngelscriptBinds& Binds)
	{
		auto Reader_ = Binds.ExistingClassForTarget("FMemoryReader");
		Reader_.Constructor(
			"void f(const TArray<uint8>& Data, bool ForceByteSwapping = false)",
			&FAngelscriptFMemoryReaderBinds::Construct);
		Reader_.Method("int TotalSize() const", &FAngelscriptFMemoryReaderBinds::TotalSize);
		Reader_.Method("int Tell() const", &FAngelscriptFMemoryReaderBinds::Tell);
		Reader_.Method("void Seek(int InPos)", &FAngelscriptFMemoryReaderBinds::Seek);
		Reader_.Method("void Skip(int Count)", &FAngelscriptFMemoryReaderBinds::Skip);
		Reader_.Method("int8 ReadInt8()", &FAngelscriptFMemoryReaderBinds::ReadInt8);
		Reader_.Method("uint8 ReadUInt8()", &FAngelscriptFMemoryReaderBinds::ReadUInt8);
		Reader_.Method("int16 ReadInt16()", &FAngelscriptFMemoryReaderBinds::ReadInt16);
		Reader_.Method("uint16 ReadUInt16()", &FAngelscriptFMemoryReaderBinds::ReadUInt16);
		Reader_.Method("int32 ReadInt32()", &FAngelscriptFMemoryReaderBinds::ReadInt32);
		Reader_.Method("uint32 ReadUInt32()", &FAngelscriptFMemoryReaderBinds::ReadUInt32);
		Reader_.Method("int64 ReadInt64()", &FAngelscriptFMemoryReaderBinds::ReadInt64);
		Reader_.Method("uint64 ReadUInt64()", &FAngelscriptFMemoryReaderBinds::ReadUInt64);
		Reader_.Method("float32 ReadFloat()", &FAngelscriptFMemoryReaderBinds::ReadFloat);
		Reader_.Method("float64 ReadDouble()", &FAngelscriptFMemoryReaderBinds::ReadDouble);
		Reader_.Method("TArray<uint8> ReadBytes(int Count)", &FAngelscriptFMemoryReaderBinds::ReadBytes);
		Reader_.Method("FString ReadAnsiString(int Count)", &FAngelscriptFMemoryReaderBinds::ReadAnsiString);
	}
}

AS_FORCE_LINK const FAngelscriptBind Bind_FMemoryReaderType(
	TEXT("FMemoryReader.Type"),
	EAngelscriptBindPhase::TypeDeclarations,
	&BindFMemoryReaderType);

AS_FORCE_LINK const FAngelscriptBind Bind_FMemoryReader(
	TEXT("FMemoryReader.Functions"),
	EAngelscriptBindPhase::ManualBindings,
	&BindFMemoryReaderFunctions);
