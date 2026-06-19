#include "../../AngelscriptRuntime/Core/AngelscriptInclude.h"

#include "CQTest.h"
#include "Misc/ScopeExit.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace AngelscriptTest_Core_AngelscriptHeaderShimTests_Private
{
	struct FNativeMessageEntry
	{
		FString Section;
		int32 Row = 0;
		int32 Column = 0;
		FString Message;
	};

	struct FNativeMessageCollector
	{
		TArray<FNativeMessageEntry> Entries;

		static void Callback(const asSMessageInfo* MessageInfo, void* UserData)
		{
			if (MessageInfo == nullptr || UserData == nullptr)
			{
				return;
			}

			FNativeMessageCollector* Collector = static_cast<FNativeMessageCollector*>(UserData);
			FNativeMessageEntry Entry;
			Entry.Section = UTF8_TO_TCHAR(MessageInfo->section != nullptr ? MessageInfo->section : "");
			Entry.Row = MessageInfo->row;
			Entry.Column = MessageInfo->col;
			Entry.Message = UTF8_TO_TCHAR(MessageInfo->message != nullptr ? MessageInfo->message : "");
			Collector->Entries.Add(MoveTemp(Entry));
		}

		FString Format() const
		{
			FString Result;
			for (const FNativeMessageEntry& Entry : Entries)
			{
				if (!Result.IsEmpty())
				{
					Result += LINE_TERMINATOR;
				}

				Result += FString::Printf(
					TEXT("%s:%d:%d %s"),
					Entry.Section.IsEmpty() ? TEXT("<memory>") : *Entry.Section,
					Entry.Row,
					Entry.Column,
					*Entry.Message);
			}

			return Result.IsEmpty() ? TEXT("<no native AngelScript diagnostics>") : Result;
		}
	};

	template<typename TObjectType>
	struct TScopedAsRelease
	{
		TObjectType* Object = nullptr;

		explicit TScopedAsRelease(TObjectType* InObject)
			: Object(InObject)
		{
		}

		~TScopedAsRelease()
		{
			if (Object != nullptr)
			{
				Object->Release();
			}
		}
	};

	struct FScopedAsEngineRelease
	{
		asIScriptEngine* Engine = nullptr;

		explicit FScopedAsEngineRelease(asIScriptEngine* InEngine)
			: Engine(InEngine)
		{
		}

		~FScopedAsEngineRelease()
		{
			if (Engine != nullptr)
			{
				Engine->ShutDownAndRelease();
			}
		}
	};
}


TEST_CLASS_WITH_FLAGS(FAngelscriptHeaderShimTests,
	"Angelscript.TestModule.Engine.HeaderShim",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(RawAngelscriptApiRoundTrip)
	{
		using namespace AngelscriptTest_Core_AngelscriptHeaderShimTests_Private;
		const ANSICHAR* RawLibraryVersion = asGetLibraryVersion();
		ASSERT_THAT(IsNotNull(RawLibraryVersion, TEXT("HeaderShim native API test should expose a library version string")));

		const FString LibraryVersion = ANSI_TO_TCHAR(RawLibraryVersion);
		ASSERT_THAT(IsFalse(LibraryVersion.IsEmpty(), TEXT("HeaderShim native API test should expose a non-empty library version string")));

		FNativeMessageCollector MessageCollector;
		asIScriptEngine* NativeEngine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
		ASSERT_THAT(IsNotNull(NativeEngine, TEXT("HeaderShim native API test should create a raw AngelScript engine")));

		FScopedAsEngineRelease EngineScope(NativeEngine);

		const int32 CallbackResult = NativeEngine->SetMessageCallback(
			asFUNCTION(FNativeMessageCollector::Callback),
			&MessageCollector,
			asCALL_CDECL);
		ASSERT_THAT(AreEqual(static_cast<int32>(asSUCCESS), CallbackResult, TEXT("HeaderShim native API test should install the message callback")));

		asIScriptModule* Module = NativeEngine->GetModule("ASHeaderShimRoundTrip", asGM_ALWAYS_CREATE);
		ASSERT_THAT(IsNotNull(Module, TEXT("HeaderShim native API test should create a native script module")));

		const char* Source = "int Entry() { return 5; }";
		asIScriptFunction* Function = nullptr;
		const int32 CompileResult = Module->CompileFunction("ASHeaderShimRoundTrip", Source, 0, 0, &Function);
		ASSERT_THAT(AreEqual(
			static_cast<int32>(asSUCCESS),
			CompileResult,
			FString::Printf(TEXT("HeaderShim native API test should compile the raw function successfully. Diagnostics: %s"), *MessageCollector.Format())));

		ASSERT_THAT(IsNotNull(Function, TEXT("HeaderShim native API test should receive a compiled function")));

		TScopedAsRelease<asIScriptFunction> FunctionScope(Function);

		asIScriptContext* Context = NativeEngine->CreateContext();
		ASSERT_THAT(IsNotNull(Context, TEXT("HeaderShim native API test should create a native script context")));

		TScopedAsRelease<asIScriptContext> ContextScope(Context);

		const int32 PrepareResult = Context->Prepare(Function);
		ASSERT_THAT(AreEqual(static_cast<int32>(asSUCCESS), PrepareResult, TEXT("HeaderShim native API test should prepare the raw function successfully")));

		const int32 ExecuteResult = Context->Execute();
		ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_FINISHED), ExecuteResult, TEXT("HeaderShim native API test should finish execution successfully")));

		ASSERT_THAT(AreEqual(5, static_cast<int32>(Context->GetReturnDWord()), TEXT("HeaderShim native API test should return the compiled Entry() result")));
	}
};

#endif
