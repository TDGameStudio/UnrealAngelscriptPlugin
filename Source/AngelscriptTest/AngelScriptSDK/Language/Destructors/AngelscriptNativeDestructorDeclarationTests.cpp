#include "../../Support/AngelscriptNativeFixtureTestSupport.h"
#include "../../Support/AngelscriptNativeLanguageCaseTestSupport.h"

#include "CQTest.h"
#include "Misc/ScopeExit.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FDestructorDeclarationTests,
	"Angelscript.TestModule.AngelScriptSDK.Language.Destructors.Declaration",
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

	static constexpr asPWORD DestructorDeclarationStateUserDataSlot =
		static_cast<asPWORD>(0x44544F524445434Cull);

	enum class EDeclarationOutcome : uint8
	{
		Finished,
		Exception,
		FinishedIgnoringDestructorException,
		CompileRejected,
	};

	struct FScenarioCase
	{
		const ANSICHAR* CatalogName;
		EDeclarationOutcome Outcome;
		const ANSICHAR* PrimaryType;
		int32 ExpectedValue;
	};

	struct FObservationCase
	{
		const ANSICHAR* CatalogName;
	};

	inline static constexpr FScenarioCase ScenarioCases[] =
	{
		{
			"implicit_script_value",
			EDeclarationOutcome::Finished,
			"FImplicitValue",
			41,
		},
		{
			"declared_script_value",
			EDeclarationOutcome::Finished,
			"FDeclaredValue",
			42,
		},
		{
			"implicit_script_reference",
			EDeclarationOutcome::Finished,
			"FImplicitReference",
			43,
		},
		{
			"declared_script_reference",
			EDeclarationOutcome::Finished,
			"FDeclaredReference",
			44,
		},
		{
			"native_value",
			EDeclarationOutcome::Finished,
			"FNativeCaseValue",
			45,
		},
		{
			"native_reference",
			EDeclarationOutcome::Finished,
			"FNativeCaseReference",
			46,
		},
		{
			"empty_destructor",
			EDeclarationOutcome::Finished,
			"FEmptyDestructor",
			47,
		},
		{
			"field_destructor",
			EDeclarationOutcome::Finished,
			"FFieldOwner",
			48,
		},
		{
			"base_derived_destructor",
			EDeclarationOutcome::Finished,
			"FDestructorDerived",
			49,
		},
		{
			"private_destructor",
			EDeclarationOutcome::Finished,
			"FPrivateDestructor",
			50,
		},
		{
			"throwing_destructor",
			// The current fork deliberately ignores exceptions raised by a
			// nested script destructor. Keep this active as a characterization
			// of that policy while still verifying cleanup and the outer result.
			EDeclarationOutcome::FinishedIgnoringDestructorException,
			"FThrowingDestructor",
			51,
		},
		{
			"throwing_derived_members_base",
			// The ignored exception must not interrupt the remaining derived
			// member and base teardown sequence.
			EDeclarationOutcome::FinishedIgnoringDestructorException,
			"FThrowingDerived",
			52,
		},
		{
			"malformed_destructor",
			EDeclarationOutcome::CompileRejected,
			"FMalformedDestructor",
			0,
		},
	};

	inline static constexpr FObservationCase ObservationCases[] =
	{
		{ "compile" },
		{ "metadata" },
		{ "runtime" },
		{ "cleanup" },
	};

	struct FDestructorDeclarationState
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

	static FDestructorDeclarationState* GetActiveDeclarationState()
	{
		asIScriptContext* const Context = asGetActiveContext();
		return Context != nullptr && Context->GetEngine() != nullptr
			? static_cast<FDestructorDeclarationState*>(
				Context->GetEngine()->GetUserData(
					DestructorDeclarationStateUserDataSlot))
			: nullptr;
	}

	static void RecordDestructorDeclarationMarker(const int32 Marker)
	{
		if (FDestructorDeclarationState* const State =
			GetActiveDeclarationState())
		{
			State->Markers.Add(Marker);
		}
	}

	static void RaiseDeclaredDestructorException()
	{
		asIScriptContext* const Context = asGetActiveContext();
		if (Context != nullptr)
		{
			Context->SetException(
				"Declared destructor exception sentinel");
		}
	}

	static bool RegisterDestructorDeclarationBridge(
		asIScriptEngine& ScriptEngine,
		FDestructorDeclarationState& State)
	{
		ScriptEngine.SetUserData(
			&State,
			DestructorDeclarationStateUserDataSlot);
		const ASAutoCaller::FunctionCaller MarkerCaller =
			ASAutoCaller::MakeFunctionCaller(
				RecordDestructorDeclarationMarker);
		const ASAutoCaller::FunctionCaller ExceptionCaller =
			ASAutoCaller::MakeFunctionCaller(
				RaiseDeclaredDestructorException);
		return ScriptEngine.RegisterGlobalFunction(
			"void RecordDestructorDeclarationMarker(int Marker)",
			asFUNCTION(RecordDestructorDeclarationMarker),
			asCALL_CDECL,
			*(asFunctionCaller*)&MarkerCaller) >= 0
			&& ScriptEngine.RegisterGlobalFunction(
				"void RaiseDeclaredDestructorException()",
				asFUNCTION(RaiseDeclaredDestructorException),
				asCALL_CDECL,
				*(asFunctionCaller*)&ExceptionCaller) >= 0;
	}

	static void AppendImplicitScriptValue(FString& Source)
	{
		using namespace AngelscriptNativeTestSupport;

		AppendGeneratedAsLine(Source, TEXT("struct FImplicitValue"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tFNativeCaseValue Tracked;"));
		AppendGeneratedAsLine(Source, TEXT("\tint Value = 41;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("int RunDestructorDeclaration()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tFImplicitValue Object;"));
		AppendGeneratedAsLine(Source, TEXT("\treturn Object.Value;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
	}

	static void AppendDeclaredScriptValue(FString& Source)
	{
		using namespace AngelscriptNativeTestSupport;

		AppendGeneratedAsLine(Source, TEXT("struct FDeclaredValue"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tFNativeCaseValue Tracked;"));
		AppendGeneratedAsLine(Source, TEXT("\tint Value = 42;"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("\t~FDeclaredValue()"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(
			Source,
			TEXT("\t\tRecordDestructorDeclarationMarker(102);"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("int RunDestructorDeclaration()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tFDeclaredValue Object;"));
		AppendGeneratedAsLine(Source, TEXT("\treturn Object.Value;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
	}

	static void AppendImplicitScriptReference(FString& Source)
	{
		using namespace AngelscriptNativeTestSupport;

		AppendGeneratedAsLine(Source, TEXT("class FImplicitReference"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tFNativeCaseValue Tracked;"));
		AppendGeneratedAsLine(Source, TEXT("\tint Value = 43;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("int RunDestructorDeclaration()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(
			Source,
			TEXT("\tFImplicitReference Object = FImplicitReference();"));
		AppendGeneratedAsLine(Source, TEXT("\treturn Object.Value;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
	}

	static void AppendDeclaredScriptReference(FString& Source)
	{
		using namespace AngelscriptNativeTestSupport;

		AppendGeneratedAsLine(Source, TEXT("class FDeclaredReference"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tFNativeCaseValue Tracked;"));
		AppendGeneratedAsLine(Source, TEXT("\tint Value = 44;"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("\t~FDeclaredReference()"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(
			Source,
			TEXT("\t\tRecordDestructorDeclarationMarker(104);"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("int RunDestructorDeclaration()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(
			Source,
			TEXT("\tFDeclaredReference Object = FDeclaredReference();"));
		AppendGeneratedAsLine(Source, TEXT("\treturn Object.Value;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
	}

	static void AppendNativeValue(FString& Source)
	{
		using namespace AngelscriptNativeTestSupport;

		AppendGeneratedAsLine(Source, TEXT("int RunDestructorDeclaration()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(
			Source,
			TEXT("\tFNativeCaseValue Object(45);"));
		AppendGeneratedAsLine(Source, TEXT("\treturn Object.Value;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
	}

	static void AppendNativeReference(FString& Source)
	{
		using namespace AngelscriptNativeTestSupport;

		AppendGeneratedAsLine(Source, TEXT("int RunDestructorDeclaration()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(
			Source,
			TEXT("\tFNativeCaseReference Object = CreateNativeCaseReference(46);"));
		AppendGeneratedAsLine(Source, TEXT("\treturn Object.Value;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
	}

	static void AppendEmptyDestructor(FString& Source)
	{
		using namespace AngelscriptNativeTestSupport;

		AppendGeneratedAsLine(Source, TEXT("struct FEmptyDestructor"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tFNativeCaseValue Tracked;"));
		AppendGeneratedAsLine(Source, TEXT("\tint Value = 47;"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("\t~FEmptyDestructor()"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("int RunDestructorDeclaration()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tFEmptyDestructor Object;"));
		AppendGeneratedAsLine(Source, TEXT("\treturn Object.Value;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
	}

	static void AppendFieldDestructor(FString& Source)
	{
		using namespace AngelscriptNativeTestSupport;

		for (int32 Index = 1; Index <= 3; ++Index)
		{
			AppendGeneratedAsLine(Source, FString::Printf(
				TEXT("struct FDeclarationField%d"),
				Index));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(
				Source,
				TEXT("\tFNativeCaseValue Tracked;"));
			AppendGeneratedAsLine(Source);
			AppendGeneratedAsLine(Source, FString::Printf(
				TEXT("\t~FDeclarationField%d()"),
				Index));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, FString::Printf(
				TEXT("\t\tRecordDestructorDeclarationMarker(%d);"),
				300 + Index));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
			AppendGeneratedAsLine(Source, TEXT("}"));
			AppendGeneratedAsLine(Source);
		}
		AppendGeneratedAsLine(Source, TEXT("struct FFieldOwner"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tFDeclarationField1 First;"));
		AppendGeneratedAsLine(Source, TEXT("\tFDeclarationField2 Middle;"));
		AppendGeneratedAsLine(Source, TEXT("\tFDeclarationField3 Last;"));
		AppendGeneratedAsLine(Source, TEXT("\tint Value = 48;"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("\t~FFieldOwner()"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(
			Source,
			TEXT("\t\tRecordDestructorDeclarationMarker(399);"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("int RunDestructorDeclaration()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tFFieldOwner Object;"));
		AppendGeneratedAsLine(Source, TEXT("\treturn Object.Value;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
	}

	static void AppendBaseDerivedDestructor(FString& Source)
	{
		using namespace AngelscriptNativeTestSupport;

		AppendGeneratedAsLine(Source, TEXT("class FDestructorBase"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tFNativeCaseValue BaseTracked;"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("\t~FDestructorBase()"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(
			Source,
			TEXT("\t\tRecordDestructorDeclarationMarker(502);"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(
			Source,
			TEXT("class FDestructorDerived : FDestructorBase"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(
			Source,
			TEXT("\tFNativeCaseValue DerivedTracked;"));
		AppendGeneratedAsLine(Source, TEXT("\tint Value = 49;"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("\t~FDestructorDerived()"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(
			Source,
			TEXT("\t\tRecordDestructorDeclarationMarker(501);"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("int RunDestructorDeclaration()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(
			Source,
			TEXT("\tFDestructorDerived Object = FDestructorDerived();"));
		AppendGeneratedAsLine(Source, TEXT("\treturn Object.Value;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
	}

	static void AppendPrivateDestructor(FString& Source)
	{
		using namespace AngelscriptNativeTestSupport;

		AppendGeneratedAsLine(Source, TEXT("class FPrivateDestructor"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tFNativeCaseValue Tracked;"));
		AppendGeneratedAsLine(Source, TEXT("\tint Value = 50;"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(
			Source,
			TEXT("\tprivate ~FPrivateDestructor()"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(
			Source,
			TEXT("\t\tRecordDestructorDeclarationMarker(601);"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("int RunDestructorDeclaration()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(
			Source,
			TEXT("\tFPrivateDestructor Object = FPrivateDestructor();"));
		AppendGeneratedAsLine(Source, TEXT("\treturn Object.Value;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
	}

	static void AppendThrowingDestructor(FString& Source)
	{
		using namespace AngelscriptNativeTestSupport;

		AppendGeneratedAsLine(Source, TEXT("class FThrowingDestructor"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tFNativeCaseValue Tracked;"));
		AppendGeneratedAsLine(Source, TEXT("\tint Value = 51;"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("\t~FThrowingDestructor()"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(
			Source,
			TEXT("\t\tRecordDestructorDeclarationMarker(701);"));
		AppendGeneratedAsLine(
			Source,
			TEXT("\t\tRaiseDeclaredDestructorException();"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("int RunDestructorDeclaration()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(
			Source,
			TEXT("\tFThrowingDestructor Object = FThrowingDestructor();"));
		AppendGeneratedAsLine(Source, TEXT("\treturn Object.Value;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
	}

	static void AppendThrowingDerivedMembersBase(FString& Source)
	{
		using namespace AngelscriptNativeTestSupport;

		AppendGeneratedAsLine(Source, TEXT("struct FThrowingFirstMember"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tFNativeCaseValue Tracked;"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("\t~FThrowingFirstMember()"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(
			Source,
			TEXT("\t\tRecordDestructorDeclarationMarker(811);"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("struct FThrowingSecondMember"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tFNativeCaseValue Tracked;"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("\t~FThrowingSecondMember()"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(
			Source,
			TEXT("\t\tRecordDestructorDeclarationMarker(812);"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("class FThrowingBase"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tFNativeCaseValue BaseTracked;"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("\t~FThrowingBase()"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(
			Source,
			TEXT("\t\tRecordDestructorDeclarationMarker(802);"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(
			Source,
			TEXT("class FThrowingDerived : FThrowingBase"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tFThrowingFirstMember First;"));
		AppendGeneratedAsLine(Source, TEXT("\tFThrowingSecondMember Second;"));
		AppendGeneratedAsLine(Source, TEXT("\tint Value = 52;"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("\t~FThrowingDerived()"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(
			Source,
			TEXT("\t\tRecordDestructorDeclarationMarker(801);"));
		AppendGeneratedAsLine(
			Source,
			TEXT("\t\tRaiseDeclaredDestructorException();"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("int RunDestructorDeclaration()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(
			Source,
			TEXT("\tFThrowingDerived Object = FThrowingDerived();"));
		AppendGeneratedAsLine(Source, TEXT("\treturn Object.Value;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
	}

	static void AppendMalformedDestructor(FString& Source)
	{
		using namespace AngelscriptNativeTestSupport;

		AppendGeneratedAsLine(Source, TEXT("class FMalformedDestructor"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(
			Source,
			TEXT("\t~FMalformedDestructor(int InvalidParameter)"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source, TEXT("}"));
	}

	static void AppendRecoveryFunction(FString& Source)
	{
		using namespace AngelscriptNativeTestSupport;

		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(
			Source,
			TEXT("int RunDestructorDeclarationRecovery()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(
			Source,
			TEXT("\tFNativeCaseValue Recovery(97);"));
		AppendGeneratedAsLine(Source, TEXT("\treturn Recovery.Value;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
	}

	static FString BuildDestructorDeclarationSource(
		const FScenarioCase& Scenario)
	{
		FString Source;
		if (IsScenario(Scenario, "implicit_script_value"))
		{
			AppendImplicitScriptValue(Source);
		}
		else if (IsScenario(Scenario, "declared_script_value"))
		{
			AppendDeclaredScriptValue(Source);
		}
		else if (
			IsScenario(Scenario, "implicit_script_reference"))
		{
			AppendImplicitScriptReference(Source);
		}
		else if (
			IsScenario(Scenario, "declared_script_reference"))
		{
			AppendDeclaredScriptReference(Source);
		}
		else if (IsScenario(Scenario, "native_value"))
		{
			AppendNativeValue(Source);
		}
		else if (IsScenario(Scenario, "native_reference"))
		{
			AppendNativeReference(Source);
		}
		else if (IsScenario(Scenario, "empty_destructor"))
		{
			AppendEmptyDestructor(Source);
		}
		else if (IsScenario(Scenario, "field_destructor"))
		{
			AppendFieldDestructor(Source);
		}
		else if (
			IsScenario(Scenario, "base_derived_destructor"))
		{
			AppendBaseDerivedDestructor(Source);
		}
		else if (IsScenario(Scenario, "private_destructor"))
		{
			AppendPrivateDestructor(Source);
		}
		else if (IsScenario(Scenario, "throwing_destructor"))
		{
			AppendThrowingDestructor(Source);
		}
		else if (
			IsScenario(Scenario, "throwing_derived_members_base"))
		{
			AppendThrowingDerivedMembersBase(Source);
		}
		else
		{
			AppendMalformedDestructor(Source);
		}
		if (Scenario.Outcome != EDeclarationOutcome::CompileRejected)
		{
			AppendRecoveryFunction(Source);
		}
		return Source;
	}

	static FString BuildDestructorDeclarationRecoverySource()
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

	static asIScriptFunction* FindBehaviour(
		asITypeInfo& Type,
		const asEBehaviours ExpectedBehaviour)
	{
		for (asUINT Index = 0; Index < Type.GetBehaviourCount(); ++Index)
		{
			asEBehaviours Behaviour = asBEHAVE_CONSTRUCT;
			asIScriptFunction* const Function =
				Type.GetBehaviourByIndex(Index, &Behaviour);
			if (Behaviour == ExpectedBehaviour)
			{
				return Function;
			}
		}
		return nullptr;
	}

	static TArray<int32> ExpectedMarkers(
		const FScenarioCase& Scenario)
	{
		if (IsScenario(Scenario, "declared_script_value"))
		{
			return { 102 };
		}
		if (IsScenario(Scenario, "declared_script_reference"))
		{
			return { 104 };
		}
		if (IsScenario(Scenario, "field_destructor"))
		{
			return { 399, 303, 302, 301 };
		}
		if (IsScenario(Scenario, "base_derived_destructor"))
		{
			return { 501, 502 };
		}
		if (IsScenario(Scenario, "private_destructor"))
		{
			return { 601 };
		}
		if (IsScenario(Scenario, "throwing_destructor"))
		{
			return { 701 };
		}
		if (IsScenario(Scenario, "throwing_derived_members_base"))
		{
			return { 801, 812, 811, 802 };
		}
		return {};
	}

	void VerifyMetadata(
		const FNativeCaseContext& Case,
		const FScenarioCase& Scenario,
		asIScriptEngine& ScriptEngine,
		asIScriptModule& Module)
	{
		asITypeInfo* Type = nullptr;
		if (IsScenario(Scenario, "native_value")
			|| IsScenario(Scenario, "native_reference"))
		{
			Type = ScriptEngine.GetTypeInfoByDecl(
				Scenario.PrimaryType);
		}
		else
		{
			Type = Module.GetTypeInfoByName(Scenario.PrimaryType);
		}
		ASSERT_THAT(IsNotNull(Type,
			*Case.Describe(TEXT("destructor declaration should publish its primary type"))));
		if (Type == nullptr)
		{
			return;
		}

		if (IsScenario(Scenario, "native_reference"))
		{
			ASSERT_THAT(IsNotNull(
				FindBehaviour(*Type, asBEHAVE_RELEASE),
				*Case.Describe(TEXT("native reference should publish its release behavior"))));
		}
		else
		{
			asIScriptFunction* const Destructor =
				FindBehaviour(*Type, asBEHAVE_DESTRUCT);
			ASSERT_THAT(IsNotNull(Destructor,
				*Case.Describe(TEXT("destructor scenario should publish its destructor behavior"))));
			if (IsScenario(Scenario, "private_destructor")
				&& Destructor != nullptr)
			{
				ASSERT_THAT(IsTrue(Destructor->IsPrivate(),
					*Case.Describe(TEXT("private destructor metadata should retain its access flag"))));
			}
		}

		if (IsScenario(Scenario, "base_derived_destructor")
			|| IsScenario(
				Scenario,
				"throwing_derived_members_base"))
		{
			const ANSICHAR* const BaseTypeName =
				IsScenario(Scenario, "base_derived_destructor")
					? "FDestructorBase"
					: "FThrowingBase";
			asITypeInfo* const Base =
				Module.GetTypeInfoByName(BaseTypeName);
			ASSERT_THAT(IsNotNull(Base,
				*Case.Describe(TEXT("derived destructor scenario should publish its base type"))));
			if (Base != nullptr)
			{
				ASSERT_THAT(AreEqual(Base, Type->GetBaseType(),
					*Case.Describe(TEXT("derived destructor should retain its exact base relation"))));
				ASSERT_THAT(IsNotNull(
					FindBehaviour(*Base, asBEHAVE_DESTRUCT),
					*Case.Describe(TEXT("base type should publish its own destructor"))));
			}
		}
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
		if (bRequireConstruction)
		{
			ASSERT_THAT(IsTrue(ConstructionCount > 0,
				*Case.Describe(TEXT("destructor declaration should construct tracked storage"))));
		}
		ASSERT_THAT(AreEqual(0, Lifecycle.GetLiveObjectCount(),
			*Case.Describe(TEXT("destructor declaration should leave no live tracked storage"))));
		ASSERT_THAT(AreEqual(
			ConstructionCount,
			Lifecycle.Num(ENativeLifecycleEvent::Destruct),
			*Case.Describe(TEXT("destructor declaration should balance construction and destruction"))));

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
					*Case.Describe(TEXT("destructor declaration should destroy only constructed identities"))));
				ASSERT_THAT(IsFalse(
					DestructedIds.Contains(Entry.ObjectId),
					*Case.Describe(TEXT("destructor declaration should not destroy storage twice"))));
				DestructedIds.Add(Entry.ObjectId);
			}
		}
		ASSERT_THAT(AreEqual(
			ConstructedIds.Num(),
			DestructedIds.Num(),
			*Case.Describe(TEXT("destructor declaration identities should close exactly once"))));
	}

	void CompileAndExecuteRecovery(
		const FNativeCaseContext& Case,
		FNativeTestEngine& Engine,
		asIScriptEngine& ScriptEngine,
		const FString& ModuleName,
		FNativeLifecycleRecorder& Lifecycle,
		FDestructorDeclarationState& State)
	{
		using namespace AngelscriptNativeTestSupport;

		const FString RecoverySource =
			BuildDestructorDeclarationRecoverySource();
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
			*Case.Describe(TEXT("malformed destructor should allow same-name recovery"))));
		ASSERT_THAT(IsNotNull(RecoveryModule,
			*Case.Describe(TEXT("destructor recovery should publish its module"))));
		if (RecoveryModule == nullptr)
		{
			return;
		}
		asIScriptFunction* const Recovery =
			RecoveryModule->GetFunctionByDecl(
				"int RunDestructorDeclarationRecovery()");
		ASSERT_THAT(IsNotNull(Recovery,
			*Case.Describe(TEXT("destructor recovery should publish its exact function"))));
		asIScriptContext* const Context =
			ScriptEngine.CreateContext();
		ASSERT_THAT(IsNotNull(Context,
			*Case.Describe(TEXT("destructor recovery should create a context"))));
		if (Recovery != nullptr && Context != nullptr)
		{
			ASSERT_THAT(AreEqual(
				static_cast<int32>(asEXECUTION_FINISHED),
				PrepareAndExecute(Context, Recovery),
				*Case.Describe(TEXT("destructor recovery should finish"))));
			ASSERT_THAT(AreEqual(
				97,
				static_cast<int32>(Context->GetReturnDWord()),
				*Case.Describe(TEXT("destructor recovery should return its sentinel"))));
			ASSERT_THAT(AreEqual(
				asSUCCESS,
				Context->Unprepare(),
				*Case.Describe(TEXT("destructor recovery should unprepare cleanly"))));
			Context->Release();
		}
		else if (Context != nullptr)
		{
			Context->Release();
		}
		ASSERT_THAT(AreEqual(0, State.Markers.Num(),
			*Case.Describe(TEXT("destructor recovery should emit no declaration marker"))));
		VerifyBalancedLifecycle(Case, Lifecycle, true);

		const FTCHARToUTF8 ModuleNameUtf8(*ModuleName);
		ScriptEngine.DiscardModule(ModuleNameUtf8.Get());
	}

	void RunRejectedScenario(
		const TStaticArray<FNativeCaseContext, 4>& Cases,
		FNativeTestEngine& Engine,
		asIScriptEngine& ScriptEngine,
		const FString& ModuleName,
		const FString& Source,
		FNativeLifecycleRecorder& Lifecycle,
		FDestructorDeclarationState& State)
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
			*Cases[0].Describe(TEXT("malformed destructor declaration should fail to compile"))));
		ASSERT_THAT(IsTrue(
			Engine.GetMessages().Entries.ContainsByPredicate(
				[](const FNativeMessageEntry& Entry)
				{
					return Entry.Type == asMSGTYPE_ERROR
						&& Entry.Row > 0
						&& Entry.Column > 0
						&& Entry.Message.Contains(
							TEXT("destructor must not have any parameters"));
				}),
			*Cases[1].Describe(TEXT("malformed destructor should own the exact located parameter diagnostic"))));
		ASSERT_THAT(AreEqual(0, State.Markers.Num(),
			*Cases[2].Describe(TEXT("malformed destructor should execute no marker"))));
		VerifyBalancedLifecycle(Cases[2], Lifecycle, false);

		const FTCHARToUTF8 ModuleNameUtf8(*ModuleName);
		ScriptEngine.DiscardModule(ModuleNameUtf8.Get());
		CompileAndExecuteRecovery(
			Cases[3],
			Engine,
			ScriptEngine,
			ModuleName,
			Lifecycle,
			State);
	}

	void ExecuteScenario(
		const TStaticArray<FNativeCaseContext, 4>& Cases,
		const FScenarioCase& Scenario,
		asIScriptEngine& ScriptEngine,
		asIScriptModule& Module,
		FNativeLifecycleRecorder& Lifecycle,
		FDestructorDeclarationState& State)
	{
		using namespace AngelscriptNativeTestSupport;

		VerifyMetadata(
			Cases[1],
			Scenario,
			ScriptEngine,
			Module);
		asIScriptFunction* const Entry =
			Module.GetFunctionByDecl("int RunDestructorDeclaration()");
		asIScriptFunction* const Recovery =
			Module.GetFunctionByDecl(
				"int RunDestructorDeclarationRecovery()");
		ASSERT_THAT(IsNotNull(Entry,
			*Cases[0].Describe(TEXT("destructor declaration should publish its entry"))));
		ASSERT_THAT(IsNotNull(Recovery,
			*Cases[3].Describe(TEXT("destructor declaration should publish recovery"))));
		if (Entry == nullptr || Recovery == nullptr)
		{
			return;
		}

		asIScriptContext* const Context =
			ScriptEngine.CreateContext();
		ASSERT_THAT(IsNotNull(Context,
			*Cases[2].Describe(TEXT("destructor declaration should create a reusable context"))));
		if (Context == nullptr)
		{
			return;
		}
		const int32 ExpectedState =
			Scenario.Outcome == EDeclarationOutcome::Exception
				? static_cast<int32>(asEXECUTION_EXCEPTION)
				: static_cast<int32>(asEXECUTION_FINISHED);
		ASSERT_THAT(AreEqual(
			ExpectedState,
			PrepareAndExecute(Context, Entry),
			*Cases[2].Describe(TEXT("destructor declaration execution state should match current-fork policy"))));
		if (Scenario.Outcome == EDeclarationOutcome::Exception)
		{
			ASSERT_THAT(AreEqual(
				FString(TEXT("Declared destructor exception sentinel")),
				FString(UTF8_TO_TCHAR(
					Context->GetExceptionString())),
				*Cases[2].Describe(TEXT("throwing destructor should own its exact exception"))));
			ASSERT_THAT(IsTrue(
				Context->GetExceptionLineNumber() > 0,
				*Cases[2].Describe(TEXT("throwing destructor should own a source location"))));
		}
		else
		{
			ASSERT_THAT(AreEqual(
				Scenario.ExpectedValue,
				static_cast<int32>(Context->GetReturnDWord()),
				*Cases[2].Describe(TEXT("destructor declaration should preserve its runtime value"))));
		}
		ASSERT_THAT(AreEqual(
			asSUCCESS,
			Context->Unprepare(),
			*Cases[3].Describe(TEXT("destructor declaration context should release storage"))));
		ASSERT_THAT(AreEqual(
			ExpectedMarkers(Scenario),
			State.Markers,
			*Cases[2].Describe(TEXT("declared destructor markers should preserve exact derived/owner/field order"))));
		VerifyBalancedLifecycle(Cases[3], Lifecycle, true);

		const int32 MarkersBeforeRecovery = State.Markers.Num();
		ASSERT_THAT(IsTrue(Context->Prepare(Recovery) >= 0,
			*Cases[3].Describe(TEXT("destructor declaration context should prepare recovery"))));
		ASSERT_THAT(AreEqual(
			static_cast<int32>(asEXECUTION_FINISHED),
			Context->Execute(),
			*Cases[3].Describe(TEXT("destructor declaration recovery should finish"))));
		ASSERT_THAT(AreEqual(
			97,
			static_cast<int32>(Context->GetReturnDWord()),
			*Cases[3].Describe(TEXT("destructor declaration recovery should return its sentinel"))));
		ASSERT_THAT(AreEqual(
			MarkersBeforeRecovery,
			State.Markers.Num(),
			*Cases[3].Describe(TEXT("destructor recovery should invoke no declared destructor marker"))));
		ASSERT_THAT(AreEqual(
			asSUCCESS,
			Context->Unprepare(),
			*Cases[3].Describe(TEXT("destructor declaration recovery should unprepare cleanly"))));
		Context->Release();
		VerifyBalancedLifecycle(Cases[3], Lifecycle, true);
	}

	void RunCell(const FScenarioCase& Scenario)
	{
		using namespace AngelscriptNativeTestSupport;

		TStaticArray<FNativeCaseContext, 4> Cases =
		{
			FNativeCaseContext(MakeNativeCaseId(
				"LANG-DTOR-DECLARATION",
				{
					ANSI_TO_TCHAR(
						ObservationCases[0].CatalogName),
					ANSI_TO_TCHAR(Scenario.CatalogName),
				})),
			FNativeCaseContext(MakeNativeCaseId(
				"LANG-DTOR-DECLARATION",
				{
					ANSI_TO_TCHAR(
						ObservationCases[1].CatalogName),
					ANSI_TO_TCHAR(Scenario.CatalogName),
				})),
			FNativeCaseContext(MakeNativeCaseId(
				"LANG-DTOR-DECLARATION",
				{
					ANSI_TO_TCHAR(
						ObservationCases[2].CatalogName),
					ANSI_TO_TCHAR(Scenario.CatalogName),
				})),
			FNativeCaseContext(MakeNativeCaseId(
				"LANG-DTOR-DECLARATION",
				{
					ANSI_TO_TCHAR(
						ObservationCases[3].CatalogName),
					ANSI_TO_TCHAR(Scenario.CatalogName),
				})),
		};

		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };
		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine,
			*Cases[0].Describe(TEXT("destructor declaration should create a raw SDK engine"))));
		if (ScriptEngine == nullptr)
		{
			return;
		}

		FNativeLifecycleRecorder Lifecycle;
		FDestructorDeclarationState State;
		ASSERT_THAT(IsTrue(RegisterDestructorDeclarationBridge(
			*ScriptEngine,
			State),
			*Cases[0].Describe(TEXT("destructor declaration should register its marker bridge"))));
		ASSERT_THAT(IsTrue(RegisterNativeCaseValue(
			*ScriptEngine,
			Lifecycle),
			*Cases[0].Describe(TEXT("destructor declaration should register native values"))));
		ASSERT_THAT(IsTrue(RegisterNativeCaseReference(
			*ScriptEngine,
			&Lifecycle),
			*Cases[0].Describe(TEXT("destructor declaration should register native references"))));

		const FString ModuleName = FString::Printf(
			TEXT("DestructorDeclaration_%hs"),
			Scenario.CatalogName);
		const FString Source =
			BuildDestructorDeclarationSource(Scenario);
		Engine.ResetMessages();
		Lifecycle.Reset();
		State.Reset();
		if (Scenario.Outcome == EDeclarationOutcome::CompileRejected)
		{
			RunRejectedScenario(
				Cases,
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
			*Cases[0].Describe(TEXT("destructor declaration source should compile"))));
		ASSERT_THAT(IsNotNull(Module,
			*Cases[0].Describe(TEXT("destructor declaration source should publish its module"))));
		if (Module != nullptr)
		{
			ExecuteScenario(
				Cases,
				Scenario,
				*ScriptEngine,
				*Module,
				Lifecycle,
				State);
		}

		const TArray<int32> MarkersBeforeDiscard = State.Markers;
		const int32 DestructionCountBeforeDiscard =
			Lifecycle.Num(ENativeLifecycleEvent::Destruct);
		const FTCHARToUTF8 ModuleNameUtf8(*ModuleName);
		ScriptEngine->DiscardModule(ModuleNameUtf8.Get());
		ASSERT_THAT(IsNull(ScriptEngine->GetModule(
			ModuleNameUtf8.Get(),
			asGM_ONLY_IF_EXISTS),
			*Cases[3].Describe(TEXT("destructor declaration module should discard cleanly"))));
		ASSERT_THAT(AreEqual(0, Lifecycle.GetLiveObjectCount(),
			*Cases[3].Describe(TEXT("destructor declaration discard should leave no live storage"))));
		if (IsScenario(
			Scenario,
			"throwing_derived_members_base"))
		{
			ASSERT_THAT(AreEqual(
				MarkersBeforeDiscard,
				State.Markers,
				*Cases[3].Describe(TEXT("discard should not repeat the throwing derived teardown markers"))));
			ASSERT_THAT(AreEqual(
				DestructionCountBeforeDiscard,
				Lifecycle.Num(ENativeLifecycleEvent::Destruct),
				*Cases[3].Describe(TEXT("discard should not repeat tracked native destruction"))));
		}
	}

public:
	TEST_METHOD(ScenariosByObservation)
	{
		AS_NATIVE_PRODUCT("LANG-DTOR-DECLARATION",
			AngelscriptNativeTestSupport::ENativeEvidence::Compile
				| AngelscriptNativeTestSupport::ENativeEvidence::Diagnostic
				| AngelscriptNativeTestSupport::ENativeEvidence::Runtime
				| AngelscriptNativeTestSupport::ENativeEvidence::Metadata
				| AngelscriptNativeTestSupport::ENativeEvidence::Lifecycle
				| AngelscriptNativeTestSupport::ENativeEvidence::Cleanup);

		for (const FScenarioCase& Scenario : ScenarioCases)
		{
			RunCell(Scenario);
		}
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
