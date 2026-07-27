#include "../../Support/AngelscriptNativeFixtureTestSupport.h"
#include "../../Support/AngelscriptNativeLanguageCaseTestSupport.h"

#include "CQTest.h"
#include "Misc/ScopeExit.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FConstructorBoundaryTests,
	"Angelscript.TestModule.AngelScriptSDK.Language.Constructors.Boundary",
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
	using FNativeMessageEntry =
		AngelscriptNativeTestSupport::FNativeMessageEntry;
	using FNativeTestEngine =
		AngelscriptNativeTestSupport::FNativeTestEngine;

	static constexpr asPWORD ConstructorBoundaryStateUserDataSlot =
		static_cast<asPWORD>(0x43544F52424E4459ull);

	enum class EBoundaryOutcome : uint8
	{
		CompileAccepted,
		CompileRejected,
		RuntimeSuccess,
		RuntimeException,
		ModuleDiscard,
		EngineShutdown,
	};

	struct FScenarioCase
	{
		const ANSICHAR* CatalogName;
		EBoundaryOutcome Outcome;
		const TCHAR* DiagnosticToken;
		const TCHAR* AlternateDiagnosticToken;
	};

	struct FObservationCase
	{
		const ANSICHAR* CatalogName;
	};

	inline static constexpr FScenarioCase ScenarioCases[] =
	{
		{
			"duplicate_default_signature",
			EBoundaryOutcome::CompileAccepted,
			nullptr,
			nullptr,
		},
		{
			"duplicate_parameter_signature",
			EBoundaryOutcome::CompileAccepted,
			nullptr,
			nullptr,
		},
		{
			"mismatched_constructor_name",
			EBoundaryOutcome::CompileRejected,
			TEXT("missing the return type"),
			TEXT("constructor"),
		},
		{
			"constructor_value_return",
			EBoundaryOutcome::CompileRejected,
			TEXT("Can't return value"),
			TEXT("return type is 'void'"),
		},
		{
			"super_outside_constructor",
			EBoundaryOutcome::CompileRejected,
			TEXT("super"),
			TEXT("not declared"),
		},
		{
			"super_after_statement",
			EBoundaryOutcome::RuntimeSuccess,
			nullptr,
			nullptr,
		},
		{
			"repeated_super",
			EBoundaryOutcome::CompileRejected,
			TEXT("constructor multiple times"),
			TEXT("super"),
		},
		{
			"missing_base_argument",
			EBoundaryOutcome::CompileRejected,
			TEXT("Base class doesn't have default constructor"),
			TEXT("Make explicit call to base constructor"),
		},
		{
			"ambiguous_base_argument",
			EBoundaryOutcome::CompileRejected,
			TEXT("Multiple matching signatures"),
			TEXT("Candidates are"),
		},
		{
			"inaccessible_base",
			EBoundaryOutcome::CompileRejected,
			TEXT("private method"),
			TEXT("private"),
		},
		{
			"recursive_value_field_direct",
			EBoundaryOutcome::CompileRejected,
			TEXT("Illegal member type"),
			TEXT("recursive"),
		},
		{
			"recursive_value_field_indirect",
			EBoundaryOutcome::CompileRejected,
			TEXT("Illegal member type"),
			TEXT("recursive"),
		},
		{
			"recursive_constructor_direct",
			EBoundaryOutcome::RuntimeException,
			nullptr,
			nullptr,
		},
		{
			"recursive_constructor_indirect",
			EBoundaryOutcome::RuntimeException,
			nullptr,
			nullptr,
		},
		{
			"mutable_reference_global",
			EBoundaryOutcome::CompileRejected,
			TEXT("must be const"),
			TEXT("Class types are not supported"),
		},
		{
			"module_discard_const_value_global",
			EBoundaryOutcome::ModuleDiscard,
			nullptr,
			nullptr,
		},
		{
			"engine_shutdown_const_value_global",
			EBoundaryOutcome::EngineShutdown,
			nullptr,
			nullptr,
		},
	};

	inline static constexpr FObservationCase ObservationCases[] =
	{
		{ "compile_or_execution_state" },
		{ "diagnostic_or_metadata" },
		{ "lifecycle_cleanup" },
		{ "recovery_or_teardown" },
	};

	struct FConstructorBoundaryState
	{
		TArray<int32> Markers;

		void Reset()
		{
			Markers.Reset();
		}
	};

	static bool IsScenario(
		const FScenarioCase& Scenario,
		const ANSICHAR* Name)
	{
		return FCStringAnsi::Strcmp(Scenario.CatalogName, Name) == 0;
	}

	static bool IsCompileRejected(const FScenarioCase& Scenario)
	{
		return Scenario.Outcome == EBoundaryOutcome::CompileRejected;
	}

	static bool IsCompileAccepted(const FScenarioCase& Scenario)
	{
		return Scenario.Outcome == EBoundaryOutcome::CompileAccepted;
	}

	static FConstructorBoundaryState* GetActiveBoundaryState()
	{
		asIScriptContext* const Context = asGetActiveContext();
		return Context != nullptr && Context->GetEngine() != nullptr
			? static_cast<FConstructorBoundaryState*>(
				Context->GetEngine()->GetUserData(
					ConstructorBoundaryStateUserDataSlot))
			: nullptr;
	}

	static void RecordConstructorBoundaryMarker(const int32 Marker)
	{
		if (FConstructorBoundaryState* const State =
			GetActiveBoundaryState())
		{
			State->Markers.Add(Marker);
		}
	}

	static bool RegisterConstructorBoundaryBridge(
		asIScriptEngine& ScriptEngine,
		FConstructorBoundaryState& State)
	{
		ScriptEngine.SetUserData(
			&State,
			ConstructorBoundaryStateUserDataSlot);
		const ASAutoCaller::FunctionCaller MarkerCaller =
			ASAutoCaller::MakeFunctionCaller(
				RecordConstructorBoundaryMarker);
		return ScriptEngine.RegisterGlobalFunction(
			"void RecordConstructorBoundaryMarker(int Marker)",
			asFUNCTION(RecordConstructorBoundaryMarker),
			asCALL_CDECL,
			*(asFunctionCaller*)&MarkerCaller) >= 0;
	}

	static void AppendDuplicateDefaultSignature(FString& Source)
	{
		using namespace AngelscriptNativeTestSupport;

		AppendGeneratedAsLine(Source, TEXT("struct FBoundaryValue"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tint Value = 0;"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("\tFBoundaryValue()"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(Source, TEXT("\t\tValue = 41;"));
		AppendGeneratedAsLine(
			Source,
			TEXT("\t\tRecordConstructorBoundaryMarker(41);"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("\tFBoundaryValue()"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(Source, TEXT("\t\tValue = 42;"));
		AppendGeneratedAsLine(
			Source,
			TEXT("\t\tRecordConstructorBoundaryMarker(42);"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("int RunConstructorBoundary()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(
			Source,
			TEXT("\tFBoundaryValue Value = FBoundaryValue();"));
		AppendGeneratedAsLine(Source, TEXT("\treturn Value.Value;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
	}

	static void AppendDuplicateParameterSignature(FString& Source)
	{
		using namespace AngelscriptNativeTestSupport;

		AppendGeneratedAsLine(Source, TEXT("struct FBoundaryValue"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tint Value = 0;"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("\tFBoundaryValue(int Value)"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(Source, TEXT("\t\tthis.Value = Value + 51;"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(
			Source,
			TEXT("\tFBoundaryValue(int OtherValue)"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(
			Source,
			TEXT("\t\tValue = OtherValue + 52;"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source, TEXT("}"));
	}

	static void AppendMismatchedConstructorName(FString& Source)
	{
		using namespace AngelscriptNativeTestSupport;

		AppendGeneratedAsLine(Source, TEXT("struct FBoundaryValue"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tFBoundaryOther()"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source, TEXT("}"));
	}

	static void AppendConstructorValueReturn(FString& Source)
	{
		using namespace AngelscriptNativeTestSupport;

		AppendGeneratedAsLine(Source, TEXT("struct FBoundaryValue"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tFBoundaryValue()"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(Source, TEXT("\t\treturn 1;"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source, TEXT("}"));
	}

	static void AppendSuperOutsideConstructor(FString& Source)
	{
		using namespace AngelscriptNativeTestSupport;

		AppendGeneratedAsLine(
			Source,
			TEXT("void InvokeSuperOutsideConstructor()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tsuper();"));
		AppendGeneratedAsLine(Source, TEXT("}"));
	}

	static void AppendSuperAfterStatement(FString& Source)
	{
		using namespace AngelscriptNativeTestSupport;

		AppendGeneratedAsLine(Source, TEXT("class FBoundaryBase"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tFNativeCaseValue Tracked;"));
		AppendGeneratedAsLine(Source, TEXT("\tint Value = 0;"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("\tFBoundaryBase(int InValue)"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(Source, TEXT("\t\tValue = InValue;"));
		AppendGeneratedAsLine(
			Source,
			TEXT("\t\tRecordConstructorBoundaryMarker(2);"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(
			Source,
			TEXT("class FBoundaryDerived : FBoundaryBase"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tFBoundaryDerived()"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(
			Source,
			TEXT("\t\tRecordConstructorBoundaryMarker(1);"));
		AppendGeneratedAsLine(Source, TEXT("\t\tsuper(7);"));
		AppendGeneratedAsLine(
			Source,
			TEXT("\t\tRecordConstructorBoundaryMarker(3);"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("int RunConstructorBoundary()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(
			Source,
			TEXT("\tFBoundaryDerived Object = FBoundaryDerived();"));
		AppendGeneratedAsLine(Source, TEXT("\treturn Object.Value;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
	}

	static void AppendRepeatedSuper(FString& Source)
	{
		using namespace AngelscriptNativeTestSupport;

		AppendGeneratedAsLine(Source, TEXT("class FBoundaryBase"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tFBoundaryBase()"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(
			Source,
			TEXT("class FBoundaryDerived : FBoundaryBase"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tFBoundaryDerived()"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(Source, TEXT("\t\tsuper();"));
		AppendGeneratedAsLine(Source, TEXT("\t\tsuper();"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source, TEXT("}"));
	}

	static void AppendMissingBaseArgument(FString& Source)
	{
		using namespace AngelscriptNativeTestSupport;

		AppendGeneratedAsLine(Source, TEXT("class FBoundaryBase"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tFBoundaryBase(int Value)"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(
			Source,
			TEXT("class FBoundaryDerived : FBoundaryBase"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tFBoundaryDerived()"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(Source, TEXT("\t\tsuper();"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source, TEXT("}"));
	}

	static void AppendAmbiguousBaseArgument(FString& Source)
	{
		using namespace AngelscriptNativeTestSupport;

		AppendGeneratedAsLine(Source, TEXT("class FBoundaryBase"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(
			Source,
			TEXT("\tFBoundaryBase(int Value, int Extra = 1)"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(
			Source,
			TEXT("\tFBoundaryBase(int Value, float Extra = 1.0f)"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(
			Source,
			TEXT("class FBoundaryDerived : FBoundaryBase"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tFBoundaryDerived()"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(Source, TEXT("\t\tsuper(1);"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source, TEXT("}"));
	}

	static void AppendInaccessibleBase(FString& Source)
	{
		using namespace AngelscriptNativeTestSupport;

		AppendGeneratedAsLine(Source, TEXT("class FBoundaryBase"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tprivate FBoundaryBase()"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(
			Source,
			TEXT("class FBoundaryDerived : FBoundaryBase"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tFBoundaryDerived()"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(Source, TEXT("\t\tsuper();"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source, TEXT("}"));
	}

	static void AppendDirectRecursiveValueField(FString& Source)
	{
		using namespace AngelscriptNativeTestSupport;

		AppendGeneratedAsLine(Source, TEXT("struct FBoundaryRecursive"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(
			Source,
			TEXT("\tFBoundaryRecursive Nested;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
	}

	static void AppendIndirectRecursiveValueField(FString& Source)
	{
		using namespace AngelscriptNativeTestSupport;

		AppendGeneratedAsLine(Source, TEXT("struct FBoundaryRecursiveA"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(
			Source,
			TEXT("\tFBoundaryRecursiveB Nested;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("struct FBoundaryRecursiveB"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(
			Source,
			TEXT("\tFBoundaryRecursiveA Nested;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
	}

	static void AppendDirectRecursiveConstructor(FString& Source)
	{
		using namespace AngelscriptNativeTestSupport;

		AppendGeneratedAsLine(Source, TEXT("class FBoundaryRecursive"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tFNativeCaseValue Tracked;"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("\tFBoundaryRecursive()"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(
			Source,
			TEXT("\t\tFBoundaryRecursive Next = FBoundaryRecursive();"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("int RunConstructorBoundary()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(
			Source,
			TEXT("\tFBoundaryRecursive Root = FBoundaryRecursive();"));
		AppendGeneratedAsLine(Source, TEXT("\treturn 1;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
	}

	static void AppendIndirectRecursiveConstructor(FString& Source)
	{
		using namespace AngelscriptNativeTestSupport;

		AppendGeneratedAsLine(Source, TEXT("class FBoundaryRecursiveA"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tFNativeCaseValue Tracked;"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("\tFBoundaryRecursiveA()"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(
			Source,
			TEXT("\t\tFBoundaryRecursiveB Next = FBoundaryRecursiveB();"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("class FBoundaryRecursiveB"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tFNativeCaseValue Tracked;"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("\tFBoundaryRecursiveB()"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(
			Source,
			TEXT("\t\tFBoundaryRecursiveA Next = FBoundaryRecursiveA();"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("int RunConstructorBoundary()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(
			Source,
			TEXT("\tFBoundaryRecursiveA Root = FBoundaryRecursiveA();"));
		AppendGeneratedAsLine(Source, TEXT("\treturn 1;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
	}

	static void AppendMutableReferenceGlobal(FString& Source)
	{
		using namespace AngelscriptNativeTestSupport;

		AppendGeneratedAsLine(Source, TEXT("class FBoundaryRetained"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tFNativeCaseValue Tracked;"));
		AppendGeneratedAsLine(Source, TEXT("\tint ObjectId = -1;"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("\tFBoundaryRetained()"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(Source, TEXT("\t\tTracked.Value = 71;"));
		AppendGeneratedAsLine(
			Source,
			TEXT("\t\tObjectId = BeginNativeScriptLifecycle(71);"));
		AppendGeneratedAsLine(
			Source,
			TEXT("\t\tRecordConstructorBoundaryMarker(11);"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("\t~FBoundaryRetained()"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(
			Source,
			TEXT("\t\tRecordConstructorBoundaryMarker(12);"));
		AppendGeneratedAsLine(
			Source,
			TEXT("\t\tEndNativeScriptLifecycle(ObjectId, Tracked.Value);"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(
			Source,
			TEXT("FBoundaryRetained RetainedGlobal = FBoundaryRetained();"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("int RunConstructorBoundary()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(
			Source,
			TEXT("\treturn RetainedGlobal.Tracked.Value;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
	}

	static void AppendConstValueGlobal(FString& Source)
	{
		using namespace AngelscriptNativeTestSupport;

		AppendGeneratedAsLine(Source, TEXT("struct FBoundaryRetained"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tFNativeCaseValue Tracked;"));
		AppendGeneratedAsLine(Source, TEXT("\tint ObjectId = -1;"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("\tFBoundaryRetained()"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(Source, TEXT("\t\tTracked.Value = 71;"));
		AppendGeneratedAsLine(
			Source,
			TEXT("\t\tObjectId = BeginNativeScriptLifecycle(71);"));
		AppendGeneratedAsLine(
			Source,
			TEXT("\t\tRecordConstructorBoundaryMarker(11);"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(
			Source,
			TEXT("\tFBoundaryRetained(const FBoundaryRetained& Other)"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(Source, TEXT("\t\tTracked = Other.Tracked;"));
		AppendGeneratedAsLine(
			Source,
			TEXT("\t\tObjectId = CopyNativeScriptLifecycle(Other.ObjectId, Tracked.Value);"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(
			Source,
			TEXT("\tFBoundaryRetained& opAssign(const FBoundaryRetained& Other)"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(
			Source,
			TEXT("\t\tAssignNativeScriptLifecycle(ObjectId, Other.ObjectId, Other.Tracked.Value);"));
		AppendGeneratedAsLine(Source, TEXT("\t\tTracked = Other.Tracked;"));
		AppendGeneratedAsLine(Source, TEXT("\t\treturn this;"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("\t~FBoundaryRetained()"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(
			Source,
			TEXT("\t\tRecordConstructorBoundaryMarker(12);"));
		AppendGeneratedAsLine(
			Source,
			TEXT("\t\tEndNativeScriptLifecycle(ObjectId, Tracked.Value);"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(
			Source,
			TEXT("const FBoundaryRetained RetainedGlobal = FBoundaryRetained();"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("int RunConstructorBoundary()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(
			Source,
			TEXT("\treturn RetainedGlobal.Tracked.Value;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
	}

	static void AppendRecoveryFunction(FString& Source)
	{
		using namespace AngelscriptNativeTestSupport;

		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(
			Source,
			TEXT("int RunConstructorBoundaryRecovery()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(
			Source,
			TEXT("\tFNativeCaseValue Probe(97);"));
		AppendGeneratedAsLine(Source, TEXT("\treturn Probe.Value;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
	}

	static FString BuildConstructorBoundarySource(
		const FScenarioCase& Scenario)
	{
		FString Source;
		if (IsScenario(Scenario, "duplicate_default_signature"))
		{
			AppendDuplicateDefaultSignature(Source);
		}
		else if (
			IsScenario(Scenario, "duplicate_parameter_signature"))
		{
			AppendDuplicateParameterSignature(Source);
		}
		else if (
			IsScenario(Scenario, "mismatched_constructor_name"))
		{
			AppendMismatchedConstructorName(Source);
		}
		else if (
			IsScenario(Scenario, "constructor_value_return"))
		{
			AppendConstructorValueReturn(Source);
		}
		else if (
			IsScenario(Scenario, "super_outside_constructor"))
		{
			AppendSuperOutsideConstructor(Source);
		}
		else if (IsScenario(Scenario, "super_after_statement"))
		{
			AppendSuperAfterStatement(Source);
		}
		else if (IsScenario(Scenario, "repeated_super"))
		{
			AppendRepeatedSuper(Source);
		}
		else if (IsScenario(Scenario, "missing_base_argument"))
		{
			AppendMissingBaseArgument(Source);
		}
		else if (IsScenario(Scenario, "ambiguous_base_argument"))
		{
			AppendAmbiguousBaseArgument(Source);
		}
		else if (IsScenario(Scenario, "inaccessible_base"))
		{
			AppendInaccessibleBase(Source);
		}
		else if (
			IsScenario(Scenario, "recursive_value_field_direct"))
		{
			AppendDirectRecursiveValueField(Source);
		}
		else if (
			IsScenario(Scenario, "recursive_value_field_indirect"))
		{
			AppendIndirectRecursiveValueField(Source);
		}
		else if (
			IsScenario(Scenario, "recursive_constructor_direct"))
		{
			AppendDirectRecursiveConstructor(Source);
		}
		else if (
			IsScenario(Scenario, "recursive_constructor_indirect"))
		{
			AppendIndirectRecursiveConstructor(Source);
		}
		else if (IsScenario(Scenario, "mutable_reference_global"))
		{
			AppendMutableReferenceGlobal(Source);
		}
		else if (Scenario.Outcome == EBoundaryOutcome::ModuleDiscard
			|| Scenario.Outcome == EBoundaryOutcome::EngineShutdown)
		{
			AppendConstValueGlobal(Source);
		}
		if (!IsCompileRejected(Scenario))
		{
			AppendRecoveryFunction(Source);
		}
		return Source;
	}

	static FString BuildConstructorBoundaryRecoverySource()
	{
		FString Source;
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

		PrintGeneratedAsSource(Test, SourceId, ModuleName, Source);
		const FTCHARToUTF8 ModuleNameUtf8(*ModuleName);
		const FTCHARToUTF8 SourceUtf8(*Source);
		return CompileNativeModule(
			&ScriptEngine,
			ModuleNameUtf8.Get(),
			SourceUtf8.Get(),
			OutModule);
	}

	static bool HasOwnedDiagnostic(
		const FNativeTestEngine& Engine,
		const FScenarioCase& Scenario)
	{
		return Engine.GetMessages().Entries.ContainsByPredicate(
			[&Scenario](const FNativeMessageEntry& Entry)
			{
				if (Entry.Type != asMSGTYPE_ERROR
					|| Entry.Row <= 0
					|| Entry.Column <= 0)
				{
					return false;
				}
				return (Scenario.DiagnosticToken != nullptr
						&& Entry.Message.Contains(
							Scenario.DiagnosticToken))
					|| (Scenario.AlternateDiagnosticToken != nullptr
						&& Entry.Message.Contains(
							Scenario.AlternateDiagnosticToken));
			});
	}

	static FString DescribeLifecycle(
		const FNativeLifecycleRecorder& Lifecycle)
	{
		FString Entries;
		for (const FNativeLifecycleEntry& Entry : Lifecycle.GetEntries())
		{
			if (!Entries.IsEmpty())
			{
				Entries += TEXT(", ");
			}
			Entries += FString::Printf(
				TEXT("event=%d,id=%d,related=%d,value=%d"),
				static_cast<int32>(Entry.Event),
				Entry.ObjectId,
				Entry.RelatedObjectId,
				Entry.Value);
		}
		return Entries;
	}

	static FString DescribeMarkers(const TArray<int32>& Markers)
	{
		FString Result;
		for (const int32 Marker : Markers)
		{
			if (!Result.IsEmpty())
			{
				Result += TEXT(", ");
			}
			Result += FString::FromInt(Marker);
		}
		return Result;
	}

	void VerifyBalancedLifecycle(
		const FNativeCaseContext& Case,
		const FNativeLifecycleRecorder& Lifecycle,
		const bool bRequireConstruction)
	{
		const int32 ConstructionCount =
			Lifecycle.Num(ENativeLifecycleEvent::DefaultConstruct)
			+ Lifecycle.Num(ENativeLifecycleEvent::ValueConstruct)
			+ Lifecycle.Num(ENativeLifecycleEvent::CopyConstruct);
		const int32 DestructionCount =
			Lifecycle.Num(ENativeLifecycleEvent::Destruct);
		if (Lifecycle.GetLiveObjectCount() != 0
			|| ConstructionCount != DestructionCount)
		{
			TestRunner->AddInfo(FString::Printf(
				TEXT("[%s][LIFECYCLE] live=%d construct=%d destruct=%d entries=[%s]"),
				*Case.GetId(),
				Lifecycle.GetLiveObjectCount(),
				ConstructionCount,
				DestructionCount,
				*DescribeLifecycle(Lifecycle)));
		}
		if (bRequireConstruction)
		{
			ASSERT_THAT(IsTrue(ConstructionCount > 0,
				*Case.Describe(TEXT("constructor boundary should create tracked storage"))));
		}
		ASSERT_THAT(AreEqual(0, Lifecycle.GetLiveObjectCount(),
			*Case.Describe(TEXT("constructor boundary should leave no live tracked storage"))));
		ASSERT_THAT(AreEqual(
			ConstructionCount,
			DestructionCount,
			*Case.Describe(TEXT("constructor boundary should balance every constructed identity"))));

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
					*Case.Describe(TEXT("constructor-boundary destructor should identify constructed storage"))));
				if (DestructedIds.Contains(Entry.ObjectId))
				{
					TestRunner->AddInfo(FString::Printf(
						TEXT("[%s][DUPLICATE-DESTRUCTION] entries=[%s]"),
						*Case.GetId(),
						*DescribeLifecycle(Lifecycle)));
				}
				ASSERT_THAT(IsFalse(
					DestructedIds.Contains(Entry.ObjectId),
					*Case.Describe(TEXT("constructor-boundary storage should not be destroyed twice"))));
				DestructedIds.Add(Entry.ObjectId);
			}
		}
		ASSERT_THAT(AreEqual(
			ConstructedIds.Num(),
			DestructedIds.Num(),
			*Case.Describe(TEXT("constructor-boundary identities should close exactly once"))));
	}

	void VerifyRawClassScopeCleanup(
		const FNativeCaseContext& Case,
		const FNativeLifecycleRecorder& Lifecycle)
	{
		const int32 ConstructionCount =
			Lifecycle.Num(ENativeLifecycleEvent::DefaultConstruct)
			+ Lifecycle.Num(ENativeLifecycleEvent::ValueConstruct)
			+ Lifecycle.Num(ENativeLifecycleEvent::CopyConstruct);
		const int32 DestructionCount = Lifecycle.Num(ENativeLifecycleEvent::Destruct);
		if (Lifecycle.GetLiveObjectCount() != 0
			|| DestructionCount != ConstructionCount)
		{
			TestRunner->AddInfo(FString::Printf(
				TEXT("[%s][RAW-CLASS-CLEANUP] live=%d construct=%d destruct=%d entries=[%s]"),
				*Case.GetId(),
				Lifecycle.GetLiveObjectCount(),
				ConstructionCount,
				DestructionCount,
				*DescribeLifecycle(Lifecycle)));
		}
		ASSERT_THAT(IsTrue(ConstructionCount > 0,
			*Case.Describe(TEXT("raw class normal scope should allocate tracked native storage"))));
		ASSERT_THAT(AreEqual(0, Lifecycle.GetLiveObjectCount(),
			*Case.Describe(TEXT("raw class normal scope should retire storage after lexical cleanup"))));
		ASSERT_THAT(AreEqual(ConstructionCount, DestructionCount,
			*Case.Describe(TEXT("raw class normal scope should destroy every tracked storage identity once"))));
		const TArray<FNativeLifecycleEntry>& Entries = Lifecycle.GetEntries();
		ASSERT_THAT(IsTrue(Entries.Num() > 0,
			*Case.Describe(TEXT("raw class scope limitation should expose the tracked base-field allocation"))));
		if (Entries.IsEmpty())
		{
			return;
		}

		const int32 BaseFieldObjectId = Entries[0].ObjectId;
		ASSERT_THAT(IsTrue(Entries.ContainsByPredicate(
			[BaseFieldObjectId](const FNativeLifecycleEntry& Entry)
			{
				return Entry.Event == ENativeLifecycleEvent::Destruct
					&& Entry.ObjectId == BaseFieldObjectId;
			}), *Case.Describe(TEXT("raw class normal scope should retire the tracked base field"))));
	}

	static int32 CountConstructorBehaviours(
		const asITypeInfo& Type,
		const int32 ParameterCount)
	{
		int32 Count = 0;
		for (asUINT Index = 0; Index < Type.GetBehaviourCount(); ++Index)
		{
			asEBehaviours Behaviour = asBEHAVE_CONSTRUCT;
			asIScriptFunction* const Function =
				Type.GetBehaviourByIndex(Index, &Behaviour);
			if (Function != nullptr
				&& Behaviour == asBEHAVE_CONSTRUCT
				&& static_cast<int32>(Function->GetParamCount())
					== ParameterCount)
			{
				++Count;
			}
		}
		return Count;
	}

	void CompileAndExecuteRecovery(
		const FNativeCaseContext& Case,
		FNativeTestEngine& Engine,
		asIScriptEngine& ScriptEngine,
		const FString& ModuleName,
		FNativeLifecycleRecorder& Lifecycle,
		FConstructorBoundaryState& State)
	{
		using namespace AngelscriptNativeTestSupport;

		const FString RecoverySource =
			BuildConstructorBoundaryRecoverySource();
		Engine.ResetMessages();
		Lifecycle.Reset();
		State.Reset();
		asIScriptModule* RecoveryModule = nullptr;
		ASSERT_THAT(IsTrue(CompileAndReport(
			*TestRunner,
			ScriptEngine,
			Case.GetId() + TEXT("-RECOVERY"),
			ModuleName,
			RecoverySource,
			RecoveryModule) >= 0,
			*Case.Describe(TEXT("constructor-boundary same-name recovery should compile"))));
		ASSERT_THAT(IsNotNull(RecoveryModule,
			*Case.Describe(TEXT("constructor-boundary recovery should publish its module"))));
		if (RecoveryModule == nullptr)
		{
			return;
		}

		asIScriptFunction* const Recovery =
			RecoveryModule->GetFunctionByDecl(
				"int RunConstructorBoundaryRecovery()");
		ASSERT_THAT(IsNotNull(Recovery,
			*Case.Describe(TEXT("constructor-boundary recovery should publish its exact function"))));
		asIScriptContext* const Context =
			ScriptEngine.CreateContext();
		ASSERT_THAT(IsNotNull(Context,
			*Case.Describe(TEXT("constructor-boundary recovery should create a context"))));
		if (Recovery != nullptr && Context != nullptr)
		{
			ASSERT_THAT(AreEqual(
				static_cast<int32>(asEXECUTION_FINISHED),
				PrepareAndExecute(Context, Recovery),
				*Case.Describe(TEXT("constructor-boundary recovery should finish"))));
			ASSERT_THAT(AreEqual(
				97,
				static_cast<int32>(Context->GetReturnDWord()),
				*Case.Describe(TEXT("constructor-boundary recovery should return its sentinel"))));
			ASSERT_THAT(AreEqual(
				asSUCCESS,
				Context->Unprepare(),
				*Case.Describe(TEXT("constructor-boundary recovery should release its value"))));
			Context->Release();
		}
		else if (Context != nullptr)
		{
			Context->Release();
		}
		ASSERT_THAT(AreEqual(0, State.Markers.Num(),
			*Case.Describe(TEXT("constructor-boundary recovery should emit no boundary marker"))));
		VerifyBalancedLifecycle(Case, Lifecycle, true);

		const FTCHARToUTF8 ModuleNameUtf8(*ModuleName);
		ScriptEngine.DiscardModule(ModuleNameUtf8.Get());
		ASSERT_THAT(IsNull(ScriptEngine.GetModule(
			ModuleNameUtf8.Get(),
			asGM_ONLY_IF_EXISTS),
			*Case.Describe(TEXT("constructor-boundary recovery module should discard cleanly"))));
	}

	void RunCompileAccepted(
		const TStaticArray<FNativeCaseContext, 4>& Cases,
		const FScenarioCase& Scenario,
		FNativeTestEngine& Engine,
		asIScriptEngine& ScriptEngine,
		asIScriptModule& Module,
		const FString& ModuleName,
		FNativeLifecycleRecorder& Lifecycle,
		FConstructorBoundaryState& State)
	{
		using namespace AngelscriptNativeTestSupport;

		ASSERT_THAT(IsFalse(Engine.GetMessages().Entries.ContainsByPredicate(
			[](const FNativeMessageEntry& Entry)
			{
				return Entry.Type == asMSGTYPE_ERROR;
			}),
			*Cases[0].Describe(TEXT("current-fork duplicate declaration should compile without an error diagnostic"))));

		asITypeInfo* const Type =
			Module.GetTypeInfoByName("FBoundaryValue");
		ASSERT_THAT(IsNotNull(Type,
			*Cases[1].Describe(TEXT("duplicate constructor declaration should publish its type metadata"))));
		if (Type == nullptr)
		{
			return;
		}

		const int32 ExpectedParameterCount =
			IsScenario(Scenario, "duplicate_default_signature")
				? 0
				: 1;
		const int32 ExpectedConstructorCount =
			IsScenario(Scenario, "duplicate_default_signature")
				? 1
				: 2;
		ASSERT_THAT(AreEqual(
			ExpectedConstructorCount,
			CountConstructorBehaviours(*Type, ExpectedParameterCount),
			*Cases[1].Describe(TEXT("current-fork duplicate declaration should expose its actual constructor behaviour set"))));

		if (IsScenario(Scenario, "duplicate_default_signature"))
		{
			asIScriptFunction* const Entry =
				Module.GetFunctionByDecl("int RunConstructorBoundary()");
			ASSERT_THAT(IsNotNull(Entry,
				*Cases[0].Describe(TEXT("duplicate default constructor source should publish its execution probe"))));
			asIScriptContext* const Context = ScriptEngine.CreateContext();
			ASSERT_THAT(IsNotNull(Context,
				*Cases[0].Describe(TEXT("duplicate default constructor source should create a context"))));
			if (Entry != nullptr && Context != nullptr)
			{
				ASSERT_THAT(AreEqual(
					static_cast<int32>(asEXECUTION_FINISHED),
					PrepareAndExecute(Context, Entry),
					*Cases[0].Describe(TEXT("duplicate default constructor probe should finish"))));
				ASSERT_THAT(AreEqual(
					42,
					static_cast<int32>(Context->GetReturnDWord()),
					*Cases[1].Describe(TEXT("the later duplicate default constructor should replace the active default behaviour"))));
				ASSERT_THAT(AreEqual(
					asSUCCESS,
					Context->Unprepare(),
					*Cases[2].Describe(TEXT("duplicate default constructor context should unprepare cleanly"))));
				Context->Release();
			}
			else if (Context != nullptr)
			{
				Context->Release();
			}
			const TArray<int32> ExpectedMarkers = { 42 };
			ASSERT_THAT(AreEqual(ExpectedMarkers, State.Markers,
				*Cases[1].Describe(TEXT("duplicate default constructor execution should select only the later body"))));
		}
		else
		{
			ASSERT_THAT(AreEqual(0, State.Markers.Num(),
				*Cases[2].Describe(TEXT("duplicate parameter declarations should not execute before a consumer resolves them"))));
		}
		VerifyBalancedLifecycle(Cases[2], Lifecycle, false);

		const FTCHARToUTF8 ModuleNameUtf8(*ModuleName);
		ScriptEngine.DiscardModule(ModuleNameUtf8.Get());
		ASSERT_THAT(IsNull(ScriptEngine.GetModule(
			ModuleNameUtf8.Get(),
			asGM_ONLY_IF_EXISTS),
			*Cases[2].Describe(TEXT("duplicate constructor declaration module should discard cleanly"))));
		CompileAndExecuteRecovery(
			Cases[3],
			Engine,
			ScriptEngine,
			ModuleName,
			Lifecycle,
			State);
	}

	void RunCompileRejected(
		const TStaticArray<FNativeCaseContext, 4>& Cases,
		const FScenarioCase& Scenario,
		FNativeTestEngine& Engine,
		asIScriptEngine& ScriptEngine,
		const FString& ModuleName,
		const FString& Source,
		FNativeLifecycleRecorder& Lifecycle,
		FConstructorBoundaryState& State)
	{
		asIScriptModule* Module = nullptr;
		const int BuildResult = CompileAndReport(
			*TestRunner,
			ScriptEngine,
			Cases[0].GetId(),
			ModuleName,
			Source,
			Module);
		ASSERT_THAT(IsTrue(BuildResult < 0,
			*Cases[0].Describe(TEXT("invalid constructor boundary should be rejected"))));
		const bool bHasOwnedDiagnostic = HasOwnedDiagnostic(Engine, Scenario);
		if (!bHasOwnedDiagnostic)
		{
			TestRunner->AddInfo(FString::Printf(
				TEXT("[%s][DIAGNOSTICS] %s"),
				*Cases[1].GetId(),
				*Engine.GetMessagesText()));
		}
		if (IsScenario(Scenario, "missing_base_argument"))
		{
			ASSERT_THAT(IsFalse(bHasOwnedDiagnostic,
				*Cases[1].Describe(TEXT("current fork should characterize the missing-base rejection without an owned error diagnostic"))));
			ASSERT_THAT(IsTrue(Engine.GetMessages().Entries.ContainsByPredicate(
				[](const FNativeMessageEntry& Entry)
				{
					return Entry.Type == asMSGTYPE_INFORMATION
						&& Entry.Message.Contains(TEXT("Compiling FBoundaryDerived::FBoundaryDerived()"));
				}),
				*Cases[1].Describe(TEXT("current fork should retain the compilation-info record for the silent missing-base rejection"))));
		}
		else
		{
			ASSERT_THAT(IsTrue(bHasOwnedDiagnostic,
				*Cases[1].Describe(TEXT("invalid constructor boundary should own a located scenario diagnostic"))));
		}
		ASSERT_THAT(AreEqual(0, State.Markers.Num(),
			*Cases[2].Describe(TEXT("compile-time constructor rejection should execute no marker"))));
		VerifyBalancedLifecycle(Cases[2], Lifecycle, false);

		const FTCHARToUTF8 ModuleNameUtf8(*ModuleName);
		ScriptEngine.DiscardModule(ModuleNameUtf8.Get());
		ASSERT_THAT(IsNull(ScriptEngine.GetModule(
			ModuleNameUtf8.Get(),
			asGM_ONLY_IF_EXISTS),
			*Cases[2].Describe(TEXT("failed constructor module should leave no published module"))));
		CompileAndExecuteRecovery(
			Cases[3],
			Engine,
			ScriptEngine,
			ModuleName,
			Lifecycle,
			State);
	}

	void RunRuntimeSuccess(
		const TStaticArray<FNativeCaseContext, 4>& Cases,
		asIScriptEngine& ScriptEngine,
		asIScriptModule& Module,
		FNativeLifecycleRecorder& Lifecycle,
		FConstructorBoundaryState& State)
	{
		using namespace AngelscriptNativeTestSupport;

		asITypeInfo* const Base =
			Module.GetTypeInfoByName("FBoundaryBase");
		asITypeInfo* const Derived =
			Module.GetTypeInfoByName("FBoundaryDerived");
		ASSERT_THAT(IsNotNull(Base,
			*Cases[1].Describe(TEXT("late-super boundary should publish its base metadata"))));
		ASSERT_THAT(IsNotNull(Derived,
			*Cases[1].Describe(TEXT("late-super boundary should publish its derived metadata"))));
		if (Base != nullptr && Derived != nullptr)
		{
			ASSERT_THAT(AreEqual(Base, Derived->GetBaseType(),
				*Cases[1].Describe(TEXT("late-super boundary should preserve its inheritance edge"))));
		}

		asIScriptFunction* const Entry =
			Module.GetFunctionByDecl("int RunConstructorBoundary()");
		asIScriptFunction* const Recovery =
			Module.GetFunctionByDecl(
				"int RunConstructorBoundaryRecovery()");
		ASSERT_THAT(IsNotNull(Entry,
			*Cases[0].Describe(TEXT("late-super boundary should publish its entry"))));
		ASSERT_THAT(IsNotNull(Recovery,
			*Cases[3].Describe(TEXT("late-super boundary should publish its recovery"))));
		if (Entry == nullptr || Recovery == nullptr)
		{
			return;
		}

		asIScriptContext* const Context =
			ScriptEngine.CreateContext();
		ASSERT_THAT(IsNotNull(Context,
			*Cases[0].Describe(TEXT("late-super boundary should create a reusable context"))));
		if (Context == nullptr)
		{
			return;
		}
		ASSERT_THAT(AreEqual(
			static_cast<int32>(asEXECUTION_FINISHED),
			PrepareAndExecute(Context, Entry),
			*Cases[0].Describe(TEXT("current fork should execute a sequential late super call"))));
		ASSERT_THAT(AreEqual(
			7,
			static_cast<int32>(Context->GetReturnDWord()),
			*Cases[0].Describe(TEXT("late super call should initialize the base value"))));
		ASSERT_THAT(AreEqual(
			asSUCCESS,
			Context->Unprepare(),
			*Cases[2].Describe(TEXT("late-super context should release its object"))));
		const TArray<int32> ExpectedMarkers = { 1, 2, 3 };
		ASSERT_THAT(AreEqual(ExpectedMarkers, State.Markers,
			*Cases[1].Describe(TEXT("late-super marker order should remain statement, base, then derived"))));
		VerifyRawClassScopeCleanup(Cases[2], Lifecycle);

		const int32 MarkerCount = State.Markers.Num();
		ASSERT_THAT(IsTrue(Context->Prepare(Recovery) >= 0,
			*Cases[3].Describe(TEXT("late-super context should prepare recovery"))));
		ASSERT_THAT(AreEqual(
			static_cast<int32>(asEXECUTION_FINISHED),
			Context->Execute(),
			*Cases[3].Describe(TEXT("late-super recovery should finish"))));
		ASSERT_THAT(AreEqual(
			97,
			static_cast<int32>(Context->GetReturnDWord()),
			*Cases[3].Describe(TEXT("late-super recovery should return its sentinel"))));
		ASSERT_THAT(AreEqual(MarkerCount, State.Markers.Num(),
			*Cases[3].Describe(TEXT("late-super recovery should emit no constructor marker"))));
		ASSERT_THAT(AreEqual(
			asSUCCESS,
			Context->Unprepare(),
			*Cases[3].Describe(TEXT("late-super recovery should unprepare cleanly"))));
		Context->Release();
	}

	void RunRuntimeException(
		const TStaticArray<FNativeCaseContext, 4>& Cases,
		asIScriptEngine& ScriptEngine,
		asIScriptModule& Module,
		FNativeLifecycleRecorder& Lifecycle,
		FConstructorBoundaryState& State)
	{
		using namespace AngelscriptNativeTestSupport;

		asIScriptFunction* const Entry =
			Module.GetFunctionByDecl("int RunConstructorBoundary()");
		asIScriptFunction* const Recovery =
			Module.GetFunctionByDecl(
				"int RunConstructorBoundaryRecovery()");
		ASSERT_THAT(IsNotNull(Entry,
			*Cases[0].Describe(TEXT("recursive constructor boundary should publish its entry"))));
		ASSERT_THAT(IsNotNull(Recovery,
			*Cases[3].Describe(TEXT("recursive constructor boundary should publish recovery"))));
		if (Entry == nullptr || Recovery == nullptr)
		{
			return;
		}

		asIScriptContext* const Context =
			ScriptEngine.CreateContext();
		ASSERT_THAT(IsNotNull(Context,
			*Cases[0].Describe(TEXT("recursive constructor boundary should create a context"))));
		if (Context == nullptr)
		{
			return;
		}
		ASSERT_THAT(AreEqual(
			static_cast<int32>(asEXECUTION_EXCEPTION),
			PrepareAndExecute(Context, Entry),
			*Cases[0].Describe(TEXT("recursive constructor should terminate with a managed exception"))));
		ASSERT_THAT(AreEqual(
			FString(TEXT("Stack overflow")),
			FString(UTF8_TO_TCHAR(Context->GetExceptionString())),
			*Cases[1].Describe(TEXT("recursive constructor should own the exact stack-overflow reason"))));
		ASSERT_THAT(IsNotNull(Context->GetExceptionFunction(),
			*Cases[1].Describe(TEXT("recursive constructor exception should identify its function"))));
		ASSERT_THAT(AreEqual(
			0,
			Context->GetExceptionLineNumber(),
			*Cases[1].Describe(TEXT("current fork should report stack-allocation overflow at the function-level line sentinel"))));
		ASSERT_THAT(AreEqual(
			asSUCCESS,
			Context->Unprepare(),
			*Cases[2].Describe(TEXT("recursive constructor exception should release its initialized prefix"))));
		VerifyBalancedLifecycle(Cases[2], Lifecycle, true);

		ASSERT_THAT(IsTrue(Context->Prepare(Recovery) >= 0,
			*Cases[3].Describe(TEXT("recursive constructor context should prepare recovery"))));
		ASSERT_THAT(AreEqual(
			static_cast<int32>(asEXECUTION_FINISHED),
			Context->Execute(),
			*Cases[3].Describe(TEXT("recursive constructor recovery should finish in the same context"))));
		ASSERT_THAT(AreEqual(
			97,
			static_cast<int32>(Context->GetReturnDWord()),
			*Cases[3].Describe(TEXT("recursive constructor recovery should return its sentinel"))));
		ASSERT_THAT(AreEqual(
			asSUCCESS,
			Context->Unprepare(),
			*Cases[3].Describe(TEXT("recursive constructor recovery should unprepare cleanly"))));
		Context->Release();
		ASSERT_THAT(AreEqual(0, State.Markers.Num(),
			*Cases[3].Describe(TEXT("recursive constructor recovery should emit no boundary marker"))));
		VerifyBalancedLifecycle(Cases[3], Lifecycle, true);
	}

	void VerifyRetainedMetadata(
		const FNativeCaseContext& Case,
		asIScriptModule& Module)
	{
		asITypeInfo* const Type =
			Module.GetTypeInfoByName("FBoundaryRetained");
		ASSERT_THAT(IsNotNull(Type,
			*Case.Describe(TEXT("retained constructor boundary should publish its reference type"))));
		if (Type != nullptr)
		{
			ASSERT_THAT(IsTrue(
				(Type->GetFlags() & asOBJ_VALUE) != 0,
				*Case.Describe(TEXT("retained constructor boundary should publish a value type"))));
			ASSERT_THAT(IsFalse(
				(Type->GetFlags() & asOBJ_REF) != 0,
				*Case.Describe(TEXT("retained constructor boundary should not claim unavailable script-reference global support"))));
			ASSERT_THAT(IsTrue(Type->GetBehaviourCount() > 0,
				*Case.Describe(TEXT("retained constructor boundary should publish constructor behavior"))));
		}
		ASSERT_THAT(AreEqual(
			1,
			static_cast<int32>(Module.GetGlobalVarCount()),
			*Case.Describe(TEXT("retained constructor boundary should publish one owning const global"))));
		if (Module.GetGlobalVarCount() == 1)
		{
			const char* const Declaration = Module.GetGlobalVarDeclaration(0);
			ASSERT_THAT(IsNotNull(Declaration,
				*Case.Describe(TEXT("retained constructor boundary should expose its global declaration"))));
			if (Declaration != nullptr)
			{
				const FString GlobalDeclaration = UTF8_TO_TCHAR(Declaration);
				ASSERT_THAT(IsTrue(
					GlobalDeclaration.Contains(TEXT("const FBoundaryRetained"), ESearchCase::CaseSensitive),
					*Case.Describe(TEXT("retained constructor boundary should use the current-fork legal const value global"))));
			}
		}
	}

	void ExecuteRetainedEntry(
		const TStaticArray<FNativeCaseContext, 4>& Cases,
		asIScriptEngine& ScriptEngine,
		asIScriptModule& Module)
	{
		using namespace AngelscriptNativeTestSupport;

		asIScriptFunction* const Entry =
			Module.GetFunctionByDecl("int RunConstructorBoundary()");
		ASSERT_THAT(IsNotNull(Entry,
			*Cases[0].Describe(TEXT("retained constructor boundary should publish its value probe"))));
		asIScriptContext* const Context = ScriptEngine.CreateContext();
		ASSERT_THAT(IsNotNull(Context,
			*Cases[0].Describe(TEXT("retained constructor boundary should create its probe context"))));
		if (Entry != nullptr && Context != nullptr)
		{
			ASSERT_THAT(AreEqual(
				static_cast<int32>(asEXECUTION_FINISHED),
				PrepareAndExecute(Context, Entry),
				*Cases[0].Describe(TEXT("retained constructor boundary should read the initialized const global"))));
			ASSERT_THAT(AreEqual(
				71,
				static_cast<int32>(Context->GetReturnDWord()),
				*Cases[1].Describe(TEXT("retained constructor boundary should preserve the const global's tracked value"))));
			ASSERT_THAT(AreEqual(
				asSUCCESS,
				Context->Unprepare(),
				*Cases[2].Describe(TEXT("retained constructor boundary probe should unprepare cleanly"))));
			Context->Release();
		}
		else if (Context != nullptr)
		{
			Context->Release();
		}
	}

	void VerifyConstValueGlobalBuildSequence(
		const TStaticArray<FNativeCaseContext, 4>& Cases,
		FConstructorBoundaryState& State)
	{
		const TArray<int32> ExpectedMarkers = { 11, 12 };
		if (State.Markers != ExpectedMarkers)
		{
			TestRunner->AddInfo(FString::Printf(
				TEXT("[%s][GLOBAL-MARKERS] expected=[%s] actual=[%s]"),
				*Cases[0].GetId(),
				*DescribeMarkers(ExpectedMarkers),
				*DescribeMarkers(State.Markers)));
		}
		ASSERT_THAT(AreEqual(ExpectedMarkers, State.Markers,
			*Cases[0].Describe(TEXT("const value-global build should preserve explicit construction and temporary-destruction order"))));
	}

	void RunModuleDiscard(
		const TStaticArray<FNativeCaseContext, 4>& Cases,
		FNativeTestEngine& Engine,
		asIScriptEngine& ScriptEngine,
		asIScriptModule& Module,
		const FString& ModuleName,
		FNativeLifecycleRecorder& Lifecycle,
		FConstructorBoundaryState& State)
	{
		using namespace AngelscriptNativeTestSupport;

		VerifyRetainedMetadata(Cases[1], Module);
		VerifyConstValueGlobalBuildSequence(Cases, State);
		ExecuteRetainedEntry(Cases, ScriptEngine, Module);
		VerifyConstValueGlobalBuildSequence(Cases, State);
		ASSERT_THAT(IsTrue(Lifecycle.GetLiveObjectCount() > 0,
			*Cases[2].Describe(TEXT("module should own live const-global constructor storage before discard"))));

		const FTCHARToUTF8 ModuleNameUtf8(*ModuleName);
		ScriptEngine.DiscardModule(ModuleNameUtf8.Get());
		ASSERT_THAT(IsNull(ScriptEngine.GetModule(
			ModuleNameUtf8.Get(),
			asGM_ONLY_IF_EXISTS),
			*Cases[3].Describe(TEXT("module discard should remove retained constructor metadata"))));
		const TArray<int32> ExpectedTeardownMarkers = { 11, 12, 12 };
		if (State.Markers != ExpectedTeardownMarkers)
		{
			TestRunner->AddInfo(FString::Printf(
				TEXT("[%s][GLOBAL-MARKERS] expected=[%s] actual=[%s]"),
				*Cases[3].GetId(),
				*DescribeMarkers(ExpectedTeardownMarkers),
				*DescribeMarkers(State.Markers)));
		}
		ASSERT_THAT(AreEqual(ExpectedTeardownMarkers, State.Markers,
			*Cases[3].Describe(TEXT("module discard should append the retained const value destructor after temporary cleanup"))));
		VerifyBalancedLifecycle(Cases[3], Lifecycle, true);
		CompileAndExecuteRecovery(
			Cases[3],
			Engine,
			ScriptEngine,
			ModuleName,
			Lifecycle,
			State);
	}

	void RunEngineShutdown(
		const TStaticArray<FNativeCaseContext, 4>& Cases,
		FNativeTestEngine& Engine,
		asIScriptEngine& ScriptEngine,
		asIScriptModule& Module,
		const FString& ModuleName,
		FNativeLifecycleRecorder& Lifecycle,
		FConstructorBoundaryState& State)
	{
		VerifyRetainedMetadata(Cases[1], Module);
		VerifyConstValueGlobalBuildSequence(Cases, State);
		ExecuteRetainedEntry(Cases, ScriptEngine, Module);
		VerifyConstValueGlobalBuildSequence(Cases, State);
		ASSERT_THAT(IsTrue(Lifecycle.GetLiveObjectCount() > 0,
			*Cases[2].Describe(TEXT("engine should own live const-global constructor storage before shutdown"))));

		Engine.Destroy();
		const TArray<int32> ExpectedTeardownMarkers = { 11, 12, 12 };
		if (State.Markers != ExpectedTeardownMarkers)
		{
			TestRunner->AddInfo(FString::Printf(
				TEXT("[%s][GLOBAL-MARKERS] expected=[%s] actual=[%s]"),
				*Cases[3].GetId(),
				*DescribeMarkers(ExpectedTeardownMarkers),
				*DescribeMarkers(State.Markers)));
		}
		ASSERT_THAT(AreEqual(ExpectedTeardownMarkers, State.Markers,
			*Cases[3].Describe(TEXT("engine shutdown should append the retained const value destructor after temporary cleanup"))));
		VerifyBalancedLifecycle(Cases[3], Lifecycle, true);

		Engine.Create(*TestRunner);
		asIScriptEngine* const RecoveryEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(RecoveryEngine,
			*Cases[3].Describe(TEXT("engine shutdown should allow a fresh recovery engine"))));
		if (RecoveryEngine == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(RegisterConstructorBoundaryBridge(
			*RecoveryEngine,
			State),
			*Cases[3].Describe(TEXT("fresh engine should register the boundary bridge"))));
		ASSERT_THAT(IsTrue(RegisterNativeCaseValue(
			*RecoveryEngine,
			Lifecycle),
			*Cases[3].Describe(TEXT("fresh engine should register native values"))));
		ASSERT_THAT(IsTrue(RegisterNativeScriptLifecycleBridge(
			*RecoveryEngine,
			Lifecycle),
			*Cases[3].Describe(TEXT("fresh engine should register lifecycle callbacks"))));
		CompileAndExecuteRecovery(
			Cases[3],
			Engine,
			*RecoveryEngine,
			ModuleName,
			Lifecycle,
			State);
	}

	void RunCell(const FScenarioCase& Scenario)
	{
		using namespace AngelscriptNativeTestSupport;

		TStaticArray<FNativeCaseContext, 4> Cases =
		{
			FNativeCaseContext(MakeNativeCaseId(
				"LANG-CTOR-BOUNDARY",
				{
					ANSI_TO_TCHAR(
						ObservationCases[0].CatalogName),
					ANSI_TO_TCHAR(Scenario.CatalogName),
				})),
			FNativeCaseContext(MakeNativeCaseId(
				"LANG-CTOR-BOUNDARY",
				{
					ANSI_TO_TCHAR(
						ObservationCases[1].CatalogName),
					ANSI_TO_TCHAR(Scenario.CatalogName),
				})),
			FNativeCaseContext(MakeNativeCaseId(
				"LANG-CTOR-BOUNDARY",
				{
					ANSI_TO_TCHAR(
						ObservationCases[2].CatalogName),
					ANSI_TO_TCHAR(Scenario.CatalogName),
				})),
			FNativeCaseContext(MakeNativeCaseId(
				"LANG-CTOR-BOUNDARY",
				{
					ANSI_TO_TCHAR(
						ObservationCases[3].CatalogName),
					ANSI_TO_TCHAR(Scenario.CatalogName),
				})),
		};

		FNativeLifecycleRecorder Lifecycle;
		FConstructorBoundaryState State;
		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };
		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine,
			*Cases[0].Describe(TEXT("constructor-boundary scenario should create a raw SDK engine"))));
		if (ScriptEngine == nullptr)
		{
			return;
		}
		if (Scenario.Outcome == EBoundaryOutcome::RuntimeException)
		{
			ASSERT_THAT(AreEqual(
				asSUCCESS,
				ScriptEngine->SetEngineProperty(
					asEP_MAX_STACK_SIZE,
					256),
				*Cases[0].Describe(TEXT("recursive constructor should set a deterministic stack limit"))));
		}

		ASSERT_THAT(IsTrue(RegisterConstructorBoundaryBridge(
			*ScriptEngine,
			State),
			*Cases[0].Describe(TEXT("constructor-boundary scenario should register its marker bridge"))));
		ASSERT_THAT(IsTrue(RegisterNativeCaseValue(
			*ScriptEngine,
			Lifecycle),
			*Cases[0].Describe(TEXT("constructor-boundary scenario should register native values"))));
		ASSERT_THAT(IsTrue(RegisterNativeScriptLifecycleBridge(
			*ScriptEngine,
			Lifecycle),
			*Cases[0].Describe(TEXT("constructor-boundary scenario should register lifecycle callbacks"))));

		const FString ModuleName = FString::Printf(
			TEXT("ConstructorBoundary_%hs"),
			Scenario.CatalogName);
		const FString Source =
			BuildConstructorBoundarySource(Scenario);
		Engine.ResetMessages();
		Lifecycle.Reset();
		State.Reset();
		if (IsCompileRejected(Scenario))
		{
			RunCompileRejected(
				Cases,
				Scenario,
				Engine,
				*ScriptEngine,
				ModuleName,
				Source,
				Lifecycle,
				State);
			return;
		}

		asIScriptModule* Module = nullptr;
		ASSERT_THAT(IsTrue(CompileAndReport(
			*TestRunner,
			*ScriptEngine,
			Cases[0].GetId(),
			ModuleName,
			Source,
			Module) >= 0,
			*Cases[0].Describe(TEXT("executable constructor boundary should compile"))));
		ASSERT_THAT(IsNotNull(Module,
			*Cases[0].Describe(TEXT("executable constructor boundary should publish its module"))));
		if (Module == nullptr)
		{
			return;
		}

		if (IsCompileAccepted(Scenario))
		{
			RunCompileAccepted(
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
		if (Scenario.Outcome == EBoundaryOutcome::RuntimeSuccess)
		{
			RunRuntimeSuccess(
				Cases,
				*ScriptEngine,
				*Module,
				Lifecycle,
				State);
		}
		else if (
			Scenario.Outcome == EBoundaryOutcome::RuntimeException)
		{
			RunRuntimeException(
				Cases,
				*ScriptEngine,
				*Module,
				Lifecycle,
				State);
		}
		else if (Scenario.Outcome == EBoundaryOutcome::ModuleDiscard)
		{
			RunModuleDiscard(
				Cases,
				Engine,
				*ScriptEngine,
				*Module,
				ModuleName,
				Lifecycle,
				State);
			return;
		}
		else
		{
			RunEngineShutdown(
				Cases,
				Engine,
				*ScriptEngine,
				*Module,
				ModuleName,
				Lifecycle,
				State);
			return;
		}

		const FTCHARToUTF8 ModuleNameUtf8(*ModuleName);
		ScriptEngine->DiscardModule(ModuleNameUtf8.Get());
		ASSERT_THAT(IsNull(ScriptEngine->GetModule(
			ModuleNameUtf8.Get(),
			asGM_ONLY_IF_EXISTS),
			*Cases[3].Describe(TEXT("constructor-boundary module should discard cleanly"))));
		if (Scenario.Outcome == EBoundaryOutcome::RuntimeSuccess)
		{
			VerifyRawClassScopeCleanup(Cases[3], Lifecycle);
			return;
		}
		ASSERT_THAT(AreEqual(0, Lifecycle.GetLiveObjectCount(),
			*Cases[3].Describe(TEXT("constructor-boundary discard should leave no tracked storage"))));
		VerifyBalancedLifecycle(Cases[3], Lifecycle, true);
	}

public:
	TEST_METHOD(ScenariosByObservation)
	{
		AS_NATIVE_PRODUCT("LANG-CTOR-BOUNDARY",
			AngelscriptNativeTestSupport::ENativeEvidence::Compile
				| AngelscriptNativeTestSupport::ENativeEvidence::Diagnostic
				| AngelscriptNativeTestSupport::ENativeEvidence::Runtime
				| AngelscriptNativeTestSupport::ENativeEvidence::Metadata
				| AngelscriptNativeTestSupport::ENativeEvidence::Lifecycle
				| AngelscriptNativeTestSupport::ENativeEvidence::Cleanup
				| AngelscriptNativeTestSupport::ENativeEvidence::Isolation);

		for (const FScenarioCase& Scenario : ScenarioCases)
		{
			RunCell(Scenario);
		}
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
