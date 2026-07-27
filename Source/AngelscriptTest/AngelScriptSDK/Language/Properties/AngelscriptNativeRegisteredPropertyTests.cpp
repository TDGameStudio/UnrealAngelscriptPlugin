#include "../../Support/AngelscriptNativeCaseTestSupport.h"
#include "../../Support/AngelscriptNativeCoreTestSupport.h"
#include "../../Support/AngelscriptNativeLanguageCaseTestSupport.h"

#include "CQTest.h"
#include "Misc/ScopeExit.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS_AND_TAGS(FRegisteredPropertyTests,
	"Angelscript.TestModule.AngelScriptSDK.Language.Properties.RegisteredAccessor",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter | EAutomationTestFlags::Disabled,
	TEXT("#as-v238-backport"))
{
private:
	using FNativeCaseContext = AngelscriptNativeTestSupport::FNativeCaseContext;

	enum class EAccessorSet : uint8
	{
		Getter,
		Setter,
		Both,
	};

	enum class EOperation : uint8
	{
		Read,
		Write,
		Compound,
	};

	enum class ECallbackBehavior : uint8
	{
		Normal,
		RecursiveGetter,
		RecursiveSetter,
		ThrowingGetter,
		ThrowingSetter,
	};

	struct FScenarioCase
	{
		const ANSICHAR* CatalogName;
		EAccessorSet AccessorSet;
		EOperation Operation;
		ECallbackBehavior Behavior;
		bool bConstReceiver;
		bool bConstGetter;
		bool bShouldCompile;
		bool bShouldThrow;
		const TCHAR* CompileDiagnostic;
		int32 ExpectedValue;
	};

	struct FSourceShapeCase
	{
		const ANSICHAR* CatalogName;
	};

	struct FAccessorState
	{
		ECallbackBehavior Behavior = ECallbackBehavior::Normal;
		TArray<int32> Trace;
		int32 RecursionDepth = 0;
		int32 LiveObjects = 0;
		int32 CreatedObjects = 0;
		int32 DestroyedObjects = 0;
	};

	class FRegisteredPropertyObject
	{
	public:
		FRegisteredPropertyObject(FAccessorState& InState, const int32 InValue)
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

		FAccessorState& State;
		int32 Value = 0;

	private:
		~FRegisteredPropertyObject()
		{
			--State.LiveObjects;
			++State.DestroyedObjects;
		}

		int32 ReferenceCount = 1;
	};

	struct FRegistrationResult
	{
		int32 GetterFunctionId = asNO_FUNCTION;
		int32 SetterFunctionId = asNO_FUNCTION;
		bool bSucceeded = false;
	};

	inline static constexpr FScenarioCase ScenarioCases[] =
	{
		{ "getter_read_mutable", EAccessorSet::Getter, EOperation::Read, ECallbackBehavior::Normal, false, true, true, false, TEXT(""), 31 },
		{ "getter_read_const", EAccessorSet::Getter, EOperation::Read, ECallbackBehavior::Normal, true, true, true, false, TEXT(""), 31 },
		{ "getter_nonconst_read_mutable", EAccessorSet::Getter, EOperation::Read, ECallbackBehavior::Normal, false, false, true, false, TEXT(""), 31 },
		{ "getter_nonconst_read_const_rejected", EAccessorSet::Getter, EOperation::Read, ECallbackBehavior::Normal, true, false, false, false, TEXT("Non-const method call on read-only object reference"), 0 },
		{ "getter_write_missing", EAccessorSet::Getter, EOperation::Write, ECallbackBehavior::Normal, false, true, false, false, TEXT("The property has no set accessor"), 0 },
		{ "getter_compound_missing_set", EAccessorSet::Getter, EOperation::Compound, ECallbackBehavior::Normal, false, true, false, false, TEXT("require both get and set accessors"), 0 },
		{ "setter_write_mutable", EAccessorSet::Setter, EOperation::Write, ECallbackBehavior::Normal, false, false, true, false, TEXT(""), 73 },
		{ "setter_write_const_rejected", EAccessorSet::Setter, EOperation::Write, ECallbackBehavior::Normal, true, false, false, false, TEXT("Non-const method call on read-only object reference"), 0 },
		{ "setter_read_missing", EAccessorSet::Setter, EOperation::Read, ECallbackBehavior::Normal, false, false, false, false, TEXT("The property has no get accessor"), 0 },
		{ "setter_compound_missing_get", EAccessorSet::Setter, EOperation::Compound, ECallbackBehavior::Normal, false, false, false, false, TEXT("require both get and set accessors"), 0 },
		{ "both_read_mutable", EAccessorSet::Both, EOperation::Read, ECallbackBehavior::Normal, false, true, true, false, TEXT(""), 31 },
		{ "both_write_mutable", EAccessorSet::Both, EOperation::Write, ECallbackBehavior::Normal, false, true, true, false, TEXT(""), 73 },
		{ "both_compound_mutable", EAccessorSet::Both, EOperation::Compound, ECallbackBehavior::Normal, false, true, true, false, TEXT(""), 36 },
		{ "both_compound_const_rejected", EAccessorSet::Both, EOperation::Compound, ECallbackBehavior::Normal, true, true, false, false, TEXT("Non-const method call on read-only object reference"), 0 },
		{ "recursive_getter", EAccessorSet::Getter, EOperation::Read, ECallbackBehavior::RecursiveGetter, false, true, true, false, TEXT(""), 31 },
		{ "recursive_setter", EAccessorSet::Setter, EOperation::Write, ECallbackBehavior::RecursiveSetter, false, false, true, false, TEXT(""), 73 },
		{ "throwing_getter", EAccessorSet::Getter, EOperation::Read, ECallbackBehavior::ThrowingGetter, false, true, true, true, TEXT(""), 0 },
		{ "throwing_setter", EAccessorSet::Setter, EOperation::Write, ECallbackBehavior::ThrowingSetter, false, false, true, true, TEXT(""), 0 },
	};

	inline static constexpr FSourceShapeCase SourceShapeCases[] =
	{
		{ "single_line" },
		{ "multiline" },
		{ "parenthesized" },
		{ "helper_call" },
	};

	inline static FAccessorState* ActiveState = nullptr;

	static bool IsShape(const FSourceShapeCase& SourceShapeCase, const ANSICHAR* Name)
	{
		return FCStringAnsi::Strcmp(SourceShapeCase.CatalogName, Name) == 0;
	}

	static bool HasGetter(const FScenarioCase& ScenarioCase)
	{
		return ScenarioCase.AccessorSet == EAccessorSet::Getter
			|| ScenarioCase.AccessorSet == EAccessorSet::Both;
	}

	static bool HasSetter(const FScenarioCase& ScenarioCase)
	{
		return ScenarioCase.AccessorSet == EAccessorSet::Setter
			|| ScenarioCase.AccessorSet == EAccessorSet::Both;
	}

	static FString MakeSuffix(
		const FScenarioCase& ScenarioCase,
		const FSourceShapeCase& SourceShapeCase)
	{
		return FString::Printf(
			TEXT("%hs_%hs"),
			ScenarioCase.CatalogName,
			SourceShapeCase.CatalogName);
	}

	static void AddRefRegisteredProperty(FRegisteredPropertyObject* Object)
	{
		if (Object != nullptr)
		{
			Object->AddRef();
		}
	}

	static void ReleaseRegisteredProperty(FRegisteredPropertyObject* Object)
	{
		if (Object != nullptr)
		{
			Object->Release();
		}
	}

	static void CreateRegisteredProperty(asIScriptGeneric* Generic)
	{
		if (Generic == nullptr || ActiveState == nullptr)
		{
			return;
		}
		const int32 Value = static_cast<int32>(Generic->GetArgDWord(0));
		Generic->SetReturnAddress(new FRegisteredPropertyObject(*ActiveState, Value));
	}

	static void ObserveRegisteredProperty(asIScriptGeneric* Generic)
	{
		FRegisteredPropertyObject* const Object = Generic != nullptr
			? static_cast<FRegisteredPropertyObject*>(Generic->GetArgObject(0))
			: nullptr;
		Generic->SetReturnDWord(Object != nullptr ? static_cast<asDWORD>(Object->Value) : 0);
	}

	static asIScriptFunction* FindActiveModuleFunction(const char* Declaration)
	{
		asIScriptContext* const ActiveContext = asGetActiveContext();
		asIScriptFunction* const ActiveFunction = ActiveContext != nullptr
			? ActiveContext->GetFunction()
			: nullptr;
		asIScriptEngine* const ScriptEngine = ActiveContext != nullptr
			? ActiveContext->GetEngine()
			: nullptr;
		const char* const ModuleName = ActiveFunction != nullptr
			? ActiveFunction->GetModuleName()
			: nullptr;
		asIScriptModule* const Module = ScriptEngine != nullptr && ModuleName != nullptr
			? ScriptEngine->GetModule(ModuleName, asGM_ONLY_IF_EXISTS)
			: nullptr;
		return Module != nullptr ? Module->GetFunctionByDecl(Declaration) : nullptr;
	}

	static bool ExecuteRecursiveGetter(FRegisteredPropertyObject& Object, int32& OutValue)
	{
		asIScriptContext* const ActiveContext = asGetActiveContext();
		asIScriptEngine* const ScriptEngine = ActiveContext != nullptr
			? ActiveContext->GetEngine()
			: nullptr;
		asIScriptFunction* const Function = FindActiveModuleFunction(
			"int ReenterRegisteredPropertyGet(FRegisteredProperty& inout Receiver)");
		asIScriptContext* const NestedContext = ScriptEngine != nullptr
			? ScriptEngine->CreateContext()
			: nullptr;
		if (Function == nullptr || NestedContext == nullptr)
		{
			if (NestedContext != nullptr)
			{
				NestedContext->Release();
			}
			return false;
		}
		const int PrepareResult = NestedContext->Prepare(Function);
		const int ArgumentResult = PrepareResult >= 0
			? NestedContext->SetArgObject(0, &Object)
			: asCONTEXT_NOT_PREPARED;
		const int ExecuteResult = ArgumentResult >= 0
			? NestedContext->Execute()
			: asERROR;
		if (ExecuteResult == asEXECUTION_FINISHED)
		{
			OutValue = static_cast<int32>(NestedContext->GetReturnDWord());
		}
		NestedContext->Release();
		return ExecuteResult == asEXECUTION_FINISHED;
	}

	static bool ExecuteRecursiveSetter(FRegisteredPropertyObject& Object, const int32 Value)
	{
		asIScriptContext* const ActiveContext = asGetActiveContext();
		asIScriptEngine* const ScriptEngine = ActiveContext != nullptr
			? ActiveContext->GetEngine()
			: nullptr;
		asIScriptFunction* const Function = FindActiveModuleFunction(
			"void ReenterRegisteredPropertySet(FRegisteredProperty& inout Receiver, int Value)");
		asIScriptContext* const NestedContext = ScriptEngine != nullptr
			? ScriptEngine->CreateContext()
			: nullptr;
		if (Function == nullptr || NestedContext == nullptr)
		{
			if (NestedContext != nullptr)
			{
				NestedContext->Release();
			}
			return false;
		}
		const int PrepareResult = NestedContext->Prepare(Function);
		const int ObjectResult = PrepareResult >= 0
			? NestedContext->SetArgObject(0, &Object)
			: asCONTEXT_NOT_PREPARED;
		const int ValueResult = ObjectResult >= 0
			? NestedContext->SetArgDWord(1, static_cast<asDWORD>(Value))
			: asCONTEXT_NOT_PREPARED;
		const int ExecuteResult = ValueResult >= 0
			? NestedContext->Execute()
			: asERROR;
		NestedContext->Release();
		return ExecuteResult == asEXECUTION_FINISHED;
	}

	static void GetRegisteredProperty(asIScriptGeneric* Generic)
	{
		FRegisteredPropertyObject* const Object = Generic != nullptr
			? static_cast<FRegisteredPropertyObject*>(Generic->GetObject())
			: nullptr;
		if (Generic == nullptr || Object == nullptr || ActiveState == nullptr)
		{
			return;
		}

		ActiveState->Trace.Add(100 + ActiveState->RecursionDepth);
		if (ActiveState->Behavior == ECallbackBehavior::ThrowingGetter)
		{
			if (asIScriptContext* const Context = asGetActiveContext())
			{
				Context->SetException("Registered getter failure");
			}
			return;
		}
		if (ActiveState->Behavior == ECallbackBehavior::RecursiveGetter
			&& ActiveState->RecursionDepth == 0)
		{
			++ActiveState->RecursionDepth;
			int32 RecursiveValue = 0;
			const bool bSucceeded = ExecuteRecursiveGetter(*Object, RecursiveValue);
			--ActiveState->RecursionDepth;
			if (!bSucceeded)
			{
				if (asIScriptContext* const Context = asGetActiveContext())
				{
					Context->SetException("Registered getter recursion failed");
				}
				return;
			}
			Generic->SetReturnDWord(static_cast<asDWORD>(RecursiveValue));
			return;
		}
		Generic->SetReturnDWord(static_cast<asDWORD>(Object->Value));
	}

	static void SetRegisteredProperty(asIScriptGeneric* Generic)
	{
		FRegisteredPropertyObject* const Object = Generic != nullptr
			? static_cast<FRegisteredPropertyObject*>(Generic->GetObject())
			: nullptr;
		if (Generic == nullptr || Object == nullptr || ActiveState == nullptr)
		{
			return;
		}

		const int32 Value = static_cast<int32>(Generic->GetArgDWord(0));
		ActiveState->Trace.Add(200 + ActiveState->RecursionDepth);
		if (ActiveState->Behavior == ECallbackBehavior::ThrowingSetter)
		{
			if (asIScriptContext* const Context = asGetActiveContext())
			{
				Context->SetException("Registered setter failure");
			}
			return;
		}
		if (ActiveState->Behavior == ECallbackBehavior::RecursiveSetter
			&& ActiveState->RecursionDepth == 0)
		{
			++ActiveState->RecursionDepth;
			const bool bSucceeded = ExecuteRecursiveSetter(*Object, Value);
			--ActiveState->RecursionDepth;
			if (!bSucceeded)
			{
				if (asIScriptContext* const Context = asGetActiveContext())
				{
					Context->SetException("Registered setter recursion failed");
				}
			}
			return;
		}
		Object->Value = Value;
	}

	static FRegistrationResult RegisterAccessorFixture(
		asIScriptEngine& ScriptEngine,
		FAccessorState& State,
		const FScenarioCase& ScenarioCase)
	{
		ActiveState = &State;
		FRegistrationResult Result;
		if (ScriptEngine.RegisterObjectType("FRegisteredProperty", 0, asOBJ_REF) < 0
			|| ScriptEngine.RegisterObjectBehaviour(
				"FRegisteredProperty",
				asBEHAVE_ADDREF,
				"void f()",
				asFUNCTION(AddRefRegisteredProperty),
				asCALL_CDECL_OBJFIRST) < 0
			|| ScriptEngine.RegisterObjectBehaviour(
				"FRegisteredProperty",
				asBEHAVE_RELEASE,
				"void f()",
				asFUNCTION(ReleaseRegisteredProperty),
				asCALL_CDECL_OBJFIRST) < 0
			|| ScriptEngine.RegisterGlobalFunction(
				"FRegisteredProperty CreateRegisteredProperty(int Value)",
				asFUNCTION(CreateRegisteredProperty),
				asCALL_GENERIC) < 0
			|| ScriptEngine.RegisterGlobalFunction(
				"int ObserveRegisteredProperty(const FRegisteredProperty& in Receiver)",
				asFUNCTION(ObserveRegisteredProperty),
				asCALL_GENERIC) < 0)
		{
			return Result;
		}

		if (HasGetter(ScenarioCase))
		{
			const char* const Declaration = ScenarioCase.bConstGetter
				? "int get_Value() const property"
				: "int get_Value() property";
			Result.GetterFunctionId = ScriptEngine.RegisterObjectMethod(
				"FRegisteredProperty",
				Declaration,
				asFUNCTION(GetRegisteredProperty),
				asCALL_GENERIC);
			if (Result.GetterFunctionId < 0)
			{
				return Result;
			}
		}
		if (HasSetter(ScenarioCase))
		{
			Result.SetterFunctionId = ScriptEngine.RegisterObjectMethod(
				"FRegisteredProperty",
				"void set_Value(int Value) property",
				asFUNCTION(SetRegisteredProperty),
				asCALL_GENERIC);
			if (Result.SetterFunctionId < 0)
			{
				return Result;
			}
		}
		Result.bSucceeded = true;
		return Result;
	}

	static void AppendOperationBody(
		FString& Source,
		const FScenarioCase& ScenarioCase,
		const FSourceShapeCase& SourceShapeCase,
		const int32 IndentLevel)
	{
		using namespace AngelscriptNativeTestSupport;

		const FString Indent = FString::ChrN(IndentLevel, TEXT('\t'));
		const FString ContinuationIndent = Indent + TEXT("\t");
		if (ScenarioCase.Operation == EOperation::Read)
		{
			if (IsShape(SourceShapeCase, "multiline"))
			{
				AppendGeneratedAsLine(Source, Indent + TEXT("int Observed ="));
				AppendGeneratedAsLine(Source, ContinuationIndent + TEXT("Receiver.Value;"));
				AppendGeneratedAsLine(Source, Indent + TEXT("return Observed;"));
			}
			else if (IsShape(SourceShapeCase, "parenthesized"))
			{
				AppendGeneratedAsLine(Source, Indent + TEXT("return (((Receiver.Value)));"));
			}
			else
			{
				AppendGeneratedAsLine(Source, Indent + TEXT("return Receiver.Value;"));
			}
		}
		else if (ScenarioCase.Operation == EOperation::Write)
		{
			if (IsShape(SourceShapeCase, "multiline"))
			{
				AppendGeneratedAsLine(Source, Indent + TEXT("Receiver.Value ="));
				AppendGeneratedAsLine(Source, ContinuationIndent + TEXT("73;"));
			}
			else if (IsShape(SourceShapeCase, "parenthesized"))
			{
				AppendGeneratedAsLine(Source, Indent + TEXT("((Receiver.Value)) = 73;"));
			}
			else
			{
				AppendGeneratedAsLine(Source, Indent + TEXT("Receiver.Value = 73;"));
			}
			AppendGeneratedAsLine(Source, Indent + TEXT("return ObserveRegisteredProperty(Receiver);"));
		}
		else
		{
			if (IsShape(SourceShapeCase, "multiline"))
			{
				AppendGeneratedAsLine(Source, Indent + TEXT("Receiver.Value +="));
				AppendGeneratedAsLine(Source, ContinuationIndent + TEXT("5;"));
			}
			else if (IsShape(SourceShapeCase, "parenthesized"))
			{
				AppendGeneratedAsLine(Source, Indent + TEXT("((Receiver.Value)) += 5;"));
			}
			else
			{
				AppendGeneratedAsLine(Source, Indent + TEXT("Receiver.Value += 5;"));
			}
			AppendGeneratedAsLine(Source, Indent + TEXT("return ObserveRegisteredProperty(Receiver);"));
		}
	}

	static void AppendRecursiveHelpers(
		FString& Source,
		const FScenarioCase& ScenarioCase)
	{
		using namespace AngelscriptNativeTestSupport;

		if (ScenarioCase.Behavior == ECallbackBehavior::RecursiveGetter)
		{
			AppendGeneratedAsLine(Source, TEXT("int ReenterRegisteredPropertyGet(FRegisteredProperty& inout Receiver)"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\treturn Receiver.Value;"));
			AppendGeneratedAsLine(Source, TEXT("}"));
			AppendGeneratedAsLine(Source);
		}
		else if (ScenarioCase.Behavior == ECallbackBehavior::RecursiveSetter)
		{
			AppendGeneratedAsLine(Source, TEXT("void ReenterRegisteredPropertySet(FRegisteredProperty& inout Receiver, int Value)"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\tReceiver.Value = Value;"));
			AppendGeneratedAsLine(Source, TEXT("}"));
			AppendGeneratedAsLine(Source);
		}
	}

	static void AppendShapeHelper(
		FString& Source,
		const FScenarioCase& ScenarioCase,
		const FSourceShapeCase& SourceShapeCase)
	{
		using namespace AngelscriptNativeTestSupport;

		if (!IsShape(SourceShapeCase, "helper_call"))
		{
			return;
		}
		AppendGeneratedAsLine(Source, ScenarioCase.bConstReceiver
			? TEXT("int InvokeRegisteredProperty(const FRegisteredProperty& in Receiver)")
			: TEXT("int InvokeRegisteredProperty(FRegisteredProperty& inout Receiver)"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendOperationBody(Source, ScenarioCase, SourceShapeCase, 1);
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
	}

	static FString BuildRegisteredPropertySource(
		const FScenarioCase& ScenarioCase,
		const FSourceShapeCase& SourceShapeCase)
	{
		using namespace AngelscriptNativeTestSupport;

		FString Source;
		AppendRecursiveHelpers(Source, ScenarioCase);
		AppendShapeHelper(Source, ScenarioCase, SourceShapeCase);
		AppendGeneratedAsLine(Source, TEXT("int RunRegisteredProperty()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, ScenarioCase.bConstReceiver
			? TEXT("\tconst FRegisteredProperty Receiver = CreateRegisteredProperty(31);")
			: TEXT("\tFRegisteredProperty Receiver = CreateRegisteredProperty(31);"));
		if (IsShape(SourceShapeCase, "helper_call"))
		{
			AppendGeneratedAsLine(Source, TEXT("\treturn InvokeRegisteredProperty(Receiver);"));
		}
		else
		{
			AppendOperationBody(Source, ScenarioCase, SourceShapeCase, 1);
		}
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("int RunRegisteredPropertyRecovery()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\treturn 88;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		return Source;
	}

	static FString BuildRecoverySource()
	{
		using namespace AngelscriptNativeTestSupport;

		FString Source;
		AppendGeneratedAsLine(Source, TEXT("int RunRegisteredPropertyRecovery()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\treturn 88;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		return Source;
	}

	void VerifyRegistrationMetadata(
		const FNativeCaseContext& Case,
		const FScenarioCase& ScenarioCase,
		asIScriptEngine& ScriptEngine,
		const FRegistrationResult& Registration)
	{
		ASSERT_THAT(AreEqual(int64(3), ScriptEngine.GetEngineProperty(asEP_PROPERTY_ACCESSOR_MODE),
			*Case.Describe(TEXT("bare SDK engine should retain registered-property mode 3"))));
		asITypeInfo* const Type = ScriptEngine.GetTypeInfoByDecl("FRegisteredProperty");
		ASSERT_THAT(IsNotNull(Type,
			*Case.Describe(TEXT("registered accessor fixture should publish its reference type"))));
		if (HasGetter(ScenarioCase))
		{
			asIScriptFunction* const Getter = ScriptEngine.GetFunctionById(Registration.GetterFunctionId);
			ASSERT_THAT(IsNotNull(Getter,
				*Case.Describe(TEXT("registered getter should retain its function identity"))));
			if (Getter != nullptr)
			{
				ASSERT_THAT(IsTrue(Getter->IsProperty(),
					*Case.Describe(TEXT("registered getter should retain its property trait"))));
				ASSERT_THAT(AreEqual(ScenarioCase.bConstGetter, Getter->IsReadOnly(),
					*Case.Describe(TEXT("registered getter should retain its const trait"))));
				ASSERT_THAT(AreEqual(0, static_cast<int32>(Getter->GetParamCount()),
					*Case.Describe(TEXT("non-indexed getter should expose zero parameters"))));
				ASSERT_THAT(AreEqual(asTYPEID_INT32, Getter->GetReturnTypeId(),
					*Case.Describe(TEXT("registered getter should return int"))));
			}
		}
		if (HasSetter(ScenarioCase))
		{
			asIScriptFunction* const Setter = ScriptEngine.GetFunctionById(Registration.SetterFunctionId);
			ASSERT_THAT(IsNotNull(Setter,
				*Case.Describe(TEXT("registered setter should retain its function identity"))));
			if (Setter != nullptr)
			{
				ASSERT_THAT(IsTrue(Setter->IsProperty(),
					*Case.Describe(TEXT("registered setter should retain its property trait"))));
				ASSERT_THAT(IsFalse(Setter->IsReadOnly(),
					*Case.Describe(TEXT("registered setter should remain mutable"))));
				ASSERT_THAT(AreEqual(1, static_cast<int32>(Setter->GetParamCount()),
					*Case.Describe(TEXT("non-indexed setter should expose one value parameter"))));
				ASSERT_THAT(AreEqual(asTYPEID_VOID, Setter->GetReturnTypeId(),
					*Case.Describe(TEXT("registered setter should return void"))));
			}
		}
	}

	static TArray<int32> ExpectedTrace(const FScenarioCase& ScenarioCase)
	{
		if (ScenarioCase.Behavior == ECallbackBehavior::RecursiveGetter)
		{
			return { 100, 101 };
		}
		if (ScenarioCase.Behavior == ECallbackBehavior::RecursiveSetter)
		{
			return { 200, 201 };
		}
		if (ScenarioCase.Operation == EOperation::Read)
		{
			return { 100 };
		}
		if (ScenarioCase.Operation == EOperation::Write)
		{
			return { 200 };
		}
		return { 100, 200 };
	}

	void VerifyTrace(
		const FNativeCaseContext& Case,
		const FScenarioCase& ScenarioCase,
		const FAccessorState& State)
	{
		const TArray<int32> Expected = ExpectedTrace(ScenarioCase);
		ASSERT_THAT(AreEqual(Expected.Num(), State.Trace.Num(),
			*Case.Describe(TEXT("registered accessor should emit the exact callback count"))));
		for (int32 Index = 0; Index < FMath::Min(Expected.Num(), State.Trace.Num()); ++Index)
		{
			ASSERT_THAT(AreEqual(Expected[Index], State.Trace[Index],
				*Case.Describe(TEXT("registered accessor callback order should match the scenario"))));
		}
		ASSERT_THAT(AreEqual(0, State.RecursionDepth,
			*Case.Describe(TEXT("registered accessor should restore recursive callback depth"))));
	}

	void ExecuteCompiledCell(
		const FNativeCaseContext& Case,
		const FScenarioCase& ScenarioCase,
		asIScriptEngine& ScriptEngine,
		asIScriptModule& Module,
		FAccessorState& State)
	{
		using namespace AngelscriptNativeTestSupport;

		asIScriptFunction* const Entry = Module.GetFunctionByDecl("int RunRegisteredProperty()");
		asIScriptFunction* const Recovery = Module.GetFunctionByDecl("int RunRegisteredPropertyRecovery()");
		ASSERT_THAT(IsNotNull(Entry,
			*Case.Describe(TEXT("compiled accessor cell should expose its exact entry"))));
		ASSERT_THAT(IsNotNull(Recovery,
			*Case.Describe(TEXT("compiled accessor cell should expose same-context recovery"))));
		if (Entry == nullptr || Recovery == nullptr)
		{
			return;
		}
		asIScriptContext* const Context = ScriptEngine.CreateContext();
		ASSERT_THAT(IsNotNull(Context,
			*Case.Describe(TEXT("compiled accessor cell should create a context"))));
		if (Context == nullptr)
		{
			return;
		}

		ASSERT_THAT(AreEqual(asSUCCESS, Context->Prepare(Entry),
			*Case.Describe(TEXT("registered accessor context should prepare its entry"))));
		const int ExecuteResult = Context->Execute();
		if (ScenarioCase.bShouldThrow)
		{
			ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_EXCEPTION), ExecuteResult,
				*Case.Describe(TEXT("throwing accessor callback should raise a script exception"))));
			const FString ExpectedException = ScenarioCase.Behavior == ECallbackBehavior::ThrowingGetter
				? TEXT("Registered getter failure")
				: TEXT("Registered setter failure");
			ASSERT_THAT(AreEqual(ExpectedException, FString(UTF8_TO_TCHAR(Context->GetExceptionString())),
				*Case.Describe(TEXT("throwing accessor should preserve its exact exception text"))));
			int Column = 0;
			const char* Section = nullptr;
			ASSERT_THAT(IsTrue(Context->GetExceptionLineNumber(&Column, &Section) > 0
				&& Column > 0
				&& Section != nullptr,
				*Case.Describe(TEXT("throwing accessor should expose a located script operation"))));
		}
		else
		{
			ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_FINISHED), ExecuteResult,
				*Case.Describe(TEXT("registered accessor scenario should finish"))));
			ASSERT_THAT(AreEqual(ScenarioCase.ExpectedValue, static_cast<int32>(Context->GetReturnDWord()),
				*Case.Describe(TEXT("registered accessor result should prove read, write, or compound behavior"))));
		}
		VerifyTrace(Case, ScenarioCase, State);

		ASSERT_THAT(AreEqual(asSUCCESS, Context->Unprepare(),
			*Case.Describe(TEXT("registered accessor context should release entry locals"))));
		ASSERT_THAT(AreEqual(asSUCCESS, Context->Prepare(Recovery),
			*Case.Describe(TEXT("registered accessor context should prepare recovery after success or exception"))));
		ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_FINISHED), Context->Execute(),
			*Case.Describe(TEXT("registered accessor context should execute recovery"))));
		ASSERT_THAT(AreEqual(88, static_cast<int32>(Context->GetReturnDWord()),
			*Case.Describe(TEXT("registered accessor recovery should contain no callback state"))));
		ASSERT_THAT(AreEqual(asSUCCESS, Context->Unprepare(),
			*Case.Describe(TEXT("registered accessor recovery should unprepare cleanly"))));
		Context->Release();
		ASSERT_THAT(AreEqual(0, State.LiveObjects,
			*Case.Describe(TEXT("registered accessor context cleanup should release every receiver"))));
		ASSERT_THAT(AreEqual(State.CreatedObjects, State.DestroyedObjects,
			*Case.Describe(TEXT("registered accessor receiver construction and destruction should balance"))));
	}

	void CompileRecovery(
		const FNativeCaseContext& Case,
		AngelscriptNativeTestSupport::FNativeTestEngine& Engine,
		asIScriptEngine& ScriptEngine,
		const FString& ModuleName)
	{
		using namespace AngelscriptNativeTestSupport;

		const FString RecoverySource = BuildRecoverySource();
		PrintGeneratedAsSource(*TestRunner, Case.GetId() + TEXT("-RECOVERY"), ModuleName, RecoverySource);
		const FTCHARToUTF8 ModuleNameUtf8(*ModuleName);
		const FTCHARToUTF8 SourceUtf8(*RecoverySource);
		Engine.ResetMessages();
		asIScriptModule* RecoveryModule = nullptr;
		ASSERT_THAT(IsTrue(CompileNativeModule(
			&ScriptEngine,
			ModuleNameUtf8.Get(),
			SourceUtf8.Get(),
			RecoveryModule) >= 0,
			*Case.Describe(TEXT("rejected accessor source should permit a same-name recovery build"))));
		ASSERT_THAT(IsNotNull(RecoveryModule,
			*Case.Describe(TEXT("registered accessor recovery should publish a clean module"))));
		if (RecoveryModule != nullptr)
		{
			asIScriptFunction* const Recovery = RecoveryModule->GetFunctionByDecl("int RunRegisteredPropertyRecovery()");
			ASSERT_THAT(IsNotNull(Recovery,
				*Case.Describe(TEXT("registered accessor recovery should expose its exact entry"))));
			if (Recovery != nullptr)
			{
				asIScriptContext* const Context = ScriptEngine.CreateContext();
				ASSERT_THAT(IsNotNull(Context,
					*Case.Describe(TEXT("registered accessor recovery should create a context"))));
				if (Context != nullptr)
				{
					ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_FINISHED), PrepareAndExecute(Context, Recovery),
						*Case.Describe(TEXT("registered accessor recovery should execute"))));
					ASSERT_THAT(AreEqual(88, static_cast<int32>(Context->GetReturnDWord()),
						*Case.Describe(TEXT("registered accessor recovery should return its sentinel"))));
					Context->Release();
				}
			}
		}
		ScriptEngine.DiscardModule(ModuleNameUtf8.Get());
	}

public:
	TEST_METHOD(ScenariosBySourceShape)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT("LANG-PROP-ACCESSOR",
			ENativeEvidence::Compile
				| ENativeEvidence::Diagnostic
				| ENativeEvidence::Runtime
				| ENativeEvidence::Metadata
				| ENativeEvidence::Lifecycle
				| ENativeEvidence::Debug
				| ENativeEvidence::Cleanup);

		for (const FScenarioCase& ScenarioCase : ScenarioCases)
		{
			for (const FSourceShapeCase& SourceShapeCase : SourceShapeCases)
			{
				const FNativeCaseContext Case(MakeNativeCaseId(
					"LANG-PROP-ACCESSOR",
					{
						ANSI_TO_TCHAR(ScenarioCase.CatalogName),
						ANSI_TO_TCHAR(SourceShapeCase.CatalogName),
					}));
				FAccessorState State;
				State.Behavior = ScenarioCase.Behavior;
				ActiveState = &State;
				FNativeTestEngine Engine;
				Engine.Create(*TestRunner);
				asIScriptEngine* const ScriptEngine = Engine.Get();
				ASSERT_THAT(IsNotNull(ScriptEngine,
					*Case.Describe(TEXT("registered accessor cell should create an isolated raw SDK engine"))));
				if (ScriptEngine == nullptr)
				{
					Engine.Destroy();
					ActiveState = nullptr;
					continue;
				}

				const FRegistrationResult Registration = RegisterAccessorFixture(
					*ScriptEngine,
					State,
					ScenarioCase);
				ASSERT_THAT(IsTrue(Registration.bSucceeded,
					*Case.Describe(TEXT("registered accessor fixture should publish every requested native declaration"))));
				if (!Registration.bSucceeded)
				{
					Engine.Destroy();
					ActiveState = nullptr;
					continue;
				}
				VerifyRegistrationMetadata(Case, ScenarioCase, *ScriptEngine, Registration);

				const FString Suffix = MakeSuffix(ScenarioCase, SourceShapeCase);
				const FString ModuleName = TEXT("RegisteredProperty_") + Suffix;
				const FString Source = BuildRegisteredPropertySource(ScenarioCase, SourceShapeCase);
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
				if (ScenarioCase.bShouldCompile)
				{
					ASSERT_THAT(IsTrue(BuildResult >= 0,
						*Case.Describe(TEXT("registered accessor scenario should compile"))));
					ASSERT_THAT(IsNotNull(Module,
						*Case.Describe(TEXT("registered accessor scenario should publish a module"))));
					if (BuildResult >= 0 && Module != nullptr)
					{
						ExecuteCompiledCell(Case, ScenarioCase, *ScriptEngine, *Module, State);
					}
				}
				else
				{
					ASSERT_THAT(IsTrue(BuildResult < 0,
						*Case.Describe(TEXT("invalid registered accessor use should be rejected"))));
					ASSERT_THAT(IsTrue(Engine.GetMessages().Entries.ContainsByPredicate([&ScenarioCase](const FNativeMessageEntry& Entry)
					{
						return Entry.Type == asMSGTYPE_ERROR
							&& Entry.Row > 0
							&& Entry.Column > 0
							&& !Entry.Section.IsEmpty()
							&& Entry.Message.Contains(ScenarioCase.CompileDiagnostic);
					}),
						*Case.Describe(TEXT("rejected accessor use should report its exact located diagnostic"))));
					ASSERT_THAT(AreEqual(0, State.Trace.Num(),
						*Case.Describe(TEXT("compile-time accessor rejection should invoke no native callback"))));
					ASSERT_THAT(AreEqual(0, State.LiveObjects,
						*Case.Describe(TEXT("compile-time accessor rejection should construct no receiver"))));
				}

				ScriptEngine->DiscardModule(ModuleNameUtf8.Get());
				ASSERT_THAT(IsNull(ScriptEngine->GetModule(ModuleNameUtf8.Get(), asGM_ONLY_IF_EXISTS),
					*Case.Describe(TEXT("registered accessor cell should discard its isolated module"))));
				if (!ScenarioCase.bShouldCompile)
				{
					CompileRecovery(Case, Engine, *ScriptEngine, ModuleName);
					ASSERT_THAT(IsNull(ScriptEngine->GetModule(ModuleNameUtf8.Get(), asGM_ONLY_IF_EXISTS),
						*Case.Describe(TEXT("registered accessor recovery should leave no module behind"))));
				}
				Engine.Destroy();
				ASSERT_THAT(AreEqual(0, State.LiveObjects,
					*Case.Describe(TEXT("destroying the isolated engine should leave no registered accessor object"))));
				ASSERT_THAT(AreEqual(State.CreatedObjects, State.DestroyedObjects,
					*Case.Describe(TEXT("isolated accessor engine should balance all created receiver objects"))));
				ActiveState = nullptr;
			}
		}
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
