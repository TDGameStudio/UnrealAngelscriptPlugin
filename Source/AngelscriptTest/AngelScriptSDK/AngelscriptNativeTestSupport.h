#pragma once

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

#include <cstring>

#include "StartAngelscriptHeaders.h"
#include "source/as_builder.h"
#include "source/as_bytecode.h"
#include "source/as_parser.h"
#include "source/as_scriptcode.h"
#include "source/as_scriptnode.h"
#include "source/as_tokenizer.h"
#include "EndAngelscriptHeaders.h"

namespace AngelscriptNativeTestSupport
{
	struct FNativeMessageEntry
	{
		FString Section;
		int32 Row = 0;
		int32 Column = 0;
		asEMsgType Type = asMSGTYPE_INFORMATION;
		FString Message;
	};

	struct FNativeMessageCollector
	{
		TArray<FNativeMessageEntry> Entries;

		void Reset()
		{
			Entries.Reset();
		}
	};

	inline const TCHAR* ToMessageTypeString(const asEMsgType Type)
	{
		switch (Type)
		{
		case asMSGTYPE_ERROR:
			return TEXT("Error");
		case asMSGTYPE_WARNING:
			return TEXT("Warning");
		case asMSGTYPE_INFORMATION:
		default:
			return TEXT("Info");
		}
	}

	inline void CaptureNativeMessage(const asSMessageInfo* MessageInfo, void* UserData)
	{
		if (MessageInfo == nullptr)
		{
			return;
		}

		FNativeMessageCollector* const Collector = static_cast<FNativeMessageCollector*>(UserData);
		if (Collector == nullptr)
		{
			return;
		}

		FNativeMessageEntry Entry;
		Entry.Section = UTF8_TO_TCHAR(MessageInfo->section != nullptr ? MessageInfo->section : "");
		Entry.Row = MessageInfo->row;
		Entry.Column = MessageInfo->col;
		Entry.Type = MessageInfo->type;
		Entry.Message = UTF8_TO_TCHAR(MessageInfo->message != nullptr ? MessageInfo->message : "");
		Collector->Entries.Add(MoveTemp(Entry));
	}

	inline FNativeMessageCollector& GetDefaultMessageCollector()
	{
		static FNativeMessageCollector Collector;
		return Collector;
	}

	inline FString CollectMessages(const FNativeMessageCollector& Collector)
	{
		FString Result;
		for (const FNativeMessageEntry& Entry : Collector.Entries)
		{
			if (!Result.IsEmpty())
			{
				Result += LINE_TERMINATOR;
			}

			Result += FString::Printf(
				TEXT("[%s] %s:%d:%d %s"),
				ToMessageTypeString(Entry.Type),
				Entry.Section.IsEmpty() ? TEXT("<memory>") : *Entry.Section,
				Entry.Row,
				Entry.Column,
				*Entry.Message);
		}

		return Result;
	}

	inline FString CollectFunctionDeclarations(asIScriptModule* Module)
	{
		if (Module == nullptr)
		{
			return TEXT("<null module>");
		}

		FString Result;
		const asUINT FunctionCount = Module->GetFunctionCount();
		for (asUINT FunctionIndex = 0; FunctionIndex < FunctionCount; ++FunctionIndex)
		{
			asIScriptFunction* const Function = Module->GetFunctionByIndex(FunctionIndex);
			if (Function == nullptr)
			{
				continue;
			}

			if (!Result.IsEmpty())
			{
				Result += TEXT(", ");
			}

			Result += UTF8_TO_TCHAR(Function->GetDeclaration());
		}

		return Result.IsEmpty() ? TEXT("<no functions>") : Result;
	}

	inline asIScriptEngine* CreateNativeEngine(FNativeMessageCollector* MessageCollector = nullptr)
	{
		FNativeMessageCollector* const Collector = MessageCollector != nullptr ? MessageCollector : &GetDefaultMessageCollector();
		Collector->Reset();

		asIScriptEngine* const ScriptEngine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
		if (ScriptEngine == nullptr)
		{
			return nullptr;
		}

		ScriptEngine->SetEngineProperty(asEP_ALLOW_UNSAFE_REFERENCES, 1);
		ScriptEngine->SetEngineProperty(asEP_USE_CHARACTER_LITERALS, 1);
		ScriptEngine->SetEngineProperty(asEP_ALLOW_MULTILINE_STRINGS, 1);
		ScriptEngine->SetEngineProperty(asEP_SCRIPT_SCANNER, 1);
		ScriptEngine->SetEngineProperty(asEP_OPTIMIZE_BYTECODE, 1);
		ScriptEngine->SetEngineProperty(asEP_AUTO_GARBAGE_COLLECT, 0);
		ScriptEngine->SetEngineProperty(asEP_ALTER_SYNTAX_NAMED_ARGS, 1);
		ScriptEngine->SetEngineProperty(asEP_DISALLOW_VALUE_ASSIGN_FOR_REF_TYPE, 1);
		ScriptEngine->SetEngineProperty(asEP_ALLOW_IMPLICIT_HANDLE_TYPES, 1);
		ScriptEngine->SetEngineProperty(asEP_REQUIRE_ENUM_SCOPE, 1);
		ScriptEngine->SetEngineProperty(asEP_ALWAYS_IMPL_DEFAULT_CONSTRUCT, 1);
		ScriptEngine->SetEngineProperty(asEP_ALWAYS_IMPL_DEFAULT_COPY, 1);
		ScriptEngine->SetEngineProperty(asEP_ALWAYS_IMPL_DEFAULT_COPY_CONSTRUCT, 1);
		ScriptEngine->SetEngineProperty(asEP_MEMBER_INIT_MODE, 0);
		ScriptEngine->SetEngineProperty(asEP_TYPECHECK_SWITCH_ENUMS, 1);
		ScriptEngine->SetEngineProperty(asEP_ALLOW_DOUBLE_TYPE, 1);

		const int CallbackResult = ScriptEngine->SetMessageCallback(asFUNCTION(CaptureNativeMessage), Collector, asCALL_CDECL);
		if (CallbackResult < 0)
		{
			ScriptEngine->ShutDownAndRelease();
			return nullptr;
		}

		return ScriptEngine;
	}

	inline asCScriptEngine* CreateBareSdkEngine(FAutomationTestBase* Test = nullptr)
	{
		asIScriptEngine* const RawEngine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(RawEngine);
		if (ScriptEngine == nullptr && Test != nullptr)
		{
			Test->AddError(TEXT("Failed to create bare AngelScript SDK engine"));
		}

		return ScriptEngine;
	}

	inline void DestroyNativeEngine(asIScriptEngine* ScriptEngine)
	{
		if (ScriptEngine != nullptr)
		{
			ScriptEngine->ShutDownAndRelease();
		}
	}

	inline int CompileNativeModule(asIScriptEngine* ScriptEngine, const char* ModuleName, const char* Source, asIScriptModule*& OutModule)
	{
		OutModule = nullptr;
		if (ScriptEngine == nullptr || ModuleName == nullptr || Source == nullptr)
		{
			return asINVALID_ARG;
		}

		asIScriptModule* const Module = ScriptEngine->GetModule(ModuleName, asGM_ALWAYS_CREATE);
		if (Module == nullptr)
		{
			return asNO_MODULE;
		}

		OutModule = Module;
		const int AddSectionResult = Module->AddScriptSection(ModuleName, Source, static_cast<unsigned int>(std::strlen(Source)));
		if (AddSectionResult < 0)
		{
			return AddSectionResult;
		}

		return Module->Build();
	}

	inline asIScriptModule* BuildNativeModule(asIScriptEngine* ScriptEngine, const char* ModuleName, const char* Source)
	{
		asIScriptModule* Module = nullptr;
		return CompileNativeModule(ScriptEngine, ModuleName, Source, Module) >= 0 ? Module : nullptr;
	}

	inline asIScriptFunction* GetNativeFunctionByDecl(asIScriptModule* Module, const char* Declaration)
	{
		if (Module == nullptr || Declaration == nullptr)
		{
			return nullptr;
		}

		asIScriptFunction* Function = Module->GetFunctionByDecl(Declaration);
		if (Function != nullptr)
		{
			return Function;
		}

		const FString DeclarationString = UTF8_TO_TCHAR(Declaration);
		int32 OpenParenIndex = INDEX_NONE;
		if (!DeclarationString.FindChar(TEXT('('), OpenParenIndex))
		{
			return nullptr;
		}

		const FString Prefix = DeclarationString.Left(OpenParenIndex).TrimStartAndEnd();
		int32 NameSeparatorIndex = INDEX_NONE;
		if (!Prefix.FindLastChar(TEXT(' '), NameSeparatorIndex))
		{
			return nullptr;
		}

		const FString FunctionName = Prefix.Mid(NameSeparatorIndex + 1).TrimStartAndEnd();
		if (FunctionName.IsEmpty())
		{
			return nullptr;
		}

		const FTCHARToUTF8 FunctionNameUtf8(*FunctionName);
		Function = Module->GetFunctionByName(FunctionNameUtf8.Get());
		if (Function != nullptr)
		{
			return Function;
		}

		const asUINT FunctionCount = Module->GetFunctionCount();
		if (FunctionCount == 1)
		{
			return Module->GetFunctionByIndex(0);
		}

		for (asUINT FunctionIndex = 0; FunctionIndex < FunctionCount; ++FunctionIndex)
		{
			asIScriptFunction* const CandidateFunction = Module->GetFunctionByIndex(FunctionIndex);
			if (CandidateFunction != nullptr && FunctionName.Equals(UTF8_TO_TCHAR(CandidateFunction->GetName())))
			{
				return CandidateFunction;
			}
		}

		return nullptr;
	}

	inline asIScriptFunction* GetNativeFunctionByExactDecl(asIScriptModule* Module, const char* Declaration)
	{
		if (Module == nullptr || Declaration == nullptr)
		{
			return nullptr;
		}

		return Module->GetFunctionByDecl(Declaration);
	}

	inline int PrepareAndExecute(asIScriptContext* Context, asIScriptFunction* Function)
	{
		if (Context == nullptr || Function == nullptr)
		{
			return asINVALID_ARG;
		}

		const int PrepareResult = Context->Prepare(Function);
		return PrepareResult == asSUCCESS ? Context->Execute() : PrepareResult;
	}

	inline TArray<TPair<eTokenType, size_t>> TokenizeAll(const char* Source, size_t SourceLength)
	{
		TArray<TPair<eTokenType, size_t>> Tokens;
		if (Source == nullptr)
		{
			return Tokens;
		}

		struct FTokenizerAccessor : asCTokenizer
		{
			using asCTokenizer::GetToken;
		};

		FTokenizerAccessor Tokenizer;
		size_t TokenLength = 0;
		size_t Offset = 0;
		while (Offset < SourceLength)
		{
			const eTokenType TokenType = Tokenizer.GetToken(Source + Offset, SourceLength - Offset, &TokenLength);
			Tokens.Add(TPair<eTokenType, size_t>(TokenType, TokenLength));
			if (TokenLength == 0)
			{
				break;
			}

			Offset += TokenLength;
		}

		return Tokens;
	}

	inline int32 CountNodesOfType(const asCScriptNode* Node, eScriptNode ExpectedType)
	{
		if (Node == nullptr)
		{
			return 0;
		}

		int32 Count = 0;
		for (const asCScriptNode* Current = Node; Current != nullptr; Current = Current->next)
		{
			if (Current->nodeType == ExpectedType)
			{
				++Count;
			}

			Count += CountNodesOfType(Current->firstChild, ExpectedType);
		}

		return Count;
	}

	inline TMap<eScriptNode, int32> NodeTypeHistogram(const asCScriptNode* Node)
	{
		TMap<eScriptNode, int32> Histogram;
		if (Node == nullptr)
		{
			return Histogram;
		}

		for (const asCScriptNode* Current = Node; Current != nullptr; Current = Current->next)
		{
			int32& Count = Histogram.FindOrAdd(Current->nodeType);
			++Count;

			if (Current->firstChild != nullptr)
			{
				const TMap<eScriptNode, int32> ChildHistogram = NodeTypeHistogram(Current->firstChild);
				for (const TPair<eScriptNode, int32>& Pair : ChildHistogram)
				{
					int32& ChildCount = Histogram.FindOrAdd(Pair.Key);
					ChildCount += Pair.Value;
				}
			}
		}

		return Histogram;
	}

	inline int32 MaxNodeDepth(const asCScriptNode* Node)
	{
		if (Node == nullptr)
		{
			return 0;
		}

		int32 MaxDepth = 0;
		for (const asCScriptNode* Current = Node; Current != nullptr; Current = Current->next)
		{
			const int32 ChildDepth = MaxNodeDepth(Current->firstChild);
			MaxDepth = FMath::Max(MaxDepth, 1 + ChildDepth);
		}

		return MaxDepth;
	}

	inline FString DumpBytecodeOpcodes(asCByteCode& ByteCode)
	{
		TArray<asDWORD> Buffer;
		const int32 Size = ByteCode.GetSize();
		if (Size <= 0)
		{
			return TEXT("<empty bytecode>");
		}

		Buffer.SetNumZeroed(Size);
		ByteCode.Output(Buffer.GetData());

		FString Result;
		for (int32 Index = 0; Index < Size; ++Index)
		{
			if (!Result.IsEmpty())
			{
				Result += TEXT(", ");
			}

			Result += FString::Printf(TEXT("%u"), static_cast<uint32>(Buffer[Index]));
		}

		return Result;
	}

	inline TArray<asDWORD> EmitToBuffer(asCByteCode& ByteCode)
	{
		TArray<asDWORD> Buffer;
		const int32 Size = ByteCode.GetSize();
		if (Size <= 0)
		{
			return Buffer;
		}

		Buffer.SetNumZeroed(Size);
		ByteCode.Output(Buffer.GetData());
		return Buffer;
	}
}
