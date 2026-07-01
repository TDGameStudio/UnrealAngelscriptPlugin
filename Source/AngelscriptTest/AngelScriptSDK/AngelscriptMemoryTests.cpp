#include "CQTest.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_memory.h"
#include "EndAngelscriptHeaders.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FAngelscriptMemoryTests,
	"Angelscript.TestModule.AngelScriptSDK.Memory",
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
	TEST_METHOD(Construction)
	{
		asCMemoryMgr Manager;
		ASSERT_THAT(IsTrue(true, TEXT("Constructing the internal memory manager should succeed")));
	}

	TEST_METHOD(FreeUnused)
	{
		FMemoryManagerProbe Manager;
		Manager.FreeUnusedMemory();
		ASSERT_THAT(IsTrue(true, TEXT("FreeUnusedMemory should be callable even when no pooled memory is tracked")));
		ASSERT_THAT(AreEqual(0, Manager.GetScriptNodePoolSize(),
			TEXT("FreeUnusedMemory should leave the script-node pool empty")));
		ASSERT_THAT(AreEqual(0, Manager.GetByteInstructionPoolSize(),
			TEXT("FreeUnusedMemory should leave the byte-instruction pool empty")));
	}

	TEST_METHOD(ScriptNodeReuse)
	{
		FMemoryManagerProbe Manager;
		void* FirstAllocation = Manager.AllocScriptNode();
		ASSERT_THAT(IsNotNull(FirstAllocation, TEXT("AllocScriptNode should return storage for a script node")));
		Manager.FreeScriptNode(FirstAllocation);
		ASSERT_THAT(AreEqual(1, Manager.GetScriptNodePoolSize(),
			TEXT("FreeScriptNode should retain exactly one script-node allocation in the pool")));

		void* ReusedAllocation = Manager.AllocScriptNode();
		ASSERT_THAT(AreEqual(FirstAllocation, ReusedAllocation,
			TEXT("AllocScriptNode should reuse the most recently freed script-node allocation")));
		ASSERT_THAT(AreEqual(0, Manager.GetScriptNodePoolSize(),
			TEXT("Reusing a script-node allocation should remove it from the pool")));
		Manager.FreeScriptNode(ReusedAllocation);
	}

	TEST_METHOD(ByteInstructionReuse)
	{
		FMemoryManagerProbe Manager;
		void* FirstAllocation = Manager.AllocByteInstruction();
		ASSERT_THAT(IsNotNull(FirstAllocation, TEXT("AllocByteInstruction should return storage for a bytecode instruction")));
		Manager.FreeByteInstruction(FirstAllocation);
		ASSERT_THAT(AreEqual(1, Manager.GetByteInstructionPoolSize(),
			TEXT("FreeByteInstruction should retain exactly one byte-instruction allocation in the pool")));

		void* ReusedAllocation = Manager.AllocByteInstruction();
		ASSERT_THAT(AreEqual(FirstAllocation, ReusedAllocation,
			TEXT("AllocByteInstruction should reuse the most recently freed bytecode instruction allocation")));
		ASSERT_THAT(AreEqual(0, Manager.GetByteInstructionPoolSize(),
			TEXT("Reusing a bytecode instruction allocation should remove it from the pool")));
		Manager.FreeByteInstruction(ReusedAllocation);
	}

	TEST_METHOD(PoolLeakTracking)
	{
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
