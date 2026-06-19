#include "AngelscriptFunctionalTestUtils.h"
#include "AngelscriptTestMacros.h"

#include "CQTest.h"
#include "ClassGenerator/ASClass.h"
#include "Components/ActorTestSpawner.h"
#include "Components/ActorComponent.h"
#include "GameFramework/Actor.h"
#include "Misc/ScopeExit.h"
#include "UObject/CoreNet.h"
#include "UObject/StructOnScope.h"
#include "UObject/UnrealType.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptFunctionalTestUtils;

namespace AngelscriptTest_Core_AngelscriptComponentProcessEventTests_Private
{
	static const FName ProcessEventModuleName(TEXT("ComponentProcessEventValidate"));
	static const FString ProcessEventFilename(TEXT("ComponentProcessEventValidate.as"));
	static const FName ProcessEventClassName(TEXT("UComponentProcessEventValidate"));
	static const FName ServerFunctionName(TEXT("Server_RecordValue"));
	static const FName ValidateFunctionName(TEXT("Server_RecordValue_Validate"));
	static const FName ValidateCallsPropertyName(TEXT("ValidateCalls"));
	static const FName BodyCallsPropertyName(TEXT("BodyCalls"));
	static const FName LastAcceptedValuePropertyName(TEXT("LastAcceptedValue"));
	static constexpr int32 AcceptedValue = 7;
	static constexpr int32 RejectedValue = -3;

		UActorComponent* CreateProcessEventTestCaseComponent(
		FAutomationTestBase& Test,
		AActor& OwnerActor,
		UClass* ComponentClass)
	{
		FNoDiscardAsserter Assert(Test);
		if (!Assert.IsNotNull(ComponentClass, TEXT("Component ProcessEvent test case should compile to a valid component class")))
		{
			return nullptr;
		}

		UActorComponent* Component = NewObject<UActorComponent>(&OwnerActor, ComponentClass);
		if (!Assert.IsNotNull(Component, TEXT("Component ProcessEvent test case should instantiate a runtime component")))
		{
			return nullptr;
		}

		OwnerActor.AddInstanceComponent(Component);
		Component->OnComponentCreated();
		Component->RegisterComponent();
		Component->Activate(true);

		UActorComponent* TypedComponent = Cast<UActorComponent>(Component);
		if (!Assert.IsNotNull(TypedComponent, TEXT("Component ProcessEvent test case should instantiate a UActorComponent")))
		{
			return nullptr;
		}

		return TypedComponent;
	}

	bool InvokeServerRecordValue(
		FAutomationTestBase& Test,
		FAngelscriptEngine& Engine,
		UActorComponent& Component,
		UFunction& Function,
		int32 InValue)
	{
		FNoDiscardAsserter Assert(Test);
		FStructOnScope FunctionParameters(&Function);
		uint8* ParametersMemory = FunctionParameters.GetStructMemory();
		if (!Assert.IsNotNull(ParametersMemory, TEXT("Component ProcessEvent test case should allocate parameter storage")))
		{
			return false;
		}

		FIntProperty* ValueProperty = FindFProperty<FIntProperty>(&Function, TEXT("Value"));
		if (!Assert.IsNotNull(ValueProperty, TEXT("Component ProcessEvent test case should expose the RPC value parameter")))
		{
			return false;
		}

		ValueProperty->SetPropertyValue_InContainer(ParametersMemory, InValue);

		FAngelscriptEngineScope ExecutionScope(Engine, &Component);
		Component.ProcessEvent(&Function, ParametersMemory);
		return true;
	}

	bool ExpectIntPropertyValue(
		FAutomationTestBase& Test,
		UObject& Object,
		FName PropertyName,
		int32 ExpectedValue,
		const TCHAR* Context)
	{
		int32 ActualValue = 0;
		if (!ReadPropertyValue<FIntProperty>(Test, &Object, PropertyName, ActualValue))
		{
			return false;
		}

		FNoDiscardAsserter Assert(Test);
		return Assert.AreEqual(ExpectedValue, ActualValue, Context);
	}
}


TEST_CLASS_WITH_FLAGS(FAngelscriptComponentProcessEventTests,
	"Angelscript.TestModule.Engine.Component.ProcessEvent",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(WithValidationRoutesValidateBeforeRpcBody)
	{
		using namespace AngelscriptTest_Core_AngelscriptComponentProcessEventTests_Private;
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine);
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ProcessEventModuleName.ToString());
			ASTEST_RESET_ENGINE(Engine);
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ProcessEventModuleName,
			ProcessEventFilename,
			TEXT(R"AS(
UCLASS()
class UComponentProcessEventValidate : UActorComponent
{
	UPROPERTY()
	int ValidateCalls = 0;

	UPROPERTY()
	int BodyCalls = 0;

	UPROPERTY()
	int LastAcceptedValue = -1;

	UFUNCTION(Server, WithValidation)
	void Server_RecordValue(int Value)
	{
		BodyCalls += 1;
		LastAcceptedValue = Value;
	}

	UFUNCTION()
	bool Server_RecordValue_Validate(int Value)
	{
		ValidateCalls += 1;
		return Value >= 0;
	}
}
)AS"),
			ProcessEventClassName);
		if (ScriptClass == nullptr)
		{
			return;
		}

		UFunction* ServerFunction = FindGeneratedFunction(ScriptClass, ServerFunctionName);
		UASFunction* GeneratedServerFunction = Cast<UASFunction>(ServerFunction);
		if (!this->Assert.IsNotNull(ServerFunction, TEXT("Component ProcessEvent test case should generate the server RPC"))
			|| !this->Assert.IsNotNull(GeneratedServerFunction, TEXT("Component ProcessEvent test case should expose the server RPC as UASFunction")))
		{
			return;
		}

		UFunction* ValidateFunction = GeneratedServerFunction->GetRuntimeValidateFunction();
		if (!this->Assert.IsNotNull(ValidateFunction, TEXT("Component ProcessEvent test case should cache the _Validate companion function")))
		{
			return;
		}

		ASSERT_THAT(IsTrue(ServerFunction->HasAnyFunctionFlags(FUNC_NetValidate), TEXT("Component ProcessEvent test case should mark the generated RPC as requiring validation")));
		ASSERT_THAT(IsTrue(ValidateFunction->GetFName() == ValidateFunctionName, TEXT("Component ProcessEvent test case should resolve the expected _Validate function")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor& HostActor = Spawner.SpawnActor<AActor>();
		UActorComponent* Component = CreateProcessEventTestCaseComponent(*TestRunner, HostActor, ScriptClass);
		if (Component == nullptr)
		{
			return;
		}

		BeginPlayActor(Engine, HostActor);

		RPC_ResetLastFailedReason();
		if (!InvokeServerRecordValue(*TestRunner, Engine, *Component, *ServerFunction, AcceptedValue))
		{
			return;
		}

		if (!ExpectIntPropertyValue(
				*TestRunner,
				*Component,
				ValidateCallsPropertyName,
				1,
				TEXT("Component ProcessEvent should call the _Validate companion before accepting the RPC"))
			|| !ExpectIntPropertyValue(
				*TestRunner,
				*Component,
				BodyCallsPropertyName,
				1,
				TEXT("Component ProcessEvent should execute the RPC body when validation passes"))
			|| !ExpectIntPropertyValue(
				*TestRunner,
				*Component,
				LastAcceptedValuePropertyName,
				AcceptedValue,
				TEXT("Component ProcessEvent should persist the accepted RPC payload")))
		{
			return;
		}

		ASSERT_THAT(IsTrue(RPC_GetLastFailedReason() == nullptr, TEXT("Component ProcessEvent should not record a validation failure for accepted input")));

		RPC_ResetLastFailedReason();
		if (!InvokeServerRecordValue(*TestRunner, Engine, *Component, *ServerFunction, RejectedValue))
		{
			return;
		}

		if (!ExpectIntPropertyValue(
				*TestRunner,
				*Component,
				ValidateCallsPropertyName,
				2,
				TEXT("Component ProcessEvent should call the _Validate companion again for rejected input"))
			|| !ExpectIntPropertyValue(
				*TestRunner,
				*Component,
				BodyCallsPropertyName,
				1,
				TEXT("Component ProcessEvent should not execute the RPC body when validation fails"))
			|| !ExpectIntPropertyValue(
				*TestRunner,
				*Component,
				LastAcceptedValuePropertyName,
				AcceptedValue,
				TEXT("Component ProcessEvent should preserve the last accepted payload after validation failure")))
		{
			return;
		}

		const TCHAR* FailedReason = RPC_GetLastFailedReason();
		if (!this->Assert.IsNotNull(FailedReason, TEXT("Component ProcessEvent should record the failed validation function name")))
		{
			return;
		}

		ASSERT_THAT(AreEqual(ValidateFunctionName.ToString(), FString(FailedReason), TEXT("Component ProcessEvent should report the _Validate function name on validation failure")));

		}
	}
};

#endif
