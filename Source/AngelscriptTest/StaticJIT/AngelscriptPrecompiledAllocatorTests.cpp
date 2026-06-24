#include "CQTest.h"
#include "StaticJIT/PrecompiledDataAllocator.h"

#if WITH_DEV_AUTOMATION_TESTS

TEST_CLASS_WITH_FLAGS(FAngelscriptPrecompiledAllocatorTests,
	"Angelscript.TestModule.StaticJIT.PrecompiledAllocator",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	static bool RunPrecompiledAllocatorResizeAndMove(FAutomationTestBase& Test)
	{
		FMemMark Mark(GScriptPreallocatedMemStack);
		FNoDiscardAsserter LocalAssert(Test);

		using FIntAllocator = TPrecompiledAllocator<>::ForElementType<int32>;

		FIntAllocator SourceAllocator;
		SourceAllocator.ResizeAllocation(0, 2, sizeof(int32));

		int32* InitialAllocation = SourceAllocator.GetAllocation();
		if (!LocalAssert.IsNotNull(InitialAllocation, TEXT("Precompiled allocator should allocate an initial buffer")))
		{
			return false;
		}

		InitialAllocation[0] = 17;
		InitialAllocation[1] = 42;

		SourceAllocator.ResizeAllocation(2, 4, sizeof(int32));
		int32* GrownAllocation = SourceAllocator.GetAllocation();
		if (!LocalAssert.IsNotNull(GrownAllocation, TEXT("Precompiled allocator should keep a valid allocation after growing")))
		{
			return false;
		}

		if (!LocalAssert.AreEqual(17, GrownAllocation[0], TEXT("Precompiled allocator should preserve the first element when growing")))
		{
			return false;
		}
		if (!LocalAssert.AreEqual(42, GrownAllocation[1], TEXT("Precompiled allocator should preserve the second element when growing")))
		{
			return false;
		}
		if (!LocalAssert.AreEqual(static_cast<UPTRINT>(0), reinterpret_cast<UPTRINT>(GrownAllocation) % alignof(int32), TEXT("Precompiled allocator should keep the allocation aligned for int32")))
		{
			return false;
		}

		FIntAllocator TargetAllocator;
		TargetAllocator.MoveToEmpty(SourceAllocator);

		if (!LocalAssert.IsNull(SourceAllocator.GetAllocation(), TEXT("Precompiled allocator should clear the source allocation after MoveToEmpty")))
		{
			return false;
		}
		if (!LocalAssert.IsTrue(TargetAllocator.GetAllocation() == GrownAllocation, TEXT("Precompiled allocator should preserve the destination allocation pointer after MoveToEmpty")))
		{
			return false;
		}
		if (!LocalAssert.AreEqual(17, TargetAllocator.GetAllocation()[0], TEXT("Precompiled allocator should preserve the first moved element")))
		{
			return false;
		}

		return LocalAssert.AreEqual(42, TargetAllocator.GetAllocation()[1], TEXT("Precompiled allocator should preserve the second moved element"));
	}

public:
	TEST_METHOD(PrecompiledAllocatorResizeAndMove)
	{
		ASSERT_THAT(IsTrue(RunPrecompiledAllocatorResizeAndMove(*TestRunner)));
	}
};

#endif
