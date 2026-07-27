#include "../../Support/AngelscriptNativeCaseTestSupport.h"
#include "../../Support/AngelscriptNativeCoreTestSupport.h"
#include "../../Support/AngelscriptNativeFixtureTestSupport.h"
#include "../../Support/AngelscriptNativeLanguageCaseTestSupport.h"

#include "CQTest.h"
#include "Misc/ScopeExit.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS_AND_TAGS(FPropertyFailureTests,
	"Angelscript.TestModule.AngelScriptSDK.Language.Properties.Failure",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter | EAutomationTestFlags::Disabled,
	TEXT("#as-v238-backport"))
{
private:
	using FNativeCaseContext = AngelscriptNativeTestSupport::FNativeCaseContext;
	using FNativeMessageCollector = AngelscriptNativeTestSupport::FNativeMessageCollector;
	using FNativeTrackedValue = AngelscriptNativeTestSupport::FNativeTrackedValue;

	enum class EFailureKind : uint8
	{
		RemovedPropertyDecorator,
		RemovedVirtualProperty,
		MissingGetter,
		MissingSetter,
		RegisteredMismatchedTypes,
		RegisteredDuplicateGetter,
		RegisteredDuplicateSetter,
		RecursiveGetter,
		RecursiveSetter,
		ThrowingGetter,
		ThrowingSetter,
		NullReceiver,
		InaccessibleField,
		CompoundValueReceiver,
	};

	enum class EFailurePhase : uint8
	{
		Registration,
		Compile,
		Runtime,
	};

	struct FFailureCase
	{
		const ANSICHAR* CatalogName;
		EFailureKind Kind;
		EFailurePhase Phase;
		const TCHAR* ExpectedText;
	};

	struct FProbeCase
	{
		const ANSICHAR* CatalogName;
		bool bAlternate;
	};

	struct FRecoveryCase
	{
		const ANSICHAR* CatalogName;
		bool bSameState;
	};

	struct FPropertyFailureState
	{
		EFailureKind Kind = EFailureKind::MissingGetter;
		TArray<int32> Trace;
		int32 RecursionDepth = 0;
		int32 LiveObjects = 0;
		int32 CreatedObjects = 0;
		int32 DestroyedObjects = 0;

		void Reset(const EFailureKind InKind)
		{
			Kind = InKind;
			Trace.Reset();
			RecursionDepth = 0;
			LiveObjects = 0;
			CreatedObjects = 0;
			DestroyedObjects = 0;
		}
	};

	class FPropertyFailureObject
	{
	public:
		FPropertyFailureObject(FPropertyFailureState& InState, const int32 InValue)
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

		FPropertyFailureState& State;
		int32 Value = 0;

	private:
		~FPropertyFailureObject()
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
		int32 DuplicateResult = asSUCCESS;
		int32 ExpectedSelectedMarker = INDEX_NONE;
		bool bSucceeded = false;
	};

	inline static constexpr FFailureCase FailureCases[] =
	{
		{ "removed_property_decorator", EFailureKind::RemovedPropertyDecorator, EFailurePhase::Compile,
			TEXT("The 'property' decorator has been removed") },
		{ "removed_virtual_property", EFailureKind::RemovedVirtualProperty, EFailurePhase::Compile,
			TEXT("Virtual property syntax has been removed") },
		{ "missing_getter", EFailureKind::MissingGetter, EFailurePhase::Compile,
			TEXT("The property has no get accessor") },
		{ "missing_setter", EFailureKind::MissingSetter, EFailurePhase::Compile,
			TEXT("The property has no set accessor") },
		{ "registered_mismatched_types", EFailureKind::RegisteredMismatchedTypes, EFailurePhase::Compile,
			TEXT("has mismatching types for the get and set accessors") },
		{ "registered_duplicate_getter", EFailureKind::RegisteredDuplicateGetter, EFailurePhase::Registration,
			TEXT("asALREADY_REGISTERED") },
		{ "registered_duplicate_setter", EFailureKind::RegisteredDuplicateSetter, EFailurePhase::Registration,
			TEXT("asALREADY_REGISTERED") },
		{ "recursive_getter", EFailureKind::RecursiveGetter, EFailurePhase::Runtime,
			TEXT("Recursive registered getter stopped") },
		{ "recursive_setter", EFailureKind::RecursiveSetter, EFailurePhase::Runtime,
			TEXT("Recursive registered setter stopped") },
		{ "throwing_getter", EFailureKind::ThrowingGetter, EFailurePhase::Runtime,
			TEXT("Registered getter failure") },
		{ "throwing_setter", EFailureKind::ThrowingSetter, EFailurePhase::Runtime,
			TEXT("Registered setter failure") },
		{ "null_receiver", EFailureKind::NullReceiver, EFailurePhase::Runtime,
			TEXT("Null pointer access") },
		{ "inaccessible_field", EFailureKind::InaccessibleField, EFailurePhase::Compile,
			TEXT("Illegal access to private property 'Hidden'") },
		{ "compound_value_receiver", EFailureKind::CompoundValueReceiver, EFailurePhase::Compile,
			TEXT("Compound assignments with property accessors on value types are not supported") },
	};

	inline static constexpr FProbeCase ProbeCases[] =
	{
		{ "direct", false },
		{ "alternate_path", true },
	};

	inline static constexpr FRecoveryCase RecoveryCases[] =
	{
		{ "fresh_module", false },
		{ "same_module_or_context", true },
	};

	inline static FPropertyFailureState* ActiveState = nullptr;

	static bool IsKind(const FFailureCase& FailureCase, const EFailureKind Kind)
	{
		return FailureCase.Kind == Kind;
	}

	static bool IsGetterFailure(const FFailureCase& FailureCase)
	{
		return IsKind(FailureCase, EFailureKind::MissingSetter)
			|| IsKind(FailureCase, EFailureKind::RegisteredDuplicateGetter)
			|| IsKind(FailureCase, EFailureKind::RecursiveGetter)
			|| IsKind(FailureCase, EFailureKind::ThrowingGetter)
			|| IsKind(FailureCase, EFailureKind::NullReceiver);
	}

	static bool IsSetterFailure(const FFailureCase& FailureCase)
	{
		return IsKind(FailureCase, EFailureKind::MissingGetter)
			|| IsKind(FailureCase, EFailureKind::RegisteredDuplicateSetter)
			|| IsKind(FailureCase, EFailureKind::RecursiveSetter)
			|| IsKind(FailureCase, EFailureKind::ThrowingSetter);
	}

	static FString MakeSuffix(
		const FFailureCase& FailureCase,
		const FProbeCase& ProbeCase,
		const FRecoveryCase& RecoveryCase)
	{
		return FString::Printf(
			TEXT("%hs_%hs_%hs"),
			FailureCase.CatalogName,
			ProbeCase.CatalogName,
			RecoveryCase.CatalogName);
	}

	static void AddRefPropertyFailure(FPropertyFailureObject* Object)
	{
		if (Object != nullptr)
		{
			Object->AddRef();
		}
	}

	static void ReleasePropertyFailure(FPropertyFailureObject* Object)
	{
		if (Object != nullptr)
		{
			Object->Release();
		}
	}

	static void CreatePropertyFailure(asIScriptGeneric* Generic)
	{
		if (Generic == nullptr || ActiveState == nullptr)
		{
			return;
		}
		Generic->SetReturnAddress(new FPropertyFailureObject(
			*ActiveState,
			static_cast<int32>(Generic->GetArgDWord(0))));
	}

	static void ObservePropertyFailure(asIScriptGeneric* Generic)
	{
		FPropertyFailureObject* const Object = Generic != nullptr
			? static_cast<FPropertyFailureObject*>(Generic->GetArgObject(0))
			: nullptr;
		if (Generic != nullptr)
		{
			Generic->SetReturnDWord(Object != nullptr ? static_cast<asDWORD>(Object->Value) : 0);
		}
	}

	static asIScriptFunction* FindActiveModuleFunction(const char* Declaration)
	{
		asIScriptContext* const Context = asGetActiveContext();
		asIScriptFunction* const ActiveFunction = Context != nullptr ? Context->GetFunction() : nullptr;
		asIScriptEngine* const ScriptEngine = Context != nullptr ? Context->GetEngine() : nullptr;
		const char* const ModuleName = ActiveFunction != nullptr ? ActiveFunction->GetModuleName() : nullptr;
		asIScriptModule* const Module = ScriptEngine != nullptr && ModuleName != nullptr
			? ScriptEngine->GetModule(ModuleName, asGM_ONLY_IF_EXISTS)
			: nullptr;
		return Module != nullptr ? Module->GetFunctionByDecl(Declaration) : nullptr;
	}

	static bool ExecuteRecursiveGetter(FPropertyFailureObject& Object)
	{
		asIScriptContext* const ActiveContext = asGetActiveContext();
		asIScriptEngine* const ScriptEngine = ActiveContext != nullptr
			? ActiveContext->GetEngine()
			: nullptr;
		asIScriptFunction* const Function = FindActiveModuleFunction(
			"int ReenterPropertyFailureGet(FPropertyFailure& inout Receiver)");
		asIScriptContext* const Context = ScriptEngine != nullptr ? ScriptEngine->CreateContext() : nullptr;
		if (Function == nullptr || Context == nullptr)
		{
			if (Context != nullptr)
			{
				Context->Release();
			}
			return false;
		}
		const int PrepareResult = Context->Prepare(Function);
		const int ArgumentResult = PrepareResult >= 0
			? Context->SetArgObject(0, &Object)
			: asCONTEXT_NOT_PREPARED;
		const int ExecuteResult = ArgumentResult >= 0 ? Context->Execute() : asERROR;
		Context->Release();
		return ExecuteResult == asEXECUTION_FINISHED;
	}

	static bool ExecuteRecursiveSetter(FPropertyFailureObject& Object, const int32 Value)
	{
		asIScriptContext* const ActiveContext = asGetActiveContext();
		asIScriptEngine* const ScriptEngine = ActiveContext != nullptr
			? ActiveContext->GetEngine()
			: nullptr;
		asIScriptFunction* const Function = FindActiveModuleFunction(
			"void ReenterPropertyFailureSet(FPropertyFailure& inout Receiver, int Value)");
		asIScriptContext* const Context = ScriptEngine != nullptr ? ScriptEngine->CreateContext() : nullptr;
		if (Function == nullptr || Context == nullptr)
		{
			if (Context != nullptr)
			{
				Context->Release();
			}
			return false;
		}
		const int PrepareResult = Context->Prepare(Function);
		const int ObjectResult = PrepareResult >= 0
			? Context->SetArgObject(0, &Object)
			: asCONTEXT_NOT_PREPARED;
		const int ValueResult = ObjectResult >= 0
			? Context->SetArgDWord(1, static_cast<asDWORD>(Value))
			: asCONTEXT_NOT_PREPARED;
		const int ExecuteResult = ValueResult >= 0 ? Context->Execute() : asERROR;
		Context->Release();
		return ExecuteResult == asEXECUTION_FINISHED;
	}

	static void GetPropertyFailure(asIScriptGeneric* Generic)
	{
		FPropertyFailureObject* const Object = Generic != nullptr
			? static_cast<FPropertyFailureObject*>(Generic->GetObject())
			: nullptr;
		if (Generic == nullptr || Object == nullptr || ActiveState == nullptr)
		{
			return;
		}

		ActiveState->Trace.Add(100 + ActiveState->RecursionDepth);
		if (ActiveState->Kind == EFailureKind::ThrowingGetter)
		{
			if (asIScriptContext* const Context = asGetActiveContext())
			{
				Context->SetException("Registered getter failure");
			}
			return;
		}
		if (ActiveState->Kind == EFailureKind::RecursiveGetter)
		{
			if (ActiveState->RecursionDepth > 0)
			{
				if (asIScriptContext* const Context = asGetActiveContext())
				{
					Context->SetException("Recursive registered getter stopped");
				}
				return;
			}
			++ActiveState->RecursionDepth;
			const bool bNestedFinished = ExecuteRecursiveGetter(*Object);
			--ActiveState->RecursionDepth;
			if (!bNestedFinished)
			{
				if (asIScriptContext* const Context = asGetActiveContext())
				{
					Context->SetException("Recursive registered getter stopped");
				}
				return;
			}
		}
		Generic->SetReturnDWord(static_cast<asDWORD>(Object->Value));
	}

	static void SetPropertyFailure(asIScriptGeneric* Generic)
	{
		FPropertyFailureObject* const Object = Generic != nullptr
			? static_cast<FPropertyFailureObject*>(Generic->GetObject())
			: nullptr;
		if (Generic == nullptr || Object == nullptr || ActiveState == nullptr)
		{
			return;
		}

		const int32 Value = static_cast<int32>(Generic->GetArgDWord(0));
		ActiveState->Trace.Add(200 + ActiveState->RecursionDepth);
		if (ActiveState->Kind == EFailureKind::ThrowingSetter)
		{
			if (asIScriptContext* const Context = asGetActiveContext())
			{
				Context->SetException("Registered setter failure");
			}
			return;
		}
		if (ActiveState->Kind == EFailureKind::RecursiveSetter)
		{
			if (ActiveState->RecursionDepth > 0)
			{
				if (asIScriptContext* const Context = asGetActiveContext())
				{
					Context->SetException("Recursive registered setter stopped");
				}
				return;
			}
			++ActiveState->RecursionDepth;
			const bool bNestedFinished = ExecuteRecursiveSetter(*Object, Value);
			--ActiveState->RecursionDepth;
			if (!bNestedFinished)
			{
				if (asIScriptContext* const Context = asGetActiveContext())
				{
					Context->SetException("Recursive registered setter stopped");
				}
				return;
			}
		}
		Object->Value = Value;
	}

	static void GetPropertyFailurePrimary(asIScriptGeneric* Generic)
	{
		FPropertyFailureObject* const Object = Generic != nullptr
			? static_cast<FPropertyFailureObject*>(Generic->GetObject())
			: nullptr;
		if (Generic != nullptr && Object != nullptr && ActiveState != nullptr)
		{
			ActiveState->Trace.Add(110);
			Generic->SetReturnDWord(static_cast<asDWORD>(Object->Value));
		}
	}

	static void GetPropertyFailureAlternate(asIScriptGeneric* Generic)
	{
		FPropertyFailureObject* const Object = Generic != nullptr
			? static_cast<FPropertyFailureObject*>(Generic->GetObject())
			: nullptr;
		if (Generic != nullptr && Object != nullptr && ActiveState != nullptr)
		{
			ActiveState->Trace.Add(111);
			Generic->SetReturnDWord(static_cast<asDWORD>(Object->Value));
		}
	}

	static void SetPropertyFailurePrimary(asIScriptGeneric* Generic)
	{
		FPropertyFailureObject* const Object = Generic != nullptr
			? static_cast<FPropertyFailureObject*>(Generic->GetObject())
			: nullptr;
		if (Generic != nullptr && Object != nullptr && ActiveState != nullptr)
		{
			ActiveState->Trace.Add(210);
			Object->Value = static_cast<int32>(Generic->GetArgDWord(0));
		}
	}

	static void SetPropertyFailureAlternate(asIScriptGeneric* Generic)
	{
		FPropertyFailureObject* const Object = Generic != nullptr
			? static_cast<FPropertyFailureObject*>(Generic->GetObject())
			: nullptr;
		if (Generic != nullptr && Object != nullptr && ActiveState != nullptr)
		{
			ActiveState->Trace.Add(211);
			Object->Value = static_cast<int32>(Generic->GetArgDWord(0));
		}
	}

	static void GetPropertyFailureFloat(asIScriptGeneric* Generic)
	{
		FPropertyFailureObject* const Object = Generic != nullptr
			? static_cast<FPropertyFailureObject*>(Generic->GetObject())
			: nullptr;
		if (Generic != nullptr)
		{
			Generic->SetReturnFloat(Object != nullptr ? static_cast<float>(Object->Value) : 0.0f);
		}
	}

	static void SetPropertyFailureFloat(asIScriptGeneric* Generic)
	{
		FPropertyFailureObject* const Object = Generic != nullptr
			? static_cast<FPropertyFailureObject*>(Generic->GetObject())
			: nullptr;
		if (Generic != nullptr && Object != nullptr)
		{
			Object->Value = FMath::RoundToInt(Generic->GetArgFloat(0));
		}
	}

	static void GetValueReceiverProperty(asIScriptGeneric* Generic)
	{
		FNativeTrackedValue* const Object = Generic != nullptr
			? static_cast<FNativeTrackedValue*>(Generic->GetObject())
			: nullptr;
		if (Generic != nullptr)
		{
			Generic->SetReturnDWord(Object != nullptr ? static_cast<asDWORD>(Object->Value) : 0);
		}
	}

	static void SetValueReceiverProperty(asIScriptGeneric* Generic)
	{
		FNativeTrackedValue* const Object = Generic != nullptr
			? static_cast<FNativeTrackedValue*>(Generic->GetObject())
			: nullptr;
		if (Generic != nullptr && Object != nullptr)
		{
			Object->Value = static_cast<int32>(Generic->GetArgDWord(0));
		}
	}

	static bool RegisterBaseFixture(asIScriptEngine& ScriptEngine)
	{
		return ScriptEngine.RegisterObjectType("FPropertyFailure", 0, asOBJ_REF) >= 0
			&& ScriptEngine.RegisterObjectBehaviour(
				"FPropertyFailure",
				asBEHAVE_ADDREF,
				"void f()",
				asFUNCTION(AddRefPropertyFailure),
				asCALL_CDECL_OBJFIRST) >= 0
			&& ScriptEngine.RegisterObjectBehaviour(
				"FPropertyFailure",
				asBEHAVE_RELEASE,
				"void f()",
				asFUNCTION(ReleasePropertyFailure),
				asCALL_CDECL_OBJFIRST) >= 0
			&& ScriptEngine.RegisterGlobalFunction(
				"FPropertyFailure CreatePropertyFailure(int Value)",
				asFUNCTION(CreatePropertyFailure),
				asCALL_GENERIC) >= 0
			&& ScriptEngine.RegisterGlobalFunction(
				"int ObservePropertyFailure(const FPropertyFailure& in Receiver)",
				asFUNCTION(ObservePropertyFailure),
				asCALL_GENERIC) >= 0;
	}

	static int32 RegisterGetter(
		asIScriptEngine& ScriptEngine,
		const char* Declaration,
		const asSFuncPtr& Function)
	{
		return ScriptEngine.RegisterObjectMethod(
			"FPropertyFailure",
			Declaration,
			Function,
			asCALL_GENERIC);
	}

	static int32 RegisterSetter(
		asIScriptEngine& ScriptEngine,
		const char* Declaration,
		const asSFuncPtr& Function)
	{
		return ScriptEngine.RegisterObjectMethod(
			"FPropertyFailure",
			Declaration,
			Function,
			asCALL_GENERIC);
	}

	static FRegistrationResult RegisterFailureFixture(
		asIScriptEngine& ScriptEngine,
		const FFailureCase& FailureCase,
		const FProbeCase& ProbeCase)
	{
		FRegistrationResult Result;
		if (!RegisterBaseFixture(ScriptEngine))
		{
			return Result;
		}

		if (IsKind(FailureCase, EFailureKind::CompoundValueReceiver))
		{
			Result.GetterFunctionId = ScriptEngine.RegisterObjectMethod(
				"FNativeCaseValue",
				"int get_Access() const property",
				asFUNCTION(GetValueReceiverProperty),
				asCALL_GENERIC);
			Result.SetterFunctionId = ScriptEngine.RegisterObjectMethod(
				"FNativeCaseValue",
				"void set_Access(int Value) property",
				asFUNCTION(SetValueReceiverProperty),
				asCALL_GENERIC);
			Result.bSucceeded = Result.GetterFunctionId >= 0 && Result.SetterFunctionId >= 0;
			return Result;
		}

		if (IsKind(FailureCase, EFailureKind::RegisteredDuplicateGetter))
		{
			const asSFuncPtr First = ProbeCase.bAlternate
				? asFUNCTION(GetPropertyFailureAlternate)
				: asFUNCTION(GetPropertyFailurePrimary);
			const asSFuncPtr Second = ProbeCase.bAlternate
				? asFUNCTION(GetPropertyFailurePrimary)
				: asFUNCTION(GetPropertyFailureAlternate);
			Result.GetterFunctionId = RegisterGetter(
				ScriptEngine,
				"int get_Value() const property",
				First);
			Result.DuplicateResult = RegisterGetter(
				ScriptEngine,
				"int get_Value() const property",
				Second);
			Result.ExpectedSelectedMarker = ProbeCase.bAlternate ? 111 : 110;
			Result.bSucceeded = Result.GetterFunctionId >= 0
				&& Result.DuplicateResult == asALREADY_REGISTERED;
			return Result;
		}

		if (IsKind(FailureCase, EFailureKind::RegisteredDuplicateSetter))
		{
			const asSFuncPtr First = ProbeCase.bAlternate
				? asFUNCTION(SetPropertyFailureAlternate)
				: asFUNCTION(SetPropertyFailurePrimary);
			const asSFuncPtr Second = ProbeCase.bAlternate
				? asFUNCTION(SetPropertyFailurePrimary)
				: asFUNCTION(SetPropertyFailureAlternate);
			Result.SetterFunctionId = RegisterSetter(
				ScriptEngine,
				"void set_Value(int Value) property",
				First);
			Result.DuplicateResult = RegisterSetter(
				ScriptEngine,
				"void set_Value(int Value) property",
				Second);
			Result.ExpectedSelectedMarker = ProbeCase.bAlternate ? 211 : 210;
			Result.bSucceeded = Result.SetterFunctionId >= 0
				&& Result.DuplicateResult == asALREADY_REGISTERED;
			return Result;
		}

		if (IsKind(FailureCase, EFailureKind::RegisteredMismatchedTypes))
		{
			if (ProbeCase.bAlternate)
			{
				Result.SetterFunctionId = RegisterSetter(
					ScriptEngine,
					"void set_Value(int Value) property",
					asFUNCTION(SetPropertyFailure));
				Result.GetterFunctionId = RegisterGetter(
					ScriptEngine,
					"float get_Value() const property",
					asFUNCTION(GetPropertyFailureFloat));
			}
			else
			{
				Result.GetterFunctionId = RegisterGetter(
					ScriptEngine,
					"int get_Value() const property",
					asFUNCTION(GetPropertyFailure));
				Result.SetterFunctionId = RegisterSetter(
					ScriptEngine,
					"void set_Value(float Value) property",
					asFUNCTION(SetPropertyFailureFloat));
			}
			Result.bSucceeded = Result.GetterFunctionId >= 0 && Result.SetterFunctionId >= 0;
			return Result;
		}

		if (!IsSetterFailure(FailureCase))
		{
			Result.GetterFunctionId = RegisterGetter(
				ScriptEngine,
				"int get_Value() const property",
				asFUNCTION(GetPropertyFailure));
		}
		if (!IsGetterFailure(FailureCase))
		{
			Result.SetterFunctionId = RegisterSetter(
				ScriptEngine,
				"void set_Value(int Value) property",
				asFUNCTION(SetPropertyFailure));
		}
		Result.bSucceeded = (IsSetterFailure(FailureCase) || Result.GetterFunctionId >= 0)
			&& (IsGetterFailure(FailureCase) || Result.SetterFunctionId >= 0);
		return Result;
	}

	static void AppendRemovedSyntax(
		FString& Source,
		const FFailureCase& FailureCase,
		const FProbeCase& ProbeCase)
	{
		using namespace AngelscriptNativeTestSupport;

		AppendGeneratedAsLine(Source, TEXT("class FRemovedPropertyOwner"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		if (ProbeCase.bAlternate)
		{
			AppendGeneratedAsLine(Source, TEXT("\tint Stable = 3;"));
			AppendGeneratedAsLine(Source);
		}
		if (IsKind(FailureCase, EFailureKind::RemovedPropertyDecorator))
		{
			AppendGeneratedAsLine(Source, TEXT("\tint GetValue() property"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, ProbeCase.bAlternate
				? TEXT("\t\treturn Stable;")
				: TEXT("\t\treturn 3;"));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
		}
		else
		{
			AppendGeneratedAsLine(Source, TEXT("\tint Value"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t\tget"));
			AppendGeneratedAsLine(Source, TEXT("\t\t{"));
			AppendGeneratedAsLine(Source, ProbeCase.bAlternate
				? TEXT("\t\t\treturn Stable;")
				: TEXT("\t\t\treturn 3;"));
			AppendGeneratedAsLine(Source, TEXT("\t\t}"));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
		}
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
	}

	static void AppendInaccessibleSource(FString& Source, const FProbeCase& ProbeCase)
	{
		using namespace AngelscriptNativeTestSupport;

		AppendGeneratedAsLine(Source, TEXT("class FPrivatePropertyOwner"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tprivate int Hidden = 7;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		if (ProbeCase.bAlternate)
		{
			AppendGeneratedAsLine(Source, TEXT("class FPrivatePropertyProbe"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\tint Invoke(FPrivatePropertyOwner Receiver)"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t\treturn Receiver.Hidden;"));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
			AppendGeneratedAsLine(Source, TEXT("}"));
			AppendGeneratedAsLine(Source);
		}
		AppendGeneratedAsLine(Source, TEXT("int RunPropertyFailure()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(
			Source,
			TEXT("\tFPrivatePropertyOwner Receiver = FPrivatePropertyOwner();"));
		if (ProbeCase.bAlternate)
		{
			AppendGeneratedAsLine(
				Source,
				TEXT("\tFPrivatePropertyProbe Probe = FPrivatePropertyProbe();"));
			AppendGeneratedAsLine(Source, TEXT("\treturn Probe.Invoke(Receiver);"));
		}
		else
		{
			AppendGeneratedAsLine(Source, TEXT("\treturn Receiver.Hidden;"));
		}
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
	}

	static void AppendRecursiveHelpers(FString& Source, const FFailureCase& FailureCase)
	{
		using namespace AngelscriptNativeTestSupport;

		if (IsKind(FailureCase, EFailureKind::RecursiveGetter))
		{
			AppendGeneratedAsLine(
				Source,
				TEXT("int ReenterPropertyFailureGet(FPropertyFailure& inout Receiver)"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\treturn Receiver.Value;"));
			AppendGeneratedAsLine(Source, TEXT("}"));
			AppendGeneratedAsLine(Source);
		}
		else if (IsKind(FailureCase, EFailureKind::RecursiveSetter))
		{
			AppendGeneratedAsLine(
				Source,
				TEXT("void ReenterPropertyFailureSet(FPropertyFailure& inout Receiver, int Value)"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\tReceiver.Value = Value;"));
			AppendGeneratedAsLine(Source, TEXT("}"));
			AppendGeneratedAsLine(Source);
		}
	}

	static void AppendRegisteredOperation(
		FString& Source,
		const FFailureCase& FailureCase,
		const TCHAR* Prefix)
	{
		using namespace AngelscriptNativeTestSupport;

		if (IsKind(FailureCase, EFailureKind::MissingGetter)
			|| IsKind(FailureCase, EFailureKind::RecursiveGetter)
			|| IsKind(FailureCase, EFailureKind::ThrowingGetter)
			|| IsKind(FailureCase, EFailureKind::RegisteredDuplicateGetter)
			|| IsKind(FailureCase, EFailureKind::NullReceiver))
		{
			AppendGeneratedAsLine(Source, FString::Printf(TEXT("%sreturn Receiver.Value;"), Prefix));
		}
		else if (IsKind(FailureCase, EFailureKind::RegisteredMismatchedTypes))
		{
			AppendGeneratedAsLine(Source, FString::Printf(TEXT("%sReceiver.Value += 5;"), Prefix));
			AppendGeneratedAsLine(
				Source,
				FString::Printf(TEXT("%sreturn ObservePropertyFailure(Receiver);"), Prefix));
		}
		else
		{
			AppendGeneratedAsLine(Source, FString::Printf(TEXT("%sReceiver.Value = 73;"), Prefix));
			AppendGeneratedAsLine(
				Source,
				FString::Printf(TEXT("%sreturn ObservePropertyFailure(Receiver);"), Prefix));
		}
	}

	static void AppendRegisteredSource(
		FString& Source,
		const FFailureCase& FailureCase,
		const FProbeCase& ProbeCase)
	{
		using namespace AngelscriptNativeTestSupport;

		AppendRecursiveHelpers(Source, FailureCase);
		if (ProbeCase.bAlternate)
		{
			AppendGeneratedAsLine(
				Source,
				TEXT("int InvokePropertyFailure(FPropertyFailure& inout Receiver)"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendRegisteredOperation(Source, FailureCase, TEXT("\t"));
			AppendGeneratedAsLine(Source, TEXT("}"));
			AppendGeneratedAsLine(Source);
		}
		AppendGeneratedAsLine(Source, TEXT("int RunPropertyFailure()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, IsKind(FailureCase, EFailureKind::NullReceiver)
			? TEXT("\tFPropertyFailure Receiver = nullptr;")
			: TEXT("\tFPropertyFailure Receiver = CreatePropertyFailure(31);"));
		if (ProbeCase.bAlternate)
		{
			AppendGeneratedAsLine(Source, TEXT("\treturn InvokePropertyFailure(Receiver);"));
		}
		else
		{
			AppendRegisteredOperation(Source, FailureCase, TEXT("\t"));
		}
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
	}

	static void AppendValueReceiverSource(FString& Source, const FProbeCase& ProbeCase)
	{
		using namespace AngelscriptNativeTestSupport;

		if (ProbeCase.bAlternate)
		{
			AppendGeneratedAsLine(Source, TEXT("int InvokeValuePropertyFailure(FNativeCaseValue& inout Receiver)"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\tReceiver.Access += 5;"));
			AppendGeneratedAsLine(Source, TEXT("\treturn Receiver.Value;"));
			AppendGeneratedAsLine(Source, TEXT("}"));
			AppendGeneratedAsLine(Source);
		}
		AppendGeneratedAsLine(Source, TEXT("int RunPropertyFailure()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tFNativeCaseValue Receiver(31);"));
		if (ProbeCase.bAlternate)
		{
			AppendGeneratedAsLine(Source, TEXT("\treturn InvokeValuePropertyFailure(Receiver);"));
		}
		else
		{
			AppendGeneratedAsLine(Source, TEXT("\tReceiver.Access += 5;"));
			AppendGeneratedAsLine(Source, TEXT("\treturn Receiver.Value;"));
		}
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
	}

	static FString BuildPropertyFailureSource(
		const FFailureCase& FailureCase,
		const FProbeCase& ProbeCase)
	{
		using namespace AngelscriptNativeTestSupport;

		FString Source;
		if (IsKind(FailureCase, EFailureKind::RemovedPropertyDecorator)
			|| IsKind(FailureCase, EFailureKind::RemovedVirtualProperty))
		{
			AppendRemovedSyntax(Source, FailureCase, ProbeCase);
		}
		else if (IsKind(FailureCase, EFailureKind::InaccessibleField))
		{
			AppendInaccessibleSource(Source, ProbeCase);
		}
		else if (IsKind(FailureCase, EFailureKind::CompoundValueReceiver))
		{
			AppendValueReceiverSource(Source, ProbeCase);
		}
		else
		{
			AppendRegisteredSource(Source, FailureCase, ProbeCase);
		}

		AppendGeneratedAsLine(Source, TEXT("int RunPropertyFailureRecovery()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\treturn 97;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		return Source;
	}

	static FString BuildRecoverySource()
	{
		using namespace AngelscriptNativeTestSupport;

		FString Source;
		AppendGeneratedAsLine(Source, TEXT("int RunPropertyFailureRecovery()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\treturn 97;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		return Source;
	}

	static bool HasLocatedDiagnostic(
		const FNativeMessageCollector& Messages,
		const FString& Section,
		const TCHAR* ExpectedText)
	{
		return Messages.Entries.ContainsByPredicate([
			&Section,
			ExpectedText](const AngelscriptNativeTestSupport::FNativeMessageEntry& Entry)
		{
			return Entry.Type == asMSGTYPE_ERROR
				&& Entry.Section == Section
				&& Entry.Row > 0
				&& Entry.Column > 0
				&& Entry.Message.Contains(ExpectedText);
		});
	}

	void VerifyRegistrationMetadata(
		const FNativeCaseContext& Case,
		const FFailureCase& FailureCase,
		asIScriptEngine& ScriptEngine,
		const FRegistrationResult& Registration)
	{
		ASSERT_THAT(AreEqual(int64(3), ScriptEngine.GetEngineProperty(asEP_PROPERTY_ACCESSOR_MODE),
			*Case.Describe(TEXT("failure fixture should retain bare-SDK property accessor mode 3"))));
		if (Registration.GetterFunctionId >= 0)
		{
			asIScriptFunction* const Getter = ScriptEngine.GetFunctionById(Registration.GetterFunctionId);
			ASSERT_THAT(IsNotNull(Getter,
				*Case.Describe(TEXT("failure fixture should retain its registered getter identity"))));
			if (Getter != nullptr)
			{
				ASSERT_THAT(IsTrue(Getter->IsProperty(),
					*Case.Describe(TEXT("failure getter should retain its property trait"))));
			}
		}
		if (Registration.SetterFunctionId >= 0)
		{
			asIScriptFunction* const Setter = ScriptEngine.GetFunctionById(Registration.SetterFunctionId);
			ASSERT_THAT(IsNotNull(Setter,
				*Case.Describe(TEXT("failure fixture should retain its registered setter identity"))));
			if (Setter != nullptr)
			{
				ASSERT_THAT(IsTrue(Setter->IsProperty(),
					*Case.Describe(TEXT("failure setter should retain its property trait"))));
			}
		}
		if (FailureCase.Phase == EFailurePhase::Registration)
		{
			ASSERT_THAT(AreEqual(static_cast<int32>(asALREADY_REGISTERED), Registration.DuplicateResult,
				*Case.Describe(TEXT("duplicate accessor registration should return asALREADY_REGISTERED"))));
			const int32 SurvivingId = IsKind(FailureCase, EFailureKind::RegisteredDuplicateGetter)
				? Registration.GetterFunctionId
				: Registration.SetterFunctionId;
			ASSERT_THAT(IsNotNull(ScriptEngine.GetFunctionById(SurvivingId),
				*Case.Describe(TEXT("duplicate rejection should preserve exactly the first accessor identity"))));
		}
	}

	void VerifyTrace(
		const FNativeCaseContext& Case,
		const FFailureCase& FailureCase,
		const FRegistrationResult& Registration,
		const FPropertyFailureState& State)
	{
		TArray<int32> Expected;
		if (IsKind(FailureCase, EFailureKind::RecursiveGetter))
		{
			Expected = { 100, 101 };
		}
		else if (IsKind(FailureCase, EFailureKind::RecursiveSetter))
		{
			Expected = { 200, 201 };
		}
		else if (IsKind(FailureCase, EFailureKind::ThrowingGetter))
		{
			Expected = { 100 };
		}
		else if (IsKind(FailureCase, EFailureKind::ThrowingSetter))
		{
			Expected = { 200 };
		}
		else if (FailureCase.Phase == EFailurePhase::Registration)
		{
			Expected = { Registration.ExpectedSelectedMarker };
		}
		ASSERT_THAT(AreEqual(Expected.Num(), State.Trace.Num(),
			*Case.Describe(TEXT("property failure should stop at the exact callback boundary"))));
		for (int32 Index = 0; Index < FMath::Min(Expected.Num(), State.Trace.Num()); ++Index)
		{
			ASSERT_THAT(AreEqual(Expected[Index], State.Trace[Index],
				*Case.Describe(TEXT("property failure callback trace should preserve exact order"))));
		}
		ASSERT_THAT(AreEqual(0, State.RecursionDepth,
			*Case.Describe(TEXT("property failure should restore recursive callback depth"))));
	}

	void VerifyLifecycle(
		const FNativeCaseContext& Case,
		const FPropertyFailureState& State)
	{
		ASSERT_THAT(AreEqual(0, State.LiveObjects,
			*Case.Describe(TEXT("property failure should leave no native receiver alive"))));
		ASSERT_THAT(AreEqual(State.CreatedObjects, State.DestroyedObjects,
			*Case.Describe(TEXT("property failure should balance receiver construction and destruction"))));
	}

	void ExecuteRecoveryFunction(
		const FNativeCaseContext& Case,
		asIScriptContext& Context,
		asIScriptFunction& Recovery)
	{
		ASSERT_THAT(IsTrue(Context.Unprepare() >= 0,
			*Case.Describe(TEXT("property-failure context should unprepare before recovery"))));
		ASSERT_THAT(IsTrue(Context.Prepare(&Recovery) >= 0,
			*Case.Describe(TEXT("property-failure context should prepare recovery"))));
		ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_FINISHED), Context.Execute(),
			*Case.Describe(TEXT("property-failure recovery should finish"))));
		ASSERT_THAT(AreEqual(97, static_cast<int32>(Context.GetReturnDWord()),
			*Case.Describe(TEXT("property-failure recovery should return its clean sentinel"))));
	}

	void ExecuteFaultingOrRegistrationCell(
		const FNativeCaseContext& Case,
		const FFailureCase& FailureCase,
		const FRecoveryCase& RecoveryCase,
		const FRegistrationResult& Registration,
		asIScriptEngine& ScriptEngine,
		asIScriptModule& Module,
		FPropertyFailureState& State)
	{
		asIScriptFunction* const Entry = Module.GetFunctionByDecl("int RunPropertyFailure()");
		asIScriptFunction* const Recovery = Module.GetFunctionByDecl("int RunPropertyFailureRecovery()");
		ASSERT_THAT(IsNotNull(Entry,
			*Case.Describe(TEXT("runtime/registration property cell should expose its exact entry"))));
		ASSERT_THAT(IsNotNull(Recovery,
			*Case.Describe(TEXT("runtime/registration property cell should expose same-context recovery"))));
		if (Entry == nullptr || Recovery == nullptr)
		{
			return;
		}

		asIScriptContext* const Context = ScriptEngine.CreateContext();
		ASSERT_THAT(IsNotNull(Context,
			*Case.Describe(TEXT("runtime/registration property cell should create a context"))));
		if (Context == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(Context->Prepare(Entry) >= 0,
			*Case.Describe(TEXT("runtime/registration property context should prepare its entry"))));
		const int ExecuteResult = Context->Execute();
		if (FailureCase.Phase == EFailurePhase::Runtime)
		{
			ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_EXCEPTION), ExecuteResult,
				*Case.Describe(TEXT("runtime property failure should raise an exception"))));
			ASSERT_THAT(AreEqual(
				FString(FailureCase.ExpectedText),
				FString(UTF8_TO_TCHAR(Context->GetExceptionString())),
				*Case.Describe(TEXT("runtime property failure should preserve its exact exception"))));
			int Column = 0;
			const char* Section = nullptr;
			ASSERT_THAT(IsTrue(Context->GetExceptionLineNumber(&Column, &Section) > 0
				&& Column > 0
				&& Section != nullptr,
				*Case.Describe(TEXT("runtime property failure should expose its exact operation location"))));
		}
		else
		{
			ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_FINISHED), ExecuteResult,
				*Case.Describe(TEXT("surviving first accessor should execute after duplicate rejection"))));
			const int32 ExpectedResult = IsKind(FailureCase, EFailureKind::RegisteredDuplicateGetter)
				? 31
				: 73;
			ASSERT_THAT(AreEqual(ExpectedResult, static_cast<int32>(Context->GetReturnDWord()),
				*Case.Describe(TEXT("surviving first accessor should preserve runtime behavior"))));
		}
		VerifyTrace(Case, FailureCase, Registration, State);

		if (RecoveryCase.bSameState)
		{
			ExecuteRecoveryFunction(Case, *Context, *Recovery);
		}
		ASSERT_THAT(IsTrue(Context->Unprepare() >= 0,
			*Case.Describe(TEXT("runtime/registration property context should release its receiver"))));
		Context->Release();
		VerifyLifecycle(Case, State);
	}

	void CompileAndExecuteFreshRecovery(
		const FNativeCaseContext& Case,
		AngelscriptNativeTestSupport::FNativeTestEngine& Engine,
		asIScriptEngine& ScriptEngine,
		const FString& ModuleName)
	{
		using namespace AngelscriptNativeTestSupport;

		const FString RecoveryModuleName = ModuleName + TEXT("_Recovery");
		const FString RecoverySource = BuildRecoverySource();
		PrintGeneratedAsSource(
			*TestRunner,
			Case.GetId() + TEXT("-RECOVERY"),
			RecoveryModuleName,
			RecoverySource);
		const FTCHARToUTF8 RecoveryModuleNameUtf8(*RecoveryModuleName);
		const FTCHARToUTF8 RecoverySourceUtf8(*RecoverySource);
		Engine.ResetMessages();
		asIScriptModule* RecoveryModule = nullptr;
		ASSERT_THAT(IsTrue(CompileNativeModule(
			&ScriptEngine,
			RecoveryModuleNameUtf8.Get(),
			RecoverySourceUtf8.Get(),
			RecoveryModule) >= 0,
			*Case.Describe(TEXT("property failure should permit a clean fresh recovery module"))));
		ASSERT_THAT(IsNotNull(RecoveryModule,
			*Case.Describe(TEXT("fresh property recovery should publish a module"))));
		if (RecoveryModule != nullptr)
		{
			asIScriptFunction* const Recovery =
				RecoveryModule->GetFunctionByDecl("int RunPropertyFailureRecovery()");
			ASSERT_THAT(IsNotNull(Recovery,
				*Case.Describe(TEXT("fresh property recovery should expose its exact entry"))));
			if (Recovery != nullptr)
			{
				asIScriptContext* const Context = ScriptEngine.CreateContext();
				ASSERT_THAT(IsNotNull(Context,
					*Case.Describe(TEXT("fresh property recovery should create a context"))));
				if (Context != nullptr)
				{
					ASSERT_THAT(AreEqual(
						static_cast<int32>(asEXECUTION_FINISHED),
						PrepareAndExecute(Context, Recovery),
						*Case.Describe(TEXT("fresh property recovery should finish"))));
					ASSERT_THAT(AreEqual(97, static_cast<int32>(Context->GetReturnDWord()),
						*Case.Describe(TEXT("fresh property recovery should return its sentinel"))));
					Context->Release();
				}
			}
		}
		ScriptEngine.DiscardModule(RecoveryModuleNameUtf8.Get());
		ASSERT_THAT(IsNull(ScriptEngine.GetModule(
			RecoveryModuleNameUtf8.Get(),
			asGM_ONLY_IF_EXISTS),
			*Case.Describe(TEXT("fresh property recovery should discard its module"))));
	}

	void CompileAndExecuteSameNameRecovery(
		const FNativeCaseContext& Case,
		AngelscriptNativeTestSupport::FNativeTestEngine& Engine,
		asIScriptEngine& ScriptEngine,
		const FString& ModuleName)
	{
		using namespace AngelscriptNativeTestSupport;

		const FString RecoverySource = BuildRecoverySource();
		PrintGeneratedAsSource(
			*TestRunner,
			Case.GetId() + TEXT("-RECOVERY"),
			ModuleName,
			RecoverySource);
		const FTCHARToUTF8 ModuleNameUtf8(*ModuleName);
		const FTCHARToUTF8 RecoverySourceUtf8(*RecoverySource);
		Engine.ResetMessages();
		asIScriptModule* RecoveryModule = nullptr;
		ASSERT_THAT(IsTrue(CompileNativeModule(
			&ScriptEngine,
			ModuleNameUtf8.Get(),
			RecoverySourceUtf8.Get(),
			RecoveryModule) >= 0,
			*Case.Describe(TEXT("compile-time property failure should permit same-name recovery"))));
		ASSERT_THAT(IsNotNull(RecoveryModule,
			*Case.Describe(TEXT("same-name property recovery should publish a module"))));
		if (RecoveryModule != nullptr)
		{
			asIScriptFunction* const Recovery =
				RecoveryModule->GetFunctionByDecl("int RunPropertyFailureRecovery()");
			ASSERT_THAT(IsNotNull(Recovery,
				*Case.Describe(TEXT("same-name property recovery should expose its exact entry"))));
			if (Recovery != nullptr)
			{
				asIScriptContext* const Context = ScriptEngine.CreateContext();
				ASSERT_THAT(IsNotNull(Context,
					*Case.Describe(TEXT("same-name property recovery should create a context"))));
				if (Context != nullptr)
				{
					ASSERT_THAT(AreEqual(
						static_cast<int32>(asEXECUTION_FINISHED),
						PrepareAndExecute(Context, Recovery),
						*Case.Describe(TEXT("same-name property recovery should finish"))));
					ASSERT_THAT(AreEqual(97, static_cast<int32>(Context->GetReturnDWord()),
						*Case.Describe(TEXT("same-name property recovery should return its sentinel"))));
					Context->Release();
				}
			}
		}
		ScriptEngine.DiscardModule(ModuleNameUtf8.Get());
	}

public:
	TEST_METHOD(FailuresByRecoveryAndProbe)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT("LANG-PROP-FAILURE",
			ENativeEvidence::Compile
				| ENativeEvidence::Diagnostic
				| ENativeEvidence::Runtime
				| ENativeEvidence::Metadata
				| ENativeEvidence::Lifecycle
				| ENativeEvidence::Debug
				| ENativeEvidence::Cleanup
				| ENativeEvidence::Isolation);

		for (const FFailureCase& FailureCase : FailureCases)
		{
			for (const FProbeCase& ProbeCase : ProbeCases)
			{
				for (const FRecoveryCase& RecoveryCase : RecoveryCases)
				{
					const FNativeCaseContext Case(MakeNativeCaseId(
						"LANG-PROP-FAILURE",
						{
							ANSI_TO_TCHAR(FailureCase.CatalogName),
							ANSI_TO_TCHAR(ProbeCase.CatalogName),
							ANSI_TO_TCHAR(RecoveryCase.CatalogName),
						}));
					FPropertyFailureState State;
					State.Reset(FailureCase.Kind);
					ActiveState = &State;
					FNativeLifecycleRecorder Lifecycle;
					Lifecycle.Reset();
					FNativeTestEngine Engine;
					Engine.Create(*TestRunner);
					asIScriptEngine* const ScriptEngine = Engine.Get();
					ASSERT_THAT(IsNotNull(ScriptEngine,
						*Case.Describe(TEXT("property-failure cell should create an isolated raw SDK engine"))));
					if (ScriptEngine == nullptr)
					{
						Engine.Destroy();
						ActiveState = nullptr;
						continue;
					}

					ASSERT_THAT(IsTrue(RegisterNativeCaseValue(*ScriptEngine, Lifecycle),
						*Case.Describe(TEXT("property-failure cell should register its tracked value receiver"))));
					const FString Suffix = MakeSuffix(FailureCase, ProbeCase, RecoveryCase);
					const FString ModuleName = TEXT("PropertyFailure_") + Suffix;
					const FString Source = BuildPropertyFailureSource(FailureCase, ProbeCase);
					PrintGeneratedAsSource(*TestRunner, Case.GetId(), ModuleName, Source);
					const FRegistrationResult Registration = RegisterFailureFixture(
						*ScriptEngine,
						FailureCase,
						ProbeCase);
					ASSERT_THAT(IsTrue(Registration.bSucceeded,
						*Case.Describe(TEXT("property-failure fixture should reach its expected registration state"))));
					VerifyRegistrationMetadata(Case, FailureCase, *ScriptEngine, Registration);

					const FTCHARToUTF8 ModuleNameUtf8(*ModuleName);
					const FTCHARToUTF8 SourceUtf8(*Source);
					Engine.ResetMessages();
					asIScriptModule* Module = nullptr;
					const int BuildResult = CompileNativeModule(
						ScriptEngine,
						ModuleNameUtf8.Get(),
						SourceUtf8.Get(),
						Module);
					if (FailureCase.Phase == EFailurePhase::Compile)
					{
						ASSERT_THAT(IsTrue(BuildResult < 0,
							*Case.Describe(TEXT("compile-time property failure should be rejected"))));
						ASSERT_THAT(IsTrue(HasLocatedDiagnostic(
							Engine.GetMessages(),
							ModuleName,
							FailureCase.ExpectedText),
							*Case.Describe(TEXT("compile-time property failure should report its exact located diagnostic"))));
						ASSERT_THAT(AreEqual(0, State.Trace.Num(),
							*Case.Describe(TEXT("compile-time property failure should invoke no accessor callback"))));
						ASSERT_THAT(IsTrue(Module == nullptr
							|| Module->GetFunctionByDecl("int RunPropertyFailure()") == nullptr,
							*Case.Describe(TEXT("failed property build should not publish its execution entry"))));
					}
					else
					{
						ASSERT_THAT(IsTrue(BuildResult >= 0,
							*Case.Describe(TEXT("runtime/registration property failure source should compile"))));
						ASSERT_THAT(IsNotNull(Module,
							*Case.Describe(TEXT("runtime/registration property failure should publish a module"))));
						if (BuildResult >= 0 && Module != nullptr)
						{
							ExecuteFaultingOrRegistrationCell(
								Case,
								FailureCase,
								RecoveryCase,
								Registration,
								*ScriptEngine,
								*Module,
								State);
						}
					}

					ScriptEngine->DiscardModule(ModuleNameUtf8.Get());
					ASSERT_THAT(IsNull(ScriptEngine->GetModule(
						ModuleNameUtf8.Get(),
						asGM_ONLY_IF_EXISTS),
						*Case.Describe(TEXT("property-failure cell should discard its failing module"))));
					if (FailureCase.Phase == EFailurePhase::Compile && RecoveryCase.bSameState)
					{
						CompileAndExecuteSameNameRecovery(
							Case,
							Engine,
							*ScriptEngine,
							ModuleName);
					}
					else if (!RecoveryCase.bSameState)
					{
						CompileAndExecuteFreshRecovery(
							Case,
							Engine,
							*ScriptEngine,
							ModuleName);
					}

					VerifyLifecycle(Case, State);
					ASSERT_THAT(AreEqual(0, Lifecycle.GetLiveObjectCount(),
						*Case.Describe(TEXT("property-failure cell should leave no tracked value receiver"))));
					Engine.Destroy();
					VerifyLifecycle(Case, State);
					ActiveState = nullptr;
				}
			}
		}
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
