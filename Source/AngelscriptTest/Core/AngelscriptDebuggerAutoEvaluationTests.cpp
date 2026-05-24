#include "AngelscriptSettings.h"
#include "AngelscriptType.h"
#include "ClassGenerator/ASClass.h"
#include "AngelscriptFunctionalTestUtils.h"
#include "AngelscriptTestMacros.h"

#include "CQTest.h"
#include "Containers/StringConv.h"
#include "Misc/ScopeExit.h"
#include "UObject/GarbageCollection.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UnrealType.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptFunctionalTestUtils;

namespace AngelscriptTest_Core_AngelscriptDebuggerAutoEvaluationTests_Private
{
	asITypeInfo* FindScriptTypeInfoForClass(
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

		Test.TestNotNull(
			*FString::Printf(TEXT("Debugger auto-evaluate test should resolve script type '%s'"), *BoundTypeName),
			ScriptType);
		return ScriptType;
	}

	asIScriptFunction* FindMethodByDecl(
		FAutomationTestBase& Test,
		asITypeInfo& ScriptType,
		const FString& Declaration)
	{
		const FTCHARToUTF8 DeclarationUtf8(*Declaration);
		asIScriptFunction* Function = ScriptType.GetMethodByDecl(DeclarationUtf8.Get());
		Test.TestNotNull(
			*FString::Printf(TEXT("Debugger auto-evaluate test should resolve method '%s'"), *Declaration),
			Function);
		return Function;
	}

	FString BuildDebuggerFunctionPath(const asIScriptFunction& ScriptFunction)
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
}


TEST_CLASS_WITH_FLAGS(FAngelscriptDebuggerAutoEvaluationTests,
	"Angelscript.TestModule.Engine.Debugger.AutoEvaluate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(RespectsBlacklistAndTracksSourceProperty)
	{
		using namespace AngelscriptTest_Core_AngelscriptDebuggerAutoEvaluationTests_Private;
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine);

		static const FName ModuleName(TEXT("CoreDebuggerAutoEvaluateWorldless"));
		static const FName GeneratedClassName(TEXT("UDebuggerAutoEvaluateWorldlessProbe"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
			ASTEST_RESET_ENGINE(Engine);
			CollectGarbage(RF_NoFlags, true);
		};

		UAngelscriptSettings& Settings = UAngelscriptSettings::Get();
		const TSet<FString> SavedWithoutWorldBlacklist = Settings.DebuggerBlacklistAutomaticFunctionEvaluationWithoutWorldContext;
		ON_SCOPE_EXIT
		{
			Settings.DebuggerBlacklistAutomaticFunctionEvaluationWithoutWorldContext = SavedWithoutWorldBlacklist;
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("CoreDebuggerAutoEvaluateWorldless.as"),
			TEXT(R"AS(
UCLASS()
class UDebuggerAutoEvaluateWorldlessProbe : UObject
{
	UPROPERTY()
	int StoredValue = 42;

	UFUNCTION()
	int GetStoredValue() const
	{
		return StoredValue;
	}
}
)AS"),
			GeneratedClassName);

		if (ScriptClass != nullptr)
		{
			asITypeInfo* ScriptType = FindScriptTypeInfoForClass(*TestRunner, Engine, ScriptClass);
			asIScriptFunction* GetterFunction = ScriptType != nullptr
				? FindMethodByDecl(*TestRunner, *ScriptType, TEXT("int GetStoredValue() const"))
				: nullptr;
			FIntProperty* StoredValueProperty = FindFProperty<FIntProperty>(ScriptClass, TEXT("StoredValue"));
			UObject* Target = NewObject<UObject>(GetTransientPackage(), ScriptClass, TEXT("DebuggerAutoEvaluateWorldlessTarget"));

			if (!TestRunner->TestNotNull(TEXT("Debugger auto-evaluate test should expose the generated StoredValue property"), StoredValueProperty) ||
				!TestRunner->TestNotNull(TEXT("Debugger auto-evaluate test should instantiate the generated UObject"), Target) ||
				!TestRunner->TestNotNull(TEXT("Debugger auto-evaluate test should resolve the generated getter method"), GetterFunction))
			{
				return;
			}

			TestRunner->TestTrue(
				TEXT("Debugger auto-evaluate test should keep the generated UObject worldless so the without-world blacklist path is reachable"),
				Target->GetWorld() == nullptr);

			void* const StoredValueAddress = StoredValueProperty->ContainerPtrToValuePtr<void>(Target);
			int32* const StoredValuePtr = static_cast<int32*>(StoredValueAddress);
			if (!TestRunner->TestNotNull(TEXT("Debugger auto-evaluate test should expose reflected StoredValue storage"), StoredValuePtr))
			{
				return;
			}

			*StoredValuePtr = 42;

			FDebuggerValue EvaluatedValue;
			const bool bEvaluated = FAngelscriptType::GetDebuggerValueFromFunction(
				GetterFunction,
				Target,
				EvaluatedValue,
				ScriptType,
				ScriptClass,
				TEXT("StoredValue"));
			TestRunner->TestTrue(
				TEXT("Debugger auto-evaluate test should evaluate the generated getter before blacklist filtering"),
				bEvaluated);
			if (bEvaluated)
			{
				TestRunner->TestEqual(
					TEXT("Debugger auto-evaluate test should stringify the getter result as the current StoredValue"),
					EvaluatedValue.Value,
					FString(TEXT("42")));
				TestRunner->TestTrue(
					TEXT("Debugger auto-evaluate test should mark function-return debugger values as temporary"),
					EvaluatedValue.bTemporaryValue);
				TestRunner->TestTrue(
					TEXT("Debugger auto-evaluate test should track the non-temporary StoredValue address"),
					EvaluatedValue.GetNonTemporaryAddress() == StoredValueAddress);
				TestRunner->TestTrue(
					TEXT("Debugger auto-evaluate test should monitor the StoredValue address for refresh"),
					EvaluatedValue.GetAddressToMonitor() == StoredValueAddress);
				TestRunner->TestEqual(
					TEXT("Debugger auto-evaluate test should report the StoredValue monitor size as int32"),
					EvaluatedValue.GetAddressToMonitorValueSize(),
					static_cast<int32>(sizeof(int32)));
			}

			Settings.DebuggerBlacklistAutomaticFunctionEvaluationWithoutWorldContext.Add(
				BuildDebuggerFunctionPath(*GetterFunction));

			FDebuggerValue BlacklistedValue;
			TestRunner->TestFalse(
				TEXT("Debugger auto-evaluate test should reject the getter once it is blacklisted for objects without world context"),
				FAngelscriptType::GetDebuggerValueFromFunction(
					GetterFunction,
					Target,
					BlacklistedValue,
					ScriptType,
					ScriptClass,
					TEXT("StoredValue")));
		}

		}
	}
};

#endif
