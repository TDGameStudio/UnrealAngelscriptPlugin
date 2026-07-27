#include "../../Support/AngelscriptNativeCaseTestSupport.h"
#include "../../Support/AngelscriptNativeCoreTestSupport.h"
#include "../../Support/AngelscriptNativeLanguageCaseTestSupport.h"

#include "CQTest.h"
#include "Misc/ScopeExit.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FPropertyForkSemanticsTests,
	"Angelscript.TestModule.AngelScriptSDK.Language.Properties.ForkSemantics",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	using FNativeCaseContext = AngelscriptNativeTestSupport::FNativeCaseContext;
	using FNativeMessageEntry = AngelscriptNativeTestSupport::FNativeMessageEntry;
	using FNativeTestEngine = AngelscriptNativeTestSupport::FNativeTestEngine;

	enum class EDecoratorCase : uint8
	{
		Getter,
		Setter,
		IndexedGetter,
		IndexedSetter,
	};

	enum class EAutomaticAccessCase : uint8
	{
		Read,
		Write,
		IndexedRead,
		IndexedWrite,
	};

	struct FCarrierState
	{
		int32 LiveObjects = 0;
		int32 CreatedObjects = 0;
		int32 DestroyedObjects = 0;
		int32 GetterCalls = 0;
		int32 SetterCalls = 0;

		void ResetCalls()
		{
			GetterCalls = 0;
			SetterCalls = 0;
		}
	};

	class FPropertyCarrier
	{
	public:
		explicit FPropertyCarrier(FCarrierState& InState, const int32 InValue)
			: State(InState)
			, Value(InValue)
		{
			++State.LiveObjects;
			++State.CreatedObjects;
		}

		void AddRef()
		{
			++ReferenceCount;
		}

		void Release()
		{
			--ReferenceCount;
			if (ReferenceCount == 0)
			{
				delete this;
			}
		}

		FCarrierState& State;
		int32 Value = 0;

	private:
		~FPropertyCarrier()
		{
			--State.LiveObjects;
			++State.DestroyedObjects;
		}

		int32 ReferenceCount = 1;
	};

	inline static FCarrierState* ActiveState = nullptr;

	inline static constexpr EDecoratorCase DecoratorCases[] =
	{
		EDecoratorCase::Getter,
		EDecoratorCase::Setter,
		EDecoratorCase::IndexedGetter,
		EDecoratorCase::IndexedSetter,
	};

	inline static constexpr EAutomaticAccessCase AutomaticAccessCases[] =
	{
		EAutomaticAccessCase::Read,
		EAutomaticAccessCase::Write,
		EAutomaticAccessCase::IndexedRead,
		EAutomaticAccessCase::IndexedWrite,
	};

	static const TCHAR* GetDecoratorCaseName(const EDecoratorCase Case)
	{
		switch (Case)
		{
		case EDecoratorCase::Getter:
			return TEXT("getter");
		case EDecoratorCase::Setter:
			return TEXT("setter");
		case EDecoratorCase::IndexedGetter:
			return TEXT("indexed_getter");
		case EDecoratorCase::IndexedSetter:
			return TEXT("indexed_setter");
		default:
			return TEXT("unknown");
		}
	}

	static const TCHAR* GetAutomaticAccessCaseName(const EAutomaticAccessCase Case)
	{
		switch (Case)
		{
		case EAutomaticAccessCase::Read:
			return TEXT("read");
		case EAutomaticAccessCase::Write:
			return TEXT("write");
		case EAutomaticAccessCase::IndexedRead:
			return TEXT("indexed_read");
		case EAutomaticAccessCase::IndexedWrite:
			return TEXT("indexed_write");
		default:
			return TEXT("unknown");
		}
	}

	static void AddRefCarrier(FPropertyCarrier* Object)
	{
		if (Object != nullptr)
		{
			Object->AddRef();
		}
	}

	static void ReleaseCarrier(FPropertyCarrier* Object)
	{
		if (Object != nullptr)
		{
			Object->Release();
		}
	}

	static void CreateCarrier(asIScriptGeneric* Generic)
	{
		if (Generic == nullptr || ActiveState == nullptr)
		{
			return;
		}

		Generic->SetReturnAddress(new FPropertyCarrier(
			*ActiveState,
			static_cast<int32>(Generic->GetArgDWord(0))));
	}

	static void GetValue(asIScriptGeneric* Generic)
	{
		FPropertyCarrier* const Object = Generic != nullptr
			? static_cast<FPropertyCarrier*>(Generic->GetObject())
			: nullptr;
		if (Generic == nullptr || Object == nullptr)
		{
			return;
		}

		++Object->State.GetterCalls;
		Generic->SetReturnDWord(static_cast<asDWORD>(Object->Value));
	}

	static void SetValue(asIScriptGeneric* Generic)
	{
		FPropertyCarrier* const Object = Generic != nullptr
			? static_cast<FPropertyCarrier*>(Generic->GetObject())
			: nullptr;
		if (Generic == nullptr || Object == nullptr)
		{
			return;
		}

		++Object->State.SetterCalls;
		Object->Value = static_cast<int32>(Generic->GetArgDWord(0));
	}

	static void GetIndexedValue(asIScriptGeneric* Generic)
	{
		FPropertyCarrier* const Object = Generic != nullptr
			? static_cast<FPropertyCarrier*>(Generic->GetObject())
			: nullptr;
		if (Generic == nullptr || Object == nullptr)
		{
			return;
		}

		++Object->State.GetterCalls;
		Generic->SetReturnDWord(static_cast<asDWORD>(
			Object->Value + static_cast<int32>(Generic->GetArgDWord(0))));
	}

	static void SetIndexedValue(asIScriptGeneric* Generic)
	{
		FPropertyCarrier* const Object = Generic != nullptr
			? static_cast<FPropertyCarrier*>(Generic->GetObject())
			: nullptr;
		if (Generic == nullptr || Object == nullptr)
		{
			return;
		}

		++Object->State.SetterCalls;
		Object->Value = static_cast<int32>(Generic->GetArgDWord(1));
	}

	static bool HasDiagnosticContaining(const FNativeTestEngine& Engine, const TCHAR* const ExpectedText)
	{
		return Engine.GetMessages().Entries.ContainsByPredicate([ExpectedText](const FNativeMessageEntry& Entry)
		{
			return Entry.Message.Contains(ExpectedText);
		});
	}

	static bool RegisterCarrierBase(asIScriptEngine& ScriptEngine)
	{
		return ScriptEngine.RegisterObjectType(
			"FPropertyForkCarrier",
			0,
			asOBJ_REF | asOBJ_IMPLICIT_HANDLE) >= 0
			&& ScriptEngine.RegisterObjectBehaviour(
				"FPropertyForkCarrier",
				asBEHAVE_ADDREF,
				"void f()",
				asFUNCTION(AddRefCarrier),
				asCALL_CDECL_OBJFIRST) >= 0
			&& ScriptEngine.RegisterObjectBehaviour(
				"FPropertyForkCarrier",
				asBEHAVE_RELEASE,
				"void f()",
				asFUNCTION(ReleaseCarrier),
				asCALL_CDECL_OBJFIRST) >= 0
			&& ScriptEngine.RegisterGlobalFunction(
				"FPropertyForkCarrier CreatePropertyForkCarrier(int Value)",
				asFUNCTION(CreateCarrier),
				asCALL_GENERIC) >= 0;
	}

	static FString BuildDecoratorSource(const EDecoratorCase Case)
	{
		using namespace AngelscriptNativeTestSupport;

		FString Source;
		AppendGeneratedAsLine(Source, TEXT("class FPropertyDecoratorProbe"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		switch (Case)
		{
		case EDecoratorCase::Getter:
			AppendGeneratedAsLine(Source, TEXT("\tint get_Value() property"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t\treturn 41;"));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
			break;
		case EDecoratorCase::Setter:
			AppendGeneratedAsLine(Source, TEXT("\tvoid set_Value(int Value) property"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
			break;
		case EDecoratorCase::IndexedGetter:
			AppendGeneratedAsLine(Source, TEXT("\tint get_Value(int Index) property"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t\treturn Index;"));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
			break;
		case EDecoratorCase::IndexedSetter:
			AppendGeneratedAsLine(Source, TEXT("\tvoid set_Value(int Index, int Value) property"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
			break;
		default:
			break;
		}
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("int RunPropertyDecoratorProbe()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\treturn 97;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		return Source;
	}

	static FString GetNativeDecoratorDeclaration(const EDecoratorCase Case)
	{
		switch (Case)
		{
		case EDecoratorCase::Getter:
			return TEXT("int get_Value() const property");
		case EDecoratorCase::Setter:
			return TEXT("void set_Value(int Value) property");
		case EDecoratorCase::IndexedGetter:
			return TEXT("int get_Value(int Index) const property");
		case EDecoratorCase::IndexedSetter:
			return TEXT("void set_Value(int Index, int Value) property");
		default:
			return FString();
		}
	}

	static FString BuildAutomaticAccessSource(const EAutomaticAccessCase Case)
	{
		using namespace AngelscriptNativeTestSupport;

		FString Source;
		AppendGeneratedAsLine(Source, TEXT("int RunAutomaticPropertyAccess()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tFPropertyForkCarrier Receiver = CreatePropertyForkCarrier(31);"));
		switch (Case)
		{
		case EAutomaticAccessCase::Read:
			AppendGeneratedAsLine(Source, TEXT("\treturn Receiver.Value;"));
			break;
		case EAutomaticAccessCase::Write:
			AppendGeneratedAsLine(Source, TEXT("\tReceiver.Value = 41;"));
			AppendGeneratedAsLine(Source, TEXT("\treturn Receiver.Value;"));
			break;
		case EAutomaticAccessCase::IndexedRead:
			AppendGeneratedAsLine(Source, TEXT("\treturn Receiver.Value[2];"));
			break;
		case EAutomaticAccessCase::IndexedWrite:
			AppendGeneratedAsLine(Source, TEXT("\tReceiver.Value[2] = 41;"));
			AppendGeneratedAsLine(Source, TEXT("\treturn Receiver.Value[2];"));
			break;
		default:
			break;
		}
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		return Source;
	}

	static FString BuildDirectMethodSource()
	{
		using namespace AngelscriptNativeTestSupport;

		FString Source;
		AppendGeneratedAsLine(Source, TEXT("int RunDirectPropertyMethod()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tFPropertyForkCarrier Receiver = CreatePropertyForkCarrier(31);"));
		AppendGeneratedAsLine(Source, TEXT("\tReceiver.SetValue(41);"));
		AppendGeneratedAsLine(Source, TEXT("\tReceiver.SetIndexedValue(2, 43);"));
		AppendGeneratedAsLine(Source, TEXT("\treturn Receiver.GetValue() + Receiver.GetIndexedValue(2);"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		return Source;
	}

	int32 CompileSource(
		FNativeTestEngine& Engine,
		const FNativeCaseContext& Case,
		const FString& ModuleName,
		const FString& Source,
		asIScriptModule*& OutModule)
	{
		using namespace AngelscriptNativeTestSupport;

		PrintGeneratedAsSource(*TestRunner, Case.GetId(), ModuleName, Source);
		const FTCHARToUTF8 ModuleNameUtf8(*ModuleName);
		const FTCHARToUTF8 SourceUtf8(*Source);
		Engine.ResetMessages();
		const int32 CompileResult = CompileNativeModule(Engine.Get(), ModuleNameUtf8.Get(), SourceUtf8.Get(), OutModule);
		if (CompileResult < 0)
		{
			TestRunner->AddInfo(FString::Printf(
				TEXT("[%s] compiler diagnostics:\n%s"),
				*Case.GetId(),
				*Engine.GetMessagesText()));
		}
		return CompileResult;
	}

public:
	TEST_METHOD(DecoratorRegistrationAndAutomaticAccessAreRejected)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT("LANG-PROP-FORK-SEMANTICS",
			ENativeEvidence::Compile
				| ENativeEvidence::Diagnostic
				| ENativeEvidence::Runtime
				| ENativeEvidence::Metadata
				| ENativeEvidence::Lifecycle
				| ENativeEvidence::Cleanup
				| ENativeEvidence::Isolation);

		for (const EDecoratorCase DecoratorCase : DecoratorCases)
		{
			const FNativeCaseContext Case(MakeNativeCaseId(
				"LANG-PROP-FORK-SEMANTICS",
				{ TEXT("script-decorator"), GetDecoratorCaseName(DecoratorCase) }));
			const FString ModuleName = FString::Printf(
				TEXT("PropertyForkDecorator_%s"),
				GetDecoratorCaseName(DecoratorCase));
			const FString Source = BuildDecoratorSource(DecoratorCase);
			FNativeTestEngine Engine;
			Engine.Create(*TestRunner);
			ON_SCOPE_EXIT { Engine.Destroy(); };
			asIScriptModule* Module = nullptr;
			const int32 CompileResult = CompileSource(Engine, Case, ModuleName, Source, Module);
			ASSERT_THAT(IsTrue(CompileResult < 0,
				*Case.Describe(TEXT("removed property decorator source should be rejected"))));
			ASSERT_THAT(IsTrue(HasDiagnosticContaining(Engine, TEXT("The 'property' decorator has been removed")),
				*Case.Describe(TEXT("removed property decorator source should retain its exact diagnostic"))));
			const FTCHARToUTF8 ModuleNameUtf8(*ModuleName);
			Engine.Get()->DiscardModule(ModuleNameUtf8.Get());
			ASSERT_THAT(IsNull(Engine.Get()->GetModule(ModuleNameUtf8.Get(), asGM_ONLY_IF_EXISTS),
				*Case.Describe(TEXT("rejected decorator source should leave no module registration"))));
		}

		for (const EDecoratorCase DecoratorCase : DecoratorCases)
		{
			const FNativeCaseContext Case(MakeNativeCaseId(
				"LANG-PROP-FORK-SEMANTICS",
				{ TEXT("native-registration"), GetDecoratorCaseName(DecoratorCase) }));
			FNativeTestEngine RejectionEngine;
			RejectionEngine.Create(*TestRunner);
			ON_SCOPE_EXIT { RejectionEngine.Destroy(); };
			asIScriptEngine* const RejectionScriptEngine = RejectionEngine.Get();
			ASSERT_THAT(IsNotNull(RejectionScriptEngine,
				*Case.Describe(TEXT("native registration rejection should create a raw SDK engine"))));
			if (RejectionScriptEngine == nullptr)
			{
				continue;
			}

			ASSERT_THAT(IsTrue(RegisterCarrierBase(*RejectionScriptEngine),
				*Case.Describe(TEXT("native registration rejection should register the implicit-reference carrier"))));
			const FString Declaration = GetNativeDecoratorDeclaration(DecoratorCase);
			const FTCHARToUTF8 DeclarationUtf8(*Declaration);
			const bool bGetter = DecoratorCase == EDecoratorCase::Getter
				|| DecoratorCase == EDecoratorCase::IndexedGetter;
			const int32 Result = RejectionScriptEngine->RegisterObjectMethod(
				"FPropertyForkCarrier",
				DeclarationUtf8.Get(),
				bGetter
					? asFUNCTION(GetValue)
					: asFUNCTION(SetValue),
				asCALL_GENERIC);
			TestRunner->AddInfo(FString::Printf(
				TEXT("[%s] native registration result=%d diagnostics:\n%s"),
				*Case.GetId(),
				Result,
				*RejectionEngine.GetMessagesText()));
			ASSERT_THAT(AreEqual(static_cast<int32>(asINVALID_DECLARATION), Result,
				*Case.Describe(TEXT("native property decorator registration should return asINVALID_DECLARATION"))));
		}

		FCarrierState State;
		ActiveState = &State;
		ON_SCOPE_EXIT { ActiveState = nullptr; };
		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };
		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine,
			TEXT("property fork-semantics owner should create a raw SDK engine")));
		if (ScriptEngine == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsTrue(RegisterCarrierBase(*ScriptEngine),
			TEXT("property fork-semantics owner should register its implicit-reference carrier")));

		ASSERT_THAT(IsTrue(ScriptEngine->RegisterObjectMethod(
			"FPropertyForkCarrier",
			"int GetValue() const",
			asFUNCTION(GetValue),
			asCALL_GENERIC) >= 0,
			TEXT("decorator-free GetValue should remain an ordinary native method")));
		ASSERT_THAT(IsTrue(ScriptEngine->RegisterObjectMethod(
			"FPropertyForkCarrier",
			"void SetValue(int Value)",
			asFUNCTION(SetValue),
			asCALL_GENERIC) >= 0,
			TEXT("decorator-free SetValue should remain an ordinary native method")));
		ASSERT_THAT(IsTrue(ScriptEngine->RegisterObjectMethod(
			"FPropertyForkCarrier",
			"int GetIndexedValue(int Index) const",
			asFUNCTION(GetIndexedValue),
			asCALL_GENERIC) >= 0,
			TEXT("decorator-free indexed getter should remain an ordinary native method")));
		ASSERT_THAT(IsTrue(ScriptEngine->RegisterObjectMethod(
			"FPropertyForkCarrier",
			"void SetIndexedValue(int Index, int Value)",
			asFUNCTION(SetIndexedValue),
			asCALL_GENERIC) >= 0,
			TEXT("decorator-free indexed setter should remain an ordinary native method")));

		for (const EAutomaticAccessCase AutomaticAccessCase : AutomaticAccessCases)
		{
			State.ResetCalls();
			const FNativeCaseContext Case(MakeNativeCaseId(
				"LANG-PROP-FORK-SEMANTICS",
				{ TEXT("automatic-access"), GetAutomaticAccessCaseName(AutomaticAccessCase) }));
			const FString ModuleName = FString::Printf(
				TEXT("PropertyForkAutomatic_%s"),
				GetAutomaticAccessCaseName(AutomaticAccessCase));
			const FString Source = BuildAutomaticAccessSource(AutomaticAccessCase);
			asIScriptModule* Module = nullptr;
			const int32 CompileResult = CompileSource(Engine, Case, ModuleName, Source, Module);
			ASSERT_THAT(IsTrue(CompileResult < 0,
				*Case.Describe(TEXT("automatic property access should be rejected without a property trait"))));
			ASSERT_THAT(IsTrue(HasDiagnosticContaining(Engine, TEXT("'Value' is not a member")),
				*Case.Describe(TEXT("automatic property rejection should identify the unresolved member"))));
			ASSERT_THAT(AreEqual(0, State.GetterCalls,
				*Case.Describe(TEXT("rejected automatic property access should invoke no getter callback"))));
			ASSERT_THAT(AreEqual(0, State.SetterCalls,
				*Case.Describe(TEXT("rejected automatic property access should invoke no setter callback"))));
			const FTCHARToUTF8 ModuleNameUtf8(*ModuleName);
			ScriptEngine->DiscardModule(ModuleNameUtf8.Get());
			ASSERT_THAT(IsNull(ScriptEngine->GetModule(ModuleNameUtf8.Get(), asGM_ONLY_IF_EXISTS),
				*Case.Describe(TEXT("rejected automatic property source should leave no module registration"))));
		}

		State.ResetCalls();
		const FNativeCaseContext DirectCase(MakeNativeCaseId(
			"LANG-PROP-FORK-SEMANTICS",
			{ TEXT("direct-method"), TEXT("read-write-indexed") }));
		const FString DirectModuleName = TEXT("PropertyForkDirectMethod");
		const FString DirectSource = BuildDirectMethodSource();
		asIScriptModule* DirectModule = nullptr;
		const int32 DirectCompileResult = CompileSource(
			Engine,
			DirectCase,
			DirectModuleName,
			DirectSource,
			DirectModule);
		ASSERT_THAT(IsTrue(DirectCompileResult >= 0,
			*DirectCase.Describe(TEXT("direct GetX and SetX method source should compile"))));
		asIScriptFunction* const DirectEntry = DirectModule != nullptr
			? DirectModule->GetFunctionByDecl("int RunDirectPropertyMethod()")
			: nullptr;
		ASSERT_THAT(IsNotNull(DirectEntry,
			*DirectCase.Describe(TEXT("direct method source should publish its exact entry"))));
		if (DirectEntry != nullptr)
		{
			asIScriptContext* const Context = ScriptEngine->CreateContext();
			ASSERT_THAT(IsNotNull(Context,
				*DirectCase.Describe(TEXT("direct method source should create an execution context"))));
			if (Context != nullptr)
			{
				ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_FINISHED),
					PrepareAndExecute(Context, DirectEntry),
					*DirectCase.Describe(TEXT("direct method source should execute"))));
				const int32 ReturnValue = static_cast<int32>(Context->GetReturnDWord());
				TestRunner->AddInfo(FString::Printf(
					TEXT("[%s] direct normal-method result=%d"),
					*DirectCase.GetId(),
					ReturnValue));
				ASSERT_THAT(AreEqual(88, ReturnValue,
					*DirectCase.Describe(TEXT("direct methods should retain independent getter and indexed-getter behavior"))));
				Context->Release();
			}
		}
		ASSERT_THAT(AreEqual(2, State.GetterCalls,
			*DirectCase.Describe(TEXT("direct method source should call both getters exactly once"))));
		ASSERT_THAT(AreEqual(2, State.SetterCalls,
			*DirectCase.Describe(TEXT("direct method source should call both setters exactly once"))));
		const FTCHARToUTF8 DirectModuleNameUtf8(*DirectModuleName);
		ScriptEngine->DiscardModule(DirectModuleNameUtf8.Get());
		ASSERT_THAT(IsNull(ScriptEngine->GetModule(DirectModuleNameUtf8.Get(), asGM_ONLY_IF_EXISTS),
			*DirectCase.Describe(TEXT("direct method source should discard its module"))));
		ASSERT_THAT(AreEqual(0, State.LiveObjects,
			*DirectCase.Describe(TEXT("property fork-semantics owner should release every carrier"))));
		ASSERT_THAT(AreEqual(State.CreatedObjects, State.DestroyedObjects,
			*DirectCase.Describe(TEXT("property fork-semantics owner should balance carrier lifetime"))));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
