// ============================================================================
// AngelscriptMemoryReaderBindingsTests.cpp
//
// FMemoryReader binding coverage — CQTest refactor.
// Automation ID:
//   Angelscript.TestModule.Bindings.MemoryReader.FAngelscriptMemoryReaderBindingsTest.*
//
// Sections:
//   ReadOperations  — TotalSize, Tell, Seek, Skip, ReadUInt8, ReadUInt16,
//                     ReadInt32, ReadBytes, ReadAnsiString
//   OutOfBoundsSkip — exception on skip past array bounds
//
// CQTest adaptation notes:
//   The monolithic `int Entry()` has been split into self-contained per-
//   operation functions, each returning 1 (pass) or 0 (fail).  A shared
//   MakeTestData() AS helper constructs the canonical 12-byte buffer.
//   The out-of-bounds skip test uses a no-arg wrapper so that
//   ExecuteFunctionExpectingScriptException can validate the exception path.
// ============================================================================

#include "CQTest.h"
#include "Shared/AngelscriptTestMacros.h"
#include "Shared/AngelscriptBindingsCoverage.h"
#include "Shared/AngelscriptBindingsModuleBuilder.h"
#include "Shared/AngelscriptBindingsAssertions.h"

#include "Serialization/MemoryReader.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptTestSupport;
using namespace AngelscriptTestBindings;
using namespace AngelscriptReflectiveAccess;

// ----------------------------------------------------------------------------
// Profile
// ----------------------------------------------------------------------------

static const FBindingsCoverageProfile GMemoryReaderProfile{
	TEXT("MemoryReader"),            // Theme
	TEXT(""),                        // Variant
	TEXT("ASMemReader"),             // ModulePrefix
	TEXT("MemReader"),               // CasePrefix
	TEXT("MemoryReaderBindings"),    // LogCategory
};

// ----------------------------------------------------------------------------
// Test class
// ----------------------------------------------------------------------------

TEST_CLASS_WITH_FLAGS(FAngelscriptMemoryReaderBindingsTest,
	"Angelscript.TestModule.Bindings.MemoryReader",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	BEFORE_ALL()
	{
		ASTEST_CREATE_ENGINE();
	}

	AFTER_ALL() { FAngelscriptEngine& Engine = ASTEST_GET_ENGINE(); ASTEST_RESET_ENGINE(Engine); }

	// ====================================================================
	// Section: ReadOperations
	// ====================================================================

	TEST_METHOD(ReadOperations)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FCoverageModuleScope Mod(*TestRunner, Engine, GMemoryReaderProfile, TEXT("ReadOps"), TEXT(R"(
TArray<uint8> MakeTestData()
{
	TArray<uint8> Data;
	Data.Add(0x41); Data.Add(0x42); Data.Add(0x10); Data.Add(0x00);
	Data.Add(0x78); Data.Add(0x56); Data.Add(0x34); Data.Add(0x12);
	Data.Add(0x43); Data.Add(0x44); Data.Add(0x45); Data.Add(0x46);
	return Data;
}

int MemReader_TotalSize()
{
	TArray<uint8> Data = MakeTestData();
	FMemoryReader Reader(Data);
	return (Reader.TotalSize() == 12) ? 1 : 0;
}

int MemReader_InitialTell()
{
	TArray<uint8> Data = MakeTestData();
	FMemoryReader Reader(Data);
	return (Reader.Tell() == 0) ? 1 : 0;
}

int MemReader_ReadUInt8()
{
	TArray<uint8> Data = MakeTestData();
	FMemoryReader Reader(Data);
	return (Reader.ReadUInt8() == 0x41 && Reader.Tell() == 1) ? 1 : 0;
}

int MemReader_ReadUInt16()
{
	TArray<uint8> Data = MakeTestData();
	FMemoryReader Reader(Data);
	Reader.ReadUInt8();
	return (Reader.ReadUInt16() == 0x1042 && Reader.Tell() == 3) ? 1 : 0;
}

int MemReader_SeekAndReadInt32()
{
	TArray<uint8> Data = MakeTestData();
	FMemoryReader Reader(Data);
	Reader.Seek(4);
	return (Reader.Tell() == 4 && Reader.ReadInt32() == 0x12345678 && Reader.Tell() == 8) ? 1 : 0;
}

int MemReader_ReadBytes()
{
	TArray<uint8> Data = MakeTestData();
	FMemoryReader Reader(Data);
	Reader.Seek(8);
	TArray<uint8> TailBytes = Reader.ReadBytes(4);
	if (TailBytes.Num() != 4) return 0;
	if (TailBytes[0] != 0x43 || TailBytes[1] != 0x44 || TailBytes[2] != 0x45 || TailBytes[3] != 0x46) return 0;
	return (Reader.Tell() == 12) ? 1 : 0;
}

int MemReader_Skip()
{
	TArray<uint8> Data = MakeTestData();
	FMemoryReader Reader(Data);
	Reader.Seek(2);
	Reader.Skip(2);
	return (Reader.Tell() == 4) ? 1 : 0;
}

int MemReader_ReadAnsiString()
{
	TArray<uint8> Data = MakeTestData();
	FMemoryReader Reader(Data);
	Reader.Seek(8);
	return (Reader.ReadAnsiString(4) == "CDEF" && Reader.Tell() == 12) ? 1 : 0;
}
)"));
		if (!Mod.IsValid()) return;
		auto& M = Mod.GetModule();

		ExpectGlobalInt(*TestRunner, Engine, M, GMemoryReaderProfile, TEXT("int MemReader_TotalSize()"), TEXT("TotalSize should return 12 for 12-byte buffer"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M, GMemoryReaderProfile, TEXT("int MemReader_InitialTell()"), TEXT("Tell should return 0 on a fresh reader"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M, GMemoryReaderProfile, TEXT("int MemReader_ReadUInt8()"), TEXT("ReadUInt8 should read first byte and advance cursor"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M, GMemoryReaderProfile, TEXT("int MemReader_ReadUInt16()"), TEXT("ReadUInt16 should read two bytes and advance cursor"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M, GMemoryReaderProfile, TEXT("int MemReader_SeekAndReadInt32()"), TEXT("Seek + ReadInt32 should read four bytes at offset"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M, GMemoryReaderProfile, TEXT("int MemReader_ReadBytes()"), TEXT("ReadBytes should read tail bytes correctly"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M, GMemoryReaderProfile, TEXT("int MemReader_Skip()"), TEXT("Skip should advance cursor without reading"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M, GMemoryReaderProfile, TEXT("int MemReader_ReadAnsiString()"), TEXT("ReadAnsiString should read ASCII characters correctly"), 1);
	}

	// ====================================================================
	// Section: OutOfBoundsSkip
	// ====================================================================

	TEST_METHOD(OutOfBoundsSkip)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FCoverageModuleScope Mod(*TestRunner, Engine, GMemoryReaderProfile, TEXT("InvalidSkip"), TEXT(R"(
void MemReader_TriggerInvalidSkip()
{
	TArray<uint8> Data;
	for (uint8 i = 0; i < 8; i++)
		Data.Add(i);
	FMemoryReader Reader(Data);
	Reader.Seek(7);
	Reader.Skip(2);
}
)"));
		if (!Mod.IsValid()) return;
		auto& M = Mod.GetModule();

		TestRunner->AddExpectedError(TEXT("Skipping past array bounds"), EAutomationExpectedErrorFlags::Contains, 0);
		TestRunner->AddExpectedError(*Mod.GetModuleName(), EAutomationExpectedErrorFlags::Contains, 0);
		TestRunner->AddExpectedError(TEXT("void MemReader_TriggerInvalidSkip()"), EAutomationExpectedErrorFlags::Contains, 0, false);

		ExecuteFunctionExpectingScriptException(*TestRunner, Engine, M, GMemoryReaderProfile,
			TEXT("void MemReader_TriggerInvalidSkip()"),
			TEXT("out-of-bounds skip should surface a runtime exception"),
			TEXT("Skipping past array bounds"));
	}
};

#endif
