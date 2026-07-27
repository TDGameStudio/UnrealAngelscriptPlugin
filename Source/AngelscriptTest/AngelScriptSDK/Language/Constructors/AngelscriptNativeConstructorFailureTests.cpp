#include "../../Support/AngelscriptNativeFixtureTestSupport.h"
#include "../../Support/AngelscriptNativeLanguageCaseTestSupport.h"

#include "CQTest.h"
#include "Misc/ScopeExit.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FConstructorFailureTests,
	"Angelscript.TestModule.AngelScriptSDK.Language.Constructors.Failure",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	using ENativeLifecycleEvent =
		AngelscriptNativeTestSupport::ENativeLifecycleEvent;
	using FNativeCaseContext =
		AngelscriptNativeTestSupport::FNativeCaseContext;
	using FNativeLifecycleEntry =
		AngelscriptNativeTestSupport::FNativeLifecycleEntry;
	using FNativeLifecycleFaultController =
		AngelscriptNativeTestSupport::FNativeLifecycleFaultController;
	using FNativeLifecycleRecorder =
		AngelscriptNativeTestSupport::FNativeLifecycleRecorder;
	using FNativeTestEngine =
		AngelscriptNativeTestSupport::FNativeTestEngine;

	static constexpr asPWORD ConstructorFailureStateUserDataSlot =
		static_cast<asPWORD>(0x43544F524641494Cull);

	struct FFailureCase
	{
		const ANSICHAR* CatalogName;
		int32 Stage;
	};

	struct FDepthCase
	{
		const ANSICHAR* CatalogName;
	};

	struct FObservationCase
	{
		const ANSICHAR* CatalogName;
	};

	inline static constexpr FFailureCase FailureCases[] =
	{
		{ "none", 0 },
		{ "base", 1 },
		{ "member_first", 2 },
		{ "member_middle", 3 },
		{ "member_last", 4 },
		{ "derived_body", 5 },
		{ "copy", 6 },
		{ "conversion", 7 },
	};

	inline static constexpr FDepthCase DepthCases[] =
	{
		{ "flat_members" },
		{ "nested_members" },
		{ "deep_nested_members" },
		{ "base_and_derived_members" },
	};

	inline static constexpr FObservationCase ObservationCases[] =
	{
		{ "values" },
		{ "event_order" },
		{ "cleanup" },
		{ "context_reuse" },
	};

	struct FConstructorFailureState
	{
		int32 FailureStage = 0;
		int32 TriggeredStage = 0;
		int32 ConversionCalls = 0;
		TArray<int32> BegunStages;
		TArray<int32> CompletedStages;
		TArray<int32> CompletedValues;
		TArray<int32> DestroyedStages;

		void Reset(const int32 InFailureStage)
		{
			FailureStage = InFailureStage;
			TriggeredStage = 0;
			ConversionCalls = 0;
			BegunStages.Reset();
			CompletedStages.Reset();
			CompletedValues.Reset();
			DestroyedStages.Reset();
		}
	};

	static FConstructorFailureState* GetActiveState()
	{
		asIScriptContext* const Context = asGetActiveContext();
		return Context != nullptr
			? static_cast<FConstructorFailureState*>(
				Context->GetEngine()->GetUserData(
					ConstructorFailureStateUserDataSlot))
			: nullptr;
	}

	static void RecordConstructorFailureBegin(const int32 Stage)
	{
		if (FConstructorFailureState* const State = GetActiveState())
		{
			State->BegunStages.Add(Stage);
		}
	}

	static void MaybeFailConstructorStage(const int32 Stage)
	{
		FConstructorFailureState* const State = GetActiveState();
		if (State == nullptr || State->FailureStage != Stage)
		{
			return;
		}
		State->TriggeredStage = Stage;
		if (asIScriptContext* const Context = asGetActiveContext())
		{
			const FString Message =
				FString::Printf(
					TEXT("Constructor failure stage %d"),
					Stage);
			const FTCHARToUTF8 MessageUtf8(*Message);
			Context->SetException(MessageUtf8.Get());
		}
	}

	static void RecordConstructorFailureComplete(
		const int32 Stage,
		const int32 Value)
	{
		if (FConstructorFailureState* const State = GetActiveState())
		{
			State->CompletedStages.Add(Stage);
			State->CompletedValues.Add(Value);
		}
	}

	static void RecordConstructorFailureDestroy(const int32 Stage)
	{
		if (FConstructorFailureState* const State = GetActiveState())
		{
			State->DestroyedStages.Add(Stage);
		}
	}

	static int32 ConvertConstructorFailureInput(const int32 Value)
	{
		if (FConstructorFailureState* const State = GetActiveState())
		{
			++State->ConversionCalls;
			if (State->FailureStage == 7)
			{
				State->TriggeredStage = 7;
				if (asIScriptContext* const Context = asGetActiveContext())
				{
					Context->SetException(
						"Constructor conversion input fault");
				}
			}
		}
		return Value;
	}

	static bool RegisterConstructorFailureBridge(
		asIScriptEngine& ScriptEngine,
		FConstructorFailureState& State)
	{
		ScriptEngine.SetUserData(
			&State,
			ConstructorFailureStateUserDataSlot);
		const ASAutoCaller::FunctionCaller BeginCaller =
			ASAutoCaller::MakeFunctionCaller(
				RecordConstructorFailureBegin);
		const ASAutoCaller::FunctionCaller FailCaller =
			ASAutoCaller::MakeFunctionCaller(
				MaybeFailConstructorStage);
		const ASAutoCaller::FunctionCaller CompleteCaller =
			ASAutoCaller::MakeFunctionCaller(
				RecordConstructorFailureComplete);
		const ASAutoCaller::FunctionCaller DestroyCaller =
			ASAutoCaller::MakeFunctionCaller(
				RecordConstructorFailureDestroy);
		const ASAutoCaller::FunctionCaller ConversionCaller =
			ASAutoCaller::MakeFunctionCaller(
				ConvertConstructorFailureInput);
		return ScriptEngine.RegisterGlobalFunction(
			"void RecordConstructorFailureBegin(int Stage)",
			asFUNCTION(RecordConstructorFailureBegin),
			asCALL_CDECL,
			*(asFunctionCaller*)&BeginCaller) >= 0
			&& ScriptEngine.RegisterGlobalFunction(
				"void MaybeFailConstructorStage(int Stage)",
				asFUNCTION(MaybeFailConstructorStage),
				asCALL_CDECL,
				*(asFunctionCaller*)&FailCaller) >= 0
			&& ScriptEngine.RegisterGlobalFunction(
				"void RecordConstructorFailureComplete(int Stage, int Value)",
				asFUNCTION(RecordConstructorFailureComplete),
				asCALL_CDECL,
				*(asFunctionCaller*)&CompleteCaller) >= 0
			&& ScriptEngine.RegisterGlobalFunction(
				"void RecordConstructorFailureDestroy(int Stage)",
				asFUNCTION(RecordConstructorFailureDestroy),
				asCALL_CDECL,
				*(asFunctionCaller*)&DestroyCaller) >= 0
			&& ScriptEngine.RegisterGlobalFunction(
				"int ConvertConstructorFailureInput(int Value)",
				asFUNCTION(ConvertConstructorFailureInput),
				asCALL_CDECL,
				*(asFunctionCaller*)&ConversionCaller) >= 0;
	}

	static bool IsFailure(
		const FFailureCase& FailureCase,
		const ANSICHAR* CatalogName)
	{
		return FCStringAnsi::Strcmp(
			FailureCase.CatalogName,
			CatalogName) == 0;
	}

	static bool IsDepth(
		const FDepthCase& DepthCase,
		const ANSICHAR* CatalogName)
	{
		return FCStringAnsi::Strcmp(
			DepthCase.CatalogName,
			CatalogName) == 0;
	}

	static void AppendStageValueType(
		FString& Source,
		const TCHAR* TypeName,
		const int32 Stage,
		const int32 Value)
	{
		using namespace AngelscriptNativeTestSupport;

		AppendGeneratedAsLine(Source, FString::Printf(
			TEXT("struct %s"),
			TypeName));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, FString::Printf(
			TEXT("\tint Value = %d;"),
			Value));
		AppendGeneratedAsLine(Source, TEXT("\tFNativeCaseValue Payload;"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, FString::Printf(
			TEXT("\t%s()"),
			TypeName));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(Source, FString::Printf(
			TEXT("\t\tRecordConstructorFailureBegin(%d);"),
			Stage));
		AppendGeneratedAsLine(Source, FString::Printf(
			TEXT("\t\tMaybeFailConstructorStage(%d);"),
			Stage));
		AppendGeneratedAsLine(Source, FString::Printf(
			TEXT("\t\tRecordConstructorFailureComplete(%d, Value);"),
			Stage));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, FString::Printf(
			TEXT("\t~%s()"),
			TypeName));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(Source, FString::Printf(
			TEXT("\t\tRecordConstructorFailureDestroy(%d);"),
			Stage));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
	}

	static void AppendStageValueTypes(FString& Source)
	{
		AppendStageValueType(
			Source,
			TEXT("FFirstFailureValue"),
			2,
			20);
		AppendStageValueType(
			Source,
			TEXT("FMiddleFailureValue"),
			3,
			30);
		AppendStageValueType(
			Source,
			TEXT("FLastFailureValue"),
			4,
			40);
	}

	static void AppendNestedWrapper(
		FString& Source,
		const TCHAR* WrapperName,
		const TCHAR* InnerType,
		const TCHAR* PropertyName,
		const bool bAddTrackedContext)
	{
		using namespace AngelscriptNativeTestSupport;

		AppendGeneratedAsLine(Source, FString::Printf(
			TEXT("struct %s"),
			WrapperName));
		AppendGeneratedAsLine(Source, TEXT("{"));
		if (bAddTrackedContext)
		{
			AppendGeneratedAsLine(Source, TEXT("\tFNativeCaseValue Context;"));
		}
		AppendGeneratedAsLine(Source, FString::Printf(
			TEXT("\t%s %s;"),
			InnerType,
			PropertyName));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
	}

	static void AppendDepthWrappers(
		FString& Source,
		const FDepthCase& DepthCase)
	{
		if (IsDepth(DepthCase, "flat_members"))
		{
			return;
		}
		const bool bTrackedContext =
			IsDepth(DepthCase, "base_and_derived_members");
		AppendNestedWrapper(
			Source,
			TEXT("FFirstFailureWrapper"),
			TEXT("FFirstFailureValue"),
			TEXT("Value"),
			bTrackedContext);
		AppendNestedWrapper(
			Source,
			TEXT("FMiddleFailureWrapper"),
			TEXT("FMiddleFailureValue"),
			TEXT("Value"),
			bTrackedContext);
		AppendNestedWrapper(
			Source,
			TEXT("FLastFailureWrapper"),
			TEXT("FLastFailureValue"),
			TEXT("Value"),
			bTrackedContext);
		if (IsDepth(DepthCase, "deep_nested_members"))
		{
			AppendNestedWrapper(
				Source,
				TEXT("FDeepFirstFailureWrapper"),
				TEXT("FFirstFailureWrapper"),
				TEXT("Inner"),
				true);
			AppendNestedWrapper(
				Source,
				TEXT("FDeepMiddleFailureWrapper"),
				TEXT("FMiddleFailureWrapper"),
				TEXT("Inner"),
				true);
			AppendNestedWrapper(
				Source,
				TEXT("FDeepLastFailureWrapper"),
				TEXT("FLastFailureWrapper"),
				TEXT("Inner"),
				true);
		}
	}

	static FString MemberType(
		const FDepthCase& DepthCase,
		const TCHAR* StageName)
	{
		if (IsDepth(DepthCase, "flat_members"))
		{
			return FString::Printf(
				TEXT("F%sFailureValue"),
				StageName);
		}
		if (IsDepth(DepthCase, "deep_nested_members"))
		{
			return FString::Printf(
				TEXT("FDeep%sFailureWrapper"),
				StageName);
		}
		return FString::Printf(
			TEXT("F%sFailureWrapper"),
			StageName);
	}

	static FString MemberValueExpression(
		const FDepthCase& DepthCase,
		const TCHAR* PropertyName)
	{
		if (IsDepth(DepthCase, "flat_members"))
		{
			return FString::Printf(
				TEXT("%s.Value"),
				PropertyName);
		}
		if (IsDepth(DepthCase, "deep_nested_members"))
		{
			return FString::Printf(
				TEXT("%s.Inner.Value.Value"),
				PropertyName);
		}
		return FString::Printf(
			TEXT("%s.Value.Value"),
			PropertyName);
	}

	static void AppendFailureGraph(
		FString& Source,
		const FDepthCase& DepthCase)
	{
		using namespace AngelscriptNativeTestSupport;

		AppendGeneratedAsLine(Source, TEXT("class FConstructorFailureBase"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tint BaseValue = 10;"));
		AppendGeneratedAsLine(Source, TEXT("\tFNativeCaseValue BasePayload;"));
		if (IsDepth(DepthCase, "base_and_derived_members"))
		{
			AppendGeneratedAsLine(Source, TEXT("\tFNativeCaseValue BaseContext;"));
		}
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("\tFConstructorFailureBase()"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(Source, TEXT("\t\tRecordConstructorFailureBegin(1);"));
		AppendGeneratedAsLine(Source, TEXT("\t\tMaybeFailConstructorStage(1);"));
		AppendGeneratedAsLine(Source, TEXT("\t\tRecordConstructorFailureComplete(1, BaseValue);"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("\t~FConstructorFailureBase()"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(Source, TEXT("\t\tRecordConstructorFailureDestroy(1);"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);

		AppendGeneratedAsLine(Source, TEXT("class FConstructorFailureGraph : FConstructorFailureBase"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		if (IsDepth(DepthCase, "base_and_derived_members"))
		{
			AppendGeneratedAsLine(Source, TEXT("\tFNativeCaseValue DerivedPrefix;"));
		}
		AppendGeneratedAsLine(Source, FString::Printf(
			TEXT("\t%s First;"),
			*MemberType(DepthCase, TEXT("First"))));
		AppendGeneratedAsLine(Source, FString::Printf(
			TEXT("\t%s Middle;"),
			*MemberType(DepthCase, TEXT("Middle"))));
		AppendGeneratedAsLine(Source, FString::Printf(
			TEXT("\t%s Last;"),
			*MemberType(DepthCase, TEXT("Last"))));
		if (IsDepth(DepthCase, "base_and_derived_members"))
		{
			AppendGeneratedAsLine(Source, TEXT("\tFNativeCaseValue DerivedSuffix;"));
		}
		AppendGeneratedAsLine(Source, TEXT("\tint DerivedValue = 50;"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("\tFConstructorFailureGraph()"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(Source, TEXT("\t\tsuper();"));
		AppendGeneratedAsLine(Source, TEXT("\t\tRecordConstructorFailureBegin(5);"));
		AppendGeneratedAsLine(Source, TEXT("\t\tMaybeFailConstructorStage(5);"));
		AppendGeneratedAsLine(Source, TEXT("\t\tRecordConstructorFailureComplete(5, DerivedValue);"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("\t~FConstructorFailureGraph()"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(Source, TEXT("\t\tRecordConstructorFailureDestroy(5);"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("\tint Total()"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(Source, FString::Printf(
			TEXT("\t\treturn BaseValue + %s + %s + %s + DerivedValue;"),
			*MemberValueExpression(DepthCase, TEXT("First")),
			*MemberValueExpression(DepthCase, TEXT("Middle")),
			*MemberValueExpression(DepthCase, TEXT("Last"))));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
	}

	static void AppendTransferProbes(FString& Source)
	{
		using namespace AngelscriptNativeTestSupport;

		AppendGeneratedAsLine(Source, TEXT("struct FConstructorConversionProbe"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tint Value = 0;"));
		AppendGeneratedAsLine(Source, TEXT("\tFNativeCaseValue Payload;"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("\tFConstructorConversionProbe(int InValue)"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(Source, TEXT("\t\tValue = InValue;"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
	}

	static void AppendFailureEntry(
		FString& Source,
		const FFailureCase& FailureCase)
	{
		using namespace AngelscriptNativeTestSupport;

		AppendGeneratedAsLine(Source, TEXT("int RunConstructorFailure()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tFConstructorFailureGraph Graph = FConstructorFailureGraph();"));
		if (IsFailure(FailureCase, "copy"))
		{
			AppendGeneratedAsLine(Source, TEXT("\tRecordConstructorFailureBegin(6);"));
			AppendGeneratedAsLine(Source, TEXT("\tFNativeCaseValue Source;"));
			AppendGeneratedAsLine(Source, TEXT("\tArmNextNativeCaseValueCopyFault();"));
			AppendGeneratedAsLine(Source, TEXT("\tFNativeCaseValue Target(Source);"));
			AppendGeneratedAsLine(Source, TEXT("\tRecordConstructorFailureComplete(6, 60);"));
		}
		else if (IsFailure(FailureCase, "conversion"))
		{
			AppendGeneratedAsLine(Source, TEXT("\tRecordConstructorFailureBegin(7);"));
			AppendGeneratedAsLine(
				Source,
				TEXT("\tFConstructorConversionProbe Probe(ConvertConstructorFailureInput(7));"));
			AppendGeneratedAsLine(Source, TEXT("\tRecordConstructorFailureComplete(7, Probe.Value);"));
		}
		AppendGeneratedAsLine(Source, TEXT("\treturn Graph.Total();"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("int RunConstructorFailureRecovery()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\treturn 97;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
	}

	static FString BuildConstructorFailureSource(
		const FFailureCase& FailureCase,
		const FDepthCase& DepthCase)
	{
		FString Source;
		AppendStageValueTypes(Source);
		AppendDepthWrappers(Source, DepthCase);
		AppendFailureGraph(Source, DepthCase);
		AppendTransferProbes(Source);
		AppendFailureEntry(Source, FailureCase);
		return Source;
	}

	static FString DescribeStages(const TArray<int32>& Stages)
	{
		return FString::JoinBy(
			Stages,
			TEXT(","),
			[](const int32 Stage)
			{
				return FString::FromInt(Stage);
			});
	}

	static asIScriptFunction* FindBehaviour(
		asITypeInfo& Type,
		const asEBehaviours ExpectedBehaviour,
		const int32 ExpectedParameterCount)
	{
		for (asUINT Index = 0; Index < Type.GetBehaviourCount(); ++Index)
		{
			asEBehaviours Behaviour = asBEHAVE_CONSTRUCT;
			asIScriptFunction* const Function =
				Type.GetBehaviourByIndex(Index, &Behaviour);
			if (Function != nullptr
				&& Behaviour == ExpectedBehaviour
				&& static_cast<int32>(Function->GetParamCount())
					== ExpectedParameterCount)
			{
				return Function;
			}
		}
		return nullptr;
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
				|| DwordIndex + static_cast<asUINT>(InstructionSize) > BytecodeLength)
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

	void ReportControlledFailureBytecode(
		const FNativeCaseContext& Case,
		const FFailureCase& FailureCase,
		const FDepthCase& DepthCase,
		asIScriptModule& Module,
		asIScriptFunction& Entry)
	{
		if (!IsDepth(DepthCase, "flat_members")
			|| !IsFailure(FailureCase, "member_first"))
		{
			return;
		}

		const auto ReportTypeBehaviours = [this, &Case](
			asIScriptModule& InModule,
			const ANSICHAR* TypeName)
		{
			asITypeInfo* const Type = InModule.GetTypeInfoByName(TypeName);
			if (Type == nullptr)
			{
				TestRunner->AddInfo(FString::Printf(
					TEXT("[%s] constructor-failure bytecode type=%hs missing"),
					*Case.GetId(),
					TypeName));
				return;
			}

			asIScriptFunction* const Constructor = FindBehaviour(
				*Type,
				asBEHAVE_CONSTRUCT,
				0);
			asIScriptFunction* const Destructor = FindBehaviour(
				*Type,
				asBEHAVE_DESTRUCT,
				0);
			TestRunner->AddInfo(FString::Printf(
				TEXT("[%s] constructor-failure bytecode type=%hs constructor={%s} destructor={%s}"),
				*Case.GetId(),
				TypeName,
				Constructor != nullptr
					? *DescribeFunctionBytecode(*Constructor)
					: TEXT("<missing>"),
				Destructor != nullptr
					? *DescribeFunctionBytecode(*Destructor)
					: TEXT("<missing>")));
		};

		TestRunner->AddInfo(FString::Printf(
			TEXT("[%s] constructor-failure bytecode entry={%s}"),
			*Case.GetId(),
			*DescribeFunctionBytecode(Entry)));
		ReportTypeBehaviours(Module, "FConstructorFailureGraph");
		ReportTypeBehaviours(Module, "FFirstFailureValue");
	}

	void ReportWorkflowState(
		const FNativeCaseContext& Case,
		const int32 ExecuteResult,
		const int32 ReturnValue,
		const FConstructorFailureState& State,
		const FNativeLifecycleRecorder& Lifecycle,
		const FNativeLifecycleFaultController& FaultController)
	{
		TestRunner->AddInfo(FString::Printf(
			TEXT("[CTOR-FAILURE-STATE] Id=%s Execute=%d Return=%d Triggered=%d CopyFaults=%d ConversionCalls=%d Begun=[%s] Completed=[%s] Values=[%s] Destroyed=[%s] NativeLive=%d NativeEntries=%d"),
			*Case.GetId(),
			ExecuteResult,
			ReturnValue,
			State.TriggeredStage,
			FaultController.GetTriggeredCopyCount(),
			State.ConversionCalls,
			*DescribeStages(State.BegunStages),
			*DescribeStages(State.CompletedStages),
			*DescribeStages(State.CompletedValues),
			*DescribeStages(State.DestroyedStages),
			Lifecycle.GetLiveObjectCount(),
			Lifecycle.GetEntries().Num()));
	}

	static TArray<int32> ExpectedBegunStages(
		const FFailureCase& FailureCase)
	{
		switch (FailureCase.Stage)
		{
		case 1:
			return { 2, 3, 4, 1 };
		case 2:
			return { 2 };
		case 3:
			return { 2, 3 };
		case 4:
			return { 2, 3, 4 };
		case 6:
			return { 2, 3, 4, 1, 5, 6 };
		case 7:
			return { 2, 3, 4, 1, 5, 7 };
		default:
			return { 2, 3, 4, 1, 5 };
		}
	}

	static TArray<int32> ExpectedCompletedStages(
		const FFailureCase& FailureCase)
	{
		switch (FailureCase.Stage)
		{
		case 1:
			return { 2, 3, 4 };
		case 2:
			return {};
		case 3:
			return { 2 };
		case 4:
			return { 2, 3 };
		case 5:
			return { 2, 3, 4, 1 };
		default:
			return { 2, 3, 4, 1, 5 };
		}
	}

	static TArray<int32> ExpectedDestroyedStages(
		const FFailureCase& FailureCase)
	{
		// Destruction follows the language lifetime rule: derived body first,
		// then derived members in reverse declaration order, then the base body.
		return { 5, 4, 3, 2, 1 };
	}

	void VerifyValues(
		const FNativeCaseContext& Case,
		const FFailureCase& FailureCase,
		const int32 ExecuteResult,
		const int32 ReturnValue,
		const FConstructorFailureState& State,
		const FNativeLifecycleFaultController& FaultController)
	{
		const bool bExpectedSuccess = FailureCase.Stage == 0;
		ASSERT_THAT(AreEqual(
			bExpectedSuccess
				? static_cast<int32>(asEXECUTION_FINISHED)
				: static_cast<int32>(asEXECUTION_EXCEPTION),
			ExecuteResult,
			*Case.Describe(TEXT("constructor-failure execution state should match the selected failure point"))));
		if (bExpectedSuccess)
		{
			ASSERT_THAT(AreEqual(150, ReturnValue,
				*Case.Describe(TEXT("complete constructor graph should publish all five stage values"))));
			ASSERT_THAT(AreEqual(0, State.TriggeredStage,
				*Case.Describe(TEXT("normal construction should trigger no failure stage"))));
		}
		else if (FailureCase.Stage == 6)
		{
			ASSERT_THAT(AreEqual(
				1,
				FaultController.GetTriggeredCopyCount(),
				*Case.Describe(TEXT("copy failure should consume exactly one native copy fault"))));
		}
		else
		{
			ASSERT_THAT(AreEqual(
				FailureCase.Stage,
				State.TriggeredStage,
				*Case.Describe(TEXT("constructor failure should identify its exact configured stage"))));
		}

		const TArray<int32> ExpectedCompleted =
			ExpectedCompletedStages(FailureCase);
		ASSERT_THAT(AreEqual(
			ExpectedCompleted.Num(),
			State.CompletedStages.Num(),
			*Case.Describe(TEXT("constructor workflow should publish every completed stage"))));
		ASSERT_THAT(AreEqual(
			ExpectedCompleted.Num(),
			State.CompletedValues.Num(),
			*Case.Describe(TEXT("constructor workflow should publish one value per completed stage"))));
		for (int32 Index = 0;
			Index < ExpectedCompleted.Num()
				&& Index < State.CompletedStages.Num()
				&& Index < State.CompletedValues.Num();
			++Index)
		{
			ASSERT_THAT(AreEqual(
				ExpectedCompleted[Index],
				State.CompletedStages[Index],
				*Case.Describe(TEXT("constructor completed stages should preserve the fork construction order"))));
			ASSERT_THAT(AreEqual(
				ExpectedCompleted[Index] * 10,
				State.CompletedValues[Index],
				*Case.Describe(TEXT("constructor completed stage should retain its value"))));
		}
		ASSERT_THAT(AreEqual(
			FailureCase.Stage == 7 ? 1 : 0,
			State.ConversionCalls,
			*Case.Describe(TEXT("only conversion failure should evaluate the conversion input"))));
	}

	void VerifyEventOrder(
		const FNativeCaseContext& Case,
		const FFailureCase& FailureCase,
		const FConstructorFailureState& State)
	{
		const TArray<int32> ExpectedBegun =
			ExpectedBegunStages(FailureCase);
		ASSERT_THAT(AreEqual(
			ExpectedBegun.Num(),
			State.BegunStages.Num(),
			*Case.Describe(TEXT("constructor begin trace should stop at the exact failure stage"))));
		for (int32 Index = 0;
			Index < ExpectedBegun.Num()
				&& Index < State.BegunStages.Num();
			++Index)
		{
			ASSERT_THAT(AreEqual(
				ExpectedBegun[Index],
				State.BegunStages[Index],
				*Case.Describe(TEXT("constructor begin trace should preserve the current fork construction order"))));
		}

		const TArray<int32> ExpectedDestroyed =
			ExpectedDestroyedStages(FailureCase);
		ASSERT_THAT(AreEqual(
			ExpectedDestroyed.Num(),
			State.DestroyedStages.Num(),
			*Case.Describe(TEXT("constructor workflow should publish the exact script destructor trace"))));
		for (int32 Index = 0;
			Index < ExpectedDestroyed.Num()
				&& Index < State.DestroyedStages.Num();
			++Index)
		{
			ASSERT_THAT(AreEqual(
				ExpectedDestroyed[Index],
				State.DestroyedStages[Index],
				*Case.Describe(TEXT("constructor workflow should preserve its exact fork destructor order"))));
		}
	}

	void VerifyCleanup(
		const FNativeCaseContext& Case,
		const FFailureCase& FailureCase,
		const FNativeLifecycleRecorder& Lifecycle)
	{
		using namespace AngelscriptNativeTestSupport;

		ASSERT_THAT(AreEqual(0, Lifecycle.GetLiveObjectCount(),
			*Case.Describe(TEXT("constructor workflow should leave no native field or retired partial copy alive"))));
		const int32 ConstructionCount =
			Lifecycle.Num(ENativeLifecycleEvent::DefaultConstruct)
			+ Lifecycle.Num(ENativeLifecycleEvent::ValueConstruct)
			+ Lifecycle.Num(ENativeLifecycleEvent::CopyConstruct);
		ASSERT_THAT(IsTrue(ConstructionCount > 0,
			*Case.Describe(TEXT("constructor-failure topology should construct real tracked fields"))));
		const int32 DestructionCount =
			Lifecycle.Num(ENativeLifecycleEvent::Destruct);
		ASSERT_THAT(AreEqual(
			ConstructionCount,
			DestructionCount,
			*Case.Describe(TEXT("constructor workflow should retire every fully or partially created native field"))));

		TSet<int32> ConstructedIds;
		TSet<int32> DestructedIds;
		for (const FNativeLifecycleEntry& Entry : Lifecycle.GetEntries())
		{
			if (Entry.Event == ENativeLifecycleEvent::DefaultConstruct
				|| Entry.Event == ENativeLifecycleEvent::ValueConstruct
				|| Entry.Event == ENativeLifecycleEvent::CopyConstruct)
			{
				ASSERT_THAT(IsFalse(ConstructedIds.Contains(Entry.ObjectId),
					*Case.Describe(TEXT("constructor-failure lifecycle should allocate unique identities"))));
				ConstructedIds.Add(Entry.ObjectId);
			}
			else if (Entry.Event == ENativeLifecycleEvent::Destruct)
			{
				ASSERT_THAT(IsTrue(ConstructedIds.Contains(Entry.ObjectId),
					*Case.Describe(TEXT("constructor-failure destructor should identify constructed storage"))));
				ASSERT_THAT(IsFalse(DestructedIds.Contains(Entry.ObjectId),
					*Case.Describe(TEXT("constructor-failure storage should not be destroyed twice"))));
				DestructedIds.Add(Entry.ObjectId);
			}
		}
		ASSERT_THAT(AreEqual(ConstructedIds.Num(), DestructedIds.Num(),
			*Case.Describe(TEXT("constructor workflow native identities should balance"))));
	}

	void VerifyContextReuse(
		const FNativeCaseContext& Case,
		asIScriptContext& Context,
		asIScriptFunction& Recovery,
		const FConstructorFailureState& State,
		const FNativeLifecycleRecorder& Lifecycle)
	{
		const int32 BegunBefore = State.BegunStages.Num();
		const int32 CompletedBefore = State.CompletedStages.Num();
		const int32 DestroyedBefore = State.DestroyedStages.Num();
		const int32 LifecycleBefore = Lifecycle.GetEntries().Num();
		ASSERT_THAT(IsTrue(Context.Prepare(&Recovery) >= 0,
			*Case.Describe(TEXT("constructor-failure context should prepare recovery after cleanup"))));
		ASSERT_THAT(AreEqual(
			static_cast<int32>(asEXECUTION_FINISHED),
			Context.Execute(),
			*Case.Describe(TEXT("constructor-failure recovery should finish in the same context"))));
		ASSERT_THAT(AreEqual(
			97,
			static_cast<int32>(Context.GetReturnDWord()),
			*Case.Describe(TEXT("constructor-failure recovery should return its sentinel"))));
		ASSERT_THAT(AreEqual(BegunBefore, State.BegunStages.Num(),
			*Case.Describe(TEXT("constructor-failure recovery should begin no construction stage"))));
		ASSERT_THAT(AreEqual(CompletedBefore, State.CompletedStages.Num(),
			*Case.Describe(TEXT("constructor-failure recovery should complete no construction stage"))));
		ASSERT_THAT(AreEqual(DestroyedBefore, State.DestroyedStages.Num(),
			*Case.Describe(TEXT("constructor-failure recovery should destroy no construction stage"))));
		ASSERT_THAT(AreEqual(LifecycleBefore, Lifecycle.GetEntries().Num(),
			*Case.Describe(TEXT("constructor-failure recovery should create no tracked storage"))));
		ASSERT_THAT(AreEqual(
			asSUCCESS,
			Context.Unprepare(),
			*Case.Describe(TEXT("constructor-failure recovery should unprepare cleanly"))));
	}

	void RunWorkflow(
		const FFailureCase& FailureCase,
		const FDepthCase& DepthCase)
	{
		using namespace AngelscriptNativeTestSupport;

		const FString FailureName =
			ANSI_TO_TCHAR(FailureCase.CatalogName);
		const FString DepthName =
			ANSI_TO_TCHAR(DepthCase.CatalogName);
		const FNativeCaseContext ValuesCase(MakeNativeCaseId(
			"LANG-CTOR-ORDER-FAILURE",
			{ *DepthName, *FailureName, TEXT("values") }));
		const FNativeCaseContext EventCase(MakeNativeCaseId(
			"LANG-CTOR-ORDER-FAILURE",
			{ *DepthName, *FailureName, TEXT("event_order") }));
		const FNativeCaseContext CleanupCase(MakeNativeCaseId(
			"LANG-CTOR-ORDER-FAILURE",
			{ *DepthName, *FailureName, TEXT("cleanup") }));
		const FNativeCaseContext ReuseCase(MakeNativeCaseId(
			"LANG-CTOR-ORDER-FAILURE",
			{ *DepthName, *FailureName, TEXT("context_reuse") }));

		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };
		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine,
			*ValuesCase.Describe(TEXT("constructor-failure workflow should create a raw SDK engine"))));
		if (ScriptEngine == nullptr)
		{
			return;
		}

		FConstructorFailureState State;
		FNativeLifecycleRecorder Lifecycle;
		FNativeLifecycleFaultController FaultController;
		ASSERT_THAT(IsTrue(RegisterConstructorFailureBridge(*ScriptEngine, State),
			*ValuesCase.Describe(TEXT("constructor-failure workflow should register its event bridge"))));
		ASSERT_THAT(IsTrue(RegisterNativeCaseValue(
			*ScriptEngine,
			Lifecycle,
			&FaultController),
			*ValuesCase.Describe(TEXT("constructor-failure workflow should register faultable native values"))));

		const FString ModuleName =
			TEXT("ConstructorFailure_") + DepthName + TEXT("_") + FailureName;
		const FString Source =
			BuildConstructorFailureSource(FailureCase, DepthCase);
		PrintGeneratedAsSource(
			*TestRunner,
			ValuesCase.GetId(),
			ModuleName,
			Source);
		const FTCHARToUTF8 ModuleNameUtf8(*ModuleName);
		const FTCHARToUTF8 SourceUtf8(*Source);
		Engine.ResetMessages();
		asIScriptModule* Module = nullptr;
		const int32 CompileResult = CompileNativeModule(
			ScriptEngine,
			ModuleNameUtf8.Get(),
			SourceUtf8.Get(),
			Module);
		if (CompileResult < 0)
		{
			TestRunner->AddInfo(Engine.GetMessagesText());
		}
		ASSERT_THAT(IsTrue(CompileResult >= 0,
			*ValuesCase.Describe(TEXT("constructor-failure workflow should compile"))));
		ASSERT_THAT(IsNotNull(Module,
			*ValuesCase.Describe(TEXT("constructor-failure workflow should publish its module"))));
		if (Module == nullptr)
		{
			return;
		}

		asIScriptFunction* const Entry =
			Module->GetFunctionByDecl("int RunConstructorFailure()");
		asIScriptFunction* const Recovery =
			Module->GetFunctionByDecl("int RunConstructorFailureRecovery()");
		ASSERT_THAT(IsNotNull(Entry,
			*ValuesCase.Describe(TEXT("constructor-failure workflow should publish its exact entry"))));
		ASSERT_THAT(IsNotNull(Recovery,
			*ReuseCase.Describe(TEXT("constructor-failure workflow should publish its recovery entry"))));
		if (Entry == nullptr || Recovery == nullptr)
		{
			return;
		}

		ReportControlledFailureBytecode(
			ValuesCase,
			FailureCase,
			DepthCase,
			*Module,
			*Entry);

		State.Reset(FailureCase.Stage);
		Lifecycle.Reset();
		FaultController.Reset();
		asIScriptContext* const Context = ScriptEngine->CreateContext();
		ASSERT_THAT(IsNotNull(Context,
			*ValuesCase.Describe(TEXT("constructor-failure workflow should create a reusable context"))));
		if (Context == nullptr)
		{
			return;
		}
		const int32 ExecuteResult =
			PrepareAndExecute(Context, Entry);
		const int32 ReturnValue =
			ExecuteResult == asEXECUTION_FINISHED
				? static_cast<int32>(Context->GetReturnDWord())
				: 0;
		if (FailureCase.Stage != 0)
		{
			ASSERT_THAT(IsTrue(Context->GetExceptionLineNumber() > 0,
				*ValuesCase.Describe(TEXT("constructor failure should retain a positive exception line"))));
			ASSERT_THAT(IsNotNull(Context->GetExceptionFunction(),
				*ValuesCase.Describe(TEXT("constructor failure should retain its owning exception function"))));
			ASSERT_THAT(IsTrue(Context->GetExceptionString() != nullptr,
				*ValuesCase.Describe(TEXT("constructor failure should retain its exact exception text"))));
		}
		ASSERT_THAT(AreEqual(
			asSUCCESS,
			Context->Unprepare(),
			*CleanupCase.Describe(TEXT("constructor-failure context should release partial storage"))));
		ReportWorkflowState(
			ValuesCase,
			ExecuteResult,
			ReturnValue,
			State,
			Lifecycle,
			FaultController);

		VerifyValues(
			ValuesCase,
			FailureCase,
			ExecuteResult,
			ReturnValue,
			State,
			FaultController);
		VerifyEventOrder(EventCase, FailureCase, State);
		VerifyCleanup(CleanupCase, FailureCase, Lifecycle);
		VerifyContextReuse(
			ReuseCase,
			*Context,
			*Recovery,
			State,
			Lifecycle);
		Context->Release();

		ScriptEngine->DiscardModule(ModuleNameUtf8.Get());
		ASSERT_THAT(IsNull(ScriptEngine->GetModule(
			ModuleNameUtf8.Get(),
			asGM_ONLY_IF_EXISTS),
			*CleanupCase.Describe(TEXT("constructor-failure module should discard cleanly"))));
		ASSERT_THAT(AreEqual(0, Lifecycle.GetLiveObjectCount(),
			*CleanupCase.Describe(TEXT("constructor workflow module discard should leave no live object"))));
	}

public:
	TEST_METHOD(FailurePointsByDepthAndObservation)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT("LANG-CTOR-ORDER-FAILURE",
			ENativeEvidence::Runtime
				| ENativeEvidence::Lifecycle
				| ENativeEvidence::Diagnostic
				| ENativeEvidence::Cleanup);

		for (const FDepthCase& DepthCase : DepthCases)
		{
			for (const FFailureCase& FailureCase : FailureCases)
			{
				RunWorkflow(FailureCase, DepthCase);
			}
		}
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
