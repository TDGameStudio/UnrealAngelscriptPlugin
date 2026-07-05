#include "AngelscriptFunctionalTestUtils.h"
#include "AngelscriptTestMacros.h"

#include "CQTest.h"
#include "ClassGenerator/ASClass.h"
#include "Core/AngelscriptUhtCoverageTestTypes.h"
#include "Misc/ScopeExit.h"
#include "UObject/UnrealType.h"

// Test Layer: Runtime Integration
#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FAngelscriptASFunctionWorldContextTests,
	"Angelscript.TestModule.ClassGenerator.ASFunction",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	inline static const FName WorldContextModuleName = FName(TEXT("ASFunctionWorldContext"));
	inline static const FString WorldContextFilename = FString(TEXT("ASFunctionWorldContext.as"));
	inline static const FName WorldContextStaticsClassName = FName(TEXT("UModule_ASFunctionWorldContextStatics"));

	struct FCheckWorldContextParams
	{
		AActor* WorldContextObject = nullptr;
		int32 Value = 0;
		int32 ReturnValue = 0;
	};

public:
	BEFORE_ALL()
	{
		ASTEST_CREATE_ENGINE();
	}

	AFTER_ALL()
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		ASTEST_RESET_ENGINE(Engine);
	}

	TEST_METHOD(StaticWorldContextRuntimeCallUsesValidParmOffset)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope EngineScope(Engine);
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*WorldContextModuleName.ToString());
		};

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor& ContextActor = Spawner.SpawnActor<AActor>();
		AActor& PreviousContextActor = Spawner.SpawnActor<AActor>();
		UObject* PreviousContext = &PreviousContextActor;
		ASSERT_THAT(IsNotNull(&ContextActor, TEXT("World-context function test should create a test case actor")));
		ASSERT_THAT(IsNotNull(PreviousContext, TEXT("World-context function test should create a previous ambient context")));
		if (&ContextActor == nullptr || PreviousContext == nullptr)
		{ return; }

		const FString ScriptSource = ASTEST_AS(R"AS(
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
		ASSERT_THAT(IsTrue(bCompiled, TEXT("World-context function test should compile the annotated static callable module")));
		if (!bCompiled) { return; }

		UClass* ScriptClass = FindGeneratedClass(&Engine, WorldContextStaticsClassName);
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("World-context function test should generate the module statics class")));
		if (ScriptClass == nullptr) { return; }

		UFunction* GeneratedFunction = FindGeneratedFunction(ScriptClass, TEXT("CheckWorldContext"));
		UASFunction* ScriptFunction = Cast<UASFunction>(GeneratedFunction);
		FObjectProperty* WorldContextProperty = FindFProperty<FObjectProperty>(GeneratedFunction, TEXT("WorldContextObject"));
		FIntProperty* ValueProperty = FindFProperty<FIntProperty>(GeneratedFunction, TEXT("Value"));
		FIntProperty* ReturnProperty = FindFProperty<FIntProperty>(GeneratedFunction, TEXT("ReturnValue"));
		ASSERT_THAT(IsNotNull(GeneratedFunction, TEXT("World-context function test should expose the generated static function")));
		ASSERT_THAT(IsNotNull(ScriptFunction, TEXT("World-context function test should generate a UASFunction")));
		ASSERT_THAT(IsNotNull(WorldContextProperty, TEXT("World-context function test should expose the WorldContextObject property")));
		ASSERT_THAT(IsNotNull(ValueProperty, TEXT("World-context function test should expose the Value property")));
		ASSERT_THAT(IsNotNull(ReturnProperty, TEXT("World-context function test should expose the ReturnValue property")));
		if (GeneratedFunction == nullptr || ScriptFunction == nullptr || WorldContextProperty == nullptr || ValueProperty == nullptr || ReturnProperty == nullptr)
		{ return; }

		ASSERT_THAT(IsTrue(GeneratedFunction->HasAnyFunctionFlags(FUNC_Static), TEXT("World-context function test should compile the callable as a static UFunction")));
		ASSERT_THAT(IsFalse(ScriptFunction->bIsWorldContextGenerated, TEXT("Explicit WorldContext metadata should not generate an extra synthetic parameter")));
		ASSERT_THAT(AreEqual(0, ScriptFunction->WorldContextIndex, TEXT("WorldContext parameter should stay at argument index 0")));
		ASSERT_THAT(IsTrue(ScriptFunction->WorldContextOffsetInParms >= 0, TEXT("WorldContext parameter should record a valid parameter offset")));
		ASSERT_THAT(AreEqual(ScriptFunction->WorldContextOffsetInParms, WorldContextProperty->GetOffset_ForUFunction(), TEXT("WorldContext parameter offset should match the reflected property layout")));
		ASSERT_THAT(IsTrue(IsAngelscriptGenerated(WorldContextProperty), TEXT("Generated function parameters should be reported as Angelscript-generated properties")));
		ASSERT_THAT(IsTrue(IsAngelscriptGenerated(ValueProperty), TEXT("Generated function ordinary parameters should be reported as Angelscript-generated properties")));
		ASSERT_THAT(IsTrue(IsAngelscriptGenerated(ReturnProperty), TEXT("Generated function return parameters should be reported as Angelscript-generated properties")));
		ASSERT_THAT(IsTrue(IsAngelscriptWorldContextProperty(WorldContextProperty), TEXT("WorldContextObject parameter should be reported as an Angelscript world-context property")));
		ASSERT_THAT(IsFalse(IsAngelscriptWorldContextProperty(ValueProperty), TEXT("Ordinary generated parameters should not be reported as world-context properties")));
		ASSERT_THAT(IsFalse(IsAngelscriptWorldContextProperty(ReturnProperty), TEXT("Generated return parameters should not be reported as world-context properties")));

		UFunction* NativeFunction = UAngelscriptUhtCoverageTestLibrary::StaticClass()->FindFunctionByName(TEXT("RequiresWorldContext"));
		FIntProperty* NativeValueProperty = NativeFunction != nullptr ? FindFProperty<FIntProperty>(NativeFunction, TEXT("Value")) : nullptr;
		ASSERT_THAT(IsNotNull(NativeFunction, TEXT("World-context function test should find a native comparison function")));
		ASSERT_THAT(IsNotNull(NativeValueProperty, TEXT("World-context function test should find a native comparison property")));
		if (NativeFunction == nullptr || NativeValueProperty == nullptr)
		{ return; }
		ASSERT_THAT(IsFalse(IsAngelscriptGenerated(NativeValueProperty), TEXT("Native UFunction parameters should not be reported as Angelscript-generated properties")));
		ASSERT_THAT(IsFalse(IsAngelscriptWorldContextProperty(NativeValueProperty), TEXT("Native UFunction parameters should not be reported as Angelscript world-context properties")));

		FCheckWorldContextParams Params;
		Params.WorldContextObject = &ContextActor;
		Params.Value = 7;

		UObject* AmbientBeforeScope = FAngelscriptEngine::GetAmbientWorldContext();
		{
			FScopedTestWorldContextScope PreviousContextScope(PreviousContext);
			ASSERT_THAT(IsTrue(FAngelscriptEngine::GetAmbientWorldContext() == PreviousContext, TEXT("Outer test scope should install the previous ambient context")));
			if (FAngelscriptEngine::GetAmbientWorldContext() != PreviousContext) { return; }

			ScriptFunction->RuntimeCallEvent(ScriptClass->GetDefaultObject(), &Params);

			ASSERT_THAT(AreEqual(7, Params.ReturnValue, TEXT("RuntimeCallEvent should route the explicit WorldContextObject through ambient world-context lookup")));
			ASSERT_THAT(IsTrue(FAngelscriptEngine::GetAmbientWorldContext() == PreviousContext, TEXT("RuntimeCallEvent should restore the previous ambient context before leaving the outer scope")));
		}

		ASSERT_THAT(IsTrue(FAngelscriptEngine::GetAmbientWorldContext() == AmbientBeforeScope, TEXT("World-context runtime call should restore the ambient context after the scoped override exits")));
	}
};

#endif
