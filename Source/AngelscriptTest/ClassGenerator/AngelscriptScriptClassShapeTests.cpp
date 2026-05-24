#include "AngelscriptFunctionalTestUtils.h"

#include "AngelscriptTestMacros.h"
#include "CQTest.h"
#include "ClassGenerator/ASClass.h"
#include "Components/ActorTestSpawner.h"
#include "Misc/ScopeExit.h"
#include "UObject/UnrealType.h"

// Test Layer: UE Functional
#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptFunctionalTestUtils;

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
	TEST_METHOD(ScriptInheritancePreservesParentPropertyAndOverride)
	{
		using namespace ScriptClassShapeTest;
		FAngelscriptEngine& Engine = ScriptClassShapeTest::AcquireFreshScriptClassShapeEngine();
		FAngelscriptEngineScope EngineScope(Engine);
		static const FName ModuleName(TEXT("TestScriptClassScriptInheritance"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
			ASTEST_RESET_ENGINE(Engine);
		};

		static const FString ScriptSource = TEXT(R"AS(
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

		UClass* ParentClass = CompileScriptModule(*TestRunner, Engine, ModuleName,
			TEXT("TestScriptClassScriptInheritance.as"), ScriptSource, TEXT("ATestScriptInheritanceParent"));
		if (ParentClass == nullptr) { return; }

		UClass* ChildClass = FindGeneratedClass(&Engine, TEXT("ATestScriptInheritanceChild"));
		if (!TestRunner->TestNotNull(TEXT("Script-inheritance test case should generate the child class"), ChildClass)) { return; }

		UASClass* ParentASClass = Cast<UASClass>(ParentClass);
		UASClass* ChildASClass = Cast<UASClass>(ChildClass);
		if (!TestRunner->TestNotNull(TEXT("Script-inheritance test case should compile the parent as a UASClass"), ParentASClass)
			|| !TestRunner->TestNotNull(TEXT("Script-inheritance test case should compile the child as a UASClass"), ChildASClass))
		{ return; }

		TestRunner->TestTrue(TEXT("Script-inheritance test case should keep the child class actor-derived"), ChildClass->IsChildOf(AActor::StaticClass()));
		TestRunner->TestTrue(TEXT("Script-inheritance test case should make the child class inherit from the parent class"), ChildClass->IsChildOf(ParentClass));
		TestRunner->TestEqual(TEXT("Script-inheritance test case should keep the generated child superclass exact"), ChildClass->GetSuperClass(), ParentClass);
		TestRunner->TestEqual(TEXT("Script-inheritance parent should already be its own most-up-to-date class"), ParentASClass->GetMostUpToDateClass(), ParentClass);
		TestRunner->TestEqual(TEXT("Script-inheritance child should already be its own most-up-to-date class"), ChildASClass->GetMostUpToDateClass(), ChildClass);

		UObject* ChildDefaultObject = ChildClass->GetDefaultObject();
		if (!TestRunner->TestNotNull(TEXT("Script-inheritance test case should provide a child CDO"), ChildDefaultObject)) { return; }

		int32 ChildDefaultParentValue = 0;
		if (!ReadPropertyValue<FIntProperty>(*TestRunner, ChildDefaultObject, TEXT("ParentValue"), ChildDefaultParentValue)) { return; }

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* ParentActor = SpawnScriptActor(*TestRunner, Spawner, ParentClass);
		AActor* ChildActor = SpawnScriptActor(*TestRunner, Spawner, ChildClass);
		if (!TestRunner->TestNotNull(TEXT("Script-inheritance test case should spawn the parent actor"), ParentActor)
			|| !TestRunner->TestNotNull(TEXT("Script-inheritance test case should spawn the child actor"), ChildActor))
		{ return; }

		int32 ChildInstanceParentValue = 0;
		if (!ReadPropertyValue<FIntProperty>(*TestRunner, ChildActor, TEXT("ParentValue"), ChildInstanceParentValue)) { return; }

		UFunction* ParentGetValueFunction = FindGeneratedFunction(ParentClass, TEXT("GetValue"));
		UFunction* ChildGetValueFunction = FindGeneratedFunction(ChildClass, TEXT("GetValue"));
		if (!TestRunner->TestNotNull(TEXT("Script-inheritance test case should generate the parent GetValue function"), ParentGetValueFunction)
			|| !TestRunner->TestNotNull(TEXT("Script-inheritance test case should generate the child GetValue function"), ChildGetValueFunction))
		{ return; }

		int32 ParentResult = 0;
		int32 ChildResult = 0;
		if (!TestRunner->TestTrue(TEXT("Script-inheritance test case should execute the parent GetValue function"), ExecuteGeneratedIntEventOnGameThread(&Engine, ParentActor, ParentGetValueFunction, ParentResult))
			|| !TestRunner->TestTrue(TEXT("Script-inheritance test case should execute the child GetValue function"), ExecuteGeneratedIntEventOnGameThread(&Engine, ChildActor, ChildGetValueFunction, ChildResult)))
		{ return; }

		TestRunner->TestEqual(TEXT("Script-inheritance test case should flow the parent default value into the child CDO"), ChildDefaultParentValue, 21);
		TestRunner->TestEqual(TEXT("Script-inheritance test case should expose the inherited parent property on child instances"), ChildInstanceParentValue, 21);
		TestRunner->TestEqual(TEXT("Script-inheritance test case should preserve the parent function result on the parent actor"), ParentResult, 21);
		TestRunner->TestEqual(TEXT("Script-inheritance test case should dispatch the child override instead of the parent implementation"), ChildResult, 217);
	}

	TEST_METHOD(EmptyActorCompilesAndSpawns)
	{
		using namespace ScriptClassShapeTest;
		FAngelscriptEngine& Engine = ScriptClassShapeTest::AcquireFreshScriptClassShapeEngine();
		FAngelscriptEngineScope EngineScope(Engine);
		static const FName ModuleName(TEXT("TestScriptClassEmptyActor"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
			ASTEST_RESET_ENGINE(Engine);
		};

		UClass* ScriptClass = CompileScriptModule(*TestRunner, Engine, ModuleName,
			TEXT("TestScriptClassEmptyActor.as"),
			TEXT(R"AS(
UCLASS()
class AEmptyScriptActor : AActor
{
}
)AS"),
			TEXT("AEmptyScriptActor"));
		if (ScriptClass == nullptr) { return; }

		UASClass* ASClass = Cast<UASClass>(ScriptClass);
		if (!TestRunner->TestNotNull(TEXT("Empty script actor test case should generate a UASClass"), ASClass)) { return; }

		TestRunner->TestTrue(TEXT("Empty script actor test case should stay actor-derived"), ScriptClass->IsChildOf(AActor::StaticClass()));
		TestRunner->TestEqual(TEXT("Empty script actor test case should use AActor as the exact generated superclass"), ScriptClass->GetSuperClass(), AActor::StaticClass());
		TestRunner->TestEqual(TEXT("Empty script actor test case should not synthesize any declared user properties"), ScriptClassShapeTest::CountDeclaredProperties(*ScriptClass), 0);

		UObject* ClassDefaultObject = ScriptClass->GetDefaultObject();
		if (!TestRunner->TestNotNull(TEXT("Empty script actor test case should provide a class default object"), ClassDefaultObject)) { return; }

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		if (!TestRunner->TestNotNull(TEXT("Empty script actor test case should spawn the generated actor class"), Actor)) { return; }

		BeginPlayActor(Engine, *Actor);
		TestRunner->TestTrue(TEXT("Empty script actor test case should enter BeginPlay even without user properties or functions"), Actor->HasActorBegunPlay());
		TestRunner->TestTrue(TEXT("Empty script actor test case should allow the spawned actor to enter the destroy flow"), Actor->Destroy());
		TestRunner->TestTrue(TEXT("Empty script actor test case should mark the actor as being destroyed after Destroy()"), Actor->IsActorBeingDestroyed());
	}
};

#endif
