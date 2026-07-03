#include "CQTest.h"
#include "AngelscriptTestEngineHelper.h"
#include "AngelscriptTestExecute.h"
#include "AngelscriptTestMacros.h"

#include "Misc/ScopeExit.h"
#include "UObject/Class.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UnrealType.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FAngelscriptBlueprintTypeBindingsTest,
	"Angelscript.TestModule.Bindings.BlueprintType",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
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

	TEST_METHOD(BlueprintEventAndUStructContractSmoke)
	{
		static const FName ModuleName(TEXT("ASBindings_BlueprintTypeContract"));
		static const FString ScriptFilename(TEXT("ASBindings_BlueprintTypeContract.as"));

		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		const FString ScriptSource = ASTEST_AS(R"AS(
			USTRUCT(BlueprintType)
			struct FBindingBlueprintTypePayload
			{
				UPROPERTY()
				int Value = 7;
			}

			UCLASS(BlueprintType)
			class UBindingBlueprintTypeCarrier : UObject
			{
				UPROPERTY()
				FBindingBlueprintTypePayload Stored;

				UFUNCTION(BlueprintCallable)
				int ReadStoredValue(FBindingBlueprintTypePayload Payload)
				{
					return Payload.Value;
				}

				UFUNCTION(BlueprintEvent)
				int ComputeValue(FBindingBlueprintTypePayload Payload)
				{
					return Payload.Value + 1;
				}

				UFUNCTION()
				int RunContract()
				{
					FBindingBlueprintTypePayload Payload;
					Payload.Value = 41;
					Stored = Payload;
					return ComputeValue(Stored);
				}
			}

			int RunBlueprintTypeContract()
			{
				UBindingBlueprintTypeCarrier Carrier = UBindingBlueprintTypeCarrier();
				return Carrier.RunContract();
			}
			)AS");

		ECompileResult CompileResult = ECompileResult::Error;
		const bool bCompiled = CompileModuleWithResult(
			&Engine,
			ECompileType::FullReload,
			ModuleName,
			ScriptFilename,
			ScriptSource,
			CompileResult);
		ASSERT_THAT(IsTrue(bCompiled, TEXT("Blueprint type contract fixture should compile")));
		ASSERT_THAT(AreEqual(
			ECompileResult::FullyHandled,
			CompileResult,
			TEXT("Blueprint type contract fixture should finish with a fully handled compile result")));
		if (!bCompiled)
		{
			return;
		}

		int32 RuntimeResult = 0;
		ASSERT_THAT(IsTrue(
			ExecuteIntFunction(&Engine, ScriptFilename, ModuleName, TEXT("int RunBlueprintTypeContract()"), RuntimeResult),
			TEXT("Blueprint type contract fixture should execute through the generated BlueprintEvent wrapper")));
		ASSERT_THAT(AreEqual(
			42,
			RuntimeResult,
			TEXT("BlueprintEvent wrapper should dispatch to the script implementation with AS USTRUCT arguments")));

		UClass* GeneratedClass = FindGeneratedClass(&Engine, TEXT("UBindingBlueprintTypeCarrier"));
		ASSERT_THAT(IsNotNull(GeneratedClass, TEXT("BlueprintType AS class should materialize a generated UClass")));
		if (GeneratedClass == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsTrue(
			GeneratedClass->HasMetaData(TEXT("BlueprintType")),
			TEXT("UCLASS(BlueprintType) should preserve BlueprintType metadata")));

		UFunction* ReadStoredValueFunction = FindGeneratedFunction(GeneratedClass, TEXT("ReadStoredValue"));
		UFunction* ComputeValueFunction = FindGeneratedFunction(GeneratedClass, TEXT("ComputeValue"));
		ASSERT_THAT(IsNotNull(ReadStoredValueFunction, TEXT("BlueprintCallable function should materialize as a UFunction")));
		ASSERT_THAT(IsNotNull(ComputeValueFunction, TEXT("BlueprintEvent function should materialize as a UFunction")));
		if (ReadStoredValueFunction == nullptr || ComputeValueFunction == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsTrue(
			ReadStoredValueFunction->HasAnyFunctionFlags(FUNC_BlueprintCallable),
			TEXT("UFUNCTION(BlueprintCallable) should preserve FUNC_BlueprintCallable")));
		ASSERT_THAT(IsTrue(
			ComputeValueFunction->HasAnyFunctionFlags(FUNC_BlueprintEvent),
			TEXT("UFUNCTION(BlueprintEvent) should preserve FUNC_BlueprintEvent")));

		FStructProperty* StoredProperty = FindFProperty<FStructProperty>(GeneratedClass, TEXT("Stored"));
		ASSERT_THAT(IsNotNull(StoredProperty, TEXT("AS USTRUCT property should materialize as an FStructProperty")));
		if (StoredProperty == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsNotNull(StoredProperty->Struct, TEXT("AS USTRUCT property should point at a generated UScriptStruct")));
		if (StoredProperty->Struct != nullptr)
		{
			ASSERT_THAT(IsNotNull(
				FindFProperty<FIntProperty>(StoredProperty->Struct, TEXT("Value")),
				TEXT("Generated UScriptStruct should preserve its reflected Value field")));
		}
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
