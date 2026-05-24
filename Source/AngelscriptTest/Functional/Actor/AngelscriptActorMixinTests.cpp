#include "CQTest.h"
#include "Shared/AngelscriptTestMacros.h"
#include "Functional/Actor/AngelscriptActorTestHelpers.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptFunctionalTestUtils;
using namespace AngelscriptActorTestUtils;

TEST_CLASS_WITH_FLAGS(FAngelscriptActorMixinTest,
	"Angelscript.TestModule.Actor.Mixin",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	BEFORE_ALL()
	{
		ASTEST_CREATE_ENGINE();
	}

	AFTER_ALL()
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		ASTEST_RESET_ENGINE(Engine);
	}

	TEST_METHOD(SetActorQuat)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		static const FName ModuleName(TEXT("TestActorMixinSetQuat"));
		ON_SCOPE_EXIT { Engine.DiscardModule(*ModuleName.ToString()); };

		UClass* ScriptClass = CompileScriptModule(*TestRunner, Engine, ModuleName,
			TEXT("TestActorMixinSetQuat.as"),
			TEXT(R"AS(
UCLASS()
class ATestActorMixinSetQuat : AActor
{
	UPROPERTY(DefaultComponent, RootComponent)
	USceneComponent RootScene;

	UFUNCTION()
	int RunSetQuatTest()
	{
		FQuat Target = FQuat(FRotator(0.0, 90.0, 0.0));
		SetActorQuat(Target);

		FRotator Result = GetActorRotation();
		float YawDiff = Math::Abs(Result.Yaw - 90.0);
		if (YawDiff > 1.0)
			return 10;

		SetActorQuat(FQuat(FRotator(0.0, 45.0, 0.0)));
		FRotator Result2 = GetActorRotation();
		float YawDiff2 = Math::Abs(Result2.Yaw - 45.0);
		if (YawDiff2 > 1.0)
			return 20;

		return 1;
	}
}
)AS"),
			TEXT("ATestActorMixinSetQuat"));
		if (ScriptClass == nullptr) return;

		FAngelscriptTestWorld W(*TestRunner, Engine);
		if (!W.IsValid()) return;
		AActor* Actor = W.SpawnActorOfClass(ScriptClass);
		if (!TestRunner->TestNotNull(TEXT("Actor should spawn"), Actor)) return;
		W.BeginPlay(*Actor);

		FFunctionInvoker Invoker(*TestRunner, Actor, FName(TEXT("RunSetQuatTest")));
		if (!Invoker.IsValid()) return;
		TestRunner->TestEqual(TEXT("SetActorQuat should apply FQuat rotation to the actor"),
			Invoker.CallAndReturn<int32>(INDEX_NONE), 1);
	}

	TEST_METHOD(SetActorLocationSweep)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		static const FName ModuleName(TEXT("TestActorMixinSetLocSweep"));
		ON_SCOPE_EXIT { Engine.DiscardModule(*ModuleName.ToString()); };

		UClass* ScriptClass = CompileScriptModule(*TestRunner, Engine, ModuleName,
			TEXT("TestActorMixinSetLocSweep.as"),
			TEXT(R"AS(
UCLASS()
class ATestActorMixinSetLocSweep : AActor
{
	UPROPERTY(DefaultComponent, RootComponent)
	USceneComponent RootScene;

	UFUNCTION()
	int RunSetLocSweepTest()
	{
		FHitResult Hit;
		bool bMoved = SetActorLocation(FVector(500.0, 0.0, 0.0), false, Hit, false);
		if (!bMoved)
			return 10;

		FVector NewLoc = GetActorLocation();
		if (!NewLoc.Equals(FVector(500.0, 0.0, 0.0)))
			return 20;

		FHitResult Hit2;
		bool bMoved2 = SetActorLocation(FVector(1000.0, 200.0, 0.0), false, Hit2, true);
		if (!bMoved2)
			return 30;

		FVector FinalLoc = GetActorLocation();
		if (!FinalLoc.Equals(FVector(1000.0, 200.0, 0.0)))
			return 40;

		return 1;
	}
}
)AS"),
			TEXT("ATestActorMixinSetLocSweep"));
		if (ScriptClass == nullptr) return;

		FAngelscriptTestWorld W(*TestRunner, Engine);
		if (!W.IsValid()) return;
		AActor* Actor = W.SpawnActorOfClass(ScriptClass);
		if (!TestRunner->TestNotNull(TEXT("Actor should spawn"), Actor)) return;
		W.BeginPlay(*Actor);

		FFunctionInvoker Invoker(*TestRunner, Actor, FName(TEXT("RunSetLocSweepTest")));
		if (!Invoker.IsValid()) return;
		TestRunner->TestEqual(TEXT("SetActorLocation advanced mixin should move the actor and support teleport flag"),
			Invoker.CallAndReturn<int32>(INDEX_NONE), 1);
	}

	TEST_METHOD(SetActorLocationAndRotation)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		static const FName ModuleName(TEXT("TestActorMixinSetLocAndRot"));
		ON_SCOPE_EXIT { Engine.DiscardModule(*ModuleName.ToString()); };

		UClass* ScriptClass = CompileScriptModule(*TestRunner, Engine, ModuleName,
			TEXT("TestActorMixinSetLocAndRot.as"),
			TEXT(R"AS(
UCLASS()
class ATestActorMixinSetLocAndRot : AActor
{
	UPROPERTY(DefaultComponent, RootComponent)
	USceneComponent RootScene;

	UFUNCTION()
	int RunSetLocAndRotTest()
	{
		FVector TargetLoc = FVector(100.0, 200.0, 300.0);
		FRotator TargetRot = FRotator(0.0, 90.0, 0.0);
		FHitResult Hit;

		bool bMoved = SetActorLocationAndRotation(TargetLoc, TargetRot, false, Hit, false);
		if (!bMoved)
			return 10;

		FVector ResultLoc = GetActorLocation();
		if (!ResultLoc.Equals(TargetLoc))
			return 20;

		FRotator ResultRot = GetActorRotation();
		float YawDiff = Math::Abs(ResultRot.Yaw - 90.0);
		if (YawDiff > 1.0)
			return 30;

		return 1;
	}
}
)AS"),
			TEXT("ATestActorMixinSetLocAndRot"));
		if (ScriptClass == nullptr) return;

		FAngelscriptTestWorld W(*TestRunner, Engine);
		if (!W.IsValid()) return;
		AActor* Actor = W.SpawnActorOfClass(ScriptClass);
		if (!TestRunner->TestNotNull(TEXT("Actor should spawn"), Actor)) return;
		W.BeginPlay(*Actor);

		FFunctionInvoker Invoker(*TestRunner, Actor, FName(TEXT("RunSetLocAndRotTest")));
		if (!Invoker.IsValid()) return;
		TestRunner->TestEqual(TEXT("SetActorLocationAndRotation should set both position and rotation"),
			Invoker.CallAndReturn<int32>(INDEX_NONE), 1);
	}

	TEST_METHOD(GetAttachedActors)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		static const FName ModuleName(TEXT("TestActorMixinGetAttached"));
		ON_SCOPE_EXIT { Engine.DiscardModule(*ModuleName.ToString()); };

		UClass* ScriptClass = CompileScriptModule(*TestRunner, Engine, ModuleName,
			TEXT("TestActorMixinGetAttached.as"),
			TEXT(R"AS(
UCLASS()
class ATestMixinAttachChild : AActor
{
	UPROPERTY(DefaultComponent, RootComponent)
	USceneComponent ChildRoot;
}

UCLASS()
class ATestMixinAttachParent : AActor
{
	UPROPERTY(DefaultComponent, RootComponent)
	USceneComponent ParentRoot;

	UFUNCTION()
	int RunGetAttachedTest()
	{
		TArray<AActor> Before;
		GetAttachedActors(Before);
		if (Before.Num() != 0)
			return 10;

		AActor Child1 = SpawnActor(ATestMixinAttachChild::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, n"Child1");
		if (Child1 == nullptr)
			return 20;
		Child1.AttachToActor(this, n"NAME_None", EAttachmentRule::KeepWorld, EAttachmentRule::KeepWorld, EAttachmentRule::KeepWorld, false);

		AActor Child2 = SpawnActor(ATestMixinAttachChild::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, n"Child2");
		if (Child2 == nullptr)
			return 30;
		Child2.AttachToActor(this, n"NAME_None", EAttachmentRule::KeepWorld, EAttachmentRule::KeepWorld, EAttachmentRule::KeepWorld, false);

		TArray<AActor> After;
		GetAttachedActors(After);
		if (After.Num() != 2)
			return 40;

		return 1;
	}
}
)AS"),
			TEXT("ATestMixinAttachParent"));
		if (ScriptClass == nullptr) return;

		FAngelscriptTestWorld W(*TestRunner, Engine);
		if (!W.IsValid()) return;
		AActor* Actor = W.SpawnActorOfClass(ScriptClass);
		if (!TestRunner->TestNotNull(TEXT("Actor should spawn"), Actor)) return;
		W.BeginPlay(*Actor);

		FFunctionInvoker Invoker(*TestRunner, Actor, FName(TEXT("RunGetAttachedTest")));
		if (!Invoker.IsValid()) return;
		TestRunner->TestEqual(TEXT("GetAttachedActors should enumerate attached child actors"),
			Invoker.CallAndReturn<int32>(INDEX_NONE), 1);
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
