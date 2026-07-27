#include "../../Support/AngelscriptNativeFixtureTestSupport.h"
#include "../../Support/AngelscriptNativeLanguageCaseTestSupport.h"

#include "CQTest.h"
#include "Misc/ScopeExit.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FDestructorExitTests,
	"Angelscript.TestModule.AngelScriptSDK.Language.Destructors.OwnerExit",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	using ENativeLifecycleEvent =
		AngelscriptNativeTestSupport::ENativeLifecycleEvent;
	using FNativeCaseContext =
		AngelscriptNativeTestSupport::FNativeCaseContext;
	using FNativeMessageEntry =
		AngelscriptNativeTestSupport::FNativeMessageEntry;
	using FNativeLifecycleEntry =
		AngelscriptNativeTestSupport::FNativeLifecycleEntry;
	using FNativeLifecycleRecorder =
		AngelscriptNativeTestSupport::FNativeLifecycleRecorder;
	using FNativeTestEngine =
		AngelscriptNativeTestSupport::FNativeTestEngine;

	static constexpr asPWORD DestructorExitStateUserDataSlot =
		static_cast<asPWORD>(0x44544F5245584954ull);

	enum class EOwnerKind : uint8
	{
		LocalValue,
		NestedLocalValue,
		FieldOwner,
		BaseDerivedOwner,
		TemporaryValue,
		ReturnedValue,
		ArgumentCopy,
		Reference,
		ReferenceAlias,
		Global,
	};

	enum class ETerminalKind : uint8
	{
		Normal,
		Exception,
		Abort,
		Unprepare,
		ModuleDiscard,
		EngineShutdown,
	};

	struct FScenarioCase
	{
		const ANSICHAR* CatalogName;
		EOwnerKind Owner;
		ETerminalKind Terminal;
		int32 RouteId;
	};

	struct FNestingCase
	{
		const ANSICHAR* CatalogName;
		int32 UnitCount;
		int32 TokenBase;
	};

	struct FObservationCase
	{
		const ANSICHAR* CatalogName;
	};

	inline static constexpr FScenarioCase ScenarioCases[] =
	{
		{
			"local_block_end",
			EOwnerKind::LocalValue,
			ETerminalKind::Normal,
			1,
		},
		{
			"local_return",
			EOwnerKind::LocalValue,
			ETerminalKind::Normal,
			2,
		},
		{
			"local_early_return",
			EOwnerKind::LocalValue,
			ETerminalKind::Normal,
			3,
		},
		{
			"local_break",
			EOwnerKind::LocalValue,
			ETerminalKind::Normal,
			4,
		},
		{
			"local_continue",
			EOwnerKind::LocalValue,
			ETerminalKind::Normal,
			5,
		},
		{
			"local_switch_exit",
			EOwnerKind::LocalValue,
			ETerminalKind::Normal,
			6,
		},
		{
			"local_exception",
			EOwnerKind::LocalValue,
			ETerminalKind::Exception,
			7,
		},
		{
			"local_abort",
			EOwnerKind::LocalValue,
			ETerminalKind::Abort,
			8,
		},
		{
			"local_unprepare",
			EOwnerKind::LocalValue,
			ETerminalKind::Unprepare,
			9,
		},
		{
			"nested_local_block_end",
			EOwnerKind::NestedLocalValue,
			ETerminalKind::Normal,
			10,
		},
		{
			"nested_local_return",
			EOwnerKind::NestedLocalValue,
			ETerminalKind::Normal,
			11,
		},
		{
			"nested_local_exception",
			EOwnerKind::NestedLocalValue,
			ETerminalKind::Exception,
			12,
		},
		{
			"field_block_end",
			EOwnerKind::FieldOwner,
			ETerminalKind::Normal,
			13,
		},
		{
			"field_return",
			EOwnerKind::FieldOwner,
			ETerminalKind::Normal,
			14,
		},
		{
			"field_exception",
			EOwnerKind::FieldOwner,
			ETerminalKind::Exception,
			15,
		},
		{
			"field_abort",
			EOwnerKind::FieldOwner,
			ETerminalKind::Abort,
			16,
		},
		{
			"base_derived_block_end",
			EOwnerKind::BaseDerivedOwner,
			ETerminalKind::Normal,
			17,
		},
		{
			"base_derived_return",
			EOwnerKind::BaseDerivedOwner,
			ETerminalKind::Normal,
			18,
		},
		{
			"base_derived_exception",
			EOwnerKind::BaseDerivedOwner,
			ETerminalKind::Exception,
			19,
		},
		{
			"base_derived_abort",
			EOwnerKind::BaseDerivedOwner,
			ETerminalKind::Abort,
			20,
		},
		{
			"temporary_statement_end",
			EOwnerKind::TemporaryValue,
			ETerminalKind::Normal,
			21,
		},
		{
			"temporary_early_return",
			EOwnerKind::TemporaryValue,
			ETerminalKind::Normal,
			22,
		},
		{
			"temporary_exception",
			EOwnerKind::TemporaryValue,
			ETerminalKind::Exception,
			23,
		},
		{
			"returned_value_consume",
			EOwnerKind::ReturnedValue,
			ETerminalKind::Normal,
			24,
		},
		{
			"returned_value_discard",
			EOwnerKind::ReturnedValue,
			ETerminalKind::Normal,
			25,
		},
		{
			"returned_value_exception",
			EOwnerKind::ReturnedValue,
			ETerminalKind::Exception,
			26,
		},
		{
			"argument_copy_return",
			EOwnerKind::ArgumentCopy,
			ETerminalKind::Normal,
			27,
		},
		{
			"argument_copy_exception",
			EOwnerKind::ArgumentCopy,
			ETerminalKind::Exception,
			28,
		},
		{
			"argument_copy_abort",
			EOwnerKind::ArgumentCopy,
			ETerminalKind::Abort,
			29,
		},
		{
			"reference_scope_end",
			EOwnerKind::Reference,
			ETerminalKind::Normal,
			30,
		},
		{
			"reference_alias_scope_end",
			EOwnerKind::ReferenceAlias,
			ETerminalKind::Normal,
			31,
		},
		{
			"reference_return",
			EOwnerKind::Reference,
			ETerminalKind::Normal,
			32,
		},
		{
			"reference_exception",
			EOwnerKind::Reference,
			ETerminalKind::Exception,
			33,
		},
		{
			"reference_abort",
			EOwnerKind::Reference,
			ETerminalKind::Abort,
			34,
		},
		{
			"reference_unprepare",
			EOwnerKind::Reference,
			ETerminalKind::Unprepare,
			35,
		},
		{
			"module_discard_global",
			EOwnerKind::Global,
			ETerminalKind::ModuleDiscard,
			36,
		},
		{
			"engine_shutdown_global",
			EOwnerKind::Global,
			ETerminalKind::EngineShutdown,
			37,
		},
	};

	inline static constexpr FNestingCase NestingCases[] =
	{
		{ "one", 1, 100 },
		{ "sequential", 3, 200 },
		{ "nested_scopes", 3, 300 },
		{ "nested_calls", 3, 400 },
		{ "loop", 3, 500 },
		{ "recursion", 3, 600 },
	};

	inline static constexpr FObservationCase ObservationCases[] =
	{
		{ "event_order" },
		{ "ownership_once" },
		{ "terminal_state_recovery" },
	};

	struct FDestructorExitState
	{
		FNativeLifecycleRecorder* Lifecycle = nullptr;
		ETerminalKind Terminal = ETerminalKind::Normal;
		TArray<int32> RouteMarkers;
		int32 TerminalCalls = 0;
		int32 LiveAtTerminal = INDEX_NONE;
		int32 ConstructsAtTerminal = INDEX_NONE;
		int32 DestructsAtTerminal = INDEX_NONE;
		int32 GlobalReadyCalls = 0;
		int32 TerminalRequestResult = INDEX_NONE;

		void Reset(
			FNativeLifecycleRecorder& InLifecycle,
			const ETerminalKind InTerminal)
		{
			Lifecycle = &InLifecycle;
			Terminal = InTerminal;
			RouteMarkers.Reset();
			TerminalCalls = 0;
			LiveAtTerminal = INDEX_NONE;
			ConstructsAtTerminal = INDEX_NONE;
			DestructsAtTerminal = INDEX_NONE;
			GlobalReadyCalls = 0;
			TerminalRequestResult = INDEX_NONE;
		}
	};

	static bool IsScenario(
		const FScenarioCase& Scenario,
		const ANSICHAR* Name)
	{
		return FCStringAnsi::Strcmp(
			Scenario.CatalogName,
			Name) == 0;
	}

	static bool IsGlobalScenario(
		const FScenarioCase& Scenario)
	{
		return Scenario.Owner == EOwnerKind::Global;
	}

	static int32 ConstructionCount(
		const FNativeLifecycleRecorder& Lifecycle)
	{
		return Lifecycle.Num(
			ENativeLifecycleEvent::ValueConstruct)
			+ Lifecycle.Num(
				ENativeLifecycleEvent::CopyConstruct);
	}

	static FDestructorExitState* GetActiveDestructorExitState()
	{
		asIScriptContext* const Context = asGetActiveContext();
		return Context != nullptr && Context->GetEngine() != nullptr
			? static_cast<FDestructorExitState*>(
				Context->GetEngine()->GetUserData(
					DestructorExitStateUserDataSlot))
			: nullptr;
	}

	static void SnapshotDestructorExitTerminal(
		FDestructorExitState& State,
		const int32 RouteId)
	{
		State.RouteMarkers.Add(RouteId);
		++State.TerminalCalls;
		if (State.Lifecycle != nullptr)
		{
			State.LiveAtTerminal =
				State.Lifecycle->GetLiveObjectCount();
			State.ConstructsAtTerminal =
				ConstructionCount(*State.Lifecycle);
			State.DestructsAtTerminal =
				State.Lifecycle->Num(
					ENativeLifecycleEvent::Destruct);
		}
	}

	static void ReachDestructorExitTerminal(const int32 RouteId)
	{
		FDestructorExitState* const State =
			GetActiveDestructorExitState();
		asIScriptContext* const Context = asGetActiveContext();
		if (State == nullptr || Context == nullptr)
		{
			return;
		}

		SnapshotDestructorExitTerminal(*State, RouteId);
		switch (State->Terminal)
		{
		case ETerminalKind::Exception:
			Context->SetException(
				"Destructor owner-exit exception sentinel");
			break;
		case ETerminalKind::Abort:
			State->TerminalRequestResult = Context->Abort();
			break;
		case ETerminalKind::Unprepare:
			State->TerminalRequestResult = Context->Suspend();
			break;
		case ETerminalKind::Normal:
		case ETerminalKind::ModuleDiscard:
		case ETerminalKind::EngineShutdown:
		default:
			break;
		}
	}

	static int32 RecordDestructorExitGlobalReady(
		const int32 RouteId)
	{
		if (FDestructorExitState* const State =
			GetActiveDestructorExitState())
		{
			SnapshotDestructorExitTerminal(
				*State,
				RouteId);
			++State->GlobalReadyCalls;
			return State->LiveAtTerminal;
		}
		return INDEX_NONE;
	}

	static bool RegisterDestructorExitBridge(
		asIScriptEngine& ScriptEngine,
		FDestructorExitState& State)
	{
		ScriptEngine.SetUserData(
			&State,
			DestructorExitStateUserDataSlot);
		const ASAutoCaller::FunctionCaller TerminalCaller =
			ASAutoCaller::MakeFunctionCaller(
				ReachDestructorExitTerminal);
		const ASAutoCaller::FunctionCaller GlobalReadyCaller =
			ASAutoCaller::MakeFunctionCaller(
				RecordDestructorExitGlobalReady);
		return ScriptEngine.RegisterGlobalFunction(
			"void ReachDestructorExitTerminal(int RouteId)",
			asFUNCTION(ReachDestructorExitTerminal),
			asCALL_CDECL,
			*(asFunctionCaller*)&TerminalCaller) >= 0
			&& ScriptEngine.RegisterGlobalFunction(
				"int RecordDestructorExitGlobalReady(int RouteId)",
				asFUNCTION(RecordDestructorExitGlobalReady),
				asCALL_CDECL,
				*(asFunctionCaller*)&GlobalReadyCaller) >= 0;
	}

	static void AppendExitValueType(FString& Source)
	{
		using namespace AngelscriptNativeTestSupport;

		AppendGeneratedAsLine(Source, TEXT("struct FExitValue"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tint ObjectId = -1;"));
		AppendGeneratedAsLine(Source, TEXT("\tint Token = 0;"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("\tFExitValue()"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(
			Source,
			TEXT("\t\tObjectId = BeginNativeScriptLifecycle(0);"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(
			Source,
			TEXT("\tFExitValue(int InToken)"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(Source, TEXT("\t\tToken = InToken;"));
		AppendGeneratedAsLine(
			Source,
			TEXT("\t\tObjectId = BeginNativeScriptLifecycle(Token);"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(
			Source,
			TEXT("\tFExitValue(const FExitValue& Other)"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(Source, TEXT("\t\tToken = Other.Token;"));
		AppendGeneratedAsLine(
			Source,
			TEXT("\t\tObjectId = CopyNativeScriptLifecycle(Other.ObjectId, Token);"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(
			Source,
			TEXT("\tFExitValue& opAssign(const FExitValue& Other)"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(
			Source,
			TEXT("\t\tAssignNativeScriptLifecycle(ObjectId, Other.ObjectId, Other.Token);"));
		AppendGeneratedAsLine(Source, TEXT("\t\tToken = Other.Token;"));
		AppendGeneratedAsLine(Source, TEXT("\t\treturn this;"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("\t~FExitValue()"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(
			Source,
			TEXT("\t\tEndNativeScriptLifecycle(ObjectId, Token);"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
	}

	static void AppendExitReferenceType(FString& Source)
	{
		using namespace AngelscriptNativeTestSupport;

		AppendGeneratedAsLine(Source, TEXT("class FExitReference"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tint ObjectId = -1;"));
		AppendGeneratedAsLine(Source, TEXT("\tint Token = 0;"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(
			Source,
			TEXT("\tFExitReference()"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(
			Source,
			TEXT("\t\tObjectId = BeginNativeScriptLifecycle(0);"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(
			Source,
			TEXT("\tFExitReference(int InToken)"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(Source, TEXT("\t\tToken = InToken;"));
		AppendGeneratedAsLine(
			Source,
			TEXT("\t\tObjectId = BeginNativeScriptLifecycle(Token);"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("\t~FExitReference()"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(
			Source,
			TEXT("\t\tEndNativeScriptLifecycle(ObjectId, Token);"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("\tint GetToken() const"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(Source, TEXT("\t\treturn Token;"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
	}

	static void AppendOwnerTypes(FString& Source)
	{
		using namespace AngelscriptNativeTestSupport;

		AppendGeneratedAsLine(Source, TEXT("class FExitFieldOwner"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tFExitReference First;"));
		AppendGeneratedAsLine(Source, TEXT("\tFExitReference Middle;"));
		AppendGeneratedAsLine(Source, TEXT("\tFExitReference Last;"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(
			Source,
			TEXT("\tFExitFieldOwner(int Token)"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(
			Source,
			TEXT("\t\tFirst = FExitReference(Token * 10 + 1);"));
		AppendGeneratedAsLine(
			Source,
			TEXT("\t\tMiddle = FExitReference(Token * 10 + 2);"));
		AppendGeneratedAsLine(
			Source,
			TEXT("\t\tLast = FExitReference(Token * 10 + 3);"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("class FExitBaseOwner"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tFExitReference BaseField;"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(
			Source,
			TEXT("\tFExitBaseOwner(int Token)"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(
			Source,
			TEXT("\t\tBaseField = FExitReference(Token * 10 + 1);"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(
			Source,
			TEXT("class FExitDerivedOwner : FExitBaseOwner"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(
			Source,
			TEXT("\tFExitReference DerivedMiddle;"));
		AppendGeneratedAsLine(
			Source,
			TEXT("\tFExitReference DerivedLast;"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(
			Source,
			TEXT("\tFExitDerivedOwner(int Token)"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(Source, TEXT("\t\tsuper(Token);"));
		AppendGeneratedAsLine(
			Source,
			TEXT("\t\tDerivedMiddle = FExitReference(Token * 10 + 2);"));
		AppendGeneratedAsLine(
			Source,
			TEXT("\t\tDerivedLast = FExitReference(Token * 10 + 3);"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
	}

	static void AppendValueHelpers(FString& Source)
	{
		using namespace AngelscriptNativeTestSupport;

		AppendGeneratedAsLine(
			Source,
			TEXT("int ObserveExitValue(const FExitValue& Value)"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\treturn Value.Token;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(
			Source,
			TEXT("FExitValue MakeExitValue(int Token)"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(
			Source,
			TEXT("\treturn FExitValue(Token);"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(
			Source,
			TEXT("void ConsumeExitTemporaryAndTerminate(const FExitValue& Value, int RouteId)"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(
			Source,
			TEXT("\tif (Value.Token >= 0)"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(
			Source,
			TEXT("\t\tReachDestructorExitTerminal(RouteId);"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(
			Source,
			TEXT("void ConsumeExitValueByValue(FExitValue Value, int RouteId, bool Terminal)"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tif (Terminal)"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(
			Source,
			TEXT("\t\tReachDestructorExitTerminal(RouteId);"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
	}

	static void AppendCommonTypesAndHelpers(FString& Source)
	{
		AppendExitValueType(Source);
		AppendExitReferenceType(Source);
		AppendOwnerTypes(Source);
		AppendValueHelpers(Source);
	}

	static FString Indent(const int32 Level)
	{
		return FString::ChrN(Level, TEXT('\t'));
	}

	static void AppendPassiveOwner(
		FString& Source,
		const FScenarioCase& Scenario,
		const FString& TokenExpression,
		const FString& Suffix,
		const int32 IndentLevel)
	{
		using namespace AngelscriptNativeTestSupport;

		const FString Prefix = Indent(IndentLevel);
		switch (Scenario.Owner)
		{
		case EOwnerKind::LocalValue:
			AppendGeneratedAsLine(Source, FString::Printf(
				TEXT("%sFExitValue Owner%s(%s);"),
				*Prefix,
				*Suffix,
				*TokenExpression));
			break;
		case EOwnerKind::NestedLocalValue:
			AppendGeneratedAsLine(Source, FString::Printf(
				TEXT("%sFExitValue Outer%s(%s);"),
				*Prefix,
				*Suffix,
				*TokenExpression));
			AppendGeneratedAsLine(
				Source,
				Prefix + TEXT("{"));
			AppendGeneratedAsLine(Source, FString::Printf(
				TEXT("%s\tFExitValue Inner%s(%s + 1);"),
				*Prefix,
				*Suffix,
				*TokenExpression));
			AppendGeneratedAsLine(
				Source,
				Prefix + TEXT("}"));
			break;
		case EOwnerKind::FieldOwner:
			AppendGeneratedAsLine(Source, FString::Printf(
				TEXT("%sFExitFieldOwner Owner%s = FExitFieldOwner(%s);"),
				*Prefix,
				*Suffix,
				*TokenExpression));
			break;
		case EOwnerKind::BaseDerivedOwner:
			AppendGeneratedAsLine(Source, FString::Printf(
				TEXT("%sFExitDerivedOwner Owner%s = FExitDerivedOwner(%s);"),
				*Prefix,
				*Suffix,
				*TokenExpression));
			break;
		case EOwnerKind::TemporaryValue:
			AppendGeneratedAsLine(Source, FString::Printf(
				TEXT("%sObserveExitValue(FExitValue(%s));"),
				*Prefix,
				*TokenExpression));
			break;
		case EOwnerKind::ReturnedValue:
			if (IsScenario(
				Scenario,
				"returned_value_discard"))
			{
				AppendGeneratedAsLine(Source, FString::Printf(
					TEXT("%sMakeExitValue(%s);"),
					*Prefix,
					*TokenExpression));
			}
			else
			{
				AppendGeneratedAsLine(Source, FString::Printf(
					TEXT("%sFExitValue Returned%s = MakeExitValue(%s);"),
					*Prefix,
					*Suffix,
					*TokenExpression));
			}
			break;
		case EOwnerKind::ArgumentCopy:
			AppendGeneratedAsLine(Source, FString::Printf(
				TEXT("%sFExitValue Source%s(%s);"),
				*Prefix,
				*Suffix,
				*TokenExpression));
			AppendGeneratedAsLine(Source, FString::Printf(
				TEXT("%sConsumeExitValueByValue(Source%s, %d, false);"),
				*Prefix,
				*Suffix,
				Scenario.RouteId));
			break;
		case EOwnerKind::ReferenceAlias:
			AppendGeneratedAsLine(Source, FString::Printf(
				TEXT("%sFExitReference Owner%s = FExitReference(%s);"),
				*Prefix,
				*Suffix,
				*TokenExpression));
			AppendGeneratedAsLine(Source, FString::Printf(
				TEXT("%sFExitReference Alias%s = Owner%s;"),
				*Prefix,
				*Suffix,
				*Suffix));
			break;
		case EOwnerKind::Reference:
		default:
			AppendGeneratedAsLine(Source, FString::Printf(
				TEXT("%sFExitReference Owner%s = FExitReference(%s);"),
				*Prefix,
				*Suffix,
				*TokenExpression));
			break;
		}
	}

	static void AppendSimpleTerminalRoute(
		FString& Source,
		const FScenarioCase& Scenario,
		const FString& TokenExpression,
		const int32 IndentLevel)
	{
		using namespace AngelscriptNativeTestSupport;

		const FString Prefix = Indent(IndentLevel);
		if (IsScenario(Scenario, "local_early_return"))
		{
			AppendGeneratedAsLine(Source, FString::Printf(
				TEXT("%sif (%s > 0)"),
				*Prefix,
				*TokenExpression));
			AppendGeneratedAsLine(
				Source,
				Prefix + TEXT("{"));
			AppendGeneratedAsLine(Source, FString::Printf(
				TEXT("%s\tReachDestructorExitTerminal(%d);"),
				*Prefix,
				Scenario.RouteId));
			AppendGeneratedAsLine(
				Source,
				Prefix + TEXT("\treturn;"));
			AppendGeneratedAsLine(
				Source,
				Prefix + TEXT("}"));
			return;
		}

		AppendGeneratedAsLine(Source, FString::Printf(
			TEXT("%sReachDestructorExitTerminal(%d);"),
			*Prefix,
			Scenario.RouteId));
		if (IsScenario(Scenario, "local_return")
			|| IsScenario(Scenario, "nested_local_return")
			|| IsScenario(Scenario, "field_return")
			|| IsScenario(Scenario, "base_derived_return")
			|| IsScenario(Scenario, "reference_return"))
		{
			AppendGeneratedAsLine(
				Source,
				Prefix + TEXT("return;"));
		}
	}

	static void AppendLocalControlTerminal(
		FString& Source,
		const FScenarioCase& Scenario,
		const FString& TokenExpression,
		const FString& Suffix,
		const int32 IndentLevel)
	{
		using namespace AngelscriptNativeTestSupport;

		const FString Prefix = Indent(IndentLevel);
		if (IsScenario(Scenario, "local_break"))
		{
			AppendGeneratedAsLine(
				Source,
				Prefix + TEXT("while (true)"));
			AppendGeneratedAsLine(
				Source,
				Prefix + TEXT("{"));
			AppendPassiveOwner(
				Source,
				Scenario,
				TokenExpression,
				Suffix,
				IndentLevel + 1);
			AppendGeneratedAsLine(Source, FString::Printf(
				TEXT("%s\tReachDestructorExitTerminal(%d);"),
				*Prefix,
				Scenario.RouteId));
			AppendGeneratedAsLine(
				Source,
				Prefix + TEXT("\tbreak;"));
			AppendGeneratedAsLine(
				Source,
				Prefix + TEXT("}"));
			return;
		}
		if (IsScenario(Scenario, "local_continue"))
		{
			AppendGeneratedAsLine(
				Source,
				Prefix + TEXT("for (int ExitIndex = 0; ExitIndex < 1; ++ExitIndex)"));
			AppendGeneratedAsLine(
				Source,
				Prefix + TEXT("{"));
			AppendPassiveOwner(
				Source,
				Scenario,
				TokenExpression,
				Suffix,
				IndentLevel + 1);
			AppendGeneratedAsLine(Source, FString::Printf(
				TEXT("%s\tReachDestructorExitTerminal(%d);"),
				*Prefix,
				Scenario.RouteId));
			AppendGeneratedAsLine(
				Source,
				Prefix + TEXT("\tcontinue;"));
			AppendGeneratedAsLine(
				Source,
				Prefix + TEXT("}"));
			return;
		}

		AppendGeneratedAsLine(
			Source,
			Prefix + TEXT("switch (1)"));
		AppendGeneratedAsLine(
			Source,
			Prefix + TEXT("{"));
		AppendGeneratedAsLine(
			Source,
			Prefix + TEXT("case 1:"));
		AppendGeneratedAsLine(
			Source,
			Prefix + TEXT("{"));
		AppendPassiveOwner(
			Source,
			Scenario,
			TokenExpression,
			Suffix,
			IndentLevel + 1);
		AppendGeneratedAsLine(Source, FString::Printf(
			TEXT("%s\tReachDestructorExitTerminal(%d);"),
			*Prefix,
			Scenario.RouteId));
		AppendGeneratedAsLine(
			Source,
			Prefix + TEXT("\tbreak;"));
		AppendGeneratedAsLine(
			Source,
			Prefix + TEXT("}"));
		AppendGeneratedAsLine(
			Source,
			Prefix + TEXT("default:"));
		AppendGeneratedAsLine(
			Source,
			Prefix + TEXT("{"));
		AppendGeneratedAsLine(
			Source,
			Prefix + TEXT("\tbreak;"));
		AppendGeneratedAsLine(
			Source,
			Prefix + TEXT("}"));
		AppendGeneratedAsLine(
			Source,
			Prefix + TEXT("}"));
	}

	static void AppendNestedLocalTerminal(
		FString& Source,
		const FScenarioCase& Scenario,
		const FString& TokenExpression,
		const FString& Suffix,
		const int32 IndentLevel)
	{
		using namespace AngelscriptNativeTestSupport;

		const FString Prefix = Indent(IndentLevel);
		AppendGeneratedAsLine(Source, FString::Printf(
			TEXT("%sFExitValue Outer%s(%s);"),
			*Prefix,
			*Suffix,
			*TokenExpression));
		AppendGeneratedAsLine(
			Source,
			Prefix + TEXT("{"));
		AppendGeneratedAsLine(Source, FString::Printf(
			TEXT("%s\tFExitValue Inner%s(%s + 1);"),
			*Prefix,
			*Suffix,
			*TokenExpression));
		AppendSimpleTerminalRoute(
			Source,
			Scenario,
			TokenExpression,
			IndentLevel + 1);
		AppendGeneratedAsLine(
			Source,
			Prefix + TEXT("}"));
	}

	static void AppendTerminalOwner(
		FString& Source,
		const FScenarioCase& Scenario,
		const FString& TokenExpression,
		const FString& Suffix,
		const int32 IndentLevel)
	{
		using namespace AngelscriptNativeTestSupport;

		const FString Prefix = Indent(IndentLevel);
		if (IsScenario(Scenario, "local_break")
			|| IsScenario(Scenario, "local_continue")
			|| IsScenario(Scenario, "local_switch_exit"))
		{
			AppendLocalControlTerminal(
				Source,
				Scenario,
				TokenExpression,
				Suffix,
				IndentLevel);
			return;
		}
		if (Scenario.Owner == EOwnerKind::NestedLocalValue)
		{
			AppendNestedLocalTerminal(
				Source,
				Scenario,
				TokenExpression,
				Suffix,
				IndentLevel);
			return;
		}
		if (IsScenario(
			Scenario,
			"temporary_early_return"))
		{
			AppendGeneratedAsLine(Source, FString::Printf(
				TEXT("%sif (ObserveExitValue(FExitValue(%s)) == %s)"),
				*Prefix,
				*TokenExpression,
				*TokenExpression));
			AppendGeneratedAsLine(
				Source,
				Prefix + TEXT("{"));
			AppendGeneratedAsLine(Source, FString::Printf(
				TEXT("%s\tReachDestructorExitTerminal(%d);"),
				*Prefix,
				Scenario.RouteId));
			AppendGeneratedAsLine(
				Source,
				Prefix + TEXT("\treturn;"));
			AppendGeneratedAsLine(
				Source,
				Prefix + TEXT("}"));
			return;
		}
		if (IsScenario(Scenario, "temporary_exception"))
		{
			AppendGeneratedAsLine(Source, FString::Printf(
				TEXT("%sConsumeExitTemporaryAndTerminate(FExitValue(%s), %d);"),
				*Prefix,
				*TokenExpression,
				Scenario.RouteId));
			return;
		}
		if (Scenario.Owner == EOwnerKind::ArgumentCopy)
		{
			AppendGeneratedAsLine(Source, FString::Printf(
				TEXT("%sFExitValue Source%s(%s);"),
				*Prefix,
				*Suffix,
				*TokenExpression));
			AppendGeneratedAsLine(Source, FString::Printf(
				TEXT("%sConsumeExitValueByValue(Source%s, %d, true);"),
				*Prefix,
				*Suffix,
				Scenario.RouteId));
			return;
		}

		AppendPassiveOwner(
			Source,
			Scenario,
			TokenExpression,
			Suffix,
			IndentLevel);
		AppendSimpleTerminalRoute(
			Source,
			Scenario,
			TokenExpression,
			IndentLevel);
	}

	static void AppendOneShape(
		FString& Source,
		const FScenarioCase& Scenario,
		const FNestingCase& Nesting)
	{
		using namespace AngelscriptNativeTestSupport;

		AppendGeneratedAsLine(
			Source,
			TEXT("void RunDestructorExit()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendTerminalOwner(
			Source,
			Scenario,
			FString::FromInt(Nesting.TokenBase + 1),
			TEXT("One"),
			1);
		AppendGeneratedAsLine(Source, TEXT("}"));
	}

	static void AppendSequentialShape(
		FString& Source,
		const FScenarioCase& Scenario,
		const FNestingCase& Nesting)
	{
		using namespace AngelscriptNativeTestSupport;

		AppendGeneratedAsLine(
			Source,
			TEXT("void RunDestructorExit()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		for (int32 Index = 1; Index <= Nesting.UnitCount; ++Index)
		{
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			const FString Token =
				FString::FromInt(Nesting.TokenBase + Index);
			const FString Suffix =
				FString::Printf(TEXT("Sequential%d"), Index);
			if (Index == Nesting.UnitCount)
			{
				AppendTerminalOwner(
					Source,
					Scenario,
					Token,
					Suffix,
					2);
			}
			else
			{
				AppendPassiveOwner(
					Source,
					Scenario,
					Token,
					Suffix,
					2);
			}
			AppendGeneratedAsLine(Source, TEXT("\t}"));
		}
		AppendGeneratedAsLine(Source, TEXT("}"));
	}

	static void AppendNestedScopeLevel(
		FString& Source,
		const FScenarioCase& Scenario,
		const FNestingCase& Nesting,
		const int32 Level)
	{
		using namespace AngelscriptNativeTestSupport;

		const FString Prefix = Indent(Level);
		AppendGeneratedAsLine(
			Source,
			Prefix + TEXT("{"));
		const FString Token =
			FString::FromInt(Nesting.TokenBase + Level);
		const FString Suffix =
			FString::Printf(TEXT("Scope%d"), Level);
		if (Level == Nesting.UnitCount)
		{
			AppendTerminalOwner(
				Source,
				Scenario,
				Token,
				Suffix,
				Level + 1);
		}
		else
		{
			AppendPassiveOwner(
				Source,
				Scenario,
				Token,
				Suffix,
				Level + 1);
			AppendNestedScopeLevel(
				Source,
				Scenario,
				Nesting,
				Level + 1);
		}
		AppendGeneratedAsLine(
			Source,
			Prefix + TEXT("}"));
	}

	static void AppendNestedScopesShape(
		FString& Source,
		const FScenarioCase& Scenario,
		const FNestingCase& Nesting)
	{
		using namespace AngelscriptNativeTestSupport;

		AppendGeneratedAsLine(
			Source,
			TEXT("void RunDestructorExit()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendNestedScopeLevel(
			Source,
			Scenario,
			Nesting,
			1);
		AppendGeneratedAsLine(Source, TEXT("}"));
	}

	static void AppendNestedCallsShape(
		FString& Source,
		const FScenarioCase& Scenario,
		const FNestingCase& Nesting)
	{
		using namespace AngelscriptNativeTestSupport;

		for (int32 Level = Nesting.UnitCount; Level >= 1; --Level)
		{
			AppendGeneratedAsLine(Source, FString::Printf(
				TEXT("void RunDestructorExitCall%d()"),
				Level));
			AppendGeneratedAsLine(Source, TEXT("{"));
			const FString Token =
				FString::FromInt(Nesting.TokenBase + Level);
			const FString Suffix =
				FString::Printf(TEXT("Call%d"), Level);
			if (Level == Nesting.UnitCount)
			{
				AppendTerminalOwner(
					Source,
					Scenario,
					Token,
					Suffix,
					1);
			}
			else
			{
				AppendPassiveOwner(
					Source,
					Scenario,
					Token,
					Suffix,
					1);
				AppendGeneratedAsLine(Source, FString::Printf(
					TEXT("\tRunDestructorExitCall%d();"),
					Level + 1));
			}
			AppendGeneratedAsLine(Source, TEXT("}"));
			AppendGeneratedAsLine(Source);
		}
		AppendGeneratedAsLine(
			Source,
			TEXT("void RunDestructorExit()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(
			Source,
			TEXT("\tRunDestructorExitCall1();"));
		AppendGeneratedAsLine(Source, TEXT("}"));
	}

	static void AppendLoopShape(
		FString& Source,
		const FScenarioCase& Scenario,
		const FNestingCase& Nesting)
	{
		using namespace AngelscriptNativeTestSupport;

		AppendGeneratedAsLine(
			Source,
			TEXT("void RunDestructorExit()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, FString::Printf(
			TEXT("\tfor (int ShapeIndex = 1; ShapeIndex <= %d; ++ShapeIndex)"),
			Nesting.UnitCount));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(Source, FString::Printf(
			TEXT("\t\tif (ShapeIndex < %d)"),
			Nesting.UnitCount));
		AppendGeneratedAsLine(Source, TEXT("\t\t{"));
		AppendPassiveOwner(
			Source,
			Scenario,
			FString::Printf(
				TEXT("%d + ShapeIndex"),
				Nesting.TokenBase),
			TEXT("LoopPassive"),
			3);
		AppendGeneratedAsLine(Source, TEXT("\t\t}"));
		AppendGeneratedAsLine(Source, TEXT("\t\telse"));
		AppendGeneratedAsLine(Source, TEXT("\t\t{"));
		AppendTerminalOwner(
			Source,
			Scenario,
			FString::Printf(
				TEXT("%d + ShapeIndex"),
				Nesting.TokenBase),
			TEXT("LoopTerminal"),
			3);
		AppendGeneratedAsLine(Source, TEXT("\t\t}"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source, TEXT("}"));
	}

	static void AppendRecursionShape(
		FString& Source,
		const FScenarioCase& Scenario,
		const FNestingCase& Nesting)
	{
		using namespace AngelscriptNativeTestSupport;

		AppendGeneratedAsLine(
			Source,
			TEXT("void RunDestructorExitRecursive(int Depth)"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tif (Depth <= 1)"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendTerminalOwner(
			Source,
			Scenario,
			FString::Printf(
				TEXT("%d + Depth"),
				Nesting.TokenBase),
			TEXT("RecursiveTerminal"),
			2);
		AppendGeneratedAsLine(Source, TEXT("\t\treturn;"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendPassiveOwner(
			Source,
			Scenario,
			FString::Printf(
				TEXT("%d + Depth"),
				Nesting.TokenBase),
			TEXT("RecursivePassive"),
			1);
		AppendGeneratedAsLine(
			Source,
			TEXT("\tRunDestructorExitRecursive(Depth - 1);"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(
			Source,
			TEXT("void RunDestructorExit()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, FString::Printf(
			TEXT("\tRunDestructorExitRecursive(%d);"),
			Nesting.UnitCount));
		AppendGeneratedAsLine(Source, TEXT("}"));
	}

	static void AppendNonGlobalShape(
		FString& Source,
		const FScenarioCase& Scenario,
		const FNestingCase& Nesting)
	{
		if (FCStringAnsi::Strcmp(
			Nesting.CatalogName,
			"one") == 0)
		{
			AppendOneShape(Source, Scenario, Nesting);
		}
		else if (FCStringAnsi::Strcmp(
			Nesting.CatalogName,
			"sequential") == 0)
		{
			AppendSequentialShape(
				Source,
				Scenario,
				Nesting);
		}
		else if (FCStringAnsi::Strcmp(
			Nesting.CatalogName,
			"nested_scopes") == 0)
		{
			AppendNestedScopesShape(
				Source,
				Scenario,
				Nesting);
		}
		else if (FCStringAnsi::Strcmp(
			Nesting.CatalogName,
			"nested_calls") == 0)
		{
			AppendNestedCallsShape(
				Source,
				Scenario,
				Nesting);
		}
		else if (FCStringAnsi::Strcmp(
			Nesting.CatalogName,
			"loop") == 0)
		{
			AppendLoopShape(
				Source,
				Scenario,
				Nesting);
		}
		else
		{
			AppendRecursionShape(
				Source,
				Scenario,
				Nesting);
		}
	}

	static void AppendGlobalFactoryHelpers(FString& Source)
	{
		using namespace AngelscriptNativeTestSupport;

		AppendGeneratedAsLine(
			Source,
			TEXT("FExitReference BuildExitGlobalCall3(int TokenBase)"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(
			Source,
			TEXT("\treturn FExitReference(TokenBase + 3);"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(
			Source,
			TEXT("FExitReference BuildExitGlobalCall2(int TokenBase)"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(
			Source,
			TEXT("\tFExitReference Local = FExitReference(TokenBase + 2);"));
		AppendGeneratedAsLine(
			Source,
			TEXT("\treturn BuildExitGlobalCall3(TokenBase);"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(
			Source,
			TEXT("FExitReference BuildExitGlobalCall1(int TokenBase)"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(
			Source,
			TEXT("\tFExitReference Local = FExitReference(TokenBase + 1);"));
		AppendGeneratedAsLine(
			Source,
			TEXT("\treturn BuildExitGlobalCall2(TokenBase);"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(
			Source,
			TEXT("FExitReference BuildExitGlobalNestedCall(int Depth, int TokenBase)"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tif (Depth <= 1)"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(
			Source,
			TEXT("\t\treturn FExitReference(TokenBase + Depth);"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(
			Source,
			TEXT("\tFExitReference Local = FExitReference(TokenBase + Depth);"));
		AppendGeneratedAsLine(
			Source,
			TEXT("\treturn BuildExitGlobalNestedCall(Depth - 1, TokenBase);"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(
			Source,
			TEXT("FExitReference BuildExitGlobalLoop(int TokenBase)"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tFExitReference Result;"));
		AppendGeneratedAsLine(
			Source,
			TEXT("\tfor (int Index = 1; Index <= 3; ++Index)"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(
			Source,
			TEXT("\t\tResult = FExitReference(TokenBase + Index);"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source, TEXT("\treturn Result;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
	}

	static void AppendGlobalShape(
		FString& Source,
		const FScenarioCase& Scenario,
		const FNestingCase& Nesting)
	{
		using namespace AngelscriptNativeTestSupport;

		AppendGlobalFactoryHelpers(Source);
		if (FCStringAnsi::Strcmp(
			Nesting.CatalogName,
			"one") == 0)
		{
			AppendGeneratedAsLine(Source, FString::Printf(
				TEXT("FExitReference ExitGlobal = FExitReference(%d);"),
				Nesting.TokenBase + 1));
		}
		else if (FCStringAnsi::Strcmp(
			Nesting.CatalogName,
			"sequential") == 0)
		{
			for (int32 Index = 1;
				Index <= Nesting.UnitCount;
				++Index)
			{
				AppendGeneratedAsLine(Source, FString::Printf(
					TEXT("FExitReference ExitGlobal%d = FExitReference(%d);"),
					Index,
					Nesting.TokenBase + Index));
			}
		}
		else if (FCStringAnsi::Strcmp(
			Nesting.CatalogName,
			"nested_scopes") == 0)
		{
			AppendGeneratedAsLine(Source, FString::Printf(
				TEXT("FExitFieldOwner ExitGlobal = FExitFieldOwner(%d);"),
				Nesting.TokenBase + 1));
		}
		else if (FCStringAnsi::Strcmp(
			Nesting.CatalogName,
			"loop") == 0)
		{
			AppendGeneratedAsLine(Source, FString::Printf(
				TEXT("FExitReference ExitGlobal = BuildExitGlobalLoop(%d);"),
				Nesting.TokenBase));
		}
		else if (FCStringAnsi::Strcmp(
			Nesting.CatalogName,
			"nested_calls") == 0)
		{
			AppendGeneratedAsLine(Source, FString::Printf(
				TEXT("FExitReference ExitGlobal = BuildExitGlobalCall1(%d);"),
				Nesting.TokenBase));
		}
		else
		{
			AppendGeneratedAsLine(Source, FString::Printf(
				TEXT("FExitReference ExitGlobal = BuildExitGlobalNestedCall(%d, %d);"),
				Nesting.UnitCount,
				Nesting.TokenBase));
		}
		AppendGeneratedAsLine(Source, FString::Printf(
			TEXT("int ExitGlobalReady = RecordDestructorExitGlobalReady(%d);"),
			Scenario.RouteId));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(
			Source,
			TEXT("void RunDestructorExit()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(
			Source,
			TEXT("\tif (ExitGlobalReady <= 0)"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(
			Source,
			TEXT("\t\tReachDestructorExitTerminal(-1);"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source, TEXT("}"));
	}

	static void AppendRecoveryFunction(FString& Source)
	{
		using namespace AngelscriptNativeTestSupport;

		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(
			Source,
			TEXT("int RunDestructorExitRecovery()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(
			Source,
			TEXT("\tFExitReference Recovery = FExitReference(909);"));
		AppendGeneratedAsLine(
			Source,
			TEXT("\treturn Recovery.GetToken();"));
		AppendGeneratedAsLine(Source, TEXT("}"));
	}

	static FString BuildDestructorExitSource(
		const FScenarioCase& Scenario,
		const FNestingCase& Nesting)
	{
		FString Source;
		AppendCommonTypesAndHelpers(Source);
		if (IsGlobalScenario(Scenario))
		{
			AppendGlobalShape(
				Source,
				Scenario,
				Nesting);
		}
		else
		{
			AppendNonGlobalShape(
				Source,
				Scenario,
				Nesting);
		}
		AppendRecoveryFunction(Source);
		return Source;
	}

	static FString BuildDestructorExitRecoverySource()
	{
		FString Source;
		AppendCommonTypesAndHelpers(Source);
		AppendRecoveryFunction(Source);
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

		PrintGeneratedAsSource(
			Test,
			SourceId,
			ModuleName,
			Source);
		const FTCHARToUTF8 ModuleNameUtf8(*ModuleName);
		const FTCHARToUTF8 SourceUtf8(*Source);
		return CompileNativeModule(
			&ScriptEngine,
			ModuleNameUtf8.Get(),
			SourceUtf8.Get(),
			OutModule);
	}

	static int32 ExpectedExecutionState(
		const ETerminalKind Terminal)
	{
		switch (Terminal)
		{
		case ETerminalKind::Exception:
			return asEXECUTION_EXCEPTION;
		case ETerminalKind::Abort:
		case ETerminalKind::Unprepare:
			// Abort and Suspend are explicit asERROR stubs in this fork. The
			// active characterization therefore observes the normal completion
			// state; future native terminal semantics remain a separate upgrade
			// target.
			return asEXECUTION_FINISHED;
		case ETerminalKind::Normal:
		case ETerminalKind::ModuleDiscard:
		case ETerminalKind::EngineShutdown:
		default:
			return asEXECUTION_FINISHED;
		}
	}

	static bool TerminalMayHaveNoLiveOwner(
		const FScenarioCase& Scenario)
	{
		return IsScenario(
			Scenario,
			"temporary_statement_end")
			|| IsScenario(
				Scenario,
				"temporary_early_return")
			|| IsScenario(
				Scenario,
				"returned_value_discard");
	}

	void VerifyMetadata(
		const FNativeCaseContext& Case,
		const FScenarioCase& Scenario,
		asIScriptModule& Module)
	{
		asIScriptFunction* const Entry =
			Module.GetFunctionByDecl(
				"void RunDestructorExit()");
		asIScriptFunction* const Recovery =
			Module.GetFunctionByDecl(
				"int RunDestructorExitRecovery()");
		ASSERT_THAT(IsNotNull(Entry,
			*Case.Describe(TEXT("owner-exit source should publish its exact entry"))));
		ASSERT_THAT(IsNotNull(Recovery,
			*Case.Describe(TEXT("owner-exit source should publish recovery"))));
		if (Entry != nullptr)
		{
			ASSERT_THAT(IsNotNull(
				Entry->GetScriptSectionName(),
				*Case.Describe(TEXT("owner-exit entry should retain source metadata"))));
		}

		asITypeInfo* const ValueType =
			Module.GetTypeInfoByName("FExitValue");
		asITypeInfo* const ReferenceType =
			Module.GetTypeInfoByName("FExitReference");
		ASSERT_THAT(IsNotNull(ValueType,
			*Case.Describe(TEXT("owner-exit source should publish its value owner type"))));
		ASSERT_THAT(IsNotNull(ReferenceType,
			*Case.Describe(TEXT("owner-exit source should publish its reference owner type"))));
		if (ValueType != nullptr)
		{
			ASSERT_THAT(IsTrue(
				(ValueType->GetFlags() & asOBJ_VALUE) != 0,
				*Case.Describe(TEXT("owner-exit value fixture should retain value semantics"))));
		}
		if (ReferenceType != nullptr)
		{
			ASSERT_THAT(IsTrue(
				(ReferenceType->GetFlags() & asOBJ_REF) != 0,
				*Case.Describe(TEXT("owner-exit reference fixture should retain reference semantics"))));
		}
		if (Scenario.Owner == EOwnerKind::BaseDerivedOwner)
		{
			asITypeInfo* const Base =
				Module.GetTypeInfoByName("FExitBaseOwner");
			asITypeInfo* const Derived =
				Module.GetTypeInfoByName(
					"FExitDerivedOwner");
			ASSERT_THAT(IsNotNull(Base,
				*Case.Describe(TEXT("base-derived exit scenario should publish its base type"))));
			ASSERT_THAT(IsNotNull(Derived,
				*Case.Describe(TEXT("base-derived exit scenario should publish its derived type"))));
			if (Base != nullptr && Derived != nullptr)
			{
				ASSERT_THAT(AreEqual(
					Base,
					Derived->GetBaseType(),
					*Case.Describe(TEXT("base-derived exit scenario should retain exact inheritance metadata"))));
			}
		}
		if (IsGlobalScenario(Scenario))
		{
			ASSERT_THAT(IsTrue(
				Module.GetGlobalVarCount() >= 2,
				*Case.Describe(TEXT("global owner-exit source should publish owner and readiness globals"))));
		}
	}

	void VerifyEventOrder(
		const FNativeCaseContext& Case,
		const FScenarioCase& Scenario,
		const FDestructorExitState& State,
		const FNativeLifecycleRecorder& Lifecycle)
	{
		const TArray<int32> ExpectedRouteMarkers =
		{
			Scenario.RouteId,
		};
		ASSERT_THAT(AreEqual(
			1,
			State.TerminalCalls,
			*Case.Describe(TEXT("owner-exit workflow should cross its terminal boundary exactly once"))));
		ASSERT_THAT(AreEqual(
			ExpectedRouteMarkers,
			State.RouteMarkers,
			*Case.Describe(TEXT("owner-exit workflow should retain the exact selected route marker"))));
		ASSERT_THAT(IsTrue(
			State.ConstructsAtTerminal >= 0
				&& State.DestructsAtTerminal >= 0,
			*Case.Describe(TEXT("owner-exit workflow should snapshot construction and destruction at its terminal boundary"))));
		if (TerminalMayHaveNoLiveOwner(Scenario))
		{
			ASSERT_THAT(AreEqual(
				0,
				State.LiveAtTerminal,
				*Case.Describe(TEXT("statement-ended temporary or discarded return should retire before the terminal route"))));
		}
		else
		{
			ASSERT_THAT(IsTrue(
				State.LiveAtTerminal > 0,
				*Case.Describe(TEXT("owner-exit terminal route should observe a live selected owner"))));
		}

		TMap<int32, int32> ConstructionIndexById;
		const TArray<FNativeLifecycleEntry>& Entries =
			Lifecycle.GetEntries();
		for (int32 Index = 0; Index < Entries.Num(); ++Index)
		{
			const FNativeLifecycleEntry& Entry = Entries[Index];
			if (Entry.Event
					== ENativeLifecycleEvent::ValueConstruct
				|| Entry.Event
					== ENativeLifecycleEvent::CopyConstruct)
			{
				ConstructionIndexById.Add(
					Entry.ObjectId,
					Index);
			}
			else if (Entry.Event
				== ENativeLifecycleEvent::Destruct)
			{
				const int32* const ConstructionIndex =
					ConstructionIndexById.Find(
						Entry.ObjectId);
				ASSERT_THAT(IsNotNull(
					ConstructionIndex,
					*Case.Describe(TEXT("owner-exit destruction should reference a prior construction event"))));
				if (ConstructionIndex != nullptr)
				{
					ASSERT_THAT(IsTrue(
						*ConstructionIndex < Index,
						*Case.Describe(TEXT("owner-exit destruction should occur after its own construction"))));
				}
			}
		}

		if (Scenario.Owner == EOwnerKind::FieldOwner
			|| Scenario.Owner
				== EOwnerKind::BaseDerivedOwner)
		{
			TArray<int32> DestructionValues;
			for (const FNativeLifecycleEntry& Entry : Entries)
			{
				if (Entry.Event
					== ENativeLifecycleEvent::Destruct)
				{
					DestructionValues.Add(Entry.Value);
				}
			}
			for (int32 Index = 0;
				Index + 2 < DestructionValues.Num();
				++Index)
			{
				if (DestructionValues[Index] % 10 == 3)
				{
					ASSERT_THAT(AreEqual(
						DestructionValues[Index] - 1,
						DestructionValues[Index + 1],
						*Case.Describe(TEXT("field and derived cleanup should destroy the middle field after the last field"))));
					ASSERT_THAT(AreEqual(
						DestructionValues[Index] - 2,
						DestructionValues[Index + 2],
						*Case.Describe(TEXT("field and derived cleanup should destroy the base or first field last"))));
				}
			}
		}
	}

	void VerifyOwnershipOnce(
		const FNativeCaseContext& Case,
		const FScenarioCase& Scenario,
		const FNativeLifecycleRecorder& Lifecycle)
	{
		const int32 Constructs =
			ConstructionCount(Lifecycle);
		const int32 Destructs =
			Lifecycle.Num(ENativeLifecycleEvent::Destruct);
		ASSERT_THAT(IsTrue(
			Constructs > 0,
			*Case.Describe(TEXT("owner-exit workflow should construct at least one tracked owner"))));
		ASSERT_THAT(AreEqual(
			Constructs,
			Destructs,
			*Case.Describe(TEXT("owner-exit workflow should destroy every constructed and copied identity once"))));
		ASSERT_THAT(AreEqual(
			0,
			Lifecycle.GetLiveObjectCount(),
			*Case.Describe(TEXT("owner-exit workflow should leave no live tracked identity after cleanup"))));

		TSet<int32> ConstructedIds;
		TSet<int32> DestructedIds;
		for (const FNativeLifecycleEntry& Entry :
			Lifecycle.GetEntries())
		{
			if (Entry.Event
					== ENativeLifecycleEvent::ValueConstruct
				|| Entry.Event
					== ENativeLifecycleEvent::CopyConstruct)
			{
				ASSERT_THAT(IsFalse(
					ConstructedIds.Contains(
						Entry.ObjectId),
					*Case.Describe(TEXT("owner-exit workflow should allocate each tracked identity once"))));
				ConstructedIds.Add(Entry.ObjectId);
				if (Entry.Event
					== ENativeLifecycleEvent::CopyConstruct)
				{
					ASSERT_THAT(IsTrue(
						ConstructedIds.Contains(
							Entry.RelatedObjectId),
						*Case.Describe(TEXT("owner-exit copy should identify an already constructed source"))));
				}
			}
			else if (Entry.Event
				== ENativeLifecycleEvent::Destruct)
			{
				ASSERT_THAT(IsTrue(
					ConstructedIds.Contains(
						Entry.ObjectId),
					*Case.Describe(TEXT("owner-exit workflow should destroy only constructed identities"))));
				ASSERT_THAT(IsFalse(
					DestructedIds.Contains(
						Entry.ObjectId),
					*Case.Describe(TEXT("owner-exit workflow should never destroy one identity twice"))));
				DestructedIds.Add(Entry.ObjectId);
			}
		}
		ASSERT_THAT(AreEqual(
			ConstructedIds.Num(),
			DestructedIds.Num(),
			*Case.Describe(TEXT("owner-exit constructed and destructed identity sets should match"))));
		if (Scenario.Owner == EOwnerKind::ArgumentCopy)
		{
			ASSERT_THAT(AreEqual(
				0,
				Lifecycle.Num(ENativeLifecycleEvent::CopyConstruct),
				*Case.Describe(TEXT("current fork normalizes script value parameters to const inout references without a copy"))));
		}
		if (Scenario.Owner == EOwnerKind::ReferenceAlias)
		{
			ASSERT_THAT(AreEqual(
				0,
				Lifecycle.Num(
					ENativeLifecycleEvent::CopyConstruct),
				*Case.Describe(TEXT("reference aliases should not create a second script object identity"))));
		}
	}

	void VerifyContextTerminal(
		const FNativeCaseContext& Case,
		const FScenarioCase& Scenario,
		const FDestructorExitState& State,
		asIScriptContext& Context,
		const int32 ExecutionState)
	{
		const int32 Expected =
			ExpectedExecutionState(Scenario.Terminal);
		ASSERT_THAT(AreEqual(
			Expected,
			ExecutionState,
			*Case.Describe(TEXT("owner-exit execution state should match its selected route"))));
		ASSERT_THAT(AreEqual(
			Expected,
			Context.GetState(),
			*Case.Describe(TEXT("owner-exit context should retain the selected terminal state"))));
		if (Scenario.Terminal == ETerminalKind::Abort
			|| Scenario.Terminal == ETerminalKind::Unprepare)
		{
			ASSERT_THAT(AreEqual(
				asERROR,
				State.TerminalRequestResult,
				*Case.Describe(TEXT("current fork should report the unsupported terminal request explicitly"))));
		}
		if (Scenario.Terminal == ETerminalKind::Exception)
		{
			ASSERT_THAT(AreEqual(
				FString(TEXT("Destructor owner-exit exception sentinel")),
				FString(UTF8_TO_TCHAR(
					Context.GetExceptionString())),
				*Case.Describe(TEXT("owner-exit exception should retain its exact owner"))));
			ASSERT_THAT(IsTrue(
				Context.GetExceptionLineNumber() > 0,
				*Case.Describe(TEXT("owner-exit exception should retain a source location"))));
		}
		else if (Scenario.Terminal == ETerminalKind::Abort
			|| Scenario.Terminal
				== ETerminalKind::Unprepare)
		{
			ASSERT_THAT(IsTrue(
				Context.GetExceptionString() == nullptr
					|| Context.GetExceptionString()[0] == '\0',
				*Case.Describe(TEXT("abort and host-unprepare routes should not fabricate an exception"))));
		}
	}

	void ExecuteSameContextRecovery(
		const FNativeCaseContext& Case,
		asIScriptContext& Context,
		asIScriptFunction& Recovery,
		FNativeLifecycleRecorder& Lifecycle,
		const FDestructorExitState& State)
	{
		const int32 RoutesBeforeRecovery =
			State.RouteMarkers.Num();
		const int32 ConstructsBeforeRecovery =
			ConstructionCount(Lifecycle);
		const int32 DestructsBeforeRecovery =
			Lifecycle.Num(
				ENativeLifecycleEvent::Destruct);
		ASSERT_THAT(IsTrue(Context.Prepare(&Recovery) >= 0,
			*Case.Describe(TEXT("owner-exit context should prepare same-context recovery"))));
		ASSERT_THAT(AreEqual(
			static_cast<int32>(asEXECUTION_FINISHED),
			Context.Execute(),
			*Case.Describe(TEXT("owner-exit same-context recovery should finish"))));
		ASSERT_THAT(AreEqual(
			909,
			static_cast<int32>(
				Context.GetReturnDWord()),
			*Case.Describe(TEXT("owner-exit recovery should return its sentinel"))));
		ASSERT_THAT(AreEqual(
			asSUCCESS,
			Context.Unprepare(),
			*Case.Describe(TEXT("owner-exit recovery should unprepare cleanly"))));
		ASSERT_THAT(AreEqual(
			RoutesBeforeRecovery,
			State.RouteMarkers.Num(),
			*Case.Describe(TEXT("owner-exit recovery should replay no terminal route"))));
		ASSERT_THAT(AreEqual(
			ConstructsBeforeRecovery + 1,
			ConstructionCount(Lifecycle),
			*Case.Describe(TEXT("owner-exit recovery should construct one independent reference"))));
		ASSERT_THAT(AreEqual(
			DestructsBeforeRecovery + 1,
			Lifecycle.Num(
				ENativeLifecycleEvent::Destruct),
			*Case.Describe(TEXT("owner-exit recovery should destroy its reference once"))));
		ASSERT_THAT(AreEqual(
			0,
			Lifecycle.GetLiveObjectCount(),
			*Case.Describe(TEXT("owner-exit recovery should leave no live identity"))));
	}

	void CompileAndExecuteFreshRecovery(
		const FNativeCaseContext& Case,
		FNativeTestEngine& Engine,
		asIScriptEngine& ScriptEngine,
		const FString& ModuleName,
		FNativeLifecycleRecorder& Lifecycle,
		FDestructorExitState& State)
	{
		const FString Source =
			BuildDestructorExitRecoverySource();
		Engine.ResetMessages();
		asIScriptModule* Module = nullptr;
		ASSERT_THAT(IsTrue(CompileAndReport(
			*TestRunner,
			ScriptEngine,
			Case.GetId() + TEXT("-RECOVERY"),
			ModuleName,
			Source,
			Module) >= 0,
			*Case.Describe(TEXT("owner-exit fresh recovery source should compile"))));
		ASSERT_THAT(IsNotNull(Module,
			*Case.Describe(TEXT("owner-exit fresh recovery should publish its module"))));
		if (Module == nullptr)
		{
			return;
		}

		asIScriptFunction* const Recovery =
			Module->GetFunctionByDecl(
				"int RunDestructorExitRecovery()");
		ASSERT_THAT(IsNotNull(Recovery,
			*Case.Describe(TEXT("owner-exit fresh recovery should publish its entry"))));
		asIScriptContext* const Context =
			ScriptEngine.CreateContext();
		ASSERT_THAT(IsNotNull(Context,
			*Case.Describe(TEXT("owner-exit fresh recovery should create a context"))));
		if (Recovery != nullptr && Context != nullptr)
		{
			ExecuteSameContextRecovery(
				Case,
				*Context,
				*Recovery,
				Lifecycle,
				State);
			Context->Release();
		}
		else if (Context != nullptr)
		{
			Context->Release();
		}

		const FTCHARToUTF8 ModuleNameUtf8(*ModuleName);
		ScriptEngine.DiscardModule(
			ModuleNameUtf8.Get());
	}

	void RunNonGlobal(
		const TStaticArray<FNativeCaseContext, 3>& Cases,
		const FScenarioCase& Scenario,
		asIScriptEngine& ScriptEngine,
		asIScriptModule& Module,
		FNativeLifecycleRecorder& Lifecycle,
		FDestructorExitState& State)
	{
		asIScriptFunction* const Entry =
			Module.GetFunctionByDecl(
				"void RunDestructorExit()");
		asIScriptFunction* const Recovery =
			Module.GetFunctionByDecl(
				"int RunDestructorExitRecovery()");
		ASSERT_THAT(IsNotNull(Entry,
			*Cases[0].Describe(TEXT("owner-exit module should publish its primary entry"))));
		ASSERT_THAT(IsNotNull(Recovery,
			*Cases[2].Describe(TEXT("owner-exit module should publish recovery"))));
		if (Entry == nullptr || Recovery == nullptr)
		{
			return;
		}

		asIScriptContext* const Context =
			ScriptEngine.CreateContext();
		ASSERT_THAT(IsNotNull(Context,
			*Cases[2].Describe(TEXT("owner-exit workflow should create a reusable context"))));
		if (Context == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsTrue(Context->Prepare(Entry) >= 0,
			*Cases[2].Describe(TEXT("owner-exit context should prepare its entry"))));
		const int32 ExecutionState = Context->Execute();
		VerifyContextTerminal(
			Cases[2],
			Scenario,
			State,
			*Context,
			ExecutionState);
		ASSERT_THAT(AreEqual(
			asSUCCESS,
			Context->Unprepare(),
			*Cases[2].Describe(TEXT("owner-exit context should unprepare and release its stack"))));
		VerifyEventOrder(
			Cases[0],
			Scenario,
			State,
			Lifecycle);
		VerifyOwnershipOnce(
			Cases[1],
			Scenario,
			Lifecycle);
		ExecuteSameContextRecovery(
			Cases[2],
			*Context,
			*Recovery,
			Lifecycle,
			State);
		Context->Release();
	}

	void ExecuteGlobalProbe(
		const FNativeCaseContext& Case,
		asIScriptEngine& ScriptEngine,
		asIScriptModule& Module)
	{
		asIScriptFunction* const Entry =
			Module.GetFunctionByDecl(
				"void RunDestructorExit()");
		ASSERT_THAT(IsNotNull(Entry,
			*Case.Describe(TEXT("global owner-exit module should publish its probe entry"))));
		asIScriptContext* const Context =
			ScriptEngine.CreateContext();
		ASSERT_THAT(IsNotNull(Context,
			*Case.Describe(TEXT("global owner-exit probe should create a context"))));
		if (Entry != nullptr && Context != nullptr)
		{
			ASSERT_THAT(AreEqual(
				static_cast<int32>(
					asEXECUTION_FINISHED),
				AngelscriptNativeTestSupport::PrepareAndExecute(
					Context,
					Entry),
				*Case.Describe(TEXT("global owner-exit probe should finish before teardown"))));
			ASSERT_THAT(AreEqual(
				asSUCCESS,
				Context->Unprepare(),
				*Case.Describe(TEXT("global owner-exit probe should unprepare cleanly"))));
			Context->Release();
		}
		else if (Context != nullptr)
		{
			Context->Release();
		}
	}

	void RunGlobal(
		const TStaticArray<FNativeCaseContext, 3>& Cases,
		const FScenarioCase& Scenario,
		FNativeTestEngine& Engine,
		asIScriptEngine& ScriptEngine,
		asIScriptModule& Module,
		const FString& ModuleName,
		FNativeLifecycleRecorder& Lifecycle,
		FDestructorExitState& State)
	{
		ASSERT_THAT(AreEqual(
			1,
			State.GlobalReadyCalls,
			*Cases[0].Describe(TEXT("global owner-exit source should reach readiness exactly once"))));
		ASSERT_THAT(IsTrue(
			State.LiveAtTerminal > 0,
			*Cases[0].Describe(TEXT("global owner-exit source should retain live module-owned storage"))));
		ExecuteGlobalProbe(
			Cases[2],
			ScriptEngine,
			Module);

		const FTCHARToUTF8 ModuleNameUtf8(*ModuleName);
		if (Scenario.Terminal
			== ETerminalKind::ModuleDiscard)
		{
			ASSERT_THAT(IsTrue(
				ScriptEngine.DiscardModule(
					ModuleNameUtf8.Get()) >= 0,
				*Cases[2].Describe(TEXT("module-global owner should discard cleanly"))));
			ASSERT_THAT(IsNull(ScriptEngine.GetModule(
				ModuleNameUtf8.Get(),
				asGM_ONLY_IF_EXISTS),
				*Cases[2].Describe(TEXT("module-global discard should remove module metadata"))));
			VerifyEventOrder(
				Cases[0],
				Scenario,
				State,
				Lifecycle);
			VerifyOwnershipOnce(
				Cases[1],
				Scenario,
				Lifecycle);
			CompileAndExecuteFreshRecovery(
				Cases[2],
				Engine,
				ScriptEngine,
				ModuleName,
				Lifecycle,
				State);
			return;
		}

		Engine.Destroy();
		VerifyEventOrder(
			Cases[0],
			Scenario,
			State,
			Lifecycle);
		VerifyOwnershipOnce(
			Cases[1],
			Scenario,
			Lifecycle);
		Engine.Create(*TestRunner);
		asIScriptEngine* const RecoveryEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(RecoveryEngine,
			*Cases[2].Describe(TEXT("engine-global teardown should allow a fresh raw SDK engine"))));
		if (RecoveryEngine == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(RegisterDestructorExitBridge(
			*RecoveryEngine,
			State),
			*Cases[2].Describe(TEXT("fresh owner-exit engine should register its route bridge"))));
		ASSERT_THAT(IsTrue(RegisterNativeScriptLifecycleBridge(
			*RecoveryEngine,
			Lifecycle),
			*Cases[2].Describe(TEXT("fresh owner-exit engine should register lifecycle callbacks"))));
		CompileAndExecuteFreshRecovery(
			Cases[2],
			Engine,
			*RecoveryEngine,
			ModuleName,
			Lifecycle,
			State);
	}

	void RunCell(
		const FScenarioCase& Scenario,
		const FNestingCase& Nesting)
	{
		using namespace AngelscriptNativeTestSupport;

		TStaticArray<FNativeCaseContext, 3> Cases =
		{
			FNativeCaseContext(MakeNativeCaseId(
				"LANG-DTOR-OWNER-EXIT",
				{
					ANSI_TO_TCHAR(
						Scenario.CatalogName),
					ANSI_TO_TCHAR(
						Nesting.CatalogName),
					ANSI_TO_TCHAR(
						ObservationCases[0].CatalogName),
				})),
			FNativeCaseContext(MakeNativeCaseId(
				"LANG-DTOR-OWNER-EXIT",
				{
					ANSI_TO_TCHAR(
						Scenario.CatalogName),
					ANSI_TO_TCHAR(
						Nesting.CatalogName),
					ANSI_TO_TCHAR(
						ObservationCases[1].CatalogName),
				})),
			FNativeCaseContext(MakeNativeCaseId(
				"LANG-DTOR-OWNER-EXIT",
				{
					ANSI_TO_TCHAR(
						Scenario.CatalogName),
					ANSI_TO_TCHAR(
						Nesting.CatalogName),
					ANSI_TO_TCHAR(
						ObservationCases[2].CatalogName),
				})),
		};

		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };
		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine,
			*Cases[0].Describe(TEXT("owner-exit cell should create an isolated raw SDK engine"))));
		if (ScriptEngine == nullptr)
		{
			return;
		}

		FNativeLifecycleRecorder Lifecycle;
		FDestructorExitState State;
		State.Reset(Lifecycle, Scenario.Terminal);
		ASSERT_THAT(IsTrue(RegisterDestructorExitBridge(
			*ScriptEngine,
			State),
			*Cases[0].Describe(TEXT("owner-exit cell should register its terminal bridge"))));
		ASSERT_THAT(IsTrue(RegisterNativeScriptLifecycleBridge(
			*ScriptEngine,
			Lifecycle),
			*Cases[0].Describe(TEXT("owner-exit cell should register script lifecycle callbacks"))));

		const FString ModuleName = FString::Printf(
			TEXT("DestructorExit_%hs_%hs"),
			Scenario.CatalogName,
			Nesting.CatalogName);
		const FString Source =
			BuildDestructorExitSource(
				Scenario,
				Nesting);
		Engine.ResetMessages();
		Lifecycle.Reset();
		State.Reset(Lifecycle, Scenario.Terminal);
		asIScriptModule* Module = nullptr;
		const int CompileResult = CompileAndReport(
			*TestRunner,
			*ScriptEngine,
			Cases[0].GetId(),
			ModuleName,
			Source,
			Module);
		if (IsGlobalScenario(Scenario))
		{
			ASSERT_THAT(IsTrue(
				CompileResult < 0,
				*Cases[0].Describe(TEXT("current fork should reject mutable module-global owner teardown"))));
			ASSERT_THAT(IsTrue(
				Engine.GetMessages().Entries.ContainsByPredicate(
					[](const FNativeMessageEntry& Entry)
					{
						return Entry.Type == asMSGTYPE_ERROR;
					}),
				*Cases[0].Describe(TEXT("global owner rejection should publish a compiler diagnostic"))));
			if (Module != nullptr)
			{
				const FTCHARToUTF8 ModuleNameUtf8(*ModuleName);
				ScriptEngine->DiscardModule(ModuleNameUtf8.Get());
			}
			ASSERT_THAT(IsNull(
				ScriptEngine->GetModule(
					FTCHARToUTF8(*ModuleName).Get(),
					asGM_ONLY_IF_EXISTS),
				*Cases[2].Describe(TEXT("rejected global owner module should discard without residual metadata"))));
			CompileAndExecuteFreshRecovery(
				Cases[2],
				Engine,
				*ScriptEngine,
				ModuleName,
				Lifecycle,
				State);
			return;
		}
		ASSERT_THAT(IsTrue(
			CompileResult >= 0,
			*Cases[0].Describe(TEXT("owner-exit generated source should compile"))));
		ASSERT_THAT(IsNotNull(Module,
			*Cases[0].Describe(TEXT("owner-exit generated source should publish its module"))));
		if (Module == nullptr)
		{
			return;
		}

		VerifyMetadata(
			Cases[0],
			Scenario,
			*Module);
		if (IsGlobalScenario(Scenario))
		{
			RunGlobal(
				Cases,
				Scenario,
				Engine,
				*ScriptEngine,
				*Module,
				ModuleName,
				Lifecycle,
				State);
			return;
		}

		RunNonGlobal(
			Cases,
			Scenario,
			*ScriptEngine,
			*Module,
			Lifecycle,
			State);
		const FTCHARToUTF8 ModuleNameUtf8(*ModuleName);
		ASSERT_THAT(IsTrue(
			ScriptEngine->DiscardModule(
				ModuleNameUtf8.Get()) >= 0,
			*Cases[2].Describe(TEXT("owner-exit module should discard cleanly"))));
		ASSERT_THAT(IsNull(ScriptEngine->GetModule(
			ModuleNameUtf8.Get(),
			asGM_ONLY_IF_EXISTS),
			*Cases[2].Describe(TEXT("owner-exit discard should remove module metadata"))));
		ASSERT_THAT(AreEqual(
			0,
			Lifecycle.GetLiveObjectCount(),
			*Cases[2].Describe(TEXT("owner-exit isolation should retain no tracked object"))));
	}

public:
	TEST_METHOD(ScenariosByNestingAndObservation)
	{
		AS_NATIVE_PRODUCT("LANG-DTOR-OWNER-EXIT",
			AngelscriptNativeTestSupport::ENativeEvidence::Runtime
				| AngelscriptNativeTestSupport::ENativeEvidence::Metadata
				| AngelscriptNativeTestSupport::ENativeEvidence::Lifecycle
				| AngelscriptNativeTestSupport::ENativeEvidence::Diagnostic
				| AngelscriptNativeTestSupport::ENativeEvidence::Cleanup
				| AngelscriptNativeTestSupport::ENativeEvidence::Isolation);

		for (const FScenarioCase& Scenario : ScenarioCases)
		{
			for (const FNestingCase& Nesting : NestingCases)
			{
				RunCell(
					Scenario,
					Nesting);
			}
		}
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
