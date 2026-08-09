#include "Bind_FMemoryReader.h"

#include "AngelscriptBinds.h"

#include "Serialization/MemoryReader.h"

/**
 * FMemoryReader construction, positioning, and primitive reads.
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | AngelScript usage signature                                                                          | Purpose / parameter notes                                                                                        |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FMemoryReader Reader(const TArray<uint8>& Data, bool ForceByteSwapping = false);                     | Constructs an archive over the supplied byte array.                                                              |
 * |                                                                                                      | @param ForceByteSwapping Reverses serialized byte order when true.                                               |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | int Reader.TotalSize() const;                                                                        | Returns the total byte count.                                                                                    |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | int Reader.Tell() const;                                                                             | Returns the current read offset.                                                                                 |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | void Reader.Seek(int InPos);                                                                         | Moves to an absolute byte offset.                                                                                |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | void Reader.Skip(int Count);                                                                         | Advances by a byte count.                                                                                        |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | int8 Reader.ReadInt8();                                                                              | Reads a signed 8-bit integer.                                                                                    |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | uint8 Reader.ReadUInt8();                                                                            | Reads an unsigned 8-bit integer.                                                                                 |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | int16 Reader.ReadInt16();                                                                            | Reads a signed 16-bit integer.                                                                                   |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | uint16 Reader.ReadUInt16();                                                                          | Reads an unsigned 16-bit integer.                                                                                |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | int32 Reader.ReadInt32();                                                                            | Reads a signed 32-bit integer.                                                                                   |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | uint32 Reader.ReadUInt32();                                                                          | Reads an unsigned 32-bit integer.                                                                                |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | int64 Reader.ReadInt64();                                                                            | Reads a signed 64-bit integer.                                                                                   |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | uint64 Reader.ReadUInt64();                                                                          | Reads an unsigned 64-bit integer.                                                                                |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | float32 Reader.ReadFloat();                                                                          | Reads a 32-bit floating-point value.                                                                             |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | float64 Reader.ReadDouble();                                                                         | Reads a 64-bit floating-point value.                                                                             |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | TArray<uint8> Reader.ReadBytes(int Count);                                                           | Reads Count raw bytes.                                                                                           |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FString Reader.ReadAnsiString(int Count);                                                            | Reads Count ANSI bytes as a string.                                                                              |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 */

AS_FORCE_LINK const FAngelscriptBind Bind_FMemoryReaderType(
	TEXT("FMemoryReader.Type"),
	EAngelscriptBindPhase::TypeDeclarations,
	[](FAngelscriptBinds& Binds)
	{
		Binds.ValueClassForTarget<FMemoryReader>("FMemoryReader", FBindFlags());
	});

AS_FORCE_LINK const FAngelscriptBind Bind_FMemoryReader(
	TEXT("FMemoryReader.Functions"),
	EAngelscriptBindPhase::ManualBindings,
	[](FAngelscriptBinds& Binds)
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
	});
