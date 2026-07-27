#pragma once

#include "../../Support/AngelscriptNativeFixtureTestSupport.h"
#include "../../Support/AngelscriptNativeLanguageCaseTestSupport.h"

namespace AngelscriptNativeReferenceTestSupport
{
	inline constexpr asPWORD ReferenceStateUserDataSlot =
		static_cast<asPWORD>(0x5245465354415445ull);

	enum class EReferenceKind : int32
	{
		Root = 101,
		Derived = 202,
		Unrelated = 303,
	};

	struct FReferenceState;

	class FReferenceRoot
	{
	public:
		FReferenceRoot(
			FReferenceState& InState,
			EReferenceKind InKind,
			int32 InValue);
		virtual ~FReferenceRoot();

		void AddRef();
		void Release();
		int32 GetIdentity() const;
		int32 GetKind() const;
		int32 GetValue() const;
		int32 GetReferenceCount() const;
		FReferenceState& GetState() const;
		void SetValue(int32 InValue);
		void Link(FReferenceRoot* InPeer);
		void ClearPeer();
		FReferenceRoot* GetPeer() const;
		asILockableSharedBool* GetWeakFlag() const;

	private:
		FReferenceState& State;
		EReferenceKind Kind = EReferenceKind::Root;
		int32 Identity = INDEX_NONE;
		int32 Value = 0;
		int32 ReferenceCount = 1;
		FReferenceRoot* Peer = nullptr;
		asILockableSharedBool* WeakFlag = nullptr;
	};

	class FReferenceDerived final : public FReferenceRoot
	{
	public:
		FReferenceDerived(
			FReferenceState& State,
			int32 Value)
			: FReferenceRoot(
				State,
				EReferenceKind::Derived,
				Value)
		{
		}
	};

	class FReferenceUnrelated final : public FReferenceRoot
	{
	public:
		FReferenceUnrelated(
			FReferenceState& State,
			int32 Value)
			: FReferenceRoot(
				State,
				EReferenceKind::Unrelated,
				Value)
		{
		}
	};

	struct FReferenceState
	{
		asIScriptEngine* ScriptEngine = nullptr;
		int32 NextIdentity = 1;
		int32 Created = 0;
		int32 Destroyed = 0;
		int32 LiveObjects = 0;
		int32 AddRefCalls = 0;
		int32 ReleaseCalls = 0;
		int32 MutationCalls = 0;
		int32 LinkCalls = 0;
		int32 ClearPeerCalls = 0;
		int32 WeakFlagQueries = 0;
		int32 MutableCastCalls = 0;
		int32 ConstCastCalls = 0;
		int32 SameReferenceCalls = 0;
		bool bTrackWeakFlags = false;
		FReferenceRoot* RetainedNativeObject = nullptr;
		TArray<int32> CreatedIdentities;
		TArray<int32> DestroyedIdentities;
		TArray<FReferenceRoot*> LivePointers;
		TArray<asILockableSharedBool*> TrackedWeakFlags;
		TMap<int32, int32> CurrentReferenceCounts;
		TMap<int32, int32> CurrentValues;

		int32 AllocateIdentity()
		{
			return NextIdentity++;
		}

		void ResetCounters()
		{
			NextIdentity = 1;
			Created = 0;
			Destroyed = 0;
			LiveObjects = 0;
			AddRefCalls = 0;
			ReleaseCalls = 0;
			MutationCalls = 0;
			LinkCalls = 0;
			ClearPeerCalls = 0;
			WeakFlagQueries = 0;
			MutableCastCalls = 0;
			ConstCastCalls = 0;
			SameReferenceCalls = 0;
			bTrackWeakFlags = false;
			CreatedIdentities.Reset();
			DestroyedIdentities.Reset();
			LivePointers.Reset();
			TrackedWeakFlags.Reset();
			CurrentReferenceCounts.Reset();
			CurrentValues.Reset();
			RetainedNativeObject = nullptr;
		}

		void ReleaseRetainedNativeObject()
		{
			if (RetainedNativeObject != nullptr)
			{
				FReferenceRoot* const Object =
					RetainedNativeObject;
				RetainedNativeObject = nullptr;
				Object->Release();
			}
		}

		void BreakAllCycles()
		{
			const TArray<FReferenceRoot*> Snapshot =
				LivePointers;
			for (FReferenceRoot* const Object : Snapshot)
			{
				if (LivePointers.Contains(Object))
				{
					Object->ClearPeer();
				}
			}
		}

		void ReleaseTrackedWeakFlags()
		{
			for (asILockableSharedBool* const Flag
				: TrackedWeakFlags)
			{
				if (Flag != nullptr)
				{
					Flag->Release();
				}
			}
			TrackedWeakFlags.Reset();
		}
	};

	inline FReferenceRoot::FReferenceRoot(
		FReferenceState& InState,
		const EReferenceKind InKind,
		const int32 InValue)
		: State(InState)
		, Kind(InKind)
		, Identity(State.AllocateIdentity())
		, Value(InValue)
		, WeakFlag(asCreateLockableSharedBool())
	{
		++State.Created;
		++State.LiveObjects;
		State.CreatedIdentities.Add(Identity);
		State.LivePointers.Add(this);
		if (State.bTrackWeakFlags
			&& WeakFlag != nullptr)
		{
			WeakFlag->AddRef();
			State.TrackedWeakFlags.Add(WeakFlag);
		}
		State.CurrentReferenceCounts.Add(
			Identity,
			ReferenceCount);
		State.CurrentValues.Add(
			Identity,
			Value);
	}

	inline FReferenceRoot::~FReferenceRoot()
	{
		ClearPeer();
		if (WeakFlag != nullptr)
		{
			WeakFlag->Set(true);
			WeakFlag->Release();
			WeakFlag = nullptr;
		}
		++State.Destroyed;
		--State.LiveObjects;
		State.DestroyedIdentities.Add(Identity);
		State.LivePointers.RemoveSingle(this);
		State.CurrentReferenceCounts.Remove(Identity);
		State.CurrentValues.Remove(Identity);
	}

	inline void FReferenceRoot::AddRef()
	{
		++ReferenceCount;
		++State.AddRefCalls;
		State.CurrentReferenceCounts.Add(
			Identity,
			ReferenceCount);
	}

	inline void FReferenceRoot::Release()
	{
		++State.ReleaseCalls;
		--ReferenceCount;
		State.CurrentReferenceCounts.Add(
			Identity,
			ReferenceCount);
		if (ReferenceCount == 0)
		{
			delete this;
		}
	}

	inline int32 FReferenceRoot::GetIdentity() const
	{
		return Identity;
	}

	inline int32 FReferenceRoot::GetKind() const
	{
		return static_cast<int32>(Kind);
	}

	inline int32 FReferenceRoot::GetValue() const
	{
		return Value;
	}

	inline int32 FReferenceRoot::GetReferenceCount() const
	{
		return ReferenceCount;
	}

	inline FReferenceState& FReferenceRoot::GetState() const
	{
		return State;
	}

	inline void FReferenceRoot::SetValue(
		const int32 InValue)
	{
		Value = InValue;
		++State.MutationCalls;
		State.CurrentValues.Add(
			Identity,
			Value);
	}

	inline void FReferenceRoot::Link(
		FReferenceRoot* const InPeer)
	{
		if (Peer == InPeer)
		{
			return;
		}
		ClearPeer();
		Peer = InPeer;
		if (Peer != nullptr)
		{
			Peer->AddRef();
		}
		++State.LinkCalls;
	}

	inline void FReferenceRoot::ClearPeer()
	{
		if (Peer != nullptr)
		{
			FReferenceRoot* const Previous = Peer;
			Peer = nullptr;
			++State.ClearPeerCalls;
			Previous->Release();
		}
	}

	inline FReferenceRoot* FReferenceRoot::GetPeer() const
	{
		return Peer;
	}

	inline asILockableSharedBool* FReferenceRoot::GetWeakFlag() const
	{
		++State.WeakFlagQueries;
		return WeakFlag;
	}

	inline FReferenceState* GetReferenceState(
		asIScriptGeneric& Generic)
	{
		return Generic.GetEngine() != nullptr
			? static_cast<FReferenceState*>(
				Generic.GetEngine()->GetUserData(
					ReferenceStateUserDataSlot))
			: nullptr;
	}

	inline void GenericReferenceAddRef(
		asIScriptGeneric* Generic)
	{
		if (Generic != nullptr)
		{
			if (FReferenceRoot* const Object =
				static_cast<FReferenceRoot*>(
					Generic->GetObject()))
			{
				Object->AddRef();
			}
		}
	}

	inline void GenericReferenceRelease(
		asIScriptGeneric* Generic)
	{
		if (Generic != nullptr)
		{
			if (FReferenceRoot* const Object =
				static_cast<FReferenceRoot*>(
					Generic->GetObject()))
			{
				Object->Release();
			}
		}
	}

	inline void GenericGetIdentity(
		asIScriptGeneric* Generic)
	{
		if (Generic == nullptr)
		{
			return;
		}
		const FReferenceRoot* const Object =
			static_cast<const FReferenceRoot*>(
				Generic->GetObject());
		Generic->SetReturnDWord(
			Object != nullptr
				? static_cast<asDWORD>(
					Object->GetIdentity())
				: 0);
	}

	inline void GenericGetKind(
		asIScriptGeneric* Generic)
	{
		if (Generic == nullptr)
		{
			return;
		}
		const FReferenceRoot* const Object =
			static_cast<const FReferenceRoot*>(
				Generic->GetObject());
		Generic->SetReturnDWord(
			Object != nullptr
				? static_cast<asDWORD>(
					Object->GetKind())
				: 0);
	}

	inline void GenericGetValue(
		asIScriptGeneric* Generic)
	{
		if (Generic == nullptr)
		{
			return;
		}
		const FReferenceRoot* const Object =
			static_cast<const FReferenceRoot*>(
				Generic->GetObject());
		Generic->SetReturnDWord(
			Object != nullptr
				? static_cast<asDWORD>(
					Object->GetValue())
				: 0);
	}

	inline void GenericSetValue(
		asIScriptGeneric* Generic)
	{
		if (Generic == nullptr)
		{
			return;
		}
		FReferenceRoot* const Object =
			static_cast<FReferenceRoot*>(
				Generic->GetObject());
		if (Object != nullptr)
		{
			Object->SetValue(
				static_cast<int32>(
					Generic->GetArgDWord(0)));
		}
	}

	inline void GenericGetReferenceCount(
		asIScriptGeneric* Generic)
	{
		if (Generic == nullptr)
		{
			return;
		}
		const FReferenceRoot* const Object =
			static_cast<const FReferenceRoot*>(
				Generic->GetObject());
		Generic->SetReturnDWord(
			Object != nullptr
				? static_cast<asDWORD>(
					Object->GetReferenceCount())
				: 0);
	}

	inline void GenericLinkReference(
		asIScriptGeneric* Generic)
	{
		if (Generic == nullptr)
		{
			return;
		}
		FReferenceRoot* const Object =
			static_cast<FReferenceRoot*>(
				Generic->GetObject());
		if (Object != nullptr)
		{
			FReferenceRoot* Other = nullptr;
			if (FReferenceRoot** const OtherHandleAddress =
				static_cast<FReferenceRoot**>(
					Generic->GetArgAddress(0)))
			{
				Other = *OtherHandleAddress;
			}
			Object->Link(
				Other);
		}
	}

	inline void GenericClearReferencePeer(
		asIScriptGeneric* Generic)
	{
		if (Generic != nullptr)
		{
			if (FReferenceRoot* const Object =
				static_cast<FReferenceRoot*>(
					Generic->GetObject()))
			{
				Object->ClearPeer();
			}
		}
	}

	inline void GenericGetPeerIdentity(
		asIScriptGeneric* Generic)
	{
		if (Generic == nullptr)
		{
			return;
		}
		const FReferenceRoot* const Object =
			static_cast<const FReferenceRoot*>(
				Generic->GetObject());
		const FReferenceRoot* const Peer =
			Object != nullptr
				? Object->GetPeer()
				: nullptr;
		Generic->SetReturnDWord(
			Peer != nullptr
				? static_cast<asDWORD>(
					Peer->GetIdentity())
				: 0);
	}

	inline void GenericGetWeakFlag(
		asIScriptGeneric* Generic)
	{
		if (Generic == nullptr)
		{
			return;
		}
		const FReferenceRoot* const Object =
			static_cast<const FReferenceRoot*>(
				Generic->GetObject());
		Generic->SetReturnAddress(
			Object != nullptr
				? Object->GetWeakFlag()
				: nullptr);
	}

	inline void ReturnReferenceObject(
		asIScriptGeneric& Generic,
		FReferenceRoot* const Object)
	{
		Generic.SetReturnAddress(Object);
	}

	inline void GenericMakeRoot(
		asIScriptGeneric* Generic)
	{
		if (Generic == nullptr)
		{
			return;
		}
		FReferenceState* const State =
			GetReferenceState(*Generic);
		ReturnReferenceObject(
			*Generic,
			State != nullptr
				? new FReferenceRoot(
					*State,
					EReferenceKind::Root,
					static_cast<int32>(
						Generic->GetArgDWord(0)))
				: nullptr);
	}

	inline void GenericMakeDerived(
		asIScriptGeneric* Generic)
	{
		if (Generic == nullptr)
		{
			return;
		}
		FReferenceState* const State =
			GetReferenceState(*Generic);
		ReturnReferenceObject(
			*Generic,
			State != nullptr
				? new FReferenceDerived(
					*State,
					static_cast<int32>(
						Generic->GetArgDWord(0)))
				: nullptr);
	}

	inline void GenericMakeDerivedAsRoot(
		asIScriptGeneric* Generic)
	{
		GenericMakeDerived(Generic);
	}

	inline void GenericMakeUnrelated(
		asIScriptGeneric* Generic)
	{
		if (Generic == nullptr)
		{
			return;
		}
		FReferenceState* const State =
			GetReferenceState(*Generic);
		ReturnReferenceObject(
			*Generic,
			State != nullptr
				? new FReferenceUnrelated(
					*State,
					static_cast<int32>(
						Generic->GetArgDWord(0)))
				: nullptr);
	}

	inline void GenericMakeNull(
		asIScriptGeneric* Generic)
	{
		if (Generic != nullptr)
		{
			Generic->SetReturnAddress(nullptr);
		}
	}

	inline void GenericGetNativeObject(
		asIScriptGeneric* Generic)
	{
		if (Generic == nullptr)
		{
			return;
		}
		FReferenceState* const State =
			GetReferenceState(*Generic);
		if (State == nullptr)
		{
			Generic->SetReturnAddress(nullptr);
			return;
		}
		if (State->RetainedNativeObject == nullptr)
		{
			State->RetainedNativeObject =
				new FReferenceRoot(
					*State,
					EReferenceKind::Root,
					static_cast<int32>(
						Generic->GetArgDWord(0)));
		}
		State->RetainedNativeObject->AddRef();
		Generic->SetReturnAddress(
			State->RetainedNativeObject);
	}

	inline FReferenceRoot* AddReferenceForCast(
		FReferenceRoot* const Object,
		const bool bConst)
	{
		if (Object != nullptr)
		{
			Object->AddRef();
			FReferenceState& State =
				Object->GetState();
			if (bConst)
			{
				++State.ConstCastCalls;
			}
			else
			{
				++State.MutableCastCalls;
			}
		}
		return Object;
	}

	inline FReferenceDerived* DowncastReference(
		FReferenceRoot* const Object,
		const bool bConst)
	{
		if (Object == nullptr)
		{
			return nullptr;
		}
		FReferenceState& State =
			Object->GetState();
		if (bConst)
		{
			++State.ConstCastCalls;
		}
		else
		{
			++State.MutableCastCalls;
		}
		// Reference fixtures compile with RTTI disabled. The immutable kind set by
		// FReferenceRoot's constructor is the fixture's exact type discriminator.
		if (Object->GetKind() != static_cast<int32>(EReferenceKind::Derived))
		{
			return nullptr;
		}
		FReferenceDerived* const Derived =
			static_cast<FReferenceDerived*>(Object);
		if (Derived != nullptr)
		{
			Derived->AddRef();
		}
		return Derived;
	}

	inline void GenericDerivedToRoot(
		asIScriptGeneric* Generic,
		const bool bConst)
	{
		if (Generic == nullptr)
		{
			return;
		}
		Generic->SetReturnAddress(
			AddReferenceForCast(
				static_cast<FReferenceRoot*>(
					Generic->GetObject()),
				bConst));
	}

	inline void GenericDerivedToRootMutable(
		asIScriptGeneric* Generic)
	{
		GenericDerivedToRoot(Generic, false);
	}

	inline void GenericDerivedToRootConst(
		asIScriptGeneric* Generic)
	{
		GenericDerivedToRoot(Generic, true);
	}

	inline void GenericRootToDerived(
		asIScriptGeneric* Generic,
		const bool bConst)
	{
		if (Generic == nullptr)
		{
			return;
		}
		Generic->SetReturnAddress(
			DowncastReference(
				static_cast<FReferenceRoot*>(
					Generic->GetObject()),
				bConst));
	}

	inline void GenericRootToDerivedMutable(
		asIScriptGeneric* Generic)
	{
		GenericRootToDerived(Generic, false);
	}

	inline void GenericRootToDerivedConst(
		asIScriptGeneric* Generic)
	{
		GenericRootToDerived(Generic, true);
	}

	inline void GenericSameReference(
		asIScriptGeneric* Generic)
	{
		if (Generic == nullptr)
		{
			return;
		}
		FReferenceState* const State =
			GetReferenceState(*Generic);
		if (State != nullptr)
		{
			++State->SameReferenceCalls;
		}
		FReferenceRoot* First = nullptr;
		if (FReferenceRoot** const FirstHandleAddress =
			static_cast<FReferenceRoot**>(
				Generic->GetArgAddress(0)))
		{
			First = *FirstHandleAddress;
		}
		FReferenceRoot* Second = nullptr;
		if (FReferenceRoot** const SecondHandleAddress =
			static_cast<FReferenceRoot**>(
				Generic->GetArgAddress(1)))
		{
			Second = *SecondHandleAddress;
		}
		Generic->SetReturnByte(
			First == Second);
	}

	inline bool RegisterReferenceType(
		asIScriptEngine& Engine,
		const ANSICHAR* const TypeName,
		const bool bRegisterWeakReference)
	{
		if (Engine.RegisterObjectType(
			TypeName,
			0,
			asOBJ_REF | asOBJ_IMPLICIT_HANDLE) < 0
			|| Engine.RegisterObjectBehaviour(
				TypeName,
				asBEHAVE_ADDREF,
				"void f()",
				asFUNCTION(GenericReferenceAddRef),
				asCALL_GENERIC) < 0
			|| Engine.RegisterObjectBehaviour(
				TypeName,
				asBEHAVE_RELEASE,
				"void f()",
				asFUNCTION(GenericReferenceRelease),
				asCALL_GENERIC) < 0)
		{
			return false;
		}
		if (bRegisterWeakReference
			&& Engine.RegisterObjectBehaviour(
				TypeName,
				asBEHAVE_GET_WEAKREF_FLAG,
				"int &f()",
				asFUNCTION(GenericGetWeakFlag),
				asCALL_GENERIC) < 0)
		{
			return false;
		}
		return Engine.RegisterObjectMethod(
				TypeName,
				"int GetIdentity() const",
				asFUNCTION(GenericGetIdentity),
				asCALL_GENERIC) >= 0
			&& Engine.RegisterObjectMethod(
				TypeName,
				"int GetKind() const",
				asFUNCTION(GenericGetKind),
				asCALL_GENERIC) >= 0
			&& Engine.RegisterObjectMethod(
				TypeName,
				"int GetValue() const",
				asFUNCTION(GenericGetValue),
				asCALL_GENERIC) >= 0
			&& Engine.RegisterObjectMethod(
				TypeName,
				"void SetValue(int Value)",
				asFUNCTION(GenericSetValue),
				asCALL_GENERIC) >= 0
			&& Engine.RegisterObjectMethod(
				TypeName,
				"int GetReferenceCount() const",
				asFUNCTION(GenericGetReferenceCount),
				asCALL_GENERIC) >= 0
			&& Engine.RegisterObjectMethod(
				TypeName,
				"void Link(const FRefRoot&in Other)",
				asFUNCTION(GenericLinkReference),
				asCALL_GENERIC) >= 0
			&& Engine.RegisterObjectMethod(
				TypeName,
				"void ClearPeer()",
				asFUNCTION(GenericClearReferencePeer),
				asCALL_GENERIC) >= 0
			&& Engine.RegisterObjectMethod(
				TypeName,
				"int GetPeerIdentity() const",
				asFUNCTION(GenericGetPeerIdentity),
				asCALL_GENERIC) >= 0;
	}

	inline bool RegisterReferenceCasts(
		asIScriptEngine& Engine)
	{
		return Engine.RegisterObjectMethod(
			"FRefDerived",
			"FRefRoot opImplCast()",
			asFUNCTION(GenericDerivedToRootMutable),
			asCALL_GENERIC) >= 0
			&& Engine.RegisterObjectMethod(
				"FRefDerived",
				"const FRefRoot opImplCast() const",
				asFUNCTION(GenericDerivedToRootConst),
				asCALL_GENERIC) >= 0
			&& Engine.RegisterObjectMethod(
				"FRefRoot",
				"FRefDerived opCast()",
				asFUNCTION(GenericRootToDerivedMutable),
				asCALL_GENERIC) >= 0
			&& Engine.RegisterObjectMethod(
				"FRefRoot",
				"const FRefDerived opCast() const",
				asFUNCTION(GenericRootToDerivedConst),
				asCALL_GENERIC) >= 0;
	}

	inline bool RegisterReferenceFactories(
		asIScriptEngine& Engine)
	{
		return Engine.RegisterGlobalFunction(
			"FRefRoot MakeRefRoot(int Value)",
			asFUNCTION(GenericMakeRoot),
			asCALL_GENERIC) >= 0
			&& Engine.RegisterGlobalFunction(
				"FRefDerived MakeRefDerived(int Value)",
				asFUNCTION(GenericMakeDerived),
				asCALL_GENERIC) >= 0
			&& Engine.RegisterGlobalFunction(
				"FRefRoot MakeRefDerivedAsRoot(int Value)",
				asFUNCTION(GenericMakeDerivedAsRoot),
				asCALL_GENERIC) >= 0
			&& Engine.RegisterGlobalFunction(
				"FRefUnrelated MakeRefUnrelated(int Value)",
				asFUNCTION(GenericMakeUnrelated),
				asCALL_GENERIC) >= 0
			&& Engine.RegisterGlobalFunction(
				"FRefRoot MakeNullRef()",
				asFUNCTION(GenericMakeNull),
				asCALL_GENERIC) >= 0
			&& Engine.RegisterGlobalFunction(
				"FRefRoot GetNativeRef(int Value)",
				asFUNCTION(GenericGetNativeObject),
				asCALL_GENERIC) >= 0
			&& Engine.RegisterGlobalFunction(
				"bool SameReference(const FRefRoot&in First, const FRefRoot&in Second)",
				asFUNCTION(GenericSameReference),
				asCALL_GENERIC) >= 0
			&& Engine.RegisterGlobalFunction(
				"bool SameReference(const FRefRoot&in First, const FRefDerived&in Second)",
				asFUNCTION(GenericSameReference),
				asCALL_GENERIC) >= 0
			&& Engine.RegisterGlobalFunction(
				"bool SameReference(const FRefDerived&in First, const FRefRoot&in Second)",
				asFUNCTION(GenericSameReference),
				asCALL_GENERIC) >= 0
			&& Engine.RegisterGlobalFunction(
				"bool SameReference(const FRefDerived&in First, const FRefDerived&in Second)",
				asFUNCTION(GenericSameReference),
				asCALL_GENERIC) >= 0;
	}

	inline bool RegisterReferenceFixtures(
		asIScriptEngine& Engine,
		FReferenceState& State,
		const bool bRegisterWeakReference = false)
	{
		State.ScriptEngine = &Engine;
		Engine.SetUserData(
			&State,
			ReferenceStateUserDataSlot);
		return RegisterReferenceType(
			Engine,
			"FRefRoot",
			bRegisterWeakReference)
			&& RegisterReferenceType(
				Engine,
				"FRefDerived",
				bRegisterWeakReference)
			&& RegisterReferenceType(
				Engine,
				"FRefUnrelated",
				bRegisterWeakReference)
			&& RegisterReferenceCasts(Engine)
			&& RegisterReferenceFactories(Engine);
	}

	inline bool HasAnyError(
		const AngelscriptNativeTestSupport::FNativeTestEngine& Engine)
	{
		for (const AngelscriptNativeTestSupport::FNativeMessageEntry& Message
			: Engine.GetMessages().Entries)
		{
			if (Message.Type == asMSGTYPE_ERROR)
			{
				return true;
			}
		}
		return false;
	}

	inline bool HasDiagnosticContaining(
		const AngelscriptNativeTestSupport::FNativeTestEngine& Engine,
		const TCHAR* const Token)
	{
		for (const AngelscriptNativeTestSupport::FNativeMessageEntry& Message
			: Engine.GetMessages().Entries)
		{
			if (Message.Type == asMSGTYPE_ERROR
				&& Message.Message.Contains(
					Token,
					ESearchCase::IgnoreCase))
			{
				return true;
			}
		}
		return false;
	}

	// Keep failed generated-reference cases reviewable from the automation report.
	// PrintGeneratedAsSource records the source itself; these helpers attach the raw
	// SDK result, diagnostics, and remaining native ownership to the assertion that
	// failed. This is deliberately shared by all reference themes so a fork-specific
	// parser or API restriction cannot be reduced to an unexplained boolean failure.
	inline FString DescribeReferenceBuild(
		const AngelscriptNativeTestSupport::FNativeTestEngine& Engine,
		const int BuildResult)
	{
		return FString::Printf(
			TEXT("Build=%d Diagnostics={%s}"),
			BuildResult,
			*Engine.GetMessagesText());
	}

	inline FString DescribeReferenceState(
		const FReferenceState& State)
	{
		TArray<int32> Identities;
		State.CurrentReferenceCounts.GetKeys(Identities);
		Identities.Sort();

		FString ReferenceCounts;
		for (const int32 Identity : Identities)
		{
			if (!ReferenceCounts.IsEmpty())
			{
				ReferenceCounts += TEXT(", ");
			}
			ReferenceCounts += FString::Printf(
				TEXT("%d:%d"),
				Identity,
				State.CurrentReferenceCounts.FindRef(Identity));
		}

		return FString::Printf(
			TEXT("Created=%d Destroyed=%d Live=%d AddRef=%d Release=%d CurrentRefCounts={%s}"),
			State.Created,
			State.Destroyed,
			State.LiveObjects,
			State.AddRefCalls,
			State.ReleaseCalls,
			*ReferenceCounts);
	}

	inline int CompileAndPrint(
		FAutomationTestBase& Test,
		asIScriptEngine& ScriptEngine,
		const FString& SourceId,
		const FString& ModuleName,
		const FString& Source,
		asIScriptModule*& OutModule)
	{
		AngelscriptNativeTestSupport::PrintGeneratedAsSource(
			Test,
			SourceId,
			ModuleName,
			Source);
		const FTCHARToUTF8 ModuleNameUtf8(*ModuleName);
		const FTCHARToUTF8 SourceUtf8(*Source);
		return AngelscriptNativeTestSupport::CompileNativeModule(
			&ScriptEngine,
			ModuleNameUtf8.Get(),
			SourceUtf8.Get(),
			OutModule);
	}

	inline void DiscardReferenceModule(
		asIScriptEngine& ScriptEngine,
		const FString& ModuleName)
	{
		const FTCHARToUTF8 ModuleNameUtf8(*ModuleName);
		ScriptEngine.DiscardModule(
			ModuleNameUtf8.Get());
	}
}
