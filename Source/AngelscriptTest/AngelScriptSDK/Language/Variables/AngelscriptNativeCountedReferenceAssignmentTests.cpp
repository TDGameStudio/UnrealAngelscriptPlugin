#include "../../Support/AngelscriptNativeCaseTestSupport.h"
#include "../../Support/AngelscriptNativeCoreTestSupport.h"
#include "../../Support/AngelscriptNativeFixtureTestSupport.h"
#include "../../Support/AngelscriptNativeLanguageCaseTestSupport.h"

#include "AngelscriptTestMacros.h"
#include "CQTest.h"
#include "Misc/ScopeExit.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_objecttype.h"
#include "source/as_scriptfunction.h"
#include "EndAngelscriptHeaders.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FCountedReferenceAssignmentTests,
	"Angelscript.TestModule.AngelScriptSDK.Language.Variables.Lifetime",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	enum class EReferenceAssignmentScenario : uint8
	{
		FactoryLocal,
		Overwrite,
		NullAssignment,
		ParameterReturn,
		ExceptionFrame,
		SaveLoad,
		ParameterReturnSaveLoad,
	};

	static const TCHAR* GetReferenceAssignmentScenarioName(
		const EReferenceAssignmentScenario Scenario)
	{
		switch (Scenario)
		{
		case EReferenceAssignmentScenario::FactoryLocal:
			return TEXT("factory_local");
		case EReferenceAssignmentScenario::Overwrite:
			return TEXT("overwrite");
		case EReferenceAssignmentScenario::NullAssignment:
			return TEXT("null_assignment");
		case EReferenceAssignmentScenario::ParameterReturn:
			return TEXT("parameter_return");
		case EReferenceAssignmentScenario::ExceptionFrame:
			return TEXT("exception_frame");
		case EReferenceAssignmentScenario::SaveLoad:
			return TEXT("save_load");
		case EReferenceAssignmentScenario::ParameterReturnSaveLoad:
			return TEXT("parameter_return_save_load");
		default:
			return TEXT("unknown");
		}
	}

	static FString BuildReferenceAssignmentSource(
		const EReferenceAssignmentScenario Scenario)
	{
		using namespace AngelscriptNativeTestSupport;

		FString Source;
		if (Scenario == EReferenceAssignmentScenario::ParameterReturn
			|| Scenario == EReferenceAssignmentScenario::ParameterReturnSaveLoad)
		{
			AppendGeneratedAsLine(Source, TEXT("FNativeCaseReference PassCountedReference(FNativeCaseReference Input)"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\treturn Input;"));
			AppendGeneratedAsLine(Source, TEXT("}"));
			AppendGeneratedAsLine(Source);
		}
		else if (Scenario == EReferenceAssignmentScenario::ExceptionFrame)
		{
			AppendGeneratedAsLine(Source, TEXT("int RaiseCountedReferenceException()"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\tint Zero = 0;"));
			AppendGeneratedAsLine(Source, TEXT("\treturn 1 / Zero;"));
			AppendGeneratedAsLine(Source, TEXT("}"));
			AppendGeneratedAsLine(Source);
		}

		AppendGeneratedAsLine(Source, TEXT("int RunCountedReferenceAssignment()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		switch (Scenario)
		{
		case EReferenceAssignmentScenario::FactoryLocal:
		case EReferenceAssignmentScenario::SaveLoad:
			AppendGeneratedAsLine(Source, TEXT("\tFNativeCaseReference Source = CreateNativeCaseReference(410);"));
			AppendGeneratedAsLine(Source, TEXT("\tFNativeCaseReference Value;"));
			AppendGeneratedAsLine(Source, TEXT("\tValue = Source;"));
			AppendGeneratedAsLine(Source, TEXT("\treturn Value.Value;"));
			break;
		case EReferenceAssignmentScenario::Overwrite:
			AppendGeneratedAsLine(Source, TEXT("\tFNativeCaseReference Value = CreateNativeCaseReference(420);"));
			AppendGeneratedAsLine(Source, TEXT("\tValue = CreateNativeCaseReference(421);"));
			AppendGeneratedAsLine(Source, TEXT("\treturn Value.Value;"));
			break;
		case EReferenceAssignmentScenario::NullAssignment:
			AppendGeneratedAsLine(Source, TEXT("\tFNativeCaseReference Value = CreateNativeCaseReference(430);"));
			AppendGeneratedAsLine(Source, TEXT("\tValue = nullptr;"));
			AppendGeneratedAsLine(Source, TEXT("\tif (Value == nullptr)"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t\treturn 1;"));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
			AppendGeneratedAsLine(Source, TEXT("\treturn 0;"));
			break;
		case EReferenceAssignmentScenario::ParameterReturn:
		case EReferenceAssignmentScenario::ParameterReturnSaveLoad:
			AppendGeneratedAsLine(Source, TEXT("\tFNativeCaseReference Value = CreateNativeCaseReference(440);"));
			AppendGeneratedAsLine(Source, TEXT("\tFNativeCaseReference Result = PassCountedReference(Value);"));
			AppendGeneratedAsLine(Source, TEXT("\treturn Result.Value;"));
			break;
		case EReferenceAssignmentScenario::ExceptionFrame:
			AppendGeneratedAsLine(Source, TEXT("\tFNativeCaseReference Value = CreateNativeCaseReference(450);"));
			AppendGeneratedAsLine(Source, TEXT("\treturn RaiseCountedReferenceException() + Value.Value;"));
			break;
		default:
			AppendGeneratedAsLine(Source, TEXT("\treturn -1;"));
			break;
		}
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("int RecoverCountedReferenceAssignment()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\treturn 89;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		return Source;
	}

	static FString DescribeBytecode(asIScriptFunction* Function)
	{
		if (Function == nullptr)
		{
			return TEXT("<null function>");
		}

		asUINT BytecodeLength = 0;
		const asDWORD* const Bytecode = Function->GetByteCode(&BytecodeLength);
		if (Bytecode == nullptr || BytecodeLength == 0)
		{
			return TEXT("<empty bytecode>");
		}

		TArray<FString> Instructions;
		asUINT DwordIndex = 0;
		while (DwordIndex < BytecodeLength)
		{
			const asEBCInstr Opcode =
				static_cast<asEBCInstr>(*reinterpret_cast<const asBYTE*>(&Bytecode[DwordIndex]));
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
					TEXT("%u:%hs<size=%d outside length=%u>"),
					DwordIndex,
					asBCInfo[Opcode].name,
					InstructionSize,
					BytecodeLength));
				break;
			}

			FString EncodedWords;
			for (int32 WordIndex = 0; WordIndex < InstructionSize; ++WordIndex)
			{
				if (!EncodedWords.IsEmpty())
				{
					EncodedWords += TEXT(",");
				}
				EncodedWords += FString::Printf(
					TEXT("%08x"),
					Bytecode[DwordIndex + static_cast<asUINT>(WordIndex)]);
			}

			Instructions.Add(FString::Printf(
				TEXT("%u:%hs<size=%d words=%s>"),
				DwordIndex,
				asBCInfo[Opcode].name,
				InstructionSize,
				*EncodedWords));
			DwordIndex += static_cast<asUINT>(InstructionSize);
		}

		return FString::Join(Instructions, TEXT("; "));
	}

	static bool ContainsBytecodeOpcode(
		asIScriptFunction* Function,
		const asEBCInstr ExpectedOpcode)
	{
		if (Function == nullptr)
		{
			return false;
		}

		asUINT BytecodeLength = 0;
		const asDWORD* const Bytecode = Function->GetByteCode(&BytecodeLength);
		if (Bytecode == nullptr || BytecodeLength == 0)
		{
			return false;
		}

		asUINT DwordIndex = 0;
		while (DwordIndex < BytecodeLength)
		{
			const asEBCInstr Opcode =
				static_cast<asEBCInstr>(*reinterpret_cast<const asBYTE*>(&Bytecode[DwordIndex]));
			if (Opcode == ExpectedOpcode)
			{
				return true;
			}
			if (static_cast<int32>(Opcode) > static_cast<int32>(asBC_MAXBYTECODE))
			{
				return false;
			}

			const int32 InstructionSize = asBCTypeSize[asBCInfo[Opcode].type];
			if (InstructionSize <= 0
				|| DwordIndex + static_cast<asUINT>(InstructionSize) > BytecodeLength)
			{
				return false;
			}
			DwordIndex += static_cast<asUINT>(InstructionSize);
		}

		return false;
	}

	struct FTypedReferenceCopyObservation
	{
		int32 Count = 0;
		bool bAllOperandsMatch = true;
		int32 FirstVariableOffset = INDEX_NONE;
	};

	static FTypedReferenceCopyObservation ObserveTypedReferenceCopies(
		asIScriptFunction* Function,
		const asEBCInstr ExpectedOpcode,
		const asITypeInfo* ExpectedType)
	{
		FTypedReferenceCopyObservation Observation;
		if (Function == nullptr || ExpectedType == nullptr)
		{
			Observation.bAllOperandsMatch = false;
			return Observation;
		}

		asUINT BytecodeLength = 0;
		const asDWORD* const Bytecode = Function->GetByteCode(&BytecodeLength);
		if (Bytecode == nullptr || BytecodeLength == 0)
		{
			Observation.bAllOperandsMatch = false;
			return Observation;
		}

		asUINT DwordIndex = 0;
		while (DwordIndex < BytecodeLength)
		{
			const asEBCInstr Opcode =
				static_cast<asEBCInstr>(*reinterpret_cast<const asBYTE*>(&Bytecode[DwordIndex]));
			if (static_cast<int32>(Opcode) > static_cast<int32>(asBC_MAXBYTECODE))
			{
				Observation.bAllOperandsMatch = false;
				break;
			}

			const int32 InstructionSize = asBCTypeSize[asBCInfo[Opcode].type];
			if (InstructionSize <= 0
				|| DwordIndex + static_cast<asUINT>(InstructionSize) > BytecodeLength)
			{
				Observation.bAllOperandsMatch = false;
				break;
			}

			if (Opcode == ExpectedOpcode)
			{
				++Observation.Count;
				Observation.bAllOperandsMatch &=
					reinterpret_cast<const asITypeInfo*>(
						asBC_PTRARG(&Bytecode[DwordIndex])) == ExpectedType;
				if (ExpectedOpcode == asBC_RefCpyV
					&& Observation.FirstVariableOffset == INDEX_NONE)
				{
					Observation.FirstVariableOffset =
						static_cast<int32>(asBC_SWORDARG0(&Bytecode[DwordIndex]));
				}
			}
			DwordIndex += static_cast<asUINT>(InstructionSize);
		}

		return Observation;
	}

	static bool FindObjectMoveInstruction(
		asIScriptFunction* Function,
		int32& OutStackOffset,
		int32& OutVariableOffset)
	{
		OutStackOffset = INDEX_NONE;
		OutVariableOffset = INDEX_NONE;
		if (Function == nullptr)
		{
			return false;
		}

		asUINT BytecodeLength = 0;
		const asDWORD* const Bytecode = Function->GetByteCode(&BytecodeLength);
		if (Bytecode == nullptr || BytecodeLength == 0)
		{
			return false;
		}

		asUINT DwordIndex = 0;
		while (DwordIndex < BytecodeLength)
		{
			const asEBCInstr Opcode =
				static_cast<asEBCInstr>(*reinterpret_cast<const asBYTE*>(&Bytecode[DwordIndex]));
			if (static_cast<int32>(Opcode) > static_cast<int32>(asBC_MAXBYTECODE))
			{
				return false;
			}

			const int32 InstructionSize = asBCTypeSize[asBCInfo[Opcode].type];
			if (InstructionSize <= 0
				|| DwordIndex + static_cast<asUINT>(InstructionSize) > BytecodeLength)
			{
				return false;
			}

			if (Opcode == asBC_GETOBJ)
			{
				OutStackOffset = static_cast<int32>(asBC_WORDARG0(&Bytecode[DwordIndex]));
				OutVariableOffset = static_cast<int32>(asBC_SWORDARG1(&Bytecode[DwordIndex]));
				return true;
			}

			DwordIndex += static_cast<asUINT>(InstructionSize);
		}

		return false;
	}


	static FString DescribeFunctionCallLayout(asIScriptFunction* Function)
	{
		const asCScriptFunction* const InternalFunction =
			static_cast<const asCScriptFunction*>(Function);
		if (InternalFunction == nullptr)
		{
			return TEXT("<missing script-function call layout>");
		}

		FString ParameterOffsets;
		for (asUINT Index = 0; Index < InternalFunction->parameterOffsets.GetLength(); ++Index)
		{
			if (!ParameterOffsets.IsEmpty())
			{
				ParameterOffsets += TEXT(", ");
			}
			ParameterOffsets += FString::FromInt(InternalFunction->parameterOffsets[Index]);
		}

		return FString::Printf(
			TEXT("return=%s return-on-stack=%d argument-space=%d total-space=%d parameter-offsets=[%s]"),
			UTF8_TO_TCHAR(InternalFunction->returnType.Format(InternalFunction->nameSpace).AddressOf()),
			InternalFunction->DoesReturnOnStack(),
			InternalFunction->spaceNeededForArguments,
			InternalFunction->totalSpaceBeforeFunction,
			*ParameterOffsets);
	}

	static FString DescribeObjectLifetimeMetadata(asIScriptFunction* Function)
	{
		const asCScriptFunction* const InternalFunction =
			static_cast<const asCScriptFunction*>(Function);
		if (InternalFunction == nullptr || InternalFunction->scriptData == nullptr)
		{
			return TEXT("<missing script-function metadata>");
		}

		const asCScriptFunction::ScriptFunctionData& Data = *InternalFunction->scriptData;
		FString ObjectEntries;
		for (asUINT Index = 0; Index < Data.objVariablePos.GetLength(); ++Index)
		{
			if (!ObjectEntries.IsEmpty())
			{
				ObjectEntries += TEXT(", ");
			}

			const asCTypeInfo* const Type = Data.objVariableTypes[Index];
			ObjectEntries += FString::Printf(
				TEXT("{offset=%d heap=%d type=%s}"),
				Data.objVariablePos[Index],
				Index < Data.objVariablesOnHeap,
				Type != nullptr ? UTF8_TO_TCHAR(Type->GetName()) : TEXT("<null>"));
		}

		FString VariableEntries;
		for (asUINT Index = 0; Index < Data.variables.GetLength(); ++Index)
		{
			const asSScriptVariable* const Variable = Data.variables[Index];
			if (Variable == nullptr)
			{
				continue;
			}

			if (!VariableEntries.IsEmpty())
			{
				VariableEntries += TEXT(", ");
			}

			VariableEntries += FString::Printf(
				TEXT("{offset=%d heap=%d type=%s name=%s}"),
				Variable->stackOffset,
				Variable->onHeap,
				UTF8_TO_TCHAR(Variable->type.Format(InternalFunction->nameSpace).AddressOf()),
				UTF8_TO_TCHAR(Variable->name.AddressOf()));
		}

		return FString::Printf(
			TEXT("space=%d heap-count=%u objects=[%s] variables=[%s]"),
			Data.variableSpace,
			Data.objVariablesOnHeap,
			*ObjectEntries,
			*VariableEntries);
	}

	static bool DescribeNativeReferenceRegistration(
		asIScriptEngine& ScriptEngine,
		FString& OutDescription)
	{
		asITypeInfo* const Type = ScriptEngine.GetTypeInfoByDecl("FNativeCaseReference");
		if (Type == nullptr)
		{
			OutDescription = TEXT("<FNativeCaseReference type was not registered>");
			return false;
		}

		bool bHasAddRef = false;
		bool bHasRelease = false;
		FString BehaviourDeclarations;
		const asUINT BehaviourCount = Type->GetBehaviourCount();
		for (asUINT BehaviourIndex = 0; BehaviourIndex < BehaviourCount; ++BehaviourIndex)
		{
			asEBehaviours Behaviour = asBEHAVE_MAX;
			asIScriptFunction* const Function = Type->GetBehaviourByIndex(
				BehaviourIndex,
				&Behaviour);
			if (Function == nullptr)
			{
				continue;
			}

			if (!BehaviourDeclarations.IsEmpty())
			{
				BehaviourDeclarations += TEXT(", ");
			}
			BehaviourDeclarations += FString::Printf(
				TEXT("%d:%s"),
				static_cast<int32>(Behaviour),
				UTF8_TO_TCHAR(Function->GetDeclaration()));
			bHasAddRef |= Behaviour == asBEHAVE_ADDREF;
			bHasRelease |= Behaviour == asBEHAVE_RELEASE;
		}

		const asQWORD Flags = Type->GetFlags();
		const bool bReference = (Flags & asOBJ_REF) != 0;
		const bool bImplicitHandle = (Flags & asOBJ_IMPLICIT_HANDLE) != 0;
		const bool bNoCount = (Flags & asOBJ_NOCOUNT) != 0;
		const bool bHasUserData = Type->GetUserData() != nullptr;
		OutDescription = FString::Printf(
			TEXT("flags=0x%llx ref=%d implicitHandle=%d noCount=%d userData=%d behaviours=[%s]"),
			static_cast<uint64>(Flags),
			bReference,
			bImplicitHandle,
			bNoCount,
			bHasUserData,
			*BehaviourDeclarations);
		return bReference && bImplicitHandle && !bNoCount && !bHasUserData
			&& bHasAddRef && bHasRelease;
	}

public:
	TEST_METHOD(RetainedReferenceCopyFunctionOutlivesModuleLookupInBothOptimizationModes)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT_PART(
			"LANG-VAR-COUNTED-REFERENCE-ASSIGNMENT",
			"retained_function_ownership_after_module_lookup_removal");

		struct FOptimizationCase
		{
			const TCHAR* Name;
			bool bOptimize;
			asEBCInstr ExpectedCopyOpcode;
			asEBCInstr RejectedCopyOpcode;
		};

		const FOptimizationCase Cases[] =
		{
			{ TEXT("raw_refcpy"), false, asBC_REFCPY, asBC_RefCpyV },
			{ TEXT("optimized_refcpyv"), true, asBC_RefCpyV, asBC_REFCPY },
		};

		for (const FOptimizationCase& OptimizationCase : Cases)
		{
			FNativeLifecycleRecorder Lifecycle;
			FNativeTestEngine Engine;
			Engine.Create(*TestRunner);
			ON_SCOPE_EXIT
			{
				Engine.Destroy();
			};

			asIScriptEngine* const ScriptEngine = Engine.Get();
			ASSERT_THAT(IsNotNull(ScriptEngine,
				TEXT("Retained counted-reference function should create a case-owned raw SDK engine")));
			if (ScriptEngine == nullptr)
			{
				continue;
			}

			const FNativeCaseContext Case(MakeNativeCaseId(
				"LANG-VAR-COUNTED-REFERENCE-ASSIGNMENT",
				{ TEXT("retained_function"), OptimizationCase.Name }));
			ASSERT_THAT(AreEqual(
				static_cast<int32>(asSUCCESS),
				ScriptEngine->SetEngineProperty(
					asEP_OPTIMIZE_BYTECODE,
					OptimizationCase.bOptimize ? 1 : 0),
				*Case.Describe(TEXT("retained counted-reference function should select its exact optimization mode"))));
			ASSERT_THAT(IsTrue(RegisterNativeCaseReference(*ScriptEngine, &Lifecycle),
				*Case.Describe(TEXT("retained counted-reference function should register its counted reference fixture"))));

			asITypeInfo* const PublicReferenceType =
				ScriptEngine->GetTypeInfoByDecl("FNativeCaseReference");
			ASSERT_THAT(IsNotNull(PublicReferenceType,
				*Case.Describe(TEXT("retained counted-reference function should publish its exact fixture type"))));
			if (PublicReferenceType == nullptr)
			{
				continue;
			}
			asCTypeInfo& ReferenceType = *static_cast<asCTypeInfo*>(PublicReferenceType);
			const int32 RegisteredTypeReferenceBaseline =
				static_cast<int32>(ReferenceType.GetInternalReferenceCountForTesting());
			ASSERT_THAT(IsTrue(RegisteredTypeReferenceBaseline > 0,
				*Case.Describe(TEXT("retained counted-reference function should expose a positive type-reference baseline"))));

			const FString ScriptSource = ASTEST_AS(R"AS(
				int RunRetainedCountedReferenceCopy()
				{
					FNativeCaseReference Source = CreateNativeCaseReference(510);
					FNativeCaseReference Value;
					Value = Source;
					return Value.Value;
				}
				)AS");
			const FString ModuleName =
				Case.MakeModuleName(TEXT("RetainedCountedReferenceCopy"));
			PrintGeneratedAsSource(*TestRunner, Case.GetId(), ModuleName, ScriptSource);
			const FTCHARToUTF8 ModuleNameUtf8(*ModuleName);
			const FTCHARToUTF8 ScriptSourceUtf8(*ScriptSource);

			asIScriptModule* Module = nullptr;
			const int32 BuildResult = CompileNativeModule(
				ScriptEngine,
				ModuleNameUtf8.Get(),
				ScriptSourceUtf8.Get(),
				Module);
			ASSERT_THAT(AreEqual(
				static_cast<int32>(asSUCCESS),
				BuildResult,
				*Case.Describe(TEXT("retained counted-reference function source should compile"))));
			ASSERT_THAT(IsNotNull(Module,
				*Case.Describe(TEXT("retained counted-reference function source should publish a module"))));
			if (BuildResult != asSUCCESS || Module == nullptr)
			{
				continue;
			}

			asIScriptFunction* Entry = GetNativeFunctionByExactDecl(
				Module,
				"int RunRetainedCountedReferenceCopy()");
			ASSERT_THAT(IsNotNull(Entry,
				*Case.Describe(TEXT("retained counted-reference function should expose its exact entry declaration"))));
			if (Entry == nullptr)
			{
				ScriptEngine->DiscardModule(ModuleNameUtf8.Get());
				continue;
			}

			ASSERT_THAT(IsTrue(Entry->AddRef() > 0,
				*Case.Describe(TEXT("retained counted-reference function should acquire an external function owner"))));
			bool bEntryRetained = true;
			ON_SCOPE_EXIT
			{
				if (bEntryRetained)
				{
					Entry->Release();
				}
			};

			ASSERT_THAT(AreEqual(
				static_cast<int32>(asSUCCESS),
				ScriptEngine->DiscardModule(ModuleNameUtf8.Get()),
				*Case.Describe(TEXT("retained counted-reference function should discard its named module publication"))));
			ASSERT_THAT(IsNull(
				ScriptEngine->GetModule(ModuleNameUtf8.Get(), asGM_ONLY_IF_EXISTS),
				*Case.Describe(TEXT("retained counted-reference function should outlive a removed module lookup"))));

			const FTypedReferenceCopyObservation ExpectedCopies =
				ObserveTypedReferenceCopies(
					Entry,
					OptimizationCase.ExpectedCopyOpcode,
					PublicReferenceType);
			const FTypedReferenceCopyObservation RejectedCopies =
				ObserveTypedReferenceCopies(
					Entry,
					OptimizationCase.RejectedCopyOpcode,
					PublicReferenceType);
			const FString RetainedBytecode = DescribeBytecode(Entry);
			TestRunner->AddInfo(FString::Printf(
				TEXT("[%s] retained-function mode=%s bytecode=[%s] expected-copies=%d rejected-copies=%d type-ref-baseline=%d type-ref-retained=%d"),
				*Case.GetId(),
				OptimizationCase.bOptimize ? TEXT("optimized") : TEXT("raw"),
				*RetainedBytecode,
				ExpectedCopies.Count,
				RejectedCopies.Count,
				RegisteredTypeReferenceBaseline,
				static_cast<int32>(ReferenceType.GetInternalReferenceCountForTesting())));
			ASSERT_THAT(IsTrue(ExpectedCopies.Count > 0,
				*Case.Describe(TEXT("retained counted-reference function should preserve its mode-specific reference-copy opcode"))));
			ASSERT_THAT(AreEqual(0, RejectedCopies.Count,
				*Case.Describe(TEXT("retained counted-reference function should not mix raw and optimized copy forms"))));
			ASSERT_THAT(IsTrue(ExpectedCopies.bAllOperandsMatch,
				*Case.Describe(TEXT("retained counted-reference copy should keep its exact registered type operand"))));
			if (OptimizationCase.bOptimize)
			{
				ASSERT_THAT(IsTrue(ExpectedCopies.FirstVariableOffset > 0,
					*Case.Describe(TEXT("optimized retained counted-reference copy should retain its destination variable offset"))));
			}

			Lifecycle.Reset();
			asIScriptContext* const Context = ScriptEngine->CreateContext();
			ASSERT_THAT(IsNotNull(Context,
				*Case.Describe(TEXT("retained counted-reference function should create an execution context after discard"))));
			if (Context == nullptr)
			{
				continue;
			}

			const int32 ExecuteResult = PrepareAndExecute(Context, Entry);
			ASSERT_THAT(AreEqual(
				static_cast<int32>(asEXECUTION_FINISHED),
				ExecuteResult,
				*Case.Describe(TEXT("retained counted-reference function should execute after module lookup removal"))));
			if (ExecuteResult == asEXECUTION_FINISHED)
			{
				ASSERT_THAT(AreEqual(
					510,
					static_cast<int32>(Context->GetReturnDWord()),
					*Case.Describe(TEXT("retained counted-reference function should preserve its copied value"))));
			}
			ASSERT_THAT(AreEqual(
				static_cast<int32>(asSUCCESS),
				Context->Unprepare(),
				*Case.Describe(TEXT("retained counted-reference function context should unprepare exactly once"))));
			Context->Release();

			ASSERT_THAT(AreEqual(1, Lifecycle.Num(ENativeLifecycleEvent::ValueConstruct),
				*Case.Describe(TEXT("retained counted-reference function should construct one factory object"))));
			ASSERT_THAT(AreEqual(2, Lifecycle.Num(ENativeLifecycleEvent::AddRef),
				*Case.Describe(TEXT("retained counted-reference function should add the two local owners"))));
			ASSERT_THAT(AreEqual(3, Lifecycle.Num(ENativeLifecycleEvent::Release),
				*Case.Describe(TEXT("retained counted-reference function should release factory and local owners exactly once"))));
			ASSERT_THAT(AreEqual(1, Lifecycle.Num(ENativeLifecycleEvent::Destruct),
				*Case.Describe(TEXT("retained counted-reference function should destroy its factory object exactly once"))));
			ASSERT_THAT(AreEqual(0, Lifecycle.GetLiveObjectCount(),
				*Case.Describe(TEXT("retained counted-reference function should restore the object-lifetime baseline"))));

			const int32 RemainingFunctionReferences =
				static_cast<int32>(Entry->Release());
			bEntryRetained = false;
			Entry = nullptr;
			ASSERT_THAT(AreEqual(
				0,
				RemainingFunctionReferences,
				*Case.Describe(TEXT("retained counted-reference function release should consume its final owner"))));
			ASSERT_THAT(AreEqual(
				static_cast<int32>(asSUCCESS),
				ScriptEngine->GarbageCollect(asGC_FULL_CYCLE),
				*Case.Describe(TEXT("successful garbage collection should retire the no-longer-retained discarded module"))));
			ASSERT_THAT(IsNull(
				ScriptEngine->GetModule(ModuleNameUtf8.Get(), asGM_ONLY_IF_EXISTS),
				*Case.Describe(TEXT("discarded module lookup should remain absent after garbage-collection retirement"))));
			ASSERT_THAT(AreEqual(
				RegisteredTypeReferenceBaseline,
				static_cast<int32>(ReferenceType.GetInternalReferenceCountForTesting()),
				*Case.Describe(TEXT("retained counted-reference function release should restore the exact registered type-reference baseline"))));
			ASSERT_THAT(AreEqual(0, Lifecycle.GetLiveObjectCount(),
				*Case.Describe(TEXT("retained function release should not resurrect a native counted-reference object"))));
		}
	}

	TEST_METHOD(CountedReferenceAssignmentsBalanceAcrossOwnershipTransitions)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT("LANG-VAR-COUNTED-REFERENCE-ASSIGNMENT",
			ENativeEvidence::Compile
				| ENativeEvidence::Diagnostic
				| ENativeEvidence::Runtime
				| ENativeEvidence::Metadata
				| ENativeEvidence::Bytecode
				| ENativeEvidence::Lifecycle
				| ENativeEvidence::Cleanup
				| ENativeEvidence::Isolation);

		struct FReferenceAssignmentCase
		{
			EReferenceAssignmentScenario Scenario;
			int32 ExpectedReturnValue;
			int32 ExpectedConstructionCount;
			int32 ExpectedAddRefCount;
			int32 ExpectedReleaseCount;
			bool bExpectException;
		};

		const FReferenceAssignmentCase Cases[] =
		{
			{ EReferenceAssignmentScenario::FactoryLocal, 410, 1, 2, 3, false },
			{ EReferenceAssignmentScenario::Overwrite, 421, 2, 2, 4, false },
			{ EReferenceAssignmentScenario::NullAssignment, 1, 1, 1, 2, false },
			{ EReferenceAssignmentScenario::ParameterReturn, 440, 1, 4, 5, false },
			{ EReferenceAssignmentScenario::ExceptionFrame, 0, 1, 1, 2, true },
			{ EReferenceAssignmentScenario::SaveLoad, 410, 1, 2, 3, false },
			{ EReferenceAssignmentScenario::ParameterReturnSaveLoad, 440, 1, 4, 5, false },
		};

		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };
		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine,
			TEXT("Counted-reference assignment regression should create a raw SDK engine")));
		if (ScriptEngine == nullptr)
		{
			return;
		}

		FNativeLifecycleRecorder Lifecycle;
		ASSERT_THAT(IsTrue(RegisterNativeCaseReference(*ScriptEngine, &Lifecycle),
			TEXT("Counted-reference assignment regression should register the raw counted reference fixture")));

		FString ReferenceRegistration;
		ASSERT_THAT(IsTrue(DescribeNativeReferenceRegistration(*ScriptEngine, ReferenceRegistration),
			TEXT("Counted-reference assignment regression should retain addref/release type metadata")));
		TestRunner->AddInfo(FString::Printf(
			TEXT("[LANG-VAR-LIFETIME-COUNTED-REFERENCE-ASSIGNMENT] registration=%s"),
			*ReferenceRegistration));

		for (const FReferenceAssignmentCase& AssignmentCase : Cases)
		{
			Lifecycle.Reset();
			const FString ScenarioName = GetReferenceAssignmentScenarioName(AssignmentCase.Scenario);
			const FNativeCaseContext Case(MakeNativeCaseId(
				"LANG-VAR-COUNTED-REFERENCE-ASSIGNMENT",
				{ *ScenarioName }));
			const FString SourceModuleName = Case.MakeModuleName(TEXT("CountedReferenceAssignment"));
			const FString Source = BuildReferenceAssignmentSource(AssignmentCase.Scenario);
			PrintGeneratedAsSource(*TestRunner, Case.GetId(), SourceModuleName, Source);

			const FTCHARToUTF8 SourceModuleNameUtf8(*SourceModuleName);
			const FTCHARToUTF8 SourceUtf8(*Source);
			Engine.ResetMessages();
			asIScriptModule* Module = nullptr;
			const int BuildResult = CompileNativeModule(
				ScriptEngine,
				SourceModuleNameUtf8.Get(),
				SourceUtf8.Get(),
				Module);
			if (BuildResult != asSUCCESS)
			{
				TestRunner->AddInfo(FString::Printf(
					TEXT("[%s] compile-messages=[%s]"),
					*Case.GetId(),
					*CollectMessages(Engine.GetMessages())));
			}
			ASSERT_THAT(AreEqual(static_cast<int32>(asSUCCESS), BuildResult,
				*Case.Describe(TEXT("counted-reference assignment source should compile"))));
			ASSERT_THAT(IsNotNull(Module,
				*Case.Describe(TEXT("counted-reference assignment source should publish a module"))));
			if (BuildResult != asSUCCESS || Module == nullptr)
			{
				TestRunner->AddInfo(CollectMessages(Engine.GetMessages()));
				continue;
			}

			asIScriptFunction* Entry = GetNativeFunctionByExactDecl(
				Module,
				"int RunCountedReferenceAssignment()");
			ASSERT_THAT(IsNotNull(Entry,
				*Case.Describe(TEXT("counted-reference assignment module should expose its exact entry declaration"))));
			if (Entry == nullptr)
			{
				ScriptEngine->DiscardModule(SourceModuleNameUtf8.Get());
				continue;
			}

			const FString EntryBytecode = DescribeBytecode(Entry);
			const FString SourceObjectMetadata = DescribeObjectLifetimeMetadata(Entry);
			FString SourcePassThroughCallLayout;
			TestRunner->AddInfo(FString::Printf(
				TEXT("[%s] source bytecode=[%s]"),
				*Case.GetId(),
				*EntryBytecode));
			UE_LOG(LogTemp, Display,
				TEXT("[AS-BYTECODE] Id=%s Module=%s Program=[%s]"),
				*Case.GetId(),
				*SourceModuleName,
				*EntryBytecode);
			UE_LOG(LogTemp, Display,
				TEXT("[AS-OBJECT-METADATA] Stage=source Id=%s Module=%s Values=[%s]"),
				*Case.GetId(),
				*SourceModuleName,
				*SourceObjectMetadata);
			if (AssignmentCase.Scenario == EReferenceAssignmentScenario::ParameterReturn
				|| AssignmentCase.Scenario == EReferenceAssignmentScenario::ParameterReturnSaveLoad)
			{
				int32 ObjectMoveStackOffset = INDEX_NONE;
				int32 ObjectMoveVariableOffset = INDEX_NONE;
				ASSERT_THAT(IsTrue(FindObjectMoveInstruction(
					Entry,
					ObjectMoveStackOffset,
					ObjectMoveVariableOffset),
					*Case.Describe(TEXT("counted-reference parameter-return should move its unique argument temporary into the call stack"))));
				ASSERT_THAT(IsTrue(ObjectMoveStackOffset >= 0,
					*Case.Describe(TEXT("counted-reference parameter-return object move should target a non-negative call-stack offset"))));
				ASSERT_THAT(IsTrue(ObjectMoveVariableOffset > 0,
					*Case.Describe(TEXT("counted-reference parameter-return object move should retain its nonzero source variable offset"))));
				TestRunner->AddInfo(FString::Printf(
					TEXT("[%s] object-move={stack-offset=%d variable-offset=%d}"),
					*Case.GetId(),
					ObjectMoveStackOffset,
					ObjectMoveVariableOffset));

				asIScriptFunction* const PassThrough = GetNativeFunctionByExactDecl(
					Module,
					"FNativeCaseReference PassCountedReference(FNativeCaseReference)");
				ASSERT_THAT(IsNotNull(PassThrough,
					*Case.Describe(TEXT("counted-reference parameter-return source should publish its exact pass-through declaration"))));
				if (PassThrough != nullptr)
				{
					SourcePassThroughCallLayout = DescribeFunctionCallLayout(PassThrough);
					int ParameterTypeId = asINVALID_TYPE;
					asDWORD ParameterFlags = 0;
					const char* ParameterName = nullptr;
					ASSERT_THAT(AreEqual(static_cast<int32>(asSUCCESS), PassThrough->GetParam(
						0,
						&ParameterTypeId,
						&ParameterFlags,
						&ParameterName),
						*Case.Describe(TEXT("counted-reference pass-through should expose parameter ABI metadata"))));
					TestRunner->AddInfo(FString::Printf(
						TEXT("[%s] pass-through declaration=%s parameter={type-id=%d flags=0x%08x name=%s} call-layout=[%s] bytecode=[%s]"),
						*Case.GetId(),
						UTF8_TO_TCHAR(PassThrough->GetDeclaration()),
						ParameterTypeId,
						ParameterFlags,
						UTF8_TO_TCHAR(ParameterName != nullptr ? ParameterName : ""),
						*SourcePassThroughCallLayout,
						*DescribeBytecode(PassThrough)));
				}
			}
			const bool bUsesReferenceCopy = ContainsBytecodeOpcode(Entry, asBC_REFCPY)
				|| ContainsBytecodeOpcode(Entry, asBC_RefCpyV);
			ASSERT_THAT(IsTrue(bUsesReferenceCopy,
				*Case.Describe(TEXT("counted-reference assignment should retain a reference-copy instruction"))));
			if (bUsesReferenceCopy)
			{
				ASSERT_THAT(AreEqual(
					static_cast<int32>(1 + AS_PTR_SIZE),
					asBCTypeSize[asBCInfo[asBC_RefCpyV].type],
					*Case.Describe(TEXT("optimized counted-reference copy should carry its object type operand"))));
			}

			if (AssignmentCase.Scenario == EReferenceAssignmentScenario::SaveLoad
				|| AssignmentCase.Scenario == EReferenceAssignmentScenario::ParameterReturnSaveLoad)
			{
				FMemoryBinaryStream Stream;
				ASSERT_THAT(AreEqual(static_cast<int32>(asSUCCESS), Module->SaveByteCode(&Stream, false),
					*Case.Describe(TEXT("counted-reference assignment module should serialize its typed bytecode"))));
				ASSERT_THAT(IsTrue(Stream.Num() > 0,
					*Case.Describe(TEXT("counted-reference assignment serialization should write bytes"))));

				FMemoryBinaryStream LegacyStream;
				const asBYTE LegacyDebugInfoFlag = 0;
				ASSERT_THAT(AreEqual(static_cast<int32>(asSUCCESS), LegacyStream.Write(&LegacyDebugInfoFlag, 1),
					*Case.Describe(TEXT("counted-reference assignment incompatibility fixture should write an unframed legacy prefix"))));
				const FString LegacyModuleName = SourceModuleName + TEXT("_Legacy");
				const FTCHARToUTF8 LegacyModuleNameUtf8(*LegacyModuleName);
				asIScriptModule* const LegacyModule = ScriptEngine->GetModule(LegacyModuleNameUtf8.Get(), asGM_ALWAYS_CREATE);
				ASSERT_THAT(IsNotNull(LegacyModule,
					*Case.Describe(TEXT("counted-reference assignment incompatibility fixture should create a destination module"))));
				if (LegacyModule != nullptr)
				{
					Engine.ResetMessages();
					bool bLegacyDebugWasStripped = true;
					ASSERT_THAT(AreEqual(static_cast<int32>(asERROR), LegacyModule->LoadByteCode(&LegacyStream, &bLegacyDebugWasStripped),
						*Case.Describe(TEXT("unframed legacy bytecode should be rejected before instruction decoding"))));
					ASSERT_THAT(IsFalse(LegacyStream.HasReadError(),
						*Case.Describe(TEXT("unframed legacy bytecode should be rejected by its first marker byte without an end-of-stream read"))));
					TestRunner->AddInfo(FString::Printf(
						TEXT("[%s] legacy-load-messages=[%s]"),
						*Case.GetId(),
						*CollectMessages(Engine.GetMessages())));
				}
				ScriptEngine->DiscardModule(LegacyModuleNameUtf8.Get());
				ScriptEngine->DiscardModule(SourceModuleNameUtf8.Get());

				const FString RestoredModuleName = SourceModuleName + TEXT("_Restored");
				const FTCHARToUTF8 RestoredModuleNameUtf8(*RestoredModuleName);
				Stream.ResetReadPosition();
				Module = ScriptEngine->GetModule(RestoredModuleNameUtf8.Get(), asGM_ALWAYS_CREATE);
				ASSERT_THAT(IsNotNull(Module,
					*Case.Describe(TEXT("counted-reference assignment round trip should create a restore module"))));
				bool bDebugWasStripped = true;
				const int LoadResult = Module != nullptr
					? Module->LoadByteCode(&Stream, &bDebugWasStripped)
					: asNO_MODULE;
				ASSERT_THAT(AreEqual(static_cast<int32>(asSUCCESS), LoadResult,
					*Case.Describe(TEXT("counted-reference assignment round trip should load only the typed bytecode layout"))));
				ASSERT_THAT(IsFalse(bDebugWasStripped,
					*Case.Describe(TEXT("counted-reference assignment round trip should preserve debug information"))));
				if (LoadResult != asSUCCESS || Module == nullptr)
				{
					TestRunner->AddInfo(CollectMessages(Engine.GetMessages()));
					ScriptEngine->DiscardModule(RestoredModuleNameUtf8.Get());
					continue;
				}

				Entry = GetNativeFunctionByExactDecl(Module, "int RunCountedReferenceAssignment()");
				ASSERT_THAT(IsNotNull(Entry,
					*Case.Describe(TEXT("counted-reference assignment round trip should restore the exact entry declaration"))));
				if (Entry == nullptr)
				{
					ScriptEngine->DiscardModule(RestoredModuleNameUtf8.Get());
					continue;
				}
				const FString RestoredBytecode = DescribeBytecode(Entry);
				const FString RestoredObjectMetadata = DescribeObjectLifetimeMetadata(Entry);
				TestRunner->AddInfo(FString::Printf(
					TEXT("[%s] restored bytecode=[%s]"),
					*Case.GetId(),
					*RestoredBytecode));
				UE_LOG(LogTemp, Display,
					TEXT("[AS-BYTECODE-RESTORED] Id=%s Module=%s Program=[%s]"),
					*Case.GetId(),
					*RestoredModuleName,
					*RestoredBytecode);
				UE_LOG(LogTemp, Display,
					TEXT("[AS-OBJECT-METADATA] Stage=restored Id=%s Module=%s Values=[%s]"),
					*Case.GetId(),
					*RestoredModuleName,
					*RestoredObjectMetadata);
				ASSERT_THAT(AreEqual(SourceObjectMetadata, RestoredObjectMetadata,
					*Case.Describe(TEXT("counted-reference assignment round trip should rebuild the exact automatic-object metadata"))));
				if (AssignmentCase.Scenario == EReferenceAssignmentScenario::ParameterReturnSaveLoad)
				{
					asIScriptFunction* const RestoredPassThrough = GetNativeFunctionByExactDecl(
						Module,
						"FNativeCaseReference PassCountedReference(FNativeCaseReference)");
					ASSERT_THAT(IsNotNull(RestoredPassThrough,
						*Case.Describe(TEXT("counted-reference parameter-return round trip should restore its exact pass-through declaration"))));
					if (RestoredPassThrough != nullptr)
					{
						const FString RestoredPassThroughCallLayout = DescribeFunctionCallLayout(RestoredPassThrough);
						TestRunner->AddInfo(FString::Printf(
							TEXT("[%s] restored pass-through call-layout=[%s]"),
							*Case.GetId(),
							*RestoredPassThroughCallLayout));
						ASSERT_THAT(AreEqual(SourcePassThroughCallLayout, RestoredPassThroughCallLayout,
							*Case.Describe(TEXT("counted-reference parameter-return round trip should rebuild its exact call layout"))));
					}
				}
			}

			asIScriptContext* const Context = ScriptEngine->CreateContext();
			ASSERT_THAT(IsNotNull(Context,
				*Case.Describe(TEXT("counted-reference assignment should create an execution context"))));
			if (Context == nullptr)
			{
				ScriptEngine->DiscardModule(Module->GetName());
				continue;
			}

			const int ExecuteResult = PrepareAndExecute(Context, Entry);
			ASSERT_THAT(AreEqual(
				AssignmentCase.bExpectException
					? static_cast<int32>(asEXECUTION_EXCEPTION)
					: static_cast<int32>(asEXECUTION_FINISHED),
				ExecuteResult,
				*Case.Describe(TEXT("counted-reference assignment should finish or fault through its designated ownership path"))));
			if (!AssignmentCase.bExpectException && ExecuteResult == asEXECUTION_FINISHED)
			{
				ASSERT_THAT(AreEqual(AssignmentCase.ExpectedReturnValue,
					static_cast<int32>(Context->GetReturnDWord()),
					*Case.Describe(TEXT("counted-reference assignment should preserve the selected result identity"))));
			}
			if (AssignmentCase.bExpectException)
			{
				ASSERT_THAT(IsNotNull(Context->GetExceptionFunction(),
					*Case.Describe(TEXT("counted-reference exception cleanup should retain an exception owner before unprepare"))));
			}

			ASSERT_THAT(AreEqual(static_cast<int32>(asSUCCESS), Context->Unprepare(),
				*Case.Describe(TEXT("counted-reference assignment context should unprepare after its ownership transition"))));
			const FString LifecycleEntries = CollectNativeLifecycleEntries(Lifecycle);
			TestRunner->AddInfo(FString::Printf(
				TEXT("[%s] lifecycle expected={construct=%d addref=%d release=%d destruct=%d live=0} actual={construct=%d addref=%d release=%d destruct=%d live=%d} entries=[%s]"),
				*Case.GetId(),
				AssignmentCase.ExpectedConstructionCount,
				AssignmentCase.ExpectedAddRefCount,
				AssignmentCase.ExpectedReleaseCount,
				AssignmentCase.ExpectedConstructionCount,
				Lifecycle.Num(ENativeLifecycleEvent::ValueConstruct),
				Lifecycle.Num(ENativeLifecycleEvent::AddRef),
				Lifecycle.Num(ENativeLifecycleEvent::Release),
				Lifecycle.Num(ENativeLifecycleEvent::Destruct),
				Lifecycle.GetLiveObjectCount(),
				*LifecycleEntries));
			ASSERT_THAT(AreEqual(AssignmentCase.ExpectedConstructionCount,
				Lifecycle.Num(ENativeLifecycleEvent::ValueConstruct),
				*Case.Describe(TEXT("counted-reference assignment should construct each factory owner exactly once"))));
			ASSERT_THAT(AreEqual(AssignmentCase.ExpectedAddRefCount,
				Lifecycle.Num(ENativeLifecycleEvent::AddRef),
				*Case.Describe(TEXT("counted-reference assignment should retain every newly owned alias exactly once"))));
			ASSERT_THAT(AreEqual(AssignmentCase.ExpectedReleaseCount,
				Lifecycle.Num(ENativeLifecycleEvent::Release),
				*Case.Describe(TEXT("counted-reference assignment should release factory, overwritten, parameter, and local ownership exactly once"))));
			ASSERT_THAT(AreEqual(AssignmentCase.ExpectedConstructionCount,
				Lifecycle.Num(ENativeLifecycleEvent::Destruct),
				*Case.Describe(TEXT("counted-reference assignment should destroy every factory owner exactly once"))));
			ASSERT_THAT(AreEqual(0, Lifecycle.GetLiveObjectCount(),
				*Case.Describe(TEXT("counted-reference assignment should leave no native owner live after cleanup"))));

			asIScriptFunction* const Recovery = GetNativeFunctionByExactDecl(
				Module,
				"int RecoverCountedReferenceAssignment()");
			ASSERT_THAT(IsNotNull(Recovery,
				*Case.Describe(TEXT("counted-reference assignment module should expose a recovery declaration"))));
			if (Recovery != nullptr)
			{
				ASSERT_THAT(AreEqual(static_cast<int32>(asSUCCESS), Context->Prepare(Recovery),
					*Case.Describe(TEXT("counted-reference assignment should reuse its unprepared context"))));
				ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_FINISHED), Context->Execute(),
					*Case.Describe(TEXT("counted-reference assignment recovery should execute after cleanup"))));
				ASSERT_THAT(AreEqual(89, static_cast<int32>(Context->GetReturnDWord()),
					*Case.Describe(TEXT("counted-reference assignment recovery should return its independent result"))));
				ASSERT_THAT(AreEqual(static_cast<int32>(asSUCCESS), Context->Unprepare(),
					*Case.Describe(TEXT("counted-reference assignment recovery should unprepare cleanly"))));
			}
			Context->Release();
			ScriptEngine->DiscardModule(Module->GetName());
		}
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
