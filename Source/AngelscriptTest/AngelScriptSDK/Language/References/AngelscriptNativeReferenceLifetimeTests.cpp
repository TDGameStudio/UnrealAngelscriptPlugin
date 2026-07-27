#include "AngelscriptNativeReferenceTestSupport.h"

#include "CQTest.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FReferenceLifetimeTests,
	"Angelscript.TestModule.AngelScriptSDK.Language.References.Lifetime",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	using FNativeCaseContext =
		AngelscriptNativeTestSupport::FNativeCaseContext;
	using FNativeTestEngine =
		AngelscriptNativeTestSupport::FNativeTestEngine;
	using FReferenceRoot =
		AngelscriptNativeReferenceTestSupport::FReferenceRoot;
	using FReferenceState =
		AngelscriptNativeReferenceTestSupport::FReferenceState;

	enum class EOwnerState : uint8
	{
		OwnerLive,
		ScopeExit,
		ReturnedAlias,
		ModuleRetained,
		ModuleDiscarded,
		ContextRetained,
		ContextReleased,
		GCCycle,
	};

	enum class EReferenceState : uint8
	{
		None,
		OneAlias,
		MultipleAliases,
		Cycle,
		WeakFlag,
	};

	struct FOwnerCase
	{
		const ANSICHAR* CatalogName;
		EOwnerState State;
	};

	struct FReferenceCase
	{
		const ANSICHAR* CatalogName;
		EReferenceState State;
	};

	struct FObservationCase
	{
		const ANSICHAR* CatalogName;
	};

	struct FGCStatisticsSnapshot
	{
		asUINT CurrentSize = 0;
		asUINT TotalDestroyed = 0;
		asUINT TotalDetected = 0;
		asUINT NewObjects = 0;
		asUINT TotalNewDestroyed = 0;
	};

	inline static constexpr FOwnerCase OwnerCases[] =
	{
		{ "owner_live", EOwnerState::OwnerLive },
		{ "scope_exit", EOwnerState::ScopeExit },
		{ "returned_alias", EOwnerState::ReturnedAlias },
		{ "module_retained", EOwnerState::ModuleRetained },
		{ "module_discarded", EOwnerState::ModuleDiscarded },
		{ "context_retained", EOwnerState::ContextRetained },
		{ "context_released", EOwnerState::ContextReleased },
		{ "gc_cycle", EOwnerState::GCCycle },
	};

	inline static constexpr FReferenceCase ReferenceCases[] =
	{
		{ "none", EReferenceState::None },
		{ "one_alias", EReferenceState::OneAlias },
		{
			"multiple_aliases",
			EReferenceState::MultipleAliases,
		},
		{ "cycle", EReferenceState::Cycle },
		{ "weak_flag", EReferenceState::WeakFlag },
	};

	inline static constexpr FObservationCase ObservationCases[] =
	{
		{ "identity" },
		{ "refcount" },
		{ "weak_flag" },
		{ "destruction" },
		{ "gc_stats" },
	};

	struct FGCLifetimeNode
	{
		inline static asIScriptEngine* ScriptEngine = nullptr;
		inline static int32 NextIdentity = 1;
		inline static int32 Created = 0;
		inline static int32 Destroyed = 0;
		inline static int32 LiveCount = 0;
		inline static int32 AddRefCalls = 0;
		inline static int32 ReleaseCalls = 0;
		inline static TArray<int32> CreatedIdentities;
		inline static TArray<int32> DestroyedIdentities;

		FGCLifetimeNode()
			: Identity(NextIdentity++)
			, WeakFlag(asCreateLockableSharedBool())
		{
			++Created;
			++LiveCount;
			CreatedIdentities.Add(Identity);
		}

		~FGCLifetimeNode()
		{
			if (Peer != nullptr)
			{
				FGCLifetimeNode* const Previous = Peer;
				Peer = nullptr;
				Previous->Release();
			}
			if (WeakFlag != nullptr)
			{
				WeakFlag->Set(true);
				WeakFlag->Release();
				WeakFlag = nullptr;
			}
			++Destroyed;
			--LiveCount;
			DestroyedIdentities.Add(Identity);
		}

		void AddRef()
		{
			++ReferenceCount;
			++AddRefCalls;
		}

		void Release()
		{
			++ReleaseCalls;
			if (--ReferenceCount == 0)
			{
				delete this;
			}
		}

		int32 GetReferenceCount() const
		{
			return ReferenceCount;
		}

		void SetGCFlag()
		{
			bGCFlag = true;
		}

		bool GetGCFlag() const
		{
			return bGCFlag;
		}

		void Link(FGCLifetimeNode* const Other)
		{
			if (Peer == Other)
			{
				return;
			}
			if (Peer != nullptr)
			{
				FGCLifetimeNode* const Previous = Peer;
				Peer = nullptr;
				Previous->Release();
			}
			Peer = Other;
			if (Peer != nullptr)
			{
				Peer->AddRef();
			}
		}

		void EnumReferences()
		{
			if (Peer != nullptr
				&& ScriptEngine != nullptr)
			{
				ScriptEngine->GCEnumCallback(Peer);
			}
		}

		void ReleaseAllReferences()
		{
			if (Peer != nullptr)
			{
				FGCLifetimeNode* const Previous = Peer;
				Peer = nullptr;
				Previous->Release();
			}
		}

		asILockableSharedBool* GetWeakFlag() const
		{
			return WeakFlag;
		}

		int32 Identity = INDEX_NONE;
		int32 ReferenceCount = 1;
		bool bGCFlag = false;
		FGCLifetimeNode* Peer = nullptr;
		asILockableSharedBool* WeakFlag = nullptr;
	};

	static void ResetGCNodeState()
	{
		FGCLifetimeNode::NextIdentity = 1;
		FGCLifetimeNode::Created = 0;
		FGCLifetimeNode::Destroyed = 0;
		FGCLifetimeNode::LiveCount = 0;
		FGCLifetimeNode::AddRefCalls = 0;
		FGCLifetimeNode::ReleaseCalls = 0;
		FGCLifetimeNode::CreatedIdentities.Reset();
		FGCLifetimeNode::DestroyedIdentities.Reset();
	}

	static void GCNodeAddRef(
		FGCLifetimeNode* const Object)
	{
		if (Object != nullptr)
		{
			Object->AddRef();
		}
	}

	static void GCNodeRelease(
		FGCLifetimeNode* const Object)
	{
		if (Object != nullptr)
		{
			Object->Release();
		}
	}

	static int GCNodeGetRefCount(
		FGCLifetimeNode* const Object)
	{
		return Object != nullptr
			? Object->GetReferenceCount()
			: 0;
	}

	static void GCNodeSetFlag(
		FGCLifetimeNode* const Object)
	{
		if (Object != nullptr)
		{
			Object->SetGCFlag();
		}
	}

	static bool GCNodeGetFlag(
		FGCLifetimeNode* const Object)
	{
		return Object != nullptr
			&& Object->GetGCFlag();
	}

	static void GCNodeEnumReferences(
		FGCLifetimeNode* const Object,
		int&)
	{
		if (Object != nullptr)
		{
			Object->EnumReferences();
		}
	}

	static void GCNodeReleaseReferences(
		FGCLifetimeNode* const Object,
		int&)
	{
		if (Object != nullptr)
		{
			Object->ReleaseAllReferences();
		}
	}

	static asILockableSharedBool* GCNodeGetWeakFlag(
		FGCLifetimeNode* const Object)
	{
		return Object != nullptr
			? Object->GetWeakFlag()
			: nullptr;
	}

	static bool RegisterGCNode(
		asIScriptEngine& ScriptEngine,
		asITypeInfo*& OutType)
	{
		FGCLifetimeNode::ScriptEngine =
			&ScriptEngine;
		const ASAutoCaller::FunctionCaller AddRefCaller =
			ASAutoCaller::MakeFunctionCaller(
				GCNodeAddRef);
		const ASAutoCaller::FunctionCaller ReleaseCaller =
			ASAutoCaller::MakeFunctionCaller(
				GCNodeRelease);
		const ASAutoCaller::FunctionCaller GetRefCountCaller =
			ASAutoCaller::MakeFunctionCaller(
				GCNodeGetRefCount);
		const ASAutoCaller::FunctionCaller SetFlagCaller =
			ASAutoCaller::MakeFunctionCaller(
				GCNodeSetFlag);
		const ASAutoCaller::FunctionCaller GetFlagCaller =
			ASAutoCaller::MakeFunctionCaller(
				GCNodeGetFlag);
		const ASAutoCaller::FunctionCaller EnumCaller =
			ASAutoCaller::MakeFunctionCaller(
				GCNodeEnumReferences);
		const ASAutoCaller::FunctionCaller ReleaseRefsCaller =
			ASAutoCaller::MakeFunctionCaller(
				GCNodeReleaseReferences);
		const ASAutoCaller::FunctionCaller WeakFlagCaller =
			ASAutoCaller::MakeFunctionCaller(
				GCNodeGetWeakFlag);
		const bool bRegistered =
			ScriptEngine.RegisterObjectType(
				"FGCLifetimeNode",
				0,
				asOBJ_REF | asOBJ_GC) >= 0
			&& ScriptEngine.RegisterObjectBehaviour(
				"FGCLifetimeNode",
				asBEHAVE_ADDREF,
				"void f()",
				asFUNCTION(GCNodeAddRef),
				asCALL_CDECL_OBJFIRST,
				*(asFunctionCaller*)&AddRefCaller) >= 0
			&& ScriptEngine.RegisterObjectBehaviour(
				"FGCLifetimeNode",
				asBEHAVE_RELEASE,
				"void f()",
				asFUNCTION(GCNodeRelease),
				asCALL_CDECL_OBJFIRST,
				*(asFunctionCaller*)&ReleaseCaller) >= 0
			&& ScriptEngine.RegisterObjectBehaviour(
				"FGCLifetimeNode",
				asBEHAVE_GETREFCOUNT,
				"int f()",
				asFUNCTION(GCNodeGetRefCount),
				asCALL_CDECL_OBJFIRST,
				*(asFunctionCaller*)&GetRefCountCaller) >= 0
			&& ScriptEngine.RegisterObjectBehaviour(
				"FGCLifetimeNode",
				asBEHAVE_SETGCFLAG,
				"void f()",
				asFUNCTION(GCNodeSetFlag),
				asCALL_CDECL_OBJFIRST,
				*(asFunctionCaller*)&SetFlagCaller) >= 0
			&& ScriptEngine.RegisterObjectBehaviour(
				"FGCLifetimeNode",
				asBEHAVE_GETGCFLAG,
				"bool f()",
				asFUNCTION(GCNodeGetFlag),
				asCALL_CDECL_OBJFIRST,
				*(asFunctionCaller*)&GetFlagCaller) >= 0
			&& ScriptEngine.RegisterObjectBehaviour(
				"FGCLifetimeNode",
				asBEHAVE_ENUMREFS,
				"void f(int& in gcCycle)",
				asFUNCTION(GCNodeEnumReferences),
				asCALL_CDECL_OBJFIRST,
				*(asFunctionCaller*)&EnumCaller) >= 0
			&& ScriptEngine.RegisterObjectBehaviour(
				"FGCLifetimeNode",
				asBEHAVE_RELEASEREFS,
				"void f(int& in gcCycle)",
				asFUNCTION(GCNodeReleaseReferences),
				asCALL_CDECL_OBJFIRST,
				*(asFunctionCaller*)&ReleaseRefsCaller) >= 0
			&& ScriptEngine.RegisterObjectBehaviour(
				"FGCLifetimeNode",
				asBEHAVE_GET_WEAKREF_FLAG,
				"int &f()",
				asFUNCTION(GCNodeGetWeakFlag),
				asCALL_CDECL_OBJFIRST,
				*(asFunctionCaller*)&WeakFlagCaller) >= 0;
		OutType =
			ScriptEngine.GetTypeInfoByName(
				"FGCLifetimeNode");
		return bRegistered
			&& OutType != nullptr;
	}

	static FGCStatisticsSnapshot CaptureGCStatistics(
		asIScriptEngine& ScriptEngine)
	{
		FGCStatisticsSnapshot Snapshot;
		ScriptEngine.GetGCStatistics(
			&Snapshot.CurrentSize,
			&Snapshot.TotalDestroyed,
			&Snapshot.TotalDetected,
			&Snapshot.NewObjects,
			&Snapshot.TotalNewDestroyed);
		return Snapshot;
	}

	static bool RunFullGC(
		asIScriptEngine& ScriptEngine)
	{
		for (int32 Index = 0; Index < 8; ++Index)
		{
			if (ScriptEngine.GarbageCollect(
				asGC_FULL_CYCLE,
				1) < 0)
			{
				return false;
			}
		}
		return true;
	}

	static FNativeCaseContext MakeObservationContext(
		const FOwnerCase& Owner,
		const FReferenceCase& Reference,
		const FObservationCase& Observation)
	{
		return FNativeCaseContext(
			AngelscriptNativeTestSupport::
				MakeNativeCaseId(
					"LANG-REF-LIFETIME",
					{
						ANSI_TO_TCHAR(
							Observation.CatalogName),
						ANSI_TO_TCHAR(
							Owner.CatalogName),
						ANSI_TO_TCHAR(
							Reference.CatalogName),
					}));
	}

	static bool HasReferenceObject(
		const FReferenceCase& Reference)
	{
		return Reference.State
			!= EReferenceState::None;
	}

	static void AppendLocalGraph(
		FString& Source,
		const FReferenceCase& Reference,
		const TCHAR* const Indent)
	{
		using namespace AngelscriptNativeTestSupport;

		const FString Prefix(Indent);
		if (!HasReferenceObject(Reference))
		{
			AppendGeneratedAsLine(
				Source,
				Prefix
					+ TEXT("FRefRoot Primary = nullptr;"));
			return;
		}
		AppendGeneratedAsLine(
			Source,
			Prefix
				+ TEXT("FRefRoot Primary = MakeRefRoot(41);"));
		if (Reference.State
			== EReferenceState::MultipleAliases)
		{
			AppendGeneratedAsLine(
				Source,
				Prefix
					+ TEXT("FRefRoot AliasA = Primary;"));
			AppendGeneratedAsLine(
				Source,
				Prefix
					+ TEXT("FRefRoot AliasB = Primary;"));
		}
		if (Reference.State
			== EReferenceState::Cycle)
		{
			AppendGeneratedAsLine(
				Source,
				Prefix
					+ TEXT("Primary.Link(Primary);"));
		}
	}

	static void AppendScopeFunction(
		FString& Source,
		const FReferenceCase& Reference)
	{
		using namespace AngelscriptNativeTestSupport;

		AppendGeneratedAsLine(
			Source,
			TEXT("int RunScopeLifetime()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendLocalGraph(
			Source,
			Reference,
			TEXT("\t"));
		if (Reference.State
			== EReferenceState::Cycle)
		{
			AppendGeneratedAsLine(
				Source,
				TEXT("\tPrimary.ClearPeer();"));
		}
		AppendGeneratedAsLine(
			Source,
			TEXT("\treturn Primary == nullptr"));
		AppendGeneratedAsLine(
			Source,
			TEXT("\t\t? 0"));
		AppendGeneratedAsLine(
			Source,
			TEXT("\t\t: Primary.GetIdentity();"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
	}

	static void AppendReturnFunction(
		FString& Source,
		const FReferenceCase& Reference)
	{
		using namespace AngelscriptNativeTestSupport;

		AppendGeneratedAsLine(
			Source,
			TEXT("FRefRoot ReturnLifetime()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendLocalGraph(
			Source,
			Reference,
			TEXT("\t"));
		if (Reference.State
			== EReferenceState::Cycle)
		{
			AppendGeneratedAsLine(
				Source,
				TEXT("\tPrimary.ClearPeer();"));
		}
		AppendGeneratedAsLine(
			Source,
			TEXT("\treturn Primary;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
	}

	static void AppendRecoveryFunction(
		FString& Source)
	{
		using namespace AngelscriptNativeTestSupport;

		AppendGeneratedAsLine(
			Source,
			TEXT("int RecoverReferenceLifetime()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\treturn 919;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
	}

	static FString BuildReferenceLifetimeSource(
		const FReferenceCase& Reference)
	{
		FString Source;
		AppendScopeFunction(
			Source,
			Reference);
		AppendReturnFunction(
			Source,
			Reference);
		AppendRecoveryFunction(Source);
		return Source;
	}

	static FString BuildReferenceLifetimeGlobalRestrictionSource(
		const FReferenceCase& Reference)
	{
		using namespace AngelscriptNativeTestSupport;

		FString Source;
		AppendGeneratedAsLine(
			Source,
			TEXT("FRefRoot GReferenceOwner;"));
		AppendGeneratedAsLine(
			Source,
			TEXT("int GMutableObservation = 0;"));
		if (Reference.State == EReferenceState::MultipleAliases)
		{
			AppendGeneratedAsLine(
				Source,
				TEXT("FRefRoot GReferenceAlias;"));
		}
		AppendRecoveryFunction(Source);
		return Source;
	}

	static FString BuildReferenceLifetimeGcMarkerSource()
	{
		using namespace AngelscriptNativeTestSupport;

		FString Source;
		AppendGeneratedAsLine(
			Source,
			TEXT("int RunLifetimeGcMarker()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\treturn 1;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendRecoveryFunction(Source);
		return Source;
	}

	static FString BuildReferenceLifetimeRecoverySource()
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
		AngelscriptNativeTestSupport::
			PrintGeneratedAsSource(
				Test,
				SourceId,
				ModuleName,
				Source);
		const FTCHARToUTF8 ModuleNameUtf8(
			*ModuleName);
		const FTCHARToUTF8 SourceUtf8(*Source);
		return AngelscriptNativeTestSupport::
			CompileNativeModule(
				&ScriptEngine,
				ModuleNameUtf8.Get(),
				SourceUtf8.Get(),
				OutModule);
	}

	asILockableSharedBool* CaptureWeakFlag(
		asIScriptEngine& ScriptEngine,
		FReferenceState& State)
	{
		if (State.LivePointers.IsEmpty())
		{
			if (State.TrackedWeakFlags.IsEmpty())
			{
				return nullptr;
			}
			asILockableSharedBool* const Flag =
				State.TrackedWeakFlags[0];
			if (Flag != nullptr)
			{
				Flag->AddRef();
			}
			return Flag;
		}
		asITypeInfo* const Type =
			ScriptEngine.GetTypeInfoByName(
				"FRefRoot");
		if (Type == nullptr)
		{
			return nullptr;
		}
		asILockableSharedBool* const Flag =
			ScriptEngine.GetWeakRefFlagOfScriptObject(
				State.LivePointers[0],
				Type);
		if (Flag != nullptr)
		{
			Flag->AddRef();
		}
		return Flag;
	}

	void ExecuteVoidFunction(
		const FNativeCaseContext& Case,
		asIScriptEngine& ScriptEngine,
		asIScriptModule& Module,
		const ANSICHAR* const Declaration)
	{
		asIScriptFunction* const Function =
			Module.GetFunctionByDecl(Declaration);
		asIScriptContext* const Context =
			ScriptEngine.CreateContext();
		ASSERT_THAT(IsNotNull(Function,
			*Case.Describe(TEXT("reference lifetime should publish the requested boundary function"))));
		ASSERT_THAT(IsNotNull(Context,
			*Case.Describe(TEXT("reference lifetime boundary should create a context"))));
		if (Function != nullptr
			&& Context != nullptr)
		{
			ASSERT_THAT(AreEqual(
				asSUCCESS,
				Context->Prepare(Function),
				*Case.Describe(TEXT("reference lifetime boundary should prepare"))));
			ASSERT_THAT(AreEqual(
				static_cast<int32>(asEXECUTION_FINISHED),
				Context->Execute(),
				*Case.Describe(TEXT("reference lifetime boundary should finish"))));
			ASSERT_THAT(AreEqual(
				asSUCCESS,
				Context->Unprepare(),
				*Case.Describe(TEXT("reference lifetime boundary should unprepare"))));
		}
		if (Context != nullptr)
		{
			Context->Release();
		}
	}

	void ExecuteRecovery(
		const FNativeCaseContext& Case,
		asIScriptEngine& ScriptEngine,
		asIScriptModule& Module)
	{
		asIScriptFunction* const Recovery =
			Module.GetFunctionByDecl(
				"int RecoverReferenceLifetime()");
		asIScriptContext* const Context =
			ScriptEngine.CreateContext();
		ASSERT_THAT(IsNotNull(Recovery,
			*Case.Describe(TEXT("reference lifetime should publish recovery"))));
		ASSERT_THAT(IsNotNull(Context,
			*Case.Describe(TEXT("reference lifetime recovery should create a context"))));
		if (Recovery != nullptr
			&& Context != nullptr)
		{
			ASSERT_THAT(AreEqual(
				asSUCCESS,
				Context->Prepare(Recovery),
				*Case.Describe(TEXT("reference lifetime recovery should prepare"))));
			ASSERT_THAT(AreEqual(
				static_cast<int32>(asEXECUTION_FINISHED),
				Context->Execute(),
				*Case.Describe(TEXT("reference lifetime recovery should finish"))));
			ASSERT_THAT(AreEqual(
				919,
				static_cast<int32>(
					Context->GetReturnDWord()),
				*Case.Describe(TEXT("reference lifetime recovery should return its sentinel"))));
			ASSERT_THAT(AreEqual(
				asSUCCESS,
				Context->Unprepare(),
				*Case.Describe(TEXT("reference lifetime recovery should unprepare"))));
		}
		if (Context != nullptr)
		{
			Context->Release();
		}
	}

	void CompileAndExecuteRecoveryModule(
		const FNativeCaseContext& Case,
		FNativeTestEngine& Engine,
		asIScriptEngine& ScriptEngine,
		const FString& ModuleName)
	{
		const FString Source =
			BuildReferenceLifetimeRecoverySource();
		Engine.ResetMessages();
		asIScriptModule* Module = nullptr;
		ASSERT_THAT(IsTrue(
			CompileAndReport(
				*TestRunner,
				ScriptEngine,
				Case.GetId() + TEXT("-RECOVERY"),
				ModuleName,
				Source,
				Module) >= 0,
			*Case.Describe(TEXT("reference lifetime same-name recovery should compile"))));
		ASSERT_THAT(IsNotNull(Module,
			*Case.Describe(TEXT("reference lifetime same-name recovery should publish a module"))));
		if (Module != nullptr)
		{
			ExecuteRecovery(
				Case,
				ScriptEngine,
				*Module);
		}
		AngelscriptNativeReferenceTestSupport::
			DiscardReferenceModule(
				ScriptEngine,
				ModuleName);
	}

	void VerifyOrdinaryObservations(
		const FOwnerCase& Owner,
		const FReferenceCase& Reference,
		FReferenceState& State,
		asILockableSharedBool* WeakFlag,
		const int32 LiveAtObservation,
		const int32 RefCountAtObservation,
		const FGCStatisticsSnapshot& BeforeGC,
		const FGCStatisticsSnapshot& AfterGC)
	{
		const FNativeCaseContext IdentityCase =
			MakeObservationContext(
				Owner,
				Reference,
				ObservationCases[0]);
		const FNativeCaseContext RefCountCase =
			MakeObservationContext(
				Owner,
				Reference,
				ObservationCases[1]);
		const FNativeCaseContext WeakCase =
			MakeObservationContext(
				Owner,
				Reference,
				ObservationCases[2]);
		const FNativeCaseContext DestructionCase =
			MakeObservationContext(
				Owner,
				Reference,
				ObservationCases[3]);
		const FNativeCaseContext GCCase =
			MakeObservationContext(
				Owner,
				Reference,
				ObservationCases[4]);
		const bool bHasObject =
			HasReferenceObject(Reference);
		const bool bExpectedLiveAtObservation =
			bHasObject
			&& Owner.State
				!= EOwnerState::ScopeExit;
		ASSERT_THAT(AreEqual(
			bExpectedLiveAtObservation,
			LiveAtObservation > 0,
			*IdentityCase.Describe(TEXT("reference lifetime live boundary should match the selected reference state"))));
		if (bExpectedLiveAtObservation)
		{
			TSet<int32> UniqueCreated;
			for (const int32 Identity
				: State.CreatedIdentities)
			{
				UniqueCreated.Add(Identity);
			}
			ASSERT_THAT(AreEqual(
				State.CreatedIdentities.Num(),
				UniqueCreated.Num(),
				*IdentityCase.Describe(TEXT("reference lifetime should allocate each identity exactly once"))));
			ASSERT_THAT(IsTrue(
				RefCountAtObservation >= 1,
				*RefCountCase.Describe(TEXT("live reference lifetime object should retain at least one owning reference"))));
			ASSERT_THAT(IsNotNull(WeakFlag,
				*WeakCase.Describe(TEXT("live reference lifetime object should expose the core weak flag"))));
		}
		else
		{
			ASSERT_THAT(AreEqual(
				0,
				RefCountAtObservation,
				*RefCountCase.Describe(TEXT("non-live reference boundary should expose no reference count"))));
			if (!bHasObject)
			{
				ASSERT_THAT(IsNull(WeakFlag,
					*WeakCase.Describe(TEXT("none reference state should expose no weak flag"))));
			}
		}
		ASSERT_THAT(AreEqual(
			0,
			State.LiveObjects,
			*DestructionCase.DescribeResult(
				TEXT("reference lifetime ordinary cleanup"),
				TEXT("Live=0 after the selected owner boundary"),
				DescribeReferenceState(State))));
		ASSERT_THAT(AreEqual(
			State.Created,
			State.Destroyed,
			*DestructionCase.Describe(TEXT("reference lifetime boundary should destroy every ordinary object"))));
		TArray<int32> Created =
			State.CreatedIdentities;
		TArray<int32> Destroyed =
			State.DestroyedIdentities;
		Created.Sort();
		Destroyed.Sort();
		ASSERT_THAT(AreEqual(
			Created,
			Destroyed,
			*DestructionCase.Describe(TEXT("reference lifetime should destroy the exact created identities"))));
		if (Reference.State
			== EReferenceState::Cycle)
		{
			ASSERT_THAT(IsTrue(
				State.LinkCalls >= 1,
				*IdentityCase.Describe(TEXT("cycle reference state should form its peer cycle before the selected boundary"))));
			ASSERT_THAT(IsTrue(
				State.ClearPeerCalls >= 1,
				*DestructionCase.Describe(TEXT("cycle reference state should execute its explicit peer-break route before cleanup"))));
		}
		if (WeakFlag != nullptr)
		{
			ASSERT_THAT(IsTrue(
				WeakFlag->Get(),
				*WeakCase.Describe(TEXT("core weak flag should become true after the referenced object is destroyed"))));
		}
		if (Reference.State
				== EReferenceState::WeakFlag
			&& Owner.State
				!= EOwnerState::ScopeExit)
		{
			ASSERT_THAT(IsTrue(
				State.WeakFlagQueries >= 1,
				*WeakCase.Describe(TEXT("weak-flag reference state should invoke the registered core behavior"))));
		}
		ASSERT_THAT(AreEqual(
			BeforeGC.CurrentSize,
			AfterGC.CurrentSize,
			*GCCase.Describe(TEXT("ordinary automatic references should not enter the core GC object set"))));
		ASSERT_THAT(IsTrue(
			AfterGC.TotalDestroyed
				>= BeforeGC.TotalDestroyed,
			*GCCase.Describe(TEXT("ordinary reference cleanup should not regress GC destruction statistics"))));
	}

	void RunOrdinaryWorkflow(
		const FOwnerCase& Owner,
		const FReferenceCase& Reference)
	{
		using namespace AngelscriptNativeReferenceTestSupport;
		using namespace AngelscriptNativeTestSupport;

		const FNativeCaseContext IdentityCase =
			MakeObservationContext(
				Owner,
				Reference,
				ObservationCases[0]);
		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};
		asIScriptEngine* const ScriptEngine =
			Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine,
			*IdentityCase.Describe(TEXT("reference lifetime workflow should create a raw engine"))));
		if (ScriptEngine == nullptr)
		{
			return;
		}
		FReferenceState State;
		State.ResetCounters();
		State.bTrackWeakFlags = true;
		ASSERT_THAT(IsTrue(
			RegisterReferenceFixtures(
				*ScriptEngine,
				State,
				true),
			*IdentityCase.Describe(TEXT("reference lifetime workflow should register core reference fixtures"))));
		const FString ModuleName = FString::Printf(
			TEXT("ReferenceLifetime_%s"),
			*IdentityCase.GetId());
		const FString Source =
			BuildReferenceLifetimeSource(
				Reference);
		Engine.ResetMessages();
		asIScriptModule* Module = nullptr;
		const int BuildResult =
			CompileAndReport(
				*TestRunner,
				*ScriptEngine,
				IdentityCase.GetId()
					+ TEXT("-WORKFLOW"),
				ModuleName,
				Source,
				Module);
		ASSERT_THAT(IsTrue(
			BuildResult >= 0,
			*IdentityCase.DescribeResult(
				TEXT("reference lifetime ordinary workflow"),
				TEXT("successful build"),
				DescribeReferenceBuild(
					Engine,
					BuildResult))));
		ASSERT_THAT(IsNotNull(Module,
			*IdentityCase.Describe(TEXT("reference lifetime workflow should publish a module"))));
		ASSERT_THAT(IsFalse(
			HasAnyError(Engine),
			*IdentityCase.Describe(TEXT("reference lifetime workflow should emit no errors"))));
		if (Module == nullptr)
		{
			return;
		}

		asIScriptContext* RetainedContext = nullptr;
		switch (Owner.State)
		{
		case EOwnerState::OwnerLive:
		case EOwnerState::ReturnedAlias:
		case EOwnerState::ContextRetained:
		case EOwnerState::ContextReleased:
		{
			asIScriptFunction* const Function =
				Module->GetFunctionByDecl(
					"FRefRoot ReturnLifetime()");
			RetainedContext =
				ScriptEngine->CreateContext();
			ASSERT_THAT(IsNotNull(Function,
				*IdentityCase.Describe(TEXT("reference lifetime return boundary should publish its exact function"))));
			ASSERT_THAT(IsNotNull(RetainedContext,
				*IdentityCase.Describe(TEXT("reference lifetime return boundary should create a context"))));
			if (Function != nullptr
				&& RetainedContext != nullptr)
			{
				ASSERT_THAT(AreEqual(
					asSUCCESS,
					RetainedContext->Prepare(Function),
					*IdentityCase.Describe(TEXT("reference lifetime return boundary should prepare"))));
				ASSERT_THAT(AreEqual(
					static_cast<int32>(asEXECUTION_FINISHED),
					RetainedContext->Execute(),
					*IdentityCase.Describe(TEXT("reference lifetime return boundary should finish"))));
			}
			break;
		}
		case EOwnerState::ScopeExit:
			ExecuteVoidFunction(
				IdentityCase,
				*ScriptEngine,
				*Module,
				"int RunScopeLifetime()");
			break;
		default:
			break;
		}

		const int32 LiveAtObservation =
			State.LiveObjects;
		const int32 RefCountAtObservation =
			State.LivePointers.IsEmpty()
				? 0
				: State.LivePointers[0]
					->GetReferenceCount();
		asILockableSharedBool* const WeakFlag =
			CaptureWeakFlag(
				*ScriptEngine,
				State);
		if (WeakFlag != nullptr)
		{
			if (Owner.State == EOwnerState::ScopeExit)
			{
				ASSERT_THAT(IsTrue(
					WeakFlag->Get(),
					*MakeObservationContext(
						Owner,
						Reference,
						ObservationCases[2])
							.Describe(TEXT("scope-exit weak flag should already be invalidated at the boundary"))));
			}
			else
			{
				ASSERT_THAT(IsFalse(
					WeakFlag->Get(),
					*MakeObservationContext(
						Owner,
						Reference,
						ObservationCases[2])
							.Describe(TEXT("core weak flag should remain false while its object is live or retained by a cycle"))));
			}
		}
		const FGCStatisticsSnapshot BeforeGC =
			CaptureGCStatistics(
				*ScriptEngine);

		switch (Owner.State)
		{
		case EOwnerState::OwnerLive:
		case EOwnerState::ReturnedAlias:
			if (RetainedContext != nullptr)
			{
				RetainedContext->Unprepare();
			}
			break;
		case EOwnerState::ContextRetained:
			ASSERT_THAT(AreEqual(
				LiveAtObservation,
				State.LiveObjects,
				*IdentityCase.Describe(TEXT("prepared retained context should preserve its returned reference"))));
			if (RetainedContext != nullptr)
			{
				RetainedContext->Unprepare();
			}
			break;
		case EOwnerState::ContextReleased:
			if (RetainedContext != nullptr)
			{
				RetainedContext->Release();
				RetainedContext = nullptr;
			}
			break;
		case EOwnerState::ScopeExit:
			break;
		default:
			break;
		}
		if (RetainedContext != nullptr)
		{
			RetainedContext->Release();
			RetainedContext = nullptr;
		}
		State.BreakAllCycles();
		State.ReleaseRetainedNativeObject();
		ExecuteRecovery(
			IdentityCase,
			*ScriptEngine,
			*Module);
		DiscardReferenceModule(
			*ScriptEngine,
			ModuleName);
		ASSERT_THAT(IsTrue(
			RunFullGC(*ScriptEngine),
			*MakeObservationContext(
				Owner,
				Reference,
				ObservationCases[4])
					.Describe(TEXT("ordinary reference workflow should complete a core GC pass"))));
		const FGCStatisticsSnapshot AfterGC =
			CaptureGCStatistics(
				*ScriptEngine);
		VerifyOrdinaryObservations(
			Owner,
			Reference,
			State,
			WeakFlag,
			LiveAtObservation,
			RefCountAtObservation,
			BeforeGC,
			AfterGC);
		if (WeakFlag != nullptr)
		{
			WeakFlag->Release();
		}
		State.ReleaseTrackedWeakFlags();
	}

	void RunRejectedGlobalOwnerWorkflow(
		const FOwnerCase& Owner,
		const FReferenceCase& Reference)
	{
		using namespace AngelscriptNativeReferenceTestSupport;
		using namespace AngelscriptNativeTestSupport;

		const FNativeCaseContext Case =
			MakeObservationContext(
				Owner,
				Reference,
				ObservationCases[0]);
		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};
		asIScriptEngine* const ScriptEngine =
			Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine,
			*Case.Describe(TEXT("global-owner fork restriction should create a raw engine"))));
		if (ScriptEngine == nullptr)
		{
			return;
		}

		FReferenceState State;
		State.ResetCounters();
		ASSERT_THAT(IsTrue(
			RegisterReferenceFixtures(
				*ScriptEngine,
				State),
			*Case.Describe(TEXT("global-owner fork restriction should register core reference fixtures"))));
		const FString ModuleName = FString::Printf(
			TEXT("ReferenceLifetimeGlobalRestriction_%s"),
			*Case.GetId());
		const FString Source =
			BuildReferenceLifetimeGlobalRestrictionSource(
				Reference);
		Engine.ResetMessages();
		asIScriptModule* Module = nullptr;
		const int BuildResult =
			CompileAndReport(
				*TestRunner,
				*ScriptEngine,
				Case.GetId() + TEXT("-GLOBAL-RESTRICTION"),
				ModuleName,
				Source,
				Module);
		ASSERT_THAT(IsTrue(
			BuildResult < 0,
			*Case.DescribeResult(
				TEXT("reference global owner restriction"),
				TEXT("current fork rejects class/reference globals and mutable script globals"),
				DescribeReferenceBuild(
					Engine,
					BuildResult))));
		ASSERT_THAT(IsTrue(
			HasDiagnosticContaining(
				Engine,
				TEXT("Class types are not supported for global variables")),
			*Case.DescribeResult(
				TEXT("reference global owner restriction diagnostic"),
				TEXT("class/reference global rejection"),
				Engine.GetMessagesText())));
		ASSERT_THAT(IsTrue(
			HasDiagnosticContaining(
				Engine,
				TEXT("Mutable global variables are not supported")),
			*Case.DescribeResult(
				TEXT("reference global owner restriction diagnostic"),
				TEXT("mutable global rejection"),
				Engine.GetMessagesText())));
		if (Module != nullptr)
		{
			ASSERT_THAT(IsNull(
				Module->GetFunctionByDecl(
					"int RecoverReferenceLifetime()"),
				*Case.Describe(TEXT("failed global-owner build should not publish a recovery function in its module shell"))));
		}
		DiscardReferenceModule(
			*ScriptEngine,
			ModuleName);
		CompileAndExecuteRecoveryModule(
			Case,
			Engine,
			*ScriptEngine,
			ModuleName);
		ASSERT_THAT(AreEqual(
			0,
			State.LiveObjects,
			*Case.DescribeResult(
				TEXT("reference global owner restriction cleanup"),
				TEXT("Live=0 because rejected source must not allocate a native reference"),
				DescribeReferenceState(State))));
	}

	void RunGCWorkflow(
		const FOwnerCase& Owner,
		const FReferenceCase& Reference)
	{
		using namespace AngelscriptNativeReferenceTestSupport;
		using namespace AngelscriptNativeTestSupport;

		const FNativeCaseContext IdentityCase =
			MakeObservationContext(
				Owner,
				Reference,
				ObservationCases[0]);
		const FNativeCaseContext RefCountCase =
			MakeObservationContext(
				Owner,
				Reference,
				ObservationCases[1]);
		const FNativeCaseContext WeakCase =
			MakeObservationContext(
				Owner,
				Reference,
				ObservationCases[2]);
		const FNativeCaseContext DestructionCase =
			MakeObservationContext(
				Owner,
				Reference,
				ObservationCases[3]);
		const FNativeCaseContext GCCase =
			MakeObservationContext(
				Owner,
				Reference,
				ObservationCases[4]);
		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};
		asIScriptEngine* const ScriptEngine =
			Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine,
			*IdentityCase.Describe(TEXT("GC reference lifetime workflow should create a raw engine"))));
		if (ScriptEngine == nullptr)
		{
			return;
		}
		FReferenceState OrdinaryState;
		OrdinaryState.ResetCounters();
		ASSERT_THAT(IsTrue(
			RegisterReferenceFixtures(
				*ScriptEngine,
				OrdinaryState),
			*IdentityCase.Describe(TEXT("GC reference lifetime workflow should retain ordinary reference registration"))));
		asITypeInfo* GCType = nullptr;
		ResetGCNodeState();
		ASSERT_THAT(IsTrue(
			RegisterGCNode(
				*ScriptEngine,
				GCType),
			*GCCase.Describe(TEXT("GC reference lifetime workflow should register all core GC behaviors"))));
		const FString ModuleName = FString::Printf(
			TEXT("ReferenceLifetimeGC_%s"),
			*IdentityCase.GetId());
		const FString Source =
			BuildReferenceLifetimeGcMarkerSource();
		Engine.ResetMessages();
		asIScriptModule* Module = nullptr;
		const int BuildResult =
			CompileAndReport(
				*TestRunner,
				*ScriptEngine,
				IdentityCase.GetId()
					+ TEXT("-WORKFLOW"),
				ModuleName,
				Source,
				Module);
		ASSERT_THAT(IsTrue(
			BuildResult >= 0,
			*IdentityCase.DescribeResult(
				TEXT("GC reference lifetime workflow"),
				TEXT("successful build"),
				DescribeReferenceBuild(
					Engine,
					BuildResult))));
		ASSERT_THAT(IsNotNull(Module,
			*IdentityCase.Describe(TEXT("GC reference lifetime marker source should publish a module"))));
		if (Module != nullptr)
		{
			ExecuteVoidFunction(
				IdentityCase,
				*ScriptEngine,
				*Module,
				"int RunLifetimeGcMarker()");
		}

		FGCLifetimeNode* First = nullptr;
		FGCLifetimeNode* Second = nullptr;
		if (HasReferenceObject(Reference)
			&& GCType != nullptr)
		{
			First = new FGCLifetimeNode();
			if (Reference.State
					== EReferenceState::Cycle
				|| Reference.State
					== EReferenceState::MultipleAliases)
			{
				Second = new FGCLifetimeNode();
				First->Link(Second);
				Second->Link(First);
				ScriptEngine->NotifyGarbageCollectorOfNewObject(
					Second,
					GCType);
			}
			else
			{
				First->Link(First);
			}
			if (Reference.State
				== EReferenceState::MultipleAliases)
			{
				First->AddRef();
			}
			ScriptEngine->NotifyGarbageCollectorOfNewObject(
				First,
				GCType);
		}
		asILockableSharedBool* WeakFlag = nullptr;
		if (First != nullptr
			&& GCType != nullptr)
		{
			WeakFlag =
				ScriptEngine->GetWeakRefFlagOfScriptObject(
					First,
					GCType);
			if (WeakFlag != nullptr)
			{
				WeakFlag->AddRef();
				ASSERT_THAT(IsFalse(
					WeakFlag->Get(),
					*WeakCase.Describe(TEXT("GC reference weak flag should remain false before collection"))));
			}
		}
		ASSERT_THAT(AreEqual(
			HasReferenceObject(Reference),
			FGCLifetimeNode::LiveCount > 0,
			*IdentityCase.Describe(TEXT("GC reference workflow should create nodes exactly for non-none states"))));
		if (First != nullptr)
		{
			ASSERT_THAT(IsTrue(
				First->GetReferenceCount() >= 2,
				*RefCountCase.Describe(TEXT("GC reference cycle should hold both external and cyclic ownership"))));
		}
		const FGCStatisticsSnapshot BeforeRelease =
			CaptureGCStatistics(
				*ScriptEngine);
		if (First != nullptr)
		{
			First->Release();
			if (Reference.State
				== EReferenceState::MultipleAliases)
			{
				First->Release();
			}
		}
		if (Second != nullptr)
		{
			Second->Release();
		}
		ASSERT_THAT(IsTrue(
			RunFullGC(*ScriptEngine),
			*GCCase.Describe(TEXT("GC reference workflow should complete full collection passes"))));
		const FGCStatisticsSnapshot AfterCollect =
			CaptureGCStatistics(
				*ScriptEngine);
		ASSERT_THAT(AreEqual(
			0,
			FGCLifetimeNode::LiveCount,
			*DestructionCase.Describe(TEXT("GC reference workflow should collect every released cycle node"))));
		ASSERT_THAT(AreEqual(
			FGCLifetimeNode::Created,
			FGCLifetimeNode::Destroyed,
			*DestructionCase.Describe(TEXT("GC reference workflow should destroy every created node"))));
		TArray<int32> Created =
			FGCLifetimeNode::CreatedIdentities;
		TArray<int32> Destroyed =
			FGCLifetimeNode::DestroyedIdentities;
		Created.Sort();
		Destroyed.Sort();
		ASSERT_THAT(AreEqual(
			Created,
			Destroyed,
			*IdentityCase.Describe(TEXT("GC reference workflow should destroy the exact created identities"))));
		if (HasReferenceObject(Reference))
		{
			ASSERT_THAT(IsTrue(
				AfterCollect.TotalDestroyed
					> BeforeRelease.TotalDestroyed,
				*GCCase.Describe(TEXT("GC reference workflow should increase collected-object destruction statistics"))));
			ASSERT_THAT(IsTrue(
				AfterCollect.CurrentSize
					<= BeforeRelease.CurrentSize,
				*GCCase.Describe(TEXT("GC reference workflow should not retain released cycle nodes"))));
			ASSERT_THAT(IsNotNull(WeakFlag,
				*WeakCase.Describe(TEXT("GC reference object should expose the core weak flag behavior"))));
		}
		if (WeakFlag != nullptr)
		{
			ASSERT_THAT(IsTrue(
				WeakFlag->Get(),
				*WeakCase.Describe(TEXT("GC reference weak flag should become true after collection"))));
			WeakFlag->Release();
		}
		if (Module != nullptr)
		{
			ExecuteRecovery(
				IdentityCase,
				*ScriptEngine,
				*Module);
		}
		DiscardReferenceModule(
			*ScriptEngine,
			ModuleName);
	}

	void RunWorkflow(
		const FOwnerCase& Owner,
		const FReferenceCase& Reference)
	{
		if (Owner.State == EOwnerState::GCCycle)
		{
			RunGCWorkflow(
				Owner,
				Reference);
		}
		else if (Owner.State == EOwnerState::ModuleRetained
			|| Owner.State == EOwnerState::ModuleDiscarded)
		{
			RunRejectedGlobalOwnerWorkflow(
				Owner,
				Reference);
		}
		else
		{
			RunOrdinaryWorkflow(
				Owner,
				Reference);
		}
	}

public:
	TEST_METHOD(OwnerStatesByReferenceAndObservation)
	{
		AS_NATIVE_PRODUCT("LANG-REF-LIFETIME",
			AngelscriptNativeTestSupport::ENativeEvidence::Runtime
				| AngelscriptNativeTestSupport::ENativeEvidence::Lifecycle
				| AngelscriptNativeTestSupport::ENativeEvidence::Cleanup
				| AngelscriptNativeTestSupport::ENativeEvidence::Isolation);

		for (const FOwnerCase& Owner : OwnerCases)
		{
			for (const FReferenceCase& Reference
				: ReferenceCases)
			{
				RunWorkflow(
					Owner,
					Reference);
			}
		}
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
