#include "AngelscriptFunctionalTestUtils.h"

// Debugger value ownership coverage.
#include "AngelscriptTestMacros.h"
#include "../../AngelscriptRuntime/Core/AngelscriptSettings.h"
#include "../../AngelscriptRuntime/Core/AngelscriptType.h"
#include "ClassGenerator/ASClass.h"

#include "Containers/StringConv.h"
#include "CQTest.h"
#include "Misc/ScopeExit.h"
#include "UObject/GarbageCollection.h"
#include "UObject/UObjectGlobals.h"

#if WITH_ANGELSCRIPT_UNITTESTS

using namespace AngelscriptFunctionalTestUtils;



TEST_CLASS_WITH_FLAGS(FDebuggerValueTests,
	"Angelscript.TestModule.Debugger.Value",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	static asITypeInfo* FindScriptTypeInfoForClass(
		FAutomationTestBase& Test,
		FAngelscriptEngine& Engine,
		UClass* ScriptClass)
	{
		const FString BoundTypeName = FAngelscriptType::GetBoundClassName(ScriptClass);
		asITypeInfo* ScriptType = nullptr;
		if (const UASClass* ScriptASClass = Cast<UASClass>(ScriptClass))
		{
			ScriptType = static_cast<asITypeInfo*>(ScriptASClass->ScriptTypePtr);
		}

		if (ScriptType == nullptr)
		{
			const FTCHARToUTF8 BoundTypeNameUtf8(*BoundTypeName);
			ScriptType = Engine.GetScriptEngine()->GetTypeInfoByName(BoundTypeNameUtf8.Get());
		}

		FNoDiscardAsserter LocalAssert(Test);
		(void)LocalAssert.IsNotNull(
			ScriptType,
			*FString::Printf(TEXT("Debugger value getter tracking should resolve script type '%s'"), *BoundTypeName));
		return ScriptType;
	}

	static asIScriptFunction* FindMethodByDecl(
		FAutomationTestBase& Test,
		asITypeInfo& ScriptType,
		const FString& Declaration)
	{
		const FTCHARToUTF8 DeclarationUtf8(*Declaration);
		asIScriptFunction* Function = ScriptType.GetMethodByDecl(DeclarationUtf8.Get());
		if (Function == nullptr)
		{
			FString FunctionName;
			int32 OpenParenIndex = INDEX_NONE;
			if (Declaration.FindChar(TEXT('('), OpenParenIndex))
			{
				const FString Prefix = Declaration.Left(OpenParenIndex).TrimStartAndEnd();
				int32 NameSeparatorIndex = INDEX_NONE;
				if (Prefix.FindLastChar(TEXT(' '), NameSeparatorIndex))
				{
					FunctionName = Prefix.Mid(NameSeparatorIndex + 1).TrimStartAndEnd();
				}
			}

			if (!FunctionName.IsEmpty())
			{
				const FTCHARToUTF8 FunctionNameUtf8(*FunctionName);
				Function = ScriptType.GetMethodByName(FunctionNameUtf8.Get());
			}
		}

		FNoDiscardAsserter LocalAssert(Test);
		(void)LocalAssert.IsNotNull(
			Function,
			*FString::Printf(TEXT("Debugger value getter tracking should resolve method '%s'"), *Declaration));
		return Function;
	}

	static bool ExpectTrackedDebuggerValue(
		FAutomationTestBase& Test,
		const TCHAR* Context,
		const FDebuggerValue& DebugValue,
		const FString& ExpectedValue,
		void* ExpectedAddress)
	{
		FNoDiscardAsserter LocalAssert(Test);
		bool bOk = true;
		bOk &= LocalAssert.AreEqual(
			ExpectedValue,
			DebugValue.Value,
			*FString::Printf(TEXT("%s should stringify the getter result"), Context));
		bOk &= LocalAssert.IsTrue(
			DebugValue.bTemporaryValue,
			*FString::Printf(TEXT("%s should mark debugger output as a temporary value"), Context));
		bOk &= LocalAssert.IsTrue(
			DebugValue.GetAddressToMonitor() == ExpectedAddress,
			*FString::Printf(TEXT("%s should bind the monitored address back to the Health property"), Context));
		bOk &= LocalAssert.IsTrue(
			DebugValue.NonTemporaryAddress == ExpectedAddress || DebugValue.AddressToMonitor == ExpectedAddress,
			*FString::Printf(TEXT("%s should preserve a concrete non-temporary or monitor address"), Context));
		bOk &= LocalAssert.AreEqual(
			static_cast<int32>(sizeof(int32)),
			DebugValue.GetAddressToMonitorValueSize(),
			*FString::Printf(TEXT("%s should report the expected monitor value size"), Context));
		return bOk;
	}

	static FString BuildDebuggerFunctionPath(const asIScriptFunction& ScriptFunction)
	{
		FString FunctionPath;
		if (ScriptFunction.GetObjectType() != nullptr)
		{
			FunctionPath = ANSI_TO_TCHAR(ScriptFunction.GetObjectType()->GetName());
			FunctionPath += TEXT(".");
		}

		FunctionPath += ANSI_TO_TCHAR(ScriptFunction.GetName());
		return FunctionPath;
	}

public:
	TEST_METHOD(GetterPropertyTracking)
	{
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine);
		static const FName ModuleName(TEXT("InternalsDebuggerValueGetterTracking"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
			ASTEST_RESET_ENGINE(Engine);
			CollectGarbage(RF_NoFlags, true);
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("InternalsDebuggerValueGetterTracking.as"),
			TEXT(R"AS(
UCLASS()
class ADebuggerValueGetterProbe : AActor
{
	UPROPERTY()
	int Health = 42;

	UFUNCTION()
	int GetHealth() const
	{
		return Health;
	}
}
)AS"),
			TEXT("ADebuggerValueGetterProbe"));

		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();

		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		asITypeInfo* ScriptType = FindScriptTypeInfoForClass(*TestRunner, Engine, ScriptClass);
		asIScriptFunction* GetterFunction = ScriptType != nullptr
			? FindMethodByDecl(*TestRunner, *ScriptType, TEXT("int GetHealth() const"))
			: nullptr;
		FIntProperty* HealthProperty = FindFProperty<FIntProperty>(ScriptClass, TEXT("Health"));

		ASSERT_THAT(IsNotNull(Actor, TEXT("Debugger value getter tracking should spawn the script actor")));
		ASSERT_THAT(IsNotNull(HealthProperty, TEXT("Debugger value getter tracking should expose the native Health property")));
		ASSERT_THAT(IsNotNull(Actor->GetWorld(), TEXT("Debugger value getter tracking should keep the spawned actor inside a world")));
		ASSERT_THAT(IsNotNull(GetterFunction, TEXT("Debugger value getter tracking should resolve the script getter method")));

		void* const HealthAddress = HealthProperty->ContainerPtrToValuePtr<void>(Actor);
		int32* const HealthValue = static_cast<int32*>(HealthAddress);
		ASSERT_THAT(IsNotNull(HealthValue, TEXT("Debugger value getter tracking should expose reflected Health storage")));

		ASSERT_THAT(AreEqual(42, *HealthValue, TEXT("Debugger value getter tracking should start from the default Health value")));

		FDebuggerValue FirstValue;
		const bool bFirstResolved = FAngelscriptType::GetDebuggerValueFromFunction(
			GetterFunction,
			Actor,
			FirstValue,
			ScriptType,
			ScriptClass,
			TEXT("Health"));
		ASSERT_THAT(IsTrue(bFirstResolved, TEXT("Debugger value getter tracking should evaluate the getter once before mutation")));
		if (bFirstResolved)
		{
			ExpectTrackedDebuggerValue(
				*TestRunner,
				TEXT("Debugger value getter tracking first evaluation"),
				FirstValue,
				TEXT("42"),
				HealthAddress);
		}

		*HealthValue = 99;
		ASSERT_THAT(AreEqual(99, *HealthValue, TEXT("Debugger value getter tracking should mutate the reflected Health storage in place")));

		FDebuggerValue SecondValue;
		const bool bSecondResolved = FAngelscriptType::GetDebuggerValueFromFunction(
			GetterFunction,
			Actor,
			SecondValue,
			ScriptType,
			ScriptClass,
			TEXT("Health"));
		ASSERT_THAT(IsTrue(bSecondResolved, TEXT("Debugger value getter tracking should evaluate the getter again after mutation")));
		if (bSecondResolved)
		{
			ExpectTrackedDebuggerValue(
				*TestRunner,
				TEXT("Debugger value getter tracking second evaluation"),
				SecondValue,
				TEXT("99"),
				HealthAddress);
		}

		}
	}

	TEST_METHOD(FunctionEvaluationGuards)
	{
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine);
		static const FName ModuleName(TEXT("InternalsDebuggerValueFunctionEvaluationGuards"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
			ASTEST_RESET_ENGINE(Engine);
			CollectGarbage(RF_NoFlags, true);
		};

		UAngelscriptSettings& Settings = UAngelscriptSettings::Get();
		const TSet<FString> SavedBlacklist = Settings.DebuggerBlacklistAutomaticFunctionEvaluation;
		const TSet<FString> SavedWithoutWorldBlacklist = Settings.DebuggerBlacklistAutomaticFunctionEvaluationWithoutWorldContext;
		ON_SCOPE_EXIT
		{
			Settings.DebuggerBlacklistAutomaticFunctionEvaluation = SavedBlacklist;
			Settings.DebuggerBlacklistAutomaticFunctionEvaluationWithoutWorldContext = SavedWithoutWorldBlacklist;
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("InternalsDebuggerValueFunctionEvaluationGuards.as"),
			TEXT(R"AS(
UCLASS()
class UDebuggerValueGuardProbe : UObject
{
	UPROPERTY()
	int EvalCount = 0;

	UFUNCTION()
	int GetValue()
	{
		EvalCount += 1;
		return 42;
	}

	UFUNCTION()
	int NeedsArg(int Value)
	{
		EvalCount += 100;
		return Value;
	}
}
)AS"),
			TEXT("UDebuggerValueGuardProbe"));

		if (ScriptClass == nullptr)
		{
			return;
		}

		asITypeInfo* ScriptType = FindScriptTypeInfoForClass(*TestRunner, Engine, ScriptClass);
		asIScriptFunction* GetterFunction = ScriptType != nullptr
			? FindMethodByDecl(*TestRunner, *ScriptType, TEXT("int GetValue()"))
			: nullptr;
		asIScriptFunction* NeedsArgFunction = ScriptType != nullptr
			? FindMethodByDecl(*TestRunner, *ScriptType, TEXT("int NeedsArg(int)"))
			: nullptr;
		FIntProperty* EvalCountProperty = FindFProperty<FIntProperty>(ScriptClass, TEXT("EvalCount"));
		UObject* Target = NewObject<UObject>(GetTransientPackage(), ScriptClass, TEXT("DebuggerValueGuardTarget"));

		ASSERT_THAT(IsNotNull(EvalCountProperty, TEXT("Debugger value guard test should expose the EvalCount property")));
		ASSERT_THAT(IsNotNull(Target, TEXT("Debugger value guard test should instantiate the generated UObject")));
		ASSERT_THAT(IsNotNull(GetterFunction, TEXT("Debugger value guard test should resolve the generated getter method")));
		ASSERT_THAT(IsNotNull(NeedsArgFunction, TEXT("Debugger value guard test should resolve the generated NeedsArg method")));

		ASSERT_THAT(IsTrue(Target->GetWorld() == nullptr, TEXT("Debugger value guard test should keep the generated UObject worldless so the without-world blacklist path is reachable")));

		int32* const EvalCountPtr = EvalCountProperty->ContainerPtrToValuePtr<int32>(Target);
		ASSERT_THAT(IsNotNull(EvalCountPtr, TEXT("Debugger value guard test should expose reflected EvalCount storage")));

		Settings.DebuggerBlacklistAutomaticFunctionEvaluation.Reset();
		Settings.DebuggerBlacklistAutomaticFunctionEvaluationWithoutWorldContext.Reset();
		Settings.DebuggerBlacklistAutomaticFunctionEvaluationWithoutWorldContext.Add(
			BuildDebuggerFunctionPath(*GetterFunction));

		FDebuggerValue WithoutWorldValue;
		ASSERT_THAT(IsFalse(
			FAngelscriptType::GetDebuggerValueFromFunction(
				GetterFunction,
				Target,
				WithoutWorldValue,
				ScriptType,
				ScriptClass),
			TEXT("Debugger value guard test should reject a getter blacklisted for objects without world context")));
		ASSERT_THAT(AreEqual(0, *EvalCountPtr, TEXT("Debugger value guard test should not execute the getter when the without-world blacklist matches")));

		Settings.DebuggerBlacklistAutomaticFunctionEvaluationWithoutWorldContext.Reset();
		Settings.DebuggerBlacklistAutomaticFunctionEvaluation.Add(
			BuildDebuggerFunctionPath(*GetterFunction));

		FDebuggerValue UnconditionalValue;
		ASSERT_THAT(IsFalse(
			FAngelscriptType::GetDebuggerValueFromFunction(
				GetterFunction,
				Target,
				UnconditionalValue,
				ScriptType,
				ScriptClass),
			TEXT("Debugger value guard test should reject a getter blacklisted for all debugger evaluation")));
		ASSERT_THAT(AreEqual(0, *EvalCountPtr, TEXT("Debugger value guard test should still leave EvalCount untouched after the unconditional blacklist guard")));

		Settings.DebuggerBlacklistAutomaticFunctionEvaluation.Reset();

		FDebuggerValue NeedsArgValue;
		ASSERT_THAT(IsFalse(
			FAngelscriptType::GetDebuggerValueFromFunction(
				NeedsArgFunction,
				Target,
				NeedsArgValue,
				ScriptType,
				ScriptClass),
			TEXT("Debugger value guard test should reject methods whose signature still requires parameters")));
		ASSERT_THAT(AreEqual(0, *EvalCountPtr, TEXT("Debugger value guard test should not execute the parameterized method when the signature guard rejects it")));

		FDebuggerValue GetterValue;
		const bool bGetterResolved = FAngelscriptType::GetDebuggerValueFromFunction(
			GetterFunction,
			Target,
			GetterValue,
			ScriptType,
			ScriptClass);
		ASSERT_THAT(IsTrue(bGetterResolved, TEXT("Debugger value guard test should evaluate the getter once all guards are removed")));
		if (bGetterResolved)
		{
			ASSERT_THAT(AreEqual(
				FString(TEXT("42")),
				GetterValue.Value,
				TEXT("Debugger value guard test should stringify the getter return value once evaluation is allowed")));
			ASSERT_THAT(IsTrue(GetterValue.bTemporaryValue, TEXT("Debugger value guard test should report the getter result as a temporary debugger value")));
		}

		ASSERT_THAT(AreEqual(1, *EvalCountPtr, TEXT("Debugger value guard test should increment EvalCount exactly once after the successful evaluation")));

		}
	}

	TEST_METHOD(InheritedGetterTracksBasePropertyAddress)
	{
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine);
		static const FName ModuleName(TEXT("InternalsDebuggerValueInheritedGetterTracking"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
			ASTEST_RESET_ENGINE(Engine);
			CollectGarbage(RF_NoFlags, true);
		};

		UClass* DerivedClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("InternalsDebuggerValueInheritedGetterTracking.as"),
			TEXT(R"AS(
UCLASS()
class ADebuggerValueBaseProbe : AActor
{
	UPROPERTY()
	int Health = 42;
}

UCLASS()
class ADebuggerValueDerivedProbe : ADebuggerValueBaseProbe
{
	UFUNCTION()
	int GetHealth() const
	{
		return Health;
	}
}
)AS"),
			TEXT("ADebuggerValueDerivedProbe"));

		if (DerivedClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();

		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, DerivedClass);
		asITypeInfo* ScriptType = FindScriptTypeInfoForClass(*TestRunner, Engine, DerivedClass);
		asIScriptFunction* GetterFunction = ScriptType != nullptr
			? FindMethodByDecl(*TestRunner, *ScriptType, TEXT("int GetHealth() const"))
			: nullptr;
		FIntProperty* HealthProperty = FindFProperty<FIntProperty>(DerivedClass, TEXT("Health"));

		ASSERT_THAT(IsNotNull(Actor, TEXT("Debugger value inherited getter tracking should spawn the derived script actor")));
		ASSERT_THAT(IsNotNull(HealthProperty, TEXT("Debugger value inherited getter tracking should expose the inherited Health property")));
		ASSERT_THAT(IsNotNull(Actor->GetWorld(), TEXT("Debugger value inherited getter tracking should keep the spawned actor inside a world")));
		ASSERT_THAT(IsNotNull(GetterFunction, TEXT("Debugger value inherited getter tracking should resolve the derived getter method")));

		void* const HealthAddress = HealthProperty->ContainerPtrToValuePtr<void>(Actor);
		int32* const HealthValue = static_cast<int32*>(HealthAddress);
		ASSERT_THAT(IsNotNull(HealthValue, TEXT("Debugger value inherited getter tracking should expose reflected Health storage")));

		ASSERT_THAT(AreEqual(
			42,
			*HealthValue,
			TEXT("Debugger value inherited getter tracking should start from the base-class default Health value")));

		FDebuggerValue FirstValue;
		const bool bFirstResolved = FAngelscriptType::GetDebuggerValueFromFunction(
			GetterFunction,
			Actor,
			FirstValue,
			ScriptType,
			DerivedClass,
			TEXT("Health"));
		ASSERT_THAT(IsTrue(bFirstResolved, TEXT("Debugger value inherited getter tracking should evaluate the derived getter before mutation")));
		if (bFirstResolved)
		{
			ExpectTrackedDebuggerValue(
				*TestRunner,
				TEXT("Debugger value inherited getter tracking first evaluation"),
				FirstValue,
				TEXT("42"),
				HealthAddress);
		}

		*HealthValue = 99;
		ASSERT_THAT(AreEqual(
			99,
			*HealthValue,
			TEXT("Debugger value inherited getter tracking should mutate the inherited Health storage in place")));

		FDebuggerValue SecondValue;
		const bool bSecondResolved = FAngelscriptType::GetDebuggerValueFromFunction(
			GetterFunction,
			Actor,
			SecondValue,
			ScriptType,
			DerivedClass,
			TEXT("Health"));
		ASSERT_THAT(IsTrue(bSecondResolved, TEXT("Debugger value inherited getter tracking should evaluate the derived getter again after mutation")));
		if (bSecondResolved)
		{
			ExpectTrackedDebuggerValue(
				*TestRunner,
				TEXT("Debugger value inherited getter tracking second evaluation"),
				SecondValue,
				TEXT("99"),
				HealthAddress);
		}

		}
	}
};

#endif
