#include "../Support/AngelscriptNativeCoreTestSupport.h"
#include "../Support/AngelscriptNativeExecutionTestSupport.h"
#include "../Support/AngelscriptNativeLanguageCaseTestSupport.h"

#include "AngelscriptTestMacros.h"
#include "CQTest.h"
#include "Misc/ScopeExit.h"

#include <string>

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FEngineRegistrationServiceTests,
	"Angelscript.TestModule.AngelScriptSDK.Engine.RegistrationServices",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	struct FNativeReviewStringValue
	{
		asDWORD Words[2];
	};

	class FNativeReviewStringFactory final : public asIStringFactory
	{
	private:
		struct FStringEntry
		{
			explicit FStringEntry(std::string InBytes)
				: Bytes(MoveTemp(InBytes))
			{
			}

			std::string Bytes;
			int32 ReferenceCount = 1;
		};

	public:
		const void* GetStringConstant(const char* Data, asUINT Length) override
		{
			const std::string Bytes =
				Data != nullptr
					? std::string(Data, static_cast<size_t>(Length))
					: std::string();

			for (const TUniquePtr<FStringEntry>& Entry : Entries)
			{
				if (Entry->Bytes == Bytes)
				{
					++Entry->ReferenceCount;
					++AcquireCallCount;
					return Entry.Get();
				}
			}

			TUniquePtr<FStringEntry> Entry = MakeUnique<FStringEntry>(Bytes);
			const FStringEntry* const Result = Entry.Get();
			Entries.Add(MoveTemp(Entry));
			++AcquireCallCount;
			return Result;
		}

		int ReleaseStringConstant(const void* String) override
		{
			for (int32 EntryIndex = 0; EntryIndex < Entries.Num(); ++EntryIndex)
			{
				FStringEntry* const Entry = Entries[EntryIndex].Get();
				if (Entry != String)
				{
					continue;
				}

				++ReleaseCallCount;
				--Entry->ReferenceCount;
				if (Entry->ReferenceCount == 0)
				{
					Entries.RemoveAt(EntryIndex);
				}
				return asSUCCESS;
			}

			return asINVALID_ARG;
		}

		int GetRawStringData(
			const void* String,
			char* Data,
			asUINT* Length) const override
		{
			if (Length == nullptr)
			{
				return asINVALID_ARG;
			}

			const FStringEntry* const Entry = FindEntry(String);
			if (Entry == nullptr)
			{
				return asINVALID_ARG;
			}

			*Length = static_cast<asUINT>(Entry->Bytes.size());
			if (Data == nullptr)
			{
				++RawLengthQueryCount;
				return asSUCCESS;
			}

			if (!Entry->Bytes.empty())
			{
				FMemory::Memcpy(
					Data,
					Entry->Bytes.data(),
					Entry->Bytes.size());
			}
			++RawCopyCallCount;
			return asSUCCESS;
		}

		int32 GetAcquireCallCount() const
		{
			return AcquireCallCount;
		}

		int32 GetReleaseCallCount() const
		{
			return ReleaseCallCount;
		}

		int32 GetRawLengthQueryCount() const
		{
			return RawLengthQueryCount;
		}

		int32 GetRawCopyCallCount() const
		{
			return RawCopyCallCount;
		}

		int32 GetEntryCount() const
		{
			return Entries.Num();
		}

		int32 GetTotalReferenceCount() const
		{
			int32 TotalReferenceCount = 0;
			for (const TUniquePtr<FStringEntry>& Entry : Entries)
			{
				TotalReferenceCount += Entry->ReferenceCount;
			}
			return TotalReferenceCount;
		}

	private:
		const FStringEntry* FindEntry(const void* String) const
		{
			for (const TUniquePtr<FStringEntry>& Entry : Entries)
			{
				if (Entry.Get() == String)
				{
					return Entry.Get();
				}
			}
			return nullptr;
		}

		TArray<TUniquePtr<FStringEntry>> Entries;
		int32 AcquireCallCount = 0;
		int32 ReleaseCallCount = 0;
		mutable int32 RawLengthQueryCount = 0;
		mutable int32 RawCopyCallCount = 0;
	};

	static void ObserveNativeReviewString(asIScriptGeneric*)
	{
	}

	static FString BuildReviewSource(
		const TCHAR* Operation,
		const TCHAR* Input,
		const TCHAR* FunctionName)
	{
		using namespace AngelscriptNativeTestSupport;

		FString Source;
		AppendGeneratedAsLine(
			Source,
			FString::Printf(
				TEXT("// native_operation=%s input=%s"),
				Operation,
				Input));
		AppendGeneratedAsLine(
			Source,
			FString::Printf(TEXT("void %s()"), FunctionName));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\t// The host invokes the public raw SDK API described above."));
		AppendGeneratedAsLine(Source, TEXT("}"));
		return Source;
	}

	static void PrintReviewSource(
		FAutomationTestBase& Test,
		const TCHAR* SourceId,
		const TCHAR* Operation,
		const TCHAR* Input,
		const TCHAR* FunctionName)
	{
		AngelscriptNativeTestSupport::PrintGeneratedAsSource(
			Test,
			SourceId,
			TEXT("EngineRegistrationServiceReview"),
			BuildReviewSource(Operation, Input, FunctionName));
	}

public:
	TEST_METHOD(StringFactoryReferenceAndRawDataLifecycle)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT("ENG-REGISTRATION-STRING-FACTORY-INTERFACE",
			ENativeEvidence::Runtime
				| ENativeEvidence::Metadata
				| ENativeEvidence::Lifecycle
				| ENativeEvidence::Cleanup
				| ENativeEvidence::Isolation);

		PrintReviewSource(
			*TestRunner,
			TEXT("ENG-REGISTRATION-STRING-FACTORY-INTERFACE-ALPHA-REUSE"),
			TEXT("asIStringFactory.GetStringConstant/GetRawStringData/ReleaseStringConstant"),
			TEXT("alpha acquire-twice query-copy release-twice"),
			TEXT("ReviewStringFactoryAlphaReuse"));

		FNativeReviewStringFactory Factory;
		const void* const FirstConstant =
			Factory.GetStringConstant("alpha", 5);
		const void* const ReusedConstant =
			Factory.GetStringConstant("alpha", 5);

		ASSERT_THAT(IsNotNull(
			FirstConstant,
			TEXT("String factory should create a stable constant for alpha")));
		ASSERT_THAT(AreEqual(
			FirstConstant,
			ReusedConstant,
			TEXT("Equal byte sequences should reuse the exact string-constant identity")));
		ASSERT_THAT(AreEqual(
			1,
			Factory.GetEntryCount(),
			TEXT("Repeated acquisition should retain one interned entry")));
		ASSERT_THAT(AreEqual(
			2,
			Factory.GetTotalReferenceCount(),
			TEXT("Repeated acquisition should retain two live references")));
		ASSERT_THAT(AreEqual(
			2,
			Factory.GetAcquireCallCount(),
			TEXT("Factory should observe both public acquisition calls")));

		asUINT RawLength = MAX_uint32;
		ASSERT_THAT(AreEqual(
			static_cast<int32>(asSUCCESS),
			Factory.GetRawStringData(FirstConstant, nullptr, &RawLength),
			TEXT("Raw string length query should succeed without a destination buffer")));
		ASSERT_THAT(AreEqual(
			5u,
			RawLength,
			TEXT("Raw string length query should preserve the exact byte length")));

		TArray<ANSICHAR> RawBytes;
		RawBytes.SetNumZeroed(static_cast<int32>(RawLength) + 1);
		ASSERT_THAT(AreEqual(
			static_cast<int32>(asSUCCESS),
			Factory.GetRawStringData(
				FirstConstant,
				RawBytes.GetData(),
				&RawLength),
			TEXT("Raw string copy should succeed for an owned constant")));
		ASSERT_THAT(AreEqual(
			FString(TEXT("alpha")),
			FString(ANSI_TO_TCHAR(RawBytes.GetData())),
			TEXT("Raw string copy should preserve every source byte")));
		ASSERT_THAT(AreEqual(
			1,
			Factory.GetRawLengthQueryCount(),
			TEXT("Factory should distinguish the length-only raw-data query")));
		ASSERT_THAT(AreEqual(
			1,
			Factory.GetRawCopyCallCount(),
			TEXT("Factory should distinguish the raw-data copy")));

		ASSERT_THAT(AreEqual(
			static_cast<int32>(asSUCCESS),
			Factory.ReleaseStringConstant(FirstConstant),
			TEXT("First release should preserve the reused entry")));
		ASSERT_THAT(AreEqual(
			1,
			Factory.GetTotalReferenceCount(),
			TEXT("First release should leave exactly one live reference")));
		ASSERT_THAT(AreEqual(
			static_cast<int32>(asSUCCESS),
			Factory.ReleaseStringConstant(ReusedConstant),
			TEXT("Final release should destroy the interned entry")));
		ASSERT_THAT(AreEqual(
			0,
			Factory.GetEntryCount(),
			TEXT("Final release should leave no interned entries")));
		ASSERT_THAT(AreEqual(
			2,
			Factory.GetReleaseCallCount(),
			TEXT("Factory should observe both balanced public release calls")));

		PrintReviewSource(
			*TestRunner,
			TEXT("ENG-REGISTRATION-STRING-FACTORY-INTERFACE-NULL-RAW-DATA-REJECT"),
			TEXT("asIStringFactory.GetRawStringData"),
			TEXT("string=null length=out"),
			TEXT("ReviewNullRawStringData"));
		ASSERT_THAT(AreEqual(
			static_cast<int32>(asINVALID_ARG),
			Factory.GetRawStringData(nullptr, nullptr, &RawLength),
			TEXT("Raw data lookup should reject a null string identity")));

		PrintReviewSource(
			*TestRunner,
			TEXT("ENG-REGISTRATION-STRING-FACTORY-INTERFACE-STALE-RELEASE-REJECT"),
			TEXT("asIStringFactory.ReleaseStringConstant"),
			TEXT("identity=fully-released-alpha"),
			TEXT("ReviewStaleStringConstantRelease"));
		ASSERT_THAT(AreEqual(
			static_cast<int32>(asINVALID_ARG),
			Factory.ReleaseStringConstant(FirstConstant),
			TEXT("Release should reject an identity after its final reference is gone")));
	}

	TEST_METHOD(StringFactoryRegistrationLiteralAndBytecodeLifecycle)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT("ENG-REGISTRATION-STRING-FACTORY-SERVICE",
			ENativeEvidence::Compile
				| ENativeEvidence::Bytecode
				| ENativeEvidence::Metadata
				| ENativeEvidence::Lifecycle
				| ENativeEvidence::Cleanup
				| ENativeEvidence::Isolation);

		FNativeReviewStringFactory Factory;
		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};

		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(
			ScriptEngine,
			TEXT("String factory service product should create a raw SDK engine")));
		if (ScriptEngine == nullptr)
		{
			return;
		}

		PrintReviewSource(
			*TestRunner,
			TEXT("ENG-REGISTRATION-STRING-FACTORY-SERVICE-FRESH-QUERY"),
			TEXT("asIScriptEngine.GetStringFactoryReturnTypeId"),
			TEXT("fresh-engine flags=0xA5A5A5A5"),
			TEXT("ReviewFreshStringFactoryQuery"));
		asDWORD UnregisteredFlags = 0xA5A5A5A5u;
		ASSERT_THAT(AreEqual(
			static_cast<int32>(asNO_FUNCTION),
			ScriptEngine->GetStringFactoryReturnTypeId(&UnregisteredFlags),
			TEXT("Fresh engine should report that no string factory is registered")));
		ASSERT_THAT(AreEqual(
			0xA5A5A5A5u,
			UnregisteredFlags,
			TEXT("Missing string factory query should not overwrite the caller's flags")));

		const int StringTypeId = ScriptEngine->RegisterObjectType(
			"NativeReviewString",
			sizeof(FNativeReviewStringValue),
			asOBJ_VALUE
				| asOBJ_POD
				| asGetTypeTraits<FNativeReviewStringValue>());
		ASSERT_THAT(IsTrue(
			StringTypeId >= 0,
			TEXT("String factory service should register its local raw value type")));
		if (StringTypeId < 0)
		{
			return;
		}

		PrintReviewSource(
			*TestRunner,
			TEXT("ENG-REGISTRATION-STRING-FACTORY-SERVICE-REGISTER-VALUE-TYPE"),
			TEXT("asIScriptEngine.RegisterStringFactory/GetStringFactoryReturnTypeId"),
			TEXT("NativeReviewString factory=local"),
			TEXT("ReviewValueStringFactoryRegistration"));
		ASSERT_THAT(AreEqual(
			static_cast<int32>(asSUCCESS),
			ScriptEngine->RegisterStringFactory(
				"NativeReviewString",
				&Factory),
			TEXT("Engine should register the local raw string factory")));

		asDWORD RegisteredFlags = MAX_uint32;
		ASSERT_THAT(AreEqual(
			StringTypeId,
			ScriptEngine->GetStringFactoryReturnTypeId(&RegisteredFlags),
			TEXT("String factory query should return the exact registered type identity")));
		ASSERT_THAT(AreEqual(
			0u,
			RegisteredFlags,
			TEXT("String factory query should publish the fork's exact zero flags")));
		ASSERT_THAT(AreEqual(
			FString(TEXT("NativeReviewString")),
			FString(UTF8_TO_TCHAR(
				ScriptEngine->GetTypeDeclaration(StringTypeId))),
			TEXT("Registered string factory type ID should round-trip through its declaration")));

		PrintReviewSource(
			*TestRunner,
			TEXT("ENG-REGISTRATION-STRING-FACTORY-SERVICE-REPEAT-SAME"),
			TEXT("asIScriptEngine.RegisterStringFactory"),
			TEXT("NativeReviewString same-factory repeat"),
			TEXT("ReviewRepeatedStringFactoryRegistration"));
		ASSERT_THAT(AreEqual(
			static_cast<int32>(asSUCCESS),
			ScriptEngine->RegisterStringFactory(
				"NativeReviewString",
				&Factory),
			TEXT("Repeating the identical factory registration should preserve the service")));
		ASSERT_THAT(AreEqual(
			StringTypeId,
			ScriptEngine->GetStringFactoryReturnTypeId(),
			TEXT("Repeated identical registration should preserve the exact string type ID")));

		const int ObserverRegistrationResult =
			ScriptEngine->RegisterGlobalFunction(
				"void ObserveNativeReviewString(const NativeReviewString&in Value)",
				asFUNCTION(ObserveNativeReviewString),
				asCALL_GENERIC);
		ASSERT_THAT(IsTrue(
			ObserverRegistrationResult >= 0,
			TEXT("String factory service should register its local literal observer")));
		if (ObserverRegistrationResult < 0)
		{
			return;
		}

		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			void UseNativeReviewStringLiteral()
			{
				ObserveNativeReviewString("alpha");
			}
			)AS");
		PrintGeneratedAsSource(
			*TestRunner,
			TEXT("ENG-REGISTRATION-STRING-FACTORY-SERVICE-LITERAL-ALPHA"),
			TEXT("EngineRegistrationStringFactoryLiteral"),
			UTF8_TO_TCHAR(ScriptSource.c_str()));

		asIScriptModule* const Module =
			ScriptEngine->GetModule(
				"EngineRegistrationStringFactoryLiteral",
				asGM_ALWAYS_CREATE);
		ASSERT_THAT(IsNotNull(
			Module,
			TEXT("String factory service should create its literal module")));
		if (Module == nullptr)
		{
			return;
		}

		const int AddSectionResult = Module->AddScriptSection(
			"EngineRegistrationStringFactoryLiteral.as",
			ScriptSource.c_str());
		ASSERT_THAT(AreEqual(
			static_cast<int32>(asSUCCESS),
			AddSectionResult,
			TEXT("String factory service should add the printed literal section")));
		if (AddSectionResult < 0)
		{
			return;
		}

		const int BuildResult = Module->Build();
		ASSERT_THAT(AreEqual(
			static_cast<int32>(asSUCCESS),
			BuildResult,
			TEXT("Registered string factory should compile a core string literal without an addon")));
		if (BuildResult < 0)
		{
			return;
		}

		ASSERT_THAT(IsTrue(
			Factory.GetAcquireCallCount() > 0,
			TEXT("Literal compilation should acquire at least one factory constant")));
		ASSERT_THAT(IsTrue(
			Factory.GetTotalReferenceCount() > 0,
			TEXT("Compiled literal bytecode should retain an owned factory reference")));

		FMemoryBinaryStream Bytecode;
		ASSERT_THAT(AreEqual(
			static_cast<int32>(asSUCCESS),
			Module->SaveByteCode(&Bytecode),
			TEXT("String literal module should serialize through the local factory")));
		ASSERT_THAT(IsTrue(
			Bytecode.GetBytes().Num() > 0,
			TEXT("String literal bytecode serialization should emit bytes")));
		ASSERT_THAT(IsTrue(
			Factory.GetRawLengthQueryCount() > 0,
			TEXT("Bytecode serialization should query the raw string length")));
		ASSERT_THAT(IsTrue(
			Factory.GetRawCopyCallCount() > 0,
			TEXT("Bytecode serialization should copy the raw string bytes")));

		Engine.Destroy();
		ASSERT_THAT(AreEqual(
			0,
			Factory.GetTotalReferenceCount(),
			TEXT("Engine teardown should release every compiled string constant")));
		ASSERT_THAT(AreEqual(
			Factory.GetAcquireCallCount(),
			Factory.GetReleaseCallCount(),
			TEXT("String constant acquisition and release counts should balance at teardown")));
	}

	TEST_METHOD(StringFactoryRegistrationRejectsInvalidInputsInIsolation)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT("ENG-REGISTRATION-STRING-FACTORY-BOUNDARIES",
			ENativeEvidence::Diagnostic
				| ENativeEvidence::Metadata
				| ENativeEvidence::Cleanup
				| ENativeEvidence::Isolation);

		FNativeReviewStringFactory Factory;

		{
			FNativeTestEngine NullFactoryEngine;
			NullFactoryEngine.Create(*TestRunner);
			ON_SCOPE_EXIT
			{
				NullFactoryEngine.Destroy();
			};

			asIScriptEngine* const ScriptEngine =
				NullFactoryEngine.Get();
			ASSERT_THAT(IsNotNull(
				ScriptEngine,
				TEXT("Null-factory boundary should create an isolated raw SDK engine")));
			if (ScriptEngine != nullptr)
			{
				PrintReviewSource(
					*TestRunner,
					TEXT("ENG-REGISTRATION-STRING-FACTORY-BOUNDARIES-NULL-FACTORY"),
					TEXT("asIScriptEngine.RegisterStringFactory"),
					TEXT("datatype=int factory=null"),
					TEXT("ReviewNullStringFactory"));
				ASSERT_THAT(AreEqual(
					static_cast<int32>(asINVALID_ARG),
					ScriptEngine->RegisterStringFactory("int", nullptr),
					TEXT("String factory registration should reject a null factory")));
				ASSERT_THAT(AreEqual(
					static_cast<int32>(asNO_FUNCTION),
					ScriptEngine->GetStringFactoryReturnTypeId(),
					TEXT("Rejected null factory should not publish a string type")));
				ASSERT_THAT(IsTrue(
					NullFactoryEngine.GetMessagesText().Contains(
						TEXT("RegisterStringFactory"),
						ESearchCase::CaseSensitive),
					TEXT("Null factory rejection should retain its registration diagnostic")));
			}
		}

		{
			FNativeTestEngine UnknownTypeEngine;
			UnknownTypeEngine.Create(*TestRunner);
			ON_SCOPE_EXIT
			{
				UnknownTypeEngine.Destroy();
			};

			asIScriptEngine* const ScriptEngine =
				UnknownTypeEngine.Get();
			ASSERT_THAT(IsNotNull(
				ScriptEngine,
				TEXT("Unknown-type boundary should create an isolated raw SDK engine")));
			if (ScriptEngine != nullptr)
			{
				PrintReviewSource(
					*TestRunner,
					TEXT("ENG-REGISTRATION-STRING-FACTORY-BOUNDARIES-UNKNOWN-TYPE"),
					TEXT("asIScriptEngine.RegisterStringFactory"),
					TEXT("datatype=MissingNativeReviewString"),
					TEXT("ReviewUnknownStringFactoryType"));
				ASSERT_THAT(AreEqual(
					static_cast<int32>(asINVALID_TYPE),
					ScriptEngine->RegisterStringFactory(
						"MissingNativeReviewString",
						&Factory),
					TEXT("String factory registration should reject an unknown type")));
				ASSERT_THAT(AreEqual(
					static_cast<int32>(asNO_FUNCTION),
					ScriptEngine->GetStringFactoryReturnTypeId(),
					TEXT("Rejected unknown type should not publish a string factory")));
				ASSERT_THAT(IsTrue(
					UnknownTypeEngine.GetMessagesText().Contains(
						TEXT("RegisterStringFactory"),
						ESearchCase::CaseSensitive),
					TEXT("Unknown string type rejection should retain its registration diagnostic")));
			}
		}

		{
			FNativeTestEngine HandleTypeEngine;
			HandleTypeEngine.Create(*TestRunner);
			ON_SCOPE_EXIT
			{
				HandleTypeEngine.Destroy();
			};

			asIScriptEngine* const ScriptEngine =
				HandleTypeEngine.Get();
			ASSERT_THAT(IsNotNull(
				ScriptEngine,
				TEXT("Handle-type boundary should create an isolated raw SDK engine")));
			if (ScriptEngine != nullptr)
			{
				const int ReferenceTypeId =
					ScriptEngine->RegisterObjectType(
						"NativeReviewStringReference",
						0,
						asOBJ_REF | asOBJ_NOCOUNT);
				ASSERT_THAT(IsTrue(
					ReferenceTypeId >= 0,
					TEXT("Handle-type boundary should register its local reference type")));
				if (ReferenceTypeId >= 0)
				{
					PrintReviewSource(
						*TestRunner,
						TEXT("ENG-REGISTRATION-STRING-FACTORY-BOUNDARIES-HANDLE-TYPE"),
						TEXT("asIScriptEngine.RegisterStringFactory"),
						TEXT("datatype=NativeReviewStringReference@"),
						TEXT("ReviewHandleStringFactoryType"));
					ASSERT_THAT(AreEqual(
						static_cast<int32>(asINVALID_TYPE),
						ScriptEngine->RegisterStringFactory(
							"NativeReviewStringReference@",
							&Factory),
						TEXT("String factory registration should reject an object handle type")));
					ASSERT_THAT(AreEqual(
						static_cast<int32>(asNO_FUNCTION),
						ScriptEngine->GetStringFactoryReturnTypeId(),
						TEXT("Rejected handle type should not publish a string factory")));
					ASSERT_THAT(IsTrue(
						HandleTypeEngine.GetMessagesText().Contains(
							TEXT("RegisterStringFactory"),
							ESearchCase::CaseSensitive),
						TEXT("Handle string type rejection should retain its registration diagnostic")));
				}
			}
		}
	}

	TEST_METHOD(DefaultArrayTypeRegistrationAndFailureIsolation)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT("ENG-REGISTRATION-DEFAULT-ARRAY-TYPE",
			ENativeEvidence::Metadata
				| ENativeEvidence::Diagnostic
				| ENativeEvidence::Lifecycle
				| ENativeEvidence::Isolation);

		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};

		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(
			ScriptEngine,
			TEXT("Default-array product should create a raw SDK engine")));
		if (ScriptEngine == nullptr)
		{
			return;
		}

		PrintReviewSource(
			*TestRunner,
			TEXT("ENG-REGISTRATION-DEFAULT-ARRAY-TYPE-FRESH"),
			TEXT("asIScriptEngine.GetDefaultArrayTypeId"),
			TEXT("fresh-engine"),
			TEXT("ReviewFreshDefaultArrayType"));
		ASSERT_THAT(AreEqual(
			static_cast<int32>(asINVALID_TYPE),
			ScriptEngine->GetDefaultArrayTypeId(),
			TEXT("Fresh engine should expose no default array type")));

		PrintReviewSource(
			*TestRunner,
			TEXT("ENG-REGISTRATION-DEFAULT-ARRAY-TYPE-PRIMITIVE-REJECT"),
			TEXT("asIScriptEngine.RegisterDefaultArrayType"),
			TEXT("int"),
			TEXT("ReviewPrimitiveDefaultArrayType"));
		ASSERT_THAT(AreEqual(
			static_cast<int32>(asINVALID_TYPE),
			ScriptEngine->RegisterDefaultArrayType("int"),
			TEXT("Default array registration should reject a primitive type")));
		ASSERT_THAT(AreEqual(
			static_cast<int32>(asINVALID_TYPE),
			ScriptEngine->GetDefaultArrayTypeId(),
			TEXT("Rejected primitive should leave the default array unset")));

		const int NonTemplateTypeId =
			ScriptEngine->RegisterObjectType(
				"NativeReviewNonTemplate",
				0,
				asOBJ_REF | asOBJ_NOCOUNT);
		ASSERT_THAT(IsTrue(
			NonTemplateTypeId >= 0,
			TEXT("Default-array product should register its non-template control type")));
		if (NonTemplateTypeId >= 0)
		{
			PrintReviewSource(
				*TestRunner,
				TEXT("ENG-REGISTRATION-DEFAULT-ARRAY-TYPE-NON-TEMPLATE-REJECT"),
				TEXT("asIScriptEngine.RegisterDefaultArrayType"),
				TEXT("NativeReviewNonTemplate"),
				TEXT("ReviewNonTemplateDefaultArrayType"));
			ASSERT_THAT(AreEqual(
				static_cast<int32>(asINVALID_TYPE),
				ScriptEngine->RegisterDefaultArrayType(
					"NativeReviewNonTemplate"),
				TEXT("Default array registration should reject a non-template object type")));
			ASSERT_THAT(AreEqual(
				static_cast<int32>(asINVALID_TYPE),
				ScriptEngine->GetDefaultArrayTypeId(),
				TEXT("Rejected non-template object should leave the default array unset")));
		}

		const int ArrayTemplateRegistrationResult =
			ScriptEngine->RegisterObjectType(
				"NativeReviewArray<class T>",
				0,
				asOBJ_REF | asOBJ_TEMPLATE | asOBJ_NOCOUNT);
		ASSERT_THAT(IsTrue(
			ArrayTemplateRegistrationResult >= 0,
			TEXT("Default-array product should register its local template type")));
		if (ArrayTemplateRegistrationResult < 0)
		{
			return;
		}

		asITypeInfo* const RegisteredArrayType =
			ScriptEngine->GetTypeInfoByName("NativeReviewArray");
		ASSERT_THAT(IsNotNull(
			RegisteredArrayType,
			TEXT("Registered array template should publish its exact TypeInfo")));
		if (RegisteredArrayType == nullptr)
		{
			return;
		}
		const int ArrayTemplateTypeId = RegisteredArrayType->GetTypeId();

		PrintReviewSource(
			*TestRunner,
			TEXT("ENG-REGISTRATION-DEFAULT-ARRAY-TYPE-TEMPLATE-ACCEPT"),
			TEXT("asIScriptEngine.RegisterDefaultArrayType"),
			TEXT("NativeReviewArray<T>"),
			TEXT("ReviewTemplateDefaultArrayType"));
		ASSERT_THAT(AreEqual(
			static_cast<int32>(asSUCCESS),
			ScriptEngine->RegisterDefaultArrayType(
				"NativeReviewArray<T>"),
			TEXT("Default array registration should accept the local template type")));
		ASSERT_THAT(AreEqual(
			ArrayTemplateTypeId,
			ScriptEngine->GetDefaultArrayTypeId(),
			TEXT("Default array query should return the exact registered template type ID")));

		asITypeInfo* const DefaultArrayType =
			ScriptEngine->GetTypeInfoById(
				ScriptEngine->GetDefaultArrayTypeId());
		ASSERT_THAT(IsNotNull(
			DefaultArrayType,
			TEXT("Default array type ID should resolve through engine type metadata")));
		if (DefaultArrayType != nullptr)
		{
			ASSERT_THAT(AreEqual(
				FString(TEXT("NativeReviewArray")),
				FString(UTF8_TO_TCHAR(DefaultArrayType->GetName())),
				TEXT("Default array metadata should preserve the template base name")));
			ASSERT_THAT(IsTrue(
				(DefaultArrayType->GetFlags() & asOBJ_TEMPLATE) != 0,
				TEXT("Default array metadata should preserve the template flag")));
		}

		PrintReviewSource(
			*TestRunner,
			TEXT("ENG-REGISTRATION-DEFAULT-ARRAY-TYPE-POST-INSTALL-INVALID"),
			TEXT("asIScriptEngine.RegisterDefaultArrayType"),
			TEXT("int after NativeReviewArray<T>"),
			TEXT("ReviewPostInstallInvalidDefaultArrayType"));
		ASSERT_THAT(AreEqual(
			static_cast<int32>(asINVALID_TYPE),
			ScriptEngine->RegisterDefaultArrayType("int"),
			TEXT("Invalid registration after success should still be rejected")));
		ASSERT_THAT(AreEqual(
			ArrayTemplateTypeId,
			ScriptEngine->GetDefaultArrayTypeId(),
			TEXT("Rejected post-install input should preserve the existing default array identity")));

		TestRunner->AddInfo(
			TEXT("[AS-FORK-LIMITATION] Repeating a successful default-array registration is excluded because the current setter overwrites the pointer after AddRefInternal without releasing the previous internal reference"));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
