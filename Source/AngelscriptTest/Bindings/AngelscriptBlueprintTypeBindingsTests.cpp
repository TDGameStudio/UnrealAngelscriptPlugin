#include "CQTest.h"
#include "AngelscriptTestEngineHelper.h"
#include "AngelscriptTestExecute.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestModuleScope.h"

#include "Misc/ScopeExit.h"
#include "UObject/Class.h"
#include "UObject/GarbageCollection.h"
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

	TEST_METHOD(ObjectTemplateMethodSurfaceExecutes)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		const FString ScriptSource = ASTEST_AS(R"AS(
			int RunObjectTemplateMethodSurface()
			{
				TSubclassOf<UObject> ClassValue;
				ClassValue.Set(UTexture2D::StaticClass());
				if (ClassValue.Get() != UTexture2D::StaticClass())
				{
					return 1;
				}

				TWeakObjectPtr<UObject> EmptyWeak;
				if (!EmptyWeak.IsExplicitlyNull() || EmptyWeak.IsStale())
				{
					return 2;
				}

				UObject StrongObject = NewObject(GetTransientPackage(), UTexture2D::StaticClass());
				TObjectPtr<UObject> ObjectPtr = StrongObject;
				TObjectPtr<UObject> CopiedObjectPtr = ObjectPtr;
				if (CopiedObjectPtr.Get() != StrongObject
					|| CopiedObjectPtr != ObjectPtr
					|| CopiedObjectPtr != StrongObject)
				{
					return 3;
				}

				CopiedObjectPtr = nullptr;
				if (CopiedObjectPtr.Get() != nullptr)
				{
					return 4;
				}

				TWeakObjectPtr<UObject> LiveWeak = StrongObject;
				if (!LiveWeak.IsValid() || LiveWeak.IsStale() || LiveWeak.IsExplicitlyNull())
				{
					return 5;
				}

				return 0;
			}
			)AS");

		FScopedAngelscriptModule ModuleScope(
			*TestRunner,
			Engine,
			TEXT("ASBlueprintType_ObjectTemplateMethodSurface"),
			ScriptSource);
		ASSERT_THAT(IsTrue(
			ModuleScope.IsValid(),
			TEXT("Object template method-surface module should compile")));
		if (!ModuleScope.IsValid())
		{
			return;
		}

		ASSERT_THAT(IsTrue(
			ExpectGlobalInt(
				*TestRunner,
				Engine,
				ModuleScope.GetModule(),
				TEXT("int RunObjectTemplateMethodSurface()"),
				TEXT("TSubclassOf and TWeakObjectPtr method surfaces should execute"),
				0),
			TEXT("TSubclassOf.Set and weak-pointer state queries should execute through their bindings")));
	}

	TEST_METHOD(TObjectPtrRejectsValueTypeTemplateArgument)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		const FString ExpectedDiagnostics[] = {
			TEXT("Attempting to instantiate invalid template type 'TObjectPtr<FVector>'"),
		};
		const FString ScriptSource = ASTEST_AS(R"AS(
			void ConstructInvalidObjectPtr()
			{
				TObjectPtr<FVector> InvalidObjectPtr;
			}
			)AS");

		ASSERT_THAT(IsTrue(
			CompileAndExpectFailure(
				*TestRunner,
				Engine,
				TEXT("ASBlueprintType_TObjectPtrValueTypeRejection"),
				ScriptSource,
				TEXT("TObjectPtr should reject value-type template arguments"),
				MakeArrayView(ExpectedDiagnostics)),
			TEXT("TObjectPtr<FVector> should fail with the template callback diagnostic")));
	}

	TEST_METHOD(WeakObjectPtrDistinguishesLiveStaleAndExplicitNullStates)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		const FString ScriptSource = ASTEST_AS(R"AS(
			void CaptureWeakObject(TWeakObjectPtr<UObject>& ObservedWeak)
			{
				UObject TemporaryObject = NewObject(
					GetTransientPackage(),
					UTexture2D,
					n"BlueprintTypeWeakStateObject");
				ObservedWeak = TemporaryObject;
			}

			void ResetWeakObject(TWeakObjectPtr<UObject>& ObservedWeak)
			{
				ObservedWeak = nullptr;
			}

			int ObserveWeakState(const TWeakObjectPtr<UObject>& ObservedWeak)
			{
				int Result = ObservedWeak.IsValid() ? 1 : 0;
				Result |= ObservedWeak.IsStale() ? 2 : 0;
				Result |= ObservedWeak.IsExplicitlyNull() ? 4 : 0;
				return Result;
			}
			)AS");

		FScopedAngelscriptModule ModuleScope(
			*TestRunner,
			Engine,
			TEXT("ASBlueprintType_WeakObjectPtrStates"),
			ScriptSource);
		ASSERT_THAT(IsTrue(
			ModuleScope.IsValid(),
			TEXT("Weak-object-pointer state module should compile")));
		if (!ModuleScope.IsValid())
		{
			return;
		}

		TWeakObjectPtr<UObject> ObservedWeak;
		FASGlobalFunctionInvoker CaptureInvoker(
			*TestRunner,
			Engine,
			ModuleScope.GetModule(),
			TEXT("void CaptureWeakObject(TWeakObjectPtr<UObject>&)"));
		CaptureInvoker.AddArgRef(ObservedWeak);
		ASSERT_THAT(IsTrue(
			CaptureInvoker.Call(),
			TEXT("Weak-object-pointer fixture should capture an unrooted live object")));

		auto ObserveWeakState = [&]()
		{
			FASGlobalFunctionInvoker ObserveInvoker(
				*TestRunner,
				Engine,
				ModuleScope.GetModule(),
				TEXT("int ObserveWeakState(const TWeakObjectPtr<UObject>&)"));
			ObserveInvoker.AddArgRef(ObservedWeak);
			return ObserveInvoker.CallAndReturn<int32>(INDEX_NONE);
		};

		ASSERT_THAT(AreEqual(
			1,
			ObserveWeakState(),
			TEXT("Live weak-object-pointer state should be observable")));

		CollectGarbage(RF_NoFlags, true);
		ASSERT_THAT(AreEqual(
			2,
			ObserveWeakState(),
			TEXT("Stale weak-object-pointer state should remain distinguishable")));

		FASGlobalFunctionInvoker ResetInvoker(
			*TestRunner,
			Engine,
			ModuleScope.GetModule(),
			TEXT("void ResetWeakObject(TWeakObjectPtr<UObject>&)"));
		ResetInvoker.AddArgRef(ObservedWeak);
		ASSERT_THAT(IsTrue(
			ResetInvoker.Call(),
			TEXT("Weak-object-pointer fixture should reset the stale pointer")));
		ASSERT_THAT(AreEqual(
			4,
			ObserveWeakState(),
			TEXT("Explicit-null weak-object-pointer state should be observable")));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
