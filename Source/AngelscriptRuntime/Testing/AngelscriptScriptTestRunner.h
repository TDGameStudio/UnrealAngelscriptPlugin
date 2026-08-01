#pragma once

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Templates/SharedPointer.h"
#include "UObject/StrongObjectPtr.h"

#include "Testing/AngelscriptScriptTestRegistry.h"
#include "Testing/AngelscriptTestSuite.h"
#include "Testing/LatentAutomationCommand.h"

class AActor;
struct FAngelscriptEngine;
class UActorComponent;
class UGameInstance;
class UWorld;
class ALatentAutomationCommandClientExecutor;
class FAngelscriptScriptTestExecutionContext;

/**
 * Marks a synchronous call into script-owned test code. Hot reload observes
 * this scope and leaves queued file changes untouched until the callback has
 * returned, so no class generation can invalidate a live script stack.
 */
class ANGELSCRIPTRUNTIME_API FAngelscriptScriptTestCallbackScope
{
public:
	explicit FAngelscriptScriptTestCallbackScope(
		bool bInSuppressExceptionLogging = false);
	FAngelscriptScriptTestCallbackScope(
		FAngelscriptScriptTestExecutionContext* InExecutionContext,
		bool bInSuppressExceptionLogging = false);
	~FAngelscriptScriptTestCallbackScope();

	FAngelscriptScriptTestCallbackScope(
		const FAngelscriptScriptTestCallbackScope&) = delete;
	FAngelscriptScriptTestCallbackScope& operator=(
		const FAngelscriptScriptTestCallbackScope&) = delete;

private:
	bool bSuppressExceptionLogging = false;
};

enum class EAngelscriptScriptTestPhase : uint8
{
	None,
	BeforeAll,
	BeforeEach,
	TestMethod,
	Command,
	AfterEach,
	Teardown,
	AfterAll,
	Cleanup,
	Complete,
};

enum class EAngelscriptScriptTestCommandType : uint8
{
	Action,
	Condition,
	Delay,
	Advanced,
};

enum class EAngelscriptScriptTestClientCommandPhase : uint8
{
	CreateExecutor,
	Setup,
	SetupClient,
	Execute,
	Finish,
	FinishClient,
	Done,
};

struct FAngelscriptScriptTestCommand
{
	EAngelscriptScriptTestCommandType Type =
		EAngelscriptScriptTestCommandType::Action;
	FName Callback;
	FString Description;
	double TimeoutSeconds = 0.0;
	double StartSeconds = 0.0;
	bool bStarted = false;
	bool bAdvancedAfterRan = false;
	bool bServerSuccess = false;
	bool bClientSuccess = false;
	EAngelscriptScriptTestClientCommandPhase ClientPhase =
		EAngelscriptScriptTestClientCommandPhase::CreateExecutor;
	TStrongObjectPtr<ULatentAutomationCommand> AdvancedCommand;
	TWeakObjectPtr<ALatentAutomationCommandClientExecutor> ClientExecutor;
};

/**
 * Per-leaf state. It owns every object that is unsafe to migrate across an
 * AngelScript generation and is the only state reached by suite helpers.
 */
class ANGELSCRIPTRUNTIME_API FAngelscriptScriptTestExecutionContext
	: public TSharedFromThis<FAngelscriptScriptTestExecutionContext>
{
public:
	FAngelscriptScriptTestExecutionContext(
		const FAngelscriptScriptTestDescriptor& InDescriptor,
		FAutomationTestBase* InAutomationTest);
	~FAngelscriptScriptTestExecutionContext();

	const FAngelscriptScriptTestDescriptor& GetDescriptor() const
	{
		return Descriptor;
	}

	UAngelscriptTestSuite* GetSuite() const
	{
		return Suite.Get();
	}

	UWorld* GetWorld() const
	{
		return TestWorld.Get();
	}

	EAngelscriptScriptTestPhase GetPhase() const
	{
		return Phase;
	}

	bool HasFailed() const;
	bool IsComplete() const
	{
		return Phase == EAngelscriptScriptTestPhase::Complete;
	}

	bool Start(bool bEnqueueAutomation = true);
	bool Update();
	void Cancel(const FString& Reason);
	void Finish();

	void Fail(const FString& Message, bool bThrowControlledException = true);
	void ExpectError(
		const FString& Pattern,
		EAutomationExpectedErrorFlags::MatchType MatchType,
		int32 Occurrences,
		bool bRegex);

	bool EnqueueAction(
		FName Callback,
		const FString& Description,
		bool bTeardown);
	bool EnqueueCondition(
		FName Callback,
		double TimeoutSeconds,
		const FString& Description);
	bool EnqueueDelay(double Seconds, const FString& Description);
	bool EnqueueAdvanced(
		ULatentAutomationCommand* Command,
		double TimeoutSeconds);

	bool CreateTestWorld(bool bInitializeGameSubsystems);
	void DestroyTestWorld();
	UObject* SpawnObject(UClass* ObjectClass, UObject* Outer);
	AActor* SpawnActor(
		UClass* ActorClass,
		const FVector& Location,
		const FRotator& Rotation);
	UActorComponent* SpawnComponent(
		UClass* ComponentClass,
		AActor* Owner,
		bool bRegister);
	void BeginPlay(AActor* Actor);
	void BeginPlayAll();
	void TickWorld(float DeltaSeconds, int32 NumTicks);
	void TickActor(AActor* Actor, float DeltaSeconds, int32 NumTicks);
	void TickComponent(
		UActorComponent* Component,
		float DeltaSeconds,
		int32 NumTicks);
	void AdvanceTime(float DeltaSeconds, int32 NumTicks);
	void DestroyActor(AActor* Actor, bool bDrain);

	const TArray<FName>& GetTrace() const
	{
		return Trace;
	}

private:
	friend class FAngelscriptScriptTestRunner;

	bool ResolveSuite();
	bool InvokeReflected(FName MethodName);
	bool InvokeScriptCallback(FName MethodName, bool bCondition, bool& OutValue);
	bool ValidateTimeout(double Seconds, const FString& Operation);
	bool CanBuildMainCommand() const;
	bool CanRegisterTeardown() const;
	void RunAfterEach();
	void RunNextTeardown();
	void FinalizeAdvancedCommand(
		FAngelscriptScriptTestCommand& Command);
	void Cleanup();
	bool OwnsActor(const AActor* Actor) const;
	bool OwnsComponent(const UActorComponent* Component) const;

	FAngelscriptScriptTestDescriptor Descriptor;
	FAutomationTestBase* AutomationTest = nullptr;
	TStrongObjectPtr<UAngelscriptTestSuite> Suite;
	EAngelscriptScriptTestPhase Phase =
		EAngelscriptScriptTestPhase::None;
	bool bFailed = false;
	bool bCanceled = false;
	bool bAfterEachRan = false;
	bool bExpectedMessagesFinalized = false;
	bool bCleanupRan = false;
	FString CancellationReason;
	TArray<FName> Trace;
	TArray<FAngelscriptScriptTestCommand> MainCommands;
	TArray<FAngelscriptScriptTestCommand> TeardownCommands;
	int32 MainCommandIndex = 0;
	FAngelscriptEngine* OwningEngine = nullptr;

	TStrongObjectPtr<UWorld> TestWorld;
	TStrongObjectPtr<UGameInstance> TestGameInstance;
	bool bWorldRooted = false;
	TArray<TStrongObjectPtr<UObject>> TrackedObjects;
	TArray<TWeakObjectPtr<AActor>> TrackedActors;
	TArray<TWeakObjectPtr<UActorComponent>> TrackedComponents;
};

class ANGELSCRIPTRUNTIME_API FAngelscriptScriptTestRunner
{
public:
	static bool Run(
		const FAngelscriptScriptTestId& Id,
		FAutomationTestBase& AutomationTest);

	static TSharedPtr<FAngelscriptScriptTestExecutionContext> Start(
		const FAngelscriptScriptTestId& Id,
		FAutomationTestBase& AutomationTest,
		bool bEnqueueAutomation = true);

	static TSharedPtr<FAngelscriptScriptTestExecutionContext> FindContext(
		const UAngelscriptTestSuite* Suite);
	static FAngelscriptScriptTestExecutionContext* GetActiveContext();

	static UWorld* FindWorld(const UAngelscriptTestSuite* Suite);

	static void CancelModules(
		const TSet<FString>& ModuleNames,
		const FString& Reason);

	static void CancelEngine(
		FAngelscriptEngine* Engine,
		const FString& Reason);
	static void CancelAll(const FString& Reason);
	static bool InvokeSuiteLifecycle(
		UAngelscriptTestSuite* Suite,
		FName MethodName,
		EAngelscriptScriptTestPhase Phase,
		FAutomationTestBase& Result);
	static bool ReportSuiteLifecycleMisuse(
		const UAngelscriptTestSuite* Suite,
		const FString& Message);
	static bool ReportActiveLifecycleMisuse(const FString& Message);
	static void ReportActiveContextMisuse(const FString& Message);
	static bool IsControlledException(const ANSICHAR* Exception);
	static bool IsExecutingScriptCallback();
	static bool ShouldSuppressScriptExceptionLogging();

private:
	friend class FAngelscriptScriptTestExecutionContext;

	static void Associate(
		UAngelscriptTestSuite* Suite,
		const TSharedRef<FAngelscriptScriptTestExecutionContext>& Context);
	static void Dissociate(UAngelscriptTestSuite* Suite);
	static void RememberActive(
		const TSharedRef<FAngelscriptScriptTestExecutionContext>& Context);
	static void ForgetActive(
		const FAngelscriptScriptTestExecutionContext* Context);
};
