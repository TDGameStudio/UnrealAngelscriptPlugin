#pragma once

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"

#include <cstdio>
#include <cstring>
#include <string>

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
	inline constexpr asPWORD NativeTestEngineUserDataSlot = static_cast<asPWORD>(0x4E41544956454153ull);

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

	struct FNativeTestEngine;

	inline FNativeTestEngine* GetNativeTestEngine(asIScriptEngine* Engine)
	{
		return Engine != nullptr
			? static_cast<FNativeTestEngine*>(Engine->GetUserData(NativeTestEngineUserDataSlot))
			: nullptr;
	}

	inline void NativeTestAssert_Generic(asIScriptGeneric* Generic);

	struct FNativeTestEngine
	{
		void Create(FAutomationTestBase& Test, FSDKBufferedOutStream* InBufferedOutStream = nullptr)
		{
			if (ScriptEngine != nullptr)
			{
				Destroy();
			}

			CurrentTest = &Test;
			BufferedOutStream = InBufferedOutStream;
			ScriptEngine = CreateNativeEngine(&Messages);
			if (!Test.TestNotNull(TEXT("Native test engine should create a shared raw engine"), ScriptEngine))
			{
				return;
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
					Fail(TEXT("Native test engine should install the buffered output callback"), __FILE__, __LINE__);
					Destroy();
					return;
				}
			}

			ScriptEngine->SetUserData(this, NativeTestEngineUserDataSlot);
			const int RegisterAssertResult = ScriptEngine->RegisterGlobalFunction(
				"void assert(bool bCondition)",
				asFUNCTION(NativeTestAssert_Generic),
				asCALL_GENERIC);
			if (RegisterAssertResult < 0)
			{
				Fail(
					FString::Printf(TEXT("Native test engine should register script-side assert(bool) (Result=%d)"), RegisterAssertResult),
					__FILE__,
					__LINE__);
				Destroy();
			}
		}

		void Destroy()
		{
			DestroyNativeEngine(ScriptEngine);
			ScriptEngine = nullptr;
			CurrentTest = nullptr;
			BufferedOutStream = nullptr;
			bFailed = false;
			Messages.Reset();
		}

		void Reset(FAutomationTestBase& Test)
		{
			CurrentTest = &Test;
			bFailed = false;
			ResetMessages();
			if (BufferedOutStream != nullptr)
			{
				BufferedOutStream->Clear();
			}
		}

		void ResetMessages()
		{
			Messages.Reset();
		}

		asIScriptEngine* Get() const
		{
			return ScriptEngine;
		}

		FNativeMessageCollector& GetMessages()
		{
			return Messages;
		}

		const FNativeMessageCollector& GetMessages() const
		{
			return Messages;
		}

		FString GetMessagesText() const
		{
			return CollectMessages(Messages);
		}

		bool HasFailed() const
		{
			return bFailed;
		}

		FSDKBufferedOutStream* GetBufferedOutStream() const
		{
			return BufferedOutStream;
		}

		void Fail(const TCHAR* Reason, const char* File, int Line)
		{
			bFailed = true;
			if (CurrentTest != nullptr)
			{
				CurrentTest->AddError(FString::Printf(TEXT("%s [%hs:%d]"), Reason, File, Line));
			}
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

			if (CurrentTest != nullptr)
			{
				CurrentTest->AddError(Message);
			}
		}

	private:
		FNativeMessageCollector Messages;
		FAutomationTestBase* CurrentTest = nullptr;
		FSDKBufferedOutStream* BufferedOutStream = nullptr;
		asIScriptEngine* ScriptEngine = nullptr;
		bool bFailed = false;
	};

	inline void NativeTestAssert_Generic(asIScriptGeneric* Generic)
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
		if (FNativeTestEngine* Engine = GetNativeTestEngine(Generic->GetEngine()))
		{
			Engine->FailWithContext(TEXT("SDK Assert(false) triggered"), Context, __FILE__, __LINE__);
		}

		if (Context != nullptr)
		{
			Context->SetException("Assert failed");
		}
	}

	struct FScopedNativeModule
	{
		FScopedNativeModule(
			FAutomationTestBase& InTest,
			FNativeTestEngine& InEngine,
			const char* InModuleName,
			const char* Source)
			: Test(InTest)
			, Engine(InEngine)
			, ModuleName(InModuleName != nullptr ? InModuleName : "")
		{
			asIScriptEngine* const ScriptEngine = Engine.Get();
			if (!Test.TestNotNull(TEXT("Native module scope should have a script engine"), ScriptEngine))
			{
				return;
			}

			Module = BuildNativeModule(ScriptEngine, ModuleName.c_str(), Source);
			if (!Test.TestNotNull(TEXT("Native module scope should compile the module"), Module))
			{
				const FString Messages = Engine.GetMessagesText();
				if (!Messages.IsEmpty())
				{
					Test.AddInfo(Messages);
				}
			}
		}

		~FScopedNativeModule()
		{
			asIScriptEngine* const ScriptEngine = Engine.Get();
			if (ScriptEngine != nullptr && !ModuleName.empty())
			{
				ScriptEngine->DiscardModule(ModuleName.c_str());
			}
		}

		FScopedNativeModule(const FScopedNativeModule&) = delete;
		FScopedNativeModule& operator=(const FScopedNativeModule&) = delete;

		bool IsValid() const
		{
			return Module != nullptr;
		}

		asIScriptModule* Get() const
		{
			return Module;
		}

		asIScriptModule* operator->() const
		{
			return Module;
		}

		operator asIScriptModule*() const
		{
			return Module;
		}

		const char* GetModuleName() const
		{
			return ModuleName.c_str();
		}

	private:
		FAutomationTestBase& Test;
		FNativeTestEngine& Engine;
		std::string ModuleName;
		asIScriptModule* Module = nullptr;
	};

	struct FScopedNativeModuleName
	{
		FScopedNativeModuleName(FNativeTestEngine& InEngine, const char* InModuleName)
			: Engine(InEngine)
			, ModuleName(InModuleName != nullptr ? InModuleName : "")
		{
		}

		~FScopedNativeModuleName()
		{
			asIScriptEngine* const ScriptEngine = Engine.Get();
			if (ScriptEngine != nullptr && !ModuleName.empty())
			{
				ScriptEngine->DiscardModule(ModuleName.c_str());
			}
		}

		FScopedNativeModuleName(const FScopedNativeModuleName&) = delete;
		FScopedNativeModuleName& operator=(const FScopedNativeModuleName&) = delete;

		const char* Get() const
		{
			return ModuleName.c_str();
		}

	private:
		FNativeTestEngine& Engine;
		std::string ModuleName;
	};

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

	inline const asCScriptNode* FindFirstNodeOfType(const asCScriptNode* Node, const eScriptNode Type)
	{
		for (const asCScriptNode* Current = Node; Current != nullptr; Current = Current->next)
		{
			if (Current->nodeType == Type)
			{
				return Current;
			}

			if (const asCScriptNode* Child = FindFirstNodeOfType(Current->firstChild, Type))
			{
				return Child;
			}
		}

		return nullptr;
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

	// -------------------------------------------------------------------------
	// Shared SDK test accessors
	// -------------------------------------------------------------------------
	// These helper types were previously duplicated (with identical or
	// near-identical bodies) across many AngelScriptSDK test files, each wrapped
	// in a uniquely-named namespace to avoid Unity Build collisions. They are
	// consolidated here so every test can share a single definition.

	// Exposes asCTokenizer::GetToken for tokenizer tests.
	struct FTokenizerAccessor : asCTokenizer
	{
		using asCTokenizer::GetToken;
	};

	// Creates (or replaces) a module on the given bare SDK engine.
	inline asCModule* CreateSdkModule(asCScriptEngine* ScriptEngine, const char* ModuleName)
	{
		return static_cast<asCModule*>(ScriptEngine->GetModule(ModuleName, asGM_ALWAYS_CREATE));
	}

	// Superset parser accessor: exposes every snippet-parse helper that the
	// individual parser/script-node tests need. Methods unused by a given test
	// are simply never called.
	struct FParserAccessor : asCParser
	{
		explicit FParserAccessor(asCBuilder* Builder)
			: asCParser(Builder)
		{
		}

		void ResetParser()
		{
			Reset();
		}

		asCScriptNode* ParseExpressionSnippet(asCScriptCode* InScript)
		{
			Reset();
			script = InScript;
			return ParseExpression();
		}

		asCScriptNode* ParseAssignmentSnippet(asCScriptCode* InScript)
		{
			Reset();
			script = InScript;
			return ParseAssignment();
		}

		asCScriptNode* ParseConditionSnippet(asCScriptCode* InScript)
		{
			Reset();
			script = InScript;
			return ParseCondition();
		}

		asCScriptNode* ParseStatementSnippet(asCScriptCode* InScript)
		{
			Reset();
			script = InScript;
			return ParseStatement();
		}

		int ParseScriptSnippetWithoutImplicitReset(asCScriptCode* InScript)
		{
			script = InScript;
			scriptNode = asCParser::ParseScript(false);
			return errorWhileParsing ? -1 : 0;
		}
	};

	// Shared bytecode test fixture: owns a bare engine, module, builder, and an
	// empty asCByteCode. Pass bOptimize=true to enable bytecode optimization.
	// Previously duplicated across the bytecode test files.
	struct FBytecodeFixture
	{
		explicit FBytecodeFixture(const char* ModuleName, bool bOptimize = false)
		{
			Engine = CreateBareSdkEngine();
			if (Engine != nullptr && bOptimize)
			{
				Engine->SetEngineProperty(asEP_OPTIMIZE_BYTECODE, 1);
			}

			Module = Engine != nullptr ? static_cast<asCModule*>(Engine->GetModule(ModuleName, asGM_ALWAYS_CREATE)) : nullptr;
			Builder = Module != nullptr ? new asCBuilder(Engine, Module) : nullptr;
			ByteCode = Builder != nullptr ? new asCByteCode(Builder) : nullptr;
		}

		~FBytecodeFixture()
		{
			delete ByteCode;
			delete Builder;
			if (Engine != nullptr)
			{
				Engine->ShutDownAndRelease();
			}
		}

		bool IsValid(FAutomationTestBase& Test) const
		{
			return Test.TestNotNull(TEXT("Bytecode fixture should create a bare engine"), Engine)
				&& Test.TestNotNull(TEXT("Bytecode fixture should create a module"), Module)
				&& Test.TestNotNull(TEXT("Bytecode fixture should create a builder"), Builder)
				&& Test.TestNotNull(TEXT("Bytecode fixture should create bytecode"), ByteCode);
		}

		asCScriptEngine* Engine = nullptr;
		asCModule* Module = nullptr;
		asCBuilder* Builder = nullptr;
		asCByteCode* ByteCode = nullptr;
	};

	inline int32 CountInstructions(asCByteCode& ByteCode)
	{
		int32 Count = 0;
		for (const asCByteInstruction* Instruction = ByteCode.GetFirstInstr(); Instruction != nullptr; Instruction = Instruction->next)
		{
			++Count;
		}
		return Count;
	}

	inline bool ContainsOpcode(asCByteCode& ByteCode, const asEBCInstr Opcode)
	{
		for (const asCByteInstruction* Instruction = ByteCode.GetFirstInstr(); Instruction != nullptr; Instruction = Instruction->next)
		{
			if (Instruction->op == Opcode)
			{
				return true;
			}
		}

		return false;
	}

	// Shared in-memory binary stream for SaveByteCode/LoadByteCode round-trip
	// tests. Exposes both Truncate(NewSize) and TruncateBy(BytesToRemove) so it
	// covers every caller previously using a per-file copy.
	class FMemoryBinaryStream final : public asIBinaryStream
	{
	public:
		int Write(const void* Ptr, asUINT Size) override
		{
			if (Ptr == nullptr)
			{
				return asINVALID_ARG;
			}

			const int32 Start = Bytes.Num();
			Bytes.AddUninitialized(static_cast<int32>(Size));
			FMemory::Memcpy(Bytes.GetData() + Start, Ptr, static_cast<SIZE_T>(Size));
			return asSUCCESS;
		}

		int Read(void* Ptr, asUINT Size) override
		{
			if (Ptr == nullptr)
			{
				return asINVALID_ARG;
			}

			if (Bytes.Num() - ReadOffset < static_cast<int32>(Size))
			{
				return asERROR;
			}

			FMemory::Memcpy(Ptr, Bytes.GetData() + ReadOffset, static_cast<SIZE_T>(Size));
			ReadOffset += static_cast<int32>(Size);
			return asSUCCESS;
		}

		void ResetReadOffset()
		{
			ReadOffset = 0;
		}

		// Remove a fixed number of bytes from the end.
		void TruncateBy(int32 BytesToRemove)
		{
			Bytes.SetNum(FMath::Max(0, Bytes.Num() - BytesToRemove), EAllowShrinking::No);
			ReadOffset = FMath::Min(ReadOffset, Bytes.Num());
		}

		// Shrink the buffer to an absolute size.
		void Truncate(int32 NewSize)
		{
			Bytes.SetNum(FMath::Max(NewSize, 0), EAllowShrinking::No);
			ReadOffset = FMath::Min(ReadOffset, Bytes.Num());
		}

		int32 Num() const
		{
			return Bytes.Num();
		}

	private:
		TArray<uint8> Bytes;
		int32 ReadOffset = 0;
	};

	// Returns true if the collector captured an error message containing Needle.
	inline bool ContainsError(const FNativeMessageCollector& Messages, const TCHAR* Needle)
	{
		for (const FNativeMessageEntry& Entry : Messages.Entries)
		{
			if (Entry.Type == asMSGTYPE_ERROR && Entry.Message.Contains(Needle))
			{
				return true;
			}
		}

		return false;
	}

	// Compiles a snippet on a throwaway native engine and returns the build code,
	// capturing diagnostics into Messages. The engine is destroyed before return.
	inline int CompileSnippet(const char* ModuleName, const char* Source, FNativeMessageCollector& Messages)
	{
		asIScriptEngine* ScriptEngine = CreateNativeEngine(&Messages);
		if (ScriptEngine == nullptr)
		{
			return asERROR;
		}

		ON_SCOPE_EXIT
		{
			DestroyNativeEngine(ScriptEngine);
		};

		asIScriptModule* Module = nullptr;
		return CompileNativeModule(ScriptEngine, ModuleName, Source, Module);
	}
}
