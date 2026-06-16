#pragma once

#include "AngelscriptNativeTestSupport.h"

#include "Misc/AutomationTest.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace AngelscriptSDKTestSupport
{
	inline constexpr asPWORD SDKAdapterUserDataSlot = static_cast<asPWORD>(0x415353444B415054ull);

	struct FSDKBufferedOutStream
	{
		std::string Buffer;

		void Clear()
		{
			Buffer.clear();
		}

		void Callback(asSMessageInfo* MessageInfo)
		{
			if (MessageInfo == nullptr)
			{
				return;
			}

			const char* MessageType = "Info   ";
			switch (MessageInfo->type)
			{
			case asMSGTYPE_ERROR:
				MessageType = "Error  ";
				break;
			case asMSGTYPE_WARNING:
				MessageType = "Warning";
				break;
			case asMSGTYPE_INFORMATION:
			default:
				MessageType = "Info   ";
				break;
			}

			char Formatted[1024];
			std::snprintf(
				Formatted,
				sizeof(Formatted),
				"%s (%d, %d) : %s : %s\n",
				MessageInfo->section != nullptr ? MessageInfo->section : "",
				MessageInfo->row,
				MessageInfo->col,
				MessageType,
				MessageInfo->message != nullptr ? MessageInfo->message : "");

			Formatted[sizeof(Formatted) - 1] = '\0';
			Buffer += Formatted;
		}
	};

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

	struct FAngelscriptSDKTestAdapter
	{
		explicit FAngelscriptSDKTestAdapter(FAutomationTestBase& InTest)
			: Test(InTest)
		{
		}

		void Fail(const TCHAR* Reason, const char* File, int Line)
		{
			bFailed = true;
			Test.AddError(FString::Printf(TEXT("%s [%hs:%d]"), Reason, File, Line));
		}

		void Fail(const char* Reason, const char* File, int Line)
		{
			Fail(ANSI_TO_TCHAR(Reason != nullptr ? Reason : "<null reason>"), File, Line);
		}

		void Fail(const FString& Reason, const char* File, int Line)
		{
			Fail(*Reason, File, Line);
		}

		void FailWithContext(const TCHAR* Reason, asIScriptContext* Context, const char* File, int Line)
		{
			bFailed = true;

			FString Message = FString::Printf(TEXT("%s [%hs:%d]"), Reason, File, Line);
			if (Context != nullptr)
			{
				int Column = 0;
				const char* SectionName = nullptr;
				const int ScriptLine = Context->GetLineNumber(0, &Column, &SectionName);

				if (asIScriptFunction* Function = Context->GetFunction())
				{
					Message += FString::Printf(
						TEXT(" Function=%hs Module=%hs"),
						Function->GetDeclaration(),
						Function->GetModuleName() != nullptr ? Function->GetModuleName() : "<anonymous>");
				}

				Message += FString::Printf(
					TEXT(" Section=%hs Line=%d Column=%d"),
					SectionName != nullptr ? SectionName : "",
					ScriptLine,
					Column);
			}

			Test.AddError(Message);
		}

		FAutomationTestBase& Test;
		bool bFailed = false;
	};

	inline FAngelscriptSDKTestAdapter* GetSDKAdapter(asIScriptEngine* Engine)
	{
		return Engine != nullptr
			? static_cast<FAngelscriptSDKTestAdapter*>(Engine->GetUserData(SDKAdapterUserDataSlot))
			: nullptr;
	}

	inline void SDKAssert_Generic(asIScriptGeneric* Generic)
	{
		if (Generic == nullptr)
		{
			return;
		}

		const bool bExpression = sizeof(bool) == 1
			? Generic->GetArgByte(0) != 0
			: Generic->GetArgDWord(0) != 0;

		if (bExpression)
		{
			return;
		}

		asIScriptContext* Context = asGetActiveContext();
		if (FAngelscriptSDKTestAdapter* Adapter = GetSDKAdapter(Generic->GetEngine()))
		{
			Adapter->FailWithContext(TEXT("SDK Assert(false) triggered"), Context, __FILE__, __LINE__);
		}

		if (Context != nullptr)
		{
			Context->SetException("Assert failed");
		}
	}

	inline int RegisterSDKAssert(asIScriptEngine* Engine, FAngelscriptSDKTestAdapter& Adapter)
	{
		if (Engine == nullptr)
		{
			Adapter.Fail(TEXT("RegisterSDKAssert requires a valid script engine"), __FILE__, __LINE__);
			return asINVALID_ARG;
		}

		Engine->SetUserData(&Adapter, SDKAdapterUserDataSlot);
		return Engine->RegisterGlobalFunction("void assert(bool bCondition)", asFUNCTION(SDKAssert_Generic), asCALL_GENERIC);
	}

	inline asIScriptEngine* CreateSDKTestEngine(FAngelscriptSDKTestAdapter& Adapter, FSDKBufferedOutStream* BufferedOutStream = nullptr)
	{
		asIScriptEngine* ScriptEngine = AngelscriptNativeTestSupport::CreateNativeEngine();
		if (ScriptEngine == nullptr)
		{
			Adapter.Fail(TEXT("CreateSDKTestEngine should create a standalone AngelScript engine"), __FILE__, __LINE__);
			return nullptr;
		}

		if (BufferedOutStream != nullptr)
		{
			BufferedOutStream->Clear();
			const int CallbackResult = ScriptEngine->SetMessageCallback(
				asMETHODPR(FSDKBufferedOutStream, Callback, (asSMessageInfo*), void),
				BufferedOutStream,
				asCALL_THISCALL);
			if (CallbackResult < 0)
			{
				Adapter.Fail(TEXT("CreateSDKTestEngine should install the buffered output callback"), __FILE__, __LINE__);
				AngelscriptNativeTestSupport::DestroyNativeEngine(ScriptEngine);
				return nullptr;
			}
		}

		const int RegisterResult = RegisterSDKAssert(ScriptEngine, Adapter);
		if (RegisterResult < 0)
		{
			Adapter.Fail(
				FString::Printf(TEXT("CreateSDKTestEngine should register script-side Assert(bool) (RegisterResult=%d)"), RegisterResult),
				__FILE__,
				__LINE__);
			AngelscriptNativeTestSupport::DestroyNativeEngine(ScriptEngine);
			return nullptr;
		}

		return ScriptEngine;
	}

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

#define SDK_TEST_FAILED(Adapter, Reason) \
	do \
	{ \
		(Adapter).Fail((Reason), __FILE__, __LINE__); \
	} while (false)
