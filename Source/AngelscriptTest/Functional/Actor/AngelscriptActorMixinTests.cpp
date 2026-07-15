#include "CQTest.h"
#include "AngelscriptTestMacros.h"
#include "Functional/Actor/AngelscriptActorTestHelpers.h"
#include "FunctionLibraries/AngelscriptActorLibrary.h"

#if WITH_ANGELSCRIPT_UNITTESTS

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
		ASSERT_THAT(IsNotNull(Actor, TEXT("Actor should spawn")));
		W.BeginPlay(*Actor);

		FFunctionInvoker Invoker(*TestRunner, Actor, FName(TEXT("RunSetQuatTest")));
		if (!Invoker.IsValid()) return;
		ASSERT_THAT(AreEqual(
			1,
			Invoker.CallAndReturn<int32>(INDEX_NONE),
			TEXT("SetActorQuat should apply FQuat rotation to the actor")));
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
		ASSERT_THAT(IsNotNull(Actor, TEXT("Actor should spawn")));
		W.BeginPlay(*Actor);

		FFunctionInvoker Invoker(*TestRunner, Actor, FName(TEXT("RunSetLocSweepTest")));
		if (!Invoker.IsValid()) return;
		ASSERT_THAT(AreEqual(
			1,
			Invoker.CallAndReturn<int32>(INDEX_NONE),
			TEXT("SetActorLocation advanced mixin should move the actor and support teleport flag")));
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
		ASSERT_THAT(IsNotNull(Actor, TEXT("Actor should spawn")));
		W.BeginPlay(*Actor);

		FFunctionInvoker Invoker(*TestRunner, Actor, FName(TEXT("RunSetLocAndRotTest")));
		if (!Invoker.IsValid()) return;
		ASSERT_THAT(AreEqual(
			1,
			Invoker.CallAndReturn<int32>(INDEX_NONE),
			TEXT("SetActorLocationAndRotation should set both position and rotation")));
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
		ASSERT_THAT(IsNotNull(Actor, TEXT("Actor should spawn")));
		W.BeginPlay(*Actor);

		FFunctionInvoker Invoker(*TestRunner, Actor, FName(TEXT("RunGetAttachedTest")));
		if (!Invoker.IsValid()) return;
		ASSERT_THAT(AreEqual(
			1,
			Invoker.CallAndReturn<int32>(INDEX_NONE),
			TEXT("GetAttachedActors should enumerate attached child actors")));
	}

	TEST_METHOD(GetAttachedActorsOfClassNullGuards)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		static const FName ModuleName(TEXT("TestActorMixinGetAttachedOfClass"));
		ON_SCOPE_EXIT { Engine.DiscardModule(*ModuleName.ToString()); };

		UClass* ScriptClass = CompileScriptModule(*TestRunner, Engine, ModuleName,
			TEXT("TestActorMixinGetAttachedOfClass.as"),
			TEXT(R"AS(
UCLASS()
class ATestMixinAttachedFilterChild : AActor
{
	UPROPERTY(DefaultComponent, RootComponent)
	USceneComponent ChildRoot;
}

UCLASS()
class ATestMixinAttachedFilterOther : AActor
{
	UPROPERTY(DefaultComponent, RootComponent)
	USceneComponent OtherRoot;
}

UCLASS()
class ATestMixinAttachedFilterParent : AActor
{
	UPROPERTY(DefaultComponent, RootComponent)
	USceneComponent ParentRoot;

	UFUNCTION()
	int RunValidClassFilter()
	{
		AActor MatchingChild = SpawnActor(ATestMixinAttachedFilterChild::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, n"MatchingChild");
		if (MatchingChild == nullptr)
			return 10;
		MatchingChild.AttachToActor(this, n"NAME_None", EAttachmentRule::KeepWorld, EAttachmentRule::KeepWorld, EAttachmentRule::KeepWorld, false);

		AActor OtherChild = SpawnActor(ATestMixinAttachedFilterOther::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, n"OtherChild");
		if (OtherChild == nullptr)
			return 20;
		OtherChild.AttachToActor(this, n"NAME_None", EAttachmentRule::KeepWorld, EAttachmentRule::KeepWorld, EAttachmentRule::KeepWorld, false);

		TArray<AActor> FilteredActors = GetAttachedActorsOfClass(ATestMixinAttachedFilterChild::StaticClass());
		if (FilteredActors.Num() != 1)
			return 30;
		if (!FilteredActors[0].IsA(ATestMixinAttachedFilterChild::StaticClass()))
			return 40;

		return 1;
	}

	UFUNCTION()
	int RunNullClass()
	{
		AActor Child = SpawnActor(ATestMixinAttachedFilterChild::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, n"NullClassChild");
		if (Child == nullptr)
			return 10;
		Child.AttachToActor(this, n"NAME_None", EAttachmentRule::KeepWorld, EAttachmentRule::KeepWorld, EAttachmentRule::KeepWorld, false);

		TSubclassOf<AActor> NullClass;
		TArray<AActor> FilteredActors = GetAttachedActorsOfClass(NullClass);
		return FilteredActors.Num() == 0 ? 1 : 20;
	}

}
)AS"),
			TEXT("ATestMixinAttachedFilterParent"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Attached actor class-filter regression class should compile")));
		if (ScriptClass == nullptr) return;

		FAngelscriptTestWorld W(*TestRunner, Engine);
		if (!W.IsValid()) return;
		AActor* Actor = W.SpawnActorOfClass(ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Attached actor class-filter regression actor should spawn")));
		if (Actor == nullptr) return;
		W.BeginPlay(*Actor);

		FFunctionInvoker ValidFilterInvoker(*TestRunner, Actor, FName(TEXT("RunValidClassFilter")));
		if (!ValidFilterInvoker.IsValid()) return;
		ASSERT_THAT(AreEqual(
			1,
			ValidFilterInvoker.CallAndReturn<int32>(INDEX_NONE),
			TEXT("GetAttachedActorsOfClass should retain only actors matching the requested class")));

		FFunctionInvoker NullClassInvoker(*TestRunner, Actor, FName(TEXT("RunNullClass")));
		if (!NullClassInvoker.IsValid()) return;
		ASSERT_THAT(AreEqual(
			1,
			NullClassInvoker.CallAndReturn<int32>(INDEX_NONE),
			TEXT("GetAttachedActorsOfClass should return an empty array for a null class")));

		const TArray<AActor*> NullActorResult = UAngelscriptActorLibrary::GetAttachedActorsOfClass(nullptr, AActor::StaticClass());
		ASSERT_THAT(AreEqual(
			0,
			NullActorResult.Num(),
			TEXT("GetAttachedActorsOfClass should return an empty array for a null actor")));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
