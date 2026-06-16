#include "CQTest.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_memory.h"
#include "EndAngelscriptHeaders.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
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
}


TEST_CLASS_WITH_FLAGS(FAngelscriptMemoryTests,
	"Angelscript.TestModule.AngelScriptSDK.Memory",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(Construction)
	{
		asCMemoryMgr Manager;
		TestRunner->TestTrue(TEXT("Constructing the internal memory manager should succeed"), true);
	}

	TEST_METHOD(FreeUnused)
	{
		FMemoryManagerProbe Manager;
		Manager.FreeUnusedMemory();
		TestRunner->TestTrue(TEXT("FreeUnusedMemory should be callable even when no pooled memory is tracked"), true);
		TestRunner->TestEqual(TEXT("FreeUnusedMemory should leave the script-node pool empty"), Manager.GetScriptNodePoolSize(), 0);
		TestRunner->TestEqual(TEXT("FreeUnusedMemory should leave the byte-instruction pool empty"), Manager.GetByteInstructionPoolSize(), 0);
	}

	TEST_METHOD(ScriptNodeReuse)
	{
		FMemoryManagerProbe Manager;
		void* FirstAllocation = Manager.AllocScriptNode();
		TestRunner->TestNotNull(TEXT("AllocScriptNode should return storage for a script node"), FirstAllocation);
		Manager.FreeScriptNode(FirstAllocation);
		TestRunner->TestEqual(TEXT("FreeScriptNode should retain exactly one script-node allocation in the pool"), Manager.GetScriptNodePoolSize(), 1);

		void* ReusedAllocation = Manager.AllocScriptNode();
		TestRunner->TestEqual(TEXT("AllocScriptNode should reuse the most recently freed script-node allocation"), ReusedAllocation, FirstAllocation);
		TestRunner->TestEqual(TEXT("Reusing a script-node allocation should remove it from the pool"), Manager.GetScriptNodePoolSize(), 0);
		Manager.FreeScriptNode(ReusedAllocation);
	}

	TEST_METHOD(ByteInstructionReuse)
	{
		FMemoryManagerProbe Manager;
		void* FirstAllocation = Manager.AllocByteInstruction();
		TestRunner->TestNotNull(TEXT("AllocByteInstruction should return storage for a bytecode instruction"), FirstAllocation);
		Manager.FreeByteInstruction(FirstAllocation);
		TestRunner->TestEqual(TEXT("FreeByteInstruction should retain exactly one byte-instruction allocation in the pool"), Manager.GetByteInstructionPoolSize(), 1);

		void* ReusedAllocation = Manager.AllocByteInstruction();
		TestRunner->TestEqual(TEXT("AllocByteInstruction should reuse the most recently freed bytecode instruction allocation"), ReusedAllocation, FirstAllocation);
		TestRunner->TestEqual(TEXT("Reusing a bytecode instruction allocation should remove it from the pool"), Manager.GetByteInstructionPoolSize(), 0);
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

		TestRunner->TestEqual(TEXT("The script-node pool should track every freed script-node allocation"), Manager.GetScriptNodePoolSize(), 2);
		TestRunner->TestEqual(TEXT("The byte-instruction pool should track every freed bytecode allocation"), Manager.GetByteInstructionPoolSize(), 1);

		Manager.FreeUnusedMemory();
		TestRunner->TestEqual(TEXT("FreeUnusedMemory should release all tracked script-node allocations"), Manager.GetScriptNodePoolSize(), 0);
		TestRunner->TestEqual(TEXT("FreeUnusedMemory should release all tracked bytecode allocations"), Manager.GetByteInstructionPoolSize(), 0);
	}
};

#endif
