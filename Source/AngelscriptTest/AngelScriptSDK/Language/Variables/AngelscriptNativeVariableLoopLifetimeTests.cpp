#include "../../Support/AngelscriptNativeCaseTestSupport.h"
#include "../../Support/AngelscriptNativeCoreTestSupport.h"
#include "../../Support/AngelscriptNativeDebugTestSupport.h"
#include "../../Support/AngelscriptNativeFixtureTestSupport.h"
#include "../../Support/AngelscriptNativeLanguageCaseTestSupport.h"

#include "CQTest.h"
#include "Misc/ScopeExit.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FVariableLoopLifetimeTests,
	"Angelscript.TestModule.AngelScriptSDK.Language.Variables.LoopLifetime",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	using FNativeCaseContext = AngelscriptNativeTestSupport::FNativeCaseContext;
	using FNativeLifecycleRecorder = AngelscriptNativeTestSupport::FNativeLifecycleRecorder;
	using ENativeLifecycleEvent = AngelscriptNativeTestSupport::ENativeLifecycleEvent;

	struct FTypeCase
	{
		const ANSICHAR* CatalogName;
		const ANSICHAR* ScriptType;
		bool bScript;
	};

	struct FPlacementCase
	{
		const ANSICHAR* CatalogName;
	};

	struct FIterationCase
	{
		const ANSICHAR* CatalogName;
		int32 Count;
	};

	struct FExitCase
	{
		const ANSICHAR* CatalogName;
		int32 EventBase;
		int32 ReturnBase;
	};

	struct FLoopObservation
	{
		TArray<int32> Events;

		void Reset()
		{
			Events.Reset();
		}
	};

	struct FLoopDebugObservation
	{
		int32 LineCallbacks = 0;
		int32 VisibleOwnerSamples = 0;
		int32 OwnerAddressSamples = 0;
	};

	inline static constexpr FTypeCase TypeCases[] =
	{
		{ "script_value", "FScriptLoopValue", true },
		{ "native_value", "FNativeCaseValue", false },
	};

	inline static constexpr FPlacementCase PlacementCases[] =
	{
		{ "for_initializer" },
		{ "for_body" },
		{ "while_body" },
		{ "do_body" },
		{ "nested_body" },
	};

	inline static constexpr FIterationCase IterationCases[] =
	{
		{ "zero", 0 },
		{ "one", 1 },
		{ "three", 3 },
		{ "eight", 8 },
	};

	inline static constexpr FExitCase ExitCases[] =
	{
		{ "normal", 3000, 7000 },
		{ "break", 3100, 7100 },
		{ "continue", 3200, 7200 },
		{ "return", 3300, 7300 },
		{ "exception", 3400, 7400 },
	};

	inline static constexpr asPWORD LoopObservationUserDataSlot = static_cast<asPWORD>(0x5641524C4F4F504Full);
	inline static constexpr asPWORD LoopDebugObservationUserDataSlot = static_cast<asPWORD>(0x5641524C4442474Full);

	static bool IsPlacement(const FPlacementCase& PlacementCase, const ANSICHAR* Name)
	{
		return FCStringAnsi::Strcmp(PlacementCase.CatalogName, Name) == 0;
	}

	static bool IsExit(const FExitCase& ExitCase, const ANSICHAR* Name)
	{
		return FCStringAnsi::Strcmp(ExitCase.CatalogName, Name) == 0;
	}

	static FString MakeSuffix(
		const FExitCase& ExitCase,
		const FIterationCase& IterationCase,
		const FPlacementCase& PlacementCase,
		const FTypeCase& TypeCase)
	{
		return FString::Printf(
			TEXT("%hs_%hs_%hs_%hs"),
			ExitCase.CatalogName,
			IterationCase.CatalogName,
			PlacementCase.CatalogName,
			TypeCase.CatalogName);
	}

	static int32 BeginScriptLoopValue(const int32 Value)
	{
		FNativeLifecycleRecorder* const Recorder = AngelscriptNativeTestSupport::GetActiveNativeLifecycleRecorder();
		if (Recorder == nullptr)
		{
			return INDEX_NONE;
		}
		const int32 ObjectId = Recorder->AllocateObjectId();
		Recorder->Record(ENativeLifecycleEvent::ValueConstruct, ObjectId, INDEX_NONE, Value);
		return ObjectId;
	}

	static void EndScriptLoopValue(const int32 ObjectId, const int32 Value)
	{
		if (FNativeLifecycleRecorder* const Recorder = AngelscriptNativeTestSupport::GetActiveNativeLifecycleRecorder())
		{
			Recorder->Record(ENativeLifecycleEvent::Destruct, ObjectId, INDEX_NONE, Value);
		}
	}

	static FLoopObservation* GetActiveLoopObservation()
	{
		asIScriptContext* const Context = asGetActiveContext();
		return Context != nullptr && Context->GetEngine() != nullptr
			? static_cast<FLoopObservation*>(Context->GetEngine()->GetUserData(LoopObservationUserDataSlot))
			: nullptr;
	}

	static void RecordVariableLoopEvent(const int32 Event)
	{
		if (FLoopObservation* const Observation = GetActiveLoopObservation())
		{
			Observation->Events.Add(Event);
		}
	}

	static int32 AdvanceVariableLoop(const int32 Index)
	{
		RecordVariableLoopEvent(4000 + Index);
		return Index + 1;
	}

	static void CaptureVariableLoopLine(asCContext* Context)
	{
		if (Context == nullptr)
		{
			return;
		}
		FLoopDebugObservation* const Observation = static_cast<FLoopDebugObservation*>(
			Context->GetUserData(LoopDebugObservationUserDataSlot));
		if (Observation == nullptr)
		{
			return;
		}

		++Observation->LineCallbacks;
		asIScriptFunction* const Function = Context->GetFunction(0);
		if (Function == nullptr)
		{
			return;
		}
		for (asUINT Index = 0; Index < Function->GetVarCount(); ++Index)
		{
			const char* Name = nullptr;
			if (Function->GetVar(Index, &Name) >= 0
				&& Name != nullptr
				&& FCStringAnsi::Strcmp(Name, "Owner") == 0
				&& Context->IsVarInScope(Index, 0))
			{
				++Observation->VisibleOwnerSamples;
				if (Context->GetAddressOfVar(Index, 0) != nullptr)
				{
					++Observation->OwnerAddressSamples;
				}
			}
		}
	}

	static bool RegisterLoopSupport(
		asIScriptEngine& Engine,
		FNativeLifecycleRecorder& Lifecycle,
		FLoopObservation& Observation)
	{
		Engine.SetUserData(&Lifecycle, AngelscriptNativeTestSupport::NativeLifecycleRecorderUserDataSlot);
		Engine.SetUserData(&Observation, LoopObservationUserDataSlot);
		const ASAutoCaller::FunctionCaller BeginCaller = ASAutoCaller::MakeFunctionCaller(BeginScriptLoopValue);
		const ASAutoCaller::FunctionCaller EndCaller = ASAutoCaller::MakeFunctionCaller(EndScriptLoopValue);
		const ASAutoCaller::FunctionCaller EventCaller = ASAutoCaller::MakeFunctionCaller(RecordVariableLoopEvent);
		const ASAutoCaller::FunctionCaller AdvanceCaller = ASAutoCaller::MakeFunctionCaller(AdvanceVariableLoop);
		return Engine.RegisterGlobalFunction(
			"int BeginScriptLoopValue(int Value)",
			asFUNCTION(BeginScriptLoopValue),
			asCALL_CDECL,
			*(asFunctionCaller*)&BeginCaller) >= 0
			&& Engine.RegisterGlobalFunction(
				"void EndScriptLoopValue(int ObjectId, int Value)",
				asFUNCTION(EndScriptLoopValue),
				asCALL_CDECL,
				*(asFunctionCaller*)&EndCaller) >= 0
			&& Engine.RegisterGlobalFunction(
				"void RecordVariableLoopEvent(int Event)",
				asFUNCTION(RecordVariableLoopEvent),
				asCALL_CDECL,
				*(asFunctionCaller*)&EventCaller) >= 0
			&& Engine.RegisterGlobalFunction(
				"int AdvanceVariableLoop(int Index)",
				asFUNCTION(AdvanceVariableLoop),
				asCALL_CDECL,
				*(asFunctionCaller*)&AdvanceCaller) >= 0;
	}

	static void AppendScriptValueType(FString& Source, const FTypeCase& TypeCase)
	{
		using namespace AngelscriptNativeTestSupport;

		if (!TypeCase.bScript)
		{
			return;
		}
		AppendGeneratedAsLine(Source, TEXT("struct FScriptLoopValue"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tint Value = 0;"));
		AppendGeneratedAsLine(Source, TEXT("\tint ObjectId = -1;"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("\tFScriptLoopValue(int InValue)"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(Source, TEXT("\t\tValue = InValue;"));
		AppendGeneratedAsLine(Source, TEXT("\t\tObjectId = BeginScriptLoopValue(Value);"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("\t~FScriptLoopValue()"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(Source, TEXT("\t\tEndScriptLoopValue(ObjectId, Value);"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
	}

	static void AppendExceptionHelper(FString& Source)
	{
		using namespace AngelscriptNativeTestSupport;

		AppendGeneratedAsLine(Source, TEXT("int RaiseVariableLoopException()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tint Zero = 0;"));
		AppendGeneratedAsLine(Source, TEXT("\treturn 1 / Zero;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
	}

	static void AppendOwnerDeclaration(
		FString& Source,
		const FTypeCase& TypeCase,
		const FString& Indent,
		const FString& ValueExpression)
	{
		using namespace AngelscriptNativeTestSupport;

		AppendGeneratedAsLine(Source, FString::Printf(
			TEXT("%s%hs Owner(100 + %s);"),
			*Indent,
			TypeCase.ScriptType,
			*ValueExpression));
	}

	static void AppendBodyPrelude(
		FString& Source,
		const FTypeCase& TypeCase,
		const FPlacementCase& PlacementCase,
		const FString& Indent)
	{
		using namespace AngelscriptNativeTestSupport;

		AppendGeneratedAsLine(Source, Indent + TEXT("++BodyCount;"));
		AppendGeneratedAsLine(Source, Indent + TEXT("RecordVariableLoopEvent(1000 + Index);"));
		if (!IsPlacement(PlacementCase, "for_initializer"))
		{
			AppendOwnerDeclaration(Source, TypeCase, Indent, TEXT("Index"));
		}
		AppendGeneratedAsLine(Source, Indent + TEXT("BodyCount += Owner.Value - Owner.Value;"));
	}

	static bool UsesManualIncrement(const FPlacementCase& PlacementCase)
	{
		return IsPlacement(PlacementCase, "while_body")
			|| IsPlacement(PlacementCase, "do_body");
	}

	static void AppendTransfer(
		FString& Source,
		const FExitCase& ExitCase,
		const FPlacementCase& PlacementCase,
		const FString& Indent)
	{
		using namespace AngelscriptNativeTestSupport;

		AppendGeneratedAsLine(Source, FString::Printf(
			TEXT("%sRecordVariableLoopEvent(%d + Index);"),
			*Indent,
			ExitCase.EventBase));
		if (IsExit(ExitCase, "break"))
		{
			AppendGeneratedAsLine(Source, Indent + TEXT("break;"));
		}
		else if (IsExit(ExitCase, "continue"))
		{
			if (UsesManualIncrement(PlacementCase))
			{
				AppendGeneratedAsLine(Source, Indent + TEXT("Index = AdvanceVariableLoop(Index);"));
			}
			AppendGeneratedAsLine(Source, Indent + TEXT("continue;"));
		}
		else if (IsExit(ExitCase, "return"))
		{
			AppendGeneratedAsLine(Source, FString::Printf(
				TEXT("%sreturn %d + BodyCount;"),
				*Indent,
				ExitCase.ReturnBase));
		}
		else if (IsExit(ExitCase, "exception"))
		{
			AppendGeneratedAsLine(Source, Indent + TEXT("BodyCount += RaiseVariableLoopException();"));
		}
	}

	static void AppendForInitializerLoop(
		FString& Source,
		const FTypeCase& TypeCase,
		const FIterationCase& IterationCase,
		const FExitCase& ExitCase,
		const FPlacementCase& PlacementCase)
	{
		using namespace AngelscriptNativeTestSupport;

		AppendGeneratedAsLine(Source, FString::Printf(TEXT("\tint Limit = %d;"), IterationCase.Count));
		AppendGeneratedAsLine(Source, TEXT("\tint Index = 0;"));
		AppendGeneratedAsLine(Source, TEXT("\tint BodyCount = 0;"));
		AppendGeneratedAsLine(Source, FString::Printf(
			TEXT("\tfor (%hs Owner(100); Index < Limit; Index = AdvanceVariableLoop(Index))"),
			TypeCase.ScriptType));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendBodyPrelude(Source, TypeCase, PlacementCase, TEXT("\t\t"));
		AppendTransfer(Source, ExitCase, PlacementCase, TEXT("\t\t"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source, FString::Printf(
			TEXT("\treturn %d + BodyCount;"),
			ExitCase.ReturnBase));
	}

	static void AppendForBodyLoop(
		FString& Source,
		const FTypeCase& TypeCase,
		const FIterationCase& IterationCase,
		const FExitCase& ExitCase,
		const FPlacementCase& PlacementCase,
		const bool bNested)
	{
		using namespace AngelscriptNativeTestSupport;

		AppendGeneratedAsLine(Source, FString::Printf(TEXT("\tint Limit = %d;"), IterationCase.Count));
		AppendGeneratedAsLine(Source, TEXT("\tint Index = 0;"));
		AppendGeneratedAsLine(Source, TEXT("\tint BodyCount = 0;"));
		AppendGeneratedAsLine(Source, TEXT("\tfor (; Index < Limit; Index = AdvanceVariableLoop(Index))"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		if (bNested)
		{
			AppendGeneratedAsLine(Source, TEXT("\t\t{"));
		}
		const FString Indent = bNested ? TEXT("\t\t\t") : TEXT("\t\t");
		AppendBodyPrelude(Source, TypeCase, PlacementCase, Indent);
		AppendTransfer(Source, ExitCase, PlacementCase, Indent);
		if (bNested)
		{
			AppendGeneratedAsLine(Source, TEXT("\t\t}"));
		}
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source, FString::Printf(
			TEXT("\treturn %d + BodyCount;"),
			ExitCase.ReturnBase));
	}

	static void AppendWhileLoop(
		FString& Source,
		const FTypeCase& TypeCase,
		const FIterationCase& IterationCase,
		const FExitCase& ExitCase,
		const FPlacementCase& PlacementCase)
	{
		using namespace AngelscriptNativeTestSupport;

		AppendGeneratedAsLine(Source, FString::Printf(TEXT("\tint Limit = %d;"), IterationCase.Count));
		AppendGeneratedAsLine(Source, TEXT("\tint Index = 0;"));
		AppendGeneratedAsLine(Source, TEXT("\tint BodyCount = 0;"));
		AppendGeneratedAsLine(Source, TEXT("\twhile (Index < Limit)"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendBodyPrelude(Source, TypeCase, PlacementCase, TEXT("\t\t"));
		AppendTransfer(Source, ExitCase, PlacementCase, TEXT("\t\t"));
		if (!IsExit(ExitCase, "break")
			&& !IsExit(ExitCase, "continue")
			&& !IsExit(ExitCase, "return")
			&& !IsExit(ExitCase, "exception"))
		{
			AppendGeneratedAsLine(Source, TEXT("\t\tIndex = AdvanceVariableLoop(Index);"));
		}
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source, FString::Printf(
			TEXT("\treturn %d + BodyCount;"),
			ExitCase.ReturnBase));
	}

	static void AppendDoLoop(
		FString& Source,
		const FTypeCase& TypeCase,
		const FIterationCase& IterationCase,
		const FExitCase& ExitCase,
		const FPlacementCase& PlacementCase)
	{
		using namespace AngelscriptNativeTestSupport;

		AppendGeneratedAsLine(Source, FString::Printf(TEXT("\tint Limit = %d;"), IterationCase.Count));
		AppendGeneratedAsLine(Source, TEXT("\tint Index = 0;"));
		AppendGeneratedAsLine(Source, TEXT("\tint BodyCount = 0;"));
		AppendGeneratedAsLine(Source, TEXT("\tdo"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendBodyPrelude(Source, TypeCase, PlacementCase, TEXT("\t\t"));
		AppendTransfer(Source, ExitCase, PlacementCase, TEXT("\t\t"));
		if (!IsExit(ExitCase, "break")
			&& !IsExit(ExitCase, "continue")
			&& !IsExit(ExitCase, "return")
			&& !IsExit(ExitCase, "exception"))
		{
			AppendGeneratedAsLine(Source, TEXT("\t\tIndex = AdvanceVariableLoop(Index);"));
		}
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source, TEXT("\twhile (Index < Limit);"));
		AppendGeneratedAsLine(Source, FString::Printf(
			TEXT("\treturn %d + BodyCount;"),
			ExitCase.ReturnBase));
	}

	static FString BuildVariableLoopSource(
		const FTypeCase& TypeCase,
		const FPlacementCase& PlacementCase,
		const FIterationCase& IterationCase,
		const FExitCase& ExitCase)
	{
		using namespace AngelscriptNativeTestSupport;

		FString Source;
		AppendScriptValueType(Source, TypeCase);
		if (IsExit(ExitCase, "exception"))
		{
			AppendExceptionHelper(Source);
		}
		AppendGeneratedAsLine(Source, TEXT("int RunVariableLoopLifetime()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		if (IsPlacement(PlacementCase, "for_initializer"))
		{
			AppendForInitializerLoop(Source, TypeCase, IterationCase, ExitCase, PlacementCase);
		}
		else if (IsPlacement(PlacementCase, "for_body"))
		{
			AppendForBodyLoop(Source, TypeCase, IterationCase, ExitCase, PlacementCase, false);
		}
		else if (IsPlacement(PlacementCase, "while_body"))
		{
			AppendWhileLoop(Source, TypeCase, IterationCase, ExitCase, PlacementCase);
		}
		else if (IsPlacement(PlacementCase, "do_body"))
		{
			AppendDoLoop(Source, TypeCase, IterationCase, ExitCase, PlacementCase);
		}
		else
		{
			AppendForBodyLoop(Source, TypeCase, IterationCase, ExitCase, PlacementCase, true);
		}
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("int CleanAfterVariableLoopLifetime()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\treturn 91;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		return Source;
	}

	static int32 FullBodyCount(
		const FPlacementCase& PlacementCase,
		const FIterationCase& IterationCase)
	{
		return IsPlacement(PlacementCase, "do_body")
			? FMath::Max(1, IterationCase.Count)
			: IterationCase.Count;
	}

	static int32 ExpectedBodyCount(
		const FPlacementCase& PlacementCase,
		const FIterationCase& IterationCase,
		const FExitCase& ExitCase)
	{
		const int32 FullCount = FullBodyCount(PlacementCase, IterationCase);
		if (FullCount > 0
			&& (IsExit(ExitCase, "break")
				|| IsExit(ExitCase, "return")
				|| IsExit(ExitCase, "exception")))
		{
			return 1;
		}
		return FullCount;
	}

	static int32 ExpectedOwnerCount(
		const FPlacementCase& PlacementCase,
		const FIterationCase& IterationCase,
		const FExitCase& ExitCase)
	{
		return IsPlacement(PlacementCase, "for_initializer")
			? 1
			: ExpectedBodyCount(PlacementCase, IterationCase, ExitCase);
	}

	static bool ShouldRaiseException(
		const FPlacementCase& PlacementCase,
		const FIterationCase& IterationCase,
		const FExitCase& ExitCase)
	{
		return IsExit(ExitCase, "exception")
			&& ExpectedBodyCount(PlacementCase, IterationCase, ExitCase) > 0;
	}

	static TArray<int32> MakeExpectedEvents(
		const FPlacementCase& PlacementCase,
		const FIterationCase& IterationCase,
		const FExitCase& ExitCase)
	{
		TArray<int32> Expected;
		const int32 BodyCount = ExpectedBodyCount(PlacementCase, IterationCase, ExitCase);
		for (int32 Index = 0; Index < BodyCount; ++Index)
		{
			Expected.Add(1000 + Index);
			Expected.Add(ExitCase.EventBase + Index);
			if (IsExit(ExitCase, "normal") || IsExit(ExitCase, "continue"))
			{
				Expected.Add(4000 + Index);
			}
		}
		return Expected;
	}

	void VerifyStaticDebugMetadata(
		const FNativeCaseContext& Case,
		const FTypeCase& TypeCase,
		asIScriptModule& Module)
	{
		asIScriptFunction* const Entry = Module.GetFunctionByDecl("int RunVariableLoopLifetime()");
		ASSERT_THAT(IsNotNull(Entry,
			*Case.Describe(TEXT("loop-lifetime cell should expose its exact entry metadata"))));
		if (Entry == nullptr)
		{
			return;
		}
		const int32 ExpectedTypeId = Module.GetTypeIdByDecl(TypeCase.ScriptType);
		bool bFoundOwner = false;
		for (asUINT Index = 0; Index < Entry->GetVarCount(); ++Index)
		{
			const char* Name = nullptr;
			int TypeId = asTYPEID_VOID;
			if (Entry->GetVar(Index, &Name, &TypeId) >= 0
				&& Name != nullptr
				&& FCStringAnsi::Strcmp(Name, "Owner") == 0)
			{
				bFoundOwner = true;
				ASSERT_THAT(AreEqual(ExpectedTypeId, TypeId,
					*Case.Describe(TEXT("loop Owner should preserve its catalog debug type"))));
				const char* const Declaration = Entry->GetVarDecl(Index, true);
				ASSERT_THAT(IsTrue(Declaration != nullptr
					&& FString(UTF8_TO_TCHAR(Declaration)).Contains(TEXT("Owner")),
					*Case.Describe(TEXT("loop Owner should retain its exact debug declaration"))));
			}
		}
		ASSERT_THAT(IsTrue(bFoundOwner,
			*Case.Describe(TEXT("loop-lifetime entry should publish the Owner debug variable"))));
	}

	void VerifyLifecycle(
		const FNativeCaseContext& Case,
		const FPlacementCase& PlacementCase,
		const FIterationCase& IterationCase,
		const FExitCase& ExitCase,
		const FNativeLifecycleRecorder& Lifecycle)
	{
		const int32 ExpectedCount = ExpectedOwnerCount(PlacementCase, IterationCase, ExitCase);
		ASSERT_THAT(AreEqual(ExpectedCount, Lifecycle.Num(ENativeLifecycleEvent::ValueConstruct),
			*Case.Describe(TEXT("loop placement should construct Owner at the exact reachable frequency"))));
		ASSERT_THAT(AreEqual(0, Lifecycle.Num(ENativeLifecycleEvent::DefaultConstruct),
			*Case.Describe(TEXT("loop Owner should always use its value constructor"))));
		ASSERT_THAT(AreEqual(0, Lifecycle.Num(ENativeLifecycleEvent::CopyConstruct),
			*Case.Describe(TEXT("loop declaration should not introduce implicit Owner copies"))));
		ASSERT_THAT(AreEqual(0, Lifecycle.Num(ENativeLifecycleEvent::Assign),
			*Case.Describe(TEXT("loop declaration should not assign an already live Owner"))));
		ASSERT_THAT(AreEqual(ExpectedCount, Lifecycle.Num(ENativeLifecycleEvent::Destruct),
			*Case.Describe(TEXT("every reached loop Owner should be destroyed exactly once"))));
		ASSERT_THAT(AreEqual(0, Lifecycle.GetLiveObjectCount(),
			*Case.Describe(TEXT("loop exit should leave no Owner alive"))));

		TArray<int32> ConstructedIds;
		TArray<int32> DestructedIds;
		for (const AngelscriptNativeTestSupport::FNativeLifecycleEntry& Entry : Lifecycle.GetEntries())
		{
			if (Entry.Event == ENativeLifecycleEvent::ValueConstruct)
			{
				ConstructedIds.Add(Entry.ObjectId);
			}
			else if (Entry.Event == ENativeLifecycleEvent::Destruct)
			{
				DestructedIds.Add(Entry.ObjectId);
			}
		}
		ASSERT_THAT(AreEqual(ConstructedIds, DestructedIds,
			*Case.Describe(TEXT("per-iteration Owner destruction should follow construction order"))));
	}

	void VerifyTrace(
		const FNativeCaseContext& Case,
		const FPlacementCase& PlacementCase,
		const FIterationCase& IterationCase,
		const FExitCase& ExitCase,
		const FLoopObservation& Observation)
	{
		const TArray<int32> Expected = MakeExpectedEvents(PlacementCase, IterationCase, ExitCase);
		ASSERT_THAT(AreEqual(Expected, Observation.Events,
			*Case.Describe(TEXT("loop body, transfer, and increment events should match the exact reachable trace"))));
	}

	void ExecuteCell(
		const FNativeCaseContext& Case,
		const FTypeCase& TypeCase,
		const FPlacementCase& PlacementCase,
		const FIterationCase& IterationCase,
		const FExitCase& ExitCase,
		asIScriptEngine& ScriptEngine,
		asIScriptModule& Module,
		FNativeLifecycleRecorder& Lifecycle,
		FLoopObservation& Observation,
		const FString& ModuleName)
	{
		using namespace AngelscriptNativeTestSupport;

		VerifyStaticDebugMetadata(Case, TypeCase, Module);
		asIScriptFunction* const Entry = Module.GetFunctionByDecl("int RunVariableLoopLifetime()");
		asIScriptFunction* const Recovery = Module.GetFunctionByDecl("int CleanAfterVariableLoopLifetime()");
		ASSERT_THAT(IsNotNull(Recovery,
			*Case.Describe(TEXT("loop-lifetime cell should expose its recovery entry"))));
		if (Entry == nullptr || Recovery == nullptr)
		{
			return;
		}

		asIScriptContext* const Context = ScriptEngine.CreateContext();
		ASSERT_THAT(IsNotNull(Context,
			*Case.Describe(TEXT("loop-lifetime cell should create an execution context"))));
		if (Context == nullptr)
		{
			return;
		}

		FLoopDebugObservation DebugObservation;
		Context->SetUserData(&DebugObservation, LoopDebugObservationUserDataSlot);
		asCContext* const NativeContext = static_cast<asCContext*>(Context);
		ASSERT_THAT(AreEqual(asSUCCESS, NativeContext->SetLineCallback(CaptureVariableLoopLine),
			*Case.Describe(TEXT("loop-lifetime context should install its raw line callback"))));
		const int32 ExecuteResult = PrepareAndExecute(Context, Entry);
		const bool bShouldRaiseException = ShouldRaiseException(PlacementCase, IterationCase, ExitCase);
		if (bShouldRaiseException)
		{
			ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_EXCEPTION), ExecuteResult,
				*Case.Describe(TEXT("reached loop exception transfer should stop execution"))));
			ASSERT_THAT(IsNotNull(Context->GetExceptionFunction(),
				*Case.Describe(TEXT("loop exception should identify the throwing function"))));
			const char* ExceptionSection = nullptr;
			int ExceptionColumn = 0;
			ASSERT_THAT(IsTrue(Context->GetExceptionLineNumber(&ExceptionColumn, &ExceptionSection) > 0,
				*Case.Describe(TEXT("loop exception should retain a one-based source line"))));
			ASSERT_THAT(IsTrue(ExceptionColumn > 0,
				*Case.Describe(TEXT("loop exception should retain a one-based source column"))));
			ASSERT_THAT(AreEqual(ModuleName, FString(UTF8_TO_TCHAR(ExceptionSection != nullptr ? ExceptionSection : "")),
				*Case.Describe(TEXT("loop exception should retain the generated module section"))));
		}
		else
		{
			ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_FINISHED), ExecuteResult,
				*Case.Describe(TEXT("normal or unreachable-transfer loop should finish"))));
			if (ExecuteResult == asEXECUTION_FINISHED)
			{
				ASSERT_THAT(AreEqual(
					ExitCase.ReturnBase + ExpectedBodyCount(PlacementCase, IterationCase, ExitCase),
					static_cast<int32>(Context->GetReturnDWord()),
					*Case.Describe(TEXT("loop return should encode the reached body count and transfer family"))));
			}
		}

		ASSERT_THAT(IsTrue(DebugObservation.LineCallbacks > 0,
			*Case.Describe(TEXT("loop execution should produce raw line callbacks"))));
		if (ExpectedBodyCount(PlacementCase, IterationCase, ExitCase) > 0)
		{
			ASSERT_THAT(IsTrue(DebugObservation.VisibleOwnerSamples > 0,
				*Case.Describe(TEXT("a reached loop Owner should become visible in debug scope"))));
			ASSERT_THAT(IsTrue(DebugObservation.OwnerAddressSamples > 0,
				*Case.Describe(TEXT("a visible loop Owner should expose a live stack address"))));
		}
		else if (!IsPlacement(PlacementCase, "for_initializer"))
		{
			ASSERT_THAT(AreEqual(0, DebugObservation.VisibleOwnerSamples,
				*Case.Describe(TEXT("an unreachable body-local Owner should never enter debug scope"))));
		}

		NativeContext->SetLineCallback(nullptr);
		ASSERT_THAT(AreEqual(asSUCCESS, Context->Unprepare(),
			*Case.Describe(TEXT("loop-lifetime context should unprepare after transfer or exception"))));
		VerifyLifecycle(Case, PlacementCase, IterationCase, ExitCase, Lifecycle);
		VerifyTrace(Case, PlacementCase, IterationCase, ExitCase, Observation);
		const int32 LifecycleCount = Lifecycle.GetEntries().Num();
		const int32 TraceCount = Observation.Events.Num();
		ASSERT_THAT(AreEqual(asSUCCESS, Context->Prepare(Recovery),
			*Case.Describe(TEXT("loop-lifetime recovery should prepare on the same context"))));
		ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_FINISHED), Context->Execute(),
			*Case.Describe(TEXT("loop-lifetime recovery should execute after cleanup"))));
		ASSERT_THAT(AreEqual(91, static_cast<int32>(Context->GetReturnDWord()),
			*Case.Describe(TEXT("loop-lifetime recovery should not retain stale transfer state"))));
		ASSERT_THAT(AreEqual(LifecycleCount, Lifecycle.GetEntries().Num(),
			*Case.Describe(TEXT("recovery should not create hidden loop owners"))));
		ASSERT_THAT(AreEqual(TraceCount, Observation.Events.Num(),
			*Case.Describe(TEXT("recovery should not append hidden loop events"))));
		Context->Release();
	}

public:
	TEST_METHOD(TypesByPlacementIterationsAndExit)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT("LANG-VAR-LOOP-DECL-LIFETIME",
			ENativeEvidence::Compile
			| ENativeEvidence::Runtime
			| ENativeEvidence::Diagnostic
			| ENativeEvidence::Lifecycle
			| ENativeEvidence::Debug
			| ENativeEvidence::Cleanup);

		// Raw SDK engines do not receive the host runtime's debugger-state update.
		// Keep the callback gate local to this debug-observable product.
		FScopedNativeDebugCallbacks ScopedDebugCallbacks;

		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };
		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine,
			TEXT("Variable-loop-lifetime product should create a raw SDK engine")));
		if (ScriptEngine == nullptr)
		{
			return;
		}

		FNativeLifecycleRecorder Lifecycle;
		FLoopObservation Observation;
		Lifecycle.Reset();
		Observation.Reset();
		ASSERT_THAT(IsTrue(RegisterNativeCaseValue(*ScriptEngine, Lifecycle),
			TEXT("Variable-loop-lifetime product should register its tracked native value")));
		ASSERT_THAT(IsTrue(RegisterLoopSupport(*ScriptEngine, Lifecycle, Observation),
			TEXT("Variable-loop-lifetime product should register script lifecycle and trace callbacks")));

		for (const FExitCase& ExitCase : ExitCases)
		{
			for (const FIterationCase& IterationCase : IterationCases)
			{
				for (const FPlacementCase& PlacementCase : PlacementCases)
				{
					for (const FTypeCase& TypeCase : TypeCases)
					{
						Lifecycle.Reset();
						Observation.Reset();
						const FNativeCaseContext Case(MakeNativeCaseId(
							"LANG-VAR-LOOP-DECL-LIFETIME",
							{
								ANSI_TO_TCHAR(ExitCase.CatalogName),
								ANSI_TO_TCHAR(IterationCase.CatalogName),
								ANSI_TO_TCHAR(PlacementCase.CatalogName),
								ANSI_TO_TCHAR(TypeCase.CatalogName),
							}));
						const FString Suffix = MakeSuffix(ExitCase, IterationCase, PlacementCase, TypeCase);
						const FString ModuleName = TEXT("VariableLoopLifetime_") + Suffix;
						const FString Source = BuildVariableLoopSource(TypeCase, PlacementCase, IterationCase, ExitCase);
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
							*Case.Describe(TEXT("variable-loop-lifetime cell should compile"))));
						ASSERT_THAT(IsNotNull(Module,
							*Case.Describe(TEXT("variable-loop-lifetime cell should publish a module"))));
						if (BuildResult >= 0 && Module != nullptr)
						{
							ExecuteCell(
								Case,
								TypeCase,
								PlacementCase,
								IterationCase,
								ExitCase,
								*ScriptEngine,
								*Module,
								Lifecycle,
								Observation,
								ModuleName);
						}

						ScriptEngine->DiscardModule(ModuleNameUtf8.Get());
						ASSERT_THAT(IsNull(ScriptEngine->GetModule(ModuleNameUtf8.Get(), asGM_ONLY_IF_EXISTS),
							*Case.Describe(TEXT("loop-lifetime cell should discard its isolated module"))));
						ASSERT_THAT(AreEqual(0, Lifecycle.GetLiveObjectCount(),
							*Case.Describe(TEXT("loop-lifetime module discard should leave no Owner alive"))));
					}
				}
			}
		}
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
