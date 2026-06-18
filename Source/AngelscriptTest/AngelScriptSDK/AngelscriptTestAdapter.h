#pragma once

#include "AngelscriptNativeTestSupport.h"

#include "Misc/AutomationTest.h"

#include <cstring>
#include <vector>

namespace AngelscriptSDKTestSupport
{
	using AngelscriptNativeTestSupport::FNativeTestEngine;
	using AngelscriptNativeTestSupport::FSDKBufferedOutStream;

	class FSDKBytecodeStream final : public asIBinaryStream
	{
	public:
		int Write(const void* Ptr, asUINT Size) override
		{
			if (Ptr == nullptr && Size > 0)
			{
				return -1;
			}

			if (Size == 0)
			{
				return 0;
			}

			const asBYTE* BytePtr = static_cast<const asBYTE*>(Ptr);
			Buffer.insert(Buffer.end(), BytePtr, BytePtr + Size);
			return 0;
		}

		int Read(void* Ptr, asUINT Size) override
		{
			if (Ptr == nullptr && Size > 0)
			{
				return -1;
			}

			if (ReadOffset + Size > Buffer.size())
			{
				return -1;
			}

			if (Size > 0)
			{
				std::memcpy(Ptr, Buffer.data() + ReadOffset, Size);
				ReadOffset += Size;
			}

			return 0;
		}

		void Restart()
		{
			ReadOffset = 0;
		}

		int32 Num() const
		{
			return static_cast<int32>(Buffer.size());
		}

	private:
		std::vector<asBYTE> Buffer;
		size_t ReadOffset = 0;
	};

	inline int SDKExecuteString(asIScriptEngine* Engine, asIScriptModule* Module, const char* Code)
	{
		if (Engine == nullptr || Module == nullptr || Code == nullptr)
		{
			return asINVALID_ARG;
		}

		const bool bLooksLikeStatementSnippet = std::strchr(Code, '{') == nullptr;
		const FString SourceText = bLooksLikeStatementSnippet
			? FString::Printf(TEXT("void __SDKExecuteString() { %s }"), ANSI_TO_TCHAR(Code))
			: FString(ANSI_TO_TCHAR(Code));
		const FTCHARToUTF8 SourceTextUtf8(*SourceText);

		const int AddSectionResult = Module->AddScriptSection("SDKExecuteString", SourceTextUtf8.Get(), SourceTextUtf8.Length());
		if (AddSectionResult < 0)
		{
			return AddSectionResult;
		}

		const int BuildResult = Module->Build();
		if (BuildResult < 0)
		{
			return BuildResult;
		}

		asIScriptFunction* Function = nullptr;
		if (bLooksLikeStatementSnippet)
		{
			Function = Module->GetFunctionByDecl("void __SDKExecuteString()");
		}
		else if (Module->GetFunctionCount() == 1)
		{
			Function = Module->GetFunctionByIndex(0);
		}

		if (Function == nullptr)
		{
			return asNO_FUNCTION;
		}

		asIScriptContext* Context = Engine->CreateContext();
		if (Context == nullptr)
		{
			Function->Release();
			return asERROR;
		}

		const int ExecuteResult = AngelscriptNativeTestSupport::PrepareAndExecute(Context, Function);
		Context->Release();
		Function->Release();
		return ExecuteResult;
	}

	inline int SDKExecuteString(asIScriptEngine* Engine, const char* Code)
	{
		if (Engine == nullptr || Code == nullptr)
		{
			return asINVALID_ARG;
		}

		asIScriptModule* Module = Engine->GetModule("_assdk_exec_", asGM_ALWAYS_CREATE);
		return SDKExecuteString(Engine, Module, Code);
	}
}
