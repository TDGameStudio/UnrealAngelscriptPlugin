#include "../../Support/AngelscriptNativeFixtureTestSupport.h"
#include "../../Support/AngelscriptNativeLanguageCaseTestSupport.h"

#include "CQTest.h"
#include "Misc/ScopeExit.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FConstructorVisibilityTests,
	"Angelscript.TestModule.AngelScriptSDK.Language.Constructors.Visibility",
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

	static constexpr asPWORD ConstructorVisibilityStateUserDataSlot =
		static_cast<asPWORD>(0x43544F5256495349ull);

	struct FVisibilityCase
	{
		const ANSICHAR* CatalogName;
		const TCHAR* Prefix;
	};

	struct FSiteCase
	{
		const ANSICHAR* CatalogName;
	};

	struct FSelectionCase
	{
		const ANSICHAR* CatalogName;
		int32 Marker;
	};

	inline static constexpr FVisibilityCase VisibilityCases[] =
	{
		{ "default", TEXT("") },
		{ "protected", TEXT("protected ") },
		{ "private", TEXT("private ") },
	};

	inline static constexpr FSiteCase SiteCases[] =
	{
		{ "owner" },
		{ "derived" },
		{ "unrelated" },
		{ "global" },
	};

	inline static constexpr FSelectionCase SelectionCases[] =
	{
		{ "exact", 101 },
		{ "promotion", 201 },
		{ "explicit_cast", 301 },
		{ "implicit_rejected", 401 },
		{ "ambiguous", 501 },
		{ "missing", 601 },
	};

	struct FConstructorVisibilityState
	{
		TArray<int32> SelectedMarkers;

		void Reset()
		{
			SelectedMarkers.Reset();
		}
	};

	static FConstructorVisibilityState* GetActiveState()
	{
		asIScriptContext* const Context = asGetActiveContext();
		return Context != nullptr
			? static_cast<FConstructorVisibilityState*>(
				Context->GetEngine()->GetUserData(
					ConstructorVisibilityStateUserDataSlot))
			: nullptr;
	}

	static void RecordConstructorVisibilitySelection(const int32 Marker)
	{
		if (FConstructorVisibilityState* const State = GetActiveState())
		{
			State->SelectedMarkers.Add(Marker);
		}
	}

	static bool RegisterConstructorVisibilityBridge(
		asIScriptEngine& ScriptEngine,
		FConstructorVisibilityState& State)
	{
		ScriptEngine.SetUserData(
			&State,
			ConstructorVisibilityStateUserDataSlot);
		const ASAutoCaller::FunctionCaller SelectionCaller =
			ASAutoCaller::MakeFunctionCaller(
				RecordConstructorVisibilitySelection);
		return ScriptEngine.RegisterGlobalFunction(
			"void RecordConstructorVisibilitySelection(int Marker)",
			asFUNCTION(RecordConstructorVisibilitySelection),
			asCALL_CDECL,
			*(asFunctionCaller*)&SelectionCaller) >= 0;
	}

	static bool IsVisibility(
		const FVisibilityCase& VisibilityCase,
		const ANSICHAR* CatalogName)
	{
		return FCStringAnsi::Strcmp(
			VisibilityCase.CatalogName,
			CatalogName) == 0;
	}

	static bool IsSite(
		const FSiteCase& SiteCase,
		const ANSICHAR* CatalogName)
	{
		return FCStringAnsi::Strcmp(
			SiteCase.CatalogName,
			CatalogName) == 0;
	}

	static bool IsSelection(
		const FSelectionCase& SelectionCase,
		const ANSICHAR* CatalogName)
	{
		return FCStringAnsi::Strcmp(
			SelectionCase.CatalogName,
			CatalogName) == 0;
	}

	static bool IsAccessible(
		const FVisibilityCase& VisibilityCase,
		const FSiteCase& SiteCase)
	{
		if (IsVisibility(VisibilityCase, "default"))
		{
			return true;
		}
		if (IsVisibility(VisibilityCase, "protected"))
		{
			return IsSite(SiteCase, "owner")
				|| IsSite(SiteCase, "derived");
		}
		return IsSite(SiteCase, "owner");
	}

	static bool IsDirectSelection(
		const FSelectionCase& SelectionCase)
	{
		return IsSelection(SelectionCase, "exact")
			|| IsSelection(SelectionCase, "promotion")
			|| IsSelection(SelectionCase, "explicit_cast");
	}

	static bool ExpectedBuild(
		const FVisibilityCase& VisibilityCase,
		const FSiteCase& SiteCase,
		const FSelectionCase& SelectionCase)
	{
		return IsDirectSelection(SelectionCase)
			&& IsAccessible(VisibilityCase, SiteCase);
	}

	static const ANSICHAR* CandidateType(
		const FSelectionCase& SelectionCase)
	{
		if (IsSelection(SelectionCase, "promotion")
			|| IsSelection(SelectionCase, "explicit_cast"))
		{
			return "int64";
		}
		if (IsSelection(SelectionCase, "missing"))
		{
			return "FVisibilityNoMatch";
		}
		return "int";
	}

	static FString ConstructorCallArgument(
		const FSelectionCase& SelectionCase)
	{
		if (IsSelection(SelectionCase, "promotion"))
		{
			return TEXT("int8(7)");
		}
		if (IsSelection(SelectionCase, "explicit_cast"))
		{
			return TEXT("int64(int8(7))");
		}
		return TEXT("int(7)");
	}

	static void AppendNoMatchType(FString& Source)
	{
		using namespace AngelscriptNativeTestSupport;

		AppendGeneratedAsLine(Source, TEXT("struct FVisibilityNoMatch"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tint Value = 0;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
	}

	static void AppendSelectedConstructor(
		FString& Source,
		const FVisibilityCase& VisibilityCase,
		const FSelectionCase& SelectionCase)
	{
		using namespace AngelscriptNativeTestSupport;

		const ANSICHAR* const Type = CandidateType(SelectionCase);
		if (IsSelection(SelectionCase, "ambiguous"))
		{
			AppendGeneratedAsLine(Source, FString::Printf(
				TEXT("\t%sFVisibilityTarget(int InValue, int Extra = 1)"),
				VisibilityCase.Prefix));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t\tValue = InValue + Extra;"));
			AppendGeneratedAsLine(Source, TEXT("\t\tObjectId = BeginNativeScriptLifecycle(Value);"));
			AppendGeneratedAsLine(Source, TEXT("\t\tRecordConstructorVisibilitySelection(501);"));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
			AppendGeneratedAsLine(Source);
			AppendGeneratedAsLine(Source, FString::Printf(
				TEXT("\t%sFVisibilityTarget(int InValue, float Extra = 1.0f)"),
				VisibilityCase.Prefix));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t\tValue = InValue + int(Extra);"));
			AppendGeneratedAsLine(Source, TEXT("\t\tObjectId = BeginNativeScriptLifecycle(Value);"));
			AppendGeneratedAsLine(Source, TEXT("\t\tRecordConstructorVisibilitySelection(502);"));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
			return;
		}

		AppendGeneratedAsLine(Source, FString::Printf(
			TEXT("\t%sFVisibilityTarget(%hs InValue)"),
			VisibilityCase.Prefix,
			Type));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		if (FCStringAnsi::Strcmp(Type, "FVisibilityNoMatch") == 0)
		{
			AppendGeneratedAsLine(Source, TEXT("\t\tValue = InValue.Value;"));
		}
		else
		{
			AppendGeneratedAsLine(Source, TEXT("\t\tValue = int(InValue);"));
		}
		AppendGeneratedAsLine(Source, TEXT("\t\tObjectId = BeginNativeScriptLifecycle(Value);"));
		AppendGeneratedAsLine(Source, FString::Printf(
			TEXT("\t\tRecordConstructorVisibilitySelection(%d);"),
			SelectionCase.Marker));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
	}

	static void AppendOwnerExercise(
		FString& Source,
		const FSelectionCase& SelectionCase)
	{
		using namespace AngelscriptNativeTestSupport;

		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("\tint ExerciseVisibility()"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		if (IsSelection(SelectionCase, "implicit_rejected"))
		{
			AppendGeneratedAsLine(Source, TEXT("\t\tConsumeVisibilityTarget(int(7));"));
			AppendGeneratedAsLine(Source, TEXT("\t\treturn 0;"));
		}
		else
		{
			AppendGeneratedAsLine(Source, FString::Printf(
				TEXT("\t\tFVisibilityTarget Selected = FVisibilityTarget(%s);"),
				*ConstructorCallArgument(SelectionCase)));
			AppendGeneratedAsLine(Source, TEXT("\t\treturn Selected.Value;"));
		}
		AppendGeneratedAsLine(Source, TEXT("\t}"));
	}

	static void AppendTargetType(
		FString& Source,
		const FVisibilityCase& VisibilityCase,
		const FSiteCase& SiteCase,
		const FSelectionCase& SelectionCase)
	{
		using namespace AngelscriptNativeTestSupport;

		AppendGeneratedAsLine(Source, TEXT("class FVisibilityTarget"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tint Value = 0;"));
		AppendGeneratedAsLine(Source, TEXT("\tint ObjectId = -1;"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("\tFVisibilityTarget()"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(Source, TEXT("\t\tValue = 1;"));
		AppendGeneratedAsLine(Source, TEXT("\t\tObjectId = BeginNativeScriptLifecycle(Value);"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source);
		AppendSelectedConstructor(
			Source,
			VisibilityCase,
			SelectionCase);
		if (IsSite(SiteCase, "owner"))
		{
			AppendOwnerExercise(Source, SelectionCase);
		}
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("\t~FVisibilityTarget()"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(Source, TEXT("\t\tEndNativeScriptLifecycle(ObjectId, Value);"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
	}

	static void AppendImplicitConsumer(FString& Source)
	{
		using namespace AngelscriptNativeTestSupport;

		AppendGeneratedAsLine(Source, TEXT("void ConsumeVisibilityTarget(FVisibilityTarget Value)"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
	}

	static void AppendDerivedSite(
		FString& Source,
		const FSelectionCase& SelectionCase)
	{
		using namespace AngelscriptNativeTestSupport;

		AppendGeneratedAsLine(Source, TEXT("class FVisibilityDerived : FVisibilityTarget"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tFVisibilityDerived()"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		if (IsSelection(SelectionCase, "implicit_rejected"))
		{
			AppendGeneratedAsLine(Source, TEXT("\t\tsuper();"));
			AppendGeneratedAsLine(Source, TEXT("\t\tConsumeVisibilityTarget(int(7));"));
		}
		else
		{
			AppendGeneratedAsLine(Source, FString::Printf(
				TEXT("\t\tsuper(%s);"),
				*ConstructorCallArgument(SelectionCase)));
		}
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
	}

	static void AppendUnrelatedSite(
		FString& Source,
		const FSelectionCase& SelectionCase)
	{
		using namespace AngelscriptNativeTestSupport;

		AppendGeneratedAsLine(Source, TEXT("class FVisibilityUnrelated"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tint ExerciseVisibility()"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		if (IsSelection(SelectionCase, "implicit_rejected"))
		{
			AppendGeneratedAsLine(Source, TEXT("\t\tConsumeVisibilityTarget(int(7));"));
			AppendGeneratedAsLine(Source, TEXT("\t\treturn 0;"));
		}
		else
		{
			AppendGeneratedAsLine(Source, FString::Printf(
				TEXT("\t\tFVisibilityTarget Selected = FVisibilityTarget(%s);"),
				*ConstructorCallArgument(SelectionCase)));
			AppendGeneratedAsLine(Source, TEXT("\t\treturn Selected.Value;"));
		}
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
	}

	static void AppendEntryFunction(
		FString& Source,
		const FSiteCase& SiteCase,
		const FSelectionCase& SelectionCase)
	{
		using namespace AngelscriptNativeTestSupport;

		AppendGeneratedAsLine(Source, TEXT("int RunConstructorVisibility()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		if (IsSite(SiteCase, "owner"))
		{
			AppendGeneratedAsLine(Source, TEXT("\tFVisibilityTarget Owner = FVisibilityTarget();"));
			AppendGeneratedAsLine(Source, TEXT("\treturn Owner.ExerciseVisibility();"));
		}
		else if (IsSite(SiteCase, "derived"))
		{
			AppendGeneratedAsLine(Source, TEXT("\tFVisibilityDerived Selected = FVisibilityDerived();"));
			AppendGeneratedAsLine(
				Source,
				IsSelection(SelectionCase, "implicit_rejected")
					? TEXT("\treturn 0;")
					: TEXT("\treturn Selected.Value;"));
		}
		else if (IsSite(SiteCase, "unrelated"))
		{
			AppendGeneratedAsLine(Source, TEXT("\tFVisibilityUnrelated Caller = FVisibilityUnrelated();"));
			AppendGeneratedAsLine(Source, TEXT("\treturn Caller.ExerciseVisibility();"));
		}
		else if (IsSelection(SelectionCase, "implicit_rejected"))
		{
			AppendGeneratedAsLine(Source, TEXT("\tConsumeVisibilityTarget(int(7));"));
			AppendGeneratedAsLine(Source, TEXT("\treturn 0;"));
		}
		else
		{
			AppendGeneratedAsLine(Source, FString::Printf(
				TEXT("\tFVisibilityTarget Selected = FVisibilityTarget(%s);"),
				*ConstructorCallArgument(SelectionCase)));
			AppendGeneratedAsLine(Source, TEXT("\treturn Selected.Value;"));
		}
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("int RunConstructorVisibilityRecovery()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\treturn 97;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
	}

	static FString BuildConstructorVisibilitySource(
		const FVisibilityCase& VisibilityCase,
		const FSiteCase& SiteCase,
		const FSelectionCase& SelectionCase)
	{
		FString Source;
		AppendNoMatchType(Source);
		AppendTargetType(
			Source,
			VisibilityCase,
			SiteCase,
			SelectionCase);
		AppendImplicitConsumer(Source);
		if (IsSite(SiteCase, "derived"))
		{
			AppendDerivedSite(Source, SelectionCase);
		}
		else if (IsSite(SiteCase, "unrelated"))
		{
			AppendUnrelatedSite(Source, SelectionCase);
		}
		AppendEntryFunction(Source, SiteCase, SelectionCase);
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

	static bool HasLocatedDiagnostic(
		const FNativeTestEngine& Engine,
		const TCHAR* ExpectedText)
	{
		return Engine.GetMessages().Entries.ContainsByPredicate(
			[ExpectedText](const FNativeMessageEntry& Entry)
			{
				return Entry.Type == asMSGTYPE_ERROR
					&& Entry.Row > 0
					&& Entry.Column > 0
					&& Entry.Message.Contains(ExpectedText);
			});
	}

	static const TCHAR* ExpectedDiagnostic(
		const FVisibilityCase& VisibilityCase,
		const FSelectionCase& SelectionCase)
	{
		if (IsSelection(SelectionCase, "ambiguous"))
		{
			return TEXT("Multiple matching signatures");
		}
		if (IsSelection(SelectionCase, "missing")
			|| IsSelection(SelectionCase, "implicit_rejected"))
		{
			return TEXT("No matching signatures");
		}
		return IsVisibility(VisibilityCase, "private")
			? TEXT("Illegal call to private method")
			: TEXT("Illegal call to protected method");
	}

	static asIScriptFunction* FindSelectedConstructor(
		asITypeInfo& Type,
		asIScriptModule& Module,
		const FSelectionCase& SelectionCase)
	{
		const int32 ExpectedTypeId =
			Module.GetTypeIdByDecl(CandidateType(SelectionCase));
		for (asUINT Index = 0; Index < Type.GetBehaviourCount(); ++Index)
		{
			asEBehaviours Behaviour = asBEHAVE_CONSTRUCT;
			asIScriptFunction* const Function =
				Type.GetBehaviourByIndex(Index, &Behaviour);
			if (Function == nullptr
				|| Behaviour != asBEHAVE_CONSTRUCT
				|| Function->GetParamCount() != 1)
			{
				continue;
			}
			int TypeId = asTYPEID_VOID;
			if (Function->GetParam(0, &TypeId) >= 0
				&& TypeId == ExpectedTypeId)
			{
				return Function;
			}
		}
		return nullptr;
	}

	void VerifyMetadata(
		const FNativeCaseContext& Case,
		const FVisibilityCase& VisibilityCase,
		const FSelectionCase& SelectionCase,
		asIScriptEngine& ScriptEngine,
		asIScriptModule& Module)
	{
		asITypeInfo* const Type =
			Module.GetTypeInfoByName("FVisibilityTarget");
		ASSERT_THAT(IsNotNull(Type,
			*Case.Describe(TEXT("constructor-visibility module should publish its target type"))));
		if (Type == nullptr)
		{
			return;
		}
		asIScriptFunction* const Constructor =
			FindSelectedConstructor(
				*Type,
				Module,
				SelectionCase);
		ASSERT_THAT(IsNotNull(Constructor,
			*Case.Describe(TEXT("constructor-visibility metadata should resolve the selected constructor"))));
		if (Constructor == nullptr)
		{
			return;
		}

		ASSERT_THAT(AreEqual(
			IsVisibility(VisibilityCase, "private"),
			Constructor->IsPrivate(),
			*Case.Describe(TEXT("constructor metadata should preserve private visibility"))));
		ASSERT_THAT(AreEqual(
			IsVisibility(VisibilityCase, "protected"),
			Constructor->IsProtected(),
			*Case.Describe(TEXT("constructor metadata should preserve protected visibility"))));
		int TypeId = asTYPEID_VOID;
		asDWORD Flags = asTM_NONE;
		const char* Name = nullptr;
		ASSERT_THAT(IsTrue(Constructor->GetParam(
			0,
			&TypeId,
			&Flags,
			&Name) >= 0,
			*Case.Describe(TEXT("selected constructor should expose its parameter metadata"))));
		ASSERT_THAT(AreEqual(
			Module.GetTypeIdByDecl(CandidateType(SelectionCase)),
			TypeId,
			*Case.Describe(TEXT("selected constructor should preserve its parameter type"))));
		const asDWORD ExpectedFlags = asTM_CONST;
		if (Flags != ExpectedFlags)
		{
			const char* const TypeDeclarationAnsi =
				ScriptEngine.GetTypeDeclaration(TypeId);
			const FString TypeDeclaration = TypeDeclarationAnsi != nullptr
				? UTF8_TO_TCHAR(TypeDeclarationAnsi)
				: TEXT("<null>");
			const char* const ConstructorDeclarationAnsi =
				Constructor->GetDeclaration();
			const FString ConstructorDeclaration =
				ConstructorDeclarationAnsi != nullptr
					? UTF8_TO_TCHAR(ConstructorDeclarationAnsi)
					: TEXT("<null>");
			UE_LOG(LogTemp, Display,
				TEXT("[AS-CTOR-VISIBILITY-PARAM] Id=%s TypeId=%d Type='%s' Flags=0x%X ExpectedFlags=0x%X Name='%s' Declaration='%s'"),
				*Case.GetId(),
				TypeId,
				*TypeDeclaration,
				static_cast<uint32>(Flags),
				static_cast<uint32>(ExpectedFlags),
				Name != nullptr ? UTF8_TO_TCHAR(Name) : TEXT("<null>"),
				*ConstructorDeclaration);
		}
		ASSERT_THAT(AreEqual(
			ExpectedFlags,
			Flags,
			*Case.Describe(TEXT("selected constructor parameter should publish const metadata"))));
		ASSERT_THAT(IsTrue(Name != nullptr
			&& FCStringAnsi::Strcmp(Name, "InValue") == 0,
			*Case.Describe(TEXT("selected constructor should preserve its parameter name"))));
		ASSERT_THAT(IsNotNull(
			Module.GetFunctionByDecl("int RunConstructorVisibility()"),
			*Case.Describe(TEXT("constructor-visibility module should publish its exact entry"))));
	}

	void VerifyLifecycle(
		const FNativeCaseContext& Case,
		const FNativeLifecycleRecorder& Lifecycle)
	{
		using namespace AngelscriptNativeTestSupport;

		ASSERT_THAT(AreEqual(0, Lifecycle.GetLiveObjectCount(),
			*Case.Describe(TEXT("constructor-visibility execution should leave no live object"))));
		const int32 ConstructionCount =
			Lifecycle.Num(ENativeLifecycleEvent::DefaultConstruct)
			+ Lifecycle.Num(ENativeLifecycleEvent::ValueConstruct)
			+ Lifecycle.Num(ENativeLifecycleEvent::CopyConstruct);
		ASSERT_THAT(IsTrue(ConstructionCount > 0,
			*Case.Describe(TEXT("constructor-visibility execution should construct a real script object"))));
		ASSERT_THAT(AreEqual(
			ConstructionCount,
			Lifecycle.Num(ENativeLifecycleEvent::Destruct),
			*Case.Describe(TEXT("constructor-visibility script objects should balance destruction"))));

		TSet<int32> ConstructedIds;
		TSet<int32> DestructedIds;
		for (const FNativeLifecycleEntry& Entry : Lifecycle.GetEntries())
		{
			if (Entry.Event == ENativeLifecycleEvent::DefaultConstruct
				|| Entry.Event == ENativeLifecycleEvent::ValueConstruct
				|| Entry.Event == ENativeLifecycleEvent::CopyConstruct)
			{
				ConstructedIds.Add(Entry.ObjectId);
			}
			else if (Entry.Event == ENativeLifecycleEvent::Destruct)
			{
				ASSERT_THAT(IsTrue(ConstructedIds.Contains(Entry.ObjectId),
					*Case.Describe(TEXT("constructor-visibility destructor should identify constructed storage"))));
				ASSERT_THAT(IsFalse(DestructedIds.Contains(Entry.ObjectId),
					*Case.Describe(TEXT("constructor-visibility storage should not be destroyed twice"))));
				DestructedIds.Add(Entry.ObjectId);
			}
		}
		ASSERT_THAT(AreEqual(ConstructedIds.Num(), DestructedIds.Num(),
			*Case.Describe(TEXT("constructor-visibility lifecycle identities should balance"))));
	}

	void ExecuteModule(
		const FNativeCaseContext& Case,
		const FVisibilityCase& VisibilityCase,
		const FSelectionCase& SelectionCase,
		asIScriptEngine& ScriptEngine,
		asIScriptModule& Module,
		FConstructorVisibilityState& State,
		FNativeLifecycleRecorder& Lifecycle)
	{
		using namespace AngelscriptNativeTestSupport;

		VerifyMetadata(
			Case,
			VisibilityCase,
			SelectionCase,
			ScriptEngine,
			Module);
		asIScriptFunction* const Entry =
			Module.GetFunctionByDecl("int RunConstructorVisibility()");
		asIScriptFunction* const Recovery =
			Module.GetFunctionByDecl("int RunConstructorVisibilityRecovery()");
		if (Entry == nullptr || Recovery == nullptr)
		{
			return;
		}

		asIScriptContext* const Context = ScriptEngine.CreateContext();
		ASSERT_THAT(IsNotNull(Context,
			*Case.Describe(TEXT("constructor-visibility cell should create a reusable context"))));
		if (Context == nullptr)
		{
			return;
		}
		ASSERT_THAT(AreEqual(
			static_cast<int32>(asEXECUTION_FINISHED),
			PrepareAndExecute(Context, Entry),
			*Case.Describe(TEXT("constructor-visibility entry should finish"))));
		ASSERT_THAT(AreEqual(
			7,
			static_cast<int32>(Context->GetReturnDWord()),
			*Case.Describe(TEXT("constructor-visibility entry should preserve the selected value"))));
		ASSERT_THAT(AreEqual(
			asSUCCESS,
			Context->Unprepare(),
			*Case.Describe(TEXT("constructor-visibility context should release all objects"))));
		ASSERT_THAT(AreEqual(1, State.SelectedMarkers.Num(),
			*Case.Describe(TEXT("constructor-visibility entry should execute one selected body"))));
		if (State.SelectedMarkers.Num() == 1)
		{
			ASSERT_THAT(AreEqual(
				SelectionCase.Marker,
				State.SelectedMarkers[0],
				*Case.Describe(TEXT("constructor-visibility marker should identify the selected overload"))));
		}
		VerifyLifecycle(Case, Lifecycle);

		const int32 MarkerCountBeforeRecovery =
			State.SelectedMarkers.Num();
		ASSERT_THAT(IsTrue(Context->Prepare(Recovery) >= 0,
			*Case.Describe(TEXT("constructor-visibility context should prepare recovery"))));
		ASSERT_THAT(AreEqual(
			static_cast<int32>(asEXECUTION_FINISHED),
			Context->Execute(),
			*Case.Describe(TEXT("constructor-visibility recovery should finish"))));
		ASSERT_THAT(AreEqual(
			97,
			static_cast<int32>(Context->GetReturnDWord()),
			*Case.Describe(TEXT("constructor-visibility recovery should return its sentinel"))));
		ASSERT_THAT(AreEqual(
			MarkerCountBeforeRecovery,
			State.SelectedMarkers.Num(),
			*Case.Describe(TEXT("constructor-visibility recovery should select no constructor"))));
		ASSERT_THAT(AreEqual(
			asSUCCESS,
			Context->Unprepare(),
			*Case.Describe(TEXT("constructor-visibility recovery should unprepare cleanly"))));
		Context->Release();
	}

	static const FSiteCase& LegalRecoverySite(
		const FVisibilityCase& VisibilityCase,
		const FSiteCase& OriginalSite)
	{
		if (IsVisibility(VisibilityCase, "private"))
		{
			return SiteCases[0];
		}
		if (IsVisibility(VisibilityCase, "protected")
			&& !IsAccessible(VisibilityCase, OriginalSite))
		{
			return SiteCases[1];
		}
		return OriginalSite;
	}

	void RunCell(
		const FVisibilityCase& VisibilityCase,
		const FSiteCase& SiteCase,
		const FSelectionCase& SelectionCase)
	{
		using namespace AngelscriptNativeTestSupport;

		const FNativeCaseContext Case(MakeNativeCaseId(
			"LANG-CTOR-VISIBILITY",
			{
				ANSI_TO_TCHAR(SelectionCase.CatalogName),
				ANSI_TO_TCHAR(SiteCase.CatalogName),
				ANSI_TO_TCHAR(VisibilityCase.CatalogName),
			}));
		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };
		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine,
			*Case.Describe(TEXT("constructor-visibility cell should create a raw SDK engine"))));
		if (ScriptEngine == nullptr)
		{
			return;
		}

		FConstructorVisibilityState State;
		FNativeLifecycleRecorder Lifecycle;
		ASSERT_THAT(IsTrue(RegisterConstructorVisibilityBridge(*ScriptEngine, State),
			*Case.Describe(TEXT("constructor-visibility cell should register its selection bridge"))));
		ASSERT_THAT(IsTrue(RegisterNativeScriptLifecycleBridge(*ScriptEngine, Lifecycle),
			*Case.Describe(TEXT("constructor-visibility cell should register script lifecycle callbacks"))));

		const FString ModuleName = FString::Printf(
			TEXT("ConstructorVisibility_%hs_%hs_%hs"),
			VisibilityCase.CatalogName,
			SiteCase.CatalogName,
			SelectionCase.CatalogName);
		const FString Source = BuildConstructorVisibilitySource(
			VisibilityCase,
			SiteCase,
			SelectionCase);
		Engine.ResetMessages();
		State.Reset();
		Lifecycle.Reset();
		asIScriptModule* Module = nullptr;
		const int BuildResult = CompileAndReport(
			*TestRunner,
			*ScriptEngine,
			Case.GetId(),
			ModuleName,
			Source,
			Module);
		const bool bExpectedBuild =
			ExpectedBuild(
				VisibilityCase,
				SiteCase,
				SelectionCase);
		ASSERT_THAT(AreEqual(
			bExpectedBuild,
			BuildResult >= 0,
			*Case.Describe(TEXT("constructor-visibility build result should match access and selection"))));

		if (bExpectedBuild)
		{
			ASSERT_THAT(IsNotNull(Module,
				*Case.Describe(TEXT("legal constructor-visibility cell should publish its module"))));
			if (Module != nullptr)
			{
				ExecuteModule(
					Case,
					VisibilityCase,
					SelectionCase,
					*ScriptEngine,
					*Module,
					State,
					Lifecycle);
			}
		}
		else
		{
			ASSERT_THAT(IsTrue(BuildResult < 0,
				*Case.Describe(TEXT("illegal constructor-visibility cell should fail to build"))));
			ASSERT_THAT(IsTrue(HasLocatedDiagnostic(
				Engine,
				ExpectedDiagnostic(
					VisibilityCase,
					SelectionCase)),
				*Case.Describe(TEXT("illegal constructor-visibility cell should own its located diagnostic"))));
			ASSERT_THAT(IsTrue(Module == nullptr
				|| Module->GetFunctionByDecl("int RunConstructorVisibility()") == nullptr,
				*Case.Describe(TEXT("failed constructor-visibility module should publish no callable entry"))));
			ASSERT_THAT(AreEqual(0, State.SelectedMarkers.Num(),
				*Case.Describe(TEXT("compile-time constructor rejection should execute no selected body"))));
			ASSERT_THAT(AreEqual(0, Lifecycle.GetEntries().Num(),
				*Case.Describe(TEXT("compile-time constructor rejection should construct no script object"))));

			const FTCHARToUTF8 ModuleNameUtf8(*ModuleName);
			ScriptEngine->DiscardModule(ModuleNameUtf8.Get());
			const FSiteCase& RecoverySite =
				LegalRecoverySite(VisibilityCase, SiteCase);
			const FSelectionCase& RecoverySelection =
				SelectionCases[0];
			const FString RecoverySource =
				BuildConstructorVisibilitySource(
					VisibilityCase,
					RecoverySite,
					RecoverySelection);
			Engine.ResetMessages();
			State.Reset();
			Lifecycle.Reset();
			Module = nullptr;
			ASSERT_THAT(IsTrue(CompileAndReport(
				*TestRunner,
				*ScriptEngine,
				Case.GetId() + TEXT("-RECOVERY"),
				ModuleName,
				RecoverySource,
				Module) >= 0,
				*Case.Describe(TEXT("constructor-visibility rejection should allow legal same-name recovery"))));
			ASSERT_THAT(IsNotNull(Module,
				*Case.Describe(TEXT("constructor-visibility recovery should publish its module"))));
			if (Module != nullptr)
			{
				ExecuteModule(
					Case,
					VisibilityCase,
					RecoverySelection,
					*ScriptEngine,
					*Module,
					State,
					Lifecycle);
			}
		}

		const FTCHARToUTF8 ModuleNameUtf8(*ModuleName);
		ScriptEngine->DiscardModule(ModuleNameUtf8.Get());
		ASSERT_THAT(IsNull(ScriptEngine->GetModule(
			ModuleNameUtf8.Get(),
			asGM_ONLY_IF_EXISTS),
			*Case.Describe(TEXT("constructor-visibility module should discard cleanly"))));
		ASSERT_THAT(AreEqual(0, Lifecycle.GetLiveObjectCount(),
			*Case.Describe(TEXT("constructor-visibility module discard should leave no live object"))));
	}

public:
	TEST_METHOD(VisibilityBySiteAndSelection)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT("LANG-CTOR-VISIBILITY",
			ENativeEvidence::Compile
				| ENativeEvidence::Diagnostic
				| ENativeEvidence::Runtime
				| ENativeEvidence::Metadata
				| ENativeEvidence::Lifecycle
				| ENativeEvidence::Cleanup);

		for (const FVisibilityCase& VisibilityCase : VisibilityCases)
		{
			for (const FSiteCase& SiteCase : SiteCases)
			{
				for (const FSelectionCase& SelectionCase : SelectionCases)
				{
					RunCell(
						VisibilityCase,
						SiteCase,
						SelectionCase);
				}
			}
		}
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
