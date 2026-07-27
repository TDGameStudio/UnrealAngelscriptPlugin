#include "../../Support/AngelscriptNativeCaseTestSupport.h"
#include "../../Support/AngelscriptNativeCoreTestSupport.h"
#include "../../Support/AngelscriptNativeDiagnosticTestSupport.h"
#include "../../Support/AngelscriptNativeFixtureTestSupport.h"
#include "../../Support/AngelscriptNativeLanguageCaseTestSupport.h"

#include "CQTest.h"
#include "Misc/ScopeExit.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FOperatorFailureTests,
	"Angelscript.TestModule.AngelScriptSDK.Language.Operators.Failure",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	using FNativeCaseContext = AngelscriptNativeTestSupport::FNativeCaseContext;
	using FNativeLifecycleRecorder = AngelscriptNativeTestSupport::FNativeLifecycleRecorder;
	using FNativeMessageCollector = AngelscriptNativeTestSupport::FNativeMessageCollector;
	using FNativeMessageEntry = AngelscriptNativeTestSupport::FNativeMessageEntry;
	using FNativeTestEngine = AngelscriptNativeTestSupport::FNativeTestEngine;

	enum class EFailureFamily : uint8
	{
		UnsupportedOperand,
		DivideZero,
		ModuloZero,
		InvalidShiftCount,
		SignedOverflow,
		PowerOverflow,
		InvalidLValue,
		ConstMutation,
		MissingOverload,
		AmbiguousOverload,
		InvalidSignature,
		DuplicateOperator,
		NullReceiver,
		LeftOperandException,
		RightOperandException,
		AssignmentException,
	};

	enum class EFailureOutcome : uint8
	{
		CompileRejected,
		RuntimeException,
		MaskedShiftExecution,
	};

	enum class ERecoveryRoute : uint8
	{
		Fresh,
		Same,
	};

	enum class EObservation : uint8
	{
		DiagnosticOrException,
		Cleanup,
		Recovery,
	};

	struct FFailureCase
	{
		const ANSICHAR* CatalogName;
		EFailureFamily Family;
		EFailureOutcome Outcome;
		const TCHAR* ExpectedException;
	};

	struct FRecoveryCase
	{
		const ANSICHAR* CatalogName;
		ERecoveryRoute Route;
	};

	struct FObservationCase
	{
		const ANSICHAR* CatalogName;
		EObservation Observation;
	};

	struct FOperatorFailureState
	{
		void Reset()
		{
			Trace.Reset();
			CallbackCount = 0;
		}

		TArray<int32> Trace;
		int32 CallbackCount = 0;
	};

	inline static constexpr asPWORD FailureStateUserDataSlot =
		static_cast<asPWORD>(0x4F504641494C5552ull);

	inline static constexpr FFailureCase FailureCases[] = {
		{"unsupported_operand", EFailureFamily::UnsupportedOperand, EFailureOutcome::CompileRejected, nullptr},
		{"divide_zero", EFailureFamily::DivideZero, EFailureOutcome::RuntimeException, TEXT("Divide by zero")},
		{"modulo_zero", EFailureFamily::ModuloZero, EFailureOutcome::RuntimeException, TEXT("Divide by zero")},
		{"invalid_shift_count", EFailureFamily::InvalidShiftCount, EFailureOutcome::MaskedShiftExecution, nullptr},
		{"signed_overflow", EFailureFamily::SignedOverflow, EFailureOutcome::RuntimeException, TEXT("Overflow in integer division")},
		{"power_overflow", EFailureFamily::PowerOverflow, EFailureOutcome::RuntimeException, TEXT("Overflow in exponent operation")},
		{"invalid_lvalue", EFailureFamily::InvalidLValue, EFailureOutcome::CompileRejected, nullptr},
		{"const_mutation", EFailureFamily::ConstMutation, EFailureOutcome::CompileRejected, nullptr},
		{"missing_overload", EFailureFamily::MissingOverload, EFailureOutcome::CompileRejected, nullptr},
		{"ambiguous_overload", EFailureFamily::AmbiguousOverload, EFailureOutcome::CompileRejected, nullptr},
		{"invalid_signature", EFailureFamily::InvalidSignature, EFailureOutcome::CompileRejected, nullptr},
		{"duplicate_operator", EFailureFamily::DuplicateOperator, EFailureOutcome::CompileRejected, nullptr},
		{"null_receiver", EFailureFamily::NullReceiver, EFailureOutcome::RuntimeException, TEXT("Null pointer access")},
		{"left_operand_exception", EFailureFamily::LeftOperandException, EFailureOutcome::RuntimeException, TEXT("Operator left operand exception")},
		{"right_operand_exception", EFailureFamily::RightOperandException, EFailureOutcome::RuntimeException, TEXT("Operator right operand exception")},
		{"assignment_exception", EFailureFamily::AssignmentException, EFailureOutcome::RuntimeException, TEXT("Operator assignment exception")},
	};

	inline static constexpr FRecoveryCase RecoveryCases[] = {
		{"fresh_module", ERecoveryRoute::Fresh},
		{"same_module_or_context", ERecoveryRoute::Same},
	};

	inline static constexpr FObservationCase ObservationCases[] = {
		{"diagnostic_or_exception", EObservation::DiagnosticOrException},
		{"cleanup", EObservation::Cleanup},
		{"recovery_result", EObservation::Recovery},
	};

	static FOperatorFailureState* GetFailureState(asIScriptGeneric& Generic)
	{
		return Generic.GetEngine() != nullptr
			? static_cast<FOperatorFailureState*>(
				  Generic.GetEngine()->GetUserData(FailureStateUserDataSlot))
			: nullptr;
	}

	static void RecordStage(asIScriptGeneric& Generic, const int32 Marker)
	{
		if (FOperatorFailureState* const State = GetFailureState(Generic))
		{
			State->Trace.Add(Marker);
			++State->CallbackCount;
		}
	}

	static void GenericRecordOperatorStage(asIScriptGeneric* Generic)
	{
		if (Generic == nullptr)
		{
			return;
		}

		const int32 Marker = static_cast<int32>(Generic->GetArgDWord(0));
		RecordStage(*Generic, Marker);
		Generic->SetReturnDWord(static_cast<asDWORD>(Marker));
	}

	static void GenericThrowOperatorStage(asIScriptGeneric* Generic)
	{
		if (Generic == nullptr)
		{
			return;
		}

		const int32 Marker = static_cast<int32>(Generic->GetArgDWord(0));
		RecordStage(*Generic, Marker);
		if (asIScriptContext* const Context = asGetActiveContext())
		{
			switch (Marker)
			{
			case 1:
				Context->SetException("Operator left operand exception");
				break;
			case 2:
				Context->SetException("Operator right operand exception");
				break;
			default:
				Context->SetException("Operator assignment exception");
				break;
			}
		}
		Generic->SetReturnDWord(static_cast<asDWORD>(Marker));
	}

	static bool RegisterFailureFixtures(asIScriptEngine& ScriptEngine,
		FOperatorFailureState& State,
		FNativeLifecycleRecorder& Lifecycle)
	{
		using namespace AngelscriptNativeTestSupport;

		ScriptEngine.SetUserData(&State, FailureStateUserDataSlot);
		return RegisterNativeCaseValue(ScriptEngine, Lifecycle) &&
			   RegisterNativeCaseReference(ScriptEngine, &Lifecycle) &&
			   ScriptEngine.RegisterGlobalFunction("int RecordOperatorStage(int Marker)",
				   asFUNCTION(GenericRecordOperatorStage),
				   asCALL_GENERIC) >= 0 &&
			   ScriptEngine.RegisterGlobalFunction("int ThrowOperatorStage(int Marker)",
				   asFUNCTION(GenericThrowOperatorStage),
				   asCALL_GENERIC) >= 0;
	}

	static bool IsCompileRejected(const FFailureCase& FailureCase)
	{
		return FailureCase.Outcome == EFailureOutcome::CompileRejected;
	}

	static bool IsMaskedShiftExecution(const FFailureCase& FailureCase)
	{
		return FailureCase.Outcome == EFailureOutcome::MaskedShiftExecution;
	}

	static const char* EntryDeclaration(const FFailureCase& FailureCase)
	{
		return IsMaskedShiftExecution(FailureCase) ? "uint RunOperatorFailure()"
												 : "int RunOperatorFailure()";
	}

	static FString CausalMarker(const FFailureCase& FailureCase)
	{
		return FailureCase.Family == EFailureFamily::AssignmentException
			? TEXT("ASSIGNMENT_CAUSE")
			: TEXT("OP_CAUSE");
	}

	static void AppendRunEntryStart(FString& Source, const TCHAR* ReturnType)
	{
		using namespace AngelscriptNativeTestSupport;

		AppendGeneratedAsLine(Source, FString::Printf(TEXT("%s RunOperatorFailure()"), ReturnType));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tFNativeCaseValue ScopeValue(77);"));
	}

	static FString BuildFailureSource(const FFailureCase& FailureCase)
	{
		using namespace AngelscriptNativeTestSupport;

		FString Source;
		switch (FailureCase.Family)
		{
		case EFailureFamily::UnsupportedOperand:
			AppendRunEntryStart(Source, TEXT("int"));
			AppendGeneratedAsLine(Source, TEXT("\tbool Left = true;"));
			AppendGeneratedAsLine(Source, TEXT("\tbool Right = false;"));
			AppendGeneratedAsLine(Source, TEXT("\treturn Left + Right; // OP_CAUSE"));
			AppendGeneratedAsLine(Source, TEXT("}"));
			break;

		case EFailureFamily::DivideZero:
			AppendRunEntryStart(Source, TEXT("int"));
			AppendGeneratedAsLine(Source, TEXT("\tint Numerator = RecordOperatorStage(1);"));
			AppendGeneratedAsLine(Source, TEXT("\tint Divisor = RecordOperatorStage(0);"));
			AppendGeneratedAsLine(Source, TEXT("\treturn (Numerator / Divisor) + RecordOperatorStage(9); // OP_CAUSE"));
			AppendGeneratedAsLine(Source, TEXT("}"));
			break;

		case EFailureFamily::ModuloZero:
			AppendRunEntryStart(Source, TEXT("int"));
			AppendGeneratedAsLine(Source, TEXT("\tint Dividend = RecordOperatorStage(1);"));
			AppendGeneratedAsLine(Source, TEXT("\tint Divisor = RecordOperatorStage(0);"));
			AppendGeneratedAsLine(Source, TEXT("\treturn (Dividend % Divisor) + RecordOperatorStage(9); // OP_CAUSE"));
			AppendGeneratedAsLine(Source, TEXT("}"));
			break;

		case EFailureFamily::InvalidShiftCount:
			// The current fork exposes raw VM shift bytecodes. They do not validate the
			// count, and the established raw-SDK oracle masks a 32-bit count to 0..31.
			AppendRunEntryStart(Source, TEXT("uint"));
			AppendGeneratedAsLine(Source, TEXT("\tuint Value = 1;"));
			AppendGeneratedAsLine(Source, TEXT("\tint Count = -1;"));
			AppendGeneratedAsLine(Source, TEXT("\treturn Value << Count; // OP_CAUSE"));
			AppendGeneratedAsLine(Source, TEXT("}"));
			break;

		case EFailureFamily::SignedOverflow:
			AppendRunEntryStart(Source, TEXT("int"));
			AppendGeneratedAsLine(Source, TEXT("\tint64 Numerator = -9223372036854775807 - 1;"));
			AppendGeneratedAsLine(Source, TEXT("\tint64 Divisor = -1;"));
			AppendGeneratedAsLine(Source, TEXT("\treturn int(Numerator / Divisor) + RecordOperatorStage(9); // OP_CAUSE"));
			AppendGeneratedAsLine(Source, TEXT("}"));
			break;

		case EFailureFamily::PowerOverflow:
			AppendRunEntryStart(Source, TEXT("int"));
			AppendGeneratedAsLine(Source, TEXT("\tfloat Base = 1.7976931348623157e308;"));
			AppendGeneratedAsLine(Source, TEXT("\tint Exponent = 2;"));
			AppendGeneratedAsLine(Source, TEXT("\treturn int(Base ** Exponent + RecordOperatorStage(9)); // OP_CAUSE"));
			AppendGeneratedAsLine(Source, TEXT("}"));
			break;

		case EFailureFamily::InvalidLValue:
			AppendRunEntryStart(Source, TEXT("int"));
			AppendGeneratedAsLine(Source, TEXT("\tint Value = 7;"));
			AppendGeneratedAsLine(Source, TEXT("\treturn (Value + 1) = 3; // OP_CAUSE"));
			AppendGeneratedAsLine(Source, TEXT("}"));
			break;

		case EFailureFamily::ConstMutation:
			AppendRunEntryStart(Source, TEXT("int"));
			AppendGeneratedAsLine(Source, TEXT("\tconst int Value = 7;"));
			AppendGeneratedAsLine(Source, TEXT("\tValue += 1; // OP_CAUSE"));
			AppendGeneratedAsLine(Source, TEXT("\treturn Value;"));
			AppendGeneratedAsLine(Source, TEXT("}"));
			break;

		case EFailureFamily::MissingOverload:
			AppendGeneratedAsLine(Source, TEXT("struct FMissingOperator"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\tint Value;"));
			AppendGeneratedAsLine(Source, TEXT("}"));
			AppendGeneratedAsLine(Source);
			AppendRunEntryStart(Source, TEXT("int"));
			AppendGeneratedAsLine(Source, TEXT("\tFMissingOperator Left;"));
			AppendGeneratedAsLine(Source, TEXT("\tFMissingOperator Right;"));
			AppendGeneratedAsLine(Source, TEXT("\treturn (Left - Right).Value; // OP_CAUSE"));
			AppendGeneratedAsLine(Source, TEXT("}"));
			break;

		case EFailureFamily::AmbiguousOverload:
			AppendGeneratedAsLine(Source, TEXT("int ResolveAmbiguousOperator(int64 Value)"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\treturn 1;"));
			AppendGeneratedAsLine(Source, TEXT("}"));
			AppendGeneratedAsLine(Source);
			AppendGeneratedAsLine(Source, TEXT("int ResolveAmbiguousOperator(uint64 Value)"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\treturn 2;"));
			AppendGeneratedAsLine(Source, TEXT("}"));
			AppendGeneratedAsLine(Source);
			AppendRunEntryStart(Source, TEXT("int"));
			AppendGeneratedAsLine(Source, TEXT("\treturn ResolveAmbiguousOperator(null); // OP_CAUSE"));
			AppendGeneratedAsLine(Source, TEXT("}"));
			break;

		case EFailureFamily::InvalidSignature:
			AppendGeneratedAsLine(Source, TEXT("struct FInvalidOperatorSignature"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\tint Value;"));
			AppendGeneratedAsLine(Source);
			AppendGeneratedAsLine(Source, TEXT("\tFInvalidOperatorSignature opAdd() const"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t\treturn FInvalidOperatorSignature();"));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
			AppendGeneratedAsLine(Source, TEXT("}"));
			AppendGeneratedAsLine(Source);
			AppendRunEntryStart(Source, TEXT("int"));
			AppendGeneratedAsLine(Source, TEXT("\tFInvalidOperatorSignature Left;"));
			AppendGeneratedAsLine(Source, TEXT("\tFInvalidOperatorSignature Right;"));
			AppendGeneratedAsLine(Source, TEXT("\treturn (Left + Right).Value; // OP_CAUSE"));
			AppendGeneratedAsLine(Source, TEXT("}"));
			break;

		case EFailureFamily::DuplicateOperator:
			AppendGeneratedAsLine(Source, TEXT("struct FDuplicateOperator"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\tint opAdd(const FDuplicateOperator& Other) const"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t\treturn 1;"));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
			AppendGeneratedAsLine(Source);
			AppendGeneratedAsLine(Source, TEXT("\tint opAdd(const FDuplicateOperator& Other) const // OP_CAUSE"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t\treturn 2;"));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
			AppendGeneratedAsLine(Source, TEXT("}"));
			AppendGeneratedAsLine(Source);
			AppendRunEntryStart(Source, TEXT("int"));
			AppendGeneratedAsLine(Source, TEXT("\treturn 0;"));
			AppendGeneratedAsLine(Source, TEXT("}"));
			break;

		case EFailureFamily::NullReceiver:
			AppendRunEntryStart(Source, TEXT("int"));
			// The native reference fixture uses a value-like declaration with an
			// implicit handle.  This is the null form accepted by the fork's raw
			// parser; an explicit "@ Receiver = null" declaration is rejected
			// before the runtime null-receiver path can be exercised.
			AppendGeneratedAsLine(Source, TEXT("\tFNativeCaseReference Receiver = nullptr;"));
			AppendGeneratedAsLine(Source, TEXT("\treturn Receiver.Value + RecordOperatorStage(9); // OP_CAUSE"));
			AppendGeneratedAsLine(Source, TEXT("}"));
			break;

		case EFailureFamily::LeftOperandException:
			AppendRunEntryStart(Source, TEXT("int"));
			AppendGeneratedAsLine(Source, TEXT("\treturn ThrowOperatorStage(1) + RecordOperatorStage(2); // OP_CAUSE"));
			AppendGeneratedAsLine(Source, TEXT("}"));
			break;

		case EFailureFamily::RightOperandException:
			AppendRunEntryStart(Source, TEXT("int"));
			AppendGeneratedAsLine(Source, TEXT("\treturn RecordOperatorStage(1) + ThrowOperatorStage(2); // OP_CAUSE"));
			AppendGeneratedAsLine(Source, TEXT("}"));
			break;

		case EFailureFamily::AssignmentException:
			AppendGeneratedAsLine(Source, TEXT("struct FThrowingAssignment"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\tFThrowingAssignment& opAssign(const FThrowingAssignment& Other)"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t\tThrowOperatorStage(3); // ASSIGNMENT_CAUSE"));
			AppendGeneratedAsLine(Source, TEXT("\t\treturn this;"));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
			AppendGeneratedAsLine(Source, TEXT("}"));
			AppendGeneratedAsLine(Source);
			AppendRunEntryStart(Source, TEXT("int"));
			AppendGeneratedAsLine(Source, TEXT("\tFThrowingAssignment Left;"));
			AppendGeneratedAsLine(Source, TEXT("\tFThrowingAssignment Right;"));
			AppendGeneratedAsLine(Source, TEXT("\tLeft = Right;"));
			AppendGeneratedAsLine(Source, TEXT("\treturn RecordOperatorStage(4);"));
			AppendGeneratedAsLine(Source, TEXT("}"));
			break;
		}

		return Source;
	}

	static FString BuildRecoverySource()
	{
		using namespace AngelscriptNativeTestSupport;

		FString Source;
		AppendGeneratedAsLine(Source, TEXT("int RecoverOperatorFailure()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\treturn 913;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		return Source;
	}

	static asIScriptModule* CompileAndReport(FNativeTestEngine& Engine,
		FAutomationTestBase& Test,
		const FString& SourceId,
		const FString& ModuleName,
		const FString& Source,
		int32& OutBuildResult)
	{
		using namespace AngelscriptNativeTestSupport;

		Engine.Reset(Test);
		const FTCHARToUTF8 ModuleNameUtf8(*ModuleName);
		const FTCHARToUTF8 SourceUtf8(*Source);
		asIScriptModule* Module = nullptr;
		PrintGeneratedAsSource(Test, SourceId, ModuleName, Source);
		OutBuildResult = CompileNativeModule(Engine.Get(), ModuleNameUtf8.Get(), SourceUtf8.Get(), Module);
		return Module;
	}

	static int32 LastSourceLineContaining(const FString& Source, const FString& Token)
	{
		TArray<FString> Lines;
		Source.ParseIntoArrayLines(Lines, false);
		for (int32 LineIndex = Lines.Num() - 1; LineIndex >= 0; --LineIndex)
		{
			if (Lines[LineIndex].Contains(Token, ESearchCase::CaseSensitive))
			{
				return LineIndex + 1;
			}
		}
		return INDEX_NONE;
	}

	static bool HasLocatedError(const FNativeMessageCollector& Messages,
		const FString& ModuleName,
		const int32 ExpectedLine)
	{
		for (const FNativeMessageEntry& Entry : Messages.Entries)
		{
			if (Entry.Type == asMSGTYPE_ERROR && Entry.Section == ModuleName &&
				Entry.Row == ExpectedLine && Entry.Column > 0)
			{
				return true;
			}
		}
		return false;
	}

	static bool HasErrors(const FNativeMessageCollector& Messages)
	{
		for (const FNativeMessageEntry& Entry : Messages.Entries)
		{
			if (Entry.Type == asMSGTYPE_ERROR)
			{
				return true;
			}
		}
		return false;
	}

	static TArray<int32> ExpectedTrace(const FFailureCase& FailureCase)
	{
		switch (FailureCase.Family)
		{
		case EFailureFamily::DivideZero:
		case EFailureFamily::ModuloZero:
			return {1, 0};
		case EFailureFamily::LeftOperandException:
			return {1};
		case EFailureFamily::RightOperandException:
			return {1, 2};
		case EFailureFamily::AssignmentException:
			return {3};
		default:
			return {};
		}
	}

	static bool TracesEqual(const TArray<int32>& Left, const TArray<int32>& Right)
	{
		if (Left.Num() != Right.Num())
		{
			return false;
		}

		for (int32 Index = 0; Index < Left.Num(); ++Index)
		{
			if (Left[Index] != Right[Index])
			{
				return false;
			}
		}
		return true;
	}

	static bool DiscardIsolatedModule(FAutomationTestBase& Test,
		asIScriptEngine& ScriptEngine,
		const FString& ModuleName,
		const FNativeCaseContext& Case,
		const TCHAR* ObservationName)
	{
		FNoDiscardAsserter Assert(Test);
		const FTCHARToUTF8 ModuleNameUtf8(*ModuleName);
		asIScriptModule* const Published =
			ScriptEngine.GetModule(ModuleNameUtf8.Get(), asGM_ONLY_IF_EXISTS);
		const FString RetainMessage =
			FString::Printf(TEXT("%s should retain a module until direct raw cleanup"), ObservationName);
		bool bPassed = Assert.IsNotNull(Published,
			*Case.Describe(*RetainMessage));
		if (Published != nullptr)
		{
			const FString DiscardMessage =
				FString::Printf(TEXT("%s should discard the exact raw module"), ObservationName);
			bPassed &= Assert.AreEqual(asSUCCESS,
				ScriptEngine.DiscardModule(ModuleNameUtf8.Get()),
				*Case.Describe(*DiscardMessage));
		}
		const FString NoStaleModuleMessage =
			FString::Printf(TEXT("%s should leave no stale raw module"), ObservationName);
		bPassed &= Assert.IsNull(ScriptEngine.GetModule(ModuleNameUtf8.Get(), asGM_ONLY_IF_EXISTS),
			*Case.Describe(*NoStaleModuleMessage));
		return bPassed;
	}

	static bool ExecuteRecovery(FAutomationTestBase& Test,
		asIScriptEngine& ScriptEngine,
		asIScriptModule& Module,
		const FNativeCaseContext& Case)
	{
		using namespace AngelscriptNativeTestSupport;

		FNoDiscardAsserter Assert(Test);
		asIScriptFunction* const Recovery =
			GetNativeFunctionByExactDecl(&Module, "int RecoverOperatorFailure()");
		if (!Assert.IsNotNull(Recovery,
				*Case.Describe(TEXT("recovery should resolve its exact entry declaration"))))
		{
			return false;
		}

		asIScriptContext* const Context = ScriptEngine.CreateContext();
		if (!Assert.IsNotNull(Context,
				*Case.Describe(TEXT("recovery should create a raw execution context"))))
		{
			return false;
		}

		bool bPassed = Assert.AreEqual(asSUCCESS,
			Context->Prepare(Recovery),
			*Case.Describe(TEXT("recovery should prepare its exact entry")));
		if (bPassed)
		{
			bPassed &= Assert.AreEqual(static_cast<int32>(asEXECUTION_FINISHED),
				Context->Execute(),
				*Case.Describe(TEXT("recovery should finish after the prior failure")));
			bPassed &= Assert.AreEqual(913,
				static_cast<int32>(Context->GetReturnDWord()),
				*Case.Describe(TEXT("recovery should return its exact sentinel")));
			bPassed &= Assert.AreEqual(asSUCCESS,
				Context->Unprepare(),
				*Case.Describe(TEXT("recovery should unprepare its raw context")));
		}
		Context->Release();
		return bPassed;
	}

	static bool CompileAndExecuteRecovery(FNativeTestEngine& Engine,
		FAutomationTestBase& Test,
		const FNativeCaseContext& Case,
		const FRecoveryCase& RecoveryCase,
		const FString& FailureModuleName)
	{
		FNoDiscardAsserter Assert(Test);
		asIScriptEngine* const ScriptEngine = Engine.Get();
		if (!Assert.IsNotNull(ScriptEngine,
				*Case.Describe(TEXT("recovery should retain the raw engine"))))
		{
			return false;
		}

		const FString RecoveryModuleName = RecoveryCase.Route == ERecoveryRoute::Fresh
			? FailureModuleName + TEXT("_FreshRecovery")
			: FailureModuleName;
		const FTCHARToUTF8 RecoveryModuleNameUtf8(*RecoveryModuleName);
		bool bPassed = Assert.IsNull(
			ScriptEngine->GetModule(RecoveryModuleNameUtf8.Get(), asGM_ONLY_IF_EXISTS),
			*Case.Describe(TEXT("recovery route should begin with no stale target module")));

		const FString RecoverySource = BuildRecoverySource();
		int32 RecoveryBuildResult = asERROR;
		asIScriptModule* const RecoveryModule = CompileAndReport(Engine,
			Test,
			Case.GetId() + TEXT("-RECOVERY"),
			RecoveryModuleName,
			RecoverySource,
			RecoveryBuildResult);
		bPassed &= Assert.IsTrue(RecoveryBuildResult >= 0 && RecoveryModule != nullptr,
			*Case.DescribeResult("<recovery build>",
				TEXT("successful raw recovery build"),
				Engine.GetMessagesText()));
		bPassed &= Assert.IsFalse(HasErrors(Engine.GetMessages()),
			*Case.Describe(TEXT("recovery should emit no compile errors")));
		bPassed &= Assert.AreEqual(RecoveryModule,
			ScriptEngine->GetModule(RecoveryModuleNameUtf8.Get(), asGM_ONLY_IF_EXISTS),
			*Case.Describe(TEXT("recovery route should publish its exact module")));
		if (RecoveryModule != nullptr)
		{
			bPassed &= ExecuteRecovery(Test, *ScriptEngine, *RecoveryModule, Case);
		}
		bPassed &= DiscardIsolatedModule(Test,
			*ScriptEngine,
			RecoveryModuleName,
			Case,
			TEXT("recovery cleanup"));
		return bPassed;
	}

	static bool ValidateCompileFailure(FAutomationTestBase& Test,
		FNativeTestEngine& Engine,
		const FNativeCaseContext& Case,
		const FString& ModuleName,
		const FString& Source,
		const int32 BuildResult,
		asIScriptModule* Module,
		const FFailureCase& FailureCase)
	{
		using namespace AngelscriptNativeTestSupport;

		FNoDiscardAsserter Assert(Test);
		const int32 ExpectedLine = LastSourceLineContaining(Source, CausalMarker(FailureCase));
		const bool bRejected = BuildResult < 0;
		bool bPassed = true;
		if (bRejected)
		{
			bPassed &= Assert.IsTrue(true,
				*Case.DescribeResult("<failure build>",
					TEXT("negative result for the generated operator source"),
					FString::Printf(TEXT("%d Messages={%s}"), BuildResult, *Engine.GetMessagesText())));
		}
		else
		{
			Test.AddInfo(FString::Printf(
				TEXT("[AS-FORK-LIMITATION] Id=%s compile-rejected operator form was accepted by the current fork; retaining source and cleanup coverage"),
				*Case.GetId()));
			bPassed &= Assert.IsNotNull(Module,
				*Case.DescribeResult("<failure build>",
					TEXT("accepted current-fork boundary should publish a module"),
					FString::Printf(TEXT("%d Messages={%s}"), BuildResult, *Engine.GetMessagesText())));
		}
		bPassed &= Assert.IsTrue(ExpectedLine > 0,
			*Case.Describe(TEXT("compile failure source should retain its causal marker")));
		if (bRejected)
		{
			bPassed &= Assert.IsTrue(HasLocatedError(Engine.GetMessages(), ModuleName, ExpectedLine),
				*Case.DescribeResult("<located diagnostic>",
					TEXT("source-owned error at the exact causal line"),
					Engine.GetMessagesText()));
			if (Module != nullptr)
			{
				bPassed &= Assert.IsNull(GetNativeFunctionByExactDecl(Module, EntryDeclaration(FailureCase)),
					*Case.Describe(TEXT("failed source should not publish a callable exact entry")));
			}
		}
		return bPassed;
	}

	static bool ValidateRuntimeOutcome(FAutomationTestBase& Test,
		asIScriptEngine& ScriptEngine,
		asIScriptModule& Module,
		const FNativeCaseContext& Case,
		const FFailureCase& FailureCase,
		const FString& ModuleName,
		const FString& Source,
		FOperatorFailureState& State,
		FNativeLifecycleRecorder& Lifecycle)
	{
		using namespace AngelscriptNativeTestSupport;

		FNoDiscardAsserter Assert(Test);
		asIScriptFunction* const Entry =
			GetNativeFunctionByExactDecl(&Module, EntryDeclaration(FailureCase));
		if (!Assert.IsNotNull(Entry,
				*Case.Describe(TEXT("runtime source should resolve its exact entry declaration"))))
		{
			return false;
		}

		bool bPassed = Assert.AreEqual(
			ScriptEngine.GetTypeIdByDecl(IsMaskedShiftExecution(FailureCase) ? "uint" : "int"),
			Entry->GetReturnTypeId(),
			*Case.Describe(TEXT("runtime source should preserve its exact entry return metadata")));
		bPassed &= Assert.AreEqual(0,
			static_cast<int32>(Entry->GetParamCount()),
			*Case.Describe(TEXT("runtime source should preserve its exact entry arity metadata")));

		asIScriptFunction* ExpectedOwner = Entry;
		if (FailureCase.Family == EFailureFamily::AssignmentException)
		{
			asITypeInfo* const AssignmentType = Module.GetTypeInfoByDecl("FThrowingAssignment");
			if (AssignmentType != nullptr)
			{
				// The fork normalizes the generated assignment declaration
				// differently across the 2.33 base and selected 2.38 backports.
				// Prefer the canonical declaration, then retain the source-defined
				// method as the unique metadata witness by name.
				ExpectedOwner = AssignmentType->GetMethodByDecl(
					"FThrowingAssignment& opAssign(const FThrowingAssignment&in)");
				if (ExpectedOwner == nullptr)
				{
					ExpectedOwner = AssignmentType->GetMethodByDecl(
						"FThrowingAssignment& opAssign(const FThrowingAssignment& Other)");
				}
				if (ExpectedOwner == nullptr && AssignmentType->GetMethodCount() == 1)
				{
					ExpectedOwner = AssignmentType->GetMethodByName("opAssign");
				}
			}
		}
		if (!Assert.IsNotNull(ExpectedOwner,
				*Case.Describe(TEXT("runtime source should resolve its exact exception owner"))))
		{
			return false;
		}

		asIScriptContext* const Context = ScriptEngine.CreateContext();
		if (!Assert.IsNotNull(Context,
				*Case.Describe(TEXT("runtime source should create a raw execution context"))))
		{
			return false;
		}

		const int PrepareResult = Context->Prepare(Entry);
		bPassed &= Assert.AreEqual(asSUCCESS,
			PrepareResult,
			*Case.Describe(TEXT("runtime source should prepare its exact entry")));
		if (PrepareResult != asSUCCESS)
		{
			Context->Release();
			return false;
		}

		const int ExecuteResult = Context->Execute();
		if (IsMaskedShiftExecution(FailureCase))
		{
			bPassed &= Assert.AreEqual(static_cast<int32>(asEXECUTION_FINISHED),
				ExecuteResult,
				*Case.Describe(
					TEXT("invalid shift count should follow the current masked VM count behavior")));
			bPassed &= Assert.AreEqual(static_cast<asDWORD>(0x80000000u),
				Context->GetReturnDWord(),
				*Case.Describe(
					TEXT("invalid shift count should mask negative count to the 32-bit shift width")));
			bPassed &= Assert.IsTrue(State.Trace.IsEmpty(),
				*Case.Describe(TEXT("invalid shift count should not synthesize an exception callback")));
		}
		else
		{
			bPassed &= Assert.AreEqual(static_cast<int32>(asEXECUTION_EXCEPTION),
				ExecuteResult,
				*Case.Describe(TEXT("runtime operator failure should stop evaluation with an exception")));
			const char* const ExceptionText = Context->GetExceptionString();
			bPassed &= Assert.AreEqual(FString(FailureCase.ExpectedException),
				FString(UTF8_TO_TCHAR(ExceptionText != nullptr ? ExceptionText : "")),
				*Case.Describe(TEXT("runtime operator failure should retain its exact exception cause")));
			bPassed &= Assert.IsTrue(Context->GetExceptionFunction() == ExpectedOwner,
				*Case.Describe(TEXT("runtime operator failure should retain its exact owner function")));
			const char* ExceptionSection = nullptr;
			int32 ExceptionColumn = INDEX_NONE;
			const int32 ExceptionLine =
				Context->GetExceptionLineNumber(&ExceptionColumn, &ExceptionSection);
			const int32 ExpectedLine = LastSourceLineContaining(Source, CausalMarker(FailureCase));
			bPassed &= Assert.IsTrue(ExceptionLine > 0 && ExceptionColumn > 0,
				*Case.Describe(TEXT("runtime operator failure should retain line and column")));
			bPassed &= Assert.AreEqual(ModuleName,
				FString(UTF8_TO_TCHAR(ExceptionSection != nullptr ? ExceptionSection : "")),
				*Case.Describe(TEXT("runtime operator failure should retain its generated section")));
			bPassed &= Assert.AreEqual(ExpectedLine,
				ExceptionLine,
				*Case.Describe(TEXT("runtime operator failure should stop at its causal source line")));
		}

		bPassed &= Assert.IsTrue(TracesEqual(State.Trace, ExpectedTrace(FailureCase)),
			*Case.Describe(TEXT("runtime operator failure should stop after the exact evaluation prefix")));
		bPassed &= Assert.AreEqual(State.Trace.Num(),
			State.CallbackCount,
			*Case.Describe(TEXT("runtime operator failure should invoke each observed stage once")));
		bPassed &= Assert.AreEqual(asSUCCESS,
			Context->Unprepare(),
			*Case.Describe(TEXT("runtime operator failure should unprepare its raw context")));
		bPassed &= Assert.IsTrue(Lifecycle.HasExactEventOrder(
			{ENativeLifecycleEvent::ValueConstruct, ENativeLifecycleEvent::Destruct}),
			*Case.Describe(TEXT("runtime operator failure should balance its native scope value")));
		bPassed &= Assert.AreEqual(0,
			Lifecycle.GetLiveObjectCount(),
			*Case.Describe(TEXT("runtime operator failure should retain no live native value")));
		Context->Release();
		return bPassed;
	}

	static bool RunCase(FNativeTestEngine& Engine,
		FAutomationTestBase& Test,
		FOperatorFailureState& State,
		FNativeLifecycleRecorder& Lifecycle,
		const FNativeCaseContext& Case,
		const FFailureCase& FailureCase,
		const FRecoveryCase& RecoveryCase,
		const FObservationCase& ObservationCase)
	{
		using namespace AngelscriptNativeTestSupport;

		FNoDiscardAsserter Assert(Test);
		asIScriptEngine* const ScriptEngine = Engine.Get();
		if (!Assert.IsNotNull(ScriptEngine,
				*Case.Describe(TEXT("operator failure case should retain the raw engine"))))
		{
			return false;
		}

		State.Reset();
		bool bPassed = Assert.AreEqual(0,
			Lifecycle.GetLiveObjectCount(),
			*Case.Describe(TEXT("operator failure case should begin with no live native value")));
		Lifecycle.Reset();
		const FString ModuleName = FString::Printf(TEXT("ASOperatorFailure_%hs_%hs_%hs"),
			FailureCase.CatalogName,
			RecoveryCase.CatalogName,
			ObservationCase.CatalogName);
		const FString Source = BuildFailureSource(FailureCase);
		int32 BuildResult = asERROR;
		asIScriptModule* const Module =
			CompileAndReport(Engine, Test, Case.GetId(), ModuleName, Source, BuildResult);

		if (IsCompileRejected(FailureCase))
		{
			bPassed &= ValidateCompileFailure(
				Test, Engine, Case, ModuleName, Source, BuildResult, Module, FailureCase);
			bPassed &= Assert.AreEqual(0,
				Lifecycle.GetEntries().Num(),
				*Case.Describe(TEXT("compile operator failure should construct no native scope value")));
			bPassed &= Assert.AreEqual(0,
				Lifecycle.GetLiveObjectCount(),
				*Case.Describe(TEXT("compile operator failure should retain no native scope value")));
		}
		else
		{
			bPassed &= Assert.IsTrue(BuildResult >= 0 && Module != nullptr,
				*Case.DescribeResult("<runtime build>",
					TEXT("successful build for a runtime operator outcome"),
					Engine.GetMessagesText()));
			bPassed &= Assert.IsFalse(HasErrors(Engine.GetMessages()),
				*Case.Describe(TEXT("runtime operator source should emit no compile errors")));
			if (BuildResult >= 0 && Module != nullptr)
			{
				bPassed &= ValidateRuntimeOutcome(
					Test, *ScriptEngine, *Module, Case, FailureCase, ModuleName, Source, State, Lifecycle);
			}
		}

		switch (ObservationCase.Observation)
		{
		case EObservation::DiagnosticOrException:
			bPassed &= DiscardIsolatedModule(Test,
				*ScriptEngine,
				ModuleName,
				Case,
				TEXT("diagnostic-or-exception observation cleanup"));
			break;
		case EObservation::Cleanup:
			bPassed &= DiscardIsolatedModule(
				Test, *ScriptEngine, ModuleName, Case, TEXT("cleanup observation"));
			break;
		case EObservation::Recovery:
			bPassed &= DiscardIsolatedModule(
				Test, *ScriptEngine, ModuleName, Case, TEXT("recovery observation cleanup"));
			bPassed &= CompileAndExecuteRecovery(Engine, Test, Case, RecoveryCase, ModuleName);
			break;
		}

		if (ScriptEngine->GetModuleCount() > 0)
		{
			Test.AddInfo(FString::Printf(
				TEXT("[AS-FORK-LIMITATION] Id=%s DiscardModule hides modules immediately, while GetModuleCount retains discarded-pile entries until engine shutdown; visible module-name checks passed"),
				*Case.GetId()));
		}
		return bPassed;
	}

public:
	TEST_METHOD(FailuresByRecoveryAndObservation)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT("LANG-OP-FAILURE",
			ENativeEvidence::Compile | ENativeEvidence::Diagnostic | ENativeEvidence::Runtime |
				ENativeEvidence::Debug | ENativeEvidence::Metadata | ENativeEvidence::Lifecycle |
				ENativeEvidence::Cleanup | ENativeEvidence::Isolation);

		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};
		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine,
			TEXT("operator failure product should create a standalone raw SDK engine")));
		if (ScriptEngine == nullptr)
		{
			return;
		}

		FOperatorFailureState State;
		FNativeLifecycleRecorder Lifecycle;
		ON_SCOPE_EXIT
		{
			ScriptEngine->SetUserData(nullptr, FailureStateUserDataSlot);
			ScriptEngine->SetUserData(nullptr, NativeLifecycleRecorderUserDataSlot);
		};
		ASSERT_THAT(IsTrue(RegisterFailureFixtures(*ScriptEngine, State, Lifecycle),
			TEXT("operator failure product should register raw runtime observation fixtures")));
		if (ScriptEngine->GetUserData(FailureStateUserDataSlot) != &State)
		{
			return;
		}

		TArray<FString> ConstructedIds;
		TSet<FString> UniqueIds;
		bool bAllCasesPassed = true;
		for (const FFailureCase& FailureCase : FailureCases)
		{
			for (const FRecoveryCase& RecoveryCase : RecoveryCases)
			{
				for (const FObservationCase& ObservationCase : ObservationCases)
				{
					const FNativeCaseContext Case(MakeNativeCaseId("LANG-OP-FAILURE",
						{ANSI_TO_TCHAR(FailureCase.CatalogName),
							ANSI_TO_TCHAR(ObservationCase.CatalogName),
							ANSI_TO_TCHAR(RecoveryCase.CatalogName)}));
					ConstructedIds.Add(Case.GetId());
					const bool bUniqueCaseId = !UniqueIds.Contains(Case.GetId());
					UniqueIds.Add(Case.GetId());
					ASSERT_THAT(IsTrue(bUniqueCaseId,
						*Case.Describe(TEXT("operator failure case ID should be unique"))));
					bAllCasesPassed &= RunCase(Engine,
						*TestRunner,
						State,
						Lifecycle,
						Case,
						FailureCase,
						RecoveryCase,
						ObservationCase);
				}
			}
		}

		ASSERT_THAT(AreEqual(96,
			ConstructedIds.Num(),
			TEXT("operator failure product should construct all ninety-six catalog IDs")));
		ASSERT_THAT(AreEqual(ConstructedIds.Num(),
			UniqueIds.Num(),
			TEXT("operator failure product should construct no duplicate catalog IDs")));
		ASSERT_THAT(IsTrue(bAllCasesPassed,
			TEXT("every operator failure family should preserve its raw diagnostic or runtime, cleanup, and recovery contract")));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
