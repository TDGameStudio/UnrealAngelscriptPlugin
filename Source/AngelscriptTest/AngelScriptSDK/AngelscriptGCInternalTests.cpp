#include "AngelscriptTestUtilities.h"
#include "AngelscriptTestMacros.h"
#include "CQTest.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_gc.h"
#include "source/as_scriptengine.h"
#include "EndAngelscriptHeaders.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	struct FGCProbeObject;

	static asIScriptEngine* GGCProbeScriptEngine = nullptr;

	struct FGCProbeObject
	{
		static int32 LiveCount;

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

	int32 FGCProbeObject::LiveCount = 0;

	void GCProbeAddRef(FGCProbeObject* Self)
	{
		Self->AddRef();
	}

	void GCProbeRelease(FGCProbeObject* Self)
	{
		Self->Release();
	}

	int GCProbeGetRefCount(FGCProbeObject* Self)
	{
		return Self->GetRefCount();
	}

	void GCProbeSetGCFlag(FGCProbeObject* Self)
	{
		Self->SetGCFlag();
	}

	bool GCProbeGetGCFlag(FGCProbeObject* Self)
	{
		return Self->GetGCFlag();
	}

	void GCProbeEnumReferences(FGCProbeObject* Self, int&)
	{
		int Dummy = 0;
		Self->EnumReferences(Dummy);
	}

	void GCProbeReleaseAllReferences(FGCProbeObject* Self, int&)
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

	FGCStatisticsSnapshot GetGCStatisticsSnapshot(asIScriptEngine& ScriptEngine)
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

	bool RegisterGCProbeType(FNoDiscardAsserter& Assert, asIScriptEngine& ScriptEngine, asITypeInfo*& OutType)
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

	FGCProbeObject* CreateSelfCycle(asIScriptEngine& ScriptEngine, asITypeInfo& Type)
	{
		FGCProbeObject* Node = new FGCProbeObject();
		Node->LinkTo(Node);
		ScriptEngine.NotifyGarbageCollectorOfNewObject(Node, &Type);
		return Node;
	}

	FGCProbeObject* CreateTwoNodeCycle(asIScriptEngine& ScriptEngine, asITypeInfo& Type)
	{
		FGCProbeObject* A = new FGCProbeObject();
		FGCProbeObject* B = new FGCProbeObject();
		A->LinkTo(B);
		B->LinkTo(A);
		ScriptEngine.NotifyGarbageCollectorOfNewObject(A, &Type);
		ScriptEngine.NotifyGarbageCollectorOfNewObject(B, &Type);
		return A;
	}

	bool RunFullGarbageCollection(asIScriptEngine& ScriptEngine)
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
}


TEST_CLASS_WITH_FLAGS(FAngelscriptGCInternalTests,
	"Angelscript.TestModule.AngelScriptSDK.GC",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(Statistics)
	{
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine);
		asCGarbageCollector Collector;
		Collector.engine = static_cast<asCScriptEngine*>(Engine.GetScriptEngine());

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
		}
	}

	TEST_METHOD(EmptyCollect)
	{
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine);
		asCGarbageCollector Collector;
		Collector.engine = static_cast<asCScriptEngine*>(Engine.GetScriptEngine());

		const int Result = Collector.GarbageCollect(asGC_FULL_CYCLE, 1);
		ASSERT_THAT(AreEqual(0, Result,
			TEXT("GC full cycle on an empty collector should complete immediately")));
		}
	}

	TEST_METHOD(InvalidLookup)
	{
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine);
		asCGarbageCollector Collector;
		Collector.engine = static_cast<asCScriptEngine*>(Engine.GetScriptEngine());

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
		}
	}

	TEST_METHOD(ReportUndestroyedEmpty)
	{
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine);
		asCGarbageCollector Collector;
		Collector.engine = static_cast<asCScriptEngine*>(Engine.GetScriptEngine());

		const int Result = Collector.ReportAndReleaseUndestroyedObjects();
		ASSERT_THAT(AreEqual(0, Result,
			TEXT("ReportAndReleaseUndestroyedObjects should return zero when no objects are tracked")));
		}
	}

	TEST_METHOD(ManualCycleCollection)
	{
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine);
		asIScriptEngine* ScriptEngine = Engine.GetScriptEngine();
		asITypeInfo* GCProbeType = nullptr;
		if (!RegisterGCProbeType(this->Assert, *ScriptEngine, GCProbeType))
		{
			return;
		}

		FGCProbeObject::LiveCount = 0;
		FGCProbeObject* Node = CreateSelfCycle(*ScriptEngine, *GCProbeType);
		ASSERT_THAT(IsNotNull(Node,
			TEXT("Manual GC cycle test should create a self-referencing probe object")));

		const FGCStatisticsSnapshot BeforeRelease = GetGCStatisticsSnapshot(*ScriptEngine);
		Node->Release();

		ASSERT_THAT(IsTrue(RunFullGarbageCollection(*ScriptEngine),
			TEXT("Manual GC cycle test should finish a full collection pass")));

		const FGCStatisticsSnapshot AfterCollect = GetGCStatisticsSnapshot(*ScriptEngine);
		ASSERT_THAT(IsTrue(AfterCollect.TotalDestroyed > BeforeRelease.TotalDestroyed,
			TEXT("Manual GC should destroy at least one released cyclic object")));
		ASSERT_THAT(IsTrue(AfterCollect.CurrentSize <= BeforeRelease.CurrentSize,
			TEXT("Manual GC should not increase the number of tracked objects after collecting a released cycle")));
		ASSERT_THAT(AreEqual(0, FGCProbeObject::LiveCount,
			TEXT("Manual GC should leave no probe objects alive after collecting a self-cycle")));
		}
	}

	TEST_METHOD(CycleDetection)
	{
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine);
		asIScriptEngine* ScriptEngine = Engine.GetScriptEngine();
		asITypeInfo* GCProbeType = nullptr;
		if (!RegisterGCProbeType(this->Assert, *ScriptEngine, GCProbeType))
		{
			return;
		}

		FGCProbeObject::LiveCount = 0;
		FGCProbeObject* Root = CreateSelfCycle(*ScriptEngine, *GCProbeType);
		ASSERT_THAT(IsNotNull(Root,
			TEXT("GC cycle detection test should create a self cycle")));

		const FGCStatisticsSnapshot BeforeRelease = GetGCStatisticsSnapshot(*ScriptEngine);
		Root->Release();

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
		}
	}

	TEST_METHOD(TwoNodeCycleCollection)
	{
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine);
		asIScriptEngine* ScriptEngine = Engine.GetScriptEngine();
		asITypeInfo* GCProbeType = nullptr;
		if (!RegisterGCProbeType(this->Assert, *ScriptEngine, GCProbeType))
		{
			return;
		}

		FGCProbeObject::LiveCount = 0;
		FGCProbeObject* Root = CreateTwoNodeCycle(*ScriptEngine, *GCProbeType);
		ASSERT_THAT(IsNotNull(Root,
			TEXT("GC two-node cycle collection test should create the root probe object")));

		FGCProbeObject* Peer = Root->Peer;
		ASSERT_THAT(IsNotNull(Peer,
			TEXT("GC two-node cycle collection test should create the peer probe object")));

		ASSERT_THAT(AreNotEqual(Root, Peer,
			TEXT("GC two-node cycle collection test should build two distinct probe objects")));
		ASSERT_THAT(AreEqual(2, FGCProbeObject::LiveCount,
			TEXT("GC two-node cycle collection test should start with two live probe objects")));

		const FGCStatisticsSnapshot BeforeRelease = GetGCStatisticsSnapshot(*ScriptEngine);
		Root->Release();
		Peer->Release();

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
		}
	}
};

#endif
