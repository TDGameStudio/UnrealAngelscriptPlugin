#include "AngelscriptTestMacros.h"
#include "CQTest.h"
#include "../Support/AngelscriptNativeCoreTestSupport.h"

// Raw SDK memory behavior coverage.

#include "StartAngelscriptHeaders.h"
#include "source/as_memory.h"
#include "EndAngelscriptHeaders.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FMemoryTests,
	"Angelscript.TestModule.AngelScriptSDK.Engine.Memory",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	class FMemoryManagerProbe final : public asCMemoryMgr
	{
	public:
		int32 GetScriptNodePoolSize() const
		{
			return scriptNodePool.Num();
		}

		int32 GetByteInstructionPoolSize() const
		{
			return byteInstructionPool.Num();
		}
	};

public:
	TEST_METHOD(MemoryConstruction)
	{
		AS_NATIVE_NON_PRODUCT("AggregateSupport",
			"Every owned memory-pool method constructs an independent asCMemoryMgr; this constructor-only smoke carries no additional observable contract");

		asCMemoryMgr Manager;
		ASSERT_THAT(IsTrue(true, TEXT("Constructing the internal memory manager should succeed")));
	}

	TEST_METHOD(MemoryFreeUnused)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT("ENG-MEMORY-EMPTY-FREE-UNUSED",
			ENativeEvidence::Runtime
				| ENativeEvidence::Lifecycle
				| ENativeEvidence::Cleanup
				| ENativeEvidence::Isolation);

		FMemoryManagerProbe Manager;
		Manager.FreeUnusedMemory();
		Manager.FreeUnusedMemory();
		ASSERT_THAT(AreEqual(0, Manager.GetScriptNodePoolSize(),
			TEXT("Repeated FreeUnusedMemory should leave the script-node pool empty")));
		ASSERT_THAT(AreEqual(0, Manager.GetByteInstructionPoolSize(),
			TEXT("Repeated FreeUnusedMemory should leave the byte-instruction pool empty")));
	}

	TEST_METHOD(MemoryScriptNodeReuse)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT("ENG-MEMORY-SCRIPT-NODE-LIFO-REUSE",
			ENativeEvidence::Runtime
				| ENativeEvidence::Lifecycle
				| ENativeEvidence::Cleanup
				| ENativeEvidence::Isolation);

		FMemoryManagerProbe Manager;
		void* FirstAllocation = Manager.AllocScriptNode();
		void* SecondAllocation = Manager.AllocScriptNode();
		ASSERT_THAT(IsNotNull(
			FirstAllocation,
			TEXT("AllocScriptNode should return storage for the first script node")));
		ASSERT_THAT(IsNotNull(
			SecondAllocation,
			TEXT("AllocScriptNode should return storage for the second script node")));
		ASSERT_THAT(AreNotEqual(
			FirstAllocation,
			SecondAllocation,
			TEXT("Two live script-node allocations should own distinct storage")));

		Manager.FreeScriptNode(FirstAllocation);
		Manager.FreeScriptNode(SecondAllocation);
		ASSERT_THAT(AreEqual(2, Manager.GetScriptNodePoolSize(),
			TEXT("FreeScriptNode should retain both script-node allocations in the pool")));

		void* FirstReuse = Manager.AllocScriptNode();
		void* SecondReuse = Manager.AllocScriptNode();
		ASSERT_THAT(AreEqual(SecondAllocation, FirstReuse,
			TEXT("AllocScriptNode should first reuse the most recently freed script-node allocation")));
		ASSERT_THAT(AreEqual(FirstAllocation, SecondReuse,
			TEXT("AllocScriptNode should then reuse the earlier script-node allocation")));
		ASSERT_THAT(AreEqual(0, Manager.GetScriptNodePoolSize(),
			TEXT("Reusing both script-node allocations should empty the pool")));
		Manager.FreeScriptNode(FirstReuse);
		Manager.FreeScriptNode(SecondReuse);
	}

	TEST_METHOD(ByteInstructionReuse)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT("ENG-MEMORY-BYTE-INSTRUCTION-LIFO-REUSE",
			ENativeEvidence::Runtime
				| ENativeEvidence::Lifecycle
				| ENativeEvidence::Cleanup
				| ENativeEvidence::Isolation);

		FMemoryManagerProbe Manager;
		void* FirstAllocation = Manager.AllocByteInstruction();
		void* SecondAllocation = Manager.AllocByteInstruction();
		ASSERT_THAT(IsNotNull(
			FirstAllocation,
			TEXT("AllocByteInstruction should return storage for the first bytecode instruction")));
		ASSERT_THAT(IsNotNull(
			SecondAllocation,
			TEXT("AllocByteInstruction should return storage for the second bytecode instruction")));
		ASSERT_THAT(AreNotEqual(
			FirstAllocation,
			SecondAllocation,
			TEXT("Two live bytecode-instruction allocations should own distinct storage")));

		Manager.FreeByteInstruction(FirstAllocation);
		Manager.FreeByteInstruction(SecondAllocation);
		ASSERT_THAT(AreEqual(2, Manager.GetByteInstructionPoolSize(),
			TEXT("FreeByteInstruction should retain both byte-instruction allocations in the pool")));

		void* FirstReuse = Manager.AllocByteInstruction();
		void* SecondReuse = Manager.AllocByteInstruction();
		ASSERT_THAT(AreEqual(SecondAllocation, FirstReuse,
			TEXT("AllocByteInstruction should first reuse the most recently freed bytecode instruction allocation")));
		ASSERT_THAT(AreEqual(FirstAllocation, SecondReuse,
			TEXT("AllocByteInstruction should then reuse the earlier bytecode instruction allocation")));
		ASSERT_THAT(AreEqual(0, Manager.GetByteInstructionPoolSize(),
			TEXT("Reusing both bytecode-instruction allocations should empty the pool")));
		Manager.FreeByteInstruction(FirstReuse);
		Manager.FreeByteInstruction(SecondReuse);
	}

	TEST_METHOD(PoolLeakTracking)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT("ENG-MEMORY-POOL-BULK-RELEASE",
			ENativeEvidence::Runtime
				| ENativeEvidence::Lifecycle
				| ENativeEvidence::Cleanup
				| ENativeEvidence::Isolation);

		FMemoryManagerProbe Manager;
		void* ScriptNodeA = Manager.AllocScriptNode();
		void* ScriptNodeB = Manager.AllocScriptNode();
		void* Instruction = Manager.AllocByteInstruction();

		Manager.FreeScriptNode(ScriptNodeA);
		Manager.FreeScriptNode(ScriptNodeB);
		Manager.FreeByteInstruction(Instruction);

		ASSERT_THAT(AreEqual(2, Manager.GetScriptNodePoolSize(),
			TEXT("The script-node pool should track every freed script-node allocation")));
		ASSERT_THAT(AreEqual(1, Manager.GetByteInstructionPoolSize(),
			TEXT("The byte-instruction pool should track every freed bytecode allocation")));

		Manager.FreeUnusedMemory();
		ASSERT_THAT(AreEqual(0, Manager.GetScriptNodePoolSize(),
			TEXT("FreeUnusedMemory should release all tracked script-node allocations")));
		ASSERT_THAT(AreEqual(0, Manager.GetByteInstructionPoolSize(),
			TEXT("FreeUnusedMemory should release all tracked bytecode allocations")));
	}
};

#endif
