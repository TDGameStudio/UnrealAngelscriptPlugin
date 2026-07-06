#include "AngelscriptFunctionalTestUtils.h"
#include "AngelscriptTestMacros.h"

#include "CQTest.h"
#include "ClassGenerator/ASClass.h"
#include "Misc/ScopeExit.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UnrealType.h"

#include <limits>

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FAngelscriptASFunctionOptimizedCallTests,
	"Angelscript.TestModule.ClassGenerator.ASFunction",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	inline static const FName ModuleName = FName(TEXT("ASFunctionOptimizedCall"));
	inline static const FString ScriptFilename = FString(TEXT("ASFunctionOptimizedCall.as"));
	inline static const FName GeneratedClassName = FName(TEXT("UOptimizedCallTarget"));
	inline static const FName PingCountPropertyName = FName(TEXT("PingCount"));
	inline static const FName StoredFloatHundredthsPropertyName = FName(TEXT("StoredFloatHundredths"));
	inline static const FName StoredDoubleHundredthsPropertyName = FName(TEXT("StoredDoubleHundredths"));
	inline static const FName ObservedRefValuePropertyName = FName(TEXT("ObservedRefValue"));
	static const int32 FloatNaNMarker = -7001;
	static const int32 FloatPositiveInfinityMarker = 7002;
	static const int32 FloatNegativeInfinityMarker = -7002;
	static const int32 DoubleNaNMarker = -8001;
	static const int32 DoublePositiveInfinityMarker = 8002;
	static const int32 DoubleNegativeInfinityMarker = -8002;

	static UASClass* CompileOptimizedCallTarget(FAutomationTestBase& Test, FAngelscriptEngine& Engine)
	{
		const FString ScriptSource = ASTEST_AS(R"AS(
			UCLASS()
			class UOptimizedCallTarget : UObject
			{
				UPROPERTY()
				int PingCount = 0;

				UPROPERTY()
				int StoredFloatHundredths = 0;

				UPROPERTY()
				int StoredDoubleHundredths = 0;

				UPROPERTY()
				int ObservedRefValue = 0;

				UFUNCTION()
				void Ping()
				{
					PingCount += 1;
				}

				UFUNCTION()
				uint8 GetByteCode()
				{
					return 42;
				}

				UFUNCTION()
				void StoreFloat(float32 InValue)
				{
					if (Math::IsNaN(InValue))
					{
						StoredFloatHundredths = -7001;
						return;
					}
					if (!Math::IsFinite(InValue))
					{
						StoredFloatHundredths = InValue > 0.0f ? 7002 : -7002;
						return;
					}
					StoredFloatHundredths = int(InValue * 100.0f);
				}

				UFUNCTION()
				void StoreDouble(float64 InValue)
				{
					if (Math::IsNaN(InValue))
					{
						StoredDoubleHundredths = -8001;
						return;
					}
					if (!Math::IsFinite(InValue))
					{
						StoredDoubleHundredths = InValue > 0.0 ? 8002 : -8002;
						return;
					}
					StoredDoubleHundredths = int(InValue * 100.0);
				}

				UFUNCTION()
				void BumpRef(int& Value)
				{
					Value += 3;
					ObservedRefValue = Value;
				}

				UFUNCTION()
				uint8 BumpRefAndReturn(int& Value)
				{
					Value += 4;
					ObservedRefValue = Value;
					return 77;
				}

				UFUNCTION()
				uint8 ThrowByte()
				{
					Throw("ASFUNCTION_OPTIMIZED_THROW_MARKER");
					return 99;
				}
			}
			)AS");

		UClass* GeneratedClass = AngelscriptFunctionalTestUtils::CompileScriptModule(
			Test,
			Engine,
			ModuleName,
			ScriptFilename,
			ScriptSource,
			GeneratedClassName);
		if (GeneratedClass == nullptr)
		{
			return nullptr;
		}

		return Cast<UASClass>(GeneratedClass);
	}

	static UASFunction* RequireGeneratedScriptFunction(
		FAutomationTestBase& Test,
		UClass* ScriptClass,
		const TCHAR* FunctionName)
	{
		UASFunction* ScriptFunction = Cast<UASFunction>(::FindGeneratedFunction(ScriptClass, FunctionName));
		FNoDiscardAsserter LocalAssert(Test);
		if (!LocalAssert.IsNotNull(
				ScriptFunction,
				*FString::Printf(TEXT("Optimized-call test case should generate '%s' as a UASFunction"), FunctionName)))
		{
			return nullptr;
		}
		return ScriptFunction;
	}
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

	TEST_METHOD(OptimizedCallWrappersPreserveArgumentsAndReturnValues)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope EngineScope(Engine);

		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*FAngelscriptASFunctionOptimizedCallTests::ModuleName.ToString());
		};

		UASClass* ScriptClass = FAngelscriptASFunctionOptimizedCallTests::CompileOptimizedCallTarget(*TestRunner, Engine);
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Optimized-call test case should compile to a UASClass")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		UASFunction* PingFunction = FAngelscriptASFunctionOptimizedCallTests::RequireGeneratedScriptFunction(*TestRunner, ScriptClass, TEXT("Ping"));
		UASFunction* GetByteCodeFunction = FAngelscriptASFunctionOptimizedCallTests::RequireGeneratedScriptFunction(*TestRunner, ScriptClass, TEXT("GetByteCode"));
		UASFunction* StoreFloatFunction = FAngelscriptASFunctionOptimizedCallTests::RequireGeneratedScriptFunction(*TestRunner, ScriptClass, TEXT("StoreFloat"));
		UASFunction* StoreDoubleFunction = FAngelscriptASFunctionOptimizedCallTests::RequireGeneratedScriptFunction(*TestRunner, ScriptClass, TEXT("StoreDouble"));
		UASFunction* BumpRefFunction = FAngelscriptASFunctionOptimizedCallTests::RequireGeneratedScriptFunction(*TestRunner, ScriptClass, TEXT("BumpRef"));
		UASFunction* BumpRefAndReturnFunction = FAngelscriptASFunctionOptimizedCallTests::RequireGeneratedScriptFunction(*TestRunner, ScriptClass, TEXT("BumpRefAndReturn"));
		if (PingFunction == nullptr
			|| GetByteCodeFunction == nullptr
			|| StoreFloatFunction == nullptr
			|| StoreDoubleFunction == nullptr
			|| BumpRefFunction == nullptr
			|| BumpRefAndReturnFunction == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsTrue(
			PingFunction->GetClass() == UASFunction_NoParams::StaticClass()
				|| PingFunction->GetClass() == UASFunction_NoParams_JIT::StaticClass(),
			TEXT("Optimized-call test case should route Ping through the dedicated no-params dispatch class")));
		ASSERT_THAT(IsTrue(
			GetByteCodeFunction->GetClass() == UASFunction_ByteReturn::StaticClass()
				|| GetByteCodeFunction->GetClass() == UASFunction_ByteReturn_JIT::StaticClass(),
			TEXT("Optimized-call test case should route GetByteCode through the dedicated byte-return dispatch class")));
		ASSERT_THAT(IsTrue(
			StoreFloatFunction->GetClass() == UASFunction_FloatArg::StaticClass()
				|| StoreFloatFunction->GetClass() == UASFunction_FloatArg_JIT::StaticClass(),
			TEXT("Optimized-call test case should route StoreFloat through the dedicated float-argument dispatch class")));
		ASSERT_THAT(IsTrue(
			StoreDoubleFunction->GetClass() == UASFunction_DoubleArg::StaticClass()
				|| StoreDoubleFunction->GetClass() == UASFunction_DoubleArg_JIT::StaticClass(),
			TEXT("Optimized-call test case should route StoreDouble through the dedicated double-argument dispatch class")));
		ASSERT_THAT(IsTrue(
			BumpRefFunction->GetClass() == UASFunction_ReferenceArg::StaticClass()
				|| BumpRefFunction->GetClass() == UASFunction_ReferenceArg_JIT::StaticClass(),
			TEXT("Optimized-call test case should route BumpRef through the dedicated reference-argument dispatch class")));

		UObject* Instance = NewObject<UObject>(GetTransientPackage(), ScriptClass, TEXT("OptimizedCallTargetInstance"));
		ASSERT_THAT(IsNotNull(Instance, TEXT("Optimized-call test case should instantiate the generated UObject")));
		if (Instance == nullptr)
		{
			return;
		}

		PingFunction->OptimizedCall(Instance);

		int32 PingCount = INDEX_NONE;
		if (!AngelscriptFunctionalTestUtils::ReadPropertyValue<FIntProperty>(*TestRunner, Instance, FAngelscriptASFunctionOptimizedCallTests::PingCountPropertyName, PingCount)
			|| !this->Assert.AreEqual(1, PingCount, TEXT("OptimizedCall should execute a no-parameter void function exactly once")))
		{
			return;
		}

		const uint8 ByteResult = GetByteCodeFunction->OptimizedCall_ByteReturn(Instance);
		if (!this->Assert.AreEqual(42, static_cast<int32>(ByteResult), TEXT("OptimizedCall_ByteReturn should preserve the script byte return value")))
		{
			return;
		}

		StoreFloatFunction->OptimizedCall_FloatArg(Instance, 12.5f);

		int32 StoredFloatHundredths = INDEX_NONE;
		if (!AngelscriptFunctionalTestUtils::ReadPropertyValue<FIntProperty>(*TestRunner, Instance, FAngelscriptASFunctionOptimizedCallTests::StoredFloatHundredthsPropertyName, StoredFloatHundredths)
			|| !this->Assert.AreEqual(1250, StoredFloatHundredths, TEXT("OptimizedCall_FloatArg should pass the float argument without mangling its value")))
		{
			return;
		}

		StoreDoubleFunction->OptimizedCall_DoubleArg(Instance, 42.25);

		int32 StoredDoubleHundredths = INDEX_NONE;
		if (!AngelscriptFunctionalTestUtils::ReadPropertyValue<FIntProperty>(*TestRunner, Instance, FAngelscriptASFunctionOptimizedCallTests::StoredDoubleHundredthsPropertyName, StoredDoubleHundredths)
			|| !this->Assert.AreEqual(4225, StoredDoubleHundredths, TEXT("OptimizedCall_DoubleArg should pass the double argument without mangling its value")))
		{
			return;
		}

		int32 RefArgument = 5;
		BumpRefFunction->OptimizedCall_RefArg(Instance, &RefArgument);

		int32 ObservedRefValue = INDEX_NONE;
		if (!this->Assert.AreEqual(8, RefArgument, TEXT("OptimizedCall_RefArg should write through the referenced argument"))
			|| !AngelscriptFunctionalTestUtils::ReadPropertyValue<FIntProperty>(*TestRunner, Instance, FAngelscriptASFunctionOptimizedCallTests::ObservedRefValuePropertyName, ObservedRefValue)
			|| !this->Assert.AreEqual(8, ObservedRefValue, TEXT("OptimizedCall_RefArg should expose the mutated reference value inside script state")))
		{
			return;
		}

		int32 RefArgumentWithReturn = 9;
		const uint8 RefReturnValue = BumpRefAndReturnFunction->OptimizedCall_RefArg_ByteReturn(Instance, &RefArgumentWithReturn);
		if (!this->Assert.AreEqual(13, RefArgumentWithReturn, TEXT("OptimizedCall_RefArg_ByteReturn should write through the referenced argument"))
			|| !AngelscriptFunctionalTestUtils::ReadPropertyValue<FIntProperty>(*TestRunner, Instance, FAngelscriptASFunctionOptimizedCallTests::ObservedRefValuePropertyName, ObservedRefValue)
			|| !this->Assert.AreEqual(13, ObservedRefValue, TEXT("OptimizedCall_RefArg_ByteReturn should expose the mutated reference value inside script state"))
			|| !this->Assert.AreEqual(77, static_cast<int32>(RefReturnValue), TEXT("OptimizedCall_RefArg_ByteReturn should preserve the script byte return value")))
		{
			return;
		}

	}

	TEST_METHOD(OptimizedCallWrappersExposeFallbackAndSpecialValueBehavior)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope EngineScope(Engine);

		bool bModuleDiscarded = false;
		ON_SCOPE_EXIT
		{
			if (!bModuleDiscarded)
			{
				Engine.DiscardModule(*FAngelscriptASFunctionOptimizedCallTests::ModuleName.ToString());
			}
		};

		UASClass* ScriptClass = FAngelscriptASFunctionOptimizedCallTests::CompileOptimizedCallTarget(*TestRunner, Engine);
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Optimized-call fallback test case should compile to a UASClass")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		UASFunction* GetByteCodeFunction = FAngelscriptASFunctionOptimizedCallTests::RequireGeneratedScriptFunction(*TestRunner, ScriptClass, TEXT("GetByteCode"));
		UASFunction* StoreFloatFunction = FAngelscriptASFunctionOptimizedCallTests::RequireGeneratedScriptFunction(*TestRunner, ScriptClass, TEXT("StoreFloat"));
		UASFunction* StoreDoubleFunction = FAngelscriptASFunctionOptimizedCallTests::RequireGeneratedScriptFunction(*TestRunner, ScriptClass, TEXT("StoreDouble"));
		UASFunction* BumpRefFunction = FAngelscriptASFunctionOptimizedCallTests::RequireGeneratedScriptFunction(*TestRunner, ScriptClass, TEXT("BumpRef"));
		UASFunction* BumpRefAndReturnFunction = FAngelscriptASFunctionOptimizedCallTests::RequireGeneratedScriptFunction(*TestRunner, ScriptClass, TEXT("BumpRefAndReturn"));
		UASFunction* ThrowByteFunction = FAngelscriptASFunctionOptimizedCallTests::RequireGeneratedScriptFunction(*TestRunner, ScriptClass, TEXT("ThrowByte"));
		if (GetByteCodeFunction == nullptr
			|| StoreFloatFunction == nullptr
			|| StoreDoubleFunction == nullptr
			|| BumpRefFunction == nullptr
			|| BumpRefAndReturnFunction == nullptr
			|| ThrowByteFunction == nullptr)
		{
			return;
		}

		UObject* Instance = NewObject<UObject>(GetTransientPackage(), ScriptClass, TEXT("OptimizedCallFallbackTargetInstance"));
		ASSERT_THAT(IsNotNull(Instance, TEXT("Optimized-call fallback test case should instantiate the generated UObject")));
		if (Instance == nullptr)
		{
			return;
		}

		StoreFloatFunction->OptimizedCall_FloatArg(Instance, std::numeric_limits<float>::quiet_NaN());
		int32 StoredFloatHundredths = INDEX_NONE;
		if (!AngelscriptFunctionalTestUtils::ReadPropertyValue<FIntProperty>(*TestRunner, Instance, FAngelscriptASFunctionOptimizedCallTests::StoredFloatHundredthsPropertyName, StoredFloatHundredths)
			|| !this->Assert.AreEqual(FAngelscriptASFunctionOptimizedCallTests::FloatNaNMarker, StoredFloatHundredths, TEXT("OptimizedCall_FloatArg should preserve a script-observable NaN classification")))
		{
			return;
		}

		StoreFloatFunction->OptimizedCall_FloatArg(Instance, std::numeric_limits<float>::infinity());
		if (!AngelscriptFunctionalTestUtils::ReadPropertyValue<FIntProperty>(*TestRunner, Instance, FAngelscriptASFunctionOptimizedCallTests::StoredFloatHundredthsPropertyName, StoredFloatHundredths)
			|| !this->Assert.AreEqual(FAngelscriptASFunctionOptimizedCallTests::FloatPositiveInfinityMarker, StoredFloatHundredths, TEXT("OptimizedCall_FloatArg should preserve a script-observable positive infinity classification")))
		{
			return;
		}

		StoreFloatFunction->OptimizedCall_FloatArg(Instance, -std::numeric_limits<float>::infinity());
		if (!AngelscriptFunctionalTestUtils::ReadPropertyValue<FIntProperty>(*TestRunner, Instance, FAngelscriptASFunctionOptimizedCallTests::StoredFloatHundredthsPropertyName, StoredFloatHundredths)
			|| !this->Assert.AreEqual(FAngelscriptASFunctionOptimizedCallTests::FloatNegativeInfinityMarker, StoredFloatHundredths, TEXT("OptimizedCall_FloatArg should preserve a script-observable negative infinity classification")))
		{
			return;
		}

		StoreDoubleFunction->OptimizedCall_DoubleArg(Instance, std::numeric_limits<double>::quiet_NaN());
		int32 StoredDoubleHundredths = INDEX_NONE;
		if (!AngelscriptFunctionalTestUtils::ReadPropertyValue<FIntProperty>(*TestRunner, Instance, FAngelscriptASFunctionOptimizedCallTests::StoredDoubleHundredthsPropertyName, StoredDoubleHundredths)
			|| !this->Assert.AreEqual(FAngelscriptASFunctionOptimizedCallTests::DoubleNaNMarker, StoredDoubleHundredths, TEXT("OptimizedCall_DoubleArg should preserve a script-observable NaN classification")))
		{
			return;
		}

		StoreDoubleFunction->OptimizedCall_DoubleArg(Instance, std::numeric_limits<double>::infinity());
		if (!AngelscriptFunctionalTestUtils::ReadPropertyValue<FIntProperty>(*TestRunner, Instance, FAngelscriptASFunctionOptimizedCallTests::StoredDoubleHundredthsPropertyName, StoredDoubleHundredths)
			|| !this->Assert.AreEqual(FAngelscriptASFunctionOptimizedCallTests::DoublePositiveInfinityMarker, StoredDoubleHundredths, TEXT("OptimizedCall_DoubleArg should preserve a script-observable positive infinity classification")))
		{
			return;
		}

		StoreDoubleFunction->OptimizedCall_DoubleArg(Instance, -std::numeric_limits<double>::infinity());
		if (!AngelscriptFunctionalTestUtils::ReadPropertyValue<FIntProperty>(*TestRunner, Instance, FAngelscriptASFunctionOptimizedCallTests::StoredDoubleHundredthsPropertyName, StoredDoubleHundredths)
			|| !this->Assert.AreEqual(FAngelscriptASFunctionOptimizedCallTests::DoubleNegativeInfinityMarker, StoredDoubleHundredths, TEXT("OptimizedCall_DoubleArg should preserve a script-observable negative infinity classification")))
		{
			return;
		}

		TestRunner->AddExpectedErrorPlain(TEXT("ASFUNCTION_OPTIMIZED_THROW_MARKER"), EAutomationExpectedErrorFlags::Contains, 1);
		TestRunner->AddExpectedErrorPlain(TEXT("ASFunctionOptimizedCall"), EAutomationExpectedErrorFlags::Contains, 1);
		TestRunner->AddExpectedError(TEXT("uint8 UOptimizedCallTarget::ThrowByte()"), EAutomationExpectedErrorFlags::Contains, 1, false);

		const uint8 ThrowResult = ThrowByteFunction->OptimizedCall_ByteReturn(Instance);
		if (!this->Assert.AreEqual(0, static_cast<int32>(ThrowResult), TEXT("OptimizedCall_ByteReturn should return the fallback byte value after a script exception")))
		{
			return;
		}

		const uint8 ByteResultBeforeDiscard = GetByteCodeFunction->OptimizedCall_ByteReturn(Instance);
		if (!this->Assert.AreEqual(42, static_cast<int32>(ByteResultBeforeDiscard), TEXT("OptimizedCall_ByteReturn should return script value before discard")))
		{
			return;
		}

		Engine.DiscardModule(*FAngelscriptASFunctionOptimizedCallTests::ModuleName.ToString());
		bModuleDiscarded = true;

		int32 RefArgument = 41;
		BumpRefFunction->OptimizedCall_RefArg(Instance, &RefArgument);
		if (!this->Assert.AreEqual(41, RefArgument, TEXT("OptimizedCall_RefArg should leave referenced values untouched after discard")))
		{
			return;
		}

		int32 RefArgumentWithReturn = 51;
		const uint8 RefReturnAfterDiscard = BumpRefAndReturnFunction->OptimizedCall_RefArg_ByteReturn(Instance, &RefArgumentWithReturn);
		if (!this->Assert.AreEqual(51, RefArgumentWithReturn, TEXT("OptimizedCall_RefArg_ByteReturn should leave referenced values untouched after discard"))
			|| !this->Assert.AreEqual(0, static_cast<int32>(RefReturnAfterDiscard), TEXT("OptimizedCall_RefArg_ByteReturn should return fallback byte value after discard")))
		{
			return;
		}

		const uint8 ByteResultAfterDiscard = GetByteCodeFunction->OptimizedCall_ByteReturn(Instance);
		ASSERT_THAT(AreEqual(0, static_cast<int32>(ByteResultAfterDiscard), TEXT("OptimizedCall_ByteReturn should return fallback byte value after discard")));

	}
};

#endif
