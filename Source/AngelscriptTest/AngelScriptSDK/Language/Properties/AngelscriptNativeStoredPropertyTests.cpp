#include "../../Support/AngelscriptNativeCaseTestSupport.h"
#include "../../Support/AngelscriptNativeCoreTestSupport.h"
#include "../../Support/AngelscriptNativeFixtureTestSupport.h"
#include "../../Support/AngelscriptNativeLanguageCaseTestSupport.h"

#include "CQTest.h"
#include "Misc/ScopeExit.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FStoredPropertyTests,
	"Angelscript.TestModule.AngelScriptSDK.Language.Properties.Stored",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	using FNativeCaseContext = AngelscriptNativeTestSupport::FNativeCaseContext;
	using FNativeLifecycleRecorder = AngelscriptNativeTestSupport::FNativeLifecycleRecorder;
	using FNativeTypeCase = AngelscriptNativeTestSupport::FNativeTypeCase;
	using ENativeLifecycleEvent = AngelscriptNativeTestSupport::ENativeLifecycleEvent;
	using ENativeValueCategory = AngelscriptNativeTestSupport::ENativeValueCategory;

	struct FOperationCase
	{
		const ANSICHAR* CatalogName;
	};

	struct FReceiverCase
	{
		const ANSICHAR* CatalogName;
		int32 Marker;
	};

	inline static constexpr FOperationCase OperationCases[] =
	{
		{ "default_read" },
		{ "write" },
		{ "compound_write" },
		{ "copy" },
		{ "reference_mutation" },
	};

	inline static constexpr FReceiverCase ReceiverCases[] =
	{
		{ "mutable", 11 },
		{ "const", 11 },
		{ "base_view", 22 },
		{ "derived_view", 22 },
	};

	static bool IsOperation(const FOperationCase& OperationCase, const ANSICHAR* Name)
	{
		return FCStringAnsi::Strcmp(OperationCase.CatalogName, Name) == 0;
	}

	static bool IsReceiver(const FReceiverCase& ReceiverCase, const ANSICHAR* Name)
	{
		return FCStringAnsi::Strcmp(ReceiverCase.CatalogName, Name) == 0;
	}

	static bool IsValueObjectType(const FNativeTypeCase& TypeCase)
	{
		return TypeCase.Category == ENativeValueCategory::ScriptValue
			|| TypeCase.Category == ENativeValueCategory::NativeValue;
	}

	static bool SupportsCompoundWrite(const FNativeTypeCase& TypeCase)
	{
		return TypeCase.Category == ENativeValueCategory::SignedInteger
			|| TypeCase.Category == ENativeValueCategory::UnsignedInteger
			|| TypeCase.Category == ENativeValueCategory::FloatingPoint
			|| TypeCase.Category == ENativeValueCategory::Typedef;
	}

	static bool ShouldCompile(
		const FNativeTypeCase& TypeCase,
		const FOperationCase& OperationCase,
		const FReceiverCase& ReceiverCase)
	{
		if (IsReceiver(ReceiverCase, "const")
			&& !IsOperation(OperationCase, "default_read")
			&& !IsOperation(OperationCase, "copy"))
		{
			return false;
		}
		if (IsOperation(OperationCase, "compound_write"))
		{
			return SupportsCompoundWrite(TypeCase);
		}
		return true;
	}

	static FString MakeSuffix(
		const FOperationCase& OperationCase,
		const FReceiverCase& ReceiverCase,
		const FNativeTypeCase& TypeCase)
	{
		return FString::Printf(
			TEXT("%hs_%hs_%hs"),
			OperationCase.CatalogName,
			ReceiverCase.CatalogName,
			TypeCase.CatalogName);
	}

	static FString MakeTypedValue(const FNativeTypeCase& TypeCase, const int32 Value)
	{
		if (TypeCase.Category == ENativeValueCategory::Boolean)
		{
			return Value == 0 ? TEXT("false") : TEXT("true");
		}
		if (TypeCase.Category == ENativeValueCategory::Enum)
		{
			return Value == 0 ? TEXT("ENativeCaseEnum::Zero") : TEXT("ENativeCaseEnum::One");
		}
		return FString::Printf(TEXT("%hs(%d)"), TypeCase.ScriptType, Value);
	}

	static int32 SourceObservedValue(const FNativeTypeCase& TypeCase)
	{
		return TypeCase.Category == ENativeValueCategory::Boolean
			|| TypeCase.Category == ENativeValueCategory::Enum
			? 1
			: 29;
	}

	static int32 CopyMutationValue(const FNativeTypeCase& TypeCase)
	{
		return TypeCase.Category == ENativeValueCategory::Boolean
			|| TypeCase.Category == ENativeValueCategory::Enum
			? 1
			: 37;
	}

	static int32 ExpectedCoreResult(
		const FNativeTypeCase& TypeCase,
		const FOperationCase& OperationCase)
	{
		if (IsOperation(OperationCase, "default_read"))
		{
			return 0;
		}
		if (IsOperation(OperationCase, "copy"))
		{
			return CopyMutationValue(TypeCase);
		}
		return SourceObservedValue(TypeCase);
	}

	static int32 ExpectedResult(
		const FNativeTypeCase& TypeCase,
		const FOperationCase& OperationCase,
		const FReceiverCase& ReceiverCase)
	{
		return ExpectedCoreResult(TypeCase, OperationCase) * 100 + ReceiverCase.Marker;
	}

	static void AppendTypeDeclarations(FString& Source, const FNativeTypeCase& TypeCase)
	{
		using namespace AngelscriptNativeTestSupport;

		// NativeCaseAlias is registered through RegisterCoreLanguageTypedef because this fork rejects script typedef syntax.
		if (TypeCase.Category == ENativeValueCategory::Enum)
		{
			AppendGeneratedAsLine(Source, TEXT("enum ENativeCaseEnum"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\tZero = 0,"));
			AppendGeneratedAsLine(Source, TEXT("\tOne = 1"));
			AppendGeneratedAsLine(Source, TEXT("}"));
			AppendGeneratedAsLine(Source);
		}
		else if (TypeCase.Category == ENativeValueCategory::ScriptValue)
		{
			AppendGeneratedAsLine(Source, TEXT("struct FScriptCaseValue"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\tint Value = 0;"));
			AppendGeneratedAsLine(Source, TEXT("\tint ObjectId = -1;"));
			AppendGeneratedAsLine(Source);
			AppendGeneratedAsLine(Source, TEXT("\tFScriptCaseValue()"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t\tObjectId = BeginNativeScriptLifecycle(Value);"));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
			AppendGeneratedAsLine(Source);
			AppendGeneratedAsLine(Source, TEXT("\tFScriptCaseValue(int InValue)"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t\tValue = InValue;"));
			AppendGeneratedAsLine(Source, TEXT("\t\tObjectId = BeginNativeScriptLifecycle(Value);"));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
			AppendGeneratedAsLine(Source);
			AppendGeneratedAsLine(Source, TEXT("\tFScriptCaseValue(const FScriptCaseValue& Other)"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t\tValue = Other.Value;"));
			AppendGeneratedAsLine(Source, TEXT("\t\tObjectId = CopyNativeScriptLifecycle(Other.ObjectId, Value);"));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
			AppendGeneratedAsLine(Source);
			AppendGeneratedAsLine(Source, TEXT("\tFScriptCaseValue& opAssign(const FScriptCaseValue& Other)"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t\tValue = Other.Value;"));
			AppendGeneratedAsLine(Source, TEXT("\t\tAssignNativeScriptLifecycle(ObjectId, Other.ObjectId, Value);"));
			AppendGeneratedAsLine(Source, TEXT("\t\treturn this;"));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
			AppendGeneratedAsLine(Source);
			AppendGeneratedAsLine(Source, TEXT("\t~FScriptCaseValue()"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t\tEndNativeScriptLifecycle(ObjectId, Value);"));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
			AppendGeneratedAsLine(Source, TEXT("}"));
			AppendGeneratedAsLine(Source);
		}
	}

	static void AppendObservationFunction(FString& Source, const FNativeTypeCase& TypeCase)
	{
		using namespace AngelscriptNativeTestSupport;

		if (IsValueObjectType(TypeCase))
		{
			AppendGeneratedAsLine(Source, FString::Printf(
				TEXT("int ObserveStoredProperty(const %hs& in Value)"),
				TypeCase.ScriptType));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\treturn Value.Value;"));
		}
		else if (TypeCase.Category == ENativeValueCategory::Boolean)
		{
			AppendGeneratedAsLine(Source, TEXT("int ObserveStoredProperty(bool Value)"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\treturn Value ? 1 : 0;"));
		}
		else
		{
			AppendGeneratedAsLine(Source, FString::Printf(
				TEXT("int ObserveStoredProperty(%hs Value)"),
				TypeCase.ScriptType));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\treturn int(Value);"));
		}
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
	}

	static void AppendOwnerTypes(FString& Source, const FNativeTypeCase& TypeCase)
	{
		using namespace AngelscriptNativeTestSupport;

		AppendGeneratedAsLine(Source, TEXT("class FStoredPropertyBase"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, FString::Printf(TEXT("\t%hs Stored;"), TypeCase.ScriptType));
		AppendGeneratedAsLine(Source, TEXT("\tint ReceiverMarker = 11;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("class FStoredPropertyDerived : FStoredPropertyBase"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tFStoredPropertyDerived()"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(Source, TEXT("\t\tReceiverMarker = 22;"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
	}

	static void AppendReferenceMutationFunction(FString& Source, const FNativeTypeCase& TypeCase)
	{
		using namespace AngelscriptNativeTestSupport;

		AppendGeneratedAsLine(Source, FString::Printf(
			TEXT("void MutateStoredProperty(%hs& inout Value)"),
			TypeCase.ScriptType));
		AppendGeneratedAsLine(Source, TEXT("{"));
		if (IsValueObjectType(TypeCase))
		{
			AppendGeneratedAsLine(Source, FString::Printf(
				TEXT("\tValue.Value = %d;"),
				SourceObservedValue(TypeCase)));
		}
		else
		{
			AppendGeneratedAsLine(Source, FString::Printf(
				TEXT("\tValue = %s;"),
				*MakeTypedValue(TypeCase, SourceObservedValue(TypeCase))));
		}
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
	}

	static void AppendReceiverDeclaration(
		FString& Source,
		const FReceiverCase& ReceiverCase)
	{
		using namespace AngelscriptNativeTestSupport;

		if (IsReceiver(ReceiverCase, "mutable"))
		{
			AppendGeneratedAsLine(Source, TEXT("\tFStoredPropertyBase Receiver = FStoredPropertyBase();"));
		}
		else if (IsReceiver(ReceiverCase, "const"))
		{
			AppendGeneratedAsLine(Source, TEXT("\tconst FStoredPropertyBase Receiver = FStoredPropertyBase();"));
		}
		else if (IsReceiver(ReceiverCase, "base_view"))
		{
			AppendGeneratedAsLine(Source, TEXT("\tFStoredPropertyDerived DerivedReceiver = FStoredPropertyDerived();"));
			AppendGeneratedAsLine(Source, TEXT("\tFStoredPropertyBase Receiver = DerivedReceiver;"));
		}
		else
		{
			AppendGeneratedAsLine(Source, TEXT("\tFStoredPropertyDerived Receiver = FStoredPropertyDerived();"));
		}
	}

	static void AppendCopyMutation(
		FString& Source,
		const FNativeTypeCase& TypeCase)
	{
		using namespace AngelscriptNativeTestSupport;

		if (IsValueObjectType(TypeCase))
		{
			AppendGeneratedAsLine(Source, FString::Printf(
				TEXT("\tCopyValue.Value = %d;"),
				CopyMutationValue(TypeCase)));
		}
		else
		{
			AppendGeneratedAsLine(Source, FString::Printf(
				TEXT("\tCopyValue = %s;"),
				*MakeTypedValue(TypeCase, CopyMutationValue(TypeCase))));
		}
	}

	static void AppendOperation(
		FString& Source,
		const FNativeTypeCase& TypeCase,
		const FOperationCase& OperationCase)
	{
		using namespace AngelscriptNativeTestSupport;

		if (IsOperation(OperationCase, "write"))
		{
			AppendGeneratedAsLine(Source, FString::Printf(
				TEXT("\tReceiver.Stored = %s;"),
				*MakeTypedValue(TypeCase, SourceObservedValue(TypeCase))));
		}
		else if (IsOperation(OperationCase, "compound_write"))
		{
			AppendGeneratedAsLine(Source, FString::Printf(
				TEXT("\tReceiver.Stored += %s;"),
				*MakeTypedValue(TypeCase, SourceObservedValue(TypeCase))));
		}
		else if (IsOperation(OperationCase, "copy"))
		{
			AppendGeneratedAsLine(Source, FString::Printf(
				TEXT("\t%hs CopyValue = Receiver.Stored;"),
				TypeCase.ScriptType));
			AppendCopyMutation(Source, TypeCase);
		}
		else if (IsOperation(OperationCase, "reference_mutation"))
		{
			AppendGeneratedAsLine(Source, TEXT("\tMutateStoredProperty(Receiver.Stored);"));
		}
	}

	static void AppendResult(FString& Source, const FOperationCase& OperationCase)
	{
		using namespace AngelscriptNativeTestSupport;

		if (IsOperation(OperationCase, "copy"))
		{
			AppendGeneratedAsLine(Source, TEXT("\tint CoreResult = ObserveStoredProperty(Receiver.Stored) * 100 + ObserveStoredProperty(CopyValue);"));
		}
		else
		{
			AppendGeneratedAsLine(Source, TEXT("\tint CoreResult = ObserveStoredProperty(Receiver.Stored);"));
		}
		AppendGeneratedAsLine(Source, TEXT("\treturn CoreResult * 100 + Receiver.ReceiverMarker;"));
	}

	static FString BuildStoredPropertySource(
		const FNativeTypeCase& TypeCase,
		const FOperationCase& OperationCase,
		const FReceiverCase& ReceiverCase)
	{
		using namespace AngelscriptNativeTestSupport;

		FString Source;
		AppendTypeDeclarations(Source, TypeCase);
		AppendObservationFunction(Source, TypeCase);
		AppendOwnerTypes(Source, TypeCase);
		if (IsOperation(OperationCase, "reference_mutation"))
		{
			AppendReferenceMutationFunction(Source, TypeCase);
		}
		AppendGeneratedAsLine(Source, TEXT("int RunStoredProperty()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendReceiverDeclaration(Source, ReceiverCase);
		AppendOperation(Source, TypeCase, OperationCase);
		AppendResult(Source, OperationCase);
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		return Source;
	}

	static FString BuildRecoverySource()
	{
		using namespace AngelscriptNativeTestSupport;

		FString Source;
		AppendGeneratedAsLine(Source, TEXT("int RunStoredPropertyRecovery()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\treturn 97;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		return Source;
	}

	static bool HasLocatedError(const AngelscriptNativeTestSupport::FNativeMessageCollector& Messages)
	{
		return Messages.Entries.ContainsByPredicate([](const AngelscriptNativeTestSupport::FNativeMessageEntry& Entry)
		{
			return Entry.Type == asMSGTYPE_ERROR
				&& Entry.Row > 0
				&& Entry.Column > 0
				&& !Entry.Section.IsEmpty()
				&& !Entry.Message.IsEmpty();
		});
	}

	void VerifyMetadata(
		const FNativeCaseContext& Case,
		const FNativeTypeCase& TypeCase,
		const FReceiverCase& ReceiverCase,
		asIScriptModule& Module)
	{
		asITypeInfo* const BaseType = Module.GetTypeInfoByName("FStoredPropertyBase");
		asITypeInfo* const DerivedType = Module.GetTypeInfoByName("FStoredPropertyDerived");
		ASSERT_THAT(IsNotNull(BaseType,
			*Case.Describe(TEXT("stored-property cell should publish its base owner type"))));
		ASSERT_THAT(IsNotNull(DerivedType,
			*Case.Describe(TEXT("stored-property cell should publish its derived owner type"))));
		if (BaseType != nullptr)
		{
			const char* Name = nullptr;
			int TypeId = asTYPEID_VOID;
			ASSERT_THAT(IsTrue(BaseType->GetProperty(0, &Name, &TypeId) >= 0
				&& Name != nullptr
				&& FCStringAnsi::Strcmp(Name, "Stored") == 0,
				*Case.Describe(TEXT("stored-property base should expose the exact Stored field"))));
			ASSERT_THAT(AreEqual(Module.GetTypeIdByDecl(TypeCase.ScriptType), TypeId,
				*Case.Describe(TEXT("Stored field should preserve its catalog type metadata"))));
		}
		if (BaseType != nullptr && DerivedType != nullptr)
		{
			ASSERT_THAT(AreEqual(BaseType, DerivedType->GetBaseType(),
				*Case.Describe(TEXT("derived property receiver should preserve the exact base relation"))));
		}

		asIScriptFunction* const Entry = Module.GetFunctionByDecl("int RunStoredProperty()");
		ASSERT_THAT(IsNotNull(Entry,
			*Case.Describe(TEXT("stored-property cell should expose its exact entry declaration"))));
		if (Entry != nullptr && IsReceiver(ReceiverCase, "const"))
		{
			bool bFoundConstReceiver = false;
			for (asUINT Index = 0; Index < Entry->GetVarCount(); ++Index)
			{
				const char* Name = nullptr;
				if (Entry->GetVar(Index, &Name) >= 0
					&& Name != nullptr
					&& FCStringAnsi::Strcmp(Name, "Receiver") == 0)
				{
					const char* const Declaration = Entry->GetVarDecl(Index, true);
					bFoundConstReceiver = Declaration != nullptr
						&& FString(UTF8_TO_TCHAR(Declaration)).Contains(TEXT("const"));
				}
			}
			ASSERT_THAT(IsTrue(bFoundConstReceiver,
				*Case.Describe(TEXT("const receiver should retain its const debug declaration"))));
		}
	}

	void VerifyLifecycle(
		const FNativeCaseContext& Case,
		const FNativeTypeCase& TypeCase,
		const FOperationCase& OperationCase,
		const FNativeLifecycleRecorder& Lifecycle)
	{
		ASSERT_THAT(AreEqual(0, Lifecycle.GetLiveObjectCount(),
			*Case.Describe(TEXT("stored-property execution should leave no tracked value alive"))));
		if (!IsValueObjectType(TypeCase))
		{
			ASSERT_THAT(AreEqual(0, Lifecycle.GetEntries().Num(),
				*Case.Describe(TEXT("primitive stored property should not produce object lifecycle events"))));
			return;
		}

		TSet<int32> ConstructedIds;
		TSet<int32> DestructedIds;
		for (const AngelscriptNativeTestSupport::FNativeLifecycleEntry& Entry : Lifecycle.GetEntries())
		{
			if (Entry.Event == ENativeLifecycleEvent::DefaultConstruct
				|| Entry.Event == ENativeLifecycleEvent::ValueConstruct
				|| Entry.Event == ENativeLifecycleEvent::CopyConstruct)
			{
				ASSERT_THAT(IsFalse(ConstructedIds.Contains(Entry.ObjectId),
					*Case.Describe(TEXT("property value construction should allocate a unique identity"))));
				ConstructedIds.Add(Entry.ObjectId);
			}
			else if (Entry.Event == ENativeLifecycleEvent::Destruct)
			{
				ASSERT_THAT(IsTrue(ConstructedIds.Contains(Entry.ObjectId),
					*Case.Describe(TEXT("property destructor should identify a constructed value"))));
				ASSERT_THAT(IsFalse(DestructedIds.Contains(Entry.ObjectId),
					*Case.Describe(TEXT("property value should be destroyed no more than once"))));
				DestructedIds.Add(Entry.ObjectId);
			}
		}
		ASSERT_THAT(IsTrue(ConstructedIds.Num() > 0,
			*Case.Describe(TEXT("value-object property should construct real storage"))));
		ASSERT_THAT(AreEqual(ConstructedIds.Num(), DestructedIds.Num(),
			*Case.Describe(TEXT("every stored-property object should have one destructor"))));
		if (IsOperation(OperationCase, "write"))
		{
			ASSERT_THAT(IsTrue(Lifecycle.Num(ENativeLifecycleEvent::Assign) > 0,
				*Case.Describe(TEXT("value-object write should invoke the field assignment behavior"))));
		}
		else if (IsOperation(OperationCase, "reference_mutation"))
		{
			ASSERT_THAT(AreEqual(0, Lifecycle.Num(ENativeLifecycleEvent::Assign),
				*Case.Describe(TEXT("inout field mutation should update nested state without replacing the value object"))));
		}
		else if (IsOperation(OperationCase, "copy"))
		{
			ASSERT_THAT(IsTrue(Lifecycle.Num(ENativeLifecycleEvent::CopyConstruct) > 0,
				*Case.Describe(TEXT("property copy should construct independent local storage"))));
		}
	}

	void ExecuteLegalCell(
		const FNativeCaseContext& Case,
		const FNativeTypeCase& TypeCase,
		const FOperationCase& OperationCase,
		const FReceiverCase& ReceiverCase,
		asIScriptEngine& ScriptEngine,
		asIScriptModule& Module,
		FNativeLifecycleRecorder& Lifecycle)
	{
		using namespace AngelscriptNativeTestSupport;

		VerifyMetadata(Case, TypeCase, ReceiverCase, Module);
		asIScriptFunction* const Entry = Module.GetFunctionByDecl("int RunStoredProperty()");
		if (Entry == nullptr)
		{
			return;
		}
		asIScriptContext* const Context = ScriptEngine.CreateContext();
		ASSERT_THAT(IsNotNull(Context,
			*Case.Describe(TEXT("stored-property cell should create an execution context"))));
		if (Context == nullptr)
		{
			return;
		}
		ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_FINISHED), PrepareAndExecute(Context, Entry),
			*Case.Describe(TEXT("legal stored-property operation should finish"))));
		ASSERT_THAT(AreEqual(ExpectedResult(TypeCase, OperationCase, ReceiverCase), static_cast<int32>(Context->GetReturnDWord()),
			*Case.Describe(TEXT("stored-property result should encode value/copy behavior and receiver identity"))));
		ASSERT_THAT(AreEqual(asSUCCESS, Context->Unprepare(),
			*Case.Describe(TEXT("stored-property context should release receiver storage"))));
		Context->Release();
		VerifyLifecycle(Case, TypeCase, OperationCase, Lifecycle);
	}

	void CompileRecovery(
		const FNativeCaseContext& Case,
		asIScriptEngine& ScriptEngine,
		AngelscriptNativeTestSupport::FNativeTestEngine& Engine,
		const FString& ModuleName)
	{
		using namespace AngelscriptNativeTestSupport;

		const FString RecoverySource = BuildRecoverySource();
		PrintGeneratedAsSource(*TestRunner, Case.GetId() + TEXT("-RECOVERY"), ModuleName, RecoverySource);
		const FTCHARToUTF8 ModuleNameUtf8(*ModuleName);
		const FTCHARToUTF8 RecoverySourceUtf8(*RecoverySource);
		Engine.ResetMessages();
		asIScriptModule* RecoveryModule = nullptr;
		ASSERT_THAT(IsTrue(CompileNativeModule(
			&ScriptEngine,
			ModuleNameUtf8.Get(),
			RecoverySourceUtf8.Get(),
			RecoveryModule) >= 0,
			*Case.Describe(TEXT("rejected property operation should permit same-name recovery"))));
		ASSERT_THAT(IsNotNull(RecoveryModule,
			*Case.Describe(TEXT("stored-property recovery should publish a module"))));
		if (RecoveryModule != nullptr)
		{
			asIScriptFunction* const Recovery = RecoveryModule->GetFunctionByDecl("int RunStoredPropertyRecovery()");
			ASSERT_THAT(IsNotNull(Recovery,
				*Case.Describe(TEXT("stored-property recovery should expose its exact entry"))));
			if (Recovery != nullptr)
			{
				asIScriptContext* const Context = ScriptEngine.CreateContext();
				ASSERT_THAT(IsNotNull(Context,
					*Case.Describe(TEXT("stored-property recovery should create an execution context"))));
				if (Context != nullptr)
				{
					ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_FINISHED), PrepareAndExecute(Context, Recovery),
						*Case.Describe(TEXT("stored-property recovery should execute cleanly"))));
					ASSERT_THAT(AreEqual(97, static_cast<int32>(Context->GetReturnDWord()),
						*Case.Describe(TEXT("stored-property recovery should not retain failed state"))));
					Context->Release();
				}
			}
		}
		ScriptEngine.DiscardModule(ModuleNameUtf8.Get());
	}

public:
	TEST_METHOD(TypesByOperationAndReceiver)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT("LANG-PROP-VALUE-OP",
			ENativeEvidence::Compile
			| ENativeEvidence::Diagnostic
			| ENativeEvidence::Runtime
			| ENativeEvidence::Metadata
			| ENativeEvidence::Lifecycle);

		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };
		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine,
			TEXT("Stored-property product should create a raw SDK engine")));
		if (ScriptEngine == nullptr)
		{
			return;
		}

		FNativeLifecycleRecorder Lifecycle;
		Lifecycle.Reset();
		ASSERT_THAT(IsTrue(RegisterNativeCaseValue(*ScriptEngine, Lifecycle),
			TEXT("Stored-property product should register its tracked native value")));
		ASSERT_THAT(IsTrue(RegisterNativeScriptLifecycleBridge(*ScriptEngine, Lifecycle),
			TEXT("Stored-property product should register script-owned lifecycle callbacks")));
		ASSERT_THAT(IsTrue(RegisterCoreLanguageTypedef(*ScriptEngine),
			TEXT("Stored-property product should register its core typedef alias")));

		for (const FOperationCase& OperationCase : OperationCases)
		{
			for (const FReceiverCase& ReceiverCase : ReceiverCases)
			{
				for (const FNativeTypeCase& TypeCase : NativeTypeCases)
				{
					if (!IsCoreValueTypeCase(TypeCase))
					{
						continue;
					}

					Lifecycle.Reset();
					const FNativeCaseContext Case(MakeNativeCaseId(
						"LANG-PROP-VALUE-OP",
						{
							ANSI_TO_TCHAR(OperationCase.CatalogName),
							ANSI_TO_TCHAR(ReceiverCase.CatalogName),
							ANSI_TO_TCHAR(TypeCase.CatalogName),
						}));
					const FString Suffix = MakeSuffix(OperationCase, ReceiverCase, TypeCase);
					const FString ModuleName = TEXT("StoredProperty_") + Suffix;
					const FString Source = BuildStoredPropertySource(TypeCase, OperationCase, ReceiverCase);
					PrintGeneratedAsSource(*TestRunner, Case.GetId(), ModuleName, Source);
					const FTCHARToUTF8 ModuleNameUtf8(*ModuleName);
					const FTCHARToUTF8 SourceUtf8(*Source);
					Engine.ResetMessages();
					asIScriptModule* Module = nullptr;
					const int BuildResult = CompileNativeModule(
						ScriptEngine,
						ModuleNameUtf8.Get(),
						SourceUtf8.Get(),
						Module);
					const bool bShouldCompile = ShouldCompile(TypeCase, OperationCase, ReceiverCase);
					if (bShouldCompile)
					{
						ASSERT_THAT(IsTrue(BuildResult >= 0,
							*FString::Printf(TEXT("%s. BuildResult=%d Messages={%s}"),
								*Case.Describe(TEXT("legal stored-property cell should compile")),
								BuildResult,
								*Engine.GetMessagesText())));
						ASSERT_THAT(IsNotNull(Module,
							*Case.Describe(TEXT("legal stored-property cell should publish a module"))));
						if (BuildResult >= 0 && Module != nullptr)
						{
							ExecuteLegalCell(
								Case,
								TypeCase,
								OperationCase,
								ReceiverCase,
								*ScriptEngine,
								*Module,
								Lifecycle);
						}
					}
					else
					{
						ASSERT_THAT(IsTrue(BuildResult < 0,
							*Case.Describe(TEXT("const or unsupported stored-property cell should be rejected"))));
						ASSERT_THAT(IsTrue(HasLocatedError(Engine.GetMessages()),
							*Case.Describe(TEXT("rejected stored-property cell should report a located diagnostic"))));
						ASSERT_THAT(AreEqual(0, Lifecycle.GetLiveObjectCount(),
							*Case.Describe(TEXT("failed stored-property build should create no runtime value"))));
					}

					ScriptEngine->DiscardModule(ModuleNameUtf8.Get());
					ASSERT_THAT(IsNull(ScriptEngine->GetModule(ModuleNameUtf8.Get(), asGM_ONLY_IF_EXISTS),
						*Case.Describe(TEXT("stored-property cell should discard its isolated module"))));
					if (!bShouldCompile)
					{
						CompileRecovery(Case, *ScriptEngine, Engine, ModuleName);
						ASSERT_THAT(IsNull(ScriptEngine->GetModule(ModuleNameUtf8.Get(), asGM_ONLY_IF_EXISTS),
							*Case.Describe(TEXT("stored-property recovery should leave no module behind"))));
					}
				}
			}
		}
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
