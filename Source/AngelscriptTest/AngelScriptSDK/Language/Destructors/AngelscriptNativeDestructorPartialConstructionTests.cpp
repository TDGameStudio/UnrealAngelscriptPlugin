#include "../../Support/AngelscriptNativeFixtureTestSupport.h"
#include "../../Support/AngelscriptNativeLanguageCaseTestSupport.h"

#include "CQTest.h"
#include "Misc/ScopeExit.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FDestructorPartialConstructionTests,
	"Angelscript.TestModule.AngelScriptSDK.Language.Destructors.PartialConstruction",
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

	static constexpr asPWORD DestructorPartialStateUserDataSlot =
		static_cast<asPWORD>(0x44544F5250415254ull);

	enum class EPartialExit : uint8
	{
		Normal,
		Exception,
		Abort,
		Unprepare,
	};

	struct FTopologyCase
	{
		const ANSICHAR* CatalogName;
		const ANSICHAR* PrimaryType;
		bool bHasTransfer;
		bool bRequiresNativeFieldAddRef;
		bool bSelfTransfer;
		bool bExpectedSameRoot;
	};

	struct FBoundaryCase
	{
		const ANSICHAR* CatalogName;
		int32 Stage;
		int32 OwnedObjectCount;
		int32 ExpectedNormalResult;
	};

	struct FExitCase
	{
		const ANSICHAR* CatalogName;
		EPartialExit Kind;
		int32 ExpectedExecutionState;
	};

	inline static constexpr FTopologyCase TopologyCases[] =
	{
		{
			"independent",
			nullptr,
			false,
			false,
			false,
			false,
		},
		{
			"nested_member",
			"FPartialNestedOwner",
			false,
			false,
			false,
			false,
		},
		{
			"base_derived",
			"FPartialDerivedOwner",
			false,
			false,
			false,
			false,
		},
		{
			"copy_transfer",
			"FPartialBundle",
			true,
			true,
			false,
			false,
		},
		{
			"assignment_transfer",
			"FPartialBundle",
			true,
			true,
			false,
			false,
		},
		{
			"self_assignment",
			"FPartialBundle",
			true,
			false,
			true,
			true,
		},
		{
			"reference_alias",
			"FPartialReferenceOwner",
			true,
			false,
			false,
			true,
		},
	};

	inline static constexpr FBoundaryCase BoundaryCases[] =
	{
		{ "before_root", 0, 0, 0 },
		{ "root_started", 1, 0, 1 },
		{ "first_owned", 2, 1, 101 },
		{ "middle_owned", 3, 2, 303 },
		{ "all_owned", 4, 3, 606 },
		{ "complete", 5, 3, 606 },
	};

	inline static constexpr FExitCase ExitCases[] =
	{
		{
			"normal",
			EPartialExit::Normal,
			asEXECUTION_FINISHED,
		},
		{
			"exception",
			EPartialExit::Exception,
			asEXECUTION_EXCEPTION,
		},
		{
			"abort",
			EPartialExit::Abort,
			// Abort() is an explicit asERROR stub in the current fork, so
			// the active characterization observes normal completion.
			asEXECUTION_FINISHED,
		},
		{
			"unprepare",
			EPartialExit::Unprepare,
			// Suspend() is the matching current-fork asERROR stub.
			asEXECUTION_FINISHED,
		},
	};

	struct FDestructorPartialState
	{
		FNativeLifecycleRecorder* Lifecycle = nullptr;
		EPartialExit Exit = EPartialExit::Normal;
		TArray<int32> ReachedStages;
		int32 ReachedBoundary = INDEX_NONE;
		int32 LiveObjectsAtExit = INDEX_NONE;
		int32 DestructsAtExit = INDEX_NONE;
		int32 AddRefsBeforeTransfer = INDEX_NONE;
		int32 ReleasesBeforeTransfer = INDEX_NONE;
		int32 AddRefsAtExit = INDEX_NONE;
		int32 ReleasesAtExit = INDEX_NONE;
		int32 TerminalRequestResult = INDEX_NONE;
		int32 RelationObservationCount = 0;
		bool bSameRoot = false;
		bool bSameFirst = false;
		bool bSameMiddle = false;
		bool bSameLast = false;

		void Reset(
			FNativeLifecycleRecorder& InLifecycle,
			const EPartialExit InExit)
		{
			Lifecycle = &InLifecycle;
			Exit = InExit;
			ReachedStages.Reset();
			ReachedBoundary = INDEX_NONE;
			LiveObjectsAtExit = INDEX_NONE;
			DestructsAtExit = INDEX_NONE;
			AddRefsBeforeTransfer = INDEX_NONE;
			ReleasesBeforeTransfer = INDEX_NONE;
			AddRefsAtExit = INDEX_NONE;
			ReleasesAtExit = INDEX_NONE;
			TerminalRequestResult = INDEX_NONE;
			RelationObservationCount = 0;
			bSameRoot = false;
			bSameFirst = false;
			bSameMiddle = false;
			bSameLast = false;
		}
	};

	static bool IsTopology(
		const FTopologyCase& Topology,
		const ANSICHAR* Name)
	{
		return FCStringAnsi::Strcmp(Topology.CatalogName, Name) == 0;
	}

	static FDestructorPartialState* GetActivePartialState()
	{
		asIScriptContext* const Context = asGetActiveContext();
		return Context != nullptr && Context->GetEngine() != nullptr
			? static_cast<FDestructorPartialState*>(
				Context->GetEngine()->GetUserData(
					DestructorPartialStateUserDataSlot))
			: nullptr;
	}

	static void RecordDestructorPartialStage(const int32 Stage)
	{
		if (FDestructorPartialState* const State =
			GetActivePartialState())
		{
			State->ReachedStages.Add(Stage);
		}
	}

	static void RecordDestructorPartialTransferStart()
	{
		if (FDestructorPartialState* const State =
			GetActivePartialState();
			State != nullptr && State->Lifecycle != nullptr)
		{
			State->AddRefsBeforeTransfer =
				State->Lifecycle->Num(ENativeLifecycleEvent::AddRef);
			State->ReleasesBeforeTransfer =
				State->Lifecycle->Num(ENativeLifecycleEvent::Release);
		}
	}

	static void RecordDestructorPartialRelation(
		const bool bSameRoot,
		const bool bSameFirst,
		const bool bSameMiddle,
		const bool bSameLast)
	{
		if (FDestructorPartialState* const State =
			GetActivePartialState())
		{
			++State->RelationObservationCount;
			State->bSameRoot = bSameRoot;
			State->bSameFirst = bSameFirst;
			State->bSameMiddle = bSameMiddle;
			State->bSameLast = bSameLast;
		}
	}

	static bool ReachDestructorPartialBoundary(const int32 Boundary)
	{
		FDestructorPartialState* const State =
			GetActivePartialState();
		asIScriptContext* const Context = asGetActiveContext();
		if (State == nullptr
			|| State->Lifecycle == nullptr
			|| Context == nullptr)
		{
			return true;
		}

		State->ReachedBoundary = Boundary;
		State->LiveObjectsAtExit =
			State->Lifecycle->GetLiveObjectCount();
		State->DestructsAtExit =
			State->Lifecycle->Num(ENativeLifecycleEvent::Destruct);
		State->AddRefsAtExit =
			State->Lifecycle->Num(ENativeLifecycleEvent::AddRef);
		State->ReleasesAtExit =
			State->Lifecycle->Num(ENativeLifecycleEvent::Release);
		switch (State->Exit)
		{
		case EPartialExit::Exception:
			Context->SetException(
				"Destructor partial exception sentinel");
			break;
		case EPartialExit::Abort:
			State->TerminalRequestResult = Context->Abort();
			break;
		case EPartialExit::Unprepare:
			State->TerminalRequestResult = Context->Suspend();
			break;
		case EPartialExit::Normal:
		default:
			break;
		}
		return true;
	}

	static bool RegisterDestructorPartialBridge(
		asIScriptEngine& ScriptEngine,
		FDestructorPartialState& State)
	{
		ScriptEngine.SetUserData(
			&State,
			DestructorPartialStateUserDataSlot);
		const ASAutoCaller::FunctionCaller StageCaller =
			ASAutoCaller::MakeFunctionCaller(
				RecordDestructorPartialStage);
		const ASAutoCaller::FunctionCaller TransferCaller =
			ASAutoCaller::MakeFunctionCaller(
				RecordDestructorPartialTransferStart);
		const ASAutoCaller::FunctionCaller RelationCaller =
			ASAutoCaller::MakeFunctionCaller(
				RecordDestructorPartialRelation);
		const ASAutoCaller::FunctionCaller BoundaryCaller =
			ASAutoCaller::MakeFunctionCaller(
				ReachDestructorPartialBoundary);
		return ScriptEngine.RegisterGlobalFunction(
			"void RecordDestructorPartialStage(int Stage)",
			asFUNCTION(RecordDestructorPartialStage),
			asCALL_CDECL,
			*(asFunctionCaller*)&StageCaller) >= 0
			&& ScriptEngine.RegisterGlobalFunction(
				"void RecordDestructorPartialTransferStart()",
				asFUNCTION(RecordDestructorPartialTransferStart),
				asCALL_CDECL,
				*(asFunctionCaller*)&TransferCaller) >= 0
			&& ScriptEngine.RegisterGlobalFunction(
				"void RecordDestructorPartialRelation(bool SameRoot, bool SameFirst, bool SameMiddle, bool SameLast)",
				asFUNCTION(RecordDestructorPartialRelation),
				asCALL_CDECL,
				*(asFunctionCaller*)&RelationCaller) >= 0
			&& ScriptEngine.RegisterGlobalFunction(
				"bool ReachDestructorPartialBoundary(int Boundary)",
				asFUNCTION(ReachDestructorPartialBoundary),
				asCALL_CDECL,
				*(asFunctionCaller*)&BoundaryCaller) >= 0;
	}

	static void AppendReferenceFields(FString& Source)
	{
		using namespace AngelscriptNativeTestSupport;

		AppendGeneratedAsLine(
			Source,
			TEXT("\tFNativeCaseReference First;"));
		AppendGeneratedAsLine(
			Source,
			TEXT("\tFNativeCaseReference Middle;"));
		AppendGeneratedAsLine(
			Source,
			TEXT("\tFNativeCaseReference Last;"));
	}

	static void AppendNestedOwnerType(FString& Source)
	{
		using namespace AngelscriptNativeTestSupport;

		AppendGeneratedAsLine(
			Source,
			TEXT("class FPartialNestedOwner"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendReferenceFields(Source);
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
	}

	static void AppendBaseDerivedOwnerTypes(FString& Source)
	{
		using namespace AngelscriptNativeTestSupport;

		AppendGeneratedAsLine(
			Source,
			TEXT("class FPartialBaseOwner"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(
			Source,
			TEXT("\tFNativeCaseReference First;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(
			Source,
			TEXT("class FPartialDerivedOwner : FPartialBaseOwner"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(
			Source,
			TEXT("\tFNativeCaseReference Middle;"));
		AppendGeneratedAsLine(
			Source,
			TEXT("\tFNativeCaseReference Last;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
	}

	static void AppendBundleType(FString& Source)
	{
		using namespace AngelscriptNativeTestSupport;

		AppendGeneratedAsLine(Source, TEXT("struct FPartialBundle"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendReferenceFields(Source);
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
	}

	static void AppendReferenceOwnerType(FString& Source)
	{
		using namespace AngelscriptNativeTestSupport;

		AppendGeneratedAsLine(
			Source,
			TEXT("class FPartialReferenceOwner"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendReferenceFields(Source);
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
	}

	static void AppendTopologyTypes(
		FString& Source,
		const FTopologyCase& Topology)
	{
		if (IsTopology(Topology, "nested_member"))
		{
			AppendNestedOwnerType(Source);
		}
		else if (IsTopology(Topology, "base_derived"))
		{
			AppendBaseDerivedOwnerTypes(Source);
		}
		else if (IsTopology(Topology, "reference_alias"))
		{
			AppendReferenceOwnerType(Source);
		}
		else if (Topology.bHasTransfer)
		{
			AppendBundleType(Source);
		}
	}

	static FString RootVariableName(
		const FTopologyCase& Topology)
	{
		return IsTopology(Topology, "independent")
			? FString()
			: FString(
				Topology.bHasTransfer
					? TEXT("Source")
					: TEXT("Root"));
	}

	static void AppendRootStart(
		FString& Source,
		const FTopologyCase& Topology)
	{
		using namespace AngelscriptNativeTestSupport;

		if (IsTopology(Topology, "independent"))
		{
			AppendGeneratedAsLine(
				Source,
				TEXT("\tint RootStarted = 1;"));
		}
		else
		{
			AppendGeneratedAsLine(Source, FString::Printf(
				TEXT("\t%hs %s = %hs();"),
				Topology.PrimaryType,
				*RootVariableName(Topology),
				Topology.PrimaryType));
		}
	}

	static FString OwnedExpression(
		const FTopologyCase& Topology,
		const TCHAR* FieldName)
	{
		return IsTopology(Topology, "independent")
			? FString(FieldName)
			: FString::Printf(
				TEXT("%s.%s"),
				*RootVariableName(Topology),
				FieldName);
	}

	static void AppendOwnedStage(
		FString& Source,
		const FTopologyCase& Topology,
		const TCHAR* FieldName,
		const int32 Value)
	{
		using namespace AngelscriptNativeTestSupport;

		if (IsTopology(Topology, "independent"))
		{
			AppendGeneratedAsLine(Source, FString::Printf(
				TEXT("\tFNativeCaseReference %s = CreateNativeCaseReference(%d);"),
				FieldName,
				Value));
		}
		else
		{
			AppendGeneratedAsLine(Source, FString::Printf(
				TEXT("\t%s = CreateNativeCaseReference(%d);"),
				*OwnedExpression(Topology, FieldName),
				Value));
		}
	}

	static FString SourceChecksum(
		const FTopologyCase& Topology)
	{
		return FString::Printf(
			TEXT("%s.Value + %s.Value + %s.Value"),
			*OwnedExpression(Topology, TEXT("First")),
			*OwnedExpression(Topology, TEXT("Middle")),
			*OwnedExpression(Topology, TEXT("Last")));
	}

	static void AppendTransferCompletion(
		FString& Source,
		const FTopologyCase& Topology)
	{
		using namespace AngelscriptNativeTestSupport;

		if (!Topology.bHasTransfer)
		{
			AppendGeneratedAsLine(
				Source,
				TEXT("\tint Completed = 1;"));
			return;
		}

		AppendGeneratedAsLine(
			Source,
			TEXT("\tRecordDestructorPartialTransferStart();"));
		if (IsTopology(Topology, "copy_transfer"))
		{
			AppendGeneratedAsLine(
				Source,
				TEXT("\tFPartialBundle Target = Source;"));
			AppendGeneratedAsLine(
				Source,
				TEXT("\tRecordDestructorPartialRelation(false, Source.First is Target.First, Source.Middle is Target.Middle, Source.Last is Target.Last);"));
		}
		else if (IsTopology(Topology, "assignment_transfer"))
		{
			AppendGeneratedAsLine(
				Source,
				TEXT("\tFPartialBundle Target;"));
			AppendGeneratedAsLine(
				Source,
				TEXT("\tTarget = Source;"));
			AppendGeneratedAsLine(
				Source,
				TEXT("\tRecordDestructorPartialRelation(false, Source.First is Target.First, Source.Middle is Target.Middle, Source.Last is Target.Last);"));
		}
		else if (IsTopology(Topology, "self_assignment"))
		{
			AppendGeneratedAsLine(
				Source,
				TEXT("\tSource = Source;"));
			AppendGeneratedAsLine(
				Source,
				TEXT("\tRecordDestructorPartialRelation(true, Source.First is Source.First, Source.Middle is Source.Middle, Source.Last is Source.Last);"));
		}
		else
		{
			AppendGeneratedAsLine(
				Source,
				TEXT("\tFPartialReferenceOwner Alias = Source;"));
			AppendGeneratedAsLine(
				Source,
				TEXT("\tRecordDestructorPartialRelation(Source is Alias, Source.First is Alias.First, Source.Middle is Alias.Middle, Source.Last is Alias.Last);"));
		}
	}

	static FString CompleteResultExpression(
		const FTopologyCase& Topology)
	{
		if (IsTopology(Topology, "copy_transfer")
			|| IsTopology(Topology, "assignment_transfer"))
		{
			return SourceChecksum(Topology)
				+ TEXT(" + Target.First.Value")
				+ TEXT(" + Target.Middle.Value")
				+ TEXT(" + Target.Last.Value");
		}
		if (IsTopology(Topology, "reference_alias"))
		{
			return SourceChecksum(Topology)
				+ TEXT(" + Alias.First.Value")
				+ TEXT(" + Alias.Middle.Value")
				+ TEXT(" + Alias.Last.Value");
		}
		return SourceChecksum(Topology);
	}

	static void AppendBoundaryExit(
		FString& Source,
		const int32 Stage,
		const FString& ResultExpression)
	{
		using namespace AngelscriptNativeTestSupport;

		AppendGeneratedAsLine(Source, FString::Printf(
			TEXT("\tRecordDestructorPartialStage(%d);"),
			Stage));
		AppendGeneratedAsLine(Source, FString::Printf(
			TEXT("\tif (ReachDestructorPartialBoundary(%d))"),
			Stage));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(
			Source,
			FString::Printf(
				TEXT("\t\treturn %s;"),
				*ResultExpression));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
	}

	static FString ResultExpressionAtStage(
		const FTopologyCase& Topology,
		const int32 Stage)
	{
		switch (Stage)
		{
		case 0:
			return TEXT("0");
		case 1:
			return TEXT("1");
		case 2:
			return OwnedExpression(
				Topology,
				TEXT("First")) + TEXT(".Value");
		case 3:
			return OwnedExpression(
				Topology,
				TEXT("First")) + TEXT(".Value + ")
				+ OwnedExpression(
					Topology,
					TEXT("Middle")) + TEXT(".Value");
		case 4:
			return SourceChecksum(Topology);
		case 5:
		default:
			return CompleteResultExpression(Topology);
		}
	}

	static void AppendSelectedWorkflow(
		FString& Source,
		const FTopologyCase& Topology,
		const FBoundaryCase& Boundary)
	{
		using namespace AngelscriptNativeTestSupport;

		AppendGeneratedAsLine(
			Source,
			TEXT("int RunDestructorPartial()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		if (Boundary.Stage == 0)
		{
			AppendBoundaryExit(
				Source,
				0,
				ResultExpressionAtStage(Topology, 0));
			AppendGeneratedAsLine(Source, TEXT("\treturn -1;"));
			AppendGeneratedAsLine(Source, TEXT("}"));
			return;
		}

		AppendRootStart(Source, Topology);
		if (Boundary.Stage == 1)
		{
			AppendBoundaryExit(
				Source,
				1,
				ResultExpressionAtStage(Topology, 1));
			AppendGeneratedAsLine(Source, TEXT("\treturn -1;"));
			AppendGeneratedAsLine(Source, TEXT("}"));
			return;
		}

		AppendGeneratedAsLine(
			Source,
			TEXT("\tRecordDestructorPartialStage(1);"));
		AppendOwnedStage(
			Source,
			Topology,
			TEXT("First"),
			101);
		if (Boundary.Stage == 2)
		{
			AppendBoundaryExit(
				Source,
				2,
				ResultExpressionAtStage(Topology, 2));
			AppendGeneratedAsLine(Source, TEXT("\treturn -1;"));
			AppendGeneratedAsLine(Source, TEXT("}"));
			return;
		}

		AppendGeneratedAsLine(
			Source,
			TEXT("\tRecordDestructorPartialStage(2);"));
		AppendOwnedStage(
			Source,
			Topology,
			TEXT("Middle"),
			202);
		if (Boundary.Stage == 3)
		{
			AppendBoundaryExit(
				Source,
				3,
				ResultExpressionAtStage(Topology, 3));
			AppendGeneratedAsLine(Source, TEXT("\treturn -1;"));
			AppendGeneratedAsLine(Source, TEXT("}"));
			return;
		}

		AppendGeneratedAsLine(
			Source,
			TEXT("\tRecordDestructorPartialStage(3);"));
		AppendOwnedStage(
			Source,
			Topology,
			TEXT("Last"),
			303);
		if (Boundary.Stage == 4)
		{
			AppendBoundaryExit(
				Source,
				4,
				ResultExpressionAtStage(Topology, 4));
			AppendGeneratedAsLine(Source, TEXT("\treturn -1;"));
			AppendGeneratedAsLine(Source, TEXT("}"));
			return;
		}

		AppendGeneratedAsLine(
			Source,
			TEXT("\tRecordDestructorPartialStage(4);"));
		AppendTransferCompletion(Source, Topology);
		AppendBoundaryExit(
			Source,
			5,
			ResultExpressionAtStage(Topology, 5));
		AppendGeneratedAsLine(Source, TEXT("\treturn -1;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
	}

	static void AppendRecoveryFunction(FString& Source)
	{
		using namespace AngelscriptNativeTestSupport;

		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(
			Source,
			TEXT("int RunDestructorPartialRecovery()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(
			Source,
			TEXT("\tFNativeCaseReference Recovery = CreateNativeCaseReference(909);"));
		AppendGeneratedAsLine(
			Source,
			TEXT("\treturn Recovery.Value;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
	}

	static FString BuildDestructorPartialSource(
		const FTopologyCase& Topology,
		const FBoundaryCase& Boundary)
	{
		FString Source;
		AppendTopologyTypes(Source, Topology);
		AppendSelectedWorkflow(
			Source,
			Topology,
			Boundary);
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

	static TArray<int32> ExpectedReachedStages(
		const int32 BoundaryStage)
	{
		if (BoundaryStage == 0)
		{
			return { 0 };
		}

		TArray<int32> Result;
		for (int32 Stage = 1; Stage <= BoundaryStage; ++Stage)
		{
			Result.Add(Stage);
		}
		return Result;
	}

	static TArray<int32> ExpectedDestroyedValues(
		const int32 OwnedObjectCount)
	{
		switch (OwnedObjectCount)
		{
		case 1:
			return { 101 };
		case 2:
			return { 202, 101 };
		case 3:
			return { 303, 202, 101 };
		case 0:
		default:
			return {};
		}
	}

	static TArray<int32> GatherDestroyedValues(
		const FNativeLifecycleRecorder& Lifecycle)
	{
		TArray<int32> Values;
		for (const FNativeLifecycleEntry& Entry :
			Lifecycle.GetEntries())
		{
			if (Entry.Event == ENativeLifecycleEvent::Destruct)
			{
				Values.Add(Entry.Value);
			}
		}
		return Values;
	}

	static FString FormatIntArray(const TArray<int32>& Values)
	{
		FString Result = TEXT("[");
		for (int32 Index = 0; Index < Values.Num(); ++Index)
		{
			if (Index > 0)
			{
				Result += TEXT(", ");
			}
			Result += FString::FromInt(Values[Index]);
		}
		Result += TEXT("]");
		return Result;
	}

	void VerifyCompiledMetadata(
		const FNativeCaseContext& Case,
		const FTopologyCase& Topology,
		asIScriptModule& Module)
	{
		asIScriptFunction* const Entry =
			Module.GetFunctionByDecl("int RunDestructorPartial()");
		ASSERT_THAT(IsNotNull(Entry,
			*Case.Describe(TEXT("partial destructor source should publish its exact entry"))));
		if (Entry != nullptr)
		{
			ASSERT_THAT(IsNotNull(Entry->GetScriptSectionName(),
				*Case.Describe(TEXT("partial destructor entry should retain source metadata"))));
		}

		if (Topology.PrimaryType == nullptr)
		{
			return;
		}

		asITypeInfo* const Type =
			Module.GetTypeInfoByName(Topology.PrimaryType);
		ASSERT_THAT(IsNotNull(Type,
			*Case.Describe(TEXT("partial destructor topology should publish its primary owner type"))));
		if (Type == nullptr)
		{
			return;
		}

		if (IsTopology(Topology, "base_derived"))
		{
			asITypeInfo* const Base =
				Module.GetTypeInfoByName("FPartialBaseOwner");
			ASSERT_THAT(IsNotNull(Base,
				*Case.Describe(TEXT("base-derived partial topology should publish its base type"))));
			if (Base != nullptr)
			{
				ASSERT_THAT(AreEqual(
					Base,
					Type->GetBaseType(),
					*Case.Describe(TEXT("base-derived partial topology should retain the exact base relation"))));
			}
		}
		else
		{
			ASSERT_THAT(AreEqual(
				3,
				static_cast<int32>(Type->GetPropertyCount()),
				*Case.Describe(TEXT("partial owner type should publish all three ownership slots"))));
		}

		if (IsTopology(Topology, "copy_transfer")
			|| IsTopology(Topology, "assignment_transfer")
			|| IsTopology(Topology, "self_assignment"))
		{
			ASSERT_THAT(IsTrue(
				(Type->GetFlags() & asOBJ_VALUE) != 0,
				*Case.Describe(TEXT("value-transfer topology should use independent bundle storage"))));
			// This fork does not synthesize 2.38's automatic copy/assignment
			// special members for script value types. Keep the metadata probe
			// active so an upgrade that changes the contract is visible.
			ASSERT_THAT(IsNull(
				Type->GetMethodByName("opAssign"),
				*Case.Describe(TEXT("current fork should not synthesize an automatic assignment member"))));
		}
		if (IsTopology(Topology, "reference_alias"))
		{
			ASSERT_THAT(IsTrue(
				(Type->GetFlags() & asOBJ_REF) != 0,
				*Case.Describe(TEXT("reference-alias topology should use reference owner storage"))));
		}
	}

	void VerifyExitState(
		const FNativeCaseContext& Case,
		const FTopologyCase& Topology,
		const FBoundaryCase& Boundary,
		const FExitCase& Exit,
		const FDestructorPartialState& State,
		asIScriptContext& Context,
		const int32 ExecutionState)
	{
		ASSERT_THAT(AreEqual(
			Exit.ExpectedExecutionState,
			ExecutionState,
			*Case.Describe(TEXT("partial destructor execution state should match the selected termination route"))));
		ASSERT_THAT(AreEqual(
			Exit.ExpectedExecutionState,
			Context.GetState(),
			*Case.Describe(TEXT("partial destructor context state should retain the selected termination route"))));
		if (Exit.Kind == EPartialExit::Abort
			|| Exit.Kind == EPartialExit::Unprepare)
		{
			ASSERT_THAT(AreEqual(
				asERROR,
				State.TerminalRequestResult,
				*Case.Describe(TEXT("current fork should report the unsupported partial termination request explicitly"))));
		}
		if (Exit.Kind == EPartialExit::Normal)
		{
			const bool bDuplicatesObservedValues =
				Boundary.Stage == 5
				&& (IsTopology(Topology, "copy_transfer")
					|| IsTopology(
						Topology,
						"assignment_transfer")
					|| IsTopology(
						Topology,
						"reference_alias"));
			ASSERT_THAT(AreEqual(
				bDuplicatesObservedValues
					? Boundary.ExpectedNormalResult * 2
					: Boundary.ExpectedNormalResult,
				static_cast<int32>(Context.GetReturnDWord()),
				*Case.Describe(TEXT("normal partial destructor path should return the reached prefix checksum"))));
		}
		else if (Exit.Kind == EPartialExit::Exception)
		{
			ASSERT_THAT(AreEqual(
				FString(TEXT("Destructor partial exception sentinel")),
				FString(UTF8_TO_TCHAR(
					Context.GetExceptionString())),
				*Case.Describe(TEXT("partial destructor exception should retain its exact owner"))));
			ASSERT_THAT(IsTrue(
				Context.GetExceptionLineNumber() > 0,
				*Case.Describe(TEXT("partial destructor exception should retain a source location"))));
		}
		else
		{
			ASSERT_THAT(IsTrue(
				Context.GetExceptionString() == nullptr
					|| Context.GetExceptionString()[0] == '\0',
				*Case.Describe(TEXT("abort and suspend routes should not fabricate a script exception"))));
		}
	}

	void VerifyBoundarySnapshot(
		const FNativeCaseContext& Case,
		const FTopologyCase& Topology,
		const FBoundaryCase& Boundary,
		const FDestructorPartialState& State)
	{
		ASSERT_THAT(AreEqual(
			Boundary.Stage,
			State.ReachedBoundary,
			*Case.Describe(TEXT("partial destructor should stop at the selected real boundary"))));
		ASSERT_THAT(AreEqual(
			ExpectedReachedStages(Boundary.Stage),
			State.ReachedStages,
			*Case.Describe(TEXT("partial destructor should reach the exact construction or transfer prefix"))));
		ASSERT_THAT(AreEqual(
			Boundary.OwnedObjectCount,
			State.LiveObjectsAtExit,
			*Case.Describe(TEXT("partial destructor boundary should own the exact reached object prefix"))));
		ASSERT_THAT(AreEqual(
			0,
			State.DestructsAtExit,
			*Case.Describe(TEXT("partial destructor should destroy no owned object before the exit boundary"))));

		const bool bAtCompletedTransfer =
			Boundary.Stage == 5 && Topology.bHasTransfer;
		ASSERT_THAT(AreEqual(
			bAtCompletedTransfer ? 1 : 0,
			State.RelationObservationCount,
			*Case.Describe(TEXT("partial destructor should observe transfer relations only after transfer completion"))));
		if (!bAtCompletedTransfer)
		{
			return;
		}

		ASSERT_THAT(AreEqual(
			Topology.bExpectedSameRoot,
			State.bSameRoot,
			*Case.Describe(TEXT("partial transfer should preserve the expected root storage or alias relation"))));
		ASSERT_THAT(IsTrue(
			State.bSameFirst
				&& State.bSameMiddle
				&& State.bSameLast,
			*Case.Describe(TEXT("partial transfer should preserve all three referenced owned identities"))));
		ASSERT_THAT(IsTrue(
			State.AddRefsBeforeTransfer >= 0
				&& State.ReleasesBeforeTransfer >= 0,
			*Case.Describe(TEXT("partial transfer should capture its pre-transfer lifecycle boundary"))));
		if (Topology.bRequiresNativeFieldAddRef)
		{
			ASSERT_THAT(AreEqual(
				3,
				State.AddRefsAtExit
					- State.AddRefsBeforeTransfer,
				*Case.Describe(TEXT("copy and assignment transfer should add one owner for each referenced field"))));
		}
		if (Topology.bSelfTransfer)
		{
			ASSERT_THAT(AreEqual(
				State.AddRefsAtExit
					- State.AddRefsBeforeTransfer,
				State.ReleasesAtExit
					- State.ReleasesBeforeTransfer,
				*Case.Describe(TEXT("self-assignment should keep any temporary ownership operations balanced"))));
			ASSERT_THAT(AreEqual(
				0,
				State.DestructsAtExit,
				*Case.Describe(TEXT("self-assignment should destroy no owned identity at the transfer boundary"))));
		}
	}

	void VerifyPrimaryCleanup(
		const FNativeCaseContext& Case,
		const FBoundaryCase& Boundary,
		const FNativeLifecycleRecorder& Lifecycle)
	{
		const TArray<int32> ExpectedValues =
			ExpectedDestroyedValues(Boundary.OwnedObjectCount);
		const TArray<int32> ActualValues =
			GatherDestroyedValues(Lifecycle);
		ASSERT_THAT(AreEqual(
			0,
			Lifecycle.GetLiveObjectCount(),
			*Case.Describe(TEXT("partial destructor cleanup should leave no live native identity"))));
		ASSERT_THAT(AreEqual(
			Boundary.OwnedObjectCount,
			Lifecycle.Num(ENativeLifecycleEvent::ValueConstruct),
			*Case.Describe(TEXT("partial destructor should construct only the reached owned prefix"))));
		ASSERT_THAT(AreEqual(
			Boundary.OwnedObjectCount,
			Lifecycle.Num(ENativeLifecycleEvent::Destruct),
			*Case.Describe(TEXT("partial destructor should destroy every reached identity exactly once"))));
		ASSERT_THAT(AreEqual(
			ExpectedValues,
			ActualValues,
			*Case.DescribeResult(
				TEXT("destructor value order"),
				FormatIntArray(ExpectedValues),
				FormatIntArray(ActualValues))));

		TSet<int32> ConstructedIds;
		TSet<int32> DestructedIds;
		for (const FNativeLifecycleEntry& Entry :
			Lifecycle.GetEntries())
		{
			if (Entry.Event
				== ENativeLifecycleEvent::ValueConstruct)
			{
				ASSERT_THAT(IsFalse(
					ConstructedIds.Contains(Entry.ObjectId),
					*Case.Describe(TEXT("partial destructor should allocate each native identity once"))));
				ConstructedIds.Add(Entry.ObjectId);
			}
			else if (Entry.Event
				== ENativeLifecycleEvent::Destruct)
			{
				ASSERT_THAT(IsTrue(
					ConstructedIds.Contains(Entry.ObjectId),
					*Case.Describe(TEXT("partial destructor should destroy only constructed identities"))));
				ASSERT_THAT(IsFalse(
					DestructedIds.Contains(Entry.ObjectId),
					*Case.Describe(TEXT("partial destructor should never destroy an identity twice"))));
				DestructedIds.Add(Entry.ObjectId);
			}
		}
		ASSERT_THAT(AreEqual(
			ConstructedIds.Num(),
			DestructedIds.Num(),
			*Case.Describe(TEXT("partial destructor constructed and destroyed identity sets should match"))));
	}

	void ExecuteRecovery(
		const FNativeCaseContext& Case,
		asIScriptContext& Context,
		asIScriptFunction& Recovery,
		FNativeLifecycleRecorder& Lifecycle,
		const FDestructorPartialState& State)
	{
		const int32 StageCountBeforeRecovery =
			State.ReachedStages.Num();
		const int32 ConstructCountBeforeRecovery =
			Lifecycle.Num(ENativeLifecycleEvent::ValueConstruct);
		const int32 DestructCountBeforeRecovery =
			Lifecycle.Num(ENativeLifecycleEvent::Destruct);
		ASSERT_THAT(IsTrue(Context.Prepare(&Recovery) >= 0,
			*Case.Describe(TEXT("partial destructor context should prepare same-context recovery"))));
		ASSERT_THAT(AreEqual(
			static_cast<int32>(asEXECUTION_FINISHED),
			Context.Execute(),
			*Case.Describe(TEXT("partial destructor same-context recovery should finish"))));
		ASSERT_THAT(AreEqual(
			909,
			static_cast<int32>(Context.GetReturnDWord()),
			*Case.Describe(TEXT("partial destructor recovery should return its sentinel"))));
		ASSERT_THAT(AreEqual(
			asSUCCESS,
			Context.Unprepare(),
			*Case.Describe(TEXT("partial destructor recovery should unprepare cleanly"))));
		ASSERT_THAT(AreEqual(
			StageCountBeforeRecovery,
			State.ReachedStages.Num(),
			*Case.Describe(TEXT("partial destructor recovery should replay no primary stage"))));
		ASSERT_THAT(AreEqual(
			ConstructCountBeforeRecovery + 1,
			Lifecycle.Num(ENativeLifecycleEvent::ValueConstruct),
			*Case.Describe(TEXT("partial destructor recovery should construct one independent identity"))));
		ASSERT_THAT(AreEqual(
			DestructCountBeforeRecovery + 1,
			Lifecycle.Num(ENativeLifecycleEvent::Destruct),
			*Case.Describe(TEXT("partial destructor recovery should destroy its identity once"))));
		ASSERT_THAT(AreEqual(
			0,
			Lifecycle.GetLiveObjectCount(),
			*Case.Describe(TEXT("partial destructor recovery should leave no live identity"))));
	}

	void RunCell(
		const FTopologyCase& Topology,
		const FBoundaryCase& Boundary,
		const FExitCase& Exit)
	{
		using namespace AngelscriptNativeTestSupport;

		const FNativeCaseContext Case(MakeNativeCaseId(
			"LANG-DTOR-PARTIAL",
			{
				ANSI_TO_TCHAR(Topology.CatalogName),
				ANSI_TO_TCHAR(Boundary.CatalogName),
				ANSI_TO_TCHAR(Exit.CatalogName),
			}));
		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };
		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine,
			*Case.Describe(TEXT("partial destructor should create an isolated raw SDK engine"))));
		if (ScriptEngine == nullptr)
		{
			return;
		}

		FNativeLifecycleRecorder Lifecycle;
		FDestructorPartialState State;
		State.Reset(Lifecycle, Exit.Kind);
		ASSERT_THAT(IsTrue(RegisterDestructorPartialBridge(
			*ScriptEngine,
			State),
			*Case.Describe(TEXT("partial destructor should register its boundary bridge"))));
		ASSERT_THAT(IsTrue(RegisterNativeCaseReference(
			*ScriptEngine,
			&Lifecycle),
			*Case.Describe(TEXT("partial destructor should register tracked native references"))));

		const FString ModuleName = FString::Printf(
			TEXT("DestructorPartial_%hs_%hs_%hs"),
			Topology.CatalogName,
			Boundary.CatalogName,
			Exit.CatalogName);
		const FString Source =
			BuildDestructorPartialSource(
				Topology,
				Boundary);
		Engine.ResetMessages();
		Lifecycle.Reset();
		State.Reset(Lifecycle, Exit.Kind);
		asIScriptModule* Module = nullptr;
		const int CompileResult = CompileAndReport(
			*TestRunner,
			*ScriptEngine,
			Case.GetId(),
			ModuleName,
			Source,
			Module);
		const bool bKnownTransferRestriction =
			Boundary.Stage == 5 && Topology.bHasTransfer;
		if (bKnownTransferRestriction)
		{
			ASSERT_THAT(IsTrue(
				CompileResult < 0,
				*Case.Describe(TEXT("current fork should reject automatic transfer special-member syntax"))));
			ASSERT_THAT(IsTrue(
				Engine.GetMessages().Entries.ContainsByPredicate(
					[](const FNativeMessageEntry& Entry)
					{
						return Entry.Type == asMSGTYPE_ERROR;
					}),
				*Case.Describe(TEXT("transfer rejection should publish a compiler diagnostic"))));
			if (Module != nullptr)
			{
				const FTCHARToUTF8 ModuleNameUtf8(*ModuleName);
				ScriptEngine->DiscardModule(ModuleNameUtf8.Get());
			}
			ASSERT_THAT(IsNull(
				ScriptEngine->GetModule(
					FTCHARToUTF8(*ModuleName).Get(),
					asGM_ONLY_IF_EXISTS),
				*Case.Describe(TEXT("rejected transfer module should discard without residual metadata"))));
			ASSERT_THAT(AreEqual(
				0,
				Lifecycle.GetLiveObjectCount(),
				*Case.Describe(TEXT("rejected transfer syntax should allocate no native identity"))));
			return;
		}
		ASSERT_THAT(IsTrue(
			CompileResult >= 0,
			*Case.Describe(TEXT("partial destructor generated source should compile"))));
		ASSERT_THAT(IsNotNull(Module,
			*Case.Describe(TEXT("partial destructor generated source should publish its module"))));
		if (Module == nullptr)
		{
			return;
		}

		VerifyCompiledMetadata(
			Case,
			Topology,
			*Module);
		asIScriptFunction* const Entry =
			Module->GetFunctionByDecl(
				"int RunDestructorPartial()");
		asIScriptFunction* const Recovery =
			Module->GetFunctionByDecl(
				"int RunDestructorPartialRecovery()");
		ASSERT_THAT(IsNotNull(Entry,
			*Case.Describe(TEXT("partial destructor module should expose its primary entry"))));
		ASSERT_THAT(IsNotNull(Recovery,
			*Case.Describe(TEXT("partial destructor module should expose recovery"))));
		if (Entry == nullptr || Recovery == nullptr)
		{
			return;
		}

		asIScriptContext* const Context =
			ScriptEngine->CreateContext();
		ASSERT_THAT(IsNotNull(Context,
			*Case.Describe(TEXT("partial destructor should create a reusable context"))));
		if (Context == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsTrue(Context->Prepare(Entry) >= 0,
			*Case.Describe(TEXT("partial destructor context should prepare its entry"))));
		const int32 ExecutionState = Context->Execute();
		VerifyExitState(
			Case,
			Topology,
			Boundary,
			Exit,
			State,
			*Context,
			ExecutionState);
		VerifyBoundarySnapshot(
			Case,
			Topology,
			Boundary,
			State);
		ASSERT_THAT(AreEqual(
			asSUCCESS,
			Context->Unprepare(),
			*Case.Describe(TEXT("partial destructor termination should unprepare and release reached storage"))));
		VerifyPrimaryCleanup(
			Case,
			Boundary,
			Lifecycle);
		ExecuteRecovery(
			Case,
			*Context,
			*Recovery,
			Lifecycle,
			State);
		Context->Release();

		const FTCHARToUTF8 ModuleNameUtf8(*ModuleName);
		ASSERT_THAT(IsTrue(
			ScriptEngine->DiscardModule(
				ModuleNameUtf8.Get()) >= 0,
			*Case.Describe(TEXT("partial destructor module should discard cleanly"))));
		ASSERT_THAT(IsNull(ScriptEngine->GetModule(
			ModuleNameUtf8.Get(),
			asGM_ONLY_IF_EXISTS),
			*Case.Describe(TEXT("partial destructor discard should remove module metadata"))));
		ASSERT_THAT(AreEqual(
			0,
			Lifecycle.GetLiveObjectCount(),
			*Case.Describe(TEXT("partial destructor engine isolation should retain no native object"))));
	}

public:
	TEST_METHOD(TopologiesByBoundaryAndExit)
	{
		AS_NATIVE_PRODUCT("LANG-DTOR-PARTIAL",
			AngelscriptNativeTestSupport::ENativeEvidence::Runtime
				| AngelscriptNativeTestSupport::ENativeEvidence::Diagnostic
				| AngelscriptNativeTestSupport::ENativeEvidence::Lifecycle
				| AngelscriptNativeTestSupport::ENativeEvidence::Cleanup
				| AngelscriptNativeTestSupport::ENativeEvidence::Isolation);

		for (const FTopologyCase& Topology : TopologyCases)
		{
			for (const FBoundaryCase& Boundary : BoundaryCases)
			{
				for (const FExitCase& Exit : ExitCases)
				{
					RunCell(
						Topology,
						Boundary,
						Exit);
				}
			}
		}
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
