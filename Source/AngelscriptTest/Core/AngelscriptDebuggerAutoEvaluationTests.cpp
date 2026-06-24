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


TEST_CLASS_WITH_FLAGS(FAngelscriptDebuggerAutoEvaluationTests,
	"Angelscript.TestModule.Engine.Debugger.AutoEvaluate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
static asITypeInfo* FindScriptTypeInfoForClass(
	FAutomationTestBase& Test,
	FAngelscriptEngine& Engine,
	UClass* ScriptClass)
{
	FNoDiscardAsserter LocalAssert(Test);
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

	if (!LocalAssert.IsNotNull(
			ScriptType,
			*FString::Printf(TEXT("Debugger auto-evaluate test should resolve script type '%s'"), *BoundTypeName)))
	{
		return nullptr;
	}
	return ScriptType;
}

static asIScriptFunction* FindMethodByDecl(
	FAutomationTestBase& Test,
	asITypeInfo& ScriptType,
	const FString& Declaration)
{
	FNoDiscardAsserter LocalAssert(Test);
	const FTCHARToUTF8 DeclarationUtf8(*Declaration);
	asIScriptFunction* Function = ScriptType.GetMethodByDecl(DeclarationUtf8.Get());
	if (!LocalAssert.IsNotNull(
			Function,
			*FString::Printf(TEXT("Debugger auto-evaluate test should resolve method '%s'"), *Declaration)))
	{
		return nullptr;
	}
	return Function;
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
	TEST_METHOD(RespectsBlacklistAndTracksSourceProperty)
	{
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

			if (!this->Assert.IsNotNull(StoredValueProperty, TEXT("Debugger auto-evaluate test should expose the generated StoredValue property")) ||
				!this->Assert.IsNotNull(Target, TEXT("Debugger auto-evaluate test should instantiate the generated UObject")) ||
				!this->Assert.IsNotNull(GetterFunction, TEXT("Debugger auto-evaluate test should resolve the generated getter method")))
			{
				return;
			}

			ASSERT_THAT(IsTrue(
				Target->GetWorld() == nullptr,
				TEXT("Debugger auto-evaluate test should keep the generated UObject worldless so the without-world blacklist path is reachable")));

			void* const StoredValueAddress = StoredValueProperty->ContainerPtrToValuePtr<void>(Target);
			int32* const StoredValuePtr = static_cast<int32*>(StoredValueAddress);
			if (!this->Assert.IsNotNull(StoredValuePtr, TEXT("Debugger auto-evaluate test should expose reflected StoredValue storage")))
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
			ASSERT_THAT(IsTrue(
				bEvaluated,
				TEXT("Debugger auto-evaluate test should evaluate the generated getter before blacklist filtering")));
			ASSERT_THAT(AreEqual(
				FString(TEXT("42")),
				EvaluatedValue.Value,
				TEXT("Debugger auto-evaluate test should stringify the getter result as the current StoredValue")));
			ASSERT_THAT(IsTrue(
				EvaluatedValue.bTemporaryValue,
				TEXT("Debugger auto-evaluate test should mark function-return debugger values as temporary")));
			ASSERT_THAT(IsTrue(
				EvaluatedValue.GetNonTemporaryAddress() == StoredValueAddress,
				TEXT("Debugger auto-evaluate test should track the non-temporary StoredValue address")));
			ASSERT_THAT(IsTrue(
				EvaluatedValue.GetAddressToMonitor() == StoredValueAddress,
				TEXT("Debugger auto-evaluate test should monitor the StoredValue address for refresh")));
			ASSERT_THAT(AreEqual(
				static_cast<int32>(sizeof(int32)),
				EvaluatedValue.GetAddressToMonitorValueSize(),
				TEXT("Debugger auto-evaluate test should report the StoredValue monitor size as int32")));

			Settings.DebuggerBlacklistAutomaticFunctionEvaluationWithoutWorldContext.Add(
				BuildDebuggerFunctionPath(*GetterFunction));

			FDebuggerValue BlacklistedValue;
			ASSERT_THAT(IsFalse(
				FAngelscriptType::GetDebuggerValueFromFunction(
					GetterFunction,
					Target,
					BlacklistedValue,
					ScriptType,
					ScriptClass,
					TEXT("StoredValue")),
				TEXT("Debugger auto-evaluate test should reject the getter once it is blacklisted for objects without world context")));
		}

		}
	}
};

#endif
