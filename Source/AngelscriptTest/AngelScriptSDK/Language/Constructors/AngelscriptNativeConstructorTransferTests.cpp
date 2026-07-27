#include "../../Support/AngelscriptNativeFixtureTestSupport.h"
#include "../../Support/AngelscriptNativeLanguageCaseTestSupport.h"

#include "CQTest.h"
#include "Misc/ScopeExit.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FConstructorTransferTests,
	"Angelscript.TestModule.AngelScriptSDK.Language.Constructors.Transfer",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	using ENativeLifecycleEvent =
		AngelscriptNativeTestSupport::ENativeLifecycleEvent;
	using FNativeCaseContext =
		AngelscriptNativeTestSupport::FNativeCaseContext;
	using FNativeLifecycleEntry =
		AngelscriptNativeTestSupport::FNativeLifecycleEntry;
	using FNativeLifecycleRecorder =
		AngelscriptNativeTestSupport::FNativeLifecycleRecorder;
	using FNativeTestEngine =
		AngelscriptNativeTestSupport::FNativeTestEngine;

	static constexpr asPWORD ConstructorTransferStateUserDataSlot =
		static_cast<asPWORD>(0x43544F525452414Eull);

	enum class ETransferObjectKind : uint8
	{
		ScriptValue,
		NativeValue,
		ScriptReference,
		NativeReference,
		DerivedReference,
	};

	enum class ETransferSourceRoute : uint8
	{
		Local,
		Temporary,
		Return,
		ExactDerived,
		BaseView,
	};

	struct FSourceCase
	{
		const ANSICHAR* CatalogName;
		ETransferObjectKind ObjectKind;
		ETransferSourceRoute Route;
		int32 DynamicMarker;
	};

	struct FWorkflowCase
	{
		const ANSICHAR* CatalogName;
	};

	struct FObservationCase
	{
		const ANSICHAR* CatalogName;
	};

	inline static constexpr FSourceCase SourceCases[] =
	{
		{ "script_value_local", ETransferObjectKind::ScriptValue, ETransferSourceRoute::Local, 101 },
		{ "script_value_temporary", ETransferObjectKind::ScriptValue, ETransferSourceRoute::Temporary, 101 },
		{ "script_value_return", ETransferObjectKind::ScriptValue, ETransferSourceRoute::Return, 101 },
		{ "native_value_local", ETransferObjectKind::NativeValue, ETransferSourceRoute::Local, 201 },
		{ "native_value_temporary", ETransferObjectKind::NativeValue, ETransferSourceRoute::Temporary, 201 },
		{ "native_value_return", ETransferObjectKind::NativeValue, ETransferSourceRoute::Return, 201 },
		{ "script_reference_local", ETransferObjectKind::ScriptReference, ETransferSourceRoute::Local, 301 },
		{ "script_reference_temporary", ETransferObjectKind::ScriptReference, ETransferSourceRoute::Temporary, 301 },
		{ "script_reference_return", ETransferObjectKind::ScriptReference, ETransferSourceRoute::Return, 301 },
		{ "native_reference_local", ETransferObjectKind::NativeReference, ETransferSourceRoute::Local, 401 },
		{ "native_reference_temporary", ETransferObjectKind::NativeReference, ETransferSourceRoute::Temporary, 401 },
		{ "native_reference_return", ETransferObjectKind::NativeReference, ETransferSourceRoute::Return, 401 },
		{ "derived_reference_exact", ETransferObjectKind::DerivedReference, ETransferSourceRoute::ExactDerived, 502 },
		{ "derived_reference_base_view", ETransferObjectKind::DerivedReference, ETransferSourceRoute::BaseView, 502 },
	};

	inline static constexpr FWorkflowCase WorkflowCases[] =
	{
		{ "copy_declaration_local" },
		{ "field_constructor_transfer" },
		{ "assignment_local" },
		{ "field_assignment_after_default" },
		{ "argument_transfer" },
		{ "return_transfer" },
		{ "self_assignment" },
		{ "chained_assignment" },
	};

	inline static constexpr FObservationCase ObservationCases[] =
	{
		{ "initial_identity_value" },
		{ "source_or_temporary_state" },
		{ "target_mutation_relation" },
		{ "lifecycle_cleanup" },
	};

	struct FConstructorTransferObservation
	{
		int32 Stage = INDEX_NONE;
		int32 SourceValue = 0;
		int32 TargetValue = 0;
		int32 AuxiliaryValue = 0;
		int32 Relation = 0;
		int32 DynamicMarker = 0;
	};

	struct FConstructorTransferSnapshot
	{
		int32 Stage = INDEX_NONE;
		int32 Constructions = 0;
		int32 Copies = 0;
		int32 Assignments = 0;
		int32 AddRefs = 0;
		int32 Releases = 0;
		int32 Destructions = 0;
		int32 LiveObjects = 0;
	};

	struct FConstructorTransferState
	{
		TArray<FConstructorTransferObservation> Observations;
		TArray<FConstructorTransferSnapshot> Snapshots;
		FNativeLifecycleRecorder* Lifecycle = nullptr;

		void Reset(FNativeLifecycleRecorder& InLifecycle)
		{
			Observations.Reset();
			Snapshots.Reset();
			Lifecycle = &InLifecycle;
		}
	};

	static bool IsWorkflow(
		const FWorkflowCase& WorkflowCase,
		const ANSICHAR* Name)
	{
		return FCStringAnsi::Strcmp(WorkflowCase.CatalogName, Name) == 0;
	}

	static bool IsValueObject(const FSourceCase& SourceCase)
	{
		return SourceCase.ObjectKind == ETransferObjectKind::ScriptValue
			|| SourceCase.ObjectKind == ETransferObjectKind::NativeValue;
	}

	static bool IsReferenceObject(const FSourceCase& SourceCase)
	{
		return !IsValueObject(SourceCase);
	}

	static bool IsScriptObject(const FSourceCase& SourceCase)
	{
		return SourceCase.ObjectKind == ETransferObjectKind::ScriptValue
			|| SourceCase.ObjectKind == ETransferObjectKind::ScriptReference
			|| SourceCase.ObjectKind == ETransferObjectKind::DerivedReference;
	}

	static bool IsCompositeScriptObject(const FSourceCase& SourceCase)
	{
		return IsScriptObject(SourceCase);
	}

	static bool HasRetainedSource(const FSourceCase& SourceCase)
	{
		return SourceCase.Route != ETransferSourceRoute::Temporary;
	}

	static bool IsDerivedCase(const FSourceCase& SourceCase)
	{
		return SourceCase.ObjectKind == ETransferObjectKind::DerivedReference;
	}

	static bool IsNativeReferenceTemporarySelfAssignment(
		const FSourceCase& SourceCase,
		const FWorkflowCase& WorkflowCase)
	{
		return SourceCase.ObjectKind == ETransferObjectKind::NativeReference
			&& SourceCase.Route == ETransferSourceRoute::Temporary
			&& IsWorkflow(WorkflowCase, "self_assignment");
	}

	static bool IsScriptReferenceLocalCopyDeclaration(
		const FSourceCase& SourceCase,
		const FWorkflowCase& WorkflowCase)
	{
		return SourceCase.ObjectKind == ETransferObjectKind::ScriptReference
			&& SourceCase.Route == ETransferSourceRoute::Local
			&& IsWorkflow(WorkflowCase, "copy_declaration_local");
	}

	static FString TypeName(const FSourceCase& SourceCase)
	{
		switch (SourceCase.ObjectKind)
		{
		case ETransferObjectKind::ScriptValue:
			return TEXT("FTransferValue");
		case ETransferObjectKind::NativeValue:
			return TEXT("FNativeCaseValue");
		case ETransferObjectKind::ScriptReference:
			return TEXT("FTransferReference");
		case ETransferObjectKind::NativeReference:
			return TEXT("FNativeCaseReference");
		case ETransferObjectKind::DerivedReference:
			return SourceCase.Route == ETransferSourceRoute::BaseView
				? TEXT("FTransferBase")
				: TEXT("FTransferDerived");
		default:
			return TEXT("FTransferValue");
		}
	}

	static FString CreationExpression(
		const FSourceCase& SourceCase,
		const int32 Value)
	{
		if (SourceCase.ObjectKind == ETransferObjectKind::NativeReference)
		{
			return FString::Printf(
				TEXT("CreateNativeCaseReference(%d)"),
				Value);
		}
		if (SourceCase.ObjectKind == ETransferObjectKind::DerivedReference)
		{
			return FString::Printf(TEXT("FTransferDerived(%d)"), Value);
		}
		return FString::Printf(
			TEXT("%s(%d)"),
			*TypeName(SourceCase),
			Value);
	}

	static FString DefaultExpression(const FSourceCase& SourceCase)
	{
		if (SourceCase.ObjectKind == ETransferObjectKind::NativeReference)
		{
			return TEXT("CreateNativeCaseReference(0)");
		}
		if (SourceCase.ObjectKind == ETransferObjectKind::DerivedReference)
		{
			return SourceCase.Route == ETransferSourceRoute::BaseView
				? TEXT("FTransferBase(0)")
				: TEXT("FTransferDerived(0)");
		}
		return FString::Printf(
			TEXT("%s(0)"),
			*TypeName(SourceCase));
	}

	static int32 StateValue(
		const FSourceCase& SourceCase,
		const int32 Value)
	{
		return IsCompositeScriptObject(SourceCase)
			? Value * 10000 + (Value + 100)
			: Value;
	}

	static FConstructorTransferState* GetActiveTransferState()
	{
		asIScriptContext* const Context = asGetActiveContext();
		return Context != nullptr && Context->GetEngine() != nullptr
			? static_cast<FConstructorTransferState*>(
				Context->GetEngine()->GetUserData(
					ConstructorTransferStateUserDataSlot))
			: nullptr;
	}

	static void RecordConstructorTransferState(
		const int32 Stage,
		const int32 SourceValue,
		const int32 TargetValue,
		const int32 AuxiliaryValue,
		const int32 Relation,
		const int32 DynamicMarker)
	{
		if (FConstructorTransferState* const State =
			GetActiveTransferState())
		{
			State->Observations.Add({
				Stage,
				SourceValue,
				TargetValue,
				AuxiliaryValue,
				Relation,
				DynamicMarker,
			});
		}
	}

	static void RecordConstructorTransferBoundary(const int32 Stage)
	{
		FConstructorTransferState* const State = GetActiveTransferState();
		if (State == nullptr || State->Lifecycle == nullptr)
		{
			return;
		}

		const FNativeLifecycleRecorder& Lifecycle = *State->Lifecycle;
		State->Snapshots.Add({
			Stage,
			Lifecycle.Num(ENativeLifecycleEvent::DefaultConstruct)
				+ Lifecycle.Num(ENativeLifecycleEvent::ValueConstruct)
				+ Lifecycle.Num(ENativeLifecycleEvent::CopyConstruct),
			Lifecycle.Num(ENativeLifecycleEvent::CopyConstruct),
			Lifecycle.Num(ENativeLifecycleEvent::Assign),
			Lifecycle.Num(ENativeLifecycleEvent::AddRef),
			Lifecycle.Num(ENativeLifecycleEvent::Release),
			Lifecycle.Num(ENativeLifecycleEvent::Destruct),
			Lifecycle.GetLiveObjectCount(),
		});
	}

	static bool RegisterConstructorTransferBridge(
		asIScriptEngine& ScriptEngine,
		FConstructorTransferState& State)
	{
		ScriptEngine.SetUserData(
			&State,
			ConstructorTransferStateUserDataSlot);
		const ASAutoCaller::FunctionCaller ObservationCaller =
			ASAutoCaller::MakeFunctionCaller(
				RecordConstructorTransferState);
		const ASAutoCaller::FunctionCaller BoundaryCaller =
			ASAutoCaller::MakeFunctionCaller(
				RecordConstructorTransferBoundary);
		return ScriptEngine.RegisterGlobalFunction(
			"void RecordConstructorTransferState(int Stage, int SourceValue, int TargetValue, int AuxiliaryValue, int Relation, int DynamicMarker)",
			asFUNCTION(RecordConstructorTransferState),
			asCALL_CDECL,
			*(asFunctionCaller*)&ObservationCaller) >= 0
			&& ScriptEngine.RegisterGlobalFunction(
				"void RecordConstructorTransferBoundary(int Stage)",
				asFUNCTION(RecordConstructorTransferBoundary),
				asCALL_CDECL,
				*(asFunctionCaller*)&BoundaryCaller) >= 0;
	}

	static void AppendScriptValueType(FString& Source)
	{
		using namespace AngelscriptNativeTestSupport;

		AppendGeneratedAsLine(Source, TEXT("struct FTransferValue"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tint Value = 0;"));
		AppendGeneratedAsLine(Source, TEXT("\tint Nested = 100;"));
		AppendGeneratedAsLine(Source, TEXT("\tint ObjectId = -1;"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("\tFTransferValue()"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(
			Source,
			TEXT("\t\tObjectId = BeginNativeScriptLifecycle(Value);"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(
			Source,
			TEXT("\tFTransferValue(int InValue)"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(Source, TEXT("\t\tValue = InValue;"));
		AppendGeneratedAsLine(
			Source,
			TEXT("\t\tNested = InValue + 100;"));
		AppendGeneratedAsLine(
			Source,
			TEXT("\t\tObjectId = BeginNativeScriptLifecycle(Value);"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(
			Source,
			TEXT("\tFTransferValue(const FTransferValue& Other)"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(Source, TEXT("\t\tValue = Other.Value;"));
		AppendGeneratedAsLine(Source, TEXT("\t\tNested = Other.Nested;"));
		AppendGeneratedAsLine(
			Source,
			TEXT("\t\tObjectId = CopyNativeScriptLifecycle(Other.ObjectId, Value);"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(
			Source,
			TEXT("\tFTransferValue& opAssign(const FTransferValue& Other)"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(
			Source,
			TEXT("\t\tAssignNativeScriptLifecycle(ObjectId, Other.ObjectId, Other.Value);"));
		AppendGeneratedAsLine(Source, TEXT("\t\tValue = Other.Value;"));
		AppendGeneratedAsLine(Source, TEXT("\t\tNested = Other.Nested;"));
		AppendGeneratedAsLine(Source, TEXT("\t\treturn this;"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("\t~FTransferValue()"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(
			Source,
			TEXT("\t\tEndNativeScriptLifecycle(ObjectId, Value);"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
	}

	static void AppendScriptReferenceType(FString& Source)
	{
		using namespace AngelscriptNativeTestSupport;

		AppendGeneratedAsLine(Source, TEXT("class FTransferReference"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tint Value = 0;"));
		AppendGeneratedAsLine(Source, TEXT("\tint Nested = 100;"));
		AppendGeneratedAsLine(Source, TEXT("\tint ObjectId = -1;"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("\tFTransferReference()"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(
			Source,
			TEXT("\t\tObjectId = BeginNativeScriptLifecycle(Value);"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(
			Source,
			TEXT("\tFTransferReference(int InValue)"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(Source, TEXT("\t\tValue = InValue;"));
		AppendGeneratedAsLine(
			Source,
			TEXT("\t\tNested = InValue + 100;"));
		AppendGeneratedAsLine(
			Source,
			TEXT("\t\tObjectId = BeginNativeScriptLifecycle(Value);"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("\tint DynamicKind()"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(Source, TEXT("\t\treturn 301;"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("\t~FTransferReference()"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(
			Source,
			TEXT("\t\tEndNativeScriptLifecycle(ObjectId, Value);"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
	}

	static void AppendDerivedReferenceTypes(FString& Source)
	{
		using namespace AngelscriptNativeTestSupport;

		AppendGeneratedAsLine(Source, TEXT("class FTransferBase"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tint Value = 0;"));
		AppendGeneratedAsLine(Source, TEXT("\tint Nested = 100;"));
		AppendGeneratedAsLine(Source, TEXT("\tint ObjectId = -1;"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("\tFTransferBase()"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(
			Source,
			TEXT("\t\tObjectId = BeginNativeScriptLifecycle(Value);"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("\tFTransferBase(int InValue)"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(Source, TEXT("\t\tValue = InValue;"));
		AppendGeneratedAsLine(
			Source,
			TEXT("\t\tNested = InValue + 100;"));
		AppendGeneratedAsLine(
			Source,
			TEXT("\t\tObjectId = BeginNativeScriptLifecycle(Value);"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("\tint DynamicKind()"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(Source, TEXT("\t\treturn 501;"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("\t~FTransferBase()"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(
			Source,
			TEXT("\t\tEndNativeScriptLifecycle(ObjectId, Value);"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(
			Source,
			TEXT("class FTransferDerived : FTransferBase"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tint DerivedObjectId = -1;"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("\tFTransferDerived()"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(Source, TEXT("\t\tsuper();"));
		AppendGeneratedAsLine(
			Source,
			TEXT("\t\tDerivedObjectId = BeginNativeScriptLifecycle(Value + 1000);"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(
			Source,
			TEXT("\tFTransferDerived(int InValue)"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(Source, TEXT("\t\tsuper(InValue);"));
		AppendGeneratedAsLine(
			Source,
			TEXT("\t\tDerivedObjectId = BeginNativeScriptLifecycle(Value + 1000);"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("\tint DynamicKind()"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(Source, TEXT("\t\treturn 502;"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("\t~FTransferDerived()"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(
			Source,
			TEXT("\t\tEndNativeScriptLifecycle(DerivedObjectId, Value + 1000);"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
	}

	static void AppendTypeDeclarations(
		FString& Source,
		const FSourceCase& SourceCase)
	{
		if (SourceCase.ObjectKind == ETransferObjectKind::ScriptValue)
		{
			AppendScriptValueType(Source);
		}
		else if (
			SourceCase.ObjectKind
			== ETransferObjectKind::ScriptReference)
		{
			AppendScriptReferenceType(Source);
		}
		else if (
			SourceCase.ObjectKind
			== ETransferObjectKind::DerivedReference)
		{
			AppendDerivedReferenceTypes(Source);
		}
	}

	static FString ParameterDeclaration(
		const FSourceCase& SourceCase,
		const TCHAR* Name)
	{
		return IsValueObject(SourceCase)
			? FString::Printf(
				TEXT("const %s& in %s"),
				*TypeName(SourceCase),
				Name)
			: FString::Printf(
				TEXT("%s %s"),
				*TypeName(SourceCase),
				Name);
	}

	static FString MutableParameterDeclaration(
		const FSourceCase& SourceCase,
		const TCHAR* Name)
	{
		return IsValueObject(SourceCase)
			? FString::Printf(
				TEXT("%s& inout %s"),
				*TypeName(SourceCase),
				Name)
			: FString::Printf(
				TEXT("%s %s"),
				*TypeName(SourceCase),
				Name);
	}

	static FString ValueParameterDeclaration(
		const FSourceCase& SourceCase,
		const TCHAR* Name)
	{
		return FString::Printf(
			TEXT("%s %s"),
			*TypeName(SourceCase),
			Name);
	}

	static void AppendStateHelpers(
		FString& Source,
		const FSourceCase& SourceCase)
	{
		using namespace AngelscriptNativeTestSupport;

		AppendGeneratedAsLine(Source, FString::Printf(
			TEXT("int ReadTransferState(%s)"),
			*ParameterDeclaration(SourceCase, TEXT("Object"))));
		AppendGeneratedAsLine(Source, TEXT("{"));
		if (IsCompositeScriptObject(SourceCase))
		{
			AppendGeneratedAsLine(
				Source,
				TEXT("\treturn Object.Value * 10000 + Object.Nested;"));
		}
		else
		{
			AppendGeneratedAsLine(Source, TEXT("\treturn Object.Value;"));
		}
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, FString::Printf(
			TEXT("void SetTransferState(%s, int NewValue)"),
			*MutableParameterDeclaration(SourceCase, TEXT("Object"))));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tObject.Value = NewValue;"));
		if (IsCompositeScriptObject(SourceCase))
		{
			AppendGeneratedAsLine(
				Source,
				TEXT("\tObject.Nested = NewValue + 100;"));
		}
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, FString::Printf(
			TEXT("int ReadTransferDynamicKind(%s)"),
			*ParameterDeclaration(SourceCase, TEXT("Object"))));
		AppendGeneratedAsLine(Source, TEXT("{"));
		if (IsDerivedCase(SourceCase)
			|| SourceCase.ObjectKind
				== ETransferObjectKind::ScriptReference)
		{
			AppendGeneratedAsLine(
				Source,
				TEXT("\treturn Object.DynamicKind();"));
		}
		else
		{
			AppendGeneratedAsLine(Source, FString::Printf(
				TEXT("\treturn %d;"),
				SourceCase.DynamicMarker));
		}
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
	}

	static void AppendSourceFactory(
		FString& Source,
		const FSourceCase& SourceCase)
	{
		using namespace AngelscriptNativeTestSupport;

		if (SourceCase.Route != ETransferSourceRoute::Return)
		{
			return;
		}
		AppendGeneratedAsLine(Source, FString::Printf(
			TEXT("%s MakeConstructorTransferSource()"),
			*TypeName(SourceCase)));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, FString::Printf(
			TEXT("\treturn %s;"),
			*CreationExpression(SourceCase, 7)));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
	}

	static void AppendHolderType(
		FString& Source,
		const FSourceCase& SourceCase,
		const FWorkflowCase& WorkflowCase)
	{
		using namespace AngelscriptNativeTestSupport;

		if (!IsWorkflow(
				WorkflowCase,
				"field_constructor_transfer")
			&& !IsWorkflow(
				WorkflowCase,
				"field_assignment_after_default"))
		{
			return;
		}

		AppendGeneratedAsLine(Source, TEXT("struct FTransferHolder"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, FString::Printf(
			TEXT("\t%s Target;"),
			*TypeName(SourceCase)));
		if (IsWorkflow(
			WorkflowCase,
			"field_constructor_transfer"))
		{
			AppendGeneratedAsLine(Source);
			AppendGeneratedAsLine(Source, FString::Printf(
				TEXT("\tFTransferHolder(%s)"),
				*ValueParameterDeclaration(
					SourceCase,
					TEXT("InTarget"))));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(
				Source,
				TEXT("\t\tTarget = InTarget;"));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
		}
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
	}

	static FString SourceOperand(const FSourceCase& SourceCase)
	{
		return HasRetainedSource(SourceCase)
			? TEXT("Source")
			: CreationExpression(SourceCase, 7);
	}

	static FString RetainedSourceRead(const FSourceCase& SourceCase)
	{
		return HasRetainedSource(SourceCase)
			? TEXT("ReadTransferState(Source)")
			: TEXT("-1");
	}

	static FString RelationExpression(
		const FSourceCase& SourceCase,
		const FString& TargetExpression)
	{
		if (!HasRetainedSource(SourceCase))
		{
			return TEXT("2");
		}
		if (IsReferenceObject(SourceCase))
		{
			return FString::Printf(
				TEXT("Source == %s ? 1 : 0"),
				*TargetExpression);
		}
		return TEXT("0");
	}

	static void AppendSourceDeclaration(
		FString& Source,
		const FSourceCase& SourceCase)
	{
		using namespace AngelscriptNativeTestSupport;

		if (!HasRetainedSource(SourceCase))
		{
			return;
		}
		if (SourceCase.Route == ETransferSourceRoute::Return)
		{
			AppendGeneratedAsLine(Source, FString::Printf(
				TEXT("\t%s Source = MakeConstructorTransferSource();"),
				*TypeName(SourceCase)));
			return;
		}
		if (SourceCase.Route == ETransferSourceRoute::BaseView)
		{
			AppendGeneratedAsLine(
				Source,
				TEXT("\tFTransferDerived DynamicSource = FTransferDerived(7);"));
			AppendGeneratedAsLine(
				Source,
				TEXT("\tFTransferBase Source = DynamicSource;"));
			return;
		}
		if (IsValueObject(SourceCase))
		{
			AppendGeneratedAsLine(Source, FString::Printf(
				TEXT("\t%s Source(7);"),
				*TypeName(SourceCase)));
		}
		else
		{
			AppendGeneratedAsLine(Source, FString::Printf(
				TEXT("\t%s Source = %s;"),
				*TypeName(SourceCase),
				*CreationExpression(SourceCase, 7)));
		}
	}

	static void AppendDefaultTarget(
		FString& Source,
		const FSourceCase& SourceCase,
		const TCHAR* Name)
	{
		using namespace AngelscriptNativeTestSupport;

		if (IsValueObject(SourceCase))
		{
			AppendGeneratedAsLine(Source, FString::Printf(
				TEXT("\t%s %s;"),
				*TypeName(SourceCase),
				Name));
		}
		else
		{
			AppendGeneratedAsLine(Source, FString::Printf(
				TEXT("\t%s %s = %s;"),
				*TypeName(SourceCase),
				Name,
				*DefaultExpression(SourceCase)));
		}
	}

	static void AppendRecord(
		FString& Source,
		const int32 Stage,
		const FString& SourceExpression,
		const FString& TargetExpression,
		const FString& AuxiliaryExpression,
		const FString& Relation,
		const FString& DynamicExpression)
	{
		using namespace AngelscriptNativeTestSupport;

		AppendGeneratedAsLine(Source, FString::Printf(
			TEXT("\tRecordConstructorTransferState(%d, %s, %s, %s, %s, %s);"),
			Stage,
			*SourceExpression,
			*TargetExpression,
			*AuxiliaryExpression,
			*Relation,
			*DynamicExpression));
		AppendGeneratedAsLine(Source, FString::Printf(
			TEXT("\tRecordConstructorTransferBoundary(%d);"),
			Stage));
	}

	static void AppendStandardMutationSequence(
		FString& Source,
		const FSourceCase& SourceCase,
		const FString& TargetExpression,
		const FString& AuxiliaryExpression)
	{
		const FString ReadTarget = FString::Printf(
			TEXT("ReadTransferState(%s)"),
			*TargetExpression);
		const FString ReadAuxiliary = FString::Printf(
			TEXT("ReadTransferState(%s)"),
			*AuxiliaryExpression);
		const FString DynamicTarget = FString::Printf(
			TEXT("ReadTransferDynamicKind(%s)"),
			*TargetExpression);
		const FString Relation =
			RelationExpression(SourceCase, TargetExpression);
		AppendRecord(
			Source,
			0,
			RetainedSourceRead(SourceCase),
			ReadTarget,
			ReadAuxiliary,
			Relation,
			DynamicTarget);
		if (HasRetainedSource(SourceCase))
		{
			AngelscriptNativeTestSupport::AppendGeneratedAsLine(
				Source,
				TEXT("\tSetTransferState(Source, 23);"));
		}
		AppendRecord(
			Source,
			1,
			RetainedSourceRead(SourceCase),
			ReadTarget,
			ReadAuxiliary,
			Relation,
			DynamicTarget);
		AngelscriptNativeTestSupport::AppendGeneratedAsLine(
			Source,
			FString::Printf(
				TEXT("\tSetTransferState(%s, 31);"),
				*TargetExpression));
		AppendRecord(
			Source,
			2,
			RetainedSourceRead(SourceCase),
			ReadTarget,
			ReadAuxiliary,
			Relation,
			DynamicTarget);
	}

	static void AppendCopyDeclarationWorkflow(
		FString& Source,
		const FSourceCase& SourceCase)
	{
		using namespace AngelscriptNativeTestSupport;

		AppendSourceDeclaration(Source, SourceCase);
		if (IsValueObject(SourceCase))
		{
			AppendGeneratedAsLine(Source, FString::Printf(
				TEXT("\t%s Target(%s);"),
				*TypeName(SourceCase),
				*SourceOperand(SourceCase)));
		}
		else
		{
			AppendGeneratedAsLine(Source, FString::Printf(
				TEXT("\t%s Target = %s;"),
				*TypeName(SourceCase),
				*SourceOperand(SourceCase)));
		}
		AppendStandardMutationSequence(
			Source,
			SourceCase,
			TEXT("Target"),
			TEXT("Target"));
	}

	static void AppendFieldConstructorWorkflow(
		FString& Source,
		const FSourceCase& SourceCase)
	{
		using namespace AngelscriptNativeTestSupport;

		AppendSourceDeclaration(Source, SourceCase);
		AppendGeneratedAsLine(Source, FString::Printf(
			TEXT("\tFTransferHolder Container(%s);"),
			*SourceOperand(SourceCase)));
		AppendStandardMutationSequence(
			Source,
			SourceCase,
			TEXT("Container.Target"),
			TEXT("Container.Target"));
	}

	static void AppendAssignmentWorkflow(
		FString& Source,
		const FSourceCase& SourceCase)
	{
		using namespace AngelscriptNativeTestSupport;

		AppendSourceDeclaration(Source, SourceCase);
		AppendDefaultTarget(Source, SourceCase, TEXT("Target"));
		AppendGeneratedAsLine(Source, FString::Printf(
			TEXT("\tTarget = %s;"),
			*SourceOperand(SourceCase)));
		AppendStandardMutationSequence(
			Source,
			SourceCase,
			TEXT("Target"),
			TEXT("Target"));
	}

	static void AppendFieldAssignmentWorkflow(
		FString& Source,
		const FSourceCase& SourceCase)
	{
		using namespace AngelscriptNativeTestSupport;

		AppendSourceDeclaration(Source, SourceCase);
		AppendGeneratedAsLine(Source, TEXT("\tFTransferHolder Container;"));
		AppendGeneratedAsLine(Source, FString::Printf(
			TEXT("\tContainer.Target = %s;"),
			*SourceOperand(SourceCase)));
		AppendStandardMutationSequence(
			Source,
			SourceCase,
			TEXT("Container.Target"),
			TEXT("Container.Target"));
	}

	static void AppendArgumentHelper(
		FString& Source,
		const FSourceCase& SourceCase)
	{
		using namespace AngelscriptNativeTestSupport;

		const FString TargetName = IsValueObject(SourceCase)
			? TEXT("MutableTarget")
			: TEXT("Target");
		AppendGeneratedAsLine(Source, FString::Printf(
			TEXT("int ExerciseConstructorTransferArgument(%s)"),
			*ValueParameterDeclaration(SourceCase, TEXT("Target"))));
		AppendGeneratedAsLine(Source, TEXT("{"));
		if (IsValueObject(SourceCase))
		{
			AppendGeneratedAsLine(Source, FString::Printf(
				TEXT("\t%s MutableTarget(Target);"),
				*TypeName(SourceCase)));
		}
		AppendRecord(
			Source,
			0,
			TEXT("-1"),
			FString::Printf(TEXT("ReadTransferState(%s)"), *TargetName),
			FString::Printf(TEXT("ReadTransferState(%s)"), *TargetName),
			TEXT("3"),
			FString::Printf(TEXT("ReadTransferDynamicKind(%s)"), *TargetName));
		AppendGeneratedAsLine(Source, FString::Printf(
			TEXT("\tSetTransferState(%s, 23);"),
			*TargetName));
		AppendRecord(
			Source,
			1,
			TEXT("-1"),
			FString::Printf(TEXT("ReadTransferState(%s)"), *TargetName),
			FString::Printf(TEXT("ReadTransferState(%s)"), *TargetName),
			TEXT("3"),
			FString::Printf(TEXT("ReadTransferDynamicKind(%s)"), *TargetName));
		AppendGeneratedAsLine(
			Source,
			FString::Printf(TEXT("\treturn ReadTransferState(%s);"), *TargetName));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
	}

	static void AppendArgumentWorkflow(
		FString& Source,
		const FSourceCase& SourceCase)
	{
		using namespace AngelscriptNativeTestSupport;

		AppendSourceDeclaration(Source, SourceCase);
		AppendGeneratedAsLine(Source, FString::Printf(
			TEXT("\tint Result = ExerciseConstructorTransferArgument(%s);"),
			*SourceOperand(SourceCase)));
		AppendRecord(
			Source,
			2,
			RetainedSourceRead(SourceCase),
			TEXT("Result"),
			TEXT("Result"),
			HasRetainedSource(SourceCase)
				? (IsReferenceObject(SourceCase)
					? TEXT("1")
					: TEXT("0"))
				: TEXT("2"),
			FString::FromInt(SourceCase.DynamicMarker));
	}

	static void AppendReturnHelper(
		FString& Source,
		const FSourceCase& SourceCase)
	{
		using namespace AngelscriptNativeTestSupport;

		const FString ValueName = IsValueObject(SourceCase)
			? TEXT("MutableValue")
			: TEXT("Value");
		AppendGeneratedAsLine(Source, FString::Printf(
			TEXT("%s ForwardConstructorTransfer(%s)"),
			*TypeName(SourceCase),
			*ValueParameterDeclaration(SourceCase, TEXT("Value"))));
		AppendGeneratedAsLine(Source, TEXT("{"));
		if (IsValueObject(SourceCase))
		{
			AppendGeneratedAsLine(Source, FString::Printf(
				TEXT("\t%s MutableValue(Value);"),
				*TypeName(SourceCase)));
		}
		AppendGeneratedAsLine(Source, FString::Printf(
			TEXT("\tSetTransferState(%s, 23);"),
			*ValueName));
		AppendRecord(
			Source,
			0,
			FString::Printf(TEXT("ReadTransferState(%s)"), *ValueName),
			TEXT("-1"),
			TEXT("-1"),
			TEXT("4"),
			FString::Printf(TEXT("ReadTransferDynamicKind(%s)"), *ValueName));
		AppendGeneratedAsLine(Source, FString::Printf(
			TEXT("\treturn %s;"),
			*ValueName));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
	}

	static void AppendReturnWorkflow(
		FString& Source,
		const FSourceCase& SourceCase)
	{
		using namespace AngelscriptNativeTestSupport;

		AppendSourceDeclaration(Source, SourceCase);
		AppendGeneratedAsLine(Source, FString::Printf(
			TEXT("\t%s Target = ForwardConstructorTransfer(%s);"),
			*TypeName(SourceCase),
			*SourceOperand(SourceCase)));
		const FString Relation =
			RelationExpression(SourceCase, TEXT("Target"));
		AppendRecord(
			Source,
			1,
			RetainedSourceRead(SourceCase),
			TEXT("ReadTransferState(Target)"),
			TEXT("ReadTransferState(Target)"),
			Relation,
			TEXT("ReadTransferDynamicKind(Target)"));
		AppendGeneratedAsLine(Source, TEXT("\tSetTransferState(Target, 31);"));
		AppendRecord(
			Source,
			2,
			RetainedSourceRead(SourceCase),
			TEXT("ReadTransferState(Target)"),
			TEXT("ReadTransferState(Target)"),
			Relation,
			TEXT("ReadTransferDynamicKind(Target)"));
	}

	static void AppendSelfAssignmentWorkflow(
		FString& Source,
		const FSourceCase& SourceCase)
	{
		using namespace AngelscriptNativeTestSupport;

		AppendSourceDeclaration(Source, SourceCase);
		if (IsValueObject(SourceCase))
		{
			AppendGeneratedAsLine(Source, FString::Printf(
				TEXT("\t%s Target(%s);"),
				*TypeName(SourceCase),
				*SourceOperand(SourceCase)));
		}
		else
		{
			AppendGeneratedAsLine(Source, FString::Printf(
				TEXT("\t%s Target = %s;"),
				*TypeName(SourceCase),
				*SourceOperand(SourceCase)));
		}
		AppendGeneratedAsLine(Source, TEXT("\tTarget = Target;"));
		AppendStandardMutationSequence(
			Source,
			SourceCase,
			TEXT("Target"),
			TEXT("Target"));
	}

	static void AppendChainedAssignmentWorkflow(
		FString& Source,
		const FSourceCase& SourceCase)
	{
		using namespace AngelscriptNativeTestSupport;

		AppendSourceDeclaration(Source, SourceCase);
		AppendDefaultTarget(Source, SourceCase, TEXT("First"));
		AppendDefaultTarget(Source, SourceCase, TEXT("Second"));
		AppendGeneratedAsLine(Source, FString::Printf(
			TEXT("\tFirst = Second = %s;"),
			*SourceOperand(SourceCase)));
		AppendStandardMutationSequence(
			Source,
			SourceCase,
			TEXT("First"),
			TEXT("Second"));
	}

	static void AppendEntryFunction(
		FString& Source,
		const FSourceCase& SourceCase,
		const FWorkflowCase& WorkflowCase)
	{
		using namespace AngelscriptNativeTestSupport;

		AppendGeneratedAsLine(Source, TEXT("int RunConstructorTransfer()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		if (IsWorkflow(WorkflowCase, "copy_declaration_local"))
		{
			AppendCopyDeclarationWorkflow(Source, SourceCase);
		}
		else if (
			IsWorkflow(WorkflowCase, "field_constructor_transfer"))
		{
			AppendFieldConstructorWorkflow(Source, SourceCase);
		}
		else if (IsWorkflow(WorkflowCase, "assignment_local"))
		{
			AppendAssignmentWorkflow(Source, SourceCase);
		}
		else if (
			IsWorkflow(
				WorkflowCase,
				"field_assignment_after_default"))
		{
			AppendFieldAssignmentWorkflow(Source, SourceCase);
		}
		else if (IsWorkflow(WorkflowCase, "argument_transfer"))
		{
			AppendArgumentWorkflow(Source, SourceCase);
		}
		else if (IsWorkflow(WorkflowCase, "return_transfer"))
		{
			AppendReturnWorkflow(Source, SourceCase);
		}
		else if (IsWorkflow(WorkflowCase, "self_assignment"))
		{
			AppendSelfAssignmentWorkflow(Source, SourceCase);
		}
		else
		{
			AppendChainedAssignmentWorkflow(Source, SourceCase);
		}
		AppendGeneratedAsLine(Source, TEXT("\treturn 1;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(
			Source,
			TEXT("int RunConstructorTransferRecovery()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\treturn 97;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
	}

	static FString BuildConstructorTransferSource(
		const FSourceCase& SourceCase,
		const FWorkflowCase& WorkflowCase)
	{
		FString Source;
		AppendTypeDeclarations(Source, SourceCase);
		AppendStateHelpers(Source, SourceCase);
		AppendSourceFactory(Source, SourceCase);
		AppendHolderType(Source, SourceCase, WorkflowCase);
		if (IsWorkflow(WorkflowCase, "argument_transfer"))
		{
			AppendArgumentHelper(Source, SourceCase);
		}
		else if (IsWorkflow(WorkflowCase, "return_transfer"))
		{
			AppendReturnHelper(Source, SourceCase);
		}
		AppendEntryFunction(Source, SourceCase, WorkflowCase);
		return Source;
	}

	static int CompileAndReport(
		FAutomationTestBase& Test,
		asIScriptEngine& ScriptEngine,
		const FString& SourceId,
		const FString& ModuleName,
		const FString& Source,
		asIScriptModule*& OutModule)
	{
		using namespace AngelscriptNativeTestSupport;

		PrintGeneratedAsSource(Test, SourceId, ModuleName, Source);
		const FTCHARToUTF8 ModuleNameUtf8(*ModuleName);
		const FTCHARToUTF8 SourceUtf8(*Source);
		return CompileNativeModule(
			&ScriptEngine,
			ModuleNameUtf8.Get(),
			SourceUtf8.Get(),
			OutModule);
	}

	static FString DescribeFunctionBytecode(asIScriptFunction& Function)
	{
		asUINT BytecodeLength = 0;
		const asDWORD* const Bytecode = Function.GetByteCode(&BytecodeLength);
		if (Bytecode == nullptr || BytecodeLength == 0)
		{
			return TEXT("<empty>");
		}

		TArray<FString> Instructions;
		asUINT DwordIndex = 0;
		while (DwordIndex < BytecodeLength)
		{
			const asEBCInstr Opcode = static_cast<asEBCInstr>(
				*reinterpret_cast<const asBYTE*>(&Bytecode[DwordIndex]));
			if (static_cast<int32>(Opcode) > static_cast<int32>(asBC_MAXBYTECODE))
			{
				Instructions.Add(FString::Printf(
					TEXT("%u:<invalid=%u>"),
					DwordIndex,
					static_cast<uint32>(static_cast<asBYTE>(Opcode))));
				break;
			}

			const int32 InstructionSize = asBCTypeSize[asBCInfo[Opcode].type];
			if (InstructionSize <= 0
				|| DwordIndex + static_cast<asUINT>(InstructionSize)
					> BytecodeLength)
			{
				Instructions.Add(FString::Printf(
					TEXT("%u:%hs<size=%d outside=%u>"),
					DwordIndex,
					asBCInfo[Opcode].name,
					InstructionSize,
					BytecodeLength));
				break;
			}

			FString Words;
			for (int32 WordIndex = 0;
				WordIndex < InstructionSize;
				++WordIndex)
			{
				if (WordIndex == 0)
				{
					Words += FString::Printf(
						TEXT("%08x"),
						Bytecode[DwordIndex + static_cast<asUINT>(WordIndex)]);
				}
				else
				{
					Words += FString::Printf(
						TEXT(",%08x"),
						Bytecode[DwordIndex + static_cast<asUINT>(WordIndex)]);
				}
			}
			Instructions.Add(FString::Printf(
				TEXT("%u:%hs[%s]"),
				DwordIndex,
				asBCInfo[Opcode].name,
				*Words));
			DwordIndex += static_cast<asUINT>(InstructionSize);
		}
		return FString::Join(Instructions, TEXT("; "));
	}

	void ReportNativeReferenceTemporarySelfBytecode(
		const FNativeCaseContext& Case,
		const FSourceCase& SourceCase,
		const FWorkflowCase& WorkflowCase,
		asIScriptFunction& Entry)
	{
		if (!IsNativeReferenceTemporarySelfAssignment(
			SourceCase,
			WorkflowCase))
		{
			return;
		}

		UE_LOG(LogTemp, Display,
			TEXT("[AS-CELL-BYTECODE] Id=%s Subject=native-reference-temporary-self Instructions={%s}"),
			*Case.GetId(),
			*DescribeFunctionBytecode(Entry));
	}

	void ReportScriptReferenceLocalCopyBytecode(
		const FNativeCaseContext& Case,
		const FSourceCase& SourceCase,
		const FWorkflowCase& WorkflowCase,
		asIScriptFunction& Entry)
	{
		if (!IsScriptReferenceLocalCopyDeclaration(SourceCase, WorkflowCase))
		{
			return;
		}

		UE_LOG(LogTemp, Display,
			TEXT("[AS-CELL-BYTECODE] Id=%s Subject=script-reference-local-copy Instructions={%s}"),
			*Case.GetId(),
			*DescribeFunctionBytecode(Entry));
	}

	void ReportNativeReferenceTemporarySelfLifecycle(
		const FNativeCaseContext& Case,
		const FSourceCase& SourceCase,
		const FWorkflowCase& WorkflowCase,
		const TCHAR* Phase,
		const FNativeLifecycleRecorder& Lifecycle)
	{
		if (!IsNativeReferenceTemporarySelfAssignment(
			SourceCase,
			WorkflowCase))
		{
			return;
		}

		UE_LOG(LogTemp, Display,
			TEXT("[AS-CELL-LIFECYCLE] Id=%s Subject=native-reference-temporary-self Phase=%s Live=%d Entries=[%s]"),
			*Case.GetId(),
			Phase,
			Lifecycle.GetLiveObjectCount(),
			*CollectNativeLifecycleEntries(Lifecycle));
	}

	void ReportCellPhase(
		const FNativeCaseContext& Case,
		const TCHAR* Phase,
		const FNativeLifecycleRecorder& Lifecycle)
	{
		UE_LOG(LogTemp, Display,
			TEXT("[AS-CELL-PHASE] Id=%s Phase=%s Live=%d"),
			*Case.GetId(),
			Phase,
			Lifecycle.GetLiveObjectCount());
	}

	static int32 ExpectedRelation(
		const FSourceCase& SourceCase)
	{
		if (!HasRetainedSource(SourceCase))
		{
			return 2;
		}
		return IsReferenceObject(SourceCase) ? 1 : 0;
	}

	static FConstructorTransferObservation ExpectedObservation(
		const FSourceCase& SourceCase,
		const FWorkflowCase& WorkflowCase,
		const int32 Stage)
	{
		const int32 Initial = StateValue(SourceCase, 7);
		const int32 MutatedSource = StateValue(SourceCase, 23);
		const int32 MutatedTarget = StateValue(SourceCase, 31);
		const bool bRetained = HasRetainedSource(SourceCase);
		const bool bReference = IsReferenceObject(SourceCase);

		FConstructorTransferObservation Expected;
		Expected.Stage = Stage;
		Expected.DynamicMarker = SourceCase.DynamicMarker;
		if (IsWorkflow(WorkflowCase, "argument_transfer"))
		{
			if (Stage == 0)
			{
				Expected.SourceValue = -1;
				Expected.TargetValue = Initial;
				Expected.AuxiliaryValue = Initial;
				Expected.Relation = 3;
			}
			else if (Stage == 1)
			{
				Expected.SourceValue = -1;
				Expected.TargetValue = MutatedSource;
				Expected.AuxiliaryValue = MutatedSource;
				Expected.Relation = 3;
			}
			else
			{
				Expected.SourceValue = bRetained
					? (bReference ? MutatedSource : Initial)
					: -1;
				Expected.TargetValue = MutatedSource;
				Expected.AuxiliaryValue = MutatedSource;
				Expected.Relation = ExpectedRelation(SourceCase);
			}
			return Expected;
		}
		if (IsWorkflow(WorkflowCase, "return_transfer"))
		{
			if (Stage == 0)
			{
				Expected.SourceValue = MutatedSource;
				Expected.TargetValue = -1;
				Expected.AuxiliaryValue = -1;
				Expected.Relation = 4;
			}
			else if (Stage == 1)
			{
				Expected.SourceValue = bRetained
					? (bReference ? MutatedSource : Initial)
					: -1;
				Expected.TargetValue = MutatedSource;
				Expected.AuxiliaryValue = MutatedSource;
				Expected.Relation = ExpectedRelation(SourceCase);
			}
			else
			{
				Expected.SourceValue = bRetained
					? (bReference ? MutatedTarget : Initial)
					: -1;
				Expected.TargetValue = MutatedTarget;
				Expected.AuxiliaryValue = MutatedTarget;
				Expected.Relation = ExpectedRelation(SourceCase);
			}
			return Expected;
		}

		Expected.Relation = ExpectedRelation(SourceCase);
		if (Stage == 0)
		{
			Expected.SourceValue = bRetained ? Initial : -1;
			Expected.TargetValue = Initial;
			Expected.AuxiliaryValue = Initial;
		}
		else if (Stage == 1)
		{
			Expected.SourceValue = bRetained ? MutatedSource : -1;
			Expected.TargetValue =
				bRetained && bReference ? MutatedSource : Initial;
			Expected.AuxiliaryValue = Expected.TargetValue;
		}
		else
		{
			Expected.SourceValue = bRetained
				? (bReference ? MutatedTarget : MutatedSource)
				: -1;
			Expected.TargetValue = MutatedTarget;
			Expected.AuxiliaryValue =
				IsWorkflow(WorkflowCase, "chained_assignment")
					? (bReference ? MutatedTarget : Initial)
					: MutatedTarget;
		}
		return Expected;
	}

	void VerifyMetadata(
		const FNativeCaseContext& Case,
		const FSourceCase& SourceCase,
		const FWorkflowCase& WorkflowCase,
		asIScriptEngine& ScriptEngine,
		asIScriptModule& Module)
	{
		asITypeInfo* Type = nullptr;
		if (SourceCase.ObjectKind == ETransferObjectKind::NativeValue
			|| SourceCase.ObjectKind
				== ETransferObjectKind::NativeReference)
		{
			const FTCHARToUTF8 TypeNameUtf8(*TypeName(SourceCase));
			Type = ScriptEngine.GetTypeInfoByDecl(TypeNameUtf8.Get());
		}
		else
		{
			const FTCHARToUTF8 TypeNameUtf8(*TypeName(SourceCase));
			Type = Module.GetTypeInfoByName(TypeNameUtf8.Get());
		}
		ASSERT_THAT(IsNotNull(Type,
			*Case.Describe(TEXT("constructor-transfer source type should publish metadata"))));
		if (Type != nullptr)
		{
			ASSERT_THAT(AreEqual(
				IsValueObject(SourceCase),
				(Type->GetFlags() & asOBJ_VALUE) != 0,
				*Case.Describe(TEXT("constructor-transfer metadata should preserve value/reference semantics"))));
			if (SourceCase.ObjectKind == ETransferObjectKind::ScriptReference
				|| SourceCase.ObjectKind == ETransferObjectKind::DerivedReference)
			{
				ASSERT_THAT(IsTrue(
					(Type->GetFlags() & (asOBJ_SCRIPT_OBJECT | asOBJ_REF | asOBJ_NOCOUNT))
						== (asOBJ_SCRIPT_OBJECT | asOBJ_REF | asOBJ_NOCOUNT),
					*Case.Describe(TEXT("raw script-reference lifecycle coverage should exercise the no-count script-object contract"))));
				ASSERT_THAT(IsNull(Type->GetUserData(),
					*Case.Describe(TEXT("raw script-reference lifecycle coverage should exclude UASClass-owned types"))));
			}
		}

		if (SourceCase.Route == ETransferSourceRoute::BaseView)
		{
			asITypeInfo* const Base =
				Module.GetTypeInfoByName("FTransferBase");
			asITypeInfo* const Derived =
				Module.GetTypeInfoByName("FTransferDerived");
			ASSERT_THAT(IsNotNull(Base,
				*Case.Describe(TEXT("base-view transfer should publish its base type"))));
			ASSERT_THAT(IsNotNull(Derived,
				*Case.Describe(TEXT("base-view transfer should publish its dynamic derived type"))));
			if (Base != nullptr && Derived != nullptr)
			{
				ASSERT_THAT(AreEqual(Base, Derived->GetBaseType(),
					*Case.Describe(TEXT("base-view transfer should retain the exact inheritance edge"))));
			}
		}

		if (IsWorkflow(WorkflowCase, "field_constructor_transfer")
			|| IsWorkflow(
				WorkflowCase,
				"field_assignment_after_default"))
		{
			asITypeInfo* const Holder =
				Module.GetTypeInfoByName("FTransferHolder");
			ASSERT_THAT(IsNotNull(Holder,
				*Case.Describe(TEXT("field transfer should publish its holder type"))));
			if (Holder != nullptr)
			{
				ASSERT_THAT(AreEqual(1, Holder->GetPropertyCount(),
					*Case.Describe(TEXT("field transfer holder should own one target field"))));
			}
		}

		asIScriptFunction* const Entry =
			Module.GetFunctionByDecl("int RunConstructorTransfer()");
		asIScriptFunction* const Recovery =
			Module.GetFunctionByDecl(
				"int RunConstructorTransferRecovery()");
		ASSERT_THAT(IsNotNull(Entry,
			*Case.Describe(TEXT("constructor-transfer module should publish its exact entry"))));
		ASSERT_THAT(IsNotNull(Recovery,
			*Case.Describe(TEXT("constructor-transfer module should publish its recovery entry"))));
	}

	void VerifyObservations(
		const FNativeCaseContext& InitialCase,
		const FNativeCaseContext& SourceCaseId,
		const FNativeCaseContext& MutationCase,
		const FSourceCase& SourceCase,
		const FWorkflowCase& WorkflowCase,
		const FConstructorTransferState& State)
	{
		ASSERT_THAT(AreEqual(3, State.Observations.Num(),
			*InitialCase.Describe(TEXT("constructor-transfer workflow should publish three ordered observations"))));
		if (State.Observations.Num() != 3)
		{
			return;
		}

		const FNativeCaseContext* Cases[] =
		{
			&InitialCase,
			&SourceCaseId,
			&MutationCase,
		};
		for (int32 Stage = 0; Stage < 3; ++Stage)
		{
			const FConstructorTransferObservation Expected =
				ExpectedObservation(SourceCase, WorkflowCase, Stage);
			const FConstructorTransferObservation& Actual =
				State.Observations[Stage];
			const FNativeCaseContext& Case = *Cases[Stage];
			ASSERT_THAT(AreEqual(Expected.Stage, Actual.Stage,
				*Case.Describe(TEXT("constructor-transfer observation should preserve stage order"))));
			ASSERT_THAT(AreEqual(
				Expected.SourceValue,
				Actual.SourceValue,
				*Case.Describe(TEXT("constructor-transfer source or temporary state should match"))));
			ASSERT_THAT(AreEqual(
				Expected.TargetValue,
				Actual.TargetValue,
				*Case.Describe(TEXT("constructor-transfer target state should match"))));
			ASSERT_THAT(AreEqual(
				Expected.AuxiliaryValue,
				Actual.AuxiliaryValue,
				*Case.Describe(TEXT("constructor-transfer secondary destination should match"))));
			ASSERT_THAT(AreEqual(
				Expected.Relation,
				Actual.Relation,
				*Case.Describe(TEXT("constructor-transfer value independence or reference identity should match"))));
			ASSERT_THAT(AreEqual(
				Expected.DynamicMarker,
				Actual.DynamicMarker,
				*Case.Describe(TEXT("constructor-transfer static/base view should preserve dynamic kind"))));
		}
	}

	void VerifySnapshots(
		const FNativeCaseContext& Case,
		const FSourceCase& SourceCase,
		const FWorkflowCase& WorkflowCase,
		const FConstructorTransferState& State)
	{
		ASSERT_THAT(AreEqual(3, State.Snapshots.Num(),
			*Case.Describe(TEXT("constructor-transfer workflow should publish three lifecycle boundaries"))));
		if (State.Snapshots.Num() != 3)
		{
			return;
		}
		for (int32 Stage = 0; Stage < 3; ++Stage)
		{
			ASSERT_THAT(AreEqual(Stage, State.Snapshots[Stage].Stage,
				*Case.Describe(TEXT("constructor-transfer lifecycle boundary should preserve stage order"))));
		}
		ASSERT_THAT(AreEqual(
			State.Snapshots[1].Constructions,
			State.Snapshots[2].Constructions,
			*Case.Describe(TEXT("target mutation should construct no replacement storage"))));
		ASSERT_THAT(AreEqual(
			State.Snapshots[1].Copies,
			State.Snapshots[2].Copies,
			*Case.Describe(TEXT("target mutation should create no hidden copy"))));
		ASSERT_THAT(AreEqual(
			State.Snapshots[1].Assignments,
			State.Snapshots[2].Assignments,
			*Case.Describe(TEXT("scalar/nested mutation should not assign the owner object"))));
		if (SourceCase.Route == ETransferSourceRoute::Temporary
			&& IsValueObject(SourceCase))
		{
			if (IsWorkflow(WorkflowCase, "argument_transfer"))
			{
				ASSERT_THAT(IsTrue(
					State.Snapshots[2].Destructions
						> State.Snapshots[1].Destructions,
					*Case.Describe(TEXT("temporary argument storage should retire before the outer observation"))));
			}
			else if (IsWorkflow(WorkflowCase, "return_transfer"))
			{
				ASSERT_THAT(IsTrue(
					State.Snapshots[1].Destructions
						> State.Snapshots[0].Destructions,
					*Case.Describe(TEXT("temporary return parameter should retire after materializing the target"))));
			}
			else
			{
				ASSERT_THAT(IsTrue(
					State.Snapshots[0].Destructions > 0,
					*Case.Describe(TEXT("direct temporary source should retire before the first retained-target boundary"))));
			}
		}
	}

	void VerifyFinalLifecycle(
		const FNativeCaseContext& Case,
		const FSourceCase& SourceCase,
		const FWorkflowCase& WorkflowCase,
		const FNativeLifecycleRecorder& Lifecycle)
	{
		ASSERT_THAT(AreEqual(0, Lifecycle.GetLiveObjectCount(),
			*Case.Describe(TEXT("constructor-transfer workflow should leave no tracked object alive"))));
		const int32 ConstructionCount =
			Lifecycle.Num(ENativeLifecycleEvent::DefaultConstruct)
			+ Lifecycle.Num(ENativeLifecycleEvent::ValueConstruct)
			+ Lifecycle.Num(ENativeLifecycleEvent::CopyConstruct);
		ASSERT_THAT(IsTrue(ConstructionCount > 0,
			*Case.Describe(TEXT("constructor-transfer workflow should construct real tracked storage"))));
		ASSERT_THAT(AreEqual(
			ConstructionCount,
			Lifecycle.Num(ENativeLifecycleEvent::Destruct),
			*Case.Describe(TEXT("constructor-transfer workflow should destroy each storage identity once"))));

		if (IsValueObject(SourceCase))
		{
			if (IsWorkflow(WorkflowCase, "copy_declaration_local")
				|| IsWorkflow(WorkflowCase, "self_assignment")
				|| IsWorkflow(WorkflowCase, "argument_transfer")
				|| IsWorkflow(WorkflowCase, "return_transfer"))
			{
				ASSERT_THAT(IsTrue(
					Lifecycle.Num(
						ENativeLifecycleEvent::CopyConstruct) > 0,
					*Case.Describe(TEXT("value copy/argument/return workflow should invoke copy construction"))));
			}
			if (IsWorkflow(WorkflowCase, "assignment_local")
				|| IsWorkflow(
					WorkflowCase,
					"field_constructor_transfer")
				|| IsWorkflow(
					WorkflowCase,
					"field_assignment_after_default")
				|| IsWorkflow(WorkflowCase, "self_assignment")
				|| IsWorkflow(WorkflowCase, "chained_assignment"))
			{
				ASSERT_THAT(IsTrue(
					Lifecycle.Num(ENativeLifecycleEvent::Assign) > 0,
					*Case.Describe(TEXT("value assignment workflow should invoke assignment behavior"))));
			}
		}
		if (SourceCase.ObjectKind
			== ETransferObjectKind::NativeReference)
		{
			ASSERT_THAT(IsTrue(
				Lifecycle.Num(ENativeLifecycleEvent::AddRef) > 0,
				*Case.Describe(TEXT("native-reference transfer should retain at least one alias"))));
			ASSERT_THAT(IsTrue(
				Lifecycle.Num(ENativeLifecycleEvent::Release) > 0,
				*Case.Describe(TEXT("native-reference transfer should release every retained alias"))));
		}

		TSet<int32> ConstructedIds;
		TSet<int32> DestructedIds;
		for (const FNativeLifecycleEntry& Entry : Lifecycle.GetEntries())
		{
			if (Entry.Event == ENativeLifecycleEvent::DefaultConstruct
				|| Entry.Event
					== ENativeLifecycleEvent::ValueConstruct
				|| Entry.Event
					== ENativeLifecycleEvent::CopyConstruct)
			{
				ConstructedIds.Add(Entry.ObjectId);
			}
			else if (Entry.Event == ENativeLifecycleEvent::Destruct)
			{
				ASSERT_THAT(IsTrue(
					ConstructedIds.Contains(Entry.ObjectId),
					*Case.Describe(TEXT("constructor-transfer destructor should identify constructed storage"))));
				ASSERT_THAT(IsFalse(
					DestructedIds.Contains(Entry.ObjectId),
					*Case.Describe(TEXT("constructor-transfer storage should not be destroyed twice"))));
				DestructedIds.Add(Entry.ObjectId);
			}
		}
		ASSERT_THAT(AreEqual(
			ConstructedIds.Num(),
			DestructedIds.Num(),
			*Case.Describe(TEXT("constructor-transfer lifecycle identities should balance"))));
	}

	void ExecuteCell(
		const TStaticArray<FNativeCaseContext, 4>& Cases,
		const FSourceCase& SourceCase,
		const FWorkflowCase& WorkflowCase,
		asIScriptEngine& ScriptEngine,
		asIScriptModule& Module,
		FConstructorTransferState& State,
		FNativeLifecycleRecorder& Lifecycle)
	{
		using namespace AngelscriptNativeTestSupport;

		VerifyMetadata(
			Cases[0],
			SourceCase,
			WorkflowCase,
			ScriptEngine,
			Module);
		asIScriptFunction* const Entry =
			Module.GetFunctionByDecl("int RunConstructorTransfer()");
		asIScriptFunction* const Recovery =
			Module.GetFunctionByDecl(
				"int RunConstructorTransferRecovery()");
		if (Entry == nullptr || Recovery == nullptr)
		{
			return;
		}

		asIScriptContext* const Context = ScriptEngine.CreateContext();
		ASSERT_THAT(IsNotNull(Context,
			*Cases[0].Describe(TEXT("constructor-transfer workflow should create a reusable context"))));
		if (Context == nullptr)
		{
			return;
		}
		ASSERT_THAT(AreEqual(
			static_cast<int32>(asEXECUTION_FINISHED),
			PrepareAndExecute(Context, Entry),
			*Cases[0].Describe(TEXT("constructor-transfer workflow should finish"))));
		ASSERT_THAT(AreEqual(
			1,
			static_cast<int32>(Context->GetReturnDWord()),
			*Cases[0].Describe(TEXT("constructor-transfer workflow should return its success sentinel"))));
		ASSERT_THAT(AreEqual(
			asSUCCESS,
			Context->Unprepare(),
			*Cases[3].Describe(TEXT("constructor-transfer context should release workflow storage"))));

		VerifyObservations(
			Cases[0],
			Cases[1],
			Cases[2],
			SourceCase,
			WorkflowCase,
			State);
		VerifySnapshots(
			Cases[3],
			SourceCase,
			WorkflowCase,
			State);
		VerifyFinalLifecycle(
			Cases[3],
			SourceCase,
			WorkflowCase,
			Lifecycle);

		const int32 ObservationsBeforeRecovery =
			State.Observations.Num();
		const int32 SnapshotsBeforeRecovery =
			State.Snapshots.Num();
		ASSERT_THAT(IsTrue(Context->Prepare(Recovery) >= 0,
			*Cases[3].Describe(TEXT("constructor-transfer context should prepare recovery"))));
		ASSERT_THAT(AreEqual(
			static_cast<int32>(asEXECUTION_FINISHED),
			Context->Execute(),
			*Cases[3].Describe(TEXT("constructor-transfer recovery should finish"))));
		ASSERT_THAT(AreEqual(
			97,
			static_cast<int32>(Context->GetReturnDWord()),
			*Cases[3].Describe(TEXT("constructor-transfer recovery should return its sentinel"))));
		ASSERT_THAT(AreEqual(
			ObservationsBeforeRecovery,
			State.Observations.Num(),
			*Cases[3].Describe(TEXT("constructor-transfer recovery should perform no transfer"))));
		ASSERT_THAT(AreEqual(
			SnapshotsBeforeRecovery,
			State.Snapshots.Num(),
			*Cases[3].Describe(TEXT("constructor-transfer recovery should publish no lifecycle boundary"))));
		ASSERT_THAT(AreEqual(
			asSUCCESS,
			Context->Unprepare(),
			*Cases[3].Describe(TEXT("constructor-transfer recovery should unprepare cleanly"))));
		Context->Release();
	}

	void RunCell(
		const FSourceCase& SourceCase,
		const FWorkflowCase& WorkflowCase)
	{
		using namespace AngelscriptNativeTestSupport;

		TStaticArray<FNativeCaseContext, 4> Cases =
		{
			FNativeCaseContext(MakeNativeCaseId(
				"LANG-CTOR-TRANSFER",
				{
					ANSI_TO_TCHAR(
						ObservationCases[0].CatalogName),
					ANSI_TO_TCHAR(SourceCase.CatalogName),
					ANSI_TO_TCHAR(WorkflowCase.CatalogName),
				})),
			FNativeCaseContext(MakeNativeCaseId(
				"LANG-CTOR-TRANSFER",
				{
					ANSI_TO_TCHAR(
						ObservationCases[1].CatalogName),
					ANSI_TO_TCHAR(SourceCase.CatalogName),
					ANSI_TO_TCHAR(WorkflowCase.CatalogName),
				})),
			FNativeCaseContext(MakeNativeCaseId(
				"LANG-CTOR-TRANSFER",
				{
					ANSI_TO_TCHAR(
						ObservationCases[2].CatalogName),
					ANSI_TO_TCHAR(SourceCase.CatalogName),
					ANSI_TO_TCHAR(WorkflowCase.CatalogName),
				})),
			FNativeCaseContext(MakeNativeCaseId(
				"LANG-CTOR-TRANSFER",
				{
					ANSI_TO_TCHAR(
						ObservationCases[3].CatalogName),
					ANSI_TO_TCHAR(SourceCase.CatalogName),
					ANSI_TO_TCHAR(WorkflowCase.CatalogName),
				})),
		};

		// Native reference callbacks retain this recorder until the raw engine
		// releases its final object. Construct it before the engine so the scope
		// exit destroys the engine while the callback target is still valid.
		FNativeLifecycleRecorder Lifecycle;
		FConstructorTransferState State;
		State.Reset(Lifecycle);

		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };
		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine,
			*Cases[0].Describe(TEXT("constructor-transfer cell should create a raw SDK engine"))));
		if (ScriptEngine == nullptr)
		{
			return;
		}
		ReportCellPhase(Cases[0], TEXT("engine-created"), Lifecycle);

		ASSERT_THAT(IsTrue(RegisterConstructorTransferBridge(
			*ScriptEngine,
			State),
			*Cases[0].Describe(TEXT("constructor-transfer cell should register its observation bridge"))));
		ASSERT_THAT(IsTrue(RegisterNativeCaseValue(
			*ScriptEngine,
			Lifecycle),
			*Cases[0].Describe(TEXT("constructor-transfer cell should register native values"))));
		ASSERT_THAT(IsTrue(RegisterNativeCaseReference(
			*ScriptEngine,
			&Lifecycle),
			*Cases[0].Describe(TEXT("constructor-transfer cell should register native references"))));
		ASSERT_THAT(IsTrue(RegisterNativeScriptLifecycleBridge(
			*ScriptEngine,
			Lifecycle),
			*Cases[0].Describe(TEXT("constructor-transfer cell should register script lifecycle callbacks"))));
		ReportCellPhase(Cases[0], TEXT("native-registration-complete"), Lifecycle);

		const FString ModuleName = FString::Printf(
			TEXT("ConstructorTransfer_%hs_%hs"),
			SourceCase.CatalogName,
			WorkflowCase.CatalogName);
		const FString Source = BuildConstructorTransferSource(
			SourceCase,
			WorkflowCase);
		Engine.ResetMessages();
		Lifecycle.Reset();
		State.Reset(Lifecycle);
		asIScriptModule* Module = nullptr;
		const int CompileResult = CompileAndReport(
			*TestRunner,
			*ScriptEngine,
			Cases[0].GetId(),
			ModuleName,
			Source,
			Module);
		if (CompileResult < 0)
		{
			TestRunner->AddInfo(FString::Printf(
				TEXT("[AS-CELL-COMPILE-DIAGNOSTICS] Id=%s Module=%s Result=%d%s%s"),
				*Cases[0].GetId(),
				*ModuleName,
				CompileResult,
				LINE_TERMINATOR,
				*Engine.GetMessagesText()));
		}
		ASSERT_THAT(IsTrue(CompileResult >= 0,
			*Cases[0].Describe(TEXT("constructor-transfer source should compile"))));
		ASSERT_THAT(IsNotNull(Module,
			*Cases[0].Describe(TEXT("constructor-transfer source should publish its module"))));
		if (Module != nullptr)
		{
			ReportCellPhase(Cases[0], TEXT("module-compiled"), Lifecycle);
			asIScriptFunction* const Entry = Module->GetFunctionByDecl(
				"int RunConstructorTransfer()");
			if (Entry != nullptr)
			{
				ReportNativeReferenceTemporarySelfBytecode(
					Cases[0],
					SourceCase,
					WorkflowCase,
					*Entry);
				ReportScriptReferenceLocalCopyBytecode(
					Cases[0],
					SourceCase,
					WorkflowCase,
					*Entry);
			}
			ExecuteCell(
				Cases,
				SourceCase,
				WorkflowCase,
				*ScriptEngine,
				*Module,
				State,
				Lifecycle);
			ReportCellPhase(Cases[3], TEXT("execution-unprepared"), Lifecycle);
			ReportNativeReferenceTemporarySelfLifecycle(
				Cases[3],
				SourceCase,
				WorkflowCase,
				TEXT("after_unprepare"),
				Lifecycle);
		}

		const FTCHARToUTF8 ModuleNameUtf8(*ModuleName);
		ScriptEngine->DiscardModule(ModuleNameUtf8.Get());
		ReportCellPhase(Cases[3], TEXT("module-discarded"), Lifecycle);
		ReportNativeReferenceTemporarySelfLifecycle(
			Cases[3],
			SourceCase,
			WorkflowCase,
			TEXT("after_discard"),
			Lifecycle);
		ASSERT_THAT(IsNull(ScriptEngine->GetModule(
			ModuleNameUtf8.Get(),
			asGM_ONLY_IF_EXISTS),
			*Cases[3].Describe(TEXT("constructor-transfer module should discard cleanly"))));
		ASSERT_THAT(AreEqual(0, Lifecycle.GetLiveObjectCount(),
			*Cases[3].Describe(TEXT("constructor-transfer discard should leave no live storage"))));

		Engine.Destroy();
		ReportCellPhase(Cases[3], TEXT("engine-destroyed"), Lifecycle);

		if (IsNativeReferenceTemporarySelfAssignment(
			SourceCase,
			WorkflowCase))
		{
			ReportNativeReferenceTemporarySelfLifecycle(
				Cases[3],
				SourceCase,
				WorkflowCase,
				TEXT("after_engine_destroy"),
				Lifecycle);
		}
	}

public:
	TEST_METHOD(SourcesByWorkflowAndObservation)
	{
		AS_NATIVE_PRODUCT("LANG-CTOR-TRANSFER",
			AngelscriptNativeTestSupport::ENativeEvidence::Compile
				| AngelscriptNativeTestSupport::ENativeEvidence::Runtime
				| AngelscriptNativeTestSupport::ENativeEvidence::Metadata
				| AngelscriptNativeTestSupport::ENativeEvidence::Lifecycle
				| AngelscriptNativeTestSupport::ENativeEvidence::Cleanup
				| AngelscriptNativeTestSupport::ENativeEvidence::Isolation);

		for (const FSourceCase& SourceCase : SourceCases)
		{
			for (const FWorkflowCase& WorkflowCase : WorkflowCases)
			{
				RunCell(SourceCase, WorkflowCase);
			}
		}
	}

};

#endif // WITH_ANGELSCRIPT_UNITTESTS
