#include "AngelscriptFunctionalTestUtils.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestWorld.h"

#include "CQTest.h"
#include "ClassGenerator/ASClass.h"
#include "Components/SceneComponent.h"
#include "Misc/ScopeExit.h"
#include "UObject/StructOnScope.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UnrealType.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FAngelscriptASFunctionArgumentMatrixTests,
	"Angelscript.TestModule.Generator.ASFunction.ArgumentMatrix",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	inline static const FName ModuleName = FName(TEXT("ASFunctionArgumentMatrix"));
	inline static const FString ScriptFilename = FString(TEXT("ASFunctionArgumentMatrix.as"));
	inline static const FName CarrierClassName = FName(TEXT("UASFunctionArgumentMatrixCarrier"));
	inline static const FName PayloadClassName = FName(TEXT("UASFunctionArgumentMatrixPayload"));
	inline static const FName FloatOverrideClassName = FName(TEXT("AASFunctionArgumentMatrixFloatOverride"));

	static int32 CountNonReturnParameters(UFunction* Function)
	{
		int32 Count = 0;
		for (TFieldIterator<FProperty> It(Function); It; ++It)
		{
			FProperty* Property = *It;
			if (Property->HasAnyPropertyFlags(CPF_Parm) && !Property->HasAnyPropertyFlags(CPF_ReturnParm))
			{
				++Count;
			}
		}
		return Count;
	}

	static const UASFunction::FArgument* FindScriptArgument(const UASFunction* Function, FName PropertyName)
	{
		if (Function == nullptr)
		{
			return nullptr;
		}

		return Function->Arguments.FindByPredicate(
			[PropertyName](const UASFunction::FArgument& Argument)
			{
				return Argument.Property != nullptr && Argument.Property->GetFName() == PropertyName;
			});
	}

	static bool InvokeWithParams(
		FAutomationTestBase& Test,
		FAngelscriptEngine& Engine,
		UObject* Object,
		UFunction* Function,
		void* ParamsMemory)
	{
		FNoDiscardAsserter LocalAssert(Test);
		if (!LocalAssert.IsNotNull(Object, TEXT("ASFunction argument matrix should have a valid target object"))
			|| !LocalAssert.IsNotNull(Function, TEXT("ASFunction argument matrix should have a valid reflected function"))
			|| !LocalAssert.IsNotNull(ParamsMemory, TEXT("ASFunction argument matrix should have reflected parameter memory")))
		{
			return false;
		}

		FAngelscriptEngineScope FunctionScope(Engine, Object);
		Object->ProcessEvent(Function, ParamsMemory);
		return true;
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

	TEST_METHOD(GeneratedFunctionPropertiesPreserveDefaultsRefsReturnsAndScriptObjectTypes)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope EngineScope(Engine);
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		const FString ScriptSource = ASTEST_AS(R"AS(
			UCLASS()
			class UASFunctionArgumentMatrixPayload : UObject
			{
				UPROPERTY()
				int Marker = 41;
			}

			UCLASS()
			class UASFunctionArgumentMatrixCarrier : UObject
			{
				UPROPERTY()
				UASFunctionArgumentMatrixPayload StoredPayload;

				UPROPERTY()
				int LastOutput = 0;

				UPROPERTY()
				int LastMutable = 0;

				UPROPERTY()
				FString LastLabel;

				UPROPERTY()
				bool bLastBool = false;

				UPROPERTY()
				int LastFloatHundredths = 0;

				UPROPERTY()
				int LastDoubleHundredths = 0;

				UFUNCTION(BlueprintCallable)
				void PodScalars(bool bFlag, float32 FloatValue, float64 DoubleValue)
				{
					bLastBool = bFlag;
					LastFloatHundredths = int(FloatValue * 100.0f);
					LastDoubleHundredths = int(DoubleValue * 100.0);
				}

				UFUNCTION(BlueprintCallable)
				float32 ReturnFloat()
				{
					return 12.25f;
				}

				UFUNCTION(BlueprintCallable)
				float64 ReturnDouble()
				{
					return 24.5;
				}

				UFUNCTION(BlueprintCallable, meta=(AutoCreateRefTerm="Label", AdvancedDisplay="Label"))
				int DefaultsAndReturn(int Value = 13, const FString&in Label = "ArgDefault")
				{
					LastLabel = Label;
					return Value + Label.Len();
				}

				UFUNCTION(BlueprintCallable)
				void OutAndInout(int Input, int&out Output, int&inout Mutable)
				{
					Output = Input * 2;
					Mutable += 3;
					LastOutput = Output;
					LastMutable = Mutable;
				}

				UFUNCTION(BlueprintCallable)
				UASFunctionArgumentMatrixPayload EchoPayload(UASFunctionArgumentMatrixPayload Payload)
				{
					StoredPayload = Payload;
					return Payload;
				}

				UFUNCTION(BlueprintCallable)
				int DispatchDefaults()
				{
					return DefaultsAndReturn();
				}
			}

			UCLASS()
			class AASFunctionArgumentMatrixFloatOverride : AActor
			{
				UPROPERTY()
				int LastTickHundredths = 0;

				UFUNCTION(BlueprintOverride)
				void Tick(float DeltaTime)
				{
					LastTickHundredths = int(DeltaTime * 100.0);
				}
			}
			)AS");

		UASClass* CarrierClass = Cast<UASClass>(AngelscriptFunctionalTestUtils::CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			ScriptFilename,
			ScriptSource,
			CarrierClassName));
		UClass* PayloadClass = ::FindGeneratedClass(&Engine, PayloadClassName);
		UClass* FloatOverrideClass = ::FindGeneratedClass(&Engine, FloatOverrideClassName);
		ASSERT_THAT(IsNotNull(CarrierClass, TEXT("ASFunction argument matrix carrier should compile to a UASClass")));
		ASSERT_THAT(IsNotNull(PayloadClass, TEXT("ASFunction argument matrix payload should compile to a generated UClass")));
		ASSERT_THAT(IsNotNull(FloatOverrideClass, TEXT("ASFunction argument matrix float override should compile to a generated UClass")));
		if (CarrierClass == nullptr || PayloadClass == nullptr || FloatOverrideClass == nullptr)
		{
			return;
		}

		UFunction* PodScalars = ::FindGeneratedFunction(CarrierClass, TEXT("PodScalars"));
		UFunction* ReturnFloat = ::FindGeneratedFunction(CarrierClass, TEXT("ReturnFloat"));
		UFunction* ReturnDouble = ::FindGeneratedFunction(CarrierClass, TEXT("ReturnDouble"));
		UFunction* DefaultsAndReturn = ::FindGeneratedFunction(CarrierClass, TEXT("DefaultsAndReturn"));
		UFunction* OutAndInout = ::FindGeneratedFunction(CarrierClass, TEXT("OutAndInout"));
		UFunction* EchoPayload = ::FindGeneratedFunction(CarrierClass, TEXT("EchoPayload"));
		UFunction* DispatchDefaults = ::FindGeneratedFunction(CarrierClass, TEXT("DispatchDefaults"));
		UFunction* NativeFloatOverride = ::FindGeneratedFunction(FloatOverrideClass, TEXT("ReceiveTick"));
		ASSERT_THAT(IsNotNull(PodScalars, TEXT("ASFunction argument matrix should expose PodScalars")));
		ASSERT_THAT(IsNotNull(ReturnFloat, TEXT("ASFunction argument matrix should expose ReturnFloat")));
		ASSERT_THAT(IsNotNull(ReturnDouble, TEXT("ASFunction argument matrix should expose ReturnDouble")));
		ASSERT_THAT(IsNotNull(DefaultsAndReturn, TEXT("ASFunction argument matrix should expose DefaultsAndReturn")));
		ASSERT_THAT(IsNotNull(OutAndInout, TEXT("ASFunction argument matrix should expose OutAndInout")));
		ASSERT_THAT(IsNotNull(EchoPayload, TEXT("ASFunction argument matrix should expose EchoPayload")));
		ASSERT_THAT(IsNotNull(DispatchDefaults, TEXT("ASFunction argument matrix should expose DispatchDefaults")));
		ASSERT_THAT(IsNotNull(NativeFloatOverride, TEXT("ASFunction argument matrix should expose ReceiveTick override")));
		if (PodScalars == nullptr || ReturnFloat == nullptr || ReturnDouble == nullptr
			|| DefaultsAndReturn == nullptr || OutAndInout == nullptr || EchoPayload == nullptr
			|| DispatchDefaults == nullptr || NativeFloatOverride == nullptr)
		{
			return;
		}

		UASFunction* PodScalarsScriptFunction = Cast<UASFunction>(PodScalars);
		UASFunction* ReturnFloatScriptFunction = Cast<UASFunction>(ReturnFloat);
		UASFunction* ReturnDoubleScriptFunction = Cast<UASFunction>(ReturnDouble);
		UASFunction* NativeFloatOverrideScriptFunction = Cast<UASFunction>(NativeFloatOverride);
		ASSERT_THAT(IsNotNull(PodScalarsScriptFunction, TEXT("PodScalars should generate as UASFunction")));
		ASSERT_THAT(IsNotNull(ReturnFloatScriptFunction, TEXT("ReturnFloat should generate as UASFunction")));
		ASSERT_THAT(IsNotNull(ReturnDoubleScriptFunction, TEXT("ReturnDouble should generate as UASFunction")));
		ASSERT_THAT(IsNotNull(NativeFloatOverrideScriptFunction, TEXT("ReceiveTick override should generate as UASFunction")));
		if (PodScalarsScriptFunction == nullptr || ReturnFloatScriptFunction == nullptr
			|| ReturnDoubleScriptFunction == nullptr || NativeFloatOverrideScriptFunction == nullptr)
		{
			return;
		}

		FBoolProperty* BoolValue = FindFProperty<FBoolProperty>(PodScalars, TEXT("bFlag"));
		FFloatProperty* FloatValue = FindFProperty<FFloatProperty>(PodScalars, TEXT("FloatValue"));
		FDoubleProperty* DoubleValue = FindFProperty<FDoubleProperty>(PodScalars, TEXT("DoubleValue"));
		FFloatProperty* FloatReturn = CastField<FFloatProperty>(ReturnFloat->GetReturnProperty());
		FDoubleProperty* DoubleReturn = CastField<FDoubleProperty>(ReturnDouble->GetReturnProperty());
		ASSERT_THAT(IsNotNull(BoolValue, TEXT("PodScalars should reflect bool parameter")));
		ASSERT_THAT(IsNotNull(FloatValue, TEXT("PodScalars should reflect float32 parameter")));
		ASSERT_THAT(IsNotNull(DoubleValue, TEXT("PodScalars should reflect float64 parameter")));
		ASSERT_THAT(IsNotNull(FloatReturn, TEXT("ReturnFloat should reflect float32 return")));
		ASSERT_THAT(IsNotNull(DoubleReturn, TEXT("ReturnDouble should reflect float64 return")));
		if (BoolValue == nullptr || FloatValue == nullptr || DoubleValue == nullptr || FloatReturn == nullptr || DoubleReturn == nullptr)
		{
			return;
		}

		const UASFunction::FArgument* BoolArgument = FindScriptArgument(PodScalarsScriptFunction, TEXT("bFlag"));
		const UASFunction::FArgument* FloatArgument = FindScriptArgument(PodScalarsScriptFunction, TEXT("FloatValue"));
		const UASFunction::FArgument* DoubleArgument = FindScriptArgument(PodScalarsScriptFunction, TEXT("DoubleValue"));
		ASSERT_THAT(IsNotNull(BoolArgument, TEXT("PodScalars should retain bool argument behavior")));
		ASSERT_THAT(IsNotNull(FloatArgument, TEXT("PodScalars should retain float32 argument behavior")));
		ASSERT_THAT(IsNotNull(DoubleArgument, TEXT("PodScalars should retain float64 argument behavior")));
		if (BoolArgument == nullptr || FloatArgument == nullptr || DoubleArgument == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(BoolArgument->VMBehavior == UASFunction::EArgumentVMBehavior::Value1Byte, TEXT("bool parameter should use one-byte VM behavior")));
		ASSERT_THAT(IsTrue(FloatArgument->VMBehavior == UASFunction::EArgumentVMBehavior::Value4Byte, TEXT("float32 parameter should use four-byte VM behavior")));
		ASSERT_THAT(IsTrue(DoubleArgument->VMBehavior == UASFunction::EArgumentVMBehavior::Value8Byte, TEXT("float64 parameter should use eight-byte VM behavior")));
		ASSERT_THAT(IsTrue(ReturnFloatScriptFunction->ReturnArgument.VMBehavior == UASFunction::EArgumentVMBehavior::Value4Byte, TEXT("float32 return should use four-byte VM behavior")));
		ASSERT_THAT(IsTrue(ReturnDoubleScriptFunction->ReturnArgument.VMBehavior == UASFunction::EArgumentVMBehavior::Value8Byte, TEXT("float64 return should use eight-byte VM behavior")));

		FFloatProperty* ExtendedFloatValue = FindFProperty<FFloatProperty>(NativeFloatOverride, TEXT("DeltaSeconds"));
		const UASFunction::FArgument* ExtendedFloatArgument = FindScriptArgument(NativeFloatOverrideScriptFunction, TEXT("DeltaSeconds"));
		ASSERT_THAT(IsNotNull(ExtendedFloatValue, TEXT("ReceiveTick override should reflect native float parameter")));
		ASSERT_THAT(IsNotNull(ExtendedFloatArgument, TEXT("ReceiveTick override should retain float bridge argument")));
		if (ExtendedFloatValue == nullptr || ExtendedFloatArgument == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(ExtendedFloatArgument->VMBehavior == UASFunction::EArgumentVMBehavior::FloatExtendedToDouble, TEXT("native float ReceiveTick parameter should bridge script float as double")));

		FIntProperty* DefaultValue = FindFProperty<FIntProperty>(DefaultsAndReturn, TEXT("Value"));
		FStrProperty* DefaultLabel = FindFProperty<FStrProperty>(DefaultsAndReturn, TEXT("Label"));
		FIntProperty* DefaultReturn = CastField<FIntProperty>(DefaultsAndReturn->GetReturnProperty());
		ASSERT_THAT(IsNotNull(DefaultValue, TEXT("DefaultsAndReturn should reflect Value parameter")));
		ASSERT_THAT(IsNotNull(DefaultLabel, TEXT("DefaultsAndReturn should reflect Label parameter")));
		ASSERT_THAT(IsNotNull(DefaultReturn, TEXT("DefaultsAndReturn should reflect int return")));
		if (DefaultValue == nullptr || DefaultLabel == nullptr || DefaultReturn == nullptr)
		{
			return;
		}

		ASSERT_THAT(AreEqual(2, CountNonReturnParameters(DefaultsAndReturn), TEXT("DefaultsAndReturn should expose two non-return parameters")));
		ASSERT_THAT(AreEqual(FString(TEXT("13")), DefaultsAndReturn->GetMetaData(TEXT("CPP_Default_Value")), TEXT("int default argument should emit CPP_Default metadata")));
		ASSERT_THAT(AreEqual(FString(TEXT("ArgDefault")), DefaultsAndReturn->GetMetaData(TEXT("CPP_Default_Label")), TEXT("FString default argument should emit CPP_Default metadata")));
		ASSERT_THAT(AreEqual(FString(TEXT("Label")), DefaultsAndReturn->GetMetaData(TEXT("AutoCreateRefTerm")), TEXT("AutoCreateRefTerm metadata should round-trip")));
		ASSERT_THAT(AreEqual(FString(TEXT("Label")), DefaultsAndReturn->GetMetaData(TEXT("AdvancedDisplay")), TEXT("AdvancedDisplay metadata should round-trip")));
		ASSERT_THAT(IsTrue(DefaultLabel->HasAllPropertyFlags(CPF_ConstParm | CPF_OutParm | CPF_ReferenceParm), TEXT("const FString&in should carry const/out/reference flags")));
		ASSERT_THAT(IsTrue(DefaultReturn->HasAnyPropertyFlags(CPF_Parm | CPF_OutParm | CPF_ReturnParm), TEXT("return parameter should carry UFunction return flags")));

		FIntProperty* Input = FindFProperty<FIntProperty>(OutAndInout, TEXT("Input"));
		FIntProperty* Output = FindFProperty<FIntProperty>(OutAndInout, TEXT("Output"));
		FIntProperty* Mutable = FindFProperty<FIntProperty>(OutAndInout, TEXT("Mutable"));
		ASSERT_THAT(IsNotNull(Input, TEXT("OutAndInout should reflect Input parameter")));
		ASSERT_THAT(IsNotNull(Output, TEXT("OutAndInout should reflect Output parameter")));
		ASSERT_THAT(IsNotNull(Mutable, TEXT("OutAndInout should reflect Mutable parameter")));
		if (Input == nullptr || Output == nullptr || Mutable == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsFalse(Input->HasAnyPropertyFlags(CPF_OutParm | CPF_ReferenceParm), TEXT("plain int parameter should not become a ref/out parameter")));
		ASSERT_THAT(IsTrue(Output->HasAnyPropertyFlags(CPF_OutParm), TEXT("int&out parameter should carry CPF_OutParm")));
		ASSERT_THAT(IsFalse(Output->HasAnyPropertyFlags(CPF_ReferenceParm), TEXT("int&out parameter should remain out-only")));
		ASSERT_THAT(IsTrue(Mutable->HasAllPropertyFlags(CPF_OutParm | CPF_ReferenceParm), TEXT("int&inout parameter should carry out/reference flags")));
		ASSERT_THAT(IsFalse(Mutable->HasAnyPropertyFlags(CPF_ConstParm), TEXT("int&inout parameter should not carry const flags")));

		FObjectProperty* PayloadParameter = FindFProperty<FObjectProperty>(EchoPayload, TEXT("Payload"));
		FObjectProperty* PayloadReturn = CastField<FObjectProperty>(EchoPayload->GetReturnProperty());
		ASSERT_THAT(IsNotNull(PayloadParameter, TEXT("EchoPayload should reflect script-object parameter")));
		ASSERT_THAT(IsNotNull(PayloadReturn, TEXT("EchoPayload should reflect script-object return")));
		if (PayloadParameter == nullptr || PayloadReturn == nullptr)
		{
			return;
		}
		ASSERT_THAT(AreEqual(PayloadClass, PayloadParameter->PropertyClass, TEXT("script-object parameter should bind to the generated payload class")));
		ASSERT_THAT(AreEqual(PayloadClass, PayloadReturn->PropertyClass, TEXT("script-object return should bind to the generated payload class")));
		ASSERT_THAT(IsTrue(PayloadReturn->HasAnyPropertyFlags(CPF_ReturnParm), TEXT("script-object return should carry CPF_ReturnParm")));

		UObject* Carrier = NewObject<UObject>(GetTransientPackage(), CarrierClass, TEXT("ASFunctionArgumentMatrixCarrierInstance"));
		UObject* Payload = NewObject<UObject>(GetTransientPackage(), PayloadClass, TEXT("ASFunctionArgumentMatrixPayloadInstance"));
		FAngelscriptTestWorld TestWorld(*TestRunner, Engine);
		AActor* FloatOverride = TestWorld.SpawnActorOfClass(FloatOverrideClass);
		ASSERT_THAT(IsNotNull(Carrier, TEXT("ASFunction argument matrix should instantiate the carrier")));
		ASSERT_THAT(IsNotNull(Payload, TEXT("ASFunction argument matrix should instantiate the payload")));
		ASSERT_THAT(IsTrue(TestWorld.IsValid(), TEXT("ASFunction argument matrix should create a test world for actor tick dispatch")));
		ASSERT_THAT(IsNotNull(FloatOverride, TEXT("ASFunction argument matrix should spawn the float override actor")));
		if (Carrier == nullptr || Payload == nullptr || !TestWorld.IsValid() || FloatOverride == nullptr)
		{
			return;
		}

		FStructOnScope PodParams(PodScalars);
		void* PodParamsMemory = PodParams.GetStructMemory();
		BoolValue->SetPropertyValue_InContainer(PodParamsMemory, true);
		FloatValue->SetPropertyValue_InContainer(PodParamsMemory, 12.5f);
		DoubleValue->SetPropertyValue_InContainer(PodParamsMemory, 42.25);
		if (!InvokeWithParams(*TestRunner, Engine, Carrier, PodScalars, PodParamsMemory))
		{
			return;
		}

		bool bLastBool = false;
		int32 LastFloatHundredths = INDEX_NONE;
		int32 LastDoubleHundredths = INDEX_NONE;
		ASSERT_THAT(IsTrue(AngelscriptFunctionalTestUtils::ReadPropertyValue<FBoolProperty>(*TestRunner, Carrier, TEXT("bLastBool"), bLastBool), TEXT("PodScalars should expose bLastBool")));
		ASSERT_THAT(IsTrue(AngelscriptFunctionalTestUtils::ReadPropertyValue<FIntProperty>(*TestRunner, Carrier, TEXT("LastFloatHundredths"), LastFloatHundredths), TEXT("PodScalars should expose LastFloatHundredths")));
		ASSERT_THAT(IsTrue(AngelscriptFunctionalTestUtils::ReadPropertyValue<FIntProperty>(*TestRunner, Carrier, TEXT("LastDoubleHundredths"), LastDoubleHundredths), TEXT("PodScalars should expose LastDoubleHundredths")));
		ASSERT_THAT(IsTrue(bLastBool, TEXT("bool POD parameter should pass through ProcessEvent")));
		ASSERT_THAT(AreEqual(1250, LastFloatHundredths, TEXT("float32 POD parameter should pass through ProcessEvent")));
		ASSERT_THAT(AreEqual(4225, LastDoubleHundredths, TEXT("float64 POD parameter should pass through ProcessEvent")));

		FStructOnScope FloatReturnParams(ReturnFloat);
		if (!InvokeWithParams(*TestRunner, Engine, Carrier, ReturnFloat, FloatReturnParams.GetStructMemory()))
		{
			return;
		}
		ASSERT_THAT(IsNear(12.25f, FloatReturn->GetPropertyValue_InContainer(FloatReturnParams.GetStructMemory()), UE_KINDA_SMALL_NUMBER, TEXT("float32 return should pass through the reflected return slot")));

		FStructOnScope DoubleReturnParams(ReturnDouble);
		if (!InvokeWithParams(*TestRunner, Engine, Carrier, ReturnDouble, DoubleReturnParams.GetStructMemory()))
		{
			return;
		}
		ASSERT_THAT(IsNear(24.5, DoubleReturn->GetPropertyValue_InContainer(DoubleReturnParams.GetStructMemory()), static_cast<double>(UE_KINDA_SMALL_NUMBER), TEXT("float64 return should pass through the reflected return slot")));

		TestWorld.BeginPlay(*FloatOverride);
		TestWorld.DispatchActorTick(*FloatOverride, 2.25f, 1);
		int32 LastTickHundredths = INDEX_NONE;
		ASSERT_THAT(IsTrue(AngelscriptFunctionalTestUtils::ReadPropertyValue<FIntProperty>(*TestRunner, FloatOverride, TEXT("LastTickHundredths"), LastTickHundredths), TEXT("Tick override should expose LastTickHundredths")));
		ASSERT_THAT(AreEqual(225, LastTickHundredths, TEXT("native float ReceiveTick parameter should round-trip through the FloatExtendedToDouble bridge")));

		FStructOnScope DefaultsParams(DispatchDefaults);
		if (!InvokeWithParams(*TestRunner, Engine, Carrier, DispatchDefaults, DefaultsParams.GetStructMemory()))
		{
			return;
		}
		FIntProperty* DispatchReturn = CastField<FIntProperty>(DispatchDefaults->GetReturnProperty());
		ASSERT_THAT(IsNotNull(DispatchReturn, TEXT("DispatchDefaults should expose an int return slot")));
		if (DispatchReturn == nullptr)
		{
			return;
		}
		ASSERT_THAT(AreEqual(23, DispatchReturn->GetPropertyValue_InContainer(DefaultsParams.GetStructMemory()), TEXT("script default-argument dispatch should use reflected default values")));

		FString LastLabel;
		ASSERT_THAT(IsTrue(AngelscriptFunctionalTestUtils::ReadPropertyValue<FStrProperty>(*TestRunner, Carrier, TEXT("LastLabel"), LastLabel), TEXT("default dispatch should expose LastLabel")));
		ASSERT_THAT(AreEqual(FString(TEXT("ArgDefault")), LastLabel, TEXT("default dispatch should write the FString default value")));

		FStructOnScope RefParams(OutAndInout);
		void* RefParamsMemory = RefParams.GetStructMemory();
		Input->SetPropertyValue_InContainer(RefParamsMemory, 12);
		Mutable->SetPropertyValue_InContainer(RefParamsMemory, 5);
		if (!InvokeWithParams(*TestRunner, Engine, Carrier, OutAndInout, RefParamsMemory))
		{
			return;
		}
		ASSERT_THAT(AreEqual(24, Output->GetPropertyValue_InContainer(RefParamsMemory), TEXT("int&out parameter should write back through the reflected buffer")));
		ASSERT_THAT(AreEqual(8, Mutable->GetPropertyValue_InContainer(RefParamsMemory), TEXT("int&inout parameter should mutate the reflected caller slot")));

		FStructOnScope PayloadParams(EchoPayload);
		void* PayloadParamsMemory = PayloadParams.GetStructMemory();
		PayloadParameter->SetObjectPropertyValue_InContainer(PayloadParamsMemory, Payload);
		if (!InvokeWithParams(*TestRunner, Engine, Carrier, EchoPayload, PayloadParamsMemory))
		{
			return;
		}
		ASSERT_THAT(AreEqual(Payload, PayloadReturn->GetObjectPropertyValue_InContainer(PayloadParamsMemory), TEXT("script-object return should preserve the payload UObject identity")));
	}
	TEST_METHOD(ScriptComponentPropertiesExposeGeneratedObjectAndSubclassTypes)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope EngineScope(Engine);
		const FName CodeSuperModuleName(TEXT("ASFunctionArgumentMatrixCodeSuper"));
		const FString CodeSuperFilename(TEXT("ASFunctionArgumentMatrixCodeSuper.as"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*CodeSuperModuleName.ToString());
		};

		const FString ScriptSource = ASTEST_AS(R"AS(
			UCLASS()
			class UASFunctionArgumentMatrixBaseSceneComponent : USceneComponent
			{
			}

			UCLASS()
			class UASFunctionArgumentMatrixChildSceneComponent : UASFunctionArgumentMatrixBaseSceneComponent
			{
			}

			UCLASS()
			class AASFunctionArgumentMatrixCodeSuperActor : AActor
			{
				UPROPERTY(DefaultComponent, RootComponent)
				UASFunctionArgumentMatrixChildSceneComponent ScriptRoot;

				UPROPERTY()
				UASFunctionArgumentMatrixChildSceneComponent ScriptComponentRef;

				UPROPERTY()
				TSubclassOf<UASFunctionArgumentMatrixChildSceneComponent> ScriptComponentClass;
			}
			)AS");

		UClass* ActorClass = AngelscriptFunctionalTestUtils::CompileScriptModule(
			*TestRunner,
			Engine,
			CodeSuperModuleName,
			CodeSuperFilename,
			ScriptSource,
			TEXT("AASFunctionArgumentMatrixCodeSuperActor"));
		UClass* BaseComponentClass = ::FindGeneratedClass(&Engine, TEXT("UASFunctionArgumentMatrixBaseSceneComponent"));
		UClass* ChildComponentClass = ::FindGeneratedClass(&Engine, TEXT("UASFunctionArgumentMatrixChildSceneComponent"));
		ASSERT_THAT(IsNotNull(ActorClass, TEXT("Code-super actor should compile")));
		ASSERT_THAT(IsNotNull(BaseComponentClass, TEXT("Code-super base component should compile")));
		ASSERT_THAT(IsNotNull(ChildComponentClass, TEXT("Code-super child component should compile")));
		if (ActorClass == nullptr || BaseComponentClass == nullptr || ChildComponentClass == nullptr)
		{
			return;
		}

		ASSERT_THAT(AreEqual(USceneComponent::StaticClass(), BaseComponentClass->GetSuperClass(), TEXT("Base script component should resolve to native scene-component code super")));
		ASSERT_THAT(AreEqual(BaseComponentClass, ChildComponentClass->GetSuperClass(), TEXT("Child script component should keep its script superclass")));

		FObjectProperty* RootProperty = FindFProperty<FObjectProperty>(ActorClass, TEXT("ScriptRoot"));
		FObjectProperty* RefProperty = FindFProperty<FObjectProperty>(ActorClass, TEXT("ScriptComponentRef"));
		FClassProperty* ClassProperty = FindFProperty<FClassProperty>(ActorClass, TEXT("ScriptComponentClass"));
		ASSERT_THAT(IsNotNull(RootProperty, TEXT("Default component property should reflect as an object property")));
		ASSERT_THAT(IsNotNull(RefProperty, TEXT("Script component reference should reflect as an object property")));
		ASSERT_THAT(IsNotNull(ClassProperty, TEXT("Script component subclass should reflect as a class property")));
		if (RootProperty == nullptr || RefProperty == nullptr || ClassProperty == nullptr)
		{
			return;
		}

		ASSERT_THAT(AreEqual(ChildComponentClass, RootProperty->PropertyClass, TEXT("Default component property should keep the generated script component class")));
		ASSERT_THAT(AreEqual(ChildComponentClass, RefProperty->PropertyClass, TEXT("Object property should keep the generated script component class")));
		ASSERT_THAT(AreEqual(UClass::StaticClass(), ClassProperty->PropertyClass, TEXT("TSubclassOf property should use UClass as the reflected property class")));
		ASSERT_THAT(AreEqual(ChildComponentClass, ClassProperty->MetaClass, TEXT("TSubclassOf property should keep the generated script component as MetaClass")));
	}
};

#endif