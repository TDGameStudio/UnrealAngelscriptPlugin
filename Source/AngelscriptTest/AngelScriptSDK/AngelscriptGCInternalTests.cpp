#include "AngelscriptTestUtilities.h"
#include "AngelscriptTestMacros.h"
#include "CQTest.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_gc.h"
#include "source/as_scriptengine.h"
#include "EndAngelscriptHeaders.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace AngelscriptTest_AngelScriptSDK_AngelscriptGCInternalTests_Private
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

	bool RegisterGCProbeType(FAutomationTestBase& Test, asIScriptEngine& ScriptEngine, asITypeInfo*& OutType)
	{
		GGCProbeScriptEngine = &ScriptEngine;
		OutType = ScriptEngine.GetTypeInfoByName("GCProbeObject");
		if (OutType != nullptr)
		{
			return true;
		}

		const int RegisterTypeResult = ScriptEngine.RegisterObjectType("GCProbeObject", 0, asOBJ_REF | asOBJ_GC);
		if (!Test.TestTrue(TEXT("GC probe object type should register successfully"), RegisterTypeResult >= 0 || RegisterTypeResult == asALREADY_REGISTERED))
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
		if (!Test.TestTrue(TEXT("GC probe object should register all GC behaviours"), bBehavioursRegistered))
		{
			return false;
		}

		OutType = ScriptEngine.GetTypeInfoByName("GCProbeObject");
		return Test.TestNotNull(TEXT("GC probe object should be visible through the type system"), OutType);
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
		using namespace AngelscriptTest_AngelScriptSDK_AngelscriptGCInternalTests_Private;
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

		TestRunner->TestEqual(TEXT("Fresh GC collector should start with zero tracked objects"), CurrentSize, 0u);
		TestRunner->TestEqual(TEXT("Fresh GC collector should start with zero destroyed objects"), TotalDestroyed, 0u);
		TestRunner->TestEqual(TEXT("Fresh GC collector should start with zero detected cycles"), TotalDetected, 0u);
		TestRunner->TestEqual(TEXT("Fresh GC collector should start with zero new objects"), NewObjects, 0u);
		TestRunner->TestEqual(TEXT("Fresh GC collector should start with zero newly destroyed objects"), TotalNewDestroyed, 0u);
		}
	}

	TEST_METHOD(EmptyCollect)
	{
		using namespace AngelscriptTest_AngelScriptSDK_AngelscriptGCInternalTests_Private;
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine);
		asCGarbageCollector Collector;
		Collector.engine = static_cast<asCScriptEngine*>(Engine.GetScriptEngine());

		const int Result = Collector.GarbageCollect(asGC_FULL_CYCLE, 1);
		TestRunner->TestEqual(TEXT("GC full cycle on an empty collector should complete immediately"), Result, 0);
		}
	}

	TEST_METHOD(InvalidLookup)
	{
		using namespace AngelscriptTest_AngelScriptSDK_AngelscriptGCInternalTests_Private;
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine);
		asCGarbageCollector Collector;
		Collector.engine = static_cast<asCScriptEngine*>(Engine.GetScriptEngine());

		asUINT SeqNbr = 123;
		void* Object = reinterpret_cast<void*>(0x1);
		asITypeInfo* Type = reinterpret_cast<asITypeInfo*>(0x1);
		const int Result = Collector.GetObjectInGC(0, &SeqNbr, &Object, &Type);

		TestRunner->TestEqual(TEXT("GetObjectInGC should reject out-of-range lookups on an empty collector"), Result, asINVALID_ARG);
		TestRunner->TestEqual(TEXT("GetObjectInGC should zero the sequence number on failure"), SeqNbr, 0u);
		TestRunner->TestEqual(TEXT("GetObjectInGC should null the object pointer on failure"), Object, static_cast<void*>(nullptr));
		TestRunner->TestEqual(TEXT("GetObjectInGC should null the type pointer on failure"), Type, static_cast<asITypeInfo*>(nullptr));
		}
	}

	TEST_METHOD(ReportUndestroyedEmpty)
	{
		using namespace AngelscriptTest_AngelScriptSDK_AngelscriptGCInternalTests_Private;
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine);
		asCGarbageCollector Collector;
		Collector.engine = static_cast<asCScriptEngine*>(Engine.GetScriptEngine());

		const int Result = Collector.ReportAndReleaseUndestroyedObjects();
		TestRunner->TestEqual(TEXT("ReportAndReleaseUndestroyedObjects should return zero when no objects are tracked"), Result, 0);
		}
	}

	TEST_METHOD(ManualCycleCollection)
	{
		using namespace AngelscriptTest_AngelScriptSDK_AngelscriptGCInternalTests_Private;
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine);
		asIScriptEngine* ScriptEngine = Engine.GetScriptEngine();
		asITypeInfo* GCProbeType = nullptr;
		if (!RegisterGCProbeType(*TestRunner, *ScriptEngine, GCProbeType))
		{
			return;
		}

		FGCProbeObject::LiveCount = 0;
		FGCProbeObject* Node = CreateSelfCycle(*ScriptEngine, *GCProbeType);
		if (!TestRunner->TestNotNull(TEXT("Manual GC cycle test should create a self-referencing probe object"), Node))
		{
			return;
		}

		const FGCStatisticsSnapshot BeforeRelease = GetGCStatisticsSnapshot(*ScriptEngine);
		Node->Release();

		if (!TestRunner->TestTrue(TEXT("Manual GC cycle test should finish a full collection pass"), RunFullGarbageCollection(*ScriptEngine)))
		{
			return;
		}

		const FGCStatisticsSnapshot AfterCollect = GetGCStatisticsSnapshot(*ScriptEngine);
		TestRunner->TestTrue(TEXT("Manual GC should destroy at least one released cyclic object"), AfterCollect.TotalDestroyed > BeforeRelease.TotalDestroyed);
		TestRunner->TestTrue(TEXT("Manual GC should not increase the number of tracked objects after collecting a released cycle"), AfterCollect.CurrentSize <= BeforeRelease.CurrentSize);
		TestRunner->TestEqual(TEXT("Manual GC should leave no probe objects alive after collecting a self-cycle"), FGCProbeObject::LiveCount, 0);
		}
	}

	TEST_METHOD(CycleDetection)
	{
		using namespace AngelscriptTest_AngelScriptSDK_AngelscriptGCInternalTests_Private;
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine);
		asIScriptEngine* ScriptEngine = Engine.GetScriptEngine();
		asITypeInfo* GCProbeType = nullptr;
		if (!RegisterGCProbeType(*TestRunner, *ScriptEngine, GCProbeType))
		{
			return;
		}

		FGCProbeObject::LiveCount = 0;
		FGCProbeObject* Root = CreateSelfCycle(*ScriptEngine, *GCProbeType);
		if (!TestRunner->TestNotNull(TEXT("GC cycle detection test should create a self cycle"), Root))
		{
			return;
		}

		const FGCStatisticsSnapshot BeforeRelease = GetGCStatisticsSnapshot(*ScriptEngine);
		Root->Release();

		if (!TestRunner->TestEqual(TEXT("GC cycle detection should accept a detect-only full cycle"), ScriptEngine->GarbageCollect(asGC_FULL_CYCLE | asGC_DETECT_GARBAGE, 1), 0))
		{
			return;
		}

		const FGCStatisticsSnapshot AfterDetect = GetGCStatisticsSnapshot(*ScriptEngine);
		TestRunner->TestTrue(TEXT("GC should detect at least one cyclic object after releasing a self-cycle"), AfterDetect.TotalDetected >= BeforeRelease.TotalDetected + 1);
		TestRunner->TestTrue(TEXT("Detect-only GC should keep the cyclic object tracked until destroy runs"), AfterDetect.CurrentSize >= 1);

		if (!TestRunner->TestTrue(TEXT("GC cycle detection test should complete subsequent collection passes"), RunFullGarbageCollection(*ScriptEngine)))
		{
			return;
		}

		const FGCStatisticsSnapshot AfterCollect = GetGCStatisticsSnapshot(*ScriptEngine);
		TestRunner->TestTrue(TEXT("GC should eventually destroy the detected cycle"), AfterCollect.TotalDestroyed >= AfterDetect.TotalDestroyed);
		TestRunner->TestEqual(TEXT("GC should leave no probe objects alive after collecting the detected self-cycle"), FGCProbeObject::LiveCount, 0);
		}
	}

	TEST_METHOD(TwoNodeCycleCollection)
	{
		using namespace AngelscriptTest_AngelScriptSDK_AngelscriptGCInternalTests_Private;
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine);
		asIScriptEngine* ScriptEngine = Engine.GetScriptEngine();
		asITypeInfo* GCProbeType = nullptr;
		if (!RegisterGCProbeType(*TestRunner, *ScriptEngine, GCProbeType))
		{
			return;
		}

		FGCProbeObject::LiveCount = 0;
		FGCProbeObject* Root = CreateTwoNodeCycle(*ScriptEngine, *GCProbeType);
		if (!TestRunner->TestNotNull(TEXT("GC two-node cycle collection test should create the root probe object"), Root))
		{
			return;
		}

		FGCProbeObject* Peer = Root->Peer;
		if (!TestRunner->TestNotNull(TEXT("GC two-node cycle collection test should create the peer probe object"), Peer))
		{
			return;
		}

		TestRunner->TestNotEqual(TEXT("GC two-node cycle collection test should build two distinct probe objects"), Root, Peer);
		TestRunner->TestEqual(TEXT("GC two-node cycle collection test should start with two live probe objects"), FGCProbeObject::LiveCount, 2);

		const FGCStatisticsSnapshot BeforeRelease = GetGCStatisticsSnapshot(*ScriptEngine);
		Root->Release();
		Peer->Release();

		if (!TestRunner->TestEqual(TEXT("GC two-node cycle detection should accept a detect-only full cycle"), ScriptEngine->GarbageCollect(asGC_FULL_CYCLE | asGC_DETECT_GARBAGE, 1), 0))
		{
			return;
		}

		const FGCStatisticsSnapshot AfterDetect = GetGCStatisticsSnapshot(*ScriptEngine);
		TestRunner->TestTrue(TEXT("GC should detect at least one released two-node cycle"), AfterDetect.TotalDetected >= BeforeRelease.TotalDetected + 1);
		TestRunner->TestTrue(TEXT("Detect-only GC should keep both released cyclic objects tracked until destroy runs"), AfterDetect.CurrentSize >= BeforeRelease.CurrentSize);

		if (!TestRunner->TestTrue(TEXT("GC two-node cycle collection test should complete subsequent collection passes"), RunFullGarbageCollection(*ScriptEngine)))
		{
			return;
		}

		const FGCStatisticsSnapshot AfterCollect = GetGCStatisticsSnapshot(*ScriptEngine);
		TestRunner->TestTrue(TEXT("GC should destroy released two-node cycle objects during full collection"), AfterCollect.TotalDestroyed > AfterDetect.TotalDestroyed);
		TestRunner->TestTrue(TEXT("Full collection should remove both released cyclic probe objects from GC tracking"), AfterDetect.CurrentSize >= AfterCollect.CurrentSize + 2);
		TestRunner->TestEqual(TEXT("GC should leave no probe objects alive after collecting the detected two-node cycle"), FGCProbeObject::LiveCount, 0);
		}
	}
};

#endif
