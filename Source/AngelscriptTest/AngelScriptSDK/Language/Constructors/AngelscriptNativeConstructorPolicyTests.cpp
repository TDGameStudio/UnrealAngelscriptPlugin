#include "../../Support/AngelscriptNativeFixtureTestSupport.h"
#include "../../Support/AngelscriptNativeLanguageCaseTestSupport.h"
#include "CQTest.h"
#include "Misc/ScopeExit.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FConstructorPolicyTests,
	"Angelscript.TestModule.AngelScriptSDK.Language.Constructors.Policy",
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

	static constexpr asPWORD ConstructorPolicyStateUserDataSlot =
		static_cast<asPWORD>(0x43544F52504F4C59ull);

	struct FScenario
	{
		const ANSICHAR* CatalogName;
		bool bDisableGeneratedDefaults;
		bool bExpectedBuild;
		const ANSICHAR* PrimaryTypeName;
		int32 ExpectedReturnValue;
		const TCHAR* ExpectedDiagnostic;
		bool bExpectDefaultConstructor;
		bool bExpectParameterizedConstructor;
		bool bExpectFactory;
		bool bExpectAssignment;
		bool bExpectDestructor;
		bool bExpectNativeCopy;
		bool bExpectNativeAssignment;
		bool bExpectTrackedLifecycle;
	};

	inline static constexpr FScenario Scenarios[] =
	{
		{
			"implicit_struct_default",
			false,
			true,
			"FPolicyValue",
			0,
			nullptr,
			true,
			false,
			false,
			false,
			true,
			false,
			false,
			true,
		},
		{
			"declared_struct_default",
			false,
			true,
			"FPolicyValue",
			12,
			nullptr,
			true,
			false,
			false,
			false,
			true,
			false,
			false,
			true,
		},
		{
			"parameter_preserves_generated_default",
			false,
			true,
			"FPolicyValue",
			13,
			nullptr,
			true,
			true,
			false,
			false,
			true,
			false,
			false,
			true,
		},
		{
			"parameter_suppresses_default_option_off",
			true,
			false,
			"FPolicyValue",
			0,
			TEXT("No default constructor for object of type 'FPolicyValue'"),
			false,
			true,
			false,
			true,
			false,
			false,
			false,
			false,
		},
		{
			"implicit_struct_copy",
			false,
			false,
			"FPolicyValue",
			30,
			TEXT("No matching signatures to 'FPolicyValue(FPolicyValue)'"),
			true,
			true,
			false,
			false,
			true,
			true,
			false,
			true,
		},
		{
			"declared_struct_copy",
			false,
			true,
			"FPolicyValue",
			32,
			nullptr,
			true,
			true,
			false,
			false,
			true,
			false,
			true,
			true,
		},
		{
			"implicit_struct_assignment",
			false,
			true,
			"FPolicyValue",
			34,
			nullptr,
			true,
			false,
			false,
			false,
			true,
			false,
			true,
			true,
		},
		{
			"declared_struct_assignment",
			false,
			true,
			"FPolicyValue",
			36,
			nullptr,
			true,
			false,
			false,
			true,
			true,
			false,
			true,
			true,
		},
		{
			"user_destructor_copy",
			false,
			false,
			"FPolicyValue",
			38,
			TEXT("No matching signatures to 'FPolicyValue(FPolicyValue)'"),
			true,
			true,
			false,
			true,
			false,
			true,
			false,
			true,
		},
		{
			"class_factory_default",
			false,
			true,
			"FPolicyClass",
			20,
			nullptr,
			true,
			false,
			true,
			false,
			true,
			false,
			false,
			true,
		},
		{
			"class_parameter_factory",
			false,
			true,
			"FPolicyClass",
			21,
			nullptr,
			true,
			true,
			true,
			false,
			true,
			false,
			false,
			true,
		},
		{
			"derived_generated_default",
			false,
			true,
			"FPolicyDerived",
			34,
			nullptr,
			true,
			false,
			true,
			false,
			true,
			false,
			false,
			true,
		},
		{
			"derived_explicit_super",
			false,
			true,
			"FPolicyDerived",
			26,
			nullptr,
			true,
			false,
			true,
			false,
			true,
			false,
			false,
			true,
		},
		{
			"missing_base_default_option_off",
			true,
			false,
			"FPolicyDerived",
			0,
			TEXT("Base class doesn't have default constructor"),
			true,
			false,
			true,
			true,
			false,
			false,
			false,
			false,
		},
		{
			"copy_after_user_constructor",
			false,
			false,
			"FPolicyValue",
			60,
			TEXT("No matching signatures to 'FPolicyValue(FPolicyValue)'"),
			true,
			true,
			false,
			true,
			true,
			true,
			false,
			true,
		},
		{
			"assignment_self_stability",
			false,
			true,
			"FPolicyValue",
			84,
			nullptr,
			true,
			false,
			false,
			true,
			true,
			false,
			true,
			true,
		},
	};

	struct FConstructorPolicyState
	{
		TArray<int32> Markers;

		void Reset()
		{
			Markers.Reset();
		}
	};

	static FConstructorPolicyState* GetActiveState()
	{
		asIScriptContext* const Context = asGetActiveContext();
		return Context != nullptr
			? static_cast<FConstructorPolicyState*>(
				Context->GetEngine()->GetUserData(ConstructorPolicyStateUserDataSlot))
			: nullptr;
	}

	static void RecordConstructorPolicyMarker(const int32 Marker)
	{
		if (FConstructorPolicyState* const State = GetActiveState())
		{
			State->Markers.Add(Marker);
		}
	}

	static bool RegisterConstructorPolicyBridge(
		asIScriptEngine& ScriptEngine,
		FConstructorPolicyState& State)
	{
		ScriptEngine.SetUserData(&State, ConstructorPolicyStateUserDataSlot);
		const ASAutoCaller::FunctionCaller MarkerCaller =
			ASAutoCaller::MakeFunctionCaller(RecordConstructorPolicyMarker);
		return ScriptEngine.RegisterGlobalFunction(
			"void RecordConstructorPolicyMarker(int Marker)",
			asFUNCTION(RecordConstructorPolicyMarker),
			asCALL_CDECL,
			*(asFunctionCaller*)&MarkerCaller) >= 0;
	}

	static bool IsScenario(const FScenario& Scenario, const ANSICHAR* CatalogName)
	{
		return FCStringAnsi::Strcmp(Scenario.CatalogName, CatalogName) == 0;
	}

	static void AppendRecoveryFunction(FString& Source)
	{
		using namespace AngelscriptNativeTestSupport;

		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("int RunConstructorPolicyRecovery()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\treturn 97;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
	}

	static void AppendImplicitStructDefault(FString& Source)
	{
		using namespace AngelscriptNativeTestSupport;

		AppendGeneratedAsLine(Source, TEXT("struct FPolicyValue"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tint Value = 0;"));
		AppendGeneratedAsLine(Source, TEXT("\tFNativeCaseValue Payload;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("int RunConstructorPolicy()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tFPolicyValue Value;"));
		AppendGeneratedAsLine(Source, TEXT("\tRecordConstructorPolicyMarker(101);"));
		AppendGeneratedAsLine(Source, TEXT("\treturn Value.Value + Value.Payload.Value;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
	}

	static void AppendDeclaredStructDefault(FString& Source)
	{
		using namespace AngelscriptNativeTestSupport;

		AppendGeneratedAsLine(Source, TEXT("struct FPolicyValue"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tint Value = 0;"));
		AppendGeneratedAsLine(Source, TEXT("\tFNativeCaseValue Payload;"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("\tFPolicyValue()"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(Source, TEXT("\t\tValue = 12;"));
		AppendGeneratedAsLine(Source, TEXT("\t\tRecordConstructorPolicyMarker(201);"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("int RunConstructorPolicy()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tFPolicyValue Value;"));
		AppendGeneratedAsLine(Source, TEXT("\tRecordConstructorPolicyMarker(202);"));
		AppendGeneratedAsLine(Source, TEXT("\treturn Value.Value + Value.Payload.Value;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
	}

	static void AppendParameterPreservesDefault(FString& Source)
	{
		using namespace AngelscriptNativeTestSupport;

		AppendGeneratedAsLine(Source, TEXT("struct FPolicyValue"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tint Value = 0;"));
		AppendGeneratedAsLine(Source, TEXT("\tFNativeCaseValue Payload;"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("\tFPolicyValue(int InValue)"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(Source, TEXT("\t\tValue = InValue;"));
		AppendGeneratedAsLine(Source, TEXT("\t\tPayload.Value = InValue;"));
		AppendGeneratedAsLine(Source, TEXT("\t\tRecordConstructorPolicyMarker(301);"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("int RunConstructorPolicy()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tFPolicyValue DefaultValue;"));
		AppendGeneratedAsLine(Source, TEXT("\tFPolicyValue ParameterValue(13);"));
		AppendGeneratedAsLine(Source, TEXT("\tRecordConstructorPolicyMarker(302);"));
		AppendGeneratedAsLine(Source, TEXT("\treturn DefaultValue.Value + ParameterValue.Value;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
	}

	static void AppendParameterSuppressesDefault(FString& Source)
	{
		using namespace AngelscriptNativeTestSupport;

		AppendGeneratedAsLine(Source, TEXT("struct FPolicyValue"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tint Value = 0;"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("\tFPolicyValue(int InValue)"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(Source, TEXT("\t\tValue = InValue;"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("int RunConstructorPolicy()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tFPolicyValue Value;"));
		AppendGeneratedAsLine(Source, TEXT("\treturn Value.Value;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
	}

	static void AppendImplicitStructCopy(FString& Source)
	{
		using namespace AngelscriptNativeTestSupport;

		AppendGeneratedAsLine(Source, TEXT("struct FPolicyValue"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tint Value = 0;"));
		AppendGeneratedAsLine(Source, TEXT("\tFNativeCaseValue Payload;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("int RunConstructorPolicy()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tFPolicyValue Source;"));
		AppendGeneratedAsLine(Source, TEXT("\tSource.Value = 15;"));
		AppendGeneratedAsLine(Source, TEXT("\tSource.Payload.Value = 15;"));
		AppendGeneratedAsLine(Source, TEXT("\tFPolicyValue Target(Source);"));
		AppendGeneratedAsLine(Source, TEXT("\tRecordConstructorPolicyMarker(501);"));
		AppendGeneratedAsLine(Source, TEXT("\treturn Target.Value + Target.Payload.Value;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
	}

	static void AppendDeclaredStructCopy(FString& Source)
	{
		using namespace AngelscriptNativeTestSupport;

		AppendGeneratedAsLine(Source, TEXT("struct FPolicyValue"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tint Value = 0;"));
		AppendGeneratedAsLine(Source, TEXT("\tFNativeCaseValue Payload;"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("\tFPolicyValue()"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(Source, TEXT("\t\tRecordConstructorPolicyMarker(601);"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("\tFPolicyValue(const FPolicyValue& Other)"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(Source, TEXT("\t\tValue = Other.Value;"));
		AppendGeneratedAsLine(Source, TEXT("\t\tPayload = Other.Payload;"));
		AppendGeneratedAsLine(Source, TEXT("\t\tRecordConstructorPolicyMarker(602);"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("int RunConstructorPolicy()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tFPolicyValue Source;"));
		AppendGeneratedAsLine(Source, TEXT("\tSource.Value = 16;"));
		AppendGeneratedAsLine(Source, TEXT("\tSource.Payload.Value = 16;"));
		AppendGeneratedAsLine(Source, TEXT("\tFPolicyValue Target(Source);"));
		AppendGeneratedAsLine(Source, TEXT("\tRecordConstructorPolicyMarker(603);"));
		AppendGeneratedAsLine(Source, TEXT("\treturn Target.Value + Target.Payload.Value;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
	}

	static void AppendImplicitStructAssignment(FString& Source)
	{
		using namespace AngelscriptNativeTestSupport;

		AppendGeneratedAsLine(Source, TEXT("struct FPolicyValue"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tint Value = 0;"));
		AppendGeneratedAsLine(Source, TEXT("\tFNativeCaseValue Payload;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("int RunConstructorPolicy()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tFPolicyValue Source;"));
		AppendGeneratedAsLine(Source, TEXT("\tSource.Value = 17;"));
		AppendGeneratedAsLine(Source, TEXT("\tSource.Payload.Value = 17;"));
		AppendGeneratedAsLine(Source, TEXT("\tFPolicyValue Target;"));
		AppendGeneratedAsLine(Source, TEXT("\tTarget = Source;"));
		AppendGeneratedAsLine(Source, TEXT("\tRecordConstructorPolicyMarker(701);"));
		AppendGeneratedAsLine(Source, TEXT("\treturn Target.Value + Target.Payload.Value;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
	}

	static void AppendDeclaredStructAssignment(FString& Source)
	{
		using namespace AngelscriptNativeTestSupport;

		AppendGeneratedAsLine(Source, TEXT("struct FPolicyValue"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tint Value = 0;"));
		AppendGeneratedAsLine(Source, TEXT("\tFNativeCaseValue Payload;"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("\tFPolicyValue& opAssign(const FPolicyValue& Other)"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(Source, TEXT("\t\tValue = Other.Value;"));
		AppendGeneratedAsLine(Source, TEXT("\t\tPayload = Other.Payload;"));
		AppendGeneratedAsLine(Source, TEXT("\t\tRecordConstructorPolicyMarker(801);"));
		AppendGeneratedAsLine(Source, TEXT("\t\treturn this;"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("int RunConstructorPolicy()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tFPolicyValue Source;"));
		AppendGeneratedAsLine(Source, TEXT("\tSource.Value = 18;"));
		AppendGeneratedAsLine(Source, TEXT("\tSource.Payload.Value = 18;"));
		AppendGeneratedAsLine(Source, TEXT("\tFPolicyValue Target;"));
		AppendGeneratedAsLine(Source, TEXT("\tTarget = Source;"));
		AppendGeneratedAsLine(Source, TEXT("\tRecordConstructorPolicyMarker(802);"));
		AppendGeneratedAsLine(Source, TEXT("\treturn Target.Value + Target.Payload.Value;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
	}

	static void AppendUserDestructorCopy(FString& Source)
	{
		using namespace AngelscriptNativeTestSupport;

		AppendGeneratedAsLine(Source, TEXT("struct FPolicyValue"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tint Value = 0;"));
		AppendGeneratedAsLine(Source, TEXT("\tFNativeCaseValue Payload;"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("\t~FPolicyValue()"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(Source, TEXT("\t\tRecordConstructorPolicyMarker(902);"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("int RunConstructorPolicy()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tFPolicyValue Source;"));
		AppendGeneratedAsLine(Source, TEXT("\tSource.Value = 19;"));
		AppendGeneratedAsLine(Source, TEXT("\tSource.Payload.Value = 19;"));
		AppendGeneratedAsLine(Source, TEXT("\tFPolicyValue Target(Source);"));
		AppendGeneratedAsLine(Source, TEXT("\tRecordConstructorPolicyMarker(901);"));
		AppendGeneratedAsLine(Source, TEXT("\treturn Target.Value + Target.Payload.Value;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
	}

	static void AppendClassFactoryDefault(FString& Source)
	{
		using namespace AngelscriptNativeTestSupport;

		AppendGeneratedAsLine(Source, TEXT("class FPolicyClass"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tint Value = 0;"));
		AppendGeneratedAsLine(Source, TEXT("\tint ObjectId = -1;"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("\tFPolicyClass()"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(Source, TEXT("\t\tValue = 20;"));
		AppendGeneratedAsLine(Source, TEXT("\t\tObjectId = BeginNativeScriptLifecycle(Value);"));
		AppendGeneratedAsLine(Source, TEXT("\t\tRecordConstructorPolicyMarker(1001);"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("\t~FPolicyClass()"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(Source, TEXT("\t\tRecordConstructorPolicyMarker(1003);"));
		AppendGeneratedAsLine(Source, TEXT("\t\tEndNativeScriptLifecycle(ObjectId, Value);"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("int RunConstructorPolicy()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tFPolicyClass Value = FPolicyClass();"));
		AppendGeneratedAsLine(Source, TEXT("\tRecordConstructorPolicyMarker(1002);"));
		AppendGeneratedAsLine(Source, TEXT("\treturn Value.Value;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
	}

	static void AppendClassParameterFactory(FString& Source)
	{
		using namespace AngelscriptNativeTestSupport;

		AppendGeneratedAsLine(Source, TEXT("class FPolicyClass"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tint Value = 0;"));
		AppendGeneratedAsLine(Source, TEXT("\tint ObjectId = -1;"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("\tFPolicyClass(int InValue)"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(Source, TEXT("\t\tValue = InValue;"));
		AppendGeneratedAsLine(Source, TEXT("\t\tObjectId = BeginNativeScriptLifecycle(Value);"));
		AppendGeneratedAsLine(Source, TEXT("\t\tRecordConstructorPolicyMarker(1101);"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("\t~FPolicyClass()"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(Source, TEXT("\t\tRecordConstructorPolicyMarker(1103);"));
		AppendGeneratedAsLine(Source, TEXT("\t\tEndNativeScriptLifecycle(ObjectId, Value);"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("int RunConstructorPolicy()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tFPolicyClass Value = FPolicyClass(21);"));
		AppendGeneratedAsLine(Source, TEXT("\tRecordConstructorPolicyMarker(1102);"));
		AppendGeneratedAsLine(Source, TEXT("\treturn Value.Value;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
	}

	static void AppendDerivedGeneratedDefault(FString& Source)
	{
		using namespace AngelscriptNativeTestSupport;

		AppendGeneratedAsLine(Source, TEXT("class FPolicyBase"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tint BaseValue = 0;"));
		AppendGeneratedAsLine(Source, TEXT("\tint ObjectId = -1;"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("\tFPolicyBase()"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(Source, TEXT("\t\tBaseValue = 22;"));
		AppendGeneratedAsLine(Source, TEXT("\t\tObjectId = BeginNativeScriptLifecycle(BaseValue);"));
		AppendGeneratedAsLine(Source, TEXT("\t\tRecordConstructorPolicyMarker(1201);"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("\t~FPolicyBase()"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(Source, TEXT("\t\tRecordConstructorPolicyMarker(1203);"));
		AppendGeneratedAsLine(Source, TEXT("\t\tEndNativeScriptLifecycle(ObjectId, BaseValue);"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("class FPolicyDerived : FPolicyBase"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tint DerivedValue = 12;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("int RunConstructorPolicy()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tFPolicyDerived Value = FPolicyDerived();"));
		AppendGeneratedAsLine(Source, TEXT("\tRecordConstructorPolicyMarker(1202);"));
		AppendGeneratedAsLine(Source, TEXT("\treturn Value.BaseValue + Value.DerivedValue;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
	}

	static void AppendDerivedExplicitSuper(FString& Source)
	{
		using namespace AngelscriptNativeTestSupport;

		AppendGeneratedAsLine(Source, TEXT("class FPolicyBase"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tint BaseValue = 0;"));
		AppendGeneratedAsLine(Source, TEXT("\tint ObjectId = -1;"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("\tFPolicyBase(int InValue)"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(Source, TEXT("\t\tBaseValue = InValue;"));
		AppendGeneratedAsLine(Source, TEXT("\t\tObjectId = BeginNativeScriptLifecycle(BaseValue);"));
		AppendGeneratedAsLine(Source, TEXT("\t\tRecordConstructorPolicyMarker(1301);"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("\t~FPolicyBase()"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(Source, TEXT("\t\tRecordConstructorPolicyMarker(1304);"));
		AppendGeneratedAsLine(Source, TEXT("\t\tEndNativeScriptLifecycle(ObjectId, BaseValue);"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("class FPolicyDerived : FPolicyBase"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tint DerivedValue = 13;"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("\tFPolicyDerived()"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(Source, TEXT("\t\tsuper(13);"));
		AppendGeneratedAsLine(Source, TEXT("\t\tRecordConstructorPolicyMarker(1302);"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("int RunConstructorPolicy()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tFPolicyDerived Value = FPolicyDerived();"));
		AppendGeneratedAsLine(Source, TEXT("\tRecordConstructorPolicyMarker(1303);"));
		AppendGeneratedAsLine(Source, TEXT("\treturn Value.BaseValue + Value.DerivedValue;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
	}

	static void AppendMissingBaseDefault(FString& Source)
	{
		using namespace AngelscriptNativeTestSupport;

		AppendGeneratedAsLine(Source, TEXT("class FPolicyBase"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tFPolicyBase(int InValue)"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("class FPolicyDerived : FPolicyBase"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tFPolicyDerived()"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("int RunConstructorPolicy()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tFPolicyDerived Value = FPolicyDerived();"));
		AppendGeneratedAsLine(Source, TEXT("\treturn 1;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
	}

	static void AppendCopyAfterUserConstructor(FString& Source)
	{
		using namespace AngelscriptNativeTestSupport;

		AppendGeneratedAsLine(Source, TEXT("struct FPolicyValue"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tint Value = 0;"));
		AppendGeneratedAsLine(Source, TEXT("\tFNativeCaseValue Payload;"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("\tFPolicyValue(int InValue)"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(Source, TEXT("\t\tValue = InValue;"));
		AppendGeneratedAsLine(Source, TEXT("\t\tPayload.Value = InValue;"));
		AppendGeneratedAsLine(Source, TEXT("\t\tRecordConstructorPolicyMarker(1501);"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("int RunConstructorPolicy()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tFPolicyValue Source(15);"));
		AppendGeneratedAsLine(Source, TEXT("\tFPolicyValue Target(Source);"));
		AppendGeneratedAsLine(Source, TEXT("\tRecordConstructorPolicyMarker(1502);"));
		AppendGeneratedAsLine(Source, TEXT("\treturn Source.Value + Target.Value + Target.Payload.Value + Source.Payload.Value;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
	}

	static void AppendAssignmentSelfStability(FString& Source)
	{
		using namespace AngelscriptNativeTestSupport;

		AppendGeneratedAsLine(Source, TEXT("struct FPolicyValue"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tint Value = 0;"));
		AppendGeneratedAsLine(Source, TEXT("\tFNativeCaseValue Payload;"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("\tFPolicyValue& opAssign(const FPolicyValue& Other)"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(Source, TEXT("\t\tValue = Other.Value;"));
		AppendGeneratedAsLine(Source, TEXT("\t\tPayload = Other.Payload;"));
		AppendGeneratedAsLine(Source, TEXT("\t\tRecordConstructorPolicyMarker(1601);"));
		AppendGeneratedAsLine(Source, TEXT("\t\treturn this;"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("int RunConstructorPolicy()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tFPolicyValue Value;"));
		AppendGeneratedAsLine(Source, TEXT("\tValue.Value = 42;"));
		AppendGeneratedAsLine(Source, TEXT("\tValue.Payload.Value = 42;"));
		AppendGeneratedAsLine(Source, TEXT("\tValue = Value;"));
		AppendGeneratedAsLine(Source, TEXT("\tRecordConstructorPolicyMarker(1602);"));
		AppendGeneratedAsLine(Source, TEXT("\treturn Value.Value + Value.Payload.Value;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
	}

	static FString BuildConstructorPolicySource(const FScenario& Scenario)
	{
		FString Source;
		if (IsScenario(Scenario, "implicit_struct_default"))
		{
			AppendImplicitStructDefault(Source);
		}
		else if (IsScenario(Scenario, "declared_struct_default"))
		{
			AppendDeclaredStructDefault(Source);
		}
		else if (IsScenario(Scenario, "parameter_preserves_generated_default"))
		{
			AppendParameterPreservesDefault(Source);
		}
		else if (IsScenario(Scenario, "parameter_suppresses_default_option_off"))
		{
			AppendParameterSuppressesDefault(Source);
		}
		else if (IsScenario(Scenario, "implicit_struct_copy"))
		{
			AppendImplicitStructCopy(Source);
		}
		else if (IsScenario(Scenario, "declared_struct_copy"))
		{
			AppendDeclaredStructCopy(Source);
		}
		else if (IsScenario(Scenario, "implicit_struct_assignment"))
		{
			AppendImplicitStructAssignment(Source);
		}
		else if (IsScenario(Scenario, "declared_struct_assignment"))
		{
			AppendDeclaredStructAssignment(Source);
		}
		else if (IsScenario(Scenario, "user_destructor_copy"))
		{
			AppendUserDestructorCopy(Source);
		}
		else if (IsScenario(Scenario, "class_factory_default"))
		{
			AppendClassFactoryDefault(Source);
		}
		else if (IsScenario(Scenario, "class_parameter_factory"))
		{
			AppendClassParameterFactory(Source);
		}
		else if (IsScenario(Scenario, "derived_generated_default"))
		{
			AppendDerivedGeneratedDefault(Source);
		}
		else if (IsScenario(Scenario, "derived_explicit_super"))
		{
			AppendDerivedExplicitSuper(Source);
		}
		else if (IsScenario(Scenario, "missing_base_default_option_off"))
		{
			AppendMissingBaseDefault(Source);
		}
		else if (IsScenario(Scenario, "copy_after_user_constructor"))
		{
			AppendCopyAfterUserConstructor(Source);
		}
		else
		{
			AppendAssignmentSelfStability(Source);
		}
		AppendRecoveryFunction(Source);
		return Source;
	}

	static FString BuildConstructorPolicyRecoverySource()
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

	static int32 CountBehaviours(
		asITypeInfo& Type,
		const asEBehaviours ExpectedBehaviour,
		const int32 ParameterCount = INDEX_NONE)
	{
		int32 Count = 0;
		for (asUINT Index = 0; Index < Type.GetBehaviourCount(); ++Index)
		{
			asEBehaviours Behaviour = asBEHAVE_CONSTRUCT;
			asIScriptFunction* const Function =
				Type.GetBehaviourByIndex(Index, &Behaviour);
			if (Function != nullptr
				&& Behaviour == ExpectedBehaviour
				&& (ParameterCount == INDEX_NONE
					|| static_cast<int32>(Function->GetParamCount()) == ParameterCount))
			{
				++Count;
			}
		}
		return Count;
	}

	static int32 CountMethodsNamed(asITypeInfo& Type, const ANSICHAR* Name)
	{
		int32 Count = 0;
		for (asUINT Index = 0; Index < Type.GetMethodCount(); ++Index)
		{
			asIScriptFunction* const Method = Type.GetMethodByIndex(Index);
			if (Method != nullptr
				&& FCStringAnsi::Strcmp(Method->GetName(), Name) == 0)
			{
				++Count;
			}
		}
		return Count;
	}

	static FString DescribeTypeMetadata(const asITypeInfo& Type)
	{
		FString Behaviours;
		for (asUINT Index = 0; Index < Type.GetBehaviourCount(); ++Index)
		{
			asEBehaviours Behaviour = asBEHAVE_CONSTRUCT;
			asIScriptFunction* const Function =
				Type.GetBehaviourByIndex(Index, &Behaviour);
			if (!Behaviours.IsEmpty())
			{
				Behaviours += TEXT(", ");
			}
			const char* const Declaration =
				Function != nullptr ? Function->GetDeclaration() : nullptr;
			Behaviours += FString::Printf(
				TEXT("%d:%s"),
				static_cast<int32>(Behaviour),
				Declaration != nullptr ? UTF8_TO_TCHAR(Declaration) : TEXT("<null>"));
		}

		FString MethodDeclarations;
		for (asUINT Index = 0; Index < Type.GetMethodCount(); ++Index)
		{
			asIScriptFunction* const Method = Type.GetMethodByIndex(Index);
			if (!MethodDeclarations.IsEmpty())
			{
				MethodDeclarations += TEXT(", ");
			}
			const char* const Declaration =
				Method != nullptr ? Method->GetDeclaration() : nullptr;
			MethodDeclarations += Declaration != nullptr
				? UTF8_TO_TCHAR(Declaration)
				: TEXT("<null>");
		}

		return FString::Printf(
			TEXT("Type='%s' Behaviours=[%s] Factories=%u Methods=[%s]"),
			UTF8_TO_TCHAR(Type.GetName()),
			*Behaviours,
			Type.GetFactoryCount(),
			*MethodDeclarations);
	}

	static bool HasErrorContaining(
		const AngelscriptNativeTestSupport::FNativeMessageCollector& Messages,
		const TCHAR* ExpectedText)
	{
		return Messages.Entries.ContainsByPredicate(
			[ExpectedText](const AngelscriptNativeTestSupport::FNativeMessageEntry& Entry)
			{
				return Entry.Type == asMSGTYPE_ERROR
					&& Entry.Row > 0
					&& Entry.Column > 0
					&& Entry.Message.Contains(ExpectedText);
			});
	}

	static TArray<int32> ExpectedMarkers(const FScenario& Scenario)
	{
		if (IsScenario(Scenario, "implicit_struct_default"))
		{
			return { 101 };
		}
		if (IsScenario(Scenario, "declared_struct_default"))
		{
			return { 201, 202 };
		}
		if (IsScenario(Scenario, "parameter_preserves_generated_default"))
		{
			return { 301, 302 };
		}
		if (IsScenario(Scenario, "implicit_struct_copy"))
		{
			return { 501 };
		}
		if (IsScenario(Scenario, "declared_struct_copy"))
		{
			return { 601, 602, 603 };
		}
		if (IsScenario(Scenario, "implicit_struct_assignment"))
		{
			return { 701 };
		}
		if (IsScenario(Scenario, "declared_struct_assignment"))
		{
			return { 801, 802 };
		}
		if (IsScenario(Scenario, "user_destructor_copy"))
		{
			return { 901, 902, 902 };
		}
		if (IsScenario(Scenario, "class_factory_default"))
		{
			return { 1001, 1002, 1003 };
		}
		if (IsScenario(Scenario, "class_parameter_factory"))
		{
			return { 1101, 1102, 1103 };
		}
		if (IsScenario(Scenario, "derived_generated_default"))
		{
			return { 1201, 1202, 1203 };
		}
		if (IsScenario(Scenario, "derived_explicit_super"))
		{
			return { 1301, 1302, 1303, 1304 };
		}
		if (IsScenario(Scenario, "copy_after_user_constructor"))
		{
			return { 1501, 1502 };
		}
		if (IsScenario(Scenario, "assignment_self_stability"))
		{
			return { 1601, 1602 };
		}
		return {};
	}

	void VerifyCompileObservation(
		const FNativeCaseContext& Case,
		const FScenario& Scenario,
		const int BuildResult,
		asIScriptModule* Module,
		const FNativeTestEngine& Engine)
	{
		ASSERT_THAT(AreEqual(
			Scenario.bExpectedBuild,
			BuildResult >= 0,
			*Case.Describe(TEXT("constructor-policy build result should match the catalog outcome"))));
		if (Scenario.bExpectedBuild)
		{
			ASSERT_THAT(IsNotNull(Module,
				*Case.Describe(TEXT("successful constructor-policy build should publish its module"))));
			ASSERT_THAT(IsFalse(Engine.GetMessages().Entries.ContainsByPredicate(
				[](const FNativeMessageEntry& Entry)
				{
					return Entry.Type == asMSGTYPE_ERROR;
				}),
				*Case.Describe(TEXT("successful constructor-policy build should emit no errors"))));
		}
		else
		{
			ASSERT_THAT(IsTrue(BuildResult < 0,
				*Case.Describe(TEXT("rejected constructor-policy source should return a negative build code"))));
			ASSERT_THAT(IsTrue(HasErrorContaining(
				Engine.GetMessages(),
				Scenario.ExpectedDiagnostic),
				*Case.Describe(TEXT("rejected constructor-policy source should own its located exact diagnostic"))));
		}
	}

	void VerifyMetadataObservation(
		const FNativeCaseContext& Case,
		const FScenario& Scenario,
		asIScriptModule* Module)
	{
		if (!Scenario.bExpectedBuild)
		{
			ASSERT_THAT(IsTrue(Module == nullptr
				|| Module->GetFunctionByDecl("int RunConstructorPolicy()") == nullptr,
				*Case.Describe(TEXT("failed constructor-policy module should publish no callable entry"))));
			return;
		}

		ASSERT_THAT(IsNotNull(Module,
			*Case.Describe(TEXT("constructor-policy metadata requires a built module"))));
		if (Module == nullptr)
		{
			return;
		}
		asITypeInfo* const Type = Module->GetTypeInfoByName(Scenario.PrimaryTypeName);
		ASSERT_THAT(IsNotNull(Type,
			*Case.Describe(TEXT("constructor-policy module should publish its primary type"))));
		if (Type == nullptr)
		{
			return;
		}

		const int32 DefaultConstructorCount =
			CountBehaviours(*Type, asBEHAVE_CONSTRUCT, 0);
		const int32 ParameterizedConstructorCount =
			CountBehaviours(*Type, asBEHAVE_CONSTRUCT) - DefaultConstructorCount;
		ASSERT_THAT(AreEqual(
			Scenario.bExpectDefaultConstructor,
			DefaultConstructorCount > 0,
			*Case.Describe(TEXT("default-constructor metadata should match current fork policy"))));
		ASSERT_THAT(AreEqual(
			Scenario.bExpectParameterizedConstructor,
			ParameterizedConstructorCount > 0,
			*Case.Describe(TEXT("parameterized/copy constructor metadata should match the scenario"))));
		ASSERT_THAT(AreEqual(
			Scenario.bExpectFactory,
			Type->GetFactoryCount() > 0,
			*Case.Describe(TEXT("factory metadata should distinguish class and struct construction"))));
		const bool bHasAssignment = CountMethodsNamed(*Type, "opAssign") > 0;
		if (Scenario.bExpectAssignment != bHasAssignment)
		{
			UE_LOG(LogTemp, Display,
				TEXT("[AS-CTOR-POLICY-METADATA] Id=%s ExpectedAssignment=%d ActualAssignment=%d %s"),
				*Case.GetId(),
				Scenario.bExpectAssignment ? 1 : 0,
				bHasAssignment ? 1 : 0,
				*DescribeTypeMetadata(*Type));
		}
		ASSERT_THAT(AreEqual(
			Scenario.bExpectAssignment,
			bHasAssignment,
			*Case.Describe(TEXT("assignment metadata should publish the generated or declared operation"))));
		ASSERT_THAT(AreEqual(
			Scenario.bExpectDestructor,
			CountBehaviours(*Type, asBEHAVE_DESTRUCT) > 0,
			*Case.Describe(TEXT("destructor metadata should reflect tracked fields, inheritance, or user declarations"))));

		asIScriptFunction* const Entry =
			Module->GetFunctionByDecl("int RunConstructorPolicy()");
		ASSERT_THAT(IsNotNull(Entry,
			*Case.Describe(TEXT("constructor-policy module should publish its exact runtime entry"))));
		if (Entry != nullptr)
		{
			ASSERT_THAT(IsTrue(Entry->GetScriptSectionName() != nullptr,
				*Case.Describe(TEXT("constructor-policy entry should retain source-section metadata"))));
		}
	}

	void VerifyRuntimeObservation(
		const FNativeCaseContext& Case,
		const FScenario& Scenario,
		asIScriptEngine& ScriptEngine,
		asIScriptModule* Module,
		FConstructorPolicyState& State)
	{
		if (!Scenario.bExpectedBuild)
		{
			ASSERT_THAT(AreEqual(0, State.Markers.Num(),
				*Case.Describe(TEXT("compile-time rejection should execute no constructor-policy marker"))));
			return;
		}
		if (Module == nullptr)
		{
			return;
		}

		asIScriptFunction* const Entry =
			Module->GetFunctionByDecl("int RunConstructorPolicy()");
		asIScriptFunction* const Recovery =
			Module->GetFunctionByDecl("int RunConstructorPolicyRecovery()");
		ASSERT_THAT(IsNotNull(Entry,
			*Case.Describe(TEXT("runtime observation should resolve the constructor-policy entry"))));
		ASSERT_THAT(IsNotNull(Recovery,
			*Case.Describe(TEXT("runtime observation should resolve the recovery entry"))));
		if (Entry == nullptr || Recovery == nullptr)
		{
			return;
		}

		asIScriptContext* const Context = ScriptEngine.CreateContext();
		ASSERT_THAT(IsNotNull(Context,
			*Case.Describe(TEXT("constructor-policy scenario should create a reusable context"))));
		if (Context == nullptr)
		{
			return;
		}

		ASSERT_THAT(AreEqual(
			static_cast<int32>(asEXECUTION_FINISHED),
			AngelscriptNativeTestSupport::PrepareAndExecute(Context, Entry),
			*Case.Describe(TEXT("constructor-policy entry should finish"))));
		ASSERT_THAT(AreEqual(
			Scenario.ExpectedReturnValue,
			static_cast<int32>(Context->GetReturnDWord()),
			*Case.Describe(TEXT("constructor-policy entry should return the scenario value"))));
		ASSERT_THAT(AreEqual(
			asSUCCESS,
			Context->Unprepare(),
			*Case.Describe(TEXT("constructor-policy context should release local storage"))));

		const TArray<int32> Expected = ExpectedMarkers(Scenario);
		ASSERT_THAT(AreEqual(Expected.Num(), State.Markers.Num(),
			*Case.Describe(TEXT("constructor-policy marker count should match the complete behavior trace"))));
		for (int32 Index = 0;
			Index < FMath::Min(Expected.Num(), State.Markers.Num());
			++Index)
		{
			ASSERT_THAT(AreEqual(Expected[Index], State.Markers[Index],
				*Case.Describe(TEXT("constructor-policy marker order should be stable"))));
		}

		const int32 MarkerCountBeforeRecovery = State.Markers.Num();
		ASSERT_THAT(IsTrue(Context->Prepare(Recovery) >= 0,
			*Case.Describe(TEXT("constructor-policy context should prepare recovery"))));
		ASSERT_THAT(AreEqual(
			static_cast<int32>(asEXECUTION_FINISHED),
			Context->Execute(),
			*Case.Describe(TEXT("constructor-policy recovery should finish in the same context"))));
		ASSERT_THAT(AreEqual(
			97,
			static_cast<int32>(Context->GetReturnDWord()),
			*Case.Describe(TEXT("constructor-policy recovery should return its clean sentinel"))));
		ASSERT_THAT(AreEqual(MarkerCountBeforeRecovery, State.Markers.Num(),
			*Case.Describe(TEXT("constructor-policy recovery should not replay construction"))));
		ASSERT_THAT(AreEqual(
			asSUCCESS,
			Context->Unprepare(),
			*Case.Describe(TEXT("constructor-policy recovery should unprepare cleanly"))));
		Context->Release();
	}

	void VerifyLifecycleObservation(
		const FNativeCaseContext& Case,
		const FScenario& Scenario,
		const FNativeLifecycleRecorder& Lifecycle)
	{
		using namespace AngelscriptNativeTestSupport;

		ASSERT_THAT(AreEqual(0, Lifecycle.GetLiveObjectCount(),
			*Case.Describe(TEXT("constructor-policy scenario should leave no tracked object alive"))));
		if (!Scenario.bExpectedBuild || !Scenario.bExpectTrackedLifecycle)
		{
			ASSERT_THAT(AreEqual(0, Lifecycle.GetEntries().Num(),
				*Case.Describe(TEXT("rejected or scalar-only policy scenario should create no tracked lifecycle"))));
			return;
		}

		const int32 ConstructionCount =
			Lifecycle.Num(ENativeLifecycleEvent::DefaultConstruct)
			+ Lifecycle.Num(ENativeLifecycleEvent::ValueConstruct)
			+ Lifecycle.Num(ENativeLifecycleEvent::CopyConstruct);
		ASSERT_THAT(IsTrue(ConstructionCount > 0,
			*Case.Describe(TEXT("constructor-policy scenario should construct tracked storage"))));
		ASSERT_THAT(AreEqual(
			ConstructionCount,
			Lifecycle.Num(ENativeLifecycleEvent::Destruct),
			*Case.Describe(TEXT("every tracked construction should have exactly one destruction"))));
		ASSERT_THAT(AreEqual(
			Scenario.bExpectNativeCopy,
			Lifecycle.Num(ENativeLifecycleEvent::CopyConstruct) > 0,
			*Case.Describe(TEXT("native-field copy construction should match implicit or declared copy policy"))));
		ASSERT_THAT(AreEqual(
			Scenario.bExpectNativeAssignment,
			Lifecycle.Num(ENativeLifecycleEvent::Assign) > 0,
			*Case.Describe(TEXT("native-field assignment should match generated, declared, or self-assignment policy"))));

		TSet<int32> ConstructedIds;
		TSet<int32> DestructedIds;
		for (const FNativeLifecycleEntry& Entry : Lifecycle.GetEntries())
		{
			if (Entry.Event == ENativeLifecycleEvent::DefaultConstruct
				|| Entry.Event == ENativeLifecycleEvent::ValueConstruct
				|| Entry.Event == ENativeLifecycleEvent::CopyConstruct)
			{
				ASSERT_THAT(IsFalse(ConstructedIds.Contains(Entry.ObjectId),
					*Case.Describe(TEXT("tracked constructor should allocate a unique identity"))));
				ConstructedIds.Add(Entry.ObjectId);
			}
			else if (Entry.Event == ENativeLifecycleEvent::Destruct)
			{
				ASSERT_THAT(IsTrue(ConstructedIds.Contains(Entry.ObjectId),
					*Case.Describe(TEXT("tracked destructor should reference constructed storage"))));
				ASSERT_THAT(IsFalse(DestructedIds.Contains(Entry.ObjectId),
					*Case.Describe(TEXT("tracked storage should be destroyed no more than once"))));
				DestructedIds.Add(Entry.ObjectId);
			}
		}
		ASSERT_THAT(AreEqual(ConstructedIds.Num(), DestructedIds.Num(),
			*Case.Describe(TEXT("tracked constructor and destructor identities should balance"))));
	}

	void RunRejectedRecovery(
		const FNativeCaseContext& Case,
		FNativeTestEngine& Engine,
		const FString& ModuleName,
		FConstructorPolicyState& State,
		FNativeLifecycleRecorder& Lifecycle)
	{
		using namespace AngelscriptNativeTestSupport;

		asIScriptEngine* const ScriptEngine = Engine.Get();
		if (ScriptEngine == nullptr)
		{
			return;
		}
		const FTCHARToUTF8 ModuleNameUtf8(*ModuleName);
		ScriptEngine->DiscardModule(ModuleNameUtf8.Get());
		ASSERT_THAT(IsNull(ScriptEngine->GetModule(
			ModuleNameUtf8.Get(),
			asGM_ONLY_IF_EXISTS),
			*Case.Describe(TEXT("rejected constructor-policy module should discard before recovery"))));

		Engine.ResetMessages();
		const FString RecoverySource = BuildConstructorPolicyRecoverySource();
		asIScriptModule* RecoveryModule = nullptr;
		ASSERT_THAT(IsTrue(CompileAndReport(
			*TestRunner,
			*ScriptEngine,
			Case.GetId() + TEXT("-RECOVERY"),
			ModuleName,
			RecoverySource,
			RecoveryModule) >= 0,
			*Case.Describe(TEXT("constructor-policy rejection should allow a clean same-name recovery"))));
		ASSERT_THAT(IsNotNull(RecoveryModule,
			*Case.Describe(TEXT("constructor-policy recovery should publish a clean module"))));
		if (RecoveryModule != nullptr)
		{
			asIScriptFunction* const Recovery =
				RecoveryModule->GetFunctionByDecl("int RunConstructorPolicyRecovery()");
			ASSERT_THAT(IsNotNull(Recovery,
				*Case.Describe(TEXT("constructor-policy recovery should publish its exact entry"))));
			if (Recovery != nullptr)
			{
				asIScriptContext* const Context = ScriptEngine->CreateContext();
				ASSERT_THAT(IsNotNull(Context,
					*Case.Describe(TEXT("constructor-policy recovery should create a context"))));
				if (Context != nullptr)
				{
					ASSERT_THAT(AreEqual(
						static_cast<int32>(asEXECUTION_FINISHED),
						AngelscriptNativeTestSupport::PrepareAndExecute(
							Context,
							Recovery),
						*Case.Describe(TEXT("constructor-policy recovery should finish"))));
					ASSERT_THAT(AreEqual(
						97,
						static_cast<int32>(Context->GetReturnDWord()),
						*Case.Describe(TEXT("constructor-policy recovery should return its sentinel"))));
					Context->Release();
				}
			}
		}
		ASSERT_THAT(AreEqual(0, State.Markers.Num(),
			*Case.Describe(TEXT("constructor-policy rejection recovery should execute no policy marker"))));
		ASSERT_THAT(AreEqual(0, Lifecycle.GetLiveObjectCount(),
			*Case.Describe(TEXT("constructor-policy rejection recovery should leave no live tracked object"))));
		ASSERT_THAT(AreEqual(0, Lifecycle.GetEntries().Num(),
			*Case.Describe(TEXT("constructor-policy rejection recovery should create no tracked storage"))));

		ScriptEngine->DiscardModule(ModuleNameUtf8.Get());
		ASSERT_THAT(IsNull(ScriptEngine->GetModule(
			ModuleNameUtf8.Get(),
			asGM_ONLY_IF_EXISTS),
			*Case.Describe(TEXT("constructor-policy recovery module should discard cleanly"))));
	}

public:
	TEST_METHOD(ScenariosByObservation)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT("LANG-CTOR-SPECIAL-POLICY",
			ENativeEvidence::Compile
				| ENativeEvidence::Diagnostic
				| ENativeEvidence::Runtime
				| ENativeEvidence::Metadata
				| ENativeEvidence::Lifecycle
				| ENativeEvidence::Cleanup);

		for (const FScenario& Scenario : Scenarios)
		{
			FNativeTestEngine Engine;
			Engine.Create(*TestRunner);
			ON_SCOPE_EXIT { Engine.Destroy(); };
			asIScriptEngine* const ScriptEngine = Engine.Get();
			const FString ScenarioName = ANSI_TO_TCHAR(Scenario.CatalogName);
			const FNativeCaseContext CompileCase(MakeNativeCaseId(
				"LANG-CTOR-SPECIAL-POLICY",
				{ TEXT("compile"), *ScenarioName }));
			const FNativeCaseContext MetadataCase(MakeNativeCaseId(
				"LANG-CTOR-SPECIAL-POLICY",
				{ TEXT("metadata"), *ScenarioName }));
			const FNativeCaseContext RuntimeCase(MakeNativeCaseId(
				"LANG-CTOR-SPECIAL-POLICY",
				{ TEXT("runtime"), *ScenarioName }));
			const FNativeCaseContext LifecycleCase(MakeNativeCaseId(
				"LANG-CTOR-SPECIAL-POLICY",
				{ TEXT("lifecycle"), *ScenarioName }));

			ASSERT_THAT(IsNotNull(ScriptEngine,
				*CompileCase.Describe(TEXT("constructor-policy scenario should create a raw SDK engine"))));
			if (ScriptEngine == nullptr)
			{
				continue;
			}
			if (Scenario.bDisableGeneratedDefaults)
			{
				ASSERT_THAT(IsTrue(
					ScriptEngine->SetEngineProperty(
						asEP_ALWAYS_IMPL_DEFAULT_CONSTRUCT,
						0) >= 0,
					*CompileCase.Describe(TEXT("option-off scenario should disable generated defaults"))));
			}

			FConstructorPolicyState State;
			FNativeLifecycleRecorder Lifecycle;
			ASSERT_THAT(IsTrue(RegisterConstructorPolicyBridge(*ScriptEngine, State),
				*CompileCase.Describe(TEXT("constructor-policy scenario should register its marker bridge"))));
			ASSERT_THAT(IsTrue(RegisterNativeCaseValue(*ScriptEngine, Lifecycle),
				*CompileCase.Describe(TEXT("constructor-policy scenario should register tracked native values"))));
			ASSERT_THAT(IsTrue(RegisterNativeScriptLifecycleBridge(*ScriptEngine, Lifecycle),
				*CompileCase.Describe(TEXT("constructor-policy scenario should register script lifecycle callbacks"))));

			const FString ModuleName =
				TEXT("ConstructorPolicy_") + ScenarioName;
			const FString Source = BuildConstructorPolicySource(Scenario);
			Engine.ResetMessages();
			State.Reset();
			Lifecycle.Reset();
			asIScriptModule* Module = nullptr;
			const int BuildResult = CompileAndReport(
				*TestRunner,
				*ScriptEngine,
				CompileCase.GetId(),
				ModuleName,
				Source,
				Module);
			if (Scenario.bExpectedBuild && BuildResult < 0)
			{
				UE_LOG(LogTemp, Display,
					TEXT("[AS-CTOR-POLICY-COMPILE-DIAGNOSTICS] Id=%s Module=%s Result=%d\n%s"),
					*CompileCase.GetId(),
					*ModuleName,
					BuildResult,
					*Engine.GetMessagesText());
			}

			VerifyCompileObservation(
				CompileCase,
				Scenario,
				BuildResult,
				Module,
				Engine);
			VerifyMetadataObservation(
				MetadataCase,
				Scenario,
				Module);
			VerifyRuntimeObservation(
				RuntimeCase,
				Scenario,
				*ScriptEngine,
				Module,
				State);
			VerifyLifecycleObservation(
				LifecycleCase,
				Scenario,
				Lifecycle);

			if (!Scenario.bExpectedBuild)
			{
				RunRejectedRecovery(
					RuntimeCase,
					Engine,
					ModuleName,
					State,
					Lifecycle);
				continue;
			}

			const FTCHARToUTF8 ModuleNameUtf8(*ModuleName);
			ScriptEngine->DiscardModule(ModuleNameUtf8.Get());
			ASSERT_THAT(IsNull(ScriptEngine->GetModule(
				ModuleNameUtf8.Get(),
				asGM_ONLY_IF_EXISTS),
				*LifecycleCase.Describe(TEXT("constructor-policy module should discard cleanly"))));
			ASSERT_THAT(AreEqual(0, Lifecycle.GetLiveObjectCount(),
				*LifecycleCase.Describe(TEXT("module discard should leave no tracked constructor-policy object"))));
		}
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
