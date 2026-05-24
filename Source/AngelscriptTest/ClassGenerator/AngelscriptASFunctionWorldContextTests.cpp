#include "Shared/AngelscriptFunctionalTestUtils.h"
#include "Shared/AngelscriptTestMacros.h"

#include "CQTest.h"
#include "ClassGenerator/ASClass.h"
#include "Core/AngelscriptUhtCoverageTestTypes.h"
#include "Misc/ScopeExit.h"
#include "UObject/UnrealType.h"

// Test Layer: Runtime Integration
#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptFunctionalTestUtils;

namespace AngelscriptTest_ClassGenerator_AngelscriptASFunctionWorldContextTests_Private
{
	static const FName WorldContextModuleName(TEXT("ASFunctionWorldContext"));
	static const FString WorldContextFilename(TEXT("ASFunctionWorldContext.as"));
	static const FName WorldContextStaticsClassName(TEXT("UModule_ASFunctionWorldContextStatics"));

	struct FCheckWorldContextParams
	{
		AActor* WorldContextObject = nullptr;
		int32 Value = 0;
		int32 ReturnValue = 0;
	};
}

TEST_CLASS_WITH_FLAGS(FAngelscriptASFunctionWorldContextTests,
	"Angelscript.TestModule.ClassGenerator.ASFunction",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(StaticWorldContextRuntimeCallUsesValidParmOffset)
	{
		using namespace AngelscriptTest_ClassGenerator_AngelscriptASFunctionWorldContextTests_Private;
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine);
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*WorldContextModuleName.ToString());
			ASTEST_RESET_ENGINE(Engine);
		};

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor& ContextActor = Spawner.SpawnActor<AActor>();
		AActor& PreviousContextActor = Spawner.SpawnActor<AActor>();
		UObject* PreviousContext = &PreviousContextActor;
		if (!TestRunner->TestNotNull(TEXT("World-context function test should create a test case actor"), &ContextActor)
			|| !TestRunner->TestNotNull(TEXT("World-context function test should create a previous ambient context"), PreviousContext))
		{ return; }

		const FString ScriptSource = TEXT(R"AS(
UFUNCTION(BlueprintCallable, meta = (WorldContext = "WorldContextObject"))
int CheckWorldContext(AActor WorldContextObject, int Value)
{
	if (__WorldContext() != WorldContextObject)
		return -10;

	UWorld CurrentWorld = GetCurrentWorld();
	if (CurrentWorld == null)
		return -20;
	if (CurrentWorld != WorldContextObject.GetWorld())
		return -30;

	return Value;
}
)AS");

		const bool bCompiled = CompileAnnotatedModuleFromMemory(&Engine, WorldContextModuleName, WorldContextFilename, ScriptSource);
		if (!TestRunner->TestTrue(TEXT("World-context function test should compile the annotated static callable module"), bCompiled)) { return; }

		UClass* ScriptClass = FindGeneratedClass(&Engine, WorldContextStaticsClassName);
		if (!TestRunner->TestNotNull(TEXT("World-context function test should generate the module statics class"), ScriptClass)) { return; }

		UFunction* GeneratedFunction = FindGeneratedFunction(ScriptClass, TEXT("CheckWorldContext"));
		UASFunction* ScriptFunction = Cast<UASFunction>(GeneratedFunction);
		FObjectProperty* WorldContextProperty = FindFProperty<FObjectProperty>(GeneratedFunction, TEXT("WorldContextObject"));
		FIntProperty* ValueProperty = FindFProperty<FIntProperty>(GeneratedFunction, TEXT("Value"));
		FIntProperty* ReturnProperty = FindFProperty<FIntProperty>(GeneratedFunction, TEXT("ReturnValue"));
		if (!TestRunner->TestNotNull(TEXT("World-context function test should expose the generated static function"), GeneratedFunction)
			|| !TestRunner->TestNotNull(TEXT("World-context function test should generate a UASFunction"), ScriptFunction)
			|| !TestRunner->TestNotNull(TEXT("World-context function test should expose the WorldContextObject property"), WorldContextProperty)
			|| !TestRunner->TestNotNull(TEXT("World-context function test should expose the Value property"), ValueProperty)
			|| !TestRunner->TestNotNull(TEXT("World-context function test should expose the ReturnValue property"), ReturnProperty))
		{ return; }

		TestRunner->TestTrue(TEXT("World-context function test should compile the callable as a static UFunction"), GeneratedFunction->HasAnyFunctionFlags(FUNC_Static));
		TestRunner->TestFalse(TEXT("Explicit WorldContext metadata should not generate an extra synthetic parameter"), ScriptFunction->bIsWorldContextGenerated);
		TestRunner->TestEqual(TEXT("WorldContext parameter should stay at argument index 0"), ScriptFunction->WorldContextIndex, 0);
		if (!TestRunner->TestTrue(TEXT("WorldContext parameter should record a valid parameter offset"), ScriptFunction->WorldContextOffsetInParms >= 0)) { return; }
		if (!TestRunner->TestEqual(TEXT("WorldContext parameter offset should match the reflected property layout"), ScriptFunction->WorldContextOffsetInParms, WorldContextProperty->GetOffset_ForUFunction())) { return; }
		TestRunner->TestTrue(TEXT("Generated function parameters should be reported as Angelscript-generated properties"), IsAngelscriptGenerated(WorldContextProperty));
		TestRunner->TestTrue(TEXT("Generated function ordinary parameters should be reported as Angelscript-generated properties"), IsAngelscriptGenerated(ValueProperty));
		TestRunner->TestTrue(TEXT("Generated function return parameters should be reported as Angelscript-generated properties"), IsAngelscriptGenerated(ReturnProperty));
		TestRunner->TestTrue(TEXT("WorldContextObject parameter should be reported as an Angelscript world-context property"), IsAngelscriptWorldContextProperty(WorldContextProperty));
		TestRunner->TestFalse(TEXT("Ordinary generated parameters should not be reported as world-context properties"), IsAngelscriptWorldContextProperty(ValueProperty));
		TestRunner->TestFalse(TEXT("Generated return parameters should not be reported as world-context properties"), IsAngelscriptWorldContextProperty(ReturnProperty));

		UFunction* NativeFunction = UAngelscriptUhtCoverageTestLibrary::StaticClass()->FindFunctionByName(TEXT("RequiresWorldContext"));
		FIntProperty* NativeValueProperty = NativeFunction != nullptr ? FindFProperty<FIntProperty>(NativeFunction, TEXT("Value")) : nullptr;
		if (!TestRunner->TestNotNull(TEXT("World-context function test should find a native comparison function"), NativeFunction)
			|| !TestRunner->TestNotNull(TEXT("World-context function test should find a native comparison property"), NativeValueProperty))
		{ return; }
		TestRunner->TestFalse(TEXT("Native UFunction parameters should not be reported as Angelscript-generated properties"), IsAngelscriptGenerated(NativeValueProperty));
		TestRunner->TestFalse(TEXT("Native UFunction parameters should not be reported as Angelscript world-context properties"), IsAngelscriptWorldContextProperty(NativeValueProperty));

		FCheckWorldContextParams Params;
		Params.WorldContextObject = &ContextActor;
		Params.Value = 7;

		UObject* AmbientBeforeScope = FAngelscriptEngine::GetAmbientWorldContext();
		{
			FScopedTestWorldContextScope PreviousContextScope(PreviousContext);
			if (!TestRunner->TestTrue(TEXT("Outer test scope should install the previous ambient context"), FAngelscriptEngine::GetAmbientWorldContext() == PreviousContext)) { return; }

			ScriptFunction->RuntimeCallEvent(ScriptClass->GetDefaultObject(), &Params);

			TestRunner->TestEqual(TEXT("RuntimeCallEvent should route the explicit WorldContextObject through ambient world-context lookup"), Params.ReturnValue, 7);
			TestRunner->TestTrue(TEXT("RuntimeCallEvent should restore the previous ambient context before leaving the outer scope"), FAngelscriptEngine::GetAmbientWorldContext() == PreviousContext);
		}

		TestRunner->TestTrue(TEXT("World-context runtime call should restore the ambient context after the scoped override exits"), FAngelscriptEngine::GetAmbientWorldContext() == AmbientBeforeScope);
		}
	}
};

#endif
