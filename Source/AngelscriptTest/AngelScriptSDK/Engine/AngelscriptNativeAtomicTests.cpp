#include "CQTest.h"

#include "AngelscriptTestMacros.h"
#include "../Support/AngelscriptNativeCoreTestSupport.h"
#include "../Support/AngelscriptNativeCaseTestSupport.h"
#include "../Support/AngelscriptNativeLanguageCaseTestSupport.h"

// Raw SDK atomic implementation coverage.

#include "HAL/PlatformProcess.h"
#include "HAL/Runnable.h"
#include "HAL/RunnableThread.h"
#include "Misc/ScopeExit.h"

#include <atomic>

#include "StartAngelscriptHeaders.h"
#include "source/as_atomic.h"
#include "EndAngelscriptHeaders.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FAtomicTests,
	"Angelscript.TestModule.AngelScriptSDK.Engine.Atomic",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	static constexpr int32 WorkerCount = 4;
	static constexpr int32 OperationsPerWorker = 4096;

	class FAtomicWorker final : public FRunnable
	{
	public:
		FAtomicWorker(
			asCAtomic& InAtomic,
			const int32 InWorkerIndex,
			std::atomic<int32>* const InWorkerCompletions)
			: Atomic(InAtomic)
			, WorkerIndex(InWorkerIndex)
			, WorkerCompletions(InWorkerCompletions)
		{
		}

		uint32 Run() override
		{
			for (int32 Index = 0; Index < OperationsPerWorker; ++Index)
			{
				Atomic.atomicInc();
			}

			for (int32 Index = 0; Index < OperationsPerWorker; ++Index)
			{
				Atomic.atomicDec();
			}

			WorkerCompletions[WorkerIndex].fetch_add(
				1,
				std::memory_order_release);
			return 0;
		}

	private:
		asCAtomic& Atomic;
		int32 WorkerIndex;
		std::atomic<int32>* WorkerCompletions;
	};

	enum class EAtomicOperation : uint8
	{
		SetAndGet,
		Increment,
		Decrement,
		BalancedPair,
	};

	class FParameterizedAtomicWorker final : public FRunnable
	{
	public:
		FParameterizedAtomicWorker(
			asCAtomic& InAtomic,
			const EAtomicOperation InOperation,
			const int32 InInitialValue,
			const int32 InIterations)
			: Atomic(InAtomic)
			, Operation(InOperation)
			, InitialValue(InInitialValue)
			, Iterations(InIterations)
		{
		}

		uint32 Run() override
		{
			switch (Operation)
			{
			case EAtomicOperation::SetAndGet:
				for (int32 Index = 0; Index < Iterations; ++Index)
				{
					Atomic.set(InitialValue);
					Atomic.get();
				}
				break;
			case EAtomicOperation::Increment:
				for (int32 Index = 0; Index < Iterations; ++Index)
				{
					Atomic.atomicInc();
				}
				break;
			case EAtomicOperation::Decrement:
				for (int32 Index = 0; Index < Iterations; ++Index)
				{
					Atomic.atomicDec();
				}
				break;
			case EAtomicOperation::BalancedPair:
				for (int32 Index = 0; Index < Iterations; ++Index)
				{
					Atomic.atomicInc();
					Atomic.atomicDec();
				}
				break;
			}

			return 0;
		}

	private:
		asCAtomic& Atomic;
		EAtomicOperation Operation;
		int32 InitialValue;
		int32 Iterations;
	};

	class FLockableBoolWorker final : public FRunnable
	{
	public:
		FLockableBoolWorker(
			asILockableSharedBool& InFlag,
			const bool bInTargetValue,
			const int32 InIterations,
			std::atomic<int32>& InActiveWorkers,
			std::atomic<int32>& InMaximumConcurrentWorkers,
			std::atomic<int32>& InOverlapViolations,
			std::atomic<int32>& InCompletedSections)
			: Flag(InFlag)
			, bTargetValue(bInTargetValue)
			, Iterations(InIterations)
			, ActiveWorkers(InActiveWorkers)
			, MaximumConcurrentWorkers(InMaximumConcurrentWorkers)
			, OverlapViolations(InOverlapViolations)
			, CompletedSections(InCompletedSections)
		{
		}

		uint32 Run() override
		{
			for (int32 Iteration = 0; Iteration < Iterations; ++Iteration)
			{
				Flag.Lock();
				const int32 ConcurrentWorkers =
					ActiveWorkers.fetch_add(1, std::memory_order_acq_rel) + 1;
				int32 PreviousMaximum =
					MaximumConcurrentWorkers.load(std::memory_order_relaxed);
				while (ConcurrentWorkers > PreviousMaximum
					&& !MaximumConcurrentWorkers.compare_exchange_weak(
						PreviousMaximum,
						ConcurrentWorkers,
						std::memory_order_release,
						std::memory_order_relaxed))
				{
				}
				if (ConcurrentWorkers > 1)
				{
					OverlapViolations.fetch_add(1, std::memory_order_relaxed);
				}

				FPlatformProcess::SleepNoStats(0.0001f);
				Flag.Set(bTargetValue);
				ActiveWorkers.fetch_sub(1, std::memory_order_acq_rel);
				Flag.Unlock();
				CompletedSections.fetch_add(1, std::memory_order_release);
			}

			return 0;
		}

	private:
		asILockableSharedBool& Flag;
		bool bTargetValue;
		int32 Iterations;
		std::atomic<int32>& ActiveWorkers;
		std::atomic<int32>& MaximumConcurrentWorkers;
		std::atomic<int32>& OverlapViolations;
		std::atomic<int32>& CompletedSections;
	};

	static int32 ApplySingleThreadOperation(
		asCAtomic& Atomic,
		const EAtomicOperation Operation,
		const int32 InitialValue,
		const int32 Iterations)
	{
		Atomic.set(InitialValue);
		switch (Operation)
		{
		case EAtomicOperation::SetAndGet:
			return static_cast<int32>(Atomic.get());
		case EAtomicOperation::Increment:
			for (int32 Index = 0; Index < Iterations; ++Index)
			{
				Atomic.atomicInc();
			}
			return static_cast<int32>(Atomic.get());
		case EAtomicOperation::Decrement:
			for (int32 Index = 0; Index < Iterations; ++Index)
			{
				Atomic.atomicDec();
			}
			return static_cast<int32>(Atomic.get());
		case EAtomicOperation::BalancedPair:
			for (int32 Index = 0; Index < Iterations; ++Index)
			{
				Atomic.atomicInc();
				Atomic.atomicDec();
			}
			return static_cast<int32>(Atomic.get());
		}

		return static_cast<int32>(Atomic.get());
	}

	static int32 RunConcurrentOperation(
		FAutomationTestBase& Test,
		asCAtomic& Atomic,
		const EAtomicOperation Operation,
		const int32 InitialValue,
		const int32 InWorkerCount,
		const int32 Iterations)
	{
		FNoDiscardAsserter Assert(Test);
		Atomic.set(InitialValue);

		TArray<TUniquePtr<FParameterizedAtomicWorker>> Workers;
		TArray<FRunnableThread*> Threads;
		Workers.Reserve(InWorkerCount);
		Threads.Reserve(InWorkerCount);
		for (int32 WorkerIndex = 0; WorkerIndex < InWorkerCount; ++WorkerIndex)
		{
			Workers.Add(MakeUnique<FParameterizedAtomicWorker>(
				Atomic,
				Operation,
				InitialValue,
				Iterations));
			FRunnableThread* const Thread = FRunnableThread::Create(
				Workers.Last().Get(),
				*FString::Printf(TEXT("AngelscriptAtomicProduct_%d"), WorkerIndex));
			if (!Assert.IsNotNull(Thread, TEXT("Parameterized atomic worker should be created")))
			{
				continue;
			}
			Threads.Add(Thread);
		}

		for (FRunnableThread* const Thread : Threads)
		{
			Thread->WaitForCompletion();
			delete Thread;
		}

		return static_cast<int32>(Atomic.get());
	}

public:
	TEST_METHOD(DefaultConstructionStartsAtZero)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT("ENG-ATOMIC-DEFAULT-CONSTRUCTION",
			ENativeEvidence::Runtime
				| ENativeEvidence::Isolation);

		asCAtomic MutatedControl;
		MutatedControl.set(73);
		asCAtomic FreshAtomic;
		ASSERT_THAT(AreEqual(0, static_cast<int32>(FreshAtomic.get()),
			TEXT("Default-constructed atomic value should start at zero")));
		FreshAtomic.set(19);
		ASSERT_THAT(AreEqual(
			73,
			static_cast<int32>(MutatedControl.get()),
			TEXT("Mutating a fresh atomic should not contaminate an independently owned atomic")));
	}

	TEST_METHOD(SetAndGetPreserveValue)
	{
		AS_NATIVE_NON_PRODUCT("LegacyCompatibility",
			"ENG-ATOMIC-OPERATIONS supersedes this two-value set/get predecessor with four initial values across single and concurrent worker modes");

		asCAtomic Atomic;
		Atomic.set(42);
		ASSERT_THAT(AreEqual(42, static_cast<int32>(Atomic.get()),
			TEXT("Atomic get should return the last value set")));

		Atomic.set(0);
		ASSERT_THAT(AreEqual(0, static_cast<int32>(Atomic.get()),
			TEXT("Atomic value should support resetting to zero")));
	}

	TEST_METHOD(IncrementAndDecrementReturnExpectedValues)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT("ENG-ATOMIC-RETURN-TRANSITIONS",
			ENativeEvidence::Runtime
				| ENativeEvidence::Isolation);

		asCAtomic Atomic;
		asCAtomic Control;
		Control.set(91);
		ASSERT_THAT(AreEqual(1, static_cast<int32>(Atomic.atomicInc()),
			TEXT("Increment should return the incremented value")));
		ASSERT_THAT(AreEqual(91, static_cast<int32>(Control.get()),
			TEXT("Primary increment should not mutate the independent control atomic")));
		ASSERT_THAT(AreEqual(2, static_cast<int32>(Atomic.atomicInc()),
			TEXT("Second increment should observe the previous result")));
		ASSERT_THAT(AreEqual(91, static_cast<int32>(Control.get()),
			TEXT("Consecutive primary transitions should preserve the control atomic")));
		ASSERT_THAT(AreEqual(1, static_cast<int32>(Atomic.atomicDec()),
			TEXT("Decrement should return the decremented value")));
		ASSERT_THAT(AreEqual(1, static_cast<int32>(Atomic.get()),
			TEXT("Read should observe the result of increment/decrement operations")));
		ASSERT_THAT(AreEqual(91, static_cast<int32>(Control.get()),
			TEXT("Completed primary transitions should leave the control atomic unchanged")));
	}

	TEST_METHOD(ConcurrentIncrementAndDecrementRemainBalanced)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT("ENG-ATOMIC-BATCHED-CONCURRENCY",
			ENativeEvidence::Runtime
				| ENativeEvidence::Lifecycle
				| ENativeEvidence::Cleanup
				| ENativeEvidence::Isolation);

		asCAtomic Atomic;
		TArray<TUniquePtr<FAtomicWorker>> Workers;
		TArray<FRunnableThread*> Threads;
		std::atomic<int32> WorkerCompletions[WorkerCount]{};
		Workers.Reserve(WorkerCount);
		Threads.Reserve(WorkerCount);

		for (int32 WorkerIndex = 0; WorkerIndex < WorkerCount; ++WorkerIndex)
		{
			Workers.Add(MakeUnique<FAtomicWorker>(
				Atomic,
				WorkerIndex,
				WorkerCompletions));
			FRunnableThread* const Thread = FRunnableThread::Create(
				Workers.Last().Get(),
				*FString::Printf(TEXT("AngelscriptAtomic_%d"), WorkerIndex));
			ASSERT_THAT(IsNotNull(Thread, TEXT("Atomic worker thread should be created")));
			if (Thread == nullptr)
			{
				continue;
			}

			Threads.Add(Thread);
		}

		for (FRunnableThread* const Thread : Threads)
		{
			Thread->WaitForCompletion();
			delete Thread;
		}

		ASSERT_THAT(AreEqual(0, static_cast<int32>(Atomic.get()),
			TEXT("Balanced concurrent increments and decrements should restore zero")));
		for (int32 WorkerIndex = 0; WorkerIndex < WorkerCount; ++WorkerIndex)
		{
			ASSERT_THAT(AreEqual(
				1,
				WorkerCompletions[WorkerIndex].load(std::memory_order_acquire),
				*FString::Printf(
					TEXT("Atomic worker %d should complete exactly its own batch once"),
					WorkerIndex)));
		}
	}

	TEST_METHOD(OperationsByInitialValueAndWorkerMode)
	{
		using namespace AngelscriptNativeTestSupport;
		AS_NATIVE_PRODUCT("ENG-ATOMIC-OPERATIONS",
			ENativeEvidence::Runtime
			| ENativeEvidence::Isolation);

		struct FOperationCase
		{
			const TCHAR* Id;
			EAtomicOperation Operation;
		};
		struct FInitialCase
		{
			const TCHAR* Id;
			int32 Value;
		};
		struct FWorkerModeCase
		{
			const TCHAR* Id;
			int32 WorkerCount;
		};

		const FOperationCase Operations[] =
		{
			{ TEXT("set_get"), EAtomicOperation::SetAndGet },
			{ TEXT("increment"), EAtomicOperation::Increment },
			{ TEXT("decrement"), EAtomicOperation::Decrement },
			{ TEXT("balanced_pair"), EAtomicOperation::BalancedPair },
		};
		const FInitialCase InitialValues[] =
		{
			{ TEXT("zero"), 0 },
			{ TEXT("negative_one"), -1 },
			{ TEXT("one"), 1 },
			{ TEXT("positive"), 42 },
		};
		const FWorkerModeCase WorkerModes[] =
		{
			{ TEXT("single"), 1 },
			{ TEXT("concurrent"), WorkerCount },
		};

		constexpr int32 Iterations = 64;
		int32 ObservedCaseCount = 0;
		for (const FOperationCase& Operation : Operations)
		{
			for (const FInitialCase& Initial : InitialValues)
			{
				for (const FWorkerModeCase& WorkerMode : WorkerModes)
				{
					const FString CaseId = MakeNativeCaseId(
						"ENG-ATOMIC-OPERATIONS",
						{ Operation.Id, Initial.Id, WorkerMode.Id });
					const FNativeCaseContext Case(CaseId);
					TestRunner->AddInfo(Case.Describe(TEXT("running direct asCAtomic operation")));

					asCAtomic Atomic;
					asCAtomic Control;
					Control.set(-31337);
					const int32 ActualValue = WorkerMode.WorkerCount == 1
						? ApplySingleThreadOperation(Atomic, Operation.Operation, Initial.Value, Iterations)
						: RunConcurrentOperation(
							*TestRunner,
							Atomic,
							Operation.Operation,
							Initial.Value,
							WorkerMode.WorkerCount,
							Iterations);

					int32 ExpectedValue = Initial.Value;
					if (Operation.Operation == EAtomicOperation::Increment)
					{
						ExpectedValue += Iterations * WorkerMode.WorkerCount;
					}
					else if (Operation.Operation == EAtomicOperation::Decrement)
					{
						ExpectedValue -= Iterations * WorkerMode.WorkerCount;
					}

					ASSERT_THAT(AreEqual(
						ExpectedValue,
						ActualValue,
						*Case.Describe(TEXT("asCAtomic should preserve the expected final value"))));
					ASSERT_THAT(AreEqual(
						-31337,
						static_cast<int32>(Control.get()),
						*Case.Describe(TEXT("atomic cell should leave an independent control value unchanged"))));
					++ObservedCaseCount;
				}
			}
		}

		ASSERT_THAT(AreEqual(
			32,
			ObservedCaseCount,
			TEXT("Operation × initial value × worker mode should execute every atomic cell")));
	}

	TEST_METHOD(LockAndUnlockSerializeWorkerMutation)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT("ENG-LOCKABLE-SHARED-BOOL-CONTENTION",
			ENativeEvidence::Runtime
				| ENativeEvidence::Lifecycle
				| ENativeEvidence::Cleanup
				| ENativeEvidence::Isolation);

		struct FBoolCase
		{
			const TCHAR* Id;
			bool bValue;
		};
		struct FWorkerCase
		{
			const TCHAR* Id;
			int32 Count;
		};

		const FBoolCase InitialValues[] =
		{
			{ TEXT("initial_false"), false },
			{ TEXT("initial_true"), true },
		};
		const FBoolCase TargetValues[] =
		{
			{ TEXT("target_false"), false },
			{ TEXT("target_true"), true },
		};
		const FWorkerCase WorkerModes[] =
		{
			{ TEXT("single"), 1 },
			{ TEXT("contended"), WorkerCount },
		};
		constexpr int32 Iterations = 32;

		int32 ObservedCaseCount = 0;
		for (const FBoolCase& Initial : InitialValues)
		{
			for (const FBoolCase& Target : TargetValues)
			{
				for (const FWorkerCase& WorkerMode : WorkerModes)
				{
					const FString CaseId = MakeNativeCaseId(
						"ENG-LOCKABLE-SHARED-BOOL-CONTENTION",
						{ Initial.Id, Target.Id, WorkerMode.Id });
					FString ReviewSource;
					AppendGeneratedAsLine(
						ReviewSource,
						FString::Printf(
							TEXT("// initial=%s target=%s workers=%d iterations=%d"),
							Initial.bValue ? TEXT("true") : TEXT("false"),
							Target.bValue ? TEXT("true") : TEXT("false"),
							WorkerMode.Count,
							Iterations));
					PrintGeneratedAsSource(
						*TestRunner,
						*CaseId,
						TEXT("LockableSharedBoolContention"),
						ReviewSource);

					asILockableSharedBool* const Flag =
						asCreateLockableSharedBool();
					ASSERT_THAT(IsNotNull(
						Flag,
						TEXT("Lockable shared-bool product should create a raw shared flag")));
					if (Flag == nullptr)
					{
						continue;
					}
					ON_SCOPE_EXIT { Flag->Release(); };
					Flag->Set(Initial.bValue);
					ASSERT_THAT(AreEqual(
						Initial.bValue,
						Flag->Get(),
						TEXT("Lockable shared-bool product should preserve its initial value")));

					std::atomic<int32> ActiveWorkers{ 0 };
					std::atomic<int32> MaximumConcurrentWorkers{ 0 };
					std::atomic<int32> OverlapViolations{ 0 };
					std::atomic<int32> CompletedSections{ 0 };
					TArray<TUniquePtr<FLockableBoolWorker>> Workers;
					TArray<FRunnableThread*> Threads;
					Workers.Reserve(WorkerMode.Count);
					Threads.Reserve(WorkerMode.Count);
					for (int32 WorkerIndex = 0;
						WorkerIndex < WorkerMode.Count;
						++WorkerIndex)
					{
						Workers.Add(MakeUnique<FLockableBoolWorker>(
							*Flag,
							Target.bValue,
							Iterations,
							ActiveWorkers,
							MaximumConcurrentWorkers,
							OverlapViolations,
							CompletedSections));
						FRunnableThread* const Thread = FRunnableThread::Create(
							Workers.Last().Get(),
							*FString::Printf(
								TEXT("AngelscriptLockableBool_%d"),
								WorkerIndex));
						ASSERT_THAT(IsNotNull(
							Thread,
							TEXT("Lockable shared-bool worker should be created")));
						if (Thread != nullptr)
						{
							Threads.Add(Thread);
						}
					}

					for (FRunnableThread* const Thread : Threads)
					{
						Thread->WaitForCompletion();
						delete Thread;
					}

					ASSERT_THAT(AreEqual(
						0,
						ActiveWorkers.load(std::memory_order_acquire),
						TEXT("Lockable shared-bool product should leave no active critical section")));
					ASSERT_THAT(AreEqual(
						1,
						MaximumConcurrentWorkers.load(std::memory_order_acquire),
						TEXT("Lock and Unlock should serialize every worker critical section")));
					ASSERT_THAT(AreEqual(
						0,
						OverlapViolations.load(std::memory_order_acquire),
						TEXT("Lock and Unlock should prevent overlapping worker mutation")));
					ASSERT_THAT(AreEqual(
						WorkerMode.Count * Iterations,
						CompletedSections.load(std::memory_order_acquire),
						TEXT("Every lock-protected worker mutation should complete")));
					ASSERT_THAT(AreEqual(
						Target.bValue,
						Flag->Get(),
						TEXT("Lockable shared-bool product should preserve the final protected value")));
					++ObservedCaseCount;
				}
			}
		}

		ASSERT_THAT(AreEqual(
			8,
			ObservedCaseCount,
			TEXT("Initial value, target value, and worker mode should execute every shared-bool cell")));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
