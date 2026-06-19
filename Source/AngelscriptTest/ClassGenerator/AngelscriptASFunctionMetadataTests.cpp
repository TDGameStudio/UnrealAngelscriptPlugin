#include "AngelscriptFunctionalTestUtils.h"
#include "AngelscriptTestMacros.h"

#include "CQTest.h"
#include "ClassGenerator/ASClass.h"
#include "Core/AngelscriptUhtCoverageTestTypes.h"
#include "Misc/ScopeExit.h"
#include "UObject/GarbageCollection.h"
#include "UObject/UnrealType.h"

// Test Layer: Runtime Integration
#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptFunctionalTestUtils;

namespace AngelscriptTest_ClassGenerator_AngelscriptASFunctionMetadataTests_Private
{
	static const TCHAR* CQMessage(const TCHAR* Message)
	{
		return Message;
	}

	static const TCHAR* CQMessage(const FString& Message)
	{
		return *Message;
	}

	static bool CheckTrue(FAutomationTestBase& Test, const TCHAR* Message, bool bActual)
	{
		FNoDiscardAsserter Assert(Test);
		return Assert.IsTrue(bActual, Message);
	}

	static bool CheckFalse(FAutomationTestBase& Test, const TCHAR* Message, bool bActual)
	{
		FNoDiscardAsserter Assert(Test);
		return Assert.IsFalse(bActual, Message);
	}

	template <typename ActualType, typename ExpectedType>
	static bool CheckEqual(FAutomationTestBase& Test, const TCHAR* Message, const ActualType& Actual, const ExpectedType& Expected)
	{
		FNoDiscardAsserter Assert(Test);
		return Assert.AreEqual(Expected, Actual, Message);
	}

	template <typename ActualType, typename ExpectedType>
	static bool CheckEqual(FAutomationTestBase& Test, const FString& Message, const ActualType& Actual, const ExpectedType& Expected)
	{
		return CheckEqual(Test, CQMessage(Message), Actual, Expected);
	}

	template <typename ValueType>
	static bool CheckNotNull(FAutomationTestBase& Test, const TCHAR* Message, const ValueType& Value)
	{
		FNoDiscardAsserter Assert(Test);
		return Assert.IsNotNull(Value, Message);
	}

	static const FName NetValidateModuleName(TEXT("ASFunctionNetValidateCache"));
	static const FString NetValidateFilename(TEXT("ASFunctionNetValidateCache.as"));
	static const FName ClassificationModuleName(TEXT("ASFunctionMetadataClassification"));
	static const FString ClassificationFilename(TEXT("ASFunctionMetadataClassification.as"));
	static const FName ClassificationClassName(TEXT("UASFunctionMetadataClassification"));
	static const FName ClassificationStaticsClassName(TEXT("UModule_ASFunctionMetadataClassificationStatics"));

	void CollectNonReturnParameters(UFunction& Function, TArray<FProperty*>& OutParameters)
	{
		for (TFieldIterator<FProperty> It(&Function); It; ++It)
		{
			FProperty* Property = *It;
			if (Property->HasAnyPropertyFlags(CPF_Parm) && !Property->HasAnyPropertyFlags(CPF_ReturnParm))
			{
				OutParameters.Add(Property);
			}
		}
	}

	bool ExpectMatchingParameterSignature(
		FAutomationTestBase& Test,
		UFunction& ServerFunction,
		UFunction& ValidateFunction)
	{
		TArray<FProperty*> ServerParameters;
		TArray<FProperty*> ValidateParameters;
		CollectNonReturnParameters(ServerFunction, ServerParameters);
		CollectNonReturnParameters(ValidateFunction, ValidateParameters);

		if (!CheckEqual(Test,
				TEXT("ASFunction.NetValidateCachesValidateFunction should keep the same parameter count on the _Validate function"),
				ValidateParameters.Num(),
				ServerParameters.Num()))
		{
			return false;
		}

		for (int32 Index = 0; Index < ServerParameters.Num(); ++Index)
		{
			FProperty* ServerParameter = ServerParameters[Index];
			FProperty* ValidateParameter = ValidateParameters[Index];
			const FString Context = FString::Printf(TEXT("ASFunction.NetValidateCachesValidateFunction parameter %d"), Index);
			if (!CheckEqual(Test, Context + TEXT(" should preserve the parameter name"), ValidateParameter->GetFName(), ServerParameter->GetFName())
				|| !CheckEqual(Test, Context + TEXT(" should preserve the parameter property class"), ValidateParameter->GetClass(), ServerParameter->GetClass())
				|| !CheckEqual(Test, Context + TEXT(" should preserve the parameter cpp type"), ValidateParameter->GetCPPType(), ServerParameter->GetCPPType()))
			{
				return false;
			}
		}

		return true;
	}
}

TEST_CLASS_WITH_FLAGS(FAngelscriptASFunctionMetadataTests,
	"Angelscript.TestModule.ClassGenerator.ASFunction",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(NetValidateCachesValidateFunction)
	{
		using namespace AngelscriptTest_ClassGenerator_AngelscriptASFunctionMetadataTests_Private;
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine);
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*NetValidateModuleName.ToString());
			ASTEST_RESET_ENGINE(Engine);
		};

		UClass* ScriptClass = CompileScriptModule(*TestRunner, Engine, NetValidateModuleName, NetValidateFilename,
			TEXT(R"AS(
UCLASS()
class AASFunctionNetValidateCache : AActor
{
	UFUNCTION(Server, WithValidation)
	void Server_SetValue(int Value)
	{
	}

	UFUNCTION()
	bool Server_SetValue_Validate(int Value)
	{
		return Value >= 0;
	}
}
)AS"),
			TEXT("AASFunctionNetValidateCache"));
		if (ScriptClass == nullptr) { return; }

		UFunction* ServerFunction = FindGeneratedFunction(ScriptClass, TEXT("Server_SetValue"));
		UFunction* ValidateFunction = FindGeneratedFunction(ScriptClass, TEXT("Server_SetValue_Validate"));
		UASFunction* GeneratedServerFunction = Cast<UASFunction>(ServerFunction);
		if (!CheckNotNull(*TestRunner, TEXT("ASFunction.NetValidateCachesValidateFunction should generate the server RPC"), ServerFunction)
			|| !CheckNotNull(*TestRunner, TEXT("ASFunction.NetValidateCachesValidateFunction should generate the _Validate companion function"), ValidateFunction)
			|| !CheckNotNull(*TestRunner, TEXT("ASFunction.NetValidateCachesValidateFunction should expose the server RPC as UASFunction"), GeneratedServerFunction))
		{ return; }

		ASSERT_THAT(IsTrue(ServerFunction->HasAnyFunctionFlags(FUNC_Net), TEXT("ASFunction.NetValidateCachesValidateFunction should mark the server RPC as net")));
		ASSERT_THAT(IsTrue(ServerFunction->HasAnyFunctionFlags(FUNC_NetValidate), TEXT("ASFunction.NetValidateCachesValidateFunction should mark the server RPC as requiring validation")));

		UFunction* CachedValidateFunction = GeneratedServerFunction->GetRuntimeValidateFunction();
		if (!CheckNotNull(*TestRunner, TEXT("ASFunction.NetValidateCachesValidateFunction should cache the _Validate function on the generated RPC"), CachedValidateFunction))
		{ return; }

		if (!CheckTrue(*TestRunner, TEXT("ASFunction.NetValidateCachesValidateFunction should return the reflected _Validate function"), CachedValidateFunction == ValidateFunction)
			|| !CheckTrue(*TestRunner, TEXT("ASFunction.NetValidateCachesValidateFunction should return the same cached pointer on repeated lookups"), GeneratedServerFunction->GetRuntimeValidateFunction() == CachedValidateFunction))
		{ return; }

		FProperty* ReturnProperty = nullptr;
		for (TFieldIterator<FProperty> It(ValidateFunction); It; ++It)
		{
			if (It->HasAnyPropertyFlags(CPF_ReturnParm))
			{
				ReturnProperty = *It;
				break;
			}
		}

		if (!CheckNotNull(*TestRunner, TEXT("ASFunction.NetValidateCachesValidateFunction should expose a return property on the _Validate function"), ReturnProperty)
			|| !CheckTrue(*TestRunner, TEXT("ASFunction.NetValidateCachesValidateFunction should keep the _Validate return type as bool"), ReturnProperty->IsA<FBoolProperty>()))
		{ return; }

		if (!ExpectMatchingParameterSignature(*TestRunner, *ServerFunction, *ValidateFunction))
		{ return; }

		}
	}

	TEST_METHOD(GeneratedNativeAndStaleMetadataAreClassified)
	{
		using namespace AngelscriptTest_ClassGenerator_AngelscriptASFunctionMetadataTests_Private;
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine);

		bool bModuleDiscarded = false;
		ON_SCOPE_EXIT
		{
			if (!bModuleDiscarded)
			{
				Engine.DiscardModule(*ClassificationModuleName.ToString());
			}
			ASTEST_RESET_ENGINE(Engine);
		};

		const FString ScriptSource = TEXT(R"AS(
UCLASS()
class UASFunctionMetadataClassification : UObject
{
	UPROPERTY()
	int StoredValue = 12;

	UFUNCTION()
	int ComputeValue(int Value)
	{
		return Value + StoredValue;
	}
}

UFUNCTION(BlueprintCallable, meta = (WorldContext = "WorldContextObject"))
int CheckMetadataWorldContext(UObject WorldContextObject, int Value)
{
	return Value;
}
)AS");

		if (!CheckTrue(*TestRunner,
				TEXT("ASFunction metadata classification test should compile"),
				CompileAnnotatedModuleFromMemory(&Engine, ClassificationModuleName, ClassificationFilename, ScriptSource)))
		{
			return;
		}

		UClass* ScriptClass = FindGeneratedClass(&Engine, ClassificationClassName);
		UClass* StaticsClass = FindGeneratedClass(&Engine, ClassificationStaticsClassName);
		if (!CheckNotNull(*TestRunner, TEXT("ASFunction metadata classification test should generate the UObject class"), ScriptClass)
			|| !CheckNotNull(*TestRunner, TEXT("ASFunction metadata classification test should generate the module statics class"), StaticsClass))
		{
			return;
		}

		UASFunction* GeneratedFunction = Cast<UASFunction>(FindGeneratedFunction(ScriptClass, TEXT("ComputeValue")));
		FIntProperty* GeneratedClassProperty = FindFProperty<FIntProperty>(ScriptClass, TEXT("StoredValue"));
		FIntProperty* GeneratedParamProperty = GeneratedFunction != nullptr ? FindFProperty<FIntProperty>(GeneratedFunction, TEXT("Value")) : nullptr;
		FIntProperty* GeneratedReturnProperty = GeneratedFunction != nullptr ? FindFProperty<FIntProperty>(GeneratedFunction, TEXT("ReturnValue")) : nullptr;
		if (!CheckNotNull(*TestRunner, TEXT("ASFunction metadata classification test should expose ComputeValue as a UASFunction"), GeneratedFunction)
			|| !CheckNotNull(*TestRunner, TEXT("ASFunction metadata classification test should expose a generated class property"), GeneratedClassProperty)
			|| !CheckNotNull(*TestRunner, TEXT("ASFunction metadata classification test should expose a generated parameter property"), GeneratedParamProperty)
			|| !CheckNotNull(*TestRunner, TEXT("ASFunction metadata classification test should expose a generated return property"), GeneratedReturnProperty))
		{
			return;
		}

		UASFunction* WorldContextFunction = Cast<UASFunction>(FindGeneratedFunction(StaticsClass, TEXT("CheckMetadataWorldContext")));
		FObjectProperty* WorldContextProperty = WorldContextFunction != nullptr ? FindFProperty<FObjectProperty>(WorldContextFunction, TEXT("WorldContextObject")) : nullptr;
		FIntProperty* WorldContextValueProperty = WorldContextFunction != nullptr ? FindFProperty<FIntProperty>(WorldContextFunction, TEXT("Value")) : nullptr;
		if (!CheckNotNull(*TestRunner, TEXT("ASFunction metadata classification test should expose the world-context UASFunction"), WorldContextFunction)
			|| !CheckNotNull(*TestRunner, TEXT("ASFunction metadata classification test should expose the WorldContextObject property"), WorldContextProperty)
			|| !CheckNotNull(*TestRunner, TEXT("ASFunction metadata classification test should expose the ordinary world-context function property"), WorldContextValueProperty))
		{
			return;
		}

		UFunction* NativeFunction = UAngelscriptUhtCoverageTestLibrary::StaticClass()->FindFunctionByName(TEXT("RequiresWorldContext"));
		FIntProperty* NativeValueProperty = NativeFunction != nullptr ? FindFProperty<FIntProperty>(NativeFunction, TEXT("Value")) : nullptr;
		if (!CheckNotNull(*TestRunner, TEXT("ASFunction metadata classification test should find a native comparison function"), NativeFunction)
			|| !CheckNotNull(*TestRunner, TEXT("ASFunction metadata classification test should find a native comparison property"), NativeValueProperty))
		{
			return;
		}

		ASSERT_THAT(IsFalse(IsAngelscriptGenerated(static_cast<const UFunction*>(nullptr)), TEXT("Null UFunction should not be classified as Angelscript-generated")));
		ASSERT_THAT(IsFalse(IsAngelscriptGenerated(static_cast<const FProperty*>(nullptr)), TEXT("Null FProperty should not be classified as Angelscript-generated")));
		ASSERT_THAT(IsFalse(IsAngelscriptWorldContextProperty(static_cast<const FProperty*>(nullptr)), TEXT("Null FProperty should not be classified as an Angelscript world-context property")));

		ASSERT_THAT(IsTrue(IsAngelscriptGenerated(GeneratedFunction), TEXT("Generated UASFunction should be classified as Angelscript-generated")));
		ASSERT_THAT(IsTrue(IsAngelscriptGenerated(GeneratedClassProperty), TEXT("Generated class property should be classified as Angelscript-generated")));
		ASSERT_THAT(IsTrue(IsAngelscriptGenerated(GeneratedParamProperty), TEXT("Generated parameter property should be classified as Angelscript-generated")));
		ASSERT_THAT(IsTrue(IsAngelscriptGenerated(GeneratedReturnProperty), TEXT("Generated return property should be classified as Angelscript-generated")));

		ASSERT_THAT(IsFalse(IsAngelscriptGenerated(NativeFunction), TEXT("Native UFunction should not be classified as Angelscript-generated")));
		ASSERT_THAT(IsFalse(IsAngelscriptGenerated(NativeValueProperty), TEXT("Native UFunction property should not be classified as Angelscript-generated")));
		ASSERT_THAT(IsFalse(IsAngelscriptWorldContextProperty(NativeValueProperty), TEXT("Native UFunction property should not be classified as an Angelscript world-context property")));

		ASSERT_THAT(IsTrue(IsAngelscriptGenerated(WorldContextProperty), TEXT("Generated world-context parameter should be classified as Angelscript-generated")));
		ASSERT_THAT(IsTrue(IsAngelscriptWorldContextProperty(WorldContextProperty), TEXT("Generated world-context parameter should be classified as an Angelscript world-context property")));
		ASSERT_THAT(IsTrue(IsAngelscriptGenerated(WorldContextValueProperty), TEXT("Generated ordinary world-context function parameter should be classified as Angelscript-generated")));
		ASSERT_THAT(IsFalse(IsAngelscriptWorldContextProperty(WorldContextValueProperty), TEXT("Generated ordinary world-context function parameter should not be classified as a world-context property")));

		const FString SourcePathBeforeDiscard = GeneratedFunction->GetSourceFilePath();
		const int32 SourceLineBeforeDiscard = GeneratedFunction->GetSourceLineNumber();
		if (!CheckFalse(*TestRunner, TEXT("Generated UASFunction should expose non-empty source path before discard"), SourcePathBeforeDiscard.IsEmpty())
			|| !CheckTrue(*TestRunner, TEXT("Generated UASFunction should expose positive source line before discard"), SourceLineBeforeDiscard > 0))
		{
			return;
		}

		Engine.DiscardModule(*ClassificationModuleName.ToString());
		bModuleDiscarded = true;
		CollectGarbage(RF_NoFlags, true);

		const FString SourcePathAfterDiscard = GeneratedFunction->GetSourceFilePath();
		const int32 SourceLineAfterDiscard = GeneratedFunction->GetSourceLineNumber();

		ASSERT_THAT(IsTrue(IsAngelscriptGenerated(GeneratedFunction), TEXT("Stale generated UASFunction should remain classified as Angelscript-generated")));
		ASSERT_THAT(IsTrue(
			SourcePathAfterDiscard.IsEmpty() || SourceLineAfterDiscard == -1,
			TEXT("Stale generated UASFunction should clear or invalidate source metadata after discard")));

		}
	}
};

#endif
