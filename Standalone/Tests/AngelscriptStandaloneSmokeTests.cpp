#include "angelscript.h"

#include <cstdint>
#include <cstring>
#include <iostream>
#include <utility>
#include <vector>

namespace
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

		int Read(void* Data, asUINT Size) override
		{
			if (ReadOffset + Size > Buffer.size())
			{
				bReadOverflow = true;
				std::memset(Data, 0, Size);
				return -1;
			}

			std::memcpy(Data, Buffer.data() + ReadOffset, Size);
			ReadOffset += Size;
			return 0;
		}

		void Rewind()
		{
			ReadOffset = 0;
			bReadOverflow = false;
		}

		std::vector<std::uint8_t> Buffer;
		std::size_t ReadOffset = 0;
		bool bReadOverflow = false;
	};

	bool Require(bool bCondition, const char* Message)
	{
		if (!bCondition)
		{
			std::cerr << Message << '\n';
		}
		return bCondition;
	}
}

int main()
{
	asIScriptEngine* Engine = asCreateScriptEngine();
	if (!Require(Engine != nullptr, "asCreateScriptEngine returned null"))
	{
		return 1;
	}

	bool bPassed = true;
	asIScriptModule* SourceModule = Engine->GetModule("smoke", asGM_ALWAYS_CREATE);
	const char* Source = "int main() { return 42; }";
	bPassed &= Require(SourceModule != nullptr, "GetModule failed");
	bPassed &= Require(SourceModule->AddScriptSection("smoke.as", Source) >= 0, "AddScriptSection failed");
	bPassed &= Require(SourceModule->Build() >= 0, "Build failed");

	FMemoryByteCodeStream Stream;
	bPassed &= Require(SourceModule->SaveByteCode(&Stream) >= 0, "SaveByteCode failed");
	bPassed &= Require(!Stream.Buffer.empty(), "SaveByteCode produced no data");

	asIScriptModule* LoadedModule = Engine->GetModule("loaded", asGM_ALWAYS_CREATE);
	Stream.Rewind();
	bPassed &= Require(LoadedModule->LoadByteCode(&Stream) >= 0, "LoadByteCode failed");
	bPassed &= Require(!Stream.bReadOverflow, "LoadByteCode read past the bytecode buffer");

	asIScriptFunction* EntryPoint = LoadedModule->GetFunctionByDecl("int main()");
	bPassed &= Require(EntryPoint != nullptr, "main entry point was not restored");

	asIScriptContext* Context = Engine->CreateContext();
	bPassed &= Require(Context != nullptr, "CreateContext failed");
	if (Context != nullptr && EntryPoint != nullptr)
	{
		bPassed &= Require(Context->Prepare(EntryPoint) >= 0, "Prepare failed");
		bPassed &= Require(Context->Execute() == asEXECUTION_FINISHED, "Execute did not finish");
		bPassed &= Require(Context->GetReturnDWord() == 42, "main returned the wrong result");
		Context->Release();
	}

	asIScriptModule* StructModule =
		Engine->GetModule("struct-source", asGM_ALWAYS_CREATE);
	const char* StructSource =
		"struct FValue { int Value = 41; }\n"
		"int structMain() { FValue Value; return Value.Value + 1; }\n";
	bPassed &= Require(
		StructModule != nullptr
			&& StructModule->AddScriptSection(
				"struct.as",
				StructSource) >= 0
			&& StructModule->Build() >= 0,
		"script struct source did not compile");
	FMemoryByteCodeStream StructStream;
	bPassed &= Require(
		StructModule != nullptr
			&& StructModule->SaveByteCode(&StructStream) >= 0
			&& !StructStream.Buffer.empty(),
		"script struct bytecode did not save");
	asIScriptModule* LoadedStructModule =
		Engine->GetModule("struct-loaded", asGM_ALWAYS_CREATE);
	StructStream.Rewind();
	bPassed &= Require(
		LoadedStructModule != nullptr
			&& LoadedStructModule->LoadByteCode(&StructStream) >= 0
			&& !StructStream.bReadOverflow,
		"script struct bytecode did not restore");
	asIScriptFunction* StructEntryPoint =
		LoadedStructModule != nullptr
			? LoadedStructModule->GetFunctionByDecl("int structMain()")
			: nullptr;
	asIScriptContext* StructContext = Engine->CreateContext();
	bPassed &= Require(
		StructEntryPoint != nullptr && StructContext != nullptr,
		"restored script struct entry point is missing");
	if (StructEntryPoint != nullptr && StructContext != nullptr)
	{
		bPassed &= Require(
			StructContext->Prepare(StructEntryPoint) >= 0
				&& StructContext->Execute() == asEXECUTION_FINISHED
				&& StructContext->GetReturnDWord() == 42,
			"restored script struct returned the wrong result");
	}
	if (StructContext != nullptr)
	{
		StructContext->Release();
	}

	Engine->ShutDownAndRelease();
	return bPassed ? 0 : 1;
}
