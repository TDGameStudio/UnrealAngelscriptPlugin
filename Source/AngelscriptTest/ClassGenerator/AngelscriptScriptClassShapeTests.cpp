#include "AngelscriptFunctionalTestUtils.h"

#include "AngelscriptTestMacros.h"
#include "CQTest.h"
#include "ClassGenerator/ASClass.h"
#include "Components/ActorTestSpawner.h"
#include "Misc/ScopeExit.h"
#include "UObject/UnrealType.h"

// Test Layer: UE Functional
#if WITH_ANGELSCRIPT_UNITTESTS

namespace ScriptClassShapeTest
{
	FAngelscriptEngine& AcquireFreshScriptClassShapeEngine()
	{
		DestroySharedAndStrayGlobalTestEngine();
		return AcquireCleanSharedCloneEngine();
	}

	int32 CountDeclaredProperties(const UClass& ScriptClass)
	{
		int32 PropertyCount = 0;
		for (TFieldIterator<FProperty> It(&ScriptClass, EFieldIteratorFlags::ExcludeSuper); It; ++It)
		{
			++PropertyCount;
		}

		return PropertyCount;
	}
}

TEST_CLASS_WITH_FLAGS(FAngelscriptScriptClassShapeTests,
	"Angelscript.TestModule.ScriptClass",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	TEST_METHOD(ScriptInheritancePreservesParentPropertyAndOverride)
	{
		FAngelscriptEngine& Engine = ScriptClassShapeTest::AcquireFreshScriptClassShapeEngine();
		FAngelscriptEngineScope EngineScope(Engine);
		static const FName ModuleName(TEXT("TestScriptClassScriptInheritance"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
			ResetSharedCloneEngine(Engine);
		};

		static const FString ScriptSource = ASTEST_AS(R"AS(
			UCLASS()
			class ATestScriptInheritanceParent : AActor
			{
				UPROPERTY()
				int ParentValue = 21;

				UFUNCTION(BlueprintEvent)
				int GetValue()
				{
					return ParentValue;
				}
			}

			UCLASS()
			class ATestScriptInheritanceChild : ATestScriptInheritanceParent
			{
				UFUNCTION(BlueprintOverride)
				int GetValue()
				{
					return ParentValue * 10 + 7;
				}
			}
			)AS");

		UClass* ParentClass = AngelscriptFunctionalTestUtils::CompileScriptModule(*TestRunner, Engine, ModuleName,
			TEXT("TestScriptClassScriptInheritance.as"), ScriptSource, TEXT("ATestScriptInheritanceParent"));
		if (ParentClass == nullptr) { return; }

		UClass* ChildClass = ::FindGeneratedClass(&Engine, TEXT("ATestScriptInheritanceChild"));
		ASSERT_THAT(IsNotNull(ChildClass, TEXT("Script-inheritance test case should generate the child class")));
		if (ChildClass == nullptr) { return; }

		UASClass* ParentASClass = Cast<UASClass>(ParentClass);
		UASClass* ChildASClass = Cast<UASClass>(ChildClass);
		ASSERT_THAT(IsNotNull(ParentASClass, TEXT("Script-inheritance test case should compile the parent as a UASClass")));
		ASSERT_THAT(IsNotNull(ChildASClass, TEXT("Script-inheritance test case should compile the child as a UASClass")));
		if (ParentASClass == nullptr || ChildASClass == nullptr)
		{ return; }

		ASSERT_THAT(IsTrue(ChildClass->IsChildOf(AActor::StaticClass()), TEXT("Script-inheritance test case should keep the child class actor-derived")));
		ASSERT_THAT(IsTrue(ChildClass->IsChildOf(ParentClass), TEXT("Script-inheritance test case should make the child class inherit from the parent class")));
		ASSERT_THAT(AreEqual(ParentClass, ChildClass->GetSuperClass(), TEXT("Script-inheritance test case should keep the generated child superclass exact")));
		ASSERT_THAT(AreEqual(ParentClass, ParentASClass->GetMostUpToDateClass(), TEXT("Script-inheritance parent should already be its own most-up-to-date class")));
		ASSERT_THAT(AreEqual(ChildClass, ChildASClass->GetMostUpToDateClass(), TEXT("Script-inheritance child should already be its own most-up-to-date class")));

		UObject* ChildDefaultObject = ChildClass->GetDefaultObject();
		ASSERT_THAT(IsNotNull(ChildDefaultObject, TEXT("Script-inheritance test case should provide a child CDO")));
		if (ChildDefaultObject == nullptr) { return; }

		int32 ChildDefaultParentValue = 0;
		if (!AngelscriptFunctionalTestUtils::ReadPropertyValue<FIntProperty>(*TestRunner, ChildDefaultObject, TEXT("ParentValue"), ChildDefaultParentValue)) { return; }

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* ParentActor = AngelscriptFunctionalTestUtils::SpawnScriptActor(*TestRunner, Spawner, ParentClass);
		AActor* ChildActor = AngelscriptFunctionalTestUtils::SpawnScriptActor(*TestRunner, Spawner, ChildClass);
		ASSERT_THAT(IsNotNull(ParentActor, TEXT("Script-inheritance test case should spawn the parent actor")));
		ASSERT_THAT(IsNotNull(ChildActor, TEXT("Script-inheritance test case should spawn the child actor")));
		if (ParentActor == nullptr || ChildActor == nullptr)
		{ return; }

		int32 ChildInstanceParentValue = 0;
		if (!AngelscriptFunctionalTestUtils::ReadPropertyValue<FIntProperty>(*TestRunner, ChildActor, TEXT("ParentValue"), ChildInstanceParentValue)) { return; }

		UFunction* ParentGetValueFunction = ::FindGeneratedFunction(ParentClass, TEXT("GetValue"));
		UFunction* ChildGetValueFunction = ::FindGeneratedFunction(ChildClass, TEXT("GetValue"));
		ASSERT_THAT(IsNotNull(ParentGetValueFunction, TEXT("Script-inheritance test case should generate the parent GetValue function")));
		ASSERT_THAT(IsNotNull(ChildGetValueFunction, TEXT("Script-inheritance test case should generate the child GetValue function")));
		if (ParentGetValueFunction == nullptr || ChildGetValueFunction == nullptr)
		{ return; }

		int32 ParentResult = 0;
		int32 ChildResult = 0;
		const bool bParentExecuted = ::ExecuteGeneratedIntEventOnGameThread(&Engine, ParentActor, ParentGetValueFunction, ParentResult);
		const bool bChildExecuted = ::ExecuteGeneratedIntEventOnGameThread(&Engine, ChildActor, ChildGetValueFunction, ChildResult);
		ASSERT_THAT(IsTrue(bParentExecuted, TEXT("Script-inheritance test case should execute the parent GetValue function")));
		ASSERT_THAT(IsTrue(bChildExecuted, TEXT("Script-inheritance test case should execute the child GetValue function")));
		if (!bParentExecuted || !bChildExecuted)
		{ return; }

		ASSERT_THAT(AreEqual(21, ChildDefaultParentValue, TEXT("Script-inheritance test case should flow the parent default value into the child CDO")));
		ASSERT_THAT(AreEqual(21, ChildInstanceParentValue, TEXT("Script-inheritance test case should expose the inherited parent property on child instances")));
		ASSERT_THAT(AreEqual(21, ParentResult, TEXT("Script-inheritance test case should preserve the parent function result on the parent actor")));
		ASSERT_THAT(AreEqual(217, ChildResult, TEXT("Script-inheritance test case should dispatch the child override instead of the parent implementation")));
	}

	TEST_METHOD(EmptyActorCompilesAndSpawns)
	{
		FAngelscriptEngine& Engine = ScriptClassShapeTest::AcquireFreshScriptClassShapeEngine();
		FAngelscriptEngineScope EngineScope(Engine);
		static const FName ModuleName(TEXT("TestScriptClassEmptyActor"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
			ResetSharedCloneEngine(Engine);
		};

		UClass* ScriptClass = AngelscriptFunctionalTestUtils::CompileScriptModule(*TestRunner, Engine, ModuleName,
			TEXT("TestScriptClassEmptyActor.as"),
			ASTEST_AS(R"AS(
				UCLASS()
				class AEmptyScriptActor : AActor
				{
				}
				)AS"),
			TEXT("AEmptyScriptActor"));
		if (ScriptClass == nullptr) { return; }

		UASClass* ASClass = Cast<UASClass>(ScriptClass);
		ASSERT_THAT(IsNotNull(ASClass, TEXT("Empty script actor test case should generate a UASClass")));
		if (ASClass == nullptr) { return; }

		ASSERT_THAT(IsTrue(ScriptClass->IsChildOf(AActor::StaticClass()), TEXT("Empty script actor test case should stay actor-derived")));
		ASSERT_THAT(AreEqual(AActor::StaticClass(), ScriptClass->GetSuperClass(), TEXT("Empty script actor test case should use AActor as the exact generated superclass")));
		ASSERT_THAT(AreEqual(0, ScriptClassShapeTest::CountDeclaredProperties(*ScriptClass), TEXT("Empty script actor test case should not synthesize any declared user properties")));

		UObject* ClassDefaultObject = ScriptClass->GetDefaultObject();
		ASSERT_THAT(IsNotNull(ClassDefaultObject, TEXT("Empty script actor test case should provide a class default object")));
		if (ClassDefaultObject == nullptr) { return; }

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = AngelscriptFunctionalTestUtils::SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Empty script actor test case should spawn the generated actor class")));
		if (Actor == nullptr) { return; }

		AngelscriptFunctionalTestUtils::BeginPlayActor(Engine, *Actor);
		ASSERT_THAT(IsTrue(Actor->HasActorBegunPlay(), TEXT("Empty script actor test case should enter BeginPlay even without user properties or functions")));
		ASSERT_THAT(IsTrue(Actor->Destroy(), TEXT("Empty script actor test case should allow the spawned actor to enter the destroy flow")));
		ASSERT_THAT(IsTrue(Actor->IsActorBeingDestroyed(), TEXT("Empty script actor test case should mark the actor as being destroyed after Destroy()")));
	}
};

#endif
