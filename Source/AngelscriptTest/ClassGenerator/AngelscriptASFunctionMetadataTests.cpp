#include "Shared/AngelscriptFunctionalTestUtils.h"
#include "Shared/AngelscriptTestMacros.h"

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

		if (!Test.TestEqual(
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
			if (!Test.TestEqual(*(Context + TEXT(" should preserve the parameter name")), ValidateParameter->GetFName(), ServerParameter->GetFName())
				|| !Test.TestEqual(*(Context + TEXT(" should preserve the parameter property class")), ValidateParameter->GetClass(), ServerParameter->GetClass())
				|| !Test.TestEqual(*(Context + TEXT(" should preserve the parameter cpp type")), ValidateParameter->GetCPPType(), ServerParameter->GetCPPType()))
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
		if (!TestRunner->TestNotNull(TEXT("ASFunction.NetValidateCachesValidateFunction should generate the server RPC"), ServerFunction)
			|| !TestRunner->TestNotNull(TEXT("ASFunction.NetValidateCachesValidateFunction should generate the _Validate companion function"), ValidateFunction)
			|| !TestRunner->TestNotNull(TEXT("ASFunction.NetValidateCachesValidateFunction should expose the server RPC as UASFunction"), GeneratedServerFunction))
		{ return; }

		TestRunner->TestTrue(TEXT("ASFunction.NetValidateCachesValidateFunction should mark the server RPC as net"), ServerFunction->HasAnyFunctionFlags(FUNC_Net));
		TestRunner->TestTrue(TEXT("ASFunction.NetValidateCachesValidateFunction should mark the server RPC as requiring validation"), ServerFunction->HasAnyFunctionFlags(FUNC_NetValidate));

		UFunction* CachedValidateFunction = GeneratedServerFunction->GetRuntimeValidateFunction();
		if (!TestRunner->TestNotNull(TEXT("ASFunction.NetValidateCachesValidateFunction should cache the _Validate function on the generated RPC"), CachedValidateFunction))
		{ return; }

		if (!TestRunner->TestTrue(TEXT("ASFunction.NetValidateCachesValidateFunction should return the reflected _Validate function"), CachedValidateFunction == ValidateFunction)
			|| !TestRunner->TestTrue(TEXT("ASFunction.NetValidateCachesValidateFunction should return the same cached pointer on repeated lookups"), GeneratedServerFunction->GetRuntimeValidateFunction() == CachedValidateFunction))
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

		if (!TestRunner->TestNotNull(TEXT("ASFunction.NetValidateCachesValidateFunction should expose a return property on the _Validate function"), ReturnProperty)
			|| !TestRunner->TestTrue(TEXT("ASFunction.NetValidateCachesValidateFunction should keep the _Validate return type as bool"), ReturnProperty->IsA<FBoolProperty>()))
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

		if (!TestRunner->TestTrue(
				TEXT("ASFunction metadata classification test should compile"),
				CompileAnnotatedModuleFromMemory(&Engine, ClassificationModuleName, ClassificationFilename, ScriptSource)))
		{
			return;
		}

		UClass* ScriptClass = FindGeneratedClass(&Engine, ClassificationClassName);
		UClass* StaticsClass = FindGeneratedClass(&Engine, ClassificationStaticsClassName);
		if (!TestRunner->TestNotNull(TEXT("ASFunction metadata classification test should generate the UObject class"), ScriptClass)
			|| !TestRunner->TestNotNull(TEXT("ASFunction metadata classification test should generate the module statics class"), StaticsClass))
		{
			return;
		}

		UASFunction* GeneratedFunction = Cast<UASFunction>(FindGeneratedFunction(ScriptClass, TEXT("ComputeValue")));
		FIntProperty* GeneratedClassProperty = FindFProperty<FIntProperty>(ScriptClass, TEXT("StoredValue"));
		FIntProperty* GeneratedParamProperty = GeneratedFunction != nullptr ? FindFProperty<FIntProperty>(GeneratedFunction, TEXT("Value")) : nullptr;
		FIntProperty* GeneratedReturnProperty = GeneratedFunction != nullptr ? FindFProperty<FIntProperty>(GeneratedFunction, TEXT("ReturnValue")) : nullptr;
		if (!TestRunner->TestNotNull(TEXT("ASFunction metadata classification test should expose ComputeValue as a UASFunction"), GeneratedFunction)
			|| !TestRunner->TestNotNull(TEXT("ASFunction metadata classification test should expose a generated class property"), GeneratedClassProperty)
			|| !TestRunner->TestNotNull(TEXT("ASFunction metadata classification test should expose a generated parameter property"), GeneratedParamProperty)
			|| !TestRunner->TestNotNull(TEXT("ASFunction metadata classification test should expose a generated return property"), GeneratedReturnProperty))
		{
			return;
		}

		UASFunction* WorldContextFunction = Cast<UASFunction>(FindGeneratedFunction(StaticsClass, TEXT("CheckMetadataWorldContext")));
		FObjectProperty* WorldContextProperty = WorldContextFunction != nullptr ? FindFProperty<FObjectProperty>(WorldContextFunction, TEXT("WorldContextObject")) : nullptr;
		FIntProperty* WorldContextValueProperty = WorldContextFunction != nullptr ? FindFProperty<FIntProperty>(WorldContextFunction, TEXT("Value")) : nullptr;
		if (!TestRunner->TestNotNull(TEXT("ASFunction metadata classification test should expose the world-context UASFunction"), WorldContextFunction)
			|| !TestRunner->TestNotNull(TEXT("ASFunction metadata classification test should expose the WorldContextObject property"), WorldContextProperty)
			|| !TestRunner->TestNotNull(TEXT("ASFunction metadata classification test should expose the ordinary world-context function property"), WorldContextValueProperty))
		{
			return;
		}

		UFunction* NativeFunction = UAngelscriptUhtCoverageTestLibrary::StaticClass()->FindFunctionByName(TEXT("RequiresWorldContext"));
		FIntProperty* NativeValueProperty = NativeFunction != nullptr ? FindFProperty<FIntProperty>(NativeFunction, TEXT("Value")) : nullptr;
		if (!TestRunner->TestNotNull(TEXT("ASFunction metadata classification test should find a native comparison function"), NativeFunction)
			|| !TestRunner->TestNotNull(TEXT("ASFunction metadata classification test should find a native comparison property"), NativeValueProperty))
		{
			return;
		}

		TestRunner->TestFalse(TEXT("Null UFunction should not be classified as Angelscript-generated"), IsAngelscriptGenerated(static_cast<const UFunction*>(nullptr)));
		TestRunner->TestFalse(TEXT("Null FProperty should not be classified as Angelscript-generated"), IsAngelscriptGenerated(static_cast<const FProperty*>(nullptr)));
		TestRunner->TestFalse(TEXT("Null FProperty should not be classified as an Angelscript world-context property"), IsAngelscriptWorldContextProperty(static_cast<const FProperty*>(nullptr)));

		TestRunner->TestTrue(TEXT("Generated UASFunction should be classified as Angelscript-generated"), IsAngelscriptGenerated(GeneratedFunction));
		TestRunner->TestTrue(TEXT("Generated class property should be classified as Angelscript-generated"), IsAngelscriptGenerated(GeneratedClassProperty));
		TestRunner->TestTrue(TEXT("Generated parameter property should be classified as Angelscript-generated"), IsAngelscriptGenerated(GeneratedParamProperty));
		TestRunner->TestTrue(TEXT("Generated return property should be classified as Angelscript-generated"), IsAngelscriptGenerated(GeneratedReturnProperty));

		TestRunner->TestFalse(TEXT("Native UFunction should not be classified as Angelscript-generated"), IsAngelscriptGenerated(NativeFunction));
		TestRunner->TestFalse(TEXT("Native UFunction property should not be classified as Angelscript-generated"), IsAngelscriptGenerated(NativeValueProperty));
		TestRunner->TestFalse(TEXT("Native UFunction property should not be classified as an Angelscript world-context property"), IsAngelscriptWorldContextProperty(NativeValueProperty));

		TestRunner->TestTrue(TEXT("Generated world-context parameter should be classified as Angelscript-generated"), IsAngelscriptGenerated(WorldContextProperty));
		TestRunner->TestTrue(TEXT("Generated world-context parameter should be classified as an Angelscript world-context property"), IsAngelscriptWorldContextProperty(WorldContextProperty));
		TestRunner->TestTrue(TEXT("Generated ordinary world-context function parameter should be classified as Angelscript-generated"), IsAngelscriptGenerated(WorldContextValueProperty));
		TestRunner->TestFalse(TEXT("Generated ordinary world-context function parameter should not be classified as a world-context property"), IsAngelscriptWorldContextProperty(WorldContextValueProperty));

		const FString SourcePathBeforeDiscard = GeneratedFunction->GetSourceFilePath();
		const int32 SourceLineBeforeDiscard = GeneratedFunction->GetSourceLineNumber();
		if (!TestRunner->TestFalse(TEXT("Generated UASFunction should expose non-empty source path before discard"), SourcePathBeforeDiscard.IsEmpty())
			|| !TestRunner->TestTrue(TEXT("Generated UASFunction should expose positive source line before discard"), SourceLineBeforeDiscard > 0))
		{
			return;
		}

		Engine.DiscardModule(*ClassificationModuleName.ToString());
		bModuleDiscarded = true;
		CollectGarbage(RF_NoFlags, true);

		const FString SourcePathAfterDiscard = GeneratedFunction->GetSourceFilePath();
		const int32 SourceLineAfterDiscard = GeneratedFunction->GetSourceLineNumber();

		TestRunner->TestTrue(TEXT("Stale generated UASFunction should remain classified as Angelscript-generated"), IsAngelscriptGenerated(GeneratedFunction));
		TestRunner->TestTrue(
			TEXT("Stale generated UASFunction should clear or invalidate source metadata after discard"),
			SourcePathAfterDiscard.IsEmpty() || SourceLineAfterDiscard == -1);

		}
	}
};

#endif
