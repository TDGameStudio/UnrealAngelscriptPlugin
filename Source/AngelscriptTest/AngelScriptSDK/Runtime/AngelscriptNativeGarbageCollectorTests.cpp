#include "../Support/AngelscriptNativeCoreTestSupport.h"
#include "../Support/AngelscriptNativeCaseTestSupport.h"
#include "CQTest.h"
#include "Misc/ScopeExit.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_gc.h"
#include "source/as_scriptengine.h"
#include "EndAngelscriptHeaders.h"

#if WITH_ANGELSCRIPT_UNITTESTS



TEST_CLASS_WITH_FLAGS(FGarbageCollectorTests,
	"Angelscript.TestModule.AngelScriptSDK.Runtime.GarbageCollector",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	struct FGCProbeObject;

	inline static asIScriptEngine* GGCProbeScriptEngine = nullptr;

	struct FGCProbeObject
	{
		inline static int32 LiveCount = 0;

		explicit FGCProbeObject()
		{
			++LiveCount;
		}

		~FGCProbeObject()
		{
			if (Peer != nullptr)
			{
				FGCProbeObject* Referenced = Peer;
				Peer = nullptr;
				Referenced->Release();
			}

			--LiveCount;
		}

		void AddRef()
		{
			++RefCount;
		}

		void Release()
		{
			if (--RefCount == 0)
			{
				delete this;
			}
		}

		int GetRefCount() const
		{
			return RefCount;
		}

		void SetGCFlag()
		{
			bGCFlag = true;
		}

		bool GetGCFlag() const
		{
			return bGCFlag;
		}

		void EnumReferences(int&)
		{
			if (Peer != nullptr && GGCProbeScriptEngine != nullptr)
			{
				GGCProbeScriptEngine->GCEnumCallback(Peer);
			}
		}

		void ReleaseAllReferences(int&)
		{
			if (Peer != nullptr)
			{
				FGCProbeObject* Referenced = Peer;
				Peer = nullptr;
				Referenced->Release();
			}
		}

		void LinkTo(FGCProbeObject* Other)
		{
			if (Peer == Other)
			{
				return;
			}

			if (Peer != nullptr)
			{
				Peer->Release();
			}

			Peer = Other;
			if (Peer != nullptr)
			{
				Peer->AddRef();
			}
		}

		int RefCount = 1;
		bool bGCFlag = false;
		FGCProbeObject* Peer = nullptr;
	};

	static void GCProbeAddRef(FGCProbeObject* Self)
	{
		Self->AddRef();
	}

	static void GCProbeRelease(FGCProbeObject* Self)
	{
		Self->Release();
	}

	static int GCProbeGetRefCount(FGCProbeObject* Self)
	{
		return Self->GetRefCount();
	}

	static void GCProbeSetGCFlag(FGCProbeObject* Self)
	{
		Self->SetGCFlag();
	}

	static bool GCProbeGetGCFlag(FGCProbeObject* Self)
	{
		return Self->GetGCFlag();
	}

	static void GCProbeEnumReferences(FGCProbeObject* Self, int&)
	{
		int Dummy = 0;
		Self->EnumReferences(Dummy);
	}

	static void GCProbeReleaseAllReferences(FGCProbeObject* Self, int&)
	{
		int Dummy = 0;
		Self->ReleaseAllReferences(Dummy);
	}

	struct FGCStatisticsSnapshot
	{
		asUINT CurrentSize = 0;
		asUINT TotalDestroyed = 0;
		asUINT TotalDetected = 0;
		asUINT NewObjects = 0;
		asUINT TotalNewDestroyed = 0;
	};

	static FGCStatisticsSnapshot GetGCStatisticsSnapshot(asIScriptEngine& ScriptEngine)
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

	static bool RegisterGCProbeType(FNoDiscardAsserter& Assert, asIScriptEngine& ScriptEngine, asITypeInfo*& OutType)
	{
		GGCProbeScriptEngine = &ScriptEngine;
		OutType = ScriptEngine.GetTypeInfoByName("GCProbeObject");
		if (OutType != nullptr)
		{
			return true;
		}

		const int RegisterTypeResult = ScriptEngine.RegisterObjectType("GCProbeObject", 0, asOBJ_REF | asOBJ_GC);
		if (!Assert.IsTrue(RegisterTypeResult >= 0 || RegisterTypeResult == asALREADY_REGISTERED,
			TEXT("GC probe object type should register successfully")))
		{
			return false;
		}

		const bool bBehavioursRegistered =
			ScriptEngine.RegisterObjectBehaviour("GCProbeObject", asBEHAVE_ADDREF, "void f()", asFUNCTION(GCProbeAddRef), asCALL_CDECL_OBJFIRST) >= 0 &&
			ScriptEngine.RegisterObjectBehaviour("GCProbeObject", asBEHAVE_RELEASE, "void f()", asFUNCTION(GCProbeRelease), asCALL_CDECL_OBJFIRST) >= 0 &&
			ScriptEngine.RegisterObjectBehaviour("GCProbeObject", asBEHAVE_GETREFCOUNT, "int f()", asFUNCTION(GCProbeGetRefCount), asCALL_CDECL_OBJFIRST) >= 0 &&
			ScriptEngine.RegisterObjectBehaviour("GCProbeObject", asBEHAVE_SETGCFLAG, "void f()", asFUNCTION(GCProbeSetGCFlag), asCALL_CDECL_OBJFIRST) >= 0 &&
			ScriptEngine.RegisterObjectBehaviour("GCProbeObject", asBEHAVE_GETGCFLAG, "bool f()", asFUNCTION(GCProbeGetGCFlag), asCALL_CDECL_OBJFIRST) >= 0 &&
			ScriptEngine.RegisterObjectBehaviour("GCProbeObject", asBEHAVE_ENUMREFS, "void f(int&in gcCycle)" , asFUNCTION(GCProbeEnumReferences), asCALL_CDECL_OBJFIRST) >= 0 &&
			ScriptEngine.RegisterObjectBehaviour("GCProbeObject", asBEHAVE_RELEASEREFS, "void f(int&in gcCycle)", asFUNCTION(GCProbeReleaseAllReferences), asCALL_CDECL_OBJFIRST) >= 0;
		if (!Assert.IsTrue(bBehavioursRegistered, TEXT("GC probe object should register all GC behaviours")))
		{
			return false;
		}

		OutType = ScriptEngine.GetTypeInfoByName("GCProbeObject");
		return Assert.IsNotNull(OutType, TEXT("GC probe object should be visible through the type system"));
	}

	static FGCProbeObject* CreateSelfCycle(asIScriptEngine& ScriptEngine, asITypeInfo& Type, int& OutNotifyResult)
	{
		FGCProbeObject* Node = new FGCProbeObject();
		Node->LinkTo(Node);
		OutNotifyResult = ScriptEngine.NotifyGarbageCollectorOfNewObject(Node, &Type);
		return Node;
	}

	static FGCProbeObject* CreateTwoNodeCycle(
		asIScriptEngine& ScriptEngine,
		asITypeInfo& Type,
		int& OutFirstNotifyResult,
		int& OutSecondNotifyResult)
	{
		FGCProbeObject* A = new FGCProbeObject();
		FGCProbeObject* B = new FGCProbeObject();
		A->LinkTo(B);
		B->LinkTo(A);
		OutFirstNotifyResult = ScriptEngine.NotifyGarbageCollectorOfNewObject(A, &Type);
		OutSecondNotifyResult = ScriptEngine.NotifyGarbageCollectorOfNewObject(B, &Type);
		return A;
	}

	static void ReleaseUnregisteredCycle(FGCProbeObject* Root)
	{
		if (Root == nullptr)
		{
			return;
		}

		FGCProbeObject* Peer = Root->Peer;
		Root->LinkTo(nullptr);
		if (Peer != nullptr && Peer != Root)
		{
			Peer->LinkTo(nullptr);
		}

		Root->Release();
		if (Peer != nullptr && Peer != Root)
		{
			Peer->Release();
		}
	}

	struct FGCProbeEngineScope
	{
		~FGCProbeEngineScope()
		{
			ClearCallbackBinding();
			Engine.Destroy();
		}

		bool Initialize(FNoDiscardAsserter& Asserter, FAutomationTestBase& Test)
		{
			Engine.Create(Test);
			ScriptEngine = Engine.Get();
			if (!Asserter.IsNotNull(ScriptEngine, TEXT("GC probe scenario should create an isolated standalone engine")))
			{
				return false;
			}

			return RegisterGCProbeType(Asserter, *ScriptEngine, ProbeType);
		}

		void ClearCallbackBinding()
		{
			if (GGCProbeScriptEngine == ScriptEngine)
			{
				GGCProbeScriptEngine = nullptr;
			}
		}

		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		asIScriptEngine* ScriptEngine = nullptr;
		asITypeInfo* ProbeType = nullptr;
	};

	static bool RunFullGarbageCollection(asIScriptEngine& ScriptEngine)
	{
		for (int Iteration = 0; Iteration < 8; ++Iteration)
		{
			const int Result = ScriptEngine.GarbageCollect(asGC_FULL_CYCLE, 1);
			if (Result < 0)
			{
				return false;
			}
		}
		return true;
	}

	struct FGCProbeCycleCleanupScope
	{
		FGCProbeCycleCleanupScope(asIScriptEngine& InScriptEngine, FGCProbeObject* InRoot)
			: ScriptEngine(InScriptEngine)
			, Root(InRoot)
		{
		}

		~FGCProbeCycleCleanupScope()
		{
			Cleanup();
		}

		void MarkExternalReferencesReleased()
		{
			bExternalReferencesHeld = false;
		}

		bool Cleanup()
		{
			if (bCleanupComplete)
			{
				return FGCProbeObject::LiveCount == 0;
			}

			if (FGCProbeObject::LiveCount == 0)
			{
				bCleanupComplete = true;
				return true;
			}

			if (bExternalReferencesHeld)
			{
				ReleaseUnregisteredCycle(Root);
				bExternalReferencesHeld = false;
			}

			bCleanupComplete =
				RunFullGarbageCollection(ScriptEngine) && FGCProbeObject::LiveCount == 0;
			return bCleanupComplete;
		}

		asIScriptEngine& ScriptEngine;
		FGCProbeObject* Root = nullptr;
		bool bExternalReferencesHeld = true;
		bool bCleanupComplete = false;
	};
public:
	TEST_METHOD(GarbageCollectorStatistics)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT("RT-GC-EMPTY-SERVICE-CONTRACTS",
			ENativeEvidence::Runtime
			| ENativeEvidence::Metadata
			| ENativeEvidence::Diagnostic
			| ENativeEvidence::Cleanup
			| ENativeEvidence::Isolation);

		FNativeTestEngine Engine;
		FNativeTestEngine IsolatedEngine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			IsolatedEngine.Destroy();
			Engine.Destroy();
		};
		asIScriptEngine* ScriptEngine = Engine.Get();
		if (ScriptEngine == nullptr)
		{
			TestRunner->AddError(TEXT("Native GC tests should create a standalone engine"));
			return;
		}
		asCGarbageCollector Collector;
		Collector.engine = static_cast<asCScriptEngine*>(ScriptEngine);
		ASSERT_THAT(AreEqual(static_cast<asCScriptEngine*>(ScriptEngine), Collector.engine,
			TEXT("Primary direct collector should bind to the primary standalone engine")));

		asUINT CurrentSize = MAX_uint32;
		asUINT TotalDestroyed = MAX_uint32;
		asUINT TotalDetected = MAX_uint32;
		asUINT NewObjects = MAX_uint32;
		asUINT TotalNewDestroyed = MAX_uint32;
		Collector.GetStatistics(&CurrentSize, &TotalDestroyed, &TotalDetected, &NewObjects, &TotalNewDestroyed);

		ASSERT_THAT(AreEqual(0u, CurrentSize,
			TEXT("Fresh GC collector should start with zero tracked objects")));
		ASSERT_THAT(AreEqual(0u, TotalDestroyed,
			TEXT("Fresh GC collector should start with zero destroyed objects")));
		ASSERT_THAT(AreEqual(0u, TotalDetected,
			TEXT("Fresh GC collector should start with zero detected cycles")));
		ASSERT_THAT(AreEqual(0u, NewObjects,
			TEXT("Fresh GC collector should start with zero new objects")));
		ASSERT_THAT(AreEqual(0u, TotalNewDestroyed,
			TEXT("Fresh GC collector should start with zero newly destroyed objects")));

		const int EmptyCollectResult = Collector.GarbageCollect(asGC_FULL_CYCLE, 1);
		ASSERT_THAT(AreEqual(0, EmptyCollectResult,
			TEXT("Fresh GC collector should complete an empty full cycle immediately")));

		asUINT SequenceNumber = 123;
		void* Object = reinterpret_cast<void*>(0x1);
		asITypeInfo* Type = reinterpret_cast<asITypeInfo*>(0x1);
		const int InvalidLookupResult = Collector.GetObjectInGC(0, &SequenceNumber, &Object, &Type);
		ASSERT_THAT(AreEqual(asINVALID_ARG, InvalidLookupResult,
			TEXT("Fresh GC collector should reject an out-of-range lookup")));
		ASSERT_THAT(AreEqual(0u, SequenceNumber,
			TEXT("Fresh GC collector should reset the lookup sequence number on failure")));
		ASSERT_THAT(IsNull(Object,
			TEXT("Fresh GC collector should reset the lookup object pointer on failure")));
		ASSERT_THAT(IsNull(Type,
			TEXT("Fresh GC collector should reset the lookup type pointer on failure")));

		asUINT PublicSequenceNumber = 456;
		void* PublicObject = reinterpret_cast<void*>(0x2);
		asITypeInfo* PublicType = reinterpret_cast<asITypeInfo*>(0x2);
		const int PublicInvalidLookupResult =
			ScriptEngine->GetObjectInGC(0, &PublicSequenceNumber, &PublicObject, &PublicType);
		ASSERT_THAT(AreEqual(asINVALID_ARG, PublicInvalidLookupResult,
			TEXT("Fresh engine GC service should reject an out-of-range lookup")));
		ASSERT_THAT(AreEqual(0u, PublicSequenceNumber,
			TEXT("Fresh engine GC service should reset the lookup sequence number on failure")));
		ASSERT_THAT(IsNull(PublicObject,
			TEXT("Fresh engine GC service should reset the lookup object pointer on failure")));
		ASSERT_THAT(IsNull(PublicType,
			TEXT("Fresh engine GC service should reset the lookup type pointer on failure")));

		const int UndestroyedResult = Collector.ReportAndReleaseUndestroyedObjects();
		ASSERT_THAT(AreEqual(0, UndestroyedResult,
			TEXT("Fresh GC collector should report zero undestroyed objects")));

		Collector.GetStatistics(&CurrentSize, &TotalDestroyed, &TotalDetected, &NewObjects, &TotalNewDestroyed);
		ASSERT_THAT(AreEqual(0u, CurrentSize,
			TEXT("Empty collector operations should leave the primary collector untracked")));
		ASSERT_THAT(AreEqual(0u, TotalDestroyed,
			TEXT("Empty collector operations should leave the primary destruction count unchanged")));
		ASSERT_THAT(AreEqual(0u, TotalDetected,
			TEXT("Empty collector operations should leave the primary detected-cycle count unchanged")));
		ASSERT_THAT(AreEqual(0u, NewObjects,
			TEXT("Empty collector operations should leave the primary new-object count unchanged")));
		ASSERT_THAT(AreEqual(0u, TotalNewDestroyed,
			TEXT("Empty collector operations should leave the primary new-destruction count unchanged")));

		asUINT PublicCurrentSize = MAX_uint32;
		asUINT PublicTotalDestroyed = MAX_uint32;
		asUINT PublicTotalDetected = MAX_uint32;
		asUINT PublicNewObjects = MAX_uint32;
		asUINT PublicTotalNewDestroyed = MAX_uint32;
		ScriptEngine->GetGCStatistics(
			&PublicCurrentSize,
			&PublicTotalDestroyed,
			&PublicTotalDetected,
			&PublicNewObjects,
			&PublicTotalNewDestroyed);
		ASSERT_THAT(AreEqual(0u, PublicCurrentSize,
			TEXT("Direct collector operations should leave the primary engine GC service untracked")));
		ASSERT_THAT(AreEqual(0u, PublicTotalDestroyed,
			TEXT("Direct collector operations should leave the primary engine destruction count unchanged")));
		ASSERT_THAT(AreEqual(0u, PublicTotalDetected,
			TEXT("Direct collector operations should leave the primary engine detected-cycle count unchanged")));
		ASSERT_THAT(AreEqual(0u, PublicNewObjects,
			TEXT("Direct collector operations should leave the primary engine new-object count unchanged")));
		ASSERT_THAT(AreEqual(0u, PublicTotalNewDestroyed,
			TEXT("Direct collector operations should leave the primary engine new-destruction count unchanged")));

		IsolatedEngine.Create(*TestRunner);
		asIScriptEngine* IsolatedScriptEngine = IsolatedEngine.Get();
		if (IsolatedScriptEngine == nullptr)
		{
			TestRunner->AddError(TEXT("Empty GC isolation check should create a second standalone engine"));
			return;
		}

		ASSERT_THAT(AreNotEqual(ScriptEngine, IsolatedScriptEngine,
			TEXT("Empty GC isolation check should use a distinct standalone engine")));
		asCGarbageCollector IsolatedCollector;
		IsolatedCollector.engine = static_cast<asCScriptEngine*>(IsolatedScriptEngine);
		ASSERT_THAT(AreEqual(static_cast<asCScriptEngine*>(IsolatedScriptEngine), IsolatedCollector.engine,
			TEXT("Isolated direct collector should bind to the isolated standalone engine")));
		ASSERT_THAT(AreNotEqual(&Collector, &IsolatedCollector,
			TEXT("Isolation check should use a distinct direct collector instance")));

		asUINT IsolatedCurrentSize = MAX_uint32;
		asUINT IsolatedTotalDestroyed = MAX_uint32;
		asUINT IsolatedTotalDetected = MAX_uint32;
		asUINT IsolatedNewObjects = MAX_uint32;
		asUINT IsolatedTotalNewDestroyed = MAX_uint32;
		IsolatedCollector.GetStatistics(
			&IsolatedCurrentSize,
			&IsolatedTotalDestroyed,
			&IsolatedTotalDetected,
			&IsolatedNewObjects,
			&IsolatedTotalNewDestroyed);
		ASSERT_THAT(AreEqual(0u, IsolatedCurrentSize,
			TEXT("Isolated collector should start with zero tracked objects")));
		ASSERT_THAT(AreEqual(0u, IsolatedTotalDestroyed,
			TEXT("Isolated collector should start with zero destroyed objects")));
		ASSERT_THAT(AreEqual(0u, IsolatedTotalDetected,
			TEXT("Isolated collector should start with zero detected cycles")));
		ASSERT_THAT(AreEqual(0u, IsolatedNewObjects,
			TEXT("Isolated collector should start with zero new objects")));
		ASSERT_THAT(AreEqual(0u, IsolatedTotalNewDestroyed,
			TEXT("Isolated collector should start with zero newly destroyed objects")));
		ASSERT_THAT(AreEqual(0, IsolatedCollector.GarbageCollect(asGC_FULL_CYCLE, 1),
			TEXT("Isolated collector should independently complete an empty full cycle")));
		ASSERT_THAT(AreEqual(0, IsolatedCollector.ReportAndReleaseUndestroyedObjects(),
			TEXT("Isolated collector cleanup should independently report zero undestroyed objects")));

		IsolatedCollector.GetStatistics(
			&IsolatedCurrentSize,
			&IsolatedTotalDestroyed,
			&IsolatedTotalDetected,
			&IsolatedNewObjects,
			&IsolatedTotalNewDestroyed);
		ASSERT_THAT(AreEqual(0u, IsolatedCurrentSize,
			TEXT("Isolated collector cleanup should leave zero tracked objects")));
		ASSERT_THAT(AreEqual(0u, IsolatedTotalDestroyed,
			TEXT("Isolated collector cleanup should leave its destruction count unchanged")));
		ASSERT_THAT(AreEqual(0u, IsolatedTotalDetected,
			TEXT("Isolated collector cleanup should leave its detected-cycle count unchanged")));
		ASSERT_THAT(AreEqual(0u, IsolatedNewObjects,
			TEXT("Isolated collector cleanup should leave zero new objects")));
		ASSERT_THAT(AreEqual(0u, IsolatedTotalNewDestroyed,
			TEXT("Isolated collector cleanup should leave its new-destruction count unchanged")));

		asUINT IsolatedPublicCurrentSize = MAX_uint32;
		asUINT IsolatedPublicTotalDestroyed = MAX_uint32;
		asUINT IsolatedPublicTotalDetected = MAX_uint32;
		asUINT IsolatedPublicNewObjects = MAX_uint32;
		asUINT IsolatedPublicTotalNewDestroyed = MAX_uint32;
		IsolatedScriptEngine->GetGCStatistics(
			&IsolatedPublicCurrentSize,
			&IsolatedPublicTotalDestroyed,
			&IsolatedPublicTotalDetected,
			&IsolatedPublicNewObjects,
			&IsolatedPublicTotalNewDestroyed);
		ASSERT_THAT(AreEqual(0u, IsolatedPublicCurrentSize,
			TEXT("Isolated direct collector operations should leave the isolated engine GC service untracked")));
		ASSERT_THAT(AreEqual(0u, IsolatedPublicTotalDestroyed,
			TEXT("Isolated direct collector operations should leave the isolated engine destruction count unchanged")));
		ASSERT_THAT(AreEqual(0u, IsolatedPublicTotalDetected,
			TEXT("Isolated direct collector operations should leave the isolated engine detected-cycle count unchanged")));
		ASSERT_THAT(AreEqual(0u, IsolatedPublicNewObjects,
			TEXT("Isolated direct collector operations should leave the isolated engine new-object count unchanged")));
		ASSERT_THAT(AreEqual(0u, IsolatedPublicTotalNewDestroyed,
			TEXT("Isolated direct collector operations should leave the isolated engine new-destruction count unchanged")));

		Collector.GetStatistics(&CurrentSize, &TotalDestroyed, &TotalDetected, &NewObjects, &TotalNewDestroyed);
		ASSERT_THAT(AreEqual(0u, CurrentSize,
			TEXT("Isolated collector activity should not add tracked objects to the primary collector")));
		ASSERT_THAT(AreEqual(0u, TotalDestroyed,
			TEXT("Isolated collector activity should not change the primary destruction count")));
		ASSERT_THAT(AreEqual(0u, TotalDetected,
			TEXT("Isolated collector activity should not change the primary detected-cycle count")));
		ASSERT_THAT(AreEqual(0u, NewObjects,
			TEXT("Isolated collector activity should not change the primary new-object count")));
		ASSERT_THAT(AreEqual(0u, TotalNewDestroyed,
			TEXT("Isolated collector activity should not change the primary new-destruction count")));

		ScriptEngine->GetGCStatistics(
			&PublicCurrentSize,
			&PublicTotalDestroyed,
			&PublicTotalDetected,
			&PublicNewObjects,
			&PublicTotalNewDestroyed);
		ASSERT_THAT(AreEqual(0u, PublicCurrentSize,
			TEXT("Isolated engine activity should not add tracked objects to the primary engine GC service")));
		ASSERT_THAT(AreEqual(0u, PublicTotalDestroyed,
			TEXT("Isolated engine activity should not change the primary engine destruction count")));
		ASSERT_THAT(AreEqual(0u, PublicTotalDetected,
			TEXT("Isolated engine activity should not change the primary engine detected-cycle count")));
		ASSERT_THAT(AreEqual(0u, PublicNewObjects,
			TEXT("Isolated engine activity should not change the primary engine new-object count")));
		ASSERT_THAT(AreEqual(0u, PublicTotalNewDestroyed,
			TEXT("Isolated engine activity should not change the primary engine new-destruction count")));
	}

	TEST_METHOD(GarbageCollectorEmptyCollect)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_NON_PRODUCT("AggregateSupport",
			"The empty full-collection result is the collect operation of RT-GC-EMPTY-SERVICE-CONTRACTS.");

		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};
		asIScriptEngine* ScriptEngine = Engine.Get();
		if (ScriptEngine == nullptr)
		{
			TestRunner->AddError(TEXT("Native GC tests should create a standalone engine"));
			return;
		}
		asCGarbageCollector Collector;
		Collector.engine = static_cast<asCScriptEngine*>(ScriptEngine);

		const int Result = Collector.GarbageCollect(asGC_FULL_CYCLE, 1);
		ASSERT_THAT(AreEqual(0, Result,
			TEXT("GC full cycle on an empty collector should complete immediately")));
	}

	TEST_METHOD(GarbageCollectorInvalidLookup)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_NON_PRODUCT("AggregateSupport",
			"The invalid empty lookup outputs are the lookup operation of RT-GC-EMPTY-SERVICE-CONTRACTS.");

		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};
		asIScriptEngine* ScriptEngine = Engine.Get();
		if (ScriptEngine == nullptr)
		{
			TestRunner->AddError(TEXT("Native GC tests should create a standalone engine"));
			return;
		}
		asCGarbageCollector Collector;
		Collector.engine = static_cast<asCScriptEngine*>(ScriptEngine);

		asUINT SeqNbr = 123;
		void* Object = reinterpret_cast<void*>(0x1);
		asITypeInfo* Type = reinterpret_cast<asITypeInfo*>(0x1);
		const int Result = Collector.GetObjectInGC(0, &SeqNbr, &Object, &Type);

		ASSERT_THAT(AreEqual(asINVALID_ARG, Result,
			TEXT("GetObjectInGC should reject out-of-range lookups on an empty collector")));
		ASSERT_THAT(AreEqual(0u, SeqNbr,
			TEXT("GetObjectInGC should zero the sequence number on failure")));
		ASSERT_THAT(AreEqual(static_cast<void*>(nullptr), Object,
			TEXT("GetObjectInGC should null the object pointer on failure")));
		ASSERT_THAT(AreEqual(static_cast<asITypeInfo*>(nullptr), Type,
			TEXT("GetObjectInGC should null the type pointer on failure")));

		SeqNbr = 456;
		Object = reinterpret_cast<void*>(0x2);
		Type = reinterpret_cast<asITypeInfo*>(0x2);
		const int PublicResult =
			ScriptEngine->GetObjectInGC(0, &SeqNbr, &Object, &Type);
		ASSERT_THAT(AreEqual(
			asINVALID_ARG,
			PublicResult,
			TEXT("asIScriptEngine GetObjectInGC should reject an empty out-of-range lookup")));
		ASSERT_THAT(AreEqual(
			0u,
			SeqNbr,
			TEXT("Public GC lookup should zero the sequence number on failure")));
		ASSERT_THAT(IsNull(
			Object,
			TEXT("Public GC lookup should null the object pointer on failure")));
		ASSERT_THAT(IsNull(
			Type,
			TEXT("Public GC lookup should null the type pointer on failure")));
	}

	TEST_METHOD(ReportUndestroyedEmpty)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_NON_PRODUCT("AggregateSupport",
			"The empty undestroyed-object report is the report operation of RT-GC-EMPTY-SERVICE-CONTRACTS.");

		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};
		asIScriptEngine* ScriptEngine = Engine.Get();
		if (ScriptEngine == nullptr)
		{
			TestRunner->AddError(TEXT("Native GC tests should create a standalone engine"));
			return;
		}
		asCGarbageCollector Collector;
		Collector.engine = static_cast<asCScriptEngine*>(ScriptEngine);

		const int Result = Collector.ReportAndReleaseUndestroyedObjects();
		ASSERT_THAT(AreEqual(0, Result,
			TEXT("ReportAndReleaseUndestroyedObjects should return zero when no objects are tracked")));
	}

	TEST_METHOD(ManualCycleCollection)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT("RT-GC-CYCLE-TOPOLOGY-PHASES",
			ENativeEvidence::Runtime
			| ENativeEvidence::Metadata
			| ENativeEvidence::Lifecycle
			| ENativeEvidence::Cleanup
			| ENativeEvidence::Isolation);

		{
			TestRunner->AddInfo(TEXT("GC case: self_cycle_detect_then_full"));
			ASSERT_THAT(AreEqual(0, FGCProbeObject::LiveCount,
				TEXT("self_cycle_detect_then_full should begin with no live probe objects")));

			FGCProbeEngineScope Scenario;
			const bool bInitialized = Scenario.Initialize(this->Assert, *TestRunner);
			ASSERT_THAT(IsTrue(bInitialized,
				TEXT("self_cycle_detect_then_full should initialize an independent engine and probe type")));
			if (!bInitialized)
			{
				return;
			}
			ASSERT_THAT(IsTrue(GGCProbeScriptEngine == Scenario.ScriptEngine,
				TEXT("self_cycle_detect_then_full callbacks should bind to the case-owned engine")));

			const FGCStatisticsSnapshot Baseline = GetGCStatisticsSnapshot(*Scenario.ScriptEngine);
			ASSERT_THAT(AreEqual(0u, Baseline.CurrentSize,
				TEXT("self_cycle_detect_then_full should begin with zero tracked objects")));
			ASSERT_THAT(AreEqual(0u, Baseline.TotalDestroyed,
				TEXT("self_cycle_detect_then_full should begin with zero destroyed objects")));
			ASSERT_THAT(AreEqual(0u, Baseline.TotalDetected,
				TEXT("self_cycle_detect_then_full should begin with zero detected objects")));
			ASSERT_THAT(AreEqual(0u, Baseline.NewObjects,
				TEXT("self_cycle_detect_then_full should begin with zero new objects")));
			ASSERT_THAT(AreEqual(0u, Baseline.TotalNewDestroyed,
				TEXT("self_cycle_detect_then_full should begin with zero newly destroyed objects")));

			int NotifyResult = asERROR;
			FGCProbeObject* Root =
				CreateSelfCycle(*Scenario.ScriptEngine, *Scenario.ProbeType, NotifyResult);
			FGCProbeCycleCleanupScope Cleanup(*Scenario.ScriptEngine, Root);
			ASSERT_THAT(IsNotNull(Root,
				TEXT("self_cycle_detect_then_full should create its self-referencing probe")));
			ASSERT_THAT(AreEqual(0, NotifyResult,
				TEXT("self_cycle_detect_then_full should register its first GC object with sequence zero")));
			if (NotifyResult < 0)
			{
				ASSERT_THAT(IsTrue(Cleanup.Cleanup(),
					TEXT("self_cycle_detect_then_full should clean an object rejected by GC notification")));
				Scenario.ClearCallbackBinding();
				return;
			}

			const FGCStatisticsSnapshot AfterCreate = GetGCStatisticsSnapshot(*Scenario.ScriptEngine);
			ASSERT_THAT(AreEqual(Baseline.CurrentSize + 1, AfterCreate.CurrentSize,
				TEXT("self_cycle_detect_then_full should add exactly one tracked object")));
			ASSERT_THAT(AreEqual(Baseline.TotalDestroyed, AfterCreate.TotalDestroyed,
				TEXT("self_cycle_detect_then_full creation should not destroy an object")));
			ASSERT_THAT(AreEqual(Baseline.TotalDetected, AfterCreate.TotalDetected,
				TEXT("self_cycle_detect_then_full creation should not detect an object")));
			ASSERT_THAT(AreEqual(Baseline.NewObjects + 1, AfterCreate.NewObjects,
				TEXT("self_cycle_detect_then_full should add exactly one new object")));
			ASSERT_THAT(AreEqual(Baseline.TotalNewDestroyed, AfterCreate.TotalNewDestroyed,
				TEXT("self_cycle_detect_then_full creation should not destroy a new object")));
			ASSERT_THAT(AreEqual(1, FGCProbeObject::LiveCount,
				TEXT("self_cycle_detect_then_full should create exactly one live probe")));

			Root->Release();
			Cleanup.MarkExternalReferencesReleased();
			const FGCStatisticsSnapshot AfterRelease = GetGCStatisticsSnapshot(*Scenario.ScriptEngine);
			ASSERT_THAT(AreEqual(AfterCreate.CurrentSize, AfterRelease.CurrentSize,
				TEXT("self_cycle_detect_then_full should remain tracked after releasing its external reference")));
			ASSERT_THAT(AreEqual(AfterCreate.TotalDestroyed, AfterRelease.TotalDestroyed,
				TEXT("self_cycle_detect_then_full external release should not destroy the cycle")));
			ASSERT_THAT(AreEqual(AfterCreate.TotalDetected, AfterRelease.TotalDetected,
				TEXT("self_cycle_detect_then_full external release should not detect the cycle")));
			ASSERT_THAT(AreEqual(AfterCreate.NewObjects, AfterRelease.NewObjects,
				TEXT("self_cycle_detect_then_full external release should preserve the new-object count")));
			ASSERT_THAT(AreEqual(AfterCreate.TotalNewDestroyed, AfterRelease.TotalNewDestroyed,
				TEXT("self_cycle_detect_then_full external release should not destroy a new object")));
			ASSERT_THAT(AreEqual(1, FGCProbeObject::LiveCount,
				TEXT("self_cycle_detect_then_full should remain alive before detection")));

			const int DetectResult =
				Scenario.ScriptEngine->GarbageCollect(asGC_FULL_CYCLE | asGC_DETECT_GARBAGE, 1);
			ASSERT_THAT(AreEqual(0, DetectResult,
				TEXT("self_cycle_detect_then_full detect-only full cycle should complete")));

			const FGCStatisticsSnapshot AfterDetect = GetGCStatisticsSnapshot(*Scenario.ScriptEngine);
			ASSERT_THAT(AreEqual(AfterCreate.CurrentSize, AfterDetect.CurrentSize,
				TEXT("self_cycle_detect_then_full detection should retain its tracked object")));
			ASSERT_THAT(AreEqual(AfterCreate.TotalDestroyed, AfterDetect.TotalDestroyed,
				TEXT("self_cycle_detect_then_full detection should not destroy its object")));
			ASSERT_THAT(IsTrue(AfterDetect.TotalDetected >= Baseline.TotalDetected + 1,
				TEXT("self_cycle_detect_then_full should detect at least one cyclic object")));
			ASSERT_THAT(AreEqual(Baseline.NewObjects, AfterDetect.NewObjects,
				TEXT("self_cycle_detect_then_full detection should move its object out of the new list")));
			ASSERT_THAT(AreEqual(Baseline.TotalNewDestroyed, AfterDetect.TotalNewDestroyed,
				TEXT("self_cycle_detect_then_full detection should not destroy a new object")));
			ASSERT_THAT(AreEqual(1, FGCProbeObject::LiveCount,
				TEXT("self_cycle_detect_then_full detection should retain its live object")));

			const int FullCollectResult = Scenario.ScriptEngine->GarbageCollect(asGC_FULL_CYCLE, 1);
			ASSERT_THAT(AreEqual(0, FullCollectResult,
				TEXT("self_cycle_detect_then_full ordinary full collection should complete")));

			const FGCStatisticsSnapshot AfterCollect = GetGCStatisticsSnapshot(*Scenario.ScriptEngine);
			ASSERT_THAT(AreEqual(Baseline.CurrentSize, AfterCollect.CurrentSize,
				TEXT("self_cycle_detect_then_full should return to its tracked-object baseline")));
			ASSERT_THAT(AreEqual(Baseline.TotalDestroyed + 1, AfterCollect.TotalDestroyed,
				TEXT("self_cycle_detect_then_full should destroy exactly one object")));
			ASSERT_THAT(IsTrue(AfterCollect.TotalDetected >= AfterDetect.TotalDetected,
				TEXT("self_cycle_detect_then_full cleanup should preserve the detected-object total")));
			ASSERT_THAT(AreEqual(Baseline.NewObjects, AfterCollect.NewObjects,
				TEXT("self_cycle_detect_then_full should finish with zero new objects")));
			ASSERT_THAT(AreEqual(Baseline.TotalNewDestroyed, AfterCollect.TotalNewDestroyed,
				TEXT("self_cycle_detect_then_full should destroy its old-list object, not a new-list object")));
			ASSERT_THAT(AreEqual(0, FGCProbeObject::LiveCount,
				TEXT("self_cycle_detect_then_full should leave no live probes")));
			ASSERT_THAT(IsTrue(Cleanup.Cleanup(),
				TEXT("self_cycle_detect_then_full cleanup guard should observe a clean case")));
			Scenario.ClearCallbackBinding();
			ASSERT_THAT(IsNull(GGCProbeScriptEngine,
				TEXT("self_cycle_detect_then_full should clear its callback engine before destruction")));
		}

		{
			TestRunner->AddInfo(TEXT("GC case: self_cycle_direct_full"));
			ASSERT_THAT(AreEqual(0, FGCProbeObject::LiveCount,
				TEXT("self_cycle_direct_full should begin with no live probe objects")));

			FGCProbeEngineScope Scenario;
			const bool bInitialized = Scenario.Initialize(this->Assert, *TestRunner);
			ASSERT_THAT(IsTrue(bInitialized,
				TEXT("self_cycle_direct_full should initialize an independent engine and probe type")));
			if (!bInitialized)
			{
				return;
			}
			ASSERT_THAT(IsTrue(GGCProbeScriptEngine == Scenario.ScriptEngine,
				TEXT("self_cycle_direct_full callbacks should bind to the case-owned engine")));

			const FGCStatisticsSnapshot Baseline = GetGCStatisticsSnapshot(*Scenario.ScriptEngine);
			ASSERT_THAT(AreEqual(0u, Baseline.CurrentSize,
				TEXT("self_cycle_direct_full should begin with zero tracked objects")));
			ASSERT_THAT(AreEqual(0u, Baseline.TotalDestroyed,
				TEXT("self_cycle_direct_full should begin with zero destroyed objects")));
			ASSERT_THAT(AreEqual(0u, Baseline.TotalDetected,
				TEXT("self_cycle_direct_full should begin with zero detected objects")));
			ASSERT_THAT(AreEqual(0u, Baseline.NewObjects,
				TEXT("self_cycle_direct_full should begin with zero new objects")));
			ASSERT_THAT(AreEqual(0u, Baseline.TotalNewDestroyed,
				TEXT("self_cycle_direct_full should begin with zero newly destroyed objects")));

			int NotifyResult = asERROR;
			FGCProbeObject* Root =
				CreateSelfCycle(*Scenario.ScriptEngine, *Scenario.ProbeType, NotifyResult);
			FGCProbeCycleCleanupScope Cleanup(*Scenario.ScriptEngine, Root);
			ASSERT_THAT(IsNotNull(Root,
				TEXT("self_cycle_direct_full should create its self-referencing probe")));
			ASSERT_THAT(AreEqual(0, NotifyResult,
				TEXT("self_cycle_direct_full should register its first GC object with sequence zero")));
			if (NotifyResult < 0)
			{
				ASSERT_THAT(IsTrue(Cleanup.Cleanup(),
					TEXT("self_cycle_direct_full should clean an object rejected by GC notification")));
				Scenario.ClearCallbackBinding();
				return;
			}

			const FGCStatisticsSnapshot AfterCreate = GetGCStatisticsSnapshot(*Scenario.ScriptEngine);
			ASSERT_THAT(AreEqual(Baseline.CurrentSize + 1, AfterCreate.CurrentSize,
				TEXT("self_cycle_direct_full should add exactly one tracked object")));
			ASSERT_THAT(AreEqual(Baseline.TotalDestroyed, AfterCreate.TotalDestroyed,
				TEXT("self_cycle_direct_full creation should not destroy an object")));
			ASSERT_THAT(AreEqual(Baseline.TotalDetected, AfterCreate.TotalDetected,
				TEXT("self_cycle_direct_full creation should not detect an object")));
			ASSERT_THAT(AreEqual(Baseline.NewObjects + 1, AfterCreate.NewObjects,
				TEXT("self_cycle_direct_full should add exactly one new object")));
			ASSERT_THAT(AreEqual(Baseline.TotalNewDestroyed, AfterCreate.TotalNewDestroyed,
				TEXT("self_cycle_direct_full creation should not destroy a new object")));
			ASSERT_THAT(AreEqual(1, FGCProbeObject::LiveCount,
				TEXT("self_cycle_direct_full should create exactly one live probe")));

			Root->Release();
			Cleanup.MarkExternalReferencesReleased();
			const FGCStatisticsSnapshot AfterRelease = GetGCStatisticsSnapshot(*Scenario.ScriptEngine);
			ASSERT_THAT(AreEqual(AfterCreate.CurrentSize, AfterRelease.CurrentSize,
				TEXT("self_cycle_direct_full should remain tracked after releasing its external reference")));
			ASSERT_THAT(AreEqual(AfterCreate.TotalDestroyed, AfterRelease.TotalDestroyed,
				TEXT("self_cycle_direct_full external release should not destroy the cycle")));
			ASSERT_THAT(AreEqual(AfterCreate.TotalDetected, AfterRelease.TotalDetected,
				TEXT("self_cycle_direct_full should not run detection before ordinary full collection")));
			ASSERT_THAT(AreEqual(AfterCreate.NewObjects, AfterRelease.NewObjects,
				TEXT("self_cycle_direct_full external release should preserve the new-object count")));
			ASSERT_THAT(AreEqual(AfterCreate.TotalNewDestroyed, AfterRelease.TotalNewDestroyed,
				TEXT("self_cycle_direct_full external release should not destroy a new object")));
			ASSERT_THAT(AreEqual(1, FGCProbeObject::LiveCount,
				TEXT("self_cycle_direct_full should remain alive before ordinary full collection")));

			const int FullCollectResult = Scenario.ScriptEngine->GarbageCollect(asGC_FULL_CYCLE, 1);
			ASSERT_THAT(AreEqual(0, FullCollectResult,
				TEXT("self_cycle_direct_full ordinary full collection should complete without a prior detect call")));

			const FGCStatisticsSnapshot AfterCollect = GetGCStatisticsSnapshot(*Scenario.ScriptEngine);
			ASSERT_THAT(AreEqual(Baseline.CurrentSize, AfterCollect.CurrentSize,
				TEXT("self_cycle_direct_full should return to its tracked-object baseline")));
			ASSERT_THAT(AreEqual(Baseline.TotalDestroyed + 1, AfterCollect.TotalDestroyed,
				TEXT("self_cycle_direct_full should destroy exactly one object")));
			ASSERT_THAT(IsTrue(AfterCollect.TotalDetected >= Baseline.TotalDetected + 1,
				TEXT("self_cycle_direct_full should detect its self cycle inside the ordinary full collection")));
			ASSERT_THAT(AreEqual(Baseline.NewObjects, AfterCollect.NewObjects,
				TEXT("self_cycle_direct_full should finish with zero new objects")));
			ASSERT_THAT(AreEqual(Baseline.TotalNewDestroyed, AfterCollect.TotalNewDestroyed,
				TEXT("self_cycle_direct_full should destroy its promoted old-list object")));
			ASSERT_THAT(AreEqual(0, FGCProbeObject::LiveCount,
				TEXT("self_cycle_direct_full should leave no live probes")));
			ASSERT_THAT(IsTrue(Cleanup.Cleanup(),
				TEXT("self_cycle_direct_full cleanup guard should observe a clean case")));
			Scenario.ClearCallbackBinding();
			ASSERT_THAT(IsNull(GGCProbeScriptEngine,
				TEXT("self_cycle_direct_full should clear its callback engine before destruction")));
		}

		{
			TestRunner->AddInfo(TEXT("GC case: two_node_detect_then_full"));
			ASSERT_THAT(AreEqual(0, FGCProbeObject::LiveCount,
				TEXT("two_node_detect_then_full should begin with no live probe objects")));

			FGCProbeEngineScope Scenario;
			const bool bInitialized = Scenario.Initialize(this->Assert, *TestRunner);
			ASSERT_THAT(IsTrue(bInitialized,
				TEXT("two_node_detect_then_full should initialize an independent engine and probe type")));
			if (!bInitialized)
			{
				return;
			}
			ASSERT_THAT(IsTrue(GGCProbeScriptEngine == Scenario.ScriptEngine,
				TEXT("two_node_detect_then_full callbacks should bind to the case-owned engine")));

			const FGCStatisticsSnapshot Baseline = GetGCStatisticsSnapshot(*Scenario.ScriptEngine);
			ASSERT_THAT(AreEqual(0u, Baseline.CurrentSize,
				TEXT("two_node_detect_then_full should begin with zero tracked objects")));
			ASSERT_THAT(AreEqual(0u, Baseline.TotalDestroyed,
				TEXT("two_node_detect_then_full should begin with zero destroyed objects")));
			ASSERT_THAT(AreEqual(0u, Baseline.TotalDetected,
				TEXT("two_node_detect_then_full should begin with zero detected objects")));
			ASSERT_THAT(AreEqual(0u, Baseline.NewObjects,
				TEXT("two_node_detect_then_full should begin with zero new objects")));
			ASSERT_THAT(AreEqual(0u, Baseline.TotalNewDestroyed,
				TEXT("two_node_detect_then_full should begin with zero newly destroyed objects")));

			int FirstNotifyResult = asERROR;
			int SecondNotifyResult = asERROR;
			FGCProbeObject* Root = CreateTwoNodeCycle(
				*Scenario.ScriptEngine,
				*Scenario.ProbeType,
				FirstNotifyResult,
				SecondNotifyResult);
			FGCProbeCycleCleanupScope Cleanup(*Scenario.ScriptEngine, Root);
			ASSERT_THAT(IsNotNull(Root,
				TEXT("two_node_detect_then_full should create its root probe")));
			FGCProbeObject* Peer = Root->Peer;
			ASSERT_THAT(IsNotNull(Peer,
				TEXT("two_node_detect_then_full should create its peer probe")));
			ASSERT_THAT(AreNotEqual(Root, Peer,
				TEXT("two_node_detect_then_full should create two distinct probes")));
			ASSERT_THAT(AreEqual(0, FirstNotifyResult,
				TEXT("two_node_detect_then_full should register its first GC object with sequence zero")));
			ASSERT_THAT(AreEqual(1, SecondNotifyResult,
				TEXT("two_node_detect_then_full should register its second GC object with sequence one")));
			if (FirstNotifyResult < 0 || SecondNotifyResult < 0)
			{
				ASSERT_THAT(IsTrue(Cleanup.Cleanup(),
					TEXT("two_node_detect_then_full should clean a partially registered cycle")));
				Scenario.ClearCallbackBinding();
				return;
			}

			const FGCStatisticsSnapshot AfterCreate = GetGCStatisticsSnapshot(*Scenario.ScriptEngine);
			ASSERT_THAT(AreEqual(Baseline.CurrentSize + 2, AfterCreate.CurrentSize,
				TEXT("two_node_detect_then_full should add exactly two tracked objects")));
			ASSERT_THAT(AreEqual(Baseline.TotalDestroyed, AfterCreate.TotalDestroyed,
				TEXT("two_node_detect_then_full creation should not destroy an object")));
			ASSERT_THAT(AreEqual(Baseline.TotalDetected, AfterCreate.TotalDetected,
				TEXT("two_node_detect_then_full creation should not detect an object")));
			ASSERT_THAT(AreEqual(Baseline.NewObjects + 2, AfterCreate.NewObjects,
				TEXT("two_node_detect_then_full should add exactly two new objects")));
			ASSERT_THAT(AreEqual(Baseline.TotalNewDestroyed, AfterCreate.TotalNewDestroyed,
				TEXT("two_node_detect_then_full creation should not destroy a new object")));
			ASSERT_THAT(AreEqual(2, FGCProbeObject::LiveCount,
				TEXT("two_node_detect_then_full should create exactly two live probes")));

			Root->Release();
			Peer->Release();
			Cleanup.MarkExternalReferencesReleased();
			const FGCStatisticsSnapshot AfterRelease = GetGCStatisticsSnapshot(*Scenario.ScriptEngine);
			ASSERT_THAT(AreEqual(AfterCreate.CurrentSize, AfterRelease.CurrentSize,
				TEXT("two_node_detect_then_full should remain tracked after releasing external references")));
			ASSERT_THAT(AreEqual(AfterCreate.TotalDestroyed, AfterRelease.TotalDestroyed,
				TEXT("two_node_detect_then_full external release should not destroy the cycle")));
			ASSERT_THAT(AreEqual(AfterCreate.TotalDetected, AfterRelease.TotalDetected,
				TEXT("two_node_detect_then_full external release should not detect the cycle")));
			ASSERT_THAT(AreEqual(AfterCreate.NewObjects, AfterRelease.NewObjects,
				TEXT("two_node_detect_then_full external release should preserve the new-object count")));
			ASSERT_THAT(AreEqual(AfterCreate.TotalNewDestroyed, AfterRelease.TotalNewDestroyed,
				TEXT("two_node_detect_then_full external release should not destroy a new object")));
			ASSERT_THAT(AreEqual(2, FGCProbeObject::LiveCount,
				TEXT("two_node_detect_then_full should keep both probes alive before detection")));

			const int DetectResult =
				Scenario.ScriptEngine->GarbageCollect(asGC_FULL_CYCLE | asGC_DETECT_GARBAGE, 1);
			ASSERT_THAT(AreEqual(0, DetectResult,
				TEXT("two_node_detect_then_full detect-only full cycle should complete")));

			const FGCStatisticsSnapshot AfterDetect = GetGCStatisticsSnapshot(*Scenario.ScriptEngine);
			ASSERT_THAT(AreEqual(AfterCreate.CurrentSize, AfterDetect.CurrentSize,
				TEXT("two_node_detect_then_full detection should retain both tracked objects")));
			ASSERT_THAT(AreEqual(AfterCreate.TotalDestroyed, AfterDetect.TotalDestroyed,
				TEXT("two_node_detect_then_full detection should not destroy either object")));
			ASSERT_THAT(IsTrue(AfterDetect.TotalDetected >= Baseline.TotalDetected + 2,
				TEXT("two_node_detect_then_full should detect at least two cyclic objects")));
			ASSERT_THAT(AreEqual(Baseline.NewObjects, AfterDetect.NewObjects,
				TEXT("two_node_detect_then_full detection should move both objects out of the new list")));
			ASSERT_THAT(AreEqual(Baseline.TotalNewDestroyed, AfterDetect.TotalNewDestroyed,
				TEXT("two_node_detect_then_full detection should not destroy a new object")));
			ASSERT_THAT(AreEqual(2, FGCProbeObject::LiveCount,
				TEXT("two_node_detect_then_full detection should retain both live objects")));

			const int FullCollectResult = Scenario.ScriptEngine->GarbageCollect(asGC_FULL_CYCLE, 1);
			ASSERT_THAT(AreEqual(0, FullCollectResult,
				TEXT("two_node_detect_then_full ordinary full collection should complete")));

			const FGCStatisticsSnapshot AfterCollect = GetGCStatisticsSnapshot(*Scenario.ScriptEngine);
			ASSERT_THAT(AreEqual(Baseline.CurrentSize, AfterCollect.CurrentSize,
				TEXT("two_node_detect_then_full should return to its tracked-object baseline")));
			ASSERT_THAT(AreEqual(Baseline.TotalDestroyed + 2, AfterCollect.TotalDestroyed,
				TEXT("two_node_detect_then_full should destroy exactly two objects")));
			ASSERT_THAT(IsTrue(AfterCollect.TotalDetected >= AfterDetect.TotalDetected,
				TEXT("two_node_detect_then_full cleanup should preserve the detected-object total")));
			ASSERT_THAT(AreEqual(Baseline.NewObjects, AfterCollect.NewObjects,
				TEXT("two_node_detect_then_full should finish with zero new objects")));
			ASSERT_THAT(AreEqual(Baseline.TotalNewDestroyed, AfterCollect.TotalNewDestroyed,
				TEXT("two_node_detect_then_full should destroy its old-list objects")));
			ASSERT_THAT(AreEqual(0, FGCProbeObject::LiveCount,
				TEXT("two_node_detect_then_full should leave no live probes")));
			ASSERT_THAT(IsTrue(Cleanup.Cleanup(),
				TEXT("two_node_detect_then_full cleanup guard should observe a clean case")));
			Scenario.ClearCallbackBinding();
			ASSERT_THAT(IsNull(GGCProbeScriptEngine,
				TEXT("two_node_detect_then_full should clear its callback engine before destruction")));
		}

		{
			TestRunner->AddInfo(TEXT("GC case: two_node_direct_full"));
			ASSERT_THAT(AreEqual(0, FGCProbeObject::LiveCount,
				TEXT("two_node_direct_full should begin with no live probe objects")));

			FGCProbeEngineScope Scenario;
			const bool bInitialized = Scenario.Initialize(this->Assert, *TestRunner);
			ASSERT_THAT(IsTrue(bInitialized,
				TEXT("two_node_direct_full should initialize an independent engine and probe type")));
			if (!bInitialized)
			{
				return;
			}
			ASSERT_THAT(IsTrue(GGCProbeScriptEngine == Scenario.ScriptEngine,
				TEXT("two_node_direct_full callbacks should bind to the case-owned engine")));

			const FGCStatisticsSnapshot Baseline = GetGCStatisticsSnapshot(*Scenario.ScriptEngine);
			ASSERT_THAT(AreEqual(0u, Baseline.CurrentSize,
				TEXT("two_node_direct_full should begin with zero tracked objects")));
			ASSERT_THAT(AreEqual(0u, Baseline.TotalDestroyed,
				TEXT("two_node_direct_full should begin with zero destroyed objects")));
			ASSERT_THAT(AreEqual(0u, Baseline.TotalDetected,
				TEXT("two_node_direct_full should begin with zero detected objects")));
			ASSERT_THAT(AreEqual(0u, Baseline.NewObjects,
				TEXT("two_node_direct_full should begin with zero new objects")));
			ASSERT_THAT(AreEqual(0u, Baseline.TotalNewDestroyed,
				TEXT("two_node_direct_full should begin with zero newly destroyed objects")));

			int FirstNotifyResult = asERROR;
			int SecondNotifyResult = asERROR;
			FGCProbeObject* Root = CreateTwoNodeCycle(
				*Scenario.ScriptEngine,
				*Scenario.ProbeType,
				FirstNotifyResult,
				SecondNotifyResult);
			FGCProbeCycleCleanupScope Cleanup(*Scenario.ScriptEngine, Root);
			ASSERT_THAT(IsNotNull(Root,
				TEXT("two_node_direct_full should create its root probe")));
			FGCProbeObject* Peer = Root->Peer;
			ASSERT_THAT(IsNotNull(Peer,
				TEXT("two_node_direct_full should create its peer probe")));
			ASSERT_THAT(AreNotEqual(Root, Peer,
				TEXT("two_node_direct_full should create two distinct probes")));
			ASSERT_THAT(AreEqual(0, FirstNotifyResult,
				TEXT("two_node_direct_full should register its first GC object with sequence zero")));
			ASSERT_THAT(AreEqual(1, SecondNotifyResult,
				TEXT("two_node_direct_full should register its second GC object with sequence one")));
			if (FirstNotifyResult < 0 || SecondNotifyResult < 0)
			{
				ASSERT_THAT(IsTrue(Cleanup.Cleanup(),
					TEXT("two_node_direct_full should clean a partially registered cycle")));
				Scenario.ClearCallbackBinding();
				return;
			}

			const FGCStatisticsSnapshot AfterCreate = GetGCStatisticsSnapshot(*Scenario.ScriptEngine);
			ASSERT_THAT(AreEqual(Baseline.CurrentSize + 2, AfterCreate.CurrentSize,
				TEXT("two_node_direct_full should add exactly two tracked objects")));
			ASSERT_THAT(AreEqual(Baseline.TotalDestroyed, AfterCreate.TotalDestroyed,
				TEXT("two_node_direct_full creation should not destroy an object")));
			ASSERT_THAT(AreEqual(Baseline.TotalDetected, AfterCreate.TotalDetected,
				TEXT("two_node_direct_full creation should not detect an object")));
			ASSERT_THAT(AreEqual(Baseline.NewObjects + 2, AfterCreate.NewObjects,
				TEXT("two_node_direct_full should add exactly two new objects")));
			ASSERT_THAT(AreEqual(Baseline.TotalNewDestroyed, AfterCreate.TotalNewDestroyed,
				TEXT("two_node_direct_full creation should not destroy a new object")));
			ASSERT_THAT(AreEqual(2, FGCProbeObject::LiveCount,
				TEXT("two_node_direct_full should create exactly two live probes")));

			Root->Release();
			Peer->Release();
			Cleanup.MarkExternalReferencesReleased();
			const FGCStatisticsSnapshot AfterRelease = GetGCStatisticsSnapshot(*Scenario.ScriptEngine);
			ASSERT_THAT(AreEqual(AfterCreate.CurrentSize, AfterRelease.CurrentSize,
				TEXT("two_node_direct_full should remain tracked after releasing external references")));
			ASSERT_THAT(AreEqual(AfterCreate.TotalDestroyed, AfterRelease.TotalDestroyed,
				TEXT("two_node_direct_full external release should not destroy the cycle")));
			ASSERT_THAT(AreEqual(AfterCreate.TotalDetected, AfterRelease.TotalDetected,
				TEXT("two_node_direct_full should not run detection before ordinary full collection")));
			ASSERT_THAT(AreEqual(AfterCreate.NewObjects, AfterRelease.NewObjects,
				TEXT("two_node_direct_full external release should preserve the new-object count")));
			ASSERT_THAT(AreEqual(AfterCreate.TotalNewDestroyed, AfterRelease.TotalNewDestroyed,
				TEXT("two_node_direct_full external release should not destroy a new object")));
			ASSERT_THAT(AreEqual(2, FGCProbeObject::LiveCount,
				TEXT("two_node_direct_full should keep both probes alive before ordinary full collection")));

			const int FullCollectResult = Scenario.ScriptEngine->GarbageCollect(asGC_FULL_CYCLE, 1);
			ASSERT_THAT(AreEqual(0, FullCollectResult,
				TEXT("two_node_direct_full ordinary full collection should complete without a prior detect call")));

			const FGCStatisticsSnapshot AfterCollect = GetGCStatisticsSnapshot(*Scenario.ScriptEngine);
			ASSERT_THAT(AreEqual(Baseline.CurrentSize, AfterCollect.CurrentSize,
				TEXT("two_node_direct_full should return to its tracked-object baseline")));
			ASSERT_THAT(AreEqual(Baseline.TotalDestroyed + 2, AfterCollect.TotalDestroyed,
				TEXT("two_node_direct_full should destroy exactly two objects")));
			ASSERT_THAT(IsTrue(AfterCollect.TotalDetected >= Baseline.TotalDetected + 2,
				TEXT("two_node_direct_full should detect both objects inside the ordinary full collection")));
			ASSERT_THAT(AreEqual(Baseline.NewObjects, AfterCollect.NewObjects,
				TEXT("two_node_direct_full should finish with zero new objects")));
			ASSERT_THAT(AreEqual(Baseline.TotalNewDestroyed, AfterCollect.TotalNewDestroyed,
				TEXT("two_node_direct_full should destroy its promoted old-list objects")));
			ASSERT_THAT(AreEqual(0, FGCProbeObject::LiveCount,
				TEXT("two_node_direct_full should leave no live probes")));
			ASSERT_THAT(IsTrue(Cleanup.Cleanup(),
				TEXT("two_node_direct_full cleanup guard should observe a clean case")));
			Scenario.ClearCallbackBinding();
			ASSERT_THAT(IsNull(GGCProbeScriptEngine,
				TEXT("two_node_direct_full should clear its callback engine before destruction")));
		}
	}

	TEST_METHOD(GarbageCollectorCycleDetection)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_NON_PRODUCT("AggregateSupport",
			"Retained self-cycle support smoke for the legacy detect-then-destroy path; ManualCycleCollection remains the Exact Owner.");

		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			GGCProbeScriptEngine = nullptr;
			Engine.Destroy();
		};
		asIScriptEngine* ScriptEngine = Engine.Get();
		if (ScriptEngine == nullptr)
		{
			TestRunner->AddError(TEXT("Native GC tests should create a standalone engine"));
			return;
		}
		asITypeInfo* GCProbeType = nullptr;
		if (!RegisterGCProbeType(this->Assert, *ScriptEngine, GCProbeType))
		{
			return;
		}

		FGCProbeObject::LiveCount = 0;
		int NotifyResult = asERROR;
		FGCProbeObject* Root = CreateSelfCycle(*ScriptEngine, *GCProbeType, NotifyResult);
		FGCProbeCycleCleanupScope Cleanup(*ScriptEngine, Root);
		ASSERT_THAT(IsNotNull(Root,
			TEXT("GC cycle detection test should create a self cycle")));
		ASSERT_THAT(AreEqual(0, NotifyResult,
			TEXT("GC cycle detection test should register its first GC object with sequence zero")));
		if (NotifyResult < 0)
		{
			ASSERT_THAT(IsTrue(Cleanup.Cleanup(),
				TEXT("GC cycle detection test should clean an object rejected by GC notification")));
			return;
		}

		const FGCStatisticsSnapshot BeforeRelease = GetGCStatisticsSnapshot(*ScriptEngine);
		Root->Release();
		Cleanup.MarkExternalReferencesReleased();

		ASSERT_THAT(AreEqual(0, ScriptEngine->GarbageCollect(asGC_FULL_CYCLE | asGC_DETECT_GARBAGE, 1),
			TEXT("GC cycle detection should accept a detect-only full cycle")));

		const FGCStatisticsSnapshot AfterDetect = GetGCStatisticsSnapshot(*ScriptEngine);
		ASSERT_THAT(IsTrue(AfterDetect.TotalDetected >= BeforeRelease.TotalDetected + 1,
			TEXT("GC should detect at least one cyclic object after releasing a self-cycle")));
		ASSERT_THAT(IsTrue(AfterDetect.CurrentSize >= 1,
			TEXT("Detect-only GC should keep the cyclic object tracked until destroy runs")));

		ASSERT_THAT(IsTrue(RunFullGarbageCollection(*ScriptEngine),
			TEXT("GC cycle detection test should complete subsequent collection passes")));

		const FGCStatisticsSnapshot AfterCollect = GetGCStatisticsSnapshot(*ScriptEngine);
		ASSERT_THAT(IsTrue(AfterCollect.TotalDestroyed >= AfterDetect.TotalDestroyed,
			TEXT("GC should eventually destroy the detected cycle")));
		ASSERT_THAT(AreEqual(0, FGCProbeObject::LiveCount,
			TEXT("GC should leave no probe objects alive after collecting the detected self-cycle")));
		ASSERT_THAT(IsTrue(Cleanup.Cleanup(),
			TEXT("GC cycle detection cleanup guard should observe a clean case")));
	}

	TEST_METHOD(TwoNodeCycleCollection)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_NON_PRODUCT("AggregateSupport",
			"Retained two-node support smoke for the legacy detect-then-destroy path; ManualCycleCollection remains the Exact Owner.");

		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			GGCProbeScriptEngine = nullptr;
			Engine.Destroy();
		};
		asIScriptEngine* ScriptEngine = Engine.Get();
		if (ScriptEngine == nullptr)
		{
			TestRunner->AddError(TEXT("Native GC tests should create a standalone engine"));
			return;
		}
		asITypeInfo* GCProbeType = nullptr;
		if (!RegisterGCProbeType(this->Assert, *ScriptEngine, GCProbeType))
		{
			return;
		}

		FGCProbeObject::LiveCount = 0;
		int FirstNotifyResult = asERROR;
		int SecondNotifyResult = asERROR;
		FGCProbeObject* Root =
			CreateTwoNodeCycle(*ScriptEngine, *GCProbeType, FirstNotifyResult, SecondNotifyResult);
		FGCProbeCycleCleanupScope Cleanup(*ScriptEngine, Root);
		ASSERT_THAT(IsNotNull(Root,
			TEXT("GC two-node cycle collection test should create the root probe object")));

		FGCProbeObject* Peer = Root->Peer;
		ASSERT_THAT(IsNotNull(Peer,
			TEXT("GC two-node cycle collection test should create the peer probe object")));

		ASSERT_THAT(AreNotEqual(Root, Peer,
			TEXT("GC two-node cycle collection test should build two distinct probe objects")));
		ASSERT_THAT(AreEqual(2, FGCProbeObject::LiveCount,
			TEXT("GC two-node cycle collection test should start with two live probe objects")));
		ASSERT_THAT(AreEqual(0, FirstNotifyResult,
			TEXT("GC two-node cycle collection test should register its first GC object with sequence zero")));
		ASSERT_THAT(AreEqual(1, SecondNotifyResult,
			TEXT("GC two-node cycle collection test should register its second GC object with sequence one")));
		if (FirstNotifyResult < 0 || SecondNotifyResult < 0)
		{
			ASSERT_THAT(IsTrue(Cleanup.Cleanup(),
				TEXT("GC two-node cycle collection test should clean a partially registered cycle")));
			return;
		}

		const FGCStatisticsSnapshot BeforeRelease = GetGCStatisticsSnapshot(*ScriptEngine);
		Root->Release();
		Peer->Release();
		Cleanup.MarkExternalReferencesReleased();

		ASSERT_THAT(AreEqual(0, ScriptEngine->GarbageCollect(asGC_FULL_CYCLE | asGC_DETECT_GARBAGE, 1),
			TEXT("GC two-node cycle detection should accept a detect-only full cycle")));

		const FGCStatisticsSnapshot AfterDetect = GetGCStatisticsSnapshot(*ScriptEngine);
		ASSERT_THAT(IsTrue(AfterDetect.TotalDetected >= BeforeRelease.TotalDetected + 1,
			TEXT("GC should detect at least one released two-node cycle")));
		ASSERT_THAT(IsTrue(AfterDetect.CurrentSize >= BeforeRelease.CurrentSize,
			TEXT("Detect-only GC should keep both released cyclic objects tracked until destroy runs")));

		ASSERT_THAT(IsTrue(RunFullGarbageCollection(*ScriptEngine),
			TEXT("GC two-node cycle collection test should complete subsequent collection passes")));

		const FGCStatisticsSnapshot AfterCollect = GetGCStatisticsSnapshot(*ScriptEngine);
		ASSERT_THAT(IsTrue(AfterCollect.TotalDestroyed > AfterDetect.TotalDestroyed,
			TEXT("GC should destroy released two-node cycle objects during full collection")));
		ASSERT_THAT(IsTrue(AfterDetect.CurrentSize >= AfterCollect.CurrentSize + 2,
			TEXT("Full collection should remove both released cyclic probe objects from GC tracking")));
		ASSERT_THAT(AreEqual(0, FGCProbeObject::LiveCount,
			TEXT("GC should leave no probe objects alive after collecting the detected two-node cycle")));
		ASSERT_THAT(IsTrue(Cleanup.Cleanup(),
			TEXT("GC two-node cycle collection cleanup guard should observe a clean case")));
	}
};

#endif
