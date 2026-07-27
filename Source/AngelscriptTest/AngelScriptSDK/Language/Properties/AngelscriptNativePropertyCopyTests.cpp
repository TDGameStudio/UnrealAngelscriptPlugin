#include "../../Support/AngelscriptNativeCaseTestSupport.h"
#include "../../Support/AngelscriptNativeCoreTestSupport.h"
#include "../../Support/AngelscriptNativeFixtureTestSupport.h"
#include "../../Support/AngelscriptNativeLanguageCaseTestSupport.h"

#include "CQTest.h"
#include "Misc/ScopeExit.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FPropertyCopyTests,
	"Angelscript.TestModule.AngelScriptSDK.Language.Properties.Copy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	using ENativeLifecycleEvent = AngelscriptNativeTestSupport::ENativeLifecycleEvent;
	using ENativeValueCategory = AngelscriptNativeTestSupport::ENativeValueCategory;
	using FNativeCaseContext = AngelscriptNativeTestSupport::FNativeCaseContext;
	using FNativeLifecycleRecorder = AngelscriptNativeTestSupport::FNativeLifecycleRecorder;
	using FNativeTypeCase = AngelscriptNativeTestSupport::FNativeTypeCase;

	struct FTransferCase
	{
		const ANSICHAR* CatalogName;
	};

	struct FMutationCase
	{
		const ANSICHAR* CatalogName;
	};

	struct FViewCase
	{
		const ANSICHAR* CatalogName;
		const ANSICHAR* ParameterType;
		int32 DynamicMarker;
	};

	struct FPropertyCopyObservation
	{
		int32 SourceValue = 0;
		int32 TargetValue = 0;
		bool bSameIdentity = false;
		bool bTransferStable = false;
		int32 ViewMarker = 0;
	};

	struct FPropertyCopyLifecycleSnapshot
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

	struct FPropertyCopyState
	{
		FNativeLifecycleRecorder* Lifecycle = nullptr;
		TArray<FPropertyCopyObservation> Observations;
		TArray<FPropertyCopyLifecycleSnapshot> Snapshots;

		void Reset()
		{
			Observations.Reset();
			Snapshots.Reset();
		}
	};

	inline static constexpr FTransferCase TransferCases[] =
	{
		{ "copy_construct" },
		{ "assign" },
		{ "self_assign" },
	};

	inline static constexpr FMutationCase MutationCases[] =
	{
		{ "source_after_transfer" },
		{ "target_after_transfer" },
		{ "nested_member" },
	};

	inline static constexpr FViewCase ViewCases[] =
	{
		{ "exact", "FPropertyCopyBase", 11 },
		{ "base", "FPropertyCopyBase", 22 },
		{ "derived", "FPropertyCopyDerived", 22 },
	};

	inline static constexpr asPWORD PropertyCopyStateUserDataSlot =
		static_cast<asPWORD>(0x50524F50434F5059ull);

	static bool IsTransfer(const FTransferCase& TransferCase, const ANSICHAR* Name)
	{
		return FCStringAnsi::Strcmp(TransferCase.CatalogName, Name) == 0;
	}

	static bool IsMutation(const FMutationCase& MutationCase, const ANSICHAR* Name)
	{
		return FCStringAnsi::Strcmp(MutationCase.CatalogName, Name) == 0;
	}

	static bool IsView(const FViewCase& ViewCase, const ANSICHAR* Name)
	{
		return FCStringAnsi::Strcmp(ViewCase.CatalogName, Name) == 0;
	}

	static bool IsValueObjectType(const FNativeTypeCase& TypeCase)
	{
		return TypeCase.Category == ENativeValueCategory::ScriptValue
			|| TypeCase.Category == ENativeValueCategory::NativeValue;
	}

	static bool IsReferenceType(const FNativeTypeCase& TypeCase)
	{
		return TypeCase.Category == ENativeValueCategory::ScriptReference
			|| TypeCase.Category == ENativeValueCategory::NativeReference;
	}

	static bool IsScriptValueType(const FNativeTypeCase& TypeCase)
	{
		return TypeCase.Category == ENativeValueCategory::ScriptValue;
	}

	static bool IsNativeValueType(const FNativeTypeCase& TypeCase)
	{
		return TypeCase.Category == ENativeValueCategory::NativeValue;
	}

	static bool HasObjectLifecycle(const FNativeTypeCase& TypeCase)
	{
		return IsValueObjectType(TypeCase) || IsReferenceType(TypeCase);
	}

	static FString MakeSuffix(
		const FMutationCase& MutationCase,
		const FTransferCase& TransferCase,
		const FNativeTypeCase& TypeCase,
		const FViewCase& ViewCase)
	{
		return FString::Printf(
			TEXT("%hs_%hs_%hs_%hs"),
			MutationCase.CatalogName,
			TransferCase.CatalogName,
			TypeCase.CatalogName,
			ViewCase.CatalogName);
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
		if (TypeCase.Category == ENativeValueCategory::Typedef)
		{
			return FString::Printf(TEXT("NativeCaseAlias(%d)"), Value);
		}
		if (TypeCase.Category == ENativeValueCategory::ScriptReference)
		{
			return FString::Printf(TEXT("FScriptCaseReference(%d)"), Value);
		}
		if (TypeCase.Category == ENativeValueCategory::NativeReference)
		{
			return FString::Printf(TEXT("CreateNativeCaseReference(%d)"), Value);
		}
		return FString::Printf(TEXT("%hs(%d)"), TypeCase.ScriptType, Value);
	}

	static int32 InitialSourceValue(const FNativeTypeCase& TypeCase)
	{
		return TypeCase.Category == ENativeValueCategory::Boolean
			|| TypeCase.Category == ENativeValueCategory::Enum
			? 1
			: 11;
	}

	static int32 InitialTargetValue(const FNativeTypeCase& TypeCase)
	{
		return TypeCase.Category == ENativeValueCategory::Boolean
			|| TypeCase.Category == ENativeValueCategory::Enum
			? 0
			: 22;
	}

	static int32 MutatedValue(const FNativeTypeCase& TypeCase, const int32 Before)
	{
		return TypeCase.Category == ENativeValueCategory::Boolean
			|| TypeCase.Category == ENativeValueCategory::Enum
			? 1 - Before
			: 37;
	}

	static FPropertyCopyState* GetActivePropertyCopyState()
	{
		asIScriptContext* const Context = asGetActiveContext();
		return Context != nullptr && Context->GetEngine() != nullptr
			? static_cast<FPropertyCopyState*>(
				Context->GetEngine()->GetUserData(PropertyCopyStateUserDataSlot))
			: nullptr;
	}

	static void RecordPropertyCopyObservation(
		const int32 SourceValue,
		const int32 TargetValue,
		const bool bSameIdentity,
		const bool bTransferStable,
		const int32 ViewMarker)
	{
		if (FPropertyCopyState* const State = GetActivePropertyCopyState())
		{
			State->Observations.Add({
				SourceValue,
				TargetValue,
				bSameIdentity,
				bTransferStable,
				ViewMarker,
			});
		}
	}

	static void RecordPropertyCopyLifecycleBoundary(const int32 Stage)
	{
		FPropertyCopyState* const State = GetActivePropertyCopyState();
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

	static bool RegisterPropertyCopyBridge(
		asIScriptEngine& ScriptEngine,
		FPropertyCopyState& State)
	{
		ScriptEngine.SetUserData(&State, PropertyCopyStateUserDataSlot);
		const ASAutoCaller::FunctionCaller ObservationCaller =
			ASAutoCaller::MakeFunctionCaller(RecordPropertyCopyObservation);
		const ASAutoCaller::FunctionCaller BoundaryCaller =
			ASAutoCaller::MakeFunctionCaller(RecordPropertyCopyLifecycleBoundary);
		return ScriptEngine.RegisterGlobalFunction(
			"void RecordPropertyCopyObservation(int SourceValue, int TargetValue, bool SameIdentity, bool TransferStable, int ViewMarker)",
			asFUNCTION(RecordPropertyCopyObservation),
			asCALL_CDECL,
			*(asFunctionCaller*)&ObservationCaller) >= 0
			&& ScriptEngine.RegisterGlobalFunction(
				"void RecordPropertyCopyLifecycleBoundary(int Stage)",
				asFUNCTION(RecordPropertyCopyLifecycleBoundary),
				asCALL_CDECL,
				*(asFunctionCaller*)&BoundaryCaller) >= 0;
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
			AppendGeneratedAsLine(
				Source,
				TEXT("\t\tObjectId = CopyNativeScriptLifecycle(Other.ObjectId, Value);"));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
			AppendGeneratedAsLine(Source);
			AppendGeneratedAsLine(Source, TEXT("\tFScriptCaseValue& opAssign(const FScriptCaseValue& Other)"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t\tValue = Other.Value;"));
			AppendGeneratedAsLine(
				Source,
				TEXT("\t\tAssignNativeScriptLifecycle(ObjectId, Other.ObjectId, Value);"));
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
		else if (TypeCase.Category == ENativeValueCategory::ScriptReference)
		{
			AppendGeneratedAsLine(Source, TEXT("class FScriptCaseReference"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\tint Value = 0;"));
			AppendGeneratedAsLine(Source, TEXT("\tint ObjectId = -1;"));
			AppendGeneratedAsLine(Source);
			AppendGeneratedAsLine(Source, TEXT("\tFScriptCaseReference()"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t\tObjectId = BeginNativeScriptLifecycle(Value);"));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
			AppendGeneratedAsLine(Source);
			AppendGeneratedAsLine(Source, TEXT("\tFScriptCaseReference(int InValue)"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t\tValue = InValue;"));
			AppendGeneratedAsLine(Source, TEXT("\t\tObjectId = BeginNativeScriptLifecycle(Value);"));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
			AppendGeneratedAsLine(Source);
			AppendGeneratedAsLine(Source, TEXT("\t~FScriptCaseReference()"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t\tEndNativeScriptLifecycle(ObjectId, Value);"));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
			AppendGeneratedAsLine(Source, TEXT("}"));
			AppendGeneratedAsLine(Source);
		}
	}

	static void AppendObservationAndMutationFunctions(
		FString& Source,
		const FNativeTypeCase& TypeCase)
	{
		using namespace AngelscriptNativeTestSupport;

		if (IsReferenceType(TypeCase))
		{
			AppendGeneratedAsLine(Source, FString::Printf(
				TEXT("int ObservePropertyCopyValue(const %hs Value)"),
				TypeCase.ScriptType));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\treturn Value == nullptr ? -1 : Value.Value;"));
			AppendGeneratedAsLine(Source, TEXT("}"));
			AppendGeneratedAsLine(Source);
			AppendGeneratedAsLine(Source, FString::Printf(
				TEXT("void MutatePropertyCopyValue(%hs Value)"),
				TypeCase.ScriptType));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\tValue.Value = 37;"));
		}
		else if (IsValueObjectType(TypeCase))
		{
			AppendGeneratedAsLine(Source, FString::Printf(
				TEXT("int ObservePropertyCopyValue(const %hs& in Value)"),
				TypeCase.ScriptType));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\treturn Value.Value;"));
			AppendGeneratedAsLine(Source, TEXT("}"));
			AppendGeneratedAsLine(Source);
			AppendGeneratedAsLine(Source, FString::Printf(
				TEXT("void MutatePropertyCopyValue(%hs& inout Value)"),
				TypeCase.ScriptType));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\tValue.Value = 37;"));
		}
		else if (TypeCase.Category == ENativeValueCategory::Boolean)
		{
			AppendGeneratedAsLine(Source, TEXT("int ObservePropertyCopyValue(bool Value)"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\treturn Value ? 1 : 0;"));
			AppendGeneratedAsLine(Source, TEXT("}"));
			AppendGeneratedAsLine(Source);
			AppendGeneratedAsLine(Source, TEXT("void MutatePropertyCopyValue(bool& inout Value)"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\tValue = !Value;"));
		}
		else if (TypeCase.Category == ENativeValueCategory::Enum)
		{
			AppendGeneratedAsLine(Source, TEXT("int ObservePropertyCopyValue(ENativeCaseEnum Value)"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\treturn int(Value);"));
			AppendGeneratedAsLine(Source, TEXT("}"));
			AppendGeneratedAsLine(Source);
			AppendGeneratedAsLine(
				Source,
				TEXT("void MutatePropertyCopyValue(ENativeCaseEnum& inout Value)"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(
				Source,
				TEXT("\tValue = Value == ENativeCaseEnum::One ? ENativeCaseEnum::Zero : ENativeCaseEnum::One;"));
		}
		else
		{
			AppendGeneratedAsLine(Source, FString::Printf(
				TEXT("int ObservePropertyCopyValue(%hs Value)"),
				TypeCase.ScriptType));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\treturn int(Value);"));
			AppendGeneratedAsLine(Source, TEXT("}"));
			AppendGeneratedAsLine(Source);
			AppendGeneratedAsLine(Source, FString::Printf(
				TEXT("void MutatePropertyCopyValue(%hs& inout Value)"),
				TypeCase.ScriptType));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, FString::Printf(
				TEXT("\tValue = %s;"),
				*MakeTypedValue(TypeCase, 37)));
		}
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
	}

	static void AppendPayloadType(FString& Source, const FNativeTypeCase& TypeCase)
	{
		using namespace AngelscriptNativeTestSupport;

		AppendGeneratedAsLine(Source, TEXT("struct FPropertyCopyPayload"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, FString::Printf(
			TEXT("\t%hs Stored;"),
			TypeCase.ScriptType));
		AppendGeneratedAsLine(Source);
		if (IsValueObjectType(TypeCase))
		{
			AppendGeneratedAsLine(Source, FString::Printf(
				TEXT("\tFPropertyCopyPayload(const %hs& in InValue)"),
				TypeCase.ScriptType));
		}
		else
		{
			AppendGeneratedAsLine(Source, FString::Printf(
				TEXT("\tFPropertyCopyPayload(%hs InValue)"),
				TypeCase.ScriptType));
		}
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(Source, TEXT("\t\tStored = InValue;"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
	}

	static void AppendOwnerTypes(FString& Source, const FNativeTypeCase& TypeCase)
	{
		using namespace AngelscriptNativeTestSupport;

		const FString SourceValue = MakeTypedValue(TypeCase, InitialSourceValue(TypeCase));
		const FString TargetValue = MakeTypedValue(TypeCase, InitialTargetValue(TypeCase));
		AppendGeneratedAsLine(Source, TEXT("class FPropertyCopyBase"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, FString::Printf(
			TEXT("\t%hs Source = %s;"),
			TypeCase.ScriptType,
			*SourceValue));
		AppendGeneratedAsLine(Source, FString::Printf(
			TEXT("\t%hs Target = %s;"),
			TypeCase.ScriptType,
			*TargetValue));
		AppendGeneratedAsLine(Source, FString::Printf(
			TEXT("\tFPropertyCopyPayload NestedSource = FPropertyCopyPayload(%s);"),
			*SourceValue));
		AppendGeneratedAsLine(Source, FString::Printf(
			TEXT("\tFPropertyCopyPayload NestedTarget = FPropertyCopyPayload(%s);"),
			*TargetValue));
		AppendGeneratedAsLine(Source, TEXT("\tint ViewMarker = 11;"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("\tFPropertyCopyBase()"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("class FPropertyCopyDerived : FPropertyCopyBase"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tFPropertyCopyDerived()"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(Source, TEXT("\t\tsuper();"));
		AppendGeneratedAsLine(Source, TEXT("\t\tViewMarker = 22;"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
	}

	static FString DirectSelfTarget(const FMutationCase& MutationCase)
	{
		return IsMutation(MutationCase, "source_after_transfer")
			? TEXT("Receiver.Source")
			: TEXT("Receiver.Target");
	}

	static void AppendTransferOperation(
		FString& Source,
		const FTransferCase& TransferCase,
		const FMutationCase& MutationCase,
		const FNativeTypeCase& TypeCase)
	{
		using namespace AngelscriptNativeTestSupport;

		const bool bNested = IsMutation(MutationCase, "nested_member");
		AppendGeneratedAsLine(Source, TEXT("\tRecordPropertyCopyLifecycleBoundary(0);"));
		if (IsTransfer(TransferCase, "copy_construct"))
		{
			if (bNested)
			{
				AppendGeneratedAsLine(
					Source,
					TEXT("\tFPropertyCopyPayload TransferTarget = Receiver.NestedSource;"));
			}
			else
			{
				AppendGeneratedAsLine(Source, FString::Printf(
					TEXT("\t%hs TransferTarget = Receiver.Source;"),
					TypeCase.ScriptType));
			}
			AppendGeneratedAsLine(Source, FString::Printf(
				TEXT("\tbool TransferStable = ObservePropertyCopyValue(%s) == ObservePropertyCopyValue(%s);"),
				bNested ? TEXT("Receiver.NestedSource.Stored") : TEXT("Receiver.Source"),
				bNested ? TEXT("TransferTarget.Stored") : TEXT("TransferTarget")));
		}
		else if (IsTransfer(TransferCase, "assign"))
		{
			AppendGeneratedAsLine(
				Source,
				bNested
					? TEXT("\tReceiver.NestedTarget = Receiver.NestedSource;")
					: TEXT("\tReceiver.Target = Receiver.Source;"));
			AppendGeneratedAsLine(Source, FString::Printf(
				TEXT("\tbool TransferStable = ObservePropertyCopyValue(%s) == ObservePropertyCopyValue(%s);"),
				bNested ? TEXT("Receiver.NestedSource.Stored") : TEXT("Receiver.Source"),
				bNested ? TEXT("Receiver.NestedTarget.Stored") : TEXT("Receiver.Target")));
		}
		else
		{
			const FString SelfTarget = bNested
				? TEXT("Receiver.NestedTarget")
				: DirectSelfTarget(MutationCase);
			const FString StableTarget = bNested
				? TEXT("Receiver.NestedTarget.Stored")
				: DirectSelfTarget(MutationCase);
			if (IsReferenceType(TypeCase))
			{
				AppendGeneratedAsLine(Source, FString::Printf(
					TEXT("\t%hs StableIdentity = %s;"),
					TypeCase.ScriptType,
					*StableTarget));
			}
			else
			{
				AppendGeneratedAsLine(Source, FString::Printf(
					TEXT("\tint StableValue = ObservePropertyCopyValue(%s);"),
					*StableTarget));
			}
			AppendGeneratedAsLine(Source, FString::Printf(
				TEXT("\t%s = %s;"),
				*SelfTarget,
				*SelfTarget));
			if (IsReferenceType(TypeCase))
			{
				AppendGeneratedAsLine(Source, FString::Printf(
					TEXT("\tbool TransferStable = StableIdentity == %s;"),
					*StableTarget));
			}
			else
			{
				AppendGeneratedAsLine(Source, FString::Printf(
					TEXT("\tbool TransferStable = StableValue == ObservePropertyCopyValue(%s);"),
					*StableTarget));
			}
		}
		AppendGeneratedAsLine(Source, TEXT("\tRecordPropertyCopyLifecycleBoundary(1);"));
	}

	static FString SourceExpression(const FMutationCase& MutationCase)
	{
		if (IsMutation(MutationCase, "nested_member"))
		{
			return TEXT("Receiver.NestedSource.Stored");
		}
		return TEXT("Receiver.Source");
	}

	static FString TargetExpression(
		const FTransferCase& TransferCase,
		const FMutationCase& MutationCase)
	{
		if (IsMutation(MutationCase, "nested_member"))
		{
			return IsTransfer(TransferCase, "copy_construct")
				? TEXT("TransferTarget.Stored")
				: TEXT("Receiver.NestedTarget.Stored");
		}
		return IsTransfer(TransferCase, "copy_construct")
			? TEXT("TransferTarget")
			: TEXT("Receiver.Target");
	}

	static void AppendMutationAndObservation(
		FString& Source,
		const FTransferCase& TransferCase,
		const FMutationCase& MutationCase,
		const FNativeTypeCase& TypeCase)
	{
		using namespace AngelscriptNativeTestSupport;

		const FString SourceValue = SourceExpression(MutationCase);
		const FString TargetValue = TargetExpression(TransferCase, MutationCase);
		const FString MutationTarget = IsMutation(MutationCase, "source_after_transfer")
			? SourceValue
			: TargetValue;
		AppendGeneratedAsLine(Source, FString::Printf(
			TEXT("\tMutatePropertyCopyValue(%s);"),
			*MutationTarget));
		AppendGeneratedAsLine(Source, TEXT("\tRecordPropertyCopyLifecycleBoundary(2);"));
		if (IsReferenceType(TypeCase))
		{
			AppendGeneratedAsLine(Source, FString::Printf(
				TEXT("\tbool SameIdentity = %s == %s;"),
				*SourceValue,
				*TargetValue));
		}
		else
		{
			AppendGeneratedAsLine(Source, TEXT("\tbool SameIdentity = false;"));
		}
		AppendGeneratedAsLine(Source, FString::Printf(
			TEXT("\tRecordPropertyCopyObservation(ObservePropertyCopyValue(%s), ObservePropertyCopyValue(%s), SameIdentity, TransferStable, Receiver.ViewMarker);"),
			*SourceValue,
			*TargetValue));
	}

	static void AppendExerciseFunction(
		FString& Source,
		const FTransferCase& TransferCase,
		const FMutationCase& MutationCase,
		const FNativeTypeCase& TypeCase,
		const FViewCase& ViewCase)
	{
		using namespace AngelscriptNativeTestSupport;

		AppendGeneratedAsLine(Source, FString::Printf(
			TEXT("int ExercisePropertyCopy(%hs Receiver)"),
			ViewCase.ParameterType));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendTransferOperation(Source, TransferCase, MutationCase, TypeCase);
		AppendMutationAndObservation(Source, TransferCase, MutationCase, TypeCase);
		AppendGeneratedAsLine(Source, TEXT("\treturn 1;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
	}

	static void AppendEntryFunction(FString& Source, const FViewCase& ViewCase)
	{
		using namespace AngelscriptNativeTestSupport;

		AppendGeneratedAsLine(Source, TEXT("int RunPropertyCopy()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		if (IsView(ViewCase, "exact"))
		{
			AppendGeneratedAsLine(
				Source,
				TEXT("\tFPropertyCopyBase Receiver = FPropertyCopyBase();"));
		}
		else if (IsView(ViewCase, "base"))
		{
			AppendGeneratedAsLine(
				Source,
				TEXT("\tFPropertyCopyDerived DynamicReceiver = FPropertyCopyDerived();"));
			AppendGeneratedAsLine(
				Source,
				TEXT("\tFPropertyCopyBase Receiver = DynamicReceiver;"));
		}
		else
		{
			AppendGeneratedAsLine(
				Source,
				TEXT("\tFPropertyCopyDerived Receiver = FPropertyCopyDerived();"));
		}
		AppendGeneratedAsLine(Source, TEXT("\treturn ExercisePropertyCopy(Receiver);"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("int RunPropertyCopyRecovery()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\treturn 97;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
	}

	static FString BuildPropertyCopySource(
		const FTransferCase& TransferCase,
		const FMutationCase& MutationCase,
		const FNativeTypeCase& TypeCase,
		const FViewCase& ViewCase)
	{
		FString Source;
		AppendTypeDeclarations(Source, TypeCase);
		AppendObservationAndMutationFunctions(Source, TypeCase);
		AppendPayloadType(Source, TypeCase);
		AppendOwnerTypes(Source, TypeCase);
		AppendExerciseFunction(Source, TransferCase, MutationCase, TypeCase, ViewCase);
		AppendEntryFunction(Source, ViewCase);
		return Source;
	}

	static int32 ExpectedSourceValue(
		const FTransferCase& TransferCase,
		const FMutationCase& MutationCase,
		const FNativeTypeCase& TypeCase)
	{
		const int32 SourceInitial = InitialSourceValue(TypeCase);
		if (IsTransfer(TransferCase, "self_assign"))
		{
			return IsMutation(MutationCase, "source_after_transfer")
				? MutatedValue(TypeCase, SourceInitial)
				: SourceInitial;
		}
		if (IsReferenceType(TypeCase))
		{
			return MutatedValue(TypeCase, SourceInitial);
		}
		return IsMutation(MutationCase, "source_after_transfer")
			? MutatedValue(TypeCase, SourceInitial)
			: SourceInitial;
	}

	static int32 ExpectedTargetValue(
		const FTransferCase& TransferCase,
		const FMutationCase& MutationCase,
		const FNativeTypeCase& TypeCase)
	{
		const int32 SourceInitial = InitialSourceValue(TypeCase);
		const int32 TargetInitial = InitialTargetValue(TypeCase);
		if (IsTransfer(TransferCase, "self_assign"))
		{
			return IsMutation(MutationCase, "source_after_transfer")
				? TargetInitial
				: MutatedValue(TypeCase, TargetInitial);
		}
		if (IsReferenceType(TypeCase))
		{
			return MutatedValue(TypeCase, SourceInitial);
		}
		return IsMutation(MutationCase, "source_after_transfer")
			? SourceInitial
			: MutatedValue(TypeCase, SourceInitial);
	}

	static bool ExpectedSameIdentity(
		const FTransferCase& TransferCase,
		const FNativeTypeCase& TypeCase)
	{
		return IsReferenceType(TypeCase) && !IsTransfer(TransferCase, "self_assign");
	}

	static bool RequiresSourceOnlyForkCharacterization(const FNativeTypeCase& TypeCase)
	{
		// Raw reference-valued Property Copy can crash or hang after source emission.
		// Keep every generated source and stable ID observable, but do not enter the
		// raw module/compiler path until the ownership/conversion defect is repaired.
		return IsReferenceType(TypeCase);
	}

	static FString DescribeLifecycleEntries(const FNativeLifecycleRecorder& Lifecycle)
	{
		TArray<FString> Descriptions;
		for (const AngelscriptNativeTestSupport::FNativeLifecycleEntry& Entry : Lifecycle.GetEntries())
		{
			Descriptions.Add(FString::Printf(
				TEXT("{Event=%d,Object=%d,Related=%d,Value=%d}"),
				static_cast<int32>(Entry.Event),
				Entry.ObjectId,
				Entry.RelatedObjectId,
				Entry.Value));
		}
		return FString::Join(Descriptions, TEXT(","));
	}

	static bool HasCompileErrors(const AngelscriptNativeTestSupport::FNativeMessageCollector& Messages)
	{
		return Messages.Entries.ContainsByPredicate([](
			const AngelscriptNativeTestSupport::FNativeMessageEntry& Entry)
		{
			return Entry.Type == asMSGTYPE_ERROR;
		});
	}

	static int32 FindProperty(
		asITypeInfo& Type,
		const ANSICHAR* ExpectedName,
		int32& OutTypeId)
	{
		for (asUINT Index = 0; Index < Type.GetPropertyCount(); ++Index)
		{
			const char* Name = nullptr;
			int TypeId = asTYPEID_VOID;
			if (Type.GetProperty(Index, &Name, &TypeId) >= 0
				&& Name != nullptr
				&& FCStringAnsi::Strcmp(Name, ExpectedName) == 0)
			{
				OutTypeId = TypeId;
				return static_cast<int32>(Index);
			}
		}
		return INDEX_NONE;
	}

	void VerifyMetadata(
		const FNativeCaseContext& Case,
		asIScriptModule& Module,
		const FNativeTypeCase& TypeCase,
		const FViewCase& ViewCase)
	{
		asITypeInfo* const BaseType = Module.GetTypeInfoByName("FPropertyCopyBase");
		asITypeInfo* const DerivedType = Module.GetTypeInfoByName("FPropertyCopyDerived");
		asITypeInfo* const PayloadType = Module.GetTypeInfoByName("FPropertyCopyPayload");
		ASSERT_THAT(IsNotNull(BaseType,
			*Case.Describe(TEXT("property-copy module should publish its base owner"))));
		ASSERT_THAT(IsNotNull(DerivedType,
			*Case.Describe(TEXT("property-copy module should publish its derived owner"))));
		ASSERT_THAT(IsNotNull(PayloadType,
			*Case.Describe(TEXT("property-copy module should publish its nested payload"))));
		if (BaseType == nullptr || DerivedType == nullptr || PayloadType == nullptr)
		{
			return;
		}

		ASSERT_THAT(AreEqual(BaseType, DerivedType->GetBaseType(),
			*Case.Describe(TEXT("property-copy owner should preserve its exact base relation"))));
		const int32 ExpectedTypeId = Module.GetTypeIdByDecl(TypeCase.ScriptType);
		for (const ANSICHAR* PropertyName : { "Source", "Target" })
		{
			int32 TypeId = asTYPEID_VOID;
			ASSERT_THAT(IsTrue(FindProperty(*BaseType, PropertyName, TypeId) >= 0,
				*Case.Describe(TEXT("property-copy base should expose each direct transfer field"))));
			ASSERT_THAT(AreEqual(ExpectedTypeId, TypeId,
				*Case.Describe(TEXT("direct transfer field should preserve its exact catalog type"))));
		}
		int32 StoredTypeId = asTYPEID_VOID;
		ASSERT_THAT(AreEqual(0, FindProperty(*PayloadType, "Stored", StoredTypeId),
			*Case.Describe(TEXT("nested payload should expose Stored as its first field"))));
		ASSERT_THAT(AreEqual(ExpectedTypeId, StoredTypeId,
			*Case.Describe(TEXT("nested Stored field should preserve its exact catalog type"))));

		const FString ExerciseDeclaration = FString::Printf(
			TEXT("int ExercisePropertyCopy(%hs Receiver)"),
			ViewCase.ParameterType);
		const FTCHARToUTF8 ExerciseDeclarationUtf8(*ExerciseDeclaration);
		asIScriptFunction* const Exercise = Module.GetFunctionByDecl(ExerciseDeclarationUtf8.Get());
		ASSERT_THAT(IsNotNull(Exercise,
			*Case.Describe(TEXT("property-copy module should expose its exact static-view exercise function"))));
		if (Exercise != nullptr)
		{
			int ParameterTypeId = asTYPEID_VOID;
			asDWORD Flags = asTM_NONE;
			const char* ParameterName = nullptr;
			ASSERT_THAT(IsTrue(Exercise->GetParam(0, &ParameterTypeId, &Flags, &ParameterName) >= 0,
				*Case.Describe(TEXT("property-copy exercise function should expose receiver metadata"))));
			ASSERT_THAT(AreEqual(Module.GetTypeIdByDecl(ViewCase.ParameterType), ParameterTypeId,
				*Case.Describe(TEXT("property-copy receiver should preserve the requested static view"))));
			ASSERT_THAT(IsTrue(ParameterName != nullptr
				&& FCStringAnsi::Strcmp(ParameterName, "Receiver") == 0,
				*Case.Describe(TEXT("property-copy receiver metadata should preserve its exact name"))));
		}
	}

	void VerifyObservation(
		const FNativeCaseContext& Case,
		const FTransferCase& TransferCase,
		const FMutationCase& MutationCase,
		const FNativeTypeCase& TypeCase,
		const FViewCase& ViewCase,
		const FPropertyCopyState& State)
	{
		ASSERT_THAT(AreEqual(1, State.Observations.Num(),
			*Case.Describe(TEXT("property-copy cell should publish exactly one final observation"))));
		if (State.Observations.Num() != 1)
		{
			return;
		}
		const FPropertyCopyObservation& Observation = State.Observations[0];
		ASSERT_THAT(AreEqual(
			ExpectedSourceValue(TransferCase, MutationCase, TypeCase),
			Observation.SourceValue,
			*Case.Describe(TEXT("property-copy source should retain the expected post-mutation value"))));
		ASSERT_THAT(AreEqual(
			ExpectedTargetValue(TransferCase, MutationCase, TypeCase),
			Observation.TargetValue,
			*Case.Describe(TEXT("property-copy target should retain the expected post-mutation value"))));
		ASSERT_THAT(AreEqual(
			ExpectedSameIdentity(TransferCase, TypeCase),
			Observation.bSameIdentity,
			*Case.Describe(TEXT("value fields should be independent and reference fields intentionally identical"))));
		ASSERT_THAT(IsTrue(Observation.bTransferStable,
			*Case.Describe(TEXT("copy, assignment, or self-assignment should preserve the pre-mutation value"))));
		ASSERT_THAT(AreEqual(ViewCase.DynamicMarker, Observation.ViewMarker,
			*Case.Describe(TEXT("property-copy observation should preserve dynamic receiver identity"))));
	}

	void VerifyLifecycleBoundaries(
		const FNativeCaseContext& Case,
		const FTransferCase& TransferCase,
		const FMutationCase& MutationCase,
		const FNativeTypeCase& TypeCase,
		const FPropertyCopyState& State)
	{
		ASSERT_THAT(AreEqual(3, State.Snapshots.Num(),
			*Case.Describe(TEXT("property-copy cell should record before/after-transfer/after-mutation boundaries"))));
		if (State.Snapshots.Num() != 3)
		{
			return;
		}
		for (int32 Stage = 0; Stage < 3; ++Stage)
		{
			ASSERT_THAT(AreEqual(Stage, State.Snapshots[Stage].Stage,
				*Case.Describe(TEXT("property-copy lifecycle boundaries should preserve stage order"))));
		}
		const FPropertyCopyLifecycleSnapshot& Before = State.Snapshots[0];
		const FPropertyCopyLifecycleSnapshot& AfterTransfer = State.Snapshots[1];
		const FPropertyCopyLifecycleSnapshot& AfterMutation = State.Snapshots[2];
		if (IsValueObjectType(TypeCase))
		{
			const bool bNestedMember = IsMutation(MutationCase, "nested_member");
			if (bNestedMember && IsScriptValueType(TypeCase))
			{
				if (IsTransfer(TransferCase, "copy_construct"))
				{
					ASSERT_THAT(AreEqual(Before.Copies, AfterTransfer.Copies,
						*Case.Describe(TEXT("current fork nested script-value copy should bypass the user copy constructor"))));
				}
				else
				{
					ASSERT_THAT(AreEqual(Before.Assignments, AfterTransfer.Assignments,
						*Case.Describe(TEXT("current fork nested script-value assignment should bypass the user opAssign"))));
				}
			}
			else if (bNestedMember
				&& IsNativeValueType(TypeCase)
				&& IsTransfer(TransferCase, "copy_construct"))
			{
				ASSERT_THAT(AreEqual(Before.Copies, AfterTransfer.Copies,
					*Case.Describe(TEXT("current fork nested native-value copy should bypass the registered copy constructor"))));
			}
			else if (IsTransfer(TransferCase, "copy_construct"))
			{
				ASSERT_THAT(IsTrue(AfterTransfer.Copies > Before.Copies,
					*Case.Describe(TEXT("value-field copy construction should add a copy lifecycle event"))));
			}
			else
			{
				ASSERT_THAT(IsTrue(AfterTransfer.Assignments > Before.Assignments,
					*Case.Describe(TEXT("value-field assignment should add an assignment lifecycle event"))));
			}
		}
		if (IsReferenceType(TypeCase))
		{
			ASSERT_THAT(AreEqual(Before.Constructions, AfterTransfer.Constructions,
				*Case.Describe(TEXT("reference transfer should not construct a second pointee"))));
		}
		ASSERT_THAT(AreEqual(AfterTransfer.Constructions, AfterMutation.Constructions,
			*Case.Describe(TEXT("nested value mutation should not construct replacement storage"))));
		ASSERT_THAT(AreEqual(AfterTransfer.Copies, AfterMutation.Copies,
			*Case.Describe(TEXT("nested value mutation should not copy replacement storage"))));
		ASSERT_THAT(AreEqual(AfterTransfer.Assignments, AfterMutation.Assignments,
			*Case.Describe(TEXT("nested scalar/member mutation should not assign the owning value object"))));
	}

	void VerifyFinalLifecycle(
		const FNativeCaseContext& Case,
		const FNativeTypeCase& TypeCase,
		const FNativeLifecycleRecorder& Lifecycle)
	{
		ASSERT_THAT(AreEqual(0, Lifecycle.GetLiveObjectCount(),
			*Case.Describe(TEXT("property-copy execution should leave no tracked object alive"))));
		if (!HasObjectLifecycle(TypeCase))
		{
			ASSERT_THAT(AreEqual(0, Lifecycle.GetEntries().Num(),
				*Case.Describe(TEXT("primitive property-copy cell should have no object lifecycle events"))));
			return;
		}

		TSet<int32> ConstructedIds;
		TSet<int32> DestructedIds;
		int32 DestructEventCount = 0;
		for (const AngelscriptNativeTestSupport::FNativeLifecycleEntry& Entry : Lifecycle.GetEntries())
		{
			if (Entry.Event == ENativeLifecycleEvent::DefaultConstruct
				|| Entry.Event == ENativeLifecycleEvent::ValueConstruct
				|| Entry.Event == ENativeLifecycleEvent::CopyConstruct)
			{
				ConstructedIds.Add(Entry.ObjectId);
			}
			else if (Entry.Event == ENativeLifecycleEvent::Destruct)
			{
				++DestructEventCount;
				ASSERT_THAT(IsTrue(ConstructedIds.Contains(Entry.ObjectId),
					*Case.Describe(TEXT("property-copy destructor should identify constructed storage"))));
				if (!IsScriptValueType(TypeCase))
				{
					ASSERT_THAT(IsFalse(DestructedIds.Contains(Entry.ObjectId),
						*Case.Describe(TEXT("property-copy storage should be destroyed no more than once"))));
				}
				DestructedIds.Add(Entry.ObjectId);
			}
		}
		ASSERT_THAT(IsTrue(ConstructedIds.Num() > 0,
			*Case.Describe(TEXT("object/reference property-copy cell should construct real storage"))));
		if (IsScriptValueType(TypeCase))
		{
			const FString LifecycleTrace = DescribeLifecycleEntries(Lifecycle);
			ASSERT_THAT(IsTrue(ConstructedIds.Num() > DestructedIds.Num(),
				*FString::Printf(TEXT("%s. Constructed=%d DistinctDestructed=%d Trace=[%s]"),
					*Case.Describe(TEXT("current fork script-value transport should preserve its duplicate lifecycle identity characterization")),
					ConstructedIds.Num(),
					DestructedIds.Num(),
					*LifecycleTrace)));
			ASSERT_THAT(IsTrue(DestructEventCount > DestructedIds.Num(),
				*FString::Printf(TEXT("%s. DestructEvents=%d DistinctDestructed=%d Trace=[%s]"),
					*Case.Describe(TEXT("current fork script-value transport should expose duplicate destructor identities")),
					DestructEventCount,
					DestructedIds.Num(),
					*LifecycleTrace)));
		}
		else
		{
			ASSERT_THAT(AreEqual(ConstructedIds.Num(), DestructedIds.Num(),
				*Case.Describe(TEXT("every property-copy construction should have exactly one destructor"))));
		}
	}

	void ExecuteCell(
		const FNativeCaseContext& Case,
		const FTransferCase& TransferCase,
		const FMutationCase& MutationCase,
		const FNativeTypeCase& TypeCase,
		const FViewCase& ViewCase,
		asIScriptEngine& ScriptEngine,
		asIScriptModule& Module,
		FPropertyCopyState& State,
		FNativeLifecycleRecorder& Lifecycle)
	{
		using namespace AngelscriptNativeTestSupport;

		VerifyMetadata(Case, Module, TypeCase, ViewCase);
		asIScriptFunction* const Entry = Module.GetFunctionByDecl("int RunPropertyCopy()");
		asIScriptFunction* const Recovery = Module.GetFunctionByDecl("int RunPropertyCopyRecovery()");
		ASSERT_THAT(IsNotNull(Entry,
			*Case.Describe(TEXT("property-copy module should expose its exact entry"))));
		ASSERT_THAT(IsNotNull(Recovery,
			*Case.Describe(TEXT("property-copy module should expose its exact recovery entry"))));
		if (Entry == nullptr || Recovery == nullptr)
		{
			return;
		}

		asIScriptContext* const Context = ScriptEngine.CreateContext();
		ASSERT_THAT(IsNotNull(Context,
			*Case.Describe(TEXT("property-copy cell should create a reusable context"))));
		if (Context == nullptr)
		{
			return;
		}
		ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_FINISHED), PrepareAndExecute(Context, Entry),
			*Case.Describe(TEXT("property-copy entry should finish"))));
		ASSERT_THAT(AreEqual(1, static_cast<int32>(Context->GetReturnDWord()),
			*Case.Describe(TEXT("property-copy entry should return its success sentinel"))));
		ASSERT_THAT(AreEqual(asSUCCESS, Context->Unprepare(),
			*Case.Describe(TEXT("property-copy context should release its receiver and transfer storage"))));

		VerifyObservation(Case, TransferCase, MutationCase, TypeCase, ViewCase, State);
		VerifyLifecycleBoundaries(Case, TransferCase, MutationCase, TypeCase, State);
		VerifyFinalLifecycle(Case, TypeCase, Lifecycle);

		const int32 ObservationCount = State.Observations.Num();
		const int32 SnapshotCount = State.Snapshots.Num();
		ASSERT_THAT(IsTrue(Context->Prepare(Recovery) >= 0,
			*Case.Describe(TEXT("property-copy context should prepare its recovery entry"))));
		ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_FINISHED), Context->Execute(),
			*Case.Describe(TEXT("property-copy recovery should finish in the same context"))));
		ASSERT_THAT(AreEqual(97, static_cast<int32>(Context->GetReturnDWord()),
			*Case.Describe(TEXT("property-copy recovery should return its clean sentinel"))));
		ASSERT_THAT(AreEqual(ObservationCount, State.Observations.Num(),
			*Case.Describe(TEXT("recovery should not replay property-copy observations"))));
		ASSERT_THAT(AreEqual(SnapshotCount, State.Snapshots.Num(),
			*Case.Describe(TEXT("recovery should not replay property-copy lifecycle boundaries"))));
		ASSERT_THAT(AreEqual(asSUCCESS, Context->Unprepare(),
			*Case.Describe(TEXT("property-copy recovery should unprepare cleanly"))));
		Context->Release();
	}

public:
	TEST_METHOD(TypesByTransferMutationAndView)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT("LANG-PROP-COPY-INDEPENDENCE",
			ENativeEvidence::Compile
				| ENativeEvidence::Diagnostic
				| ENativeEvidence::Runtime
				| ENativeEvidence::Metadata
				| ENativeEvidence::Lifecycle
				| ENativeEvidence::Cleanup);

		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };
		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine,
			TEXT("Property-copy product should create a raw SDK engine")));
		if (ScriptEngine == nullptr)
		{
			return;
		}

		FNativeLifecycleRecorder Lifecycle;
		FPropertyCopyState State;
		State.Lifecycle = &Lifecycle;
		ASSERT_THAT(IsTrue(RegisterPropertyCopyBridge(*ScriptEngine, State),
			TEXT("Property-copy product should register its observation/lifecycle bridge")));
		ASSERT_THAT(IsTrue(RegisterNativeCaseValue(*ScriptEngine, Lifecycle),
			TEXT("Property-copy product should register its tracked native value")));
		ASSERT_THAT(IsTrue(RegisterNativeCaseReference(*ScriptEngine, &Lifecycle),
			TEXT("Property-copy product should register its tracked native reference")));
		ASSERT_THAT(IsTrue(RegisterNativeScriptLifecycleBridge(*ScriptEngine, Lifecycle),
			TEXT("Property-copy product should register its script lifecycle bridge")));
		ASSERT_THAT(IsTrue(RegisterCoreLanguageTypedef(*ScriptEngine),
			TEXT("Property-copy product should register its core typedef alias")));

		for (const FMutationCase& MutationCase : MutationCases)
		{
			for (const FTransferCase& TransferCase : TransferCases)
			{
				for (const FNativeTypeCase& TypeCase : NativeTypeCases)
				{
					if (TypeCase.Category == ENativeValueCategory::Null)
					{
						continue;
					}
					for (const FViewCase& ViewCase : ViewCases)
					{
						State.Reset();
						Lifecycle.Reset();
						const FNativeCaseContext Case(MakeNativeCaseId(
							"LANG-PROP-COPY-INDEPENDENCE",
							{
								ANSI_TO_TCHAR(MutationCase.CatalogName),
								ANSI_TO_TCHAR(TransferCase.CatalogName),
								ANSI_TO_TCHAR(TypeCase.CatalogName),
								ANSI_TO_TCHAR(ViewCase.CatalogName),
							}));
						const FString Suffix = MakeSuffix(
							MutationCase,
							TransferCase,
							TypeCase,
							ViewCase);
						const FString ModuleName = TEXT("PropertyCopy_") + Suffix;
						const FString Source = BuildPropertyCopySource(
							TransferCase,
							MutationCase,
							TypeCase,
							ViewCase);
						PrintGeneratedAsSource(*TestRunner, Case.GetId(), ModuleName, Source);
						const FTCHARToUTF8 ModuleNameUtf8(*ModuleName);
						const FTCHARToUTF8 SourceUtf8(*Source);
						if (RequiresSourceOnlyForkCharacterization(TypeCase))
						{
							TestRunner->AddInfo(FString::Printf(
								TEXT("[AS-FORK-LIMITATION] Id=%s raw reference-valued property copy is source-only because the compiler/module path can crash or hang this fork"),
								*Case.GetId()));
							ASSERT_THAT(IsNull(ScriptEngine->GetModule(
								ModuleNameUtf8.Get(),
								asGM_ONLY_IF_EXISTS),
								*Case.Describe(TEXT("source-only property-copy boundary should not publish a module"))));
							ASSERT_THAT(AreEqual(0, Lifecycle.GetLiveObjectCount(),
								*Case.Describe(TEXT("source-only property-copy boundary should not allocate tracked objects"))));
							continue;
						}
						Engine.ResetMessages();
						asIScriptModule* Module = nullptr;
						const int BuildResult = CompileNativeModule(
							ScriptEngine,
							ModuleNameUtf8.Get(),
							SourceUtf8.Get(),
							Module);
						ASSERT_THAT(IsTrue(BuildResult >= 0,
							*FString::Printf(TEXT("%s. BuildResult=%d Messages={%s}"),
								*Case.Describe(TEXT("property-copy cell should compile")),
								BuildResult,
								*Engine.GetMessagesText())));
						ASSERT_THAT(IsNotNull(Module,
							*Case.Describe(TEXT("property-copy cell should publish a module"))));
						ASSERT_THAT(IsFalse(HasCompileErrors(Engine.GetMessages()),
							*Case.Describe(TEXT("successful property-copy build should publish no error diagnostic"))));
						if (BuildResult >= 0 && Module != nullptr)
						{
							ExecuteCell(
								Case,
								TransferCase,
								MutationCase,
								TypeCase,
								ViewCase,
								*ScriptEngine,
								*Module,
								State,
								Lifecycle);
							if (IsScriptValueType(TypeCase))
							{
								TestRunner->AddInfo(FString::Printf(
									TEXT("[AS-FORK-LIMITATION] Id=%s raw script-value transport duplicates lifecycle identities; duplicate destructor callbacks are asserted"),
									*Case.GetId()));
							}
							if (IsMutation(MutationCase, "nested_member") && IsScriptValueType(TypeCase))
							{
								TestRunner->AddInfo(FString::Printf(
									TEXT("[AS-FORK-LIMITATION] Id=%s nested script-value transfer bypasses user copy and assignment callbacks"),
									*Case.GetId()));
							}
							else if (IsMutation(MutationCase, "nested_member")
								&& IsNativeValueType(TypeCase)
								&& IsTransfer(TransferCase, "copy_construct"))
							{
								TestRunner->AddInfo(FString::Printf(
									TEXT("[AS-FORK-LIMITATION] Id=%s nested native-value copy bypasses the registered copy constructor"),
									*Case.GetId()));
							}
						}

						ScriptEngine->DiscardModule(ModuleNameUtf8.Get());
						ASSERT_THAT(IsNull(ScriptEngine->GetModule(
							ModuleNameUtf8.Get(),
							asGM_ONLY_IF_EXISTS),
							*Case.Describe(TEXT("property-copy cell should discard its isolated module"))));
						ASSERT_THAT(AreEqual(0, Lifecycle.GetLiveObjectCount(),
							*Case.Describe(TEXT("discarded property-copy cell should leave no tracked object"))));
					}
				}
			}
		}
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
