#include "../../Support/AngelscriptNativeCaseTestSupport.h"
#include "../../Support/AngelscriptNativeCoreTestSupport.h"
#include "../../Support/AngelscriptNativeFixtureTestSupport.h"
#include "../../Support/AngelscriptNativeLanguageCaseTestSupport.h"

#include "CQTest.h"
#include "Misc/ScopeExit.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_scriptfunction.h"
#include "EndAngelscriptHeaders.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FVariableLifetimeTests,
	"Angelscript.TestModule.AngelScriptSDK.Language.Variables.Lifetime",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	struct FOwnerCase
	{
		const ANSICHAR* CatalogName;
		int32 Seed;
		AngelscriptNativeTestSupport::ENativeLifecycleEvent ConstructionEvent;
		bool bReference;
	};

	struct FNestingCase
	{
		const ANSICHAR* CatalogName;
	};

	struct FExitCase
	{
		const ANSICHAR* CatalogName;
		int32 ReturnValue;
		bool bException;
	};

	enum class ELifetimeExpectation : uint8
	{
		ScopedDestruction,
		CurrentForkRawClassRetention,
	};

	inline static constexpr FOwnerCase OwnerCases[] =
	{
		{ "local_value", 110, AngelscriptNativeTestSupport::ENativeLifecycleEvent::ValueConstruct, false },
		{ "nested_value", 210, AngelscriptNativeTestSupport::ENativeLifecycleEvent::DefaultConstruct, false },
		{ "field", 310, AngelscriptNativeTestSupport::ENativeLifecycleEvent::DefaultConstruct, false },
		{ "reference", 410, AngelscriptNativeTestSupport::ENativeLifecycleEvent::ValueConstruct, true },
	};

	inline static constexpr FNestingCase NestingCases[] =
	{
		{ "one" },
		{ "sequential" },
		{ "nested_scopes" },
		{ "loop" },
		{ "nested_call" },
	};

	inline static constexpr FExitCase ExitCases[] =
	{
		{ "block_end", 71, false },
		{ "return", 72, false },
		{ "break", 73, false },
		{ "continue", 74, false },
		{ "exception", 0, true },
	};

	static bool IsOwner(const FOwnerCase& OwnerCase, const ANSICHAR* Name)
	{
		return FCStringAnsi::Strcmp(OwnerCase.CatalogName, Name) == 0;
	}

	static bool IsNesting(const FNestingCase& NestingCase, const ANSICHAR* Name)
	{
		return FCStringAnsi::Strcmp(NestingCase.CatalogName, Name) == 0;
	}

	static bool IsExit(const FExitCase& ExitCase, const ANSICHAR* Name)
	{
		return FCStringAnsi::Strcmp(ExitCase.CatalogName, Name) == 0;
	}

	static FString MakeSuffix(
		const FExitCase& ExitCase,
		const FNestingCase& NestingCase,
		const FOwnerCase& OwnerCase)
	{
		return FString::Printf(
			TEXT("%hs_%hs_%hs"),
			ExitCase.CatalogName,
			NestingCase.CatalogName,
			OwnerCase.CatalogName);
	}

	static FString MakeOwnerVariableName(const FOwnerCase& OwnerCase, const int32 Ordinal)
	{
		return FString::Printf(
			TEXT("%hsOwner%d"),
			OwnerCase.CatalogName,
			Ordinal);
	}

	static void AppendOwnerType(FString& Source, const FOwnerCase& OwnerCase)
	{
		using namespace AngelscriptNativeTestSupport;

		if (IsOwner(OwnerCase, "nested_value"))
		{
			AppendGeneratedAsLine(Source, TEXT("struct FNestedLifetimeValue"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\tFNativeCaseValue Tracked;"));
			AppendGeneratedAsLine(Source);
			AppendGeneratedAsLine(Source, TEXT("\tFNestedLifetimeValue()"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
			AppendGeneratedAsLine(Source, TEXT("}"));
			AppendGeneratedAsLine(Source);
		}
		else if (IsOwner(OwnerCase, "field"))
		{
			AppendGeneratedAsLine(Source, TEXT("class FFieldLifetimeOwner"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\tFNativeCaseValue Tracked;"));
			AppendGeneratedAsLine(Source);
			AppendGeneratedAsLine(Source, TEXT("\tFFieldLifetimeOwner()"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
			AppendGeneratedAsLine(Source, TEXT("}"));
			AppendGeneratedAsLine(Source);
		}
	}

	static void AppendExceptionHelper(FString& Source)
	{
		using namespace AngelscriptNativeTestSupport;

		AppendGeneratedAsLine(Source, TEXT("int RaiseVariableLifetimeException()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tint Zero = 0;"));
		AppendGeneratedAsLine(Source, TEXT("\treturn 1 / Zero;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
	}

	static void AppendOwnerDeclaration(
		FString& Source,
		const FOwnerCase& OwnerCase,
		const int32 Ordinal,
		const FString& Indent)
	{
		using namespace AngelscriptNativeTestSupport;

		const FString VariableName = MakeOwnerVariableName(OwnerCase, Ordinal);
		const int32 Value = OwnerCase.Seed + Ordinal;
		if (IsOwner(OwnerCase, "local_value"))
		{
			AppendGeneratedAsLine(Source, FString::Printf(
				TEXT("%sFNativeCaseValue %s(%d);"),
				*Indent,
				*VariableName,
				Value));
		}
		else if (IsOwner(OwnerCase, "nested_value"))
		{
			AppendGeneratedAsLine(Source, FString::Printf(
				TEXT("%sFNestedLifetimeValue %s;"),
				*Indent,
				*VariableName));
			AppendGeneratedAsLine(Source, FString::Printf(
				TEXT("%s%s.Tracked.Value = %d;"),
				*Indent,
				*VariableName,
				Value));
		}
		else if (IsOwner(OwnerCase, "field"))
		{
			AppendGeneratedAsLine(Source, FString::Printf(
				TEXT("%sFFieldLifetimeOwner %s = FFieldLifetimeOwner();"),
				*Indent,
				*VariableName));
			AppendGeneratedAsLine(Source, FString::Printf(
				TEXT("%s%s.Tracked.Value = %d;"),
				*Indent,
				*VariableName,
				Value));
		}
		else
		{
			AppendGeneratedAsLine(Source, FString::Printf(
				TEXT("%sFNativeCaseReference %s = CreateNativeCaseReference(%d);"),
				*Indent,
				*VariableName,
				Value));
		}
	}

	static void AppendOwnerObservation(
		FString& Source,
		const FOwnerCase& OwnerCase,
		const int32 Ordinal,
		const FString& Indent)
	{
		using namespace AngelscriptNativeTestSupport;

		const FString VariableName = MakeOwnerVariableName(OwnerCase, Ordinal);
		const TCHAR* AccessSuffix = IsOwner(OwnerCase, "local_value")
			|| IsOwner(OwnerCase, "reference")
			? TEXT(".Value")
			: TEXT(".Tracked.Value");
		AppendGeneratedAsLine(Source, FString::Printf(
			TEXT("%sTrace += %s%s;"),
			*Indent,
			*VariableName,
			AccessSuffix));
	}

	static void AppendOwnerAndObservation(
		FString& Source,
		const FOwnerCase& OwnerCase,
		const int32 Ordinal,
		const FString& Indent)
	{
		AppendOwnerDeclaration(Source, OwnerCase, Ordinal, Indent);
		AppendOwnerObservation(Source, OwnerCase, Ordinal, Indent);
	}

	static void AppendTransfer(
		FString& Source,
		const FExitCase& ExitCase,
		const FString& Indent)
	{
		using namespace AngelscriptNativeTestSupport;

		if (IsExit(ExitCase, "return"))
		{
			AppendGeneratedAsLine(Source, FString::Printf(
				TEXT("%sreturn %d + Trace - Trace;"),
				*Indent,
				ExitCase.ReturnValue));
		}
		else if (IsExit(ExitCase, "break"))
		{
			AppendGeneratedAsLine(Source, Indent + TEXT("break;"));
		}
		else if (IsExit(ExitCase, "continue"))
		{
			AppendGeneratedAsLine(Source, Indent + TEXT("continue;"));
		}
		else if (IsExit(ExitCase, "exception"))
		{
			AppendGeneratedAsLine(Source, Indent + TEXT("Trace += RaiseVariableLifetimeException();"));
		}
	}

	static void AppendSingleScopeBody(
		FString& Source,
		const FOwnerCase& OwnerCase,
		const FExitCase& ExitCase)
	{
		using namespace AngelscriptNativeTestSupport;

		AppendGeneratedAsLine(Source, TEXT("\tfor (int TransferLoop = 0; TransferLoop < 1; ++TransferLoop)"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendOwnerAndObservation(Source, OwnerCase, 0, TEXT("\t\t"));
		AppendTransfer(Source, ExitCase, TEXT("\t\t"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
	}

	static void AppendSequentialScopeBody(
		FString& Source,
		const FOwnerCase& OwnerCase,
		const FExitCase& ExitCase)
	{
		using namespace AngelscriptNativeTestSupport;

		AppendGeneratedAsLine(Source, TEXT("\tfor (int TransferLoop = 0; TransferLoop < 1; ++TransferLoop)"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendOwnerAndObservation(Source, OwnerCase, 0, TEXT("\t\t"));
		AppendOwnerAndObservation(Source, OwnerCase, 1, TEXT("\t\t"));
		AppendTransfer(Source, ExitCase, TEXT("\t\t"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
	}

	static void AppendNestedScopeBody(
		FString& Source,
		const FOwnerCase& OwnerCase,
		const FExitCase& ExitCase)
	{
		using namespace AngelscriptNativeTestSupport;

		AppendGeneratedAsLine(Source, TEXT("\tfor (int TransferLoop = 0; TransferLoop < 1; ++TransferLoop)"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendOwnerAndObservation(Source, OwnerCase, 0, TEXT("\t\t"));
		AppendGeneratedAsLine(Source, TEXT("\t\t{"));
		AppendOwnerAndObservation(Source, OwnerCase, 1, TEXT("\t\t\t"));
		AppendTransfer(Source, ExitCase, TEXT("\t\t\t"));
		AppendGeneratedAsLine(Source, TEXT("\t\t}"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
	}

	static void AppendLoopScopeBody(
		FString& Source,
		const FOwnerCase& OwnerCase,
		const FExitCase& ExitCase)
	{
		using namespace AngelscriptNativeTestSupport;

		AppendGeneratedAsLine(Source, TEXT("\tfor (int OwnerIteration = 0; OwnerIteration < 2; ++OwnerIteration)"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendOwnerAndObservation(Source, OwnerCase, 0, TEXT("\t\t"));
		AppendTransfer(Source, ExitCase, TEXT("\t\t"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
	}

	static void AppendNestedCallFunction(
		FString& Source,
		const FOwnerCase& OwnerCase,
		const FExitCase& ExitCase)
	{
		using namespace AngelscriptNativeTestSupport;

		AppendGeneratedAsLine(Source, TEXT("int RunVariableLifetimeNestedCall()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tint Trace = 0;"));
		AppendGeneratedAsLine(Source, TEXT("\tfor (int TransferLoop = 0; TransferLoop < 1; ++TransferLoop)"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendOwnerAndObservation(Source, OwnerCase, 1, TEXT("\t\t"));
		AppendTransfer(Source, ExitCase, TEXT("\t\t"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source, FString::Printf(
			TEXT("\treturn %d + Trace - Trace;"),
			ExitCase.ReturnValue));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
	}

	static void AppendEntryFunction(
		FString& Source,
		const FOwnerCase& OwnerCase,
		const FNestingCase& NestingCase,
		const FExitCase& ExitCase)
	{
		using namespace AngelscriptNativeTestSupport;

		if (IsNesting(NestingCase, "nested_call"))
		{
			AppendNestedCallFunction(Source, OwnerCase, ExitCase);
		}

		AppendGeneratedAsLine(Source, TEXT("int RunVariableLifetime()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tint Trace = 0;"));
		if (IsNesting(NestingCase, "one"))
		{
			AppendSingleScopeBody(Source, OwnerCase, ExitCase);
		}
		else if (IsNesting(NestingCase, "sequential"))
		{
			AppendSequentialScopeBody(Source, OwnerCase, ExitCase);
		}
		else if (IsNesting(NestingCase, "nested_scopes"))
		{
			AppendNestedScopeBody(Source, OwnerCase, ExitCase);
		}
		else if (IsNesting(NestingCase, "loop"))
		{
			AppendLoopScopeBody(Source, OwnerCase, ExitCase);
		}
		else
		{
			AppendOwnerAndObservation(Source, OwnerCase, 0, TEXT("\t"));
			AppendGeneratedAsLine(Source, TEXT("\tint NestedResult = RunVariableLifetimeNestedCall();"));
			AppendGeneratedAsLine(Source, TEXT("\treturn NestedResult + Trace - Trace;"));
		}

		if (!IsNesting(NestingCase, "nested_call"))
		{
			AppendGeneratedAsLine(Source, FString::Printf(
				TEXT("\treturn %d + Trace - Trace;"),
				ExitCase.ReturnValue));
		}
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
	}

	static FString BuildVariableLifetimeSource(
		const FOwnerCase& OwnerCase,
		const FNestingCase& NestingCase,
		const FExitCase& ExitCase)
	{
		using namespace AngelscriptNativeTestSupport;

		FString Source;
		AppendOwnerType(Source, OwnerCase);
		if (ExitCase.bException)
		{
			AppendExceptionHelper(Source);
		}
		AppendEntryFunction(Source, OwnerCase, NestingCase, ExitCase);
		AppendGeneratedAsLine(Source, TEXT("int CleanAfterVariableLifetime()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\treturn 89;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		return Source;
	}

	static int32 ExpectedOwnerCount(
		const FNestingCase& NestingCase,
		const FExitCase& ExitCase)
	{
		if (IsNesting(NestingCase, "one"))
		{
			return 1;
		}
		if (IsNesting(NestingCase, "loop"))
		{
			return IsExit(ExitCase, "block_end") || IsExit(ExitCase, "continue") ? 2 : 1;
		}
		return 2;
	}

	static bool ShouldDestroyInConstructionOrder(
		const FNestingCase& NestingCase,
		const FExitCase& ExitCase)
	{
		if (!IsNesting(NestingCase, "loop"))
		{
			return false;
		}
		return FCStringAnsi::Strcmp(ExitCase.CatalogName, "block_end") == 0
			|| FCStringAnsi::Strcmp(ExitCase.CatalogName, "continue") == 0;
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

	void VerifyLifecycle(
		const AngelscriptNativeTestSupport::FNativeCaseContext& Case,
		const FOwnerCase& OwnerCase,
		const FNestingCase& NestingCase,
		const FExitCase& ExitCase,
		const AngelscriptNativeTestSupport::FNativeLifecycleRecorder& Lifecycle,
		const ELifetimeExpectation Expectation)
	{
		using namespace AngelscriptNativeTestSupport;

		const int32 ExpectedCount = ExpectedOwnerCount(NestingCase, ExitCase);
		const int32 ConstructionCount =
			Lifecycle.Num(ENativeLifecycleEvent::DefaultConstruct)
			+ Lifecycle.Num(ENativeLifecycleEvent::ValueConstruct)
			+ Lifecycle.Num(ENativeLifecycleEvent::CopyConstruct);
		const int32 DestructionCount = Lifecycle.Num(ENativeLifecycleEvent::Destruct);
		if (Expectation == ELifetimeExpectation::ScopedDestruction
			&& (Lifecycle.GetLiveObjectCount() != 0
				|| ConstructionCount != DestructionCount))
		{
			TestRunner->AddInfo(FString::Printf(
				TEXT("[%s] variable-lifetime lifecycle live=%d construct=%d destruct=%d entries=[%s]"),
				*Case.GetId(),
				Lifecycle.GetLiveObjectCount(),
				ConstructionCount,
				DestructionCount,
				*CollectNativeLifecycleEntries(Lifecycle)));
		}
		ASSERT_THAT(AreEqual(ExpectedCount, Lifecycle.Num(OwnerCase.ConstructionEvent),
			*Case.Describe(TEXT("owner kind should record its exact construction count"))));
		const ENativeLifecycleEvent OtherConstructionEvent = OwnerCase.ConstructionEvent == ENativeLifecycleEvent::DefaultConstruct
			? ENativeLifecycleEvent::ValueConstruct
			: ENativeLifecycleEvent::DefaultConstruct;
		ASSERT_THAT(AreEqual(0, Lifecycle.Num(OtherConstructionEvent),
			*Case.Describe(TEXT("owner kind should not use the other construction path"))));
		ASSERT_THAT(AreEqual(0, Lifecycle.Num(ENativeLifecycleEvent::CopyConstruct),
			*Case.Describe(TEXT("scope ownership should not introduce an implicit value copy"))));
		ASSERT_THAT(AreEqual(0, Lifecycle.Num(ENativeLifecycleEvent::Assign),
			*Case.Describe(TEXT("direct field mutation should not invoke value assignment"))));
		if (Expectation == ELifetimeExpectation::CurrentForkRawClassRetention)
		{
			ASSERT_THAT(AreEqual(0, Lifecycle.Num(ENativeLifecycleEvent::Destruct),
				*Case.Describe(TEXT("current fork raw script-class fields should retain their native storage"))));
			ASSERT_THAT(AreEqual(ExpectedCount, Lifecycle.GetLiveObjectCount(),
				*Case.Describe(TEXT("current fork raw script-class fields should retain every constructed owner"))));
			ASSERT_THAT(AreEqual(ExpectedCount, Lifecycle.Num(OwnerCase.ConstructionEvent),
				*Case.Describe(TEXT("current fork raw script-class retention should preserve every construction event"))));
			return;
		}
		ASSERT_THAT(AreEqual(ExpectedCount, Lifecycle.Num(ENativeLifecycleEvent::Destruct),
			*Case.Describe(TEXT("every constructed scope owner should be destroyed exactly once"))));
		ASSERT_THAT(AreEqual(0, Lifecycle.GetLiveObjectCount(),
			*Case.Describe(TEXT("scope exit should leave no tracked owner alive"))));

		TArray<int32> ConstructedIds;
		TArray<int32> DestructedIds;
		TMap<int32, int32> ConstructionEntryById;
		for (int32 EntryIndex = 0; EntryIndex < Lifecycle.GetEntries().Num(); ++EntryIndex)
		{
			const FNativeLifecycleEntry& Entry = Lifecycle.GetEntries()[EntryIndex];
			if (Entry.Event == OwnerCase.ConstructionEvent)
			{
				ASSERT_THAT(IsFalse(ConstructionEntryById.Contains(Entry.ObjectId),
					*Case.Describe(TEXT("each owner construction should allocate a unique identity"))));
				ConstructionEntryById.Add(Entry.ObjectId, EntryIndex);
				ConstructedIds.Add(Entry.ObjectId);
			}
			else if (Entry.Event == ENativeLifecycleEvent::Destruct)
			{
				ASSERT_THAT(IsTrue(ConstructionEntryById.Contains(Entry.ObjectId),
					*Case.Describe(TEXT("each destructor should identify a constructed owner"))));
				if (const int32* ConstructionIndex = ConstructionEntryById.Find(Entry.ObjectId))
				{
					ASSERT_THAT(IsTrue(*ConstructionIndex < EntryIndex,
						*Case.Describe(TEXT("each destructor should occur after its construction"))));
				}
				ASSERT_THAT(IsFalse(DestructedIds.Contains(Entry.ObjectId),
					*Case.Describe(TEXT("an owner identity should be destroyed no more than once"))));
				DestructedIds.Add(Entry.ObjectId);
			}
		}
		ASSERT_THAT(AreEqual(ExpectedCount, ConstructedIds.Num(),
			*Case.Describe(TEXT("construction trace should contain every expected owner"))));
		ASSERT_THAT(AreEqual(ExpectedCount, DestructedIds.Num(),
			*Case.Describe(TEXT("destruction trace should contain every expected owner"))));
		bool bDestructionOrderMatches = true;
		FString ExpectedDestructionOrder;
		FString ActualDestructionOrder;
		for (int32 Index = 0; Index < ExpectedCount; ++Index)
		{
			const int32 ExpectedDestructedId = ShouldDestroyInConstructionOrder(NestingCase, ExitCase)
				? ConstructedIds[Index]
				: ConstructedIds[ExpectedCount - Index - 1];
			ExpectedDestructionOrder += FString::Printf(
				TEXT("%s%d"),
				Index == 0 ? TEXT("") : TEXT(","),
				ExpectedDestructedId);
			ActualDestructionOrder += FString::Printf(
				TEXT("%s%d"),
				Index == 0 ? TEXT("") : TEXT(","),
				DestructedIds.IsValidIndex(Index) ? DestructedIds[Index] : INDEX_NONE);
			bDestructionOrderMatches &= DestructedIds.IsValidIndex(Index)
				&& ExpectedDestructedId == DestructedIds[Index];
		}
		if (!bDestructionOrderMatches)
		{
			TestRunner->AddInfo(FString::Printf(
				TEXT("[%s] destruction-order expected=[%s] actual=[%s] entries=[%s]"),
				*Case.GetId(),
				*ExpectedDestructionOrder,
				*ActualDestructionOrder,
				*CollectNativeLifecycleEntries(Lifecycle)));
		}
		for (int32 Index = 0; Index < ExpectedCount; ++Index)
		{
			const int32 ExpectedDestructedId = ShouldDestroyInConstructionOrder(NestingCase, ExitCase)
				? ConstructedIds[Index]
				: ConstructedIds[ExpectedCount - Index - 1];
			ASSERT_THAT(AreEqual(ExpectedDestructedId, DestructedIds[Index],
				*Case.Describe(TEXT("destruction order should match sequential-iteration or reverse-scope ownership"))));
		}

		if (OwnerCase.bReference)
		{
			ASSERT_THAT(AreEqual(ExpectedCount, Lifecycle.Num(ENativeLifecycleEvent::AddRef),
				*Case.Describe(TEXT("each counted native-reference assignment should retain the local owner"))));
			ASSERT_THAT(AreEqual(ExpectedCount * 2, Lifecycle.Num(ENativeLifecycleEvent::Release),
				*Case.Describe(TEXT("each counted native-reference assignment should release its temporary and local owners"))));
		}
		else
		{
			ASSERT_THAT(AreEqual(0, Lifecycle.Num(ENativeLifecycleEvent::AddRef),
				*Case.Describe(TEXT("value owners should not produce reference-retain events"))));
			ASSERT_THAT(AreEqual(0, Lifecycle.Num(ENativeLifecycleEvent::Release),
				*Case.Describe(TEXT("value owners should not produce reference-release events"))));
		}
	}

	void ExecuteCell(
		const AngelscriptNativeTestSupport::FNativeCaseContext& Case,
		const FExitCase& ExitCase,
		const FOwnerCase& OwnerCase,
		const FNestingCase& NestingCase,
		asIScriptEngine& ScriptEngine,
		asIScriptModule& Module,
		AngelscriptNativeTestSupport::FNativeLifecycleRecorder& Lifecycle,
		const FString& ModuleName,
		const ELifetimeExpectation Expectation)
	{
		using namespace AngelscriptNativeTestSupport;

		asIScriptFunction* const Entry = GetNativeFunctionByExactDecl(&Module, "int RunVariableLifetime()");
		asIScriptFunction* const Recovery = GetNativeFunctionByExactDecl(&Module, "int CleanAfterVariableLifetime()");
		ASSERT_THAT(IsNotNull(Entry,
			*Case.Describe(TEXT("variable-lifetime module should expose its exact entry declaration"))));
		ASSERT_THAT(IsNotNull(Recovery,
			*Case.Describe(TEXT("variable-lifetime module should expose its recovery declaration"))));
		if (Entry == nullptr || Recovery == nullptr)
		{
			return;
		}

		const bool bReferenceDiagnosticCell = OwnerCase.bReference
			&& IsNesting(NestingCase, "one")
			&& IsExit(ExitCase, "block_end");
		if (bReferenceDiagnosticCell)
		{
			FString ReferenceRegistration;
			const bool bReferenceRegistrationMatchesCleanupContract =
				DescribeNativeReferenceRegistration(ScriptEngine, ReferenceRegistration);
			const FString BytecodeDescription = DescribeBytecode(Entry);
			TestRunner->AddInfo(FString::Printf(
				TEXT("[%s] native-reference cleanup registration=%s bytecode=[%s]"),
				*Case.GetId(),
				*ReferenceRegistration,
				*BytecodeDescription));
			ASSERT_THAT(IsTrue(bReferenceRegistrationMatchesCleanupContract,
				*Case.Describe(TEXT("native reference registration should expose counted implicit-handle addref/release metadata without type user data"))));
			ASSERT_THAT(IsTrue(ContainsBytecodeOpcode(Entry, asBC_FREE),
				*Case.Describe(TEXT("native reference local should emit a free instruction for scope cleanup"))));
		}

		asIScriptContext* const Context = ScriptEngine.CreateContext();
		ASSERT_THAT(IsNotNull(Context,
			*Case.Describe(TEXT("variable-lifetime cell should create an execution context"))));
		if (Context == nullptr)
		{
			return;
		}

		const int32 ExecuteResult = PrepareAndExecute(Context, Entry);
		if (ExitCase.bException)
		{
			ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_EXCEPTION), ExecuteResult,
				*Case.Describe(TEXT("exception exit should stop the lifetime entry"))));
			ASSERT_THAT(IsTrue(Context->GetExceptionString() != nullptr && Context->GetExceptionString()[0] != '\0',
				*Case.Describe(TEXT("exception exit should retain non-empty exception text"))));
			ASSERT_THAT(IsNotNull(Context->GetExceptionFunction(),
				*Case.Describe(TEXT("exception exit should identify its throwing function"))));
			ASSERT_THAT(IsTrue(Context->GetCallstackSize() > 0,
				*Case.Describe(TEXT("exception exit should retain its script call stack before cleanup"))));
			const char* ExceptionSection = nullptr;
			int ExceptionColumn = 0;
			ASSERT_THAT(IsTrue(Context->GetExceptionLineNumber(&ExceptionColumn, &ExceptionSection) > 0,
				*Case.Describe(TEXT("exception exit should retain a one-based source line"))));
			ASSERT_THAT(IsTrue(ExceptionColumn > 0,
				*Case.Describe(TEXT("exception exit should retain a one-based source column"))));
			ASSERT_THAT(AreEqual(ModuleName, FString(UTF8_TO_TCHAR(ExceptionSection != nullptr ? ExceptionSection : "")),
				*Case.Describe(TEXT("exception exit should retain the generated module section"))));
		}
		else
		{
			if (ExecuteResult != asEXECUTION_FINISHED)
			{
				const char* const ExceptionText = Context->GetExceptionString();
				const asIScriptFunction* const ExceptionFunction =
					Context->GetExceptionFunction();
				const char* ExceptionSection = nullptr;
				int ExceptionColumn = 0;
				const int ExceptionLine = Context->GetExceptionLineNumber(
					&ExceptionColumn,
					&ExceptionSection);
				TestRunner->AddInfo(FString::Printf(
					TEXT("[%s] lifetime entry returned %d; exception='%s' function='%s' location=%s:%d:%d"),
					*Case.GetId(),
					ExecuteResult,
					UTF8_TO_TCHAR(ExceptionText != nullptr ? ExceptionText : ""),
					ExceptionFunction != nullptr
						? UTF8_TO_TCHAR(ExceptionFunction->GetDeclaration())
						: TEXT("<none>"),
					UTF8_TO_TCHAR(ExceptionSection != nullptr ? ExceptionSection : ""),
					ExceptionLine,
					ExceptionColumn));
			}
			ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_FINISHED), ExecuteResult,
				*Case.Describe(TEXT("non-exception lifetime exit should finish"))));
			if (ExecuteResult == asEXECUTION_FINISHED)
			{
				ASSERT_THAT(AreEqual(ExitCase.ReturnValue, static_cast<int32>(Context->GetReturnDWord()),
					*Case.Describe(TEXT("lifetime entry should report the reached transfer path"))));
			}
		}

		ASSERT_THAT(AreEqual(asSUCCESS, Context->Unprepare(),
			*Case.Describe(TEXT("variable-lifetime context should unprepare after its exit"))));
		VerifyLifecycle(Case, OwnerCase, NestingCase, ExitCase, Lifecycle, Expectation);
		const int32 LifecycleEntryCount = Lifecycle.GetEntries().Num();
		ASSERT_THAT(AreEqual(asSUCCESS, Context->Prepare(Recovery),
			*Case.Describe(TEXT("clean recovery should prepare on the same context"))));
		ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_FINISHED), Context->Execute(),
			*Case.Describe(TEXT("clean recovery should execute after lifetime cleanup"))));
		ASSERT_THAT(AreEqual(89, static_cast<int32>(Context->GetReturnDWord()),
			*Case.Describe(TEXT("reused context should not retain stale transfer state"))));
		ASSERT_THAT(AreEqual(LifecycleEntryCount, Lifecycle.GetEntries().Num(),
			*Case.Describe(TEXT("clean recovery should not create hidden lifetime events"))));
		Context->Release();
	}

public:
	TEST_METHOD(OwnersByExitAndNesting)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT("LANG-VAR-LIFETIME",
			ENativeEvidence::Runtime
			| ENativeEvidence::Lifecycle
			| ENativeEvidence::Cleanup);

		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };
		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine,
			TEXT("Variable-lifetime product should create a raw SDK engine")));
		if (ScriptEngine == nullptr)
		{
			return;
		}

		FNativeLifecycleRecorder Lifecycle;
		Lifecycle.Reset();
		ASSERT_THAT(IsTrue(RegisterNativeCaseValue(*ScriptEngine, Lifecycle),
			TEXT("Variable-lifetime product should register its tracked value type")));
		ASSERT_THAT(IsTrue(RegisterNativeCaseReference(*ScriptEngine, &Lifecycle),
			TEXT("Variable-lifetime product should register its tracked reference type")));

		for (const FExitCase& ExitCase : ExitCases)
		{
			for (const FNestingCase& NestingCase : NestingCases)
			{
				for (const FOwnerCase& OwnerCase : OwnerCases)
				{
					Lifecycle.Reset();
					const FNativeCaseContext Case(MakeNativeCaseId(
						"LANG-VAR-LIFETIME",
						{
							ANSI_TO_TCHAR(ExitCase.CatalogName),
							ANSI_TO_TCHAR(NestingCase.CatalogName),
							ANSI_TO_TCHAR(OwnerCase.CatalogName),
						}));
					const FString Suffix = MakeSuffix(ExitCase, NestingCase, OwnerCase);
					const FString ModuleName = TEXT("VariableLifetime_") + Suffix;
					const FString Source = BuildVariableLifetimeSource(OwnerCase, NestingCase, ExitCase);
					PrintGeneratedAsSource(*TestRunner, Case.GetId(), ModuleName, Source);
					const FTCHARToUTF8 ModuleNameUtf8(*ModuleName);
					const FTCHARToUTF8 SourceUtf8(*Source);
					Engine.ResetMessages();
					asIScriptModule* Module = nullptr;
					const int BuildResult = CompileNativeModule(
						ScriptEngine,
						ModuleNameUtf8.Get(),
						SourceUtf8.Get(),
						Module);
					ASSERT_THAT(IsTrue(BuildResult >= 0,
						*Case.Describe(TEXT("variable-lifetime cell should compile"))));
					ASSERT_THAT(IsNotNull(Module,
						*Case.Describe(TEXT("variable-lifetime cell should publish a module"))));
					if (BuildResult >= 0 && Module != nullptr)
					{
						ExecuteCell(
							Case,
							ExitCase,
							OwnerCase,
							NestingCase,
							*ScriptEngine,
							*Module,
							Lifecycle,
							ModuleName,
							ELifetimeExpectation::ScopedDestruction);
					}

					ScriptEngine->DiscardModule(ModuleNameUtf8.Get());
					ASSERT_THAT(IsNull(ScriptEngine->GetModule(ModuleNameUtf8.Get(), asGM_ONLY_IF_EXISTS),
						*Case.Describe(TEXT("variable-lifetime cell should discard its isolated module"))));
					const int32 ExpectedLiveCount = 0;
					ASSERT_THAT(AreEqual(ExpectedLiveCount, Lifecycle.GetLiveObjectCount(),
						*Case.Describe(TEXT("module discard should leave no tracked owner alive"))));
				}
			}
		}
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
